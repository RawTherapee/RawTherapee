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

#include <vector>

#include <gtkmm.h>

#include "dynamicprofilepanel.h"
#include "options.h"
#include "rtengine/profilestore.h"

class ExternalEditorPreferences;
class RTWindow;
class Splash;
class ToolLocationPreference;

class Preferences final :
    public Gtk::Dialog,
    public ProfileStoreListener
{

    class ExtensionColumns :
        public Gtk::TreeModel::ColumnRecord
    {
    public:
        Gtk::TreeModelColumn<bool>  enabled;
        Gtk::TreeModelColumn<Glib::ustring>  ext;
        ExtensionColumns()
        {
            add (enabled);
            add (ext);
        }
    };
    ExtensionColumns extensionColumns;
    Glib::RefPtr<Gtk::ListStore> extensionModel;


    class BehavColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Gtk::TreeModelColumn<Glib::ustring> label;
        Gtk::TreeModelColumn<bool>          badd;
        Gtk::TreeModelColumn<bool>          bset;
        Gtk::TreeModelColumn<bool>          visible;
        Gtk::TreeModelColumn<int>           addsetid;
        BehavColumns()
        {
            add (label);
            add (badd);
            add (bset);
            add (visible);
            add (addsetid);
        }
    };

    Glib::RefPtr<Gtk::TreeStore> behModel;
    BehavColumns behavColumns;
    std::vector<Glib::ustring> themeNames;
    Glib::RefPtr<Glib::Regex> regex;
    Glib::MatchInfo matchInfo;
    Splash* splash;
    ProfileStoreComboBox* rprofiles;
    Gtk::TreeIter currRawRow; // :)
    ProfileStoreComboBox* iprofiles;
    Gtk::TreeIter currImgRow;
    Gtk::ComboBoxText* languages;
    Gtk::CheckButton* ckbLangAutoDetect;
    Gtk::Entry* dateformat;
    MyFileChooserEntry* startupdir;
    Gtk::RadioButton* sdcurrent;
    Gtk::RadioButton* sdlast;
    Gtk::RadioButton* sdhome;
    Gtk::RadioButton* sdother;
    MyFileChooserButton* gimpDir;
    MyFileChooserButton* psDir;
    Gtk::Entry* editorToSendTo;
    Gtk::RadioButton* edGimp;
    Gtk::RadioButton* edPS;
    Gtk::RadioButton* edOther;
    ExternalEditorPreferences *externalEditors;

    Gtk::RadioButton *editor_dir_temp;
    Gtk::RadioButton *editor_dir_current;
    Gtk::RadioButton *editor_dir_custom;
    MyFileChooserButton *editor_dir_custom_path;
    Gtk::CheckButton *editor_float32;
    Gtk::CheckButton *editor_bypass_output_profile;

    MyFileChooserButton* darkFrameDir;
    MyFileChooserButton* flatFieldDir;
    MyFileChooserButton* clutsDir;
    MyFileChooserButton* cameraProfilesDir;
    MyFileChooserButton* lensProfilesDir;
    MyFileChooserEntry* lensfunDbDir;
    Gtk::Label *dfLabel;
    Gtk::Label *ffLabel;

    Gtk::CheckButton* showDateTime;
    Gtk::CheckButton* showBasicExif;
    Gtk::CheckButton* showExpComp;

    MyFileChooserButton* iccDir;
    Gtk::ComboBoxText* prtProfile;
    Gtk::ComboBoxText* prtIntent;
    Gtk::CheckButton* prtBPC;
    Gtk::ComboBoxText* monProfile;
    Gtk::ComboBoxText* monIntent;
    Gtk::CheckButton* mcie;
    Gtk::CheckButton* monBPC;
    Gtk::CheckButton* cbAutoMonProfile;
    //Gtk::CheckButton* cbAutocielab;
    Gtk::CheckButton* cbdaubech;
    Gtk::SpinButton*  hlThresh;
    Gtk::SpinButton*  shThresh;
    Gtk::CheckButton* mwbacorr;
 //   Gtk::CheckButton* mwbaforc;
 //   Gtk::CheckButton* mwbanopurp;
    Gtk::CheckButton* mwbaena;
//    Gtk::CheckButton* mwbaenacustom;

//    Gtk::CheckButton* mwbasort;
//    Gtk::SpinButton*  wbacorrnb;
//    Gtk::SpinButton*  wbaprecis;
//    Gtk::SpinButton*  wbasizeref;
//    Gtk::SpinButton*  wbagreendelta;

    Gtk::SpinButton*  panFactor;
    Gtk::CheckButton* rememberZoomPanCheckbutton;

