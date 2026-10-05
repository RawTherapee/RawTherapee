/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "camconst.h"
#include "color.h"
#include "curves.h"
#include "dcp.h"
#include "dfmanager.h"
#include "ffmanager.h"
#include "iccmatrices.h"
#include "iccstore.h"
#include "imagefloat.h"
#include "improcfun.h"
#include "jaggedarray.h"
#include "median.h"
#include "mytime.h"
#include "pdaflinesfilter.h"
#include "pixelsmap.h"
#include "procparams.h"
#include "rawimage.h"
#include "rawimagesource_i.h"
#include "rawimagesource.h"
#include "rescale.h"
#include "rt_math.h"
#include "rtengine.h"
#include "rtlensfun.h"
#include "lensmetadata.h"
#include "rtgui/options.h"

//#define BENCHMARK
#include "StopWatch.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include "opthelper.h"

namespace
{

float clipitc(float x)
{
    if (std::isnan(x)) {
        x = 0.1f;
    } else {
        x = rtengine::LIM(x, 0.1f, 65534.9f);//White balance Itcwb - limit values 
    }
    return x;
}

void rotateLine(const float* const line, rtengine::PlanarPtr<float> &channel, const int tran, const int i, const int w, const int h)
{
    switch (tran & TR_ROT) {
        case TR_R180:
            for (int j = 0; j < w; j++) {
                channel(h - 1 - i, w - 1 - j) = line[j];
            }

            break;

        case TR_R90:
            for (int j = 0; j < w; j++) {
                channel(j, h - 1 - i) = line[j];
            }

            break;

        case TR_R270:
            for (int j = 0; j < w; j++) {
                channel(w - 1 - j, i) = line[j];
            }

            break;

        case TR_NONE:
        default:
            for (int j = 0; j < w; j++) {
                channel(i, j) = line[j];
            }
    }
}

void transLineStandard(const float* const red, const float* const green, const float* const blue, const int i, rtengine::Imagefloat* const image, const int tran, const int imwidth, const int imheight)
{
    // conventional CCD coarse rotation
    rotateLine(red, image->r, tran, i, imwidth, imheight);
    rotateLine(green, image->g, tran, i, imwidth, imheight);
    rotateLine(blue, image->b, tran, i, imwidth, imheight);
}

void transLineFuji(const float* const red, const float* const green, const float* const blue, const int i, rtengine::Imagefloat* const image, const int tran, const int imheight, const int fw)
{

    // Fuji SuperCCD rotation + coarse rotation
    int start = std::abs(fw - i);
    int w = fw * 2 + 1;
    int h = (imheight - fw) * 2 + 1;
    int end = std::min(h + fw - i, w - fw + i);

    switch (tran & TR_ROT) {
        case TR_R180:
            for (int j = start; j < end; j++) {
                int y = i + j - fw;
                int x = fw - i + j;

                if (x >= 0 && y < image->getHeight() && y >= 0 && x < image->getWidth()) {
                    image->r(image->getHeight() - 1 - y, image->getWidth() - 1 - x) = red[j];
                    image->g(image->getHeight() - 1 - y, image->getWidth() - 1 - x) = green[j];
                    image->b(image->getHeight() - 1 - y, image->getWidth() - 1 - x) = blue[j];
                }
            }

            break;

        case TR_R270:
            for (int j = start; j < end; j++) {
                int y = i + j - fw;
                int x = fw - i + j;

                if (x >= 0 && x < image->getHeight() && y >= 0 && y < image->getWidth()) {
                    image->r(image->getHeight() - 1 - x, y) = red[j];
                    image->g(image->getHeight() - 1 - x, y) = green[j];
                    image->b(image->getHeight() - 1 - x, y) = blue[j];
                }
            }

            break;

        case TR_R90:
            for (int j = start; j < end; j++) {
                int y = i + j - fw;
                int x = fw - i + j;

                if (x >= 0 && y < image->getWidth() && y >= 0 && x < image->getHeight()) {
                    image->r(x, image->getWidth() - 1 - y) = red[j];
                    image->g(x, image->getWidth() - 1 - y) = green[j];
                    image->b(x, image->getWidth() - 1 - y) = blue[j];
                }
            }

            break;

        case TR_NONE:
        default:
            for (int j = start; j < end; j++) {
                int y = i + j - fw;
                int x = fw - i + j;

                if (x >= 0 && y < image->getHeight() && y >= 0 && x < image->getWidth()) {
                    image->r(y, x) = red[j];
                    image->g(y, x) = green[j];
                    image->b(y, x) = blue[j];
                }
            }
    }
}

void transLineD1x(const float* const red, const float* const green, const float* const blue, const int i, rtengine::Imagefloat* const image, const int tran, const int imwidth, const int imheight, const bool oddHeight, const bool clip)
{
    // Nikon D1X has an uncommon sensor with 4028 x 1324 sensels.
    // Vertical sensel size is 2x horizontal sensel size
    // We have to do vertical interpolation for the 'missing' rows
    // We do that in combination with coarse rotation

    switch (tran & TR_ROT) {
        case TR_R180: // rotate 180 degree
            for (int j = 0; j < imwidth; j++) {
                image->r(2 * (imheight - 1 - i), imwidth - 1 - j) = red[j];
                image->g(2 * (imheight - 1 - i), imwidth - 1 - j) = green[j];
                image->b(2 * (imheight - 1 - i), imwidth - 1 - j) = blue[j];
            }

            if (i == 0) {
                for (int j = 0; j < imwidth; j++) {
                    image->r(2 * imheight - 1, imwidth - 1 - j) = red[j];
                    image->g(2 * imheight - 1, imwidth - 1 - j) = green[j];
                    image->b(2 * imheight - 1, imwidth - 1 - j) = blue[j];
                }
            }

            if (i == 1 || i == 2) { // linear interpolation
                int row = 2 * imheight - 1 - 2 * i;

                for (int j = 0; j < imwidth; j++) {
                    int col = imwidth - 1 - j;
                    image->r(row, col) = (red[j] + image->r(row + 1, col)) / 2;
                    image->g(row, col) = (green[j] + image->g(row + 1, col)) / 2;
                    image->b(row, col) = (blue[j] + image->b(row + 1, col)) / 2;
                }

                if (i == 2 && oddHeight) {
                    row = 2 * imheight;

                    for (int j = 0; j < imwidth; j++) {
                        int col = imwidth - 1 - j;
                        image->r(row, col) = (red[j] + image->r(row - 2, col)) / 2;
                        image->g(row, col) = (green[j] + image->g(row - 2, col)) / 2;
                        image->b(row, col) = (blue[j] + image->b(row - 2, col)) / 2;
                    }
                }
            } else if (i == imheight - 1 || i == imheight - 2) {
                int row = 2 * imheight - 1 - 2 * i;

                for (int j = 0; j < imwidth; j++) {
                    int col = imwidth - 1 - j;
                    image->r(row, col) = (red[j] + image->r(row + 1, col)) / 2;
                    image->g(row, col) = (green[j] + image->g(row + 1, col)) / 2;
                    image->b(row, col) = (blue[j] + image->b(row + 1, col)) / 2;
                }

                row = 2 * imheight - 1 - 2 * i + 2;

                for (int j = 0; j < imwidth; j++) {
                    int col = imwidth - 1 - j;
                    image->r(row, col) = (red[j] + image->r(row + 1, col)) / 2;
                    image->g(row, col) = (green[j] + image->g(row + 1, col)) / 2;
                    image->b(row, col) = (blue[j] + image->b(row + 1, col)) / 2;
                }
            } else if (i > 2 && i < imheight - 1) { // vertical bicubic interpolation
                int row = 2 * imheight - 1 - 2 * i + 2;

                for (int j = 0; j < imwidth; j++) {
                    int col = imwidth - 1 - j;
                    image->r(row, col) = MAX(0.f, -0.0625f * (red[j] + image->r(row + 3, col)) + 0.5625f * (image->r(row - 1, col) + image->r(row + 1, col)));
                    image->g(row, col) = MAX(0.f, -0.0625f * (green[j] + image->g(row + 3, col)) + 0.5625f * (image->g(row - 1, col) + image->g(row + 1, col)));
                    image->b(row, col) = MAX(0.f, -0.0625f * (blue[j] + image->b(row + 3, col)) + 0.5625f * (image->b(row - 1, col) + image->b(row + 1, col)));

                    if (clip) {
                        image->r(row, col) = MIN(image->r(row, col), rtengine::MAXVALF);
                        image->g(row, col) = MIN(image->g(row, col), rtengine::MAXVALF);
                        image->b(row, col) = MIN(image->b(row, col), rtengine::MAXVALF);
                    }
                }
            }

            break;

        case TR_R90: // rotate right
            if (i == 0) {
                for (int j = 0; j < imwidth; j++) {
                    image->r(j, 2 * imheight - 1) = red[j];
                    image->g(j, 2 * imheight - 1) = green[j];
                    image->b(j, 2 * imheight - 1) = blue[j];
                }
            }

            for (int j = 0; j < imwidth; j++) {
                image->r(j, 2 * (imheight - 1 - i)) = red[j];
                image->g(j, 2 * (imheight - 1 - i)) = green[j];
                image->b(j, 2 * (imheight - 1 - i)) = blue[j];
            }

            if (i == 1 || i == 2) { // linear interpolation
                int col = 2 * imheight - 1 - 2 * i;

                for (int j = 0; j < imwidth; j++) {
                    image->r(j, col) = (red[j] + image->r(j, col + 1)) / 2;
                    image->g(j, col) = (green[j] + image->g(j, col + 1)) / 2;
                    image->b(j, col) = (blue[j] + image->b(j, col + 1)) / 2;

                    if (oddHeight && i == 2) {
                        image->r(j, 2 * imheight) = (red[j] + image->r(j, 2 * imheight - 2)) / 2;
                        image->g(j, 2 * imheight) = (green[j] + image->g(j, 2 * imheight - 2)) / 2;
                        image->b(j, 2 * imheight) = (blue[j] + image->b(j, 2 * imheight - 2)) / 2;
                    }
                }
            } else if (i == imheight - 1) {
                int col = 2 * imheight - 1 - 2 * i;

                for (int j = 0; j < imwidth; j++) {
                    image->r(j, col) = (red[j] + image->r(j, col + 1)) / 2;
                    image->g(j, col) = (green[j] + image->g(j, col + 1)) / 2;
                    image->b(j, col) = (blue[j] + image->b(j, col + 1)) / 2;
                }

                col = 2 * imheight - 1 - 2 * i + 2;

                for (int j = 0; j < imwidth; j++) {
                    image->r(j, col) = (red[j] + image->r(j, col + 1)) / 2;
                    image->g(j, col) = (green[j] + image->g(j, col + 1)) / 2;
                    image->b(j, col) = (blue[j] + image->b(j, col + 1)) / 2;
                }
            } else if (i > 2 && i < imheight - 1) { // vertical bicubic interpolation
                int col = 2 * imheight - 1 - 2 * i + 2;

                for (int j = 0; j < imwidth; j++) {
                    image->r(j, col) = MAX(0.f, -0.0625f * (red[j] + image->r(j, col + 3)) + 0.5625f * (image->r(j, col - 1) + image->r(j, col + 1)));
                    image->g(j, col) = MAX(0.f, -0.0625f * (green[j] + image->g(j, col + 3)) + 0.5625f * (image->g(j, col - 1) + image->g(j, col + 1)));
                    image->b(j, col) = MAX(0.f, -0.0625f * (blue[j] + image->b(j, col + 3)) + 0.5625f * (image->b(j, col - 1) + image->b(j, col + 1)));

                    if (clip) {
                        image->r(j, col) = MIN(image->r(j, col), rtengine::MAXVALF);
                        image->g(j, col) = MIN(image->g(j, col), rtengine::MAXVALF);
                        image->b(j, col) = MIN(image->b(j, col), rtengine::MAXVALF);
                    }
                }
            }

            break;

        case TR_R270: // rotate left
            if (i == 0) {
                for (int j = imwidth - 1, row = 0; j >= 0; j--, row++) {
                    image->r(row, 2 * i) = red[j];
                    image->g(row, 2 * i) = green[j];
                    image->b(row, 2 * i) = blue[j];
                }
            } else if (i == 1 || i == 2) { // linear interpolation
                for (int j = imwidth - 1, row = 0; j >= 0; j--, row++) {
                    image->r(row, 2 * i) = red[j];
                    image->g(row, 2 * i) = green[j];
                    image->b(row, 2 * i) = blue[j];
                    image->r(row, 2 * i - 1) = (red[j] + image->r(row, 2 * i - 2)) * 0.5f;
                    image->g(row, 2 * i - 1) = (green[j] + image->g(row, 2 * i - 2)) * 0.5f;
                    image->b(row, 2 * i - 1) = (blue[j] + image->b(row, 2 * i - 2)) * 0.5f;
                }
            } else if (i > 0 && i < imheight) { // vertical bicubic interpolation
                for (int j = imwidth - 1, row = 0; j >= 0; j--, row++) {
                    image->r(row, 2 * i - 3) = MAX(0.f, -0.0625f * (red[j] + image->r(row, 2 * i - 6)) + 0.5625f * (image->r(row, 2 * i - 2) + image->r(row, 2 * i - 4)));
                    image->g(row, 2 * i - 3) = MAX(0.f, -0.0625f * (green[j] + image->g(row, 2 * i - 6)) + 0.5625f * (image->g(row, 2 * i - 2) + image->g(row, 2 * i - 4)));
                    image->b(row, 2 * i - 3) = MAX(0.f, -0.0625f * (blue[j] + image->b(row, 2 * i - 6)) + 0.5625f * (image->b(row, 2 * i - 2) + image->b(row, 2 * i - 4)));

                    if (clip) {
                        image->r(row, 2 * i - 3) = MIN(image->r(row, 2 * i - 3), rtengine::MAXVALF);
                        image->g(row, 2 * i - 3) = MIN(image->g(row, 2 * i - 3), rtengine::MAXVALF);
                        image->b(row, 2 * i - 3) = MIN(image->b(row, 2 * i - 3), rtengine::MAXVALF);
                    }

                    image->r(row, 2 * i) = red[j];
                    image->g(row, 2 * i) = green[j];
                    image->b(row, 2 * i) = blue[j];
                }
            }

            if (i == imheight - 1) {
                for (int j = imwidth - 1, row = 0; j >= 0; j--, row++) {
                    image->r(row, 2 * i - 1) = MAX(0.f, -0.0625f * (red[j] + image->r(row, 2 * i - 4)) + 0.5625f * (image->r(row, 2 * i) + image->r(row, 2 * i - 2)));
                    image->g(row, 2 * i - 1) = MAX(0.f, -0.0625f * (green[j] + image->g(row, 2 * i - 4)) + 0.5625f * (image->g(row, 2 * i) + image->g(row, 2 * i - 2)));
                    image->b(row, 2 * i - 1) = MAX(0.f, -0.0625f * (blue[j] + image->b(row, 2 * i - 4)) + 0.5625f * (image->b(row, 2 * i) + image->b(row, 2 * i - 2)));

                    if (clip) {
                        image->r(j, 2 * i - 1) = MIN(image->r(j, 2 * i - 1), rtengine::MAXVALF);
                        image->g(j, 2 * i - 1) = MIN(image->g(j, 2 * i - 1), rtengine::MAXVALF);
                        image->b(j, 2 * i - 1) = MIN(image->b(j, 2 * i - 1), rtengine::MAXVALF);
                    }

                    image->r(row, 2 * i + 1) = (red[j] + image->r(row, 2 * i - 1)) / 2;
                    image->g(row, 2 * i + 1) = (green[j] + image->g(row, 2 * i - 1)) / 2;
                    image->b(row, 2 * i + 1) = (blue[j] + image->b(row, 2 * i - 1)) / 2;

                    if (oddHeight) {
                        image->r(row, 2 * i + 2) = (red[j] + image->r(row, 2 * i - 2)) / 2;
                        image->g(row, 2 * i + 2) = (green[j] + image->g(row, 2 * i - 2)) / 2;
                        image->b(row, 2 * i + 2) = (blue[j] + image->b(row, 2 * i - 2)) / 2;
                    }
                }
            }

            break;

        case TR_NONE: // no coarse rotation
        default:
            rotateLine(red, image->r, tran, 2 * i, imwidth, imheight);
            rotateLine(green, image->g, tran, 2 * i, imwidth, imheight);
            rotateLine(blue, image->b, tran, 2 * i, imwidth, imheight);

            if (i == 1 || i == 2) { // linear interpolation
                for (int j = 0; j < imwidth; j++) {
                    image->r(2 * i - 1, j) = (red[j] + image->r(2 * i - 2, j)) / 2;
                    image->g(2 * i - 1, j) = (green[j] + image->g(2 * i - 2, j)) / 2;
                    image->b(2 * i - 1, j) = (blue[j] + image->b(2 * i - 2, j)) / 2;
                }
            } else if (i > 2 && i < imheight) { // vertical bicubic interpolation
                for (int j = 0; j < imwidth; j++) {
                    image->r(2 * i - 3, j) = MAX(0.f, -0.0625f * (red[j] + image->r(2 * i - 6, j)) + 0.5625f * (image->r(2 * i - 2, j) + image->r(2 * i - 4, j)));
                    image->g(2 * i - 3, j) = MAX(0.f, -0.0625f * (green[j] + image->g(2 * i - 6, j)) + 0.5625f * (image->g(2 * i - 2, j) + image->g(2 * i - 4, j)));
                    image->b(2 * i - 3, j) = MAX(0.f, -0.0625f * (blue[j] + image->b(2 * i - 6, j)) + 0.5625f * (image->b(2 * i - 2, j) + image->b(2 * i - 4, j)));

                    if (clip) {
                        image->r(2 * i - 3, j) = MIN(image->r(2 * i - 3, j), rtengine::MAXVALF);
                        image->g(2 * i - 3, j) = MIN(image->g(2 * i - 3, j), rtengine::MAXVALF);
                        image->b(2 * i - 3, j) = MIN(image->b(2 * i - 3, j), rtengine::MAXVALF);
                    }
                }
            }

            if (i == imheight - 1) {
                for (int j = 0; j < imwidth; j++) {
                    image->r(2 * i - 1, j) = MAX(0.f, -0.0625f * (red[j] + image->r(2 * i - 4, j)) + 0.5625f * (image->r(2 * i, j) + image->r(2 * i - 2, j)));
                    image->g(2 * i - 1, j) = MAX(0.f, -0.0625f * (green[j] + image->g(2 * i - 4, j)) + 0.5625f * (image->g(2 * i, j) + image->g(2 * i - 2, j)));
                    image->b(2 * i - 1, j) = MAX(0.f, -0.0625f * (blue[j] + image->b(2 * i - 4, j)) + 0.5625f * (image->b(2 * i, j) + image->b(2 * i - 2, j)));

                    if (clip) {
                        image->r(2 * i - 1, j) = MIN(image->r(2 * i - 1, j), rtengine::MAXVALF);
                        image->g(2 * i - 1, j) = MIN(image->g(2 * i - 1, j), rtengine::MAXVALF);
                        image->b(2 * i - 1, j) = MIN(image->b(2 * i - 1, j), rtengine::MAXVALF);
                    }

                    image->r(2 * i + 1, j) = (red[j] + image->r(2 * i - 1, j)) / 2;
                    image->g(2 * i + 1, j) = (green[j] + image->g(2 * i - 1, j)) / 2;
                    image->b(2 * i + 1, j) = (blue[j] + image->b(2 * i - 1, j)) / 2;

                    if (oddHeight) {
                        image->r(2 * i + 2, j) = (red[j] + image->r(2 * i - 2, j)) / 2;
                        image->g(2 * i + 2, j) = (green[j] + image->g(2 * i - 2, j)) / 2;
                        image->b(2 * i + 2, j) = (blue[j] + image->b(2 * i - 2, j)) / 2;
                    }
                }
            }
    }
}

bool checkRawDataDimensions(const array2D<float> &rawData, const rtengine::RawImage &rawImage, int width, int height)
{
    const int colors = (rawImage.getSensorType() == rtengine::ST_BAYER ||
                           rawImage.getSensorType() == rtengine::ST_FUJI_XTRANS ||
                           rawImage.get_colors() == 1)
                           ? 1
                           : 3;
    return rawData.getHeight() == height && rawData.getWidth() == colors * width;
}

}


namespace rtengine
{

RawImageSource::RawImageSource()
    : ImageSource()
    , W(0), H(0)
    , plistener(nullptr)
    , scale_mul{}
    , c_black{}
    , c_white{}
    , cblacksom{}
    , ref_pre_mul{}
    , refwb_red(0.0)
    , refwb_green(0.0)
    , refwb_blue(0.0)
    , rgb_cam{}
    , cam_rgb{}
    , xyz_cam{}
    , cam_xyz{}
    , fuji(false)
    , d1x(false)
    , border(4)
    , chmax{}
    , hlmax{}
    , clmax{}
    , initialGain(0.0)
    , camInitialGain(0.0)
    , defGain(0.0)
    , camProfile(nullptr)
    , ri(nullptr)
    , rawData(0, 0)
    , green(0, 0)
    , greenloc(0, 0)
    , red(0, 0)
    , redloc(0, 0)
    , blue(0, 0)
    , blueloc(0, 0)
    , greenCache(nullptr)
    , redCache(nullptr)
    , blueCache(nullptr)
    , rawDirty(true)
    , histMatchingParams(new procparams::ColorManagementParams)
{
    embProfile = nullptr;
    rgbSourceModified = false;

    for (int i = 0; i < 4; ++i) {
        psRedBrightness[i] = psGreenBrightness[i] = psBlueBrightness[i] = 1.f;
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

RawImageSource::~RawImageSource()
{

    delete idata;
    delete redCache;
    delete greenCache;
    delete blueCache;

    if (camProfile) {
        cmsCloseProfile(camProfile);
    }

    if (embProfile) {
        cmsCloseProfile(embProfile);
    }
}

unsigned RawImageSource::FC(int row, int col) const
{
    return ri->FC(row, col);
}

eSensorType RawImageSource::getSensorType() const
{
    return ri != nullptr ? ri->getSensorType() : ST_NONE;
}

bool RawImageSource::isMono() const
{
    return ri->get_colors() == 1;
}

int RawImageSource::getRotateDegree() const
{
    return ri->get_rotateDegree();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::transformRect(const PreviewProps &pp, int tran, int &ssx1, int &ssy1, int &width, int &height, int &fw)
{
    int pp_x = pp.getX() + border;
    int pp_y = pp.getY() + border;
    int pp_width = pp.getWidth();
    int pp_height = pp.getHeight();

    if (d1x) {
        if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
            pp_x /= 2;
            pp_width = pp_width / 2 + 1;
        } else {
            pp_y /= 2;
            pp_height = pp_height / 2 + 1;
        }
    }

    int w = W, h = H;

    if (fuji) {
        w = ri->get_FujiWidth() * 2 + 1;
        h = (H - ri->get_FujiWidth()) * 2 + 1;
    }

    int sw = w, sh = h;

    if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
        sw = h;
        sh = w;
    }

    if (pp_width > sw - 2 * border) {
        pp_width = sw - 2 * border;
    }

    if (pp_height > sh - 2 * border) {
        pp_height = sh - 2 * border;
    }

    int ppx = pp_x, ppy = pp_y;

    if (tran & TR_HFLIP) {
        ppx = max(sw - pp_x - pp_width, 0);
    }

    if (tran & TR_VFLIP) {
        ppy = max(sh - pp_y - pp_height, 0);
    }

    int sx1 = ppx;        // assuming it's >=0
    int sy1 = ppy;        // assuming it's >=0
    int sx2 = min(ppx + pp_width, w - 1);
    int sy2 = min(ppy + pp_height, h - 1);

    if ((tran & TR_ROT) == TR_R180) {
        sx1 = max(w - ppx - pp_width, 0);
        sy1 = max(h - ppy - pp_height, 0);
        sx2 = min(sx1 + pp_width, w - 1);
        sy2 = min(sy1 + pp_height, h - 1);
    } else if ((tran & TR_ROT) == TR_R90) {
        sx1 = ppy;
        sy1 = max(h - ppx - pp_width, 0);
        sx2 = min(sx1 + pp_height, w - 1);
        sy2 = min(sy1 + pp_width, h - 1);
    } else if ((tran & TR_ROT) == TR_R270) {
        sx1 = max(w - ppy - pp_height, 0);
        sy1 = ppx;
        sx2 = min(sx1 + pp_height, w - 1);
        sy2 = min(sy1 + pp_width, h - 1);
    }

    if (fuji) {
        // atszamoljuk a koordinatakat fuji-ra:
        // recalculate the coordinates fuji-ra:
        ssx1 = (sx1 + sy1) / 2;
        ssy1 = (sy1 - sx2) / 2 + ri->get_FujiWidth();
        int ssx2 = (sx2 + sy2) / 2 + 1;
        int ssy2 = (sy2 - sx1) / 2 + ri->get_FujiWidth();
        fw = (sx2 - sx1) / 2 / pp.getSkip();
        width = (ssx2 - ssx1) / pp.getSkip() + ((ssx2 - ssx1) % pp.getSkip() > 0);
        height = (ssy2 - ssy1) / pp.getSkip() + ((ssy2 - ssy1) % pp.getSkip() > 0);
    } else {
        ssx1 = sx1;
        ssy1 = sy1;
        width = (sx2 + 1 - sx1) / pp.getSkip() + ((sx2 + 1 - sx1) % pp.getSkip() > 0);
        height = (sy2 + 1 - sy1) / pp.getSkip() + ((sy2 + 1 - sy1) % pp.getSkip() > 0);
    }
}

float calculate_scale_mul(float scale_mul[4], const float pre_mul_[4], const float c_white[4], const float c_black[4], bool isMono, int colors)
{
    if (isMono || colors == 1) {
        for (int c = 0; c < 4; c++) {
            scale_mul[c] = 65535.f / (c_white[c] - c_black[c]);
        }
    } else {
        float pre_mul[4];

        for (int c = 0; c < 4; c++) {
            pre_mul[c] = pre_mul_[c];
        }

        if (pre_mul[3] == 0) {
            pre_mul[3] = pre_mul[1]; // G2 == G1
        }

        float maxpremul = max(pre_mul[0], pre_mul[1], pre_mul[2], pre_mul[3]);

        for (int c = 0; c < 4; c++) {
            scale_mul[c] = (pre_mul[c] / maxpremul) * 65535.f / (c_white[c] - c_black[c]);
        }
    }

    float gain = max(scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]) / min(scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]);
    return gain;
}

void RawImageSource::wbMul2Camera(double &rm, double &gm, double &bm)
{
    double r = rm;
    double g = gm;
    double b = bm;

    auto imatrices = getImageMatrices();

    if (imatrices) {
        double rr = imatrices->cam_rgb[0][0] * r + imatrices->cam_rgb[0][1] * g + imatrices->cam_rgb[0][2] * b;
        double gg = imatrices->cam_rgb[1][0] * r + imatrices->cam_rgb[1][1] * g + imatrices->cam_rgb[1][2] * b;
        double bb = imatrices->cam_rgb[2][0] * r + imatrices->cam_rgb[2][1] * g + imatrices->cam_rgb[2][2] * b;
        r = rr;
        g = gg;
        b = bb;
    }

    rm = ri->get_pre_mul(0) / r;
    gm = ri->get_pre_mul(1) / g;
    bm = ri->get_pre_mul(2) / b;

    rm /= gm;
    bm /= gm;
    gm = 1.0;
}


void RawImageSource::wbCamera2Mul(double &rm, double &gm, double &bm)
{
    auto imatrices = getImageMatrices();

    double r = ri->get_pre_mul(0) / rm;
    double g = ri->get_pre_mul(1) / gm;
    double b = ri->get_pre_mul(2) / bm;

    if (imatrices) {
        double rr = imatrices->rgb_cam[0][0] * r + imatrices->rgb_cam[0][1] * g + imatrices->rgb_cam[0][2] * b;
        double gg = imatrices->rgb_cam[1][0] * r + imatrices->rgb_cam[1][1] * g + imatrices->rgb_cam[1][2] * b;
        double bb = imatrices->rgb_cam[2][0] * r + imatrices->rgb_cam[2][1] * g + imatrices->rgb_cam[2][2] * b;
        r = rr;
        g = gg;
        b = bb;
    }

    rm = r / g;
    bm = b / g;
    gm = 1.0;
}




void RawImageSource::getWBMults(const ColorTemp &ctemp, const RAWParams &raw, std::array<float, 4>& out_scale_mul, float &autoGainComp, float &rm, float &gm, float &bm) const
{
    // compute channel multipliers
    double r, g, b;
    //float rm, gm, bm;

    if (ctemp.getTemp() < 0) {
        // no white balance, ie revert the pre-process white balance to restore original unbalanced raw camera color
        rm = ri->get_pre_mul(0);
        gm = ri->get_pre_mul(1);
        bm = ri->get_pre_mul(2);
    } else {
        ctemp.getMultipliers(r, g, b);
        rm = imatrices.cam_rgb[0][0] * r + imatrices.cam_rgb[0][1] * g + imatrices.cam_rgb[0][2] * b;
        gm = imatrices.cam_rgb[1][0] * r + imatrices.cam_rgb[1][1] * g + imatrices.cam_rgb[1][2] * b;
        bm = imatrices.cam_rgb[2][0] * r + imatrices.cam_rgb[2][1] * g + imatrices.cam_rgb[2][2] * b;
    }

    // adjust gain so the maximum raw value of the least scaled channel just hits max
    const float new_pre_mul[4] = { ri->get_pre_mul(0) / rm, ri->get_pre_mul(1) / gm, ri->get_pre_mul(2) / bm, ri->get_pre_mul(3) / gm };
    float new_scale_mul[4];

    bool isMono = (ri->getSensorType() == ST_FUJI_XTRANS && raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::MONO))
                  || (ri->getSensorType() == ST_BAYER && raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::MONO));

    float c_white[4];

    for (int i = 0; i < 4; ++i) {
        c_white[i] = (ri->get_white(i) - cblacksom[i]) / static_cast<float>(raw.expos) + cblacksom[i];
    }

    float gain = calculate_scale_mul(new_scale_mul, new_pre_mul, c_white, cblacksom, isMono, ri->get_colors());
    rm = new_scale_mul[0] / scale_mul[0] * gain;
    gm = new_scale_mul[1] / scale_mul[1] * gain;
    bm = new_scale_mul[2] / scale_mul[2] * gain;
    //fprintf(stderr, "camera gain: %f, current wb gain: %f, diff in stops %f\n", camInitialGain, gain, log2(camInitialGain) - log2(gain));

    const float expcomp = std::pow(2, ri->getBaselineExposure());
    rm *= expcomp;
    gm *= expcomp;
    bm *= expcomp;

    out_scale_mul[0] = scale_mul[0];
    out_scale_mul[1] = scale_mul[1];
    out_scale_mul[2] = scale_mul[2];
    out_scale_mul[3] = scale_mul[3];

    autoGainComp = camInitialGain / initialGain;
}

void RawImageSource::getImage(const ColorTemp &ctemp, int tran, Imagefloat* image, const PreviewProps &pp, const ToneCurveParams &hrp, const RAWParams &raw)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

    MyMutex::MyLock lock(getImageMutex);

    tran = defTransform(ri, tran);

    // compute channel multipliers
    double r, g, b;
    float rm, gm, bm;

    if (ctemp.getTemp() < 0) {
        // no white balance, ie revert the pre-process white balance to restore original unbalanced raw camera color
        rm = ri->get_pre_mul(0);
        gm = ri->get_pre_mul(1);
        bm = ri->get_pre_mul(2);
    } else {
        // ctemp.getMultipliers (r, g, b);
        r = g = b = 1;
        wbCamera2Mul(r, g, b);
        rm = imatrices.cam_rgb[0][0] * r + imatrices.cam_rgb[0][1] * g + imatrices.cam_rgb[0][2] * b;
        gm = imatrices.cam_rgb[1][0] * r + imatrices.cam_rgb[1][1] * g + imatrices.cam_rgb[1][2] * b;
        bm = imatrices.cam_rgb[2][0] * r + imatrices.cam_rgb[2][1] * g + imatrices.cam_rgb[2][2] * b;
    }

    if (true) {
        // adjust gain so the maximum raw value of the least scaled channel just hits max
        const float new_pre_mul[4] = { ri->get_pre_mul(0) / rm, ri->get_pre_mul(1) / gm, ri->get_pre_mul(2) / bm, ri->get_pre_mul(3) / gm };
        float new_scale_mul[4];

        bool isMono = (ri->getSensorType() == ST_FUJI_XTRANS && raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::MONO))
                      || (ri->getSensorType() == ST_BAYER && raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::MONO));

        for (int i = 0; i < 4; ++i) {
            c_white[i] = (ri->get_white(i) - cblacksom[i]) / static_cast<float>(raw.expos) + cblacksom[i];
        }

        float gain = calculate_scale_mul(new_scale_mul, new_pre_mul, c_white, cblacksom, isMono, ri->get_colors());
        rm = new_scale_mul[0] / scale_mul[0] * gain;
        gm = new_scale_mul[1] / scale_mul[1] * gain;
        bm = new_scale_mul[2] / scale_mul[2] * gain;
        //fprintf(stderr, "camera gain: %f, current wb gain: %f, diff in stops %f\n", camInitialGain, gain, log2(camInitialGain) - log2(gain));
    } else {
//        // old scaling: used a fixed reference gain based on camera (as-shot) white balance
//
//        // how much we need to scale each channel to get our new white balance
//        rm = refwb_red / rm;
//        gm = refwb_green / gm;
//        bm = refwb_blue / bm;
//        // normalize so larger multiplier becomes 1.0
//        float minval = min(rm, gm, bm);
//        rm /= minval;
//        gm /= minval;
//        bm /= minval;
//        // multiply with reference gain, ie as-shot WB
//        rm *= camInitialGain;
//        gm *= camInitialGain;
//        bm *= camInitialGain;
    }

    defGain = 0.0;
    // compute image area to render in order to provide the requested part of the image
    int sx1, sy1, imwidth, imheight, fw, d1xHeightOdd = 0;
    transformRect(pp, tran, sx1, sy1, imwidth, imheight, fw);

    // check possible overflows
    int maximwidth, maximheight;

    if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
        maximwidth = image->getHeight();
        maximheight = image->getWidth();
    } else {
        maximwidth = image->getWidth();
        maximheight = image->getHeight();
    }

    if (d1x) {
        // D1X has only half of the required rows
        // we interpolate the missing ones later to get correct aspect ratio
        // if the height is odd we also have to add an additional row to avoid a black line
        d1xHeightOdd = maximheight & 1;
        maximheight /= 2;
        imheight = maximheight;
    }

    // correct if overflow (very rare), but not fuji because it is corrected in transline
    if (!fuji && imwidth > maximwidth) {
        imwidth = maximwidth;
    }

    if (!fuji && imheight > maximheight) {
        imheight = maximheight;
    }

    if (fuji) { // zero image to avoid access to uninitialized values in further processing because fuji super-ccd processing is not clean...
        for (int i = 0; i < image->getHeight(); ++i) {
            for (int j = 0; j < image->getWidth(); ++j) {
                image->r(i, j) = image->g(i, j) = image->b(i, j) = 0;
            }
        }
    }

    int maxx = this->W, maxy = this->H, skip = pp.getSkip();

    bool iscolor = (hrp.method == "Color" || hrp.method == "Coloropp");
    const bool doClip = (chmax[0] >= clmax[0] || chmax[1] >= clmax[1] || chmax[2] >= clmax[2]) && !hrp.hrenabled && hrp.clampOOG;
    bool doHr = (hrp.hrenabled && !iscolor);

    if (hrp.hrenabled && iscolor) {
        if (!rgbSourceModified) {
            if (hrp.method == "Color") {
                if (settings->verbose) {
                    printf("Applying Highlight Recovery: Color propagation.\n");
                }

                HLRecovery_inpaint(red, green, blue, hrp.hlbl);
            } else if (hrp.method == "Coloropp"  && ctemp.getTemp() >= 0) {
                float s[3] = { rm, gm, bm };
                highlight_recovery_opposed(s, ctemp, hrp.hlth);
            }

            rgbSourceModified = true;
        }
    }

    // now apply the wb coefficients
    if (ctemp.getTemp() >= 0) {
        double r, g, b;
        ctemp.getMultipliers(r, g, b);
        wbMul2Camera(r, g, b);

        rm *= r;
        gm *= g;
        bm *= b;
    }

    hlmax[0] = clmax[0] * rm;
    hlmax[1] = clmax[1] * gm;
    hlmax[2] = clmax[2] * bm;

    const float expcomp = std::pow(2, ri->getBaselineExposure());
    rm *= expcomp;
    gm *= expcomp;
    bm *= expcomp;

    float area = skip * skip;
    rm /= area;
    gm /= area;
    bm /= area;


#ifdef _OPENMP
    #pragma omp parallel if(!d1x)       // omp disabled for D1x to avoid race conditions (see Issue 1088 http://code.google.com/p/rawtherapee/issues/detail?id=1088)
    {
#endif
        // render the requested image part
        float line_red[imwidth] ALIGNED16;
        float line_grn[imwidth] ALIGNED16;
        float line_blue[imwidth] ALIGNED16;

#ifdef _OPENMP
        #pragma omp for schedule(dynamic,16)
#endif

        for (int ix = 0; ix < imheight; ix++) {
            int i = sy1 + skip * ix;
            i = std::min(i, maxy - skip); // avoid trouble

            if (ri->getSensorType() == ST_BAYER || ri->getSensorType() == ST_FUJI_XTRANS || ri->get_colors() == 1 || ri->get_colors() == 3) {
                for (int j = 0, jx = sx1; j < imwidth; j++, jx += skip) {
                    jx = std::min(jx, maxx - skip); // avoid trouble

                    float rtot = 0.f, gtot = 0.f, btot = 0.f;

                    for (int m = 0; m < skip; m++)
                        for (int n = 0; n < skip; n++) {
                            rtot += red[i + m][jx + n];
                            gtot += green[i + m][jx + n];
                            btot += blue[i + m][jx + n];
                        }

                    rtot *= rm;
                    gtot *= gm;
                    btot *= bm;

                    if (doClip) {
                        // note: as hlmax[] can be larger than CLIP and we can later apply negative
                        // exposure this means that we can clip away local highlights which actually
                        // are not clipped. We have to do that though as we only check pixel by pixel
                        // and don't know if this will transition into a clipped area, if so we need
                        // to clip also surrounding to make a good colour transition
                        rtot = CLIP(rtot);
                        gtot = CLIP(gtot);
                        btot = CLIP(btot);
                    }

                    line_red[j] = rtot;
                    line_grn[j] = gtot;
                    line_blue[j] = btot;
                }
            } else {
                for (int j = 0, jx = sx1; j < imwidth; j++, jx += skip) {
                    if (jx > maxx - skip) {
                        jx = maxx - skip - 1;
                    }

                    float rtot, gtot, btot;
                    rtot = gtot = btot = 0;

                    for (int m = 0; m < skip; m++)
                        for (int n = 0; n < skip; n++) {
                            rtot += rawData[i + m][(jx + n) * 3 + 0];
                            gtot += rawData[i + m][(jx + n) * 3 + 1];
                            btot += rawData[i + m][(jx + n) * 3 + 2];
                        }

                    rtot *= rm;
                    gtot *= gm;
                    btot *= bm;

                    if (doClip) {
                        rtot = CLIP(rtot);
                        gtot = CLIP(gtot);
                        btot = CLIP(btot);
                    }

                    line_red[j] = rtot;
                    line_grn[j] = gtot;
                    line_blue[j] = btot;

                }
            }

            //process all highlight recovery other than "Color"
            if (doHr) {
                hlRecovery(hrp.method, line_red, line_grn, line_blue, imwidth, hlmax);
            }

            if (d1x) {
                transLineD1x(line_red, line_grn, line_blue, ix, image, tran, imwidth, imheight, d1xHeightOdd, doClip);
            } else if (fuji) {
                transLineFuji(line_red, line_grn, line_blue, ix, image, tran, imheight, fw);
            } else {
                transLineStandard(line_red, line_grn, line_blue, ix, image, tran, imwidth, imheight);
            }

        }

#ifdef _OPENMP
    }
