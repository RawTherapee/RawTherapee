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
#include <cstring>
#include <cstdio>

#include "colortemp.h"
#include "image8.h"
#include "imagefloat.h"
#include "rtengine.h"

using namespace rtengine;


Image8::Image8 ()
{
}

Image8::Image8 (int w, int h)
{
    allocate (w, h);
}

Image8::~Image8 ()
{
}

void Image8::getScanline (int row, unsigned char* buffer, int bps, bool isFloat) const
{

    if (data == nullptr) {
        return;
    }

    if (bps == 8) {
        memcpy (buffer, data + row * width * 3, width * 3);
    } else if (bps == 16) {
        unsigned short* sbuffer = (unsigned short*) buffer;

        for (int i = 0, ix = row * width * 3; i < width * 3; ++i, ++ix) {
            sbuffer[i] = static_cast<unsigned short>(data[ix]) * 257;
        }
    }
}

void Image8::setScanline (int row, const unsigned char* buffer, int bps, unsigned int numSamples)
{

    if (data == nullptr) {
        return;
    }

    switch (sampleFormat) {
    case (IIOSF_UNSIGNED_CHAR):
        if(numSamples == 1) {
            for(size_t i = 0; i < static_cast<size_t>(width); ++i) {
                data[row * width * 3 + 3 * i] = data[row * width * 3 + 3 * i + 1] = data[row * width * 3 + 3 * i + 2] = buffer[i];
            }
        } else {
            memcpy (data + (uint64_t)row * (uint64_t)width * (uint64_t)3u, buffer, width * 3);
        }
        break;

    case (IIOSF_UNSIGNED_SHORT): {
        const unsigned short* sbuffer = (const unsigned short*) buffer;

        for (int i = 0, ix = row * width * 3; i < width * 3; ++i, ++ix) {
            data[ix] = uint16ToUint8Rounded(sbuffer[i]);
        }

        break;
    }

    default:
        // Other type are ignored, but could be implemented if necessary
        break;
    }
}

Image8* Image8::copy () const
{

    Image8* cp = new Image8 (width, height);
    copyData(cp);
    return cp;
}

