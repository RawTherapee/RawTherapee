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
#include "dirbrowser.h"

#include <iostream>
#include <cstring>

#include "guiutils.h"
#include "rtsurface.h"
#include "multilangmgr.h"
#include "options.h"

#ifdef _WIN32
#include "rtengine/leanwindows.h"
#endif // _WIN32

namespace
{

std::vector<Glib::ustring> listSubDirs (const Glib::RefPtr<Gio::File>& dir, bool addHidden)
{
    std::vector<Glib::ustring> subDirs;

    try {

        // CD-ROM with no disc inserted are reported, but do not exist.
        if (!Glib::file_test (dir->get_path (), Glib::FILE_TEST_EXISTS)) {
            return subDirs;
        }

        auto enumerator = dir->enumerate_children ("standard::name,standard::type,standard::is-hidden");

        while (true) {
            try {
                auto file = enumerator->next_file ();
                if (!file) {
                    break;
                }
#ifdef _WIN32
                if (!file->has_attribute("standard::type")) {
                    continue;
                }
#endif
                if (file->get_file_type () != Gio::FILE_TYPE_DIRECTORY) {
                    continue;
                }
                if (!addHidden && file->is_hidden ()) {
                    continue;
                }
                subDirs.push_back (file->get_name ());
            } catch (const Glib::Exception& exception) {

                if (rtengine::settings->verbose) {
                    std::cerr << exception.what().c_str() << std::endl;
                }

            }
        }

    } catch (const Glib::Exception& exception) {

        if (rtengine::settings->verbose) {
            std::cerr << "Failed to list subdirectories of \"" << dir->get_parse_name() << "\": " << exception.what () << std::endl;
        }

    }

    return subDirs;
}

}

DirBrowser::DirBrowser () : dirTreeModel(),
    dtColumns(),
    tvc(M("DIRBROWSER_FOLDERS")),

    openfolder("folder-open-small"),
    closedfolder("folder-closed-small"),
    icdrom("device-optical"),
    ifloppy("device-floppy"),
    ihdd("device-hdd"),
    inetwork("device-network"),
    iremovable("device-usb"),

    expandSuccess(false)
#ifdef _WIN32
    , volumes(0)
#endif
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);
    dirtree = Gtk::manage ( new Gtk::TreeView() );
    scrolledwindow4 = Gtk::manage ( new Gtk::ScrolledWindow() );
    crt.property_ellipsize() = Pango::ELLIPSIZE_END;

//   dirtree->set_flags(Gtk::CAN_FOCUS);
    dirtree->set_headers_visible();
    dirtree->set_headers_clickable();
    dirtree->set_rules_hint(false);
    dirtree->set_reorderable(false);
    dirtree->set_enable_search(false);
    scrolledwindow4->set_can_focus(true);
    scrolledwindow4->set_shadow_type(Gtk::SHADOW_NONE);
    scrolledwindow4->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledwindow4->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
    scrolledwindow4->add(*dirtree);

    pack_start (*scrolledwindow4);
    dirtree->show ();
    scrolledwindow4->show ();
}

DirBrowser::~DirBrowser()
{
    idle_register.destroy();

#ifdef _WIN32
    timerEventSource.disconnect();
#endif
}

void DirBrowser::fillDirTree ()
{
    //Create the Tree model:
    dirTreeModel = Gtk::TreeStore::create(dtColumns);
    dirtree->set_model (dirTreeModel);

    fillRoot ();

    Gtk::CellRendererPixbuf* render_pb = Gtk::manage ( new Gtk::CellRendererPixbuf () );
    render_pb->property_stock_size() = Gtk::ICON_SIZE_SMALL_TOOLBAR;
    tvc.pack_start (*render_pb, false);
    tvc.add_attribute(*render_pb, "icon-name", dtColumns.icon_name);
    tvc.pack_start (crt);
    tvc.add_attribute(crt, "text", dtColumns.filename);

    dirtree->append_column(tvc);

    const auto& options = App::get().options();
    tvc.set_sort_order(options.dirBrowserSortType);
    tvc.set_sort_column(dtColumns.filename);
    tvc.set_sort_indicator(true);
    tvc.set_clickable();

    dirTreeModel->set_sort_column(dtColumns.filename, options.dirBrowserSortType);

    crt.property_ypad() = 0;
    render_pb->property_ypad() = 0;

    dirtree->signal_row_expanded().connect(sigc::mem_fun(*this, &DirBrowser::row_expanded));
    dirtree->signal_row_collapsed().connect(sigc::mem_fun(*this, &DirBrowser::row_collapsed));
    dirtree->signal_row_activated().connect(sigc::mem_fun(*this, &DirBrowser::row_activated));
    dirTreeModel->signal_sort_column_changed().connect(sigc::mem_fun(*this, &DirBrowser::on_sort_column_changed));
}

