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
#include "options.h"
#include <cstdio>
#include <glib/gstdio.h>
#include <glibmm/date.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <glibmm/regex.h>
#include <iostream>
#include <sstream>
#include "multilangmgr.h"
#include "addsetids.h"
#include "guiutils.h"
#include "pathutils.h"
#include "version.h"

#include "rtengine/procparams.h"
#include "rtengine/rtapp.h"
#include "rtengine/rtengine.h"
#include "rtengine/utils.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include "rtengine/leanwindows.h"
#include <shlobj.h>
#endif

namespace
{

/**
 * Removes invalid entries from the extensions and enabled vectors and adds and
 * enables missing known extensions.
 *
 * @param extensions Extensions to tidy.
 * @param enabled Enabled flags corresponding to the extensions.
 * @param knownExtensions List of known extensions.
 */
void tidyParseExtensions(
    std::vector<Glib::ustring> &extensions,
    std::vector<int> &enabled,
    const std::vector<std::string> &knownExtensions)
{
    // Remove excess (invalid) entries.
    const auto extensionCount = std::min(extensions.size(), enabled.size());
    extensions.resize(extensionCount);
    enabled.resize(extensionCount);

    // Gather existing extensions.
    const std::set<Glib::ustring> existingExtensions(
            extensions.cbegin(), extensions.cend());

    // Append missing extensions and enable them.
    for (const auto & ext : knownExtensions) {
        if (existingExtensions.find(ext) == existingExtensions.cend()) {
            extensions.emplace_back(ext);
            enabled.emplace_back(1);
        }
    }
}

} // namespace

// User's settings directory, including images' profiles if used
Glib::ustring Options::rtdir;
// User's cached data directory
Glib::ustring Options::cacheBaseDir;

Options::Options()
{

    defProfError = 0;
    setDefaults();
}

const char *DefaultLanguage = "English (US)";

inline bool Options::checkProfilePath(Glib::ustring &path)
{
    if (path.empty()) {
        return false;
    }

    Glib::ustring p = getUserProfilePath();

    if (!p.empty() && Glib::file_test(path + App::PARAM_FILE_EXTENSION, Glib::FILE_TEST_EXISTS)) {
        return true;
    }

    p = getGlobalProfilePath();

    return !p.empty() && Glib::file_test(path + App::PARAM_FILE_EXTENSION, Glib::FILE_TEST_EXISTS);
}

bool Options::checkDirPath(Glib::ustring &path, Glib::ustring errString)
{
    if (Glib::file_test(path, Glib::FILE_TEST_EXISTS) && Glib::file_test(path, Glib::FILE_TEST_IS_DIR)) {
        return true;
    } else {
        if (!errString.empty()) {
            std::cerr << errString << std::endl;
        }

        return false;
    }
}

