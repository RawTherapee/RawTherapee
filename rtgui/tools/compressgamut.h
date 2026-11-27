/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2024 RawTherapee team
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

#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/smartscroll.h"

class Compressgamut final :
    public ToolParamBlock,
    public AdjusterListener,
    public rtengine::CompgamutListener,
    public FoldableToolPanel
{

protected:
    Adjuster* th_c;
    Adjuster* th_m;
    Adjuster* th_y;
    Adjuster* d_c;
    Adjuster* d_m;
    Adjuster* d_y;
    Gtk::Label* acLabel;
    Gtk::Label* acLabelrgb;
    Gtk::Label* acLabelcmy;  
    Adjuster* pwr;
    bool lastAutodc;
    bool lastAutodm;
    bool lastAutody;
    bool updThrDefaults;    ///< Threshold defaults only available for the Rec2020 working profile

    MyComboBoxText *colorspace;
    sigc::connection colorspaceconn;
    Gtk::CheckButton* rolloff;
    sigc::connection rolloffconn;
    bool lastrolloff;
    rtengine::ProcEvent EvcgColorspace;
    rtengine::ProcEvent Evcgthc;
    rtengine::ProcEvent Evcgthm;
    rtengine::ProcEvent Evcgthy;
    rtengine::ProcEvent Evcgdc;
    rtengine::ProcEvent Evcgdm;
    rtengine::ProcEvent Evcgdy;
    rtengine::ProcEvent Evcgroll;
    rtengine::ProcEvent Evcgpwr;
    rtengine::ProcEvent Evcgenabled;
    rtengine::ProcEvent Evcgdcautoon;
    rtengine::ProcEvent Evcgdmautoon;
    rtengine::ProcEvent Evcgdyautoon;

private:
    IdleRegister idle_register;

public:
    static const Glib::ustring TOOL_NAME;

    Compressgamut ();
    ~Compressgamut() override;


    void read           (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write          (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults    (const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void setBatchMode   (bool batchMode) override;
    void achromaticChanged (double acmax, double acmax0, double acmax1, double acmax2, bool auto_dc, bool auto_dm, bool auto_dy) override;
    void adjusterAutoToggled (Adjuster* a, bool newval) override;   
    void adjusterChanged (Adjuster* a, double newval) override;
    void enabledChanged  () override;
    void rolloff_change();

    void trimValues          (rtengine::procparams::ProcParams* pp) override;
    void colorspaceChanged();
    void updategamutGUI();
};
