////////////////////////////////////////////////////////////////
//
//
//
//
//  code dated: 9 , 2019
//
//  Ipwaveletcc is free software: you can redistribute it and/or modify
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
// *  2014 - 2019  2020 2024 - Jacques Desmis <jdesmis@gmail.com>
// *  2014 Ingo Weyrich <heckflosse@i-weyrich.de>

//
////////////////////////////////////////////////////////////////

#include <cassert>
#include <cmath>

#include "array2D.h"
#include "color.h"
#include "curves.h"
#include "EdgePreservingDecomposition.h"
#include "iccstore.h"
#include "improcfun.h"
#include "imagefloat.h"
#include "labimage.h"
#include "gauss.h"
#include "boxblur.h"
#include "LUT.h"
#include "median.h"
#include "opthelper.h"
#include "procparams.h"
#include "rt_math.h"
#include "rtengine.h"
#include "sleef.h"
#include "rtgui/options.h"
#include "guidedfilter.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#include "cplx_wavelet_dec.h"
//#define BENCHMARK
#include "StopWatch.h"

namespace rtengine
{

struct cont_params {
    float mul[10];
    float sigm;
    int chrom;
    int chro;
    float chrwav;
    int contrast;
    float th;
    float thH;
    float conres;
    float conresH;
    float blurres;
    float blurcres;
    float bluwav;
    float radius;
    float chrores;
    bool oldsh;
    float hueres;
    float sky;
    float b_l, t_l, b_r, t_r;
    float b_ly, t_ly, b_ry, t_ry;
    float b_lsl, t_lsl, b_rsl, t_rsl;
    float b_lhl, t_lhl, b_rhl, t_rhl;
    float edg_low, edg_mean, edg_sd, edg_max;
    float lev0s, lev0n, lev1s, lev1n, lev2s, lev2n, lev3s, lev3n, lev4n, lev4t;
    float b_lpast, t_lpast, b_rpast, t_rpast;
    float b_lsat, t_lsat, b_rsat, t_rsat;
    int rad;
    float eff;
    int val;
    int til;
    int numlevH, numlevS;
    float mulC[9];
    float mulopaRG[9];
    float mulopaBY[9];
    bool curv;
    bool opaBY;
    bool opaRG;
    bool edgcurv;
    bool diagcurv;
    bool denoicurv;
    bool denoicurvh;
    int CHmet;
    int CHSLmet;
    int EDmet;
    bool HSmet;
    bool avoi;
    float strength;
    int reinforce;
    bool detectedge;
    int backm;
    float eddet;
    float eddetthr;
    float eddetthrHi;
    bool link;
    bool lip3;
    bool tonemap;
    bool diag;
    float tmstrength;
    float balan;
    float sigmafin;
    float sigmaton;
    float sigmacol;
    float sigmadir;
    int denmet;
    int mixmet;
    int quamet;
    int slimet;
    int ite;
    int contmet;
    bool opaW;
    int BAmet;
    bool bam;
    float blhigh;
    float grhigh;
    float blmed;
    float grmed;
    float bllow;
    float grlow;
    bool cbena;
    bool contena;
    bool chromena;
    bool edgeena;
    bool resena;
    bool finena;
    bool toningena;
    bool noiseena;
    bool blena;
    int maxilev;
    float edgsens;
    float edgampl;
    int neigh;
    bool lipp;
    float ballum;
    float balchrom;
    float chromfi;
    float chromco;
    float factor;
    float scaling;
    float scaledirect;
    float a_scale;
    float a_base;
    float b_scale;
    float b_base;
    float a_high;
    float a_low;
    float b_high;
    float b_low;
    float rangeab;
    float protab;
    float sigmm;
    float sigmm14;
    float sigmm56;
    float levden;
    float thrden;
    float limden;
    int complex;
};

int wavNestedLevels = 1;

std::unique_ptr<LUTf> ImProcFunctions::buildMeaLut(const float inVals[11], const float mea[10], float& lutFactor)
{
    constexpr int lutSize = 100;

    const float lutMax = std::ceil(mea[9]);
    const float lutDiff = lutMax / lutSize;

    std::vector<float> lutVals(lutSize);
    int jStart = 1;
    for (int i = 0; i < lutSize; ++i) {
        const float val = i * lutDiff;
        if (val < mea[0]) {
            // still < first value => no interpolation
            lutVals[i] = inVals[0];
        } else {
            for (int j = jStart; j < 10; ++j) {
                if (val == mea[j]) {
                    // exact match => no interpolation
                    lutVals[i] = inVals[j];
                    ++jStart;
                    break;
                }
                if (val < mea[j]) {
                    // interpolate
                    const float dist = (val - mea[j - 1]) / (mea[j] - mea[j - 1]);
                    lutVals[i] = rtengine::intp(dist, inVals[j], inVals[j - 1]);
                    break;
                }
                lutVals[i] = inVals[10];
            }
        }
    }
    lutFactor = lutDiff == 0.f ? 0.f : 1.f / lutDiff;
    return std::unique_ptr<LUTf>(new LUTf(lutVals));
}

void ImProcFunctions::ip_wavelet(LabImage * lab, LabImage * dst, int kall, const procparams::WaveletParams & waparams, const WavCurve & wavCLVCcurve, const WavCurve & wavdenoise,  WavCurve & wavdenoiseh, const Wavblcurve & wavblcurve, const WavOpacityCurveRG & waOpacityCurveRG, const WavOpacityCurveSH & waOpacityCurveSH, const WavOpacityCurveBY & waOpacityCurveBY,  const WavOpacityCurveW & waOpacityCurveW, const WavOpacityCurveWL & waOpacityCurveWL, const LUTf &wavclCurve, int skip)


{
    TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(params->icm.workingProfile);
    const double wip[3][3] = {
        {wiprof[0][0], wiprof[0][1], wiprof[0][2]},
        {wiprof[1][0], wiprof[1][1], wiprof[1][2]},
        {wiprof[2][0], wiprof[2][1], wiprof[2][2]}
    };
    const int imheight = lab->H, imwidth = lab->W;
   int levwavL = 0;
                        //Flat curve for H=f(H) in final touchup for guidedfilter
    FlatCurve* wavguidCurve = new FlatCurve(params->wavelet.wavguidcurve); //curve H=f(H)
    bool wavguidutili = false;

    if (!wavguidCurve || wavguidCurve->isIdentity()) {
        if (wavguidCurve) {
            delete wavguidCurve;
            wavguidCurve = nullptr;
        }
    } else {
        wavguidutili = true;
    }
//flat curve for equalizer H
    FlatCurve* wavhueCurve = new FlatCurve(params->wavelet.wavhuecurve); //curve H=f(H)
    bool wavhueutili = false;

    if (!wavhueCurve || wavhueCurve->isIdentity()) {
        if (wavhueCurve) {
            delete wavhueCurve;
            wavhueCurve = nullptr;
        }
    } else {
        wavhueutili = true;
    }

    struct cont_params cp;

    cp.avoi = params->wavelet.avoid;

    if (params->wavelet.complexmethod == "normal") {
        cp.complex = 0;
    } else if (params->wavelet.complexmethod == "expert") {
        cp.complex = 1;
    } else {
        cp.complex = 0;
    }


    if (params->wavelet.Medgreinf == "more") {
        cp.reinforce = 1;
    } else if (params->wavelet.Medgreinf == "none") {
        cp.reinforce = 2;
    } else if (params->wavelet.Medgreinf == "less") {
        cp.reinforce = 3;
    }

    if (params->wavelet.NPmethod == "none") {
        cp.lip3 = false;
    } else if (params->wavelet.NPmethod == "low") {
        cp.lip3 = true;
        cp.neigh = 0;
    } else if (params->wavelet.NPmethod == "high") {
        cp.lip3 = true;
        cp.neigh = 1;
    }

    cp.lipp = params->wavelet.lipst;
    cp.diag = params->wavelet.tmr;
    cp.balan = (float)params->wavelet.balance;
    cp.ite = params->wavelet.iter;
    cp.tonemap = params->wavelet.tmrs != 0;
    cp.bam = false;
    cp.sigmafin = params->wavelet.sigmafin;
    cp.sigmaton = params->wavelet.sigmaton;
    cp.sigmacol = params->wavelet.sigmacol;
    cp.sigmadir = params->wavelet.sigmadir;
    cp.sigmm = params->wavelet.sigm;
    cp.levden = params->wavelet.levden;
    cp.thrden = 0.01f * params->wavelet.thrden;
    cp.limden = params->wavelet.limden;

    if (params->wavelet.TMmethod == "cont") {
        cp.contmet = 1;
    } else if (params->wavelet.TMmethod == "tm") {
        cp.contmet = 2;
    }


    cp.denmet = 4;
    //if (params->wavelet.denmethod == "equ") {
    //    cp.denmet = 0;
    //} else if (params->wavelet.denmethod == "high") {
    //    cp.denmet = 1;
    //} else if (params->wavelet.denmethod == "low") {
    //    cp.denmet = 2;
    //} else if (params->wavelet.denmethod == "12high") {
    //    cp.denmet = 3;
    //} else if (params->wavelet.denmethod == "12low") {
    //    cp.denmet = 4;
    //}

    if (params->wavelet.mixmethod == "nois") {
        cp.mixmet = 0;
    } else if (params->wavelet.mixmethod == "mix") {
        cp.mixmet = 1;
    } else if (params->wavelet.mixmethod == "mix7") {
        cp.mixmet = 2;
    } else if (params->wavelet.mixmethod == "den") {
        cp.mixmet = 3;
    }

    if (params->wavelet.quamethod == "cons") {
        cp.quamet = 0;
    } else if (params->wavelet.quamethod == "agre") {
        cp.quamet = 1;
    }

    if (params->wavelet.slimethod == "sli") {
        cp.slimet = 0;
    } else if (params->wavelet.slimethod == "cur") {
        cp.slimet = 1;
    }

    if (params->wavelet.BAmethod != "none") {
        cp.bam = true;

        if (params->wavelet.BAmethod == "sli") {
            cp.BAmet = 1;
        } else if (params->wavelet.BAmethod == "cur") {
            cp.BAmet = 2;
        }
    }
    int minwinnoise = rtengine::min(imwidth, imheight);

    cp.sigm = params->wavelet.sigma;

    cp.tmstrength = params->wavelet.tmrs;
    cp.contena = params->wavelet.expcontrast;
    cp.chromena = params->wavelet.expchroma;
    cp.edgeena = params->wavelet.expedge;
    cp.resena = params->wavelet.expresid;
    cp.finena = params->wavelet.expfinal;
    cp.toningena = params->wavelet.exptoning;
    cp.noiseena = params->wavelet.expnoise && minwinnoise > 128;//128 limit for 6 levels wavelet denoise issue 7146
    cp.blena = params->wavelet.expbl;
    cp.chrwav = 0.01f * params->wavelet.chrwav;

    if (params->wavelet.Backmethod == "black") {
        cp.backm = 0;
    } else if (params->wavelet.Backmethod == "grey") {
        cp.backm = 1;
    } else if (params->wavelet.Backmethod == "resid") {
        cp.backm = 2;
    }

    cp.link = params->wavelet.linkedg;
    cp.eddet = (float) params->wavelet.edgedetect;
    cp.eddetthr = (float) params->wavelet.edgedetectthr;
    cp.eddetthrHi = (float) params->wavelet.edgedetectthr2;

    cp.edgsens = 60.f;
    cp.edgampl = 10.f;

    if (cp.lipp) {
        cp.edgsens = (float) params->wavelet.edgesensi;
        cp.edgampl = (float) params->wavelet.edgeampli;
    }

    const int maxmul = params->wavelet.thres;
    cp.maxilev = maxmul;
    static const float scales[10] = {1.f, 2.f, 4.f, 8.f, 16.f, 32.f, 64.f, 128.f, 256.f, 512.f};
    float scaleskip[10];

    for (int sc = 0; sc < 10; sc++) {
        scaleskip[sc] = scales[sc] / skip;
    }

    constexpr float atten0 = 0.40f;
    constexpr float atten123 = 0.90f;

    //int DaubLen = settings->daubech ? 8 : 6;
    int DaubLen = 6;

    if (params->wavelet.daubcoeffmethod == "2_") {
        DaubLen = 4;
    } else if (params->wavelet.daubcoeffmethod == "4_") {
        DaubLen = 6;
    } else if (params->wavelet.daubcoeffmethod == "6_") {
        DaubLen = 8;
    } else if (params->wavelet.daubcoeffmethod == "10_") {
        DaubLen = 12;
    } else if (params->wavelet.daubcoeffmethod == "14_") {
        DaubLen = 16;
    } else if (params->wavelet.daubcoeffmethod == "20_"){
        DaubLen = 22;
    }

    cp.CHSLmet = 1;
    cp.EDmet = 2;
/*
    if (params->wavelet.EDmethod == "SL") {
        cp.EDmet = 1;
    } else if (params->wavelet.EDmethod == "CU") {
        cp.EDmet = 2;
    }
*/
    cp.cbena = params->wavelet.cbenab;
    cp.blhigh = (float)params->wavelet.bluehigh;
    cp.grhigh = (float)params->wavelet.greenhigh;
    cp.blmed = (float)params->wavelet.bluemed;
    cp.grmed = (float)params->wavelet.greenmed;
    cp.bllow = (float)params->wavelet.bluelow;
    cp.grlow = (float)params->wavelet.greenlow;
    cp.curv = false;
    cp.edgcurv = false;
    cp.diagcurv = false;
    cp.denoicurv = false;
    cp.denoicurvh = false;
    cp.opaRG = false;
    cp.opaBY = false;
    cp.opaW = false;
    cp.CHmet = 0;
    cp.HSmet = false;


    if (params->wavelet.CHmethod == "without") {
        cp.CHmet = 0;
    } else if (params->wavelet.CHmethod == "with") {
        cp.CHmet = 1;
    } else if (params->wavelet.CHmethod == "link") {
        cp.CHmet = 2;
    }

    if (params->wavelet.HSmethod == "with") {
        cp.HSmet = true;
    }

    cp.strength = rtengine::min(1.f, rtengine::max(0.f, ((float)params->wavelet.strength / 100.f)));

    for (int m = 0; m < maxmul; m++) {
        cp.mulC[m] = waparams.ch[m];
    }
    for (int m = maxmul; m < 9; m++) {
        cp.mulC[m] = 0.f;
    }
    
//printf("maxmul=%i\n", maxmul);
    cp.factor = WaveletParams::LABGRID_CORR_MAX * 3.276f;
    cp.scaling = WaveletParams::LABGRID_CORR_SCALE;
    cp.scaledirect = WaveletParams::LABGRIDL_DIRECT_SCALE;
    cp.a_scale = (params->wavelet.labgridAHigh - params->wavelet.labgridALow) / cp.factor / cp.scaling;
    cp.a_base = params->wavelet.labgridALow / cp.scaling;
    cp.b_scale = (params->wavelet.labgridBHigh - params->wavelet.labgridBLow) / cp.factor / cp.scaling;
    cp.b_base = params->wavelet.labgridBLow / cp.scaling;
    cp.a_high = 3.276f * params->wavelet.labgridAHigh;
    cp.a_low = 3.276f * params->wavelet.labgridALow;
    cp.b_high = 3.276f * params->wavelet.labgridBHigh;
    cp.b_low = 3.276f * params->wavelet.labgridBLow;
    cp.rangeab = params->wavelet.rangeab;
    cp.protab = params->wavelet.protab;

    if (waOpacityCurveRG) {
        cp.opaRG = true;
    }

    if (cp.opaRG) {
        cp.mulopaRG[0] = 200.f * (waOpacityCurveRG[0] - 0.5f);
        cp.mulopaRG[1] = 200.f * (waOpacityCurveRG[62] - 0.5f);
        cp.mulopaRG[2] = 200.f * (waOpacityCurveRG[125] - 0.5f);
        cp.mulopaRG[3] = 200.f * (waOpacityCurveRG[187] - 0.5f);
        cp.mulopaRG[4] = 200.f * (waOpacityCurveRG[250] - 0.5f);
        cp.mulopaRG[5] = 200.f * (waOpacityCurveRG[312] - 0.5f);
        cp.mulopaRG[6] = 200.f * (waOpacityCurveRG[375] - 0.5f);
        cp.mulopaRG[7] = 200.f * (waOpacityCurveRG[438] - 0.5f);
        cp.mulopaRG[8] = 200.f * (waOpacityCurveRG[500] - 0.5f);
    } else {
        for (int level = 0; level < 9; level++) {
            cp.mulopaRG[level] = 0.f;
        }
    }

    if (waOpacityCurveBY) {
        cp.opaBY = true;
    }

    if (cp.opaBY) {
        cp.mulopaBY[0] = 200.f * (waOpacityCurveBY[0] - 0.5f);
        cp.mulopaBY[1] = 200.f * (waOpacityCurveBY[62] - 0.5f);
        cp.mulopaBY[2] = 200.f * (waOpacityCurveBY[125] - 0.5f);
        cp.mulopaBY[3] = 200.f * (waOpacityCurveBY[187] - 0.5f);
        cp.mulopaBY[4] = 200.f * (waOpacityCurveBY[250] - 0.5f);
        cp.mulopaBY[5] = 200.f * (waOpacityCurveBY[312] - 0.5f);
        cp.mulopaBY[6] = 200.f * (waOpacityCurveBY[375] - 0.5f);
        cp.mulopaBY[7] = 200.f * (waOpacityCurveBY[438] - 0.5f);
        cp.mulopaBY[8] = 200.f * (waOpacityCurveBY[500] - 0.5f);
    } else {
        for (int level = 0; level < 9; level++) {
            cp.mulopaBY[level] = 0.f;
        }
    }

    if (wavCLVCcurve) {
        cp.edgcurv = true;
    }

    if (waOpacityCurveWL) {
        cp.diagcurv = true;
    }

    if (wavdenoise) {
        for (int i = 0; i < 500; i++) {
            if (wavdenoise[i] != 1.0) {
                cp.denoicurv  = true;
                break;
            }
        }
    }

    if(cp.complex == 0) {
        wavdenoiseh = wavdenoise;
    }

    if (wavdenoiseh) {
        for (int i = 0; i < 500; i++) {
            if (wavdenoiseh[i] != 1.0) {
                cp.denoicurvh  = true;
                break;
            }
        }
    }

    for (int m = 0; m < maxmul; m++) {
        cp.mul[m] = waparams.c[m];
    }
    for (int m = maxmul; m < 10; m++) {
        cp.mul[m] = 0.f;
    }

    cp.mul[9] = (float) waparams.sup;

    for (int sc = 0; sc < 10; sc++) { //reduce strength if zoom < 100%  for contrast
        if (sc == 0) {
            if (scaleskip[sc] < 1.f) {
                cp.mul[sc] *= (atten0 * scaleskip[sc]);
            }
        } else {
            if (scaleskip[sc] < 1.f) {
                cp.mul[sc] *= (atten123 * scaleskip[sc]);
            }
        }
    }

    for (int sc = 0; sc < 9; sc++) { //reduce strength if zoom < 100%  for chroma and tuning
        if (sc == 0) {
            if (scaleskip[sc] < 1.f) {
                cp.mulC[sc] *= (atten0 * scaleskip[sc]);
                cp.mulopaRG[sc] *= (atten0 * scaleskip[sc]);
                cp.mulopaBY[sc] *= (atten0 * scaleskip[sc]);
            }
        } else {
            if (scaleskip[sc] < 1.f) {
                cp.mulC[sc] *= (atten123 * scaleskip[sc]);
                cp.mulopaRG[sc] *= (atten123 * scaleskip[sc]);
                cp.mulopaBY[sc] *= (atten123 * scaleskip[sc]);
            }
        }
    }

    cp.chro = waparams.chro;
    cp.chrom = waparams.chroma;
    cp.contrast = waparams.contrast;
    cp.rad = waparams.edgrad;
    cp.val = waparams.edgval;
    cp.til = waparams.edgthresh;
    cp.eff = waparams.edgeffect;
    cp.balchrom = waparams.balchrom;
    cp.chromfi = 0.1f * waparams.chromfi;
    cp.chromco = 0.1f * waparams.chromco;
    cp.ballum = waparams.ballum;
    cp.conres = waparams.rescon;
    cp.conresH = waparams.resconH;
    cp.radius = waparams.radius;
    cp.chrores = waparams.reschro;
    cp.oldsh = waparams.oldsh;
    cp.blurres = waparams.resblur;
    cp.blurcres = waparams.resblurc;
    cp.bluwav = waparams.bluwav;
    //cp.hueres=waparams.reshue;
    cp.hueres = 2.f;
    cp.th = float(waparams.thr);
    cp.thH = float(waparams.thrH);
    cp.sky = waparams.sky;
    //skin
    cp.b_l = static_cast<float>(params->wavelet.hueskin.getBottomLeft()) / 100.0f;
    cp.t_l = static_cast<float>(params->wavelet.hueskin.getTopLeft()) / 100.0f;
    cp.b_r = static_cast<float>(params->wavelet.hueskin.getBottomRight()) / 100.0f;
    cp.t_r = static_cast<float>(params->wavelet.hueskin.getTopRight()) / 100.0f;

    cp.b_ly = static_cast<float>(params->wavelet.hueskin2.getBottomLeft()) / 100.0f;
    cp.t_ly = static_cast<float>(params->wavelet.hueskin2.getTopLeft()) / 100.0f;
    cp.b_ry = static_cast<float>(params->wavelet.hueskin2.getBottomRight()) / 100.0f;
    cp.t_ry = static_cast<float>(params->wavelet.hueskin2.getTopRight()) / 100.0f;
    cp.numlevH = params->wavelet.threshold -1;

    //shadows
    cp.b_lsl = static_cast<float>(params->wavelet.bllev.getBottomLeft());
    cp.t_lsl = static_cast<float>(params->wavelet.bllev.getTopLeft());
    cp.b_rsl = static_cast<float>(params->wavelet.bllev.getBottomRight());
    cp.t_rsl = static_cast<float>(params->wavelet.bllev.getTopRight());
    cp.numlevS = params->wavelet.threshold2; //rtengine::max(cp.numlevS, maxlevS);
    //highlight
    cp.b_lhl = static_cast<float>(params->wavelet.hllev.getBottomLeft());
    cp.t_lhl = static_cast<float>(params->wavelet.hllev.getTopLeft());
    cp.b_rhl = static_cast<float>(params->wavelet.hllev.getBottomRight());
    cp.t_rhl = static_cast<float>(params->wavelet.hllev.getTopRight());
    //pastel
    cp.b_lpast = static_cast<float>(params->wavelet.pastlev.getBottomLeft());
    cp.t_lpast = static_cast<float>(params->wavelet.pastlev.getTopLeft());
    cp.b_rpast = static_cast<float>(params->wavelet.pastlev.getBottomRight());
    cp.t_rpast = static_cast<float>(params->wavelet.pastlev.getTopRight());
    //saturated
    cp.b_lsat = static_cast<float>(params->wavelet.satlev.getBottomLeft());
    cp.t_lsat = static_cast<float>(params->wavelet.satlev.getTopLeft());
    cp.b_rsat = static_cast<float>(params->wavelet.satlev.getBottomRight());
    cp.t_rsat = static_cast<float>(params->wavelet.satlev.getTopRight());
    //edge local contrast
    cp.edg_low = static_cast<float>(params->wavelet.edgcont.getBottomLeft());
    cp.edg_mean = static_cast<float>(params->wavelet.edgcont.getTopLeft());
    cp.edg_max = static_cast<float>(params->wavelet.edgcont.getBottomRight());
    cp.edg_sd = static_cast<float>(params->wavelet.edgcont.getTopRight());
    //level noise
    cp.lev0s = static_cast<float>(params->wavelet.level0noise.getBottom());
    cp.lev0n = static_cast<float>(params->wavelet.level0noise.getTop());
    cp.lev1s = static_cast<float>(params->wavelet.level1noise.getBottom());
    cp.lev1n = static_cast<float>(params->wavelet.level1noise.getTop());
    cp.lev2s = static_cast<float>(params->wavelet.level2noise.getBottom());
    cp.lev2n = static_cast<float>(params->wavelet.level2noise.getTop());
    cp.lev3s = static_cast<float>(params->wavelet.level3noise.getBottom());
    cp.lev3n = static_cast<float>(params->wavelet.level3noise.getTop());
    cp.lev4n = static_cast<float>(params->wavelet.leveldenoise.getTop());
    cp.lev4t = 0.01f * static_cast<float>(params->wavelet.leveldenoise.getBottom());
    cp.sigmm14 = static_cast<float>(params->wavelet.levelsigm.getTop());
    cp.sigmm56 = static_cast<float>(params->wavelet.levelsigm.getBottom());

    cp.detectedge = params->wavelet.medianlev;
    int minwin = rtengine::min(imwidth, imheight);
    int maxlevelcrop = 9;

    if (cp.mul[9] != 0) {
        maxlevelcrop = 10;
    }

    // adapt maximum level wavelet to size of crop
    if (minwin * skip < 1024) {
        maxlevelcrop = 9;    //sampling wavelet 512
    }

    if (minwin * skip < 512) {
        maxlevelcrop = 8;    //sampling wavelet 256
    }

    if (minwin * skip < 256) {
        maxlevelcrop = 7;    //sampling 128
    }

    if (minwin * skip < 128) {
        maxlevelcrop = 6;
    }

    if (minwin * skip < 64) {
        maxlevelcrop = 5;
    }

    if (minwin * skip < 32) {
        maxlevelcrop = 4;
    }
    
    int levwav = params->wavelet.thres;
    if(params->wavelet.expnoise) {
        levwav = 6;
    }

    if (levwav == 9 && cp.mul[9] != 0) {
        levwav = 10;
    }

    levwav = rtengine::min(maxlevelcrop, levwav);

    // I suppress this functionality ==> crash for level < 3
    if (levwav < 1) {
        return;    // nothing to do
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // begin tile processing of image

    //output buffer
    int realtile = 0;

    if (params->wavelet.Tilesmethod == "big") {
        realtile = 22;
    }

    /*
        if (params->wavelet.Tilesmethod == "lit") {
            realtile = 12;
        }
    */
    int tilesize = 128 * realtile;
    int overlap = (int) tilesize * 0.125f;
    int numtiles_W, numtiles_H, tilewidth, tileheight, tileWskip, tileHskip;

    if (params->wavelet.Tilesmethod == "full") {
        kall = 0;
    }

    Tile_calc(tilesize, overlap, kall, imwidth, imheight, numtiles_W, numtiles_H, tilewidth, tileheight, tileWskip, tileHskip);

    const int numtiles = numtiles_W * numtiles_H;
    LabImage * dsttmp;

    if (numtiles == 1) {
        dsttmp = dst;
    } else {
        dsttmp = new LabImage(imwidth, imheight);

        for (int n = 0; n < 3 * imwidth * imheight; n++) {
            dsttmp->data[n] = 0;
        }
    }

    //now we have tile dimensions, overlaps
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    int minsizetile = rtengine::min(tilewidth, tileheight);
    int maxlev2 = 10;

    if (minsizetile < 1024 && levwav == 10) {
        maxlev2 = 9;
    }

    if (minsizetile < 512) {
        maxlev2 = 8;
    }

    if (minsizetile < 256) {
        maxlev2 = 7;
    }

    if (minsizetile < 128) {
        maxlev2 = 6;
    }

    if (minsizetile < 64) {
        maxlev2 = 5;
    }

    if (minsizetile < 32) {
        maxlev2 = 4;
    }

    levwav = rtengine::min(maxlev2, levwav);

#ifdef _OPENMP
    int numthreads = 1;
    int maxnumberofthreadsforwavelet = 0;

    //reduce memory for big tile size
    if (kall != 0) {
        if (realtile <= 22) {
            maxnumberofthreadsforwavelet = 2;
        }

        if (realtile <= 20) {
            maxnumberofthreadsforwavelet = 3;
        }

        if (realtile <= 18) {
            maxnumberofthreadsforwavelet = 4;
        }

        if (realtile <= 16) {
            maxnumberofthreadsforwavelet = 6;
        }

        if (realtile <= 14) {
            maxnumberofthreadsforwavelet = 8;
        }

        if ((maxnumberofthreadsforwavelet == 6 || maxnumberofthreadsforwavelet == 8)  && levwav == 10) {
            maxnumberofthreadsforwavelet -= 2;
        }

        if (levwav <= 7 && maxnumberofthreadsforwavelet == 8) {
            maxnumberofthreadsforwavelet = 0;
        }
    }



    // Calculate number of tiles. If less than omp_get_max_threads(), then limit num_threads to number of tiles
    const auto rgbDenoiseThreadLimit = App::get().options().rgbDenoiseThreadLimit;
    if (rgbDenoiseThreadLimit > 0) {
        maxnumberofthreadsforwavelet = rtengine::LIM(rgbDenoiseThreadLimit / 2, 1, maxnumberofthreadsforwavelet);
    }

    numthreads = rtengine::min(numtiles, omp_get_max_threads());

    if (maxnumberofthreadsforwavelet > 0) {
        numthreads = rtengine::min(numthreads, maxnumberofthreadsforwavelet);
    }

#ifdef _OPENMP
    wavNestedLevels = omp_get_max_threads() / numthreads;
    bool oldNested = omp_get_nested();

    if (wavNestedLevels < 2) {
        wavNestedLevels = 1;
    } else {
        omp_set_nested(true);
    }

    if (maxnumberofthreadsforwavelet > 0)
        while (wavNestedLevels * numthreads > maxnumberofthreadsforwavelet) {
            wavNestedLevels--;
        }

#endif

    if (settings->verbose) {
        printf("Ip Wavelet uses %d main thread(s) and up to %d nested thread(s) for each main thread\n", numthreads, wavNestedLevels);
    }

    #pragma omp parallel num_threads(numthreads)
#endif
    {
        float mean[10];
        float meanN[10];
        float sigma[10];
        float sigmaN[10];
        float MaxP[10];
        float MaxN[10];
        float meand[10];
        float meanNd[10];
        float sigmad[10];
        float sigmaNd[10];
        float MaxPd[10];
        float MaxNd[10];

        float meanab[10];
        float meanNab[10];
        float sigmaab[10];
        float sigmaNab[10];
        float MaxPab[10];
        float MaxNab[10];

        array2D<float> varchro(tilewidth, tileheight);

        float** varhue = new float*[tileheight];

        for (int i = 0; i < tileheight; i++) {
            varhue[i] = new float[tilewidth];
        }



#ifdef _OPENMP
        #pragma omp for schedule(dynamic) collapse(2)
#endif

        for (int tiletop = 0; tiletop < imheight; tiletop += tileHskip) {
            for (int tileleft = 0; tileleft < imwidth ; tileleft += tileWskip) {
                int tileright = rtengine::min(imwidth, tileleft + tilewidth);
                int tilebottom = rtengine::min(imheight, tiletop + tileheight);
                int width  = tileright - tileleft;
                int height = tilebottom - tiletop;
                LabImage * labco;
                float **Lold = nullptr;
                float *LoldBuffer = nullptr;

                if (numtiles == 1) { // untiled processing => we can use output buffer for labco
                    labco = dst;

                    if (cp.avoi) { // we need a buffer to hold a copy of the L channel
                        Lold = new float*[tileheight];
                        LoldBuffer = new float[tilewidth * tileheight];
                        memcpy(LoldBuffer, lab->L[0], tilewidth * tileheight * sizeof(float));

                        for (int i = 0; i < tileheight; i++) {
                            Lold[i] = LoldBuffer + i * tilewidth;
                        }
                    }

                } else {
                    labco = new LabImage(width, height);
                    Lold = lab->L;
                }

#ifdef _OPENMP
                #pragma omp parallel for num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

                for (int i = tiletop; i < tilebottom; i++) {
                    const int i1 = i - tiletop;
                    int j = tileleft;
#if defined(__SSE2__) || defined(RT_SIMDE)
                    const vfloat c327d68v = F2V(327.68f);

                    for (; j < tileright - 3; j += 4) {
                        const int j1 = j - tileleft;
                        const vfloat av = LVFU(lab->a[i][j]);
                        const vfloat bv = LVFU(lab->b[i][j]);
                        STVFU(varhue[i1][j1], xatan2f(bv, av));
                        STVFU(varchro[i1][j1], vsqrtf(SQRV(av) + SQRV(bv)) / c327d68v);

                        if (labco != lab) {
                            STVFU((labco->L[i1][j1]), LVFU(lab->L[i][j]));
                            STVFU((labco->a[i1][j1]), av);
                            STVFU((labco->b[i1][j1]), bv);
                        }
                    }

#endif

                    for (; j < tileright; j++) {
                        const int j1 = j - tileleft;
                        const float a = lab->a[i][j];
                        const float b = lab->b[i][j];
                        varhue[i1][j1] = xatan2f(b, a);
                        varchro[i1][j1] = (sqrtf(a * a + b * b)) / 327.68f;

                        if (labco != lab) {
                            labco->L[i1][j1] = lab->L[i][j];
                            labco->a[i1][j1] = a;
                            labco->b[i1][j1] = b;
                        }
                    }
                }

                //to avoid artifacts in blue sky
                if (params->wavelet.median) {
                    float** tmL;
                    int wid = labco->W;
                    int hei = labco->H;
                    int borderL = 1;
                    tmL = new float*[hei];

                    for (int i = 0; i < hei; i++) {
                        tmL[i] = new float[wid];
                    }

                    for (int i = borderL; i < hei - borderL; i++) {
                        for (int j = borderL; j < wid - borderL; j++) {
                            tmL[i][j] = labco->L[i][j];
                        }
                    }

#ifdef _OPENMP
                    #pragma omp parallel for num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

                    for (int i = 1; i < hei - 1; i++) {
                        for (int j = 1; j < wid - 1; j++) {
                            if ((varhue[i][j] < -1.3f && varhue[i][j] > - 2.5f)  && (varchro[i][j] > 15.f && varchro[i][j] < 55.f) && labco->L[i][j] > 6000.f) { //blue sky + med3x3  ==> after for more effect use denoise
                                tmL[i][j] = median(labco->L[i][j], labco->L[i - 1][j], labco->L[i + 1][j], labco->L[i][j + 1], labco->L[i][j - 1], labco->L[i - 1][j - 1], labco->L[i - 1][j + 1], labco->L[i + 1][j - 1], labco->L[i + 1][j + 1]);      //3x3
                            }
                        }
                    }

                    for (int i = borderL; i < hei - borderL; i++) {
                        for (int j = borderL; j < wid - borderL; j++) {
                            labco->L[i][j] = tmL[i][j];
                        }
                    }

                    for (int i = 0; i < hei; i++) {
                        delete [] tmL[i];
                    }

                    delete [] tmL;
                    // end blue sky
                }

                if (numtiles == 1) {
                    // reduce the varhue array to get faster access in following processing and reduce peak memory usage
                    float temphue[(tilewidth + 1) / 2] ALIGNED64;

                    for (int i = 0; i < (tileheight + 1) / 2; i++) {
                        for (int j = 0; j < (tilewidth + 1) / 2; j++) {
                            temphue[j] = varhue[i * 2][j * 2];
                        }

                        delete [] varhue[i];
                        varhue[i] = new float[(tilewidth + 1) / 2];
                        memcpy(varhue[i], temphue, ((tilewidth + 1) / 2) * sizeof(float));
                    }

                    for (int i = (tileheight + 1) / 2; i < tileheight; i++) {
                        delete [] varhue[i];
                        varhue[i] = nullptr;
                    }
                } else { // reduce the varhue array to get faster access in following processing
                    for (int i = 0; i < (tileheight + 1) / 2; i++) {
                        for (int j = 0; j < (tilewidth + 1) / 2; j++) {
                            varhue[i][j] = varhue[i * 2][j * 2];
                        }
                    }
                }

                int datalen = labco->W * labco->H;

                levwavL = levwav;
                bool ref0 = false;

                if ((cp.lev0s > 0.f || cp.lev1s > 0.f || cp.lev2s > 0.f || cp.lev3s > 0.f) && cp.noiseena) {
                    ref0 = true;
                }

                bool wavcurvecomp = false;//not enable if 0.75

                if (wavblcurve) {
                    for (int i = 0; i < 500; i++) {
                        if (wavblcurve[i] != 0.) {
                            wavcurvecomp = true;
                        }
                    }
                }

                bool exblurL = cp.blena && wavcurvecomp;

                if (exblurL) {
                    if (cp.mul[0] == 0.f) {
                        cp.mul[0] = 0.01f;//to always enable WaveletcontAllL if no contrast is needed
                    }
                }
                
                if (cp.BAmet != 0) {
                    if (cp.mul[0] == 0.f) {
                        cp.mul[0] = 0.01f;
                    }
                }
              //  printf("cp4=%f cpmul5=%f cp6=%f cp7=%f cp8=%f\n", (double)cp.mul[4], (double)cp.mul[5],(double)cp.mul[6],(double)cp.mul[7],(double)cp.mul[8]);

                if (levwavL == 6  && cp.noiseena && cp.chromfi == 0.f) {
                    cp.chromfi = 0.01f;
                }

                if (cp.chromfi > 0.f || cp.chromco > 0.f) {
                    if (levwavL < 7) {
                        levwavL = 7;
                    }
                }
                if (levwavL < 5 && cp.noiseena) {
                    levwavL = 6;    //to allow edge and denoise  => I always allocate 3 (4) levels..because if user select wavelet it is to do something !!
                }
                
                if (!exblurL && cp.contrast == 0.f && cp.blurres == 0.f && !cp.noiseena && !cp.tonemap && !cp.resena && !cp.chromena && !cp.toningena && !cp.finena && !cp.edgeena && cp.conres == 0.f && cp.conresH == 0.f && cp.val == 0  && !ref0 && params->wavelet.CLmethod == "all") { // no processing of residual L or edge=> we probably can reduce the number of levels
                    while (levwavL > 0 && cp.mul[levwavL - 1] == 0.f) { // cp.mul[level] == 0.f means no changes to level
                        levwavL--;
                    }
                }
                bool isdenoisL = (cp.lev0n > 0.1f || cp.lev1n > 0.1f || cp.lev2n > 0.1f || cp.lev3n > 0.1f || cp.lev4n > 0.1f);

                
/*
                if(cp.denoicurvh  || cp.levdenhigh > 0.01f) {
                    levwavL = levwav;
                }
*/                
                float th = 0.01f * (float) waparams.thrend;
                if(th > 0.f) {
                    levwavL = levwav;
                }


                bool usechrom = cp.chromfi > 0.f || cp.chromco > 0.f;
                levwavL = rtengine::min(maxlevelcrop, levwavL);
                levwavL = rtengine::min(maxlev2, levwavL);

                if (settings->verbose) {
                    printf("Level decomp L=%i\n", levwavL);
                }

                if (levwavL > 0) {
                    const std::unique_ptr<wavelet_decomposition> Ldecomp(new wavelet_decomposition(labco->data, labco->W, labco->H, levwavL, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
                 //   const std::unique_ptr<wavelet_decomposition> Ldecomp2(new wavelet_decomposition(labco->data, labco->W, labco->H, levwavL, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));

                    if (!Ldecomp->memory_allocation_failed()) {
                        float madL[10][3];

                        //     float madL[8][3];
#ifdef _OPENMP
                        #pragma omp parallel for schedule(dynamic) collapse(2) num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

                        for (int lvl = 0; lvl < levwavL; lvl++) {
                            for (int dir = 1; dir < 4; dir++) {
                                int Wlvl_L = Ldecomp->level_W(lvl);
                                int Hlvl_L = Ldecomp->level_H(lvl);

                                const float* const* WavCoeffs_L = Ldecomp->level_coeffs(lvl);

                                madL[lvl][dir - 1] = SQR(Mad(WavCoeffs_L[dir], Wlvl_L * Hlvl_L));

                                if (settings->verbose) {
                                  //  printf("Luminance noise estimate (sqr) madL=%.0f lvl=%i dir=%i\n", madL[lvl][dir - 1], lvl, dir - 1);
                                }
                            }
                        }

                        bool ref = false;

                        if ((cp.lev0s > 0.f || cp.lev1s > 0.f || cp.lev2s > 0.f || cp.lev3s > 0.f) && cp.noiseena) {
                            ref = true;
                            
                        }

                        bool contr = false;

                        for (int f = 0; f < levwavL; f++) {
                            if (cp.mul[f] != 0.f) {
                                contr = true;
                            }
                        }

                     //   if (cp.val > 0 || ref || contr || cp.denoicurv || cp.denoicurvh || cp.noiseena  || cp.levdenlow > 0.f || cp.thrden > 0.f ) { //edge
                        if (cp.val > 0 || ref || contr || cp.denoicurv || cp.denoicurvh || cp.noiseena  || cp.thrden > 0.f ) { //edge
                            Evaluate2(*Ldecomp, mean, meanN, sigma, sigmaN, MaxP, MaxN, wavNestedLevels); 
                        }

                        //init for edge and denoise
                        float vari[6];

                        vari[0] = 0.8f * SQR((cp.lev0n / 125.f) * (1.f + cp.lev0n / 25.f));
                        vari[1] = 0.8f * SQR((cp.lev1n / 125.f) * (1.f + cp.lev1n / 25.f));
                        vari[2] = 0.8f * SQR((cp.lev2n / 125.f) * (1.f + cp.lev2n / 25.f));
                        vari[3] = 0.8f * SQR((cp.lev3n / 125.f) * (1.f + cp.lev3n / 25.f));
                        vari[4] = 0.8f * SQR((cp.lev4n / 125.f) * (1.f + cp.lev4n / 25.f));
                        vari[5] = 0.8f * SQR((cp.lev4n / 125.f) * (1.f + cp.lev4n / 25.f));
                        float kr3 = 1.f;

                        if (cp.lev3n < 10.f) {
                            kr3 = 0.3f;
                        } else if (cp.lev3n < 30.f) {
                            kr3 = 0.6f;
                        } else if (cp.lev3n < 70.f) {
                            kr3 = 0.8f;
                        } else {
                            kr3 = 1.f;
                        }

                        float kr4 = 1.f;

                        if (cp.lev4n < 10.f) {
                            kr4 = 0.6f;
                        } else if (cp.lev4n < 30.f) {
                            kr4 = 0.8f;
                        } else if (cp.lev4n < 70.f) {
                            kr4 = 0.9f;
                        } else {
                            kr4 = 1.f;
                        }
                        if ((cp.lev0n > 0.1f || cp.lev1n > 0.1f || cp.lev2n > 0.1f || cp.lev3n > 0.1f || cp.lev4n > 0.1f) && cp.noiseena) {
                            int edge = 6;
                            vari[0] = rtengine::max(0.000001f, vari[0]);
                            vari[1] = rtengine::max(0.000001f, vari[1]);
                            vari[2] = rtengine::max(0.000001f, vari[2]);
                            vari[3] = rtengine::max(0.000001f, kr3 * vari[3]);
                            vari[4] = rtengine::max(0.000001f, kr4 * vari[4]);
                            vari[5] = rtengine::max(0.000001f, kr4 * vari[5]);
                            
                            const std::unique_ptr<wavelet_decomposition> Ldecomp2(new wavelet_decomposition(labco->data, labco->W, labco->H, levwavL, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
                            if(!Ldecomp2->memory_allocation_failed()){
                                if (settings->verbose) {
                                    printf("LUM var0=%f var1=%f var2=%f var3=%f var4=%f\n", vari[0], vari[1], vari[2], vari[3], vari[4]);
                                }

                            //     float* noisevarlum = nullptr;  // we need a dummy to pass it to WaveletDenoiseAllL
                                int GWL = labco->W;
                                int GHL = labco->H;
                                float* noisevarlum = new float[GHL * GWL];
                                float* noisevarhue = new float[GHL * GWL];
                                int GW2L = (GWL + 1) / 2;

                                constexpr float nvlh[13] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.7f, 0.5f}; //high value
                                constexpr float nvll[13] = {0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.7f, 0.8f, 1.f, 1.f, 1.f}; //low value

                                constexpr float seuillow = 3000.f;//low
                                constexpr float seuilhigh = 18000.f;//high
                                const int index = 10 - cp.ballum;
                                const float ac = (nvlh[index] - nvll[index]) / (seuillow - seuilhigh);
                                const float bc = nvlh[index] - seuillow * ac;

#ifdef _OPENMP
                                #pragma omp parallel for
#endif
                                for (int ir = 0; ir < GHL; ir++)
                                    for (int jr = 0; jr < GWL; jr++) {
                                        float lN = labco->L[ir][jr];

                                        if (lN < seuillow) {
                                            noisevarlum[(ir >> 1)*GW2L + (jr >> 1)] =  nvlh[index];
                                        } else if (lN < seuilhigh) {
                                            noisevarlum[(ir >> 1)*GW2L + (jr >> 1)] = ac * lN + bc;
                                        } else {
                                            noisevarlum[(ir >> 1)*GW2L + (jr >> 1)] =  nvll[index];
                                        }
                                    }

                                if(wavhueutili) {
#ifdef _OPENMP
        #pragma omp parallel for
#endif
                                    for (int ir = 0; ir < GHL; ir++)
                                        for (int jr = 0; jr < GWL; jr++) {
                                            float hueG = xatan2f(labco->b[ir][jr], labco->a[ir][jr]);
                                            noisevarhue[(ir >> 1)*GW2L + (jr >> 1)] = 1.f +  2.f * (static_cast<float>(wavhueCurve->getVal(Color::huelab_to_huehsv2(hueG))) - 0.5f);
                                            noisevarlum[(ir >> 1)*GW2L + (jr >> 1)] *= noisevarhue[(ir >> 1)*GW2L + (jr >> 1)];
                                        }
                                }

        


                                if(cp.quamet == 0) {
                                    if (settings->verbose) {
                                        printf("denoise standard L\n");
                                    }
                                    WaveletDenoiseAllL(*Ldecomp, noisevarlum, madL, vari, edge, 1);
                                } else {
                                    if (settings->verbose) {
                                        printf("denoise bishrink L\n");
                                    }
                                    WaveletDenoiseAll_BiShrinkL(*Ldecomp, noisevarlum, madL, vari, edge, 1);

                                    WaveletDenoiseAllL(*Ldecomp, noisevarlum, madL, vari, edge, 1);
                                }
                                delete[] noisevarlum;

                                //evaluate after denoise
                                bool exitifzero = true;
                                Evaluate2(*Ldecomp, meand, meanNd, sigmad, sigmaNd, MaxPd, MaxNd, wavNestedLevels);
                                for (int dir = 1; dir < 4; dir++) {
                                    for (int level = 0; level < levwavL; level++) {
                                        if(mean[level] < 0.1f || meand[level] < 0.1f || sigma[level] < 0.1f || sigmad[level] < 0.1f) {
                                            printf("near zero - exit\n");
                                            exitifzero = false;
                                        }
                                    }
                                }
                                
                                //for level 0 1 2 3
                                float thr = 0.f;
                                float thrend = cp.thrden; //cp.levdenlow;
                                if(thrend < 0.01f) thr = 0.95f;
                                else if(thrend < 0.02f) thr = 0.9f;
                                else if(thrend < 0.04f) thr = 0.8f;
                                else if(thrend < 0.06f) thr = 0.7f;
                                else if(thrend < 0.08f) thr = 0.6f;
                                else if(thrend < 0.1f) thr = 0.5f;
                                else if(thrend < 0.2f) thr = 0.2f;
                                else thr = 0.f;

                                FlatCurve wavlow({
                                    FCT_MinMaxCPoints,
                                    0, 1, 0.35, 0.35,thrend, 1.0, 0.35, 0.35, thrend + 0.01f, thr, 0.35, 0.35, 1, thr, 0.35, 0.35
                                });
                                //for level 4 
                                float thrhigh = 0.f;
                                float threndhigh = cp.lev4t; //cp.levdenlow;
                                if(threndhigh < 0.01f) thrhigh = 0.95f;
                                else if(threndhigh < 0.02f) thrhigh = 0.9f;
                                else if(threndhigh < 0.04f) thrhigh = 0.8f;
                                else if(threndhigh < 0.06f) thrhigh = 0.7f;
                                else if(threndhigh < 0.08f) thrhigh = 0.6f;
                                else if(threndhigh < 0.1f) thrhigh = 0.5f;
                                else thrhigh = 0.f;

                                FlatCurve wavhigh({
                                    FCT_MinMaxCPoints,
                                    0, 1, 0.35, 0.35,threndhigh, 1.0, 0.35, 0.35, threndhigh + 0.01f, thrhigh, 0.35, 0.35, 1, thrhigh, 0.35, 0.35
                                });
                                
                                float thrmed = 0.f;
                                float threndmed = 1.f - cp.limden;
                                if(threndmed < 0.02f) thrmed = 0.5f;
                                else if(threndmed < 0.05f) thrmed = 0.2f;
                                else thrmed = 0.f;

                                FlatCurve wavmed({
                                    FCT_MinMaxCPoints,
                                    0, 1, 0.35, 0.35,threndmed, 1.0, 0.35, 0.35, threndmed + 0.01f, thrmed, 0.35, 0.35, 1, thrmed, 0.35, 0.35
                                });
                                
                                float siglh[10];
                                float levref = 6;
                                //levref = levwavL-1;
                                if(cp.complex == 1){
                                    for (int level = 0; level < levref; level++) {
                                        if(level > 3) {
                                            siglh[level] = cp.sigmm56;
                                        } else {
                                            siglh[level] = cp.sigmm14;
                                        }
                                    }
                                } else {
                                    levref = 4;
                                    for (int level = 0; level < levref; level++) {
                                            siglh[level] = cp.sigmm;
                                    }
                                }
                                
//                                    printf("sig0=%f sig1=%f sig2=%f sig3=%f sig4=%f sig5=%f\n", siglh[0], siglh[1],siglh[2],siglh[3],siglh[4],siglh[5]);
                                
                                
                                bool execut = false;
                                
                                if(cp.slimet == 0) {
                                   // if(cp.levdenlow > 0.f) {
                                    if(cp.thrden > 0.f) {
                                        execut = true;
                                    }
                                } else {
                                    if(cp.denoicurv) {
                                        execut = true;
                                    }
                                }
                               // }
                                if (execut && exitifzero) {
                                    for (int dir = 1; dir < 4; dir++) {
                                        for (int level = 0; level < levref; level++) {
                                            const int Wlvl_L = Ldecomp->level_W(level);
                                            const int Hlvl_L = Ldecomp->level_H(level);
                                            const float* const WavCoeffs_L = Ldecomp->level_coeffs(level)[dir];//first decomp denoised
                                            const float* const WavCoeffs_L2 = Ldecomp2->level_coeffs(level)[dir];//second decomp before denoise
                                            const int k4 = cp.complex == 1 ? 4 : 3;
                                            const int k5 = cp.complex == 1 ? 5 : 3;

                                            auto WavL0 = Ldecomp->level_coeffs(0)[dir];
                                            auto WavL1 = Ldecomp->level_coeffs(1)[dir];
                                            auto WavL2 = Ldecomp->level_coeffs(2)[dir];
                                            auto WavL3 = Ldecomp->level_coeffs(3)[dir];
                                            auto WavL4 = Ldecomp->level_coeffs(k4)[dir];
                                            auto WavL5 = Ldecomp->level_coeffs(k5)[dir];
                                               //not denoise
                                            const auto WavL02 = Ldecomp2->level_coeffs(0)[dir];
                                            const auto WavL12 = Ldecomp2->level_coeffs(1)[dir];
                                            const auto WavL22 = Ldecomp2->level_coeffs(2)[dir];
                                            const auto WavL32 = Ldecomp2->level_coeffs(3)[dir];
                                            const auto WavL42 = Ldecomp2->level_coeffs(k4)[dir];
                                            const auto WavL52 = Ldecomp2->level_coeffs(k5)[dir];
                                            if (settings->verbose) {
                                                printf("level=%i mean=%.0f meanden=%.0f sigma=%.0f  sigmaden=%.0f Max=%.0f Maxden=%.0f\n", level, mean[level], meand[level], sigma[level], sigmad[level],MaxP[level], MaxPd[level]);
                                            }

                                            //find local contrast
                                            constexpr float weights[4] = {0.f, 0.5f, 0.7f, 1.f};
                                            const float weightL = weights[rtengine::LIM(cp.mixmet, 0, 3)];
                                            const float tempmean = intp(weightL, meand[level], mean[level]);
                                            const float tempsig = intp(weightL, sigmad[level], sigma[level]);
                                            const float tempmax = intp(weightL, MaxPd[level], MaxP[level]);

                                            if (MaxP[level] > 0.f && mean[level] != 0.f && sigma[level] != 0.f) { //curve
                                                constexpr float insigma = 0.666f; //SD
                                                const float logmax = log(tempmax); //log Max
                                                //cp.sigmm change the "wider" of sigma
                                                const float rapX = (tempmean + siglh[level] * tempsig) / tempmax; //rapport between sD / max
                                                const float inx = log(insigma);
                                                const float iny = log(rapX);
                                                const float rap = inx / iny; //koef
                                                const float asig = 0.166f / (tempsig * siglh[level]);
                                                const float bsig = 0.5f - asig * tempmean;
                                                const float amean = 1.f / tempmean;

                                                //equalizer for levels 0 1 and 3...  1.33 and 0.75 arbitrary values
                                                float kcFactor = 1.f;
                                                if(cp.denmet == 1) {
                                                    if(level == 0 || level == 3) {
                                                        kcFactor = 1.7f;
                                                    }
                                                } else if(cp.denmet == 2) {
                                                    if(level == 0 || level == 3) {
                                                        kcFactor = 0.3f;
                                                    }
                                                } else if(cp.denmet == 3) {
                                                    if(level == 0 || level == 1) {
                                                        kcFactor = 1.7f;
                                                    }
                                                } else if(cp.denmet == 4) {
                                                    if(level == 0 || level == 1) {
                                                        kcFactor = 0.3f;
                                                    }
                                                }
                                                const float k = 1.f / (siglh[level] > 1.f ? SQR(siglh[level]) : siglh[level]);
                                                const float maxVal = tempmean + siglh[level] * tempsig;

#ifdef _OPENMP
                                                #pragma omp parallel for schedule(dynamic, Wlvl_L * 16) num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif
                                                for (int i = 0; i < Wlvl_L * Hlvl_L; i++) {
                                                    float absciss;
                                                    const float tempwav = std::fabs(intp(weightL, WavCoeffs_L[i], WavCoeffs_L2[i]));

                                                    if (tempwav >= maxVal) { //for max
                                                        absciss = xexpf((xlogf(tempwav) - logmax) * rap);
                                                    } else if (tempwav >= tempmean) {
                                                        absciss = asig * tempwav + bsig;
                                                    } else {
                                                        absciss = 0.5f * pow_F(amean * tempwav, k);
                                                    }

                                                    float kc;
                                                    if (cp.slimet == 0) {
                                                        kc = wavlow.getVal(absciss) - 1.f;
                                                    } else {
                                                        kc = wavdenoise[absciss * 500.f] - 1.f;
                                                    }
                                                    if (kc < 0) {
                                                        kc = -SQR(kc); //approximation to simulate sliders denoise
                                                    }
                                                    kc *= kcFactor;

                                                    const float reduceeffect = kc <= 0.f ? 1.f : 1.2f; //1.2 allows to increase denoise (not used)

                                                    if (level < 4) {
                                                        float kintermlow = 1.f + reduceeffect * kc;
                                                        kintermlow = kintermlow <= 0.f ? 0.01f : kintermlow;
                                                        WavL0[i] = WavL02[i] + (WavL0[i] - WavL02[i]) * kintermlow;
                                                        WavL1[i] = WavL12[i] + (WavL1[i] - WavL12[i]) * kintermlow;
                                                        WavL2[i] = WavL22[i] + (WavL2[i] - WavL22[i]) * kintermlow;
                                                        WavL3[i] = WavL32[i] + (WavL3[i] - WavL32[i]) * kintermlow;
                                                    } 
                                                    if (cp.complex == 1){
                                                        if (cp.limden > 0.f) {
                                                            const float kcmed = -SQR(wavmed.getVal(absciss) - 1.f);
                                                            float kintermed = 1.f + reduceeffect * kcmed;
                                                            kintermed = kintermed <= 0.f ? 0.01f : kintermed;
                                                            WavL0[i] = WavL02[i] + (WavL0[i] - WavL02[i]) * kintermed;
                                                            WavL1[i] = WavL12[i] + (WavL1[i] - WavL12[i]) * kintermed;
                                                            WavL2[i] = WavL22[i] + (WavL2[i] - WavL22[i]) * kintermed;
                                                            WavL3[i] = WavL32[i] + (WavL3[i] - WavL32[i]) * kintermed;
                                                        }
                                                        const float kchigh = -SQR(wavhigh.getVal(absciss) - 1.f);
                                                        float kintermhigh = 1.f + reduceeffect * kchigh;
                                                        kintermhigh = kintermhigh <= 0.f ? 0.01f : kintermhigh;
                                                        WavL4[i] = WavL42[i] + (WavL4[i] - WavL42[i]) * kintermhigh;
                                                        WavL5[i] = WavL52[i] + (WavL5[i] - WavL52[i]) * kintermhigh;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (settings->verbose) {
                                        Evaluate2(*Ldecomp, meand, meanNd, sigmad, sigmaNd, MaxPd, MaxNd, wavNestedLevels);
                                            for (int dir = 1; dir < 4; dir++) {
                                                for (int level = 0; level < levref; level++) {
                                                        printf("AFTER LC level=%i mean=%.0f meanden=%.0f sigma=%.0f  sigmaden=%.0f Max=%.0f Maxden=%.0f\n", level, mean[level], meand[level], sigma[level], sigmad[level],MaxP[level], MaxPd[level]);
                                                }
                                            }
                                    }
                                    
                                }
                              delete[] noisevarhue;
                            }
                        }
                        //Flat curve for Contrast=f(H) in levels
                        FlatCurve* ChCurve = new FlatCurve(params->wavelet.Chcurve); //curve C=f(H)
                        bool Chutili = false;

                        if (!ChCurve || ChCurve->isIdentity()) {
                            if (ChCurve) {
                                delete ChCurve;
                                ChCurve = nullptr;
                            }
                        } else {
                            Chutili = true;
                        }

                        WaveletcontAllL(labco, varhue, varchro, *Ldecomp, wavblcurve, cp, skip, mean, sigma, MaxP, MaxN, wavCLVCcurve, waOpacityCurveW, waOpacityCurveSH, ChCurve, Chutili);

                        if (cp.val > 0 || ref || contr  || cp.diagcurv) { //edge
                            Evaluate2(*Ldecomp, mean, meanN, sigma, sigmaN, MaxP, MaxN, wavNestedLevels);
                        }

                        WaveletcontAllLfinal(*Ldecomp, cp, mean, sigma, MaxP, waOpacityCurveWL);

                        //Evaluate2(*Ldecomp, cp, ind, mean, meanN, sigma, sigmaN, MaxP, MaxN, madL);
                        /*
                                                Ldecomp->reconstruct(labco->data, cp.strength);
                                            }
                                        }
                        */
                        if (!usechrom) {
                            Ldecomp->reconstruct(labco->data, cp.strength);
                        }

                        float variC[7];
                        float variCb[7];

                        float noisecfr = cp.chromfi;
                        float noiseccr = cp.chromco;

                        if (cp.balchrom > 0.f) {
                            noisecfr = cp.chromfi + 0.1f * cp.balchrom;
                            noiseccr = cp.chromco + 0.1f * cp.balchrom;
                        }

                        float noisecfb = cp.chromfi;
                        float noiseccb = cp.chromco;

                        if (cp.balchrom < 0.f) {
                            noisecfb = cp.chromfi - 0.1f * cp.balchrom;
                            noiseccb = cp.chromco - 0.1f * cp.balchrom;
                        }


                        if (noisecfr < 0.f) {
                            noisecfr = 0.00001f;
                        }

                        if (noiseccr < 0.f) {
                            noiseccr = 0.00001f;
                        }

                        if (noisecfb < 0.f) {
                            noisecfb = 0.00001f;
                        }

                        if (noiseccb < 0.f) {
                            noiseccb = 0.0001f;
                        }
                        int edge = 2;
                        variC[0] = SQR(noisecfr);
                        variC[1] = SQR(noisecfr);
                        variC[2] = SQR(noisecfr);

                        variC[3] = SQR(noisecfr);
                        variC[4] = SQR(noisecfr);
                        variC[5] = SQR(noiseccr);
                        variC[6] = SQR(noiseccr);

                        variCb[0] = SQR(noisecfb);
                        variCb[1] = SQR(noisecfb);
                        variCb[2] = SQR(noisecfb);

                        variCb[3] = SQR(noisecfb);
                        variCb[4] = SQR(noisecfb);
                        variCb[5] = SQR(noiseccb);
                        variCb[6] = SQR(noiseccb);

                        float k1 = 0.f;
                        float k2 = 0.f;
                        float k3 = 0.f;

                        if (cp.chromfi < 0.2f) {
                            k1 = 0.05f;
                            k2 = 0.f;
                            k3 = 0.f;
                        } else if (cp.chromfi < 0.3f) {
                            k1 = 0.1f;
                            k2 = 0.0f;
                            k3 = 0.f;
                        } else if (cp.chromfi < 0.5f) {
                            k1 = 0.2f;
                            k2 = 0.1f;
                            k3 = 0.f;
                        } else if (cp.chromfi < 0.8f) {
                            k1 = 0.3f;
                            k2 = 0.25f;
                            k3 = 0.f;
                        } else if (cp.chromfi < 1.f) {
                            k1 = 0.4f;
                            k2 = 0.25f;
                            k3 = 0.1f;
                        } else if (cp.chromfi < 2.f) {
                            k1 = 0.5f;
                            k2 = 0.3f;
                            k3 = 0.15f;
                        } else if (cp.chromfi < 3.f) {
                            k1 = 0.6f;
                            k2 = 0.45f;
                            k3 = 0.3f;
                        } else if (cp.chromfi < 4.f) {
                            k1 = 0.7f;
                            k2 = 0.5f;
                            k3 = 0.4f;
                        } else if (cp.chromfi < 5.f) {
                            k1 = 0.8f;
                            k2 = 0.6f;
                            k3 = 0.5f;
                        } else if (cp.chromfi < 6.f) {
                            k1 = 0.85f;
                            k2 = 0.7f;
                            k3 = 0.6f;
                        } else if (cp.chromfi < 8.f) {
                            k1 = 0.9f;
                            k2 = 0.8f;
                            k3 = 0.7f;
                        } else if (cp.chromfi < 10.f) {
                            k1 = 1.f;
                            k2 = 1.f;
                            k3 = 0.9f;

                        } else {
                            k1 = 1.f;
                            k2 = 1.f;
                            k3 = 1.f;
                        }

                        float minic = 0.000001f;
                        variC[0] = max(minic, variC[0]);
                        variC[1] = max(minic, k1 * variC[1]);
                        variC[2] = max(minic, k2 * variC[2]);
                        variC[3] = max(minic, k3 * variC[3]);

                        variCb[0] = max(minic, variCb[0]);
                        variCb[1] = max(minic, k1 * variCb[1]);
                        variCb[2] = max(minic, k2 * variCb[2]);
                        variCb[3] = max(minic, k3 * variCb[3]);

                        float k4 = 0.f;
                        float k5 = 0.f;
                        float k6 = 0.f;

                        if (cp.chromco < 0.2f) {
                            k4 = 0.1f;
                            k5 = 0.02f;
                        } else if (cp.chromco < 0.5f) {
                            k4 = 0.15f;
                            k5 = 0.05f;
                        } else if (cp.chromco < 1.f) {
                            k4 = 0.15f;
                            k5 = 0.1f;
                        } else if (cp.chromco < 3.f) {
                            k4 = 0.3f;
                            k5 = 0.15f;
                        } else if (cp.chromco < 4.f) {
                            k4 = 0.6f;
                            k5 = 0.4f;
                        } else if (cp.chromco < 6.f) {
                            k4 = 0.8f;
                            k5 = 0.6f;
                        } else {
                            k4 = 1.f;
                            k5 = 1.f;
                        }

                        variC[4] = max(0.000001f, k4 * variC[4]);
                        variC[5] = max(0.000001f, k5 * variC[5]);
                        variCb[4] = max(0.000001f, k4 * variCb[4]);
                        variCb[5] = max(0.000001f, k5 * variCb[5]);

                        if (cp.chromco < 4.f) {
                            k6 = 0.f;
                        } else if (cp.chromco < 5.f) {
                            k6 = 0.4f;
                        } else if (cp.chromco < 6.f) {
                            k6 = 0.7f;
                        } else {
                            k6 = 1.f;
                        }

                        variC[6] = max(0.00001f, k6 * variC[6]);
                        variCb[6] = max(0.00001f, k6 * variCb[6]);

                        if (settings->verbose) {
                            printf("CHRO var0=%f va1=%f va2=%f va3=%f va4=%f val5=%f va6=%f\n", variC[0], variC[1], variC[2], variC[3], variC[4], variC[5], variC[6]);
                        }

                        /*
                                                for (int y = 0; y < 7; y++) {
                                                    printf("y=%i madL=%f varia=%f variab=%f\n", y, madL[y][1], variC[y], variCb[y]);
                                                }
                        */
                        float nvch = 0.6f;//high value
                        float nvcl = 0.1f;//low value

                        if (cp.chromco > 30.f) {
                            nvch = 0.8f;
                            nvcl = 0.4f;
                        }

                        float seuil = 4000.f;//low
                        float seuil2 = 15000.f;//high
                        //ac and bc for transition
                        float ac = (nvch - nvcl) / (seuil - seuil2);
                        float bc = nvch - seuil * ac;
                        int GW = labco->W;
                        int GH = labco->H;
                        float* noisevarchrom = new float[GH * GW];
                        //noisevarchrom in function chroma
                        int GW2 = (GW + 1) / 2;
                        float noisevarab_r = 100.f;

                        for (int ir = 0; ir < GH; ir++)
                            for (int jr = 0; jr < GW; jr++) {
                                float cN = sqrt(SQR(labco->a[ir][jr]) + SQR(labco->b[ir][jr]));

                                if (cN < seuil) {
                                    noisevarchrom[(ir >> 1)*GW2 + (jr >> 1)] =  nvch;
                                } else if (cN < seuil2) {
                                    noisevarchrom[(ir >> 1)*GW2 + (jr >> 1)] = ac * cN + bc;
                                } else {
                                    noisevarchrom[(ir >> 1)*GW2 + (jr >> 1)] =  nvcl;
                                }
                            }


                        //Flat curve for H=f(H) in residual image
                        FlatCurve* hhCurve = new FlatCurve(params->wavelet.hhcurve); //curve H=f(H)
                        bool hhutili = false;

                        if (!hhCurve || hhCurve->isIdentity()) {
                            if (hhCurve) {
                                delete hhCurve;
                                hhCurve = nullptr;
                            }
                        } else {
                            hhutili = true;
                        }

                        bool exblurab = cp.chrwav > 0.f && exblurL;

                        if (!hhutili) { //always a or b
                            int levwava = levwav;

                            if (!exblurab && cp.chrores == 0.f  && cp.blurcres == 0.f && !cp.noiseena && !cp.tonemap && !cp.resena && !cp.chromena && !cp.toningena && !cp.finena && !cp.edgeena && params->wavelet.CLmethod == "all" && !cp.cbena) { // no processing of residual ab => we probably can reduce the number of levels
                                while (levwava > 0 && !cp.diag && (((cp.CHmet == 2 && (cp.chro == 0.f || cp.mul[levwava - 1] == 0.f)) || (cp.CHmet != 2 && (levwava == 10 || (!cp.curv  || cp.mulC[levwava - 1] == 0.f))))) && (!cp.opaRG || levwava == 10 || (cp.opaRG && cp.mulopaRG[levwava - 1] == 0.f)) && ((levwava == 10 || (cp.CHSLmet == 1 && cp.mulC[levwava - 1] == 0.f)))) {
                                    levwava--;
                                }
                            }

                            if (cp.chromfi > 0.f || cp.chromco > 0.f) {
                                if (levwava < 7) {
                                    levwava = 7;
                                }
                            }

                            levwava = rtengine::min(maxlevelcrop, levwava);
                            levwava = rtengine::min(maxlev2, levwava);
                            levwava = rtengine::min(levwav, levwava);
                            if (settings->verbose) {
                                printf("Leval decomp a=%i\n", levwava);
                            }

                            if (levwava > 0) {
                                const std::unique_ptr<wavelet_decomposition> adecomp(new wavelet_decomposition(labco->data + datalen, labco->W, labco->H, levwava, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
                                if (!adecomp->memory_allocation_failed()) {
                                    if(levwava == 6) {
                                        edge = 1;
                                    }
                                    if (cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) && cp.quamet == 0 && isdenoisL)) {
                                        if (settings->verbose) {
                                            printf("denoise standard a \n");
                                        }

                                       WaveletDenoiseAllAB(*Ldecomp, *adecomp, noisevarchrom, madL, variC, edge, noisevarab_r, true, false, false, 1);

                                    } else if (cp.noiseena && ((cp.chromfi > 0.f && cp.chromco >= 0.f) && cp.quamet == 1 && isdenoisL)){
                                        if (settings->verbose) {
                                            printf("denoise bishrink a \n");
                                        }

                                        WaveletDenoiseAll_BiShrinkAB(*Ldecomp, *adecomp, noisevarchrom, madL, variC, edge, noisevarab_r, true, false, false, 1);
                                        WaveletDenoiseAllAB(*Ldecomp, *adecomp, noisevarchrom, madL, variC, edge, noisevarab_r, true, false, false, 1);
                                       
                                    }

                                    Evaluate2(*adecomp, meanab, meanNab, sigmaab, sigmaNab, MaxPab, MaxNab, wavNestedLevels);

                                    WaveletcontAllAB(labco, varhue, varchro, *adecomp, wavblcurve, waOpacityCurveW, cp, true, skip, meanab, sigmaab);

                                    adecomp->reconstruct(labco->data + datalen, cp.strength);

                                }
                            }

                            int levwavb = levwav;

                            if (!exblurab && cp.chrores == 0.f && cp.blurcres == 0.f && !cp.noiseena && !cp.tonemap && !cp.resena && !cp.chromena && !cp.toningena && !cp.finena && !cp.edgeena && params->wavelet.CLmethod == "all" && !cp.cbena) { // no processing of residual ab => we probably can reduce the number of levels
                                while (levwavb > 0 &&  !cp.diag && (((cp.CHmet == 2 && (cp.chro == 0.f || cp.mul[levwavb - 1] == 0.f)) || (cp.CHmet != 2 && (levwavb == 10 || (!cp.curv || cp.mulC[levwavb - 1] == 0.f))))) && (!cp.opaBY || levwavb == 10 || (cp.opaBY && cp.mulopaBY[levwavb - 1] == 0.f)) && ((levwavb == 10 || (cp.CHSLmet == 1 && cp.mulC[levwavb - 1] == 0.f)))) {
                                    levwavb--;
                                }
                            }

                            if (cp.chromfi > 0.f || cp.chromco > 0.f) {
                                if (levwavb < 7) {
                                    levwavb = 7;
                                }
                            }


                            levwavb = rtengine::min(maxlevelcrop, levwavb);
                            levwavb = rtengine::min(maxlev2, levwavb);
                            levwavb = rtengine::min(levwav, levwavb);

                            if (settings->verbose) {
                                printf("Leval decomp b=%i\n", levwavb);
                            }

                            if (levwavb > 0) {
                                const std::unique_ptr<wavelet_decomposition> bdecomp(new wavelet_decomposition(labco->data + 2 * datalen, labco->W, labco->H, levwavb, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
                                if(levwavb == 6) {
                                    edge = 1;
                                }

                                if (!bdecomp->memory_allocation_failed()) {
                                  //  if (cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) && cp.chromco < 2.f )) {
                                    if (cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) &&  cp.quamet == 0 && isdenoisL)) {
                                        WaveletDenoiseAllAB(*Ldecomp, *bdecomp, noisevarchrom, madL, variCb, edge, noisevarab_r, true, false, false, 1);
                                        if (settings->verbose) {
                                            printf("Denoise standard b\n");
                                        }
                                    } else if (cp.noiseena && ((cp.chromfi > 0.f && cp.chromco >= 0.f) && cp.quamet == 1 && isdenoisL)){

                                        WaveletDenoiseAll_BiShrinkAB(*Ldecomp, *bdecomp, noisevarchrom, madL, variCb, edge, noisevarab_r, true, false, false, 1);
                                        WaveletDenoiseAllAB(*Ldecomp, *bdecomp, noisevarchrom, madL, variCb, edge, noisevarab_r, true, false, false, 1);
                                        if (settings->verbose) {
                                            printf("Denoise bishrink b\n");
                                        }

                                    }

                                    Evaluate2(*bdecomp, meanab, meanNab, sigmaab, sigmaNab, MaxPab, MaxNab, wavNestedLevels);
                                    WaveletcontAllAB(labco, varhue, varchro, *bdecomp, wavblcurve, waOpacityCurveW, cp, false, skip, meanab, sigmaab);
                                    bdecomp->reconstruct(labco->data + 2 * datalen, cp.strength);
                                }
                            }
                        } else {// a and b
                            int levwavab = levwav;

                            if (cp.chromfi > 0.f || cp.chromco > 0.f) {
                                if (levwavab < 7) {
                                    levwavab = 7;
                                }
                            }

                            if (levwavab > 0) {
                                const std::unique_ptr<wavelet_decomposition> adecomp(new wavelet_decomposition(labco->data + datalen, labco->W, labco->H, levwavab, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
                                const std::unique_ptr<wavelet_decomposition> bdecomp(new wavelet_decomposition(labco->data + 2 * datalen, labco->W, labco->H, levwavab, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));

                                if (!adecomp->memory_allocation_failed() && !bdecomp->memory_allocation_failed()) {
                                    if (cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) && cp.quamet == 0 && isdenoisL)) {
                                        WaveletDenoiseAllAB(*Ldecomp, *adecomp, noisevarchrom, madL, variC, edge, noisevarab_r, true, false, false, 1);
                                        if (settings->verbose) {
                                            printf("Denoise standard ab\n");
                                        }
                                    } else if (cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) && cp.quamet == 1 && isdenoisL)) { 
                                        WaveletDenoiseAll_BiShrinkAB(*Ldecomp, *adecomp, noisevarchrom, madL, variC, edge, noisevarab_r, true, false, false, 1);
                                        WaveletDenoiseAllAB(*Ldecomp, *adecomp, noisevarchrom, madL, variC, edge, noisevarab_r, true, false, false, 1);
                                        if (settings->verbose) {
                                            printf("Denoise bishrink ab\n");
                                        }
                                    }

                                    Evaluate2(*adecomp, meanab, meanNab, sigmaab, sigmaNab, MaxPab, MaxNab, wavNestedLevels);
                                    WaveletcontAllAB(labco, varhue, varchro, *adecomp, wavblcurve, waOpacityCurveW, cp, true, skip, meanab, sigmaab);
                                    if (cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) && cp.quamet == 0 && isdenoisL)) {
                                        WaveletDenoiseAllAB(*Ldecomp, *bdecomp, noisevarchrom, madL, variCb, edge, noisevarab_r, true, false, false, 1);
                                    } else if(cp.noiseena && ((cp.chromfi > 0.f || cp.chromco > 0.f) && cp.quamet == 1 && isdenoisL)) {
                                        WaveletDenoiseAll_BiShrinkAB(*Ldecomp, *bdecomp, noisevarchrom, madL, variCb, edge, noisevarab_r, true, false, false, 1);
                                        WaveletDenoiseAllAB(*Ldecomp, *bdecomp, noisevarchrom, madL, variCb, edge, noisevarab_r, true, false, false, 1);
                                    }

                                    Evaluate2(*bdecomp, meanab, meanNab, sigmaab, sigmaNab, MaxPab, MaxNab, wavNestedLevels);

                                    WaveletcontAllAB(labco, varhue, varchro, *bdecomp, wavblcurve, waOpacityCurveW, cp, false, skip, meanab, sigmaab);
                                    WaveletAandBAllAB(*adecomp, *bdecomp, cp, hhCurve, hhutili);

                                    adecomp->reconstruct(labco->data + datalen, cp.strength);
                                    bdecomp->reconstruct(labco->data + 2 * datalen, cp.strength);

                                }
                            }
                        }

                        delete[] noisevarchrom;

                        if (hhCurve) {
                            delete hhCurve;
                        }

                        if (usechrom) {
                            Ldecomp->reconstruct(labco->data, cp.strength);
                        }
 
                    }
                }

                if (numtiles > 1 || (numtiles == 1 /*&& cp.avoi*/)) { //in all case since I add contrast curve
                    //calculate mask for feathering output tile overlaps
                    float Vmask[height + overlap] ALIGNED16;
                    float Hmask[width + overlap] ALIGNED16;

                    if (numtiles > 1) {
                        for (int i = 0; i < height; i++) {
                            Vmask[i] = 1;
                        }

                        for (int j = 0; j < width; j++) {
                            Hmask[j] = 1;
                        }

                        for (int i = 0; i < overlap; i++) {
                            float mask = SQR(sin((rtengine::RT_PI * i) / (2 * overlap)));

                            if (tiletop > 0) {
                                Vmask[i] = mask;
                            }

                            if (tilebottom < imheight) {
                                Vmask[height - i] = mask;
                            }

                            if (tileleft > 0) {
                                Hmask[i] = mask;
                            }

                            if (tileright < imwidth) {
                                Hmask[width - i] = mask;
                            }
                        }
                    }

                    bool highlight = params->toneCurve.hrenabled;

#ifdef _OPENMP
                    #pragma omp parallel for schedule(dynamic,16) num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

                    for (int i = tiletop; i < tilebottom; i++) {
                        const int i1 = i - tiletop;
                        float L, a, b;
#if defined(__SSE2__) || defined(RT_SIMDE)
                        const int rowWidth = tileright - tileleft;
                        float atan2Buffer[rowWidth] ALIGNED64;
                        float chprovBuffer[rowWidth] ALIGNED64;
                        float xBuffer[rowWidth] ALIGNED64;
                        float yBuffer[rowWidth] ALIGNED64;

                        if (cp.avoi) {
                            int col = 0;
                            const vfloat onev = F2V(1.f);
                            const vfloat c327d68v = F2V(327.68f);

                            for (; col < rowWidth - 3; col += 4) {
                                const vfloat av = LVFU(labco->a[i1][col]);
                                const vfloat bv = LVFU(labco->b[i1][col]);
                                STVF(atan2Buffer[col], xatan2f(bv, av));

                                const vfloat cv = vsqrtf(SQRV(av) + SQRV(bv));
                                vfloat yv = av / cv;
                                vfloat xv = bv / cv;
                                const vmask xyMask = vmaskf_eq(ZEROV, cv);
                                yv = vself(xyMask, onev, yv);
                                xv = vselfnotzero(xyMask, xv);
                                STVF(yBuffer[col], yv);
                                STVF(xBuffer[col], xv);
                                STVF(chprovBuffer[col], cv / c327d68v);

                            }

                            for (; col < rowWidth; col++) {
                                const float la = labco->a[i1][col];
                                const float lb = labco->b[i1][col];
                                atan2Buffer[col] = xatan2f(lb, la);
                                const float Chprov1 = sqrtf(SQR(la) + SQR(lb));
                                yBuffer[col] = (Chprov1 == 0.f) ? 1.f : la / Chprov1;
                                xBuffer[col] = (Chprov1 == 0.f) ? 0.f : lb / Chprov1;
                                chprovBuffer[col] = Chprov1 / 327.68f;
                            }
                        }

#endif

                        for (int j = tileleft; j < tileright; j++) {
                            const int j1 = j - tileleft;

                            if (cp.avoi) { //Gamut and Munsell
#if defined(__SSE2__) || defined(RT_SIMDE)
                                float HH = atan2Buffer[j1];
                                float Chprov1 = chprovBuffer[j1];
                                float2 sincosv;
                                sincosv.y = yBuffer[j1];
                                sincosv.x = xBuffer[j1];
#else
                                a = labco->a[i1][j1];
                                b = labco->b[i1][j1];
                                float HH = xatan2f(b, a);
                                float Chprov1 = sqrtf(SQR(a) + SQR(b));
                                float2 sincosv;
                                sincosv.y = (Chprov1 == 0.0f) ? 1.f : a / (Chprov1);
                                sincosv.x = (Chprov1 == 0.0f) ? 0.f : b / (Chprov1);
                                Chprov1 /= 327.68f;
#endif
                                const float Lin = labco->L[i1][j1];

                                if (wavclCurve  && cp.finena) {
                                    labco->L[i1][j1] = (0.5f * Lin  + 1.5f * wavclCurve[Lin]) / 2.f;   //apply contrast curve
                                }

                                L = labco->L[i1][j1];

                                float Lprov1 = L / 327.68f;
                                float Lprov2 = Lold[i][j] / 327.68f;
                                float memChprov = varchro[i1][j1];
                                float R, G, B;
                                Color::gamutLchonly(HH, sincosv, Lprov1, Chprov1, R, G, B, wip, highlight, 0.15f, 0.96f);
                                L = Lprov1 * 327.68f;

                                a = 327.68f * Chprov1 * sincosv.y; //gamut
                                b = 327.68f * Chprov1 * sincosv.x; //gamut
                                float correctionHue = 0.0f; // Munsell's correction
                                float correctlum = 0.0f;
                                Lprov1 = L / 327.68f;
                                const float Chprov = sqrtf(SQR(a) + SQR(b)) / 327.68f;
                                Color::AllMunsellLch(true, Lprov1, Lprov2, HH, Chprov, memChprov, correctionHue, correctlum);

                                if (correctionHue != 0.f || correctlum != 0.f) { // only calculate sin and cos if HH changed
                                    if (std::fabs(correctionHue) < 0.015f) {
                                        HH += correctlum;    // correct only if correct Munsell chroma very little.
                                    }

                                    sincosv = xsincosf(HH + correctionHue);
                                }

                                a = 327.68f * Chprov * sincosv.y; // apply Munsell
                                b = 327.68f * Chprov * sincosv.x; // apply Munsell
                            } else {//general case
                                L = labco->L[i1][j1];
                                const float Lin = std::max(0.f, L);

                                if (wavclCurve  && cp.finena) {
                                    labco->L[i1][j1] = (0.5f * Lin + 1.5f * wavclCurve[Lin]) / 2.f;   //apply contrast curve
                                }

                                L = labco->L[i1][j1];
                                a = labco->a[i1][j1];
                                b = labco->b[i1][j1];
                            }

                            if (numtiles > 1) {
                                float factor = Vmask[i1] * Hmask[j1];
                                if(L <= 0.f) {
                                    L= 1.f;
                                }
                                dsttmp->L[i][j] += factor * L;
                                dsttmp->a[i][j] += factor * a;
                                dsttmp->b[i][j] += factor * b;
                            } else {
                                if(L <= 0.f) {
                                    L= 1.f;
                                }
                                dsttmp->L[i][j] = L;
                                dsttmp->a[i][j] = a;
                                dsttmp->b[i][j] = b;

                            }
                        }
                    }
                }

                if (LoldBuffer != nullptr) {
                    delete [] LoldBuffer;
                    delete [] Lold;
                }

                if (numtiles > 1) {
                    delete labco;
                }
            }
        }

        for (int i = 0; i < tileheight; i++)
            if (varhue[i] != nullptr) {
                delete [] varhue[i];
            }

        delete [] varhue;
    }
#ifdef _OPENMP
    omp_set_nested(oldNested);
#endif

    if (numtiles != 1) {
        dst->CopyFrom(dsttmp);
        delete dsttmp;
    }

    if (waparams.softradend > 0.f  && cp.finena) {
        float guid = waparams.softradend;
        float strend = waparams.strend;
        float detend = (float) waparams.detend;
        float thrend = 0.01f * (float) waparams.thrend;
        int ww = lab->W;
        int hh = lab->H;
        array2D<float> LL(ww, hh);
        array2D<float> LLbef(ww, hh);
        array2D<float> LAbef(ww, hh);
        array2D<float> LBbef(ww, hh);
        array2D<float> guide(ww, hh);
        const float blend = LIM01(float(strend) / 100.f);
        float mean[10];
        float meanN[10];
        float sigma[10];
        float sigmaN[10];
        float MaxP[10];
        float MaxN[10];
        float meang[10];
        float meanNg[10];
        float sigmag[10];
        float sigmaNg[10];
        float MaxPg[10];
        float MaxNg[10];

        bool multiTh = false;

#ifdef _OPENMP

        if (numthreads > 1) {
            multiTh = true;
        }

        #pragma omp parallel for
#endif
        for (int y = 0; y < hh; y++) {
            for (int x = 0; x < ww; x++) {
                LL[y][x] = dst->L[y][x];
                LLbef[y][x] = dst->L[y][x];
                LAbef[y][x] = dst->a[y][x];
                LBbef[y][x] = dst->b[y][x];
                float ll = LL[y][x] / 32768.f;
                guide[y][x] = xlin2log(rtengine::max(ll, 0.f), 10.f);
            }
        }
        array2D<float> iL(ww, hh, LL, 0);
        int r = rtengine::max(int(guid / skip), 1);

        const float epsil = 0.001f * std::pow(2, - detend);
        rtengine::guidedFilterLog(guide, 10.f, LL, r, epsil, multiTh);
        //take Hue to modulate LL
        //LL in function of LLbef and Labef Lbbef
        if(wavguidutili) {
#ifdef _OPENMP
        #pragma omp parallel for
#endif
            for (int y = 0; y < hh ; y++) {
                for (int x = 0; x < ww; x++) {
                    float hueG = xatan2f(LBbef[y][x], LAbef[y][x]);
                    float valparam = 1.f * (static_cast<float>(wavguidCurve->getVal(Color::huelab_to_huehsv2(hueG))) - 0.5f);
                    LL[y][x] = LLbef[y][x] + (LL[y][x] - LLbef[y][x]) * (1.f + valparam);
                }
            }
        }
        //end hue
        
        
    if (thrend > 0.f) {
        //2 decomposition LL after guidefilter and dst before (perhaps dst no need)
        const std::unique_ptr<wavelet_decomposition> LdecompLL(new wavelet_decomposition(LL[0], ww, hh, levwavL, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
        const std::unique_ptr<wavelet_decomposition> Ldecompdst(new wavelet_decomposition(dst->L[0], ww, hh, levwavL, 1, skip, rtengine::max(1, wavNestedLevels), DaubLen));
        if (!LdecompLL->memory_allocation_failed() && !Ldecompdst->memory_allocation_failed()) {
            Evaluate2(*LdecompLL, meang, meanNg, sigmag, sigmaNg, MaxPg, MaxNg, wavNestedLevels);
            Evaluate2(*Ldecompdst, mean, meanN, sigma, sigmaN, MaxP, MaxN, wavNestedLevels);
            constexpr float sig = 2.f;

            // original code for variable sig
            float k = sig;
            // cppcheck-suppress knownConditionTrueFalse
            if (sig > 1.f) {
                k = SQR(sig);
            }

            float thr = 0.f;
            if (thrend < 0.02f) thr = 0.5f;
            else if (thrend < 0.1f) thr = 0.2f;
            else thr = 0.f;

            FlatCurve wavguid({
                FCT_MinMaxCPoints,
                0, 1, 0.35, 0.35,thrend, 1.0, 0.35, 0.35, thrend + 0.01f, thr, 0.35, 0.35, 1, thr, 0.35, 0.35
            });

            for (int dir = 1; dir < 4; dir++) {
                for (int level = 0; level < levwavL-1; level++) {
                    const int Wlvl_L = LdecompLL->level_W(level);
                    const int Hlvl_L = LdecompLL->level_H(level);
                    float* const* WavCoeffs_L = LdecompLL->level_coeffs(level);//first decomp denoised
                    float* const* WavCoeffs_L2 = Ldecompdst->level_coeffs(level);//second decomp before denoise
                    if (settings->verbose) {
                        printf("level=%i mean=%.0f meanden=%.0f sigma=%.0f  sigmaden=%.0f Max=%.0f Maxden=%.0f\n", level, mean[level], meang[level], sigma[level], sigmag[level],MaxP[level], MaxPg[level]);
                    }

                    //find local contrast
                    const float tempmean = 0.3f * mean[level] + 0.7f * meang[level];
                    const float tempsig = 0.3f * sigma[level] + 0.7f * sigmag[level];
                    const float tempmax = 0.3f * MaxP[level] + 0.7f * MaxPg[level];
                                            
                    if (MaxP[level] > 0.f && mean[level] != 0.f && sigma[level] != 0.f) { //curve
                        constexpr float insigma = 0.666f; //SD
                        const float logmax = log(tempmax); //log Max
                        const float rapX = (tempmean + sig * tempsig) / tempmax; //rapport between sD / max
                        const float inx = log(insigma);
                        const float iny = log(rapX);
                        const float rap = inx / iny; //koef
                        const float asig = 0.166f / (tempsig * sig);
                        const float bsig = 0.5f - asig * tempmean;
                        const float amean = 1.f / tempmean;

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic, Wlvl_L * 16) num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

                        for (int i = 0; i < Wlvl_L * Hlvl_L; i++) {
                            float absciss;
                            const float tempwav = std::fabs(0.7f * WavCoeffs_L[dir][i] + 0.3f * WavCoeffs_L2[dir][i]);

                            if (tempwav >= tempmean + sig * tempsig) { //for max
                                const float vald = (xlogf(tempwav) - logmax) * rap;
                                absciss = xexpf(vald);
                            } else if (tempwav >= tempmean) {
                                absciss = asig * tempwav + bsig;
                            } else {
                                absciss = amean * tempwav;
                                if (sig == 2.f) { // for sig = 2.f we can use a faster calculation because the exponent in this case is 0.25
                                    absciss = 0.5f * std::sqrt(std::sqrt(absciss));
                                } else { // original code for variable sig
                                    absciss = 0.5f * pow_F(absciss, 1.f / k);
                                }
                            }
                            float kc = wavguid.getVal(absciss) - 1.f;
                            kc = kc < 0.f ? -SQR(kc) : kc; // approximation to simulate sliders denoise

                            const float reduceeffect = kc <= 0.f ? 1.f : 1.2f;//1.2 allows to increase denoise (not used)
                            const float kinterm = rtengine::max(1.f + reduceeffect * kc, 0.f);
                            WavCoeffs_L[dir][i] = intp(kinterm, WavCoeffs_L[dir][i], WavCoeffs_L2[dir][i]); // interpolate using kinterm
                        }
                    }
                }
            }
            LdecompLL->reconstruct(LL[0], cp.strength);
        }
    }

        //end local contrast
#ifdef _OPENMP
        #pragma omp parallel for
#endif
        for (int y = 0; y < hh ; y++) {
            for (int x = 0; x < ww; x++) {
                LL[y][x] = intp(blend, LL[y][x] , iL[y][x]);
                dst->L[y][x] = LL[y][x];
            }
        }
    }
}


void ImProcFunctions::Aver(const float* RESTRICT DataList, int datalen, float &averagePlus, float &averageNeg, float &max, float &min, int numThreads)
{

    //find absolute mean
    int countP = 0, countN = 0;
    double averaP = 0.0, averaN = 0.0; // use double precision for large summations

    constexpr float thres = 32.7f;//different from zero to take into account only data large enough 32.7 = 0.1 in range 0..100 very low value
    max = 0.f;
    min = RT_INFINITY_F;
#ifdef _OPENMP
    #pragma omp parallel num_threads(numThreads) if (numThreads>1)
#endif
    {
        float lmax = 0.f, lmin = 0.f;
#ifdef _OPENMP
        #pragma omp for reduction(+:averaP,averaN,countP,countN) nowait
#endif

        for (int i = 0; i < datalen; i++) {
            if (DataList[i] >= thres) {
                averaP += static_cast<double>(DataList[i]);
                lmax = rtengine::max(lmax, DataList[i]);
                countP++;
            } else if (DataList[i] < -thres) {
                averaN += static_cast<double>(DataList[i]);
                lmin = rtengine::min(lmin, DataList[i]);
                countN++;
            }
        }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            max = rtengine::max(max, lmax);
            min = rtengine::min(min, lmin);
        }
    }

    if (countP > 0) {
        averagePlus = averaP / countP;
    } else {
        averagePlus = 0;
    }

    if (countN > 0) {
        averageNeg = averaN / countN;
    } else {
        averageNeg = 0;
    }

}


void ImProcFunctions::Sigma(const float* RESTRICT DataList, int datalen, float averagePlus, float averageNeg, float &sigmaPlus, float &sigmaNeg, int numThreads)
{
    int countP = 0, countN = 0;
    double variP = 0.0, variN = 0.0; // use double precision for large summations
    float thres = 32.7f;//different from zero to take into account only data large enough 32.7 = 0.1 in range 0..100

#ifdef _OPENMP
    #pragma omp parallel for reduction(+:variP,variN,countP,countN) num_threads(numThreads) if (numThreads>1)
#endif

    for (int i = 0; i < datalen; i++) {
        if (DataList[i] >= thres) {
            variP += static_cast<double>(SQR(DataList[i] - averagePlus));
            countP++;
        } else if (DataList[i] <= -thres) {
            variN += static_cast<double>(SQR(DataList[i] - averageNeg));
            countN++;
        }
    }

    if (countP > 0) {
        sigmaPlus = sqrt(variP / countP);
    } else {
        sigmaPlus = 0;
    }

    if (countN > 0) {
        sigmaNeg = sqrt(variN / countN);
    } else {
        sigmaNeg = 0;
    }

}

void ImProcFunctions::Evaluate2(const wavelet_decomposition &WaveletCoeffs_L, float *mean, float *meanN, float *sigma, float *sigmaN, float *MaxP, float *MaxN, int numThreads)
{
//StopWatch Stop1("Evaluate2");
    int maxlvl = WaveletCoeffs_L.maxlevel();

    for (int lvl = 0; lvl < maxlvl; lvl++) {

        int Wlvl_L = WaveletCoeffs_L.level_W(lvl);
        int Hlvl_L = WaveletCoeffs_L.level_H(lvl);

        const float* const* WavCoeffs_L = WaveletCoeffs_L.level_coeffs(lvl);

        Eval2(WavCoeffs_L, lvl, Wlvl_L, Hlvl_L, mean, meanN, sigma, sigmaN, MaxP, MaxN, numThreads);
    }

}

void ImProcFunctions::calceffect(int level, float *mean, float *sigma, float *mea, float effect, float offs)
{
    float rap = 0.f;
    float sig = 1.f;

    if (effect < 1.f) {
        sig = effect;
    }

    if (effect <= 1.f) {
        rap =  offs * mean[level] -  sig * sigma[level];
    }

    if (rap > 0.f) {
        mea[0] = rap;
    } else {
        mea[0] = mean[level] / 6.f;
    }

    rap = 0.f;

    if (effect <= 1.f) {
        rap =  offs * mean[level] - 0.5f * sig * sigma[level];
    }

    if (rap > 0.f) {
        mea[1] = rap;
    } else {
        mea[1] = mean[level] / 4.f;
    }

    rap = 0.f;

    if (effect <= 1.f) {
        rap =  offs * mean[level] - 0.2f * sig * sigma[level];
    }

    if (rap > 0.f) {
        mea[2] = rap;
    } else {
        mea[2] = mean[level] / 2.f;
    }

    mea[3] = offs * mean[level]; // 50% data
    mea[4] = offs * mean[level] + effect * sigma[level] / 2.f;
    mea[5] = offs * mean[level] + effect * sigma[level]; //66%
    mea[6] = offs * mean[level] + effect * 1.2f * sigma[level];
    mea[7] = offs * mean[level] + effect * 1.5f * sigma[level]; //
    mea[8] = offs * mean[level] + effect * 2.f * sigma[level]; //95%
    mea[9] = offs * mean[level] + effect * 2.5f * sigma[level]; //99%
}

void ImProcFunctions::Eval2(const float* const* WavCoeffs_L, int level, int W_L, int H_L, float *mean, float *meanN, float *sigma, float *sigmaN, float *MaxP, float *MaxN, int numThreads)
{

    float avLP[4], avLN[4];
    float maxL[4], minL[4];
    float sigP[4], sigN[4];
    float AvL, AvN, SL, SN, maxLP, maxLN;

    for (int dir = 1; dir < 4; dir++) {
        Aver(WavCoeffs_L[dir], W_L * H_L, avLP[dir], avLN[dir], maxL[dir], minL[dir], numThreads);
        Sigma(WavCoeffs_L[dir], W_L * H_L, avLP[dir], avLN[dir], sigP[dir], sigN[dir], numThreads);
    }

    AvL = 0.f;
    AvN = 0.f;
    SL = 0.f;
    SN = 0.f;
    maxLP = 0.f;
    maxLN = 0.f;

    for (int dir = 1; dir < 4; dir++) {
        AvL += avLP[dir];
        AvN += avLN[dir];
        SL += sigP[dir];
        SN += sigN[dir];
        maxLP += maxL[dir];
        maxLN += minL[dir];
    }

    AvL /= 3;
    AvN /= 3;
    SL /= 3;
    SN /= 3;
    maxLP /= 3;
    maxLN /= 3;

    mean[level] = AvL;
    meanN[level] = AvN;
    sigma[level] = SL;
    sigmaN[level] = SN;
    MaxP[level] = maxLP;
    MaxN[level] = maxLN;
}

void ImProcFunctions::CompressDR(float *Source, int W_L, int H_L, float Compression, float DetailBoost)
{
    const int n = W_L * H_L;

    float exponent;

    if (DetailBoost > 0.f && DetailBoost < 0.05f) {
        float betemp = expf(-(2.f - DetailBoost + 0.694f)) - 1.f; //0.694 = log(2)
        exponent = 1.2f * xlogf(-betemp);
        exponent /= 20.f;
    } else if (DetailBoost >= 0.05f && DetailBoost < 0.25f) {
        float betemp = expf(-(2.f - DetailBoost + 0.694f)) - 1.f; //0.694 = log(2)
        exponent = 1.2f * xlogf(-betemp);
        exponent /= (-75.f * DetailBoost + 23.75f);
    } else if (DetailBoost >= 0.25f) {
        float betemp = expf(-(2.f - DetailBoost + 0.694f)) - 1.f; //0.694 = log(2)
        exponent = 1.2f * xlogf(-betemp);
        exponent /= (-2.f * DetailBoost + 5.5f);
    } else {
        exponent = (Compression - 1.0f) / 20.f;
    }

    exponent += 1.f;

    // now calculate Source = pow(Source, exponent)
#if defined(__SSE2__) || defined(RT_SIMDE)
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        const vfloat exponentv = F2V(exponent);
#ifdef _OPENMP
        #pragma omp for
#endif

        for (int i = 0; i < n - 3; i += 4) {
            STVFU(Source[i], xexpf(xlogf(LVFU(Source[i])) * exponentv));
        }
    }

    for (int i = n - (n % 4); i < n; i++) {
        Source[i] = xexpf(xlogf(Source[i]) * exponent);
    }

#else
#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int i = 0; i < n; i++) {
        Source[i] = xexpf(xlogf(Source[i]) * exponent);
    }

#endif

}

void ImProcFunctions::ContrastResid(float * WavCoeffs_L0, const cont_params &cp, int W_L, int H_L, float max0)
{
    const float stren = cp.tmstrength;
    const float gamm = params->wavelet.gamma;

#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int i = 0; i < W_L * H_L; i++) {
        WavCoeffs_L0[i] *= (gamm / max0);
    }

    const float Compression = std::exp(-stren);       //This modification turns numbers symmetric around 0 into exponents.
    const float DetailBoost = std::max(stren, 0.f);   //Go with effect of exponent only if uncompressing.

    CompressDR(WavCoeffs_L0, W_L, H_L, Compression, DetailBoost);

    max0 /= gamm;
#ifdef _OPENMP
    #pragma omp parallel for            // removed schedule(dynamic,10)
#endif

    for (int ii = 0; ii < W_L * H_L; ii++) {
        WavCoeffs_L0[ii] *= max0;
    }
}

void ImProcFunctions::EPDToneMapResid(float * WavCoeffs_L0, unsigned int Iterates, int skip, const cont_params& cp, int W_L, int H_L, float max0)
{


    const float stren = cp.tmstrength;
    const float edgest = params->wavelet.edgs;
    const float sca = params->wavelet.scale;
    const float gamm = params->wavelet.gamma;
    constexpr int rew = 0; //params->epd.reweightingIterates;

    EdgePreservingDecomposition epd2(W_L, H_L);

#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int i = 0; i < W_L * H_L; i++) {
        WavCoeffs_L0[i] *= (gamm / max0);
    }

    const float Compression = std::exp(-stren);       //This modification turns numbers symmetric around 0 into exponents.
    const float DetailBoost = std::max(stren, 0.f);   //Go with effect of exponent only if uncompressing.

    //Auto select number of iterates. Note that p->EdgeStopping = 0 makes a Gaussian blur.
    if (Iterates == 0) {
        Iterates = (unsigned int)(edgest * 15.0f);
    }

    epd2.CompressDynamicRange(WavCoeffs_L0, sca / skip, edgest, Compression, DetailBoost, Iterates, rew);

    max0 /= gamm;
    //Restore past range, also desaturate a bit per Mantiuk's Color correction for tone mapping.
#ifdef _OPENMP
    #pragma omp parallel for            // removed schedule(dynamic,10)
#endif

    for (int ii = 0; ii < W_L * H_L; ii++) {
        WavCoeffs_L0[ii] *= max0;
    }
}

void ImProcFunctions::WaveletcontAllLfinal(wavelet_decomposition& WaveletCoeffs_L, const cont_params &cp, float *mean, float *sigma, float *MaxP, const WavOpacityCurveWL & waOpacityCurveWL)
{
    int maxlvl = WaveletCoeffs_L.maxlevel();
    float* WavCoeffs_L0 = WaveletCoeffs_L.get_coeff0();

    for (int dir = 1; dir < 4; dir++) {
        for (int lvl = 0; lvl < maxlvl; lvl++) {
            int Wlvl_L = WaveletCoeffs_L.level_W(lvl);
            int Hlvl_L = WaveletCoeffs_L.level_H(lvl);
            float* const* WavCoeffs_L = WaveletCoeffs_L.level_coeffs(lvl);
            finalContAllL(WavCoeffs_L, WavCoeffs_L0, lvl, dir, cp, Wlvl_L, Hlvl_L, mean, sigma, MaxP, waOpacityCurveWL);
        }
    }
}


void ImProcFunctions::WaveletcontAllL(LabImage * labco, float ** varhue, float **varchrom, wavelet_decomposition& WaveletCoeffs_L, const Wavblcurve & wavblcurve,
     struct cont_params &cp, int skip, float *mean, float *sigma, float *MaxP, float *MaxN, const WavCurve & wavCLVCcurve, const WavOpacityCurveW & waOpacityCurveW, const WavOpacityCurveSH & waOpacityCurveSH, FlatCurve* ChCurve, bool Chutili)
{
//   BENCHFUN
    const int maxlvl = WaveletCoeffs_L.maxlevel();
    const int W_L = WaveletCoeffs_L.level_W(0);
    const int H_L = WaveletCoeffs_L.level_H(0);
    float* WavCoeffs_L0 = WaveletCoeffs_L.get_coeff0();

    const float contrast = cp.contrast;
    double avedbl = 0.0; // use double precision for large summations
    float max0 = 0.f;

    if (contrast != 0.f || (cp.tonemap && cp.resena)) { // contrast = 0.f means that all will be multiplied by 1.f, so we can skip this step
#ifdef _OPENMP
        #pragma omp parallel for reduction(+:avedbl) reduction(max:max0) num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

        for (int i = 0; i < W_L * H_L; i++) {
            avedbl += static_cast<double>(WavCoeffs_L0[i]);
            max0 = std::max(WavCoeffs_L0[i], max0);
        }
    }

//tone mapping
    if (cp.tonemap && cp.contmet == 2 && cp.resena) {
        //iterate = 5
        EPDToneMapResid(WavCoeffs_L0, 0, skip, cp, W_L, H_L, max0);
    }

//end tonemapping


    max0 /= 327.68f;
    const float ave = avedbl / (W_L * H_L);
    const float avg = LIM01(ave / 32768.f);

    const double contreal = 0.6 * contrast;
    DiagonalCurve resid_contrast({
        DCT_NURBS,
        0, 0,
        avg - avg * (0.6 - contreal / 250.0), avg - avg * (0.6 + contreal / 250.0),
        avg + (1. - avg) * (0.6 - contreal / 250.0), avg + (1. - avg) * (0.6 + contreal / 250.0),
        1, 1
    });

    if (contrast != 0.f && cp.resena && max0 > 0.f) { // contrast = 0.f means that all will be multiplied by 1.f, so we can skip this step

#ifdef _OPENMP
        #pragma omp parallel for num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

        for (int i = 0; i < W_L * H_L; i++) {
            float buf = LIM01(WavCoeffs_L0[i] / 32768.f);
            buf = resid_contrast.getVal(buf);
            buf *= 32768.f;
            WavCoeffs_L0[i] = buf;
        }
    }

    if (cp.tonemap && cp.contmet == 1 && cp.resena) {
        const float maxp = max0 * 256.f;
        ContrastResid(WavCoeffs_L0, cp, W_L, H_L, maxp);
    }

 //   if ((cp.conres >= 0.f || cp.conresH >= 0.f) && cp.resena && !cp.oldsh) { // cp.conres = 0.f and cp.comresH = 0.f means that all will be multiplied by 1.f, so we can skip this step
    if ((cp.conres >= 0.f || cp.conresH >= 0.f) && cp.resena) { // cp.conres = 0.f and cp.comresH = 0.f means that all will be multiplied by 1.f, so we can skip this step
        const std::unique_ptr<LabImage> temp(new LabImage(W_L, H_L));
#ifdef _OPENMP
        #pragma omp parallel for num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

        for (int i = 0; i < H_L; i++) {
            for (int j = 0; j < W_L; j++) {
                temp->L[i][j] = WavCoeffs_L0[i * W_L + j];
            }
        }

        ImProcFunctions::shadowsHighlights(temp.get(), true, 1, cp.conresH, cp.conres, cp.radius, skip, cp.thH, cp.th);

#ifdef _OPENMP
        #pragma omp parallel for num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

        for (int i = 0; i < H_L; i++) {
            for (int j = 0; j < W_L; j++) {
                WavCoeffs_L0[i * W_L + j] = temp->L[i][j];
            }
        }
    }

  //  if ((cp.conres != 0.f || cp.conresH != 0.f) && cp.resena && cp.oldsh) { // cp.conres = 0.f and cp.comresH = 0.f means that all will be multiplied by 1.f, so we can skip this step
    if ((cp.conres < 0.f || cp.conresH < 0.f) && cp.resena) { // cp.conres = 0.f and cp.comresH = 0.f means that all will be multiplied by 1.f, so we can skip this step
#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = 0; i < W_L * H_L; i++) {
            float LL = WavCoeffs_L0[i];
            float LL100 = LL / 327.68f;
            float tran = 5.f;//transition
            //shadow
            cp.th = std::max(cp.th, 0.01f);

            if (cp.th > (100.f - tran)) {
                tran = 100.f - cp.th;
                tran = std::max(tran, 0.01f);
            }

            if (LL100 < cp.th) {
                constexpr float alp = 3.f; //increase contrast sahdow in lowlights  between 1 and ??
                float aalp = (1.f - alp) / cp.th; //no changes for LL100 = cp.th
                float kk = aalp * LL100 + alp;
                WavCoeffs_L0[i] *= (1.f + kk * cp.conres / 200.f);
            } else if (LL100 < cp.th + tran) {
                float ath = -cp.conres / tran;
                float bth = cp.conres - ath * cp.th;
                WavCoeffs_L0[i] *= (1.f + (LL100 * ath + bth) / 200.f);
            }

            //highlight
            tran = 5.f;

            if (cp.thH < (tran)) {
                tran = cp.thH;
                tran = std::max(tran, 0.01f);
            }

            if (LL100 > cp.thH) {
                WavCoeffs_L0[i] *= (1.f + cp.conresH / 200.f);
            } else if (LL100 > (cp.thH - tran)) {
                float athH = cp.conresH / tran;
                float bthH = cp.conresH - athH * cp.thH;
                WavCoeffs_L0[i] *= (1.f + (LL100 * athH + bthH) / 200.f);
            }
        }
    }

//Blur luma
    if (cp.blurres != 0.f  && cp.resena) {
        int minWL = min(W_L, H_L);

        //printf("skip=%i WL=%i HL=%i min=%i\n", skip, W_L, H_L, minWL);
        if (minWL > 140) { //disabled if too low windows
            constexpr float k = 0.5f;
            float rad = k * cp.blurres / skip;
            float * bef = new float[W_L * H_L];
            float * aft = new float[W_L * H_L];

            for (int i = 0; i < H_L * W_L; i++) {
                bef[i] = WavCoeffs_L0[i];
            }

            boxblur(bef, aft, rad, W_L, H_L, false);

            for (int i = 0; i < H_L * W_L; i++) {
                WavCoeffs_L0[i] = aft[i];
            }

            delete[] bef;
            delete[] aft;
        }
    }

    float *koeLi[12];

    const std::unique_ptr<float[]> koeLibuffer(new float[12 * H_L * W_L]());

    for (int i = 0; i < 12; i++) {
        koeLi[i] = &koeLibuffer[i * W_L * H_L];
    }

    float maxkoeLi[12] = {0.f};
#ifdef _OPENMP
    #pragma omp parallel num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif
    {
        //enabled Lipschitz..replace simple by complex edge detection
        // I found this concept on the web (doctoral thesis on medical Imaging)
        // I was inspired by the principle of Canny and Lipschitz (continuity and derivability)
        // I adapted the principle but have profoundly changed the algorithm
        // One can 1) change all parameters and found good parameters;
        //one can also change in calckoe
        constexpr float edd = 3.f;
        constexpr float eddlow = 15.f;
        float eddlipinfl = 0.005f * cp.edgsens + 0.4f;
        float eddlipampl = 1.f + cp.edgampl / 50.f;

        if (cp.detectedge && cp.val > 0) { //enabled Lipschitz control...more memory..more time...
            const std::unique_ptr<float[]> tmCBuffer(new float[H_L * W_L]);
            float *tmC[H_L];

            for (int i = 0; i < H_L; i++) {
                tmC[i] = &tmCBuffer[i * W_L];
            }
            float gradw = cp.eddet;
            float tloww = cp.eddetthr;

#ifdef _OPENMP
            #pragma omp for schedule(dynamic) collapse(2)
#endif

            for (int lvl = 0; lvl < 4; lvl++) {
                for (int dir = 1; dir < 4; dir++) {
                    const float* const* WavCoeffs_LL = WaveletCoeffs_L.level_coeffs(lvl);
                    float tempkoeli = 0.f;
                    calckoe (WavCoeffs_LL[dir], gradw, tloww, koeLi[lvl * 3 + dir - 1], lvl, W_L, H_L, edd, tempkoeli, tmC);
                    maxkoeLi[lvl * 3 + dir - 1] = tempkoeli ;
                    // return convolution KoeLi and maxkoeLi of level 0 1 2 3 and Dir Horiz, Vert, Diag
                }
            }

            float aamp = 1.f + cp.eddetthrHi / 100.f;

            for (int lvl = 0; lvl < 4; lvl++) {
#ifdef _OPENMP
                #pragma omp for schedule(dynamic,16)
#endif

                for (int i = 1; i < H_L - 1; i++) {
                    for (int j = 1; j < W_L - 1; j++) {
                        //treatment of koeLi and maxkoeLi
                        float interm = 0.f;

                        if (cp.lip3 && cp.lipp) {
                            // comparison between pixel and neighbours
                            const auto neigh = cp.neigh == 1;
                            const auto kneigh = neigh ? 28.f : 38.f;
                            const auto somm = neigh ? 40.f : 50.f;

                            for (int dir = 1; dir < 4; dir++) { //neighbours proxi
                                koeLi[lvl * 3 + dir - 1][i * W_L + j] = (kneigh * koeLi[lvl * 3 + dir - 1][i * W_L + j] + 2.f * koeLi[lvl * 3 + dir - 1][(i - 1) * W_L + j] + 2.f * koeLi[lvl * 3 + dir - 1][(i + 1) * W_L + j]
                                                                        + 2.f * koeLi[lvl * 3 + dir - 1][i * W_L + j + 1] + 2.f * koeLi[lvl * 3 + dir - 1][i * W_L + j - 1] + koeLi[lvl * 3 + dir - 1][(i - 1) * W_L + j - 1]
                                                                        + koeLi[lvl * 3 + dir - 1][(i - 1) * W_L + j + 1] + koeLi[lvl * 3 + dir - 1][(i + 1) * W_L + j - 1] + koeLi[lvl * 3 + dir - 1][(i + 1) * W_L + j + 1]) / somm;
                            }
                        }

                        for (int dir = 1; dir < 4; dir++) {
                            //here I evaluate combinaison of vert / diag / horiz...we are with multiplicators of the signal
                            interm += SQR(koeLi[lvl * 3 + dir - 1][i * W_L + j]);
                        }

                        interm = sqrt(interm);

//                  interm /= 1.732f;//interm = pseudo variance koeLi
                        interm *= 0.57736721f;
                        float kampli = 1.f;
                        float eps = 0.0001f;
                        // I think this double ratio (alph, beta) is better than arctg

                        float alph = koeLi[lvl * 3][i * W_L + j] / (koeLi[lvl * 3 + 1][i * W_L + j] + eps); //ratio between horizontal and vertical
                        float beta = koeLi[lvl * 3 + 2][i * W_L + j] / (koeLi[lvl * 3 + 1][i * W_L + j] + eps); //ratio between diagonal and horizontal

                        float alipinfl = (eddlipampl - 1.f) / (1.f - eddlipinfl);
                        float blipinfl = eddlipampl - alipinfl;

                        //alph evaluate the direction of the gradient regularity Lipschitz
                        // if = 1 we are on an edge
                        // if 0 we are not
                        // we can change and use log..or Arctg but why ?? we can change if need ...
                        //Liamp=1 for eddlipinfl
                        //liamp > 1 for alp >eddlipinfl and alph < 1
                        //Liamp < 1 for alp < eddlipinfl and alph > 0
                        if (alph > 1.f) {
                            alph = 1.f / alph;
                        }

                        if (beta > 1.f) {
                            beta = 1.f / beta;
                        }

                        //take into account diagonal
                        //if in same value OK
                        //if not no edge or reduction
                        float bet = 1.f;

                        if (alph > eddlipinfl && beta < 0.85f * eddlipinfl) { //0.85 arbitrary value ==> eliminate from edge if H V D too different
                            bet = beta;
                        }

                        float AmpLip = 1.f;

                        if (alph > eddlipinfl) {
                            AmpLip = alipinfl * alph + blipinfl;    //If beta low reduce kampli
                            kampli = SQR(bet) * AmpLip * aamp;
                        } else {
                            AmpLip = (1.f / eddlipinfl) * SQR(SQR(alph * bet));    //Strong Reduce if beta low
                            kampli = AmpLip / aamp;
                        }

                        interm *= kampli;

                        if (interm < cp.eddetthr / eddlow) {
                            interm = 0.01f;    //eliminate too low values
                        }

                        //we can change this part of algo==> not equal but ponderate
                        koeLi[lvl * 3][i * W_L + j] = koeLi[lvl * 3 + 1][i * W_L + j] = koeLi[lvl * 3 + 2][i * W_L + j] = interm; //new value
                        //here KoeLi contains values where gradient is high and coef high, and eliminate low values...
                    }
                }
            }

            // end
        }

        bool wavcurvecomp = false;//not enable if 0.75

        if (wavblcurve) {
            for (int i = 0; i < 500; i++) {
                if (wavblcurve[i] != 0.) {
                    wavcurvecomp = true;
                    break;
                }
            }
        }

        std::unique_ptr<float[]> aft;
#ifdef _OPENMP
        #pragma omp for schedule(dynamic) collapse(2)
#endif

        for (int dir = 1; dir < 4; dir++) {
            for (int lvl = 0; lvl < maxlvl; lvl++) {

                int Wlvl_L = WaveletCoeffs_L.level_W(lvl);
                int Hlvl_L = WaveletCoeffs_L.level_H(lvl);

                float* const* WavCoeffs_L = WaveletCoeffs_L.level_coeffs(lvl);

                ContAllL(koeLi, maxkoeLi[lvl * 3 + dir - 1], true, maxlvl, labco,  varhue, varchrom, WavCoeffs_L, WavCoeffs_L0, lvl, dir, cp, Wlvl_L, Hlvl_L, skip, mean, sigma, MaxP, MaxN, wavCLVCcurve, waOpacityCurveW, waOpacityCurveSH, ChCurve, Chutili);

                if (std::min(Wlvl_L, Hlvl_L) > 180) {
                    if (wavblcurve && wavcurvecomp && cp.blena) {
                        // printf("Blur level L\n");
                        float mea[10];
                        const float effect = cp.bluwav;
                        constexpr float offs = 1.f;
                        calceffect(lvl, mean, sigma, mea, effect, offs);
                        float lutFactor;
                        const float inVals[] = {0.05f, 0.2f, 0.7f, 1.f, 1.f, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f, 0.01f};

                        const auto meaLut = buildMeaLut(inVals, mea, lutFactor);
                        if (!aft.get()) {
                            aft.reset(new float[Wlvl_L * Hlvl_L]);
                        }

                        //blur level
                        const float klev = wavblcurve[lvl * 55.5f] * 80.f / skip;
                        auto WavL = WavCoeffs_L[dir];
                        boxblur(WavL, aft.get(), klev, Wlvl_L, Hlvl_L, false);

                        int co = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
                        const vfloat lutFactorv = F2V(lutFactor);
                        for (; co < Hlvl_L * Wlvl_L - 3; co += 4) {
                            const vfloat valv = LVFU(WavL[co]);
                            STVFU(WavL[co], intp((*meaLut)[vabsf(valv) * lutFactorv], LVFU(aft[co]), valv));
                        }
#endif
                        for (; co < Hlvl_L * Wlvl_L; co++) {
                            WavL[co] = intp((*meaLut)[std::fabs(WavL[co]) * lutFactor], aft[co], WavL[co]);
                        }
                    }
                }
            }
        }
    }
}

void ImProcFunctions::WaveletAandBAllAB(wavelet_decomposition& WaveletCoeffs_a, wavelet_decomposition& WaveletCoeffs_b,
                                        const cont_params &cp, FlatCurve* hhCurve, bool hhutili)
{
    //   StopWatch Stop1("WaveletAandBAllAB");
    if (hhutili  && cp.resena) {  // H=f(H)
        int W_L = WaveletCoeffs_a.level_W(0);
        int H_L = WaveletCoeffs_a.level_H(0);

        float* WavCoeffs_a0 = WaveletCoeffs_a.get_coeff0();
        float* WavCoeffs_b0 = WaveletCoeffs_b.get_coeff0();
#ifdef _OPENMP
        #pragma omp parallel num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif
        {
#if defined(__SSE2__) || defined(RT_SIMDE)
            float huebuffer[W_L] ALIGNED64;
            float chrbuffer[W_L] ALIGNED64;
#endif // __SSE2__
#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16)
#endif

            for (int i = 0; i < H_L; i++) {
#if defined(__SSE2__) || defined(RT_SIMDE)
                // precalculate hue and chr
                int k;

                for (k = 0; k < W_L - 3; k += 4) {
                    const vfloat av = LVFU(WavCoeffs_a0[i * W_L + k]);
                    const vfloat bv = LVFU(WavCoeffs_b0[i * W_L + k]);
                    STVF(huebuffer[k], xatan2f(bv, av));
                    STVF(chrbuffer[k], vsqrtf(SQRV(av) + SQRV(bv)));
                }

                for (; k < W_L; k++) {
                    huebuffer[k] = xatan2f(WavCoeffs_b0[i * W_L + k], WavCoeffs_a0[i * W_L + k]);
                    chrbuffer[k] = sqrtf(SQR(WavCoeffs_b0[i * W_L + k]) + SQR(WavCoeffs_a0[i * W_L + k])) / 327.68f;
                }

#endif // __SSE2__

                for (int j = 0; j < W_L; j++) {

#if defined(__SSE2__) || defined(RT_SIMDE)
                    float hueR = huebuffer[j];
                    float chR = chrbuffer[j];
#else
                    float hueR = xatan2f(WavCoeffs_b0[i * W_L + j], WavCoeffs_a0[i * W_L + j]);
                    float chR = sqrtf(SQR(WavCoeffs_b0[i * W_L + j]) + SQR(WavCoeffs_a0[i * W_L + j]));
#endif
                    /*      if (editID == EUID_WW_HHCurve) {//H pipette
                                            float valpar =Color::huelab_to_huehsv2(hueR);
                                            editWhatever->v(i,j) = valpar;
                                    }
                    */
                    float valparam = (static_cast<float>(hhCurve->getVal(Color::huelab_to_huehsv2(hueR))) - 0.5f) * 1.7f + hueR; //get H=f(H)  1.7 optimisation !
                    float2 sincosval = xsincosf(valparam);
                    WavCoeffs_a0[i * W_L + j] = chR * sincosval.y;
                    WavCoeffs_b0[i * W_L + j] = chR * sincosval.x;
                }
            }
        }
    }

}

void ImProcFunctions::WaveletcontAllAB(LabImage * labco, float ** varhue, float **varchrom, wavelet_decomposition& WaveletCoeffs_ab, const Wavblcurve & wavblcurve, const WavOpacityCurveW & waOpacityCurveW,
          struct cont_params &cp, const bool useChannelA, int skip, float *meanab, float *sigmaab)
{
//BENCHFUN
    int maxlvl = WaveletCoeffs_ab.maxlevel();
    int W_L = WaveletCoeffs_ab.level_W(0);
    int H_L = WaveletCoeffs_ab.level_H(0);

