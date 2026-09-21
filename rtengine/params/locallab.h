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

#pragma once

#include "threshold.h"

#include <glibmm/ustring.h>

#include <vector>

namespace Glib {
class KeyFile;
}

class ParamsEdited;

namespace rtengine {
namespace procparams {

struct LocallabParams {
    struct LocallabSpot {
        // Control spot settings
        Glib::ustring name;
        bool isvisible;
        Glib::ustring prevMethod; // show, hide
        Glib::ustring shape; // ELI, RECT
        Glib::ustring spotMethod; // norm, exc
        Glib::ustring wavMethod; // D2, D4, D6, D10, D14
        int sensiexclu;
        int structexclu;
        double struc;
        Glib::ustring shapeMethod; // IND, SYM, INDSL, SYMSL
        Glib::ustring avoidgamutMethod; // NONE, LAB, XYZ
		
        std::vector<int> loc; // For ellipse/rectangle: {locX, locXL, locY, locYT}
        int centerX;
        int centerY;
        int circrad;
        Glib::ustring qualityMethod; // none, std, enh, enhsup, contr, sob2
        Glib::ustring complexMethod; // sim, mod, all
        double transit;
        double feather;
        double thresh;
        double iter;
        double balan;
        double balanh;
        double colorde;
        double colorscope;
        double avoidrad;
        double transitweak;
        double transitgrad;
        bool hishow;
        bool activ;
        bool avoidneg;
        bool blwh;
        bool recurs;
        bool laplac;
        bool deltae;
        bool shortc;
        bool savrest;
        int scopemask;
        double denoichmask;
        int lumask;
        // Color & Light
        bool visicolor;
        bool expcolor;
        int complexcolor;
        bool curvactiv;
        int lightness;
        double reparcol;
        double gamc;
        int contrast;
        int chroma;
        double labgridALow;
        double labgridBLow;
        double labgridAHigh;
        double labgridBHigh;
        double labgridALowmerg;
        double labgridBLowmerg;
        double labgridAHighmerg;
        double labgridBHighmerg;
        int strengthgrid;
        int sensi;
        int structcol;
        double strcol;
        double strcolab;
        double strcolh;
        double angcol;
        double feathercol;
        int blurcolde;
        double blurcol;
        double contcol;
        int blendmaskcol;
        double radmaskcol;
        double chromaskcol;
        double gammaskcol;
        double slomaskcol;
        int shadmaskcol;
        double strumaskcol;
        double lapmaskcol;
        Glib::ustring qualitycurveMethod; // none, std
        Glib::ustring gridMethod; // one, two
        Glib::ustring merMethod; // mone, mtwo, mthr, mfou, mfiv
        Glib::ustring toneMethod; // one, two, thr, fou
        Glib::ustring mergecolMethod; // one, two, thr, fou, fiv, six, sev, sev0, sev1, sev2, hei, nin, ten, ele, twe, thi, for, hue, sat, col, lum
        std::vector<double> llcurve;
        std::vector<double> lccurve;
        std::vector<double> cccurve;
        std::vector<double> clcurve;
        std::vector<double> rgbcurve;
        std::vector<double> LHcurve;
        std::vector<double> HHcurve;
        std::vector<double> CHcurve;
        bool invers;
        bool special;
        bool toolcol;
        bool enaColorMask;
        bool fftColorMask;
        std::vector<double> CCmaskcurve;
        std::vector<double> LLmaskcurve;
        std::vector<double> HHmaskcurve;
        std::vector<double> HHhmaskcurve;
        double softradiuscol;
        double opacol;
        double mercol;
        double merlucol;
        double conthrcol;
        std::vector<double> Lmaskcurve;
        std::vector<double> LLmaskcolcurvewav;
        Threshold<int> csthresholdcol;
        double recothresc;
        double lowthresc;
        double higthresc;
        double decayc;
        // Exposure
        bool visiexpose;
        bool expexpose;
        int complexexpose;
        double expcomp;
        int hlcompr;
        int hlcomprthresh;
        int black;
        int shadex;
        int shcompr;
        int expchroma;
        int sensiex;
        int structexp;
        int blurexpde;
        double gamex;
        double strexp;
        double angexp;
        double featherexp;
        std::vector<double> excurve;
        bool norm;
        bool inversex;
        bool enaExpMask;
        bool enaExpMaskaft;
        std::vector<double> CCmaskexpcurve;
        std::vector<double> LLmaskexpcurve;
        std::vector<double> HHmaskexpcurve;
        int blendmaskexp;
        double radmaskexp;
        double chromaskexp;
        double gammaskexp;
        double slomaskexp;
        double lapmaskexp;
        double strmaskexp;
        double angmaskexp;
        double softradiusexp;
        std::vector<double> Lmaskexpcurve;
        Glib::ustring expMethod; // std, pde
        Glib::ustring exnoiseMethod; // none, med, medhi
        double laplacexp;
        double reparexp;
        double balanexp;
        double linear;
        double gamm;
        double fatamount;
        double fatdetail;
        bool fatsatur;
        double fatanchor;
        double fatlevel;
        double recothrese;
        double lowthrese;
        double higthrese;
        double decaye;
        // Shadow highlight
        bool visishadhigh;
        bool expshadhigh;
        int complexshadhigh;
        Glib::ustring shMethod; // std, tone
        Glib::ustring ghsMethod; // rgb, lum, sat
        Glib::ustring ghsMatmet;//none agx Jz cam16
        Glib::ustring ghsMode; // lin, ghs
        double ghs_D;
        double ghs_slope;
        double ghs_chro;
        double ghs_B;
        double ghs_SP;
        bool SPAutoRadius;
        double ghs_LP;
        double ghs_HP;
        double ghs_LC;
        double ghs_MID;
        double ghs_BLP;
        double ghs_HLP;
        bool ghs_autobw;
        bool ghs_agx;
        bool ghs_smooth;
        bool ghs_inv;
        double mich_exp;
        double mich_spar;
        double mich_kpar;
        double mich_sat;
        double mich_out;
        bool mich_black;
        bool mich_white;
        double mich_high;
        bool mich_jdx;

