/*
 * Developer-only array entry point for RawTherapee's Markesteijn X-Trans
 * implementation.  Production processing continues to use RawImageSource;
 * this interface exists so research tests can compare the actual algorithm
 * against exact linear-RGB ground truth without passing through output color
 * management.
 */
#pragma once

#include <string>

namespace rtengine
{

enum class MarkesteijnXTransErrorCode {
    NONE,
    SIZE,
    CFA,
    ALLOCATION,
    NONFINITE,
    INTERNAL
};

const char *markesteijnXTransErrorCodeName(MarkesteijnXTransErrorCode code);

struct MarkesteijnXTransRunResult final {
    MarkesteijnXTransErrorCode code;
    std::string message;

    MarkesteijnXTransRunResult(
        MarkesteijnXTransErrorCode errorCode = MarkesteijnXTransErrorCode::NONE,
        const std::string &errorMessage = {})
        : code(errorCode), message(errorMessage)
    {
    }

    explicit operator bool() const
    {
        return code == MarkesteijnXTransErrorCode::NONE;
    }
};

// All planes are contiguous row-major float32 in the RawTherapee 0..65535
// camera-linear domain.  The three-pass/CIE-Lab path is used, matching the
// ordinary "3-pass (best)" method without any later processing.
MarkesteijnXTransRunResult demosaicMarkesteijnXTransReference(
    const float *mosaic,
    float *red,
    float *green,
    float *blue,
    int width,
    int height,
    const int xtrans[6][6]);

} // namespace rtengine
