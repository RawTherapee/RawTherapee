/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "advanced.h"

#include "curves.h"
#include "serdes.h"
#include "utils.h"

#include "rtgui/paramsedited.h"

#include <glibmm/keyfile.h>

#include <sstream>

namespace rtengine {
namespace procparams {

ColorAppearanceParams::ColorAppearanceParams() :
    enabled(false),
    degree(90),
    autodegree(true),
    degreeout(90),
    autodegreeout(true),
    curve{
       DCT_Linear
    },
    curvered{
       DCT_Linear
    },
    curvegreen{
       DCT_Linear
    },
    curveblue{
       DCT_Linear
    },
    curve2{
       DCT_Linear
    },
    curve3{
       DCT_Linear
    },
    curveMode(TcMode::LIGHT),
    curveMode2(TcMode::BRIGHT),
    curveMode3(CtcMode::CHROMA),
    complexmethod("normal"),
    modelmethod("16"),
    catmethod("clas"),
    surround("Average"),
    surrsrc("Average"),
    adapscen(2000.0),
    autoadapscen(true),
    ybscen(18),
    autoybscen(true),
    adaplum(16),
    badpixsl(0),
    wbmodel("RawT"),
    illum("i50"),
    algo("No"),
    contrast(0.0),
    qcontrast(0.0),
    jlight(0.0),
    qbright(0.0),
    chroma(0.0),
    schroma(0.0),
    schromared(0.0),
    schromagreen(0.0),
    schromablue(0.0),
    mchroma(0.0),
    colorh(0.0),
    colorhred(0.0),
    colorhgreen(0.0),
    colorhblue(0.0),
    brighthres(10.0),
    rstprotection(0.0),
    surrsource(false),
    gamut(false),
    datacie(false),
    tonecie(false),
    tempout(5003),
    autotempout(true),
    ybout(18),
    greenout(1.0),
    tempsc(5003),
    greensc(1.0)
{
}

bool ColorAppearanceParams::operator ==(const ColorAppearanceParams& other) const
{
    return
        enabled == other.enabled
        && degree == other.degree
        && autodegree == other.autodegree
        && degreeout == other.degreeout
        && autodegreeout == other.autodegreeout
        && curve == other.curve
        && curvered == other.curvered
        && curvegreen == other.curvegreen
        && curveblue == other.curveblue
        && curve2 == other.curve2
        && curve3 == other.curve3
        && curveMode == other.curveMode
        && curveMode2 == other.curveMode2
        && curveMode3 == other.curveMode3
        && complexmethod == other.complexmethod
        && modelmethod == other.modelmethod
        && catmethod == other.catmethod
        && surround == other.surround
        && surrsrc == other.surrsrc
        && adapscen == other.adapscen
        && autoadapscen == other.autoadapscen
        && ybscen == other.ybscen
        && autoybscen == other.autoybscen
        && adaplum == other.adaplum
        && badpixsl == other.badpixsl
        && wbmodel == other.wbmodel
        && illum == other.illum
        && algo == other.algo
        && contrast == other.contrast
        && qcontrast == other.qcontrast
        && jlight == other.jlight
        && qbright == other.qbright
        && chroma == other.chroma
        && schroma == other.schroma
        && schromared == other.schromared
        && schromagreen == other.schromagreen
        && schromablue == other.schromablue
        && mchroma == other.mchroma
        && colorh == other.colorh
        && colorhred == other.colorhred
        && colorhgreen == other.colorhgreen
        && colorhblue == other.colorhblue
        && brighthres == other.brighthres
        && rstprotection == other.rstprotection
        && surrsource == other.surrsource
        && gamut == other.gamut
        && datacie == other.datacie
        && tonecie == other.tonecie
        && tempout == other.tempout
        && autotempout == other.autotempout
        && ybout == other.ybout
        && greenout == other.greenout
        && tempsc == other.tempsc
        && greensc == other.greensc;
}

bool ColorAppearanceParams::operator !=(const ColorAppearanceParams& other) const
{
    return !(*this == other);
}

RetinexParams::RetinexParams() :
    enabled(false),
    cdcurve{
        DCT_Linear
    },
    cdHcurve{
        DCT_Linear
    },
    lhcurve{
        DCT_Linear
    },
    transmissionCurve{
        FCT_MinMaxCPoints,
        0.00,
        0.50,
        0.35,
        0.35,
        0.60,
        0.75,
        0.35,
        0.35,
        1.00,
        0.50,
        0.35,
        0.35
    },
    gaintransmissionCurve{
        FCT_MinMaxCPoints,
        0.00,
        0.1,
        0.35,
        0.00,
        0.25,
        0.25,
        0.35,
        0.35,
        0.70,
        0.25,
        0.35,
        0.35,
        1.00,
        0.1,
        0.00,
        0.00
    },
    mapcurve{
        DCT_Linear
    },
    str(20),
    scal(3),
    iter(1),
    grad(1),
    grads(1),
    gam(1.30),
    slope(3.),
    neigh(80),
    offs(0),
    highlights(0),
    htonalwidth(80),
    shadows(0),
    stonalwidth(80),
    radius(40),
    complexmethod("normal"),
    retinexMethod("high"),
    retinexcolorspace("Lab"),
    gammaretinex("none"),
    mapMethod("none"),
    viewMethod("none"),
    vart(200),
    limd(8),
    highl(4),
    skal(3),
    medianmap(false)
{
}

bool RetinexParams::operator ==(const RetinexParams& other) const
{
    return
        enabled == other.enabled
        && cdcurve == other.cdcurve
        && cdHcurve == other.cdHcurve
        && lhcurve == other.lhcurve
        && transmissionCurve == other.transmissionCurve
        && gaintransmissionCurve == other.gaintransmissionCurve
        && mapcurve == other.mapcurve
        && str == other.str
        && scal == other.scal
        && iter == other.iter
        && grad == other.grad
        && grads == other.grads
        && gam == other.gam
        && slope == other.slope
        && neigh == other.neigh
        && offs == other.offs
        && highlights == other.highlights
        && htonalwidth == other.htonalwidth
        && shadows == other.shadows
        && stonalwidth == other.stonalwidth
        && radius == other.radius
        && complexmethod == other.complexmethod
        && retinexMethod == other.retinexMethod
        && retinexcolorspace == other.retinexcolorspace
        && gammaretinex == other.gammaretinex
        && mapMethod == other.mapMethod
        && viewMethod == other.viewMethod
        && vart == other.vart
        && limd == other.limd
        && highl == other.highl
        && skal == other.skal
        && medianmap == other.medianmap;
}

bool RetinexParams::operator !=(const RetinexParams& other) const
{
    return !(*this == other);
}

void RetinexParams::getCurves(RetinextransmissionCurve &transmissionCurveLUT, RetinexgaintransmissionCurve &gaintransmissionCurveLUT) const
{
    transmissionCurveLUT.Set(this->transmissionCurve);
    gaintransmissionCurveLUT.Set(this->gaintransmissionCurve);

}

const double WaveletParams::LABGRID_CORR_MAX = 12800.f;
const double WaveletParams::LABGRID_CORR_SCALE = 3.276f;
const double WaveletParams::LABGRIDL_DIRECT_SCALE = 41950.;

WaveletParams::WaveletParams() :
    ccwcurve{
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
        0.35
    },
    wavdenoise{
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
        0.35
    },
    wavdenoiseh{
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
        0.35
    },
    blcurve{
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
        0.35
/*
        0.0,
        0.35,
        0.35,
        1.0,
        0.0,
        0.35,
        0.35
*/
    },
    opacityCurveRG{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        1.00,
        0.50,
        0.35,
        0.35
    },
    //opacityCurveSH{
    //    static_cast<double>(FCT_MinMaxCPoints),
    //    0.,
    //    1.,
    //    0.35,
    //    0.35,
    //    0.15,
    //    0.9,
    //    0.35,
    //    0.35,
    //    0.4,
    //    0.8,
    //    0.35,
    //    0.35,
    //    0.4,
    //    0.5,
    //    0.35,
    //    0.35,
    //    0.5,
    //    0.5,
    //    0.35,
    //    0.35,
    //    0.5,
    //    0.2,
    //    0.35,
    //    0.35,
    //    0.8,
    //    0.1,
    //    0.35,
    //    0.35,
    //    1.0,
    //    0.,
    //    0.35,
    //    0.35
    //},
/*
    opacityCurveSH{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        1.,
        0.35,
        0.35,
        0.4,
        0.5,
        0.35,
        0.35,
        0.5,
        0.5,
        0.35,
        0.35,
        1.,
        0.,
        0.35,
        0.35
    },
*/
    opacityCurveBY{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        1.00,
        0.50,
        0.35,
        0.35
    },
    opacityCurveW{
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
        0.00
    },
    opacityCurveWL{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        1.00,
        0.50,
        0.35,
        0.35
    },
    hhcurve{
        FCT_Linear
    },
    wavguidcurve{
        FCT_Linear
    },
    wavhuecurve{
        FCT_Linear
    },
    Chcurve{
        FCT_Linear
    },
    wavclCurve {
        DCT_Linear
    },
    enabled(false),
    median(false),
    medianlev(false),
    linkedg(false),
    cbenab(false),
    greenlow(0),
    bluelow(0),
    greenmed(0),
    bluemed(0),
    greenhigh(0),
    bluehigh(0),
    ballum(7.),
    sigm(1.0),
    levden(0.),
    thrden(0.),
    limden(0.),
    balchrom(0.),
    chromfi(0.),
    chromco(0.),
    mergeL(20.),
    mergeC(20.),
    softrad(0.),
    softradend(0.),
    strend(50.),
    detend(0),
    thrend(0),
    lipst(false),
    avoid(false),
    showmask(false),
    oldsh(true),
    tmr(false),
    strength(100),
    balance(0),
    sigmafin(1.0),
    sigmaton(1.0),
    sigmacol(1.0),
    sigmadir(1.0),
    rangeab(20.0),
    protab(0.0),
    iter(0),
    expcontrast(false),
    expchroma(false),
    c{},
    ch{},
    expedge(false),
    expbl(false),
    expresid(false),
    expfinal(false),
    exptoning(false),
    expnoise(false),
    expclari(false),
    labgridALow(0.0),
    labgridBLow(0.0),
    labgridAHigh(0.0),
    labgridBHigh(0.0),
    Lmethod(4),
    CLmethod("all"),
    Backmethod("grey"),
    Tilesmethod("full"),
    complexmethod("normal"),
    //denmethod("12low"),
    mixmethod("mix"),
    slimethod("sli"),
    quamethod("cons"),
    daubcoeffmethod("6_"),
    CHmethod("without"),
    Medgreinf("less"),
    ushamethod("clari"),
    CHSLmethod("SL"),
    EDmethod("CU"),
    NPmethod("none"),
    BAmethod("none"),
    TMmethod("cont"),
    Dirmethod("all"),
    HSmethod("with"),
    sigma(1.0),
    offset(1.0),
    lowthr(40.0),
    rescon(0),
    resconH(0),
    reschro(0),
    resblur(0),
    resblurc(0),
    tmrs(0),
    edgs(1.4),
    scale(1.),
    gamma(1),
    sup(0),
    sky(0.0),
    thres(7),
    chroma(5),
    chro(0),
    threshold(4),
    threshold2(5),
    edgedetect(90),
    edgedetectthr(20),
    edgedetectthr2(0),
    edgesensi(60),
    edgeampli(10),
    contrast(0),
    edgrad(15),
    edgeffect(1.0),
    edgval(0),
    edgthresh(10),
    thr(30),
    thrH(70),
    radius(40),
    skinprotect(0.0),
    chrwav(0.),
    bluwav(1.0),
    hueskin(-5, 25, 170, 120, false),
    hueskin2(-260, -250, -130, -140, false),
    hllev(50, 75, 100, 98, false),
    bllev(0, 2, 50, 25, false),
    pastlev(0, 2, 30, 20, false),
    satlev(30, 45, 130, 100, false),
    edgcont(0, 10, 75, 40, false),
    level0noise(0, 0, false),
    level1noise(0, 0, false),
    level2noise(0, 0, false),
    level3noise(0, 0, false),
    leveldenoise(0, 0, false),
    levelsigm(1, 1, false)
{
}

bool WaveletParams::operator ==(const WaveletParams& other) const
{
    return
        ccwcurve == other.ccwcurve
        && wavdenoise == other.wavdenoise
        && wavdenoiseh == other.wavdenoiseh
        && blcurve == other.blcurve
        && opacityCurveRG == other.opacityCurveRG
        //&& opacityCurveSH == other.opacityCurveSH
        && opacityCurveBY == other.opacityCurveBY
        && opacityCurveW == other.opacityCurveW
        && opacityCurveWL == other.opacityCurveWL
        && hhcurve == other.hhcurve
        && wavguidcurve == other.wavguidcurve
        && wavhuecurve == other.wavhuecurve
        && Chcurve == other.Chcurve
        && wavclCurve == other.wavclCurve
        && enabled == other.enabled
        && median == other.median
        && medianlev == other.medianlev
        && linkedg == other.linkedg
        && cbenab == other.cbenab
        && greenlow == other.greenlow
        && bluelow == other.bluelow
        && greenmed == other.greenmed
        && bluemed == other.bluemed
        && greenhigh == other.greenhigh
        && bluehigh == other.bluehigh
        && ballum == other.ballum
        && sigm == other.sigm
        && levden == other.levden
        && thrden == other.thrden
        && limden == other.limden
        && balchrom == other.balchrom
        && chromfi == other.chromfi
        && chromco == other.chromco
        && mergeL == other.mergeL
        && mergeC == other.mergeC
        && softrad == other.softrad
        && softradend == other.softradend
        && strend == other.strend
        && detend == other.detend
        && thrend == other.thrend
        && lipst == other.lipst
        && avoid == other.avoid
        && showmask == other.showmask
        && oldsh == other.oldsh
        && tmr == other.tmr
        && strength == other.strength
        && balance == other.balance
        && sigmafin == other.sigmafin
        && sigmaton == other.sigmaton
        && sigmacol == other.sigmacol
        && sigmadir == other.sigmadir
        && rangeab == other.rangeab
        && protab == other.protab
        && iter == other.iter
        && labgridALow == other.labgridALow
        && labgridBLow == other.labgridBLow
        && labgridAHigh == other.labgridAHigh
        && labgridBHigh == other.labgridBHigh
        && expcontrast == other.expcontrast
        && expchroma == other.expchroma
        && [this, &other]() -> bool
            {
                for (unsigned int i = 0; i < 9; ++i) {
                    if (c[i] != other.c[i] || ch[i] != other.ch[i]) {
                        return false;
                    }
                }
                return true;
            }()
        && expedge == other.expedge
        && expbl == other.expbl
        && expresid == other.expresid
        && expfinal == other.expfinal
        && expclari == other.expclari
        && exptoning == other.exptoning
        && expnoise == other.expnoise
        && Lmethod == other.Lmethod
        && CLmethod == other.CLmethod
        && Backmethod == other.Backmethod
        && Tilesmethod == other.Tilesmethod
        && complexmethod == other.complexmethod
        //&& denmethod == other.denmethod
        && mixmethod == other.mixmethod
        && slimethod == other.slimethod
        && quamethod == other.quamethod
        && daubcoeffmethod == other.daubcoeffmethod
        && CHmethod == other.CHmethod
        && Medgreinf == other.Medgreinf
        && ushamethod == other.ushamethod
        && CHSLmethod == other.CHSLmethod
        && EDmethod == other.EDmethod
        && NPmethod == other.NPmethod
        && BAmethod == other.BAmethod
        && TMmethod == other.TMmethod
        && Dirmethod == other.Dirmethod
        && HSmethod == other.HSmethod
        && sigma == other.sigma
        && offset == other.offset
        && lowthr == other.lowthr
        && rescon == other.rescon
        && resconH == other.resconH
        && reschro == other.reschro
        && resblur == other.resblur
        && resblurc == other.resblurc
        && tmrs == other.tmrs
        && edgs == other.edgs
        && scale == other.scale
        && gamma == other.gamma
        && sup == other.sup
        && sky == other.sky
        && thres == other.thres
        && chroma == other.chroma
        && chro == other.chro
        && threshold == other.threshold
        && threshold2 == other.threshold2
        && edgedetect == other.edgedetect
        && edgedetectthr == other.edgedetectthr
        && edgedetectthr2 == other.edgedetectthr2
        && edgesensi == other.edgesensi
        && edgeampli == other.edgeampli
        && contrast == other.contrast
        && edgrad == other.edgrad
        && edgeffect == other.edgeffect
        && edgval == other.edgval
        && edgthresh == other.edgthresh
        && thr == other.thr
        && thrH == other.thrH
        && radius == other.radius
        && skinprotect == other.skinprotect
        && chrwav == other.chrwav
        && bluwav == other.bluwav
        && hueskin == other.hueskin
        && hueskin2 == other.hueskin2
        && hllev == other.hllev
        && bllev == other.bllev
        && pastlev == other.pastlev
        && satlev == other.satlev
        && edgcont == other.edgcont
        && level0noise == other.level0noise
        && level1noise == other.level1noise
        && level2noise == other.level2noise
        && level3noise == other.level3noise
        && leveldenoise == other.leveldenoise
        && levelsigm == other.levelsigm;
}

bool WaveletParams::operator !=(const WaveletParams& other) const
{
    return !(*this == other);
}

void WaveletParams::getCurves(
    WavCurve& cCurve,
    WavCurve& wavdenoise,
    WavCurve& wavdenoiseh,
    Wavblcurve& tCurve,
    WavOpacityCurveRG& opacityCurveLUTRG,
    WavOpacityCurveSH& opacityCurveLUTSH,
    WavOpacityCurveBY& opacityCurveLUTBY,
    WavOpacityCurveW& opacityCurveLUTW,
    WavOpacityCurveWL& opacityCurveLUTWL
) const
{
    cCurve.Set(this->ccwcurve);
    wavdenoise.Set(this->wavdenoise);
    wavdenoiseh.Set(this->wavdenoiseh);
    tCurve.Set(this->blcurve);
    opacityCurveLUTRG.Set(this->opacityCurveRG);
    //opacityCurveLUTSH.Set(this->opacityCurveSH);
    opacityCurveLUTBY.Set(this->opacityCurveBY);
    opacityCurveLUTW.Set(this->opacityCurveW);
    opacityCurveLUTWL.Set(this->opacityCurveWL);

}

void loadColorAppearanceParams(const Glib::KeyFile& keyFile,
                               ColorAppearanceParams& colorappearance,
                               ParamsEdited* pedited,
                               int ppVersion)
{
    if (!keyFile.has_group("Color appearance")) return;

    assignFromKeyfile(keyFile, "Color appearance", "Enabled", colorappearance.enabled, pedited->colorappearance.enabled);
    assignFromKeyfile(keyFile, "Color appearance", "Degree", colorappearance.degree, pedited->colorappearance.degree);
    assignFromKeyfile(keyFile, "Color appearance", "AutoDegree", colorappearance.autodegree, pedited->colorappearance.autodegree);
    assignFromKeyfile(keyFile, "Color appearance", "Degreeout", colorappearance.degreeout, pedited->colorappearance.degreeout);

    assignFromKeyfile(keyFile, "Color appearance", "AutoDegreeout", colorappearance.autodegreeout, pedited->colorappearance.autodegreeout);

    if (keyFile.has_key("Color appearance", "complex")) {
        assignFromKeyfile(keyFile, "Color appearance", "complex", colorappearance.complexmethod, pedited->colorappearance.complexmethod);
    } else if (colorappearance.enabled) {
        colorappearance.complexmethod = "expert";
        if (pedited) {
            pedited->colorappearance.complexmethod = true;
        }
    }

    if (keyFile.has_key("Color appearance", "ModelCat")) {
        assignFromKeyfile(keyFile, "Color appearance", "ModelCat", colorappearance.modelmethod, pedited->colorappearance.modelmethod);
    } else if (colorappearance.enabled) {
        colorappearance.modelmethod = "02";
        if (pedited) {
            pedited->colorappearance.modelmethod = true;
        }
    }
    assignFromKeyfile(keyFile, "Color appearance", "CatCat", colorappearance.catmethod, pedited->colorappearance.catmethod);

    assignFromKeyfile(keyFile, "Color appearance", "Surround", colorappearance.surround, pedited->colorappearance.surround);
    assignFromKeyfile(keyFile, "Color appearance", "Surrsrc", colorappearance.surrsrc, pedited->colorappearance.surrsrc);
    assignFromKeyfile(keyFile, "Color appearance", "AdaptLum", colorappearance.adaplum, pedited->colorappearance.adaplum);
    assignFromKeyfile(keyFile, "Color appearance", "Badpixsl", colorappearance.badpixsl, pedited->colorappearance.badpixsl);
    assignFromKeyfile(keyFile, "Color appearance", "Model", colorappearance.wbmodel, pedited->colorappearance.wbmodel);
    assignFromKeyfile(keyFile, "Color appearance", "Illum", colorappearance.illum, pedited->colorappearance.illum);
    assignFromKeyfile(keyFile, "Color appearance", "Algorithm", colorappearance.algo, pedited->colorappearance.algo);
    assignFromKeyfile(keyFile, "Color appearance", "J-Light", colorappearance.jlight, pedited->colorappearance.jlight);
    assignFromKeyfile(keyFile, "Color appearance", "Q-Bright", colorappearance.qbright, pedited->colorappearance.qbright);
    assignFromKeyfile(keyFile, "Color appearance", "C-Chroma", colorappearance.chroma, pedited->colorappearance.chroma);
    assignFromKeyfile(keyFile, "Color appearance", "S-Chroma", colorappearance.schroma, pedited->colorappearance.schroma);
    assignFromKeyfile(keyFile, "Color appearance", "S-Chroma-red", colorappearance.schromared, pedited->colorappearance.schromared);
    assignFromKeyfile(keyFile, "Color appearance", "S-Chroma-green", colorappearance.schromagreen, pedited->colorappearance.schromagreen);
    assignFromKeyfile(keyFile, "Color appearance", "S-Chroma-blue", colorappearance.schromablue, pedited->colorappearance.schromablue);
    assignFromKeyfile(keyFile, "Color appearance", "M-Chroma", colorappearance.mchroma, pedited->colorappearance.mchroma);
    assignFromKeyfile(keyFile, "Color appearance", "RSTProtection", colorappearance.rstprotection, pedited->colorappearance.rstprotection);
    assignFromKeyfile(keyFile, "Color appearance", "J-Contrast", colorappearance.contrast, pedited->colorappearance.contrast);
    assignFromKeyfile(keyFile, "Color appearance", "Q-Contrast", colorappearance.qcontrast, pedited->colorappearance.qcontrast);
    assignFromKeyfile(keyFile, "Color appearance", "H-Hue", colorappearance.colorh, pedited->colorappearance.colorh);
    assignFromKeyfile(keyFile, "Color appearance", "H-Hue-red", colorappearance.colorhred, pedited->colorappearance.colorhred);
    assignFromKeyfile(keyFile, "Color appearance", "H-Hue-green", colorappearance.colorhgreen, pedited->colorappearance.colorhgreen);
    assignFromKeyfile(keyFile, "Color appearance", "H-Hue-blue", colorappearance.colorhblue, pedited->colorappearance.colorhblue);
    assignFromKeyfile(keyFile, "Color appearance", "Brighthres", colorappearance.brighthres, pedited->colorappearance.brighthres);
    assignFromKeyfile(keyFile, "Color appearance", "AdaptScene", colorappearance.adapscen, pedited->colorappearance.adapscen);
    assignFromKeyfile(keyFile, "Color appearance", "AutoAdapscen", colorappearance.autoadapscen, pedited->colorappearance.autoadapscen);
    assignFromKeyfile(keyFile, "Color appearance", "YbScene", colorappearance.ybscen, pedited->colorappearance.ybscen);
    assignFromKeyfile(keyFile, "Color appearance", "Autoybscen", colorappearance.autoybscen, pedited->colorappearance.autoybscen);
    assignFromKeyfile(keyFile, "Color appearance", "SurrSource", colorappearance.surrsource, pedited->colorappearance.surrsource);
    assignFromKeyfile(keyFile, "Color appearance", "Gamut", colorappearance.gamut, pedited->colorappearance.gamut);
    assignFromKeyfile(keyFile, "Color appearance", "Tempout", colorappearance.tempout, pedited->colorappearance.tempout);
    assignFromKeyfile(keyFile, "Color appearance", "Autotempout", colorappearance.autotempout, pedited->colorappearance.autotempout);
    assignFromKeyfile(keyFile, "Color appearance", "Greenout", colorappearance.greenout, pedited->colorappearance.greenout);
    assignFromKeyfile(keyFile, "Color appearance", "Tempsc", colorappearance.tempsc, pedited->colorappearance.tempsc);
    assignFromKeyfile(keyFile, "Color appearance", "Greensc", colorappearance.greensc, pedited->colorappearance.greensc);
    assignFromKeyfile(keyFile, "Color appearance", "Ybout", colorappearance.ybout, pedited->colorappearance.ybout);
    assignFromKeyfile(keyFile, "Color appearance", "Datacie", colorappearance.datacie, pedited->colorappearance.datacie);
    assignFromKeyfile(keyFile, "Color appearance", "Tonecie", colorappearance.tonecie, pedited->colorappearance.tonecie);

    const std::map<std::string, ColorAppearanceParams::TcMode> tc_mapping = {
        {"Lightness", ColorAppearanceParams::TcMode::LIGHT},
        {"Brightness", ColorAppearanceParams::TcMode::BRIGHT}
    };
    assignFromKeyfile(keyFile, "Color appearance", "CurveMode", tc_mapping, colorappearance.curveMode, pedited->colorappearance.curveMode);
    assignFromKeyfile(keyFile, "Color appearance", "CurveMode2", tc_mapping, colorappearance.curveMode2, pedited->colorappearance.curveMode2);

    assignFromKeyfile(
        keyFile,
        "Color appearance",
        "CurveMode3",
        {
            {"Chroma", ColorAppearanceParams::CtcMode::CHROMA},
            {"Saturation", ColorAppearanceParams::CtcMode::SATUR},
            {"Colorfullness", ColorAppearanceParams::CtcMode::COLORF}
        },
        colorappearance.curveMode3,
        pedited->colorappearance.curveMode3
    );

    if (ppVersion > 200) {
        assignFromKeyfile(keyFile, "Color appearance", "Curve", colorappearance.curve, pedited->colorappearance.curve);
        assignFromKeyfile(keyFile, "Color appearance", "Curvered", colorappearance.curvered, pedited->colorappearance.curvered);
        assignFromKeyfile(keyFile, "Color appearance", "Curvegreen", colorappearance.curvegreen, pedited->colorappearance.curvegreen);
        assignFromKeyfile(keyFile, "Color appearance", "Curveblue", colorappearance.curveblue, pedited->colorappearance.curveblue);
        assignFromKeyfile(keyFile, "Color appearance", "Curve2", colorappearance.curve2, pedited->colorappearance.curve2);
        assignFromKeyfile(keyFile, "Color appearance", "Curve3", colorappearance.curve3, pedited->colorappearance.curve3);
    }
}

void saveColorAppearanceParams(Glib::KeyFile& keyFile,
                               const ColorAppearanceParams& colorappearance,
                               const ParamsEdited* pedited)
{
    saveToKeyfile(!pedited || pedited->colorappearance.enabled, "Color appearance", "Enabled", colorappearance.enabled, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.degree, "Color appearance", "Degree", colorappearance.degree, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.autodegree, "Color appearance", "AutoDegree", colorappearance.autodegree, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.degreeout, "Color appearance", "Degreeout", colorappearance.degreeout, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.autodegreeout, "Color appearance", "AutoDegreeout", colorappearance.autodegreeout, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.surround, "Color appearance", "Surround", colorappearance.surround, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.complexmethod, "Color appearance", "complex", colorappearance.complexmethod, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.modelmethod, "Color appearance", "ModelCat", colorappearance.modelmethod, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.catmethod, "Color appearance", "CatCat", colorappearance.catmethod, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.surrsrc, "Color appearance", "Surrsrc", colorappearance.surrsrc, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.adaplum, "Color appearance", "AdaptLum", colorappearance.adaplum, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.badpixsl, "Color appearance", "Badpixsl", colorappearance.badpixsl, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.wbmodel, "Color appearance", "Model", colorappearance.wbmodel, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.illum, "Color appearance", "Illum", colorappearance.illum, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.algo, "Color appearance", "Algorithm", colorappearance.algo, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.jlight, "Color appearance", "J-Light", colorappearance.jlight, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.qbright, "Color appearance", "Q-Bright", colorappearance.qbright, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.chroma, "Color appearance", "C-Chroma", colorappearance.chroma, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.schroma, "Color appearance", "S-Chroma", colorappearance.schroma, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.schromared, "Color appearance", "S-Chroma-red", colorappearance.schromared, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.schromagreen, "Color appearance", "S-Chroma-green", colorappearance.schromagreen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.schromablue, "Color appearance", "S-Chroma-blue", colorappearance.schromablue, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.mchroma, "Color appearance", "M-Chroma", colorappearance.mchroma, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.contrast, "Color appearance", "J-Contrast", colorappearance.contrast, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.qcontrast, "Color appearance", "Q-Contrast", colorappearance.qcontrast, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.colorh, "Color appearance", "H-Hue", colorappearance.colorh, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.colorhred, "Color appearance", "H-Hue-red", colorappearance.colorhred, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.colorhgreen, "Color appearance", "H-Hue-green", colorappearance.colorhgreen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.colorhblue, "Color appearance", "H-Hue-blue", colorappearance.colorhblue, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.brighthres, "Color appearance", "Brighthres", colorappearance.brighthres, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.rstprotection, "Color appearance", "RSTProtection", colorappearance.rstprotection, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.adapscen, "Color appearance", "AdaptScene", colorappearance.adapscen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.autoadapscen, "Color appearance", "AutoAdapscen", colorappearance.autoadapscen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.ybscen, "Color appearance", "YbScene", colorappearance.ybscen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.autoybscen, "Color appearance", "Autoybscen", colorappearance.autoybscen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.surrsource, "Color appearance", "SurrSource", colorappearance.surrsource, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.gamut, "Color appearance", "Gamut", colorappearance.gamut, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.tempout, "Color appearance", "Tempout", colorappearance.tempout, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.autotempout, "Color appearance", "Autotempout", colorappearance.autotempout, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.greenout, "Color appearance", "Greenout", colorappearance.greenout, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.tempsc, "Color appearance", "Tempsc", colorappearance.tempsc, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.greensc, "Color appearance", "Greensc", colorappearance.greensc, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.ybout, "Color appearance", "Ybout", colorappearance.ybout, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.datacie, "Color appearance", "Datacie", colorappearance.datacie, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.tonecie, "Color appearance", "Tonecie", colorappearance.tonecie, keyFile);

    const std::map<ColorAppearanceParams::TcMode, const char*> ca_mapping = {
        {ColorAppearanceParams::TcMode::LIGHT, "Lightness"},
        {ColorAppearanceParams::TcMode::BRIGHT, "Brightness"}
    };

    saveToKeyfile(!pedited || pedited->colorappearance.curveMode, "Color appearance", "CurveMode", ca_mapping, colorappearance.curveMode, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.curveMode2, "Color appearance", "CurveMode2", ca_mapping, colorappearance.curveMode2, keyFile);
    saveToKeyfile(
        !pedited || pedited->colorappearance.curveMode3,
        "Color appearance",
        "CurveMode3",
        {
            {ColorAppearanceParams::CtcMode::CHROMA, "Chroma"},
            {ColorAppearanceParams::CtcMode::SATUR, "Saturation"},
            {ColorAppearanceParams::CtcMode::COLORF, "Colorfullness"}
        },
        colorappearance.curveMode3,
        keyFile
    );
    saveToKeyfile(!pedited || pedited->colorappearance.curve, "Color appearance", "Curve", colorappearance.curve, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.curvered, "Color appearance", "Curvered", colorappearance.curvered, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.curvegreen, "Color appearance", "Curvegreen", colorappearance.curvegreen, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.curveblue, "Color appearance", "Curveblue", colorappearance.curveblue, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.curve2, "Color appearance", "Curve2", colorappearance.curve2, keyFile);
    saveToKeyfile(!pedited || pedited->colorappearance.curve3, "Color appearance", "Curve3", colorappearance.curve3, keyFile);
}

void loadRetinexParams(const Glib::KeyFile& keyFile, RetinexParams& retinex,
                       ParamsEdited* pedited)
{
    if (!keyFile.has_group("Retinex")) return;

    assignFromKeyfile(keyFile, "Retinex", "Enabled", retinex.enabled, pedited->retinex.enabled);
    assignFromKeyfile(keyFile, "Retinex", "Median", retinex.medianmap, pedited->retinex.medianmap);

    if (keyFile.has_key("Retinex", "complexMethod")) {
        assignFromKeyfile(keyFile, "Retinex", "complexMethod", retinex.complexmethod, pedited->retinex.complexmethod);
    } else if (retinex.enabled) {
        retinex.complexmethod = "expert";
        if (pedited) {
            pedited->retinex.complexmethod = true;
        }
    }

    assignFromKeyfile(keyFile, "Retinex", "RetinexMethod", retinex.retinexMethod, pedited->retinex.retinexMethod);
    assignFromKeyfile(keyFile, "Retinex", "mapMethod", retinex.mapMethod, pedited->retinex.mapMethod);
    assignFromKeyfile(keyFile, "Retinex", "viewMethod", retinex.viewMethod, pedited->retinex.viewMethod);

    assignFromKeyfile(keyFile, "Retinex", "Retinexcolorspace", retinex.retinexcolorspace, pedited->retinex.retinexcolorspace);
    assignFromKeyfile(keyFile, "Retinex", "Gammaretinex", retinex.gammaretinex, pedited->retinex.gammaretinex);
    assignFromKeyfile(keyFile, "Retinex", "Neigh", retinex.neigh, pedited->retinex.neigh);
    assignFromKeyfile(keyFile, "Retinex", "Str", retinex.str, pedited->retinex.str);
    assignFromKeyfile(keyFile, "Retinex", "Scal", retinex.scal, pedited->retinex.scal);
    assignFromKeyfile(keyFile, "Retinex", "Iter", retinex.iter, pedited->retinex.iter);
    assignFromKeyfile(keyFile, "Retinex", "Grad", retinex.grad, pedited->retinex.grad);
    assignFromKeyfile(keyFile, "Retinex", "Grads", retinex.grads, pedited->retinex.grads);
    assignFromKeyfile(keyFile, "Retinex", "Gam", retinex.gam, pedited->retinex.gam);
    assignFromKeyfile(keyFile, "Retinex", "Slope", retinex.slope, pedited->retinex.slope);
    assignFromKeyfile(keyFile, "Retinex", "Offs", retinex.offs, pedited->retinex.offs);
    assignFromKeyfile(keyFile, "Retinex", "Vart", retinex.vart, pedited->retinex.vart);
    assignFromKeyfile(keyFile, "Retinex", "Limd", retinex.limd, pedited->retinex.limd);
    assignFromKeyfile(keyFile, "Retinex", "highl", retinex.highl, pedited->retinex.highl);
    assignFromKeyfile(keyFile, "Retinex", "skal", retinex.skal, pedited->retinex.skal);
    assignFromKeyfile(keyFile, "Retinex", "CDCurve", retinex.cdcurve, pedited->retinex.cdcurve);

    assignFromKeyfile(keyFile, "Retinex", "MAPCurve", retinex.mapcurve, pedited->retinex.mapcurve);

    assignFromKeyfile(keyFile, "Retinex", "CDHCurve", retinex.cdHcurve, pedited->retinex.cdHcurve);

    assignFromKeyfile(keyFile, "Retinex", "LHCurve", retinex.lhcurve, pedited->retinex.lhcurve);

    assignFromKeyfile(keyFile, "Retinex", "Highlights", retinex.highlights, pedited->retinex.highlights);
    assignFromKeyfile(keyFile, "Retinex", "HighlightTonalWidth", retinex.htonalwidth, pedited->retinex.htonalwidth);
    assignFromKeyfile(keyFile, "Retinex", "Shadows", retinex.shadows, pedited->retinex.shadows);
    assignFromKeyfile(keyFile, "Retinex", "ShadowTonalWidth", retinex.stonalwidth, pedited->retinex.stonalwidth);

    assignFromKeyfile(keyFile, "Retinex", "Radius", retinex.radius, pedited->retinex.radius);

    assignFromKeyfile(keyFile, "Retinex", "TransmissionCurve", retinex.transmissionCurve, pedited->retinex.transmissionCurve);

    assignFromKeyfile(keyFile, "Retinex", "GainTransmissionCurve", retinex.gaintransmissionCurve, pedited->retinex.gaintransmissionCurve);
}

void saveRetinexParams(Glib::KeyFile& keyFile, const RetinexParams& retinex,
                       const ParamsEdited* pedited)
{
    saveToKeyfile(!pedited || pedited->retinex.enabled, "Retinex", "Enabled", retinex.enabled, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.str, "Retinex", "Str", retinex.str, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.scal, "Retinex", "Scal", retinex.scal, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.iter, "Retinex", "Iter", retinex.iter, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.grad, "Retinex", "Grad", retinex.grad, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.grads, "Retinex", "Grads", retinex.grads, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.gam, "Retinex", "Gam", retinex.gam, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.slope, "Retinex", "Slope", retinex.slope, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.medianmap, "Retinex", "Median", retinex.medianmap, keyFile);

    saveToKeyfile(!pedited || pedited->retinex.neigh, "Retinex", "Neigh", retinex.neigh, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.offs, "Retinex", "Offs", retinex.offs, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.vart, "Retinex", "Vart", retinex.vart, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.limd, "Retinex", "Limd", retinex.limd, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.highl, "Retinex", "highl", retinex.highl, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.skal, "Retinex", "skal", retinex.skal, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.complexmethod, "Retinex", "complexMethod", retinex.complexmethod, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.retinexMethod, "Retinex", "RetinexMethod", retinex.retinexMethod, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.mapMethod, "Retinex", "mapMethod", retinex.mapMethod, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.viewMethod, "Retinex", "viewMethod", retinex.viewMethod, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.retinexcolorspace, "Retinex", "Retinexcolorspace", retinex.retinexcolorspace, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.gammaretinex, "Retinex", "Gammaretinex", retinex.gammaretinex, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.cdcurve, "Retinex", "CDCurve", retinex.cdcurve, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.mapcurve, "Retinex", "MAPCurve", retinex.mapcurve, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.cdHcurve, "Retinex", "CDHCurve", retinex.cdHcurve, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.lhcurve, "Retinex", "LHCurve", retinex.lhcurve, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.highlights, "Retinex", "Highlights", retinex.highlights, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.htonalwidth, "Retinex", "HighlightTonalWidth", retinex.htonalwidth, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.shadows, "Retinex", "Shadows", retinex.shadows, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.stonalwidth, "Retinex", "ShadowTonalWidth", retinex.stonalwidth, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.radius, "Retinex", "Radius", retinex.radius, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.transmissionCurve, "Retinex", "TransmissionCurve", retinex.transmissionCurve, keyFile);
    saveToKeyfile(!pedited || pedited->retinex.gaintransmissionCurve, "Retinex", "GainTransmissionCurve", retinex.gaintransmissionCurve, keyFile);
}

void loadWaveletParams(const Glib::KeyFile& keyFile, WaveletParams& wavelet,
                       ParamsEdited* pedited, int ppVersion)
{
    if (!keyFile.has_group("Wavelet")) return;

    assignFromKeyfile(keyFile, "Wavelet", "Enabled", wavelet.enabled, pedited->wavelet.enabled);
    assignFromKeyfile(keyFile, "Wavelet", "Strength", wavelet.strength, pedited->wavelet.strength);
    assignFromKeyfile(keyFile, "Wavelet", "Balance", wavelet.balance, pedited->wavelet.balance);
    assignFromKeyfile(keyFile, "Wavelet", "Sigmafin", wavelet.sigmafin, pedited->wavelet.sigmafin);
    assignFromKeyfile(keyFile, "Wavelet", "Sigmaton", wavelet.sigmaton, pedited->wavelet.sigmaton);
    assignFromKeyfile(keyFile, "Wavelet", "Sigmacol", wavelet.sigmacol, pedited->wavelet.sigmacol);
    assignFromKeyfile(keyFile, "Wavelet", "Sigmadir", wavelet.sigmadir, pedited->wavelet.sigmadir);
    assignFromKeyfile(keyFile, "Wavelet", "Rangeab", wavelet.rangeab, pedited->wavelet.rangeab);
    assignFromKeyfile(keyFile, "Wavelet", "Protab", wavelet.protab, pedited->wavelet.protab);
    assignFromKeyfile(keyFile, "Wavelet", "Iter", wavelet.iter, pedited->wavelet.iter);
    assignFromKeyfile(keyFile, "Wavelet", "Median", wavelet.median, pedited->wavelet.median);
    assignFromKeyfile(keyFile, "Wavelet", "Medianlev", wavelet.medianlev, pedited->wavelet.medianlev);
    assignFromKeyfile(keyFile, "Wavelet", "Linkedg", wavelet.linkedg, pedited->wavelet.linkedg);
    assignFromKeyfile(keyFile, "Wavelet", "CBenab", wavelet.cbenab, pedited->wavelet.cbenab);
    assignFromKeyfile(keyFile, "Wavelet", "CBgreenhigh", wavelet.greenhigh, pedited->wavelet.greenhigh);
    assignFromKeyfile(keyFile, "Wavelet", "CBgreenmed", wavelet.greenmed, pedited->wavelet.greenmed);
    assignFromKeyfile(keyFile, "Wavelet", "CBgreenlow", wavelet.greenlow, pedited->wavelet.greenlow);
    assignFromKeyfile(keyFile, "Wavelet", "CBbluehigh", wavelet.bluehigh, pedited->wavelet.bluehigh);
    assignFromKeyfile(keyFile, "Wavelet", "CBbluemed", wavelet.bluemed, pedited->wavelet.bluemed);
    assignFromKeyfile(keyFile, "Wavelet", "CBbluelow", wavelet.bluelow, pedited->wavelet.bluelow);
    assignFromKeyfile(keyFile, "Wavelet", "Ballum", wavelet.ballum, pedited->wavelet.ballum);
    assignFromKeyfile(keyFile, "Wavelet", "Sigm", wavelet.sigm, pedited->wavelet.sigm);
    assignFromKeyfile(keyFile, "Wavelet", "Levden", wavelet.levden, pedited->wavelet.levden);
    assignFromKeyfile(keyFile, "Wavelet", "Thrden", wavelet.thrden, pedited->wavelet.thrden);
    assignFromKeyfile(keyFile, "Wavelet", "Limden", wavelet.limden, pedited->wavelet.limden);
    assignFromKeyfile(keyFile, "Wavelet", "Balchrom", wavelet.balchrom, pedited->wavelet.balchrom);
    assignFromKeyfile(keyFile, "Wavelet", "Chromfine", wavelet.chromfi, pedited->wavelet.chromfi);
    assignFromKeyfile(keyFile, "Wavelet", "Chromcoarse", wavelet.chromco, pedited->wavelet.chromco);
    assignFromKeyfile(keyFile, "Wavelet", "MergeL", wavelet.mergeL, pedited->wavelet.mergeL);
    assignFromKeyfile(keyFile, "Wavelet", "MergeC", wavelet.mergeC, pedited->wavelet.mergeC);
    assignFromKeyfile(keyFile, "Wavelet", "Softrad", wavelet.softrad, pedited->wavelet.softrad);
    assignFromKeyfile(keyFile, "Wavelet", "Softradend", wavelet.softradend, pedited->wavelet.softradend);
    assignFromKeyfile(keyFile, "Wavelet", "Strend", wavelet.strend, pedited->wavelet.strend);
    assignFromKeyfile(keyFile, "Wavelet", "Detend", wavelet.detend, pedited->wavelet.detend);
    assignFromKeyfile(keyFile, "Wavelet", "Thrend", wavelet.thrend, pedited->wavelet.thrend);
    assignFromKeyfile(keyFile, "Wavelet", "Lipst", wavelet.lipst, pedited->wavelet.lipst);
    assignFromKeyfile(keyFile, "Wavelet", "AvoidColorShift", wavelet.avoid, pedited->wavelet.avoid);
    assignFromKeyfile(keyFile, "Wavelet", "Showmask", wavelet.showmask, pedited->wavelet.showmask);
    assignFromKeyfile(keyFile, "Wavelet", "Oldsh", wavelet.oldsh, pedited->wavelet.oldsh);
    assignFromKeyfile(keyFile, "Wavelet", "TMr", wavelet.tmr, pedited->wavelet.tmr);
    assignFromKeyfile(keyFile, "Wavelet", "LabGridALow", wavelet.labgridALow, pedited->wavelet.labgridALow);
    assignFromKeyfile(keyFile, "Wavelet", "LabGridBLow", wavelet.labgridBLow, pedited->wavelet.labgridBLow);
    assignFromKeyfile(keyFile, "Wavelet", "LabGridAHigh", wavelet.labgridAHigh, pedited->wavelet.labgridAHigh);
    assignFromKeyfile(keyFile, "Wavelet", "LabGridBHigh", wavelet.labgridBHigh, pedited->wavelet.labgridBHigh);

    if (ppVersion < 331) { // wavelet.Lmethod was a string before version 331
        Glib::ustring temp;
        assignFromKeyfile(keyFile, "Wavelet", "LevMethod", temp, pedited->wavelet.Lmethod);

        try {
            wavelet.Lmethod = std::stoi(temp);
        } catch (...) {
        }
    } else {
        assignFromKeyfile(keyFile, "Wavelet", "LevMethod", wavelet.Lmethod, pedited->wavelet.Lmethod);
    }

    assignFromKeyfile(keyFile, "Wavelet", "ChoiceLevMethod", wavelet.CLmethod, pedited->wavelet.CLmethod);
    assignFromKeyfile(keyFile, "Wavelet", "BackMethod", wavelet.Backmethod, pedited->wavelet.Backmethod);
    assignFromKeyfile(keyFile, "Wavelet", "TilesMethod", wavelet.Tilesmethod, pedited->wavelet.Tilesmethod);

    if (keyFile.has_key("Wavelet", "complexMethod")) {
        assignFromKeyfile(keyFile, "Wavelet", "complexMethod", wavelet.complexmethod, pedited->wavelet.complexmethod);
    } else if (wavelet.enabled) {
        wavelet.complexmethod = "expert";
        if (pedited) {
            pedited->wavelet.complexmethod = true;
        }
    }

    //assignFromKeyfile(keyFile, "Wavelet", "denMethod", wavelet.denmethod, pedited->wavelet.denmethod);
    assignFromKeyfile(keyFile, "Wavelet", "mixMethod", wavelet.mixmethod, pedited->wavelet.mixmethod);
    assignFromKeyfile(keyFile, "Wavelet", "sliMethod", wavelet.slimethod, pedited->wavelet.slimethod);
    assignFromKeyfile(keyFile, "Wavelet", "quaMethod", wavelet.quamethod, pedited->wavelet.quamethod);
    assignFromKeyfile(keyFile, "Wavelet", "DaubMethod", wavelet.daubcoeffmethod, pedited->wavelet.daubcoeffmethod);
    assignFromKeyfile(keyFile, "Wavelet", "CHromaMethod", wavelet.CHmethod, pedited->wavelet.CHmethod);
    assignFromKeyfile(keyFile, "Wavelet", "Medgreinf", wavelet.Medgreinf, pedited->wavelet.Medgreinf);
    assignFromKeyfile(keyFile, "Wavelet", "Ushamethod", wavelet.ushamethod, pedited->wavelet.ushamethod);
    assignFromKeyfile(keyFile, "Wavelet", "CHSLromaMethod", wavelet.CHSLmethod, pedited->wavelet.CHSLmethod);
    assignFromKeyfile(keyFile, "Wavelet", "EDMethod", wavelet.EDmethod, pedited->wavelet.EDmethod);
    assignFromKeyfile(keyFile, "Wavelet", "NPMethod", wavelet.NPmethod, pedited->wavelet.NPmethod);
    assignFromKeyfile(keyFile, "Wavelet", "BAMethod", wavelet.BAmethod, pedited->wavelet.BAmethod);
    assignFromKeyfile(keyFile, "Wavelet", "TMMethod", wavelet.TMmethod, pedited->wavelet.TMmethod);
    assignFromKeyfile(keyFile, "Wavelet", "HSMethod", wavelet.HSmethod, pedited->wavelet.HSmethod);
    assignFromKeyfile(keyFile, "Wavelet", "DirMethod", wavelet.Dirmethod, pedited->wavelet.Dirmethod);
    assignFromKeyfile(keyFile, "Wavelet", "Sigma", wavelet.sigma, pedited->wavelet.sigma);
    assignFromKeyfile(keyFile, "Wavelet", "Offset", wavelet.offset, pedited->wavelet.offset);
    assignFromKeyfile(keyFile, "Wavelet", "Lowthr", wavelet.lowthr, pedited->wavelet.lowthr);
    assignFromKeyfile(keyFile, "Wavelet", "ResidualcontShadow", wavelet.rescon, pedited->wavelet.rescon);
    assignFromKeyfile(keyFile, "Wavelet", "ResidualcontHighlight", wavelet.resconH, pedited->wavelet.resconH);
    assignFromKeyfile(keyFile, "Wavelet", "Residualchroma", wavelet.reschro, pedited->wavelet.reschro);
    assignFromKeyfile(keyFile, "Wavelet", "Residualblur", wavelet.resblur, pedited->wavelet.resblur);
    assignFromKeyfile(keyFile, "Wavelet", "Residualblurc", wavelet.resblurc, pedited->wavelet.resblurc);
    assignFromKeyfile(keyFile, "Wavelet", "ResidualTM", wavelet.tmrs, pedited->wavelet.tmrs);
    assignFromKeyfile(keyFile, "Wavelet", "ResidualEDGS", wavelet.edgs, pedited->wavelet.edgs);
    assignFromKeyfile(keyFile, "Wavelet", "ResidualSCALE", wavelet.scale, pedited->wavelet.scale);
    assignFromKeyfile(keyFile, "Wavelet", "Residualgamma", wavelet.gamma, pedited->wavelet.gamma);
    assignFromKeyfile(keyFile, "Wavelet", "ContExtra", wavelet.sup, pedited->wavelet.sup);
    assignFromKeyfile(keyFile, "Wavelet", "HueRangeResidual", wavelet.sky, pedited->wavelet.sky);
    assignFromKeyfile(keyFile, "Wavelet", "MaxLev", wavelet.thres, pedited->wavelet.thres);
    assignFromKeyfile(keyFile, "Wavelet", "ThresholdHighlight", wavelet.threshold, pedited->wavelet.threshold);
    assignFromKeyfile(keyFile, "Wavelet", "ThresholdShadow", wavelet.threshold2, pedited->wavelet.threshold2);
    assignFromKeyfile(keyFile, "Wavelet", "Edgedetect", wavelet.edgedetect, pedited->wavelet.edgedetect);
    assignFromKeyfile(keyFile, "Wavelet", "Edgedetectthr", wavelet.edgedetectthr, pedited->wavelet.edgedetectthr);
    assignFromKeyfile(keyFile, "Wavelet", "EdgedetectthrHi", wavelet.edgedetectthr2, pedited->wavelet.edgedetectthr2);
    assignFromKeyfile(keyFile, "Wavelet", "Edgesensi", wavelet.edgesensi, pedited->wavelet.edgesensi);
    assignFromKeyfile(keyFile, "Wavelet", "Edgeampli", wavelet.edgeampli, pedited->wavelet.edgeampli);
    assignFromKeyfile(keyFile, "Wavelet", "ThresholdChroma", wavelet.chroma, pedited->wavelet.chroma);
    assignFromKeyfile(keyFile, "Wavelet", "ChromaLink", wavelet.chro, pedited->wavelet.chro);
    assignFromKeyfile(keyFile, "Wavelet", "Contrast", wavelet.contrast, pedited->wavelet.contrast);
    assignFromKeyfile(keyFile, "Wavelet", "Edgrad", wavelet.edgrad, pedited->wavelet.edgrad);
    assignFromKeyfile(keyFile, "Wavelet", "Edgeffect", wavelet.edgeffect, pedited->wavelet.edgeffect);
    assignFromKeyfile(keyFile, "Wavelet", "Edgval", wavelet.edgval, pedited->wavelet.edgval);
    assignFromKeyfile(keyFile, "Wavelet", "ThrEdg", wavelet.edgthresh, pedited->wavelet.edgthresh);
    assignFromKeyfile(keyFile, "Wavelet", "ThresholdResidShadow", wavelet.thr, pedited->wavelet.thr);
    assignFromKeyfile(keyFile, "Wavelet", "ThresholdResidHighLight", wavelet.thrH, pedited->wavelet.thrH);
    assignFromKeyfile(keyFile, "Wavelet", "Residualradius", wavelet.radius, pedited->wavelet.radius);
    assignFromKeyfile(keyFile, "Wavelet", "ContrastCurve", wavelet.ccwcurve, pedited->wavelet.ccwcurve);
    assignFromKeyfile(keyFile, "Wavelet", "blcurve", wavelet.blcurve, pedited->wavelet.blcurve);
    assignFromKeyfile(keyFile, "Wavelet", "OpacityCurveRG", wavelet.opacityCurveRG, pedited->wavelet.opacityCurveRG);
    //assignFromKeyfile(keyFile, "Wavelet", "Levalshc", wavelet.opacityCurveSH, pedited->wavelet.opacityCurveSH);
    assignFromKeyfile(keyFile, "Wavelet", "OpacityCurveBY", wavelet.opacityCurveBY, pedited->wavelet.opacityCurveBY);
    assignFromKeyfile(keyFile, "Wavelet", "wavdenoise", wavelet.wavdenoise, pedited->wavelet.wavdenoise);
    assignFromKeyfile(keyFile, "Wavelet", "wavdenoiseh", wavelet.wavdenoiseh, pedited->wavelet.wavdenoiseh);
    assignFromKeyfile(keyFile, "Wavelet", "OpacityCurveW", wavelet.opacityCurveW, pedited->wavelet.opacityCurveW);
    assignFromKeyfile(keyFile, "Wavelet", "OpacityCurveWL", wavelet.opacityCurveWL, pedited->wavelet.opacityCurveWL);
    assignFromKeyfile(keyFile, "Wavelet", "HHcurve", wavelet.hhcurve, pedited->wavelet.hhcurve);
    assignFromKeyfile(keyFile, "Wavelet", "Wavguidcurve", wavelet.wavguidcurve, pedited->wavelet.wavguidcurve);
    assignFromKeyfile(keyFile, "Wavelet", "Wavhuecurve", wavelet.wavhuecurve, pedited->wavelet.wavhuecurve);
    assignFromKeyfile(keyFile, "Wavelet", "CHcurve", wavelet.Chcurve, pedited->wavelet.Chcurve);
    assignFromKeyfile(keyFile, "Wavelet", "WavclCurve", wavelet.wavclCurve, pedited->wavelet.wavclCurve);

    if (keyFile.has_key("Wavelet", "Hueskin")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "Hueskin");

        if (thresh.size() >= 4) {
            wavelet.hueskin.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.hueskin = true;
        }
    }