#ifdef _WIN32
void DirBrowser::addRoot (char letter)
{

    char volume[4];
    volume[0] = letter;
    strcpy (volume + 1, ":\\");

    Gtk::TreeModel::iterator root = dirTreeModel->append();
    root->set_value (dtColumns.filename, Glib::ustring(volume));
    root->set_value (dtColumns.dirname, Glib::ustring(volume));

    int type = GetDriveType (volume);

    if (type == DRIVE_CDROM) {
        root->set_value (dtColumns.icon_name, icdrom);
    } else if (type == DRIVE_REMOVABLE) {
        if (letter - 'A' < 2) {
            root->set_value (dtColumns.icon_name, ifloppy);
        } else {
            root->set_value (dtColumns.icon_name, iremovable);
        }
    } else if (type == DRIVE_REMOTE) {
        root->set_value (dtColumns.icon_name, inetwork);
    } else if (type == DRIVE_FIXED) {
        root->set_value (dtColumns.icon_name, ihdd);
    }

    Gtk::TreeModel::iterator child = dirTreeModel->append (root->children());
    child->set_value (dtColumns.filename, Glib::ustring("foo"));
}

bool DirBrowser::updateVolumes ()
{
    unsigned int nvolumes = GetLogicalDrives ();

    if (nvolumes != volumes) {
        for (int i = 0; i < 32; i++) {
            if (((volumes >> i) & 1) && !((nvolumes >> i) & 1)) { // volume i has been deleted
                for (Gtk::TreeModel::iterator iter = dirTreeModel->children().begin(); iter != dirTreeModel->children().end(); ++iter)
                    if (iter->get_value (dtColumns.filename).c_str()[0] - 'A' == i) {
                        dirTreeModel->erase (iter);
                        break;
                    }
            } else if (!((volumes >> i) & 1) && ((nvolumes >> i) & 1)) {
                addRoot ('A' + i);    // volume i has been added
            }
        }
        volumes = nvolumes;
    }

    return true;
}

#endif

void DirBrowser::fillRoot ()
{

#ifdef _WIN32
    volumes = GetLogicalDrives ();

    for (int i = 0; i < 32; i++) {
        if ((volumes >> i) & 1) {
            addRoot ('A' + i);
        }
    }

    // Keep updating the volumes on 5 second interval
    timerEventSource = Glib::signal_timeout().connect(sigc::mem_fun(*this, &DirBrowser::updateVolumes), 5000);
#else
    Gtk::TreeModel::Row rootRow = *(dirTreeModel->append());
    rootRow[dtColumns.filename] = "/";
    rootRow[dtColumns.dirname] = "/";
    Gtk::TreeModel::Row childRow = *(dirTreeModel->append(rootRow.children()));
    childRow[dtColumns.filename] = "foo";
#endif
}

void DirBrowser::on_sort_column_changed() const
{
    auto& options = App::get().mut_options();
    options.dirBrowserSortType = tvc.get_sort_order();
}

