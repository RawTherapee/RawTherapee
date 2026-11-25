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
#include <functional>

#include "saveasdlg.h"

#include "guiutils.h"
#include "multilangmgr.h"
#include "pathutils.h"
#include "rtimage.h"

#include "rtengine/utils.h"

namespace
{

Glib::ustring getCurrentFilename(const Gtk::FileChooserWidget* fchooser)
{
    Glib::ustring res = fchooser->get_filename();

    // NB: There seem to be a bug in Gtkmm2.22 / FileChooserWidget : if you suppress the filename entry and
    //     click on a folder in the list, the filename field is empty but get_filename will return the folder's path :/
    if (Glib::file_test(res, Glib::FILE_TEST_IS_DIR)) {
        res = fchooser->get_current_name();
    }

    return res;
}

}

SaveAsDialog::SaveAsDialog (const Glib::ustring &initialDir, Gtk::Window* parent)
    : Gtk::Dialog (M("GENERAL_SAVE"), *parent)
{
    Gtk::Box* box = get_content_area ();

    fchooser = Gtk::manage( new Gtk::FileChooserWidget (Gtk::FILE_CHOOSER_ACTION_SAVE) );
    fchooser->set_current_folder (initialDir);
    fchooser->signal_file_activated().connect(sigc::mem_fun(*this, &SaveAsDialog::okPressed));

    filter_jpg = Gtk::FileFilter::create();
    filter_jpg->set_name(M("SAVEDLG_JPGFILTER"));
    filter_jpg->add_pattern("*.jpg");
    filter_jpg->add_pattern("*.JPG");
    filter_jpg->add_pattern("*.jpeg");
    filter_jpg->add_pattern("*.JPEG");
    filter_jpg->add_pattern("*.jpe");
    filter_jpg->add_pattern("*.JPE");

    filter_tif = Gtk::FileFilter::create();
    filter_tif->set_name(M("SAVEDLG_JPGFILTER"));
    filter_tif->add_pattern("*.tif");
    filter_tif->add_pattern("*.TIF");
    filter_tif->add_pattern("*.tiff");
    filter_tif->add_pattern("*.TIFF");

    filter_png = Gtk::FileFilter::create();
    filter_png->set_name(M("SAVEDLG_JPGFILTER"));
    filter_png->add_pattern("*.png");
    filter_png->add_pattern("*.PNG");

    filter_avif = Gtk::FileFilter::create();
    filter_avif->set_name(M("SAVEDLG_JPGFILTER"));
    filter_avif->add_pattern("*.avif");
    filter_avif->add_pattern("*.AVIF");

    const auto& options = App::get().options();
    formatChanged (options.saveFormat.format);

// Output Options
// ~~~~~~~~~~~~~~
    formatOpts = Gtk::manage( new SaveFormatPanel () );
    setExpandAlignProperties(formatOpts, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    formatOpts->setListener (this);

// queue/immediate
// ~~~~~~~~~~~~~~~
    saveMethod[0]  = Gtk::manage( new Gtk::RadioButton (M("SAVEDLG_SAVEIMMEDIATELY")) );
    saveMethod[1]  = Gtk::manage( new Gtk::RadioButton (M("SAVEDLG_PUTTOQUEUEHEAD")) );
    saveMethod[2]  = Gtk::manage( new Gtk::RadioButton (M("SAVEDLG_PUTTOQUEUETAIL")) );

    Gtk::RadioButton::Group g = saveMethod[0]->get_group();
    saveMethod[1]->set_group (g);
    saveMethod[2]->set_group (g);

    if (options.saveMethodNum >= 0 && options.saveMethodNum < 3) {
        saveMethod[options.saveMethodNum]->set_active (true);
    }

    saveMethod[0]->signal_clicked().connect( sigc::mem_fun(*this, &SaveAsDialog::saveImmediatlyClicked) );
    saveMethod[1]->signal_clicked().connect( sigc::mem_fun(*this, &SaveAsDialog::putToQueueClicked) );
    saveMethod[2]->signal_clicked().connect( sigc::mem_fun(*this, &SaveAsDialog::putToQueueClicked) );

// Force output format option
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
    forceFormatOpts = Gtk::manage( new Gtk::CheckButton (M("SAVEDLG_FORCEFORMATOPTS")) );
    forceFormatOpts->set_active(options.forceFormatOpts);
    forceFormatOpts->set_sensitive(options.saveMethodNum > 0);
    forceFormatOpts->signal_clicked().connect( sigc::mem_fun(*this, &SaveAsDialog::forceFmtOptsSwitched) );
    // update sensitivity of the SaveFormatPanel
    formatOpts->set_sensitive(options.saveMethodNum == 0 || options.forceFormatOpts);

// Unique filename option
// ~~~~~~~~~~~~~~~~~~~~~~
    autoSuffix = Gtk::manage( new Gtk::CheckButton (M("SAVEDLG_AUTOSUFFIX")) );
    autoSuffix->set_active(options.autoSuffix);

// buttons
// ~~~~~~~
    Gtk::Button* ok     = Gtk::manage( new Gtk::Button (M("GENERAL_OK")) );
    Gtk::Button* cancel = Gtk::manage( new Gtk::Button (M("GENERAL_CANCEL")) );

    ok->set_tooltip_markup (M("TP_SAVEDIALOG_OK_TOOLTIP"));

    ok->signal_clicked().connect( sigc::mem_fun(*this, &SaveAsDialog::okPressed) );
    cancel->signal_clicked().connect( sigc::mem_fun(*this, &SaveAsDialog::cancelPressed) );

// pack everything
// ~~~~~~~~~~~~~~~
    Gtk::Box* vbox_bottomRight = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    // There is no queue in simple mode, so no need to choose
    if (!App::get().isSimpleEditor()) {
        vbox_bottomRight->pack_start (*saveMethod[0], Gtk::PACK_SHRINK, 2);
        vbox_bottomRight->pack_start (*saveMethod[1], Gtk::PACK_SHRINK, 2);
        vbox_bottomRight->pack_start (*saveMethod[2], Gtk::PACK_SHRINK, 2);
        vbox_bottomRight->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK, 5);
    }

    vbox_bottomRight->pack_start (*forceFormatOpts, Gtk::PACK_SHRINK, 4);
    vbox_bottomRight->pack_start (*autoSuffix, Gtk::PACK_SHRINK, 4);

    Gtk::Box* hbox_bottom = Gtk::manage( new Gtk::Box() );
    hbox_bottom->pack_start (*formatOpts, Gtk::PACK_EXPAND_WIDGET, 2);
    hbox_bottom->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK, 2);
    hbox_bottom->pack_start (*vbox_bottomRight, Gtk::PACK_EXPAND_WIDGET, 2);

    box->pack_start (*fchooser);
    box->pack_start (*hbox_bottom, Gtk::PACK_SHRINK, 2);

    get_action_area()->pack_end (*ok, Gtk::PACK_SHRINK, 4);
    get_action_area()->pack_end (*cancel, Gtk::PACK_SHRINK, 4);

    show_all_children ();

    formatOpts->init (options.saveFormat);

    signal_key_press_event().connect( sigc::mem_fun(*this, &SaveAsDialog::keyPressed) );
}

