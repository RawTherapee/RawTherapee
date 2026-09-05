#include "tgmr/validation.h"

#include "tgmr/corpus_analysis.h"
#include "tgmr/model_v2.h"
#include "tgmr/sha256.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace tgmr
{
namespace
{

constexpr unsigned AREA = 49;
constexpr unsigned COMPONENTS = 32;
constexpr unsigned SHORTLIST = 8;
constexpr unsigned COARSE = 9;
constexpr unsigned PHASES = 18;
constexpr std::size_t PHASE_BYTES = 337396;
constexpr std::size_t PAYLOAD_BYTES = PHASES * PHASE_BYTES;

struct Phase final {
    std::array<std::uint32_t, AREA> observed{};
    std::uint32_t sampled = 0;
    std::array<std::uint32_t, 2> targets{};
    std::array<float, COMPONENTS> logWeights{};
    std::vector<float> meansObserved;
    std::vector<float> meansTarget;
    std::vector<float> fullLower;
    std::array<float, COMPONENTS> fullLogdet{};
    std::vector<float> gains;
    std::array<std::uint32_t, COARSE> coarsePositions{};
    std::vector<float> coarseLower;
    std::array<float, COMPONENTS> coarseLogdet{};
    std::array<float, COMPONENTS> coarseScale{};
    std::array<float, COMPONENTS> fullScale{};
    std::array<unsigned, 3> channelCounts{};
};

class Reader final
{
public:
    explicit Reader(const std::vector<std::uint8_t> &input) : input_(input) {}

    std::uint32_t u32()
    {
        if (offset_ > input_.size() || input_.size() - offset_ < 4) {
            throw std::runtime_error("TGMR validation payload is truncated");
        }
        const std::uint32_t value = static_cast<std::uint32_t>(input_[offset_])
            | (static_cast<std::uint32_t>(input_[offset_ + 1]) << 8)
            | (static_cast<std::uint32_t>(input_[offset_ + 2]) << 16)
            | (static_cast<std::uint32_t>(input_[offset_ + 3]) << 24);
        offset_ += 4;
        return value;
    }

    float f32()
    {
        const std::uint32_t bits = u32();
        float value = 0.f;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) {
            throw std::runtime_error("TGMR validation payload contains a non-finite value");
        }
        return value;
    }

    std::size_t offset() const { return offset_; }

private:
    const std::vector<std::uint8_t> &input_;
    std::size_t offset_ = 0;
};

template<typename Container>
void floats(Reader &reader, Container &output)
{
    for (float &value : output) value = reader.f32();
}

std::vector<std::uint8_t> readModel(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open TGMR validation model");
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (size < 0 || static_cast<std::uint64_t>(size) > 64ULL * 1024 * 1024) {
        throw std::runtime_error("TGMR validation model size is invalid");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char *>(bytes.data()), size);
    if (!stream) throw std::runtime_error("short TGMR validation model read");
    return bytes;
}

std::array<Phase, PHASES> parsePhases(const std::vector<std::uint8_t> &payload)
{
    if (payload.size() != PAYLOAD_BYTES) {
        throw std::runtime_error("TGMR validation requires the frozen K32/S9/q8 payload");
    }
    Reader reader(payload);
    std::array<Phase, PHASES> phases;
    for (Phase &phase : phases) {
        for (std::uint32_t &value : phase.observed) {
            value = reader.u32();
            if (value >= 3 * AREA) throw std::runtime_error("invalid observed index");
            ++phase.channelCounts[value / AREA];
        }
        phase.sampled = reader.u32();
        phase.targets[0] = reader.u32();
        phase.targets[1] = reader.u32();
        floats(reader, phase.logWeights);
        phase.meansObserved.resize(COMPONENTS * AREA);
        phase.meansTarget.resize(COMPONENTS * 2);
        phase.fullLower.resize(COMPONENTS * AREA * AREA);
        phase.gains.resize(COMPONENTS * 2 * AREA);
        phase.coarseLower.resize(COMPONENTS * COARSE * COARSE);
        floats(reader, phase.meansObserved);
        floats(reader, phase.meansTarget);
        floats(reader, phase.fullLower);
        floats(reader, phase.fullLogdet);
        floats(reader, phase.gains);
        for (std::uint32_t &position : phase.coarsePositions) position = reader.u32();
        floats(reader, phase.coarseLower);
        floats(reader, phase.coarseLogdet);
        if (phase.sampled > 2 || phase.targets[0] > 2 || phase.targets[1] > 2
            || phase.channelCounts[0] == 0 || phase.channelCounts[1] == 0
            || phase.channelCounts[2] == 0) {
            throw std::runtime_error("invalid TGMR validation phase contract");
        }
        float coarseMaximum = -std::numeric_limits<float>::infinity();
        float fullMaximum = -std::numeric_limits<float>::infinity();
        for (unsigned component = 0; component < COMPONENTS; ++component) {
            coarseMaximum = std::max(coarseMaximum,
                phase.logWeights[component] - 0.5f * phase.coarseLogdet[component]);
            fullMaximum = std::max(fullMaximum,
                (phase.logWeights[component] - 0.5f * phase.fullLogdet[component]) / 4.f);
            for (unsigned row = 0; row < AREA; ++row) {
                if (!(phase.fullLower[(component * AREA + row) * AREA + row] > 0.f)) {
                    throw std::runtime_error("invalid full TGMR Cholesky factor");
                }
            }
            for (unsigned row = 0; row < COARSE; ++row) {
                if (!(phase.coarseLower[(component * COARSE + row) * COARSE + row] > 0.f)) {
                    throw std::runtime_error("invalid coarse TGMR Cholesky factor");
                }
            }
        }
        for (unsigned component = 0; component < COMPONENTS; ++component) {
            phase.coarseScale[component] = std::exp((
                phase.logWeights[component] - 0.5f * phase.coarseLogdet[component]
                - coarseMaximum) / 6.f);
            phase.fullScale[component] = std::exp(
                (phase.logWeights[component] - 0.5f * phase.fullLogdet[component]) / 4.f
                - fullMaximum);
        }
    }
    if (reader.offset() != payload.size()) {
        throw std::runtime_error("TGMR validation payload has trailing bytes");
    }
    return phases;
}

template<unsigned N>
float quadratic(const float *lower, const std::array<float, N> &residual)
{
    std::array<float, N> solved{};
    float result = 0.f;
    for (unsigned row = 0; row < N; ++row) {
        float value = residual[row];
        for (unsigned column = 0; column < row; ++column) {
            value -= lower[row * N + column] * solved[column];
        }
        solved[row] = value / lower[row * N + row];
        result += solved[row] * solved[row];
    }
    return result;
}

std::array<float, 3> predict(const Phase &phase, const PatchRecord &record)
{
    std::array<float, AREA> observed{};
    std::array<float, 3> sums{};
    for (unsigned position = 0; position < AREA; ++position) {
        const std::uint32_t index = phase.observed[position];
        observed[position] = record.rgb[index] / 65535.f;
        sums[index / AREA] += observed[position];
    }
    float dc = 0.f;
    for (unsigned channel = 0; channel < 3; ++channel) {
        dc += sums[channel] / phase.channelCounts[channel];
    }
    dc /= 3.f;
    std::array<float, AREA> centered{};
    for (unsigned position = 0; position < AREA; ++position) {
        centered[position] = observed[position] - dc;
    }
    std::array<float, SHORTLIST> shortlistScores;
    shortlistScores.fill(-std::numeric_limits<float>::infinity());
    std::array<unsigned, SHORTLIST> ids;
    ids.fill(COMPONENTS);
    for (unsigned component = 0; component < COMPONENTS; ++component) {
        std::array<float, COARSE> residual{};
        for (unsigned row = 0; row < COARSE; ++row) {
            const unsigned position = phase.coarsePositions[row];
            residual[row] = centered[position]
                - phase.meansObserved[component * AREA + position];
        }
        const float q = quadratic<COARSE>(
            phase.coarseLower.data() + component * COARSE * COARSE, residual);
        const float score = phase.coarseScale[component] / (1.f + q / 3.f);
        unsigned insertion = SHORTLIST;
        for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
            if (score > shortlistScores[slot]
                || (score == shortlistScores[slot] && component < ids[slot])) {
                insertion = slot;
                break;
            }
        }
        if (insertion != SHORTLIST) {
            for (unsigned slot = SHORTLIST - 1; slot > insertion; --slot) {
                shortlistScores[slot] = shortlistScores[slot - 1];
                ids[slot] = ids[slot - 1];
            }
            shortlistScores[insertion] = score;
            ids[insertion] = component;
        }
    }
    std::array<float, SHORTLIST> weights{};
    std::array<std::array<float, 2>, SHORTLIST> predictions{};
    float totalWeight = 0.f;
    for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
        const unsigned component = ids[slot];
        std::array<float, AREA> residual{};
        for (unsigned position = 0; position < AREA; ++position) {
            residual[position] = centered[position]
                - phase.meansObserved[component * AREA + position];
        }
        const float q = quadratic<AREA>(
            phase.fullLower.data() + component * AREA * AREA, residual);
        const float s = 1.f + q / 3.f;
        const float s2 = s * s;
        const float s4 = s2 * s2;
        weights[slot] = phase.fullScale[component] / (s4 * s2 * std::sqrt(s));
        totalWeight += weights[slot];
        for (unsigned target = 0; target < 2; ++target) {
            float value = phase.meansTarget[component * 2 + target] + dc;
            const float *gain = phase.gains.data() + (component * 2 + target) * AREA;
            for (unsigned position = 0; position < AREA; ++position) {
                value += gain[position] * residual[position];
            }
            predictions[slot][target] = value;
        }
    }
    if (!(totalWeight > 0.f) || !std::isfinite(totalWeight)) {
        throw std::runtime_error("TGMR validation posterior is non-finite");
    }
    std::array<float, 3> output{};
    for (unsigned target = 0; target < 2; ++target) {
        for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
            output[phase.targets[target]] +=
                predictions[slot][target] * (weights[slot] / totalWeight);
        }
    }
    output[phase.sampled] = record.rgb[phase.sampled * AREA + AREA / 2] / 65535.f;
    if (!std::all_of(output.begin(), output.end(), [](float value) {
            return std::isfinite(value);
        })) {
        throw std::runtime_error("TGMR validation prediction is non-finite");
    }
    return output;
}

