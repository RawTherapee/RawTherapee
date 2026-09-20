/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>frame
 *
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
 *  2019-2020 Pierre Cabrera <pierre.cab@gmail.com>
 */
#ifndef _LOCALLABTOOLS_H_
#define _LOCALLABTOOLS_H_

#include "labgrid.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/thresholdadjuster.h"
#include "widgets/curves/curveeditorgroup.h"
#include "widgets/curves/curveeditor.h"

/* ==== LocallabToolListener ==== */
class LocallabTool;
class LocallabToolListener
{
public:
    LocallabToolListener() {};
    virtual ~LocallabToolListener() {};

    virtual void resetOtherMaskView(LocallabTool* current) = 0;
    virtual void toolRemoved(LocallabTool* current) = 0;
};


/* ==== LocallabTool ==== */
class LocallabTool:
    public ToolPanel,
    public CurveListener,
    public ColorProvider,
    public AdjusterListener
{
protected:
    // LocallabTool mode enumeration
    enum modeType {
        Expert = 0,
        Normal = 1,
        Simple = 2
    };
    rtengine::ProcEvent Evlocallabpreviewcol;
    rtengine::ProcEvent Evlocallabpreviewexe;
    rtengine::ProcEvent Evlocallabpreviewsh;
    rtengine::ProcEvent Evlocallabpreviewvib;
    rtengine::ProcEvent Evlocallabpreviewtm;
    rtengine::ProcEvent Evlocallabpreviewlc;
    rtengine::ProcEvent Evlocallabpreviewlog;
    rtengine::ProcEvent Evlocallabpreviewcie;
    rtengine::ProcEvent Evlocallabpreviewmas;
    rtengine::ProcEvent Evlocallabnormcie12;
    rtengine::ProcEvent Evlocallabnormcie;
    rtengine::ProcEvent Evlocallabstrumaskcie;
    rtengine::ProcEvent EvLocallabtoolcie;
    rtengine::ProcEvent EvLocallabfftcieMask;
    rtengine::ProcEvent Evlocallabcontcie;
    rtengine::ProcEvent Evlocallabblurcie;
    rtengine::ProcEvent Evlocallabhighmaskcie;
    rtengine::ProcEvent Evlocallabshadmaskcie;
    rtengine::ProcEvent EvlocallabLLmaskcieshapewav;
    rtengine::ProcEvent EvlocallabcsThresholdcie;
    rtengine::ProcEvent Evlocallabcomprcie;
    rtengine::ProcEvent Evlocallabstrcielog;
    rtengine::ProcEvent Evlocallabsatcie;
    rtengine::ProcEvent Evlocallablogcieq;
    rtengine::ProcEvent Evlocallabcomprcieth;
    rtengine::ProcEvent EvlocallabHHhmaskcieshape;
    rtengine::ProcEvent EvlocallabbwevMethod12;
    rtengine::ProcEvent Evlocallabgamjcie;
    rtengine::ProcEvent Evlocallabslopjcie;
    rtengine::ProcEvent Evlocallabsatjcie;
    rtengine::ProcEvent Evlocallabmidtciemet;
    rtengine::ProcEvent Evlocallabmidtcie;
    rtengine::ProcEvent Evlocallabcontsig;
    rtengine::ProcEvent Evlocallabskewsig;
    rtengine::ProcEvent Evlocallabwhitsig;
    rtengine::ProcEvent Evlocallabslopesmo;
    rtengine::ProcEvent Evlocallabslopesmoq;
    rtengine::ProcEvent Evlocallabslopesmor;
    rtengine::ProcEvent Evlocallabslopesmog;
    rtengine::ProcEvent Evlocallabslopesmob;
    rtengine::ProcEvent Evlocallabkslopesmor;
    rtengine::ProcEvent Evlocallabkslopesmog;
    rtengine::ProcEvent Evlocallabkslopesmob;
    rtengine::ProcEvent Evlocallabsmoothcie;
    rtengine::ProcEvent Evlocallabsmoothcielnk;
    rtengine::ProcEvent Evlocallabsmoothcieinv;
    rtengine::ProcEvent Evlocallabsmoothcieth;
    rtengine::ProcEvent Evlocallabsmoothciethtrc;
    rtengine::ProcEvent Evlocallabsmoothcietrc;
    rtengine::ProcEvent Evlocallabsmoothcietrcrel;
    rtengine::ProcEvent Evlocallabsmoothcieyb;
    rtengine::ProcEvent Evlocallabsmoothcielum;
    rtengine::ProcEvent Evlocallabsmoothciehigh;
    rtengine::ProcEvent Evlocallabsmoothciemet;
    rtengine::ProcEvent Evlocallabsigcie;
    rtengine::ProcEvent Evlocallabillcie;
    rtengine::ProcEvent Evlocallabprimcie;
    rtengine::ProcEvent Evlocallabcatcie;
    rtengine::ProcEvent Evlocallabwhitescie;
    rtengine::ProcEvent Evlocallabblackscie;
    rtengine::ProcEvent Evlocallabwhiteslog;
    rtengine::ProcEvent Evlocallabblackslog;
    rtengine::ProcEvent Evlocallabcomprlog;
    rtengine::ProcEvent Evlocallabsatlog;
    rtengine::ProcEvent Evlocallabstrelog;
    rtengine::ProcEvent Evlocallabredxl;
    rtengine::ProcEvent Evlocallabredyl;
    rtengine::ProcEvent Evlocallabgrexl;
    rtengine::ProcEvent Evlocallabgreyl;
    rtengine::ProcEvent Evlocallabbluxl;
    rtengine::ProcEvent Evlocallabbluyl;
    rtengine::ProcEvent EvlocallabGridciexy;
    rtengine::ProcEvent EvlocallabGridghs;
    rtengine::ProcEvent Evlocallabgamutcie;
    rtengine::ProcEvent Evlocallabbwcie;
    rtengine::ProcEvent Evlocallabexpprecam;
    rtengine::ProcEvent Evlocallablightsigqcie12;
    rtengine::ProcEvent Evlocallabcontsigqcie;
    rtengine::ProcEvent Evlocallabrefi;
    rtengine::ProcEvent Evlocallabshiftxl;
    rtengine::ProcEvent Evlocallabshiftyl;
    rtengine::ProcEvent Evlocallabanggradcie;
    rtengine::ProcEvent Evlocallabstrgradcie;
    rtengine::ProcEvent Evlocallabdetailciejz;
    rtengine::ProcEvent EvlocallabenacieMaskall;
    rtengine::ProcEvent Evlocallabfeathercol;
    rtengine::ProcEvent Evlocallabfeathervib;
    rtengine::ProcEvent Evlocallabfeatherexp;
    rtengine::ProcEvent Evlocallabfeatherwav;
    rtengine::ProcEvent Evlocallabfeatherlog;
    rtengine::ProcEvent Evlocallabfeathercie;
    rtengine::ProcEvent EvlocallabfeatherSH;
    rtengine::ProcEvent Evlocallabfeather_mask;
    rtengine::ProcEvent Evlocallaboffslc;
    rtengine::ProcEvent EvlocallabmodeQJ;
    rtengine::ProcEvent EvlocallabbwevMethod;   
    rtengine::ProcEvent Evlocallabsigmoidldacie;
    rtengine::ProcEvent Evlocallabsigmoidthcie; 
    rtengine::ProcEvent Evlocallabsigmoidblcie; 
    rtengine::ProcEvent Evlocallabsigmoidsenscie;
    rtengine::ProcEvent Evlocallabsigq;
    rtengine::ProcEvent Evlocallabsigq_12;
    rtengine::ProcEvent Evlocallabsigjz;
    rtengine::ProcEvent Evlocallabforcebw;

    rtengine::ProcEvent Evlocallabsigmoidldajzcie;
    rtengine::ProcEvent Evlocallabsigmoidthjzcie;
    rtengine::ProcEvent Evlocallabsigmoidbljzcie;
    rtengine::ProcEvent Evlocallablogcie_12;
    // LocallabTool parameters
    bool needMode;
    bool isLocActivated;
    const Glib::ustring *spotNameSource;
    LocallabToolListener* locToolListener;

    // LocallabTool generic widgets
    MyExpander* exp;
    MyComboBoxText* const complexity;

    sigc::connection enaExpConn, complexityConn;

    IdleRegister idle_register;

    Glib::ustring getSpotName() const;

public:
    // Locallab tool constructor/destructor
    LocallabTool(Gtk::Box* content, Glib::ustring toolName, Glib::ustring UILabel, bool need11 = false, bool needMode = true);
    virtual ~LocallabTool();

    // Getter for Locallab tool expander
    MyExpander* getExpander() override
    {
        return exp;
    }

    // Getter/setter for Locallab tool expanded status
    void setExpanded(bool expanded) override
    {
        exp->set_expanded(expanded);
    }

    bool getExpanded() override
    {
        return exp->get_expanded();
    }

    // Setter for Locallab activation indicator
    void isLocallabActivated(bool cond)
    {
        isLocActivated = cond;
    }

    // Setter for spot name source
    void setSpotNameSource(const Glib::ustring *source)
    {
        spotNameSource = source;
    }

    // Setter for Locallab tool listener
    void setLocallabToolListener(LocallabToolListener* ltl)
    {
        locToolListener = ltl;
    }

    // Management functions to add/remove Locallab tool
    void addLocallabTool(bool raiseEvent);
    void removeLocallabTool(bool raiseEvent);
    bool isLocallabToolAdded();

    // Mask background management function
    void refChanged(const double huer, const double lumar, const double chromar, const float fab);

    // Mask preview functions
    virtual bool isMaskViewActive()
    {
        return false;
    };
    virtual void resetMaskView() {};
    virtual void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) {};

    virtual Gtk::ToggleButton *getPreviewDeltaEButton() const;
    virtual sigc::connection *getPreviewDeltaEButtonConnection();

    // Advice tooltips management function
    virtual void updateAdviceTooltips(const bool showTooltips) {};

    /* Notes:
     - callerId #1: Mask CC shape (bottom bar) + Color CC/LC shape (left bar)
     - callerId #2: Mask HH shape (main curve and bottom bar)
     - callerId #3: Color LH/HH shape (main curve)
     - callerId #4: Color CC/LC shape (bottom bar)
    */
    void colorForValue(double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller* caller) override;

    // To be implemented
    virtual void setDefaultExpanderVisibility() {};
    virtual void disableListener();
    virtual void enableListener();
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override {};
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override {};
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override {};
    void adjusterChanged(Adjuster* a, double newval) override {};
    void curveChanged(CurveEditor* ce) override {};
    virtual void adjusterAutoToggled(Adjuster* a, bool newval){};