void SaveAsDialog::saveImmediatlyClicked ()
{
    forceFormatOpts->set_sensitive(false);
    formatOpts->set_sensitive(true);
}

void SaveAsDialog::putToQueueClicked ()
{
    forceFormatOpts->set_sensitive(true);
    formatOpts->set_sensitive(forceFormatOpts->get_active());
}

void SaveAsDialog::forceFmtOptsSwitched ()
{
    formatOpts->set_sensitive(forceFormatOpts->get_active());
}

bool SaveAsDialog::getForceFormatOpts ()
{

    return forceFormatOpts->get_active();
}

bool SaveAsDialog::getAutoSuffix ()
{

    return autoSuffix->get_active();
}

bool SaveAsDialog::getImmediately ()
{

    return App::get().isSimpleEditor() ? true : saveMethod[0]->get_active();
}

bool SaveAsDialog::getToHeadOfQueue ()
{

    return saveMethod[1]->get_active ();
}

bool SaveAsDialog::getToTailOfQueue ()
{

    return saveMethod[2]->get_active ();
}

int SaveAsDialog::getSaveMethodNum ()
{
    if (App::get().isSimpleEditor()) {
        return 0;
    }

    for (int i = 0; i < 3; i++)
        if (saveMethod[i]->get_active()) {
            return i;
        }

    return -1;
}

Glib::ustring SaveAsDialog::getFileName ()
{

    return fname;
}

