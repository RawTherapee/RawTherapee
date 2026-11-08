/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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
#include "filecatalog.h"

#include <algorithm>
#include <iterator>
#include <iostream>
#include <iomanip>

#include <glib/gstdio.h>

#include "rtengine/rt_math.h"
#include "rtengine/procparams.h"

#include "batchqueue.h"
#include "batchqueueentry.h"
#include "cachemanager.h"
#include "coarsepanel.h"
#include "filepanel.h"
#include "guiutils.h"
#include "inspector.h"
#include "multilangmgr.h"
#include "options.h"
#include "pathutils.h"
#include "placesbrowser.h"
#include "rtimage.h"
#include "thumbimageupdater.h"
#include "thumbnail.h"
#include "toolbar.h"
#include "windows/renamedlg.h"

using namespace std;

namespace {

int GetRecursionDepth(const Glib::ustring base, const Glib::RefPtr<Gio::File>& directory)
{
    if (!directory || base == directory->get_path()) {
        return 0;
    }

    int depth = 1;

    Glib::RefPtr<Gio::File> parent = directory->get_parent();
    while (parent && base != parent->get_path()) {
        depth++;
        parent = parent->get_parent();
    }

    return depth;
}

void getFilesRecursively(
    const Glib::ustring &dir_path,
    int max_depth,
    int &dir_quota,
    std::vector<Glib::ustring> &file_names,
    std::vector<Glib::RefPtr<Gio::File>> *directories_explored)
{
    const auto& options = App::get().options();
    try {
        const auto dir = Gio::File::create_for_path(dir_path);

        static const auto enumerate_attrs =
            std::string(G_FILE_ATTRIBUTE_STANDARD_NAME) + "," +
            G_FILE_ATTRIBUTE_STANDARD_TYPE + "," +
            G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN + "," +
            G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET;
        auto enumerator = dir->enumerate_children(
            enumerate_attrs,
            options.browseRecursiveFollowLinks
                ? Gio::FileQueryInfoFlags::FILE_QUERY_INFO_NONE
                : Gio::FileQueryInfoFlags::FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);

        if (directories_explored) {
            directories_explored->push_back(dir);
        }

        while (true) {
            try {
                const auto file = enumerator->next_file();
                if (!file) {
                    break;
                }
#ifdef _WIN32
                if (!file->has_attribute("standard::type")) {
                    continue;
                }
#endif
                if (!options.fbShowHidden && file->is_hidden()) {
                    continue;
                }

                if (file->get_file_type() == Gio::FILE_TYPE_DIRECTORY) {
                    if (max_depth > 0 && dir_quota > 0) {
                        const Glib::ustring child_dir_path = Glib::build_filename(dir_path, file->get_name());
                        getFilesRecursively(child_dir_path, max_depth - 1, --dir_quota, file_names, directories_explored);
                    }
                    continue;
                }

                const Glib::ustring fname = file->get_name();
                const auto lastdot = fname.find_last_of('.');

                if (lastdot >= fname.length() - 1) {
                    continue;
                }

                const auto& extensions = options.parsedExtensionsSet;
                if (extensions.find(fname.substr(lastdot + 1).lowercase()) == extensions.end()) {
                    continue;
                }

                file_names.emplace_back(Glib::build_filename(dir_path, fname));
            } catch (const Glib::Exception& exception) {
                if (rtengine::settings->verbose) {
                    std::cerr << exception.what() << std::endl;
                }
            }
        }

    } catch (const Glib::Exception& exception) {
        if (rtengine::settings->verbose) {
            std::cerr << "Failed to list directory \"" << dir_path << "\": " << exception.what() << std::endl;
        }
    }
}


} // namespace

