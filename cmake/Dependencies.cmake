include(FetchContent)
find_package(PkgConfig REQUIRED)

include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

# Use a macro as functions/find_package()/pkg_check_modules() do not set
# variables in the top-level CMakeLists scope.
macro(rt_setup_dependencies)
    rt_fetch_content()

    find_package(ATOMIC)
    if(ENABLE_TCMALLOC)
        find_package(TCMALLOC)
    endif()

    find_package(JPEG REQUIRED)
    find_package(PNG REQUIRED)
    find_package(TIFF 4.0.4 REQUIRED)
    find_package(ZLIB REQUIRED)

    # Gtk version shall be greater than 3.24.3 for fixed Hi-DPI support
    pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0>=3.24.3)
    pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0>=3.24)

    if(GTK_VERSION VERSION_GREATER "3.24.1" AND GTK_VERSION VERSION_LESS "3.24.7")
        if(GTK_VERSION VERSION_EQUAL "3.24.5")
            set(CERTAINTY "known to")
        else()
            set(CERTAINTY "likely to")
        endif()
        message(
            WARNING
                "\nWarning! You are using GTK+ version "
                ${GTK_VERSION}
                " which is "
                ${CERTAINTY}
                " have an issue where combobox menu scroll-arrows are missing when a Gtk::ComboBox list does not fit vertically on the screen. As a result, users of your build will not be able to select items in the following comboboxes: Processing Profiles, Film Simulation, and the camera and lens profiles in Profiled Lens Correction.\nIt is recommended that you either downgrade GTK+ to <= 3.24.1 or upgrade to >= 3.24.7."
        )
    endif()

    # These should be transitively included from PkgConfig::GTKMM.
    pkg_check_modules(CAIROMM REQUIRED IMPORTED_TARGET cairomm-1.0)
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0>=2.48)
    pkg_check_modules(GIOMM REQUIRED IMPORTED_TARGET giomm-2.4>=2.48)
    pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0>=2.48)
    pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.4>=2.48)
    pkg_check_modules(GOBJECT REQUIRED IMPORTED_TARGET gobject-2.0>=2.48)
    pkg_check_modules(GTHREAD REQUIRED IMPORTED_TARGET gthread-2.0>=2.48)
    pkg_check_modules(SIGC REQUIRED IMPORTED_TARGET sigc++-2.0>=2.3.1)

    pkg_check_modules(EXIV2 REQUIRED IMPORTED_TARGET exiv2>=0.24)
    pkg_check_modules(EXPAT REQUIRED IMPORTED_TARGET expat>=2.1)
    pkg_check_modules(IPTCDATA REQUIRED IMPORTED_TARGET libiptcdata)
    if("${SVG_BACKEND}" STREQUAL "librsvg")
        pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.52)
    elseif("${SVG_BACKEND}" STREQUAL "lunasvg")
        if(WITH_SYSTEM_LUNASVG)
            pkg_check_modules(LUNASVG REQUIRED IMPORTED_TARGET lunasvg>=3.3.0)
        endif()
    else()
        message(FATAL_ERROR "Unknown value for SVG_BACKEND")
    endif()

    pkg_check_modules(LCMS REQUIRED IMPORTED_TARGET lcms2>=2.6)
    # By default, little-cms2 uses the 'register' keyword which is deprecated
    # and removed in c++17. This definition forces it not to use the keyword.
    target_compile_definitions(PkgConfig::LCMS INTERFACE "CMS_NO_REGISTER_KEYWORD")

    pkg_check_modules(LENSFUN REQUIRED IMPORTED_TARGET lensfun>=0.2)
    # Sets LENSFUN_HAS_LOAD_DIRECTORY
    rt_check_lensfun_has_load_directory()

    # Check before calling rt_add_fftw3f_omp_support()
    pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)
    if(OPTION_OMP)
        find_package(OpenMP)
        if(OpenMP_CXX_FOUND)
            # Assumes FFTW3F is the package name for fftw3f
            rt_add_fftw3f_omp_support()
        endif()
    endif()

    rt_setup_jxl()

    if(WITH_SYSTEM_FMT)
        find_package(FMT REQUIRED)
    endif()

    if(WITH_SYSTEM_KLT)
        find_package(KLT REQUIRED)
    endif()

    if(WITH_SYSTEM_LIBRAW)
        pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw_r>=0.21)
    else()
        set(LIBRAW_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/external/libraw/lib/.libs/libraw_r.a")
        if(WIN32)
            list(APPEND LIBRAW_LIBRARIES "-lws2_32")
        endif()
    endif()

    # Check for libcanberra-gtk3 (sound events on Linux):
    if(USE_LIBCANBERRA)
        pkg_check_modules(CANBERRA REQUIRED IMPORTED_TARGET libcanberra-gtk3)
        target_compile_definitions(PkgConfig::CANBERRA INTERFACE "USE_CANBERRA")
    endif()

    if(WITH_SIMDE)
        pkg_check_modules(SIMDE REQUIRED simde)
        add_compile_definitions(RT_SIMDE)
        add_compile_definitions(SIMDE_ENABLE_NATIVE_ALIASES)
    endif()
endmacro()

macro(rt_fetch_content)
    set(DEPS)

    # fmt::fmt
    if(NOT WITH_SYSTEM_FMT)
        set(FMT_INSTALL OFF) # Static library doesn't need separate install
        set(FMT_SYSTEM_HEADERS ON) # Exclude headers from linters
        set(FMT_UNICODE ON)
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt
            GIT_TAG e424e3f2e607da02742f73db84873b8084fc714c # 12.0.0
            GIT_SHALLOW ON
        )
        list(APPEND DEPS fmt)
    endif()

    # lunasvg::lunasvg
    if("${SVG_BACKEND}" STREQUAL "lunasvg")
        if(NOT WITH_SYSTEM_LUNASVG)
            set(LUNASVG_INSTALL OFF)
            set(PLUTOVG_INSTALL OFF)
            FetchContent_Declare(
                lunasvg
                # Use a forked version that has an option to disable the
                # separate installation of the library
                GIT_REPOSITORY https://github.com/digitalcarp/lunasvg
                GIT_TAG c0021e1cd0bf39d182090b68b9ff8bd2ee92b372
                # GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
                # GIT_TAG 83c58df8103dc7dca423dfd824992af94d49bed6 # v3.5.0
                GIT_SHALLOW ON
            )
            list(APPEND DEPS lunasvg)
        endif()
    endif()

    # Tracy::TracyClient
    if(WITH_TRACY_PROFILER)
        set(TRACY_ENABLE ON)
        set(TRACY_ON_DEMAND "${WITH_TRACY_ON_DEMAND}")
        set(TRACY_ONLY_LOCALHOST "${WITH_TRACY_ONLY_LOCALHOST}")
        set(TRACY_NO_BROADCAST "${WITH_TRACY_NO_BROADCAST}")
        set(TRACY_NO_CALLSTACK_INLINES ON)
        set(TRACY_NO_FRAME_IMAGE ON)
        set(TRACY_NO_VSYNC_CAPTURE ON)
        FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG 099df3de3dc37eca4712c06b8320fb9c53596edd # v0.14.0
            GIT_SHALLOW ON
        )
        list(APPEND DEPS tracy)
    endif()

    # Add all FetchContent-declared libraries here.
    # Don't use FetchContent_Declare after this.
    FetchContent_MakeAvailable(${DEPS})
