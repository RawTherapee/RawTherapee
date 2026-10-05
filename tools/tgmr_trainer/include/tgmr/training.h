#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tgmr
{

enum class TrainingBackend {
    CANONICAL,
    CPU,
    OMP_TARGET,
};

struct MixtureModel final {
    std::size_t dimension = 0;
    std::size_t components = 0;
    std::vector<double> weights;
    std::vector<double> means;
    std::vector<double> scales;
    std::vector<double> effectiveCounts;
    double meanLogLikelihood = 0.0;
    std::uint32_t gaussianIterations = 0;
    std::uint32_t studentIterations = 0;
};

struct FitConfiguration final {
    std::size_t components = 32;
    std::uint32_t gaussianIterations = 10;
    std::uint32_t studentIterations = 30;
    double covarianceFloor = 1e-6;
    double degreesOfFreedom = 3.0;
    std::uint64_t seed = 0x5847544d52ULL;
    std::size_t batchSize = 4096;
    std::size_t maximumBytes = 8ULL * 1024 * 1024 * 1024;
    std::uint64_t sourceLimit = 0;
    int device = -1;
    unsigned gpuYieldMilliseconds = 0;
    TrainingBackend backend = TrainingBackend::CANONICAL;
};

using IterationCallback = std::function<void(
    const MixtureModel &, const char *stage, std::uint32_t iteration)>;

// Fit one dense full-covariance mixture.  Samples are a contiguous N x D
// row-major matrix.  The canonical backend uses one thread and fixed reduction
// order; CPU parallelism is component/batch based and changes only rounding.
MixtureModel fitStudentTMixture(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    std::size_t dimension,
    const FitConfiguration &configuration,
    const IterationCallback &callback = {});

MixtureModel continueStudentTMixture(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    const FitConfiguration &configuration,
    MixtureModel initial,
    std::uint32_t additionalGaussianIterations,
    std::uint32_t additionalStudentIterations,
    const IterationCallback &callback = {});

bool choleskyLower(
    const double *matrix,
    double *lower,
    std::size_t dimension);

double studentTLogDensity(
    const double *sample,
    const double *mean,
    const double *lower,
    std::size_t dimension,
    double degreesOfFreedom,
    double *quadratic = nullptr);

} // namespace tgmr