void Options::updatePaths()
{

    Glib::ustring tmpPath;

    userProfilePath = "";
    globalProfilePath = "";

    if (Glib::path_is_absolute(profilePath)) {
        // absolute path
        if (!checkDirPath(profilePath, "")) {
            g_mkdir_with_parents(profilePath.c_str(), 511);

            if (!checkDirPath(profilePath, "")) {  // had problems with mkdir_with_parents return value on OS X, just check dir again
                Glib::ustring msg = Glib::ustring::compose("Creation of the user's processing profile directory \"%1\" failed!\n", profilePath);
                throw Error(msg);
            }
        }

        if (checkDirPath(profilePath, "Error: the user's processing profile path doesn't point to a directory or doesn't exist!\n")) {
            userProfilePath = profilePath;
            tmpPath = Glib::build_filename(App::get().argv0(), "profiles");

            if (checkDirPath(tmpPath, "Error: the global's processing profile path doesn't point to a directory or doesn't exist!\n")) {
                if (userProfilePath != tmpPath) {
                    globalProfilePath = tmpPath;
                }
            }
        } else {
            tmpPath = Glib::build_filename(App::get().argv0(), "profiles");

            if (checkDirPath(tmpPath, "Error: the global's processing profile path doesn't point to a directory or doesn't exist!\n")) {
                globalProfilePath = tmpPath;
            }
        }
    } else {
        // relative paths
        tmpPath = Glib::build_filename(rtdir, profilePath);

        if (!checkDirPath(tmpPath, "")) {
            g_mkdir_with_parents(tmpPath.c_str(), 511);

            if (!checkDirPath(tmpPath, "")) {
                Glib::ustring msg = Glib::ustring::compose("Creation of the user's processing profile directory \"%1\" failed!\n", tmpPath.c_str());
                throw Error(msg);
            }
        }

        if (checkDirPath(tmpPath, "Error: the user's processing profile path doesn't point to a directory!\n")) {
            userProfilePath = tmpPath;
        }

        tmpPath = Glib::build_filename(App::get().argv0(), "profiles");

        if (checkDirPath(tmpPath, "Error: the user's processing profile path doesn't point to a directory or doesn't exist!\n")) {
            globalProfilePath = tmpPath;
        }
    }

    Glib::ustring preferredPath = getPreferredProfilePath();

    // Paths are updated only if the user or global profile path is set
    if (lastRgbCurvesDir.empty() || !Glib::file_test(lastRgbCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastRgbCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastRgbCurvesDir = preferredPath;
    }

    if (lastLabCurvesDir.empty() || !Glib::file_test(lastLabCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastLabCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastLabCurvesDir = preferredPath;
    }

    if (lastRetinexDir.empty() || !Glib::file_test(lastRetinexDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastLabCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastRetinexDir = preferredPath;
    }

    if (lastDenoiseCurvesDir.empty() || !Glib::file_test(lastDenoiseCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastDenoiseCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastDenoiseCurvesDir = preferredPath;
    }

    if (lastWaveletCurvesDir.empty() || !Glib::file_test(lastWaveletCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastWaveletCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastWaveletCurvesDir = preferredPath;
    }

    if (lastlocalCurvesDir.empty() || !Glib::file_test(lastlocalCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastlocalCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastlocalCurvesDir = preferredPath;
    }

    if (lastlocalCurvesDirghs.empty() || !Glib::file_test(lastlocalCurvesDirghs, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastlocalCurvesDirghs, Glib::FILE_TEST_IS_DIR)) {
        lastlocalCurvesDirghs = preferredPath;
    }

    if (lastPFCurvesDir.empty() || !Glib::file_test(lastPFCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastPFCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastPFCurvesDir = preferredPath;
    }

    if (lastHsvCurvesDir.empty() || !Glib::file_test(lastHsvCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastHsvCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastHsvCurvesDir = preferredPath;
    }

    if (lastToneCurvesDir.empty() || !Glib::file_test(lastToneCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastToneCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastToneCurvesDir = preferredPath;
    }

    if (lastIcmCurvesDir.empty() || !Glib::file_test(lastIcmCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastIcmCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastIcmCurvesDir = preferredPath;
    }

    if (lastProfilingReferenceDir.empty() || !Glib::file_test(lastProfilingReferenceDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastProfilingReferenceDir, Glib::FILE_TEST_IS_DIR)) {
        lastProfilingReferenceDir = preferredPath;
    }

    if (lastVibranceCurvesDir.empty() || !Glib::file_test(lastVibranceCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastVibranceCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastVibranceCurvesDir = preferredPath;
    }

    if (loadSaveProfilePath.empty() || !Glib::file_test(loadSaveProfilePath, Glib::FILE_TEST_EXISTS) || !Glib::file_test(loadSaveProfilePath, Glib::FILE_TEST_IS_DIR)) {
        loadSaveProfilePath = preferredPath;
    }

    if (lastBWCurvesDir.empty() || !Glib::file_test(lastBWCurvesDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastBWCurvesDir, Glib::FILE_TEST_IS_DIR)) {
        lastBWCurvesDir = preferredPath;
    }

    if (lastICCProfCreatorDir.empty() || !Glib::file_test(lastICCProfCreatorDir, Glib::FILE_TEST_EXISTS) || !Glib::file_test(lastICCProfCreatorDir, Glib::FILE_TEST_IS_DIR)) {
        lastICCProfCreatorDir = preferredPath;
    }
}

Glib::ustring Options::getPreferredProfilePath() const
{
    if (!userProfilePath.empty()) {
        return userProfilePath;
    } else if (!globalProfilePath.empty()) {
        return globalProfilePath;
    } else {
        return "";
    }
}

/** @brief Get the absolute path of the given filename or the "Neutral" special value
  *
  *@param profName  path + filename of the procparam to look for. A filename without path can be provided for backward compatibility.
  *                 In this case, this parameter will be updated with the new format.
  *@return Send back the absolute path of the given filename or "Neutral" if "Neutral" has been set to profName. Implementer will have
  *        to test for this particular value. If the absolute path is invalid (e.g. the file doesn't exist), it will return an empty string.
  */
Glib::ustring Options::findProfilePath(Glib::ustring &profName) const
{
    if (profName.empty()) {
        return "";
    }

    if (profName == DEFPROFILE_INTERNAL) {
        return profName;
    }

    if (profName == DEFPROFILE_DYNAMIC) {
        return profName;
    }

    Glib::ustring p = profName.substr(0, 4);

    if (p == "${U}") {
        // the path starts by the User virtual path
        p = getUserProfilePath();
        Glib::ustring fullPath = Glib::build_filename(p, profName.substr(5) + App::PARAM_FILE_EXTENSION);

        if (!p.empty() && Glib::file_test(fullPath, Glib::FILE_TEST_EXISTS)) {
            return Glib::path_get_dirname(fullPath);
        }
    } else if (p == "${G}") {
        // the path starts by the User virtual path
        p = getGlobalProfilePath();
        Glib::ustring fullPath = Glib::build_filename(p, profName.substr(5) + App::PARAM_FILE_EXTENSION);

        if (!p.empty() && Glib::file_test(fullPath, Glib::FILE_TEST_EXISTS)) {
            return Glib::path_get_dirname(fullPath);
        }
    } else {
        // compatibility case -> convert the path to the new format
        p = getUserProfilePath();
        Glib::ustring fullPath = Glib::build_filename(p, profName + App::PARAM_FILE_EXTENSION);

        if (!p.empty() && Glib::file_test(fullPath, Glib::FILE_TEST_EXISTS)) {
            // update the profile path
            profName = Glib::build_filename("${U}", profName);
            return Glib::path_get_dirname(fullPath);
        }

        p = getGlobalProfilePath();
        fullPath = Glib::build_filename(p, profName + App::PARAM_FILE_EXTENSION);

        if (!p.empty() && Glib::file_test(fullPath, Glib::FILE_TEST_EXISTS)) {
            profName = Glib::build_filename("${G}", profName);
            return Glib::path_get_dirname(fullPath);
        }
    }

    return "";

}

void Options::setDefaults()
{

    windowWidth = 1200;
    windowHeight = 680;
    windowX = 0;
    windowY = 0;
    windowMaximized = true;
    windowMonitor = 0;
    meowMonitor = -1;
    meowMaximized = true;
    meowWidth = 1200;
    meowHeight = 680;
    meowX = 0;
    meowY = 0;
    saveAsDialogWidth = 920;
    saveAsDialogHeight = 680;
    savesParamsAtExit = true;
    saveFormat.format = "jpg";
    saveFormat.jpegQuality = 92;
    saveFormat.jpegSubSamp = 2;
    saveFormat.pngBits = 8;
    saveFormat.tiffBits = 16;
    saveFormat.tiffFloat = false;
    saveFormat.tiffUncompressed = true;
    saveFormat.bigTiff = false;
    saveFormat.saveParams = true;

    saveFormatBatch.format = "jpg";
    saveFormatBatch.jpegQuality = 92;
    saveFormatBatch.jpegSubSamp = 2;
    saveFormatBatch.pngBits = 8;
    saveFormatBatch.tiffBits = 16;
    saveFormatBatch.tiffFloat = false;
    saveFormatBatch.tiffUncompressed = true;
    saveFormatBatch.saveParams = true;

    savePathTemplate = "%p1/converted/%f";
    savePathFolder = "";
    saveUsePathTemplate = true;
    defProfRaw = DEFPROFILE_RAW;
    defProfImg = DEFPROFILE_IMG;
    dateFormat = "%y-%m-%d";
    adjusterMinDelay = 100;
    adjusterMaxDelay = 200;
    startupDir = STARTUPDIR_LAST;
    startupPath = "";
    useBundledProfiles = true;
    detailWindowWidth = -1;
    detailWindowHeight = -1;
    dirBrowserWidth = 260;
    dirBrowserHeight = 350;
    dirBrowserSortType = Gtk::SORT_ASCENDING;
    preferencesWidth = 800;
    preferencesHeight = 600;
    toolPanelWidth = 400;
    browserToolPanelWidth = 465;
    browserToolPanelHeight = 600;
    browserToolPanelOpened = true;;
    browserDirPanelOpened = true;
    editorFilmStripOpened = true;
    historyPanelWidth = 330;
    fontFamily = "default";
    fontSize = 10;
    CPFontFamily = "default";
    CPFontSize = 8;
    lastScale = 5;
    lastShowAllExif = false;
    panAccelFactor = 5;
    rememberZoomAndPan = true;
    lastCropSize = 1;
    fbOnlyRaw = false;
    fbShowDateTime = true;
    fbShowBasicExif = true;
    fbShowExpComp = false;
    maxZoomLimit = MaxZoom::PERCENTS_1600;
#ifdef _WIN32
    // use windows setting for visibility of hidden files/folders
    SHELLFLAGSTATE sft = { };
    SHGetSettings(&sft, SSF_SHOWALLOBJECTS);
    fbShowHidden = sft.fShowAllObjects;
#else
    fbShowHidden = false;
#endif
    fbArrangement = 2;                  // was 0
    navRGBUnit = NavigatorUnit::PERCENT;
    navHSVUnit = NavigatorUnit::PERCENT;
    multiUser = true;
    profilePath = "profiles";
    loadSaveProfilePath = "";           // will be corrected in load as otherwise construction fails
    lastCopyMovePath = "";
    version = "0.0.0.0";                // temporary value; will be correctly set in RTWindow::on_realize
    thumbSize = 160;
    thumbSizeTab = 160;
    thumbSizeQueue = 160;
    sameThumbSize = false;               // preferring speed of switch between file browser and single editor tab
    showHistory = true;
    showFilePanelState = 0;             // Not used anymore ; was the thumb strip state
    showInfo = true;
    cropPPI = 600;
    showClippedHighlights = false;
    showClippedShadows = false;
    highlightThreshold = 253;           // was 254
    shadowThreshold = 8;                // was 0
    bgcolor = 0;
    blinkClipped = false;
    language = DefaultLanguage;
    languageAutoDetect = langMgr.isOSLanguageDetectSupported();
    lastSaveAsPath = "";
    overwriteOutputFile = false;        // if TRUE, existing output JPGs/PNGs are overwritten, instead of adding ..-1.jpg, -2.jpg etc.
    theme = "RawTherapee";
    maxThumbnailHeight = 250;
    maxThumbnailWidth = 800;
    maxCacheEntries = 20000;
    thumbInterp = 1;
    autoSuffix = true;
    forceFormatOpts = true;
    saveMethodNum = 0;              // 0->immediate, 1->putToQueuHead, 2->putToQueueTail
    saveParamsFile = true;              // was false, but saving the procparams files next to the file make more sense when reorganizing file tree than in a cache
    saveParamsCache = false;            // there's no need to save the procparams files in a cache if saveParamsFile is true
    paramsLoadLocation = PLL_Input;     // was PLL_Cache
    procQueueEnabled = false;
    gimpDir = "";
    psDir = "";
    customEditorProg = "";
    externalEditors.clear();
    externalEditorIndex = -1;
    CPBKeys = CPBKT_TID;
    editorToSendTo = 1;
    editor_out_dir = EDITOR_OUT_DIR_TEMP;
    editor_custom_out_dir = "";
    editor_float32 = false;
    editor_bypass_output_profile = false;
    favoriteDirs.clear();
    tpOpen.clear();
    autoSaveTpOpen = true;
    //crvOpen.clear ();
    parseExtensions.clear();
    favorites.clear();
    cloneFavoriteTools = false;
    parseExtensionsEnabled.clear();
    parsedExtensions.clear();
    parsedExtensionsSet.clear();
    browseRecursive = false;
    browseRecursiveDepth = 10;
    browseRecursiveMaxDirs = 100;
    browseRecursiveFollowLinks = true;
    renameUseTemplates = false;
    renameTemplates.clear();
    thumbnailZoomRatios.clear();
    thumbnailZoomRatios.push_back(0.2);
    thumbnailZoomRatios.push_back(0.3);
    thumbnailZoomRatios.push_back(0.45);
    thumbnailZoomRatios.push_back(0.6);
    thumbnailZoomRatios.push_back(0.8);
    thumbnailZoomRatios.push_back(1.0);
    overlayedFileNames = false;
    filmStripOverlayedFileNames = false;
    internalThumbIfUntouched = true;    // if TRUE, only fast, internal preview images are taken if the image is not edited yet
    showFileNames = true;
    filmStripShowFileNames = false;
    tabbedUI = false;
    mainNBVertical = true;
    multiDisplayMode = 0;
    histogramPosition = 1;
    histogramRed = true;
    histogramGreen = true;
    histogramBlue = true;
    histogramLuma = false;
    histogramChroma = false;
    histogramBar = true;
    histogramHeight = 200;
    histogramDrawMode = 0;
    histogramScopeType = ScopeType::HISTOGRAM;
    histogramShowOptionButtons = false;
    histogramTraceBrightness = 1;
    curvebboxpos = 1;
    complexity = 2;
    spotmet = 0;

    showInspectorObservedArea = false;
    inspectorWindow = false;
    pinInspector = false;
    zoomOnScroll = true;
    reverseDiscreteScrollDir = false;
    reverseSmoothScrollDir = false;
    smoothScrollZoomSensitivity = SMOOTH_SCROLL_ZOOM_SENSITIVITY_DEFAULT;
    smoothScrollPanSensitivity = SMOOTH_SCROLL_PAN_SENSITIVITY_DEFAULT;
    zoom11Mode = Zoom11Mode::BASIC;

    prevdemo = PD_Sidecar;

    rgbDenoiseThreadLimit = 0;
#if defined( _OPENMP ) && defined( __x86_64__ )
    clutCacheSize = omp_get_num_procs();
#else
    clutCacheSize = 1;
#endif
    filledProfile = false;
    maxInspectorBuffers = 2; //  a rather conservative value for low specced systems...
    inspectorDelay = 0;
    serializeTiffRead = true;
    measure = false;
    chunkSizeAMAZE = 2;
    chunkSizeCA = 2;
    chunkSizeRCD = 2;
    chunkSizeRGB = 2;
    chunkSizeXT = 2;
    FileBrowserToolbarSingleRow = false;
    hideTPVScrollbar = false;
    whiteBalanceSpotSize = 8;
    showFilmStripToolBar = false;
    menuGroupRank = true;
    menuGroupLabel = true;
    menuGroupFileOperations = true;
    menuGroupProfileOperations = true;
    menuGroupExtProg = true;
    showtooltip = false;

    ICCPC_primariesPreset = "sRGB",
    ICCPC_redPrimaryX = 0.6400;
    ICCPC_redPrimaryY = 0.3300;
    ICCPC_greenPrimaryX = 0.3000;
    ICCPC_greenPrimaryY = 0.6000;
    ICCPC_bluePrimaryX = 0.1500;
    ICCPC_bluePrimaryY = 0.0600;
    ICCPC_gammaPreset = "Custom";
    ICCPC_gamma = 2.4;
    ICCPC_slope = 12.92;
    ICCPC_profileVersion = "v4";
    ICCPC_illuminant = "DEF";
    ICCPC_description = "";
    ICCPC_copyright = Options::getICCProfileCopyright();
    ICCPC_appendParamsToDesc = false;

    clutsDir = "./cluts";

    cutOverlayBrush = std::vector<double> (4);
    cutOverlayBrush[3] = 0.667;  // :-p

    navGuideBrush = std::vector<double> (4);
    //default to red
    navGuideBrush[0] = 1.0;
    navGuideBrush[1] = 0.0;
    navGuideBrush[2] = 0.0;
    navGuideBrush[3] = 1.0;

    sndEnable = true;
    sndLngEditProcDoneSecs = 3.0;
#ifdef __linux__
    sndBatchQueueDone = "complete";
    sndLngEditProcDone = "window-attention";
#endif

    // 0 = SET mode, 1 = ADD mode
    baBehav.assign(ADDSET_PARAM_NUM, 0);

    rtSettings.darkFramesPath = "";
    rtSettings.flatFieldsPath = "";
    rtSettings.cameraProfilesPath = "";
    rtSettings.lensProfilesPath = "";

#ifdef _WIN32
    const gchar* sysRoot = g_getenv("SystemRoot");  // Returns e.g. "c:\Windows"

    if (sysRoot != NULL) {
        rtSettings.iccDirectory = Glib::ustring(sysRoot) + Glib::ustring("\\System32\\spool\\drivers\\color");
    } else {
        rtSettings.iccDirectory = "C:\\WINDOWS\\System32\\spool\\drivers\\color";
    }

#elif defined __APPLE__
    rtSettings.iccDirectory = "/library/ColorSync/Profiles/Displays";
#else
    rtSettings.iccDirectory = "/usr/share/color/icc";
#endif
    rtSettings.enableLibRaw = true;
//   rtSettings.viewingdevice = 0;
//   rtSettings.viewingdevicegrey = 3;
    //  rtSettings.viewinggreySc = 1;

    rtSettings.printerProfile = Glib::ustring();
    rtSettings.printerIntent = rtengine::RI_RELATIVE;
    rtSettings.printerBPC = true;
    rtSettings.monitorProfile = Glib::ustring();
    rtSettings.monitorIntent = rtengine::RI_RELATIVE;
    rtSettings.monitorBPC = true;
    rtSettings.autocielab = false;
    rtSettings.autoMonitorProfile = false;
    rtSettings.adobe = "RTv2_Medium"; // put the name of yours profiles (here windows)
    rtSettings.prophoto = "RTv2_Large"; // these names appear in the menu "output profile"
    rtSettings.widegamut = "RTv2_Wide";
    rtSettings.DCIP3 = "RTv2_DCIP3";
    rtSettings.srgb = "RTv4_sRGB";
    rtSettings.bruce = "RTv2_Bruce";
    rtSettings.beta = "RTv2_Beta";
    rtSettings.best = "RTv2_Best";
    rtSettings.rec2020 = "RTv2_Rec2020";
    rtSettings.ACESp0 = "RTv2_ACES-AP0";
    rtSettings.ACESp1 = "RTv2_ACES-AP1";
    rtSettings.verbose = false;
    rtSettings.gamutICC = true;
    rtSettings.gamutLch = true;
    rtSettings.amchroma = 40;//between 20 and 140   low values increase effect..and also artifacts, high values reduces
    rtSettings.amchromajz = 40;//between 5 and 100  low values increase effect..and also artifacts, high values reduces
    rtSettings.level0_cbdl = 0;
    rtSettings.level123_cbdl = 30;
//locallab
    rtSettings.cropsleep = 50;//generate a pause of 50 µs for dcrop (100%)to avoid crash when moving window, between 0 to ??
    rtSettings.reduchigh = 0.85;//transition for luminance in scope
    rtSettings.reduclow = 0.85;//transition for luminance out scope
    rtSettings.detectshape = true;//experimental new detection shape
    rtSettings.previewselection = 5;//between 1 to 40
    rtSettings.cbdlsensi = 1.0;//between 0.001 to 1
    rtSettings.fftwsigma = true; //choice between sigma^2 or empirical formula
    rtSettings.basecorlog = 0.12;//reduction max Q in Cam16 sigmoid Log encoding between 0.05 and 0.5
// end locallab
    rtSettings.itcwb_enable = true;
    rtSettings.itcwb_deltaspec = 0.075;
    rtSettings.itcwb_powponder = 0.15;//max 0.2
//wavelet
    rtSettings.edghi = 3.0;//1.1 and 5.
    rtSettings.edglo = 0.5;//0.1 and 0.95
    rtSettings.limrad = 20.;//1 and 60


    rtSettings.protectred = 60;
    rtSettings.protectredh = 0.3;
    rtSettings.CRI_color = 0;
 //   rtSettings.autocielab = true;
    rtSettings.denoiselabgamma = 2;
    rtSettings.HistogramWorking = false;

    rtSettings.daubech = false;

    // #4327 - Noise Reduction settings removed from Preferences
    rtSettings.nrauto = 10; // between 2 and 20
    rtSettings.nrautomax = 40; // between 5 and 100
    rtSettings.nrhigh = 0.45; // between 0.1 and 0.9
    rtSettings.nrwavlevel = 1; // integer between 0 and 2
    rtSettings.leveldnv = 2;
    rtSettings.leveldnti = 0;
    rtSettings.leveldnaut = 0;
    rtSettings.leveldnliss = 0;
    rtSettings.leveldnautsimpl = 0;

//  rtSettings.colortoningab =0.7;
//  rtSettings.decaction =0.3;
//  rtSettings.ciebadpixgauss=false;
    rtSettings.rgbcurveslumamode_gamut = true;

    fastexport_bypass_sharpening         = true;
    fastexport_bypass_sharpenEdge        = true;
    fastexport_bypass_sharpenMicro       = true;
    //fastexport_bypass_lumaDenoise        = true;
    //fastexport_bypass_colorDenoise       = true;
    fastexport_bypass_defringe           = true;
    fastexport_bypass_dirpyrDenoise      = true;
    fastexport_bypass_dirpyrequalizer    = true;
    fastexport_bypass_wavelet    = true;
    fastexport_raw_bayer_method                  = "fast";
    //fastexport_bypass_raw_bayer_all_enhance    = true;
    fastexport_bypass_raw_bayer_dcb_iterations   = true;
    fastexport_bypass_raw_bayer_dcb_enhance      = true;
    fastexport_bypass_raw_bayer_lmmse_iterations = true;
    fastexport_bypass_raw_bayer_linenoise        = true;
    fastexport_bypass_raw_bayer_greenthresh      = true;
    fastexport_raw_xtrans_method                 = "fast";
    fastexport_bypass_raw_ccSteps        = true;
    fastexport_bypass_raw_ca             = true;
    fastexport_bypass_raw_df             = true;
    fastexport_bypass_raw_ff             = true;
    fastexport_icm_input_profile         = "(camera)";
    fastexport_icm_working_profile       = "ProPhoto";
    // Must be set after options.rtSettings.srgb has already been initialized!
    fastexport_icm_output_profile        = rtSettings.srgb;
    fastexport_icm_outputIntent          = rtengine::RI_RELATIVE;
    fastexport_icm_outputBPC             = true;
    fastexport_resize_enabled            = true;
    fastexport_resize_scale              = 1;
    fastexport_resize_appliesTo          = "Cropped area";
    fastexport_resize_method             = "Lanczos";
    fastexport_resize_dataspec           = 3;
    fastexport_resize_width              = 900;
    fastexport_resize_height             = 900;
    fastexport_resize_longedge           = 900;
    fastexport_resize_shortedge          = 900;
    fastexport_use_fast_pipeline         = true;

    lastIccDir = rtSettings.iccDirectory;
    lastDarkframeDir = rtSettings.darkFramesPath;
    lastFlatfieldDir = rtSettings.flatFieldsPath;
    lastCameraProfilesDir = rtSettings.cameraProfilesPath;
    lastLensProfilesDir = rtSettings.lensProfilesPath;
//  rtSettings.bw_complementary = true;
    // There is no reasonable default for curves. We can still suppose that they will take place
    // in a subdirectory of the user's own ProcParams presets, i.e. in a subdirectory
    // of the one pointed to by the "profile" field.
    // The following fields will then be initialized when "profile" will have its final value,
    // at the end of the "updatePaths" method.
    lastRgbCurvesDir = "";
    lastLabCurvesDir = "";
    lastRetinexDir = "";
    lastDenoiseCurvesDir = "";
    lastWaveletCurvesDir = "";
    lastlocalCurvesDir = "";
    lastlocalCurvesDirghs = "";
    lastPFCurvesDir = "";
    lastHsvCurvesDir = "";
    lastToneCurvesDir = "";
    lastIcmCurvesDir = "";
    lastVibranceCurvesDir = "";
    lastProfilingReferenceDir = "";
    lastBWCurvesDir = "";
    lastLensProfileDir = "";
    lastICCProfCreatorDir = "";
    gimpPluginShowInfoDialog = true;
    maxRecentFolders = 15;
    thumbnailRankColorMode = Options::ThumbnailPropertyMode::PROCPARAMS;
    sortMethod = SORT_BY_NAME;
    sortDescending = false;
    rtSettings.lensfunDbDirectory = ""; // set also in main.cc and main-cli.cc
    cropGuides = CROP_GUIDE_FULL;
    cropAutoFit = false;

    rtSettings.thumbnail_inspector_mode = rtengine::Settings::ThumbnailInspectorMode::JPEG;

    rtSettings.xmp_sidecar_style = rtengine::Settings::XmpSidecarStyle::STD;
    rtSettings.metadata_xmp_sync = rtengine::Settings::MetadataXmpSync::NONE;
}

Options* Options::copyFrom(Options* other)
{
    *this = *other;
    return this;
}

void Options::filterOutParsedExtensions()
{
    parsedExtensions.clear();
    parsedExtensionsSet.clear();

    for (unsigned int i = 0; i < parseExtensions.size(); i++)
        if (parseExtensionsEnabled[i]) {
            parsedExtensions.push_back(parseExtensions[i].lowercase());
            parsedExtensionsSet.emplace(parseExtensions[i].lowercase());
        }
}

void Options::readFromFile(Glib::ustring fname)
{
    setlocale(LC_NUMERIC, "C");  // to set decimal point to "."

    Glib::KeyFile keyFile;

    if (!Glib::file_test(fname, Glib::FILE_TEST_EXISTS)) {
        Glib::ustring msg = Glib::ustring::compose("Options file %1 does not exist", fname);
        throw Error(msg);
    }

    try {
        if (keyFile.load_from_file(fname)) {

// --------------------------------------------------------------------------------------------------------

            if (keyFile.has_group("General")) {
                if (keyFile.has_key("General", "TabbedEditor")) {
                    tabbedUI = keyFile.get_boolean("General", "TabbedEditor");
                }

                if (keyFile.has_key("General", "StartupDirectory")) {
                    if (keyFile.get_string("General", "StartupDirectory") == "home") {
                        startupDir = STARTUPDIR_HOME;
                    } else if (keyFile.get_string("General", "StartupDirectory") == "current") {
                        startupDir = STARTUPDIR_CURRENT;
                    } else if (keyFile.get_string("General", "StartupDirectory") == "last") {
                        startupDir = STARTUPDIR_LAST;
                    } else if (keyFile.get_string("General", "StartupDirectory") == "custom") {
                        startupDir = STARTUPDIR_CUSTOM;
                    }
                }

                if (keyFile.has_key("General", "StartupPath")) {
                    startupPath = keyFile.get_string("General", "StartupPath");
                }

                if (keyFile.has_key("General", "DateFormat")) {
                    dateFormat = keyFile.get_string("General", "DateFormat");
                }

                if (keyFile.has_key("General", "AdjusterMinDelay")) {
                    adjusterMinDelay = keyFile.get_integer("General", "AdjusterMinDelay");
                }

                if (keyFile.has_key("General", "AdjusterMaxDelay")) {
                    adjusterMaxDelay = keyFile.get_integer("General", "AdjusterMaxDelay");
                }

                if (keyFile.has_key("General", "StoreLastProfile")) {
                    savesParamsAtExit = keyFile.get_boolean("General", "StoreLastProfile");
                }

                if (keyFile.has_key("General", "MultiUser")) {
                    multiUser = keyFile.get_boolean("General", "MultiUser");
                }

                if (keyFile.has_key("General", "Version")) {
                    version = keyFile.get_string("General", "Version");
                }

                if (keyFile.has_key("General", "Language")) {
                    language = keyFile.get_string("General", "Language");
                    if (!language.compare("Espanol")) {
                        language = "Espanol (Latin America)";
                    }
                }

                if (keyFile.has_key("General", "LanguageAutoDetect")) {
                    languageAutoDetect = keyFile.get_boolean("General", "LanguageAutoDetect");
                }

                if (keyFile.has_key("General", "Theme")) {
                    theme = keyFile.get_string("General", "Theme");
                }

                if (keyFile.has_key("General", "DarkFramesPath")) {
                    rtSettings.darkFramesPath = keyFile.get_string("General", "DarkFramesPath");
                }

                if (keyFile.has_key("General", "FlatFieldsPath")) {
                    rtSettings.flatFieldsPath = keyFile.get_string("General", "FlatFieldsPath");
                }

                if (keyFile.has_key("General", "CameraProfilesPath")) {
                    rtSettings.cameraProfilesPath = keyFile.get_string("General", "CameraProfilesPath");
                }

                if (keyFile.has_key("General", "LensProfilesPath")) {
                    rtSettings.lensProfilesPath = keyFile.get_string("General", "LensProfilesPath");
                }

                if (keyFile.has_key("General", "Verbose")) {
                    rtSettings.verbose = keyFile.get_boolean("General", "Verbose");
                }

                if (keyFile.has_key("General", "Detectshape")) {
                    rtSettings.detectshape = keyFile.get_boolean("General", "Detectshape");
                }

                if (keyFile.has_key("General", "Fftwsigma")) {
                    rtSettings.fftwsigma = keyFile.get_boolean("General", "Fftwsigma");
                }

                if (keyFile.has_key("General", "Cropsleep")) {
                    rtSettings.cropsleep          = keyFile.get_integer("General", "Cropsleep");
                }

                if (keyFile.has_key("General", "Reduchigh")) {
                    rtSettings.reduchigh          = keyFile.get_double("General", "Reduchigh");
                }

                if (keyFile.has_key("General", "Reduclow")) {
                    rtSettings.reduclow          = keyFile.get_double("General", "Reduclow");
                }
            }

            // TODO: Remove.
            if (keyFile.has_group("External Editor")) {
                if (keyFile.has_key("External Editor", "EditorKind")) {
                    editorToSendTo = keyFile.get_integer("External Editor", "EditorKind");
                }

                if (keyFile.has_key("External Editor", "GimpDir")) {
                    gimpDir = keyFile.get_string("External Editor", "GimpDir");
                }

                if (keyFile.has_key("External Editor", "PhotoshopDir")) {
                    psDir = keyFile.get_string("External Editor", "PhotoshopDir");
                }

                if (keyFile.has_key("External Editor", "CustomEditor")) {
                    customEditorProg = keyFile.get_string("External Editor", "CustomEditor");
                }

                if (keyFile.has_key("External Editor", "OutputDir")) {
                    int v = keyFile.get_integer("External Editor", "OutputDir");
                    if (v < int(EDITOR_OUT_DIR_TEMP) || v > int(EDITOR_OUT_DIR_CUSTOM)) {
                        editor_out_dir = EDITOR_OUT_DIR_TEMP;
                    } else {
                        editor_out_dir = EditorOutDir(v);
                    }
                }

                if (keyFile.has_key("External Editor", "CustomOutputDir")) {
                    editor_custom_out_dir = keyFile.get_string("External Editor", "CustomOutputDir");
                }

                if (keyFile.has_key("External Editor", "Float32")) {
                    editor_float32 = keyFile.get_boolean("External Editor", "Float32");
                }

                if (keyFile.has_key("External Editor", "BypassOutputProfile")) {
                    editor_bypass_output_profile = keyFile.get_boolean("External Editor", "BypassOutputProfile");
                }

            }

            if (keyFile.has_group("External Editor")) {
                if (keyFile.has_key("External Editor", "Names")
                        || keyFile.has_key("External Editor", "Commands")
                        || keyFile.has_key("External Editor", "NativeCommands")
                        || keyFile.has_key("External Editor", "IconsSerialized")) {
                    // Multiple external editors.

                    const auto & names =
                        !keyFile.has_key("External Editor", "Names") ?
                            std::vector<Glib::ustring>() :
                            static_cast<std::vector<Glib::ustring>>(
                                keyFile.get_string_list("External Editor", "Names"));
                    const auto & commands =
                        !keyFile.has_key("External Editor", "Commands") ?
                            std::vector<Glib::ustring>() :
                            static_cast<std::vector<Glib::ustring>>(
                                keyFile.get_string_list("External Editor", "Commands"));
                    const auto & native_commands =
                        !keyFile.has_key("External Editor", "NativeCommands") ?
                            std::vector<bool>() :
                            static_cast<std::vector<bool>>(
                                keyFile.get_boolean_list("External Editor", "NativeCommands"));
                    const auto & icons_serialized =
                        !keyFile.has_key("External Editor", "IconsSerialized") ?
                            std::vector<Glib::ustring>() :
                            static_cast<std::vector<Glib::ustring>>(
                                keyFile.get_string_list("External Editor", "IconsSerialized"));
                    externalEditors = std::vector<ExternalEditor>(std::max(std::max(
                        names.size(), commands.size()), icons_serialized.size()));
                    for (unsigned i = 0; i < names.size(); i++) {
                        externalEditors[i].name = names[i];
                    }
                    for (unsigned i = 0; i < commands.size(); i++) {
                        externalEditors[i].command = commands[i];
                    }
                    for (unsigned i = 0; i < native_commands.size(); i++) {
                        externalEditors[i].native_command = native_commands[i];
                    }
                    for (unsigned i = 0; i < icons_serialized.size(); i++) {
                        externalEditors[i].icon_serialized = icons_serialized[i];
                    }

                    if (keyFile.has_key("External Editor", "EditorIndex")) {
                        int index = keyFile.get_integer("External Editor", "EditorIndex");
                        externalEditorIndex = std::min(
                            std::max(-1, index),
                            static_cast<int>(externalEditors.size())
                        );
                    }
                } else if (keyFile.has_key("External Editor", "EditorKind")) {
                    // Legacy fixed external editors. Convert to flexible.

                    // GIMP == 1, Photoshop == 2, Custom == 3.
                    editorToSendTo = keyFile.get_integer("External Editor", "EditorKind");

#ifdef _WIN32
                    auto getIconSerialized = [](const Glib::ustring &executable) {
                        // Backslashes and quotes must be escaped in the text representation of GVariant strings.
                        // See https://www.freedesktop.org/software/gstreamer-sdk/data/docs/2012.5/glib/gvariant-text.html#gvariant-text-strings
                        Glib::ustring exec_escaped = "";
                        for (const auto character : executable) {
                            if (character == '\\' || character == '\'') {
                                exec_escaped += '\\';
                            }
                            exec_escaped += character;
                        }
                        return Glib::ustring::compose("('themed', <['%1,0', '%1,0-symbolic']>)", exec_escaped);
                    };
                    Glib::ustring gimpDir = "";
                    if (keyFile.has_key("External Editor", "GimpDir")) {
                        gimpDir = keyFile.get_string("External Editor", "GimpDir");
                    }
                    auto executable = Glib::build_filename(gimpDir, "bin", "gimp-win-remote");
                    if (Glib::file_test(executable, Glib::FILE_TEST_IS_EXECUTABLE)) {
                        if (editorToSendTo == 1) {
                            externalEditorIndex = externalEditors.size();
                        }
                        externalEditors.emplace_back("GIMP", executable, true, getIconSerialized(executable));
                    } else {
                        for (auto ver = 12; ver >= 0; --ver) {
                            executable = Glib::build_filename(gimpDir, "bin", Glib::ustring::compose(Glib::ustring("gimp-2.%1.exe"), ver));
                            if (Glib::file_test(executable, Glib::FILE_TEST_IS_EXECUTABLE)) {
                                if (editorToSendTo == 1) {
                                    externalEditorIndex = externalEditors.size();
                                }
                                externalEditors.emplace_back("GIMP", executable, true, getIconSerialized(executable));
                                break;
                            }
                        }
                    }

                    Glib::ustring psDir = "";
                    if (keyFile.has_key("External Editor", "PhotoshopDir")) {
                        psDir = keyFile.get_string("External Editor", "PhotoshopDir");
                    }
                    executable = Glib::build_filename(psDir, "Photoshop.exe");
                    if (Glib::file_test(executable, Glib::FILE_TEST_IS_EXECUTABLE)) {
                        if (editorToSendTo == 2) {
                            externalEditorIndex = externalEditors.size();
                        }
                        externalEditors.emplace_back("Photoshop", executable, true, getIconSerialized(executable));
                    }

                    if (keyFile.has_key("External Editor", "CustomEditor")) {
                        executable = keyFile.get_string("External Editor", "CustomEditor");
                        if (!executable.empty()) {
                            if (editorToSendTo == 3) {
                                externalEditorIndex = externalEditors.size();
                            }
                            externalEditors.emplace_back("-", executable, true, "");
                        }
                    }
#elif defined __APPLE__
                    if (editorToSendTo == 1) {
                        externalEditorIndex = externalEditors.size();
                    }
                    externalEditors.emplace_back("GIMP", "open -a GIMP", true, "");
                    externalEditors.emplace_back("GIMP-dev", "open -a GIMP-dev", true, "");

                    if (editorToSendTo == 2) {
                        externalEditorIndex = externalEditors.size();
                    }
                    externalEditors.emplace_back("Photoshop", "open -a Photoshop", true, "");

                    if (keyFile.has_key("External Editor", "CustomEditor")) {
                        auto executable = keyFile.get_string("External Editor", "CustomEditor");
                        if (!executable.empty()) {
                            if (editorToSendTo == 3) {
                                externalEditorIndex = externalEditors.size();
                            }
                            externalEditors.emplace_back("-", executable, true, "");
                        }
                    }
#else
                    const Glib::ustring gimp_icon_serialized = "('themed', <['gimp', 'gimp-symbolic']>)";
                    if (Glib::find_program_in_path("gimp").compare("")) {
                        if (editorToSendTo == 1) {
                            externalEditorIndex = externalEditors.size();
                        }
                        externalEditors.emplace_back("GIMP", "gimp", true, gimp_icon_serialized);
                    } else if (Glib::find_program_in_path("gimp-remote").compare("")) {
                        if (editorToSendTo == 1) {
                            externalEditorIndex = externalEditors.size();
                        }
                        externalEditors.emplace_back("GIMP", "gimp-remote", true, gimp_icon_serialized);
                    }

                    if (keyFile.has_key("External Editor", "CustomEditor")) {
                        auto executable = keyFile.get_string("External Editor", "CustomEditor");
                        if (!executable.empty()) {
                            if (editorToSendTo == 3) {
                                externalEditorIndex = externalEditors.size();
                            }
                            externalEditors.emplace_back("-", executable, true, "");
                        }
                    }
#endif
                }
            }

            if (keyFile.has_group("Output")) {
                if (keyFile.has_key("Output", "Format")) {
                    saveFormat.format = keyFile.get_string("Output", "Format");
                }

                if (keyFile.has_key("Output", "JpegQuality")) {
                    saveFormat.jpegQuality = keyFile.get_integer("Output", "JpegQuality");
                }

                if (keyFile.has_key("Output", "JpegSubSamp")) {
                    saveFormat.jpegSubSamp = keyFile.get_integer("Output", "JpegSubSamp");
                }

                if (keyFile.has_key("Output", "PngBps")) {
                    saveFormat.pngBits = keyFile.get_integer("Output", "PngBps");
                }

                if (keyFile.has_key("Output", "TiffBps")) {
                    saveFormat.tiffBits = keyFile.get_integer("Output", "TiffBps");
                }

                if (keyFile.has_key("Output", "TiffFloat")) {
                    saveFormat.tiffFloat = keyFile.get_boolean("Output", "TiffFloat");
                }

                if (keyFile.has_key("Output", "TiffUncompressed")) {
                    saveFormat.tiffUncompressed = keyFile.get_boolean("Output", "TiffUncompressed");
                }

                if (keyFile.has_key("Output", "BigTiff")) {
                    saveFormat.bigTiff = keyFile.get_boolean("Output", "BigTiff");
                }

                if (keyFile.has_key("Output", "SaveProcParams")) {
                    saveFormat.saveParams = keyFile.get_boolean("Output", "SaveProcParams");
                }


                if (keyFile.has_key("Output", "FormatBatch")) {
                    saveFormatBatch.format = keyFile.get_string("Output", "FormatBatch");
                }

                if (keyFile.has_key("Output", "JpegQualityBatch")) {
                    saveFormatBatch.jpegQuality = keyFile.get_integer("Output", "JpegQualityBatch");
                }

                if (keyFile.has_key("Output", "JpegSubSampBatch")) {
                    saveFormatBatch.jpegSubSamp = keyFile.get_integer("Output", "JpegSubSampBatch");
                }

                if (keyFile.has_key("Output", "PngBpsBatch")) {
                    saveFormatBatch.pngBits = keyFile.get_integer("Output", "PngBpsBatch");
                }

                if (keyFile.has_key("Output", "TiffBpsBatch")) {
                    saveFormatBatch.tiffBits = keyFile.get_integer("Output", "TiffBpsBatch");
                }

                if (keyFile.has_key("Output", "TiffFloatBatch")) {
                    saveFormatBatch.tiffFloat = keyFile.get_boolean("Output", "TiffFloatBatch");
                }

                if (keyFile.has_key("Output", "TiffUncompressedBatch")) {
                    saveFormatBatch.tiffUncompressed = keyFile.get_boolean("Output", "TiffUncompressedBatch");
                }

                if (keyFile.has_key("Output", "SaveProcParamsBatch")) {
                    saveFormatBatch.saveParams = keyFile.get_boolean("Output", "SaveProcParamsBatch");
                }

                if (keyFile.has_key("Output", "Path")) {
                    savePathTemplate = keyFile.get_string("Output", "Path");
                }

                if (keyFile.has_key("Output", "PathTemplate")) {
                    savePathTemplate = keyFile.get_string("Output", "PathTemplate");
                }

                if (keyFile.has_key("Output", "PathFolder")) {
                    savePathFolder = keyFile.get_string("Output", "PathFolder");
                }

                if (keyFile.has_key("Output", "AutoSuffix")) {
                    autoSuffix = keyFile.get_boolean("Output", "AutoSuffix");
                }

                if (keyFile.has_key("Output", "ForceFormatOpts")) {
                    forceFormatOpts = keyFile.get_boolean("Output", "ForceFormatOpts");
                }

                if (keyFile.has_key("Output", "SaveMethodNum")) {
                    saveMethodNum = keyFile.get_integer("Output", "SaveMethodNum");
                }

                if (keyFile.has_key("Output", "UsePathTemplate")) {
                    saveUsePathTemplate = keyFile.get_boolean("Output", "UsePathTemplate");
                }

                if (keyFile.has_key("Output", "LastSaveAsPath")) {
                    lastSaveAsPath = keyFile.get_string("Output", "LastSaveAsPath");
                }

                if (keyFile.has_key("Output", "OverwriteOutputFile")) {
                    overwriteOutputFile = keyFile.get_boolean("Output", "OverwriteOutputFile");
                }
            }

            if (keyFile.has_group("Profiles")) {
                if (keyFile.has_key("Profiles", "Directory")) {
                    profilePath = keyFile.get_string("Profiles", "Directory");
                }

                if (keyFile.has_key("Profiles", "UseBundledProfiles")) {
                    useBundledProfiles = keyFile.get_boolean("Profiles", "UseBundledProfiles");
                }

                if (keyFile.has_key("Profiles", "LoadSaveProfilePath")) {
                    loadSaveProfilePath = keyFile.get_string("Profiles", "LoadSaveProfilePath");
                }

                if (keyFile.has_key("Profiles", "RawDefault")) {
                    defProfRaw = keyFile.get_string("Profiles", "RawDefault");
                }

                if (keyFile.has_key("Profiles", "ImgDefault")) {
                    defProfImg = keyFile.get_string("Profiles", "ImgDefault");
                }

                if (keyFile.has_key("Profiles", "FilledProfile")) {
                    filledProfile = keyFile.get_boolean("Profiles", "FilledProfile");
                }

                if (keyFile.has_key("Profiles", "SaveParamsWithFile")) {
                    saveParamsFile = keyFile.get_boolean("Profiles", "SaveParamsWithFile");
                }

                if (keyFile.has_key("Profiles", "SaveParamsToCache")) {
                    saveParamsCache = keyFile.get_boolean("Profiles", "SaveParamsToCache");
                }

                if (keyFile.has_key("Profiles", "LoadParamsFromLocation")) {
                    paramsLoadLocation = (PPLoadLocation)keyFile.get_integer("Profiles", "LoadParamsFromLocation");
                }

                if (keyFile.has_key("Profiles", "CustomProfileBuilder")) {
                    CPBPath = keyFile.get_string("Profiles", "CustomProfileBuilder");  // for backward compatibility only
                }

                if (keyFile.has_key("Profiles", "CustomProfileBuilderPath")) {
                    CPBPath = keyFile.get_string("Profiles", "CustomProfileBuilderPath");
                }

                if (keyFile.has_key("Profiles", "CustomProfileBuilderKeys")) {
                    CPBKeys = (CPBKeyType)keyFile.get_integer("Profiles", "CustomProfileBuilderKeys");
                }
            }

            if (keyFile.has_group("File Browser")) {
                if (keyFile.has_key("File Browser", "ThumbnailSize")) {
                    thumbSize = keyFile.get_integer("File Browser", "ThumbnailSize");
                }

                if (keyFile.has_key("File Browser", "ThumbnailSizeTab")) {
                    thumbSizeTab = keyFile.get_integer("File Browser", "ThumbnailSizeTab");
                }

                if (keyFile.has_key("File Browser", "ThumbnailSizeQueue")) {
                    thumbSizeQueue = keyFile.get_integer("File Browser", "ThumbnailSizeQueue");
                }

                if (keyFile.has_key("File Browser", "SameThumbSize")) {
                    sameThumbSize = keyFile.get_integer("File Browser", "SameThumbSize");
                }

                if (keyFile.has_key("File Browser", "BrowseOnlyRaw")) {
                    fbOnlyRaw = keyFile.get_boolean("File Browser", "BrowseOnlyRaw");
                }

                if (keyFile.has_key("File Browser", "BrowserShowsDate")) {
                    fbShowDateTime = keyFile.get_boolean("File Browser", "BrowserShowsDate");
                }

                if (keyFile.has_key("File Browser", "BrowserShowsExif")) {
                    fbShowBasicExif = keyFile.get_boolean("File Browser", "BrowserShowsExif");
                }

                if (keyFile.has_key("File Browser", "BrowserShowsExpComp")) {
                    fbShowExpComp = keyFile.get_boolean("File Browser", "BrowserShowsExpComp");
                }

#ifndef _WIN32
                if (keyFile.has_key("File Browser", "BrowserShowsHidden")) {
                    fbShowHidden = keyFile.get_boolean("File Browser", "BrowserShowsHidden");
                }
#endif

                if (keyFile.has_key("File Browser", "MaxPreviewHeight")) {
                    maxThumbnailHeight = keyFile.get_integer("File Browser", "MaxPreviewHeight");
                }

                if (keyFile.has_key("File Browser", "MaxPreviewWidth")) {
                    maxThumbnailWidth = keyFile.get_integer("File Browser", "MaxPreviewWidth");
                }

                if (keyFile.has_key("File Browser", "MaxCacheEntries")) {
                    maxCacheEntries = keyFile.get_integer("File Browser", "MaxCacheEntries");
                }

                if (keyFile.has_key("File Browser", "ParseExtensions")) {
                    auto l = keyFile.get_string_list("File Browser", "ParseExtensions");
                    if (!l.empty()) {
                        parseExtensions = l;
                    }
                }

                if (keyFile.has_key("File Browser", "ParseExtensionsEnabled")) {
                    auto l = keyFile.get_integer_list("File Browser", "ParseExtensionsEnabled");
                    if (!l.empty()) {
                        parseExtensionsEnabled = l;
                    }
                }

                // check and add extensions that are missing from config
                tidyParseExtensions(parseExtensions, parseExtensionsEnabled, knownExtensions);

                if (keyFile.has_key("File Browser", "ThumbnailArrangement")) {
                    fbArrangement = keyFile.get_integer("File Browser", "ThumbnailArrangement");
                }

                if (keyFile.has_key("File Browser", "ThumbnailInterpolation")) {
                    thumbInterp = keyFile.get_integer("File Browser", "ThumbnailInterpolation");
                }

                if (keyFile.has_key("File Browser", "FavoriteDirs")) {
                    favoriteDirs = keyFile.get_string_list("File Browser", "FavoriteDirs");
                }

                if (keyFile.has_key("File Browser", "RenameTemplates")) {
                    renameTemplates = keyFile.get_string_list("File Browser", "RenameTemplates");
                }

                if (keyFile.has_key("File Browser", "RenameUseTemplates")) {
                    renameUseTemplates = keyFile.get_boolean("File Browser", "RenameUseTemplates");
                }

                if (keyFile.has_key("File Browser", "ThumbnailZoomRatios")) {
                    thumbnailZoomRatios = keyFile.get_double_list("File Browser", "ThumbnailZoomRatios");
                }

                if (keyFile.has_key("File Browser", "OverlayedFileNames")) {
                    overlayedFileNames = keyFile.get_boolean("File Browser", "OverlayedFileNames");
                }

                if (keyFile.has_key("File Browser", "FilmStripOverlayedFileNames")) {
                    filmStripOverlayedFileNames = keyFile.get_boolean("File Browser", "FilmStripOverlayedFileNames");
                }

                if (keyFile.has_key("File Browser", "ShowFileNames")) {
                    showFileNames = keyFile.get_boolean("File Browser", "ShowFileNames");
                }

                if (keyFile.has_key("File Browser", "FilmStripShowFileNames")) {
                    filmStripShowFileNames = keyFile.get_boolean("File Browser", "FilmStripShowFileNames");
                }

                if (keyFile.has_key("File Browser", "InternalThumbIfUntouched")) {
                    internalThumbIfUntouched = keyFile.get_boolean("File Browser", "InternalThumbIfUntouched");
                }

                if (keyFile.has_key("File Browser", "menuGroupRank")) {
                    menuGroupRank = keyFile.get_boolean("File Browser", "menuGroupRank");
                }

                if (keyFile.has_key("File Browser", "menuGroupLabel")) {
                    menuGroupLabel = keyFile.get_boolean("File Browser", "menuGroupLabel");
                }

                if (keyFile.has_key("File Browser", "menuGroupFileOperations")) {
                    menuGroupFileOperations = keyFile.get_boolean("File Browser", "menuGroupFileOperations");
                }

                if (keyFile.has_key("File Browser", "menuGroupProfileOperations")) {
                    menuGroupProfileOperations = keyFile.get_boolean("File Browser", "menuGroupProfileOperations");
                }

                if (keyFile.has_key("File Browser", "menuGroupExtProg")) {
                    menuGroupExtProg = keyFile.get_boolean("File Browser", "menuGroupExtProg");
                }

                if (keyFile.has_key("File Browser", "MaxRecentFolders")) {
                    maxRecentFolders = keyFile.get_integer("File Browser", "MaxRecentFolders");
                }

                recentFolders.reserve(maxRecentFolders + 10);  // reserve some more than maxRecentFolders, because at runtime it stores more than that

                if (keyFile.has_key("File Browser", "RecentFolders")) {
                    recentFolders = keyFile.get_string_list("File Browser", "RecentFolders");
                }

                if (keyFile.has_key("File Browser", "SortMethod")) {
                    int v = keyFile.get_integer("File Browser", "SortMethod");
                    if (v < int(0) || v >= int(SORT_METHOD_COUNT)) {
                        sortMethod = SORT_BY_NAME;
                    } else {
                        sortMethod = SortMethod(v);
                    }
                }

                if (keyFile.has_key("File Browser", "SortDescending")) {
                    sortDescending = keyFile.get_boolean("File Browser", "SortDescending");
                }

                if (keyFile.has_key("File Browser", "BrowseRecursive")) {
                    browseRecursive = keyFile.get_boolean("File Browser", "BrowseRecursive");
                }

                if (keyFile.has_key("File Browser", "BrowseRecursiveDepth")) {
                    browseRecursiveDepth = keyFile.get_integer("File Browser", "BrowseRecursiveDepth");
                }

                if (keyFile.has_key("File Browser", "BrowseRecursiveMaxDirs")) {
                    browseRecursiveMaxDirs = keyFile.get_integer("File Browser", "BrowseRecursiveMaxDirs");
                }

                if (keyFile.has_key("File Browser", "BrowseRecursiveFollowLinks")) {
                    browseRecursiveFollowLinks = keyFile.get_boolean("File Browser", "BrowseRecursiveFollowLinks");
                }

                if (keyFile.has_key("File Browser", "ThumbnailRankColorMode")) {
                    std::string val = keyFile.get_string("File Browser", "ThumbnailRankColorMode");
                    if (val == "xmp") {
                        thumbnailRankColorMode = ThumbnailPropertyMode::XMP;
                    } else {
                        thumbnailRankColorMode = ThumbnailPropertyMode::PROCPARAMS;
                    }
                }
            }

            if (keyFile.has_group("Clipping Indication")) {
                if (keyFile.has_key("Clipping Indication", "HighlightThreshold")) {
                    highlightThreshold = keyFile.get_integer("Clipping Indication", "HighlightThreshold");
                }

                if (keyFile.has_key("Clipping Indication", "ShadowThreshold")) {
                    shadowThreshold = keyFile.get_integer("Clipping Indication", "ShadowThreshold");
                }

                if (keyFile.has_key("Clipping Indication", "BlinkClipped")) {
                    blinkClipped = keyFile.get_boolean("Clipping Indication", "BlinkClipped");
                }
            }

            if (keyFile.has_group("Performance")) {
                if (keyFile.has_key("Performance", "RgbDenoiseThreadLimit")) {
                    rgbDenoiseThreadLimit = keyFile.get_integer("Performance", "RgbDenoiseThreadLimit");
                }

                if (keyFile.has_key("Performance", "ClutCacheSize")) {
                    clutCacheSize = keyFile.get_integer("Performance", "ClutCacheSize");
                }

                if (keyFile.has_key("Performance", "MaxInspectorBuffers")) {
                    maxInspectorBuffers = keyFile.get_integer("Performance", "MaxInspectorBuffers");
                }

                if (keyFile.has_key("Performance", "InspectorDelay")) {
                    inspectorDelay = keyFile.get_integer("Performance", "InspectorDelay");
                }

                if (keyFile.has_key("Performance", "PreviewDemosaicFromSidecar")) {
                    prevdemo = (prevdemo_t)keyFile.get_integer("Performance", "PreviewDemosaicFromSidecar");
                }

                if (keyFile.has_key("Performance", "SerializeTiffRead")) {
                    serializeTiffRead = keyFile.get_boolean("Performance", "SerializeTiffRead");
                }

                if (keyFile.has_key("Performance", "Measure")) {
                    measure = keyFile.get_boolean("Performance", "Measure");
                }

                if (keyFile.has_key("Performance", "ChunkSizeAMAZE")) {
                    chunkSizeAMAZE = std::min(16, std::max(1, keyFile.get_integer("Performance", "ChunkSizeAMAZE")));
                }

                if (keyFile.has_key("Performance", "ChunkSizeCA")) {
                    chunkSizeCA = std::min(16, std::max(1, keyFile.get_integer("Performance", "ChunkSizeCA")));
                }

                if (keyFile.has_key("Performance", "ChunkSizeRCD")) {
                    chunkSizeRCD = std::min(16, std::max(1, keyFile.get_integer("Performance", "ChunkSizeRCD")));
                }

                if (keyFile.has_key("Performance", "ChunkSizeRGB")) {
                    chunkSizeRGB = std::min(16, std::max(1, keyFile.get_integer("Performance", "ChunkSizeRGB")));
                }

                if (keyFile.has_key("Performance", "ChunkSizeXT")) {
                    chunkSizeXT = std::min(16, std::max(1, keyFile.get_integer("Performance", "ChunkSizeXT")));
                }

                if (keyFile.has_key("Performance", "ThumbnailInspectorMode")) {
                    rtSettings.thumbnail_inspector_mode = static_cast<rtengine::Settings::ThumbnailInspectorMode>(keyFile.get_integer("Performance", "ThumbnailInspectorMode"));
                }
            }

            if (keyFile.has_group("GUI")) {
                if (keyFile.has_key("GUI", "Favorites")) {
                    favorites = keyFile.get_string_list("GUI", "Favorites");
                }

                if (keyFile.has_key("GUI", "FavoritesCloneTools")) {
                    cloneFavoriteTools = keyFile.get_boolean("GUI", "FavoritesCloneTools");
                }

                if (keyFile.has_key("GUI", "WindowWidth")) {
                    windowWidth = keyFile.get_integer("GUI", "WindowWidth");
                }

                if (keyFile.has_key("GUI", "WindowHeight")) {
                    windowHeight = keyFile.get_integer("GUI", "WindowHeight");
                }

                if (keyFile.has_key("GUI", "WindowX")) {
                    windowX = keyFile.get_integer("GUI", "WindowX");
                }

                if (keyFile.has_key("GUI", "WindowY")) {
                    windowY = keyFile.get_integer("GUI", "WindowY");
                }

                if (keyFile.has_key("GUI", "WindowMonitor")) {
                    windowMonitor = keyFile.get_integer("GUI", "WindowMonitor");
                }

                if (keyFile.has_key("GUI", "MeowMonitor")) {
                    meowMonitor = keyFile.get_integer("GUI", "MeowMonitor");
                }

                if (keyFile.has_key("GUI", "MeowMaximized")) {
                    meowMaximized = keyFile.get_boolean("GUI", "MeowMaximized");
                }

                if (keyFile.has_key("GUI", "MeowWidth")) {
                    meowWidth = keyFile.get_integer("GUI", "MeowWidth");
                }

                if (keyFile.has_key("GUI", "MeowHeight")) {
                    meowHeight = keyFile.get_integer("GUI", "MeowHeight");
                }

                if (keyFile.has_key("GUI", "MeowX")) {
                    meowX = keyFile.get_integer("GUI", "MeowX");
                }

                if (keyFile.has_key("GUI", "MeowY")) {
                    meowY = keyFile.get_integer("GUI", "MeowY");
                }

                if (keyFile.has_key("GUI", "WindowMaximized")) {
                    windowMaximized = keyFile.get_boolean("GUI", "WindowMaximized");
                }

                if (keyFile.has_key("GUI", "DetailWindowWidth")) {
                    detailWindowWidth = keyFile.get_integer("GUI", "DetailWindowWidth");
                }

                if (keyFile.has_key("GUI", "DetailWindowHeight")) {
                    detailWindowHeight = keyFile.get_integer("GUI", "DetailWindowHeight");
                }

                if (keyFile.has_key("GUI", "DirBrowserWidth")) {
                    dirBrowserWidth = keyFile.get_integer("GUI", "DirBrowserWidth");
                }

                if (keyFile.has_key("GUI", "DirBrowserHeight")) {
                    dirBrowserHeight = keyFile.get_integer("GUI", "DirBrowserHeight");
                }

                if (keyFile.has_key("GUI", "SortType")) {
                    dirBrowserSortType = static_cast<Gtk::SortType>(keyFile.get_integer("GUI", "SortType"));
                }

                if (keyFile.has_key("GUI", "PreferencesWidth")) {
                    preferencesWidth = keyFile.get_integer("GUI", "PreferencesWidth");
                }

                if (keyFile.has_key("GUI", "PreferencesHeight")) {
                    preferencesHeight = keyFile.get_integer("GUI", "PreferencesHeight");
                }

                if (keyFile.has_key("GUI", "SaveAsDialogWidth")) {
                    saveAsDialogWidth = keyFile.get_integer("GUI", "SaveAsDialogWidth");
                }

                if (keyFile.has_key("GUI", "SaveAsDialogHeight")) {
                    saveAsDialogHeight = keyFile.get_integer("GUI", "SaveAsDialogHeight");
                }

                if (keyFile.has_key("GUI", "ToolPanelWidth")) {
                    toolPanelWidth = keyFile.get_integer("GUI", "ToolPanelWidth");
                }

                if (keyFile.has_key("GUI", "BrowserToolPanelWidth")) {
                    browserToolPanelWidth = keyFile.get_integer("GUI", "BrowserToolPanelWidth");
                }

                if (keyFile.has_key("GUI", "BrowserToolPanelHeight")) {
                    browserToolPanelHeight = keyFile.get_integer("GUI", "BrowserToolPanelHeight");
                }

                if (keyFile.has_key("GUI", "BrowserToolPanelOpened")) {
                    browserToolPanelOpened = keyFile.get_boolean("GUI", "BrowserToolPanelOpened");
                }

                if (keyFile.has_key("GUI", "BrowserDirPanelOpened")) {
                    browserDirPanelOpened = keyFile.get_boolean("GUI", "BrowserDirPanelOpened");
                }

                if (keyFile.has_key("GUI", "EditorFilmStripOpened")) {
                    editorFilmStripOpened = keyFile.get_boolean("GUI", "EditorFilmStripOpened");
                }

                if (keyFile.has_key("GUI", "HistoryPanelWidth")) {
                    historyPanelWidth = keyFile.get_integer("GUI", "HistoryPanelWidth");
                }

                if (keyFile.has_key("GUI", "FontFamily")) {
                    fontFamily = keyFile.get_string("GUI", "FontFamily");
                }

                if (keyFile.has_key("GUI", "FontSize")) {
                    fontSize = keyFile.get_integer("GUI", "FontSize");
                }

                if (keyFile.has_key("GUI", "CPFontFamily")) {
                    CPFontFamily = keyFile.get_string("GUI", "CPFontFamily");
                }

                if (keyFile.has_key("GUI", "CPFontSize")) {
                    CPFontSize = keyFile.get_integer("GUI", "CPFontSize");
                }

                if (keyFile.has_key("GUI", "LastPreviewScale")) {
                    lastScale = keyFile.get_integer("GUI", "LastPreviewScale");
                }

                if (keyFile.has_key("GUI", "LastShowAllExif")) {
                    lastShowAllExif = keyFile.get_boolean("GUI", "LastShowAllExif");
                }

                if (keyFile.has_key("GUI", "PanAccelFactor")) {
                    panAccelFactor = keyFile.get_integer("GUI", "PanAccelFactor");
                }

                if (keyFile.has_key("GUI", "RememberZoomAndPan")) {
                    rememberZoomAndPan = keyFile.get_boolean("GUI", "RememberZoomAndPan");
                }

                if (keyFile.has_key("GUI", "LastCropSize")) {
                    lastCropSize = keyFile.get_integer("GUI", "LastCropSize");
                }

                if (keyFile.has_key("GUI", "ShowHistory")) {
                    showHistory = keyFile.get_boolean("GUI", "ShowHistory");
                }

                if (keyFile.has_key("GUI", "ShowFilePanelState")) {
                    showFilePanelState = keyFile.get_integer("GUI", "ShowFilePanelState");
                }

                if (keyFile.has_key("GUI", "ShowInfo")) {
                    showInfo = keyFile.get_boolean("GUI", "ShowInfo");
                }

                if (keyFile.has_key("GUI", "MainNBVertical")) {
                    mainNBVertical = keyFile.get_boolean("GUI", "MainNBVertical");
                }

                if (keyFile.has_key("GUI", "ShowClippedHighlights")) {
                    showClippedHighlights = keyFile.get_boolean("GUI", "ShowClippedHighlights");
                }

                if (keyFile.has_key("GUI", "ShowClippedShadows")) {
                    showClippedShadows = keyFile.get_boolean("GUI", "ShowClippedShadows");
                }

                if (keyFile.has_key("GUI", "FrameColor")) {
                    bgcolor = keyFile.get_integer("GUI", "FrameColor");
                }

                if (keyFile.has_key("GUI", "ProcessingQueueEnbled")) {
                    procQueueEnabled = keyFile.get_boolean("GUI", "ProcessingQueueEnbled");
                }

                if (keyFile.has_key("GUI", "ToolPanelsExpanded")) {
                    tpOpen = keyFile.get_integer_list("GUI", "ToolPanelsExpanded");
                }

                if (keyFile.has_key("GUI", "ToolPanelsExpandedAutoSave")) {
                    autoSaveTpOpen = keyFile.get_boolean("GUI", "ToolPanelsExpandedAutoSave");
                }

                if (keyFile.has_key("GUI", "MultiDisplayMode")) {
                    multiDisplayMode = keyFile.get_integer("GUI", "MultiDisplayMode");
                }

                //if (keyFile.has_key ("GUI", "CurvePanelsExpanded")) crvOpen = keyFile.get_integer_list ("GUI", "CurvePanelsExpanded");
                if (keyFile.has_key("GUI", "CutOverlayBrush")) {
                    cutOverlayBrush = keyFile.get_double_list("GUI", "CutOverlayBrush");
                }

                if (keyFile.has_key("GUI", "NavGuideBrush")) {
                    navGuideBrush = keyFile.get_double_list("GUI", "NavGuideBrush");
                }

                if (keyFile.has_key("GUI", "HistogramPosition")) {
                    histogramPosition = keyFile.get_integer("GUI", "HistogramPosition");
                }

                if (keyFile.has_key("GUI", "HistogramRed")) {
                    histogramRed = keyFile.get_boolean("GUI", "HistogramRed");
                }

                if (keyFile.has_key("GUI", "HistogramGreen")) {
                    histogramGreen = keyFile.get_boolean("GUI", "HistogramGreen");
                }

                if (keyFile.has_key("GUI", "HistogramBlue")) {
                    histogramBlue = keyFile.get_boolean("GUI", "HistogramBlue");
                }

                if (keyFile.has_key("GUI", "HistogramLuma")) {
                    histogramLuma = keyFile.get_boolean("GUI", "HistogramLuma");
                }

                if (keyFile.has_key("GUI", "HistogramChroma")) {
                    histogramChroma = keyFile.get_boolean("GUI", "HistogramChroma");
                }

                if (keyFile.has_key("GUI", "HistogramRAW")) {
                    // Legacy option, replaced by HistogramScopeType.
                    if (keyFile.get_boolean("GUI", "HistogramRAW")) {
                        histogramScopeType = ScopeType::HISTOGRAM_RAW;
                    }
                }

                if (keyFile.has_key("GUI", "HistogramBar")) {
                    histogramBar = keyFile.get_boolean("GUI", "HistogramBar");
                }

                if (keyFile.has_key("GUI", "HistogramHeight")) {
                    histogramHeight = keyFile.get_integer("GUI", "HistogramHeight");
                }

                if (keyFile.has_key("GUI", "HistogramDrawMode")) {
                    histogramDrawMode = keyFile.get_integer("GUI", "HistogramDrawMode");
                }

                if (keyFile.has_key("GUI", "HistogramScopeType")) {
                    histogramScopeType = static_cast<ScopeType>(keyFile.get_integer("GUI", "HistogramScopeType"));
                }

                if (keyFile.has_key("GUI", "HistogramShowOptionButtons")) {
                    histogramShowOptionButtons = keyFile.get_boolean("GUI", "HistogramShowOptionButtons");
                }

                if (keyFile.has_key("GUI", "HistogramTraceBrightness")) {
                    histogramTraceBrightness = keyFile.get_double("GUI", "HistogramTraceBrightness");
                }

                if (keyFile.has_key("GUI", "NavigatorRGBUnit")) {
                    navRGBUnit = (NavigatorUnit)keyFile.get_integer("GUI", "NavigatorRGBUnit");
                }

                if (keyFile.has_key("GUI", "NavigatorHSVUnit")) {
                    navHSVUnit = (NavigatorUnit)keyFile.get_integer("GUI", "NavigatorHSVUnit");
                }


                if (keyFile.has_key("GUI", "ShowFilmStripToolBar")) {
                    showFilmStripToolBar = keyFile.get_boolean("GUI", "ShowFilmStripToolBar");
                }

                if (keyFile.has_key("GUI", "Showtooltip")) {//show tooltip in locallab
                    showtooltip = keyFile.get_boolean("GUI", "Showtooltip");
                }

                if (keyFile.has_key("GUI", "FileBrowserToolbarSingleRow")) {
                    FileBrowserToolbarSingleRow = keyFile.get_boolean("GUI", "FileBrowserToolbarSingleRow");
                }

                if (keyFile.has_key("GUI", "HideTPVScrollbar")) {
                    hideTPVScrollbar = keyFile.get_boolean("GUI", "HideTPVScrollbar");
                }

                if (keyFile.has_key("GUI", "HistogramWorking")) {
                    rtSettings.HistogramWorking = keyFile.get_boolean("GUI", "HistogramWorking");
                }

                if (keyFile.has_key("GUI", "CurveBBoxPosition")) {
                    curvebboxpos = keyFile.get_integer("GUI", "CurveBBoxPosition");
                }

                if (keyFile.has_key("GUI", "Complexity")) {
                    complexity = keyFile.get_integer("GUI", "Complexity");
                }

                if (keyFile.has_key("GUI", "Spotmet")) {
                    spotmet = keyFile.get_integer("GUI", "Spotmet");
                }

                if (keyFile.has_key("GUI", "ShowInspectorObservedArea")) {
                    showInspectorObservedArea = keyFile.get_boolean(
                        "GUI", "ShowInspectorObservedArea");
                }

                if (keyFile.has_key("GUI", "InspectorWindow")) {
                    inspectorWindow = keyFile.get_boolean("GUI", "InspectorWindow");
                }

                if (keyFile.has_key("GUI", "PinInspectorWindow")) {
                    pinInspector = keyFile.get_boolean("GUI", "PinInspectorWindow");
                }

                if (keyFile.has_key("GUI", "ZoomOnScroll")) {
                    zoomOnScroll = keyFile.get_boolean("GUI", "ZoomOnScroll");
                }

                if (keyFile.has_key("GUI", "MaxZoom")) {
                    maxZoomLimit = static_cast<MaxZoom>(keyFile.get_integer("GUI", "MaxZoom"));
                }

                if (keyFile.has_key("GUI", "ReverseDiscreteScrollDirection")) {
                    reverseDiscreteScrollDir =
                        keyFile.get_boolean("GUI", "ReverseDiscreteScrollDirection");
                }

                if (keyFile.has_key("GUI", "ReverseSmoothScrollDirection")) {
                    reverseSmoothScrollDir =
                        keyFile.get_boolean("GUI", "ReverseSmoothScrollDirection");
                }

                if (keyFile.has_key("GUI", "SmoothScrollZoomSensitivity")) {
                    smoothScrollZoomSensitivity =
                        keyFile.get_integer("GUI", "SmoothScrollZoomSensitivity");
                }

                if (keyFile.has_key("GUI", "SmoothScrollPanSensitivity")) {
                    smoothScrollPanSensitivity =
                        keyFile.get_integer("GUI", "SmoothScrollPanSensitivity");
                }

                if (keyFile.has_key("GUI", "Zoom11Mode")) {
                    Glib::ustring value = keyFile.get_string("GUI", "Zoom11Mode");
                    if (value == "CENTER_CURSOR") {
                        zoom11Mode = Zoom11Mode::CENTER_CURSOR;
                    } else if (value == "PRESERVE_CURSOR") {
                        zoom11Mode = Zoom11Mode::PRESERVE_CURSOR;
                    } else {
                        zoom11Mode = Zoom11Mode::BASIC;
                    }
                }
            }

            if (keyFile.has_group("Crop Settings")) {
                if (keyFile.has_key("Crop Settings", "PPI")) {
                    cropPPI = keyFile.get_integer("Crop Settings", "PPI");
                }

                if (keyFile.has_key("Crop Settings", "GuidesMode")) {
                    cropGuides = CropGuidesMode(std::max(int(CROP_GUIDE_NONE), std::min(keyFile.get_integer("Crop Settings", "GuidesMode"), int(CROP_GUIDE_FULL))));
                }

                if (keyFile.has_key("Crop Settings", "AutoFit")) {
                    cropAutoFit = keyFile.get_boolean("Crop Settings", "AutoFit");
                }
            }

            const Glib::ustring groupRawDecoder = "Raw Decoder";
            if (keyFile.has_group(groupRawDecoder)) {
                const Glib::ustring keyEnableLibRaw = "EnableLibRaw";
                if (keyFile.has_key(groupRawDecoder, keyEnableLibRaw)) {
                    rtSettings.enableLibRaw = keyFile.get_boolean(groupRawDecoder, keyEnableLibRaw);
                }
            }

            if (keyFile.has_group("Color Management")) {
                if (keyFile.has_key("Color Management", "ICCDirectory")) {
                    rtSettings.iccDirectory = keyFile.get_string("Color Management", "ICCDirectory");
                }

                if (keyFile.has_key("Color Management", "PrinterIntent")) {
                    rtSettings.printerIntent = static_cast<rtengine::RenderingIntent>(keyFile.get_integer("Color Management", "PrinterIntent"));
                }

                if (keyFile.has_key("Color Management", "PrinterBPC")) {
                    rtSettings.printerBPC = keyFile.get_boolean("Color Management", "PrinterBPC");
                }

                if (keyFile.has_key("Color Management", "PrinterProfile")) {
                    rtSettings.printerProfile = keyFile.get_string("Color Management", "PrinterProfile");
                }

                if (keyFile.has_key("Color Management", "MonitorProfile")) {
                    rtSettings.monitorProfile = keyFile.get_string("Color Management", "MonitorProfile");
                }

                if (keyFile.has_key("Color Management", "AutoMonitorProfile")) {
                    rtSettings.autoMonitorProfile = keyFile.get_boolean("Color Management", "AutoMonitorProfile");
                }


                if (keyFile.has_key("Color Management", "RGBcurvesLumamode_Gamut")) {
                    rtSettings.rgbcurveslumamode_gamut = keyFile.get_boolean("Color Management", "RGBcurvesLumamode_Gamut");
                }

                if (keyFile.has_key("Color Management", "Intent")) {
                    rtSettings.monitorIntent = static_cast<rtengine::RenderingIntent>(keyFile.get_integer("Color Management", "Intent"));
                }

                if (keyFile.has_key("Color Management", "MonitorBPC")) {
                    rtSettings.monitorBPC = keyFile.get_boolean("Color Management", "MonitorBPC");
                }

                if (keyFile.has_key("Color Management", "Autocielab")) {
                    rtSettings.autocielab = keyFile.get_boolean("Color Management", "Autocielab");
                }

                if (keyFile.has_key("Color Management", "CRI")) {
                    rtSettings.CRI_color = keyFile.get_integer("Color Management", "CRI");
                }

                if (keyFile.has_key("Color Management", "DenoiseLabgamma")) {
                    rtSettings.denoiselabgamma = keyFile.get_integer("Color Management", "DenoiseLabgamma");
                }

                /*
                if (keyFile.has_key ("Color Management", "view")) {
                rtSettings.viewingdevice = keyFile.get_integer ("Color Management", "view");
                }

                if (keyFile.has_key ("Color Management", "grey")) {
                rtSettings.viewingdevicegrey = keyFile.get_integer ("Color Management", "grey");
                }
                */
                /*
                                if (keyFile.has_key ("Color Management", "greySc")) {
                                    rtSettings.viewinggreySc = keyFile.get_integer ("Color Management", "greySc");
                                }
                */

                if (keyFile.has_key("Color Management", "CBDLlevel0")) {
                    rtSettings.level0_cbdl = keyFile.get_double("Color Management", "CBDLlevel0");
                }

                if (keyFile.has_key("Color Management", "CBDLlevel123")) {
                    rtSettings.level123_cbdl = keyFile.get_double("Color Management", "CBDLlevel123");
                }

                if (keyFile.has_key("Color Management", "Itcwb_enable")) {
                    rtSettings.itcwb_enable = keyFile.get_boolean("Color Management", "Itcwb_enable");
                }


                if (keyFile.has_key("Color Management", "Itcwb_deltaspec")) {
                    rtSettings.itcwb_deltaspec = keyFile.get_double("Color Management", "Itcwb_deltaspec");
                }


                if (keyFile.has_key("Color Management", "Itcwb_powponder")) {
                    rtSettings.itcwb_powponder = keyFile.get_double("Color Management", "Itcwb_powponder");
                }


                //if (keyFile.has_key ("Color Management", "Colortoningab")) rtSettings.colortoningab = keyFile.get_double("Color Management", "Colortoningab");
                //if (keyFile.has_key ("Color Management", "Decaction")) rtSettings.decaction = keyFile.get_double("Color Management", "Decaction");

                if (keyFile.has_key("Color Management", "WhiteBalanceSpotSize")) {
                    whiteBalanceSpotSize = keyFile.get_integer("Color Management", "WhiteBalanceSpotSize");
                }

                if (keyFile.has_key("Color Management", "GamutICC")) {
                    rtSettings.gamutICC = keyFile.get_boolean("Color Management", "GamutICC");
                }

                if (keyFile.has_key("Color Management", "AdobeRGB")) {
                    rtSettings.adobe = keyFile.get_string("Color Management", "AdobeRGB");
                    if (rtSettings.adobe == "RT_Medium_gsRGB"  || rtSettings.adobe == "RTv4_Medium") {
                        rtSettings.adobe = "RTv2_Medium";
                    }
                }

                if (keyFile.has_key("Color Management", "ProPhoto")) {
                    rtSettings.prophoto = keyFile.get_string("Color Management", "ProPhoto");
                    if (rtSettings.prophoto == "RT_Large_gBT709"  || rtSettings.prophoto == "RTv4_Large") {
                        rtSettings.prophoto = "RTv2_Large";
                    }
                }

                if (keyFile.has_key("Color Management", "WideGamut")) {
                    rtSettings.widegamut = keyFile.get_string("Color Management", "WideGamut");
                    if (rtSettings.widegamut == "WideGamutRGB"  || rtSettings.widegamut == "RTv4_Wide") {
                        rtSettings.widegamut = "RTv2_Wide";
                    }
                }

                if (keyFile.has_key("Color Management", "DCIP3")) {
                    rtSettings.DCIP3 = keyFile.get_string("Color Management", "DCIP3");
                    if (rtSettings.DCIP3 == "RTv4_DCIP3") {
                        rtSettings.DCIP3 = "RTv2_DCIP3";
                    }
                }

                if (keyFile.has_key("Color Management", "sRGB")) {
                    rtSettings.srgb = keyFile.get_string("Color Management", "sRGB");
                    if (rtSettings.srgb == "RT_sRGB"  || rtSettings.srgb == "RTv2_sRGB") {
                        rtSettings.srgb = "RTv4_sRGB";
                    }
                }

                if (keyFile.has_key("Color Management", "Beta")) {
                    rtSettings.beta = keyFile.get_string("Color Management", "Beta");
                    if (rtSettings.beta == "BetaRGB"  || rtSettings.beta == "RTv4_Beta") {
                        rtSettings.beta = "RTv2_Beta";
                    }
                }

                if (keyFile.has_key("Color Management", "Best")) {
                    rtSettings.best = keyFile.get_string("Color Management", "Best");
                    if (rtSettings.best == "BestRGB" || rtSettings.best == "RTv4_Best") {
                        rtSettings.best = "RTv2_Best";
                    }
                }

                if (keyFile.has_key("Color Management", "Rec2020")) {
                    rtSettings.rec2020 = keyFile.get_string("Color Management", "Rec2020");
                    if (rtSettings.rec2020 == "Rec2020"  || rtSettings.rec2020 == "RTv4_Rec2020") {
                        rtSettings.rec2020 = "RTv2_Rec2020";
                    }
                }

                if (keyFile.has_key("Color Management", "Bruce")) {
                    rtSettings.bruce = keyFile.get_string("Color Management", "Bruce");
                    if (rtSettings.bruce == "Bruce"  || rtSettings.bruce == "RTv4_Bruce") {
                        rtSettings.bruce = "RTv2_Bruce";
                    }
                }

                if (keyFile.has_key("Color Management", "ACES-AP0")) {
                    rtSettings.ACESp0 = keyFile.get_string("Color Management", "ACES-AP0");
                    if (rtSettings.ACESp0 == "RTv4_ACES-AP0") {
                        rtSettings.ACESp0 = "RTv2_ACES-AP0";
                    }

                }

                if (keyFile.has_key("Color Management", "ACES-AP1")) {
                    rtSettings.ACESp1 = keyFile.get_string("Color Management", "ACES-AP1");
                    if (rtSettings.ACESp1 == "RTv4_ACES-AP1") {
                        rtSettings.ACESp1 = "RTv2_ACES-AP1";
                    }

                }

                if (keyFile.has_key("Color Management", "GamutLch")) {
                    rtSettings.gamutLch = keyFile.get_boolean("Color Management", "GamutLch");
                }

                if (keyFile.has_key("Color Management", "ProtectRed")) {
                    rtSettings.protectred = keyFile.get_integer("Color Management", "ProtectRed");
                }

                if (keyFile.has_key("Color Management", "ProtectRedH")) {
                    rtSettings.protectredh = keyFile.get_double("Color Management", "ProtectRedH");
                }

                if (keyFile.has_key("Color Management", "Amountchroma")) {
                    rtSettings.amchroma = keyFile.get_integer("Color Management", "Amountchroma");
                }

                if (keyFile.has_key("Color Management", "JzAmountchroma")) {
                    rtSettings.amchromajz = keyFile.get_integer("Color Management", "JzAmountchroma");
                }

                if (keyFile.has_key("Color Management", "ClutsDirectory")) {
                    clutsDir = keyFile.get_string("Color Management", "ClutsDirectory");
                }

                //if( keyFile.has_key ("Color Management", "Ciebadpixgauss")) rtSettings.ciebadpixgauss = keyFile.get_boolean("Color Management", "Ciebadpixgauss");

                if (keyFile.has_key("Color Management", "Previewselection")) {//Intensity of preview selection deltaE
                    rtSettings.previewselection = keyFile.get_integer("Color Management", "Previewselection");
                }


                if (keyFile.has_key("Color Management", "Cbdlsensi")) {//sensibility to crash for cbdl
                    rtSettings.cbdlsensi = keyFile.get_double("Color Management", "Cbdlsensi");
                }

                if (keyFile.has_key("Color Management", "Besecorlog")) {//sensi base log for Q
                    rtSettings.basecorlog = keyFile.get_double("Color Management", "Basecorlog");
                }


            }

            if (keyFile.has_group("Wavelet")) {
                if (keyFile.has_key("Wavelet", "Edghi")) {
                    rtSettings.edghi = keyFile.get_double("Wavelet", "Edghi");
                }

                if (keyFile.has_key("Wavelet", "Edglo")) {
                    rtSettings.edglo = keyFile.get_double("Wavelet", "Edglo");
                }

                if (keyFile.has_key("Wavelet", "Limrad")) {
                    rtSettings.limrad = keyFile.get_double("Wavelet", "Limrad");
                }

            }


            if (keyFile.has_group("ICC Profile Creator")) {
                if (keyFile.has_key("ICC Profile Creator", "PimariesPreset")) {
                    ICCPC_primariesPreset = keyFile.get_string("ICC Profile Creator", "PimariesPreset");
                }

                if (keyFile.has_key("ICC Profile Creator", "RedPrimaryX")) {
                    ICCPC_redPrimaryX = keyFile.get_double("ICC Profile Creator", "RedPrimaryX");
                }

                if (keyFile.has_key("ICC Profile Creator", "RedPrimaryY")) {
                    ICCPC_redPrimaryY = keyFile.get_double("ICC Profile Creator", "RedPrimaryY");
                }

                if (keyFile.has_key("ICC Profile Creator", "GreenPrimaryX")) {
                    ICCPC_greenPrimaryX = keyFile.get_double("ICC Profile Creator", "GreenPrimaryX");
                }

                if (keyFile.has_key("ICC Profile Creator", "GreenPrimaryY")) {
                    ICCPC_greenPrimaryY = keyFile.get_double("ICC Profile Creator", "GreenPrimaryY");
                }

                if (keyFile.has_key("ICC Profile Creator", "BluePrimaryX")) {
                    ICCPC_bluePrimaryX = keyFile.get_double("ICC Profile Creator", "BluePrimaryX");
                }

                if (keyFile.has_key("ICC Profile Creator", "BluePrimaryY")) {
                    ICCPC_bluePrimaryY = keyFile.get_double("ICC Profile Creator", "BluePrimaryY");
                }

                if (keyFile.has_key("ICC Profile Creator", "GammaPreset")) {
                    ICCPC_gammaPreset = keyFile.get_string("ICC Profile Creator", "GammaPreset");
                }

                if (keyFile.has_key("ICC Profile Creator", "Gamma")) {
                    ICCPC_gamma = keyFile.get_double("ICC Profile Creator", "Gamma");
                }

                if (keyFile.has_key("ICC Profile Creator", "Slope")) {
                    ICCPC_slope = keyFile.get_double("ICC Profile Creator", "Slope");
                }

                if (keyFile.has_key("ICC Profile Creator", "ProfileVersion")) {
                    ICCPC_profileVersion = keyFile.get_string("ICC Profile Creator", "ProfileVersion");
                }

                if (keyFile.has_key("ICC Profile Creator", "Illuminant")) {
                    ICCPC_illuminant = keyFile.get_string("ICC Profile Creator", "Illuminant");
                }

                if (keyFile.has_key("ICC Profile Creator", "Description")) {
                    ICCPC_description = keyFile.get_string("ICC Profile Creator", "Description");
                }

                if (keyFile.has_key("ICC Profile Creator", "Copyright")) {
                    ICCPC_copyright = keyFile.get_string("ICC Profile Creator", "Copyright");
                }

                if (keyFile.has_key("ICC Profile Creator", "AppendParamsToDesc")) {
                    ICCPC_appendParamsToDesc = keyFile.get_boolean("ICC Profile Creator", "AppendParamsToDesc");
                }
            }

            if (keyFile.has_group("Batch Processing")) {
                if (keyFile.has_key("Batch Processing", "AdjusterBehavior")) {
                    baBehav = keyFile.get_integer_list("Batch Processing", "AdjusterBehavior");
                    baBehav.resize(ADDSET_PARAM_NUM);
                }
            }

            if (keyFile.has_group("Sounds")) {
                if (keyFile.has_key("Sounds", "Enable")) {
                    sndEnable = keyFile.get_boolean("Sounds", "Enable");
                }

                if (keyFile.has_key("Sounds", "BatchQueueDone")) {
                    sndBatchQueueDone = keyFile.get_string("Sounds", "BatchQueueDone");
                }

                if (keyFile.has_key("Sounds", "LngEditProcDone")) {
                    sndLngEditProcDone = keyFile.get_string("Sounds", "LngEditProcDone");
                }

                if (keyFile.has_key("Sounds", "LngEditProcDoneSecs")) {
                    sndLngEditProcDoneSecs = keyFile.get_double("Sounds", "LngEditProcDoneSecs");
                }
            }

            if (keyFile.has_group("Fast Export")) {
                if (keyFile.has_key("Fast Export", "fastexport_bypass_sharpening")) {
                    fastexport_bypass_sharpening = keyFile.get_boolean("Fast Export", "fastexport_bypass_sharpening");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_sharpenEdge")) {
                    fastexport_bypass_sharpenEdge = keyFile.get_boolean("Fast Export", "fastexport_bypass_sharpenEdge");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_sharpenMicro")) {
                    fastexport_bypass_sharpenMicro = keyFile.get_boolean("Fast Export", "fastexport_bypass_sharpenMicro");
                }

                //if (keyFile.has_key ("Fast Export", "fastexport_bypass_lumaDenoise" )) fastexport_bypass_lumaDenoise = keyFile.get_boolean ("Fast Export", "fastexport_bypass_lumaDenoise" );
                //if (keyFile.has_key ("Fast Export", "fastexport_bypass_colorDenoise" )) fastexport_bypass_colorDenoise = keyFile.get_boolean ("Fast Export", "fastexport_bypass_colorDenoise" );
                if (keyFile.has_key("Fast Export", "fastexport_bypass_defringe")) {
                    fastexport_bypass_defringe = keyFile.get_boolean("Fast Export", "fastexport_bypass_defringe");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_dirpyrDenoise")) {
                    fastexport_bypass_dirpyrDenoise = keyFile.get_boolean("Fast Export", "fastexport_bypass_dirpyrDenoise");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_dirpyrequalizer")) {
                    fastexport_bypass_dirpyrequalizer = keyFile.get_boolean("Fast Export", "fastexport_bypass_dirpyrequalizer");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_wavelet")) {
                    fastexport_bypass_wavelet = keyFile.get_boolean("Fast Export", "fastexport_bypass_wavelet");
                }

                if (keyFile.has_key("Fast Export", "fastexport_raw_dmethod")) {
                    fastexport_raw_bayer_method = keyFile.get_string("Fast Export", "fastexport_raw_dmethod");
                }

                if (keyFile.has_key("Fast Export", "fastexport_raw_bayer_method")) {
                    fastexport_raw_bayer_method = keyFile.get_string("Fast Export", "fastexport_raw_bayer_method");
                }

//if (keyFile.has_key ("Fast Export", "fastexport_bypass_raw_bayer_all_enhance" )) fastexport_bypass_raw_bayer_all_enhance = keyFile.get_boolean ("Fast Export", "fastexport_bypass_raw_all_enhance" );
                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_dcb_iterations")) {
                    fastexport_bypass_raw_bayer_dcb_iterations = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_dcb_iterations");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_bayer_dcb_iterations")) {
                    fastexport_bypass_raw_bayer_dcb_iterations = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_bayer_dcb_iterations");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_dcb_enhance")) {
                    fastexport_bypass_raw_bayer_dcb_enhance = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_dcb_enhance");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_bayer_dcb_enhance")) {
                    fastexport_bypass_raw_bayer_dcb_enhance = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_bayer_dcb_enhance");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_lmmse_iterations")) {
                    fastexport_bypass_raw_bayer_lmmse_iterations = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_lmmse_iterations");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_bayer_lmmse_iterations")) {
                    fastexport_bypass_raw_bayer_lmmse_iterations = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_bayer_lmmse_iterations");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_linenoise")) {
                    fastexport_bypass_raw_bayer_linenoise = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_linenoise");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_bayer_linenoise")) {
                    fastexport_bypass_raw_bayer_linenoise = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_bayer_linenoise");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_greenthresh")) {
                    fastexport_bypass_raw_bayer_greenthresh = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_greenthresh");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_bayer_greenthresh")) {
                    fastexport_bypass_raw_bayer_greenthresh = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_bayer_greenthresh");
                }

                if (keyFile.has_key("Fast Export", "fastexport_raw_xtrans_method")) {
                    fastexport_raw_xtrans_method = keyFile.get_string("Fast Export", "fastexport_raw_xtrans_method");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_ccSteps")) {
                    fastexport_bypass_raw_ccSteps = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_ccSteps");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_ca")) {
                    fastexport_bypass_raw_ca = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_ca");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_df")) {
                    fastexport_bypass_raw_df = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_df");
                }

                if (keyFile.has_key("Fast Export", "fastexport_bypass_raw_ff")) {
                    fastexport_bypass_raw_ff = keyFile.get_boolean("Fast Export", "fastexport_bypass_raw_ff");
                }

                if (keyFile.has_key("Fast Export", "fastexport_icm_input")) {
                    fastexport_icm_input_profile = keyFile.get_string("Fast Export", "fastexport_icm_input");
                }

                if (keyFile.has_key("Fast Export", "fastexport_icm_working")) {
                    fastexport_icm_working_profile = keyFile.get_string("Fast Export", "fastexport_icm_working");
                }

                if (keyFile.has_key("Fast Export", "fastexport_icm_output")) {
                    fastexport_icm_output_profile = keyFile.get_string("Fast Export", "fastexport_icm_output");
                }

                if (keyFile.has_key("Fast Export", "fastexport_icm_output_intent")) {
                    fastexport_icm_outputIntent = static_cast<rtengine::RenderingIntent>(keyFile.get_integer("Fast Export", "fastexport_icm_output_intent"));
                }

                if (keyFile.has_key("Fast Export", "fastexport_icm_output_bpc")) {
                    fastexport_icm_outputBPC = keyFile.get_boolean("Fast Export", "fastexport_icm_output_bpc");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_enabled")) {
                    fastexport_resize_enabled = keyFile.get_boolean("Fast Export", "fastexport_resize_enabled");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_scale")) {
                    fastexport_resize_scale = keyFile.get_double("Fast Export", "fastexport_resize_scale");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_appliesTo")) {
                    fastexport_resize_appliesTo = keyFile.get_string("Fast Export", "fastexport_resize_appliesTo");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_method")) {
                    fastexport_resize_method = keyFile.get_string("Fast Export", "fastexport_resize_method");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_dataspec")) {
                    fastexport_resize_dataspec = keyFile.get_integer("Fast Export", "fastexport_resize_dataspec");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_width")) {
                    fastexport_resize_width = keyFile.get_integer("Fast Export", "fastexport_resize_width");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_height")) {
                    fastexport_resize_height = keyFile.get_integer("Fast Export", "fastexport_resize_height");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_longedge")) {
                    fastexport_resize_longedge = keyFile.get_integer("Fast Export", "fastexport_resize_longedge");
                }

                if (keyFile.has_key("Fast Export", "fastexport_resize_shortedge")) {
                    fastexport_resize_shortedge = keyFile.get_integer("Fast Export", "fastexport_resize_shortedge");
                }

                if (keyFile.has_key("Fast Export", "fastexport_use_fast_pipeline")) {
                    fastexport_use_fast_pipeline = keyFile.get_integer("Fast Export", "fastexport_use_fast_pipeline");
                }
            }

            if (keyFile.has_group("Dialogs")) {
                safeDirGet(keyFile, "Dialogs", "LastIccDir", lastIccDir);
                safeDirGet(keyFile, "Dialogs", "LastDarkframeDir", lastDarkframeDir);
                safeDirGet(keyFile, "Dialogs", "LastFlatfieldDir", lastFlatfieldDir);
                safeDirGet(keyFile, "Dialogs", "LastCameraProfilesDir", lastCameraProfilesDir);
                safeDirGet(keyFile, "Dialogs", "LastLensProfilesDir", lastLensProfilesDir);
                safeDirGet(keyFile, "Dialogs", "LastRgbCurvesDir", lastRgbCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastLabCurvesDir", lastLabCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastRetinexDir", lastRetinexDir);
                safeDirGet(keyFile, "Dialogs", "LastDenoiseCurvesDir", lastDenoiseCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastWaveletCurvesDir", lastWaveletCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastlocalCurvesDir", lastlocalCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastlocalCurvesDirghs", lastlocalCurvesDirghs);

                safeDirGet(keyFile, "Dialogs", "LastPFCurvesDir", lastPFCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastHsvCurvesDir", lastHsvCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastBWCurvesDir", lastBWCurvesDir);

                safeDirGet(keyFile, "Dialogs", "LastToneCurvesDir", lastToneCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastIcmCurvesDir", lastIcmCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastVibranceCurvesDir", lastVibranceCurvesDir);
                safeDirGet(keyFile, "Dialogs", "LastProfilingReferenceDir", lastProfilingReferenceDir);
                safeDirGet(keyFile, "Dialogs", "LastLensProfileDir", lastLensProfileDir);
                safeDirGet(keyFile, "Dialogs", "LastICCProfCreatorDir", lastICCProfCreatorDir);
                safeDirGet(keyFile, "Dialogs", "LastCopyMovePath", lastCopyMovePath);

                if (keyFile.has_key("Dialogs", "GimpPluginShowInfoDialog")) {
                    gimpPluginShowInfoDialog = keyFile.get_boolean("Dialogs", "GimpPluginShowInfoDialog");
                }
            }

            if (keyFile.has_group("Lensfun")) {
                if (keyFile.has_key("Lensfun", "DBDirectory")) {
                    rtSettings.lensfunDbDirectory = keyFile.get_string("Lensfun", "DBDirectory");
                }
            }

            if (keyFile.has_group("Metadata")) {
                if (keyFile.has_key("Metadata", "XMPSidecarStyle")) {
                    std::string val = keyFile.get_string("Metadata", "XMPSidecarStyle");
                    if (val == "ext") {
                        rtSettings.xmp_sidecar_style = rtengine::Settings::XmpSidecarStyle::EXT;
                    } else {
                        rtSettings.xmp_sidecar_style = rtengine::Settings::XmpSidecarStyle::STD;
                    }
                }
                if (keyFile.has_key("Metadata", "XMPSynchronization")) {
                    std::string val = keyFile.get_string("Metadata", "XMPSynchronization");
                    if (val == "read") {
                        rtSettings.metadata_xmp_sync = rtengine::Settings::MetadataXmpSync::READ;
                    } else if (val == "readwrite") {
                        rtSettings.metadata_xmp_sync = rtengine::Settings::MetadataXmpSync::READ_WRITE;
                    } else {
                        rtSettings.metadata_xmp_sync = rtengine::Settings::MetadataXmpSync::NONE;
                    }
                }
            }

// --------------------------------------------------------------------------------------------------------

            filterOutParsedExtensions();

            return;

        }
    } catch (Glib::Error &err) {
        Glib::ustring msg = Glib::ustring::compose("Options::readFromFile / Error code %1 while reading values from \"%2\":\n%3", err.code(), fname, err.what());

        if (App::get().options().rtSettings.verbose) {
            printf("%s\n", msg.c_str());
        }

        throw Error(msg);
    } catch (...) {
        Glib::ustring msg = Glib::ustring::compose("Options::readFromFile / Unknown exception while trying to load \"%1\"!", fname);

        if (App::get().options().rtSettings.verbose) {
            printf("%s\n", msg.c_str());
        }

        throw Error(msg);
    }
}