const char *splitName(CorpusSplit split)
{
    switch (split) {
        case CorpusSplit::TRAIN: return "train";
        case CorpusSplit::VALIDATION: return "validation";
        case CorpusSplit::TEST: return "test";
    }
    return "unknown";
}

double psnr(double mse)
{
    return mse == 0.0 ? std::numeric_limits<double>::infinity()
                      : -10.0 * std::log10(mse);
}

} // namespace

ValidationReport validateModelOnCorpus(
    const std::string &modelPath,
    const std::string &corpusPath,
    CorpusSplit selectedSplit,
    std::uint64_t patchLimit)
{
    const auto start = std::chrono::steady_clock::now();
    const auto modelBytes = readModel(modelPath);
    std::vector<std::uint8_t> payload;
    const ModelV2Inspection modelInspection = inspectModelV2(modelBytes, &payload);
    const auto phases = parsePhases(payload);
    ValidationReport report;
    report.modelSha256 = hex(sha256(modelBytes.data(), modelBytes.size()));
    report.split = selectedSplit;
    report.requestedLimit = patchLimit;
    std::vector<double> patchErrors;
    std::array<double, PHASES> phaseSquared{};
    std::array<std::uint64_t, PHASES> phaseValues{};
    std::array<std::array<double, 3>, 3> stratumSquared{};
    struct SourceAccumulator { double squared = 0.0; std::uint64_t values = 0; };
    std::map<std::uint32_t, SourceAccumulator> sourceErrors;
    const CorpusInspection corpus = inspectCorpus(corpusPath,
        [&](const PatchRecord &record, std::uint64_t) {
            if (record.split != selectedSplit
                || (patchLimit != 0 && report.metrics.patches >= patchLimit)) {
                return;
            }
            double patchSquared = 0.0;
            for (unsigned phase = 0; phase < PHASES; ++phase) {
                const auto prediction = predict(phases[phase], record);
                double phaseError = 0.0;
                for (unsigned channel = 0; channel < 3; ++channel) {
                    const double truth = record.rgb[channel * AREA + AREA / 2] / 65535.0;
                    const double difference = prediction[channel] - truth;
                    phaseError += difference * difference;
                }
                patchSquared += phaseError;
                phaseSquared[phase] += phaseError;
                phaseValues[phase] += 3;
            }
            const std::uint64_t values = PHASES * 3;
            report.metrics.scalarValues += values;
            ++report.metrics.patches;
            patchErrors.push_back(std::sqrt(patchSquared / values));
            sourceErrors[record.sourceOrdinal].squared += patchSquared;
            sourceErrors[record.sourceOrdinal].values += values;
            const PatchSignalStrata strata = fixedCorpusV1PatchStrata(record);
            const std::array<unsigned, 3> levels{{
                strata.brightness, strata.chroma, strata.texture}};
            for (unsigned metric = 0; metric < levels.size(); ++metric) {
                StratumValidationMetrics &cell =
                    report.metrics.strata[metric][levels[metric]];
                ++cell.patches;
                cell.scalarValues += values;
                stratumSquared[metric][levels[metric]] += patchSquared;
            }
        });
    if (modelInspection.identity.corpusSha256 != corpus.header.payloadSha256) {
        throw std::runtime_error(
            "TGMR validation corpus payload differs from the model corpus identity");
    }
    report.corpusPayloadSha256 = hex(corpus.header.payloadSha256);
    if (report.metrics.patches == 0 || report.metrics.scalarValues == 0) {
        throw std::runtime_error("TGMR validation split contains no selected patches");
    }
    const double totalSquared = std::accumulate(
        phaseSquared.begin(), phaseSquared.end(), 0.0);
    report.metrics.mse = totalSquared / report.metrics.scalarValues;
    report.metrics.psnr = psnr(report.metrics.mse);
    std::sort(patchErrors.begin(), patchErrors.end());
    const std::size_t p99 = std::min(patchErrors.size() - 1,
        static_cast<std::size_t>(std::floor(0.99 * patchErrors.size())));
    report.metrics.patchRmsP99 = patchErrors[p99];
    report.metrics.worstPatchRms = patchErrors.back();
    std::vector<double> sourcePsnr;
    sourcePsnr.reserve(sourceErrors.size());
    for (const auto &entry : sourceErrors) {
        SourceValidationMetrics metrics;
        metrics.sourceOrdinal = entry.first;
        metrics.scalarValues = entry.second.values;
        metrics.patches = entry.second.values / (PHASES * 3);
        metrics.mse = entry.second.squared / entry.second.values;
        metrics.psnr = psnr(metrics.mse);
        sourcePsnr.push_back(metrics.psnr);
        report.metrics.sources.push_back(metrics);
    }
    std::sort(sourcePsnr.begin(), sourcePsnr.end());
    report.metrics.medianSourcePsnr = sourcePsnr[sourcePsnr.size() / 2];
    for (unsigned phase = 0; phase < PHASES; ++phase) {
        report.metrics.phasePsnr[phase] = psnr(phaseSquared[phase] / phaseValues[phase]);
    }
    const auto phaseRange = std::minmax_element(
        report.metrics.phasePsnr.begin(), report.metrics.phasePsnr.end());
    report.metrics.phasePsnrSpread = *phaseRange.second - *phaseRange.first;
    for (unsigned metric = 0; metric < report.metrics.strata.size(); ++metric) {
        for (unsigned level = 0; level < report.metrics.strata[metric].size(); ++level) {
            StratumValidationMetrics &cell = report.metrics.strata[metric][level];
            if (cell.scalarValues == 0) {
                // Limited development validations can legitimately omit a
                // stratum. The complete production splits are separately
                // required to satisfy the corpus balance gate.
                cell.mse = 0.0;
                cell.psnr = 0.0;
                continue;
            }
            cell.mse = stratumSquared[metric][level] / cell.scalarValues;
            cell.psnr = psnr(cell.mse);
        }
    }
    report.elapsedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    return report;
}

