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

#include <map>
#include <vector>

#include <glibmm/ustring.h>
#include <gtkmm/label.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>

#include "threadutils.h"
#include "widgets/basic/smartscroll.h"

class ProfileStoreEntry;

/**
 * @brief subclass of Gtk::Label with extra fields for Combobox and Menu, to link with a ProfileStoreEntry
 */
class ProfileStoreLabel final : public Gtk::Label
{

public:
    const ProfileStoreEntry *entry;

#ifndef NDEBUG
    ProfileStoreLabel() : Gtk::Label ("*** error ***"), entry (nullptr) {}
#else
    ProfileStoreLabel() : Gtk::Label (""), entry (nullptr) {}
#endif

    /** @brief Create a new ProfileStoreLabel
      *
      * @param entry      Pointer to the ProfileStoreEntry object, be it a directory or a file
      */
    explicit ProfileStoreLabel (const ProfileStoreEntry *entry);
    ProfileStoreLabel (const ProfileStoreLabel &other);
};

class ProfileStoreComboBox final : public MyComboBox
{

private:
    class MethodColumns final : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Gtk::TreeModelColumn<Glib::ustring> label;
        Gtk::TreeModelColumn<const ProfileStoreEntry*> profileStoreEntry;
        MethodColumns()
        {
            add (label);
            add (profileStoreEntry);
        }
    };

    Glib::RefPtr<Gtk::TreeStore> refTreeModel;
    MethodColumns methodColumns;
    void refreshProfileList_ (Gtk::TreeModel::Row *parentRow, int parentFolderId, bool initial, const std::vector<const ProfileStoreEntry*> *entryList);
    Gtk::TreeIter findRowFromEntry_ (Gtk::TreeModel::Children childs, const ProfileStoreEntry *pse) const;
    Gtk::TreeIter findRowFromFullPath_ (Gtk::TreeModel::Children childs, int parentFolderId, const Glib::ustring &name) const;
    Gtk::TreeIter findRowFromEntry (const ProfileStoreEntry *pse) const;
    Gtk::TreeIter findRowFromFullPath (const Glib::ustring &path) const;


public:
    ProfileStoreComboBox();
    void updateProfileList();
    Glib::ustring getCurrentLabel() const;
    const ProfileStoreEntry* getSelectedEntry() const;
    Glib::ustring getFullPathFromActiveRow () const;
    bool setActiveRowFromFullPath (const Glib::ustring &oldPath);
    bool setActiveRowFromEntry (const ProfileStoreEntry *pse);
    bool setInternalEntry ();
    Gtk::TreeIter getRowFromLabel (const Glib::ustring &name) const;
    Gtk::TreeIter addRow (const ProfileStoreEntry *profileStoreEntry);
    void deleteRow (const ProfileStoreEntry *profileStoreEntry);
};
