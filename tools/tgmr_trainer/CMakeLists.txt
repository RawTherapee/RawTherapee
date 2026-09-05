cmake_minimum_required(VERSION 3.16)

# This directory is both a normal RawTherapee subdirectory and an independent
# CMake project.  The latter keeps the corpus/training tool usable without
# building rtengine or the GUI.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(rt_tgmr_train VERSION 0.1.0 LANGUAGES C CXX)
    include(CTest)
endif()

find_package(ZLIB REQUIRED)
find_package(OpenMP REQUIRED COMPONENTS CXX)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TGMR_LCMS REQUIRED IMPORTED_TARGET lcms2>=2.6)

option(TGMR_ENABLE_OMP_TARGET
    "Compile the experimental physical-GPU OpenMP-target trainer backend" OFF)
set(TGMR_OMP_TARGET_FLAGS "" CACHE STRING
    "Compiler/linker flags for the selected AMDGCN or NVPTX OpenMP target")

# The exported model binds the exact trainer implementation, not an arbitrary
# revision label supplied on the command line.  Relative paths plus individual
# file digests make the aggregate independent of the checkout location.
file(GLOB_RECURSE TGMR_TRAINER_REVISION_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
list(APPEND TGMR_TRAINER_REVISION_INPUTS
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/training_identity_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../rtengine/cJSON.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../rtengine/cJSON.h")
list(SORT TGMR_TRAINER_REVISION_INPUTS)
set(TGMR_TRAINER_REVISION_MANIFEST "")
foreach(TGMR_REVISION_INPUT IN LISTS TGMR_TRAINER_REVISION_INPUTS)
    file(RELATIVE_PATH TGMR_REVISION_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../.." "${TGMR_REVISION_INPUT}")
    file(SHA256 "${TGMR_REVISION_INPUT}" TGMR_REVISION_FILE_SHA256)
    string(APPEND TGMR_TRAINER_REVISION_MANIFEST
        "${TGMR_REVISION_PATH}:${TGMR_REVISION_FILE_SHA256}\n")
endforeach()
string(SHA256 TGMR_TRAINER_REVISION_SHA256 "${TGMR_TRAINER_REVISION_MANIFEST}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${TGMR_TRAINER_REVISION_INPUTS})
configure_file(src/training_identity_config.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tgmr/training_identity_config.h" @ONLY)

add_library(rt_tgmr_tooling STATIC
    src/camera_matrices.cc
    src/corpus.cc
    src/corpus_analysis.cc
    src/image.cc
    src/manifest.cc
    src/model_v2.cc
    src/patch_selection.cc
    src/sha256.cc
    src/source_selection.cc
    src/training.cc
    src/training_identity.cc
    src/validation.cc
    src/xtrans_training.cc
    ../../rtengine/cJSON.c)
target_include_directories(rt_tgmr_tooling
    PUBLIC include
    PRIVATE ../../rtengine "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(rt_tgmr_tooling PUBLIC cxx_std_17)
target_link_libraries(rt_tgmr_tooling PUBLIC
    ZLIB::ZLIB OpenMP::OpenMP_CXX JPEG::JPEG PNG::PNG TIFF::TIFF PkgConfig::TGMR_LCMS)
target_compile_definitions(rt_tgmr_tooling PUBLIC CMS_NO_REGISTER_KEYWORD=1)

# The canonical trainer mode relies on a fixed source-level reduction order.
# Prevent the compiler from silently changing each multiply/add into a fused
# operation so clean GCC and Clang builds have the same documented numerical
# contract.  Optimized CPU execution still uses OpenMP and vectorizable loops;
# only contraction is disabled.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rt_tgmr_tooling PRIVATE -ffp-contract=off)
endif()
if(TGMR_ENABLE_OMP_TARGET)
    if(TGMR_OMP_TARGET_FLAGS STREQUAL "")
        message(FATAL_ERROR
            "TGMR_ENABLE_OMP_TARGET requires explicit TGMR_OMP_TARGET_FLAGS for AMDGCN or NVPTX")
    endif()
    separate_arguments(TGMR_OMP_TARGET_FLAG_LIST NATIVE_COMMAND "${TGMR_OMP_TARGET_FLAGS}")
    target_compile_definitions(rt_tgmr_tooling PRIVATE TGMR_ENABLE_OMP_TARGET=1)
    target_compile_options(rt_tgmr_tooling PRIVATE ${TGMR_OMP_TARGET_FLAG_LIST})
    target_link_options(rt_tgmr_tooling INTERFACE ${TGMR_OMP_TARGET_FLAG_LIST})
endif()

add_executable(rt-tgmr-train src/main.cc)
target_compile_features(rt-tgmr-train PRIVATE cxx_std_17)
target_include_directories(rt-tgmr-train PRIVATE ../../rtengine)
target_link_libraries(rt-tgmr-train PRIVATE rt_tgmr_tooling)

if(COMMAND rt_setup_target)
    rt_setup_target(rt_tgmr_tooling)
    rt_setup_target(rt-tgmr-train)
endif()

if(BUILD_TESTING)
    add_executable(rt-tgmr-trainer-tests tests/tooling_tests.cc)
    target_compile_features(rt-tgmr-trainer-tests PRIVATE cxx_std_17)
    target_link_libraries(rt-tgmr-trainer-tests PRIVATE rt_tgmr_tooling)
    if(COMMAND rt_setup_target)
        rt_setup_target(rt-tgmr-trainer-tests)
    endif()
    add_test(NAME tgmr.trainer.tooling COMMAND rt-tgmr-trainer-tests)
    set_tests_properties(tgmr.trainer.tooling PROPERTIES
        LABELS "tgmr;trainer;corpus" TIMEOUT 60)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME tgmr.trainer.python-corpus
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s tools/tgmr_trainer/tests -p "test_*.py")
        set_tests_properties(tgmr.trainer.python-corpus PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
            LABELS "tgmr;trainer;corpus" TIMEOUT 60)
    endif()
endif()

install(TARGETS rt-tgmr-train RUNTIME DESTINATION bin OPTIONAL)
