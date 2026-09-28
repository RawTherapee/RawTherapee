/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2011 Oliver Duis <www.oliverduis.de>
 *  Copyright (c) 2011 Michael Ezra <www.michaelezra.com>
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
#include <algorithm>
#include <map>

#include <glibmm/ustring.h>

#include "filebrowser.h"

#include "batchqueue.h"
#include "clipboard.h"
#include "inspector.h"
#include "multilangmgr.h"
#include "options.h"
#include "paramsedited.h"
#include "profilestorecombobox.h"
#include "procparamchangers.h"
#include "rtimage.h"
#include "threadutils.h"
#include "thumbnail.h"

#include "rtengine/dfmanager.h"
#include "rtengine/ffmanager.h"
#include "rtengine/procparams.h"

namespace
{

const Glib::ustring* getOriginalExtension (const ThumbBrowserEntryBase* entry)
{
    // We use the parsed extensions as a priority list,
    // i.e. what comes earlier in the list is considered an original of what comes later.
    typedef std::vector<Glib::ustring> ExtensionVector;
    typedef ExtensionVector::const_iterator ExtensionIterator;

    const ExtensionVector& originalExtensions = App::get().options().parsedExtensions;

    // Extract extension from basename
    const Glib::ustring basename = Glib::path_get_basename (entry->filename.lowercase());

    const Glib::ustring::size_type pos = basename.find_last_of ('.');
    if (pos >= basename.length () - 1) {
        return nullptr;
    }

    const Glib::ustring extension = basename.substr (pos + 1);

    // Try to find a matching original extension
    for (ExtensionIterator originalExtension = originalExtensions.begin(); originalExtension != originalExtensions.end(); ++originalExtension) {
        if (*originalExtension == extension) {
            return &*originalExtension;
        }
    }

    return nullptr;
}

ThumbBrowserEntryBase* selectOriginalEntry (ThumbBrowserEntryBase* original, ThumbBrowserEntryBase* candidate)
{
    if (original == nullptr) {
        return candidate;
    }

    // The candidate will become the new original, if it has an original extension
    // and if its extension is higher in the list than the old original.
    if (const Glib::ustring* candidateExtension = getOriginalExtension (candidate)) {
        if (const Glib::ustring* originalExtension = getOriginalExtension (original)) {
            return candidateExtension < originalExtension ? candidate : original;
        }
    }

    return original;
}

void findOriginalEntries (const std::vector<ThumbBrowserEntryBase*>& entries)
{
    // Sort all entries into buckets by basename without extension
    std::map<Glib::ustring, std::vector<ThumbBrowserEntryBase*>> byBasename;

    for (const auto entry : entries) {
        const auto basename = Glib::path_get_basename(entry->filename.lowercase());

        const auto pos = basename.find_last_of('.');
        if (pos >= basename.length() - 1) {
            entry->setOriginal(nullptr);
            continue;
        }

        const auto withoutExtension = basename.substr(0, pos);

        byBasename[withoutExtension].push_back(entry);
    }

    // Find the original image for each bucket
    for (const auto& bucket : byBasename) {
        const auto& lentries = bucket.second;
        ThumbBrowserEntryBase* original = nullptr;

        // Select the most likely original in a first pass...
        for (const auto entry : lentries) {
            original = selectOriginalEntry(original, entry);
        }

        // ...and link all other images to it in a second pass.
        for (const auto entry : lentries) {
            entry->setOriginal(entry != original ? original : nullptr);
        }
    }
}

}

