/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2016-2024 Jacques Desmis <jdesmis@gmail.com>
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

// Locallab Tone Mapping Tool - extracted from iplocallab.cc

#include <cmath>

#include "improcfun.h"
#include "imagefloat.h"
#include "labimage.h"
#include "rt_math.h"
#include "sleef.h"
#include "color.h"
#include "guidedfilter.h"
#include "settings.h"
#include "procparams.h"
#include "iplocallab.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace rtengine {

void ImProcFunctions::tone_eqcam(ImProcFunctions *ipf, Imagefloat *rgb, int midtone, const Glib::ustring &workingProfile, double scale, bool multithread)
{
    ToneEqualizerParams params;
    params.enabled = true;
    params.regularization = 0.f;
    params.pivot = 0.f;
    params.bands[0] = 0;
    params.bands[2] = midtone;
    params.bands[4] = 0;
    params.bands[5] = 0;
    int mid = abs(midtone);
    int threshmid = 50;
    if(mid > threshmid) {
        params.bands[1] = sign(midtone) * (mid - threshmid);
        params.bands[3] = sign(midtone) * (mid - threshmid);
    }
    ipf->toneEqualizer(rgb, params, workingProfile, scale, multithread);
}

void ImProcFunctions::tone_eqcam2(ImProcFunctions *ipf, Imagefloat *rgb, int whits, int blacks, const Glib::ustring &workingProfile, double scale, bool multithread)
{
    ToneEqualizerParams params;
    params.enabled = true;
    params.regularization = 0.f;
    params.pivot = 0.f;
    params.bands[0] = blacks;
    int bla = abs(blacks);
    int threshblawhi = 50;
    int threshblawhi2 = 70;
    int threshblawhi3 = 40;
    if(bla > threshblawhi) {
        params.bands[1] = sign(blacks) * (bla - threshblawhi);
    }
    if(bla > threshblawhi2) {
        params.bands[2] = sign(blacks) * (bla - threshblawhi2);
    }

    params.bands[4] = whits;
    int whi = abs(whits);
    if(whi > threshblawhi) {
        params.bands[3] = sign(whits) * (whi - threshblawhi);
    }
    if(whi > threshblawhi3) {
        params.bands[5] = sign(whits) * (whi - threshblawhi3);
    }
    ipf->toneEqualizer(rgb, params, workingProfile, scale, multithread);
}


// tone mapping from
//  https://github.com/thatcherfreeman/utility-dctls/
// Copyright of the original code
/*
MIT License

Copyright (c) 2023 Thatcher Freeman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
// I also took some code from Alberto Grigio
*/
//Copyright (c) 2023 Thatcher Freeman
// Adapted to Rawtherapee Jacques Desmis mars / june 2024  jdesmis@gmail.com

float ImProcFunctions::rolloff_freeman_function(float x, float dr, float b, float c, float kmid)
{
    return (dr * (x / (x + b)) + c) * kmid;//Simple sigmoid (rather a polynomial asymptotic power function) ponderate with kmid - take into account if need Mean Yb scene and Mean Yb viewing and slope value
}
//Copyright (c) 2023 Thatcher Freeman
// Adapted to Rawtherapee Jacques Desmis mars 2024  jdesmis@gmail.com
float ImProcFunctions::scene_referred_contrast(float x, float mid_gray_scene, float gamma)
{
    return mid_gray_scene * std::pow(x / mid_gray_scene, gamma);//apply gamma
}
//Copyright (c) 2023 Thatcher Freeman
// Adapted to Rawtherapee Jacques Desmis mars 2024  jdesmis@gmail.com
float ImProcFunctions::get_freeman_parameters(float x, bool rolloff_, float mid_gray_scene, float gamma, float slopelim, float dr, float b, float c, float kmid)
{
    if (rolloff_ && x <= mid_gray_scene / slopelim) {//general smooth - till Yb scene
        return x;
    } else {
        return rolloff_freeman_function(scene_referred_contrast(x, mid_gray_scene / slopelim, gamma), dr, b, c, kmid);//simulate polynomial power function with a slope to begin
    }
}

