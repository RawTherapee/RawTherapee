/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "locallab.h"

#include "serdes.h"

#include "diagonalcurvetypes.h"
#include "flatcurvetypes.h"

#include "rtgui/options.h"
#include "rtgui/paramsedited.h"

#include <glibmm/keyfile.h>

#include <cmath>
#include <string>

namespace rtengine {
namespace procparams {

namespace {

struct LoadUtil {
    const Glib::KeyFile& keyFile;

    LocallabParams::LocallabSpot& spot;

    ParamsEdited* pedited;
    LocallabParamsEdited::LocallabSpotEdited& spotEdited;
    const std::string index_str;
    const int ppVersion;

    LoadUtil(const Glib::KeyFile& kf,
             LocallabParams::LocallabSpot& sp,
             ParamsEdited* pe,
             LocallabParamsEdited::LocallabSpotEdited& spe,
             const std::string& str,
             int ppVer)
        : keyFile(kf),
          spot(sp),
          pedited(pe),
          spotEdited(spe),
          index_str(str),
          ppVersion(ppVer)
    {
    }

    void controlSpotSettings();
    void colorLight();
    void exposure();
    void shadowHighlight();
    void vibrance();
    void softLight();
    void blurNoise();
    void toneMapping();
    void retinex();
    void sharpening();
    void localContrast();
    void contrastByDetailLevels();
    void logEncoding();
    void mask();
    void ciecam();
};

struct SaveUtil {
    Glib::KeyFile& keyFile;

    const LocallabParams::LocallabSpot& spot;

    const ParamsEdited* pedited;
    const LocallabParamsEdited::LocallabSpotEdited* const spot_edited = nullptr;
    const std::string index_str;

    SaveUtil(Glib::KeyFile& kf,
             const LocallabParams::LocallabSpot& sp,
             const ParamsEdited* pe,
             const LocallabParamsEdited::LocallabSpotEdited* const spe,
             const std::string& str)
        : keyFile(kf), spot(sp), pedited(pe), spot_edited(spe), index_str(str)
    {
    }

