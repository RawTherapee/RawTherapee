/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  2016 - 2024 Jacques Desmis <jdesmis@gmail.com>
 *  2016 - 2020 Ingo Weyrich <heckflosse@i-weyrich.de>
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
#pragma once

#include "rt_math.h"
#include "LUT.h"
#include "array2D.h"
#include "labimage.h"
#include "curves.h"

namespace rtengine {

// Forward declarations
class LabImage;
class Imagefloat;
class wavelet_decomposition;

namespace procparams {
    struct LocallabParams;
}

//------------------------------------------------------------------------------
// Constants used across locallab tools
//------------------------------------------------------------------------------
namespace locallab {

constexpr int limscope = 80;
constexpr int mSPsharp = 39;      // minimum size Spot Sharp due to buildblendmask
constexpr int mSPsharpCS = 150;   // minimum size Spot Sharp due to buildblendmask with capture sharpening
constexpr int mSPwav = 32;        // minimum size Spot Wavelet
constexpr int mDEN = 128;         // minimum size Spot Denoise
constexpr int mSP = 5;            // minimum size Spot
constexpr float MAXSCOPE = 1.25f;
constexpr float MINSCOPE = 0.025f;
constexpr int TS = 64;            // Tile size
constexpr float epsilonw = 0.001f / (TS * TS);  // tolerance
constexpr int offset = 25;        // shift between tiles

inline constexpr float clipLoc(float x) {
    return x;
}

inline constexpr float clipDE(float x) {
    return LIM(x, 0.3f, 1.f);
}

inline constexpr float clipR(float x) {
    return LIM(x, 0.f, 65535.f);
}

inline constexpr float clipC(float x) {
    return LIM(x, -100000.f, 100000.f);
}

inline constexpr float clipChro(float x) {
    return LIM(x, 0.f, 300.f);
}

} // namespace locallab

//------------------------------------------------------------------------------
// local_params structure - holds all parameters for local adjustments
//------------------------------------------------------------------------------
struct local_params {
    float yc, xc;
    float ycent, xcent;
    float lx, ly;
    float lxL, lyT;
    float transweak;
    float transgrad;
    float iterat;
    float balance;
    float balanceh;
    int colorde;
    float cir;
    bool recur;
    float thr;
    float stru;
    int chro, cont, sens, sensh, senscb, sensbn, senstm, sensex, sensexclu, sensden, senslc, senssf, senshs, senscolor;
    float reparden;
    float repartm;
    float clarityml;
    float contresid;
    bool deltaem;
    float struco;
    float strengrid;
    float struexc;
    float blendmacol;
    float radmacol;
    float chromacol;
    float gammacol;
    float slomacol;
    float blendmalc;
    float radmalc;
    float chromalc;
    float radmaexp;
    float chromaexp;
    float gammaexp;
    float slomaexp;
    float strmaexp;
    float angmaexp;
    float str_mas;
    float ang_mas;
    float feather_mas;
    float strexp;
    float angexp;
    float featherexp;
    float strSH;
    float angSH;
    float featherSH;
    float strcol;
    float strcolab;
    float strcolh;
    float angcol;
    float feathcol;
    float strvib;
    float strvibab;
    float strvibh;
    float angvib;
    float feathervib;
    float angwav;
    float featherwav;
    float strwav;
    float blendmaL;
    float radmaL;
    float chromaL;

    float strengthw;
    float radiusw;
    float detailw;
    float gradw;
    float tloww;
    float thigw;
    float edgw;
    float basew;

    float anglog;
    float featherlog;
    float strlog;
    float softradiusexp;
    float softradiuscol;
    float softradiuscb;
    float softradiusret;
    float softradiustm;
    float blendmaexp;
    float radmaSH;
    float blendmaSH;
    float chromaSH;
    float gammaSH;
    float slomaSH;
    float radmavib;
    float blendmavib;
    float chromavib;
    float gammavib;
    float slomavib;
    float radmacb;
    float blendmacb;
    float chromacbm;
    float gammacb;
    float slomacb;
    float radmatm;
    float blendmatm;
    float chromatm;
    float gammatm;
    float slomatm;

    float radmabl;
    float blendmabl;
    float chromabl;
    float gammabl;
    float slomabl;

