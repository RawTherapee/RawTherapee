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

class IPTCPanel final :
    public Gtk::Box,
    public ToolPanel
{

private:
    const std::unique_ptr<rtengine::procparams::IPTCPairs> changeList;
    const std::unique_ptr<rtengine::procparams::IPTCPairs> defChangeList;
    const std::unique_ptr<rtengine::procparams::IPTCPairs> embeddedData;
    /** Embedded data that RawTherapee does not allow the user to edit. */
    const std::unique_ptr<rtengine::procparams::IPTCPairs> otherEncodedEmbeddedData;
    bool changelist_valid_;
    bool embedded_data_is_utf8 = false;

    Gtk::TextView*  captionView;
    Glib::RefPtr<Gtk::TextBuffer> captionText;
    Gtk::Entry*     captionWriter;
    Gtk::Entry*     headline;
    Gtk::Entry*     instructions;
    MyComboBoxText* keyword;
    Gtk::ListViewText*  keywords;
    Gtk::Button*    addKW;
    Gtk::Button*    delKW;
    MyComboBoxText* category;
    MyComboBoxText* suppCategory;
    Gtk::ListViewText*      suppCategories;
    Gtk::Button*    addSC;
    Gtk::Button*    delSC;

    Gtk::Entry*     creator;
    Gtk::Entry*     creatorJobTitle;
    Gtk::Entry*     credit;
    Gtk::Entry*     source;
    Gtk::Entry*     copyright;
    Gtk::Entry*     city;
    Gtk::Entry*     province;
    Gtk::Entry*     country;
    Gtk::Entry*     title;
    Gtk::Entry*     dateCreated;
    Gtk::Entry*     transReference;

    Gtk::Button*    reset;
    Gtk::Button*    file;
    Gtk::Button*    copy;
    Gtk::Button*    paste;

    sigc::connection conns[16];

    void applyChangeList ();
    void updateChangeList ();

public:
    IPTCPanel ();

    void read           (const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write          (rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults    (const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;

    void setImageData   (const rtengine::FramesMetaData* id);

    void notifyListener ();

    void addKeyWord     ();
    void delKeyWord     ();
    void addSuppCategory ();
    void delSuppCategory ();

    void resetClicked   ();
    void fileClicked    ();
    void copyClicked    ();
    void pasteClicked   ();
};