bool canonicalMsePsnrConsistent(double mse, double psnrValue)
{
    if (!(mse > 0.0) || !std::isfinite(mse) || !std::isfinite(psnrValue)) {
        return false;
    }

    // std::fixed/setprecision(12) introduces at most half a unit in the last
    // emitted decimal for each value.  Include the much smaller error obtained
    // by mapping the rounded PSNR back into MSE, plus floating-point evaluation
    // slack.  This remains tight enough to reject any material report change.
    constexpr double halfDecimalUnit = 0.5e-12;
    const double psnrMse = std::pow(10.0, -psnrValue / 10.0);
    const double mappedPsnrRounding = psnrMse
        * (std::log(10.0) / 10.0) * halfDecimalUnit;
    const double arithmeticSlack = 8.0 * std::numeric_limits<double>::epsilon()
        * std::max(std::abs(mse), std::abs(psnrMse));
    return std::abs(psnrMse - mse)
        <= halfDecimalUnit + mappedPsnrRounding + arithmeticSlack;
}

std::string canonicalValidationJson(const ValidationReport &report)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"corpus_payload_sha256\": \"" << report.corpusPayloadSha256 << "\",\n"
        << "  \"format\": \"rawtherapee-tgmr-validation-report-v1\",\n"
        << "  \"median_source_psnr\": " << report.metrics.medianSourcePsnr << ",\n"
        << "  \"model_sha256\": \"" << report.modelSha256 << "\",\n"
        << "  \"mse\": " << report.metrics.mse << ",\n"
        << "  \"patch_rms_p99\": " << report.metrics.patchRmsP99 << ",\n"
        << "  \"patches\": " << report.metrics.patches << ",\n"
        << "  \"phase_psnr\": [";
    for (unsigned phase = 0; phase < PHASES; ++phase) {
        if (phase) output << ", ";
        output << report.metrics.phasePsnr[phase];
    }
    output << "],\n"
        << "  \"phase_psnr_spread\": " << report.metrics.phasePsnrSpread << ",\n"
        << "  \"psnr\": " << report.metrics.psnr << ",\n"
        << "  \"requested_limit\": " << report.requestedLimit << ",\n"
        << "  \"scalar_values\": " << report.metrics.scalarValues << ",\n"
        << "  \"sources\": [";
    for (std::size_t index = 0; index < report.metrics.sources.size(); ++index) {
        const SourceValidationMetrics &source = report.metrics.sources[index];
        if (index) output << ',';
        output << "\n    {\"mse\": " << source.mse
            << ", \"patches\": " << source.patches
            << ", \"psnr\": " << source.psnr
            << ", \"scalar_values\": " << source.scalarValues
            << ", \"source_ordinal\": " << source.sourceOrdinal << '}';
    }
    if (!report.metrics.sources.empty()) output << '\n';
    output
        << "  ],\n"
        << "  \"split\": \"" << splitName(report.split) << "\",\n"
        << "  \"strata\": {\n";
    static const char *metricNames[] = {"brightness", "chroma", "texture"};
    static const char *levelNames[] = {"low", "middle", "high"};
    for (unsigned metric = 0; metric < report.metrics.strata.size(); ++metric) {
        output << "    \"" << metricNames[metric] << "\": [";
        for (unsigned level = 0; level < report.metrics.strata[metric].size(); ++level) {
            const StratumValidationMetrics &cell = report.metrics.strata[metric][level];
            if (level) output << ',';
            output << "\n      {\"level\": \"" << levelNames[level]
                << "\", \"mse\": " << cell.mse
                << ", \"patches\": " << cell.patches
                << ", \"psnr\": " << cell.psnr
                << ", \"scalar_values\": " << cell.scalarValues << '}';
        }
        output << "\n    ]" << (metric == 2 ? "\n" : ",\n");
    }
    output << "  },\n"
        << "  \"worst_patch_rms\": " << report.metrics.worstPatchRms << "\n"
        << "}\n";
    return output.str();
}

} // namespace tgmr
