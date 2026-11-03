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
#include "placesbrowser.h"

#include "guiutils.h"
#include "rtimage.h"
#include "options.h"
#include "toolpanel.h"

#ifdef _WIN32
#include "rtengine/leanwindows.h"
#include <shlwapi.h>
#include <shlobj.h>
#endif

PlacesBrowser::PlacesBrowser ()
    : refresh_pending(false), is_refreshing(false)
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    scrollw = Gtk::manage (new Gtk::ScrolledWindow ());
    scrollw->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    pack_start (*scrollw);

    // Since Gtk3, we can't have image+text buttons natively. We'll comply to the Gtk guidelines and choose one of them (icons here)
    add = Gtk::manage (new Gtk::Button ());
    add->set_tooltip_text(M("MAIN_FRAME_PLACES_ADD"));
    setExpandAlignProperties(add, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    //add->get_style_context()->set_junction_sides(Gtk::JUNCTION_RIGHT);
    add->get_style_context()->add_class("Left");
    add->set_image (*Gtk::manage (new RTImage ("add-small", Gtk::ICON_SIZE_BUTTON)));
    del = Gtk::manage (new Gtk::Button ());
    del->set_tooltip_text(M("MAIN_FRAME_PLACES_DEL"));
    setExpandAlignProperties(del, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    //del->get_style_context()->set_junction_sides(Gtk::JUNCTION_LEFT);
    del->get_style_context()->add_class("Right");
    del->set_image (*Gtk::manage (new RTImage ("remove-small", Gtk::ICON_SIZE_BUTTON)));
    Gtk::Grid* buttonBox = Gtk::manage (new Gtk::Grid ());
    buttonBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    buttonBox->attach_next_to(*add, Gtk::POS_LEFT, 1, 1);
    buttonBox->attach_next_to(*del, *add, Gtk::POS_RIGHT, 1, 1);

    pack_start (*buttonBox, Gtk::PACK_SHRINK, 2);

    treeView = Gtk::manage (new Gtk::TreeView ());
    treeView->set_can_focus(false);
    scrollw->add (*treeView);

    placesModel = Gtk::ListStore::create (placesColumns);
    treeView->set_model (placesModel);
    treeView->set_headers_visible (true);

    Gtk::TreeView::Column *iviewcol = Gtk::manage (new Gtk::TreeView::Column (M("MAIN_FRAME_PLACES")));
    Gtk::CellRendererPixbuf *iconCR  = Gtk::manage (new Gtk::CellRendererPixbuf());
    Gtk::CellRendererText *labelCR  = Gtk::manage (new Gtk::CellRendererText());
    labelCR->property_ellipsize() = Pango::ELLIPSIZE_MIDDLE;
    iviewcol->pack_start (*iconCR, false);
    iviewcol->pack_start (*labelCR, true);
    iviewcol->add_attribute (*iconCR, "gicon", 0);
    iviewcol->add_attribute (*labelCR, "text", placesColumns.label);
    treeView->append_column (*iviewcol);

    treeView->set_row_separator_func (sigc::mem_fun(*this, &PlacesBrowser::rowSeparatorFunc));

    vm = Gio::VolumeMonitor::get();

    vm->signal_mount_changed().connect (sigc::mem_fun(*this, &PlacesBrowser::mountChanged));
    vm->signal_mount_added().connect (sigc::mem_fun(*this, &PlacesBrowser::mountChanged));
    vm->signal_mount_removed().connect (sigc::mem_fun(*this, &PlacesBrowser::mountChanged));
    vm->signal_volume_changed().connect (sigc::mem_fun(*this, &PlacesBrowser::volumeChanged));
    vm->signal_volume_added().connect (sigc::mem_fun(*this, &PlacesBrowser::volumeChanged));
    vm->signal_volume_removed().connect (sigc::mem_fun(*this, &PlacesBrowser::volumeChanged));
    vm->signal_drive_connected().connect (sigc::mem_fun(*this, &PlacesBrowser::driveChanged));
    vm->signal_drive_disconnected().connect (sigc::mem_fun(*this, &PlacesBrowser::driveChanged));
    vm->signal_drive_changed().connect (sigc::mem_fun(*this, &PlacesBrowser::driveChanged));

    treeView->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &PlacesBrowser::selectionChanged));
    add->signal_clicked().connect(sigc::mem_fun(*this, &PlacesBrowser::addPressed));
    del->signal_clicked().connect(sigc::mem_fun(*this, &PlacesBrowser::delPressed));

    show_all ();
}

