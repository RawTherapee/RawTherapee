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

// Locallab CIECAM Tool - extracted from iplocallab.cc

#include <cmath>

#include "improcfun.h"
#include "colortemp.h"
#include "curves.h"
#include "iccstore.h"
#include "labimage.h"
#include "color.h"
#include "rt_math.h"
#include "sleef.h"
#include "settings.h"
#include "cplx_wavelet_dec.h"
#include "ciecam02.h"
#include "guidedfilter.h"
#include "iccmatrices.h"
#include "simde_helper.h"
#include "iplocallab.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace
{

// Constants used by ciecamloc_02float
constexpr double czlim = rtengine::RT_SQRT1_2;

constexpr double clipazbz(double x)
{
    return rtengine::LIM(x, -0.5, 0.5);
}

constexpr double clipcz(double x)
{
    return rtengine::LIM(x, 0., czlim);
}

constexpr double clipjz05(double x)
{
    return rtengine::LIM(x, 0.0006, 1.0);
}

// Helper function for sigmoid
void sigmoidla(float &valj, float thresj, float lambda)
{
    //thres : shifts the action of sigmoid to darker tones or lights
    //lambda : changes the "slope" of the sigmoid. Low values give a flat curve, high values a "rectangular / orthogonal" curve
    valj =  1.f / (1.f + xexpf(lambda - (lambda / thresj) * valj));
}

// Gamut mapping for Jz az bz
void gamutjz(double &Jz, double &az, double &bz, double pl, const double wip[3][3], const float higherCoef, const float lowerCoef)
{
    //Not used...bad results
    constexpr float ClipLevel = 65535.0f;
    bool inGamut;

    //  int nb = 0;
    do {
        inGamut = true;
        double L_, M_, S_;
        double xx, yy, zz;
        bool zcam = false;
        rtengine::Ciecam02::jzczhzxyz(xx, yy, zz, Jz, az, bz, pl, L_, M_, S_, zcam);
        double x, y, z;
        x = 65535. * (d65_d50[0][0] * xx + d65_d50[0][1] * yy + d65_d50[0][2] * zz);
        y = 65535. * (d65_d50[1][0] * xx + d65_d50[1][1] * yy + d65_d50[1][2] * zz);
        z = 65535. * (d65_d50[2][0] * xx + d65_d50[2][1] * yy + d65_d50[2][2] * zz);
        float R, G, B;
        rtengine::Color:: xyz2rgb(x, y, z, R, G, B, wip);

        if (rtengine::min(R, G, B) < 0.f  || rtengine::max(R, G, B) > ClipLevel) {
            //    nb++;
            double hz = xatan2f(bz, az);
            float2 sincosval = xsincosf(hz);
            double Cz = sqrt(az * az + bz * bz);
            Cz *= (double) higherCoef;

            if (Cz < 0.01 && Jz > 0.05) { //empirical values
                Jz -= (double) lowerCoef;
            }

            az = clipazbz(Cz * (double) sincosval.y);
            bz = clipazbz(Cz * (double) sincosval.x);

            inGamut = false;
        }
    } while (!inGamut);
}

// Copyright 2018 Alberto Griggio <alberto.griggio@gmail.com>
float find_gray(float source_gray, float target_gray)
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

} // namespace

namespace rtengine {

void ImProcFunctions::ciecamloc_02float(struct local_params& lp, int sp, LabImage* lab, int bfw, int bfh, int call, int sk, const LUTf& cielocalcurve, bool localcieutili, const LUTf& cielocalcurve2, bool localcieutili2,
                                        const LUTf& jzlocalcurve, bool localjzutili, const LUTf& czlocalcurve, bool localczutili, const LUTf& czjzlocalcurve, bool localczjzutili, const LocCHCurve& locchCurvejz, const LocHHCurve& lochhCurvejz, const LocLHCurve& loclhCurvejz, bool HHcurvejz, bool CHcurvejz, bool LHcurvejz,
                                        const LocwavCurve& locwavCurvejz, bool locwavutilijz, float &maxicam, float &contsig, float &lightsig
                                       )
{
//    BENCHFUN
    if (!params->locallab.spots.at(sp).activ) { //disable all ciecam functions
        return;
    }

    bool ciec = false;
    bool iscie = false;


    int modeqj = 1;
    if (params->locallab.spots.at(sp).modeQJ == "511") {
        modeqj = 0;
    } else if (params->locallab.spots.at(sp).modeQJ == "512") {
        modeqj = 1;
    }

    if (params->locallab.spots.at(sp).ciecam && params->locallab.spots.at(sp).explog && call == 1) {
        ciec = true;
        iscie = false;
    } else if (params->locallab.spots.at(sp).expcie && call == 0) {
        ciec = true;
        iscie = true;
    }

    bool z_cam = false; //params->locallab.spots.at(sp).jabcie; //alaways use normal algorithm, Zcam giev often bad results
    bool jabcie = false;//always disabled
    bool issigjz12 = params->locallab.spots.at(sp).sigjz12;
    bool issigq12 = params->locallab.spots.at(sp).sigq12;

    bool islogjz = params->locallab.spots.at(sp).forcebw;
    bool issigjz = params->locallab.spots.at(sp).sigjz;
    bool issigq = params->locallab.spots.at(sp).sigq;

    bool issig = true; //params->locallab.spots.at(sp).sigcie;

    //sigmoid J Q variables
   // const float sigmoidlambda = params->locallab.spots.at(sp).sigmoidldacie12;
   // const float sigmoidth = params->locallab.spots.at(sp).sigmoidthcie;
   // const float sigmoidbl = params->locallab.spots.at(sp).sigmoidblcie12;
    const bool sigmoidnorm = params->locallab.spots.at(sp).normcie;

    const float sigmoidlambda = params->locallab.spots.at(sp).sigmoidldacie;
    const float sigmoidth = params->locallab.spots.at(sp).sigmoidthcie;
    const float sigmoidbl = params->locallab.spots.at(sp).sigmoidblcie;


    int mobwev12 = 0;
    if (params->locallab.spots.at(sp).bwevMethod12 == "sigQ") {
        mobwev12 = 0;
    } else if (params->locallab.spots.at(sp).bwevMethod12 == "slop") {
        mobwev12 = 1;
    }

    int mobwev = 0;
    float sumcamq01 = 0.5f;

    if (params->locallab.spots.at(sp).bwevMethod == "none") {
        mobwev = 0;
    } else if (params->locallab.spots.at(sp).bwevMethod == "sig") {
        mobwev = 1;
    } else if (params->locallab.spots.at(sp).bwevMethod == "logsig") {
        mobwev = 2;
    }

    float senssig =(float) params->locallab.spots.at(sp).sigmoidsenscie;

    float middle_grey_contrast = params->locallab.spots.at(sp).sigmoidldacie12;
    float contrast_skewness = params->locallab.spots.at(sp).sigmoidthcie12;
    float white_point_disp = params->locallab.spots.at(sp).sigmoidblcie12;
    float middle_grey = 0.01 * params->locallab.spots.at(sp).sourceGraycie;
    middle_grey *= 2.f;//take into account Ciecam
    middle_grey = std::min(middle_grey, 0.6f);

    float black_point =  xexpf(lp.blackevjz * std::log(2.f) + xlogf(middle_grey));
    float white_pointsig = xexpf(lp.whiteevjz * std::log(2.f) + xlogf(middle_grey));//to adapt if need and remove slider whitsig

    float slopsmootq =(float) params->locallab.spots.at(sp).slopesmoq;
    float mid_gray_view = 0.01f * lp.targetgraycie;
    TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(params->icm.workingProfile);
    const double wip[3][3] = {//improve precision with double
        {wiprof[0][0], wiprof[0][1], wiprof[0][2]},
        {wiprof[1][0], wiprof[1][1], wiprof[1][2]},
        {wiprof[2][0], wiprof[2][1], wiprof[2][2]}
    };
    float plum = (float) params->locallab.spots.at(sp).pqremapcam16;

    int mocam = 1;

    if(lp.moka == 1) {
        mocam = 1;
    } else if (lp.moka == 2) {
        mocam = 2;
    }

    int mecamcurve = 0;

    if (params->locallab.spots.at(sp).toneMethodcie == "one") {
        mecamcurve = 0;
    } else if (params->locallab.spots.at(sp).toneMethodcie == "two") {
        mecamcurve = 1;
    }

    int mecamcurve2 = 0;

    if (params->locallab.spots.at(sp).toneMethodcie2 == "onec") {
        mecamcurve2 = 0;
    } else if (params->locallab.spots.at(sp).toneMethodcie2 == "twoc") {
        mecamcurve2 = 1;
    } else if (params->locallab.spots.at(sp).toneMethodcie2 == "thrc") {
        mecamcurve2 = 2;
    }

    float th = 1.f;
//    const float at = 1.f - sigmoidth;
//    const float bt = sigmoidth;

   // const float ath = sigmoidth - 1.f;
   // const float bth = 1;
    float sila = pow_F(sigmoidlambda, senssig);
    sila = LIM01(sila);
    const float sigm = 3.3f + 7.1f * (1.f - sila); //e^10.4 = 32860 => sigm vary from 3.3 to 10.4
    float bl = std::min(sigmoidbl, 1.f);//reused old slider
    if(params->locallab.spots.at(sp).logcieq) {
        bl = 0.01f * (float) params->locallab.spots.at(sp).strcielog;
        bl = std::min(bl, 1.f);
    }

    //end sigmoid

    int width = lab->W, height = lab->H;
    float Yw;
    Yw = 1.0f;
    double Xw, Zw;
    float f = 0.f, nc = 0.f, la, c = 0.f, xw, yw, zw, f2 = 1.f, c2 = 1.f, nc2 = 1.f, yb2;
    float fl, n, nbb, ncb, aw; //d
    float xwd, ywd, zwd, xws, yws, zws;
    //  int alg = 0;
    double Xwout, Zwout;
    double Xwsc, Zwsc;

    LUTu hist16J(32768, LUT_CLIP_BELOW | LUT_CLIP_ABOVE, true);
    LUTu hist16Q(32768, LUT_CLIP_BELOW | LUT_CLIP_ABOVE, true);
    //for J light and contrast
    LUTf CAMBrightCurveJ(32768, LUT_CLIP_BELOW | LUT_CLIP_ABOVE);
    LUTf CAMBrightCurveQ(32768, LUT_CLIP_BELOW | LUT_CLIP_ABOVE);
    LUTf CAMBrightCurveQsig(32768, LUT_CLIP_BELOW | LUT_CLIP_ABOVE);

#ifdef _OPENMP
    const int numThreads = min(max(width * height / 65536, 1), omp_get_max_threads());
    #pragma omp parallel num_threads(numThreads) if(numThreads>1)
#endif
    {
        LUTu hist16Jthr(hist16J.getSize(), LUT_CLIP_BELOW | LUT_CLIP_ABOVE, true);
        LUTu hist16Qthr(hist16Q.getSize(), LUT_CLIP_BELOW | LUT_CLIP_ABOVE, true);

#ifdef _OPENMP
        #pragma omp for
#endif

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) { //rough correspondence between L and J
                float currL = lab->L[i][j] / 327.68f;
                float koef; //rough correspondence between L and J

                if (currL > 50.f) {
                    if (currL > 70.f) {
                        if (currL > 80.f) {
                            if (currL > 85.f) {
                                koef = 0.97f;
                            } else {
                                koef = 0.93f;
                            }
                        } else {
                            koef = 0.87f;
                        }
                    } else {
                        if (currL > 60.f) {
                            koef = 0.85f;
                        } else {
                            koef = 0.8f;
                        }
                    }
                } else {
                    if (currL > 10.f) {
                        if (currL > 20.f) {
                            if (currL > 40.f) {
                                koef = 0.75f;
                            } else {
                                koef = 0.7f;
                            }
                        } else {
                            koef = 0.9f;
                        }
                    } else {
                        koef = 1.0;
                    }
                }

                hist16Jthr[(int)((koef * lab->L[i][j]))]++;    //evaluate histogram luminance L # J
                hist16Qthr[CLIP((int)(32768.f * sqrt((koef * (lab->L[i][j])) / 32768.f)))]++;     //for brightness Q : approximation for Q=wh*sqrt(J/100)  J not equal L
            }
        }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            hist16J += hist16Jthr;
            hist16Q += hist16Qthr;
        }
    }
