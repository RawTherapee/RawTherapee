/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2016-2024 Jacques Desmis <jdesmis@gmail.com>
 *  Copyright (c) 2018 Alberto Griggio <alberto.griggio@gmail.com>
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
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */

// Locallab Log Encoding Tool - extracted from iplocallab.cc

#include <cmath>
#include <iostream>

#include "improcfun.h"
#include "imagefloat.h"
#include "labimage.h"
#include "rt_math.h"
#include "sleef.h"
#include "color.h"
#include "iccstore.h"
#include "imagesource.h"
#include "guidedfilter.h"
#include "settings.h"
#include "procparams.h"
#include "iplocallab.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace rtengine {

//-----------------------------------------------------------------------------
// Local helper functions for log encoding
//-----------------------------------------------------------------------------

// Copyright 2018 Alberto Griggio <alberto.griggio@gmail.com>
static float find_gray(float source_gray, float target_gray)
{
    // find a base such that log2lin(base, source_gray) = target_gray
    // log2lin is (base^source_gray - 1) / (base - 1), so we solve
    //
    //  (base^source_gray - 1) / (base - 1) = target_gray, that is
    //
    //  base^source_gray - 1 - base * target_gray + target_gray = 0
    //
    // use a bisection method (maybe later change to Netwon)

    if (source_gray <= 0.f) {
        return 0.f;
    }

    const auto f =
    [ = ](float x) -> float {
        return std::pow(x, source_gray) - 1.f - target_gray * x + target_gray;
    };

    // first find the interval we are interested in

    float lo = 1.f;

    while (f(lo) <= 0.f) {
        lo *= 2.f;
    }

    float hi = lo * 2.f;

    while (f(hi) >= 0.f) {
        hi *= 2.f;
    }

    if (std::isinf(hi)) {
        return 0.f;
    }

    // now search for a zero
    for (int iter = 0; iter < 100; ++iter) {
        float mid = lo + (hi - lo) / 2.f;
        float v = f(mid);

        if (std::abs(v) < 1e-4f || (hi - lo) / lo <= 1e-4f) {
            return mid;
        }

        if (v > 0.f) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    return 0.f; // not found
}

// taken from darktable
inline float power_norm(float r, float g, float b)
{
    r = std::abs(r);
    g = std::abs(g);
    b = std::abs(b);

    float r2 = SQR(r);
    float g2 = SQR(g);
    float b2 = SQR(b);

    float d = r2 + g2 + b2;
    float n = r * r2 + g * g2 + b * b2;

    return n / std::max(d, 1e-12f);
}

inline float ev2gray(float ev)
{
    return std::pow(2.f, -ev + std::log2(0.18f));
}


inline float gray2ev(float gray)
{
    return std::log2(0.18f / gray);
}

// copyright 2018 Alberto Griggio <alberto.griggio@gmail.com>

inline float norm2(float r, float g, float b, TMatrix ws)
{
    constexpr float hi = std::numeric_limits<float>::max() / 100.f;
    return std::min(hi, power_norm(r, g, b) / 2.f + Color::rgbLuminance(r, g, b, ws) / 2.f);
}

inline float norm_3(float r, float g, float b, TMatrix ws, float raplim)//lowers the equivalent luminance if the white point is high
{
    constexpr float hi = std::numeric_limits<float>::max() / 100.f;
    float pwn = 0.5f;//standard repartition between XYZ luminance and Out of gamut values
    if (raplim < 1.2f) {//raplim : ratio between the normal value 'reasonable_limit_white_point' and reality
        pwn = 0.55f;//Tested on images with WP linear close to 4 - Near Sunset
    } else if (raplim < 1.5f) {//Very high White point
        pwn = 0.75f;//Tested on images with WP linear close to 5 or 6
    } else {
        pwn = 0.85f;//Tested on images with WP linear close to 6 and above //LEDs
    }
    return std::min(hi, (1.f - pwn) * power_norm(r, g, b) + pwn * Color::rgbLuminance(r, g, b, ws));//I reversed the action of the two components to better account for what happens out of gamut.
}

inline float norm(float r, float g, float b, TMatrix ws)
{
    return (Color::rgbLuminance(r, g, b, ws));
}

//-----------------------------------------------------------------------------
// ImProcFunctions methods for log encoding
//-----------------------------------------------------------------------------

void ImProcFunctions::mean_sig(const float* const * const savenormL, float &meanf, float &stdf, int xStart, int xEnd, int yStart, int yEnd) const
{
    const int size = (yEnd - yStart) * (xEnd - xStart);
    // use double precision for large accumulations
    double meand = 0.0;
    double stdd = 0.0;
#ifdef _OPENMP
    #pragma omp parallel for reduction(+:meand, stdd) if(multiThread)
#endif

    for (int y = yStart; y < yEnd; ++y) {
        for (int x = xStart; x < xEnd; ++x) {
            meand += static_cast<double>(savenormL[y][x]);
            stdd += SQR(static_cast<double>(savenormL[y][x]));
        }
    }

    meand /= size;
    stdd /= size;
    stdd -= SQR(meand);
    stdf = std::sqrt(stdd);
    meanf = meand;
}


// basic log encoding taken from ACESutil.Lin_to_Log2, from
// https://github.com/ampas/aces-dev
// (as seen on pixls.us)
// copyright 2018 Alberto Griggio <alberto.griggio@gmail.com>
void ImProcFunctions::log_encode(Imagefloat *rgb, struct local_params & lp, bool multiThread, int bfw, int bfh)
{
    // BENCHFUN
        float gray = 0.1f;
        float shadows_range = 0.f;
        bool comprlog = 0.f;
        float comprfactorlog = 0.f;
        float dynamic_range = 1.f;
        float targray = 0.1f;

    bool satcontrol = false;

    if(lp.logena) {
        gray = 0.01f * lp.sourcegray;
        shadows_range = lp.blackev;
        comprlog = lp.comprlo  > 0.f;
        comprfactorlog = lp.comprlo;
        dynamic_range = max(lp.whiteev - lp.blackev, 0.5f);
        targray = lp.targetgray;
        satcontrol = lp.satlog;

    } else if (lp.cieena) {
        gray = 0.01f * lp.sourcegraycie;
        shadows_range = lp.blackevjz;
        comprlog = lp.comprlocie  > 0.f;
        comprfactorlog = lp.comprlocie;
        dynamic_range = max(lp.whiteevjz - lp.blackevjz, 0.5f);
        targray = lp.targetgraycie;
        satcontrol = lp.satcie;
    }
    float comprthlog = 1.f;

    const float noise = pow_F(2.f, -16.f);
    const float log2 = xlogf(2.f);
    const float base = targray > 1 && targray < 100 && dynamic_range > 0 ? find_gray(std::abs(shadows_range) / dynamic_range, 0.01f * targray) : 0.f;
    const float linbase = rtengine::max(base, 2.f);//2 to avoid bad behavior
    TMatrix ws = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);
    float ac = -5.f;//max 4
    float bc = 4.f;
    if(comprlog < 0.6f) {
        comprthlog = ac * comprlog + bc;
    } else {
        comprthlog = 1.f;
    }

    if (settings->verbose) {
        printf("Base Log encoding std=%5.1f\n", (double) linbase);
    }

    const auto apply =
    [ = ](float x, bool scale = true) -> float {
        if (scale)
        {
            x /= 65535.f;
        }

        x = rtengine::max(x, noise);
        x = rtengine::max(x / gray, noise);
        if (comprlog && x >= comprthlog)
        {
                x = intp(comprfactorlog, (std::tanh((x - comprthlog) / comprthlog) + 1.f) * comprthlog, x);
        }


        x = rtengine::max((xlogf(x) / log2 - shadows_range) / dynamic_range, noise);
        assert(x == x);


        if (linbase > 0.f)
        {
            x = xlog2lin(x, linbase);
        }

        if (scale)
        {
            return x * 65535.f;
        } else
        {
            return x;
        }
    };

    const auto sf =
        [=](float s, float c) -> float
        {
            if (c > noise) {

                return 1.f - min(std::abs(s) / c, 1.f);
            } else {
                return 0.f;
            }
        };
//added 2024 02
    const auto apply_sat =
        [&](float &r, float &g, float &b, float f) -> void
        {
            float ll = Color::rgbLuminance(r, g, b, ws);
            float rl = r - ll;
            float gl = g - ll;
            float bl = b - ll;
            float s = intp(max(sf(rl, r), sf(gl, g), sf(bl, b)), pow_F(f, 0.3f) * 0.6f + 0.4f, 1.f);
            r = ll + s * rl;
            g = ll + s * gl;
            b = ll + s * bl;
        };


    float detail = lp.detail;//Log encoding
    if(lp.cieena) {//Cam16
        detail = lp.detailcie;
    }
    const int W = rgb->getWidth(), H = rgb->getHeight();

    if (detail == 0.f) {//no local contrast
#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float r = rgb->r(y, x);
                float g = rgb->g(y, x);
                float b = rgb->b(y, x);
                float m = norm2(r, g, b, ws);

                if (m > noise) {
                    float mm = apply(m);
                    float f = mm / m;
                    f = min(f, 1000000.f);

                    r *= f;
                    b *= f;
                    g *= f;

                    if (satcontrol && f < 1.f) {
                        apply_sat(r, g, b, f);
                    }

                    r = CLIP(r);
                    g = CLIP(g);
                    b = CLIP(b);
                }

                assert(r == r);
                assert(g == g);
                assert(b == b);

                rgb->r(y, x) = r;
                rgb->g(y, x) = g;
                rgb->b(y, x) = b;
            }
        }
    } else  {//local contrast

        array2D<float> Y(W, H);
        {
            constexpr float base_posterization = 20.f;
            array2D<float> Y2(W, H);

#ifdef _OPENMP
            #pragma omp parallel for if (multiThread)
#endif

            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    Y2[y][x] = norm2(rgb->r(y, x), rgb->g(y, x), rgb->b(y, x), ws) / 65535.f;
                    float l = xlogf(rtengine::max(Y2[y][x], 1e-9f));
                    float ll = round(l * base_posterization) / base_posterization;
                    Y[y][x] = xexpf(ll);
                    assert(std::isfinite(Y[y][x]));
                }
            }

            const float radius = rtengine::max(rtengine::max(bfw, W), rtengine::max(bfh, H)) / 30.f;
            const float epsilon = 0.005f;
            rtengine::guidedFilter(Y2, Y, Y, radius, epsilon, multiThread);
        }
        const float blend = detail;

