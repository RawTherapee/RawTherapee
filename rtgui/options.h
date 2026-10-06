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
#include <vector>
#if defined __has_include
#if __has_include(<gtkmm/enums.h>)
#include <gtkmm/enums.h>
#else
#include <gtkmm-3.0/gtkmm/enums.h>
#endif
#else
#include <gtkmm/enums.h>
#endif
#include "rtengine/rtapp.h"
#include "rtengine/settings.h"
#include <exception>

#define STARTUPDIR_CURRENT 0
#define STARTUPDIR_HOME    1
#define STARTUPDIR_CUSTOM  2
#define STARTUPDIR_LAST    3

#define THEMEREGEXSTR      "^(.+)\\.css$"

// Default bundled profile name to use for Raw images
#ifdef _WIN32
#define DEFPROFILE_RAW      "${G}\\Auto-Matched Curve - ISO Low"
#else
#define DEFPROFILE_RAW      "${G}/Auto-Matched Curve - ISO Low"
#endif
// Default bundled profile name to use for Standard images
#define DEFPROFILE_IMG      "Neutral"
// Profile name to use for internal values' profile
#define DEFPROFILE_INTERNAL "Neutral"
// Special name for the Dynamic profile
#define DEFPROFILE_DYNAMIC  "Dynamic"

struct ExternalEditor {
    ExternalEditor();
    ExternalEditor(const Glib::ustring &name, const Glib::ustring &command, bool native_command, const Glib::ustring &icon_serialized);
    Glib::ustring name;
    Glib::ustring command;
    bool native_command;
    Glib::ustring icon_serialized;

    bool operator==(const ExternalEditor & other) const;
    bool operator!=(const ExternalEditor & other) const;
};

struct SaveFormat {
    SaveFormat(
        const Glib::ustring& _format,
        int _png_bits,
        int _jpeg_quality,
        int _jpeg_sub_samp,
        int _tiff_bits,
        bool _tiff_float,
        bool _tiff_uncompressed,
        bool _big_tiff,
        bool _save_params
    ) :
        format(_format),
        pngBits(_png_bits),
        jpegQuality(_jpeg_quality),
        jpegSubSamp(_jpeg_sub_samp),
        tiffBits(_tiff_bits),
        tiffFloat(_tiff_float),
        tiffUncompressed(_tiff_uncompressed),
        bigTiff(_big_tiff),
        saveParams(_save_params)
    {
    }
    SaveFormat(
        const Glib::ustring& _format,
        int _png_bits,
        int _tiff_bits,
        bool _tiff_float
    ) :
        SaveFormat(
            _format,
            _png_bits,
            90,
            2,
            _tiff_bits,
            _tiff_float,
            true,
            false,
            true
        )
    {
    }
    SaveFormat() :
        SaveFormat("jpg", 8, 8, false)
    {
    }

    Glib::ustring format;
    int pngBits;
    int jpegQuality;
    int jpegSubSamp;  // 1=best compression, 3=best quality
    int tiffBits;
    bool tiffFloat;
    bool tiffUncompressed;
    bool bigTiff;
    bool saveParams;
};

enum ThFileType {FT_Invalid = -1, FT_None = 0, FT_Raw = 1, FT_Jpeg = 2, FT_Tiff = 3, FT_Png = 4, FT_Custom = 5, FT_Tiff16 = 6, FT_Png16 = 7, FT_Custom16 = 8};
enum PPLoadLocation {PLL_Cache = 0, PLL_Input = 1};
enum CPBKeyType {CPBKT_TID = 0, CPBKT_NAME = 1, CPBKT_TID_NAME = 2};
enum prevdemo_t {PD_Sidecar = 1, PD_Fast = 0};

namespace Glib
{

class KeyFile;

}

class Options
{
public:
    class Error: public std::exception
    {
    public:
        explicit Error (const Glib::ustring &msg): msg_ (msg) {}
        const char *what() const throw() override
        {
            return msg_.c_str();
        }
        const Glib::ustring &get_msg() const throw()
        {
            return msg_;
        }