protected:
    // To be implemented
    virtual void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) {}; // Only necessary when using mask

private:
    // Remove button event function
    bool on_remove_change(GdkEventButton* event);

    // Tool expander event function
    void foldThemAll(GdkEventButton* event);

    // Complexity mode event function
    void complexityModeChanged();

    // To be implemented
    virtual void enabledChanged() {};
    virtual void convertParamToNormal() {}; // From Expert mode to Normal mode; Only necessary when using mode
    virtual void convertParamToSimple() {}; // From Normal mode to Simple mode; Only necessary when using mode
    virtual void updateGUIToMode(const modeType new_type) {}; // Only necessary when using mode
 //   virtual void adjusterAutoToggled(Adjuster* a, bool newval) {};

};

/* ==== LocallabColor ==== */
class LocallabColor:
    public Gtk::Box,
    public LocallabTool,
    public ThresholdAdjusterListener
{
private:
    // Color & Light specific widgets
    Gtk::Frame* const lumFrame;
    Adjuster* const reparcol;
    Adjuster* const gamc;
    Adjuster* const lightness;
    Adjuster* const contrast;
    Adjuster* const chroma;
    Gtk::CheckButton* const curvactiv;
    Gtk::Frame* const gridFrame;
    LabGrid* const labgrid;
    MyComboBoxText* const gridMethod;
    Adjuster* const strengthgrid;
    Adjuster* const sensi;
    Gtk::ToggleButton* const previewcol;
    
    Adjuster* const structcol;
    Adjuster* const blurcolde;
    Adjuster* const softradiuscol;
    MyExpander* const exprecov;
    Gtk::Label* const maskusablec;
    Gtk::Label* const maskunusablec;
    Adjuster* const recothresc;
    Adjuster* const lowthresc;
    Adjuster* const higthresc;
    Adjuster* const decayc;
    Gtk::CheckButton* const invers;
    MyExpander* const expgradcol;
    Adjuster* const strcol;
    Adjuster* const strcolab;
    Adjuster* const strcolh;
    Adjuster* const angcol;
    Adjuster* const feathercol;
    MyExpander* const expcurvcol;
    Gtk::Label* const labqualcurv;
    MyComboBoxText* const qualitycurveMethod;
    CurveEditorGroup* const llCurveEditorG;
    DiagonalCurveEditor* const llshape;
    DiagonalCurveEditor* const ccshape;
    CurveEditorGroup* const clCurveEditorG;
    DiagonalCurveEditor* const clshape;
    DiagonalCurveEditor* const lcshape;
    CurveEditorGroup* const HCurveEditorG;
    FlatCurveEditor* const LHshape;
    CurveEditorGroup* const H3CurveEditorG;
    FlatCurveEditor* const CHshape;
    CurveEditorGroup* const H2CurveEditorG;
    FlatCurveEditor* const HHshape;
    CurveEditorGroup* const rgbCurveEditorG;
    MyComboBoxText* const toneMethod;
    DiagonalCurveEditor* const rgbshape;
    Gtk::CheckButton* const special;
    MyExpander* const expmaskcol1;
    MyComboBoxText* const merMethod;
    ToolParamBlock* const mask7;
    MyComboBoxText* const mergecolMethod;
    Adjuster* const mercol;
    Adjuster* const opacol;
    Adjuster* const conthrcol;
    Gtk::Frame* const gridmerFrame;
    LabGrid* const labgridmerg;
    Adjuster* const merlucol;
    MyExpander* const expmaskcol;
    Gtk::Frame* const mergecolFrame ;
    MyComboBoxText* const showmaskcolMethod;
    MyComboBoxText* const showmaskcolMethodinv;
    Gtk::CheckButton* const enaColorMask;
    CurveEditorGroup* const maskCurveEditorG;
    FlatCurveEditor* const CCmaskshape;
    FlatCurveEditor* const LLmaskshape;
    FlatCurveEditor* const HHmaskshape;
    Gtk::Frame* const struFrame;
    Adjuster* const strumaskcol;
    Gtk::CheckButton* const toolcol;
    Gtk::Frame* const blurFrame;
    Gtk::CheckButton* const fftColorMask;
    Adjuster* const contcol;
    Adjuster* const blurcol;
    Adjuster* const blendmaskcol;
    Gtk::Frame* const toolcolFrame;
    Gtk::Frame* const toolcolFrame2;
    Adjuster* const radmaskcol;
    Adjuster* const lapmaskcol;
    Adjuster* const chromaskcol;
    Adjuster* const gammaskcol;
    Adjuster* const slomaskcol;
    Adjuster* const shadmaskcol;
    CurveEditorGroup* const maskHCurveEditorG;
    FlatCurveEditor* const HHhmaskshape;
    CurveEditorGroup* const mask2CurveEditorG;
    DiagonalCurveEditor* const Lmaskshape;
    CurveEditorGroup* const mask2CurveEditorGwav;
    FlatCurveEditor* const LLmaskcolshapewav;
    ThresholdAdjuster* const csThresholdcol;

    sigc::connection curvactivConn, previewcolConn, gridMethodConn, inversConn, qualitycurveMethodConn, toneMethodConn, specialConn, merMethodConn, mergecolMethodConn, showmaskcolMethodConn, showmaskcolMethodConninv, enaColorMaskConn, toolcolConn, fftColorMaskConn;

public:
    LocallabColor();
    ~LocallabColor();

    void setListener(ToolPanelListener* tpl) override;
    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;
    int nbmaskcol;
    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override {}; // Not used
//    void adjusterChanged3(ThresholdAdjuster* a, double newBottom, double newTop) override {};
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override {}; // Not used
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;
    void curveChanged(CurveEditor* ce) override;
    void updateguicolor(int spottype);
    void updateguiscopecolor(int scope);
    void previewcolChanged();

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;
    void curvactivChanged();
    void gridMethodChanged();
    void inversChanged();
    void qualitycurveMethodChanged();
    void toneMethodChanged();
    void specialChanged();
    void merMethodChanged();
    void mergecolMethodChanged();
    void showmaskcolMethodChanged();
    void showmaskcolMethodChangedinv();
    void enaColorMaskChanged();
    void toolcolChanged();
    void fftColorMaskChanged();
    void updateColorGUI1();
    void updateColorGUI2();
    void updateColorGUI3();
};

/* ==== LocallabExposure ==== */
class LocallabExposure:
    public Gtk::Box,
    public LocallabTool
{
private:
    // Exposure specific widgets
    MyComboBoxText* const expMethod;
//    Gtk::Frame* const pdeFrame;
    MyExpander* const exppde;
    Adjuster* const laplacexp;
    Adjuster* const reparexp;
    Adjuster* const linear;
    Adjuster* const balanexp;
    Adjuster* const gamm;
    Gtk::Label* const labelexpmethod;
    MyComboBoxText* const exnoiseMethod;
//    Gtk::Frame* const fatFrame;
    MyExpander* const expfat;
    Adjuster* const fatamount;
    Adjuster* const fatdetail;
    Gtk::CheckButton* const fatsatur;
    Gtk::CheckButton* const norm;
    Adjuster* const fatlevel;
    Adjuster* const fatanchor;
    Adjuster* const gamex;
    Adjuster* const sensiex;
    Gtk::ToggleButton* const previewexe;
    
    Adjuster* const structexp;
    Adjuster* const blurexpde;
    MyExpander* const exptoolexp;
    Adjuster* const expcomp;
    Adjuster* const black;
    Adjuster* const hlcompr;
    Adjuster* const hlcomprthresh;
    Adjuster* const shadex;
    Adjuster* const shcompr;
    Adjuster* const expchroma;
    CurveEditorGroup* const curveEditorG;
    DiagonalCurveEditor* shapeexpos;
    MyExpander* const exprecove;
    Gtk::Label* const maskusablee;
    Gtk::Label* const maskunusablee;
    Adjuster* const recothrese;
    Adjuster* const lowthrese;
    Adjuster* const higthrese;
    Adjuster* const decaye;
    
    MyExpander* const expgradexp;
    Adjuster* const strexp;
    Adjuster* const angexp;
    Adjuster* const featherexp;
    Adjuster* const softradiusexp;
    Gtk::CheckButton* const inversex;
    MyExpander* const expmaskexp;
    MyComboBoxText* const showmaskexpMethod;
    MyComboBoxText* const showmaskexpMethodinv;
    Gtk::CheckButton* const enaExpMask;
    Gtk::CheckButton* const enaExpMaskaft;
    CurveEditorGroup* const maskexpCurveEditorG;
    FlatCurveEditor* const CCmaskexpshape;
    FlatCurveEditor* const LLmaskexpshape;
    FlatCurveEditor* const HHmaskexpshape;
    Adjuster* const blendmaskexp;
    Adjuster* const radmaskexp;
    Adjuster* const lapmaskexp;
    Adjuster* const chromaskexp;
    Adjuster* const gammaskexp;
    Adjuster* const slomaskexp;
    Gtk::Frame* const gradFramemask;
    Adjuster* const strmaskexp;
    Adjuster* const angmaskexp;
    CurveEditorGroup* const mask2expCurveEditorG;
    DiagonalCurveEditor* const Lmaskexpshape;
    rtengine::ProcEvent Evlocallabtmosatur;

    sigc::connection expMethodConn, exnoiseMethodConn, previewexeConn, inversexConn, normConn, fatsaturConn, showmaskexpMethodConn, showmaskexpMethodConninv, enaExpMaskConn, enaExpMaskaftConn;

public:
    LocallabExposure();
    ~LocallabExposure();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;
    int nbmaskexp;

    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void curveChanged(CurveEditor* ce) override;
    void updateguiexpos(int spottype);
    void previewexeChanged();

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void expMethodChanged();
    void exnoiseMethodChanged();
    void inversexChanged();
    void normChanged();
    void fatsaturChanged();
    void showmaskexpMethodChanged();
    void showmaskexpMethodChangedinv();
    void enaExpMaskChanged();
    void enaExpMaskaftChanged();

    void updateExposureGUI1();
    void updateExposureGUI2();
    void updateExposureGUI3();
};


