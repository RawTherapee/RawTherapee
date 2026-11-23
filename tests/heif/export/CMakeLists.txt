# Exercise the production engine and image types, including disabled builds.
set(heif_test_gui_sources
    ${PROJECT_SOURCE_DIR}/rtgui/alignedmalloc.cc
    ${PROJECT_SOURCE_DIR}/rtgui/editcallbacks.cc
    ${PROJECT_SOURCE_DIR}/rtgui/multilangmgr.cc
    ${PROJECT_SOURCE_DIR}/rtgui/options.cc
    ${PROJECT_SOURCE_DIR}/rtgui/paramsedited.cc
    ${PROJECT_SOURCE_DIR}/rtgui/pathutils.cc
    ${PROJECT_SOURCE_DIR}/rtgui/threadutils.cc)
add_executable(heif-export-checks engine-export.cc ${heif_test_gui_sources})
rt_setup_target(heif-export-checks)
target_compile_definitions(heif-export-checks PRIVATE CLIVERSION)
target_include_directories(heif-export-checks PRIVATE ${PROJECT_BINARY_DIR}/rtgui)
target_link_libraries(heif-export-checks PRIVATE rtengine)
add_test(NAME heif-engine-export COMMAND heif-export-checks "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set_tests_properties(heif-engine-export PROPERTIES TIMEOUT 180)

add_executable(heif-metadata-checks metadata-export.cc ${heif_test_gui_sources})
rt_setup_target(heif-metadata-checks)
target_compile_definitions(heif-metadata-checks PRIVATE CLIVERSION)
target_include_directories(heif-metadata-checks PRIVATE ${PROJECT_BINARY_DIR}/rtgui)
target_link_libraries(heif-metadata-checks PRIVATE rtengine)
add_test(NAME heif-export-metadata COMMAND heif-metadata-checks "${CMAKE_CURRENT_BINARY_DIR}/metadata-artifacts")
set_tests_properties(heif-export-metadata PROPERTIES TIMEOUT 120)

add_executable(heif-import-checks ../import/engine-import.cc ${heif_test_gui_sources})
rt_setup_target(heif-import-checks)
target_compile_definitions(heif-import-checks PRIVATE CLIVERSION)
target_include_directories(heif-import-checks PRIVATE ${PROJECT_BINARY_DIR}/rtgui)
target_link_libraries(heif-import-checks PRIVATE rtengine)
add_test(NAME heif-engine-import COMMAND heif-import-checks "${CMAKE_CURRENT_BINARY_DIR}/import-artifacts")
set_tests_properties(heif-engine-import PROPERTIES TIMEOUT 180 ENVIRONMENT "OMP_NUM_THREADS=4")

# Test the production fallback without replacing Exiv2's buffer parsers.
# This interposition harness uses the Exiv2 >=0.28 ABI and Linux loader.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXIV2_VERSION VERSION_GREATER_EQUAL "0.28")
    add_library(heif-no-bmff SHARED ../import/no-bmff.cc)
    target_link_libraries(heif-no-bmff PRIVATE PkgConfig::EXIV2)
    add_dependencies(heif-import-checks heif-no-bmff)
    if(HEIF_FOUND)
        add_test(NAME heif-import-metadata-fallback COMMAND ${CMAKE_COMMAND} -E env
            "LD_PRELOAD=$<TARGET_FILE:heif-no-bmff>" "OMP_NUM_THREADS=4"
            $<TARGET_FILE:heif-import-checks> "${CMAKE_CURRENT_BINARY_DIR}/import-artifacts" --read-only)
        set_tests_properties(heif-import-metadata-fallback PROPERTIES DEPENDS heif-engine-import TIMEOUT 30)
    endif()
endif()
