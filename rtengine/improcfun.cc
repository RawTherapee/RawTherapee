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
#include <cmath>

#include <glib.h>
#include <glibmm/ustring.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "alignedbuffer.h"
#include "calc_distort.h"
#include "ciecam02.h"
#include "cieimage.h"
#include "clutstore.h"
#include "color.h"
#include "colortemp.h"
#include "curves.h"
#include "dcp.h"
#include "EdgePreservingDecomposition.h"
#include "iccmatrices.h"
#include "iccstore.h"
#include "imagesource.h"
#include "improcfun.h"
#include "labimage.h"
#include "pipettebuffer.h"
#include "procparams.h"
#include "rt_math.h"
#include "rtengine.h"
#include "rtthumbnail.h"
#include "satandvalueblendingcurve.h"
#include "StopWatch.h"
#include "utils.h"

#include "rtgui/editcallbacks.h"

#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic warning "-Wdouble-promotion"

namespace
{

using namespace rtengine;


// begin of helper function for rgbProc()
void shadowToneCurve(const LUTf &shtonecurve, float *rtemp, float *gtemp, float *btemp, int istart, int tH, int jstart, int tW, int tileSize)
{

#if (defined( __SSE2__ ) && defined( __x86_64__ )) || defined(RT_SIMDE)
    vfloat cr = F2V(0.299f);
    vfloat cg = F2V(0.587f);
    vfloat cb = F2V(0.114f);
#endif

    for (int i = istart, ti = 0; i < tH; i++, ti++) {
        int j = jstart, tj = 0;
#if (defined( __SSE2__ ) && defined( __x86_64__ )) || defined(RT_SIMDE)

        for (; j < tW - 3; j += 4, tj += 4) {

            vfloat rv = LVF(rtemp[ti * tileSize + tj]);
            vfloat gv = LVF(gtemp[ti * tileSize + tj]);
            vfloat bv = LVF(btemp[ti * tileSize + tj]);

            //shadow tone curve
            vfloat Yv = cr * rv + cg * gv + cb * bv;
            vfloat tonefactorv = shtonecurve[Yv];
            STVF(rtemp[ti * tileSize + tj], rv * tonefactorv);
            STVF(gtemp[ti * tileSize + tj], gv * tonefactorv);
            STVF(btemp[ti * tileSize + tj], bv * tonefactorv);
        }

#endif

        for (; j < tW; j++, tj++) {

            float r = rtemp[ti * tileSize + tj];
            float g = gtemp[ti * tileSize + tj];
            float b = btemp[ti * tileSize + tj];

            //shadow tone curve
            float Y = (0.299f * r + 0.587f * g + 0.114f * b);
            float tonefactor = shtonecurve[Y];
            rtemp[ti * tileSize + tj] = rtemp[ti * tileSize + tj] * tonefactor;
            gtemp[ti * tileSize + tj] = gtemp[ti * tileSize + tj] * tonefactor;
            btemp[ti * tileSize + tj] = btemp[ti * tileSize + tj] * tonefactor;
        }
    }
}

void highlightToneCurve(const LUTf &hltonecurve, float *rtemp, float *gtemp, float *btemp, int istart, int tH, int jstart, int tW, int tileSize, float exp_scale, float comp, float hlrange)
{

#if (defined( __SSE2__ ) && defined( __x86_64__ )) || defined(RT_SIMDE)
    vfloat threev = F2V(3.f);
    vfloat maxvalfv = F2V(MAXVALF);
#endif

    for (int i = istart, ti = 0; i < tH; i++, ti++) {
        int j = jstart, tj = 0;
#if (defined( __SSE2__ ) && defined( __x86_64__ )) || defined(RT_SIMDE)

        for (; j < tW - 3; j += 4, tj += 4) {

            vfloat rv = LVF(rtemp[ti * tileSize + tj]);
            vfloat gv = LVF(gtemp[ti * tileSize + tj]);
            vfloat bv = LVF(btemp[ti * tileSize + tj]);

            //TODO: proper treatment of out-of-gamut colors
            //float tonefactor = hltonecurve[(0.299f*r+0.587f*g+0.114f*b)];
            vmask maxMask = vmaskf_ge(vmaxf(rv, vmaxf(gv, bv)), maxvalfv);

            if (_mm_movemask_ps((vfloat)maxMask)) {
                for (int k = 0; k < 4; ++k) {
                    float r = rtemp[ti * tileSize + tj + k];
                    float g = gtemp[ti * tileSize + tj + k];
                    float b = btemp[ti * tileSize + tj + k];
                    float tonefactor = ((r < MAXVALF ? hltonecurve[r] : CurveFactory::hlcurve(exp_scale, comp, hlrange, r)) +
                                        (g < MAXVALF ? hltonecurve[g] : CurveFactory::hlcurve(exp_scale, comp, hlrange, g)) +
                                        (b < MAXVALF ? hltonecurve[b] : CurveFactory::hlcurve(exp_scale, comp, hlrange, b))) / 3.f;

                    // note: tonefactor includes exposure scaling, that is here exposure slider and highlight compression takes place
                    rtemp[ti * tileSize + tj + k] = r * tonefactor;
                    gtemp[ti * tileSize + tj + k] = g * tonefactor;
                    btemp[ti * tileSize + tj + k] = b * tonefactor;
                }
            } else {
                vfloat tonefactorv = (hltonecurve.cb(rv) + hltonecurve.cb(gv) + hltonecurve.cb(bv)) / threev;
                // note: tonefactor includes exposure scaling, that is here exposure slider and highlight compression takes place
                STVF(rtemp[ti * tileSize + tj], rv * tonefactorv);
                STVF(gtemp[ti * tileSize + tj], gv * tonefactorv);
                STVF(btemp[ti * tileSize + tj], bv * tonefactorv);
            }
        }

#endif

        for (; j < tW; j++, tj++) {

            float r = rtemp[ti * tileSize + tj];
            float g = gtemp[ti * tileSize + tj];
            float b = btemp[ti * tileSize + tj];

            //TODO: proper treatment of out-of-gamut colors
            //float tonefactor = hltonecurve[(0.299f*r+0.587f*g+0.114f*b)];
            float tonefactor = ((r < MAXVALF ? hltonecurve[r] : CurveFactory::hlcurve(exp_scale, comp, hlrange, r)) +
                                (g < MAXVALF ? hltonecurve[g] : CurveFactory::hlcurve(exp_scale, comp, hlrange, g)) +
                                (b < MAXVALF ? hltonecurve[b] : CurveFactory::hlcurve(exp_scale, comp, hlrange, b))) / 3.f;

            // note: tonefactor includes exposure scaling, that is here exposure slider and highlight compression takes place
            rtemp[ti * tileSize + tj] = r * tonefactor;
            gtemp[ti * tileSize + tj] = g * tonefactor;
            btemp[ti * tileSize + tj] = b * tonefactor;
        }
    }
}

void proPhotoBlue(float *rtemp, float *gtemp, float *btemp, int istart, int tH, int jstart, int tW, int tileSize)
{
    // this is a hack to avoid the blue=>black bug (Issue 2141)
    for (int i = istart, ti = 0; i < tH; i++, ti++) {
        int j = jstart, tj = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)

        for (; j < tW - 3; j += 4, tj += 4) {
            vfloat rv = LVF(rtemp[ti * tileSize + tj]);
            vfloat gv = LVF(gtemp[ti * tileSize + tj]);
            vmask zeromask = vorm(vmaskf_eq(rv, ZEROV), vmaskf_eq(gv, ZEROV));

            if (_mm_movemask_ps((vfloat)zeromask)) {
                for (int k = 0; k < 4; ++k) {
                    float r = rtemp[ti * tileSize + tj + k];
                    float g = gtemp[ti * tileSize + tj + k];

                    float b = btemp[ti * tileSize + tj + k];

                    if ((r == 0.0f || g == 0.0f) && rtengine::min(r, g, b) >= 0.f) {
                        float h, s, v;
                        Color::rgb2hsv(r, g, b, h, s, v);
                        s *= 0.99f;
                        Color::hsv2rgb(h, s, v, rtemp[ti * tileSize + tj + k], gtemp[ti * tileSize + tj + k], btemp[ti * tileSize + tj + k]);
                    }
                }
            }
        }

#endif

        for (; j < tW; j++, tj++) {
            float r = rtemp[ti * tileSize + tj];
            float g = gtemp[ti * tileSize + tj];
            float b = btemp[ti * tileSize + tj];

            if ((r == 0.0f || g == 0.0f) && rtengine::min(r, g, b) >= 0.f) {
                float h, s, v;
                Color::rgb2hsv(r, g, b, h, s, v);
                s *= 0.99f;
                Color::hsv2rgb(h, s, v, rtemp[ti * tileSize + tj], gtemp[ti * tileSize + tj], btemp[ti * tileSize + tj]);
            }
        }
    }
}

void customToneCurve(const ToneCurve &customToneCurve, ToneCurveMode curveMode, float *rtemp, float *gtemp, float *btemp, int istart, int tH, int jstart, int tW, int tileSize, PerceptualToneCurveState ptcApplyState)
{

    if (curveMode == ToneCurveMode::STD) { // Standard
        const StandardToneCurve& userToneCurve = static_cast<const StandardToneCurve&>(customToneCurve);

        for (int i = istart, ti = 0; i < tH; i++, ti++) {
            userToneCurve.BatchApply(0, tW - jstart, &rtemp[ti * tileSize], &gtemp[ti * tileSize], &btemp[ti * tileSize]);
        }
    } else if (curveMode == ToneCurveMode::FILMLIKE) { // Adobe like
        const AdobeToneCurve& userToneCurve = static_cast<const AdobeToneCurve&>(customToneCurve);

        for (int i = istart, ti = 0; i < tH; i++, ti++) {
            userToneCurve.BatchApply(0, tW - jstart, &rtemp[ti * tileSize], &gtemp[ti * tileSize], &btemp[ti * tileSize]);
        }
    } else if (curveMode == ToneCurveMode::SATANDVALBLENDING) { // apply the curve on the saturation and value channels
        const SatAndValueBlendingToneCurve& userToneCurve = static_cast<const SatAndValueBlendingToneCurve&>(customToneCurve);

        for (int i = istart, ti = 0; i < tH; i++, ti++) {
            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                userToneCurve.Apply(rtemp[ti * tileSize + tj], gtemp[ti * tileSize + tj], btemp[ti * tileSize + tj]);
            }
        }
    } else if (curveMode == ToneCurveMode::WEIGHTEDSTD) { // apply the curve to the rgb channels, weighted
        const WeightedStdToneCurve& userToneCurve = static_cast<const WeightedStdToneCurve&>(customToneCurve);

        for (int i = istart, ti = 0; i < tH; i++, ti++) {
            userToneCurve.BatchApply(0, tW - jstart, &rtemp[ti * tileSize], &gtemp[ti * tileSize], &btemp[ti * tileSize]);
        }
    } else if (curveMode == ToneCurveMode::LUMINANCE) { // apply the curve to the luminance channel
        const LuminanceToneCurve& userToneCurve = static_cast<const LuminanceToneCurve&>(customToneCurve);

        for (int i = istart, ti = 0; i < tH; i++, ti++) {
            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                userToneCurve.Apply(rtemp[ti * tileSize + tj], gtemp[ti * tileSize + tj], btemp[ti * tileSize + tj]);
            }
        }
    } else if (curveMode == ToneCurveMode::PERCEPTUAL) { // apply curve while keeping color appearance constant
        const PerceptualToneCurve& userToneCurve = static_cast<const PerceptualToneCurve&>(customToneCurve);

        for (int i = istart, ti = 0; i < tH; i++, ti++) {
            userToneCurve.BatchApply(0, tW - jstart, &rtemp[ti * tileSize], &gtemp[ti * tileSize], &btemp[ti * tileSize], ptcApplyState);
        }
    }
}

void fillEditFloat(float *editIFloatTmpR, float *editIFloatTmpG, float *editIFloatTmpB, float *rtemp, float *gtemp, float *btemp, int istart, int tH, int jstart, int tW, int tileSize)
{
    for (int i = istart, ti = 0; i < tH; i++, ti++) {
        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
            editIFloatTmpR[ti * tileSize + tj] = Color::gamma2curve[rtemp[ti * tileSize + tj]] / 65535.f;
            editIFloatTmpG[ti * tileSize + tj] = Color::gamma2curve[gtemp[ti * tileSize + tj]] / 65535.f;
            editIFloatTmpB[ti * tileSize + tj] = Color::gamma2curve[btemp[ti * tileSize + tj]] / 65535.f;
        }
    }
}
// end of helper function for rgbProc()

}

namespace rtengine
{

using namespace procparams;

ImProcFunctions::~ImProcFunctions()
{
    if (monitorTransform) {
        cmsDeleteTransform(monitorTransform);
    }
}

void ImProcFunctions::setScale(double iscale)
{
    scale = iscale;
}


void ImProcFunctions::updateColorProfiles(const Glib::ustring& monitorProfile, RenderingIntent monitorIntent, bool softProof, bool gamutCheck)
{
    // set up monitor transform
    if (monitorTransform) {
        cmsDeleteTransform(monitorTransform);
    }

    gamutWarning.reset(nullptr);

    monitorTransform = nullptr;

    cmsHPROFILE monitor = nullptr;

    if (!monitorProfile.empty()) {
#if !defined(__APPLE__) // No support for monitor profiles on OS X, all data is sRGB
        monitor = ICCStore::getInstance()->getProfile(monitorProfile);
#else
        monitor = ICCStore::getInstance()->getProfile(settings->srgb);
#endif
    }

    if (monitor) {
        MyMutex::MyLock lcmsLock(*lcmsMutex);

        cmsUInt32Number flags;
        cmsHPROFILE iprof  = cmsCreateLab4Profile(nullptr);
        cmsHPROFILE gamutprof = nullptr;
        cmsUInt32Number gamutbpc = 0;
        RenderingIntent gamutintent = RI_RELATIVE;

        bool softProofCreated = false;

        if (softProof) {
            cmsHPROFILE oprof = nullptr;
            RenderingIntent outIntent;

            flags = cmsFLAGS_SOFTPROOFING | cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE;

            if (!settings->printerProfile.empty()) {
                oprof = ICCStore::getInstance()->getProfile(settings->printerProfile);

                if (settings->printerBPC) {
                    flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
                }

                outIntent = RenderingIntent(settings->printerIntent);
            } else {
                oprof = ICCStore::getInstance()->getProfile(params->icm.outputProfile);

                if (params->icm.outputBPC) {
                    flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
                }

                outIntent = params->icm.outputIntent;
            }

            if (oprof) {
                // NOCACHE is for thread safety, NOOPTIMIZE for precision

                // if (gamutCheck) {
                //     flags |= cmsFLAGS_GAMUTCHECK;
                // }

                const auto make_gamma_table =
                [](cmsHPROFILE prof, cmsTagSignature tag) -> void {
                    cmsToneCurve *tc = static_cast<cmsToneCurve *>(cmsReadTag(prof, tag));

                    if (tc)
                    {
                        const cmsUInt16Number *table = cmsGetToneCurveEstimatedTable(tc);
                        cmsToneCurve *tc16 = cmsBuildTabulatedToneCurve16(nullptr, cmsGetToneCurveEstimatedTableEntries(tc), table);

                        if (tc16) {
                            cmsWriteTag(prof, tag, tc16);
                            cmsFreeToneCurve(tc16);
                        }
                    }
                };

                cmsHPROFILE softproof = ProfileContent(oprof).toProfile();

                if (softproof) {
                    make_gamma_table(softproof, cmsSigRedTRCTag);
                    make_gamma_table(softproof, cmsSigGreenTRCTag);
                    make_gamma_table(softproof, cmsSigBlueTRCTag);
                }

                monitorTransform = cmsCreateProofingTransform(
                                       iprof, TYPE_Lab_FLT,
                                       monitor, TYPE_RGB_FLT,
                                       softproof, //oprof,
                                       monitorIntent, outIntent,
                                       flags
                                   );

                if (softproof) {
                    cmsCloseProfile(softproof);
                }

                if (monitorTransform) {
                    softProofCreated = true;
                }

                if (gamutCheck) {
                    gamutprof = oprof;

                    if (params->icm.outputBPC) {
                        gamutbpc = cmsFLAGS_BLACKPOINTCOMPENSATION;
                    }

                    gamutintent = outIntent;
                }
            }
        } else if (gamutCheck) {
            // flags = cmsFLAGS_GAMUTCHECK | cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE;
            // if (settings->monitorBPC) {
            //     flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
            // }

            // monitorTransform = cmsCreateProofingTransform(iprof, TYPE_Lab_FLT, monitor, TYPE_RGB_8, monitor, monitorIntent, monitorIntent, flags);

            // if (monitorTransform) {
            //     softProofCreated = true;
            // }
            gamutprof = monitor;

            if (settings->monitorBPC) {
                gamutbpc = cmsFLAGS_BLACKPOINTCOMPENSATION;
            }

            gamutintent = monitorIntent;
        }

        if (!softProofCreated) {
            flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE;

            if (settings->monitorBPC) {
                flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
            }

            monitorTransform = cmsCreateTransform(iprof, TYPE_Lab_FLT, monitor, TYPE_RGB_FLT, monitorIntent, flags);
        }

        if (gamutCheck && gamutprof) {
            gamutWarning.reset(new GamutWarning(iprof, gamutprof, gamutintent, gamutbpc));
        }

        cmsCloseProfile(iprof);
    }
}

void ImProcFunctions::firstAnalysis(const Imagefloat* const original, const ProcParams &params, LUTu & histogram)
{

    TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params.icm.workingProfile);

    lumimul[0] = wprof[1][0];
    lumimul[1] = wprof[1][1];
    lumimul[2] = wprof[1][2];
    int W = original->getWidth();
    int H = original->getHeight();

    float lumimulf[3] = {static_cast<float>(lumimul[0]), static_cast<float>(lumimul[1]), static_cast<float>(lumimul[2])};

    // calculate histogram of the y channel needed for contrast curve calculation in exposure adjustments
    histogram.clear();

    if (multiThread) {

#ifdef _OPENMP
        const int numThreads = min(max(W * H / (int)histogram.getSize(), 1), omp_get_max_threads());
        #pragma omp parallel num_threads(numThreads) if(numThreads>1)
#endif
        {
            LUTu hist(histogram.getSize());
            hist.clear();
#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int i = 0; i < H; i++) {
                for (int j = 0; j < W; j++) {

                    float r = original->r(i, j);
                    float g = original->g(i, j);
                    float b = original->b(i, j);

                    int y = (lumimulf[0] * r + lumimulf[1] * g + lumimulf[2] * b);
                    hist[y]++;
                }
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            histogram += hist;

        }
#ifdef _OPENMP
        static_cast<void>(numThreads);  // to silence cppcheck warning
#endif
    } else {
        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {

                float r = original->r(i, j);
                float g = original->g(i, j);
                float b = original->b(i, j);

                int y = (lumimulf[0] * r + lumimulf[1] * g + lumimulf[2] * b);
                histogram[y]++;
            }
        }
    }
}


// Copyright (c) 2012 -2020 - 2026 Jacques Desmis <jdesmis@gmail.com>

void ImProcFunctions::ciecam_02float(CieImage* ncie, float adap, int pW, int pwb, LabImage* lab, const ProcParams* params,
                                     const ColorAppearance & customColCurve1, const ColorAppearance & customColCurvered, const ColorAppearance & customColCurvegreen, const ColorAppearance & customColCurveblue, const ColorAppearance & customColCurve2, const ColorAppearance & customColCurve3,
                                     LUTu & histLCAM, LUTu & histCCAM, LUTf & CAMBrightCurveJ, LUTf & CAMBrightCurveQ, float &mean, int Iterates, int scale, bool execsharp, float &d, float &dj, float &yb, int rtt,
                                     bool showSharpMask)
{
    if (params->colorappearance.enabled) {
        //preparate for histograms CIECAM
        LUTu hist16JCAM;
        LUTu hist16_CCAM;

        if (pW != 1 && params->colorappearance.datacie) { //only with improccoordinator
            hist16JCAM(32768);
            hist16JCAM.clear();
            hist16_CCAM(48000);
            hist16_CCAM.clear();
        }

        //end preparate histogram
        int width = lab->W, height = lab->H;
        float minQ = 10000.f;
        float maxQ = -1000.f;
        double Yw = 1.0;
        double Xw, Zw;
        float f = 0.f, nc = 0.f, la, c = 0.f, xw, yw, zw, f2 = 1.f, c2 = 1.f, nc2 = 1.f, yb2;
        float fl, n, nbb, ncb, aw; //d
        float xwd, ywd, zwd, xws, yws, zws;
        int alg = 0;
        bool algepd = false;
        double Xwout, Zwout;
        double Xwsc, Zwsc;

        const bool epdEnabled = params->epd.enabled;
        bool ciedata = (params->colorappearance.datacie && pW != 1) && !((params->colorappearance.tonecie && (epdEnabled)) || (params->sharpening.enabled && settings->autocielab && execsharp)
                       || (params->dirpyrequalizer.enabled && settings->autocielab) || (params->defringe.enabled && settings->autocielab)  || (params->sharpenMicro.enabled && settings->autocielab)
                       || (params->impulseDenoise.enabled && settings->autocielab) || (params->colorappearance.badpixsl > 0 && settings->autocielab));

        ColorTemp::temp2mulxyz(params->wb.temperature, params->wb.method, params->wb.observer, Xw, Zw);  //compute white Xw Yw Zw  : white current WB
        ColorTemp::temp2mulxyz(params->colorappearance.tempout, "Custom", params->wb.observer, Xwout, Zwout);//compute white Xw Yw Zw viewing : white current WB
        ColorTemp::temp2mulxyz(params->colorappearance.tempsc, "Custom", params->wb.observer, Xwsc, Zwsc);//compute white Xw Yw Zw source : white current WB

        //viewing condition for surrsrc - Surround
        if (params->colorappearance.surrsrc == "Average") {
            f  = 1.00f;
            c  = 0.69f;
            nc = 1.00f;
        } else if (params->colorappearance.surrsrc == "Dim") {
            f  = 0.9f;
            c  = 0.59f;
            nc = 0.9f;
        } else if (params->colorappearance.surrsrc == "Dark") {
            f  = 0.8f;
            c  = 0.525f;
            nc = 0.8f;
        } else if (params->colorappearance.surrsrc == "ExtremelyDark") {
            f  = 0.8f;
            c  = 0.41f;
            nc = 0.8f;
        }


        //viewing condition for surround Viewing
        if (params->colorappearance.surround == "Average") {
            f2  = 1.0f;
            c2  = 0.69f;
            nc2 = 1.0f;
        } else if (params->colorappearance.surround == "Dim") {
            f2  = 0.9f;
            c2  = 0.59f;
            nc2 = 0.9f;
        } else if (params->colorappearance.surround == "Dark") {
            f2  = 0.8f;
            c2  = 0.525f;
            nc2 = 0.8f;
        } else if (params->colorappearance.surround == "ExtremelyDark") {
            f2  = 0.8f;
            c2  = 0.41f;
            nc2 = 0.8f;
        }

        /*
                //scene condition for surround
                if (params->colorappearance.surrsource)  {
                    f  = 0.85f;    // if user => source image has surround very dark
                    c  = 0.55f;
                    nc = 0.85f;
                }
        */
        //with which algorithm  JC, JS, QM, ALL
        if (params->colorappearance.algo == "JC") {
            alg = 0;
        } else if (params->colorappearance.algo == "JS") {
            alg = 1;
        } else if (params->colorappearance.algo == "QM")  {
            alg = 2;
            algepd = true;
        } else { /*if(params->colorappearance.algo == "ALL")*/
            alg = 3;
            algepd = true;
        }

        xwd = 100.0 * Xwout;
        zwd = 100.0 * Zwout;
        ywd = 100.0 / params->colorappearance.greenout;//approximation to simplify

        xws = 100.0 * Xwsc;
        zws = 100.0 * Zwsc;
        yws = 100.0 / params->colorappearance.greensc;//approximation to simplify


        /*
                //settings white point of output device - or illuminant viewing
                if (settings->viewingdevice == 0) {
                    xwd = 96.42f;    //5000K
                    ywd = 100.0f;
                    zwd = 82.52f;
                } else if (settings->viewingdevice == 1) {
                    xwd = 95.68f;    //5500
                    ywd = 100.0f;
                    zwd = 92.15f;
                } else if (settings->viewingdevice == 2) {
                    xwd = 95.24f;    //6000
                    ywd = 100.0f;
                    zwd = 100.81f;
                } else if (settings->viewingdevice == 3)  {
                    xwd = 95.04f;    //6500
                    ywd = 100.0f;
                    zwd = 108.88f;
                } else if (settings->viewingdevice == 4)  {
                    xwd = 109.85f;    //tungsten
                    ywd = 100.0f;
                    zwd = 35.58f;
                } else if (settings->viewingdevice == 5)  {
                    xwd = 99.18f;    //fluo F2
                    ywd = 100.0f;
                    zwd = 67.39f;
                } else if (settings->viewingdevice == 6)  {
                    xwd = 95.04f;    //fluo F7
                    ywd = 100.0f;
                    zwd = 108.75f;
                } else {
                    xwd = 100.96f;    //fluo F11
                    ywd = 100.0f;
                    zwd = 64.35f;
                }
        */
        yb2 = params->colorappearance.ybout;
        /*
                //settings mean Luminance Y of output device or viewing
                if (settings->viewingdevicegrey == 0) {
                    yb2 = 5.0f;
                } else if (settings->viewingdevicegrey == 1) {
                    yb2 = 10.0f;
                } else if (settings->viewingdevicegrey == 2) {
                    yb2 = 15.0f;
                } else if (settings->viewingdevicegrey == 3) {
                    yb2 = 18.0f;
                } else if (settings->viewingdevicegrey == 4) {
                    yb2 = 23.0f;
                } else if (settings->viewingdevicegrey == 5)  {
                    yb2 = 30.0f;
                } else {
                    yb2 = 40.0f;
                }
        */
        //La and la2 = ambiant luminosity scene and viewing
        la = float (params->colorappearance.adapscen);

        if (pwb == 2) {
            if (params->colorappearance.autoadapscen) {
                la = adap;
            }
        }

        /*
                if (alg >= 2 && la < 200.f) {
                    la = 200.f;
                }
        */
        const float la2 = float (params->colorappearance.adaplum);

        // level of chromatic adaptation scene
        const float deg = (params->colorappearance.degree) / 100.0f;
        const float pilot = params->colorappearance.autodegree ? 2.0f : deg;

        // level of chromatic adaptation viewing
        const float degout = (params->colorappearance.degreeout) / 100.0f;
        const float pilotout = params->colorappearance.autodegreeout ? 2.0f : degout;

        //algoritm's params
        float chr = 0.f;
        //Various adaptation between chroma, saturation, colorfulness
        if (alg == 0 || alg == 3) {
            chr = params->colorappearance.chroma;

            if (chr == -100.0f) {
                chr = -99.8f;
            }
        }

        float schr = 0.f;

        if (alg == 3 || alg == 1) {
            schr = params->colorappearance.schroma;

            if (schr > 0.f) {
                schr = schr / 2.f;    //divide sensibility for saturation
            }

            if (alg == 3) {
                if (schr == -100.f) {
                    schr = -99.f;
                }

                if (schr == 100.f) {
                    schr = 98.f;
                }
            } else {
                if (schr == -100.f) {
                    schr = -99.8f;
                }
            }
        }

        float mchr = 0.f;

        if (alg == 3 || alg == 2) {
            mchr = params->colorappearance.mchroma;

            if (mchr == -100.0f) {
                mchr = -99.8f ;
            }

            if (mchr == 100.0f) {
                mchr = 99.9f;
            }
        }

        const float hue = params->colorappearance.colorh;
        const float huered = params->colorappearance.colorhred;
        const float schrred = params->colorappearance.schromared;
        const float huegreen = params->colorappearance.colorhgreen;
        const float schrgreen = params->colorappearance.schromagreen;
        const float hueblue = params->colorappearance.colorhblue;
        const float schrblue = params->colorappearance.schromablue;
        const float brighthres = 10.f;

        const float rstprotection = 100. - params->colorappearance.rstprotection;

        // extracting data from 'params' to avoid cache flush (to be confirmed)
        const ColorAppearanceParams::TcMode curveMode = params->colorappearance.curveMode;
        const bool hasColCurve1 = bool (customColCurve1);
        const bool t1L = hasColCurve1 && curveMode == ColorAppearanceParams::TcMode::LIGHT;
        const bool hasColCurvered = bool (customColCurvered);
        const bool hasColCurvegreen = bool (customColCurvegreen);
        const bool hasColCurveblue = bool (customColCurveblue);

        const ColorAppearanceParams::TcMode curveMode2 = params->colorappearance.curveMode2;
        const bool hasColCurve2 = bool (customColCurve2);

        const ColorAppearanceParams::CtcMode curveMode3 = params->colorappearance.curveMode3;
        const bool hasColCurve3 = bool (customColCurve3);

        bool needJ = (alg == 0 || alg == 1 || alg == 3);
        bool needQ = (alg == 2 || alg == 3);
        LUTu hist16J;//histogram J
        LUTu hist16Q;;//histogram Q

        if ((needJ && CAMBrightCurveJ.dirty) || (needQ && CAMBrightCurveQ.dirty) || (std::isnan(mean) && settings->viewinggreySc != 0)) {

            if (needJ) {
                hist16J(32768);
                hist16J.clear();
            }

            if (needQ) {
                hist16Q(32768);
                hist16Q.clear();
            }

            double sum = 0.0; // use double precision for large summations

#ifdef _OPENMP
            const int numThreads = min(max(width * height / 65536, 1), omp_get_max_threads());
            #pragma omp parallel num_threads(numThreads) if(numThreads>1)
#endif
            {
                LUTu hist16Jthr;
                LUTu hist16Qthr;

                if (needJ) {
                    hist16Jthr(hist16J.getSize());
                    hist16Jthr.clear();
                }

                if (needQ) {
                    hist16Qthr(hist16Q.getSize());
                    hist16Qthr.clear();
                }

#ifdef _OPENMP
                #pragma omp for reduction(+:sum)
#endif
//rough correspondence between L and J - It's an approximation, but at this stage I haven't found anything simple.

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

                        if (needJ) {
                            hist16Jthr[(int)((koef * lab->L[i][j]))]++;    //evaluate histogram luminance L # J
                        }

                        //estimation of wh only with La
                        /*
                           float whestim = 500.f;

                           if (la < 200.f) {
                           whestim = 200.f;
                           } else if (la < 2500.f) {
                           whestim = 350.f;
                           } else {
                           whestim = 500.f;
                           }
                        */
                        if (needQ) {
                            hist16Qthr[CLIP((int)(32768.f * sqrt((koef * (lab->L[i][j])) / 32768.f)))]++;     //for brightness Q : approximation for Q=wh*sqrt(J/100)  J not equal L
                            //perhaps  needs to introduce whestim ??
                            //hist16Qthr[ (int) (sqrtf ((koef * (lab->L[i][j])) * 32768.f))]++;  //for brightness Q : approximation for Q=wh*sqrt(J/100)  J not equal L
                        }

                        sum += static_cast<double>(koef) * static_cast<double>(lab->L[i][j]); //evaluate mean J to calculate Yb
                        //sumQ += whestim * sqrt ((koef * (lab->L[i][j])) / 32768.f);
                        //can be used in case of...
                    }
                }

#ifdef _OPENMP
                #pragma omp critical
#endif
                {
                    if (needJ) {
                        hist16J += hist16Jthr;
                    }

                    if (needQ) {
                        hist16Q += hist16Qthr;
                    }

                }

                if (std::isnan(mean)) {
                    mean = (sum / ((height) * width)) / 327.68; //for Yb  for all image...if one day "pipette" we can adapt Yb for each zone
                }
            }
#ifdef _OPENMP
            static_cast<void>(numThreads); // to silence cppcheck warning
#endif

            //evaluate lightness, contrast
        }



        if (params->colorappearance.autoybscen  &&  pwb == 2) {//auto
            //Evaluate mean luminance scene Yb%
            if (mean < 15.f) {
                yb = 3.0f;
            } else if (mean < 30.f) {
                yb = 5.0f;
            } else if (mean < 40.f) {
                yb = 10.0f;
            } else if (mean < 45.f) {
                yb = 15.0f;
            } else if (mean < 50.f) {
                yb = 18.0f;
            } else if (mean < 55.f) {
                yb = 23.0f;
            } else if (mean < 60.f) {
                yb = 30.0f;
            } else if (mean < 70.f) {
                yb = 40.0f;
            } else if (mean < 80.f) {
                yb = 60.0f;
            } else if (mean < 90.f) {
                yb = 80.0f;
            } else {
                yb = 90.0f;
            }

        } else {
            yb = (float) params->colorappearance.ybscen;
        }

        const bool highlight = params->toneCurve.hrenabled; //Get the value if "highlight reconstruction" is activated

        const int gamu = (params->colorappearance.gamut) ? 1 : 0;
        xw = 100.0 * Xw;
        yw = 100.0 * Yw;
        zw = 100.0 * Zw;
        float xw1 = 0.f, yw1 = 0.f, zw1 = 0.f, xw2 = 0.f, yw2 = 0.f, zw2 = 0.f;

        // settings of WB: scene and viewing
        if (params->colorappearance.wbmodel == "RawT") {
            xw1 = 96.46f;    //use RT WB; CAT 02 is used for output device (see prefreneces)
            yw1 = 100.0f;
            zw1 = 82.445f;
            xw2 = xwd;
            yw2 = ywd;
            zw2 = zwd;
        } else if (params->colorappearance.wbmodel == "RawTCAT02") {
            xw1 = xw;    // Settings RT WB are used for CAT02 => mix , CAT02 is use for output device (screen: D50 D65, projector: lamp, LED) see preferences
            yw1 = yw;
            zw1 = zw;
            xw2 = xwd;
            yw2 = ywd;
            zw2 = zwd;
        } else if (params->colorappearance.wbmodel == "free") {
            xw1 = xws;    // free temp and green
            yw1 = yws;
            zw1 = zws;
            xw2 = xwd;
            yw2 = ywd;
            zw2 = zwd;
        }


        float cz, wh, pfl;
        int c16 = 1;
        //choice between CIECAM02 or CIECAM16
        if (params->colorappearance.modelmethod == "02") {
            c16 = 1;
        } else if (params->colorappearance.modelmethod == "16") {
            c16 = 16;
        } //I don't use PQ here...hence no 21

        float plum = 100.f;
        Ciecam02::initcam1float(yb, pilot, f, la, xw, yw, zw, n, d, nbb, ncb, cz, aw, wh, pfl, fl, c, c16, plum);

        const float pow1 = pow_F(1.64f - pow_F(0.29f, n), 0.73f);
        float nj, nbbj, ncbj, czj, awj, flj;
        //initialize CAM for scene
        Ciecam02::initcam2float(yb2, pilotout, f2,  la2,  xw2,  yw2,  zw2, nj, dj, nbbj, ncbj, czj, awj, flj, c16, plum);
