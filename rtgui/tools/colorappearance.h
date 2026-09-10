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

#include <gtkmm.h>

#include "colorprovider.h"
#include "curvelistener.h"
#include "guiutils.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/smartscroll.h"

class DiagonalCurveEditor;
class CurveEditorGroup;
class CurveEditor;
class MyExpander;

class ColorAppearance final :
    public ToolParamBlock,
    public AdjusterListener,
    public FoldableToolPanel,
    public rtengine::AutoCamListener,
    public CurveListener,
    public ColorProvider
{
public:
    static const Glib::ustring TOOL_NAME;

    ColorAppearance ();
    ~ColorAppearance () override;

    void read           (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write          (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults    (const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void setBatchMode   (bool batchMode) override;
    void adjusterChanged     (Adjuster* a, double newval) override;
    void adjusterAutoToggled (Adjuster* a, bool newval) override;
//    void adjusterAdapToggled (Adjuster* a, bool newval);
    void enabledChanged      () override;
    void surroundChanged     ();
    void surrsrcChanged     ();
    void wbmodelChanged      ();
    void illumChanged      ();
    void algoChanged         ();
    void surrsource_toggled  ();
    void gamut_toggled       ();
//   void badpix_toggled       ();
    void datacie_toggled     ();
    void tonecie_toggled     ();
//    void sharpcie_toggled     ();
    void autoCamChanged (double ccam, double ccamout) override;
    bool autoCamComputed_ ();
    void adapCamChanged (double cadap) override;
    void wbCamChanged(double tem, double tin, bool autotemp) override;
    bool adapCamComputed_ ();
    void ybCamChanged (int yb) override;
    bool ybCamComputed_ ();
    void curveChanged        (CurveEditor* ce) override;
    void curveMode1Changed   ();
    bool curveMode1Changed_  ();
    void curveMode2Changed   ();
    bool curveMode2Changed_  ();
    void curveMode3Changed   ();
    bool curveMode3Changed_  ();
    void neutral_pressed       ();
    void complexmethodChanged();
    void modelmethodChanged();
    void catmethodChanged();
    void convertParamToNormal();

    void updateGUIToMode(int mode);

    void expandCurve         (bool isExpanded);
    bool isCurveExpanded     ();
    void autoOpenCurve       () override;

    void setAdjusterBehavior (bool degreeadd, bool adapscenadd, bool adaplumadd, bool badpixsladd, bool jlightadd, bool chromaadd, bool contrastadd, bool rstprotectionadd, bool qbrightadd, bool qcontrastadd, bool schromaadd, bool schromaredadd, bool schromagreenadd, bool schromablueadd, bool mchromaadd, bool colorhadd, bool colorhredadd,  bool colorhgreenadd, bool colorhblueadd, bool degreeoutadd, bool tempoutadd);
    void trimValues          (rtengine::procparams::ProcParams* pp) override;
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
    void colorForValue (double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller *caller) override;
    void updateToolState (std::vector<int> &tpOpen);
    void writeOptions (std::vector<int> &tpOpen);

private:
    rtengine::ProcEvent EvCATAutotempout;
    rtengine::ProcEvent EvCATillum;
    rtengine::ProcEvent EvCATcomplex;
    rtengine::ProcEvent EvCATmodel;
    rtengine::ProcEvent EvCATcat;
    rtengine::ProcEvent EvCATcolorhred;
    rtengine::ProcEvent EvCATschromared;
    rtengine::ProcEvent EvCATcolorhgreen;
    rtengine::ProcEvent EvCATschromagreen;
    rtengine::ProcEvent EvCATcolorhblue;
    rtengine::ProcEvent EvCATschromablue;
    rtengine::ProcEvent EvCATCurvered;
    rtengine::ProcEvent EvCATCurvegreen;
    rtengine::ProcEvent EvCATCurveblue;


    bool bgTTipQuery (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip);
    bool srTTipQuery (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip);
    void foldAllButMe (GdkEventButton* event, MyExpander *expander);

    Glib::RefPtr<Gtk::Tooltip> bgTTips;
    Glib::RefPtr<Gtk::Tooltip> srTTips;
    Glib::RefPtr<Gdk::Pixbuf> bgPixbuf;
    Glib::RefPtr<Gdk::Pixbuf> srPixbuf;
    Gtk::Frame* pRGBFrame;
    Adjuster* degree;
    Adjuster* adapscen;
    Adjuster* ybscen;
    Adjuster* adaplum;
    Adjuster* degreeout;
    Adjuster* badpixsl;
    Adjuster* jlight;
    Adjuster* qbright;
    Adjuster* chroma;
    Adjuster* schroma;
    Adjuster* mchroma;
    Adjuster* rstprotection;
    Adjuster* contrast;
    Adjuster* qcontrast;
    Adjuster* colorh;
    Adjuster* colorhred;
    Adjuster* schromared;
    Adjuster* colorhgreen;
    Adjuster* schromagreen;
    Adjuster* colorhblue;
    Adjuster* schromablue;

    Adjuster* tempout;
    Adjuster* greenout;
    Adjuster* ybout;
    Adjuster* tempsc;
    Adjuster* greensc;

    MyExpander* expadjust;

    MyComboBoxText* toneCurveMode;
    MyComboBoxText* toneCurveMode2;
    MyComboBoxText* toneCurveMode3;
    MyComboBoxText* complexmethod;
    MyComboBoxText* modelmethod;
    MyComboBoxText* catmethod;


    //Adjuster* edge;
    Gtk::CheckButton* surrsource;
    Gtk::CheckButton* gamut;
//   Gtk::CheckButton* badpix;
    Gtk::CheckButton* datacie;
    Gtk::CheckButton* tonecie;
    //  Gtk::CheckButton* sharpcie;
    Gtk::Button* neutral;

    MyComboBoxText* surrsrc;
    sigc::connection  surrsrcconn;

    MyComboBoxText*   surround;
    sigc::connection  surroundconn;
    MyComboBoxText*   wbmodel;
    sigc::connection  wbmodelconn;
    MyComboBoxText*   illum;
    sigc::connection  illumconn;
    MyComboBoxText*   algo;
    sigc::connection  algoconn;
    sigc::connection  surrconn;
    sigc::connection  gamutconn, datacieconn, tonecieconn /*,badpixconn , sharpcieconn*/;
    sigc::connection  tcmodeconn, tcmode2conn, tcmode3conn, neutralconn;
    sigc::connection  complexmethodconn, modelmethodconn, catmethodconn;
    Gtk::Box* alHBox;
    Gtk::Box* wbmHBox;
    Gtk::Box* illumHBox;
    CurveEditorGroup* curveEditorG;
    CurveEditorGroup* curveEditorG2;
    CurveEditorGroup* curveEditorG3;
    CurveEditorGroup* curveEditorGred;
    CurveEditorGroup* curveEditorGgreen;
    CurveEditorGroup* curveEditorGblue;

    DiagonalCurveEditor* shape;
    DiagonalCurveEditor* shape2;
    DiagonalCurveEditor* shape3;
    DiagonalCurveEditor* shapered;
    DiagonalCurveEditor* shapegreen;
    DiagonalCurveEditor* shapeblue;

    double nextCcam, nextCcamout, nextCadap;
    int nextYbscn;
    bool lastAutoDegree;
    bool lastAutoAdapscen;
    bool lastAutoDegreeout;
    bool lastAutoybscen;
    bool lastAutotempout;
    bool lastsurr;
    bool lastgamut;
    bool lastdatacie;
    bool lasttonecie;
    double nexttemp;
    double nextgreen;

    IdleRegister idle_register;
};
