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

#include <set>

#include <giomm.h>

#include "exiffiltersettings.h"
#include "exportpanel.h"
#include "filebrowser.h"
#include "fileselectionchangelistener.h"
#include "fileselectionlistener.h"
#include "filterpanel.h"
#include "previewloader.h"
#include "threadutils.h"

#include "rtengine/noncopyable.h"

class FilePanel;
class CoarsePanel;
class ToolBar;

/*
 * Class:
 *   - handling the list of file (add/remove them)
 *   - handling the thumbnail toolbar,
 *   - monitoring the directory (for any change)
 */
class FileCatalog final : public Gtk::Box,
    public PreviewLoaderListener,
    public FilterPanelListener,
    public FileBrowserListener,
    public ExportPanelListener,
    public rtengine::NonCopyable
{
public:
    typedef sigc::slot<void, const Glib::ustring&> DirSelectionSlot;

private:
    typedef std::chrono::system_clock FileCClock;

    struct FileMonitorInfo {
        FileMonitorInfo(const Glib::RefPtr<Gio::FileMonitor> &file_monitor, const Glib::ustring &file_path) :
            fileMonitor(file_monitor), filePath(file_path) {}
        Glib::RefPtr<Gio::FileMonitor> fileMonitor;
        Glib::ustring filePath;
    };

    /**
     * @brief Data that is used to reset the file catalog contents if user presses Y,X or SHIFT+F3/F4
     */
    struct DirectoryResetInfo {
        DirectoryResetInfo() :
            recursive(false) {}
        bool          recursive;
        Glib::ustring directory;
    };

    FilePanel* filepanel;
    Gtk::Box* hBox;
    Glib::ustring selectedDirectory;
    DirectoryResetInfo resetData;
    int selectedDirectoryId;
    bool enabled;
    bool inTabMode;  // Tab mode has e.g. different progress bar handling
    Glib::ustring imageToSelect_fname;
    Glib::ustring refImageForOpen_fname; // Next/previous for Editor's perspective
    eRTNav actionNextPrevious;

    FileSelectionListener* listener;
    FileSelectionChangeListener* fslistener;
    ImageAreaToolListener* iatlistener;
    DirSelectionSlot selectDir;

    Gtk::Box* buttonBar;
    Gtk::Box* hbToolBar1;
    MyScrolledToolbar* hbToolBar1STB;

    Gtk::Box* fltrRankbox;
    Gtk::Box* fltrLabelbox;
    Gtk::Box* fltrVbox1;

    Gtk::Box* fltrEditedBox;
    Gtk::Box* fltrRecentlySavedBox;
    Gtk::Box* fltrVbox2;

    Gtk::Separator* vSepiLeftPanel;

    Gtk::ToggleButton* tbLeftPanel_1;
    Gtk::ToggleButton* tbRightPanel_1;
    Gtk::ToggleButton* bFilterClear;
    Gtk::ToggleButton* bUnRanked;
    Gtk::ToggleButton* bRank[5];
    Gtk::ToggleButton* bUnCLabeled;
    Gtk::ToggleButton* bCLabel[5];//color label
    Gtk::ToggleButton* bEdited[2];
    Gtk::ToggleButton* bRecentlySaved[2];
    Gtk::ToggleButton* bTrash;
    Gtk::ToggleButton* bNotTrash;
    Gtk::ToggleButton* bOriginal;
    Gtk::ToggleButton* bRecursive;
    Gtk::ToggleButton* categoryButtons[20];
    Gtk::ToggleButton* exifInfo;
    sigc::connection bCateg[20];
    Gtk::Image* iFilterClear, *igFilterClear;
    Gtk::Image* iranked[5], *igranked[5], *iUnRanked, *igUnRanked;
    Gtk::Image* iCLabeled[5], *igCLabeled[5], *iUnCLabeled, *igUnCLabeled;
    Gtk::Image* iEdited[2], *igEdited[2];
    Gtk::Image* iRecentlySaved[2], *igRecentlySaved[2];
    Gtk::Image *iTrashShowEmpty, *iTrashShowFull;
    Gtk::Image *iNotTrash, *iOriginal;
    Gtk::Image *iRefreshWhite, *iRefreshRed;
    Gtk::Image *iLeftPanel_1_Show, *iLeftPanel_1_Hide, *iRightPanel_1_Show, *iRightPanel_1_Hide;
    Gtk::Image *iQueryClear;

    Gtk::Entry* BrowsePath;
    Gtk::Button* buttonBrowsePath;

    Gtk::Entry* Query;
    Gtk::Button* buttonQueryClear;

    double hScrollPos[18];
    double vScrollPos[18];
    int lastScrollPos;

    Gtk::Box* trashButtonBox;

    Gtk::Button* zoomInButton;
    Gtk::Button* zoomOutButton;

    RTImage* progressImage;
    Gtk::Label* progressLabel;

    MyMutex dirEFSMutex;
    ExifFilterSettings dirEFS;
    ExifFilterSettings currentEFS;
    bool hasValidCurrentEFS;

    FilterPanel* filterPanel;
    ExportPanel* exportPanel;

    std::size_t previewsToLoad;

    std::vector<Glib::ustring> fileNameList;                ///< Currently managed files.
    std::vector<std::pair<FileCClock::time_point,Glib::ustring>> pendingFiles; ///< Files that have been created but are not yet managed.
    std::set<Glib::ustring> editedFiles;
    guint modifierKey; // any modifiers held when rank button was pressed

    std::vector<FileMonitorInfo> dirMonitors;

    IdleRegister idle_register;

    sigc::connection   timerEventSource;                    ///< File monitoring timer

    void addAndOpenFile (const Glib::ustring& fname);
    void addFile (const Glib::ustring& fName);
    void addToPendingFiles (const Glib::ustring& fName);
    std::vector<Glib::ustring> getFileList(std::vector<Glib::RefPtr<Gio::File>> *dirs_explored = nullptr);
    std::vector<Glib::ustring> getFileList(Glib::ustring root, int start_depth, std::vector<Glib::RefPtr<Gio::File>> *dirs_explored = nullptr);
    BrowserFilter getFilter ();
    void refreshDirectoryMonitors(const std::vector<Glib::RefPtr<Gio::File>> &dirs_to_monitor, bool compound_update = false);
    void trashChanged ();

    bool eventDeletedFile(const Glib::RefPtr<Gio::File>& file);
    void eventDeletedDirectory(const Glib::RefPtr<Gio::File>& directory);
    void eventDirectoryCreated(const Glib::RefPtr<Gio::File>& directory);
    void eventChangesDoneDirectory(const Glib::RefPtr<Gio::File>& directory);
    void eventChangesDoneFile(const Glib::RefPtr<Gio::File>& file);

public:
    // thumbnail browsers
    FileBrowser* fileBrowser;

    CoarsePanel* coarsePanel;
    ToolBar* toolBar;

    FileCatalog (CoarsePanel* cp, ToolBar* tb, FilePanel* filepanel);
    ~FileCatalog() override;
    void dirSelected (const Glib::ustring& dirname, const Glib::ustring& openfile);
    void closeDir    ();
    void refreshEditedState (const std::set<Glib::ustring>& efiles);

    // previewloaderlistener interface
    void previewFailed(int dir_id, Glib::ustring file, FailReason reason) override;
    void previewReady (int dir_id, FileBrowserEntry* fdn) override;
    void previewsFinished (int dir_id) override;
    // called asynchronously from the main event loop
    void previewsFinishedUI(int dir_id);

    void _refreshProgressBar();

    void setInspector(Inspector* inspector)
    {
        if (fileBrowser) {
            fileBrowser->setInspector(inspector);
        }
    }
    void disableInspector()
    {
        if (fileBrowser) {
            fileBrowser->disableInspector();
        }
    }
    void enableInspector()
    {
        if (fileBrowser) {
            fileBrowser->enableInspector();
        }
    }

    // filterpanel interface
    void exifFilterChanged () override;

    // exportpanel interface
    void exportRequested() override;

    Glib::ustring lastSelectedDir ()
    {
        return selectedDirectory;
    }
    void setEnabled (bool e);   // if not enabled, it does not open image
    void enableTabMode(bool enable);  // sets progress bar

    // accessors for FileBrowser
    void redrawAll ();
    void refreshThumbImages ();
    void refreshHeight ();

    void filterApplied() override;
    void openRequested(const std::vector<Thumbnail*>& tbe) override;
    void deleteRequested(const std::vector<FileBrowserEntry*>& tbe, bool inclBatchProcessed, bool onlySelected) override;
    void copyMoveRequested(const std::vector<FileBrowserEntry*>& tbe, bool moveRequested) override;
    void developRequested(const std::vector<FileBrowserEntry*>& tbe, bool fastmode) override;
    void renameRequested(const std::vector<FileBrowserEntry*>& tbe) override;
    void selectionChanged(const std::vector<Thumbnail*>& tbe) override;
    void clearFromCacheRequested(const std::vector<FileBrowserEntry*>& tbe, bool leavenotrace) override;
    bool isInTabMode() const override;

    void emptyTrash ();
    bool trashIsEmpty ();

    void setFileSelectionListener (FileSelectionListener* l)
    {
        listener = l;
    }
    void setFileSelectionChangeListener (FileSelectionChangeListener* l)
    {
        fslistener = l;
    }
    void setImageAreaToolListener (ImageAreaToolListener* l)
    {
        iatlistener = l;
    }
    void setDirSelector (const DirSelectionSlot& selectDir);

    void setFilterPanel (FilterPanel* fpanel);
    void setExportPanel (ExportPanel* expanel);
    void exifInfoButtonToggled();
    void categoryButtonToggled (Gtk::ToggleButton* b, bool isMouseClick);
    void showRecursiveToggled();
    bool capture_event(GdkEventButton* event);
    void filterChanged ();

    void saveResetState ();
    bool restoreResetState ();

    void on_realize() override;
    void _openImage (const std::vector<Thumbnail*>& tmb);

    void zoomIn ();
    void zoomOut ();

    void buttonBrowsePathPressed ();
    bool BrowsePath_key_pressed (GdkEventKey *event);
    void buttonQueryClearPressed ();
    void executeQuery ();
    bool Query_key_pressed(GdkEventKey *event);
    void updateFBQueryTB (bool singleRow);
    void updateFBToolBarVisibility (bool showFilmStripToolBar);

    void tbLeftPanel_1_toggled ();
    void tbLeftPanel_1_visible (bool visible);
    void tbRightPanel_1_toggled ();
    void tbRightPanel_1_visible (bool visible);

    void openNextImage ()
    {
        fileBrowser->openNextImage();
    }
    void openPrevImage ()
    {
        fileBrowser->openPrevImage();
    }
    void selectImage (Glib::ustring fname, bool clearFilters);
    void openNextPreviousEditorImage (Glib::ustring fname, eRTNav nextPrevious);

    bool handleShortcutKey (GdkEventKey* event);
    bool handleShortcutKeyRelease(GdkEventKey *event);

    bool CheckSidePanelsVisibility();
    void toggleSidePanels();
    void toggleLeftPanel();
    void toggleRightPanel();

    void showToolBar();
    void hideToolBar();

    void on_dir_changed (const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& other_file, Gio::FileMonitorEvent event_type);

    bool timerEvents();
};

inline void FileCatalog::setDirSelector (const FileCatalog::DirSelectionSlot& selectDir)
{
    this->selectDir = selectDir;
}