    float* WavCoeffs_ab0 = WaveletCoeffs_ab.get_coeff0();

#ifdef _OPENMP
    #pragma omp parallel num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif
    {
        if (cp.chrores != 0.f  && cp.resena) { // cp.chrores == 0.f means all will be multiplied by 1.f, so we can skip the processing of residual

#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int i = 0; i < W_L * H_L; i++) {
                const float skyprot = cp.sky;
                //chroma
                int ii = i / W_L;
                int jj = i - ii * W_L;
                float modhue = varhue[ii][jj];
                float scale = 1.f;

                if (skyprot > 0.f) {
                    if ((modhue < cp.t_ry && modhue > cp.t_ly)) {
                        scale = (100.f - cp.sky) / 100.1f;
                    } else if ((modhue >= cp.t_ry && modhue < cp.b_ry)) {
                        scale = (100.f - cp.sky) / 100.1f;
                        float ar = (scale - 1.f) / (cp.t_ry - cp.b_ry);
                        float br = scale - cp.t_ry * ar;
                        scale = ar * modhue + br;
                    } else if ((modhue > cp.b_ly && modhue < cp.t_ly)) {
                        scale = (100.f - cp.sky) / 100.1f;
                        float al = (scale - 1.f) / (-cp.b_ly + cp.t_ly);
                        float bl = scale - cp.t_ly * al;
                        scale = al * modhue + bl;
                    }
                } else if (skyprot < 0.f) {
                    if ((modhue > cp.t_ry || modhue < cp.t_ly)) {
                        scale = (100.f + cp.sky) / 100.1f;
                    }
                }

                WavCoeffs_ab0[i] *= (1.f + cp.chrores * (scale) / 100.f);

            }
        }

