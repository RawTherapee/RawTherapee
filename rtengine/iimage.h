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
#pragma once

#include <utility>
#include <vector>

#include <lcms2.h>

#include "alignedbuffer.h"
#include "coord2d.h"
#include "imagedimensions.h"
#include "LUT.h"
#include "rt_math.h"
#include "procparams.h"
#include "rtgui/threadutils.h"

#define TR_NONE     0
#define TR_R90      1
#define TR_R180     2
#define TR_R270     3
#define TR_VFLIP    4
#define TR_HFLIP    8
#define TR_ROT      3

#define CHECK_BOUNDS 0

namespace Glib
{

class ustring;

}

namespace rtengine
{

namespace procparams
{

struct CoarseTransformParams;

}

class ProgressListener;
class Color;

extern const char sImage8[];
extern const char sImage16[];
extern const char sImagefloat[];

int getCoarseBitMask(const procparams::CoarseTransformParams& coarse);
const LUTf& getigammatab();

enum TypeInterpolation { TI_Nearest, TI_Bilinear };

// --------------------------------------------------------------------
//                       Generic classes
// --------------------------------------------------------------------

class ImageDatas : virtual public ImageDimensions
{
public:
    template<class S, class D>
    void convertTo(S src, D& dst) const
    {
        dst = src;
    }

    // parameters that will never be used, replaced by the subclasses r, g and b parameters!
    // they are still necessary to implement operator() in this parent class
    virtual ~ImageDatas() {}
    virtual void allocate (int W, int H) {}
    virtual void rotate (int deg) {}
    // free the memory allocated for the image data without deleting the object.
    virtual void flushData ()
    {
        allocate(0, 0);
    }

    virtual void hflip () {}
    virtual void vflip () {}

    // Read the raw dump of the data
    void readData  (FILE *fh) {}
    // Write a raw dump of the data
    void writeData (FILE *fh) const {}

    virtual void normalizeInt (int srcMinVal, int srcMaxVal) {};
    virtual void normalizeFloat (float srcMinVal, float srcMaxVal) {};
    virtual void computeHistogramAutoWB (double &avg_r, double &avg_g, double &avg_b, int &n, LUTu &histogram, int compression) const {}
    virtual void getSpotWBData (double &reds, double &greens, double &blues, int &rn, int &gn, int &bn,
                                std::vector<Coord2D> &red, std::vector<Coord2D> &green, std::vector<Coord2D> &blue,
                                int tran) const {}
    virtual void getAutoWBMultipliers (double &rm, double &gm, double &bm) const
    {
        rm = gm = bm = 1.0;
    }
    virtual void getAutoWBMultipliersitc(bool extra, double &tempref, double &greenref, double &tempitc, double &greenitc, float &temp0, float &delta, int &bia, int &dread, int &kcam,  int &nocam, float &studgood,  float &minchrom, int &kmin, float &minhist, float &maxhist, int begx, int begy, int yEn, int xEn, int cx, int cy, int bf_h, int bf_w, double &rm, double &gm, double &bm, const procparams::WBParams & wbpar, const procparams::ColorManagementParams &cmp, const procparams::RAWParams &raw, const procparams::ToneCurveParams &hrp)
    {
        rm = gm = bm = 1.0;
    }

};

template<>
inline void ImageDatas::convertTo(unsigned short src, unsigned char& dst) const
{
    dst = uint16ToUint8Rounded(src);
}
template<>
inline void ImageDatas::convertTo(unsigned char src, int& dst) const
{
    dst = src * 257;
}
template<>
inline void ImageDatas::convertTo(unsigned char src, unsigned short& dst) const
{
    dst = src * 257;
}
template<>
inline void ImageDatas::convertTo(float src, unsigned char& dst) const
{
    dst = uint16ToUint8Rounded(CLIP(src));
}
template<>
inline void ImageDatas::convertTo(unsigned char src, float& dst) const
{
    dst = src * 257;
}
template<>
inline void ImageDatas::convertTo(float src, float& dst) const
{
    dst = std::isnan(src) ? 0.f : src;
}

// --------------------------------------------------------------------
//                       Planar order classes
// --------------------------------------------------------------------

template <class T>
class PlanarPtr
{
protected:
    AlignedBuffer<T*> ab;
public:
#if CHECK_BOUNDS
    size_t width_, height_;
#endif
    T** ptrs;

#if CHECK_BOUNDS
    PlanarPtr() : width_(0), height_(0), ptrs (NULL) {}
#else
    PlanarPtr() : ptrs (nullptr) {}
#endif

    bool resize(size_t newSize)
    {
        if (ab.resize(newSize)) {
            ptrs = ab.data;
            return true;
        } else {
            ptrs = nullptr;
            return false;
        }
    }
    void swap (PlanarPtr<T> &other)
    {
        ab.swap(other.ab);
        T** tmpsPtrs = other.ptrs;
        other.ptrs = ptrs;
        ptrs = tmpsPtrs;

#if CHECK_BOUNDS
        size_t tmp = other.width_;
        other.width_ = width_;
        width_ = tmp;
        tmp = other.height_;
        other.height_ = height_;
        height_ = tmp;
#endif
    }

    T*&       operator() (size_t row)
    {
#if CHECK_BOUNDS
        assert (row < height_);
#endif
        return ptrs[row];
    }
    // Will send back the start of a row, starting with a red, green or blue value
    T*        operator() (size_t row) const
    {
#if CHECK_BOUNDS
        assert (row < height_);
#endif
        return ptrs[row];
    }
    // Will send back a value at a given row, col position
    T&        operator() (size_t row, size_t col)
    {
#if CHECK_BOUNDS
        assert (row < height_ && col < width_);
#endif
        return ptrs[row][col];
    }
    const T&   operator() (size_t row, size_t col) const
    {
#if CHECK_BOUNDS
        assert (row < height_ && col < width_);
#endif
        return ptrs[row][col];
    }
};

template <class T>
class PlanarWhateverData : virtual public ImageDatas
{

private:
    AlignedBuffer<T> abData;

    size_t rowstride;    // Plan size, in bytes (all padding bytes included)

public:
    T* data;
    PlanarPtr<T> v;  // v stands for "value", whatever it represent

    PlanarWhateverData() : rowstride(0), data (nullptr) {}
    PlanarWhateverData(int w, int h) : rowstride(0), data (nullptr)
    {
        allocate(w, h);
    }

    // Send back the row stride. WARNING: unit = byte, not element!
    size_t getRowStride () const
    {
        return rowstride;
    }

    void swap(PlanarWhateverData<T> &other)
    {
        abData.swap(other.abData);
        v.swap(other.v);
        T* tmpData = other.data;
        other.data = data;
        data = tmpData;
        int tmpWidth = other.width;
        other.width = width;
        width = tmpWidth;
        int tmpHeight = other.height;
        other.height = height;
        height = tmpHeight;
#if CHECK_BOUNDS
        v.width_ = width;
        v.height_ = height;
#endif
    }

    // use as pointer to data
    //operator void*() { return data; };