    if (keyFile.has_key("Wavelet", "HueRange")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "HueRange");

        if (thresh.size() >= 4) {
            wavelet.hueskin2.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.hueskin2 = true;
        }
    }

    if (keyFile.has_key("Wavelet", "HLRange")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "HLRange");

        if (thresh.size() >= 4) {
            wavelet.hllev.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.hllev = true;
        }
    }

    if (keyFile.has_key("Wavelet", "SHRange")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "SHRange");

        if (thresh.size() >= 4) {
            wavelet.bllev.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.bllev = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Edgcont")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "Edgcont");

        if (thresh.size() >= 4) {
            wavelet.edgcont.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.edgcont = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Level0noise")) {
        const std::vector<double> thresh = keyFile.get_double_list("Wavelet", "Level0noise");

        if (thresh.size() >= 2) {
            wavelet.level0noise.setValues(thresh[0], thresh[1]);
        }

        if (pedited) {
            pedited->wavelet.level0noise = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Level1noise")) {
        const std::vector<double> thresh = keyFile.get_double_list("Wavelet", "Level1noise");

        if (thresh.size() >= 2) {
            wavelet.level1noise.setValues(thresh[0], thresh[1]);
        }

        if (pedited) {
            pedited->wavelet.level1noise = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Level2noise")) {
        const std::vector<double> thresh = keyFile.get_double_list("Wavelet", "Level2noise");

        if (thresh.size() >= 2) {
            wavelet.level2noise.setValues(thresh[0], thresh[1]);
        }

        if (pedited) {
            pedited->wavelet.level2noise = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Level3noise")) {
        const std::vector<double> thresh = keyFile.get_double_list("Wavelet", "Level3noise");

        if (thresh.size() >= 2) {
            wavelet.level3noise.setValues(thresh[0], thresh[1]);
        }

        if (pedited) {
            pedited->wavelet.level3noise = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Leveldenoise")) {
        const std::vector<double> thresh = keyFile.get_double_list("Wavelet", "Leveldenoise");

        if (thresh.size() >= 2) {
            wavelet.leveldenoise.setValues(thresh[0], thresh[1]);
        }

        if (pedited) {
            pedited->wavelet.leveldenoise = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Levelsigm")) {
        const std::vector<double> thresh = keyFile.get_double_list("Wavelet", "Levelsigm");

        if (thresh.size() >= 2) {
            wavelet.levelsigm.setValues(thresh[0], thresh[1]);
        }

        if (pedited) {
            pedited->wavelet.levelsigm = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Pastlev")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "Pastlev");

        if (thresh.size() >= 4) {
            wavelet.pastlev.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.pastlev = true;
        }
    }

    if (keyFile.has_key("Wavelet", "Satlev")) {
        const std::vector<int> thresh = keyFile.get_integer_list("Wavelet", "Satlev");

        if (thresh.size() >= 4) {
            wavelet.satlev.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
        }

        if (pedited) {
            pedited->wavelet.satlev = true;
        }
    }

    assignFromKeyfile(keyFile, "Wavelet", "Skinprotect", wavelet.skinprotect, pedited->wavelet.skinprotect);
    assignFromKeyfile(keyFile, "Wavelet", "chrwav", wavelet.chrwav, pedited->wavelet.chrwav);
    assignFromKeyfile(keyFile, "Wavelet", "bluwav", wavelet.bluwav, pedited->wavelet.bluwav);
    assignFromKeyfile(keyFile, "Wavelet", "Expcontrast", wavelet.expcontrast, pedited->wavelet.expcontrast);
    assignFromKeyfile(keyFile, "Wavelet", "Expchroma", wavelet.expchroma, pedited->wavelet.expchroma);

    for (int i = 0; i < 9; ++i) {
        std::stringstream ss;
        ss << "Contrast" << (i + 1);

        if (keyFile.has_key("Wavelet", ss.str())) {
            wavelet.c[i] = keyFile.get_integer("Wavelet", ss.str());

            if (pedited) {
                pedited->wavelet.c[i] = true;
            }
        }
    }

    for (int i = 0; i < 9; ++i) {
        std::stringstream ss;
        ss << "Chroma" << (i + 1);

        if (keyFile.has_key("Wavelet", ss.str())) {
            wavelet.ch[i] = keyFile.get_integer("Wavelet", ss.str());

            if (pedited) {
                pedited->wavelet.ch[i] = true;
            }
        }
    }

    assignFromKeyfile(keyFile, "Wavelet", "Expedge", wavelet.expedge, pedited->wavelet.expedge);
    assignFromKeyfile(keyFile, "Wavelet", "expbl", wavelet.expbl, pedited->wavelet.expbl);
    assignFromKeyfile(keyFile, "Wavelet", "Expresid", wavelet.expresid, pedited->wavelet.expresid);
    assignFromKeyfile(keyFile, "Wavelet", "Expfinal", wavelet.expfinal, pedited->wavelet.expfinal);
    assignFromKeyfile(keyFile, "Wavelet", "Exptoning", wavelet.exptoning, pedited->wavelet.exptoning);
    assignFromKeyfile(keyFile, "Wavelet", "Expnoise", wavelet.expnoise, pedited->wavelet.expnoise);
    assignFromKeyfile(keyFile, "Wavelet", "Expclari", wavelet.expclari, pedited->wavelet.expclari);
}

void saveWaveletParams(Glib::KeyFile& keyFile, const WaveletParams& wavelet,
                       const ParamsEdited* pedited)
{
    saveToKeyfile(!pedited || pedited->wavelet.enabled, "Wavelet", "Enabled", wavelet.enabled, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.strength, "Wavelet", "Strength", wavelet.strength, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.balance, "Wavelet", "Balance", wavelet.balance, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sigmafin, "Wavelet", "Sigmafin", wavelet.sigmafin, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sigmaton, "Wavelet", "Sigmaton", wavelet.sigmaton, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sigmacol, "Wavelet", "Sigmacol", wavelet.sigmacol, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sigmadir, "Wavelet", "Sigmadir", wavelet.sigmadir, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.rangeab, "Wavelet", "Rangeab", wavelet.rangeab, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.protab, "Wavelet", "Protab", wavelet.protab, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.iter, "Wavelet", "Iter", wavelet.iter, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.thres, "Wavelet", "MaxLev", wavelet.thres, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.Tilesmethod, "Wavelet", "TilesMethod", wavelet.Tilesmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.complexmethod, "Wavelet", "complexMethod", wavelet.complexmethod, keyFile);
    //saveToKeyfile(!pedited || pedited->wavelet.denmethod, "Wavelet", "denMethod", wavelet.denmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.mixmethod, "Wavelet", "mixMethod", wavelet.mixmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.slimethod, "Wavelet", "sliMethod", wavelet.slimethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.quamethod, "Wavelet", "quaMethod", wavelet.quamethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.daubcoeffmethod, "Wavelet", "DaubMethod", wavelet.daubcoeffmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.CLmethod, "Wavelet", "ChoiceLevMethod", wavelet.CLmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.Backmethod, "Wavelet", "BackMethod", wavelet.Backmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.Lmethod, "Wavelet", "LevMethod", wavelet.Lmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.Dirmethod, "Wavelet", "DirMethod", wavelet.Dirmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.greenhigh, "Wavelet", "CBgreenhigh", wavelet.greenhigh, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.greenmed, "Wavelet", "CBgreenmed", wavelet.greenmed, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.greenlow, "Wavelet", "CBgreenlow", wavelet.greenlow, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.bluehigh, "Wavelet", "CBbluehigh", wavelet.bluehigh, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.bluemed, "Wavelet", "CBbluemed", wavelet.bluemed, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.bluelow, "Wavelet", "CBbluelow", wavelet.bluelow, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.ballum, "Wavelet", "Ballum", wavelet.ballum, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sigm, "Wavelet", "Sigm", wavelet.sigm, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.levden, "Wavelet", "Levden", wavelet.levden, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.thrden, "Wavelet", "Thrden", wavelet.thrden, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.limden, "Wavelet", "Limden", wavelet.limden, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.balchrom, "Wavelet", "Balchrom", wavelet.balchrom, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.chromfi, "Wavelet", "Chromfine", wavelet.chromfi, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.chromco, "Wavelet", "Chromcoarse", wavelet.chromco, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.mergeL, "Wavelet", "MergeL", wavelet.mergeL, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.mergeC, "Wavelet", "MergeC", wavelet.mergeC, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.softrad, "Wavelet", "Softrad", wavelet.softrad, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.softradend, "Wavelet", "Softradend", wavelet.softradend, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.strend, "Wavelet", "Strend", wavelet.strend, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.detend, "Wavelet", "Detend", wavelet.detend, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.thrend, "Wavelet", "Thrend", wavelet.thrend, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expcontrast, "Wavelet", "Expcontrast", wavelet.expcontrast, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expchroma, "Wavelet", "Expchroma", wavelet.expchroma, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expedge, "Wavelet", "Expedge", wavelet.expedge, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expbl, "Wavelet", "expbl", wavelet.expbl, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expresid, "Wavelet", "Expresid", wavelet.expresid, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expfinal, "Wavelet", "Expfinal", wavelet.expfinal, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.exptoning, "Wavelet", "Exptoning", wavelet.exptoning, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expnoise, "Wavelet", "Expnoise", wavelet.expnoise, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.expclari, "Wavelet", "Expclari", wavelet.expclari, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.labgridALow, "Wavelet", "LabGridALow", wavelet.labgridALow, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.labgridBLow, "Wavelet", "LabGridBLow", wavelet.labgridBLow, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.labgridAHigh, "Wavelet", "LabGridAHigh", wavelet.labgridAHigh, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.labgridBHigh, "Wavelet", "LabGridBHigh", wavelet.labgridBHigh, keyFile);

    for (int i = 0; i < 9; i++) {
        std::stringstream ss;
        ss << "Contrast" << (i + 1);

        saveToKeyfile(!pedited || pedited->wavelet.c[i], "Wavelet", ss.str(), wavelet.c[i], keyFile);
    }

    for (int i = 0; i < 9; i++) {
        std::stringstream ss;
        ss << "Chroma" << (i + 1);

        saveToKeyfile(!pedited || pedited->wavelet.ch[i], "Wavelet", ss.str(), wavelet.ch[i], keyFile);
    }

    saveToKeyfile(!pedited || pedited->wavelet.sup, "Wavelet", "ContExtra", wavelet.sup, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.HSmethod, "Wavelet", "HSMethod", wavelet.HSmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.hllev, "Wavelet", "HLRange", wavelet.hllev.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.bllev, "Wavelet", "SHRange", wavelet.bllev.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgcont, "Wavelet", "Edgcont", wavelet.edgcont.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.level0noise, "Wavelet", "Level0noise", wavelet.level0noise.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.level1noise, "Wavelet", "Level1noise", wavelet.level1noise.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.level2noise, "Wavelet", "Level2noise", wavelet.level2noise.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.level3noise, "Wavelet", "Level3noise", wavelet.level3noise.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.leveldenoise, "Wavelet", "Leveldenoise", wavelet.leveldenoise.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.levelsigm, "Wavelet", "Levelsigm", wavelet.levelsigm.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.threshold, "Wavelet", "ThresholdHighlight", wavelet.threshold, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.threshold2, "Wavelet", "ThresholdShadow", wavelet.threshold2, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgedetect, "Wavelet", "Edgedetect", wavelet.edgedetect, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgedetectthr, "Wavelet", "Edgedetectthr", wavelet.edgedetectthr, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgedetectthr2, "Wavelet", "EdgedetectthrHi", wavelet.edgedetectthr2, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgesensi, "Wavelet", "Edgesensi", wavelet.edgesensi, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgeampli, "Wavelet", "Edgeampli", wavelet.edgeampli, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.chroma, "Wavelet", "ThresholdChroma", wavelet.chroma, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.CHmethod, "Wavelet", "CHromaMethod", wavelet.CHmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.Medgreinf, "Wavelet", "Medgreinf", wavelet.Medgreinf, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.ushamethod, "Wavelet", "Ushamethod", wavelet.ushamethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.CHSLmethod, "Wavelet", "CHSLromaMethod", wavelet.CHSLmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.EDmethod, "Wavelet", "EDMethod", wavelet.EDmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.NPmethod, "Wavelet", "NPMethod", wavelet.NPmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.BAmethod, "Wavelet", "BAMethod", wavelet.BAmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.TMmethod, "Wavelet", "TMMethod", wavelet.TMmethod, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.chro, "Wavelet", "ChromaLink", wavelet.chro, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.ccwcurve, "Wavelet", "ContrastCurve", wavelet.ccwcurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.blcurve, "Wavelet", "blcurve", wavelet.blcurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.pastlev, "Wavelet", "Pastlev", wavelet.pastlev.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.satlev, "Wavelet", "Satlev", wavelet.satlev.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.opacityCurveRG, "Wavelet", "OpacityCurveRG", wavelet.opacityCurveRG, keyFile);
    //saveToKeyfile(!pedited || pedited->wavelet.opacityCurveSH, "Wavelet", "Levalshc", wavelet.opacityCurveSH, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.opacityCurveBY, "Wavelet", "OpacityCurveBY", wavelet.opacityCurveBY, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.wavdenoise, "Wavelet", "wavdenoise", wavelet.wavdenoise, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.wavdenoiseh, "Wavelet", "wavdenoiseh", wavelet.wavdenoiseh, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.opacityCurveW, "Wavelet", "OpacityCurveW", wavelet.opacityCurveW, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.opacityCurveWL, "Wavelet", "OpacityCurveWL", wavelet.opacityCurveWL, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.hhcurve, "Wavelet", "HHcurve", wavelet.hhcurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.wavguidcurve, "Wavelet", "Wavguidcurve", wavelet.wavguidcurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.wavhuecurve, "Wavelet", "Wavhuecurve", wavelet.wavhuecurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.Chcurve, "Wavelet", "CHcurve", wavelet.Chcurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.wavclCurve, "Wavelet", "WavclCurve", wavelet.wavclCurve, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.median, "Wavelet", "Median", wavelet.median, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.medianlev, "Wavelet", "Medianlev", wavelet.medianlev, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.linkedg, "Wavelet", "Linkedg", wavelet.linkedg, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.cbenab, "Wavelet", "CBenab", wavelet.cbenab, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.lipst, "Wavelet", "Lipst", wavelet.lipst, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.skinprotect, "Wavelet", "Skinprotect", wavelet.skinprotect, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.chrwav, "Wavelet", "chrwav", wavelet.chrwav, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.bluwav, "Wavelet", "bluwav", wavelet.bluwav, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.hueskin, "Wavelet", "Hueskin", wavelet.hueskin.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgrad, "Wavelet", "Edgrad", wavelet.edgrad, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgeffect, "Wavelet", "Edgeffect", wavelet.edgeffect, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgval, "Wavelet", "Edgval", wavelet.edgval, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgthresh, "Wavelet", "ThrEdg", wavelet.edgthresh, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.avoid, "Wavelet", "AvoidColorShift", wavelet.avoid, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.showmask, "Wavelet", "Showmask", wavelet.showmask, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.oldsh, "Wavelet", "Oldsh", wavelet.oldsh, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.tmr, "Wavelet", "TMr", wavelet.tmr, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sigma, "Wavelet", "Sigma", wavelet.sigma, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.offset, "Wavelet", "Offset", wavelet.offset, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.lowthr, "Wavelet", "Lowthr", wavelet.lowthr, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.rescon, "Wavelet", "ResidualcontShadow", wavelet.rescon, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.resconH, "Wavelet", "ResidualcontHighlight", wavelet.resconH, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.thr, "Wavelet", "ThresholdResidShadow", wavelet.thr, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.thrH, "Wavelet", "ThresholdResidHighLight", wavelet.thrH, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.radius, "Wavelet", "Residualradius", wavelet.radius, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.reschro, "Wavelet", "Residualchroma", wavelet.reschro, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.resblur, "Wavelet", "Residualblur", wavelet.resblur, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.resblurc, "Wavelet", "Residualblurc", wavelet.resblurc, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.tmrs, "Wavelet", "ResidualTM", wavelet.tmrs, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.edgs, "Wavelet", "ResidualEDGS", wavelet.edgs, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.scale, "Wavelet", "ResidualSCALE", wavelet.scale, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.gamma, "Wavelet", "Residualgamma", wavelet.gamma, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.sky, "Wavelet", "HueRangeResidual", wavelet.sky, keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.hueskin2, "Wavelet", "HueRange", wavelet.hueskin2.toVector(), keyFile);
    saveToKeyfile(!pedited || pedited->wavelet.contrast, "Wavelet", "Contrast", wavelet.contrast, keyFile);
}

}  // namespace procparams
}  // namespace rtengine