    private:
        Glib::ustring msg_;
    };

private:
    enum class DefProfError : short {
        defProfRawMissing        = 1 << 0,
        bundledDefProfRawMissing = 1 << 1,
        defProfImgMissing        = 1 << 2,
        bundledDefProfImgMissing = 1 << 3
    };
    short defProfError;
    Glib::ustring userProfilePath;
    Glib::ustring globalProfilePath;
    bool checkProfilePath (Glib::ustring &path);
    bool checkDirPath (Glib::ustring &path, Glib::ustring errString);
    void updatePaths();
    int getString (const char* src, char* dst);
    void error (int line);
    /**
     * Safely reads a directory from the configuration file and only applies it
     * to the provided destination variable if there is a non-empty string in
     * the configuration.
     *
     * @param keyFile file to read configuration from
     * @param section name of the section in the configuration file
     * @param entryName name of the entry in the configuration file
     * @param destination destination variable to store to
     * @return @c true if @p destination was changed
     */
    bool safeDirGet (const Glib::KeyFile& keyFile, const Glib::ustring& section,
                     const Glib::ustring& entryName, Glib::ustring& destination);

public:
    enum class NavigatorUnit {
        PERCENT,
        R0_255,
        R0_1,
        _COUNT
    };

    enum class ScopeType {
        NONE = -1,
        HISTOGRAM,
        HISTOGRAM_RAW,
        PARADE,
        VECTORSCOPE_HC,
        VECTORSCOPE_HS,
        WAVEFORM
    };

    bool savesParamsAtExit;
    SaveFormat saveFormat, saveFormatBatch;
    Glib::ustring savePathTemplate;
    Glib::ustring savePathFolder;
    bool saveUsePathTemplate;
    Glib::ustring defProfRaw;
    Glib::ustring defProfImg;
    Glib::ustring dateFormat;
    int adjusterMinDelay;
    int adjusterMaxDelay;
    int  startupDir;
    Gtk::SortType dirBrowserSortType;
    Glib::ustring startupPath;
    Glib::ustring profilePath; // can be an absolute or relative path; depending on this value, bundled profiles may not be found
    bool useBundledProfiles;   // only used if multiUser == true
    Glib::ustring lastCopyMovePath;
    Glib::ustring loadSaveProfilePath;
    Glib::ustring lastSaveAsPath;
    int saveAsDialogWidth;
    int saveAsDialogHeight;
    int toolPanelWidth;
    int browserToolPanelWidth;
    int browserToolPanelHeight;
    bool browserToolPanelOpened;
    bool browserDirPanelOpened;
    bool editorFilmStripOpened;
    int historyPanelWidth;
    int windowX;
    int windowY;
    int windowWidth;
    int windowHeight;
    bool windowMaximized;
    int windowMonitor;
    int meowMonitor;
    bool meowMaximized;
    int meowWidth;
    int meowHeight;
    int meowX;
    int meowY;
    int detailWindowWidth;
    int detailWindowHeight;
    int dirBrowserWidth;
    int dirBrowserHeight;
    int preferencesWidth;
    int preferencesHeight;
    bool lastShowAllExif;
    int lastScale;
    int panAccelFactor;
    int lastCropSize;
    Glib::ustring fontFamily;    // RT's main font family
    int fontSize;                // RT's main font size (units: pt)
    Glib::ustring CPFontFamily;  // ColorPicker font family
    int CPFontSize;              // ColorPicker font size (units: pt)
    bool fbOnlyRaw;
    bool fbShowDateTime;
    bool fbShowBasicExif;
    bool fbShowExpComp;
    bool fbShowHidden;
    int  fbArrangement;
    NavigatorUnit navRGBUnit;
    NavigatorUnit navHSVUnit;
    bool multiUser;
    static Glib::ustring rtdir;
    Glib::ustring version;
    int thumbSize, thumbSizeTab, thumbSizeQueue;
    bool sameThumbSize;     // Will use only one thumb size for the file browser and the single editor tab, and avoid recomputing them
    bool showHistory;
    int showFilePanelState; // 0: normal, 1: maximized, 2: normal, 3: hidden
    bool showInfo;
    bool mainNBVertical;  // main notebook vertical tabs?
    bool showClippedHighlights;
    bool showClippedShadows;
    int highlightThreshold;
    int shadowThreshold;
    bool blinkClipped;
    int bgcolor;
    Glib::ustring language;
    bool languageAutoDetect;
    Glib::ustring theme;
    static Glib::ustring cacheBaseDir;
    bool autoSuffix;
    bool forceFormatOpts;
    int saveMethodNum;
    bool saveParamsFile;
    bool saveParamsCache;
    PPLoadLocation paramsLoadLocation;
    bool procQueueEnabled;
    Glib::ustring gimpDir;
    Glib::ustring psDir;
    Glib::ustring customEditorProg;
    std::vector<ExternalEditor> externalEditors;
    int externalEditorIndex;
    Glib::ustring CPBPath; // Custom Profile Builder's path
    CPBKeyType CPBKeys; // Custom Profile Builder's key type
    int editorToSendTo;
    enum EditorOutDir {
        EDITOR_OUT_DIR_TEMP,
        EDITOR_OUT_DIR_CURRENT,
        EDITOR_OUT_DIR_CUSTOM
    };
    EditorOutDir editor_out_dir; // output directory for "open in external editor"
    Glib::ustring editor_custom_out_dir;
    bool editor_float32;
    bool editor_bypass_output_profile;