    void controlSpotSettings();
    void colorLight();
    void exposure();
    void shadowHighlight();
    void vibrance();
    void softLight();
    void blurNoise();
    void toneMapping();
    void retinex();
    void sharpening();
    void localContrast();
    void contrastByDetailLevels();
    void logEncoding();
    void mask();
    void ciecam();
};

}  // namespace

LocallabParams::LocallabSpot::LocallabSpot() :
    // Control spot settings
    name(""),
    isvisible(true),
    prevMethod("hide"),
    shape("ELI"),
    spotMethod("norm"),
    wavMethod("D6"),
    sensiexclu(12),
    structexclu(0),
    struc(4.0),
    shapeMethod("IND"),
    avoidgamutMethod("MUNS"),
    loc{150, 150, 150, 150},
    centerX(0),
    centerY(0),
    circrad(18.),
    qualityMethod("enh"),
    complexMethod("mod"),
    transit(60.),
    feather(25.),
    thresh(2.0),
    iter(2.0),
    balan(1.0),
    balanh(1.0),
    colorde(5.0),
    colorscope(30.0),
    avoidrad(0.),
    transitweak(1.0),
    transitgrad(0.0),
    hishow(App::get().options().complexity != 2),
    activ(true),
    avoidneg(true),
    blwh(false),
    recurs(false),
    laplac(true),
    deltae(true),
    shortc(false),
    savrest(false),
    scopemask(60),
    denoichmask(0.),
    lumask(10),
    // Color & Light
    visicolor(false),
    expcolor(false),
    complexcolor(2),
    curvactiv(false),
    lightness(0),
    reparcol(100.),
    gamc(1.),
    contrast(0),
    chroma(0),
    labgridALow(0.0),
    labgridBLow(0.0),
    labgridAHigh(0.0),
    labgridBHigh(0.0),
    labgridALowmerg(0.0),
    labgridBLowmerg(0.0),
    labgridAHighmerg(-3500.0),
    labgridBHighmerg(-4600.0),
    strengthgrid(30),
    sensi(30),
    structcol(0),
    strcol(0.),
    strcolab(0.),
    strcolh(0.),
    angcol(0.),
    feathercol(25.),
    blurcolde(5),
    blurcol(0.2),
    contcol(0.),
    blendmaskcol(0),
    radmaskcol(0.0),
    chromaskcol(0.0),
    gammaskcol(1.0),
    slomaskcol(0.0),
    shadmaskcol(0),
    strumaskcol(0.),
    lapmaskcol(0.0),
    qualitycurveMethod("none"),
    gridMethod("one"),
    merMethod("mone"),
    toneMethod("fou"),
    mergecolMethod("one"),
    llcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    lccurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    cccurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    clcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    rgbcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    LHcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    HHcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    CHcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    invers(false),
    special(false),
    toolcol(false),
    enaColorMask(false),
    fftColorMask(true),
    CCmaskcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    LLmaskcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    HHmaskcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    HHhmaskcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        0.50,
        0.5,
        0.35,
        0.35,
        1.00,
        0.5,
        0.35,
        0.35,
    },
    softradiuscol(0.0),
    opacol(60.0),
    mercol(18.0),
    merlucol(32.0),
    conthrcol(0.0),
    Lmaskcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    LLmaskcolcurvewav{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    csthresholdcol(0, 0, 6, 5, false),
    recothresc(1.),
    lowthresc(12.),
    higthresc(85.),
    decayc(2.),
    // Exposure
    visiexpose(false),
    expexpose(false),
    complexexpose(0),
    expcomp(0.0),
    hlcompr(0),
    hlcomprthresh(0),
    black(0),
    shadex(0),
    shcompr(50),
    expchroma(5),
    sensiex(60),
    structexp(0),
    blurexpde(5),
    gamex(1.),
    strexp(0.),
    angexp(0.),
    featherexp(25.),
    excurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    norm(true),
    inversex(false),
    enaExpMask(false),
    enaExpMaskaft(false),
    CCmaskexpcurve{
        static_cast<double>(FCT_MinMaxCPoints),0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskexpcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskexpcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    blendmaskexp(0),
    radmaskexp(0.0),
    chromaskexp(0.0),
    gammaskexp(1.0),
    slomaskexp(0.0),
    lapmaskexp(0.0),
    strmaskexp(0.0),
    angmaskexp(0.0),
    softradiusexp(0.0),
    Lmaskexpcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    expMethod("std"),
    exnoiseMethod("none"),
    laplacexp(0.0),
    reparexp(100.0),
    balanexp(1.0),
    linear(0.05),
    gamm(0.4),
    fatamount(1.0),
    fatdetail(40.0),
    fatsatur(false),
    fatanchor(50.0),
    fatlevel(1.),
    recothrese(1.),
    lowthrese(12.),
    higthrese(85.),
    decaye(2.),
    // Shadow highlight
    visishadhigh(false),
    expshadhigh(false),
    complexshadhigh(0),
    shMethod("micha"),
    ghsMethod("rgbstd"),
    ghsMatmet("none"), // I think it's better to leave it as 'none' and let the user choose their preferred mode.
    ghsMode("ghs"),
    ghs_D(0.001),
    ghs_mtfstr(0.25),
    ghs_slope(9.03296),
    ghs_chro(0.0),
    ghs_B(0.),
    ghs_SP(0.015),//initialized with a low value to avoid zero
    SPAutoRadius(false), //auto Symmetry point - I think it's better to put 'false' and leave the initiative to the user.
    ghs_LP(0.),
    ghs_HP(1.),
    ghs_LC(0.),
    ghs_MID(0.),
    ghs_BLP(0.),
    ghs_HLP(1.),
    ghs_autobw(false),
    ghs_agx(true),
    ghs_mtf(false),
    ghs_smooth(false),
    ghs_inv(false),
    mich_exp(0.),
    mich_spar(1.),
    mich_kpar(0.18),
    mich_sat(1.15),
    mich_out(1.),
    mich_black(false),
    mich_white(false),
    mich_high(0.),
    mich_jdx(false),
    mich_mtf(0.),
    multsh{0, 0, 0, 0, 0, 0},
    highlights(0),
    h_tonalwidth(70),
    shadows(0),
    s_tonalwidth(30),
    sh_radius(40),
    sensihs(30),
    enaSHMask(false),
    CCmaskSHcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskSHcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskSHcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    blendmaskSH(0),
    radmaskSH(0.0),
    blurSHde(5),
    strSH(0.),
    angSH(0.),
    featherSH(25.),
    inverssh(false),
    chromaskSH(0.0),
    gammaskSH(1.0),
    slomaskSH(0.0),
    lapmaskSH(0.0),
    detailSH(0),
    tePivot(0.),
    reparsh(100.),
    LmaskSHcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    fatamountSH(1.0),
    fatanchorSH(50.0),
    gamSH(2.4),
    sloSH(12.92),
    recothress(1.),
    lowthress(12.),
    higthress(85.),
    decays(2.),
    // Vibrance
    visivibrance(false),
    expvibrance(false),
    complexvibrance(0),
    saturated(0),
    pastels(0),
    vibgam(1.0),
    warm(0),
    psthreshold({0, 75, false}),
    protectskins(false),
    avoidcolorshift(true),
    pastsattog(true),
    sensiv(30),
    skintonescurve{
        static_cast<double>(DCT_Linear)
    },
    CCmaskvibcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskvibcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskvibcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    enavibMask(false),
    blendmaskvib(0),
    radmaskvib(0.0),
    chromaskvib(0.0),
    gammaskvib(1.0),
    slomaskvib(0.0),
    lapmaskvib(0.0),
    strvib(0.0),
    strvibab(0.0),
    strvibh(0.0),
    angvib(1.0),
    feathervib(1.0),
    Lmaskvibcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothresv(1.),
    lowthresv(12.),
    higthresv(85.),
    decayv(2.),
    // Soft Light
    visisoft(false),
    expsoft(false),
    complexsoft(0),
    streng(0),
    sensisf(30),
    laplace(25.),
    softMethod("soft"),
    // Blur & Noise
    visiblur(false),
    expblur(false),
    complexblur(0),
    radius(1.5),
    strength(0),
    sensibn(40),
    itera(1),
    guidbl(0),
    strbl(50),
    recothres(1.),
    lowthres(12.),
    higthres(85.),
    recothresd(1.),
    lowthresd(12.),
    midthresd(0.),
    midthresdch(0.),
    higthresd(85.),
    decayd(2.),
    isogr(400),
    strengr(0),
    scalegr(100),
    divgr(1.),
    epsbl(0),
    blMethod("blur"),
    chroMethod("lum"),
    quamethod("none"),
    blurMethod("norm"),
    medMethod("33"),
    usemask(false),
    invmaskd(false),
    invmask(false),
    levelthr(85.),
    lnoiselow(1.),
    levelthrlow(12.),
    activlum(true),
    madlsav{100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f},  
    noiselumf(0.),
    noiselumf0(0.),
    noiselumf2(0.),
    noiselumc(0.),
    noiselumdetail(50.),
    noiselequal(7),
    noisegam(1.),
    noisechrof(0.),
    noisechroc(0.),
    noisechrodetail(50.),
    adjblur(0),
    bilateral(0),
    nlstr(0),
    nldet(50),
    nlpat(2),
    nlrad(5),
    nlgam(3.),
    nliter(1),
    sensiden(60),
    reparden(100.),
    detailthr(50),
    locwavcurveden{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.09,
        0.35,
        0.,
        0.33,
        0.17,
        0.33,
        0.35,
        1.0,
        0.03,
        0.35,
        0.35,
    },
    locwavcurvehue{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    locwavcurvehuecont{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    showmaskblMethodtyp("nois"),
    CCmaskblcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskblcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskblcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    enablMask(false),
    fftwbl(false),
    invbl(false),
    toolbl(false),
    blendmaskbl(0),
    radmaskbl(0.0),
    chromaskbl(0.0),
    gammaskbl(1.0),
    slomaskbl(0.0),
    lapmaskbl(0.0),
    shadmaskbl(0),
    shadmaskblsha(0),
    strumaskbl(0.),
    Lmaskblcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    LLmaskblcurvewav{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    csthresholdblur(0, 0, 6, 5, false),
    denocontrast(10.),
    denoAutocontrast(true),
    contrshow(false),
    lockmadl(false),
    madllock(false),
    enacontrast(true),
    denoratio(95),
    denomask(30.),
    // Tone Mapping
    visitonemap(false),
    exptonemap(false),
    complextonemap(0),
    stren(0.5),
    gamma(1.0),
    estop(1.4),
    scaltm(1.0),
    repartm(100.0),
    rewei(0),
    satur(0.),
    sensitm(60),
    softradiustm(0.0),
    amount(95.),
    equiltm(true),
    CCmasktmcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmasktmcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmasktmcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    enatmMask(false),
    enatmMaskaft(false),
    blendmasktm(0),
    radmasktm(0.0),
    chromasktm(0.0),
    gammasktm(1.0),
    slomasktm(0.0),
    lapmasktm(0.0),
    Lmasktmcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothrest(1.),
    lowthrest(12.),
    higthrest(85.),
    decayt(2.),
    // Retinex
    visireti(false),
    expreti(false),
    complexreti(0),
    retinexMethod("high"),
    str(0.),
    chrrt(0.0),
    neigh(50.0),
    vart(150.0),
    offs(0.0),
    dehaz(0),
    depth(25),
    sensih(60),
    localTgaincurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.12,
        0.35,
        0.35,
        0.70,
        0.50,
        0.35,
        0.35,
        1.00,
        0.12,
        0.35,
        0.35,
    },
    localTtranscurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.5,
        0.5,
        0.35,
        0.35,
        1.00,
        0.50,
        0.35,
        0.35,
    },
    inversret(false),
    equilret(false),
    loglin(true),
    dehazeSaturation(50.0),
    dehazeblack(0.0),
    softradiusret(40.0),
    CCmaskreticurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskreticurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskreticurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    enaretiMask(false),
    enaretiMasktmap(true),
    blendmaskreti(0),
    radmaskreti(0.0),
    chromaskreti(0.0),
    gammaskreti(1.0),
    slomaskreti(0.0),
    lapmaskreti(0.0),
    scalereti(2.0),
    darkness(2.0),
    lightnessreti(1.0),
    limd(8.0),
    cliptm(1.0),
    fftwreti(false),
    Lmaskreticurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothresr(1.),
    lowthresr(12.),
    higthresr(85.),
    decayr(2.),
    // Sharpening
    visisharp(false),
    expsharp(false),
    complexsharp(0),
    sharcontrast(20),
    deconvAutoshar(true),
    sharradius(0.75),
    sharamount(100),
    shardamping(0),
    shariter(30),
    sharblur(0.2),
    shargam(1.0),
    sensisha(40),
    inverssha(false),
    sharshow(false),
    itercheck(true),
    methodcap("cap"),
    capradius(0.75),
    deconvAutoRadius(false),
    deconvCoBoost(0.),
    deconvCoProt(50.),
    deconvCoLat(25.),
    deconvCogam(1.),
    reparsha(100.),
    // Local Contrast
    visicontrast(false),
    expcontrast(false),
    complexcontrast(0),
    lcradius(80),
    lcamount(0.0),
    lcdarkness(1.0),
    lclightness(1.0),
    sigmalc(1.0),
    offslc(1.0),
    levelwav(4),
    residcont(0.0),
    residsha(0.0),
    residshathr(30.0),
    residhi(0.0),
    residhithr(70.0),
    gamlc(1.0),
    residgam(2.40),
    residslop(12.94),
    residblur(0.0),
    levelblur(0.0),
    sigmabl(1.0),
    residchro(0.0),
    residcomp(0.0),
    sigma(1.0),
    offset(1.0),
    sigmadr(1.0),
    threswav(1.4),
    chromalev(1.0),
    chromablu(0.0),
    sigmadc(1.0),
    deltad(0.0),
    fatres(0.0),
    clarilres(0.0),
    claricres(0.0),
    clarisoft(1.0),
    sigmalc2(1.0),
    strwav(0.0),
    angwav(0.0),
    featherwav(25.0),
    strengthw(0.0),
    sigmaed(1.0),
    radiusw(15.0),
    detailw(10.0),
    gradw(90.0),
    tloww(20.0),
    thigw(0.0),
    edgw(60.0),
    basew(10.0),
    sensilc(60),
    reparw(100.),
    fftwlc(false),
    blurlc(true),
    wavblur(false),
    wavedg(false),
    waveshow(false),
    wavcont(false),
    wavcomp(false),
    wavgradl(false),
    wavcompre(false),
    origlc(false),
    processwav(false),
    localcontMethod("wav"),
    localedgMethod("thr"),
    localneiMethod("low"),
    locwavcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    csthreshold(0, 0, 7, 5, false),
    loclevwavcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.0,
        0.0,
        0.35,
        0.5,
        0.,
        0.35,
        0.35,
        1.0,
        0.0,
        0.35,
        0.35,
    },
    locconwavcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    loccompwavcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.00,
        0.35,
        0.35,
        0.00,
        0.35,
        0.75,
        0.35,
        0.35,
        0.60,
        0.75,
        0.35,
        0.35,
        1.00,
        0.35,
        0.00,
        0.00,
    },
    loccomprewavcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.75,
        0.35,
        0.35,
        1.,
        0.75,
        0.35,
        0.35,
    },
    locedgwavcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.25,
        0.35,
        0.35,
        0.50,
        0.75,
        0.35,
        0.35,
        0.90,
        0.0,
        0.35,
        0.35,
    },
    CCmasklccurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmasklccurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmasklccurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    enalcMask(false),
    blendmasklc(0),
    radmasklc(0.0),
    chromasklc(0.0),
    Lmasklccurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothresw(1.),
    lowthresw(12.),
    higthresw(85.),
    decayw(2.),
    // Contrast by detail levels
    visicbdl(false),
    expcbdl(false),
    complexcbdl(0),
    mult{1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
    chromacbdl(0.),
    threshold(0.2),
    sensicb(60),
    clarityml(0.1),
    contresid(0),
    softradiuscb(0.0),
    enacbMask(false),
    CCmaskcbcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskcbcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskcbcurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    blendmaskcb(0),
    radmaskcb(0.0),
    chromaskcb(0.0),
    gammaskcb(1.0),
    slomaskcb(0.0),
    lapmaskcb(0.0),
    Lmaskcbcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothrescb(1.),
    lowthrescb(12.),
    higthrescb(85.),
    decaycb(2.),
    // Log encoding
    visilog(false),
    explog(false),
    complexlog(0),
    autocompute(false),
    sourceGray(10.),
    sourceabs(2000.),
    targabs(16.),
    targetGray(18.),
    catad(0.),
    saturl(0.),
    chroml(0.),
    lightl(0.),
    lightq(0.),
    contl(0.),
    contthres(0.),
    contq(0.),
    colorfl(0.),
    LcurveL{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    Autogray(true),
    fullimage(true),
    repar(100.0),
    ciecam(false),
    satlog(false),
    blackEv(-5.00),
    whiteEv(10.00),
    whiteslog(0),
    blackslog(0),
    comprlog(0.4),
    strelog(100.),
    detail(0.6),
    sensilog(60),
    sursour("Average"),
    surround("Average"),
    baselog(2.),
    strlog(0.0),
    anglog(0.0),
    featherlog(25.0),
    CCmaskcurveL{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    LLmaskcurveL{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    HHmaskcurveL{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    enaLMask(false),
    blendmaskL(0),
    radmaskL(0.),
    chromaskL(0.),
    LmaskcurveL{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothresl(1.),
    lowthresl(12.),
    higthresl(85.),
    decayl(2.),
    // mask
    visimask(false),
    complexmask(0),
    expmask(false),
    sensimask(60),
    blendmask(-10.),
    blendmaskab(-10.),
    softradiusmask(1.0),
    enamask(false),
    fftmask(true),
    blurmask(0.2),
    contmask(0.),
    CCmask_curve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    LLmask_curve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    HHmask_curve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.00,
        1.0,
        0.35,
        0.35,
    },
    strumaskmask(0.),
    toolmask(false),
    radmask(0.0),
    lapmask(0.0),
    chromask(0.0),
    gammask(1.0),
    slopmask(0.0),
    shadmask(0.0),
    str_mask(0),
    ang_mask(0),
    feather_mask(25),
    HHhmask_curve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        0.50,
        0.5,
        0.35,
        0.35,
        1.00,
        0.5,
        0.35,
        0.35,
    },
    Lmask_curve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    LLmask_curvewav{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    csthresholdmask(0, 0, 6, 5, false),
    // ciecam
    visicie(false),
    expcie(false),
    expprecam(false),
    complexcie(0),
    reparcie(100.),
    sensicie(60),
    Autograycie(true),
    sigybjz12(false),
    qtoj(false),
    jabcie(true),
    comprcieauto(false),
    normcie12(false),
    normcie(true),
    gamutcie(true),
    bwcie(false),
    sigcie(true),
    logcie(false),
    satcie(true),
    logcieq(false),
    smoothcie(false),
    smoothcietrc(false),
    smoothcietrcrel(true),
    smoothcieyb(false),
    smoothcielum(false),
    smoothciehigh(true),
    smoothcielnk(true),
    smoothcieinv(false),
    logjz(false),
    sigjz12(false),
    sigjz(false),
    forcebw(false),
    sigq12(false),
    sigq(false),
    chjzcie(true),
    sourceGraycie(18.),
    sourceabscie(2000.),
    sursourcie("Average"),
    modecie("com"),
    modecam("cam16"),
    modeQJ("512"),
    bwevMethod12("slop"),
    bwevMethod("sig"),
    saturlcie(0.),
    rstprotectcie(0.),
    chromlcie(0.),
    huecie(0.),
    toneMethodcie("one"),
    ciecurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    toneMethodcie2("onec"),
    ciecurve2{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    chromjzcie(0.),
    saturjzcie(0.),
    huejzcie(0.),
    softjzcie(0.),
    strsoftjzcie(100.),
    thrhjzcie(60.),
    jzcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    czcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    czjzcurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    HHcurvejz{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    CHcurvejz{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    LHcurvejz{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.166,
        0.50,
        0.35,
        0.35,
        0.333,
        0.50,
        0.35,
        0.35,
        0.50,
        0.50,
        0.35,
        0.35,
        0.666,
        0.50,
        0.35,
        0.35,
        0.833,
        0.50,
        0.35,
        0.35,
    },
    lightlcie(0.),
    lightjzcie(0.),
    lightqcie(0.),
    lightsigqcie(0.),
    contlcie(0.),
    contjzcie(0.),
    detailciejz(0.),
    adapjzcie(4.0),
    jz100(0.25),
    pqremap(120.),
    pqremapcam16(100.),
    hljzcie(0.0),
    hlthjzcie(70.0),
    shjzcie(0.0),
    shthjzcie(40.0),
    radjzcie(40.0),
    sigmalcjz(1.),
    clarilresjz(0.),
    claricresjz(0.),
    clarisoftjz(0.),
    locwavcurvejz{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    csthresholdjz(0, 0, 7, 4, false),
    contthrescie(0.),
    blackEvjz(-5.00),
    whiteEvjz(10.00),
    targetjz(18.0),
    sigmoidldacie12(1.8),
    sigmoidthcie12(0.0),
    sigmoidblcie12(100.),
    sigmoidldacie(0.5),
    sigmoidthcie(1.2),
    sigmoidsenscie(0.9),
    sigmoidblcie(0.75),
    comprcie(0.4),
    strcielog(80.),
    comprcieth(6.),
    gamjcie(2.4),
    smoothcieth(1.0),
    smoothciethtrc(0.),
    slopjcie(12.923),
    satjcie(0.5),
    contsig(1.15),
    skewsig(0.),
    whitsig(100.),
    slopesmo(1.),
    slopesmoq(1.),
    slopesmor(1.),
    slopesmog(1.),
    slopesmob(1.),
    kslopesmor(1.),
    kslopesmog(1.),
    kslopesmob(1.),
    invcurve{//set to 0, 1 and 1, 0 to inverse color
        static_cast<double>(DCT_NURBS),
        0.0,
        1.0,
        1.0,
        0.0,
    },
    midtciemet("one"),
    midtcie(0),
    grexl(0.1700),//new Rec2020 default values in Color Appearance (CAM16) as in 'Abstract profile', instead of Prophoto.
    greyl(0.7970),
    bluxl(0.1310),
    bluyl(0.0460),
    redxl(0.7080),
    redyl(0.2920),
    refi(0.),
    shiftxl(0.),
    shiftyl(0.),
    whitescie(20),
    blackscie(0),
    illMethod("d65"),
    smoothciemet("none"),
    primMethod("rec"),
    catMethod("brad"),
    sigmoidldajzcie12(1.3),
    sigmoidthjzcie12(0.),
    sigmoidbljzcie12(100.),
    sigmoidldajzcie(0.5),
    sigmoidthjzcie(1.),
    sigmoidbljzcie(1.),
    contqcie(0.),
    contsigqcie(0.),
    colorflcie(0.),
    targabscie(16.),
    targetGraycie(18.),
    catadcie(0.),
    detailcie(0.),
    surroundcie("Average"),
    strgradcie(0.),
    anggradcie(0.),
    feathercie(25.),
    enacieMask(false),
    enacieMaskall(false),
    CCmaskciecurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    LLmaskciecurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHmaskciecurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.0,
        0.35,
        0.35,
        0.50,
        1.0,
        0.35,
        0.35,
        1.0,
        1.0,
        0.35,
        0.35,
    },
    HHhmaskciecurve{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        0.50,
        0.5,
        0.35,
        0.35,
        1.00,
        0.5,
        0.35,
        0.35,
    },

    blendmaskcie(0),
    radmaskcie(0.0),
    chromaskcie(0.0),
    lapmaskcie(0.0),
    gammaskcie(1.0),
    slomaskcie(0.0),
    Lmaskciecurve{
        static_cast<double>(DCT_NURBS),
        0.0,
        0.0,
        1.0,
        1.0,
    },
    recothrescie(1.),
    lowthrescie(12.),
    higthrescie(85.),
    decaycie(2.),
    strumaskcie(0.),
    toolcie(false),
    fftcieMask(true),
    contcie(0.),
    blurcie(0.2),
    highmaskcie(0.),
    shadmaskcie(0.),
     LLmaskciecurvewav{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.5,
        0.35,
        0.35,
        1.,
        0.5,
        0.35,
        0.35,
    },
    csthresholdcie(0, 0, 6, 5, false)
   

{
  // init settings with Preferences / options : must be followed by call to spotMethodChanged in controlspotpanel.cc  (idle_register)
  // new values default with different SpotMethod.

    const auto spotmet = App::get().options().spotmet;

    if(spotmet == 3) {//global
        spotMethod = "main";
        loc = {3000, 3000, 3000, 3000};
        transit =100.;
        shape = "RECT";

    } else if(spotmet == 2) {//full image
        spotMethod = "full";
        loc = {3000, 3000, 3000, 3000};
        transit =100.;
        shape = "RECT";       
        sensi = 30;
        sensiex = 60;
        sensihs = 30;
        sensiv = 30;
        sensisf = 30; 
        sensibn = 40;
        sensiden = 60;
        sensitm = 60;
        sensih = 60;
        sensisha = 40;
        sensilc = 60;
        sensicb = 60; 
        sensilog = 60;
        sensimask = 60;
        sensicie = 60;
        
    } else if(spotmet == 1) {//exclude
        spotMethod = "exc";
        shape = "ELI";
        loc = {150, 150, 150, 150};
        transit= 60.;
        sensi = 30;
        sensiex = 60;
        sensihs = 30;
        sensiv = 30;
        sensibn = 40;
        sensiden = 60;
        sensitm = 60;
        sensih = 60;
        sensisha = 40;       
        sensilc = 60;        
        sensicb = 60; 
        sensilog = 60;
        sensimask = 60;
        sensicie = 60;
        
    } else if(spotmet == 0) {//normal
        spotMethod = "norm";
        shape = "ELI";
        loc = {150, 150, 150, 150};
        transit= 60.;
        sensi = 30;
        sensiex = 60;
        sensihs = 30;
        sensiv = 30;
        sensibn = 40;
        sensiden = 60;
        sensitm = 60;
        sensih = 60;
        sensisha = 40;       
        sensilc = 60;        
        sensicb = 60; 
        sensilog = 60;
        sensimask = 60;
        sensicie = 60;  
    }

}

bool LocallabParams::LocallabSpot::operator ==(const LocallabSpot& other) const
{
    // clang-format off
    return
        // Control spot settings
        name == other.name
        && isvisible == other.isvisible
        && prevMethod == other.prevMethod
        && shape == other.shape
        && spotMethod == other.spotMethod
        && wavMethod == other.wavMethod
        && sensiexclu == other.sensiexclu
        && structexclu == other.structexclu
        && struc == other.struc
        && shapeMethod == other.shapeMethod
        && avoidgamutMethod == other.avoidgamutMethod
        && loc == other.loc
        && centerX == other.centerX
        && centerY == other.centerY
        && circrad == other.circrad
        && qualityMethod == other.qualityMethod
        && complexMethod == other.complexMethod
        && transit == other.transit
        && feather == other.feather
        && thresh == other.thresh
        && iter == other.iter
        && balan == other.balan
        && balanh == other.balanh
        && colorde == other.colorde
        && colorscope == other.colorscope
        && avoidrad == other.avoidrad
        && transitweak == other.transitweak
        && transitgrad == other.transitgrad
        && hishow == other.hishow
        && activ == other.activ
        && avoidneg == other.avoidneg
        && blwh == other.blwh
        && recurs == other.recurs
        && laplac == other.laplac
        && deltae == other.deltae
        && shortc == other.shortc
        && savrest == other.savrest
        && scopemask == other.scopemask
        && denoichmask == other.denoichmask
        && lumask == other.lumask
        // Color & Light
        && visicolor == other.visicolor
        && expcolor == other.expcolor
        && complexcolor == other.complexcolor
        && curvactiv == other.curvactiv
        && lightness == other.lightness
        && reparcol == other.reparcol
        && gamc == other.gamc
        && contrast == other.contrast
        && chroma == other.chroma
        && labgridALow == other.labgridALow
        && labgridBLow == other.labgridBLow
        && labgridAHigh == other.labgridAHigh
        && labgridBHigh == other.labgridBHigh
        && labgridALowmerg == other.labgridALowmerg
        && labgridBLowmerg == other.labgridBLowmerg
        && labgridAHighmerg == other.labgridAHighmerg
        && labgridBHighmerg == other.labgridBHighmerg
        && strengthgrid == other.strengthgrid
        && sensi == other.sensi
        && structcol == other.structcol
        && strcol == other.strcol
        && strcolab == other.strcolab
        && strcolh == other.strcolh
        && angcol == other.angcol
        && feathercol == other.feathercol
        && blurcolde == other.blurcolde
        && blurcol == other.blurcol
        && contcol == other.contcol
        && blendmaskcol == other.blendmaskcol
        && radmaskcol == other.radmaskcol
        && chromaskcol == other.chromaskcol
        && gammaskcol == other.gammaskcol
        && slomaskcol == other.slomaskcol
        && shadmaskcol == other.shadmaskcol
        && strumaskcol == other.strumaskcol
        && lapmaskcol == other.lapmaskcol
        && qualitycurveMethod == other.qualitycurveMethod
        && gridMethod == other.gridMethod
        && merMethod == other.merMethod
        && toneMethod == other.toneMethod
        && mergecolMethod == other.mergecolMethod
        && llcurve == other.llcurve
        && lccurve == other.lccurve
        && cccurve == other.cccurve
        && clcurve == other.clcurve
        && rgbcurve == other.rgbcurve
        && LHcurve == other.LHcurve
        && HHcurve == other.HHcurve
        && CHcurve == other.CHcurve
        && invers == other.invers
        && special == other.special
        && toolcol == other.toolcol
        && enaColorMask == other.enaColorMask
        && fftColorMask == other.fftColorMask
        && CCmaskcurve == other.CCmaskcurve
        && LLmaskcurve == other.LLmaskcurve
        && HHmaskcurve == other.HHmaskcurve
        && HHhmaskcurve == other.HHhmaskcurve
        && softradiuscol == other.softradiuscol
        && opacol == other.opacol
        && mercol == other.mercol
        && merlucol == other.merlucol
        && conthrcol == other.conthrcol
        && Lmaskcurve == other.Lmaskcurve
        && LLmaskcolcurvewav == other.LLmaskcolcurvewav
        && csthresholdcol == other.csthresholdcol
        && recothresc == other.recothresc
        && lowthresc == other.lowthresc
        && higthresc == other.higthresc
        && decayc == other.decayc
        // Exposure
        && visiexpose == other.visiexpose
        && expexpose == other.expexpose
        && complexexpose == other.complexexpose
        && expcomp == other.expcomp
        && hlcompr == other.hlcompr
        && hlcomprthresh == other.hlcomprthresh
        && black == other.black
        && shadex == other.shadex
        && shcompr == other.shcompr
        && expchroma == other.expchroma
        && sensiex == other.sensiex
        && structexp == other.structexp
        && blurexpde == other.blurexpde
        && gamex == other.gamex
        && strexp == other.strexp
        && angexp == other.angexp
        && featherexp == other.featherexp
        && excurve == other.excurve
        && norm == other.norm
        && inversex == other.inversex
        && enaExpMask == other.enaExpMask
        && enaExpMaskaft == other.enaExpMaskaft
        && CCmaskexpcurve == other.CCmaskexpcurve
        && LLmaskexpcurve == other.LLmaskexpcurve
        && HHmaskexpcurve == other.HHmaskexpcurve
        && blendmaskexp == other.blendmaskexp
        && radmaskexp == other.radmaskexp
        && chromaskexp == other.chromaskexp
        && gammaskexp == other.gammaskexp
        && slomaskexp == other.slomaskexp
        && lapmaskexp == other.lapmaskexp
        && strmaskexp == other.strmaskexp
        && angmaskexp == other.angmaskexp
        && softradiusexp == other.softradiusexp
        && Lmaskexpcurve == other.Lmaskexpcurve
        && expMethod == other.expMethod
        && exnoiseMethod == other.exnoiseMethod
        && laplacexp == other.laplacexp
        && reparexp == other.reparexp
        && balanexp == other.balanexp
        && linear == other.linear
        && gamm == other.gamm
        && fatamount == other.fatamount
        && fatdetail == other.fatdetail
        && fatsatur == other.fatsatur
        && fatanchor == other.fatanchor
        && fatlevel == other.fatlevel
        && recothrese == other.recothrese
        && lowthrese == other.lowthrese
        && higthrese == other.higthrese
        && decaye == other.decaye
        // Shadow highlight
        && visishadhigh == other.visishadhigh
        && expshadhigh == other.expshadhigh
        && complexshadhigh == other.complexshadhigh
        && shMethod == other.shMethod
        && ghsMethod == other.ghsMethod
        && ghsMatmet == other.ghsMatmet
        && ghsMode == other.ghsMode
        && ghs_D == other.ghs_D
        && ghs_mtfstr == other.ghs_mtfstr
        && ghs_slope == other.ghs_slope
        && ghs_chro == other.ghs_chro
        && ghs_B == other.ghs_B
        && SPAutoRadius == other.SPAutoRadius       
        && (SPAutoRadius || (ghs_SP == other.ghs_SP))
        //&& ghs_SP == other.ghs_SP
        && ghs_LP == other.ghs_LP
        && ghs_HP == other.ghs_HP
        && ghs_LC == other.ghs_LC
        && ghs_MID == other.ghs_MID
        && ghs_BLP == other.ghs_BLP
        && ghs_HLP == other.ghs_HLP
        && ghs_autobw == other.ghs_autobw
        && ghs_agx == other.ghs_agx
        && ghs_mtf == other.ghs_mtf
        && ghs_smooth == other.ghs_smooth
        && ghs_inv == other.ghs_inv
        && mich_exp == other.mich_exp
        && mich_spar == other.mich_spar
        && mich_kpar == other.mich_kpar
        && mich_sat == other.mich_sat
        && mich_out == other.mich_out
        && mich_black == other.mich_black
        && mich_white == other.mich_white
        && mich_high == other.mich_high
        && mich_jdx == other.mich_jdx
        && mich_mtf == other.mich_mtf

        && [this, &other]() -> bool
            {
                for (int i = 0; i < 6; ++i) {
                    if (multsh[i] != other.multsh[i]) {
                        return false;
                    }
                }
                return true;
            }()
        && highlights == other.highlights
        && h_tonalwidth == other.h_tonalwidth
        && shadows == other.shadows
        && s_tonalwidth == other.s_tonalwidth
        && sh_radius == other.sh_radius
        && sensihs == other.sensihs
        && enaSHMask == other.enaSHMask
        && CCmaskSHcurve == other.CCmaskSHcurve
        && LLmaskSHcurve == other.LLmaskSHcurve
        && HHmaskSHcurve == other.HHmaskSHcurve
        && blendmaskSH == other.blendmaskSH
        && radmaskSH == other.radmaskSH
        && blurSHde == other.blurSHde
        && strSH == other.strSH
        && angSH == other.angSH
        && featherSH == other.featherSH
        && inverssh == other.inverssh
        && chromaskSH == other.chromaskSH
        && gammaskSH == other.gammaskSH
        && slomaskSH == other.slomaskSH
        && lapmaskSH == other.lapmaskSH
        && detailSH == other.detailSH
        && tePivot == other.tePivot
        && reparsh == other.reparsh
        && LmaskSHcurve == other.LmaskSHcurve
        && fatamountSH == other.fatamountSH
        && fatanchorSH == other.fatanchorSH
        && gamSH == other.gamSH
        && sloSH == other.sloSH
        && recothress == other.recothress
        && lowthress == other.lowthress
        && higthress == other.higthress
        && decays == other.decays
        // Vibrance
        && visivibrance == other.visivibrance
        && expvibrance == other.expvibrance
        && complexvibrance == other.complexvibrance
        && saturated == other.saturated
        && pastels == other.pastels
        && vibgam == other.vibgam
        && warm == other.warm
        && psthreshold == other.psthreshold
        && protectskins == other.protectskins
        && avoidcolorshift == other.avoidcolorshift
        && pastsattog == other.pastsattog
        && sensiv == other.sensiv
        && skintonescurve == other.skintonescurve
        && CCmaskvibcurve == other.CCmaskvibcurve
        && LLmaskvibcurve == other.LLmaskvibcurve
        && HHmaskvibcurve == other.HHmaskvibcurve
        && enavibMask == other.enavibMask
        && blendmaskvib == other.blendmaskvib
        && radmaskvib == other.radmaskvib
        && chromaskvib == other.chromaskvib
        && gammaskvib == other.gammaskvib
        && slomaskvib == other.slomaskvib
        && lapmaskvib == other.lapmaskvib
        && strvib == other.strvib
        && strvibab == other.strvibab
        && strvibh == other.strvibh
        && angvib == other.angvib
        && feathervib == other.feathervib
        && Lmaskvibcurve == other.Lmaskvibcurve
        && recothresv == other.recothresv
        && lowthresv == other.lowthresv
        && higthresv == other.higthresv
        && decayv == other.decayv
        // Soft Light
        && visisoft == other.visisoft
        && expsoft == other.expsoft
        && complexsoft == other.complexsoft
        && streng == other.streng
        && sensisf == other.sensisf
        && laplace == other.laplace
        && softMethod == other.softMethod
        // Blur & Noise
        && visiblur == other.visiblur
        && expblur == other.expblur
        && complexblur == other.complexblur
        && radius == other.radius
        && strength == other.strength
        && sensibn == other.sensibn
        && itera == other.itera
        && guidbl == other.guidbl
        && strbl == other.strbl
        && recothres == other.recothres
        && lowthres == other.lowthres
        && higthres == other.higthres
        && recothresd == other.recothresd
        && lowthresd == other.lowthresd
        && midthresd == other.midthresd
        && midthresdch == other.midthresdch
        && higthresd == other.higthresd
        && decayd == other.decayd
        && isogr == other.isogr
        && strengr == other.strengr
        && scalegr == other.scalegr
        && divgr == other.divgr
        && epsbl == other.epsbl
        && blMethod == other.blMethod
        && chroMethod == other.chroMethod
        && quamethod == other.quamethod
        && blurMethod == other.blurMethod
        && usemask == other.usemask
        && invmaskd == other.invmaskd
        && invmask == other.invmask
        && levelthr == other.levelthr
        && lnoiselow == other.lnoiselow
        && levelthrlow == other.levelthrlow
        && medMethod == other.medMethod
        && activlum == other.activlum
        && [this, &other]() -> bool
            {
                for (int i = 0; i < 21; ++i) {
                    if (madlsav[i] != other.madlsav[i]) {
                        return false;
                    }
                }
                return true;
            }()    
        && noiselumf == other.noiselumf
        && noiselumf0 == other.noiselumf0
        && noiselumf2 == other.noiselumf2
        && noiselumc == other.noiselumc
        && noiselumdetail == other.noiselumdetail
        && noiselequal == other.noiselequal
        && noisegam == other.noisegam
        && noisechrof == other.noisechrof
        && noisechroc == other.noisechroc
        && noisechrodetail == other.noisechrodetail
        && adjblur == other.adjblur
        && bilateral == other.bilateral
        && nlstr == other.nlstr
        && nldet == other.nldet
        && nlpat == other.nlpat
        && nlrad == other.nlrad
        && nlgam == other.nlgam
        && nliter == other.nliter
        && sensiden == other.sensiden
        && reparden == other.reparden
        && detailthr == other.detailthr
        && locwavcurveden == other.locwavcurveden
        && locwavcurvehue == other.locwavcurvehue
        && locwavcurvehuecont == other.locwavcurvehuecont
        && showmaskblMethodtyp == other.showmaskblMethodtyp
        && CCmaskblcurve == other.CCmaskblcurve
        && LLmaskblcurve == other.LLmaskblcurve
        && HHmaskblcurve == other.HHmaskblcurve
        && enablMask == other.enablMask
        && fftwbl == other.fftwbl
        && invbl == other.invbl
        && toolbl == other.toolbl
        && blendmaskbl == other.blendmaskbl
        && radmaskbl == other.radmaskbl
        && chromaskbl == other.chromaskbl
        && gammaskbl == other.gammaskbl
        && slomaskbl == other.slomaskbl
        && lapmaskbl == other.lapmaskbl
        && shadmaskbl == other.shadmaskbl
        && shadmaskblsha == other.shadmaskblsha
        && strumaskbl == other.strumaskbl
        && Lmaskblcurve == other.Lmaskblcurve
        && LLmaskblcurvewav == other.LLmaskblcurvewav
        && csthresholdblur == other.csthresholdblur
        && denocontrast == other.denocontrast
        && denoAutocontrast == other.denoAutocontrast
        && contrshow == other.contrshow
        && lockmadl == other.lockmadl
        && madllock == other.madllock
        && enacontrast == other.enacontrast
        && denoratio == other.denoratio
        && denomask == other.denomask
        // Tone Mapping
        && visitonemap == other.visitonemap
        && exptonemap == other.exptonemap
        && complextonemap == other.complextonemap
        && stren == other.stren
        && gamma == other.gamma
        && estop == other.estop
        && scaltm == other.scaltm
        && repartm == other.repartm
        && rewei == other.rewei
        && satur == other.satur
        && sensitm == other.sensitm
        && softradiustm == other.softradiustm
        && amount == other.amount
        && equiltm == other.equiltm
        && CCmasktmcurve == other.CCmasktmcurve
        && LLmasktmcurve == other.LLmasktmcurve
        && HHmasktmcurve == other.HHmasktmcurve
        && enatmMask == other.enatmMask
        && enatmMaskaft == other.enatmMaskaft
        && blendmasktm == other.blendmasktm
        && radmasktm == other.radmasktm
        && chromasktm == other.chromasktm
        && gammasktm == other.gammasktm
        && slomasktm == other.slomasktm
        && lapmasktm == other.lapmasktm
        && Lmasktmcurve == other.Lmasktmcurve
        && recothrest == other.recothrest
        && lowthrest == other.lowthrest
        && higthrest == other.higthrest
        && decayt == other.decayt
        // Retinex
        && visireti == other.visireti
        && expreti == other.expreti
        && complexreti == other.complexreti
        && retinexMethod == other.retinexMethod
        && str == other.str
        && chrrt == other.chrrt
        && neigh == other.neigh
        && vart == other.vart
        && offs == other.offs
        && dehaz == other.dehaz
        && depth == other.depth
        && sensih == other.sensih
        && localTgaincurve == other.localTgaincurve
        && localTtranscurve == other.localTtranscurve
        && inversret == other.inversret
        && equilret == other.equilret
        && loglin == other.loglin
        && dehazeSaturation == other.dehazeSaturation
        && dehazeblack == other.dehazeblack
        && softradiusret == other.softradiusret
        && CCmaskreticurve == other.CCmaskreticurve
        && LLmaskreticurve == other.LLmaskreticurve
        && HHmaskreticurve == other.HHmaskreticurve
        && enaretiMask == other.enaretiMask
        && enaretiMasktmap == other.enaretiMasktmap
        && blendmaskreti == other.blendmaskreti
        && radmaskreti == other.radmaskreti
        && chromaskreti == other.chromaskreti
        && gammaskreti == other.gammaskreti
        && slomaskreti == other.slomaskreti
        && lapmaskreti == other.lapmaskreti
        && scalereti == other.scalereti
        && darkness == other.darkness
        && lightnessreti == other.lightnessreti
        && limd == other.limd
        && cliptm == other.cliptm
        && fftwreti == other.fftwreti
        && Lmaskreticurve == other.Lmaskreticurve
        && recothresr == other.recothresr
        && lowthresr == other.lowthresr
        && higthresr == other.higthresr
        && decayr == other.decayr
        // Sharpening
        && visisharp == other.visisharp
        && expsharp == other.expsharp
        && complexsharp == other.complexsharp
    //    && sharcontrast == other.sharcontrast
        && (deconvAutoshar || (sharcontrast == other.sharcontrast))
        && sharradius == other.sharradius
        && sharamount == other.sharamount
        && shardamping == other.shardamping
        && shariter == other.shariter
        && sharblur == other.sharblur
        && shargam == other.shargam
        && sensisha == other.sensisha
        && inverssha == other.inverssha
        && sharshow == other.sharshow
        && itercheck == other.itercheck
        && methodcap == other.methodcap
        && deconvAutoRadius == other.deconvAutoRadius       
        && (deconvAutoRadius || (capradius == other.capradius))
        && deconvCoBoost == other.deconvCoBoost     
        && deconvCoProt == other.deconvCoProt     
        && deconvCoLat == other.deconvCoLat     
        && deconvCogam == other.deconvCogam     
        && reparsha == other.reparsha     

        // Local contrast
        && visicontrast == other.visicontrast
        && expcontrast == other.expcontrast
        && complexcontrast == other.complexcontrast
        && lcradius == other.lcradius
        && lcamount == other.lcamount
        && lcdarkness == other.lcdarkness
        && lclightness == other.lclightness
        && sigmalc == other.sigmalc
        && offslc == other.offslc
        && levelwav == other.levelwav
        && residcont == other.residcont
        && residsha == other.residsha
        && residshathr == other.residshathr
        && residhi == other.residhi
        && residhithr == other.residhithr
        && gamlc == other.gamlc
        && residgam == other.residgam
        && residslop == other.residslop
        && residblur == other.residblur
        && levelblur == other.levelblur
        && sigmabl == other.sigmabl
        && residchro == other.residchro
        && residcomp == other.residcomp
        && sigma == other.sigma
        && offset == other.offset
        && sigmadr == other.sigmadr
        && threswav == other.threswav
        && chromalev == other.chromalev
        && chromablu == other.chromablu
        && sigmadc == other.sigmadc
        && deltad == other.deltad
        && fatres == other.fatres
        && clarilres == other.clarilres
        && claricres == other.claricres
        && clarisoft == other.clarisoft
        && sigmalc2 == other.sigmalc2
        && strwav == other.strwav
        && angwav == other.angwav
        && featherwav == other.featherwav
        && strengthw == other.strengthw
        && sigmaed == other.sigmaed
        && radiusw == other.radiusw
        && detailw == other.detailw
        && gradw == other.gradw
        && tloww == other.tloww
        && thigw == other.thigw
        && edgw == other.edgw
        && basew == other.basew
        && sensilc == other.sensilc
        && reparw == other.reparw
        && fftwlc == other.fftwlc
        && blurlc == other.blurlc
        && wavblur == other.wavblur
        && wavedg == other.wavedg
        && waveshow == other.waveshow
        && wavcont == other.wavcont
        && wavcomp == other.wavcomp
        && wavgradl == other.wavgradl
        && wavcompre == other.wavcompre
        && origlc == other.origlc
        && processwav == other.processwav
        && localcontMethod == other.localcontMethod
        && localedgMethod == other.localedgMethod
        && localneiMethod == other.localneiMethod
        && locwavcurve == other.locwavcurve
        && csthreshold == other.csthreshold
        && loclevwavcurve == other.loclevwavcurve
        && locconwavcurve == other.locconwavcurve
        && loccompwavcurve == other.loccompwavcurve
        && loccomprewavcurve == other.loccomprewavcurve
        && locedgwavcurve == other.locedgwavcurve
        && CCmasklccurve == other.CCmasklccurve
        && LLmasklccurve == other.LLmasklccurve
        && HHmasklccurve == other.HHmasklccurve
        && enalcMask == other.enalcMask
        && blendmasklc == other.blendmasklc
        && radmasklc == other.radmasklc
        && chromasklc == other.chromasklc
        && Lmasklccurve == other.Lmasklccurve
        && recothresw == other.recothresw
        && lowthresw == other.lowthresw
        && higthresw == other.higthresw
        && decayw == other.decayw
        // Contrast by detail levels
        && visicbdl == other.visicbdl
        && expcbdl == other.expcbdl
        && complexcbdl == other.complexcbdl
        && [this, &other]() -> bool
            {
                for (int i = 0; i < 6; ++i) {
                    if (mult[i] != other.mult[i]) {
                        return false;
                    }
                }
                return true;
            }()
        && chromacbdl == other.chromacbdl
        && threshold == other.threshold
        && sensicb == other.sensicb
        && clarityml == other.clarityml
        && contresid == other.contresid
        && softradiuscb == other.softradiuscb
        && enacbMask == other.enacbMask
        && CCmaskcbcurve == other.CCmaskcbcurve
        && LLmaskcbcurve == other.LLmaskcbcurve
        && HHmaskcbcurve == other.HHmaskcbcurve
        && blendmaskcb == other.blendmaskcb
        && radmaskcb == other.radmaskcb
        && chromaskcb == other.chromaskcb
        && gammaskcb == other.gammaskcb
        && slomaskcb == other.slomaskcb
        && lapmaskcb == other.lapmaskcb
        && Lmaskcbcurve == other.Lmaskcbcurve
        && recothrescb == other.recothrescb
        && lowthrescb == other.lowthrescb
        && higthrescb == other.higthrescb
        && decaycb == other.decaycb
        // Log encoding
        && visilog == other.visilog
        && explog == other.explog
        && complexlog == other.complexlog
        && autocompute == other.autocompute
        && sourceGray == other.sourceGray
        && sourceabs == other.sourceabs
        && targabs == other.targabs
        && targetGray == other.targetGray
        && catad == other.catad
        && saturl == other.saturl
        && chroml == other.chroml
        && lightl == other.lightl
        && lightq == other.lightq
        && contl == other.contl
        && contthres == other.contthres
        && contq == other.contq
        && colorfl == other.colorfl
        && LcurveL == other.LcurveL
        && Autogray == other.Autogray
        && fullimage == other.fullimage
        && repar == other.repar
        && ciecam == other.ciecam
        && satlog == other.satlog
        && blackEv == other.blackEv
        && whiteEv == other.whiteEv
        && whiteslog == other.whiteslog
        && blackslog == other.blackslog
        && comprlog == other.comprlog
        && strelog == other.strelog
        && detail == other.detail
        && sensilog == other.sensilog
        && baselog == other.baselog
        && sursour == other.sursour
        && surround == other.surround
        && strlog == other.strlog
        && anglog == other.anglog
        && featherlog == other.featherlog
        && CCmaskcurveL == other.CCmaskcurveL
        && LLmaskcurveL == other.LLmaskcurveL
        && HHmaskcurveL == other.HHmaskcurveL
        && enaLMask == other.enaLMask
        && blendmaskL == other.blendmaskL
        && radmaskL == other.radmaskL
        && chromaskL == other.chromaskL
        && LmaskcurveL == other.LmaskcurveL
        && recothresl == other.recothresl
        && lowthresl == other.lowthresl
        && higthresl == other.higthresl
        && decayl == other.decayl
        // mask
        && visimask == other.visimask
        && complexmask == other.complexmask
        && expmask == other.expmask
        && sensimask == other.sensimask
        && blendmask == other.blendmask
        && blendmaskab == other.blendmaskab
        && softradiusmask == other.softradiusmask
        && enamask == other.enamask
        && fftmask == other.fftmask
        && blurmask == other.blurmask
        && contmask == other.contmask
        && CCmask_curve == other.CCmask_curve
        && LLmask_curve == other.LLmask_curve
        && HHmask_curve == other.HHmask_curve
        && strumaskmask == other.strumaskmask
        && toolmask == other.toolmask
        && radmask == other.radmask
        && lapmask == other.lapmask
        && chromask == other.chromask
        && gammask == other.gammask
        && slopmask == other.slopmask
        && shadmask == other.shadmask
        && str_mask == other.str_mask
        && ang_mask == other.ang_mask
        && feather_mask == other.feather_mask
        && HHhmask_curve == other.HHhmask_curve
        && Lmask_curve == other.Lmask_curve
        && LLmask_curvewav == other.LLmask_curvewav
        && csthresholdmask == other.csthresholdmask
        //ciecam
        && visicie == other.visicie
        && expcie == other.expcie
        && expprecam == other.expprecam
        && complexcie == other.complexcie
        && reparcie == other.reparcie
        && sensicie == other.sensicie
        && Autograycie == other.Autograycie
        && sigybjz12 == other.sigybjz12
        && qtoj == other.qtoj
        && jabcie == other.jabcie
        && comprcieauto == other.comprcieauto
        && normcie12 == other.normcie12
        && normcie == other.normcie
        && gamutcie == other.gamutcie
        && bwcie == other.bwcie
        && sigcie == other.sigcie
        && logcie == other.logcie
        && satcie == other.satcie
        && logcieq == other.logcieq
        && smoothcie == other.smoothcie
        && smoothcietrc == other.smoothcietrc
        && smoothcietrcrel == other.smoothcietrcrel
        && smoothcieyb == other.smoothcieyb
        && smoothcielum == other.smoothcielum
        && smoothciehigh == other.smoothciehigh
        && smoothcielnk == other.smoothcielnk
        && smoothcieinv == other.smoothcieinv
        && logjz == other.logjz
        && sigjz12 == other.sigjz12
        && sigjz == other.sigjz
        && forcebw == other.forcebw
        && sigq12 == other.sigq12
        && sigq == other.sigq
        && chjzcie == other.chjzcie
        && sourceGraycie == other.sourceGraycie
        && sourceabscie == other.sourceabscie
        && sursourcie == other.sursourcie
        && modecie == other.modecie
        && modecam == other.modecam
        && modeQJ == other.modeQJ
        && bwevMethod12 == other.bwevMethod12
        && bwevMethod == other.bwevMethod
        && saturlcie == other.saturlcie
        && rstprotectcie == other.rstprotectcie
        && chromlcie == other.chromlcie
        && huecie == other.huecie
        && toneMethodcie == other.toneMethodcie
        && ciecurve == other.ciecurve
        && toneMethodcie2 == other.toneMethodcie2
        && ciecurve2 == other.ciecurve2
        && chromjzcie == other.chromjzcie
        && saturjzcie == other.saturjzcie
        && huejzcie == other.huejzcie
        && softjzcie == other.softjzcie
        && strsoftjzcie == other.strsoftjzcie
        && thrhjzcie == other.thrhjzcie
        && jzcurve == other.jzcurve
        && czcurve == other.czcurve
        && czjzcurve == other.czjzcurve
        && invcurve == other.invcurve
        && HHcurvejz == other.HHcurvejz
        && CHcurvejz == other.CHcurvejz
        && LHcurvejz == other.LHcurvejz
        && lightlcie == other.lightlcie
        && lightjzcie == other.lightjzcie
        && lightqcie == other.lightqcie
        && lightsigqcie == other.lightsigqcie
        && contlcie == other.contlcie
        && contjzcie == other.contjzcie
        && detailciejz == other.detailciejz
        && adapjzcie == other.adapjzcie
        && jz100 == other.jz100
        && pqremap == other.pqremap
        && pqremapcam16 == other.pqremapcam16
        && hljzcie == other.hljzcie
        && hlthjzcie == other.hlthjzcie
        && shjzcie == other.shjzcie
        && shthjzcie == other.shthjzcie
        && radjzcie == other.radjzcie
        && sigmalcjz == other.sigmalcjz
        && clarilresjz == other.clarilresjz
        && claricresjz == other.claricresjz
        && clarisoftjz == other.clarisoftjz
        && locwavcurvejz == other.locwavcurvejz
        && csthresholdjz == other.csthresholdjz
        && contthrescie == other.contthrescie
        && blackEvjz == other.blackEvjz
        && whiteEvjz == other.whiteEvjz
        && targetjz == other.targetjz
        && sigmoidldacie12 == other.sigmoidldacie12
        && sigmoidthcie12 == other.sigmoidthcie12
        && sigmoidblcie12 == other.sigmoidblcie12
        && sigmoidldacie == other.sigmoidldacie
        && sigmoidthcie == other.sigmoidthcie
        && sigmoidsenscie == other.sigmoidsenscie
        && sigmoidblcie == other.sigmoidblcie
        && comprcie == other.comprcie
        && strcielog == other.strcielog
        && comprcieth == other.comprcieth
        && gamjcie == other.gamjcie
        && smoothcieth == other.smoothcieth
        && smoothciethtrc == other.smoothciethtrc
        && slopjcie == other.slopjcie
        && satjcie == other.satjcie
        && contsig == other.contsig
        && skewsig == other.skewsig
        && whitsig == other.whitsig
        && slopesmo == other.slopesmo
        && slopesmoq == other.slopesmoq
        && slopesmor == other.slopesmor
        && slopesmog == other.slopesmog
        && slopesmob == other.slopesmob
        && kslopesmor == other.kslopesmor
        && kslopesmog == other.kslopesmog
        && kslopesmob == other.kslopesmob
        && midtciemet == other.midtciemet
        && midtcie == other.midtcie
        && redxl == other.redxl
        && redyl == other.redyl
        && grexl == other.grexl
        && greyl == other.greyl
        && bluxl == other.bluxl
        && bluyl == other.bluyl
        && refi == other.refi
        && shiftxl == other.shiftxl
        && shiftyl == other.shiftyl
        && labgridcieALow == other.labgridcieALow
        && labgridcieBLow == other.labgridcieBLow
        && labgridcieAHigh == other.labgridcieAHigh
        && labgridcieBHigh == other.labgridcieBHigh
        && labgridcieGx == other.labgridcieGx
        && labgridcieGy == other.labgridcieGy
        && labgridcieWx == other.labgridcieWx
        && labgridcieWy == other.labgridcieWy        
        && labgridcieMx == other.labgridcieMx
        && labgridcieMy == other.labgridcieMy        
        && whitescie == other.whitescie
        && blackscie == other.blackscie
        && illMethod == other.illMethod
        && smoothciemet == other.smoothciemet
        && primMethod == other.primMethod
        && catMethod == other.catMethod
        && sigmoidldajzcie12 == other.sigmoidldajzcie12
        && sigmoidthjzcie12 == other.sigmoidthjzcie12
        && sigmoidbljzcie12 == other.sigmoidbljzcie12
        && sigmoidldajzcie == other.sigmoidldajzcie
        && sigmoidthjzcie == other.sigmoidthjzcie
        && sigmoidbljzcie == other.sigmoidbljzcie
        && contqcie == other.contqcie
        && contsigqcie == other.contsigqcie
        && colorflcie == other.colorflcie
        && targabscie == other.targabscie
        && targetGraycie == other.targetGraycie
        && catadcie == other.catadcie
        && detailcie == other.detailcie
        && strgradcie == other.strgradcie
        && anggradcie == other.anggradcie
        && feathercie == other.feathercie
        && surroundcie == other.surroundcie
        && enacieMask == other.enacieMask
        && enacieMaskall == other.enacieMaskall
        && CCmaskciecurve == other.CCmaskciecurve
        && LLmaskciecurve == other.LLmaskciecurve
        && HHmaskciecurve == other.HHmaskciecurve
        && HHhmaskciecurve == other.HHhmaskcurve
        && blendmaskcie == other.blendmaskcie
        && radmaskcie == other.radmaskcie
        && chromaskcie == other.chromaskcie
        && lapmaskcie == other.lapmaskcie
        && gammaskcie == other.gammaskcie
        && slomaskcie == other.slomaskcie
        && Lmaskciecurve == other.Lmaskciecurve
        && recothrescie == other.recothrescie
        && lowthrescie == other.lowthrescie
        && higthrescie == other.higthrescie
        && decaycie == other.decaycie
        && strumaskcie == other.strumaskcie
        && toolcie == other.toolcie       
        && blurcie == other.blurcie
        && contcie == other.contcie
        && highmaskcie == other.highmaskcie
        && shadmaskcie == other.shadmaskcie
        && fftcieMask == other.fftcieMask
        && LLmaskciecurvewav == other.LLmaskciecurvewav
        && csthresholdcie == other.csthresholdcie;
    // clang-format on
}

const double LocallabParams::LABGRIDL_CORR_MAX = 12800.;
const double LocallabParams::LABGRIDL_CORR_SCALE = 3.276;
const double LocallabParams::LABGRIDL_DIRECT_SCALE = 41950.;

LocallabParams::LocallabParams() :
    enabled(false),
    selspot(0),
    spots()
{
}

bool LocallabParams::operator ==(const LocallabParams& other) const
{
    return
        enabled == other.enabled
        && selspot == other.selspot
        && spots == other.spots;
}

void loadLocalLabParams(const Glib::KeyFile& keyFile, LocallabParams& locallab,
                        ParamsEdited* pedited, int ppVersion)
{
    if (!keyFile.has_group("Locallab")) return;

    assignFromKeyfile(keyFile, "Locallab", "Enabled", locallab.enabled, pedited->locallab.enabled);
    assignFromKeyfile(keyFile, "Locallab", "Selspot", locallab.selspot, pedited->locallab.selspot);

    Glib::ustring ppName;
    bool peName;
    int i = 0;

    while (assignFromKeyfile(keyFile, "Locallab", "Name_" + std::to_string(i), ppName, peName)) {
        const std::string index_str = std::to_string(i);

        // Create new LocallabSpot and LocallabParamsEdited
        LocallabParams::LocallabSpot spot;
        spot.name = ppName;
        LocallabParamsEdited::LocallabSpotEdited spotEdited(false);
        spotEdited.name = peName;

        LoadUtil deserialize(keyFile, spot, pedited, spotEdited, index_str, ppVersion);

        deserialize.controlSpotSettings();
        deserialize.colorLight();
        deserialize.exposure();
        deserialize.shadowHighlight();
        deserialize.vibrance();
        deserialize.softLight();
        deserialize.blurNoise();
        deserialize.toneMapping();
        deserialize.retinex();
        deserialize.sharpening();
        deserialize.localContrast();
        deserialize.contrastByDetailLevels();
        deserialize.logEncoding();
        deserialize.mask();
        deserialize.ciecam();

        locallab.spots.push_back(spot);
        if (pedited) {
            pedited->locallab.spots.push_back(spotEdited);
        }
        ++i;
    }
}

void LoadUtil::controlSpotSettings()
{
    assignFromKeyfile(keyFile, "Locallab", "Isvisible_" + index_str, spot.isvisible, spotEdited.isvisible);
    assignFromKeyfile(keyFile, "Locallab", "PrevMethod_" + index_str, spot.prevMethod, spotEdited.prevMethod);
    assignFromKeyfile(keyFile, "Locallab", "Shape_" + index_str, spot.shape, spotEdited.shape);
    assignFromKeyfile(keyFile, "Locallab", "SpotMethod_" + index_str, spot.spotMethod, spotEdited.spotMethod);
    assignFromKeyfile(keyFile, "Locallab", "wavMethod_" + index_str, spot.wavMethod, spotEdited.wavMethod);
    assignFromKeyfile(keyFile, "Locallab", "SensiExclu_" + index_str, spot.sensiexclu, spotEdited.sensiexclu);
    assignFromKeyfile(keyFile, "Locallab", "StructExclu_" + index_str, spot.structexclu, spotEdited.structexclu);
    assignFromKeyfile(keyFile, "Locallab", "Struc_" + index_str, spot.struc, spotEdited.struc);
    assignFromKeyfile(keyFile, "Locallab", "ShapeMethod_" + index_str, spot.shapeMethod, spotEdited.shapeMethod);
    if (keyFile.has_key("Locallab", "AvoidgamutMethod_" + index_str)) {
        assignFromKeyfile(keyFile, "Locallab", "AvoidgamutMethod_" + index_str, spot.avoidgamutMethod, spotEdited.avoidgamutMethod);
        /*
        if (ppVersion < 351) {
           if(spot.avoidgamutMethod == "XYZ") {//5.10 default value
               spot.avoidgamutMethod = "MUNS";//set to Munsell only
           }
        }
        */
    } else if (keyFile.has_key("Locallab", "Avoid_" + index_str)) {
        const bool avoid = keyFile.get_boolean("Locallab", "Avoid_" + index_str);
        const bool munsell = keyFile.has_key("Locallab", "Avoidmun_" + index_str) && keyFile.get_boolean("Locallab", "Avoidmun_" + index_str);
        spot.avoidgamutMethod = avoid ? (munsell ? "MUNS" : "LAB") : "NONE";
        if (pedited) {
            spotEdited.avoidgamutMethod = true;
        }
    }
    assignFromKeyfile(keyFile, "Locallab", "Loc_" + index_str, spot.loc, spotEdited.loc);
    assignFromKeyfile(keyFile, "Locallab", "CenterX_" + index_str, spot.centerX, spotEdited.centerX);
    assignFromKeyfile(keyFile, "Locallab", "CenterY_" + index_str, spot.centerY, spotEdited.centerY);
    assignFromKeyfile(keyFile, "Locallab", "Circrad_" + index_str, spot.circrad, spotEdited.circrad);
    assignFromKeyfile(keyFile, "Locallab", "QualityMethod_" + index_str, spot.qualityMethod, spotEdited.qualityMethod);
    assignFromKeyfile(keyFile, "Locallab", "ComplexMethod_" + index_str, spot.complexMethod, spotEdited.complexMethod);
    assignFromKeyfile(keyFile, "Locallab", "Transit_" + index_str, spot.transit, spotEdited.transit);
    assignFromKeyfile(keyFile, "Locallab", "Feather_" + index_str, spot.feather, spotEdited.feather);
    assignFromKeyfile(keyFile, "Locallab", "Thresh_" + index_str, spot.thresh, spotEdited.thresh);
    assignFromKeyfile(keyFile, "Locallab", "Iter_" + index_str, spot.iter, spotEdited.iter);
    assignFromKeyfile(keyFile, "Locallab", "Balan_" + index_str, spot.balan, spotEdited.balan);
    assignFromKeyfile(keyFile, "Locallab", "Balanh_" + index_str, spot.balanh, spotEdited.balanh);
    assignFromKeyfile(keyFile, "Locallab", "Colorde_" + index_str, spot.colorde, spotEdited.colorde);
    assignFromKeyfile(keyFile, "Locallab", "Colorscope_" + index_str, spot.colorscope, spotEdited.colorscope);
    assignFromKeyfile(keyFile, "Locallab", "Avoidrad_" + index_str, spot.avoidrad, spotEdited.avoidrad);
    assignFromKeyfile(keyFile, "Locallab", "Transitweak_" + index_str, spot.transitweak, spotEdited.transitweak);
    assignFromKeyfile(keyFile, "Locallab", "Transitgrad_" + index_str, spot.transitgrad, spotEdited.transitgrad);
    assignFromKeyfile(keyFile, "Locallab", "Hishow_" + index_str, spot.hishow, spotEdited.hishow);
    assignFromKeyfile(keyFile, "Locallab", "Activ_" + index_str, spot.activ, spotEdited.activ);
    assignFromKeyfile(keyFile, "Locallab", "Avoidneg_" + index_str, spot.avoidneg, spotEdited.avoidneg);
    assignFromKeyfile(keyFile, "Locallab", "Blwh_" + index_str, spot.blwh, spotEdited.blwh);
    assignFromKeyfile(keyFile, "Locallab", "Recurs_" + index_str, spot.recurs, spotEdited.recurs);
    assignFromKeyfile(keyFile, "Locallab", "Laplac_" + index_str, spot.laplac, spotEdited.laplac);
    assignFromKeyfile(keyFile, "Locallab", "Deltae_" + index_str, spot.deltae, spotEdited.deltae);
    assignFromKeyfile(keyFile, "Locallab", "Shortc_" + index_str, spot.shortc, spotEdited.shortc);
    assignFromKeyfile(keyFile, "Locallab", "Savrest_" + index_str, spot.savrest, spotEdited.savrest);
    assignFromKeyfile(keyFile, "Locallab", "Scopemask_" + index_str, spot.scopemask, spotEdited.scopemask);
    assignFromKeyfile(keyFile, "Locallab", "Denoichmask_" + index_str, spot.denoichmask, spotEdited.denoichmask);
    assignFromKeyfile(keyFile, "Locallab", "Lumask_" + index_str, spot.lumask, spotEdited.lumask);
}

void LoadUtil::colorLight()
{
    spot.visicolor = assignFromKeyfile(keyFile, "Locallab", "Expcolor_" + index_str, spot.expcolor, spotEdited.expcolor);

    if (spot.visicolor) {
        spotEdited.visicolor = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexcolor_" + index_str, spot.complexcolor, spotEdited.complexcolor);
    assignFromKeyfile(keyFile, "Locallab", "Curvactiv_" + index_str, spot.curvactiv, spotEdited.curvactiv);
    assignFromKeyfile(keyFile, "Locallab", "Lightness_" + index_str, spot.lightness, spotEdited.lightness);
    assignFromKeyfile(keyFile, "Locallab", "Reparcol_" + index_str, spot.reparcol, spotEdited.reparcol);
    assignFromKeyfile(keyFile, "Locallab", "Gamc_" + index_str, spot.gamc, spotEdited.gamc);
    assignFromKeyfile(keyFile, "Locallab", "Contrast_" + index_str, spot.contrast, spotEdited.contrast);
    assignFromKeyfile(keyFile, "Locallab", "Chroma_" + index_str, spot.chroma, spotEdited.chroma);
    assignFromKeyfile(keyFile, "Locallab", "labgridALow_" + index_str, spot.labgridALow, spotEdited.labgridALow);
    assignFromKeyfile(keyFile, "Locallab", "labgridBLow_" + index_str, spot.labgridBLow, spotEdited.labgridBLow);
    assignFromKeyfile(keyFile, "Locallab", "labgridAHigh_" + index_str, spot.labgridAHigh, spotEdited.labgridAHigh);
    assignFromKeyfile(keyFile, "Locallab", "labgridBHigh_" + index_str, spot.labgridBHigh, spotEdited.labgridBHigh);
    assignFromKeyfile(keyFile, "Locallab", "labgridALowmerg_" + index_str, spot.labgridALowmerg, spotEdited.labgridALowmerg);
    assignFromKeyfile(keyFile, "Locallab", "labgridBLowmerg_" + index_str, spot.labgridBLowmerg, spotEdited.labgridBLowmerg);
    assignFromKeyfile(keyFile, "Locallab", "labgridAHighmerg_" + index_str, spot.labgridAHighmerg, spotEdited.labgridAHighmerg);
    assignFromKeyfile(keyFile, "Locallab", "labgridBHighmerg_" + index_str, spot.labgridBHighmerg, spotEdited.labgridBHighmerg);
    assignFromKeyfile(keyFile, "Locallab", "Strengthgrid_" + index_str, spot.strengthgrid, spotEdited.strengthgrid);
    assignFromKeyfile(keyFile, "Locallab", "Colorscope_" + index_str, spot.colorscope, spotEdited.colorscope);

    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Colorscope_" + index_str)) {
            spot.sensi = keyFile.get_integer("Locallab", "Colorscope_" + index_str);
            spotEdited.sensi = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Sensi_" + index_str, spot.sensi, spotEdited.sensi);
    }
    assignFromKeyfile(keyFile, "Locallab", "Structcol_" + index_str, spot.structcol, spotEdited.structcol);
    assignFromKeyfile(keyFile, "Locallab", "Strcol_" + index_str, spot.strcol, spotEdited.strcol);
    assignFromKeyfile(keyFile, "Locallab", "Strcolab_" + index_str, spot.strcolab, spotEdited.strcolab);
    assignFromKeyfile(keyFile, "Locallab", "Strcolh_" + index_str, spot.strcolh, spotEdited.strcolh);
    assignFromKeyfile(keyFile, "Locallab", "Angcol_" + index_str, spot.angcol, spotEdited.angcol);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.feathercol = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.feathercol = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Feathercol_" + index_str, spot.feathercol, spotEdited.feathercol);
    }
    assignFromKeyfile(keyFile, "Locallab", "Blurcolde_" + index_str, spot.blurcolde, spotEdited.blurcolde);
    assignFromKeyfile(keyFile, "Locallab", "Blurcol_" + index_str, spot.blurcol, spotEdited.blurcol);
    assignFromKeyfile(keyFile, "Locallab", "Contcol_" + index_str, spot.contcol, spotEdited.contcol);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskcol_" + index_str, spot.blendmaskcol, spotEdited.blendmaskcol);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskcol_" + index_str, spot.radmaskcol, spotEdited.radmaskcol);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskcol_" + index_str, spot.chromaskcol, spotEdited.chromaskcol);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskcol_" + index_str, spot.gammaskcol, spotEdited.gammaskcol);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskcol_" + index_str, spot.slomaskcol, spotEdited.slomaskcol);
    assignFromKeyfile(keyFile, "Locallab", "shadmaskcol_" + index_str, spot.shadmaskcol, spotEdited.shadmaskcol);
    assignFromKeyfile(keyFile, "Locallab", "strumaskcol_" + index_str, spot.strumaskcol, spotEdited.strumaskcol);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskcol_" + index_str, spot.lapmaskcol, spotEdited.lapmaskcol);
    assignFromKeyfile(keyFile, "Locallab", "QualityCurveMethod_" + index_str, spot.qualitycurveMethod, spotEdited.qualitycurveMethod);
    assignFromKeyfile(keyFile, "Locallab", "gridMethod_" + index_str, spot.gridMethod, spotEdited.gridMethod);
    assignFromKeyfile(keyFile, "Locallab", "Merg_Method_" + index_str, spot.merMethod, spotEdited.merMethod);
    assignFromKeyfile(keyFile, "Locallab", "ToneMethod_" + index_str, spot.toneMethod, spotEdited.toneMethod);
    assignFromKeyfile(keyFile, "Locallab", "mergecolMethod_" + index_str, spot.mergecolMethod, spotEdited.mergecolMethod);
    assignFromKeyfile(keyFile, "Locallab", "LLCurve_" + index_str, spot.llcurve, spotEdited.llcurve);
    assignFromKeyfile(keyFile, "Locallab", "LCCurve_" + index_str, spot.lccurve, spotEdited.lccurve);
    assignFromKeyfile(keyFile, "Locallab", "CCCurve_" + index_str, spot.cccurve, spotEdited.cccurve);
    assignFromKeyfile(keyFile, "Locallab", "CLCurve_" + index_str, spot.clcurve, spotEdited.clcurve);
    assignFromKeyfile(keyFile, "Locallab", "RGBCurve_" + index_str, spot.rgbcurve, spotEdited.rgbcurve);
    assignFromKeyfile(keyFile, "Locallab", "LHCurve_" + index_str, spot.LHcurve, spotEdited.LHcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHCurve_" + index_str, spot.HHcurve, spotEdited.HHcurve);
    assignFromKeyfile(keyFile, "Locallab", "CHCurve_" + index_str, spot.CHcurve, spotEdited.CHcurve);
    assignFromKeyfile(keyFile, "Locallab", "Invers_" + index_str, spot.invers, spotEdited.invers);
    assignFromKeyfile(keyFile, "Locallab", "Special_" + index_str, spot.special, spotEdited.special);
    assignFromKeyfile(keyFile, "Locallab", "Toolcol_" + index_str, spot.toolcol, spotEdited.toolcol);
    assignFromKeyfile(keyFile, "Locallab", "EnaColorMask_" + index_str, spot.enaColorMask, spotEdited.enaColorMask);
    assignFromKeyfile(keyFile, "Locallab", "FftColorMask_" + index_str, spot.fftColorMask, spotEdited.fftColorMask);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskCurve_" + index_str, spot.CCmaskcurve, spotEdited.CCmaskcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskCurve_" + index_str, spot.LLmaskcurve, spotEdited.LLmaskcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskCurve_" + index_str, spot.HHmaskcurve, spotEdited.HHmaskcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHhmaskCurve_" + index_str, spot.HHhmaskcurve, spotEdited.HHhmaskcurve);
    assignFromKeyfile(keyFile, "Locallab", "Softradiuscol_" + index_str, spot.softradiuscol, spotEdited.softradiuscol);
    assignFromKeyfile(keyFile, "Locallab", "Opacol_" + index_str, spot.opacol, spotEdited.opacol);
    assignFromKeyfile(keyFile, "Locallab", "Mercol_" + index_str, spot.mercol, spotEdited.mercol);
    assignFromKeyfile(keyFile, "Locallab", "Merlucol_" + index_str, spot.merlucol, spotEdited.merlucol);
    assignFromKeyfile(keyFile, "Locallab", "Conthrcol_" + index_str, spot.conthrcol, spotEdited.conthrcol);
    assignFromKeyfile(keyFile, "Locallab", "LmaskCurve_" + index_str, spot.Lmaskcurve, spotEdited.Lmaskcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskcolCurvewav_" + index_str, spot.LLmaskcolcurvewav, spotEdited.LLmaskcolcurvewav);

    if (keyFile.has_key("Locallab", "CSThresholdcol_" + index_str)) {
        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "CSThresholdcol_" + index_str);

        if (thresh.size() >= 4) {
            spot.csthresholdcol.setValues(thresh[0], thresh[1], min(thresh[2], 10), min(thresh[3], 10));
        }

        spotEdited.csthresholdcol = true;
    }
    assignFromKeyfile(keyFile, "Locallab", "Recothresc_" + index_str, spot.recothresc, spotEdited.recothresc);
    assignFromKeyfile(keyFile, "Locallab", "Lowthresc_" + index_str, spot.lowthresc, spotEdited.lowthresc);
    assignFromKeyfile(keyFile, "Locallab", "Higthresc_" + index_str, spot.higthresc, spotEdited.higthresc);
    assignFromKeyfile(keyFile, "Locallab", "Decayc_" + index_str, spot.decayc, spotEdited.decayc);
}

void LoadUtil::exposure()
{
    spot.visiexpose = assignFromKeyfile(keyFile, "Locallab", "Expexpose_" + index_str, spot.expexpose, spotEdited.expexpose);

    if (spot.visiexpose) {
        spotEdited.visiexpose = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Laplacexp_" + index_str, spot.laplacexp, spotEdited.laplacexp);
    assignFromKeyfile(keyFile, "Locallab", "Complexexpose_" + index_str, spot.complexexpose, spotEdited.complexexpose);
    if (ppVersion <= 350 && spot.laplacexp > 0.f) { // Contrast attenuator moved to "advanced" after 5.10. Set complexity to "advanced" if Contrast attenuator is in use.
        spot.complexexpose = 0;
        spotEdited.complexexpose = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Expcomp_" + index_str, spot.expcomp, spotEdited.expcomp);
    assignFromKeyfile(keyFile, "Locallab", "Hlcompr_" + index_str, spot.hlcompr, spotEdited.hlcompr);
    assignFromKeyfile(keyFile, "Locallab", "Hlcomprthresh_" + index_str, spot.hlcomprthresh, spotEdited.hlcomprthresh);
    assignFromKeyfile(keyFile, "Locallab", "Black_" + index_str, spot.black, spotEdited.black);
    assignFromKeyfile(keyFile, "Locallab", "Shadex_" + index_str, spot.shadex, spotEdited.shadex);
    assignFromKeyfile(keyFile, "Locallab", "Shcompr_" + index_str, spot.shcompr, spotEdited.shcompr);
    assignFromKeyfile(keyFile, "Locallab", "Expchroma_" + index_str, spot.expchroma, spotEdited.expchroma);
    assignFromKeyfile(keyFile, "Locallab", "Sensiex_" + index_str, spot.sensiex, spotEdited.sensiex);
    assignFromKeyfile(keyFile, "Locallab", "Structexp_" + index_str, spot.structexp, spotEdited.structexp);
    assignFromKeyfile(keyFile, "Locallab", "Blurexpde_" + index_str, spot.blurexpde, spotEdited.blurexpde);
    assignFromKeyfile(keyFile, "Locallab", "Gamex_" + index_str, spot.gamex, spotEdited.gamex);
    assignFromKeyfile(keyFile, "Locallab", "Strexp_" + index_str, spot.strexp, spotEdited.strexp);
    assignFromKeyfile(keyFile, "Locallab", "Angexp_" + index_str, spot.angexp, spotEdited.angexp);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.featherexp = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.featherexp = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Featherexp_" + index_str, spot.featherexp, spotEdited.featherexp);
    }
    assignFromKeyfile(keyFile, "Locallab", "ExCurve_" + index_str, spot.excurve, spotEdited.excurve);
    assignFromKeyfile(keyFile, "Locallab", "Norm_" + index_str, spot.norm, spotEdited.norm);
    assignFromKeyfile(keyFile, "Locallab", "Inversex_" + index_str, spot.inversex, spotEdited.inversex);
    assignFromKeyfile(keyFile, "Locallab", "EnaExpMask_" + index_str, spot.enaExpMask, spotEdited.enaExpMask);
    assignFromKeyfile(keyFile, "Locallab", "EnaExpMaskaft_" + index_str, spot.enaExpMaskaft, spotEdited.enaExpMaskaft);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskexpCurve_" + index_str, spot.CCmaskexpcurve, spotEdited.CCmaskexpcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskexpCurve_" + index_str, spot.LLmaskexpcurve, spotEdited.LLmaskexpcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskexpCurve_" + index_str, spot.HHmaskexpcurve, spotEdited.HHmaskexpcurve);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskexp_" + index_str, spot.blendmaskexp, spotEdited.blendmaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskexp_" + index_str, spot.radmaskexp, spotEdited.radmaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskexp_" + index_str, spot.chromaskexp, spotEdited.chromaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskexp_" + index_str, spot.gammaskexp, spotEdited.gammaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskexp_" + index_str, spot.slomaskexp, spotEdited.slomaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskexp_" + index_str, spot.lapmaskexp, spotEdited.lapmaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Strmaskexp_" + index_str, spot.strmaskexp, spotEdited.strmaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Angmaskexp_" + index_str, spot.angmaskexp, spotEdited.angmaskexp);
    assignFromKeyfile(keyFile, "Locallab", "Softradiusexp_" + index_str, spot.softradiusexp, spotEdited.softradiusexp);
    assignFromKeyfile(keyFile, "Locallab", "LmaskexpCurve_" + index_str, spot.Lmaskexpcurve, spotEdited.Lmaskexpcurve);
    assignFromKeyfile(keyFile, "Locallab", "ExpMethod_" + index_str, spot.expMethod, spotEdited.expMethod);
    assignFromKeyfile(keyFile, "Locallab", "ExnoiseMethod_" + index_str, spot.exnoiseMethod, spotEdited.exnoiseMethod);
    //    assignFromKeyfile(keyFile, "Locallab", "Laplacexp_" + index_str, spot.laplacexp, spotEdited.laplacexp);
    assignFromKeyfile(keyFile, "Locallab", "Reparexp_" + index_str, spot.reparexp, spotEdited.reparexp);
    assignFromKeyfile(keyFile, "Locallab", "Balanexp_" + index_str, spot.balanexp, spotEdited.balanexp);
    assignFromKeyfile(keyFile, "Locallab", "Linearexp_" + index_str, spot.linear, spotEdited.linear);
    assignFromKeyfile(keyFile, "Locallab", "Gamm_" + index_str, spot.gamm, spotEdited.gamm);
    assignFromKeyfile(keyFile, "Locallab", "Fatamount_" + index_str, spot.fatamount, spotEdited.fatamount);
    assignFromKeyfile(keyFile, "Locallab", "Fatdetail_" + index_str, spot.fatdetail, spotEdited.fatdetail);
    assignFromKeyfile(keyFile, "Locallab", "Fatsatur_" + index_str, spot.fatsatur, spotEdited.fatsatur);
    assignFromKeyfile(keyFile, "Locallab", "Fatanchor_" + index_str, spot.fatanchor, spotEdited.fatanchor);
    assignFromKeyfile(keyFile, "Locallab", "Fatlevel_" + index_str, spot.fatlevel, spotEdited.fatlevel);
    assignFromKeyfile(keyFile, "Locallab", "Recothrese_" + index_str, spot.recothrese, spotEdited.recothrese);
    assignFromKeyfile(keyFile, "Locallab", "Lowthrese_" + index_str, spot.lowthrese, spotEdited.lowthrese);
    assignFromKeyfile(keyFile, "Locallab", "Higthrese_" + index_str, spot.higthrese, spotEdited.higthrese);
    assignFromKeyfile(keyFile, "Locallab", "Decaye_" + index_str, spot.decaye, spotEdited.decaye);
}

void LoadUtil::shadowHighlight()
{
    spot.visishadhigh = assignFromKeyfile(keyFile, "Locallab", "Expshadhigh_" + index_str, spot.expshadhigh, spotEdited.expshadhigh);

    if (spot.visishadhigh) {
        spotEdited.visishadhigh = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexshadhigh_" + index_str, spot.complexshadhigh, spotEdited.complexshadhigh);
    assignFromKeyfile(keyFile, "Locallab", "ShMethod_" + index_str, spot.shMethod, spotEdited.shMethod);
    assignFromKeyfile(keyFile, "Locallab", "GhsMethod_" + index_str, spot.ghsMethod, spotEdited.ghsMethod);
    assignFromKeyfile(keyFile, "Locallab", "GhsMatmet_" + index_str, spot.ghsMatmet, spotEdited.ghsMatmet);
    assignFromKeyfile(keyFile, "Locallab", "GhsMode_" + index_str, spot.ghsMode, spotEdited.ghsMode);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_D_" + index_str, spot.ghs_D, spotEdited.ghs_D);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_mtfstr_" + index_str, spot.ghs_mtfstr, spotEdited.ghs_mtfstr);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_slope_" + index_str, spot.ghs_slope, spotEdited.ghs_slope);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_chro_" + index_str, spot.ghs_chro, spotEdited.ghs_chro);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_B_" + index_str, spot.ghs_B, spotEdited.ghs_B);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_SP_" + index_str, spot.ghs_SP, spotEdited.ghs_SP);
    assignFromKeyfile(keyFile, "Locallab", "SPAutoRadius_" + index_str, spot.SPAutoRadius, spotEdited.SPAutoRadius);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_LP_" + index_str, spot.ghs_LP, spotEdited.ghs_LP);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_HP_" + index_str, spot.ghs_HP, spotEdited.ghs_HP);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_LC_" + index_str, spot.ghs_LC, spotEdited.ghs_LC);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_MID_" + index_str, spot.ghs_MID, spotEdited.ghs_MID);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_BLP_" + index_str, spot.ghs_BLP, spotEdited.ghs_BLP);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_HLP_" + index_str, spot.ghs_HLP, spotEdited.ghs_HLP);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_autobw_" + index_str, spot.ghs_autobw, spotEdited.ghs_autobw);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_agx_" + index_str, spot.ghs_agx, spotEdited.ghs_agx);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_mtf_" + index_str, spot.ghs_mtf, spotEdited.ghs_mtf);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_smooth_" + index_str, spot.ghs_smooth, spotEdited.ghs_smooth);
    assignFromKeyfile(keyFile, "Locallab", "Ghs_inv_" + index_str, spot.ghs_inv, spotEdited.ghs_inv);

    assignFromKeyfile(keyFile, "Locallab", "Mich_exp_" + index_str, spot.mich_exp, spotEdited.mich_exp);
    assignFromKeyfile(keyFile, "Locallab", "Mich_spar_" + index_str, spot.mich_spar, spotEdited.mich_spar);
    assignFromKeyfile(keyFile, "Locallab", "Mich_kpar_" + index_str, spot.mich_kpar, spotEdited.mich_kpar);
    assignFromKeyfile(keyFile, "Locallab", "Mich_sat_" + index_str, spot.mich_sat, spotEdited.mich_sat);
    assignFromKeyfile(keyFile, "Locallab", "Mich_out_" + index_str, spot.mich_out, spotEdited.mich_out);
    assignFromKeyfile(keyFile, "Locallab", "Mich_black_" + index_str, spot.mich_black, spotEdited.mich_black);
    assignFromKeyfile(keyFile, "Locallab", "Mich_white_" + index_str, spot.mich_white, spotEdited.mich_white);
    assignFromKeyfile(keyFile, "Locallab", "Mich_high_" + index_str, spot.mich_high, spotEdited.mich_high);
    assignFromKeyfile(keyFile, "Locallab", "Mich_jdx_" + index_str, spot.mich_jdx, spotEdited.mich_jdx);
    assignFromKeyfile(keyFile, "Locallab", "Mich_mtf_" + index_str, spot.mich_mtf, spotEdited.mich_mtf);

    for (int j = 0; j < 6; j ++) {
        assignFromKeyfile(keyFile, "Locallab", "Multsh" + std::to_string(j) + "_" + index_str, spot.multsh[j], spotEdited.multsh[j]);
    }

    assignFromKeyfile(keyFile, "Locallab", "Expshadhigh_" + index_str, spot.expshadhigh, spotEdited.expshadhigh);
    assignFromKeyfile(keyFile, "Locallab", "highlights_" + index_str, spot.highlights, spotEdited.highlights);
    assignFromKeyfile(keyFile, "Locallab", "h_tonalwidth_" + index_str, spot.h_tonalwidth, spotEdited.h_tonalwidth);
    assignFromKeyfile(keyFile, "Locallab", "shadows_" + index_str, spot.shadows, spotEdited.shadows);
    assignFromKeyfile(keyFile, "Locallab", "s_tonalwidth_" + index_str, spot.s_tonalwidth, spotEdited.s_tonalwidth);
    assignFromKeyfile(keyFile, "Locallab", "sh_radius_" + index_str, spot.sh_radius, spotEdited.sh_radius);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Colorscope_" + index_str)) {
            spot.sensihs = keyFile.get_integer("Locallab", "Colorscope_" + index_str);
            spotEdited.sensihs = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "sensihs_" + index_str, spot.sensihs, spotEdited.sensihs);
    }
    assignFromKeyfile(keyFile, "Locallab", "EnaSHMask_" + index_str, spot.enaSHMask, spotEdited.enaSHMask);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskSHCurve_" + index_str, spot.CCmaskSHcurve, spotEdited.CCmaskSHcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskSHCurve_" + index_str, spot.LLmaskSHcurve, spotEdited.LLmaskSHcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskSHCurve_" + index_str, spot.HHmaskSHcurve, spotEdited.HHmaskSHcurve);
    assignFromKeyfile(keyFile, "Locallab", "BlendmaskSH_" + index_str, spot.blendmaskSH, spotEdited.blendmaskSH);
    assignFromKeyfile(keyFile, "Locallab", "RadmaskSH_" + index_str, spot.radmaskSH, spotEdited.radmaskSH);
    assignFromKeyfile(keyFile, "Locallab", "BlurSHde_" + index_str, spot.blurSHde, spotEdited.blurSHde);
    assignFromKeyfile(keyFile, "Locallab", "StrSH_" + index_str, spot.strSH, spotEdited.strSH);
    assignFromKeyfile(keyFile, "Locallab", "AngSH_" + index_str, spot.angSH, spotEdited.angSH);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.featherSH = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.featherSH = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "FeatherSH_" + index_str, spot.featherSH, spotEdited.featherSH);
    }

    assignFromKeyfile(keyFile, "Locallab", "Inverssh_" + index_str, spot.inverssh, spotEdited.inverssh);
    assignFromKeyfile(keyFile, "Locallab", "ChromaskSH_" + index_str, spot.chromaskSH, spotEdited.chromaskSH);
    assignFromKeyfile(keyFile, "Locallab", "GammaskSH_" + index_str, spot.gammaskSH, spotEdited.gammaskSH);
    assignFromKeyfile(keyFile, "Locallab", "SlomaskSH_" + index_str, spot.slomaskSH, spotEdited.slomaskSH);
    assignFromKeyfile(keyFile, "Locallab", "LapmaskSH_" + index_str, spot.lapmaskSH, spotEdited.lapmaskSH);
    assignFromKeyfile(keyFile, "Locallab", "DetailSH_" + index_str, spot.detailSH, spotEdited.detailSH);
    assignFromKeyfile(keyFile, "Locallab", "TePivot_" + index_str, spot.tePivot, spotEdited.tePivot);
    assignFromKeyfile(keyFile, "Locallab", "Reparsh_" + index_str, spot.reparsh, spotEdited.reparsh);
    assignFromKeyfile(keyFile, "Locallab", "LmaskSHCurve_" + index_str, spot.LmaskSHcurve, spotEdited.LmaskSHcurve);
    assignFromKeyfile(keyFile, "Locallab", "FatamountSH_" + index_str, spot.fatamountSH, spotEdited.fatamountSH);
    assignFromKeyfile(keyFile, "Locallab", "FatanchorSH_" + index_str, spot.fatanchorSH, spotEdited.fatanchorSH);
    assignFromKeyfile(keyFile, "Locallab", "GamSH_" + index_str, spot.gamSH, spotEdited.gamSH);
    assignFromKeyfile(keyFile, "Locallab", "SloSH_" + index_str, spot.sloSH, spotEdited.sloSH);
    assignFromKeyfile(keyFile, "Locallab", "Recothress_" + index_str, spot.recothress, spotEdited.recothress);
    assignFromKeyfile(keyFile, "Locallab", "Lowthress_" + index_str, spot.lowthress, spotEdited.lowthress);
    assignFromKeyfile(keyFile, "Locallab", "Higthress_" + index_str, spot.higthress, spotEdited.higthress);
    assignFromKeyfile(keyFile, "Locallab", "Decays_" + index_str, spot.decays, spotEdited.decays);
}