#if defined(__SSE2__) || defined(RT_SIMDE)
        const float reccmcz = 1.f / (c2 * czj);
#endif
        const float pow1n = pow_F(1.64f - pow_F(0.29f, nj), 0.73f);
        //initialize conversion coefficient between J and Q, with White point differences
        const float epsil = 0.0001f;
        const float coefQ = 32767.f / wh;//conversion coeeficient taking into account 'achromatic response to white' and absolute luminance
        const float a_w = aw;//achromatic response to white
        const float c_ = c;//surround  background
        const float f_l = fl;//models the increase in brightness and colorfulness
        const float coe = pow_F(fl, 0.25f);
        const float QproFactor = (0.4f / c) * (aw + 4.0f) ;
        const bool LabPassOne = !((params->colorappearance.tonecie && (epdEnabled)) || (params->sharpening.enabled && settings->autocielab && execsharp)
                                  || (params->dirpyrequalizer.enabled && settings->autocielab) || (params->defringe.enabled && settings->autocielab)  || (params->sharpenMicro.enabled && settings->autocielab)
                                  || (params->impulseDenoise.enabled && settings->autocielab) || (params->colorappearance.badpixsl > 0 && settings->autocielab));
        //Sliders J and Q
        if (needJ) {
            if (!CAMBrightCurveJ) {
                CAMBrightCurveJ(32768, LUT_CLIP_ABOVE);
            }

            if (CAMBrightCurveJ.dirty) {
                Ciecam02::curveJfloat(params->colorappearance.jlight, params->colorappearance.contrast, 0.6f, hist16J, CAMBrightCurveJ); //lightness and contrast J
                CAMBrightCurveJ /= 327.68f;
                CAMBrightCurveJ.dirty = false;
            }
        }

        if (needQ) {
            if (!CAMBrightCurveQ) {
                CAMBrightCurveQ(32768, LUT_CLIP_ABOVE);
            }

            if (CAMBrightCurveQ.dirty) {
                Ciecam02::curveJfloat(params->colorappearance.qbright, params->colorappearance.qcontrast, 0.6f, hist16Q, CAMBrightCurveQ); //brightness and contrast Q
                //  CAMBrightCurveQ /= coefQ;
                CAMBrightCurveQ.dirty = false;
            }
        }


        //matrix for current working space
        TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(params->icm.workingProfile);
        const float wip[3][3] = {
            { (float)wiprof[0][0], (float)wiprof[0][1], (float)wiprof[0][2]},
            { (float)wiprof[1][0], (float)wiprof[1][1], (float)wiprof[1][2]},
            { (float)wiprof[2][0], (float)wiprof[2][1], (float)wiprof[2][2]}
        };

#if defined(__SSE2__) || defined(RT_SIMDE)
        int bufferLength = ((width + 3) / 4) * 4; // bufferLength has to be a multiple of 4
#endif
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            float minQThr = 10000.f;
            float maxQThr = -1000.f;
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
                vfloat x, y, z;
                vfloat J, C, h, Q, M, s;

                vfloat c655d35 = F2V(655.35f);

                for (k = 0; k < width - 3; k += 4) {
                    //convert from Lab to XYZ
                    Color::Lab2XYZ(LVFU(lab->L[i][k]), LVFU(lab->a[i][k]), LVFU(lab->b[i][k]), x, y, z);
                    x = x / c655d35;
                    y = y / c655d35;
                    z = z / c655d35;
                    float plum = 100.f;
                    vfloat plumv = F2V(plum);
                    //convert from XYZ to LMS Cat02/16
                    Ciecam02::xyz2jchqms_ciecam02float(J, C,  h,
                                                       Q,  M,  s, F2V(aw), F2V(fl), F2V(wh),
                                                       x,  y,  z,
                                                       F2V(xw1), F2V(yw1),  F2V(zw1),
                                                       F2V(c),  F2V(nc), F2V(pow1), F2V(nbb), F2V(ncb), F2V(pfl), F2V(cz), F2V(d), c16, plumv);
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
                    x = (float)x1 / 655.35f;
                    y = (float)y1 / 655.35f;
                    z = (float)z1 / 655.35f;
                    //process source==> normal
                    Ciecam02::xyz2jchqms_ciecam02float(J, C,  h,
                                                       Q,  M,  s, aw, fl, wh,
                                                       x,  y,  z,
                                                       xw1, yw1,  zw1,
                                                       c,  nc, pow1, nbb, ncb, pfl, cz, d, c16, plum);
#endif
                    float Jpro, Cpro, hpro, Qpro, Mpro, spro;
                    //now we have the CIECAM variables pixel by pixel
                    Jpro = J;
                    Cpro = C;
                    hpro = h;
                    Qpro = Q;
                    Mpro = M;
                    spro = s;
                    bool jp = false;
                    bool jpred = false;
                    bool jpgreen = false;
                    bool jpblue = false;
                    //the order in which each treatment is processed is very important. Otherwise, the variables are not up to date
                    //first main Curves Q
                    if ((hasColCurve1) && (curveMode == ColorAppearanceParams::TcMode::BRIGHT)) {
                        jp = true;
                        float Qq = Qpro * coefQ;
                        float Qold = Qpro;
                        const Brightcurve& userColCurveB1 = static_cast<const Brightcurve&>(customColCurve1);
                        userColCurveB1.Apply(Qq);
                        Qq = Qq / coefQ;
                        Qpro = 0.2f * (Qq - Qold) + Qold;
                    }

                    if ((hasColCurve2) && (curveMode2 == ColorAppearanceParams::TcMode::BRIGHT)) {
                        jp = true;
                        float Qq2 = Qpro * coefQ;
                        float Qold2 = Qpro;
                        const Brightcurve& userColCurveB2 = static_cast<const Brightcurve&>(customColCurve2);
                        userColCurveB2.Apply(Qq2);
                        Qq2 = Qq2 / coefQ;
                        Qpro = 0.2f * (Qq2 - Qold2) + Qold2;
                    }

                    if (jp) {
                        Jpro = SQR((10.f * Qpro) / wh);
                    }
                    //Red Green Blue variations
                    //I haven't addressed the transitions or spillovers during other hue modifications, to keep things simple... The effects are marginal and minor.
                    //It's possible to set a Hue equalizer for each color range (Red, Green, Blue), which is more purist but complicates the interface and usage.
                    constexpr float attenuation_hue = 0.555f;//Hue attenuation factor of 100/180. I find the changes too significant. It's just a convention.

                    // Creates a transition to mitigate the effects of "brightness" (red green blue) curves on artifacts
                    constexpr float klimb = 0.5f;//ponderation
                    const float limb = brighthres + klimb * (100.f - brighthres);//intermediate zone where the application of the curve is progressive
                    constexpr float mink = 0.01f;//minimum curve factor
                    const float kam = (1.f - mink) / (limb - brighthres);//linear interpolation
                    const float kbm = mink - kam * brighthres;//linear interpolation
                    if((hpro > 340.f && hpro <= 360.f) || (hpro > 0.f && hpro <= 100.f)) {//Red CIECAM
                        if ((hasColCurvered)) {
                            jpred = true;
                            float Qq = Qpro * coefQ;
                            float Qold = Qpro;
                            const Brightcurve& userColCurveBred = static_cast<const Brightcurve&>(customColCurvered);
                            const float kreduc = Cpro * kam + kbm;//calculates the reduction in the applied force of the curve as a function of Cpro
                            if (Cpro  > brighthres) {
                                userColCurveBred.Apply(Qq);
                            }
                            Qq = Qq / coefQ;
                            Qpro = 0.2f * kreduc * (Qq - Qold) + Qold;
                        }
                        if (jpred) {
                            Jpro = SQR((10.f * Qpro) / wh);
                        }

                    
                        hpro = hpro + attenuation_hue * huered;//rotation Red
                        spro = spro * (1.f + (schrred / 100.f));//change Red saturation 
                        float Cp = (spro * spro * Qpro) / (1000000.f);//recalculate Chroma
                        Mpro = SQR(spro) * Qpro / 10000.f;//recalculate Mpro Colorfulness
                        Cpro = Cp * 100.f;


                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                    }

                    if((hpro > 100.f && hpro <= 190)) {//Green CIECAM 
                        if ((hasColCurvegreen)) {
                            jpgreen = true;
                            float Qq = Qpro * coefQ;
                            float Qold = Qpro;
                            const Brightcurve& userColCurveBgreen = static_cast<const Brightcurve&>(customColCurvegreen);
                            if (Cpro  > brighthres) {
                                userColCurveBgreen.Apply(Qq);
                            }
                            const float kreduc = Cpro * kam + kbm;

                            Qq = Qq / coefQ;
                            Qpro = 0.2f * kreduc * (Qq - Qold) + Qold;
                        }
                        if (jpgreen) {
                            Jpro = SQR((10.f * Qpro) / wh);
                        }
                    
                        hpro = hpro + attenuation_hue * huegreen;//Rotation Green
                        spro = spro * (1.f + (schrgreen / 100.f));//change Green saturation 
                        float Cp = (spro * spro * Qpro) / (1000000.f);//Evaluate Chroma with Brightness Q and saturation
                        Mpro = SQR(spro) * Qpro / 10000.f;//recalculate Mpro Colorfulness
                        Cpro = Cp * 100.f;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                    }

                    if((hpro > 190.f && hpro <= 340)) {//blue CIECAM
                        if ((hasColCurveblue)) {
                            jpblue = true;
                            float Qq = Qpro * coefQ;
                            float Qold = Qpro;
                            const Brightcurve& userColCurveBblue = static_cast<const Brightcurve&>(customColCurveblue);
                            if (Cpro  > brighthres) {
                                userColCurveBblue.Apply(Qq);
                            }
                            const float kreduc = Cpro * kam + kbm;

                            Qq = Qq / coefQ;
                            Qpro = 0.2f * kreduc * (Qq - Qold) + Qold;
                        }
                        if (jpblue) {
                            Jpro = SQR((10.f * Qpro) / wh);
                        }

                        hpro = hpro + attenuation_hue * hueblue;//Rotation Blue
                        spro = spro * (1.f + (schrblue / 100.f));//change Blue saturation 
                        float Cp = (spro * spro * Qpro) / (1000000.f);
                        Mpro = SQR(spro) * Qpro / 10000.f;//recalculate Mpro Colorfulness
                        Cpro = Cp * 100.f;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                    }
                    //We could have introduced Yellow, more in line with the CIECAM concept...but that would have confused (I think) users 'used' to R, G, B
                    //red CIECAM 340 - 100
                    //green CIECAM 100 - 190
                    //blue CIECAM 190 - 340

                    // we cannot have all algorithms with all chroma curves
                    if (alg == 0) {
                        Jpro = CAMBrightCurveJ[Jpro * 327.68f]; //lightness CIECAM02 + contrast
                        Qpro = QproFactor * sqrtf(Jpro);//calculate Q brightness
                        float Cp = (spro * spro * Qpro) / (1000000.f);//Chroma
                        Cpro = Cp * 100.f;
                        float sres;
                        Ciecam02::curvecolorfloat(chr, Cp, sres, 1.8f);//calculate parameters to master Red (here with saturation)
                        Color::skinredfloat(Jpro, hpro, sres, Cp, 55.f, 30.f, 1, rstprotection, 100.f, Cpro);
                        hpro = hpro + hue;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                        
                    } else if (alg == 1)  {
                        // Lightness saturation
                        Jpro = CAMBrightCurveJ[Jpro * 327.68f]; //lightness CIECAM02 + contrast
                        float sres;
                        float Sp = spro / 100.0f;
                        float parsat = 1.5f; //parsat=1.5 =>saturation  ; 1.8 => chroma ; 2.5 => colorfulness (personal evaluation)
                        Ciecam02::curvecolorfloat(schr, Sp, sres, parsat);
                        float dred = 100.f; // in C mode
                        float protect_red = 80.0f; // in C mode
                        dred = 100.0f * sqrtf((dred * coe) / Qpro);
                        protect_red = 100.0f * sqrtf((protect_red * coe) / Qpro);
                        Color::skinredfloat(Jpro, hpro, sres, Sp, dred, protect_red, 0, rstprotection, 100.f, spro);
                        Qpro = QproFactor * sqrtf(Jpro);
                        Cpro = (spro * spro * Qpro) / (10000.0f);
                        hpro = hpro + hue;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                        
                    } else if (alg == 2) {

                        Qpro = CAMBrightCurveQ[(float)(Qpro * coefQ)] / coefQ;//brightness and contrast

                        float Mp, sres;
                        Mp = Mpro / 100.0f;
                        Ciecam02::curvecolorfloat(mchr, Mp, sres, 2.5f);//calculate parameters to master Red (here with colorfulness)
                        float dred = 100.f; //in C mode
                        float protect_red = 80.0f; // in C mode
                        dred *= coe; //in M mode
                        protect_red *= coe; //M mode
                        Color::skinredfloat(Jpro, hpro, sres, Mp, dred, protect_red, 0, rstprotection, 100.f, Mpro);
                        Jpro = SQR((10.f * Qpro) / wh);//recalculate J lightness
                        Cpro = Mpro / coe;
                        Qpro = (Qpro == 0.f ? epsil : Qpro); // avoid division by zero
                        spro = 100.0f * sqrtf(Mpro / Qpro);//recalculate saturation
                        hpro = hpro + hue;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                        
                    } else { /*if(alg == 3) */
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
                        Cpro = Mpro / coe;
                        Qpro = (Qpro == 0.f ? epsil : Qpro); // avoid division by zero
                        spro = 100.0f * sqrtf(Mpro / Qpro);

                        if(settings->autocielab) {//avoid artifacts
                            if (Jpro > 99.9f) {
                                Jpro = 99.9f;
                            }
                        }

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
                        Ciecam02::curvecolorfloat(chr, Cp, sres, 1.8f);
                        Color::skinredfloat(Jpro, hpro, sres, Cp, 55.f, 30.f, 1, rstprotection, 100.f, Cpro);
                        hpro = hpro + hue;

                        if (hpro < 0.0f) {
                            hpro += 360.0f;    //hue
                        }
                    }
                    //Lightness curves
                    if (hasColCurve1 && (curveMode == ColorAppearanceParams::TcMode::LIGHT)) {
                        float Jj = (float) Jpro * 327.68f;
                        float Jold = Jj;
                        float Jold100 = (float) Jpro;
                        float redu = 25.f;
                        float reduc = 1.f;
                        const Lightcurve& userColCurveJ1 = static_cast<const Lightcurve&>(customColCurve1);
                        userColCurveJ1.Apply(Jj);

                        if (Jj > Jold) {
                            if (Jj < 65535.f)  {
                                if (Jold < 327.68f * redu) {
                                    Jj = 0.3f * (Jj - Jold) + Jold;    //divide sensibility
                                } else        {
                                    reduc = LIM((100.f - Jold100) / (100.f - redu), 0.f, 1.f);
                                    Jj = 0.3f * reduc * (Jj - Jold) + Jold; //reduct sensibility in highlights
                                }
                            }
                        } else if (Jj > 10.f) {
                            Jj = 0.8f * (Jj - Jold) + Jold;
                        } else if (Jj >= 0.f) {
                            Jj = 0.90f * (Jj - Jold) + Jold;    // not zero ==>artifacts
                        }

                        Jpro = (float)(Jj / 327.68f);

                        if (Jpro < 1.f) {
                            Jpro = 1.f;
                        }
                    }

                    if (hasColCurve2 && (curveMode2 == ColorAppearanceParams::TcMode::LIGHT)) {
                        float Jj = (float) Jpro * 327.68f;
                        float Jold = Jj;
                        float Jold100 = (float) Jpro;
                        float redu = 25.f;
                        float reduc = 1.f;
                        const Lightcurve& userColCurveJ2 = static_cast<const Lightcurve&>(customColCurve2);
                        userColCurveJ2.Apply(Jj);

                        if (Jj > Jold) {
                            if (Jj < 65535.f)  {
                                if (Jold < 327.68f * redu) {
                                    Jj = 0.3f * (Jj - Jold) + Jold;    //divide sensibility
                                } else        {
                                    reduc = LIM((100.f - Jold100) / (100.f - redu), 0.f, 1.f);
                                    Jj = 0.3f * reduc * (Jj - Jold) + Jold; //reduct sensibility in highlights
                                }
                            }
                        } else if (Jj > 10.f) {
                            if (!t1L) {
                                Jj = 0.8f * (Jj - Jold) + Jold;
                            } else {
                                Jj = 0.4f * (Jj - Jold) + Jold;
                            }
                        } else if (Jj >= 0.f) {
                            if (!t1L) {
                                Jj = 0.90f * (Jj - Jold) + Jold;    // not zero ==>artifacts
                            } else {
                                Jj = 0.5f * (Jj - Jold) + Jold;
                            }
                        }

                        Jpro = (float)(Jj / 327.68f);

                        if (Jpro < 1.f) {
                            Jpro = 1.f;
                        }
                    }
                    //curves chroma, saturation, colorfulness
                    if (hasColCurve3) {//curve 3 with chroma saturation colorfulness
                        if (curveMode3 == ColorAppearanceParams::CtcMode::CHROMA) {
                            float parsat = 0.8f; //0.68;
                            float coef = 327.68f / parsat;
                            float Cc = (float) Cpro * coef;
                            float Ccold = Cc;
                            const Chromacurve& userColCurve = static_cast<const Chromacurve&>(customColCurve3);
                            userColCurve.Apply(Cc);
                            float dred = 55.f;
                            float protect_red = 30.0f;
                            int sk = 1;
                            float ko = 1.f / coef;
                            Color::skinredfloat(Jpro, hpro, Cc, Ccold, dred, protect_red, sk, rstprotection, ko, Cpro);
                        } else if (curveMode3 == ColorAppearanceParams::CtcMode::SATUR) {
                            float parsat = 0.8f; //0.6
                            float coef = 327.68f / parsat;
                            float Ss = (float) spro * coef;
                            float Sold = Ss;
                            const Saturcurve& userColCurve = static_cast<const Saturcurve&>(customColCurve3);
                            userColCurve.Apply(Ss);
                            Ss = 0.6f * (Ss - Sold) + Sold; //divide sensibility saturation
                            float dred = 100.f; // in C mode
                            float protect_red = 80.0f; // in C mode
                            dred = 100.0f * sqrtf((dred * coe) / Qpro);
                            protect_red = 100.0f * sqrtf((protect_red * coe) / Qpro);
                            int sk = 0;
                            float ko = 1.f / coef;
                            Color::skinredfloat(Jpro, hpro, Ss, Sold, dred, protect_red, sk, rstprotection, ko, spro);
                            Qpro = (4.0f / c) * sqrtf(Jpro / 100.0f) * (aw + 4.0f) ;
                            Cpro = (spro * spro * Qpro) / (10000.0f);
                        } else if (curveMode3 == ColorAppearanceParams::CtcMode::COLORF) { //
                            float parsat = 0.8f; //0.68;
                            float coef = 327.68f / parsat;
                            float Mm = (float) Mpro * coef;
                            float Mold = Mm;
                            const Colorfcurve& userColCurve = static_cast<const Colorfcurve&>(customColCurve3);
                            userColCurve.Apply(Mm);
                            float dred = 100.f; //in C mode
                            float protect_red = 80.0f; // in C mode
                            dred *= coe; //in M mode
                            protect_red *= coe;
                            int sk = 0;
                            float ko = 1.f / coef;
                            Color::skinredfloat(Jpro, hpro, Mm, Mold, dred, protect_red, sk, rstprotection, ko, Mpro);
                            Cpro = Mpro / coe;
                        }
                    }

                    //to retrieve the correct values of variables


                    //retrieve values C,J...s
                    C = Cpro;
                    J = Jpro;
                    Q = Qpro;
                    M = Mpro;
                    h = hpro;
                    s = spro;

                    if (params->colorappearance.tonecie  || settings->autocielab) { //use pointer for tonemapping with CIECAM and also sharpening , defringe, contrast detail
                        ncie->Q_p[i][j] = (float)Q + epsil; //epsil to avoid Q=0
                        ncie->M_p[i][j] = (float)M + epsil;
                        ncie->J_p[i][j] = (float)J + epsil;
                        ncie->h_p[i][j] = (float)h;
                        ncie->C_p[i][j] = (float)C + epsil;
                        ncie->sh_p[i][j] = (float) 3276.8f * (sqrtf(J)) ;

                        if (epdEnabled) {
                            if (ncie->Q_p[i][j] < minQThr) {
                                minQThr = ncie->Q_p[i][j];    //minima
                            }

                            if (ncie->Q_p[i][j] > maxQThr) {
                                maxQThr = ncie->Q_p[i][j];    //maxima
                            }
                        }
                    }

                    if (!params->colorappearance.tonecie  || !settings->autocielab || !epdEnabled) {

                        if (ciedata) { //only with improccoordinator
                            // Data for J Q M s and C histograms
                            int posl, posc;
                            float brli;
                            float chsacol;
                            float libr;
                            float colch;

                            //update histogram
                            if (curveMode == ColorAppearanceParams::TcMode::BRIGHT) {
                                brli = 70.0f;
                                libr = Q;     //40.0 to 100.0 approximative factor for Q  - 327 for J
                            } else { /*if(curveMode == ColorAppearanceParams::TCMode::LIGHT)*/
                                brli = 327.f;
                                libr = J;    //327 for J
                            }

                            posl = (int)(libr * brli);
                            hist16JCAM[posl]++;

                            if (curveMode3 == ColorAppearanceParams::CtcMode::CHROMA) {
                                chsacol = 400.f;
                                colch = C;    //450.0 approximative factor for s    320 for M
                            } else if (curveMode3 == ColorAppearanceParams::CtcMode::SATUR) {
                                chsacol = 450.0f;
                                colch = s;
                            } else { /*if(curveMode3 == ColorAppearanceParams::CTCMode::COLORF)*/
                                chsacol = 400.0f;
                                colch = M;
                            }

                            posc = (int)(colch * chsacol);
                            hist16_CCAM[posc]++;

                        }

                        if (LabPassOne) {
#if defined(__SSE2__) || defined(RT_SIMDE)
                            // write to line buffers
                            Jbuffer[j] = J;
                            Cbuffer[j] = C;
                            hbuffer[j] = h;
#else
                            float xx, yy, zz;
                            //process normal==> viewing
                            TMatrix wprofc = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);
                            const double wpc[3][3] = {//improve precision with double
                                {wprofc[0][0], wprofc[0][1], wprofc[0][2]},
                                {wprofc[1][0], wprofc[1][1], wprofc[1][2]},
                                {wprofc[2][0], wprofc[2][1], wprofc[2][2]}
                            };
                            //conversion from CAM to XYZ
                            Ciecam02::jch2xyz_ciecam02float(xx, yy, zz,
                                                            J,  C, h,
                                                            xw2, yw2,  zw2,
                                                            c2, nc2, pow1n, nbbj, ncbj, flj, czj, dj, awj, c16, plum);
                            float x, y, z;
                            x = xx * 655.35f;
                            y = yy * 655.35f;
                            z = zz * 655.35f;
                            float Ll, aa, bb;

                            //convert xyz=>lab
                            if (gamu == 1) {
                                Color::gamutmap(x, y, z, wpc);
                            }

                            Color::XYZ2Lab(x,  y,  z, Ll, aa, bb);
                            lab->L[i][j] = Ll;
                            lab->a[i][j] = aa;
                            lab->b[i][j] = bb;

#endif
                        }
                    }
                }