//   Gtk::ComboBoxText* view;
//    Gtk::ComboBoxText* grey;
//    Gtk::ComboBoxText* greySc;
    Gtk::ComboBoxText* dnv;
    Gtk::ComboBoxText* dnti;
    Gtk::ComboBoxText* dnaut;
    Gtk::ComboBoxText* dnautsimpl;
    Gtk::ComboBoxText* dnwavlev;
    Gtk::ComboBoxText* dnliss;

    Gtk::Frame* waveletFrame;
    Gtk::Box* waveletTileSizeHBox;
    Gtk::Label* waveletTileSizeLabel;
    Gtk::ComboBoxText* waveletTileSizeCombo;

    Gtk::ComboBoxText* cprevdemo;
    Gtk::CheckButton* ctiffserialize;
    Gtk::ComboBoxText* curveBBoxPosC;
    Gtk::ComboBoxText* curveBBoxPosS;

    Gtk::ComboBoxText* complexitylocal;
    Gtk::ComboBoxText* spotlocal;

    Gtk::CheckButton* inspectorWindowCB;
    Gtk::CheckButton* zoomOnScrollCB;

    Gtk::ComboBoxText* themeCBT;
    Gtk::FontButton* mainFontFB;
    Gtk::FontButton* colorPickerFontFB;
    Gtk::ColorButton* cropMaskColorCB;
    Gtk::ColorButton* navGuideColorCB;

    Gtk::SpinButton*   maxRecentFolders;
    Gtk::SpinButton*   maxThumbHeightSB;
    Gtk::SpinButton*   maxCacheEntriesSB;
    Gtk::Entry*     extension;
    Gtk::TreeView*  extensions;
    Gtk::Button*    addExt;
    Gtk::Button*    delExt;
    Gtk::Button*    moveExtUp;
    Gtk::Button*    moveExtDown;
    Gtk::CheckButton* overlayedFileNames;
    Gtk::CheckButton* filmStripOverlayedFileNames;
    Gtk::CheckButton* sameThumbSize;
    Gtk::SpinButton* browseRecursiveDepth;
    Gtk::SpinButton* browseRecursiveMaxDirs;
    Gtk::CheckButton* browseRecursiveFollowLinks{nullptr};
    Gtk::SpinButton* newFileDelayTime;

    Gtk::SpinButton*  threadsSpinBtn;
    Gtk::SpinButton*  clutCacheSizeSB;
    Gtk::CheckButton* measureCB;
    Gtk::SpinButton*  chunkSizeAMSB;
    Gtk::SpinButton*  chunkSizeCASB;
    Gtk::SpinButton*  chunkSizeRCDSB;
    Gtk::SpinButton*  chunkSizeRGBSB;
    Gtk::SpinButton*  chunkSizeXTSB;
    Gtk::SpinButton*  maxInspectorBuffersSB;
    Gtk::ComboBoxText *thumbnailInspectorMode;

    Gtk::CheckButton* ckbmenuGroupRank;
    Gtk::CheckButton* ckbmenuGroupLabel;
    Gtk::CheckButton* ckbmenuGroupFileOperations;
    Gtk::CheckButton* ckbmenuGroupProfileOperations;
    Gtk::CheckButton* ckbmenuGroupExtProg;

    Gtk::Button*      behAddAll;
    Gtk::Button*      behSetAll;
    Gtk::CheckButton* chOverwriteOutputFile;

    Gtk::ComboBoxText* saveParamsPreference;
    Gtk::CheckButton* useBundledProfiles;
    Gtk::ComboBoxText* loadParamsPreference;
    Gtk::ComboBoxText* editorLayout;
    RTWindow* parent;

    Gtk::CheckButton* ckbSndEnable;
    Gtk::Entry* txtSndBatchQueueDone;
    Gtk::Entry* txtSndLngEditProcDone;
    Gtk::SpinButton* spbSndLngEditProcDoneSecs;

    Gtk::CheckButton* ckbInternalThumbIfUntouched;

    Gtk::CheckButton *thumbnailRankColorMode;

    Gtk::Entry* txtCustProfBuilderPath;
    Gtk::ComboBoxText* custProfBuilderLabelType;

    Gtk::CheckButton* ckbHistogramPositionLeft;
    Gtk::CheckButton* ckbFileBrowserToolbarSingleRow;
    Gtk::CheckButton* ckbShowFilmStripToolBar;
    Gtk::CheckButton* ckbHideTPVScrollbar;
    Gtk::CheckButton* ckbshowtooltiplocallab;

    Gtk::CheckButton* ckbAutoSaveTpOpen;
    Gtk::Button* btnSaveTpOpenNow;

    DynamicProfilePanel *dynProfilePanel;

    Gtk::ComboBoxText *cropGuidesCombo;
    Gtk::CheckButton *cropAutoFitCB;

    Gtk::CheckButton *enableLibRaw;

    Gtk::ComboBoxText *maxZoomCombo;

    Gtk::ComboBoxText *metadataSyncCombo;
    Gtk::ComboBoxText *xmpSidecarCombo;

    Glib::ustring storedValueRaw;
    Glib::ustring storedValueImg;

    Options moptions;
    sigc::connection tconn, sconn, fconn, cpfconn, addc, setc, dfconn, ffconn, bpconn, rpconn, ipconn;
    sigc::connection autoMonProfileConn, sndEnableConn, langAutoDetectConn, autocielabConn, observer10Conn;
    Glib::ustring initialTheme;
    Glib::ustring initialFontFamily;
    int initialFontSize;
    bool newFont;
    bool newCPFont;

    ToolLocationPreference *toolLocationPreference;

    void fillPreferences ();
    void storePreferences ();
    void parseDir       (Glib::ustring dirname, std::vector<Glib::ustring>& items, Glib::ustring ext);
    void parseThemeDir  (Glib::ustring dirname);
    void updateDFinfos ();
    void updateFFinfos ();
    void workflowUpdate();
    void themeChanged  ();
    void fontChanged   ();
    void cpFontChanged ();
    void forRAWComboChanged ();
    void forImageComboChanged ();
    void layoutComboChanged ();
    void bundledProfilesChanged ();
    void iccDirChanged ();
    void switchThemeTo (Glib::ustring newTheme);
    void switchFontTo  (const Glib::ustring &newFontFamily, const int newFontSize);
    bool splashClosed (GdkEventAny* event);

    int getThemeRowNumber (const Glib::ustring& name);

    void appendBehavList (Gtk::TreeModel::iterator& parent, Glib::ustring label, int id, bool set);

    Gtk::ScrolledWindow *swGeneral;
    Gtk::ScrolledWindow *swImageProcessing;
    Gtk::ScrolledWindow *swFavorites;
    Gtk::ScrolledWindow *swDynamicProfile;
    Gtk::ScrolledWindow *swFileBrowser;
    Gtk::ScrolledWindow *swColorMan;
    Gtk::ScrolledWindow *swBatchProc;
    Gtk::ScrolledWindow *swPerformance;
    Gtk::ScrolledWindow *swSounds;

    Gtk::Widget *getGeneralPanel();
    Gtk::Widget *getImageProcessingPanel();
    Gtk::Widget *getFavoritesPanel();
    Gtk::Widget *getDynamicProfilePanel();
    Gtk::Widget *getFileBrowserPanel();
    Gtk::Widget *getColorManPanel();
    Gtk::Widget *getBatchProcPanel();
    Gtk::Widget *getPerformancePanel();
    Gtk::Widget *getSoundsPanel();

