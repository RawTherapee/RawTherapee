cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0127)
    cmake_policy(SET CMP0127 NEW) # Added in CMake 3.22
endif()
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 OLD) # Added in CMake 3.31
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

# Must stay before the project() command:
if(${CMAKE_EXTRA_GENERATOR} MATCHES "Eclipse CDT4")
    set(CMAKE_CXX_COMPILER_ARG1
        "-std=c++11"
        CACHE STRING "C++ version for eclipse" FORCE)
    # Users building with Eclipse should set CMAKE_ECLIPSE_VERSION through the
    # command line to their current version of Eclipse:
    # set(CMAKE_ECLIPSE_VERSION "4.6.0" CACHE STRING "Eclipse version" FORCE)
endif()

project(RawTherapee)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/StandardProjectSettings.cmake)
rt_force_out_of_source_builds()
rt_basic_build_settings()

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10")
    message(FATAL_ERROR "Building RawTherapee requires using AppleClang 10 or higher!")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8")
    message(FATAL_ERROR "Building RawTherapee requires using Clang 8 or higher!")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.1")
    message(FATAL_ERROR "Building RawTherapee requires using GCC version 8.1 or higher!")
endif()

# Warning for GCC vectorization issues, which causes problems #5749 and #6384:
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "10.0" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.2")
    message(STATUS "WARNING: gcc ${CMAKE_CXX_COMPILER_VERSION} is known to miscompile RawTherapee when using -ftree-loop-vectorize, forcing the option to be off")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-tree-loop-vectorize")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-loop-vectorize")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0")
    message(STATUS "WARNING: gcc ${CMAKE_CXX_COMPILER_VERSION} is known to miscompile RawTherapee when using --ffp-contract=fast, forcing the option to be off")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffp-contract=off")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")
endif()

include(CMakeDependentOption)

# Cache name suffix examples: "" = ~/.config/RawTherapee, "5" =
# ~/.config/RawTherapee-5, "_testing" = ~/.config/RawTherapee_testing Use "" for
# stable releases and "5-dev" for anything else.
set(CACHE_NAME_SUFFIX "" CACHE STRING "RawTherapee's cache folder suffix")

# For macOS only, OSX_DEV_BUILD option allows using relative paths instead of absolute
# paths. Consequently, for development builds, application can be launching without
# being bundled. However, file access can be restricted for some folder.
cmake_dependent_option(
    OSX_DEV_BUILD "Generate macOS development builds"
    OFF "APPLE"
    OFF # otherwise
)

# On macOS, optionally generate the final zip artifact file without version in the name for nightly upload purposes.
cmake_dependent_option(
    OSX_NIGHTLY "Generate a generically-named zip"
    OFF "APPLE"
    OFF # otherwise
)

# On macOS, optionally generate RawTherapee_<branch>_macOS_<buildtype>.zip for the CI
cmake_dependent_option(
    OSX_CONTINUOUS "Generate a generically-named zip for CI"
    OFF "APPLE"
    OFF # otherwise
)

# Generate a universal macOS build
cmake_dependent_option(
    OSX_UNIVERSAL "Generate a universal app"
    OFF "APPLE"
    OFF # otherwise
)

# On macOS: merge the app via a specific url to generate a universal bundle with both x86_64 and arm64
if(OSX_UNIVERSAL)
    if(NOT "${OSX_UNIVERSAL_URL}")
        if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
            set(OSX_UNIVERSAL_URL "file:///rawtherapee/latest/RawTherapee_macOS_x86_64_latest.zip" CACHE STRING "URL of x86_64 app for lipo")
        else()
            set(OSX_UNIVERSAL_URL "file:///rawtherapee/latest/RawTherapee_macOS_arm64_latest.zip" CACHE STRING "URL of arm64 app for lipo")
        endif()
    endif()
endif()