    /* If any of the required allocation fails, "width" and "height" are set to -1, and all remaining buffer are freed
     * Can be safely used to reallocate an existing image */
    void allocate (int W, int H) override
    {
        if (W == width && H == height) {
            return;
        }

        width = W;
        height = H;
#if CHECK_BOUNDS
        v.width_ = width;
        v.height_ = height;
#endif

        if (sizeof(T) > 1) {
            // 128 bits memory alignment for >8bits data
            rowstride = ( width * sizeof(T) + 15 ) / 16 * 16;
        } else {
            // No memory alignment for 8bits data
            rowstride = width * sizeof(T);
        }

        // find the padding length to ensure a 128 bits alignment for each row
        size_t size = rowstride * height;

        if (!width) {
            size = 0;
            rowstride = 0;
        }

        if (size && abData.resize(size, 1)
                && v.resize(height) ) {
            data   = abData.data;
        } else {
            // asking for a new size of 0 is safe and will free memory, if any!
            abData.resize(0);
            data = nullptr;
            v.resize(0);
            width = height = -1;
#if CHECK_BOUNDS
            v.width_ = v.height_ = -1;
#endif

            return;
        }

        char *start   = (char*)(data);

        for (int i = 0; i < height; ++i) {
            int k = i * rowstride;
            v(i) = (T*)(start   + k);
        }
    }

    /** Copy the data to another PlanarWhateverData */
    void copyData(PlanarWhateverData<T> *dest) const
    {
        assert (dest != NULL);
        // Make sure that the size is the same, reallocate if necessary
        dest->allocate(width, height);

        if (dest->width == -1) {
            return;
        }

        for (int i = 0; i < height; i++) {
            memcpy (dest->v(i), v(i), width * sizeof(T));
        }
    }

    /** Copy the a sub-region of the data to another PlanarRGBData */
    void copyData(PlanarWhateverData<T> *dest, int x, int y, int width, int height)
    {
        assert (dest != NULL);
        // Make sure that the size is the same, reallocate if necessary
        dest->allocate(width, height);

        if (dest->width == -1) {
            printf("ERROR: PlanarRGBData::copyData >>> allocation failed!\n");
            return;
        }

        for (int i = y, j = 0; i < y + height; ++i, ++j) {
            memcpy (dest->v(i) + x, v(j), width * sizeof(T));
        }
    }

    void rotate (int deg) override
    {

        if (deg == 90) {
            PlanarWhateverData<T> rotatedImg(height, width);  // New image, rotated

            for (int ny = 0; ny < rotatedImg.height; ny++) {
                int ox = ny;
                int oy = height - 1;

                for (int nx = 0; nx < rotatedImg.width; nx++) {
                    rotatedImg.v(ny, nx) = v(oy, ox);
                    --oy;
                }
            }

            swap(rotatedImg);
        } else if (deg == 270) {
            PlanarWhateverData<T> rotatedImg(height, width);  // New image, rotated

            for (int nx = 0; nx < rotatedImg.width; nx++) {
                int oy = nx;
                int ox = width - 1;

                for (int ny = 0; ny < rotatedImg.height; ny++) {
                    rotatedImg.v(ny, nx) = v(oy, ox);
                    --ox;
                }
            }

            swap(rotatedImg);
        } else if (deg == 180) {
            int height2 = height / 2;

#ifdef _OPENMP
            // difficult to find a cutoff value where parallelization is counter productive because of processor's data cache collision...
            bool bigImage = width > 32 && height > 50;
            #pragma omp parallel for schedule(static) if(bigImage)
#endif

            for (int i = 0; i < height2; i++) {
                for (int j = 0; j < width; j++) {
                    int x = width - 1 - j;
                    int y = height - 1 - i;

                    std::swap(v(i, j), v(y, x));
                }
            }

            // Middle row of odd-height images: only go half way otherwise the
            // pixels will be swapped twice.
            if (height & 1) {
                int i = height / 2;
                int width2 = width / 2;
                for (int j = 0; j < width2; j++) {
                    int x = width - 1 - j;

                    std::swap(v(i, j), v(i, x));
                }
            }
#ifdef _OPENMP
            static_cast<void>(bigImage); // to silence cppcheck warning
#endif
        }
    }

    template <class IC>
    void resizeImgTo (int nw, int nh, TypeInterpolation interp, PlanarWhateverData<IC> *imgPtr) const
    {
        //printf("resizeImgTo: resizing %s image data (%d x %d) to %s (%d x %d)\n", getType(), width, height, imgPtr->getType(), imgPtr->width, imgPtr->height);
        if (width == nw && height == nh) {
            // special case where no resizing is necessary, just type conversion....
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    convertTo(v(i, j), imgPtr->v(i, j));
                }
            }
        } else if (interp == TI_Nearest) {
            for (int i = 0; i < nh; i++) {
                int ri = i * height / nh;

                for (int j = 0; j < nw; j++) {
                    int ci = j * width / nw;
                    convertTo(v(ri, ci), imgPtr->v(i, j));
                }
            }
        } else if (interp == TI_Bilinear) {
            for (int i = 0; i < nh; i++) {
                int sy = i * height / nh;

                if (sy >= height) {
                    sy = height - 1;
                }

                float dy = float(i) * float(height) / float(nh) - float(sy);
                int ny = sy + 1;

                if (ny >= height) {
                    ny = sy;
                }

                for (int j = 0; j < nw; j++) {
                    int sx = j * width / nw;

                    if (sx >= width) {
                        sx = width;
                    }

                    float dx = float(j) * float(width) / float(nw) - float(sx);
                    int nx = sx + 1;

                    if (nx >= width) {
                        nx = sx;
                    }

                    convertTo(v(sy, sx) * (1.f - dx) * (1.f - dy) + v(sy, nx)*dx * (1.f - dy) + v(ny, sx) * (1.f - dx)*dy + v(ny, nx)*dx * dy, imgPtr->v(i, j));
                }
            }
        } else {
            // This case should never occur!
            for (int i = 0; i < nh; i++) {
                for (int j = 0; j < nw; j++) {
                    v(i, j) = 0;
                }
            }
        }
    }

    void hflip () override
    {
        int width2 = width / 2;

#ifdef _OPENMP
        // difficult to find a cutoff value where parallelization is counter productive because of processor's data cache collision...
        bool bigImage = width > 32 && height > 50;
        #pragma omp parallel for schedule(static) if(bigImage)
#endif

        for (int i = 0; i < height; i++)
            for (int j = 0; j < width2; j++) {
                float temp;
                int x = width - 1 - j;

                temp = v(i, j);
                v(i, j) = v(i, x);
                v(i, x) = temp;
            }
#ifdef _OPENMP
        static_cast<void>(bigImage); // to silence cppcheck warning
#endif
    }

    void vflip () override
    {

        int height2 = height / 2;

#ifdef _OPENMP
        // difficult to find a cutoff value where parallelization is counter productive because of processor's data cache collision...
        bool bigImage = width > 32 && height > 50;
        #pragma omp parallel for schedule(static) if(bigImage)
#endif

        for (int i = 0; i < height2; i++)
            for (int j = 0; j < width; j++) {
                T temp;
                int y = height - 1 - i;

                temp = v(i, j);
                v(i, j) = v(y, j);
                v(y, j) = temp;
            }
#ifdef _OPENMP
        static_cast<void>(bigImage); // to silence cppcheck warning
#endif
    }

    void transformPixel (int x, int y, int tran, int& tx, int& ty) const
    {

        if (!tran) {
            tx = x;
            ty = y;
            return;
        }

        int W = width;
        int H = height;
        int sw = W, sh = H;

        if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
            sw = H;
            sh = W;
        }

        int ppx = x, ppy = y;

        if (tran & TR_HFLIP) {
            ppx = sw - 1 - x;
        }

        if (tran & TR_VFLIP) {
            ppy = sh - 1 - y;
        }

        tx = ppx;
        ty = ppy;

        if ((tran & TR_ROT) == TR_R180) {
            tx = W - 1 - ppx;
            ty = H - 1 - ppy;
        } else if ((tran & TR_ROT) == TR_R90) {
            tx = ppy;
            ty = H - 1 - ppx;
        } else if ((tran & TR_ROT) == TR_R270) {
            tx = W - 1 - ppy;
            ty = ppx;
        }
    }

    void getPipetteData (T &value, int posX, int posY, int squareSize, int tran) const
    {
        int x;
        int y;
        float accumulator = 0.f;  // using float to avoid range overflow; -> please creates specialization if necessary
        unsigned long int n = 0;
        int halfSquare = squareSize / 2;
        transformPixel (posX, posY, tran, x, y);

        for (int iy = y - halfSquare; iy < y - halfSquare + squareSize; ++iy) {
            for (int ix = x - halfSquare; ix < x - halfSquare + squareSize; ++ix) {
                if (ix >= 0 && iy >= 0 && ix < width && iy < height) {
                    accumulator += float(this->v(iy, ix));
                    ++n;
                }
            }
        }

        value = n ? T(accumulator / float(n)) : T(0);
    }

    void readData (FILE *f)
    {
        for (int i = 0; i < height; i++) {
            fread (v(i), sizeof(T), width, f);
        }
    }

    void writeData (FILE *f) const
    {
        for (int i = 0; i < height; i++) {
            fwrite (v(i), sizeof(T), width, f);
        }
    }

    void fill (T value) {
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                v(i, j) = value;
            }
        }
    }

};