#if defined(__SSE2__) || defined(RT_SIMDE)
                // process line buffers
                float *xbuffer = Qbuffer;
                float *ybuffer = Mbuffer;
                float *zbuffer = sbuffer;

                for (k = 0; k < bufferLength; k += 4) {
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
                    Color::XYZ2Lab(xbuffer[j], ybuffer[j], zbuffer[j], Ll, aa, bb);

                    // gamut control in Lab mode; I must study how to do with cIECAM only
                    if (gamu == 1) {//disabled by default - old algorithm
                        float Lprov1, Chprov1;
                        Lprov1 = Ll / 327.68f;
                        Chprov1 = sqrtf(SQR(aa) + SQR(bb)) / 327.68f;
                        float2  sincosval;

                        if (Chprov1 == 0.0f) {
                            sincosval.y = 1.f;
                            sincosval.x = 0.0f;
                        } else {
                            sincosval.y = aa / (Chprov1 * 327.68f);
                            sincosval.x = bb / (Chprov1 * 327.68f);
                        }

                        //gamut control : Lab values are in gamut
                        Color::gamutLchonly(sincosval, Lprov1, Chprov1, wip, highlight, 0.15f, 0.96f);
                        lab->L[i][j] = Lprov1 * 327.68f;
                        lab->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                        lab->b[i][j] = 327.68f * Chprov1 * sincosval.x;
                    } else {
                        lab->L[i][j] = Ll;
                        lab->a[i][j] = aa;
                        lab->b[i][j] = bb;
                    }
                }

#endif
            }

#ifdef _OPENMP
            #pragma omp critical
#endif
            {
                if (minQThr < minQ) {
                    minQ = minQThr;
                }

                if (maxQThr > maxQ) {
                    maxQ = maxQThr;
                }
            }
        }

        // End of parallelization
        if (!params->colorappearance.tonecie || !settings->autocielab) { //normal

            if (ciedata) {
                //update histogram J
                hist16JCAM.compressTo(histLCAM);
                //update histogram C
                hist16_CCAM.compressTo(histCCAM);
            }
        }

        if (settings->autocielab) {
            if ((params->colorappearance.tonecie && (epdEnabled)) || (params->sharpening.enabled && settings->autocielab && execsharp)
                    || (params->dirpyrequalizer.enabled && settings->autocielab) || (params->defringe.enabled && settings->autocielab)  || (params->sharpenMicro.enabled && settings->autocielab)
                    || (params->impulseDenoise.enabled && settings->autocielab) || (params->colorappearance.badpixsl > 0 && settings->autocielab)) {



//all this treatments reduce artifacts, but can lead to slightly different results

                if (params->defringe.enabled)
                    if (execsharp) {
                        lab->deleteLab();
                        defringecam(ncie); //defringe adapted to CIECAM
                        lab->reallocLab();
                    }

                if (params->dirpyrequalizer.enabled && params->dirpyrequalizer.gamutlab && rtt) { //remove artifacts by gaussian blur - skin control, but not for thumbs
                    constexpr float artifact = 4.f;
                    constexpr float chrom = 50.f;
                    const bool hotbad = params->dirpyrequalizer.skinprotect != 0.0;

                    lab->deleteLab();
                    badpixcam(ncie, artifact / scale, 5, 2, chrom, hotbad);   //enabled remove artifacts for cbDL
                    lab->reallocLab();
                }

                if (params->colorappearance.badpixsl > 0 && execsharp) {
                    int mode = params->colorappearance.badpixsl;
                    lab->deleteLab();
                    badpixcam(ncie, 3.0, 10, mode, 0, true); //for bad pixels CIECAM
                    lab->reallocLab();
                }

                if (params->impulseDenoise.enabled) if (execsharp) {
                        float **buffers[3];
                        buffers[0] = lab->L;
                        buffers[1] = lab->a;
                        buffers[2] = lab->b;
                        impulsedenoisecam(ncie, buffers);  //impulse adapted to CIECAM
                    }

                if (params->sharpenMicro.enabled)if (execsharp) {
                        MLmicrocontrastcam(ncie);
                    }

                if (params->sharpening.enabled)
                    if (execsharp) {
                        float **buffer = lab->L; // We can use the L-buffer from lab as buffer to save some memory
                        sharpeningcam(ncie, buffer, showSharpMask);  // sharpening adapted to CIECAM
                    }

                if (params->dirpyrequalizer.enabled)  {

                    if (rtt == 1) {
                        float b_l = static_cast<float>(params->dirpyrequalizer.hueskin.getBottomLeft()) / 100.0f;
                        float t_l = static_cast<float>(params->dirpyrequalizer.hueskin.getTopLeft()) / 100.0f;
                        float t_r = static_cast<float>(params->dirpyrequalizer.hueskin.getTopRight()) / 100.0f;
                        lab->deleteLab();
                        dirpyr_equalizercam(ncie, ncie->sh_p, ncie->sh_p, ncie->W, ncie->H, ncie->h_p, ncie->C_p, params->dirpyrequalizer.mult, params->dirpyrequalizer.threshold, params->dirpyrequalizer.skinprotect, b_l, t_l, t_r, scale);  //contrast by detail adapted to CIECAM
                        lab->reallocLab();
                    }

                }

                const float Qredi = (4.0f / c_)  * (a_w + 4.0f);
                const float co_e = (pow_F(f_l, 0.25f));


#ifdef _OPENMP
                #pragma omp parallel
#endif
                {
#ifdef _OPENMP
                    #pragma omp for schedule(dynamic, 10)
#endif

                    for (int i = 0; i < height; i++) // update CieImages with new values after sharpening, defringe, contrast by detail level
                        for (int j = 0; j < width; j++) {
                            float interm = fabsf(ncie->sh_p[i][j] / (32768.f));
                            ncie->J_p[i][j] = 100.0f * SQR(interm);
                            ncie->Q_p[i][j] = interm * Qredi;
                            ncie->M_p[i][j] = ncie->C_p[i][j] * co_e;
                        }
                }
            }
        }

        if ((params->colorappearance.tonecie && (epdEnabled)) || (params->sharpening.enabled && settings->autocielab && execsharp)
                || (params->dirpyrequalizer.enabled && settings->autocielab) || (params->defringe.enabled && settings->autocielab)  || (params->sharpenMicro.enabled && settings->autocielab)
                || (params->impulseDenoise.enabled && settings->autocielab) || (params->colorappearance.badpixsl > 0 && settings->autocielab)) {

            ciedata = (params->colorappearance.datacie && pW != 1);

            if (epdEnabled  && params->colorappearance.tonecie && algepd) {
                lab->deleteLab();
                EPDToneMapCIE(ncie, a_w, c_, width, height, minQ, maxQ, Iterates, scale);
                lab->reallocLab();
            }


            constexpr float eps = 0.0001f;
            const float co_e = (pow_F(f_l, 0.25f)) + eps;

#ifdef _OPENMP
            #pragma omp parallel
#endif
            {
#if defined(__SSE2__) || defined(RT_SIMDE)
                // one line buffer per channel
                float Jbuffer[bufferLength] ALIGNED16;
                float Cbuffer[bufferLength] ALIGNED16;
                float hbuffer[bufferLength] ALIGNED16;
                float *xbuffer = Jbuffer; // we can use one of the above buffers
                float *ybuffer = Cbuffer; //             "
                float *zbuffer = hbuffer; //             "
#endif

#ifdef _OPENMP
                #pragma omp for schedule(dynamic, 10)
#endif

                for (int i = 0; i < height; i++) { // update CIECAM with new values after tone-mapping
                    for (int j = 0; j < width; j++) {

                        if (epdEnabled) {
                            ncie->J_p[i][j] = (100.0f * ncie->Q_p[i][j] * ncie->Q_p[i][j]) / SQR((4.f / c) * (aw + 4.f));
                        }

                        const float ncie_C_p = (ncie->M_p[i][j]) / co_e;

                        //show histogram in CIECAM mode (Q,J, M,s,C)
                        if (ciedata) {
                            // Data for J Q M s and C histograms
                            int posl, posc;
                            float brli = 327.f;
                            float chsacol = 327.f;
                            float libr;
                            float colch;

                            if (curveMode == ColorAppearanceParams::TcMode::BRIGHT) {
                                brli = 70.0f;
                                libr = ncie->Q_p[i][j];    //40.0 to 100.0 approximative factor for Q  - 327 for J
                            } else { /*if(curveMode == ColorAppearanceParams::TCMode::LIGHT)*/
                                brli = 327.f;
                                libr = ncie->J_p[i][j];    //327 for J
                            }

                            posl = (int)(libr * brli);
                            hist16JCAM[posl]++;

                            if (curveMode3 == ColorAppearanceParams::CtcMode::CHROMA) {
                                chsacol = 400.f;//327.f;
                                colch = ncie_C_p;
                            } else if (curveMode3 == ColorAppearanceParams::CtcMode::SATUR) {
                                chsacol = 450.0f;
                                colch = 100.f * sqrtf(ncie_C_p / ncie->Q_p[i][j]);
                            } else { /*if(curveMode3 == ColorAppearanceParams::CTCMode::COLORF)*/
                                chsacol = 400.f;//327.0f;
                                colch = ncie->M_p[i][j];
                            }

                            posc = (int)(colch * chsacol);
                            hist16_CCAM[posc]++;
                        }

                        //end histograms

#if defined(__SSE2__) || defined(RT_SIMDE)
                        Jbuffer[j] = ncie->J_p[i][j];
                        Cbuffer[j] = ncie_C_p;
                        hbuffer[j] = ncie->h_p[i][j];
#else
                        float xx, yy, zz;
                        Ciecam02::jch2xyz_ciecam02float(xx, yy, zz,
                                                        ncie->J_p[i][j],  ncie_C_p, ncie->h_p[i][j],
                                                        xw2, yw2,  zw2,
                                                        c2, nc2, pow1n, nbbj, ncbj, flj, czj, dj, awj, c16, plum);
                        float x = (float)xx * 655.35f;
                        float y = (float)yy * 655.35f;
                        float z = (float)zz * 655.35f;
                        float Ll, aa, bb;
                        //convert xyz=>lab
                        Color::XYZ2Lab(x,  y,  z, Ll, aa, bb);

                        if (gamu == 1) {//gamut Lab control - disabled by default (old algorithm maintained by compatibility)
                            float Lprov1, Chprov1;
                            Lprov1 = Ll / 327.68f;
                            Chprov1 = sqrtf(SQR(aa) + SQR(bb)) / 327.68f;
                            float2  sincosval;

                            if (Chprov1 == 0.0f) {
                                sincosval.y = 1.f;
                                sincosval.x = 0.0f;
                            } else {
                                sincosval.y = aa / (Chprov1 * 327.68f);
                                sincosval.x = bb / (Chprov1 * 327.68f);
                            }


                            //gamut control : Lab values are in gamut
                            Color::gamutLchonly(sincosval, Lprov1, Chprov1, wip, highlight, 0.15f, 0.96f);

                            lab->L[i][j] = Lprov1 * 327.68f;
                            lab->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                            lab->b[i][j] = 327.68f * Chprov1 * sincosval.x;
                        } else {
                            lab->L[i][j] = Ll;
                            lab->a[i][j] = aa;
                            lab->b[i][j] = bb;
                        }

#endif
                    }

#if defined(__SSE2__) || defined(RT_SIMDE)
                    // process line buffers
                    int k;
                    vfloat x, y, z;
                    vfloat c655d35 = F2V(655.35f);

                    for (k = 0; k < bufferLength; k += 4) {
                        Ciecam02::jch2xyz_ciecam02float(x, y, z,
                                                        LVF(Jbuffer[k]), LVF(Cbuffer[k]), LVF(hbuffer[k]),
                                                        F2V(xw2), F2V(yw2), F2V(zw2),
                                                        F2V(nc2), F2V(pow1n), F2V(nbbj), F2V(ncbj), F2V(flj), F2V(dj), F2V(awj), F2V(reccmcz), c16, F2V(plum));
                        x *= c655d35;
                        y *= c655d35;
                        z *= c655d35;
                        STVF(xbuffer[k], x);
                        STVF(ybuffer[k], y);
                        STVF(zbuffer[k], z);
                    }

                    // XYZ2Lab uses a lookup table. The function behind that lut is a cube root.
                    // SSE can't beat the speed of that lut, so it doesn't make sense to use SSE
                    for (int j = 0; j < width; j++) {
                        float Ll, aa, bb;
                        //convert xyz=>lab
                        Color::XYZ2Lab(xbuffer[j], ybuffer[j], zbuffer[j], Ll, aa, bb);

                        if (gamu == 1) {
                            float Lprov1, Chprov1;
                            Lprov1 = Ll / 327.68f;
                            Chprov1 = sqrtf(SQR(aa) + SQR(bb)) / 327.68f;
                            float2  sincosval;

                            if (Chprov1 == 0.0f) {
                                sincosval.y = 1.f;
                                sincosval.x = 0.0f;
                            } else {
                                sincosval.y = aa / (Chprov1 * 327.68f);
                                sincosval.x = bb / (Chprov1 * 327.68f);
                            }

                            //gamut control : Lab values are in gamut
                            Color::gamutLchonly(sincosval, Lprov1, Chprov1, wip, highlight, 0.15f, 0.96f);
                            lab->L[i][j] = Lprov1 * 327.68f;
                            lab->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                            lab->b[i][j] = 327.68f * Chprov1 * sincosval.x;
                        } else {
                            lab->L[i][j] = Ll;
                            lab->a[i][j] = aa;
                            lab->b[i][j] = bb;
                        }

                    }

#endif // __SSE2__
                }

            } //end parallelization

            //show CIECAM histograms
            if (ciedata) {
                //update histogram J and Q
                //update histogram J
                hist16JCAM.compressTo(histLCAM);

                //update color histogram M,s,C
                hist16_CCAM.compressTo(histCCAM);
            }
        }
    }
}
//end CIECAM

void ImProcFunctions::moyeqt(Imagefloat* working, float &moyS, float &eqty)
{
    BENCHFUN

    const int height = working->getHeight();
    const int width = working->getWidth();
    double moy = 0.0;
    double sqrs = 0.0;

#ifdef _OPENMP
    #pragma omp parallel for reduction(+:moy,sqrs) schedule(dynamic,16)
#endif

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            const double s = Color::rgb2s(CLIP(working->r(i, j)), CLIP(working->g(i, j)), CLIP(working->b(i, j)));
            moy += s;
            sqrs += SQR(s);
        }
    }

    moy /= (height * width);
    sqrs /= (height * width);
    eqty = std::sqrt(std::max(sqrs - SQR(moy), 0.0));
    moyS = moy;
}

static inline void
filmlike_clip_rgb_tone(float *r, float *g, float *b, const float L)
{
    float r_ = *r > L ? L : *r;
    float b_ = *b > L ? L : *b;
    float g_ = b_ + ((r_ - b_) * (*g - *b) / (*r - *b));
    *r = r_;
    *g = g_;
    *b = b_;
}

/*static*/ void
filmlike_clip(float *r, float *g, float *b)
{
    // This is Adobe's hue-stable film-like curve with a diagonal, ie only used for clipping. Can probably be further optimized.
    const float L = 65535.0;

    if (*r >= *g) {
        if (*g > *b) {         // Case 1: r >= g >  b
            filmlike_clip_rgb_tone(r, g, b, L);
        } else if (*b > *r) {  // Case 2: b >  r >= g
            filmlike_clip_rgb_tone(b, r, g, L);
        } else if (*b > *g) {  // Case 3: r >= b >  g
            filmlike_clip_rgb_tone(r, b, g, L);
        } else {               // Case 4: r >= g == b
            *r = *r > L ? L : *r;
            *g = *g > L ? L : *g;
            *b = *g;
        }
    } else {
        if (*r >= *b) {        // Case 5: g >  r >= b
            filmlike_clip_rgb_tone(g, r, b, L);
        } else if (*b > *g) {  // Case 6: b >  g >  r
            filmlike_clip_rgb_tone(b, g, r, L);
        } else {               // Case 7: g >= b >  r
            filmlike_clip_rgb_tone(g, b, r, L);
        }
    }
}

void ImProcFunctions::rgbProc(Imagefloat* working, LabImage* lab, PipetteBuffer *pipetteBuffer, const LUTf& hltonecurve, const LUTf& shtonecurve, const LUTf& tonecurve,
                              int sat, const LUTf& rCurve, const LUTf& gCurve, const LUTf& bCurve, float satLimit, float satLimitOpacity,
                              const ColorGradientCurve& ctColorCurve, const OpacityCurve& ctOpacityCurve, bool opautili, const LUTf& clToningcurve, const LUTf& cl2Toningcurve,
                              const ToneCurve& customToneCurve1, const ToneCurve& customToneCurve2, const ToneCurve& customToneCurvebw1, const ToneCurve& customToneCurvebw2,
                              double &rrm, double &ggm, double &bbm, float &autor, float &autog, float &autob, DCPProfile *dcpProf, const DCPProfileApplyState& asIn,
                              LUTu& histToneCurve, size_t chunkSize, bool measure)
{
    rgbProc(working, lab, pipetteBuffer, hltonecurve, shtonecurve, tonecurve, sat, rCurve, gCurve, bCurve, satLimit, satLimitOpacity, ctColorCurve, ctOpacityCurve, opautili,
            clToningcurve, cl2Toningcurve, customToneCurve1, customToneCurve2,  customToneCurvebw1, customToneCurvebw2, rrm, ggm, bbm, autor, autog, autob,
            params->toneCurve.expcomp, params->toneCurve.hlcompr, params->toneCurve.hlcomprthresh, dcpProf, asIn, histToneCurve, chunkSize, measure);
}

// Process RGB image and convert to LAB space
void ImProcFunctions::rgbProc(Imagefloat* working, LabImage* lab, PipetteBuffer *pipetteBuffer, const LUTf& hltonecurve, const LUTf& shtonecurve, const LUTf& tonecurve,
                              int sat, const LUTf& rCurve, const LUTf& gCurve, const LUTf& bCurve, float satLimit, float satLimitOpacity,
                              const ColorGradientCurve& ctColorCurve, const OpacityCurve& ctOpacityCurve, bool opautili, const LUTf& clToningcurve, const LUTf& cl2Toningcurve,
                              const ToneCurve& customToneCurve1, const ToneCurve& customToneCurve2, const ToneCurve& customToneCurvebw1, const ToneCurve& customToneCurvebw2,
                              double &rrm, double &ggm, double &bbm, float &autor, float &autog, float &autob, double expcomp, int hlcompr, int hlcomprthresh,
                              DCPProfile *dcpProf, const DCPProfileApplyState& asIn, LUTu& histToneCurve, size_t chunkSize, bool measure)
{

    std::unique_ptr<StopWatch> stop;

    if (measure) {
        std::cout << "rgb processing " << working->getWidth() << "x" << working->getHeight() << " image with " << chunkSize << " tiles per thread" << std::endl;
        stop.reset(new StopWatch("rgb processing"));
    }

    const bool split_tiled_parts_1_2 = params->toneEqualizer.enabled;

    std::unique_ptr<Imagefloat> tmpImage;

    Imagefloat* editImgFloat = nullptr;
    PlanarWhateverData<float>* editWhatever = nullptr;
    EditUniqueID editID = pipetteBuffer && pipetteBuffer->bufferCreated() ? pipetteBuffer->getEditID() : EUID_None;

    if (editID != EUID_None) {
        switch (pipetteBuffer->getDataProvider()->getCurrSubscriber()->getPipetteBufferType()) {
            case (BT_IMAGEFLOAT):
                editImgFloat = pipetteBuffer->getImgFloatBuffer();
                break;

            case (BT_LABIMAGE):
                break;

            case (BT_SINGLEPLANE_FLOAT):
                editWhatever = pipetteBuffer->getSinglePlaneBuffer();
                break;
        }
    }

    TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);
    TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(params->icm.workingProfile);

    float toxyz[3][3] = {
        {
            static_cast<float>(wprof[0][0] / static_cast<double>(Color::D50x)),
            static_cast<float>(wprof[0][1] / static_cast<double>(Color::D50x)),
            static_cast<float>(wprof[0][2] / static_cast<double>(Color::D50x))
        }, {
            static_cast<float>(wprof[1][0]),
            static_cast<float>(wprof[1][1]),
            static_cast<float>(wprof[1][2])
        }, {
            static_cast<float>(wprof[2][0] / static_cast<double>(Color::D50z)),
            static_cast<float>(wprof[2][1] / static_cast<double>(Color::D50z)),
            static_cast<float>(wprof[2][2] / static_cast<double>(Color::D50z))
        }
    };
    float maxFactorToxyz = max(toxyz[1][0], toxyz[1][1], toxyz[1][2]);
    float equalR = maxFactorToxyz / toxyz[1][0];
    float equalG = maxFactorToxyz / toxyz[1][1];
    float equalB = maxFactorToxyz / toxyz[1][2];

    //inverse matrix user select
    double wip[3][3] = {
        {wiprof[0][0], wiprof[0][1], wiprof[0][2]},
        {wiprof[1][0], wiprof[1][1], wiprof[1][2]},
        {wiprof[2][0], wiprof[2][1], wiprof[2][2]}
    };

    double wp[3][3] = {
        {wprof[0][0], wprof[0][1], wprof[0][2]},
        {wprof[1][0], wprof[1][1], wprof[1][2]},
        {wprof[2][0], wprof[2][1], wprof[2][2]}
    };

    bool mixchannels = params->chmixer.enabled &&
                       (params->chmixer.red[0] != 1000 || params->chmixer.red[1] != 0     || params->chmixer.red[2] != 0   ||
                        params->chmixer.green[0] != 0 || params->chmixer.green[1] != 1000 || params->chmixer.green[2] != 0 ||
                        params->chmixer.blue[0] != 0  || params->chmixer.blue[1] != 0    || params->chmixer.blue[2] != 1000);

    FlatCurve* hCurve = nullptr;
    FlatCurve* sCurve = nullptr;
    FlatCurve* vCurve = nullptr;
    FlatCurve* bwlCurve = nullptr;

    FlatCurveType hCurveType = (FlatCurveType)params->hsvequalizer.hcurve.at(0);
    FlatCurveType sCurveType = (FlatCurveType)params->hsvequalizer.scurve.at(0);
    FlatCurveType vCurveType = (FlatCurveType)params->hsvequalizer.vcurve.at(0);
    FlatCurveType bwlCurveType = (FlatCurveType)params->blackwhite.luminanceCurve.at(0);
    bool hCurveEnabled = params->hsvequalizer.enabled && hCurveType > FCT_Linear;
    bool sCurveEnabled = params->hsvequalizer.enabled && sCurveType > FCT_Linear;
    bool vCurveEnabled = params->hsvequalizer.enabled && vCurveType > FCT_Linear;
    bool bwlCurveEnabled = bwlCurveType > FCT_Linear;

    // TODO: We should create a 'skip' value like for CurveFactory::complexsgnCurve (rtengine/curves.cc)
    if (hCurveEnabled) {
        hCurve = new FlatCurve(params->hsvequalizer.hcurve);

        if (hCurve->isIdentity()) {
            delete hCurve;
            hCurve = nullptr;
            hCurveEnabled = false;
        }
    }

    if (sCurveEnabled) {
        sCurve = new FlatCurve(params->hsvequalizer.scurve);

        if (sCurve->isIdentity()) {
            delete sCurve;
            sCurve = nullptr;
            sCurveEnabled = false;
        }
    }

    if (vCurveEnabled) {
        vCurve = new FlatCurve(params->hsvequalizer.vcurve);

        if (vCurve->isIdentity()) {
            delete vCurve;
            vCurve = nullptr;
            vCurveEnabled = false;
        }
    }

    if (bwlCurveEnabled) {
        bwlCurve = new FlatCurve(params->blackwhite.luminanceCurve);

        if (bwlCurve->isIdentity()) {
            delete bwlCurve;
            bwlCurve = nullptr;
            bwlCurveEnabled = false;
        }
    }

    std::shared_ptr<CLUT3D> hald_clut;
    bool clutAndWorkingProfilesAreSame = false;
    TMatrix xyz2clut = {}, clut2xyz = {};
#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat v_work2xyz[3][3] ALIGNED16;
    vfloat v_xyz2clut[3][3] ALIGNED16;
    vfloat v_clut2xyz[3][3] ALIGNED16;
    vfloat v_xyz2work[3][3] ALIGNED16;
#endif

    if (params->filmSimulation.enabled && !params->filmSimulation.clutFilename.empty()) {
        hald_clut = CLUTStore::getInstance().getClut(params->filmSimulation.clutFilename);

        if (hald_clut) {
            clutAndWorkingProfilesAreSame = hald_clut->getProfile() == params->icm.workingProfile;

            if (!clutAndWorkingProfilesAreSame) {
                xyz2clut = ICCStore::getInstance()->workingSpaceInverseMatrix(hald_clut->getProfile());
                clut2xyz = ICCStore::getInstance()->workingSpaceMatrix(hald_clut->getProfile());

#if defined(__SSE2__) || defined(RT_SIMDE)

                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        v_work2xyz[i][j] = F2V(wprof[i][j]);
                        v_xyz2clut[i][j] = F2V(xyz2clut[i][j]);
                        v_xyz2work[i][j] = F2V(wiprof[i][j]);
                        v_clut2xyz[i][j] = F2V(clut2xyz[i][j]);
                    }
                }

#endif

            }
        }
    }

    const float film_simulation_strength = static_cast<float>(params->filmSimulation.strength) / 100.0f;

    const float exp_scale = pow(2.0, expcomp);
    const float comp = (max(0.0, expcomp) + 1.0) * hlcompr / 100.0;
    const float shoulder = ((65536.f / max(1.0f, exp_scale)) * (hlcomprthresh / 200.f)) + 0.1f;
    const float hlrange = 65536.f - shoulder;
    const int tone_curve_black = params->toneCurve.black;
    const bool isProPhoto = (params->icm.workingProfile == "ProPhoto");
    // extracting data from 'params' to avoid cache flush (to be confirmed)
    ToneCurveMode curveMode = params->toneCurve.curveMode;
    ToneCurveMode curveMode2 = params->toneCurve.curveMode2;
    bool highlight = params->toneCurve.hrenabled;//Get the value if "highlight reconstruction" is activated
    bool hasToneCurve1 = bool (customToneCurve1);
    bool hasToneCurve2 = bool (customToneCurve2);
    BlackWhiteParams::TcMode beforeCurveMode = params->blackwhite.beforeCurveMode;
    BlackWhiteParams::TcMode afterCurveMode = params->blackwhite.afterCurveMode;

    bool hasToneCurvebw1 = bool (customToneCurvebw1);
    bool hasToneCurvebw2 = bool (customToneCurvebw2);

    PerceptualToneCurveState ptc1ApplyState, ptc2ApplyState;

    if (hasToneCurve1 && curveMode == ToneCurveMode::PERCEPTUAL) {
        const PerceptualToneCurve& userToneCurve = static_cast<const PerceptualToneCurve&>(customToneCurve1);
        userToneCurve.initApplyState(ptc1ApplyState, params->icm.workingProfile);
    }

    if (hasToneCurve2 && curveMode2 == ToneCurveMode::PERCEPTUAL) {
        const PerceptualToneCurve& userToneCurve = static_cast<const PerceptualToneCurve&>(customToneCurve2);
        userToneCurve.initApplyState(ptc2ApplyState, params->icm.workingProfile);
    }

    bool hasColorToning = params->colorToning.enabled && bool (ctOpacityCurve) &&  bool (ctColorCurve) && params->colorToning.method != "LabGrid";