        int multsh[6];
        int highlights;
        int h_tonalwidth;
        int shadows;
        int s_tonalwidth;
        int sh_radius;
        int sensihs;
        bool enaSHMask;
        std::vector<double> CCmaskSHcurve;
        std::vector<double> LLmaskSHcurve;
        std::vector<double> HHmaskSHcurve;
        int blendmaskSH;
        double radmaskSH;
        int blurSHde;
        double strSH;
        double angSH;
        double featherSH;
        bool inverssh;
        double chromaskSH;
        double gammaskSH;
        double slomaskSH;
        double lapmaskSH;
        int detailSH;
        double tePivot;
        double reparsh;
        std::vector<double> LmaskSHcurve;
        double fatamountSH;
        double fatanchorSH;
        double gamSH;
        double sloSH;
        double recothress;
        double lowthress;
        double higthress;
        double decays;
        // Vibrance
        bool visivibrance;
        bool expvibrance;
        int complexvibrance;
        int saturated;
        int pastels;
        double vibgam;
        int warm;
        Threshold<int> psthreshold;
        bool protectskins;
        bool avoidcolorshift;
        bool pastsattog;
        int sensiv;
        std::vector<double> skintonescurve;
        std::vector<double> CCmaskvibcurve;
        std::vector<double> LLmaskvibcurve;
        std::vector<double> HHmaskvibcurve;
        bool enavibMask;
        int blendmaskvib;
        double radmaskvib;
        double chromaskvib;
        double gammaskvib;
        double slomaskvib;
        double lapmaskvib;
        double strvib;
        double strvibab;
        double strvibh;
        double angvib;
        double feathervib;
        std::vector<double> Lmaskvibcurve;
        double recothresv;
        double lowthresv;
        double higthresv;
        double decayv;
        // Soft Light
        bool visisoft;
        bool expsoft;
        int complexsoft;
        int streng;
        int sensisf;
        double laplace;
        Glib::ustring softMethod; // soft, reti
        // Blur & Noise
        bool visiblur;
        bool expblur;
        int complexblur;
        double radius;
        int strength;
        int sensibn;
        int itera;
        int guidbl;
        int strbl;
        double recothres;
        double lowthres;
        double higthres;
        double recothresd;
        double lowthresd;
        double midthresd;
        double midthresdch;
        double higthresd;
        double decayd;
        int isogr;
        int strengr;
        int scalegr;
        double divgr;
        int epsbl;
        Glib::ustring blMethod; // blur, med, guid
        Glib::ustring chroMethod; // lum, chr, all
        Glib::ustring quamethod; // cons agre
        Glib::ustring blurMethod; // norm, inv
        Glib::ustring medMethod; // none, 33, 55, 77, 99
        bool usemask;
        bool invmaskd;
        bool invmask;
        double levelthr;
        double lnoiselow;
        double levelthrlow;
        bool activlum;
        double madlsav[21];
        double noiselumf;
        double noiselumf0;
        double noiselumf2;
        double noiselumc;
        double noiselumdetail;
        int noiselequal;
        double noisegam;
        double noisechrof;
        double noisechroc;
        double noisechrodetail;
        int adjblur;
        int bilateral;
        int nlstr;
        int nldet;
        int nlpat;
        int nlrad;
        double nlgam;
        int nliter;
        int sensiden;
        double reparden;
        int detailthr;
        std::vector<double> locwavcurveden;
        std::vector<double> locwavcurvehue;
        std::vector<double> locwavcurvehuecont;
        Glib::ustring showmaskblMethodtyp;
        std::vector<double> CCmaskblcurve;
        std::vector<double> LLmaskblcurve;
        std::vector<double> HHmaskblcurve;
        bool enablMask;
        bool fftwbl;
        bool invbl;
        bool toolbl;
        int blendmaskbl;
        double radmaskbl;
        double chromaskbl;
        double gammaskbl;
        double slomaskbl;
        double lapmaskbl;
        int shadmaskbl;
        int shadmaskblsha;
        double strumaskbl;
        std::vector<double> Lmaskblcurve;
        std::vector<double> LLmaskblcurvewav;
        Threshold<int> csthresholdblur;
        double denocontrast;
        bool denoAutocontrast;
        bool contrshow;
        bool lockmadl;
        bool madllock;
        bool enacontrast;
        double denoratio;
        double denomask;
        // Tone Mapping
        bool visitonemap;
        bool exptonemap;
        int complextonemap;
        double stren;
        double gamma;
        double estop;
        double scaltm;
        double repartm;
        int rewei;
        double satur;
        int sensitm;
        double softradiustm;
        double amount;
        bool equiltm;
        std::vector<double> CCmasktmcurve;
        std::vector<double> LLmasktmcurve;
        std::vector<double> HHmasktmcurve;
        bool enatmMask;
        bool enatmMaskaft;
        int blendmasktm;
        double radmasktm;
        double chromasktm;
        double gammasktm;
        double slomasktm;
        double lapmasktm;
        std::vector<double> Lmasktmcurve;
        double recothrest;
        double lowthrest;
        double higthrest;
        double decayt;
        // Retinex
        bool visireti;
        bool expreti;
        int complexreti;
        Glib::ustring retinexMethod; // low, uni, high
        double str;
        double chrrt;
        double neigh;
        double vart;
        double offs;
        int dehaz;
        int depth;
        int sensih;
        std::vector<double> localTgaincurve;
        std::vector<double> localTtranscurve;
        bool inversret;
        bool equilret;
        bool loglin;
        double dehazeSaturation;
        double dehazeblack;
        double softradiusret;
        std::vector<double> CCmaskreticurve;
        std::vector<double> LLmaskreticurve;
        std::vector<double> HHmaskreticurve;
        bool enaretiMask;
        bool enaretiMasktmap;
        int blendmaskreti;
        double radmaskreti;
        double chromaskreti;
        double gammaskreti;
        double slomaskreti;
        double lapmaskreti;
        double scalereti;
        double darkness;
        double lightnessreti;
        double limd;
        double cliptm;
        bool fftwreti;
        std::vector<double> Lmaskreticurve;
        double recothresr;
        double lowthresr;
        double higthresr;
        double decayr;
        // Sharpening
        bool visisharp;
        bool expsharp;
        int complexsharp;
        int sharcontrast;
        bool deconvAutoshar;
        double sharradius;
        int sharamount;
        int shardamping;
        int shariter;
        double sharblur;
        double shargam;
        int sensisha;
        bool inverssha;
        bool sharshow;
        bool itercheck;
        Glib::ustring methodcap;
        double capradius;
        bool deconvAutoRadius;
        double deconvCoBoost;                
        double deconvCoProt;                
        double deconvCoLat;                
        double deconvCogam;                
        double reparsha;                

