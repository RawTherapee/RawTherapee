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
class EditDataProvider;
class FlatCurveEditor;

class BlackWhite final :
    public ToolParamBlock,
    public AdjusterListener,
    public FoldableToolPanel,
    public rtengine::AutoBWListener,
    public CurveListener,
    public ColorProvider
{
public:
    static const Glib::ustring TOOL_NAME;

    BlackWhite ();
    ~BlackWhite () override;

    void read            (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write           (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults     (const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void setBatchMode    (bool batchMode) override;
    void autoOpenCurve   () override;
    void setEditProvider (EditDataProvider *provider) override;

    void autoch_toggled  ();
    void neutral_pressed ();

    void updateRGBLabel      ();
    void adjusterChanged     (Adjuster* a, double newval) override;
    void setAdjusterBehavior (bool bwadd, bool bwgadd);
    void trimValues          (rtengine::procparams::ProcParams* pp) override;
    void enabledcc_toggled   ();
    void enabledChanged      () override;
    void methodChanged       ();
    void filterChanged       ();
    void settingChanged      ();
    void colorForValue (double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller* caller) override;
    void BWChanged           (double redbw, double greenbw, double bluebw) override;
    bool BWComputed_         ();
    void curveChanged        (CurveEditor* ce) override;
    void curveMode1Changed   ();
    bool curveMode1Changed_  ();
    void curveMode1Changed2  ();
    bool curveMode1Changed2_ ();
    void algoChanged         ();

    Glib::ustring getSettingString ();
    Glib::ustring getFilterString  ();
    Glib::ustring getalgoString  ();

private:
    void showLuminance();
    void hideLuminance();
    void showFilter();
    void hideFilter();
    void showEnabledCC();
    void hideEnabledCC();
    void showMixer(int nChannels, bool RGBIsSensitive = true);
    void hideMixer();
    void showGamma();
    void hideGamma();

    FlatCurveEditor*     luminanceCurve;
    Gtk::Separator*     luminanceSep;
    CurveEditorGroup*    luminanceCEG;
    CurveEditorGroup*    beforeCurveCEG;
    DiagonalCurveEditor* beforeCurve;
    MyComboBoxText*      beforeCurveMode;
    CurveEditorGroup*    afterCurveCEG;
    DiagonalCurveEditor* afterCurve;
    MyComboBoxText*      afterCurveMode;
    Gtk::ToggleButton*   autoch;
    Gtk::Box*            autoHBox;
    Gtk::Button*         neutral;
    Gtk::Label*          RGBLabels;
    MyComboBoxText*      algo;
    sigc::connection     algoconn;
    Gtk::Label*          alLabel;
    Gtk::Box*            algoHBox;

    Adjuster *mixerRed;
    Adjuster *mixerGreen;
    Adjuster *mixerBlue;
    Adjuster *gammaRed;
    Adjuster *gammaGreen;
    Adjuster *gammaBlue;
    Adjuster *mixerOrange;
    Adjuster *mixerYellow;
    Adjuster *mixerCyan;
    Adjuster *mixerMagenta;
    Adjuster *mixerPurple;
    MyComboBoxText*   method;
    sigc::connection  methodconn;
    Gtk::Box*         filterHBox;
    Gtk::Separator*  filterSep, *filterSep2;
    MyComboBoxText*   filter;
    sigc::connection  filterconn;
    Gtk::Box*         settingHBox;
    MyComboBoxText*   setting;
    sigc::connection  settingconn;
    Gtk::Frame* mixerFrame;
    Gtk::Box*  mixerVBox;
    Gtk::Frame* gammaFrame;

    Gtk::Image *imgIcon[11];

    Gtk::Separator* enabledccSep;
    Gtk::CheckButton* enabledcc;
    bool lastEnabledcc, lastAuto;
    sigc::connection enaccconn, tcmodeconn, tcmodeconn2, autoconn, neutralconn;

    double nextredbw;
    double nextgreenbw;
    double nextbluebw;
    int nextcount = 0;

    IdleRegister idle_register;
};