template <class T>
class PlanarRGBData : virtual public ImageDatas
{

private:
    AlignedBuffer<T> abData;

    size_t rowstride;    // Plan size, in bytes (all padding bytes included)
    size_t planestride;  // Row length, in bytes (padding bytes included)
protected:
    T* data;

public:
    PlanarPtr<T> r;
    PlanarPtr<T> g;
    PlanarPtr<T> b;

    PlanarRGBData() : rowstride(0), planestride(0), data (nullptr) {}
    PlanarRGBData(size_t w, size_t h) : rowstride(0), planestride(0), data (nullptr)
    {
        allocate(w, h);
    }

    // Send back the row stride. WARNING: unit = byte, not element!
    size_t getRowStride () const
    {
        return rowstride;
    }
    // Send back the plane stride. WARNING: unit = byte, not element!
    size_t getPlaneStride () const
    {
        return planestride;
    }

    void swap(PlanarRGBData<T> &other)
    {
        abData.swap(other.abData);
        r.swap(other.r);
        g.swap(other.g);
        b.swap(other.b);
        T* tmpData = other.data;
        other.data = data;
        data = tmpData;
        int tmpWidth = other.width;
        other.width = width;
        width = tmpWidth;
        int tmpHeight = other.height;
        other.height = height;
        height = tmpHeight;
#if CHECK_BOUNDS
        r.width_ = width;
        r.height_ = height;
        g.width_ = width;
        g.height_ = height;
        b.width_ = width;
        b.height_ = height;
#endif
    }

    // use as pointer to data
    //operator void*() { return data; };

    /* If any of the required allocation fails, "width" and "height" are set to -1, and all remaining buffer are freed
     * Can be safely used to reallocate an existing image */
    void allocate (int W, int H) final
    {

        if (W == width && H == height) {
            return;
        }

        width = W;
        height = H;
#if CHECK_BOUNDS
        r.width_ = width;
        r.height_ = height;
        g.width_ = width;
        g.height_ = height;
        b.width_ = width;
        b.height_ = height;
#endif

        if (sizeof(T) > 1) {
            // 128 bits memory alignment for >8bits data
            rowstride = ( width * sizeof(T) + 15 ) / 16 * 16;
            planestride = rowstride * height;
        } else {
            // No memory alignment for 8bits data
            rowstride = width * sizeof(T);
            planestride = rowstride * height;
        }

        // find the padding length to ensure a 128 bits alignment for each row
        size_t size = (size_t)rowstride * 3 * (size_t)height;

        if (!width) {
            size = 0;
            rowstride = 0;
        }

        if (size && abData.resize(size, 1)
                && r.resize(height)
                && g.resize(height)
                && b.resize(height) ) {
            data   = abData.data;
        } else {
            // asking for a new size of 0 is safe and will free memory, if any!
            abData.resize(0);
            data = nullptr;
            r.resize(0);
            g.resize(0);
            b.resize(0);
            width = height = -1;
#if CHECK_BOUNDS
            r.width_ = r.height_ = -1;
            g.width_ = g.height_ = -1;
            b.width_ = b.height_ = -1;
#endif
            return;
        }

        char *redstart   = (char*)(data);
        char *greenstart = (char*)(data) +   planestride;
        char *bluestart  = (char*)(data) + 2 * planestride;

        for (int i = 0; i < height; ++i) {
            size_t k = i * rowstride;
            r(i) = (T*)(redstart   + k);
            g(i) = (T*)(greenstart + k);
            b(i) = (T*)(bluestart  + k);
        }
    }

    /** Copy the data to another PlanarRGBData */
    void copyData(PlanarRGBData<T> *dest) const
    {
        assert (dest != nullptr);
        // Make sure that the size is the same, reallocate if necessary
        dest->allocate(width, height);

        if (dest->width == -1) {
            printf("ERROR: PlanarRGBData::copyData >>> allocation failed!\n");
            return;
        }

        for (int i = 0; i < height; i++) {
            memcpy (dest->r(i), r(i), width * sizeof(T));
            memcpy (dest->g(i), g(i), width * sizeof(T));
            memcpy (dest->b(i), b(i), width * sizeof(T));
        }
    }

    /** Copy the a sub-region of the data to another PlanarRGBData */
    void copyData(PlanarRGBData<T> *dest, int x, int y, int width, int height)
    {
        assert (dest != NULL);
        // Make sure that the size is the same, reallocate if necessary
        dest->allocate(width, height);

        if (dest->width == -1) {
            printf("ERROR: PlanarRGBData::copyData >>> allocation failed!\n");
            return;
        }

        for (int i = y, j = 0; i < y + height; ++i, ++j) {
            memcpy (dest->r(i) + x, r(j), width * sizeof(T));
            memcpy (dest->g(i) + x, g(j), width * sizeof(T));
            memcpy (dest->b(i) + x, b(j), width * sizeof(T));
        }
    }

