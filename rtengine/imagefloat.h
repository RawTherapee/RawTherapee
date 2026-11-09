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
//
// A class representing a 16 bit rgb image with separate planes and 16 byte aligned data
//
#pragma once

#include "imageio.h"

namespace rtengine
{
using namespace procparams;

class Image8;
class Image16;
class LabImage;

/*
 * Image type used by most tools; expected range: [0.0 ; 65535.0]
 */
class Imagefloat final : public IImagefloat, public ImageIO
{

public:

    Imagefloat ();
    Imagefloat (int width, int height);
    ~Imagefloat () override;

    Imagefloat* copy () const;
    Imagefloat*          copySubRegion (int x, int y, int width, int height);

    void getStdImage (const ColorTemp &ctemp, int tran, Imagefloat* image, const PreviewProps &pp) const override;

    const char* getType () const override
    {
        return sImagefloat;
    }

    int getBPS () const override
    {
        return 8 * sizeof(float);
    }

    void getScanline (int row, unsigned char* buffer, int bps, bool isFloat = false) const override;
    void setScanline (int row, const unsigned char* buffer, int bps, unsigned int numSamples) override;

    // functions inherited from IImagefloat:
    MyMutex& getMutex () override
    {
        return mutex ();
    }
    cmsHPROFILE getProfile () const override
    {
        return getEmbeddedProfile ();
    }
    int saveToFile (const Glib::ustring &fname) const override
    {
        return save (fname);
    }
    int saveAsPNG  (const Glib::ustring &fname, int bps = -1) const override
    {
        return savePNG (fname, bps);
    }
    int saveAsJPEG (const Glib::ustring &fname, int quality = 100, int subSamp = 3) const override
    {
        return saveJPEG (fname, quality, subSamp);
    }
    int saveAsTIFF (
        const Glib::ustring &fname,
        int bps = -1,
        bool isFloat = false,
        bool uncompressed = false,
        bool big = false
    ) const override
    {
        return saveTIFF (fname, bps, isFloat, uncompressed, big);
    }
    int saveAsAVIF (const Glib::ustring &fname, int bps = 8, int quality = 90) const override
    {
        return saveAVIF (fname, bps, quality);
    }
    void setSaveProgressListener (ProgressListener* pl) override
    {
        setProgressListener (pl);
    }

    inline uint16_t DNG_FloatToHalf(float f) const
    {
        union {
            float f;
            uint32_t i;
        } tmp;

        tmp.f = f;
        const int32_t lsign = (tmp.i >> 16) & 0x00008000;
        int32_t exponent = ((tmp.i >> 23) & 0x000000ff) - (127 - 15);
        int32_t mantissa = tmp.i & 0x007fffff;
        if (exponent <= 0) {
            if (exponent < -10) {
                return (uint16_t)lsign;
            }
            mantissa = (mantissa | 0x00800000) >> (1 - exponent);
            if (mantissa &  0x00001000)
                mantissa += 0x00002000;
            return (uint16_t)(lsign | (mantissa >> 13));
        } else if (exponent == 0xff - (127 - 15)) {
            if (mantissa == 0) {
                return (uint16_t)(lsign | 0x7c00);
            } else {
                return (uint16_t)(lsign | 0x7c00 | (mantissa >> 13));
            }
        }
        if (mantissa & 0x00001000) {
            mantissa += 0x00002000;
            if (mantissa & 0x00800000) {
                mantissa = 0;           // overflow in significand,
                exponent += 1;          // adjust exponent
            }
        }
        if (exponent > 30) {
            return (uint16_t)(lsign | 0x7c00); // infinity with the same sign as f.
        }
        return (uint16_t)(lsign | (exponent << 10) | (mantissa >> 13));
    }

    // From DNG SDK dng_utils.h
    inline float         DNG_HalfToFloat(uint16_t halfValue)
    {
        union {
            float f;
            uint32_t i;
        } tmp;

        const int32_t lsign = (halfValue >> 15) & 0x00000001;
        int32_t exponent = (halfValue >> 10) & 0x0000001f;
        int32_t mantissa = halfValue & 0x000003ff;
        if (exponent == 0) {
            if (mantissa == 0) {
                // Plus or minus zero
                tmp.i = (uint32_t) (lsign << 31);
                return tmp.f;
            } else {
                // Denormalized number -- renormalize it
                while (!(mantissa & 0x00000400)) {
                    mantissa <<= 1;
                    exponent -=  1;
                }
                exponent += 1;
                mantissa &= ~0x00000400;
            }
        } else if (exponent == 31) {
            if (mantissa == 0) {
                // Positive or negative infinity, convert to maximum (16 bit) values.
                tmp.i = (uint32_t)((lsign << 31) | ((0x1eL + 127 - 15) << 23) | (0x3ffL << 13));
                return tmp.f;
            } else {
                // Nan -- Just set to zero.
                return 0;
            }
        }
        // Normalized number
        exponent += (127 - 15);
        mantissa <<= 13;
        // Assemble sign, exponent and mantissa.
        tmp.i = (uint32_t) ((lsign << 31) | (exponent << 23) | mantissa);
        return tmp.f;
    }

    inline uint32_t      DNG_FP24ToFloat(const uint8_t * input)
    {
        const int32_t lsign = (input[0] >> 7) & 0x01;
        int32_t exponent = input[0] & 0x7F;
        int32_t mantissa = (((int32_t) input[1]) << 8) | input[2];
        if (exponent == 0) {
            if (mantissa == 0) {
                // Plus or minus zero
                return (uint32_t) (lsign << 31);
            } else {
                // Denormalized number -- renormalize it
                while (!(mantissa & 0x00010000)) {
                    mantissa <<= 1;
                    exponent -=  1;
                }
                exponent += 1;
                mantissa &= ~0x00010000;
            }
        } else if (exponent == 127) {
            if (mantissa == 0) {
                // Positive or negative infinity, convert to maximum (24 bit) values.
                return (uint32_t) ((lsign << 31) | ((0x7eL + 128 - 64) << 23) |  (0xffffL << 7));
            } else {
                // Nan -- Just set to zero.
                return 0;
            }
        }
        // Normalized number
        exponent += (128 - 64);
        mantissa <<= 7;
        // Assemble sign, exponent and mantissa.
        return (uint32_t) ((lsign << 31) | (exponent << 23) | mantissa);
    }

    void fill(const float value, const bool multithread = false);
    void multiply(float factor, bool multithread);
    void                 normalizeFloat(float srcMinVal, float srcMaxVal) override;
    void                 normalizeFloatTo1(bool multithread=true);
    void                 normalizeFloatTo65535(bool multithread=true);
    void                 ExecCMSTransform(cmsHTRANSFORM hTransform);
    void                 ExecCMSTransform(cmsHTRANSFORM hTransform, const LabImage &labImage, int cx, int cy);
};

}
