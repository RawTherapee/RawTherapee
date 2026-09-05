#include "tgmr/xtrans_training.h"

#include "tgmr/corpus.h"
#include "tgmr/sha256.h"
#include "tgmr/training_identity.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace tgmr
{
namespace
{

constexpr int CFA[6][6] = {
    {1, 0, 1, 1, 2, 1},
    {2, 1, 2, 0, 1, 0},
    {1, 0, 1, 1, 2, 1},
    {1, 2, 1, 1, 0, 1},
    {0, 1, 0, 2, 1, 2},
    {1, 2, 1, 1, 0, 1},
};
constexpr std::array<std::uint8_t, 8> CHECKPOINT_MAGIC{{'R','T','T','G','M','C','1',0}};
constexpr std::size_t CHECKPOINT_HEADER = 256;
constexpr std::size_t CHECKPOINT_AUTH_OFFSET = 160;
constexpr std::size_t CHECKPOINT_AUTH_BYTES = 32;

void put32(std::uint8_t *output, std::uint32_t value)
{
    for (unsigned index = 0; index < 4; ++index) {
        output[index] = static_cast<std::uint8_t>(value >> (8 * index));
    }
}

void put64(std::uint8_t *output, std::uint64_t value)
{
    for (unsigned index = 0; index < 8; ++index) {
        output[index] = static_cast<std::uint8_t>(value >> (8 * index));
    }
}

std::uint32_t get32(const std::uint8_t *input)
{
    return static_cast<std::uint32_t>(input[0])
        | (static_cast<std::uint32_t>(input[1]) << 8)
        | (static_cast<std::uint32_t>(input[2]) << 16)
        | (static_cast<std::uint32_t>(input[3]) << 24);
}

std::uint64_t get64(const std::uint8_t *input)
{
    std::uint64_t result = 0;
    for (unsigned index = 0; index < 8; ++index) {
        result |= static_cast<std::uint64_t>(input[index]) << (8 * index);
    }
    return result;
}

void putDouble(std::uint8_t *output, double value)
{
    static_assert(sizeof(double) == 8, "TGMR training requires binary64");
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    put64(output, bits);
}

double getDouble(const std::uint8_t *input)
{
    const std::uint64_t bits = get64(input);
    double result = 0.0;
    std::memcpy(&result, &bits, sizeof(result));
    if (!std::isfinite(result)) {
        throw std::runtime_error("checkpoint contains a non-finite binary64 value");
    }
    return result;
}

float getFloat(const std::uint8_t *input)
{
    const std::uint32_t bits = get32(input);
    float result = 0.f;
    std::memcpy(&result, &bits, sizeof(result));
    if (!std::isfinite(result)) {
        throw std::runtime_error("TGMR phase payload contains a non-finite float32 value");
    }
    return result;
}

void appendU32(std::vector<std::uint8_t> &output, std::uint32_t value)
{
    const std::size_t offset = output.size();
    output.resize(offset + 4);
    put32(output.data() + offset, value);
}

void appendF32(std::vector<std::uint8_t> &output, double value)
{
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
        throw std::runtime_error("model coefficient cannot be represented as float32");
    }
    const float converted = static_cast<float>(value);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &converted, sizeof(bits));
    appendU32(output, bits);
}

void appendDoubles(
    std::vector<std::uint8_t> &output,
    const std::vector<double> &values)
{
    for (double value : values) {
        const std::size_t offset = output.size();
        output.resize(offset + 8);
        putDouble(output.data() + offset, value);
    }
}

std::vector<std::uint8_t> readFile(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot open phase checkpoint: " + path);
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (size < static_cast<std::streamoff>(CHECKPOINT_HEADER)
        || size > static_cast<std::streamoff>(64 * 1024 * 1024)) {
        throw std::runtime_error("phase checkpoint size is invalid");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char *>(result.data()), size);
    if (!stream) {
        throw std::runtime_error("short phase checkpoint read");
    }
    return result;
}