endmacro()

function(rt_add_fftw3f_omp_support)
    # Check for libfftw3f_omp
    if(NOT FFTW3F_FOUND)
        message(FATAL_ERROR "Missing PkgConfig::FFTW3F target")
    endif()

    # Prepare for check_c_source_compiles()
    set(CMAKE_REQUIRED_INCLUDES ${FFTW3F_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES)
    foreach(l ${FFTW3F_LIBRARIES})
        set(_f "_f-NOTFOUND")
        find_library(_f ${l} PATHS ${FFTW3F_LIBRARY_DIRS})
        list(APPEND CMAKE_REQUIRED_LIBRARIES ${_f})
    endforeach()

    check_c_source_compiles(
        "
        #include <fftw3.h>
        int main()
        {
            fftwf_init_threads();
            fftwf_plan_with_nthreads(1);
            return 0;
        }
        "
        FFTW3F_SUPPORTS_MULTITHREADING
    )
    if(FFTW3F_SUPPORTS_MULTITHREADING)
        target_compile_definitions(PkgConfig::FFTW3F INTERFACE "RT_FFTW3F_OMP")
        return()
    endif()

    message(STATUS "Searching for library fftw3f_omp")
    find_library(fftw3f_omp fftw3f_omp PATHS ${FFTW3F_LIBRARY_DIRS})
    if(fftw3f_omp)
        message(STATUS "  Found ${fftw3f_omp}")
        target_link_libraries(PkgConfig::FFTW3F INTERFACE ${fftw3f_omp})
        target_compile_definitions(PkgConfig::FFTW3F INTERFACE "RT_FFTW3F_OMP")
    endif()
endfunction()

macro(rt_setup_jxl)
    if(NOT (WITH_JXL OR WITH_JXL STREQUAL "AUTO"))
        set(JXL_VERSION "Disabled")
    else()
        if(NOT WITH_JXL STREQUAL "AUTO")
            set(JXL_REQUIRED "REQUIRED")
        else()
            set(JXL_REQUIRED "")
        endif()

        pkg_check_modules(JXL ${JXL_REQUIRED} IMPORTED_TARGET libjxl)
        pkg_check_modules(JXLTHREADS ${JXL_REQUIRED} IMPORTED_TARGET libjxl_threads)

        if(JXL_FOUND)
            target_compile_definitions(PkgConfig::JXL INTERFACE "LIBJXL")
        else()
            set(JXL_VERSION "Disabled, Auto")
        endif()
    endif()
endmacro()

# Check whether the used version of lensfun has lfDatabase::LoadDirectory
function(rt_check_lensfun_has_load_directory)
    if(NOT LENSFUN_FOUND)
        message(FATAL_ERROR "Missing PkgConfig::LENSFUN")
    endif()

    set(CMAKE_REQUIRED_INCLUDES ${LENSFUN_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES)
    foreach(l ${LENSFUN_LIBRARIES})
        if(LENSFUN_LIBRARY_DIRS)
            # the NO_DEFAULT_PATH is to make sure we find the lensfun version we
            # want, and not the system's one (e.g. if we have a custom version
            # installed in a non-standard location)
            set(_l "_l-NOTFOUND")
            message(STATUS "Searching for library ${l} in ${LENSFUN_LIBRARY_DIRS}")
            find_library(_l ${l} PATHS ${LENSFUN_LIBRARY_DIRS} NO_DEFAULT_PATH)
        else()
            # LENSFUN_LIBRARY_DIRS can be empty if lensfun is installed in the
            # default path. In this case, adding NO_DEFAULT_PATH would make
            # find_library fail...
            set(_l "_l-NOTFOUND")
            message(STATUS "searching for library ${l}")
            find_library(_l ${l})
        endif()
        message(STATUS " Found ${_l}")
        list(APPEND CMAKE_REQUIRED_LIBRARIES ${_l})
    endforeach()

    check_cxx_source_compiles(
        "
        #include <lensfun.h>
        int main()
        {
            lfDatabase *db = 0;
            bool b = db->LoadDirectory(0);
            return 0;
        }
        "
        LENSFUN_HAS_LOAD_DIRECTORY
    )
    set(LENSFUN_HAS_LOAD_DIRECTORY ${LENSFUN_HAS_LOAD_DIRECTORY} PARENT_SCOPE)
endfunction()