#endif

    if (fuji) {
        int a = ((tran & TR_ROT) == TR_R90 && image->getWidth() % 2 == 0) || ((tran & TR_ROT) == TR_R180 && image->getHeight() % 2 + image->getWidth() % 2 == 1) || ((tran & TR_ROT) == TR_R270 && image->getHeight() % 2 == 0);

        // first row
        for (int j = 1 + a; j < image->getWidth() - 1; j += 2) {
            image->r(0, j) = (image->r(1, j) + image->r(0, j + 1) + image->r(0, j - 1)) / 3;
            image->g(0, j) = (image->g(1, j) + image->g(0, j + 1) + image->g(0, j - 1)) / 3;
            image->b(0, j) = (image->b(1, j) + image->b(0, j + 1) + image->b(0, j - 1)) / 3;
        }

        // other rows
        for (int i = 1; i < image->getHeight() - 1; i++) {
            for (int j = 2 - (a + i + 1) % 2; j < image->getWidth() - 1; j += 2) {
                // edge-adaptive interpolation
                float dh = (std::fabs(image->r(i, j + 1) - image->r(i, j - 1)) + std::fabs(image->g(i, j + 1) - image->g(i, j - 1)) + std::fabs(image->b(i, j + 1) - image->b(i, j - 1)));
                float dv = (std::fabs(image->r(i + 1, j) - image->r(i - 1, j)) + std::fabs(image->g(i + 1, j) - image->g(i - 1, j)) + std::fabs(image->b(i + 1, j) - image->b(i - 1, j)));
                float eh = 1.f / (1.f + dh);
                float ev = 1.f / (1.f + dv);
                image->r(i, j) = (eh * (image->r(i, j + 1) + image->r(i, j - 1)) + ev * (image->r(i + 1, j) + image->r(i - 1, j))) / (2.f * (eh + ev));
                image->g(i, j) = (eh * (image->g(i, j + 1) + image->g(i, j - 1)) + ev * (image->g(i + 1, j) + image->g(i - 1, j))) / (2.f * (eh + ev));
                image->b(i, j) = (eh * (image->b(i, j + 1) + image->b(i, j - 1)) + ev * (image->b(i + 1, j) + image->b(i - 1, j))) / (2.f * (eh + ev));
            }

            // first pixel
            if (2 - (a + i + 1) % 2 == 2) {
                image->r(i, 0) = (image->r(i + 1, 0) + image->r(i - 1, 0) + image->r(i, 1)) / 3;
                image->g(i, 0) = (image->g(i + 1, 0) + image->g(i - 1, 0) + image->g(i, 1)) / 3;
                image->b(i, 0) = (image->b(i + 1, 0) + image->b(i - 1, 0) + image->b(i, 1)) / 3;
            }

            // last pixel
            if (2 - (a + i + image->getWidth()) % 2 == 2) {
                image->r(i, image->getWidth() - 1) = (image->r(i + 1, image->getWidth() - 1) + image->r(i - 1, image->getWidth() - 1) + image->r(i, image->getWidth() - 2)) / 3;
                image->g(i, image->getWidth() - 1) = (image->g(i + 1, image->getWidth() - 1) + image->g(i - 1, image->getWidth() - 1) + image->g(i, image->getWidth() - 2)) / 3;
                image->b(i, image->getWidth() - 1) = (image->b(i + 1, image->getWidth() - 1) + image->b(i - 1, image->getWidth() - 1) + image->b(i, image->getWidth() - 2)) / 3;
            }
        }

        // last row
        int offset = (a == 1 && image->getHeight() % 2) || (a == 0 && image->getHeight() % 2 == 0);

        for (int j = 1 + offset; j < image->getWidth() - 1; j += 2) {
            image->r(image->getHeight() - 1, j) = (image->r(image->getHeight() - 2, j) + image->r(image->getHeight() - 1, j + 1) + image->r(image->getHeight() - 1, j - 1)) / 3;
            image->g(image->getHeight() - 1, j) = (image->g(image->getHeight() - 2, j) + image->g(image->getHeight() - 1, j + 1) + image->g(image->getHeight() - 1, j - 1)) / 3;
            image->b(image->getHeight() - 1, j) = (image->b(image->getHeight() - 2, j) + image->b(image->getHeight() - 1, j + 1) + image->b(image->getHeight() - 1, j - 1)) / 3;
        }
    }

    // Flip if needed
    if (tran & TR_HFLIP) {
        hflip(image);
    }

    if (tran & TR_VFLIP) {
        vflip(image);
    }

    // Colour correction (only when running on full resolution)
    if (pp.getSkip() == 1) {
        switch (ri->getSensorType()) {
            case ST_BAYER:
                processFalseColorCorrection(image, raw.bayersensor.ccSteps);
                break;

            case ST_FUJI_XTRANS:
                processFalseColorCorrection(image, raw.xtranssensor.ccSteps);
                break;

            case ST_FOVEON:
            case ST_NONE:
                break;
        }
    }
}

DCPProfile *RawImageSource::getDCP(const ColorManagementParams &cmp, DCPProfileApplyState &as)
{
    if (cmp.inputProfile == "(camera)" || cmp.inputProfile == "(none)") {
        return nullptr;
    }

    DCPProfile *dcpProf = nullptr;
    cmsHPROFILE dummy;
    findInputProfile(cmp.inputProfile, nullptr, (static_cast<const FramesData*>(getMetaData()))->getCamera(), fileName, &dcpProf, dummy);

    if (dcpProf == nullptr) {
        if (settings->verbose) {
            printf("Can't load DCP profile '%s'!\n", cmp.inputProfile.c_str());
        }

        return nullptr;
    }

    dcpProf->setStep2ApplyState(cmp.workingProfile, cmp.toneCurve, cmp.applyLookTable, cmp.applyBaselineExposureOffset, as);
    return dcpProf;
}

void RawImageSource::convertColorSpace(Imagefloat* image, const ColorManagementParams &cmp, const ColorTemp &wb)
{
    cmsHPROFILE in;
    DCPProfile *dcpProf;

    if (!findInputProfile(cmp.inputProfile, embProfile, (static_cast<const FramesData*>(getMetaData()))->getCamera(), fileName, &dcpProf, in)) {
        return;
    }

    double pre_mul[3] = { ri->get_pre_mul(0), ri->get_pre_mul(1), ri->get_pre_mul(2) };
    colorSpaceConversion_(image, cmp, wb, pre_mul, camProfile, imatrices.xyz_cam, in, dcpProf);
}

void RawImageSource::colorSpaceConversion(Imagefloat* im, const ColorManagementParams& cmp, const ColorTemp &wb, double pre_mul[3], cmsHPROFILE embedded, cmsHPROFILE camprofile, double cam[3][3], const std::string &camName, const Glib::ustring &fileName)
{
    cmsHPROFILE in;
    DCPProfile *dcpProf;
    if (findInputProfile(cmp.inputProfile, embedded, camName, fileName, &dcpProf, in)) {
        colorSpaceConversion_(im, cmp, wb, pre_mul, camprofile, cam, in, dcpProf);
    }
}

void RawImageSource::getFullSize(int& w, int& h, int tr)
{
    computeFullSize(ri, tr, w, h, border);
}


void RawImageSource::computeFullSize(const RawImage *ri, int tr, int &w, int &h, int border)
{
    tr = defTransform(ri, tr);

    const int W = ri->get_width();
    const int H = ri->get_height();
    const bool fuji = ri->get_FujiWidth() != 0;
    const bool d1x = !ri->get_model().compare("D1X");
    const int b =
        border >= 0 ? border :
        (ri->getSensorType() == ST_BAYER ? 4 :
         (ri->getSensorType() == ST_FUJI_XTRANS ? 7 : 0));

    if (fuji) {
        w = ri->get_FujiWidth() * 2 + 1;
        h = (H - ri->get_FujiWidth()) * 2 + 1;
    } else if (d1x) {
        w = W;
        h = 2 * H;
    } else {
        w = W;
        h = H;
    }

    if ((tr & TR_ROT) == TR_R90 || (tr & TR_ROT) == TR_R270) {
        int tmp = w;
        w = h;
        h = tmp;
    }

    w -= 2 * b;
    h -= 2 * b;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::getSize(const PreviewProps &pp, int& w, int& h)
{
    w = pp.getWidth() / pp.getSkip() + (pp.getWidth() % pp.getSkip() > 0);
    h = pp.getHeight() / pp.getSkip() + (pp.getHeight() % pp.getSkip() > 0);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::hflip(Imagefloat* image)
{
    image->hflip();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::vflip(Imagefloat* image)
{
    image->vflip();
}


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int RawImageSource::load(const Glib::ustring &fname, bool firstFrameOnly)
{

    MyTime t1, t2;
    t1.set();
    fileName = fname;

    if (plistener) {
        plistener->setProgressStr("PROGRESSBAR_DECODING");
        plistener->setProgress(0.0);
    }

    ri = new RawImage(fname);
    int errCode = ri->loadRaw(false, 0, false);

    if (errCode) {
        return errCode;
    }

    const bool isHasselblad = ri->get_maker() == "Hasselblad";

    numFrames = firstFrameOnly && (numFrames < 7 || !isHasselblad) ? 1 : ri->getFrameCount();

    errCode = 0;

    if (numFrames >= 7 && isHasselblad) {
        // special case to avoid crash when loading Hasselblad H6D-100cMS pixelshift files
        // limit to 6 frames and skip first frame, as first frame is not bayer
        if (firstFrameOnly) {
            numFrames = 1;
        } else {
            numFrames = 6;
        }

        riFrames.clear();
        riFrames.resize(numFrames);

#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            int errCodeThr = 0;
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (unsigned int i = 0; i < numFrames; ++i) {
                if (i == 0) {
                    riFrames[i].reset(ri);
                    errCodeThr = riFrames[i]->loadRaw(true, i + 1, true, plistener, 0.8);
                } else {
                    riFrames[i].reset(new RawImage(fname));
                    errCodeThr = riFrames[i]->loadRaw(true, i + 1);
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                errCode = errCodeThr ? errCodeThr : errCode;
            }
        }
    } else if (numFrames > 1) {
        riFrames.clear();
        riFrames.resize(numFrames);

#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            int errCodeThr = 0;
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (unsigned int i = 0; i < numFrames; ++i)
            {
                if (i == 0) {
                    riFrames[i].reset(ri);
                    errCodeThr = riFrames[i]->loadRaw(true, i, true, plistener, 0.8);
                } else {
                    riFrames[i].reset(new RawImage(fname));
                    errCodeThr = riFrames[i]->loadRaw(true, i);
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                errCode = errCodeThr ? errCodeThr : errCode;
            }
        }
    } else {
        riFrames.clear();
        riFrames.emplace_back(ri);
        errCode = riFrames.back()->loadRaw(true, 0, true, plistener, 0.8);
    }
    rawDataFrames.resize(riFrames.size());
    rawDataBuffer.clear();
    rawDataBuffer.resize(riFrames.size() - 1);

    if (!errCode) {
        for (unsigned int i = 0; i < numFrames; ++i) {
            riFrames[i]->compress_image(i);
        }
    } else {
        return errCode;
    }

    if (numFrames > 1) { // this disables multi frame support for Fuji S5 until I found a solution to handle different dimensions
        if (riFrames[0]->get_width() != riFrames[1]->get_width() || riFrames[0]->get_height() != riFrames[1]->get_height()) {
            numFrames = 1;
        }
    }

    if (plistener) {
        plistener->setProgress(0.9);
    }

    /***** Copy once constant data extracted from raw *******/
    W = ri->get_width();
    H = ri->get_height();
    fuji = ri->get_FujiWidth() != 0;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            imatrices.rgb_cam[i][j] = ri->get_colors() == 1 ? (i == j) : ri->get_rgb_cam(i, j);
        }

    // compute inverse of the color transformation matrix
    // first arg is matrix, second arg is inverse
    inverse33(imatrices.rgb_cam, imatrices.cam_rgb);

    d1x = ! ri->get_model().compare("D1X");

    if (ri->getSensorType() == ST_BAYER) {
        border = 4;
    } else if (ri->getSensorType() == ST_FUJI_XTRANS) {
        border = 7;
    } else {
        border = 0;
    }

    if (ri->get_profile()) {
        embProfile = cmsOpenProfileFromMem(ri->get_profile(), ri->get_profileLen());
    }

    // create profile
    memset(imatrices.xyz_cam, 0, sizeof(imatrices.xyz_cam));

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++) {
                imatrices.xyz_cam[i][j] += xyz_sRGB[i][k] * imatrices.rgb_cam[k][j];
            }

    camProfile = ICCStore::getInstance()->createFromMatrix(imatrices.xyz_cam, false, "Camera");
    inverse33(imatrices.xyz_cam, imatrices.cam_xyz);

    // First we get the "as shot" ("Camera") white balance and store it
    float pre_mul[4];
    // FIXME: get_colorsCoeff not so much used nowadays, when we have calculate_scale_mul() function here
    ri->get_colorsCoeff(pre_mul, scale_mul, c_black, false);//modify  for black level
    camInitialGain = max(scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]) / min(scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]);

    double camwb_red = ri->get_pre_mul(0) / pre_mul[0];
    double camwb_green = ri->get_pre_mul(1) / pre_mul[1];
    double camwb_blue = ri->get_pre_mul(2) / pre_mul[2];
    double cam_r = imatrices.rgb_cam[0][0] * camwb_red + imatrices.rgb_cam[0][1] * camwb_green + imatrices.rgb_cam[0][2] * camwb_blue;
    double cam_g = imatrices.rgb_cam[1][0] * camwb_red + imatrices.rgb_cam[1][1] * camwb_green + imatrices.rgb_cam[1][2] * camwb_blue;
    double cam_b = imatrices.rgb_cam[2][0] * camwb_red + imatrices.rgb_cam[2][1] * camwb_green + imatrices.rgb_cam[2][2] * camwb_blue;
    camera_wb = ColorTemp(cam_r, cam_g, cam_b, 1., ColorTemp::DEFAULT_OBSERVER);  // as shot WB

    if (settings->verbose) {
        printf("Raw As Shot White balance: temp %f, tint %f\n", camera_wb.getTemp(), camera_wb.getGreen());
    }

    /*{
            // Test code: if you want to test a specific white balance
        ColorTemp d50wb = ColorTemp(5000.0, 1.0, 1.0, "Custom");
        double rm,gm,bm,r,g,b;
        d50wb.getMultipliers(r, g, b);
        camwb_red = imatrices.cam_rgb[0][0]*r + imatrices.cam_rgb[0][1]*g + imatrices.cam_rgb[0][2]*b;
        camwb_green = imatrices.cam_rgb[1][0]*r + imatrices.cam_rgb[1][1]*g + imatrices.cam_rgb[1][2]*b;
        camwb_blue = imatrices.cam_rgb[2][0]*r + imatrices.cam_rgb[2][1]*g + imatrices.cam_rgb[2][2]*b;
        double pre_mul[3], dmax = 0;
        pre_mul[0] = ri->get_pre_mul(0) / camwb_red;
        pre_mul[1] = ri->get_pre_mul(1) / camwb_green;
        pre_mul[2] = ri->get_pre_mul(2) / camwb_blue;
        for (int c = 0; c < 3; c++) {
            if (dmax < pre_mul[c])
                dmax = pre_mul[c];
                }
                for (int c = 0; c < 3; c++) {
            pre_mul[c] /= dmax;
                }
                camwb_red *= dmax;
                camwb_green *= dmax;
                camwb_blue *= dmax;
                for (int c = 0; c < 3; c++) {
            int sat = ri->get_white(c) - ri->get_cblack(c);
            scale_mul[c] = pre_mul[c] * 65535.0 / sat;
                }
                scale_mul[3] = pre_mul[1] * 65535.0 / (ri->get_white(3) - ri->get_cblack(3));
                initialGain = 1.0 / min(pre_mul[0], pre_mul[1], pre_mul[2]);
    }*/

    for (unsigned int i = 0; i < numFrames; ++i) {
        riFrames[i]->set_prefilters();
    }


    // Load complete Exif information
    idata = new FramesData(fname); // TODO: std::unique_ptr<>
    idata->setDCRawFrameCount(numFrames);
    {
        int ww, hh;
        getFullSize(ww, hh);
        idata->setDimensions(ww, hh);
    }

    green(W, H);
    red(W, H);
    blue(W, H);
    //hpmap = allocArray<char>(W, H);

    if (plistener) {
        plistener->setProgress(1.0);
    }

    plistener = nullptr; // This must be reset, because only load() is called through progressConnector
    t2.set();

    if (settings->verbose) {
        printf("Load %s: %d usec\n", fname.c_str(), t2.etime(t1));
    }

    return 0; // OK!
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::preprocess(const RAWParams &raw, const LensProfParams &lensProf, const CoarseTransformParams& coarse, float &reddeha, float &greendeha, float &bluedeha,  bool prepareDenoise)
{
//    BENCHFUN
    MyTime t1, t2;
    t1.set();

    {
        // Recalculate the scaling coefficients, using auto WB if selected in the Preprocess WB param.
        // Auto WB gives us better demosaicing and CA auto-correct performance for strange white balance settings (such as UniWB)
        float dummy_cblk[4] = { 0.f }; // Avoid overwriting c_black, see issue #5676
        ri->get_colorsCoeff(ref_pre_mul, scale_mul, dummy_cblk, raw.preprocessWB.mode == RAWParams::PreprocessWB::Mode::AUTO);

        refwb_red = ri->get_pre_mul(0) / ref_pre_mul[0];
        refwb_green = ri->get_pre_mul(1) / ref_pre_mul[1];
        refwb_blue = ri->get_pre_mul(2) / ref_pre_mul[2];
        initialGain = max(scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]) / min(scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]);

        const double ref_r = imatrices.rgb_cam[0][0] * refwb_red + imatrices.rgb_cam[0][1] * refwb_green + imatrices.rgb_cam[0][2] * refwb_blue;
        const double ref_g = imatrices.rgb_cam[1][0] * refwb_red + imatrices.rgb_cam[1][1] * refwb_green + imatrices.rgb_cam[1][2] * refwb_blue;
        const double ref_b = imatrices.rgb_cam[2][0] * refwb_red + imatrices.rgb_cam[2][1] * refwb_green + imatrices.rgb_cam[2][2] * refwb_blue;
        const ColorTemp ReferenceWB = ColorTemp(ref_r, ref_g, ref_b, 1., ColorTemp::DEFAULT_OBSERVER);

        if (settings->verbose) {
            printf("Raw Reference white balance: temp %f, tint %f, multipliers [%f %f %f | %f %f %f]\n", ReferenceWB.getTemp(), ReferenceWB.getGreen(), ref_r, ref_g, ref_b, refwb_red, refwb_blue, refwb_green);
        }
    }


    Glib::ustring newDF = raw.dark_frame;
    const RawImage* rid = nullptr;

    if (!raw.df_autoselect) {
        if (!raw.dark_frame.empty()) {
            rid = DFManager::getInstance().searchDarkFrame(raw.dark_frame);
        }
    } else {
        rid = DFManager::getInstance().searchDarkFrame(idata->getMake(), idata->getModel(), idata->getISOSpeed(), idata->getShutterSpeed(), idata->getDateTimeAsTS());
    }

    if (rid && settings->verbose) {
        printf("Subtracting Darkframe:%s\n", rid->get_filename().c_str());
    }

    std::unique_ptr<PixelsMap> bitmapBads;

    int totBP = 0; // Hold count of bad pixels to correct

    if (ri->zeroIsBad() || (getMetaData()->hasFixBadPixelsConstant() && getMetaData()->getFixBadPixelsConstant() == 0)) { // mark all pixels with value zero as bad, has to be called before FF and DF. dcraw sets this flag only for some cameras (mainly Panasonic and Leica)
        bitmapBads.reset(new PixelsMap(W, H));
        totBP = findZeroPixels(*bitmapBads);

        if (settings->verbose) {
            printf("%d pixels with value zero marked as bad pixels\n", totBP);
        }
    }

    //FLATFIELD start
    RawImage *rif = nullptr;

    if (!raw.ff_AutoSelect) {
        if (!raw.ff_file.empty()) {
            rif = ffm.searchFlatField(raw.ff_file);
        }
    } else {
        rif = ffm.searchFlatField(idata->getMake(), idata->getModel(), idata->getLens(), idata->getFocalLen(), idata->getFNumber(), idata->getDateTimeAsTS());
    }

    bool hasFlatField = (rif != nullptr);

    if (hasFlatField && settings->verbose) {
        printf("Flat Field Correction:%s\n", rif->get_filename().c_str());
    }

    if (numFrames == 4) {
        int bufferNumber = 0;

        for (unsigned int i = 0; i < 4; ++i) {
            if (i == currFrame) {
                copyOriginalPixels(raw, ri, rid, rif, rawData, reddeha, greendeha, bluedeha);
                rawDataFrames[i] = &rawData;
            } else {
                if (!rawDataBuffer[bufferNumber]) {
                    rawDataBuffer[bufferNumber].reset(new array2D<float>);
                }

                rawDataFrames[i] = rawDataBuffer[bufferNumber].get();
                ++bufferNumber;
                copyOriginalPixels(raw, riFrames[i].get(), rid, rif, *rawDataFrames[i], reddeha, greendeha, bluedeha);
            }
        }
    } else if (numFrames == 2 && currFrame == 2) { // average the frames
        if (!rawDataBuffer[0]) {
            rawDataBuffer[0].reset(new array2D<float>);
        }

        rawDataFrames[1] = rawDataBuffer[0].get();
        copyOriginalPixels(raw, riFrames[1].get(), rid, rif, *rawDataFrames[1], reddeha, greendeha, bluedeha);
        copyOriginalPixels(raw, ri, rid, rif, rawData, reddeha, greendeha, bluedeha);

        for (int i = 0; i < H; ++i) {
            for (int j = 0; j < W; ++j) {
                rawData[i][j] = (rawData[i][j] + (*rawDataFrames[1])[i][j]) * 0.5f;
            }
        }
    } else {
        copyOriginalPixels(raw, ri, rid, rif, rawData, reddeha, greendeha, bluedeha);
    }

    //FLATFIELD end

    if (raw.ff_FromMetaData && isGainMapSupported()) {
        applyDngGainMap(c_black, getMetaData()->getGainMaps());
    }

    // Always correct camera badpixels from .badpixels file
    const std::vector<badPix> *bp = DFManager::getInstance().getBadPixels(ri->get_maker(), ri->get_model(), idata->getSerialNumber());

    if (bp) {
        if (!bitmapBads) {
            bitmapBads.reset(new PixelsMap(W, H));
        }

        totBP += bitmapBads->set(*bp);

        if (settings->verbose) {
            std::cout << "Correcting " << bp->size() << " pixels from .badpixels" << std::endl;
        }
    }

    // If darkframe selected, correct hotpixels found on darkframe
    bp = nullptr;

    if (raw.df_autoselect) {
        bp = DFManager::getInstance().getHotPixels(idata->getMake(), idata->getModel(), idata->getISOSpeed(), idata->getShutterSpeed(), idata->getDateTimeAsTS());
    } else if (!raw.dark_frame.empty()) {
        bp = DFManager::getInstance().getHotPixels(raw.dark_frame);
    }

    if (bp) {
        if (!bitmapBads) {
            bitmapBads.reset(new PixelsMap(W, H));
        }

        totBP += bitmapBads->set(*bp);

        if (settings->verbose && !bp->empty()) {
            std::cout << "Correcting " << bp->size() << " hotpixels from darkframe" << std::endl;
        }
    }

    if (numFrames == 4) {
        for (int i = 0; i < 4; ++i) {
            scaleColors(0, 0, W, H, raw, *rawDataFrames[i]);
        }
    } else {
        scaleColors(0, 0, W, H, raw, rawData); //+ + raw parameters for black level(raw.blackxx)
    }

    // Correct vignetting of lens profile
    if (!hasFlatField && lensProf.useVign && lensProf.lcMode != LensProfParams::LcMode::NONE) {
        std::unique_ptr<LensCorrection> pmap;

        if (lensProf.useMetadata()) {
            auto corr = MetadataLensCorrectionFinder::findCorrection(idata);
            if (corr) {
                corr->initCorrections(W, H, coarse, -1);
                pmap = std::move(corr);
            }
        } else if (lensProf.useLensfun()) {
            pmap = LFDatabase::getInstance()->findModifier(lensProf, idata, W, H, coarse, -1);
        } else {
            const std::shared_ptr<LCPProfile> pLCPProf = LCPStore::getInstance()->getProfile(lensProf.lcpFile);

            if (pLCPProf) { // don't check focal length to allow distortion correction for lenses without chip, also pass dummy focal length 1 in case of 0
                pmap.reset(new LCPMapper(pLCPProf, max(idata->getFocalLen(), 1.0), idata->getFocalLen35mm(), idata->getFocusDist(), idata->getFNumber(), true, false, W, H, coarse, -1));
            }
        }

        if (pmap) {
            LensCorrection &map = *pmap;

            if (ri->getSensorType() == ST_BAYER || ri->getSensorType() == ST_FUJI_XTRANS || ri->get_colors() == 1) {
                if (numFrames == 4) {
                    for (int i = 0; i < 4; ++i) {
                        map.processVignette(W, H, *rawDataFrames[i]);
                    }
                } else {
                    map.processVignette(W, H, rawData);
                }
            } else if (ri->get_colors() == 3) {
                map.processVignette3Channels(W, H, rawData);
            }
        }
    }

    defGain = 0.0;//log(initialGain) / log(2.0);

    if (ri->getSensorType() == ST_BAYER && (raw.hotPixelFilter > 0 || raw.deadPixelFilter > 0)) {
        if (plistener) {
            plistener->setProgressStr("PROGRESSBAR_HOTDEADPIXELFILTER");
            plistener->setProgress(0.0);
        }

        if (!bitmapBads) {
            bitmapBads.reset(new PixelsMap(W, H));
        }

        int nFound = findHotDeadPixels(*bitmapBads, raw.hotdeadpix_thresh, raw.hotPixelFilter, raw.deadPixelFilter);
        totBP += nFound;

        if (settings->verbose && nFound > 0) {
            printf("Correcting %d hot/dead pixels found inside image\n", nFound);
        }
    }

    if (ri->getSensorType() == ST_BAYER && raw.bayersensor.pdafLinesFilter) {
        PDAFLinesFilter f(ri);

        if (!bitmapBads) {
            bitmapBads.reset(new PixelsMap(W, H));
        }

        int n = f.mark(rawData, *bitmapBads);
        totBP += n;

        if (n > 0) {
            if (settings->verbose) {
                printf("Marked %d hot pixels from PDAF lines\n", n);
            }

            auto &thresh = f.greenEqThreshold();

            if (numFrames == 4) {
                for (int i = 0; i < 4; ++i) {
                    green_equilibrate(thresh, *rawDataFrames[i]);
                }
            } else {
                green_equilibrate(thresh, rawData);
            }
        }
    }

    // check if green equilibration is needed. If yes, compute G channel pre-compensation factors
    const auto globalGreenEq =
    [&]() -> bool {
        CameraConstantsStore *ccs = CameraConstantsStore::getInstance();
        const CameraConst *cc = ccs->get(ri->get_maker().c_str(), ri->get_model().c_str());
        return cc && cc->get_globalGreenEquilibration();
    };

    if (ri->getSensorType() == ST_BAYER && (raw.bayersensor.greenthresh || (globalGreenEq() && raw.bayersensor.method != RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::VNG4)))) {
        if (settings->verbose) {
            printf("Performing global green equilibration...\n");
        }

        // global correction
        if (numFrames == 4) {
            for (int i = 0; i < 4; ++i) {
                green_equilibrate_global(*rawDataFrames[i]);
            }
        } else {
            green_equilibrate_global(rawData);
        }
    }

    if (ri->getSensorType() == ST_BAYER && raw.bayersensor.greenthresh > 0) {
        if (plistener) {
            plistener->setProgressStr("PROGRESSBAR_GREENEQUIL");
            plistener->setProgress(0.0);
        }

        GreenEqulibrateThreshold thresh(0.01 * raw.bayersensor.greenthresh);

        if (numFrames == 4) {
            for (int i = 0; i < 4; ++i) {
                green_equilibrate(thresh, *rawDataFrames[i]);
            }
        } else {
            green_equilibrate(thresh, rawData);
        }
    }


    if (totBP) {
        if (ri->getSensorType() == ST_BAYER) {
            if (numFrames == 4) {
                for (int i = 0; i < 4; ++i) {
                    interpolateBadPixelsBayer(*bitmapBads, *rawDataFrames[i]);
                }
            } else {
                interpolateBadPixelsBayer(*bitmapBads, rawData);
            }
        } else if (ri->getSensorType() == ST_FUJI_XTRANS) {
            interpolateBadPixelsXtrans(*bitmapBads);
        } else {
            interpolateBadPixelsNColours(*bitmapBads, ri->get_colors());
        }
    }

    if (ri->getSensorType() == ST_BAYER && raw.bayersensor.linenoise > 0) {
        if (plistener) {
            plistener->setProgressStr("PROGRESSBAR_LINEDENOISE");
            plistener->setProgress(0.0);
        }

        std::unique_ptr<CFALineDenoiseRowBlender> line_denoise_rowblender;

        if (raw.bayersensor.linenoiseDirection == RAWParams::BayerSensor::LineNoiseDirection::PDAF_LINES) {
            PDAFLinesFilter f(ri);
            line_denoise_rowblender = f.lineDenoiseRowBlender();
        } else {
            line_denoise_rowblender.reset(new CFALineDenoiseRowBlender());
        }

        cfa_linedn(0.00002 * (raw.bayersensor.linenoise), int(raw.bayersensor.linenoiseDirection) & int(RAWParams::BayerSensor::LineNoiseDirection::VERTICAL), int(raw.bayersensor.linenoiseDirection) & int(RAWParams::BayerSensor::LineNoiseDirection::HORIZONTAL), *line_denoise_rowblender);
    }

    if ((raw.ca_autocorrect || std::fabs(raw.cared) > 0.001 || std::fabs(raw.cablue) > 0.001) && ri->getSensorType() == ST_BAYER) { // Auto CA correction disabled for X-Trans, for now...
        if (plistener) {
            plistener->setProgressStr("PROGRESSBAR_RAWCACORR");
            plistener->setProgress(0.0);
        }

        const auto& options = App::get().options();
        if (numFrames == 4) {
            double fitParams[64];
            float *buffer = CA_correct_RT(raw.ca_autocorrect, raw.caautoiterations, raw.cared, raw.cablue, raw.ca_avoidcolourshift, raw.bayersensor.border, *rawDataFrames[0], fitParams, false, true, nullptr, false, options.chunkSizeCA, options.measure);
            for (int i = 1; i < 3; ++i) {
                CA_correct_RT(raw.ca_autocorrect, raw.caautoiterations, raw.cared, raw.cablue, raw.ca_avoidcolourshift, raw.bayersensor.border, *rawDataFrames[i], fitParams, true, false, buffer, false, options.chunkSizeCA, options.measure);
            }

            CA_correct_RT(raw.ca_autocorrect, raw.caautoiterations, raw.cared, raw.cablue, raw.ca_avoidcolourshift, raw.bayersensor.border, *rawDataFrames[3], fitParams, true, false, buffer, true, options.chunkSizeCA, options.measure);
        } else {
            CA_correct_RT(raw.ca_autocorrect, raw.caautoiterations, raw.cared, raw.cablue, raw.ca_avoidcolourshift, raw.bayersensor.border, rawData, nullptr, false, false, nullptr, true, options.chunkSizeCA, options.measure);
        }
    }

    if (prepareDenoise) {
        LUTu aehist;
        int aehistcompr;
        double clip = 0;
        int brightness, contrast, black, hlcompr, hlcomprthresh;
        getAutoExpHistogram(aehist, aehistcompr);
        ImProcFunctions::getAutoExp(aehist, aehistcompr, clip, dirpyrdenoiseExpComp, brightness, contrast, black, hlcompr, hlcomprthresh);
    }

    t2.set();

    if (settings->verbose) {
        printf("Preprocessing: %d usec\n", t2.etime(t1));
    }

    rawDirty = true;
    return;
}
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::demosaic(const RAWParams &raw, bool autoContrast, double &contrastThreshold, bool cache)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

    MyTime t1, t2;
    t1.set();

    const auto& options = App::get().options();
    if (ri->getSensorType() == ST_BAYER) {
        if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::HPHD)) {
            hphd_demosaic();
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::VNG4)) {
            vng4_demosaic(rawData, red, green, blue);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::AHD)) {
            ahd_demosaic();
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::AMAZE)) {
            amaze_demosaic_RT(0, 0, W, H, rawData, red, green, blue, options.chunkSizeAMAZE, options.measure);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::AMAZEBILINEAR)
                   || raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::AMAZEVNG4)
                   || raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::DCBBILINEAR)
                   || raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::DCBVNG4)
                   || raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::RCDBILINEAR)
                   || raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::RCDVNG4)) {
            if (!autoContrast) {
                double threshold = raw.bayersensor.dualDemosaicContrast;
                dual_demosaic_RT(true, raw, W, H, rawData, red, green, blue, threshold, false);
            } else {
                dual_demosaic_RT(true, raw, W, H, rawData, red, green, blue, contrastThreshold, true);
            }
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::PIXELSHIFT)) {
            pixelshift(0, 0, W, H, raw, currFrame, ri->get_maker(), ri->get_model(), raw.expos);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::DCB)) {
            dcb_demosaic(raw.bayersensor.dcb_iterations, raw.bayersensor.dcb_enhance);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::EAHD)) {
            eahd_demosaic();
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::IGV)) {
            igv_interpolate(W, H);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::LMMSE)) {
            lmmse_interpolate_omp(W, H, rawData, red, green, blue, raw.bayersensor.lmmse_iterations);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::FAST)) {
            fast_demosaic();
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::MONO)) {
            nodemosaic(true);
        } else if (raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::RCD)) {
            rcd_demosaic(options.chunkSizeRCD, options.measure);
        } else {
            nodemosaic(false);
        }
    } else if (ri->getSensorType() == ST_FUJI_XTRANS) {
        if (raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::FAST)) {
            fast_xtrans_interpolate(rawData, red, green, blue);
        } else if (raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::ONE_PASS)) {
            xtrans_interpolate(1, false, options.chunkSizeXT, options.measure);
        } else if (raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::THREE_PASS)) {
            xtrans_interpolate(3, true, options.chunkSizeXT, options.measure);
        } else if (raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::FOUR_PASS) || raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::TWO_PASS)) {
            if (!autoContrast) {
                double threshold = raw.xtranssensor.dualDemosaicContrast;
                dual_demosaic_RT(false, raw, W, H, rawData, red, green, blue, threshold, false);
            } else {
                dual_demosaic_RT(false, raw, W, H, rawData, red, green, blue, contrastThreshold, true);
            }
        } else if (raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::MONO)) {
            nodemosaic(true);
        } else {
            nodemosaic(false);
        }
    } else if (ri->get_colors() == 1) {
        // Monochrome
        nodemosaic(true);
    } else {
        // RGB
        nodemosaic(false);
    }

    t2.set();


    rgbSourceModified = false;

    if (cache) {
        if (!redCache) {
            redCache = new array2D<float>(W, H);
            greenCache = new array2D<float>(W, H);
            blueCache = new array2D<float>(W, H);
        }

#ifdef _OPENMP
        #pragma omp parallel sections
#endif
        {
#ifdef _OPENMP
            #pragma omp section
#endif

            for (int i = 0; i < H; ++i) {
                for (int j = 0; j < W; ++j) {
                    (*redCache)[i][j] = red[i][j];
                }
            }

#ifdef _OPENMP
            #pragma omp section
#endif

            for (int i = 0; i < H; ++i) {
                for (int j = 0; j < W; ++j) {
                    (*greenCache)[i][j] = green[i][j];
                }
            }

#ifdef _OPENMP
            #pragma omp section
#endif

            for (int i = 0; i < H; ++i) {
                for (int j = 0; j < W; ++j) {
                    (*blueCache)[i][j] = blue[i][j];
                }
            }
        }
    } else {
        delete redCache;
        redCache = nullptr;
        delete greenCache;
        greenCache = nullptr;
        delete blueCache;
        blueCache = nullptr;
    }

    if (settings->verbose) {
        if (getSensorType() == ST_BAYER) {
            printf("Demosaicing Bayer data: %s - %d usec\n", raw.bayersensor.method.c_str(), t2.etime(t1));
        } else if (getSensorType() == ST_FUJI_XTRANS) {
            printf("Demosaicing X-Trans data: %s - %d usec\n", raw.xtranssensor.method.c_str(), t2.etime(t1));
        }
    }
}