#ifdef _OPENMP
    static_cast<void>(numThreads); // to silence cppcheck warning
#endif

    //evaluate lightness, contrast

    if (ciec) {
        float contL = 0.f;
        float lightL = 0.f;
        float contQ = 0.f;
        float lightQ = 0.f;

        if (iscie) {
            contL = 0.6 * params->locallab.spots.at(sp).contlcie; //0.6 less effect, no need 1.
            lightL = 0.4 * params->locallab.spots.at(sp).lightlcie; //0.4 less effect, no need 1.
            contQ = 0.5 * params->locallab.spots.at(sp).contqcie; //0.5 less effect, no need 1.
            lightQ = 0.4 * params->locallab.spots.at(sp).lightqcie; //0.4 less effect, no need 1.
        } else {
            contL = 0.6 * params->locallab.spots.at(sp).contl; //0.6 less effect, no need 1.
            lightL = 0.4 * params->locallab.spots.at(sp).lightl; //0.4 less effect, no need 1.
            contQ = 0.5 * params->locallab.spots.at(sp).contq; //0.5 less effect, no need 1.
            lightQ = 0.4 * params->locallab.spots.at(sp).lightq; //0.4 less effect, no need 1.

        }

        float contthresL = 0.f;

        if (iscie) {
            contthresL = params->locallab.spots.at(sp).contthrescie;
        } else {
            contthresL = params->locallab.spots.at(sp).contthres;
        }

        float contthresQ = contthresL;

        if (contL < 0.f) {
            contthresL *= -1;
        }

        float thL = 0.6f;
        thL = 0.3f * contthresL + 0.6f;

        if (contQ < 0.f) {
            contthresQ *= -1;
        }

        float thQ = 0.6f;
        thQ = 0.3f * contthresQ + 0.6f;
        float thQsig = 0.6f;
        Ciecam02::curveJfloat(lightL, contL, thL, hist16J, CAMBrightCurveJ); //lightness J and contrast J
        CAMBrightCurveJ /= 327.68f;
        double podcont = 40.;//50
        double podcont0 = 40.;//50
        double podcont1 = 30.;//35.
        double ka = -(podcont0 - podcont1) / 0.5;
        double kb = podcont1 - ka;

        double podlight = 35.;
        double podlight0 = 35.;
        double podlight1 = 40.;//45
        double kal = -(podlight0 - podlight1) / 0.5;
        double kbl = podlight1 - kal;

        double contbase = params->locallab.spots.at(sp).sigmoidldacie;

        if(contbase <= 0.5)  {
            podcont = podcont0;
            podlight = podlight0;

        } else {
            podcont = ka * contbase + kb;
            podlight = kal * contbase + kbl;
        }
        Ciecam02::curveJfloat(lightQ, contQ, thQ, hist16Q, CAMBrightCurveQ); //brightness Q and contrast Q
        lightsig = -podlight * contbase;
        contsig = podcont * contbase;
        Ciecam02::curveJfloat(lightsig, contsig, thQsig, hist16Q, CAMBrightCurveQsig); //brightness Q and contrast Q bypass.
    }


    int tempo = 5000;

    if (params->locallab.spots.at(sp).expvibrance && call == 2) {
        if (params->locallab.spots.at(sp).warm > 0) {
            tempo = 5000 - 30 * params->locallab.spots.at(sp).warm;
        } else if (params->locallab.spots.at(sp).warm < 0) {
            tempo = 5000 - 70 * params->locallab.spots.at(sp).warm;
        }
    }


    if (ciec) {
        if (iscie) {
            if (params->locallab.spots.at(sp).catadcie > 0) {
                tempo = 5000 - 30 * params->locallab.spots.at(sp).catadcie;
            } else if (params->locallab.spots.at(sp).catadcie < 0) {
                tempo = 5000 - 70 * params->locallab.spots.at(sp).catadcie;
            }
        } else {
            if (params->locallab.spots.at(sp).catad > 0) {
                tempo = 5000 - 30 * params->locallab.spots.at(sp).catad;
            } else if (params->locallab.spots.at(sp).catad < 0) {
                tempo = 5000 - 70 * params->locallab.spots.at(sp).catad;
            }
        }
    }

    ColorTemp::temp2mulxyz(params->wb.temperature, params->wb.method, params->wb.observer, Xw, Zw);  //compute white Xw Yw Zw  : white current WB
    ColorTemp::temp2mulxyz(tempo, "Custom", params->wb.observer, Xwout, Zwout);
    ColorTemp::temp2mulxyz(5000, "Custom", params->wb.observer, Xwsc, Zwsc);

    //viewing condition for surrsrc
    f  = 1.00f;
    c  = 0.69f;
    nc = 1.00f;
    //viewing condition for surround
    f2 = 1.0f, c2 = 0.69f, nc2 = 1.0f;

    if (ciec) {
        if (iscie) {
            //surround source with only 2 choices (because Log encoding before)
            if(lp.sursouci == 0) {
                f = 1.0f, c = 0.69f, nc = 1.0f;
            } else if (lp.sursouci == 1){
                f  = 0.9f;
                c  = 0.59f;
                nc = 0.9f;
            } else if (lp.sursouci == 2) {
                f  = 0.8f;
                c  = 0.525f;
                nc = 0.8f;
            } else if (lp.sursouci == 3) {
                f  = 0.8f;
                c  = 0.41f;
                nc = 0.8f;
            } else if (lp.sursouci == 4) {
                f = 1.0f, c = 0.702f, nc = 1.0f;//very small surround effect for Jz - Also disable Ciecam further
            }
        } else {
            if (params->locallab.spots.at(sp).sursour == "Average") {
                f = 1.0f, c = 0.69f, nc = 1.0f;
            } else if (params->locallab.spots.at(sp).sursour == "Dim") {
                f  = 0.9f;
                c  = 0.59f;
                nc = 0.9f;
            } else if (params->locallab.spots.at(sp).sursour == "Dark") {
                f  = 0.8f;
                c  = 0.525f;
                nc = 0.8f;
            } else if (params->locallab.spots.at(sp).sursour == "exDark") {
                f  = 0.8f;
                c  = 0.41f;
                nc = 0.8f;
            }
        }

        //viewing condition for surround
        if (iscie) {
            if (params->locallab.spots.at(sp).surroundcie == "Average") {
                f2 = 1.0f, c2 = 0.69f, nc2 = 1.0f;
            } else if (params->locallab.spots.at(sp).surroundcie == "Dim") {
                f2  = 0.9f;
                c2  = 0.59f;
                nc2 = 0.9f;
            } else if (params->locallab.spots.at(sp).surroundcie == "Dark") {
                f2  = 0.8f;
                c2  = 0.525f;
                nc2 = 0.8f;
            } else if (params->locallab.spots.at(sp).surroundcie == "ExtremelyDark") {
                f2  = 0.8f;
                c2  = 0.41f;
                nc2 = 0.8f;
            }
        } else {
            if (params->locallab.spots.at(sp).surround == "Average") {
                f2 = 1.0f, c2 = 0.69f, nc2 = 1.0f;
            } else if (params->locallab.spots.at(sp).surround == "Dim") {
                f2  = 0.9f;
                c2  = 0.59f;
                nc2 = 0.9f;
            } else if (params->locallab.spots.at(sp).surround == "Dark") {
                f2  = 0.8f;
                c2  = 0.525f;
                nc2 = 0.8f;
            } else if (params->locallab.spots.at(sp).surround == "ExtremelyDark") {
                f2  = 0.8f;
                c2  = 0.41f;
                nc2 = 0.8f;
            }

        }
    }

    xwd = 100.0 * Xwout;
    zwd = 100.0 * Zwout;
    ywd = 100.f;

    xws = 100.0 * Xwsc;
    zws = 100.0 * Zwsc;
    yws = 100.f;


    //La and la2 = ambiant luminosity scene and viewing
    la = 400.f;
    float la2 = 400.f;

    if (ciec) {
        if (iscie) {
            la = params->locallab.spots.at(sp).sourceabscie;
            la2 = params->locallab.spots.at(sp).targabscie;
        } else {
            la = params->locallab.spots.at(sp).sourceabs;
            la2 = params->locallab.spots.at(sp).targabs;
        }
    }

    const float pilot = 2.f;
    const float pilotout = 2.f;
    double avgm = 0.;
    //algoritm's params
    float yb = 18.f;
    yb2 = 18;

    if (ciec) {
        if (iscie) {
            yb = params->locallab.spots.at(sp).sourceGraycie;//
            avgm = (double) pow_F(0.01f * (yb - 1.f), 0.45f);;
            yb2 = params->locallab.spots.at(sp).targetGraycie;
        } else {
            yb = params->locallab.spots.at(sp).targetGray;//target because we are after Log encoding
            yb2 = params->locallab.spots.at(sp).targetGray;
        }
    }

    if (params->locallab.spots.at(sp).expcie && call == 10 && params->locallab.spots.at(sp).modecam == "jz") {
        yb = params->locallab.spots.at(sp).sourceGraycie;//for Jz calculate Yb and surround in Lab and cam16 before process Jz
        la = params->locallab.spots.at(sp).sourceabscie;
            if(lp.sursouci == 0) {
                f = 1.0f, c = 0.69f, nc = 1.0f;
            } else if (lp.sursouci == 1){
                f  = 0.9f;
                c  = 0.59f;
                nc = 0.9f;
            } else if (lp.sursouci == 2) {
                f  = 0.8f;
                c  = 0.525f;
                nc = 0.8f;
            } else if (lp.sursouci == 3) {
                f  = 0.8f;
                c  = 0.41f;
                nc = 0.8f;
            } else if (lp.sursouci == 4) {
                f = 1.0f, c = 0.702f, nc = 1.0f;//very small surround effect for Jz
            }

    }

    float schr = 0.f;
    float mchr = 0.f;
    float cchr = 0.f;
    float rstprotection = 0.f;
    float hue = 0.f;

    if (ciec) {
        if (iscie) {
            rstprotection =  params->locallab.spots.at(sp).rstprotectcie;
            hue = params->locallab.spots.at(sp).huecie;

            cchr = params->locallab.spots.at(sp).chromlcie;

            if (cchr == -100.0f) {
                cchr = -99.8f;
            }

            schr = params->locallab.spots.at(sp).saturlcie;

            if (schr > 0.f) {
                schr = schr / 2.f;    //divide sensibility for saturation
            }

            if (schr == -100.f) {
                schr = -99.8f;
            }

            mchr = params->locallab.spots.at(sp).colorflcie;

            if (mchr == -100.0f) {
                mchr = -99.8f ;
            }

            if (mchr == 100.0f) {
                mchr = 99.9f;
            }

        } else {
            cchr = params->locallab.spots.at(sp).chroml;

            if (cchr == -100.0f) {
                cchr = -99.8f;
            }

            schr = params->locallab.spots.at(sp).saturl;

            if (schr > 0.f) {
                schr = schr / 2.f;    //divide sensibility for saturation
            }

            if (schr == -100.f) {
                schr = -99.8f;
            }

            mchr = params->locallab.spots.at(sp).colorfl;

            if (mchr == -100.0f) {
                mchr = -99.8f ;
            }

            if (mchr == 100.0f) {
                mchr = 99.9f;
            }
        }
    }

    float d, dj;

    // const int gamu = 0; //(params->colorappearance.gamut) ? 1 : 0;
    xw = 100.0 * Xw;
    yw = 100.f * Yw;
    zw = 100.0 * Zw;
    float xw1 = xws, yw1 = yws, zw1 = zws, xw2 = xwd, yw2 = ywd, zw2 = zwd;
    float cz, wh, pfl;
    int c16 = 16;//always cat16
    bool c20 = true;

    if (c20  && plum > 100.f) {
        c16 = 21;//I define 21...for 2021 :)
    }

    int level_bljz = params->locallab.spots.at(sp).csthresholdjz.getBottomLeft();
    int level_hljz = params->locallab.spots.at(sp).csthresholdjz.getTopLeft();
    int level_brjz = params->locallab.spots.at(sp).csthresholdjz.getBottomRight();
    int level_hrjz = params->locallab.spots.at(sp).csthresholdjz.getTopRight();

    float alowjz = 1.f;
    float blowjz = 0.f;

    if (level_hljz != level_bljz) {
        alowjz = 1.f / (level_hljz - level_bljz);
        blowjz = -alowjz * level_bljz;
    }

    float ahighjz = 1.f;
    float bhighjz = 0.f;

    if (level_hrjz != level_brjz) {
        ahighjz = 1.f / (level_hrjz - level_brjz);
        bhighjz =  -ahighjz * level_brjz;
    }

    float sigmalcjz = params->locallab.spots.at(sp).sigmalcjz;
    float jzamountchr = 0.01 * params->locallab.spots.at(sp).thrhjzcie;
    bool jzch = params->locallab.spots.at(sp).chjzcie;
    double jzamountchroma = 0.01 * settings->amchromajz;

    if (jzamountchroma < 0.05) {
        jzamountchroma = 0.05;
    }

    if (jzamountchroma > 2.) {
        jzamountchroma = 2.;
    }

    Ciecam02::initcam1float(yb, pilot, f, la, xw, yw, zw, n, d, nbb, ncb, cz, aw, wh, pfl, fl, c, c16, plum);
    const float pow1 = pow_F(1.64f - pow_F(0.29f, n), 0.73f);
    float nj, nbbj, ncbj, czj, awj, flj;
    Ciecam02::initcam2float(yb2, pilotout, f2,  la2,  xw2,  yw2,  zw2, nj, dj, nbbj, ncbj, czj, awj, flj, c16, plum);