    void rotate (int deg) final
    {

        if (deg == 90) {
            PlanarRGBData<T> rotatedImg(height, width);  // New image, rotated

            for (int ny = 0; ny < rotatedImg.height; ny++) {
                int ox = ny;
                int oy = height - 1;

                for (int nx = 0; nx < rotatedImg.width; nx++) {
                    rotatedImg.r(ny, nx) = r(oy, ox);
                    rotatedImg.g(ny, nx) = g(oy, ox);
                    rotatedImg.b(ny, nx) = b(oy, ox);
                    --oy;
                }
            }

            swap(rotatedImg);
        } else if (deg == 270) {
            PlanarRGBData<T> rotatedImg(height, width);  // New image, rotated

            for (int nx = 0; nx < rotatedImg.width; nx++) {
                int oy = nx;
                int ox = width - 1;

                for (int ny = 0; ny < rotatedImg.height; ny++) {
                    rotatedImg.r(ny, nx) = r(oy, ox);
                    rotatedImg.g(ny, nx) = g(oy, ox);
                    rotatedImg.b(ny, nx) = b(oy, ox);
                    --ox;
                }
            }

            swap(rotatedImg);
        } else if (deg == 180) {
            int height2 = height / 2;

#ifdef _OPENMP
            // difficult to find a cutoff value where parallelization is counter productive because of processor's data cache collision...
            bool bigImage = width > 32 && height > 50;
            #pragma omp parallel for schedule(static) if(bigImage)
#endif

            for (int i = 0; i < height2; i++) {
                for (int j = 0; j < width; j++) {
                    int x = width - 1 - j;
                    int y = height - 1 - i;

                    std::swap(r(i, j), r(y, x));
                    std::swap(g(i, j), g(y, x));
                    std::swap(b(i, j), b(y, x));
                }
            }

            // Middle row of odd-height images: only go half way otherwise the
            // pixels will be swapped twice.
            if (height & 1) {
                int i = height / 2;
                int width2 = width / 2;
                for (int j = 0; j < width2; j++) {
                    int x = width - 1 - j;

                    std::swap(r(i, j), r(i, x));
                    std::swap(g(i, j), g(i, x));
                    std::swap(b(i, j), b(i, x));
                }
            }
#ifdef _OPENMP
            static_cast<void>(bigImage); // to silence cppcheck warning
#endif
        }
    }

    template <class IC>
    void resizeImgTo (int nw, int nh, TypeInterpolation interp, IC *imgPtr) const
    {
        //printf("resizeImgTo: resizing %s image data (%d x %d) to %s (%d x %d)\n", getType(), width, height, imgPtr->getType(), imgPtr->width, imgPtr->height);
        if (width == nw && height == nh) {
            // special case where no resizing is necessary, just type conversion....
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    convertTo(r(i, j), imgPtr->r(i, j));
                    convertTo(g(i, j), imgPtr->g(i, j));
                    convertTo(b(i, j), imgPtr->b(i, j));
                }
            }
        } else if (interp == TI_Nearest) {
            for (int i = 0; i < nh; i++) {
                int ri = i * height / nh;

                for (int j = 0; j < nw; j++) {
                    int ci = j * width / nw;
                    convertTo(r(ri, ci), imgPtr->r(i, j));
                    convertTo(g(ri, ci), imgPtr->g(i, j));
                    convertTo(b(ri, ci), imgPtr->b(i, j));
                }
            }
        } else if (interp == TI_Bilinear) {
            float heightByNh = float(height) / float(nh);
            float widthByNw = float(width) / float(nw);
            float syf = 0.f;

            for (int i = 0; i < nh; i++, syf += heightByNh) {
                int sy = syf;
                float dy = syf - float(sy);
                int ny = sy < height - 1 ? sy + 1 : sy;

                float sxf = 0.f;

                for (int j = 0; j < nw; j++, sxf += widthByNw) {
                    int sx = sxf;
                    float dx = sxf - float(sx);
                    int nx = sx < width - 1 ? sx + 1 : sx;

                    convertTo(r(sy, sx) * (1.f - dx) * (1.f - dy) + r(sy, nx)*dx * (1.f - dy) + r(ny, sx) * (1.f - dx)*dy + r(ny, nx)*dx * dy, imgPtr->r(i, j));
                    convertTo(g(sy, sx) * (1.f - dx) * (1.f - dy) + g(sy, nx)*dx * (1.f - dy) + g(ny, sx) * (1.f - dx)*dy + g(ny, nx)*dx * dy, imgPtr->g(i, j));
                    convertTo(b(sy, sx) * (1.f - dx) * (1.f - dy) + b(sy, nx)*dx * (1.f - dy) + b(ny, sx) * (1.f - dx)*dy + b(ny, nx)*dx * dy, imgPtr->b(i, j));
                }
            }
        } else {
            // This case should never occur!
            for (int i = 0; i < nh; i++) {
                for (int j = 0; j < nw; j++) {
                    imgPtr->r(i, j) = 0;
                    imgPtr->g(i, j) = 0;
                    imgPtr->b(i, j) = 0;
                }
            }
        }
    }

    void hflip () final
    {
        int width2 = width / 2;

#ifdef _OPENMP
        // difficult to find a cutoff value where parallelization is counter productive because of processor's data cache collision...
        bool bigImage = width > 32 && height > 50;
        #pragma omp parallel for schedule(static) if(bigImage)
#endif

        for (int i = 0; i < height; i++)
            for (int j = 0; j < width2; j++) {
                float temp;
                int x = width - 1 - j;

                temp = r(i, j);
                r(i, j) = r(i, x);
                r(i, x) = temp;

                temp = g(i, j);
                g(i, j) = g(i, x);
                g(i, x) = temp;

                temp = b(i, j);
                b(i, j) = b(i, x);
                b(i, x) = temp;
            }
#ifdef _OPENMP
        static_cast<void>(bigImage); // to silence cppcheck warning
#endif
    }

    void vflip () final
    {

        int height2 = height / 2;

#ifdef _OPENMP
        // difficult to find a cutoff value where parallelization is counter productive because of processor's data cache collision...
        bool bigImage = width > 32 && height > 50;
        #pragma omp parallel for schedule(static) if(bigImage)
#endif

        for (int i = 0; i < height2; i++)
            for (int j = 0; j < width; j++) {
                T tempR, tempG, tempB;
                int y = height - 1 - i;

                tempR = r(i, j);
                r(i, j) = r(y, j);
                r(y, j) = tempR;

                tempG = g(i, j);
                g(i, j) = g(y, j);
                g(y, j) = tempG;

                tempB = b(i, j);
                b(i, j) = b(y, j);
                b(y, j) = tempB;
            }
#ifdef _OPENMP
        static_cast<void>(bigImage); // to silence cppcheck warning
#endif
    }

    void calcGrayscaleHist(unsigned int *hist16) const
    {
        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++) {
                unsigned short rIdx, gIdx, bIdx;
                convertTo(r(row, col), rIdx);
                convertTo(g(row, col), gIdx);
                convertTo(b(row, col), bIdx);
                hist16[rIdx]++;
                hist16[gIdx] += 2; // Bayer 2x green correction
                hist16[bIdx]++;
            }
    }

    void computeAutoHistogram (LUTu & histogram, int& histcompr) const
    {
        histcompr = 3;

        histogram(65536 >> histcompr);
        histogram.clear();
        const LUTf& igammatab = getigammatab();

#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            LUTu histThr(histogram.getSize());
            histThr.clear();
#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16) nowait
#endif
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    float r_, g_, b_;
                    convertTo<T, float>(r(i, j), r_);
                    convertTo<T, float>(g(i, j), g_);
                    convertTo<T, float>(b(i, j), b_);
                    histThr[static_cast<int>(igammatab[r_]) >> histcompr]++;
                    histThr[static_cast<int>(igammatab[g_]) >> histcompr]++;
                    histThr[static_cast<int>(igammatab[b_]) >> histcompr]++;
                }
            }
