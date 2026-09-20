set(PROCPARAM_SRCS
    procparams.cc
    params/advanced.cc
    params/locallab.cc
    params/raw.cc
    params/serdes.cc
)

set(RTENGINESOURCEFILES
    # We may override new/delete when memory profiling is enabled
    newdelete.cc

    alpha.cc
    ahd_demosaic_RT.cc
    amaze_demosaic_RT.cc
    aspectratios.cc
    badpixels.cc
    bayer_bilinear_demosaic.cc
    boxblur.cc
    canon_cr3_decoder.cc
    CA_correct_RT.cc
    calc_distort.cc
    camconst.cc
    capturesharpening.cc
    cfa_linedn_RT.cc
    ciecam02.cc
    cieimage.cc
    cJSON.c
    clutstore.cc
    color.cc
    colortemp.cc
    coord.cc
    cplx_wavelet_dec.cc
    curves.cc
    dcp.cc
    dcraw.cc
    dcrop.cc
    demosaic_algos.cc
    dfmanager.cc
    diagonalcurves.cc
    dirpyr_equalizer.cc
    dual_demosaic_RT.cc
    dynamicprofile.cc
    eahd_demosaic.cc
    EdgePreservingDecomposition.cc
    fast_demo.cc
    ffmanager.cc
    filmnegativeproc.cc
    flatcurves.cc
    FTblockDN.cc
    gamutwarning.cc
    gauss.cc
    green_equil_RT.cc
    guidedfilter.cc
    hilite_recon.cc
    histmatching.cc
    hphd_demosaic_RT.cc
    iccjpeg.cc
    iccstore.cc
    iimage.cc
    image16.cc
    image8.cc
    imagedata.cc
    imagedimensions.cc
    imagefloat.cc
    imageio.cc
    imagesource.cc
    improccoordinator.cc
    improcfun.cc
    impulse_denoise.cc
    init.cc
    ipdehaze.cc
    ipgrain.cc
    iplab2rgb.cc
    iplocallab.cc
    locallab/iplocallab_common.cc
    locallab/iplocallab_denoise.cc
    locallab/iplocallab_log.cc
    locallab/iplocallab_tonemap.cc
    locallab/iplocallab_ciecam.cc
    locallab/iplocallab_softlight.cc
    locallab/iplocallab_exposure.cc
    iplabregions.cc
    iplocalcontrast.cc
    ipresize.cc
    ipretinex.cc
    ipshadowshighlights.cc
    ipsharpen.cc
    ipsharpenedges.cc
    ipsoftlight.cc
    iptoneequalizer.cc
    iptransform.cc
    ipvibrance.cc
    ipwavelet.cc
    jdatasrc.cc
    jpeg_ijg/jpeg_memsrc.cc
    labimage.cc
    lcp.cc
    lensmetadata.cc
    lmmse_demosaic.cc
    loadinitial.cc
    metadata.cc
    munselllch.cc
    myfile.cc
    panasonic_decoders.cc
    pdaflinesfilter.cc
    perspectivecorrection.cc
    PF_correct_RT.cc
    pipettebuffer.cc
    pixelshift.cc
    previewimage.cc
    processingjob.cc
    profilestore.cc
    rawflatfield.cc
    rawimage.cc
    rawimagesource.cc
    rcd_demosaic.cc
    refreshmap.cc
    rt_algo.cc
    rtapp.cc
    rtlensfun.cc
    rtthumbnail.cc
    shmap.cc
    simpleprocess.cc
    spot.cc
    stdimagesource.cc
    svgrender.cc
    tmo_fattal02.cc
    utils.cc
    vng4_demosaic_RT.cc
    xtrans_demosaic.cc
    ${PROCPARAM_SRCS}
)