        // Local Contrast
        bool visicontrast;
        bool expcontrast;
        int complexcontrast;
        int lcradius;
        double lcamount;
        double lcdarkness;
        double lclightness;
        double sigmalc;
        double offslc;
        double gradlc;
        int levelwav;
        double residcont;
        double residsha;
        double residshathr;
        double residhi;
        double residhithr;
        double gamlc;
        double residgam;
        double residslop;
        double residblur;
        double levelblur;
        double sigmabl;
        double residchro;
        double residcomp;
        double sigma;
        double offset;
        double sigmadr;
        double threswav;
        double chromalev;
        double chromablu;
        double sigmadc;
        double deltad;
        double fatres;
        double clarilres;
        double claricres;
        double clarisoft;
        double sigmalc2;
        double strwav;
        double angwav;
        double featherwav;
        double strengthw;
        double sigmaed;
        double radiusw;
        double detailw;
        double gradw;
        double tloww;
        double thigw;
        double edgw;
        double basew;
        int sensilc;
        double reparw;
        bool fftwlc;
        bool blurlc;
        bool wavblur;
        bool wavedg;
        bool waveshow;
        bool wavcont;
        bool wavcomp;
        bool wavgradl;
        bool wavcompre;
        bool origlc;
        bool processwav;
        bool limitwav;
        Glib::ustring localcontMethod; // loc, wav
        Glib::ustring localedgMethod; // fir, sec, thr
        Glib::ustring localneiMethod; // none, low, high
        std::vector<double> locwavcurve;
        Threshold<int> csthreshold;
        std::vector<double> loclevwavcurve;
        std::vector<double> locconwavcurve;
        std::vector<double> loccompwavcurve;
        std::vector<double> loccomprewavcurve;
        std::vector<double> locedgwavcurve;
        std::vector<double> CCmasklccurve;
        std::vector<double> LLmasklccurve;
        std::vector<double> HHmasklccurve;
        bool enalcMask;
        int blendmasklc;
        double radmasklc;
        double chromasklc;
        std::vector<double> Lmasklccurve;
        double recothresw;
        double lowthresw;
        double higthresw;
        double decayw;
        // Contrast by detail levels
        bool visicbdl;
        bool expcbdl;
        int complexcbdl;
        double mult[6];
        double chromacbdl;
        double threshold;
        int sensicb;
        double clarityml;
        int contresid;
        double softradiuscb;
        bool enacbMask;
        std::vector<double> CCmaskcbcurve;
        std::vector<double> LLmaskcbcurve;
        std::vector<double> HHmaskcbcurve;
        int blendmaskcb;
        double radmaskcb;
        double chromaskcb;
        double gammaskcb;
        double slomaskcb;
        double lapmaskcb;
        std::vector<double> Lmaskcbcurve;
        double recothrescb;
        double lowthrescb;
        double higthrescb;
        double decaycb;
        // Log encoding
        bool visilog;
        bool explog;
        int complexlog;
        bool autocompute;
        double sourceGray;
        double sourceabs;
        double targabs;
        double targetGray;
        double catad;
        double saturl;
        double chroml;
        double lightl;
        double lightq;
        double contl;
        double contthres;
        double contq;
        double colorfl;
        std::vector<double> LcurveL;
        bool Autogray;
        bool fullimage;
        double repar;
        bool ciecam;
        bool satlog;
        double blackEv;
        double whiteEv;
        int whiteslog;
        int blackslog;
        double comprlog;
        double strelog;
        double detail;
        int sensilog;
        Glib::ustring sursour;
        Glib::ustring surround;
        double baselog;
        double strlog;
        double anglog;
        double featherlog;
        std::vector<double> CCmaskcurveL;
        std::vector<double> LLmaskcurveL;
        std::vector<double> HHmaskcurveL;
        bool enaLMask;
        double blendmaskL;
        double radmaskL;
        double chromaskL;
        std::vector<double> LmaskcurveL;
        double recothresl;
        double lowthresl;
        double higthresl;
        double decayl;