        if (cp.cbena  && cp.resena) { //if user select Toning and color balance

#ifdef _OPENMP
            #pragma omp for nowait
#endif

            for (int i = 0; i < W_L * H_L; i++) {
                int ii = i / W_L;
                int jj = i - ii * W_L;
                float LL = (labco->L[ii * 2][jj * 2]) / 327.68f; //I use labco but I can use also WavCoeffs_L0 (more exact but more memory)

                float sca = 1.f; //amplifier - reducter...about 1, but perhaps 0.6 or 1.3

                if (useChannelA) { //green red (little magenta)
                    //transition to avoid artifacts with 6 between 30 to 36 and  63 to 69
                    float aa = (cp.grmed - cp.grlow) / 6.f;
                    float bb = cp.grlow - 30.f * aa;
                    float aaa = (cp.grhigh - cp.grmed) / 6.f;
                    float bbb = cp.grmed - 63.f * aaa;

                    if (LL < 30.f) { //shadows
                        WavCoeffs_ab0[i] += cp.grlow * (sca) * 300.f;
                    } else if (LL >= 30.f && LL < 36.f) { //transition
                        float tr = aa * LL + bb;
                        WavCoeffs_ab0[i] += tr * (sca) * 300.f;
                    } else if (LL >= 36.f && LL < 63.f) { //midtones
                        WavCoeffs_ab0[i] += cp.grmed * (sca) * 300.f;
                    } else if (LL >= 63.f && LL < 69.f) { //transition
                        float trh = aaa * LL + bbb;
                        WavCoeffs_ab0[i] += trh * (sca) * 300.f;
                    } else if (LL >= 69.f) { //highlights
                        WavCoeffs_ab0[i] += cp.grhigh * (sca) * 300.f;
                    }
                } else { //blue yellow
                    //transition with 6 between 30 to 36 and 63 to 69
                    float aa1 = (cp.blmed - cp.bllow) / 6.f;
                    float bb1 = cp.bllow - 30.f * aa1;
                    float aaa1 = (cp.blhigh - cp.blmed) / 6.f;
                    float bbb1 = cp.blmed - 63.f * aaa1;

                    if (LL < 30.f) {
                        WavCoeffs_ab0[i] += cp.bllow * (sca) * 300.f;
                    } else if (LL >= 30.f && LL < 36.f) {
                        float tr1 = aa1 * LL + bb1;
                        WavCoeffs_ab0[i] += tr1 * (sca) * 300.f;
                    } else if (LL >= 36.f && LL < 63.f) {
                        WavCoeffs_ab0[i] += cp.blmed * (sca) * 300.f;
                    } else if (LL >= 63.f && LL < 69.f) {
                        float trh1 = aaa1 * LL + bbb1;
                        WavCoeffs_ab0[i] += trh1 * (sca) * 300.f;
                    } else if (LL >= 69.f) {
                        WavCoeffs_ab0[i] += cp.blhigh * (sca) * 300.f;
                    }
                }
            }
        }

//Blur chroma
        if (cp.blurcres != 0.f  && cp.resena) {
            int minWL = min(W_L, H_L);

            //printf("skip=%i WL=%i HL=%i min=%i\n", skip, W_L, H_L, minWL);
            if (minWL > 140) { //disabled if too low windows
                constexpr float k = 0.5f;
                float rad = k * cp.blurcres / skip;
                float * bef = new float[W_L * H_L];
                float * aft = new float[W_L * H_L];

                for (int i = 0; i < H_L * W_L; i++) {
                    bef[i] = WavCoeffs_ab0[i];
                }

                boxblur(bef, aft, rad, W_L, H_L, false);

                for (int i = 0; i < H_L * W_L; i++) {
                    WavCoeffs_ab0[i] = aft[i];
                }

                delete[] bef;
                delete[] aft;
            }
        }