/* ==== LocallabjShadow ==== */
class LocallabShadow:
    public Gtk::Box,
    public LocallabTool
{
private:
    // Shadow highlight specific widgets
    MyComboBoxText* const shMethod;
    Adjuster* const reparsh;
    const std::array<Adjuster*, 6> multipliersh;
    Adjuster* const detailSH;
    Adjuster* const tePivot;
    Adjuster* const highlights;
    Adjuster* const h_tonalwidth;
    Adjuster* const shadows;
    Adjuster* const s_tonalwidth;
    Adjuster* const sh_radius;
    Adjuster* const sensihs;
    Gtk::ToggleButton* const previewsh;
    
    Adjuster* const blurSHde;
    MyExpander* const exprecovs;
    Gtk::Label* const maskusables;
    Gtk::Label* const maskunusables;
    Adjuster* const recothress;
    Adjuster* const lowthress;
    Adjuster* const higthress;
    Adjuster* const decays;
    Gtk::Frame* const gamFrame;
    Adjuster* const gamSH;
    Adjuster* const sloSH;

    MyComboBoxText* const ghsMethod;
    Gtk::Frame* const gridFrameghs;
    LabGrid* const labgridghs;
    Gtk::Frame* const ghsFrame;
    Gtk::Box* const matHBox;
    Gtk::CheckButton* const ghs_agx;
    MyComboBoxText* const ghsMatmet;
    Adjuster* const ghs_D;
    Gtk::Frame* const Framemtf;
    Gtk::CheckButton* const ghs_mtf;
    Adjuster* const ghs_mtfstr;
    Gtk::Frame* const Lab_Frame;
    Adjuster* const ghs_slope;
    Adjuster* const ghs_chro;
    Adjuster* const ghs_B;
    Adjuster* const ghs_SP;
    Gtk::Label* const ghssymLabel;
    Gtk::Label* const ghsmidLabel;
    Gtk::Label* const ghsmaxrgbLabel;
    Adjuster* const ghs_LP;
    Adjuster* const ghs_HP;
    Gtk::Frame* const LC_Frame;
    Adjuster* const ghs_LC;
    Adjuster* const ghs_MID;
    Gtk::Frame* const BP_Frame;
    Gtk::CheckButton* const ghs_autobw;
    Adjuster* const ghs_BLP;
    Adjuster* const ghs_HLP;
    Gtk::Label* const ghsbpwpLabels;
    Gtk::Label* const ghsbpwpvalueLabels;
    Gtk::Label* const ghscolorLabels;
    Gtk::Label* const ghsDRLabels;
    Gtk::CheckButton* const ghs_smooth;
    Gtk::CheckButton* const ghs_inv;

    Gtk::Frame* const michFrame;
    Gtk::Frame* const michtone_Frame;
    Adjuster* const mich_exp;
    Adjuster* const mich_spar;
    Adjuster* const mich_kpar;
    Gtk::CheckButton* const mich_jdx;
    Adjuster* const mich_sat;
    Adjuster* const mich_out;
    Gtk::Label* const michbwLabel;
    Gtk::Label* const michdataLabel;

    Gtk::CheckButton* const mich_black;
    Gtk::CheckButton* const mich_white;
    Adjuster* const mich_high;
    Adjuster* const mich_mtf;

    MyExpander* const expgradsh;
    Adjuster* const strSH;
    Adjuster* const angSH;
    Adjuster* const featherSH;
    Gtk::CheckButton* const inverssh;
    MyExpander* const expmasksh;
    MyComboBoxText* const showmaskSHMethod;
    MyComboBoxText* const showmaskSHMethodinv;
    Gtk::CheckButton* const enaSHMask;
    CurveEditorGroup* const maskSHCurveEditorG;
    FlatCurveEditor* const CCmaskSHshape;
    FlatCurveEditor* const LLmaskSHshape;
    FlatCurveEditor* const HHmaskSHshape;
    Adjuster* const blendmaskSH;
    Adjuster* const radmaskSH;
    Adjuster* const lapmaskSH;
    Adjuster* const chromaskSH;
    Adjuster* const gammaskSH;
    Adjuster* const slomaskSH;
    CurveEditorGroup* const mask2SHCurveEditorG;
    DiagonalCurveEditor* const LmaskSHshape;
    Gtk::Frame* const fatSHFrame;
    Adjuster* const fatamountSH;
    Adjuster* const fatanchorSH;

    rtengine::ProcEvent EvlocallabTePivot;
    rtengine::ProcEvent EvlocallabghsMethod;
    rtengine::ProcEvent Evlocallabghs_D;
    rtengine::ProcEvent Evlocallabghs_slope;
    rtengine::ProcEvent Evlocallabghs_chro;
    rtengine::ProcEvent Evlocallabghs_B;
    rtengine::ProcEvent Evlocallabghs_SP;
    rtengine::ProcEvent EvlocallabautoSPson;
    rtengine::ProcEvent EvlocallabautoSPoff;
    rtengine::ProcEvent Evlocallabghs_LP;
    rtengine::ProcEvent Evlocallabghs_HP;
    rtengine::ProcEvent Evlocallabghs_LC;
    rtengine::ProcEvent Evlocallabghs_MID;
    rtengine::ProcEvent Evlocallabghs_BLP;
    rtengine::ProcEvent Evlocallabghs_HLP;
    rtengine::ProcEvent Evlocallabghs_smooth;
    rtengine::ProcEvent Evlocallabghs_autobw;
    rtengine::ProcEvent Evlocallabghs_inv;
    rtengine::ProcEvent Evlocallabghs_agx;
    rtengine::ProcEvent Evlocallabghs_mtf;
    rtengine::ProcEvent Evlocallabghs_mtfstr;
    rtengine::ProcEvent Evlocallabghs_Matmet;
    rtengine::ProcEvent Evlocallabmich_exp;
    rtengine::ProcEvent Evlocallabmich_spar;
    rtengine::ProcEvent Evlocallabmich_kpar;
    rtengine::ProcEvent Evlocallabmich_sat;
    rtengine::ProcEvent Evlocallabmich_out;
    rtengine::ProcEvent Evlocallabmich_black;
    rtengine::ProcEvent Evlocallabmich_white;
    rtengine::ProcEvent Evlocallabmich_high;
    rtengine::ProcEvent Evlocallabmich_jdx;
    rtengine::ProcEvent Evlocallabmich_mtf;

    sigc::connection shMethodConn, ghsMethodConn, ghsMatmetConn, previewshConn, inversshConn, ghs_smoothConn, ghs_autobwConn, ghs_agxConn, ghs_mtfConn, ghs_invConn, mich_blackConn, mich_jdxConn, mich_whiteConn, showmaskSHMethodConn, showmaskSHMethodConninv, enaSHMaskConn;

public:
    LocallabShadow();
    ~LocallabShadow();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguishad(int spottype);
    void updateguiscopesahd(int scope);
    int nbmasksh;
    int nbwb;
    int nbsym2;
    void updateghsbw2(double ghsb, double ghsw, bool ghsaut);
    void updateghsbw(int bp, int wp, double minbp, double maxwp, double symev, double midgrey, double maxrgb, double sig3, double maxR, double maxG, double maxB, double drghs, bool ghsau);
    void updatemichbw(double michb, double michw, double michmean, double michmax, bool michaut);

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void curveChanged(CurveEditor* ce) override;
    void previewshChanged();
    void adjusterAutoToggled(Adjuster* a, bool newval);
    void autoSPChanged(float radius);

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void shMethodChanged();
    void ghsMethodChanged();
    void ghsMatmetChanged();
   
    void inversshChanged();
    void ghs_smoothChanged();
    void ghs_autobwChanged();
    void ghs_agxChanged(); 
    void ghs_mtfChanged(); 
    void ghs_invChanged();
    void mich_blackChanged();
    void mich_whiteChanged();
    void mich_jdxChanged();

    void showmaskSHMethodChanged();
    void showmaskSHMethodChangedinv();
    void enaSHMaskChanged();
    void updateShadowGUImask();
    void updateShadowGUIshmet();
    void updateShadowGUIsym();
};