FileCatalog::FileCatalog (CoarsePanel* cp, ToolBar* tb, FilePanel* filepanel) :
    filepanel(filepanel),
    selectedDirectoryId(1),
    actionNextPrevious(NAV_NONE),
    listener(nullptr),
    fslistener(nullptr),
    iatlistener(nullptr),
    hbToolBar1STB(nullptr),
    progressImage(nullptr),
    progressLabel(nullptr),
    hasValidCurrentEFS(false),
    filterPanel(nullptr),
    exportPanel(nullptr),
    previewsToLoad(0),
    modifierKey(0),
    coarsePanel(cp),
    toolBar(tb)
{

    set_orientation(Gtk::ORIENTATION_VERTICAL);

    inTabMode = false;

    set_name ("FileBrowser");

    //  construct and initialize thumbnail browsers
    fileBrowser = Gtk::manage( new FileBrowser() );
    fileBrowser->setFileBrowserListener (this);
    fileBrowser->setArrangement (ThumbBrowserBase::TB_Vertical);
    fileBrowser->show ();

    set_size_request(0, 250);
    // construct trash panel with the extra "empty trash" button
    trashButtonBox = Gtk::manage( new Gtk::Box(Gtk::ORIENTATION_VERTICAL) );
    Gtk::Button* emptyT = Gtk::manage( new Gtk::Button ());
    emptyT->set_tooltip_markup (M("FILEBROWSER_EMPTYTRASHHINT"));
    emptyT->set_image (*Gtk::manage(new RTImage ("trash-delete", Gtk::ICON_SIZE_LARGE_TOOLBAR)));
    emptyT->signal_pressed().connect (sigc::mem_fun(*this, &FileCatalog::emptyTrash));
    trashButtonBox->pack_start (*emptyT, Gtk::PACK_SHRINK, 4);
    emptyT->show ();
    trashButtonBox->show ();

    //initialize hbToolBar1
    hbToolBar1 = Gtk::manage(new Gtk::Box ());

    //setup BrowsePath
    iRefreshWhite = new RTImage("refresh-small", Gtk::ICON_SIZE_BUTTON);
    iRefreshRed = new RTImage("refresh-red-small", Gtk::ICON_SIZE_BUTTON);

    BrowsePath = Gtk::manage(new Gtk::Entry ());
    BrowsePath->set_width_chars (50);
    BrowsePath->set_tooltip_markup (M("FILEBROWSER_BROWSEPATHHINT"));
    Gtk::Box* hbBrowsePath = Gtk::manage(new Gtk::Box ());
    buttonBrowsePath = Gtk::manage(new Gtk::Button ());
    buttonBrowsePath->set_image (*iRefreshWhite);
    buttonBrowsePath->set_tooltip_markup (M("FILEBROWSER_BROWSEPATHBUTTONHINT"));
    buttonBrowsePath->set_relief (Gtk::RELIEF_NONE);
    buttonBrowsePath->signal_clicked().connect (sigc::mem_fun(*this, &FileCatalog::buttonBrowsePathPressed));
    hbBrowsePath->pack_start (*BrowsePath, Gtk::PACK_EXPAND_WIDGET, 0);
    hbBrowsePath->pack_start (*buttonBrowsePath, Gtk::PACK_SHRINK, 0);
    hbToolBar1->pack_start (*hbBrowsePath, Gtk::PACK_EXPAND_WIDGET, 0);

    BrowsePath->signal_activate().connect (sigc::mem_fun(*this, &FileCatalog::buttonBrowsePathPressed)); //respond to the Enter key
    BrowsePath->signal_key_press_event().connect(sigc::mem_fun(*this, &FileCatalog::BrowsePath_key_pressed));

    //setup Query
    iQueryClear = new RTImage("cancel-small", Gtk::ICON_SIZE_BUTTON);
    Gtk::Label* labelQuery = Gtk::manage(new Gtk::Label(M("FILEBROWSER_QUERYLABEL")));
    Query = Gtk::manage(new Gtk::Entry ()); // cannot use Gtk::manage here as FileCatalog::getFilter will fail on Query->get_text()
    Query->set_text("");
    Query->set_width_chars (20); // TODO !!! add this value to options?
    Query->set_max_width_chars (20);
    Query->set_tooltip_markup (M("FILEBROWSER_QUERYHINT"));
    Gtk::Box* hbQuery = Gtk::manage(new Gtk::Box ());
    buttonQueryClear = Gtk::manage(new Gtk::Button ());
    buttonQueryClear->set_image (*iQueryClear);
    buttonQueryClear->set_tooltip_markup (M("FILEBROWSER_QUERYBUTTONHINT"));
    buttonQueryClear->set_relief (Gtk::RELIEF_NONE);
    buttonQueryClear->signal_clicked().connect( sigc::mem_fun(*this, &FileCatalog::buttonQueryClearPressed) );
    hbQuery->pack_start (*labelQuery, Gtk::PACK_SHRINK, 0);
    hbQuery->pack_start (*Query, Gtk::PACK_SHRINK, 0);
    hbQuery->pack_start (*buttonQueryClear, Gtk::PACK_SHRINK, 0);
    hbToolBar1->pack_start (*hbQuery, Gtk::PACK_SHRINK, 0);

    Query->signal_activate().connect (sigc::mem_fun(*this, &FileCatalog::executeQuery)); //respond to the Enter key
    Query->signal_key_press_event().connect(sigc::mem_fun(*this, &FileCatalog::Query_key_pressed));

    const auto& options = App::get().options();

    // if NOT a single row toolbar
    if (!options.FileBrowserToolbarSingleRow) {
        hbToolBar1STB = Gtk::manage(new MyScrolledToolbar());
        hbToolBar1STB->set_name("FileBrowserQueryToolbar");
        hbToolBar1STB->add(*hbToolBar1);
        pack_start (*hbToolBar1STB, Gtk::PACK_SHRINK, 0);
    }

    // setup button bar
    buttonBar = Gtk::manage( new Gtk::Box () );
    buttonBar->set_name ("ToolBarPanelFileBrowser");
    MyScrolledToolbar *stb = Gtk::manage(new MyScrolledToolbar());
    stb->set_name("FileBrowserIconToolbar");
    stb->add(*buttonBar);
    pack_start (*stb, Gtk::PACK_SHRINK);

    tbLeftPanel_1 = new Gtk::ToggleButton ();
    iLeftPanel_1_Show = new RTImage("panel-to-right", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    iLeftPanel_1_Hide = new RTImage("panel-to-left", Gtk::ICON_SIZE_LARGE_TOOLBAR);

    tbLeftPanel_1->set_relief(Gtk::RELIEF_NONE);
    tbLeftPanel_1->set_active (true);
    tbLeftPanel_1->set_tooltip_markup (M("MAIN_TOOLTIP_SHOWHIDELP1"));
    tbLeftPanel_1->set_image (*iLeftPanel_1_Hide);
    tbLeftPanel_1->signal_toggled().connect( sigc::mem_fun(*this, &FileCatalog::tbLeftPanel_1_toggled) );
    buttonBar->pack_start (*tbLeftPanel_1, Gtk::PACK_SHRINK);

    vSepiLeftPanel = new Gtk::Separator(Gtk::ORIENTATION_VERTICAL);
    buttonBar->pack_start (*vSepiLeftPanel, Gtk::PACK_SHRINK);

    iFilterClear = new RTImage ("filter-clear", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    igFilterClear = new RTImage ("filter", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    bFilterClear = Gtk::manage(new Gtk::ToggleButton ());
    bFilterClear->set_active (true);
    bFilterClear->set_image(*iFilterClear);
    bFilterClear->set_relief (Gtk::RELIEF_NONE);
    bFilterClear->set_tooltip_markup (M("FILEBROWSER_SHOWDIRHINT"));
    bFilterClear->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);
    bCateg[0] = bFilterClear->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bFilterClear, true));
    buttonBar->pack_start (*bFilterClear, Gtk::PACK_SHRINK);
    buttonBar->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK);

    fltrVbox1 = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    fltrRankbox = Gtk::manage (new Gtk::Box());
    fltrRankbox->get_style_context()->add_class("smallbuttonbox");
    fltrLabelbox = Gtk::manage (new Gtk::Box());
    fltrLabelbox->get_style_context()->add_class("smallbuttonbox");

    iUnRanked = new RTImage ("star-gold-hollow-small", Gtk::ICON_SIZE_BUTTON);
    igUnRanked = new RTImage ("star-hollow-small", Gtk::ICON_SIZE_BUTTON);
    bUnRanked = Gtk::manage( new Gtk::ToggleButton () );
    bUnRanked->get_style_context()->add_class("smallbutton");
    bUnRanked->set_active (false);
    bUnRanked->set_image (*igUnRanked);
    bUnRanked->set_relief (Gtk::RELIEF_NONE);
    bUnRanked->set_tooltip_markup (M("FILEBROWSER_SHOWUNRANKHINT"));
    bCateg[1] = bUnRanked->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bUnRanked, true));
    fltrRankbox->pack_start (*bUnRanked, Gtk::PACK_SHRINK);
    bUnRanked->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);

    for (int i = 0; i < 5; i++) {
        iranked[i] = new RTImage ("star-gold-small", Gtk::ICON_SIZE_BUTTON);
        igranked[i] = new RTImage ("star-small", Gtk::ICON_SIZE_BUTTON);
        iranked[i]->show ();
        igranked[i]->show ();
        bRank[i] = Gtk::manage( new Gtk::ToggleButton () );
        bRank[i]->get_style_context()->add_class("smallbutton");
        bRank[i]->set_image (*igranked[i]);
        bRank[i]->set_relief (Gtk::RELIEF_NONE);
        fltrRankbox->pack_start (*bRank[i], Gtk::PACK_SHRINK);
        bCateg[i + 2] = bRank[i]->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bRank[i], true));
        bRank[i]->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);
    }

    // Toolbar
    // Similar image arrays in filebrowser.cc
    std::array<std::string, 6> clabelActiveIcons = {"circle-gray-small", "circle-red-small", "circle-yellow-small", "circle-green-small", "circle-blue-small", "circle-purple-small"};
    std::array<std::string, 6> clabelInactiveIcons = {"circle-empty-gray-small", "circle-empty-red-small", "circle-empty-yellow-small", "circle-empty-green-small", "circle-empty-blue-small", "circle-empty-purple-small"};

    iUnCLabeled = new RTImage(clabelActiveIcons[0], Gtk::ICON_SIZE_BUTTON);
    igUnCLabeled = new RTImage(clabelInactiveIcons[0], Gtk::ICON_SIZE_BUTTON);
    bUnCLabeled = Gtk::manage(new Gtk::ToggleButton());
    bUnCLabeled->get_style_context()->add_class("smallbutton");
    bUnCLabeled->set_active(false);
    bUnCLabeled->set_image(*igUnCLabeled);
    bUnCLabeled->set_relief(Gtk::RELIEF_NONE);
    bUnCLabeled->set_tooltip_markup(M("FILEBROWSER_SHOWUNCOLORHINT"));
    bCateg[7] = bUnCLabeled->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bUnCLabeled, true));
    fltrLabelbox->pack_start(*bUnCLabeled, Gtk::PACK_SHRINK);
    bUnCLabeled->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);

    for (int i = 0; i < 5; i++) {
        iCLabeled[i] = new RTImage(clabelActiveIcons[i+1], Gtk::ICON_SIZE_BUTTON);
        igCLabeled[i] = new RTImage(clabelInactiveIcons[i+1], Gtk::ICON_SIZE_BUTTON);
        iCLabeled[i]->show();
        igCLabeled[i]->show();
        bCLabel[i] = Gtk::manage(new Gtk::ToggleButton());
        bCLabel[i]->get_style_context()->add_class("smallbutton");
        bCLabel[i]->set_image(*igCLabeled[i]);
        bCLabel[i]->set_relief(Gtk::RELIEF_NONE);
        fltrLabelbox->pack_start(*bCLabel[i], Gtk::PACK_SHRINK);
        bCateg[i + 8] = bCLabel[i]->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bCLabel[i], true));
        bCLabel[i]->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);
    }

    fltrVbox1->pack_start (*fltrRankbox, Gtk::PACK_SHRINK, 0);
    fltrVbox1->pack_start (*fltrLabelbox, Gtk::PACK_SHRINK, 0);
    buttonBar->pack_start (*fltrVbox1, Gtk::PACK_SHRINK);

    bRank[0]->set_tooltip_markup (M("FILEBROWSER_SHOWRANK1HINT"));
    bRank[1]->set_tooltip_markup (M("FILEBROWSER_SHOWRANK2HINT"));
    bRank[2]->set_tooltip_markup (M("FILEBROWSER_SHOWRANK3HINT"));
    bRank[3]->set_tooltip_markup (M("FILEBROWSER_SHOWRANK4HINT"));
    bRank[4]->set_tooltip_markup (M("FILEBROWSER_SHOWRANK5HINT"));

    bCLabel[0]->set_tooltip_markup (M("FILEBROWSER_SHOWCOLORLABEL1HINT"));
    bCLabel[1]->set_tooltip_markup (M("FILEBROWSER_SHOWCOLORLABEL2HINT"));
    bCLabel[2]->set_tooltip_markup (M("FILEBROWSER_SHOWCOLORLABEL3HINT"));
    bCLabel[3]->set_tooltip_markup (M("FILEBROWSER_SHOWCOLORLABEL4HINT"));
    bCLabel[4]->set_tooltip_markup (M("FILEBROWSER_SHOWCOLORLABEL5HINT"));

    buttonBar->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK);

    fltrVbox2 = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    fltrEditedBox = Gtk::manage (new Gtk::Box());
    fltrEditedBox->get_style_context()->add_class("smallbuttonbox");
    fltrRecentlySavedBox = Gtk::manage (new Gtk::Box());
    fltrRecentlySavedBox->get_style_context()->add_class("smallbuttonbox");

    // bEdited
    // TODO The "g" variant was the more transparent variant of the icon, used
    // when the button was not toggled. Simplify this, change to ordinary
    // togglebutton, use CSS for opacity change.
    iEdited[0] = new RTImage ("tick-hollow-small", Gtk::ICON_SIZE_BUTTON);
    igEdited[0] = new RTImage ("tick-hollow-small", Gtk::ICON_SIZE_BUTTON);
    iEdited[1] = new RTImage ("tick-small", Gtk::ICON_SIZE_BUTTON);
    igEdited[1] = new RTImage ("tick-small", Gtk::ICON_SIZE_BUTTON);

    for (int i = 0; i < 2; i++) {
        iEdited[i]->show ();
        bEdited[i] = Gtk::manage(new Gtk::ToggleButton ());
        bEdited[i]->get_style_context()->add_class("smallbutton");
        bEdited[i]->set_active (false);
        bEdited[i]->set_image (*igEdited[i]);
        bEdited[i]->set_relief (Gtk::RELIEF_NONE);
        fltrEditedBox->pack_start (*bEdited[i], Gtk::PACK_SHRINK);
        //13, 14
        bCateg[i + 13] = bEdited[i]->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bEdited[i], true));
        bEdited[i]->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);
    }

    bEdited[0]->set_tooltip_markup (M("FILEBROWSER_SHOWEDITEDNOTHINT"));
    bEdited[1]->set_tooltip_markup (M("FILEBROWSER_SHOWEDITEDHINT"));

    // RecentlySaved
    // TODO The "g" variant was the more transparent variant of the icon, used
    // when the button was not toggled. Simplify this, change to ordinary
    // togglebutton, use CSS for opacity change.
    iRecentlySaved[0] = new RTImage ("saved-no-small", Gtk::ICON_SIZE_BUTTON);
    igRecentlySaved[0] = new RTImage ("saved-no-small", Gtk::ICON_SIZE_BUTTON);
    iRecentlySaved[1] = new RTImage ("saved-yes-small", Gtk::ICON_SIZE_BUTTON);
    igRecentlySaved[1] = new RTImage ("saved-yes-small", Gtk::ICON_SIZE_BUTTON);

    for (int i = 0; i < 2; i++) {
        iRecentlySaved[i]->show ();
        bRecentlySaved[i] = Gtk::manage(new Gtk::ToggleButton ());
        bRecentlySaved[i]->get_style_context()->add_class("smallbutton");
        bRecentlySaved[i]->set_active (false);
        bRecentlySaved[i]->set_image (*igRecentlySaved[i]);
        bRecentlySaved[i]->set_relief (Gtk::RELIEF_NONE);
        fltrRecentlySavedBox->pack_start (*bRecentlySaved[i], Gtk::PACK_SHRINK);
        //15, 16
        bCateg[i + 15] = bRecentlySaved[i]->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bRecentlySaved[i], true));
        bRecentlySaved[i]->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);
    }

    bRecentlySaved[0]->set_tooltip_markup (M("FILEBROWSER_SHOWRECENTLYSAVEDNOTHINT"));
    bRecentlySaved[1]->set_tooltip_markup (M("FILEBROWSER_SHOWRECENTLYSAVEDHINT"));

    fltrVbox2->pack_start (*fltrEditedBox, Gtk::PACK_SHRINK, 0);
    fltrVbox2->pack_start (*fltrRecentlySavedBox, Gtk::PACK_SHRINK, 0);
    buttonBar->pack_start (*fltrVbox2, Gtk::PACK_SHRINK);

    buttonBar->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK);

    // Trash
    iTrashShowEmpty = new RTImage("trash-empty-show", Gtk::ICON_SIZE_LARGE_TOOLBAR) ;
    iTrashShowFull  = new RTImage("trash-full-show", Gtk::ICON_SIZE_LARGE_TOOLBAR) ;

    bTrash = Gtk::manage( new Gtk::ToggleButton () );
    bTrash->set_image (*iTrashShowEmpty);
    bTrash->set_relief (Gtk::RELIEF_NONE);
    bTrash->set_tooltip_markup (M("FILEBROWSER_SHOWTRASHHINT"));
    bCateg[17] = bTrash->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bTrash, true));
    bTrash->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);

    iNotTrash = new RTImage("trash-hide-deleted", Gtk::ICON_SIZE_LARGE_TOOLBAR) ;
    iOriginal = new RTImage("filter-original", Gtk::ICON_SIZE_LARGE_TOOLBAR);

    bNotTrash = Gtk::manage( new Gtk::ToggleButton () );
    bNotTrash->set_image (*iNotTrash);
    bNotTrash->set_relief (Gtk::RELIEF_NONE);
    bNotTrash->set_tooltip_markup (M("FILEBROWSER_SHOWNOTTRASHHINT"));
    bCateg[18] = bNotTrash->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bNotTrash, true));
    bNotTrash->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);

    bOriginal = Gtk::manage( new Gtk::ToggleButton () );
    bOriginal->set_image (*iOriginal);
    bOriginal->set_tooltip_markup (M("FILEBROWSER_SHOWORIGINALHINT"));
    bOriginal->set_relief (Gtk::RELIEF_NONE);
    bCateg[19] = bOriginal->signal_toggled().connect (sigc::bind(sigc::mem_fun(*this, &FileCatalog::categoryButtonToggled), bOriginal, true));
    bOriginal->signal_button_press_event().connect (sigc::mem_fun(*this, &FileCatalog::capture_event), false);

    bRecursive = Gtk::manage(new Gtk::ToggleButton());
    bRecursive->set_image(*Gtk::manage(new RTImage("folder-subfolder", Gtk::ICON_SIZE_LARGE_TOOLBAR)));
    bRecursive->set_tooltip_text(M("FILEBROWSER_SHOWRECURSIVE"));
    bRecursive->set_relief(Gtk::RELIEF_NONE);
    bRecursive->set_active(options.browseRecursive);
    bRecursive->signal_toggled().connect(sigc::mem_fun(*this, &FileCatalog::showRecursiveToggled));

    buttonBar->pack_start (*bTrash, Gtk::PACK_SHRINK);
    buttonBar->pack_start (*bNotTrash, Gtk::PACK_SHRINK);
    buttonBar->pack_start (*bOriginal, Gtk::PACK_SHRINK);
    buttonBar->pack_start(*bRecursive, Gtk::PACK_SHRINK);
    buttonBar->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK);
    fileBrowser->trash_changed().connect( sigc::mem_fun(*this, &FileCatalog::trashChanged) );

    // 0  - bFilterClear
    // 1  - bUnRanked
    // 2  - bRank[0]
    // 3  - bRank[1]
    // 4  - bRank[2]
    // 5  - bRank[3]
    // 6  - bRank[4]
    // 7  - bUnCLabeled
    // 8  - bCLabel[0]
    // 9  - bCLabel[1]
    // 10 - bCLabel[2]
    // 11 - bCLabel[3]
    // 12 - bCLabel[4]
    // 13 - bEdited[0]
    // 14 - bEdited[1]
    // 15 - bRecentlySaved[0]
    // 16 - bRecentlySaved[1]
    // 17 - bTrash
    // 18 - bNotTrash
    // 19 - bOriginal

    categoryButtons[0] = bFilterClear;
    categoryButtons[1] = bUnRanked;

    for (int i = 0; i < 5; i++) {
        categoryButtons[i + 2] = bRank[i];
    }

    categoryButtons[7] = bUnCLabeled;

    for (int i = 0; i < 5; i++) {
        categoryButtons[i + 8] = bCLabel[i];
    }

    for (int i = 0; i < 2; i++) {
        categoryButtons[i + 13] = bEdited[i];
    }

    for (int i = 0; i < 2; i++) {
        categoryButtons[i + 15] = bRecentlySaved[i];
    }

    categoryButtons[17] = bTrash;
    categoryButtons[18] = bNotTrash;
    categoryButtons[19] = bOriginal;

    exifInfo = Gtk::manage(new Gtk::ToggleButton ());
    exifInfo->set_image (*Gtk::manage(new RTImage ("info", Gtk::ICON_SIZE_LARGE_TOOLBAR)));
    exifInfo->set_relief (Gtk::RELIEF_NONE);
    exifInfo->set_tooltip_markup (M("FILEBROWSER_SHOWEXIFINFO"));
    exifInfo->set_active( options.showFileNames );
    exifInfo->signal_toggled().connect(sigc::mem_fun(*this, &FileCatalog::exifInfoButtonToggled));
    buttonBar->pack_start (*exifInfo, Gtk::PACK_SHRINK);

    // thumbnail zoom
    Gtk::Box* zoomBox = Gtk::manage( new Gtk::Box () );
    zoomInButton  = Gtk::manage(  new Gtk::Button () );
    zoomInButton->set_image (*Gtk::manage(new RTImage ("magnifier-plus", Gtk::ICON_SIZE_LARGE_TOOLBAR)));
    zoomInButton->signal_pressed().connect (sigc::mem_fun(*this, &FileCatalog::zoomIn));
    zoomInButton->set_relief (Gtk::RELIEF_NONE);
    zoomInButton->set_tooltip_markup (M("FILEBROWSER_ZOOMINHINT"));
    zoomBox->pack_end (*zoomInButton, Gtk::PACK_SHRINK);
    zoomOutButton  = Gtk::manage( new Gtk::Button () );
    zoomOutButton->set_image (*Gtk::manage(new RTImage ("magnifier-minus", Gtk::ICON_SIZE_LARGE_TOOLBAR)));
    zoomOutButton->signal_pressed().connect (sigc::mem_fun(*this, &FileCatalog::zoomOut));
    zoomOutButton->set_relief (Gtk::RELIEF_NONE);
    zoomOutButton->set_tooltip_markup (M("FILEBROWSER_ZOOMOUTHINT"));
    zoomBox->pack_end (*zoomOutButton, Gtk::PACK_SHRINK);

    buttonBar->pack_start (*zoomBox, Gtk::PACK_SHRINK);
    buttonBar->pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK);

    // if it IS a single row toolbar
    if (options.FileBrowserToolbarSingleRow) {
        buttonBar->pack_start (*hbToolBar1, Gtk::PACK_EXPAND_WIDGET, 0);
    }

    tbRightPanel_1 = new Gtk::ToggleButton ();
    iRightPanel_1_Show = new RTImage("panel-to-left", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    iRightPanel_1_Hide = new RTImage("panel-to-right", Gtk::ICON_SIZE_LARGE_TOOLBAR);

    tbRightPanel_1->set_relief(Gtk::RELIEF_NONE);
    tbRightPanel_1->set_active (true);
    tbRightPanel_1->set_tooltip_markup (M("MAIN_TOOLTIP_SHOWHIDERP1"));
    tbRightPanel_1->set_image (*iRightPanel_1_Hide);
    tbRightPanel_1->signal_toggled().connect( sigc::mem_fun(*this, &FileCatalog::tbRightPanel_1_toggled) );
    buttonBar->pack_end (*tbRightPanel_1, Gtk::PACK_SHRINK);

    buttonBar->pack_end (*coarsePanel, Gtk::PACK_SHRINK);
    buttonBar->pack_end (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK, 4);
    buttonBar->pack_end (*toolBar, Gtk::PACK_SHRINK);
    buttonBar->pack_end (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), Gtk::PACK_SHRINK, 4);

    // add default panel
    hBox = Gtk::manage( new Gtk::Box () );
    hBox->show ();
    hBox->pack_end (*fileBrowser);
    hBox->set_name ("FilmstripPanel");
    fileBrowser->applyFilter (getFilter()); // warning: can call this only after all objects used in getFilter (e.g. Query) are instantiated
    //printf("FileCatalog::FileCatalog  fileBrowser->applyFilter (getFilter())\n");
    pack_start (*hBox);

    enabled = true;

    lastScrollPos = 0;

    for (int i = 0; i < 18; i++) {
        hScrollPos[i] = 0;
        vScrollPos[i] = 0;
    }
}