#if defined(__SSE2__) || defined(RT_SIMDE)
    const float reccmcz = 1.f / (c2 * czj);
#endif
    const float epsil = 0.0001f;
    const float coefQ = 32767.f / wh;
    const float coefq = 1.f / wh;

    const float pow1n = pow_F(1.64f - pow_F(0.29f, nj), 0.73f);
    const float coe = pow_F(fl, 0.25f);
    const float QproFactor = (0.4f / c) * (aw + 4.0f) ;
    const double shadows_range =  params->locallab.spots.at(sp).blackEvjz;
    const double targetgray = params->locallab.spots.at(sp).targetjz;
    double targetgraycor = 0.15;
    double dynamic_range = std::max(params->locallab.spots.at(sp).whiteEvjz - shadows_range, 0.5);
    const double noise = pow(2., -16.6);//16.6 instead of 16 a little less than others, but we work in double
    const double log2 = xlog(2.);
    const float log2f = xlogf(2.f);

    float middle_grey_contrastjz = params->locallab.spots.at(sp).sigmoidldajzcie12;
    float contrast_skewnessjz = params->locallab.spots.at(sp).sigmoidthjzcie12;
    float white_point_dispjz = params->locallab.spots.at(sp).sigmoidbljzcie12;
    float middle_greyjz = 0.01 * params->locallab.spots.at(sp).sourceGraycie;
    middle_greyjz *= 2.f;
    middle_greyjz = std::min(middle_greyjz, 0.6f);

    float black_pointjz =  xexpf(lp.blackevjz * std::log(2.f) + xlogf(middle_greyjz));
    float white_pointsigjz = xexpf(lp.whiteevjz * std::log(2.f) + xlogf(middle_greyjz));//to adapt if need and remove slider whitsig
    float drjz = white_pointsigjz - black_pointjz;
    if(params->locallab.spots.at(sp).sigybjz12) {
        middle_greyjz = middle_greyjz * drjz + black_pointjz;
    }

    if ((mocam == 2)  && call == 0) { //Jz az bz ==> Jz Cz Hz before Ciecam16
        double mini = 1000.;
        double maxi = -1000.;
        double sum = 0.;
        int nc = 0;
        double epsiljz = 0.0001;
        //Remapping see https://hal.inria.fr/hal-02131890/document    I took some ideas in this text, and add my personal adaptation
        // image quality assessment of HDR and WCG images https://tel.archives-ouvertes.fr/tel-02378332/document
        double adapjz = params->locallab.spots.at(sp).adapjzcie;
        double jz100 = params->locallab.spots.at(sp).jz100;
        double pl = params->locallab.spots.at(sp).pqremap;
        double jzw, azw, bzw;
        jzw = 0.18;//Jz white

        bool Qtoj = params->locallab.spots.at(sp).qtoj;//betwwen lightness to brightness
        const bool logjz =  params->locallab.spots.at(sp).logjz;//log encoding
//calculate min, max, mean for Jz
#ifdef _OPENMP
        #pragma omp parallel for reduction(min:mini) reduction(max:maxi) reduction(+:sum) if(multiThread)
#endif

        for (int i = 0; i < height; i += 1) {
            for (int k = 0; k < width; k += 1) {
                float L = lab->L[i][k];
                float a = lab->a[i][k];
                float b = lab->b[i][k];
                float x, y, z;
                //convert Lab => XYZ
                Color::Lab2XYZ(L, a, b, x, y, z);
                x = x / 65535.f;
                y = y / 65535.f;
                z = z / 65535.f;
                double Jz, az, bz;
                double xx, yy, zz;
                //D50 ==> D65
                xx = (d50_d65[0][0] * (double) x + d50_d65[0][1] * (double) y + d50_d65[0][2] * (double) z);
                yy = (d50_d65[1][0] * (double) x + d50_d65[1][1] * (double) y + d50_d65[1][2] * (double) z);
                zz = (d50_d65[2][0] * (double) x + d50_d65[2][1] * (double) y + d50_d65[2][2] * (double) z);

                double L_p, M_p, S_p;
                bool zcam = z_cam;

                Ciecam02::xyz2jzczhz(Jz, az, bz, xx, yy, zz, pl, L_p, M_p, S_p, zcam);

                if (Jz > maxi) {
                    maxi = Jz;
                }

                if (Jz < mini) {
                    mini = Jz;
                }

                sum += Jz;
                // I read bz, az values and Hz ==> with low chroma values Hz are very different from lab always around 1.4 radians ???? for blue...
            }
        }

        nc = height * width;
        sum = sum / nc;
        maxi += epsiljz;
        sum += epsiljz;
        //remapping Jz
        double ijz100 = 1. / jz100;
        double ajz = (ijz100 - 1.) / 9.; //9 = sqrt(100) - 1 with a parabolic curve after jz100 - we can change for others curve ..log...(you must change also in locallabtool2)
        double bjz = 1. - ajz;
        //relation between adapjz and Absolute luminance source (La), adapjz =sqrt(La) - see locallabtool2 adapjzcie
        double interm = jz100 * (adapjz * ajz + bjz);
        double bj = (10. - maxi) / 9.;
        double aj = maxi - bj;
        double to_screen = (aj * interm + bj) / maxi;
        //to screen - remapping of Jz in function real scene absolute luminance

        double to_one = 1.;//only for calculation in range 0..1 or 0..32768
        to_one = 1 / (maxi * to_screen);

        if (adapjz == 10.) { //force original algorithm if La > 10000
            to_screen = 1.;
        }

        if (Qtoj) {
            double xxw = (d50_d65[0][0] * (double) Xw + d50_d65[0][1] * (double) Yw + d50_d65[0][2] * (double) Zw);
            double yyw = (d50_d65[1][0] * (double) Xw + d50_d65[1][1] * (double) Yw + d50_d65[1][2] * (double) Zw);
            double zzw = (d50_d65[2][0] * (double) Xw + d50_d65[2][1] * (double) Yw + d50_d65[2][2] * (double) Zw);
            double L_pa, M_pa, S_pa;
            Ciecam02::xyz2jzczhz(jzw, azw, bzw, xxw, yyw, zzw, pl, L_pa, M_pa, S_pa, z_cam);

            if (settings->verbose) { //calculate Jz white for use of lightness instead brightness
                printf("Jzwhite=%f \n", jzw);
            }

        }

        const std::unique_ptr<LabImage> temp(new LabImage(width, height));
        const std::unique_ptr<LabImage> tempresid(new LabImage(width, height));
        const std::unique_ptr<LabImage> tempres(new LabImage(width, height));
        array2D<double> JJz(width, height);
        array2D<double> Aaz(width, height);
        array2D<double> Bbz(width, height);
        int highhs =  params->locallab.spots.at(sp).hljzcie;
        int hltonahs = params->locallab.spots.at(sp).hlthjzcie;
        int shadhs = params->locallab.spots.at(sp).shjzcie;
        int shtonals = params->locallab.spots.at(sp).shthjzcie;
        int radhs = params->locallab.spots.at(sp).radjzcie;
        float softjz = (float) params->locallab.spots.at(sp).softjzcie;

        avgm = 0.5 * (sum * to_screen * to_one + avgm);//empirical formula
        double miny = 0.1;
        double delta = 0.015 * (double) sqrt(std::max(100.f, la) / 100.f);//small adaptation in function La scene
        double maxy = 0.65;//empirical value
        double maxreal = maxi * to_screen;
        double maxjzw = jzw * to_screen;

        if (settings->verbose) {
            printf("La=%4.1f PU_adap=%2.1f maxi=%f mini=%f mean=%f, avgm=%f to_screen=%f Max_real=%f to_one=%f\n", (double) la, adapjz, maxi, mini, sum, avgm, to_screen, maxreal, to_one);
        }
        const float sigmoidlambdajz = params->locallab.spots.at(sp).sigmoidldajzcie;
        const float sigmoidthjz = params->locallab.spots.at(sp).sigmoidthjzcie;
        const float sigmoidbljz = params->locallab.spots.at(sp).sigmoidbljzcie;

        float thjz = 1.f;
        const float atjz = 1.f - sigmoidthjz;
        const float btjz = sigmoidthjz;

        const float athjz = sigmoidthjz - 1.f;
        const float bthjz = 1.f;
        float powsig = pow_F(sigmoidlambdajz, 0.5f);
        const float sigmjz = 3.3f + 7.1f * (1.f - powsig); // e^10.4 = 32860
        const float bljz = sigmoidbljz;



        double contreal = 0.2 *  params->locallab.spots.at(sp).contjzcie;
        DiagonalCurve jz_contrast({
            DCT_NURBS,
            0, 0,
            avgm - avgm * (0.6 - contreal / 250.0), avgm - avgm * (0.6 + contreal / 250.0),
            avgm + (1. - avgm) * (0.6 - contreal / 250.0), avgm + (1. - avgm) * (0.6 + contreal / 250.0),
            1, 1
        });
        //all calculations in double for best results...but slow
        double lightreal = 0.2 *  params->locallab.spots.at(sp).lightjzcie;
        double chromz =  params->locallab.spots.at(sp).chromjzcie;
        double saturz =  params->locallab.spots.at(sp).saturjzcie;
        double dhue = 0.0174 * params->locallab.spots.at(sp).huejzcie;
        DiagonalCurve jz_light({
            DCT_NURBS,
            0, 0,
            miny, miny + lightreal / 150.,
            maxy, min(1.0, maxy + delta + lightreal / 300.0),
            1, 1
        });
        DiagonalCurve jz_lightn({
            DCT_NURBS,
            0, 0,
            max(0.0, miny  - lightreal / 150.), miny,
            maxy + delta - lightreal / 300.0, maxy + delta,
            1, 1
        });
        bool wavcurvejz = false;

        if (locwavCurvejz && locwavutilijz) {
            for (int i = 0; i < 500; i++) {
                if (locwavCurvejz[i] != 0.5f) {
                    wavcurvejz = true;
                    break;
                }
            }
        }

        float mjjz = lp.mLjz;

        if (wavcurvejz && lp.mLjz == 0.f) {
            mjjz = 0.0f;//to enable clarity if need in some cases mjjz = 0.0001f
        }

        //log encoding Jz
        double gray = 0.15;
        /*
        const double shadows_range =  params->locallab.spots.at(sp).blackEvjz;
        const double targetgray = params->locallab.spots.at(sp).targetjz;
        double targetgraycor = 0.15;
        double dynamic_range = std::max(params->locallab.spots.at(sp).whiteEvjz - shadows_range, 0.5);
        const double noise = pow(2., -16.6);//16.6 instead of 16 a little less than others, but we work in double
        const double log2 = xlog(2.);
        */
        double base = 10.;
        double linbase = 10.;

        if (logjz) { //with brightness Jz
            gray = 0.01 * params->locallab.spots.at(sp).sourceGraycie;//acts as amplifier (gain) : needs same type of modifications than targetgraycor with pow
            gray = pow(gray, 1.2);//or 1.15 => modification to increase sensitivity gain, only on defaults, of course we can change this value manually...take into account suuround and Yb Cam16
            targetgraycor = pow(0.01 * targetgray, 1.15);//or 1.2 small reduce effect -> take into account a part of surround (before it was at 1.2)
            base = targetgray > 1. && targetgray < 100. && dynamic_range > 0. ? (double) find_gray(std::abs((float) shadows_range) / (float) dynamic_range, (float)(targetgraycor)) : 0.;
            linbase = std::max(base, 2.);//2. minimal base log to avoid very bad results

            if (settings->verbose) {
                printf("Base logarithm encoding Jz=%5.1f\n", linbase);
            }
        }

        const auto applytojz =
        [ = ](double x) -> double {

            x = std::max(x, noise);
            x = std::max(x / gray, noise);//gray = gain - before log conversion
            x = std::max((xlog(x) / log2 - shadows_range) / dynamic_range, noise);//x in range EV
            assert(x == x);

            if (linbase > 0.)//apply log base in function of targetgray blackEvjz and Dynamic Range
            {
                x = xlog2lin(x, linbase);
            }

            return x;
        };

#ifdef _OPENMP
        #pragma omp parallel for if(multiThread)
#endif

        for (int i = 0; i < height; i++) {
            for (int k = 0; k < width; k++) {
                float L = lab->L[i][k];
                float a = lab->a[i][k];
                float b = lab->b[i][k];
                float x, y, z;
                //convert Lab => XYZ
                Color::Lab2XYZ(L, a, b, x, y, z);
                x = x / 65535.f;
                y = y / 65535.f;
                z = z / 65535.f;
                double Jz, az, bz;//double need because matrix with const(1.6295499532821566e-11) and others
                double xx, yy, zz;
                //change WP to D65
                xx = (d50_d65[0][0] * (double) x + d50_d65[0][1] * (double) y + d50_d65[0][2] * (double) z);
                yy = (d50_d65[1][0] * (double) x + d50_d65[1][1] * (double) y + d50_d65[1][2] * (double) z);
                zz = (d50_d65[2][0] * (double) x + d50_d65[2][1] * (double) y + d50_d65[2][2] * (double) z);

                double L_p, M_p, S_p;
                bool zcam = z_cam;
                Ciecam02::xyz2jzczhz(Jz, az, bz, xx, yy, zz, pl, L_p, M_p, S_p, zcam);
                //remapping Jz
                Jz = Jz * to_screen;
                az = az * to_screen;
                bz = bz * to_screen;
                JJz[i][k] = Jz;
                Aaz[i][k] = az;
                Bbz[i][k] = bz;

                if (highhs > 0 || shadhs > 0  || wavcurvejz || mjjz != 0.f || lp.mCjz != 0.f  || LHcurvejz || HHcurvejz || CHcurvejz) {
                    //here we work in float with usual functions  SH / wavelets / curves H
                    temp->L[i][k] = tempresid->L[i][k] = tempres->L[i][k] = (float) to_one * 32768.f * (float) JJz[i][k];
                    temp->a[i][k] = tempresid->a[i][k] = tempres->a[i][k] = (float) to_one * 32768.f * (float) Aaz[i][k];
                    temp->b[i][k] = tempresid->b[i][k] = tempres->b[i][k] = (float) to_one * 32768.f * (float) Bbz[i][k];
                }
            }
        }

        if (highhs > 0 || shadhs > 0) {
            ImProcFunctions::shadowsHighlights(temp.get(), true, 1, highhs, shadhs, radhs, sk, hltonahs * maxi * to_screen * to_one, shtonals * maxi * to_screen * to_one);
#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int i = 0; i < height; i++) {
                for (int k = 0; k < width; k++) {//reinitialize datas after SH...: guide, etc.
                    tempresid->L[i][k] = tempres->L[i][k] = temp->L[i][k];
                    tempresid->a[i][k] = tempres->a[i][k] = temp->a[i][k];
                    tempresid->b[i][k] = tempres->b[i][k] = temp->b[i][k];
                }
            }
        }

        //others "Lab" treatment...to adapt

        if (wavcurvejz  || mjjz != 0.f || lp.mCjz != 0.f) { //local contrast wavelet and clarity
#ifdef _OPENMP
            const int numThreads = omp_get_max_threads();
#else
            const int numThreads = 1;

#endif
            // adap maximum level wavelet to size of RT-spot
            int wavelet_level = 1 + params->locallab.spots.at(sp).csthresholdjz.getBottomRight();//retrieve with +1 maximum wavelet_level
            wavelet_level = rtengine::max(5, wavelet_level);
            int minwin = rtengine::min(width, height);
            int maxlevelspot = 9;//maximum possible

            // adapt maximum level wavelet to size of crop
            while ((1 << maxlevelspot) >= (minwin) && maxlevelspot  > 1) {
                --maxlevelspot ;
            }


            wavelet_level = rtengine::min(wavelet_level, maxlevelspot);
            int maxlvl = wavelet_level;

            //simple local contrast in function luminance
            if (locwavCurvejz && locwavutilijz && wavcurvejz) {
                float strengthjz = 1.2f;
                std::unique_ptr<wavelet_decomposition> wdspot(new wavelet_decomposition(temp->L[0], bfw, bfh, maxlvl, 1, sk, numThreads, lp.daubLen));//lp.daubLen

                if (wdspot->memory_allocation_failed()) {
                    return;
                }

                maxlvl = wdspot->maxlevel();
                wavlc(*wdspot, level_bljz, level_hljz, maxlvl, level_hrjz, level_brjz, ahighjz, bhighjz, alowjz, blowjz, sigmalcjz, 1.f, strengthjz, locwavCurvejz, numThreads);
                wdspot->reconstruct(temp->L[0], 1.f);

            }

            float thr = 0.001f;
            int flag = 2;

            // begin clarity wavelet jz
            if (mjjz != 0.f || lp.mCjz != 0.f) {
                float mL0 = 0.f;
                float mC0 = 0.f;
                bool exec = false;
                float mL = mjjz;
                float mC = lp.mCjz;
                clarimerge(lp, mL, mC, exec, tempresid.get(), wavelet_level, sk, numThreads);

                if (maxlvl <= 4) {
                    mL0 = 0.f;
                    mC0 = 0.f;
                    mL = -1.5f * mL;//increase only for sharpen
                    mC = -mC;
                    thr = 1.f;
                    flag = 0;

                } else {
                    mL0 = mL;
                    mC0 = mC;
                    thr = 1.f;
                    flag = 2;
                }

                LabImage *mergfile = temp.get();
#ifdef _OPENMP
                #pragma omp parallel for if (multiThread)
#endif

                for (int x = 0; x < height; x++)
                    for (int y = 0; y < width; y++) {
                        temp->L[x][y] = locallab::clipLoc((1.f + mL0) * mergfile->L[x][y] - mL * tempresid->L[x][y]);
                        temp->a[x][y] = locallab::clipC((1.f + mC0) * mergfile->a[x][y] - mC * tempresid->a[x][y]);
                        temp->b[x][y] = locallab::clipC((1.f + mC0) * mergfile->b[x][y] - mC * tempresid->b[x][y]);
                    }
            }

            if (lp.softrjz >= 0.5f && (wavcurvejz || std::fabs(mjjz) > 0.001f)) {//guidedfilter
                softproc(tempres.get(), temp.get(), lp.softrjz, height, width, 0.001, 0.00001, thr, sk, multiThread, flag);
            }
        }