    int maxThumbnailHeight;
    int maxThumbnailWidth;
    std::size_t maxCacheEntries;
    int thumbInterp; // 0: nearest, 1: bilinear

    std::vector<std::string> knownExtensions = {
        "3fr", "arw", "arq", "cr2",  "cr3", "crf", "crw",  "dcr", "dng",
        "fff", "iiq", "jpg", "jpeg", "jxl", "kdc", "mef",  "mos", "mrw",
        "nef", "nrw", "orf", "ori",  "pef", "png", "raf",  "raw", "rw2",
        "rwl", "rwz", "sr2", "srf",  "srw", "tif", "tiff", "x3f"};

    std::vector<Glib::ustring> parseExtensions;   // List containing all extensions type
    std::vector<int> parseExtensionsEnabled;      // List of bool to retain extension or not
    std::vector<Glib::ustring> parsedExtensions;  // List containing all retained extensions (lowercase)
    std::set<std::string> parsedExtensionsSet;  // Set containing all retained extensions (lowercase)
    bool browseRecursive;
    int browseRecursiveDepth;
    int browseRecursiveMaxDirs;
    bool browseRecursiveFollowLinks;
    std::vector<int> tpOpen;
    bool autoSaveTpOpen;
    //std::vector<int> crvOpen;
    std::vector<int> baBehav;
    rtengine::Settings rtSettings;
    bool showtooltip;
    std::vector<Glib::ustring> favoriteDirs;
    std::vector<Glib::ustring> renameTemplates;
    bool renameUseTemplates;
    bool internalThumbIfUntouched;
    bool overwriteOutputFile;
    int complexity;
    int spotmet;

    bool showInspectorObservedArea;
    bool inspectorWindow; // open inspector in separate window
    bool pinInspector;
    bool zoomOnScroll;    // translate scroll events to zoom
    bool reverseDiscreteScrollDir;
    bool reverseSmoothScrollDir;
    int smoothScrollZoomSensitivity;
    int smoothScrollPanSensitivity;
    static constexpr int SMOOTH_SCROLL_ZOOM_SENSITIVITY_MIN = -100;
    static constexpr int SMOOTH_SCROLL_ZOOM_SENSITIVITY_MAX = 100;
    static constexpr int SMOOTH_SCROLL_ZOOM_SENSITIVITY_DEFAULT = 0;
    static constexpr int SMOOTH_SCROLL_PAN_SENSITIVITY_MIN = -100;
    static constexpr int SMOOTH_SCROLL_PAN_SENSITIVITY_MAX = 100;
    static constexpr int SMOOTH_SCROLL_PAN_SENSITIVITY_DEFAULT = 0;

    enum class Zoom11Mode { BASIC, CENTER_CURSOR, PRESERVE_CURSOR };
    Zoom11Mode zoom11Mode;

    std::vector<double> thumbnailZoomRatios;
    bool overlayedFileNames;
    bool filmStripOverlayedFileNames;
    bool showFileNames;
    bool filmStripShowFileNames;
    bool tabbedUI;
    bool rememberZoomAndPan;
    int multiDisplayMode;  // 0=none, 1=Edit panels on other display
    std::vector<double> cutOverlayBrush;  // Red;Green;Blue;Alpha , all ranging 0..1
    std::vector<double> navGuideBrush;  // Red;Green;Blue;Alpha , all ranging 0..1

