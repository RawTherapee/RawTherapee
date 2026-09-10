# Common source files for both CLI and non-CLI execautables
set(CLISOURCEFILES
    alignedmalloc.cc
    editcallbacks.cc
    main-cli.cc
    multilangmgr.cc
    options.cc
    paramsedited.cc
    pathutils.cc
    threadutils.cc
)

set(RT_GUI_TOOL_PANEL_SOURCES
    tools/bayerpreprocess.cc
    tools/bayerprocess.cc
    tools/bayerrawexposure.cc
    tools/blackwhite.cc
    tools/cacorrection.cc
    tools/chmixer.cc
    tools/colorappearance.cc
    tools/colortoning.cc
    tools/compressgamut.cc
    tools/crop.cc
    tools/cropguide.cc
    tools/darkframe.cc
    tools/defringe.cc
    tools/dehaze.cc
    tools/dirpyrdenoise.cc
    tools/dirpyrequalizer.cc
    tools/distortion.cc
    tools/epd.cc
    tools/fattaltonemap.cc
    tools/filmnegative.cc
    tools/filmsimulation.cc
    tools/flatfield.cc
    tools/framing.cc
    tools/gradient.cc
    tools/hsvequalizer.cc
    tools/icmpanel.cc
    tools/impulsedenoise.cc
    tools/labcurve.cc
    tools/lensgeom.cc
    tools/lensprofile.cc
    tools/localcontrast.cc
    tools/locallab.cc
    tools/locallabtools.cc
    tools/locallabtools2.cc
    tools/pcvignette.cc
    tools/pdsharpening.cc
    tools/perspective.cc
    tools/preprocess.cc
    tools/preprocesswb.cc
    tools/prsharpening.cc
    tools/rawcacorrection.cc
    tools/rawexposure.cc
    tools/resize.cc
    tools/retinex.cc
    tools/rgbcurves.cc
    tools/rotate.cc
    tools/sensorbayer.cc
    tools/sensorxtrans.cc
    tools/shadowshighlights.cc
    tools/sharpenedge.cc
    tools/sharpening.cc
    tools/sharpenmicro.cc
    tools/softlight.cc
    tools/spot.cc
    tools/tonecurve.cc
    tools/toneequalizer.cc
    tools/vibrance.cc
    tools/vignetting.cc
    tools/wavelet.cc
    tools/whitebalance.cc
    tools/xtransprocess.cc
    tools/xtransrawexposure.cc
)

set(RT_GUI_WIDGET_SOURCES
    # Basic
    widgets/basic/adjuster.cc
    widgets/basic/checkbox.cc
    widgets/basic/coloredbar.cc
    widgets/basic/colorpreview.cc
    widgets/basic/coordinateadjuster.cc
    widgets/basic/lwbutton.cc
    widgets/basic/lwbuttonset.cc
    widgets/basic/myexpander.cc
    widgets/basic/popupbutton.cc
    widgets/basic/popupcommon.cc
    widgets/basic/popuptogglebutton.cc
    widgets/basic/shcselector.cc
    widgets/basic/thresholdadjuster.cc
    widgets/basic/thresholdselector.cc
    # Curves
    widgets/curves/curveeditor.cc
    widgets/curves/curveeditorgroup.cc
    widgets/curves/diagonalcurveeditorsubgroup.cc
    widgets/curves/flatcurveeditorsubgroup.cc
    widgets/curves/mycurve.cc
    widgets/curves/mydiagonalcurve.cc
    widgets/curves/myflatcurve.cc
)

set(RT_GUI_WINDOWS_SOURCES
    windows/editwindow.cc
    windows/iccprofilecreator.cc
    windows/partialpastedlg.cc
    windows/preferences.cc
    windows/renamedlg.cc
    windows/rtappchooserdialog.cc
    windows/rtwindow.cc
    windows/saveasdlg.cc
    windows/splash.cc
)