# By default we don't use a specific processor target, so PROC_TARGET_NUMBER is
# set to 0. Specify other values to optimize for specific processor architecture
# as listed in ProcessorTargets.cmake:
set(PROC_TARGET_NUMBER
    0
    CACHE
        STRING
        "Selected target processor from the list above (taken from ProcessorTargets.cmake)"
)

# Set special compilation flags for rtengine which get added to CMAKE_CXX_FLAGS:
# Some Linux distros build with -O2 instead of -O3. We explicitly enable auto
# vectorization by using -ftree-vectorize
set(RTENGINE_CXX_FLAGS
    "-ftree-vectorize"
    CACHE STRING "Special compilation flags for RTEngine")

# Loads the ProcessorTargets list:
include(ProcessorTargets.cmake)
set(PROC_LABEL
    "undefined"
    CACHE STRING
          "Target processor label, unused if PROC_TARGET_NUMBER = 0 or 2")
set(PROC_FLAGS
    ""
    CACHE STRING "Target processor related build/link flags")
if((NOT (PROC_TARGET_NUMBER EQUAL 0)) AND (NOT (PROC_TARGET_NUMBER EQUAL 2)))
    set(PROC_LABEL ${PROC_TARGET_${PROC_TARGET_NUMBER}_LABEL})
endif()
if(NOT (PROC_TARGET_NUMBER EQUAL 0))
    set(PROC_FLAGS ${PROC_TARGET_${PROC_TARGET_NUMBER}_FLAGS})
endif()
if(UNIX AND PROC_LABEL STREQUAL "undefined")
    execute_process(
        COMMAND uname -p
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE cpu)
    if("${cpu}" STREQUAL "unknown")
        set(PROC_LABEL "${CMAKE_SYSTEM_PROCESSOR}")
    else()
        set(PROC_LABEL "${cpu}")
    endif()
endif()

# If PROC_FORCED_LABEL exists, its value is loaded in PROC_LABEL to override the
# one from ProcessorTargets:
if(DEFINED PROC_FORCED_LABEL)
    set(PROC_LABEL ${PROC_FORCED_LABEL})
endif()

# Add the proc flags to the build flags:
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PROC_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PROC_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PROC_FLAGS}")

# On i386 Linux we can fix unaligned SSE malloc (see GitHub issue #4432)
include(FindUnalignedMalloc)

if(WIN32)
    # Add additional paths. Look in the MinGW path first, then in the Gtkmm
    # path. If you wish to build some dependent libraries, you have to install
    # them in MinGW to use them:
    set(CMAKE_PREFIX_PATH
        $ENV{MINGW_BASEPATH} $ENV{GTKMM_BASEPATH}
        CACHE STRING "Additional search paths")
endif()

if(APPLE) # TODO make -mtune generic conditional and/or specifiable.
    if(CMAKE_CXX_COMPILER MATCHES "g\\+\\+-mp-4.[5-8]"
       OR CMAKE_CXX_COMPILER_ARG1 MATCHES "g\\+\\+-mp-4.[5-8]")
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} /usr/lib/libstdc++.6.dylib -Wl,-headerpad_max_install_names -mtune=generic -framework Foundation"
        )
        message(
            STATUS
                "CMAKE_CXX_COMPILER is MacPorts GCC.\n   CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}"
        )
    endif()

    # Set minimum system version
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    )
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-headerpad_max_install_names -mtune=generic -framework Foundation"
    )
endif()

option(BUILD_SHARED "Build with shared libraries" OFF)
option(WARNINGS_AS_ERRORS "Error out on compiler warnings" OFF)
option(WITH_MYFILE_MMAP "Build using memory mapped file" ON)
option(WITH_LTO "Build with link-time optimizations" OFF)
option(WITH_SAN "Build with run-time sanitizer" OFF)
option(WITH_SYSTEM_FMT "Build using system FMT library." OFF)
option(WITH_SYSTEM_KLT "Build using system KLT library." OFF)
option(WITH_SYSTEM_LIBRAW "Build using system LibRaw library." OFF)
option(OPTION_OMP "Build with OpenMP support" ON)
option(WITH_SIMDE "Build with SIMD Everywhere support" OFF)

