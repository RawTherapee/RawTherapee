#include "tgmr/training.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <tuple>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace tgmr
{
namespace
{

constexpr double PI = 3.141592653589793238462643383279502884;

std::size_t checkedMultiply(std::size_t left, std::size_t right, const char *what)
{
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
        throw std::runtime_error(std::string("TGMR size overflow: ") + what);
    }
    return left * right;
}

std::size_t checkedAdd(std::size_t left, std::size_t right, const char *what)
{
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw std::runtime_error(std::string("TGMR size overflow: ") + what);
    }
    return left + right;
}

class Random final
{
public:
    explicit Random(std::uint64_t seed) : state_(seed ? seed : 1) {}

    std::uint64_t next()
    {
        std::uint64_t value = state_;
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        state_ = value;
        return value * 0x2545f4914f6cdd1dULL;
    }

    double unit()
    {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    std::uint64_t state_;
};

double squaredDistance(const double *a, const double *b, std::size_t dimension)
{
    double result = 0.0;
    for (std::size_t value = 0; value < dimension; ++value) {
        const double difference = a[value] - b[value];
        result += difference * difference;
    }
    return result;
}

void factorModel(const MixtureModel &model, std::vector<double> &factors,
                 std::vector<double> &logDeterminants)
{
    const std::size_t d = model.dimension;
    const std::size_t matrix = d * d;
    factors.assign(model.components * matrix, 0.0);
    logDeterminants.assign(model.components, 0.0);
    for (std::size_t component = 0; component < model.components; ++component) {
        double *lower = factors.data() + component * matrix;
        if (!choleskyLower(model.scales.data() + component * matrix, lower, d)) {
            throw std::runtime_error("mixture scale is not positive definite");
        }
        double logdet = 0.0;
        for (std::size_t row = 0; row < d; ++row) {
            logdet += 2.0 * std::log(lower[row * d + row]);
        }
        logDeterminants[component] = logdet;
    }
}

void canonicalize(MixtureModel &model)
{
    std::vector<std::size_t> order(model.components);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (model.weights[a] != model.weights[b]) {
            return model.weights[a] > model.weights[b];
        }
        const double *meanA = model.means.data() + a * model.dimension;
        const double *meanB = model.means.data() + b * model.dimension;
        return std::lexicographical_compare(
            meanA, meanA + model.dimension, meanB, meanB + model.dimension);
    });
    const auto oldWeights = model.weights;
    const auto oldMeans = model.means;
    const auto oldScales = model.scales;
    const auto oldCounts = model.effectiveCounts;
    const std::size_t matrix = model.dimension * model.dimension;
    for (std::size_t output = 0; output < order.size(); ++output) {
        const std::size_t input = order[output];
        model.weights[output] = oldWeights[input];
        std::copy(oldMeans.begin() + input * model.dimension,
                  oldMeans.begin() + (input + 1) * model.dimension,
                  model.means.begin() + output * model.dimension);
        std::copy(oldScales.begin() + input * matrix,
                  oldScales.begin() + (input + 1) * matrix,
                  model.scales.begin() + output * matrix);
        model.effectiveCounts[output] = oldCounts[input];
    }
}