bool Options::safeDirGet(const Glib::KeyFile& keyFile, const Glib::ustring& section,
                         const Glib::ustring& entryName, Glib::ustring& destination)
{
    try {

        if (keyFile.has_key(section, entryName) && !keyFile.get_string(section, entryName).empty()) {
            destination = keyFile.get_string(section, entryName);
            return true;
        }

    } catch (Glib::KeyFileError&) {}

    return false;
}

void Options::saveToFile(Glib::ustring fname)
{

    Glib::ustring keyData;

    try {

        Glib::KeyFile keyFile;

        keyFile.set_boolean("General", "TabbedEditor", tabbedUI);
        keyFile.set_boolean("General", "StoreLastProfile", savesParamsAtExit);

        if (startupDir == STARTUPDIR_HOME) {
            keyFile.set_string("General", "StartupDirectory", "home");
        } else if (startupDir == STARTUPDIR_CURRENT) {
            keyFile.set_string("General", "StartupDirectory", "current");
        } else if (startupDir == STARTUPDIR_CUSTOM) {
            keyFile.set_string("General", "StartupDirectory", "custom");
        } else if (startupDir == STARTUPDIR_LAST) {
            keyFile.set_string("General", "StartupDirectory", "last");
        }

        keyFile.set_string("General", "StartupPath", startupPath);
        keyFile.set_string("General", "DateFormat", dateFormat);
        keyFile.set_integer("General", "AdjusterMinDelay", adjusterMinDelay);
        keyFile.set_integer("General", "AdjusterMaxDelay", adjusterMaxDelay);
        keyFile.set_boolean("General", "MultiUser", multiUser);
        keyFile.set_string("General", "Language", language);
        keyFile.set_boolean("General", "LanguageAutoDetect", languageAutoDetect);
        keyFile.set_string("General", "Theme", theme);
        keyFile.set_string("General", "Version", RTVERSION);
        keyFile.set_string("General", "DarkFramesPath", rtSettings.darkFramesPath);
        keyFile.set_string("General", "FlatFieldsPath", rtSettings.flatFieldsPath);
        keyFile.set_string("General", "CameraProfilesPath", rtSettings.cameraProfilesPath);
        keyFile.set_string("General", "LensProfilesPath", rtSettings.lensProfilesPath);
        keyFile.set_boolean("General", "Verbose", rtSettings.verbose);
        keyFile.set_integer("General", "Cropsleep", rtSettings.cropsleep);
        keyFile.set_double("General", "Reduchigh", rtSettings.reduchigh);
        keyFile.set_double("General", "Reduclow", rtSettings.reduclow);
        keyFile.set_boolean("General", "Detectshape", rtSettings.detectshape);
        keyFile.set_boolean("General", "Fftwsigma", rtSettings.fftwsigma);

        // TODO: Remove.
        keyFile.set_integer("External Editor", "EditorKind", editorToSendTo);
        keyFile.set_string("External Editor", "GimpDir", gimpDir);
        keyFile.set_string("External Editor", "PhotoshopDir", psDir);
        keyFile.set_string("External Editor", "CustomEditor", customEditorProg);
        keyFile.set_integer("External Editor", "OutputDir", int(editor_out_dir));
        keyFile.set_string("External Editor", "CustomOutputDir", editor_custom_out_dir);
        keyFile.set_boolean("External Editor", "Float32", editor_float32);
        keyFile.set_boolean("External Editor", "BypassOutputProfile", editor_bypass_output_profile);

        {
        std::vector<Glib::ustring> names;
        std::vector<Glib::ustring> commands;
        std::vector<bool> native_commands;
        std::vector<Glib::ustring> icons_serialized;

        for (const auto & editor : externalEditors) {
            names.push_back(editor.name);
            commands.push_back(editor.command);
            native_commands.push_back(editor.native_command);
            icons_serialized.push_back(editor.icon_serialized);
        }

        keyFile.set_string_list("External Editor", "Names", names);
        keyFile.set_string_list("External Editor", "Commands", commands);
        keyFile.set_boolean_list("External Editor", "NativeCommands", native_commands);
        keyFile.set_string_list("External Editor", "IconsSerialized", icons_serialized);

        keyFile.set_integer("External Editor", "EditorIndex", externalEditorIndex);
        }

        keyFile.set_boolean("File Browser", "BrowseOnlyRaw", fbOnlyRaw);
        keyFile.set_boolean("File Browser", "BrowserShowsDate", fbShowDateTime);
        keyFile.set_boolean("File Browser", "BrowserShowsExif", fbShowBasicExif);
        keyFile.set_boolean("File Browser", "BrowserShowsExpComp", fbShowExpComp);
#ifndef _WIN32
        keyFile.set_boolean("File Browser", "BrowserShowsHidden", fbShowHidden);
#endif
        keyFile.set_integer("File Browser", "ThumbnailSize", thumbSize);
        keyFile.set_integer("File Browser", "ThumbnailSizeTab", thumbSizeTab);
        keyFile.set_integer("File Browser", "ThumbnailSizeQueue", thumbSizeQueue);
        keyFile.set_integer("File Browser", "SameThumbSize", sameThumbSize);
        keyFile.set_integer("File Browser", "MaxPreviewHeight", maxThumbnailHeight);
        keyFile.set_integer("File Browser", "MaxPreviewWidth", maxThumbnailWidth);
        keyFile.set_integer("File Browser", "MaxCacheEntries", maxCacheEntries);
        Glib::ArrayHandle<Glib::ustring> pext = parseExtensions;
        keyFile.set_string_list("File Browser", "ParseExtensions", pext);
        Glib::ArrayHandle<int> pextena = parseExtensionsEnabled;
        keyFile.set_integer_list("File Browser", "ParseExtensionsEnabled", pextena);
        keyFile.set_integer("File Browser", "ThumbnailArrangement", fbArrangement);
        keyFile.set_integer("File Browser", "ThumbnailInterpolation", thumbInterp);
        Glib::ArrayHandle<Glib::ustring> pfav = favoriteDirs;
        keyFile.set_string_list("File Browser", "FavoriteDirs", pfav);
        Glib::ArrayHandle<Glib::ustring> pren = renameTemplates;
        keyFile.set_string_list("File Browser", "RenameTemplates", pren);
        keyFile.set_boolean("File Browser", "RenameUseTemplates", renameUseTemplates);
        Glib::ArrayHandle<double> ptzoom = thumbnailZoomRatios;
        keyFile.set_double_list("File Browser", "ThumbnailZoomRatios", ptzoom);
        keyFile.set_boolean("File Browser", "OverlayedFileNames", overlayedFileNames);
        keyFile.set_boolean("File Browser", "FilmStripOverlayedFileNames", filmStripOverlayedFileNames);
        keyFile.set_boolean("File Browser", "ShowFileNames", showFileNames);
        keyFile.set_boolean("File Browser", "FilmStripShowFileNames", filmStripShowFileNames);
        keyFile.set_boolean("File Browser", "InternalThumbIfUntouched", internalThumbIfUntouched);
        keyFile.set_boolean("File Browser", "menuGroupRank", menuGroupRank);
        keyFile.set_boolean("File Browser", "menuGroupLabel", menuGroupLabel);
        keyFile.set_boolean("File Browser", "menuGroupFileOperations", menuGroupFileOperations);
        keyFile.set_boolean("File Browser", "menuGroupProfileOperations", menuGroupProfileOperations);
        keyFile.set_boolean("File Browser", "menuGroupExtProg", menuGroupExtProg);
        keyFile.set_integer("File Browser", "MaxRecentFolders", maxRecentFolders);
        {
            std::vector<Glib::ustring> temp;
            temp.reserve(maxRecentFolders);

            for (unsigned int i = 0; i < std::min(recentFolders.size(), maxRecentFolders); i++) {
                temp.push_back(recentFolders[i]);
            }

            keyFile.set_string_list("File Browser", "RecentFolders", temp);
        }
        switch (thumbnailRankColorMode) {
        case ThumbnailPropertyMode::XMP:
            keyFile.set_string("File Browser", "ThumbnailRankColorMode", "xmp");
            break;
        default: // ThumbnailPropertyMode::PROCPARAMS
            keyFile.set_string("File Browser", "ThumbnailRankColorMode", "procparams");
            break;
        }
        keyFile.set_integer("File Browser", "SortMethod", sortMethod);
        keyFile.set_boolean("File Browser", "SortDescending", sortDescending);
        keyFile.set_boolean("File Browser", "BrowseRecursive", browseRecursive);
        keyFile.set_integer("File Browser", "BrowseRecursiveDepth", browseRecursiveDepth);
        keyFile.set_integer("File Browser", "BrowseRecursiveMaxDirs", browseRecursiveMaxDirs);
        keyFile.set_boolean("File Browser", "BrowseRecursiveFollowLinks", browseRecursiveFollowLinks);
        keyFile.set_integer("Clipping Indication", "HighlightThreshold", highlightThreshold);
        keyFile.set_integer("Clipping Indication", "ShadowThreshold", shadowThreshold);
        keyFile.set_boolean("Clipping Indication", "BlinkClipped", blinkClipped);

        keyFile.set_integer("Performance", "RgbDenoiseThreadLimit", rgbDenoiseThreadLimit);
        keyFile.set_integer("Performance", "ClutCacheSize", clutCacheSize);
        keyFile.set_integer("Performance", "MaxInspectorBuffers", maxInspectorBuffers);
        keyFile.set_integer("Performance", "InspectorDelay", inspectorDelay);
        keyFile.set_integer("Performance", "PreviewDemosaicFromSidecar", prevdemo);
        keyFile.set_boolean("Performance", "SerializeTiffRead", serializeTiffRead);
        keyFile.set_integer("Performance", "Measure", measure);
        keyFile.set_integer("Performance", "ChunkSizeAMAZE", chunkSizeAMAZE);
        keyFile.set_integer("Performance", "ChunkSizeRCD", chunkSizeRCD);
        keyFile.set_integer("Performance", "ChunkSizeRGB", chunkSizeRGB);
        keyFile.set_integer("Performance", "ChunkSizeXT", chunkSizeXT);
        keyFile.set_integer("Performance", "ChunkSizeCA", chunkSizeCA);
        keyFile.set_integer("Performance", "ThumbnailInspectorMode", int(rtSettings.thumbnail_inspector_mode));


        keyFile.set_string("Output", "Format", saveFormat.format);
        keyFile.set_integer("Output", "JpegQuality", saveFormat.jpegQuality);
        keyFile.set_integer("Output", "JpegSubSamp", saveFormat.jpegSubSamp);
        keyFile.set_integer("Output", "PngBps", saveFormat.pngBits);
        keyFile.set_integer("Output", "TiffBps", saveFormat.tiffBits);
        keyFile.set_boolean("Output", "TiffFloat", saveFormat.tiffFloat);
        keyFile.set_boolean("Output", "TiffUncompressed", saveFormat.tiffUncompressed);
        keyFile.set_boolean("Output", "BigTiff", saveFormat.bigTiff);
        keyFile.set_boolean("Output", "SaveProcParams", saveFormat.saveParams);

        keyFile.set_string("Output", "FormatBatch", saveFormatBatch.format);
        keyFile.set_integer("Output", "JpegQualityBatch", saveFormatBatch.jpegQuality);
        keyFile.set_integer("Output", "JpegSubSampBatch", saveFormatBatch.jpegSubSamp);
        keyFile.set_integer("Output", "PngBpsBatch", saveFormatBatch.pngBits);
        keyFile.set_integer("Output", "TiffBpsBatch", saveFormatBatch.tiffBits);
        keyFile.set_boolean("Output", "TiffFloatBatch", saveFormatBatch.tiffFloat);
        keyFile.set_boolean("Output", "TiffUncompressedBatch", saveFormatBatch.tiffUncompressed);
        keyFile.set_boolean("Output", "SaveProcParamsBatch", saveFormatBatch.saveParams);

        keyFile.set_string("Output", "PathTemplate", savePathTemplate);
        keyFile.set_string("Output", "PathFolder", savePathFolder);
        keyFile.set_boolean("Output", "AutoSuffix", autoSuffix);
        keyFile.set_boolean("Output", "ForceFormatOpts", forceFormatOpts);
        keyFile.set_integer("Output", "SaveMethodNum", saveMethodNum);
        keyFile.set_boolean("Output", "UsePathTemplate", saveUsePathTemplate);
        keyFile.set_string("Output", "LastSaveAsPath", lastSaveAsPath);
        keyFile.set_boolean("Output", "OverwriteOutputFile", overwriteOutputFile);

        keyFile.set_string("Profiles", "Directory", profilePath);
        keyFile.set_boolean("Profiles", "UseBundledProfiles", useBundledProfiles);
        keyFile.set_string("Profiles", "LoadSaveProfilePath", loadSaveProfilePath);
        keyFile.set_string("Profiles", "RawDefault", defProfRaw);
        keyFile.set_string("Profiles", "ImgDefault", defProfImg);
        keyFile.set_boolean("Profiles", "FilledProfile", filledProfile);
        keyFile.set_boolean("Profiles", "SaveParamsWithFile", saveParamsFile);
        keyFile.set_boolean("Profiles", "SaveParamsToCache", saveParamsCache);
        keyFile.set_integer("Profiles", "LoadParamsFromLocation", paramsLoadLocation);
        keyFile.set_string("Profiles", "CustomProfileBuilderPath", CPBPath);
        keyFile.set_integer("Profiles", "CustomProfileBuilderKeys", CPBKeys);

        Glib::ArrayHandle<Glib::ustring> ahfavorites = favorites;
        keyFile.set_string_list("GUI", "Favorites", ahfavorites);
        keyFile.set_boolean("GUI", "FavoritesCloneTools", cloneFavoriteTools);
        keyFile.set_integer("GUI", "WindowWidth", windowWidth);
        keyFile.set_integer("GUI", "WindowHeight", windowHeight);
        keyFile.set_integer("GUI", "WindowX", windowX);
        keyFile.set_integer("GUI", "WindowY", windowY);
        keyFile.set_integer("GUI", "WindowMonitor", windowMonitor);
        keyFile.set_integer("GUI", "MeowMonitor", meowMonitor);
        keyFile.set_boolean("GUI", "MeowMaximized", meowMaximized);
        keyFile.set_integer("GUI", "MeowWidth", meowWidth);
        keyFile.set_integer("GUI", "MeowHeight", meowHeight);
        keyFile.set_integer("GUI", "MeowX", meowX);
        keyFile.set_integer("GUI", "MeowY", meowY);
        keyFile.set_boolean("GUI", "WindowMaximized", windowMaximized);
        keyFile.set_integer("GUI", "DetailWindowWidth", detailWindowWidth);
        keyFile.set_integer("GUI", "DetailWindowHeight", detailWindowHeight);
        keyFile.set_integer("GUI", "DirBrowserWidth", dirBrowserWidth);
        keyFile.set_integer("GUI", "DirBrowserHeight", dirBrowserHeight);
        keyFile.set_integer("GUI", "SortType", dirBrowserSortType);
        keyFile.set_integer("GUI", "PreferencesWidth", preferencesWidth);
        keyFile.set_integer("GUI", "PreferencesHeight", preferencesHeight);
        keyFile.set_integer("GUI", "SaveAsDialogWidth", saveAsDialogWidth);
        keyFile.set_integer("GUI", "SaveAsDialogHeight", saveAsDialogHeight);
        keyFile.set_integer("GUI", "ToolPanelWidth", toolPanelWidth);
        keyFile.set_integer("GUI", "BrowserToolPanelWidth", browserToolPanelWidth);
        keyFile.set_integer("GUI", "BrowserToolPanelHeight", browserToolPanelHeight);
        keyFile.set_boolean("GUI", "BrowserToolPanelOpened", browserToolPanelOpened);
        keyFile.set_boolean("GUI", "EditorFilmStripOpened", editorFilmStripOpened);
        keyFile.set_boolean("GUI", "BrowserDirPanelOpened", browserDirPanelOpened);
        keyFile.set_integer("GUI", "HistoryPanelWidth", historyPanelWidth);
        keyFile.set_string("GUI", "FontFamily", fontFamily);
        keyFile.set_integer("GUI", "FontSize", fontSize);
        keyFile.set_string("GUI", "CPFontFamily", CPFontFamily);
        keyFile.set_integer("GUI", "CPFontSize", CPFontSize);
        keyFile.set_integer("GUI", "LastPreviewScale", lastScale);
        keyFile.set_boolean("GUI", "LastShowAllExif", lastShowAllExif);
        keyFile.set_integer("GUI", "PanAccelFactor", panAccelFactor);
        keyFile.set_boolean("GUI", "RememberZoomAndPan", rememberZoomAndPan);
        keyFile.set_integer("GUI", "LastCropSize", lastCropSize);
        keyFile.set_boolean("GUI", "ShowHistory", showHistory);
        keyFile.set_integer("GUI", "ShowFilePanelState", showFilePanelState);
        keyFile.set_boolean("GUI", "ShowInfo", showInfo);
        keyFile.set_boolean("GUI", "MainNBVertical", mainNBVertical);
        keyFile.set_boolean("GUI", "ShowClippedHighlights", showClippedHighlights);
        keyFile.set_boolean("GUI", "ShowClippedShadows", showClippedShadows);
        keyFile.set_integer("GUI", "FrameColor", bgcolor);
        keyFile.set_boolean("GUI", "ProcessingQueueEnbled", procQueueEnabled);
        Glib::ArrayHandle<int> tpopen = tpOpen;
        keyFile.set_integer_list("GUI", "ToolPanelsExpanded", tpopen);
        keyFile.set_boolean("GUI", "ToolPanelsExpandedAutoSave", autoSaveTpOpen);
        keyFile.set_integer("GUI", "MultiDisplayMode", multiDisplayMode);
        keyFile.set_double_list("GUI", "CutOverlayBrush", cutOverlayBrush);
        keyFile.set_double_list("GUI", "NavGuideBrush", navGuideBrush);
        keyFile.set_integer("GUI", "HistogramPosition", histogramPosition);
        keyFile.set_boolean("GUI", "HistogramRed", histogramRed);
        keyFile.set_boolean("GUI", "HistogramGreen", histogramGreen);
        keyFile.set_boolean("GUI", "HistogramBlue", histogramBlue);
        keyFile.set_boolean("GUI", "HistogramLuma", histogramLuma);
        keyFile.set_boolean("GUI", "HistogramChroma", histogramChroma);
        keyFile.set_boolean("GUI", "HistogramBar", histogramBar);
        keyFile.set_integer("GUI", "HistogramHeight", histogramHeight);
        keyFile.set_integer("GUI", "HistogramDrawMode", histogramDrawMode);
        keyFile.set_integer("GUI", "HistogramScopeType", rtengine::toUnderlying(histogramScopeType));
        keyFile.set_boolean("GUI", "HistogramShowOptionButtons", histogramShowOptionButtons);
        keyFile.set_double("GUI", "HistogramTraceBrightness", histogramTraceBrightness);
        keyFile.set_integer("GUI", "NavigatorRGBUnit", (int)navRGBUnit);
        keyFile.set_integer("GUI", "NavigatorHSVUnit", (int)navHSVUnit);
        keyFile.set_boolean("GUI", "ShowFilmStripToolBar", showFilmStripToolBar);
        keyFile.set_boolean("GUI", "FileBrowserToolbarSingleRow", FileBrowserToolbarSingleRow);
        keyFile.set_boolean("GUI", "HideTPVScrollbar", hideTPVScrollbar);
        keyFile.set_boolean("GUI", "HistogramWorking", rtSettings.HistogramWorking);
        keyFile.set_integer("GUI", "CurveBBoxPosition", curvebboxpos);
        keyFile.set_boolean("GUI", "Showtooltip", showtooltip);
        keyFile.set_integer("GUI", "Complexity", complexity);
        keyFile.set_integer("GUI", "Spotmet", spotmet);

        keyFile.set_boolean("GUI", "ShowInspectorObservedArea",
                            showInspectorObservedArea);
        keyFile.set_boolean("GUI", "InspectorWindow", inspectorWindow);
        keyFile.set_boolean("GUI", "PinInspectorWindow", pinInspector);
        keyFile.set_boolean("GUI", "ZoomOnScroll", zoomOnScroll);
        keyFile.set_integer("GUI", "MaxZoom", static_cast<int>(maxZoomLimit));
        keyFile.set_boolean("GUI", "ReverseDiscreteScrollDirection",
                            reverseDiscreteScrollDir);
        keyFile.set_boolean("GUI", "ReverseSmoothScrollDirection",
                            reverseSmoothScrollDir);
        keyFile.set_integer("GUI", "SmoothScrollZoomSensitivity",
                            smoothScrollZoomSensitivity);
        keyFile.set_integer("GUI", "SmoothScrollPanSensitivity",
                            smoothScrollPanSensitivity);
        keyFile.set_string("GUI", "Zoom11Mode",
            [&]() {
                switch (zoom11Mode) {
                    case Zoom11Mode::CENTER_CURSOR: return "CENTER_CURSOR";
                    case Zoom11Mode::PRESERVE_CURSOR: return "PRESERVE_CURSOR";
                    case Zoom11Mode::BASIC:
                    default:
                        return "BASIC";
                };
            }());

        //Glib::ArrayHandle<int> crvopen = crvOpen;
        //keyFile.set_integer_list ("GUI", "CurvePanelsExpanded", crvopen);

        keyFile.set_integer("Crop Settings", "PPI", cropPPI);
        keyFile.set_integer("Crop Settings", "GuidesMode", cropGuides);
        keyFile.set_boolean("Crop Settings", "AutoFit", cropAutoFit);

        keyFile.set_boolean("Raw Decoder", "EnableLibRaw", rtSettings.enableLibRaw);

        keyFile.set_string("Color Management", "PrinterProfile", rtSettings.printerProfile);
        keyFile.set_integer("Color Management", "PrinterIntent", rtSettings.printerIntent);
        keyFile.set_boolean("Color Management", "PrinterBPC", rtSettings.printerBPC);

        keyFile.set_string("Color Management", "ICCDirectory", rtSettings.iccDirectory);
        keyFile.set_string("Color Management", "MonitorProfile", rtSettings.monitorProfile);
        keyFile.set_boolean("Color Management", "AutoMonitorProfile", rtSettings.autoMonitorProfile);
        keyFile.set_boolean("Color Management", "Autocielab", rtSettings.autocielab);
        keyFile.set_boolean("Color Management", "RGBcurvesLumamode_Gamut", rtSettings.rgbcurveslumamode_gamut);
        keyFile.set_integer("Color Management", "Intent", rtSettings.monitorIntent);
        keyFile.set_boolean("Color Management", "MonitorBPC", rtSettings.monitorBPC);


        //keyFile.set_integer ("Color Management", "view", rtSettings.viewingdevice);
        //keyFile.set_integer ("Color Management", "grey", rtSettings.viewingdevicegrey);
//        keyFile.set_integer ("Color Management", "greySc", rtSettings.viewinggreySc);

        keyFile.set_string("Color Management", "AdobeRGB", rtSettings.adobe);
        keyFile.set_string("Color Management", "ProPhoto", rtSettings.prophoto);
        keyFile.set_string("Color Management", "WideGamut", rtSettings.widegamut);
        keyFile.set_string("Color Management", "DCIP3", rtSettings.DCIP3);
        keyFile.set_string("Color Management", "sRGB", rtSettings.srgb);
        keyFile.set_string("Color Management", "Beta", rtSettings.beta);
        keyFile.set_string("Color Management", "Best", rtSettings.best);
        keyFile.set_string("Color Management", "Rec2020", rtSettings.rec2020);
        keyFile.set_string("Color Management", "Bruce", rtSettings.bruce);
        keyFile.set_string("Color Management", "ACES-AP0", rtSettings.ACESp0);
        keyFile.set_string("Color Management", "ACES-AP1", rtSettings.ACESp1);
        keyFile.set_integer("Color Management", "WhiteBalanceSpotSize", whiteBalanceSpotSize);
        keyFile.set_boolean("Color Management", "GamutICC", rtSettings.gamutICC);
        keyFile.set_boolean("Color Management", "GamutLch", rtSettings.gamutLch);
        keyFile.set_integer("Color Management", "ProtectRed", rtSettings.protectred);
        keyFile.set_integer("Color Management", "Amountchroma", rtSettings.amchroma);
        keyFile.set_integer("Color Management", "JzAmountchroma", rtSettings.amchromajz);
        keyFile.set_double("Color Management", "ProtectRedH", rtSettings.protectredh);
        keyFile.set_integer("Color Management", "CRI", rtSettings.CRI_color);
        keyFile.set_integer("Color Management", "DenoiseLabgamma", rtSettings.denoiselabgamma);
        //keyFile.set_boolean ("Color Management", "Ciebadpixgauss", rtSettings.ciebadpixgauss);
        keyFile.set_double("Color Management", "CBDLlevel0", rtSettings.level0_cbdl);
        keyFile.set_double("Color Management", "CBDLlevel123", rtSettings.level123_cbdl);
        keyFile.set_boolean("Color Management", "Itcwb_enable", rtSettings.itcwb_enable);
        keyFile.set_double("Color Management", "Itcwb_deltaspec", rtSettings.itcwb_deltaspec);
        keyFile.set_double("Color Management", "Itcwb_powponder", rtSettings.itcwb_powponder);

        //keyFile.set_double  ("Color Management", "Colortoningab", rtSettings.colortoningab);
        //keyFile.set_double  ("Color Management", "Decaction", rtSettings.decaction);
        keyFile.set_string("Color Management", "ClutsDirectory", clutsDir);
        keyFile.set_integer("Color Management", "Previewselection", rtSettings.previewselection);
        keyFile.set_double("Color Management", "Cbdlsensi", rtSettings.cbdlsensi);
        keyFile.set_double("Color Management", "Basecorlog", rtSettings.basecorlog);

        keyFile.set_double("Wavelet", "Edghi", rtSettings.edghi);
        keyFile.set_double("Wavelet", "Edglo", rtSettings.edglo);
        keyFile.set_double("Wavelet", "Limrad", rtSettings.limrad);


        keyFile.set_string("ICC Profile Creator", "PimariesPreset", ICCPC_primariesPreset);
        keyFile.set_double("ICC Profile Creator", "RedPrimaryX", ICCPC_redPrimaryX);
        keyFile.set_double("ICC Profile Creator", "RedPrimaryY", ICCPC_redPrimaryY);
        keyFile.set_double("ICC Profile Creator", "GreenPrimaryX", ICCPC_greenPrimaryX);
        keyFile.set_double("ICC Profile Creator", "GreenPrimaryY", ICCPC_greenPrimaryY);
        keyFile.set_double("ICC Profile Creator", "BluePrimaryX", ICCPC_bluePrimaryX);
        keyFile.set_double("ICC Profile Creator", "BluePrimaryY", ICCPC_bluePrimaryY);
        keyFile.set_string("ICC Profile Creator", "GammaPreset", ICCPC_gammaPreset);
        keyFile.set_double("ICC Profile Creator", "Gamma", ICCPC_gamma);
        keyFile.set_double("ICC Profile Creator", "Slope", ICCPC_slope);
        keyFile.set_string("ICC Profile Creator", "ProfileVersion", ICCPC_profileVersion);
        keyFile.set_string("ICC Profile Creator", "Illuminant", ICCPC_illuminant);
        keyFile.set_string("ICC Profile Creator", "Description", ICCPC_description);
        keyFile.set_string("ICC Profile Creator", "Copyright", ICCPC_copyright);
        keyFile.set_boolean("ICC Profile Creator", "AppendParamsToDesc", ICCPC_appendParamsToDesc);


        Glib::ArrayHandle<int> bab = baBehav;
        keyFile.set_integer_list("Batch Processing", "AdjusterBehavior", bab);

        keyFile.set_boolean("Sounds", "Enable", sndEnable);
        keyFile.set_string("Sounds", "BatchQueueDone", sndBatchQueueDone);
        keyFile.set_string("Sounds", "LngEditProcDone", sndLngEditProcDone);
        keyFile.set_double("Sounds", "LngEditProcDoneSecs", sndLngEditProcDoneSecs);


        keyFile.set_boolean("Fast Export", "fastexport_bypass_sharpening", fastexport_bypass_sharpening);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_sharpenEdge", fastexport_bypass_sharpenEdge);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_sharpenMicro", fastexport_bypass_sharpenMicro);
        //keyFile.set_boolean ("Fast Export", "fastexport_bypass_lumaDenoise" , fastexport_bypass_lumaDenoise);
        //keyFile.set_boolean ("Fast Export", "fastexport_bypass_colorDenoise" , fastexport_bypass_colorDenoise);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_defringe", fastexport_bypass_defringe);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_dirpyrDenoise", fastexport_bypass_dirpyrDenoise);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_dirpyrequalizer", fastexport_bypass_dirpyrequalizer);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_wavelet", fastexport_bypass_wavelet);
        keyFile.set_string("Fast Export", "fastexport_raw_bayer_method", fastexport_raw_bayer_method);
        //keyFile.set_boolean ("Fast Export", "fastexport_bypass_bayer_raw_all_enhance" , fastexport_bypass_raw_bayer_all_enhance);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_bayer_dcb_iterations", fastexport_bypass_raw_bayer_dcb_iterations);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_bayer_dcb_enhance", fastexport_bypass_raw_bayer_dcb_enhance);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_bayer_lmmse_iterations", fastexport_bypass_raw_bayer_lmmse_iterations);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_bayer_linenoise", fastexport_bypass_raw_bayer_linenoise);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_bayer_greenthresh", fastexport_bypass_raw_bayer_greenthresh);
        keyFile.set_string("Fast Export", "fastexport_raw_xtrans_method", fastexport_raw_xtrans_method);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_ccSteps", fastexport_bypass_raw_ccSteps);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_ca", fastexport_bypass_raw_ca);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_df", fastexport_bypass_raw_df);
        keyFile.set_boolean("Fast Export", "fastexport_bypass_raw_ff", fastexport_bypass_raw_ff);
        keyFile.set_string("Fast Export", "fastexport_icm_input", fastexport_icm_input_profile);
        keyFile.set_string("Fast Export", "fastexport_icm_working", fastexport_icm_working_profile);
        keyFile.set_string("Fast Export", "fastexport_icm_output", fastexport_icm_output_profile);
        keyFile.set_integer("Fast Export", "fastexport_icm_output_intent", fastexport_icm_outputIntent);
        keyFile.set_boolean("Fast Export", "fastexport_icm_output_bpc", fastexport_icm_outputBPC);
        keyFile.set_boolean("Fast Export", "fastexport_resize_enabled", fastexport_resize_enabled);
        keyFile.set_double("Fast Export", "fastexport_resize_scale", fastexport_resize_scale);
        keyFile.set_string("Fast Export", "fastexport_resize_appliesTo", fastexport_resize_appliesTo);
        keyFile.set_string("Fast Export", "fastexport_resize_method", fastexport_resize_method);
        keyFile.set_integer("Fast Export", "fastexport_resize_dataspec", fastexport_resize_dataspec);
        keyFile.set_integer("Fast Export", "fastexport_resize_width", fastexport_resize_width);
        keyFile.set_integer("Fast Export", "fastexport_resize_height", fastexport_resize_height);
        keyFile.set_integer("Fast Export", "fastexport_resize_longedge", fastexport_resize_longedge);
        keyFile.set_integer("Fast Export", "fastexport_resize_shortedge", fastexport_resize_shortedge);
        keyFile.set_integer("Fast Export", "fastexport_use_fast_pipeline", fastexport_use_fast_pipeline);

        keyFile.set_string("Dialogs", "LastIccDir", lastIccDir);
        keyFile.set_string("Dialogs", "LastDarkframeDir", lastDarkframeDir);
        keyFile.set_string("Dialogs", "LastFlatfieldDir", lastFlatfieldDir);
        keyFile.set_string("Dialogs", "LastCameraProfilesDir", lastCameraProfilesDir);
        keyFile.set_string("Dialogs", "LastLensProfilesDir", lastLensProfilesDir);
        keyFile.set_string("Dialogs", "LastRgbCurvesDir", lastRgbCurvesDir);
        keyFile.set_string("Dialogs", "LastLabCurvesDir", lastLabCurvesDir);
        keyFile.set_string("Dialogs", "LastRetinexDir", lastRetinexDir);
        keyFile.set_string("Dialogs", "LastDenoiseCurvesDir", lastDenoiseCurvesDir);
        keyFile.set_string("Dialogs", "LastWaveletCurvesDir", lastWaveletCurvesDir);
        keyFile.set_string("Dialogs", "LastlocalCurvesDir", lastlocalCurvesDir);
        keyFile.set_string("Dialogs", "LastlocalCurvesDirghs", lastlocalCurvesDirghs);
        keyFile.set_string("Dialogs", "LastPFCurvesDir", lastPFCurvesDir);
        keyFile.set_string("Dialogs", "LastHsvCurvesDir", lastHsvCurvesDir);
        keyFile.set_string("Dialogs", "LastBWCurvesDir", lastBWCurvesDir);
        keyFile.set_string("Dialogs", "LastToneCurvesDir", lastToneCurvesDir);
        keyFile.set_string("Dialogs", "LastIcmCurvesDir", lastIcmCurvesDir);
        keyFile.set_string("Dialogs", "LastVibranceCurvesDir", lastVibranceCurvesDir);
        keyFile.set_string("Dialogs", "LastProfilingReferenceDir", lastProfilingReferenceDir);
        keyFile.set_string("Dialogs", "LastLensProfileDir", lastLensProfileDir);
        keyFile.set_string("Dialogs", "LastICCProfCreatorDir", lastICCProfCreatorDir);
        keyFile.set_string("Dialogs", "LastCopyMovePath", lastCopyMovePath);
        keyFile.set_boolean("Dialogs", "GimpPluginShowInfoDialog", gimpPluginShowInfoDialog);

        keyFile.set_string("Lensfun", "DBDirectory", rtSettings.lensfunDbDirectory);

        switch (rtSettings.xmp_sidecar_style) {
        case rtengine::Settings::XmpSidecarStyle::EXT:
            keyFile.set_string("Metadata", "XMPSidecarStyle", "ext");
            break;
        default:
            keyFile.set_string("Metadata", "XMPSidecarStyle", "std");
        }

        switch (rtSettings.metadata_xmp_sync) {
        case rtengine::Settings::MetadataXmpSync::READ:
            keyFile.set_string("Metadata", "XMPSynchronization", "read");
            break;
        case rtengine::Settings::MetadataXmpSync::READ_WRITE:
            keyFile.set_string("Metadata", "XMPSynchronization", "readwrite");
            break;
        default:
            keyFile.set_string("Metadata", "XMPSynchronization", "none");
        }

        keyData = keyFile.to_data();

    } catch (Glib::KeyFileError &e) {
        throw Error(e.what());
    }

    FILE *f = g_fopen(fname.c_str(), "wt");

    if (f == nullptr) {
        std::cout << "Warning! Unable to save your preferences to: " << fname << std::endl;
        Glib::ustring msg_ = Glib::ustring::compose(M("MAIN_MSG_WRITEFAILED"), fname.c_str());
        throw Error(msg_);
    } else {
        fprintf(f, "%s", keyData.c_str());
        fclose(f);
    }
}

