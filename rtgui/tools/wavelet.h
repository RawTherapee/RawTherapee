/*
 *  This file is part of RawTherapee.
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
 *
 *  2014 2020 Jacques Desmis <jdesmis@gmail.com>
 */

#pragma once

#include <gtkmm.h>

#include "colorprovider.h"
#include "curvelistener.h"
#include "guiutils.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/thresholdadjuster.h"

class CurveEditor;
class CurveEditorGroup;
class DiagonalCurveEditor;
class EditDataProvider;
class FlatCurveEditor;
class LabGrid;
class MyExpander;

class Wavelet final :
    public ToolParamBlock,
    public ThresholdAdjusterListener,
    public AdjusterListener,
    public CurveListener,
    public ColorProvider,
    public rtengine::WaveletListener,
    public FoldableToolPanel
{
public:
    static const Glib::ustring TOOL_NAME;

    Wavelet();
    ~Wavelet() override;
    bool wavComputed_();
    void adjusterChanged(Adjuster* a, double newval) override;
    void autoOpenCurve() override;
    void curveChanged(CurveEditor* ce) override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
//    void setAdjusterBehavior(bool multiplieradd, bool thresholdadd, bool threshold2add, bool thresadd, bool chroadd, bool chromaadd, bool contrastadd, bool skinadd, bool reschroadd, bool tmrsadd, bool resconadd, bool resconHadd, bool thradd, bool thrHadd, bool skyadd, bool edgradadd, bool edgvaladd, bool strengthadd, bool gammaadd, bool edgedetectadd, bool edgedetectthradd, bool edgedetectthr2add);
    void setAdjusterBehavior (bool multiplieradd, bool thresholdadd, bool threshold2add, bool thresadd, bool chroadd, bool chromaadd, bool contrastadd, bool skinadd, bool reschroadd, bool tmrsadd, bool edgsadd, bool scaleadd, bool resconadd, bool resconHadd, bool thradd, bool thrHadd, bool radiusadd, bool skyadd, bool edgradadd, bool edgvaladd, bool strengthadd, bool gammaadd, bool edgedetectadd, bool edgedetectthradd, bool edgedetectthr2add);
    void setBatchMode(bool batchMode) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void setEditProvider(EditDataProvider *provider) override;
    void updateToolState(const std::vector<int>& tpOpen);
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void writeOptions(std::vector<int> &tpOpen);

    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override;
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override;
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override;
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;

private:
    rtengine::ProcEvent EvWavenaclari;
    rtengine::ProcEvent EvWavushamet;
    rtengine::ProcEvent EvWavballum;
    rtengine::ProcEvent EvWavbalchrom;
    rtengine::ProcEvent EvWavchromfi;
    rtengine::ProcEvent EvWavchromco;
    rtengine::ProcEvent EvWavmergeL;
    rtengine::ProcEvent EvWavmergeC;
    rtengine::ProcEvent EvWavsoftrad;
    rtengine::ProcEvent EvWavsoftradend;
    rtengine::ProcEvent EvWavshowmask;
    rtengine::ProcEvent EvWavedgs;
    rtengine::ProcEvent EvWavscale;
    rtengine::ProcEvent EvWavradius;
    rtengine::ProcEvent EvWavsigma;
    rtengine::ProcEvent EvWavenabl;
    rtengine::ProcEvent EvWavchrwav;
    rtengine::ProcEvent EvWavoldsh;
    rtengine::ProcEvent EvWavoffset;
    rtengine::ProcEvent EvWavlowthr;
    rtengine::ProcEvent EvWavbluwav;
    rtengine::ProcEvent EvWavblshape;
    rtengine::ProcEvent EvWavresblur;
    rtengine::ProcEvent EvWavresblurc;
    rtengine::ProcEvent EvWavedgeffect;
    rtengine::ProcEvent EvWavsigmafin;
    rtengine::ProcEvent EvWavsigmaton;
    rtengine::ProcEvent EvWavsigmacol;
    rtengine::ProcEvent EvWavsigmadir;
    rtengine::ProcEvent EvWavLabGridValue;
    rtengine::ProcEvent EvWavrangeab;
    rtengine::ProcEvent EvWavprotab;
    rtengine::ProcEvent EvWavlevelshc;
    rtengine::ProcEvent EvWavcomplexmet;
    rtengine::ProcEvent EvWavsigm;
    rtengine::ProcEvent EvWavdenoise;
    rtengine::ProcEvent EvWavdenmethod;
    rtengine::ProcEvent EvWavmixmethod;
    rtengine::ProcEvent EvWavquamethod;
    rtengine::ProcEvent EvWavlevden;
    rtengine::ProcEvent EvWavdenoiseh;
    rtengine::ProcEvent EvWavstrend;
    rtengine::ProcEvent EvWavdetend;
    rtengine::ProcEvent EvWavlevdenois;
    rtengine::ProcEvent EvWavslimethod;
    rtengine::ProcEvent EvWavthrend;
    rtengine::ProcEvent EvWavguid;
    rtengine::ProcEvent EvWavhue;
    rtengine::ProcEvent EvWavthrden;
    rtengine::ProcEvent EvWavlevelsigm;
    rtengine::ProcEvent EvWavlimden;

    LabGrid *labgrid;

    void foldAllButMe(GdkEventButton* event, MyExpander *expander);
    void setListener(ToolPanelListener *tpl) override;

    void colorForValue(double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller* caller) override;
    void BAmethodChanged();
    void NPmethodChanged();
    void BackmethodChanged();
    void CHSLmethodChanged();
    void CHmethodChanged();
    void CLmethodChanged();
    void DirmethodChanged();
    void EDmethodChanged();
    void HSmethodChanged();
    void LmethodChanged();
    void MedgreinfChanged();
    void TMmethodChanged();
    void complexmethodChanged();
    //void denmethodChanged();
    void mixmethodChanged();
    void quamethodChanged();
    void slimethodChanged();
    void TilesmethodChanged();
    void avoidToggled();
    void showmaskToggled ();
    void oldshToggled ();
    void cbenabToggled();
    void contrastMinusPressed();
    void contrastPlusPressed();
    void daubcoeffmethodChanged();
    void enabledChanged() override;
    void linkedgToggled();
    void lipstToggled();
    void medianToggled();
    void medianlevToggled();
    void neutralPressed();
    void neutral_pressed();
    void neutralchPressed();
    void tmrToggled();
    void updatewavLabel ();
    void wavChanged(double nlevel) override;
    void ushamethodChanged();
    void updateGUI();
    void updateGUImaxlev();
    void convertParamToNormal();
    void updateGUIToMode(int mode);
    void HSmethodUpdateUI();
    void CHmethodUpdateUI();
//  void CHSLmethodChangedUI();
    void EDmethodUpdateUI();
    void NPmethodUpdateUI();
    void BAmethodUpdateUI();
    void TMmethodUpdateUI();
//  void BackmethodUpdateUI();
    void CLmethodUpdateUI();
//  void TilesmethodUpdateUI();
//  void daubcoeffmethodUpdateUI();
//  void MedgreinfUpdateUI();
//  void DirmethodUpdateUI();
//  void LmethodUpdateUI();
    void adjusterUpdateUI(Adjuster* a);
    void enabledUpdateUI();
    void medianlevUpdateUI();
    void cbenabUpdateUI();
    void lipstUpdateUI();

    void enableToggled(MyExpander* expander);

    CurveEditorGroup* const curveEditorG;
    //CurveEditorGroup* const curveEditorC;
    //FlatCurveEditor* opacityShapeSH;

    CurveEditorGroup* const CCWcurveEditorG;
    CurveEditorGroup* const curveEditorbl;
    CurveEditorGroup* const curveEditorRES;
    CurveEditorGroup* const curveEditorGAM;
    Gtk::Separator* const separatorNeutral;
    Gtk::Separator* const separatoredge;

    CurveEditorGroup* const opaCurveEditorG;
    FlatCurveEditor* opacityShapeRG;
    CurveEditorGroup* const opacityCurveEditorG;
    FlatCurveEditor* opacityShapeBY;
    CurveEditorGroup* const CurveEditorwavnoise;
    FlatCurveEditor* wavdenoise;
    CurveEditorGroup* const CurveEditorwavnoiseh;
    FlatCurveEditor* wavdenoiseh;
    CurveEditorGroup* const CurveEditorwavguid;
    FlatCurveEditor* wavguidf;
    CurveEditorGroup* const CurveEditorwavhue;
    FlatCurveEditor* wavhue;
    CurveEditorGroup* const opacityCurveEditorW;
    CurveEditorGroup* const opacityCurveEditorWL;
    FlatCurveEditor* opacityShape;
    FlatCurveEditor* opacityShapeWL;
    FlatCurveEditor* hhshape;
    FlatCurveEditor* Chshape;
    DiagonalCurveEditor* clshape;

    FlatCurveEditor* ccshape;
    FlatCurveEditor* blshape;
    Gtk::CheckButton* const median;
    Gtk::CheckButton* const medianlev;
    Gtk::CheckButton* const linkedg;
    Gtk::CheckButton* const cbenab;
    Gtk::CheckButton* const lipst;
    Gtk::CheckButton* const avoid;
    Gtk::CheckButton* const tmr;
    Gtk::CheckButton* const showmask;
    Gtk::CheckButton* const oldsh;

    Gtk::Button* const neutralchButton;
    Adjuster* correction[9];
    Adjuster* correctionch[9];
    Adjuster* const sigma;
    Adjuster* const offset;
    Adjuster* const lowthr;
    Adjuster* const rescon;
    Adjuster* const resconH;
    Adjuster* const reschro;
    Adjuster* const resblur;
    Adjuster* const resblurc;
    Adjuster* const bluwav;
    Adjuster* const tmrs;
    Adjuster* const edgs;
    Adjuster* const scale;
    Adjuster* const gamma;
    Adjuster* const sup;
    Adjuster* const sky;
    Adjuster* const thres;
    Adjuster* const chroma;
    Adjuster* const chro;
    Adjuster* const contrast;
    Adjuster* const thr;
    Adjuster* const thrH;
    Adjuster* const radius;
    Adjuster* const skinprotect;
    Adjuster* const edgrad;
    Adjuster* const edgeffect;
    Adjuster* const edgval;
    Adjuster* const edgthresh;
    Adjuster* const strength;
    Adjuster* const balance;
    Adjuster* const iter;
    Adjuster* const sigmafin;
    Adjuster* const sigmaton;
    Adjuster* const sigmacol;
    Adjuster* const sigmadir;
    Adjuster* const rangeab;
    Adjuster* const protab;

    Adjuster* greenlow;
    Adjuster* bluelow;
    Adjuster* greenmed;
    Adjuster* bluemed;
    Adjuster* greenhigh;
    Adjuster* bluehigh;

    ThresholdAdjuster* const hueskin;
    ThresholdAdjuster* const hueskin2;
    ThresholdAdjuster* const hllev;
    ThresholdAdjuster* const bllev;
    ThresholdAdjuster* const pastlev;
    ThresholdAdjuster* const satlev;
    ThresholdAdjuster* const edgcont;
    ThresholdAdjuster* const level0noise;
    ThresholdAdjuster* const level1noise;
    ThresholdAdjuster* const level2noise;
    ThresholdAdjuster* const level3noise;
    ThresholdAdjuster* const leveldenoise;
    ThresholdAdjuster* const levelsigm;

    Adjuster* const sigm;
    Adjuster* const levden;
    Adjuster* const thrden;
    Adjuster* const limden;
    Adjuster* const threshold;
    Adjuster* const threshold2;
    Adjuster* const edgedetect;
    Adjuster* const edgedetectthr;
    Adjuster* const edgedetectthr2;
    Adjuster* const edgesensi;
    Adjuster* const edgeampli;
    Adjuster* const ballum;
    Adjuster* const balchrom;
    Adjuster* const chromfi;
    Adjuster* const chromco;
    Adjuster* const mergeL;
    Adjuster* const mergeC;
    Adjuster* const softrad;
    Adjuster* const softradend;
    Adjuster* const strend;
    Adjuster* const detend;
    Adjuster* const thrend;
    Adjuster* const chrwav;

    MyComboBoxText* const Lmethod;
    sigc::connection  Lmethodconn;
    MyComboBoxText* const CHmethod;
    sigc::connection  CHmethodconn;
    MyComboBoxText* const CHSLmethod;
    sigc::connection  CHSLmethodconn;
    MyComboBoxText* const EDmethod;
    sigc::connection  EDmethodconn;
    MyComboBoxText* const BAmethod;
    sigc::connection  BAmethodconn;
    MyComboBoxText* const NPmethod;
    sigc::connection  NPmethodconn;
    MyComboBoxText* const TMmethod;
    sigc::connection  TMmethodconn;
    MyComboBoxText* const HSmethod;
    sigc::connection  HSmethodconn;
    MyComboBoxText* const CLmethod;
    sigc::connection  CLmethodconn;
    MyComboBoxText* const Backmethod;
    sigc::connection  Backmethodconn;
    MyComboBoxText* const complexmethod;
    sigc::connection  complexmethodconn;
    MyComboBoxText* const Tilesmethod;
    sigc::connection  Tilesmethodconn;
    MyComboBoxText* const daubcoeffmethod;
    sigc::connection  daubcoeffmethodconn;
    MyComboBoxText* const Dirmethod;
    sigc::connection  Dirmethodconn;
    MyComboBoxText* const Medgreinf;
    sigc::connection  MedgreinfConn;
    MyComboBoxText* const ushamethod;
    sigc::connection  ushamethodconn;
    //MyComboBoxText* const denmethod;
    //sigc::connection  denmethodconn;
    MyComboBoxText* const mixmethod;
    sigc::connection  mixmethodconn;
    MyComboBoxText* const quamethod;
    sigc::connection  quamethodconn;
    MyComboBoxText* const slimethod;
    sigc::connection  slimethodconn;

    Gtk::Frame* const chanMixerHLFrame;
    Gtk::Frame* const chanMixerMidFrame;
    Gtk::Frame* const chanMixerShadowsFrame;
    Gtk::Frame* const shFrame;
    Gtk::Frame* const contFrame;
    Gtk::Frame* const blurFrame;
    Gtk::Frame* const chromaFrame;
    Gtk::Frame* const chroFrame;
    Gtk::Frame* const fincFrame;
    Gtk::Frame* const dirFrame;
    Gtk::Frame* const tonFrame;
    Gtk::Frame* const guidFrame;

    Gtk::Label* const wavLabels;
    Gtk::Label* const labmC;
    Gtk::Label* const labmNP;
    Gtk::Label* const usharpLabel;
    MyExpander* const expchroma;
    MyExpander* const expcontrast;
    MyExpander* const expedge;
    MyExpander* const expfinal;
    MyExpander* const expgamut;
    MyExpander* const expnoise;
    MyExpander* const expresid;
    MyExpander* const expsettings;
    MyExpander* const exptoning;
    MyExpander* const expclari;
    MyExpander* const expbl;

    Gtk::Box* const neutrHBox;
    Gtk::Box* const usharpHBox;
    Gtk::Box* const ctboxch;
    Gtk::Box* const quaHBox;
    Gtk::Box* const sliHBox;
    //Gtk::Box* const denHBox;
    Gtk::Box* const mixHBox;
    Gtk::Box* const ctboxBA;// = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    sigc::connection enableChromaConn, enableContrastConn, enableEdgeConn, enabletmConn, enableFinalConn, enableclariConn;
    sigc::connection enableNoiseConn, enableResidConn, enableToningConn;
    sigc::connection medianConn, avoidConn, tmrConn, medianlevConn, linkedgConn, lipstConn, cbenabConn, neutralconn, showmaskConn, oldshConn;
    sigc::connection neutralPressedConn;
    sigc::connection contrastPlusPressedConn;
    sigc::connection contrastMinusPressedConn;
    sigc::connection neutralchPressedConn;

    bool lastmedian, lastmedianlev, lastlinkedg, lastavoid, lastlipst, lasttmr, lastcbenab, lastshowmask, lastoldsh;
    int nextnlevel;

    IdleRegister idle_register;
};