void LoadUtil::vibrance()
{
    spot.visivibrance = assignFromKeyfile(keyFile, "Locallab", "Expvibrance_" + index_str, spot.expvibrance, spotEdited.expvibrance);

    if (spot.visivibrance) {
        spotEdited.visivibrance = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexvibrance_" + index_str, spot.complexvibrance, spotEdited.complexvibrance);
    assignFromKeyfile(keyFile, "Locallab", "Saturated_" + index_str, spot.saturated, spotEdited.saturated);
    assignFromKeyfile(keyFile, "Locallab", "Pastels_" + index_str, spot.pastels, spotEdited.pastels);
    assignFromKeyfile(keyFile, "Locallab", "Vibgam_" + index_str, spot.vibgam, spotEdited.vibgam);
    assignFromKeyfile(keyFile, "Locallab", "Warm_" + index_str, spot.warm, spotEdited.warm);

    if (keyFile.has_key("Locallab", "PSThreshold_" + index_str)) {
        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "PSThreshold_" + index_str);

        if (thresh.size() >= 2) {
            spot.psthreshold.setValues(thresh[0], thresh[1]);
        }

        spotEdited.psthreshold = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "ProtectSkins_" + index_str, spot.protectskins, spotEdited.protectskins);
    assignFromKeyfile(keyFile, "Locallab", "AvoidColorShift_" + index_str, spot.avoidcolorshift, spotEdited.avoidcolorshift);
    assignFromKeyfile(keyFile, "Locallab", "PastSatTog_" + index_str, spot.pastsattog, spotEdited.pastsattog);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Colorscope_" + index_str)) {
            spot.sensiv = keyFile.get_integer("Locallab", "Colorscope_" + index_str);
            spotEdited.sensiv = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Sensiv_" + index_str, spot.sensiv, spotEdited.sensiv);
    }
    assignFromKeyfile(keyFile, "Locallab", "SkinTonesCurve_" + index_str, spot.skintonescurve, spotEdited.skintonescurve);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskvibCurve_" + index_str, spot.CCmaskvibcurve, spotEdited.CCmaskvibcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskvibCurve_" + index_str, spot.LLmaskvibcurve, spotEdited.LLmaskvibcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskvibCurve_" + index_str, spot.HHmaskvibcurve, spotEdited.HHmaskvibcurve);
    assignFromKeyfile(keyFile, "Locallab", "EnavibMask_" + index_str, spot.enavibMask, spotEdited.enavibMask);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskvib_" + index_str, spot.blendmaskvib, spotEdited.blendmaskvib);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskvib_" + index_str, spot.radmaskvib, spotEdited.radmaskvib);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskvib_" + index_str, spot.chromaskvib, spotEdited.chromaskvib);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskvib_" + index_str, spot.gammaskvib, spotEdited.gammaskvib);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskvib_" + index_str, spot.slomaskvib, spotEdited.slomaskvib);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskvib_" + index_str, spot.lapmaskvib, spotEdited.lapmaskvib);
    assignFromKeyfile(keyFile, "Locallab", "Strvib_" + index_str, spot.strvib, spotEdited.strvib);
    assignFromKeyfile(keyFile, "Locallab", "Strvibab_" + index_str, spot.strvibab, spotEdited.strvibab);
    assignFromKeyfile(keyFile, "Locallab", "Strvibh_" + index_str, spot.strvibh, spotEdited.strvibh);
    assignFromKeyfile(keyFile, "Locallab", "Angvib_" + index_str, spot.angvib, spotEdited.angvib);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.feathervib = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.feathervib = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Feathervib_" + index_str, spot.feathervib, spotEdited.feathervib);
    }

    assignFromKeyfile(keyFile, "Locallab", "LmaskvibCurve_" + index_str, spot.Lmaskvibcurve, spotEdited.Lmaskvibcurve);
    assignFromKeyfile(keyFile, "Locallab", "Recothresv_" + index_str, spot.recothresv, spotEdited.recothresv);
    assignFromKeyfile(keyFile, "Locallab", "Lowthresv_" + index_str, spot.lowthresv, spotEdited.lowthresv);
    assignFromKeyfile(keyFile, "Locallab", "Higthresv_" + index_str, spot.higthresv, spotEdited.higthresv);
    assignFromKeyfile(keyFile, "Locallab", "Decayv_" + index_str, spot.decayv, spotEdited.decayv);
}