/* ==== LocallabVibrance ==== */
class LocallabVibrance:
    public Gtk::Box,
    public LocallabTool,
    public ThresholdAdjusterListener,
    public ThresholdCurveProvider
{
private:
    // Vibrance specific widgets
    Adjuster* const saturated;
    Adjuster* const pastels;
    Adjuster* const vibgam;
    Adjuster* const warm;
    ThresholdAdjuster* const psThreshold;
    Gtk::CheckButton* const protectSkins;
    Gtk::CheckButton* const avoidColorShift;
    Gtk::CheckButton* const pastSatTog;
    Adjuster* const sensiv;
    Gtk::ToggleButton* const previewvib;
    
    CurveEditorGroup* const curveEditorGG;
    DiagonalCurveEditor* const skinTonesCurve;
    MyExpander* const exprecovv;
    Gtk::Label* const maskusablev;
    Gtk::Label* const maskunusablev;
    Adjuster* const recothresv;
    Adjuster* const lowthresv;
    Adjuster* const higthresv;
    Adjuster* const decayv;
    MyExpander* const expgradvib;
    Adjuster* const strvib;
    Adjuster* const strvibab;
    Adjuster* const strvibh;
    Adjuster* const angvib;
    Adjuster* const feathervib;
    MyExpander* const expmaskvib;
    MyComboBoxText* const showmaskvibMethod;
    Gtk::CheckButton* const enavibMask;
    CurveEditorGroup* const maskvibCurveEditorG;
    FlatCurveEditor* const CCmaskvibshape;
    FlatCurveEditor* const LLmaskvibshape;
    FlatCurveEditor* const HHmaskvibshape;
    Adjuster* const blendmaskvib;
    Adjuster* const radmaskvib;
    Adjuster* const lapmaskvib;
    Adjuster* const chromaskvib;
    Adjuster* const gammaskvib;
    Adjuster* const slomaskvib;
    CurveEditorGroup* const mask2vibCurveEditorG;
    DiagonalCurveEditor* const Lmaskvibshape;

    sigc::connection pskinsConn, previewvibConn, ashiftConn, pastsattogConn, showmaskvibMethodConn, enavibMaskConn;

public:
    LocallabVibrance();
    ~LocallabVibrance();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;
    int nbmaskvib;

    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguivib(int spottype);
    void updateguiscopevib(int scope);

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override {}; // Not used
//    void adjusterChanged3(ThresholdAdjuster* a, double newBottom, double newTop) override {};
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override;
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override {}; // Not used
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override {}; // Not used
    std::vector<double> getCurvePoints(ThresholdSelector* tAdjuster) const override;
    void curveChanged(CurveEditor* ce) override;
    void previewvibChanged();

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void protectskins_toggled();
    void avoidcolorshift_toggled();
    void pastsattog_toggled();
    void showmaskvibMethodChanged();
    void enavibMaskChanged();

    void updateVibranceGUI();
};

/* ==== LocallabSoft ==== */
class LocallabSoft:
    public Gtk::Box,
    public LocallabTool
{
private:
    // Soft light specific widgets
    MyComboBoxText* const softMethod;
    Gtk::Box* const ctboxsoftmethod;
    MyComboBoxText* const showmasksoftMethod;
    Adjuster* const streng;
    Adjuster* const laplace;
    Adjuster* const sensisf;

    sigc::connection softMethodConn, showmasksoftMethodConn;

public:
    LocallabSoft();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguisoft(int spottype);

    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;

private:
    void complexityModeChanged();

    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void softMethodChanged();
    void showmasksoftMethodChanged();

    void updateSoftGUI();
};

/* ==== LocallabBlur ==== */
class LocallabBlur:
    public Gtk::Box,
    public LocallabTool,
    public ThresholdAdjusterListener
//    public ThresholdCurveProvider
    
{
private:
    // Blur & Noise specific widgets
    MyExpander* const expblnoise;
    MyComboBoxText* const blMethod;
    Gtk::CheckButton* const fftwbl;
    Adjuster* const radius;
    Adjuster* const strength;
    Gtk::Frame* const grainFrame;
    Gtk::Frame* const grainFrame2;
    Adjuster* const isogr;
    Adjuster* const strengr;
    Adjuster* const scalegr;
    Adjuster* const divgr;
    MyComboBoxText* const medMethod;
    Adjuster* const itera;
    Adjuster* const guidbl;
    Adjuster* const strbl;
    Adjuster* const epsbl;
    MyExpander* const expdenoise2;
    Adjuster* const recothres;
    Adjuster* const lowthres;
    Adjuster* const higthres;
    Adjuster* const sensibn;
    
    MyComboBoxText* const blurMethod;
    Gtk::CheckButton* const invbl;
    MyComboBoxText* const chroMethod;
    Gtk::CheckButton* const activlum;
    MyExpander* const expdenoise;
    Gtk::Frame* const denoFrame;

    Gtk::CheckButton* const enacontrast;
    Adjuster* const denocontrast;
    Adjuster* const denoratio;
    Gtk::CheckButton* const contrshow;
    Adjuster* const denomask;

    MyComboBoxText* const quamethod;
    MyExpander* const expdenoisenl;
    MyExpander* const expdenoiselum;
    MyExpander* const expdenoisech;
    std::unique_ptr<CurveEditorGroup> LocalcurveEditorwavden;    
    FlatCurveEditor* const wavshapeden;
    Gtk::Label* const lCLabels;
    Gtk::Label* const lumLabels;
    Gtk::Label* const lum46Labels;
    Gtk::Label* const chroLabels;
    Gtk::Label* const chro46Labels;
    Gtk::CheckButton* const lockmadl;
    Gtk::Frame* const madlFrame;
    const std::array<Adjuster*, 21> madls;
    Gtk::CheckButton* const madllock;
    
    MyExpander* const expdenoise1;
    Gtk::Label* const maskusable;
    Gtk::Label* const maskunusable;
    Gtk::Label* const maskusable2;
    Gtk::Label* const maskunusable2;
    Gtk::Label* const maskusable3;
    Gtk::Label* const maskunusable3;

    Gtk::CheckButton* const usemask;
    Adjuster* const lnoiselow;
    Adjuster* const levelthr;
    Adjuster* const levelthrlow;
    Adjuster* const noiselumf0;
    Adjuster* const noiselumf;
    Adjuster* const noiselumf2;
    Adjuster* const noiselumc;
    Adjuster* const noiselumdetail;
    Adjuster* const noiselequal;
    Adjuster* const noisegam;
    std::unique_ptr<CurveEditorGroup> LocalcurveEditorwavhue;    
    FlatCurveEditor* wavhue;
    std::unique_ptr<CurveEditorGroup> LocalcurveEditorwavhuecont;    
    FlatCurveEditor* wavhuecont;
    Adjuster* const noisechrof;
    Adjuster* const noisechroc;
    Adjuster* const noisechrodetail;
    Gtk::Frame* const detailFrame;
    Adjuster* const detailthr;
    Adjuster* const adjblur;
    MyExpander* const expdenoise3;
    Adjuster* const recothresd;
    Adjuster* const lowthresd;
    Adjuster* const midthresd;
    Adjuster* const midthresdch;
    Adjuster* const higthresd;
    Adjuster* const decayd;
    
    Gtk::CheckButton* const invmaskd;
    Gtk::CheckButton* const invmask;
    Gtk::Frame* const prevFrame;
    Adjuster* const nlstr;
    Adjuster* const nldet;
    Adjuster* const nlpat;
    Adjuster* const nlrad;
    Adjuster* const nlgam;
    Adjuster* const nliter;
    Adjuster* const bilateral;
    Adjuster* const sensiden;

    rtengine::ProcEvent Evlocallabnliter;
   
    Adjuster* const reparden;
    Gtk::Button* neutral;
    MyExpander* const expmaskbl;
    MyComboBoxText* const showmaskblMethod;
    MyComboBoxText* const showmaskblMethodtyp;
    Gtk::CheckButton* const enablMask;
    std::unique_ptr<CurveEditorGroup> maskblCurveEditorG;    
    FlatCurveEditor* const CCmaskblshape;
    FlatCurveEditor* const LLmaskblshape;
    FlatCurveEditor* const HHmaskblshape;
    Adjuster* const strumaskbl;
    Gtk::CheckButton* const toolbl;
    Gtk::Frame* const toolblFrame;
    Gtk::Frame* const toolblFrame2;
    Adjuster* const blendmaskbl;
    Adjuster* const radmaskbl;
    Adjuster* const lapmaskbl;
    Adjuster* const chromaskbl;
    Adjuster* const gammaskbl;
    Adjuster* const slomaskbl;
    Adjuster* const shadmaskbl;
    Adjuster* const shadmaskblsha;
    std::unique_ptr<CurveEditorGroup> mask2blCurveEditorG;    
    DiagonalCurveEditor* const Lmaskblshape;
    std::unique_ptr<CurveEditorGroup> mask2blCurveEditorGwav;    
    FlatCurveEditor* const LLmaskblshapewav;
    Gtk::Box* const quaHBox;
    ThresholdAdjuster* const csThresholdblur;

    sigc::connection blMethodConn, fftwblConn, invblConn, contrshowConn, lockmadlConn, madllockConn, enacontrastConn, medMethodConn, blurMethodConn, chroMethodConn, activlumConn, showmaskblMethodConn, showmaskblMethodtypConn, enablMaskConn, toolblConn;
    sigc::connection  quamethodconn, usemaskConn, invmaskdConn, invmaskConn, neutralconn;
    rtengine::ProcEvent Evlocallabdenocontrast;
    rtengine::ProcEvent Evlocallabautodenoon;
    rtengine::ProcEvent Evlocallabautodenooff;
    rtengine::ProcEvent Evlocallabcontrshow;
    rtengine::ProcEvent Evlocallabenacontrast;
    rtengine::ProcEvent Evlocallabdenoratio;
    rtengine::ProcEvent Evlocallabdenomask;
    rtengine::ProcEvent EvlocallabwavCurvehuecont;
    rtengine::ProcEvent Evlocallablockmadl;
    rtengine::ProcEvent Evlocallablockmadls;
    rtengine::ProcEvent Evlocallabmadllock;


public:
    LocallabBlur();
    ~LocallabBlur();
    void updatedenlc(const double highres, const double nres, const double highres46, const double nres46, const double Lhighres, const double Lnres, const double Lhighres46, const double Lnres46);
    void updatemadlc(const double m0, const double m1, const double m2, const double m3, const double m4, const double m5, const double m6, const double m7,
        const double m8, const double m9, const double m10, const double m11, const double m12, const double m13, const double m14, const double m15,
        const double m16, const double m17, const double m18, const double m19, const double m20, const bool madloc);

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void neutral_pressed();
    void updateguiblur(int spottype);

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override {}; // Not used
//    void adjusterChanged3(ThresholdAdjuster* a, double newBotto, double newTo) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override {}; // Not used
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;
    void curveChanged(CurveEditor* ce) override;
    void adjusterAutoToggled(Adjuster* a, bool newval);
    void autodenoContrastChanged(float autodenoContrast);

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;
    void contrshowChanged();
    void enacontrastChanged();
    void lockmadlChanged();
    void madllockChanged();

    void blMethodChanged();
    void fftwblChanged();
    void usemaskChanged();
    void invmaskdChanged();
    void invmaskChanged();
    void invblChanged();
    void medMethodChanged();
    void blurMethodChanged();
    void chroMethodChanged();
    void activlumChanged();
    void showmaskblMethodChanged();
    void showmaskblMethodtypChanged();
    void enablMaskChanged();
    void toolblChanged();
    void quamethodChanged();

    void updateBlurGUI();
};

