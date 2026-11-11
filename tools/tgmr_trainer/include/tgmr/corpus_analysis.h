#pragma once

#include "tgmr/corpus.h"

#include <array>
#include <cstdint>
#include <string>

namespace tgmr
{

// These fixed boundaries are part of corpus-v1's auditable coverage contract.
// They are deliberately simple and operate on normalized, linear RGB patches.
struct CorpusStatistics final {
    CorpusInspection inspection;
    std::array<std::uint64_t, 3> uniqueSources{};
    std::array<std::array<std::uint64_t, 3>, 3> brightness{};
    std::array<std::array<std::uint64_t, 3>, 3> chroma{};
    std::array<std::array<std::uint64_t, 3>, 3> texture{};
    std::array<double, 3> meanLuminance{};
    std::array<double, 3> meanChroma{};
    std::array<double, 3> meanGradient{};
    std::array<double, 2> brightnessThresholds{{0.08,0.65}};
    std::array<double, 2> chromaThresholds{{0.03,0.15}};
    std::array<double, 2> textureThresholds{{0.01,0.05}};
};

// The fixed corpus-v1 signal strata are also used by validation and release
// qualification. Keeping this calculation shared ensures that a patch counted
// as, for example, high-chroma during the balance gate has the same meaning in
// the model-error report.
struct PatchSignalStrata final {
    unsigned brightness = 0;
    unsigned chroma = 0;
    unsigned texture = 0;
};

PatchSignalStrata fixedCorpusV1PatchStrata(const PatchRecord &record);

CorpusStatistics analyzeCorpus(const std::string &path);
CorpusStatistics analyzeCorpusTrainingTertiles(const std::string &path);
std::string canonicalCorpusReportJson(const CorpusStatistics &statistics);
std::string corpusReportCsv(const CorpusStatistics &statistics);
std::string corpusReportHtml(const CorpusStatistics &statistics);
std::string canonicalBalanceJson(
    const CorpusStatistics &statistics,
    std::uint64_t requiredTraining = 10'000,
    std::uint64_t requiredValidationTest = 1'000);

} // namespace tgmr
