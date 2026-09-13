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

#pragma once

#include "threshold.h"

#include <glibmm/ustring.h>

#include <vector>

namespace Glib {
class KeyFile;
}

class ParamsEdited;

namespace rtengine {

class RetinexgaintransmissionCurve;
class RetinextransmissionCurve;
class WavCurve;
class Wavblcurve;
class WavOpacityCurveBY;
class WavOpacityCurveSH;
class WavOpacityCurveRG;
class WavOpacityCurveW;
class WavOpacityCurveWL;

namespace procparams {

struct ColorAppearanceParams {
    enum class TcMode {
        LIGHT,    // Lightness mode
        BRIGHT,   // Brightness mode
    };

    enum class CtcMode {
        CHROMA,   // chroma mode
        SATUR,    // saturation mode
        COLORF,   // colorfullness mode
    };

    bool          enabled;
    int           degree;
    bool          autodegree;
    int           degreeout;
    bool          autodegreeout;
    std::vector<double> curve;
    std::vector<double> curvered;
    std::vector<double> curvegreen;
    std::vector<double> curveblue;
    std::vector<double> curve2;
    std::vector<double> curve3;
    TcMode     curveMode;
    TcMode     curveMode2;
    CtcMode    curveMode3;
    Glib::ustring complexmethod;
    Glib::ustring modelmethod;
    Glib::ustring catmethod;

    Glib::ustring surround;
    Glib::ustring surrsrc;
    double        adapscen;
    bool          autoadapscen;
    int        ybscen;
    bool          autoybscen;

    double        adaplum;
    int           badpixsl;
    Glib::ustring wbmodel;
    Glib::ustring illum;
    Glib::ustring algo;
    double        contrast;
    double        qcontrast;
    double        jlight;
    double        qbright;
    double        chroma;
    double        schroma;
    double        schromared;
    double        schromagreen;
    double        schromablue;
    double        mchroma;
    double        colorh;
    double        colorhred;
    double        colorhgreen;
    double        colorhblue;
    double        brighthres;
    double        rstprotection;
    bool          surrsource;
    bool          gamut;
    bool          datacie;
    bool          tonecie;
    int tempout;
    bool          autotempout;
    int ybout;
    double greenout;
    int tempsc;
    double greensc;

    ColorAppearanceParams();

    bool operator ==(const ColorAppearanceParams& other) const;
    bool operator !=(const ColorAppearanceParams& other) const;
};

struct RetinexParams {
    bool enabled;
    std::vector<double>   cdcurve;
    std::vector<double>   cdHcurve;
    std::vector<double>   lhcurve;
    std::vector<double> transmissionCurve;
    std::vector<double> gaintransmissionCurve;
    std::vector<double>   mapcurve;
    int     str;
    int     scal;
    int     iter;
    int     grad;
    int     grads;
    double  gam;
    double  slope;
    int     neigh;
    int     offs;
    int     highlights;
    int     htonalwidth;
    int     shadows;
    int     stonalwidth;
    int     radius;

    Glib::ustring complexmethod;
    Glib::ustring retinexMethod;
    Glib::ustring retinexcolorspace;
    Glib::ustring gammaretinex;
    Glib::ustring mapMethod;
    Glib::ustring viewMethod;
    int     vart;
    int     limd;
    int     highl;
    int     skal;
    bool    medianmap;

    RetinexParams();

    bool operator ==(const RetinexParams& other) const;
    bool operator !=(const RetinexParams& other) const;

    void getCurves(RetinextransmissionCurve& transmissionCurveLUT, RetinexgaintransmissionCurve& gaintransmissionCurveLUT) const;
};

struct WaveletParams {
    std::vector<double> ccwcurve;
    std::vector<double> wavdenoise;
    std::vector<double> wavdenoiseh;
    std::vector<double> blcurve;
    std::vector<double> levelshc;
    std::vector<double> opacityCurveRG;
    //std::vector<double> opacityCurveSH;
    std::vector<double> opacityCurveBY;
    std::vector<double> opacityCurveW;
    std::vector<double> opacityCurveWL;
    std::vector<double> hhcurve;
    std::vector<double> wavguidcurve;
    std::vector<double> wavhuecurve;
    std::vector<double> Chcurve;
    std::vector<double> wavclCurve;
    bool enabled;
    bool median;
    bool medianlev;
    bool linkedg;
    bool cbenab;
    int greenlow;
    int bluelow;
    int greenmed;
    int bluemed;
    int greenhigh;
    int bluehigh;
    double ballum;
    double sigm;
    double levden;
    double thrden;
    double limden;
    double balchrom;
    double chromfi;
    double chromco;
    double mergeL;
    double mergeC;
    double softrad;
    double softradend;
    double strend;
    int detend;
    double thrend;

