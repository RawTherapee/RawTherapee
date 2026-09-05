////////////////////////////////////////////////////////////////
//
//  Xtrans demosaic algorithm
//
//  code dated: April 18, 2018
//
//  xtrans_demosaic.cc is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////

#include "color.h"
#include "rtengine.h"
#include "rawimage.h"
#include "rawimagesource.h"
#include "xtrans_cfa.h"
#include "xtrans_markesteijn.h"
#include "rt_algo.h"
#include "rt_math.h"
#include "rtgui/multilangmgr.h"
#include "opthelper.h"
#include "StopWatch.h"

#include <cstring>
#include <new>

namespace rtengine
{
const float xyz_rgb[3][3] = {          // XYZ from RGB
    { 0.412453, 0.357580, 0.180423 },
    { 0.212671, 0.715160, 0.072169 },
    { 0.019334, 0.119193, 0.950227 }
};
const float d65_white[3] = { 0.950456, 1, 1.088754 };

void RawImageSource::cielab (const float (*rgb)[3], float* l, float* a, float *b, const int width, const int height, const int labWidth, const float xyz_cam[3][3])
{
    static LUTf cbrt(0x14000);

    if (!rgb) {
        static bool cbrtinit = false;
        if(!cbrtinit) {
#ifdef _OPENMP
            #pragma omp parallel for
#endif
            for (int i = 0; i < 0x14000; i++) {
                double r = i / 65535.0;
                cbrt[i] = r > Color::eps ? std::cbrt(r) : (Color::kappa * r + 16.0) / 116.0;
            }

            cbrtinit = true;
        }

        return;
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat c116v = F2V(116.f);
    vfloat c16v = F2V(16.f);
    vfloat c500v = F2V(500.f);
    vfloat c200v = F2V(200.f);
    vfloat xyz_camv[3][3];

    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++) {
            xyz_camv[i][j] = F2V(xyz_cam[i][j]);
        }

#endif // __SSE2__

    for(int i = 0; i < height; i++) {
        int j = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)

        for(; j < labWidth - 3; j += 4) {
            vfloat redv, greenv, bluev;
            vconvertrgbrgbrgbrgb2rrrrggggbbbb(rgb[i * width + j], redv, greenv, bluev);
            vfloat xyz0v = redv * xyz_camv[0][0] + greenv * xyz_camv[0][1] + bluev * xyz_camv[0][2];
            vfloat xyz1v = redv * xyz_camv[1][0] + greenv * xyz_camv[1][1] + bluev * xyz_camv[1][2];
            vfloat xyz2v = redv * xyz_camv[2][0] + greenv * xyz_camv[2][1] + bluev * xyz_camv[2][2];
            xyz0v = cbrt[_mm_cvtps_epi32(xyz0v)];
            xyz1v = cbrt[_mm_cvtps_epi32(xyz1v)];
            xyz2v = cbrt[_mm_cvtps_epi32(xyz2v)];

            STVFU(l[i * labWidth + j], c116v * xyz1v - c16v);
            STVFU(a[i * labWidth + j], c500v * (xyz0v - xyz1v));
            STVFU(b[i * labWidth + j], c200v * (xyz1v - xyz2v));
        }

#endif

        for(; j < labWidth; j++) {
            float xyz[3] = {0.5f, 0.5f, 0.5f};

            for(int c = 0; c < 3; c++) {
                float val = rgb[i * width + j][c];
                xyz[0] += xyz_cam[0][c] * val;
                xyz[1] += xyz_cam[1][c] * val;
                xyz[2] += xyz_cam[2][c] * val;
            }

            xyz[0] = cbrt[(int) xyz[0]];
            xyz[1] = cbrt[(int) xyz[1]];
            xyz[2] = cbrt[(int) xyz[2]];

            l[i * labWidth + j] = 116 * xyz[1] - 16;
            a[i * labWidth + j] = 500 * (xyz[0] - xyz[1]);
            b[i * labWidth + j] = 200 * (xyz[1] - xyz[2]);
        }
    }
}


#define fcol(row,col) xtrans[(row)%6][(col)%6]
#define isgreen(row,col) (xtrans[(row)%3][(col)%3]&1)

namespace
{

void xtransborder_interpolate_for_cfa(
    int border,
    int width,
    int height,
    const int xtrans[6][6],
    const array2D<float> &rawData,
    array2D<float> &red,
    array2D<float> &green,
    array2D<float> &blue)
{
    const float weight[3][3] = {
                                {0.25f, 0.5f, 0.25f},
                                {0.5f,  0.f,  0.5f},
                                {0.25f, 0.5f, 0.25f}
                               };

    for (int row = 0; row < height; row++)
        for (int col = 0; col < width; col++) {
            if (col == border && row >= border && row < height - border) {
                col = width - border;
            }

            float sum[6] = {0.f};

            for (int y = MAX(0, row - 1), v = row == 0 ? 0 : -1; y <= MIN(row + 1, height - 1); y++, v++)
                for (int x = MAX(0, col - 1), h = col == 0 ? 0 : -1; x <= MIN(col + 1, width - 1); x++, h++) {
                    int f = fcol(y, x);
                    sum[f] += rawData[y][x] * weight[v + 1][h + 1];
                    sum[f + 3] += weight[v + 1][h + 1];
                }

            switch(fcol(row, col)) {
            case 0:
                red[row][col] = rawData[row][col];
                green[row][col] = (sum[1] / sum[4]);
                blue[row][col] = (sum[2] / sum[5]);
                break;

            case 1:
                if(sum[3] == 0.f) { // at the 4 corner pixels it can happen, that we have only green pixels in 2x2 area
                    red[row][col] = green[row][col] = blue[row][col] = rawData[row][col];
                } else {
                    red[row][col] = (sum[0] / sum[3]);
                    green[row][col] = rawData[row][col];
                    blue[row][col] = (sum[2] / sum[5]);
                }

                break;

            case 2:
                red[row][col] = (sum[0] / sum[3]);
                green[row][col] = (sum[1] / sum[4]);
                blue[row][col] = rawData[row][col];
            }
        }
}

} // namespace