        bool wavcurvecomp = false;//not enable if 0.75

        if (wavblcurve) {
            for (int i = 0; i < 500; i++) {
                if (wavblcurve[i] != 0.) {
                    wavcurvecomp = true;
                    break;
                }
            }
        }

        std::unique_ptr<float[]> aft;
#ifdef _OPENMP
        #pragma omp for schedule(dynamic) collapse(2)
#endif

        for (int dir = 1; dir < 4; dir++) {
            for (int lvl = 0; lvl < maxlvl; lvl++) {

                int Wlvl_ab = WaveletCoeffs_ab.level_W(lvl);
                int Hlvl_ab = WaveletCoeffs_ab.level_H(lvl);

                float* const* WavCoeffs_ab = WaveletCoeffs_ab.level_coeffs(lvl);
                ContAllAB(labco, maxlvl, varhue, varchrom, WavCoeffs_ab, WavCoeffs_ab0, lvl, dir, waOpacityCurveW, cp, Wlvl_ab, Hlvl_ab, useChannelA, meanab, sigmaab);
                
                if(std::min(Wlvl_ab, Hlvl_ab) > 180) {
                    if (wavblcurve && wavcurvecomp && cp.blena && cp.chrwav > 0.f) {
                        float mea[10];
                        const float effect = cp.bluwav;
                        constexpr float offs = 1.f;
                        calceffect(lvl, meanab, sigmaab, mea, effect, offs);
                        float lutFactor;
                        const float inVals[] = {0.05f, 0.2f, 0.7f, 1.f, 1.f, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f, 0.00f};
                        const auto meaLut = buildMeaLut(inVals, mea, lutFactor);

                        if (!aft.get()) {
                            aft.reset(new float[Wlvl_ab * Hlvl_ab]);
                        }

                        //blur level
                        const float klev = wavblcurve[lvl * 55.5f] * 80.f / skip;
                        boxblur(WavCoeffs_ab[dir], aft.get(), klev, Wlvl_ab, Hlvl_ab, false);

                        auto WavAb = WavCoeffs_ab[dir];
                        int co = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
                        const vfloat lutFactorv = F2V(lutFactor);
                        for (; co < Hlvl_ab * Wlvl_ab - 3; co += 4) {
                            const vfloat valv = LVFU(WavAb[co]);
                            STVFU(WavAb[co], intp((*meaLut)[vabsf(valv) * lutFactorv], LVFU(aft[co]), valv));
                        }
#endif
                        for (; co < Hlvl_ab * Wlvl_ab; co++) {
                            WavAb[co] = intp((*meaLut)[std::fabs(WavAb[co]) * lutFactor], aft[co], WavAb[co]);
                        }
                    }
                }
            }
        }
    }
}