std::array<std::uint8_t, 32> authenticate(std::vector<std::uint8_t> bytes)
{
    if (bytes.size() < CHECKPOINT_AUTH_OFFSET + CHECKPOINT_AUTH_BYTES) {
        throw std::runtime_error("phase checkpoint is truncated");
    }
    std::fill(bytes.begin() + CHECKPOINT_AUTH_OFFSET,
              bytes.begin() + CHECKPOINT_AUTH_OFFSET + CHECKPOINT_AUTH_BYTES, 0);
    return sha256(bytes.data(), bytes.size());
}

bool allZero(const std::uint8_t *data, std::size_t count)
{
    return std::all_of(data, data + count, [](std::uint8_t value) { return value == 0; });
}

std::vector<double> solveCholesky(
    const std::vector<double> &lower,
    const std::vector<double> &right,
    std::size_t dimension)
{
    std::vector<double> work(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
        double value = right[row];
        for (std::size_t column = 0; column < row; ++column) {
            value -= lower[row * dimension + column] * work[column];
        }
        work[row] = value / lower[row * dimension + row];
    }
    std::vector<double> output(dimension);
    for (std::size_t reverse = 0; reverse < dimension; ++reverse) {
        const std::size_t row = dimension - reverse - 1;
        double value = work[row];
        for (std::size_t column = row + 1; column < dimension; ++column) {
            value -= lower[column * dimension + row] * output[column];
        }
        output[row] = value / lower[row * dimension + row];
    }
    return output;
}

} // namespace

std::array<PhaseContract, 18> xtransPhaseContracts()
{
    std::array<PhaseContract, 18> output{};
    std::set<std::array<int, 36>> seen;
    std::size_t phase = 0;
    for (unsigned originY = 0; originY < 6; ++originY) {
        for (unsigned originX = 0; originX < 6; ++originX) {
            std::array<int, 36> pattern{};
            for (unsigned y = 0; y < 6; ++y) {
                for (unsigned x = 0; x < 6; ++x) {
                    pattern[y * 6 + x] = CFA[(y + originY) % 6][(x + originX) % 6];
                }
            }
            if (!seen.insert(pattern).second) {
                continue;
            }
            if (phase >= output.size()) {
                throw std::runtime_error("more than 18 unique X-Trans phases");
            }
            PhaseContract &contract = output[phase];
            contract.index = phase;
            contract.originX = originX;
            contract.originY = originY;
            for (unsigned y = 0; y < 7; ++y) {
                for (unsigned x = 0; x < 7; ++x) {
                    const unsigned spatial = y * 7 + x;
                    const unsigned channel = CFA[(y + originY) % 6][(x + originX) % 6];
                    contract.observedIndices[spatial] = channel * 49 + spatial;
                }
            }
            contract.sampledCenterChannel = contract.observedIndices[24] / 49;
            unsigned target = 0;
            for (unsigned channel = 0; channel < 3; ++channel) {
                if (channel != contract.sampledCenterChannel) {
                    contract.targetChannels[target++] = channel;
                }
            }
            ++phase;
        }
    }
    if (phase != output.size()) {
        throw std::runtime_error("X-Trans phase derivation did not produce 18 phases");
    }
    return output;
}