//void RawImageSource::retinexPrepareBuffers(ColorManagementParams cmp, RetinexParams retinexParams, multi_array2D<float, 3> &conversionBuffer, LUTu &lhist16RETI)
void RawImageSource::retinexPrepareBuffers(const ColorManagementParams& cmp, const RetinexParams &retinexParams, multi_array2D<float, 4> &conversionBuffer, LUTu &lhist16RETI)
{
    bool useHsl = (retinexParams.retinexcolorspace == "HSLLOG" || retinexParams.retinexcolorspace == "HSLLIN");
    conversionBuffer[0](W - 2 * border, H - 2 * border);
    conversionBuffer[1](W - 2 * border, H - 2 * border);
    conversionBuffer[2](W - 2 * border, H - 2 * border);
    conversionBuffer[3](W - 2 * border, H - 2 * border);

    LUTf *retinexgamtab = nullptr;//gamma before and after Retinex to restore tones
    LUTf lutTonereti;

    if (retinexParams.gammaretinex == "low") {
        retinexgamtab = &(Color::gammatab_115_2);
    } else if (retinexParams.gammaretinex == "mid") {
        retinexgamtab = &(Color::gammatab_13_2);
    } else if (retinexParams.gammaretinex == "hig") {
        retinexgamtab = &(Color::gammatab_145_3);
    } else if (retinexParams.gammaretinex == "fre") {
        GammaValues g_a;
        double pwr = 1.0 / retinexParams.gam;
        double gamm = retinexParams.gam;
        double ts = retinexParams.slope;
        double gamm2 = retinexParams.gam;

        if (gamm2 < 1.) {
            std::swap(pwr, gamm);
        }

        Color::calcGamma(pwr, ts, g_a); // call to calcGamma with selected gamma and slope

        double start;
        double add;

        if (gamm2 < 1.) {
            start = g_a[2];
            add = g_a[4];
        } else {
            start = g_a[3];
            add = g_a[4];
        }

        double mul = 1. + g_a[4];

        lutTonereti(65536);

        for (int i = 0; i < 65536; i++) {
            double val = (i) / 65535.;
            double x;

            if (gamm2 < 1.) {
                x = Color::igammareti(val, gamm, start, ts, mul, add);
            } else {
                x = Color::gammareti(val, gamm, start, ts, mul, add);
            }

            lutTonereti[i] = CLIP(x * 65535.);// CLIP avoid in some case extra values
        }

        retinexgamtab = &lutTonereti;
    }

    /*
    //test with amsterdam.pef and other files
    float rr,gg,bb;
    rr=red[50][2300];
    gg=green[50][2300];
    bb=blue[50][2300];
    printf("rr=%f gg=%f bb=%f \n",rr,gg,bb);
    rr=red[1630][370];
    gg=green[1630][370];
    bb=blue[1630][370];
    printf("rr1=%f gg1=%f bb1=%f \n",rr,gg,bb);
    rr=red[380][1630];
    gg=green[380][1630];
    bb=blue[380][1630];
    printf("rr2=%f gg2=%f bb2=%f \n",rr,gg,bb);
    */
    /*
    if (retinexParams.highlig < 100 && retinexParams.retinexMethod == "highliplus") {//try to recover magenta...very difficult !
        float hig = ((float)retinexParams.highlig)/100.f;
        float higgb = ((float)retinexParams.grbl)/100.f;

    #ifdef _OPENMP
            #pragma omp parallel for
    #endif
            for (int i = border; i < H - border; i++) {
                for (int j = border; j < W - border; j++) {
                    float R_,G_,B_;
                    R_=red[i][j];
                    G_=green[i][j];
                    B_=blue[i][j];

                    //empirical method to find highlight magenta with no conversion RGB and no white balance
                    //red = master   Gr and Bl default higgb=0.5
         //           if (R_>65535.f*hig  && G_ > 65535.f*higgb && B_ > 65535.f*higgb) conversionBuffer[3][i - border][j - border] = R_;
          //          else conversionBuffer[3][i - border][j - border] = 0.f;
                }
            }
    }
    */
    if (retinexParams.gammaretinex != "none" && retinexParams.str != 0 && retinexgamtab) {//gamma

#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = border; i < H - border; i++) {
            for (int j = border; j < W - border; j++) {
                float R_, G_, B_;
                R_ = red[i][j];
                G_ = green[i][j];
                B_ = blue[i][j];

                red[i][j] = (*retinexgamtab)[R_];
                green[i][j] = (*retinexgamtab)[G_];
                blue[i][j] = (*retinexgamtab)[B_];
            }
        }
    }

    if (useHsl) {
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            // one LUT per thread
            LUTu lhist16RETIThr;

            if (lhist16RETI)
            {
                lhist16RETIThr(lhist16RETI.getSize());
                lhist16RETIThr.clear();
            }

#if defined(__SSE2__) || defined(RT_SIMDE)
            vfloat c32768 = F2V(32768.f);
#endif
#ifdef _OPENMP
            #pragma omp for
#endif

            for (int i = border; i < H - border; i++)
            {
                int j = border;
#if defined(__SSE2__) || defined(RT_SIMDE)

                for (; j < W - border - 3; j += 4) {
                    vfloat H, S, L;
                    Color::rgb2hsl(LVFU(red[i][j]), LVFU(green[i][j]), LVFU(blue[i][j]), H, S, L);
                    STVFU(conversionBuffer[0][i - border][j - border], H);
                    STVFU(conversionBuffer[1][i - border][j - border], S);
                    L *= c32768;
                    STVFU(conversionBuffer[2][i - border][j - border], L);
                    STVFU(conversionBuffer[3][i - border][j - border], H);

                    if (lhist16RETI) {
                        for (int p = 0; p < 4; p++) {
                            int pos = (conversionBuffer[2][i - border][j - border + p]);//histogram in curve HSL
                            lhist16RETIThr[pos]++;
                        }
                    }
                }

#endif

                for (; j < W - border; j++) {
                    float L;
                    //rgb=>lab
                    Color::rgb2hslfloat(red[i][j], green[i][j], blue[i][j], conversionBuffer[0][i - border][j - border], conversionBuffer[1][i - border][j - border], L);
                    L *= 32768.f;
                    conversionBuffer[2][i - border][j - border] = L;

                    if (lhist16RETI) {
                        int pos = L;
                        lhist16RETIThr[pos]++;
                    }
                }
            }

#ifdef _OPENMP
            #pragma omp critical
            {
                if (lhist16RETI)
                {
                    lhist16RETI += lhist16RETIThr; // Add per Thread LUT to global LUT
                }
            }
#endif

        }
    } else {
        TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(cmp.workingProfile);
        const float wp[3][3] = {
            {static_cast<float>(wprof[0][0]), static_cast<float>(wprof[0][1]), static_cast<float>(wprof[0][2])},
            {static_cast<float>(wprof[1][0]), static_cast<float>(wprof[1][1]), static_cast<float>(wprof[1][2])},
            {static_cast<float>(wprof[2][0]), static_cast<float>(wprof[2][1]), static_cast<float>(wprof[2][2])}
        };

        // Conversion rgb -> lab is hard to vectorize because it uses a lut (that's not the main problem)
        // and it uses a condition inside XYZ2Lab which is almost impossible to vectorize without making it slower...
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            // one LUT per thread
            LUTu lhist16RETIThr;

            if (lhist16RETI) {
                lhist16RETIThr(lhist16RETI.getSize());
                lhist16RETIThr.clear();
            }

#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16)
#endif

            for (int i = border; i < H - border; i++)
                for (int j = border; j < W - border; j++) {
                    float X, Y, Z, L, aa, bb;
                    //rgb=>lab
                    Color::rgbxyz(red[i][j], green[i][j], blue[i][j], X, Y, Z, wp);
                    //convert Lab
                    Color::XYZ2Lab(X, Y, Z, L, aa, bb);
                    conversionBuffer[0][i - border][j - border] = aa;
                    conversionBuffer[1][i - border][j - border] = bb;
                    conversionBuffer[2][i - border][j - border] = L;
                    conversionBuffer[3][i - border][j - border] = xatan2f(bb, aa);

//                   if (R_>40000.f  && G_ > 30000.f && B_ > 30000.f) conversionBuffer[3][i - border][j - border] = R_;
//                   else conversionBuffer[3][i - border][j - border] = 0.f;
                    if (lhist16RETI) {
                        int pos = L;
                        lhist16RETIThr[pos]++;//histogram in Curve Lab
                    }
                }

#ifdef _OPENMP
            #pragma omp critical
            {
                if (lhist16RETI) {
                    lhist16RETI += lhist16RETIThr; // Add per Thread LUT to global LUT
                }
            }
#endif

        }
    }



}

void RawImageSource::retinexPrepareCurves(const RetinexParams &retinexParams, LUTf &cdcurve, LUTf &mapcurve, RetinextransmissionCurve &retinextransmissionCurve, RetinexgaintransmissionCurve &retinexgaintransmissionCurve, bool &retinexcontlutili, bool &mapcontlutili, bool &useHsl, LUTu & lhist16RETI, LUTu & histLRETI)
{
    useHsl = (retinexParams.retinexcolorspace == "HSLLOG" || retinexParams.retinexcolorspace == "HSLLIN");

    if (useHsl) {
        retinexcontlutili = CurveFactory::diagonalCurve2Lut(retinexParams.cdHcurve, cdcurve, 1, lhist16RETI, histLRETI);
    } else {
        retinexcontlutili = CurveFactory::diagonalCurve2Lut(retinexParams.cdcurve, cdcurve, 1, lhist16RETI, histLRETI);
    }

    mapcontlutili = CurveFactory::diagonalCurve2Lut(retinexParams.mapcurve, mapcurve, 1, lhist16RETI, histLRETI);
    mapcurve *= 0.5f;
    retinexParams.getCurves(retinextransmissionCurve, retinexgaintransmissionCurve);
}

void RawImageSource::retinex(const ColorManagementParams& cmp, const RetinexParams &deh, const ToneCurveParams& Tc, LUTf & cdcurve, LUTf & mapcurve, const RetinextransmissionCurve & dehatransmissionCurve, const RetinexgaintransmissionCurve & dehagaintransmissionCurve, multi_array2D<float, 4> &conversionBuffer, bool dehacontlutili, bool mapcontlutili, bool useHsl, float &minCD, float &maxCD, float &mini, float &maxi, float &Tmean, float &Tsigma, float &Tmin, float &Tmax, LUTu &histLRETI)
{
    MyTime t4, t5;
    t4.set();

    if (settings->verbose) {
        printf("Applying Retinex\n");
    }

    LUTf lutToneireti;
    lutToneireti(65536);

    LUTf *retinexigamtab = nullptr;//gamma before and after Retinex to restore tones

    if (deh.gammaretinex == "low") {
        retinexigamtab = &(Color::igammatab_115_2);
    } else if (deh.gammaretinex == "mid") {
        retinexigamtab = &(Color::igammatab_13_2);
    } else if (deh.gammaretinex == "hig") {
        retinexigamtab = &(Color::igammatab_145_3);
    } else if (deh.gammaretinex == "fre") {
        GammaValues g_a;
        double pwr = 1.0 / deh.gam;
        double gamm = deh.gam;
        double gamm2 = gamm;
        double ts = deh.slope;

        if (gamm2 < 1.) {
            std::swap(pwr, gamm);
        }

        Color::calcGamma(pwr, ts, g_a); // call to calcGamma with selected gamma and slope

        double mul = 1. + g_a[4];
        double add;
        double start;

        if (gamm2 < 1.) {
            start = g_a[3];
            add = g_a[3];
        } else {
            add = g_a[4];
            start = g_a[2];
        }

        //    printf("g_a0=%f g_a1=%f g_a2=%f g_a3=%f g_a4=%f\n", g_a0,g_a1,g_a2,g_a3,g_a4);
        for (int i = 0; i < 65536; i++) {
            double val = (i) / 65535.;
            double x;

            if (gamm2 < 1.) {
                x = Color::gammareti(val, gamm, start, ts, mul, add);
            } else {
                x = Color::igammareti(val, gamm, start, ts, mul, add);
            }

            lutToneireti[i] = CLIP(x * 65535.);
        }

        retinexigamtab = &lutToneireti;
    }

    // We need a buffer with original L data to allow correct blending
    // red, green and blue still have original size of raw, but we can't use the borders
    const int HNew = H - 2 * border;
    const int WNew = W - 2 * border;

    array2D<float> LBuffer(WNew, HNew);
    float **temp = conversionBuffer[2]; // one less dereference
    LUTf dLcurve;
    LUTu hist16RET;

    if (dehacontlutili && histLRETI) {
        hist16RET(32768);
        hist16RET.clear();
        histLRETI.clear();
        dLcurve(32768);
    }

    FlatCurve* chcurve = nullptr;//curve c=f(H)
    bool chutili = false;

    if (deh.enabled && deh.retinexMethod == "highli") {
        chcurve = new FlatCurve(deh.lhcurve);

        if (!chcurve || chcurve->isIdentity()) {
            if (chcurve) {
                delete chcurve;
                chcurve = nullptr;
            }
        } else {
            chutili = true;
        }
    }



#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        // one LUT per thread
        LUTu hist16RETThr;

        if (hist16RET) {
            hist16RETThr(hist16RET.getSize());
            hist16RETThr.clear();
        }

#ifdef _OPENMP
        #pragma omp for
#endif

        for (int i = 0; i < H - 2 * border; i++)
            if (dehacontlutili)
                for (int j = 0; j < W - 2 * border; j++) {
                    LBuffer[i][j] = cdcurve[2.f * temp[i][j]] / 2.f;

                    if (histLRETI) {
                        int pos = LBuffer[i][j];
                        hist16RETThr[pos]++; //histogram in Curve
                    }
                } else
                for (int j = 0; j < W - 2 * border; j++) {
                    LBuffer[i][j] = temp[i][j];
                }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            if (hist16RET) {
                hist16RET += hist16RETThr; // Add per Thread LUT to global LUT
            }
        }
    }

    if (hist16RET) {//update histogram
        // TODO : When rgbcurvesspeedup branch is merged into master, replace this by the following 1-liner
        // hist16RET.compressTo(histLRETI);
        // also remove declaration and init of dLcurve some lines above then and finally remove this comment :)
        for (int i = 0; i < 32768; i++) {
            float val = (double)i / 32767.0;
            dLcurve[i] = val;
        }

        for (int i = 0; i < 32768; i++) {
            float hval = dLcurve[i];
            int hi = (int)(255.0f * hval);
            histLRETI[hi] += hist16RET[i];
        }
    }

    MSR(LBuffer, conversionBuffer[2], conversionBuffer[3], mapcurve, mapcontlutili, WNew, HNew, deh, dehatransmissionCurve, dehagaintransmissionCurve, minCD, maxCD, mini, maxi, Tmean, Tsigma, Tmin, Tmax);

    if (useHsl) {
        if (chutili) {
#ifdef _OPENMP
            #pragma omp parallel for
#endif

            for (int i = border; i < H - border; i++) {
                int j = border;

                for (; j < W - border; j++) {

                    float valp = (chcurve->getVal(conversionBuffer[3][i - border][j - border]) - 0.5f);
                    conversionBuffer[1][i - border][j - border] *= (1.f + 2.f * valp);

                }
            }
        }

#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = border; i < H - border; i++) {
            int j = border;
#if defined(__SSE2__) || defined(RT_SIMDE)
            vfloat c32768 = F2V(32768.f);

            for (; j < W - border - 3; j += 4) {
                vfloat R, G, B;
                Color::hsl2rgb(LVFU(conversionBuffer[0][i - border][j - border]), LVFU(conversionBuffer[1][i - border][j - border]), LVFU(LBuffer[i - border][j - border]) / c32768, R, G, B);

                STVFU(red[i][j], R);
                STVFU(green[i][j], G);
                STVFU(blue[i][j], B);
            }

#endif

            for (; j < W - border; j++) {
                Color::hsl2rgbfloat(conversionBuffer[0][i - border][j - border], conversionBuffer[1][i - border][j - border], LBuffer[i - border][j - border] / 32768.f, red[i][j], green[i][j], blue[i][j]);
            }
        }

    } else {
        TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(cmp.workingProfile);

        double wip[3][3] = {
            {wiprof[0][0], wiprof[0][1], wiprof[0][2]},
            {wiprof[1][0], wiprof[1][1], wiprof[1][2]},
            {wiprof[2][0], wiprof[2][1], wiprof[2][2]}
        };
        // gamut control only in Lab mode
        const bool highlight = Tc.hrenabled;
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
#if defined(__SSE2__) || defined(RT_SIMDE)
            // we need some line buffers to precalculate some expensive stuff using SSE
            float atan2Buffer[W] ALIGNED16;
            float sqrtBuffer[W] ALIGNED16;
            float sincosxBuffer[W] ALIGNED16;
            float sincosyBuffer[W] ALIGNED16;
            const vfloat c327d68v = F2V(327.68);
            const vfloat onev = F2V(1.f);
#endif // __SSE2__
#ifdef _OPENMP
            #pragma omp for
#endif

            for (int i = border; i < H - border; i++) {
#if defined(__SSE2__) || defined(RT_SIMDE)
                // vectorized precalculation
                {
                    int j = border;

                    for (; j < W - border - 3; j += 4)
                    {
                        vfloat av = LVFU(conversionBuffer[0][i - border][j - border]);
                        vfloat bv = LVFU(conversionBuffer[1][i - border][j - border]);
                        vfloat chprovv = vsqrtf(SQRV(av) + SQRV(bv));
                        STVF(sqrtBuffer[j - border], chprovv / c327d68v);
                        vfloat HHv = xatan2f(bv, av);
                        STVF(atan2Buffer[j - border], HHv);
                        av /= chprovv;
                        bv /= chprovv;
                        vmask selMask = vmaskf_eq(chprovv, ZEROV);
                        STVF(sincosyBuffer[j - border], vself(selMask, onev, av));
                        STVF(sincosxBuffer[j - border], vselfnotzero(selMask, bv));
                    }

                    for (; j < W - border; j++)
                    {
                        float aa = conversionBuffer[0][i - border][j - border];
                        float bb = conversionBuffer[1][i - border][j - border];
                        float Chprov1 = std::sqrt(SQR(aa) + SQR(bb)) / 327.68f;
                        sqrtBuffer[j - border] = Chprov1;
                        float HH = xatan2f(bb, aa);
                        atan2Buffer[j - border] = HH;

                        if (Chprov1 == 0.0f) {
                            sincosyBuffer[j - border] = 1.f;
                            sincosxBuffer[j - border] = 0.0f;
                        } else {
                            sincosyBuffer[j - border] = aa / (Chprov1 * 327.68f);
                            sincosxBuffer[j - border] = bb / (Chprov1 * 327.68f);
                        }
                    }
                }
#endif // __SSE2__

                for (int j = border; j < W - border; j++) {
                    float Lprov1 = (LBuffer[i - border][j - border]) / 327.68f;
#if defined(__SSE2__) || defined(RT_SIMDE)
                    float Chprov1 = sqrtBuffer[j - border];
                    float  HH = atan2Buffer[j - border];
                    float2 sincosval;
                    sincosval.x = sincosxBuffer[j - border];
                    sincosval.y = sincosyBuffer[j - border];

#else
                    float aa = conversionBuffer[0][i - border][j - border];
                    float bb = conversionBuffer[1][i - border][j - border];
                    float Chprov1 = std::sqrt(SQR(aa) + SQR(bb)) / 327.68f;
                    float  HH = xatan2f(bb, aa);
                    float2 sincosval;// = xsincosf(HH);

                    if (Chprov1 == 0.0f) {
                        sincosval.y = 1.f;
                        sincosval.x = 0.0f;
                    } else {
                        sincosval.y = aa / (Chprov1 * 327.68f);
                        sincosval.x = bb / (Chprov1 * 327.68f);
                    }

#endif

                    if (chutili) {  // c=f(H)
                        float valp = float((chcurve->getVal(Color::huelab_to_huehsv2(HH)) - 0.5f));
                        Chprov1 *= (1.f + 2.f * valp);
                    }

                    float R, G, B;
                    //gamut control : Lab values are in gamut
                    Color::gamutLchonly(HH, sincosval, Lprov1, Chprov1, R, G, B, wip, highlight, 0.15f, 0.96f);



                    conversionBuffer[0][i - border][j - border] = 327.68f * Chprov1 * sincosval.y;
                    conversionBuffer[1][i - border][j - border] = 327.68f * Chprov1 * sincosval.x;
                    LBuffer[i - border][j - border] = Lprov1 * 327.68f;
                }
            }
        }
        //end gamut control
#if defined(__SSE2__) || defined(RT_SIMDE)
        vfloat wipv[3][3];

        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                wipv[i][j] = F2V(wiprof[i][j]);
            }

#endif // __SSE2__
#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = border; i < H - border; i++) {
            int j = border;
#if defined(__SSE2__) || defined(RT_SIMDE)

            for (; j < W - border - 3; j += 4) {
                vfloat x_, y_, z_;
                vfloat R, G, B;
                Color::Lab2XYZ(LVFU(LBuffer[i - border][j - border]), LVFU(conversionBuffer[0][i - border][j - border]), LVFU(conversionBuffer[1][i - border][j - border]), x_, y_, z_) ;
                Color::xyz2rgb(x_, y_, z_, R, G, B, wipv);

                STVFU(red[i][j], R);
                STVFU(green[i][j], G);
                STVFU(blue[i][j], B);

            }

#endif

            for (; j < W - border; j++) {
                float x_, y_, z_;
                float R, G, B;
                Color::Lab2XYZ(LBuffer[i - border][j - border], conversionBuffer[0][i - border][j - border], conversionBuffer[1][i - border][j - border], x_, y_, z_) ;
                Color::xyz2rgb(x_, y_, z_, R, G, B, wip);
                red[i][j] = R;
                green[i][j] = G;
                blue[i][j] = B;
            }
        }
    }

    if (chcurve) {
        delete chcurve;
    }

    if (deh.gammaretinex != "none"  && deh.str != 0) { //inverse gamma
#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = border; i < H - border; i++) {
            for (int j = border; j < W - border; j++) {
                float R_, G_, B_;
                R_ = red[i][j];
                G_ = green[i][j];
                B_ = blue[i][j];
                red[i][j] = (*retinexigamtab)[R_];
                green[i][j] = (*retinexigamtab)[G_];
                blue[i][j] = (*retinexigamtab)[B_];
            }
        }
    }

    rgbSourceModified = false; // tricky handling for Color propagation
    t5.set();

    if (settings->verbose) {
        printf("Retinex=%d usec\n",  t5.etime(t4));
    }

}

void RawImageSource::flush()
{
    for (auto &buffer : rawDataBuffer) {
        buffer.reset();
    }

    if (rawData) {
        rawData(0, 0);
    }

    if (green) {
        green(0, 0);
    }

    if (red) {
        red(0, 0);
    }

    if (blue) {
        blue(0, 0);
    }

    if (greenloc) {
        greenloc(0, 0);
    }

    if (redloc) {
        redloc(0, 0);
    }

    if (blueloc) {
        blueloc(0, 0);
    }
}

void RawImageSource::HLRecovery_Global(const ToneCurveParams &hrp)
{
//   if (hrp.hrenabled && hrp.method == "Color") {
//       if (!rgbSourceModified) {
//           if (settings->verbose) {
//               printf ("Applying Highlight Recovery: Color propagation...\n");
//           }
//
//           HLRecovery_inpaint (red, green, blue, hrp.hlbl);
//
//            rgbSourceModified = true;
//       }
//    }

}

/* Copy original pixel data and
 * subtract dark frame (if present) from current image and apply flat field correction (if present)
 */
void RawImageSource::copyOriginalPixels(const RAWParams &raw, RawImage *src, const RawImage *riDark, RawImage *riFlatFile, array2D<float> &rawData, float &reddeha, float &greendeha, float &bluedeha)
{
    minVals[0] = minVals[1] = minVals[2] = std::numeric_limits<float>::max();
    const auto tmpfilters = ri->get_filters();
    ri->set_filters(ri->prefilters); // we need 4 blacks for bayer processing
    float black[4];
    ri->get_colorsCoeff(nullptr, nullptr, black, false);
    ri->set_filters(tmpfilters);

    if (ri->getSensorType() == ST_BAYER || ri->getSensorType() == ST_FUJI_XTRANS) {
        if (!rawData) {
            rawData(W, H);
        }

        if (riDark && W == riDark->get_width() && H == riDark->get_height()) { // This works also for xtrans-sensors, because black[0] to black[4] are equal for these
            StopWatch Stop1("darkframe subtraction");
#ifdef _OPENMP
            #pragma omp parallel for
#endif

            for (int row = 0; row < H; row++) {
                const int c0 = FC(row, 0);
                const float black0 = black[(c0 == 1 && !(row & 1)) ? 3 : c0];
                const int c1 = FC(row, 1);
                const float black1 = black[(c1 == 1 && !(row & 1)) ? 3 : c1];
                int col;

                for (col = 0; col < W - 1; col += 2) {
                    rawData[row][col] = max(src->data[row][col] + black0 - riDark->data[row][col], 0.0f);
                    rawData[row][col + 1] = max(src->data[row][col + 1] + black1 - riDark->data[row][col + 1], 0.0f);
                }

                if (col < W) {
                    rawData[row][col] = max(src->data[row][col] + black0 - riDark->data[row][col], 0.0f);
                }
            }
        } else {

#ifdef _OPENMP
            #pragma omp parallel for
#endif

            for (int row = 0; row < H; row++) {
                for (int col = 0; col < W; col++) {
                    rawData[row][col] = src->data[row][col];
                }
            }
        }
/*
    Copyright (c) Ingo Weyrich  2020 (heckflosse67@gmx.de)
*/
        
        if (ri->getSensorType() == ST_BAYER) {
            getMinValsBayer(rawData, ri->zeroIsBad());
        } else {
            getMinValsXtrans(rawData);
        }
        //
        reddeha = minVals[0] == std::numeric_limits<float>::max() ? 0.f : minVals[0];
        greendeha = minVals[1] == std::numeric_limits<float>::max() ? 0.f : minVals[1];
        bluedeha = minVals[2] == std::numeric_limits<float>::max() ? 0.f : minVals[2];
        if (riFlatFile && W == riFlatFile->get_width() && H == riFlatFile->get_height()) {
            processFlatField(raw, riFlatFile, rawData, black);
        }  // flatfield
    } else if (ri->get_colors() == 1) {
        // Monochrome
        if (!rawData) {
            rawData(W, H);
        }

        if (riDark && W == riDark->get_width() && H == riDark->get_height()) {
            for (int row = 0; row < H; row++) {
                for (int col = 0; col < W; col++) {
                    rawData[row][col] = max(src->data[row][col] + black[0] - riDark->data[row][col], 0.0f);
                }
            }
        } else {
            for (int row = 0; row < H; row++) {
                for (int col = 0; col < W; col++) {
                    rawData[row][col] = src->data[row][col];
                }
            }
        }

        if (riFlatFile && W == riFlatFile->get_width() && H == riFlatFile->get_height()) {
            processFlatField(raw, riFlatFile, rawData, black);
        }  // flatfield
    } else {
        // No bayer pattern
        // TODO: Is there a flat field correction possible?
        if (!rawData) {
            rawData(3 * W, H);
        }

        if (riDark && W == riDark->get_width() && H == riDark->get_height()) {
            for (int row = 0; row < H; row++) {
                for (int col = 0; col < W; col++) {
                    int c = FC(row, col);
                    int c4 = (c == 1 && !(row & 1)) ? 3 : c;
                    rawData[row][3 * col + 0] = max(src->data[row][3 * col + 0] + black[c4] - riDark->data[row][3 * col + 0], 0.0f);
                    rawData[row][3 * col + 1] = max(src->data[row][3 * col + 1] + black[c4] - riDark->data[row][3 * col + 1], 0.0f);
                    rawData[row][3 * col + 2] = max(src->data[row][3 * col + 2] + black[c4] - riDark->data[row][3 * col + 2], 0.0f);
                }
            }
        } else {
            for (int row = 0; row < H; row++) {
                for (int col = 0; col < W; col++) {
                    rawData[row][3 * col + 0] = src->data[row][3 * col + 0];
                    rawData[row][3 * col + 1] = src->data[row][3 * col + 1];
                    rawData[row][3 * col + 2] = src->data[row][3 * col + 2];
                }
            }
        }
    }
}

// Scale original pixels into the range 0 65535 using black offsets and multipliers
void RawImageSource::scaleColors(int winx, int winy, int winw, int winh, const RAWParams &raw, array2D<float> &rawData)
{
    chmax[0] = chmax[1] = chmax[2] = chmax[3] = 0; //channel maxima
    float black_lev[4] = {0.f};//black level

    //adjust black level  (eg Canon)
    bool isMono = false;

    if (getSensorType() == ST_BAYER || getSensorType() == ST_FOVEON) {

        if (raw.bayersensor.Dehablack) {
            black_lev[0] = minVals[0];
            black_lev[1] = minVals[1];
            black_lev[2] = minVals[2];
            black_lev[3] = minVals[1];
        } else {
            black_lev[0] = raw.bayersensor.black1; //R
            black_lev[1] = raw.bayersensor.black0; //G1
            black_lev[2] = raw.bayersensor.black2; //B
            black_lev[3] = raw.bayersensor.black3; //G2
        }

        isMono = RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::MONO) == raw.bayersensor.method;
    } else if (getSensorType() == ST_FUJI_XTRANS) {

        if (raw.xtranssensor.Dehablackx) {
            black_lev[0] = minVals[0];
            black_lev[1] = minVals[1];
            black_lev[2] = minVals[2];
            black_lev[3] = minVals[1];
        } else {
            black_lev[0] = raw.xtranssensor.blackred; //R
            black_lev[1] = raw.xtranssensor.blackgreen; //G1
            black_lev[2] = raw.xtranssensor.blackblue; //B
            black_lev[3] = raw.xtranssensor.blackgreen; //G2  (set, only used with a Bayer filter)
        }
 
        isMono = RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::MONO) == raw.xtranssensor.method;
    }

    for (int i = 0; i < 4 ; i++) {
        cblacksom[i] = max(c_black[i] + black_lev[i], 0.0f);    // adjust black level
    }

    for (int i = 0; i < 4; ++i) {
        c_white[i] = (ri->get_white(i) - cblacksom[i]) / raw.expos + cblacksom[i];
    }

    initialGain = calculate_scale_mul(scale_mul, ref_pre_mul, c_white, cblacksom, isMono, ri->get_colors()); // recalculate scale colors with adjusted levels

    //fprintf(stderr, "recalc: %f [%f %f %f %f]\n", initialGain, scale_mul[0], scale_mul[1], scale_mul[2], scale_mul[3]);
    for (int i = 0; i < 4 ; i++) {
        clmax[i] = (c_white[i] - cblacksom[i]) * scale_mul[i];    // raw clip level
    }

    // this seems strange, but it works

    // scale image colors

    if (ri->getSensorType() == ST_BAYER) {
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            float tmpchmax[3];
            tmpchmax[0] = tmpchmax[1] = tmpchmax[2] = 0.0f;
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int row = winy; row < winy + winh; row ++)
            {
                for (int col = winx; col < winx + winw; col++) {
                    const int c = FC(row, col);                        // three colors,  0=R, 1=G,  2=B
                    const int c4 = (c == 1 && !(row & 1)) ? 3 : c;    // four  colors,  0=R, 1=G1, 2=B, 3=G2
                    const float val = max(0.f, rawData[row][col] - cblacksom[c4]) * scale_mul[c4];
                    rawData[row][col] = val;
                    tmpchmax[c] = max(tmpchmax[c], val);
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                chmax[0] = max(tmpchmax[0], chmax[0]);
                chmax[1] = max(tmpchmax[1], chmax[1]);
                chmax[2] = max(tmpchmax[2], chmax[2]);
            }
        }
    } else if (ri->get_colors() == 1) {
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            float tmpchmax = 0.0f;
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int row = winy; row < winy + winh; row ++)
            {
                for (int col = winx; col < winx + winw; col++) {
                    const float val = max(0.f, rawData[row][col] - cblacksom[0]) * scale_mul[0];
                    rawData[row][col] = val;
                    tmpchmax = max(tmpchmax, val);
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                chmax[0] = chmax[1] = chmax[2] = chmax[3] = max(tmpchmax, chmax[0]);
            }
        }
    } else if (ri->getSensorType() == ST_FUJI_XTRANS) {
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            float tmpchmax[3];
            tmpchmax[0] = tmpchmax[1] = tmpchmax[2] = 0.0f;
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int row = winy; row < winy + winh; row ++)
            {
                for (int col = winx; col < winx + winw; col++) {
                    const int c = ri->XTRANSFC(row, col);
                    const float val = max(0.f, rawData[row][col] - cblacksom[c]) * scale_mul[c];
                    rawData[row][col] = val;
                    tmpchmax[c] = max(tmpchmax[c], val);
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                chmax[0] = max(tmpchmax[0], chmax[0]);
                chmax[1] = max(tmpchmax[1], chmax[1]);
                chmax[2] = max(tmpchmax[2], chmax[2]);
            }
        }
    } else {
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            float tmpchmax[3];
            tmpchmax[0] = tmpchmax[1] = tmpchmax[2] = 0.0f;
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int row = winy; row < winy + winh; row ++)
            {
                for (int col = winx; col < winx + winw; col++) {
                    for (int c = 0; c < 3; c++) {                 // three colors,  0=R, 1=G,  2=B
                        const float val = max(0.f, rawData[row][3 * col + c] - cblacksom[c]) * scale_mul[c];
                        rawData[row][3 * col + c] = val;
                        tmpchmax[c] = max(tmpchmax[c], val);
                    }
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                chmax[0] = max(tmpchmax[0], chmax[0]);
                chmax[1] = max(tmpchmax[1], chmax[1]);
                chmax[2] = max(tmpchmax[2], chmax[2]);
            }
        }
        chmax[3] = chmax[1];
    }

}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int RawImageSource::defTransform(const RawImage *ri, int tran)
{

    int deg = ri->get_rotateDegree();

    if ((tran & TR_ROT) == TR_R180) {
        deg += 180;
    } else if ((tran & TR_ROT) == TR_R90) {
        deg += 90;
    } else if ((tran & TR_ROT) == TR_R270) {
        deg += 270;
    }

    deg %= 360;

    int ret = 0;

    if (deg == 90) {
        ret |= TR_R90;
    } else if (deg == 180) {
        ret |= TR_R180;
    } else if (deg == 270) {
        ret |= TR_R270;
    }

    if (tran & TR_HFLIP) {
        ret |= TR_HFLIP;
    }

    if (tran & TR_VFLIP) {
        ret |= TR_VFLIP;
    }

    return ret;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// Thread called part
void RawImageSource::processFalseColorCorrectionThread(Imagefloat* im, array2D<float> &rbconv_Y, array2D<float> &rbconv_I, array2D<float> &rbconv_Q, array2D<float> &rbout_I, array2D<float> &rbout_Q, const int row_from, const int row_to)
{

    const int W = im->getWidth();
    constexpr float onebynine = 1.f / 9.f;

#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat buffer[12];
    vfloat* pre1 = &buffer[0];
    vfloat* pre2 = &buffer[3];
    vfloat* post1 = &buffer[6];
    vfloat* post2 = &buffer[9];
#else
    float buffer[12];
    float* pre1 = &buffer[0];
    float* pre2 = &buffer[3];
    float* post1 = &buffer[6];
    float* post2 = &buffer[9];
#endif

    int px = (row_from - 1) % 3, cx = row_from % 3, nx = 0;

    convert_row_to_YIQ(im->r(row_from - 1), im->g(row_from - 1), im->b(row_from - 1), rbconv_Y[px], rbconv_I[px], rbconv_Q[px], W);
    convert_row_to_YIQ(im->r(row_from), im->g(row_from), im->b(row_from), rbconv_Y[cx], rbconv_I[cx], rbconv_Q[cx], W);

    for (int j = 0; j < W; j++) {
        rbout_I[px][j] = rbconv_I[px][j];
        rbout_Q[px][j] = rbconv_Q[px][j];
    }

    for (int i = row_from; i < row_to; i++) {

        px = (i - 1) % 3;
        cx = i % 3;
        nx = (i + 1) % 3;

        convert_row_to_YIQ(im->r(i + 1), im->g(i + 1), im->b(i + 1), rbconv_Y[nx], rbconv_I[nx], rbconv_Q[nx], W);

#if defined(__SSE2__) || defined(RT_SIMDE)
        pre1[0] = _mm_setr_ps(rbconv_I[px][0], rbconv_Q[px][0], 0, 0), pre1[1] = _mm_setr_ps(rbconv_I[cx][0], rbconv_Q[cx][0], 0, 0), pre1[2] = _mm_setr_ps(rbconv_I[nx][0], rbconv_Q[nx][0], 0, 0);
        pre2[0] = _mm_setr_ps(rbconv_I[px][1], rbconv_Q[px][1], 0, 0), pre2[1] = _mm_setr_ps(rbconv_I[cx][1], rbconv_Q[cx][1], 0, 0), pre2[2] = _mm_setr_ps(rbconv_I[nx][1], rbconv_Q[nx][1], 0, 0);

        // fill first element in rbout_I and rbout_Q
        rbout_I[cx][0] = rbconv_I[cx][0];
        rbout_Q[cx][0] = rbconv_Q[cx][0];

        // median I channel
        for (int j = 1; j < W - 2; j += 2) {
            post1[0] = _mm_setr_ps(rbconv_I[px][j + 1], rbconv_Q[px][j + 1], 0, 0), post1[1] = _mm_setr_ps(rbconv_I[cx][j + 1], rbconv_Q[cx][j + 1], 0, 0), post1[2] = _mm_setr_ps(rbconv_I[nx][j + 1], rbconv_Q[nx][j + 1], 0, 0);
            const auto middle = middle4of6(pre2[0], pre2[1], pre2[2], post1[0], post1[1], post1[2]);
            vfloat medianval = median(pre1[0], pre1[1], pre1[2], middle[0], middle[1], middle[2], middle[3]);
            rbout_I[cx][j] = medianval[0];
            rbout_Q[cx][j] = medianval[1];
            post2[0] = _mm_setr_ps(rbconv_I[px][j + 2], rbconv_Q[px][j + 2], 0, 0), post2[1] = _mm_setr_ps(rbconv_I[cx][j + 2], rbconv_Q[cx][j + 2], 0, 0), post2[2] = _mm_setr_ps(rbconv_I[nx][j + 2], rbconv_Q[nx][j + 2], 0, 0);
            medianval = median(post2[0], post2[1], post2[2], middle[0], middle[1], middle[2], middle[3]);
            rbout_I[cx][j + 1] = medianval[0];
            rbout_Q[cx][j + 1] = medianval[1];
            std::swap(pre1, post1);
            std::swap(pre2, post2);
        }

        // fill last elements in rbout_I and rbout_Q
        rbout_I[cx][W - 1] = rbconv_I[cx][W - 1];
        rbout_I[cx][W - 2] = rbconv_I[cx][W - 2];
        rbout_Q[cx][W - 1] = rbconv_Q[cx][W - 1];
        rbout_Q[cx][W - 2] = rbconv_Q[cx][W - 2];

#else
        pre1[0] = rbconv_I[px][0], pre1[1] = rbconv_I[cx][0], pre1[2] = rbconv_I[nx][0];
        pre2[0] = rbconv_I[px][1], pre2[1] = rbconv_I[cx][1], pre2[2] = rbconv_I[nx][1];

        // fill first element in rbout_I
        rbout_I[cx][0] = rbconv_I[cx][0];

        // median I channel
        for (int j = 1; j < W - 2; j += 2) {
            post1[0] = rbconv_I[px][j + 1], post1[1] = rbconv_I[cx][j + 1], post1[2] = rbconv_I[nx][j + 1];
            const auto middle = middle4of6(pre2[0], pre2[1], pre2[2], post1[0], post1[1], post1[2]);
            rbout_I[cx][j] = median(pre1[0], pre1[1], pre1[2], middle[0], middle[1], middle[2], middle[3]);
            post2[0] = rbconv_I[px][j + 2], post2[1] = rbconv_I[cx][j + 2], post2[2] = rbconv_I[nx][j + 2];
            rbout_I[cx][j + 1] = median(post2[0], post2[1], post2[2], middle[0], middle[1], middle[2], middle[3]);
            std::swap(pre1, post1);
            std::swap(pre2, post2);
        }

        // fill last elements in rbout_I
        rbout_I[cx][W - 1] = rbconv_I[cx][W - 1];
        rbout_I[cx][W - 2] = rbconv_I[cx][W - 2];

        pre1[0] = rbconv_Q[px][0], pre1[1] = rbconv_Q[cx][0], pre1[2] = rbconv_Q[nx][0];
        pre2[0] = rbconv_Q[px][1], pre2[1] = rbconv_Q[cx][1], pre2[2] = rbconv_Q[nx][1];

        // fill first element in rbout_Q
        rbout_Q[cx][0] = rbconv_Q[cx][0];

        // median Q channel
        for (int j = 1; j < W - 2; j += 2) {
            post1[0] = rbconv_Q[px][j + 1], post1[1] = rbconv_Q[cx][j + 1], post1[2] = rbconv_Q[nx][j + 1];
            const auto middle = middle4of6(pre2[0], pre2[1], pre2[2], post1[0], post1[1], post1[2]);
            rbout_Q[cx][j] = median(pre1[0], pre1[1], pre1[2], middle[0], middle[1], middle[2], middle[3]);
            post2[0] = rbconv_Q[px][j + 2], post2[1] = rbconv_Q[cx][j + 2], post2[2] = rbconv_Q[nx][j + 2];
            rbout_Q[cx][j + 1] = median(post2[0], post2[1], post2[2], middle[0], middle[1], middle[2], middle[3]);
            std::swap(pre1, post1);
            std::swap(pre2, post2);
        }

        // fill last elements in rbout_Q
        rbout_Q[cx][W - 1] = rbconv_Q[cx][W - 1];
        rbout_Q[cx][W - 2] = rbconv_Q[cx][W - 2];
#endif

        // blur i-1th row
        if (i > row_from) {
            convert_to_RGB(im->r(i - 1, 0), im->g(i - 1, 0), im->b(i - 1, 0), rbconv_Y[px][0], rbout_I[px][0], rbout_Q[px][0]);

#ifdef _OPENMP
            #pragma omp simd
#endif

            for (int j = 1; j < W - 1; j++) {
                float I = (rbout_I[px][j - 1] + rbout_I[px][j] + rbout_I[px][j + 1] + rbout_I[cx][j - 1] + rbout_I[cx][j] + rbout_I[cx][j + 1] + rbout_I[nx][j - 1] + rbout_I[nx][j] + rbout_I[nx][j + 1]) * onebynine;
                float Q = (rbout_Q[px][j - 1] + rbout_Q[px][j] + rbout_Q[px][j + 1] + rbout_Q[cx][j - 1] + rbout_Q[cx][j] + rbout_Q[cx][j + 1] + rbout_Q[nx][j - 1] + rbout_Q[nx][j] + rbout_Q[nx][j + 1]) * onebynine;
                convert_to_RGB(im->r(i - 1, j), im->g(i - 1, j), im->b(i - 1, j), rbconv_Y[px][j], I, Q);
            }

            convert_to_RGB(im->r(i - 1, W - 1), im->g(i - 1, W - 1), im->b(i - 1, W - 1), rbconv_Y[px][W - 1], rbout_I[px][W - 1], rbout_Q[px][W - 1]);
        }
    }

    // blur last 3 row and finalize H-1th row
    convert_to_RGB(im->r(row_to - 1, 0), im->g(row_to - 1, 0), im->b(row_to - 1, 0), rbconv_Y[cx][0], rbout_I[cx][0], rbout_Q[cx][0]);
#ifdef _OPENMP
    #pragma omp simd
#endif

    for (int j = 1; j < W - 1; j++) {
        float I = (rbout_I[px][j - 1] + rbout_I[px][j] + rbout_I[px][j + 1] + rbout_I[cx][j - 1] + rbout_I[cx][j] + rbout_I[cx][j + 1] + rbconv_I[nx][j - 1] + rbconv_I[nx][j] + rbconv_I[nx][j + 1]) * onebynine;
        float Q = (rbout_Q[px][j - 1] + rbout_Q[px][j] + rbout_Q[px][j + 1] + rbout_Q[cx][j - 1] + rbout_Q[cx][j] + rbout_Q[cx][j + 1] + rbconv_Q[nx][j - 1] + rbconv_Q[nx][j] + rbconv_Q[nx][j + 1]) * onebynine;
        convert_to_RGB(im->r(row_to - 1, j), im->g(row_to - 1, j), im->b(row_to - 1, j), rbconv_Y[cx][j], I, Q);
    }

    convert_to_RGB(im->r(row_to - 1, W - 1), im->g(row_to - 1, W - 1), im->b(row_to - 1, W - 1), rbconv_Y[cx][W - 1], rbout_I[cx][W - 1], rbout_Q[cx][W - 1]);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// correction_YIQ_LQ
void RawImageSource::processFalseColorCorrection(Imagefloat* im, const int steps)
{

    if (im->getHeight() < 4 || steps < 1) {
        return;
    }

#ifdef _OPENMP
    #pragma omp parallel
    {
        multi_array2D<float, 5> buffer(W, 3);
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();
        int blk = (im->getHeight() - 2) / nthreads;

        for (int t = 0; t < steps; t++) {

            if (tid < nthreads - 1) {
                processFalseColorCorrectionThread(im, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], 1 + tid * blk, 1 + (tid + 1)*blk);
            } else {
                processFalseColorCorrectionThread(im, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], 1 + tid * blk, im->getHeight() - 1);
            }

            #pragma omp barrier
        }
    }
#else
    multi_array2D<float, 5> buffer(W, 3);

    for (int t = 0; t < steps; t++) {
        processFalseColorCorrectionThread(im, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], 1, im->getHeight() - 1);
    }