void ImProcFunctions::calckoe (const float* WavCoeffs, float gradw, float tloww, float *koeLi, int level, int W_L, int H_L, float edd, float &maxkoeLi, float **tmC, bool multiThread)
{
    const int borderL = tloww < 75.f ? 1 : 2;

    if (tloww < 75.f) {
        // I calculate coefficients with r size matrix 3x3 r=1 ; 5x5 r=2; 7x7 r=3
        /*
        float k[2*r][2*r];
        for (int i=1;i<=(2*r+1);i++) {
                    for (int j=1;j<=(2*r+1);j++) {
                        k[i][j]=(1.f/6.283*sigma*sigma)*exp(-SQR(i-r-1)+SQR(j-r-1)/2.f*SQR(sigma));
                    }
        }
        //I could also use Gauss.h for 3x3
        // If necessary I can put a 7x7 matrix
        */
        float c0, c1, c2, mult;
        if (tloww < 30.f) { //sigma=0.55
            c0 = 8.94f;
            c1 = 1.71f;
            c2 = 0.33f;
            mult = 0.0584795f;
        } else if (tloww < 50.f) { //sigma=0.85
            c0 = 4.0091f;
            c1 = 2.0068f;
            c2 = 1.0045f;
            mult = 0.062288f;
        } else { //sigma=1.1
            c0 = 3.025f;
            c1 = 2.001f;
            c2 = 1.323f;
            mult = 0.06127f;
        }
        c0 *= mult;
        c1 *= mult;
        c2 *= mult;
#ifdef _OPENMP
        #pragma omp parallel for if(multiThread)
#endif
        for (int i = 1; i < H_L - 1; i++) {
            for (int j = 1; j < W_L - 1; j++) {
                tmC[i][j] = c0 * WavCoeffs[i * W_L + j] +
                            c1 * ((WavCoeffs[(i - 1) * W_L + j] + WavCoeffs[(i + 1) * W_L + j]) + (WavCoeffs[i * W_L + j + 1] + WavCoeffs[i * W_L + j - 1])) +
                            c2 * ((WavCoeffs[(i - 1) * W_L + j - 1] + WavCoeffs[(i - 1) * W_L + j + 1]) + (WavCoeffs[(i + 1) * W_L + j - 1] + WavCoeffs[(i + 1) * W_L + j + 1]));
            }
        }
    } else {
        if (level > 1) { // do not activate 5x5 if level 0 or 1
            // Gaussian 1.1
            // 0.5 2 3 2 0.5
            // 2 7 10 7 2
            // 3 10 15 10 3
            // 2 7 10 7 2
            // 0.5 2 3 2 0.5
            // divi 113
            //Gaussian 1.4
            // 2 4 5 4 2
            // 4 9 12 9 4
            // 5 12 15 12 5
            // 4 9 12 9 4
            // 2 4 5 4 2
            // divi 159
            float c0, c1, c2, c3, c4, c5, mult;
            if (tloww < 85.f) { //sigma=1.1
                c0 = 15.f;
                c1 = 10.f;
                c2 = 7.f;
                c3 = 3.f;
                c4 = 2.f;
                c5 = 0.5f;
                mult = 0.0088495f;
            } else { //sigma=1.4
                c0 = 15.f;
                c1 = 12.f;
                c2 = 9.f;
                c3 = 5.f;
                c4 = 4.f;
                c5 = 2.f;
                mult = 0.0062893f;
            }
            c0 *= mult;
            c1 *= mult;
            c2 *= mult;
            c3 *= mult;
            c4 *= mult;
            c5 *= mult;
#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif
            for (int i = 2; i < H_L - 2; i++) {
                for (int j = 2; j < W_L - 2; j++) {
                    tmC[i][j] = c0 * WavCoeffs[i * W_L + j] +
                                c1 * ((WavCoeffs[(i - 1) * W_L + j] + WavCoeffs[(i + 1) * W_L + j]) + (WavCoeffs[i * W_L + j + 1] + WavCoeffs[i * W_L + j - 1])) +
                                c2 * ((WavCoeffs[(i - 1) * W_L + j - 1] + WavCoeffs[(i - 1) * W_L + j + 1]) + (WavCoeffs[(i + 1) * W_L + j - 1] + WavCoeffs[(i + 1) * W_L + j + 1])) +
                                c3 * ((WavCoeffs[(i - 2) * W_L + j] + WavCoeffs[(i + 2) * W_L + j]) + (WavCoeffs[i * W_L + j - 2] + WavCoeffs[i * W_L + j + 2])) +
                                c4 * ((WavCoeffs[(i - 2) * W_L + j - 1] + WavCoeffs[(i - 2) * W_L + j + 1]) + (WavCoeffs[(i + 2) * W_L + j + 1] + WavCoeffs[(i + 2) * W_L + j - 1]) +
                                      (WavCoeffs[(i - 1) * W_L + j - 2] + WavCoeffs[(i - 1) * W_L + j + 2]) + (WavCoeffs[(i + 1) * W_L + j + 2] + WavCoeffs[(i + 1) * W_L + j - 2])) +
                                c5 * ((WavCoeffs[(i - 2) * W_L + j - 2] + WavCoeffs[(i - 2) * W_L + j + 2]) + (WavCoeffs[(i + 2) * W_L + j - 2] + WavCoeffs[(i + 2) * W_L + j + 2]));
                }
            }
        } else {
#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif
            for (int i = 0; i < H_L; i++) {
                for (int j = 0; j < W_L; j++) {
                    koeLi[i * W_L + j] = 0.f;
                }
            }
            return;
        }
    }

    // fill borders with 1.f
    int ii = 0;
    for (; ii < borderL; ii++) {
        for (int j = 0; j < W_L; j++) {
            koeLi[ii * W_L + j] = 1.f;
        }
    }
    for (; ii < H_L - borderL; ii++) {
        for (int j = 0; j < borderL; j++) {
            koeLi[ii * W_L + j] = 1.f;
        }
        for (int j = W_L - borderL; j < W_L; j++) {
            koeLi[ii * W_L + j] = 1.f;
        }
    }
    for (; ii < H_L; ii++) {
        for (int j = 0; j < W_L; j++) {
            koeLi[ii * W_L + j] = 1.f;
        }
    }

    constexpr float thr = 40.f; //avoid artifact eg. noise...to test
    const float thr2 = 1.5f * edd + gradw / 30.f; //edd can be modified in option ed_detect
    const float diffFactor = gradw / 100.f;

    for (int i = borderL; i < H_L - borderL; i++) {
        for (int j = borderL; j < W_L - borderL; j++) {
            // my own algo : probably a little false, but simpler as Lipschitz !
            // Thr2 = maximum of the function ==> Lipsitch says = probably edge
            float temp = rtengine::max(std::fabs(WavCoeffs[i * W_L + j]), thr);
            koeLi[i * W_L + j] = rtengine::min(thr2, std::fabs(tmC[i][j] / temp)); // limit maxi

            //it will be more complicated to calculate both Wh and Wv, but we have also Wd==> pseudo Lipschitz
            if (koeLi[i * W_L + j] > maxkoeLi) {
                maxkoeLi = koeLi[i * W_L + j];
            }
            float diff = maxkoeLi - koeLi[i * W_L + j];
            diff *= diffFactor;
            koeLi[i * W_L + j] = maxkoeLi - diff;
        }
    }
}


