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

#include "saveformatpanel.h"

class SaveAsDialog final :
    public Gtk::Dialog,
    public FormatChangeListener
{

protected:
    Gtk::FileChooserWidget* fchooser;
    Gtk::CheckButton* autoSuffix;
    Gtk::CheckButton* forceFormatOpts;
    SaveFormatPanel* formatOpts;
    Glib::ustring fname;
    Glib::RefPtr<Gtk::FileFilter> filter_jpg;
    Glib::RefPtr<Gtk::FileFilter> filter_tif;
    Glib::RefPtr<Gtk::FileFilter> filter_png;
    Glib::RefPtr<Gtk::FileFilter> filter_avif;
    Gtk::RadioButton* saveMethod[3]; /*  0 -> immediately
                                      *  1 -> putToQueueHead
                                      *  2 -> putToQueueTail
                                      */
    void  forceFmtOptsSwitched ();
    void  saveImmediatlyClicked ();
    void  putToQueueClicked ();

public:
    explicit SaveAsDialog (const Glib::ustring &initialDir, Gtk::Window* parent);

    Glib::ustring   getFileName        ();
    Glib::ustring   getDirectory       ();
    SaveFormat      getFormat          ();
    bool            getForceFormatOpts ();
    bool            getAutoSuffix      ();
    bool            getImmediately     ();
    bool            getToHeadOfQueue   ();
    bool            getToTailOfQueue   ();
    int             getSaveMethodNum   ();

    void  setInitialFileName (const Glib::ustring& iname);
    void  setImagePath (const Glib::ustring& imagePath);

    void okPressed ();
    void cancelPressed ();
    void formatChanged(const Glib::ustring& format) override;
    bool keyPressed (GdkEventKey* event);
};