#endif
}

// Some camera input profiles need gamma preprocessing
// gamma is applied before the CMS, correct line fac=lineFac*rawPixel+LineSum after the CMS
void RawImageSource::getProfilePreprocParams(cmsHPROFILE in, float& gammaFac, float& lineFac, float& lineSum)
{
    gammaFac = 0;
    lineFac = 1;
    lineSum = 0;

    char copyright[256];
    copyright[0] = 0;

    if (cmsGetProfileInfoASCII(in, cmsInfoCopyright, cmsNoLanguage, cmsNoCountry, copyright, 256) > 0) {
        if (strstr(copyright, "Phase One") != nullptr) {
            gammaFac = 0.55556;    // 1.8
        } else if (strstr(copyright, "Nikon Corporation") != nullptr) {
            gammaFac = 0.5;
            lineFac = -0.4;
            lineSum = 1.35; // determined in reverse by measuring NX an RT developed colorchecker PNGs
        }
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static void
lab2ProphotoRgbD50(float L, float A, float B, float& r, float& g, float& b)
{
    float X;
    float Y;
    float Z;

    {
        // convert from Lab to XYZ
        float x, y, z, fx, fy, fz;

        fy = (L + 16.0f) / 116.0f;
        fx = A / 500.0f + fy;
        fz = fy - B / 200.0f;

        if (fy > 24.0f / 116.0f) {
            y = fy * fy * fy;
        } else {
            y = (fy - 16.0f / 116.0f) / 7.787036979f;
        }

        if (fx > 24.0f / 116.0f) {
            x = fx * fx * fx;
        } else {
            x = (fx - 16.0 / 116.0) / 7.787036979f;
        }

        if (fz > 24.0f / 116.0f) {
            z = fz * fz * fz;
        } else {
            z = (fz - 16.0f / 116.0f) / 7.787036979f;
        }

        //0.9642, 1.0000, 0.8249 D50
        X = x * 0.9642;
        Y = y;
        Z = z * 0.8249;
    }
    r = prophoto_xyz[0][0] * X + prophoto_xyz[0][1] * Y + prophoto_xyz[0][2] * Z;
    g = prophoto_xyz[1][0] * X + prophoto_xyz[1][1] * Y + prophoto_xyz[1][2] * Z;
    b = prophoto_xyz[2][0] * X + prophoto_xyz[2][1] * Y + prophoto_xyz[2][2] * Z;
}

// Converts raw image including ICC input profile to working space - floating point version
void RawImageSource::colorSpaceConversion_(Imagefloat* im, const ColorManagementParams& cmp, const ColorTemp &wb, double pre_mul[3], cmsHPROFILE camprofile, double camMatrix[3][3], cmsHPROFILE in, DCPProfile *dcpProf)
{

//    MyTime t1, t2, t3;
//    t1.set ();
    if (dcpProf != nullptr) {
        // DCP processing
        const DCPProfile::Triple pre_mul_row = {
            pre_mul[0],
            pre_mul[1],
            pre_mul[2]
        };
        const DCPProfile::Matrix cam_matrix = {{
                {camMatrix[0][0], camMatrix[0][1], camMatrix[0][2]},
                {camMatrix[1][0], camMatrix[1][1], camMatrix[1][2]},
                {camMatrix[2][0], camMatrix[2][1], camMatrix[2][2]}
            }
        };
        dcpProf->apply(im, cmp.dcpIlluminant, cmp.workingProfile, wb, pre_mul_row, cam_matrix, cmp.applyHueSatMap);
        return;
    }

    if (in == nullptr) {
        // use default camprofile, supplied by dcraw
        // in this case we avoid using the slllllooooooowwww lcms

        // Calculate matrix for direct conversion raw>working space
        TMatrix work = ICCStore::getInstance()->workingSpaceInverseMatrix(cmp.workingProfile);
        double mat[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                for (int k = 0; k < 3; k++) {
                    mat[i][j] += work[i][k] * camMatrix[k][j];    // rgb_xyz * imatrices.xyz_cam
                }

#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = 0; i < im->getHeight(); i++)
            for (int j = 0; j < im->getWidth(); j++) {

                float newr = mat[0][0] * im->r(i, j) + mat[0][1] * im->g(i, j) + mat[0][2] * im->b(i, j);
                float newg = mat[1][0] * im->r(i, j) + mat[1][1] * im->g(i, j) + mat[1][2] * im->b(i, j);
                float newb = mat[2][0] * im->r(i, j) + mat[2][1] * im->g(i, j) + mat[2][2] * im->b(i, j);

                im->r(i, j) = newr;
                im->g(i, j) = newg;
                im->b(i, j) = newb;

            }
    } else {
        bool working_space_is_prophoto = (cmp.workingProfile == "ProPhoto");

        // use supplied input profile

        /*
          The goal here is to in addition to user-made custom ICC profiles also support profiles
          supplied with other popular raw converters. As curves affect color rendering and
          different raw converters deal with them differently (and few if any is as flexible
          as RawTherapee) we cannot really expect to get the *exact* same color rendering here.
          However we try hard to make the best out of it.

          Third-party input profiles that contain a LUT (usually A2B0 tag) often needs some preprocessing,
          as ICC LUTs are not really designed for dealing with linear camera data. Generally one
          must apply some sort of curve to get efficient use of the LUTs. Unfortunately how you
          should preprocess is not standardized so there are almost as many ways as there are
          software makers, and for each one we have to reverse engineer to find out how it has
          been done. (The ICC files made for RT has linear LUTs)

          ICC profiles which only contain the <r,g,b>XYZ tags (ie only a color matrix) should
          (hopefully) not require any pre-processing.

          Some LUT ICC profiles apply a contrast curve and desaturate highlights (to give a "film-like"
          behavior. These will generally work with RawTherapee, but will not produce good results when
          you enable highlight recovery/reconstruction, as that data is added linearly on top of the
          original range. RawTherapee works best with linear ICC profiles.
        */

        enum camera_icc_type {
            CAMERA_ICC_TYPE_GENERIC, // Generic, no special pre-processing required, RTs own is this way
            CAMERA_ICC_TYPE_PHASE_ONE, // Capture One profiles
            CAMERA_ICC_TYPE_LEAF, // Leaf profiles, former Leaf Capture now in Capture One, made for Leaf digital backs
            CAMERA_ICC_TYPE_NIKON // Nikon NX profiles
        } camera_icc_type = CAMERA_ICC_TYPE_GENERIC;

        float leaf_prophoto_mat[3][3];
        {
            // identify ICC type
            char copyright[256] = "";
            char description[256] = "";

            cmsGetProfileInfoASCII(in, cmsInfoCopyright, cmsNoLanguage, cmsNoCountry, copyright, 256);
            cmsGetProfileInfoASCII(in, cmsInfoDescription, cmsNoLanguage, cmsNoCountry, description, 256);
            camera_icc_type = CAMERA_ICC_TYPE_GENERIC;

            // Note: order the identification with the most detailed matching first since the more general ones may also match the more detailed
            if ((strstr(copyright, "Leaf") != nullptr ||
                    strstr(copyright, "Phase One A/S") != nullptr ||
                    strstr(copyright, "Kodak") != nullptr ||
                    strstr(copyright, "Creo") != nullptr) &&
                    (strstr(description, "LF2 ") == description ||
                     strstr(description, "LF3 ") == description ||
                     strstr(description, "LeafLF2") == description ||
                     strstr(description, "LeafLF3") == description ||
                     strstr(description, "LeafLF4") == description ||
                     strstr(description, "MamiyaLF2") == description ||
                     strstr(description, "MamiyaLF3") == description)) {
                camera_icc_type = CAMERA_ICC_TYPE_LEAF;
            } else if (strstr(copyright, "Phase One A/S") != nullptr) {
                camera_icc_type = CAMERA_ICC_TYPE_PHASE_ONE;
            } else if (strstr(copyright, "Nikon Corporation") != nullptr) {
                camera_icc_type = CAMERA_ICC_TYPE_NIKON;
            }
        }

        // Initialize transform
        cmsHTRANSFORM hTransform;
        cmsHPROFILE prophoto = ICCStore::getInstance()->workingSpace("ProPhoto"); // We always use Prophoto to apply the ICC profile to minimize problems with clipping in LUT conversion.
        bool transform_via_pcs_lab = false;
        bool separate_pcs_lab_highlights = false;

        // check if the working space is fully contained in prophoto
        if (!working_space_is_prophoto && camera_icc_type == CAMERA_ICC_TYPE_GENERIC) {
            TMatrix toxyz = ICCStore::getInstance()->workingSpaceMatrix(cmp.workingProfile);
            TMatrix torgb = ICCStore::getInstance()->workingSpaceInverseMatrix("ProPhoto");
            float rgb[3] = {0.f, 0.f, 0.f};

            for (int i = 0; i < 2 && !working_space_is_prophoto; ++i) {
                rgb[i] = 1.f;
                float x, y, z;

                Color::rgbxyz(rgb[0], rgb[1], rgb[2], x, y, z, toxyz);
                Color::xyz2rgb(x, y, z, rgb[0], rgb[1], rgb[2], torgb);

                for (int j = 0; j < 2; ++j) {
                    if (rgb[j] < 0.f || rgb[j] > 1.f) {
                        working_space_is_prophoto = true;
                        prophoto = ICCStore::getInstance()->workingSpace(cmp.workingProfile);

                        if (settings->verbose) {
                            std::cout << "colorSpaceConversion_: converting directly to " << cmp.workingProfile << " instead of passing through ProPhoto" << std::endl;
                        }

                        break;
                    }

                    rgb[j] = 0.f;
                }
            }
        }

        lcmsMutex->lock();

        switch (camera_icc_type) {
            case CAMERA_ICC_TYPE_PHASE_ONE:
            case CAMERA_ICC_TYPE_LEAF: {
                // These profiles have a RGB to Lab cLUT, gives gamma 1.8 output, and expects a "film-like" curve on input
                transform_via_pcs_lab = true;
                separate_pcs_lab_highlights = true;
                // We transform to Lab because we can and that we avoid getting an unnecessary unmatched gamma conversion which we would need to revert.
                hTransform = cmsCreateTransform(in, TYPE_RGB_FLT, nullptr, TYPE_Lab_FLT, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE);

                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        leaf_prophoto_mat[i][j] = 0;

                        for (int k = 0; k < 3; k++) {
                            leaf_prophoto_mat[i][j] += prophoto_xyz[i][k] * camMatrix[k][j];
                        }
                    }
                }

                break;
            }

            case CAMERA_ICC_TYPE_NIKON:
            case CAMERA_ICC_TYPE_GENERIC:
            default:
                hTransform = cmsCreateTransform(in, TYPE_RGB_FLT, prophoto, TYPE_RGB_FLT, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE);   // NOCACHE is important for thread safety
                break;
        }

        lcmsMutex->unlock();

        if (hTransform == nullptr) {
            // Fallback: create transform from camera profile. Should not happen normally.
            lcmsMutex->lock();
            hTransform = cmsCreateTransform(camprofile, TYPE_RGB_FLT, prophoto, TYPE_RGB_FLT, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE);
            lcmsMutex->unlock();
        }

        TMatrix toxyz = {}, torgb = {};

        if (!working_space_is_prophoto) {
            toxyz = ICCStore::getInstance()->workingSpaceMatrix("ProPhoto");
            torgb = ICCStore::getInstance()->workingSpaceInverseMatrix(cmp.workingProfile);  //sRGB .. Adobe...Wide...
        }

#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            AlignedBuffer<float> buffer(im->getWidth() * 3);
            AlignedBuffer<float> hl_buffer(im->getWidth() * 3);
            AlignedBuffer<float> hl_scale(im->getWidth());
#ifdef _OPENMP
            #pragma omp for schedule(static)
#endif

            for (int h = 0; h < im->getHeight(); ++h) {
                float *p = buffer.data, *pR = im->r(h), *pG = im->g(h), *pB = im->b(h);

                // Apply pre-processing
                for (int w = 0; w < im->getWidth(); ++w) {
                    float r = *(pR++);
                    float g = *(pG++);
                    float b = *(pB++);

                    // convert to 0-1 range as LCMS expects that
                    r /= 65535.0f;
                    g /= 65535.0f;
                    b /= 65535.0f;

                    float maxc = max(r, g, b);

                    if (maxc <= 1.0) {
                        hl_scale.data[w] = 1.0;
                    } else {
                        // highlight recovery extend the range past the clip point, which means we can get values larger than 1.0 here.
                        // LUT ICC profiles only work in the 0-1 range so we scale down to fit and restore after conversion.
                        hl_scale.data[w] = 1.0 / maxc;
                        r *= hl_scale.data[w];
                        g *= hl_scale.data[w];
                        b *= hl_scale.data[w];
                    }

                    switch (camera_icc_type) {
                        case CAMERA_ICC_TYPE_PHASE_ONE:
                            // Here we apply a curve similar to Capture One's "Film Standard" + gamma, the reason is that the LUTs embedded in the
                            // ICCs are designed to work on such input, and if you provide it with a different curve you don't get as good result.
                            // We will revert this curve after we've made the color transform. However when we revert the curve, we'll notice that
                            // highlight rendering suffers due to that the LUT transform don't expand well, therefore we do a less compressed
                            // conversion too and mix them, this gives us the highest quality and most flexible result.
                            hl_buffer.data[3 * w + 0] = pow_F(r, 1.0 / 1.8);
                            hl_buffer.data[3 * w + 1] = pow_F(g, 1.0 / 1.8);
                            hl_buffer.data[3 * w + 2] = pow_F(b, 1.0 / 1.8);
                            r = phaseOneIccCurveInv->getVal(r);
                            g = phaseOneIccCurveInv->getVal(g);
                            b = phaseOneIccCurveInv->getVal(b);
                            break;

                        case CAMERA_ICC_TYPE_LEAF: {
                            // Leaf profiles expect that the camera native RGB has been converted to Prophoto RGB
                            float newr = leaf_prophoto_mat[0][0] * r + leaf_prophoto_mat[0][1] * g + leaf_prophoto_mat[0][2] * b;
                            float newg = leaf_prophoto_mat[1][0] * r + leaf_prophoto_mat[1][1] * g + leaf_prophoto_mat[1][2] * b;
                            float newb = leaf_prophoto_mat[2][0] * r + leaf_prophoto_mat[2][1] * g + leaf_prophoto_mat[2][2] * b;
                            hl_buffer.data[3 * w + 0] = pow_F(newr, 1.0 / 1.8);
                            hl_buffer.data[3 * w + 1] = pow_F(newg, 1.0 / 1.8);
                            hl_buffer.data[3 * w + 2] = pow_F(newb, 1.0 / 1.8);
                            r = phaseOneIccCurveInv->getVal(newr);
                            g = phaseOneIccCurveInv->getVal(newg);
                            b = phaseOneIccCurveInv->getVal(newb);
                            break;
                        }

                        case CAMERA_ICC_TYPE_NIKON:
                            // gamma 0.5
                            r = sqrtf(r);
                            g = sqrtf(g);
                            b = sqrtf(b);
                            break;

                        case CAMERA_ICC_TYPE_GENERIC:
                        default:
                            // do nothing
                            break;
                    }

                    *(p++) = r;
                    *(p++) = g;
                    *(p++) = b;
                }

                // Run icc transform
                cmsDoTransform(hTransform, buffer.data, buffer.data, im->getWidth());

                if (separate_pcs_lab_highlights) {
                    cmsDoTransform(hTransform, hl_buffer.data, hl_buffer.data, im->getWidth());
                }

                // Apply post-processing
                p = buffer.data;
                pR = im->r(h);
                pG = im->g(h);
                pB = im->b(h);

                for (int w = 0; w < im->getWidth(); ++w) {

                    float r, g, b, hr = 0.f, hg = 0.f, hb = 0.f;

                    if (transform_via_pcs_lab) {
                        float L = *(p++);
                        float A = *(p++);
                        float B = *(p++);
                        // profile connection space CIELAB should have D50 illuminant
                        lab2ProphotoRgbD50(L, A, B, r, g, b);

                        if (separate_pcs_lab_highlights) {
                            lab2ProphotoRgbD50(hl_buffer.data[3 * w + 0], hl_buffer.data[3 * w + 1], hl_buffer.data[3 * w + 2], hr, hg, hb);
                        }
                    } else {
                        r = *(p++);
                        g = *(p++);
                        b = *(p++);
                    }

                    // restore pre-processing and/or add post-processing for the various ICC types
                    switch (camera_icc_type) {
                        default:
                            break;

                        case CAMERA_ICC_TYPE_PHASE_ONE:
                        case CAMERA_ICC_TYPE_LEAF: {
                            // note the 1/1.8 gamma, it's the gamma that the profile has applied, which we must revert before we can revert the curve
                            r = phaseOneIccCurve->getVal(pow_F(r, 1.0 / 1.8));
                            g = phaseOneIccCurve->getVal(pow_F(g, 1.0 / 1.8));
                            b = phaseOneIccCurve->getVal(pow_F(b, 1.0 / 1.8));
                            const float mix = 0.25; // may seem a low number, but remember this is linear space, mixing starts 2 stops from clipping
                            const float maxc = max(r, g, b);

                            if (maxc > mix) {
                                float fac = (maxc - mix) / (1.0 - mix);
                                fac = sqrtf(sqrtf(fac)); // gamma 0.25 to mix in highlight render relatively quick
                                r = (1.0 - fac) * r + fac * hr;
                                g = (1.0 - fac) * g + fac * hg;
                                b = (1.0 - fac) * b + fac * hb;
                            }

                            break;
                        }

                        case CAMERA_ICC_TYPE_NIKON: {
                            const float lineFac = -0.4;
                            const float lineSum = 1.35;
                            r *= r * lineFac + lineSum;
                            g *= g * lineFac + lineSum;
                            b *= b * lineFac + lineSum;
                            break;
                        }
                    }

                    // restore highlight scaling if any
                    if (hl_scale.data[w] != 1.0) {
                        float fac = 1.0 / hl_scale.data[w];
                        r *= fac;
                        g *= fac;
                        b *= fac;
                    }

                    // If we don't have ProPhoto as chosen working profile, convert. This conversion is clipless, ie if we convert
                    // to a small space such as sRGB we may end up with negative values and values larger than max.
                    if (!working_space_is_prophoto) {
                        //convert from Prophoto to XYZ
                        float x = (toxyz[0][0] * r + toxyz[0][1] * g + toxyz[0][2] * b) ;
                        float y = (toxyz[1][0] * r + toxyz[1][1] * g + toxyz[1][2] * b) ;
                        float z = (toxyz[2][0] * r + toxyz[2][1] * g + toxyz[2][2] * b) ;
                        //convert from XYZ to cmp.working  (sRGB...Adobe...Wide..)
                        r = ((torgb[0][0] * x + torgb[0][1] * y + torgb[0][2] * z)) ;
                        g = ((torgb[1][0] * x + torgb[1][1] * y + torgb[1][2] * z)) ;
                        b = ((torgb[2][0] * x + torgb[2][1] * y + torgb[2][2] * z)) ;
                    }

                    // return to the 0.0 - 65535.0 range (with possible negative and > max values present)
                    r *= 65535.0;
                    g *= 65535.0;
                    b *= 65535.0;

                    *(pR++) = r;
                    *(pG++) = g;
                    *(pB++) = b;
                }
            }
        } // End of parallelization
        cmsDeleteTransform(hTransform);
    }

//t3.set ();
//        printf ("ICM TIME: %d usec\n", t3.etime(t1));
}


// Determine RAW input and output profiles. Returns TRUE on success
bool RawImageSource::findInputProfile(Glib::ustring inProfile, cmsHPROFILE embedded, std::string camName, const Glib::ustring &fileName, DCPProfile **dcpProf, cmsHPROFILE& in)
{
    in = nullptr; // cam will be taken on NULL
    *dcpProf = nullptr;

    if (inProfile == "(none)") {
        return false;
    }

    if (inProfile == "(embedded)") {
        if (embedded) {
            in = embedded;
        } else {
            *dcpProf = DCPStore::getInstance()->getProfile(fileName);
        }
    } else if (inProfile == "(cameraICC)") {
        // DCPs have higher quality, so use them first
        *dcpProf = DCPStore::getInstance()->getStdProfile(camName);

        if (*dcpProf == nullptr) {
            in = ICCStore::getInstance()->getStdProfile(camName);
        }
    } else if (inProfile != "(camera)" && !inProfile.empty()) {
        Glib::ustring normalName = inProfile;

        if (!inProfile.compare(0, 5, "file:")) {
            normalName = inProfile.substr(5);
        }

        if (DCPStore::getInstance()->isValidDCPFileName(normalName)) {
            *dcpProf = DCPStore::getInstance()->getProfile(normalName);
        }

        if (*dcpProf == nullptr) {
            in = ICCStore::getInstance()->getProfile(inProfile);
        }
    }

    // "in" might be NULL because of "not found". That's ok, we take the cam profile then

    return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// derived from Dcraw "blend_highlights()"
//  very effective to reduce (or remove) the magenta, but with levels of grey !
void RawImageSource::HLRecovery_blend(float* rin, float* gin, float* bin, int width, float maxval, float* hlmax)
{

    constexpr int ColorCount = 3;

    // Transform matrixes rgb>lab and back
    constexpr float trans[ColorCount][ColorCount] = { { 1, 1, 1 }, { 1.7320508, -1.7320508, 0 }, { -1, -1, 2 } };
    constexpr float itrans[ColorCount][ColorCount] = { { 1, 0.8660254, -0.5 }, { 1, -0.8660254, -0.5 }, { 1, 0, 1 } };

    float minpt = rtengine::min(hlmax[0], hlmax[1], hlmax[2]); //min of the raw clip points
    //float maxpt=max(hlmax[0],hlmax[1],hlmax[2]);//max of the raw clip points
    //float medpt=hlmax[0]+hlmax[1]+hlmax[2]-minpt-maxpt;//median of the raw clip points
    float maxave = (hlmax[0] + hlmax[1] + hlmax[2]) / 3; //ave of the raw clip points
    //some thresholds:
    const float clipthresh = 0.95;
    const float fixthresh = 0.5;
    const float satthresh = 0.5;

    float clip[3];

    for (int c = 0; c < ColorCount; ++c) {
        clip[c] = rtengine::min(maxave, hlmax[c]);
    }

    // Determine the maximum level (clip) of all channels
    const float clippt = clipthresh * maxval;
    const float fixpt = fixthresh * minpt;
    const float desatpt = satthresh * maxave + (1 - satthresh) * maxval;

    for (int col = 0; col < width; col++) {
        float rgb[ColorCount], cam[2][ColorCount], lab[2][ColorCount], sum[2], chratio, lratio = 0;
        float L, C, H;

        // Copy input pixel to rgb so it's easier to access in loops
        rgb[0] = rin[col];
        rgb[1] = gin[col];
        rgb[2] = bin[col];

        // If no channel is clipped, do nothing on pixel
        int cc;

        for (cc = 0; cc < ColorCount; ++cc) {
            if (rgb[cc] > clippt) {
                break;
            }
        }

        if (cc == ColorCount) {
            continue;
        }

        // Initialize cam with raw input [0] and potentially clipped input [1]
        for (int c = 0; c < ColorCount; ++c) {
            lratio += min(rgb[c], clip[c]);
            cam[0][c] = rgb[c];
            cam[1][c] = min(cam[0][c], maxval);
        }

        // Calculate the lightness correction ratio (chratio)
        for (int i = 0; i < 2; i++) {
            for (int c = 0; c < ColorCount; ++c) {
                lab[i][c] = 0;

                for (int j = 0; j < ColorCount; j++) {
                    lab[i][c] += trans[c][j] * cam[i][j];
                }
            }

            sum[i] = 0;

            for (int c = 1; c < ColorCount; c++) {
                sum[i] += SQR(lab[i][c]);
            }
        }

        chratio = std::sqrt(sum[1] / sum[0]);

        // Apply ratio to lightness in LCH space
        for (int c = 1; c < ColorCount; c++) {
            lab[0][c] *= chratio;
        }

        // Transform back from LCH to RGB
        for (int c = 0; c < ColorCount; ++c) {
            cam[0][c] = 0;

            for (int j = 0; j < ColorCount; j++) {
                cam[0][c] += itrans[c][j] * lab[0][j];
            }
        }

        for (int c = 0; c < ColorCount; ++c) {
            rgb[c] = cam[0][c] / ColorCount;
        }

        // Copy converted pixel back
        if (rin[col] > fixpt) {
            float rfrac = SQR((min(clip[0], rin[col]) - fixpt) / (clip[0] - fixpt));
            rin[col] = min(maxave, rfrac * rgb[0] + (1 - rfrac) * rin[col]);
        }

        if (gin[col] > fixpt) {
            float gfrac = SQR((min(clip[1], gin[col]) - fixpt) / (clip[1] - fixpt));
            gin[col] = min(maxave, gfrac * rgb[1] + (1 - gfrac) * gin[col]);
        }

        if (bin[col] > fixpt) {
            float bfrac = SQR((min(clip[2], bin[col]) - fixpt) / (clip[2] - fixpt));
            bin[col] = min(maxave, bfrac * rgb[2] + (1 - bfrac) * bin[col]);
        }

        lratio /= (rin[col] + gin[col] + bin[col]);
        L = (rin[col] + gin[col] + bin[col]) / 3;
        C = lratio * 1.732050808 * (rin[col] - gin[col]);
        H = lratio * (2 * bin[col] - rin[col] - gin[col]);
        rin[col] = L - H / 6.0 + C / 3.464101615;
        gin[col] = L - H / 6.0 - C / 3.464101615;
        bin[col] = L + H / 3.0;

        if ((L = (rin[col] + gin[col] + bin[col]) / 3) > desatpt) {
            float Lfrac = max(0.0f, (maxave - L) / (maxave - desatpt));
            C = Lfrac * 1.732050808 * (rin[col] - gin[col]);
            H = Lfrac * (2 * bin[col] - rin[col] - gin[col]);
            rin[col] = L - H / 6.0 + C / 3.464101615;
            gin[col] = L - H / 6.0 - C / 3.464101615;
            bin[col] = L + H / 3.0;
        }
    }
}

void RawImageSource::HLRecovery_Luminance(float* rin, float* gin, float* bin, float* rout, float* gout, float* bout, int width, float maxval)
{

    for (int i = 0; i < width; i++) {
        float r = rin[i], g = gin[i], b = bin[i];

        if (r > maxval || g > maxval || b > maxval) {
            float ro = min(r, maxval);
            float go = min(g, maxval);
            float bo = min(b, maxval);
            double L = r + g + b;
            double C = 1.732050808 * (r - g);
            double H = 2 * b - r - g;
            double Co = 1.732050808 * (ro - go);
            double Ho = 2 * bo - ro - go;

            if (r != g && g != b) {
                double ratio = std::sqrt((Co * Co + Ho * Ho) / (C * C + H * H));
                C *= ratio;
                H *= ratio;
            }

            float rr = L / 3.0 - H / 6.0 + C / 3.464101615;
            float gr = L / 3.0 - H / 6.0 - C / 3.464101615;
            float br = L / 3.0 + H / 3.0;
            rout[i] = rr;
            gout[i] = gr;
            bout[i] = br;
        } else {
            rout[i] = rin[i];
            gout[i] = gin[i];
            bout[i] = bin[i];
        }
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::HLRecovery_CIELab(float* rin, float* gin, float* bin, float* rout, float* gout, float* bout,
                                       int width, float maxval, double xyz_cam[3][3], double cam_xyz[3][3])
{

    //static bool crTableReady = false;

    // lookup table for Lab conversion
    // perhaps should be centralized, universally defined so we don't keep remaking it???
    /*for (int ix=0; ix < 0x10000; ix++) {
            float rx = ix / 65535.0;
            fv[ix] = rx > 0.008856 ? exp(1.0/3 * log(rx)) : 7.787*rx + 16/116.0;
        }*/
    //crTableReady = true;


    for (int i = 0; i < width; i++) {
        float r = rin[i], g = gin[i], b = bin[i];

        if (r > maxval || g > maxval || b > maxval) {
            float ro = min(r, maxval);
            float go = min(g, maxval);
            float bo = min(b, maxval);
            float yy = xyz_cam[1][0] * r + xyz_cam[1][1] * g + xyz_cam[1][2] * b;
            float fy = (yy < 65535.0 ? Color::cachef[yy] / 327.68 : std::cbrt(yy / MAXVALD));
            // compute LCH decomposition of the clipped pixel (only color information, thus C and H will be used)
            float x = xyz_cam[0][0] * ro + xyz_cam[0][1] * go + xyz_cam[0][2] * bo;
            float y = xyz_cam[1][0] * ro + xyz_cam[1][1] * go + xyz_cam[1][2] * bo;
            float z = xyz_cam[2][0] * ro + xyz_cam[2][1] * go + xyz_cam[2][2] * bo;
            x = (x < 65535.0 ? Color::cachef[x] / 327.68 : std::cbrt(x / MAXVALD));
            y = (y < 65535.0 ? Color::cachef[y] / 327.68 : std::cbrt(y / MAXVALD));
            z = (z < 65535.0 ? Color::cachef[z] / 327.68 : std::cbrt(z / MAXVALD));
            // convert back to rgb
            double fz = fy - y + z;
            double fx = fy + x - y;

            double zr = Color::f2xyz(fz);
            double xr = Color::f2xyz(fx);

            x = xr * 65535.0 ;
            y = yy;
            z = zr * 65535.0 ;
            float rr = cam_xyz[0][0] * x + cam_xyz[0][1] * y + cam_xyz[0][2] * z;
            float gr = cam_xyz[1][0] * x + cam_xyz[1][1] * y + cam_xyz[1][2] * z;
            float br = cam_xyz[2][0] * x + cam_xyz[2][1] * y + cam_xyz[2][2] * z;
            rout[i] = (rr);
            gout[i] = (gr);
            bout[i] = (br);
        } else {
            rout[i] = (rin[i]);
            gout[i] = (gin[i]);
            bout[i] = (bin[i]);
        }
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::hlRecovery(const std::string &method, float* red, float* green, float* blue, int width, float* hlmax)
{
//  BENCHFUN

    if (method == "Luminance") {
        HLRecovery_Luminance(red, green, blue, red, green, blue, width, 65535.0);
    } else if (method == "CIELab blending") {
        HLRecovery_CIELab(red, green, blue, red, green, blue, width, 65535.0, imatrices.xyz_cam, imatrices.cam_xyz);
    } else if (method == "Blend") { // derived from Dcraw
        HLRecovery_blend(red, green, blue, width, 65535.0, hlmax);
    }

}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::getAutoExpHistogram(LUTu & histogram, int& histcompr)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

//    BENCHFUN
    histcompr = 3;

    histogram(65536 >> histcompr);
    histogram.clear();
    const float refwb[3] = {static_cast<float>(refwb_red  / (1 << histcompr)), static_cast<float>(refwb_green / (1 << histcompr)), static_cast<float>(refwb_blue / (1 << histcompr))};

#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        LUTu tmphistogram(histogram.getSize());
        tmphistogram.clear();
#ifdef _OPENMP
        #pragma omp for schedule(dynamic,16) nowait
#endif

        for (int i = border; i < H - border; i++) {
            int start, end;
            getRowStartEnd(i, start, end);

            if (ri->getSensorType() == ST_BAYER) {
                // precalculate factors to avoid expensive per pixel calculations
                float refwb0 = refwb[ri->FC(i, start)];
                float refwb1 = refwb[ri->FC(i, start + 1)];
                int j;

                for (j = start; j < end - 1; j += 2) {
                    tmphistogram[(int)(refwb0 * rawData[i][j])] += 4;
                    tmphistogram[(int)(refwb1 * rawData[i][j + 1])] += 4;
                }

                if (j < end) {
                    tmphistogram[(int)(refwb0 * rawData[i][j])] += 4;
                }
            } else if (ri->getSensorType() == ST_FUJI_XTRANS) {
                // precalculate factors to avoid expensive per pixel calculations
                float refwb0 = refwb[ri->XTRANSFC(i, start)];
                float refwb1 = refwb[ri->XTRANSFC(i, start + 1)];
                float refwb2 = refwb[ri->XTRANSFC(i, start + 2)];
                float refwb3 = refwb[ri->XTRANSFC(i, start + 3)];
                float refwb4 = refwb[ri->XTRANSFC(i, start + 4)];
                float refwb5 = refwb[ri->XTRANSFC(i, start + 5)];
                int j;

                for (j = start; j < end - 5; j += 6) {
                    tmphistogram[(int)(refwb0 * rawData[i][j])] += 4;
                    tmphistogram[(int)(refwb1 * rawData[i][j + 1])] += 4;
                    tmphistogram[(int)(refwb2 * rawData[i][j + 2])] += 4;
                    tmphistogram[(int)(refwb3 * rawData[i][j + 3])] += 4;
                    tmphistogram[(int)(refwb4 * rawData[i][j + 4])] += 4;
                    tmphistogram[(int)(refwb5 * rawData[i][j + 5])] += 4;
                }

                for (; j < end; j++) {
                    tmphistogram[(int)(refwb[ri->XTRANSFC(i, j)] * rawData[i][j])] += 4;
                }
            } else if (ri->get_colors() == 1) {
                for (int j = start; j < end; j++) {
                    tmphistogram[(int)(refwb[0] * rawData[i][j])]++;
                }
            } else {
                for (int j = start; j < end; j++) {
                    tmphistogram[(int)(refwb[0] * rawData[i][3 * j + 0])]++;
                    tmphistogram[(int)(refwb[1] * rawData[i][3 * j + 1])]++;
                    tmphistogram[(int)(refwb[2] * rawData[i][3 * j + 2])]++;
                }
            }
        }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            histogram += tmphistogram;
        }
    }
}

// Histogram MUST be 256 in size; gamma is applied, blackpoint and gain also
void RawImageSource::getRAWHistogram(LUTu & histRedRaw, LUTu & histGreenRaw, LUTu & histBlueRaw)
{
//    BENCHFUN
    histRedRaw.clear();
    histGreenRaw.clear();
    histBlueRaw.clear();

    const float maxWhite = rtengine::max(c_white[0], c_white[1], c_white[2], c_white[3]);
    const float scale = maxWhite <= 1.f ? 65535.f : 1.f; // special case for float raw images in [0.0;1.0] range
    const float multScale = maxWhite <= 1.f ? 1.f / 255.f : 255.f;
    const float mult[4] = { multScale / (c_white[0] - cblacksom[0]),
                            multScale / (c_white[1] - cblacksom[1]),
                            multScale / (c_white[2] - cblacksom[2]),
                            multScale / (c_white[3] - cblacksom[3])
                          };

    const bool fourColours = ri->getSensorType() == ST_BAYER && ((mult[1] != mult[3] || cblacksom[1] != cblacksom[3]) || FC(0, 0) == 3 || FC(0, 1) == 3 || FC(1, 0) == 3 || FC(1, 1) == 3);

    constexpr int histoSize = 65536;
    LUTu hist[4];
    hist[0](histoSize);
    hist[0].clear();

    if (ri->get_colors() > 1) {
        hist[1](histoSize);
        hist[1].clear();
        hist[2](histoSize);
        hist[2].clear();
    }

    if (fourColours) {
        hist[3](histoSize);
        hist[3].clear();
    }

#ifdef _OPENMP
    int numThreads;
    // reduce the number of threads under certain conditions to avoid overhead of too many critical regions
    numThreads = std::sqrt((((H - 2 * border) * (W - 2 * border)) / 262144.f));
    numThreads = std::min(std::max(numThreads, 1), omp_get_max_threads());

    #pragma omp parallel num_threads(numThreads)
#endif
    {
        // we need one LUT per color and thread, which corresponds to 1 MB per thread
        LUTu tmphist[4];
        tmphist[0](histoSize);
        tmphist[0].clear();

        if (ri->get_colors() > 1) {
            tmphist[1](histoSize);
            tmphist[1].clear();
            tmphist[2](histoSize);
            tmphist[2].clear();

            if (fourColours) {
                tmphist[3](histoSize);
                tmphist[3].clear();
            }
        }

#ifdef _OPENMP
        #pragma omp for nowait
#endif

        for (int i = border; i < H - border; i++) {
            int start, end;
            getRowStartEnd(i, start, end);

            if (ri->getSensorType() == ST_BAYER) {
                int j;
                int c1 = FC(i, start);
                c1 = (fourColours && c1 == 1 && !(i & 1)) ? 3 : c1;
                int c2 = FC(i, start + 1);
                c2 = (fourColours && c2 == 1 && !(i & 1)) ? 3 : c2;

                for (j = start; j < end - 1; j += 2) {
                    tmphist[c1][(int)(ri->data[i][j] * scale)]++;
                    tmphist[c2][(int)(ri->data[i][j + 1] * scale)]++;
                }

                if (j < end) { // last pixel of row if width is odd
                    tmphist[c1][(int)(ri->data[i][j] * scale)]++;
                }
            } else if (ri->get_colors() == 1) {
                for (int j = start; j < end; j++) {
                    tmphist[0][(int)(ri->data[i][j] * scale)]++;
                }
            } else if (ri->getSensorType() == ST_FUJI_XTRANS) {
                for (int j = start; j < end - 1; j += 2) {
                    int c = ri->XTRANSFC(i, j);
                    tmphist[c][(int)(ri->data[i][j] * scale)]++;
                }
            } else {
                for (int j = start; j < end; j++) {
                    for (int c = 0; c < 3; c++) {
                        tmphist[c][(int)(ri->data[i][3 * j + c] * scale)]++;
                    }
                }
            }
        }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            hist[0] += tmphist[0];

            if (ri->get_colors() > 1) {
                hist[1] += tmphist[1];
                hist[2] += tmphist[2];

                if (fourColours) {
                    hist[3] += tmphist[3];
                }
            }
        } // end of critical region
    } // end of parallel region

    const auto getidx =
    [&](int c, int i) -> int {
        float f = mult[c] * std::max(0.f, i - cblacksom[c]);
        return f > 0.f ? (f < 1.f ? 1 : std::min(int(f), 255)) : 0;
    };

    for (int i = 0; i < histoSize; i++) {
        int idx = getidx(0, i);
        histRedRaw[idx] += hist[0][i];

        if (ri->get_colors() > 1) {
            idx = getidx(1, i);
            histGreenRaw[idx] += hist[1][i];

            if (fourColours) {
                idx = getidx(3, i);
                histGreenRaw[idx] += hist[3][i];
            }

            idx = getidx(2, i);
            histBlueRaw[idx] += hist[2][i];
        }
    }

    if (ri->getSensorType() == ST_BAYER)    // since there are twice as many greens, correct for it
        for (int i = 0; i < 256; i++) {
            histGreenRaw[i] >>= 1;
        } else if (ri->getSensorType() == ST_FUJI_XTRANS) // since Xtrans has 2.5 as many greens, correct for it
        for (int i = 0; i < 256; i++) {
            histGreenRaw[i] = (histGreenRaw[i] * 2) / 5;
        } else if (ri->get_colors() == 1) { // monochrome sensor => set all histograms equal
        histGreenRaw += histRedRaw;
        histBlueRaw += histRedRaw;
    }

}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::getRowStartEnd(int x, int &start, int &end)
{
    if (fuji) {
        int fw = ri->get_FujiWidth();
        start = std::abs(fw - x) + border;
        end = min(H + W - fw - x, fw + x) - border;
    } else {
        start = border;
        end = W - border;
    }
}


static void histoxyY_low(int bfhitc, int bfwitc, const array2D<float> & xc, const array2D<float> & yc, const array2D<float> & Yc, LUTf &xxx, LUTf &yyy, LUTf &YYY, LUTu &histxy)
{
    //calculate histogram x y in a range of 190 colors
    //this "choice" are guided by generally colors who are in nature skin, sky, etc. in those cases "steps" are small
    // of course we can change to be more precise
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        LUTu histxythr(histxy.getSize());
        histxythr.clear();
        LUTf xxxthr(xxx.getSize());
        xxxthr.clear();
        LUTf yyythr(yyy.getSize());
        yyythr.clear();
        LUTf YYYthr(YYY.getSize());
        YYYthr.clear();
#ifdef _OPENMP
        #pragma omp for schedule(dynamic, 4) nowait
#endif

        for (int y = 0; y < bfhitc ; y++)
        {
            for (int x = 0; x < bfwitc ; x++) {
                int nh = -1;

                if (xc[y][x] < 0.12f && xc[y][x] > 0.03f && yc[y][x] > 0.1f) { // near Prophoto
                    if (yc[y][x] < 0.2f) {
                        nh = 0;
                        //blue hard
                    } else if (yc[y][x] < 0.3f) {
                        nh = 1;
                        //blue
                    } else if (yc[y][x] < 0.4f) {
                        nh = 2;

                    } else if (yc[y][x] < 0.5f) {
                        //blue green
                        nh = 3;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 4;
                    } else if (yc[y][x] < 0.82f) {
                        //green
                        nh = 5;
                    }
                } else if (xc[y][x] < 0.24f && yc[y][x] > 0.05f) {
                    if (yc[y][x] < 0.2f) {
                        nh = 6;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 7;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 8;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 9;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 10;
                    } else if (yc[y][x] < 0.75f) {
                        nh = 11;
                    }
                } else if (xc[y][x] < 0.28f && yc[y][x] > 0.1f) {//blue sky and other
                    if (yc[y][x] < 0.2f) {
                        nh = 12;
                    } else if (yc[y][x] < 0.25f) {
                        nh = 13;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 14;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 15;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 16;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 17;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 18;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 19;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 20;
                    } else if (yc[y][x] < 0.75f) {
                        nh = 21;
                    }
                } else if (xc[y][x] < 0.31f && yc[y][x] > 0.1f) {//near neutral others
                    if (yc[y][x] < 0.2f) {
                        nh = 22;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 23;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 24;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 25;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 26;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 27;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 28;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 29;
                    }
                } else if (xc[y][x] < 0.325f && yc[y][x] > 0.1f) {//neutral  34
                    if (yc[y][x] < 0.2f) {
                        nh = 30;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 31;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 32;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 33;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 34;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 35;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 36;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 37;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 38;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 39;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 40;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 41;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 42;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 43;
                    }
                } else if (xc[y][x] < 0.335f && yc[y][x] > 0.1f) {//neutral
                    if (yc[y][x] < 0.2f) {
                        nh = 44;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 45;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 46;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 47;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 48;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 49;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 50;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 51;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 52;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 53;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 54;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 55;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 56;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 57;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 58;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 59;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 60;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 61;
                    }
                } else if (xc[y][x] < 0.340f && yc[y][x] > 0.1f) {//neutral
                    if (yc[y][x] < 0.2f) {
                        nh = 62;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 63;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 64;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 65;
                    } else if (yc[y][x] < 0.325f) {
                        nh = 66;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 67;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 68;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 69;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 70;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 71;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 72;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 73;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 74;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 75;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 76;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 77;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 78;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 79;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 80;
                    }
                } else if (xc[y][x] < 0.345f && yc[y][x] > 0.1f) {//neutral  37
                    if (yc[y][x] < 0.2f) {
                        nh = 81;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 82;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 83;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 84;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 85;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 86;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 87;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 88;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 89;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 90;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 91;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 92;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 93;
                    } else if (yc[y][x] < 0.39f) {
                        nh = 94;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 95;
                    } else if (yc[y][x] < 0.42f) {
                        nh = 96;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 97;
                    } else if (yc[y][x] < 0.48f) {
                        nh = 98;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 99;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 100;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 101;
                    }
                } else if (xc[y][x] < 0.355f && yc[y][x] > 0.1f) {//neutral  37
                    if (yc[y][x] < 0.2f) {
                        nh = 102;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 103;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 104;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 105;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 106;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 107;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 108;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 109;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 110;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 111;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 112;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 113;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 114;
                    } else if (yc[y][x] < 0.39f) {
                        nh = 115;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 116;
                    } else if (yc[y][x] < 0.42f) {
                        nh = 117;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 118;
                    } else if (yc[y][x] < 0.48f) {
                        nh = 119;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 120;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 121;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 122;
                    }
                } else if (xc[y][x] < 0.365f && yc[y][x] > 0.15f) {  //0.4
                    if (yc[y][x] < 0.2f) {
                        nh = 123;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 124;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 125;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 126;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 127;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 128;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 129;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 130;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 131;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 132;
                    } else if (yc[y][x] < 0.39f) {
                        nh = 133;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 134;
                    } else if (yc[y][x] < 0.42f) {
                        nh = 135;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 136;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 137;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 138;
                    } else if (yc[y][x] < 0.63f) {
                        nh = 139;
                    }
                } else if (xc[y][x] < 0.405f && yc[y][x] > 0.15f) {//45
                    if (yc[y][x] < 0.2f) {
                        nh = 140;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 141;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 142;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 143;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 144;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 145;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 146;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 147;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 148;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 149;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 150;
                    }
                } else if (xc[y][x] < 0.445f && yc[y][x] > 0.15f) {//45
                    if (yc[y][x] < 0.2f) {
                        nh = 151;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 152;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 153;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 154;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 155;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 156;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 157;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 158;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 159;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 160;
                    } else if (yc[y][x] < 0.58f) {
                        nh = 161;
                    }
                } else if (xc[y][x] < 0.495f && yc[y][x] > 0.15f) {
                    if (yc[y][x] < 0.2f) {
                        nh = 162;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 163;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 164;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 165;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 166;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 167;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 168;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 169;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 170;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 171;
                    }
                } else if (xc[y][x] < 0.545f && yc[y][x] > 0.15f) {
                    if (yc[y][x] < 0.2f) {
                        nh = 172;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 173;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 174;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 175;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 176;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 177;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 178;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 179;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 180;
                    }
                } else if (xc[y][x] < 0.595f && yc[y][x] > 0.15f) {
                    if (yc[y][x] < 0.22f) {
                        nh = 181;
                    } else if (yc[y][x] < 0.25f) {
                        nh = 182;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 183;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 184;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 185;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 186;
                    }
                } else if (xc[y][x] < 0.65f && yc[y][x] > 0.12f) {
                    if (yc[y][x] < 0.25f) {
                        nh = 187;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 188;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 189;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 190;
                    }
                } else if (xc[y][x] < 0.75f && yc[y][x] > 0.1f) {
                    nh = 191;
                }

                if (nh >= 0) {
                    histxythr[nh]++;
                    xxxthr[nh] += xc[y][x];
                    yyythr[nh] += yc[y][x];
                    YYYthr[nh] += Yc[y][x];
                }
            }
        }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            histxy += histxythr;
            xxx += xxxthr;
            yyy += yyythr;
            YYY += YYYthr;
        }
    }
}