void RawImageSource::xtransborder_interpolate (int border, array2D<float> &red, array2D<float> &green, array2D<float> &blue)
{
    int xtrans[6][6];
    ri->getXtransMatrix(xtrans);
    xtransborder_interpolate_for_cfa(
        border, W, H, xtrans, rawData, red, green, blue);
}

/*
   Frank Markesteijn's algorithm for Fuji X-Trans sensors
   adapted to RT by Ingo Weyrich 2014
*/
// override CLIP function to test unclipped output
#define CLIP(x) (x)
void RawImageSource::xtrans_interpolate (const int passes, const bool useCieLab, size_t chunkSize, bool measure)
{
    xtrans_interpolate_impl(
        passes, useCieLab, chunkSize, measure, nullptr, nullptr);
}

void RawImageSource::xtrans_interpolate_impl(
    const int passes,
    const bool useCieLab,
    size_t chunkSize,
    bool measure,
    const int overrideXtrans[6][6],
    const float overrideRgbCam[3][3])
{

    std::unique_ptr<StopWatch> stop;

    if (measure) {
        std::cout << passes << "-pass Xtrans Demosaicing " << W << "x" << H << " image with " << chunkSize << " tiles per thread" << std::endl;
        stop.reset(new StopWatch("xtrans demosaic"));
    }

    constexpr int ts = 114;      /* Tile Size */
    constexpr int tsh = ts / 2;  /* half of Tile Size */

    double progress = 0.0;
    const bool plistenerActive = plistener;

    if (plistenerActive) {
        plistener->setProgressStr (Glib::ustring::compose(M("TP_RAW_DMETHOD_PROGRESSBAR"), M("TP_RAW_XTRANS")));
        plistener->setProgress (progress);
    }

    int xtrans[6][6];
    if (overrideXtrans) {
        std::memcpy(xtrans, overrideXtrans, sizeof(xtrans));
    } else {
        ri->getXtransMatrix(xtrans);
    }

    constexpr short  orth[12] = { 1, 0, 0, 1, -1, 0, 0, -1, 1, 0, 0, 1 },
    patt[2][16] = { { 0, 1, 0, -1, 2, 0, -1, 0, 1, 1, 1, -1, 0, 0, 0, 0 },
        { 0, 1, 0, -2, 1, 0, -2, 0, 1, 1, -2, -2, 1, -1, -1, 1 }
    },
    dir[4] = { 1, ts, ts + 1, ts - 1 };

    // sgrow/sgcol is the offset in the sensor matrix of the solitary
    // green pixels
    ushort sgrow = 0, sgcol = 0;

    const int height = H, width = W;

    float xyz_cam[3][3];
    {
        float rgb_cam[3][4] = {};
        if (overrideRgbCam) {
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    rgb_cam[row][col] = overrideRgbCam[row][col];
                }
            }
        } else {
            ri->getRgbCam(rgb_cam);
        }
        int k;

        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                for (xyz_cam[i][j] = k = 0; k < 3; k++) {
                    xyz_cam[i][j] += xyz_rgb[i][k] * rgb_cam[k][j] / d65_white[i];
                }
    }

    /* Map a green hexagon around each non-green pixel and vice versa:  */
    short allhex[2][3][3][8];
    {
        int gint, d, h, v, ng, row, col;

        for (row = 0; row < 3; row++)
            for (col = 0; col < 3; col++) {
                gint = isgreen(row, col);

                for (ng = d = 0; d < 10; d += 2) {
                    if (isgreen(row + orth[d] + 6, col + orth[d + 2] + 6)) {
                        ng = 0;
                    } else {
                        ng++;
                    }

                    if (ng == 4) {
                        // if there are four non-green pixels adjacent in cardinal
                        // directions, this is the solitary green pixel
                        sgrow = row;
                        sgcol = col;
                    }

                    if (ng == gint + 1) {
                        for (int c = 0; c < 8; c++) {
                            v = orth[d] * patt[gint][c * 2] + orth[d + 1] * patt[gint][c * 2 + 1];
                            h = orth[d + 2] * patt[gint][c * 2] + orth[d + 3] * patt[gint][c * 2 + 1];
                            allhex[0][row][col][c ^ (gint * 2 & d)] = h + v * width;
                            allhex[1][row][col][c ^ (gint * 2 & d)] = h + v * ts;
                        }
                    }
                }
            }

    }

    if(plistenerActive) {
        progress += 0.05;
        plistener->setProgress(progress);
    }


    double progressInc = 36.0 * (1.0 - progress) / ((H * W) / ((ts - 16) * (ts - 16)));
    const int ndir = 4 << (passes > 1);
    cielab (nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr);
    struct s_minmaxgreen {
        float min;
        float max;
    };

    int RightShift[3];

    for(int row = 0; row < 3; row++) {
        // count number of green pixels in three cols
        int greencount = 0;

        for(int col = 0; col < 3; col++) {
            greencount += isgreen(row, col);
        }

        RightShift[row] = (greencount == 2);
    }

#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        int progressCounter = 0;

        float *buffer = (float *) malloc ((ts * ts * (ndir * 4 + 3) + 128) * sizeof(float));
        float (*rgb)[ts][ts][3] = (float(*)[ts][ts][3]) buffer;
        float (*lab)[ts - 8][ts - 8] = (float (*)[ts - 8][ts - 8])(buffer + ts * ts * (ndir * 3));
        float (*drv)[ts - 10][ts - 10] = (float (*)[ts - 10][ts - 10])   (buffer + ts * ts * (ndir * 3 + 3));
        uint8_t (*homo)[ts][ts] = (uint8_t  (*)[ts][ts])   (lab); // we can reuse the lab-buffer because they are not used together
        s_minmaxgreen  (*greenminmaxtile)[tsh] = (s_minmaxgreen(*)[tsh]) (lab); // we can reuse the lab-buffer because they are not used together
        uint8_t (*homosum)[ts][ts] = (uint8_t (*)[ts][ts]) (drv); // we can reuse the drv-buffer because they are not used together
        uint8_t (*homosummax)[ts] = (uint8_t (*)[ts]) homo[ndir - 1]; // we can reuse the homo-buffer because they are not used together