if(LENSFUN_HAS_LOAD_DIRECTORY)
    set_source_files_properties(
        rtlensfun.cc PROPERTIES
        COMPILE_DEFINITIONS RT_LENSFUN_HAS_LOAD_DIRECTORY
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0")
    # procparams.cc takes a long time to compile with optimizations starting
    # with GCC 12.1 due to PTA (see issue #6548)
    get_source_file_property(PROCPARAMS_COMPILE_OPTIONS procparams.cc COMPILE_OPTIONS)
    if(PROCPARAMS_COMPILE_OPTIONS STREQUAL "NOTFOUND")
        set(PROCPARAMS_COMPILE_OPTIONS "")
    else()
        set(PROCPARAMS_COMPILE_OPTIONS "${PROCPARAMS_COMPILE_OPTIONS};")
    endif()
    set(PROCPARAMS_COMPILE_OPTIONS "${PROCPARAMS_COMPILE_OPTIONS}-fno-tree-pta")

    set_source_files_properties(
        procparams.cc PROPERTIES
        COMPILE_OPTIONS ${PROCPARAMS_COMPILE_OPTIONS}
    )
endif()

if(NOT WITH_SYSTEM_KLT)
    set(RTENGINESOURCEFILES ${RTENGINESOURCEFILES}
        klt/convolve.cc
        klt/error.cc
        klt/klt_util.cc
        klt/klt.cc
        klt/pnmio.cc
        klt/pyramid.cc
        klt/selectGoodFeatures.cc
        klt/storeFeatures.cc
        klt/trackFeatures.cc
        klt/writeFeatures.cc
    )
    set(KLT_LIBRARIES)
endif()

add_library(rtengine STATIC "${RTENGINESOURCEFILES}")

add_dependencies(rtengine UpdateInfo)

target_include_directories(rtengine
    PUBLIC
        # Other libraries should use #include "rtengine/header.h"
        ${PROJECT_SOURCE_DIR}
    PRIVATE
        # Subdirectories that are not libraries themselves should be included
        # like #include "your_subdirectory/header.h"
        ${CMAKE_CURRENT_SOURCE_DIR}
)
# UpdateInfo creates "rtgui/version.h"
target_include_directories(rtengine BEFORE PUBLIC ${CMAKE_BINARY_DIR})

if(EXTRA_INCDIR)
    target_include_directories(rtengine PUBLIC "${EXTRA_INCDIR}")
endif()

rt_setup_target(rtengine)
target_compile_options(rtengine PRIVATE ${RTENGINE_CXX_FLAGS})

if(WITH_BENCHMARK)
    target_compile_definitions(rtengine PUBLIC "BENCHMARK")
endif()

# When linking against RT's version of libraw, there will be linking issues for
# jpeg and zlib if libraw is not linked before them.
if(NOT WITH_SYSTEM_LIBRAW)
    include(LibRaw.cmake)
    target_include_directories(rtengine PRIVATE "${CMAKE_SOURCE_DIR}/rtengine/libraw")
    target_link_libraries(rtengine PUBLIC ${LIBRAW_LIBRARIES})
else()
    target_link_libraries(rtengine PUBLIC PkgConfig::LIBRAW)
endif()

target_link_libraries(rtengine
    PUBLIC
        fmt::fmt
        JPEG::JPEG
        PkgConfig::CAIROMM
        PkgConfig::EXIV2
        PkgConfig::EXPAT
        PkgConfig::FFTW3F
        PkgConfig::GLIBMM
        PkgConfig::GTKMM
        PkgConfig::IPTCDATA
        PkgConfig::LCMS
        PkgConfig::LENSFUN
        PNG::PNG
        TIFF::TIFF
        ZLIB::ZLIB
        ${EXTRA_LIB}
)

# Atomic is required in some builds: #6821
if(ATOMIC_FOUND)
    target_link_libraries(rtengine PUBLIC ${ATOMIC_LIBRARIES})
endif()

if(JXL_FOUND)
    target_link_libraries(rtengine PUBLIC PkgConfig::JXL)
    if (JXLTHREADS_FOUND)
        target_link_libraries(rtengine PUBLIC PkgConfig::JXLTHREADS)
    endif()
endif()

if(NOT WITH_SYSTEM_KLT)
    target_include_directories(rtengine PUBLIC "${CMAKE_SOURCE_DIR}/rtengine/klt")
    # Source files are compiled as part of rtengine directly.
else()
    target_include_directories(rtengine PUBLIC ${KLT_INCLUDE_DIRS})
    target_link_libraries(rtengine PUBLIC ${KLT_LIBRARIES})
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(rtengine PUBLIC OpenMP::OpenMP_CXX)
endif()

if("${SVG_BACKEND}" STREQUAL "librsvg")
    target_link_libraries(rtengine PUBLIC PkgConfig::RSVG)
    target_compile_definitions(rtengine PUBLIC "SVG_BACKEND_RSVG")
elseif("${SVG_BACKEND}" STREQUAL "lunasvg")
    if(WITH_SYSTEM_LUNASVG)
        target_link_libraries(rtengine PUBLIC PkgConfig::LUNASVG)
    else()
        target_link_libraries(rtengine PUBLIC lunasvg::lunasvg)
    endif()
    target_compile_definitions(rtengine PUBLIC "SVG_BACKEND_LUNASVG")
else()
    message(FATAL_ERROR "Unknown SVG rendering backend")
endif()

if(TCMALLOC_FOUND)
    target_compile_definitions(rtengine PUBLIC "RT_USING_TCMALLOC")
endif()

if(WITH_TRACY_PROFILER)
    target_link_libraries(rtengine PUBLIC Tracy::TracyClient)
    target_compile_definitions(rtengine PUBLIC "RT_TRACING_ENABLE")

    if(WITH_TRACY_MEMORY_PROFILING)
        if(TCMALLOC_FOUND AND NOT WITH_TRACY_ON_DEMAND)
            message(SEND_ERROR "-DWITH_TRACY_ON_DEMAND=ON is required for memory profiling with tcmalloc")
        endif()
        target_compile_definitions(rtengine PUBLIC "WITH_TRACY_MEMORY_PROFILING")
    endif()
endif()

# It may be nice to store library version too
if(BUILD_SHARED_LIBS)
    install(TARGETS rtengine DESTINATION "${LIBDIR}")
endif()

set(CAMCONSTSFILE "camconst.json")
install(FILES ${CAMCONSTSFILE} DESTINATION "${DATADIR}" PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