// Copyright 6-2024 modified 3-2026 - Jacques Desmis <jdesmis@gmail.com>
void ImProcFunctions::complete_local_contrast (LabImage * lab, LabImage * dst, const procparams::WaveletParams & waparams, const procparams::ColorManagementParams & cmparams, const WavOpacityCurveWL & cmOpacityCurveWL, int skip, int &level_hr, int &maxlevpo, bool &wavcurvecont)
{
    wavcurvecont = false;
    if (cmOpacityCurveWL) {//activate only if one value not equal to 0.5
        for (int i = 0; i < 500; i++) {
            if (cmOpacityCurveWL[i] != 0.5f) {
               wavcurvecont = true;
               break;
            }
        }
    }


    if(wavcurvecont && cmparams.wavExp) {//enable curve and expander
        //This modification significantly reduces potential artifacts caused by increased local contrast due to wavelets. It has virtually no visible effect on the resulting image, other than the absorption of the artifacts.
        const std::unique_ptr<LabImage> lab2(new LabImage(lab->W, lab->H));//New variable Lab, which will be used for wavelet decomposition.

        constexpr float artifact_minimum = 3000.f;//The threshold below which wavelet decomposition is not performed, and therefore no contrast enhancement is achieved, to avoid artifacts due to proximity to the gamut limit.
        constexpr float artifact_minimum_low = 10.f;//Very low replacement value, but without impact on the decomposition, except for some proximity of wavelets.
        constexpr float artifact_minimum_lowab = 0.f;//With 0, we are necessarily within the gamut

        constexpr float artifact_maximum = 27768.f;//The threshold above which wavelet decomposition is not performed, and therefore no contrast enhancement is achieved, to avoid artifacts due to proximity to the gamut limit.
        constexpr float artifact_maximum_high = 32600.f;//Very high replacement value, but without impact on the decomposition, except for some proximity of wavelets.
        constexpr float artifact_maximum_highab = 0.f;//With 0, we are necessarily within the gamut

#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif
        for (int i = 0; i < lab->H; i++){
            for (int j = 0; j < lab->W; j++){
                if (lab->L[i][j] > artifact_minimum  && lab->L[i][j] < artifact_maximum) {
                    lab2->L[i][j] = lab->L[i][j];
                    lab2->a[i][j] = lab->a[i][j];
                    lab2->b[i][j] = lab->b[i][j];
                } else if (lab->L[i][j] < artifact_minimum) {
                    lab2->L[i][j] = artifact_minimum_low;
                    lab2->a[i][j] = artifact_minimum_lowab;
                    lab2->b[i][j] = artifact_minimum_lowab;
                } else if (lab->L[i][j] > artifact_maximum) {
                    lab2->L[i][j] = artifact_maximum_high;
                    lab2->a[i][j] = artifact_maximum_highab;
                    lab2->b[i][j] = artifact_maximum_highab;
                }
            }
        }
    
    
    
#ifdef _OPENMP
        const int numThreads = omp_get_max_threads();
#else
        const int numThreads = 1;

#endif
            const int width = lab->W;
            const int height = lab->H;
            const int wavelet_lev = 7;//default
            int DaubLen = 4;//type of wave : number of vanishing moments
            if (waparams.daubcoeffmethod == "2_") {//If the user changes this value in the Wavelet levels menu
                DaubLen = 4;
            } else if (waparams.daubcoeffmethod == "4_") {
                DaubLen = 6;//default
            } else if (waparams.daubcoeffmethod == "6_") {
                DaubLen = 8;
            } else if (waparams.daubcoeffmethod == "10_") {
                DaubLen = 12;
            } else if (params->wavelet.daubcoeffmethod == "14_") {
                DaubLen = 16;
            } else if (params->wavelet.daubcoeffmethod == "20_") {
                DaubLen = 22;
            }
            const float sigmafin = cmparams.sigmatrc;//attenuation response signal
            const int pyrwav = cmparams.pyrwavtrc;//levels contrast profiles
            const float offset = cmparams.offstrc;//offset signal
            int level_bl = 0;//adapted to each levels profile 
            int level_hl = 1;//adapted to each levels profile 
            int level_br = wavelet_lev;
            level_hr = wavelet_lev;//to adapt if necessary

            //6 contrast profiles to change range levels and rolloff for high contrast positive and negative - of course we can add anothers
            //I change only values for LUT for high contrast values and not for low levels, but we can...
            //These parameters are passed to Inval, to focus more or less on the "center" of the signal.
            float inva5 = 0.8f;
            float inva6 = 0.7f;
            float inva7 = 0.5f;
            float inva8 = 0.4f;
            float inva9 = 0.3f;
            float inva10 = 0.1f;

            if (pyrwav == 1) {//low contrast
                level_bl = 0;
                level_hl = 1;
                level_br = wavelet_lev - 3;
                level_hr = wavelet_lev - 2;
            } else if (pyrwav == 2) {
                level_bl = 0;
                level_hl = 0;
                level_br = wavelet_lev - 3;
                level_hr = wavelet_lev - 1;
                if (cmparams.wsmoothciesli == 0.f) {
                    inva5 = 1.f;
                    inva6 = 0.8f;
                    inva7 = 0.65f;
                    inva8 = 0.5f;
                    inva9 = 0.3f;
                    inva10 = 0.2f;
                }
            } else if ( pyrwav == 3) {//default
                level_bl = 0;
                level_hl = 0;
                level_br = wavelet_lev - 2;
                level_hr = wavelet_lev;
                if (cmparams.wsmoothciesli == 0.f) {
                    inva5 = 1.f; 
                    inva6 = 0.9f;
                    inva7 = 0.7f;
                    inva8 = 0.6f;
                    inva9 = 0.4f;
                    inva10 = 0.2f;
                }
            } else if ( pyrwav == 4) {
                level_bl = 0;
                level_hl = 0;
                level_br = wavelet_lev -1;
                level_hr = wavelet_lev +1;
                if (cmparams.wsmoothciesli == 0.f) {
                    inva5 = 0.9f;
                    inva6 = 0.8f;
                    inva7 = 0.6f;
                    inva8 = 0.5f;
                    inva9 = 0.3f;
                    inva10 = 0.1f;
                }
            } else if ( pyrwav == 5) {
                level_bl = 0;
                level_hl = 0;
                level_br = wavelet_lev - 1;
                level_hr = wavelet_lev + 2;//be careful the preview must be big enough to see the changes
                if (cmparams.wsmoothciesli == 0.f) {
                    inva5 = 0.8f;
                    inva6 = 0.6f;
                    inva7 = 0.5f;
                    inva8 = 0.3f;
                    inva9 = 0.2f;
                    inva10 = 0.05f;
                }//last choice not used for various reasons
            } else if ( pyrwav == 6) {//agresive - maximum - in this case LUT are minimal to avoid artifacts -be careful the preview must be big enough to see the changes
                level_bl = 0;
                level_hl = 0;
                level_br = wavelet_lev;
                level_hr = wavelet_lev + 2;//here maximum
            }

            //find possible max levels in function of windows preview size.
            int minwin = rtengine::min(width, height);
            int maxlevelspot = 9;//maximum possible

            // adapt maximum level wavelet to size of crop
            while ((1 << maxlevelspot) >= minwin && maxlevelspot  > 1) {
                --maxlevelspot ;
            }

            int wavelet_level = rtengine::min(level_hr, maxlevelspot);
            int maxlvl = wavelet_level;
            maxlevpo = maxlvl;
            //decomposition wavelet , we can change Daublen (moment wavelet) in Tab - Wavelet Levels with subsampling = 1
            std::unique_ptr<wavelet_decomposition> wdspot = std::unique_ptr<wavelet_decomposition>(new wavelet_decomposition(lab2->L[0], width, height, maxlvl, 1, skip, numThreads, DaubLen));
            if (wdspot->memory_allocation_failed()) {//probably if not enough memory.
                return;
            }

            //residual contrast
            const float contresid = cmparams.residtrc;

            if (contresid != 0) {//residual image
                int W_L = wdspot->level_W(0);
                int H_L = wdspot->level_H(0);
                float *wav_L0 = wdspot->get_coeff0();//residual image


                const float maxh = 1.25f; //amplification contrast above mean, we can change 1.25f
                const float maxl = 1.25f; //reduction contrast under mean
                const float multL = contresid * (maxl - 1.f) / 100.f + 1.f;
                const float multH = contresid * (maxh - 1.f) / 100.f + 1.f;
                double avedbl = 0.0; // use double precision for large summations
                float max0 = 0.f;
                float min0 = FLT_MAX;

#ifdef _OPENMP
#       pragma omp parallel for reduction(+:avedbl) if (multiThread)
#endif
                for (int i = 0; i < W_L * H_L; i++) {
                    avedbl += wav_L0[i];
                }

#ifdef _OPENMP
#       pragma omp parallel if (multiThread)
#endif
        {
                float lminL = FLT_MAX;
                float lmaxL = 0.f;

#ifdef _OPENMP
#           pragma omp for
#endif
                for (int i = 0; i < W_L * H_L; i++) {
                    lminL = min(lminL, wav_L0[i]);
                    lmaxL = max(lmaxL, wav_L0[i]);
                }

#ifdef _OPENMP
#           pragma omp critical
#endif
            {
                    min0 = min(min0, lminL);
                    max0 = max(max0, lmaxL);
            }
        }

                max0 /= 327.68f;
                min0 /= 327.68f;
                float ave = avedbl / double(W_L * H_L);
                //transitions
                float av = ave / 327.68f;
                float ah = (multH - 1.f) / (av - max0);
                float bh = 1.f - max0 * ah;
                float al = (multL - 1.f) / (av - min0);
                float bl = 1.f - min0 * al;

                if (max0 > 0.0) { 
#ifdef _OPENMP
#           pragma omp parallel for if (multiThread)
#endif
                    for (int i = 0; i < W_L * H_L; i++) {
                        if (wav_L0[i] < 32768.f) {
                            float prov;

                            if (wav_L0[i] > ave) {
                                float kh = ah * (wav_L0[i] / 327.68f) + bh;
                                prov = wav_L0[i];
                                wav_L0[i] = ave + kh * (wav_L0[i] - ave);
                            } else {
                                float kl = al * (wav_L0[i] / 327.68f) + bl;
                                prov = wav_L0[i];
                                wav_L0[i] = ave - kl * (ave - wav_L0[i]);
                            }

                            float diflc = wav_L0[i] - prov;
                            wav_L0[i] =  prov + diflc;
                        }
                    }
                }
            }
            //end residual contrast

            //begin variable contrast
            // Declaration with 10 levels to calculate mean , mean negative, sigma, sigma negative, Max et Max negative for each level
            // An old algorithm conceived in 2012...which has been 'copied' elsewhere... all the better.
            float mean[10];
            float meanN[10];
            float sigma[10];
            float sigmaN[10];
            float MaxP[10];
            float MaxN[10];
            Evaluate2(*wdspot, mean, meanN, sigma, sigmaN, MaxP, MaxN, numThreads);//calculate mean sigma Max for each levels
            float alow = 1.f;
            float blow = 0.f;

            if (level_hl != level_bl) {//transitions low levels
                alow = 1.f / (level_hl - level_bl);
                blow = -alow * level_bl;
            }

            float ahigh = 1.f;
            float bhigh = 0.f;

            if (level_hr != level_br) {//transitions high levels
                ahigh = 1.f / (level_hr - level_br);
                bhigh =  -ahigh * level_br;
            }
            //What follows is a model of a complex phenomenon, simplifying the signal shape. The action is a function of the contrast (in the wavelet sense), calculating a mean and a standard deviation (which are not actually mean and standard deviations).
            for (int dir = 1; dir < 4; dir++) {//for each direction
                for (int level = level_bl; level < maxlvl; ++level) {//for each levels
                    int W_L = wdspot->level_W(level);
                    int H_L = wdspot->level_H(level);
                    float* const* wav_L = wdspot->level_coeffs(level);
                    //sigmafin = attenuation response to change signal shape
                    // I use only positives values to simplify calculations... possible improvment.
                    if (MaxP[level] > 0.f && mean[level] != 0.f && sigma[level] != 0.f) {
                        float insigma = 0.666f; //SD standard deviation (modelisation)
                        float logmax = log(MaxP[level]); //log Max
                        float rapX = (offset * mean[level] + sigmafin * sigma[level]) / MaxP[level];//rapport between SD / max
                        //offset move mean location in signal
                        float inx = log(insigma);
                        float iny = log(rapX);
                        float rap = inx / iny;
                        //transitions
                        float asig = 0.166f / (sigma[level] * sigmafin);
                        float bsig = 0.5f - asig * (mean[level] * offset);
                        float amean = 0.5f / (mean[level] * offset);
                        const float effect = sigmafin;
                        float mea[10];//simulation using mean and sigma, to evaluate signal
                        calceffect(level, mean, sigma, mea, effect, offset);
                        float klev = 1.f;
                        if (level >= level_hl && level <= level_hr) {
                           klev = 1.f;
                        }
                        //change klev with real change in levels - see contrast profiles
                        //transition in beginning low levels
                        if (level_hl != level_bl) {
                            if (level >= level_bl && level < level_hl) {
                                klev = alow * level + blow;
                            }
                        }
                        //transition in max levels
                        if (level_hr != level_br) {
                            if (level > level_hr && level <= level_br) {
                               klev = ahigh * level + bhigh;
                            }
                        }
                        const float threshold = offset * mean[level] + sigmafin * sigma[level];//base signal calculation.
                        float lutFactor;//inva5, inva6, inva7, inva8, inva9, inva10 are define in Contrast profiles.
                        float inVals[] = {0.05f, 0.2f, 0.7f, 1.f, 1.f, inva5, inva6, inva7, inva8, inva9, inva10};//values to give for calculate LUT along signal : minimal near 0 or MaxP
                        const auto meaLut = buildMeaLut(inVals, mea, lutFactor);//build LUT
                        

#ifdef _OPENMP
                        #pragma omp parallel for if (multiThread)
#endif

                        for (int y = 0; y < H_L; y++) {
                            for (int x = 0; x < W_L; x++) {//for each pixel
                                if (cmOpacityCurveWL) {//if curve enable
                                    float absciss;//position in curve and signal
                                    float &val = wav_L[dir][y * W_L + x];
                                    const float WavCL = std::fabs(wav_L[dir][y * W_L + x]);

                                    if (WavCL >= threshold) { //for max take into account attenuation response and offset
                                        float valcour = xlogf(fabsf(val));
                                        float valc = valcour - logmax;
                                        float vald = valc * rap;
                                        absciss = xexpf(vald);
                                    } else if (WavCL >= offset * mean[level]) {//offset only
                                        absciss = asig * WavCL + bsig;
                                    } else {
                                        absciss = amean * WavCL;
                                    }
                                    float kc = klev * (cmOpacityCurveWL[absciss * 500.f] - 0.5f);
                                    float amplieffect = kc <= 0.f ? 1.f : 1.7f;//we can change 1.5 - to 1.7 or more or less

                                    float kinterm = 1.f + amplieffect * kc;
                                    kinterm = kinterm <= 0.f ? 0.01f : kinterm;
                                    val *= (1.f + (kinterm - 1.f) * (*meaLut)[WavCL * lutFactor]);//change signal (contrast) for each level, direction, with LUT.

                                }
                            }
                        }
                    }
                }
            }
            //reconstruct lab
            wdspot->reconstruct(lab2->L[0], 1.f);

//Of course, we only retrieve the lab2 part that was used for the decomposition
#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif            
            for (int i = 0; i < lab->H; i++){
                for (int j = 0; j < lab->W; j++){
                    if (lab->L[i][j] > artifact_minimum  && lab->L[i][j] < artifact_maximum) {
                        lab->L[i][j] = lab2->L[i][j];
                        lab->a[i][j] = lab2->a[i][j];
                        lab->b[i][j] = lab2->b[i][j];
                    }
                }
            }
    }

}