MixtureModel initialize(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    std::size_t dimension,
    const FitConfiguration &configuration)
{
    const std::size_t k = configuration.components;
    MixtureModel model;
    model.dimension = dimension;
    model.components = k;
    model.weights.assign(k, 1.0 / k);
    model.means.resize(k * dimension);
    model.scales.assign(k * dimension * dimension, 0.0);
    model.effectiveCounts.assign(k, 0.0);
    Random random(configuration.seed);
    std::vector<double> distances(sampleCount, std::numeric_limits<double>::infinity());
    std::vector<std::size_t> centers;
    centers.reserve(k);
    centers.push_back(static_cast<std::size_t>(random.next() % sampleCount));
    for (std::size_t component = 1; component < k; ++component) {
        const double *latest = samples.data() + centers.back() * dimension;
        double total = 0.0;
        for (std::size_t sample = 0; sample < sampleCount; ++sample) {
            distances[sample] = std::min(distances[sample], squaredDistance(
                samples.data() + sample * dimension, latest, dimension));
            total += distances[sample];
        }
        if (!(total > 0.0) || !std::isfinite(total)) {
            centers.push_back(component % sampleCount);
            continue;
        }
        const double selected = random.unit() * total;
        double cumulative = 0.0;
        std::size_t index = sampleCount - 1;
        for (std::size_t sample = 0; sample < sampleCount; ++sample) {
            cumulative += distances[sample];
            if (cumulative >= selected) {
                index = sample;
                break;
            }
        }
        centers.push_back(index);
    }
    for (std::size_t component = 0; component < k; ++component) {
        std::copy(samples.begin() + centers[component] * dimension,
                  samples.begin() + (centers[component] + 1) * dimension,
                  model.means.begin() + component * dimension);
    }
    std::vector<std::size_t> assignment(sampleCount);
    std::vector<double> counts(k, 0.0);
    std::fill(model.means.begin(), model.means.end(), 0.0);
    for (std::size_t sample = 0; sample < sampleCount; ++sample) {
        const double *value = samples.data() + sample * dimension;
        std::size_t best = 0;
        double bestDistance = std::numeric_limits<double>::infinity();
        for (std::size_t component = 0; component < k; ++component) {
            const double distance = squaredDistance(
                value, samples.data() + centers[component] * dimension, dimension);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = component;
            }
        }
        assignment[sample] = best;
        counts[best] += 1.0;
        for (std::size_t valueIndex = 0; valueIndex < dimension; ++valueIndex) {
            model.means[best * dimension + valueIndex] += value[valueIndex];
        }
    }
    for (std::size_t component = 0; component < k; ++component) {
        if (counts[component] == 0.0) {
            counts[component] = 1.0;
            std::copy(samples.begin() + centers[component] * dimension,
                      samples.begin() + (centers[component] + 1) * dimension,
                      model.means.begin() + component * dimension);
        } else {
            for (std::size_t value = 0; value < dimension; ++value) {
                model.means[component * dimension + value] /= counts[component];
            }
        }
    }
    const std::size_t matrix = dimension * dimension;
    for (std::size_t sample = 0; sample < sampleCount; ++sample) {
        const std::size_t component = assignment[sample];
        const double *value = samples.data() + sample * dimension;
        const double *mean = model.means.data() + component * dimension;
        double *covariance = model.scales.data() + component * matrix;
        for (std::size_t row = 0; row < dimension; ++row) {
            const double a = value[row] - mean[row];
            for (std::size_t column = 0; column <= row; ++column) {
                covariance[row * dimension + column] += a * (value[column] - mean[column]);
            }
        }
    }
    for (std::size_t component = 0; component < k; ++component) {
        double *covariance = model.scales.data() + component * matrix;
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t column = 0; column <= row; ++column) {
                const double value = covariance[row * dimension + column] / counts[component];
                covariance[row * dimension + column] = value;
                covariance[column * dimension + row] = value;
            }
            covariance[row * dimension + row] += configuration.covarianceFloor;
        }
        model.weights[component] = counts[component] / sampleCount;
        model.effectiveCounts[component] = counts[component];
    }
    canonicalize(model);
    return model;
}

double quadraticLower(
    const double *sample,
    const double *mean,
    const double *lower,
    std::size_t dimension,
    double *solved)
{
    double result = 0.0;
    for (std::size_t row = 0; row < dimension; ++row) {
        double value = sample[row] - mean[row];
        for (std::size_t column = 0; column < row; ++column) {
            value -= lower[row * dimension + column] * solved[column];
        }
        solved[row] = value / lower[row * dimension + row];
        result += solved[row] * solved[row];
    }
    return result;
}