        // mask
        bool visimask;
        int complexmask;
        bool expmask;
        int sensimask;
        double blendmask;
        double blendmaskab;
        double softradiusmask;
        bool enamask;
        bool fftmask;
        double blurmask;
        double contmask;
        std::vector<double> CCmask_curve;
        std::vector<double> LLmask_curve;
        std::vector<double> HHmask_curve;
        double strumaskmask;
        bool toolmask;
        double radmask;
        double lapmask;
        double chromask;
        double gammask;
        double slopmask;
        double shadmask;
        int str_mask;
        int ang_mask;
        int feather_mask;
        std::vector<double> HHhmask_curve;
        std::vector<double> Lmask_curve;
        std::vector<double> LLmask_curvewav;
        Threshold<int> csthresholdmask;
        //ciecam
        bool visicie;
        bool expcie;
        bool expprecam;
        int complexcie;
        double reparcie;
        int sensicie;
        double blurciede;
        bool Autograycie;
        bool sigybjz12;
        bool qtoj;
        bool jabcie;
        bool comprcieauto;
        bool normcie12;
        bool normcie;
        bool gamutcie;
        bool bwcie;
        bool sigcie;
        bool logcie;
        bool satcie;
        bool logcieq;
        bool smoothcie;
        bool smoothcietrc;
        bool smoothcietrcrel;
        bool smoothcieyb;
        bool smoothcielum;
        bool smoothciehigh;
        bool smoothcielnk;
        bool smoothcieinv;
        bool logjz;
        bool sigjz12;
        bool sigjz;
        bool forcebw;
        bool sigq12;
        bool sigq;
        bool chjzcie;
        double sourceGraycie;
        double sourceabscie;
        Glib::ustring sursourcie;
        Glib::ustring modecie;
        Glib::ustring modecam;
        Glib::ustring modeQJ;
        Glib::ustring bwevMethod12;
        Glib::ustring bwevMethod;
        double saturlcie;
        double rstprotectcie;
        double chromlcie;
        double huecie;
        