void ImProcFunctions::finalContAllL(float* const* WavCoeffs_L, float * WavCoeffs_L0, int level, int dir, const cont_params &cp,
                                    int W_L, int H_L, float *mean, float *sigma, float *MaxP, const WavOpacityCurveWL & waOpacityCurveWL)
{
    if (cp.diagcurv  && cp.finena && MaxP[level] > 0.f && mean[level] != 0.f && sigma[level] != 0.f) { //curve
        float insigma = 0.666f; //SD
        float logmax = log(MaxP[level]); //log Max
        float rapX = (mean[level] + cp.sigmafin * sigma[level]) / (MaxP[level]); //rapport between sD / max
        float inx = log(insigma);
        float iny = log(rapX);
        float rap = inx / iny; //koef
        float asig = 0.166f / (sigma[level] * cp.sigmafin);
        float bsig = 0.5f - asig * mean[level];
        float amean = 0.5f / (mean[level]);

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic, W_L * 16) num_threads(wavNestedLevels) if (wavNestedLevels>1)
#endif

        for (int i = 0; i < W_L * H_L; i++) {
            float absciss;

            if (std::fabs(WavCoeffs_L[dir][i]) >= (mean[level] + cp.sigmafin * sigma[level])) { //for max
                float valcour = xlogf(std::fabs(WavCoeffs_L[dir][i]));
                float valc = valcour - logmax;
                float vald = valc * rap;
                absciss = xexpf(vald);
            } else if (std::fabs(WavCoeffs_L[dir][i]) >= mean[level]) {
                absciss = asig * std::fabs(WavCoeffs_L[dir][i]) + bsig;
            } else {
                absciss = amean * std::fabs(WavCoeffs_L[dir][i]);
            }

            float kc = waOpacityCurveWL[absciss * 500.f] - 0.5f;
            float reduceeffect = kc <= 0.f ? 1.f : 1.5f;

            float kinterm = 1.f + reduceeffect * kc;
            kinterm = kinterm <= 0.f ? 0.01f : kinterm;

            WavCoeffs_L[dir][i] *=  kinterm;
        }
    }

    int choicelevel = params->wavelet.Lmethod - 1;
    choicelevel = choicelevel == -1 ? 4 : choicelevel;

    int choiceClevel = 0;

    if (params->wavelet.CLmethod == "one") {
        choiceClevel = 0;
    } else if (params->wavelet.CLmethod == "inf") {
        choiceClevel = 1;
    } else if (params->wavelet.CLmethod == "sup") {
        choiceClevel = 2;
    } else if (params->wavelet.CLmethod == "all") {
        choiceClevel = 3;
    }

    int choiceDir = 0;

    if (params->wavelet.Dirmethod == "one") {
        choiceDir = 1;
    } else if (params->wavelet.Dirmethod == "two") {
        choiceDir = 2;
    } else if (params->wavelet.Dirmethod == "thr") {
        choiceDir = 3;
    } else if (params->wavelet.Dirmethod == "all") {
        choiceDir = 0;
    }

    int dir1 = (choiceDir == 2) ? 1 : 2;
    int dir2 = (choiceDir == 3) ? 1 : 3;

    if (choiceClevel < 3) { // not all levels visible, paint residual
        if (level == 0) {
            if (cp.backm != 2) { // nothing to change when residual is used as background
                float backGroundColor = (cp.backm == 1) ? 12000.f : 0.f;

                for (int i = 0; i < W_L * H_L; i++) {
                    WavCoeffs_L0[i] = backGroundColor;
                }
            }
        }
    }

    if (choiceClevel == 0) { // Only one level

        if (choiceDir == 0) { // All directions
            if (level != choicelevel) { // zero all for the levels != choicelevel
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_L * H_L; i++) {
                        WavCoeffs_L[d][i] = 0.f;
                    }
                }
            }
        } else { // zero the unwanted directions for level == choicelevel

            if (choicelevel >= cp.maxilev) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_L * H_L; i++) {
                        WavCoeffs_L[d][i] = 0.f;
                    }
                }
            } else if (level != choicelevel) { // zero all for the levels != choicelevel
                for (int i = 0; i < W_L * H_L; i++) {
                    WavCoeffs_L[dir1][i] = WavCoeffs_L[dir2][i] = 0.f;
                }
            }
        }
    } else if (choiceClevel == 1) { // Only below level
        if (choiceDir == 0) { // All directions
            if (level > choicelevel) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_L * H_L; i++) {
                        WavCoeffs_L[d][i] = 0.f;
                    }
                }
            }
        } else { // zero the unwanted directions for level >= choicelevel
            if (level > choicelevel) {
                for (int i = 0; i < W_L * H_L; i++) {
                    WavCoeffs_L[dir1][i] = WavCoeffs_L[dir2][i] = 0.f;
                }
            }
        }
    } else if (choiceClevel == 2) { // Only above level
        if (choiceDir == 0) { // All directions
            if (level <= choicelevel) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_L * H_L; i++) {
                        WavCoeffs_L[d][i] = 0.f;
                    }
                }
            }
        } else { // zero the unwanted directions for level >= choicelevel
            if (choicelevel >= cp.maxilev) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_L * H_L; i++) {
                        WavCoeffs_L[d][i] = 0.f;
                    }
                }
            }


            else if (level <= choicelevel) {
                for (int i = 0; i < W_L * H_L; i++) {
                    WavCoeffs_L[dir1][i] = WavCoeffs_L[dir2][i] = 0.f;
                }
            }
        }
    }


}

void ImProcFunctions::ContAllL(float *koeLi[12], float maxkoeLi, bool lipschitz, int maxlvl, LabImage * labco, const float* const* varhue, const float* const* varchrom, float* const* WavCoeffs_L, float * WavCoeffs_L0, int level, int dir, struct cont_params &cp,
                               int W_L, int H_L, int skip, float *mean, float *sigma, float *MaxP, float *MaxN, const WavCurve & wavCLVCcurve, const WavOpacityCurveW & waOpacityCurveW, const WavOpacityCurveSH & waOpacityCurveSH, FlatCurve* ChCurve, bool Chutili)
{
    assert(level >= 0);
    assert(maxlvl > level);

    static const float scales[10] = {1.f, 2.f, 4.f, 8.f, 16.f, 32.f, 64.f, 128.f, 256.f, 512.f};
    float scaleskip[10];

    for (int sc = 0; sc < 10; sc++) {
        scaleskip[sc] = scales[sc] / skip;
    }
/*
    if (settings->verbose) {
        printf("level=%i mean=%f sigma=%f maxp=%f\n", level, mean[level], sigma[level], MaxP[level]);
    }
*/
    constexpr float t_r = 40.f;
    constexpr float t_l = 10.f;
    constexpr float b_r = 75.f;
    constexpr float edd = 3.f;
    constexpr float eddstrength = 1.3f;
    constexpr float aedstr = (eddstrength - 1.f) / 90.f;
    constexpr float bedstr = 1.f - 10.f * aedstr;

    std::unique_ptr<float[]> beta(new float[W_L * H_L]);

    for (int co = 0; co < H_L * W_L; co++) {
        beta[co] = 1.f;
    }

    if (cp.eff < 2.5f) {
        float effect = cp.eff;
        float offs = 1.f;
        float mea[10];

        calceffect(level, mean, sigma, mea, effect, offs);

        for (int co = 0; co < H_L * W_L; co++) {
            float WavCL = std::fabs(WavCoeffs_L[dir][co]);

            if (WavCL < mea[0]) {
                beta[co] = 0.05f;
            } else if (WavCL < mea[1]) {
                beta[co] = 0.2f;
            } else if (WavCL < mea[2]) {
                beta[co] = 0.7f;
            } else if (WavCL < mea[3]) {
                beta[co] = 1.f;    //standard
            } else if (WavCL < mea[4]) {
                beta[co] = 1.f;
            } else if (WavCL < mea[5]) {
                beta[co] = 0.8f;    //+sigma
            } else if (WavCL < mea[6]) {
                beta[co] = 0.6f;
            } else if (WavCL < mea[7]) {
                beta[co] = 0.4f;
            } else if (WavCL < mea[8]) {
                beta[co] = 0.2f;    // + 2 sigma
            } else if (WavCL < mea[9]) {
                beta[co] = 0.1f;
            } else {
                beta[co] = 0.0f;
            }

        }
    }


    if (cp.val > 0  && cp.edgeena) {


        float * koe = nullptr;
        float maxkoe = 0.f;

        if (!lipschitz) {
            koe = new float [H_L * W_L];

            for (int i = 0; i < W_L * H_L; i++) {
                koe[i] = 0.f;
            }

            maxkoe = 0.f;

            if (cp.detectedge) {
                float** tmC;
                int borderL = 1;
                tmC = new float*[H_L];

                for (int i = 0; i < H_L; i++) {
                    tmC[i] = new float[W_L];
                }

                {
                    for (int i = 1; i < H_L - 1; i++) {
                        for (int j = 1; j < W_L - 1; j++) {
                            //edge detection wavelet TMC Canny
                            // also possible to detect noise with 5x5 instead of 3x3
                            tmC[i][j] = (4.f * WavCoeffs_L[dir][i * W_L + j] + 2.f * WavCoeffs_L[dir][(i - 1) * W_L + j] + 2.f * WavCoeffs_L[dir][(i + 1) * W_L + j]
                                         + 2.f * WavCoeffs_L[dir][i * W_L + j + 1] + 2.f * WavCoeffs_L[dir][i * W_L + j - 1] + WavCoeffs_L[dir][(i - 1) * W_L + j - 1]
                                         + WavCoeffs_L[dir][(i - 1) * W_L + j + 1] + WavCoeffs_L[dir][(i + 1) * W_L + j - 1] + WavCoeffs_L[dir][(i + 1) * W_L + j + 1]) / 16.f;

                            // apply to each direction Wavelet level : horizontal / vertiacle / diagonal
                        }
                    }
                }




                for (int i = borderL; i < H_L - borderL; i++) {
                    for (int j = borderL; j < W_L - borderL; j++) {
                        // my own algo : probably a little false, but simpler as Lipschitz !
                        float thr = 40.f; //avoid artifact eg. noise...to test
                        float thr2 = edd; //edd can be modified in option ed_detect
                        thr2 += cp.eddet / 30.f; //to test
                        float temp = WavCoeffs_L[dir][i * W_L + j];

                        if (temp >= 0.f &&  temp < thr) {
                            temp = thr;
                        }

                        if (temp < 0.f &&  temp > -thr) {
                            temp = -thr;
                        }

                        koe[i * W_L + j] = rtengine::min(thr2, std::fabs(tmC[i][j] / temp));

                        maxkoe = rtengine::max(maxkoe, koe[i * W_L + j]);
                        float diff = maxkoe - koe[i * W_L + j];
                        diff *= (cp.eddet / 100.f);
                        float interm = maxkoe - diff;

                        if (interm < cp.eddetthr / 30.f) {
                            interm = 0.01f;
                        }

                        koe[i * W_L + j] = interm;

                    }
                }

                for (int i = 0; i < H_L; i++) {
                    delete [] tmC[i];
                }

                delete [] tmC;

            }
        }

        //end detect edge
        float rad = ((float)cp.rad) / 60.f; //radius ==> not too high value to avoid artifacts
        float value = ((float)cp.val) / 8.f; //strength

        if (scaleskip[1] < 1.f) {
            float atten01234 = 0.80f;
            value *= (atten01234 * scaleskip[1]);    //for zoom < 100% reduce strength...I choose level 1...but!!
        }
        float edghig = settings->edghi;//increase or reduce "reinforce"
        float edglow = settings->edglo;//increase or reduce "reduce"
        float limrad = settings->limrad;//threshold action in function radius (rad)
       // printf("edghi=%f edglo=%f limrad=%f\n", edghig, edglow, limrad); 
        // value *= beta;
        float edge = 1.f;
        float lim0 = limrad; //arbitrary limit for low radius and level between 2 or 3 to 30 maxi
        float lev = float (level);
        float repart = (float)cp.til;


        if (cp.reinforce != 2) {
            const float brepart =
                cp.reinforce == 1
                ? edghig
                : edglow;
            const float arepart = -(brepart - 1.f) / (lim0 / 60.f);

            if (rad < (lim0 / 60.f)) {
                repart *= (arepart * rad + brepart);    //linear repartition of repart
            }
        }
        float al0 = 1.f + (repart) / 50.f;
        float al10 = 1.0f; //arbitrary value ==> less = take into account high levels
        //  float ak =-(al0-al10)/10.f;//10 = maximum levels
        float ak = -(al0 - al10) / 10.f; //10 = maximum levels
        float bk = al0;
        float koef = ak * level + bk; //modulate for levels : more levels high, more koef low ==> concentrated action on low levels, without or near for high levels
        float expkoef = -std::pow(std::fabs(rad - lev), koef); //reduce effect for high levels
      //  printf("repart=%f\n", repart);

        if (cp.reinforce == 3) {
            if (rad < (lim0 / 60.f) && level == 0) {
                expkoef *= abs(repart);    //reduce effect for low values of rad and level=0==> quasi only level 1 is effective
            }
        }

        if (cp.reinforce == 1) {
            if (rad < (lim0 / 60.f) && level == 1) {
                expkoef /= repart;    //increase effect for low values of rad and level=1==> quasi only level 0 is effective
            }
        }

        //take into account local contrast
        float refin = value * exp(expkoef);

        if (cp.link  && cp.noiseena) { //combi
            {
                if (level == 0) {
                    refin *= (1.f + cp.lev0s / 50.f);    // we can change this sensibility!
                }

                if (level == 1) {
                    refin *= (1.f + cp.lev1s / 50.f);
                }

                if (level == 2) {
                    refin *= (1.f + cp.lev2s / 50.f);
                }

                if (level == 3) {
                    refin *= (1.f + cp.lev3s / 50.f);
                }
            }
        }

        float edgePrecalc = 1.f + refin; //estimate edge "pseudo variance"

        if (cp.EDmet == 2 && MaxP[level] > 0.f) { //curve
            //  if (exa) {//curve
            float insigma = 0.666f; //SD
            float logmax = log(MaxP[level]); //log Max
            float rapX = (mean[level] + sigma[level]) / (MaxP[level]); //rapport between sD / max
            float inx = log(insigma);
            float iny = log(rapX);
            float rap = inx / iny; //koef
            float asig = 0.166f / (sigma[level]);
            float bsig = 0.5f - asig * mean[level];
            float amean = 0.5f / (mean[level]);
            float absciss = 0.f;
            float kinterm;
            float kmul;
            int borderL = 1;

            for (int i = borderL; i < H_L - borderL; i++) {
                for (int j = borderL; j < W_L - borderL; j++) {
                    int k = i * W_L + j;

                    if (cp.detectedge) {
                        if (!lipschitz) {
                            if (cp.eddet > 10.f) {
                                edge = (aedstr * cp.eddet + bedstr) * (edgePrecalc * (1.f + koe[k])) / (1.f + 0.9f * maxkoe);
                            } else {
                                edge = (edgePrecalc * (1.f + koe[k])) / (1.f + 0.9f * maxkoe);
                            }
                        }

                        if (lipschitz) {
                            if (level < 4) {
                                edge = 1.f + (edgePrecalc - 1.f) * (koeLi[level * 3][k]) / (1.f + 0.9f * maxkoeLi);
                            } else {
                                edge = edgePrecalc;
                            }
                        }
                    } else {
                        edge = edgePrecalc;
                    }

                    if (cp.edgcurv) {
                        if (std::fabs(WavCoeffs_L[dir][k]) >= (mean[level] + sigma[level])) { //for max
                            float valcour = xlogf(std::fabs(WavCoeffs_L[dir][k]));
                            float valc = valcour - logmax;
                            float vald = valc * rap;
                            absciss = exp(vald);

                        } else if (std::fabs(WavCoeffs_L[dir][k]) >= mean[level] &&  std::fabs(WavCoeffs_L[dir][k]) < (mean[level] + sigma[level])) {
                            absciss = asig * std::fabs(WavCoeffs_L[dir][k]) + bsig;
                        } else if (std::fabs(WavCoeffs_L[dir][k]) < mean[level]) {
                            absciss = amean * std::fabs(WavCoeffs_L[dir][k]);
                        }

                        // Threshold adjuster settings==> approximative for curve
                        //kmul about average cbrt(3--40 / 10)==>1.5 to 2.5
                        //kmul about SD   10--60  / 35 ==> 2
                        // kmul about low  cbrt((5.f+cp.edg_low)/5.f);==> 1.5
                        // kmul about max ==> 9
                        // we can change these values
                        // result is different not best or bad than threshold slider...but similar
                        constexpr float abssd = 4.f; //amplification reference
                        constexpr float bbssd = 2.f; //mini ampli
                        float kmuld = 0.f;

                        if (absciss > 0.666f && absciss < 1.f) {
                            constexpr float maxamp = 2.5f; //maxi ampli at end
                            constexpr float maxampd = 10.f; //maxi ampli at end
                            constexpr float a_abssd = (maxamp - abssd) / 0.333f;
                            constexpr float b_abssd = maxamp - a_abssd;
                            constexpr float da_abssd = (maxampd - abssd) / 0.333f;
                            constexpr float db_abssd = maxampd - da_abssd;
                            kmul = a_abssd * absciss + b_abssd;    //about max  ==> kinterm
                            kmuld = da_abssd * absciss + db_abssd;
                        } else {
                            constexpr float am = (abssd - bbssd) / 0.666f;
                            kmul = kmuld = absciss * am + bbssd;
                        }

                        const float kc = kmul * (wavCLVCcurve[absciss * 500.f] - 0.5f);

                        if (kc >= 0.f) {
                            float reduceeffect = 0.6f;
                            kinterm = 1.f + reduceeffect * kmul * (wavCLVCcurve[absciss * 500.f] - 0.5f);    //about 1 to 3 general and big amplification for max (under 0)
                        } else {
                            const float kcd = kmuld * (wavCLVCcurve[absciss * 500.f] - 0.5f);
                            kinterm = 1.f - (SQR(kcd)) / 10.f;
                        }

                        if (kinterm < 0.f) {
                            kinterm = 0.01f;
                        }

                        edge *= kinterm;
                        edge = rtengine::max(edge, 1.f);
                    }

                    WavCoeffs_L[dir][k] *= (1.f + (edge - 1.f) * beta[k]);
                }
            }
        } else if (cp.EDmet == 1) { //threshold adjuster
            float MaxPCompare = MaxP[level] * SQR(cp.edg_max / 100.f); //100 instead of b_r...case if b_r < 100
            float MaxNCompare = MaxN[level] * SQR(cp.edg_max / 100.f); //always reduce a little edge for near max values
            float edgeSdCompare = (mean[level] + 1.5f * sigma[level]) * SQR(cp.edg_sd / t_r); // 1.5 standard deviation #80% range between mean 50% and 80%
            float edgeMeanCompare = mean[level] * SQR(cp.edg_mean / t_l);
            float edgeLowCompare = (5.f + SQR(cp.edg_low));
            float edgeMeanFactor = cbrt(cp.edg_mean / t_l);
            float interm;

            if (cp.edg_low < 10.f) {
                interm = cbrt((5.f + cp.edg_low) / 5.f);
            } else {
                interm = 1.437f;    //cbrt(3);
            }

            float edgeLowFactor = interm;
            float edgeSdFactor = cp.edg_sd / t_r;
            float edgeMaxFactor = SQR(cp.edg_max / b_r);
            float edgMaxFsup = (cp.edg_max / b_r); //reduce increase of effect for high values contrast..if slider > b_r

            //for (int i=0; i<W_L*H_L; i++) {
            int borderL = 1;

            for (int i = borderL; i < H_L - borderL; i++) {
                for (int j = borderL; j < W_L - borderL; j++) {
                    int k = i * W_L + j;

                    if (cp.detectedge) {
                        if (!lipschitz) {
                            if (cp.eddet > 10.f) {
                                edge = (aedstr * cp.eddet + bedstr) * (edgePrecalc * (1.f + koe[k])) / (1.f + 0.9f * maxkoe);
                            } else {
                                edge = (edgePrecalc * (1.f + koe[k])) / (1.f + 0.9f * maxkoe);
                            }
                        }

                        if (lipschitz) {
                            if (level < 4) {
                                edge = 1.f + (edgePrecalc - 1.f) * (koeLi[level * 3][k]) / (1.f + 0.9f * maxkoeLi);
                            } else {
                                edge = edgePrecalc;
                            }
                        }
                    } else {
                        edge = edgePrecalc;
                    }

                    //algorithm that takes into account local contrast
                    // I use a thresholdadjuster with
                    // Bottom left ==> minimal low value for local contrast (not 0, but 5...we can change)
                    // 0 10*10 35*35 100*100 substantially correspond to the true distribution of low value, mean, standard-deviation and max (ed 5, 50, 400, 4000
                    // Top left ==> mean reference value (for each level), we can change cbrt(cp.edg_mean/10.f)
                    // Top Right==> standard deviation (for each level) we can change (cp.edg_sd/35.f)
                    // bottom right ==> Max for positif and negatif contrast we can change cp.edg_max/100.f
                    // If we move sliders to the left, local contrast is reduced
                    // if we move sliders to the right local contrast is increased
                    // MaxP, MaxN, mean, sigma are calculated if necessary (val > 0) by evaluate2(), eval2(), aver() , sigma()
//                    if (b_r < 100.f  && cp.edg_max / b_r > 1.f) { //in case of b_r < 100 and slider move to right
                    if (cp.edg_max / b_r > 1.f) { //in case of b_r < 100 and slider move to right
                        if (WavCoeffs_L[dir][k] > MaxPCompare * cp.edg_max / b_r) {
                            edge *= edgMaxFsup;

                            if (edge < 1.f) {
                                edge = 1.f;
                            }
                        } else if (WavCoeffs_L[dir][k] < MaxNCompare * cp.edg_max / b_r) {
                            edge *= edgMaxFsup;

                            if (edge < 1.f) {
                                edge = 1.f;
                            }
                        }
                    }

                    if (WavCoeffs_L[dir][k] > MaxPCompare) {
                        edge *= edgeMaxFactor;

                        if (edge < 1.f) {
                            edge = 1.f;
                        }
                    }//reduce edge if > new max
                    else if (WavCoeffs_L[dir][k] < MaxNCompare) {
                        edge *= edgeMaxFactor;

                        if (edge < 1.f) {
                            edge = 1.f;
                        }
                    }

                    if (std::fabs(WavCoeffs_L[dir][k]) >= edgeMeanCompare && std::fabs(WavCoeffs_L[dir][k]) < edgeSdCompare) {
                        //if (std::fabs(WavCoeffs_L[dir][i]) > edgeSdCompare) {
                        edge *= edgeSdFactor;

                        if (edge < 1.f) {
                            edge = 1.f;
                        }
                    }//modify effect if sd change

                    if (std::fabs(WavCoeffs_L[dir][k]) < edgeMeanCompare) {
                        edge *= edgeMeanFactor;

                        if (edge < 1.f) {
                            edge = 1.f;
                        }
                    } // modify effect if mean change

                    if (std::fabs(WavCoeffs_L[dir][k]) < edgeLowCompare) {
                        edge *= edgeLowFactor;

                        if (edge < 1.f) {
                            edge = 1.f;
                        }
                    }

                    WavCoeffs_L[dir][k] *= (1.f + (edge - 1.f) * beta[k]);
                }
            }
        }

        if (!lipschitz) {
            delete [] koe;
        }
        if (!(cp.bam && cp.finena)) {
            beta.reset();
        }
    }

    if (!cp.link && cp.noiseena)   { //used both with denoise 1 2 3
        float refine = 0.f;
        if (level == 0) {
            refine = cp.lev0s / 40.f;
        } else if (level == 1) {
            refine = cp.lev1s / 40.f;
        } else if (level == 2) {
            refine = cp.lev2s / 40.f;
        } else if (level == 3) {
            refine = cp.lev3s / 40.f;
        }

        if (refine != 0.f) {
            refine += 1.f;
            for (int i = 0; i < W_L * H_L; i++) {
                WavCoeffs_L[dir][i] *= refine;
            }
        }
    }

    float cpMul = cp.mul[level];

    if (cpMul != 0.f && cp.contena) { // cpMul == 0.f means all will be multiplied by 1.f, so we can skip this

        const float skinprot = params->wavelet.skinprotect;
        const float skinprotneg = -skinprot;
        const float factorHard = (1.f - skinprotneg / 100.f);
        const float offs = params->wavelet.offset;
        const float lowthr = params->wavelet.lowthr;
        float mea[10];
        float effect = cp.sigm;
        float lbeta;

        calceffect(level, mean, sigma, mea, effect, offs);

        bool useChromAndHue = (skinprot != 0.f || cp.HSmet);
        float modchro;

        float red0 = 0.005f * (110.f - lowthr);
        float red1 = 0.008f * (110.f - lowthr);
        float red2 = 0.011f * (110.f - lowthr);
//        int n = 0;
//        int m = 0;
//        int p = 0;
//        int q = 0;
        for (int i = 0; i < W_L * H_L; i++) {
            float kLlev = 1.f;

            if (cpMul < 0.f) {
                lbeta = 1.f; // disabled for negatives values "less contrast"
            } else {
                float WavCL = std::fabs(WavCoeffs_L[dir][i]);

                //reduction amplification: max action between mean / 2 and mean + sigma
                // arbitrary coefficient, we can add a slider !!
                if (WavCL < mea[0]) {
                    lbeta = 0.4f * red0;//preserve very low contrast (sky...)
                } else if (WavCL < mea[1]) {
                    lbeta = 0.5f * red1;
                } else if (WavCL < mea[2]) {
                    lbeta = 0.7f * red2;
                } else if (WavCL < mea[3]) {
                    lbeta = 1.f;    //standard
                } else if (WavCL < mea[4]) {
                    lbeta = 1.f;
                } else if (WavCL < mea[5]) {
                    lbeta = 0.8f;    //+sigma
                } else if (WavCL < mea[6]) {
                    lbeta = 0.6f;
                } else if (WavCL < mea[7]) {
                    lbeta = 0.4f;
                } else if (WavCL < mea[8]) {
                    lbeta = 0.2f;    // + 2 sigma
                } else if (WavCL < mea[9]) {
                    lbeta = 0.1f;
                } else {
                    lbeta = 0.0f;
                }
            }

            float scale = 1.f;
            float scale2 = 1.f;

            float LL100, LL100res, LL100init, kH[maxlvl];

            int ii = i / W_L;
            int jj = i - ii * W_L;
            float LL = labco->L[ii * 2][jj * 2];
            LL100 = LL100init = LL / 327.68f;
            LL100res = WavCoeffs_L0[i] / 327.68f;
            float delta = std::fabs(LL100init - LL100res) / (maxlvl / 2);

            for (int ml = 0; ml < maxlvl; ml++) {
                if (ml < maxlvl / 2) {
                    kH[ml] = (LL100res + ml * delta) / LL100res;    // fixed a priori max to level middle
                } else {
                    kH[ml] = (LL100init - ml * delta) / LL100res;
                }
            }


            if (useChromAndHue) {
                float modhue = varhue[ii][jj];
                modchro = varchrom[ii * 2][jj * 2];
                // hue chroma skin with initial lab data
                scale = 1.f;

                if (skinprot > 0.f) {
                    Color::SkinSatCbdl2(LL100, modhue, modchro, skinprot, scale, true, cp.b_l, cp.t_l, cp.t_r, cp.b_r, 0);  //0 for skin and extand
                } else if (skinprot < 0.f) {
                    Color::SkinSatCbdl2(LL100, modhue, modchro, skinprotneg, scale, false, cp.b_l, cp.t_l, cp.t_r, cp.b_r, 0);

                    if (scale == 1.f) {
                        scale = factorHard;
                    } else {
                        scale = 1.f;
                    }
                }

            }

            if (Chutili) {
                int i_i = i / W_L;
                int j_j = i - i_i * W_L;
                float modhue2 = varhue[i_i][j_j];
                float valparam = static_cast<float>(ChCurve->getVal(Color::huelab_to_huehsv2(modhue2))) - 0.5f; //get valparam=f(H)

                if (valparam > 0.f) {
                    scale2 = 1.f + 3.f * valparam;    //arbitrary value
                } else {
                    scale2 = 1.f + 1.9f * valparam;    //near 0 but not zero if curve # 0
                }
            }

            //linear transition HL
            float diagacc = 1.f;
            float alpha = (1024.f + 15.f * (float) cpMul * scale * scale2 * lbeta * diagacc) / 1024.f ;

            if (cp.HSmet  && cp.contena) {
           // if (cp.HSmet  && cp.contena  && waOpacityCurveSH) {//waOpacityCurveSH no long use
                float aaal = (1.f - alpha) / ((cp.b_lhl - cp.t_lhl) * kH[level]);
                float bbal = 1.f - aaal * cp.b_lhl * kH[level];
                float aaar = (alpha - 1.f) / (cp.t_rhl - cp.b_rhl) * kH[level];
                float bbbr = 1.f - cp.b_rhl * aaar * kH[level];
                //linear transition Shadows
                float aaalS = (1.f - alpha) / (cp.b_lsl - cp.t_lsl);
                float bbalS = 1.f - aaalS * cp.b_lsl;
                float aaarS = (alpha - 1.f) / (cp.t_rsl - cp.b_rsl);
                float bbbrS = 1.f - cp.b_rsl * aaarS;

                if (level <= cp.numlevH) { //in function of levels
                    if ((LL100 > cp.t_lhl * kH[level] && LL100 < cp.t_rhl * kH[level])) {
                        kLlev = alpha;
                    } else if ((LL100 > cp.b_lhl * kH[level] && LL100 <= cp.t_lhl * kH[level])) {
                        kLlev = aaal * LL100 + bbal;
                    } else if ((LL100 > cp.t_rhl * kH[level] && LL100 <= cp.b_rhl * kH[level])) {
                        kLlev = aaar * LL100 + bbbr;
                    } else {
                        kLlev = 1.f;
                    }
                }

                if (level >= cp.numlevS - 1) {
                    //   if(klevred < 0.f && level >= 3) {//level > 3 to avoid bad use of the curve if user put positives values negatives
                    if ((LL100 > cp.t_lsl && LL100 < cp.t_rsl)) {
                        kLlev = alpha;
                      //  n++;
                    } else if ((LL100 > cp.b_lsl && LL100 <= cp.t_lsl)) {
                        kLlev = aaalS * LL100 + bbalS;
                      //  m++;
                    } else if ((LL100 > cp.t_rsl && LL100 <= cp.b_rsl)) {
                        kLlev = aaarS * LL100 + bbbrS;
                      //  p++;
                    } else {
                        kLlev = 1.f;
                      //  q++;
                    }
                }

            } else {
                kLlev = alpha;
            }

            WavCoeffs_L[dir][i] *= (kLlev);
        }
        
      //  printf("lev=%i n=%i m=%i p=%i q=%i\n", level, n, m, p, q);
    }

    if (waOpacityCurveW) {
        cp.opaW = true;
    }

    if (cp.bam && cp.finena) {
        const float effect = cp.sigmadir;
        constexpr float offs = 1.f;
        float mea[10];

        calceffect(level, mean, sigma, mea, effect, offs);

        for (int co = 0; co < H_L * W_L; co++) {
            float WavCL = std::fabs(WavCoeffs_L[dir][co]);

            if (WavCL < mea[0]) {
                beta[co] = 0.05f;
            } else if (WavCL < mea[1]) {
                beta[co] = 0.2f;
            } else if (WavCL < mea[2]) {
                beta[co] = 0.7f;
            } else if (WavCL < mea[3]) {
                beta[co] = 1.f;    //standard
            } else if (WavCL < mea[4]) {
                beta[co] = 1.f;
            } else if (WavCL < mea[5]) {
                beta[co] = 0.8f;    //+sigma
            } else if (WavCL < mea[6]) {
                beta[co] = 0.6f;
            } else if (WavCL < mea[7]) {
                beta[co] = 0.4f;
            } else if (WavCL < mea[8]) {
                beta[co] = 0.2f;    // + 2 sigma
            } else if (WavCL < mea[9]) {
                beta[co] = 0.1f;
            } else {
                beta[co] = 0.01f;
            }
        }

        if (cp.opaW && cp.BAmet == 2) {
            int iteration = cp.ite;
            int itplus = 7 + iteration;
            int itmoins = 7 - iteration;
            int med = maxlvl / 2;
            int it;

            if (level < med) {
                it = itmoins;
            } else if (level == med) {
                it = 7;
            } else { /*if (level > med)*/
                it = itplus;
            }

            for (int j = 0; j < it; j++) {
                //float bal = cp.balan;//-100 +100
                float kba = 1.f;

                //  if (dir <3) kba= 1.f + bal/600.f;
                //  if (dir==3) kba = 1.f - bal/300.f;
                for (int i = 0; i < W_L * H_L; i++) {
                    int ii = i / W_L;
                    int jj = i - ii * W_L;
                    float LL100 = labco->L[ii * 2][jj * 2] / 327.68f;
                    float k1 = 0.3f * (waOpacityCurveW[6.f * LL100] - 0.5f); //k1 between 0 and 0.5    0.5==> 1/6=0.16
                    float k2 = k1 * 2.f;

                    if (dir < 3) {
                        kba = 1.f + k1;
                    }

                    if (dir == 3) {
                        kba = 1.f - k2;
                    }

                    WavCoeffs_L[dir][i] *= (1.f + (kba - 1.f) * beta[i]);
                }
            }
        }

        if (cp.BAmet == 1) {
            int iteration = cp.ite;
            int itplus = 7 + iteration;
            int itmoins = 7 - iteration;
            int med = maxlvl / 2;
            int it;

            if (level < med) {
                it = itmoins;
            } else if (level == med) {
                it = 7;
            } else { /*if (level > med)*/
                it = itplus;
            }

            for (int j = 0; j < it; j++) {
                float bal = cp.balan;//-100 +100
                float kba = 1.f;

                //  if (dir <3) kba= 1.f + bal/600.f;
                //  if (dir==3) kba = 1.f - bal/300.f;
                for (int i = 0; i < W_L * H_L; i++) {
                    int ii = i / W_L;
                    int jj = i - ii * W_L;
                    float k1 = 600.f;
                    float k2 = 300.f;
                    float LL100 = labco->L[ii * 2][jj * 2] / 327.68f;
                    constexpr float aa = 4970.f;
                    constexpr float bb = -397000.f;
                    constexpr float b0 = 100000.f;
                    constexpr float a0 = -4970.f;

                    if (LL100 > 80.f) {
                        k1 = aa * LL100 + bb;
                        k2 = 0.5f * k1;
                    }

                    if (LL100 < 20.f) {
                        k1 = a0 * LL100 + b0;
                        k2 = 0.5f * k1;
                    }

                    //k1=600.f;
                    //k2=300.f;
                    //k1=0.3f*(waOpacityCurveW[6.f*LL100]-0.5f);//k1 between 0 and 0.5    0.5==> 1/6=0.16
                    //k2=k1*2.f;
                    if (dir < 3) {
                        kba = 1.f + bal / k1;
                    }

                    if (dir == 3) {
                        kba = 1.f - bal / k2;
                    }

                    WavCoeffs_L[dir][i] *= (1.f + (kba - 1.f) * beta[i]);
                }
            }
        }
    }

    // to see each level of wavelet ...level from 0 to 8
//    int choicelevel = params->wavelet.Lmethod - 1;
//    choicelevel = choicelevel == -1 ? 4 : choicelevel;
}