Glib::ustring SaveAsDialog::getDirectory ()
{

    return fchooser->get_current_folder ();
}

SaveFormat SaveAsDialog::getFormat ()
{

    return formatOpts->getFormat ();
}

void SaveAsDialog::okPressed ()
{
    fname = getCurrentFilename(fchooser);

    // Checking if the filename field is empty. The user have to click Cancel if he don't want to specify a filename
    if (fname.empty()) {
        Gtk::MessageDialog(
            *this,
            Glib::ustring("<b>")
                + M("MAIN_MSG_EMPTYFILENAME")
                + "</b>",
            true,
            Gtk::MESSAGE_WARNING,
            Gtk::BUTTONS_OK,
            true
        ).run();
        return;
    }

    if (getExtension(fname).empty()) {
        // Extension is either empty or unfamiliar
        fname += '.' + formatOpts->getFormat().format;
    } else if (
        (
            formatOpts->getFormat().format == "jpg"
            && !rtengine::hasJpegExtension(fname)
        )
        || (
            formatOpts->getFormat().format == "tif"
            && !rtengine::hasTiffExtension(fname)
        )
        || (
            formatOpts->getFormat().format == "png"
            && !rtengine::hasPngExtension(fname)
        )
        || (
            formatOpts->getFormat().format == "avif"
            && !rtengine::hasAvifExtension(fname)
        )
    ) {
        // Create dialog to warn user that the filename may have two extensions on the end
        Gtk::MessageDialog msgd(
            *this,
            Glib::ustring("<b>")
                + M("GENERAL_WARNING")
                + ": "
                + M("SAVEDLG_WARNFILENAME")
                + " \""
                + escapeHtmlChars(Glib::path_get_basename (fname))
                + '.'
                + escapeHtmlChars(formatOpts->getFormat().format)
                + "\"</b>",
            true,
            Gtk::MESSAGE_WARNING,
            Gtk::BUTTONS_OK_CANCEL,
            true
        );

        if (msgd.run() == Gtk::RESPONSE_OK) {
            fname += "." + formatOpts->getFormat().format;
        } else {
            return;
        }
    }

    response (Gtk::RESPONSE_OK);
}

void SaveAsDialog::cancelPressed ()
{
    response (Gtk::RESPONSE_CANCEL);
}

void SaveAsDialog::formatChanged(const Glib::ustring& format)
{
    const auto sanitize_suffix =
        [this, format](const std::function<bool (const Glib::ustring&)>& has_suffix)
        {
            const Glib::ustring name = getCurrentFilename(fchooser);

            if (!has_suffix(name)) {
                fchooser->set_current_name(removeExtension(Glib::path_get_basename(name)) + '.' + format);
            }
        };

    if (format == "jpg") {
        fchooser->set_filter (filter_jpg);
        sanitize_suffix(
            [](const Glib::ustring& filename)
            {
                return rtengine::hasJpegExtension(filename);
            }
        );
    } else if (format == "png") {
        fchooser->set_filter (filter_png);
        sanitize_suffix(
            [](const Glib::ustring& filename)
            {
                return rtengine::hasPngExtension(filename);
            }
        );
    } else if (format == "tif") {
        fchooser->set_filter (filter_tif);
        sanitize_suffix(
            [](const Glib::ustring& filename)
            {
                return rtengine::hasTiffExtension(filename);
            }
        );
    } else if (format == "avif") {
        fchooser->set_filter (filter_avif);
        sanitize_suffix(
            [](const Glib::ustring& filename)
            {
                return rtengine::hasAvifExtension(filename);
            }
        );
    }
}

void SaveAsDialog::setInitialFileName (const Glib::ustring& fname)
{
    this->fname = fname;
    fchooser->set_current_name(fname);
}

void SaveAsDialog::setImagePath (const Glib::ustring& imagePath)
{
    const auto dirName = Glib::path_get_dirname (imagePath);

    try {
        fchooser->add_shortcut_folder (dirName);
    } catch (Glib::Error&) {}
}


bool SaveAsDialog::keyPressed (GdkEventKey* event)
{

    bool ctrl = event->state & GDK_CONTROL_MASK;

    if (ctrl) {
        switch(event->keyval) {
        case GDK_KEY_Return:  // Ctrl-Enter equivalent to pressing OK button
        case GDK_KEY_KP_Enter:
            SaveAsDialog::okPressed();
            return true;
        }
    }

    return false;
}