# Profiling options
option(WITH_BENCHMARK "Build with StopWatch benchmarking" OFF)
option(WITH_PROF "Build with gprof profiling instrumentation" OFF)
option(WITH_TRACY_PROFILER "Build with Tracy profiling client instrumentation" OFF)
cmake_dependent_option(WITH_TRACY_ON_DEMAND "Set TRACY_ON_DEMAND"
                       OFF "WITH_TRACY_PROFILER" OFF)
cmake_dependent_option(WITH_TRACY_ONLY_LOCALHOST "Set TRACY_ONLY_LOCALHOST"
                       ON "WITH_TRACY_PROFILER" ON)
cmake_dependent_option(WITH_TRACY_NO_BROADCAST "Set TRACY_NO_BROADCAST"
                       ON "WITH_TRACY_PROFILER" ON)
cmake_dependent_option(WITH_TRACY_MEMORY_PROFILING "Add memory profiling hooks for Tracy"
                       OFF "WITH_TRACY_PROFILER" OFF)

option(
    STRICT_MUTEX
    "True (recommended): MyMutex will behave like POSIX Mutex; False: MyMutex will behave like POSIX RecMutex; Note: forced to ON for Debug builds"
    ON)
cmake_dependent_option(
    TRACE_MYRWMUTEX
    "Trace custom R/W Mutex (Debug builds only); redirecting std::out to a file is strongly recommended!"
    OFF "IS_DEBUG"
    OFF # otherwise
)
option(
    AUTO_GDK_FLUSH
    "Use gdk_flush on all gdk_thread_leave other than the GUI thread; set it ON if you experience X Server warning/errors"
    OFF)

option(ENABLE_TCMALLOC "Use the tcmalloc library if available" OFF)
set(TCMALLOC_LIB_DIR
    ""
    CACHE PATH "Custom path for the tcmalloc library")

cmake_dependent_option(
    USE_LIBCANBERRA "Build with libcanberra"
    ON "UNIX;NOT APPLE"
    OFF # otherwise
)

set(WITH_JXL "AUTO" CACHE STRING "Build with JPEG XL support")
set_property(CACHE WITH_JXL PROPERTY STRINGS AUTO ON OFF)

set(SVG_BACKEND_OPTIONS librsvg lunasvg)
set(SVG_BACKEND librsvg CACHE STRING "SVG rendering library")
set_property(CACHE SVG_BACKEND PROPERTY STRINGS "${SVG_BACKEND_OPTIONS}")
if(NOT SVG_BACKEND IN_LIST SVG_BACKEND_OPTIONS)
    message(FATAL_ERROR "SVG_BACKEND must be one of ${SVG_BACKEND_OPTIONS}")
endif()
if("${SVG_BACKEND}" STREQUAL "lunasvg")
    message(WARNING
        " Using LunaSVG as an SVG rendering backend is experimental.\n"
        " LunaSVG is not tested during standard development and release cycles.\n"
        " Support for issues/bugs due to LunaSVG is limited.\n"
    )
endif()

# cmake_dependent_option doesn't work with CACHE variables
option(WITH_SYSTEM_LUNASVG "Build using system LunaSVG library" OFF)

# Set installation directories:
if(WIN32 OR APPLE)
    if(BUILD_BUNDLE)
        message(
            STATUS
                "You have set BUILD_BUNDLE=ON but this is not necessary - the option is forced to ON for Windows and macOS."
        )
    endif()
    set(BUILD_BUNDLE ON FORCE)
endif()

