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
#include <tiffio.h>

#include "colortemp.h"
#include "imagefloat.h"
#include "image16.h"
#include "image8.h"
#include "labimage.h"
#include <cstring>
#include "rtengine.h"
#include "iccstore.h"
#include "alignedbuffer.h"
#include "rt_math.h"
#include "color.h"
#include "procparams.h"

using namespace rtengine;

Imagefloat::Imagefloat ()
{
}

Imagefloat::Imagefloat (int w, int h)
{
    allocate (w, h);
}

Imagefloat::~Imagefloat ()
{
}

// Call this method to handle floating points input values of different size
void Imagefloat::setScanline (int row, const unsigned char* buffer, int bps, unsigned int numSamples)
{

    if (data == nullptr) {
        return;
    }

    // The DNG decoder convert to 32 bits float data even if the file contains 16 or 24 bits data.
    // DNG_HalfToFloat and DNG_FP24ToFloat from dcraw.cc can be used to manually convert
    // from 16 and 24 bits to 32 bits float respectively
    switch (sampleFormat) {
    case (IIOSF_FLOAT16): {
        int ix = 0;
        const uint16_t* sbuffer = (const uint16_t*) buffer;

        for (int i = 0; i < width; i++) {
            r(row, i) = 65535.f * DNG_HalfToFloat(sbuffer[ix++]);
            g(row, i) = 65535.f * DNG_HalfToFloat(sbuffer[ix++]);
            b(row, i) = 65535.f * DNG_HalfToFloat(sbuffer[ix++]);
        }

        break;
    }
    //case (IIOSF_FLOAT24):
    case (IIOSF_FLOAT32): {
        int ix = 0;
        const float* sbuffer = (const float*) buffer;

        for (int i = 0; i < width; i++) {
            r(row, i) = 65535.f * sbuffer[ix++];
            g(row, i) = 65535.f * sbuffer[ix++];
            b(row, i) = 65535.f * sbuffer[ix++];
        }

        break;
    }

    case (IIOSF_LOGLUV24):
    case (IIOSF_LOGLUV32): {
        int ix = 0;
        const float* sbuffer = (const float*) buffer;
        float xyzvalues[3], rgbvalues[3];

        for (int i = 0; i < width; i++) {
            xyzvalues[0] = sbuffer[ix++];
            xyzvalues[1] = sbuffer[ix++];
            xyzvalues[2] = sbuffer[ix++];
            // TODO: we may have to handle other color space than sRGB!
            Color::xyz2srgb(xyzvalues[0], xyzvalues[1], xyzvalues[2], rgbvalues[0], rgbvalues[1], rgbvalues[2]);
            r(row, i) = rgbvalues[0];
            g(row, i) = rgbvalues[1];
            b(row, i) = rgbvalues[2];
        }

        break;
    }

    default:
        // Other type are ignored, but could be implemented if necessary
        break;
    }
}


void Imagefloat::getScanline (int row, unsigned char* buffer, int bps, bool isFloat) const
{

    if (data == nullptr) {
        return;
    }

    if (isFloat) {
        if (bps == 32) {
            int ix = 0;
            float* sbuffer = (float*) buffer;
            // agriggio -- assume the image is normalized to [0, 65535]
            for (int i = 0; i < width; i++) {
                sbuffer[ix++] = r(row, i) / 65535.f;
                sbuffer[ix++] = g(row, i) / 65535.f;
                sbuffer[ix++] = b(row, i) / 65535.f;
            }
        } else if (bps == 16) {
            int ix = 0;
            uint16_t* sbuffer = (uint16_t*) buffer;
            // agriggio -- assume the image is normalized to [0, 65535]
            for (int i = 0; i < width; i++) {
                sbuffer[ix++] = DNG_FloatToHalf(r(row, i) / 65535.f);
                sbuffer[ix++] = DNG_FloatToHalf(g(row, i) / 65535.f);
                sbuffer[ix++] = DNG_FloatToHalf(b(row, i) / 65535.f);
            }
        }
    } else {
        unsigned short *sbuffer = (unsigned short *)buffer;
        for (int i = 0, ix = 0; i < width; i++) {
            float ri = r(row, i);
            float gi = g(row, i);
            float bi = b(row, i);
            if (bps == 16) {
                sbuffer[ix++] = CLIP(ri);
                sbuffer[ix++] = CLIP(gi);
                sbuffer[ix++] = CLIP(bi);
            } else if (bps == 8) {
                buffer[ix++] = rtengine::uint16ToUint8Rounded(CLIP(ri));
                buffer[ix++] = rtengine::uint16ToUint8Rounded(CLIP(gi));
                buffer[ix++] = rtengine::uint16ToUint8Rounded(CLIP(bi));
            }
        }
    }
}

