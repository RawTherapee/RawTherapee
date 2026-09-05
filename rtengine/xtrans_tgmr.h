/*
 * Experimental X-Trans Student-t Gaussian-mixture regression demosaicer.
 *
 * The statistical contract is the frozen K32/S9/q8 model documented in
 * devnotes/xtrans-tgmr-reduction-report.md and the native implementation in
 * devnotes/xtrans-tgmr-native-optimization-report.md.  Learned coefficients
 * are deliberately kept in an external authenticated artifact.
 */
#pragma once

#include "array2D.h"

#include <cstdint>
#include <memory>
#include <string>

namespace rtengine
{

constexpr const char *TGMR_XTRANS_METHOD = "tgmr";

enum class TgmrXTransErrorCode {
    NONE,
    IO,
    SIZE,
    DIGEST,
    FORMAT,
    CFA,
    ALLOCATION,
    NONFINITE,
    INTERNAL
};

const char *tgmrXTransErrorCodeName(TgmrXTransErrorCode code);

class TgmrXTransModel;

struct TgmrXTransLoadResult final {
    std::shared_ptr<const TgmrXTransModel> model;
    TgmrXTransErrorCode code = TgmrXTransErrorCode::NONE;
    std::string message;

    explicit operator bool() const
    {
        return model && code == TgmrXTransErrorCode::NONE;
    }
};

struct TgmrXTransRunResult final {
    TgmrXTransErrorCode code = TgmrXTransErrorCode::NONE;
    std::string message;
    std::uint64_t pixelCount = 0;
    std::uint64_t tileCount = 0;
    std::uint64_t workingBytesPerWorker = 0;
    std::uint32_t workerCount = 0;
    std::uint64_t elapsedMicroseconds = 0;
    bool avx2 = false;
    bool neon = false;

    explicit operator bool() const
    {
        return code == TgmrXTransErrorCode::NONE;
    }
};

TgmrXTransLoadResult loadTgmrXTransModel(const std::string &path);

// Successful models remain strongly cached for the process lifetime.  Failed
// loads are never cached so an externally supplied artifact can be fixed and
// retried without restarting RawTherapee.
TgmrXTransLoadResult loadCachedTgmrXTransModel(const std::string &path);

const std::string &tgmrXTransModelDigest(const TgmrXTransModel &model);
const char *tgmrXTransModelOrigin(const TgmrXTransModel &model);
bool tgmrXTransModelIsOfficial(const TgmrXTransModel &model);

// Registered once from rtengine::init().  It allows engine-only and CLI use
// to find an installed reviewed model without depending on rtgui/config.h.
void setTgmrXTransDataDirectory(const std::string &path);
std::string tgmrXTransDefaultModelPath();

TgmrXTransRunResult demosaicTgmrXTrans(
    const array2D<float> &rawData,
    array2D<float> &red,
    array2D<float> &green,
    array2D<float> &blue,
    int width,
    int height,
    const int xtrans[6][6],
    const std::shared_ptr<const TgmrXTransModel> &model,
    int originX = 0,
    int originY = 0,
    bool forceScalar = false);

// Flat-buffer entry point for deterministic native integration tests.  Planes
// use RawTherapee's ordinary 0..65535 nominal scale and row-major layout.
TgmrXTransRunResult demosaicTgmrXTransReference(
    const float *rawData,
    float *red,
    float *green,
    float *blue,
    int width,
    int height,
    const int xtrans[6][6],
    const std::shared_ptr<const TgmrXTransModel> &model,
    int originX = 0,
    int originY = 0,
    bool forceScalar = false);

} // namespace rtengine