FileBrowser::FileBrowser () :
    menuLabel(nullptr),
    miOpenDefaultViewer(nullptr),
    selectDF(nullptr),
    thisIsDF(nullptr),
    autoDF(nullptr),
    selectFF(nullptr),
    thisIsFF(nullptr),
    autoFF(nullptr),
    clearFromCache(nullptr),
    clearFromCacheFull(nullptr),
    colorLabel_actionData(nullptr),
    bppcl(nullptr),
    tbl(nullptr),
    numFiltered(0),
    exportPanel(nullptr)
{
    session_id_ = 0;

    ProfileStore::getInstance()->addListener(this);
    const auto& options = App::get().options();

    int p = 0;
    pmenu = new Gtk::Menu ();
    pmenu->attach (*Gtk::manage(open = new Gtk::MenuItem (M("FILEBROWSER_POPUPOPEN"))), 0, 1, p, p + 1);
    p++;
    if (options.inspectorWindow) {
        pmenu->attach (*Gtk::manage(inspect = new Gtk::MenuItem (M("FILEBROWSER_POPUPINSPECT"))), 0, 1, p, p + 1);
        p++;
    }
    pmenu->attach (*Gtk::manage(develop = new MyImageMenuItem (M("FILEBROWSER_POPUPPROCESS"), "gears")), 0, 1, p, p + 1);
    p++;
    pmenu->attach (*Gtk::manage(developfast = new Gtk::MenuItem (M("FILEBROWSER_POPUPPROCESSFAST"))), 0, 1, p, p + 1);
    p++;

    pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;
    pmenu->attach (*Gtk::manage(selall = new Gtk::MenuItem (M("FILEBROWSER_POPUPSELECTALL"))), 0, 1, p, p + 1);
    p++;

    /***********************
     * sort
     ***********************/
    const std::array<std::string, 2> cnameSortOrders = {
        M("SORT_ASCENDING"),
        M("SORT_DESCENDING"),
    };

    const std::array<std::string, Options::SORT_METHOD_COUNT> cnameSortMethods = {
        M("SORT_BY_NAME"),
        M("SORT_BY_DATE"),
        M("SORT_BY_EXIF"),
        M("SORT_BY_RANK"),
        M("SORT_BY_LABEL"),
    };

    pmenu->attach (*Gtk::manage(menuSort = new Gtk::MenuItem (M("FILEBROWSER_POPUPSORTBY"))), 0, 1, p, p + 1);
    p++;
    Gtk::Menu* submenuSort = Gtk::manage (new Gtk::Menu ());
    Gtk::RadioButtonGroup sortOrderGroup, sortMethodGroup;
    for (size_t i = 0; i < cnameSortOrders.size(); i++) {
        submenuSort->attach (*Gtk::manage(sortOrder[i] = new Gtk::RadioMenuItem (sortOrderGroup, cnameSortOrders[i])), 0, 1, p, p + 1);
        p++;
        sortOrder[i]->set_active (i == options.sortDescending);
    }
    submenuSort->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;
    for (size_t i = 0; i < cnameSortMethods.size(); i++) {
        submenuSort->attach (*Gtk::manage(sortMethod[i] = new Gtk::RadioMenuItem (sortMethodGroup, cnameSortMethods[i])), 0, 1, p, p + 1);
        p++;
        sortMethod[i]->set_active (i == options.sortMethod);
    }
    submenuSort->show_all ();
    menuSort->set_submenu (*submenuSort);

    /***********************
     * rank
     ***********************/
    if (options.menuGroupRank) {
        pmenu->attach (*Gtk::manage(menuRank = new Gtk::MenuItem (M("FILEBROWSER_POPUPRANK"))), 0, 1, p, p + 1);
        p++;
        Gtk::Menu* submenuRank = Gtk::manage (new Gtk::Menu ());
        submenuRank->attach (*Gtk::manage(rank[0] = new Gtk::MenuItem (M("FILEBROWSER_POPUPUNRANK"))), 0, 1, p, p + 1);
        p++;

        for (int i = 1; i <= 5; i++) {
            submenuRank->attach (*Gtk::manage(rank[i] = new Gtk::MenuItem (M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPRANK", i)))), 0, 1, p, p + 1);
            p++;
        }

        submenuRank->show_all ();
        menuRank->set_submenu (*submenuRank);
    } else {
        pmenu->attach (*Gtk::manage(rank[0] = new Gtk::MenuItem (M("FILEBROWSER_POPUPUNRANK"))), 0, 1, p, p + 1);
        p++;

        for (int i = 1; i <= 5; i++) {
            pmenu->attach (*Gtk::manage(rank[i] = new Gtk::MenuItem (M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPRANK", i)))), 0, 1, p, p + 1);
            p++;
        }

        pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
        p++;
    }

    if (!options.menuGroupRank || !options.menuGroupLabel) { // separate Rank and Color Labels if either is not grouped
        pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    }

    p++;

    /***********************
     * color labels
     ***********************/

    // Thumbnail context menu
    // Similar image arrays in filecatalog.cc
    std::array<std::string, 6> clabelActiveIcons = {"circle-empty-gray-small", "circle-red-small", "circle-yellow-small", "circle-green-small", "circle-blue-small", "circle-purple-small"};
    std::array<std::string, 6> clabelInactiveIcons = {"circle-empty-darkgray-small", "circle-empty-red-small", "circle-empty-yellow-small", "circle-empty-green-small", "circle-empty-blue-small", "circle-empty-purple-small"};

    if (options.menuGroupLabel) {
        pmenu->attach (*Gtk::manage(menuLabel = new Gtk::MenuItem (M("FILEBROWSER_POPUPCOLORLABEL"))), 0, 1, p, p + 1);
        p++;
        Gtk::Menu* submenuLabel = Gtk::manage (new Gtk::Menu ());

        for (int i = 0; i <= 5; i++) {
            submenuLabel->attach(*Gtk::manage(colorlabel[i] = new MyImageMenuItem(M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPCOLORLABEL", i)), clabelActiveIcons[i])), 0, 1, p, p + 1);
            p++;
        }

        submenuLabel->show_all ();
        menuLabel->set_submenu (*submenuLabel);
    } else {
        for (int i = 0; i <= 5; i++) {
            pmenu->attach(*Gtk::manage(colorlabel[i] = new MyImageMenuItem(M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPCOLORLABEL", i)), clabelInactiveIcons[i])), 0, 1, p, p + 1);
            p++;
        }
    }

    pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;

    /***********************
     * external programs
     * *********************/
#if defined(_WIN32)
    Gtk::manage(miOpenDefaultViewer = new Gtk::MenuItem (M("FILEBROWSER_OPENDEFAULTVIEWER")));
#endif

    // Build a list of menu items
    mMenuExtProgs.clear();
    amiExtProg = nullptr;

    for (const auto& action : extProgStore->getActions ()) {
        if (action.target == 1 || action.target == 2) {
            mMenuExtProgs[action.getFullName ()] = &action;
        }
    }

    // Attach them to menu
    if (!mMenuExtProgs.empty() || miOpenDefaultViewer) {
        amiExtProg = new Gtk::MenuItem*[mMenuExtProgs.size()];
        int itemNo = 0;

        if (options.menuGroupExtProg) {
            pmenu->attach (*Gtk::manage(menuExtProg = new Gtk::MenuItem (M("FILEBROWSER_EXTPROGMENU"))), 0, 1, p, p + 1);
            p++;
            Gtk::Menu* submenuExtProg = Gtk::manage (new Gtk::Menu());

#ifdef _WIN32
            if (miOpenDefaultViewer) {
                submenuExtProg->attach (*miOpenDefaultViewer, 0, 1, p, p + 1);
                p++;
            }
#endif
            for (auto it = mMenuExtProgs.begin(); it != mMenuExtProgs.end(); it++, itemNo++) {
                submenuExtProg->attach (*Gtk::manage(amiExtProg[itemNo] = new Gtk::MenuItem ((*it).first)), 0, 1, p, p + 1);
                p++;
            }

            submenuExtProg->show_all ();
            menuExtProg->set_submenu (*submenuExtProg);
        } else {
#ifdef _WIN32
            if (miOpenDefaultViewer) {
                pmenu->attach (*miOpenDefaultViewer, 0, 1, p, p + 1);
                p++;
            }
#endif
            for (auto it = mMenuExtProgs.begin(); it != mMenuExtProgs.end(); it++, itemNo++) {
                pmenu->attach (*Gtk::manage(amiExtProg[itemNo] = new Gtk::MenuItem ((*it).first)), 0, 1, p, p + 1);
                p++;
            }
        }

        pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
        p++;
    }

    /***********************
     * File Operations
     * *********************/
    if (options.menuGroupFileOperations) {
        pmenu->attach (*Gtk::manage(menuFileOperations = new Gtk::MenuItem (M("FILEBROWSER_POPUPFILEOPERATIONS"))), 0, 1, p, p + 1);
        p++;
        Gtk::Menu* submenuFileOperations = Gtk::manage (new Gtk::Menu ());

        submenuFileOperations->attach (*Gtk::manage(trash = new Gtk::MenuItem (M("FILEBROWSER_POPUPTRASH"))), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(untrash = new Gtk::MenuItem (M("FILEBROWSER_POPUPUNTRASH"))), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(rename = new Gtk::MenuItem (M("FILEBROWSER_POPUPRENAME"))), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(remove = new Gtk::MenuItem (M("FILEBROWSER_POPUPREMOVE"))), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(removeInclProc = new Gtk::MenuItem (M("FILEBROWSER_POPUPREMOVEINCLPROC"))), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(copyTo = new Gtk::MenuItem (M("FILEBROWSER_POPUPCOPYTO"))), 0, 1, p, p + 1);
        p++;
        submenuFileOperations->attach (*Gtk::manage(moveTo = new Gtk::MenuItem (M("FILEBROWSER_POPUPMOVETO"))), 0, 1, p, p + 1);
        p++;

        submenuFileOperations->show_all ();
        menuFileOperations->set_submenu (*submenuFileOperations);
    } else {
        pmenu->attach (*Gtk::manage(trash = new Gtk::MenuItem (M("FILEBROWSER_POPUPTRASH"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(untrash = new Gtk::MenuItem (M("FILEBROWSER_POPUPUNTRASH"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(rename = new Gtk::MenuItem (M("FILEBROWSER_POPUPRENAME"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(remove = new Gtk::MenuItem (M("FILEBROWSER_POPUPREMOVE"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(removeInclProc = new Gtk::MenuItem (M("FILEBROWSER_POPUPREMOVEINCLPROC"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(copyTo = new Gtk::MenuItem (M("FILEBROWSER_POPUPCOPYTO"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(moveTo = new Gtk::MenuItem (M("FILEBROWSER_POPUPMOVETO"))), 0, 1, p, p + 1);
        p++;
    }

    pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;

    /***********************
     * Profile Operations
     * *********************/
    if (options.menuGroupProfileOperations) {
        pmenu->attach (*Gtk::manage(menuProfileOperations = new Gtk::MenuItem (M("FILEBROWSER_POPUPPROFILEOPERATIONS"))), 0, 1, p, p + 1);
        p++;

        Gtk::Menu* submenuProfileOperations = Gtk::manage (new Gtk::Menu ());

        submenuProfileOperations->attach (*Gtk::manage(copyprof = new Gtk::MenuItem (M("FILEBROWSER_COPYPROFILE"))), 0, 1, p, p + 1);
        p++;
        submenuProfileOperations->attach (*Gtk::manage(pasteprof = new Gtk::MenuItem (M("FILEBROWSER_PASTEPROFILE"))), 0, 1, p, p + 1);
        p++;
        submenuProfileOperations->attach (*Gtk::manage(partpasteprof = new Gtk::MenuItem (M("FILEBROWSER_PARTIALPASTEPROFILE"))), 0, 1, p, p + 1);
        p++;
        submenuProfileOperations->attach (*Gtk::manage(applyprof = new Gtk::MenuItem (M("FILEBROWSER_APPLYPROFILE"))), 0, 1, p, p + 1);
        p++;
        submenuProfileOperations->attach (*Gtk::manage(applypartprof = new Gtk::MenuItem (M("FILEBROWSER_APPLYPROFILE_PARTIAL"))), 0, 1, p, p + 1);
        p++;
        submenuProfileOperations->attach (*Gtk::manage(resetdefaultprof = new Gtk::MenuItem (M("FILEBROWSER_RESETDEFAULTPROFILE"))), 0, 1, p, p + 1);
        p++;
        submenuProfileOperations->attach (*Gtk::manage(clearprof = new Gtk::MenuItem (M("FILEBROWSER_CLEARPROFILE"))), 0, 1, p, p + 1);
        p++;

        submenuProfileOperations->show_all ();
        menuProfileOperations->set_submenu (*submenuProfileOperations);
    } else {
        pmenu->attach (*Gtk::manage(copyprof = new Gtk::MenuItem (M("FILEBROWSER_COPYPROFILE"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(pasteprof = new Gtk::MenuItem (M("FILEBROWSER_PASTEPROFILE"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(partpasteprof = new Gtk::MenuItem (M("FILEBROWSER_PARTIALPASTEPROFILE"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(applyprof = new Gtk::MenuItem (M("FILEBROWSER_APPLYPROFILE"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(applypartprof = new Gtk::MenuItem (M("FILEBROWSER_APPLYPROFILE_PARTIAL"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(resetdefaultprof = new Gtk::MenuItem (M("FILEBROWSER_RESETDEFAULTPROFILE"))), 0, 1, p, p + 1);
        p++;
        pmenu->attach (*Gtk::manage(clearprof = new Gtk::MenuItem (M("FILEBROWSER_CLEARPROFILE"))), 0, 1, p, p + 1);
        p++;
    }


    pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;
    pmenu->attach (*Gtk::manage(menuDF = new Gtk::MenuItem (M("FILEBROWSER_DARKFRAME"))), 0, 1, p, p + 1);
    p++;
    pmenu->attach (*Gtk::manage(menuFF = new Gtk::MenuItem (M("FILEBROWSER_FLATFIELD"))), 0, 1, p, p + 1);
    p++;

    pmenu->attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;
    pmenu->attach (*Gtk::manage(cachemenu = new Gtk::MenuItem (M("FILEBROWSER_CACHE"))), 0, 1, p, p + 1);

    pmenu->show_all ();

    /***********************
     * Accelerators
     * *********************/
    pmaccelgroup = Gtk::AccelGroup::create ();
    pmenu->set_accel_group (pmaccelgroup);
    selall->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_a, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    trash->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_Delete, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    untrash->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_Delete, Gdk::SHIFT_MASK, Gtk::ACCEL_VISIBLE);
    open->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_Return, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    if (options.inspectorWindow)
        inspect->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_f, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    develop->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_B, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    developfast->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_B, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::ACCEL_VISIBLE);
    copyprof->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_C, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    pasteprof->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_V, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    partpasteprof->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_V, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::ACCEL_VISIBLE);
    copyTo->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_C, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::ACCEL_VISIBLE);
    moveTo->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_M, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::ACCEL_VISIBLE);

    // Bind to event handlers
    open->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), open));

    if (options.inspectorWindow) {
        inspect->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), inspect));
    }

    for (int i = 0; i < 2; i++) {
        sortOrder[i]->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), sortOrder[i]));
    }

    for (int i = 0; i < Options::SORT_METHOD_COUNT; i++) {
        sortMethod[i]->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), sortMethod[i]));
    }

    for (int i = 0; i < 6; i++) {
        rank[i]->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), rank[i]));
    }

    for (int i = 0; i < 6; i++) {
        colorlabel[i]->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), colorlabel[i]));
    }

    for (size_t i = 0; i < mMenuExtProgs.size(); i++) {
        amiExtProg[i]->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), amiExtProg[i]));
    }

#ifdef _WIN32
    if (miOpenDefaultViewer) {
        miOpenDefaultViewer->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), miOpenDefaultViewer));
    }
#endif

    trash->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), trash));
    untrash->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), untrash));
    develop->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), develop));
    developfast->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), developfast));
    rename->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), rename));
    remove->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), remove));
    removeInclProc->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), removeInclProc));
    selall->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), selall));
    copyTo->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), copyTo));
    moveTo->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), moveTo));
    copyprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), copyprof));
    pasteprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), pasteprof));
    partpasteprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), partpasteprof));
    applyprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), applyprof));
    applypartprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), applypartprof));
    resetdefaultprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), resetdefaultprof));
    clearprof->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), clearprof));
    cachemenu->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), cachemenu));

    // A separate pop-up menu for Color Labels
    int c = 0;
    pmenuColorLabels = new Gtk::Menu();

    for (int i = 0; i <= 5; i++) {
        pmenuColorLabels->attach(*Gtk::manage(colorlabel_pop[i] = new MyImageMenuItem(M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPCOLORLABEL", i)), clabelActiveIcons[i])), 0, 1, c, c + 1);
        c++;
    }

    pmenuColorLabels->show_all();

    // Has to be located after creation of applyprof and applypartprof
    updateProfileList ();

    // Bind to event handlers
    for (int i = 0; i <= 5; i++) {
        colorlabel_pop[i]->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuColorlabelActivated), colorlabel_pop[i]));
    }
}