#if defined(TGMR_ENABLE_OMP_TARGET) && defined(_OPENMP)
void targetStatistics(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    const MixtureModel &model,
    const std::vector<double> &factors,
    const std::vector<double> &logdet,
    bool student,
    bool update,
    const FitConfiguration &configuration,
    std::vector<double> &statistics,
    double &likelihood)
{
    // The device image is deliberately frozen to the production fitting
    // dimensions. Keeping those bounds compile-time constant avoids dynamic
    // allocation inside a target thread and makes host fallback unambiguous.
    if (model.components != 32 || model.dimension != 51) {
        throw std::runtime_error("omp-target requires the frozen K32/D51 model");
    }
    const int devices = omp_get_num_devices();
    const int device = configuration.device >= 0
        ? configuration.device : omp_get_default_device();
    if (devices <= 0 || device < 0 || device >= devices) {
        throw std::runtime_error("omp-target has no selected physical device");
    }
    omp_set_default_device(device);
    int initialDevice = 1;
#pragma omp target map(tofrom: initialDevice)
    {
        initialDevice = omp_is_initial_device();
    }
    if (initialDevice != 0) {
        throw std::runtime_error("omp-target fell back to the host");
    }

    constexpr std::size_t K = 32;
    constexpr std::size_t D = 51;
    constexpr std::size_t MATRIX = D * D;
    const std::size_t componentStride = 2 + D + MATRIX;
    const std::size_t statisticsCount = K * componentStride;
    const std::size_t batchCapacity = std::min<std::size_t>(
        sampleCount, std::max<std::size_t>(1, configuration.batchSize));
    std::vector<double> base(K);
    const double nu = configuration.degreesOfFreedom;
    for (std::size_t component = 0; component < K; ++component) {
        base[component] = std::log(model.weights[component]);
        if (student) {
            base[component] += std::lgamma(0.5 * (nu + D))
                - std::lgamma(0.5 * nu)
                - 0.5 * (D * std::log(nu * PI) + logdet[component]);
        } else {
            base[component] += -0.5 * (logdet[component] + D * std::log(2.0 * PI));
        }
    }
    std::vector<double> responsibilities(batchCapacity * K);
    std::vector<double> robust(batchCapacity * K);
    double *samplePointer = const_cast<double *>(samples.data());
    double *meanPointer = const_cast<double *>(model.means.data());
    double *factorPointer = const_cast<double *>(factors.data());
    double *basePointer = base.data();
    double *responsibilityPointer = responsibilities.data();
    double *robustPointer = robust.data();
    double *statisticsPointer = statistics.data();
    const std::size_t sampleValues = sampleCount * D;
    const std::size_t modelValues = K * D;
    const std::size_t factorValues = K * MATRIX;
    const std::size_t batchValues = batchCapacity * K;
    likelihood = 0.0;

#pragma omp target data \
    map(to: samplePointer[0:sampleValues], meanPointer[0:modelValues], \
            factorPointer[0:factorValues], basePointer[0:K]) \
    map(alloc: responsibilityPointer[0:batchValues], robustPointer[0:batchValues]) \
    map(tofrom: statisticsPointer[0:statisticsCount])
    {
        for (std::size_t begin = 0; begin < sampleCount; begin += batchCapacity) {
            const std::size_t count = std::min(batchCapacity, sampleCount - begin);
            double batchLikelihood = 0.0;
#pragma omp target teams distribute parallel for reduction(+:batchLikelihood) \
    map(tofrom: batchLikelihood) firstprivate(begin, count, student, nu)
            for (std::int64_t signedSample = 0;
                 signedSample < static_cast<std::int64_t>(count); ++signedSample) {
                const std::size_t sample = static_cast<std::size_t>(signedSample);
                const double *value = samplePointer + (begin + sample) * D;
                double logs[K];
                double latent[K];
                double maximum = -1.7976931348623157e308;
                for (std::size_t component = 0; component < K; ++component) {
                    const double *lower = factorPointer + component * MATRIX;
                    const double *mean = meanPointer + component * D;
                    double solved[D];
                    double q = 0.0;
                    for (std::size_t row = 0; row < D; ++row) {
                        double current = value[row] - mean[row];
                        for (std::size_t column = 0; column < row; ++column) {
                            current -= lower[row * D + column] * solved[column];
                        }
                        solved[row] = current / lower[row * D + row];
                        q += solved[row] * solved[row];
                    }
                    logs[component] = basePointer[component]
                        - (student ? 0.5 * (nu + D) * log(1.0 + q / nu) : 0.5 * q);
                    latent[component] = student ? (nu + D) / (nu + q) : 1.0;
                    if (logs[component] > maximum) maximum = logs[component];
                }
                double total = 0.0;
                for (std::size_t component = 0; component < K; ++component) {
                    total += exp(logs[component] - maximum);
                }
                const double normalizer = maximum + log(total);
                batchLikelihood += normalizer;
                for (std::size_t component = 0; component < K; ++component) {
                    const double responsibility = exp(logs[component] - normalizer);
                    responsibilityPointer[sample * K + component] = responsibility;
                    robustPointer[sample * K + component] =
                        responsibility * latent[component];
                }
            }
            likelihood += batchLikelihood;

            if (update) {
              // One target thread owns one sufficient-statistic element and
              // reduces it across this batch. This avoids unsupported or
              // nondeterministic floating-point atomics. Upper covariance
              // triangle slots stay zero and are not consumed by the CPU update.
#pragma omp target teams distribute parallel for firstprivate(begin, count)
              for (std::int64_t signedIndex = 0;
                   signedIndex < static_cast<std::int64_t>(statisticsCount); ++signedIndex) {
                const std::size_t index = static_cast<std::size_t>(signedIndex);
                const std::size_t component = index / componentStride;
                const std::size_t field = index % componentStride;
                double sum = 0.0;
                if (field < 2) {
                    for (std::size_t sample = 0; sample < count; ++sample) {
                        sum += field == 0
                            ? responsibilityPointer[sample * K + component]
                            : robustPointer[sample * K + component];
                    }
                } else if (field < 2 + D) {
                    const std::size_t valueIndex = field - 2;
                    for (std::size_t sample = 0; sample < count; ++sample) {
                        sum += robustPointer[sample * K + component]
                            * samplePointer[(begin + sample) * D + valueIndex];
                    }
                } else {
                    const std::size_t element = field - 2 - D;
                    const std::size_t row = element / D;
                    const std::size_t column = element % D;
                    if (column <= row) {
                        for (std::size_t sample = 0; sample < count; ++sample) {
                            const double weight = robustPointer[sample * K + component];
                            const double *value = samplePointer + (begin + sample) * D;
                            sum += weight * value[row] * value[column];
                        }
                    }
                }
                  statisticsPointer[index] += sum;
              }
            }
            if (configuration.gpuYieldMilliseconds != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    configuration.gpuYieldMilliseconds));
            }
        }
    }
}
#endif