std::vector<double> loadPhaseTrainingMatrix(
    const std::string &corpusPath,
    const PhaseContract &phase,
    std::uint64_t &sampleCount,
    std::array<std::uint8_t, 32> &corpusPayloadSha256,
    std::uint64_t sourceLimit)
{
    std::vector<double> output;
    std::set<std::array<std::uint8_t, 32>> selectedSources;
    const CorpusInspection inspection = inspectCorpus(corpusPath,
        [&](const PatchRecord &record, std::uint64_t) {
            if (record.split != CorpusSplit::TRAIN) {
                return;
            }
            if (sourceLimit != 0
                && selectedSources.find(record.sourceIdSha256) == selectedSources.end()) {
                if (selectedSources.size() >= sourceLimit) {
                    return;
                }
                selectedSources.insert(record.sourceIdSha256);
            }
            std::array<double, 3> sums{{0.0, 0.0, 0.0}};
            std::array<unsigned, 3> counts{{0, 0, 0}};
            for (std::size_t position = 0; position < 49; ++position) {
                const std::uint32_t index = phase.observedIndices[position];
                const unsigned channel = index / 49;
                sums[channel] += record.rgb[index] / 65535.0;
                ++counts[channel];
            }
            double dc = 0.0;
            for (unsigned channel = 0; channel < 3; ++channel) {
                dc += sums[channel] / counts[channel];
            }
            dc /= 3.0;
            for (std::uint32_t index : phase.observedIndices) {
                output.push_back(record.rgb[index] / 65535.0 - dc);
            }
            for (std::uint32_t channel : phase.targetChannels) {
                output.push_back(record.rgb[channel * 49 + 24] / 65535.0 - dc);
            }
        });
    corpusPayloadSha256 = inspection.header.payloadSha256;
    if (sourceLimit != 0 && selectedSources.size() != sourceLimit) {
        throw std::runtime_error("TGPC contains fewer training sources than --source-limit");
    }
    sampleCount = output.size() / 51;
    if (output.size() != sampleCount * 51) {
        throw std::runtime_error("TGPC training split did not produce an N x 51 phase matrix");
    }
    return output;
}

void writePhaseCheckpoint(
    const std::string &path,
    const PhaseCheckpoint &checkpoint,
    bool force)
{
    const MixtureModel &model = checkpoint.model;
    if (model.dimension != 51 || model.components == 0
        || model.weights.size() != model.components
        || model.means.size() != model.components * 51
        || model.scales.size() != model.components * 51 * 51
        || model.effectiveCounts.size() != model.components) {
        throw std::runtime_error("invalid phase checkpoint model");
    }
    std::ifstream existing(path, std::ios::binary);
    if (existing.good() && !force) {
        throw std::runtime_error("refusing to replace phase checkpoint: " + path);
    }
    std::vector<std::uint8_t> payload;
    appendDoubles(payload, model.weights);
    appendDoubles(payload, model.means);
    appendDoubles(payload, model.scales);
    appendDoubles(payload, model.effectiveCounts);
    std::vector<std::uint8_t> bytes(CHECKPOINT_HEADER + payload.size(), 0);
    std::copy(CHECKPOINT_MAGIC.begin(), CHECKPOINT_MAGIC.end(), bytes.begin());
    put32(bytes.data() + 8, 2);
    put32(bytes.data() + 12, CHECKPOINT_HEADER);
    put32(bytes.data() + 16, 0x01020304);
    put32(bytes.data() + 20, 0);
    put32(bytes.data() + 24, checkpoint.phase.index);
    put32(bytes.data() + 28, checkpoint.phase.originX);
    put32(bytes.data() + 32, checkpoint.phase.originY);
    put32(bytes.data() + 36, model.components);
    put32(bytes.data() + 40, model.dimension);
    put32(bytes.data() + 44, model.gaussianIterations);
    put32(bytes.data() + 48, model.studentIterations);
    put32(bytes.data() + 52, static_cast<std::uint32_t>(checkpoint.configuration.backend));
    put64(bytes.data() + 56, checkpoint.sampleCount);
    put64(bytes.data() + 64, CHECKPOINT_HEADER);
    put64(bytes.data() + 72, payload.size());
    put64(bytes.data() + 80, bytes.size());
    put64(bytes.data() + 88, checkpoint.configuration.seed);
    putDouble(bytes.data() + 96, checkpoint.configuration.covarianceFloor);
    putDouble(bytes.data() + 104, checkpoint.configuration.degreesOfFreedom);
    putDouble(bytes.data() + 112, model.meanLogLikelihood);
    std::copy(checkpoint.corpusPayloadSha256.begin(), checkpoint.corpusPayloadSha256.end(),
              bytes.begin() + 120);
    if (checkpoint.configuration.device < -1) {
        throw std::runtime_error("phase checkpoint device is outside the supported range");
    }
    put32(bytes.data() + 224, checkpoint.configuration.device < 0 ? 0U
        : static_cast<std::uint32_t>(checkpoint.configuration.device) + 1U);
    put32(bytes.data() + 228, checkpoint.configuration.gpuYieldMilliseconds);
    put64(bytes.data() + 232, checkpoint.configuration.batchSize);
    put64(bytes.data() + 240, checkpoint.configuration.maximumBytes);
    put64(bytes.data() + 248, checkpoint.configuration.sourceLimit);
    const auto payloadDigest = sha256(payload.data(), payload.size());
    std::copy(payloadDigest.begin(), payloadDigest.end(), bytes.begin() + 192);
    std::copy(payload.begin(), payload.end(), bytes.begin() + CHECKPOINT_HEADER);
    const auto authentication = authenticate(bytes);
    std::copy(authentication.begin(), authentication.end(),
              bytes.begin() + CHECKPOINT_AUTH_OFFSET);
    const std::string temporary = path + ".tmp";
    std::FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
        throw std::runtime_error("cannot create phase checkpoint temporary file");
    }
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (std::fflush(file) != 0) ok = false;
    if (std::fclose(file) != 0) ok = false;
    if (!ok) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot publish complete phase checkpoint");
    }
    if (force) std::remove(path.c_str());
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot publish complete phase checkpoint");
    }
}