#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float &r = rgb->r(y, x);
                float &g = rgb->g(y, x);
                float &b = rgb->b(y, x);
                float t = Y[y][x];
                float t2;

                if (t > noise && (t2 = norm2(r, g, b, ws)) > noise) {
                    float c = apply(t, false);
                    float f = c / t;
                    //   float t2 = norm(r, g, b);
                    float f2 = apply(t2) / t2;
                    f = intp(blend, f, f2);
                    f = min(f, 1000000.f);

                    //     assert(std::isfinite(f));
                    r *= f;
                    g *= f;
                    b *= f;
                    r = CLIP(r);
                    g = CLIP(g);
                    b = CLIP(b);
                    assert(std::isfinite(r));
                    assert(std::isfinite(g));
                    assert(std::isfinite(b));

                    if (satcontrol && f < 1.f) {
                        apply_sat(r, g, b, f);
                    }


                }
            }
        }

    }
}

// Copyright 2018 Alberto Griggio <alberto.griggio@gmail.com>
void ImProcFunctions::getAutoLogloc(int sp, ImageSource *imgsrc, float *sourceg, float *blackev, float *whiteev, bool *blackredu,  bool *Autogr, float *sourceab,  int *whits,  int *blacks, int *whitslog,  int *blackslog, int fw, int fh, float xsta, float xend, float ysta, float yend, int SCALE)
{
    //BENCHFUN
//adpatation to local adjustments Jacques Desmis 12 2019 and 11 2021 (from ART)
// improvment white aand black toen_eqcam 9 2023
    const PreviewProps pp(0, 0, fw, fh, SCALE);

    Imagefloat img(int(fw / SCALE + 0.5), int(fh / SCALE + 0.5));
    const ProcParams neutral;

    imgsrc->getImage(imgsrc->getWB(), TR_NONE, &img, pp, params->toneCurve, neutral.raw);
    imgsrc->convertColorSpace(&img, params->icm, imgsrc->getWB());
    float minVal = RT_INFINITY;
    float maxVal = -RT_INFINITY;
    TMatrix ws = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);

    constexpr float noise = 1e-5;
    const int h = fh / SCALE;
    const int w = fw / SCALE;

    const int hsta = ysta * h;
    const int hend = yend * h;

    const int wsta = xsta * w;
    const int wend = xend * w;
    int www = int(fw / SCALE + 0.5);
    int hhh = int(fh / SCALE + 0.5);
    array2D<float> YY(www, hhh);
    double mean = 0.0;
    int nc = 0;

    int whit = -whits[sp];
    int blac = -blacks[sp];

    if(params->locallab.spots.at(sp).expcie && params->locallab.spots.at(sp).Autograycie) {
        ImProcFunctions::tone_eqcam2(this, &img, whit, blac, params->icm.workingProfile, SCALE, multiThread);
    }

    int whitlog = -whitslog[sp];
    int blaclog = -blackslog[sp];

    if(params->locallab.spots.at(sp).explog && params->locallab.spots.at(sp).autocompute) {
        ImProcFunctions::tone_eqcam2(this, &img, whitlog, blaclog, params->icm.workingProfile, SCALE, multiThread);
    }

    for (int y = hsta; y < hend; ++y) {
        for (int x = wsta; x < wend; ++x) {
            const float r = img.r(y, x), g = img.g(y, x), b = img.b(y, x);
            YY[y][x] = norm2(r, g, b, ws) / 65535.f;//norm2 to find a best color luminance response in RGB
            mean += static_cast<double>((float) ws[1][0] * Color::gamma_srgb(r) + (float) ws[1][1] * Color::gamma_srgb(g) + (float) ws[1][2] * Color::gamma_srgb(b));
            //alternative to fing gray in case of above process does not works
            nc++;
        }
    }

    for (int y = hsta; y < hend; ++y) {
        for (int x = wsta; x < wend; ++x) {
            float l = YY[y][x];

            if (l > noise) {
                minVal = min(minVal, l);
                maxVal = max(maxVal, l);
            }
        }
    }


    if (!blackredu[sp]){//reduces white point when Freeman algo  or Sigmoid
        maxVal *= 1.5f;
    }
    if (!blackredu[sp]){//reduces blackpoint when Freeman algo  or Sigmoid
        minVal *= 0.5f;
    }

    //E = 2.5*2^EV => e=2.5 depends on the sensor type C=250 e=2.5 to C=330 e=3.3
    //repartition with 2.5 between 1.45 Light and shadows 0.58 => a little more 0.55...
    // https://www.pixelsham.com/2020/12/26/exposure-value-measurements/
    // https://en.wikipedia.org/wiki/Light_meter
    if (maxVal > minVal) {
        const float log2 = std::log(2.f);
        const float dynamic_range = -xlogf(minVal / maxVal) / log2;

        if (settings->verbose) {
            std::cout << "AutoLog: min = " << minVal << ", max = " << maxVal
                      << ", Dynamic Range = " << dynamic_range << std::endl;
        }

        if (Autogr[sp]) {
            double tot = 0.0;
            int n = 0;
            //0.05 0.25 arbitrary values around gray point 0.18 to find a good value as "gray" for "gain"
            const float gmax = rtengine::min(maxVal / 2.f, 0.25f);
            const float gmin = rtengine::max(minVal * std::pow(2.f, rtengine::max((dynamic_range - 1.f) / 2.f, 1.f)), 0.05f);

            if (settings->verbose) {
                std::cout << "         gray boundaries: " << gmin << ", " << gmax << std::endl;
            }

            for (int y = hsta; y < hend; ++y) {
                for (int x = wsta; x < wend; ++x) {
                    const float l = YY[y][x];

                    if (l >= gmin && l <= gmax) {
                        tot += static_cast<double>(l);
                        ++n;
                    }
                }
            }

            if (n > 0) {
                sourceg[sp] = tot / n * 100.0;

                if (settings->verbose) {
                    std::cout << "         computed gray point from " << n << " samples: " << sourceg[sp] << std::endl;
                }
            } else {//I change slightly this part of algo - more progressivity...best response in very low exposure images
                mean /= (nc * 65535.0);
                float yb;
                yb = 1.5f + 100.f * pow_F(mean, 1.8f);//empirical formula for Jz and log encode for low exposure images

                sourceg[sp] = yb;

                if (settings->verbose) {
                    std::cout << "         no samples found in range, resorting to Yb gray point value " << sourceg[sp]  << std::endl;
                }
            }
        }

        constexpr float MIN_WHITE = 2.f;
        constexpr float MAX_BLACK = -3.5f;

        const float gray = sourceg[sp] / 100.f;
        whiteev[sp] = rtengine::max(xlogf(maxVal / gray) / log2, MIN_WHITE);
        blackev[sp] = rtengine::min(whiteev[sp] - dynamic_range, MAX_BLACK);


        //calculate La - Absolute luminance shooting

        const FramesMetaData* metaData = imgsrc->getMetaData();
        float fnum = metaData->getFNumber();          // F number
        float fiso = metaData->getISOSpeed() ;        // ISO
        float fspeed = metaData->getShutterSpeed() ;  // Speed
        double fcomp = metaData->getExpComp();        // Compensation +/-
        double adap;

        if (fnum < 0.3f || fiso < 5.f || fspeed < 0.00001f) { //if no exif data or wrong
            adap = 2000.;
        } else {
            double E_V = fcomp + std::log2(double ((fnum * fnum) / fspeed / (fiso / 100.f)));
            double kexp = 0.;
            E_V += kexp * params->toneCurve.expcomp;// exposure compensation in tonecurve ==> direct EV
            E_V += 0.5 * std::log2(params->raw.expos);  // exposure raw white point ; log2 ==> linear to EV
            adap = pow(2.0, E_V - 3.0);  // cd / m2  ==> 3.0 = log2(8) =>fnum*fnum/speed = Luminance (average scene) * fiso / K (K is the reflected-light meter calibration constant according to the sensors about 12.5 or 14
            // end calculation adaptation scene luminosity
        }

        sourceab[sp] = adap;

    }
}

} // namespace rtengine
