add_executable(
    rawtherapee-xtrans-tgmr-tests
    tgmr_tests.cc
    ${PROJECT_SOURCE_DIR}/rtgui/alignedmalloc.cc
    ${PROJECT_SOURCE_DIR}/rtgui/editcallbacks.cc
    ${PROJECT_SOURCE_DIR}/rtgui/multilangmgr.cc
    ${PROJECT_SOURCE_DIR}/rtgui/options.cc
    ${PROJECT_SOURCE_DIR}/rtgui/paramsedited.cc
    ${PROJECT_SOURCE_DIR}/rtgui/pathutils.cc
    ${PROJECT_SOURCE_DIR}/rtgui/threadutils.cc)
target_link_libraries(rawtherapee-xtrans-tgmr-tests PRIVATE rtengine)
target_include_directories(
    rawtherapee-xtrans-tgmr-tests
    BEFORE PRIVATE
        ${CMAKE_BINARY_DIR}/rtgui
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/rtgui)
target_compile_definitions(rawtherapee-xtrans-tgmr-tests PRIVATE CLIVERSION)
rt_setup_target(rawtherapee-xtrans-tgmr-tests)

if(BUILD_TGMR_TRAINER)
    add_executable(
        rawtherapee-xtrans-tgmr-evaluate
        tgmr_evaluate.cc
        ${PROJECT_SOURCE_DIR}/rtgui/alignedmalloc.cc
        ${PROJECT_SOURCE_DIR}/rtgui/editcallbacks.cc
        ${PROJECT_SOURCE_DIR}/rtgui/multilangmgr.cc
        ${PROJECT_SOURCE_DIR}/rtgui/options.cc
        ${PROJECT_SOURCE_DIR}/rtgui/paramsedited.cc
        ${PROJECT_SOURCE_DIR}/rtgui/pathutils.cc
        ${PROJECT_SOURCE_DIR}/rtgui/threadutils.cc)
    target_link_libraries(
        rawtherapee-xtrans-tgmr-evaluate
        PRIVATE rtengine rt_tgmr_tooling)
    target_include_directories(
        rawtherapee-xtrans-tgmr-evaluate
        BEFORE PRIVATE
            ${PROJECT_SOURCE_DIR}/rtengine
            ${CMAKE_BINARY_DIR}/rtgui
            ${PROJECT_SOURCE_DIR})
    target_compile_definitions(rawtherapee-xtrans-tgmr-evaluate PRIVATE CLIVERSION)
    rt_setup_target(rawtherapee-xtrans-tgmr-evaluate)
endif()

add_test(
    NAME rtengine.xtrans_tgmr.contract
    COMMAND rawtherapee-xtrans-tgmr-tests contract)
set_tests_properties(
    rtengine.xtrans_tgmr.contract
    PROPERTIES LABELS "rtengine;xtrans-tgmr" TIMEOUT 60)

add_test(
    NAME rtengine.xtrans_tgmr.reviewed_parity
    COMMAND rawtherapee-xtrans-tgmr-tests reviewed-parity)
set_tests_properties(
    rtengine.xtrans_tgmr.reviewed_parity
    PROPERTIES LABELS "rtengine;xtrans-tgmr" TIMEOUT 300 SKIP_RETURN_CODE 77)

add_test(
    NAME rtengine.xtrans_tgmr.crop_phase
    COMMAND rawtherapee-xtrans-tgmr-tests crop-phase)
set_tests_properties(
    rtengine.xtrans_tgmr.crop_phase
    PROPERTIES LABELS "rtengine;xtrans-tgmr" TIMEOUT 300 SKIP_RETURN_CODE 77)
