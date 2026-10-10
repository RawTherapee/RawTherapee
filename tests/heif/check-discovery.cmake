if(NOT DEFINED MODE OR NOT DEFINED SOURCE OR NOT DEFINED BINARY)
    message(FATAL_ERROR "MODE, SOURCE, and BINARY are required")
endif()

# Hide all .pc files while retaining the real pkg-config executable. This
# verifies absent-library behavior without depending on the host installation.
file(MAKE_DIRECTORY "${BINARY}/empty-pkgconfig")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PKG_CONFIG_PATH=" "PKG_CONFIG_LIBDIR=${BINARY}/empty-pkgconfig"
        "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${BINARY}"
        -G "${GENERATOR}"
        "-DWITH_HEIF=${MODE}" -DBUILD_TESTING=OFF
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
file(WRITE "${BINARY}/discovery.log" "${output}${errors}")
if(MODE STREQUAL "ON")
    if(result EQUAL 0 OR NOT "${output}${errors}" MATCHES "libheif")
        message(FATAL_ERROR "ON must fail for missing libheif: ${output}${errors}")
    endif()
elseif(MODE STREQUAL "OFF" OR MODE STREQUAL "AUTO")
    if(NOT result EQUAL 0 OR NOT output MATCHES "HEIF: Disabled")
        message(FATAL_ERROR "${MODE} must configure without libheif: ${output}${errors}")
    endif()
else()
    if(result EQUAL 0 OR NOT "${output}${errors}" MATCHES "WITH_HEIF must be")
        message(FATAL_ERROR "Invalid mode must fail clearly: ${output}${errors}")
    endif()
endif()
