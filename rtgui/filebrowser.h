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

#include "browserfilter.h"
#include "exportpanel.h"
#include "extprog.h"
#include "filebrowserentry.h"
#include "pparamschangelistener.h"
#include "thumbbrowserbase.h"
#include "widgets/basic/lwbutton.h"
#include "windows/partialpastedlg.h"

#include "rtengine/noncopyable.h"
#include "rtengine/profilestore.h"

#include <gtkmm.h>

#include <map>

class FileBrowser;
class FileBrowserEntry;
class ProfileStoreLabel;

class FileBrowserListener
{
public:
    virtual ~FileBrowserListener() = default;
    virtual void filterApplied() = 0;
    virtual void openRequested(const std::vector<Thumbnail*>& tbe) = 0;
    virtual void developRequested(const std::vector<FileBrowserEntry*>& tbe, bool fastmode) = 0;
    virtual void renameRequested(const std::vector<FileBrowserEntry*>& tbe) = 0;
    virtual void deleteRequested(const std::vector<FileBrowserEntry*>& tbe, bool inclBatchProcessed, bool onlySelected) = 0;
    virtual void copyMoveRequested(const std::vector<FileBrowserEntry*>& tbe, bool moveRequested) = 0;
    virtual void selectionChanged(const std::vector<Thumbnail*>& tbe) = 0;
    virtual void clearFromCacheRequested(const std::vector<FileBrowserEntry*>& tbe, bool leavenotrace) = 0;
    virtual bool isInTabMode() const = 0;
};

/*
 * Class handling actions common to all thumbnails of the file browser
 */