// Perform one complete Gaussian-EM or Student-t ECM iteration without storing
// the N x K responsibility matrix.  Each worker owns fixed sufficient
// statistics, batches only bound scheduling latency, and reduction is always
// performed in ascending worker/component order.  The canonical backend uses
// exactly one worker and therefore has a fixed floating-point addition order.
void expectationMaximization(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    MixtureModel &model,
    bool student,
    const FitConfiguration &configuration,
    bool update)
{
    const std::size_t d = model.dimension;
    const std::size_t k = model.components;
    const std::size_t matrix = d * d;
    std::vector<double> factors;
    std::vector<double> logdet;
    factorModel(model, factors, logdet);
    const bool parallel = configuration.backend == TrainingBackend::CPU;
    unsigned workers = 1;
#ifdef _OPENMP
    if (parallel) workers = static_cast<unsigned>(omp_get_max_threads());
#endif
    const std::size_t componentStride = 2 + d + matrix;
    if (workers > configuration.maximumBytes / std::max<std::size_t>(
            1, k * componentStride * sizeof(double))) {
        throw std::runtime_error("TGMR per-worker sufficient statistics exceed memory limit");
    }
    std::vector<double> statistics(
        static_cast<std::size_t>(workers) * k * componentStride, 0.0);
    std::vector<double> likelihoods(workers, 0.0);
    const bool target = configuration.backend == TrainingBackend::OMP_TARGET;
#if defined(TGMR_ENABLE_OMP_TARGET) && defined(_OPENMP)
    if (target) {
        targetStatistics(samples, sampleCount, model, factors, logdet, student, update,
                         configuration, statistics, likelihoods[0]);
    } else
#else
    if (target) {
        throw std::runtime_error(
            "omp-target backend was not compiled into rt-tgmr-train");
    } else
#endif
    {
      const std::size_t batchSize = std::min<std::size_t>(
          sampleCount, std::max<std::size_t>(1, configuration.batchSize));
      for (std::size_t begin = 0; begin < sampleCount; begin += batchSize) {
        const std::size_t end = std::min(sampleCount, begin + batchSize);
#ifdef _OPENMP
#pragma omp parallel for schedule(static) if(parallel)
#endif
        for (std::int64_t signedSample = static_cast<std::int64_t>(begin);
             signedSample < static_cast<std::int64_t>(end); ++signedSample) {
            const std::size_t sample = static_cast<std::size_t>(signedSample);
            unsigned worker = 0;
#ifdef _OPENMP
            if (parallel) worker = static_cast<unsigned>(omp_get_thread_num());
#endif
            const double *value = samples.data() + sample * d;
            std::array<double, 256> logs{};
            std::array<double, 256> latent{};
            std::array<double, 256> solved{};
            double maximum = -std::numeric_limits<double>::infinity();
            for (std::size_t component = 0; component < k; ++component) {
                const double q = quadraticLower(
                    value, model.means.data() + component * d,
                    factors.data() + component * matrix, d, solved.data());
                const double density = student
                    ? std::lgamma(0.5 * (configuration.degreesOfFreedom + d))
                        - std::lgamma(0.5 * configuration.degreesOfFreedom)
                        - 0.5 * (d * std::log(configuration.degreesOfFreedom * PI)
                                 + logdet[component])
                        - 0.5 * (configuration.degreesOfFreedom + d)
                            * std::log1p(q / configuration.degreesOfFreedom)
                    : -0.5 * (q + logdet[component] + d * std::log(2.0 * PI));
                logs[component] = std::log(model.weights[component]) + density;
                latent[component] = student
                    ? (configuration.degreesOfFreedom + d)
                        / (configuration.degreesOfFreedom + q)
                    : 1.0;
                maximum = std::max(maximum, logs[component]);
            }
            double total = 0.0;
            for (std::size_t component = 0; component < k; ++component) {
                total += std::exp(logs[component] - maximum);
            }
            const double normalizer = maximum + std::log(total);
            likelihoods[worker] += normalizer;
            if (!update) continue;
            for (std::size_t component = 0; component < k; ++component) {
                const double responsibility = std::exp(logs[component] - normalizer);
                const double robust = responsibility * latent[component];
                double *componentStatistics = statistics.data()
                    + (static_cast<std::size_t>(worker) * k + component) * componentStride;
                componentStatistics[0] += responsibility;
                componentStatistics[1] += robust;
                double *sum = componentStatistics + 2;
                double *outer = sum + d;
                for (std::size_t row = 0; row < d; ++row) {
                    sum[row] += robust * value[row];
                    for (std::size_t column = 0; column <= row; ++column) {
                        outer[row * d + column] += robust * value[row] * value[column];
                    }
                }
            }
        }
      }
    }
    model.meanLogLikelihood = std::accumulate(
        likelihoods.begin(), likelihoods.end(), 0.0) / sampleCount;
    if (!update) return;
    std::vector<double> reduced(k * componentStride, 0.0);
    for (unsigned worker = 0; worker < workers; ++worker) {
        const double *input = statistics.data()
            + static_cast<std::size_t>(worker) * k * componentStride;
        for (std::size_t index = 0; index < k * componentStride; ++index) {
            reduced[index] += input[index];
        }
    }
    std::vector<double> newMeans(k * d, 0.0);
    std::vector<double> newScales(k * matrix, 0.0);
    std::vector<double> effective(k, 0.0);
    for (std::size_t component = 0; component < k; ++component) {
        const double *componentStatistics = reduced.data() + component * componentStride;
        const double count = componentStatistics[0];
        const double robustTotal = componentStatistics[1];
        if (!(count > 1e-8) || !(robustTotal > 1e-8)) {
            throw std::runtime_error("mixture component became empty");
        }
        const double *sum = componentStatistics + 2;
        const double *outer = sum + d;
        double *mean = newMeans.data() + component * d;
        double *scale = newScales.data() + component * matrix;
        for (std::size_t row = 0; row < d; ++row) mean[row] = sum[row] / robustTotal;
        for (std::size_t row = 0; row < d; ++row) {
            for (std::size_t column = 0; column <= row; ++column) {
                double value = (outer[row * d + column]
                    - robustTotal * mean[row] * mean[column]) / count;
                if (row == column) value = std::max(value, 0.0) + configuration.covarianceFloor;
                scale[row * d + column] = value;
                scale[column * d + row] = value;
            }
        }
        effective[component] = count;
    }
    model.means.swap(newMeans);
    model.scales.swap(newScales);
    model.effectiveCounts = effective;
    for (std::size_t component = 0; component < k; ++component) {
        model.weights[component] = effective[component] / sampleCount;
    }
    canonicalize(model);
}

} // namespace