void Options::load(bool lightweight)
{
    // Find the application data path

    const gchar* path;
    Glib::ustring dPath;

    path = g_getenv("RT_SETTINGS");

    if (path != nullptr) {
        rtdir = Glib::ustring(path);

        if (!Glib::path_is_absolute(rtdir)) {
            Glib::ustring msg = Glib::ustring::compose("Settings path %1 is not absolute", rtdir);
            throw Error(msg);
        }
    } else {

#ifdef _WIN32
        WCHAR pathW[MAX_PATH] = {0};

        if (SHGetSpecialFolderPathW(NULL, pathW, CSIDL_LOCAL_APPDATA, false)) {
            char pathA[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA, MAX_PATH, 0, 0);
            rtdir = Glib::build_filename(Glib::ustring(pathA), Glib::ustring(CACHEFOLDERNAME));
        }

#else
    #ifdef __APPLE__
        rtdir = Glib::build_filename(Glib::ustring(g_get_home_dir()), "/Library/Application Support/", Glib::ustring(CACHEFOLDERNAME), "/config/");
    #else
        rtdir = Glib::build_filename(Glib::ustring(g_get_user_config_dir()), Glib::ustring(CACHEFOLDERNAME));
    #endif
#endif
    }

    auto& options = App::get().mut_options();
    if (options.rtSettings.verbose) {
        printf("Settings directory (rtdir) = %s\n", rtdir.c_str());
    }

    // Set the cache folder in RT's base folder
    cacheBaseDir = Glib::build_filename(App::get().argv0(), "mycache");

    // Read the global option file (the one located in the application's base folder)
    try {
        options.readFromFile(Glib::build_filename(App::get().argv0(), "options"));
    } catch (Options::Error &) {
        // ignore errors here
    }

    if (!options.multiUser && path == nullptr) {
        rtdir = Glib::build_filename(App::get().argv0(), "mysettings");
    }

    // Modify the path of the cache folder to the one provided in RT_CACHE environment variable.
    path = g_getenv("RT_CACHE");

    if (path != nullptr) {
        cacheBaseDir = Glib::ustring(path);

        if (!Glib::path_is_absolute(cacheBaseDir)) {
            Glib::ustring msg = Glib::ustring::compose("Cache base dir %1 is not absolute", cacheBaseDir);
            throw Error(msg);
        }
    }

    // No environment variable provided, so falling back to the multi user mode, if enabled
    else if (options.multiUser) {
#ifdef _WIN32
        cacheBaseDir = Glib::build_filename(rtdir, "cache");
#else
    #ifdef __APPLE__
        cacheBaseDir = Glib::build_filename(Glib::ustring(g_get_home_dir()), "/Library/Application Support/", Glib::ustring(CACHEFOLDERNAME), "/cache/");
    #else
        cacheBaseDir = Glib::build_filename(Glib::ustring(g_get_user_cache_dir()), Glib::ustring(CACHEFOLDERNAME));
    #endif
#endif
    }

    // Read the user option file (the one located somewhere in the user's home folder)
    // Those values supersets those of the global option file
    try {
        options.readFromFile(Glib::build_filename(rtdir, "options"));
    } catch (Options::Error &) {
        // If the local option file does not exist or is broken, and the local cache folder does not exist, recreate it
        if (!g_mkdir_with_parents(rtdir.c_str(), 511)) {
            // Save the option file
            options.saveToFile(Glib::build_filename(rtdir, "options"));
        }
    }

#ifdef __APPLE__

    if (options.multiUser) {
        // make sure .local/share exists on OS X so we don't get problems with recently-used.xbel
        g_mkdir_with_parents(g_get_user_data_dir(), 511);
    }

#endif

    if (options.rtSettings.verbose) {
        printf("Cache directory (cacheBaseDir) = %s\n", cacheBaseDir.c_str());
    }

    // Update profile's path and recreate it if necessary
    options.updatePaths();

    // Check default Raw and Img procparams existence
    if (options.defProfRaw.empty()) {
        options.defProfRaw = DEFPROFILE_RAW;
    } else {
        if (!options.findProfilePath(options.defProfRaw).empty()) {
            if (options.rtSettings.verbose) {
                std::cout << "Default profile for raw images \"" << options.defProfRaw << "\" found" << std::endl;
            }
        } else {
            if (options.defProfRaw != DEFPROFILE_RAW) {
                options.setDefProfRawMissing(true);

                Glib::ustring dpr(DEFPROFILE_RAW);

                if (options.findProfilePath(dpr).empty()) {
                    options.setBundledDefProfRawMissing(true);
                }
            } else {
                options.setBundledDefProfRawMissing(true);
            }
        }
    }

    if (options.defProfImg.empty()) {
        options.defProfImg = DEFPROFILE_IMG;
    } else {
        if (!options.findProfilePath(options.defProfImg).empty()) {
            if (options.rtSettings.verbose) {
                std::cout << "Default profile for non-raw images \"" << options.defProfImg << "\" found" << std::endl;
            }
        } else {
            if (options.defProfImg != DEFPROFILE_IMG) {
                options.setDefProfImgMissing(true);

                Glib::ustring dpi(DEFPROFILE_IMG);

                if (options.findProfilePath(dpi).empty()) {
                    options.setBundledDefProfImgMissing(true);
                }
            } else {
                options.setBundledDefProfImgMissing(true);
            }
        }
    }

    // We handle languages using a hierarchy of translations.  The top of the hierarchy is default.  This includes a default translation for all items
    // (most likely using simple English).  The next level is the language: for instance, English, French, Chinese, etc.  This file should contain a
    // generic translation for all items which differ from default.  Finally there is the locale.  This is region-specific items which differ from the
    // language file.  These files must be name in the format <Language> (<LC>), where Language is the name of the language which it inherits from,
    // and LC is the local code.  Some examples of this would be English (US) (American English), French (FR) (France French), French (CA) (Canadian
    // French), etc.
    //
    // Each level will only contain the differences between itself and its parent translation.  For instance, English (UK) or English (CA) may
    // include the translation "HISTORY_MSG_34;Avoid Colour Clipping" where English would translate it as "HISTORY_MSG_34;Avoid Color Clipping" (note
    // the difference in the spelling of 'colour').
    //
    // It is important that when naming the translation files, that you stick to the format <Language> or <Language> (<LC>).  We depend on that to figure
    // out which are the parent translations.  Furthermore, there must be a file <Language> for each locale <Language> (<LC>) -- you cannot have
    // 'French (CA)' unless there is a file 'French'.

    Glib::ustring defaultTranslation = Glib::build_filename(App::get().argv0(), "languages", "default");
    Glib::ustring languageTranslation = "";
    Glib::ustring localeTranslation = "";

    if (options.languageAutoDetect) {
        options.language = langMgr.getOSUserLanguage();
    }

    if (!options.language.empty()) {
        std::vector<Glib::ustring> langPortions = Glib::Regex::split_simple(" ", options.language);

        if (langPortions.size() >= 1) {
            languageTranslation = Glib::build_filename(App::get().argv0(), "languages", langPortions.at(0));
        }

        if (langPortions.size() >= 2) {
            localeTranslation = Glib::build_filename(App::get().argv0(), "languages", options.language);
        }
    }

    langMgr.load(options.language, {localeTranslation, languageTranslation, defaultTranslation});

    rtengine::init(&options.rtSettings, App::get().argv0(), rtdir, !lightweight);

    App::get().signal_preferences_changed().emit();
}