#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                histogram += histThr;
            }
        }
    }

    void computeHistogramAutoWB (double &avg_r, double &avg_g, double &avg_b, int &n, LUTu &histogram, const int compression) const final
    {
        histogram.clear();
        avg_r = avg_g = avg_b = 0.;
        n = 0;
        const LUTf& igammatab = getigammatab();
        for (unsigned int i = 0; i < (unsigned int)(height); i++)
            for (unsigned int j = 0; j < (unsigned int)(width); j++) {
                float r_, g_, b_;
                convertTo<T, float>(r(i, j), r_);
                convertTo<T, float>(g(i, j), g_);
                convertTo<T, float>(b(i, j), b_);
                int rtemp = igammatab[r_];
                int gtemp = igammatab[g_];
                int btemp = igammatab[b_];

                histogram[rtemp >> compression]++;
                histogram[gtemp >> compression] += 2;
                histogram[btemp >> compression]++;

                // autowb computation
                if (r_ > 64000.f || g_ > 64000.f || b_ > 64000.f) {
                    continue;
                }

                avg_r += double(r_);
                avg_g += double(g_);
                avg_b += double(b_);
                n++;
            }
    }

    void getAutoWBMultipliers (double &rm, double &gm, double &bm) const override
    {

        double avg_r = 0.;
        double avg_g = 0.;
        double avg_b = 0.;
        int n = 0;
        //int p = 6;

#ifdef _OPENMP
        #pragma omp parallel for reduction(+:avg_r,avg_g,avg_b,n) schedule(dynamic,16)
#endif
        for (unsigned int i = 0; i < (unsigned int)(height); i++)
            for (unsigned int j = 0; j < (unsigned int)(width); j++) {
                float r_, g_, b_;
                convertTo<T, float>(r(i, j), r_);
                convertTo<T, float>(g(i, j), g_);
                convertTo<T, float>(b(i, j), b_);

                if (r_ > 64000.f || g_ > 64000.f || b_ > 64000.f) {
                    continue;
                }

                avg_r += double(r_);
                avg_g += double(g_);
                avg_b += double(b_);
                n++;
            }

        rm = avg_r / double(n);
        gm = avg_g / double(n);
        bm = avg_b / double(n);
    }

    void transformPixel (int x, int y, int tran, int& tx, int& ty) const
    {

        if (!tran) {
            tx = x;
            ty = y;
            return;
        }

        int W = width;
        int H = height;
        int sw = W, sh = H;

        if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
            sw = H;
            sh = W;
        }

        int ppx = x, ppy = y;

        if (tran & TR_HFLIP) {
            ppx = sw - 1 - x;
        }

        if (tran & TR_VFLIP) {
            ppy = sh - 1 - y;
        }

        tx = ppx;
        ty = ppy;

        if ((tran & TR_ROT) == TR_R180) {
            tx = W - 1 - ppx;
            ty = H - 1 - ppy;
        } else if ((tran & TR_ROT) == TR_R90) {
            tx = ppy;
            ty = H - 1 - ppx;
        } else if ((tran & TR_ROT) == TR_R270) {
            tx = W - 1 - ppy;
            ty = ppx;
        }
    }

    void getSpotWBData (double &reds, double &greens, double &blues, int &rn, int &gn, int &bn,
                                std::vector<Coord2D> &red, std::vector<Coord2D> &green, std::vector<Coord2D> &blue,
                                int tran) const override
    {
        int x;
        int y;
        reds = 0, greens = 0, blues = 0;
        rn = 0, gn = 0, bn = 0;

        for (size_t i = 0; i < red.size(); i++) {
            transformPixel (red[i].x, red[i].y, tran, x, y);

            if (x >= 0 && y >= 0 && x < width && y < height) {
                float v;
                convertTo<T, float>(this->r(y, x), v);
                reds += double(v);
                rn++;
            }

            transformPixel (green[i].x, green[i].y, tran, x, y);

            if (x >= 0 && y >= 0 && x < width && y < height) {
                float v;
                convertTo<T, float>(this->g(y, x), v);
                greens += double(v);
                gn++;
            }

            transformPixel (blue[i].x, blue[i].y, tran, x, y);

            if (x >= 0 && y >= 0 && x < width && y < height) {
                float v;
                convertTo<T, float>(this->b(y, x), v);
                blues += double(v);
                bn++;
            }
        }
    }

    void getPipetteData (T &valueR, T &valueG, T &valueB, int posX, int posY, const int squareSize, int tran) const
    {
        int x;
        int y;
        float accumulatorR = 0.f;  // using float to avoid range overflow; -> please creates specialization if necessary
        float accumulatorG = 0.f;  //    "
        float accumulatorB = 0.f;  //    "
        unsigned long int n = 0;
        int halfSquare = squareSize / 2;
        transformPixel (posX, posY, tran, x, y);

        for (int iy = y - halfSquare; iy < y - halfSquare + squareSize; ++iy) {
            for (int ix = x - halfSquare; ix < x - halfSquare + squareSize; ++ix) {
                if (ix >= 0 && iy >= 0 && ix < width && iy < height) {
                    accumulatorR += float(this->r(iy, ix));
                    accumulatorG += float(this->g(iy, ix));
                    accumulatorB += float(this->b(iy, ix));
                    ++n;
                }
            }
        }

        valueR = n ? T(accumulatorR / float(n)) : T(0);
        valueG = n ? T(accumulatorG / float(n)) : T(0);
        valueB = n ? T(accumulatorB / float(n)) : T(0);
    }

    void readData (FILE *f)
    {
        for (int i = 0; i < height; i++) {
            if (fread(r(i), sizeof(T), width, f) < static_cast<size_t>(width)) {
                break;
            }
        }

        for (int i = 0; i < height; i++) {
            if (fread(g(i), sizeof(T), width, f) < static_cast<size_t>(width)) {
                break;
            }
        }

        for (int i = 0; i < height; i++) {
            if (fread(b(i), sizeof(T), width, f) < static_cast<size_t>(width)) {
                break;
            }
        }
    }

    void writeData (FILE *f) const
    {
        for (int i = 0; i < height; i++) {
            fwrite (r(i), sizeof(T), width, f);
        }

        for (int i = 0; i < height; i++) {
            fwrite (g(i), sizeof(T), width, f);
        }

        for (int i = 0; i < height; i++) {
            fwrite (b(i), sizeof(T), width, f);
        }
    }

};