FileCatalog::~FileCatalog()
{
    idle_register.destroy();

    timerEventSource.disconnect();

    for (int i = 0; i < 5; i++) {
        delete iranked[i];
        delete igranked[i];
        delete iCLabeled[i];
        delete igCLabeled[i];
    }

    for (int i = 0; i < 2; i++) {
        delete iEdited[i];
        delete igEdited[i];
        delete iRecentlySaved[i];
        delete igRecentlySaved[i];
    }

    delete iFilterClear;
    delete igFilterClear;
    delete iUnRanked;
    delete igUnRanked;
    delete iUnCLabeled;
    delete igUnCLabeled;
    delete iTrashShowEmpty;
    delete iTrashShowFull;
    delete iNotTrash;
    delete iOriginal;
    delete iRefreshWhite;
    delete iRefreshRed;
    delete iQueryClear;
    delete iLeftPanel_1_Show;
    delete iLeftPanel_1_Hide;
    delete iRightPanel_1_Show;
    delete iRightPanel_1_Hide;
}

bool FileCatalog::capture_event(GdkEventButton* event)
{
    // need to record modifiers on the button press, because signal_toggled does not pass the event.
    modifierKey = event->state;
    return false;
}

void FileCatalog::exifInfoButtonToggled()
{
    auto& options = App::get().mut_options();
    if (inTabMode) {
        options.filmStripShowFileNames =  exifInfo->get_active();
    } else {
        options.showFileNames =  exifInfo->get_active();
    }

    fileBrowser->refreshThumbImages ();
    refreshHeight();
}

void FileCatalog::on_realize()
{

    Gtk::Box::on_realize();
    Pango::FontDescription fontd = get_style_context()->get_font();
    fileBrowser->get_pango_context()->set_font_description (fontd);
//    batchQueue->get_pango_context()->set_font_description (fontd);
}

void FileCatalog::closeDir ()
{

    if (filterPanel) {
        filterPanel->set_sensitive (false);
    }

    if (exportPanel) {
        exportPanel->set_sensitive (false);
    }

    dirMonitors.clear();

    // ignore old requests
    ++selectedDirectoryId;

    // terminate thumbnail preview loading
    previewLoader->removeAllJobs ();

    // terminate thumbnail updater
    thumbImageUpdater->removeAllJobs ();

    timerEventSource.disconnect();

    // remove entries
    selectedDirectory = "";
    fileBrowser->close ();
    fileNameList.clear ();
    pendingFiles.clear ();

    {
        MyMutex::MyLock lock(dirEFSMutex);
        dirEFS.clear ();
    }
    hasValidCurrentEFS = false;
    redrawAll ();
}

std::vector<Glib::ustring> FileCatalog::getFileList(std::vector<Glib::RefPtr<Gio::File>> *dirs_explored)
{
    std::vector<Glib::ustring> names;

    const auto& options = App::get().options();
    int dirs_left = options.browseRecursive ? options.browseRecursiveMaxDirs : 0;
    getFilesRecursively(selectedDirectory, options.browseRecursiveDepth, dirs_left, names, dirs_explored);

    return names;
}

std::vector<Glib::ustring> FileCatalog::getFileList(Glib::ustring root, int start_depth, std::vector<Glib::RefPtr<Gio::File>> *dirs_explored)
{
    std::vector<Glib::ustring> names;

    const auto& options = App::get().options();
    int dirs_left = options.browseRecursiveMaxDirs - static_cast<int>(dirMonitors.size());
    if (dirs_left >= 0) {
        getFilesRecursively(root, options.browseRecursiveDepth - start_depth, dirs_left, names, dirs_explored);
    }

    return names;
}

void FileCatalog::dirSelected (const Glib::ustring& dirname, const Glib::ustring& openfile)
{

    try {
        const Glib::RefPtr<Gio::File> dir = Gio::File::create_for_path(dirname);

        if (!dir) {
            return;
        }

        closeDir();
        previewsToLoad = 0;

        // if openfile exists, we have to open it first (it is a command line argument)
        if (!openfile.empty()) {
            addAndOpenFile (openfile);
        }

        selectedDirectory = dir->get_parse_name();

        std::vector<Glib::RefPtr<Gio::File>> allDirs;
        BrowsePath->set_text(selectedDirectory);
        buttonBrowsePath->set_image(*iRefreshWhite);
        std::vector<Glib::ustring> fileList = getFileList(&allDirs);

        for (unsigned int i = 0; i < fileList.size(); i++) {
            if (openfile.empty() || fileList[i] != openfile) { // if we opened a file at the beginning don't add it again
                addFile(fileList[i]);
            }
        }

        _refreshProgressBar ();

        if (previewsToLoad == 0) {
            filepanel->loadingThumbs(M("PROGRESSBAR_NOIMAGES"), 0);
        } else {
            filepanel->loadingThumbs(M("PROGRESSBAR_LOADINGTHUMBS"), 0);
        }

        if (App::get().options().newFileDelayTime > 0) {
            if (!timerEventSource.connected()) {
                timerEventSource = Glib::signal_timeout().connect(sigc::mem_fun(*this, &FileCatalog::timerEvents), 1000);
            }
            refreshDirectoryMonitors(allDirs);
        }
    } catch (Glib::Exception& ex) {
        std::cout << ex.what();
    }
}

void FileCatalog::refreshDirectoryMonitors(const std::vector<Glib::RefPtr<Gio::File>> &dirs_to_monitor, bool compound_update)
{
    if (!compound_update) {
        // Remove monitors on directories that are no longer shown.

        std::vector<Glib::ustring> updated_dir_names;
        std::transform(
            dirs_to_monitor.cbegin(), dirs_to_monitor.cend(),
            std::back_inserter(updated_dir_names),
            [](const Glib::RefPtr<Gio::File> &updated_dir) { return updated_dir->get_path(); });
        
        dirMonitors.erase(
            std::remove_if(dirMonitors.begin(), dirMonitors.end(),
                [&updated_dir_names](const FileMonitorInfo &fileMonitorInfo) {
                    return std::find(updated_dir_names.cbegin(), updated_dir_names.cend(), fileMonitorInfo.filePath) == updated_dir_names.cend();
                }),
            dirMonitors.end());
    }

    // Add monitors that do not exist yet.
    std::vector<Glib::ustring> monitored_dir_names;
    std::transform(
        dirMonitors.cbegin(), dirMonitors.cend(),
        std::back_inserter(monitored_dir_names),
        [](const FileMonitorInfo &dir_monitor) { return dir_monitor.filePath; });
    for (const auto &dir_to_monitor : dirs_to_monitor) {
        const auto dir_path = dir_to_monitor->get_path();
        if (std::find(monitored_dir_names.cbegin(), monitored_dir_names.cend(), dir_path) != monitored_dir_names.cend()) {
            continue; // A monitor exists already.
        }
        auto dir_monitor = dir_to_monitor->monitor_directory();
        dir_monitor->signal_changed().connect(sigc::mem_fun(*this, &FileCatalog::on_dir_changed));
        dirMonitors.emplace_back(dir_monitor, dir_path);
    }
}

void FileCatalog::enableTabMode(bool enable)
{
    inTabMode = enable;

    const auto& options = App::get().options();
    if (enable) {
        if (options.showFilmStripToolBar) {
            showToolBar();
        } else {
            hideToolBar();
        }

        exifInfo->set_active( options.filmStripShowFileNames );

    } else {
        buttonBar->show();
        hbToolBar1->show();
        if (hbToolBar1STB) {
            hbToolBar1STB->show();
        }
        exifInfo->set_active( options.showFileNames );
    }

    fileBrowser->enableTabMode(inTabMode);

    redrawAll();
}