// For drive letter comparison
bool compareMountByRoot (Glib::RefPtr<Gio::Mount> a, Glib::RefPtr<Gio::Mount> b)
{
    return a->get_root()->get_parse_name() < b->get_root()->get_parse_name();
}

void PlacesBrowser::refreshPlacesList ()
{
    is_refreshing = true;
    placesModel->clear ();

    const auto& options = App::get().options();
    // append favorites
    for (size_t i = 0; i < options.favoriteDirs.size(); i++) {
        auto fav = Gio::File::create_for_path (options.favoriteDirs[i]);

        try {
            if (auto info = fav->query_info ()) {
                Gtk::TreeModel::Row newrow = *(placesModel->append());
                newrow[placesColumns.label] = info->get_display_name ();
                newrow[placesColumns.icon]  = info->get_icon ();
                newrow[placesColumns.root]  = fav->get_parse_name ();
                newrow[placesColumns.type]  = 5;
                newrow[placesColumns.rowSeparator] = false;
            }
        } catch(Gio::Error&) {}
    }

    // append home directory
    auto hfile = Gio::File::create_for_path (userHomeDir());  // Will send back "My documents" on Windows now, which has no restricted access

    if (!placesModel->children().empty()) {
        Gtk::TreeModel::Row newrow = *(placesModel->append());
        newrow[placesColumns.rowSeparator] = true;
    }

    try {
        if (auto info = hfile->query_info ()) {
            Gtk::TreeModel::Row newrow = *(placesModel->append());
            newrow[placesColumns.label] = info->get_display_name ();
            newrow[placesColumns.icon]  = info->get_icon ();
            newrow[placesColumns.root]  = hfile->get_parse_name ();
            newrow[placesColumns.type]  = 4;
            newrow[placesColumns.rowSeparator] = false;
        }
    } catch (Gio::Error&) {}

    // append pictures directory
    hfile = Gio::File::create_for_path (userPicturesDir());

    try {
        if (auto info = hfile->query_info ()) {
            Gtk::TreeModel::Row newrow = *(placesModel->append());
            newrow[placesColumns.label] = info->get_display_name ();
            newrow[placesColumns.icon]  = info->get_icon ();
            newrow[placesColumns.root]  = hfile->get_parse_name ();
            newrow[placesColumns.type]  = 4;
            newrow[placesColumns.rowSeparator] = false;
        }
    } catch (Gio::Error&) {}

    if (!placesModel->children().empty()) {
        Gtk::TreeModel::Row newrow = *(placesModel->append());
        newrow[placesColumns.rowSeparator] = true;
    }

    // scan all drives
    std::vector<Glib::RefPtr<Gio::Drive> > drives = vm->get_connected_drives ();

    for (size_t j = 0; j < drives.size (); j++) {
        std::vector<Glib::RefPtr<Gio::Volume> > volumes = drives[j]->get_volumes ();

        if (volumes.empty()) {
            Gtk::TreeModel::Row newrow = *(placesModel->append());
            newrow[placesColumns.label] = drives[j]->get_name ();
            newrow[placesColumns.icon]  = drives[j]->get_icon ();
            newrow[placesColumns.root]  = "";
            newrow[placesColumns.type]  = 3;
            newrow[placesColumns.rowSeparator] = false;
        }

        for (size_t i = 0; i < volumes.size (); i++) {
            Glib::RefPtr<Gio::Mount> mount = volumes[i]->get_mount ();

            if (mount) { // placesed volumes
                Gtk::TreeModel::Row newrow = *(placesModel->append());
                newrow[placesColumns.label] = mount->get_name ();
                newrow[placesColumns.icon]  = mount->get_icon ();
                newrow[placesColumns.root]  = mount->get_root ()->get_parse_name ();
                newrow[placesColumns.type]  = 1;
                newrow[placesColumns.rowSeparator] = false;
            } else { // unplacesed volumes
                Gtk::TreeModel::Row newrow = *(placesModel->append());
                newrow[placesColumns.label] = volumes[i]->get_name ();
                newrow[placesColumns.icon]  = volumes[i]->get_icon ();
                newrow[placesColumns.root]  = "";
                newrow[placesColumns.type]  = 2;
                newrow[placesColumns.rowSeparator] = false;
            }
        }
    }

    // volumes not belonging to drives
    std::vector<Glib::RefPtr<Gio::Volume> > volumes = vm->get_volumes ();

    for (size_t i = 0; i < volumes.size (); i++) {
        if (!volumes[i]->get_drive ()) {
            Glib::RefPtr<Gio::Mount> mount = volumes[i]->get_mount ();

            if (mount) { // placesed volumes
                Gtk::TreeModel::Row newrow = *(placesModel->append());
                newrow[placesColumns.label] = mount->get_name ();
                newrow[placesColumns.icon]  = mount->get_icon ();
                newrow[placesColumns.root]  = mount->get_root ()->get_parse_name ();
                newrow[placesColumns.type]  = 1;
                newrow[placesColumns.rowSeparator] = false;
            } else { // unplacesed volumes
                Gtk::TreeModel::Row newrow = *(placesModel->append());
                newrow[placesColumns.label] = volumes[i]->get_name ();
                newrow[placesColumns.icon]  = volumes[i]->get_icon ();
                newrow[placesColumns.root]  = "";
                newrow[placesColumns.type]  = 2;
                newrow[placesColumns.rowSeparator] = false;
            }
        }
    }

    // places not belonging to volumes
    // (Drives in Windows)
    std::vector<Glib::RefPtr<Gio::Mount> > mounts = vm->get_mounts ();

#ifdef _WIN32
    // on Windows, it's usual to sort by drive letter, not by name
    std::sort (mounts.begin(), mounts.end(), compareMountByRoot);
#endif

    for (size_t i = 0; i < mounts.size (); i++) {
        if (!mounts[i]->get_volume ()) {
            Gtk::TreeModel::Row newrow = *(placesModel->append());
            newrow[placesColumns.label] = mounts[i]->get_name ();
            newrow[placesColumns.icon]  = mounts[i]->get_icon ();
            newrow[placesColumns.root]  = mounts[i]->get_root ()->get_parse_name ();
            newrow[placesColumns.type]  = 1;
            newrow[placesColumns.rowSeparator] = false;
        }
    }

    is_refreshing = false;
}