PhaseCheckpoint readPhaseCheckpoint(const std::string &path)
{
    const std::vector<std::uint8_t> bytes = readFile(path);
    const std::uint32_t checkpointVersion = bytes.size() >= 12
        ? get32(bytes.data() + 8) : 0;
    if (!std::equal(CHECKPOINT_MAGIC.begin(), CHECKPOINT_MAGIC.end(), bytes.begin())
        || (checkpointVersion != 1 && checkpointVersion != 2)
        || get32(bytes.data() + 12) != CHECKPOINT_HEADER
        || get32(bytes.data() + 16) != 0x01020304 || get32(bytes.data() + 20) != 0
        || get32(bytes.data() + 40) != 51 || get64(bytes.data() + 64) != CHECKPOINT_HEADER
        || get64(bytes.data() + 80) != bytes.size()
        || (checkpointVersion == 1 && !allZero(bytes.data() + 248, 8))) {
        throw std::runtime_error("unsupported phase checkpoint contract");
    }
    const std::uint32_t components = get32(bytes.data() + 36);
    const std::uint64_t expectedDoubles = components * (1ULL + 51 + 51 * 51 + 1);
    const std::uint64_t payloadBytes = get64(bytes.data() + 72);
    if (components == 0 || components > 256 || payloadBytes != expectedDoubles * 8
        || bytes.size() != CHECKPOINT_HEADER + payloadBytes
        || !allZero(bytes.data() + 152, 8)) {
        throw std::runtime_error("phase checkpoint sizes or reserved fields are invalid");
    }
    std::array<std::uint8_t, 32> storedAuthentication{};
    std::copy(bytes.begin() + CHECKPOINT_AUTH_OFFSET,
              bytes.begin() + CHECKPOINT_AUTH_OFFSET + 32, storedAuthentication.begin());
    if (authenticate(bytes) != storedAuthentication) {
        throw std::runtime_error("phase checkpoint authentication mismatch");
    }
    const auto actualPayload = sha256(bytes.data() + CHECKPOINT_HEADER,
                                      static_cast<std::size_t>(payloadBytes));
    std::array<std::uint8_t, 32> storedPayload{};
    std::copy(bytes.begin() + 192, bytes.begin() + 224, storedPayload.begin());
    if (actualPayload != storedPayload) {
        throw std::runtime_error("phase checkpoint payload digest mismatch");
    }
    PhaseCheckpoint output;
    const auto contracts = xtransPhaseContracts();
    const std::uint32_t phase = get32(bytes.data() + 24);
    if (phase >= contracts.size()) {
        throw std::runtime_error("phase checkpoint phase index is invalid");
    }
    output.phase = contracts[phase];
    if (output.phase.originX != get32(bytes.data() + 28)
        || output.phase.originY != get32(bytes.data() + 32)) {
        throw std::runtime_error("phase checkpoint origin is inconsistent");
    }
    output.sampleCount = get64(bytes.data() + 56);
    output.configuration.components = components;
    output.configuration.seed = get64(bytes.data() + 88);
    output.configuration.covarianceFloor = getDouble(bytes.data() + 96);
    output.configuration.degreesOfFreedom = getDouble(bytes.data() + 104);
    const std::uint32_t backend = get32(bytes.data() + 52);
    if (backend > static_cast<std::uint32_t>(TrainingBackend::OMP_TARGET)) {
        throw std::runtime_error("phase checkpoint backend is invalid");
    }
    output.configuration.backend = static_cast<TrainingBackend>(backend);
    const std::uint32_t encodedDevice = get32(bytes.data() + 224);
    output.configuration.device = encodedDevice == 0
        ? -1 : static_cast<int>(encodedDevice - 1);
    output.configuration.gpuYieldMilliseconds = get32(bytes.data() + 228);
    if (get64(bytes.data() + 232) != 0) {
        output.configuration.batchSize = static_cast<std::size_t>(get64(bytes.data() + 232));
    }
    if (get64(bytes.data() + 240) != 0) {
        output.configuration.maximumBytes = static_cast<std::size_t>(get64(bytes.data() + 240));
    }
    output.configuration.sourceLimit = checkpointVersion == 2
        ? get64(bytes.data() + 248) : 0;
    std::copy(bytes.begin() + 120, bytes.begin() + 152,
              output.corpusPayloadSha256.begin());
    output.model.dimension = 51;
    output.model.components = components;
    output.model.gaussianIterations = get32(bytes.data() + 44);
    output.model.studentIterations = get32(bytes.data() + 48);
    output.model.meanLogLikelihood = getDouble(bytes.data() + 112);
    const std::uint8_t *cursor = bytes.data() + CHECKPOINT_HEADER;
    auto load = [&](std::vector<double> &destination, std::size_t count) {
        destination.resize(count);
        for (double &value : destination) {
            value = getDouble(cursor);
            cursor += 8;
        }
    };
    load(output.model.weights, components);
    load(output.model.means, components * 51);
    load(output.model.scales, components * 51 * 51);
    load(output.model.effectiveCounts, components);
    return output;
}