public:
    explicit Preferences (RTWindow *rtwindow);
    ~Preferences () override;

    void savePressed ();
    void loadPressed ();
    void okPressed ();
    void cancelPressed ();
    void aboutPressed ();
    void autoMonProfileToggled ();
    void sndEnableToggled ();
    void langAutoDetectToggled ();
    void autocielabToggled ();
    void observer10Toggled ();
    void onResetToDefaultClicked ();

    void selectStartupDir ();
    void extensionsChanged ();
    void extensionChanged ();
    void addExtPressed ();
    void delExtPressed ();
    void moveExtUpPressed ();
    void moveExtDownPressed ();
    void darkFrameChanged ();
    void flatFieldChanged ();
    void clearProfilesPressed ();
    void clearThumbImagesPressed ();
    void clearAllPressed ();

    void behAddSetRadioToggled (const Glib::ustring& path, bool add);
    void behAddRadioToggled (const Glib::ustring& path);
    void behSetRadioToggled (const Glib::ustring& path);
    void behAddSetAllPressed (bool add);
    void behAddAllPressed ();
    void behSetAllPressed ();

    void storeCurrentValue() override;
    void updateProfileList() override;
    void restoreValue() override;

//    void selectICCProfileDir ();
//    void selectMonitorProfile ();
};
