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

class MyFileChooserButton;

namespace rtengine
{

class RawImage;

}

class DFProvider
{
public:
    virtual ~DFProvider() = default;
    virtual const rtengine::RawImage* getDF() = 0;
    virtual Glib::ustring GetCurrentImageFilePath() = 0;
    // add other info here
};

class DarkFrame final:
    public ToolParamBlock,
    public FoldableToolPanel
{

protected:

    MyFileChooserButton *darkFrameFile;
    Gtk::Box *hbdf;
    Gtk::Button *btnReset;
    Gtk::Label *dfLabel;
    Gtk::Label *dfInfo;
    Gtk::CheckButton* dfAuto;
    bool dfChanged;
    bool lastDFauto;
    DFProvider *dfp;
    sigc::connection dfautoconn, dfFile;
    bool b_filter_asCurrent;
    bool israw;

public:
    static const Glib::ustring TOOL_NAME;

    DarkFrame ();

    void read           (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write          (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;

    void darkFrameChanged ();
    void darkFrameReset   ();
    void dfAutoChanged    ();
    void setDFProvider    (DFProvider* p)
    {
        dfp = p;
    };
};