bool choleskyLower(const double *matrix, double *lower, std::size_t dimension)
{
    std::fill(lower, lower + dimension * dimension, 0.0);
    for (std::size_t row = 0; row < dimension; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double value = matrix[row * dimension + column];
            for (std::size_t inner = 0; inner < column; ++inner) {
                value -= lower[row * dimension + inner] * lower[column * dimension + inner];
            }
            if (row == column) {
                if (!(value > 0.0) || !std::isfinite(value)) {
                    return false;
                }
                lower[row * dimension + column] = std::sqrt(value);
            } else {
                lower[row * dimension + column] = value / lower[column * dimension + column];
            }
        }
    }
    return true;
}

double studentTLogDensity(
    const double *sample,
    const double *mean,
    const double *lower,
    std::size_t dimension,
    double degreesOfFreedom,
    double *quadratic)
{
    if (!(degreesOfFreedom > 2.0) || !std::isfinite(degreesOfFreedom)) {
        throw std::runtime_error("Student-t degrees of freedom must exceed two");
    }
    std::vector<double> solved(dimension);
    double q = 0.0;
    double logdet = 0.0;
    for (std::size_t row = 0; row < dimension; ++row) {
        double value = sample[row] - mean[row];
        for (std::size_t column = 0; column < row; ++column) {
            value -= lower[row * dimension + column] * solved[column];
        }
        solved[row] = value / lower[row * dimension + row];
        q += solved[row] * solved[row];
        logdet += 2.0 * std::log(lower[row * dimension + row]);
    }
    if (quadratic) {
        *quadratic = q;
    }
    return std::lgamma(0.5 * (degreesOfFreedom + dimension))
        - std::lgamma(0.5 * degreesOfFreedom)
        - 0.5 * (dimension * std::log(degreesOfFreedom * PI) + logdet)
        - 0.5 * (degreesOfFreedom + dimension) * std::log1p(q / degreesOfFreedom);
}