void Options::save()
{
    App::get().mut_options().saveToFile(Glib::build_filename(rtdir, "options"));
}

/*
 * return true if ext is a parsed extension (retained or not)
 */
bool Options::is_parse_extention(Glib::ustring fname) const
{
    Glib::ustring ext = getExtension(fname).lowercase();

    if (!ext.empty()) {
        // there is an extension to the filename

        // look out if it has one of the listed extensions (selected or not)
        for (unsigned int i = 0; i < parseExtensions.size(); i++) {
            if (ext == parseExtensions[i]) {
                return true;
            }
        }
    }

    return false;
}

/*
 * return true if fname ends with one of the retained image file extensions
 */
bool Options::has_retained_extention(const Glib::ustring& fname) const
{
    return parsedExtensionsSet.find(getExtension(fname).lowercase()) != parsedExtensionsSet.end();
}

// Pattern matches "5.1" from "5.1-23-g12345678", when comparing option.version to RTVERSION
bool Options::is_new_version() const {
    const std::string vs[] = {App::VERSION, version};
    std::vector<std::string> vMajor;

    for (const auto& v : vs) {
        vMajor.emplace_back(v, 0, v.find_first_not_of("0123456789."));
    }

    return vMajor.size() == 2 && vMajor[0] != vMajor[1];
}