void FileCatalog::_refreshProgressBar ()
{
    // This progress bar only measures the FIRST pass (quick thumbnails)
    // The second, usually longer pass is NOT measured by this
    const auto previewsLoaded = fileBrowser->getEntries().size();

    if (!progressImage || !progressLabel) {
        // create tab label once
        Gtk::Notebook *nb = (Gtk::Notebook *)(filepanel->get_parent());
        Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
        setExpandAlignProperties (grid, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
        progressImage = Gtk::manage(new RTImage("folder-closed", Gtk::ICON_SIZE_LARGE_TOOLBAR));
        progressLabel = Gtk::manage(new Gtk::Label(M("MAIN_FRAME_FILEBROWSER")));

        const auto& options = App::get().options();
        grid->attach_next_to(*progressImage, options.mainNBVertical ? Gtk::POS_TOP : Gtk::POS_RIGHT, 1, 1);
        grid->attach_next_to(*progressLabel, options.mainNBVertical ? Gtk::POS_TOP : Gtk::POS_RIGHT, 1, 1);
        grid->set_tooltip_markup(M("MAIN_FRAME_FILEBROWSER_TOOLTIP"));
        grid->show_all();
        if (options.mainNBVertical) {
            progressLabel->set_angle(90);
        }
        if (nb) {
            nb->set_tab_label(*filepanel, *grid);
        }
    }
    if (!previewsToLoad) {
        progressImage->set_from_icon_name("folder-closed", Gtk::ICON_SIZE_LARGE_TOOLBAR);
        const auto filteredCount = min(fileBrowser->getNumFiltered(), previewsLoaded);
        progressLabel->set_text(M("MAIN_FRAME_FILEBROWSER") +
                                (filteredCount != previewsLoaded ? " [" + Glib::ustring::format(filteredCount) + "/" : " [")
                                + Glib::ustring::format(previewsLoaded) +
                                (filteredCount != previewsLoaded ? "]" : "]"));
    } else {
        progressImage->set_from_icon_name("magnifier", Gtk::ICON_SIZE_LARGE_TOOLBAR);
        progressLabel->set_text(M("MAIN_FRAME_FILEBROWSER") + " ["
                                + Glib::ustring::format(previewsLoaded) + " ("
                                + Glib::ustring::format(previewsToLoad) + ")]" );
        filepanel->loadingThumbs("", (double)previewsLoaded / (previewsLoaded + previewsToLoad));
    }
}

void FileCatalog::previewFailed(int dir_id, Glib::ustring file, FailReason reason)
{
    idle_register.add(
        [this, dir_id, file, reason]() -> bool
        {
            if ( dir_id != selectedDirectoryId ) {
                return false;
            }

            fileNameList.erase(std::remove(fileNameList.begin(), fileNameList.end(), file), fileNameList.end());
            previewsToLoad--;

            _refreshProgressBar();
            return false;
        },
        G_PRIORITY_DEFAULT_IDLE
    );
}

void FileCatalog::previewReady (int dir_id, FileBrowserEntry* fdn)
{
    idle_register.add(
        [this, dir_id, fdn]() -> bool
        {
            if ( dir_id != selectedDirectoryId ) {
                delete fdn;
                return false;
            }

            // put it into the "full directory" browser
            fdn->setImageAreaToolListener (iatlistener);
            fileBrowser->addEntry_ (fdn);

            // update exif filter settings (minimal & maximal values of exif tags, cameras, lenses, etc...)
            const CacheImageData* cfs = fdn->thumbnail->getCacheImageData();

            {
                MyMutex::MyLock lock(dirEFSMutex);

                if (cfs->exifValid) {
                    if (cfs->fnumber < dirEFS.fnumberFrom) {
                        dirEFS.fnumberFrom = cfs->fnumber;
                    }

                    if (cfs->fnumber > dirEFS.fnumberTo) {
                        dirEFS.fnumberTo = cfs->fnumber;
                    }

                    if (cfs->shutter < dirEFS.shutterFrom) {
                        dirEFS.shutterFrom = cfs->shutter;
                    }

                    if (cfs->shutter > dirEFS.shutterTo) {
                        dirEFS.shutterTo = cfs->shutter;
                    }

                    if (cfs->iso > 0 && cfs->iso < dirEFS.isoFrom) {
                        dirEFS.isoFrom = cfs->iso;
                    }

                    if (cfs->iso > 0 && cfs->iso > dirEFS.isoTo) {
                        dirEFS.isoTo = cfs->iso;
                    }

                    if (cfs->focalLen < dirEFS.focalFrom) {
                        dirEFS.focalFrom = cfs->focalLen;
                    }

                    if (cfs->focalLen > dirEFS.focalTo) {
                        dirEFS.focalTo = cfs->focalLen;
                    }

                    //TODO: ass filters for HDR and PixelShift files
                }

                dirEFS.filetypes.insert (cfs->filetype);
                dirEFS.cameras.insert (cfs->getCamera());
                dirEFS.lenses.insert (cfs->lens);
                dirEFS.expcomp.insert (cfs->expcomp);
            }

            previewsToLoad--;

            _refreshProgressBar();
            return false;
        },
        G_PRIORITY_DEFAULT_IDLE
    );
}

// Called within GTK UI thread
void FileCatalog::previewsFinishedUI(int dir_id)
{
    if ( dir_id != selectedDirectoryId ) {
        return;
    }

    redrawAll();

    if (filterPanel) {
        filterPanel->set_sensitive(true);
        if (!hasValidCurrentEFS) {
            MyMutex::MyLock lock(dirEFSMutex);
            filterPanel->setFilter(dirEFS, true);
        } else {
            filterPanel->setFilter(currentEFS, false);
        }
    }

    if (exportPanel) {
        exportPanel->set_sensitive(true);
    }

    // restart anything that might have been loaded low quality
    fileBrowser->refreshQuickThumbImages();
    fileBrowser->applyFilter(getFilter());  // refresh total image count
    _refreshProgressBar();

    filepanel->loadingThumbs(M("PROGRESSBAR_READY"), 0);

    if (!imageToSelect_fname.empty()) {
        fileBrowser->selectImage(imageToSelect_fname);
        imageToSelect_fname = "";
    }

    if (!refImageForOpen_fname.empty() && actionNextPrevious != NAV_NONE) {
        fileBrowser->openNextPreviousEditorImage(refImageForOpen_fname, actionNextPrevious);
        refImageForOpen_fname = "";
        actionNextPrevious = NAV_NONE;
    }

    // newly added item might have been already trashed in a previous session
    trashChanged();
}

void FileCatalog::previewsFinished (int dir_id)
{
    idle_register.add(
        [this, dir_id]() -> bool
        {
            previewsFinishedUI(dir_id);
            return false;
        },
        // keep priority lower than on the other interface functions to make sure callbacks will not be executed out of order
        G_PRIORITY_DEFAULT_IDLE + 1
    );
}

void FileCatalog::setEnabled (bool e)
{
    enabled = e;
}

void FileCatalog::redrawAll ()
{
    fileBrowser->queue_draw ();
}

void FileCatalog::refreshThumbImages ()
{
    fileBrowser->refreshThumbImages ();
}

void FileCatalog::refreshHeight ()
{
    int newHeight = fileBrowser->getEffectiveHeight();

    if (newHeight < 5) {  // This may occur if there's no thumbnail.
        int w, h;
        get_size_request(w, h);
        newHeight = h;
    }

    if (hbToolBar1STB && hbToolBar1STB->is_visible()) {
        newHeight += hbToolBar1STB->get_height();
    }

    if (buttonBar->is_visible()) {
        newHeight += buttonBar->get_height();
    }

    set_size_request(0, newHeight + 2); // HOMBRE: yeah, +2, there's always 2 pixels missing... sorry for this dirty hack O:)
}

void FileCatalog::_openImage(const std::vector<Thumbnail*>& tmb)
{
    if (enabled && listener) {
        for (size_t i = 0; i < tmb.size(); i++) {
            // fileSelected does not complete with a fully loaded image, but it does do some preliminary checks
            if (!listener->fileSelected(tmb[i])) {
                tmb[i]->decreaseRef();
            } else if (!App::get().options().tabbedUI) {
                // allow only one image in single editor mode
                for (++i; i < tmb.size(); i++) {
                    tmb[i]->decreaseRef();
                }
                break;
            }
        }
    }
}

void FileCatalog::filterApplied()
{
    idle_register.add(
        [this]() -> bool
        {
            _refreshProgressBar();
            return false;
        }
    );
}

void FileCatalog::openRequested(const std::vector<Thumbnail*>& tmb)
{
    for (const auto thumb : tmb) {
        thumb->increaseRef();
    }

    idle_register.add(
        [this, tmb]() -> bool
        {
            _openImage(tmb);
            return false;
        }
    );
}

void FileCatalog::deleteRequested(const std::vector<FileBrowserEntry*>& tbe, bool inclBatchProcessed, bool onlySelected)
{
    if (tbe.empty()) {
        return;
    }

    Gtk::MessageDialog msd (getToplevelWindow(this), M("FILEBROWSER_DELETEDIALOG_HEADER"), true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
    if (onlySelected) {
        msd.set_secondary_text(Glib::ustring::compose (inclBatchProcessed ? M("FILEBROWSER_DELETEDIALOG_SELECTEDINCLPROC") : M("FILEBROWSER_DELETEDIALOG_SELECTED"), tbe.size()), true);
    } else {
        msd.set_secondary_text(Glib::ustring::compose (M("FILEBROWSER_DELETEDIALOG_ALL"), tbe.size()), true);
    }

    if (msd.run() == Gtk::RESPONSE_YES) {
        for (unsigned int i = 0; i < tbe.size(); i++) {
            const auto fname = tbe[i]->filename;

            // clear cache, remove from filebrowser and delete from filesystem
            cacheMgr->clearFromCache(fname, true);
            delete fileBrowser->delEntry(fname);
            ::g_remove (fname.c_str ());
            // delete paramfile if found
            ::g_remove ((fname + App::PARAM_FILE_EXTENSION).c_str ());
            ::g_remove ((removeExtension(fname) + App::PARAM_FILE_EXTENSION).c_str ());
            // delete .thm file
            ::g_remove ((removeExtension(fname) + ".thm").c_str ());
            ::g_remove ((removeExtension(fname) + ".THM").c_str ());

            if (inclBatchProcessed) {
                const auto& options = App::get().options();
                Glib::ustring procfName = Glib::ustring::compose ("%1.%2", BatchQueue::calcAutoFileNameBase(fname), options.saveFormatBatch.format);
                ::g_remove (procfName.c_str ());

                Glib::ustring procfNameParamFile = Glib::ustring::compose ("%1.%2.out%3", BatchQueue::calcAutoFileNameBase(fname), options.saveFormatBatch.format, App::PARAM_FILE_EXTENSION);
                ::g_remove (procfNameParamFile.c_str ());
            }
        }

        _refreshProgressBar();
        redrawAll ();
    }
}

void FileCatalog::copyMoveRequested(const std::vector<FileBrowserEntry*>& tbe, bool moveRequested)
{
    if (tbe.empty()) {
        return;
    }

    Glib::ustring fc_title;

    if (moveRequested) {
        fc_title = M("FILEBROWSER_POPUPMOVETO");
    } else {
        fc_title = M("FILEBROWSER_POPUPCOPYTO");
    }

    Gtk::FileChooserDialog fc (getToplevelWindow (this), fc_title, Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER );
    fc.add_button( M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
    fc.add_button( M("GENERAL_OK"), Gtk::RESPONSE_OK);
    auto& options = App::get().mut_options();
    if (!options.lastCopyMovePath.empty() && Glib::file_test(options.lastCopyMovePath, Glib::FILE_TEST_IS_DIR)) {
        fc.set_current_folder(options.lastCopyMovePath);
    } else {
        // open dialog at the 1-st file's path
        fc.set_current_folder(Glib::path_get_dirname(tbe[0]->filename));
    }
    //!!! TODO prevent dialog closing on "enter" key press

    if( fc.run() == Gtk::RESPONSE_OK ) {
        options.lastCopyMovePath = fc.get_current_folder();

        // iterate through selected files
        for (unsigned int i = 0; i < tbe.size(); i++) {
            Glib::ustring src_fPath = tbe[i]->filename;
            Glib::ustring src_Dir = Glib::path_get_dirname(src_fPath);
            Glib::RefPtr<Gio::File> src_file = Gio::File::create_for_path ( src_fPath );

            if( !src_file ) {
                continue;    // if file is missing - skip it
            }

            Glib::ustring fname = src_file->get_basename();
            Glib::ustring fname_noExt = removeExtension(fname);
            Glib::ustring fname_Ext = getExtension(fname);

            // construct  destination File Paths
            Glib::ustring dest_fPath = Glib::build_filename (options.lastCopyMovePath, fname);
            Glib::ustring dest_fPath_param = dest_fPath + App::PARAM_FILE_EXTENSION;

            if (moveRequested && (src_Dir == options.lastCopyMovePath)) {
                continue;
            }

            /* comparison of src_Dir and dest_Dir is done per image for compatibility with
            possible future use of Collections as source where each file's source path may be different.*/

            bool filecopymovecomplete = false;
            int i_copyindex = 1;

            while(!filecopymovecomplete) {
                // check for filename conflicts at destination - prevent overwriting (actually RT will crash on overwriting attempt)
                if (!Glib::file_test(dest_fPath, Glib::FILE_TEST_EXISTS) && !Glib::file_test(dest_fPath_param, Glib::FILE_TEST_EXISTS)) {
                    // copy/move file to destination
                    Glib::RefPtr<Gio::File> dest_file = Gio::File::create_for_path ( dest_fPath );

                    if (moveRequested) {
                        // move file
                        src_file->move(dest_file);
                        // re-attach cache files
                        cacheMgr->renameEntry (src_fPath, tbe[i]->thumbnail->getMD5(), dest_fPath);
                        // remove from browser
                        fileBrowser->delEntry (src_fPath);
                    } else {
                        src_file->copy(dest_file);
                    }


                    // attempt to copy/move paramFile only if it exist next to the src
                    Glib::RefPtr<Gio::File> scr_param = Gio::File::create_for_path (  src_fPath + App::PARAM_FILE_EXTENSION );

                    if (Glib::file_test( src_fPath + App::PARAM_FILE_EXTENSION, Glib::FILE_TEST_EXISTS)) {
                        Glib::RefPtr<Gio::File> dest_param = Gio::File::create_for_path ( dest_fPath_param);

                        // copy/move paramFile to destination
                        if (moveRequested) {
                            if (Glib::file_test( dest_fPath + App::PARAM_FILE_EXTENSION, Glib::FILE_TEST_EXISTS)) {
                                // profile already got copied to destination from cache after cacheMgr->renameEntry
                                // delete source profile as cleanup
                                ::g_remove ((src_fPath + App::PARAM_FILE_EXTENSION).c_str ());
                            } else {
                                scr_param->move(dest_param);
                            }
                        } else {
                            scr_param->copy(dest_param);
                        }
                    }

                    filecopymovecomplete = true;
                } else {
                    // adjust destination fname to avoid conflicts (append "_<index>", preserve extension)
                    Glib::ustring dest_fname = Glib::ustring::compose("%1%2%3%4%5", fname_noExt, "_", i_copyindex, ".", fname_Ext);
                    // re-construct  destination File Paths
                    dest_fPath = Glib::build_filename (options.lastCopyMovePath, dest_fname);
                    dest_fPath_param = dest_fPath + App::PARAM_FILE_EXTENSION;
                    i_copyindex++;
                }
            }//while
        } // i<tbe.size() loop

        redrawAll ();

        _refreshProgressBar();
    } // Gtk::RESPONSE_OK
}

void FileCatalog::developRequested(const std::vector<FileBrowserEntry*>& tbe, bool fastmode)
{
    if (listener) {
        std::vector<BatchQueueEntry*> entries;

        // TODO: (HOMBRE) should we still use parallelization here, now that thumbnails are processed asynchronously...?
        //#pragma omp parallel for ordered
        for (size_t i = 0; i < tbe.size(); i++) {
            FileBrowserEntry* fbe = tbe[i];
            Thumbnail* th = fbe->thumbnail;
            rtengine::procparams::ProcParams params = th->getProcParams();

            const auto& options = App::get().options();

            // if fast mode is selected, override (disable) params
            // controlling time and resource consuming tasks
            // and also those which effect is not pronounced after reducing the image size
            // TODO!!! could expose selections below via preferences
            if (fastmode) {
                if (!options.fastexport_use_fast_pipeline) {
                    if (options.fastexport_bypass_sharpening) {
                        params.sharpening.enabled = false;
                    }

                    if (options.fastexport_bypass_sharpenEdge) {
                        params.sharpenEdge.enabled = false;
                    }

                    if (options.fastexport_bypass_sharpenMicro) {
                        params.sharpenMicro.enabled = false;
                    }

                    //if (options.fastexport_bypass_lumaDenoise) params.lumaDenoise.enabled = false;
                    //if (options.fastexport_bypass_colorDenoise) params.colorDenoise.enabled = false;
                    if (options.fastexport_bypass_defringe) {
                        params.defringe.enabled = false;
                    }

                    if (options.fastexport_bypass_dirpyrDenoise) {
                        params.dirpyrDenoise.enabled = false;
                    }

                    if (options.fastexport_bypass_dirpyrequalizer) {
                        params.dirpyrequalizer.enabled = false;
                    }

                    if (options.fastexport_bypass_wavelet) {
                        params.wavelet.enabled = false;
                    }

                    //if (options.fastexport_bypass_raw_bayer_all_enhance) params.raw.bayersensor.all_enhance = false;
                    if (options.fastexport_bypass_raw_bayer_dcb_iterations) {
                        params.raw.bayersensor.dcb_iterations = 0;
                    }

                    if (options.fastexport_bypass_raw_bayer_dcb_enhance) {
                        params.raw.bayersensor.dcb_enhance = false;
                    }

                    if (options.fastexport_bypass_raw_bayer_lmmse_iterations) {
                        params.raw.bayersensor.lmmse_iterations = 0;
                    }

                    if (options.fastexport_bypass_raw_bayer_linenoise) {
                        params.raw.bayersensor.linenoise = 0;
                    }

                    if (options.fastexport_bypass_raw_bayer_greenthresh) {
                        params.raw.bayersensor.greenthresh = 0;
                    }

                    if (options.fastexport_bypass_raw_ccSteps) {
                        params.raw.bayersensor.ccSteps = params.raw.xtranssensor.ccSteps = 0;
                    }

                    if (options.fastexport_bypass_raw_ca) {
                        params.raw.ca_autocorrect = false;
                        params.raw.cared = 0;
                        params.raw.cablue = 0;
                    }

                    if (options.fastexport_bypass_raw_df) {
                        params.raw.df_autoselect = false;
                        params.raw.dark_frame = "";
                    }

                    if (options.fastexport_bypass_raw_ff) {
                        params.raw.ff_AutoSelect = false;
                        params.raw.ff_file = "";
                    }

                    params.raw.bayersensor.method = options.fastexport_raw_bayer_method;
                    params.raw.xtranssensor.method = options.fastexport_raw_xtrans_method;
                    params.icm.inputProfile = options.fastexport_icm_input_profile;
                    params.icm.workingProfile = options.fastexport_icm_working_profile;
                    params.icm.outputProfile = options.fastexport_icm_output_profile;
                    params.icm.outputIntent = rtengine::RenderingIntent(options.fastexport_icm_outputIntent);
                    params.icm.outputBPC = options.fastexport_icm_outputBPC;
                }

                if (params.resize.enabled) {
                    params.resize.width = rtengine::min(params.resize.width, options.fastexport_resize_width);
                    params.resize.height = rtengine::min(params.resize.height, options.fastexport_resize_height);
                    params.resize.longedge = rtengine::min(params.resize.longedge, options.fastexport_resize_longedge);
                    params.resize.shortedge = rtengine::min(params.resize.shortedge, options.fastexport_resize_shortedge);
                } else {
                    params.resize.width = options.fastexport_resize_width;
                    params.resize.height = options.fastexport_resize_height;
                    params.resize.longedge = options.fastexport_resize_longedge;
                    params.resize.shortedge = options.fastexport_resize_shortedge;
                }

                params.resize.enabled = options.fastexport_resize_enabled;
                params.resize.scale = options.fastexport_resize_scale;
                params.resize.appliesTo = options.fastexport_resize_appliesTo;
                params.resize.method = options.fastexport_resize_method;
                params.resize.dataspec = options.fastexport_resize_dataspec;
                params.resize.allowUpscaling = false;
            }

            rtengine::ProcessingJob* pjob = rtengine::ProcessingJob::create (fbe->filename, th->getType() == FT_Raw, params, fastmode && options.fastexport_use_fast_pipeline);

            int pw;
            int ph = BatchQueue::calcMaxThumbnailHeight();
            th->getThumbnailSize (pw, ph);

            // processThumbImage is the processing intensive part, but adding to queue must be ordered
            //#pragma omp ordered
            //{
            BatchQueueEntry* bqh = new BatchQueueEntry (pjob, params, fbe->filename, pw, ph, th, options.overwriteOutputFile);
            entries.push_back(bqh);
            //}
        }

        listener->addBatchQueueJobs( entries );
    }
}

void FileCatalog::renameRequested(const std::vector<FileBrowserEntry*>& tbe)
{
    RenameDialog* renameDlg = new RenameDialog ((Gtk::Window*)get_toplevel());

    for (size_t i = 0; i < tbe.size(); i++) {
        renameDlg->initName (Glib::path_get_basename (tbe[i]->filename), tbe[i]->thumbnail->getCacheImageData());

        Glib::ustring ofname = tbe[i]->filename;
        Glib::ustring dirName = Glib::path_get_dirname (tbe[i]->filename);
        Glib::ustring baseName = Glib::path_get_basename (tbe[i]->filename);

        bool success = false;

        do {
            if (renameDlg->run () == Gtk::RESPONSE_OK) {
                Glib::ustring nBaseName = renameDlg->getNewName ();

                // if path has directory components, exit
                if (Glib::path_get_dirname (nBaseName) != ".") {
                    continue;
                }

                // if no extension is given, concatenate the extension of the original file
                Glib::ustring ext = getExtension (nBaseName);

                if (ext.empty()) {
                    nBaseName += "." + getExtension (baseName);
                }

                Glib::ustring nfname = Glib::build_filename (dirName, nBaseName);

                /* check if filename already exists*/
                if (Glib::file_test (nfname, Glib::FILE_TEST_EXISTS)) {
                    Glib::ustring msg_ = Glib::ustring("<b>") + escapeHtmlChars(nfname) + ": " + M("MAIN_MSG_ALREADYEXISTS") + "</b>";
                    Gtk::MessageDialog msgd (msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                    msgd.run ();
                } else {
                    success = true;

                    if (::g_rename (ofname.c_str (), nfname.c_str ()) == 0) {
                        cacheMgr->renameEntry (ofname, tbe[i]->thumbnail->getMD5(), nfname);
                        ::g_remove((ofname + App::PARAM_FILE_EXTENSION).c_str ());
                        if (!timerEventSource.connected()) {
                            delete fileBrowser->delEntry(ofname);
                        }
                        // skip ahead and just add the file to bypass directory monitoring timeout
                        addFile(nfname);
                    }
                }
            } else {
                success = true;
            }
        } while (!success);

        renameDlg->hide ();
    }

    delete renameDlg;
}

void FileCatalog::selectionChanged(const std::vector<Thumbnail*>& tbe)
{
    if (fslistener) {
        fslistener->selectionChanged (tbe);
    }
}

void FileCatalog::clearFromCacheRequested(const std::vector<FileBrowserEntry*>& tbe, bool leavenotrace)
{
    if (tbe.empty()) {
        return;
    }

    for (unsigned int i = 0; i < tbe.size(); i++) {
        Glib::ustring fname = tbe[i]->filename;
        // remove from cache
        cacheMgr->clearFromCache (fname, leavenotrace);
    }
}

bool FileCatalog::isInTabMode() const
{
    return inTabMode;
}

void FileCatalog::categoryButtonToggled (Gtk::ToggleButton* b, bool isMouseClick)
{

    //was control key pressed (ignored if was not mouse click)
    bool control_down = modifierKey & GDK_CONTROL_MASK && isMouseClick;

    //was shift key pressed (ignored if was not mouse click)
    bool shift_down   = modifierKey & GDK_SHIFT_MASK && isMouseClick;

    // The event is process here, we can clear modifierKey now, it'll be set again on the next even
    modifierKey = 0;

    const int numCateg = sizeof(bCateg) / sizeof(bCateg[0]);
    const int numButtons = sizeof(categoryButtons) / sizeof(categoryButtons[0]);

    for (int i = 0; i < numCateg; i++) {
        bCateg[i].block (true);
    }

    // button already toggled when entering this function from a mouse click, so
    // we switch it back to its initial state.
    if (isMouseClick) {
        b->set_active(!b->get_active());
    }

    //if both control and shift keys were pressed, do nothing
    if (!(control_down && shift_down)) {

        fileBrowser->getScrollPosition (hScrollPos[lastScrollPos], vScrollPos[lastScrollPos]);

        //we look how many stars are already toggled on, if any
        int toggled_stars_count = 0, buttons = 0, start_star = 0, toggled_button = 0;

        for (int i = 0; i < numButtons; i++) {
            if (categoryButtons[i]->get_active()) {
                if (i > 0 && i < 17) {
                    toggled_stars_count ++;
                    start_star = i;
                }

                buttons |= (1 << i);
            }

            if (categoryButtons[i] == b) {
                toggled_button = i;
            }
        }

        // if no modifier key is pressed,
        if (!(control_down || shift_down)) {
            // if we're deselecting non-trashed or original
            if (toggled_button >= 18 && toggled_button <= 19 && (buttons & (1 << toggled_button))) {
                categoryButtons[0]->set_active (true);

                for (int i = 1; i < numButtons; i++) {
                    categoryButtons[i]->set_active (false);
                }
            }
            // if we're deselecting the only star still active
            else if (toggled_stars_count == 1 && (buttons & (1 << toggled_button))) {
                // activate clear-filters
                categoryButtons[0]->set_active (true);
                // deactivate the toggled filter
                categoryButtons[toggled_button]->set_active (false);
            }
            // if we're deselecting trash
            else if (toggled_button == 17 && (buttons & (1 << toggled_button))) {
                categoryButtons[0]->set_active (true);
                categoryButtons[17]->set_active (false);
            } else {
                // activate the toggled filter, deactivate the rest
                for (int i = 0; i < numButtons; i++) {
                    categoryButtons[i]->set_active (i == toggled_button);
                }
            }
        }
        //modifier key allowed only for stars and color labels...
        else if (toggled_button > 0 && toggled_button < 17) {
            if (control_down) {
                //control is pressed
                if (toggled_stars_count == 1 && (buttons & (1 << toggled_button))) {
                    //we're deselecting the only star still active, so we activate clear-filters
                    categoryButtons[0]->set_active(true);
                    //and we deselect the toggled star
                    categoryButtons[toggled_button]->set_active (false);
                } else if (toggled_stars_count >= 1) {
                    //we toggle the state of a star (eventually another one than the only one selected)
                    categoryButtons[toggled_button]->set_active(!categoryButtons[toggled_button]->get_active());
                } else {
                    //no star selected
                    //we deselect the 2 non star filters
                    if (buttons &  1    ) {
                        categoryButtons[0]->set_active(false);
                    }

                    if (buttons & (1 << 17)) {
                        categoryButtons[17]->set_active(false);
                    }

                    //and we toggle on the star
                    categoryButtons[toggled_button]->set_active (true);
                }
            } else {
                //shift is pressed, only allowed if 0 or 1 star & labels is selected
                if (!toggled_stars_count) {
                    //we deselect the 2 non star filters
                    if (buttons &  1      ) {
                        categoryButtons[0]->set_active(false);
                    }

                    if (buttons & (1 << 7)) {
                        categoryButtons[7]->set_active(false);
                    }

                    if (buttons & (1 << 13)) {
                        categoryButtons[13]->set_active(false);
                    }

                    if (buttons & (1 << 17)) {
                        categoryButtons[17]->set_active(false);
                    }

                    //and we set the start star to 1 (unrated images)
                    start_star = 1;
                    //we act as if one star were selected
                    toggled_stars_count = 1;
                }

                if (toggled_stars_count == 1) {
                    int current_star = min(start_star, toggled_button);
                    int last_star   = max(start_star, toggled_button);

                    //we permute the start and the end star for the next loop
                    for (; current_star <= last_star; current_star++) {
                        //we toggle on all the star in the range
                        if (!(buttons & (1 << current_star))) {
                            categoryButtons[current_star]->set_active(true);
                        }
                    }
                }

                //if more than one star & color label is selected, do nothing
            }
        }
        // ...or non-trashed or original with Control modifier
        else if (toggled_button >= 18 && toggled_button <= 19 && control_down) {
            Gtk::ToggleButton* categoryButton = categoryButtons[toggled_button];
            categoryButton->set_active (!categoryButton->get_active ());

            // If it was the first or last one, we reset the clear filter.
            if (buttons == 1 || buttons == (1 << toggled_button)) {
                bFilterClear->set_active (!categoryButton->get_active ());
            }
        }

        bool active_now, active_before;

        // FilterClear: set the right images
        // TODO: swapping FilterClear icon needs more work in categoryButtonToggled
        /*active_now = bFilterClear->get_active();
        active_before = buttons & (1 << (0)); // 0
        if      ( active_now && !active_before) bFilterClear->set_image (*iFilterClear);
        else if (!active_now &&  active_before) bFilterClear->set_image (*igFilterClear);*/

        // rank: set the right images
        for (int i = 0; i < 5; i++) {
            active_now = bRank[i]->get_active();
            active_before = buttons & (1 << (i + 2)); // 2,3,4,5,6

            if      ( active_now && !active_before) {
                bRank[i]->set_image (*iranked[i]);
            } else if (!active_now &&  active_before) {
                bRank[i]->set_image (*igranked[i]);
            }
        }

        active_now = bUnRanked->get_active();
        active_before = buttons & (1 << (1)); // 1

        if      ( active_now && !active_before) {
            bUnRanked->set_image (*iUnRanked);
        } else if (!active_now &&  active_before) {
            bUnRanked->set_image (*igUnRanked);
        }

        // color labels: set the right images
        for (int i = 0; i < 5; i++) {
            active_now = bCLabel[i]->get_active();
            active_before = buttons & (1 << (i + 8)); // 8,9,10,11,12

            if      ( active_now && !active_before) {
                bCLabel[i]->set_image (*iCLabeled[i]);
            } else if (!active_now &&  active_before) {
                bCLabel[i]->set_image (*igCLabeled[i]);
            }
        }

        active_now = bUnCLabeled->get_active();
        active_before = buttons & (1 << (7)); // 7

        if      ( active_now && !active_before) {
            bUnCLabeled->set_image (*iUnCLabeled);
        } else if (!active_now &&  active_before) {
            bUnCLabeled->set_image (*igUnCLabeled);
        }

        // Edited: set the right images
        for (int i = 0; i < 2; i++) {
            active_now = bEdited[i]->get_active();
            active_before = buttons & (1 << (i + 13)); //13,14

            if      ( active_now && !active_before) {
                bEdited[i]->set_image (*iEdited[i]);
            } else if (!active_now &&  active_before) {
                bEdited[i]->set_image (*igEdited[i]);
            }
        }

        // RecentlySaved: set the right images
        for (int i = 0; i < 2; i++) {
            active_now = bRecentlySaved[i]->get_active();
            active_before = buttons & (1 << (i + 15)); //15,16

            if      ( active_now && !active_before) {
                bRecentlySaved[i]->set_image (*iRecentlySaved[i]);
            } else if (!active_now &&  active_before) {
                bRecentlySaved[i]->set_image (*igRecentlySaved[i]);
            }
        }

        fileBrowser->applyFilter (getFilter ());
        _refreshProgressBar();

        //rearrange panels according to the selected filter
        removeIfThere (hBox, trashButtonBox);

        if (bTrash->get_active ()) {
            hBox->pack_start (*trashButtonBox, Gtk::PACK_SHRINK, 4);
        }

        hBox->queue_draw ();

        fileBrowser->setScrollPosition (hScrollPos[lastScrollPos], vScrollPos[lastScrollPos]);
    }

    for (int i = 0; i < numCateg; i++) {
        bCateg[i].block (false);
    }
}

void FileCatalog::showRecursiveToggled()
{
    bool state = bRecursive->get_active();

    if (state == App::get().options().browseRecursive) {
        // avoid unnecessary calls to dirSelected; this can happen when file catalog is reset to an older state
        return;
    }

    App::get().mut_options().browseRecursive = state;

    // Killing background threads can sometimes block the UI for a long time,
    // This may be a spot where giving the user information is needed.
    previewLoader->removeAllJobs();
    thumbImageUpdater->removeAllJobs();

    idle_register.add(
        [this]() -> bool
        {
            dirSelected(selectedDirectory, "");
            return false;
        }
    );
}

BrowserFilter FileCatalog::getFilter ()
{

    BrowserFilter filter;

    bool anyRankFilterActive = bUnRanked->get_active () || bRank[0]->get_active () || bRank[1]->get_active () || bRank[2]->get_active () || bRank[3]->get_active () || bRank[4]->get_active ();
    bool anyCLabelFilterActive = bUnCLabeled->get_active () || bCLabel[0]->get_active () || bCLabel[1]->get_active () || bCLabel[2]->get_active () || bCLabel[3]->get_active () || bCLabel[4]->get_active ();
    bool anyEditedFilterActive = bEdited[0]->get_active() || bEdited[1]->get_active();
    bool anyRecentlySavedFilterActive = bRecentlySaved[0]->get_active() || bRecentlySaved[1]->get_active();
    const bool anySupplementaryActive = bNotTrash->get_active() || bOriginal->get_active();
    /*
     * filter is setup in 2 steps
     * Step 1: handle individual filters
    */
    filter.showRanked[0] = bFilterClear->get_active() || bUnRanked->get_active () || bTrash->get_active () || anySupplementaryActive ||
                           anyCLabelFilterActive || anyEditedFilterActive || anyRecentlySavedFilterActive;

    filter.showCLabeled[0] = bFilterClear->get_active() || bUnCLabeled->get_active () || bTrash->get_active ()  || anySupplementaryActive ||
                             anyRankFilterActive || anyEditedFilterActive || anyRecentlySavedFilterActive;

    for (int i = 1; i <= 5; i++) {
        filter.showRanked[i] = bFilterClear->get_active() || bRank[i - 1]->get_active () || bTrash->get_active () || anySupplementaryActive ||
                               anyCLabelFilterActive || anyEditedFilterActive || anyRecentlySavedFilterActive;

        filter.showCLabeled[i] = bFilterClear->get_active() || bCLabel[i - 1]->get_active () || bTrash->get_active ()  || anySupplementaryActive ||
                                 anyRankFilterActive || anyEditedFilterActive || anyRecentlySavedFilterActive;
    }

    for (int i = 0; i < 2; i++) {
        filter.showEdited[i] = bFilterClear->get_active() || bEdited[i]->get_active () || bTrash->get_active ()  || anySupplementaryActive ||
                               anyRankFilterActive || anyCLabelFilterActive || anyRecentlySavedFilterActive;

        filter.showRecentlySaved[i] = bFilterClear->get_active() || bRecentlySaved[i]->get_active () || bTrash->get_active ()  || anySupplementaryActive ||
                                      anyRankFilterActive || anyCLabelFilterActive || anyEditedFilterActive;
    }


    /*
     * Step 2
     * handle the case when more than 1 filter is selected. This overrides values set in Step
     * if no filters in a group are active, filter.show for each member of that group will be set to true
     * otherwise they are set based on UI input
     */
    if ((anyRankFilterActive && anyCLabelFilterActive ) ||
            (anyRankFilterActive && anyEditedFilterActive ) ||
            (anyRankFilterActive && anyRecentlySavedFilterActive ) ||
            (anyCLabelFilterActive && anyEditedFilterActive ) ||
            (anyCLabelFilterActive && anyRecentlySavedFilterActive ) ||
            (anyEditedFilterActive && anyRecentlySavedFilterActive) ||
            (anySupplementaryActive && (anyRankFilterActive || anyCLabelFilterActive || anyEditedFilterActive || anyRecentlySavedFilterActive))) {

        filter.showRanked[0] = anyRankFilterActive ? bUnRanked->get_active () : true;
        filter.showCLabeled[0] = anyCLabelFilterActive ? bUnCLabeled->get_active () : true;

        for (int i = 1; i <= 5; i++) {
            filter.showRanked[i] = anyRankFilterActive ? bRank[i - 1]->get_active () : true;
            filter.showCLabeled[i] = anyCLabelFilterActive ? bCLabel[i - 1]->get_active () : true;
        }

        for (int i = 0; i < 2; i++) {
            filter.showEdited[i] = anyEditedFilterActive ? bEdited[i]->get_active() : true;
            filter.showRecentlySaved[i] = anyRecentlySavedFilterActive ? bRecentlySaved[i]->get_active() : true;
        }
    }


    filter.showTrash = bTrash->get_active () || !bNotTrash->get_active ();
    filter.showNotTrash = !bTrash->get_active ();
    filter.showOriginal = bOriginal->get_active();

    if (!filterPanel) {
        filter.exifFilterEnabled = false;
    } else {
        if (!hasValidCurrentEFS) {
            MyMutex::MyLock lock(dirEFSMutex);
            filter.exifFilter = dirEFS;
        } else {
            filter.exifFilter = currentEFS;
        }

        filter.exifFilterEnabled = filterPanel->isEnabled ();
    }

    //TODO add support for more query options. e.g by date, iso, f-number, etc
    //TODO could use date:<value>;iso:<value>  etc
    // default will be filename

    Glib::ustring decodedQueryFileName = Query->get_text(); // for now Query is only by file name

    // Determine the match mode - check if the first 2 characters are equal to "!="
    if (decodedQueryFileName.find("!=") == 0) {
        decodedQueryFileName = decodedQueryFileName.substr(2);
        filter.matchEqual = false;
    } else {
        filter.matchEqual = true;
    }

    // Consider that queryFileName consist of comma separated values (FilterString)
    // Evaluate if ANY of these FilterString are contained in the filename
    // This will construct OR filter within the queryFileName
    filter.vFilterStrings.clear();
    const std::vector<Glib::ustring> filterStrings = Glib::Regex::split_simple(",", decodedQueryFileName.uppercase());
    for (const auto& entry : filterStrings) {
        // ignore empty filterStrings. Otherwise filter will always return true if
        // e.g. queryFileName ends on "," and will stop being a filter
        if (!entry.empty()) {
            filter.vFilterStrings.push_back(entry);
        }
    }
    return filter;
}

void FileCatalog::filterChanged ()
{
    //TODO !!! there is too many repetitive and unnecessary executions of
    // " fileBrowser->applyFilter (getFilter()); " throughout the code
    // this needs further analysis and cleanup
    fileBrowser->applyFilter (getFilter());
    _refreshProgressBar();
}

void FileCatalog::saveResetState ()
{
    resetData.directory = selectedDirectory;
    resetData.recursive = App::get().options().browseRecursive;
}

bool FileCatalog::restoreResetState ()
{
    bool ret = false;

    if (resetData.recursive != App::get().options().browseRecursive) {
        // I think the program flow here needs to be explained:
        // The App::get().mut_options().browseRecursive is indeed set by the signal of the toggle button "showRecursiveToggled()"
        // However, the signal is handled only after the execution returns to the framework. The "buttonBrowsePathPressed()"
        // on the other hand instantly executes "DirBrowser::selectDir". If the recursive value is not set when this executes, the
        // directory might not contain the image of the "refImageForOpen_fname" variable or the one next to it that is supposed to
        // be opened.
        if (resetData.directory != selectedDirectory) {
            // this can be set only when "DirBrowser::selectDir" is called regardless of the recursive toggle or the toggle will get blocked
            App::get().mut_options().browseRecursive = resetData.recursive;
        }
        bRecursive->set_active(resetData.recursive);
        ret = true;
    }

    if (resetData.directory != selectedDirectory) {
        BrowsePath->set_text(resetData.directory);
        buttonBrowsePathPressed ();
        ret = true;
    }

    return ret;
}

bool FileCatalog::eventDeletedFile(const Glib::RefPtr<Gio::File>& file)
{
    auto pos = std::find(fileNameList.cbegin(), fileNameList.cend(), file->get_path());
    if (pos != fileNameList.cend()) {
        cacheMgr->clearFromCache(file->get_path(), true);
        delete fileBrowser->delEntry(file->get_path());

        fileNameList.erase(pos);

        _refreshProgressBar();

        return true;
    }

    return false;
}

void FileCatalog::eventDeletedDirectory(const Glib::RefPtr<Gio::File>& directory)
{
    if (!directory) {
        return;
    }

    std::vector<Glib::ustring> filesToDel;

    const std::vector<ThumbBrowserEntryBase*>& t = fileBrowser->getEntries();
    for (const auto& entry : t) {
        // trying to match with files from the directory and its subdirectories based on path
        if (entry->filename == directory->get_path() || entry->filename.rfind(directory->get_path() + G_DIR_SEPARATOR, 0) == 0) {
            filesToDel.push_back(entry->filename);
        }
    }

    bool refresh = false;

    for (const auto& toDelete : filesToDel) {
        cacheMgr->clearFromCache(toDelete, true);
        delete fileBrowser->delEntry(toDelete);

        fileNameList.erase(std::remove(fileNameList.begin(), fileNameList.end(), toDelete), fileNameList.end());

        refresh = true;
    }

    // if the directory is being cut out, the subfolders will not get their own events
    // deleting all monitors that start with a matching path
    dirMonitors.erase(std::remove_if(dirMonitors.begin(), dirMonitors.end(),
                    [&directory](const FileMonitorInfo &fileMonitorInfo)
                    { 
                        if (fileMonitorInfo.filePath == directory->get_path()) {
                            return true;
                        } else {
                            return (fileMonitorInfo.filePath.rfind(directory->get_path() + G_DIR_SEPARATOR, 0) == 0);
                        }
                    })
                    , dirMonitors.end());

    if (refresh) {
        _refreshProgressBar();
    }
}

void FileCatalog::eventDirectoryCreated(const Glib::RefPtr<Gio::File>& directory)
{
    const auto& options = App::get().options();

    if (!options.browseRecursive) {
        return;
    }

    int recursiondepth = GetRecursionDepth(selectedDirectory, directory);

    if (recursiondepth > options.browseRecursiveDepth || static_cast<int>(dirMonitors.size()) >= options.browseRecursiveMaxDirs) {
        return;
    }

    if (std::find_if(dirMonitors.cbegin(), dirMonitors.cend(), 
                    [&directory](const FileMonitorInfo &fileMonitorInfo) { return fileMonitorInfo.filePath == directory->get_path(); }
                    ) != dirMonitors.cend()) {
        // the monitor already exists
        return;
    }

    // only creating monitor, the files get individual events
    auto dir_path = directory->get_path();
    auto dir_monitor = directory->monitor_directory();
    dir_monitor->signal_changed().connect(sigc::mem_fun(*this, &FileCatalog::on_dir_changed));
    dirMonitors.emplace_back(dir_monitor, dir_path);
}

void FileCatalog::eventChangesDoneDirectory(const Glib::RefPtr<Gio::File>& directory)
{
    const auto& options = App::get().options();

    if (!options.browseRecursive) {
        return;
    }

    int recursiondepth = GetRecursionDepth(selectedDirectory, directory);

    if (recursiondepth > options.browseRecursiveDepth || static_cast<int>(dirMonitors.size()) >= options.browseRecursiveMaxDirs) {
        return;
    }
    // Motivation for having a recursive search for files and directories:
    // * If the directory was cut&pasted, there are no events for individual files
    // * Also, sometimes events come in wrong order (files before dir), in which case a file could have been left undiscovered

    // build a set of collate-keys for faster search
    std::set<std::string> oldNames;
    for (const auto& oldName : fileNameList) {
        oldNames.insert(oldName.collate_key());
    }

    std::vector<Glib::RefPtr<Gio::File>> newDirs;
    std::vector<Glib::ustring> fileList = getFileList(directory->get_path(), recursiondepth, &newDirs);
    for (const auto& newName : fileList) {
        if (oldNames.find(newName.collate_key()) == oldNames.end()) {
            addToPendingFiles(newName);
        }
    }
    _refreshProgressBar();

    refreshDirectoryMonitors(newDirs, true);
}

void FileCatalog::eventChangesDoneFile(const Glib::RefPtr<Gio::File>& file)
{
    auto pendingFile = std::find_if(pendingFiles.begin(), pendingFiles.end(),
                                    [&file](const std::pair<FileCClock::time_point,Glib::ustring>& element){ return element.second == file->get_path(); });
    if (pendingFile != pendingFiles.end()) {
        pendingFile->first = FileCClock::now();
    } else {
        if (std::find(fileNameList.cbegin(), fileNameList.cend(), file->get_path()) == fileNameList.cend()) {
            // Probably the thumbnail has failed previously, re-adding it now as the file has had changes written to it
            pendingFiles.push_back(std::make_pair(FileCClock::now(),file->get_path()));
        }
    }
}

void FileCatalog::on_dir_changed (const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& other_file, Gio::FileMonitorEvent event_type)
{
    if (event_type == Gio::FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED) {
        return;
    }

    if (event_type == Gio::FILE_MONITOR_EVENT_CHANGED) {
        // CHANGED event only updates times in pendingFiles if there are any, be done with that as fast as possible
        auto pendingFile = std::find_if(pendingFiles.begin(), pendingFiles.end(),
                                        [&file](const std::pair<FileCClock::time_point,Glib::ustring>& e){ return e.second == file->get_path(); });
        if (pendingFile != pendingFiles.end()) {
            pendingFile->first = FileCClock::now();
        }
        return;
    }

    if (selectedDirectory.empty()) {
        return;
    }

    if (!Glib::file_test(selectedDirectory, Glib::FILE_TEST_IS_DIR)) {
        closeDir();
        return;
    }

    bool isDirectory = Glib::file_test(file->get_path(), Glib::FileTest::FILE_TEST_IS_DIR);

    if (!isDirectory && !App::get().options().has_retained_extention(file->get_parse_name())
                     && !(event_type == Gio::FILE_MONITOR_EVENT_DELETED)) {
        return;
    }

    // Notes for future reference:
    // * Some events get duplicated at least when monitoring an SMB drive connected to a Windows 10
    // * Gio::FileMonitor blocks deletes and or delete events on the monitored items (on RawTherapee this means directories, no problems with files) on SMB network shares on Windows 10.

    switch (event_type) {
        case Gio::FILE_MONITOR_EVENT_CREATED:
            if (isDirectory) {
                eventDirectoryCreated(file);
            }
            break;
        case Gio::FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
            if (!isDirectory) {
                eventChangesDoneFile(file);
            } else {
                eventChangesDoneDirectory(file);
            }  
            break;
        case Gio::FILE_MONITOR_EVENT_DELETED:
            if (!eventDeletedFile(file)) {
                // There is no way to test, that I know of, if the passed file parameter refers to a file or a directory.
                // So the logic is: if the eventDeletedFile fails, it is a directory.
                // this will cause overhead, when events for already handled files come but to resolve it completely may need some reorganization of data
                eventDeletedDirectory(file);
            }
            break;
        default:
            break;
    }
}

bool FileCatalog::timerEvents()
{
    //
    // Delayed thumbnail generation
    // There are many bugs happening if the thumbnail generation is attempted too early. These include failed thumbnails
    // and crashes when processed images are saved to the monitored directory.
    bool fileAdded = false;
    for (auto it = pendingFiles.begin(); it != pendingFiles.end(); ) {
        auto since = std::chrono::duration_cast<std::chrono::milliseconds>(FileCClock::now()-it->first);
        if (since.count() > (App::get().options().newFileDelayTime * 1000)){
            if (std::find(fileNameList.cbegin(), fileNameList.cend(), it->second) == fileNameList.cend()) {
                addFile(it->second);
                fileAdded = true;
            }

            it = pendingFiles.erase(it);
        } else {
            it++;
        }
    }
    if (fileAdded) {
        filepanel->loadingThumbs(M("PROGRESSBAR_LOADINGTHUMBS"), 0);
        _refreshProgressBar();
    }

    return true;
}

void FileCatalog::addFile (const Glib::ustring& fName)
{
    if (!fName.empty()) {
        fileNameList.push_back(fName);
        previewLoader->add(selectedDirectoryId, fName, this);
        previewsToLoad++;
    }
}

void FileCatalog::addToPendingFiles (const Glib::ustring& fName)
{
    auto found = std::find_if(pendingFiles.cbegin(), pendingFiles.cend(),
                            [&fName](const std::pair<FileCClock::time_point,Glib::ustring>& e){ return e.second == fName; });
    if (found == pendingFiles.cend()) {
        pendingFiles.push_back(std::make_pair(FileCClock::now(), fName));
    }
}

void FileCatalog::addAndOpenFile (const Glib::ustring& fname)
{
    auto file = Gio::File::create_for_path(fname);

    if (!file ) {
        return;
    }

    if (!file->query_exists()) {
        return;
    }

    try {

        const auto info = file->query_info();

        if (!info) {
            return;
        }

        const auto lastdot = info->get_name().find_last_of('.');
        if (lastdot != Glib::ustring::npos) {
            if (!App::get().options().is_extention_enabled(info->get_name().substr(lastdot + 1))) {
                return;
            }
        } else {
            return;
        }


        // if supported, load thumbnail first
        const auto tmb = cacheMgr->getEntry(file->get_parse_name());

        if (!tmb) {
            return;
        }

        FileBrowserEntry* entry = new FileBrowserEntry(tmb, file->get_parse_name());
        previewReady(selectedDirectoryId, entry);
        // open the file
        tmb->increaseRef();
        idle_register.add(
            [this, tmb]() -> bool
            {
                _openImage({tmb});
                return false;
            }
        );

    } catch(Gio::Error&) {}
}

void FileCatalog::emptyTrash ()
{

    const auto& t = fileBrowser->getEntries();
    std::vector<FileBrowserEntry*> toDel;

    for (const auto entry : t) {
        if ((static_cast<FileBrowserEntry*>(entry))->thumbnail->getTrashed()) {
            toDel.push_back(static_cast<FileBrowserEntry*>(entry));
        }
    }
    if (toDel.size() > 0) {
        deleteRequested(toDel, false, false);
        trashChanged();
    }
}

bool FileCatalog::trashIsEmpty ()
{

    const auto& t = fileBrowser->getEntries();

    for (const auto entry : t) {
        if ((static_cast<FileBrowserEntry*>(entry))->thumbnail->getTrashed()) {
            return false;
        }
    }
    return true;
}

void FileCatalog::zoomIn ()
{

    fileBrowser->zoomIn ();
    refreshHeight();

}
void FileCatalog::zoomOut ()
{

    fileBrowser->zoomOut ();
    refreshHeight();

}
void FileCatalog::refreshEditedState (const std::set<Glib::ustring>& efiles)
{

    editedFiles = efiles;
    fileBrowser->refreshEditedState (efiles);
}

void FileCatalog::exportRequested()
{
}

// Called within GTK UI thread
void FileCatalog::exifFilterChanged ()
{

    currentEFS = filterPanel->getFilter ();
    hasValidCurrentEFS = true;
    fileBrowser->applyFilter (getFilter ());
    _refreshProgressBar();
}

void FileCatalog::setFilterPanel (FilterPanel* fpanel)
{

    filterPanel = fpanel;
    filterPanel->set_sensitive (false);
    filterPanel->setFilterPanelListener (this);
}

void FileCatalog::setExportPanel(ExportPanel* expanel)
{
    exportPanel = expanel;
    exportPanel->set_sensitive (false);
    exportPanel->setExportPanelListener (this);
    fileBrowser->setExportPanel(expanel);
}

void FileCatalog::trashChanged ()
{
    if (trashIsEmpty()) {
        bTrash->set_image(*iTrashShowEmpty);
    } else {
        bTrash->set_image(*iTrashShowFull);
    }
}

// Called within GTK UI thread
void FileCatalog::buttonQueryClearPressed ()
{
    Query->set_text("");
    FileCatalog::executeQuery ();
}

// Called within GTK UI thread
void FileCatalog::executeQuery()
{
    // if BrowsePath text was changed, do a full browse;
    // otherwise filter only

    if (BrowsePath->get_text() != selectedDirectory) {
        buttonBrowsePathPressed ();
    } else {
        FileCatalog::filterChanged ();
    }
}

bool FileCatalog::Query_key_pressed (GdkEventKey *event)
{

    bool shift = event->state & GDK_SHIFT_MASK;

    switch (event->keyval) {
    case GDK_KEY_Escape:

        // Clear Query if the Escape character is pressed within it
        if (!shift) {
            FileCatalog::buttonQueryClearPressed ();
            return true;
        }

        break;

    default:
        break;
    }

    return false;
}

void FileCatalog::updateFBQueryTB (bool singleRow)
{
    hbToolBar1->reference();

    if (singleRow) {
        if (hbToolBar1STB) {
            hbToolBar1STB->remove_with_viewport();
            removeIfThere(this, hbToolBar1STB, false);
            buttonBar->pack_start(*hbToolBar1, Gtk::PACK_EXPAND_WIDGET, 0);
            hbToolBar1STB = nullptr;
        }
    } else {
        if (!hbToolBar1STB) {
            removeIfThere(buttonBar, hbToolBar1, false);
            hbToolBar1STB = Gtk::manage(new MyScrolledToolbar());
            hbToolBar1STB->set_name("FileBrowserQueryToolbar");
            hbToolBar1STB->add(*hbToolBar1);
            hbToolBar1STB->show();
            pack_start (*hbToolBar1STB, Gtk::PACK_SHRINK, 0);
            reorder_child(*hbToolBar1STB, 0);
        }
    }

    hbToolBar1->unreference();
}

void FileCatalog::updateFBToolBarVisibility (bool showFilmStripToolBar)
{
    if (showFilmStripToolBar) {
        showToolBar();
    } else {
        hideToolBar();
    }

    refreshHeight();
}

void FileCatalog::buttonBrowsePathPressed ()
{
    Glib::ustring BrowsePathValue = BrowsePath->get_text();
    Glib::ustring DecodedPathPrefix = "";
    Glib::ustring FirstChar;

    // handle shortcuts in the BrowsePath -- START
    // read the 1-st character from the path
    FirstChar = BrowsePathValue.substr (0, 1);

    if (FirstChar == "~") { // home directory
        DecodedPathPrefix = PlacesBrowser::userHomeDir ();
    } else if (FirstChar == "!") { // user's pictures directory
        DecodedPathPrefix = PlacesBrowser::userPicturesDir ();
    }

    if (!DecodedPathPrefix.empty()) {
        BrowsePathValue = Glib::ustring::compose ("%1%2", DecodedPathPrefix, BrowsePathValue.substr (1, BrowsePath->get_text_length() - 1));
        BrowsePath->set_text(BrowsePathValue);
    }

    // handle shortcuts in the BrowsePath -- END

    // validate the path
    if (Glib::file_test(BrowsePathValue, Glib::FILE_TEST_IS_DIR) && selectDir) {
        selectDir (BrowsePathValue);
    } else
        // error, likely path not found: show red arrow
    {
        buttonBrowsePath->set_image (*iRefreshRed);
    }
}

bool FileCatalog::BrowsePath_key_pressed (GdkEventKey *event)
{

    bool shift = event->state & GDK_SHIFT_MASK;

    switch (event->keyval) {
    case GDK_KEY_Escape:

        // On Escape character Reset BrowsePath to selectedDirectory
        if (!shift) {
            BrowsePath->set_text(selectedDirectory);
            // place cursor at the end
            BrowsePath->select_region(BrowsePath->get_text_length(), BrowsePath->get_text_length());
            return true;
        }

        break;

    default:
        break;
    }

    return false;
}

void FileCatalog::tbLeftPanel_1_visible (bool visible)
{
    if (visible) {
        tbLeftPanel_1->show();
        vSepiLeftPanel->show();
    } else {
        tbLeftPanel_1->hide();
        vSepiLeftPanel->hide();
    }
}
void FileCatalog::tbRightPanel_1_visible (bool visible)
{
    if (visible) {
        tbRightPanel_1->show();
    } else {
        tbRightPanel_1->hide();
    }
}
void FileCatalog::tbLeftPanel_1_toggled ()
{
    removeIfThere (filepanel->dirpaned, filepanel->placespaned, false);

    auto& options = App::get().mut_options();
    if (tbLeftPanel_1->get_active()) {
        filepanel->dirpaned->pack1 (*filepanel->placespaned, false, true);
        tbLeftPanel_1->set_image (*iLeftPanel_1_Hide);
        options.browserDirPanelOpened = true;
    } else {
        tbLeftPanel_1->set_image (*iLeftPanel_1_Show);
        options.browserDirPanelOpened = false;
    }
}

void FileCatalog::tbRightPanel_1_toggled ()
{
    auto& options = App::get().mut_options();
    if (tbRightPanel_1->get_active()) {
        filepanel->rightBox->show();
        tbRightPanel_1->set_image (*iRightPanel_1_Hide);
        options.browserToolPanelOpened = true;
    } else {
        filepanel->rightBox->hide();
        tbRightPanel_1->set_image (*iRightPanel_1_Show);
        options.browserToolPanelOpened = false;
    }
}

bool FileCatalog::CheckSidePanelsVisibility()
{
    return tbLeftPanel_1->get_active() || tbRightPanel_1->get_active();
}

void FileCatalog::toggleSidePanels()
{
    // toggle left AND right panels

    bool bAllSidePanelsVisible;
    bAllSidePanelsVisible = CheckSidePanelsVisibility();

    tbLeftPanel_1->set_active (!bAllSidePanelsVisible);
    tbRightPanel_1->set_active (!bAllSidePanelsVisible);
}

void FileCatalog::toggleLeftPanel()
{
    tbLeftPanel_1->set_active (!tbLeftPanel_1->get_active());
}

void FileCatalog::toggleRightPanel()
{
    tbRightPanel_1->set_active (!tbRightPanel_1->get_active());
}


void FileCatalog::selectImage (Glib::ustring fname, bool clearFilters)
{
    if (clearFilters) { // clear all filters
        Query->set_text("");
        categoryButtonToggled(bFilterClear, false);

        // disable exif filters
        if (filterPanel->isEnabled()) {
            filterPanel->setEnabled (false);
        }
    }

    if (restoreResetState()) {
        // Directory was changed -
        // If the user has traversed around directories in the File Browser and now wants to
        // reset the file catalog to the directory of the opened image with X or Y key
        //
        // the actual selection of image will be handled asynchronously at the end of FileCatalog::previewsFinishedUI
        imageToSelect_fname = fname;
    } else {
        fileBrowser->selectImage(fname);
        imageToSelect_fname = "";
    }
}


void FileCatalog::openNextPreviousEditorImage (Glib::ustring fname, eRTNav nextPrevious)
{
    if (restoreResetState()) {
        // Directory was changed -
        // If the user has traversed around directories in the File Browser and now wants to
        // continue from the image opened in the editor with SHIFT+F3/F4 keys
        refImageForOpen_fname = fname;
        actionNextPrevious = nextPrevious;
    } else {
        fileBrowser->openNextPreviousEditorImage(fname, nextPrevious);
        refImageForOpen_fname = "";
        actionNextPrevious = NAV_NONE;
    }
}

bool FileCatalog::handleShortcutKey (GdkEventKey* event)
{

    bool ctrl = event->state & GDK_CONTROL_MASK;
    bool shift = event->state & GDK_SHIFT_MASK;
    bool alt = event->state & GDK_MOD1_MASK;
#ifdef __WIN32__
    bool altgr = event->state & GDK_MOD2_MASK;
#else
    bool altgr = event->state & GDK_MOD5_MASK;
#endif
    modifierKey = event->state;

    // GUI Layout
    switch(event->keyval) {
    case GDK_KEY_l:
        if (!alt) {
            tbLeftPanel_1->set_active (!tbLeftPanel_1->get_active());    // toggle left panel
        }

        if (alt && !ctrl) {
            tbRightPanel_1->set_active (!tbRightPanel_1->get_active());    // toggle right panel
        }

        if (alt && ctrl) {
            tbLeftPanel_1->set_active (!tbLeftPanel_1->get_active()); // toggle left panel
            tbRightPanel_1->set_active (!tbRightPanel_1->get_active()); // toggle right panel
        }

        return true;

    case GDK_KEY_m:
        if (!ctrl && !alt) {
            toggleSidePanels();
        }

        return true;
    }

    if (shift) {
        switch(event->keyval) {
        case GDK_KEY_Escape:
            BrowsePath->set_text(selectedDirectory);
            // set focus on something neutral, this is useful to remove focus from BrowsePath and Query
            // when need to execute a shortcut, which otherwise will be typed into those fields
            filepanel->grab_focus();
            return true;
        }
    }

#ifdef __WIN32__

    if (!alt && shift && !altgr) {
        switch(event->hardware_keycode) {
        case 0x30:
            categoryButtonToggled(bUnRanked, false);
            return true;

        case 0x31:
            categoryButtonToggled(bRank[0], false);
            return true;

        case 0x32:
            categoryButtonToggled(bRank[1], false);
            return true;

        case 0x33:
            categoryButtonToggled(bRank[2], false);
            return true;

        case 0x34:
            categoryButtonToggled(bRank[3], false);
            return true;

        case 0x35:
            categoryButtonToggled(bRank[4], false);
            return true;

        case 0x36:
            categoryButtonToggled(bEdited[0], false);
            return true;

        case 0x37:
            categoryButtonToggled(bEdited[1], false);
            return true;
        }
    }

    if (!alt && !shift) {
        switch(event->keyval) {

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (BrowsePath->is_focus()) {
                FileCatalog::buttonBrowsePathPressed ();
                return true;
            }

            break;
        }
    }

    if (alt && !shift) { // shift is reserved for color labeling
        switch(event->hardware_keycode) {
        case 0x30:
            categoryButtonToggled(bUnCLabeled, false);
            return true;

        case 0x31:
            categoryButtonToggled(bCLabel[0], false);
            return true;

        case 0x32:
            categoryButtonToggled(bCLabel[1], false);
            return true;

        case 0x33:
            categoryButtonToggled(bCLabel[2], false);
            return true;

        case 0x34:
            categoryButtonToggled(bCLabel[3], false);
            return true;

        case 0x35:
            categoryButtonToggled(bCLabel[4], false);
            return true;

        case 0x36:
            categoryButtonToggled(bRecentlySaved[0], false);
            return true;

        case 0x37:
            categoryButtonToggled(bRecentlySaved[1], false);
            return true;
        }
    }

#else

    if (!alt && shift && !altgr) {
        switch(event->hardware_keycode) {
        case 0x13:
            categoryButtonToggled(bUnRanked, false);
            return true;

        case 0x0a:
            categoryButtonToggled(bRank[0], false);
            return true;

        case 0x0b:
            categoryButtonToggled(bRank[1], false);
            return true;

        case 0x0c:
            categoryButtonToggled(bRank[2], false);
            return true;

        case 0x0d:
            categoryButtonToggled(bRank[3], false);
            return true;

        case 0x0e:
            categoryButtonToggled(bRank[4], false);
            return true;

        case 0x0f:
            categoryButtonToggled(bEdited[0], false);
            return true;

        case 0x10:
            categoryButtonToggled(bEdited[1], false);
            return true;
        }
    }

    if (!alt && !shift) {
        switch(event->keyval) {

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (BrowsePath->is_focus()) {
                FileCatalog::buttonBrowsePathPressed ();
                return true;
            }

            break;
        }
    }

    if (alt && !shift) { // shift is reserved for color labeling
        switch(event->hardware_keycode) {
        case 0x13:
            categoryButtonToggled(bUnCLabeled, false);
            return true;

        case 0x0a:
            categoryButtonToggled(bCLabel[0], false);
            return true;

        case 0x0b:
            categoryButtonToggled(bCLabel[1], false);
            return true;

        case 0x0c:
            categoryButtonToggled(bCLabel[2], false);
            return true;

        case 0x0d:
            categoryButtonToggled(bCLabel[3], false);
            return true;

        case 0x0e:
            categoryButtonToggled(bCLabel[4], false);
            return true;

        case 0x0f:
            categoryButtonToggled(bRecentlySaved[0], false);
            return true;

        case 0x10:
            categoryButtonToggled(bRecentlySaved[1], false);
            return true;
        }
    }

#endif

    if (!ctrl && !alt) {
        switch(event->keyval) {
        case GDK_KEY_d:
        case GDK_KEY_D:
            categoryButtonToggled(bFilterClear, false);
            return true;
        }
    }

    auto& options = App::get().mut_options();
    if (!ctrl || (alt && !options.tabbedUI)) {
        switch(event->keyval) {

        case GDK_KEY_bracketright:
            coarsePanel->rotateRight();
            return true;

        case GDK_KEY_bracketleft:
            coarsePanel->rotateLeft();
            return true;

        case GDK_KEY_i:
        case GDK_KEY_I:
            exifInfo->set_active (!exifInfo->get_active());
            return true;

        case GDK_KEY_plus:
        case GDK_KEY_equal:
            zoomIn();
            return true;

        case GDK_KEY_minus:
        case GDK_KEY_underscore:
            zoomOut();
            return true;
        default: // do nothing, avoids a cppcheck false positive
            break;
        }
    }

    if (ctrl && !alt) {
        switch (event->keyval) {
        case GDK_KEY_o:
            BrowsePath->select_region(0, BrowsePath->get_text_length());
            BrowsePath->grab_focus();
            return true;

        case GDK_KEY_f:
            Query->select_region(0, Query->get_text_length());
            Query->grab_focus();
            return true;

        case GDK_KEY_t:
        case GDK_KEY_T:
            modifierKey = 0; // HOMBRE: yet another hack.... otherwise the shortcut won't work
            categoryButtonToggled(bTrash, false);
            return true;
        }
    }

    if (!ctrl && !alt && shift) {
        switch (event->keyval) {
        case GDK_KEY_t:
        case GDK_KEY_T:
            if (inTabMode) {
                if (options.showFilmStripToolBar) {
                    hideToolBar();
                } else {
                    showToolBar();
                }

                options.showFilmStripToolBar = !options.showFilmStripToolBar;
            }

            return true;
        }
    }

    if (!ctrl && !alt && !shift) {
        switch (event->keyval) {
        case GDK_KEY_t:
        case GDK_KEY_T:
            if (inTabMode) {
                if (options.showFilmStripToolBar) {
                    hideToolBar();
                } else {
                    showToolBar();
                }

                options.showFilmStripToolBar = !options.showFilmStripToolBar;
            }

            refreshHeight();
            return true;
        }
    }

    if (!ctrl && !alt) {
        switch (event->keyval) {
        case GDK_KEY_f:
            fileBrowser->getInspector()->showWindow(false, true);
            return true;
        case GDK_KEY_F:
            fileBrowser->getInspector()->showWindow(false, false);
            return true;
        }
    }

    return fileBrowser->keyPressed(event);
}

bool FileCatalog::handleShortcutKeyRelease(GdkEventKey* event)
{
    bool ctrl = event->state & GDK_CONTROL_MASK;
    bool alt = event->state & GDK_MOD1_MASK;

    if (!ctrl && !alt) {
        switch (event->keyval) {
        case GDK_KEY_f:
        case GDK_KEY_F:
            fileBrowser->getInspector()->hideWindow();
            return true;
        }
    }

    return false;
}

void FileCatalog::showToolBar()
{
    if (hbToolBar1STB) {
        hbToolBar1STB->show();
    }

    buttonBar->show();
}

void FileCatalog::hideToolBar()
{
    if (hbToolBar1STB) {
        hbToolBar1STB->hide();
    }

    buttonBar->hide();
}