//    bool hasColorToningLabGrid = params->colorToning.enabled && params->colorToning.method == "LabGrid";
    //  float satLimit = float(params->colorToning.satProtectionThreshold)/100.f*0.7f+0.3f;
    //  float satLimitOpacity = 1.f-(float(params->colorToning.saturatedOpacity)/100.f);
    float strProtect = pow_F((float (params->colorToning.strength) / 100.f), 0.4f);

    float RedLow = params->colorToning.redlow / 100.0;
    float GreenLow = params->colorToning.greenlow / 100.0;
    float BlueLow = params->colorToning.bluelow / 100.0;
    float RedMed = params->colorToning.redmed / 100.0;
    float GreenMed = params->colorToning.greenmed / 100.0;
    float BlueMed = params->colorToning.bluemed / 100.0;
    float RedHigh = params->colorToning.redhigh / 100.0;
    float GreenHigh = params->colorToning.greenhigh / 100.0;
    float BlueHigh = params->colorToning.bluehigh / 100.0;
    float SatLow = params->colorToning.shadowsColSat.getBottom() / 100.f;
    float SatHigh = params->colorToning.hlColSat.getBottom() / 100.f;

    float Balan = params->colorToning.balance;

    float chMixRR = params->chmixer.red[0] / 10.f;
    float chMixRG = params->chmixer.red[1] / 10.f;
    float chMixRB = params->chmixer.red[2] / 10.f;
    float chMixGR = params->chmixer.green[0] / 10.f;
    float chMixGG = params->chmixer.green[1] / 10.f;
    float chMixGB = params->chmixer.green[2] / 10.f;
    float chMixBR = params->chmixer.blue[0] / 10.f;
    float chMixBG = params->chmixer.blue[1] / 10.f;
    float chMixBB = params->chmixer.blue[2] / 10.f;

    bool blackwhite = params->blackwhite.enabled;
    bool complem = params->blackwhite.enabledcc;
    float bwr = params->blackwhite.mixerRed;
    float bwg = params->blackwhite.mixerGreen;
    float bwb = params->blackwhite.mixerBlue;
    float bwrgam = params->blackwhite.gammaRed;
    float bwggam = params->blackwhite.gammaGreen;
    float bwbgam = params->blackwhite.gammaBlue;
    float mixerOrange = params->blackwhite.mixerOrange;
    float mixerYellow = params->blackwhite.mixerYellow;
    float mixerCyan = params->blackwhite.mixerCyan;
    float mixerMagenta = params->blackwhite.mixerMagenta;
    float mixerPurple = params->blackwhite.mixerPurple;
    int algm = 0;

    if (params->blackwhite.method == "Desaturation") {
        algm = 0;
    } else if (params->blackwhite.method == "LumEqualizer") {
        algm = 1;
    } else if (params->blackwhite.method == "ChannelMixer") {
        algm = 2;
    }

    float kcorec = 1.f;
    //gamma correction of each channel
    float gamvalr = 125.f;
    float gamvalg = 125.f;
    float gamvalb = 125.f;
    bool computeMixerAuto = params->blackwhite.autoc && (autor < -5000.f);

    if (bwrgam < 0) {
        gamvalr = 100.f;
    }

    if (bwggam < 0) {
        gamvalg = 100.f;
    }

    if (bwbgam < 0) {
        gamvalb = 100.f;
    }

    float gammabwr = 1.f;
    float gammabwg = 1.f;
    float gammabwb = 1.f;
    //if     (params->blackwhite.setting=="Ma" || params->blackwhite.setting=="Mr" || params->blackwhite.setting=="Fr" || params->blackwhite.setting=="Fa")  {
    {
        gammabwr = 1.f - bwrgam / gamvalr;
        gammabwg = 1.f - bwggam / gamvalg;
        gammabwb = 1.f - bwbgam / gamvalb;
    }
    bool hasgammabw = gammabwr != 1.f || gammabwg != 1.f || gammabwb != 1.f;

    if (hasColorToning || blackwhite || (params->dirpyrequalizer.cbdlMethod == "bef" && params->dirpyrequalizer.enabled) || split_tiled_parts_1_2) {
        tmpImage.reset(new Imagefloat(working->getWidth(), working->getHeight()));
    }

    // For tonecurve histogram
    int toneCurveHistSize = histToneCurve ? histToneCurve.getSize() : 0;
    int histToneCurveCompression = 0;

    if (toneCurveHistSize > 0) {
        histToneCurve.clear();
        histToneCurveCompression = log2(65536 / toneCurveHistSize);
    }

    // For tonecurve histogram
    const float lumimulf[3] = {static_cast<float>(lumimul[0]), static_cast<float>(lumimul[1]), static_cast<float>(lumimul[2])};

#define TS 112

    const auto tiled_part_1 =
        [working,
            mixchannels,
            &hltonecurve, &shtonecurve,
            chMixRR, chMixRG, chMixRB,
            chMixGR, chMixGG, chMixGB,
            chMixBR, chMixBG, chMixBB,
            exp_scale, comp, hlrange, tone_curve_black](
            int istart, int jstart, int tH, int tW,
            float *rtemp, float *gtemp, float *btemp) {

            for (int i = istart, ti = 0; i < tH; i++, ti++) {
                for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                    rtemp[ti * TS + tj] = working->r(i, j);
                    gtemp[ti * TS + tj] = working->g(i, j);
                    btemp[ti * TS + tj] = working->b(i, j);
                }
            }

            if (mixchannels) {
                for (int i = istart, ti = 0; i < tH; i++, ti++) {
                    for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                        float r = rtemp[ti * TS + tj];
                        float g = gtemp[ti * TS + tj];
                        float b = btemp[ti * TS + tj];

                        // if (i==100 & j==100) printf("rgbProc input R= %f  G= %f  B= %f  \n",r,g,b);
                        float rmix = (r * chMixRR + g * chMixRG + b * chMixRB) / 100.f;
                        float gmix = (r * chMixGR + g * chMixGG + b * chMixGB) / 100.f;
                        float bmix = (r * chMixBR + g * chMixBG + b * chMixBB) / 100.f;

                        rtemp[ti * TS + tj] = rmix;
                        gtemp[ti * TS + tj] = gmix;
                        btemp[ti * TS + tj] = bmix;
                    }
                }
            }

            highlightToneCurve(hltonecurve, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS, exp_scale, comp, hlrange);

            if (tone_curve_black != 0) {
                shadowToneCurve(shtonecurve, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS);
            }
        };

#ifdef _OPENMP
    #pragma omp parallel if (multiThread)
#endif
    {
        size_t perChannelSizeBytes = padToAlignment(sizeof(float) * TS * TS + 4 * 64);
        AlignedBuffer<float> buffer(3 * perChannelSizeBytes);
        char *editIFloatBuffer = nullptr;
        char *editWhateverBuffer = nullptr;
        float *rtemp = buffer.data;
        float *gtemp = &rtemp[perChannelSizeBytes / sizeof(float)];
        float *btemp = &gtemp[perChannelSizeBytes / sizeof(float)];
        int istart;
        int jstart;
        int tW;
        int tH;

        // zero out the buffers
        memset(rtemp, 0, 3 * perChannelSizeBytes);

        // Allocating buffer for the PipetteBuffer
        float *editIFloatTmpR = nullptr, *editIFloatTmpG = nullptr, *editIFloatTmpB = nullptr, *editWhateverTmp = nullptr;

        if (editImgFloat) {
            editIFloatBuffer = (char *) malloc(3 * sizeof(float) * TS * TS + 20 * 64 + 63);
            char *data = (char*)((uintptr_t (editIFloatBuffer) + uintptr_t (63)) / 64 * 64);

            editIFloatTmpR = (float (*))data;
            editIFloatTmpG = (float (*))((char*)editIFloatTmpR + sizeof(float) * TS * TS + 4 * 64);
            editIFloatTmpB = (float (*))((char*)editIFloatTmpG + sizeof(float) * TS * TS + 8 * 64);
        }

        if (editWhatever) {
            editWhateverBuffer = (char *) malloc(sizeof(float) * TS * TS + 20 * 64 + 63);
            char *data = (char*)((uintptr_t (editWhateverBuffer) + uintptr_t (63)) / 64 * 64);

            editWhateverTmp = (float (*))data;
        }

        float out_rgbx[4 * TS] ALIGNED16; // Line buffer for CLUT
        float clutr[TS] ALIGNED16;
        float clutg[TS] ALIGNED16;
        float clutb[TS] ALIGNED16;

        LUTu histToneCurveThr;

        if (toneCurveHistSize > 0) {
            histToneCurveThr(toneCurveHistSize);
            histToneCurveThr.clear();
        }

        if (split_tiled_parts_1_2) {

#ifdef _OPENMP
        #pragma omp for schedule(dynamic, chunkSize) collapse(2)
#endif

            for (int ii = 0; ii < working->getHeight(); ii += TS) {
                for (int jj = 0; jj < working->getWidth(); jj += TS) {
                    istart = ii;
                    jstart = jj;
                    tH = min(ii + TS, working->getHeight());
                    tW = min(jj + TS, working->getWidth());


                    tiled_part_1(istart, jstart, tH, tW, rtemp, gtemp, btemp);

                    // Copy tile to image.
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            tmpImage->r(i, j) = rtemp[ti * TS + tj];
                            tmpImage->g(i, j) = gtemp[ti * TS + tj];
                            tmpImage->b(i, j) = btemp[ti * TS + tj];
                        }
                    }
                }
            }
        }

#ifdef _OPENMP
        #pragma omp single
#endif
        if (params->toneEqualizer.enabled) {
            toneEqualizer(tmpImage.get());
        }

#ifdef _OPENMP
        #pragma omp for schedule(dynamic, chunkSize) collapse(2)
#endif

        for (int ii = 0; ii < working->getHeight(); ii += TS)
            for (int jj = 0; jj < working->getWidth(); jj += TS) {
                istart = ii;
                jstart = jj;
                tH = min(ii + TS, working->getHeight());
                tW = min(jj + TS, working->getWidth());

                if (split_tiled_parts_1_2) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            rtemp[ti * TS + tj] = tmpImage->r(i, j);
                            gtemp[ti * TS + tj] = tmpImage->g(i, j);
                            btemp[ti * TS + tj] = tmpImage->b(i, j);
                        }
                    }
                } else {
                    tiled_part_1(istart, jstart, tH, tW, rtemp, gtemp, btemp);
                }

                if (dcpProf) {
                    dcpProf->step2ApplyTile(rtemp, gtemp, btemp, tW - jstart, tH - istart, TS, asIn);
                }

                if (params->toneCurve.clampOOG) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            // clip out of gamut colors, without distorting colour too bad
                            float r = std::max(rtemp[ti * TS + tj], 0.f);
                            float g = std::max(gtemp[ti * TS + tj], 0.f);
                            float b = std::max(btemp[ti * TS + tj], 0.f);

                            if (OOG(r) || OOG(g) || OOG(b)) {
                                filmlike_clip(&r, &g, &b);
                            }

                            rtemp[ti * TS + tj] = r;
                            gtemp[ti * TS + tj] = g;
                            btemp[ti * TS + tj] = b;
                        }
                    }

                }

                if (histToneCurveThr) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {

                            //brightness/contrast
                            float r = tonecurve[ CLIP(rtemp[ti * TS + tj]) ];
                            float g = tonecurve[ CLIP(gtemp[ti * TS + tj]) ];
                            float b = tonecurve[ CLIP(btemp[ti * TS + tj]) ];

                            int y = CLIP<int> (lumimulf[0] * Color::gamma2curve[rtemp[ti * TS + tj]] + lumimulf[1] * Color::gamma2curve[gtemp[ti * TS + tj]] + lumimulf[2] * Color::gamma2curve[btemp[ti * TS + tj]]);
                            histToneCurveThr[y >> histToneCurveCompression]++;

                            setUnlessOOG(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], r, g, b);
                        }
                    }
                } else {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        int j = jstart, tj = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
                        float tmpr[4] ALIGNED16;
                        float tmpg[4] ALIGNED16;
                        float tmpb[4] ALIGNED16;

                        for (; j < tW - 3; j += 4, tj += 4) {
                            //brightness/contrast
                            STVF(tmpr[0], tonecurve(LVF(rtemp[ti * TS + tj])));
                            STVF(tmpg[0], tonecurve(LVF(gtemp[ti * TS + tj])));
                            STVF(tmpb[0], tonecurve(LVF(btemp[ti * TS + tj])));

                            for (int k = 0; k < 4; ++k) {
                                setUnlessOOG(rtemp[ti * TS + tj + k], gtemp[ti * TS + tj + k], btemp[ti * TS + tj + k], tmpr[k], tmpg[k], tmpb[k]);
                            }
                        }

#endif

                        for (; j < tW; j++, tj++) {
                            //brightness/contrast
                            setUnlessOOG(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], tonecurve[rtemp[ti * TS + tj]], tonecurve[gtemp[ti * TS + tj]], tonecurve[btemp[ti * TS + tj]]);
                        }
                    }
                }

                if (editID == EUID_ToneCurve1) {  // filling the pipette buffer
                    fillEditFloat(editIFloatTmpR, editIFloatTmpG, editIFloatTmpB, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS);
                }

                if (hasToneCurve1) {
                    customToneCurve(customToneCurve1, curveMode, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS, ptc1ApplyState);
                }

                if (editID == EUID_ToneCurve2) {  // filling the pipette buffer
                    fillEditFloat(editIFloatTmpR, editIFloatTmpG, editIFloatTmpB, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS);
                }

                if (hasToneCurve2) {
                    customToneCurve(customToneCurve2, curveMode2, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS, ptc2ApplyState);
                }

                if (editID == EUID_RGB_R) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            editWhateverTmp[ti * TS + tj] = Color::gamma2curve[rtemp[ti * TS + tj]] / 65536.f;
                        }
                    }
                } else if (editID == EUID_RGB_G) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            editWhateverTmp[ti * TS + tj] = Color::gamma2curve[gtemp[ti * TS + tj]] / 65536.f;
                        }
                    }
                } else if (editID == EUID_RGB_B) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            editWhateverTmp[ti * TS + tj] = Color::gamma2curve[btemp[ti * TS + tj]] / 65536.f;
                        }
                    }
                }

                if (params->rgbCurves.enabled && (rCurve || gCurve || bCurve)) { // if any of the RGB curves is engaged
                    if (!params->rgbCurves.lumamode) { // normal RGB mode

                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                // individual R tone curve
                                if (rCurve) {
                                    setUnlessOOG(rtemp[ti * TS + tj], rCurve[ rtemp[ti * TS + tj] ]);
                                }

                                // individual G tone curve
                                if (gCurve) {
                                    setUnlessOOG(gtemp[ti * TS + tj], gCurve[ gtemp[ti * TS + tj] ]);
                                }

                                // individual B tone curve
                                if (bCurve) {
                                    setUnlessOOG(btemp[ti * TS + tj], bCurve[ btemp[ti * TS + tj] ]);
                                }
                            }
                        }
                    } else { //params->rgbCurves.lumamode==true (Luminosity mode)
                        // rCurve.dump("r_curve");//debug

                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                // rgb values before RGB curves
                                float r = rtemp[ti * TS + tj] ;
                                float g = gtemp[ti * TS + tj] ;
                                float b = btemp[ti * TS + tj] ;
                                //convert to Lab to get a&b before RGB curves
                                float x = toxyz[0][0] * r + toxyz[0][1] * g + toxyz[0][2] * b;
                                float y = toxyz[1][0] * r + toxyz[1][1] * g + toxyz[1][2] * b;
                                float z = toxyz[2][0] * r + toxyz[2][1] * g + toxyz[2][2] * b;

                                float fx = x < MAXVALF ? Color::cachef[x] : 327.68f * std::cbrt(x / MAXVALF);
                                float fy = y < MAXVALF ? Color::cachef[y] : 327.68f * std::cbrt(y / MAXVALF);
                                float fz = z < MAXVALF ? Color::cachef[z] : 327.68f * std::cbrt(z / MAXVALF);

                                float a_1 = 500.0f * (fx - fy);
                                float b_1 = 200.0f * (fy - fz);

                                // rgb values after RGB curves
                                if (rCurve) {
                                    float rNew = rCurve[r];
                                    r += (rNew - r) * equalR;
                                }

                                if (gCurve) {
                                    float gNew = gCurve[g];
                                    g += (gNew - g) * equalG;
                                }

                                if (bCurve) {
                                    float bNew = bCurve[b];
                                    b += (bNew - b) * equalB;
                                }

                                // Luminosity after
                                // only Luminance in Lab
                                float newy = toxyz[1][0] * r + toxyz[1][1] * g + toxyz[1][2] * b;
                                float L_2 = newy <= MAXVALF ? Color::cachefy[newy] : 327.68f * (116.f * xcbrtf(newy / MAXVALF) - 16.f);

                                //gamut control
                                if (settings->rgbcurveslumamode_gamut) {
                                    float Lpro = L_2 / 327.68f;
                                    float Chpro = sqrtf(SQR(a_1) + SQR(b_1)) / 327.68f;
                                    float HH = NAN; // we set HH to NAN, because then it will be calculated in Color::gamutLchonly only if needed
//                                    float HH = xatan2f(b_1, a_1);
                                    // According to mathematical laws we can get the sin and cos of HH by simple operations even if we don't calculate HH
                                    float2 sincosval;

                                    if (Chpro == 0.0f) {
                                        sincosval.y = 1.0f;
                                        sincosval.x = 0.0f;
                                    } else {
                                        sincosval.y = a_1 / (Chpro * 327.68f);
                                        sincosval.x = b_1 / (Chpro * 327.68f);
                                    }

                                    //gamut control : Lab values are in gamut
                                    Color::gamutLchonly(HH, sincosval, Lpro, Chpro, r, g, b, wip, highlight, 0.15f, 0.96f);
                                    //end of gamut control
                                } else {
                                    float x_, y_, z_;
                                    //calculate RGB with L_2 and old value of a and b
                                    Color::Lab2XYZ(L_2, a_1, b_1, x_, y_, z_) ;
                                    Color::xyz2rgb(x_, y_, z_, r, g, b, wip);
                                }

                                setUnlessOOG(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], r, g, b);
                            }
                        }
                    }
                }

                if (editID == EUID_HSV_H || editID == EUID_HSV_S || editID == EUID_HSV_V) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            float h, s, v;
                            Color::rgb2hsv(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], h, s, v);
                            editWhateverTmp[ti * TS + tj] = h;
                        }
                    }
                }

                if (sat != 0 || hCurveEnabled || sCurveEnabled || vCurveEnabled) {
                    const float satby100 = sat / 100.f;

                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            float h, s, v;
                            Color::rgb2hsvtc(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], h, s, v);
                            h /= 6.f;

                            if (sat > 0) {
                                s = std::max(0.f, intp(satby100, 1.f - SQR(SQR(1.f - std::min(s, 1.0f))), s));
                            } else { /*if (sat < 0)*/
                                s *= 1.f + satby100;
                            }

                            //HSV equalizer
                            if (hCurveEnabled) {
                                h = (hCurve->getVal(h) - 0.5) * 2.0 + static_cast<double>(h);

                                if (h > 1.0f) {
                                    h -= 1.0f;
                                } else if (h < 0.0f) {
                                    h += 1.0f;
                                }
                            }

                            if (sCurveEnabled) {
                                //shift saturation
                                float satparam = (sCurve->getVal(double (h)) - 0.5) * 2;

                                if (satparam > 0.00001f) {
                                    s = (1.f - satparam) * s + satparam * (1.f - SQR(1.f - min(s, 1.0f)));

                                    if (s < 0.f) {
                                        s = 0.f;
                                    }
                                } else if (satparam < -0.00001f) {
                                    s *= 1.f + satparam;
                                }

                            }

                            if (vCurveEnabled) {
                                if (v < 0) {
                                    v = 0;    // important
                                }

                                //shift value
                                float valparam = vCurve->getVal(h) - 0.5;
                                valparam *= (1.f - SQR(SQR(1.f - min(s, 1.0f))));

                                if (valparam > 0.00001f) {
                                    v = (1.f - valparam) * v + valparam * (1.f - SQR(1.f - min(v, 1.0f)));   // SQR (SQR  to increase action and avoid artifacts

                                    if (v < 0) {
                                        v = 0;
                                    }
                                } else {
                                    if (valparam < -0.00001f) {
                                        v *= (1.f + valparam);    //1.99 to increase action
                                    }
                                }

                            }

                            Color::hsv2rgbdcp(h * 6.f, s, v, rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj]);
                        }
                    }
                }

                if (isProPhoto) { // this is a hack to avoid the blue=>black bug (Issue 2141)
                    proPhotoBlue(rtemp, gtemp, btemp, istart, tH, jstart, tW, TS);
                }

                if (hasColorToning && !blackwhite) {
                    if (params->colorToning.method == "Splitlr") {
                        constexpr float reducac = 0.4f;
                        int preser = 0;

                        if (params->colorToning.lumamode) {
                            preser = 1;
                        }

                        const float balanS = 1.f + Balan / 100.f; //balan between 0 and 2
                        const float balanH = 1.f - Balan / 100.f;
                        float rh, gh, bh;
                        float rl, gl, bl;
                        float xh, yh, zh;
                        float xl, yl, zl;
                        const float iplow = ctColorCurve.low;
                        const float iphigh = ctColorCurve.high;
                        //2 colours
                        ctColorCurve.getVal(iphigh, xh, yh, zh);
                        ctColorCurve.getVal(iplow, xl, yl, zl);

                        Color::xyz2rgb(xh, yh, zh, rh, gh, bh, wip);
                        Color::xyz2rgb(xl, yl, zl, rl, gl, bl, wip);
                        //reteave rgb value with s and l =1
                        retreavergb(rl, gl, bl);
                        const float krl = rl / (rl + gl + bl);
                        const float kgl = gl / (rl + gl + bl);
                        const float kbl = bl / (rl + gl + bl);
                        retreavergb(rh, gh, bh);
                        const float krh = rh / (rh + gh + bh);
                        const float kgh = gh / (rh + gh + bh);
                        const float kbh = bh / (rh + gh + bh);
                        constexpr int mode = 0;

                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                toning2col(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], iplow, iphigh, krl, kgl, kbl, krh, kgh, kbh, SatLow, SatHigh, balanS, balanH, reducac, mode, preser, strProtect);
                            }
                        }
                    }

                    // colour toning with colour
                    else if (params->colorToning.method == "Splitco") {
                        constexpr float reducac = 0.3f;
                        constexpr int mode = 0;

                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                const float r = rtemp[ti * TS + tj];
                                const float g = gtemp[ti * TS + tj];
                                const float b = btemp[ti * TS + tj];
                                float ro, go, bo;
                                toningsmh(r, g, b, ro, go, bo, RedLow, GreenLow, BlueLow, RedMed, GreenMed, BlueMed, RedHigh, GreenHigh, BlueHigh, reducac, mode, strProtect);

                                if (params->colorToning.lumamode) {
                                    const float lumbefore = 0.299f * r + 0.587f * g + 0.114f * b;
                                    const float lumafter = 0.299f * ro + 0.587f * go + 0.114f * bo;
                                    const float preserv = lumbefore / lumafter;
                                    ro *= preserv;
                                    go *= preserv;
                                    bo *= preserv;
                                }

                                setUnlessOOG(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], CLIP(ro), CLIP(go), CLIP(bo));
                            }
                        }
                    }

                    //colortoning with shift color XYZ or Lch
                    else if (params->colorToning.method == "Lab" && opautili) {
                        int algo = 0;
                        bool twocol = true;//true=500 color   false=2 color
                        int metchrom = 0;

                        if (params->colorToning.twocolor == "Std") {
                            metchrom = 0;
                        } else if (params->colorToning.twocolor == "All") {
                            metchrom = 1;
                        } else if (params->colorToning.twocolor == "Separ") {
                            metchrom = 2;
                        } else if (params->colorToning.twocolor == "Two") {
                            metchrom = 3;
                        }

                        if (metchrom == 3) {
                            twocol = false;
                        }

                        float iplow = 0.f, iphigh = 0.f;

                        if (!twocol) {
                            iplow = (float)ctColorCurve.low;
                            iphigh = (float)ctColorCurve.high;
                        }

                        int twoc = 0; //integer instead of bool to let more possible choice...other than 2 and 500.

                        if (!twocol) {
                            twoc = 0;    // 2 colours
                        } else {
                            twoc = 1;    // 500 colours
                        }

                        if (params->colorToning.method == "Lab") {
                            algo = 1;
                        } else if (params->colorToning.method == "Lch") {
                            algo = 2;    //in case of
                        }

                        if (algo <= 2) {
                            for (int i = istart, ti = 0; i < tH; i++, ti++) {
                                for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                    float r = rtemp[ti * TS + tj];
                                    float g = gtemp[ti * TS + tj];
                                    float b = btemp[ti * TS + tj];
                                    float ro, go, bo;
                                    labtoning(r, g, b, ro, go, bo, algo, metchrom, twoc, satLimit, satLimitOpacity, ctColorCurve, ctOpacityCurve, clToningcurve, cl2Toningcurve, iplow, iphigh, wp, wip);
                                    setUnlessOOG(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], ro, go, bo);
                                }
                            }
                        }
                    } else if (params->colorToning.method.substr(0, 3) == "RGB" && opautili) {
                        // color toning
                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                float r = rtemp[ti * TS + tj];
                                float g = gtemp[ti * TS + tj];
                                float b = btemp[ti * TS + tj];

                                // Luminance = (0.299f*r + 0.587f*g + 0.114f*b)

                                float s, l;
                                Color::rgb2slfloat(r, g, b, s, l);

                                float l_ = Color::gammatab_srgb1[l * 65535.f];

                                // get the opacity and tweak it to preserve saturated colors
                                float opacity = 0.f;

                                if (ctOpacityCurve) {
                                    opacity = (1.f - min<float> (s / satLimit, 1.f) * (1.f - satLimitOpacity)) * ctOpacityCurve.lutOpacityCurve[l_ * 500.f];
                                }

                                float r2, g2, b2;
                                ctColorCurve.getVal(l_, r2, g2, b2);  // get the color from the color curve

                                float h2, s2, l2;
                                Color::rgb2hslfloat(r2, g2, b2, h2, s2, l2);  // transform this new color to hsl

                                Color::hsl2rgbfloat(h2, s + ((1.f - s) * (1.f - l) * 0.7f), l, r2, g2, b2);

                                rtemp[ti * TS + tj] = r + (r2 - r) * opacity; // merge the color to the old color, depending on the opacity
                                gtemp[ti * TS + tj] = g + (g2 - g) * opacity;
                                btemp[ti * TS + tj] = b + (b2 - b) * opacity;
                            }
                        }
                    }
                }

                // filling the pipette buffer
                if (editID == EUID_BlackWhiteBeforeCurve) {
                    fillEditFloat(editIFloatTmpR, editIFloatTmpG, editIFloatTmpB, rtemp, gtemp, btemp, istart, tH, jstart, tW, TS);
                } else if (editID == EUID_BlackWhiteLuminance) {
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            float X, Y, Z, L, aa, bb;
                            //rgb=>lab
                            Color::rgbxyz(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], X, Y, Z, wp);
                            //convert Lab
                            Color::XYZ2Lab(X, Y, Z, L, aa, bb);
                            //end rgb=>lab
                            float HH = xatan2f(bb, aa);  // HH hue in -3.141  +3.141

                            editWhateverTmp[ti * TS + tj] = float (Color::huelab_to_huehsv2(HH));
                        }
                    }
                }

                //black and white
                if (blackwhite) {
                    if (hasToneCurvebw1) {
                        if (beforeCurveMode == BlackWhiteParams::TcMode::STD_BW) { // Standard
                            for (int i = istart, ti = 0; i < tH; i++, ti++) {
                                for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                    const StandardToneCurve& userToneCurvebw = static_cast<const StandardToneCurve&>(customToneCurvebw1);
                                    userToneCurvebw.Apply(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj]);
                                }
                            }
                        } else if (beforeCurveMode == BlackWhiteParams::TcMode::FILMLIKE_BW) { // Adobe like
                            for (int i = istart, ti = 0; i < tH; i++, ti++) {
                                for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                    const AdobeToneCurve& userToneCurvebw = static_cast<const AdobeToneCurve&>(customToneCurvebw1);
                                    userToneCurvebw.Apply(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj]);
                                }
                            }
                        } else if (beforeCurveMode == BlackWhiteParams::TcMode::SATANDVALBLENDING_BW) { // apply the curve on the saturation and value channels
                            for (int i = istart, ti = 0; i < tH; i++, ti++) {
                                for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                    const SatAndValueBlendingToneCurve& userToneCurvebw = static_cast<const SatAndValueBlendingToneCurve&>(customToneCurvebw1);
                                    // rtemp[ti * TS + tj] = CLIP<float> (rtemp[ti * TS + tj]);
                                    // gtemp[ti * TS + tj] = CLIP<float> (gtemp[ti * TS + tj]);
                                    // btemp[ti * TS + tj] = CLIP<float> (btemp[ti * TS + tj]);
                                    userToneCurvebw.Apply(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj]);
                                }
                            }
                        } else if (beforeCurveMode == BlackWhiteParams::TcMode::WEIGHTEDSTD_BW) { // apply the curve to the rgb channels, weighted
                            for (int i = istart, ti = 0; i < tH; i++, ti++) {
                                for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                    const WeightedStdToneCurve& userToneCurvebw = static_cast<const WeightedStdToneCurve&>(customToneCurvebw1);
                                    // rtemp[ti * TS + tj] = CLIP<float> (rtemp[ti * TS + tj]);
                                    // gtemp[ti * TS + tj] = CLIP<float> (gtemp[ti * TS + tj]);
                                    // btemp[ti * TS + tj] = CLIP<float> (btemp[ti * TS + tj]);

                                    userToneCurvebw.Apply(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj]);
                                }
                            }
                        }
                    }

                    if (algm == 0) { //lightness
                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {

                                float r = rtemp[ti * TS + tj];
                                float g = gtemp[ti * TS + tj];
                                float b = btemp[ti * TS + tj];

                                // --------------------------------------------------

                                // Method 1: Luminosity (code taken from Gimp)
                                /*
                                float maxi = max(r, g, b);
                                float mini = min(r, g, b);
                                r = g = b = (maxi+mini)/2;
                                */

                                // Method 2: Luminance (former RT code)
                                r = g = b = (0.299f * r + 0.587f * g + 0.114f * b);

                                // --------------------------------------------------

#if ! defined(__SSE2__) && ! defined(RT_SIMDE)

                                //gamma correction: pseudo TRC curve
                                if (hasgammabw) {
                                    Color::trcGammaBW(r, g, b, gammabwr, gammabwg, gammabwb);
                                }

#endif
                                rtemp[ti * TS + tj] = r;
                                gtemp[ti * TS + tj] = g;
                                btemp[ti * TS + tj] = b;
                            }

#if defined(__SSE2__) || defined(RT_SIMDE)

                            if (hasgammabw) {
                                //gamma correction: pseudo TRC curve
                                Color::trcGammaBWRow(&rtemp[ti * TS], &gtemp[ti * TS], &btemp[ti * TS], tW - jstart, gammabwr, gammabwg, gammabwb);
                            }

#endif

                        }
                    } else if (algm == 1) { //Luminance mixer in Lab mode to avoid artifacts
                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                                //rgb => xyz
                                float X, Y, Z;
                                Color::rgbxyz(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], X, Y, Z, wp);
                                //xyz => Lab
                                float L, aa, bb;
                                Color::XYZ2Lab(X, Y, Z, L, aa, bb);
                                float CC = sqrtf(SQR(aa) + SQR(bb)) / 327.68f;    //CC chromaticity in 0..180 or more
                                float HH = xatan2f(bb, aa);  // HH hue in -3.141  +3.141
                                float2 sincosval;

                                if (CC == 0.0f) {
                                    sincosval.y = 1.f;
                                    sincosval.x = 0.0f;
                                } else {
                                    sincosval.y = aa / (CC * 327.68f);
                                    sincosval.x = bb / (CC * 327.68f);
                                }

                                if (bwlCurveEnabled) {
                                    L /= 32768.f;
                                    double hr = Color::huelab_to_huehsv2(HH);
                                    float valparam = (bwlCurve->getVal(hr) - 0.5) * 2.0; //get l_r=f(H)
                                    float kcc = (CC / 70.f); //take Chroma into account...70 "middle" of chromaticity (arbitrary and simple), one can imagine other algorithme
                                    //reduct action for low chroma and increase action for high chroma
                                    valparam *= kcc;

                                    if (valparam > 0.f) {
                                        L = (1.f - valparam) * L + valparam * (1.f - SQR(SQR(SQR(SQR(1.f - min(L, 1.0f))))));      // SQR (SQR((SQR)  to increase action in low light
                                    } else {
                                        L *= (1.f + valparam);    //for negative
                                    }

                                    L *= 32768.f;
                                }

                                float RR, GG, BB;
                                L /= 327.68f;
                                //gamut control : Lab values are in gamut
                                Color::gamutLchonly(HH, sincosval, L, CC, RR, GG, BB, wip, highlight, 0.15f, 0.96f);
                                L *= 327.68f;
                                //convert l => rgb
                                Color::L2XYZ(L, X, Y, Z);
                                float newRed; // We use the red channel for bw
                                Color::xyz2r(X, Y, Z, newRed, wip);
                                rtemp[ti * TS + tj] = gtemp[ti * TS + tj] = btemp[ti * TS + tj] = newRed;