/*
 * return true if ext is an enabled extension
 */
bool Options::is_extention_enabled(const Glib::ustring& ext) const
{
    return parsedExtensionsSet.find(ext.lowercase()) != parsedExtensionsSet.end();
}

Glib::ustring Options::getUserProfilePath() const
{
    return userProfilePath;
}

Glib::ustring Options::getGlobalProfilePath() const
{
    return globalProfilePath;
}

bool Options::is_defProfRawMissing() const
{
    return defProfError & rtengine::toUnderlying(DefProfError::defProfRawMissing);
}
bool Options::is_defProfImgMissing() const
{
    return defProfError & rtengine::toUnderlying(DefProfError::defProfImgMissing);
}
void Options::setDefProfRawMissing(bool value)
{
    if (value) {
        defProfError |= rtengine::toUnderlying(DefProfError::defProfRawMissing);
    } else {
        defProfError &= ~rtengine::toUnderlying(DefProfError::defProfRawMissing);
    }
}
void Options::setDefProfImgMissing(bool value)
{
    if (value) {
        defProfError |= rtengine::toUnderlying(DefProfError::defProfImgMissing);
    } else {
        defProfError &= ~rtengine::toUnderlying(DefProfError::defProfImgMissing);
    }
}
bool Options::is_bundledDefProfRawMissing() const
{
    return defProfError & rtengine::toUnderlying(DefProfError::bundledDefProfRawMissing);
}
bool Options::is_bundledDefProfImgMissing() const
{
    return defProfError & rtengine::toUnderlying(DefProfError::bundledDefProfImgMissing);
}
void Options::setBundledDefProfRawMissing(bool value)
{
    if (value) {
        defProfError |= rtengine::toUnderlying(DefProfError::bundledDefProfRawMissing);
    } else {
        defProfError &= ~rtengine::toUnderlying(DefProfError::bundledDefProfRawMissing);
    }
}
void Options::setBundledDefProfImgMissing(bool value)
{
    if (value) {
        defProfError |= rtengine::toUnderlying(DefProfError::bundledDefProfImgMissing);
    } else {
        defProfError &= ~rtengine::toUnderlying(DefProfError::bundledDefProfImgMissing);
    }
}
Glib::ustring Options::getICCProfileCopyright()
{
    Glib::Date now;
    now.set_time_current();
    return Glib::ustring::compose("Copyright RawTherapee %1, CC0", now.get_year());
}

ExternalEditor::ExternalEditor() = default;

ExternalEditor::ExternalEditor(
    const Glib::ustring &name, const Glib::ustring &command, bool native_command, const Glib::ustring &icon_serialized
): name(name), command(command), native_command(native_command), icon_serialized(icon_serialized) {}

bool ExternalEditor::operator==(const ExternalEditor &other) const
{
    return this->name == other.name && this->command == other.command && this->native_command == other.native_command && this->icon_serialized == other.icon_serialized;
}

bool ExternalEditor::operator!=(const ExternalEditor &other) const
{
    return !(*this == other);
}