std::vector<std::uint8_t> exportPhasePayload(
    const std::array<PhaseCheckpoint, 18> &checkpoints,
    double tau)
{
    if (!(tau > 0.0) || !std::isfinite(tau)) {
        throw std::runtime_error("TGMR export tau must be finite and positive");
    }
    std::vector<std::uint8_t> output;
    const std::array<std::uint32_t, 9> coarse{{16,17,18,23,24,25,30,31,32}};
    const PhaseCheckpoint &reference = checkpoints.front();
    const auto referenceConfiguration =
        fitConfigurationSha256(reference.configuration);
    for (std::size_t phaseIndex = 0; phaseIndex < checkpoints.size(); ++phaseIndex) {
        const PhaseCheckpoint &checkpoint = checkpoints[phaseIndex];
        const MixtureModel &model = checkpoint.model;
        if (checkpoint.phase.index != phaseIndex || model.dimension != 51
            || model.components != 32 || checkpoint.configuration.degreesOfFreedom != 3.0) {
            throw std::runtime_error("checkpoint does not match frozen K32 TGMR export");
        }
        if (checkpoint.sampleCount != reference.sampleCount
            || checkpoint.corpusPayloadSha256 != reference.corpusPayloadSha256
            || fitConfigurationSha256(checkpoint.configuration)
                != referenceConfiguration
            || checkpoint.configuration.components != reference.configuration.components
            || checkpoint.configuration.covarianceFloor
                != reference.configuration.covarianceFloor
            || checkpoint.configuration.degreesOfFreedom
                != reference.configuration.degreesOfFreedom
            || checkpoint.configuration.seed != reference.configuration.seed
            || checkpoint.configuration.sourceLimit
                != reference.configuration.sourceLimit
            || checkpoint.configuration.backend != reference.configuration.backend
            || model.gaussianIterations != reference.model.gaussianIterations
            || model.studentIterations != reference.model.studentIterations) {
            throw std::runtime_error(
                "phase checkpoints use inconsistent training data or settings");
        }
        if (model.weights.size() != 32 || model.means.size() != 32 * 51
            || model.scales.size() != 32 * 51 * 51
            || model.effectiveCounts.size() != 32) {
            throw std::runtime_error("checkpoint model arrays have unexpected sizes");
        }
        for (double weight : model.weights) {
            if (!(weight > 0.0) || !std::isfinite(weight)) {
                throw std::runtime_error("checkpoint has invalid component weights");
            }
        }
        for (double value : model.means) {
            if (!std::isfinite(value)) {
                throw std::runtime_error("checkpoint has non-finite component means");
            }
        }
        for (double value : model.scales) {
            if (!std::isfinite(value)) {
                throw std::runtime_error("checkpoint has non-finite component covariance");
            }
        }
        for (double value : model.effectiveCounts) {
            if (!(value >= 0.0) || !std::isfinite(value)) {
                throw std::runtime_error("checkpoint has invalid component populations");
            }
        }
        for (std::uint32_t value : checkpoint.phase.observedIndices) appendU32(output, value);
        appendU32(output, checkpoint.phase.sampledCenterChannel);
        appendU32(output, checkpoint.phase.targetChannels[0]);
        appendU32(output, checkpoint.phase.targetChannels[1]);
        for (double weight : model.weights) appendF32(output, std::log(weight));
        for (std::size_t component = 0; component < 32; ++component) {
            for (std::size_t value = 0; value < 49; ++value) {
                appendF32(output, model.means[component * 51 + value]);
            }
        }
        for (std::size_t component = 0; component < 32; ++component) {
            appendF32(output, model.means[component * 51 + 49]);
            appendF32(output, model.means[component * 51 + 50]);
        }
        std::vector<std::vector<double>> fullFactors;
        std::vector<double> fullLogdet;
        std::vector<std::vector<double>> gains;
        for (std::size_t component = 0; component < 32; ++component) {
            const double *scale = model.scales.data() + component * 51 * 51;
            std::vector<double> covariance(49 * 49);
            for (std::size_t row = 0; row < 49; ++row) {
                for (std::size_t column = 0; column < 49; ++column) {
                    covariance[row * 49 + column] = scale[row * 51 + column];
                }
                covariance[row * 49 + row] += tau * tau;
            }
            std::vector<double> lower(49 * 49);
            if (!choleskyLower(covariance.data(), lower.data(), 49)) {
                throw std::runtime_error("TGMR full observed covariance is not positive definite");
            }
            double logdet = 0.0;
            for (std::size_t row = 0; row < 49; ++row) {
                logdet += 2.0 * std::log(lower[row * 49 + row]);
            }
            fullFactors.push_back(lower);
            fullLogdet.push_back(logdet);
            for (std::size_t target = 0; target < 2; ++target) {
                std::vector<double> right(49);
                for (std::size_t row = 0; row < 49; ++row) {
                    right[row] = scale[row * 51 + 49 + target];
                }
                gains.push_back(solveCholesky(lower, right, 49));
            }
        }
        for (const auto &factor : fullFactors) {
            for (double value : factor) appendF32(output, value);
        }
        for (double value : fullLogdet) appendF32(output, value);
        for (const auto &gain : gains) {
            for (double value : gain) appendF32(output, value);
        }
        for (std::uint32_t value : coarse) appendU32(output, value);
        for (std::size_t component = 0; component < 32; ++component) {
            const double *scale = model.scales.data() + component * 51 * 51;
            std::vector<double> covariance(9 * 9);
            for (std::size_t row = 0; row < 9; ++row) {
                for (std::size_t column = 0; column < 9; ++column) {
                    covariance[row * 9 + column] = scale[coarse[row] * 51 + coarse[column]];
                }
                covariance[row * 9 + row] += tau * tau;
            }
            std::vector<double> lower(9 * 9);
            if (!choleskyLower(covariance.data(), lower.data(), 9)) {
                throw std::runtime_error("TGMR coarse covariance is not positive definite");
            }
            for (double value : lower) appendF32(output, value);
        }
        for (std::size_t component = 0; component < 32; ++component) {
            const double *scale = model.scales.data() + component * 51 * 51;
            std::vector<double> covariance(9 * 9);
            for (std::size_t row = 0; row < 9; ++row) {
                for (std::size_t column = 0; column < 9; ++column) {
                    covariance[row * 9 + column] = scale[coarse[row] * 51 + coarse[column]];
                }
                covariance[row * 9 + row] += tau * tau;
            }
            std::vector<double> lower(9 * 9);
            choleskyLower(covariance.data(), lower.data(), 9);
            double logdet = 0.0;
            for (std::size_t row = 0; row < 9; ++row) logdet += 2.0 * std::log(lower[row * 9 + row]);
            appendF32(output, logdet);
        }
    }
    return output;
}