#if ! defined(__SSE2__) && ! defined(RT_SIMDE)

                                if (hasgammabw) {
                                    //gamma correction: pseudo TRC curve
                                    Color::trcGammaBW(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], gammabwr, gammabwg, gammabwb);
                                }

#endif
                            }

#if defined(__SSE2__) || defined(RT_SIMDE)

                            if (hasgammabw) {
                                //gamma correction: pseudo TRC curve
                                Color::trcGammaBWRow(&rtemp[ti * TS], &gtemp[ti * TS], &btemp[ti * TS], tW - jstart, gammabwr, gammabwg, gammabwb);
                            }

#endif
                        }
                    }
                }


                // Film Simulations
                if (hald_clut) {

                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        if (!clutAndWorkingProfilesAreSame) {
                            // Convert from working to clut profile
                            int j = jstart;
                            int tj = 0;

#if defined(__SSE2__) || defined(RT_SIMDE)

                            for (; j < tW - 3; j += 4, tj += 4) {
                                vfloat sourceR = LVF(rtemp[ti * TS + tj]);
                                vfloat sourceG = LVF(gtemp[ti * TS + tj]);
                                vfloat sourceB = LVF(btemp[ti * TS + tj]);

                                vfloat x;
                                vfloat y;
                                vfloat z;
                                Color::rgbxyz(sourceR, sourceG, sourceB, x, y, z, v_work2xyz);
                                Color::xyz2rgb(x, y, z, sourceR, sourceG, sourceB, v_xyz2clut);

                                STVF(clutr[tj], sourceR);
                                STVF(clutg[tj], sourceG);
                                STVF(clutb[tj], sourceB);
                            }

#endif

                            for (; j < tW; j++, tj++) {
                                float sourceR = rtemp[ti * TS + tj];
                                float sourceG = gtemp[ti * TS + tj];
                                float sourceB = btemp[ti * TS + tj];

                                float x, y, z;
                                Color::rgbxyz(sourceR, sourceG, sourceB, x, y, z, wprof);
                                Color::xyz2rgb(x, y, z, clutr[tj], clutg[tj], clutb[tj], xyz2clut);
                            }
                        } else {
                            memcpy(clutr, &rtemp[ti * TS], sizeof(float) * TS);
                            memcpy(clutg, &gtemp[ti * TS], sizeof(float) * TS);
                            memcpy(clutb, &btemp[ti * TS], sizeof(float) * TS);
                        }

                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            float &sourceR = clutr[tj];
                            float &sourceG = clutg[tj];
                            float &sourceB = clutb[tj];

                            // Apply gamma sRGB (default RT)
                            sourceR = Color::gamma_srgbclipped(sourceR);
                            sourceG = Color::gamma_srgbclipped(sourceG);
                            sourceB = Color::gamma_srgbclipped(sourceB);
                        }

                        hald_clut->getRGB(
                            film_simulation_strength,
                            std::min(TS, tW - jstart),
                            clutr,
                            clutg,
                            clutb,
                            out_rgbx
                        );

                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            float &sourceR = clutr[tj];
                            float &sourceG = clutg[tj];
                            float &sourceB = clutb[tj];

                            // Apply inverse gamma sRGB
                            sourceR = Color::igamma_srgb_extended(out_rgbx[tj * 4 + 0]);
                            sourceG = Color::igamma_srgb_extended(out_rgbx[tj * 4 + 1]);
                            sourceB = Color::igamma_srgb_extended(out_rgbx[tj * 4 + 2]);
                        }

                        if (!clutAndWorkingProfilesAreSame) {
                            // Convert from clut to working profile
                            int j = jstart;
                            int tj = 0;

#if defined(__SSE2__) || defined(RT_SIMDE)

                            for (; j < tW - 3; j += 4, tj += 4) {
                                vfloat sourceR = LVF(clutr[tj]);
                                vfloat sourceG = LVF(clutg[tj]);
                                vfloat sourceB = LVF(clutb[tj]);

                                vfloat x;
                                vfloat y;
                                vfloat z;
                                Color::rgbxyz(sourceR, sourceG, sourceB, x, y, z, v_clut2xyz);
                                Color::xyz2rgb(x, y, z, sourceR, sourceG, sourceB, v_xyz2work);

                                STVF(clutr[tj], sourceR);
                                STVF(clutg[tj], sourceG);
                                STVF(clutb[tj], sourceB);
                            }

#endif

                            for (; j < tW; j++, tj++) {
                                float &sourceR = clutr[tj];
                                float &sourceG = clutg[tj];
                                float &sourceB = clutb[tj];

                                float x, y, z;
                                Color::rgbxyz(sourceR, sourceG, sourceB, x, y, z, clut2xyz);
                                Color::xyz2rgb(x, y, z, sourceR, sourceG, sourceB, wiprof);
                            }
                        }

                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            setUnlessOOG(rtemp[ti * TS + tj], gtemp[ti * TS + tj], btemp[ti * TS + tj], clutr[tj], clutg[tj], clutb[tj]);
                        }
                    }
                }

                //softLight(rtemp, gtemp, btemp, istart, jstart, tW, tH, TS);

                if (!blackwhite) {
                    if (editImgFloat || editWhatever) {
                        for (int i = istart, ti = 0; i < tH; i++, ti++) {
                            for (int j = jstart, tj = 0; j < tW; j++, tj++) {

                                // filling the pipette buffer by the content of the temp pipette buffers
                                if (editImgFloat) {
                                    editImgFloat->r(i, j) = editIFloatTmpR[ti * TS + tj];
                                    editImgFloat->g(i, j) = editIFloatTmpG[ti * TS + tj];
                                    editImgFloat->b(i, j) = editIFloatTmpB[ti * TS + tj];
                                } else if (editWhatever) {
                                    editWhatever->v(i, j) = editWhateverTmp[ti * TS + tj];
                                }
                            }
                        }
                    }

                    // ready, fill lab
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        Color::RGB2Lab(&rtemp[ti * TS], &gtemp[ti * TS], &btemp[ti * TS], &(lab->L[i][jstart]), &(lab->a[i][jstart]), &(lab->b[i][jstart]), toxyz, tW - jstart);
                    }

                    // if (hasColorToningLabGrid) {
                    //     colorToningLabGrid(lab, jstart, tW, istart, tH, false);
                    // }
                } else { // black & white
                    // Auto channel mixer needs whole image, so we now copy to tmpImage and close the tiled processing
                    for (int i = istart, ti = 0; i < tH; i++, ti++) {
                        for (int j = jstart, tj = 0; j < tW; j++, tj++) {
                            // filling the pipette buffer by the content of the temp pipette buffers
                            if (editImgFloat) {
                                editImgFloat->r(i, j) = editIFloatTmpR[ti * TS + tj];
                                editImgFloat->g(i, j) = editIFloatTmpG[ti * TS + tj];
                                editImgFloat->b(i, j) = editIFloatTmpB[ti * TS + tj];
                            } else if (editWhatever) {
                                editWhatever->v(i, j) = editWhateverTmp[ti * TS + tj];
                            }

                            tmpImage->r(i, j) = rtemp[ti * TS + tj];
                            tmpImage->g(i, j) = gtemp[ti * TS + tj];
                            tmpImage->b(i, j) = btemp[ti * TS + tj];
                        }
                    }
                }
            }

        if (editIFloatBuffer) {
            free(editIFloatBuffer);
        }

        if (editWhateverBuffer) {
            free(editWhateverBuffer);
        }

#ifdef _OPENMP
        #pragma omp critical
        {
            if (toneCurveHistSize > 0) {
                histToneCurve += histToneCurveThr;
            }
        }
#endif // _OPENMP
    }

    // starting a new tile processing with a 'reduction' clause for the auto mixer computing
    if (blackwhite) {//channel-mixer
        int tW = working->getWidth();
        int tH = working->getHeight();

        if (algm == 2) { //channel-mixer
            //end auto chmix
            if (computeMixerAuto) {
                // auto channel-mixer
                double nr = 0;
                double ng = 0;
                double nb = 0;

#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 16) reduction(+:nr,ng,nb)
#endif

                for (int i = 0; i < tH; i++) {
                    for (int j = 0; j < tW; j++) {
                        nr += static_cast<double>(tmpImage->r(i, j));
                        ng += static_cast<double>(tmpImage->g(i, j));
                        nb += static_cast<double>(tmpImage->b(i, j));
                    }
                }

                double srgb = nr + ng + nb;
                double knr = srgb / nr;
                double kng = srgb / ng;
                double knb = srgb / nb;
                double sk = knr + kng + knb;
                autor = (float)(100.0 * knr / sk);
                autog = (float)(100.0 * kng / sk);
                autob = (float)(100.0 * knb / sk);

            }

            if (params->blackwhite.autoc) {
                // auto channel-mixer
                bwr = autor;
                bwg = autog;
                bwb = autob;
                mixerOrange  = 33.f;
                mixerYellow  = 33.f;
                mixerMagenta = 33.f;
                mixerPurple  = 33.f;
                mixerCyan    = 33.f;
            }

            float filcor;
            Color::computeBWMixerConstants(params->blackwhite.setting, params->blackwhite.filter, params->blackwhite.algo, filcor,
                                           bwr, bwg, bwb, mixerOrange, mixerYellow, mixerCyan, mixerPurple, mixerMagenta,
                                           params->blackwhite.autoc, complem, kcorec, rrm, ggm, bbm);

#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic, 16)
#endif

            for (int i = 0; i < tH; i++) {
                for (int j = 0; j < tW; j++) {

                    //mix channel
                    tmpImage->r(i, j) = tmpImage->g(i, j) = tmpImage->b(i, j) = /*CLIP*/ ((bwr * tmpImage->r(i, j) + bwg * tmpImage->g(i, j) + bwb * tmpImage->b(i, j)) * kcorec);

#if ! defined(__SSE2__) && ! defined(RT_SIMDE)

                    //gamma correction: pseudo TRC curve
                    if (hasgammabw) {
                        Color::trcGammaBW(tmpImage->r(i, j), tmpImage->g(i, j), tmpImage->b(i, j), gammabwr, gammabwg, gammabwb);
                    }

#endif
                }

#if defined(__SSE2__) || defined(RT_SIMDE)

                if (hasgammabw) {
                    //gamma correction: pseudo TRC curve
                    Color::trcGammaBWRow(tmpImage->r(i), tmpImage->g(i), tmpImage->b(i), tW, gammabwr, gammabwg, gammabwb);
                }

#endif
            }
        }

        if (editID == EUID_BlackWhiteAfterCurve) {
#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic, 5)
#endif

            for (int i = 0; i < tH; i++) {
                for (int j = 0; j < tW; j++) {
                    editWhatever->v(i, j) = Color::gamma2curve[tmpImage->r(i, j)] / 65535.f;   // assuming that r=g=b
                }
            }
        }

        if (hasToneCurvebw2) {

            if (afterCurveMode == BlackWhiteParams::TcMode::STD_BW) { // Standard
#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 5)
#endif

                for (int i = 0; i < tH; i++) {
                    for (int j = 0; j < tW; j++) {
                        const StandardToneCurve& userToneCurve = static_cast<const StandardToneCurve&>(customToneCurvebw2);
                        userToneCurve.Apply(tmpImage->r(i, j), tmpImage->g(i, j), tmpImage->b(i, j));
                    }
                }
            } else if (afterCurveMode == BlackWhiteParams::TcMode::WEIGHTEDSTD_BW) { // apply the curve to the rgb channels, weighted
#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 5)
#endif

                for (int i = 0; i < tH; i++) { //for ulterior usage if bw data modified
                    for (int j = 0; j < tW; j++) {
                        const WeightedStdToneCurve& userToneCurve = static_cast<const WeightedStdToneCurve&>(customToneCurvebw2);

                        // tmpImage->r (i, j) = CLIP<float> (tmpImage->r (i, j));
                        // tmpImage->g (i, j) = CLIP<float> (tmpImage->g (i, j));
                        // tmpImage->b (i, j) = CLIP<float> (tmpImage->b (i, j));

                        userToneCurve.Apply(tmpImage->r(i, j), tmpImage->g(i, j), tmpImage->b(i, j));
                    }
                }
            }
        }

        //colour toning with black and white
        if (hasColorToning) {
            if (params->colorToning.method == "Splitco") {
                constexpr float reducac = 0.5f;
                constexpr int mode = 1;
#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 5)
#endif

                for (int i = 0; i < tH; i++) {
                    for (int j = 0; j < tW; j++) {
                        const float r = tmpImage->r(i, j);
                        const float g = tmpImage->g(i, j);
                        const float b = tmpImage->b(i, j);

                        const float lumbefore = 0.299f * r + 0.587f * g + 0.114f * b;

                        if (lumbefore < 65000.f  && lumbefore > 500.f) { //reduce artifacts for highlights and extreme shadows
                            float ro, go, bo;
                            toningsmh(r, g, b, ro, go, bo, RedLow, GreenLow, BlueLow, RedMed, GreenMed, BlueMed, RedHigh, GreenHigh, BlueHigh, reducac, mode, strProtect);

                            if (params->colorToning.lumamode) {
                                const float lumafter = 0.299f * ro + 0.587f * go + 0.114f * bo;
                                const float preserv = lumbefore / lumafter;
                                ro *= preserv;
                                go *= preserv;
                                bo *= preserv;
                            }

                            tmpImage->r(i, j) = /*CLIP*/(ro);
                            tmpImage->g(i, j) = /*CLIP*/(go);
                            tmpImage->b(i, j) = /*CLIP*/(bo);
                        }
                    }
                }
            }

            else if (params->colorToning.method == "Splitlr") {
                constexpr float reducac = 0.4f;
                int preser = 0;

                if (params->colorToning.lumamode) {
                    preser = 1;
                }

                const float balanS = 1.f + Balan / 100.f; //balan between 0 and 2
                const float balanH = 1.f - Balan / 100.f;
                float rh, gh, bh;
                float rl, gl, bl;
                float xh, yh, zh;
                float xl, yl, zl;
                const float iplow = ctColorCurve.low;
                const float iphigh = ctColorCurve.high;

                //2 colours
                ctColorCurve.getVal(iphigh, xh, yh, zh);
                ctColorCurve.getVal(iplow, xl, yl, zl);

                Color::xyz2rgb(xh, yh, zh, rh, gh, bh, wip);
                Color::xyz2rgb(xl, yl, zl, rl, gl, bl, wip);

                //retrieve rgb value with s and l =1
                retreavergb(rl, gl, bl);
                const float krl = rl / (rl + gl + bl);
                const float kgl = gl / (rl + gl + bl);
                const float kbl = bl / (rl + gl + bl);

                retreavergb(rh, gh, bh);
                const float krh = rh / (rh + gh + bh);
                const float kgh = gh / (rh + gh + bh);
                const float kbh = bh / (rh + gh + bh);
                constexpr int mode = 1;
#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 5)
#endif

                for (int i = 0; i < tH; i++) {
                    for (int j = 0; j < tW; j++) {
                        toning2col(tmpImage->r(i, j), tmpImage->g(i, j), tmpImage->b(i, j), tmpImage->r(i, j), tmpImage->g(i, j), tmpImage->b(i, j), iplow, iphigh, krl, kgl, kbl, krh, kgh, kbh, SatLow, SatHigh, balanS, balanH, reducac, mode, preser, strProtect);
                    }
                }
            }

            //colortoning with shift color Lab
            else if (params->colorToning.method == "Lab"  && opautili) {
                int algo = 0;
                bool twocol = true;
                int metchrom = 0;

                if (params->colorToning.twocolor == "Std") {
                    metchrom = 0;
                } else if (params->colorToning.twocolor == "All") {
                    metchrom = 1;
                } else if (params->colorToning.twocolor == "Separ") {
                    metchrom = 2;
                } else if (params->colorToning.twocolor == "Two") {
                    metchrom = 3;
                }

                if (metchrom == 3) {
                    twocol = false;
                }

                float iplow = 0.f, iphigh = 0.f;

                if (!twocol) {
                    iplow = (float)ctColorCurve.low;
                    iphigh = (float)ctColorCurve.high;

                }

                int twoc = 0; //integer instead of bool to let more possible choice...other than 2 and 500.

                if (!twocol) {
                    twoc = 0;    // 2 colours
                } else {
                    twoc = 1;    // 500 colours
                }

                if (params->colorToning.method == "Lab") {
                    algo = 1;
                } else if (params->colorToning.method == "Lch") {
                    algo = 2;    //in case of
                }

                if (algo <= 2) {
#ifdef _OPENMP
                    #pragma omp parallel for schedule(dynamic, 5)
#endif

                    for (int i = 0; i < tH; i++) {
                        for (int j = 0; j < tW; j++) {
                            float r = tmpImage->r(i, j);
                            float g = tmpImage->g(i, j);
                            float b = tmpImage->b(i, j);
                            float ro, bo, go;
                            labtoning(r, g, b, ro, go, bo, algo, metchrom,  twoc, satLimit, satLimitOpacity, ctColorCurve,  ctOpacityCurve, clToningcurve, cl2Toningcurve,  iplow, iphigh,  wp,  wip);
                            setUnlessOOG(tmpImage->r(i, j), tmpImage->g(i, j), tmpImage->b(i, j), ro, go, bo);
                        }
                    }
                }
            }

            else if (params->colorToning.method.substr(0, 3) == "RGB"  && opautili) {
                // color toning
#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 5)
#endif

                for (int i = 0; i < tH; i++) {
                    for (int j = 0; j < tW; j++) {
                        float r = tmpImage->r(i, j);
                        float g = tmpImage->g(i, j);
                        float b = tmpImage->b(i, j);

                        // Luminance = (0.299f*r + 0.587f*g + 0.114f*b)

                        float s, l;
                        Color::rgb2slfloat(r, g, b, s, l);

                        float l_ = Color::gammatab_srgb1[l * 65535.f];

                        // get the opacity and tweak it to preserve saturated colours
                        float opacity = ctOpacityCurve.lutOpacityCurve[l_ * 500.f] / 4.f;

                        float r2, g2, b2;
                        ctColorCurve.getVal(l_, r2, g2, b2); // get the colour from the colour curve

                        float h2, s2, l2;
                        Color::rgb2hslfloat(r2, g2, b2, h2, s2, l2); // transform this new colour to hsl

                        Color::hsl2rgbfloat(h2, s2, l, r2, g2, b2);

                        tmpImage->r(i, j) = intp(opacity, r2, r);
                        tmpImage->g(i, j) = intp(opacity, g2, g);
                        tmpImage->b(i, j) = intp(opacity, b2, b);
                    }
                }
            }
        }

        // filling the pipette buffer by the content of the temp pipette buffers
        // due to optimization, we have to test now if the pipette has been filled in the second tile loop, by
        // testing editID
        /*if (editImgFloat) {
            for (int i=istart,ti=0; i<tH; i++,ti++)
                for (int j=jstart,tj=0; j<tW; j++,tj++) {
                    editImgFloat->r(i,j) = editIFloatTmpR[ti*TS+tj];
                    editImgFloat->g(i,j) = editIFloatTmpG[ti*TS+tj];
                    editImgFloat->b(i,j) = editIFloatTmpB[ti*TS+tj];
                }
        }
        else*/
        /*
        if (editWhatever && (editID==EUID_BlackWhiteAfterCurve)) {
            for (int i=istart,ti=0; i<tH; i++,ti++)
                for (int j=jstart,tj=0; j<tW; j++,tj++) {
                    editWhatever->v(i,j) = editWhateverTmp[ti*TS+tj];
                }
        }
        */

        // ready, fill lab (has to be the same code than the "fill lab" above!)

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic, 5)
#endif

        for (int i = 0; i < tH; i++) {
            Color::RGB2Lab(tmpImage->r(i), tmpImage->g(i), tmpImage->b(i), lab->L[i], lab->a[i], lab->b[i], toxyz, tW);

            // if (hasColorToningLabGrid) {
            // colorToningLabGrid(lab, 0, tW, i, i + 1, false);
            // }
        }


    }

    if (hCurveEnabled) {
        delete hCurve;
    }

    if (sCurveEnabled) {
        delete sCurve;
    }

    if (vCurveEnabled) {
        delete vCurve;
    }

}

/**
* @brief retreave RGB value with maximum saturation
* @param r red input and in exit new r
* @param g green input and in exit new g
* @param b blue input and in exit new b
**/
void ImProcFunctions::retreavergb(float &r, float &g, float &b)
{
    float mini = min(r, g, b);
    float maxi = max(r, g, b);
    float kkm = 65535.f / maxi;

    if (b == mini && r == maxi) {
        r = 65535.f;
        g = kkm * (g - b);
        b = 0.f;
    } else if (b == mini && g == maxi) {
        g = 65535.f;
        r = kkm * (r - b);
        b = 0.f;
    } else if (g == mini && r == maxi) {
        r = 65535.f;
        b = kkm * (b - g);
        g = 0.f;
    } else if (g == mini && b == maxi) {
        b = 65535.f;
        r = kkm * (r - g);
        g = 0.f;
    } else if (r == mini && b == maxi) {
        b = 65535.f;
        g = kkm * (g - r);
        r = 0.f;
    } else if (r == mini && g == maxi) {
        g = 65535.f;
        b = kkm * (b - r);
        r = 0.f;
    }
}

/**
* @brief Interpolate by decreasing with a parabol k = aa*v*v + bb*v +c  v[0..1]
* @param reducac value of the reduction in the middle of the range
* @param vinf value [0..1] for beginning decrease
* @param aa second degree parameter
* @param bb first degree parameter
* @param cc third parameter
**/
void ImProcFunctions::secondeg_end(float reducac, float vinf, float &aa, float &bb, float &cc)
{
    float zrd = reducac; //value at me  linear =0.5
    float v0 = vinf; //max shadows
    float me = (1.f + v0) / 2.f; //"median" value = (v0 + 1.=/2)
    //float a1=1.f-v0;
    float a2 = me - v0;
    float a3 = 1.f - v0 * v0;
    float a4 = me * me - v0 * v0;
    aa = (1.f + (zrd - 1.f) * (1 - v0) / a2) / (a4 * (1.f - v0) / a2 - a3);
    bb = - (1.f + a3 * aa) / (1.f - v0);
    cc = - (aa + bb);
}

/**
* @brief Interpolate by increasing with a parabol k = aa*v*v + bb*v  v[0..1]
* @param reducac value of the reduction in the middle of the range
* @param vend value [0..1] for beginning increase
* @param aa second degree parameter
* @param bb first degree parameter
**/
void ImProcFunctions::secondeg_begin(float reducac, float vend, float &aam, float &bbm)
{
    aam = (2.f - 4.f * reducac) / (vend * vend);
    bbm = 1.f / vend - aam * vend;
}


