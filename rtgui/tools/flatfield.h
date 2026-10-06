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
#include "widgets/basic/adjuster.h"
#include "widgets/basic/checkbox.h"

class MyFileChooserButton;

namespace rtengine
{

class RawImage;

}

class FFProvider
{
public:
    virtual ~FFProvider() {}
    virtual rtengine::RawImage* getFF() = 0;
    virtual Glib::ustring GetCurrentImageFilePath() = 0;
    // add other info here
};

class FlatField final : public ToolParamBlock, public AdjusterListener, public CheckBoxListener, public FoldableToolPanel, public rtengine::FlatFieldAutoClipListener
{

protected:

    MyFileChooserButton *flatFieldFile;
    Gtk::Label *ffLabel;
    Gtk::Label *ffInfo;
    Gtk::Button *flatFieldFileReset;
    Gtk::CheckButton* flatFieldAutoSelect;
    CheckBox* flatFieldFromMetaData;
    Adjuster* flatFieldClipControl;
    Adjuster* flatFieldBlurRadius;
    MyComboBoxText* flatFieldBlurType;
    Gtk::Box *hbff;
    bool ffChanged;
    bool lastFFAutoSelect;
    bool lastFFAutoClipCtrl;
    FFProvider *ffp;
    sigc::connection flatFieldFileconn, flatFieldAutoSelectconn, flatFieldBlurTypeconn;
    Glib::ustring lastShortcutPath;
    bool b_filter_asCurrent;
    bool israw;
    rtengine::ProcEvent EvFlatFieldFromMetaData;

    IdleRegister idle_register;

public:
    static const Glib::ustring TOOL_NAME;

    FlatField ();
    ~FlatField () override;

    void read                (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write               (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setBatchMode        (bool batchMode) override;
    void setAdjusterBehavior (bool clipctrladd);
    void trimValues          (rtengine::procparams::ProcParams* pp) override;
    void setDefaults         (const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;

    void adjusterChanged            (Adjuster* a, double newval) override;
    void adjusterAutoToggled        (Adjuster* a, bool newval) override;
    void flatFieldFileChanged       ();
    void flatFieldFile_Reset        ();
    void flatFieldAutoSelectChanged ();
    void flatFieldBlurTypeChanged   ();
    void setShortcutPath (const Glib::ustring& path);
    void setFFProvider              (FFProvider* p)
    {
        ffp = p;
    };
    void flatFieldAutoClipValueChanged(int n = 0) override;
    void checkBoxToggled(CheckBox* c, CheckValue newval) override;
    void setGainMap(bool enabled);
};