void DirBrowser::row_expanded (const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path)
{
    expandSuccess = false;

    // We will disable model's sorting because it decreases speed of inserting new items
    // in list tree dramatically. Therefore will do:
    // 1) Disable sorting in model
    // 2) Manually sort data in the order determined by the options
    // 3) Enable sorting in model again for UI (sorting by click on header)
    int prevSortColumn;
    Gtk::SortType prevSortType;
    dirTreeModel->get_sort_column_id(prevSortColumn, prevSortType);
    dirTreeModel->set_sort_column(Gtk::TreeSortable::DEFAULT_UNSORTED_COLUMN_ID, Gtk::SORT_ASCENDING);

    const auto& options = App::get().options();
    auto dir = Gio::File::create_for_path (iter->get_value (dtColumns.dirname));
    auto subDirs = listSubDirs (dir, options.fbShowHidden);

    Gtk::TreeNodeChildren children = iter->children();
    std::list<Gtk::TreeIter> forErase(children.begin(), children.end());

    std::sort (subDirs.begin (), subDirs.end (), [&](const Glib::ustring& firstDir, const Glib::ustring& secondDir)
    {
        switch (options.dirBrowserSortType) {
        default:
        case Gtk::SORT_ASCENDING:
            return firstDir < secondDir;
        case Gtk::SORT_DESCENDING:
            return firstDir > secondDir;
        }
    });

    for (auto it = subDirs.begin(), end = subDirs.end(); it != end; ++it) {
        addDir(iter, *it);
    }

    for (auto it = forErase.begin(), end = forErase.end(); it != end; ++it) {
        dirTreeModel->erase(*it);
    }

    dirTreeModel->set_sort_column(prevSortColumn, prevSortType);

    expandSuccess = true;

    // Update row icon (only if row icon is not a volume one or is empty)
    if (iter->get_value(dtColumns.icon_name) == closedfolder || iter->get_value(dtColumns.icon_name) == "") {
        iter->set_value(dtColumns.icon_name, openfolder);
    }

    Glib::RefPtr<Gio::FileMonitor> monitor = dir->monitor_directory();
    iter->set_value(dtColumns.monitor, monitor);
    monitor->signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &DirBrowser::file_changed), iter));
}

void DirBrowser::row_collapsed (const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path)
{
    // Update row icon (only if row icon is not a volume one)
    if (iter->get_value(dtColumns.icon_name) == openfolder) {
        iter->set_value(dtColumns.icon_name, closedfolder);
    }
}

void DirBrowser::updateDir (const Gtk::TreeModel::iterator& iter)
{
    // first test if directories have been deleted
    bool change = true;

    while (change) {
        change = false;

        for (Gtk::TreeModel::iterator it = iter->children().begin(); it != iter->children().end(); ++it)
            if (!Glib::file_test (it->get_value (dtColumns.dirname), Glib::FILE_TEST_EXISTS)
                    || !Glib::file_test (it->get_value (dtColumns.dirname), Glib::FILE_TEST_IS_DIR)) {
                dirTreeModel->erase (it);
                change = true;
                break;
            }
    }

    // test if new directories have been created
    auto dir = Gio::File::create_for_path (iter->get_value (dtColumns.dirname));
    auto subDirs = listSubDirs (dir, App::get().options().fbShowHidden);

    for (size_t i = 0; i < subDirs.size(); i++) {
        bool found = false;

        for (Gtk::TreeModel::iterator it = iter->children().begin(); it != iter->children().end() && !found ; ++it) {
            found = (it->get_value (dtColumns.filename) == subDirs[i]);
        }

        if (!found) {
            addDir (iter, subDirs[i]);
        }
    }
}

void DirBrowser::addDir (const Gtk::TreeModel::iterator& iter, const Glib::ustring& dirname)
{

    Gtk::TreeModel::iterator child = dirTreeModel->append(iter->children());
    child->set_value (dtColumns.filename, dirname);
    child->set_value (dtColumns.icon_name, closedfolder);
    Glib::ustring fullname = Glib::build_filename (iter->get_value (dtColumns.dirname), dirname);
    child->set_value (dtColumns.dirname, fullname);
    Gtk::TreeModel::iterator fooRow = dirTreeModel->append(child->children());
    fooRow->set_value (dtColumns.filename, Glib::ustring("foo"));
}

void DirBrowser::row_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column)
{
    Glib::ustring dname = dirTreeModel->get_iter (path)->get_value (dtColumns.dirname);

    if (Glib::file_test (dname, Glib::FILE_TEST_IS_DIR)) {
        dirSelectionSignal (dname, Glib::ustring());
        dirtree->expand_row(path, false);
    }
}