void PlacesBrowser::queuePlacesListRefresh ()
{
    if (refresh_pending) return;

    refresh_pending = true;

    idle_register.add(
        [this]() -> bool
        {
            refresh_pending = false;
            refreshPlacesList ();
            return false;
        }
    );
}

bool PlacesBrowser::rowSeparatorFunc (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::iterator& iter)
{

    return iter->get_value (placesColumns.rowSeparator);
}

void PlacesBrowser::mountChanged (const Glib::RefPtr<Gio::Mount>& m)
{
    queuePlacesListRefresh();
}

void PlacesBrowser::volumeChanged (const Glib::RefPtr<Gio::Volume>& m)
{
    queuePlacesListRefresh();
}

void PlacesBrowser::driveChanged (const Glib::RefPtr<Gio::Drive>& m)
{
    queuePlacesListRefresh();
}

void PlacesBrowser::selectionChanged ()
{
    if (is_refreshing) return;

    Glib::RefPtr<Gtk::TreeSelection> selection = treeView->get_selection();
    Gtk::TreeModel::iterator iter = selection->get_selected();

    if (iter) {
        if (iter->get_value (placesColumns.type) == 2) {
            std::vector<Glib::RefPtr<Gio::Volume> > volumes = vm->get_volumes ();

            for (size_t i = 0; i < volumes.size(); i++)
                if (volumes[i]->get_name () == iter->get_value (placesColumns.label)) {
                    volumes[i]->mount ();
                    break;
                }
        } else if (iter->get_value (placesColumns.type) == 3) {
            std::vector<Glib::RefPtr<Gio::Drive> > drives = vm->get_connected_drives ();

            for (size_t i = 0; i < drives.size(); i++)
                if (drives[i]->get_name () == iter->get_value (placesColumns.label)) {
                    drives[i]->poll_for_media ();
                    break;
                }
        } else if (selectDir) {
            selectDir (iter->get_value (placesColumns.root));
        }
    }
}