set(NONCLISOURCEFILES
    alignedmalloc.cc
    batchqueue.cc
    batchqueuebuttonset.cc
    batchqueueentry.cc
    batchqueuepanel.cc
    batchtoolpanelcoord.cc
    bqentryupdater.cc
    browserfilter.cc
    cacheimagedata.cc
    cachemanager.cc
    clipboard.cc
    coarsepanel.cc
    controllines.cc
    controlspotpanel.cc
    crophandler.cc
    cropwindow.cc
    cursormanager.cc
    dirbrowser.cc
    drawcropguide.cc
    dynamicprofilepanel.cc
    editbuffer.cc
    editcallbacks.cc
    editorpanel.cc
    editwidgets.cc
    eventmapper.cc
    exiffiltersettings.cc
    exifpanel.cc
    exportpanel.cc
    externaleditorpreferences.cc
    extprog.cc
    filebrowser.cc
    filebrowserentry.cc
    filecatalog.cc
    filepanel.cc
    filethumbnailbuttonset.cc
    filterpanel.cc
    guiutils.cc
    hidpi.cc
    histogrampanel.cc
    history.cc
    imagearea.cc
    imageareapanel.cc
    indclippedpanel.cc
    inspector.cc
    iptcpanel.cc
    labgrid.cc
    lockablecolorpicker.cc
    main.cc
    metadatapanel.cc
    multilangmgr.cc
    navigator.cc
    options.cc
    paramsedited.cc
    pathutils.cc
    placesbrowser.cc
    previewhandler.cc
    previewloader.cc
    previewmodepanel.cc
    previewwindow.cc
    profilepanel.cc
    profilestorecombobox.cc
    recentbrowser.cc
    rtimage.cc
    rtscalable.cc
    rtsurface.cc
    saveformatpanel.cc
    soundman.cc
    threadutils.cc
    thumbbrowserbase.cc
    thumbbrowserentrybase.cc
    thumbimageupdater.cc
    thumbnail.cc
    toolbar.cc
    toollocationpref.cc
    toolpanel.cc
    toolpanelcoord.cc
    zoompanel.cc
    ${RT_GUI_TOOL_PANEL_SOURCES}
    ${RT_GUI_WIDGET_SOURCES}
    ${RT_GUI_WINDOWS_SOURCES}
)

if(APPLE)
    find_package(MacIntegration REQUIRED)
    # At the time of writing CMake has no module finder for gtkmacintegration so here we have it hard-coded, if installed via macports it should be in /opt/local/...
    set(EXTRA_LIB_RTGUI "${MacIntegration_LIBRARIES}")
    set(EXTRA_INCDIR "${EXTRA_INCDIR}" "${MacIntegration_INCLUDE_DIRS}")
endif()