        double colorhred;
        double schromared;
        std::vector<double> redcurve;
        double colorhgreen;
        double schromagreen;
        std::vector<double> greencurve;
        double colorhblue;
        double schromablue;
        std::vector<double> bluecurve;
        double brighthres;

        Glib::ustring toneMethodcie;
        std::vector<double> ciecurve;
        Glib::ustring toneMethodcie2;
        std::vector<double> ciecurve2;
        double chromjzcie;
        double saturjzcie;
        double huejzcie;
        double softjzcie;
        double strsoftjzcie;
        double thrhjzcie;
        std::vector<double> jzcurve;
        std::vector<double> czcurve;
        std::vector<double> czjzcurve;
        std::vector<double> HHcurvejz;
        std::vector<double> CHcurvejz;
        std::vector<double> LHcurvejz;
        double lightlcie;
        double lightjzcie;
        double lightqcie;
        double lightsigqcie;
        double contlcie;
        double contjzcie;
        double detailciejz;
        double adapjzcie;
        double jz100;
        double pqremap;
        double pqremapcam16;
        double hljzcie;
        double hlthjzcie;
        double shjzcie;
        double shthjzcie;
        double radjzcie;
        double sigmalcjz;
        double clarilresjz;
        double claricresjz;
        double clarisoftjz;
        std::vector<double> locwavcurvejz;
        Threshold<int> csthresholdjz;
        double contthrescie;
        double blackEvjz;
        double whiteEvjz;
        double targetjz;
        double sigmoidldacie12;
        double sigmoidthcie12;
        double sigmoidblcie12;
        double sigmoidldacie;
        double sigmoidthcie;
        double sigmoidsenscie;
        double sigmoidblcie;
        double comprcie;
        double strcielog;
        double comprcieth;
        double gamjcie;
        double smoothcieth;
        double smoothciethtrc;
        double slopjcie;
        double satjcie;
        double smoothjcie;
        double contsig;
        double skewsig;
        double whitsig;
        double slopesmo;
        double slopesmoq;
        double slopesmor;
        double slopesmog;
        double slopesmob;
        double kslopesmor;
        double kslopesmog;
        double kslopesmob;
        std::vector<double> invcurve;//inverse color negative
        Glib::ustring midtciemet;
        int midtcie;
        double grexl;
        double greyl;
        double bluxl;
        double bluyl;
        double redxl;
        double redyl;
        double refi;
        double shiftxl;
        double shiftyl;
        double labgridcieALow;
        double labgridcieBLow;
        double labgridcieAHigh;
        double labgridcieBHigh;
        double labgridcieGx;
        double labgridcieGy;
        double labgridcieWx;
        double labgridcieWy;
        double labgridcieMx;
        double labgridcieMy;
        