// --------------------------------------------------------------------
//                       Chunky order classes
// --------------------------------------------------------------------

template <class T>
class ChunkyPtr
{
private:
    T* ptr;
    ssize_t width;
public:
#if CHECK_BOUNDS
    size_t width_, height_;
#endif

#if CHECK_BOUNDS
    ChunkyPtr() : ptr (NULL), width(-1), width_(0), height_(0) {}
#else
    ChunkyPtr() : ptr (nullptr), width(-1) {}
#endif
    void init(T* base, ssize_t w = -1)
    {
        ptr = base;
        width = w;
    }
    void swap (ChunkyPtr<T> &other)
    {
        T* tmpsPtr = other.ptr;
        other.ptr = ptr;
        ptr = tmpsPtr;

        ssize_t tmpWidth = other.width;
        other.width = width;
        width = tmpWidth;

#if CHECK_BOUNDS
        size_t tmp = other.width_;
        other.width_ = width_;
        width_ = tmp;
        tmp = other.height_;
        other.height_ = height_;
        height_ = tmp;
#endif

    }

    // Will send back the start of a row, starting with a red, green or blue value
    T* operator() (size_t row) const
    {
#if CHECK_BOUNDS
        assert (row < height_);
#endif
        return &ptr[3 * (row * width)];
    }
    // Will send back a value at a given row, col position
    T& operator() (size_t row, size_t col)
    {
#if CHECK_BOUNDS
        assert (row < height_ && col < width_);
#endif
        return ptr[3 * (row * width + col)];
    }
    const T&  operator() (size_t row, size_t col) const
    {
#if CHECK_BOUNDS
        assert (row < height_ && col < width_);
#endif
        return ptr[3 * (row * width + col)];
    }
};

template <class T>
class ChunkyRGBData : virtual public ImageDatas
{

private:
    AlignedBuffer<T> abData;

public:
    T* data;
    ChunkyPtr<T> r;
    ChunkyPtr<T> g;
    ChunkyPtr<T> b;

    ChunkyRGBData() : data (nullptr) {}
    ChunkyRGBData(int w, int h) : data (nullptr)
    {
        allocate(w, h);
    }

    /** Returns the pixel data, in r/g/b order from top left to bottom right continuously.
      * @return a pointer to the pixel data */
    const T* getData ()
    {
        return data;
    }

    void swap(ChunkyRGBData<T> &other)
    {
        abData.swap(other.abData);
        r.swap(other.r);
        g.swap(other.g);
        b.swap(other.b);
        T* tmpData = other.data;
        other.data = data;
        data = tmpData;
        int tmpWidth = other.width;
        other.width = width;
        width = tmpWidth;
        int tmpHeight = other.height;
        other.height = height;
        height = tmpHeight;
#if CHECK_BOUNDS
        r.width_ = width;
        r.height_ = height;
        g.width_ = width;
        g.height_ = height;
        b.width_ = width;
        b.height_ = height;
#endif
    }

    /*
     * If any of the required allocation fails, "width" and "height" are set to -1, and all remaining buffer are freed
     * Can be safely used to reallocate an existing image or to free up it's memory with "allocate (0,0);"
     */
    void allocate (int W, int H) final
    {

        if (W == width && H == height) {
            return;
        }

        width = W;
        height = H;
#if CHECK_BOUNDS
        r.width_ = width;
        r.height_ = height;
        g.width_ = width;
        g.height_ = height;
        b.width_ = width;
        b.height_ = height;
#endif

        abData.resize((size_t)width * (size_t)height * (size_t)3);

        if (!abData.isEmpty()) {
            data = abData.data;
            r.init(data,   width);
            g.init(data + 1, width);
            b.init(data + 2, width);
        } else {
            data = nullptr;
            r.init(nullptr);
            g.init(nullptr);
            b.init(nullptr);
            width = height = -1;
#if CHECK_BOUNDS
            r.width_ = r.height_ = -1;
            g.width_ = g.height_ = -1;
            b.width_ = b.height_ = -1;
#endif
        }
    }

    /** Copy the data to another ChunkyRGBData */
    void copyData(ChunkyRGBData<T> *dest) const
    {
        assert (dest != nullptr);
        // Make sure that the size is the same, reallocate if necessary
        dest->allocate(width, height);

        if (dest->width == -1) {
            printf("ERROR: ChunkyRGBData::copyData >>> allocation failed!\n");
            return;
        }

        memcpy (dest->data, data, 3 * width * height * sizeof(T));
    }

    /** Copy the a sub-region of the data to another PlanarRGBData */
    void copyData(ChunkyRGBData<T> *dest, int x, int y, int width, int height)
    {
        assert (dest != NULL);
        // Make sure that the size is the same, reallocate if necessary
        dest->allocate(width, height);

        if (dest->width == -1) {
            printf("ERROR: PlanarRGBData::copyData >>> allocation failed!\n");
            return;
        }

        for (int i = y, j = 0; i < y + height; ++i, ++j) {
            memcpy (dest->r(i) + x, r(j), 3 * width * sizeof(T));
        }
    }

    void rotate (int deg) final
    {

        if (deg == 90) {
            ChunkyRGBData<T> rotatedImg(height, width);  // New image, rotated

            for (int ny = 0; ny < rotatedImg.height; ny++) {
                int ox = ny;
                int oy = height - 1;

                for (int nx = 0; nx < rotatedImg.width; nx++) {
                    rotatedImg.r(ny, nx) = r(oy, ox);
                    rotatedImg.g(ny, nx) = g(oy, ox);
                    rotatedImg.b(ny, nx) = b(oy, ox);
                    --oy;
                }
            }

            swap(rotatedImg);
        } else if (deg == 270) {
            ChunkyRGBData<T> rotatedImg(height, width);  // New image, rotated

            for (int nx = 0; nx < rotatedImg.width; nx++) {
                int oy = nx;
                int ox = width - 1;

                for (int ny = 0; ny < rotatedImg.height; ny++) {
                    rotatedImg.r(ny, nx) = r(oy, ox);
                    rotatedImg.g(ny, nx) = g(oy, ox);
                    rotatedImg.b(ny, nx) = b(oy, ox);
                    --ox;
                }
            }

            swap(rotatedImg);
        } else if (deg == 180) {
            int height2 = height / 2;

            // Maybe not sufficiently optimized, but will do what it has to do
            for (int i = 0; i < height2; i++) {
                for (int j = 0; j < width; j++) {
                    int x = width - 1 - j;
                    int y = height - 1 - i;

                    std::swap(r(i, j), r(y, x));
                    std::swap(g(i, j), g(y, x));
                    std::swap(b(i, j), b(y, x));
                }
            }

            // Middle row of odd-height images: only go half way otherwise the
            // pixels will be swapped twice.
            if (height & 1) {
                int i = height / 2;
                int width2 = width / 2;
                for (int j = 0; j < width2; j++) {
                    int x = width - 1 - j;

                    std::swap(r(i, j), r(i, x));
                    std::swap(g(i, j), g(i, x));
                    std::swap(b(i, j), b(i, x));
                }
            }
        }
    }