    Glib::ustring sndBatchQueueDone;
    Glib::ustring sndLngEditProcDone;
    double sndLngEditProcDoneSecs;  // Minimum processing time seconds till the sound is played
    bool sndEnable;

    int histogramPosition;  // 0=disabled, 1=left pane, 2=right pane
    bool histogramRed, histogramGreen, histogramBlue;
    bool histogramLuma, histogramChroma;
    bool histogramBar;
    int histogramHeight;
    int histogramDrawMode;
    ScopeType histogramScopeType;
    bool histogramShowOptionButtons;
    float histogramTraceBrightness;
    bool FileBrowserToolbarSingleRow;
    bool hideTPVScrollbar;
    int whiteBalanceSpotSize;
    int curvebboxpos; // 0=above, 1=right, 2=below, 3=left

    bool showFilmStripToolBar;

    // cropping options
    int cropPPI;
    enum CropGuidesMode { CROP_GUIDE_NONE, CROP_GUIDE_FRAME, CROP_GUIDE_FULL };
    CropGuidesMode cropGuides;
    bool cropAutoFit;

    // Other options

    // Maximum zoom
    enum class MaxZoom: int {
      PERCENTS_100 = 0,
      PERCENTS_200,
      PERCENTS_300,
      PERCENTS_400,
      PERCENTS_500,
      PERCENTS_600,
      PERCENTS_700,
      PERCENTS_800,
      PERCENTS_1600,
    };
    MaxZoom maxZoomLimit;

    // Performance options
    Glib::ustring clutsDir;
    int rgbDenoiseThreadLimit; // maximum number of threads for the denoising tool ; 0 = use the maximum available
    int maxInspectorBuffers;   // maximum number of buffers (i.e. images) for the Inspector feature
    int inspectorDelay;
    int clutCacheSize;
    bool filledProfile;  // Used as reminder for the ProfilePanel "mode"
    prevdemo_t prevdemo; // Demosaicing method used for the <100% preview
    bool serializeTiffRead;
    bool measure;
    size_t chunkSizeAMAZE;
    size_t chunkSizeCA;
    size_t chunkSizeRCD;
    size_t chunkSizeRGB;
    size_t chunkSizeXT;
    bool menuGroupRank;
    bool menuGroupLabel;
    bool menuGroupFileOperations;
    bool menuGroupProfileOperations;
    bool menuGroupExtProg;

    // ICC Profile Creator
    Glib::ustring ICCPC_primariesPreset;
    double ICCPC_redPrimaryX;
    double ICCPC_redPrimaryY;
    double ICCPC_greenPrimaryX;
    double ICCPC_greenPrimaryY;
    double ICCPC_bluePrimaryX;
    double ICCPC_bluePrimaryY;
    Glib::ustring ICCPC_gammaPreset;
    double ICCPC_gamma;
    double ICCPC_slope;
    Glib::ustring ICCPC_profileVersion;
    Glib::ustring ICCPC_illuminant;
    Glib::ustring ICCPC_description;
    Glib::ustring ICCPC_copyright;
    bool ICCPC_appendParamsToDesc;