if(WIN32)
    find_program(GIT_CMD git.exe HINTS ENV Path PATH_SUFFIXES ../)

    # Get the version which will be included in the application manifest
    if(GIT_CMD STREQUAL GIT_CMD-NOTFOUND)
        message(WARNING "git.exe not found, using default version")
        set(WINDOWS_APPLICATION_VERSION "0.0.0.0")
    else()
        execute_process(COMMAND ${GIT_CMD} describe --tags --always --long OUTPUT_VARIABLE GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
        # Extract the major and minor versions, plus the number of commits since the tag, then build the four component version number, e.g. "5.0-gtk2-2-g12345678" -> "5.0.2.0"
        string(REGEX REPLACE "^([0-9]+\.[0-9]+).*-([0-9]+)-g[0-9a-f]+$" "\\1.\\2.0" WINDOWS_APPLICATION_VERSION "${GIT_DESCRIBE}")
        if(WINDOWS_APPLICATION_VERSION STREQUAL GIT_DESCRIBE)
            message(WARNING "git describe output did not match expected format, using default version")
            set(WINDOWS_APPLICATION_VERSION "0.0.0.0")
        endif()
    endif()
    message(STATUS "Using version ${WINDOWS_APPLICATION_VERSION} for manifest template")

    # Create the manifest files
    set(WINDOWS_APPLICATION_NAME "RawTherapee.RawTherapee.rawtherapee")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/app.manifest.in
        ${CMAKE_CURRENT_BINARY_DIR}/app.manifest
    )
    set(WINDOWS_APPLICATION_NAME "RawTherapee.RawTherapee.rawtherapee-cli")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/app.manifest.in
        ${CMAKE_CURRENT_BINARY_DIR}/app_cli.manifest
    )

    # Create the manifest RC files
    set(MANIFEST_NAME app)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/app_manifest.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/app_manifest.rc
    )
    set(MANIFEST_NAME app_cli)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/app_manifest.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/app_manifest_cli.rc
    )

    set(EXTRA_SRC_CLI myicon.rc ${CMAKE_CURRENT_BINARY_DIR}/app_manifest_cli.rc)
    set(EXTRA_SRC_NONCLI myicon.rc ${CMAKE_CURRENT_BINARY_DIR}/app_manifest.rc)
    set(EXTRA_LIB_RTGUI winmm)
endif()

# Create new executables targets
add_executable(rth "${EXTRA_SRC_NONCLI}" "${NONCLISOURCEFILES}")
add_executable(rth-cli "${EXTRA_SRC_CLI}" "${CLISOURCEFILES}")

# Subdirectories that are not libraries themselves should be included
# like #include "your_subdirectory/header.h"
target_include_directories(rth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(rth-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Add dependencies to executables targets
add_dependencies(rth UpdateInfo)
add_dependencies(rth-cli UpdateInfo)

# Create config.h which defines where data are stored
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h")
# UpdateInfo and above creates new headers in build directory
target_include_directories(rth BEFORE PRIVATE ${CMAKE_BINARY_DIR}/rtgui)
target_include_directories(rth-cli BEFORE PRIVATE ${CMAKE_BINARY_DIR}/rtgui)

rt_setup_target(rth)
rt_setup_target(rth-cli)

#Define a target specific definition to use in code
target_compile_definitions(rth PUBLIC GUIVERSION)
target_compile_definitions(rth-cli PUBLIC CLIVERSION)

# Set executables targets properties, i.e. output filename and compile flags
# for "Debug" builds, open a console in all cases for Windows version
if((WIN32) AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set_target_properties(rth PROPERTIES LINK_FLAGS "-mwindows")
endif()
set_target_properties(rth PROPERTIES OUTPUT_NAME rawtherapee)
set_target_properties(rth-cli PROPERTIES OUTPUT_NAME rawtherapee-cli)

target_include_directories(rth PRIVATE ${EXTRA_INCDIR})
target_include_directories(rth-cli PRIVATE ${EXTRA_INCDIR})

target_link_directories(rth PRIVATE ${EXTRA_LIBDIR})
target_link_directories(rth-cli PRIVATE ${EXTRA_LIBDIR})

# Add linked libraries dependencies to executables targets
target_link_libraries(rth PRIVATE
    rtengine
    PkgConfig::GTKMM
    ${EXTRA_LIB_RTGUI}
)
target_link_libraries(rth-cli PRIVATE
    rtengine
    PkgConfig::GTKMM
    ${EXTRA_LIB_RTGUI}
)

if(TCMALLOC_FOUND)
    target_link_libraries(rth PRIVATE TCMALLOC::TCMALLOC)
    target_link_libraries(rth-cli PRIVATE TCMALLOC::TCMALLOC)
endif()
if(USE_LIBCANBERRA)
    target_link_libraries(rth PRIVATE PkgConfig::CANBERRA)
endif()

# Install executables
install(TARGETS rth DESTINATION "${BINDIR}")
install(TARGETS rth-cli DESTINATION "${BINDIR}")