//new curves Hz
#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif

        for (int i = 0; i < height; i++) {
            for (int k = 0; k < width; k++) {
                float j_z = temp->L[i][k];
                float C_z = sqrt(SQR(temp->a[i][k]) + SQR(temp->b[i][k]));
                float c_z = C_z / 32768.f;

                if (loclhCurvejz && LHcurvejz) {//Jz=f(Hz) curve
                    float kcz = (float) jzamountchr;
                    float Hz = xatan2f(temp->b[i][k], temp->a[i][k]);
                    float l_r = j_z / 32768.f;
                    float kcc = SQR(c_z / kcz);
                    jzch = true;

                    if (jzch == false) {
                        kcc = 1.f;
                    } else if (kcc > 1.f) {
                        kcc = 1.f; //cbrt(kcc);
                    }

                    float valparam = loclhCurvejz[500.f * static_cast<float>(Color::huejz_to_huehsv2((float) Hz))] - 0.5f;

                    float valparamneg;
                    valparamneg = valparam;
                    valparam *= 2.f * kcc;
                    valparamneg *= kcc;

                    if (valparam > 0.f) {
                        l_r = (1.f - valparam) * l_r + valparam * (1.f - SQR(((SQR(1.f - min(l_r, 1.0f))))));
                    } else
                        //for negative
                    {
                        float khue = 1.9f; //in reserve in case of!
                        l_r *= (1.f + khue * valparamneg);
                    }

                    temp->L[i][k] = l_r * 32768.f;
                }

                if (locchCurvejz && CHcurvejz) {//Cz=f(Hz) curve
                    float Hz = xatan2f(temp->b[i][k], temp->a[i][k]);
                    const float valparam = 1.5f * (locchCurvejz[500.f * static_cast<float>(Color::huejz_to_huehsv2((float)Hz))] - 0.5f);  //get valp=f(H)
                    float chromaCzfactor = 1.0f + valparam;
                    temp->a[i][k] *= chromaCzfactor;
                    temp->b[i][k] *= chromaCzfactor;
                }


                if (lochhCurvejz && HHcurvejz) { // Hz=f(Hz)
                    float Hz = xatan2f(temp->b[i][k], temp->a[i][k]);
                    const float valparam = 1.4f * (lochhCurvejz[500.f * static_cast<float>(Color::huejz_to_huehsv2((float)Hz))] - 0.5f) + static_cast<float>(Hz);
                    Hz = valparam;

                    if (Hz < 0.0f) {
                        Hz += (2.f * rtengine::RT_PI_F);
                    }

                    float2 sincosval = xsincosf(Hz);
                    temp->a[i][k] = C_z * sincosval.y;
                    temp->b[i][k] = C_z * sincosval.x;
                }
            }
        }

        if (loclhCurvejz && LHcurvejz && softjz > 0.f) {//Guidedilter for artifacts curve J(H)
            float thr = 0.00001f;
            int flag = 2;
            float softjzr = 0.05f * softjz;
            softproc(tempres.get(), temp.get(), softjzr, height, width, 0.000001, 0.00000001, thr, sk, multiThread, flag);
        }


        if ((lochhCurvejz && HHcurvejz) || (locchCurvejz && CHcurvejz)) { //for artifacts curve H(H)
            if (softjz > 0.f) {
                array2D<float> chro(width, height);
                array2D<float> hue(width, height);
                array2D<float> guid(width, height);

#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        hue[y][x] = xatan2f(temp->b[y][x], temp->a[y][x]);
                        chro[y][x] = sqrt(SQR(temp->b[y][x]) + SQR(temp->a[y][x])) / 32768.f;

                        if (hue[y][x] < 0.0f) {
                            hue[y][x] += (2.f * rtengine::RT_PI_F);
                        }

                        hue[y][x] /= (2.f * rtengine::RT_PI_F);
                        guid[y][x] = tempres->L[y][x] / 32768.f;
                    }
                }

                float softr = softjz;
                const float tmpblur = softr < 0.f ? -1.f / softr : 1.f + softr;
                const int r2 = rtengine::max<int>(10 / sk * tmpblur + 0.2f, 1);
                const int r1 = rtengine::max<int>(4 / sk * tmpblur + 0.5f, 1);
                constexpr float epsilmax = 0.0005f;
                constexpr float epsilmin = 0.0000001f;
                constexpr float aepsil = (epsilmax - epsilmin) / 100.f;
                constexpr float bepsil = epsilmin;
                const float epsil = softr < 0.f ? 0.001f : aepsil * softr + bepsil;

                if (lochhCurvejz && HHcurvejz) {
                    rtengine::guidedFilter(guid, hue, hue, r2, 0.5f * epsil, multiThread);
                }

                if (locchCurvejz && CHcurvejz) {
                    rtengine::guidedFilter(guid, chro, chro, r1, 0.4f * epsil, multiThread);
                }

