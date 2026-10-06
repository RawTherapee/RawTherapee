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
#include "widgets/basic/thresholdadjuster.h"
#include "widgets/curves/curveeditorgroup.h"

class CurveEditor;
class CurveEditorGroup;
class DiagonalCurveEditor;
class FlatCurveEditor;
class MyExpander;

class Retinex final :
    public ToolParamBlock,
    public FoldableToolPanel,
    public rtengine::RetinexListener,
    public CurveListener,
    public AdjusterListener,
    public ColorProvider
{
private:
    IdleRegister idle_register;
    rtengine::ProcEvent EvReticomplex;

protected:
    CurveEditorGroup* curveEditorGD;
    CurveEditorGroup* curveEditorGDH;
    CurveEditorGroup* curveEditorGH;
    CurveEditorGroup* curveEditormap;
    Adjuster* str;
    Adjuster* scal;
    Adjuster* grad;
    Adjuster* grads;
    Adjuster* iter;
    Adjuster* neigh;
    Adjuster* offs;
    Adjuster* vart;
    Adjuster* limd;
    Adjuster* highl;
    Adjuster* skal;
    Adjuster* gam;
    Adjuster* slope;
    Adjuster* highlights;
    Adjuster* h_tonalwidth;
    Adjuster* shadows;
    Adjuster* s_tonalwidth;
    Adjuster* radius;

    MyExpander* expsettings;

    Gtk::Label* labmdh;
    Gtk::Grid* dhgrid;
    Gtk::Grid* mapgrid;
    Gtk::Label* labmap;
    Gtk::Grid* viewgrid;
    Gtk::Label* labview;

    Gtk::Label* labgam;
    Gtk::Grid* gamgrid;
    Gtk::Button* neutral;

    MyComboBoxText*   retinexMethod;
    MyComboBoxText*   retinexcolorspace;
    MyComboBoxText*   gammaretinex;
    MyComboBoxText*   mapMethod;
    MyComboBoxText*   viewMethod;
    Gtk::CheckButton* medianmap;
    MyComboBoxText* complexmethod;
    sigc::connection  complexmethodconn;
    
    double nextmin;
    double nextmax;
    double nextminiT;
    double nextmaxiT;
    double nextmeanT;
    double nextminT;
    double nextmaxT;
    double nextsigma;

    Gtk::Label* mMLabels;
    Gtk::Label* transLabels;
    Gtk::Label* transLabels2;
    Gtk::Frame *gainFrame;
    Gtk::Frame *tranFrame;
    Gtk::Frame *iterFrame;
    Gtk::Frame *maskFrame;
    Gtk::Frame *equalFrame;

    DiagonalCurveEditor* cdshape;
    DiagonalCurveEditor* cdshapeH;
    DiagonalCurveEditor* mapshape;
    CurveEditorGroup* transmissionCurveEditorG;
    CurveEditorGroup* gaintransmissionCurve;
    sigc::connection retinexMethodConn, neutralconn, mapMethodConn, viewMethodConn;
    sigc::connection retinexColorSpaceConn;
    sigc::connection gammaretinexConn;
    FlatCurveEditor* transmissionShape;
    FlatCurveEditor* gaintransmissionShape;
    FlatCurveEditor* lhshape;
    bool lastmedianmap;
    sigc::connection medianmapConn;

public:
    static const Glib::ustring TOOL_NAME;

    Retinex();
    ~Retinex() override;

    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setBatchMode(bool batchMode) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void trimValues(rtengine::procparams::ProcParams* pp) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void autoOpenCurve() override;
    void medianmapChanged();
    void minmaxChanged(double cdma, double cdmin, double mini, double maxi, double Tmean, double Tsigma, double Tmin, double Tmax) override;
    void updateLabel();
    void updateTrans();
    void neutral_pressed();

    void enabledChanged() override;
    void curveChanged(CurveEditor* ce) override;
    void retinexMethodChanged();
    void mapMethodChanged();
    void viewMethodChanged();
    void retinexColorSpaceChanged();
    void gammaretinexChanged();
    void ColorSpaceUpdateUI();
    void writeOptions(std::vector<int> &tpOpen);
    void updateToolState(const std::vector<int>& tpOpen);
    void setAdjusterBehavior (bool strAdd, bool neighAdd, bool limdAdd, bool offsAdd, bool vartAdd, bool gamAdd, bool slopeAdd);
    void updateCurveBackgroundHistogram(
        const LUTu& histToneCurve,
        const LUTu& histLCurve,
        const LUTu& histCCurve,
        const LUTu& histLCAM,
        const LUTu& histCCAM,
        const LUTu& histRed,
        const LUTu& histGreen,
        const LUTu& histBlue,
        const LUTu& histLuma,
        const LUTu& histLRETI
    );

    void colorForValue(double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller* caller) override;

private:
    void foldAllButMe(GdkEventButton* event, MyExpander *expander);
    void convertParamToNormal();
    void updateGUIToMode(int mode);
    void complexmethodChanged();
};