class FileBrowser final : public ThumbBrowserBase,
    public LWButtonListener,
    public ExportPanelListener,
    public ProfileStoreListener,
    public rtengine::NonCopyable
{
private:
    typedef sigc::signal<void> type_trash_changed;

    using ThumbBrowserBase::redrawNeeded;

    IdleRegister idle_register;
    unsigned int session_id_;

protected:
    Gtk::MenuItem* rank[6];
    MyImageMenuItem* colorlabel[6];
    Gtk::MenuItem* trash;
    Gtk::MenuItem* untrash;
    Gtk::MenuItem* develop;
    Gtk::MenuItem* developfast;
    Gtk::MenuItem* rename;
    Gtk::MenuItem* remove;
    Gtk::MenuItem* removeInclProc;
    Gtk::MenuItem* open;
    Gtk::MenuItem* inspect;
    Gtk::MenuItem* selall;
    Gtk::RadioMenuItem* sortMethod[Options::SORT_METHOD_COUNT];
    Gtk::RadioMenuItem* sortOrder[2];
    Gtk::MenuItem* copyTo;
    Gtk::MenuItem* moveTo;

    Gtk::MenuItem* menuSort;
    Gtk::MenuItem* menuRank;
    Gtk::MenuItem* menuLabel;
    Gtk::MenuItem* menuFileOperations;
    Gtk::MenuItem* menuProfileOperations;
    Gtk::MenuItem* menuExtProg;
    Gtk::MenuItem** amiExtProg;
    Gtk::MenuItem* miOpenDefaultViewer;
    std::map<Glib::ustring, const ExtProgAction*> mMenuExtProgs;  // key is menuitem label

    Gtk::MenuItem* menuDF;
    Gtk::MenuItem* selectDF;
    Gtk::MenuItem* thisIsDF;
    Gtk::MenuItem* autoDF;

    Gtk::MenuItem* menuFF;
    Gtk::MenuItem* selectFF;
    Gtk::MenuItem* thisIsFF;
    Gtk::MenuItem* autoFF;

    Gtk::MenuItem* copyprof;
    Gtk::MenuItem* pasteprof;
    Gtk::MenuItem* partpasteprof;
    Gtk::MenuItem* applyprof;
    Gtk::MenuItem* applypartprof;
    Gtk::MenuItem* resetdefaultprof;
    Gtk::MenuItem* clearprof;
    Gtk::MenuItem* cachemenu;
    Gtk::MenuItem* clearFromCache;
    Gtk::MenuItem* clearFromCacheFull;
    Gtk::Menu* pmenu;

    MyImageMenuItem* colorlabel_pop[6];
    Gtk::Menu* pmenuColorLabels;
    void* colorLabel_actionData;
    void menuColorlabelActivated (Gtk::MenuItem* m); // use only when menu is invoked via FileBrowser::buttonPressed to pass actionData

    Glib::RefPtr<Gtk::AccelGroup> pmaccelgroup;

    BatchPParamsChangeListener* bppcl;
    FileBrowserListener* tbl;
    BrowserFilter filter;
    int numFiltered;

    void toTrashRequested   (std::vector<FileBrowserEntry*> tbe);
    void fromTrashRequested (std::vector<FileBrowserEntry*> tbe);
    void sortMethodRequested (int method);
    void sortOrderRequested (int order);
    void rankingRequested   (std::vector<FileBrowserEntry*> tbe, int rank);
    void colorlabelRequested   (std::vector<FileBrowserEntry*> tbe, int colorlabel);
    void requestRanking (int rank);
    void requestColorLabel(int colorlabel);
    void notifySelectionListener ();
    void openRequested( std::vector<FileBrowserEntry*> mselected);
    void inspectRequested( std::vector<FileBrowserEntry*> mselected);
    ExportPanel* exportPanel;

    type_trash_changed m_trash_changed;

public:
    FileBrowser ();
    ~FileBrowser () override;

    void addEntry (FileBrowserEntry* entry);                    ///< Moves the execution to the main UI thread
    void addEntry_ (FileBrowserEntry* entry);                   ///< Must be called only when the execution is in the main UI thread
    FileBrowserEntry*  delEntry (const Glib::ustring& fname);    // return the entry if found here return NULL otherwise
    void close ();

    unsigned int session_id() const { return session_id_; }

    void setBatchPParamsChangeListener (BatchPParamsChangeListener* l)
    {
        bppcl = l;
    }
    void setFileBrowserListener (FileBrowserListener* l)
    {
        tbl = l;
    }

    void menuItemActivated (Gtk::MenuItem* m);
    void applyMenuItemActivated (ProfileStoreLabel *label);
    void applyPartialMenuItemActivated (ProfileStoreLabel *label);

    void applyFilter (const BrowserFilter& filter);
    int getNumFiltered()
    {
        return numFiltered;
    }

    void buttonPressed (LWButton* button, int actionCode, void* actionData, int x, int y) override;
    void redrawNeeded  (LWButton* button) override;
    bool checkFilter (ThumbBrowserEntryBase* entry) const override;
    void rightClicked (int x, int y) override;
    void doubleClicked (ThumbBrowserEntryBase* entry) override;
    bool keyPressed (GdkEventKey* event) override;

    void saveThumbnailHeight (int height) override;
    int  getThumbnailHeight () override;


    void enableTabMode(bool enable);
    bool isInTabMode() override
    {
        return tbl ? tbl->isInTabMode() : false;
    }

    void openNextImage();
    void openPrevImage();
    void selectImage(const Glib::ustring& fname, bool doScroll = true);

    void copyProfile ();
    void pasteProfile ();
    void partPasteProfile ();
    void openNextPreviousEditorImage(const Glib::ustring& fname, eRTNav eNextPrevious);

#ifdef _WIN32
    void openDefaultViewer (int destination);
#endif

    void thumbRearrangementNeeded () override;

    void selectionChanged () override;

    void setExportPanel (ExportPanel* expanel);
    // exportpanel interface
    void exportRequested() override;

    void storeCurrentValue() override;
    void updateProfileList() override;
    void restoreValue() override;

    type_trash_changed trash_changed();
};