MixtureModel fitStudentTMixture(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    std::size_t dimension,
    const FitConfiguration &configuration,
    const IterationCallback &callback)
{
    const std::size_t sampleValues = checkedMultiply(sampleCount, dimension, "sample matrix");
    if (sampleCount < configuration.components || dimension == 0
        || samples.size() != sampleValues
        || configuration.components == 0
        || configuration.covarianceFloor <= 0.0
        || configuration.degreesOfFreedom <= 2.0
        || configuration.device < -1
        || configuration.batchSize == 0) {
        throw std::runtime_error("invalid or unavailable TGMR fitting configuration");
    }
    if (dimension > 256 || configuration.components > 256) {
        throw std::runtime_error("TGMR fitting dimension or component count exceeds 256");
    }
    std::size_t workers = 1;
#ifdef _OPENMP
    if (configuration.backend == TrainingBackend::CPU) {
        workers = static_cast<std::size_t>(omp_get_max_threads());
    }
#endif
    const std::size_t matrix = checkedMultiply(dimension, dimension, "covariance dimension");
    const std::size_t componentStride = checkedAdd(
        checkedAdd(2, dimension, "component statistics"), matrix,
        "component statistics");
    const std::size_t statisticsBytes = checkedMultiply(checkedMultiply(
        checkedMultiply(workers, configuration.components, "worker components"),
        componentStride, "sufficient statistics"), sizeof(double), "statistics bytes");
    const std::size_t modelBytes = checkedMultiply(checkedMultiply(
        configuration.components,
        checkedAdd(checkedAdd(matrix, dimension, "model values"), 2, "model values"),
        "model values"), sizeof(double), "model bytes");
    const std::size_t sampleBytes = checkedMultiply(samples.size(), sizeof(double), "sample bytes");
    const std::size_t targetBytes = configuration.backend == TrainingBackend::OMP_TARGET
        ? checkedMultiply(checkedMultiply(checkedMultiply(
            std::min(sampleCount, configuration.batchSize), configuration.components,
            "target batch components"), 2, "target posterior buffers"),
            sizeof(double), "target posterior bytes")
        : 0;
    const std::size_t totalBytes = checkedAdd(checkedAdd(
        checkedAdd(sampleBytes, statisticsBytes, "samples and statistics"), modelBytes,
        "samples, statistics, and model"), targetBytes, "target working set");
    if (sampleBytes > configuration.maximumBytes
        || statisticsBytes > configuration.maximumBytes
        || targetBytes > configuration.maximumBytes
        || totalBytes > configuration.maximumBytes) {
        throw std::runtime_error("TGMR fitting configuration exceeds the memory limit");
    }
    for (double value : samples) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("TGMR fitting samples contain a non-finite value");
        }
    }
    MixtureModel model = initialize(samples, sampleCount, dimension, configuration);
    for (std::uint32_t iteration = 1; iteration <= configuration.gaussianIterations; ++iteration) {
        expectationMaximization(samples, sampleCount, model, false, configuration, true);
        model.gaussianIterations = iteration;
        if (callback) {
            callback(model, "gaussian-em", iteration);
        }
    }
    for (std::uint32_t iteration = 1; iteration <= configuration.studentIterations; ++iteration) {
        expectationMaximization(samples, sampleCount, model, true, configuration, true);
        model.studentIterations = iteration;
        if (callback) {
            callback(model, "student-t-ecm", iteration);
        }
    }
    expectationMaximization(samples, sampleCount, model, true, configuration, false);
    return model;
}