//Copyright (c) 2023 Thatcher Freeman
// Adapted to Rawtherapee Jacques Desmis - August 2024

void ImProcFunctions::tonemapFreemanQ(float Q, float &Qout, float target_slope, float white_point, float black_point, float mid_gray_scene, float mid_gray_view, bool rolloff, bool takeyb)
{
    float dr;//Dynamic Range
    float b;
    float c;//black point
    float gamma;
    float mid_gray_scene_;//Mean luminance - Scene conditions
    c = black_point;
    dr = white_point - c;
    float kmid = 1.f;

    if(takeyb){
        kmid = mid_gray_scene / mid_gray_view;
        kmid = cbrt(kmid);
    }
    mid_gray_scene_ = mid_gray_scene;

    b = (dr / (mid_gray_scene_ - c)) * (1.f - ((mid_gray_scene_ - c) / dr)) * mid_gray_scene_;//b - ponderate mid_gray_scene taking into account the total DR, and the dark part below the mid_gray_scene
    gamma = target_slope * (float) std::pow((mid_gray_scene_ + b), 2.0) / (dr * b);//Calculate gamma with slope and mid_gray_scene
    Qout = get_freeman_parameters(Q, rolloff, mid_gray_scene_, gamma, 1.f, dr, b, c, kmid);//call main function
}



// Adapted to Rawtherapee Jacques Desmis 25 mars  - 5 june 2024
void ImProcFunctions::tonemapFreeman(float target_slope, float target_sloper, float target_slopeg , float target_slopeb, float white_point, float black_point, float mid_gray_scene, float mid_gray_view, bool rolloff, float smooththreshold, bool limslope, LUTf& lut, LUTf& lutr, LUTf& lutg, LUTf& lutb, int mode, bool scale, bool takeyb)
{
    float dr;//Dynamic Range
    float b;
    float c;//black point
    float gamma;
    float gammar;
    float gammag;
    float gammab;
    float mid_gray_scene_;//Mean luminance - Scene conditions // mid_gray_view //Mean luminance - Viewing conditions

    c = black_point;
    dr = white_point - c;

    if(scale) {//scale Yb mean luminance scene with white : dr and black
        mid_gray_scene_ = mid_gray_scene * dr + c;
    } else {
        mid_gray_scene_ = mid_gray_scene;
    }

    b = (dr / (mid_gray_scene_ - c)) * (1.f - ((mid_gray_scene_ - c) / dr)) * mid_gray_scene_;//b - ponderate mid_gray_scene taking into account the total DR, and the dark part below the mid_gray_scene
    gamma = target_slope * (float) std::pow((mid_gray_scene_ + b), 2.0) / (dr * b);//Caculate gamma with slope and mid_gray_scene
    gammar = target_sloper * (float) std::pow((mid_gray_scene_ + b), 2.0) / (dr * b);//Caculate gamma with slope and mid_gray_scene
    gammag = target_slopeg * (float) std::pow((mid_gray_scene_ + b), 2.0) / (dr * b);//Caculate gamma with slope and mid_gray_scene
    gammab = target_slopeb * (float) std::pow((mid_gray_scene_ + b), 2.0) / (dr * b);//Caculate gamma with slope and mid_gray_scene
    float kmid = 1.f;//general case
    //float kyb = 1.f;
    if(takeyb){
        kmid = mid_gray_scene / mid_gray_view;
        kmid = cbrt(kmid);
    }
   // if(mode == 3 && target_slope != 1.f ) {//case tone-mapping
/*

        float midutil = mid_gray_view / mid_gray_scene;//take into account ratio between Yb source and Yb viewing
        float midk = 1.f;
        float k_slope = 2.2f;
        if(target_slope >= 1.f) {
            midk = pow_F(midutil, k_slope * (target_slope - 1.f));//ponderation in function target_slope when "slope user" < 1.f
        }
        kmid = midk;

    }
*/
    if (mode == 3 && settings->verbose) {
        printf("b=%f gamma=%f slope=%f DynRange=%f kmid=%f black=%f Yb-scale=%f\n", (double) b, (double) gamma, (double) target_slope, (double) dr, (double) kmid, (double) c, (double) mid_gray_scene_);
    }
    //lut - take from Alberto Griggio
    if(mode == 4) {
        float sloplimr = 1.f;
        float sloplimg = 1.f;
        float sloplimb = 1.f;
        if(limslope) {
            rolloff = true;
        }
        //always apply threshold
        sloplimr *= smooththreshold;
        sloplimg *= smooththreshold;
        sloplimb *= smooththreshold;

        for (int i = 0; i < 65536; ++i) {// i - value image RGB
            lutr[i] = get_freeman_parameters(float(i) / 65535.f, rolloff, mid_gray_scene_, gammar, sloplimr, dr, b, c, kmid);//call main function
            lutg[i] = get_freeman_parameters(float(i) / 65535.f, rolloff, mid_gray_scene_, gammag, sloplimg, dr, b, c, kmid);//call main function
            lutb[i] = get_freeman_parameters(float(i) / 65535.f, rolloff, mid_gray_scene_, gammab, sloplimb, dr, b, c, kmid);//call main function
        }
    } else {
        kmid = 1.f;
        for (int i = 0; i < 65536; ++i) {// i - value image RGB
            lut[i] = get_freeman_parameters(float(i) / 65535.f, rolloff, mid_gray_scene_, gamma, 1.f, dr, b, c, kmid);//call main function
        }
    }
}