    template <class IC>
    void resizeImgTo (int nw, int nh, TypeInterpolation interp, IC *imgPtr) const
    {
        //printf("resizeImgTo: resizing %s image data (%d x %d) to %s (%d x %d)\n", getType(), width, height, imgPtr->getType(), imgPtr->width, imgPtr->height);
        if (width == nw && height == nh) {
            // special case where no resizing is necessary, just type conversion....
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    convertTo(r(i, j), imgPtr->r(i, j));
                    convertTo(g(i, j), imgPtr->g(i, j));
                    convertTo(b(i, j), imgPtr->b(i, j));
                }
            }
        } else if (interp == TI_Nearest) {
            for (int i = 0; i < nh; i++) {
                int ri = i * height / nh;

                for (int j = 0; j < nw; j++) {
                    int ci = j * width / nw;
                    convertTo(r(ri, ci), imgPtr->r(i, j));
                    convertTo(g(ri, ci), imgPtr->g(i, j));
                    convertTo(b(ri, ci), imgPtr->b(i, j));
                }
            }
        } else if (interp == TI_Bilinear) {
            for (int i = 0; i < nh; i++) {
                int sy = i * height / nh;

                if (sy >= height) {
                    sy = height - 1;
                }

                float dy = float(i) * float(height) / float(nh) - float(sy);
                int ny = sy + 1;

                if (ny >= height) {
                    ny = sy;
                }

                for (int j = 0; j < nw; j++) {
                    int sx = j * width / nw;

                    if (sx >= width) {
                        sx = width;
                    }

                    float dx = float(j) * float(width) / float(nw) - float(sx);
                    int nx = sx + 1;

                    if (nx >= width) {
                        nx = sx;
                    }

                    T valR = r(sy, sx) * (1.f - dx) * (1.f - dy) + r(sy, nx) * dx * (1.f - dy) + r(ny, sx) * (1.f - dx) * dy + r(ny, nx) * dx * dy;
                    T valG = g(sy, sx) * (1.f - dx) * (1.f - dy) + g(sy, nx) * dx * (1.f - dy) + g(ny, sx) * (1.f - dx) * dy + g(ny, nx) * dx * dy;
                    T valB = b(sy, sx) * (1.f - dx) * (1.f - dy) + b(sy, nx) * dx * (1.f - dy) + b(ny, sx) * (1.f - dx) * dy + b(ny, nx) * dx * dy;
                    convertTo(valR, imgPtr->r(i, j));
                    convertTo(valG, imgPtr->g(i, j));
                    convertTo(valB, imgPtr->b(i, j));
                }
            }
        } else {
            // This case should never occur!
            for (int i = 0; i < nh; i++) {
                for (int j = 0; j < nw; j++) {
                    imgPtr->r(i, j) = 0;
                    imgPtr->g(i, j) = 0;
                    imgPtr->b(i, j) = 0;
                }
            }
        }
    }

    void hflip () final
    {
        int width2 = width / 2;

        for (int i = 0; i < height; i++) {
            int offsetBegin = 0;
            int offsetEnd = 3 * (width - 1);

            for (int j = 0; j < width2; j++) {
                T temp;

                temp = data[offsetBegin];
                data[offsetBegin] = data[offsetEnd];
                data[offsetEnd] = temp;

                ++offsetBegin;
                ++offsetEnd;

                temp = data[offsetBegin];
                data[offsetBegin] = data[offsetEnd];
                data[offsetEnd] = temp;

                ++offsetBegin;
                ++offsetEnd;

                temp = data[offsetBegin];
                data[offsetBegin] = data[offsetEnd];
                data[offsetEnd] = temp;

                ++offsetBegin;
                offsetEnd -= 5;

            }
        }
    }

    void vflip () final
    {

        AlignedBuffer<T> lBuffer(3 * width);
        T* lineBuffer = lBuffer.data;
        size_t size = 3 * width * sizeof(T);

        for (int i = 0; i < height / 2; i++) {
            T *lineBegin1 = r(i);
            T *lineBegin2 = r(height - 1 - i);
            memcpy (lineBuffer, lineBegin1, size);
            memcpy (lineBegin1, lineBegin2, size);
            memcpy (lineBegin2, lineBuffer, size);
        }
    }

    void calcGrayscaleHist(unsigned int *hist16) const
    {
        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++) {
                unsigned short rIdx, gIdx, bIdx;
                convertTo(r(row, col), rIdx);
                convertTo(g(row, col), gIdx);
                convertTo(b(row, col), bIdx);
                hist16[rIdx]++;
                hist16[gIdx] += 2; // Bayer 2x green correction
                hist16[bIdx]++;
            }
    }

    void computeAutoHistogram (LUTu & histogram, int& histcompr) const
    {
        histcompr = 3;

        histogram(65536 >> histcompr);
        histogram.clear();
        const LUTf& igammatab = getigammatab();

#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            LUTu histThr(histogram.getSize());
            histThr.clear();
#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16) nowait
#endif
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    float r_, g_, b_;
                    convertTo<T, float>(r(i, j), r_);
                    convertTo<T, float>(g(i, j), g_);
                    convertTo<T, float>(b(i, j), b_);
                    histThr[static_cast<int>(igammatab[r_]) >> histcompr]++;
                    histThr[static_cast<int>(igammatab[g_]) >> histcompr]++;
                    histThr[static_cast<int>(igammatab[b_]) >> histcompr]++;
                }
            }