    // fast export options
    bool fastexport_bypass_sharpening;
    bool fastexport_bypass_sharpenEdge;
    bool fastexport_bypass_sharpenMicro;
    //bool fastexport_bypass_lumaDenoise;
    //bool fastexport_bypass_colorDenoise;
    bool fastexport_bypass_defringe;
    bool fastexport_bypass_dirpyrDenoise;
    bool fastexport_bypass_dirpyrequalizer;
    bool fastexport_bypass_wavelet;
    Glib::ustring fastexport_raw_bayer_method;
    //bool fastexport_bypass_raw_bayer_all_enhance;
    bool fastexport_bypass_raw_bayer_dcb_iterations;
    bool fastexport_bypass_raw_bayer_dcb_enhance;
    bool fastexport_bypass_raw_bayer_lmmse_iterations;
    bool fastexport_bypass_raw_bayer_linenoise;
    bool fastexport_bypass_raw_bayer_greenthresh;
    Glib::ustring fastexport_raw_xtrans_method;
    bool fastexport_bypass_raw_ccSteps;
    bool fastexport_bypass_raw_ca;
    bool fastexport_bypass_raw_df;
    bool fastexport_bypass_raw_ff;
    Glib::ustring fastexport_icm_input_profile;
    Glib::ustring fastexport_icm_working_profile;
    Glib::ustring fastexport_icm_output_profile;
    int fastexport_icm_outputIntent;
    bool          fastexport_icm_outputBPC;
    Glib::ustring fastexport_icm_custom_output_profile;
    bool          fastexport_resize_enabled;
    double        fastexport_resize_scale;
    Glib::ustring fastexport_resize_appliesTo;
    Glib::ustring fastexport_resize_method;
    int           fastexport_resize_dataspec;
    int           fastexport_resize_width;
    int           fastexport_resize_height;
    int           fastexport_resize_longedge;
    int           fastexport_resize_shortedge;
    bool fastexport_use_fast_pipeline;

    std::vector<Glib::ustring> favorites;
    bool cloneFavoriteTools;
    // Dialog settings
    Glib::ustring lastIccDir;
    Glib::ustring lastDarkframeDir;
    Glib::ustring lastFlatfieldDir;
    Glib::ustring lastCameraProfilesDir;
    Glib::ustring lastLensProfilesDir;
    Glib::ustring lastRgbCurvesDir;
    Glib::ustring lastLabCurvesDir;
    Glib::ustring lastRetinexDir;
    Glib::ustring lastDenoiseCurvesDir;
    Glib::ustring lastWaveletCurvesDir;
    Glib::ustring lastIcmCurvesDir;
    Glib::ustring lastlocalCurvesDir;
    Glib::ustring lastlocalCurvesDirghs;
    Glib::ustring lastPFCurvesDir;
    Glib::ustring lastHsvCurvesDir;
    Glib::ustring lastToneCurvesDir;
    Glib::ustring lastColorToningCurvesDir;
    Glib::ustring lastVibranceCurvesDir;
    Glib::ustring lastProfilingReferenceDir;
    Glib::ustring lastBWCurvesDir;
    Glib::ustring lastLensProfileDir;
    Glib::ustring lastICCProfCreatorDir;
    bool gimpPluginShowInfoDialog;

    size_t maxRecentFolders;                   // max. number of recent folders stored in options file
    std::vector<Glib::ustring> recentFolders;  // List containing all recent folders

    enum class ThumbnailPropertyMode {
        PROCPARAMS, // store rank and color in procparams sidecars
        XMP // store rank and color xmp sidecar
    };
    ThumbnailPropertyMode thumbnailRankColorMode;

    enum SortMethod {
        SORT_BY_NAME,
        SORT_BY_DATE,
        SORT_BY_EXIF,
        SORT_BY_RANK,
        SORT_BY_LABEL,
        SORT_METHOD_COUNT,
    };
    SortMethod sortMethod; // remembers current state of file browser
    bool sortDescending;


    Options ();

    Options* copyFrom        (Options* other);
    void filterOutParsedExtensions ();
    void setDefaults     ();
    void readFromFile (Glib::ustring fname);
    void saveToFile (Glib::ustring fname);
    static void load (bool lightweight = false);
    static void save();

    // if multiUser=false, send back the global profile path
    Glib::ustring getPreferredProfilePath() const;
    Glib::ustring getUserProfilePath() const;
    Glib::ustring getGlobalProfilePath() const;
    Glib::ustring findProfilePath (Glib::ustring &profName) const;
    bool is_parse_extention (Glib::ustring fname) const;
    bool has_retained_extention (const Glib::ustring& fname) const;
    bool is_new_version() const;
    bool is_extention_enabled (const Glib::ustring& ext) const;
    bool is_defProfRawMissing() const;
    bool is_bundledDefProfRawMissing() const;
    bool is_defProfImgMissing() const;
    bool is_bundledDefProfImgMissing() const;
    void setDefProfRawMissing (bool value);
    void setBundledDefProfRawMissing (bool value);
    void setDefProfImgMissing (bool value);
    void setBundledDefProfImgMissing (bool value);
    static Glib::ustring getICCProfileCopyright();
};