void LoadUtil::softLight()
{
    spot.visisoft = assignFromKeyfile(keyFile, "Locallab", "Expsoft_" + index_str, spot.expsoft, spotEdited.expsoft);

    if (spot.visisoft) {
        spotEdited.visisoft = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexsoft_" + index_str, spot.complexsoft, spotEdited.complexsoft);
    assignFromKeyfile(keyFile, "Locallab", "Streng_" + index_str, spot.streng, spotEdited.streng);
    assignFromKeyfile(keyFile, "Locallab", "Sensisf_" + index_str, spot.sensisf, spotEdited.sensisf);
    assignFromKeyfile(keyFile, "Locallab", "Laplace_" + index_str, spot.laplace, spotEdited.laplace);
    assignFromKeyfile(keyFile, "Locallab", "SoftMethod_" + index_str, spot.softMethod, spotEdited.softMethod);
}

void LoadUtil::blurNoise()
{
    spot.visiblur = assignFromKeyfile(keyFile, "Locallab", "Expblur_" + index_str, spot.expblur, spotEdited.expblur);

    if (spot.visiblur) {
        spotEdited.visiblur = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexblur_" + index_str, spot.complexblur, spotEdited.complexblur);
    assignFromKeyfile(keyFile, "Locallab", "Radius_" + index_str, spot.radius, spotEdited.radius);
    assignFromKeyfile(keyFile, "Locallab", "Strength_" + index_str, spot.strength, spotEdited.strength);
    assignFromKeyfile(keyFile, "Locallab", "Sensibn_" + index_str, spot.sensibn, spotEdited.sensibn);
    assignFromKeyfile(keyFile, "Locallab", "Iteramed_" + index_str, spot.itera, spotEdited.itera);
    assignFromKeyfile(keyFile, "Locallab", "Guidbl_" + index_str, spot.guidbl, spotEdited.guidbl);
    assignFromKeyfile(keyFile, "Locallab", "Strbl_" + index_str, spot.strbl, spotEdited.strbl);
    assignFromKeyfile(keyFile, "Locallab", "Recothres_" + index_str, spot.recothres, spotEdited.recothres);
    assignFromKeyfile(keyFile, "Locallab", "Lowthres_" + index_str, spot.lowthres, spotEdited.lowthres);
    assignFromKeyfile(keyFile, "Locallab", "Higthres_" + index_str, spot.higthres, spotEdited.higthres);
    assignFromKeyfile(keyFile, "Locallab", "Recothresd_" + index_str, spot.recothresd, spotEdited.recothresd);
    assignFromKeyfile(keyFile, "Locallab", "Lowthresd_" + index_str, spot.lowthresd, spotEdited.lowthresd);
    assignFromKeyfile(keyFile, "Locallab", "Midthresd_" + index_str, spot.midthresd, spotEdited.midthresd);
    assignFromKeyfile(keyFile, "Locallab", "Midthresdch_" + index_str, spot.midthresdch, spotEdited.midthresdch);
    assignFromKeyfile(keyFile, "Locallab", "Higthresd_" + index_str, spot.higthresd, spotEdited.higthresd);
    assignFromKeyfile(keyFile, "Locallab", "Decayd_" + index_str, spot.decayd, spotEdited.decayd);
    assignFromKeyfile(keyFile, "Locallab", "Isogr_" + index_str, spot.isogr, spotEdited.isogr);
    assignFromKeyfile(keyFile, "Locallab", "Strengr_" + index_str, spot.strengr, spotEdited.strengr);
    assignFromKeyfile(keyFile, "Locallab", "Scalegr_" + index_str, spot.scalegr, spotEdited.scalegr);
    assignFromKeyfile(keyFile, "Locallab", "Divgr_" + index_str, spot.divgr, spotEdited.divgr);
    assignFromKeyfile(keyFile, "Locallab", "Epsbl_" + index_str, spot.epsbl, spotEdited.epsbl);
    assignFromKeyfile(keyFile, "Locallab", "BlMethod_" + index_str, spot.blMethod, spotEdited.blMethod);
    assignFromKeyfile(keyFile, "Locallab", "ChroMethod_" + index_str, spot.chroMethod, spotEdited.chroMethod);
    assignFromKeyfile(keyFile, "Locallab", "QuaMethod_" + index_str, spot.quamethod, spotEdited.quamethod);
    assignFromKeyfile(keyFile, "Locallab", "BlurMethod_" + index_str, spot.blurMethod, spotEdited.blurMethod);
    assignFromKeyfile(keyFile, "Locallab", "Usemaskb_" + index_str, spot.usemask, spotEdited.usemask);
    assignFromKeyfile(keyFile, "Locallab", "Invmaskd_" + index_str, spot.invmaskd, spotEdited.invmaskd);
    assignFromKeyfile(keyFile, "Locallab", "Invmask_" + index_str, spot.invmask, spotEdited.invmask);
    assignFromKeyfile(keyFile, "Locallab", "Levelthr_" + index_str, spot.levelthr, spotEdited.levelthr);
    assignFromKeyfile(keyFile, "Locallab", "Lnoiselow_" + index_str, spot.lnoiselow, spotEdited.lnoiselow);
    assignFromKeyfile(keyFile, "Locallab", "Levelthrlow_" + index_str, spot.levelthrlow, spotEdited.levelthrlow);
    assignFromKeyfile(keyFile, "Locallab", "MedMethod_" + index_str, spot.medMethod, spotEdited.medMethod);
    assignFromKeyfile(keyFile, "Locallab", "activlum_" + index_str, spot.activlum, spotEdited.activlum);
    for (int j = 0; j < 21; j ++) {
        assignFromKeyfile(keyFile, "Locallab", "Madlsav" + std::to_string(j) + "_" + index_str, spot.madlsav[j], spotEdited.madlsav[j]);
    }               
    assignFromKeyfile(keyFile, "Locallab", "noiselumf_" + index_str, spot.noiselumf, spotEdited.noiselumf);
    assignFromKeyfile(keyFile, "Locallab", "noiselumf0_" + index_str, spot.noiselumf0, spotEdited.noiselumf0);
    assignFromKeyfile(keyFile, "Locallab", "noiselumf2_" + index_str, spot.noiselumf2, spotEdited.noiselumf2);
    assignFromKeyfile(keyFile, "Locallab", "noiselumc_" + index_str, spot.noiselumc, spotEdited.noiselumc);
    assignFromKeyfile(keyFile, "Locallab", "noiselumdetail_" + index_str, spot.noiselumdetail, spotEdited.noiselumdetail);
    assignFromKeyfile(keyFile, "Locallab", "noiselequal_" + index_str, spot.noiselequal, spotEdited.noiselequal);
    assignFromKeyfile(keyFile, "Locallab", "noisegam_" + index_str, spot.noisegam, spotEdited.noisegam);
    assignFromKeyfile(keyFile, "Locallab", "noisechrof_" + index_str, spot.noisechrof, spotEdited.noisechrof);
    assignFromKeyfile(keyFile, "Locallab", "noisechroc_" + index_str, spot.noisechroc, spotEdited.noisechroc);
    assignFromKeyfile(keyFile, "Locallab", "noisechrodetail_" + index_str, spot.noisechrodetail, spotEdited.noisechrodetail);
    assignFromKeyfile(keyFile, "Locallab", "Adjblur_" + index_str, spot.adjblur, spotEdited.adjblur);
    assignFromKeyfile(keyFile, "Locallab", "Bilateral_" + index_str, spot.bilateral, spotEdited.bilateral);
    assignFromKeyfile(keyFile, "Locallab", "Nlstr_" + index_str, spot.nlstr, spotEdited.nlstr);
    assignFromKeyfile(keyFile, "Locallab", "Nldet_" + index_str, spot.nldet, spotEdited.nldet);
    assignFromKeyfile(keyFile, "Locallab", "Nlpat_" + index_str, spot.nlpat, spotEdited.nlpat);
    assignFromKeyfile(keyFile, "Locallab", "Nlrad_" + index_str, spot.nlrad, spotEdited.nlrad);
    assignFromKeyfile(keyFile, "Locallab", "Nlgam_" + index_str, spot.nlgam, spotEdited.nlgam);
    assignFromKeyfile(keyFile, "Locallab", "Nliter_" + index_str, spot.nliter, spotEdited.nliter);
    assignFromKeyfile(keyFile, "Locallab", "Sensiden_" + index_str, spot.sensiden, spotEdited.sensiden);
    assignFromKeyfile(keyFile, "Locallab", "Reparden_" + index_str, spot.reparden, spotEdited.reparden);
    assignFromKeyfile(keyFile, "Locallab", "Detailthr_" + index_str, spot.detailthr, spotEdited.detailthr);
    assignFromKeyfile(keyFile, "Locallab", "LocwavCurveden_" + index_str, spot.locwavcurveden, spotEdited.locwavcurveden);
    assignFromKeyfile(keyFile, "Locallab", "LocwavCurvehue_" + index_str, spot.locwavcurvehue, spotEdited.locwavcurvehue);
    assignFromKeyfile(keyFile, "Locallab", "LocwavCurvehuecont_" + index_str, spot.locwavcurvehuecont, spotEdited.locwavcurvehuecont);
    assignFromKeyfile(keyFile, "Locallab", "Showmasktyp_" + index_str, spot.showmaskblMethodtyp, spotEdited.showmaskblMethodtyp);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskblCurve_" + index_str, spot.CCmaskblcurve, spotEdited.CCmaskblcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskblCurve_" + index_str, spot.LLmaskblcurve, spotEdited.LLmaskblcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskblCurve_" + index_str, spot.HHmaskblcurve, spotEdited.HHmaskblcurve);
    assignFromKeyfile(keyFile, "Locallab", "EnablMask_" + index_str, spot.enablMask, spotEdited.enablMask);
    assignFromKeyfile(keyFile, "Locallab", "Fftwbl_" + index_str, spot.fftwbl, spotEdited.fftwbl);
    assignFromKeyfile(keyFile, "Locallab", "Invbl_" + index_str, spot.invbl, spotEdited.invbl);
    assignFromKeyfile(keyFile, "Locallab", "Toolbl_" + index_str, spot.toolbl, spotEdited.toolbl);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskbl_" + index_str, spot.blendmaskbl, spotEdited.blendmaskbl);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskbl_" + index_str, spot.radmaskbl, spotEdited.radmaskbl);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskbl_" + index_str, spot.chromaskbl, spotEdited.chromaskbl);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskbl_" + index_str, spot.gammaskbl, spotEdited.gammaskbl);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskbl_" + index_str, spot.slomaskbl, spotEdited.slomaskbl);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskbl_" + index_str, spot.lapmaskbl, spotEdited.lapmaskbl);
    assignFromKeyfile(keyFile, "Locallab", "shadmaskbl_" + index_str, spot.shadmaskbl, spotEdited.shadmaskbl);
    assignFromKeyfile(keyFile, "Locallab", "shadmaskblsha_" + index_str, spot.shadmaskblsha, spotEdited.shadmaskblsha);
    assignFromKeyfile(keyFile, "Locallab", "strumaskbl_" + index_str, spot.strumaskbl, spotEdited.strumaskbl);
    assignFromKeyfile(keyFile, "Locallab", "LmaskblCurve_" + index_str, spot.Lmaskblcurve, spotEdited.Lmaskblcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskblCurvewav_" + index_str, spot.LLmaskblcurvewav, spotEdited.LLmaskblcurvewav);
    assignFromKeyfile(keyFile, "Locallab", "denocontrast_" + index_str, spot.denocontrast, spotEdited.denocontrast);
    assignFromKeyfile(keyFile, "Locallab", "denoAutocontrast_" + index_str, spot.denoAutocontrast, spotEdited.denoAutocontrast);
    assignFromKeyfile(keyFile, "Locallab", "contrshow_" + index_str, spot.contrshow, spotEdited.contrshow);
    assignFromKeyfile(keyFile, "Locallab", "lockmadl_" + index_str, spot.lockmadl, spotEdited.lockmadl);
    assignFromKeyfile(keyFile, "Locallab", "madllock_" + index_str, spot.madllock, spotEdited.madllock);
    assignFromKeyfile(keyFile, "Locallab", "enacontrast_" + index_str, spot.enacontrast, spotEdited.enacontrast);
    assignFromKeyfile(keyFile, "Locallab", "denoratio_" + index_str, spot.denoratio, spotEdited.denoratio);
    assignFromKeyfile(keyFile, "Locallab", "denomask_" + index_str, spot.denomask, spotEdited.denomask);

    if (keyFile.has_key("Locallab", "CSThresholdblur_" + index_str)) {
        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "CSThresholdblur_" + index_str);

        if (thresh.size() >= 4) {
            spot.csthresholdblur.setValues(thresh[0], thresh[1], min(thresh[2], 10), min(thresh[3], 10));
        }

        spotEdited.csthresholdblur = true;
    }
}

void LoadUtil::toneMapping()
{
    spot.visitonemap = assignFromKeyfile(keyFile, "Locallab", "Exptonemap_" + index_str, spot.exptonemap, spotEdited.exptonemap);

    if (spot.visitonemap) {
        spotEdited.visitonemap = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complextonemap_" + index_str, spot.complextonemap, spotEdited.complextonemap);
    assignFromKeyfile(keyFile, "Locallab", "Stren_" + index_str, spot.stren, spotEdited.stren);
    assignFromKeyfile(keyFile, "Locallab", "Gamma_" + index_str, spot.gamma, spotEdited.gamma);
    assignFromKeyfile(keyFile, "Locallab", "Estop_" + index_str, spot.estop, spotEdited.estop);
    assignFromKeyfile(keyFile, "Locallab", "Scaltm_" + index_str, spot.scaltm, spotEdited.scaltm);
    assignFromKeyfile(keyFile, "Locallab", "Repartm_" + index_str, spot.repartm, spotEdited.repartm);
    assignFromKeyfile(keyFile, "Locallab", "Rewei_" + index_str, spot.rewei, spotEdited.rewei);
    assignFromKeyfile(keyFile, "Locallab", "Satur_" + index_str, spot.satur, spotEdited.satur);
    assignFromKeyfile(keyFile, "Locallab", "Sensitm_" + index_str, spot.sensitm, spotEdited.sensitm);
    assignFromKeyfile(keyFile, "Locallab", "Softradiustm_" + index_str, spot.softradiustm, spotEdited.softradiustm);
    assignFromKeyfile(keyFile, "Locallab", "Amount_" + index_str, spot.amount, spotEdited.amount);
    assignFromKeyfile(keyFile, "Locallab", "Equiltm_" + index_str, spot.equiltm, spotEdited.equiltm);
    assignFromKeyfile(keyFile, "Locallab", "CCmasktmCurve_" + index_str, spot.CCmasktmcurve, spotEdited.CCmasktmcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmasktmCurve_" + index_str, spot.LLmasktmcurve, spotEdited.LLmasktmcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmasktmCurve_" + index_str, spot.HHmasktmcurve, spotEdited.HHmasktmcurve);
    assignFromKeyfile(keyFile, "Locallab", "EnatmMask_" + index_str, spot.enatmMask, spotEdited.enatmMask);
    assignFromKeyfile(keyFile, "Locallab", "EnatmMaskaft_" + index_str, spot.enatmMaskaft, spotEdited.enatmMaskaft);
    assignFromKeyfile(keyFile, "Locallab", "Blendmasktm_" + index_str, spot.blendmasktm, spotEdited.blendmasktm);
    assignFromKeyfile(keyFile, "Locallab", "Radmasktm_" + index_str, spot.radmasktm, spotEdited.radmasktm);
    assignFromKeyfile(keyFile, "Locallab", "Chromasktm_" + index_str, spot.chromasktm, spotEdited.chromasktm);
    assignFromKeyfile(keyFile, "Locallab", "Gammasktm_" + index_str, spot.gammasktm, spotEdited.gammasktm);
    assignFromKeyfile(keyFile, "Locallab", "Slomasktm_" + index_str, spot.slomasktm, spotEdited.slomasktm);
    assignFromKeyfile(keyFile, "Locallab", "Lapmasktm_" + index_str, spot.lapmasktm, spotEdited.lapmasktm);
    assignFromKeyfile(keyFile, "Locallab", "LmasktmCurve_" + index_str, spot.Lmasktmcurve, spotEdited.Lmasktmcurve);
    assignFromKeyfile(keyFile, "Locallab", "Recothrest_" + index_str, spot.recothrest, spotEdited.recothrest);
    assignFromKeyfile(keyFile, "Locallab", "Lowthrest_" + index_str, spot.lowthrest, spotEdited.lowthrest);
    assignFromKeyfile(keyFile, "Locallab", "Higthrest_" + index_str, spot.higthrest, spotEdited.higthrest);
    assignFromKeyfile(keyFile, "Locallab", "Decayt_" + index_str, spot.decayt, spotEdited.decayt);
}

void LoadUtil::retinex()
{
    spot.visireti = assignFromKeyfile(keyFile, "Locallab", "Expreti_" + index_str, spot.expreti, spotEdited.expreti);

    if (spot.visireti) {
        spotEdited.visireti = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexreti_" + index_str, spot.complexreti, spotEdited.complexreti);
    assignFromKeyfile(keyFile, "Locallab", "retinexMethod_" + index_str, spot.retinexMethod, spotEdited.retinexMethod);
    assignFromKeyfile(keyFile, "Locallab", "Str_" + index_str, spot.str, spotEdited.str);
    assignFromKeyfile(keyFile, "Locallab", "Chrrt_" + index_str, spot.chrrt, spotEdited.chrrt);
    assignFromKeyfile(keyFile, "Locallab", "Neigh_" + index_str, spot.neigh, spotEdited.neigh);
    assignFromKeyfile(keyFile, "Locallab", "Vart_" + index_str, spot.vart, spotEdited.vart);
    assignFromKeyfile(keyFile, "Locallab", "Offs_" + index_str, spot.offs, spotEdited.offs);
    assignFromKeyfile(keyFile, "Locallab", "Dehaz_" + index_str, spot.dehaz, spotEdited.dehaz);
    assignFromKeyfile(keyFile, "Locallab", "Depth_" + index_str, spot.depth, spotEdited.depth);
    assignFromKeyfile(keyFile, "Locallab", "Sensih_" + index_str, spot.sensih, spotEdited.sensih);
    assignFromKeyfile(keyFile, "Locallab", "TgainCurve_" + index_str, spot.localTgaincurve, spotEdited.localTgaincurve);
    assignFromKeyfile(keyFile, "Locallab", "TtransCurve_" + index_str, spot.localTtranscurve, spotEdited.localTtranscurve);
    assignFromKeyfile(keyFile, "Locallab", "Inversret_" + index_str, spot.inversret, spotEdited.inversret);
    assignFromKeyfile(keyFile, "Locallab", "Equilret_" + index_str, spot.equilret, spotEdited.equilret);
    assignFromKeyfile(keyFile, "Locallab", "Loglin_" + index_str, spot.loglin, spotEdited.loglin);
    assignFromKeyfile(keyFile, "Locallab", "dehazeSaturation_" + index_str, spot.dehazeSaturation, spotEdited.dehazeSaturation);
    assignFromKeyfile(keyFile, "Locallab", "dehazeblack_" + index_str, spot.dehazeblack, spotEdited.dehazeblack);
    assignFromKeyfile(keyFile, "Locallab", "Softradiusret_" + index_str, spot.softradiusret, spotEdited.softradiusret);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskretiCurve_" + index_str, spot.CCmaskreticurve, spotEdited.CCmaskreticurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskretiCurve_" + index_str, spot.LLmaskreticurve, spotEdited.LLmaskreticurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskretiCurve_" + index_str, spot.HHmaskreticurve, spotEdited.HHmaskreticurve);
    assignFromKeyfile(keyFile, "Locallab", "EnaretiMask_" + index_str, spot.enaretiMask, spotEdited.enaretiMask);
    assignFromKeyfile(keyFile, "Locallab", "EnaretiMasktmap_" + index_str, spot.enaretiMasktmap, spotEdited.enaretiMasktmap);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskreti_" + index_str, spot.blendmaskreti, spotEdited.blendmaskreti);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskreti_" + index_str, spot.radmaskreti, spotEdited.radmaskreti);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskreti_" + index_str, spot.chromaskreti, spotEdited.chromaskreti);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskreti_" + index_str, spot.gammaskreti, spotEdited.gammaskreti);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskreti_" + index_str, spot.slomaskreti, spotEdited.slomaskreti);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskreti_" + index_str, spot.lapmaskreti, spotEdited.lapmaskreti);
    assignFromKeyfile(keyFile, "Locallab", "Scalereti_" + index_str, spot.scalereti, spotEdited.scalereti);
    assignFromKeyfile(keyFile, "Locallab", "Darkness_" + index_str, spot.darkness, spotEdited.darkness);
    assignFromKeyfile(keyFile, "Locallab", "Lightnessreti_" + index_str, spot.lightnessreti, spotEdited.lightnessreti);
    assignFromKeyfile(keyFile, "Locallab", "Limd_" + index_str, spot.limd, spotEdited.limd);
    assignFromKeyfile(keyFile, "Locallab", "Cliptm_" + index_str, spot.cliptm, spotEdited.cliptm);
    assignFromKeyfile(keyFile, "Locallab", "Fftwreti_" + index_str, spot.fftwreti, spotEdited.fftwreti);
    assignFromKeyfile(keyFile, "Locallab", "LmaskretiCurve_" + index_str, spot.Lmaskreticurve, spotEdited.Lmaskreticurve);
    assignFromKeyfile(keyFile, "Locallab", "Recothresr_" + index_str, spot.recothresr, spotEdited.recothresr);
    assignFromKeyfile(keyFile, "Locallab", "Lowthresr_" + index_str, spot.lowthresr, spotEdited.lowthresr);
    assignFromKeyfile(keyFile, "Locallab", "Higthresr_" + index_str, spot.higthresr, spotEdited.higthresr);
    assignFromKeyfile(keyFile, "Locallab", "Decayr_" + index_str, spot.decayr, spotEdited.decayr);
}

void LoadUtil::sharpening()
{
    spot.visisharp = assignFromKeyfile(keyFile, "Locallab", "Expsharp_" + index_str, spot.expsharp, spotEdited.expsharp);

    if (spot.visisharp) {
        spotEdited.visisharp = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexsharp_" + index_str, spot.complexsharp, spotEdited.complexsharp);
    assignFromKeyfile(keyFile, "Locallab", "Sharcontrast_" + index_str, spot.sharcontrast, spotEdited.sharcontrast);
    assignFromKeyfile(keyFile, "Locallab", "Sharradius_" + index_str, spot.sharradius, spotEdited.sharradius);
    assignFromKeyfile(keyFile, "Locallab", "Sharamount_" + index_str, spot.sharamount, spotEdited.sharamount);
    assignFromKeyfile(keyFile, "Locallab", "Shardamping_" + index_str, spot.shardamping, spotEdited.shardamping);
    assignFromKeyfile(keyFile, "Locallab", "Shariter_" + index_str, spot.shariter, spotEdited.shariter);
    assignFromKeyfile(keyFile, "Locallab", "Sharblur_" + index_str, spot.sharblur, spotEdited.sharblur);
    assignFromKeyfile(keyFile, "Locallab", "Shargam_" + index_str, spot.shargam, spotEdited.shargam);
    assignFromKeyfile(keyFile, "Locallab", "Sensisha_" + index_str, spot.sensisha, spotEdited.sensisha);
    assignFromKeyfile(keyFile, "Locallab", "Inverssha_" + index_str, spot.inverssha, spotEdited.inverssha);
    assignFromKeyfile(keyFile, "Locallab", "sharshow_" + index_str, spot.sharshow, spotEdited.sharshow);
    assignFromKeyfile(keyFile, "Locallab", "itercheck_" + index_str, spot.itercheck, spotEdited.itercheck);
    assignFromKeyfile(keyFile, "Locallab", "methodcap_" + index_str, spot.methodcap, spotEdited.methodcap);
    assignFromKeyfile(keyFile, "Locallab", "capradius_" + index_str, spot.capradius, spotEdited.capradius);
    assignFromKeyfile(keyFile, "Locallab", "deconvAutoRadius_" + index_str, spot.deconvAutoRadius, spotEdited.deconvAutoRadius);
    assignFromKeyfile(keyFile, "Locallab", "deconvAutoshar_" + index_str, spot.deconvAutoshar, spotEdited.deconvAutoshar);

    assignFromKeyfile(keyFile, "Locallab", "deconvCoBoost_" + index_str, spot.deconvCoBoost, spotEdited.deconvCoBoost);
    assignFromKeyfile(keyFile, "Locallab", "deconvCoProt_" + index_str, spot.deconvCoProt, spotEdited.deconvCoProt);
    assignFromKeyfile(keyFile, "Locallab", "deconvCoLat_" + index_str, spot.deconvCoLat, spotEdited.deconvCoLat);
    assignFromKeyfile(keyFile, "Locallab", "deconvCogam_" + index_str, spot.deconvCogam, spotEdited.deconvCogam);
    assignFromKeyfile(keyFile, "Locallab", "reparsha_" + index_str, spot.reparsha, spotEdited.reparsha);
}

void LoadUtil::localContrast()
{
    spot.visicontrast = assignFromKeyfile(keyFile, "Locallab", "Expcontrast_" + index_str, spot.expcontrast, spotEdited.expcontrast);

    if (spot.visicontrast) {
        spotEdited.visicontrast = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexcontrast_" + index_str, spot.complexcontrast, spotEdited.complexcontrast);
    assignFromKeyfile(keyFile, "Locallab", "Lcradius_" + index_str, spot.lcradius, spotEdited.lcradius);
    assignFromKeyfile(keyFile, "Locallab", "Lcamount_" + index_str, spot.lcamount, spotEdited.lcamount);
    assignFromKeyfile(keyFile, "Locallab", "Lcdarkness_" + index_str, spot.lcdarkness, spotEdited.lcdarkness);
    assignFromKeyfile(keyFile, "Locallab", "Lclightness_" + index_str, spot.lclightness, spotEdited.lclightness);
    assignFromKeyfile(keyFile, "Locallab", "Sigmalc_" + index_str, spot.sigmalc, spotEdited.sigmalc);
    assignFromKeyfile(keyFile, "Locallab", "Offslc_" + index_str, spot.offslc, spotEdited.offslc);
    assignFromKeyfile(keyFile, "Locallab", "Levelwav_" + index_str, spot.levelwav, spotEdited.levelwav);
    assignFromKeyfile(keyFile, "Locallab", "Residcont_" + index_str, spot.residcont, spotEdited.residcont);
    assignFromKeyfile(keyFile, "Locallab", "Residsha_" + index_str, spot.residsha, spotEdited.residsha);
    assignFromKeyfile(keyFile, "Locallab", "Residshathr_" + index_str, spot.residshathr, spotEdited.residshathr);
    assignFromKeyfile(keyFile, "Locallab", "Residhi_" + index_str, spot.residhi, spotEdited.residhi);
    assignFromKeyfile(keyFile, "Locallab", "Gamlc_" + index_str, spot.gamlc, spotEdited.gamlc);
    assignFromKeyfile(keyFile, "Locallab", "Residhithr_" + index_str, spot.residhithr, spotEdited.residhithr);
    assignFromKeyfile(keyFile, "Locallab", "Residgam_" + index_str, spot.residgam, spotEdited.residgam);
    assignFromKeyfile(keyFile, "Locallab", "Residslop_" + index_str, spot.residslop, spotEdited.residslop);
    assignFromKeyfile(keyFile, "Locallab", "Residblur_" + index_str, spot.residblur, spotEdited.residblur);
    assignFromKeyfile(keyFile, "Locallab", "Levelblur_" + index_str, spot.levelblur, spotEdited.levelblur);
    assignFromKeyfile(keyFile, "Locallab", "Sigmabl_" + index_str, spot.sigmabl, spotEdited.sigmabl);
    assignFromKeyfile(keyFile, "Locallab", "Residchro_" + index_str, spot.residchro, spotEdited.residchro);
    assignFromKeyfile(keyFile, "Locallab", "Residcomp_" + index_str, spot.residcomp, spotEdited.residcomp);
    assignFromKeyfile(keyFile, "Locallab", "Sigma_" + index_str, spot.sigma, spotEdited.sigma);
    assignFromKeyfile(keyFile, "Locallab", "Offset_" + index_str, spot.offset, spotEdited.offset);
    assignFromKeyfile(keyFile, "Locallab", "Sigmadr_" + index_str, spot.sigmadr, spotEdited.sigmadr);
    assignFromKeyfile(keyFile, "Locallab", "Threswav_" + index_str, spot.threswav, spotEdited.threswav);
    assignFromKeyfile(keyFile, "Locallab", "Chromalev_" + index_str, spot.chromalev, spotEdited.chromalev);
    assignFromKeyfile(keyFile, "Locallab", "Chromablu_" + index_str, spot.chromablu, spotEdited.chromablu);
    assignFromKeyfile(keyFile, "Locallab", "sigmadc_" + index_str, spot.sigmadc, spotEdited.sigmadc);
    assignFromKeyfile(keyFile, "Locallab", "deltad_" + index_str, spot.deltad, spotEdited.deltad);
    assignFromKeyfile(keyFile, "Locallab", "Fatres_" + index_str, spot.fatres, spotEdited.fatres);
    assignFromKeyfile(keyFile, "Locallab", "ClariLres_" + index_str, spot.clarilres, spotEdited.clarilres);
    assignFromKeyfile(keyFile, "Locallab", "ClariCres_" + index_str, spot.claricres, spotEdited.claricres);
    assignFromKeyfile(keyFile, "Locallab", "Clarisoft_" + index_str, spot.clarisoft, spotEdited.clarisoft);
    assignFromKeyfile(keyFile, "Locallab", "Sigmalc2_" + index_str, spot.sigmalc2, spotEdited.sigmalc2);
    assignFromKeyfile(keyFile, "Locallab", "Strwav_" + index_str, spot.strwav, spotEdited.strwav);
    assignFromKeyfile(keyFile, "Locallab", "Angwav_" + index_str, spot.angwav, spotEdited.angwav);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.featherwav = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.featherwav = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Featherwav_" + index_str, spot.featherwav, spotEdited.featherwav);
    }

    assignFromKeyfile(keyFile, "Locallab", "Strengthw_" + index_str, spot.strengthw, spotEdited.strengthw);
    assignFromKeyfile(keyFile, "Locallab", "Sigmaed_" + index_str, spot.sigmaed, spotEdited.sigmaed);
    assignFromKeyfile(keyFile, "Locallab", "Radiusw_" + index_str, spot.radiusw, spotEdited.radiusw);
    assignFromKeyfile(keyFile, "Locallab", "Detailw_" + index_str, spot.detailw, spotEdited.detailw);
    assignFromKeyfile(keyFile, "Locallab", "Gradw_" + index_str, spot.gradw, spotEdited.gradw);
    assignFromKeyfile(keyFile, "Locallab", "Tloww_" + index_str, spot.tloww, spotEdited.tloww);
    assignFromKeyfile(keyFile, "Locallab", "Thigw_" + index_str, spot.thigw, spotEdited.thigw);
    assignFromKeyfile(keyFile, "Locallab", "Edgw_" + index_str, spot.edgw, spotEdited.edgw);
    assignFromKeyfile(keyFile, "Locallab", "Basew_" + index_str, spot.basew, spotEdited.basew);
    assignFromKeyfile(keyFile, "Locallab", "Sensilc_" + index_str, spot.sensilc, spotEdited.sensilc);
    assignFromKeyfile(keyFile, "Locallab", "Reparw_" + index_str, spot.reparw, spotEdited.reparw);
    assignFromKeyfile(keyFile, "Locallab", "Fftwlc_" + index_str, spot.fftwlc, spotEdited.fftwlc);
    assignFromKeyfile(keyFile, "Locallab", "Blurlc_" + index_str, spot.blurlc, spotEdited.blurlc);
    assignFromKeyfile(keyFile, "Locallab", "Wavblur_" + index_str, spot.wavblur, spotEdited.wavblur);
    assignFromKeyfile(keyFile, "Locallab", "Wavedg_" + index_str, spot.wavedg, spotEdited.wavedg);
    assignFromKeyfile(keyFile, "Locallab", "Waveshow_" + index_str, spot.waveshow, spotEdited.waveshow);
    assignFromKeyfile(keyFile, "Locallab", "Wavcont_" + index_str, spot.wavcont, spotEdited.wavcont);
    assignFromKeyfile(keyFile, "Locallab", "Wavcomp_" + index_str, spot.wavcomp, spotEdited.wavcomp);
    assignFromKeyfile(keyFile, "Locallab", "Wavgradl_" + index_str, spot.wavgradl, spotEdited.wavgradl);
    assignFromKeyfile(keyFile, "Locallab", "Wavcompre_" + index_str, spot.wavcompre, spotEdited.wavcompre);
    assignFromKeyfile(keyFile, "Locallab", "Origlc_" + index_str, spot.origlc, spotEdited.origlc);
    assignFromKeyfile(keyFile, "Locallab", "processwav_" + index_str, spot.processwav, spotEdited.processwav);
    assignFromKeyfile(keyFile, "Locallab", "localcontMethod_" + index_str, spot.localcontMethod, spotEdited.localcontMethod);
    assignFromKeyfile(keyFile, "Locallab", "localedgMethod_" + index_str, spot.localedgMethod, spotEdited.localedgMethod);
    assignFromKeyfile(keyFile, "Locallab", "localneiMethod_" + index_str, spot.localneiMethod, spotEdited.localneiMethod);
    assignFromKeyfile(keyFile, "Locallab", "LocwavCurve_" + index_str, spot.locwavcurve, spotEdited.locwavcurve);
    assignFromKeyfile(keyFile, "Locallab", "LoclevwavCurve_" + index_str, spot.loclevwavcurve, spotEdited.loclevwavcurve);
    assignFromKeyfile(keyFile, "Locallab", "LocconwavCurve_" + index_str, spot.locconwavcurve, spotEdited.locconwavcurve);
    assignFromKeyfile(keyFile, "Locallab", "LoccompwavCurve_" + index_str, spot.loccompwavcurve, spotEdited.loccompwavcurve);
    assignFromKeyfile(keyFile, "Locallab", "LoccomprewavCurve_" + index_str, spot.loccomprewavcurve, spotEdited.loccomprewavcurve);
    assignFromKeyfile(keyFile, "Locallab", "LocedgwavCurve_" + index_str, spot.locedgwavcurve, spotEdited.locedgwavcurve);

    if (keyFile.has_key("Locallab", "CSThreshold_" + index_str)) {

        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "CSThreshold_" + index_str);

        if (thresh.size() >= 4) {
            spot.csthreshold.setValues(thresh[0], thresh[1], min(thresh[2], 10), min(thresh[3], 10));
        }

        spotEdited.csthreshold = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "CCmasklcCurve_" + index_str, spot.CCmasklccurve, spotEdited.CCmasklccurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmasklcCurve_" + index_str, spot.LLmasklccurve, spotEdited.LLmasklccurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmasklcCurve_" + index_str, spot.HHmasklccurve, spotEdited.HHmasklccurve);
    assignFromKeyfile(keyFile, "Locallab", "EnalcMask_" + index_str, spot.enalcMask, spotEdited.enalcMask);
    assignFromKeyfile(keyFile, "Locallab", "Blendmasklc_" + index_str, spot.blendmasklc, spotEdited.blendmasklc);
    assignFromKeyfile(keyFile, "Locallab", "Radmasklc_" + index_str, spot.radmasklc, spotEdited.radmasklc);
    assignFromKeyfile(keyFile, "Locallab", "Chromasklc_" + index_str, spot.chromasklc, spotEdited.chromasklc);
    assignFromKeyfile(keyFile, "Locallab", "LmasklcCurve_" + index_str, spot.Lmasklccurve, spotEdited.Lmasklccurve);
    assignFromKeyfile(keyFile, "Locallab", "Recothresw_" + index_str, spot.recothresw, spotEdited.recothresw);
    assignFromKeyfile(keyFile, "Locallab", "Lowthresw_" + index_str, spot.lowthresw, spotEdited.lowthresw);
    assignFromKeyfile(keyFile, "Locallab", "Higthresw_" + index_str, spot.higthresw, spotEdited.higthresw);
    assignFromKeyfile(keyFile, "Locallab", "Decayw_" + index_str, spot.decayw, spotEdited.decayw);
}

