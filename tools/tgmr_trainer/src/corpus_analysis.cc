#include "tgmr/corpus_analysis.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace tgmr
{
namespace
{

struct PatchMetrics final {
    double luminance = 0.0;
    double chroma = 0.0;
    double gradient = 0.0;
};

unsigned bucket(double value, double first, double second)
{
    return value < first ? 0U : value < second ? 1U : 2U;
}

const char *splitName(unsigned split)
{
    static const char *names[] = {"train", "validation", "test"};
    return names[split];
}

void jsonCounts(
    std::ostringstream &output,
    const std::array<std::array<std::uint64_t, 3>, 3> &counts,
    const char *indent)
{
    output << "{\n";
    for (unsigned split = 0; split < 3; ++split) {
        output << indent << "  \"" << splitName(split) << "\": ["
               << counts[split][0] << ", " << counts[split][1] << ", "
               << counts[split][2] << "]" << (split == 2 ? "\n" : ",\n");
    }
    output << indent << "}";
}

void htmlRows(
    std::ostringstream &output,
    const char *metric,
    const std::array<std::array<std::uint64_t, 3>, 3> &counts)
{
    static const char *levels[] = {"low", "middle", "high"};
    for (unsigned split = 0; split < 3; ++split) {
        for (unsigned level = 0; level < 3; ++level) {
            output << "<tr><td>" << metric << "</td><td>" << splitName(split)
                   << "</td><td>" << levels[level] << "</td><td>"
                   << counts[split][level] << "</td></tr>\n";
        }
    }
}

PatchMetrics metrics(const PatchRecord &record)
{
    PatchMetrics result;
    double gradientEnergy = 0.0;
    for (unsigned y = 0; y < 7; ++y) {
        for (unsigned x = 0; x < 7; ++x) {
            const std::size_t index = y * 7 + x;
            const double r = record.rgb[index] / 65535.0;
            const double g = record.rgb[49 + index] / 65535.0;
            const double b = record.rgb[98 + index] / 65535.0;
            const double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            result.luminance += luma;
            result.chroma += std::max({r, g, b}) - std::min({r, g, b});
            auto priorLuma = [&](std::size_t previous) {
                return 0.2126 * record.rgb[previous] / 65535.0
                    + 0.7152 * record.rgb[49 + previous] / 65535.0
                    + 0.0722 * record.rgb[98 + previous] / 65535.0;
            };
            if (x != 0) {
                const double difference = luma - priorLuma(index - 1);
                gradientEnergy += difference * difference;
            }
            if (y != 0) {
                const double difference = luma - priorLuma(index - 7);
                gradientEnergy += difference * difference;
            }
        }
    }
    result.luminance /= 49.0;
    result.chroma /= 49.0;
    result.gradient = std::sqrt(gradientEnergy / 84.0);
    return result;
}

double quantile(std::vector<double> values, double fraction)
{
    if (values.empty()) throw std::runtime_error("cannot derive strata without training patches");
    const std::size_t index = static_cast<std::size_t>(fraction * (values.size() - 1));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

CorpusStatistics analyzeWithThresholds(
    const std::string &path,
    const std::array<double, 2> &brightnessThresholds,
    const std::array<double, 2> &chromaThresholds,
    const std::array<double, 2> &textureThresholds)
{
    CorpusStatistics result;
    result.brightnessThresholds = brightnessThresholds;
    result.chromaThresholds = chromaThresholds;
    result.textureThresholds = textureThresholds;
    std::array<std::set<std::array<std::uint8_t, 32>>, 3> sources;
    std::array<std::uint64_t, 3> counts{};
    result.inspection = inspectCorpus(path,
        [&](const PatchRecord &record, std::uint64_t) {
            const unsigned split = static_cast<unsigned>(record.split) - 1;
            sources[split].insert(record.sourceIdSha256);
            ++counts[split];
            const PatchMetrics value = metrics(record);
            ++result.brightness[split][bucket(
                value.luminance, brightnessThresholds[0], brightnessThresholds[1])];
            ++result.chroma[split][bucket(
                value.chroma, chromaThresholds[0], chromaThresholds[1])];
            ++result.texture[split][bucket(
                value.gradient, textureThresholds[0], textureThresholds[1])];
            result.meanLuminance[split] += value.luminance;
            result.meanChroma[split] += value.chroma;
            result.meanGradient[split] += value.gradient;
        });
    for (unsigned split = 0; split < 3; ++split) {
        result.uniqueSources[split] = sources[split].size();
        if (counts[split] != 0) {
            result.meanLuminance[split] /= counts[split];
            result.meanChroma[split] /= counts[split];
            result.meanGradient[split] /= counts[split];
        }
    }
    return result;
}

} // namespace

CorpusStatistics analyzeCorpus(const std::string &path)
{
    return analyzeWithThresholds(path, {{0.08,0.65}}, {{0.03,0.15}}, {{0.01,0.05}});
}

PatchSignalStrata fixedCorpusV1PatchStrata(const PatchRecord &record)
{
    const PatchMetrics value = metrics(record);
    PatchSignalStrata output;
    output.brightness = bucket(value.luminance, 0.08, 0.65);
    output.chroma = bucket(value.chroma, 0.03, 0.15);
    output.texture = bucket(value.gradient, 0.01, 0.05);
    return output;
}

CorpusStatistics analyzeCorpusTrainingTertiles(const std::string &path)
{
    std::vector<double> brightness;
    std::vector<double> chroma;
    std::vector<double> texture;
    inspectCorpus(path, [&](const PatchRecord &record, std::uint64_t) {
        if (record.split != CorpusSplit::TRAIN) return;
        const PatchMetrics value = metrics(record);
        brightness.push_back(value.luminance);
        chroma.push_back(value.chroma);
        texture.push_back(value.gradient);
    });
    return analyzeWithThresholds(path,
        {{quantile(brightness, 1.0 / 3.0), quantile(brightness, 2.0 / 3.0)}},
        {{quantile(chroma, 1.0 / 3.0), quantile(chroma, 2.0 / 3.0)}},
        {{quantile(texture, 1.0 / 3.0), quantile(texture, 2.0 / 3.0)}});
}

std::string canonicalCorpusReportJson(const CorpusStatistics &statistics)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(10)
        << "{\n"
        << "  \"brightness_counts\": ";
    jsonCounts(output, statistics.brightness, "  ");
    output << ",\n  \"chroma_counts\": ";
    jsonCounts(output, statistics.chroma, "  ");
    output << ",\n"
        << "  \"format\": \"rawtherapee-tgmr-corpus-statistics-v1\",\n"
        << "  \"mean_chroma\": [" << statistics.meanChroma[0] << ", "
        << statistics.meanChroma[1] << ", " << statistics.meanChroma[2] << "],\n"
        << "  \"mean_gradient\": [" << statistics.meanGradient[0] << ", "
        << statistics.meanGradient[1] << ", " << statistics.meanGradient[2] << "],\n"
        << "  \"mean_luminance\": [" << statistics.meanLuminance[0] << ", "
        << statistics.meanLuminance[1] << ", " << statistics.meanLuminance[2] << "],\n"
        << "  \"payload_sha256\": \""
        << hex(statistics.inspection.header.payloadSha256) << "\",\n"
        << "  \"record_count\": " << statistics.inspection.header.recordCount << ",\n"
        << "  \"strata\": {\n"
        << "    \"brightness\": [" << statistics.brightnessThresholds[0] << ", "
        << statistics.brightnessThresholds[1] << "],\n"
        << "    \"chroma\": [" << statistics.chromaThresholds[0] << ", "
        << statistics.chromaThresholds[1] << "],\n"
        << "    \"texture_gradient_rms\": [" << statistics.textureThresholds[0]
        << ", " << statistics.textureThresholds[1] << "]\n"
        << "  },\n"
        << "  \"texture_counts\": ";
    jsonCounts(output, statistics.texture, "  ");
    output << ",\n  \"unique_sources\": {\"test\": " << statistics.uniqueSources[2]
        << ", \"train\": " << statistics.uniqueSources[0]
        << ", \"validation\": " << statistics.uniqueSources[1] << "}\n"
        << "}\n";
    return output.str();
}

std::string corpusReportCsv(const CorpusStatistics &statistics)
{
    std::ostringstream output;
    output << "metric,split,stratum,count\n";
    static const char *levels[] = {"low", "middle", "high"};
    const std::pair<const char *, const std::array<std::array<std::uint64_t, 3>, 3> *> groups[] = {
        {"brightness", &statistics.brightness},
        {"chroma", &statistics.chroma},
        {"texture", &statistics.texture},
    };
    for (const auto &group : groups) {
        for (unsigned split = 0; split < 3; ++split) {
            for (unsigned level = 0; level < 3; ++level) {
                output << group.first << ',' << splitName(split) << ',' << levels[level]
                       << ',' << (*group.second)[split][level] << '\n';
            }
        }
    }
    return output.str();
}

std::string corpusReportHtml(const CorpusStatistics &statistics)
{
    std::ostringstream output;
    output << "<!doctype html>\n<meta charset=\"utf-8\">\n"
        << "<title>RawTherapee TGMR corpus report</title>\n"
        << "<style>body{font:16px system-ui;margin:2rem;max-width:70rem}"
           "table{border-collapse:collapse}th,td{border:1px solid #aaa;padding:.35rem .6rem}"
           "th{background:#eee;text-align:left}</style>\n"
        << "<h1>RawTherapee TGMR corpus report</h1>\n"
        << "<p>Authenticated payload <code>"
        << hex(statistics.inspection.header.payloadSha256) << "</code>; "
        << statistics.inspection.header.recordCount << " patches.</p>\n"
        << "<table><thead><tr><th>Metric</th><th>Split</th><th>Stratum</th>"
           "<th>Count</th></tr></thead><tbody>\n";
    htmlRows(output, "brightness", statistics.brightness);
    htmlRows(output, "chroma", statistics.chroma);
    htmlRows(output, "texture", statistics.texture);
    output << "</tbody></table>\n";
    return output.str();
}

std::string canonicalBalanceJson(
    const CorpusStatistics &statistics,
    std::uint64_t requiredTraining,
    std::uint64_t requiredValidationTest)
{
    std::ostringstream output;
    output << "{\n  \"deficits\": [\n";
    bool first = true;
    const std::pair<const char *, const std::array<std::array<std::uint64_t, 3>, 3> *> groups[] = {
        {"brightness", &statistics.brightness},
        {"chroma", &statistics.chroma},
        {"texture", &statistics.texture},
    };
    static const char *levels[] = {"low", "middle", "high"};
    for (const auto &group : groups) {
        for (unsigned split = 0; split < 3; ++split) {
            const std::uint64_t required = split == 0 ? requiredTraining : requiredValidationTest;
            for (unsigned level = 0; level < 3; ++level) {
                const std::uint64_t have = (*group.second)[split][level];
                if (have >= required) continue;
                if (!first) output << ",\n";
                first = false;
                output << "    {\"additional_patches_required\": " << required - have
                    << ", \"metric\": \"" << group.first << "\", \"split\": \""
                    << splitName(split) << "\", \"stratum\": \"" << levels[level]
                    << "\"}";
            }
        }
    }
    output << (first ? "" : "\n")
        << "  ],\n  \"format\": \"rawtherapee-tgmr-corpus-balance-v1\",\n"
        << "  \"minimum_test_per_stratum\": " << requiredValidationTest << ",\n"
        << "  \"minimum_training_per_stratum\": " << requiredTraining << ",\n"
        << "  \"minimum_validation_per_stratum\": " << requiredValidationTest << ",\n"
        << "  \"payload_sha256\": \""
        << hex(statistics.inspection.header.payloadSha256) << "\",\n"
        << "  \"status\": \"" << (first ? "balanced" : "deficient") << "\"\n}\n";
    return output.str();
}

} // namespace tgmr