/* ==== LocallabTone ==== */
class LocallabTone:
    public Gtk::Box,
    public LocallabTool
{
private:
    // Tone Mapping specific widgets
    Adjuster* const repartm;
    Adjuster* const amount;
    Adjuster* const stren;
    Gtk::CheckButton* const equiltm;
    Adjuster* const gamma;
    Adjuster* const satur;
    Adjuster* const estop;
    Adjuster* const scaltm;
    Adjuster* const rewei;
    Adjuster* const softradiustm;
    Adjuster* const sensitm;
    Gtk::ToggleButton* const previewtm;
    
    MyExpander* const exprecovt;
    Gtk::Label* const maskusablet;
    Gtk::Label* const maskunusablet;
    Adjuster* const recothrest;
    Adjuster* const lowthrest;
    Adjuster* const higthrest;
    Adjuster* const decayt;
    MyExpander* const expmasktm;
    MyComboBoxText* const showmasktmMethod;
    Gtk::CheckButton* const enatmMask;
    Gtk::CheckButton* const enatmMaskaft;
    CurveEditorGroup* const masktmCurveEditorG;
    FlatCurveEditor* const CCmasktmshape;
    FlatCurveEditor* const LLmasktmshape;
    FlatCurveEditor* const HHmasktmshape;
    Adjuster* const blendmasktm;
    Adjuster* const lapmasktm;
    Adjuster* const radmasktm;
    Adjuster* const chromasktm;
    Adjuster* const gammasktm;
    Adjuster* const slomasktm;
    CurveEditorGroup* const mask2tmCurveEditorG;
    DiagonalCurveEditor* const Lmasktmshape;

    sigc::connection equiltmConn, previewtmConn, showmasktmMethodConn, enatmMaskConn, enatmMaskaftConn;

public:
    LocallabTone();
    ~LocallabTone();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguitone(int spottype);
    void previewtmChanged();
    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void curveChanged(CurveEditor* ce) override;

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void equiltmChanged();
    void showmasktmMethodChanged();
    void enatmMaskChanged();
    void enatmMaskaftChanged();
};

/* ==== LocallabRetinex ==== */
class LocallabRetinex:
    public Gtk::Box,
    public LocallabTool
{
private:
    // Retinex specific widgets
    Gtk::Frame* const dehaFrame;
    Adjuster* const dehaz;
    Adjuster* const depth;
    Adjuster* const dehazeSaturation;
    Adjuster* const dehazeblack;
    Gtk::Frame* const retiFrame;
    Adjuster* const str;
    Gtk::CheckButton* const loglin;
    Adjuster* const sensih;
    Gtk::Frame* const retitoolFrame;
    MyComboBoxText* const retinexMethod;
    Gtk::CheckButton* const fftwreti;
    Gtk::CheckButton* const equilret;
    Adjuster* const neigh;
    Adjuster* const vart;
    Adjuster* const scalereti;
    Adjuster* const limd;
    Adjuster* const offs;
    MyExpander* const expretitools;
    Adjuster* const chrrt;
    Adjuster* const darkness;
    Adjuster* const lightnessreti;
    Adjuster* const cliptm;
    Adjuster* const softradiusret;
    CurveEditorGroup* const LocalcurveEditortransT;
    FlatCurveEditor* const cTtransshape;
    Gtk::Label* const mMLabels;
    Gtk::Label* const transLabels;
    Gtk::Label* const transLabels2;
    CurveEditorGroup* const LocalcurveEditorgainT;
    FlatCurveEditor* const cTgainshape;
    MyExpander* const exprecovr;
    Gtk::Label* const maskusabler;
    Gtk::Label* const maskunusabler;
    Adjuster* const recothresr;
    Adjuster* const lowthresr;
    Adjuster* const higthresr;
    Adjuster* const decayr;
    MyExpander* const expmaskreti;
    MyComboBoxText* const showmaskretiMethod;
    Gtk::CheckButton* const enaretiMask;
    Gtk::CheckButton* const enaretiMasktmap;
    CurveEditorGroup* const maskretiCurveEditorG;
    FlatCurveEditor* const CCmaskretishape;
    FlatCurveEditor* const LLmaskretishape;
    FlatCurveEditor* const HHmaskretishape;
    Adjuster* const blendmaskreti;
    Adjuster* const radmaskreti;
    Adjuster* const lapmaskreti;
    Adjuster* const chromaskreti;
    Adjuster* const gammaskreti;
    Adjuster* const slomaskreti;
    CurveEditorGroup* const mask2retiCurveEditorG;
    DiagonalCurveEditor* const Lmaskretishape;
    Gtk::CheckButton* const inversret;

    rtengine::ProcEvent Evlocallabdehazeblack;

    sigc::connection loglinConn, retinexMethodConn, fftwretiConn, equilretConn, showmaskretiMethodConn, enaretiMaskConn, enaretiMasktmapConn, inversretConn;

public:
    LocallabRetinex();
    ~LocallabRetinex();

    void updateMinMax(const double cdma, const double cdmin, const double mini, const double maxi, const double Tmean, const double Tsigma, const double Tmin, const double Tmax);
    void updateguireti(int spottype);

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    void updateAdviceTooltips(const bool showTooltips) override;

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void curveChanged(CurveEditor* ce) override;

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void loglinChanged();
    void retinexMethodChanged();
    void fftwretiChanged();
    void equilretChanged();
    void showmaskretiMethodChanged();
    void enaretiMaskChanged();
    void enaretiMasktmapChanged();
    void inversretChanged();

    void updateRetinexGUI1();
    void updateRetinexGUI2();
    void updateRetinexGUI3();
};

/* ==== LocallabSharp ==== */
class LocallabSharp:
    public Gtk::Box,
    public LocallabTool
{
private:
    // Adjuster* blur;
    MyComboBoxText* methodcap;


    Adjuster* const reparsha;
    Adjuster* const sharcontrast;
    Gtk::CheckButton* const sharshow;
    Adjuster* const capradius;

    Adjuster* const deconvCoBoost;
    Adjuster* const deconvCoProt;
    Adjuster* const deconvCoLat;
    Adjuster* const deconvCogam;
    Gtk::CheckButton* const itercheck;
    Gtk::Frame* const capFrame;
    Gtk::Frame* const rlFrame;
    Adjuster* const sharblur;
    Adjuster* const shargam;
    Adjuster* const sharamount;
    Adjuster* const shardamping;
    Adjuster* const shariter;
    Adjuster* const sharradius;
    Adjuster* const sensisha;
    Gtk::CheckButton* const inverssha;
    Gtk::Frame* const sharFrame;
    MyComboBoxText* const showmasksharMethod;

    rtengine::ProcEvent Evlocallabmethodcap;
    rtengine::ProcEvent Evlocallabcapradius;
    rtengine::ProcEvent Evlocallabautoradiuson;
    rtengine::ProcEvent Evlocallabautoradiusoff;
    rtengine::ProcEvent Evlocallabsharrepar;
    rtengine::ProcEvent Evlocallabsharcontraston;
    rtengine::ProcEvent Evlocallabsharcontrastoff;

    rtengine::ProcEvent Evlocallababdconvboost;
    rtengine::ProcEvent Evlocallababdcoprot;
    rtengine::ProcEvent Evlocallababdconvlat;
    rtengine::ProcEvent Evlocallababsharshow;
    rtengine::ProcEvent Evlocallababitercheck;
    rtengine::ProcEvent Evlocallababdconvgam;

    sigc::connection inversshaConn, showmasksharMethodConn, methodcapConn, sharshowConn, itercheckConn;

public:
    LocallabSharp();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguisharp(int spottype);

    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterAutoToggled(Adjuster* a, bool newval);
 //   void adjusterAutoToggled(Adjuster* a);
    void autoDeconvRadiusChanged(float radius);
    void autoContrastChanged(float autoContrast);

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;
    void sharshowChanged();
    void itercheckChanged();

    void inversshaChanged();
    void methodcapChanged();
    void showmasksharMethodChanged();
};

/* ==== LocallabContrast ==== */
class LocallabContrast:
    public Gtk::Box,
    public LocallabTool,
    public ThresholdAdjusterListener