#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        hue[y][x] *= (2.f * rtengine::RT_PI_F);
                        chro[y][x] *= 32768.f;
                        float2 sincosval = xsincosf(hue[y][x]);
                        temp->a[y][x] = chro[y][x] * sincosval.y;
                        temp->b[y][x] = chro[y][x] * sincosval.x;
                    }
                }
            }
        }


///////////////////


#ifdef _OPENMP
        #pragma omp parallel  for if(multiThread)
#endif

        for (int i = 0; i < height; i++) {
            for (int k = 0; k < width; k++) {
                //reconvert to double
                if (highhs > 0 || shadhs > 0  || wavcurvejz || mjjz != 0.f || lp.mCjz != 0.f || LHcurvejz || HHcurvejz || CHcurvejz) {
                    //now we work in double necessary for matrix conversion and when in range 0..1 with use of PQ
                    JJz[i][k] = (double)(temp->L[i][k] / (32768.f * (float) to_one));
                    Aaz[i][k] = (double)(temp->a[i][k] / (32768.f * (float) to_one));
                    Bbz[i][k] = (double)(temp->b[i][k] / (32768.f * (float) to_one));
                }

                double az =  Aaz[i][k];
                double bz =  Bbz[i][k];
                double Jz =  LIM01(JJz[i][k]);
                Jz *= to_one;
                double Cz = sqrt(az * az + bz * bz);

                //log encoding
                if (logjz) {
                    double jmz =  Jz;

                    if (jmz > noise) {
                        double mm = applytojz(jmz);
                        double f = mm / jmz;
                        Jz *= f;
                        Jz = LIM01(Jz);//clip values
                    }
                }

                //sigmoid 5.12
                if (issigjz12 && iscie && modeqj == 1) { //sigmoid Jz
                    float val = Jz;
                    float Jout = 0.f;
                    sigmoid_QJ(val, Jout, middle_grey_contrastjz, contrast_skewnessjz, middle_greyjz, black_pointjz, white_point_dispjz);

                    Jz = Jout;
                    Jz = LIM01(Jz);
                }

                //sigmoid 5.11
                if (issigjz && iscie && modeqj == 0) { //sigmoid Jz
                    float val = Jz;

                    if (islogjz) {
                        val = std::max((xlog(Jz) / log2 - shadows_range) / (dynamic_range + 1.5), noise);//in range EV
                    }

                    if (sigmoidthjz >= 1.f) {
                        thjz = athjz * val + bthjz;//threshold
                    } else {
                        thjz = atjz * val + btjz;
                    }

                    sigmoidla(val, thjz, sigmjz); //sigmz "slope" of sigmoid


                    Jz = LIM01((double) bljz * Jz + (double) val);
                }

                if (Qtoj == true) { //lightness instead of brightness
                    Jz /= to_one;
                    Jz /= maxjzw;//Jz white
                    Jz = SQR(Jz);
                }

                //contrast
                Jz = LIM01(jz_contrast.getVal(LIM01(Jz)));

                //brightness and lightness
                if (lightreal > 0) {
                    Jz = LIM01(jz_light.getVal(Jz));
                }

                if (lightreal < 0) {
                    Jz = LIM01(jz_lightn.getVal(Jz));
                }

                //Jz (Jz) curve
                double Jzold = Jz;

                if (jzlocalcurve && localjzutili) {
                    Jz = (double)(jzlocalcurve[(float) Jz * 65535.f] / 65535.f);
                    Jz  = 0.3 * (Jz - Jzold) + Jzold;
                }

                //reconvert from lightness or Brightness
                if (Qtoj == false) {
                    Jz /= to_one;
                } else {
                    Jz = sqrt(Jz);
                    Jz *= maxjzw;
                }

                double Hz;
                //remapping Cz
                Hz = xatan2(bz, az);
                double Czold = Cz;

                //Cz(Cz) curve
                if (czlocalcurve && localczutili) {
                    Cz = (double)(czlocalcurve[(float) Cz * 92666.f * (float) to_one] / (92666.f * (float) to_one));
                    Cz  = 0.5 * (Cz - Czold) + Czold;
                }

                //Cz(Jz) curve
                if (czjzlocalcurve && localczjzutili) {
                    double chromaCfactor = (double)(czjzlocalcurve[(float) Jz * 65535.f * (float) to_one]) / (Jz * 65535. * to_one);
                    Cz  *=  chromaCfactor;
                }

                //Hz in 0 2*PI
                if (Hz < 0.0) {
                    Hz += (2. * rtengine::RT_PI);
                }

                //Chroma slider
                if (chromz < 0.) {
                    Cz = Cz * (1. + 0.01 * chromz);
                } else {
                    double maxcz = czlim / to_one;
                    double fcz = Cz / maxcz;
                    double pocz = pow(fcz, 1. - 0.0024 * chromz); //increase value - before 0.0017
                    Cz = maxcz * pocz;
                    //  Cz = Cz * (1. + 0.005 * chromz);//linear
                }

                //saturation slider
                if (saturz != 0.) {
                    double js = Jz / maxjzw; //divide by Jz white
                    js = SQR(js);

                    if (js <= 0.) {
                        js = 0.0000001;
                    }

                    double Sz = Cz / (js);

                    if (saturz < 0.) {
                        Sz = Sz * (1. + 0.01 * saturz);
                    } else {
                        Sz = Sz * (1. + 0.003 * saturz);//not pow function because Sz is "open" - 0.003 empirical value to have results comparable to Cz
                    }

                    Cz = Sz * js;
                }

                //rotation hue
                Hz += dhue;

                if (Hz < 0.0) {
                    Hz += (2. * rtengine::RT_PI);
                }

                Cz = clipcz(Cz);
                double2 sincosval = xsincos(Hz);
                az = clipazbz(Cz * sincosval.y);
                bz = clipazbz(Cz * sincosval.x);
                Cz = sqrt(az * az + bz * bz);


                bz = bz / (to_screen);
                az = az / (to_screen);

                Jz = LIM01(Jz / (to_screen));

                if (jabcie) { //Not used does not work at all
                    Jz = clipjz05(Jz);
                    gamutjz(Jz, az, bz, pl, wip, 0.94, 0.004);
                }

                double L_, M_, S_;
                double xx, yy, zz;
                bool zcam = z_cam;
                //reconvert to XYZ in double
                Ciecam02::jzczhzxyz(xx, yy, zz, Jz, az, bz, pl, L_, M_, S_, zcam);
                //re enable D50
                double x, y, z;
                x = 65535. * (d65_d50[0][0] * xx + d65_d50[0][1] * yy + d65_d50[0][2] * zz);
                y = 65535. * (d65_d50[1][0] * xx + d65_d50[1][1] * yy + d65_d50[1][2] * zz);
                z = 65535. * (d65_d50[2][0] * xx + d65_d50[2][1] * yy + d65_d50[2][2] * zz);

                float Ll, aa, bb;
                Color::XYZ2Lab(x,  y,  z, Ll, aa, bb);
                lab->L[i][k] = Ll;
                lab->a[i][k] = aa;
                lab->b[i][k] = bb;
            }
        }
    }
                    //lp.sursouci==4 disable ciecam
    if ((mocam == 1 && lp.sursouci!= 4)|| mocam ==2 || call == 1  || call == 2 || call == 10) { //CAM16 call=2 vibrance warm-cool - call = 10 take into account "mean luminance Yb for Jz
//begin ciecam
        if (settings->verbose && (mocam == 1  || call == 1)) {//display only if choice cam16
            //information on Cam16 scene conditions - allows user to see choices's incidences
            float maxicamq = -1000.f;
            float maxicamj = -1000.f;
            float maxisat = -1000.f;
            float maxiM = -1000.f;
            float minicam = 1000000.f;
            float minicamq = 1000000.f;
            float minisat = 1000000.f;
            float miniM = 1000000.f;
            int nccam = 0;
            float sumcam = 0.f;
            float sumcamq = 0.f;
            float sumsat = 0.f;
            float sumM = 0.f;

            if (lp.logena && !(params->locallab.spots.at(sp).expcie && mocam == 1)) { //Log encoding only, but enable for log encoding if we use Cam16 module both with log encoding
                plum = 100.f;
            }

            //find main values Cam16
#ifdef _OPENMP
            #pragma omp parallel for reduction(min:minicam) reduction(max:maxicamj) reduction(min:minicamq) reduction(max:maxicamq) reduction(min:minisat) reduction(max:maxisat) reduction(min:miniM) reduction(max:maxiM) reduction(+:sumcam) reduction(+:sumcamq) reduction(+:sumsat) reduction(+:sumM)if(multiThread)
#endif

            for (int i = 0; i < height; i += 1) {
                for (int k = 0; k < width; k += 1) {
                    float L = lab->L[i][k];
                    float a = lab->a[i][k];
                    float b = lab->b[i][k];
                    float x, y, z;
                    //convert Lab => XYZ
                    Color::Lab2XYZ(L, a, b, x, y, z);
                    x = x / 655.35f;
                    y = y / 655.35f;
                    z = z / 655.35f;
                    float J, C, h, Q, M, s;
                    Ciecam02::xyz2jchqms_ciecam02float(J, C,  h,
                                                       Q,  M,  s, aw, fl, wh,
                                                       x,  y,  z,
                                                       xw1, yw1,  zw1,
                                                       c,  nc, pow1, nbb, ncb, pfl, cz, d, c16, plum);

                    if (J > maxicamj) {
                        maxicamj = J;
                    }

                    if (J < minicam) {
                        minicam = J;
                    }

                    sumcam += J;

                    if (Q > maxicamq) {
                        maxicamq = Q;
                    }

                    if (Q < minicamq) {
                        minicamq = Q;
                    }

                    sumcamq += Q;

                    if (s > maxisat) {
                        maxisat = s;
                    }

                    if (s < minisat) {
                        minisat = s;
                    }

                    sumsat += s;

                    if (M > maxiM) {
                        maxiM = M;
                    }

                    if (M < miniM) {
                        miniM = M;
                    }

                    sumM += M;
                }
            }

            nccam = height * width;
            sumcam = sumcam / nccam;
            sumcamq /= nccam;
            sumsat /= nccam;
            sumM /= nccam;

            if (settings->verbose) {
                printf("Cam16 Scene  Lighness_J Brightness_Q- HDR-PQ=%5.1f minJ=%3.1f maxJ=%3.1f meanJ=%3.1f minQ=%3.1f maxQ=%4.1f  meanQ=%4.1f meanQ1=%2.3f\n", (double) plum, (double) minicam, (double) maxicamj, (double) sumcam, (double) minicamq, (double) maxicamq, (double) sumcamq, (double) (sumcamq * coefq));
                printf("Cam16 Scene  Saturati-s Colorfulln_M- minSat=%3.1f maxSat=%3.1f meanSat=%3.1f minM=%3.1f maxM=%3.1f meanM=%3.1f\n", (double) minisat, (double) maxisat, (double) sumsat, (double) miniM, (double) maxiM, (double) sumM);
            }
           // maxicam = maxicamq;//maximum Brightness
            if(sumcamq < maxicamq) {
                // maxicam = sumcamq + 0.2f * minicamq;//maximum Brightness take into account
                 maxicam = sumcamq;//maximum Brightness take into account
                //ponderate maxicam with mean and mini
            } else {
                maxicam = 0.4f * sumcamq + 0.6f * maxicamq;
            }
            sumcamq01 = sumcamq * coefq;

        }

        float base = 10.f;
        float linbaseor = 10.f;
        float linbase = 10.f;
        float gray = 15.f;

        const bool compr = params->locallab.spots.at(sp).comprcie > 0.;
        float comprfactor = params->locallab.spots.at(sp).comprcie;
        float comprth = 1.f; //0.1 +  params->locallab.spots.at(sp).comprcieth;

        double drref = 8.5; //Dynamic Range standard

        double drd = ((double) dynamic_range - drref) / drref;

        double dratt = (double) dynamic_range / drref;
        comprfactor = 0.4f * comprfactor * (float) dratt;//adapt comprfactor to Dynamic Range
        float newgray = 0.18f;


     //   bool logqprov = false;
        if ((params->locallab.spots.at(sp).logcie && params->locallab.spots.at(sp).logcieq) || mobwev != 0) {//increase Dyn Range when log encoding
            dynamic_range += 0.2;//empirical value
            gray = 0.01f * (float) params->locallab.spots.at(sp).sourceGraycie;
            const float targetgraycie = params->locallab.spots.at(sp).targetGraycie;
            float targetgraycor = 0.01f * targetgraycie;
            base = targetgraycie > 1.f && targetgraycie < 100.f && (float) dynamic_range > 0.f ?  find_gray(std::abs((float) shadows_range) / (float) dynamic_range, (targetgraycor)) : 0.f;
            linbaseor = std::max(base, 2.f);//2. minimal base log to avoid very bad results

            float maxQgray = coefq * maxicam / gray;
            maxicam =  maxQgray;//setting threshold comprcieth
            const float log2 = xlogf(2.f);

            float corlog = xlogf(maxicam)/log2;//correction base logarithme
            linbase = linbaseor / corlog;
            newgray = gray; //gray - 0.022f * (6.f - maxicam);//empirical formula to take into account Q in DR. 6.f  =>approach to mean overall images

            if (settings->verbose) {
                printf("Gray=%1.3f newgray=%1.3f MaxicamQ=%3.2f Base log encode corrected Q=%5.1f Base log encode origig Q=%5.1f\n", (double) gray, (double) newgray, (double) maxicam, (double) linbase, (double) linbaseor);
            }

        }

        const auto applytoq =
        [ = ](float x) -> float {

            x = rtengine::max(x, (float) noise);
            x = rtengine::max(x / newgray, (float) noise);//gray = gain - before log conversion

            if (compr && x >= comprth)//comprth = maxicam
            {
                x = intp(comprfactor, (std::tanh((x - comprth) / comprth) + 1.f) * comprth, x); //as sigmoid... but tanh (tg hyperbolic), inspired by the work of alberto Grigio
            }

            x = rtengine::max((xlogf(x) / log2f - (float) shadows_range) / (float) dynamic_range, (float) noise);//x in range EV
            assert(x == x);

            if (linbase > 0.f)//apply log base in function of targetgray blackEvjz and Dynamic Range
            {
                x = xlog2lin(x, linbase);
            }

            return x;
        };

        //prepare Normalize luminance
        float *datain = nullptr;
        float *data = nullptr;
        float *datanorm = nullptr;

        if (((sigmoidnorm  && issigq)  || params->locallab.spots.at(sp).logcieq) && modeqj == 0) {//5.11
            datain = new float[width* height];
            data = new float[width * height];
            datanorm = new float[width * height];
#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic, 16)
#endif

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    datain[(y) * width + (x)] = lab->L[y][x];
                }
            }
        }