if(NOT DEFINED BUNDLE_BASE_INSTALL_DIR)
    if(APPLE)
        set(BUNDLE_BASE_INSTALL_DIR "${PROJECT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/MacOS")
    else()
        set(BUNDLE_BASE_INSTALL_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
    endif()
endif()

if(BUILD_BUNDLE)
    if(APPLE)
        set(BINDIR "${BUNDLE_BASE_INSTALL_DIR}")
        set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/build")
    else()
        set(BINDIR .)
        set(CMAKE_INSTALL_PREFIX "${BUNDLE_BASE_INSTALL_DIR}")
    endif()
endif()

if(NOT DEFINED BINDIR)
    set(BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if(NOT DEFINED DATADIR)
    if(BUILD_BUNDLE)
        if(APPLE)
            set(DATADIR "${PROJECT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/Resources/share")
        else()
            set(DATADIR .)
        endif()
    else()
        set(DATADIR "${CMAKE_INSTALL_PREFIX}/share/rawtherapee")
    endif()
endif()

if(NOT DEFINED LIBDIR)
    if(BUILD_BUNDLE)
        if(APPLE)
            set(LIBDIR "${PROJECT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/Frameworks")
        else()
            set(LIBDIR .)
        endif()
    else()
        # Respect CMAKE_INSTALL_LIBDIR if set
        if(DEFINED CMAKE_INSTALL_LIBDIR)
            if(IS_ABSOLUTE "${LIBDIR}")
                set(LIBDIR "${CMAKE_INSTALL_LIBDIR}")
            else()
                set(LIBDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
            endif()
        else()
            set(LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
        endif()
    endif()
endif()

if(NOT DEFINED DOCDIR)
    if(BUILD_BUNDLE)
        if(APPLE)
            set(DOCDIR "${DATADIR}/doc")
        else()
            set(DOCDIR "${DATADIR}/share/doc")
        endif()
    else()
        set(DOCDIR "${CMAKE_INSTALL_PREFIX}/share/doc/rawtherapee")
    endif()
endif()

if(NOT DEFINED CREDITSDIR)
    if(BUILD_BUNDLE)
        if(APPLE)
            set(CREDITSDIR "${DATADIR}/..")
        else()
            set(CREDITSDIR "${DATADIR}")
        endif()
    else()
        set(CREDITSDIR "${CMAKE_INSTALL_PREFIX}/share/doc/rawtherapee")
    endif()
endif()

if(NOT DEFINED LICENCEDIR)
    if(BUILD_BUNDLE)
        if(APPLE)
            set(LICENCEDIR "${CREDITSDIR}")
        else()
            set(LICENCEDIR "${DATADIR}")
        endif()
    else()
        set(LICENCEDIR "${CMAKE_INSTALL_PREFIX}/share/doc/rawtherapee")
    endif()
endif()

if(NOT DEFINED DESKTOPDIR)
    if(UNIX)
        if(BUILD_BUNDLE)
            if(APPLE)
                set(DESKTOPDIR "${DATADIR}/applications")
            else()
                set(DESKTOPDIR "${DATADIR}/share/applications")
            endif()
        else()
            set(DESKTOPDIR "${CMAKE_INSTALL_PREFIX}/share/applications")
        endif()
    endif()
endif()

if(NOT DEFINED ICONSDIR)
    if(UNIX)
        if(BUILD_BUNDLE)
            if(APPLE)
                set(ICONSDIR "${DATADIR}/icons")
            else()
                set(ICONSDIR "${DATADIR}/share/icons")
            endif()
        else()
            set(ICONSDIR "${CMAKE_INSTALL_PREFIX}/share/icons")
        endif()
    endif()
endif()

if(NOT DEFINED APPDATADIR)
    if(UNIX)
        if(BUILD_BUNDLE)
            if(APPLE)
                set(APPDATADIR "${DATADIR}/metainfo")
            else()
                set(APPDATADIR "${DATADIR}/share/metainfo")
            endif()
        else()
            set(APPDATADIR "${CMAKE_INSTALL_PREFIX}/share/metainfo")
        endif()
    endif()
endif()

if(NOT APPLE)
    if(DEFINED LENSFUNDBDIR AND NOT IS_ABSOLUTE "${LENSFUNDBDIR}")
        set(LENSFUNDBDIR "${DATADIR}/${LENSFUNDBDIR}")
    endif()
else()
    set(LENSFUNDBDIR "${LENSFUNDBDIR}")
endif()

if(APPLE)
    if("${CODESIGNID}")
        set(CODESIGNID
            "${CODESIGNID}"
            CACHE STRING "Codesigning Identity")
    else()
        set(CODESIGNID
            -
            CACHE STRING "Codesign ID set to ad-hoc")
    endif()
    if("${NOTARY}")
        set(NOTARY
            "${NOTARY}"
            CACHE STRING "Notarization Identity")
    endif()
    if("${LOCAL_PREFIX}")
        set(LOCAL_PREFIX
            "${LOCAL_PREFIX}"
            CACHE STRING "macos/gtk parent directory ie /usr/local or /opt/local or /opt/homebrew (for arm64)")
    else()
        set(LOCAL_PREFIX
            /usr/local
            CACHE STRING "macos/gtk parent directory ie /usr/local (default) or /opt/local (macports) or /opt/homebrew (for arm64)")
    endif()
    if("${FANCY_DMG}")
        set(FANCY_DMG
            ON
            CACHE BOOL
                  "Use the andreyvit/create-dmg script to make a fancy .dmg")
    endif()
endif()

# Enforce absolute paths for non-bundle builds:
if(NOT BUILD_BUNDLE)
    foreach(path BINDIR DATADIR LIBDIR DOCDIR CREDITSDIR LICENCEDIR)
        if(NOT (IS_ABSOLUTE "${${path}}"))
            message(
                FATAL_ERROR
                    "The ${path} path has to be absolute when using -DBUILD_BUNDLE=OFF"
            )
        endif()
    endforeach()
endif()

include(cmake/Dependencies.cmake)
rt_setup_dependencies()

if(WIN32)
    set(EXTRA_LIB "-lws2_32 -lshlwapi")
endif()

# Set the appropriate FFTW flags on macOS
if(APPLE AND OPTION_OMP)
    set(EXTRA_LIB "-L${LOCAL_PREFIX}/lib -lfftw3f_omp -lfftw3f -lm")
endif()

include(cmake/Compile.cmake)
add_subdirectory(external)
add_subdirectory(rtengine)
add_subdirectory(rtgui)
add_subdirectory(rtdata)

include(cmake/AboutThisBuild.cmake)
rt_generate_about_command(ABOUT_COMMAND_WITH_ARGS rtengine rth)
add_custom_target(
    UpdateInfo ALL
    COMMAND ${ABOUT_COMMAND_WITH_ARGS}
    COMMENT "Creating AboutThisBuild.txt and other version-dependent files")

install(FILES AUTHORS.txt DESTINATION "${CREDITSDIR}")
install(FILES LICENSE DESTINATION "${LICENCEDIR}")
install(FILES "${CMAKE_BINARY_DIR}/AboutThisBuild.txt"
        DESTINATION "${CREDITSDIR}")
install(
    FILES RELEASE_NOTES.txt
    DESTINATION "${CREDITSDIR}"
    OPTIONAL)

# The standard location for man pages in Linux is /usr/share/man Use "manpath"
# to see the search paths for man pages on your system.
if(BUILD_BUNDLE)
    if(APPLE)
        install(FILES "${PROJECT_SOURCE_DIR}/doc/manpage/rawtherapee.1"
                DESTINATION "${DATADIR}/man/man1")
    else()
        install(FILES "${PROJECT_SOURCE_DIR}/doc/manpage/rawtherapee.1"
                DESTINATION "${DATADIR}/share/man/man1")
    endif()
else()
    install(FILES "${PROJECT_SOURCE_DIR}/doc/manpage/rawtherapee.1"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1")
endif()

if(WIN32)
    install(DIRECTORY "licenses" DESTINATION "${LICENCEDIR}")
endif()

if(UNIX)
    install(FILES com.rawtherapee.RawTherapee.appdata.xml
            DESTINATION "${APPDATADIR}")
endif()
