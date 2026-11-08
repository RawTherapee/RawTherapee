# Kept separate so the standalone HEIF feasibility tests use the same discovery
# rules as RawTherapee without configuring its other dependencies.
include_guard(GLOBAL)

set(RT_HEIF_MIN_VERSION "1.23.6")

macro(rt_setup_heif)
    set(_RT_HEIF_MODES AUTO ON OFF)
    if(NOT WITH_HEIF IN_LIST _RT_HEIF_MODES)
        message(FATAL_ERROR "WITH_HEIF must be AUTO, ON, or OFF")
    endif()

    set(HEIF_FOUND FALSE)
    if(WITH_HEIF STREQUAL "OFF")
        set(HEIF_VERSION "Disabled")
    else()
        if(WITH_HEIF STREQUAL "ON")
            pkg_check_modules(HEIF REQUIRED IMPORTED_TARGET
                "libheif>=${RT_HEIF_MIN_VERSION}")
        else()
            pkg_check_modules(HEIF QUIET IMPORTED_TARGET
                "libheif>=${RT_HEIF_MIN_VERSION}")
        endif()

        if(HEIF_FOUND)
            target_compile_definitions(PkgConfig::HEIF INTERFACE LIBHEIF)
        else()
            set(HEIF_VERSION "Disabled, Auto (libheif >= ${RT_HEIF_MIN_VERSION} not found)")
        endif()
    endif()
    message(STATUS "HEIF: ${HEIF_VERSION}")
endmacro()