void LoadUtil::contrastByDetailLevels()
{
    spot.visicbdl = assignFromKeyfile(keyFile, "Locallab", "Expcbdl_" + index_str, spot.expcbdl, spotEdited.expcbdl);

    if (spot.visicbdl) {
        spotEdited.visicbdl = true;
    }

    assignFromKeyfile(keyFile, "Locallab", "Complexcbdl_" + index_str, spot.complexcbdl, spotEdited.complexcbdl);

    for (int j = 0; j < 6; j ++) {
        assignFromKeyfile(keyFile, "Locallab", "Mult" + std::to_string(j) + "_" + index_str, spot.mult[j], spotEdited.mult[j]);
    }

    assignFromKeyfile(keyFile, "Locallab", "Chromacbdl_" + index_str, spot.chromacbdl, spotEdited.chromacbdl);
    assignFromKeyfile(keyFile, "Locallab", "Threshold_" + index_str, spot.threshold, spotEdited.threshold);
    assignFromKeyfile(keyFile, "Locallab", "Sensicb_" + index_str, spot.sensicb, spotEdited.sensicb);
    assignFromKeyfile(keyFile, "Locallab", "Clarityml_" + index_str, spot.clarityml, spotEdited.clarityml);
    assignFromKeyfile(keyFile, "Locallab", "Contresid_" + index_str, spot.contresid, spotEdited.contresid);
    assignFromKeyfile(keyFile, "Locallab", "Softradiuscb_" + index_str, spot.softradiuscb, spotEdited.softradiuscb);
    assignFromKeyfile(keyFile, "Locallab", "EnacbMask_" + index_str, spot.enacbMask, spotEdited.enacbMask);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskcbCurve_" + index_str, spot.CCmaskcbcurve, spotEdited.CCmaskcbcurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskcbCurve_" + index_str, spot.LLmaskcbcurve, spotEdited.LLmaskcbcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskcbCurve_" + index_str, spot.HHmaskcbcurve, spotEdited.HHmaskcbcurve);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskcb_" + index_str, spot.blendmaskcb, spotEdited.blendmaskcb);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskcb_" + index_str, spot.radmaskcb, spotEdited.radmaskcb);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskcb_" + index_str, spot.chromaskcb, spotEdited.chromaskcb);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskcb_" + index_str, spot.gammaskcb, spotEdited.gammaskcb);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskcb_" + index_str, spot.slomaskcb, spotEdited.slomaskcb);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskcb_" + index_str, spot.lapmaskcb, spotEdited.lapmaskcb);
    assignFromKeyfile(keyFile, "Locallab", "LmaskcbCurve_" + index_str, spot.Lmaskcbcurve, spotEdited.Lmaskcbcurve);
    assignFromKeyfile(keyFile, "Locallab", "Recothrescb_" + index_str, spot.recothrescb, spotEdited.recothrescb);
    assignFromKeyfile(keyFile, "Locallab", "Lowthrescb_" + index_str, spot.lowthrescb, spotEdited.lowthrescb);
    assignFromKeyfile(keyFile, "Locallab", "Higthrescb_" + index_str, spot.higthrescb, spotEdited.higthrescb);
    assignFromKeyfile(keyFile, "Locallab", "Decaycb_" + index_str, spot.decaycb, spotEdited.decaycb);
}

void LoadUtil::logEncoding()
{
    spot.visilog = assignFromKeyfile(keyFile, "Locallab", "Explog_" + index_str, spot.explog, spotEdited.explog);

    if (spot.visilog) {
        spotEdited.visilog = true;
    }
    assignFromKeyfile(keyFile, "Locallab", "Complexlog_" + index_str, spot.complexlog, spotEdited.complexlog);

    assignFromKeyfile(keyFile, "Locallab", "Autocompute_" + index_str, spot.autocompute, spotEdited.autocompute);
    assignFromKeyfile(keyFile, "Locallab", "SourceGray_" + index_str, spot.sourceGray, spotEdited.sourceGray);
    assignFromKeyfile(keyFile, "Locallab", "Sourceabs_" + index_str, spot.sourceabs, spotEdited.sourceabs);
    assignFromKeyfile(keyFile, "Locallab", "Targabs_" + index_str, spot.targabs, spotEdited.targabs);
    assignFromKeyfile(keyFile, "Locallab", "TargetGray_" + index_str, spot.targetGray, spotEdited.targetGray);
    assignFromKeyfile(keyFile, "Locallab", "Catad_" + index_str, spot.catad, spotEdited.catad);
    assignFromKeyfile(keyFile, "Locallab", "Saturl_" + index_str, spot.saturl, spotEdited.saturl);
    assignFromKeyfile(keyFile, "Locallab", "Chroml_" + index_str, spot.chroml, spotEdited.chroml);
    assignFromKeyfile(keyFile, "Locallab", "Lightl_" + index_str, spot.lightl, spotEdited.lightl);
    assignFromKeyfile(keyFile, "Locallab", "Brightq_" + index_str, spot.lightq, spotEdited.lightq);
    assignFromKeyfile(keyFile, "Locallab", "Contl_" + index_str, spot.contl, spotEdited.contl);
    assignFromKeyfile(keyFile, "Locallab", "Contthres_" + index_str, spot.contthres, spotEdited.contthres);
    assignFromKeyfile(keyFile, "Locallab", "Contq_" + index_str, spot.contq, spotEdited.contq);
    assignFromKeyfile(keyFile, "Locallab", "Colorfl_" + index_str, spot.colorfl, spotEdited.colorfl);
    assignFromKeyfile(keyFile, "Locallab", "LCurveL_" + index_str, spot.LcurveL, spotEdited.LcurveL);
    assignFromKeyfile(keyFile, "Locallab", "AutoGray_" + index_str, spot.Autogray, spotEdited.Autogray);
    assignFromKeyfile(keyFile, "Locallab", "Fullimage_" + index_str, spot.fullimage, spotEdited.fullimage);
    assignFromKeyfile(keyFile, "Locallab", "Repart_" + index_str, spot.repar, spotEdited.repar);
    if (ppVersion <= 350) {//issue 7114
        if (keyFile.has_key("Locallab", "Ciecam_" + index_str)) {
            spot.ciecam = true;
            spotEdited.ciecam = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Ciecam_" + index_str, spot.ciecam, spotEdited.ciecam);
    }

    assignFromKeyfile(keyFile, "Locallab", "Satlog_" + index_str, spot.satlog, spotEdited.satlog);
    assignFromKeyfile(keyFile, "Locallab", "BlackEv_" + index_str, spot.blackEv, spotEdited.blackEv);
    assignFromKeyfile(keyFile, "Locallab", "WhiteEv_" + index_str, spot.whiteEv, spotEdited.whiteEv);
    assignFromKeyfile(keyFile, "Locallab", "Whiteslog_" + index_str, spot.whiteslog, spotEdited.whiteslog);
    assignFromKeyfile(keyFile, "Locallab", "Blackslog_" + index_str, spot.blackslog, spotEdited.blackslog);
    assignFromKeyfile(keyFile, "Locallab", "Comprlog_" + index_str, spot.comprlog, spotEdited.comprlog);
    assignFromKeyfile(keyFile, "Locallab", "Strelog_" + index_str, spot.strelog, spotEdited.strelog);
    assignFromKeyfile(keyFile, "Locallab", "Detail_" + index_str, spot.detail, spotEdited.detail);
    assignFromKeyfile(keyFile, "Locallab", "Sensilog_" + index_str, spot.sensilog, spotEdited.sensilog);
    assignFromKeyfile(keyFile, "Locallab", "Baselog_" + index_str, spot.baselog, spotEdited.baselog);
    assignFromKeyfile(keyFile, "Locallab", "Sursour_" + index_str, spot.sursour, spotEdited.sursour);
    assignFromKeyfile(keyFile, "Locallab", "Surround_" + index_str, spot.surround, spotEdited.surround);
    assignFromKeyfile(keyFile, "Locallab", "Strlog_" + index_str, spot.strlog, spotEdited.strlog);
    assignFromKeyfile(keyFile, "Locallab", "Anglog_" + index_str, spot.anglog, spotEdited.anglog);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.featherlog = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.featherlog = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Featherlog_" + index_str, spot.featherlog, spotEdited.featherlog);
    }
    assignFromKeyfile(keyFile, "Locallab", "CCmaskCurveL_" + index_str, spot.CCmaskcurveL, spotEdited.CCmaskcurveL);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskCurveL_" + index_str, spot.LLmaskcurveL, spotEdited.LLmaskcurveL);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskCurveL_" + index_str, spot.HHmaskcurveL, spotEdited.HHmaskcurveL);
    assignFromKeyfile(keyFile, "Locallab", "EnaLMask_" + index_str, spot.enaLMask, spotEdited.enaLMask);
    assignFromKeyfile(keyFile, "Locallab", "blendmaskL_" + index_str, spot.blendmaskL, spotEdited.blendmaskL);
    assignFromKeyfile(keyFile, "Locallab", "radmaskL_" + index_str, spot.radmaskL, spotEdited.radmaskL);
    assignFromKeyfile(keyFile, "Locallab", "chromaskL_" + index_str, spot.chromaskL, spotEdited.chromaskL);
    assignFromKeyfile(keyFile, "Locallab", "LmaskCurveL_" + index_str, spot.LmaskcurveL, spotEdited.LmaskcurveL);
    assignFromKeyfile(keyFile, "Locallab", "Recothresl_" + index_str, spot.recothresl, spotEdited.recothresl);
    assignFromKeyfile(keyFile, "Locallab", "Lowthresl_" + index_str, spot.lowthresl, spotEdited.lowthresl);
    assignFromKeyfile(keyFile, "Locallab", "Higthresl_" + index_str, spot.higthresl, spotEdited.higthresl);
    assignFromKeyfile(keyFile, "Locallab", "Decayl_" + index_str, spot.decayl, spotEdited.decayl);
}

void LoadUtil::mask()
{
    spot.visimask = assignFromKeyfile(keyFile, "Locallab", "Expmask_" + index_str, spot.expmask, spotEdited.expmask);
    assignFromKeyfile(keyFile, "Locallab", "Complexmask_" + index_str, spot.complexmask, spotEdited.complexmask);
    assignFromKeyfile(keyFile, "Locallab", "Sensimask_" + index_str, spot.sensimask, spotEdited.sensimask);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskmask_" + index_str, spot.blendmask, spotEdited.blendmask);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskmaskab_" + index_str, spot.blendmaskab, spotEdited.blendmaskab);
    assignFromKeyfile(keyFile, "Locallab", "Softradiusmask_" + index_str, spot.softradiusmask, spotEdited.softradiusmask);
    assignFromKeyfile(keyFile, "Locallab", "Enamask_" + index_str, spot.enamask, spotEdited.enamask);
    assignFromKeyfile(keyFile, "Locallab", "Fftmask_" + index_str, spot.fftmask, spotEdited.fftmask);
    assignFromKeyfile(keyFile, "Locallab", "Blurmask_" + index_str, spot.blurmask, spotEdited.blurmask);
    assignFromKeyfile(keyFile, "Locallab", "Contmask_" + index_str, spot.contmask, spotEdited.contmask);
    assignFromKeyfile(keyFile, "Locallab", "CCmask_Curve_" + index_str, spot.CCmask_curve, spotEdited.CCmask_curve);
    assignFromKeyfile(keyFile, "Locallab", "LLmask_Curve_" + index_str, spot.LLmask_curve, spotEdited.LLmask_curve);
    assignFromKeyfile(keyFile, "Locallab", "HHmask_Curve_" + index_str, spot.HHmask_curve, spotEdited.HHmask_curve);
    assignFromKeyfile(keyFile, "Locallab", "Strumaskmask_" + index_str, spot.strumaskmask, spotEdited.strumaskmask);
    assignFromKeyfile(keyFile, "Locallab", "Toolmask_" + index_str, spot.toolmask, spotEdited.toolmask);
    assignFromKeyfile(keyFile, "Locallab", "Radmask_" + index_str, spot.radmask, spotEdited.radmask);
    assignFromKeyfile(keyFile, "Locallab", "Lapmask_" + index_str, spot.lapmask, spotEdited.lapmask);
    assignFromKeyfile(keyFile, "Locallab", "Chromask_" + index_str, spot.chromask, spotEdited.chromask);
    assignFromKeyfile(keyFile, "Locallab", "Gammask_" + index_str, spot.gammask, spotEdited.gammask);
    assignFromKeyfile(keyFile, "Locallab", "Slopmask_" + index_str, spot.slopmask, spotEdited.slopmask);
    assignFromKeyfile(keyFile, "Locallab", "Shadmask_" + index_str, spot.shadmask, spotEdited.shadmask);
    assignFromKeyfile(keyFile, "Locallab", "Str_mask_" + index_str, spot.str_mask, spotEdited.str_mask);
    assignFromKeyfile(keyFile, "Locallab", "Ang_mask_" + index_str, spot.ang_mask, spotEdited.ang_mask);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.feather_mask = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.feather_mask = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Feather_mask_" + index_str, spot.feather_mask, spotEdited.feather_mask);
    }

    assignFromKeyfile(keyFile, "Locallab", "HHhmask_Curve_" + index_str, spot.HHhmask_curve, spotEdited.HHhmask_curve);
    assignFromKeyfile(keyFile, "Locallab", "Lmask_Curve_" + index_str, spot.Lmask_curve, spotEdited.Lmask_curve);
    assignFromKeyfile(keyFile, "Locallab", "LLmask_Curvewav_" + index_str, spot.LLmask_curvewav, spotEdited.LLmask_curvewav);

    if (keyFile.has_key("Locallab", "CSThresholdmask_" + index_str)) {
        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "CSThresholdmask_" + index_str);

        if (thresh.size() >= 4) {
            spot.csthresholdmask.setValues(thresh[0], thresh[1], min(thresh[2], 10), min(thresh[3], 10));
        }

        spotEdited.csthresholdmask = true;
    }

    if (spot.visimask) {
        spotEdited.visimask = true;
    }
}

void LoadUtil::ciecam()
{
    spot.visicie = assignFromKeyfile(keyFile, "Locallab", "Expcie_" + index_str, spot.expcie, spotEdited.expcie);

    if (spot.visicie) {
        spotEdited.visicie = true;
    }
    assignFromKeyfile(keyFile, "Locallab", "Expprecam_" + index_str, spot.expprecam, spotEdited.expprecam);
    assignFromKeyfile(keyFile, "Locallab", "Complexcie_" + index_str, spot.complexcie, spotEdited.complexcie);
    assignFromKeyfile(keyFile, "Locallab", "Reparcie_" + index_str, spot.reparcie, spotEdited.reparcie);
    assignFromKeyfile(keyFile, "Locallab", "Sensicie_" + index_str, spot.sensicie, spotEdited.sensicie);
    assignFromKeyfile(keyFile, "Locallab", "AutoGraycie_" + index_str, spot.Autograycie, spotEdited.Autograycie);
    assignFromKeyfile(keyFile, "Locallab", "sigybjz12_" + index_str, spot.sigybjz12, spotEdited.sigybjz12);
    assignFromKeyfile(keyFile, "Locallab", "Qtoj_" + index_str, spot.qtoj, spotEdited.qtoj);
    assignFromKeyfile(keyFile, "Locallab", "jabcie_" + index_str, spot.jabcie, spotEdited.jabcie);
    assignFromKeyfile(keyFile, "Locallab", "comprcieauto_" + index_str, spot.comprcieauto, spotEdited.comprcieauto);
    assignFromKeyfile(keyFile, "Locallab", "normcie12_" + index_str, spot.normcie12, spotEdited.normcie12);
    assignFromKeyfile(keyFile, "Locallab", "normcie_" + index_str, spot.normcie, spotEdited.normcie);
    assignFromKeyfile(keyFile, "Locallab", "gamutcie_" + index_str, spot.gamutcie, spotEdited.gamutcie);
    assignFromKeyfile(keyFile, "Locallab", "bwcie_" + index_str, spot.bwcie, spotEdited.bwcie);
    assignFromKeyfile(keyFile, "Locallab", "sigcie_" + index_str, spot.sigcie, spotEdited.sigcie);
    assignFromKeyfile(keyFile, "Locallab", "logcie_" + index_str, spot.logcie, spotEdited.logcie);
    assignFromKeyfile(keyFile, "Locallab", "satcie_" + index_str, spot.satcie, spotEdited.satcie);
    assignFromKeyfile(keyFile, "Locallab", "logcieq_" + index_str, spot.logcieq, spotEdited.logcieq);
    assignFromKeyfile(keyFile, "Locallab", "smoothcie_" + index_str, spot.smoothcie, spotEdited.smoothcie);
    assignFromKeyfile(keyFile, "Locallab", "smoothcietrc_" + index_str, spot.smoothcietrc, spotEdited.smoothcietrc);
    assignFromKeyfile(keyFile, "Locallab", "smoothcietrcrel_" + index_str, spot.smoothcietrcrel, spotEdited.smoothcietrcrel);
    assignFromKeyfile(keyFile, "Locallab", "smoothcieyb_" + index_str, spot.smoothcieyb, spotEdited.smoothcieyb);
    assignFromKeyfile(keyFile, "Locallab", "smoothcielum_" + index_str, spot.smoothcielum, spotEdited.smoothcielum);
    assignFromKeyfile(keyFile, "Locallab", "smoothciehigh_" + index_str, spot.smoothciehigh, spotEdited.smoothciehigh);
    assignFromKeyfile(keyFile, "Locallab", "smoothcielnk_" + index_str, spot.smoothcielnk, spotEdited.smoothcielnk);
    assignFromKeyfile(keyFile, "Locallab", "smoothcieinv_" + index_str, spot.smoothcieinv, spotEdited.smoothcieinv);
    assignFromKeyfile(keyFile, "Locallab", "Logjz_" + index_str, spot.logjz, spotEdited.logjz);
    assignFromKeyfile(keyFile, "Locallab", "Sigjz12_" + index_str, spot.sigjz12, spotEdited.sigjz12);
    assignFromKeyfile(keyFile, "Locallab", "Sigjz_" + index_str, spot.sigjz, spotEdited.sigjz);
    assignFromKeyfile(keyFile, "Locallab", "Forcebw_" + index_str, spot.forcebw, spotEdited.forcebw);
    assignFromKeyfile(keyFile, "Locallab", "ModeQJ_" + index_str, spot.modeQJ, spotEdited.modeQJ);
    assignFromKeyfile(keyFile, "Locallab", "Modecie_" + index_str, spot.modecie, spotEdited.modecie);
    assignFromKeyfile(keyFile, "Locallab", "Modecam_" + index_str, spot.modecam, spotEdited.modecam);

    assignFromKeyfile(keyFile, "Locallab", "Sigq12_" + index_str, spot.sigq12, spotEdited.sigq12);
    assignFromKeyfile(keyFile, "Locallab", "Sigq_" + index_str, spot.sigq, spotEdited.sigq);

    if (ppVersion < 352) {//change behavior Log encoding Q and Sigmoid Q and SigmoidJz when 'in the loop' Cam16
        if (keyFile.has_key("Locallab", "Sigq_" + index_str)) {
            if (spot.sigq == true) {
                spot.modeQJ = "511";
                spot.sigq12 = false;
                spotEdited.modeQJ = true;
                spotEdited.sigq12 = true;                             
            }
        }
        if (keyFile.has_key("Locallab", "logcieq_" + index_str)) {
            if (spot.logcieq == true) {
                spot.modeQJ = "511";
                spotEdited.modeQJ = true;
                spotEdited.logcieq = true;                             
            }
        }

        if (keyFile.has_key("Locallab", "Sigjz_" + index_str)) {
            if (spot.sigjz == true) {
                spot.modeQJ = "511";
                spot.sigjz12 = false;
                spotEdited.modeQJ = true;
                spotEdited.sigjz12 = true;                             
            }
        }
    } 

    assignFromKeyfile(keyFile, "Locallab", "chjzcie_" + index_str, spot.chjzcie, spotEdited.chjzcie);
    assignFromKeyfile(keyFile, "Locallab", "SourceGraycie_" + index_str, spot.sourceGraycie, spotEdited.sourceGraycie);
    assignFromKeyfile(keyFile, "Locallab", "Sourceabscie_" + index_str, spot.sourceabscie, spotEdited.sourceabscie);
    assignFromKeyfile(keyFile, "Locallab", "Sursourcie_" + index_str, spot.sursourcie, spotEdited.sursourcie);
    assignFromKeyfile(keyFile, "Locallab", "bwevMethod12_" + index_str,  spot.bwevMethod12, spotEdited.bwevMethod12);
    assignFromKeyfile(keyFile, "Locallab", "bwevMethod_" + index_str,  spot.bwevMethod, spotEdited.bwevMethod);
    assignFromKeyfile(keyFile, "Locallab", "Saturlcie_" + index_str, spot.saturlcie, spotEdited.saturlcie);
    assignFromKeyfile(keyFile, "Locallab", "Rstprotectcie_" + index_str,  spot.rstprotectcie, spotEdited.rstprotectcie);
    assignFromKeyfile(keyFile, "Locallab", "Chromlcie_" + index_str, spot.chromlcie, spotEdited.chromlcie);
    assignFromKeyfile(keyFile, "Locallab", "Huecie_" + index_str, spot.huecie, spotEdited.huecie);
    assignFromKeyfile(keyFile, "Locallab", "ToneMethodcie_" + index_str, spot.toneMethodcie, spotEdited.toneMethodcie);
    assignFromKeyfile(keyFile, "Locallab", "Ciecurve_" + index_str, spot.ciecurve, spotEdited.ciecurve);
    assignFromKeyfile(keyFile, "Locallab", "ToneMethodcie2_" + index_str, spot.toneMethodcie2, spotEdited.toneMethodcie2);
    assignFromKeyfile(keyFile, "Locallab", "Ciecurve2_" + index_str, spot.ciecurve2, spotEdited.ciecurve2);
    assignFromKeyfile(keyFile, "Locallab", "Chromjzcie_" + index_str, spot.chromjzcie, spotEdited.chromjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Saturjzcie_" + index_str, spot.saturjzcie, spotEdited.saturjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Huejzcie_" + index_str, spot.huejzcie, spotEdited.huejzcie);
    assignFromKeyfile(keyFile, "Locallab", "Softjzcie_" + index_str, spot.softjzcie, spotEdited.softjzcie);
    assignFromKeyfile(keyFile, "Locallab", "strSoftjzcie_" + index_str,  spot.strsoftjzcie, spotEdited.strsoftjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Thrhjzcie_" + index_str, spot.thrhjzcie, spotEdited.thrhjzcie);
    assignFromKeyfile(keyFile, "Locallab", "JzCurve_" + index_str, spot.jzcurve, spotEdited.jzcurve);
    assignFromKeyfile(keyFile, "Locallab", "CzCurve_" + index_str, spot.czcurve, spotEdited.czcurve);
    assignFromKeyfile(keyFile, "Locallab", "CzJzCurve_" + index_str, spot.czjzcurve, spotEdited.czjzcurve);
    assignFromKeyfile(keyFile, "Locallab", "HHCurvejz_" + index_str, spot.HHcurvejz, spotEdited.HHcurvejz);
    assignFromKeyfile(keyFile, "Locallab", "CHCurvejz_" + index_str, spot.CHcurvejz, spotEdited.CHcurvejz);
    assignFromKeyfile(keyFile, "Locallab", "LHCurvejz_" + index_str, spot.LHcurvejz, spotEdited.LHcurvejz);
    assignFromKeyfile(keyFile, "Locallab", "Lightlcie_" + index_str, spot.lightlcie, spotEdited.lightlcie);
    assignFromKeyfile(keyFile, "Locallab", "Lightjzcie_" + index_str, spot.lightjzcie, spotEdited.lightjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Brightqcie_" + index_str, spot.lightqcie, spotEdited.lightqcie);
    assignFromKeyfile(keyFile, "Locallab", "Brightsigqcie_" + index_str, spot.lightsigqcie, spotEdited.lightsigqcie);
    assignFromKeyfile(keyFile, "Locallab", "Contlcie_" + index_str, spot.contlcie, spotEdited.contlcie);
    assignFromKeyfile(keyFile, "Locallab", "Contjzcie_" + index_str, spot.contjzcie, spotEdited.contjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Detailciejz_" + index_str, spot.detailciejz, spotEdited.detailciejz);
    assignFromKeyfile(keyFile, "Locallab", "Adapjzcie_" + index_str,  spot.adapjzcie, spotEdited.adapjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Jz100_" + index_str, spot.jz100, spotEdited.jz100);
    assignFromKeyfile(keyFile, "Locallab", "PQremap_" + index_str, spot.pqremap, spotEdited.pqremap);
    assignFromKeyfile(keyFile, "Locallab", "PQremapcam16_" + index_str,  spot.pqremapcam16, spotEdited.pqremapcam16);
    assignFromKeyfile(keyFile, "Locallab", "Hljzcie_" + index_str, spot.hljzcie, spotEdited.hljzcie);
    assignFromKeyfile(keyFile, "Locallab", "Hlthjzcie_" + index_str,  spot.hlthjzcie, spotEdited.hlthjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Shjzcie_" + index_str, spot.shjzcie, spotEdited.shjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Shthjzcie_" + index_str, spot.shthjzcie, spotEdited.shthjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Radjzcie_" + index_str, spot.radjzcie, spotEdited.radjzcie);
    if (keyFile.has_key("Locallab", "CSThresholdjz_" + index_str)) {

        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "CSThresholdjz_" + index_str);

        if (thresh.size() >= 4) {
            spot.csthresholdjz.setValues(thresh[0], thresh[1], min(thresh[2], 10), min(thresh[3], 10));
        }

        spotEdited.csthresholdjz = true;
    }
    assignFromKeyfile(keyFile, "Locallab", "Sigmalcjz_" + index_str, spot.sigmalcjz, spotEdited.sigmalcjz);
    assignFromKeyfile(keyFile, "Locallab", "Clarilresjz_" + index_str, spot.clarilresjz, spotEdited.clarilresjz);
    assignFromKeyfile(keyFile, "Locallab", "Claricresjz_" + index_str, spot.claricresjz, spotEdited.claricresjz);
    assignFromKeyfile(keyFile, "Locallab", "Clarisoftjz_" + index_str, spot.clarisoftjz, spotEdited.clarisoftjz);
    assignFromKeyfile(keyFile, "Locallab", "LocwavCurvejz_" + index_str, spot.locwavcurvejz, spotEdited.locwavcurvejz);
    assignFromKeyfile(keyFile, "Locallab", "Contthrescie_" + index_str, spot.contthrescie, spotEdited.contthrescie);
    assignFromKeyfile(keyFile, "Locallab", "Contthrescie_" + index_str, spot.contthrescie, spotEdited.contthrescie);
    assignFromKeyfile(keyFile, "Locallab", "BlackEvjz_" + index_str, spot.blackEvjz, spotEdited.blackEvjz);
    assignFromKeyfile(keyFile, "Locallab", "WhiteEvjz_" + index_str, spot.whiteEvjz, spotEdited.whiteEvjz);
    assignFromKeyfile(keyFile, "Locallab", "Targetjz_" + index_str, spot.targetjz, spotEdited.targetjz);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidldacie12_" + index_str, spot.sigmoidldacie12, spotEdited.sigmoidldacie12);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidthcie12_" + index_str, spot.sigmoidthcie12, spotEdited.sigmoidthcie12);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidblcie12_" + index_str, spot.sigmoidblcie12, spotEdited.sigmoidblcie12);

    assignFromKeyfile(keyFile, "Locallab", "Sigmoidldacie_" + index_str, spot.sigmoidldacie, spotEdited.sigmoidldacie);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidthcie_" + index_str, spot.sigmoidthcie, spotEdited.sigmoidthcie);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidblcie_" + index_str, spot.sigmoidblcie, spotEdited.sigmoidblcie);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidsenscie_" + index_str, spot.sigmoidsenscie, spotEdited.sigmoidsenscie);

    assignFromKeyfile(keyFile, "Locallab", "comprcie_" + index_str, spot.comprcie, spotEdited.comprcie);
    assignFromKeyfile(keyFile, "Locallab", "strcielog_" + index_str, spot.strcielog, spotEdited.strcielog);
    assignFromKeyfile(keyFile, "Locallab", "comprcieth_" + index_str, spot.comprcieth, spotEdited.comprcieth);
    assignFromKeyfile(keyFile, "Locallab", "gamjcie_" + index_str, spot.gamjcie, spotEdited.gamjcie);
    assignFromKeyfile(keyFile, "Locallab", "smoothcieth_" + index_str, spot.smoothcieth, spotEdited.smoothcieth);
    assignFromKeyfile(keyFile, "Locallab", "smoothciethtrc_" + index_str, spot.smoothciethtrc, spotEdited.smoothciethtrc);
    assignFromKeyfile(keyFile, "Locallab", "slopjcie_" + index_str, spot.slopjcie, spotEdited.slopjcie);
    assignFromKeyfile(keyFile, "Locallab", "satjcie_" + index_str, spot.satjcie, spotEdited.satjcie);
    assignFromKeyfile(keyFile, "Locallab", "contsig_" + index_str, spot.contsig, spotEdited.contsig);
    assignFromKeyfile(keyFile, "Locallab", "skewsig_" + index_str, spot.skewsig, spotEdited.skewsig);
    assignFromKeyfile(keyFile, "Locallab", "whitsig_" + index_str, spot.whitsig, spotEdited.whitsig);
    assignFromKeyfile(keyFile, "Locallab", "slopesmo_" + index_str, spot.slopesmo, spotEdited.slopesmo);
    assignFromKeyfile(keyFile, "Locallab", "slopesmoq_" + index_str, spot.slopesmoq, spotEdited.slopesmoq);
    assignFromKeyfile(keyFile, "Locallab", "slopesmor_" + index_str, spot.slopesmor, spotEdited.slopesmor);
    assignFromKeyfile(keyFile, "Locallab", "slopesmog_" + index_str, spot.slopesmog, spotEdited.slopesmog);
    assignFromKeyfile(keyFile, "Locallab", "midtcie_" + index_str, spot.midtcie, spotEdited.midtcie);

    if (ppVersion < 353) {
        if (keyFile.has_key("Locallab", "midtcie_" + index_str)) {
            if (spot.midtcie != 0.) {//if midtone != 0 choose old method after gamma slope
                spot.midtciemet = "two";
                spotEdited.midtciemet = true;
            }
        }
    } else {   
        assignFromKeyfile(keyFile, "Locallab", "midtciemet_" + index_str, spot.midtciemet, spotEdited.midtciemet);                
    }

    assignFromKeyfile(keyFile, "Locallab", "slopesmob_" + index_str, spot.slopesmob, spotEdited.slopesmob);
    assignFromKeyfile(keyFile, "Locallab", "kslopesmor_" + index_str, spot.kslopesmor, spotEdited.kslopesmor);
    assignFromKeyfile(keyFile, "Locallab", "kslopesmog_" + index_str, spot.kslopesmog, spotEdited.kslopesmog);
    assignFromKeyfile(keyFile, "Locallab", "kslopesmob_" + index_str, spot.kslopesmob, spotEdited.kslopesmob);
    assignFromKeyfile(keyFile, "Locallab", "invcurve_" + index_str, spot.invcurve, spotEdited.invcurve);               
    assignFromKeyfile(keyFile, "Locallab", "grexl_" + index_str, spot.grexl, spotEdited.grexl);
    assignFromKeyfile(keyFile, "Locallab", "greyl_" + index_str, spot.greyl, spotEdited.greyl);
    assignFromKeyfile(keyFile, "Locallab", "bluxl_" + index_str, spot.bluxl, spotEdited.bluxl);
    assignFromKeyfile(keyFile, "Locallab", "bluyl_" + index_str, spot.bluyl, spotEdited.bluyl);
    assignFromKeyfile(keyFile, "Locallab", "redxl_" + index_str, spot.redxl, spotEdited.redxl);
    assignFromKeyfile(keyFile, "Locallab", "redyl_" + index_str, spot.redyl, spotEdited.redyl);
    assignFromKeyfile(keyFile, "Locallab", "refi_" + index_str, spot.refi, spotEdited.refi);
    assignFromKeyfile(keyFile, "Locallab", "shiftxl_" + index_str, spot.shiftxl, spotEdited.shiftxl);
    assignFromKeyfile(keyFile, "Locallab", "shiftyl_" + index_str, spot.shiftyl, spotEdited.shiftyl);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieALow_" + index_str, spot.labgridcieALow, spotEdited.labgridcieALow);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieBLow_" + index_str, spot.labgridcieBLow, spotEdited.labgridcieBLow);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieAHigh_" + index_str, spot.labgridcieAHigh, spotEdited.labgridcieAHigh);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieBHigh_" + index_str, spot.labgridcieBHigh, spotEdited.labgridcieBHigh);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieGx_" + index_str, spot.labgridcieGx, spotEdited.labgridcieGx);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieGy_" + index_str, spot.labgridcieGy, spotEdited.labgridcieGy);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieWx_" + index_str, spot.labgridcieWx, spotEdited.labgridcieWx);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieWy_" + index_str, spot.labgridcieWy, spotEdited.labgridcieWy);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieMx_" + index_str, spot.labgridcieMx, spotEdited.labgridcieMx);
    assignFromKeyfile(keyFile, "Locallab", "labgridcieMy_" + index_str, spot.labgridcieMy, spotEdited.labgridcieMy);

    assignFromKeyfile(keyFile, "Locallab", "whitescie_" + index_str, spot.whitescie, spotEdited.whitescie);
    assignFromKeyfile(keyFile, "Locallab", "blackscie_" + index_str, spot.blackscie, spotEdited.blackscie);
    assignFromKeyfile(keyFile, "Locallab", "illMethod_" + index_str, spot.illMethod, spotEdited.illMethod);
    assignFromKeyfile(keyFile, "Locallab", "smoothciemet_" + index_str, spot.smoothciemet, spotEdited.smoothciemet);
    assignFromKeyfile(keyFile, "Locallab", "primMethod_" + index_str, spot.primMethod, spotEdited.primMethod);
    assignFromKeyfile(keyFile, "Locallab", "catMethod_" + index_str, spot.catMethod, spotEdited.catMethod);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidldajzcie12_" + index_str, spot.sigmoidldajzcie12, spotEdited.sigmoidldajzcie12);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidthjzcie12_" + index_str, spot.sigmoidthjzcie12, spotEdited.sigmoidthjzcie12);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidbljzcie12_" + index_str, spot.sigmoidbljzcie12, spotEdited.sigmoidbljzcie12);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidldajzcie_" + index_str, spot.sigmoidldajzcie, spotEdited.sigmoidldajzcie);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidthjzcie_" + index_str, spot.sigmoidthjzcie, spotEdited.sigmoidthjzcie);
    assignFromKeyfile(keyFile, "Locallab", "Sigmoidbljzcie_" + index_str, spot.sigmoidbljzcie, spotEdited.sigmoidbljzcie);

    assignFromKeyfile(keyFile, "Locallab", "Contqcie_" + index_str, spot.contqcie, spotEdited.contqcie);
    assignFromKeyfile(keyFile, "Locallab", "Contsigqcie_" + index_str, spot.contsigqcie, spotEdited.contsigqcie);
    assignFromKeyfile(keyFile, "Locallab", "Colorflcie_" + index_str, spot.colorflcie, spotEdited.colorflcie);
    assignFromKeyfile(keyFile, "Locallab", "Targabscie_" + index_str, spot.targabscie, spotEdited.targabscie);
    assignFromKeyfile(keyFile, "Locallab", "TargetGraycie_" + index_str, spot.targetGraycie, spotEdited.targetGraycie);
    assignFromKeyfile(keyFile, "Locallab", "Catadcie_" + index_str, spot.catadcie, spotEdited.catadcie);
    assignFromKeyfile(keyFile, "Locallab", "Detailcie_" + index_str, spot.detailcie, spotEdited.detailcie);
    assignFromKeyfile(keyFile, "Locallab", "Surroundcie_" + index_str, spot.surroundcie, spotEdited.surroundcie);
    assignFromKeyfile(keyFile, "Locallab", "Strgradcie_" + index_str, spot.strgradcie, spotEdited.strgradcie);
    assignFromKeyfile(keyFile, "Locallab", "Anggradcie_" + index_str, spot.anggradcie, spotEdited.anggradcie);
    if (ppVersion <= 350) {
        if (keyFile.has_key("Locallab", "Feather_" + index_str)) {
            spot.feathercie = keyFile.get_integer("Locallab", "Feather_" + index_str);
            spotEdited.feathercie = true;
        }
    } else {
        assignFromKeyfile(keyFile, "Locallab", "Feathercie_" + index_str, spot.feathercie, spotEdited.feathercie);
    }

    assignFromKeyfile(keyFile, "Locallab", "EnacieMask_" + index_str,  spot.enacieMask, spotEdited.enacieMask);
    assignFromKeyfile(keyFile, "Locallab", "EnacieMaskall_" + index_str,  spot.enacieMaskall, spotEdited.enacieMaskall);
    assignFromKeyfile(keyFile, "Locallab", "CCmaskcieCurve_" + index_str, spot.CCmaskciecurve, spotEdited.CCmaskciecurve);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskcieCurve_" + index_str, spot.LLmaskciecurve, spotEdited.LLmaskciecurve);
    assignFromKeyfile(keyFile, "Locallab", "HHmaskcieCurve_" + index_str,  spot.HHmaskciecurve, spotEdited.HHmaskciecurve);
    assignFromKeyfile(keyFile, "Locallab", "HHhmaskcieCurve_" + index_str, spot.HHhmaskciecurve, spotEdited.HHhmaskciecurve);
    assignFromKeyfile(keyFile, "Locallab", "Blendmaskcie_" + index_str, spot.blendmaskcie, spotEdited.blendmaskcie);
    assignFromKeyfile(keyFile, "Locallab", "Radmaskcie_" + index_str, spot.radmaskcie, spotEdited.radmaskcie);
    assignFromKeyfile(keyFile, "Locallab", "Chromaskcie_" + index_str, spot.chromaskcie, spotEdited.chromaskcie);
    assignFromKeyfile(keyFile, "Locallab", "Lapmaskcie_" + index_str, spot.lapmaskcie, spotEdited.lapmaskcie);
    assignFromKeyfile(keyFile, "Locallab", "Gammaskcie_" + index_str, spot.gammaskcie, spotEdited.gammaskcie);
    assignFromKeyfile(keyFile, "Locallab", "Slomaskcie_" + index_str, spot.slomaskcie, spotEdited.slomaskcie);
    assignFromKeyfile(keyFile, "Locallab", "LmaskcieCurve_" + index_str, spot.Lmaskciecurve, spotEdited.Lmaskciecurve);
    assignFromKeyfile(keyFile, "Locallab", "Recothrescie_" + index_str, spot.recothrescie, spotEdited.recothrescie);
    assignFromKeyfile(keyFile, "Locallab", "Lowthrescie_" + index_str, spot.lowthrescie, spotEdited.lowthrescie);
    assignFromKeyfile(keyFile, "Locallab", "Higthrescie_" + index_str, spot.higthrescie, spotEdited.higthrescie);
    assignFromKeyfile(keyFile, "Locallab", "Decaycie_" + index_str, spot.decaycie, spotEdited.decaycie);
    assignFromKeyfile(keyFile, "Locallab", "strumaskcie_" + index_str, spot.strumaskcie, spotEdited.strumaskcie);
    assignFromKeyfile(keyFile, "Locallab", "toolcie_" + index_str, spot.toolcie, spotEdited.toolcie);
    assignFromKeyfile(keyFile, "Locallab", "FftcieMask_" + index_str, spot.fftcieMask, spotEdited.fftcieMask);
    assignFromKeyfile(keyFile, "Locallab", "contcie_" + index_str, spot.contcie, spotEdited.contcie);
    assignFromKeyfile(keyFile, "Locallab", "blurcie_" + index_str, spot.blurcie, spotEdited.blurcie);
    assignFromKeyfile(keyFile, "Locallab", "highmaskcie_" + index_str, spot.highmaskcie, spotEdited.highmaskcie);
    assignFromKeyfile(keyFile, "Locallab", "shadmaskcie_" + index_str, spot.shadmaskcie, spotEdited.shadmaskcie);
    assignFromKeyfile(keyFile, "Locallab", "LLmaskcieCurvewav_" + index_str, spot.LLmaskciecurvewav, spotEdited.LLmaskciecurvewav);

    if (ppVersion < 352) {
        // FFT Gaussian blur fixed in 5.12. Converts old radii to
        // the equivalent new radii if FFT mode is enabled. The
        // fixed blur radius has a factor of radius / 2 compared to
        // the original, so the base conversion is sqrt(2 * radius).
        // Derivation: old_radius = new_radius * new_radius / 2
        //             2 * old_radius = new_radius * new_radius
        //             sqrt(2 * old_radius) = new_radius
        if (spot.fftColorMask) {
            spot.blurcol = std::sqrt(2 * spot.blurcol);
            spotEdited.blurcol = true;
        }
        if (spot.fftwbl || spot.radius > 30.) {
            // Internally, the radius is divided by 1.4, so the
            // factor is actually radius / 1.4 / 2.
            spot.radius = std::sqrt(2.8 * spot.radius);
            spotEdited.radius = true;
        }
        if (spot.fftwlc) {
            // Internally, the radius was squared, so replace
            // old_radius with old_radius^2 before solving.
            spot.lcradius = std::sqrt(2.) * spot.lcradius;
            spotEdited.lcradius = true;
        }
        if (spot.fftmask) {
            spot.blurmask = std::sqrt(2 * spot.blurmask);
            spotEdited.blurmask = true;
        }
        if (spot.fftcieMask) {
            spot.blurcie = std::sqrt(2 * spot.blurcie);
            spotEdited.blurcie = true;
        }
    }

    if (keyFile.has_key("Locallab", "CSThresholdcie_" + index_str)) {
        const std::vector<int> thresh = keyFile.get_integer_list("Locallab", "CSThresholdcie_" + index_str);

        if (thresh.size() >= 4) {
            spot.csthresholdcie.setValues(thresh[0], thresh[1], min(thresh[2], 10), min(thresh[3], 10));
        }

        spotEdited.csthresholdcie = true;
    }
}