    float struexp;
    float blurexp;
    float blurcol;
    float blurcolmask;
    float contcolmask;
    float blurSH;
    float ligh;
    float gamc;
    float gamlc;
    float gamex;
    float lowA, lowB, highA, highB;
    float lowBmerg, highBmerg, lowAmerg, highAmerg;
    int shamo, shdamp, shiter, senssha, sensv;
    float neig;
    float strng;
    float lap;
    float lcamount;
    double shrad;
    double shblurr;
    double rad;
    double stren;
    int it;
    int guidb;
    float strbl;
    float epsb;
    float trans;
    float feath;
    int dehaze;
    int dehazeSaturation;
    int depth;
    bool inv;
    bool invex;
    bool invsh;
    bool curvact;
    bool invrad;
    bool invret;
    bool equret;
    bool equtm;
    bool invshar;
    bool actsp;
    bool ftwlc;
    bool ftwreti;
    float str;
    int qualmet;
    int qualcurvemet;
    int gridmet;
    bool prevdE;
    int showmaskcolmet;
    int showmaskcolmetinv;
    int showmaskexpmet;
    int showmaskexpmetinv;
    int showmaskSHmet;
    int showmaskSHmetinv;
    int showmaskvibmet;
    int showmasklcmet;
    int showmasksharmet;
    int showmaskcbmet;
    int showmaskretimet;
    int showmasksoftmet;
    int showmasktmmet;
    int showmaskblmet;
    int showmasklogmet;
    int showmask_met;
    int showmaskciemet;
    bool processwa;
    bool fftbl;
    float laplacexp;
    float balanexp;
    float linear;
    int fullim;
    int expmet;
    int softmet;
    int blurmet;
    int blmet;
    bool invmaskd;
    bool invmask;
    int smasktyp;
    int chromet;
    int quamet;
    int shmeth;
    int medmet;
    int locmet;
    float noiself;
    float noiself0;
    float noiself2;
    float noiseldetail;
    int detailthr;
    float recothr;
    float lowthr;
    float higthr;
    float recothrd;
    float lowthrd;
    float midthrd;
    float midthrdch;
    float higthrd;
    float decayd;
    float recothrc;
    float lowthrc;
    float higthrc;
    float decayc;
    float recothre;
    float lowthre;
    float higthre;
    float decaye;
    float recothrv;
    float lowthrv;
    float higthrv;
    float decayv;
    float recothrcb;
    float lowthrcb;
    float higthrcb;
    float decaycb;
    float recothrt;
    float lowthrt;
    float higthrt;
    float decayt;
    float recothrw;
    float lowthrw;
    float higthrw;
    float decayw;
    float recothrr;
    float lowthrr;
    float higthrr;
    float decayr;
    float recothrs;
    float lowthrs;
    float higthrs;
    float decays;
    float recothrl;
    float lowthrl;
    float higthrl;
    float decayl;
    float recothrcie;
    float lowthrcie;
    float higthrcie;
    float decaycie;
    float blurciemask;
    float contciemask;
    bool islogcie;
    bool issmoothcie;
    bool issmoothghs;
    float issmoothmich;

    float maxdataghs;
    float ghshp;
    int noiselequal;
    float noisechrodetail;
    float bilat;
    int nlstr;
    int nldet;
    int nlpat;
    int nlrad;
    int nliter;
    float nlgam;
    float noisegam;
    float noiselc;
    float noiselc4;
    float noiselc5;
    float noiselc6;
    float noisecf;
    float noisecc;
    float mulloc[6];
    int mullocsh[6];
    int detailsh;
    int whitescie;
    int midtcie;
    int midtmet;
    double tePivot;
    float threshol;
    float chromacb;
    float strengt;
    float gamm;
    float esto;
    float scalt;
    float rewe;
    float amo;
    bool colorena;
    bool blurena;
    bool tonemapena;
    bool retiena;
    bool sharpena;
    bool lcena;
    bool sfena;
    bool cbdlena;
    bool denoiena;
    bool wavcurvedenoi;
    bool expvib;
    bool exposena;
    bool hsena;
    bool vibena;
    bool logena;
    bool islocal;
    bool maskena;
    bool cieena;
    bool cut_past;
    float past;
    float satur;
    int blac;
    int shcomp;
    int shadex;
    int hlcomp;
    int hlcompthr;
    float expcomp;
    float expchroma;
    int excmet;
    int mergemet;
    int mergecolMethod;
    float opacol;
    int war;
    float adjch;
    int shapmet;
    int edgwmet;
    int neiwmet;
    bool enaColorMask;
    bool fftColorMask;
    bool enaColorMaskinv;
    bool enaExpMask;
    bool enaExpMaskinv;
    bool enaSHMask;
    bool enaSHMaskinv;
    bool enavibMask;
    bool enalcMask;
    bool enasharMask;
    bool enacbMask;
    bool enaretiMask;
    bool enaretiMasktmap;
    bool enatmMask;
    bool enablMask;
    bool enaLMask;
    bool ena_Mask;
    bool enacieMask;
    int highlihs;
    int shadowhs;
    int radiushs;
    int hltonalhs;
    int shtonalhs;
    int scalereti;
    float sourcegray;
    float targetgray;
    float blackev;
    float whiteev;
    float sourcegraycie;
    float targetgraycie;
    float blackevjz;
    float whiteevjz;
    float detail;
    float detailcie;
    float strgradcie;
    float anggradcie;
    float feathercie;
    bool satcie;
    bool satlog;
    int sensilog;
    int sensicie;
    int sensimas;
    bool Autogray;
    bool autocompute;
    float baselog;
    bool wavgradl;
    bool edgwena;
    bool lip3;
    int daubLen;
    float sigmadr;
    float sigmabl;
    float sigmaed;
    float sigmalc;
    float offslc;
    float sigmalc2;
    float residsha;
    float residshathr;
    float residhi;
    float residhithr;
    float residgam;
    float residslop;
    bool avoidneg;
    bool blwh;
    bool fftma;
    float blurma;
    float contma;
    bool activspot;
    float thrlow;
    float thrhigh;
    bool usemask;
    float lnoiselow;
    float radmacie;
    float blendmacie;
    float chromacie;
    float denoichmask;
    float mLjz;
    float mCjz;
    float softrjz;
    bool fftcieMask;
    float comprlo;
    float comprlocie;
    int moka;
    int sursouci;
    int smoothciem;
    float smoothtrc;