    bool lipst;
    bool avoid;
    bool showmask;
    bool oldsh;
    bool tmr;
    int strength;
    int balance;
    double sigmafin;
    double sigmaton;
    double sigmacol;
    double sigmadir;
    double rangeab;
    double protab;
    int iter;
    bool expcontrast;
    bool expchroma;
    int c[9];
    int ch[9];
    bool expedge;
    bool expbl;
    bool expresid;
    bool expfinal;
    bool exptoning;
    bool expnoise;
    bool expclari;
    double labgridALow;
    double labgridBLow;
    double labgridAHigh;
    double labgridBHigh;
    static const double LABGRID_CORR_MAX;
    static const double LABGRID_CORR_SCALE;
    static const double LABGRIDL_DIRECT_SCALE;
    int Lmethod;
    Glib::ustring CLmethod;
    Glib::ustring Backmethod;
    Glib::ustring Tilesmethod;
    Glib::ustring complexmethod;
    //Glib::ustring denmethod;
    Glib::ustring mixmethod;
    Glib::ustring slimethod;
    Glib::ustring quamethod;
    Glib::ustring daubcoeffmethod;
    Glib::ustring CHmethod;
    Glib::ustring Medgreinf;
    Glib::ustring ushamethod;
    Glib::ustring CHSLmethod;
    Glib::ustring EDmethod;
    Glib::ustring NPmethod;
    Glib::ustring BAmethod;
    Glib::ustring TMmethod;
    Glib::ustring Dirmethod;
    Glib::ustring HSmethod;
    double sigma;
    double offset;
    double lowthr;
    int rescon;
    int resconH;
    int reschro;
    int resblur;
    int resblurc;
    double tmrs;
    double edgs;
    double scale;
    double gamma;
    int sup;
    double sky;
    int thres;
    int chroma;
    int chro;
    int threshold;
    int threshold2;
    int edgedetect;
    int edgedetectthr;
    int edgedetectthr2;
    int edgesensi;
    int edgeampli;
    int contrast;
    int edgrad;
    double edgeffect;
    int edgval;
    int edgthresh;
    int thr;
    int thrH;
    int radius;
    double skinprotect;
    double chrwav;
    double bluwav;
    Threshold<int> hueskin;
    Threshold<int> hueskin2;
    Threshold<int> hllev;
    Threshold<int> bllev;
    Threshold<int> pastlev;
    Threshold<int> satlev;
    Threshold<int> edgcont;
    Threshold<double> level0noise;
    Threshold<double> level1noise;
    Threshold<double> level2noise;
    Threshold<double> level3noise;
    Threshold<double> leveldenoise;
    Threshold<double> levelsigm;

    WaveletParams();

    bool operator ==(const WaveletParams& other) const;
    bool operator !=(const WaveletParams& other) const;

    void getCurves(
        WavCurve& cCurve,
        WavCurve& wavdenoise,
        WavCurve& wavdenoiseh,
        Wavblcurve& tCurve,
        WavOpacityCurveRG& opacityCurveLUTRG,
        WavOpacityCurveSH& opacityCurveLUTSH,
        WavOpacityCurveBY& opacityCurveLUTBY,
        WavOpacityCurveW& opacityCurveLUTW,
        WavOpacityCurveWL& opacityCurveLUTWL 
    ) const;
};

void loadColorAppearanceParams(const Glib::KeyFile& keyFile,
                               ColorAppearanceParams& colorappearance,
                               ParamsEdited* pedited,
                               int ppVersion);

void saveColorAppearanceParams(Glib::KeyFile& keyFile,
                               const ColorAppearanceParams& colorappearance,
                               const ParamsEdited* pedited);

void loadRetinexParams(const Glib::KeyFile& keyFile, RetinexParams& retinex,
                       ParamsEdited* pedited);

void saveRetinexParams(Glib::KeyFile& keyFile, const RetinexParams& retinex,
                       const ParamsEdited* pedited);

void loadWaveletParams(const Glib::KeyFile& keyFile, WaveletParams& wavelet,
                       ParamsEdited* pedited, int ppVersion);

void saveWaveletParams(Glib::KeyFile& keyFile, const WaveletParams& wavelet,
                       const ParamsEdited* pedited);

}  // namespace procparams
}  // namespace rtengine
