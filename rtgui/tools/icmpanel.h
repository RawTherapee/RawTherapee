/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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

#include <memory>

#include <gtkmm.h>

#include "guiutils.h"
#include "toolpanel.h"
#include "curvelistener.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/popupbutton.h"
#include "widgets/basic/thresholdadjuster.h"

#include "rtengine/imagedata.h"

class ICMPanelListener
{
public:
    virtual ~ICMPanelListener() = default;
    virtual void saveInputICCReference(const Glib::ustring& fname, bool apply_wb) = 0;
};

class LabGrid;

class CurveEditor;
class CurveEditorGroup;
class DiagonalCurveEditor;
class EditDataProvider;
class FlatCurveEditor;

class ICMPanel final :
    public ToolParamBlock,
    public CurveListener,
    public FoldableToolPanel,
    public rtengine::AutoprimListener,
    public AdjusterListener
{

protected:
    Gtk::Frame* dcpFrame;
    Gtk::Frame* coipFrame;
    Gtk::Frame* redFrame;
    Gtk::Frame* colorFramecie;    
    MyExpander* trcExp;
    MyExpander* wavExp;
    MyExpander* wav2Exp;
    MyExpander* primExp;

    Adjuster* wGamma;
    Adjuster* wSlope;
    Adjuster* wapsat;
    
    Adjuster* wmidtcie;
    Gtk::CheckButton* wsmoothcie;
    Adjuster* wsmoothciesli;
    Adjuster* sigmatrc;
    Adjuster* offstrc;
    Adjuster* pyrwavtrc;
    Adjuster* residtrc;
    Gtk::Label* trcmaxdata;
    Gtk::Label* rgbmaxdata;
    Gtk::Label* satmaxdata;

    std::unique_ptr<CurveEditorGroup> opacityCurveEditorWLI;
    FlatCurveEditor* opacityShapeWLI;

    Adjuster* redx;
    Adjuster* redy;
    Adjuster* grex;
    Adjuster* grey;
    Adjuster* blux;
    Adjuster* bluy;
    Adjuster* redrot;
    Adjuster* redsat;
    Adjuster* grerot;
    Adjuster* gresat;
    Adjuster* blurot;
    Adjuster* blusat;

    Adjuster* preser;
    Adjuster* refi;
    Adjuster* shiftx;
    Adjuster* shifty;
    sigc::connection wsmoothcieconn;
    bool lastwsmoothcie;
    Gtk::Label* labmga;
    Gtk::Box* gabox;
    //Gtk::Label* blr;
    //Gtk::Label* blg;
    //Gtk::Label* blb;
    Gtk::Button* neutral;
    sigc::connection trcExpconn;
    sigc::connection wavExpconn;

    sigc::connection neutralconn;
    bool lastToneCurve;
    sigc::connection tcurveconn;
    bool lastApplyLookTable;
    sigc::connection ltableconn;
    bool lastApplyBaselineExposureOffset;
    sigc::connection beoconn;
    bool lastApplyHueSatMap;
    sigc::connection hsmconn;
    bool lastobpc;
    sigc::connection obpcconn;
    bool lastfbw;
    sigc::connection fbwconn;
    bool isBatchMode;
    bool lastgamut;
    sigc::connection gamutconn;

private:
    rtengine::ProcEvent EvICMprimariMethod;
    rtengine::ProcEvent EvICMprofileMethod;
    rtengine::ProcEvent EvICMtempMethod;
    //rtengine::ProcEvent EvICMpredx;
    //rtengine::ProcEvent EvICMpredy;
    //rtengine::ProcEvent EvICMpgrex;
    //rtengine::ProcEvent EvICMpgrey;
    //rtengine::ProcEvent EvICMpblux;
    //rtengine::ProcEvent EvICMpbluy;
    rtengine::ProcEvent EvICMgamm;
    rtengine::ProcEvent EvICMslop;
    rtengine::ProcEvent EvICMtrcinMethod;
    rtengine::ProcEvent EvICMwillMethod;
    rtengine::ProcEvent EvICMwprimMethod;
    rtengine::ProcEvent EvICMredx;
    rtengine::ProcEvent EvICMredy;
    rtengine::ProcEvent EvICMgrex;
    rtengine::ProcEvent EvICMgrey;
    rtengine::ProcEvent EvICMblux;
    rtengine::ProcEvent EvICMbluy;
    rtengine::ProcEvent EvICMredrot;
    rtengine::ProcEvent EvICMredsat;
    rtengine::ProcEvent EvICMgrerot;
    rtengine::ProcEvent EvICMgresat;
    rtengine::ProcEvent EvICMblurot;
    rtengine::ProcEvent EvICMblusat;
    
    rtengine::ProcEvent EvaIntent;
    rtengine::ProcEvent EvICMpreser;
    rtengine::ProcEvent EvICMLabGridciexy;
    rtengine::ProcEvent EvICMfbw;
    rtengine::ProcEvent EvICMgamut;
    rtengine::ProcEvent EvICMcat;
    rtengine::ProcEvent EvICMrefi;
    rtengine::ProcEvent EvICMtrcExp;
    rtengine::ProcEvent EvICMshiftx;
    rtengine::ProcEvent EvICMshifty;
    rtengine::ProcEvent EvICMwmidtcie;
    rtengine::ProcEvent EvICMwsmoothcie;
    rtengine::ProcEvent EvICMwapsat;
    rtengine::ProcEvent EvICMwsmoothciesli;
    rtengine::ProcEvent EvICMsigmatrc;
    rtengine::ProcEvent EvICMoffstrc;
    rtengine::ProcEvent EvICMopacityWLI;
    rtengine::ProcEvent EvICMpyrwavtrc;
    rtengine::ProcEvent EvICMresidtrc;
    rtengine::ProcEvent EvICMwavExp;
    rtengine::ProcEvent EvICMwgamut;
    rtengine::ProcEvent EvICMwgampower;
    rtengine::ProcEvent EvOPaperwhiteNits;
    rtengine::ProcEvent EvOMaxNits;
    rtengine::ProcEvent EvICMwgamgain;

    LabGrid *labgridcie;
    IdleRegister idle_register;

    Gtk::Box* willuBox;
    Gtk::Label* willulab;
    Gtk::Box* wprimBox;
    Gtk::Label* wprimlab;
    Gtk::Label* cielab;
    Gtk::Grid* primCoordGrid;
    Gtk::Grid* primCoordGrid2;
    Gtk::Box* riaHBox;
    Gtk::Box* preBox;
    Gtk::Box* iVBox;
    Gtk::Box* wTRCBox;
    Gtk::Frame* gamutcomp;

    Gtk::Box* wgamutBox;
    Gtk::Label* wgamutlab;

    Gtk::CheckButton* fbw;
    Gtk::CheckButton* gamut;

    Gtk::Box* wcatBox;
    Gtk::Label* wcatlab;


    Gtk::CheckButton* obpc;
    Gtk::RadioButton* inone;

    Gtk::RadioButton* iembedded;
    Gtk::RadioButton* icamera;
    Gtk::RadioButton* icameraICC;
    Gtk::RadioButton* ifromfile;
    Gtk::Label* dcpIllLabel;
    MyComboBoxText* dcpIll;
    sigc::connection dcpillconn;
    Gtk::CheckButton* ckbToneCurve;
    Gtk::CheckButton* ckbApplyLookTable;
    Gtk::CheckButton* ckbApplyBaselineExposureOffset;
    Gtk::CheckButton* ckbApplyHueSatMap;
    MyComboBoxText* wProfNames;
    sigc::connection wprofnamesconn;
    MyComboBoxText* wTRC;
    sigc::connection wtrcconn;
    MyComboBoxText* wgamut;
    sigc::connection wgamutconn;
    Adjuster* wgampower;
    Adjuster* wgamgain;

    MyComboBoxText* will;
    sigc::connection willconn;
    MyComboBoxText* wprim;
    sigc::connection wprimconn;
    MyComboBoxText* wcat;
    sigc::connection wcatconn;

    std::unique_ptr<PopUpButton> aRendIntent;
    sigc::connection arendintentconn;

    MyComboBoxText* oProfNames;
    sigc::connection oprofnamesconn;
    Gtk::Box* oNitsBox;
    Adjuster* oPaperwhiteNits;
    Adjuster* oMaxNits;
    std::unique_ptr<PopUpButton> oRendIntent;
    sigc::connection orendintentconn;
    Gtk::RadioButton* iunchanged;
    MyFileChooserButton* ipDialog;
    Gtk::RadioButton::Group opts;
    Gtk::Button* saveRef;
    sigc::connection ipc;
    Glib::ustring oldip;
    ICMPanelListener* icmplistener;

    double dcpTemperatures[2];
    Glib::ustring lastRefFilename;
    Glib::ustring camName;
    Glib::ustring filename;
    void updateDCP(int dcpIlluminant, Glib::ustring dcp_name);
    void updateRenderingIntent(const Glib::ustring &profile);
    void updateAbsoluteLuminance(const Glib::ustring &profile);
    void foldAllButMe(GdkEventButton *event, MyExpander *expander, const MyExpander *parent);

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
    Gtk::Label* wavlocLabels;

public:
    static const Glib::ustring TOOL_NAME;

    ICMPanel();
    ~ICMPanel() override;

    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setBatchMode(bool batchMode) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void adjusterChanged(Adjuster* a, double newval) override;
    void primChanged (float rx, float ry, float bx, float by, float gx, float gy) override;
    void iprimChanged (float r_x, float r_y, float b_x, float b_y, float g_x, float g_y, float w_x, float w_y, float m_x, float m_y) override;
    void maxdatawtrc(float m_data) override;
    void maxdataend(float m_rgb, float m_sat, bool gamgain) override;

    void neutral_pressed();
    void curveChanged(CurveEditor* ce) override;
    void wavlocChanged(double nlevel, double nmax, bool curveloc) override;

    void wpChanged();
    void wtrcinChanged();
    void wgamutChanged();
    void willChanged();
    void wprimChanged();
    void wcatChanged();
    void trcExpChanged();
    void wavExpChanged();
    void opChanged();
    void oiChanged(int n);
    void aiChanged(int n);
    void oBPCChanged();
    void fbwChanged();
    void wsmoothcieChanged();
    void resetpolar();
    void gamutChanged();
    void ipChanged();
    void ipSelectionChanged();
    void dcpIlluminantChanged();
    void toneCurveChanged();
    void applyLookTableChanged();
    void applyBaselineExposureOffsetChanged();
    void applyHueSatMapChanged();
    void upgateGUI_lin_pol_graph();
    void setRawMeta(bool raw, const rtengine::FramesData* pMeta);
    void saveReferencePressed();
    void setListener(ToolPanelListener* tpl) override;
    void setEditProvider(EditDataProvider *provider) override;

    void setICMPanelListener(ICMPanelListener* ipl)
    {
        icmplistener = ipl;
    }

};