void validatePhasePayload(const std::vector<std::uint8_t> &payload)
{
    constexpr std::size_t phaseBytes = 337396;
    constexpr std::size_t expectedBytes = 18 * phaseBytes;
    constexpr std::size_t area = 49;
    constexpr std::size_t components = 32;
    constexpr std::size_t coarseArea = 9;
    if (payload.size() != expectedBytes) {
        throw std::runtime_error("TGMR phase payload does not have the frozen size");
    }
    const auto contracts = xtransPhaseContracts();
    const std::array<std::uint32_t, coarseArea> expectedCoarse{{
        16,17,18,23,24,25,30,31,32,
    }};
    const std::uint8_t *cursor = payload.data();
    auto u32 = [&]() {
        const std::uint32_t result = get32(cursor);
        cursor += 4;
        return result;
    };
    auto f32 = [&]() {
        const float result = getFloat(cursor);
        cursor += 4;
        return result;
    };
    for (std::size_t phase = 0; phase < contracts.size(); ++phase) {
        for (std::uint32_t expected : contracts[phase].observedIndices) {
            if (u32() != expected) {
                throw std::runtime_error("TGMR phase payload observation contract changed");
            }
        }
        if (u32() != contracts[phase].sampledCenterChannel
            || u32() != contracts[phase].targetChannels[0]
            || u32() != contracts[phase].targetChannels[1]) {
            throw std::runtime_error("TGMR phase payload target contract changed");
        }
        for (std::size_t value = 0;
             value < components + components * area + components * 2; ++value) {
            f32();
        }
        for (std::size_t component = 0; component < components; ++component) {
            for (std::size_t row = 0; row < area; ++row) {
                for (std::size_t column = 0; column < area; ++column) {
                    const float value = f32();
                    if ((row == column && !(value > 0.f))
                        || (column > row && value != 0.f)) {
                        throw std::runtime_error("TGMR full Cholesky factor is noncanonical");
                    }
                }
            }
        }
        for (std::size_t value = 0;
             value < components + components * 2 * area; ++value) {
            f32();
        }
        for (std::uint32_t expected : expectedCoarse) {
            if (u32() != expected) {
                throw std::runtime_error("TGMR coarse support changed");
            }
        }
        for (std::size_t component = 0; component < components; ++component) {
            for (std::size_t row = 0; row < coarseArea; ++row) {
                for (std::size_t column = 0; column < coarseArea; ++column) {
                    const float value = f32();
                    if ((row == column && !(value > 0.f))
                        || (column > row && value != 0.f)) {
                        throw std::runtime_error("TGMR coarse Cholesky factor is noncanonical");
                    }
                }
            }
        }
        for (std::size_t component = 0; component < components; ++component) f32();
        if (static_cast<std::size_t>(cursor - payload.data()) != (phase + 1) * phaseBytes) {
            throw std::runtime_error("TGMR phase payload record size changed");
        }
    }
}

} // namespace tgmr