void ImProcFunctions::ContAllAB(LabImage * labco, int maxlvl, float ** varhue, float **varchrom, float* const* WavCoeffs_ab, float * WavCoeffs_ab0, int level, int dir, const WavOpacityCurveW & waOpacityCurveW, struct cont_params &cp,
                                int W_ab, int H_ab, const bool useChannelA, float *meanab, float *sigmaab)
{
    float cpMul = cp.mul[level];

    if (cpMul != 0.f && cp.CHmet == 2 && cp.chro != 0.f  && cp.chromena) { // cpMul == 0.f or cp.chro = 0.f means all will be multiplied by 1.f, so we can skip this
        const float skinprot = params->wavelet.skinprotect;
        const float skinprotneg = -skinprot;
        const float factorHard = (1.f - skinprotneg / 100.f);
        const float cpChrom = cp.chro;

        //to adjust increase contrast with local contrast
        bool useSkinControl = (skinprot != 0.f);

        float mea[10];
        float effect = cp.sigmacol;
        float betaab;
        float offs = 1.f;

        calceffect(level, meanab, sigmaab, mea, effect, offs);

        for (int i = 0; i < W_ab * H_ab; i++) {
            float WavCab = std::fabs(WavCoeffs_ab[dir][i]);

            if (WavCab < mea[0]) {
                betaab = 0.05f;
            } else if (WavCab < mea[1]) {
                betaab = 0.2f;
            } else if (WavCab < mea[2]) {
                betaab = 0.7f;
            } else if (WavCab < mea[3]) {
                betaab = 1.f;    //standard
            } else if (WavCab < mea[4]) {
                betaab = 1.f;
            } else if (WavCab < mea[5]) {
                betaab = 0.8f;    //+sigma
            } else if (WavCab < mea[6]) {
                betaab = 0.6f;
            } else if (WavCab < mea[7]) {
                betaab = 0.4f;
            } else if (WavCab < mea[8]) {
                betaab = 0.2f;    // + 2 sigma
            } else if (WavCab < mea[9]) {
                betaab = 0.1f;
            } else {
                betaab = 0.0f;
            }

            float scale = 1.f;

            if (useSkinControl) {
                int ii = i / W_ab;
                int jj = i - ii * W_ab;
                float LL100 = labco->L[ii * 2][jj * 2] / 327.68f;
                float modhue = varhue[ii][jj];
                float modchro = varchrom[ii * 2][jj * 2];
                // hue chroma skin with initial lab data

                if (skinprot > 0.f) {
                    Color::SkinSatCbdl2(LL100, modhue, modchro, skinprot, scale, true, cp.b_l, cp.t_l, cp.t_r, cp.b_r, 0);  //0 for skin and extand
                } else if (skinprot < 0.f) {
                    Color::SkinSatCbdl2(LL100, modhue, modchro, skinprotneg, scale, false, cp.b_l, cp.t_l, cp.t_r, cp.b_r, 0);
                    scale = (scale == 1.f) ? factorHard : 1.f;
                }

            }

            const float alphaC = (1024.f + 15.f * cpMul * cpChrom *  betaab * scale / 50.f) / 1024.f ;

            WavCoeffs_ab[dir][i] *= alphaC;
        }
    }

    //Curve chro

    float cpMulC = cp.mulC[level];

    //  if ( (cp.curv || cp.CHSLmet==1) && cp.CHmet!=2 && level < 9 && cpMulC != 0.f) { // cpMulC == 0.f means all will be multiplied by 1.f, so we can skip
    if (cp.CHmet != 2 && level < 9 && cpMulC != 0.f  && cp.chromena) { // cpMulC == 0.f means all will be multiplied by 1.f, so we can skip
        const float skinprot = params->wavelet.skinprotect;
        const float skinprotneg = -skinprot;
        const float factorHard = (1.f - skinprotneg / 100.f);
        bool useSkinControl = (skinprot != 0.f);


        float mea[10];
        float effect = cp.sigmacol;
        float betaab;
        float offs = 1.f;

        calceffect(level, meanab, sigmaab, mea, effect, offs);

        for (int i = 0; i < W_ab * H_ab; i++) {
            float WavCab = std::fabs(WavCoeffs_ab[dir][i]);

            if (WavCab < mea[0]) {
                betaab = 0.05f;
            } else if (WavCab < mea[1]) {
                betaab = 0.2f;
            } else if (WavCab < mea[2]) {
                betaab = 0.7f;
            } else if (WavCab < mea[3]) {
                betaab = 1.f;    //standard
            } else if (WavCab < mea[4]) {
                betaab = 1.f;
            } else if (WavCab < mea[5]) {
                betaab = 0.8f;    //+sigma
            } else if (WavCab < mea[6]) {
                betaab = 0.6f;
            } else if (WavCab < mea[7]) {
                betaab = 0.4f;
            } else if (WavCab < mea[8]) {
                betaab = 0.2f;    // + 2 sigma
            } else if (WavCab < mea[9]) {
                betaab = 0.1f;
            } else {
                betaab = 0.0f;
            }

            int ii = i / W_ab;
            int jj = i - ii * W_ab;
            //WL and W_ab are identical
            float scale = 1.f;
            float modchro = varchrom[ii * 2][jj * 2];

            if (useSkinControl) {
                // hue chroma skin with initial lab data
                float LL100 = labco->L[ii * 2][jj * 2] / 327.68f;
                float modhue = varhue[ii][jj];

                if (skinprot > 0.f) {
                    Color::SkinSatCbdl2(LL100, modhue, modchro, skinprot, scale, true, cp.b_l, cp.t_l, cp.t_r, cp.b_r, 1);  //1 for curve
                } else if (skinprot < 0.f) {
                    Color::SkinSatCbdl2(LL100, modhue, modchro, skinprotneg, scale, false, cp.b_l, cp.t_l, cp.t_r, cp.b_r, 1);
                    scale = (scale == 1.f) ? factorHard : 1.f;
                }
            }

            float beta = (1024.f + 20.f * cpMulC * scale * betaab) / 1024.f ;

            if (beta < 0.02f) {
                beta = 0.02f;
            }

            float kClev = beta;

            if (cp.CHmet == 1) {
                if (level < cp.chrom) {
                    //linear for saturated
                    if ((modchro > cp.t_lsat && modchro < cp.t_rsat)) {
                        kClev = beta;
                    } else if ((modchro > cp.b_lsat && modchro <= cp.t_lsat)) {
                        float aaal = (1.f - beta) / (cp.b_lsat - cp.t_lsat);
                        float bbal = 1.f - aaal * cp.b_lsat;
                        kClev = aaal * modchro + bbal;
                    } else if ((modchro > cp.t_rsat &&  modchro <= cp.b_rsat)) {
                        float aaar = (beta - 1.f) / (cp.t_rsat - cp.b_rsat);
                        float bbbr = 1.f - cp.b_rsat * aaar;
                        kClev = aaar * modchro + bbbr;
                    } else {
                        kClev = 1.f;
                    }
                } else {
                    //linear for pastel
                    if ((modchro > cp.t_lpast && modchro < cp.t_rpast)) {
                        kClev = beta;
                    } else if ((modchro > cp.b_lpast && modchro <= cp.t_lpast)) {
                        float aaalS = (1.f - beta) / (cp.b_lpast - cp.t_lpast);
                        float bbalS = 1.f - aaalS * cp.b_lpast;
                        kClev = aaalS * modchro + bbalS;
                    } else if ((modchro > cp.t_rpast &&  modchro <= cp.b_rpast)) {
                        float aaarS = (beta - 1.f) / (cp.t_rpast - cp.b_rpast);
                        float bbbrS = 1.f - cp.b_rpast * aaarS;
                        kClev = aaarS * modchro + bbbrS;
                    } else {
                        kClev = 1.f;
                    }
                }
            } else if (cp.CHmet == 0) {
                kClev = beta;
            }

            WavCoeffs_ab[dir][i] *= kClev;
        }
    }

    bool useOpacity;
    float mulOpacity = 0.f;

    if (useChannelA) {
        useOpacity = cp.opaRG;

        if (level < 9) {
            mulOpacity = cp.mulopaRG[level];
        }
    } else {
        useOpacity = cp.opaBY;

        if (level < 9) {
            mulOpacity = cp.mulopaBY[level];
        }
    }

    if ((useOpacity && level < 9 && mulOpacity != 0.f) && cp.toningena) { //toning
    //    if (settings->verbose) {
    //        printf("Toning enabled\n");
    //    }

        float mea[10];
        float effect = cp.sigmaton;
        float betaab;
        float offs = 1.f;
        float protec = 0.01f * (100.f - cp.protab);
        float aref1 = cp.a_high;
        float bref1 = cp.b_high;
        float aref2 = cp.a_low;
        float bref2 = cp.b_low;

        float kk = 100.f;
        float arefplus1 = aref1  + cp.rangeab * kk;
        float arefmoins1 = aref1 - cp.rangeab * kk;
        float brefplus1 = bref1 + cp.rangeab * kk;
        float brefmoins1 = bref1 - cp.rangeab * kk;

        float arefplus2 = aref2  + cp.rangeab * kk;
        float arefmoins2 = aref2 - cp.rangeab * kk;
        float brefplus2 = bref2 + cp.rangeab * kk;
        float brefmoins2 = bref2 - cp.rangeab * kk;

        calceffect(level, meanab, sigmaab, mea, effect, offs);

        for (int co = 0; co < W_ab * H_ab; co++) {
            float WavCab = std::fabs(WavCoeffs_ab[dir][co]);

            if (WavCab < mea[0]) {
                betaab = 0.05f;
            } else if (WavCab < mea[1]) {
                betaab = 0.2f;
            } else if (WavCab < mea[2]) {
                betaab = 0.7f;
            } else if (WavCab < mea[3]) {
                betaab = 1.f;    //standard
            } else if (WavCab < mea[4]) {
                betaab = 1.f;
            } else if (WavCab < mea[5]) {
                betaab = 0.8f;    //+sigma
            } else if (WavCab < mea[6]) {
                betaab = 0.6f;
            } else if (WavCab < mea[7]) {
                betaab = 0.4f;
            } else if (WavCab < mea[8]) {
                betaab = 0.2f;    // + 2 sigma
            } else if (WavCab < mea[9]) {
                betaab = 0.1f;
            } else {
                betaab = 0.0f;
            }

            float kreduc1 = 1.f;
            float kreduc2 = 1.f;
            int ii = co / W_ab;
            int jj = co - ii * W_ab;

            //    cp.protab = 0.f;// always disabled provisory...
            if (cp.protab > 0.f) {
                if (useChannelA) {
                    if ((labco->a[ii * 2][jj * 2] > arefmoins1) && (labco->a[ii * 2][jj * 2] < arefplus1)) {
                        kreduc1 = 0.5f * protec;

                        if ((labco->a[ii * 2][jj * 2] > 0.8f * arefmoins1) && (labco->a[ii * 2][jj * 2] < 0.8f * arefplus1)) {
                            kreduc1 = protec;
                        }
                    }

                } else {
                    if ((labco->b[ii * 2][jj * 2] > brefmoins1) && (labco->b[ii * 2][jj * 2] < brefplus1)) {
                        kreduc1 = 0.5f * protec;

                        if ((labco->b[ii * 2][jj * 2] > 0.8f * brefmoins1) && (labco->b[ii * 2][jj * 2] < 0.8f * brefplus1)) {
                            kreduc1 = protec;
                        }
                    }
                }

                if (useChannelA) {
                    if ((labco->a[ii * 2][jj * 2] > arefmoins2) && (labco->a[ii * 2][jj * 2] < arefplus2)) {
                        kreduc2 = 0.5f * protec;

                        if ((labco->a[ii * 2][jj * 2] > 0.8f * arefmoins2) && (labco->a[ii * 2][jj * 2] < 0.8f * arefplus2)) {
                            kreduc2 = protec;
                        }

                    }
                } else {
                    if ((labco->b[ii * 2][jj * 2] > brefmoins2) && (labco->b[ii * 2][jj * 2] < brefplus2)) {
                        kreduc2 = 0.5f * protec;

                        if ((labco->b[ii * 2][jj * 2] > brefmoins2) && (labco->b[ii * 2][jj * 2] < brefplus2)) {
                            kreduc2 = protec;
                        }
                    }
                }

            }


            // printf("pa1=%f pa2=%f\n", kreduc1, kredu2);


            float beta = (1024.f + 50.f * mulOpacity * betaab * kreduc1 * kreduc2) / 1024.f ;

            WavCoeffs_ab[dir][co] *= beta;
        }

    }

    if (waOpacityCurveW) {
        cp.opaW = true;
    }

    if (cp.bam  && cp.diag) {
        if (cp.opaW && cp.BAmet == 2) {
            int iteration = cp.ite;
            int itplus = 7 + iteration;
            int itmoins = 7 - iteration;
            int med = maxlvl / 2;
            int it;

            if (level < med) {
                it = itmoins;
            } else if (level == med) {
                it = 7;
            } else { /*if (level > med)*/
                it = itplus;
            }

            for (int j = 0; j < it; j++) {
                //float bal = cp.balan;//-100 +100
                float kba = 1.f;

                //  if (dir <3) kba= 1.f + bal/600.f;
                //  if (dir==3) kba = 1.f - bal/300.f;
                for (int i = 0; i < W_ab * H_ab; i++) {
                    int ii = i / W_ab;
                    int jj = i - ii * W_ab;
                    float LL100 = labco->L[ii * 2][jj * 2] / 327.68f;
                    float k1 = 0.3f * (waOpacityCurveW[6.f * LL100] - 0.5f); //k1 between 0 and 0.5    0.5==> 1/6=0.16
                    float k2 = k1 * 2.f;

                    if (dir < 3) {
                        kba = 1.f + k1;
                    }

                    if (dir == 3) {
                        kba = 1.f - k2;
                    }

                    WavCoeffs_ab[dir][i] *= (kba);
                }
            }
        }

        if (cp.BAmet == 1) {
            int iteration = cp.ite;
            int itplus = 7 + iteration;
            int itmoins = 7 - iteration;
            int med = maxlvl / 2;
            int it;

            if (level < med) {
                it = itmoins;
            } else if (level == med) {
                it = 7;
            } else { /*if (level > med)*/
                it = itplus;
            }

            for (int j = 0; j < it; j++) {
                float bal = cp.balan;//-100 +100
                float kba = 1.f;

                //  if (dir <3) kba= 1.f + bal/600.f;
                //  if (dir==3) kba = 1.f - bal/300.f;
                for (int i = 0; i < W_ab * H_ab; i++) {
                    int ii = i / W_ab;
                    int jj = i - ii * W_ab;
                    float k1 = 600.f;
                    float k2 = 300.f;
                    float LL100 = labco->L[ii * 2][jj * 2] / 327.68f;
                    constexpr float aa = 4970.f;
                    constexpr float bb = -397000.f;
                    constexpr float b0 = 100000.f;
                    constexpr float a0 = -4970.f;

                    if (LL100 > 80.f) {
                        k1 = aa * LL100 + bb;
                        k2 = 0.5f * k1;
                    }

                    if (LL100 < 20.f) {
                        k1 = a0 * LL100 + b0;
                        k2 = 0.5f * k1;
                    }

                    //k1=600.f;
                    //k2=300.f;
                    //k1=0.3f*(waOpacityCurveW[6.f*LL100]-0.5f);//k1 between 0 and 0.5    0.5==> 1/6=0.16
                    //k2=k1*2.f;
                    if (dir < 3) {
                        kba = 1.f + bal / k1;
                    }

                    if (dir == 3) {
                        kba = 1.f - bal / k2;
                    }

                    WavCoeffs_ab[dir][i] *= (kba);
                }
            }
        }

    }

    // to see each level of wavelet ...level from 0 to 8
    int choicelevel = params->wavelet.Lmethod - 1;
    choicelevel = choicelevel == -1 ? 4 : choicelevel;
    int choiceClevel = 0;

    if (params->wavelet.CLmethod == "one") {
        choiceClevel = 0;
    } else if (params->wavelet.CLmethod == "inf") {
        choiceClevel = 1;
    } else if (params->wavelet.CLmethod == "sup") {
        choiceClevel = 2;
    } else if (params->wavelet.CLmethod == "all") {
        choiceClevel = 3;
    }

    int choiceDir = 0;

    if (params->wavelet.Dirmethod == "one") {
        choiceDir = 1;
    } else if (params->wavelet.Dirmethod == "two") {
        choiceDir = 2;
    } else if (params->wavelet.Dirmethod == "thr") {
        choiceDir = 3;
    } else if (params->wavelet.Dirmethod == "all") {
        choiceDir = 0;
    }

    int dir1 = (choiceDir == 2) ? 1 : 2;
    int dir2 = (choiceDir == 3) ? 1 : 3;

    if (choiceClevel < 3) { // not all levels visible, paint residual
        if (level == 0) {
            if (cp.backm != 2) { // nothing to change when residual is used as background
                for (int i = 0; i < W_ab * H_ab; i++) {
                    WavCoeffs_ab0[i] = 0.f;
                }
            }
        }
    }

    if (choiceClevel == 0) { // Only one level
        if (choiceDir == 0) { // All directions
            if (level != choicelevel) { // zero all for the levels != choicelevel
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_ab * H_ab; i++) {
                        WavCoeffs_ab[d][i] = 0.f;
                    }
                }
            }
        } else { // zero the unwanted directions for level == choicelevel
            if (choicelevel >= cp.maxilev) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_ab * H_ab; i++) {
                        WavCoeffs_ab[d][i] = 0.f;
                    }
                }
            } else if (level != choicelevel) { // zero all for the levels != choicelevel
                for (int i = 0; i < W_ab * H_ab; i++) {
                    WavCoeffs_ab[dir1][i] = WavCoeffs_ab[dir2][i] = 0.f;
                }
            }
        }
    } else if (choiceClevel == 1) { // Only below level
        if (choiceDir == 0) { // All directions
            if (level > choicelevel) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_ab * H_ab; i++) {
                        WavCoeffs_ab[d][i] = 0.f;
                    }
                }
            }
        } else { // zero the unwanted directions for level >= choicelevel
            if (level > choicelevel) {
                for (int i = 0; i < W_ab * H_ab; i++) {
                    WavCoeffs_ab[dir1][i] = WavCoeffs_ab[dir2][i] = 0.f;
                }
            }
        }
    } else if (choiceClevel == 2) { // Only above level
        if (choiceDir == 0) { // All directions
            if (level <= choicelevel) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_ab * H_ab; i++) {
                        WavCoeffs_ab[d][i] = 0.f;
                    }
                }
            }
        } else { // zero the unwanted directions for level >= choicelevel
            if (choicelevel >= cp.maxilev) {
                for (int d = 1; d < 4; d++) {
                    for (int i = 0; i < W_ab * H_ab; i++) {
                        WavCoeffs_ab[d][i] = 0.f;
                    }
                }
            } else if (level <= choicelevel) {
                for (int i = 0; i < W_ab * H_ab; i++) {
                    WavCoeffs_ab[dir1][i] = WavCoeffs_ab[dir2][i] = 0.f;
                }
            }
        }
    }
}

}