#ifdef _OPENMP
        #pragma omp for collapse(2) schedule(dynamic, chunkSize) nowait
#endif

        for (int top = 3; top < height - 19; top += ts - 16)
            for (int left = 3; left < width - 19; left += ts - 16) {
                int mrow = MIN (top + ts, height - 3);
                int mcol = MIN (left + ts, width - 3);

                /* Set greenmin and greenmax to the minimum and maximum allowed values: */
                for (int row = top; row < mrow; row++) {
                    // find first non-green pixel
                    int leftstart = left;

                    for(; leftstart < mcol; leftstart++)
                        if(!isgreen(row, leftstart)) {
                            break;
                        }

                    int coloffset = (RightShift[row % 3] == 1 ? 3 : 1 + (fcol(row, leftstart + 1) & 1));

                    if(coloffset == 3) {
                        short *hex = allhex[0][row % 3][leftstart % 3];

                        for (int col = leftstart; col < mcol; col += coloffset) {
                            float minval = FLT_MAX;
                            float maxval = 0.f;
                            float *pix = &rawData[row][col];

                            for(int c = 0; c < 6; c++) {
                                float val = pix[hex[c]];

                                minval = minval < val ? minval : val;
                                maxval = maxval > val ? maxval : val;
                            }

                            greenminmaxtile[row - top][(col - left) >> 1].min = minval;
                            greenminmaxtile[row - top][(col - left) >> 1].max = maxval;
                        }
                    } else {
                        float minval = FLT_MAX;
                        float maxval = 0.f;
                        int col = leftstart;

                        if(coloffset == 2) {
                            minval = FLT_MAX;
                            maxval = 0.f;
                            float *pix = &rawData[row][col];
                            short *hex = allhex[0][row % 3][col % 3];

                            for(int c = 0; c < 6; c++) {
                                float val = pix[hex[c]];

                                minval = minval < val ? minval : val;
                                maxval = maxval > val ? maxval : val;
                            }

                            greenminmaxtile[row - top][(col - left) >> 1].min = minval;
                            greenminmaxtile[row - top][(col - left) >> 1].max = maxval;
                            col += 2;
                        }

                        short *hex = allhex[0][row % 3][col % 3];

                        for (; col < mcol - 1; col += 3) {
                            minval = FLT_MAX;
                            maxval = 0.f;
                            float *pix = &rawData[row][col];

                            for(int c = 0; c < 6; c++) {
                                float val = pix[hex[c]];

                                minval = minval < val ? minval : val;
                                maxval = maxval > val ? maxval : val;
                            }

                            greenminmaxtile[row - top][(col - left) >> 1].min = minval;
                            greenminmaxtile[row - top][(col - left) >> 1].max = maxval;
                            greenminmaxtile[row - top][(col + 1 - left) >> 1].min = minval;
                            greenminmaxtile[row - top][(col + 1 - left) >> 1].max = maxval;
                        }

                        if(col < mcol) {
                            minval = FLT_MAX;
                            maxval = 0.f;
                            float *pix = &rawData[row][col];

                            for(int c = 0; c < 6; c++) {
                                float val = pix[hex[c]];

                                minval = minval < val ? minval : val;
                                maxval = maxval > val ? maxval : val;
                            }

                            greenminmaxtile[row - top][(col - left) >> 1].min = minval;
                            greenminmaxtile[row - top][(col - left) >> 1].max = maxval;
                        }
                    }
                }

                memset(rgb, 0, ts * ts * 3 * sizeof(float));

                for (int row = top; row < mrow; row++)
                    for (int col = left; col < mcol; col++) {
                        rgb[0][row - top][col - left][fcol(row, col)] = rawData[row][col];
                    }

                for(int c = 0; c < 3; c++) {
                    memcpy (rgb[c + 1], rgb[0], sizeof * rgb);
                }

                /* Interpolate green horizontally, vertically, and along both diagonals: */
                for (int row = top; row < mrow; row++) {
                    // find first non-green pixel
                    int leftstart = left;

                    for(; leftstart < mcol; leftstart++)
                        if(!isgreen(row, leftstart)) {
                            break;
                        }

                    int coloffset = (RightShift[row % 3] == 1 ? 3 : 1 + (fcol(row, leftstart + 1) & 1));

                    if(coloffset == 3) {
                        short *hex = allhex[0][row % 3][leftstart % 3];

                        for (int col = leftstart; col < mcol; col += coloffset) {
                            float *pix = &rawData[row][col];
                            float color[4];
                            color[0] = 0.6796875f * (pix[hex[1]] + pix[hex[0]]) -
                                       0.1796875f * (pix[2 * hex[1]] + pix[2 * hex[0]]);
                            color[1] = 0.87109375f *  pix[hex[3]] + pix[hex[2]] * 0.12890625f +
                                       0.359375f * (pix[0] - pix[-hex[2]]);

                            for(int c = 0; c < 2; c++)
                                color[2 + c] = 0.640625f * pix[hex[4 + c]] + 0.359375f * pix[-2 * hex[4 + c]] + 0.12890625f *
                                               (2.f * pix[0] - pix[3 * hex[4 + c]] - pix[-3 * hex[4 + c]]);

                            for(int c = 0; c < 4; c++) {
                                rgb[c][row - top][col - left][1] = LIM(color[c], greenminmaxtile[row - top][(col - left) >> 1].min, greenminmaxtile[row - top][(col - left) >> 1].max);
                            }
                        }
                    } else {
                        short *hexmod[2];
                        hexmod[0] = allhex[0][row % 3][leftstart % 3];
                        hexmod[1] = allhex[0][row % 3][(leftstart + coloffset) % 3];

                        for (int col = leftstart, hexindex = 0; col < mcol; col += coloffset, coloffset ^= 3, hexindex ^= 1) {
                            float *pix = &rawData[row][col];
                            short *hex = hexmod[hexindex];
                            float color[4];
                            color[0] = 0.6796875f * (pix[hex[1]] + pix[hex[0]]) -
                                       0.1796875f * (pix[2 * hex[1]] + pix[2 * hex[0]]);
                            color[1] = 0.87109375f *  pix[hex[3]] + pix[hex[2]] * 0.12890625f +
                                       0.359375f * (pix[0] - pix[-hex[2]]);

                            for(int c = 0; c < 2; c++)
                                color[2 + c] = 0.640625f * pix[hex[4 + c]] + 0.359375f * pix[-2 * hex[4 + c]] + 0.12890625f *
                                               (2.f * pix[0] - pix[3 * hex[4 + c]] - pix[-3 * hex[4 + c]]);

                            for(int c = 0; c < 4; c++) {
                                rgb[c ^ 1][row - top][col - left][1] = LIM(color[c], greenminmaxtile[row - top][(col - left) >> 1].min, greenminmaxtile[row - top][(col - left) >> 1].max);
                            }
                        }
                    }
                }

                for (int pass = 0; pass < passes; pass++) {
                    if (pass == 1) {
                        memcpy (rgb += 4, buffer, 4 * sizeof * rgb);
                    }

                    /* Recalculate green from interpolated values of closer pixels: */
                    if (pass) {
                        for (int row = top + 2; row < mrow - 2; row++) {
                            int leftstart = left + 2;

                            for(; leftstart < mcol - 2; leftstart++)
                                if(!isgreen(row, leftstart)) {
                                    break;
                                }

                            int coloffset = (RightShift[row % 3] == 1 ? 3 : 1 + (fcol(row, leftstart + 1) & 1));

                            if(coloffset == 3) {
                                int f = fcol(row, leftstart);
                                short *hex = allhex[1][row % 3][leftstart % 3];

                                for (int col = leftstart; col < mcol - 2; col += coloffset, f ^= 2) {
                                    for (int d = 3; d < 6; d++) {
                                        float (*rix)[3] = &rgb[(d - 2)][row - top][col - left];
                                        float val = 0.33333333f * (rix[-2 * hex[d]][1] + 2 * (rix[hex[d]][1] - rix[hex[d]][f])
                                                                   - rix[-2 * hex[d]][f]) + rix[0][f];
                                        rix[0][1] = LIM(val, greenminmaxtile[row - top][(col - left) >> 1].min, greenminmaxtile[row - top][(col - left) >> 1].max);
                                    }
                                }
                            } else {
                                int f = fcol(row, leftstart);
                                short *hexmod[2];
                                hexmod[0] = allhex[1][row % 3][leftstart % 3];
                                hexmod[1] = allhex[1][row % 3][(leftstart + coloffset) % 3];

                                for (int col = leftstart, hexindex = 0; col < mcol - 2; col += coloffset, coloffset ^= 3, f = f ^ (coloffset & 2), hexindex ^= 1 ) {
                                    short *hex = hexmod[hexindex];

                                    for (int d = 3; d < 6; d++) {
                                        float (*rix)[3] = &rgb[(d - 2) ^ 1][row - top][col - left];
                                        float val = 0.33333333f * (rix[-2 * hex[d]][1] + 2 * (rix[hex[d]][1] - rix[hex[d]][f])
                                                                   - rix[-2 * hex[d]][f]) + rix[0][f];
                                        rix[0][1] = LIM(val, greenminmaxtile[row - top][(col - left) >> 1].min, greenminmaxtile[row - top][(col - left) >> 1].max);
                                    }
                                }
                            }
                        }
                    }

                    /* Interpolate red and blue values for solitary green pixels:   */
                    int sgstartcol = (left - sgcol + 4) / 3 * 3 + sgcol;
                    float color[3][6];

                    for (int row = (top - sgrow + 4) / 3 * 3 + sgrow; row < mrow - 2; row += 3) {
                        for (int col = sgstartcol, h = fcol(row, col + 1); col < mcol - 2; col += 3, h ^= 2) {
                            float (*rix)[3] = &rgb[0][row - top][col - left];
                            float diff[6] = {0.f};

                            for (int i = 1, d = 0; d < 6; d++, i ^= ts ^ 1, h ^= 2) {
                                for (int c = 0; c < 2; c++, h ^= 2) {
                                    float g = rix[0][1] + rix[0][1] - rix[i << c][1] - rix[-i << c][1];
                                    color[h][d] = g + rix[i << c][h] + rix[-i << c][h];

                                    if (d > 1)
                                        diff[d] += SQR (rix[i << c][1] - rix[-i << c][1]
                                                        - rix[i << c][h] + rix[-i << c][h]) + SQR(g);
                                }

                                if (d > 2 && (d & 1))    // 3, 5
                                    if (diff[d - 1] < diff[d])
                                        for(int c = 0; c < 2; c++) {
                                            color[c * 2][d] = color[c * 2][d - 1];
                                        }

                                if ((d & 1) || d < 2) { // d: 0, 1, 3, 5
                                    for(int c = 0; c < 2; c++) {
                                        rix[0][c * 2] = CLIP(0.5f * color[c * 2][d]);
                                    }

                                    rix += ts * ts;
                                }
                            }
                        }
                    }

                    /* Interpolate red for blue pixels and vice versa:      */
                    for (int row = top + 3; row < mrow - 3; row++) {
                        int leftstart = left + 3;

                        for(; leftstart < mcol - 1; leftstart++)
                            if(!isgreen(row, leftstart)) {
                                break;
                            }

                        int coloffset = (RightShift[row % 3] == 1 ? 3 : 1);
                        int c = ((row - sgrow) % 3) ? ts : 1;
                        int h = 3 * (c ^ ts ^ 1);

                        if(coloffset == 3) {
                            int f = 2 - fcol(row, leftstart);

                            for (int col = leftstart; col < mcol - 3; col += coloffset, f ^= 2) {
                                float (*rix)[3] = &rgb[0][row - top][col - left];

                                for (int d = 0; d < 4; d++, rix += ts * ts) {
                                    int i = d > 1 || ((d ^ c) & 1) ||
                                            ((fabsf(rix[0][1] - rix[c][1]) + fabsf(rix[0][1] - rix[-c][1])) < 2.f * (fabsf(rix[0][1] - rix[h][1]) + fabsf(rix[0][1] - rix[-h][1]))) ? c : h;

                                    rix[0][f] = CLIP(rix[0][1] + 0.5f * (rix[i][f] + rix[-i][f] - rix[i][1] - rix[-i][1]));
                                }
                            }
                        } else {
                            coloffset = fcol(row, leftstart + 1) == 1 ? 2 : 1;
                            int f = 2 - fcol(row, leftstart);

                            for (int col = leftstart; col < mcol - 3; col += coloffset, coloffset ^= 3, f = f ^ (coloffset & 2) ) {
                                float (*rix)[3] = &rgb[0][row - top][col - left];

                                for (int d = 0; d < 4; d++, rix += ts * ts) {
                                    int i = d > 1 || ((d ^ c) & 1) ||
                                            ((fabsf(rix[0][1] - rix[c][1]) + fabsf(rix[0][1] - rix[-c][1])) < 2.f * (fabsf(rix[0][1] - rix[h][1]) + fabsf(rix[0][1] - rix[-h][1]))) ? c : h;

                                    rix[0][f] = CLIP(rix[0][1] + 0.5f * (rix[i][f] + rix[-i][f] - rix[i][1] - rix[-i][1]));
                                }
                            }
                        }
                    }

                    /* Fill in red and blue for 2x2 blocks of green:        */
                    // Find first row of 2x2 green
                    int topstart = top + 2;

                    for(; topstart < mrow - 2; topstart++)
                        if((topstart - sgrow) % 3) {
                            break;
                        }

                    int leftstart = left + 2;

                    for(; leftstart < mcol - 2; leftstart++)
                        if((leftstart - sgcol) % 3) {
                            break;
                        }

                    int coloffsetstart = 2 - (fcol(topstart, leftstart + 1) & 1);

                    for (int row = topstart; row < mrow - 2; row++) {
                        if ((row - sgrow) % 3) {
                            short *hexmod[2];
                            hexmod[0] = allhex[1][row % 3][leftstart % 3];
                            hexmod[1] = allhex[1][row % 3][(leftstart + coloffsetstart) % 3];

                            for (int col = leftstart, coloffset = coloffsetstart, hexindex = 0; col < mcol - 2; col += coloffset, coloffset ^= 3, hexindex ^= 1) {
                                float (*rix)[3] = &rgb[0][row - top][col - left];
                                short *hex = hexmod[hexindex];

                                for (int d = 0; d < ndir; d += 2, rix += ts * ts) {
                                    if (hex[d] + hex[d + 1]) {
                                        float g = 3 * rix[0][1] - 2 * rix[hex[d]][1] - rix[hex[d + 1]][1];

                                        for (int c = 0; c < 4; c += 2) {
                                            rix[0][c] = CLIP((g + 2 * rix[hex[d]][c] + rix[hex[d + 1]][c]) * 0.33333333f);
                                        }
                                    } else {
                                        float g = 2 * rix[0][1] - rix[hex[d]][1] - rix[hex[d + 1]][1];

                                        for (int c = 0; c < 4; c += 2) {
                                            rix[0][c] = CLIP((g + rix[hex[d]][c] + rix[hex[d + 1]][c]) * 0.5f);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

// end of multipass part
                rgb = (float(*)[ts][ts][3]) buffer;
                mrow -= top;
                mcol -= left;

                if(useCieLab) {
                    /* Convert to CIELab and differentiate in all directions:   */
                    // Original dcraw algorithm uses CIELab as perceptual space
                    // (presumably coming from original AHD) and converts taking
                    // camera matrix into account.  We use this in RT.
                    for (int d = 0; d < ndir; d++) {
                        cielab(&rgb[d][4][4], &lab[0][0][0], &lab[1][0][0], &lab[2][0][0], ts, mrow - 8, ts - 8, xyz_cam);
                        int f = dir[d & 3];
                        f = f == 1 ? 1 : f - 8;

                        for (int row = 5; row < mrow - 5; row++)
#ifdef _OPENMP
                            #pragma omp simd
#endif
                            for (int col = 5; col < mcol - 5; col++) {
                                float *l = &lab[0][row - 4][col - 4];
                                float *a = &lab[1][row - 4][col - 4];
                                float *b = &lab[2][row - 4][col - 4];

                                float g = 2 * l[0] - l[f] - l[-f];
                                drv[d][row - 5][col - 5] =  SQR(g)
                                                            + SQR((2 * a[0] - a[f] - a[-f] + g * 2.1551724f))
                                                            + SQR((2 * b[0] - b[f] - b[-f] - g * 0.86206896f));
                            }

                    }
                } else {
                    // For 1-pass demosaic we use YPbPr which requires much
                    // less code and is nearly indistinguishable. It assumes the
                    // camera RGB is roughly linear.
                    for (int d = 0; d < ndir; d++) {
                        float (*yuv)[ts - 8][ts - 8] = lab; // we use the lab buffer, which has the same dimensions
#if defined(__SSE2__) || defined(RT_SIMDE)
                        vfloat zd2627v = F2V(0.2627f);
                        vfloat zd6780v = F2V(0.6780f);
                        vfloat zd0593v = F2V(0.0593f);
                        vfloat zd56433v = F2V(0.56433f);
                        vfloat zd67815v = F2V(0.67815f);
#endif

                        for (int row = 4; row < mrow - 4; row++) {
                            int col = 4;
#if defined(__SSE2__) || defined(RT_SIMDE)

                            for (; col < mcol - 7; col += 4) {
                                // use ITU-R BT.2020 YPbPr, which is great, but could use
                                // a better/simpler choice? note that imageop.h provides
                                // dt_iop_RGB_to_YCbCr which uses Rec. 601 conversion,
                                // which appears less good with specular highlights
                                vfloat redv, greenv, bluev;
                                vconvertrgbrgbrgbrgb2rrrrggggbbbb(rgb[d][row][col], redv, greenv, bluev);
                                vfloat yv = zd2627v * redv + zd6780v * greenv + zd0593v * bluev;
                                STVFU(yuv[0][row - 4][col - 4], yv);
                                STVFU(yuv[1][row - 4][col - 4], (bluev - yv) * zd56433v);
                                STVFU(yuv[2][row - 4][col - 4], (redv - yv) * zd67815v);
                            }

#endif

                            for (; col < mcol - 4; col++) {
                                // use ITU-R BT.2020 YPbPr, which is great, but could use
                                // a better/simpler choice? note that imageop.h provides
                                // dt_iop_RGB_to_YCbCr which uses Rec. 601 conversion,
                                // which appears less good with specular highlights
                                float y = 0.2627f * rgb[d][row][col][0] + 0.6780f * rgb[d][row][col][1] + 0.0593f * rgb[d][row][col][2];
                                yuv[0][row - 4][col - 4] = y;
                                yuv[1][row - 4][col - 4] = (rgb[d][row][col][2] - y) * 0.56433f;
                                yuv[2][row - 4][col - 4] = (rgb[d][row][col][0] - y) * 0.67815f;
                            }
                        }

                        int f = dir[d & 3];
                        f = f == 1 ? 1 : f - 8;

                        for (int row = 5; row < mrow - 5; row++)
                            for (int col = 5; col < mcol - 5; col++) {
                                float *y = &yuv[0][row - 4][col - 4];
                                float *u = &yuv[1][row - 4][col - 4];
                                float *v = &yuv[2][row - 4][col - 4];
                                drv[d][row - 5][col - 5] = SQR(2 * y[0] - y[f] - y[-f])
                                                           + SQR(2 * u[0] - u[f] - u[-f])
                                                           + SQR(2 * v[0] - v[f] - v[-f]);
                            }
                    }
                }

                /* Build homogeneity maps from the derivatives:         */
#if defined(__SSE2__) || defined(RT_SIMDE)
                vfloat eightv = F2V(8.f);
                vfloat zerov = F2V(0.f);
                vfloat onev = F2V(1.f);
#endif

                for (int row = 6; row < mrow - 6; row++) {
                    int col = 6;
#if defined(__SSE2__) || defined(RT_SIMDE)

                    for (; col < mcol - 9; col += 4) {
                        vfloat tr1v = vminf(LVFU(drv[0][row - 5][col - 5]), LVFU(drv[1][row - 5][col - 5]));
                        vfloat tr2v = vminf(LVFU(drv[2][row - 5][col - 5]), LVFU(drv[3][row - 5][col - 5]));

                        if(ndir > 4) {
                            vfloat tr3v = vminf(LVFU(drv[4][row - 5][col - 5]), LVFU(drv[5][row - 5][col - 5]));
                            vfloat tr4v = vminf(LVFU(drv[6][row - 5][col - 5]), LVFU(drv[7][row - 5][col - 5]));
                            tr1v = vminf(tr1v, tr3v);
                            tr1v = vminf(tr1v, tr4v);
                        }

                        tr1v = vminf(tr1v, tr2v);
                        tr1v = tr1v * eightv;

                        for (int d = 0; d < ndir; d++) {
                            uint8_t tempstore[16];
                            vfloat tempv = zerov;

                            for (int v = -1; v <= 1; v++) {
                                for (int h = -1; h <= 1; h++) {
                                    tempv += vselfzero(vmaskf_le(LVFU(drv[d][row + v - 5][col + h - 5]), tr1v), onev);
                                }
                            }

                            _mm_storeu_si128((__m128i*)&tempstore, _mm_cvtps_epi32(tempv));
                            homo[d][row][col] = tempstore[0];
                            homo[d][row][col + 1] = tempstore[4];
                            homo[d][row][col + 2] = tempstore[8];
                            homo[d][row][col + 3] = tempstore[12];

                        }
                    }

#endif

                    for (; col < mcol - 6; col++) {
                        float tr = drv[0][row - 5][col - 5] < drv[1][row - 5][col - 5] ? drv[0][row - 5][col - 5] : drv[1][row - 5][col - 5];

                        for (int d = 2; d < ndir; d++) {
                            tr = (drv[d][row - 5][col - 5] < tr ? drv[d][row - 5][col - 5] : tr);
                        }

                        tr *= 8;

                        for (int d = 0; d < ndir; d++) {
                            uint8_t temp = 0;

                            for (int v = -1; v <= 1; v++) {
                                for (int h = -1; h <= 1; h++) {
                                    temp += (drv[d][row + v - 5][col + h - 5] <= tr ? 1 : 0);
                                }
                            }

                            homo[d][row][col] = temp;
                        }
                    }
                }

                if (height - top < ts + 4) {
                    mrow = height - top + 2;
                }

                if (width - left < ts + 4) {
                    mcol = width - left + 2;
                }


                /* Build 5x5 sum of homogeneity maps */
                const int startcol = MIN(left, 8);

                for(int d = 0; d < ndir; d++) {
                    for (int row = MIN(top, 8); row < mrow - 8; row++) {
                        int col = startcol;
#if defined(__SSE2__) || defined(RT_SIMDE)
                        int endcol = row < mrow - 9 ? mcol - 8 : mcol - 23;

                        // crunching 16 values at once is faster than summing up column sums
                        for (; col < endcol; col += 16) {
                            vint v5sumv = (vint)ZEROV;

                            for(int v = -2; v <= 2; v++)
                                for(int h = -2; h <= 2; h++) {
                                    v5sumv = _mm_adds_epu8( _mm_loadu_si128((vint*)&homo[d][row + v][col + h]), v5sumv);
                                }

                            _mm_storeu_si128((vint*)&homosum[d][row][col], v5sumv);
                        }

#endif

                        if(col < mcol - 8) {
                            int v5sum[5] = {0};

                            for(int v = -2; v <= 2; v++)
                                for(int h = -2; h <= 2; h++) {
                                    v5sum[2 + h] += homo[d][row + v][col + h];
                                }

                            int blocksum = v5sum[0] + v5sum[1] + v5sum[2] + v5sum[3] + v5sum[4];
                            homosum[d][row][col] = blocksum;
                            col++;

                            // now we can subtract a column of five from blocksum and get new colsum of 5
                            for (int voffset = 0; col < mcol - 8; col++, voffset++) {
                                int colsum = homo[d][row - 2][col + 2] + homo[d][row - 1][col + 2] + homo[d][row][col + 2] + homo[d][row + 1][col + 2] + homo[d][row + 2][col + 2];
                                voffset = voffset == 5 ? 0 : voffset;  // faster than voffset %= 5;
                                blocksum -= v5sum[voffset];
                                blocksum += colsum;
                                v5sum[voffset] = colsum;
                                homosum[d][row][col] = blocksum;
                            }
                        }
                    }
                }

                // calculate maximum of homogeneity maps per pixel. Vectorized calculation is a tiny bit faster than on the fly calculation in next step
#if defined(__SSE2__) || defined(RT_SIMDE)
                vint maskv = _mm_set1_epi8(31);
#endif

                for (int row = MIN(top, 8); row < mrow - 8; row++) {
                    int col = startcol;
#if defined(__SSE2__) || defined(RT_SIMDE)
                    int endcol = row < mrow - 9 ? mcol - 8 : mcol - 23;

                    for (; col < endcol; col += 16) {
                        vint maxval1 = _mm_max_epu8(_mm_loadu_si128((vint*)&homosum[0][row][col]), _mm_loadu_si128((vint*)&homosum[1][row][col]));
                        vint maxval2 = _mm_max_epu8(_mm_loadu_si128((vint*)&homosum[2][row][col]), _mm_loadu_si128((vint*)&homosum[3][row][col]));

                        if(ndir > 4) {
                            vint maxval3 = _mm_max_epu8(_mm_loadu_si128((vint*)&homosum[4][row][col]), _mm_loadu_si128((vint*)&homosum[5][row][col]));
                            vint maxval4 = _mm_max_epu8(_mm_loadu_si128((vint*)&homosum[6][row][col]), _mm_loadu_si128((vint*)&homosum[7][row][col]));
                            maxval1 = _mm_max_epu8(maxval1, maxval3);
                            maxval1 = _mm_max_epu8(maxval1, maxval4);
                        }

                        maxval1 = _mm_max_epu8(maxval1, maxval2);
                        // there is no shift intrinsic for epu8. Shift using epi32 and mask the wrong bits out
                        vint subv = _mm_srli_epi32( maxval1, 3 );
                        subv = _mm_and_si128(subv, maskv);
                        maxval1 = _mm_subs_epu8(maxval1, subv);
                        _mm_storeu_si128((vint*)&homosummax[row][col], maxval1);
                    }

#endif

                    for (; col < mcol - 8; col ++) {
                        uint8_t maxval = homosum[0][row][col];

                        for(int d = 1; d < ndir; d++) {
                            maxval = maxval < homosum[d][row][col] ? homosum[d][row][col] : maxval;
                        }

                        maxval -= maxval >> 3;
                        homosummax[row][col] = maxval;
                    }
                }


                /* Average the most homogeneous pixels for the final result: */
                uint8_t hm[8] = {};

                for (int row = MIN(top, 8); row < mrow - 8; row++)
                    for (int col = MIN(left, 8); col < mcol - 8; col++) {

                        for (int d = 0; d < 4; d++) {
                            hm[d] = homosum[d][row][col];
                        }

                        for (int d = 4; d < ndir; d++) {
                            hm[d] = homosum[d][row][col];

                            if (hm[d - 4] < hm[d]) {
                                hm[d - 4] = 0;
                            } else if (hm[d - 4] > hm[d]) {
                                hm[d] = 0;
                            }
                        }

                        float avg[4] = {0.f};

                        uint8_t maxval = homosummax[row][col];

                        for (int d = 0; d < ndir; d++)
                            if (hm[d] >= maxval) {
                                for (int c = 0; c < 3; c++) {
                                    avg[c] += rgb[d][row][col][c];
                                }
                                avg[3]++;
                            }

                        red[row + top][col + left] = std::max(0.f, avg[0] / avg[3]);
                        green[row + top][col + left] = std::max(0.f, avg[1] / avg[3]);
                        blue[row + top][col + left] = std::max(0.f, avg[2] / avg[3]);
                    }

                if(plistenerActive && ((++progressCounter) % 32 == 0)) {
#ifdef _OPENMP
                    #pragma omp critical (xtransdemosaic)
#endif
                    {
                        progress += progressInc;
                        progress = min(1.0, progress);
                        plistener->setProgress (progress);
                    }
                }


            }

        free(buffer);
    }

    xtransborder_interpolate_for_cfa(
        passes > 1 ? 8 : 11, W, H, xtrans, rawData, red, green, blue);
}
#undef CLIP

const char *markesteijnXTransErrorCodeName(MarkesteijnXTransErrorCode code)
{
    switch (code) {
        case MarkesteijnXTransErrorCode::NONE: return "NONE";
        case MarkesteijnXTransErrorCode::SIZE: return "SIZE";
        case MarkesteijnXTransErrorCode::CFA: return "CFA";
        case MarkesteijnXTransErrorCode::ALLOCATION: return "ALLOCATION";
        case MarkesteijnXTransErrorCode::NONFINITE: return "NONFINITE";
        case MarkesteijnXTransErrorCode::INTERNAL: return "INTERNAL";
    }
    return "INTERNAL";
}

MarkesteijnXTransRunResult demosaicMarkesteijnXTransReference(
    const float *mosaic,
    float *redOutput,
    float *greenOutput,
    float *blueOutput,
    int width,
    int height,
    const int xtrans[6][6])
{
    if (!mosaic || !redOutput || !greenOutput || !blueOutput ||
            width < 32 || height < 32) {
        return {MarkesteijnXTransErrorCode::SIZE,
                "input and output planes must be non-null and at least 32x32"};
    }
    XTransCfaTransform transform;
    if (!findCanonicalXTransTransform(xtrans, transform)) {
        return {MarkesteijnXTransErrorCode::CFA,
                "unsupported X-Trans color-filter matrix"};
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    for (std::size_t index = 0; index < pixels; ++index) {
        if (!std::isfinite(mosaic[index])) {
            return {MarkesteijnXTransErrorCode::NONFINITE,
                    "mosaic contains a non-finite value"};
        }
    }

    try {
        RawImageSource source;
        source.W = width;
        source.H = height;
        source.rawData(width, height);
        source.red(width, height);
        source.green(width, height);
        source.blue(width, height);
        std::memcpy(static_cast<float *>(source.rawData), mosaic,
                    pixels * sizeof(float));
        constexpr float identity[3][3] = {
            {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}
        };
        source.xtrans_interpolate_impl(3, true, 1, false, xtrans, identity);
        std::memcpy(redOutput, static_cast<float *>(source.red), pixels * sizeof(float));
        std::memcpy(greenOutput, static_cast<float *>(source.green), pixels * sizeof(float));
        std::memcpy(blueOutput, static_cast<float *>(source.blue), pixels * sizeof(float));
    } catch (const std::bad_alloc &) {
        return {MarkesteijnXTransErrorCode::ALLOCATION, "allocation failed"};
    } catch (const std::exception &error) {
        return {MarkesteijnXTransErrorCode::INTERNAL, error.what()};
    } catch (...) {
        return {MarkesteijnXTransErrorCode::INTERNAL, "unknown internal failure"};
    }

    for (std::size_t index = 0; index < pixels; ++index) {
        if (!std::isfinite(redOutput[index]) ||
                !std::isfinite(greenOutput[index]) ||
                !std::isfinite(blueOutput[index])) {
            return {MarkesteijnXTransErrorCode::NONFINITE,
                    "demosaiced output contains a non-finite value"};
        }
    }
    return {};
}

void RawImageSource::fast_xtrans_interpolate (const array2D<float> &rawData, array2D<float> &red, array2D<float> &green, array2D<float> &blue)
{

    if (plistener) {
        plistener->setProgressStr(Glib::ustring::compose(M("TP_RAW_DMETHOD_PROGRESSBAR"), M("TP_RAW_XTRANSFAST")));
        plistener->setProgress(0.0);
    }

    xtransborder_interpolate(1, red, green, blue);
    int xtrans[6][6];
    ri->getXtransMatrix(xtrans);

    const float weight[3][3] = {
                                {0.25f, 0.5f, 0.25f},
                                {0.5f,  0.f,  0.5f},
                                {0.25f, 0.5f, 0.25f}
                               };
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 16)
#endif
    for (int row = 1; row < H - 1; ++row) {
        for (int col = 1; col < W - 1; ++col) {
            float sum[3] = {};

            for (int v = -1; v <= 1; v++) {
                for (int h = -1; h <= 1; h++) {
                    sum[fcol(row + v, col + h)] += rawData[row + v][(col + h)] * weight[v + 1][h + 1];
                }
            }

            switch(fcol(row, col)) {
            case 0: // red pixel
                red[row][col] = rawData[row][col];
                green[row][col] = sum[1] * 0.5f;
                blue[row][col] = sum[2];
                break;

            case 1: // green pixel
                green[row][col] = rawData[row][col];
                if (fcol(row, col - 1) == fcol(row, col + 1)) { // Solitary green pixel always has exactly two direct red and blue neighbors in 3x3 grid
                    red[row][col] = sum[0];
                    blue[row][col] = sum[2];
                } else { // Non solitary green pixel always has one direct and one diagonal red and blue neighbor in 3x3 grid
                    red[row][col] = sum[0] * 1.3333333f;
                    blue[row][col] = sum[2] * 1.3333333f;
                }
                break;

            case 2: // blue pixel
                red[row][col] = sum[0];
                green[row][col] = sum[1] * 0.5f;
                blue[row][col] = rawData[row][col];
                break;
            }
        }
    }

    if (plistener) {
        plistener->setProgress (1.0);
    }
}

void RawImageSource::fast_xtrans_interpolate_blend (const float* const * blend, const array2D<float> &rawData, array2D<float> &red, array2D<float> &green, array2D<float> &blue)
{

    if (plistener) {
        plistener->setProgressStr(Glib::ustring::compose(M("TP_RAW_DMETHOD_PROGRESSBAR"), M("TP_RAW_XTRANSFAST")));
        plistener->setProgress(0.0);
    }

    int xtrans[6][6];
    ri->getXtransMatrix(xtrans);

    const float weight[3][3] = {
                                {0.25f, 0.5f, 0.25f},
                                {0.5f,  0.f,  0.5f},
                                {0.25f, 0.5f, 0.25f}
                               };
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 16)
#endif
    for (int row = 8; row < H - 8; ++row) {
        for (int col = 8; col < W - 8; ++col) {
            float sum[3] = {};

            for (int v = -1; v <= 1; v++) {
                for (int h = -1; h <= 1; h++) {
                    sum[fcol(row + v, col + h)] += rawData[row + v][(col + h)] * weight[v + 1][h + 1];
                }
            }

            switch(fcol(row, col)) {
            case 0: // red pixel
                red[row][col] = intp(blend[row][col], red[row][col], rawData[row][col]);
                green[row][col] = intp(blend[row][col], green[row][col], sum[1] * 0.5f);
                blue[row][col] = intp(blend[row][col], blue[row][col], sum[2]);
                break;

            case 1: // green pixel
                green[row][col] = intp(blend[row][col], green[row][col], rawData[row][col]);
                if (fcol(row, col - 1) == fcol(row, col + 1)) { // Solitary green pixel always has exactly two direct red and blue neighbors in 3x3 grid
                    red[row][col] = intp(blend[row][col], red[row][col], sum[0]);
                    blue[row][col] = intp(blend[row][col], blue[row][col], sum[2]);
                } else { // Non solitary green pixel always has one direct and one diagonal red and blue neighbor in 3x3 grid
                    red[row][col] = intp(blend[row][col], red[row][col], sum[0] * 1.3333333f);
                    blue[row][col] = intp(blend[row][col], blue[row][col], sum[2] * 1.3333333f);
                }
                break;

            case 2: // blue pixel
                red[row][col] = intp(blend[row][col], red[row][col], sum[0]);
                green[row][col] = intp(blend[row][col], green[row][col], sum[1] * 0.5f);
                blue[row][col] = intp(blend[row][col], blue[row][col], rawData[row][col]);
                break;
            }
        }
    }

    if (plistener) {
        plistener->setProgress (1.0);
    }
}
#undef fcol
#undef isgreen

}
