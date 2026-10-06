add_library(cjson cJSON/src/cJSON.c)
add_library(External::cjson ALIAS cjson)
target_include_directories(cjson
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cJSON/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cJSON/include/cjson
)
rt_setup_target(cjson)

# [tl::expected v1.3.1](https://github.com/TartanLlama/expected)
add_library(expected INTERFACE)
add_library(tl::expected ALIAS expected)
target_include_directories(expected INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/expected)

add_library(jpeg_ijg jpeg_ijg/src/jdatasrc.cc jpeg_ijg/src/jpeg_memsrc.cc)
add_library(External::jpeg_ijg ALIAS jpeg_ijg)
target_include_directories(jpeg_ijg
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/jpeg_ijg/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/jpeg_ijg/include/jpeg_ijg
)
target_link_libraries(jpeg_ijg PUBLIC JPEG::JPEG)
rt_setup_target(jpeg_ijg)

if(NOT WITH_SYSTEM_KLT)
    add_library(klt
        klt/src/convolve.cc
        klt/src/error.cc
        klt/src/klt_util.cc
        klt/src/klt.cc
        klt/src/pnmio.cc
        klt/src/pyramid.cc
        klt/src/selectGoodFeatures.cc
        klt/src/storeFeatures.cc
        klt/src/trackFeatures.cc
        klt/src/writeFeatures.cc
    )
    add_library(External::klt ALIAS klt)
    target_include_directories(klt
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/klt/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/klt/include/klt
    )
    rt_setup_target(klt)
else()
    add_library(klt INTERFACE)
    add_library(External::klt ALIAS klt)
    target_link_libraries(klt INTERFACE PkgConfig::KLT)
endif()

add_library(RtLibRaw INTERFACE)
add_library(External::LibRaw ALIAS RtLibRaw)
if(NOT WITH_SYSTEM_LIBRAW)
    include(LibRaw.cmake)
    target_include_directories(RtLibRaw INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/libraw")
    target_link_libraries(RtLibRaw INTERFACE ${LIBRAW_LIBRARIES})
else()
    target_link_libraries(RtLibRaw INTERFACE PkgConfig::LIBRAW)
endif()

# [nonstd/span-lite v0.11.0](https://github.com/nonstd-lite/span-lite)
add_library(span INTERFACE)
add_library(External::span ALIAS span)
target_include_directories(span INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/span-lite)
target_compile_definitions(span INTERFACE
    span_FEATURE_MEMBER_AT=2
    span_FEATURE_NON_MEMBER_FIRST_LAST_SUB=1
)
