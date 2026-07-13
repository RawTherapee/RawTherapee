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

#include <array>

#include "rt_math.h"
#include "LUT.h"
#include "lcms2.h"
#include "sleef.h"

namespace Glib
{
class ustring;
}

namespace rtengine
{

typedef std::array<double, 7> GammaValues;

class Color
{

private:
    // Jacques' 195 LUTf for Munsell Lch correction
    static LUTf _4P10, _4P20, _4P30, _4P40, _4P50, _4P60;
    static LUTf _1P10, _1P20, _1P30, _1P40, _1P50, _1P60;
    static LUTf _5B40, _5B50, _5B60, _5B70, _5B80;
    static LUTf _7B40, _7B50, _7B60, _7B70, _7B80;
    static LUTf _9B40, _9B50, _9B60, _9B70, _9B80;
    static LUTf _10B40, _10B50, _10B60, _10B70, _10B80;
    static LUTf _05PB40, _05PB50, _05PB60, _05PB70, _05PB80;
    static LUTf _10PB10, _10PB20, _10PB30, _10PB40, _10PB50, _10PB60;
    static LUTf _9PB10, _9PB20, _9PB30, _9PB40, _9PB50, _9PB60, _9PB70, _9PB80;
    static LUTf _75PB10, _75PB20, _75PB30, _75PB40, _75PB50, _75PB60, _75PB70, _75PB80;
    static LUTf _6PB10, _6PB20, _6PB30, _6PB40, _6PB50, _6PB60, _6PB70, _6PB80;
    static LUTf _45PB10, _45PB20, _45PB30, _45PB40, _45PB50, _45PB60, _45PB70, _45PB80;
    static LUTf _3PB10, _3PB20, _3PB30, _3PB40, _3PB50, _3PB60, _3PB70, _3PB80;
    static LUTf _15PB10, _15PB20, _15PB30, _15PB40, _15PB50, _15PB60, _15PB70, _15PB80;
    static LUTf _10YR20, _10YR30, _10YR40, _10YR50, _10YR60, _10YR70, _10YR80, _10YR90;
    static LUTf _85YR20, _85YR30, _85YR40, _85YR50, _85YR60, _85YR70, _85YR80, _85YR90;
    static LUTf  _7YR30, _7YR40, _7YR50, _7YR60, _7YR70, _7YR80;
    static LUTf  _55YR30, _55YR40, _55YR50, _55YR60, _55YR70, _55YR80, _55YR90;
    static LUTf  _4YR30, _4YR40, _4YR50, _4YR60, _4YR70, _4YR80;
    static LUTf  _25YR30, _25YR40, _25YR50, _25YR60, _25YR70;
    static LUTf  _10R30, _10R40, _10R50, _10R60, _10R70;
    static LUTf  _9R30, _9R40, _9R50, _9R60, _9R70;
    static LUTf  _7R30, _7R40, _7R50, _7R60, _7R70;
    static LUTf  _5R10, _5R20, _5R30;
    static LUTf  _25R10, _25R20, _25R30;
    static LUTf  _10RP10, _10RP20, _10RP30;
    static LUTf  _7G30, _7G40, _7G50, _7G60, _7G70, _7G80;
    static LUTf  _5G30, _5G40, _5G50, _5G60, _5G70, _5G80;
    static LUTf  _25G30, _25G40, _25G50, _25G60, _25G70, _25G80;
    static LUTf  _1G30, _1G40, _1G50, _1G60, _1G70, _1G80;
    static LUTf  _10GY30, _10GY40, _10GY50, _10GY60, _10GY70, _10GY80;
    static LUTf  _75GY30, _75GY40, _75GY50, _75GY60, _75GY70, _75GY80;
    static LUTf  _5GY30, _5GY40, _5GY50, _5GY60, _5GY70, _5GY80;

    // Separated from init() to keep the code clear
    static void initMunsell ();
    static double hue2rgb(double p, double q, double t);
    static float hue2rgbfloat(float p, float q, float t);
#if defined(__SSE2__) || defined(RT_SIMDE)
    static vfloat hue2rgb(vfloat p, vfloat q, vfloat t);
#endif

    static float computeXYZ2Lab(float f);

public:

    typedef enum Channel {
        CHANNEL_RED            = 1 << 0,
        CHANNEL_GREEN          = 1 << 1,
        CHANNEL_BLUE           = 1 << 2,
        CHANNEL_HUE            = 1 << 3,
        CHANNEL_SATURATION     = 1 << 4,
        CHANNEL_VALUE          = 1 << 5,
        CHANNEL_LIGHTNESS      = 1 << 6,
        CHANNEL_CHROMATICITY   = 1 << 7
    } eChannel;

    typedef enum InterpolationPath {
        IP_SHORTEST, /// Interpolate color using the shortest path between 2 hues
        IP_LONGEST,  /// Interpolate color using the longest path between 2 hues
    } eInterpolationPath;

    typedef enum InterpolationDirection {
        ID_UP,       /// Interpolate color by increasing the hue value, crossing the upper limit
        ID_DOWN      /// Interpolate color by decreasing the hue value, crossing the lower limit
    } eInterpolationDirection;

    using Triple = std::array<double, 3>;

    using Matrix = std::array<Triple, 3>;

    // Wikipedia sRGB: Unlike most other RGB color spaces, the sRGB gamma cannot be expressed as a single numerical value.
    // The overall gamma is approximately 2.2, consisting of a linear (gamma 1.0) section near black, and a non-linear section elsewhere involving a 2.4 exponent
    // and a gamma (slope of log output versus log input) changing from 1.0 through about 2.3.
    constexpr static double sRGBGamma = 2.2;
    constexpr static double sRGBGammaCurve = 2.4;

    constexpr static double eps = 216.0 / 24389.0; //0.008856
    constexpr static double eps_max = MAXVALD * eps; //580.40756;
    constexpr static double kappa = 24389.0 / 27.0; //903.29630;
    constexpr static double kappaInv = 27.0 / 24389.0;
    constexpr static double epsilonExpInv3 = 6.0 / 29.0;

    constexpr static float epsf = eps;
    constexpr static float kappaf = kappa;
    constexpr static float kappaInvf = kappaInv;
    constexpr static float epsilonExpInv3f = epsilonExpInv3;

    constexpr static float D50x = 0.9642f; //0.96422;
    constexpr static float D50z = 0.8249f; //0.82521;
    constexpr static double u0 = 4.0 * static_cast<double>(D50x) / (static_cast<double>(D50x) + 15.0 + 3.0 * static_cast<double>(D50z));
    constexpr static double v0 = 9.0 / (static_cast<double>(D50x) + 15.0 + 3.0 * static_cast<double>(D50z));
    constexpr static double epskap = 8.0;
    constexpr static float epskapf = epskap;

    constexpr static float c1By116 = 1.0 / 116.0;
    constexpr static float c16By116 = 16.0 / 116.0;

    static cmsToneCurve* linearGammaTRC;

    static LUTf cachef;
    static LUTf cachefy;
    static LUTf gamma2curve;

    // look-up tables for the standard srgb gamma and its inverse (filled by init())
    static LUTf igammatab_srgb;
    static LUTf igammatab_srgb1;
    static LUTf gammatab_srgb;
    static LUTf gammatab_srgb327;
    static LUTf gammatab_srgb1;
    static LUTf gammatab_bt709;

    static LUTf denoiseGammaTab;
    static LUTf denoiseIGammaTab;

    static LUTf igammatab_24_17;
    static LUTf igammatab_bt709;
    static LUTf gammatab_24_17a;
    static LUTf gammatab_13_2;
    static LUTf igammatab_13_2;
    static LUTf gammatab_115_2;
    static LUTf igammatab_115_2;
    static LUTf gammatab_145_3;
    static LUTf igammatab_145_3;

    // look-up tables for the simple exponential gamma
    static LUTf gammatab;
    static LUTuc gammatabThumb; // for thumbnails


    static void init ();
    static void cleanup ();

    static inline float computeXYZ2LabY(float f)
    {
        if (f < 0.f) {
            return 327.68f * (kappa * f / MAXVALF);
        } else if (f > 65535.f) {
            return 327.68f * (116.f * xcbrtf(f / MAXVALF) - 16.f);
        } else {
            return cachefy[f];
        }
    }

    /**
    * @brief Extract luminance "sRGB" from red/green/blue values
    * The range of the r, g and b channel has no importance ([0 ; 1] or [0 ; 65535]...) ; r,g,b can be negatives or > max, but must be in "sRGB"
    * @param r red channel
    * @param g green channel
    * @param b blue channel
    * @return luminance value
    */
    // xyz_sRGBD65 : conversion matrix from XYZ to sRGB for D65 illuminant: we use diagonal values
    static float rgbLuminance(float r, float g, float b)
    {
        // WArning: The sum of xyz_sRGBd65[1][] is > 1.0 (i.e. 1.0000001), so we use our own adapted values)
        // 0.2126729,  0.7151521,  0.0721750
        return r * 0.2126729f + g * 0.7151521f + b * 0.0721750f;
    }
    static double rgbLuminance(double r, double g, double b)
    {
        return r * 0.2126729 + g * 0.7151521 + b * 0.0721750;
    }

    static float rgbLuminance(float r, float g, float b, const double workingspace[3][3])
    {
        return static_cast<double>(r) * workingspace[1][0] + static_cast<double>(g) * workingspace[1][1] + static_cast<double>(b) * workingspace[1][2];
    }