        int whitescie;
        int blackscie;
        Glib::ustring illMethod;
        Glib::ustring smoothciemet;
        Glib::ustring primMethod;
        Glib::ustring catMethod;
        double sigmoidldajzcie12;
        double sigmoidthjzcie12;
        double sigmoidbljzcie12;
        double sigmoidldajzcie;
        double sigmoidthjzcie;
        double sigmoidbljzcie;
        double contqcie;
        double contsigqcie;
        double colorflcie;
        double targabscie;
        double targetGraycie;
        double catadcie;
        double detailcie;
        Glib::ustring surroundcie;
        double gamgain;
        double gampower;
        Glib::ustring gamutw;
        double strgradcie;
        double anggradcie;
        double feathercie;
        bool enacieMask;
        bool enacieMaskall;
        std::vector<double> CCmaskciecurve;
        std::vector<double> LLmaskciecurve;
        std::vector<double> HHmaskciecurve;
        std::vector<double> HHhmaskciecurve;
        int blendmaskcie;
        double radmaskcie;
        double chromaskcie;
        double lapmaskcie;
        double gammaskcie;
        double slomaskcie;
        std::vector<double> Lmaskciecurve;
        double recothrescie;
        double lowthrescie;
        double higthrescie;
        double decaycie;
        double strumaskcie;
		bool toolcie;
        bool fftcieMask;
		double contcie;
		double blurcie;
		double highmaskcie;
		double shadmaskcie;
        std::vector<double> LLmaskciecurvewav;
        Threshold<int> csthresholdcie;
		
        LocallabSpot();

        bool operator ==(const LocallabSpot& other) const;
        bool operator !=(const LocallabSpot& other) const { return !(*this == other); }
    };

    static const double LABGRIDL_CORR_MAX;
    static const double LABGRIDL_CORR_SCALE;
    static const double LABGRIDL_DIRECT_SCALE;

    bool enabled;
    int selspot;
    std::vector<LocallabSpot> spots;

    LocallabParams();

    bool operator ==(const LocallabParams& other) const;
    bool operator !=(const LocallabParams& other) const { return !(*this == other); }
};

void loadLocalLabParams(const Glib::KeyFile& keyFile, LocallabParams& locallab,
                        ParamsEdited* pedited, int ppVersion);

void saveLocalLabParams(Glib::KeyFile& keyFile, const LocallabParams& locallab,
                        const ParamsEdited* pedited);

}  // namespace procparams
}  // namespace rtengine