//enable display cells

//int cellxy[80][90] ;

static void histoxyY(int bfhitc, int bfwitc, const array2D<float> & xc, const array2D<float> & yc, const array2D<float> & Yc, LUTf &xxx, LUTf &yyy, LUTf &YYY, LUTu &histxy, bool purpe)
{
    // calculate histogram x y in a range of 236 colors
    // this "choice" are guided by generally colors who are in nature skin, sky, etc. in those cases "steps" are small
    // of course we can change to be more precise
    // purp enable or not purple color in xyY - approximation...
//enable display cells
//    int totalpixels = 0;

#ifdef _OPENMP
    #pragma omp parallel  // disabled if enable display cells
#endif
    {
        LUTu histxythr(histxy.getSize());
        histxythr.clear();
        LUTf xxxthr(xxx.getSize());
        xxxthr.clear();
        LUTf yyythr(yyy.getSize());
        yyythr.clear();
        LUTf YYYthr(YYY.getSize());
        YYYthr.clear();
        bool purp = true;
        float Ypurp = 0.5f;
        float Ypurpmax = 1.f;
        //enable display cells
        /*
                // clear
                for (int i = 0; i < 80; ++i) {
                    for (int j = 0 ; j < 90; ++j) {
                        cellxy[i][j] = 0;
                    }
                }
        */
#ifdef _OPENMP
        #pragma omp for schedule(dynamic, 4) nowait //disable if enable display cells

#endif

        for (int y = 0; y < bfhitc ; y++)
        {
            for (int x = 0; x < bfwitc ; x++) {
                int nh = -1;

                if (!purpe) {
                    purp = (Yc[y][x] < Ypurp);//cut values with Y > Ypurp
                } else {
                    purp = (Yc[y][x] < Ypurpmax);//
                }

                if (xc[y][x] < 0.12f && xc[y][x] > 0.03f && yc[y][x] > 0.1f) { // near Prophoto
                    if (yc[y][x] < 0.2f) {
                        nh = 0;
                        //blue hard
                    } else if (yc[y][x] < 0.25f) {
                        nh = 1;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 2;
                        //blue
                    } else if (yc[y][x] < 0.35f) {
                        nh = 3;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 4;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 5;
                    } else if (yc[y][x] < 0.5f) {
                        //blue green
                        nh = 6;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 7;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 8;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 9;
                    } else if (yc[y][x] < 0.82f) {
                        //green
                        nh = 10;
                    }
                } else if (xc[y][x] < 0.24f && yc[y][x] > 0.05f) {
                    if (yc[y][x] < 0.2f) {
                        nh = 11;
                    } else if (yc[y][x] < 0.25f) {
                        nh = 12;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 13;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 14;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 15;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 16;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 17;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 18;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 19;
                    } else if (yc[y][x] < 0.67f) {
                        nh = 20;
                    } else if (yc[y][x] < 0.75f) {
                        nh = 21;
                    }
                } else if (xc[y][x] < 0.28f && yc[y][x] > 0.1f) {//blue sky and other
                    if (yc[y][x] < 0.2f) {
                        nh = 22;
                    } else if (yc[y][x] < 0.23f) {
                        nh = 23;
                    } else if (yc[y][x] < 0.25f) {
                        nh = 24;
                    } else if (yc[y][x] < 0.27f) {
                        nh = 25;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 26;
                    } else if (yc[y][x] < 0.31f) {
                        nh = 27;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 28;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 29;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 30;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 31;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 32;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 33;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 34;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 35;
                    } else if (yc[y][x] < 0.67f) {
                        nh = 36;
                    } else if (yc[y][x] < 0.75f) {
                        nh = 37;
                    }
                } else if (xc[y][x] < 0.31f && yc[y][x] > 0.1f) {//near neutral others
                    if (yc[y][x] < 0.2f) {
                        nh = 38;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 39;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 40;
                    } else if (yc[y][x] < 0.26f) {
                        nh = 41;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 42;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 43;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 44;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 45;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 46;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 47;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 48;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 49;
                    }
                } else if (xc[y][x] < 0.325f && yc[y][x] > 0.1f) {//neutral  34
                    if (yc[y][x] < 0.2f) {
                        nh = 50;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 51;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 52;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 53;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 54;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 55;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 56;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 57;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 58;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 59;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 60;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 61;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 62;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 63;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 64;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 65;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 66;
                    }
                } else if (xc[y][x] < 0.335f && yc[y][x] > 0.1f) {//neutral
                    if (yc[y][x] < 0.2f) {
                        nh = 67;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 68;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 69;
                    } else if (yc[y][x] < 0.27f) {
                        nh = 70;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 71;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 72;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 73;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 74;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 75;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 76;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 77;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 78;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 79;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 80;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 81;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 82;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 83;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 84;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 85;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 86;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 87;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 88;
                    }
                } else if (xc[y][x] < 0.340f && yc[y][x] > 0.1f) {//neutral
                    if (yc[y][x] < 0.2f) {
                        nh = 89;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 90;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 91;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 92;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 93;
                    } else if (yc[y][x] < 0.325f) {
                        nh = 94;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 95;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 96;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 97;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 98;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 99;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 100;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 101;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 102;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 103;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 104;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 105;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 106;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 107;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 108;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 109;
                    } else if (yc[y][x] < 0.7f) {
                        nh = 110;
                    }
                } else if (xc[y][x] < 0.345f && yc[y][x] > 0.1f) {//neutral  37
                    if (yc[y][x] < 0.2f) {
                        nh = 111;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 112;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 113;
                    } else if (yc[y][x] < 0.26f) {
                        nh = 114;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 115;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 116;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 117;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 118;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 119;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 120;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 121;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 122;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 123;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 124;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 125;
                    } else if (yc[y][x] < 0.39f) {
                        nh = 126;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 127;
                    } else if (yc[y][x] < 0.42f) {
                        nh = 128;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 129;
                    } else if (yc[y][x] < 0.48f) {
                        nh = 130;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 131;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 132;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 133;
                    }
                } else if (xc[y][x] < 0.355f && yc[y][x] > 0.1f) {//neutral  37
                    if (yc[y][x] < 0.2f) {
                        nh = 134;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 135;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 136;
                    } else if (yc[y][x] < 0.26f) {
                        nh = 137;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 138;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 139;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 140;
                    } else if (yc[y][x] < 0.335f) {
                        nh = 141;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 142;
                    } else if (yc[y][x] < 0.345f) {
                        nh = 143;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 144;
                    } else if (yc[y][x] < 0.355f) {
                        nh = 145;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 146;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 147;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 148;
                    } else if (yc[y][x] < 0.39f) {
                        nh = 149;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 150;
                    } else if (yc[y][x] < 0.42f) {
                        nh = 151;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 152;
                    } else if (yc[y][x] < 0.48f) {
                        nh = 153;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 154;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 155;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 156;
                    } else if (yc[y][x] < 0.65f) {
                        nh = 157;
                    }
                } else if (xc[y][x] < 0.365f && yc[y][x] > 0.15f) {  //0.4
                    if (yc[y][x] < 0.2f) {
                        nh = 158;
                    } else if (yc[y][x] < 0.22f) {
                        nh = 159;
                    } else if (yc[y][x] < 0.24f) {
                        nh = 160;
                    } else if (yc[y][x] < 0.26f) {
                        nh = 161;
                    } else if (yc[y][x] < 0.29f) {
                        nh = 162;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 163;
                    } else if (yc[y][x] < 0.33f) {
                        nh = 164;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 165;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 166;
                    } else if (yc[y][x] < 0.36f) {
                        nh = 167;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 168;
                    } else if (yc[y][x] < 0.38f) {
                        nh = 169;
                    } else if (yc[y][x] < 0.39f) {
                        nh = 170;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 171;
                    } else if (yc[y][x] < 0.42f) {
                        nh = 172;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 173;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 174;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 175;
                    } else if (yc[y][x] < 0.63f) {
                        nh = 176;
                    }
                } else if (xc[y][x] < 0.405f && yc[y][x] > 0.15f) {//45
                    if (yc[y][x] < 0.2f && purp) {//no take into account if purp = false
                        nh = 177;
                    } else if (yc[y][x] < 0.22f && purp) {
                        nh = 178;
                    } else if (yc[y][x] < 0.24f && purp) {
                        nh = 179;
                    } else if (yc[y][x] < 0.26f && purp) {
                        nh = 180;
                    } else if (yc[y][x] < 0.29f && purp) {
                        nh = 181;
                    } else if (yc[y][x] < 0.32f) {
                        nh = 182;
                    } else if (yc[y][x] < 0.34f) {
                        nh = 183;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 184;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 185;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 186;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 187;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 188;
                    } else if (yc[y][x] < 0.6f) {
                        nh = 189;
                    }
                } else if (xc[y][x] < 0.445f && yc[y][x] > 0.15f) {//45
                    if (yc[y][x] < 0.2f && purp) {
                        nh = 190;
                    } else if (yc[y][x] < 0.22f && purp) {
                        nh = 191;
                    } else if (yc[y][x] < 0.24f && purp) {
                        nh = 192;
                    } else if (yc[y][x] < 0.26f && purp) {
                        nh = 193;
                    } else if (yc[y][x] < 0.29f && purp) {
                        nh = 194;
                    } else if (yc[y][x] < 0.32f && purp) {
                        nh = 195;
                    } else if (yc[y][x] < 0.34f && purp) {
                        nh = 196;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 197;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 198;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 199;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 200;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 201;
                    } else if (yc[y][x] < 0.58f) {
                        nh = 202;
                    }
                } else if (xc[y][x] < 0.495f && yc[y][x] > 0.15f) {
                    if (yc[y][x] < 0.2f && purp) {
                        nh = 203;
                    } else if (yc[y][x] < 0.22f && purp) {
                        nh = 204;
                    } else if (yc[y][x] < 0.24f && purp) {
                        nh = 205;
                    } else if (yc[y][x] < 0.26f && purp) {
                        nh = 206;
                    } else if (yc[y][x] < 0.29f && purp) {
                        nh = 207;
                    } else if (yc[y][x] < 0.32f && purp) {
                        nh = 208;
                    } else if (yc[y][x] < 0.34f && purp) {
                        nh = 209;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 210;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 211;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 212;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 213;
                    } else if (yc[y][x] < 0.55f) {
                        nh = 214;
                    }
                } else if (xc[y][x] < 0.545f && yc[y][x] > 0.15f) {
                    if (yc[y][x] < 0.2f && purp) {
                        nh = 215;
                    } else if (yc[y][x] < 0.22f && purp) {
                        nh = 216;
                    } else if (yc[y][x] < 0.24f && purp) {
                        nh = 217;
                    } else if (yc[y][x] < 0.26f && purp) {
                        nh = 218;
                    } else if (yc[y][x] < 0.29f && purp) {
                        nh = 219;
                    } else if (yc[y][x] < 0.32f && purp) {
                        nh = 220;
                    } else if (yc[y][x] < 0.34f && purp) {
                        nh = 221;
                    } else if (yc[y][x] < 0.37f) {
                        nh = 222;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 223;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 224;
                    } else if (yc[y][x] < 0.5f) {
                        nh = 225;
                    }
                } else if (xc[y][x] < 0.595f && yc[y][x] > 0.15f) {
                    if (yc[y][x] < 0.22f) {
                        nh = 226;
                    } else if (yc[y][x] < 0.25f) {
                        nh = 227;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 228;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 229;
                    } else if (yc[y][x] < 0.4f) {
                        nh = 230;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 231;
                    }
                } else if (xc[y][x] < 0.65f && yc[y][x] > 0.12f) {
                    if (yc[y][x] < 0.25f) {
                        nh = 232;
                    } else if (yc[y][x] < 0.3f) {
                        nh = 233;
                    } else if (yc[y][x] < 0.35f) {
                        nh = 234;
                    } else if (yc[y][x] < 0.45f) {
                        nh = 235;
                    }
                } else if (xc[y][x] < 0.75f && yc[y][x] > 0.1f) {
                    nh = 236; //191
                }

                if (nh >= 0) {
                    histxythr[nh]++;
                    xxxthr[nh] += xc[y][x];
                    yyythr[nh] += yc[y][x];
                    YYYthr[nh] += Yc[y][x];
                }

//enable display cells
                /*
                                    // update
                                    int x1 = (int)(100.0 * (xc[y][x]));
                                    int y1 = (int)(100.0 * (yc[y][x]));

                                    if (x1 >= 0 && x1 < 80 && y1 >= 0 && y1 < 90) {
                                        cellxy[x1][y1]++;
                                        totalpixels++;
                                    }
                */
            }
        }

#ifdef _OPENMP
        #pragma omp critical    //disable if enable display cells
#endif
        {
            histxy += histxythr;
            xxx += xxxthr;
            yyy += yyythr;
            YYY += YYYthr;
        }
    }
}

float static studentXY(const array2D<float> & YYcurr, const array2D<float> & reffYY, int sizcurr, int Nc, int tt)
{
    //calculate Student coeff YY
    float somcurrY = 0.f;
    float somreffY = 0.f;
    float somcurr2Y = 0.f;
    float somreff2Y = 0.f;

    for (int i = 0; i < sizcurr; i++) {
        somcurrY += YYcurr[i][tt];
        //sum observations first group
    }

    somcurrY *= 100.f;

    for (int i = 0; i < Nc; i++) {
        somreffY += reffYY[i][tt];
        //sum observations second group
    }

    somreffY *= 100.f;

    for (int i = 0; i < sizcurr; i++) {
        somcurr2Y += SQR(YYcurr[i][tt]);
        //sum sqr observations first group
    }

    somcurr2Y *= SQR(100.f);

    for (int i = 0; i < Nc; i++) {
        somreff2Y += SQR(reffYY[i][tt]);
        //sum sqr observations second group
    }

    somreff2Y *= SQR(100.f);

    const float somsqueccurrY = somcurr2Y - SQR(somcurrY) / sizcurr;
    //sum sqr differences  first
    const float somsquecreffY = somreff2Y - SQR(somreffY) / Nc;
    //sum sqr differences  second

    const float diviY = std::sqrt(((somsqueccurrY + somsquecreffY) * (1.f / sizcurr + 1.f / Nc)) / (sizcurr + Nc - 2));
    //divisor student
    const float numerY = somcurrY / sizcurr - somreffY / Nc;
    //numerator student

    return numerY / diviY ;
    //student coeeficient
}