{
private:
    MyComboBoxText* const localcontMethod;
    Adjuster* const lcradius;
    Adjuster* const lcamount;
    Adjuster* const lcdarkness;
    Adjuster* const lclightness;
    Gtk::Frame* const contFrame;
    Adjuster* const sigmalc;
    Adjuster* const offslc;
    CurveEditorGroup* const LocalcurveEditorwav;
    FlatCurveEditor* const wavshape;
    ThresholdAdjuster* const csThreshold;
    Gtk::CheckButton* const processwav;
    
    Adjuster* const levelwav;
    MyExpander* const expresidpyr;
    Adjuster* const residcont;
    Adjuster* const residchro;
    Adjuster* const residsha;
    Adjuster* const residshathr;
    Adjuster* const residhi;
    Adjuster* const residhithr;
    Adjuster* const gamlc;
    Adjuster* const residgam;
    Adjuster* const residslop;
    Adjuster* const sensilc;
    Gtk::ToggleButton* const previewlc;
   
    Adjuster* const reparw;
    Gtk::Frame* const clariFrame;
    Adjuster* const clarilres;
    Adjuster* const claricres;
    Adjuster* const clarisoft;
    Gtk::CheckButton* const origlc;
    MyExpander* const expcontrastpyr;
    Gtk::Frame* const gradwavFrame;
    Gtk::CheckButton* const wavgradl;
    Adjuster* const sigmalc2;
    Adjuster* const strwav;
    Adjuster* const angwav;
    Adjuster* const featherwav;
    Gtk::CheckButton* const wavedg;
    Adjuster* const strengthw;
    Adjuster* const sigmaed;
    CurveEditorGroup* const LocalcurveEditorwavedg;
    FlatCurveEditor* const wavshapeedg;
    Adjuster* const gradw;
    Gtk::CheckButton* const waveshow;
    ToolParamBlock* const edgsBoxshow;
    Adjuster* const radiusw;
    Adjuster* const detailw;
    MyComboBoxText* const localedgMethod;
    Adjuster* const tloww;
    Adjuster* const thigw;
    Adjuster* const edgw;
    Adjuster* const basew;
    MyComboBoxText* const localneiMethod;
    Gtk::CheckButton* const wavblur;
    Adjuster* const levelblur;
    Adjuster* const sigmabl;
    Adjuster* const chromablu;
    CurveEditorGroup* const LocalcurveEditorwavlev;
    FlatCurveEditor* const wavshapelev;
    Adjuster* const residblur;
    Gtk::CheckButton* const blurlc;
    MyExpander* const expcontrastpyr2;
    Gtk::CheckButton* const wavcont;
    Adjuster* const sigma;
    Adjuster* const offset;
    Adjuster* const chromalev;
    CurveEditorGroup* const LocalcurveEditorwavcon;
    FlatCurveEditor* const wavshapecon;
    Gtk::CheckButton* const wavcompre;
    CurveEditorGroup* const LocalcurveEditorwavcompre;
    FlatCurveEditor* const wavshapecompre;
    Adjuster* const sigmadr;
    Adjuster* const threswav;
    Adjuster* const residcomp;
    Gtk::CheckButton* const wavcomp;
    Adjuster* const sigmadc;
    Adjuster* const deltad;
    CurveEditorGroup* const LocalcurveEditorwavcomp;
    FlatCurveEditor* const wavshapecomp;
    //Adjuster* const fatres;
    Gtk::CheckButton* const fftwlc;
    MyExpander* const exprecovw;
    Gtk::Label* const maskusablew;
    Gtk::Label* const maskunusablew;
    Adjuster* const recothresw;
    Adjuster* const lowthresw;
    Adjuster* const higthresw;
    Adjuster* const decayw;
    MyExpander* const expmasklc;
    MyComboBoxText* const showmasklcMethod;
    Gtk::CheckButton* const enalcMask;
    CurveEditorGroup* const masklcCurveEditorG;
    FlatCurveEditor* const CCmasklcshape;
    FlatCurveEditor* const LLmasklcshape;
    FlatCurveEditor* const HHmasklcshape;
    Adjuster* const blendmasklc;
    Adjuster* const radmasklc;
    Adjuster* const chromasklc;
    CurveEditorGroup* const mask2lcCurveEditorG;
    DiagonalCurveEditor* const Lmasklcshape;

    sigc::connection localcontMethodConn, previewlcConn, origlcConn, processwavConn, wavgradlConn, wavedgConn, localedgMethodConn, waveshowConn, localneiMethodConn, wavblurConn, blurlcConn, wavcontConn, wavcompreConn, wavcompConn, fftwlcConn, showmasklcMethodConn, enalcMaskConn;
    rtengine::ProcEvent Evlocallabprocesswav;

public:
    LocallabContrast();
    ~LocallabContrast();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;
    int nbmaskcont;

    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguicont(int spottype);
    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override {}; // Not used
//    void adjusterChanged3(ThresholdAdjuster* a, double newBottom, double newTop) override {};
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override {}; // Not used
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;
    void curveChanged(CurveEditor* ce) override;
    void previewlcChanged();

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void localcontMethodChanged();
    void origlcChanged();
    void processwavChanged();
    void wavgradlChanged();
    void wavedgChanged();
    void localedgMethodChanged();
    void waveshowChanged();
    void localneiMethodChanged();
    void wavblurChanged();
    void blurlcChanged();
    void wavcontChanged();
    void wavcompreChanged();
    void wavcompChanged();
    void fftwlcChanged();
    void showmasklcMethodChanged();
    void enalcMaskChanged();

    void updateContrastGUI1();
    void updateContrastGUI2();
    void updateContrastGUI3();
};

/* ==== LocallabCBDL ==== */
class LocallabCBDL:
    public Gtk::Box,
    public LocallabTool
{
private:
    Gtk::Frame* const levFrame;
    const std::array<Adjuster*, 6> multiplier;
    Adjuster* const chromacbdl;
    Adjuster* const threshold;
    Adjuster* const clarityml;
    Adjuster* const contresid;
    Adjuster* const softradiuscb;
    Adjuster* const sensicb;
    MyExpander* const exprecovcb;
    Gtk::Label* const maskusablecb;
    Gtk::Label* const maskunusablecb;
    Adjuster* const recothrescb;
    Adjuster* const lowthrescb;
    Adjuster* const higthrescb;
    Adjuster* const decaycb;
    MyExpander* const expmaskcb;
    MyComboBoxText* const showmaskcbMethod;
    Gtk::CheckButton* const enacbMask;
    CurveEditorGroup* const maskcbCurveEditorG;
    FlatCurveEditor* const CCmaskcbshape;
    FlatCurveEditor* const LLmaskcbshape;
    FlatCurveEditor* const HHmaskcbshape;
    Adjuster* const blendmaskcb;
    Adjuster* const radmaskcb;
    Adjuster* const lapmaskcb;
    Adjuster* const chromaskcb;
    Adjuster* const gammaskcb;
    Adjuster* const slomaskcb;
    CurveEditorGroup* const mask2cbCurveEditorG;
    DiagonalCurveEditor* const Lmaskcbshape;

    sigc::connection showmaskcbMethodConn, enacbMaskConn;

    Gtk::Button* const lumacontrastMinusButton;
    Gtk::Button* const lumaneutralButton;
    Gtk::Button* const lumacontrastPlusButton;

    sigc::connection lumacontrastMinusPressedConn, lumaneutralPressedConn, lumacontrastPlusPressedConn;

public:
    LocallabCBDL();
    ~LocallabCBDL();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguicbdl(int spottype);

    void setDefaultExpanderVisibility() override;
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void curveChanged(CurveEditor* ce) override;

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void showmaskcbMethodChanged();
    void enacbMaskChanged();

    void lumacontrastMinusPressed();
    void lumaneutralPressed();
    void lumacontrastPlusPressed();
};

/* ==== LocallabLog ==== */
class LocallabLog:
    public Gtk::Box,
    public LocallabTool
{
private:
    Adjuster* const repar;
    Gtk::CheckButton* const ciecam;
    Gtk::ToggleButton* const autocompute;
    Gtk::Frame* const logPFrame;
    Gtk::Frame* const logPFrame2;
    Adjuster* const blackEv;
    Adjuster* const whiteEv;
    Adjuster* const whiteslog;
    Adjuster* const blackslog;
    Adjuster* const comprlog;
    Adjuster* const strelog; 
    Gtk::CheckButton* const satlog;
    
    Gtk::CheckButton* const fullimage;
    Gtk::Frame* const logFrame;
    Gtk::CheckButton* const Autogray;
    Adjuster* const sourceGray;
    Adjuster* const sourceabs;
    MyComboBoxText*  const sursour;
    Gtk::Box* const surHBox;
    Gtk::Frame* const log1Frame;
    Gtk::Frame* const log2Frame;
    Adjuster* const targetGray;
    Adjuster* const detail;
    Adjuster* const catad;
    Adjuster* const lightl;
    Adjuster* const lightq;
    Adjuster* const contl;
    Adjuster* const contq;
    Adjuster* const contthres;
    Adjuster* const colorfl;
    Adjuster* const saturl;
    Adjuster* const chroml;
    MyExpander* const expL;
    //CurveEditorGroup* const CurveEditorL;
    //DiagonalCurveEditor* const LshapeL;
    Adjuster* const targabs;
    MyComboBoxText*  const surround;
    Gtk::Box* const surrHBox;
    
    Adjuster* const baselog;
    MyExpander* const exprecovl;
    Gtk::Label* const maskusablel;
    Gtk::Label* const maskunusablel;
    Adjuster* const recothresl;
    Adjuster* const lowthresl;
    Adjuster* const higthresl;
    Adjuster* const decayl;
    
    Adjuster* const sensilog;
    Gtk::ToggleButton* const previewlog;
    
    Gtk::Frame* const gradlogFrame;
    Adjuster* const strlog;
    Adjuster* const anglog;
    Adjuster* const featherlog;
    MyExpander* const expmaskL;
    MyComboBoxText* const showmaskLMethod;
    Gtk::CheckButton* const enaLMask;
    CurveEditorGroup* const maskCurveEditorL;
    FlatCurveEditor* const CCmaskshapeL;
    FlatCurveEditor* const LLmaskshapeL;
    FlatCurveEditor* const HHmaskshapeL;
    Adjuster* const blendmaskL;
    Adjuster* const radmaskL;
    Adjuster* const chromaskL;
    CurveEditorGroup* const mask2CurveEditorL;
    DiagonalCurveEditor* const LmaskshapeL;

    sigc::connection autoconn, ciecamconn, fullimageConn, AutograyConn;
    sigc::connection  surroundconn, sursourconn, satlogconn;
    sigc::connection showmaskLMethodConn, enaLMaskConn, previewlogConn;
public:
    LocallabLog();
    ~LocallabLog();
    
    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;
    void updateguilog(int spottype);
    void previewlogChanged();
    int nbmasklog;
    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void surroundChanged();
    void sursourChanged();
    void setDefaultExpanderVisibility() override;
    void satlogChanged();

    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void curveChanged(CurveEditor* ce) override;

    void updateAutocompute(const float blackev, const float whiteev, const float sourceg, const float sourceab, const float targetg, const float jz1);

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;
    void complexityModeChanged();

    void autocomputeToggled();
    void fullimageChanged();
    void AutograyChanged();
    void ciecamChanged();
    void showmaskLMethodChanged();
    void enaLMaskChanged();
    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void updateLogGUI();
    void updateLogGUI2();
};