Imagefloat* Imagefloat::copy () const
{

    Imagefloat* cp = new Imagefloat (width, height);
    copyData(cp);
    return cp;
}

Imagefloat* Imagefloat::copySubRegion (int x, int y, int width, int height)
{
    Imagefloat* cp = NULL;
    int realWidth  = LIM<int>(x + width,  0, this->width)  - x;
    int realHeight = LIM<int>(y + height, 0, this->height) - y;

    if (realWidth > 0 && realHeight > 0) {
        cp = new Imagefloat (realWidth, realHeight);
        copyData(cp, x, y, realWidth, realHeight);
    }

    return cp;
}

// This is called by the StdImageSource class. We assume that fp images from StdImageSource don't have to deal with gamma
void Imagefloat::getStdImage (const ColorTemp &ctemp, int tran, Imagefloat* image, const PreviewProps &pp) const
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

    int imwidth = image->width; // Destination image
    int imheight = image->height; // Destination image

    if (((tran & TR_ROT) == TR_R90) || ((tran & TR_ROT) == TR_R270)) {
        int swap = imwidth;
        imwidth = imheight;
        imheight = swap;
    }

    int maxx = width; // Source image
    int maxy = height; // Source image
    int mtran = tran & TR_ROT;
    int skip = pp.getSkip();

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

    const auto CLIP0 = [](float v) -> float { return std::max(v, 0.f); };

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
                    // overflow security check
                    if (src_x >= maxx) {
                        dst_x_lim = dst_x;
                        break;
                    }

                    lineR[dst_x] = CLIP0(rm2 * r(src_y, src_x));
                    lineG[dst_x] = CLIP0(gm2 * g(src_y, src_x));
                    lineB[dst_x] = CLIP0(bm2 * b(src_y, src_x));
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
                            rtot += r(src_y + src_sub_y, src_x + src_sub_x);
                            gtot += g(src_y + src_sub_y, src_x + src_sub_x);
                            btot += b(src_y + src_sub_y, src_x + src_sub_x);
                        }

                    // convert back to gamma and clip
                    if (src_sub_width == skip && src_sub_height == skip) {
                        // Common case where the sub-region is complete
                        lineR[dst_x] = CLIP0(rm * rtot);
                        lineG[dst_x] = CLIP0(gm * gtot);
                        lineB[dst_x] = CLIP0(bm * btot);
                    } else {
                        // computing a special factor for this incomplete sub-region
                        float larea = src_sub_width * src_sub_height;
                        lineR[dst_x] = CLIP0(rm2 * rtot / larea);
                        lineG[dst_x] = CLIP0(gm2 * gtot / larea);
                        lineB[dst_x] = CLIP0(bm2 * btot / larea);
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
}

void Imagefloat::fill(const float value, const bool multithread)
{
    const int W = width;
    const int H = height;

#ifdef _OPENMP
#   pragma omp parallel for firstprivate(W, H) schedule(dynamic, 5) if (multithread)
#endif
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            r(y, x) = value;
            g(y, x) = value;
            b(y, x) = value;
        }
    }
}