#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                histogram += histThr;
            }
        }
    }

    void computeHistogramAutoWB (double &avg_r, double &avg_g, double &avg_b, int &n, LUTu &histogram, const int compression) const final
    {
        histogram.clear();
        avg_r = avg_g = avg_b = 0.;
        n = 0;
        const LUTf& igammatab = getigammatab();

        for (unsigned int i = 0; i < (unsigned int)(height); i++)
            for (unsigned int j = 0; j < (unsigned int)(width); j++) {
                float r_, g_, b_;
                convertTo<T, float>(r(i, j), r_);
                convertTo<T, float>(g(i, j), g_);
                convertTo<T, float>(b(i, j), b_);
                int rtemp = igammatab[r_];
                int gtemp = igammatab[g_];
                int btemp = igammatab[b_];

                histogram[rtemp >> compression]++;
                histogram[gtemp >> compression] += 2;
                histogram[btemp >> compression]++;

                // autowb computation
                if (r_ > 64000.f || g_ > 64000.f || b_ > 64000.f) {
                    continue;
                }

                avg_r += double(r_);
                avg_g += double(g_);
                avg_b += double(b_);
                n++;
            }
    }

    void getAutoWBMultipliers (double &rm, double &gm, double &bm) const override
    {

        double avg_r = 0.;
        double avg_g = 0.;
        double avg_b = 0.;
        int n = 0;
        //int p = 6;

#ifdef _OPENMP
        #pragma omp parallel for reduction(+:avg_r,avg_g,avg_b,n) schedule(dynamic,16)
#endif
        for (unsigned int i = 0; i < (unsigned int)(height); i++)
            for (unsigned int j = 0; j < (unsigned int)(width); j++) {
                float r_, g_, b_;
                convertTo<T, float>(r(i, j), r_);
                convertTo<T, float>(g(i, j), g_);
                convertTo<T, float>(b(i, j), b_);

                if (r_ > 64000.f || g_ > 64000.f || b_ > 64000.f) {
                    continue;
                }

                avg_r += double(r_);
                avg_g += double(g_);
                avg_b += double(b_);
                n++;
            }

        rm = avg_r / double(n);
        gm = avg_g / double(n);
        bm = avg_b / double(n);
    }

    void transformPixel (int x, int y, int tran, int& tx, int& ty) const
    {

        if (!tran) {
            tx = x;
            ty = y;
            return;
        }

        int W = width;
        int H = height;
        int sw = W, sh = H;

        if ((tran & TR_ROT) == TR_R90 || (tran & TR_ROT) == TR_R270) {
            sw = H;
            sh = W;
        }

        int ppx = x, ppy = y;

        if (tran & TR_HFLIP) {
            ppx = sw - 1 - x;
        }

        if (tran & TR_VFLIP) {
            ppy = sh - 1 - y;
        }

        tx = ppx;
        ty = ppy;

        if ((tran & TR_ROT) == TR_R180) {
            tx = W - 1 - ppx;
            ty = H - 1 - ppy;
        } else if ((tran & TR_ROT) == TR_R90) {
            tx = ppy;
            ty = H - 1 - ppx;
        } else if ((tran & TR_ROT) == TR_R270) {
            tx = W - 1 - ppy;
            ty = ppx;
        }
    }

    void getSpotWBData (double &reds, double &greens, double &blues, int &rn, int &gn, int &bn,
                                std::vector<Coord2D> &red, std::vector<Coord2D> &green, std::vector<Coord2D> &blue,
                                int tran) const override
    {
        int x;
        int y;
        reds = 0, greens = 0, blues = 0;
        rn = 0, gn = 0, bn = 0;

        for (size_t i = 0; i < red.size(); i++) {
            transformPixel (red[i].x, red[i].y, tran, x, y);

            if (x >= 0 && y >= 0 && x < width && y < height) {
                float v;
                convertTo<T, float>(this->r(y, x), v);
                reds += double(v);
                rn++;
            }

            transformPixel (green[i].x, green[i].y, tran, x, y);

            if (x >= 0 && y >= 0 && x < width && y < height) {
                float v;
                convertTo<T, float>(this->g(y, x), v);
                greens += double(v);
                gn++;
            }

            transformPixel (blue[i].x, blue[i].y, tran, x, y);

            if (x >= 0 && y >= 0 && x < width && y < height) {
                float v;
                convertTo<T, float>(this->b(y, x), v);
                blues += double(v);
                bn++;
            }
        }
    }

    void readData (FILE *f)
    {
        for (int i = 0; i < height; i++) {
            if (fread(r(i), sizeof(T), 3 * width, f) < 3 * static_cast<size_t>(width)) {
                break;
            }
        }
    }

    void writeData (FILE *f) const
    {
        for (int i = 0; i < height; i++) {
            fwrite (r(i), sizeof(T), 3 * width, f);
        }
    }

};

// --------------------------------------------------------------------


/** @brief This class represents an image (the result of the image processing) */
class IImage : virtual public ImageDimensions
{
public:

    virtual ~IImage() {}
    /** @brief Returns a mutex that can is useful in many situations. No image operations should be performed without locking this mutex.
      * @return The mutex */
    virtual MyMutex& getMutex () = 0;
    virtual cmsHPROFILE getProfile () const = 0;
    /** @brief Saves the image to file. It autodetects the format (jpg, tif, png are supported).
      * @param fname is the name of the file
        @return the error code, 0 if none */
    virtual int saveToFile (const Glib::ustring &fname) const = 0;
    /** @brief Saves the image to file in a png format.
      * @param fname is the name of the file
      * @param compression is the amount of compression (0-6), -1 corresponds to the default
      * @param bps can be 8 or 16 depending on the bits per pixels the output file will have
        @return the error code, 0 if none */
    virtual int saveAsPNG (const Glib::ustring &fname, int bps = -1) const = 0;
    /** @brief Saves the image to file in a jpg format.
      * @param fname is the name of the file
      * @param quality is the quality of the jpeg (0...100), set it to -1 to use default
        @return the error code, 0 if none */
    virtual int saveAsJPEG (const Glib::ustring &fname, int quality = 100, int subSamp = 3 ) const = 0;
    /** @brief Saves the image to file in a tif format.
      * @param fname is the name of the file
      * @param bps can be 8 or 16 depending on the bits per pixels the output file will have
      * @param isFloat is true for saving float images. Will be ignored by file format not supporting float data
        @return the error code, 0 if none */
    virtual int saveAsTIFF (
        const Glib::ustring &fname,
        int bps = -1,
        bool isFloat = false,
        bool uncompressed = false,
        bool big = false
    ) const = 0;
    /** @brief Saves the image to file in an avif format.
      * @param fname is the name of the file
      * @param bps can be 8, 10 or 12 depending on the bits per pixels the output file will have
      * @param quality is the quality of the avif (0...100), 100 being lossless
        @return the error code, 0 if none */
    virtual int saveAsAVIF (const Glib::ustring &fname, int bps = 8, int quality = 90) const = 0;
    /** @brief Sets the progress listener if you want to follow the progress of the image saving operations (optional).
      * @param pl is the pointer to the class implementing the ProgressListener interface */
    virtual void setSaveProgressListener (ProgressListener* pl) = 0;
};

/** @brief This class represents an image having a float pixel planar representation.
    The planes are stored as two dimensional arrays. All the rows have a 128 bits alignment. */
class IImagefloat : public IImage, public PlanarRGBData<float>
{
public:
    ~IImagefloat() override {}
};

/** @brief This class represents an image having a classical 8 bits/pixel representation */
class IImage8 : public IImage, public ChunkyRGBData<unsigned char>
{
public:
    ~IImage8() override {}
};

/** @brief This class represents an image having a 16 bits/pixel planar representation.
  The planes are stored as two dimensional arrays. All the rows have a 128 bits alignment. */
class IImage16 : public IImage, public PlanarRGBData<unsigned short>
{
public:
    ~IImage16() override {}
};

}
