#pragma once

#include "tgmr/corpus.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tgmr
{

struct SourceValidationMetrics final {
    std::uint32_t sourceOrdinal = 0;
    std::uint64_t patches = 0;
    std::uint64_t scalarValues = 0;
    double mse = 0.0;
    double psnr = 0.0;
};

struct StratumValidationMetrics final {
    std::uint64_t patches = 0;
    std::uint64_t scalarValues = 0;
    double mse = 0.0;
    double psnr = 0.0;
};

struct ValidationMetrics final {
    std::uint64_t patches = 0;
    std::uint64_t scalarValues = 0;
    double mse = 0.0;
    double psnr = 0.0;
    double patchRmsP99 = 0.0;
    double worstPatchRms = 0.0;
    double medianSourcePsnr = 0.0;
    double phasePsnrSpread = 0.0;
    std::array<double, 18> phasePsnr{};
    std::vector<SourceValidationMetrics> sources;
    // brightness, chroma, texture; then low, middle, high.
    std::array<std::array<StratumValidationMetrics, 3>, 3> strata{};
};

struct ValidationReport final {
    std::string modelSha256;
    std::string corpusPayloadSha256;
    CorpusSplit split = CorpusSplit::VALIDATION;
    std::uint64_t requestedLimit = 0;
    double elapsedSeconds = 0.0;
    ValidationMetrics metrics;
};

// Evaluate every selected RGB patch through every frozen X-Trans phase using
// the same scalar K32/S9/q8 equations as the production engine. The physically
// measured center component is restored before error measurement.
ValidationReport validateModelOnCorpus(
    const std::string &modelPath,
    const std::string &corpusPath,
    CorpusSplit split,
    std::uint64_t patchLimit = 0);

// Canonical validation JSON rounds both values to twelve digits after the
// decimal point.  Authenticate their mathematical relationship using the
// resulting MSE interval instead of applying an unrealistically small PSNR
// tolerance to independently rounded values.
bool canonicalMsePsnrConsistent(double mse, double psnr);

std::string canonicalValidationJson(const ValidationReport &report);

} // namespace tgmr