void saveLocalLabParams(Glib::KeyFile& keyFile, const LocallabParams& locallab,
                        const ParamsEdited* pedited)
{
    saveToKeyfile(!pedited || pedited->locallab.enabled, "Locallab", "Enabled", locallab.enabled, keyFile);
    saveToKeyfile(!pedited || pedited->locallab.selspot, "Locallab", "Selspot", locallab.selspot, keyFile);

    for (size_t i = 0; i < locallab.spots.size(); ++i) {
        if (!(!pedited || i < pedited->locallab.spots.size())) continue;

        const LocallabParams::LocallabSpot& spot = locallab.spots.at(i);
        const LocallabParamsEdited::LocallabSpotEdited* const spot_edited =
            pedited ? &pedited->locallab.spots.at(i) : nullptr;
        const std::string index_str = std::to_string(i);

        SaveUtil serialize(keyFile, spot, pedited, spot_edited, index_str);

        serialize.controlSpotSettings();
        serialize.colorLight();
        serialize.exposure();
        serialize.shadowHighlight();
        serialize.vibrance();
        serialize.softLight();
        serialize.blurNoise();
        serialize.toneMapping();
        serialize.retinex();
        serialize.sharpening();
        serialize.localContrast();
        serialize.contrastByDetailLevels();
        serialize.logEncoding();
        serialize.mask();
        serialize.ciecam();
    }
}

void SaveUtil::controlSpotSettings()
{
    saveToKeyfile(!pedited || spot_edited->name, "Locallab", "Name_" + index_str, spot.name, keyFile);
    saveToKeyfile(!pedited || spot_edited->isvisible, "Locallab", "Isvisible_" + index_str, spot.isvisible, keyFile);
    saveToKeyfile(!pedited || spot_edited->prevMethod, "Locallab", "PrevMethod_" + index_str, spot.prevMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->shape, "Locallab", "Shape_" + index_str, spot.shape, keyFile);
    saveToKeyfile(!pedited || spot_edited->spotMethod, "Locallab", "SpotMethod_" + index_str, spot.spotMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->wavMethod, "Locallab", "WavMethod_" + index_str, spot.wavMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->sensiexclu, "Locallab", "SensiExclu_" + index_str, spot.sensiexclu, keyFile);
    saveToKeyfile(!pedited || spot_edited->structexclu, "Locallab", "StructExclu_" + index_str, spot.structexclu, keyFile);
    saveToKeyfile(!pedited || spot_edited->struc, "Locallab", "Struc_" + index_str, spot.struc, keyFile);
    saveToKeyfile(!pedited || spot_edited->shapeMethod, "Locallab", "ShapeMethod_" + index_str, spot.shapeMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->avoidgamutMethod, "Locallab", "AvoidgamutMethod_" + index_str, spot.avoidgamutMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->loc, "Locallab", "Loc_" + index_str, spot.loc, keyFile);
    saveToKeyfile(!pedited || spot_edited->centerX, "Locallab", "CenterX_" + index_str, spot.centerX, keyFile);
    saveToKeyfile(!pedited || spot_edited->centerY, "Locallab", "CenterY_" + index_str, spot.centerY, keyFile);
    saveToKeyfile(!pedited || spot_edited->circrad, "Locallab", "Circrad_" + index_str, spot.circrad, keyFile);
    saveToKeyfile(!pedited || spot_edited->qualityMethod, "Locallab", "QualityMethod_" + index_str, spot.qualityMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->complexMethod, "Locallab", "ComplexMethod_" + index_str, spot.complexMethod, keyFile);
    saveToKeyfile(!pedited || spot_edited->transit, "Locallab", "Transit_" + index_str, spot.transit, keyFile);
    saveToKeyfile(!pedited || spot_edited->feather, "Locallab", "Feather_" + index_str, spot.feather, keyFile);
    saveToKeyfile(!pedited || spot_edited->thresh, "Locallab", "Thresh_" + index_str, spot.thresh, keyFile);
    saveToKeyfile(!pedited || spot_edited->iter, "Locallab", "Iter_" + index_str, spot.iter, keyFile);
    saveToKeyfile(!pedited || spot_edited->balan, "Locallab", "Balan_" + index_str, spot.balan, keyFile);
    saveToKeyfile(!pedited || spot_edited->balanh, "Locallab", "Balanh_" + index_str, spot.balanh, keyFile);
    saveToKeyfile(!pedited || spot_edited->colorde, "Locallab", "Colorde_" + index_str, spot.colorde, keyFile);
    saveToKeyfile(!pedited || spot_edited->colorscope, "Locallab", "Colorscope_" + index_str, spot.colorscope, keyFile);
    saveToKeyfile(!pedited || spot_edited->avoidrad, "Locallab", "Avoidrad_" + index_str, spot.avoidrad, keyFile);
    saveToKeyfile(!pedited || spot_edited->transitweak, "Locallab", "Transitweak_" + index_str, spot.transitweak, keyFile);
    saveToKeyfile(!pedited || spot_edited->transitgrad, "Locallab", "Transitgrad_" + index_str, spot.transitgrad, keyFile);
    saveToKeyfile(!pedited || spot_edited->hishow, "Locallab", "Hishow_" + index_str, spot.hishow, keyFile);
    saveToKeyfile(!pedited || spot_edited->activ, "Locallab", "Activ_" + index_str, spot.activ, keyFile);
    saveToKeyfile(!pedited || spot_edited->avoidneg, "Locallab", "Avoidneg_" + index_str, spot.avoidneg, keyFile);
    saveToKeyfile(!pedited || spot_edited->blwh, "Locallab", "Blwh_" + index_str, spot.blwh, keyFile);
    saveToKeyfile(!pedited || spot_edited->recurs, "Locallab", "Recurs_" + index_str, spot.recurs, keyFile);
    saveToKeyfile(!pedited || spot_edited->laplac, "Locallab", "Laplac_" + index_str, spot.laplac, keyFile);
    saveToKeyfile(!pedited || spot_edited->deltae, "Locallab", "Deltae_" + index_str, spot.deltae, keyFile);
    saveToKeyfile(!pedited || spot_edited->shortc, "Locallab", "Shortc_" + index_str, spot.shortc, keyFile);
    saveToKeyfile(!pedited || spot_edited->savrest, "Locallab", "Savrest_" + index_str, spot.savrest, keyFile);
    saveToKeyfile(!pedited || spot_edited->scopemask, "Locallab", "Scopemask_" + index_str, spot.scopemask, keyFile);
    saveToKeyfile(!pedited || spot_edited->denoichmask, "Locallab", "Denoichmask_" + index_str, spot.denoichmask, keyFile);
    saveToKeyfile(!pedited || spot_edited->lumask, "Locallab", "Lumask_" + index_str, spot.lumask, keyFile);
}

void SaveUtil::colorLight()
{
    if ((!pedited || spot_edited->visicolor) && spot.visicolor) {
        saveToKeyfile(!pedited || spot_edited->expcolor, "Locallab", "Expcolor_" + index_str, spot.expcolor, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexcolor, "Locallab", "Complexcolor_" + index_str, spot.complexcolor, keyFile);
        saveToKeyfile(!pedited || spot_edited->curvactiv, "Locallab", "Curvactiv_" + index_str, spot.curvactiv, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightness, "Locallab", "Lightness_" + index_str, spot.lightness, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparcol, "Locallab", "Reparcol_" + index_str, spot.reparcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamc, "Locallab", "Gamc_" + index_str, spot.gamc, keyFile);
        saveToKeyfile(!pedited || spot_edited->contrast, "Locallab", "Contrast_" + index_str, spot.contrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->chroma, "Locallab", "Chroma_" + index_str, spot.chroma, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridALow, "Locallab", "labgridALow_" + index_str, spot.labgridALow, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridBLow, "Locallab", "labgridBLow_" + index_str, spot.labgridBLow, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridAHigh, "Locallab", "labgridAHigh_" + index_str, spot.labgridAHigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridBHigh, "Locallab", "labgridBHigh_" + index_str, spot.labgridBHigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridALowmerg, "Locallab", "labgridALowmerg_" + index_str, spot.labgridALowmerg, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridBLowmerg, "Locallab", "labgridBLowmerg_" + index_str, spot.labgridBLowmerg, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridAHighmerg, "Locallab", "labgridAHighmerg_" + index_str, spot.labgridAHighmerg, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridBHighmerg, "Locallab", "labgridBHighmerg_" + index_str, spot.labgridBHighmerg, keyFile);
        saveToKeyfile(!pedited || spot_edited->strengthgrid, "Locallab", "Strengthgrid_" + index_str, spot.strengthgrid, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensi, "Locallab", "Sensi_" + index_str, spot.sensi, keyFile);
        saveToKeyfile(!pedited || spot_edited->structcol, "Locallab", "Structcol_" + index_str, spot.structcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->strcol, "Locallab", "Strcol_" + index_str, spot.strcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->strcolab, "Locallab", "Strcolab_" + index_str, spot.strcolab, keyFile);
        saveToKeyfile(!pedited || spot_edited->strcolh, "Locallab", "Strcolh_" + index_str, spot.strcolh, keyFile);
        saveToKeyfile(!pedited || spot_edited->angcol, "Locallab", "Angcol_" + index_str, spot.angcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->feathercol, "Locallab", "Feathercol_" + index_str, spot.feathercol, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurcolde, "Locallab", "Blurcolde_" + index_str, spot.blurcolde, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurcol, "Locallab", "Blurcol_" + index_str, spot.blurcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->contcol, "Locallab", "Contcol_" + index_str, spot.contcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskcol, "Locallab", "Blendmaskcol_" + index_str, spot.blendmaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskcol, "Locallab", "Radmaskcol_" + index_str, spot.radmaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskcol, "Locallab", "Chromaskcol_" + index_str, spot.chromaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskcol, "Locallab", "Gammaskcol_" + index_str, spot.gammaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskcol, "Locallab", "Slomaskcol_" + index_str, spot.slomaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->shadmaskcol, "Locallab", "shadmaskcol_" + index_str, spot.shadmaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->strumaskcol, "Locallab", "strumaskcol_" + index_str, spot.strumaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskcol, "Locallab", "Lapmaskcol_" + index_str, spot.lapmaskcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->qualitycurveMethod, "Locallab", "QualityCurveMethod_" + index_str, spot.qualitycurveMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->gridMethod, "Locallab", "gridMethod_" + index_str, spot.gridMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->merMethod, "Locallab", "Merg_Method_" + index_str, spot.merMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->toneMethod, "Locallab", "ToneMethod_" + index_str, spot.toneMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->mergecolMethod, "Locallab", "mergecolMethod_" + index_str, spot.mergecolMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->llcurve, "Locallab", "LLCurve_" + index_str, spot.llcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->lccurve, "Locallab", "LCCurve_" + index_str, spot.lccurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->cccurve, "Locallab", "CCCurve_" + index_str, spot.cccurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->clcurve, "Locallab", "CLCurve_" + index_str, spot.clcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->rgbcurve, "Locallab", "RGBCurve_" + index_str, spot.rgbcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LHcurve, "Locallab", "LHCurve_" + index_str, spot.LHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHcurve, "Locallab", "HHCurve_" + index_str, spot.HHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->CHcurve, "Locallab", "CHCurve_" + index_str, spot.CHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->invers, "Locallab", "Invers_" + index_str, spot.invers, keyFile);
        saveToKeyfile(!pedited || spot_edited->special, "Locallab", "Special_" + index_str, spot.special, keyFile);
        saveToKeyfile(!pedited || spot_edited->toolcol, "Locallab", "Toolcol_" + index_str, spot.toolcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaColorMask, "Locallab", "EnaColorMask_" + index_str, spot.enaColorMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->fftColorMask, "Locallab", "FftColorMask_" + index_str, spot.fftColorMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskcurve, "Locallab", "CCmaskCurve_" + index_str, spot.CCmaskcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskcurve, "Locallab", "LLmaskCurve_" + index_str, spot.LLmaskcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskcurve, "Locallab", "HHmaskCurve_" + index_str, spot.HHmaskcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHhmaskcurve, "Locallab", "HHhmaskCurve_" + index_str, spot.HHhmaskcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->softradiuscol, "Locallab", "Softradiuscol_" + index_str, spot.softradiuscol, keyFile);
        saveToKeyfile(!pedited || spot_edited->opacol, "Locallab", "Opacol_" + index_str, spot.opacol, keyFile);
        saveToKeyfile(!pedited || spot_edited->mercol, "Locallab", "Mercol_" + index_str, spot.mercol, keyFile);
        saveToKeyfile(!pedited || spot_edited->merlucol, "Locallab", "Merlucol_" + index_str, spot.merlucol, keyFile);
        saveToKeyfile(!pedited || spot_edited->conthrcol, "Locallab", "Conthrcol_" + index_str, spot.conthrcol, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskcurve, "Locallab", "LmaskCurve_" + index_str, spot.Lmaskcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskcolcurvewav, "Locallab", "LLmaskcolCurvewav_" + index_str, spot.LLmaskcolcurvewav, keyFile);
        saveToKeyfile(!pedited || spot_edited->csthresholdcol, "Locallab", "CSThresholdcol_" + index_str, spot.csthresholdcol.toVector(), keyFile);
        saveToKeyfile(!pedited || spot_edited->recothresc, "Locallab", "Recothresc_" + index_str, spot.recothresc, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthresc, "Locallab", "Lowthresc_" + index_str, spot.lowthresc, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthresc, "Locallab", "Higthresc_" + index_str, spot.higthresc, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayc, "Locallab", "Decayc_" + index_str, spot.decayc, keyFile);
    }
}

void SaveUtil::exposure()
{
    if ((!pedited || spot_edited->visiexpose) && spot.visiexpose) {
        saveToKeyfile(!pedited || spot_edited->expexpose, "Locallab", "Expexpose_" + index_str, spot.expexpose, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexexpose, "Locallab", "Complexexpose_" + index_str, spot.complexexpose, keyFile);
        saveToKeyfile(!pedited || spot_edited->expcomp, "Locallab", "Expcomp_" + index_str, spot.expcomp, keyFile);
        saveToKeyfile(!pedited || spot_edited->hlcompr, "Locallab", "Hlcompr_" + index_str, spot.hlcompr, keyFile);
        saveToKeyfile(!pedited || spot_edited->hlcomprthresh, "Locallab", "Hlcomprthresh_" + index_str, spot.hlcomprthresh, keyFile);
        saveToKeyfile(!pedited || spot_edited->black, "Locallab", "Black_" + index_str, spot.black, keyFile);
        saveToKeyfile(!pedited || spot_edited->shadex, "Locallab", "Shadex_" + index_str, spot.shadex, keyFile);
        saveToKeyfile(!pedited || spot_edited->shcompr, "Locallab", "Shcompr_" + index_str, spot.shcompr, keyFile);
        saveToKeyfile(!pedited || spot_edited->expchroma, "Locallab", "Expchroma_" + index_str, spot.expchroma, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensiex, "Locallab", "Sensiex_" + index_str, spot.sensiex, keyFile);
        saveToKeyfile(!pedited || spot_edited->structexp, "Locallab", "Structexp_" + index_str, spot.structexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurexpde, "Locallab", "Blurexpde_" + index_str, spot.blurexpde, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamex, "Locallab", "Gamex_" + index_str, spot.gamex, keyFile);
        saveToKeyfile(!pedited || spot_edited->strexp, "Locallab", "Strexp_" + index_str, spot.strexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->angexp, "Locallab", "Angexp_" + index_str, spot.angexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->featherexp, "Locallab", "Featherexp_" + index_str, spot.featherexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->excurve, "Locallab", "ExCurve_" + index_str, spot.excurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->norm, "Locallab", "Norm_" + index_str, spot.norm, keyFile);
        saveToKeyfile(!pedited || spot_edited->inversex, "Locallab", "Inversex_" + index_str, spot.inversex, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaExpMask, "Locallab", "EnaExpMask_" + index_str, spot.enaExpMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaExpMaskaft, "Locallab", "EnaExpMaskaft_" + index_str, spot.enaExpMaskaft, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskexpcurve, "Locallab", "CCmaskexpCurve_" + index_str, spot.CCmaskexpcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskexpcurve, "Locallab", "LLmaskexpCurve_" + index_str, spot.LLmaskexpcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskexpcurve, "Locallab", "HHmaskexpCurve_" + index_str, spot.HHmaskexpcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskexp, "Locallab", "Blendmaskexp_" + index_str, spot.blendmaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskexp, "Locallab", "Radmaskexp_" + index_str, spot.radmaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskexp, "Locallab", "Chromaskexp_" + index_str, spot.chromaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskexp, "Locallab", "Gammaskexp_" + index_str, spot.gammaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskexp, "Locallab", "Slomaskexp_" + index_str, spot.slomaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskexp, "Locallab", "Lapmaskexp_" + index_str, spot.lapmaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->strmaskexp, "Locallab", "Strmaskexp_" + index_str, spot.strmaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->angmaskexp, "Locallab", "Angmaskexp_" + index_str, spot.angmaskexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->softradiusexp, "Locallab", "Softradiusexp_" + index_str, spot.softradiusexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskexpcurve, "Locallab", "LmaskexpCurve_" + index_str, spot.Lmaskexpcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->expMethod, "Locallab", "ExpMethod_" + index_str, spot.expMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->exnoiseMethod, "Locallab", "ExnoiseMethod_" + index_str, spot.exnoiseMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->laplacexp, "Locallab", "Laplacexp_" + index_str, spot.laplacexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparexp, "Locallab", "Reparexp_" + index_str, spot.reparexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->balanexp, "Locallab", "Balanexp_" + index_str, spot.balanexp, keyFile);
        saveToKeyfile(!pedited || spot_edited->linear, "Locallab", "Linearexp_" + index_str, spot.linear, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamm, "Locallab", "Gamm_" + index_str, spot.gamm, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatamount, "Locallab", "Fatamount_" + index_str, spot.fatamount, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatdetail, "Locallab", "Fatdetail_" + index_str, spot.fatdetail, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatsatur, "Locallab", "Fatsatur_" + index_str, spot.fatsatur, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatanchor, "Locallab", "Fatanchor_" + index_str, spot.fatanchor, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatlevel, "Locallab", "Fatlevel_" + index_str, spot.fatlevel, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothrese, "Locallab", "Recothrese_" + index_str, spot.recothrese, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthrese, "Locallab", "Lowthrese_" + index_str, spot.lowthrese, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthrese, "Locallab", "Higthrese_" + index_str, spot.higthrese, keyFile);
        saveToKeyfile(!pedited || spot_edited->decaye, "Locallab", "Decaye_" + index_str, spot.decaye, keyFile);
    }
}

void SaveUtil::shadowHighlight()
{
    if ((!pedited || spot_edited->visishadhigh) && spot.visishadhigh) {
        saveToKeyfile(!pedited || spot_edited->expshadhigh, "Locallab", "Expshadhigh_" + index_str, spot.expshadhigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexshadhigh, "Locallab", "Complexshadhigh_" + index_str, spot.complexshadhigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->shMethod, "Locallab", "ShMethod_" + index_str, spot.shMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghsMethod, "Locallab", "GhsMethod_" + index_str, spot.ghsMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghsMatmet, "Locallab", "GhsMatmet_" + index_str, spot.ghsMatmet, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghsMode, "Locallab", "GhsMode_" + index_str, spot.ghsMode, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_D, "Locallab", "Ghs_D_" + index_str, spot.ghs_D, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_mtfstr, "Locallab", "Ghs_mtfstr_" + index_str, spot.ghs_mtfstr, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_slope, "Locallab", "Ghs_slope_" + index_str, spot.ghs_slope, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_chro, "Locallab", "Ghs_chro_" + index_str, spot.ghs_chro, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_B, "Locallab", "Ghs_B_" + index_str, spot.ghs_B, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_SP, "Locallab", "Ghs_SP_" + index_str, spot.ghs_SP, keyFile);
        saveToKeyfile(!pedited || spot_edited->SPAutoRadius, "Locallab", "SPAutoRadius_" + index_str, spot.SPAutoRadius, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_LP, "Locallab", "Ghs_LP_" + index_str, spot.ghs_LP, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_HP, "Locallab", "Ghs_HP_" + index_str, spot.ghs_HP, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_LC, "Locallab", "Ghs_LC_" + index_str, spot.ghs_LC, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_MID, "Locallab", "Ghs_MID_" + index_str, spot.ghs_MID, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_BLP, "Locallab", "Ghs_BLP_" + index_str, spot.ghs_BLP, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_HLP, "Locallab", "Ghs_HLP_" + index_str, spot.ghs_HLP, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_autobw, "Locallab", "Ghs_autobw_" + index_str, spot.ghs_autobw, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_agx, "Locallab", "Ghs_agx_" + index_str, spot.ghs_agx, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_mtf, "Locallab", "Ghs_mtf_" + index_str, spot.ghs_mtf, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_smooth, "Locallab", "Ghs_smooth_" + index_str, spot.ghs_smooth, keyFile);
        saveToKeyfile(!pedited || spot_edited->ghs_inv, "Locallab", "Ghs_inv_" + index_str, spot.ghs_inv, keyFile);

        saveToKeyfile(!pedited || spot_edited->mich_exp, "Locallab", "Mich_exp_" + index_str, spot.mich_exp, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_spar, "Locallab", "Mich_spar_" + index_str, spot.mich_spar, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_kpar, "Locallab", "Mich_kpar_" + index_str, spot.mich_kpar, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_sat, "Locallab", "Mich_sat_" + index_str, spot.mich_sat, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_out, "Locallab", "Mich_out_" + index_str, spot.mich_out, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_black, "Locallab", "Mich_black_" + index_str, spot.mich_black, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_white, "Locallab", "Mich_white_" + index_str, spot.mich_white, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_high, "Locallab", "Mich_high_" + index_str, spot.mich_high, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_jdx, "Locallab", "Mich_jdx_" + index_str, spot.mich_jdx, keyFile);
        saveToKeyfile(!pedited || spot_edited->mich_mtf, "Locallab", "Mich_mtf_" + index_str, spot.mich_mtf, keyFile);

        for (int j = 0; j < 6; j++) {
            saveToKeyfile(!pedited || spot_edited->multsh[j], "Locallab", "Multsh" + std::to_string(j) + "_" + index_str, spot.multsh[j], keyFile);
        }

        saveToKeyfile(!pedited || spot_edited->highlights, "Locallab", "highlights_" + index_str, spot.highlights, keyFile);
        saveToKeyfile(!pedited || spot_edited->h_tonalwidth, "Locallab", "h_tonalwidth_" + index_str, spot.h_tonalwidth, keyFile);
        saveToKeyfile(!pedited || spot_edited->shadows, "Locallab", "shadows_" + index_str, spot.shadows, keyFile);
        saveToKeyfile(!pedited || spot_edited->s_tonalwidth, "Locallab", "s_tonalwidth_" + index_str, spot.s_tonalwidth, keyFile);
        saveToKeyfile(!pedited || spot_edited->sh_radius, "Locallab", "sh_radius_" + index_str, spot.sh_radius, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensihs, "Locallab", "sensihs_" + index_str, spot.sensihs, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaSHMask, "Locallab", "EnaSHMask_" + index_str, spot.enaSHMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskSHcurve, "Locallab", "CCmaskSHCurve_" + index_str, spot.CCmaskSHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskSHcurve, "Locallab", "LLmaskSHCurve_" + index_str, spot.LLmaskSHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskSHcurve, "Locallab", "HHmaskSHCurve_" + index_str, spot.HHmaskSHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskSH, "Locallab", "BlendmaskSH_" + index_str, spot.blendmaskSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskSH, "Locallab", "RadmaskSH_" + index_str, spot.radmaskSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurSHde, "Locallab", "BlurSHde_" + index_str, spot.blurSHde, keyFile);
        saveToKeyfile(!pedited || spot_edited->strSH, "Locallab", "StrSH_" + index_str, spot.strSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->angSH, "Locallab", "AngSH_" + index_str, spot.angSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->featherSH, "Locallab", "FeatherSH_" + index_str, spot.featherSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->inverssh, "Locallab", "Inverssh_" + index_str, spot.inverssh, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskSH, "Locallab", "ChromaskSH_" + index_str, spot.chromaskSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskSH, "Locallab", "GammaskSH_" + index_str, spot.gammaskSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskSH, "Locallab", "SlomaskSH_" + index_str, spot.slomaskSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->detailSH, "Locallab", "DetailSH_" + index_str, spot.detailSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->tePivot, "Locallab", "TePivot_" + index_str, spot.tePivot, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparsh, "Locallab", "Reparsh_" + index_str, spot.reparsh, keyFile);
        saveToKeyfile(!pedited || spot_edited->LmaskSHcurve, "Locallab", "LmaskSHCurve_" + index_str, spot.LmaskSHcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatamountSH, "Locallab", "FatamountSH_" + index_str, spot.fatamountSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatanchorSH, "Locallab", "FatanchorSH_" + index_str, spot.fatanchorSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamSH, "Locallab", "GamSH_" + index_str, spot.gamSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->sloSH, "Locallab", "SloSH_" + index_str, spot.sloSH, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothress, "Locallab", "Recothress_" + index_str, spot.recothress, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthress, "Locallab", "Lowthress_" + index_str, spot.lowthress, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthress, "Locallab", "Higthress_" + index_str, spot.higthress, keyFile);
        saveToKeyfile(!pedited || spot_edited->decays, "Locallab", "Decays_" + index_str, spot.decays, keyFile);
    }
}

