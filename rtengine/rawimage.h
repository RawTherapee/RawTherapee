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

#include <ctime>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>
#include <glibmm/ustring.h>

#include "dcraw.h"
#include "imageformat.h"


class LibRaw;


namespace rtengine
{


class Image8;


class RawImage: public DCraw
{
public:

    explicit RawImage(const Glib::ustring &name);
    ~RawImage();

    int loadRaw(bool loadData, unsigned int imageNum = 0, bool closeFile = true, ProgressListener *plistener = nullptr, double progressRange = 1.0);
    void get_colorsCoeff(float* pre_mul_, float* scale_mul_, float* cblack_, bool forceAutoWB);
    void set_prefilters()
    {
        if (isBayer() && get_colors() == 3) {
            prefilters = filters;
            filters &= ~((filters & 0x55555555) << 1);
        }
    }
    dcrawImage_t get_image();
    float** compress_image(unsigned int frameNum, bool freeImage = true); // revert to compressed pixels format and release image data
    float** data;             // holds pixel values, data[i][j] corresponds to the ith row and jth column
    unsigned prefilters;               // original filters saved ( used for 4 color processing )
    unsigned int getFrameCount() const { return is_raw; }

    double getBaselineExposure() const { return RT_baseline_exposure; }
 
protected:
    enum class Decoder {
        DCRAW,
        LIBRAW,
    };

    Glib::ustring filename; // complete filename
    int rotate_deg; // 0,90,180,270 degree of rotation: info taken by dcraw from exif
    char* profile_data; // Embedded ICC color profile
    float* allocation; // pointer to allocated memory
    int maximum_c4[4];
    Decoder decoder{Decoder::DCRAW};
    std::pair<int, int> sensorOrigin{};
    std::unique_ptr<LibRaw> libraw;
    std::unique_ptr<std::remove_pointer<dcrawImage_t>::type[]> image_from_float;
    bool isFoveon() const
    {
        return is_foveon;
    }

public:

    static void initCameraConstants(Glib::ustring baseDir);
    std::string get_filename() const
    {
        return filename;
    }
    int get_width()  const
    {
        return width;
    }
    int get_height() const
    {
        return height;
    }
    int get_iwidth()  const
    {
        return iwidth;
    }
    int get_iheight()  const
    {
        return iheight;
    }
    // Sensor coordinates of the first pixel in data, after decoder and camconst crops.
    std::pair<int, int> getSensorOrigin() const { return sensorOrigin; }

    int get_leftmargin()  const
    {
        return left_margin;
    }
    int get_topmargin() const
    {
        return top_margin;
    }

    int get_rawwidth() const
    {
        return raw_width;
    }

    int get_FujiWidth() const
    {
        return fuji_width;
    }

    float const * get_FloatRawImage() const
    {
        return float_raw_image;
    }

    eSensorType getSensorType() const;

    void getRgbCam(float rgbcam[3][4]);
    void getXtransMatrix(int xtransMatrix[6][6]);
    unsigned get_filters() const
    {
        return filters;
    }
    int get_colors() const
    {
        return colors;
    }
    int get_cblack(int i) const
    {
        return cblack[i];
    }
    int get_white(int i) const
    {
        if (maximum_c4[0] > 0) {
            return maximum_c4[i];
        } else {
            return maximum;
        }
    }
    unsigned short get_whiteSample(int r, int c) const
    {
        return white[r][c];
    }

    double get_ISOspeed() const
    {
        return iso_speed;
    }
    double get_shutter()  const
    {
        return shutter;
    }
    double get_aperture()  const
    {
        return aperture;
    }
    time_t get_timestamp() const
    {
        return timestamp;
    }
    int get_rotateDegree() const
    {
        return rotate_deg;
    }
    const std::string get_maker() const
    {
        return std::string(make);
    }
    const std::string get_model() const
    {
        return std::string(model);
    }

    float get_cam_mul(int c)const
    {
        return cam_mul[c];
    }
    float get_pre_mul(int c)const
    {
        if (std::isfinite(pre_mul[c])) {
            return pre_mul[c];
        } else {
            std::cout << "Failure decoding '" << filename << "', please file a bug report including the raw file and the line below:" << std::endl;
            std::cout << "rawimage.h get_pre_mul() : pre_mul[" << c << "] value " << pre_mul[c] << " automatically replaced by value 1.0" << std::endl;
            return 1.f;
        }
    }
    float get_rgb_cam(int r, int c) const
    {
        return rgb_cam[r][c];
    }

    int get_profileLen() const
    {
        return profile_length;
    }
    char* get_profile() const
    {
        return profile_data;
    }
    IMFILE *get_file() const
    {
        return ifp;
    }
    bool is_supportedThumb() const ;
    bool is_jpegThumb() const ;
    bool is_ppmThumb() const ;
    int get_thumbOffset() const
    {
        return int(thumb_offset);
    }
    int get_thumbWidth() const
    {
        return int(thumb_width);
    }
    int get_thumbHeight() const
    {
        return int(thumb_height);
    }
    int get_thumbBPS() const
    {
        return thumb_load_raw ? 16 : 8;
    }
    bool get_thumbSwap() const;
    unsigned get_thumbLength() const
    {
        return thumb_length;
    }
    bool zeroIsBad() const
    {
        return zero_is_bad == 1;
    }

    bool isXtrans() const
    {
        return filters == 9;
    }

    bool isFloat() const
    {
        return float_raw_image;
    }
    void set_filters(unsigned f)
    {
        filters = f;
    }

public:
    // dcraw functions
    void pre_interpolate();

public:
    bool ISRED(unsigned row, unsigned col) const
    {
        return ((filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3) == 0);
    }
    bool ISGREEN(unsigned row, unsigned col) const
    {
        return ((filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3) == 1);
    }
    bool ISBLUE(unsigned row, unsigned col) const
    {
        return ((filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3) == 2);
    }
    unsigned FC(unsigned row, unsigned col) const
    {
        return (filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3);
    }
    bool ISXTRANSRED(unsigned row, unsigned col) const
    {
        return ((xtrans[(row) % 6][(col) % 6]) == 0);
    }
    bool ISXTRANSGREEN(unsigned row, unsigned col) const
    {
        return ((xtrans[(row) % 6][(col) % 6]) == 1);
    }
    bool ISXTRANSBLUE(unsigned row, unsigned col) const
    {
        return ((xtrans[(row) % 6][(col) % 6]) == 2);
    }
    unsigned XTRANSFC(unsigned row, unsigned col) const
    {
        return (xtrans[(row) % 6][(col) % 6]);
    }

    unsigned DNGVERSION() const
    {
        return dng_version;
    }

public:
    bool checkThumbOk() const;
    Image8 *getThumbnail() const;

};

}