/**
* @brief color toning with 9 sliders shadows middletones highlight
* @param r red input values [0..65535]
* @param g green input values [0..65535]
* @param b blue input values [0..65535]
* @param ro red output values [0..65535]
* @param go green output values [0..65535]
* @param bo blue output values [0..65535]
* @param RedLow    [-1..1] value after transformations of sliders [-100..100] for shadows
* @param GreenLow  [-1..1] value after transformations of sliders [-100..100] for shadows
* @param BlueLow   [-1..1] value after transformations of sliders [-100..100] for shadows
* @param RedMed    [-1..1] value after transformations of sliders [-100..100] for midtones
* @param GreenMed  [-1..1] value after transformations of sliders [-100..100] for midtones
* @param BlueMed   [-1..1] value after transformations of sliders [-100..100] for midtones
* @param RedHigh   [-1..1] value after transformations of sliders [-100..100] for highlights
* @param GreenHigh [-1..1] value after transformations of sliders [-100..100] for highlights
* @param BlueHigh  [-1..1] value after transformations of sliders [-100..100] for highlights
* @param reducac value of the reduction in the middle of the range for second degree increase or decrease action
* @param mode 0 = colour, 1 = Black and White
* @param strProtect ?
**/
void ImProcFunctions::toningsmh(float r, float g, float b, float &ro, float &go, float &bo, float RedLow, float GreenLow, float BlueLow, float RedMed, float GreenMed, float BlueMed, float RedHigh, float GreenHigh, float BlueHigh, float reducac, int mode, float strProtect)
{
    const float v = max(r, g, b) / 65535.f;
    float kl = 1.f;
    float rlo; //0.4  0.5
    float rlm; //1.1
    float rlh; //1.1

    if (mode == 0) { //colour
        rlo = strProtect; //0.5 ==>  0.75
        rlh = 2.2f * strProtect;
        rlm = 1.5f * strProtect;
        constexpr float v0 = 0.15f;
        //second degree

        if (v > v0) {
            float aa, bb, cc;
            secondeg_end(reducac, v0, aa, bb, cc);
            kl = aa * v * v + bb * v + cc;    //verified ==> exact
        } else {
            float aab, bbb;
            secondeg_begin(0.7f, v0, aab, bbb);
            kl = aab * v * v + bbb * v;
        }
    } else { //bw coefficient to preserve same results as before for satlimtopacity = 0.5 (default)
        rlo = strProtect * 0.8f; //0.4
        rlm = strProtect * 2.2f; //1.1
        rlh = strProtect * 2.4f; //1.2

        if (v > 0.15f) {
            kl = (-1.f / 0.85f) * v + 1.f / 0.85f;    //Low light ==> decrease action after v=0.15
        }
    }

    {
        const float corr = 20000.f * RedLow * kl * rlo;

        if (RedLow > 0.f) {
            r += corr;
        } else {
            g -= corr;
            b -= corr;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    {
        const float corr = 20000.f * GreenLow * kl * rlo;

        if (GreenLow > 0.f) {
            g += corr;
        } else {
            r -= corr;
            b -= corr;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }


    {
        const float corr = 20000.f * BlueLow * kl * rlo;

        if (BlueLow > 0.f) {
            b += corr;
        } else {
            r -= corr;
            g -= corr;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    // mid tones
    float km;
    constexpr float v0m = 0.5f; //max action

    if (v < v0m) {
        float aam, bbm;
        float vend = v0m;
        secondeg_begin(reducac, vend, aam, bbm);
        km = aam * v * v + bbm * v; //verification = good
    } else {
        float v0mm = 0.5f; //max
        float aamm, bbmm, ccmm;
        secondeg_end(reducac, v0mm, aamm, bbmm, ccmm);
        km = aamm * v * v + bbmm * v + ccmm; //verification good
    }

    {
        const float RedM = RedMed * km * rlm;

        if (RedMed > 0.f) {
            r += 20000.f * RedM;
            g -= 10000.f * RedM;
            b -= 10000.f * RedM;
        } else {
            r += 10000.f * RedM;
            g -= 20000.f * RedM;
            b -= 20000.f * RedM;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    {
        const float GreenM = GreenMed * km * rlm;

        if (GreenMed > 0.f) {
            r -= 10000.f * GreenM;
            g += 20000.f * GreenM;
            b -= 10000.f * GreenM;
        } else {
            r -= 20000.f * GreenM;
            g += 10000.f * GreenM;
            b -= 20000.f * GreenM;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    {
        const float BlueM = BlueMed * km * rlm;

        if (BlueMed > 0.f) {
            r -= 10000.f * BlueM;
            g -= 10000.f * BlueM;
            b += 20000.f * BlueM;
        } else {
            r -= 20000.f * BlueM;
            g -= 20000.f * BlueM;
            b += 10000.f * BlueM;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    //high tones
    constexpr float v00 = 0.8f; //max action
    float aa0, bb0;
    secondeg_begin(reducac, v00, aa0, bb0);

    float kh;

    if (v > v00) { //max action
        kh = (1.f - v) / (1.f - v00);    //High tones
    } else {
        kh = v * (aa0 * v + bb0);    //verification = good
    }

    {
        const float corr = 20000.f * RedHigh * kh * rlh; //1.2

        if (RedHigh > 0.f) {
            r += corr;
        } else {
            g -= corr;
            b -= corr;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    {
        const float corr = 20000.f * GreenHigh * kh * rlh; //1.2

        if (GreenHigh > 0.f) {
            g += corr;
        } else {
            r -= corr;
            b -= corr;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    {
        const float corr = 20000.f * BlueHigh * kh * rlh; //1.2

        if (BlueHigh > 0.f) {
            b += corr;
        } else {
            r -= corr;
            g -= corr;
        }

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    ro = r;
    go = g;
    bo = b;
}

/**
* @brief color toning with 2 colors - 2 sliders saturation shadows and highlight and one balance
* @param r g b input values [0..65535]
* @param ro go bo output values [0..65535]
* @param iplow iphigh [0..1] from curve color - value of luminance shadows and highlights
* @param rl gl bl [0..65535] - color of reference shadow
* @param rh gh bh [0..65535] - color of reference highlight
* @param SatLow SatHigh [0..1] from sliders saturation shadows and highlight
* @param balanS [0..1] balance for shadows (one slider)
* @param balanH [0..1] balance for highlights (same slider than for balanS)
* @param reducac value of the reduction in the middle of the range for second degree, increase or decrease action
**/
void ImProcFunctions::toning2col(float r, float g, float b, float &ro, float &go, float &bo, float iplow, float iphigh, float krl, float kgl, float kbl, float krh, float kgh, float kbh, float SatLow, float SatHigh, float balanS, float balanH, float reducac, int mode, int preser, float strProtect)
{
    const float lumbefore = 0.299f * r + 0.587f * g + 0.114f * b;
    const float v = max(r, g, b) / 65535.f;

    const float rlo = strProtect;  //0.5 ==> 0.75  transferred value for more action
    const float rlh = 2.2f * strProtect;

    //low tones
    //second degree
    float aa, bb, cc;
    //fixed value of reducac =0.4;
    secondeg_end(reducac, iplow, aa, bb, cc);

    float aab, bbb;
    secondeg_begin(0.7f, iplow, aab, bbb);

    if (SatLow > 0.f) {
        float kl = 1.f;

        if (v > iplow) {
            kl = aa * v * v + bb * v + cc;
        } else if (mode == 0) {
            kl = aab * v * v + bbb * v;
        }

        const float kmgb = min(r, g, b);

        if (kmgb < 20000.f) {
            //I have tested ...0.85 compromise...
            kl *= pow_F((kmgb / 20000.f), 0.85f);
        }

        const float factor = 20000.f * SatLow * kl * rlo * balanS;

        if (krl > 0.f) {
            g -= factor * krl;
            b -= factor * krl;
        }

        // g = CLIP(g);
        // b = CLIP(b);

        if (kgl > 0.f) {
            r -= factor * kgl;
            b -= factor * kgl;
        }

        // r = CLIP(r);
        // b = CLIP(b);

        if (kbl > 0.f) {
            r -= factor * kbl;
            g -= factor * kbl;
        }

        // r = CLIP(r);
        // g = CLIP(g);
    }

    //high tones
    float aa0, bb0;
    //fixed value of reducac ==0.4;
    secondeg_begin(reducac, iphigh, aa0, bb0);

    if (SatHigh > 0.f) {
        float kh = 1.f;

        if (v > iphigh) {
            kh = (1.f - v) / (1.f - iphigh);    //Low light ==> decrease action after iplow
        } else {
            kh = aa0 * v * v + bb0 * v;
        }

        const float kmgb = max(r, g, b);

        if (kmgb > 45535.f) {
            constexpr float cora = 1.f / (45535.f - 65535.f);
            constexpr float corb = 1.f - cora * 45535.f;
            kh *= kmgb * cora + corb;
        }

        const float factor = 20000.f * SatHigh * kh * rlh * balanH;
        r += factor * (krh > 0.f ? krh : 0.f);
        g += factor * (kgh > 0.f ? kgh : 0.f);
        b += factor * (kbh > 0.f ? kbh : 0.f);

        // r = CLIP(r);
        // g = CLIP(g);
        // b = CLIP(b);
    }

    float preserv = 1.f;

    if (preser == 1) {
        float lumafter = 0.299f * r + 0.587f * g + 0.114f * b;
        preserv = lumbefore / lumafter;
    }

    setUnlessOOG(ro, go, bo, CLIP(r * preserv), CLIP(g * preserv), CLIP(b * preserv));
}

/**
* @brief color toning with interpolation in mode Lab
* @param r g b input values [0..65535]
* @param ro go bo output values [0..65535]
* @param algm  metchrom twoc - methods
* @param ctColorCurve curve 500 colors
* @param ctOpacityCurve curve standard 'ab'
* @param clToningcurve  curve special 'ab' and 'a'
* @param cl2Toningcurve curve special 'b'
* @param iplow iphigh [0..1] luminance
* @param wp wip 3x3 matrix and inverse conversion rgb XYZ
**/
void ImProcFunctions::labtoning(float r, float g, float b, float &ro, float &go, float &bo, int algm, int metchrom, int twoc, float satLimit, float satLimitOpacity, const ColorGradientCurve & ctColorCurve, const OpacityCurve & ctOpacityCurve, const LUTf & clToningcurve, const LUTf & cl2Toningcurve, float iplow, float iphigh, double wp[3][3], double wip[3][3])
{
    ro = CLIP(r);
    go = CLIP(g);
    bo = CLIP(b);

    float realL;
    float h, s, l;
    Color::rgb2hsl(ro, go, bo, h, s, l);
    float x2, y2, z2;
    float xl, yl, zl;

    if (twoc != 1) {
        l = (Color::gammatab_13_2[     l * 65535.f]) / 65535.f; //to compensate L from Lab
        iphigh = (Color::gammatab_13_2[iphigh * 65535.f]) / 65535.f;
        iplow  = (Color::gammatab_13_2[ iplow * 65535.f]) / 65535.f;
    }

    if (twoc == 1) {
        ctColorCurve.getVal(l, x2, y2, z2);
    } else {
        ctColorCurve.getVal(iphigh, x2, y2, z2);
        ctColorCurve.getVal(iplow, xl, yl, zl);
    }

    realL = l;


    //float opacity = ctOpacityCurve.lutOpacityCurve[l*500.f];
    //if(params->blackwhite.enabled){satLimit=80.f;satLimitOpacity=30.f;}//force BW

    // get the opacity and tweak it to preserve saturated colors
    //float l_ = Color::gamma_srgb(l*65535.f)/65535.f;
    float opacity = (1.f - min<float> (s / satLimit, 1.f) * (1.f - satLimitOpacity)) * ctOpacityCurve.lutOpacityCurve[l * 500.f];
    float opacity2 = (1.f - min<float> (s / satLimit, 1.f) * (1.f - satLimitOpacity));

    l *= 65535.f;
    float chromat = 0.f, luma = 0.f;

    if (clToningcurve[l] < l) {
        chromat = clToningcurve[l] / l - 1.f;  //special effect
    } else if (clToningcurve[l] > l) {
        chromat = 1.f - SQR(SQR(l / clToningcurve[l])); //apply C=f(L) acts  on 'a' and 'b'
    }

    if (cl2Toningcurve[l] < l) {
        luma = cl2Toningcurve[l] / l - 1.f;  //special effect
    } else if (cl2Toningcurve[l] > l) {
        luma = 1.f - SQR(SQR(l / cl2Toningcurve[l])); //apply C2=f(L) acts only on 'b'
    }

    if (algm == 1) {
        Color::interpolateRGBColor(realL, iplow, iphigh, algm, opacity, twoc, metchrom, chromat, luma, r, g, b, xl, yl, zl, x2, y2, z2, wp, wip, ro, go, bo);
    } else {
        Color::interpolateRGBColor(realL, iplow, iphigh, algm, opacity2, twoc, metchrom, chromat, luma, r, g, b, xl, yl, zl, x2, y2, z2, wp, wip, ro, go, bo);
    }
}


void ImProcFunctions::luminanceCurve(LabImage* lold, LabImage* lnew, const LUTf& curve)
{

    int W = lold->W;
    int H = lold->H;

#ifdef _OPENMP
    #pragma omp parallel for if (multiThread)
#endif

    for (int i = 0; i < H; i++)
        for (int j = 0; j < W; j++) {
            float Lin = lold->L[i][j];
            //if (Lin>0 && Lin<65535)
            lnew->L[i][j] = curve[Lin];
        }
}



void ImProcFunctions::chromiLuminanceCurve(PipetteBuffer *pipetteBuffer, int pW, LabImage* lold, LabImage* lnew, const LUTf& acurve, const LUTf& bcurve, const LUTf& satcurve, const LUTf& lhskcurve, const LUTf& clcurve, LUTf & curve, bool utili, bool autili, bool butili, bool ccutili, bool cclutili, bool clcutili, LUTu &histCCurve, LUTu &histLCurve)
{
    int W = lold->W;
    int H = lold->H;

    PlanarWhateverData<float>* editWhatever = nullptr;
    EditUniqueID editID = EUID_None;
    bool editPipette = false;

    if (pipetteBuffer) {
        editID = pipetteBuffer->getEditID();

        if (editID != EUID_None) {

            switch (pipetteBuffer->getDataProvider()->getCurrSubscriber()->getPipetteBufferType()) {
                case (BT_IMAGEFLOAT):
                    break;

                case (BT_LABIMAGE):
                    break;

                case (BT_SINGLEPLANE_FLOAT):
                    editPipette = true;
                    editWhatever = pipetteBuffer->getSinglePlaneBuffer();
                    break;
            }
        }
    }

    //-------------------------------------------------------------------------
    // support for pipettes for the new LabRegions color toning mode this is a
    // hack to fill the pipette buffers also when
    // !params->labCurve.enabled. It is ugly, but it's the smallest code
    // change that I could find
    //-------------------------------------------------------------------------
    class TempParams
    {
        const ProcParams **p_;
        const ProcParams *old_;
        ProcParams tmp_;

    public:
        explicit TempParams(const ProcParams **p): p_(p)
        {
            old_ = *p;
            tmp_.labCurve.enabled = true;
            *p_ = &tmp_;
        }

        ~TempParams()
        {
            *p_ = old_;
        }
    };
    std::unique_ptr<TempParams> tempparams;
    bool pipette_for_colortoning_labregions =
        editPipette &&
        params->colorToning.enabled && params->colorToning.method == "LabRegions";

    if (!params->labCurve.enabled && pipette_for_colortoning_labregions) {
        utili = autili = butili = ccutili = cclutili = clcutili = false;
        tempparams.reset(new TempParams(&params));
        curve.makeIdentity();
    }

    //-------------------------------------------------------------------------


    if (!params->labCurve.enabled) {
        if (editPipette && (editID == EUID_Lab_LCurve || editID == EUID_Lab_aCurve || editID == EUID_Lab_bCurve || editID == EUID_Lab_LHCurve || editID == EUID_Lab_CHCurve || editID == EUID_Lab_HHCurve || editID == EUID_Lab_CLCurve || editID == EUID_Lab_CCurve || editID == EUID_Lab_LCCurve)) {
            // fill pipette buffer with zeros to avoid crashes
            editWhatever->fill(0.f);
        }

        if (params->blackwhite.enabled && !params->colorToning.enabled) {
            for (int i = 0; i < lnew->H; ++i) {
                for (int j = 0; j < lnew->W; ++j) {
                    lnew->a[i][j] = lnew->b[i][j] = 0.f;
                }
            }
        }

        return;
    }

    // lhskcurve.dump("lh_curve");
    //init Flatcurve for C=f(H)


    FlatCurve* chCurve = nullptr;// curve C=f(H)
    bool chutili = false;

    if (params->labCurve.chromaticity > -100) {
        chCurve = new FlatCurve(params->labCurve.chcurve);

        if (chCurve->isIdentity()) {
            delete chCurve;
            chCurve = nullptr;
        }//do not use "Munsell" if Chcurve not used
        else {
            chutili = true;
        }
    }

    FlatCurve* lhCurve = nullptr;//curve L=f(H)
    bool lhutili = false;

    if (params->labCurve.chromaticity > -100) {
        lhCurve = new FlatCurve(params->labCurve.lhcurve);

        if (lhCurve->isIdentity()) {
            delete lhCurve;
            lhCurve = nullptr;
        }//do not use "Munsell" if Chcurve not used
        else {
            lhutili = true;
        }
    }

    FlatCurve* hhCurve = nullptr;//curve H=f(H)
    bool hhutili = false;

    if (params->labCurve.chromaticity > -100) {
        hhCurve = new FlatCurve(params->labCurve.hhcurve);

        if (hhCurve->isIdentity()) {
            delete hhCurve;
            hhCurve = nullptr;
        }//do not use "Munsell" if Chcurve not used
        else {
            hhutili = true;
        }
    }


    float adjustr = 1.0f;

//  if(params->labCurve.avoidclip ){
    // parameter to adapt curve C=f(C) to gamut

    if (params->icm.workingProfile == "ProPhoto")   {
        adjustr = 1.2f;   // 1.2 instead 1.0 because it's very rare to have C>170..
    } else if (params->icm.workingProfile == "Adobe RGB")  {
        adjustr = 1.8f;
    } else if (params->icm.workingProfile == "sRGB")       {
        adjustr = 2.0f;
    } else if (params->icm.workingProfile == "WideGamut")  {
        adjustr = 1.2f;
    } else if (params->icm.workingProfile == "Beta RGB")   {
        adjustr = 1.4f;
    } else if (params->icm.workingProfile == "BestRGB")    {
        adjustr = 1.4f;
    } else if (params->icm.workingProfile == "BruceRGB")   {
        adjustr = 1.8f;
    }

    const float histLFactor = pW != 1 ? histLCurve.getSize() / 100.f : 1.f;
    const float histCFactor = pW != 1 ? histCCurve.getSize() * adjustr / 65536.f : 1.f;

    // reference to the params structure has to be done outside of the parallelization to avoid CPU cache problem
    const bool highlight = params->toneCurve.hrenabled; //Get the value if "highlight reconstruction" is activated
    const int chromaticity = params->labCurve.chromaticity;
    const float chromapro = (chromaticity + 100.0f) / 100.0f;
    const bool bwonly = params->blackwhite.enabled && !params->colorToning.enabled;
    bool bwq = false;
//  if(params->ppVersion > 300  && params->labCurve.chromaticity == - 100) bwq = true;
    // const bool bwToning = params->labCurve.chromaticity == - 100  /*|| params->blackwhite.method=="Ch" || params->blackwhite.enabled */ || bwonly;
    const bool bwToning = bwq  /*|| params->blackwhite.method=="Ch" || params->blackwhite.enabled */ || bwonly;
    //if(chromaticity==-100) chromaticity==-99;
    const bool LCredsk = params->labCurve.lcredsk;
    const bool ccut = ccutili;
    const bool clut = clcutili;
    const double rstprotection = 100. - params->labCurve.rstprotection; // Red and Skin Tones Protection
    // avoid color shift is disabled when bwToning is activated and enabled if gamut is true in colorappearanace
    // const bool avoidColorShift = (params->labCurve.avoidcolorshift || (params->colorappearance.gamut && params->colorappearance.enabled)) && !bwToning ;
    //const bool avoidColorS = params->labCurve.avoidcolorshift;
    int gamutmuns = 0;

    if (params->labCurve.gamutmunselmethod == "NONE") {
        gamutmuns = 0;
    } else if (params->labCurve.gamutmunselmethod == "LAB") {
        gamutmuns = 1;
    } else if (params->labCurve.gamutmunselmethod == "XYZ") {
        gamutmuns = 2;
    } else if (params->labCurve.gamutmunselmethod == "XYZREL") {
        gamutmuns = 3;
    } else if (params->labCurve.gamutmunselmethod == "MUN") {
        gamutmuns = 4;
    }

    const float protectRed = (float)settings->protectred;
    const double protectRedH = settings->protectredh;
    const float protect_red = rtengine::LIM<float>(protectRed, 20.f, 180.f); //default=60  chroma: one can put more or less if necessary...in 'option'  40...160

    // default=0.4 rad : one can put more or less if necessary...in 'option'  0.2 ..1.0
    // avoid divide by 0 and negatives values
    // avoid too big values
    const float protect_redh = rtengine::LIM<float>(protectRedH, 0.1f, 1.f);

    // default=0.4 rad : one can put more or less if necessary...in 'option'  0.2 ..1
    // avoid divide by 0 and negatives values:minimal protection for transition
    // avoid too big values
    const float protect_redhcurg = rtengine::LIM<float>(protectRedH, 0.1f, 3.5f);

    //increase saturation after denoise : ...approximation
    float factnoise = 1.f;

    if (params->dirpyrDenoise.enabled) {
        factnoise = 1.0 + params->dirpyrDenoise.chroma / 500.0; //levels=5
    }

    const float scaleConst = 100.0f / 100.1f;


    //const bool gamutLch = settings->gamutLch;
    const float amountchroma = (float) settings->amchroma;

    TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(params->icm.workingProfile);
    const double wip[3][3] = {
        {wiprof[0][0], wiprof[0][1], wiprof[0][2]},
        {wiprof[1][0], wiprof[1][1], wiprof[1][2]},
        {wiprof[2][0], wiprof[2][1], wiprof[2][2]}
    };

    TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);
    const double wp[3][3] = {
        {wprof[0][0], wprof[0][1], wprof[0][2]},
        {wprof[1][0], wprof[1][1], wprof[1][2]},
        {wprof[2][0], wprof[2][1], wprof[2][2]}
    };

#ifdef _OPENMP
    #pragma omp parallel if (multiThread)
#endif
    {
#if defined(__SSE2__) || defined(RT_SIMDE)
        float HHBuffer[W] ALIGNED16;
        float CCBuffer[W] ALIGNED16;
#endif
#ifdef _OPENMP
        #pragma omp for schedule(dynamic, 16)
#endif

        for (int i = 0; i < H; i++) {
            // if (avoidColorShift)

            // only if user activate Lab adjustments
            //   if (autili || butili || ccutili ||  cclutili || chutili || lhutili || hhutili || clcutili || utili || chromaticity) {
            //       Color::LabGamutMunsell(lold->L[i], lold->a[i], lold->b[i], W, /*corMunsell*/true, /*lumaMuns*/false, params->toneCurve.hrenabled, /*gamut*/true, wip);
            //   }

#if defined(__SSE2__) || defined(RT_SIMDE)

            // precalculate some values using SSE
            if (bwToning || (!autili && !butili)) {
                __m128 c327d68v = _mm_set1_ps(327.68f);
                __m128 av, bv;
                int k;

                for (k = 0; k < W - 3; k += 4) {
                    av = LVFU(lold->a[i][k]);
                    bv = LVFU(lold->b[i][k]);
                    STVF(HHBuffer[k], xatan2f(bv, av));
                    STVF(CCBuffer[k], vsqrtf(SQRV(av) + SQRV(bv)) / c327d68v);
                }

                for (; k < W; k++) {
                    HHBuffer[k] = xatan2f(lold->b[i][k], lold->a[i][k]);
                    CCBuffer[k] = sqrt(SQR(lold->a[i][k]) + SQR(lold->b[i][k])) / 327.68f;
                }
            }

#endif // __SSE2__

            for (int j = 0; j < W; j++) {
                const float Lin = lold->L[i][j];
                float LL = Lin / 327.68f;
                float CC;
                float HH;
                float Chprov;
                float Chprov1;
                float memChprov;
                float2 sincosval;

                if (bwToning) { // this values will be also set when bwToning is false some lines down
#if defined(__SSE2__) || defined(RT_SIMDE)
                    // use precalculated values from above
                    HH = HHBuffer[j];
                    CC = CCBuffer[j];
#else
                    HH = xatan2f(lold->b[i][j], lold->a[i][j]);
                    CC = sqrt(SQR(lold->a[i][j]) + SQR(lold->b[i][j])) / 327.68f;
#endif

                    // According to mathematical laws we can get the sin and cos of HH by simple operations
                    if (CC == 0.0f) {
                        sincosval.y = 1.0f;
                        sincosval.x = 0.0f;
                    } else {
                        sincosval.y = lold->a[i][j] / (CC * 327.68f);
                        sincosval.x = lold->b[i][j] / (CC * 327.68f);
                    }

                    Chprov = CC;
                    Chprov1 = CC;
                    memChprov = Chprov;
                }

                if (editPipette && editID == EUID_Lab_LCurve) {
                    editWhatever->v(i, j) = LIM01<float> (Lin / 32768.0f);   // Lab L pipette
                }

                lnew->L[i][j] = curve[Lin];

                float Lprov1 = (lnew->L[i][j]) / 327.68f;

                if (editPipette) {
                    if (editID == EUID_Lab_aCurve) { // Lab a pipette
                        float chromapipa = lold->a[i][j] + 32768.f;
                        editWhatever->v(i, j) = LIM01<float> ((chromapipa) / (65536.f));
                    } else if (editID == EUID_Lab_bCurve) { //Lab b pipette
                        float chromapipb = lold->b[i][j] + 32768.f;
                        editWhatever->v(i, j) = LIM01<float> ((chromapipb) / (65536.f));
                    }
                }

                float atmp, btmp;

                atmp = lold->a[i][j];

                if (autili) {
                    atmp = acurve[atmp + 32768.0f] - 32768.0f;    // curves Lab a
                }

                btmp = lold->b[i][j];

                if (butili) {
                    btmp = bcurve[btmp + 32768.0f] - 32768.0f;    // curves Lab b
                }

                if (!bwToning) { //take into account modification of 'a' and 'b'
#if defined(__SSE2__) || defined(RT_SIMDE)
                    if (!autili && !butili) {
                        // use precalculated values from above
                        HH = HHBuffer[j];
                        CC = CCBuffer[j];
                    } else {
                        CC = sqrt(SQR(atmp) + SQR(btmp)) / 327.68f;
                        HH = xatan2f(btmp, atmp);
                    }

#else
                    CC = sqrt(SQR(atmp) + SQR(btmp)) / 327.68f;
                    HH = xatan2f(btmp, atmp);
#endif

                    // According to mathematical laws we can get the sin and cos of HH by simple operations
                    //float2  sincosval;
                    if (CC == 0.f) {
                        sincosval.y = 1.f;
                        sincosval.x = 0.f;
                    } else {
                        sincosval.y = atmp / (CC * 327.68f);
                        sincosval.x = btmp / (CC * 327.68f);
                    }

                    Chprov = CC;
                    Chprov1 = CC;
                    memChprov = Chprov;
                } // now new values of lold with 'a' and 'b'

                if (editPipette)
                    if (editID == EUID_Lab_LHCurve || editID == EUID_Lab_CHCurve || editID == EUID_Lab_HHCurve) {//H pipette
                        float valpar = Color::huelab_to_huehsv2(HH);
                        editWhatever->v(i, j) = valpar;
                    }

                if (lhutili) {  // L=f(H)
                    const float ClipLevel = 65535.f;
                    float l_r;//Luminance Lab in 0..1
                    l_r = Lprov1 / 100.f;
                    {
                        float valparam = lhCurve->getVal(Color::huelab_to_huehsv2(HH)) - 0.5; //get l_r=f(H)
                        //float valparam = float ((lhCurve->getVal (hr - 0.5f)*2));//get l_r=f(H)

                        float valparamneg;
                        valparamneg = valparam;
                        float kcc = (CC / amountchroma); //take Chroma into account...40 "middle low" of chromaticity (arbitrary and simple), one can imagine other algorithme
                        //reduce action for low chroma and increase action for high chroma
                        valparam *= 2.f * kcc;
                        valparamneg *= kcc; //slightly different for negative

                        if (valparam > 0.f) {
                            l_r = (1.f - valparam) * l_r + valparam * (1.f - SQR(((SQR(1.f - min(l_r, 1.0f))))));
                        } else
                            //for negative
                        {
                            float khue = 1.9f; //in reserve in case of!
                            l_r *= (1.f + khue * valparamneg);
                        }
                    }

                    Lprov1 = l_r * 100.f;

                    float Chprov2 = sqrt(SQR(atmp) + SQR(btmp)) / 327.68f;
                    //Gamut control especially for negative values slightly different from gamutlchonly
                    bool inRGB;

                    do {
                        inRGB = true;
                        float aprov1 = Chprov2 * sincosval.y;
                        float bprov1 = Chprov2 * sincosval.x;

                        float fy = (Color::c1By116 * Lprov1) + Color::c16By116;
                        float fx = (0.002f * aprov1) + fy;
                        float fz = fy - (0.005f * bprov1);

                        float x_ = 65535.f * Color::f2xyz(fx) * Color::D50x;
                        float z_ = 65535.f * Color::f2xyz(fz) * Color::D50z;
                        float y_ = Lprov1 > Color::epskapf ? 65535.f * fy * fy * fy : 65535.f * Lprov1 / Color::kappaf;
                        float R, G, B;
                        Color::xyz2rgb(x_, y_, z_, R, G, B, wip);

                        if (R < 0.0f || G < 0.0f || B < 0.0f) {
                            if (Lprov1 < 0.1f) {
                                Lprov1 = 0.1f;
                            }

                            Chprov2 *= 0.95f;
                            inRGB = false;
                        } else if (!highlight && (R > ClipLevel || G > ClipLevel || B > ClipLevel)) {
                            if (Lprov1 > 99.98f) {
                                Lprov1 = 99.98f;
                            }

                            Chprov2 *= 0.95f;
                            inRGB = false;
                        }
                    } while (!inRGB);

                    atmp = 327.68f * Chprov2 * sincosval.y;
                    btmp = 327.68f * Chprov2 * sincosval.x;
                }

//          calculate C=f(H)
                if (chutili) {
                    double hr = Color::huelab_to_huehsv2(HH);
                    float chparam = (chCurve->getVal(hr) - 0.5) * 2.0; //get C=f(H)

                    float chromaChfactor = 1.0f + chparam;
                    atmp *= chromaChfactor;//apply C=f(H)
                    btmp *= chromaChfactor;
                }

                if (hhutili) {  // H=f(H)
                    //hue Lab in -PI +PI
                    float valparam = (hhCurve->getVal(Color::huelab_to_huehsv2(HH)) - 0.5) * 1.7 + static_cast<double>(HH); //get H=f(H)  1.7 optimisation !
                    HH = valparam;
                    sincosval = xsincosf(HH);
                }

                if (!bwToning) {
                    float factorskinc, factorsatc, factorskinextc;

                    if (chromapro > 1.f) {
                        float scale = scaleConst;//reduction in normal zone
                        float scaleext = 1.f;//reduction in transition zone
                        Color::scalered(rstprotection, chromapro, 0.0, HH, protect_redh, scale, scaleext);  //1.0
                        float interm = (chromapro - 1.f);
                        factorskinc = 1.f + (interm * scale);
                        factorskinextc = 1.f + (interm * scaleext);
                    } else {
                        factorskinc = chromapro ; // +(chromapro)*scale;
                        factorskinextc = chromapro ;// +(chromapro)*scaleext;
                    }

                    factorsatc = chromapro * factnoise;

                    //simulate very approximative gamut f(L) : with pyramid transition
                    float dred /*=55.f*/;//C red value limit

                    if (Lprov1 < 25.f) {
                        dred = 40.f;
                    } else if (Lprov1 < 30.f) {
                        dred = 3.f * Lprov1 - 35.f;
                    } else if (Lprov1 < 70.f) {
                        dred = 55.f;
                    } else if (Lprov1 < 75.f) {
                        dred = -3.f * Lprov1 + 265.f;
                    } else {
                        dred = 40.f;
                    }

                    // end pyramid

                    // Test if chroma is in the normal range first
                    Color::transitred(HH, Chprov1, dred, factorskinc, protect_red, factorskinextc, protect_redh, factorsatc, factorsatc);
                    atmp *= factorsatc;
                    btmp *= factorsatc;

                    if (editPipette && editID == EUID_Lab_CLCurve) {
                        editWhatever->v(i, j) = LIM01<float> (LL / 100.f);   // Lab C=f(L) pipette
                    }

                    if (clut && LL > 0.f) { // begin C=f(L)
                        float factorskin, factorsat, factor, factorskinext;
                        float chromaCfactor = (clcurve[LL * 655.35f]) / (LL * 655.35f); //apply C=f(L)
                        float curf = 0.7f; //empirical coeff because curve is more progressive
                        float scale = 100.0f / 100.1f; //reduction in normal zone for curve C
                        float scaleext = 1.0f; //reduction in transition zone for curve C
                        float protect_redcur, protect_redhcur; //perhaps the same value than protect_red and protect_redh
                        float deltaHH;//HH value transition for C curve
                        protect_redcur = curf * protectRed; //default=60  chroma: one can put more or less if necessary...in 'option'  40...160==> curf =because curve is more progressive

                        if (protect_redcur < 20.0f) {
                            protect_redcur = 20.0;    // avoid too low value
                        }

                        if (protect_redcur > 180.0f) {
                            protect_redcur = 180.0;    // avoid too high value
                        }

                        protect_redhcur = curf * float (protectRedH); //default=0.4 rad : one can put more or less if necessary...in 'option'  0.2 ..1.0 ==> curf =because curve is more progressive

                        if (protect_redhcur < 0.1f) {
                            protect_redhcur = 0.1f;    //avoid divide by 0 and negatives values
                        }

                        if (protect_redhcur > 1.0f) {
                            protect_redhcur = 1.0f;    //avoid too big values
                        }

                        deltaHH = protect_redhcur; //transition hue

                        if (chromaCfactor > 0.f) {
                            Color::scalered(rstprotection, chromaCfactor, 0.0, HH, deltaHH, scale, scaleext);      //1.0
                        }

                        if (chromaCfactor > 1.f) {
                            float interm = (chromaCfactor - 1.0f) * 100.0f;
                            factorskin = 1.0f + (interm * scale) / 100.0f;
                            factorskinext = 1.0f + (interm * scaleext) / 100.0f;
                        } else {
                            factorskin = chromaCfactor; // +(1.0f-chromaCfactor)*scale;
                            factorskinext = chromaCfactor ; //+(1.0f-chromaCfactor)*scaleext;
                        }

                        factorsat = chromaCfactor;
                        factor = factorsat;
                        Color::transitred(HH, Chprov1, dred, factorskin, protect_redcur, factorskinext, deltaHH, factorsat, factor);
                        atmp = LIM(atmp * factor, min(-42000.f, atmp), max(42000.f, atmp));
                        btmp = LIM(btmp * factor, min(-42000.f, btmp), max(42000.f, btmp));
                    }

                    // end C=f(L)
                    //  if (editID == EUID_Lab_CLCurve)
                    //      editWhatever->v(i,j) = LIM01<float>(Lprov2/100.f);// Lab C=f(L) pipette

                    // I have placed C=f(C) after all C treatments to assure maximum amplitude of "C"
                    if (editPipette && editID == EUID_Lab_CCurve) {
                        float chromapip = sqrt(SQR(atmp) + SQR(btmp) + 0.001f);
                        editWhatever->v(i, j) = LIM01<float> ((chromapip) / (65536.f / adjustr));
                    }//Lab C=f(C) pipette

                    if (ccut) {
                        float factorskin, factorsat, factor, factorskinext;
                        float chroma = sqrt(SQR(atmp) + SQR(btmp) + 0.001f);
                        float chromaCfactor = (satcurve[chroma * adjustr]) / (chroma * adjustr); //apply C=f(C)
                        float curf = 0.7f; //empirical coeff because curve is more progressive
                        float scale = 100.0f / 100.1f; //reduction in normal zone for curve CC
                        float scaleext = 1.0f; //reduction in transition zone for curve CC
                        float protect_redcur, protect_redhcur; //perhaps the same value than protect_red and protect_redh
                        float deltaHH;//HH value transition for CC curve
                        protect_redcur = curf * protectRed; //default=60  chroma: one can put more or less if necessary...in 'option'  40...160==> curf =because curve is more progressive

                        if (protect_redcur < 20.0f) {
                            protect_redcur = 20.0;    // avoid too low value
                        }

                        if (protect_redcur > 180.0f) {
                            protect_redcur = 180.0;    // avoid too high value
                        }

                        protect_redhcur = curf * float (protectRedH); //default=0.4 rad : one can put more or less if necessary...in 'option'  0.2 ..1.0 ==> curf =because curve is more progressive

                        if (protect_redhcur < 0.1f) {
                            protect_redhcur = 0.1f;    //avoid divide by 0 and negatives values
                        }

                        if (protect_redhcur > 1.0f) {
                            protect_redhcur = 1.0f;    //avoid too big values
                        }

                        deltaHH = protect_redhcur; //transition hue

                        if (chromaCfactor > 0.f) {
                            Color::scalered(rstprotection, chromaCfactor, 0.0, HH, deltaHH, scale, scaleext);      //1.0
                        }

                        if (chromaCfactor > 1.f) {
                            float interm = (chromaCfactor - 1.0f) * 100.0f;
                            factorskin = 1.0f + (interm * scale) / 100.0f;
                            factorskinext = 1.0f + (interm * scaleext) / 100.0f;
                        } else {
                            //factorskin= chromaCfactor*scale;
                            //factorskinext=chromaCfactor*scaleext;
                            factorskin = chromaCfactor; // +(1.0f-chromaCfactor)*scale;
                            factorskinext = chromaCfactor ; //+(1.0f-chromaCfactor)*scaleext;

                        }

                        factorsat = chromaCfactor;
                        factor = factorsat;
                        Color::transitred(HH, Chprov1, dred, factorskin, protect_redcur, factorskinext, deltaHH, factorsat, factor);
                        atmp *= factor;
                        btmp *= factor;
                    }
                }

                // end chroma C=f(C)

                //update histogram C
                if (pW != 1) { //only with improccoordinator
                    histCCurve[histCFactor * sqrt(atmp * atmp + btmp * btmp)]++;
                }

                if (editPipette && editID == EUID_Lab_LCCurve) {
                    float chromapiplc = sqrt(SQR(atmp) + SQR(btmp) + 0.001f);
                    editWhatever->v(i, j) = LIM01<float> ((chromapiplc) / (65536.f / adjustr));
                }//Lab L=f(C) pipette


                if (cclutili && !bwToning) {    //apply curve L=f(C) for skin and rd...but also for extended color ==> near green and blue (see 'curf')

                    const float xx = 0.25f; //soft : between 0.2 and 0.4
                    float skdeltaHH;

                    skdeltaHH = protect_redhcurg; //transition hue

                    float skbeg = -0.05f; //begin hue skin
                    float skend = 1.60f; //end hue skin
                    const float chrmin = 50.0f; //to avoid artifact, because L curve is not a real curve for luminance
                    float aa, bb;
                    float zz = 0.0f;
                    float yy = 0.0f;

                    if (Chprov1 < chrmin) {
                        yy = SQR(Chprov1 / chrmin) * xx;
                    } else {
                        yy = xx;    //avoid artifact for low C
                    }

                    if (!LCredsk) {
                        skbeg = -3.1415;
                        skend = 3.14159;
                        skdeltaHH = 0.001f;
                    }

                    if (HH > skbeg && HH < skend) {
                        zz = yy;
                    } else if (HH > skbeg - skdeltaHH && HH <= skbeg) { //transition
                        aa = yy / skdeltaHH;
                        bb = -aa * (skbeg - skdeltaHH);
                        zz = aa * HH + bb;
                    } else if (HH >= skend && HH < skend + skdeltaHH) { //transition
                        aa = -yy / skdeltaHH;
                        bb = -aa * (skend + skdeltaHH);
                        zz = aa * HH + bb;
                    }

                    float chroma = sqrt(SQR(atmp) + SQR(btmp) + 0.001f);
                    float Lc = (lhskcurve[chroma * adjustr]) / (chroma * adjustr); //apply L=f(C)
                    Lc = (Lc - 1.0f) * zz + 1.0f; //reduct action
                    Lprov1 *= Lc; //adjust luminance
                }

                //update histo LC
                if (pW != 1) { //only with improccoordinator
                    histLCurve[Lprov1 * histLFactor]++;
                }


                // labCurve.bwtoning option allows to decouple modulation of a & b curves by saturation
                // with bwtoning enabled the net effect of a & b curves is visible
                if (bwToning) {
                    atmp -= lold->a[i][j];
                    btmp -= lold->b[i][j];
                    Chprov1 = sqrt(SQR(atmp) + SQR(btmp)) / 327.68f;
                    if (Chprov1 == 0.f) {
                        sincosval.x = 0.f;
                        sincosval.y = 1.f;
                    } else {
                        sincosval.x = btmp / (327.68f * Chprov1);
                        sincosval.y = atmp / (327.68f * Chprov1);
                    }
                } else {
                    Chprov1 = sqrt(SQR(atmp) + SQR(btmp)) / 327.68f;
                }

                lnew->L[i][j] = Lprov1 * 327.68f;
                lnew->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                lnew->b[i][j] = 327.68f * Chprov1 * sincosval.x;

                //gamutmap Lch ==> preserve Hue,but a little slower than gamutbdy for high values...and little faster for low values
                if (gamutmuns == 1) {
                    float R, G, B;
                    //gamut control : Lab values are in gamut
                    Color::gamutLchonly(HH, sincosval, Lprov1, Chprov1, R, G, B, wip, highlight, 0.15f, 0.96f);
                    lnew->L[i][j] = Lprov1 * 327.68f;
                    lnew->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                    lnew->b[i][j] = 327.68f * Chprov1 * sincosval.x;
                }

                if (gamutmuns == 2 || gamutmuns == 3) {

                    float xg, yg, zg;
                    Color::Lab2XYZ(lnew->L[i][j], atmp, btmp, xg, yg, zg);
                    float x0 = xg;
                    float y0 = yg;
                    float z0 = zg;

                    Color::gamutmap(xg, yg, zg, wp);

                    if (gamutmuns == 3) {//0.5f arbitrary coeff
                        xg = xg + 0.5f * (x0 - xg);
                        yg = yg + 0.5f * (y0 - yg);
                        zg = zg + 0.5f * (z0 - zg);
                    }

                    float Lag, aag2, bbg2;
                    Color::XYZ2Lab(xg, yg, zg, Lag, aag2, bbg2);
                    Lprov1 = Lag / 327.68f;
                    HH = xatan2f(bbg2, aag2);
                    Chprov1 = std::sqrt(SQR(aag2) + SQR(bbg2)) / 327.68f;

                    if (Chprov1 == 0.0f) {
                        sincosval.y = 1.f;
                        sincosval.x = 0.0f;
                    } else {
                        sincosval.y = aag2 / (Chprov1 * 327.68f);
                        sincosval.x = bbg2 / (Chprov1 * 327.68f);
                    }

                    lnew->L[i][j] = Lprov1 * 327.68f;
                    lnew->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                    lnew->b[i][j] = 327.68f * Chprov1 * sincosval.x;

                }

                if (gamutmuns > 0) {
                    if (utili || autili || butili || ccut || clut || cclutili || chutili || lhutili || hhutili || clcutili || chromaticity) {
                        float correctionHue = 0.f; // Munsell's correction
                        float correctlum = 0.f;

                        Lprov1 = lnew->L[i][j] / 327.68f;
                        Chprov = sqrt(SQR(lnew->a[i][j]) + SQR(lnew->b[i][j])) / 327.68f;
                        Color::AllMunsellLch(/*lumaMuns*/true, Lprov1, LL, HH, Chprov, memChprov, correctionHue, correctlum);

                        if (correctionHue != 0.f || correctlum != 0.f) {
                            if (fabs(correctionHue) < 0.015f) {
                                HH += correctlum;    // correct only if correct Munsell chroma very little.
                            }

                            /*      if((HH>0.0f && HH < 1.6f)   && memChprov < 70.0f) HH+=correctlum;//skin correct
                                    else if(fabs(correctionHue) < 0.3f) HH+=0.08f*correctlum;
                                    else if(fabs(correctionHue) < 0.2f) HH+=0.25f*correctlum;
                                    else if(fabs(correctionHue) < 0.1f) HH+=0.35f*correctlum;
                                    else if(fabs(correctionHue) < 0.015f) HH+=correctlum;   // correct only if correct Munsell chroma very little.
                            */
                            sincosval = xsincosf(HH + correctionHue);
                        }

                        lnew->a[i][j] = 327.68f * Chprov * sincosval.y; // apply Munsell
                        lnew->b[i][j] = 327.68f * Chprov * sincosval.x;
                    }
                }

                if (gamutmuns == 0) {

//              if(Lprov1 > maxlp) maxlp=Lprov1;
//              if(Lprov1 < minlp) minlp=Lprov1;
                    if (!bwToning) {
                        lnew->L[i][j] = Lprov1 * 327.68f;
//                  float2 sincosval = xsincosf(HH);
                        lnew->a[i][j] = 327.68f * Chprov1 * sincosval.y;
                        lnew->b[i][j] = 327.68f * Chprov1 * sincosval.x;
                    } else {
                        //Luv limiter only
                        lnew->a[i][j] = atmp;
                        lnew->b[i][j] = btmp;
                    }
                }
            }
        }
    } // end of parallelization

    if (chCurve) {
        delete chCurve;
    }

    if (lhCurve) {
        delete lhCurve;
    }

    if (hhCurve) {
        delete hhCurve;
    }

    //  t2e.set();
    //  printf("Chromil took %d nsec\n",t2e.etime(t1e));
}


//#include "cubic.cc"

//void ImProcFunctions::colorCurve (LabImage* lold, LabImage* lnew)
//{

/*    LUT<double> cmultiplier(181021);

    double boost_a = ((float)params->colorBoost.amount + 100.0) / 100.0;
    double boost_b = ((float)params->colorBoost.amount + 100.0) / 100.0;

    double c, amul = 1.0, bmul = 1.0;
    if (boost_a > boost_b) {
        c = boost_a;
        if (boost_a > 0)
            bmul = boost_b / boost_a;
    }
    else {
        c = boost_b;
        if (boost_b > 0)
            amul = boost_a / boost_b;
    }

    if (params->colorBoost.enable_saturationlimiter && c>1.0) {
        // re-generate color multiplier lookup table
        double d = params->colorBoost.saturationlimit / 3.0;
        double alpha = 0.5;
        double threshold1 = alpha * d;
        double threshold2 = c*d*(alpha+1.0) - d;
        for (int i=0; i<=181020; i++) { // lookup table stores multipliers with a 0.25 chrominance resolution
            double chrominance = (double)i/4.0;
            if (chrominance < threshold1)
                cmultiplier[i] = c;
            else if (chrominance < d)
                cmultiplier[i] = (c / (2.0*d*(alpha-1.0)) * (chrominance-d)*(chrominance-d) + c*d/2.0 * (alpha+1.0) ) / chrominance;
            else if (chrominance < threshold2)
                cmultiplier[i] = (1.0 / (2.0*d*(c*(alpha+1.0)-2.0)) * (chrominance-d)*(chrominance-d) + c*d/2.0 * (alpha+1.0) ) / chrominance;
            else
                cmultiplier[i] = 1.0;
        }
    }

    float eps = 0.001;
    double shift_a = params->colorShift.a + eps, shift_b = params->colorShift.b + eps;

    float** oa = lold->a;
    float** ob = lold->b;

    #pragma omp parallel for if (multiThread)
    for (int i=0; i<lold->H; i++)
        for (int j=0; j<lold->W; j++) {

            double wanted_c = c;
            if (params->colorBoost.enable_saturationlimiter && c>1) {
                float chroma = (float)(4.0 * sqrt((oa[i][j]+shift_a)*(oa[i][j]+shift_a) + (ob[i][j]+shift_b)*(ob[i][j]+shift_b)));
                wanted_c = cmultiplier [chroma];
            }

            double real_c = wanted_c;
            if (wanted_c >= 1.0 && params->colorBoost.avoidclip) {
                double cclip = 100000.0;
                double cr = tightestroot ((double)lnew->L[i][j]/655.35, (double)(oa[i][j]+shift_a)*amul, (double)(ob[i][j]+shift_b)*bmul, 3.079935, -1.5371515, -0.54278342);
                double cg = tightestroot ((double)lnew->L[i][j]/655.35, (double)(oa[i][j]+shift_a)*amul, (double)(ob[i][j]+shift_b)*bmul, -0.92123418, 1.87599, 0.04524418);
                double cb = tightestroot ((double)lnew->L[i][j]/655.35, (double)(oa[i][j]+shift_a)*amul, (double)(ob[i][j]+shift_b)*bmul, 0.052889682, -0.20404134, 1.15115166);
                if (cr>1.0 && cr<cclip) cclip = cr;
                if (cg>1.0 && cg<cclip) cclip = cg;
                if (cb>1.0 && cb<cclip) cclip = cb;
                if (cclip<100000.0) {
                    real_c = -cclip + 2.0*cclip / (1.0+exp(-2.0*wanted_c/cclip));
                    if (real_c<1.0)
                        real_c = 1.0;
                }
            }

            float nna = ((oa[i][j]+shift_a) * real_c * amul);
            float nnb = ((ob[i][j]+shift_b) * real_c * bmul);
            lnew->a[i][j] = LIM(nna,-32000.0f,32000.0f);
            lnew->b[i][j] = LIM(nnb,-32000.0f,32000.0f);
        }
*/
//delete [] cmultiplier;
//}

void ImProcFunctions::impulsedenoise(LabImage* lab)
{

    if (params->impulseDenoise.enabled && lab->W >= 8 && lab->H >= 8)

    {
        impulse_nr(lab, params->impulseDenoise.thresh / 20.0);
    }
}

void ImProcFunctions::impulsedenoisecam(CieImage* ncie, float **buffers[3])
{

    if (params->impulseDenoise.enabled && ncie->W >= 8 && ncie->H >= 8)

    {
        impulse_nrcam(ncie, params->impulseDenoise.thresh / 20.0, buffers);
    }
}

void ImProcFunctions::defringe(LabImage* lab)
{

    if (params->defringe.enabled && lab->W >= 8 && lab->H >= 8)

    {
        PF_correct_RT(lab, params->defringe.radius, params->defringe.threshold);
    }
}

void ImProcFunctions::defringecam(CieImage* ncie)
{
    if (params->defringe.enabled && ncie->W >= 8 && ncie->H >= 8) {
        PF_correct_RTcam(ncie, params->defringe.radius, params->defringe.threshold);
    }
}

void ImProcFunctions::badpixcam(CieImage* ncie, double rad, int thr, int mode, float chrom, bool hotbad)
{
    if (ncie->W >= 8 && ncie->H >= 8) {
        Badpixelscam(ncie, rad, thr, mode, chrom, hotbad);
    }
}

void ImProcFunctions::badpixlab(LabImage* lab, double rad, int thr, float chrom)
{
    if (lab->W >= 8 && lab->H >= 8) {
        BadpixelsLab(lab, rad, thr, chrom);
    }
}

void ImProcFunctions::dirpyrequalizer(LabImage* lab, int scale)
{
    if (params->dirpyrequalizer.enabled && lab->W >= 8 && lab->H >= 8) {
        float b_l = static_cast<float>(params->dirpyrequalizer.hueskin.getBottomLeft()) / 100.f;
        float t_l = static_cast<float>(params->dirpyrequalizer.hueskin.getTopLeft()) / 100.f;
        float t_r = static_cast<float>(params->dirpyrequalizer.hueskin.getTopRight()) / 100.f;
        //      if     (params->dirpyrequalizer.algo=="FI") choice=0;
        //      else if(params->dirpyrequalizer.algo=="LA") choice=1;

        if (params->dirpyrequalizer.gamutlab && params->dirpyrequalizer.skinprotect != 0) {
            constexpr float artifact = 4.f;
            constexpr float chrom = 50.f;
            ImProcFunctions::badpixlab(lab, artifact / scale, 5, chrom);     //for artifacts
        }

        //dirpyrLab_equalizer(lab, lab, params->dirpyrequalizer.mult);
        dirpyr_equalizer(lab->L, lab->L, lab->W, lab->H, lab->a, lab->b, params->dirpyrequalizer.mult, params->dirpyrequalizer.threshold, params->dirpyrequalizer.skinprotect, b_l, t_l, t_r, scale);
    }
}
void ImProcFunctions::EPDToneMapCIE(CieImage *ncie, float a_w, float c_, int Wid, int Hei, float minQ, float maxQ, unsigned int Iterates, int skip)
{

    if (!params->epd.enabled) {
        return;
    }

    /*
        if (params->wavelet.enabled  && params->wavelet.tmrs != 0) {
            return;
        }
    */
    float stren = params->epd.strength;
    const float edgest = std::min(params->epd.edgeStopping, params->localContrast.enabled ? 3.0 : 4.0);
    float sca = params->epd.scale;
    float gamm = params->epd.gamma;
    float rew = params->epd.reweightingIterates;
    float Qpro = (4.f / c_)  * (a_w + 4.f) ; //estimate Q max if J=100.0
    float *Qpr = ncie->Q_p[0];

    if (settings->verbose) {
        printf("minQ=%f maxQ=%f  Qpro=%f\n", static_cast<double>(minQ), static_cast<double>(maxQ), static_cast<double>(Qpro));
    }

    if (maxQ > Qpro) {
        Qpro = maxQ;
    }

    EdgePreservingDecomposition epd(Wid, Hei);

#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int i = 0; i < Hei; i++)
        for (int j = 0; j < Wid; j++) {
            ncie->Q_p[i][j] = gamm * ncie->Q_p[i][j] / (Qpro);
        }

    float Compression = expf(-stren);       //This modification turns numbers symmetric around 0 into exponents.
    float DetailBoost = stren;

    if (stren < 0.0f) {
        DetailBoost = 0.0f;    //Go with effect of exponent only if uncompressing.
    }

    //Auto select number of iterates. Note that p->EdgeStopping = 0 makes a Gaussian blur.
    if (Iterates == 0) {
        Iterates = edgest * 15.f;
    }

    //Jacques Desmis : always Iterates=5 for compatibility images between preview and output

    epd.CompressDynamicRange(Qpr, sca / (float)skip, edgest, Compression, DetailBoost, Iterates, rew);

    //Restore past range, also desaturate a bit per Mantiuk's Color correction for tone mapping.
    float s = (1.0f + 38.7889f) * powf(Compression, 1.5856f) / (1.0f + 38.7889f * powf(Compression, 1.5856f));
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,10)
#endif

    for (int i = 0; i < Hei; i++)
        for (int j = 0; j < Wid; j++) {
            ncie->Q_p[i][j] = (ncie->Q_p[i][j] * Qpro) / gamm;
            ncie->M_p[i][j] *= s;
        }

    /*
        float *Qpr2 = new float[Wid*((heir)+1)];

            for (int i=heir; i<Hei; i++)
                for (int j=0; j<Wid; j++) { Qpr2[(i-heir)*Wid+j]=ncie->Q_p[i][j];}
        if(minQ>0.0) minQ=0.0;//normally minQ always > 0...
    //  EdgePreservingDecomposition epd = EdgePreservingDecomposition(Wid, Hei);
    //EdgePreservingDecomposition epd = EdgePreservingDecomposition(Wid, Hei/2);
        for(i = N2; i != N; i++)
    //  for(i = begh*Wid; i != N; i++)
            //Qpr[i] = (Qpr[i]-minQ)/(maxQ+1.0);
            Qpr2[i-N2] = (Qpr2[i-N2]-minQ)/(Qpro+1.0);

        float Compression2 = expf(-stren);      //This modification turns numbers symmetric around 0 into exponents.
        float DetailBoost2 = stren;
        if(stren < 0.0f) DetailBoost2 = 0.0f;   //Go with effect of exponent only if uncompressing.

        //Auto select number of iterates. Note that p->EdgeStopping = 0 makes a Gaussian blur.
        if(Iterates == 0) Iterates = (unsigned int)(edgest*15.0);


        epd.CompressDynamicRange(Qpr2, sca/(float)skip, edgest, Compression2, DetailBoost2, Iterates, rew, Qpr2);

        //Restore past range, also desaturate a bit per Mantiuk's Color correction for tone mapping.
         float s2 = (1.0f + 38.7889f)*powf(Compression, 1.5856f)/(1.0f + 38.7889f*powf(Compression, 1.5856f));
            for (int i=heir; i<Hei; i++)
        //  for (int i=begh; i<endh; i++)
                for (int j=0; j<Wid; j++) {
                ncie->Q_p[i][j]=Qpr2[(i-heir)*Wid+j]*Qpro + minQ;
            //  Qpr[i*Wid+j]=Qpr[i*Wid+j]*maxQ + minQ;
            //  ncie->J_p[i][j]=(100.0* Qpr[i*Wid+j]*Qpr[i*Wid+j]) /(w_h*w_h);

                ncie->M_p[i][j]*=s2;
            }
                    delete [] Qpr2;

    */
}

void ImProcFunctions::EPDToneMaplocal(int sp, LabImage *lab, LabImage *tmp1, unsigned int Iterates, int skip)
{

    float stren = ((float)params->locallab.spots.at(sp).stren);
    const float edgest = std::min(params->locallab.spots.at(sp).estop, params->localContrast.enabled ? 3.0 : 4.0);

    float sca  = ((float)params->locallab.spots.at(sp).scaltm);
    float gamm = ((float)params->locallab.spots.at(sp).gamma);
    float satur = ((float)params->locallab.spots.at(sp).satur) / 100.f;
    float rew = ((float)params->locallab.spots.at(sp).rewei);
    //Pointers to whole data and size of it.
    float *L = lab->L[0];
    float *a = lab->a[0];
    float *b = lab->b[0];
    std::size_t N = static_cast<size_t>(lab->W) * static_cast<size_t>(lab->H);
    int WW = lab->W ;

    EdgePreservingDecomposition epd(lab->W, lab->H);

    //Due to the taking of logarithms, L must be nonnegative. Further, scale to 0 to 1 using nominal range of L, 0 to 15 bit.
    float minL = L[0];
    float maxL = minL;

#ifdef _OPENMP
    #pragma omp parallel for reduction(max:maxL) reduction(min:minL) schedule(dynamic,16)
#endif

    for (std::size_t i = 0; i < N; i++) {
        minL = rtengine::min(minL, L[i]);
        maxL = rtengine::max(maxL, L[i]);
    }

    if (minL > 0.0f) {
        minL = 0.0f;    //Disable the shift if there are no negative numbers. I wish there were just no negative numbers to begin with.
    }

    if (maxL == 0.f) { // avoid division by zero
        maxL = 1.f;
    }

    const float mult = gamm / maxL;
#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (std::size_t i = 0; i < N; i++) {
        L[i] = (L[i] - minL) * mult;
    }

    //Some interpretations.
    float Compression = expf(-stren);       //This modification turns numbers symmetric around 0 into exponents.
    float DetailBoost = stren;

    if (stren < 0.0f) {
        DetailBoost = 0.0f;    //Go with effect of exponent only if uncompressing.
    }

    //Auto select number of iterates. Note that p->EdgeStopping = 0 makes a Gaussian blur.
    if (Iterates == 0) {
        Iterates = (unsigned int)(edgest * 15.0f);
    }

    /* Debuggery. Saves L for toying with outside of RT.
    char nm[64];
    snprintf(nm, sizeof(nm), "%ux%ufloat.bin", lab->W, lab->H);
    FILE *f = fopen(nm, "wb");
    fwrite(L, N, sizeof(float), f);
    fclose(f);*/

    epd.CompressDynamicRange(L, sca / float (skip), edgest, Compression, DetailBoost, Iterates, rew);

    //Restore past range, also desaturate a bit per Mantiuk's Color correction for tone mapping.
    float s = (1.0f + 38.7889f) * powf(Compression, 1.5856f) / (1.0f + 38.7889f * powf(Compression, 1.5856f));
    float sat = s + 0.3f * s * satur;

    //printf("s=%f  sat=%f \n", s, sat);
    if (sat == 1.f) {
        sat = 1.001f;
    }

#ifdef _OPENMP
    #pragma omp parallel for            // removed schedule(dynamic,10)
#endif

    for (unsigned int i = 0; i < N; i++) {
        int x = i / WW;
        int y = i - x * WW;

        tmp1->L[x][y] = L[i] * maxL * (1.f / gamm) + minL;
        tmp1->a[x][y] = sat * a[i];
        tmp1->b[x][y] = sat * b[i];

    }
}





//Map tones by way of edge preserving decomposition.
void ImProcFunctions::EPDToneMap(LabImage *lab, unsigned int Iterates, int skip)
{

    if (!params->epd.enabled) {
        return;
    }

    const float stren = params->epd.strength;
    const float edgest = std::min(params->epd.edgeStopping, params->localContrast.enabled ? 3.0 : 4.0);
    const float sca = params->epd.scale;
    const float gamm = params->epd.gamma;
    const float rew = params->epd.reweightingIterates;
    //Pointers to whole data and size of it.
    float *L = lab->L[0];
    float *a = lab->a[0];
    float *b = lab->b[0];
    const size_t N = lab->W * lab->H;

    EdgePreservingDecomposition epd(lab->W, lab->H);

    //Due to the taking of logarithms, L must be nonnegative. Further, scale to 0 to 1 using nominal range of L, 0 to 15 bit.
    float minL = L[0];
    float maxL = L[0];
#ifdef _OPENMP
    #pragma omp parallel for reduction(min:minL) reduction(max:maxL)
#endif

    for (size_t i = 1; i < N; i++) {
        minL = std::min(minL, L[i]);
        maxL = std::max(maxL, L[i]);
    }

    if (maxL == 0.f) { // black input => do nothing
        return;
    }

    minL = std::min(minL, 0.f); //Disable the shift if there are no negative numbers. I wish there were just no negative numbers to begin with.

#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (size_t i = 0; i < N; ++i) {
        L[i] = (L[i] - minL) * (gamm / maxL);
    }

    //Some interpretations.
    const float Compression = expf(-stren); //This modification turns numbers symmetric around 0 into exponents.
    const float DetailBoost = std::max(stren, 0.f); //Go with effect of exponent only if uncompressing.

    //Auto select number of iterates. Note that p->EdgeStopping = 0 makes a Gaussian blur.
    if (Iterates == 0) {
        Iterates = edgest * 15.f;
    }

    epd.CompressDynamicRange(L, sca / skip, edgest, Compression, DetailBoost, Iterates, rew);

    //Restore past range, also desaturate a bit per Mantiuk's Color correction for tone mapping.
    const float s = (1.f + 38.7889f) * std::pow(Compression, 1.5856f) / (1.f + 38.7889f * std::pow(Compression, 1.5856f));

    maxL /= gamm;
#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (size_t ii = 0; ii < N; ++ii) {
        a[ii] *= s;
        b[ii] *= s;
        L[ii] = L[ii] * maxL + minL;
    }
}


void ImProcFunctions::getAutoExp(const LUTu &histogram, int histcompr, double clip, double& expcomp, int& bright, int& contr, int& black, int& hlcompr, int& hlcomprthresh)
{

    float scale = 65536.0f;
    float midgray = 0.1842f; //middle gray in linear gamma =1 50% luminance

    int imax = 65536 >> histcompr;
    int overex = 0;
    float sum = 0.f, hisum = 0.f, losum = 0.f;
    float ave = 0.f;

    //find average luminance
    histogram.getSumAndAverage(sum, ave);

    //find median of luminance
    size_t median = 0, count = histogram[0];

    while (count < sum / 2) {
        median++;
        count += histogram[median];
    }

    if (median == 0 || ave < 1.f) { //probably the image is a blackframe
        expcomp = 0.;
        black = 0;
        bright = 0;
        contr = 0;
        hlcompr = 0;
        hlcomprthresh = 0;
        return;
    }

    // compute std dev on the high and low side of median
    // and octiles of histogram
    float octile[8] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, ospread = 0.f;
    count = 0;

    int j = 0;

    for (; j < min((int)ave, imax); ++j) {
        if (count < 8) {
            octile[count] += histogram[j];

            if (octile[count] > sum / 8.f || (count == 7 && octile[count] > sum / 16.f)) {
                octile[count] = xlog(1. + j) / log(2.0);
                count++;// = min(count+1,7);
            }
        }

        losum += histogram[j];
    }

    for (; j < imax; ++j) {
        if (count < 8) {
            octile[count] += histogram[j];

            if (octile[count] > sum / 8.f || (count == 7 && octile[count] > sum / 16.f)) {
                octile[count] = xlog(1. + j) / log(2.0);
                count++;// = min(count+1,7);
            }
        }

        hisum += histogram[j];

    }

    if (losum == 0 || hisum == 0) { //probably the image is a blackframe
        expcomp = 0.;
        black = 0;
        bright = 0;
        contr = 0;
        hlcompr = 0;
        hlcomprthresh = 0;
        return;
    }

//    lodev = (lodev / (log(2.f) * losum));
//    hidev = (hidev / (log(2.f) * hisum));

    if (octile[6] > log1p((float)imax) / log2(2.f)) {   //if very overxposed image
        octile[6] = 1.5f * octile[5] - 0.5f * octile[4];
        overex = 2;
    }

    if (octile[7] > log1p((float)imax) / log2(2.f)) {   //if overexposed
        octile[7] = 1.5f * octile[6] - 0.5f * octile[5];
        overex = 1;
    }

    // store values of octile[6] and octile[7] for calculation of exposure compensation
    // if we don't do this and the pixture is underexposed, calculation of exposure compensation assumes
    // that it's overexposed and calculates the wrong direction
    float oct6, oct7;
    oct6 = octile[6];
    oct7 = octile[7];


    for (int i = 1; i < 8; i++) {
        if (octile[i] == 0.0f) {
            octile[i] = octile[i - 1];
        }
    }

    // compute weighted average separation of octiles
    // for future use in contrast setting
    for (int i = 1; i < 6; i++) {
        ospread += (octile[i + 1] - octile[i]) / max(0.5f, (i > 2 ? (octile[i + 1] - octile[3]) : (octile[3] - octile[i])));
    }

    ospread /= 5.f;

    if (ospread <= 0.f) { //probably the image is a blackframe
        expcomp = 0.;
        black = 0;
        bright = 0;
        contr = 0;
        hlcompr = 0;
        hlcomprthresh = 0;
        return;
    }


    // compute clipping points based on the original histograms (linear, without exp comp.)
    unsigned int clipped = 0;
    int rawmax = (imax) - 1;

    while (histogram[rawmax] + clipped <= 0 && rawmax > 1) {
        clipped += histogram[rawmax];
        rawmax--;
    }

    //compute clipped white point
    unsigned int clippable = (int)(static_cast<double>(sum) * clip / 100.0);
    clipped = 0;
    int whiteclip = (imax) - 1;

    while (whiteclip > 1 && (histogram[whiteclip] + clipped) <= clippable) {
        clipped += histogram[whiteclip];
        whiteclip--;
    }

    //compute clipped black point
    clipped = 0;
    int shc = 0;

    while (shc < whiteclip - 1 && histogram[shc] + clipped <= clippable) {
        clipped += histogram[shc];
        shc++;
    }

    //rescale to 65535 max
    rawmax <<= histcompr;
    whiteclip <<= histcompr;
    ave = ave * (1 << histcompr);
    median <<= histcompr;
    shc <<= histcompr;

//    //prevent division by 0
//    if (lodev == 0.f) {
//        lodev = 1.f;
//    }

    //compute exposure compensation as geometric mean of the amount that
    //sets the mean or median at middle gray, and the amount that sets the estimated top
    //of the histogram at or near clipping.
    //float expcomp1 = (log(/*(median/ave)*//*(hidev/lodev)*/midgray*scale/(ave-shc+midgray*shc))+log((hidev/lodev)))/log(2.f);
    float expcomp1 = (log(/*(median/ave)*//*(hidev/lodev)*/midgray * scale / (ave - shc + midgray * shc))) / log(2.f);
    float expcomp2;

    if (overex == 0) { // image is not overexposed
        expcomp2 = 0.5f * ((15.5f - histcompr - (2.f * oct7 - oct6)) + log(scale / rawmax) / log(2.f));
    } else {
        expcomp2 = 0.5f * ((15.5f - histcompr - (2.f * octile[7] - octile[6])) + log(scale / rawmax) / log(2.f));
    }

    if (fabs(expcomp1) - fabs(expcomp2) > 1.f) {   //for great expcomp
        expcomp = (expcomp1 * fabs(expcomp2) + expcomp2 * fabs(expcomp1)) / (fabs(expcomp1) + fabs(expcomp2));
    } else {
        expcomp = 0.5 * (double)expcomp1 + 0.5 * (double) expcomp2; //for small expcomp
    }

    float gain = exp((float)expcomp * log(2.f));

    float corr = sqrt(gain * scale / rawmax);
    black = (int) shc * corr;


    //now tune hlcompr to bring back rawmax to 65535
    hlcomprthresh = 0;
    //this is a series approximation of the actual formula for comp,
    //which is a transcendental equation
    double comp = (gain * whiteclip / scale - 1.f) * 2.3f; // 2.3 instead of 2 to increase slightly comp
    hlcompr = 100.0 * comp / (max(0.0, expcomp) + 1.0);
    hlcompr = max(0, min(100, hlcompr));

    //now find brightness if gain didn't bring ave to midgray using
    //the envelope of the actual 'control cage' brightness curve for simplicity
    float midtmp = gain * sqrt(median * ave) / scale;

    if (midtmp < 0.1f) {
        bright = (midgray - midtmp) * 15.f / (midtmp);
    } else {
        bright = (midgray - midtmp) * 15.f / (0.10833f - 0.0833f * midtmp);
    }

    bright = 0.25 */*(median/ave)*(hidev/lodev)*/max(0, bright);

    //compute contrast that spreads the average spacing of octiles
    contr = (int) 50.0f * (1.1f - ospread);
    contr = max(0, min(100, contr));
    //take gamma into account
    double whiteclipg = (int)(CurveFactory::gamma2(whiteclip * static_cast<double>(corr) / 65536.0) * 65536.0);

    float gavg = 0.;

    float val = 0.f;
    float increment = corr * (1 << histcompr);

    for (int i = 0; i < 65536 >> histcompr; i++) {
        gavg += histogram[i] * Color::gamma2curve[val];
        val += increment;
    }

    gavg /= sum;

    if (black < gavg) {
        int maxwhiteclip = (gavg - black) * 4 / 3 + black; // don't let whiteclip be such large that the histogram average goes above 3/4

        if (whiteclipg < maxwhiteclip) {
            whiteclipg = maxwhiteclip;
        }
    }

    whiteclipg = CurveFactory::igamma2(whiteclipg / 65535.0) * 65535.0; //need to inverse gamma transform to get correct exposure compensation parameter

    //correction with gamma
    black = (int)((65535 * black) / whiteclipg);
    //expcomp = log(65535.0 / (whiteclipg)) / log(2.0);

    //diagnostics
    //printf ("**************** AUTO LEVELS ****************\n");
    /*
    if (settings->verbose) {
        printf("sum=%i clip=%f clippable=%i  clipWh=%i  clipBl=%i\n",somm, clip, clippable,clipwh, clipbl);
        printf ("expcomp1= %f   expcomp2= %f gain= %f  expcomp=%f\n",expcomp1,expcomp2,gain,expcomp);
        printf ("expo=%f\n",expo);
        printf ("median: %i  average: %f    median/average: %f\n",median,ave, median/ave);
        printf ("average: %f\n",ave);
        printf("comp=%f hlcomp: %i\n",comp, hlcompr);
        printf ("median/average: %f\n",median/ave);
        printf ("lodev: %f   hidev: %f      hidev/lodev: %f\n",lodev,hidev,hidev/lodev);
        printf ("lodev: %f\n",lodev);
        printf ("hidev: %f\n",hidev);
        printf ("imax=%d rawmax= %d  whiteclip= %d  gain= %f\n",imax,rawmax,whiteclip,gain);
        printf ("octiles: %f %f %f %f %f %f %f %f\n",octile[0],octile[1],octile[2],octile[3],octile[4],octile[5],octile[6],octile[7]);
        printf ("ospread= %f\n",ospread);
        printf ("overexp= %i\n",overex);
    }
    */
    /*
     // %%%%%%%%%% LEGACY AUTOEXPOSURE CODE %%%%%%%%%%%%%
     // black point selection is based on the linear result (yielding better visual results)
     black = (int)(shc * corr);
     // compute the white point of the exp. compensated gamma corrected image
     double whiteclipg = (int)(CurveFactory::gamma2 (whiteclip * corr / 65536.0) * 65536.0);

     // compute average intensity of the exp compensated, gamma corrected image
     double gavg = 0;
     for (int i=0; i<65536>>histcompr; i++)
     gavg += histogram[i] * CurveFactory::gamma2((int)(corr*(i<<histcompr)<65535 ? corr*(i<<histcompr) : 65535)) / sum;

     if (black < gavg) {
     int maxwhiteclip = (gavg - black) * 4 / 3 + black; // don't let whiteclip be such large that the histogram average goes above 3/4
     //double mavg = 65536.0 / (whiteclipg-black) * (gavg - black);
     if (whiteclipg < maxwhiteclip)
     whiteclipg = maxwhiteclip;
     }

     whiteclipg = CurveFactory::igamma2 ((float)(whiteclipg/65535.0)) * 65535.0; //need to inverse gamma transform to get correct exposure compensation parameter

     black = (int)((65535*black)/whiteclipg);
     expcomp = log(65535.0 / (whiteclipg)) / log(2.0);

     if (expcomp<0.0)   expcomp = 0.0;*/
    if (expcomp < -5.0) {
        expcomp = -5.0;
    }

    if (expcomp > 12.0) {
        expcomp = 12.0;
    }

    bright = max(-100, min(bright, 100));

}


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double ImProcFunctions::getAutoDistor(const Glib::ustring &fname, int thumb_size)
{
    if (!fname.empty()) {
    	// TODO: std::unique_ptr<> to the rescue
        int w_raw = -1, h_raw = thumb_size;
        int w_thumb = -1, h_thumb = thumb_size;

        eSensorType sensorType = rtengine::ST_NONE;
        Thumbnail* thumb = rtengine::Thumbnail::loadQuickFromRaw(fname, sensorType, w_thumb, h_thumb, 1, FALSE);

        if (!thumb) {
            return 0.0;
        }

        Thumbnail* raw =   rtengine::Thumbnail::loadFromRaw(fname, sensorType, w_raw, h_raw, 1, 1.0, ColorTemp::DEFAULT_OBSERVER, FALSE, nullptr);

        if (!raw) {
            delete thumb;
            return 0.0;
        }

        if (h_thumb != h_raw) {
            delete thumb;
            delete raw;
            return 0.0;
        }

        int width;

        if (w_thumb > w_raw) {
            width = w_raw;
        } else {
            width = w_thumb;
        }

        unsigned char* thumbGray;
        unsigned char* rawGray;
        thumbGray = thumb->getGrayscaleHistEQ(width);
        rawGray = raw->getGrayscaleHistEQ(width);

        if (!thumbGray || !rawGray) {
            delete[] thumbGray;
            delete[] rawGray;
            delete thumb;
            delete raw;
            return 0.0;
        }

        double dist_amount;
        int dist_result = calcDistortion(thumbGray, rawGray, width, h_thumb, 1, dist_amount);

        if (dist_result == -1) { // not enough features found, try increasing max. number of features by factor 4
            calcDistortion(thumbGray, rawGray, width, h_thumb, 4, dist_amount);
        }

        delete[] thumbGray;
        delete[] rawGray;
        delete thumb;
        delete raw;
        return dist_amount;
    } else {
        return 0.0;
    }
}

void ImProcFunctions::rgb2lab(const Imagefloat &src, LabImage &dst, const Glib::ustring &workingSpace)
{
    TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(workingSpace);
    const float wp[3][3] = {
        {static_cast<float>(wprof[0][0]), static_cast<float>(wprof[0][1]), static_cast<float>(wprof[0][2])},
        {static_cast<float>(wprof[1][0]), static_cast<float>(wprof[1][1]), static_cast<float>(wprof[1][2])},
        {static_cast<float>(wprof[2][0]), static_cast<float>(wprof[2][1]), static_cast<float>(wprof[2][2])}
    };

    const int W = src.getWidth();
    const int H = src.getHeight();

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16)
#endif

    for (int i = 0; i < H; i++) {
        for (int j = 0; j < W; j++) {
            float X, Y, Z;
            Color::rgbxyz(src.r(i, j), src.g(i, j), src.b(i, j), X, Y, Z, wp);
            //convert Lab
            Color::XYZ2Lab(X, Y, Z, dst.L[i][j], dst.a[i][j], dst.b[i][j]);
        }
    }
}

void ImProcFunctions::rgb2lab(const Image8 &src, int x, int y, int w, int h, float L[], float a[], float b[], const procparams::ColorManagementParams &icm, bool consider_histogram_settings) const
{
    rgb2lab(src, x, y, w, h, L, a, b, icm, consider_histogram_settings, multiThread);
}

void ImProcFunctions::rgb2lab(const Image8 &src, int x, int y, int w, int h, float L[], float a[], float b[], const procparams::ColorManagementParams &icm, bool consider_histogram_settings, bool multiThread)
{
    // Adapted from ImProcFunctions::lab2rgb
    const int src_width = src.getWidth();
    const int src_height = src.getHeight();

    if (x < 0) {
        x = 0;
    }

    if (y < 0) {
        y = 0;
    }

    if (x + w > src_width) {
        w = src_width - x;
    }

    if (y + h > src_height) {
        h = src_height - y;
    }

    Glib::ustring profile;

    cmsHPROFILE oprof = nullptr;

    if (settings->HistogramWorking && consider_histogram_settings) {
        profile = icm.workingProfile;
    } else {
        profile = icm.outputProfile;

        if (icm.outputProfile.empty() || icm.outputProfile == ColorManagementParams::NoICMString) {
            profile = "sRGB";
        }

        oprof = ICCStore::getInstance()->getProfile(profile);
    }

    if (oprof) {
        cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE; // NOCACHE is important for thread safety

        if (icm.outputBPC) {
            flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
        }

        lcmsMutex->lock();
        cmsHPROFILE LabIProf  = cmsCreateLab4Profile(nullptr);
        cmsHTRANSFORM hTransform = cmsCreateTransform(oprof, TYPE_RGB_8, LabIProf, TYPE_Lab_FLT, icm.outputIntent, flags);
        cmsCloseProfile(LabIProf);
        lcmsMutex->unlock();

        // cmsDoTransform is relatively expensive
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            AlignedBuffer<float> oBuf(3 * w);
            float *outbuffer = oBuf.data;
            int condition = y + h;

#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16)
#endif

            for (int i = y; i < condition; i++) {
                const int ix = 3 * (x + i * src_width);
                int iy = 0;
                float* rL = L + (i - y) * w;
                float* ra = a + (i - y) * w;
                float* rb = b + (i - y) * w;

                cmsDoTransform(hTransform, src.data + ix, outbuffer, w);

                for (int j = 0; j < w; j++) {
                    rL[j] = outbuffer[iy++] * 327.68f;
                    ra[j] = outbuffer[iy++] * 327.68f;
                    rb[j] = outbuffer[iy++] * 327.68f;
                }
            }
        } // End of parallelization

        cmsDeleteTransform(hTransform);
    } else {
        TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(profile);
        const float wp[3][3] = {
            {static_cast<float>(wprof[0][0]), static_cast<float>(wprof[0][1]), static_cast<float>(wprof[0][2])},
            {static_cast<float>(wprof[1][0]), static_cast<float>(wprof[1][1]), static_cast<float>(wprof[1][2])},
            {static_cast<float>(wprof[2][0]), static_cast<float>(wprof[2][1]), static_cast<float>(wprof[2][2])}
        };

        const int x2 = x + w;
        const int y2 = y + h;
        constexpr float rgb_factor = 65355.f / 255.f;

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

        for (int i = y; i < y2; i++) {
            int offset = (i - y) * w;

            for (int j = x; j < x2; j++) {
                float X, Y, Z;
                // lab2rgb uses gamma2curve, which is gammatab_srgb.
                const auto& igamma = Color::igammatab_srgb;
                Color::rgbxyz(igamma[rgb_factor * src.r(i, j)], igamma[rgb_factor * src.g(i, j)], igamma[rgb_factor * src.b(i, j)], X, Y, Z, wp);
                Color::XYZ2Lab(X, Y, Z, L[offset], a[offset], b[offset]);
                offset++;
            }
        }
    }
}

void ImProcFunctions::rgb2lab(std::uint8_t red, std::uint8_t green, std::uint8_t blue, float &L, float &a, float &b, const procparams::ColorManagementParams &icm, bool consider_histogram_settings)
{
    float l_channel[1];
    float a_channel[1];
    float b_channel[1];
    rtengine::Image8 buf(1, 1);
    buf.r(0, 0) = red;
    buf.g(0, 0) = green;
    buf.b(0, 0) = blue;
    ImProcFunctions::rgb2lab(buf, 0, 0, 1, 1, l_channel, a_channel, b_channel, icm, consider_histogram_settings, false);
    L = l_channel[0];
    a = a_channel[0];
    b = b_channel[0];
}

void ImProcFunctions::lab2rgb(const LabImage &src, Imagefloat &dst, const Glib::ustring &workingSpace)
{
    TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(workingSpace);
    const float wip[3][3] = {
        {static_cast<float>(wiprof[0][0]), static_cast<float>(wiprof[0][1]), static_cast<float>(wiprof[0][2])},
        {static_cast<float>(wiprof[1][0]), static_cast<float>(wiprof[1][1]), static_cast<float>(wiprof[1][2])},
        {static_cast<float>(wiprof[2][0]), static_cast<float>(wiprof[2][1]), static_cast<float>(wiprof[2][2])}
    };

    const int W = dst.getWidth();
    const int H = dst.getHeight();
#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat wipv[3][3];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            wipv[i][j] = F2V(wiprof[i][j]);
        }
    }

#endif

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16)
#endif

    for (int i = 0; i < H; i++) {
        int j = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)

        for (; j < W - 3; j += 4) {
            vfloat X, Y, Z;
            vfloat R, G, B;
            Color::Lab2XYZ(LVFU(src.L[i][j]), LVFU(src.a[i][j]), LVFU(src.b[i][j]), X, Y, Z);
            Color::xyz2rgb(X, Y, Z, R, G, B, wipv);
            STVFU(dst.r(i, j), R);
            STVFU(dst.g(i, j), G);
            STVFU(dst.b(i, j), B);
        }

#endif

        for (; j < W; j++) {
            float X, Y, Z;
            Color::Lab2XYZ(src.L[i][j], src.a[i][j], src.b[i][j], X, Y, Z);
            Color::xyz2rgb(X, Y, Z, dst.r(i, j), dst.g(i, j), dst.b(i, j), wip);
        }
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


// adapted from the "color correction" module of Darktable. Original copyright follows
/*
    copyright (c) 2009--2010 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <https://www.gnu.org/licenses/>.
*/
void ImProcFunctions::colorToningLabGrid(LabImage *lab, int xstart, int xend, int ystart, int yend, bool MultiThread)
{
    const double factor = ColorToningParams::LABGRID_CORR_MAX * 3.0;
    const double scaling = ColorToningParams::LABGRID_CORR_SCALE;
    float a_scale = (params->colorToning.labgridAHigh - params->colorToning.labgridALow) / factor / scaling;
    float a_base = params->colorToning.labgridALow / scaling;
    float b_scale = (params->colorToning.labgridBHigh - params->colorToning.labgridBLow) / factor / scaling;
    float b_base = params->colorToning.labgridBLow / scaling;

#ifdef _OPENMP
    #pragma omp parallel for if (MultiThread)
#endif

    for (int y = ystart; y < yend; ++y) {
        for (int x = xstart; x < xend; ++x) {
            lab->a[y][x] += lab->L[y][x] * a_scale + a_base;
            lab->b[y][x] += lab->L[y][x] * b_scale + b_base;
        }
    }
}

}