void SaveUtil::vibrance()
{
    if ((!pedited || spot_edited->visivibrance) && spot.visivibrance) {
        saveToKeyfile(!pedited || spot_edited->expvibrance, "Locallab", "Expvibrance_" + index_str, spot.expvibrance, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexvibrance, "Locallab", "Complexvibrance_" + index_str, spot.complexvibrance, keyFile);
        saveToKeyfile(!pedited || spot_edited->saturated, "Locallab", "Saturated_" + index_str, spot.saturated, keyFile);
        saveToKeyfile(!pedited || spot_edited->pastels, "Locallab", "Pastels_" + index_str, spot.pastels, keyFile);
        saveToKeyfile(!pedited || spot_edited->vibgam, "Locallab", "Vibgam_" + index_str, spot.vibgam, keyFile);
        saveToKeyfile(!pedited || spot_edited->warm, "Locallab", "Warm_" + index_str, spot.warm, keyFile);
        saveToKeyfile(!pedited || spot_edited->psthreshold, "Locallab", "PSThreshold_" + index_str, spot.psthreshold.toVector(), keyFile);
        saveToKeyfile(!pedited || spot_edited->protectskins, "Locallab", "ProtectSkins_" + index_str, spot.protectskins, keyFile);
        saveToKeyfile(!pedited || spot_edited->avoidcolorshift, "Locallab", "AvoidColorShift_" + index_str, spot.avoidcolorshift, keyFile);
        saveToKeyfile(!pedited || spot_edited->pastsattog, "Locallab", "PastSatTog_" + index_str, spot.pastsattog, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensiv, "Locallab", "Sensiv_" + index_str, spot.sensiv, keyFile);
        saveToKeyfile(!pedited || spot_edited->skintonescurve, "Locallab", "SkinTonesCurve_" + index_str, spot.skintonescurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskvibcurve, "Locallab", "CCmaskvibCurve_" + index_str, spot.CCmaskvibcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskvibcurve, "Locallab", "LLmaskvibCurve_" + index_str, spot.LLmaskvibcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskvibcurve, "Locallab", "HHmaskvibCurve_" + index_str, spot.HHmaskvibcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->enavibMask, "Locallab", "EnavibMask_" + index_str, spot.enavibMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskvib, "Locallab", "Blendmaskvib_" + index_str, spot.blendmaskvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskvib, "Locallab", "Radmaskvib_" + index_str, spot.radmaskvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskvib, "Locallab", "Chromaskvib_" + index_str, spot.chromaskvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskvib, "Locallab", "Gammaskvib_" + index_str, spot.gammaskvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskvib, "Locallab", "Slomaskvib_" + index_str, spot.slomaskvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskvib, "Locallab", "Lapmaskvib_" + index_str, spot.lapmaskvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->strvib, "Locallab", "Strvib_" + index_str, spot.strvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->strvibab, "Locallab", "Strvibab_" + index_str, spot.strvibab, keyFile);
        saveToKeyfile(!pedited || spot_edited->strvibh, "Locallab", "Strvibh_" + index_str, spot.strvibh, keyFile);
        saveToKeyfile(!pedited || spot_edited->angvib, "Locallab", "Angvib_" + index_str, spot.angvib, keyFile);
        saveToKeyfile(!pedited || spot_edited->angvib, "Locallab", "Feathervib_" + index_str, spot.feathervib, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskvibcurve, "Locallab", "LmaskvibCurve_" + index_str, spot.Lmaskvibcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothresv, "Locallab", "Recothresv_" + index_str, spot.recothresv, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthresv, "Locallab", "Lowthresv_" + index_str, spot.lowthresv, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthresv, "Locallab", "Higthresv_" + index_str, spot.higthresv, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayv, "Locallab", "Decayv_" + index_str, spot.decayv, keyFile);
    }
}

void SaveUtil::softLight()
{
    if ((!pedited || spot_edited->visisoft) && spot.visisoft) {
        saveToKeyfile(!pedited || spot_edited->expsoft, "Locallab", "Expsoft_" + index_str, spot.expsoft, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexsoft, "Locallab", "Complexsoft_" + index_str, spot.complexsoft, keyFile);
        saveToKeyfile(!pedited || spot_edited->streng, "Locallab", "Streng_" + index_str, spot.streng, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensisf, "Locallab", "Sensisf_" + index_str, spot.sensisf, keyFile);
        saveToKeyfile(!pedited || spot_edited->laplace, "Locallab", "Laplace_" + index_str, spot.laplace, keyFile);
        saveToKeyfile(!pedited || spot_edited->softMethod, "Locallab", "SoftMethod_" + index_str, spot.softMethod, keyFile);
    }
}

void SaveUtil::blurNoise()
{
    if ((!pedited || spot_edited->visiblur) && spot.visiblur) {
        saveToKeyfile(!pedited || spot_edited->expblur, "Locallab", "Expblur_" + index_str, spot.expblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexblur, "Locallab", "Complexblur_" + index_str, spot.complexblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->radius, "Locallab", "Radius_" + index_str, spot.radius, keyFile);
        saveToKeyfile(!pedited || spot_edited->strength, "Locallab", "Strength_" + index_str, spot.strength, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensibn, "Locallab", "Sensibn_" + index_str, spot.sensibn, keyFile);
        saveToKeyfile(!pedited || spot_edited->itera, "Locallab", "Iteramed_" + index_str, spot.itera, keyFile);
        saveToKeyfile(!pedited || spot_edited->guidbl, "Locallab", "Guidbl_" + index_str, spot.guidbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->strbl, "Locallab", "Strbl_" + index_str, spot.strbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothres, "Locallab", "Recothres_" + index_str, spot.recothres, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthres, "Locallab", "Lowthres_" + index_str, spot.lowthres, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthres, "Locallab", "Higthres_" + index_str, spot.higthres, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothresd, "Locallab", "Recothresd_" + index_str, spot.recothresd, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthresd, "Locallab", "Lowthresd_" + index_str, spot.lowthresd, keyFile);
        saveToKeyfile(!pedited || spot_edited->midthresd, "Locallab", "Midthresd_" + index_str, spot.midthresd, keyFile);
        saveToKeyfile(!pedited || spot_edited->midthresdch, "Locallab", "Midthresdch_" + index_str, spot.midthresdch, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthresd, "Locallab", "Higthresd_" + index_str, spot.higthresd, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayd, "Locallab", "Decayd_" + index_str, spot.decayd, keyFile);
        saveToKeyfile(!pedited || spot_edited->isogr, "Locallab", "Isogr_" + index_str, spot.isogr, keyFile);
        saveToKeyfile(!pedited || spot_edited->strengr, "Locallab", "Strengr_" + index_str, spot.strengr, keyFile);
        saveToKeyfile(!pedited || spot_edited->scalegr, "Locallab", "Scalegr_" + index_str, spot.scalegr, keyFile);
        saveToKeyfile(!pedited || spot_edited->divgr, "Locallab", "Divgr_" + index_str, spot.divgr, keyFile);
        saveToKeyfile(!pedited || spot_edited->epsbl, "Locallab", "Epsbl_" + index_str, spot.epsbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->blMethod, "Locallab", "BlMethod_" + index_str, spot.blMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->chroMethod, "Locallab", "ChroMethod_" + index_str, spot.chroMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->quamethod, "Locallab", "QuaMethod_" + index_str, spot.quamethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurMethod, "Locallab", "BlurMethod_" + index_str, spot.blurMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->usemask, "Locallab", "Usemaskb_" + index_str, spot.usemask, keyFile);
        saveToKeyfile(!pedited || spot_edited->invmaskd, "Locallab", "Invmaskd_" + index_str, spot.invmaskd, keyFile);
        saveToKeyfile(!pedited || spot_edited->invmask, "Locallab", "Invmask_" + index_str, spot.invmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->levelthr, "Locallab", "Levelthr_" + index_str, spot.levelthr, keyFile);
        saveToKeyfile(!pedited || spot_edited->lnoiselow, "Locallab", "Lnoiselow_" + index_str, spot.lnoiselow, keyFile);
        saveToKeyfile(!pedited || spot_edited->levelthrlow, "Locallab", "Levelthrlow_" + index_str, spot.levelthrlow, keyFile);
        saveToKeyfile(!pedited || spot_edited->medMethod, "Locallab", "MedMethod_" + index_str, spot.medMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->activlum, "Locallab", "activlum_" + index_str, spot.activlum, keyFile);
        for (int j = 0; j < 21; j++) {
            saveToKeyfile(!pedited || spot_edited->madlsav[j], "Locallab", "Madlsav" + std::to_string(j) + "_" + index_str, spot.madlsav[j], keyFile);
        }
        saveToKeyfile(!pedited || spot_edited->noiselumf, "Locallab", "noiselumf_" + index_str, spot.noiselumf, keyFile);
        saveToKeyfile(!pedited || spot_edited->noiselumf0, "Locallab", "noiselumf0_" + index_str, spot.noiselumf0, keyFile);
        saveToKeyfile(!pedited || spot_edited->noiselumf2, "Locallab", "noiselumf2_" + index_str, spot.noiselumf2, keyFile);
        saveToKeyfile(!pedited || spot_edited->noiselumc, "Locallab", "noiselumc_" + index_str, spot.noiselumc, keyFile);
        saveToKeyfile(!pedited || spot_edited->noiselumdetail, "Locallab", "noiselumdetail_" + index_str, spot.noiselumdetail, keyFile);
        saveToKeyfile(!pedited || spot_edited->noiselequal, "Locallab", "noiselequal_" + index_str, spot.noiselequal, keyFile);
        saveToKeyfile(!pedited || spot_edited->noisegam, "Locallab", "noisegam_" + index_str, spot.noisegam, keyFile);
        saveToKeyfile(!pedited || spot_edited->noisechrof, "Locallab", "noisechrof_" + index_str, spot.noisechrof, keyFile);
        saveToKeyfile(!pedited || spot_edited->noisechroc, "Locallab", "noisechroc_" + index_str, spot.noisechroc, keyFile);
        saveToKeyfile(!pedited || spot_edited->noisechrodetail, "Locallab", "noisechrodetail_" + index_str, spot.noisechrodetail, keyFile);
        saveToKeyfile(!pedited || spot_edited->adjblur, "Locallab", "Adjblur_" + index_str, spot.adjblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->bilateral, "Locallab", "Bilateral_" + index_str, spot.bilateral, keyFile);
        saveToKeyfile(!pedited || spot_edited->nlstr, "Locallab", "Nlstr_" + index_str, spot.nlstr, keyFile);
        saveToKeyfile(!pedited || spot_edited->nldet, "Locallab", "Nldet_" + index_str, spot.nldet, keyFile);
        saveToKeyfile(!pedited || spot_edited->nlpat, "Locallab", "Nlpat_" + index_str, spot.nlpat, keyFile);
        saveToKeyfile(!pedited || spot_edited->nlrad, "Locallab", "Nlrad_" + index_str, spot.nlrad, keyFile);
        saveToKeyfile(!pedited || spot_edited->nlgam, "Locallab", "Nlgam_" + index_str, spot.nlgam, keyFile);
        saveToKeyfile(!pedited || spot_edited->nliter, "Locallab", "Nliter_" + index_str, spot.nliter, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensiden, "Locallab", "Sensiden_" + index_str, spot.sensiden, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparden, "Locallab", "Reparden_" + index_str, spot.reparden, keyFile);
        saveToKeyfile(!pedited || spot_edited->detailthr, "Locallab", "Detailthr_" + index_str, spot.detailthr, keyFile);
        saveToKeyfile(!pedited || spot_edited->locwavcurveden, "Locallab", "LocwavCurveden_" + index_str, spot.locwavcurveden, keyFile);
        saveToKeyfile(!pedited || spot_edited->locwavcurvehue, "Locallab", "LocwavCurvehue_" + index_str, spot.locwavcurvehue, keyFile);
        saveToKeyfile(!pedited || spot_edited->locwavcurvehuecont, "Locallab", "LocwavCurvehuecont_" + index_str, spot.locwavcurvehuecont, keyFile);
        saveToKeyfile(!pedited || spot_edited->showmaskblMethodtyp, "Locallab", "Showmasktyp_" + index_str, spot.showmaskblMethodtyp, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskblcurve, "Locallab", "CCmaskblCurve_" + index_str, spot.CCmaskblcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskblcurve, "Locallab", "LLmaskblCurve_" + index_str, spot.LLmaskblcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskblcurve, "Locallab", "HHmaskblCurve_" + index_str, spot.HHmaskblcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->enablMask, "Locallab", "EnablMask_" + index_str, spot.enablMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->fftwbl, "Locallab", "Fftwbl_" + index_str, spot.fftwbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->invbl, "Locallab", "Invbl_" + index_str, spot.invbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->toolbl, "Locallab", "Toolbl_" + index_str, spot.toolbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskbl, "Locallab", "Blendmaskbl_" + index_str, spot.blendmaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskbl, "Locallab", "Radmaskbl_" + index_str, spot.radmaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskbl, "Locallab", "Chromaskbl_" + index_str, spot.chromaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskbl, "Locallab", "Gammaskbl_" + index_str, spot.gammaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskbl, "Locallab", "Slomaskbl_" + index_str, spot.slomaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskbl, "Locallab", "Lapmaskbl_" + index_str, spot.lapmaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->shadmaskbl, "Locallab", "shadmaskbl_" + index_str, spot.shadmaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->shadmaskblsha, "Locallab", "shadmaskblsha_" + index_str, spot.shadmaskblsha, keyFile);
        saveToKeyfile(!pedited || spot_edited->strumaskbl, "Locallab", "strumaskbl_" + index_str, spot.strumaskbl, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskblcurve, "Locallab", "LmaskblCurve_" + index_str, spot.Lmaskblcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskblcurvewav, "Locallab", "LLmaskblCurvewav_" + index_str, spot.LLmaskblcurvewav, keyFile);
        saveToKeyfile(!pedited || spot_edited->csthresholdblur, "Locallab", "CSThresholdblur_" + index_str, spot.csthresholdblur.toVector(), keyFile);
        saveToKeyfile(!pedited || spot_edited->denocontrast, "Locallab", "denocontrast_" + index_str, spot.denocontrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->denoAutocontrast, "Locallab", "denoAutocontrast_" + index_str, spot.denoAutocontrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->contrshow, "Locallab", "contrshow_" + index_str, spot.contrshow, keyFile);
        saveToKeyfile(!pedited || spot_edited->lockmadl, "Locallab", "lockmadl_" + index_str, spot.lockmadl, keyFile);
        saveToKeyfile(!pedited || spot_edited->madllock, "Locallab", "madllock_" + index_str, spot.madllock, keyFile);
        saveToKeyfile(!pedited || spot_edited->enacontrast, "Locallab", "enacontrast_" + index_str, spot.enacontrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->denoratio, "Locallab", "denoratio_" + index_str, spot.denoratio, keyFile);
        saveToKeyfile(!pedited || spot_edited->denomask, "Locallab", "denomask_" + index_str, spot.denomask, keyFile);
    }
}

void SaveUtil::toneMapping()
{
    if ((!pedited || spot_edited->visitonemap) && spot.visitonemap) {
        saveToKeyfile(!pedited || spot_edited->exptonemap, "Locallab", "Exptonemap_" + index_str, spot.exptonemap, keyFile);
        saveToKeyfile(!pedited || spot_edited->complextonemap, "Locallab", "Complextonemap_" + index_str, spot.complextonemap, keyFile);
        saveToKeyfile(!pedited || spot_edited->stren, "Locallab", "Stren_" + index_str, spot.stren, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamma, "Locallab", "Gamma_" + index_str, spot.gamma, keyFile);
        saveToKeyfile(!pedited || spot_edited->estop, "Locallab", "Estop_" + index_str, spot.estop, keyFile);
        saveToKeyfile(!pedited || spot_edited->scaltm, "Locallab", "Scaltm_" + index_str, spot.scaltm, keyFile);
        saveToKeyfile(!pedited || spot_edited->repartm, "Locallab", "Repartm_" + index_str, spot.repartm, keyFile);
        saveToKeyfile(!pedited || spot_edited->rewei, "Locallab", "Rewei_" + index_str, spot.rewei, keyFile);
        saveToKeyfile(!pedited || spot_edited->satur, "Locallab", "Satur_" + index_str, spot.satur, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensitm, "Locallab", "Sensitm_" + index_str, spot.sensitm, keyFile);
        saveToKeyfile(!pedited || spot_edited->softradiustm, "Locallab", "Softradiustm_" + index_str, spot.softradiustm, keyFile);
        saveToKeyfile(!pedited || spot_edited->amount, "Locallab", "Amount_" + index_str, spot.amount, keyFile);
        saveToKeyfile(!pedited || spot_edited->equiltm, "Locallab", "Equiltm_" + index_str, spot.equiltm, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmasktmcurve, "Locallab", "CCmasktmCurve_" + index_str, spot.CCmasktmcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmasktmcurve, "Locallab", "LLmasktmCurve_" + index_str, spot.LLmasktmcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmasktmcurve, "Locallab", "HHmasktmCurve_" + index_str, spot.HHmasktmcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->enatmMask, "Locallab", "EnatmMask_" + index_str, spot.enatmMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->enatmMaskaft, "Locallab", "EnatmMaskaft_" + index_str, spot.enatmMaskaft, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmasktm, "Locallab", "Blendmasktm_" + index_str, spot.blendmasktm, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmasktm, "Locallab", "Radmasktm_" + index_str, spot.radmasktm, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromasktm, "Locallab", "Chromasktm_" + index_str, spot.chromasktm, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammasktm, "Locallab", "Gammasktm_" + index_str, spot.gammasktm, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomasktm, "Locallab", "Slomasktm_" + index_str, spot.slomasktm, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmasktm, "Locallab", "Lapmasktm_" + index_str, spot.lapmasktm, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmasktmcurve, "Locallab", "LmasktmCurve_" + index_str, spot.Lmasktmcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothrest, "Locallab", "Recothrest_" + index_str, spot.recothrest, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthrest, "Locallab", "Lowthrest_" + index_str, spot.lowthrest, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthrest, "Locallab", "Higthrest_" + index_str, spot.higthrest, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayt, "Locallab", "Decayt_" + index_str, spot.decayt, keyFile);
    }
}

void SaveUtil::retinex()
{
    if ((!pedited || spot_edited->visireti) && spot.visireti) {
        saveToKeyfile(!pedited || spot_edited->expreti, "Locallab", "Expreti_" + index_str, spot.expreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexreti, "Locallab", "Complexreti_" + index_str, spot.complexreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->retinexMethod, "Locallab", "retinexMethod_" + index_str, spot.retinexMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->str, "Locallab", "Str_" + index_str, spot.str, keyFile);
        saveToKeyfile(!pedited || spot_edited->chrrt, "Locallab", "Chrrt_" + index_str, spot.chrrt, keyFile);
        saveToKeyfile(!pedited || spot_edited->neigh, "Locallab", "Neigh_" + index_str, spot.neigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->vart, "Locallab", "Vart_" + index_str, spot.vart, keyFile);
        saveToKeyfile(!pedited || spot_edited->offs, "Locallab", "Offs_" + index_str, spot.offs, keyFile);
        saveToKeyfile(!pedited || spot_edited->dehaz, "Locallab", "Dehaz_" + index_str, spot.dehaz, keyFile);
        saveToKeyfile(!pedited || spot_edited->depth, "Locallab", "Depth_" + index_str, spot.depth, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensih, "Locallab", "Sensih_" + index_str, spot.sensih, keyFile);
        saveToKeyfile(!pedited || spot_edited->localTgaincurve, "Locallab", "TgainCurve_" + index_str, spot.localTgaincurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->localTtranscurve, "Locallab", "TtransCurve_" + index_str, spot.localTtranscurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->inversret, "Locallab", "Inversret_" + index_str, spot.inversret, keyFile);
        saveToKeyfile(!pedited || spot_edited->equilret, "Locallab", "Equilret_" + index_str, spot.equilret, keyFile);
        saveToKeyfile(!pedited || spot_edited->loglin, "Locallab", "Loglin_" + index_str, spot.loglin, keyFile);
        saveToKeyfile(!pedited || spot_edited->dehazeSaturation, "Locallab", "dehazeSaturation_" + index_str, spot.dehazeSaturation, keyFile);
        saveToKeyfile(!pedited || spot_edited->dehazeblack, "Locallab", "dehazeblack_" + index_str, spot.dehazeblack, keyFile);
        saveToKeyfile(!pedited || spot_edited->softradiusret, "Locallab", "Softradiusret_" + index_str, spot.softradiusret, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskreticurve, "Locallab", "CCmaskretiCurve_" + index_str, spot.CCmaskreticurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskreticurve, "Locallab", "LLmaskretiCurve_" + index_str, spot.LLmaskreticurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskreticurve, "Locallab", "HHmaskretiCurve_" + index_str, spot.HHmaskreticurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaretiMask, "Locallab", "EnaretiMask_" + index_str, spot.enaretiMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaretiMasktmap, "Locallab", "EnaretiMasktmap_" + index_str, spot.enaretiMasktmap, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskreti, "Locallab", "Blendmaskreti_" + index_str, spot.blendmaskreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskreti, "Locallab", "Radmaskreti_" + index_str, spot.radmaskreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskreti, "Locallab", "Chromaskreti_" + index_str, spot.chromaskreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskreti, "Locallab", "Gammaskreti_" + index_str, spot.gammaskreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskreti, "Locallab", "Slomaskreti_" + index_str, spot.slomaskreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskreti, "Locallab", "Lapmaskreti_" + index_str, spot.lapmaskreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->scalereti, "Locallab", "Scalereti_" + index_str, spot.scalereti, keyFile);
        saveToKeyfile(!pedited || spot_edited->darkness, "Locallab", "Darkness_" + index_str, spot.darkness, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightnessreti, "Locallab", "Lightnessreti_" + index_str, spot.lightnessreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->limd, "Locallab", "Limd_" + index_str, spot.limd, keyFile);
        saveToKeyfile(!pedited || spot_edited->cliptm, "Locallab", "Cliptm_" + index_str, spot.cliptm, keyFile);
        saveToKeyfile(!pedited || spot_edited->fftwreti, "Locallab", "Fftwreti_" + index_str, spot.fftwreti, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskreticurve, "Locallab", "LmaskretiCurve_" + index_str, spot.Lmaskreticurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothresr, "Locallab", "Recothresr_" + index_str, spot.recothresr, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthresr, "Locallab", "Lowthresr_" + index_str, spot.lowthresr, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthresr, "Locallab", "Higthresr_" + index_str, spot.higthresr, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayr, "Locallab", "Decayr_" + index_str, spot.decayr, keyFile);
    }
}

void SaveUtil::sharpening()
{
    if ((!pedited || spot_edited->visisharp) && spot.visisharp) {
        saveToKeyfile(!pedited || spot_edited->expsharp, "Locallab", "Expsharp_" + index_str, spot.expsharp, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexsharp, "Locallab", "Complexsharp_" + index_str, spot.complexsharp, keyFile);
        saveToKeyfile(!pedited || spot_edited->sharcontrast, "Locallab", "Sharcontrast_" + index_str, spot.sharcontrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->sharradius, "Locallab", "Sharradius_" + index_str, spot.sharradius, keyFile);
        saveToKeyfile(!pedited || spot_edited->sharamount, "Locallab", "Sharamount_" + index_str, spot.sharamount, keyFile);
        saveToKeyfile(!pedited || spot_edited->shardamping, "Locallab", "Shardamping_" + index_str, spot.shardamping, keyFile);
        saveToKeyfile(!pedited || spot_edited->shariter, "Locallab", "Shariter_" + index_str, spot.shariter, keyFile);
        saveToKeyfile(!pedited || spot_edited->sharblur, "Locallab", "Sharblur_" + index_str, spot.sharblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->shargam, "Locallab", "Shargam_" + index_str, spot.shargam, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensisha, "Locallab", "Sensisha_" + index_str, spot.sensisha, keyFile);
        saveToKeyfile(!pedited || spot_edited->inverssha, "Locallab", "Inverssha_" + index_str, spot.inverssha, keyFile);
        saveToKeyfile(!pedited || spot_edited->sharshow, "Locallab", "sharshow_" + index_str, spot.sharshow, keyFile);
        saveToKeyfile(!pedited || spot_edited->itercheck, "Locallab", "itercheck_" + index_str, spot.itercheck, keyFile);
        saveToKeyfile(!pedited || spot_edited->methodcap, "Locallab", "methodcap_" + index_str, spot.methodcap, keyFile);
        saveToKeyfile(!pedited || spot_edited->capradius, "Locallab", "capradius_" + index_str, spot.capradius, keyFile);
        saveToKeyfile(!pedited || spot_edited->deconvAutoRadius, "Locallab", "deconvAutoRadius_" + index_str, spot.deconvAutoRadius, keyFile);
        saveToKeyfile(!pedited || spot_edited->deconvAutoshar, "Locallab", "deconvAutoshar_" + index_str, spot.deconvAutoshar, keyFile);
        saveToKeyfile(!pedited || spot_edited->deconvCoBoost, "Locallab", "deconvCoBoost_" + index_str, spot.deconvCoBoost, keyFile);
        saveToKeyfile(!pedited || spot_edited->deconvCoProt, "Locallab", "deconvCoProt_" + index_str, spot.deconvCoProt, keyFile);
        saveToKeyfile(!pedited || spot_edited->deconvCoLat, "Locallab", "deconvCoLat_" + index_str, spot.deconvCoLat, keyFile);
        saveToKeyfile(!pedited || spot_edited->deconvCogam, "Locallab", "deconvCogam_" + index_str, spot.deconvCogam, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparsha, "Locallab", "reparsha_" + index_str, spot.reparsha, keyFile);
    }
}

void SaveUtil::localContrast()
{
    if ((!pedited || spot_edited->visicontrast) && spot.visicontrast) {
        saveToKeyfile(!pedited || spot_edited->expcontrast, "Locallab", "Expcontrast_" + index_str, spot.expcontrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexcontrast, "Locallab", "Complexcontrast_" + index_str, spot.complexcontrast, keyFile);
        saveToKeyfile(!pedited || spot_edited->lcradius, "Locallab", "Lcradius_" + index_str, spot.lcradius, keyFile);
        saveToKeyfile(!pedited || spot_edited->lcamount, "Locallab", "Lcamount_" + index_str, spot.lcamount, keyFile);
        saveToKeyfile(!pedited || spot_edited->lcdarkness, "Locallab", "Lcdarkness_" + index_str, spot.lcdarkness, keyFile);
        saveToKeyfile(!pedited || spot_edited->lclightness, "Locallab", "Lclightness_" + index_str, spot.lclightness, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmalc, "Locallab", "Sigmalc_" + index_str, spot.sigmalc, keyFile);
        saveToKeyfile(!pedited || spot_edited->offslc, "Locallab", "Offslc_" + index_str, spot.offslc, keyFile);
        saveToKeyfile(!pedited || spot_edited->levelwav, "Locallab", "Levelwav_" + index_str, spot.levelwav, keyFile);
        saveToKeyfile(!pedited || spot_edited->residcont, "Locallab", "Residcont_" + index_str, spot.residcont, keyFile);
        saveToKeyfile(!pedited || spot_edited->residsha, "Locallab", "Residsha_" + index_str, spot.residsha, keyFile);
        saveToKeyfile(!pedited || spot_edited->residshathr, "Locallab", "Residshathr_" + index_str, spot.residshathr, keyFile);
        saveToKeyfile(!pedited || spot_edited->residhi, "Locallab", "Residhi_" + index_str, spot.residhi, keyFile);
        saveToKeyfile(!pedited || spot_edited->residhithr, "Locallab", "Residhithr_" + index_str, spot.residhithr, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamlc, "Locallab", "Gamlc_" + index_str, spot.gamlc, keyFile);
        saveToKeyfile(!pedited || spot_edited->residgam, "Locallab", "Residgam_" + index_str, spot.residgam, keyFile);
        saveToKeyfile(!pedited || spot_edited->residslop, "Locallab", "Residslop_" + index_str, spot.residslop, keyFile);
        saveToKeyfile(!pedited || spot_edited->residblur, "Locallab", "Residblur_" + index_str, spot.residblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->levelblur, "Locallab", "Levelblur_" + index_str, spot.levelblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmabl, "Locallab", "Sigmabl_" + index_str, spot.sigmabl, keyFile);
        saveToKeyfile(!pedited || spot_edited->residchro, "Locallab", "Residchro_" + index_str, spot.residchro, keyFile);
        saveToKeyfile(!pedited || spot_edited->residcomp, "Locallab", "Residcomp_" + index_str, spot.residcomp, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigma, "Locallab", "Sigma_" + index_str, spot.sigma, keyFile);
        saveToKeyfile(!pedited || spot_edited->offset, "Locallab", "Offset_" + index_str, spot.offset, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmadr, "Locallab", "Sigmadr_" + index_str, spot.sigmadr, keyFile);
        saveToKeyfile(!pedited || spot_edited->threswav, "Locallab", "Threswav_" + index_str, spot.threswav, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromalev, "Locallab", "Chromalev_" + index_str, spot.chromalev, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromablu, "Locallab", "Chromablu_" + index_str, spot.chromablu, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmadc, "Locallab", "sigmadc_" + index_str, spot.sigmadc, keyFile);
        saveToKeyfile(!pedited || spot_edited->deltad, "Locallab", "deltad_" + index_str, spot.deltad, keyFile);
        saveToKeyfile(!pedited || spot_edited->fatres, "Locallab", "Fatres_" + index_str, spot.fatres, keyFile);
        saveToKeyfile(!pedited || spot_edited->clarilres, "Locallab", "ClariLres_" + index_str, spot.clarilres, keyFile);
        saveToKeyfile(!pedited || spot_edited->claricres, "Locallab", "ClariCres_" + index_str, spot.claricres, keyFile);
        saveToKeyfile(!pedited || spot_edited->clarisoft, "Locallab", "Clarisoft_" + index_str, spot.clarisoft, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmalc2, "Locallab", "Sigmalc2_" + index_str, spot.sigmalc2, keyFile);
        saveToKeyfile(!pedited || spot_edited->strwav, "Locallab", "Strwav_" + index_str, spot.strwav, keyFile);
        saveToKeyfile(!pedited || spot_edited->angwav, "Locallab", "Angwav_" + index_str, spot.angwav, keyFile);
        saveToKeyfile(!pedited || spot_edited->featherwav, "Locallab", "Featherwav_" + index_str, spot.featherwav, keyFile);
        saveToKeyfile(!pedited || spot_edited->strengthw, "Locallab", "Strengthw_" + index_str, spot.strengthw, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmaed, "Locallab", "Sigmaed_" + index_str, spot.sigmaed, keyFile);
        saveToKeyfile(!pedited || spot_edited->radiusw, "Locallab", "Radiusw_" + index_str, spot.radiusw, keyFile);
        saveToKeyfile(!pedited || spot_edited->detailw, "Locallab", "Detailw_" + index_str, spot.detailw, keyFile);
        saveToKeyfile(!pedited || spot_edited->gradw, "Locallab", "Gradw_" + index_str, spot.gradw, keyFile);
        saveToKeyfile(!pedited || spot_edited->tloww, "Locallab", "Tloww_" + index_str, spot.tloww, keyFile);
        saveToKeyfile(!pedited || spot_edited->thigw, "Locallab", "Thigw_" + index_str, spot.thigw, keyFile);
        saveToKeyfile(!pedited || spot_edited->edgw, "Locallab", "Edgw_" + index_str, spot.edgw, keyFile);
        saveToKeyfile(!pedited || spot_edited->basew, "Locallab", "Basew_" + index_str, spot.basew, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensilc, "Locallab", "Sensilc_" + index_str, spot.sensilc, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparw, "Locallab", "Reparw_" + index_str, spot.reparw, keyFile);
        saveToKeyfile(!pedited || spot_edited->fftwlc, "Locallab", "Fftwlc_" + index_str, spot.fftwlc, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurlc, "Locallab", "Blurlc_" + index_str, spot.blurlc, keyFile);
        saveToKeyfile(!pedited || spot_edited->wavblur, "Locallab", "Wavblur_" + index_str, spot.wavblur, keyFile);
        saveToKeyfile(!pedited || spot_edited->wavedg, "Locallab", "Wavedg_" + index_str, spot.wavedg, keyFile);
        saveToKeyfile(!pedited || spot_edited->waveshow, "Locallab", "Waveshow_" + index_str, spot.waveshow, keyFile);
        saveToKeyfile(!pedited || spot_edited->wavcont, "Locallab", "Wavcont_" + index_str, spot.wavcont, keyFile);
        saveToKeyfile(!pedited || spot_edited->wavcomp, "Locallab", "Wavcomp_" + index_str, spot.wavcomp, keyFile);
        saveToKeyfile(!pedited || spot_edited->wavgradl, "Locallab", "Wavgradl_" + index_str, spot.wavgradl, keyFile);
        saveToKeyfile(!pedited || spot_edited->wavcompre, "Locallab", "Wavcompre_" + index_str, spot.wavcompre, keyFile);
        saveToKeyfile(!pedited || spot_edited->origlc, "Locallab", "Origlc_" + index_str, spot.origlc, keyFile);
        saveToKeyfile(!pedited || spot_edited->processwav, "Locallab", "processwav_" + index_str, spot.processwav, keyFile);
        saveToKeyfile(!pedited || spot_edited->localcontMethod, "Locallab", "localcontMethod_" + index_str, spot.localcontMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->localedgMethod, "Locallab", "localedgMethod_" + index_str, spot.localedgMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->localneiMethod, "Locallab", "localneiMethod_" + index_str, spot.localneiMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->locwavcurve, "Locallab", "LocwavCurve_" + index_str, spot.locwavcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->csthreshold, "Locallab", "CSThreshold_" + index_str, spot.csthreshold.toVector(), keyFile);
        saveToKeyfile(!pedited || spot_edited->loclevwavcurve, "Locallab", "LoclevwavCurve_" + index_str, spot.loclevwavcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->locconwavcurve, "Locallab", "LocconwavCurve_" + index_str, spot.locconwavcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->loccompwavcurve, "Locallab", "LoccompwavCurve_" + index_str, spot.loccompwavcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->loccomprewavcurve, "Locallab", "LoccomprewavCurve_" + index_str, spot.loccomprewavcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->locedgwavcurve, "Locallab", "LocedgwavCurve_" + index_str, spot.locedgwavcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmasklccurve, "Locallab", "CCmasklcCurve_" + index_str, spot.CCmasklccurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmasklccurve, "Locallab", "LLmasklcCurve_" + index_str, spot.LLmasklccurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmasklccurve, "Locallab", "HHmasklcCurve_" + index_str, spot.HHmasklccurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->enalcMask, "Locallab", "EnalcMask_" + index_str, spot.enalcMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmasklc, "Locallab", "Blendmasklc_" + index_str, spot.blendmasklc, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmasklc, "Locallab", "Radmasklc_" + index_str, spot.radmasklc, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromasklc, "Locallab", "Chromasklc_" + index_str, spot.chromasklc, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmasklccurve, "Locallab", "LmasklcCurve_" + index_str, spot.Lmasklccurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothresw, "Locallab", "Recothresw_" + index_str, spot.recothresw, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthresw, "Locallab", "Lowthresw_" + index_str, spot.lowthresw, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthresw, "Locallab", "Higthresw_" + index_str, spot.higthresw, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayw, "Locallab", "Decayw_" + index_str, spot.decayw, keyFile);
    }
}