void RawImageSource::ItcWB(bool extra, double &tempref, double &greenref, double &tempitc, double &greenitc, float &temp0, float &delta, int &bia, int &dread, int &kcam, int &nocam, float &studgood, float &minchrom, int &kmin, float &minhist, float &maxhist,  array2D<float> &redloc, array2D<float> &greenloc, array2D<float> &blueloc, int bfw, int bfh, double &avg_rm, double &avg_gm, double &avg_bm, const ColorManagementParams &cmp, const RAWParams &raw, const WBParams & wbpar, const ToneCurveParams &hrp)
{
    /*
    Copyright (c) Jacques Desmis 6 - 2018 jdesmis@gmail.com, update 6 - 2023
    Copyright (c) Ingo Weyrich 3 - 2020 (heckflosse67@gmx.de)

    This algorithm try to find temperature correlation between 20 to 80 colors between 201 spectral color and about 20 to 55 color found in the image between 236, I just found the idea in the web "correlate with chroma" instead of RGB grey point,but I don't use any algo found on the web.

    I have test many many algorithms to find the first one that work :)
    Probably (sure) there are improvement to do...

    I have create a table temperature with temp and white point with 191 values between 2000K and 15000K we can obviously  change these values, more...with different steps
    I have create a table for tint (green)with 134 values between 0.4 to 4.
    I have create or recuparate and transformed 406 spectral colors from Colorchecker24, others color and my 468 colors target, or from web flowers, etc. with a step of 5nm, I think it is large enough.
    I think this value of 265 is now complete: I tested correlation with 60, 90, 100, 120, 155...better student increase with number of color, but now it seems stabilized
    Of course we can increase this number :)

    1) for the current raw file we create a table for each temp of RGB multipliers
    2) then, I choose the "camera temp" to initialize calculation (why not)
    3) for this temp, I calculated XYZ values for the 406 spectral data
    4) then I create for the image an "histogram", but for xyY (CIE 1931 color space or CIE 1964)
    5) for each pixel (in fact to accelerate only 1/3 for and 1/3 for y), I determine for each couple xy, the number of occurrences
    6) I sort this result in ascending order
    7) in option we can sort in another manner to take into account chroma : chromax = x - white point x, chromay = y - white point y
    8) then I compare this result, with spectral data found above in 3) with deltaE (limited to chroma)
    9) at this point we have xyY values that match Camera temp, and spectral data associated
    10) then I recalculate RGB values from xyY histogram
    11) after, I vary temp, between 2000K to 12000K
    12) RGB values are recalculated from 10) with RGB multipliers, and then xyY are calculated for each temp
    13) spectral data choose are recalculated with temp between 2000K to 12000K with matrix spectral calculation, that leads to xyY values
    14) I calculated for each couple xy, Student correlation (without Snedecor test)
    15) the good result, is the best correlation
    16) we have found the best temperature where color image and color references are correlate
    17) after we pass this value to improccoordinator

    18) in a second part if camera green is out, I used an "extra" algorithm
    19) we make vary green between 2 limits
    20) between these green limits, we make slightly vary temp and recalculated RGB multipliers
    21) with this multipliers for the RGB color find in histogram we recalculate xyY
    22) we re-adjust references color for these xyY from 20)
    23) then find all Student correlation for each couple green / temp
    24) sort these Student values, and choose the minimum
    25) then for the 3 better couple "temp / green" choose the one where green is nearest from 1.

    Some variables or function are not used, keep in case of
    I have test with cat02 but result are not stable enough ! why ??, therefore cat02 neutralized
    This operation is done (actually) 100 times and compare Student coefficient, and keep the absolute  minimum, We can probably optimize....
    But actually the goal is to find the good algorithm !

    I think, this algo is very good in most cases :) ...to verify of course.
    You can used it in images :flowers, landscape, portrait, skin, where illuminants are "normal" (daylight, blackbody)
    You must avoid when illuminant is non standard (fluorescent, LED...) and also, when the subject is lost in the image (some target to generate profiles).

    You can change  parameters in White Balance - Frame adapted to Itcwb
    Itcwb_rgreen : 1 amplitude of green variation - between 0 to 2
    Itcwb_prim : sRGB, Beta rgb (default), XYZcam, JDCmax = Use near Ciexy diagram instead of sRGB
    itcwb_delta : 4 by default can be set between 0 to 5 ==> delta temp to build histogram xy - if camera temp is not probably good
    itcwb_nopurple : false default - allow to bypass highlight recovery and inpait opposed when need flowers and not purple due to highlights...
    itcwb_green - adjust green refinement
    */
   // BENCHFUN
    MyTime t1, t2, t3, t4, t5, t6, t7, t8;
    t1.set();

    bool itciterate = true;
    bool lastitc = true;

    typedef struct Wboptim {//store config Itcwb
        float stud;
        float minc;
        double titc;
        double gritc;
        double tempre;
        double greenre;
        int drea;
        int kmi;
        float minhis;
        float maxhis;
        double avg_r;
        double avg_g;
        double avg_b;
        float delt;

    } Wboptim;

    Wboptim optitc[2] = {
        {0.f, 0.f, 5000., 1., 5000., 1., 1, 1, 10.f, 100.f, 1., 1., 1., 0.f},
        {0.f, 0.f, 5000., 1., 5000., 1., 1, 1, 10.f, 100.f, 1., 1., 1., 0.f}
    };
    int nbitc = 0;
    int choiceitc = 0;
    bool oldsampling = wbpar.itcwb_sampling;

    while (itciterate) {//loop to find best mix minchrom and studgood and deltaE patch
        Glib::ustring profuse;
        profuse = "JDCmax";

        float limx = 0.05f;
        float limy = 0.04f;

        if (wbpar.itcwb_prim == "srgb") {
            profuse = "sRGB";
            limx = 0.12f;
            limy = 0.06f;
        } else if (wbpar.itcwb_prim == "beta") {
            profuse = "Beta RGB";
            limx = 0.1f;
            limy = 0.05f;
        } else if (wbpar.itcwb_prim == "XYZcam") {
            profuse = "XYZcam";
            limx = 0.05f;
            limy = 0.04f;
        } else if (wbpar.itcwb_prim == "jdcmax") {
            profuse = "JDCmax";
            limx = 0.05f;
            limy = 0.04f;
        }


        if (oldsampling) {
            profuse = "sRGB";
        }

        float wb[3][3], iwb[3][3];
        double wb2[3][3];

        if (profuse == "XYZcam") {//thanks to Reffort

            // get a copy of the camera matrices
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    wb[r][c] = imatrices.xyz_cam[r][c];
                    wb2[r][c] = imatrices.xyz_cam[r][c];
                    iwb[r][c] = imatrices.cam_xyz[r][c];
                }
            }
        } else if ((cmp.inputProfile == "(camera)")) {//when no input profile found or if user select Camera standard
            if (settings->verbose) {
                printf("Use Camera Dcraw-Matrix and modify rgbloc\n");
            }

            //improvment with new values for redloc, greenloc, blueloc when Camera Dcraw is used
            TMatrix iwork = ICCStore::getInstance()->workingSpaceInverseMatrix(profuse);
            TMatrix workn = ICCStore::getInstance()->workingSpaceMatrix(profuse);
            double mat[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    for (int k = 0; k < 3; k++) {
                        mat[i][j] += iwork[i][k] * imatrices.xyz_cam[k][j];    // rgb_xyz * imatrices.xyz_cam
                    }

#ifdef _OPENMP
            #pragma omp parallel for
#endif

            for (int y = 0; y < bfh; y++)
                for (int x = 0; x < bfw; x++) {

                    float newred = mat[0][0] *  redloc[y][x] + mat[0][1] *  greenloc[y][x] + mat[0][2] * blueloc[y][x];
                    float newgreen = mat[1][0] *  redloc[y][x] + mat[1][1] *  greenloc[y][x] + mat[1][2] * blueloc[y][x];
                    float newblue = mat[2][0] *  redloc[y][x] + mat[2][1] *  greenloc[y][x] + mat[2][2] * blueloc[y][x];

                    redloc[y][x] = newred;//new values for redloc
                    greenloc[y][x] = newgreen;
                    blueloc[y][x] = newblue;

                }

            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    wb[r][c] = workn[r][c];
                    wb2[r][c] = workn[r][c];
                    iwb[r][c] = iwork[r][c];
                }
            }

        } else {

            TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(profuse);
            TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(profuse);

            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    wb[r][c] = wprof[r][c];
                    wb2[r][c] = wprof[r][c];
                    iwb[r][c] = wiprof[r][c];
                }
            }
        }

        if (settings->verbose) {
            printf("Sampling=%s \n", profuse.c_str());
            printf("wp = %f %f %f\n", wb[0][0], wb[0][1], wb[0][2]);
            printf("     %f %f %f\n", wb[1][0], wb[1][1], wb[1][2]);
            printf("     %f %f %f\n", wb[2][0], wb[2][1], wb[2][2]);
        }

        const int bfwitc = bfw;
        const int bfhitc = bfh;

        typedef struct WbGreen {
            double green;
            float snedecor;//1. actually but put in case of confiance interval
        } WbGreen;
        //green (tint) values between 0.4 to 4.0
        constexpr WbGreen gree[134] = {//symmetric coefficient between 0.717 and 1.40
            {0.400, 1.f},
            {0.420, 1.f},
            {0.440, 1.f},
            {0.460, 1.f},
            {0.480, 1.f},
            {0.500, 1.f},
            {0.520, 1.f},
            {0.540, 1.f},
            {0.550, 1.f},
            {0.560, 1.f},
            {0.570, 1.f},
            {0.580, 1.f},
            {0.590, 1.f},
            {0.600, 1.f},
            {0.610, 1.f},
            {0.620, 1.f},//extended range
            {0.630, 1.f},
            {0.640, 1.f},
            {0.650, 1.f},
            {0.660, 1.f},
            {0.670, 1.f},
            {0.680, 1.f},
            {0.690, 1.f},
            {0.700, 1.f},
            {0.714, 1.f},//usual 2 range
            {0.727, 1.f},
            {0.741, 1.f},
            {0.755, 1.f},
            {0.769, 1.f},
            {0.784, 1.f},
            {0.800, 1.f},
            {0.806, 1.f},
            {0.813, 1.f},
            {0.820, 1.f},//usual range
            {0.826, 1.f},
            {0.833, 1.f},
            {0.840, 1.f},
            {0.847, 1.f},
            {0.855, 1.f},
            {0.862, 1.f},
            {0.870, 1.f},
            {0.877, 1.f},
            {0.885, 1.f},
            {0.893, 1.f},
            {0.901, 1.f},
            {0.909, 1.f},
            {0.917, 1.f},
            {0.926, 1.f},
            {0.935, 1.f},
            {0.943, 1.f},//49 limit low normal
            {0.952, 1.f},
            {0.962, 1.f},
            {0.971, 1.f},
            {0.980, 1.f},
            {0.990, 1.f},
            {1.000, 1.f},//55 reference
            {1.010, 1.f},
            {1.020, 1.f},
            {1.030, 1.f},
            {1.040, 1.f},
            {1.050, 1.f},
            {1.060, 1.f},
            {1.070, 1.f},
            {1.080, 1.f},
            {1.090, 1.f},
            {1.100, 1.f},
            {1.110, 1.f},
            {1.120, 1.f},
            {1.130, 1.f},
            {1.140, 1.f},
            {1.150, 1.f},
            {1.160, 1.f},
            {1.170, 1.f},
            {1.180, 1.f},
            {1.190, 1.f},
            {1.200, 1.f},
            {1.210, 1.f},
            {1.220, 1.f},
            {1.230, 1.f},
            {1.240, 1.f},
            {1.250, 1.f},// usual range
            {1.275, 1.f},
            {1.300, 1.f},
            {1.325, 1.f},
            {1.350, 1.f},
            {1.375, 1.f},
            {1.400, 1.f},//usual 2 range
            {1.425, 1.f},
            {1.450, 1.f},
            {1.475, 1.f},
            {1.500, 1.f},
            {1.525, 1.f},
            {1.550, 1.f},
            {1.575, 1.f},//extended range
            {1.600, 1.f},
            {1.633, 1.f},
            {1.666, 1.f},
            {1.700, 1.f},
            {1.733, 1.f},
            {1.766, 1.f},
            {1.800, 1.f},
            {1.833, 1.f},
            {1.866, 1.f},
            {1.900, 1.f},
            {1.933, 1.f},
            {1.966, 1.f},
            {2.000, 1.f},
            {2.033, 1.f},
            {2.066, 1.f},
            {2.100, 1.f},
            {2.133, 1.f},
            {2.166, 1.f},
            {2.200, 1.f},
            {2.250, 1.f},
            {2.300, 1.f},
            {2.350, 1.f},
            {2.400, 1.f},
            {2.450, 1.f},
            {2.500, 1.f},
            {2.550, 1.f},
            {2.600, 1.f},
            {2.650, 1.f},
            {2.700, 1.f},
            {2.750, 1.f},
            {2.800, 1.f},
            {2.850, 1.f},
            {2.900, 1.f},
            {2.950, 1.f},
            {3.000, 1.f},
            {3.200, 1.f},
            {3.400, 1.f},
            {3.600, 1.f},
            {3.800, 1.f},
            {4.000, 1.f}
        };
        const int N_g = sizeof(gree) / sizeof(gree[0]);   //number of green

        typedef struct RangeGreen {
            int begin;
            int end;
        } RangeGreen;

        int greenrefo = 55;
        double origgreen = greenitc;

        for (int gg = 0; gg < N_g; gg++) {
            if (gree[gg].green > origgreen) {
                greenrefo = gg;//show the green
                break;
            }
        }

        constexpr RangeGreen Rangestandard = {33, 80};//usual green range
        constexpr RangeGreen Rangestandard2 = {24, 86};//usual 2 green range
        constexpr RangeGreen Rangeextended = {15, 93};
        const RangeGreen Rangemax = {0, N_g};

        RangeGreen Rangegreenused;

        if (wbpar.itcwb_rgreen == 0) {
            Rangegreenused = Rangestandard;
        } else if (wbpar.itcwb_rgreen == 1) {
            Rangegreenused = Rangestandard2;
        } else if (wbpar.itcwb_rgreen == 2) {
            Rangegreenused = Rangeextended;
        } else {
            Rangegreenused = Rangemax;
        }


        if (wbpar.itcwb_rgreen == 0) {//new way to set green
            Rangegreenused.begin = std::max(greenrefo - 13, 0);
            Rangegreenused.end = std::min(greenrefo + 13, N_g);
        }

        if (wbpar.itcwb_rgreen == 1) {//new way to set green
            Rangegreenused.begin = std::max(greenrefo - 17, 0);
            Rangegreenused.end = std::min(greenrefo + 17, N_g);
        }

        if (oldsampling == true) {
            Rangegreenused = Rangestandard2;
        }

        typedef struct WbTxyz {
            double Tem;
            double XX;
            double ZZ;
        } WbTxyz;
        //we can change step to increase precision if need  - also in Colortemp.cc with same changes
        //I don't know how to pass this structure to Colortemp !
        // X and Z values calculate for each temp between 2000K to  15000K, so no result after 15000K !
        //of course we can change the step between each temp if need

        constexpr WbTxyz Txyz[191] = {//temperature Xwb Zwb 191 values  x wb and y wb are calculated after,  Xwb and Ywb calculated with a spreadsheet
            {2001., 1.273842, 0.145295},
            {2051., 1.258802, 0.156066},
            {2101., 1.244008, 0.167533},
            {2151., 1.230570, 0.178778},
            {2201., 1.217338, 0.190697},
            {2251., 1.205305, 0.202338},
            {2301., 1.193444, 0.214632},
            {2351., 1.182648, 0.226598},
            {2401., 1.171996, 0.239195},
            {2451., 1.162290, 0.251421},
            {2501., 1.152883, 0.264539},
            {2551., 1.143965, 0.276682},
            {2605., 1.134667, 0.290722},
            {2655., 1.126659, 0.303556},
            {2705., 1.119049, 0.316446},
            {2755., 1.111814, 0.329381},
            {2790., 1.106961, 0.338455},
            {2803., 1.105381, 0.342193},
            {2825., 1.102275, 0.347542},
            {2856., 1.098258, 0.355599},
            {2880., 1.095233, 0.361840},
            {2910., 1.091550, 0.369645},
            {2930., 1.089155, 0.374849},
            {2960., 1.085649, 0.382655},
            {2980., 1.083369, 0.387858},
            {3003., 1.080982, 0.394258},
            {3025., 1.078397, 0.399561},
            {3050., 1.075727, 0.406057},
            {3075., 1.073122, 0.412550},
            {3103., 1.070277, 0.419815},
            {3128., 1.067801, 0.426296},
            {3153., 1.065384, 0.432769},
            {3175., 1.063305, 0.438459},
            {3203., 1.060906, 0.446161},
            {3225., 1.058738, 0.451367},
            {3250., 1.056535, 0.457806},
            {3280., 1.053960, 0.465519},
            {3303., 1.052034, 0.471422},
            {3353., 1.047990, 0.484218},
            {3400., 1.044547, 0.496719},
            {3450., 1.040667, 0.508891},
            {3500., 1.037145, 0.521523},
            {3550., 1.033783, 0.534090},
            {3600., 1.030574, 0.546590},
            {3650., 1.027510, 0.559020},
            {3699., 1.024834, 0.571722},
            {3801., 1.019072, 0.596102},
            {3851., 1.016527, 0.608221},
            {3902., 1.014244, 0.621136},
            {3952., 1.011729, 0.632447},
            {4002., 0.996153, 0.609518},
            {4052., 0.993720, 0.620805},
            {4102., 0.993908, 0.631520},
            {4152., 0.989179, 0.643262},
            {4202., 0.989283, 0.653999},
            {4252., 0.985039, 0.665536},
            {4302., 0.985067, 0.676288},
            {4352., 0.981271, 0.687599},
            {4402., 0.981228, 0.698349},
            {4452., 0.977843, 0.709425},
            {4502., 0.977736, 0.720159},
            {4552., 0.974728, 0.730993},
            {4602., 0.974562, 0.741698},
            {4652., 0.971899, 0.752284},
            {4702., 0.971681, 0.762949},
            {4752., 0.969335, 0.773285},
            {4802., 0.969069, 0.783899},
            {4827., 0.967570, 0.788836},
            {4852., 0.967011, 0.793982},
            {4877., 0.966465, 0.799108},
            {4902., 0.965933, 0.804214},
            {4914., 0.965682, 0.806658},
            {4927., 0.965414, 0.809229},
            {4940., 0.965149, 0.811937},
            {4952., 0.964908, 0.814366},
            {4965., 0.964650, 0.816993},
            {4977., 0.964415, 0.819412},
            {4990., 0.964163, 0.822028},
            {5002., 0.963934, 0.824438},//80
            {5015., 0.963689, 0.827044},
            {5027., 0.963465, 0.829444},
            {5040., 0.963226, 0.832039},
            {5051., 0.963008, 0.834429},
            {5065., 0.963226, 0.832039},
            {5077., 0.962563, 0.839395},
            {5090., 0.962336, 0.841968},
            {5102., 0.962129, 0.844339},
            {5115., 0.961907, 0.846902},
            {5127., 0.961706, 0.849263},
            {5140., 0.961490, 0.851815},
            {5151., 0.961294, 0.854166},
            {5177., 0.960893, 0.859049},
            {5202., 0.960501, 0.863911},
            {5253., 0.959749, 0.873572},
            {5302., 0.959313, 0.883815},
            {5351., 0.958361, 0.892644},
            {5402., 0.957903, 0.902793},
            {5452., 0.957116, 0.911379},
            {5502., 0.956639, 0.921431},
            {5553., 0.956002, 0.929779},
            {5602., 0.955509, 0.939728},
            {5652., 0.955008, 0.947842},
            {5702., 0.954502, 0.957685},
            {5752., 0.954124, 0.965569},
            {5802., 0.953608, 0.975303},
            {5852., 0.953342, 0.982963},
            {5902., 0.952818, 0.992584},
            {5952., 0.952652, 1.000025},
            {6002., 0.952122, 1.009532},
            {6052., 0.952047, 1.016759},
            {6102., 0.951514, 1.026149},
            {6152., 0.951520, 1.033168},
            {6202., 0.950985, 1.042439},
            {6252., 0.951064, 1.049256},
            {6302., 0.950530, 1.058406},
            {6352., 0.950674, 1.065027},
            {6380., 0.950576, 1.069386},
            {6402., 0.950143, 1.074055},
            {6425., 0.950428, 1.076341},
            {6452., 0.950345, 1.080484},
            {6475., 0.950277, 1.083996},
            {6502., 0.950201, 1.088097},
            {6525., 0.950139, 1.091573},
            {6552., 0.950070, 1.095633},
            {6575., 0.950014, 1.099075},
            {6602., 0.949952, 1.103094},
            {6625., 0.949902, 1.106501},
            {6652., 0.949846, 1.110479},
            {6675., 0.949801, 1.113852},
            {6702., 0.949752, 1.119138},
            {6725., 0.949712, 1.121128},
            {6752., 0.949668, 1.125027},
            {6802., 0.949596, 1.132190},
            {6852., 0.949533, 1.139281},
            {6902., 0.949033, 1.147691},
            {6952., 0.949437, 1.153246},
            {7002., 0.949402, 1.160129},
            {7052., 0.949376, 1.166966},
            {7102., 0.949358, 1.173732},
            {7152., 0.949348, 1.180429},
            {7202., 0.949346, 1.187058},
            {7252., 0.949350, 1.193619},
            {7301., 0.948896, 1.201432},
            {7352., 0.949380, 1.206541},
            {7402., 0.949405, 1.212904},
            {7451., 0.949434, 1.219076},
            {7501., 0.949471, 1.225312},
            {7551., 0.949512, 1.231485},
            {7601., 0.949099, 1.239061},
            {7675., 0.949638, 1.246525},
            {7751., 0.949729, 1.255559},
            {7825., 0.949828, 1.264225},
            {7901., 0.949498, 1.274460},
            {7952., 0.950018, 1.278800},
            {8025., 0.950137, 1.287013},
            {8095., 0.950259, 1.294777},
            {8151., 0.950361, 1.300912},
            {8225., 0.950501, 1.308915},
            {8301., 0.950253, 1.318464},
            {8375., 0.950804, 1.324786},
            {8451., 0.950966, 1.332651},
            {8525., 0.951129, 1.340199},
            {8601., 0.950941, 1.349261},
            {8701., 0.951533, 1.357724},
            {8801., 0.951772, 1.367421},
            {8901., 0.952018, 1.376935},
            {9001., 0.951969, 1.387639},
            {9201., 0.952784, 1.404422},
            {9401., 0.953081, 1.423213},//since 5 2023 I increased the number of temp references above 12000K
            {9651., 0.953993, 1.442883},
            {9901., 0.954537, 1.464134},
            {10201., 0.955520, 1.485825},
            {10501., 0.956321, 1.508623},
            {10751., 0.957057, 1.524806},
            {11001., 0.957747, 1.541281},
            {11251., 0.958436, 1.557207},
            {11501., 0.959112, 1.572366},
            {11751., 0.959784, 1.587037},
            {12001., 0.960440, 1.601019},//since 5 2023 I increased the number of temp refrences above 12000K
            {12251., 0.961090, 1.614566},
            {12501., 0.963963, 1.627492},
            {12751., 0.962350, 1.640031},
            {13001., 0.962962, 1.652055},
            {13251., 0.963561, 1.663638},
            {13501., 0.964147, 1.674804},
            {13751., 0.964720, 1.685571},
            {14001., 0.965279, 1.695919},
            {14251., 0.965827, 1.705950},
            {14501., 0.966363, 1.715637},
            {14751., 0.966886, 1.724998},
            {15001., 0.967397, 1.734047}
        };
        //compatibility 5.9
        constexpr WbTxyz Txyzs[118] = {//temperature Xwb Zwb 118 values - same table as in Rawimagesource.cc  x wb and y wb are calculated after
            {2001., 1.273842, 0.145295},
            {2101., 1.244008, 0.167533},
            {2201., 1.217338, 0.190697},
            {2301., 1.193444, 0.214632},
            {2401., 1.171996, 0.239195},
            {2501., 1.152883, 0.264539},
            {2605., 1.134667, 0.290722},
            {2655., 1.126659, 0.303556},
            {2705., 1.119049, 0.316446},
            {2755., 1.111814, 0.329381},
            {2803., 1.105381, 0.342193},
            {2856., 1.098258, 0.355599},
            {2910., 1.091550, 0.369645},
            {2960., 1.085649, 0.382655},
            {3003., 1.080982, 0.394258},
            {3050., 1.075727, 0.406057},
            {3103., 1.070277, 0.419815},
            {3153., 1.065384, 0.432769},
            {3203., 1.060906, 0.446161},
            {3250., 1.056535, 0.457806},
            {3303., 1.052034, 0.471422},
            {3353., 1.047990, 0.484218},
            {3400., 1.044547, 0.496719},
            {3450., 1.040667, 0.508891},
            {3500., 1.037145, 0.521523},
            {3550., 1.033783, 0.534090},
            {3600., 1.030574, 0.546590},
            {3650., 1.027510, 0.559020},
            {3699., 1.024834, 0.571722},
            {3801., 1.019072, 0.596102},
            {3851., 1.016527, 0.608221},
            {3902., 1.014244, 0.621136},
            {3952., 1.011729, 0.632447},
            {4002., 0.996153, 0.609518},
            {4052., 0.993720, 0.620805},
            {4102., 0.993908, 0.631520},
            {4152., 0.989179, 0.643262},
            {4202., 0.989283, 0.653999},
            {4252., 0.985039, 0.665536},
            {4302., 0.985067, 0.676288},
            {4352., 0.981271, 0.687599},
            {4402., 0.981228, 0.698349},
            {4452., 0.977843, 0.709425},
            {4502., 0.977736, 0.720159},
            {4552., 0.974728, 0.730993},
            {4602., 0.974562, 0.741698},
            {4652., 0.971899, 0.752284},
            {4702., 0.971681, 0.762949},
            {4752., 0.969335, 0.773285},
            {4802., 0.969069, 0.783899},
            {4827., 0.967570, 0.788836},
            {4852., 0.967011, 0.793982},
            {4877., 0.966465, 0.799108},
            {4902., 0.965933, 0.804214},
            {4927., 0.965414, 0.809229},
            {4952., 0.964908, 0.814366},
            {4977., 0.964415, 0.819412},
            {5002., 0.963934, 0.824438},
            {5027., 0.963465, 0.829444},
            {5052., 0.963008, 0.834429},
            {5077., 0.962563, 0.839395},
            {5102., 0.962129, 0.844339},
            {5127., 0.961706, 0.849263},
            {5152., 0.961294, 0.854166},
            {5177., 0.960893, 0.859049},
            {5202., 0.960501, 0.863911},
            {5252., 0.959749, 0.873572},
            {5302., 0.959313, 0.883815},
            {5352., 0.958361, 0.892644},
            {5402., 0.957903, 0.902793},
            {5452., 0.957116, 0.911379},
            {5502., 0.956639, 0.921431},
            {5552., 0.956002, 0.929779},
            {5602., 0.955509, 0.939728},
            {5652., 0.955008, 0.947842},
            {5702., 0.954502, 0.957685},
            {5752., 0.954124, 0.965569},
            {5802., 0.953608, 0.975303},
            {5852., 0.953342, 0.982963},
            {5902., 0.952818, 0.992584},
            {5952., 0.952652, 1.000025},
            {6002., 0.952122, 1.009532},
            {6052., 0.952047, 1.016759},
            {6102., 0.951514, 1.026149},
            {6152., 0.951520, 1.033168},
            {6202., 0.950985, 1.042439},
            {6252., 0.951064, 1.049256},
            {6302., 0.950530, 1.058406},
            {6352., 0.950674, 1.065027},
            {6402., 0.950143, 1.074055},
            {6452., 0.950345, 1.080484},
            {6502., 0.950201, 1.088097},
            {6552., 0.950070, 1.095633},
            {6602., 0.949952, 1.103094},
            {6652., 0.949846, 1.110479},
            {6702., 0.949752, 1.119138},
            {6752., 0.949668, 1.125027},
            {6802., 0.949596, 1.132190},
            {6902., 0.949033, 1.147691},
            {7002., 0.949402, 1.160129},
            {7152., 0.949348, 1.180429},
            {7301., 0.948896, 1.201432},
            {7451., 0.949434, 1.219076},
            {7601., 0.949099, 1.239061},
            {7751., 0.949729, 1.255559},
            {7901., 0.949498, 1.274460},
            {8151., 0.950361, 1.300912},
            {8301., 0.950253, 1.318464},
            {8451., 0.950966, 1.332651},
            {8601., 0.950941, 1.349261},
            {8801., 0.951772, 1.367421},
            {9001., 0.951969, 1.387639},
            {9201., 0.952784, 1.404422},
            {9401., 0.953081, 1.423213},
            {9901., 0.954537, 1.464134},
            {10501., 0.956321, 1.508623},
            {11001., 0.957747, 1.541281},
            {12001., 0.960440, 1.601019}
        };
        bool purp = true;//if inpaint-opposed or something else enable purp

        int N_t = sizeof(Txyz) / sizeof(Txyz[0]);   //number of temperature White point

        if (oldsampling) {
            N_t = sizeof(Txyzs) / sizeof(Txyzs[0]);   //number of temperature White point
        }

     //   constexpr int Nc = 428 + 1; //429 number of reference spectral colors
        int Ncr = 429;

        if (wbpar.itcwb_prim == "srgb") {
            Ncr = 429;
        } else if (wbpar.itcwb_prim == "adob") {
            Ncr = 429;
        } else if (wbpar.itcwb_prim == "XYZcam") {
            Ncr = 429;
        } else if (wbpar.itcwb_prim == "jdcmax") {
            Ncr = 429;
        }

        if (oldsampling) { //low sampling 5.9 with less spectral datas 201
            Ncr = 202;
        }

        array2D<float> Tx(N_t, Ncr);
        array2D<float> Ty(N_t, Ncr);
        array2D<float> Tz(N_t, Ncr);
        array2D<float> Ta(N_t, Ncr);
        array2D<float> Tb(N_t, Ncr);
        array2D<float> TL(N_t, Ncr);

        double TX[Ncr];
        double TY[Ncr];
        double TZ[Ncr];

        std::vector<bool> good_spectral(Ncr, false);
        std::vector<bool> good_size(Ncr, false);

        double WPX[N_t];
        double WPZ[N_t];

        float rmm[N_t];
        float gmm[N_t];
        float bmm[N_t];

        int siza = 237; //192 untill 01/2023 size of histogram

        if (oldsampling == true) {
            siza = 192;//old sampling 5.9 and before...
        }

        // tempref and greenref are camera wb values.
        // I used them by default to select good spectral values !! but they are changed after
        tempref = rtengine::min(tempref, 15000.0);
        int repref = 0;

        for (int tt = 0; tt < N_t; tt++) {
            if (Txyz[tt].Tem > tempref) {
                repref = tt;//show the select temp
                break;
            }
        }

        if (oldsampling) {
            for (int tt = 0; tt < N_t; tt++) {
                if (Txyzs[tt].Tem > tempref) {
                    repref = tt;//show the select temp
                    break;
                }
            }
        }

        if (repref >= N_t - 1) {
            repref = N_t - 2;
        }

        //calculate R G B multiplier in function illuminant and temperature
        const bool isMono = (ri->getSensorType() == ST_FUJI_XTRANS && raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::MONO))
                            || (ri->getSensorType() == ST_BAYER && raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::MONO));
        greenitc += wbpar.itcwb_green;
        double keepgreen = greenitc;

        for (int tt = 0; tt < N_t; ++tt) {
            double r, g, b;
            float rm, gm, bm;

            if (!oldsampling) {
                ColorTemp(Txyz[tt].Tem, greenitc, 1., "Custom", wbpar.observer).getMultipliers(r, g, b);
            } else {
                ColorTemp(Txyzs[tt].Tem, greenitc, 1., "Custom", wbpar.observer).getMultipliers(r, g, b);//brings differences with old version 5.9, maybe Observer in 5.9, I did not find a solution
            }

            rm = imatrices.cam_rgb[0][0] * r + imatrices.cam_rgb[0][1] * g + imatrices.cam_rgb[0][2] * b;
            gm = imatrices.cam_rgb[1][0] * r + imatrices.cam_rgb[1][1] * g + imatrices.cam_rgb[1][2] * b;
            bm = imatrices.cam_rgb[2][0] * r + imatrices.cam_rgb[2][1] * g + imatrices.cam_rgb[2][2] * b;

            const float new_pre_mul[4] = { ri->get_pre_mul(0) / rm, ri->get_pre_mul(1) / gm, ri->get_pre_mul(2) / bm, ri->get_pre_mul(3) / gm };
            float new_scale_mul[4];
            const float gain = calculate_scale_mul(new_scale_mul, new_pre_mul, c_white, cblacksom, isMono, ri->get_colors());
            rm = new_scale_mul[0] / scale_mul[0] * gain;
            gm = new_scale_mul[1] / scale_mul[1] * gain;
            bm = new_scale_mul[2] / scale_mul[2] * gain;
            rmm[tt] = rm / gm;
            gmm[tt] = 1.f;
            bmm[tt] = bm / gm;
            //return rmm, gmm, bmm in function of temp
        }

        t2.set();

        if (settings->verbose) {
            printf("First: up calculate multipliers: %d nsec\n",  t2.etime(t1));
        }

        struct hiss {//histogram
            int histnum;
            int index;
            bool operator()(const hiss& lhis, const hiss& rhis)
            {
                return lhis.histnum < rhis.histnum;
            }

        } ;

        //intermediate structure
        struct chrom {//chroma image
            float chroxy_number;
            float number;
            float hue;
            float chroxy;
            float chrox;
            float chroy;
            float Y;
            int index;
            int interest;
            bool operator()(const chrom& lchro, const chrom& rchro)
            {
                return lchro.chroxy_number < rchro.chroxy_number;
            }

        } ;

        struct Temppatch {//patch characterictics
            float minchroma;
            float delt_E;
            float minhi;
            float maxhi;
            bool operator()(const Temppatch& ltp, const Temppatch& rtp)
            {
                return ltp.minchroma < rtp.minchroma;
            }
        };

        Temppatch  Tppat[N_t];

        LUTu histxy(siza); //number of values for each pair xy

        histxy.clear();

        LUTf xxx(siza);//for color references calculated ==> max in images "like histogram"

        xxx.clear();

        LUTf yyy(siza);

        yyy.clear();

        LUTf YYY(siza);//not used directly, but necessary to keep good range

        YYY.clear();

        bool separated = true;//true

        int w = -1;

        array2D<float> reff_spect_yy_camera(N_t, 2 * Ncr + 2);

        array2D<float> reff_spect_xx_camera(N_t, 2 * Ncr + 2);

        array2D<float> reff_spect_Y_camera(N_t, 2 * Ncr + 2);

        int ttbeg = 0;

        int ttend = N_t;

        //call tempxy to calculate for 406 or 201 color references Temp and XYZ with cat02
        double wpx = 0.;

        double wpz = 0.;

        ColorTemp::tempxy(separated, repref, Tx, Ty, Tz, Ta, Tb, TL, TX, TY, TZ, wbpar, ttbeg, ttend, wpx, wpz, WPX, WPZ); //calculate chroma xy (xyY) for Z known colors on under 200 illuminants

        //find the good spectral values
        //calculate xy reference spectral for tempref
        for (int j = 0; j < Ncr ; j++) {
            float xxx = std::max(TX[j] / (TX[j] + TY[j] +  TZ[j]), 0.01); // x from xyY
            float yyy = std::max(TY[j] / (TX[j] + TY[j] +  TZ[j]), 0.01); // y from xyY
            float YY = TY[j];
            reff_spect_xx_camera[j][repref] = xxx;
            reff_spect_yy_camera[j][repref] = yyy;
            reff_spect_Y_camera[j][repref] =  YY;
 /*
            //display spectral datas
                                float xr = reff_spect_xx_camera[j][repref];
                                float yr = reff_spect_yy_camera[j][repref];
                                float Yr = reff_spect_Y_camera[j][repref];
                                float X_r = (65535.f * (xr * Yr)) / yr;
                                float Z_r = (65535.f * (1.f - xr - yr) * Yr) / yr;
                                float Y_r = 65535.f * Yr;
                                float Lr, ar, br;
                                Color::XYZ2Lab(X_r, Y_r, Z_r, Lr, ar, br);//it make sense, because known spectral color


                        printf("Nc=%i repref=%i xxx=%f yyy=%f YY=%f Lr=%f a=%f b=%f\n", j, repref, (double) xxx, (double) yyy, (double) YY, (double) Lr/327.68f, (double) ar/327.68f, (double) br/327.68f);
 */
        }

        array2D<float> xc(bfwitc, bfhitc);
        array2D<float> yc(bfwitc, bfhitc);
        array2D<float> zc(bfwitc, bfhitc);
        array2D<float> Yc(bfwitc, bfhitc);

        // int rep = rtengine::LIM(repref + 1, 0, N_t);

        //initialize calculation of xy current for tempref
        if (oldsampling == false) {

            //small denoise with median 3x3 strong
            float** tmL;
            int wid = bfw;
            int hei = bfh;
            tmL = new float*[hei];

            for (int i = 0; i < hei; ++i) {
                tmL[i] = new float[wid];
            }

            typedef ImProcFunctions::Median Median;
            Median medianTypeL = Median::TYPE_3X3_STRONG;//x2
            int pas = 2;
            ImProcFunctions::Median_Denoise(redloc, redloc, bfw, bfh, medianTypeL, pas, false, tmL);
            ImProcFunctions::Median_Denoise(greenloc, greenloc, bfw, bfh, medianTypeL, pas, false, tmL);
            ImProcFunctions::Median_Denoise(blueloc, blueloc, bfw, bfh, medianTypeL, pas, false, tmL);

            for (int i = 0; i < hei; ++i) {
                delete[] tmL[i];
            }

            delete[] tmL;
        }

        t3.set();

        if (settings->verbose) {
            printf("Second: from first to up median 3x3: %d nsec\n", t3.etime(t2));
        }

        if (oldsampling == false) {
            if (settings->verbose) {
                printf("size rgb loc - bfh=%i bfw=%i repref=%i\n", bfh, bfw, repref);
            }
            
#ifdef _OPENMP
            #pragma omp parallel for
#endif

            for (int y = 0; y < bfh ; ++y) {
                for (int x = 0; x < bfw ; ++x) {
                    redloc[y][x] = clipitc(redloc[y][x]);
                    greenloc[y][x] = clipitc(greenloc[y][x]);
                    blueloc[y][x] = clipitc(blueloc[y][x]);

                    const float RR = rmm[repref] * redloc[y][x];
                    const float GG = gmm[repref] * greenloc[y][x];
                    const float BB = bmm[repref] * blueloc[y][x];

                    Color::rgbxyz(RR, GG, BB, xc[y][x], yc[y][x], zc[y][x], wb2);//use sRGB Adobe Rec2020 ACESp0
                    float X_r = xc[y][x];
                    float Y_r = yc[y][x];
                    float Z_r = zc[y][x];

                    if (oldsampling == false) {
                        Color::gamutmap(X_r, Y_r, Z_r, wb2);//gamut control
                    }

                    const float som = X_r + Y_r + Z_r;
                    xc[y][x] = X_r / som;
                    yc[y][x] = Y_r / som;
                    Yc[y][x] = Y_r / 65535.f;
                }
            }

            //histogram xy depend of temp...but in most cases D45 ..D65..
            // these values change with temp
            //calculate for this image the mean values for each family of color, near histogram x y (number)
            //xy vary from x 0..0.77  y 0..0.82
            //neutral values are near x=0.34 0.33 0.315 0.37 y =0.35 0.36 0.34
            //skin are about x 0.45  0.49 y 0.4 0.47
            //blue sky x=0.25 y=0.28  and x=0.29 y=0.32
            // step about 0.02   x 0.32 0.34  y= 0.34 0.36 skin    --  sky x 0.24 0.30 y 0.28 0.32


            if (wbpar.itcwb_nopurple == true) {//since 21 april - change to filter magenta
                purp = false;
            }

            histoxyY(bfhitc, bfwitc, xc, yc, Yc, xxx,  yyy, YYY, histxy, purp);//purp enable,  enable purple color in WB
            //return histogram x and y for each temp and in a range of 235 colors (siza)

        } else {
            const int deltarepref = 1;

            for (int nn = 0, drep = -deltarepref; nn <= 2; ++nn, drep += deltarepref) {
                //three loop to refine color if temp camera is probably not very good
                const int rep = rtengine::LIM(repref + drep, 0, N_t);

                //initialize calculation of xy current for tempref
#ifdef _OPENMP
                #pragma omp parallel for
#endif

                for (int y = 0; y < bfh ; ++y) {
                    for (int x = 0; x < bfw ; ++x) {
                        const float RR = rmm[rep] * redloc[y][x];
                        const float GG = gmm[rep] * greenloc[y][x];
                        const float BB = bmm[rep] * blueloc[y][x];
                        Color::rgbxyY(RR, GG, BB, xc[y][x], yc[y][x], Yc[y][x], wb);
                    }
                }


                histoxyY_low(bfhitc, bfwitc, xc, yc, Yc, xxx,  yyy, YYY, histxy);
                //return histogram x and y for each temp and in a range of 158 colors (siza)
            }

        }

//enable display cells
        /*
                printf ("xc\tyc\tcount\n") ;
                printf ("--\t--\t-----\n") ;
                for (int x1 = 0 ; x1 < 80; ++x1) {
                    for (int y1 = 0; y1 < 90; ++y1) {
                        if (cellxy[x1][y1] > 0) {
                            printf ("%d\t%d\t%d\n", x1, y1, cellxy[x1][y1]);
                        }
                    }
                }
        */

        // free some memory
        xc.free();
        yc.free();
        Yc.free();
        //calculate x y Y
        const int sizcurrref = siza;//choice of number of correlate colors in image
        array2D<float> histcurrref(N_t, sizcurrref);
        array2D<float> xx_curref(N_t, sizcurrref);
        array2D<float> yy_curref(N_t, sizcurrref);
        array2D<float> YY_curref(N_t, sizcurrref);
        array2D<float> xx_curref_reduc(N_t, sizcurrref);
        array2D<float> yy_curref_reduc(N_t, sizcurrref);
        array2D<float> YY_curref_reduc(N_t, sizcurrref);
        array2D<float> nn_curref_reduc(N_t, sizcurrref);//new array to improve patch
        array2D<float> chronum_curref_reduc(N_t, sizcurrref);//new array to improve patch
        array2D<float> hue_curref_reduc(N_t, sizcurrref);//new array to improve patch
        array2D<float> chro_curref_reduc(N_t, sizcurrref);//new array to improve patch
        array2D<float> estim_hue(N_t, sizcurrref);//new array to improve patch

        hiss Wbhis[siza];

        for (int nh = 0; nh < siza; nh++) {
            Wbhis[nh].histnum = histxy[nh];
            Wbhis[nh].index = nh;
        }

        //sort in ascending order
        std::sort(Wbhis, Wbhis + siza, Wbhis[0]);
        int n1 = 0;
        int n4 = 0;
        int n15 = 0;
        int n30 = 0;

        //part to improve
        //determined the number of colors who be used after
        int ntot = 0;

        for (int nh = 0; nh < siza; nh++) {
            if (Wbhis[nh].histnum > 0) {
                ntot++;
            }
        }

        dread = ntot;//read colors

        for (int nh = 0; nh < siza; nh++) {
            if (Wbhis[nh].histnum < 30) {
                n30++;    //keep only existing color but avoid to small

                if (Wbhis[nh].histnum < 15) {
                    n15++;    //keep only existing color but avoid to small

                    if (Wbhis[nh].histnum < 4) {
                        n4++;    //keep only existing color but avoid to small

                        if (Wbhis[nh].histnum < 1) {
                            n1++;    //keep only existing color but avoid to small
                        }
                    }
                }
            }
        }

        int ntr = n30;

        if (ntr > (siza - 25)) {
            ntr = n15;    //if to less elements 25 elements mini
        }

        if (ntr > (siza - 23)) {
            ntr = n4;    //if to less elements 25 elements mini
        }

        if (ntr > (siza - 20)) {
            ntr = n1;    //if to less elements 20 elements mini - normally never be used !
        }

        int sizcurr2ref = sizcurrref - ntr;
        const int sizcu30 = sizcurrref - n30;
        int maxsiz = 70;
        maxsiz = LIM(maxsiz, 50, 80);
        int nbm = maxsiz;
        int sizcu4 = maxsiz;

        if (oldsampling == true) {
            nbm = 55;
            sizcu4 = rtengine::min(sizcu30, nbm);//size of chroma values
        }

        Tppat[repref].maxhi = Wbhis[siza - 1].histnum;
        Tppat[repref].minhi = Wbhis[siza - nbm].histnum;

        if (settings->verbose) {
            printf("number total datas read=%i\n", ntot);
            printf("Others datas - ntr=%i sizcurr2ref=%i sizcu4=%i sizcu30=%i\n", ntr, sizcurr2ref, sizcu4, sizcu30);
            printf("Number max of data samples in last patch=%i\n", (int) Tppat[repref].maxhi);
            printf("Number of data samples in beginning patch =%i\n", (int) Tppat[repref].minhi);
        }

        chrom wbchro[sizcu4];
        float swpr = wpx + wpz + 1.f;
        float xwpr = wpx / swpr;//white point for tt in xy coordinates
        float ywpr = 1.f / swpr;

        if (oldsampling == true) {
            swpr = Txyz[repref].XX + Txyz[repref].ZZ + 1.f;
            xwpr = Txyz[repref].XX / swpr;//white point for tt in xy coordinates
            ywpr = 1.f / swpr;
        }

        if (settings->verbose) {
            printf("White Point XYZ x=%f y=%f z=%f\n", wpx, 1., wpz);
            printf("White Point xyY x=%f y=%f\n", xwpr, ywpr);
        }

        float estimchrom = 0.f;

        if (oldsampling == true) {
            for (int i = 0; i < sizcu4; ++i) { //take the max values
                histcurrref[i][repref] = Wbhis[siza - (i + 1)].histnum;
                xx_curref[i][repref] = xxx[Wbhis[siza - (i + 1)].index] / histcurrref[i][repref];
                yy_curref[i][repref] = yyy[Wbhis[siza - (i + 1)].index] / histcurrref[i][repref];
                YY_curref[i][repref] = YYY[Wbhis[siza - (i + 1)].index] / histcurrref[i][repref];
            }
            if (settings->verbose) {
                printf("Sizcu4=%i\n", sizcu4);
            }

            //estimate chromaticity for references
            for (int nh = 0; nh < sizcu4; ++nh) {
                const float chxy = std::sqrt(SQR(xx_curref[nh][repref] - xwpr) + SQR(yy_curref[nh][repref] - ywpr));
                wbchro[nh].chroxy_number = chxy * std::sqrt(histcurrref[nh][repref]);
                wbchro[nh].chroxy = std::sqrt(chxy);
                wbchro[nh].chrox = xx_curref[nh][repref];
                wbchro[nh].chroy = yy_curref[nh][repref];
                wbchro[nh].Y = YY_curref[nh][repref];
                wbchro[nh].index = nh;
                estimchrom += chxy;
            }

            estimchrom /= sizcu4;

            if (settings->verbose) {
                printf("estimchrom=%f\n", estimchrom);
            }

            const int maxval = 34;

            sizcurr2ref = rtengine::min(sizcurr2ref, maxval);    //keep about the biggest values,

            for (int i = 0; i < sizcurr2ref; ++i) {
                //is condition chroxy necessary ?
                if (wbchro[sizcu4 - (i + 1)].chrox > 0.1f && wbchro[sizcu4 - (i + 1)].chroy > 0.1f && wbchro[sizcu4 - (i + 1)].chroxy > 0.0f) { //suppress value too far from reference spectral
                    w++;
                    xx_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].chrox;
                    yy_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].chroy;
                    YY_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].Y;
                    nn_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].number;
                }
            }

            //calculate deltaE xx to find best values of spectrals data - limited to chroma values
            int maxnb = 3;


            float dEmean = 0.f;
            int ndEmean = 0;
            float maxhist = -1000.f;
            float minhist = 100000000.f;

            for (int nb = 1; nb <= maxnb; ++nb) {
                for (int i = 0; i < w; ++i) {
                    float mindeltaE = 100000.f;
                    int kN = 0;

                    for (int j = 0; j < Ncr ; j++) {
                        if (!good_spectral[j]) {
                            const float deltaE = SQR(xx_curref_reduc[i][repref] - reff_spect_xx_camera[j][repref]) + SQR(yy_curref_reduc[i][repref] - reff_spect_yy_camera[j][repref]);

                            if (deltaE < mindeltaE) {
                                mindeltaE = deltaE;
                                kN = j;
                            }
                        }
                    }

                    {//display in console for 5.9
                        float spectlimit = settings->itcwb_deltaspec;
                        float dE = sqrt(SQR(xx_curref_reduc[i][repref] - reff_spect_xx_camera[kN][repref]) + SQR(yy_curref_reduc[i][repref] - reff_spect_yy_camera[kN][repref]));
                        dEmean += dE;
                        ndEmean++;

                        if (nn_curref_reduc[i][repref] < minhist) {
                            minhist = nn_curref_reduc[i][repref];
                        }

                        if (nn_curref_reduc[i][repref] > maxhist) {
                            maxhist = nn_curref_reduc[i][repref];
                        }

                        if (settings->verbose) {
                            float xr = reff_spect_xx_camera[kN][repref];
                            float yr = reff_spect_yy_camera[kN][repref];
                            float Yr = reff_spect_Y_camera[kN][repref];
                            float X_r = (65535.f * (xr * Yr)) / yr;
                            float Z_r = (65535.f * (1.f - xr - yr) * Yr) / yr;
                            float Y_r = 65535.f * Yr;
                            float Lr, ar, br;
                            Color::XYZ2Lab(X_r, Y_r, Z_r, Lr, ar, br);//it make sense, because known spectral color

                            if (dE > spectlimit) {
                                printf("i=%i kn=%i REFLAB for info not used - not relevant Lr=%3.2f ar=%3.2f br=%3.2f \n", i,  kN, (double)(Lr / 327.68f), (double)(ar / 327.68f), (double)(br / 327.68f));
                                printf("IMAGE: kn=%i hist=%7.0f chro_num=%5.1f hue=%2.2f chro=%2.3f xx=%f yy=%f YY=%f\n", kN, (double) nn_curref_reduc[i][repref], (double) chronum_curref_reduc[i][repref], (double) hue_curref_reduc[i][repref], (double) chro_curref_reduc[i][repref], (double) xx_curref_reduc[i][repref], (double) yy_curref_reduc[i][repref], (double) YY_curref_reduc[i][repref]);
                                printf("kn=%i REfxy xxr=%f yyr=%f YYr=%f\n", kN, (double) reff_spect_xx_camera[kN][repref], (double) reff_spect_yy_camera[kN][repref], (double) reff_spect_Y_camera[kN][repref]);
                                printf("kn=%i DELTA delt=%f\n", kN, dE);
                                printf("....  \n");
                            }
                        }

                    }

                    good_spectral[kN] = true;//good spectral are spectral color that match color histogram xy
                }
            }
        } else {

            for (int i = 0; i < sizcu4; ++i) { //take the max values
                histcurrref[i][repref] = Wbhis[siza - (i + 1)].histnum;
                xx_curref[i][repref] = xxx[Wbhis[siza - (i + 1)].index] / histcurrref[i][repref];
                yy_curref[i][repref] = yyy[Wbhis[siza - (i + 1)].index] / histcurrref[i][repref];
                YY_curref[i][repref] = YYY[Wbhis[siza - (i + 1)].index] / histcurrref[i][repref];
            }

            int minsize = 20;
            int maxsize = maxsiz;
            bool isponderate = true; //to build patch ponderate

            bool isponder = true;//with true moving average
            float powponder = settings->itcwb_powponder;//not used today...
            powponder = LIM(powponder, 0.01f, 0.2f);
            float estimchrom = 0.f;

            for (int j = minsize; j < maxsize; ++j) {//20 empirical minimal value default to ensure a correlation
                if (!good_size[j]) {
                    float countchxynum = 0.f;
                    estimchrom = 0.f;
                    float xh = 0.f;
                    float yh = 0.f;
                    wbchro[j].hue =  0.f;
                    wbchro[j].chroxy_number = 0.f;
                    wbchro[j].number = 0.f;
                    wbchro[j].chroxy = 0.f;
                    wbchro[j].chrox = 0.f;
                    wbchro[j].chroy = 0.f;
                    wbchro[j].Y = 0.f;
                    wbchro[j].index = 0;
                    int ind1 = 1;
                    int ind2 = -1;
                    float chxy1 = 0.f;
                    float chxy2 = 0.f;
                    float chxynum1 = 0.f;
                    float chxynum2 = 0.f;

                    for (int nh = 0; nh < j; ++nh) {
                        ind1++;
                        ind2++;

                        if (ind1 < j && !isponderate) {
                            chxy1 = std::sqrt(SQR(xx_curref[ind1][repref] - xwpr) + SQR(yy_curref[ind1][repref] - ywpr));
                        }

                        if (ind2 <= 0 && !isponderate) {
                            chxy2 = std::sqrt(SQR(xx_curref[ind2][repref] - xwpr) + SQR(yy_curref[ind2][repref] - ywpr));
                        }

                        const float chxy = std::sqrt(SQR(xx_curref[nh][repref] - xwpr) + SQR(yy_curref[nh][repref] - ywpr));
                        xh += xx_curref[nh][repref] - xwpr;
                        yh += yy_curref[nh][repref] - ywpr;
                        wbchro[nh].hue =  fmodf(xatan2f(yy_curref[nh][repref] - ywpr, xx_curref[nh][repref] - xwpr), 2.f * RT_PI_F);
                        const float chxynum = wbchro[nh].chroxy_number = chxy * pow((double) histcurrref[nh][repref], 0.05);//sqrt was too big no convergence

                        //We can replace 0.05 by powponder
                        if (ind1 < j && isponderate) { //with issorted ponderate chroma
                            chxynum1 = chxy1 * pow((double) histcurrref[ind1][repref], 0.05);//0.05 to 0.1 allows convergence, near 1.5 betwween max and min value
                        }//We can replace 0.05 by powponder

                        if (ind2 < 0 && isponderate) {
                            chxynum2 = chxy2 * pow((double) histcurrref[ind2][repref], 0.05);//We can replace 0.05 by powponder
                        }

                        wbchro[nh].number = histcurrref[nh][repref];
                        wbchro[nh].chroxy = std::sqrt(chxy);
                        wbchro[nh].chrox = xx_curref[nh][repref];
                        wbchro[nh].chroy = yy_curref[nh][repref];
                        wbchro[nh].Y = YY_curref[nh][repref];
                        wbchro[nh].index = nh;

                        if (!isponderate) {
                            estimchrom += chxy;

                            if (isponder && !isponderate) {
                                estimchrom += chxy1;
                                estimchrom += chxy2;
                            }
                        }

                        if (isponderate) {
                            estimchrom += chxynum;

                            if (isponder) {
                                estimchrom += chxynum1;
                                estimchrom += chxynum2;
                            }

                            countchxynum += pow((double)histcurrref[nh][repref], 0.05);//no error, to take into account mean value //We can replace 0.05 by powponder
                        }
                    }

                    estim_hue[j][repref] = xatan2f(yh, xh);

                    if (isponder) {
                        estimchrom /= (j + 2 * (j - 1)); //extrem not taken
                    } else {
                        estimchrom /= j;
                    }

                    if (estimchrom < minchrom) {
                        minchrom = estimchrom;
                        Tppat[repref].minchroma = minchrom;
                        kmin = j;
                    }

                    Tppat[repref].minchroma = minchrom;
                }

                good_size[kmin] = true;
            }

            sizcu4 = kmin;

            int maxval = maxsiz;
            sizcurr2ref = rtengine::min(sizcurr2ref, maxval);    //keep about the biggest values,
            int index1 = 0;
            int index2 = sizcu4;
            int indn = index1;


            for (int i = index1; i < index2; ++i) {
                if (wbchro[sizcu4 - (i + 1)].number < 400.f) { //remove too low numbers datas about an area 60*60 pixels or reparted
                    indn++;
                }
            }

            Tppat[repref].minhi = (float) rtengine::max((int) wbchro[sizcu4 - (indn + 1)].number, (int) Tppat[repref].minhi);

            if (settings->verbose) {
                printf("Index1=%i index2=%i \n", indn, index2);
            }

            if (settings->verbose) {
                printf("Info2 - patch estimation of wp displacement (before):j=%i repref=%i real=%i Tppat=%f chrom=%f hue=%f\n", kmin, repref, index2 - indn, (double) Tppat[repref].minchroma, (double) minchrom, (double) estim_hue[kmin][repref]);
            };

            float limexclu = 0.96f;//to avoid highlight in some rare cases (sky...)


            for (int i = indn; i < index2; ++i) {
                //improvment to limit high Y values wbchro[sizcu4 - (i + 1)].Y < 0.96  0.96 arbitrary high value, maybe 0.9 or 0.98...or 1.0
                if (wbchro[sizcu4 - (i + 1)].chrox > limx && wbchro[sizcu4 - (i + 1)].chroy > limy  && wbchro[sizcu4 - (i + 1)].Y < limexclu) { //remove value too far from reference spectral
                    w++;// w number of real tests
                    xx_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].chrox;
                    yy_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].chroy;
                    YY_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].Y;
                    chronum_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].chroxy_number;
                    nn_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].number;
                    hue_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].hue;
                    chro_curref_reduc[w][repref] = wbchro[sizcu4 - (i + 1)].chroxy;
                }
            }

            if (settings->verbose) {
                printf("Number of real tests=%i\n", w);
            }

            int maxnb = 1; //since 8 april 2023


            float dEmean = 0.f;
            int ndEmean = 0;
            maxhist = -1000.f;
            minhist = 100000000.f;

            for (int nb = 1; nb <= maxnb; ++nb) { //1 is good, but 2 3 or 4 help to find more spectral values
                for (int i = 0; i < w; ++i) {
                    float mindeltaE = 100000.f;
                    int kN = 0;

                    for (int j = 0; j < Ncr ; j++) {
                        if (!good_spectral[j]) {
                            const float deltaE = SQR(xx_curref_reduc[i][repref] - reff_spect_xx_camera[j][repref]) + SQR(yy_curref_reduc[i][repref] - reff_spect_yy_camera[j][repref]);

                            if (deltaE < mindeltaE) {
                                mindeltaE = deltaE;
                                kN = j;
                            }
                        }
                    }
                    //display in console
                    float spectlimit = settings->itcwb_deltaspec;
                    float dE = sqrt(SQR(xx_curref_reduc[i][repref] - reff_spect_xx_camera[kN][repref]) + SQR(yy_curref_reduc[i][repref] - reff_spect_yy_camera[kN][repref]));
                    dEmean += dE;
                    ndEmean++;

                    if (nn_curref_reduc[i][repref] < minhist) {
                        minhist = nn_curref_reduc[i][repref];
                    }

                    if (nn_curref_reduc[i][repref] > maxhist) {
                        maxhist = nn_curref_reduc[i][repref];
                    }

                    if (settings->verbose) {
                        float xr = reff_spect_xx_camera[kN][repref];
                        float yr = reff_spect_yy_camera[kN][repref];
                        float Yr = reff_spect_Y_camera[kN][repref];
                        float X_r = (65535.f * (xr * Yr)) / yr;
                        float Z_r = (65535.f * (1.f - xr - yr) * Yr) / yr;
                        float Y_r = 65535.f * Yr;
                        float Lr, ar, br;
                        Color::XYZ2Lab(X_r, Y_r, Z_r, Lr, ar, br);//it make sense, because known spectral color

                        if (dE > spectlimit) {
                            printf("i=%i kn=%i REFLAB for info not used - not relevant Lr=%3.2f ar=%3.2f br=%3.2f \n", i,  kN, (double)(Lr / 327.68f), (double)(ar / 327.68f), (double)(br / 327.68f));
                            printf("IMAGE: kn=%i hist=%7.0f chro_num=%5.1f hue=%2.2f chro=%2.3f xx=%f yy=%f YY=%f\n", kN, (double) nn_curref_reduc[i][repref], (double) chronum_curref_reduc[i][repref], (double) hue_curref_reduc[i][repref], (double) chro_curref_reduc[i][repref], (double) xx_curref_reduc[i][repref], (double) yy_curref_reduc[i][repref], (double) YY_curref_reduc[i][repref]);
                            printf("kn=%i REfxy xxr=%f yyr=%f YYr=%f\n", kN, (double) reff_spect_xx_camera[kN][repref], (double) reff_spect_yy_camera[kN][repref], (double) reff_spect_Y_camera[kN][repref]);
                            printf("kn=%i DELTA delt=%f\n", kN, dE);
                            printf("....  \n");
                        }
                    }


                    good_spectral[kN] = true;//good spectral are spectral color that match color histogram xy
                }

                if (ndEmean == 0) {
                    ndEmean = 2;
                }

                Tppat[repref].delt_E = dEmean / ndEmean;
                delta = Tppat[repref].delt_E;
                Tppat[repref].maxhi = maxhist;
                Tppat[repref].minhi = minhist;

                if (settings->verbose  && !oldsampling) {
                    printf("Patch Mean - Repref=%i deltaE=%f minhisto=%6.0f maxhisto=%7.0f \n", repref, (double) dEmean / ndEmean, (double) minhist, (double) maxhist);
                }
            }
        }

        // reuse some buffers
        array2D<float>& R_curref_reduc = xx_curref_reduc;
        array2D<float>& G_curref_reduc = yy_curref_reduc;
        array2D<float>& B_curref_reduc = YY_curref_reduc;

        //reconvert to RGB for "reduction"
        for (int i = 0; i < w; i++) {
            const float X = 65535.f * xx_curref_reduc[i][repref] * YY_curref_reduc[i][repref] / yy_curref_reduc[i][repref];
            const float Y = 65535.f * YY_curref_reduc[i][repref];
            const float Z = 65535.f * (1.f - xx_curref_reduc[i][repref] - yy_curref_reduc[i][repref]) * YY_curref_reduc[i][repref] / yy_curref_reduc[i][repref];
            float r, g, b;
            Color::xyz2rgb(X, Y, Z, r, g, b, iwb);
            R_curref_reduc[i][repref] = r / rmm[repref];
            G_curref_reduc[i][repref] = g / gmm[repref];
            B_curref_reduc[i][repref] = b / bmm[repref];
        }

        t4.set();

        if (settings->verbose) {
            printf("Third: from second to find patch: %d nsec\n", t4.etime(t3));
        }


