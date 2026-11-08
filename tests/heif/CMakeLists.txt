cmake_minimum_required(VERSION 3.15)
project(RawTherapeeHeifFeasibility LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(WITH_HEIF "ON" CACHE STRING "Build with HEIF support")
set_property(CACHE WITH_HEIF PROPERTY STRINGS AUTO ON OFF)

find_package(PkgConfig REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/Heif.cmake")
rt_setup_heif()

include(CTest)
if(BUILD_TESTING)
    foreach(mode OFF AUTO ON INVALID)
        add_test(NAME heif-discovery-${mode}
            COMMAND "${CMAKE_COMMAND}" "-DMODE=${mode}"
                "-DGENERATOR=${CMAKE_GENERATOR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DBINARY=${CMAKE_CURRENT_BINARY_DIR}/discovery-${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/check-discovery.cmake")
    endforeach()
endif()
if(HEIF_FOUND)
    pkg_check_modules(LCMS REQUIRED IMPORTED_TARGET lcms2>=2.6)
    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.48)
    add_executable(heif-feasibility feasibility.cc)
    target_link_libraries(heif-feasibility PRIVATE
        PkgConfig::HEIF PkgConfig::LCMS PkgConfig::GLIB)
    if(MSVC)
        target_compile_options(heif-feasibility PRIVATE /W4)
    else()
        target_compile_options(heif-feasibility PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME heif-capabilities COMMAND heif-feasibility --capabilities)
    add_test(NAME heif-roundtrip COMMAND heif-feasibility
        --roundtrip "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
    set_tests_properties(heif-roundtrip PROPERTIES TIMEOUT 180)
    add_test(NAME heif-failure-checks COMMAND heif-feasibility --failure-checks
        "${CMAKE_CURRENT_BINARY_DIR}/artifacts/8-444-icc-only.heic"
        "${CMAKE_CURRENT_BINARY_DIR}/failure-output")
    set_tests_properties(heif-failure-checks PROPERTIES DEPENDS heif-roundtrip)
endif()