// From ART.
void Imagefloat::multiply(float factor, bool multithread)
{
    const int W = width;
    const int H = height;
#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat vfactor = F2V(factor);
#endif

#ifdef _OPENMP
#   pragma omp parallel for firstprivate(W, H) schedule(dynamic, 5) if (multithread)
#endif
    for (int y = 0; y < H; y++) {
        int x = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
        for (; x < W-3; x += 4) {
            vfloat rv = LVF(r(y, x));
            vfloat gv = LVF(g(y, x));
            vfloat bv = LVF(b(y, x));
            STVF(r(y, x), rv * vfactor);
            STVF(g(y, x), gv * vfactor);
            STVF(b(y, x), bv * vfactor);
        }
#endif
        for (; x < W; ++x) {
            r(y, x) *= factor;
            g(y, x) *= factor;
            b(y, x) *= factor;
        }
    }
}

void Imagefloat::normalizeFloat(float srcMinVal, float srcMaxVal)
{

    const float scale = MAXVALF / (srcMaxVal - srcMinVal);
    const int w = width;
    const int h = height;

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 5)
#endif

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            r(y, x) = (r(y, x) - srcMinVal) * scale;
            g(y, x) = (g(y, x) - srcMinVal) * scale;
            b(y, x) = (b(y, x) - srcMinVal) * scale;
        }
    }
}

// convert values's range to [0;1] ; this method assumes that the input values's range is [0;65535]
void Imagefloat::normalizeFloatTo1(bool multithread)
{
    multiply(1.f/65535.f, multithread);
}

// convert values's range to [0;65535 ; this method assumes that the input values's range is [0;1]
void Imagefloat::normalizeFloatTo65535(bool multithread)
{
    multiply(65535.f, multithread);
}

// Parallelized transformation; create transform with cmsFLAGS_NOCACHE!
void Imagefloat::ExecCMSTransform(cmsHTRANSFORM hTransform)
{

    // LittleCMS cannot parallelize planar setups -- Hombre: LCMS2.4 can! But it we use this new feature, memory allocation
    // have to be modified too to build temporary buffers that allow multi processor execution
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        AlignedBuffer<float> pBuf(width * 3);

#ifdef _OPENMP
        #pragma omp for schedule(dynamic, 16)
#endif

        for (int y = 0; y < height; y++)
        {
            float *p = pBuf.data, *pR = r(y), *pG = g(y), *pB = b(y);

            for (int x = 0; x < width; x++) {
                *(p++) = *(pR++);
                *(p++) = *(pG++);
                *(p++) = *(pB++);
            }

            cmsDoTransform (hTransform, pBuf.data, pBuf.data, width);

            p = pBuf.data;
            pR = r(y);
            pG = g(y);
            pB = b(y);

            for (int x = 0; x < width; x++) {
                *(pR++) = *(p++);
                *(pG++) = *(p++);
                *(pB++) = *(p++);
            }
        } // End of parallelization
    }
}

// Parallelized transformation; create transform with cmsFLAGS_NOCACHE!
void Imagefloat::ExecCMSTransform(cmsHTRANSFORM hTransform, const LabImage &labImage, int cx, int cy)
{
    // LittleCMS cannot parallelize planar Lab float images
    // so build temporary buffers to allow multi processor execution
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        AlignedBuffer<float> bufferLab(width * 3);
        AlignedBuffer<float> bufferRGB(width * 3);

#ifdef _OPENMP
        #pragma omp for schedule(static)
#endif

        for (int y = cy; y < cy + height; y++)
        {
            float *pRGB, *pR, *pG, *pB;
            float *pLab, *pL, *pa, *pb;

            pLab= bufferLab.data;
            pL = labImage.L[y] + cx;
            pa = labImage.a[y] + cx;
            pb = labImage.b[y] + cx;

            for (int x = 0; x < width; x++) {
                *(pLab++) = *(pL++)  / 327.68f;
                *(pLab++) = *(pa++)  / 327.68f;
                *(pLab++) = *(pb++)  / 327.68f;
            }

            cmsDoTransform (hTransform, bufferLab.data, bufferRGB.data, width);

            pRGB = bufferRGB.data;
            pR = r(y - cy);
            pG = g(y - cy);
            pB = b(y - cy);

            for (int x = 0; x < width; x++) {
                *(pR++) = *(pRGB++);
                *(pG++) = *(pRGB++);
                *(pB++) = *(pRGB++);
            }
        } // End of parallelization
    }
}