void SaveUtil::contrastByDetailLevels()
{
    if ((!pedited || spot_edited->visicbdl) && spot.visicbdl) {
        saveToKeyfile(!pedited || spot_edited->expcbdl, "Locallab", "Expcbdl_" + index_str, spot.expcbdl, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexcbdl, "Locallab", "Complexcbdl_" + index_str, spot.complexcbdl, keyFile);

        for (int j = 0; j < 6; j++) {
            saveToKeyfile(!pedited || spot_edited->mult[j], "Locallab", "Mult" + std::to_string(j) + "_" + index_str, spot.mult[j], keyFile);
        }

        saveToKeyfile(!pedited || spot_edited->chromacbdl, "Locallab", "Chromacbdl_" + index_str, spot.chromacbdl, keyFile);
        saveToKeyfile(!pedited || spot_edited->threshold, "Locallab", "Threshold_" + index_str, spot.threshold, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensicb, "Locallab", "Sensicb_" + index_str, spot.sensicb, keyFile);
        saveToKeyfile(!pedited || spot_edited->clarityml, "Locallab", "Clarityml_" + index_str, spot.clarityml, keyFile);
        saveToKeyfile(!pedited || spot_edited->contresid, "Locallab", "Contresid_" + index_str, spot.contresid, keyFile);
        saveToKeyfile(!pedited || spot_edited->softradiuscb, "Locallab", "Softradiuscb_" + index_str, spot.softradiuscb, keyFile);
        saveToKeyfile(!pedited || spot_edited->enacbMask, "Locallab", "EnacbMask_" + index_str, spot.enacbMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskcbcurve, "Locallab", "CCmaskcbCurve_" + index_str, spot.CCmaskcbcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskcbcurve, "Locallab", "LLmaskcbCurve_" + index_str, spot.LLmaskcbcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskcbcurve, "Locallab", "HHmaskcbCurve_" + index_str, spot.HHmaskcbcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskcb, "Locallab", "Blendmaskcb_" + index_str, spot.blendmaskcb, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskcb, "Locallab", "Radmaskcb_" + index_str, spot.radmaskcb, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskcb, "Locallab", "Chromaskcb_" + index_str, spot.chromaskcb, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskcb, "Locallab", "Gammaskcb_" + index_str, spot.gammaskcb, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskcb, "Locallab", "Slomaskcb_" + index_str, spot.slomaskcb, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskcb, "Locallab", "Lapmaskcb_" + index_str, spot.lapmaskcb, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskcbcurve, "Locallab", "LmaskcbCurve_" + index_str, spot.Lmaskcbcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothrescb, "Locallab", "Recothrescb_" + index_str, spot.recothrescb, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthrescb, "Locallab", "Lowthrescb_" + index_str, spot.lowthrescb, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthrescb, "Locallab", "Higthrescb_" + index_str, spot.higthrescb, keyFile);
        saveToKeyfile(!pedited || spot_edited->decaycb, "Locallab", "Decaycb_" + index_str, spot.decaycb, keyFile);
    }
}

void SaveUtil::logEncoding()
{
    if ((!pedited || spot_edited->visilog) && spot.visilog) {
        saveToKeyfile(!pedited || spot_edited->explog, "Locallab", "Explog_" + index_str, spot.explog, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexlog, "Locallab", "Complexlog_" + index_str, spot.complexlog, keyFile);
        saveToKeyfile(!pedited || spot_edited->autocompute, "Locallab", "Autocompute_" + index_str, spot.autocompute, keyFile);
        saveToKeyfile(!pedited || spot_edited->sourceGray, "Locallab", "SourceGray_" + index_str, spot.sourceGray, keyFile);
        saveToKeyfile(!pedited || spot_edited->sourceabs, "Locallab", "Sourceabs_" + index_str, spot.sourceabs, keyFile);
        saveToKeyfile(!pedited || spot_edited->targabs, "Locallab", "Targabs_" + index_str, spot.targabs, keyFile);
        saveToKeyfile(!pedited || spot_edited->targetGray, "Locallab", "TargetGray_" + index_str, spot.targetGray, keyFile);
        saveToKeyfile(!pedited || spot_edited->catad, "Locallab", "Catad_" + index_str, spot.catad, keyFile);
        saveToKeyfile(!pedited || spot_edited->saturl, "Locallab", "Saturl_" + index_str, spot.saturl, keyFile);
        saveToKeyfile(!pedited || spot_edited->chroml, "Locallab", "Chroml_" + index_str, spot.chroml, keyFile);
        saveToKeyfile(!pedited || spot_edited->LcurveL, "Locallab", "LCurveL_" + index_str, spot.LcurveL, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightl, "Locallab", "Lightl_" + index_str, spot.lightl, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightq, "Locallab", "Brightq_" + index_str, spot.lightq, keyFile);
        saveToKeyfile(!pedited || spot_edited->contl, "Locallab", "Contl_" + index_str, spot.contl, keyFile);
        saveToKeyfile(!pedited || spot_edited->contthres, "Locallab", "Contthres_" + index_str, spot.contthres, keyFile);
        saveToKeyfile(!pedited || spot_edited->contq, "Locallab", "Contq_" + index_str, spot.contq, keyFile);
        saveToKeyfile(!pedited || spot_edited->colorfl, "Locallab", "Colorfl_" + index_str, spot.colorfl, keyFile);
        saveToKeyfile(!pedited || spot_edited->Autogray, "Locallab", "AutoGray_" + index_str, spot.Autogray, keyFile);
        saveToKeyfile(!pedited || spot_edited->fullimage, "Locallab", "Fullimage_" + index_str, spot.fullimage, keyFile);
        saveToKeyfile(!pedited || spot_edited->repar, "Locallab", "Repart_" + index_str, spot.repar, keyFile);
        saveToKeyfile(!pedited || spot_edited->ciecam, "Locallab", "Ciecam_" + index_str, spot.ciecam, keyFile);
        saveToKeyfile(!pedited || spot_edited->satlog, "Locallab", "Satlog_" + index_str, spot.satlog, keyFile);
        saveToKeyfile(!pedited || spot_edited->blackEv, "Locallab", "BlackEv_" + index_str, spot.blackEv, keyFile);
        saveToKeyfile(!pedited || spot_edited->whiteEv, "Locallab", "WhiteEv_" + index_str, spot.whiteEv, keyFile);
        saveToKeyfile(!pedited || spot_edited->whiteslog, "Locallab", "Whiteslog_" + index_str, spot.whiteslog, keyFile);
        saveToKeyfile(!pedited || spot_edited->blackslog, "Locallab", "Blackslog_" + index_str, spot.blackslog, keyFile);
        saveToKeyfile(!pedited || spot_edited->comprlog, "Locallab", "Comprlog_" + index_str, spot.comprlog, keyFile);
        saveToKeyfile(!pedited || spot_edited->strelog, "Locallab", "Strelog_" + index_str, spot.strelog, keyFile);
        saveToKeyfile(!pedited || spot_edited->detail, "Locallab", "Detail_" + index_str, spot.detail, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensilog, "Locallab", "Sensilog_" + index_str, spot.sensilog, keyFile);
        saveToKeyfile(!pedited || spot_edited->baselog, "Locallab", "Baselog_" + index_str, spot.baselog, keyFile);
        saveToKeyfile(!pedited || spot_edited->sursour, "Locallab", "Sursour_" + index_str, spot.sursour, keyFile);
        saveToKeyfile(!pedited || spot_edited->surround, "Locallab", "Surround_" + index_str, spot.surround, keyFile);
        saveToKeyfile(!pedited || spot_edited->strlog, "Locallab", "Strlog_" + index_str, spot.strlog, keyFile);
        saveToKeyfile(!pedited || spot_edited->anglog, "Locallab", "Anglog_" + index_str, spot.anglog, keyFile);
        saveToKeyfile(!pedited || spot_edited->featherlog, "Locallab", "Featherlog_" + index_str, spot.featherlog, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskcurveL, "Locallab", "CCmaskCurveL_" + index_str, spot.CCmaskcurveL, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskcurveL, "Locallab", "LLmaskCurveL_" + index_str, spot.LLmaskcurveL, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskcurveL, "Locallab", "HHmaskCurveL_" + index_str, spot.HHmaskcurveL, keyFile);
        saveToKeyfile(!pedited || spot_edited->enaLMask, "Locallab", "EnaLMask_" + index_str, spot.enaLMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskL, "Locallab", "blendmaskL_" + index_str, spot.blendmaskL, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskL, "Locallab", "radmaskL_" + index_str, spot.radmaskL, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskL, "Locallab", "chromaskL_" + index_str, spot.chromaskL, keyFile);
        saveToKeyfile(!pedited || spot_edited->LmaskcurveL, "Locallab", "LmaskCurveL_" + index_str, spot.LmaskcurveL, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothresl, "Locallab", "Recothresl_" + index_str, spot.recothresl, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthresl, "Locallab", "Lowthresl_" + index_str, spot.lowthresl, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthresl, "Locallab", "Higthresl_" + index_str, spot.higthresl, keyFile);
        saveToKeyfile(!pedited || spot_edited->decayl, "Locallab", "Decayl_" + index_str, spot.decayl, keyFile);
    }
}

void SaveUtil::mask()
{
    if ((!pedited || spot_edited->visimask) && spot.visimask) {
        saveToKeyfile(!pedited || spot_edited->expmask, "Locallab", "Expmask_" + index_str, spot.expmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexmask, "Locallab", "Complexmask_" + index_str, spot.complexmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensimask, "Locallab", "Sensimask_" + index_str, spot.sensimask, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmask, "Locallab", "Blendmaskmask_" + index_str, spot.blendmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskab, "Locallab", "Blendmaskmaskab_" + index_str, spot.blendmaskab, keyFile);
        saveToKeyfile(!pedited || spot_edited->softradiusmask, "Locallab", "Softradiusmask_" + index_str, spot.softradiusmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->enamask, "Locallab", "Enamask_" + index_str, spot.enamask, keyFile);
        saveToKeyfile(!pedited || spot_edited->fftmask, "Locallab", "Fftmask_" + index_str, spot.fftmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurmask, "Locallab", "Blurmask_" + index_str, spot.blurmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->contmask, "Locallab", "Contmask_" + index_str, spot.contmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmask_curve, "Locallab", "CCmask_Curve_" + index_str, spot.CCmask_curve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmask_curve, "Locallab", "LLmask_Curve_" + index_str, spot.LLmask_curve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmask_curve, "Locallab", "HHmask_Curve_" + index_str, spot.HHmask_curve, keyFile);
        saveToKeyfile(!pedited || spot_edited->strumaskmask, "Locallab", "Strumaskmask_" + index_str, spot.strumaskmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->toolmask, "Locallab", "Toolmask_" + index_str, spot.toolmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmask, "Locallab", "Radmask_" + index_str, spot.radmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmask, "Locallab", "Lapmask_" + index_str, spot.lapmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromask, "Locallab", "Chromask_" + index_str, spot.chromask, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammask, "Locallab", "Gammask_" + index_str, spot.gammask, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopmask, "Locallab", "Slopmask_" + index_str, spot.slopmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->shadmask, "Locallab", "Shadmask_" + index_str, spot.shadmask, keyFile);
        saveToKeyfile(!pedited || spot_edited->str_mask, "Locallab", "Str_mask_" + index_str, spot.str_mask, keyFile);
        saveToKeyfile(!pedited || spot_edited->ang_mask, "Locallab", "Ang_mask_" + index_str, spot.ang_mask, keyFile);
        saveToKeyfile(!pedited || spot_edited->feather_mask, "Locallab", "Feather_mask_" + index_str, spot.feather_mask, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHhmask_curve, "Locallab", "HHhmask_Curve_" + index_str, spot.HHhmask_curve, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmask_curve, "Locallab", "Lmask_Curve_" + index_str, spot.Lmask_curve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmask_curvewav, "Locallab", "LLmask_Curvewav_" + index_str, spot.LLmask_curvewav, keyFile);
        saveToKeyfile(!pedited || spot_edited->csthresholdmask, "Locallab", "CSThresholdmask_" + index_str, spot.csthresholdmask.toVector(), keyFile);
    }
}

void SaveUtil::ciecam()
{
    if ((!pedited || spot_edited->visicie) && spot.visicie) {
        saveToKeyfile(!pedited || spot_edited->expcie, "Locallab", "Expcie_" + index_str, spot.expcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->expprecam, "Locallab", "Expprecam_" + index_str, spot.expprecam, keyFile);
        saveToKeyfile(!pedited || spot_edited->complexcie, "Locallab", "Complexcie_" + index_str, spot.complexcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->reparcie, "Locallab", "Reparcie_" + index_str, spot.reparcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sensicie, "Locallab", "Sensicie_" + index_str, spot.sensicie, keyFile);
        saveToKeyfile(!pedited || spot_edited->Autograycie, "Locallab", "AutoGraycie_" + index_str, spot.Autograycie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigybjz12, "Locallab", "sigybjz12_" + index_str, spot.sigybjz12, keyFile);
        saveToKeyfile(!pedited || spot_edited->qtoj, "Locallab", "Qtoj_" + index_str, spot.qtoj, keyFile);
        saveToKeyfile(!pedited || spot_edited->jabcie, "Locallab", "jabcie_" + index_str, spot.jabcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->comprcieauto, "Locallab", "comprcieauto_" + index_str, spot.comprcieauto, keyFile);
        saveToKeyfile(!pedited || spot_edited->normcie12, "Locallab", "normcie12_" + index_str, spot.normcie12, keyFile);
        saveToKeyfile(!pedited || spot_edited->normcie, "Locallab", "normcie_" + index_str, spot.normcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamutcie, "Locallab", "gamutcie_" + index_str, spot.gamutcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->bwcie, "Locallab", "bwcie_" + index_str, spot.bwcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigcie, "Locallab", "sigcie_" + index_str, spot.sigcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->logcie, "Locallab", "logcie_" + index_str, spot.logcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->satcie, "Locallab", "satcie_" + index_str, spot.satcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->logcieq, "Locallab", "logcieq_" + index_str, spot.logcieq, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcie, "Locallab", "smoothcie_" + index_str, spot.smoothcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcietrc, "Locallab", "smoothcietrc_" + index_str, spot.smoothcietrc, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcietrcrel, "Locallab", "smoothcietrcrel_" + index_str, spot.smoothcietrcrel, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcieyb, "Locallab", "smoothcieyb_" + index_str, spot.smoothcieyb, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcielum, "Locallab", "smoothcielum_" + index_str, spot.smoothcielum, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothciehigh, "Locallab", "smoothciehigh_" + index_str, spot.smoothciehigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcielnk, "Locallab", "smoothcielnk_" + index_str, spot.smoothcielnk, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcieinv, "Locallab", "smoothcieinv_" + index_str, spot.smoothcieinv, keyFile);
        saveToKeyfile(!pedited || spot_edited->logjz, "Locallab", "Logjz_" + index_str, spot.logjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigjz12, "Locallab", "Sigjz12_" + index_str, spot.sigjz12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigjz, "Locallab", "Sigjz_" + index_str, spot.sigjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->forcebw, "Locallab", "Forcebw_" + index_str, spot.forcebw, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigq12, "Locallab", "Sigq12_" + index_str, spot.sigq12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigq, "Locallab", "Sigq_" + index_str, spot.sigq, keyFile);
        saveToKeyfile(!pedited || spot_edited->chjzcie, "Locallab", "chjzcie_" + index_str, spot.chjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sourceGraycie, "Locallab", "SourceGraycie_" + index_str, spot.sourceGraycie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sourceabscie, "Locallab", "Sourceabscie_" + index_str, spot.sourceabscie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sursourcie, "Locallab", "Sursourcie_" + index_str, spot.sursourcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->modecie, "Locallab", "Modecie_" + index_str, spot.modecie, keyFile);
        saveToKeyfile(!pedited || spot_edited->modecam, "Locallab", "Modecam_" + index_str, spot.modecam, keyFile);
        saveToKeyfile(!pedited || spot_edited->modeQJ, "Locallab", "ModeQJ_" + index_str, spot.modeQJ, keyFile);
        saveToKeyfile(!pedited || spot_edited->bwevMethod12, "Locallab", "bwevMethod12_" + index_str, spot.bwevMethod12, keyFile);
        saveToKeyfile(!pedited || spot_edited->bwevMethod, "Locallab", "bwevMethod_" + index_str, spot.bwevMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->saturlcie, "Locallab", "Saturlcie_" + index_str, spot.saturlcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->rstprotectcie, "Locallab", "Rstprotectcie_" + index_str, spot.rstprotectcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromlcie, "Locallab", "Chromlcie_" + index_str, spot.chromlcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->huecie, "Locallab", "Huecie_" + index_str, spot.huecie, keyFile);
        saveToKeyfile(!pedited || spot_edited->toneMethodcie, "Locallab", "ToneMethodcie_" + index_str, spot.toneMethodcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->ciecurve, "Locallab", "Ciecurve_" + index_str, spot.ciecurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->toneMethodcie2, "Locallab", "ToneMethodcie2_" + index_str, spot.toneMethodcie2, keyFile);
        saveToKeyfile(!pedited || spot_edited->ciecurve2, "Locallab", "Ciecurve2_" + index_str, spot.ciecurve2, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromjzcie, "Locallab", "Chromjzcie_" + index_str, spot.chromjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->saturjzcie, "Locallab", "Saturjzcie_" + index_str, spot.saturjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->huejzcie, "Locallab", "Huejzcie_" + index_str, spot.huejzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->softjzcie, "Locallab", "Softjzcie_" + index_str, spot.softjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->strsoftjzcie, "Locallab", "strSoftjzcie_" + index_str, spot.strsoftjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->thrhjzcie, "Locallab", "Thrhjzcie_" + index_str, spot.thrhjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->jzcurve, "Locallab", "JzCurve_" + index_str, spot.jzcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->czcurve, "Locallab", "CzCurve_" + index_str, spot.czcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->czjzcurve, "Locallab", "CzJzCurve_" + index_str, spot.czjzcurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHcurvejz, "Locallab", "HHCurvejz_" + index_str, spot.HHcurvejz, keyFile);
        saveToKeyfile(!pedited || spot_edited->CHcurvejz, "Locallab", "CHCurvejz_" + index_str, spot.CHcurvejz, keyFile);
        saveToKeyfile(!pedited || spot_edited->LHcurvejz, "Locallab", "LHCurvejz_" + index_str, spot.LHcurvejz, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightlcie, "Locallab", "Lightlcie_" + index_str, spot.lightlcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightjzcie, "Locallab", "Lightjzcie_" + index_str, spot.lightjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightqcie, "Locallab", "Brightqcie_" + index_str, spot.lightqcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->lightsigqcie, "Locallab", "Brightsigqcie_" + index_str, spot.lightsigqcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->contlcie, "Locallab", "Contlcie_" + index_str, spot.contlcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->contjzcie, "Locallab", "Contjzcie_" + index_str, spot.contjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->detailciejz, "Locallab", "Detailciejz_" + index_str, spot.detailciejz, keyFile);
        saveToKeyfile(!pedited || spot_edited->adapjzcie, "Locallab", "Adapjzcie_" + index_str, spot.adapjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->jz100, "Locallab", "Jz100_" + index_str, spot.jz100, keyFile);
        saveToKeyfile(!pedited || spot_edited->pqremap, "Locallab", "PQremap_" + index_str, spot.pqremap, keyFile);
        saveToKeyfile(!pedited || spot_edited->pqremapcam16, "Locallab", "PQremapcam16_" + index_str, spot.pqremapcam16, keyFile);
        saveToKeyfile(!pedited || spot_edited->hljzcie, "Locallab", "Hljzcie_" + index_str, spot.hljzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->hlthjzcie, "Locallab", "Hlthjzcie_" + index_str, spot.hlthjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->shjzcie, "Locallab", "Shjzcie_" + index_str, spot.shjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->shthjzcie, "Locallab", "Shthjzcie_" + index_str, spot.shthjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->radjzcie, "Locallab", "Radjzcie_" + index_str, spot.radjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmalcjz, "Locallab", "Sigmalcjz_" + index_str, spot.sigmalcjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->clarilresjz, "Locallab", "Clarilresjz_" + index_str, spot.clarilresjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->claricresjz, "Locallab", "Claricresjz_" + index_str, spot.claricresjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->clarisoftjz, "Locallab", "Clarisoftjz_" + index_str, spot.clarisoftjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->locwavcurvejz, "Locallab", "LocwavCurvejz_" + index_str, spot.locwavcurvejz, keyFile);
        saveToKeyfile(!pedited || spot_edited->csthresholdjz, "Locallab", "CSThresholdjz_" + index_str, spot.csthresholdjz.toVector(), keyFile);
        saveToKeyfile(!pedited || spot_edited->contthrescie, "Locallab", "Contthrescie_" + index_str, spot.contthrescie, keyFile);
        saveToKeyfile(!pedited || spot_edited->blackEvjz, "Locallab", "BlackEvjz_" + index_str, spot.blackEvjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->whiteEvjz, "Locallab", "WhiteEvjz_" + index_str, spot.whiteEvjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->targetjz, "Locallab", "Targetjz_" + index_str, spot.targetjz, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidldacie12, "Locallab", "Sigmoidldacie12_" + index_str, spot.sigmoidldacie12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidthcie12, "Locallab", "Sigmoidthcie12_" + index_str, spot.sigmoidthcie12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidblcie12, "Locallab", "Sigmoidblcie12_" + index_str, spot.sigmoidblcie12, keyFile);

        saveToKeyfile(!pedited || spot_edited->sigmoidldacie, "Locallab", "Sigmoidldacie_" + index_str, spot.sigmoidldacie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidthcie, "Locallab", "Sigmoidthcie_" + index_str, spot.sigmoidthcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidblcie, "Locallab", "Sigmoidblcie_" + index_str, spot.sigmoidblcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidsenscie, "Locallab", "Sigmoidsenscie_" + index_str, spot.sigmoidsenscie, keyFile);

        saveToKeyfile(!pedited || spot_edited->comprcie, "Locallab", "comprcie_" + index_str, spot.comprcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->strcielog, "Locallab", "strcielog_" + index_str, spot.strcielog, keyFile);
        saveToKeyfile(!pedited || spot_edited->comprcieth, "Locallab", "comprcieth_" + index_str, spot.comprcieth, keyFile);
        saveToKeyfile(!pedited || spot_edited->gamjcie, "Locallab", "gamjcie_" + index_str, spot.gamjcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothcieth, "Locallab", "smoothcieth_" + index_str, spot.smoothcieth, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothciethtrc, "Locallab", "smoothciethtrc_" + index_str, spot.smoothciethtrc, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopjcie, "Locallab", "slopjcie_" + index_str, spot.slopjcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->satjcie, "Locallab", "satjcie_" + index_str, spot.satjcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopesmo, "Locallab", "slopesmo_" + index_str, spot.slopesmo, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopesmoq, "Locallab", "slopesmoq_" + index_str, spot.slopesmoq, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopesmor, "Locallab", "slopesmor_" + index_str, spot.slopesmor, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopesmog, "Locallab", "slopesmog_" + index_str, spot.slopesmog, keyFile);
        saveToKeyfile(!pedited || spot_edited->slopesmob, "Locallab", "slopesmob_" + index_str, spot.slopesmob, keyFile);
        saveToKeyfile(!pedited || spot_edited->contsig, "Locallab", "contsig_" + index_str, spot.contsig, keyFile);
        saveToKeyfile(!pedited || spot_edited->skewsig, "Locallab", "skewsig_" + index_str, spot.skewsig, keyFile);
        saveToKeyfile(!pedited || spot_edited->whitsig, "Locallab", "whitsig_" + index_str, spot.whitsig, keyFile);

        saveToKeyfile(!pedited || spot_edited->kslopesmor, "Locallab", "kslopesmor_" + index_str, spot.kslopesmor, keyFile);
        saveToKeyfile(!pedited || spot_edited->kslopesmog, "Locallab", "kslopesmog_" + index_str, spot.kslopesmog, keyFile);
        saveToKeyfile(!pedited || spot_edited->kslopesmob, "Locallab", "kslopesmob_" + index_str, spot.kslopesmob, keyFile);
        saveToKeyfile(!pedited || spot_edited->invcurve, "Locallab", "invcurve_" + index_str, spot.invcurve, keyFile);

        saveToKeyfile(!pedited || spot_edited->midtcie, "Locallab", "midtciemet_" + index_str, spot.midtciemet, keyFile);
        saveToKeyfile(!pedited || spot_edited->midtcie, "Locallab", "midtcie_" + index_str, spot.midtcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->redxl, "Locallab", "redxl_" + index_str, spot.redxl, keyFile);
        saveToKeyfile(!pedited || spot_edited->redyl, "Locallab", "redyl_" + index_str, spot.redyl, keyFile);
        saveToKeyfile(!pedited || spot_edited->grexl, "Locallab", "grexl_" + index_str, spot.grexl, keyFile);
        saveToKeyfile(!pedited || spot_edited->greyl, "Locallab", "greyl_" + index_str, spot.greyl, keyFile);
        saveToKeyfile(!pedited || spot_edited->bluxl, "Locallab", "bluxl_" + index_str, spot.bluxl, keyFile);
        saveToKeyfile(!pedited || spot_edited->bluyl, "Locallab", "bluyl_" + index_str, spot.bluyl, keyFile);
        saveToKeyfile(!pedited || spot_edited->refi, "Locallab", "refi_" + index_str, spot.refi, keyFile);
        saveToKeyfile(!pedited || spot_edited->shiftxl, "Locallab", "shiftxl_" + index_str, spot.shiftxl, keyFile);
        saveToKeyfile(!pedited || spot_edited->shiftyl, "Locallab", "shiftyl_" + index_str, spot.shiftyl, keyFile);

        saveToKeyfile(!pedited || spot_edited->labgridcieALow, "Locallab", "labgridcieALow_" + index_str, spot.labgridcieALow, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieBLow, "Locallab", "labgridcieBLow_" + index_str, spot.labgridcieBLow, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieAHigh, "Locallab", "labgridcieAHigh_" + index_str, spot.labgridcieAHigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieBHigh, "Locallab", "labgridcieBHigh_" + index_str, spot.labgridcieBHigh, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieGx, "Locallab", "labgridcieGx_" + index_str, spot.labgridcieGx, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieGy, "Locallab", "labgridcieGy_" + index_str, spot.labgridcieGy, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieWx, "Locallab", "labgridcieWx_" + index_str, spot.labgridcieWx, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieWy, "Locallab", "labgridcieWy_" + index_str, spot.labgridcieWy, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieMx, "Locallab", "labgridcieMx_" + index_str, spot.labgridcieMx, keyFile);
        saveToKeyfile(!pedited || spot_edited->labgridcieMy, "Locallab", "labgridcieMy_" + index_str, spot.labgridcieMy, keyFile);

        saveToKeyfile(!pedited || spot_edited->whitescie, "Locallab", "whitescie_" + index_str, spot.whitescie, keyFile);
        saveToKeyfile(!pedited || spot_edited->blackscie, "Locallab", "blackscie_" + index_str, spot.blackscie, keyFile);
        saveToKeyfile(!pedited || spot_edited->illMethod, "Locallab", "illMethod_" + index_str, spot.illMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->smoothciemet, "Locallab", "smoothciemet_" + index_str, spot.smoothciemet, keyFile);
        saveToKeyfile(!pedited || spot_edited->primMethod, "Locallab", "primMethod_" + index_str, spot.primMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->catMethod, "Locallab", "catMethod_" + index_str, spot.catMethod, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidldajzcie12, "Locallab", "Sigmoidldajzcie12_" + index_str, spot.sigmoidldajzcie12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidthjzcie12, "Locallab", "Sigmoidthjzcie12_" + index_str, spot.sigmoidthjzcie12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidbljzcie12, "Locallab", "Sigmoidbljzcie12_" + index_str, spot.sigmoidbljzcie12, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidldajzcie, "Locallab", "Sigmoidldajzcie_" + index_str, spot.sigmoidldajzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidthjzcie, "Locallab", "Sigmoidthjzcie_" + index_str, spot.sigmoidthjzcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->sigmoidbljzcie, "Locallab", "Sigmoidbljzcie_" + index_str, spot.sigmoidbljzcie, keyFile);

        saveToKeyfile(!pedited || spot_edited->contqcie, "Locallab", "Contqcie_" + index_str, spot.contqcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->contsigqcie, "Locallab", "Contsigqcie_" + index_str, spot.contsigqcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->colorflcie, "Locallab", "Colorflcie_" + index_str, spot.colorflcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->targabscie, "Locallab", "Targabscie_" + index_str, spot.targabscie, keyFile);
        saveToKeyfile(!pedited || spot_edited->targetGraycie, "Locallab", "TargetGraycie_" + index_str, spot.targetGraycie, keyFile);
        saveToKeyfile(!pedited || spot_edited->catadcie, "Locallab", "Catadcie_" + index_str, spot.catadcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->detailcie, "Locallab", "Detailcie_" + index_str, spot.detailcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->strgradcie, "Locallab", "Strgradcie_" + index_str, spot.strgradcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->anggradcie, "Locallab", "Anggradcie_" + index_str, spot.anggradcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->feathercie, "Locallab", "Feathercie_" + index_str, spot.feathercie, keyFile);
        saveToKeyfile(!pedited || spot_edited->surroundcie, "Locallab", "Surroundcie_" + index_str, spot.surroundcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->enacieMask, "Locallab", "EnacieMask_" + index_str, spot.enacieMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->enacieMaskall, "Locallab", "EnacieMaskall_" + index_str, spot.enacieMaskall, keyFile);
        saveToKeyfile(!pedited || spot_edited->CCmaskciecurve, "Locallab", "CCmaskcieCurve_" + index_str, spot.CCmaskciecurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskciecurve, "Locallab", "LLmaskcieCurve_" + index_str, spot.LLmaskciecurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHmaskciecurve, "Locallab", "HHmaskcieCurve_" + index_str, spot.HHmaskciecurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->HHhmaskciecurve, "Locallab", "HHhmaskcieCurve_" + index_str, spot.HHhmaskciecurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->blendmaskcie, "Locallab", "Blendmaskcie_" + index_str, spot.blendmaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->radmaskcie, "Locallab", "Radmaskcie_" + index_str, spot.radmaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->chromaskcie, "Locallab", "Chromaskcie_" + index_str, spot.chromaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->lapmaskcie, "Locallab", "Lapmaskcie_" + index_str, spot.lapmaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->gammaskcie, "Locallab", "Gammaskcie_" + index_str, spot.gammaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->slomaskcie, "Locallab", "Slomaskcie_" + index_str, spot.slomaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->Lmaskciecurve, "Locallab", "LmaskcieCurve_" + index_str, spot.Lmaskciecurve, keyFile);
        saveToKeyfile(!pedited || spot_edited->recothrescie, "Locallab", "Recothrescie_" + index_str, spot.recothrescie, keyFile);
        saveToKeyfile(!pedited || spot_edited->lowthrescie, "Locallab", "Lowthrescie_" + index_str, spot.lowthrescie, keyFile);
        saveToKeyfile(!pedited || spot_edited->higthrescie, "Locallab", "Higthrescie_" + index_str, spot.higthrescie, keyFile);
        saveToKeyfile(!pedited || spot_edited->decaycie, "Locallab", "Decaycie_" + index_str, spot.decaycie, keyFile);
        saveToKeyfile(!pedited || spot_edited->strumaskcie, "Locallab", "strumaskcie_" + index_str, spot.strumaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->toolcie, "Locallab", "toolcie_" + index_str, spot.toolcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->fftcieMask, "Locallab", "FftcieMask_" + index_str, spot.fftcieMask, keyFile);
        saveToKeyfile(!pedited || spot_edited->contcie, "Locallab", "contcie_" + index_str, spot.contcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurcie, "Locallab", "blurcie_" + index_str, spot.blurcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurcie, "Locallab", "highmaskcie_" + index_str, spot.highmaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->blurcie, "Locallab", "shadmaskcie_" + index_str, spot.shadmaskcie, keyFile);
        saveToKeyfile(!pedited || spot_edited->LLmaskciecurvewav, "Locallab", "LLmaskcieCurvewav_" + index_str, spot.LLmaskciecurvewav, keyFile);
        saveToKeyfile(!pedited || spot_edited->csthresholdcie, "Locallab", "CSThresholdcie_" + index_str, spot.csthresholdcie.toVector(), keyFile);
    }
}

}  // namespace procparams
}  // namespace rtengine
