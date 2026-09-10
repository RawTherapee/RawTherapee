/*
 *  This file is part of RawTherapee.
 */
#pragma once

#include <gtkmm.h>

#include "colorprovider.h"
#include "curvelistener.h"
#include "guiutils.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/smartscroll.h"
#include "widgets/basic/thresholdadjuster.h"

#include "rtengine/procparams.h"

class CurveEditor;
class CurveEditorGroup;
class DiagonalCurveEditor;
class FlatCurveEditor;
class LabGrid;

class ColorToning final :
    public ToolParamBlock,
    public FoldableToolPanel,
    public rtengine::AutoColorTonListener,
    public CurveListener,
    public ColorProvider,
    public ThresholdAdjusterListener,
    public AdjusterListener
{
public:
    static const Glib::ustring TOOL_NAME;

    ColorToning ();
    ~ColorToning() override;
    void read                  (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write                 (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setBatchMode          (bool batchMode) override;
    void setDefaults           (const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void trimValues            (rtengine::procparams::ProcParams* pp) override;
    void adjusterChanged       (Adjuster* a, double newval) override;
    void setAdjusterBehavior   (bool splitAdd, bool satThresholdAdd, bool satOpacityAdd, bool strprotectAdd, bool balanceAdd);
    void neutral_pressed       ();
    //void neutralCurves_pressed ();
    void autoColorTonChanged   (int bwct, int satthres, int satprot) override;
    bool CTComp_               ();

    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override;
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override;
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override;
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;

    void enabledChanged        () override;
    void curveChanged          (CurveEditor* ce) override;
    void autosatChanged        ();
    void autoOpenCurve         () override;
    void methodChanged         ();
    void twocolorChanged       (bool changedbymethod);
    void twoColorChangedByGui  ();
    void lumamodeChanged       ();

    void colorForValue         (double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller* caller) override;

    void setListener(ToolPanelListener *tpl) override;

    void setEditProvider(EditDataProvider *provider) override;
    float blendPipetteValues(CurveEditor *ce, float chan1, float chan2, float chan3) override;

private:
    void onLabRegionSelectionChanged();
    void labRegionAddPressed();
    void labRegionRemovePressed();
    void labRegionUpPressed();
    void labRegionDownPressed();
    void labRegionCopyPressed();
    void labRegionShowMaskChanged();
    void labRegionChannelChanged();
    void labRegionPopulateList();
    void labRegionShow(int idx, bool list_only=false);
    void labRegionGet(int idx);

    //Gtk::Separator* satLimiterSep;
    Gtk::Separator* colorSep;
    CurveEditorGroup* colorCurveEditorG;
    CurveEditorGroup* opacityCurveEditorG;
    CurveEditorGroup* clCurveEditorG;
    CurveEditorGroup* cl2CurveEditorG;
    FlatCurveEditor* opacityShape;
    FlatCurveEditor* colorShape;
    DiagonalCurveEditor* clshape;
    DiagonalCurveEditor* cl2shape;
    Gtk::Box* ctbox;
    Gtk::Frame *p1Frame;

    Gtk::Box* chanMixerBox;
    MyComboBoxText* method;
    sigc::connection methodconn;
    MyComboBoxText* twocolor;
    Adjuster* redlow;
    Adjuster* greenlow;
    Adjuster* bluelow;
    Adjuster* redmed;
    Adjuster* greenmed;
    Adjuster* bluemed;
    Adjuster* redhigh;
    Adjuster* greenhigh;
    Adjuster* bluehigh;
    Adjuster* balance;
    Gtk::CheckButton* autosat;
    ThresholdAdjuster* shadowsColSat;
    ThresholdAdjuster* hlColSat;
    Adjuster* satProtectionThreshold;
    Adjuster* saturatedOpacity;
    Adjuster* strength;
    Gtk::Image* iby;
    Gtk::Image* irg;

    Gtk::Button* neutral;
    Gtk::Box* neutrHBox;
    int nextbw;
    int nextsatth;
    int nextsatpr;
    Glib::ustring nextbalcolor;
    Glib::ustring balcolor;
    sigc::connection neutralconn, twocconn; //, neutralcurvesconn;
    bool lastautosat;
    sigc::connection autosatConn;

    Gtk::CheckButton* lumamode;
    bool lastLumamode;
    sigc::connection lumamodeConn;

    rtengine::ProcEvent EvColorToningLabGridValue;
    LabGrid *labgrid;

    rtengine::ProcEvent EvLabRegionList;
    rtengine::ProcEvent EvLabRegionAB;
    rtengine::ProcEvent EvLabRegionSaturation;
    rtengine::ProcEvent EvLabRegionLightness;
    rtengine::ProcEvent EvLabRegionSlope;
    rtengine::ProcEvent EvLabRegionOffset;
    rtengine::ProcEvent EvLabRegionPower;    
    rtengine::ProcEvent EvLabRegionHueMask;
    rtengine::ProcEvent EvLabRegionChromaticityMask;
    rtengine::ProcEvent EvLabRegionLightnessMask;
    rtengine::ProcEvent EvLabRegionMaskBlur;
    rtengine::ProcEvent EvLabRegionShowMask;
    rtengine::ProcEvent EvLabRegionChannel;

    Gtk::Box* labRegionBox;
    Gtk::ListViewText *labRegionList;
    Gtk::Button *labRegionAdd;
    Gtk::Button *labRegionRemove;
    Gtk::Button *labRegionUp;
    Gtk::Button *labRegionDown;
    Gtk::Button *labRegionCopy;
    LabGrid *labRegionAB;
    Adjuster *labRegionSaturation;
    Adjuster *labRegionSlope;
    Adjuster *labRegionOffset;
    Adjuster *labRegionPower;
    MyComboBoxText *labRegionChannel;
    FlatCurveEditor *labRegionHueMask;
    FlatCurveEditor *labRegionChromaticityMask;
    FlatCurveEditor *labRegionLightnessMask;
    Adjuster *labRegionMaskBlur;
    Gtk::CheckButton *labRegionShowMask;
    std::vector<rtengine::procparams::ColorToningParams::LabCorrectionRegion> labRegionData;
    int labRegionSelected;
    sigc::connection labRegionSelectionConn;

    IdleRegister idle_register;
};