    static float rgbLuminance(float r, float g, float b, const float workingspace[3])
    {
        return r * workingspace[0] + g * workingspace[1] + b * workingspace[2];
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    static vfloat rgbLuminance(vfloat r, vfloat g, vfloat b, const vfloat workingspace[3])
    {
        return r * workingspace[0] + g * workingspace[1] + b * workingspace[2];
    }
#endif

    /**
    * @brief Convert red/green/blue to L*a*b
    * @brief Convert red/green/blue to hue/saturation/luminance
    * @param profile output profile name
    * @param profileW working profile name
    * @param r red channel [0 ; 1]
    * @param g green channel [0 ; 1]
    * @param b blue channel [0 ; 1]
    * @param L Lab L channel [0 ; 1] (return value)
    * @param a Lab a channel [0 ; 1] (return value)
    * @param b Lab b channel [0; 1] (return value)
    * @param workingSpace true: compute the Lab value using the Working color space ; false: use the Output color space
    */
    // do not use this function in a loop. It really eats processing time caused by Glib::ustring comparisons
    static void rgb2lab01 (const Glib::ustring &profile, const Glib::ustring &profileW, float r, float g, float b, float &LAB_l, float &LAB_a, float &LAB_b, bool workingSpace);

    /**
    * @brief Convert red/green/blue to hue/saturation/luminance
    * @param r red channel [0 ; 65535]
    * @param g green channel [0 ; 65535]
    * @param b blue channel [0 ; 65535]
    * @param h hue channel [0 ; 1] (return value)
    * @param s saturation channel [0 ; 1] (return value)
    * @param l luminance channel [0; 1] (return value)
    */
    static void rgb2hsl (float r, float g, float b, float &h, float &s, float &l);

    static inline void rgb2slfloat(float r, float g, float b, float &s, float &l)
    {

        float minVal = min(r, g, b);
        float maxVal = max(r, g, b);
        float C = maxVal - minVal;

        l = (maxVal + minVal) * 7.6295109e-6f; // (0.5f / 65535.f)

        if (C < 0.65535f) { // 0.00001f * 65535.f
            s = 0.f;
        } else {

            if (l <= 0.5f) {
                s = C / (maxVal + minVal);
            } else {
                s = C / (131070.f - (maxVal + minVal)); // 131070.f = 2.f * 65535.f
            }
        }
    }

    static inline void rgb2hslfloat(float r, float g, float b, float &h, float &s, float &l)
    {

        float minVal = min(r, g, b);
        float maxVal = max(r, g, b);
        float C = maxVal - minVal;

        l = (maxVal + minVal) * 7.6295109e-6f; // (0.5f / 65535.f)

        if (C < 0.65535f) { // 0.00001f * 65535.f
            h = 0.f;
            s = 0.f;
        } else {

            if (l <= 0.5f) {
                s = C / (maxVal + minVal);
            } else {
                s = C / (131070.f - (maxVal + minVal)); // 131070.f = 2.f * 65535.f
            }

            if ( r == maxVal ) {
                h = (g - b);
            } else if ( g == maxVal ) {
                h = (2.f * C) + (b - r);
            } else {
                h = (4.f * C) + (r - g);
            }

            h /= (6.f * C);

            if ( h < 0.f ) {
                h += 1.f;
            }
        }
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    static void rgb2hsl (vfloat r, vfloat g, vfloat b, vfloat &h, vfloat &s, vfloat &l);
#endif

    /**
    * @brief Convert hue/saturation/luminance in red/green/blue
    * @param h hue channel [0 ; 1]
    * @param s saturation channel [0 ; 1]
    * @param l luminance channel [0 ; 1]
    * @param r red channel [0 ; 65535] (return value)
    * @param g green channel [0 ; 65535] (return value)
    * @param b blue channel [0 ; 65535] (return value)
    */
    static void hsl2rgb (float h, float s, float l, float &r, float &g, float &b);

    static inline void hsl2rgbfloat (float h, float s, float l, float &r, float &g, float &b)
    {

        if (s == 0.f) {
            r = g = b = 65535.f * l;    //  achromatic
        } else {
            float m2;

            if (l <= 0.5f) {
                m2 = l * (1.f + s);
            } else {
                m2 = l + s - l * s;
            }

            float m1 = 2.f * l - m2;

            r = 65535.f * hue2rgbfloat (m1, m2, h * 6.f + 2.f);
            g = 65535.f * hue2rgbfloat (m1, m2, h * 6.f);
            b = 65535.f * hue2rgbfloat (m1, m2, h * 6.f - 2.f);
        }
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    static void hsl2rgb (vfloat h, vfloat s, vfloat l, vfloat &r, vfloat &g, vfloat &b);
#endif

    /**
    * @brief Convert hue/saturation/luminance in red/green/blue
    * @param h hue channel [0 ; 1]
    * @param s saturation channel [0 ; 1]
    * @param l luminance channel [0 ; 1]
    * @param r red channel [0 ; 1] (return value)
    * @param g green channel [0 ; 1] (return value)
    * @param b blue channel [0 ; 1] (return value)
    */
    static void hsl2rgb01 (float h, float s, float l, float &r, float &g, float &b);


    /**
    * @brief Convert red green blue to hue saturation value
    * @param r red channel [0 ; 65535]
    * @param g green channel [0 ; 65535]
    * @param b blue channel [0 ; 65535]
    * @param h hue channel [0 ; 1] (return value)
    * @param s saturation channel [0 ; 1] (return value)
    * @param v value channel [0 ; 1] (return value)
    */
    static void rgb2hsv (float r, float g, float b, float &h, float &s, float &v);

    /**
    * @brief Convert red green blue to hue saturation value
    * @param r red channel [0 ; 1]
    * @param g green channel [0 ; 1]
    * @param b blue channel [0 ; 1]
    * @param h hue channel [0 ; 1] (return value)
    * @param s saturation channel [0 ; 1] (return value)
    * @param v value channel [0 ; 1] (return value)
    */
    static void rgb2hsv01 (float r, float g, float b, float &h, float &s, float &v);

    static inline float rgb2s(float r, float g, float b) // fast version if only saturation is needed
    {
        float var_Min = min(r, g, b);
        float var_Max = max(r, g, b);
        float del_Max = var_Max - var_Min;

        return del_Max / (var_Max == 0.f ? 1.f : var_Max);
    }

    static inline bool rgb2hsvdcp(float r, float g, float b, float &h, float &s, float &v)
    {

        float var_Min = min(r, g, b);

        if(var_Min < 0.f) {
            return false;
        } else {
            float var_Max = max(r, g, b);
            float del_Max = var_Max - var_Min;
            v = var_Max / 65535.f;

            if (fabsf(del_Max) < 0.00001f) {
                h = 0.f;
                s = 0.f;
            } else {
                s = del_Max / var_Max;

                if ( r == var_Max ) {
                    h = (g - b) / del_Max;
                } else if ( g == var_Max ) {
                    h = 2.f + (b - r) / del_Max;
                } else { /*if ( b == var_Max ) */
                    h = 4.f + (r - g) / del_Max;
                }

                if ( h < 0.f ) {
                    h += 6.f;
                } else if ( h > 6.f ) {
                    h -= 6.f;
                }
            }

            return true;
        }
    }

    static inline void rgb2hsvtc(float r, float g, float b, float &h, float &s, float &v)
    {
        const float var_Min = min(r, g, b);
        const float var_Max = max(r, g, b);
        const float del_Max = var_Max - var_Min;

        v = var_Max / 65535.f;

        if (del_Max < 0.00001f) {
            h = 0.f;
            s = 0.f;
        } else {
            s = del_Max / var_Max;

            if (r == var_Max) {
                h = (g < b ? 6.f : 0.f) + (g - b) / del_Max;
            } else if (g == var_Max) {
                h = 2.f + (b - r) / del_Max;
            } else { /*if ( b == var_Max ) */
                h = 4.f + (r - g) / del_Max;
            }
        }
    }

    /**
    * @brief Convert hue saturation value in red green blue
    * @param h hue channel [0 ; 1]
    * @param s saturation channel [0 ; 1]
    * @param v value channel [0 ; 1]
    * @param r red channel [0 ; 65535] (return value)
    * @param g green channel [0 ; 65535] (return value)
    * @param b blue channel [0 ; 65535] (return value)
    */
    static void hsv2rgb (float h, float s, float v, float &r, float &g, float &b);

    static inline void hsv2rgbdcp (float h, float s, float v, float &r, float &g, float &b)
    {
        // special version for dcp which saves 1 division (in caller) and six multiplications (inside this function)
        const int sector = h;  // sector 0 to 5, floor() is very slow, and h is always > 0
        const float f = h - sector; // fractional part of h

        v *= 65535.f;
        const float vs = v * s;
        const float p = v - vs;
        const float q = v - f * vs;
        const float t = p + v - q;

        switch (sector) {
            case 1:
                r = q;
                g = v;
                b = p;
                break;

            case 2:
                r = p;
                g = v;
                b = t;
                break;

            case 3:
                r = p;
                g = q;
                b = v;
                break;

            case 4:
                r = t;
                g = p;
                b = v;
                break;

            case 5:
                r = v;
                g = p;
                b = q;
                break;

            default:
                r = v;
                g = t;
                b = p;
        }
    }

    static void hsv2rgb (float h, float s, float v, int &r, int &g, int &b);


    /**
    * @brief Convert hue saturation value in red green blue
    * @param h hue channel [0 ; 1]
    * @param s saturation channel [0 ; 1]
    * @param v value channel [0 ; 1]
    * @param r red channel [0 ; 1] (return value)
    * @param g green channel [0 ; 1] (return value)
    * @param b blue channel [0 ; 1] (return value)
    */
    static void hsv2rgb01 (float h, float s, float v, float &r, float &g, float &b);


    /**
    * @brief Convert xyz to red/green/blue
    * Color space : sRGB   - illuminant D50 - use matrix sRGB_xyz[]
    * @param x X coordinate [0 ; 1] or [0 ; 65535]
    * @param y Y coordinate [0 ; 1] or [0 ; 65535]
    * @param z Z coordinate [0 ; 1] or [0 ; 65535]
    * @param r red channel [same range than xyz channel] (return value)
    * @param g green channel [same range than xyz channel] (return value)
    * @param b blue channel [same range than xyz channel] (return value)
    */
    static void xyz2srgb (float x, float y, float z, float &r, float &g, float &b);


    /**
    * @brief Convert xyz to red/green/blue
    * Color space : Prophoto   - illuminant D50 -  use the Prophoto_xyz[] matrix
    * @param x X coordinate [0 ; 1] or [0 ; 65535]
    * @param y Y coordinate [0 ; 1] or [0 ; 65535]
    * @param z Z coordinate [0 ; 1] or [0 ; 65535]
    * @param r red channel [same range than xyz channel] (return value)
    * @param g green channel [same range than xyz channel] (return value)
    * @param b blue channel [same range than xyz channel] (return value)
    */
    static void xyz2Prophoto (float x, float y, float z, float &r, float &g, float &b);


    /**
    * @brief Convert rgb in xyz
    * Color space : Prophoto   - illuminant D50 - use matrix xyz_prophoto[]
    * @param r red channel [0 ; 1] or [0 ; 65535] (return value)
    * @param g green channel [0 ; 1] or [0 ; 65535] (return value)
    * @param b blue channel [0 ; 1] or [0 ; 65535] (return value)
    * @param x X coordinate [same range than xyz channel]
    * @param y Y coordinate [same range than xyz channel]
    * @param z Z coordinate [same range than xyz channel]
    */
    static void Prophotoxyz (float r, float g, float b, float &x, float &y, float &z);


    /**
    * @brief Convert xyz in rgb
    * Color space : undefined - use adhoc matrix: rgb_xyz[3][3] (iccmatrice.h) in function of working space
    * @param x X coordinate [0 ; 1] or [0 ; 65535]
    * @param y Y coordinate [0 ; 1] or [0 ; 65535]
    * @param z Z coordinate [0 ; 1] or [0 ; 65535]
    * @param r red channel [same range than xyz channel] (return value)
    * @param g green channel [same range than xyz channel] (return value)
    * @param b blue channel [same range than xyz channel] (return value)
    * @param rgb_xyz[3][3] transformation matrix to use for the conversion
    */
    static void xyz2rgb (float x, float y, float z, float &r, float &g, float &b, const double rgb_xyz[3][3]);
    static void xyz2r (float x, float y, float z, float &r, const double rgb_xyz[3][3]);
    static inline void xyz2rgb (float x, float y, float z, float &r, float &g, float &b, const float rgb_xyz[3][3])
    {
        r = ((rgb_xyz[0][0] * x + rgb_xyz[0][1] * y + rgb_xyz[0][2] * z)) ;
        g = ((rgb_xyz[1][0] * x + rgb_xyz[1][1] * y + rgb_xyz[1][2] * z)) ;
        b = ((rgb_xyz[2][0] * x + rgb_xyz[2][1] * y + rgb_xyz[2][2] * z)) ;
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    static inline void xyz2rgb (vfloat x, vfloat y, vfloat z, vfloat &r, vfloat &g, vfloat &b, const vfloat rgb_xyz[3][3])
    {
        r = ((rgb_xyz[0][0] * x + rgb_xyz[0][1] * y + rgb_xyz[0][2] * z)) ;
        g = ((rgb_xyz[1][0] * x + rgb_xyz[1][1] * y + rgb_xyz[1][2] * z)) ;
        b = ((rgb_xyz[2][0] * x + rgb_xyz[2][1] * y + rgb_xyz[2][2] * z)) ;
    }
#endif


    /**
    * @brief Convert rgb in xyz
    * Color space : undefined - use adhoc matrix : xyz_rgb[3][3] (iccmatrice.h) in function of working space
    * @param r red channel [0 ; 1] or [0 ; 65535]
    * @param g green channel [0 ; 1] or [0 ; 65535]
    * @param b blue channel [0 ; 1] or [0 ; 65535]
    * @param x X coordinate [same range than rgb channel] (return value)
    * @param y Y coordinate [same range than rgb channel] (return value)
    * @param z Z coordinate [same range than rgb channel] (return value)
    * @param xyz_rgb[3][3] transformation matrix to use for the conversion
    */
    static void rgbxyz (float r, float g, float b, float &x, float &y, float &z, const double xyz_rgb[3][3]);
    static void rgbxyY(float r, float g, float b, float &x, float &y, float &Y, const float xyz_rgb[3][3]);
    static void rgbxyz (float r, float g, float b, float &x, float &y, float &z, const float xyz_rgb[3][3]);
#if defined(__SSE2__) || defined(RT_SIMDE)
    static void rgbxyz (vfloat r, vfloat g, vfloat b, vfloat &x, vfloat &y, vfloat &z, const vfloat xyz_rgb[3][3]);
#endif

    /**
    * @brief Convert Lab in xyz
    * @param L L channel [0 ; 32768] ; L can be negative rarely or superior 32768
    * @param a channel [-42000 ; +42000] ; can be more than 42000
    * @param b channel [-42000 ; +42000] ; can be more than 42000
    * @param x X coordinate [0 ; 65535] ; can be negative! (return value)
    * @param y Y coordinate [0 ; 65535] ; can be negative! (return value)
    * @param z Z coordinate [0 ; 65535] ; can be negative! (return value)
    */
    static inline void Lab2XYZ(float L, float a, float b, float &x, float &y, float &z)
    {
        float LL = L / 327.68f;
        float aa = a / 327.68f;
        float bb = b / 327.68f;
        float fy = (c1By116 * LL) + c16By116; // (L+16)/116
        float fx = (0.002f * aa) + fy;
        float fz = fy - (0.005f * bb);
        x = 65535.f * f2xyz(fx) * D50x;
        z = 65535.f * f2xyz(fz) * D50z;
        y = (LL > epskapf) ? 65535.f * fy * fy * fy : 65535.f * LL / kappaf;
    }

    static void L2XYZ(float L, float &x, float &y, float &z);
    static float L2Y(float L);

#if defined(__SSE2__) || defined(RT_SIMDE)
static inline void Lab2XYZ(vfloat L, vfloat a, vfloat b, vfloat &x, vfloat &y, vfloat &z)
{
    vfloat c327d68 = F2V(327.68f);
    L /= c327d68;
    a /= c327d68;
    b /= c327d68;
    vfloat fy = F2V(c1By116) * L + F2V(c16By116);
    vfloat fx = F2V(0.002f) * a + fy;
    vfloat fz = fy - (F2V(0.005f) * b);
    vfloat c65535 = F2V(65535.f);
    x = c65535 * f2xyz(fx) * F2V(D50x);
    z = c65535 * f2xyz(fz) * F2V(D50z);
    vfloat res1 = fy * fy * fy;
    vfloat res2 = L / F2V(kappa);
    y = vself(vmaskf_gt(L, F2V(epskap)), res1, res2);
    y *= c65535;
}
#endif // __SSE2__

    /**
    * @brief Convert xyz in Lab
    * @param x X coordinate [0 ; 65535] ; can be negative or superior to 65535
    * @param y Y coordinate [0 ; 65535] ; can be negative or superior to 65535
    * @param z Z coordinate [0 ; 65535] ; can be negative or superior to 65535
    * @param L L channel [0 ; 32768] ; L can be negative rarely or superior 32768 (return value)
    * @param a channel [-42000 ; +42000] ; can be more than 42000 (return value)
    * @param b channel [-42000 ; +42000] ; can be more than 42000 (return value)
    */
    static void XYZ2Lab(float x, float y, float z, float &L, float &a, float &b);
    static void RGB2Lab(float *X, float *Y, float *Z, float *L, float *a, float *b, const float wp[3][3], int width);
    static void Lab2RGBLimit(float *L, float *a, float *b, float *R, float *G, float *B, const float wp[3][3], float limit, float afactor, float bfactor, int width);
    static void RGB2L(const float *R, const float *G, const float *B, float *L, const float wp[3][3], int width);

    /**
    * @brief Convert Lab in Yuv
    * @param L L channel [0 ; 32768] ; L can be negative rarely or superior 32768
    * @param a channel [-42000 ; +42000] ; can be more than 42000
    * @param b channel [-42000 ; +42000] ; can be more than 42000
    * @param Y luminance channel [0 ; 65535] (return value)
    * @param u red chrominance channel [0 ; 65535] (return value)
    * @param v blue chrominance channel [0 ; 65535] (return value)
    */
    static void Lab2Yuv(float L, float a, float b, float &Y, float &u, float &v);


    /**
    * @brief Convert Yuv in Lab
    * @param Y luminance channel [0 ; 65535]
    * @param u red chrominance channel [0 ; 65535]
    * @param v blue chrominance channel [0 ; 65535]
    * @param L L channel [0 ; 32768] ; L can be negative rarely or superior 32768 (return value)
    * @param a channel [-42000 ; +42000] ; can be more than 42000 (return value)
    * @param b channel [-42000 ; +42000] ; can be more than 42000 (return value)
    */
    static void Yuv2Lab(float Y, float u, float v, float &L, float &a, float &b, const double wp[3][3]);


    /**
    * @brief Convert the 'a' and 'b' channels of the L*a*b color space to 'c' and 'h' channels of the Lch color space (channel 'L' is identical [0 ; 32768])
    * @param a 'a' channel [-42000 ; +42000] ; can be more than 42000
    * @param b 'b' channel [-42000 ; +42000] ; can be more than 42000
    * @param c 'c' channel return value, in [0 ; 42000] ; can be more than 42000 (return value)
    * @param h 'h' channel return value, in [-PI ; +PI] (return value)
    */
    static void Lab2Lch(float a, float b, float &c, float &h);
#if defined(__SSE2__) || defined(RT_SIMDE)
    static void Lab2Lch(float *a, float *b, float *c, float *h, int w);
#endif

    /**
    * @brief Convert 'c' and 'h' channels of the Lch color space to the 'a' and 'b' channels of the L*a*b color space (channel 'L' is identical [0 ; 32768])
    * @param c 'c' channel value, in [0 ; 42000]
    * @param h 'h' channel value, in [-PI ; +PI]
    * @param a 'a' channel [-42000 ; +42000] ; can be more than 42000 (return value)
    * @param b 'b' channel [-42000 ; +42000] ; can be more than 42000 (return value)
    */
    static void Lch2Lab(float c, float h, float &a, float &b);


    /**
    * @brief Convert the 'u' and 'v' channels of the Luv color space to 'c' and 'h' channels of the Lch color space ('L' channel is identical)
    * @param u 'u' channel [unknown range!]
    * @param v 'v' channel [unknown range!]
    * @param c 'c' channel [unknown range!] (return value)
    * @param h 'h' channel [-PI ; +PI] (return value)
    */
    static void Luv2Lch(float u, float v, float &c, float &h);


    /**
    * @brief Convert 'c' and 'h' channels of the Lch color space to the 'u' and 'v' channels of the Luv color space ('L' channel is identical)
    * @param c 'c' channel [unknown range!] ; can be more than 42000
    * @param h 'h' channel [-PI ; +PI]
    * @param u 'u' channel [unknown range!] (return value)
    * @param v 'v' channel [unknown range!] (return value)
    */
    static void Lch2Luv(float c, float h, float &u, float &v);


    /**
    * @brief Return "f" in function of CIE's kappa and epsilon constants
    * @param f f can be fx fy fz where:
    *          fx=a/500 + fy  a=chroma green red [-128 ; +128]
    *          fy=(L+16)/116 L=luminance [0 ; 100]
    *          fz=fy-b/200 b=chroma blue yellow [-128 ; +128]
    */
    static inline double f2xyz(double f)
    {
        return (f > epsilonExpInv3) ? f * f * f : (116. * f - 16.) * kappaInv;

    }
    static inline float f2xyz(float f)
    {
        return (f > epsilonExpInv3f) ? f * f * f : (116.f * f - 16.f) * kappaInvf;
    }
#if defined(__SSE2__) || defined(RT_SIMDE)
    static inline vfloat f2xyz(vfloat f)
    {
        const vfloat epsilonExpInv3v = F2V(epsilonExpInv3f);
        const vfloat kappaInvv = F2V(kappaInvf);
        vfloat res1 = f * f * f;
        vfloat res2 = (F2V(116.f) * f - F2V(16.f)) * kappaInvv;
        return vself(vmaskf_gt(f, epsilonExpInv3v), res1, res2);
    }
#endif

    /**
     * @brief Calculate the effective direction (up or down) to linearly interpolating 2 colors so that it follows the shortest or longest path
     * @param h1 First hue [0 ; 1]
     * @param h2 Second hue [0 ; 1]
     * @param path Path to follow (shortest/longest)
     * @return The interpolation direction
     */
    static inline eInterpolationDirection getHueInterpolationDirection (double h1, double h2, eInterpolationPath path)
    {
        if (path == IP_SHORTEST) {
            if (h2 > h1) {
                if (h2 - h1 <= 0.5) {
                    return ID_UP;
                } else {
                    return ID_DOWN;
                }
            } else {
                if (h1 - h2 <= 0.5) {
                    return ID_DOWN;
                } else {
                    return ID_UP;
                }
            }
        } else {
            if (h2 > h1) {
                if (h2 - h1 <= 0.5) {
                    return ID_DOWN;
                } else {
                    return ID_UP;
                }
            } else {
                if (h1 - h2 <= 0.5) {
                    return ID_UP;
                } else {
                    return ID_DOWN;
                }
            }
        }
    }


    /**
     * @brief Calculate a color by linearly interpolating 2 colors
     * @param h1 First hue
     * @param h2 Second hue
     * @param balance Factor from 0 (first hue) to 1 (second hue)
     * @param dir Tells which direction the interpolation have to follow. You can get the value with getHueInterpolationDirection
     * @return The interpolated hue
     */
    static inline double interpolateHueHSV (double h1, double h2, double balance, eInterpolationDirection dir)
    {
        if (h1 == h2) {
            return h1;
        }

        if (dir == ID_DOWN) {
            if (h1 < h2) {
                double temp = h1;
                h1 = h2 - 1.;
                h2 = temp;
                balance = 1. - balance;
            }

            double h3 = h1 + balance * (h2 - h1);

            if (h3 < 0.) {
                h3 += 1.;
            }

            return h3;
        } else {
            if (h1 > h2) {
                h2 += 1.;
            }

            double h3 = h1 + balance * (h2 - h1);

            if (h3 > 1.) {
                h3 -= 1.;
            }

            return h3;
        }
    }

    /**
    * @brief Interpolate 2 colors from their respective red/green/blue channels, with a balance factor
    * @param balance gives weight to the first and second color [0 ; 1]
    *                0. = output color == first color
    *                0.5 = output color == equally mixed colors
    *                1. = output color == second color
    * @param r1 red channel of color 1 [0 ; 65535]
    * @param g1 green channel of color 1 [0 ; 65535]
    * @param b1 blue channel of color 1 [0 ; 65535]
    * @param r2 red channel of color 2 [0 ; 65535]
    * @param g2 green channel of color 2 [0 ; 65535]
    * @param b2 blue channel of color 2 [0 ; 65535]
    * @param channels bitfield of channel to interpolate (CHANNEL_LIGHTNESS|CHANNEL_CHROMATICITY|CHANNEL_HUE)
    * @param xyz_rgb color space
    * @param ro red channel of output color [0 ; 65535] (return value)
    * @param go green channel of output color [0 ; 65535] (return value)
    * @param bo blue channel of output color [0 ; 65535] (return value)
    */
    static void interpolateRGBColor (float balance, float r1, float g1, float b1, float r2, float g2, float b2, int channels, const double xyz_rgb[3][3], const double rgb_xyz[3][3], float &ro, float &go, float &bo);

    /**
    * @brief Interpolate 2 colors from their respective red/green/blue channels, with a balance factor
    * @param realL luminance hsl [0; 1]
    * @param iplow low luminance for rl [0;1]
    * @param ihigh high luminance for r2 [0;1]
    * @param algm algorithm [0;2]
    * @param balance gives weight to the first and second color [0 ; 1]
    *                0. = output color == first color
    *                0.5 = output color == equally mixed colors
    *                1. = output color == second color
    * @param twoc 2 colors or 512 int
    * @param r1 red channel of color 1 [0 ; 65535]
    * @param g1 green channel of color 1 [0 ; 65535]
    * @param b1 blue channel of color 1 [0 ; 65535]
    * @param rl red channel of color low [0 ; 65535]
    * @param gl green channel of color low [0 ; 65535]
    * @param bl blue channel of color low [0 ; 65535]

    * @param r2 red channel of color 2 or high[0 ; 65535]
    * @param g2 green channel of color 2 or high[0 ; 65535]
    * @param b2 blue channel of color 2 [or high 0 ; 65535]
    * @param channels bitfield of channel to interpolate (CHANNEL_LIGHTNESS|CHANNEL_CHROMATICITY|CHANNEL_HUE)
    * @param xyz_rgb color space
    * @param rgb_xyz inverse color space
    * @param ro red channel of output color [0 ; 65535] (return value)
    * @param go green channel of output color [0 ; 65535] (return value)
    * @param bo blue channel of output color [0 ; 65535] (return value)
    */
    static void interpolateRGBColor (float realL, float iplow, float iphigh, int algm, float balance, int twoc, int metchrom, float chromat, float luma, float r1, float g1, float b1, float xl, float yl, float zl, float x2, float y2, float z2, const double xyz_rgb[3][3], const double rgb_xyz[3][3], float &ro, float &go, float &bo);


    /**
    * @brief Interpolate a hue value as the angle of a polar coordinate with hue in the [0;1] range
    * Chose the shorter path from hue 1 to hue 2.
    * @param h1 First hue [0; 1]
    * @param h2 Second hue  [0; 1]
    * @param balance Interpolation factor [0 ; 1] where 0.=h1, 1.=h2
    * @return the interpolated value [0;1]
    */
    /*template <typename T, typename U>
    static inline T interpolatePolarHue_01 (T h1, T h2, U balance) {

        if (h1==h2)
            return h1;
        if ((h1 > h2) && (h1-h2 > T(0.5))){
            h1 -= T(1.);
            double value = h1 + T(balance) * (h2-h1);
            if (value < T(0.))
                value += T(1.);
            return value;
        }
        else if (h2-h1 > T(0.5)) {
            h2 -= T(1.);
            double value = h1 + T(balance) * (h2-h1);
            if (value < T(0.))
                value += T(1.);
            return value;
        }
        else
            return h1 + T(balance) * (h2-h1);
    }*/


    /**
    * @brief Interpolate a hue value as the angle of a polar coordinate with hue in the [-PI ; +PI] range
    * Chose the shorter path from hue 1 to hue 2.
    * @param h1 First hue [-PI ; +PI]
    * @param h2 Second hue [-PI ; +PI]
    * @param balance Interpolation factor [0 ; 1] where 0.=h1, 1.=h2
    * @return the interpolated value [-PI ; +PI]
    */
    /*template <typename T, typename U>
    static inline T interpolatePolarHue_PI (T h1, T h2, U balance) {
        if (h1==h2)
            return h1;
        if ((h1 > h2) && (h1-h2 > T(rtengine::RT_PI))){
            h1 -= T(2*rtengine::RT_PI);
            T value = h1 + T(balance) * (h2-h1);
            if (value < T(-rtengine::RT_PI))
                value += T(2*rtengine::RT_PI);
            return value;
        }
        else if (h2-h1 > T(rtengine::RT_PI)) {
            h2 -= T(2*rtengine::RT_PI);
            T value = h1 + T(balance) * (h2-h1);
            if (value < T(0))
                value += T(2*rtengine::RT_PI);
            return value;
        }
        else
            return h1 + T(balance) * (h2-h1);
    }*/


    /**
    * @brief Interpolate a hue value as the angle of a polar coordinate with hue in the [0;1] range
    * Chose the shorter path from hue 1 to hue 2.
    * @param h1 First hue [0; 1]
    * @param h2 Second hue  [0; 1]
    * @param balance Interpolation factor [0 ; 1] where 0.=h1, 1.=h2
    * @return the interpolated value [0;1]
    */
    template <typename T, typename U>
    static inline T interpolatePolarHue_01 (T h1, T h2, U balance)
    {
        float d = h2 - h1;
        float f;
        f = T(balance);
        double h;

        if (h1 > h2) {
            std::swap(h1, h2);
            d = -d;
            f = 1.f - f;
        }

        if (d < T(-rtengine::RT_PI) || d < T(0) || d > T(rtengine::RT_PI)) { //there was an inversion here !! d > T(rtengine::RT_PI)
            h1 += T(2 * rtengine::RT_PI);
            h = h1 + f * (h2 - h1);
            h = std::fmod(h, 2 * rtengine::RT_PI);
        } else {
            h = h1 + f * d;
        }

        // not strictly necessary..but in case of
        if(h < T(-rtengine::RT_PI)) {
            h = T(2 * rtengine::RT_PI) - h;
        }

        if(h > T(rtengine::RT_PI)) {
            h = h - T(2 * rtengine::RT_PI);
        }

        return h;
    }


    /**
    * @brief Interpolate a hue value as the angle of a polar coordinate with hue in the [-PI ; +PI] range
    * Chose the shorter path from hue 1 to hue 2.
    * @param h1 First hue [-PI ; +PI]
    * @param h2 Second hue [-PI ; +PI]
    * @param balance Interpolation factor [0 ; 1] where 0.=h1, 1.=h2
    * @return the interpolated value [-PI ; +PI ]
    */
    template <typename T, typename U>
    static inline T interpolatePolarHue_PI (T h1, T h2, U balance)
    {
        T d = h2 - h1;
        T f;
        f = T(balance);
        T h;

        if (h1 > h2) {
            std::swap(h1, h2);
            d = -d;
            f = 1.f - f;
        }

        if (d < T(0) || d < T(0.5) || d > T(1.)) { //there was an inversion here !! d > T(rtengine::RT_PI)
            h1 += T(1.);
            h = h1 + f * (h2 - h1);
            h = std::fmod(h, T(1.));
        } else {
            h = h1 + f * d;
        }

        // not strictly necessary..but in case of
        if(h < T(0)) {
            h = T(1.) - h;
        }

        if(h > T(1)) {
            h = h - T(1.);
        }

        return h;
    }

    /**
    * @brief Get the gamma curves' parameters used by LCMS2
    * @param pwr gamma value [>1]
    * @param ts slope [0 ; 20]
    * @param gamma a pointer to an array of 6 double gamma values:
    *        gamma0 used in ip2Lab2rgb [0 ; 1], usually near 0.5 (return value)
    *        gamma1 used in ip2Lab2rgb [0 ; 20], can be superior to 20, but it's quite unusual(return value)
    *        gamma2 used in ip2Lab2rgb [0 ; 1], usually near 0.03(return value)
    *        gamma3 used in ip2Lab2rgb [0 ; 1], usually near 0.003(return value)
    *        gamma4 used in ip2Lab2rgb [0 ; 1], usually near 0.03(return value)
    *        gamma5 used in ip2Lab2rgb [0 ; 1], usually near 0.5 (return value)
    */
    static void calcGamma (double pwr, double ts, GammaValues &gamma);


    /**
    * @brief Used by Black and White to correct gamma for each channel red, green and blue channel
    * @param r red channel input and output value [0 ; 65535]
    * @param g green channel input and output value [0 ; 65535]
    * @param b blue channel input and output value [0 ; 65535]
    * @param gammabwr gamma value for red channel [>0]
    * @param gammabwg gamma value for red channel [>0]
    * @param gammabwb gamma value for red channel [>0]
    */
    static void trcGammaBW (float &r, float &g, float &b, float gammabwr, float gammabwg, float gammabwb);
#if defined(__SSE2__) || defined(RT_SIMDE)
    static void trcGammaBWRow (float *r, float *g, float *b, int width, float gammabwr, float gammabwg, float gammabwb);
#endif


    /** @brief Compute the B&W constants for the Black and White processing and its GUI
    * @param setting main mode
    * @param filter string of the filter effect to use
    * @param algo choice between linear and special for OYCPM colors
    * @param mixerRed red channel value of the channel mixer [-100 ; +200]
    * @param mixerGreen green channel value of the channel mixer [-100 ; +200]
    * @param mixerBlue blue channel value of the channel mixer [-100 ; +200]
    * @param mixerOrange orange channel value of the channel mixer [-100 ; +200]
    * @param mixerYellow yellow channel value of the channel mixer [-100 ; +200]
    * @param mixerCyan cyan channel value of the channel mixer [-100 ; +200]
    * @param mixerPurple purple channel value of the channel mixer [-100 ; +200]
    * @param mixerMagenta magenta channel value of the channel mixer [-100 ; +200]
    * @param autoc automatic mode of the channel mixer
    * @param complement adjust complementary channel
    * @param kcorec in absolute mode, value to correct the mixer [1 ; 3], usually near 1 (return value)
    * @param rrm red channel of the mixer (return value)
    * @param ggm green channel of the mixer (return value)
    * @param bbm blue channel of the mixer (return value)
    */
    static void computeBWMixerConstants (const Glib::ustring &setting, const Glib::ustring &filter, const Glib::ustring &algo, float &filcor, float &mixerRed, float &mixerGreen,
                                         float &mixerBlue, float mixerOrange, float mixerYellow, float mixerCyan, float mixerPurple, float mixerMagenta,
                                         bool autoc, bool complement, float &kcorec, double &rrm, double &ggm, double &bbm);


    // standard srgb gamma and its inverse

    /**
    * @brief sRGB gamma
    * See also calcGamma above with the following values: pwr=2.399  ts=12.92310  mode=0.003041  imax=0.055
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamma2     (double x)      //  g3                  1+g4
    {
        //  return x <= 0.003041 ? x * 12.92310 : 1.055 * exp(log(x) / 2.39990) - 0.055;//calculate with calcgamma
        //return x <= 0.0031308 ? x * 12.92310 : 1.055 * exp(log(x) / sRGBGammaCurve) - 0.055;//standard discontinuous
        //very small differences between the 2
        return x <= 0.003040 ? x * 12.92310 : 1.055 * exp(log(x) / sRGBGammaCurve) - 0.055;//continuous
        //  return x <= 0.003041 ? x * 12.92310 : 1.055011 * exp(log(x) / sRGBGammaCurve) - 0.055011;//continuous
    }


    /**
    * @brief Inverse sRGB gamma
    * See also calcGamma above with the following values: pwr=2.3999  ts=12.92310  mode=0.003041  imax=0.055
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the inverse gamma modified's value [0 ; 1]
    */
    static inline double igamma2    (double x)      //g2
    {
        // return x <= 0.039289 ? x / 12.92310 : exp(log((x + 0.055) / 1.055) * 2.39990);//calculate with calcgamma
        // return x <= 0.04045 ? x / 12.92310 : exp(log((x + 0.055) / 1.055) * sRGBGammaCurve);//standard discontinuous
        //very small differences between the 4
        return x <= 0.039286 ? x / 12.92310 : exp(log((x + 0.055) / 1.055) * sRGBGammaCurve);//continuous
        //  return x <= 0.039293 ? x / 12.92310 : exp(log((x + 0.055011) / 1.055011) * sRGBGammaCurve);//continuous
    }


    /**
    * @brief Get the gamma value for Gamma=5.5 Slope=10
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamma55     (double x)     //  g3                  1+g4
    {
        return x <= 0.013189 ? x * 10.0 : 1.593503 * exp(log(x) / 5.5) - 0.593503; // 5.5 10
    }


    /**
    * @brief Get the inverse gamma value for Gamma=5.5 Slope=10
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the inverse gamma modified's value [0 ; 1]
    */
    static inline double igamma55    (double x)     //g2
    {
        return x <= 0.131889 ? x / 10.0 : exp(log((x + 0.593503) / 1.593503) * 5.5); // 5.5 10
    }


    /**
    * @brief Get the gamma value for Gamma=4 Slope=5
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamma4     (double x)      //  g3                  1+g4
    {
        return x <= 0.03089 ? x * 5.0 : 1.478793 * exp(log(x) / 4.1) - 0.478793; // 4  5
    }


    /**
    * @brief Get the inverse gamma value for Gamma=4 Slope=5
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the inverse gamma modified's value [0 ; 1]
    */
    static inline double igamma4    (double x)      //g2
    {
        return x <= 0.154449 ? x / 5.0 : exp(log((x + 0.478793) / 1.478793) * 4.1); // 4 5
    }


/*
    * @brief Get the gamma value for Gamma=2.2 Slope=4.5
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    *
*/
    static inline double gamma709     (double x) {
                                            return x <= 0.0176 ? x*4.5 : 1.0954*exp(log(x)/2.2)-0.0954;
                                    }
/*
    * @brief Get the inverse gamma value for Gamma=2.2 Slope=4.5
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the inverse gamma modified's value [0 ; 1]
    *
*/
    static inline double igamma709    (double x) {
                                        return x <= 0.0795 ? x/4.5 : exp(log((x+0.0954)/1.0954)*2.2);
                                    }




    /**
    * @brief Get the gamma value for Gamma=2.4 Slope=17
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamma24_17     (double x)
    {
        return x <= 0.001867 ? x * 17.0 : 1.044445 * exp(log(x) / 2.4) - 0.044445;
    }


    /**
    * @brief Get the inverse gamma value for Gamma=2.4 Slope=17
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the inverse gamma modified's value [0 ; 1]
    */
    static inline double igamma24_17    (double x)
    {
        return x <= 0.031746 ? x / 17.0 : exp(log((x + 0.044445) / 1.044445) * 2.4);
    }


    /**
    * @brief Get the gamma value for Gamma=2.6 Slope=11
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamma26_11     (double x)
    {
        return x <= 0.004921 ? x * 11.0 : 1.086603 * exp(log(x) / 2.6) - 0.086603;
    }


    /**
    * @brief Get the inverse gamma value for Gamma=2.6 Slope=11
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the inverse gamma modified's value [0 ; 1]
    */
    static inline double igamma26_11    (double x)
    {
        return x <= 0.054127 ? x / 11.0 : exp(log((x + 0.086603) / 1.086603) * 2.6);
    }
    /**
    * @brief Get the gamma value for Gamma=1.3 Slope=2
    * @param x red, green or blue channel's value [0 ; 1]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamma13_2     (double x)
    {
        return x <= 0.016613 ? x * 2.0 : 1.009968 * exp(log(x) / 1.3) - 0.009968;
    }

    static inline double igamma13_2    (double x)
    {
        return x <= 0.033226 ? x / 2.0 : exp(log((x + 0.009968) / 1.009968) * 1.3);
    }

    static inline double gamma115_2     (double x)
    {
        return x <= 0.001692 ? x * 2.0 : 1.000508 * exp(log(x) / 1.15) - 0.000508;
    }

    static inline double igamma115_2    (double x)
    {
        return x <= 0.003384 ? x / 2.0 : exp(log((x + 0.000508) / 1.000508) * 1.15);
    }

    static inline double gamma145_3     (double x)
    {
        return x <= 0.009115 ? x * 3.0 : 1.012305 * exp(log(x) / 1.45) - 0.012305;
    }

    static inline double igamma145_3    (double x)
    {
        return x <= 0.027345 ? x / 3.0 : exp(log((x + 0.012305) / 1.012305) * 1.45);
    }

//gamma for Retinex
    static inline double gammareti      (double x, double gamma, double start, double slope, double mul, double add)
    {
        return (x <= start ? x*slope : exp(log(x) / gamma) * mul - add);
    }
    static inline double igammareti     (double x, double gamma, double start, double slope, double mul, double add)
    {
        return (x <= start * slope ? x / slope : exp(log((x + add) / mul) * gamma) );
    }



    // gamma function with adjustable parameters
    //same as above with values calculate with Calcgamma above
    // X range 0..1
    static inline double gamma      (double x, double gamma, double start, double slope, double mul, double add)
    {
        return (x <= start ? x*slope : exp(log(x) / gamma) * mul - add);
    }

    static inline float gammaf      (float x, float gamma, float start, float slope)
    {
        return x <= start ? x * slope : xexpf(xlogf(x) / gamma);
    }

    //fills a LUT of size 65536 using gamma with slope...
    static void gammaf2lut (LUTf &gammacurve, float gamma, float start, float slope, float divisor, float factor);

    static inline double igamma     (double x, double gamma, double start, double slope, double mul, double add)
    {
        return (x <= start * slope ? x / slope : exp(log((x + add) / mul) * gamma) );
    }


    /**
    * @brief Very basic gamma
    * @param x red, green or blue channel's value [0 ; 1]
    * @param gamma gamma value [1 ; 5]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline double gamman      (double x, double gamma)           //standard gamma without slope...
    {
        return exp(log(x) / gamma);
    }

    /**
    * @brief Very basic gamma
    * @param x red, green or blue channel's value [0 ; 1]
    * @param gamma gamma value [1 ; 5]
    * @return the gamma modified's value [0 ; 1]
    */
    static inline float gammanf      (float x, float gamma)           //standard gamma without slope...
    {
        return xexpf(xlogf(x) / gamma);
    }
    //fills a LUT of size 65536 using gamma without slope...
    static void gammanf2lut (LUTf &gammacurve, float gamma, float divisor, float factor);

    /**
    * @brief Very simply inverse gamma
    * @param x red, green or blue channel's value [0 ; 1]
    * @param gamma gamma value [1 ; 5]
    * @return the inverse gamma modified's value [0 ; 1]
    */
    static inline double igamman     (double x, double gamma)           //standard inverse gamma without slope...
    {
        return exp(log(x) * gamma);
    }


    /**
    * @brief Get the gamma value out of look-up tables
    * Calculated with gamma function above. e.g. :
    *    for (int i=0; i<65536; i++)
    *       gammatab_srgb[i] = (65535.0 * gamma2 (i/65535.0));
    * @param x [0 ; 1]
    * @return the gamma modified's value [0 ; 65535]
    */
    static inline float  gamma_srgb       (char x)
    {
        return gammatab_srgb[x];
    }
    static inline float  gamma            (char x)
    {
        return gammatab[x];
    }
    static inline float  igamma_srgb      (char x)
    {
        return igammatab_srgb[x];
    }
    static inline float  gamma_srgb       (int x)
    {
        return gammatab_srgb[x];
    }
    static inline float  gamma            (int x)
    {
        return gammatab[x];
    }
    static inline float  igamma_srgb      (int x)
    {
        return igammatab_srgb[x];
    }
    static inline float  gamma_srgb       (float x)
    {
        return gammatab_srgb[x];
    }
    static inline float  gamma_srgbclipped       (float x)
    {
        return gamma2curve[x];
    }
    static inline float  gamma            (float x)
    {
        return gammatab[x];
    }
    static inline float  igamma_srgb      (float x)
    {
        return igammatab_srgb[x];
    }
    //static inline float  gamma_srgb       (double x) { return gammatab_srgb[x]; }
    //static inline float  gamma            (double x) { return gammatab[x]; }
    //static inline float  igamma_srgb      (double x) { return igammatab_srgb[x]; }

    // code take in ART thanks to Alberto Griggio
    // Rec.2100 PQ curve
    // https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-F.pdf
    // Perceptual Quantization / SMPTE standard ST.2084
    static float eval_PQ_curve(float x, bool oetf);

    // Hybrid-log gamma curve
    // https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-F.pdf
    static float eval_HLG_curve(float x, bool oetf);

    static float eval_ACEScct_curve(float x, bool inverse);

    static void xyz2oklab(float X, float Y, float Z, float &L, float &a, float &b);
    static void oklab2xyz(float L, float a, float b, float &X, float &Y, float &Z);

    template <class T>
    static void rgb2oklab(float R, float G, float B, float &L, float &a, float &b, const T ws[3][3])
    {
        float X, Y, Z;
        rgbxyz(R, G, B, X, Y, Z, ws);
        xyz2oklab(X, Y, Z, L, a, b);
    }

    template <class T>
    static void oklab2rgb(float L, float a, float b, float &R, float &G, float &B, const T iws[3][3])
    {
        float X, Y, Z;
        oklab2xyz(L, a, b, X, Y, Z);
        xyz2rgb(X, Y, Z, R, G, B, iws);
    }

    // --------------------------------  Jacques's Munsell correction


    /**
    * @brief Corrects the color (hue) depending on chromaticity and luminance changes
    *
    * To use in a "for" or "do while" statement.
    *
    * @param lumaMuns true => luminance correction (for delta L > 10) and chroma correction ; false => only chroma
    * @param Lprov1 luminance after [0 ; 100]
    * @param Loldd luminance before [0 ; 100]
    * @param HH hue before [-PI ; +PI]
    * @param Chprov1 chroma after [0 ; 180 (can be superior)]
    * @param CC chroma before [0 ; 180]
    * @param correctionHueChroma hue correction depending on chromaticity (saturation), in radians [0 ; 0.45] (return value)
    * @param correctlum hue correction depending on luminance (brightness, contrast,...), in radians [0 ; 0.45] (return value)
    * @param munsDbgInfo (Debug target only) object to collect information
    */

    static void AllMunsellLch (bool lumaMuns, float Lprov1, float Loldd, float HH, float Chprov1, float CC, float &correctionHueChroma, float &correctlum);
    static void AllMunsellLch (float Lprov1, float HH, float Chprov1, float CC, float &correctionHueChroma);


    /**
    * @brief Correct chromaticity and luminance so that the color stays in the working profile's gamut
    *
    * This function puts the data (Lab) in the gamut of "working profile":
    * it returns the corrected values of the chromaticity and luminance
    *
    * @param HH : hue, in radians [-PI ; +PI]
    * @param Lprov1 : input luminance value, sent back corrected [0 ; 100]  (input & output value)
    * @param Chprov1: input chroma value, sent back corrected [0 ; 180 (can be superior)]  (input & output value)
    * @param R red value of the corrected color [0 ; 65535 but can be negative or superior to 65535] (return value)
    * @param G green value of the corrected color [0 ; 65535 but can be negative or superior to 65535] (return value)
    * @param B blue value of the corrected color [0 ; 65535 but can be negative or superior to 65535] (return value)
    * @param wip working profile
    * @param isHLEnabled true if "Highlight Reconstruction " is enabled
    * @param lowerCoef a float number between [0.95 ; 1.0[
    *                  The nearest it is from 1.0, the more precise it will be, and the longer too as more iteration will be necessary
    * @param higherCoef a float number between [0.95 ; 1.0[
    *                   The nearest it is from 1.0, the more precise it will be, and the longer too as more iteration will be necessary
    * @param neg (Debug target only) to calculate iterations for negatives values
    * @param moreRGB (Debug target only) to calculate iterations for values >65535
    */
    static void gamutLchonly  (float HH, float &Lprov1, float &Chprov1, float &R, float &G, float &B, const double wip[3][3], bool isHLEnabled, float lowerCoef, float higherCoef);
    static void gamutLchonly  (float HH, float2 sincosval, float &Lprov1, float &Chprov1, float &R, float &G, float &B, const double wip[3][3], bool isHLEnabled, float lowerCoef, float higherCoef);
    static void gamutLchonly  (float2 sincosval, float &Lprov1, float &Chprov1, const float wip[3][3], bool isHLEnabled, float lowerCoef, float higherCoef);
    static void gamutLchonly  (float HH, float2 sincosval, float &Lprov1, float &Chprov1, float &saturation, const float wip[3][3], bool isHLEnabled, float lowerCoef, float higherCoef);


    /**
    * @brief Munsell gamut correction
    *
    * This function is the overall Munsell's corrections, but only on global statement. It may be better to use local statement with AllMunsellLch.
    * They are named accordingly :  gamutLchonly and AllMunsellLch
    * It can be used before and after treatment (saturation, gamma, luminance, ...)
    *
    * @param labL L channel input and output image
    *            L channel's usual range is [0 ; 100], but values can be negative or >100
    * @param laba a channel input and output image
    * @param labb b channel input and output image
    *            a and b channel's range is usually [-128 ; +128], but values can be >128
    * @param N Number of pixels to process
    * @param corMunsell performs Munsell correction
    * @param lumaMuns whether to apply luma correction or not (used only if corMuns=true)
    *                 true:  apply luma + chroma Munsell correction if delta L > 10;
    *                 false: leaves luma untouched
    * @param gamut performs gamutLch
    * @param wip matrix for working profile
    * @param multiThread whether to parallelize the loop or not
    */
    static void LabGamutMunsell (float *labL, float *laba, float *labb, int N, bool corMunsell, bool lumaMuns, bool isHLEnabled, bool gamut, const double wip[3][3]);


    /*
    * @brief Skin tone protection factor
    * Skin colors: mixed from NX2 skin color palette, Von Luschan, and photos of white, black, yellow people...
    * There are some little exceptions, but it should cover 99% case.
    * Pay attention to white balance, and do not change hue and saturation, upstream of the modification
    * Used by vibrance
    * @param lum luma value [0 ; 100]
    * @param hue hue value [-PI ; +PI]
    * @param chrom chroma value [0 ; 180]
    * @param satreduc [0.1 ; 1] (return value)
    * @param chromx [0 or 1], actually only 0 is used
    */
    static void SkinSat         (float lum, float hue, float chrom, float &satreduc);//jacques Skin color


    /**
    * @brief Munsell Lch correction
    * Find the right LUT and calculate the correction
    * @param lum luma value [0 ; 100]
    * @param hue hue value [-PI ; +PI]
    * @param chrom chroma value [0 ; 180]
    * @param memChprov store chroma [0 ; 180]
    * @param correction correction value, in radians [0 ; 0.45]
    * @param lbe hue in function of chroma, in radian [-PI ; +PI]
    * @param zone  [1 ; 4]  1=PB correction + sky  2=red yellow correction 3=Green yellow correction  4=Red purple correction
    * @param correctL true=enable the Luminance correction
    */
    static void MunsellLch      (float lum, float hue, float chrom, float memChprov, float &correction, int zone, float &lbe, bool &correctL);//jacques:  Munsell correction


    // -------------------------------- end Munsell


    static void scalered ( float rstprotection, float param, float limit, float HH, float deltaHH, float &scale, float &scaleext);
    static void transitred (float HH, float Chprov1, float dred, float factorskin, float protect_red, float factorskinext, float deltaHH, float factorsat, float &factor);
    static void skinredfloat ( float J, float h, float sres, float Sp, float dred, float protect_red, int sk, float rstprotection, float ko, float &s);

    static inline void pregamutlab(float lum, float hue, float &chr) //big approximation to limit gamut (Prophoto) before good gamut procedure for locallab chroma, to avoid crash
    {
        if (lum >= 95.0f) {
            if (hue > 1.5f && hue < 2.f) {
                chr = 120.f;
            } else if (hue > 0.7f && hue <= 1.5f) {
                chr = 60.f;
            } else {
                chr = 40.f;
            }
        }   else if (lum > 75.f) {
            if (hue > 1.f && hue < 3.14f) {
                chr = 130.f;
            } else if (hue > -0.4f && hue <= 1.f) {
                chr = 80.f;
            } else if (hue > -3.15f && hue < -2.f) {
                chr = 80.f;
            } else {
                chr = 60.f;
            }

        } else if (lum > 35.f) {
            chr = 100.f;
        }   else if (lum > 20.f) {
            if (hue < -1.f && hue > -2.f) {
                chr = 120.f;
            } else {
                chr = 80.f;
            }
        }   else if (lum > 7.f) {
            if (hue < -1.f && hue > -1.8f) {
                chr = 120.f;
            } else {
                chr = 60.f;
            }

        }   else {
            if (hue < -1.f && hue > -1.6f) {
                chr = 80.f;
            } else {
                chr = 40.f;
            }

        }

        //      if(lum < 4.f) {
        //          chr = 0.1f;
        //      }
    }


    static inline void SkinSatCbdl (float lum, float hue, float chrom, float skinprot, float &scale, bool neg, float b_l, float t_l, float t_r)
    {

        static const float C9 = 8.f, C8 = 15.f, C7 = 12.f, C4 = 7.f, C3 = 5.f, C2 = 5.f, C1 = 5.f;
        static const float H9 = 0.05f, H8 = 0.25f, H7 = 0.1f, H4 = 0.02f, H3 = 0.02f, H2 = 0.1f, H1 = 0.1f, H10 = -0.2f, H11 = -0.2f;

        // "real" skin color : take into account a slight usage of contrast and saturation in RT if option "skin" = 1, uses implicit factor 1.0
        // wide area skin color, useful if not accurate colorimetry or if the user has changed hue and saturation, uses explicit factor 0.6
        // wide area for transition, uses explicit factor 0.4

        if  (lum >= 85.0f) {
            if((hue > (t_l + 0.53f - H9) && hue < (t_r + H9)) && (chrom > 8.0f && chrom < (14.0f + C9))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if (lum >= 92.0f) {
                if((hue > t_l + 0.4f && hue < t_r) && (chrom > 7.0f && chrom < (15.0f))) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > b_l && hue < t_r) && (chrom > 7.0f && chrom < (18.0f))) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if ((hue > t_l + 0.4f && hue < t_r - 0.3f) && (chrom > 7.0f && chrom < (26.0f + C9))) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > b_l + 0.05f && hue < t_r) && (chrom > 7.0f && chrom < (35.0f + C9))) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 70.0f) {
            if((hue > t_l + 0.15f && hue < (t_r - 0.2f + H8)) && (chrom > 8.0f && chrom < (35.0f + C8))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 52.0f) {
            if((hue > t_l && hue < (t_r + H7)) && (chrom > 11.0f && chrom < (35.0f + C7))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 35.0f) {
            if((hue > t_l && hue <  (t_r + H4)) && (chrom > 13.0f && chrom < (37.0f + C4))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 20.0f) {
            if((hue > t_l && hue < (t_r + H3)) && (chrom > 7.0f && chrom < (35.0f + C3) )) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 10.0f) {
            if((hue > (t_l - 0.25f + H10) && hue < (t_r - 0.3f + H2)) && (chrom > 8.0f && chrom < (23.0f + C2))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (35.0f + C1) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.1f) && (chrom > 7.0f && chrom < (45.0f + C1) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if ((hue > (t_l - 0.2f + H10) && hue < (t_r - 0.3f + H1)) && (chrom > 8.0f && chrom < (23.0f + C1))) {
            scale = (100.f - skinprot) / 100.1f;
        } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (35.0f + C1) )) {
            scale = (100.f - skinprot * 0.6f) / 100.1f;
        } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.1f) && (chrom > 7.0f && chrom < (45.0f + C1) )) {
            scale = (100.f - skinprot * 0.4f) / 100.1f;
        }

        //extended zone for hair, beard and if user adjust high value for skinprot
        if(skinprot > 85.f && chrom < 20.f && neg) {
            float modula = -0.0666f * skinprot + 6.66f;
            scale *= modula;
        }
    }

    static inline void SkinSatCbdl2 (float lum, float hue, float chrom, float skinprot, float &scale, bool neg, float b_l, float t_l, float t_r, float b_r, int basc)
    {

        static const float C9 = 8.f, C8 = 15.f, C7 = 12.f, C4 = 7.f, C3 = 5.f, C2 = 5.f, C1 = 5.f;
        static const float H9 = 0.05f, H8 = 0.25f, H7 = 0.1f, H4 = 0.02f, H3 = 0.02f, H2 = 0.1f, H1 = 0.1f, H10 = -0.2f, H11 = -0.2f;

        // "real" skin color : take into account a slight usage of contrast and saturation in RT if option "skin" = 1, uses implicit factor 1.0
        // wide area skin color, useful if not accurate colorimetry or if the user has changed hue and saturation, uses explicit factor 0.6
        // wide area for transition, uses explicit factor 0.4
        if((b_l > -0.3f && b_r < 2.f)  || basc == 0) { //range maxi skin
            if  (lum >= 85.0f) {
                if((hue > (t_l + 0.53f - H9) && hue < (t_r + H9)) && (chrom > 8.0f && chrom < (14.0f + C9))) {
                    scale = (100.f - skinprot) / 100.1f;
                } else if (lum >= 92.0f) {
                    if((hue > t_l + 0.4f && hue < t_r) && (chrom > 7.0f && chrom < (15.0f))) {
                        scale = (100.f - skinprot * 0.6f) / 100.1f;
                    } else if ((hue > b_l && hue < t_r) && (chrom > 7.0f && chrom < (18.0f))) {
                        scale = (100.f - skinprot * 0.4f) / 100.1f;
                    }
                } else if ((hue > t_l + 0.4f && hue < t_r - 0.3f) && (chrom > 7.0f && chrom < (26.0f + C9))) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > b_l + 0.05f && hue < t_r) && (chrom > 7.0f && chrom < (35.0f + C9))) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if (lum >= 70.0f) {
                if((hue > t_l + 0.15f && hue < (t_r - 0.2f + H8)) && (chrom > 8.0f && chrom < (35.0f + C8))) {
                    scale = (100.f - skinprot) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if (lum >= 52.0f) {
                if((hue > t_l && hue < (t_r + H7)) && (chrom > 11.0f && chrom < (35.0f + C7))) {
                    scale = (100.f - skinprot) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if (lum >= 35.0f) {
                if((hue > t_l && hue <  (t_r + H4)) && (chrom > 13.0f && chrom < (37.0f + C4))) {
                    scale = (100.f - skinprot) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if (lum >= 20.0f) {
                if((hue > t_l && hue < (t_r + H3)) && (chrom > 7.0f && chrom < (35.0f + C3) )) {
                    scale = (100.f - skinprot) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if (lum >= 10.0f) {
                if((hue > (t_l - 0.25f + H10) && hue < (t_r - 0.3f + H2)) && (chrom > 8.0f && chrom < (23.0f + C2))) {
                    scale = (100.f - skinprot) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (35.0f + C1) )) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.1f) && (chrom > 7.0f && chrom < (45.0f + C1) )) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if ((hue > (t_l - 0.2f + H10) && hue < (t_r - 0.3f + H1)) && (chrom > 8.0f && chrom < (23.0f + C1))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (35.0f + C1) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.1f) && (chrom > 7.0f && chrom < (45.0f + C1) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }

            //extended zone for hair, beard and if user adjust high value for skinprot
            if(skinprot > 85.f && chrom < 20.f && neg) {
                float modula = -0.0666f * skinprot + 6.66f;
                scale *= modula;
            }
        }
        //end hue skin algo
        else if (basc == 1) { //not hue skin  linear transition or mod chroma curve
            if(hue >= t_l && hue <= t_r) {
                scale = (100.f - skinprot) / 100.1f;
            } else if(hue > b_l && hue < t_l) {
                float sc = (100.f - skinprot) / 100.1f;
                float aa = (1.f - sc) / (b_l - t_l);
                float bb = 1.f - aa * b_l;
                scale = aa * hue + bb;
            } else if(hue > t_r && hue < b_r) {
                float sc = (100.f - skinprot) / 100.1f;
                float aa = (sc - 1.f) / (t_r - b_r);
                float bb = 1.f - aa * b_r;
                scale = aa * hue + bb;
            }
        }
    }



    static inline void SkinSatCbdlCam (float lum, float hue, float chrom, float skinprot, float &scale, bool neg, float b_l, float t_l, float t_r)
    {

        static const float C9 = 8.f, C8 = 15.f, C7 = 12.f, C4 = 7.f, C3 = 5.f, C2 = 5.f, C1 = 5.f;
        static const float H9 = 0.05f, H8 = 0.25f, H7 = 0.1f, H4 = 0.02f, H3 = 0.02f, H2 = 0.1f, H1 = 0.1f, H10 = -0.2f, H11 = -0.2f;

        float HH = 0.f;

        if     (hue > 8.6f  && hue <= 74.f ) {
            HH = (1.15f / 65.4f) * hue - 0.0012f;   //H > 0.15   H<1.3
        } else if(hue > 0.f   && hue <= 8.6f ) {
            HH = (0.19f / 8.6f ) * hue - 0.04f;   //H>-0.04 H < 0.15
        } else if(hue > 355.f && hue <= 360.f) {
            HH = (0.11f / 5.0f ) * hue - 7.96f;   //H>-0.15 <-0.04
        } else if(hue > 74.f  && hue < 95.f  ) {
            HH = (0.30f / 21.0f) * hue + 0.24285f;   //H>1.3  H<1.6
        } else if(hue >= 95.f && hue < 137.5f) {
            HH = 0.01882f * hue - 0.18823f;   // H>1.6 H<2.4
        } else if(hue > 285.f && hue <= 355.f)  {
            HH = 0.1642f * hue - 5.982f;   //HH>-1.3  HH <-0.15
        }

        hue = HH;

        // "real" skin color : take into account a slight usage of contrast and saturation in RT if option "skin" = 1, uses implicit factor 1.0
        // wide area skin color, useful if not accurate colorimetry or if the user has changed hue and saturation, uses explicit factor 0.6
        // wide area for transition, uses explicit factor 0.4

        if  (lum >= 85.0f) {
            if((hue > (t_l + 0.53f - H9) && hue < (t_r + H9)) && (chrom > 8.0f && chrom < (14.0f + C9))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if (lum >= 92.0f) {
                if((hue > t_l + 0.4f && hue < t_r) && (chrom > 7.0f && chrom < (15.0f))) {
                    scale = (100.f - skinprot * 0.6f) / 100.1f;
                } else if ((hue > b_l && hue < t_r) && (chrom > 7.0f && chrom < (18.0f))) {
                    scale = (100.f - skinprot * 0.4f) / 100.1f;
                }
            } else if ((hue > t_l + 0.4f && hue < t_r - 0.3f) && (chrom > 7.0f && chrom < (26.0f + C9))) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > b_l + 0.05f && hue < t_r) && (chrom > 7.0f && chrom < (35.0f + C9))) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 70.0f) {
            if((hue > t_l + 0.15f && hue < (t_r - 0.2f + H8)) && (chrom > 8.0f && chrom < (35.0f + C8))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 52.0f) {
            if((hue > t_l && hue < (t_r + H7)) && (chrom > 11.0f && chrom < (35.0f + C7))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 35.0f) {
            if((hue > t_l && hue <  (t_r + H4)) && (chrom > 13.0f && chrom < (37.0f + C4))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 20.0f) {
            if((hue > t_l && hue < (t_r + H3)) && (chrom > 7.0f && chrom < (35.0f + C3) )) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (48.0f + C9) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r) && (chrom > 7.0f && chrom < (55.0f + C9) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if (lum >= 10.0f) {
            if((hue > (t_l - 0.25f + H10) && hue < (t_r - 0.3f + H2)) && (chrom > 8.0f && chrom < (23.0f + C2))) {
                scale = (100.f - skinprot) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (35.0f + C1) )) {
                scale = (100.f - skinprot * 0.6f) / 100.1f;
            } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.1f) && (chrom > 7.0f && chrom < (45.0f + C1) )) {
                scale = (100.f - skinprot * 0.4f) / 100.1f;
            }
        } else if ((hue > (t_l - 0.2f + H10) && hue < (t_r - 0.3f + H1)) && (chrom > 8.0f && chrom < (23.0f + C1))) {
            scale = (100.f - skinprot) / 100.1f;
        } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.2f) && (chrom > 7.0f && chrom < (35.0f + C1) )) {
            scale = (100.f - skinprot * 0.6f) / 100.1f;
        } else if ((hue > (b_l + 0.07f + H11) && hue < t_r - 0.1f) && (chrom > 7.0f && chrom < (45.0f + C1) )) {
            scale = (100.f - skinprot * 0.4f) / 100.1f;
        }

        //extended zone for hair, beard and if user adjust high value for skinprot
        if(skinprot > 85.f && chrom < 20.f && neg) {
            float modula = -0.0666f * skinprot + 6.66f;
            scale *= modula;
        }
    }


    /**
    * @brief Gamut correction in the XYZ color space
    * @param X X channel input value and corrected output value [0 ; 65535]
    * @param Y Y channel input value[0 ; 65535]
    * @param Z Z channel input value and corrected output value [0 ; 65535]
    * @param p working profile
    */
    static void gamutmap(float &X, float Y, float &Z, const double p[3][3]);

    /**
    * @brief Convert primaries in XYZ values in function of illuminant
    * @param p primaries red, gree, blue
    * @param Wx Wy white for illuminant 
    * @param pxyz return matrix XYZ 
    */
    static void primaries_to_xyz (double p[6], double Wx, double Wz, double *pxyz, int cat);

    /**
    * @brief Gamut compress from ACES
    */
    // transpose, multip, mult3 used by ACES
    static void  transpose(const Matrix &ma, Matrix &R);
    static void  multip(const Matrix &ma, const Matrix &mb, Matrix &R);
    static void  mult3(std::array<float, 3> &in, const Matrix &ma, std::array<float, 3> &out);
    //conversion RGB data with Agx matrix and inverse
    static void agx_trans(const std::array<float, 3> &rgb_in, const Matrix &to_agx, float &R, float &G, float &B);

    static void aces_reference_gamut_compression(
        const std::array<float, 3> &rgb_in,
        const std::array<float, 3> &threshold,
        const std::array<float, 3> &distance_limit,
        const Matrix &to_out, const Matrix &from_out,
        float pwr, bool rolloff,
        float &R, float &G, float &B, float &ac, float &ac0, float &ac1, float &ac2);

    /**
    * @brief Get HSV's hue from the Lab's hue
    * @param HH Lab's hue value, in radians [-PI ; +PI]
    * @return HSV's hue value [0 ; 1]
    */
    static inline double huelab_to_huehsv2 (float HH)
    {
        //hr=translate Hue Lab value  (-Pi +Pi) in approximative hr (hsv values) (0 1) [red 1/6 yellow 1/6 green 1/6 cyan 1/6 blue 1/6 magenta 1/6 ]
        // with multi linear correspondences (I expect there is no error !!)
        double hr = 0.0;
        //always put h between 0 and 1

        if      (HH >= 0.f       && HH < 0.6f    ) {
            hr = 0.11666 * double(HH) + 0.93;    //hr 0.93  1.00    full red
        } else if (HH >= 0.6f      && HH < 1.4f    ) {
            hr = 0.1125 * double(HH) - 0.0675;    //hr 0.00  0.09    red yellow orange
        } else if (HH >= 1.4f      && HH < 2.f     ) {
            hr = 0.2666 * double(HH) - 0.2833;    //hr 0.09  0.25    orange yellow
        } else if (HH >= 2.f       && HH < 3.14159f) {
            hr = 0.1489 * double(HH) - 0.04785;    //hr 0.25  0.42    yellow green green
        } else if (HH >= -3.14159f && HH < -2.8f   ) {
            hr = 0.23419 * double(HH) + 1.1557;    //hr 0.42  0.50    green
        } else if (HH >= -2.8f     && HH < -2.3f   ) {
            hr = 0.16   * double(HH) + 0.948;    //hr 0.50  0.58    cyan
        } else if (HH >= -2.3f     && HH < -0.9f   ) {
            hr = 0.12143 * double(HH) + 0.85928;    //hr 0.58  0.75    blue blue-sky
        } else if (HH >= -0.9f     && HH < -0.1f   ) {
            hr = 0.2125 * double(HH) + 0.94125;    //hr 0.75  0.92    purple magenta
        } else if (HH >= -0.1f     && HH < 0.f     ) {
            hr = 0.1    * double(HH) + 0.93;    //hr 0.92  0.93    red
        }
        // in case of !
        if     (hr < 0.0) {
            hr += 1.0;
        } else if(hr > 1.0) {
            hr -= 1.0;
        }

        return (hr);
    }

    static inline double huejz_to_huehsv2 (float HH)
    {
        //hr=translate Hue Jz value  (-Pi +Pi) in approximative hr (hsv values) (0 1) 
        // with multi linear correspondences (I expect another time with Jz there is no error !!)
        double hr = 0.0;
        //always put h between 0 and 1
        // make with my chart 468 colors...
        // HH ==> Hz value  ; hr HSv value
        if      (HH >= 0.2f && HH < 0.75f) {
            hr = 0.12727273 * double(HH) + 0.90454551;//hr 0.93  1.00    full red
        } else if (HH >= 0.75f && HH < 1.35f) {
            hr = 0.15 * double(HH) - 0.1125;//hr 0.00  0.09    red yellow orange
        } else if (HH >= 1.35f && HH < 1.85f) {
            hr = 0.32 * double(HH) - 0.342;    //hr 0.09  0.25    orange yellow
        } else if (HH >= 1.85f && HH < 2.46f) {
            hr = 0.23442623 * double(HH) -0.18368853;//hr 0.25  0.393    yellow green green
        } else if (HH >= 2.46f && HH < 3.14159f) {
            hr = 0.177526 * double(HH) -0.043714;//hr 0.393  0.51315    green  ==> 0.42 Lab
        } else if (HH >= -3.14159f && HH < -2.89f) {
            hr = 0.3009078 * double(HH) + 1.459329;//hr 0.51315  0.5897    green cyan ==> -2.30 Lab
        } else if (HH >= -2.89f && HH < -2.7f) {
            hr = 0.204542 * double(HH) + 1.1808264;//hr 0.5897  0.628563    cyan
        } else if (HH >= -2.7f && HH < -2.17f) {
            hr = 0.121547 * double(HH) + 0.956399;//hr 0.628563  0.692642    blue blue-sky
        } else if (HH >= -2.17f && HH < -0.9f) {
            hr = 0.044882 * double(HH) + 0.789901;//hr 0.692642  0.749563    blue blue-sky
        } else if (HH >= -0.9f && HH < -0.1f) {
            hr = 0.2125 * double(HH) + 0.940813;//hr 0.749563 0.919563    purple magenta
        } else if (HH >= -0.1f && HH < 0.2f) {
            hr = 0.03479 * double(HH) + 0.923042;//hr 0.919563  0.93    red
        }
        // in case of !
        if     (hr < 0.0) {
            hr += 1.0;
        } else if(hr > 1.0) {
            hr -= 1.0;
        }

        return (hr);
    }

// HSV  0.93  1.0 red  -             Lab 0.0  0.6   Jz 0.20 0.75
// HSV  0.00  0.9  red orange -      Lab 0.6  1.4   Jz 0.50 1.35
// HSV  0.09  0.25 oran - yellow -   Lab 1.4  2.0   Jz 1.35 1.85
// HSV  0.25  0.39 yellow - gree -   Lab 2.0  3.0   Jz 1.85 2.40
// HSV  0.39  0.50 green - cyan      Lab 3.0  -2.8  Jz 2.40 3.10
// HSV  0.50  0.58 cyan              Lab-2.8  -2.3  Jz 3.10 -2.90
// HSV  0.58  0.69 blue - sky        Lab-2.3  -1.3  Jz -2.90 -2.17
// HSV  0.69  0.75 blue          -   Lab-1.3  -0.9  Jz -2.17 -0.90
// HSV  0.75  0.92 purple          - Lab-0.9  -0.1  Jz -0.9 -0.10
// HSV  0.92  0.93 magenta           Lab-0.1  0.0  Jz -0.1 0.20


    static inline void RGB2Y(const float* R, const float* G, const float* B, float* Y1, float * Y2, int W) {
        int i = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
        const vfloat c1v = F2V(0.2627f);
        const vfloat c2v = F2V(0.6780f);
        const vfloat c3v = F2V(0.0593f);
        for (; i < W - 3; i += 4) {
            const vfloat Rv = vmaxf(LVFU(R[i]), ZEROV);
            const vfloat Gv = vmaxf(LVFU(G[i]), ZEROV);
            const vfloat Bv = vmaxf(LVFU(B[i]), ZEROV);
            vfloat yv = c1v * Rv + c2v * Gv + c3v * Bv;
            STVFU(Y1[i], yv);
            STVFU(Y2[i], yv);
        }
#endif
        for (; i < W; ++i) {
            const float r = std::max(R[i], 0.f);
            const float g = std::max(G[i], 0.f);
            const float b = std::max(B[i], 0.f);
            Y1[i] = Y2[i] = 0.2627f * r + 0.6780f * g + 0.0593f * b;
        }
    }

};

}