#if defined(__SSE2__) || defined(RT_SIMDE)
        int bufferLength = ((width + 3) / 4) * 4; // bufferLength has to be a multiple of 4
#endif
#ifdef _OPENMP
        #pragma omp parallel if (multiThread)
#endif
        {
#if defined(__SSE2__) || defined(RT_SIMDE)
            // one line buffer per channel and thread
            float Jbuffer[bufferLength] ALIGNED16;
            float Cbuffer[bufferLength] ALIGNED16;
            float hbuffer[bufferLength] ALIGNED16;
            float Qbuffer[bufferLength] ALIGNED16;
            float Mbuffer[bufferLength] ALIGNED16;
            float sbuffer[bufferLength] ALIGNED16;
#endif
#ifdef _OPENMP
            #pragma omp for schedule(dynamic, 16)
#endif

            for (int i = 0; i < height; i++) {
#if defined(__SSE2__) || defined(RT_SIMDE)
                // vectorized conversion from Lab to jchqms
                int k;
                vfloat c655d35 = F2V(655.35f);

                for (k = 0; k < width - 3; k += 4) {
                    vfloat x, y, z;
                    Color::Lab2XYZ(LVFU(lab->L[i][k]), LVFU(lab->a[i][k]), LVFU(lab->b[i][k]), x, y, z);
                    x = x / c655d35;
                    y = y / c655d35;
                    z = z / c655d35;
                    vfloat J, C, h, Q, M, s;
                    Ciecam02::xyz2jchqms_ciecam02float(J, C,  h,
                                                       Q,  M,  s, F2V(aw), F2V(fl), F2V(wh),
                                                       x,  y,  z,
                                                       F2V(xw1), F2V(yw1),  F2V(zw1),
                                                       F2V(c),  F2V(nc), F2V(pow1), F2V(nbb), F2V(ncb), F2V(pfl), F2V(cz), F2V(d), c16, F2V(plum));
                    STVF(Jbuffer[k], J);
                    STVF(Cbuffer[k], C);
                    STVF(hbuffer[k], h);
                    STVF(Qbuffer[k], Q);
                    STVF(Mbuffer[k], M);
                    STVF(sbuffer[k], s);
                }

                for (; k < width; k++) {
                    float L = lab->L[i][k];
                    float a = lab->a[i][k];
                    float b = lab->b[i][k];
                    float x, y, z;
                    //convert Lab => XYZ
                    Color::Lab2XYZ(L, a, b, x, y, z);
                    x = x / 655.35f;
                    y = y / 655.35f;
                    z = z / 655.35f;
                    float J, C, h, Q, M, s;
                    Ciecam02::xyz2jchqms_ciecam02float(J, C,  h,
                                                       Q,  M,  s, aw, fl, wh,
                                                       x,  y,  z,
                                                       xw1, yw1,  zw1,
                                                       c,  nc, pow1, nbb, ncb, pfl, cz, d, c16, plum);
                    Jbuffer[k] = J;
                    Cbuffer[k] = C;
                    hbuffer[k] = h;
                    Qbuffer[k] = Q;
                    Mbuffer[k] = M;
                    sbuffer[k] = s;
                }

#endif // __SSE2__

                for (int j = 0; j < width; j++) {
                    float J, C, h, Q, M, s;

#if defined(__SSE2__) || defined(RT_SIMDE)
                    // use precomputed values from above
                    J = Jbuffer[j];
                    C = Cbuffer[j];
                    h = hbuffer[j];
                    Q = Qbuffer[j];
                    M = Mbuffer[j];
                    s = sbuffer[j];
#else
                    float x, y, z;
                    float L = lab->L[i][j];
                    float a = lab->a[i][j];
                    float b = lab->b[i][j];
                    float x1, y1, z1;
                    //convert Lab => XYZ
                    Color::Lab2XYZ(L, a, b, x1, y1, z1);
                    x = x1 / 655.35f;
                    y = y1 / 655.35f;
                    z = z1 / 655.35f;
                    //process source==> normal
                    Ciecam02::xyz2jchqms_ciecam02float(J, C,  h,
                                                       Q,  M,  s, aw, fl, wh,
                                                       x,  y,  z,
                                                       xw1, yw1,  zw1,
                                                       c,  nc, pow1, nbb, ncb, pfl, cz, d, c16, plum);
#endif
                    float Jpro, Cpro, hpro, Qpro, Mpro, spro;
                    Jpro = J;
                    Cpro = C;
                    hpro = h;
                    Qpro = Q;
                    Mpro = M;
                    spro = s;

                    if (ciec  && mocam == 1) {//only Cam16
                        bool jp = false;

                      //  if (params->locallab.spots.at(sp).logcie && iscie) {//log encoding Q
                        if (params->locallab.spots.at(sp).logcie && params->locallab.spots.at(sp).logcieq && iscie  && modeqj == 0) {//log encoding Q 5.11

                            float val =  Qpro *  coefq;

                            if (val > (float) noise) {
                                float mm = applytoq(val);
                                float f = mm / val;
                                Qpro *=  f;
                            }
                        }
                        if (issig && issigq12 && iscie && modeqj == 1) { //sigmoid Q and slope based Q 5.12
                            float val = Qpro * coefq;
                            float Qout = 0.f;
                            if(mobwev12 == 0) {
                                sigmoid_QJ(val, Qout, middle_grey_contrast, contrast_skewness, middle_grey, black_point, white_point_disp);
                            }
                            if(mobwev12 == 1) {
                                bool rolloff = false;//all range
                                bool kmid = false;//not take into account Yb viewing
                                tonemapFreemanQ(val, Qout, slopsmootq , white_pointsig, black_point, middle_grey, mid_gray_view, rolloff, kmid);
                            }

                            Qpro = std::max(Qout / coefq, 0.f);
                            Jpro = SQR((10.f * Qpro) / wh);

                        }

                        if (issig && issigq && iscie && mobwev != 2 && modeqj == 0) { //sigmoid Q only and black Ev & white Ev 5.11

                           float val = Qpro * coefq;

                            if (mobwev == 1) {
                                val = std::max((xlog(val) / log2 - shadows_range) / (dynamic_range + 1.5), noise);//in range EV
                            }

                            float sigreal = sigmoidth * sumcamq01;//correction for sigmoid Q take into account mean Q

                            if (sigreal >= 1.f) {
                                th = (sigreal - 1.f) * val + 1.f;
                            } else {
                                th = (1.f - sigreal) * val + sigreal;
                            }

                            sigmoidla(val, th, sigm);
                            Qpro = std::max(Qpro + val / coefq, 0.f);
                            Qpro = CAMBrightCurveQsig[(float)(Qpro * coefQ)] / coefQ;   //brightness and contrast

                            Jpro = SQR((10.f * Qpro) / wh);

                        }

                        if ((cielocalcurve && localcieutili) && mecamcurve == 1) {//curve Q
                            jp = true;
                            float Qq = Qpro * coefQ;
                            float Qold = Qpro;
                            Qq = 0.5f * cielocalcurve[Qq * 2.f];
                            Qq = Qq / coefQ;
                            Qpro = 0.2f * (Qq - Qold) + Qold;

                            if (jp) {
                                Jpro = SQR((10.f * Qpro) / wh);
                            }
                        }

                        Qpro = CAMBrightCurveQ[(float)(Qpro * coefQ)] / coefQ;   //brightness and contrast

                        float Mp, sres;
                        Mp = Mpro / 100.0f;
                        Ciecam02::curvecolorfloat(mchr, Mp, sres, 2.5f);
                        float dred = 100.f; //in C mode
                        float protect_red = 80.0f; // in C mode
                        dred *= coe; //in M mode
                        protect_red *= coe; //M mode
                        Color::skinredfloat(Jpro, hpro, sres, Mp, dred, protect_red, 0, rstprotection, 100.f, Mpro);
                        Jpro = SQR((10.f * Qpro) / wh);
                        Qpro = (Qpro == 0.f ? epsil : Qpro); // avoid division by zero
                        spro = 100.0f * sqrtf(Mpro / Qpro);
                        Jpro = CAMBrightCurveJ[(float)(Jpro * 327.68f)];   //lightness CIECAM02 + contrast
                        float Sp = spro / 100.0f;
                        Ciecam02::curvecolorfloat(schr, Sp, sres, 1.5f);
                        dred = 100.f; // in C mode
                        protect_red = 80.0f; // in C mode
                        dred = 100.0f * sqrtf((dred * coe) / Q);
                        protect_red = 100.0f * sqrtf((protect_red * coe) / Q);
                        Color::skinredfloat(Jpro, hpro, sres, Sp, dred, protect_red, 0, rstprotection, 100.f, spro);
                        Qpro = QproFactor * sqrtf(Jpro);
                        float Cp = (spro * spro * Qpro) / (1000000.f);
                        Cpro = Cp * 100.f;
                        Ciecam02::curvecolorfloat(cchr, Cp, sres, 1.8f);
                        Color::skinredfloat(Jpro, hpro, sres, Cp, 55.f, 30.f, 1, rstprotection, 100.f, Cpro);

                        hpro = hpro + hue;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }

                        if ((cielocalcurve && localcieutili) && mecamcurve == 0) {//curve J
                            float Jj = (float) Jpro * 327.68f;
                            float Jold = Jj;
                            Jj = 0.5f * cielocalcurve[Jj * 2.f];
                            Jj = 0.3f * (Jj - Jold) + Jold;    //divide sensibility
                            Jpro = (float)(Jj / 327.68f);

                            if (Jpro < 1.f) {
                                Jpro = 1.f;
                            }
                        }

                        if (cielocalcurve2 && localcieutili2) {//chroma saturation colorfullness
                            if (mecamcurve2 == 0) {//chroma
                                float parsat = 0.8f; //0.68;
                                float coef = 327.68f / parsat;
                                float Cc = (float) Cpro * coef;
                                float Ccold = Cc;
                                Cc = 0.5f * cielocalcurve2[Cc * 2.f];
                                float dred = 55.f;
                                float protect_red = 30.0f;
                                int sk1 = 1;
                                float ko = 1.f / coef;
                                Color::skinredfloat(Jpro, hpro, Cc, Ccold, dred, protect_red, sk1, rstprotection, ko, Cpro);
                            } else if (mecamcurve2 == 1) {//saturation
                                float parsat = 0.8f; //0.6
                                float coef = 327.68f / parsat;
                                float Ss = (float) spro * coef;
                                float Sold = Ss;
                                Ss = 0.5f * cielocalcurve2[Ss * 2.f];
                                Ss = 0.6f * (Ss - Sold) + Sold; //divide sensibility saturation
                                float dred = 100.f; // in C mode
                                float protect_red = 80.0f; // in C mode
                                dred = 100.0f * sqrtf((dred * coe) / Qpro);
                                protect_red = 100.0f * sqrtf((protect_red * coe) / Qpro);
                                float ko = 1.f / coef;
                                Color::skinredfloat(Jpro, hpro, Ss, Sold, dred, protect_red, 0, rstprotection, ko, spro);
                                Qpro = (4.0f / c) * sqrtf(Jpro / 100.0f) * (aw + 4.0f) ;
                                Cpro = (spro * spro * Qpro) / (10000.0f);
                            } else if (mecamcurve2 == 2) {//colorfullness
                                float parsat = 0.8f; //0.68;
                                float coef = 327.68f / parsat;
                                float Mm = (float) Mpro * coef;
                                float Mold = Mm;
                                Mm = 0.5f * cielocalcurve2[Mm * 2.f];
                                float dred = 100.f; //in C mode
                                float protect_red = 80.0f; // in C mode
                                dred *= coe; //in M mode
                                protect_red *= coe;
                                float ko = 1.f / coef;
                                Color::skinredfloat(Jpro, hpro, Mm, Mold, dred, protect_red, 0, rstprotection, ko, Mpro);
                                Cpro = Mpro / coe;
                            }
                        }
                    }

                    //retrieve values C,J...s
                    C = Cpro;
                    J = Jpro;
                    Q = Qpro;
                    M = Mpro;
                    h = hpro;
                    s = spro;

#if defined(__SSE2__) || defined(RT_SIMDE)
                    // write to line buffers
                    Jbuffer[j] = J;
                    Cbuffer[j] = C;
                    hbuffer[j] = h;
#else
                    float xx, yy, zz;
                    //process normal==> viewing

                    Ciecam02::jch2xyz_ciecam02float(xx, yy, zz,
                                                    J,  C, h,
                                                    xw2, yw2,  zw2,
                                                    c2, nc2,  pow1n, nbbj, ncbj, flj, czj, dj, awj, c16, plum);
                    x = CLIP(xx * 655.35f);
                    y = CLIP(yy * 655.35f);
                    z = CLIP(zz * 655.35f);
                    float Ll, aa, bb;
                    //convert xyz=>lab
                    Color::XYZ2Lab(x,  y,  z, Ll, aa, bb);
                    lab->L[i][j] = Ll;
                    lab->a[i][j] = aa;
                    lab->b[i][j] = bb;
#endif
                }

#if defined(__SSE2__) || defined(RT_SIMDE)
                // process line buffers
                float *xbuffer = Qbuffer;
                float *ybuffer = Mbuffer;
                float *zbuffer = sbuffer;

                for (k = 0; k < bufferLength; k += 4) {
                    vfloat x, y, z;
                    Ciecam02::jch2xyz_ciecam02float(x, y, z,
                                                    LVF(Jbuffer[k]), LVF(Cbuffer[k]), LVF(hbuffer[k]),
                                                    F2V(xw2), F2V(yw2), F2V(zw2),
                                                    F2V(nc2), F2V(pow1n), F2V(nbbj), F2V(ncbj), F2V(flj), F2V(dj), F2V(awj), F2V(reccmcz), c16, F2V(plum));
                    STVF(xbuffer[k], x * c655d35);
                    STVF(ybuffer[k], y * c655d35);
                    STVF(zbuffer[k], z * c655d35);
                }

                // XYZ2Lab uses a lookup table. The function behind that lut is a cube root.
                // SSE can't beat the speed of that lut, so it doesn't make sense to use SSE
                for (int j = 0; j < width; j++) {
                    float Ll, aa, bb;
                    //convert xyz=>lab
                    xbuffer[j] = CLIP(xbuffer[j]);
                    ybuffer[j] = CLIP(ybuffer[j]);
                    zbuffer[j] = CLIP(zbuffer[j]);

                    Color::XYZ2Lab(xbuffer[j], ybuffer[j], zbuffer[j], Ll, aa, bb);

                    lab->L[i][j] = Ll;
                    lab->a[i][j] = aa;
                    lab->b[i][j] = bb;
                }

#endif
            }
        }
         if (((mocam == 1 && (sigmoidnorm && issigq)) || params->locallab.spots.at(sp).logcieq) && modeqj == 0) { //Normalize luminance 5.11

#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic, 16)
#endif

            for (int y = 0; y < height; y++) { //data after ciecam
                for (int x = 0; x < width; x++) {
                    data[(y) * width + (x)] = lab->L[y][x];
                    datanorm[(y) * width + (x)] = lab->L[y][x];

                }
            }

            double nbs = 1.;
            drd = std::max(drd, 1.);
            if (bl > 0.5f) {
                nbs = (1.7 * (double) bl * drd);//take into account DR to increase variance in image source
            }
            if(!params->locallab.spots.at(sp).logcieq) {// not with log encoding Q
                normalize_mean_dt(datanorm, datain, height * width, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, nbs);//normalize luminance
            }

#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic, 16)
#endif

            for (int ir = 0; ir < height; ir++) {
                for (int jr = 0; jr < width; jr++) {
                    if(!params->locallab.spots.at(sp).logcieq) {// if not Log encoding ciecam
                        data[ir * width + jr] = intp(bl, data[ir * width + jr], datanorm[ir * width + jr]);//blend with original
                    } else {
                        data[ir * width + jr] = intp(bl, data[ir * width + jr], datain[ir * width + jr]);//blend with original
                    }
                    lab->L[ir][jr] = data[ir * width + jr];
                }
            }
        }

        delete [] datain;
        delete [] data;
        delete [] datanorm;



    }



}

} // namespace rtengine