FileBrowser::~FileBrowser ()
{
    idle_register.destroy();

    ProfileStore::getInstance()->removeListener(this);
    delete pmenu;
    delete pmenuColorLabels;
    delete[] amiExtProg;
}

void FileBrowser::rightClicked (int x, int y)
{

    {
        MYREADERLOCK(l, entryRW);

        trash->set_sensitive (false);
        untrash->set_sensitive (false);

        for (size_t i = 0; i < selected.size(); i++)
            if ((static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->getTrashed()) {
                untrash->set_sensitive (true);
                break;
            }

        for (size_t i = 0; i < selected.size(); i++)
            if (!(static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->getTrashed()) {
                trash->set_sensitive (true);
                break;
            }

        pasteprof->set_sensitive (clipboard.hasProcParams());
        partpasteprof->set_sensitive (clipboard.hasProcParams());
        copyprof->set_sensitive (selected.size() == 1);
        clearprof->set_sensitive (!selected.empty());
        copyTo->set_sensitive (!selected.empty());
        moveTo->set_sensitive (!selected.empty());
    }

    // submenuDF
    int p = 0;
    Gtk::Menu* submenuDF = Gtk::manage (new Gtk::Menu ());
    submenuDF->attach (*Gtk::manage(selectDF = new Gtk::MenuItem (M("FILEBROWSER_SELECTDARKFRAME"))), 0, 1, p, p + 1);
    p++;
    submenuDF->attach (*Gtk::manage(autoDF = new Gtk::MenuItem (M("FILEBROWSER_AUTODARKFRAME"))), 0, 1, p, p + 1);
    p++;
    submenuDF->attach (*Gtk::manage(thisIsDF = new Gtk::MenuItem (M("FILEBROWSER_MOVETODARKFDIR"))), 0, 1, p, p + 1);
    selectDF->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), selectDF));
    autoDF->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), autoDF));
    thisIsDF->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), thisIsDF ));
    submenuDF->show_all ();
    menuDF->set_submenu (*submenuDF);

    // submenuFF
    p = 0;
    Gtk::Menu* submenuFF = Gtk::manage (new Gtk::Menu ());
    submenuFF->attach (*Gtk::manage(selectFF = new Gtk::MenuItem (M("FILEBROWSER_SELECTFLATFIELD"))), 0, 1, p, p + 1);
    p++;
    submenuFF->attach (*Gtk::manage(autoFF = new Gtk::MenuItem (M("FILEBROWSER_AUTOFLATFIELD"))), 0, 1, p, p + 1);
    p++;
    submenuFF->attach (*Gtk::manage(thisIsFF = new Gtk::MenuItem (M("FILEBROWSER_MOVETOFLATFIELDDIR"))), 0, 1, p, p + 1);
    selectFF->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), selectFF));
    autoFF->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), autoFF));
    thisIsFF->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), thisIsFF ));
    submenuFF->show_all ();
    menuFF->set_submenu (*submenuFF);

    // build cache sub menu
    p = 0;
    Gtk::Menu* cachesubmenu = Gtk::manage (new Gtk::Menu ());
    cachesubmenu->attach (*Gtk::manage(clearFromCache = new Gtk::MenuItem (M("FILEBROWSER_CACHECLEARFROMPARTIAL"))), 0, 1, p, p + 1);
    p++;
    cachesubmenu->attach (*Gtk::manage(clearFromCacheFull = new Gtk::MenuItem (M("FILEBROWSER_CACHECLEARFROMFULL"))), 0, 1, p, p + 1);
    p++;
    clearFromCache->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), clearFromCache));
    clearFromCacheFull->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::menuItemActivated), clearFromCacheFull));
    cachesubmenu->show_all ();
    cachemenu->set_submenu (*cachesubmenu);

    const Gdk::Rectangle rect(x, y, 1, 1);
    pmenu->popup_at_rect(internal.get_window(), rect, Gdk::GRAVITY_NORTH_WEST, Gdk::GRAVITY_NORTH_WEST, nullptr);
}

void FileBrowser::doubleClicked (ThumbBrowserEntryBase* entry)
{

    if (tbl && entry) {
        std::vector<Thumbnail*> entries;
        entries.push_back ((static_cast<FileBrowserEntry*>(entry))->thumbnail);
        tbl->openRequested (entries);
    }
}

void FileBrowser::addEntry (FileBrowserEntry* entry)
{
    const unsigned int sid = session_id();

    idle_register.add(
        [this, entry, sid]() -> bool
        {
            if (sid != session_id()) {
                delete entry;
            } else {
                addEntry_(entry);
            }

            return false;
        }
    );
}

void FileBrowser::addEntry_ (FileBrowserEntry* entry)
{
    entry->setParent(this);

    entry->selected = false;
    entry->drawable = false;
    entry->framed = editedFiles.find(entry->filename) != editedFiles.end();

    // add button set to the thumbbrowserentry
    entry->addButtonSet(new FileThumbnailButtonSet(entry));
    entry->getThumbButtonSet()->setRank(entry->thumbnail->getRank());
    entry->getThumbButtonSet()->setColorLabel(entry->thumbnail->getColorLabel());
    entry->getThumbButtonSet()->setInTrash(entry->thumbnail->getTrashed());
    entry->getThumbButtonSet()->setButtonListener(this);
    entry->resize(getThumbnailHeight());
    entry->filtered = !checkFilter(entry);
    insertEntry(entry);
}

FileBrowserEntry* FileBrowser::delEntry (const Glib::ustring& fname)
{
    MYWRITERLOCK(l, entryRW);

    for (std::vector<ThumbBrowserEntryBase*>::iterator i = fd.begin(); i != fd.end(); ++i)
        if ((*i)->filename == fname) {
            ThumbBrowserEntryBase* entry = *i;
            entry->selected = false;
            fd.erase (i);
            std::vector<ThumbBrowserEntryBase*>::iterator j = std::find (selected.begin(), selected.end(), entry);

            MYWRITERLOCK_RELEASE(l);

            if (j != selected.end()) {
                if (checkFilter (*j)) {
                    numFiltered--;
                }

                selected.erase (j);
                notifySelectionListener ();
            }

            if (lastClicked == entry) {
                lastClicked = nullptr;
            }

            redraw ();

            return (static_cast<FileBrowserEntry*>(entry));
        }

    return nullptr;
}

void FileBrowser::close ()
{
    ++session_id_;

    {
        MYWRITERLOCK(l, entryRW);

        selected.clear ();
        anchor = nullptr;

        MYWRITERLOCK_RELEASE(l); // notifySelectionListener will need read access!

        notifySelectionListener ();

        MYWRITERLOCK_ACQUIRE(l);

        // The listener merges parameters with old values, so delete afterwards
        for (size_t i = 0; i < fd.size(); i++) {
            delete fd.at(i);
        }

        fd.clear ();
    }

    lastClicked = nullptr;
}

void FileBrowser::menuColorlabelActivated (Gtk::MenuItem* m)
{

    std::vector<FileBrowserEntry*> tbe;
    tbe.push_back (static_cast<FileBrowserEntry*>(colorLabel_actionData));

    for (int i = 0; i < 6; i++)
        if (m == colorlabel_pop[i]) {
            colorlabelRequested (tbe, i);
            return;
        }
}