//end first part

        //Now begin real calculations
        separated = false;
        ttbeg = 0;
        ttend = N_t;

        ttbeg = std::max(repref - 11, 0);//enough in all cases > dgoodref
        ttend = std::min(repref + 11, N_t);

        if (oldsampling == true) {
            ttbeg = 0;
            ttend = N_t;
        }

        //recalculate histogram with good values and not estimated
        double wpx1 = 0.;
        double wpz1 = 0.;

        ColorTemp::tempxy(separated, repref, Tx, Ty, Tz, Ta, Tb, TL, TX, TY, TZ, wbpar, ttbeg, ttend, wpx1, wpz1, WPX, WPZ); //calculate chroma xy (xyY) for Z known colors on under 90 illuminants
        //calculate x y Y
        int sizcurr = siza;//choice of number of correlate colors in image
        array2D<float> xxyycurr_reduc(N_t, 2 * sizcurr);
        array2D<float> reff_spect_xxyy(N_t, 2 * Ncr + 2);
        array2D<float> reff_spect_xxyy_prov(N_t, 2 * Ncr + 2);
        t5.set();

        if (settings->verbose) {
            printf("Fourth: from third recalculate spectral: %d nsec\n",  t5.etime(t4));
        }

        float minstud = 100000.f;
        int goodref = 1;

//calculate  x y z for each pixel with multiplier rmm gmm bmm

        for (int tt = ttbeg; tt < ttend; ++tt) {//N_t
            for (int i = 0; i < w; ++i) {
                float unused;

                const float RR = rmm[tt] * R_curref_reduc[i][repref];
                const float GG = gmm[tt] * G_curref_reduc[i][repref];
                const float BB = bmm[tt] * B_curref_reduc[i][repref];
                Color::rgbxyY(RR, GG, BB, xxyycurr_reduc[2 * i][tt], xxyycurr_reduc[2 * i + 1][tt], unused, wb);
            }

            for (int j = 0; j < Ncr ; ++j) {
                reff_spect_xxyy_prov[2 * j][tt] = std::max(Tx[j][tt] / (Tx[j][tt] + Ty[j][tt] +  Tz[j][tt]), 0.01f); // x from xyY
                reff_spect_xxyy_prov[2 * j + 1][tt] = std::max(Ty[j][tt] / (Tx[j][tt] + Ty[j][tt] +  Tz[j][tt]), 0.01f); // y from xyY
            }

            int kk = -1;

            for (int i = 0; i < Ncr ; ++i) {
                if (good_spectral[i]) {
                    kk++;
                    //we calculate now absolute chroma for each spectral color
                    reff_spect_xxyy[2 * kk][tt] = reff_spect_xxyy_prov[2 * i][tt];
                    reff_spect_xxyy[2 * kk + 1][tt] = reff_spect_xxyy_prov[2 * i + 1][tt];
                }
            }

            const float abstud = std::fabs(studentXY(xxyycurr_reduc, reff_spect_xxyy, 2 * w, 2 * (kk + 1), tt));

            if (abstud < minstud) {  // find the minimum Student
                minstud = abstud;
                goodref = tt;
            }
        }

        t6.set();

        if (settings->verbose) {
            printf("Fifth: from fourth to find first correlation: %d nsec\n",  t6.etime(t5));
        }

        {
            //always used if extra = true because I made this choice, brings better results
            struct Tempgreen {
                float student;
                int tempref;
                int greenref;
                bool operator()(const Tempgreen& ltg, const Tempgreen& rtg)
                {
                    return ltg.student < rtg.student;
                }
            };
            Tempgreen  Tgstud[N_g];

            for (int i = 0; i < N_g; ++i) {//init variables with
                Tgstud[i].student = 1000.f;//max value to initialize

                if (!oldsampling) {
                    Tgstud[i].tempref = 80;//5002K position in the list
                } else {
                    Tgstud[i].tempref = 57;//5002K position in the list
                }

                Tgstud[i].greenref = 55;// 1.f position in the list
            }

            int newdelta = 1.8f * 4; // wbpar.itcwb_delta - default = 4;
            int dgoodref = rtengine::LIM(newdelta, 1, 10); // 1.8 increase delta temp scan

            if (oldsampling == true) {
                dgoodref = 2;
            }

            const int scantempbeg = rtengine::max(goodref - (dgoodref + 1), 1);
            const int scantempend = rtengine::min(goodref + dgoodref, N_t - 1);
            int goodrefgr = 1;

            for (int gr = Rangegreenused.begin; gr < Rangegreenused.end; ++gr) {
                float minstudgr = 100000.f;
                goodrefgr = 1;

                for (int tt = scantempbeg; tt < scantempend; ++tt) {
                    double r, g, b;

                    if (!oldsampling) {
                        ColorTemp(Txyz[tt].Tem, gree[gr].green, 1., "Custom", wbpar.observer).getMultipliers(r, g, b);
                    } else {
                        ColorTemp(Txyzs[tt].Tem, gree[gr].green, 1., "Custom", wbpar.observer).getMultipliers(r, g, b);
                    }

                    float rm = imatrices.cam_rgb[0][0] * r + imatrices.cam_rgb[0][1] * g + imatrices.cam_rgb[0][2] * b;
                    float gm = imatrices.cam_rgb[1][0] * r + imatrices.cam_rgb[1][1] * g + imatrices.cam_rgb[1][2] * b;
                    float bm = imatrices.cam_rgb[2][0] * r + imatrices.cam_rgb[2][1] * g + imatrices.cam_rgb[2][2] * b;
                    //recalculate Multipliers now with good range of temp and green

                    const float new_pre_mul[4] = { ri->get_pre_mul(0) / rm, ri->get_pre_mul(1) / gm, ri->get_pre_mul(2) / bm, ri->get_pre_mul(3) / gm };
                    float new_scale_mul[4];
                    const float gain = calculate_scale_mul(new_scale_mul, new_pre_mul, c_white, cblacksom, isMono, ri->get_colors());

                    rm = new_scale_mul[0] / scale_mul[0] * gain;
                    gm = new_scale_mul[1] / scale_mul[1] * gain;
                    bm = new_scale_mul[2] / scale_mul[2] * gain;
                    rmm[tt] = rm / gm;
                    gmm[tt] = 1.f;
                    bmm[tt] = bm / gm;
                }


                for (int tt = scantempbeg; tt < scantempend; ++tt) {//N_t
                    for (int i = 0; i < w; ++i) {
                        float unused;

                        const float RR = rmm[tt] * R_curref_reduc[i][repref];
                        const float GG = gmm[tt] * G_curref_reduc[i][repref];
                        const float BB = bmm[tt] * B_curref_reduc[i][repref];
                        Color::rgbxyY(RR, GG, BB, xxyycurr_reduc[2 * i][tt], xxyycurr_reduc[2 * i + 1][tt], unused, wb);
                    }

                    //recalculate xy spectral now with good range of temp and green

                    for (int j = 0; j < Ncr ; ++j) {
                        reff_spect_xxyy_prov[2 * j][tt] = std::max(Tx[j][tt] / (Tx[j][tt] + Ty[j][tt] +  Tz[j][tt]), 0.01f); // x from xyY
                        reff_spect_xxyy_prov[2 * j + 1][tt] = std::max(Ty[j][tt] / (Tx[j][tt] + Ty[j][tt] +  Tz[j][tt]), 0.01f); // y from xyY
                    }

                    int kkg = -1;

                    for (int i = 0; i < Ncr ; ++i) {
                        if (good_spectral[i]) {
                            kkg++;
                            reff_spect_xxyy[2 * kkg][tt] = reff_spect_xxyy_prov[2 * i][tt];
                            reff_spect_xxyy[2 * kkg + 1][tt] = reff_spect_xxyy_prov[2 * i + 1][tt];
                        }
                    }

                    //now we have good spectral data
                    //calculate student correlation
                    const float abstudgr = std::fabs(studentXY(xxyycurr_reduc, reff_spect_xxyy, 2 * w, 2 * (kkg + 1), tt));

                    if (abstudgr < minstudgr) {  // find the minimum Student
                        minstudgr = abstudgr;
                        goodrefgr = tt;
                    }

                    //found the values
                    Tgstud[gr].tempref = goodrefgr;
                    Tgstud[gr].greenref = gr;
                    Tgstud[gr].student = minstudgr;

                }
            }

            t7.set();

            if (settings->verbose) {
                printf("Sixth: from fifth to find patch in extra mode: %d nsec\n", t7.etime(t6));
            }

            float estimchromf = 0.f;
            float estimhuef = 0.f;
            float xhf = 0.f;
            float yhf = 0.f;

            const float swprf = WPX[goodrefgr] + WPZ[goodrefgr] + 1.f;
            const float xwprf = WPX[goodrefgr] / swpr;//white point for tt in xy coordinates

            const float ywprf = 1.f / swprf;

            for (int nh = 0; nh < w; ++nh) {
                const float chxy = std::sqrt(SQR(xxyycurr_reduc[2 * nh][goodrefgr] - xwprf) + SQR(xxyycurr_reduc[2 * nh + 1][goodrefgr] - ywprf));
                xhf += xxyycurr_reduc[2 * nh][goodrefgr] - xwprf;
                yhf += xxyycurr_reduc[2 * nh + 1][goodrefgr] - ywprf;
                estimchromf += chxy;
            }

            estimhuef = xatan2f(yhf, xhf);
            estimchromf /= w;

            if (settings->verbose) {
                printf("New white point calculated patch for information : xwprf=%f ywprf=%f\n", (double) xwprf, (double) ywprf);
                printf("Info - patch estimation of white-point displacement: chrom=%f hue=%f\n", (double) estimchromf, (double) estimhuef);
            }


            std::sort(Tgstud, Tgstud + N_g, Tgstud[0]);

            if (!oldsampling) {

                // now search the value of green the nearest of 1 with a good student value, I think it is a good choice, perhaps no...
                // I take the first values
                // I admit a symetrie in green coefiicient for rgb multiplier...probably not exactly true
                // perhaps we can used a Snedecor test ? but why...at least we have confidence interval > 90%
                int greengood = 55;

                int maxkgood = 3;//default 3 - we can change ...to test 2, 4, 5, 6. High values perhaps less good student, but it is a compromise...
                maxkgood = rtengine::LIM(maxkgood, 1, 6);// 2 6

                if (oldsampling == true) {
                    maxkgood = 3; // force to 3 with old low sampling
                }

                int mingood = std::min(std::fabs(Tgstud[0].greenref - 55), std::fabs(Tgstud[1].greenref - 55));

                for (int k = 0; k < maxkgood; ++k) {
                    mingood = std::min(std::fabs(mingood), std::fabs(Tgstud[k].greenref - 55));
                }

                for (int k = 0; k < maxkgood ; ++k) {
                    if (mingood == fabs(Tgstud[k].greenref - 55)) {
                        greengood = Tgstud[k].greenref ;
                        goodref = Tgstud[k].tempref;
                        studgood = Tgstud[k].student;
                    }
                }

                if (settings->verbose) {
                    printf("Green comparison=%f\n", keepgreen);
                    printf("Rangegreen begin=%i  Rangegreen end=%i\n", Rangegreenused.begin, Rangegreenused.end);
                    printf("scantemp begin=%i scantemp end=%i\n", scantempbeg, scantempend);
                    printf("Student_0=%f Student_k= %f\n", Tgstud[0].student, Tgstud[maxkgood - 1].student);
                    printf("mingood=%i greeng=%i goodref=%i stud=%f\n", mingood, greengood, goodref, (double) studgood);
                }

                tempitc = Txyz[goodref].Tem;

                greenitc = gree[greengood].green;

                int greencam = 55;

                for (int gg = 0; gg < N_g; gg++) {
                    if (gree[gg].green > keepgreen) {
                        greencam = gg;//show the green
                        break;
                    }
                }

                bool greenex = false;

                if ((keepgreen > 0.92 && keepgreen < 1.23)) {
                    if (abs(greengood - greencam) > 5) {
                        double ag = 0.;
                        double gcal = gree[greengood].green;
                        ag = 0.89 * (gcal - keepgreen);
                        greenitc = gcal - ag;
                        greenex = true;

                        if (settings->verbose) {
                            printf("green correction_1=%f \n", ag);
                        }
                    } else {
                        double ag = 0.;
                        double gcal = gree[greengood].green;

                        if (keepgreen > 1.09) {
                            ag = 0.10 * (gcal - keepgreen) * abs(greengood - greencam);
                        } else {
                            ag = 0.16 * (gcal - keepgreen) * abs(greengood - greencam);
                        }

                        greenitc = gcal - ag;
                        greenex = true;

                        if (settings->verbose) {
                            printf("green correction_0=%f \n", ag);
                        }

                    }
                }

                if (((keepgreen >= 0.952 && keepgreen < 1.25) && greengood > 55) && !greenex) {
                    double ag = 0.;
                    double gcal = gree[greengood].green;//empirical  correction when green suspicious
                    ag = 0.96 * (gcal - keepgreen);
                    greenitc = gcal - ag;
                    greenex = true;

                    if (settings->verbose) {
                        printf("green correction_2=%f \n", ag);
                    }
                }

                if (((greengood > 41 &&  keepgreen < 0.7)  || (greengood > 46 &&  keepgreen < 0.952)) && !greenex) {
                    double ag = 0.;
                    double gcal = gree[greengood].green;
                    ag = 0.95 * (gcal - keepgreen);//empirical  correction when green low - to improve

                    if (purp == false) {
                        ag -= 0.12;
                    }

                    if (settings->verbose) {
                        printf("green correction_3=%f \n", ag);
                    }

                    greenitc = gcal - ag;
                }
            } else {//oldsampling
                int greengood;
                int greengoodprov;
                int goodrefprov;
                float studprov;
                const int goodref0 = Tgstud[0].tempref;
                const int greengood0 = Tgstud[0].greenref - 55;//55 green = 1
                const float stud0 = Tgstud[0].student;
                const int goodref1 = Tgstud[1].tempref;
                const float stud1 = Tgstud[1].student;
                const int greengood1 = Tgstud[1].greenref - 55;
                const int goodref2 = Tgstud[2].tempref;
                const int greengood2 = Tgstud[2].greenref - 55;
                const float stud2 = Tgstud[2].student;

                if (std::fabs(greengood2) < std::fabs(greengood1)) {
                    greengoodprov = greengood2;
                    goodrefprov = goodref2;
                    studprov = stud2;
                } else {
                    greengoodprov = greengood1;
                    goodrefprov = goodref1;
                    studprov = stud1;

                }

                if (std::fabs(greengoodprov) < std::fabs(greengood0)) {
                    goodref = goodrefprov;
                    greengood = greengoodprov + 55;
                    studgood = studprov;

                } else {
                    goodref = goodref0;
                    greengood = greengood0 + 55;
                    studgood = stud0;
                }

                tempitc = Txyz[goodref].Tem;
                greenitc = gree[greengood].green;

                if (estimchrom < 0.025f) {
                    float ac = -2.40f * estimchrom + 0.06f;//small empirical  correction, maximum 0.06 if chroma=0 for all image, currently for very low chroma +0.02
                    greenitc += ac;
                }

                itciterate = false;

            }
        }

        avg_rm = 10000.f * rmm[goodref];
        avg_gm = 10000.f * gmm[goodref];
        avg_bm = 10000.f * bmm[goodref];

        //now we have temp green and student
        if (!oldsampling) {
            if (lastitc  && nocam == 0 && wbpar.itcwb_alg == false) { //try to find if another tempref
                if ((tempitc < 4000.f || tempitc > 6000.f) || extra == true) {
                    optitc[nbitc].stud = studgood;
                    optitc[nbitc].minc = Tppat[repref].minchroma;
                    optitc[nbitc].titc = tempitc;
                    optitc[nbitc].gritc = greenitc;
                    optitc[nbitc].tempre = tempref;
                    optitc[nbitc].greenre = greenref;
                    optitc[nbitc].drea = dread;
                    optitc[nbitc].kmi = kmin;
                    optitc[nbitc].minhis = Tppat[repref].minhi;
                    optitc[nbitc].maxhis = Tppat[repref].maxhi;
                    optitc[nbitc].avg_r = avg_rm;
                    optitc[nbitc].avg_g = avg_gm;
                    optitc[nbitc].avg_b = avg_bm;
                    optitc[nbitc].delt = Tppat[repref].delt_E;

                    nbitc++;

                    if (tempitc < 4000.f) {//change the second temp to be near of the first one
                        if (tempitc < 2800.f  && kcam == 1) {
                            tempitc += 151.f;
                        } else if (tempitc >= 2800.f  && kcam == 1) {
                            tempitc -= 149.f;
                        } else {
                            tempitc += 201.f;
                        }
                    } else {
                        if (tempitc < 8000.f) {
                            tempitc = 4197.f + 0.1255f * tempitc;
                        } else {
                            tempitc = 5200.f * (1.f * (tempitc / 8000.f));
                        }
                    }

                    tempref = tempitc * (1. + wbpar.tempBias);

                    optitc[nbitc].stud = studgood;
                    optitc[nbitc].minc =  Tppat[repref].minchroma;
                    optitc[nbitc].titc = tempitc;
                    optitc[nbitc].gritc = greenitc;
                    optitc[nbitc].tempre = tempref;
                    optitc[nbitc].greenre = greenref;
                    optitc[nbitc].drea = dread;
                    optitc[nbitc].kmi = kmin;
                    optitc[nbitc].minhis = Tppat[repref].minhi;
                    optitc[nbitc].maxhis = Tppat[repref].maxhi;
                    optitc[nbitc].avg_r = avg_rm;
                    optitc[nbitc].avg_g = avg_gm;
                    optitc[nbitc].avg_b = avg_bm;
                    optitc[nbitc].delt = Tppat[repref].delt_E;
                    lastitc = false;

                } else if ((tempitc >= 4000.f && tempitc <= 6000.f) || extra == true) {
                    optitc[nbitc].stud = studgood;
                    optitc[nbitc].minc = Tppat[repref].minchroma;
                    optitc[nbitc].titc = tempitc;
                    optitc[nbitc].gritc = greenitc;
                    optitc[nbitc].tempre = tempref;
                    optitc[nbitc].greenre = greenref;
                    optitc[nbitc].drea = dread;
                    optitc[nbitc].kmi = kmin;
                    optitc[nbitc].minhis = Tppat[repref].minhi;
                    optitc[nbitc].maxhis = Tppat[repref].maxhi;
                    optitc[nbitc].avg_r = avg_rm;
                    optitc[nbitc].avg_g = avg_gm;
                    optitc[nbitc].avg_b = avg_bm;
                    optitc[nbitc].delt = Tppat[repref].delt_E;

                    nbitc++;

                    if (tempitc < 5000.f) {//change the second temp to be near of the first one
                        tempitc += 105.f;
                    } else {
                        tempitc -= 105.f;
                    }

                    tempref = tempitc * (1. + wbpar.tempBias);

                    optitc[nbitc].stud = studgood;
                    optitc[nbitc].minc =  Tppat[repref].minchroma;
                    optitc[nbitc].titc = tempitc;
                    optitc[nbitc].gritc = greenitc;
                    optitc[nbitc].tempre = tempref;
                    optitc[nbitc].greenre = greenref;
                    optitc[nbitc].drea = dread;
                    optitc[nbitc].kmi = kmin;
                    optitc[nbitc].minhis = Tppat[repref].minhi;
                    optitc[nbitc].maxhis = Tppat[repref].maxhi;
                    optitc[nbitc].avg_r = avg_rm;
                    optitc[nbitc].avg_g = avg_gm;
                    optitc[nbitc].avg_b = avg_bm;
                    optitc[nbitc].delt = Tppat[repref].delt_E;
                    lastitc = false;

                }
            } else if (nocam > 0 && wbpar.itcwb_alg == false) {
                optitc[nbitc].stud = studgood;
                optitc[nbitc].minc = Tppat[repref].minchroma;
                optitc[nbitc].titc = tempitc;
                optitc[nbitc].gritc = greenitc;
                optitc[nbitc].tempre = tempref;
                optitc[nbitc].greenre = greenref;
                optitc[nbitc].drea = dread;
                optitc[nbitc].kmi = kmin;
                optitc[nbitc].minhis = Tppat[repref].minhi;
                optitc[nbitc].maxhis = Tppat[repref].maxhi;
                optitc[nbitc].avg_r = avg_rm;
                optitc[nbitc].avg_g = avg_gm;
                optitc[nbitc].avg_b = avg_bm;
                optitc[nbitc].delt = Tppat[repref].delt_E;

                nbitc++;

                if (nocam == 1) { //new tempitc empirical values to refine
                    tempitc -= 199.f;
                } else if (nocam == 2) {
                    tempitc += 201.f;
                } else if (nocam == 3) {
                    tempitc -= 199.f;
                } else if (nocam == 4) {
                    tempitc += 201.f;
                } else if (nocam == 5) {
                    tempitc += 299.f;
                } else if (nocam == 6) {
                    tempitc += 201.f;
                } else if (nocam == 7) {
                    tempitc += 299.f;
                } else if (nocam == 8) {
                    tempitc += 500.f;
                } else if (nocam == 9) {
                    tempitc += 199.f;
                } else if (nocam == 10) {
                    tempitc += 199.f;
                }

                nocam = 0;
                tempref = tempitc * (1. + wbpar.tempBias);

                optitc[nbitc].stud = studgood;
                optitc[nbitc].minc =  Tppat[repref].minchroma;
                optitc[nbitc].titc = tempitc;
                optitc[nbitc].gritc = greenitc;
                optitc[nbitc].tempre = tempref;
                optitc[nbitc].greenre = greenref;
                optitc[nbitc].drea = dread;
                optitc[nbitc].kmi = kmin;
                optitc[nbitc].minhis = Tppat[repref].minhi;
                optitc[nbitc].maxhis = Tppat[repref].maxhi;
                optitc[nbitc].avg_r = avg_rm;
                optitc[nbitc].avg_g = avg_gm;
                optitc[nbitc].avg_b = avg_bm;
                optitc[nbitc].delt = Tppat[repref].delt_E;
                lastitc = false;
            } else {
                optitc[nbitc].stud = studgood;
                optitc[nbitc].minc =  Tppat[repref].minchroma;
                optitc[nbitc].titc = tempitc;
                optitc[nbitc].gritc = greenitc;
                optitc[nbitc].tempre = tempref;
                optitc[nbitc].greenre = greenref;
                optitc[nbitc].drea = dread;
                optitc[nbitc].kmi = kmin;
                optitc[nbitc].minhis = Tppat[repref].minhi;
                optitc[nbitc].maxhis = Tppat[repref].maxhi;
                optitc[nbitc].avg_r = avg_rm;
                optitc[nbitc].avg_g = avg_gm;
                optitc[nbitc].avg_b = avg_bm;
                optitc[nbitc].delt = Tppat[repref].delt_E;

                lastitc = false;
                itciterate = false;
            }


            if (optitc[1].minc > 0.f) {
                choiceitc = 1;
                temp0 = optitc[0].titc;
            } else {
                choiceitc = 0;
                temp0 = 0.f;
            }
        }//end loop

        if (!oldsampling) {

            if (settings->verbose) {
                for (int d = 0; d < 2; d++) {
                    printf("n=%i nbitc=%i stu=%f minc=%f tempitc=%f greenitc=%f deltaE=%f choiceitc=%i\n", d, nbitc, (double) optitc[d].stud, (double) optitc[d].minc, (double) optitc[d].titc, (double) optitc[d].gritc, (double) optitc[d].delt, choiceitc);
                }
            }

            if ((nbitc == 1 && choiceitc == 1) && wbpar.itcwb_alg == false && oldsampling == false) {
                bia = 2;

                if ((std::max(optitc[1].stud, 0.0004f) * optitc[1].delt < std::max(optitc[0].stud, 0.0004f) * optitc[0].delt) && wbpar.itcwb_alg == false) {
                    bia = 3;
                } else {
                    bia = 2;
                }

                studgood = optitc[choiceitc].stud;
                minchrom = optitc[choiceitc].minc;
                tempitc = optitc[choiceitc].titc;
                greenitc = optitc[choiceitc].gritc;
                tempref = optitc[choiceitc].tempre;
                greenref = optitc[choiceitc].greenre;
                dread = optitc[choiceitc].drea;
                kmin = optitc[choiceitc].kmi;
                minhist = optitc[choiceitc].minhis;
                maxhist = optitc[choiceitc].maxhis;
                avg_rm = optitc[choiceitc].avg_r;
                avg_gm = optitc[choiceitc].avg_g;
                avg_bm = optitc[choiceitc].avg_b;
            } else if (!oldsampling) {
                studgood = optitc[0].stud;
                minchrom = optitc[0].minc;
                tempitc = optitc[0].titc;
                greenitc = optitc[0].gritc;
                tempref = optitc[0].tempre;
                greenref = optitc[0].greenre;
                dread = optitc[0].drea;
                kmin = optitc[0].kmi;
                minhist = optitc[0].minhis;
                maxhist = optitc[0].maxhis;
                avg_rm = optitc[0].avg_r;
                avg_gm = optitc[0].avg_g;
                avg_bm = optitc[0].avg_b;
            }
        }
    }

    t8.set();

    if (settings->verbose) {
        printf("Seventh: from sixth to end: %d nsec\n",  t8.etime(t7));
    }

}

void RawImageSource::WBauto(bool extra, double & tempref, double & greenref, array2D<float> &redloc, array2D<float> &greenloc, array2D<float> &blueloc, int bfw, int bfh, double & avg_rm, double & avg_gm, double & avg_bm, double & tempitc, double & greenitc, float &temp0, float &delta, int &bia,  int &dread, int &kcam, int &nocam, float & studgood, float &minchrom, int &kmin, float &minhist, float &maxhist, bool & twotimes, const WBParams & wbpar, int begx, int begy, int yEn, int xEn, int cx, int cy, const ColorManagementParams & cmp, const RAWParams & raw, const ToneCurveParams &hrp)
{
//    BENCHFUN
    //auto white balance
    //put green (tint) in reasonable limits for an Daylight illuminant
    // avoid too bi or too low values
    if (wbpar.method == "autitcgreen") {
        // bool extra = true;

        if (greenref > 0.5 && greenref < 1.3) {// 0.5 and 1.3 arbitraties values
            greenitc = greenref;

        } else {
            greenitc = 1.;
            //     extra = true;
        }

        tempitc = 5000.;
        ItcWB(extra, tempref, greenref, tempitc, greenitc, temp0, delta, bia, dread, kcam, nocam, studgood, minchrom, kmin, minhist, maxhist, redloc, greenloc, blueloc, bfw, bfh, avg_rm, avg_gm, avg_bm, cmp, raw, wbpar, hrp);
    }
}

void RawImageSource::getrgbloc(int begx, int begy, int yEn, int xEn, int cx, int cy, int bf_h, int bf_w, const WBParams & wbpar)
{
//    BENCHFUN
    //used by auto WB local to calculate red, green, blue in local region

    int precision = 3;//must be 3 5 or 9
    bool oldsampling = wbpar.itcwb_sampling;
    if (oldsampling == true) {
        precision = 5;
    }

    const int bfw = W / precision + ((W % precision) > 0 ? 1 : 0);// 5 arbitrary value can be change to 3 or 9 ;
    const int bfh = H / precision + ((H % precision) > 0 ? 1 : 0);

    greenloc(bfw, bfh);

    redloc(bfw, bfh);

    blueloc(bfw, bfh);

    double avgL = 0.0;
    //center data on normal values
    int nn = 0;

#ifdef _OPENMP
    #pragma omp parallel for reduction(+:avgL, nn)
#endif

    for (int i = 0; i < H; i ++) {
        for (int j = 0; j < W; j++) {
            const float LL = 0.299f * red[i][j] + 0.587f * green[i][j] + 0.114f * blue[i][j];
            avgL += static_cast<double>(LL);
            nn++;
        }
    }

    avgL /= nn;

    double vari = 0.f;
    int mm = 0;

#ifdef _OPENMP
    #pragma omp parallel for reduction(+:vari, mm)
#endif

    for (int i = 0; i < H; i++)
        for (int j = 0; j < W; j++) {
            const float LL = 0.299f * red[i][j] + 0.587f * green[i][j] + 0.114f * blue[i][j];
            vari += SQR(LL - avgL);
            mm++;
        }

    const float sig = std::sqrt(vari / mm);
    float multip = 60000.f / (avgL + 2.f * sig);
    if(std::isnan(multip)) {//if very bad datas with avgl and sig
        multip = 1.f;
    }
#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int i = 0; i < bfh; ++i) {
        const int ii = i * precision;

        if (ii < H) {
            for (int j = 0, jj = 0; j < bfw; ++j, jj += precision) {//isnan and <0 and > 65535 in case of
                redloc[i][j] = red[ii][jj] * multip;
                greenloc[i][j] = green[ii][jj] * multip;
                blueloc[i][j] = blue[ii][jj] * multip;
            }
        }
    }
}