void ImProcFunctions::loccont(int bfw, int bfh, LabImage* tmp1, float rad, float stren, int sk)
{
    if (rad > 0.f) {
        array2D<float> guide(bfw, bfh);
        array2D<float> LL(bfw, bfh);
#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16)
#endif

        for (int y = 0; y < bfh ; y++) {
            for (int x = 0; x < bfw; x++) {
                LL[y][x] = tmp1->L[y][x];
                float ll = LL[y][x] / 32768.f;
                guide[y][x] = xlin2log(rtengine::max(ll, 0.f), 10.f);
            }
        }

        array2D<float> iL(bfw, bfh, LL, 0);
        float gu = stren * rad;
        int r = rtengine::max(int(gu / sk), 1);
        const double epsil = 0.001 * std::pow(2.f, -10);
        float st = 0.01f * rad;
        rtengine::guidedFilterLog(guide, 10.f, LL, r, epsil, false);

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16)
#endif

        for (int y = 0; y < bfh ; y++) {
            for (int x = 0; x < bfw; x++) {
                LL[y][x] = intp(st, LL[y][x], iL[y][x]);
                tmp1->L[y][x] = LL[y][x];
            }
        }
    }
}

void ImProcFunctions::tone_eqdehaz(ImProcFunctions *ipf, Imagefloat *rgb, int whits, int blacks, const Glib::ustring &workingProfile, double scale, bool multithread)
{
    ToneEqualizerParams params;
    params.enabled = true;
    params.regularization = 0.f;
    params.pivot = 0.f;
    double blred = 0.4;
    params.bands[0] = blred * blacks;
    int bla = abs(blacks);
    int threshblawhi = 50;
    int threshblawhi2 = 85;
    if(bla > threshblawhi) {
        params.bands[1] = blred * sign(blacks) * (bla - threshblawhi);
    }
    if(bla > threshblawhi2) {
        params.bands[2] = blred * sign(blacks) * (bla - threshblawhi2);
    }

    params.bands[4] = whits;
    int whi = abs(whits);
    if(whi > threshblawhi) {
        params.bands[3] = sign(whits) * (whi - threshblawhi);
    }
    ipf->toneEqualizer(rgb, params, workingProfile, scale, multithread);
}

} // namespace rtengine