void FileBrowser::menuItemActivated (Gtk::MenuItem* m)
{
    std::vector<FileBrowserEntry*> mselected;

    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }
    }


    if (!tbl || (m != selall && mselected.empty()) ) {
        return;
    }

    for (int i = 0; i < 2; i++)
        if (m == sortOrder[i]) {
            sortOrderRequested (i);
            return;
        }

    for (int i = 0; i < Options::SORT_METHOD_COUNT; i++)
        if (m == sortMethod[i]) {
            sortMethodRequested (i);
            return;
        }

    for (int i = 0; i < 6; i++)
        if (m == rank[i]) {
            rankingRequested (mselected, i);
            return;
        }

    for (int i = 0; i < 6; i++)
        if (m == colorlabel[i]) {
            colorlabelRequested (mselected, i);
            return;
        }

    const auto& options = App::get().options();

    for (size_t j = 0; j < mMenuExtProgs.size(); j++) {
        if (m == amiExtProg[j]) {
            const auto pAct = mMenuExtProgs[m->get_label()];

            // Build vector of all file names
            std::vector<Glib::ustring> selFileNames;

            for (size_t i = 0; i < mselected.size(); i++) {
                Glib::ustring fn = mselected[i]->thumbnail->getFileName();

                // Maybe batch processed version
                if (pAct->target == 2) {
                    fn = Glib::ustring::compose ("%1.%2", BatchQueue::calcAutoFileNameBase(fn), options.saveFormatBatch.format);
                }

                selFileNames.push_back(fn);
            }

            pAct->execute (selFileNames);
            return;
        }
    }

    if (m == open) {
        openRequested(mselected);
    } else if (options.inspectorWindow && m == inspect) {
        inspectRequested(mselected);
    } else if (m == remove) {
        tbl->deleteRequested (mselected, false, true);
    } else if (m == removeInclProc) {
        tbl->deleteRequested (mselected, true, true);
    } else if (m == trash) {
        toTrashRequested (mselected);
    } else if (m == untrash) {
        fromTrashRequested (mselected);
    }

    else if (m == develop) {
        tbl->developRequested (mselected, false);
    } else if (m == developfast) {
        if (exportPanel) {
            // force saving export panel settings
            exportPanel->setExportPanelListener(nullptr);
            exportPanel->FastExportPressed();
            exportPanel->setExportPanelListener(this);
        }
        tbl->developRequested (mselected, true);
    }

    else if (m == rename) {
        tbl->renameRequested (mselected);
    } else if (m == selall) {
        lastClicked = nullptr;
        {
            MYWRITERLOCK(l, entryRW);

            selected.clear();

            for (size_t i = 0; i < fd.size(); ++i) {
                if (checkFilter(fd[i])) {
                    fd[i]->selected = true;
                    selected.push_back(fd[i]);
                }
            }
            if (!anchor && !selected.empty()) {
                anchor = selected[0];
            }
        }
        queue_draw ();
        notifySelectionListener();
    } else if( m == copyTo) {
        tbl->copyMoveRequested (mselected, false);
    }

    else if( m == moveTo) {
        tbl->copyMoveRequested (mselected, true);
    }

    else if (m == autoDF) {
        if (!mselected.empty() && bppcl) {
            bppcl->beginBatchPParamsChange(mselected.size());
        }

        for (size_t i = 0; i < mselected.size(); i++) {
            rtengine::procparams::ProcParams pp = mselected[i]->thumbnail->getProcParams();
            pp.raw.df_autoselect = true;
            pp.raw.dark_frame.clear();
            mselected[i]->thumbnail->setProcParams(pp, nullptr, FILEBROWSER, false);
        }

        if (!mselected.empty() && bppcl) {
            bppcl->endBatchPParamsChange();
        }

    } else if (m == selectDF) {
        if( !mselected.empty() ) {
            rtengine::procparams::ProcParams pp = mselected[0]->thumbnail->getProcParams();
            Gtk::FileChooserDialog fc (getToplevelWindow (this), "Dark Frame", Gtk::FILE_CHOOSER_ACTION_OPEN );
            bindCurrentFolder (fc, App::get().mut_options().lastDarkframeDir);
            fc.add_button( M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
            fc.add_button( M("GENERAL_APPLY"), Gtk::RESPONSE_APPLY);

            if(!pp.raw.dark_frame.empty()) {
                fc.set_filename( pp.raw.dark_frame );
            }

            if( fc.run() == Gtk::RESPONSE_APPLY ) {
                if (bppcl) {
                    bppcl->beginBatchPParamsChange(mselected.size());
                }

                for (size_t i = 0; i < mselected.size(); i++) {
                    rtengine::procparams::ProcParams lpp = mselected[i]->thumbnail->getProcParams();
                    lpp.raw.dark_frame = fc.get_filename();
                    lpp.raw.df_autoselect = false;
                    mselected[i]->thumbnail->setProcParams(lpp, nullptr, FILEBROWSER, false);
                }

                if (bppcl) {
                    bppcl->endBatchPParamsChange();
                }
            }
        }
    } else if( m == thisIsDF) {
        if( !options.rtSettings.darkFramesPath.empty()) {
            if (Gio::File::create_for_path(options.rtSettings.darkFramesPath)->query_exists() ) {
                for (size_t i = 0; i < mselected.size(); i++) {
                    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path ( mselected[i]->filename );

                    if( !file ) {
                        continue;
                    }

                    Glib::ustring destName = options.rtSettings.darkFramesPath + "/" + file->get_basename();
                    Glib::RefPtr<Gio::File> dest = Gio::File::create_for_path ( destName );
                    file->move(  dest );
                }

                // Reinit cache
                rtengine::DFManager::getInstance().init( options.rtSettings.darkFramesPath );
            } else {
                // Target directory creation failed, we clear the darkFramesPath setting
                App::get().mut_options().rtSettings.darkFramesPath.clear();
                Glib::ustring msg_ = Glib::ustring::compose (M("MAIN_MSG_PATHDOESNTEXIST"), escapeHtmlChars(options.rtSettings.darkFramesPath))
                                     + "\n\n" + M("MAIN_MSG_OPERATIONCANCELLED");
                Gtk::MessageDialog msgd (msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                msgd.set_title(M("TP_DARKFRAME_LABEL"));
                msgd.run ();
            }
        } else {
            Glib::ustring msg_ = M("MAIN_MSG_SETPATHFIRST") + "\n\n" + M("MAIN_MSG_OPERATIONCANCELLED");
            Gtk::MessageDialog msgd (msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            msgd.set_title(M("TP_DARKFRAME_LABEL"));
            msgd.run ();
        }
    } else if (m == autoFF) {
        if (!mselected.empty() && bppcl) {
            bppcl->beginBatchPParamsChange(mselected.size());
        }

        for (size_t i = 0; i < mselected.size(); i++) {
            rtengine::procparams::ProcParams pp = mselected[i]->thumbnail->getProcParams();
            pp.raw.ff_AutoSelect = true;
            pp.raw.ff_file.clear();
            mselected[i]->thumbnail->setProcParams(pp, nullptr, FILEBROWSER, false);
        }

        if (!mselected.empty() && bppcl) {
            bppcl->endBatchPParamsChange();
        }
    } else if (m == selectFF) {
        if( !mselected.empty() ) {
            rtengine::procparams::ProcParams pp = mselected[0]->thumbnail->getProcParams();
            Gtk::FileChooserDialog fc (getToplevelWindow (this), "Flat Field", Gtk::FILE_CHOOSER_ACTION_OPEN );
            bindCurrentFolder (fc, App::get().mut_options().lastFlatfieldDir);
            fc.add_button( M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
            fc.add_button( M("GENERAL_APPLY"), Gtk::RESPONSE_APPLY);

            if(!pp.raw.ff_file.empty()) {
                fc.set_filename( pp.raw.ff_file );
            }

            if( fc.run() == Gtk::RESPONSE_APPLY ) {
                if (bppcl) {
                    bppcl->beginBatchPParamsChange(mselected.size());
                }

                for (size_t i = 0; i < mselected.size(); i++) {
                    rtengine::procparams::ProcParams lpp = mselected[i]->thumbnail->getProcParams();
                    lpp.raw.ff_file = fc.get_filename();
                    lpp.raw.ff_AutoSelect = false;
                    mselected[i]->thumbnail->setProcParams(lpp, nullptr, FILEBROWSER, false);
                }

                if (bppcl) {
                    bppcl->endBatchPParamsChange();
                }
            }
        }
    } else if( m == thisIsFF) {
        if( !options.rtSettings.flatFieldsPath.empty()) {
            if (Gio::File::create_for_path(options.rtSettings.flatFieldsPath)->query_exists() ) {
                for (size_t i = 0; i < mselected.size(); i++) {
                    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path ( mselected[i]->filename );

                    if( !file ) {
                        continue;
                    }

                    Glib::ustring destName = options.rtSettings.flatFieldsPath + "/" + file->get_basename();
                    Glib::RefPtr<Gio::File> dest = Gio::File::create_for_path ( destName );
                    file->move(  dest );
                }

                // Reinit cache
                rtengine::ffm.init( options.rtSettings.flatFieldsPath );
            } else {
                // Target directory creation failed, we clear the flatFieldsPath setting
                App::get().mut_options().rtSettings.flatFieldsPath.clear();
                Glib::ustring msg_ = Glib::ustring::compose (M("MAIN_MSG_PATHDOESNTEXIST"), escapeHtmlChars(options.rtSettings.flatFieldsPath))
                                     + "\n\n" + M("MAIN_MSG_OPERATIONCANCELLED");
                Gtk::MessageDialog msgd (msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                msgd.set_title(M("TP_FLATFIELD_LABEL"));
                msgd.run ();
            }
        } else {
            Glib::ustring msg_ = M("MAIN_MSG_SETPATHFIRST") + "\n\n" + M("MAIN_MSG_OPERATIONCANCELLED");
            Gtk::MessageDialog msgd (msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            msgd.set_title(M("TP_FLATFIELD_LABEL"));
            msgd.run ();
        }
    } else if (m == copyprof) {
        copyProfile ();
    } else if (m == pasteprof) {
        pasteProfile ();
    } else if (m == partpasteprof) {
        partPasteProfile ();
    } else if (m == clearprof) {
        for (size_t i = 0; i < mselected.size(); i++) {
            mselected[i]->thumbnail->clearProcParams (FILEBROWSER);
        }

        queue_draw ();
    } else if (m == resetdefaultprof) {
        if (!mselected.empty() && bppcl) {
            bppcl->beginBatchPParamsChange(mselected.size());
        }

        for (size_t i = 0; i < mselected.size(); i++)  {
            const auto thumbnail = mselected[i]->thumbnail;
            const auto rank = thumbnail->getRank();
            const auto colorLabel = thumbnail->getColorLabel();
            const auto stage = thumbnail->getTrashed();

            thumbnail->createProcParamsForUpdate (false, true);
            thumbnail->setRank(rank);
            thumbnail->setColorLabel(colorLabel);
            thumbnail->setTrashed(stage);

            // Empty run to update the thumb
            rtengine::procparams::ProcParams params = thumbnail->getProcParams ();
            thumbnail->setProcParams (params, nullptr, FILEBROWSER, true, true);
        }

        if (!mselected.empty() && bppcl) {
            bppcl->endBatchPParamsChange();
        }
    } else if (m == clearFromCache) {
        tbl->clearFromCacheRequested (mselected, false);

        //queue_draw ();
    } else if (m == clearFromCacheFull) {
        tbl->clearFromCacheRequested (mselected, true);

        //queue_draw ();
#ifdef _WIN32
    } else if (miOpenDefaultViewer && m == miOpenDefaultViewer) {
        openDefaultViewer(1);
#endif
    }
}

void FileBrowser::copyProfile ()
{
    MYREADERLOCK(l, entryRW);

    if (selected.size() == 1) {
        clipboard.setProcParams ((static_cast<FileBrowserEntry*>(selected[0]))->thumbnail->getProcParams());
    }
}

void FileBrowser::pasteProfile ()
{

    if (clipboard.hasProcParams()) {
        std::vector<FileBrowserEntry*> mselected;
        {
            MYREADERLOCK(l, entryRW);

            for (unsigned int i = 0; i < selected.size(); i++) {
                mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
            }
        }

        if (!tbl || mselected.empty()) {
            return;
        }

        if (!mselected.empty() && bppcl) {
            bppcl->beginBatchPParamsChange(mselected.size());
        }

        for (unsigned int i = 0; i < mselected.size(); i++) {
            // copying read only clipboard PartialProfile to a temporary one
            const rtengine::procparams::PartialProfile& cbPartProf = clipboard.getPartialProfile();
            rtengine::procparams::PartialProfile pastedPartProf(cbPartProf.pparams, cbPartProf.pedited, true);

            // applying the PartialProfile to the thumb's ProcParams
            mselected[i]->thumbnail->setProcParams (*pastedPartProf.pparams, pastedPartProf.pedited, FILEBROWSER);
            pastedPartProf.deleteInstance();
        }

        if (!mselected.empty() && bppcl) {
            bppcl->endBatchPParamsChange();
        }

        queue_draw ();
    }
}

void FileBrowser::partPasteProfile ()
{

    if (clipboard.hasProcParams()) {

        std::vector<FileBrowserEntry*> mselected;
        {
            MYREADERLOCK(l, entryRW);

            for (unsigned int i = 0; i < selected.size(); i++) {
                mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
            }
        }

        if (!tbl || mselected.empty()) {
            return;
        }

        auto toplevel = static_cast<Gtk::Window*> (get_toplevel ());
        PartialPasteDlg partialPasteDlg (M("PARTIALPASTE_DIALOGLABEL"), toplevel);

        partialPasteDlg.updateSpotWidget(clipboard.getPartialProfile().pparams);

        int i = partialPasteDlg.run ();

        if (i == Gtk::RESPONSE_OK) {

            if (!mselected.empty() && bppcl) {
                bppcl->beginBatchPParamsChange(mselected.size());
            }

            for (auto entry : mselected) {
                // copying read only clipboard PartialProfile to a temporary one, initialized to the thumb's ProcParams
                entry->thumbnail->createProcParamsForUpdate(false, false); // this can execute customprofilebuilder to generate param file
                const rtengine::procparams::PartialProfile& cbPartProf = clipboard.getPartialProfile();
                rtengine::procparams::PartialProfile pastedPartProf(&entry->thumbnail->getProcParams (), nullptr);

                // pushing the selected values of the clipboard PartialProfile to the temporary PartialProfile
                partialPasteDlg.applyPaste (pastedPartProf.pparams, pastedPartProf.pedited, cbPartProf.pparams, cbPartProf.pedited);

                // applying the temporary PartialProfile to the thumb's ProcParams
                entry->thumbnail->setProcParams (*pastedPartProf.pparams, pastedPartProf.pedited, FILEBROWSER);
                pastedPartProf.deleteInstance();
            }

            if (!mselected.empty() && bppcl) {
                bppcl->endBatchPParamsChange();
            }

            queue_draw ();
        }

        partialPasteDlg.hide ();
    }
}

#ifdef _WIN32
void FileBrowser::openDefaultViewer (int destination)
{
    bool success = true;

    {
        MYREADERLOCK(l, entryRW);

        if (selected.size() == 1) {
            success = (static_cast<FileBrowserEntry*>(selected[0]))->thumbnail->openDefaultViewer(destination);
        }
    }

    if (!success) {
        Gtk::MessageDialog msgd(getToplevelWindow(this), M("MAIN_MSG_IMAGEUNPROCESSED"), true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        msgd.run ();
    }
}
#endif

bool FileBrowser::keyPressed (GdkEventKey* event)
{
    bool ctrl  = event->state & GDK_CONTROL_MASK;
    bool shift = event->state & GDK_SHIFT_MASK;
    bool alt   = event->state & GDK_MOD1_MASK;
#ifdef __WIN32__
    bool altgr = event->state & GDK_MOD2_MASK;
#endif

    if ((event->keyval == GDK_KEY_C || event->keyval == GDK_KEY_c) && ctrl && shift) {
        menuItemActivated (copyTo);
        return true;
    } else if ((event->keyval == GDK_KEY_M || event->keyval == GDK_KEY_m) && ctrl && shift) {
        menuItemActivated (moveTo);
        return true;
    } else if ((event->keyval == GDK_KEY_C || event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_Insert) && ctrl) {
        copyProfile ();
        return true;
    } else if ((event->keyval == GDK_KEY_V || event->keyval == GDK_KEY_v) && ctrl && !shift) {
        pasteProfile ();
        return true;
    } else if (event->keyval == GDK_KEY_Insert && shift) {
        pasteProfile ();
        return true;
    } else if ((event->keyval == GDK_KEY_V || event->keyval == GDK_KEY_v) && ctrl && shift) {
        partPasteProfile ();
        return true;
    } else if (event->keyval == GDK_KEY_Delete && !shift) {
        menuItemActivated (trash);
        return true;
    } else if (event->keyval == GDK_KEY_Delete && shift) {
        menuItemActivated (untrash);
        return true;
    } else if ((event->keyval == GDK_KEY_B || event->keyval == GDK_KEY_b) && ctrl && !shift) {
        menuItemActivated (develop);
        return true;
    } else if ((event->keyval == GDK_KEY_B || event->keyval == GDK_KEY_b) && ctrl && shift) {
        menuItemActivated (developfast);
        return true;
    } else if ((event->keyval == GDK_KEY_A || event->keyval == GDK_KEY_a) && ctrl) {
        menuItemActivated (selall);
        return true;
    } else if (event->keyval == GDK_KEY_F2 && !ctrl) {
        menuItemActivated (rename);
        return true;
    } else if (event->keyval == GDK_KEY_F3 && !(ctrl || shift || alt)) { // open Previous image from FileBrowser perspective
        FileBrowser::openPrevImage ();
        return true;
    } else if (event->keyval == GDK_KEY_F4 && !(ctrl || shift || alt)) { // open Next image from FileBrowser perspective
        FileBrowser::openNextImage ();
        return true;
    } else if (event->keyval == GDK_KEY_Left) {
        selectPrev (1, shift);
        return true;
    } else if (event->keyval == GDK_KEY_Right) {
        selectNext (1, shift);
        return true;
    } else if (event->keyval == GDK_KEY_Up) {
        selectPrev (numOfCols, shift);
        return true;
    } else if (event->keyval == GDK_KEY_Down) {
        selectNext (numOfCols, shift);
        return true;
    } else if (event->keyval == GDK_KEY_Home) {
        selectFirst (shift);
        return true;
    } else if (event->keyval == GDK_KEY_End) {
        selectLast (shift);
        return true;
    } else if(event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        std::vector<FileBrowserEntry*> mselected;

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }

        openRequested(mselected);
#ifdef _WIN32
    } else if (event->keyval == GDK_KEY_F5) {
        int dest = 1;

        if (event->state & GDK_SHIFT_MASK) {
            dest = 2;
        } else if (event->state & GDK_CONTROL_MASK) {
            dest = 3;
        }

        openDefaultViewer (dest);
        return true;
#endif
    } else if (event->keyval == GDK_KEY_Page_Up) {
        scrollPage(GDK_SCROLL_UP);
        return true;
    } else if (event->keyval == GDK_KEY_Page_Down) {
        scrollPage(GDK_SCROLL_DOWN);
        return true;
    }

#ifdef __WIN32__
    else if (!shift && !ctrl && !alt && !altgr) { // rank
        switch(event->hardware_keycode) {
        case 0x30:  // 0-key
            requestRanking (0);
            return true;

        case 0x31:  // 1-key
            requestRanking (1);
            return true;

        case 0x32:  // 2-key
            requestRanking (2);
            return true;

        case 0x33:  // 3-key
            requestRanking (3);
            return true;

        case 0x34:  // 4-key
            requestRanking (4);
            return true;

        case 0x35:  // 5-key
            requestRanking (5);
            return true;
        }
    } else if (shift && ctrl && !alt && !altgr) { // color labels
        switch(event->hardware_keycode) {
        case 0x30:  // 0-key
            requestColorLabel (0);
            return true;

        case 0x31:  // 1-key
            requestColorLabel (1);
            return true;

        case 0x32:  // 2-key
            requestColorLabel (2);
            return true;

        case 0x33:  // 3-key
            requestColorLabel (3);
            return true;

        case 0x34:  // 4-key
            requestColorLabel (4);
            return true;

        case 0x35:  // 5-key
            requestColorLabel (5);
            return true;
        }
    }

#else
    else if (!shift && !ctrl && !alt) { // rank
        switch(event->hardware_keycode) {
        case 0x13:
            requestRanking (0);
            return true;

        case 0x0a:
            requestRanking (1);
            return true;

        case 0x0b:
            requestRanking (2);
            return true;

        case 0x0c:
            requestRanking (3);
            return true;

        case 0x0d:
            requestRanking (4);
            return true;

        case 0x0e:
            requestRanking (5);
            return true;
        }
    } else if (shift && ctrl && !alt) { // color labels
        switch(event->hardware_keycode) {
        case 0x13:
            requestColorLabel (0);
            return true;

        case 0x0a:
            requestColorLabel (1);
            return true;

        case 0x0b:
            requestColorLabel (2);
            return true;

        case 0x0c:
            requestColorLabel (3);
            return true;

        case 0x0d:
            requestColorLabel (4);
            return true;

        case 0x0e:
            requestColorLabel (5);
            return true;
        }
    }

#endif

    return false;
}

void FileBrowser::saveThumbnailHeight (int height)
{
    auto& options = App::get().mut_options();
    if (!options.sameThumbSize && getLocation() == THLOC_EDITOR) {
        options.thumbSizeTab = height;
    } else {
        options.thumbSize = height;
    }
}

int FileBrowser::getThumbnailHeight ()
{
    const auto& options = App::get().options();
    // The user could have manually forced the option to a too big value
    if (!options.sameThumbSize && getLocation() == THLOC_EDITOR) {
        return std::max(std::min(options.thumbSizeTab, 800), 10);
    } else {
        return std::max(std::min(options.thumbSize, 800), 10);
    }
}

void FileBrowser::enableTabMode(bool enable)
{
    ThumbBrowserBase::enableTabMode(enable);
    const auto& options = App::get().options();
    if (options.inspectorWindow) {
        if (enable) {
            inspect->remove_accelerator(pmenu->get_accel_group(), GDK_KEY_f, (Gdk::ModifierType)0);
        }
        else {
            inspect->add_accelerator ("activate", pmenu->get_accel_group(), GDK_KEY_f, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
        }
    }
}

void FileBrowser::applyMenuItemActivated (ProfileStoreLabel *label)
{
    MYREADERLOCK(l, entryRW);

    const rtengine::procparams::PartialProfile* partProfile = ProfileStore::getInstance()->getProfile (label->entry);

    if (partProfile->pparams && !selected.empty()) {
        if (bppcl) {
            bppcl->beginBatchPParamsChange(selected.size());
        }

        for (size_t i = 0; i < selected.size(); i++) {
            (static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->setProcParams (*partProfile->pparams, partProfile->pedited, FILEBROWSER);
        }

        if (bppcl) {
            bppcl->endBatchPParamsChange();
        }

        queue_draw ();
    }
}

void FileBrowser::applyPartialMenuItemActivated (ProfileStoreLabel *label)
{

    {
        MYREADERLOCK(l, entryRW);

        if (!tbl || selected.empty()) {
            return;
        }
    }

    const rtengine::procparams::PartialProfile* srcProfiles = ProfileStore::getInstance()->getProfile (label->entry);

    if (srcProfiles->pparams) {

        auto toplevel = static_cast<Gtk::Window*> (get_toplevel ());
        PartialPasteDlg partialPasteDlg (M("PARTIALPASTE_DIALOGLABEL"), toplevel);

        partialPasteDlg.updateSpotWidget(srcProfiles->pparams);

        if (partialPasteDlg.run() == Gtk::RESPONSE_OK) {

            MYREADERLOCK(l, entryRW);

            if (bppcl) {
                bppcl->beginBatchPParamsChange(selected.size());
            }

            for (size_t i = 0; i < selected.size(); i++) {
                selected[i]->thumbnail->createProcParamsForUpdate(false, false);  // this can execute customprofilebuilder to generate param file

                rtengine::procparams::PartialProfile dstProfile(true);
                *dstProfile.pparams = (static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->getProcParams ();
                dstProfile.set(true);
                dstProfile.pedited->locallab.spots.resize(dstProfile.pparams->locallab.spots.size(), LocallabParamsEdited::LocallabSpotEdited(true));
                partialPasteDlg.applyPaste (dstProfile.pparams, dstProfile.pedited, srcProfiles->pparams, srcProfiles->pedited);
                (static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->setProcParams (*dstProfile.pparams, dstProfile.pedited, FILEBROWSER);
                dstProfile.deleteInstance();
            }

            if (bppcl) {
                bppcl->endBatchPParamsChange();
            }

            queue_draw ();
        }

        partialPasteDlg.hide ();
    }
}

void FileBrowser::applyFilter (const BrowserFilter& filter)
{

    this->filter = filter;

    // remove items not complying the filter from the selection
    bool selchanged = false;
    numFiltered = 0;
    {
        MYWRITERLOCK(l, entryRW);

        if (filter.showOriginal) {
            findOriginalEntries(fd);
        }

        for (size_t i = 0; i < fd.size(); i++) {
            if (checkFilter(fd[i])) {
                numFiltered++;
            } else if (fd[i]->selected) {
                fd[i]->selected = false;
                std::vector<ThumbBrowserEntryBase*>::iterator j = std::find(selected.begin(), selected.end(), fd[i]);
                selected.erase(j);

                if (lastClicked == fd[i]) {
                    lastClicked = nullptr;
                }

                selchanged = true;
            }
        }
        if (selected.empty() || (anchor && std::find(selected.begin(), selected.end(), anchor) == selected.end())) {
            anchor = nullptr;
        }
    }

    if (selchanged) {
        notifySelectionListener ();
    }

    tbl->filterApplied();
    redraw ();
}

bool FileBrowser::checkFilter (ThumbBrowserEntryBase* entryb) const   // true -> entry complies filter
{

    FileBrowserEntry* entry = static_cast<FileBrowserEntry*>(entryb);

    if (filter.showOriginal && entry->getOriginal()) {
        return false;
    }

    // return false if basic filter settings are not satisfied
    if ((!filter.showRanked[entry->thumbnail->getRank()] ) ||
            (!filter.showCLabeled[entry->thumbnail->getColorLabel()] ) ||

            ((entry->thumbnail->hasProcParams() && filter.showEdited[0]) && !filter.showEdited[1]) ||
            ((!entry->thumbnail->hasProcParams() && filter.showEdited[1]) && !filter.showEdited[0]) ||

            ((entry->thumbnail->isRecentlySaved() && filter.showRecentlySaved[0]) && !filter.showRecentlySaved[1]) ||
            ((!entry->thumbnail->isRecentlySaved() && filter.showRecentlySaved[1]) && !filter.showRecentlySaved[0]) ||

            (entry->thumbnail->getTrashed() && !filter.showTrash) ||
            (!entry->thumbnail->getTrashed() && !filter.showNotTrash)) {
        return false;
    }

    // return false if query is not satisfied
    if (!filter.vFilterStrings.empty()) {
        // check if image's FileName contains queryFileName (case insensitive)
        // TODO should we provide case-sensitive search option via preferences?
        std::string FileName = Glib::path_get_basename(entry->thumbnail->getFileName());
        std::transform(FileName.begin(), FileName.end(), FileName.begin(), ::toupper);
        int iFilenameMatch = 0;

        for (const auto& filterString : filter.vFilterStrings) {
            if (FileName.find(filterString) != std::string::npos) {
                ++iFilenameMatch;
                break;
            }
        }

        if (filter.matchEqual) {
            if (iFilenameMatch == 0) { //none of the vFilterStrings found in FileName
                return false;
            }
        } else {
            if (iFilenameMatch > 0) { // match is found for at least one of vFilterStrings in FileName
                return false;
            }
        }
    }

    if (!filter.exifFilterEnabled) {
        return true;
    }

    // check exif filter
    const CacheImageData* cfs = entry->thumbnail->getCacheImageData();
    double tol = 0.01;
    double tol2 = 1e-8;

    if (!cfs->exifValid) {
        return (!filter.exifFilter.filterCamera || filter.exifFilter.cameras.count(cfs->getCamera()) > 0)
               && (!filter.exifFilter.filterLens || filter.exifFilter.lenses.count(cfs->lens) > 0)
               && (!filter.exifFilter.filterFiletype || filter.exifFilter.filetypes.count(cfs->filetype) > 0)
               && (!filter.exifFilter.filterExpComp || filter.exifFilter.expcomp.count(cfs->expcomp) > 0);
    }

    return
        (!filter.exifFilter.filterShutter || (rtengine::FramesMetaData::shutterFromString(rtengine::FramesMetaData::shutterToString(cfs->shutter)) >= filter.exifFilter.shutterFrom - tol2 && rtengine::FramesMetaData::shutterFromString(rtengine::FramesMetaData::shutterToString(cfs->shutter)) <= filter.exifFilter.shutterTo + tol2))
        && (!filter.exifFilter.filterFNumber || (rtengine::FramesMetaData::apertureFromString(rtengine::FramesMetaData::apertureToString(cfs->fnumber)) >= filter.exifFilter.fnumberFrom - tol2 && rtengine::FramesMetaData::apertureFromString(rtengine::FramesMetaData::apertureToString(cfs->fnumber)) <= filter.exifFilter.fnumberTo + tol2))
        && (!filter.exifFilter.filterFocalLen || (cfs->focalLen >= filter.exifFilter.focalFrom - tol && cfs->focalLen <= filter.exifFilter.focalTo + tol))
        && (!filter.exifFilter.filterISO     || (cfs->iso >= filter.exifFilter.isoFrom && cfs->iso <= filter.exifFilter.isoTo))
        && (!filter.exifFilter.filterExpComp || filter.exifFilter.expcomp.count(cfs->expcomp) > 0)
        && (!filter.exifFilter.filterCamera  || filter.exifFilter.cameras.count(cfs->getCamera()) > 0)
        && (!filter.exifFilter.filterLens    || filter.exifFilter.lenses.count(cfs->lens) > 0)
        && (!filter.exifFilter.filterFiletype  || filter.exifFilter.filetypes.count(cfs->filetype) > 0);
}

void FileBrowser::toTrashRequested (std::vector<FileBrowserEntry*> tbe)
{

    for (size_t i = 0; i < tbe.size(); i++) {
        // try to load the last saved parameters from the cache or from the paramfile file
        tbe[i]->thumbnail->createProcParamsForUpdate(false, false, true);  // this can execute customprofilebuilder to generate param file in "flagging" mode

        // no need to notify listeners as item goes to trash, likely to be deleted

        if (tbe[i]->thumbnail->getTrashed()) {
            continue;
        }

        tbe[i]->thumbnail->setTrashed (true);

        if (tbe[i]->getThumbButtonSet()) {
            tbe[i]->getThumbButtonSet()->setRank (tbe[i]->thumbnail->getRank());
            tbe[i]->getThumbButtonSet()->setColorLabel (tbe[i]->thumbnail->getColorLabel());
            tbe[i]->getThumbButtonSet()->setInTrash (true);
            tbe[i]->thumbnail->updateCache (); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file
        }
    }

    trash_changed().emit();
    applyFilter (filter);
}

void FileBrowser::fromTrashRequested (std::vector<FileBrowserEntry*> tbe)
{

    for (size_t i = 0; i < tbe.size(); i++) {
        // if thumbnail was marked inTrash=true then param file must be there, no need to run customprofilebuilder

        if (!tbe[i]->thumbnail->getTrashed()) {
            continue;
        }

        tbe[i]->thumbnail->setTrashed (false);

        if (tbe[i]->getThumbButtonSet()) {
            tbe[i]->getThumbButtonSet()->setRank (tbe[i]->thumbnail->getRank());
            tbe[i]->getThumbButtonSet()->setColorLabel (tbe[i]->thumbnail->getColorLabel());
            tbe[i]->getThumbButtonSet()->setInTrash (false);
            tbe[i]->thumbnail->updateCache (); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file
        }
    }

    trash_changed().emit();
    applyFilter (filter);
}

void FileBrowser::sortMethodRequested (int method)
{
    auto& options = App::get().mut_options();
    options.sortMethod = Options::SortMethod(method);
    resort ();
}

void FileBrowser::sortOrderRequested (int order)
{
    auto& options = App::get().mut_options();
    options.sortDescending = !!order;
    resort ();
}

void FileBrowser::rankingRequested (std::vector<FileBrowserEntry*> tbe, int rank)
{

    if (!tbe.empty() && bppcl) {
        bppcl->beginBatchPParamsChange(tbe.size());
    }

    for (size_t i = 0; i < tbe.size(); i++) {

        // try to load the last saved parameters from the cache or from the paramfile file
        tbe[i]->thumbnail->createProcParamsForUpdate(false, false, true);  // this can execute customprofilebuilder to generate param file in "flagging" mode

        // notify listeners TODO: should do this ONLY when params changed by customprofilebuilder?
        tbe[i]->thumbnail->notifylisterners_procParamsChanged(FILEBROWSER);

        tbe[i]->thumbnail->setRank (rank);
        tbe[i]->thumbnail->updateCache (); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file
        //TODO? - should update pparams instead?

        if (tbe[i]->getThumbButtonSet()) {
            tbe[i]->getThumbButtonSet()->setRank (tbe[i]->thumbnail->getRank());
        }
    }

    applyFilter (filter);

    if (!tbe.empty() && bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::colorlabelRequested (std::vector<FileBrowserEntry*> tbe, int colorlabel)
{

    if (!tbe.empty() && bppcl) {
        bppcl->beginBatchPParamsChange(tbe.size());
    }

    for (size_t i = 0; i < tbe.size(); i++) {
        // try to load the last saved parameters from the cache or from the paramfile file
        tbe[i]->thumbnail->createProcParamsForUpdate(false, false, true);  // this can execute customprofilebuilder to generate param file in "flagging" mode

        // notify listeners TODO: should do this ONLY when params changed by customprofilebuilder?
        tbe[i]->thumbnail->notifylisterners_procParamsChanged(FILEBROWSER);

        tbe[i]->thumbnail->setColorLabel (colorlabel);
        tbe[i]->thumbnail->updateCache(); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file

        //TODO? - should update pparams instead?
        if (tbe[i]->getThumbButtonSet()) {
            tbe[i]->getThumbButtonSet()->setColorLabel (tbe[i]->thumbnail->getColorLabel());
        }
    }

    applyFilter (filter);

    if (!tbe.empty() && bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::requestRanking(int rank)
{
    std::vector<FileBrowserEntry*> mselected;
    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }
    }

    rankingRequested (mselected, rank);
}

void FileBrowser::requestColorLabel(int colorlabel)
{
    std::vector<FileBrowserEntry*> mselected;
    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }
    }

    colorlabelRequested (mselected, colorlabel);
}

void FileBrowser::buttonPressed (LWButton* button, int actionCode, void* actionData, int x, int y)
{

    if (actionCode >= 0 && actionCode <= 5) { // rank
        std::vector<FileBrowserEntry*> tbe;
        tbe.push_back (static_cast<FileBrowserEntry*>(actionData));
        rankingRequested (tbe, actionCode);
    } else if (actionCode == 6 && tbl) { // to processing queue
        std::vector<FileBrowserEntry*> tbe;
        tbe.push_back (static_cast<FileBrowserEntry*>(actionData));
        tbl->developRequested (tbe, false); // not a fast, but a FULL mode
    } else if (actionCode == 7) { // to trash / undelete
        std::vector<FileBrowserEntry*> tbe;
        FileBrowserEntry* entry = static_cast<FileBrowserEntry*>(actionData);
        tbe.push_back (entry);

        if (!entry->thumbnail->getTrashed()) {
            toTrashRequested (tbe);
        } else {
            fromTrashRequested (tbe);
        }
    } else if (actionCode == 8 && tbl) { // color label
        // show popup menu
        colorLabel_actionData = actionData;// this will be reused when pmenuColorLabels is clicked
        const Gdk::Rectangle rect(x, y, 1, 1);
        pmenuColorLabels->popup_at_rect(internal.get_window(), rect, Gdk::GRAVITY_NORTH_WEST, Gdk::GRAVITY_NORTH_WEST, nullptr);
    }
}

void FileBrowser::openNextImage()
{
    MYWRITERLOCK(l, entryRW);

    if (!fd.empty() && selected.size() > 0 && !App::get().options().tabbedUI) {
        for (size_t i = 0; i < fd.size() - 1; i++) {
            if (selected[0]->thumbnail->getFileName() == fd[i]->filename) { // located 1-st image in current selection
                if (i < fd.size() && tbl) {
                    // find the first not-filtered-out (next) image
                    for (size_t k = i + 1; k < fd.size(); k++) {
                        if (!fd[k]->filtered/*checkFilter (fd[k])*/) {

                            // clear current selection
                            for (size_t j = 0; j < selected.size(); j++) {
                                selected[j]->selected = false;
                            }

                            selected.clear();

                            // set new selection
                            fd[k]->selected = true;
                            selected.push_back(fd[k]);
                            //queue_draw();

                            MYWRITERLOCK_RELEASE(l);

                            // this will require a read access
                            notifySelectionListener();

                            MYWRITERLOCK_ACQUIRE(l);

                            // scroll to the selected position, centered horizontally in the container
                            double x1, y1;
                            getScrollPosition(x1, y1);

                            double x2 = selected[0]->getStartX();
                            double y2 = selected[0]->getStartY();

                            Thumbnail* thumb = (static_cast<FileBrowserEntry*>(fd[k]))->thumbnail;
                            int tw = fd[k]->getMinimalWidth(); // thumb width

                            int ww = get_width(); // window width

                            MYWRITERLOCK_RELEASE(l);

                            // scroll only when selected[0] is outside of the displayed bounds
                            // or less than a thumbnail's width from either edge.
                            if ((x2 > x1 + ww - 1.5 * tw) || (x2 - tw / 2 < x1)) {
                                setScrollPosition(x2 - (ww - tw) / 2, y2);
                            }

                            // open the selected image
                            tbl->openRequested({thumb});

                            return;
                        }
                    }
                }
            }
        }
    }
}

void FileBrowser::openPrevImage()
{
    MYWRITERLOCK(l, entryRW);

    if (!fd.empty() && selected.size() > 0 && !App::get().options().tabbedUI) {
        for (size_t i = 1; i < fd.size(); i++) {
            if (selected[0]->thumbnail->getFileName() == fd[i]->filename) { // located 1-st image in current selection
                if (i > 0 && tbl) {
                    // find the first not-filtered-out (previous) image
                    for (ssize_t k = (ssize_t)i - 1; k >= 0; k--) {
                        if (!fd[k]->filtered/*checkFilter (fd[k])*/) {

                            // clear current selection
                            for (size_t j = 0; j < selected.size(); j++) {
                                selected[j]->selected = false;
                            }

                            selected.clear();

                            // set new selection
                            fd[k]->selected = true;
                            selected.push_back(fd[k]);
                            //queue_draw();

                            MYWRITERLOCK_RELEASE(l);

                            // this will require a read access
                            notifySelectionListener();

                            MYWRITERLOCK_ACQUIRE(l);

                            // scroll to the selected position, centered horizontally in the container
                            double x1, y1;
                            getScrollPosition(x1, y1);

                            double x2 = selected[0]->getStartX();
                            double y2 = selected[0]->getStartY();

                            Thumbnail* thumb = (static_cast<FileBrowserEntry*>(fd[k]))->thumbnail;
                            int tw = fd[k]->getMinimalWidth(); // thumb width

                            int ww = get_width(); // window width

                            MYWRITERLOCK_RELEASE(l);

                            // scroll only when selected[0] is outside of the displayed bounds
                            // or less than a thumbnail's width from either edge.
                            if ((x2 > x1 + ww - 1.5 * tw) || (x2 - tw / 2 < x1)) {
                                setScrollPosition(x2 - (ww - tw) / 2, y2);
                            }

                            // open the selected image
                            tbl->openRequested({thumb});

                            return;
                        }
                    }
                }
            }
        }
    }
}

void FileBrowser::selectImage(const Glib::ustring& fname, bool doScroll)
{
    MYWRITERLOCK(l, entryRW);

    if (!fd.empty() && !App::get().options().tabbedUI) {
        for (size_t i = 0; i < fd.size(); i++) {
            if (fname == fd[i]->filename && !fd[i]->filtered) {
                // matching file found for sync

                // clear current selection
                for (size_t j = 0; j < selected.size(); j++) {
                    selected[j]->selected = false;
                }

                selected.clear();

                // set new selection
                fd[i]->selected = true;
                selected.push_back(fd[i]);
                queue_draw();

                MYWRITERLOCK_RELEASE(l);

                // this will require a read access
                notifySelectionListener();

                MYWRITERLOCK_ACQUIRE(l);

                // scroll to the selected position, centered horizontally in the container
                double x = selected[0]->getStartX();
                double y = selected[0]->getStartY();

                int tw = fd[i]->getMinimalWidth(); // thumb width

                int ww = get_width(); // window width

                MYWRITERLOCK_RELEASE(l);

                if (doScroll) {
                    // Center thumb
                    setScrollPosition(x - (ww - tw) / 2, y);
                }

                return;
            }
        }
    }
}

void FileBrowser::openNextPreviousEditorImage (const Glib::ustring& fname, eRTNav nextPrevious)
{

    // let FileBrowser acquire Editor's perspective
    selectImage (fname, false);

    // now switch to the requested image
    if (nextPrevious == NAV_NEXT) {
        openNextImage();
    } else if (nextPrevious == NAV_PREVIOUS) {
        openPrevImage();
    }
}

void FileBrowser::thumbRearrangementNeeded ()
{
    idle_register.add(
        [this]() -> bool
        {
            refreshThumbImages();// arrangeFiles is NOT enough
            return false;
        }
    );
}

void FileBrowser::selectionChanged ()
{

    notifySelectionListener ();
}

void FileBrowser::notifySelectionListener ()
{

    if (tbl) {
        MYREADERLOCK(l, entryRW);

        std::vector<Thumbnail*> thm;

        for (size_t i = 0; i < selected.size(); i++) {
            thm.push_back ((static_cast<FileBrowserEntry*>(selected[i]))->thumbnail);
        }

        tbl->selectionChanged (thm);
    }
}

void FileBrowser::redrawNeeded (LWButton* button)
{
    GThreadLock lock;
    queue_draw ();
}
FileBrowser::type_trash_changed FileBrowser::trash_changed ()
{
    return m_trash_changed;
}


// ExportPanel interface
void FileBrowser::exportRequested ()
{
    FileBrowser::menuItemActivated(developfast);
}

void FileBrowser::setExportPanel (ExportPanel* expanel)
{

    exportPanel = expanel;
    exportPanel->set_sensitive (false);
    exportPanel->setExportPanelListener (this);
}

void FileBrowser::storeCurrentValue()
{
}

void FileBrowser::updateProfileList()
{
    // submenu applmenu
    int p = 0;

    const std::vector<const ProfileStoreEntry*> *profEntries = ProfileStore::getInstance()->getFileList();  // lock and get a pointer to the profiles' list

    std::map<unsigned short /* folderId */, Gtk::Menu*> subMenuList;  // store the Gtk::Menu that Gtk::MenuItem will have to be attached to

    subMenuList[0] = Gtk::manage (new Gtk::Menu ()); // adding the root submenu

    const auto& options = App::get().options();

    // iterate the profile store's profile list
    for (size_t i = 0; i < profEntries->size(); i++) {
        // create a new label for the current entry (be it a folder or file)
        ProfileStoreLabel *currLabel = Gtk::manage(new ProfileStoreLabel( profEntries->at(i) ));

        // create the MenuItem object
        Gtk::MenuItem* mi = Gtk::manage (new Gtk::MenuItem (*currLabel));

        // create a new Menu object if the entry is a folder and not the root one
        if (currLabel->entry->type == PSET_FOLDER) {
            // creating the new sub-menu
            Gtk::Menu* subMenu = Gtk::manage (new Gtk::Menu ());

            // add it to the menu list
            subMenuList[currLabel->entry->folderId] = subMenu;

            // add it to the parent MenuItem
            mi->set_submenu(*subMenu);
        }

        // Hombre: ... does parentMenuId sounds like a hack?         ... Yes.
        int parentMenuId = !options.useBundledProfiles && currLabel->entry->parentFolderId == 1 ? 0 : currLabel->entry->parentFolderId;
        subMenuList[parentMenuId]->attach (*mi, 0, 1, p, p + 1);
        p++;

        if (currLabel->entry->type == PSET_FILE) {
            mi->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::applyMenuItemActivated), currLabel));
        }

        mi->show ();
    }

    if (subMenuList.size() && applyprof)
        // TODO: Check that the previous one has been deleted, including all childrens
    {
        applyprof->set_submenu (*(subMenuList.at(0)));
    }

    subMenuList.clear();
    subMenuList[0] = Gtk::manage (new Gtk::Menu ()); // adding the root submenu
    // keep profEntries list

    // submenu applpartmenu
    p = 0;

    for (size_t i = 0; i < profEntries->size(); i++) {
        ProfileStoreLabel *currLabel = Gtk::manage(new ProfileStoreLabel( profEntries->at(i) ));

        Gtk::MenuItem* mi = Gtk::manage (new Gtk::MenuItem (*currLabel));

        if (currLabel->entry->type == PSET_FOLDER) {
            // creating the new sub-menu
            Gtk::Menu* subMenu = Gtk::manage (new Gtk::Menu ());

            // add it to the menu list
            subMenuList[currLabel->entry->folderId] = subMenu;

            // add it to the parent MenuItem
            mi->set_submenu(*subMenu);
        }

        // Hombre: ... does parentMenuId sounds like a hack?         ... yes.
        int parentMenuId = !options.useBundledProfiles && currLabel->entry->parentFolderId == 1 ? 0 : currLabel->entry->parentFolderId;
        subMenuList[parentMenuId]->attach (*mi, 0, 1, p, p + 1);
        p++;

        if (currLabel->entry->type == PSET_FILE) {
            mi->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &FileBrowser::applyPartialMenuItemActivated), currLabel));
        }

        mi->show ();
    }

    if (subMenuList.size() && applypartprof)
        // TODO: Check that the previous one has been deleted, including all childrens
    {
        applypartprof->set_submenu (*(subMenuList.at(0)));
    }

    ProfileStore::getInstance()->releaseFileList();
    subMenuList.clear();
}

void FileBrowser::restoreValue()
{
}

void FileBrowser::openRequested( std::vector<FileBrowserEntry*> mselected)
{
    std::vector<Thumbnail*> entries;
    // in Single Editor Mode open only last selected image
    size_t openStart = App::get().options().tabbedUI ? 0 : ( mselected.size() > 0 ? mselected.size() - 1 : 0);

    for (size_t i = openStart; i < mselected.size(); i++) {
        entries.push_back (mselected[i]->thumbnail);
    }

    tbl->openRequested (entries);
}

void FileBrowser::inspectRequested(std::vector<FileBrowserEntry*> mselected)
{
    getInspector()->showWindow(true);
}