void RawImageSource::getAutoWBMultipliersitc(bool extra, double & tempref, double & greenref, double & tempitc, double & greenitc, float &temp0, float &delta,  int &bia, int &dread, int &kcam, int &nocam, float &studgood, float &minchrom, int &kmin,  float &minhist, float &maxhist, int begx, int begy, int yEn, int xEn, int cx, int cy, int bf_h, int bf_w, double & rm, double & gm, double & bm, const WBParams & wbpar, const ColorManagementParams & cmp, const RAWParams & raw, const ToneCurveParams &hrp)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

//    BENCHFUN
    constexpr double clipHigh = 64000.0;

    if (ri->get_colors() == 1) {
        rm = gm = bm = 1;
        return;
    }

    double avg_r = 0;
    double avg_g = 0;
    double avg_b = 0;
    int rn = 0, gn = 0, bn = 0;
    double avg_rm, avg_gm, avg_bm;

    if (wbpar.method == "autold") {
        if (fuji) {
            for (int i = 32; i < H - 32; i++) {
                int fw = ri->get_FujiWidth();
                int start = ABS(fw - i) + 32;
                int end = min(H + W - fw - i, fw + i) - 32;

                for (int j = start; j < end; j++) {
                    if (ri->getSensorType() != ST_BAYER) {
                        double dr = CLIP(initialGain * (rawData[i][3 * j]));
                        double dg = CLIP(initialGain * (rawData[i][3 * j + 1]));
                        double db = CLIP(initialGain * (rawData[i][3 * j + 2]));

                        if (dr > clipHigh || dg > clipHigh || db > clipHigh) {
                            continue;
                        }

                        avg_r += dr;
                        avg_g += dg;
                        avg_b += db;
                        rn = gn = ++bn;
                    } else {
                        int c = FC(i, j);
                        double d = CLIP(initialGain * (rawData[i][j]));

                        if (d > clipHigh) {
                            continue;
                        }

                        // Let's test green first, because they are more numerous
                        if (c == 1) {
                            avg_g += d;
                            gn++;
                        } else if (c == 0) {
                            avg_r += d;
                            rn++;
                        } else { /*if (c==2)*/
                            avg_b += d;
                            bn++;
                        }
                    }
                }
            }
        } else {
            if (ri->getSensorType() != ST_BAYER) {
                if (ri->getSensorType() == ST_FUJI_XTRANS) {
                    const double compval = clipHigh / initialGain;
#ifdef _OPENMP
                    #pragma omp parallel
#endif
                    {
                        double avg_c[3] = {0.0};
                        int cn[3] = {0};
#ifdef _OPENMP
                        #pragma omp for schedule(dynamic,16) nowait
#endif

                        for (int i = 32; i < H - 32; i++) {
                            for (int j = 32; j < W - 32; j++) {
                                // each loop read 1 rgb triplet value
                                double d = rawData[i][j];

                                if (d > compval) {
                                    continue;
                                }

                                int c = ri->XTRANSFC(i, j);
                                avg_c[c] += d;
                                cn[c]++;
                            }
                        }

#ifdef _OPENMP
                        #pragma omp critical
#endif
                        {
                            avg_r += avg_c[0];
                            avg_g += avg_c[1];
                            avg_b += avg_c[2];
                            rn += cn[0];
                            gn += cn[1];
                            bn += cn[2];
                        }
                    }
                    avg_r *= initialGain;
                    avg_g *= initialGain;
                    avg_b *= initialGain;
                } else {
                    for (int i = 32; i < H - 32; i++)
                        for (int j = 32; j < W - 32; j++) {
                            // each loop read 1 rgb triplet value

                            double dr = CLIP(initialGain * (rawData[i][3 * j]));
                            double dg = CLIP(initialGain * (rawData[i][3 * j + 1]));
                            double db = CLIP(initialGain * (rawData[i][3 * j + 2]));

                            if (dr > clipHigh || dg > clipHigh || db > clipHigh) {
                                continue;
                            }

                            avg_r += dr;
                            rn++;
                            avg_g += dg;
                            avg_b += db;
                        }

                    gn = rn;
                    bn = rn;
                }
            } else {
                //determine GRBG coset; (ey,ex) is the offset of the R subarray
                int ey, ex;

                if (ri->ISGREEN(0, 0)) { //first pixel is G
                    if (ri->ISRED(0, 1)) {
                        ey = 0;
                        ex = 1;
                    } else {
                        ey = 1;
                        ex = 0;
                    }
                } else {//first pixel is R or B
                    if (ri->ISRED(0, 0)) {
                        ey = 0;
                        ex = 0;
                    } else {
                        ey = 1;
                        ex = 1;
                    }
                }

                const double compval = clipHigh / initialGain;
#ifdef _OPENMP
                #pragma omp parallel for reduction(+:avg_r,avg_g,avg_b,rn,gn,bn) schedule(dynamic,8)
#endif

                for (int i = 32; i < H - 32; i += 2)
                    for (int j = 32; j < W - 32; j += 2) {
                        //average each Bayer quartet component individually if non-clipped
                        double d[2][2];
                        d[0][0] = rawData[i][j];
                        d[0][1] = rawData[i][j + 1];
                        d[1][0] = rawData[i + 1][j];
                        d[1][1] = rawData[i + 1][j + 1];

                        if (d[ey][ex] <= compval) {
                            avg_r += d[ey][ex];
                            rn++;
                        }

                        if (d[1 - ey][ex] <= compval) {
                            avg_g += d[1 - ey][ex];
                            gn++;
                        }

                        if (d[ey][1 - ex] <= compval) {
                            avg_g += d[ey][1 - ex];
                            gn++;
                        }

                        if (d[1 - ey][1 - ex] <= compval) {
                            avg_b += d[1 - ey][1 - ex];
                            bn++;
                        }
                    }

                avg_r *= initialGain;
                avg_g *= initialGain;
                avg_b *= initialGain;

            }
        }
    }

    if (wbpar.method == "autitcgreen") {
        bool twotimes = false;
        int precision = 3;//must be 3 5 or 9
        bool oldsampling = wbpar.itcwb_sampling;

        if (oldsampling == true) {
            precision = 5;
        }

        // bool extra = true;
        const int bfw = W / precision + ((W % precision) > 0 ? 1 : 0);// 5 arbitrary value can be change to 3 or 9 ;
        const int bfh = H / precision + ((H % precision) > 0 ? 1 : 0);
        WBauto(extra, tempref, greenref, redloc, greenloc, blueloc, bfw, bfh, avg_rm, avg_gm, avg_bm, tempitc, greenitc, temp0, delta,  bia, dread, kcam, nocam, studgood, minchrom, kmin, minhist, maxhist, twotimes, wbpar, begx, begy, yEn,  xEn,  cx,  cy, cmp, raw, hrp);
    }


    if (settings->verbose  && wbpar.method != "autitcgreen") {
        printf("RGB grey AVG: %g %g %g\n", avg_r / std::max(1, rn), avg_g / std::max(1, gn), avg_b / std::max(1, bn));
    }

    if (wbpar.method != "autitcgreen") {
        const double reds = avg_r / std::max(1, rn) * refwb_red;
        const double greens = avg_g / std::max(1, gn) * refwb_green;
        const double blues = avg_b / std::max(1, bn) * refwb_blue;
        redAWBMul = rm = imatrices.rgb_cam[0][0] * reds + imatrices.rgb_cam[0][1] * greens + imatrices.rgb_cam[0][2] * blues;
        greenAWBMul = gm = imatrices.rgb_cam[1][0] * reds + imatrices.rgb_cam[1][1] * greens + imatrices.rgb_cam[1][2] * blues;
        blueAWBMul = bm = imatrices.rgb_cam[2][0] * reds + imatrices.rgb_cam[2][1] * greens + imatrices.rgb_cam[2][2] * blues;
    }

}

void RawImageSource::getAutoWBMultipliers(double &rm, double &gm, double &bm)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

//    BENCHFUN
    constexpr double clipHigh = 64000.0;

    if (ri->get_colors() == 1) {
        rm = gm = bm = 1;
        return;
    }

    if (redAWBMul != -1.) {
        rm = redAWBMul;
        gm = greenAWBMul;
        bm = blueAWBMul;
        return;
    }

    if (!isWBProviderReady()) {
        rm = -1.0;
        gm = -1.0;
        bm = -1.0;
        return;
    }

    double avg_r = 0;
    double avg_g = 0;
    double avg_b = 0;
    int rn = 0, gn = 0, bn = 0;

    if (fuji) {
        for (int i = 32; i < H - 32; i++) {
            int fw = ri->get_FujiWidth();
            int start = std::abs(fw - i) + 32;
            int end = min(H + W - fw - i, fw + i) - 32;

            for (int j = start; j < end; j++) {
                if (ri->getSensorType() != ST_BAYER) {
                    double dr = CLIP(initialGain * (rawData[i][3 * j]));
                    double dg = CLIP(initialGain * (rawData[i][3 * j + 1]));
                    double db = CLIP(initialGain * (rawData[i][3 * j + 2]));

                    if (dr > clipHigh || dg > clipHigh || db > clipHigh) {
                        continue;
                    }

                    avg_r += dr;
                    avg_g += dg;
                    avg_b += db;
                    rn = gn = ++bn;
                } else {
                    int c = FC(i, j);
                    double d = CLIP(initialGain * (rawData[i][j]));

                    if (d > clipHigh) {
                        continue;
                    }

                    // Let's test green first, because they are more numerous
                    if (c == 1) {
                        avg_g += d;
                        gn++;
                    } else if (c == 0) {
                        avg_r += d;
                        rn++;
                    } else { /*if (c==2)*/
                        avg_b += d;
                        bn++;
                    }
                }
            }
        }
    } else {
        if (ri->getSensorType() != ST_BAYER) {
            if (ri->getSensorType() == ST_FUJI_XTRANS) {
                const double compval = clipHigh / initialGain;
#ifdef _OPENMP
                #pragma omp parallel
#endif
                {
                    double avg_c[3] = {0.0};
                    int cn[3] = {0};
#ifdef _OPENMP
                    #pragma omp for schedule(dynamic,16) nowait
#endif

                    for (int i = 32; i < H - 32; i++) {
                        for (int j = 32; j < W - 32; j++) {
                            // each loop read 1 rgb triplet value
                            double d = rawData[i][j];

                            if (d > compval) {
                                continue;
                            }

                            int c = ri->XTRANSFC(i, j);
                            avg_c[c] += d;
                            cn[c]++;
                        }
                    }

#ifdef _OPENMP
                    #pragma omp critical
#endif
                    {
                        avg_r += avg_c[0];
                        avg_g += avg_c[1];
                        avg_b += avg_c[2];
                        rn += cn[0];
                        gn += cn[1];
                        bn += cn[2];
                    }
                }
                avg_r *= initialGain;
                avg_g *= initialGain;
                avg_b *= initialGain;
            } else {
                for (int i = 32; i < H - 32; i++)
                    for (int j = 32; j < W - 32; j++) {
                        // each loop read 1 rgb triplet value

                        double dr = CLIP(initialGain * (rawData[i][3 * j]));
                        double dg = CLIP(initialGain * (rawData[i][3 * j + 1]));
                        double db = CLIP(initialGain * (rawData[i][3 * j + 2]));

                        if (dr > clipHigh || dg > clipHigh || db > clipHigh) {
                            continue;
                        }

                        avg_r += dr;
                        rn++;
                        avg_g += dg;
                        avg_b += db;
                    }

                gn = rn;
                bn = rn;
            }
        } else {
            //determine GRBG coset; (ey,ex) is the offset of the R subarray
            int ey, ex;

            if (ri->ISGREEN(0, 0)) { //first pixel is G
                if (ri->ISRED(0, 1)) {
                    ey = 0;
                    ex = 1;
                } else {
                    ey = 1;
                    ex = 0;
                }
            } else {//first pixel is R or B
                if (ri->ISRED(0, 0)) {
                    ey = 0;
                    ex = 0;
                } else {
                    ey = 1;
                    ex = 1;
                }
            }

            const double compval = clipHigh / initialGain;
#ifdef _OPENMP
            #pragma omp parallel for reduction(+:avg_r,avg_g,avg_b,rn,gn,bn) schedule(dynamic,8)
#endif

            for (int i = 32; i < H - 32; i += 2)
                for (int j = 32; j < W - 32; j += 2) {
                    //average each Bayer quartet component individually if non-clipped
                    double d[2][2];
                    d[0][0] = rawData[i][j];
                    d[0][1] = rawData[i][j + 1];
                    d[1][0] = rawData[i + 1][j];
                    d[1][1] = rawData[i + 1][j + 1];

                    if (d[ey][ex] <= compval) {
                        avg_r += d[ey][ex];
                        rn++;
                    }

                    if (d[1 - ey][ex] <= compval) {
                        avg_g += d[1 - ey][ex];
                        gn++;
                    }

                    if (d[ey][1 - ex] <= compval) {
                        avg_g += d[ey][1 - ex];
                        gn++;
                    }

                    if (d[1 - ey][1 - ex] <= compval) {
                        avg_b += d[1 - ey][1 - ex];
                        bn++;
                    }
                }

            avg_r *= initialGain;
            avg_g *= initialGain;
            avg_b *= initialGain;

        }
    }

    if (settings->verbose) {
        printf("AVG: %g %g %g\n", avg_r / std::max(1, rn), avg_g / std::max(1, gn), avg_b / std::max(1, bn));
    }

    //    return ColorTemp (pow(avg_r/rn, 1.0/6.0)*img_r, pow(avg_g/gn, 1.0/6.0)*img_g, pow(avg_b/bn, 1.0/6.0)*img_b);

    double reds = avg_r / std::max(1, rn) * refwb_red;
    double greens = avg_g / std::max(1, gn) * refwb_green;
    double blues = avg_b / std::max(1, bn) * refwb_blue;

    redAWBMul = rm = imatrices.rgb_cam[0][0] * reds + imatrices.rgb_cam[0][1] * greens + imatrices.rgb_cam[0][2] * blues;
    greenAWBMul = gm = imatrices.rgb_cam[1][0] * reds + imatrices.rgb_cam[1][1] * greens + imatrices.rgb_cam[1][2] * blues;
    blueAWBMul = bm = imatrices.rgb_cam[2][0] * reds + imatrices.rgb_cam[2][1] * greens + imatrices.rgb_cam[2][2] * blues;
}

ColorTemp RawImageSource::getSpotWB(std::vector<Coord2D> &red, std::vector<Coord2D> &green, std::vector<Coord2D> &blue, int tran, double equal, StandardObserver observer)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

    int x;
    int y;
    double reds = 0, greens = 0, blues = 0;
    unsigned int rn = 0;

    if (ri->getSensorType() != ST_BAYER) {
        if (ri->getSensorType() == ST_FUJI_XTRANS) {
            int d[9][2] = {{0, 0}, { -1, -1}, { -1, 0}, { -1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

            for (size_t i = 0; i < red.size(); i++) {
                transformPosition(red[i].x, red[i].y, tran, x, y);
                double rloc, gloc, bloc;
                int rnbrs, gnbrs, bnbrs;
                rloc = gloc = bloc = rnbrs = gnbrs = bnbrs = 0;

                for (int k = 0; k < 9; k++) {
                    int xv = x + d[k][0];
                    int yv = y + d[k][1];

                    if (xv >= 0 && yv >= 0 && xv < W && yv < H) {
                        if (ri->ISXTRANSRED(yv, xv)) { //RED
                            rloc += (rawData[yv][xv]);
                            rnbrs++;
                            continue;
                        } else if (ri->ISXTRANSBLUE(yv, xv)) { //BLUE
                            bloc += (rawData[yv][xv]);
                            bnbrs++;
                            continue;
                        } else { // GREEN
                            gloc += (rawData[yv][xv]);
                            gnbrs++;
                            continue;
                        }
                    }
                }

                rloc /= rnbrs;
                gloc /= gnbrs;
                bloc /= bnbrs;

                if (rloc < clmax[0] && gloc < clmax[1] && bloc < clmax[2]) {
                    reds += rloc;
                    greens += gloc;
                    blues += bloc;
                    rn++;
                }
            }

        } else {
            int xmin, xmax, ymin, ymax;
            int xr, xg, xb, yr, yg, yb;

            for (size_t i = 0; i < red.size(); i++) {
                transformPosition(red[i].x, red[i].y, tran, xr, yr);
                transformPosition(green[i].x, green[i].y, tran, xg, yg);
                transformPosition(blue[i].x, blue[i].y, tran, xb, yb);

                if (initialGain * (rawData[yr][3 * xr]) > 52500 ||
                        initialGain * (rawData[yg][3 * xg + 1]) > 52500 ||
                        initialGain * (rawData[yb][3 * xb + 2]) > 52500) {
                    continue;
                }

                xmin = min(xr, xg, xb);
                xmax = max(xr, xg, xb);
                ymin = min(yr, yg, yb);
                ymax = max(yr, yg, yb);

                if (xmin >= 0 && ymin >= 0 && xmax < W && ymax < H) {
                    reds    += (rawData[yr][3 * xr]);
                    greens  += (rawData[yg][3 * xg + 1]);
                    blues   += (rawData[yb][3 * xb + 2]);
                    rn++;
                }
            }
        }

    } else {

        int d[9][2] = {{0, 0}, { -1, -1}, { -1, 0}, { -1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

        for (size_t i = 0; i < red.size(); i++) {
            transformPosition(red[i].x, red[i].y, tran, x, y);
            double rloc, gloc, bloc;
            int rnbrs, gnbrs, bnbrs;
            rloc = gloc = bloc = rnbrs = gnbrs = bnbrs = 0;

            for (int k = 0; k < 9; k++) {
                int xv = x + d[k][0];
                int yv = y + d[k][1];
                int c = FC(yv, xv);

                if (xv >= 0 && yv >= 0 && xv < W && yv < H) {
                    if (c == 0) { //RED
                        rloc += (rawData[yv][xv]);
                        rnbrs++;
                        continue;
                    } else if (c == 2) { //BLUE
                        bloc += (rawData[yv][xv]);
                        bnbrs++;
                        continue;
                    } else { // GREEN
                        gloc += (rawData[yv][xv]);
                        gnbrs++;
                        continue;
                    }
                }
            }

            rloc /= std::max(1, rnbrs);
            gloc /= std::max(1, gnbrs);
            bloc /= std::max(1, bnbrs);

            if (rloc < clmax[0] && gloc < clmax[1] && bloc < clmax[2]) {
                reds += rloc;
                greens += gloc;
                blues += bloc;
                rn++;
            }

            transformPosition(green[i].x, green[i].y, tran, x, y); //these are redundant now ??? if not, repeat for these blocks same as for red[]
            rloc = gloc = bloc = rnbrs = gnbrs = bnbrs = 0;

            for (int k = 0; k < 9; k++) {
                int xv = x + d[k][0];
                int yv = y + d[k][1];
                int c = FC(yv, xv);

                if (xv >= 0 && yv >= 0 && xv < W && yv < H) {
                    if (c == 0) { //RED
                        rloc += (rawData[yv][xv]);
                        rnbrs++;
                        continue;
                    } else if (c == 2) { //BLUE
                        bloc += (rawData[yv][xv]);
                        bnbrs++;
                        continue;
                    } else { // GREEN
                        gloc += (rawData[yv][xv]);
                        gnbrs++;
                        continue;
                    }
                }
            }

            rloc /= std::max(rnbrs, 1);
            gloc /= std::max(gnbrs, 1);
            bloc /= std::max(bnbrs, 1);

            if (rloc < clmax[0] && gloc < clmax[1] && bloc < clmax[2]) {
                reds += rloc;
                greens += gloc;
                blues += bloc;
                rn++;
            }

            transformPosition(blue[i].x, blue[i].y, tran, x, y);
            rloc = gloc = bloc = rnbrs = gnbrs = bnbrs = 0;

            for (int k = 0; k < 9; k++) {
                int xv = x + d[k][0];
                int yv = y + d[k][1];
                int c = FC(yv, xv);

                if (xv >= 0 && yv >= 0 && xv < W && yv < H) {
                    if (c == 0) { //RED
                        rloc += (rawData[yv][xv]);
                        rnbrs++;
                        continue;
                    } else if (c == 2) { //BLUE
                        bloc += (rawData[yv][xv]);
                        bnbrs++;
                        continue;
                    } else { // GREEN
                        gloc += (rawData[yv][xv]);
                        gnbrs++;
                        continue;
                    }
                }
            }

            rloc /= std::max(rnbrs, 1);
            gloc /= std::max(gnbrs, 1);
            bloc /= std::max(bnbrs, 1);

            if (rloc < clmax[0] && gloc < clmax[1] && bloc < clmax[2]) {
                reds += rloc;
                greens += gloc;
                blues += bloc;
                rn++;
            }
        }
    }

    if (2u * rn < red.size()) {
        return ColorTemp(equal);
    } else {
        reds = reds / std::max(1u, rn) * refwb_red;
        greens = greens / std::max(1u, rn) * refwb_green;
        blues = blues / std::max(1u, rn) * refwb_blue;

        double rm = imatrices.rgb_cam[0][0] * reds + imatrices.rgb_cam[0][1] * greens + imatrices.rgb_cam[0][2] * blues;
        double gm = imatrices.rgb_cam[1][0] * reds + imatrices.rgb_cam[1][1] * greens + imatrices.rgb_cam[1][2] * blues;
        double bm = imatrices.rgb_cam[2][0] * reds + imatrices.rgb_cam[2][1] * greens + imatrices.rgb_cam[2][2] * blues;

        return ColorTemp(rm, gm, bm, equal, observer);
    }
}

void RawImageSource::transformPosition(int x, int y, int tran, int& ttx, int& tty)
{

    tran = defTransform(ri, tran);

    x += border;
    y += border;

    if (d1x) {
        if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
            x /= 2;
        } else {
            y /= 2;
        }
    }

    int w = W, h = H;

    if (fuji) {
        w = ri->get_FujiWidth() * 2 + 1;
        h = (H - ri->get_FujiWidth()) * 2 + 1;
    }

    int sw = w, sh = h;

    if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
        sw = h;
        sh = w;
    }

    int ppx = x, ppy = y;

    if (tran & TR_HFLIP) {
        ppx = sw - 1 - x ;
    }

    if (tran & TR_VFLIP) {
        ppy = sh - 1 - y;
    }

    int tx = ppx;
    int ty = ppy;

    if ((tran & TR_ROT) == TR_R180) {
        tx = w - 1 - ppx;
        ty = h - 1 - ppy;
    } else if ((tran & TR_ROT) == TR_R90) {
        tx = ppy;
        ty = h - 1 - ppx;
    } else if ((tran & TR_ROT) == TR_R270) {
        tx = w - 1 - ppy;
        ty = ppx;
    }

    if (fuji) {
        ttx = (tx + ty) / 2;
        tty = (ty - tx) / 2 + ri->get_FujiWidth();
    } else {
        ttx = tx;
        tty = ty;
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void RawImageSource::inverse33(const double (*rgb_cam)[3], double (*cam_rgb)[3])
{
    double nom = (rgb_cam[0][2] * rgb_cam[1][1] * rgb_cam[2][0] - rgb_cam[0][1] * rgb_cam[1][2] * rgb_cam[2][0] -
                  rgb_cam[0][2] * rgb_cam[1][0] * rgb_cam[2][1] + rgb_cam[0][0] * rgb_cam[1][2] * rgb_cam[2][1] +
                  rgb_cam[0][1] * rgb_cam[1][0] * rgb_cam[2][2] - rgb_cam[0][0] * rgb_cam[1][1] * rgb_cam[2][2]);
    cam_rgb[0][0] = (rgb_cam[1][2] * rgb_cam[2][1] - rgb_cam[1][1] * rgb_cam[2][2]) / nom;
    cam_rgb[0][1] = -(rgb_cam[0][2] * rgb_cam[2][1] - rgb_cam[0][1] * rgb_cam[2][2]) / nom;
    cam_rgb[0][2] = (rgb_cam[0][2] * rgb_cam[1][1] - rgb_cam[0][1] * rgb_cam[1][2]) / nom;
    cam_rgb[1][0] = -(rgb_cam[1][2] * rgb_cam[2][0] - rgb_cam[1][0] * rgb_cam[2][2]) / nom;
    cam_rgb[1][1] = (rgb_cam[0][2] * rgb_cam[2][0] - rgb_cam[0][0] * rgb_cam[2][2]) / nom;
    cam_rgb[1][2] = -(rgb_cam[0][2] * rgb_cam[1][0] - rgb_cam[0][0] * rgb_cam[1][2]) / nom;
    cam_rgb[2][0] = (rgb_cam[1][1] * rgb_cam[2][0] - rgb_cam[1][0] * rgb_cam[2][1]) / nom;
    cam_rgb[2][1] = -(rgb_cam[0][1] * rgb_cam[2][0] - rgb_cam[0][0] * rgb_cam[2][1]) / nom;
    cam_rgb[2][2] = (rgb_cam[0][1] * rgb_cam[1][0] - rgb_cam[0][0] * rgb_cam[1][1]) / nom;
}

DiagonalCurve* RawImageSource::phaseOneIccCurve;
DiagonalCurve* RawImageSource::phaseOneIccCurveInv;

void RawImageSource::init()
{

    {
        // Initialize Phase One ICC curves

        /* This curve is derived from TIFFTAG_TRANSFERFUNCTION of a Capture One P25+ image with applied film curve,
           exported to TIFF with embedded camera ICC. It's assumed to be similar to most standard curves in
           Capture One. It's not necessary to be exactly the same, it's just to be close to a typical curve to
           give the Phase One ICC files a good working space. */
        const double phase_one_forward[] = {
            0.0000000000, 0.0000000000, 0.0152590219, 0.0029602502, 0.0305180438, 0.0058899825, 0.0457770657, 0.0087739376, 0.0610360876, 0.0115968566,
            0.0762951095, 0.0143587396, 0.0915541314, 0.0171969177, 0.1068131533, 0.0201876860, 0.1220721752, 0.0232852674, 0.1373311971, 0.0264744030,
            0.1525902190, 0.0297245747, 0.1678492409, 0.0330205234, 0.1831082628, 0.0363775082, 0.1983672847, 0.0397802701, 0.2136263066, 0.0432593271,
            0.2288853285, 0.0467841611, 0.2441443503, 0.0503700313, 0.2594033722, 0.0540474556, 0.2746623941, 0.0577859159, 0.2899214160, 0.0616159304,
            0.3051804379, 0.0655222400, 0.3204394598, 0.0695353628, 0.3356984817, 0.0736552987, 0.3509575036, 0.0778973068, 0.3662165255, 0.0822461280,
            0.3814755474, 0.0867170214, 0.3967345693, 0.0913252461, 0.4119935912, 0.0960860609, 0.4272526131, 0.1009994659, 0.4425116350, 0.1060654612,
            0.4577706569, 0.1113298238, 0.4730296788, 0.1167925536, 0.4882887007, 0.1224841688, 0.5035477226, 0.1284046693, 0.5188067445, 0.1345540551,
            0.5340657664, 0.1409781033, 0.5493247883, 0.1476615549, 0.5645838102, 0.1546501869, 0.5798428321, 0.1619287404, 0.5951018540, 0.1695277333,
            0.6103608759, 0.1774776837, 0.6256198978, 0.1858091096, 0.6408789197, 0.1945525292, 0.6561379416, 0.2037384604, 0.6713969635, 0.2134279393,
            0.6866559854, 0.2236667430, 0.7019150072, 0.2345159075, 0.7171740291, 0.2460517281, 0.7324330510, 0.2583047227, 0.7476920729, 0.2714122225,
            0.7629510948, 0.2854352636, 0.7782101167, 0.3004959182, 0.7934691386, 0.3167620356, 0.8087281605, 0.3343862058, 0.8239871824, 0.3535820554,
            0.8392462043, 0.3745937285, 0.8545052262, 0.3977111467, 0.8697642481, 0.4232547494, 0.8850232700, 0.4515754940, 0.9002822919, 0.4830701152,
            0.9155413138, 0.5190966659, 0.9308003357, 0.5615320058, 0.9460593576, 0.6136263066, 0.9613183795, 0.6807965209, 0.9765774014, 0.7717402914,
            0.9918364233, 0.9052109560, 1.0000000000, 1.0000000000
        };
        std::vector<double> cForwardPoints;
        cForwardPoints.push_back(double(DCT_Spline));  // The first value is the curve type
        std::vector<double> cInversePoints;
        cInversePoints.push_back(double(DCT_Spline));  // The first value is the curve type

        for (unsigned int i = 0; i < sizeof(phase_one_forward) / sizeof(phase_one_forward[0]); i += 2) {
            cForwardPoints.push_back(phase_one_forward[i + 0]);
            cForwardPoints.push_back(phase_one_forward[i + 1]);
            cInversePoints.push_back(phase_one_forward[i + 1]);
            cInversePoints.push_back(phase_one_forward[i + 0]);
        }

        phaseOneIccCurve = new DiagonalCurve(cForwardPoints, CURVES_MIN_POLY_POINTS);
        phaseOneIccCurveInv = new DiagonalCurve(cInversePoints, CURVES_MIN_POLY_POINTS);
    }
}

void RawImageSource::getRawValues(int x, int y, int rotate, int &R, int &G, int &B)
{
    if (!checkRawDataDimensions(rawData, *ri, W, H) || d1x) { // Nikon D1x has special sensor. We just skip it
        R = G = B = 0;
        return;
    }

    int xnew = x + border;
    int ynew = y + border;
    rotate += ri->get_rotateDegree();
    rotate %= 360;

    if (rotate == 90) {
        std::swap(xnew, ynew);
        ynew = H - 1 - ynew;
    } else if (rotate == 180) {
        xnew = W - 1 - xnew;
        ynew = H - 1 - ynew;
    } else if (rotate == 270) {
        std::swap(xnew, ynew);
        xnew = W - 1 - xnew;
    }

    xnew = LIM(xnew, 0, W - 1);
    ynew = LIM(ynew, 0, H - 1);
    int c = ri->getSensorType() == ST_FUJI_XTRANS ? ri->XTRANSFC(ynew, xnew) : ri->FC(ynew, xnew);
    int val = round(rawData[ynew][xnew] / scale_mul[c]);

    if (c == 0) {
        R = val;
        G = 0;
        B = 0;
    } else if (c == 2) {
        R = 0;
        G = 0;
        B = val;
    } else {
        R = 0;
        G = val;
        B = 0;
    }
}
/*
    Copyright (c) Ingo Weyrich  2020 (heckflosse67@gmx.de)
*/
void RawImageSource::getMinValsXtrans(const array2D<float> &rawData) {
#ifdef _OPENMP
    #pragma omp parallel for reduction (min:minVals)
#endif
    for (int row = 0; row < H; ++row) {
        const int c0 = ri->XTRANSFC(row, 0);
        const int c1 = ri->XTRANSFC(row, 1);
        const int c2 = ri->XTRANSFC(row, 2);
        const int c3 = ri->XTRANSFC(row, 3);
        const int c4 = ri->XTRANSFC(row, 4);
        const int c5 = ri->XTRANSFC(row, 5);
        const float cb0 = c_black[c0];
        const float cb1 = c_black[c1];
        const float cb2 = c_black[c2];
        const float cb3 = c_black[c3];
        const float cb4 = c_black[c4];
        const float cb5 = c_black[c5];
        float m0 = minVals[c0];
        float m1 = minVals[c1];
        float m2 = minVals[c2];
        float m3 = minVals[c3];
        float m4 = minVals[c4];
        float m5 = minVals[c5];
        int col = 0;
        for (; col < W - 5; col += 6) {
            if (LIKELY(rawData[row][col] >= cb0)) {
                m0 = rtengine::min(m0, rawData[row][col] - cb0);
            }
            if (LIKELY(rawData[row][col + 1] >= cb1)) {
                m1 = rtengine::min(m1, rawData[row][col + 1] - cb1);
            }
            if (LIKELY(rawData[row][col + 2] >= cb2)) {
                m2 = rtengine::min(m2, rawData[row][col + 2] - cb2);
            }
            if (LIKELY(rawData[row][col + 3] >= cb3)) {
                m3 = rtengine::min(m3, rawData[row][col + 3] - cb3);
            }
            if (LIKELY(rawData[row][col + 4] >= cb4)) {
                m4 = rtengine::min(m4, rawData[row][col + 4] - cb4);
            }
            if (LIKELY(rawData[row][col + 5] >= cb5)) {
                m5 = rtengine::min(m5, rawData[row][col + 5] - cb5);
            }
        }
        for (; col < W; ++col) {
            const int c = ri->XTRANSFC(row,col);
            if (LIKELY(rawData[row][col] >= c_black[c])) {
                minVals[c] = rtengine::min(minVals[c], rawData[row][col] - c_black[c]);
            }
        }
        minVals[c0] = rtengine::min(m0, minVals[c0]);
        minVals[c1] = rtengine::min(m1, minVals[c1]);
        minVals[c2] = rtengine::min(m2, minVals[c2]);
        minVals[c3] = rtengine::min(m3, minVals[c3]);
        minVals[c4] = rtengine::min(m4, minVals[c4]);
        minVals[c5] = rtengine::min(m5, minVals[c5]);

    }
}

bool RawImageSource::isGainMapSupported() const
{
    if (!(ri->DNGVERSION() && ri->isBayer())) {
        return false;
    }
    const auto &gainMaps = getMetaData()->getGainMaps();
    const auto n = gainMaps.size();
    if (n != 4) { // we need 4 gainmaps for bayer files
        if (rtengine::settings->verbose) {
            std::cout << "GainMap has " << n << " maps, but 4 are needed" << std::endl;
        }
        return false;
    }
    unsigned int check = 0;
    bool noOp = true;
    for (const auto &m : gainMaps) {
        if (m.MapGain.size() < 1) {
            if (rtengine::settings->verbose) {
                std::cout << "GainMap has invalid size of " << m.MapGain.size() << std::endl;
            }
            return false;
        }
        if (m.MapGain.size() != static_cast<std::size_t>(m.MapPointsV) * static_cast<std::size_t>(m.MapPointsH) * static_cast<std::size_t>(m.MapPlanes)) {
            if (rtengine::settings->verbose) {
                std::cout << "GainMap has size of " << m.MapGain.size() << ", but needs " << m.MapPointsV * m.MapPointsH * m.MapPlanes << std::endl;
            }
            return false;
        }
        if (m.RowPitch != 2 || m.ColPitch != 2) {
            if (rtengine::settings->verbose) {
                std::cout << "GainMap needs Row/ColPitch of 2/2, but has " << m.RowPitch << "/" << m.ColPitch << std::endl;
            }
            return false;
        }
        if (m.Top == 0) {
            if (m.Left == 0) {
                check += 1;
            } else if (m.Left == 1) {
                check += 2;
            }
        } else if (m.Top == 1) {
            if (m.Left == 0) {
                check += 4;
            } else if (m.Left == 1) {
                check += 8;
            }
        }
        for (size_t i = 0; noOp && i < m.MapGain.size(); ++i) {
            if (m.MapGain[i] != 1.f) { // we have at least one value != 1.f => map is not a nop
                noOp = false;
            }
        }
    }
    if (noOp || check != 15) { // all maps are nops or the structure of the combination of 4 maps is not correct
        if (rtengine::settings->verbose) {
            if (noOp) {
                std::cout << "GainMap is a nop" << std::endl;
            } else {
                std::cout << "GainMap has unsupported type : " << check << std::endl;
            }
        }
        return false;
    }
    return true;
}

void RawImageSource::applyDngGainMap(const float black[4], const std::vector<GainMap> &gainMaps)
{
    assert(checkRawDataDimensions(rawData, *ri, W, H));

    // now we can apply each gain map to raw_data
    array2D<float> mvals[2][2];

    for (auto &m : gainMaps) {
        mvals[m.Top & 1][m.Left & 1](m.MapPointsH, m.MapPointsV, m.MapGain.data());
    }

    // now we assume, col_scale and row scale is the same for all maps
    const float col_scale = float(gainMaps[0].MapPointsH - 1) / float(W);
    const float row_scale = float(gainMaps[0].MapPointsV - 1) / float(H);

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 16)
#endif

    for (std::size_t y = 0; y < static_cast<size_t>(H); ++y) {
        const float rowBlack[2] = {black[FC(y, 0)], black[FC(y, 1)]};
        const float ys = y * row_scale;
        float xs = 0.f;

        for (std::size_t x = 0; x < static_cast<std::size_t>(W); ++x, xs += col_scale) {
            const float f = getBilinearValue(mvals[y & 1][x & 1], xs, ys);
            const float b = rowBlack[x & 1];
            rawData[y][x] = rtengine::max((rawData[y][x] - b) * f + b, 0.f);
        }
    }
}
/*
    Copyright (c) Ingo Weyrich  2020 (heckflosse67@gmx.de)
*/
void RawImageSource::getMinValsBayer(const array2D<float> &rawData, bool zeroIsBad) {
BENCHFUN
    if (!zeroIsBad) {
#ifdef _OPENMP
        #pragma omp parallel for reduction(min:minVals)
#endif
        for (int row = 0; row < H; ++row) {
            const int c0 = FC(row, 0);
            const int c1 = FC(row, 1);
            const float cb0 = c_black[c0];
            const float cb1 = c_black[c1];
            float m0 = minVals[c0];
            float m1 = minVals[c1];
            int col = 0;
            for (; col < W - 1; col += 2) {
                if (LIKELY(rawData[row][col] >= cb0)) {
                    m0 = rtengine::min(m0, rawData[row][col] - cb0);
                }
                if (LIKELY(rawData[row][col + 1] >= cb1)) {
                    m1 = rtengine::min(m1, rawData[row][col + 1] - cb1);
                }
            }
            if (col < W && LIKELY(rawData[row][col] >= cb0)) {
                m0 = rtengine::min(m0, rawData[row][col] - cb0);
            }
            minVals[c0] = m0;
            minVals[c1] = m1;
        }
    } else {
#ifdef _OPENMP
        #pragma omp parallel for reduction(min:minVals) schedule(dynamic,16)
#endif
        for (int row = 0; row < H; ++row) {
            const int c0 = FC(row, 0);
            const int c1 = FC(row, 1);
            const float cb0 = c_black[c0];
            const float cb1 = c_black[c1];
            float m0 = minVals[c0];
            float m1 = minVals[c1];
            int col = 0;
            for (; col < W - 1; col += 2) {
                if (LIKELY(rawData[row][col] >= cb0 && rawData[row][col] > 0.f)) {
                    m0 = rtengine::min(m0, rawData[row][col] - cb0);
                }
                if (LIKELY(rawData[row][col + 1] >= cb1 && rawData[row][col + 1] > 0.f)) {
                    m1 = rtengine::min(m1, rawData[row][col + 1] - cb1);
                }
            }
            if (col < W && LIKELY(rawData[row][col] >= cb0 && rawData[row][col] > 0.f)) {
                m0 = rtengine::min(m0, rawData[row][col] - cb0);
            }
            minVals[c0] = m0;
            minVals[c1] = m1;
        }
    }
}

void RawImageSource::cleanup()
{
    delete phaseOneIccCurve;
    delete phaseOneIccCurveInv;
}

} /* namespace */