/* ==== LocallabMask ==== */
class LocallabMask:
    public Gtk::Box,
    public LocallabTool,
    public ThresholdAdjusterListener
{
private:
    Adjuster* const sensimask;
    Gtk::ToggleButton* const previewmas;
    
    Adjuster* const blendmask;
    Adjuster* const blendmaskab;
    Adjuster* const softradiusmask;
    MyComboBoxText* const showmask_Method;
    Gtk::CheckButton* const enamask;
    CurveEditorGroup* const mask_CurveEditorG;
    FlatCurveEditor* const CCmask_shape;
    FlatCurveEditor* const LLmask_shape;
    FlatCurveEditor* const HHmask_shape;
    Gtk::Frame* const struFrame;
    Adjuster* const strumaskmask;
    Gtk::CheckButton* const toolmask;
    Gtk::Frame* const blurFrame;
    Gtk::CheckButton* const fftmask;
    Adjuster* const contmask;
    Adjuster* const blurmask;
    Gtk::Frame* const toolmaskFrame;
    Adjuster* const radmask;
    Adjuster* const lapmask;
    Adjuster* const chromask;
    Adjuster* const gammask;
    Adjuster* const slopmask;
    Adjuster* const shadmask;
    CurveEditorGroup* const mask_HCurveEditorG;
    FlatCurveEditor* const HHhmask_shape;
    CurveEditorGroup* const mask2CurveEditorG;
    DiagonalCurveEditor* const Lmask_shape;
    CurveEditorGroup* const mask2CurveEditorGwav;
    FlatCurveEditor* const LLmask_shapewav;
    ThresholdAdjuster* const csThresholdmask;
    Gtk::Frame* const gradFramemask;
    Adjuster* const str_mask;
    Adjuster* const feather_mask;
    Adjuster* const ang_mask;

    sigc::connection showmask_MethodConn, previewmasConn, enamaskConn, toolmaskConn, fftmaskConn;

public:
    LocallabMask();
    ~LocallabMask();

    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;

    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void updateguimask(int spottype);
    void previewmasChanged();

    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override {}; // Not used
//    void adjusterChanged3(ThresholdAdjuster* a, double newBottom, double newTop) override {};
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override {}; // Not used
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;
    void curveChanged(CurveEditor* ce) override;

private:
    void complexityModeChanged();

    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;

    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;

    void showmask_MethodChanged();
    void enamaskChanged();
    void toolmaskChanged();
    void fftmaskChanged();

    void updateMaskGUI();
};


