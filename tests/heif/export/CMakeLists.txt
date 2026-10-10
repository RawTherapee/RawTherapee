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