void Image8::getStdImage (const ColorTemp &ctemp, int tran, Imagefloat* image, const PreviewProps &pp) const
{
    // compute channel multipliers
    float rm = 1.f, gm = 1.f, bm = 1.f;
    if (ctemp.getTemp() >= 0) {
        double drm, dgm, dbm;
        ctemp.getMultipliers (drm, dgm, dbm);
        rm = drm;
        gm = dgm;
        bm = dbm;

        rm = 1.f / rm;
        gm = 1.f / gm;
        bm = 1.f / bm;
        float mul_lum = 0.299f * rm + 0.587f * gm + 0.114f * bm;
        rm /= mul_lum;
        gm /= mul_lum;
        bm /= mul_lum;
    }

    int sx1, sy1, sx2, sy2;

    transform (pp, tran, sx1, sy1, sx2, sy2);

    int imwidth = image->getWidth(); // Destination image
    int imheight = image->getHeight(); // Destination image

    if (((tran & TR_ROT) == TR_R90) || ((tran & TR_ROT) == TR_R270)) {
        int swap = imwidth;
        imwidth = imheight;
        imheight = swap;
    }

    int maxx = width; // Source image
    int maxy = height; // Source image
    int mtran = tran & TR_ROT;
    int skip = pp.getSkip();

    //if ((sx1 + skip*imwidth)>maxx) imwidth -- ; // we have a boundary condition that can cause errors

    // improve speed by integrating the area division into the multipliers
    // switched to using ints for the red/green/blue channel buffer.
    // Incidentally this improves accuracy too.
    float area = skip * skip;
    float rm2 = rm;
    float gm2 = gm;
    float bm2 = bm;
    rm /= area;
    gm /= area;
    bm /= area;

#ifdef _OPENMP
    #pragma omp parallel
    {
#endif
        AlignedBuffer<float> abR(imwidth);
        AlignedBuffer<float> abG(imwidth);
        AlignedBuffer<float> abB(imwidth);
        float *lineR  = abR.data;
        float *lineG  = abG.data;
        float *lineB =  abB.data;

#ifdef _OPENMP
        #pragma omp for
#endif

        // Iterating all the rows of the destination image
        for (int iy = 0; iy < imheight; iy++) {
            int dst_x_lim = imwidth; // One less is the largest dst_x used.

            if (skip == 1) {
                // special case (speedup for 1:1 scale)
                // i: source image, first line of the current destination row
                int src_y = sy1 + iy;

                // overflow security check
                if (src_y >= maxy) {
                    continue;
                }

                for (int dst_x = 0, src_x = sx1; dst_x < imwidth; dst_x++, src_x++) {
                    float r_, g_, b_;

                    // overflow security check
                    if (src_x >= maxx) {
                        dst_x_lim = dst_x;
                        break;
                    }

                    convertTo(r(src_y, src_x), r_);
                    convertTo(g(src_y, src_x), g_);
                    convertTo(b(src_y, src_x), b_);
                    lineR[dst_x] = CLIP(rm2 * r_);
                    lineG[dst_x] = CLIP(gm2 * g_);
                    lineB[dst_x] = CLIP(bm2 * b_);
                }
            } else {
                // source image, first line of the current destination row
                int src_y = sy1 + skip * iy;

                if (src_y >= maxy) {
                    continue;
                }

                for (int dst_x = 0, src_x = sx1; dst_x < imwidth; dst_x++, src_x += skip) {
                    if (src_x >= maxx) {
                        dst_x_lim = dst_x;
                        break;
                    }

                    int src_sub_width = MIN(maxx - src_x, skip);
                    int src_sub_height = MIN(maxy - src_y, skip);

                    float rtot, gtot, btot; // RGB accumulators
                    rtot = gtot = btot = 0.;

                    for (int src_sub_y = 0; src_sub_y < src_sub_height; src_sub_y++)
                        for (int src_sub_x = 0; src_sub_x < src_sub_width; src_sub_x++) {
                            float r_, g_, b_;
                            convertTo(r(src_y + src_sub_y, src_x + src_sub_x), r_);
                            convertTo(g(src_y + src_sub_y, src_x + src_sub_x), g_);
                            convertTo(b(src_y + src_sub_y, src_x + src_sub_x), b_);
                            rtot += r_;
                            gtot += g_;
                            btot += b_;
                        }

                    // convert back to gamma and clip
                    if (src_sub_width == skip && src_sub_height == skip) {
                        // Common case where the sub-region is complete
                        lineR[dst_x] = CLIP(rm * rtot);
                        lineG[dst_x] = CLIP(gm * gtot);
                        lineB[dst_x] = CLIP(bm * btot);
                    } else {
                        // computing a special factor for this incomplete sub-region
                        float larea = src_sub_width * src_sub_height;
                        lineR[dst_x] = CLIP(rm2 * rtot / larea);
                        lineG[dst_x] = CLIP(gm2 * gtot / larea);
                        lineB[dst_x] = CLIP(bm2 * btot / larea);
                    }
                }
            }

            if      (mtran == TR_NONE)
                for (int dst_x = 0; dst_x < dst_x_lim; dst_x++) {
                    image->r(iy, dst_x) = lineR[dst_x];
                    image->g(iy, dst_x) = lineG[dst_x];
                    image->b(iy, dst_x) = lineB[dst_x];
                }
            else if (mtran == TR_R180)
                for (int dst_x = 0; dst_x < dst_x_lim; dst_x++) {
                    image->r(imheight - 1 - iy, imwidth - 1 - dst_x) = lineR[dst_x];
                    image->g(imheight - 1 - iy, imwidth - 1 - dst_x) = lineG[dst_x];
                    image->b(imheight - 1 - iy, imwidth - 1 - dst_x) = lineB[dst_x];
                }
            else if (mtran == TR_R90)
                for (int dst_x = 0; dst_x < dst_x_lim; dst_x++) {
                    image->r(dst_x, imheight - 1 - iy) = lineR[dst_x];
                    image->g(dst_x, imheight - 1 - iy) = lineG[dst_x];
                    image->b(dst_x, imheight - 1 - iy) = lineB[dst_x];
                }
            else if (mtran == TR_R270)
                for (int dst_x = 0; dst_x < dst_x_lim; dst_x++) {
                    image->r(imwidth - 1 - dst_x, iy) = lineR[dst_x];
                    image->g(imwidth - 1 - dst_x, iy) = lineG[dst_x];
                    image->b(imwidth - 1 - dst_x, iy) = lineB[dst_x];
                }
        }

#ifdef _OPENMP
    }
#endif
#undef GCLIP
}