Gtk::TreePath DirBrowser::expandToDir (const Glib::ustring& absDirPath)
{

    Gtk::TreeModel::Path path;
    path.push_back(0);

    char* dcpy = strdup (absDirPath.c_str());
    char* dir = strtok (dcpy, "/\\");
#ifdef _WIN32
    int count = 0;
#endif
    expandSuccess = true;

#ifndef _WIN32
    Gtk::TreeModel::iterator j = dirTreeModel->get_iter (path);
    path.up ();
    path.push_back (0);
    row_expanded(j, path);
    path.push_back (0);
#endif

    while (dir) {
        Glib::ustring dirstr = dir;
#ifdef _WIN32

        if (count == 0) {
            dirstr = dirstr + "\\";
        }

#endif
        Gtk::TreeModel::iterator i = dirTreeModel->get_iter (path);
        int ix = 0;

        while (i && expandSuccess) {
            Gtk::TreeModel::Row crow = *i;
            Glib::ustring str = crow[dtColumns.filename];
#ifdef _WIN32

            if (str.casefold() == dirstr.casefold()) {
#else

            if (str == dirstr) {
#endif
                path.up ();
                path.push_back (ix);
                row_expanded(i, path);
                path.push_back (0);
                break;
            }

            ++ix;
            ++i;
        }
#ifdef _WIN32
        count++;
#endif
        dir = strtok(nullptr, "/\\");
    }

    free(dcpy);

    path.up ();
    dirtree->expand_to_path (path);

    return path;
}

void DirBrowser::open (const Glib::ustring& dirname, const Glib::ustring& fileName)
{
    dirtree->collapse_all ();

    // WARNING & TODO: One should test here if the directory/file has R/W access permission to avoid crash

    Glib::RefPtr<Gio::File> dir = Gio::File::create_for_path(dirname);

    if( !dir->query_exists()) {
        return;
    }

    Glib::ustring absDirPath = dir->get_parse_name ();
    Gtk::TreePath path = expandToDir (absDirPath);
    dirtree->scroll_to_row (path);
    dirtree->get_selection()->select (path);
    Glib::ustring absFilePath;

    if (!fileName.empty()) {
        absFilePath = Glib::build_filename (absDirPath, fileName);
    }

    dirSelectionSignal (absDirPath, absFilePath);
}

void DirBrowser::eventDirectoryDeleted(const Gtk::TreeModel::iterator& iter, const Glib::RefPtr<Gio::File>& directory)
{
    if (!directory) {
        return;
    }

    for (Gtk::TreeModel::iterator it = iter->children().begin(); it != iter->children().end(); ++it) {
        if (it->get_value (dtColumns.dirname) == directory->get_path()) {
            dirTreeModel->erase (it);
            break;
        }
    }
}

void DirBrowser::eventDirectoryCreated(const Gtk::TreeModel::iterator& iter, const Glib::RefPtr<Gio::File>& directory)
{
    if (!directory) {
        return;
    }

    if ( std::find_if(iter->children().begin(), iter->children().end(),
                    [&directory, this](const Gtk::TreeModel::iterator it) { return it->get_value(dtColumns.dirname) == directory->get_path(); })
                    != iter->children().end()) {
        return;
    }

    addDir(iter, directory->get_basename());
    // sub directories are needed only if the entry is expanded, they are handled by the row_expanded call
}

void DirBrowser::file_changed (const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& other_file, Gio::FileMonitorEvent event_type, const Gtk::TreeModel::iterator& iter)
{
    if (event_type == Gio::FILE_MONITOR_EVENT_CHANGED ||
        event_type == Gio::FILE_MONITOR_EVENT_CHANGES_DONE_HINT ||
        event_type == Gio::FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED ||
        event_type == Gio::FILE_MONITOR_EVENT_PRE_UNMOUNT ||
        (event_type != Gio::FILE_MONITOR_EVENT_DELETED &&
            event_type != Gio::FILE_MONITOR_EVENT_UNMOUNTED &&
            !Glib::file_test(file->get_path(), Glib::FILE_TEST_IS_DIR))) {
        return;
    }

    switch (event_type) {
        case Gio::FILE_MONITOR_EVENT_DELETED:
            // there is no way to know if the event was triggered by a file or a directory, this will be called for both
            eventDirectoryDeleted(iter, file);
            break;
        case Gio::FILE_MONITOR_EVENT_CREATED:
            eventDirectoryCreated(iter, file);
            break;
        default:
            updateDir (iter);
    }
}

void DirBrowser::selectDir (Glib::ustring dir)
{

    open (dir, "");
}