MixtureModel continueStudentTMixture(
    const std::vector<double> &samples,
    std::size_t sampleCount,
    const FitConfiguration &configuration,
    MixtureModel model,
    std::uint32_t additionalGaussianIterations,
    std::uint32_t additionalStudentIterations,
    const IterationCallback &callback)
{
    const std::size_t dimension = model.dimension;
    if (dimension == 0 || model.components != configuration.components
        || samples.size() != sampleCount * dimension
        || model.weights.size() != model.components
        || model.means.size() != model.components * dimension
        || model.scales.size() != model.components * dimension * dimension
        || model.effectiveCounts.size() != model.components) {
        throw std::runtime_error("checkpoint and continuation configuration differ");
    }
    for (std::uint32_t offset = 1; offset <= additionalGaussianIterations; ++offset) {
        expectationMaximization(samples, sampleCount, model, false, configuration, true);
        ++model.gaussianIterations;
        if (callback) {
            callback(model, "gaussian-em", model.gaussianIterations);
        }
    }
    for (std::uint32_t offset = 1; offset <= additionalStudentIterations; ++offset) {
        expectationMaximization(samples, sampleCount, model, true, configuration, true);
        ++model.studentIterations;
        if (callback) {
            callback(model, "student-t-ecm", model.studentIterations);
        }
    }
    expectationMaximization(samples, sampleCount, model, true, configuration, false);
    return model;
}

} // namespace tgmr