    float denocontra;
    float denorati;
    float denomas;
    bool contrsho;
    bool denoAutocontr;
    bool enacontr;
};

//------------------------------------------------------------------------------
// Function declarations for calcLocalParams
//------------------------------------------------------------------------------
void calcLocalParams(int sp, int oW, int oH, const procparams::LocallabParams& locallab,
                     struct local_params& lp, bool prevDeltaE,
                     int llColorMask, int llColorMaskinv, int llExpMask, int llExpMaskinv,
                     int llSHMask, int llSHMaskinv, int llvibMask, int lllcMask,
                     int llsharMask, int llcbMask, int llretiMask, int llsoftMask,
                     int lltmMask, int llblMask, int lllogMask, int ll_Mask, int llcieMask,
                     const LocwavCurve& locwavCurveden, bool locwavdenutili);

//------------------------------------------------------------------------------
// Common utility functions used by multiple tools
//------------------------------------------------------------------------------

// Sobel/Canny edge detection
void SobelCannyLuma(float **sobelL, float **luma, int bfw, int bfh, float radius);

// Calculate gamma lookup table
void calcGammaLut(double gamma, double ts, LUTf &gammaLut);

// DeltaE calculation for Laplacian
void deltaEforLaplace(float *dE, float lap, int bfw, int bfh, LabImage* bufexporig,
                      float hueref, float chromaref, float lumaref);

// Local factor calculations for ellipse and rectangle shapes
float calcLocalFactor(float lox, float loy, float lcx, float dx, float lcy, float dy,
                      float ach, float gradient);
float calcLocalFactorrect(float lox, float loy, float lcx, float dx, float lcy, float dy,
                          float ach, float gradient);

// DeltaE reduction calculation
float calcreducdE(float dE, float maxdE, float mindE, float maxdElim, float mindElim,
                  float iterat, int limscope, int scope);

// Balance deltaE calculation
float balancedeltaE(float kL);

// Light calculation helpers
float calclight(float lum, const LUTf &lightCurveloc);
float calclightinv(float lum, float koef, const LUTf &lightCurveloc);

// Gamma log functions
float igammalog(float x, float p, float s, float g2, float g4);
float gammalog(float x, float p, float s, float g3, float g4);

// Blend mode functions
float softlig(float a, float b, float minc, float maxc);
float softlig2(float a, float b);
float softlig3(float a, float b);
float overlay(float a, float b, float minc, float maxc);

inline constexpr float colburn(float a, float b) {
    return b == 0.f ? 0.f : 1.f - rtengine::min(1.f, (1.f - a) / b);
}

inline constexpr float coldodge(float a, float b) {
    return b == 1.f ? 1.f : rtengine::min(1.f, a / (1.f - b));
}

inline constexpr float screen(float a, float b, float maxc) {
    return 1.f - (1.f - a) * (maxc - b);
}

inline constexpr float exclusion(float a, float b) {
    return a + b - 2.f * a * b;
}

// Clamp function
inline float clamp(float x, float lo, float hi) {
    return fmax(fmin(x, hi), lo);
}

// Michaelis-Menten curve
inline float mm_curve(double x, double S, double K_eff) {
    return (S * x) / (fmax(K_eff, 1e-6) + x);
}

// Calculate difference between gamma sRGB and gamma LAB
void calcdif(float lmr, float &lmrc);

} // namespace rtengine