void PlacesBrowser::dirSelected (const Glib::ustring& dirname, const Glib::ustring& openfile)
{

    lastSelectedDir = dirname;
}

void PlacesBrowser::addPressed ()
{

    if (lastSelectedDir.empty()) {
        return;
    }

    auto& options = App::get().mut_options();
    // check if the dirname is already in the list. If yes, return.
    for (size_t i = 0; i < options.favoriteDirs.size(); i++)
        if (options.favoriteDirs[i] == lastSelectedDir) {
            return;
        }

    // append
    Glib::RefPtr<Gio::File> hfile = Gio::File::create_for_path (lastSelectedDir);

    if (hfile && hfile->query_exists()) {
        try {
            if (auto info = hfile->query_info ()) {
                options.favoriteDirs.push_back (hfile->get_parse_name ());
                refreshPlacesList ();
            }
        } catch(Gio::Error&) {}
    }
}

void PlacesBrowser::delPressed ()
{

    // lookup the selected item in the bookmark
    Glib::RefPtr<Gtk::TreeSelection> selection = treeView->get_selection();
    Gtk::TreeModel::iterator iter = selection->get_selected();

    auto& options = App::get().mut_options();
    if (iter && iter->get_value (placesColumns.type) == 5) {
        std::vector<Glib::ustring>::iterator i = std::find (options.favoriteDirs.begin(), options.favoriteDirs.end(), iter->get_value (placesColumns.root));

        if (i != options.favoriteDirs.end()) {
            options.favoriteDirs.erase (i);
        }
    }

    refreshPlacesList ();
}

Glib::ustring PlacesBrowser::userHomeDir ()
{
#ifdef _WIN32

    // get_home_dir crashes on some Windows configurations,
    // so we rather use the safe native functions here.
    WCHAR pathW[MAX_PATH];
    if (SHGetSpecialFolderPathW (NULL, pathW, CSIDL_PERSONAL, false)) {

        char pathA[MAX_PATH];
        if (WideCharToMultiByte (CP_UTF8, 0, pathW, -1, pathA, MAX_PATH, 0, 0)) {

            return Glib::ustring (pathA);
        }
    }

    return Glib::ustring ("C:\\");

#else

    return Glib::get_home_dir ();

#endif
}

Glib::ustring PlacesBrowser::userPicturesDir ()
{
#ifdef _WIN32

    // get_user_special_dir crashes on some Windows configurations,
    // so we rather use the safe native functions here.
    WCHAR pathW[MAX_PATH];
    if (SHGetSpecialFolderPathW (NULL, pathW, CSIDL_MYPICTURES, false)) {

        char pathA[MAX_PATH];
        if (WideCharToMultiByte (CP_UTF8, 0, pathW, -1, pathA, MAX_PATH, 0, 0)) {

            return Glib::ustring (pathA);
        }
    }

    return Glib::ustring ("C:\\");

#else

    return Glib::get_user_special_dir (G_USER_DIRECTORY_PICTURES);

#endif
}