/* ==== Locallabcie ==== */
class Locallabcie:
    public Gtk::Box,
    public ThresholdAdjusterListener,
    public LocallabTool
{
private:
    Adjuster* const sensicie;
    Gtk::ToggleButton* const previewcie;
    
    Adjuster* const reparcie;
    Gtk::CheckButton* const jabcie;
    MyComboBoxText*  const modecam;
    MyComboBoxText*  const modeQJ;
    MyComboBoxText*  const modecie;
    Gtk::Frame* const jzFrame;
    Gtk::Box* const modeHBoxcam;
    Gtk::Box* const modeHBoxQJ;
    Gtk::Box* const modeHBoxcie;
    Gtk::Frame* const cieFrame;
    MyExpander* const expcamscene;
    
    Gtk::CheckButton* const Autograycie;
    Adjuster* const sourceGraycie;
    Adjuster* const sourceabscie;
    MyComboBoxText*  const sursourcie;
    Gtk::Box* const surHBoxcie;
    Gtk::Frame* const cie1Frame;
    Gtk::Frame* const cie1lightFrame;
    Gtk::Frame* const cie1contFrame;
    Gtk::Frame* const cie1colorFrame;
    Gtk::Frame* const czlightFrame;
//    Gtk::Frame* const czcontFrame;
    Gtk::Frame* const czcolorFrame;
    Gtk::Frame* const PQFrame;
    Gtk::CheckButton* const qtoj;
    Adjuster* const lightlcie;
    Adjuster* const lightjzcie;
    Adjuster* const contjzcie;
    Adjuster* const adapjzcie;
    Adjuster* const jz100;
    Adjuster* const pqremap;
    Adjuster* const pqremapcam16;
    MyExpander* const expjz;
    Gtk::Frame* const jzshFrame;
    Adjuster* const hljzcie;
    Adjuster* const hlthjzcie;
    Adjuster* const shjzcie;
    Adjuster* const shthjzcie;
    Adjuster* const radjzcie;
    
    MyExpander* const expwavjz;
    
    Gtk::Frame* const contFramejz;
    Adjuster* const sigmalcjz;
    CurveEditorGroup* const LocalcurveEditorwavjz;
    FlatCurveEditor* const wavshapejz;
    ThresholdAdjuster* const csThresholdjz;
    Gtk::Frame* const clariFramejz;
    Adjuster* const clarilresjz;
    Adjuster* const claricresjz;
    Adjuster* const clarisoftjz;
    
    MyExpander* const expcam16;
    MyExpander* const expcamviewing;
    
    Adjuster* const lightqcie;
    Adjuster* const contlcie;
    Adjuster* const contqcie;
    Adjuster* const lightsigqcie;
    Adjuster* const contsigqcie;
    Adjuster* const contthrescie;
    Gtk::Frame* const logjzFrame;
    Gtk::CheckButton* const logjz;
    Adjuster* const blackEvjz;
    Adjuster* const whiteEvjz;
    Adjuster* const targetjz;
    Gtk::Frame* const bevwevFrame;
    Gtk::CheckButton* const sigybjz12;
    ToolParamBlock* const sigBox12;
    Gtk::Frame* const sigmoidFrame12;
    Gtk::CheckButton* const sigq12;
    Adjuster* const slopesmoq;
    Adjuster* const sigmoidldacie12;
    Adjuster* const sigmoidthcie12;
    Adjuster* const sigmoidblcie12;
    Gtk::Box* autocomprHBox;
    Gtk::ToggleButton* const comprcieauto;
    Gtk::CheckButton* const normcie12;
    Gtk::CheckButton* const normcie;
    Gtk::Box* const modeHBoxbwev12;
    MyComboBoxText* const bwevMethod12;
    Gtk::Box* const modeHBoxbwev;
    MyComboBoxText* const bwevMethod;

     ToolParamBlock* const sigBox;
    Gtk::Frame* const sigmoidFrame;
    Gtk::CheckButton* const sigq;
    Gtk::Frame* const sigmoidnormFrame;
    Adjuster* const sigmoidldacie;
    Adjuster* const sigmoidthcie;
    Adjuster* const sigmoidsenscie;
    Adjuster* const sigmoidblcie;
   
    Gtk::Frame* const logcieFrame;
    Gtk::CheckButton* const logcie;
    ToolParamBlock* const comprBox;
    Adjuster* const comprcie;
    
    Adjuster* const strcielog;
    Gtk::CheckButton* const satcie;
    Gtk::CheckButton* const logcieq;
    Adjuster* const comprcieth;
    MyExpander* const expprecam;    
    Adjuster* const gamjcie;
    Adjuster* const slopjcie;
    Adjuster* const satjcie;
    
    Gtk::Frame* const midtcieFrame;
    MyComboBoxText* const midtciemet;
    Adjuster* const midtcie;
    Gtk::CheckButton* const smoothcie;
    Gtk::CheckButton* const smoothcielnk;
    Gtk::CheckButton* const smoothcieinv;
    Gtk::CheckButton* const smoothcietrc;
    Gtk::CheckButton* const smoothcietrcrel;
    Gtk::CheckButton* const smoothcieyb;
    Gtk::CheckButton* const smoothcielum;
    Gtk::CheckButton* const smoothciehigh;
    Adjuster* const smoothcieth;
    ToolParamBlock* const ciesmoothBox;
    Gtk::Box* smoothBox;
    MyComboBoxText* const smoothciemet;
    Adjuster* const slopesmo;
    Adjuster* const slopesmor;
    Adjuster* const slopesmog;
    Adjuster* const slopesmob;
    Adjuster* const kslopesmor;
    Adjuster* const kslopesmog;
    Adjuster* const kslopesmob;
    Adjuster* const smoothciethtrc;
    
    Adjuster* const contsig;
    Adjuster* const skewsig;
    Adjuster* const whitsig;

    Adjuster* const whitescie;
    Adjuster* const blackscie;
    Gtk::Box* willBox;
    MyComboBoxText* const illMethod;
    Gtk::Box* wprimBox;
    MyComboBoxText*  const primMethod;
    Gtk::Grid* primCoordGridl;
    Gtk::Frame* trcFrame;
    Gtk::Frame* smoothFrame;
    Gtk::Frame* primillFrame;
    ToolParamBlock* const redBox;  
    Adjuster* const redxl;
    Adjuster* const redyl;
    Adjuster* const grexl;
    Adjuster* const greyl;
    Adjuster* const bluxl;
    Adjuster* const bluyl;
    Adjuster* const refi;
   
    Gtk::Frame* const gridFramecie;
    LabGrid* const labgridcie;
    Gtk::Frame* const colorFramecie;
   
    Gtk::Box* catBox;
    MyComboBoxText* const catMethod;
    Gtk::Box* gamutcieBox;
    Gtk::CheckButton* const gamutcie;
    Adjuster* const shiftxl;
    Adjuster* const shiftyl;
    Gtk::Box* bwcieBox;
    Gtk::CheckButton* const bwcie;

    Gtk::Frame* const sigmoidjzFrame12;
    Gtk::Frame* const sigmoidjzFrame;
    Gtk::Frame* const sigmoid2Frame12;
    Gtk::Frame* const sigmoid2Frame;
    Gtk::CheckButton* const sigcie;
    Gtk::CheckButton* const sigjz12;
    Adjuster* const sigmoidldajzcie12;
    Adjuster* const sigmoidthjzcie12;
    Adjuster* const sigmoidbljzcie12;

    Gtk::CheckButton* const sigjz;
    Gtk::CheckButton* const forcebw;
    Adjuster* const sigmoidldajzcie;
    Adjuster* const sigmoidthjzcie;
    Adjuster* const sigmoidbljzcie;
    
    Adjuster* const colorflcie;
    Adjuster* const saturlcie;
    Adjuster* const rstprotectcie;
    Adjuster* const chromlcie;
    Adjuster* const huecie;
    CurveEditorGroup* const cieCurveEditorG;
    MyComboBoxText* const toneMethodcie;
    DiagonalCurveEditor* const shapecie;
    CurveEditorGroup* const cieCurveEditorG2;
    MyComboBoxText* const toneMethodcie2;
    DiagonalCurveEditor* const shapecie2;
    
    Adjuster* const chromjzcie;
    Adjuster* const saturjzcie;
    Adjuster* const huejzcie;
    CurveEditorGroup* const jz1CurveEditorG;
    DiagonalCurveEditor* const shapejz;
    DiagonalCurveEditor* const shapecz;

    
    Gtk::Frame* const HFramejz;
    Gtk::Frame* const JzHFramejz;
    CurveEditorGroup* const jz2CurveEditorG;
    CurveEditorGroup* const jz3CurveEditorG;
    DiagonalCurveEditor* const shapeczjz;
    FlatCurveEditor* const HHshapejz;
    FlatCurveEditor* const CHshapejz;
    FlatCurveEditor* const LHshapejz;
    Adjuster* const softjzcie;
    Adjuster* const thrhjzcie;
    Gtk::CheckButton* const chjzcie;
    Adjuster* const strsoftjzcie;
   
    MyExpander* const expLcie;
    Gtk::Frame* const cie2Frame;
    Adjuster* const targetGraycie;
    Adjuster* const targabscie;
    Adjuster* const detailcie;
    Adjuster* const detailciejz;
    Adjuster* const catadcie;
    MyComboBoxText*  const surroundcie;
    Gtk::Box* const surrHBoxcie;

    MyExpander* const expgradcie;
    Adjuster* const strgradcie;
    Adjuster* const anggradcie;
    Adjuster* const feathercie;

    MyExpander* const exprecovcie;
    Gtk::Label* const maskusablecie;
    Gtk::Label* const maskunusablecie;
    Adjuster* const recothrescie;
    Adjuster* const lowthrescie;
    Adjuster* const higthrescie;
    Adjuster* const decaycie;

    MyExpander* const expmaskcie;
    MyComboBoxText* const showmaskcieMethod;
    Gtk::CheckButton* const enacieMask;
    Gtk::CheckButton* const enacieMaskall;
    CurveEditorGroup* const maskcieCurveEditorG;
    FlatCurveEditor* const CCmaskcieshape;
    FlatCurveEditor* const LLmaskcieshape;
    FlatCurveEditor* const HHmaskcieshape;
    Gtk::Frame* const struFramecie;
    Adjuster* const strumaskcie;
    Gtk::CheckButton* const toolcie;
    Gtk::Frame* const blurFramecie;
    Gtk::CheckButton* const fftcieMask;
    Adjuster* const contcie;
    Adjuster* const blurcie;

    Adjuster* const blendmaskcie;
    Adjuster* const radmaskcie;
    Adjuster* const lapmaskcie;
    Adjuster* const chromaskcie;
    Adjuster* const gammaskcie;
    Adjuster* const slomaskcie;
    Adjuster* const highmaskcie;
    Adjuster* const shadmaskcie;
    CurveEditorGroup* const maskcieHCurveEditorG;
    FlatCurveEditor* const HHhmaskcieshape;

    CurveEditorGroup* const mask2cieCurveEditorG;
    DiagonalCurveEditor* const Lmaskcieshape;
    Gtk::Frame* const wavFramecie;   
    CurveEditorGroup* const mask2cieCurveEditorGwav;
    FlatCurveEditor* const LLmaskcieshapewav;
    Gtk::Box* const quaHcieBox;
    ThresholdAdjuster* const csThresholdcie;
    int nextcomprciecount = 0;
   
    sigc::connection AutograycieConn, primMethodconn, illMethodconn, smoothciemetconn, catMethodconn, sigybjz12Conn, qtojConn, showmaskcieMethodConn, enacieMaskConn, enacieMaskallConn, jabcieConn, sursourcieconn, surroundcieconn, modecieconn, modecamconn, modeQJconn, comprcieautoconn, normcie12conn, normcieconn, logcieconn, satcieconn, logcieqconn, smoothcieconn, smoothcielnkconn, smoothcieinvconn, smoothciehighconn, smoothcietrcconn, smoothcietrcrelconn, smoothcieybconn,smoothcielumconn, logjzconn, sigjz12conn, forcebwconn, sigjzconn, sigq12conn, sigqconn, chjzcieconn, toneMethodcieConn, toneMethodcieConn2, toolcieConn, bwevMethod12Conn, midtciemetConn, bwevMethodConn,fftcieMaskConn, gamutcieconn, bwcieconn, expprecamconn, sigcieconn;
    sigc::connection previewcieConn, sigmoidqjcieconn;
public:
    Locallabcie();
    ~Locallabcie();

    void setListener(ToolPanelListener* tpl) override;
   
    bool isMaskViewActive() override;
    void resetMaskView() override;
    void getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask) override;
    int nbmaskcie;
    Gtk::ToggleButton *getPreviewDeltaEButton() const override;
    sigc::connection *getPreviewDeltaEButtonConnection() override;

    void updateAdviceTooltips(const bool showTooltips) override;
    void setDefaultExpanderVisibility() override;
    void updateguicie(int spottype);
    void previewcieChanged();
    void disableListener() override;
    void enableListener() override;
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) override {}; // Not used
//    void adjusterChanged3(ThresholdAdjuster* a, double newBottom, double newTop) override {};
    void adjusterChanged(ThresholdAdjuster* a, double newBottomLeft, double newTopLeft, double newBottomRight, double newTopRight) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) override {}; // Not used
    void adjusterChanged(ThresholdAdjuster* a, int newBottomLeft, int newTopLeft, int newBottomRight, int newTopRight) override {}; // Not used
    void adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR) override;
    void sursourcieChanged();
    void surroundcieChanged();
    void modecieChanged();
    void modecamChanged();
    void modeQJChanged();
    void qjmodall();
    void qjmodjz();
    void qjmodcam();
    void curveChanged(CurveEditor* ce) override;
    void toneMethodcieChanged();
    void toneMethodcie2Changed();
    void bwevMethod12Changed();
    void bwevMethodChanged();
    void midtciemetChanged();
    void updateAutocompute(const float blackev, const float whiteev, const float sourceg, const float sourceab, const float targetg, const float jz1);
    void updatePrimloc(const float redx, const float redy, const float grex, const float grey, const float blux, const float bluy);
    void updateiPrimloc(const float r_x, const float r_y, const float g_x, const float g_y, const float b_x, const float b_y, const float w_x, const float w_y, const float m_x, const float m_y,  const float me_x, const float me_y, const int pri_, const float slg, const bool lkg);
    void updatesigloc(const float cont_sig, const float light_sig);

private:
    void enabledChanged() override;
    void convertParamToNormal() override;
    void convertParamToSimple() override;
    void updateGUIToMode(const modeType new_type) override;
    void complexityModeChanged();
    void AutograycieChanged();
    void sigybjz12Changed();
    void qtojChanged();
    void jabcieChanged();
    void comprcieautoChanged();
    void normcie12Changed();
    void normcieChanged();
    void gamutcieChanged();
    void bwcieChanged();
    void illMethodChanged();
    void smoothciemetChanged();
    void primMethodChanged();
    void catMethodChanged();
    void logcieChanged();
    void satcieChanged();
    void logcieqChanged();
    void smoothcieChanged();
    void smoothcielnkChanged();
    void smoothcieinvChanged();
    void smoothciehighChanged();
    void smoothcietrcChanged();
    void smoothcietrcrelChanged();
    void smoothcieybChanged();
    void smoothcielumChanged();
    void sigcieChanged();
    void logjzChanged();
    void sigjz12Changed();
    void sigjzChanged();
    void forcebwChanged();
    void sigq12Changed();
    void sigqChanged();
    void chjzcieChanged();
    void updatecieGUI();
    void updatecielnkGUI();
    void updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz) override;
    void showmaskcieMethodChanged();
    void enacieMaskChanged();
    void enacieMaskallChanged();
    void enacieMaskallChanged2();
    void guijzczhz();
    void toolcieChanged();
    void fftcieMaskChanged();
    void expprecamChanged();

    float nextrx;
    float nextry;
    float nextbx;
    float nextby;
    float nextgx;
    float nextgy;
    float nextwx;
    float nextwy;
    float nextmx;
    float nextmy;

};

#endif
