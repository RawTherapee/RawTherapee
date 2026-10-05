#include "tgmr/manifest.h"

#include "tgmr/sha256.h"

#include "cJSON.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace tgmr
{
namespace
{

constexpr std::size_t CATALOGS = 4;
constexpr std::size_t SPLITS = 3;

const std::array<const char *, CATALOGS> CATALOG_NAMES{{
    "openimages-cvdf-v5-boxable", "pass-v3", "wikimedia-commons",
    "smithsonian-open-access",
}};

const std::array<const char *, SPLITS> SPLIT_NAMES{{"train", "validation", "test"}};

const std::array<std::array<std::size_t, SPLITS>, CATALOGS> FROZEN_QUOTAS{{
    {{3200,400,400}}, {{0,0,0}}, {{480,60,60}}, {{320,40,40}},
}};

std::string readText(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open corpus selection recipe: " + path);
    std::ostringstream output;
    output << input.rdbuf();
    if (input.bad()) throw std::runtime_error("cannot read corpus selection recipe");
    return output.str();
}

const cJSON *required(const cJSON *object, const char *name)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!value) throw std::runtime_error(std::string("selection recipe lacks ") + name);
    return value;
}

std::string requiredText(const cJSON *object, const char *name)
{
    const cJSON *value = required(object, name);
    if (!cJSON_IsString(value) || !value->valuestring || !*value->valuestring) {
        throw std::runtime_error(std::string("selection recipe ") + name
                                 + " is not a non-empty string");
    }
    return value->valuestring;
}

std::size_t exactSize(const cJSON *object, const char *name)
{
    const cJSON *value = required(object, name);
    if (!cJSON_IsNumber(value) || value->valuedouble < 0.0
        || std::floor(value->valuedouble) != value->valuedouble
        || value->valuedouble > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string("selection recipe ") + name
                                 + " is not an exact size");
    }
    return static_cast<std::size_t>(value->valuedouble);
}

std::string jsonString(const std::string &value)
{
    std::ostringstream output;
    output << '"';
    static const char hex[] = "0123456789abcdef";
    for (unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u00" << hex[character >> 4] << hex[character & 15];
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
    return output.str();
}

std::string splitName(CorpusSplit split)
{
    switch (split) {
        case CorpusSplit::TRAIN: return "train";
        case CorpusSplit::VALIDATION: return "validation";
        case CorpusSplit::TEST: return "test";
    }
    throw std::runtime_error("invalid corpus split");
}

std::size_t catalogIndex(const std::string &name)
{
    const auto entry = std::find(CATALOG_NAMES.begin(), CATALOG_NAMES.end(), name);
    if (entry == CATALOG_NAMES.end()) throw std::runtime_error("unsupported candidate catalog");
    return static_cast<std::size_t>(entry - CATALOG_NAMES.begin());
}

std::size_t splitIndex(CorpusSplit split)
{
    return static_cast<std::size_t>(static_cast<unsigned>(split) - 1U);
}

unsigned popcount64(std::uint64_t value)
{
    unsigned count = 0;
    while (value) {
        value &= value - 1;
        ++count;
    }
    return count;
}

std::uint64_t signature(const std::string &value)
{
    return std::stoull(value, nullptr, 16);
}

std::uint64_t stableOrder(const std::string &seed, const std::string &identity)
{
    const std::string combined = seed + '\0' + identity;
    const auto digest = sha256(combined.data(), combined.size());
    std::uint64_t output = 0;
    for (unsigned byte = 0; byte < 8; ++byte) {
        output = (output << 8) | digest[byte];
    }
    return output;
}

CorpusSplit assignedSplit(const std::string &seed, const SourceRecord &record)
{
    const unsigned bucket = static_cast<unsigned>(stableOrder(seed, record.authorId) % 20U);
    if (bucket < 16) return CorpusSplit::TRAIN;
    if (bucket < 18) return CorpusSplit::VALIDATION;
    return CorpusSplit::TEST;
}

double quantile(std::vector<double> values, double fraction)
{
    if (values.empty()) throw std::runtime_error("cannot derive selection tertiles from no candidates");
    const std::size_t index = static_cast<std::size_t>(fraction * (values.size() - 1));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

unsigned tertile(double value, double first, double second)
{
    return value <= first ? 0U : value <= second ? 1U : 2U;
}

bool hasTag(const SourceRecord &record, const char *tag)
{
    return std::find(record.contentTags.begin(), record.contentTags.end(), tag)
        != record.contentTags.end();
}

std::string classificationJson(const ImageClassification &value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(10)
        << "{\"channel_means\":[" << value.channelMeans[0] << ','
        << value.channelMeans[1] << ',' << value.channelMeans[2] << "],"
        << "\"chroma_ratio_mean\":" << value.chromaRatioMean << ','
        << "\"clipped_black_fraction\":" << value.clippedBlackFraction << ','
        << "\"clipped_white_fraction\":" << value.clippedWhiteFraction << ','
        << "\"dhash\":" << jsonString(value.perceptualHash) << ','
        << "\"gradient_rms\":" << value.gradientRms << ','
        << "\"hue_degrees\":" << value.hueDegrees << ",\"hue_histogram\":[";
    for (std::size_t index = 0; index < value.hueHistogram.size(); ++index) {
        if (index) output << ',';
        output << value.hueHistogram[index];
    }
    output << "],\"jpeg_blockiness\":" << value.jpegBlockiness
        << ",\"laplacian_rms\":" << value.laplacianRms
        << ",\"local_contrast\":" << value.localContrast
        << ",\"luminance_histogram\":[";
    for (std::size_t index = 0; index < value.luminanceHistogram.size(); ++index) {
        if (index) output << ',';
        output << value.luminanceHistogram[index];
    }
    output << "],\"luminance_mean\":" << value.luminanceMean
        << ",\"luminance_p01\":" << value.luminanceP01
        << ",\"luminance_p99\":" << value.luminanceP99
        << ",\"luminance_stddev\":" << value.luminanceStddev
        << ",\"perceptual_hash\":" << jsonString(value.perceptualHash)
        << ",\"phash\":" << jsonString(value.pHash)
        << ",\"saturation_histogram\":[";
    for (std::size_t index = 0; index < value.saturationHistogram.size(); ++index) {
        if (index) output << ',';
        output << value.saturationHistogram[index];
    }
    output << "],\"saturation_mean\":" << value.saturationMean << '}';
    return output.str();
}

std::string canonicalRecord(const SourceRecord &record)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '{'
        << "\"advertised_checksum\":"
        << (record.advertisedChecksum.empty() ? "null" : jsonString(record.advertisedChecksum))
        << ",\"archive_fallbacks\":[";
    for (std::size_t index = 0; index < record.archiveFallbacks.size(); ++index) {
        if (index) output << ',';
        const auto &archive = record.archiveFallbacks[index];
        output << "{\"member\":" << jsonString(archive.member)
            << ",\"member_sha256\":" << jsonString(archive.memberSha256)
            << ",\"sha256\":" << jsonString(archive.sha256)
            << ",\"url\":" << jsonString(archive.url) << '}';
    }
    output << "],\"author\":" << jsonString(record.author)
        << ",\"author_id\":" << jsonString(record.authorId)
        << ",\"author_url\":" << jsonString(record.authorUrl)
        << ",\"cache_filename\":" << jsonString(record.cacheFilename)
        << ",\"catalog\":{\"name\":" << jsonString(record.catalogName)
        << ",\"revision\":" << jsonString(record.catalogRevision)
        << ",\"snapshot_sha256\":" << jsonString(record.catalogSnapshotSha256) << '}'
        << ",\"classification\":" << classificationJson(record.classification)
        << ",\"content_tags\":[";
    for (std::size_t index = 0; index < record.contentTags.size(); ++index) {
        if (index) output << ',';
        output << jsonString(record.contentTags[index]);
    }
    output << "],\"decoded_pixel_sha256\":" << jsonString(record.decodedPixelSha256)
        << ",\"fallback_urls\":[";
    for (std::size_t index = 0; index < record.fallbackUrls.size(); ++index) {
        if (index) output << ',';
        output << jsonString(record.fallbackUrls[index]);
    }
    output << "],\"file_type\":" << jsonString(record.fileType)
        << ",\"format\":\"rawtherapee-tgmr-corpus-source-manifest-v2\""
        << ",\"height\":" << record.height
        << ",\"icc_identity\":" << jsonString(record.iccIdentity)
        << ",\"landing_page\":" << jsonString(record.landingPage)
        << ",\"license\":" << jsonString(record.license)
        << ",\"license_url\":" << jsonString(record.licenseUrl)
        << ",\"orientation\":" << record.orientation
        << ",\"original_url\":" << jsonString(record.originalUrl)
        << ",\"patch_coordinates\":[";
    for (std::size_t index = 0; index < record.patches.size(); ++index) {
        if (index) output << ',';
        const auto &patch = record.patches[index];
        output << "{\"augmentation\":{\"exposure_stops\":"
            << std::fixed << std::setprecision(10) << patch.exposureStopsQ8 / 256.0
            << ",\"kind\":" << unsigned(patch.augmentationKind)
            << ",\"matrix_id\":" << patch.matrixId
            << ",\"sequence\":" << patch.sequence << ",\"white_balance\":["
            << patch.whiteBalanceQ12[0] / 4096.0 << ','
            << patch.whiteBalanceQ12[1] / 4096.0 << ','
            << patch.whiteBalanceQ12[2] / 4096.0 << "]},\"coverage\":"
            << (patch.coverage ? "true" : "false")
            << ",\"coverage_class\":" << unsigned(patch.coverageClass)
            << ",\"x\":" << patch.x << ",\"y\":" << patch.y << '}';
    }
    output << "],\"patch_sampling_seed\":\"0x"
        << std::hex << std::setfill('0') << std::setw(16) << record.patchSamplingSeed
        << std::dec << "\",\"people_review_status\":"
        << jsonString(record.peopleReviewStatus)
        << ",\"rights\":{\"evidence_revision\":"
        << jsonString(record.rightsEvidenceRevision)
        << ",\"evidence_sha256\":" << jsonString(record.rightsEvidenceSha256)
        << ",\"evidence_url\":" << jsonString(record.rightsEvidenceUrl)
        << ",\"review_status\":" << jsonString(record.rightsReviewStatus) << '}'
        << ",\"selected\":true,\"selection_status\":\"accepted-corpus-v1\""
        << ",\"sha256\":" << jsonString(record.sha256)
        << ",\"source_id\":" << jsonString(record.sourceId)
        << ",\"split\":" << jsonString(splitName(record.split))
        << ",\"title\":" << jsonString(record.title)
        << ",\"upstream_flickr_id\":"
        << (record.upstreamFlickrId.empty() ? "null" : jsonString(record.upstreamFlickrId))
        << ",\"upstream_source_id\":" << jsonString(record.upstreamSourceId)
        << ",\"width\":" << record.width << "}\n";
    return output.str();
}

struct Candidate final {
    SourceRecord record;
    std::size_t catalog = 0;
    std::size_t split = 0;
    unsigned stratum = 0;
    std::uint64_t order = 0;
};

bool suitable(const SourceRecord &record)
{
    static const std::set<std::string> licenses{
        "CC0-1.0", "PDM-1.0", "CC-BY-2.0", "CC-BY-3.0", "CC-BY-4.0",
    };
    if (!record.manifestV2 || record.selected || record.splitAssigned
        || record.selectionStatus != "candidate-reviewed"
        || record.rightsReviewStatus != "approved"
        || licenses.find(record.license) == licenses.end()
        || std::min(record.width, record.height) < 512
        || static_cast<std::uint64_t>(record.width) * record.height < 750000ULL) {
        return false;
    }
    if (hasTag(record, "people")
        && record.peopleReviewStatus != "approved-no-minors-or-sensitive-content") {
        return false;
    }
    return record.peopleReviewStatus != "rejected";
}

} // namespace

void writeSourceManifestV2(
    const std::vector<SourceRecord> &records,
    const std::string &outputManifest,
    bool force)
{
    if (!force && std::filesystem::exists(outputManifest)) {
        throw std::runtime_error("refusing to replace source manifest");
    }
    const std::string temporary = outputManifest + ".tmp";
    try {
        std::ofstream output(temporary, std::ios::binary);
        for (const SourceRecord &record : records) {
            if (!record.manifestV2 || !record.selected || !record.splitAssigned) {
                throw std::runtime_error(
                    "v2 manifest writer accepts only selected assigned v2 records");
            }
            output << canonicalRecord(record);
        }
        output.flush();
        if (!output) throw std::runtime_error("source manifest write failed");
        output.close();
        if (force) std::filesystem::remove(outputManifest);
        std::filesystem::rename(temporary, outputManifest);
    } catch (...) {
        std::filesystem::remove(temporary);
        throw;
    }
}

std::string canonicalSourceRecordV2(const SourceRecord &record)
{
    if (!record.manifestV2 || !record.selected || !record.splitAssigned) {
        throw std::runtime_error(
            "v2 canonical writer accepts only selected assigned v2 records");
    }
    return canonicalRecord(record);
}

void freezeProductionTrainingOrder(
    const std::vector<SourceRecord> &records,
    const std::string &inputManifest,
    const std::string &outputManifest,
    const std::string &orderManifest,
    bool force)
{
    if (outputManifest == orderManifest || outputManifest == inputManifest
        || orderManifest == inputManifest) {
        throw std::runtime_error("training-order inputs and outputs must be distinct");
    }
    if (!force && (std::filesystem::exists(outputManifest)
                   || std::filesystem::exists(orderManifest))) {
        throw std::runtime_error("refusing to replace training-order output");
    }

    static const std::array<std::size_t, 5> milestones{{250,500,1000,2000,4000}};
    // Open Images, Wikimedia Commons, Smithsonian.  PASS deliberately has no
    // production-v1 allocation.
    static const std::array<std::array<std::size_t, 3>, 5> cumulative{{
        {{200,30,20}}, {{400,60,40}}, {{800,120,80}},
        {{1600,240,160}}, {{3200,480,320}},
    }};
    static const std::array<std::size_t, 3> catalogs{{0,2,3}};
    static const char seed[] = "rawtherapee-tgmr-corpus-v1-training-order";

    std::array<std::size_t, SPLITS> splitCounts{};
    std::array<std::array<std::size_t, SPLITS>, CATALOGS> catalogCounts{};
    std::vector<double> luminance;
    std::vector<double> chroma;
    std::vector<double> texture;
    for (const SourceRecord &record : records) {
        if (!record.manifestV2 || !record.selected || !record.splitAssigned) {
            throw std::runtime_error(
                "training order requires selected assigned v2 source records");
        }
        const std::size_t split = splitIndex(record.split);
        ++splitCounts[split];
        ++catalogCounts[catalogIndex(record.catalogName)][split];
        if (record.split == CorpusSplit::TRAIN) {
            luminance.push_back(record.classification.luminanceMean);
            chroma.push_back(record.classification.chromaRatioMean);
            texture.push_back(record.classification.gradientRms);
        }
    }
    if (splitCounts != std::array<std::size_t, SPLITS>{{4000,500,500}}) {
        throw std::runtime_error("training order requires the frozen 4000/500/500 split");
    }
    for (std::size_t catalog = 0; catalog < CATALOGS; ++catalog) {
        for (std::size_t split = 0; split < SPLITS; ++split) {
            if (catalogCounts[catalog][split] != FROZEN_QUOTAS[catalog][split]) {
                throw std::runtime_error("training order requires frozen catalog quotas");
            }
        }
    }

    const std::array<double, 2> luminanceThresholds{{
        quantile(luminance, 1.0 / 3.0), quantile(luminance, 2.0 / 3.0)}};
    const std::array<double, 2> chromaThresholds{{
        quantile(chroma, 1.0 / 3.0), quantile(chroma, 2.0 / 3.0)}};
    const std::array<double, 2> textureThresholds{{
        quantile(texture, 1.0 / 3.0), quantile(texture, 2.0 / 3.0)}};

    struct OrderedSource final {
        const SourceRecord *record = nullptr;
        std::uint64_t order = 0;
    };
    std::array<std::array<std::vector<OrderedSource>, 27>, 3> buckets;
    for (const SourceRecord &record : records) {
        if (record.split != CorpusSplit::TRAIN) continue;
        const std::size_t catalog = catalogIndex(record.catalogName);
        const auto entry = std::find(catalogs.begin(), catalogs.end(), catalog);
        if (entry == catalogs.end()) {
            throw std::runtime_error("training source belongs to a zero-quota catalog");
        }
        const std::size_t compactCatalog = static_cast<std::size_t>(entry - catalogs.begin());
        const unsigned brightness = tertile(
            record.classification.luminanceMean,
            luminanceThresholds[0], luminanceThresholds[1]);
        const unsigned color = tertile(
            record.classification.chromaRatioMean,
            chromaThresholds[0], chromaThresholds[1]);
        const unsigned detail = tertile(
            record.classification.gradientRms,
            textureThresholds[0], textureThresholds[1]);
        const unsigned stratum = brightness * 9U + color * 3U + detail;
        buckets[compactCatalog][stratum].push_back(
            {&record, stableOrder(seed, record.sourceId)});
    }

    std::array<std::vector<const SourceRecord *>, 3> catalogOrder;
    for (std::size_t catalog = 0; catalog < catalogOrder.size(); ++catalog) {
        std::array<std::size_t, 27> selected{};
        std::array<std::size_t, 27> totals{};
        std::size_t total = 0;
        for (std::size_t stratum = 0; stratum < 27; ++stratum) {
            auto &values = buckets[catalog][stratum];
            std::sort(values.begin(), values.end(), [](const OrderedSource &left,
                                                       const OrderedSource &right) {
                return std::tie(left.order, left.record->sourceId)
                    < std::tie(right.order, right.record->sourceId);
            });
            totals[stratum] = values.size();
            total += values.size();
        }
        catalogOrder[catalog].reserve(total);
        for (std::size_t position = 1; position <= total; ++position) {
            std::size_t best = 27;
            std::int64_t bestDeficit = std::numeric_limits<std::int64_t>::min();
            for (std::size_t stratum = 0; stratum < 27; ++stratum) {
                if (selected[stratum] == totals[stratum]) continue;
                const std::int64_t deficit = static_cast<std::int64_t>(
                    position * totals[stratum] - selected[stratum] * total);
                if (best == 27 || deficit > bestDeficit) {
                    best = stratum;
                    bestDeficit = deficit;
                }
            }
            if (best == 27) throw std::runtime_error("training stratum ordering stalled");
            catalogOrder[catalog].push_back(
                buckets[catalog][best][selected[best]++].record);
        }
    }

    std::vector<const SourceRecord *> trainingOrder;
    trainingOrder.reserve(4000);
    std::array<std::size_t, 3> consumed{};
    std::array<std::size_t, 3> previous{};
    for (std::size_t milestone = 0; milestone < milestones.size(); ++milestone) {
        std::array<std::size_t, 3> target{};
        std::size_t segmentSize = 0;
        for (std::size_t catalog = 0; catalog < target.size(); ++catalog) {
            target[catalog] = cumulative[milestone][catalog] - previous[catalog];
            segmentSize += target[catalog];
        }
        std::array<std::size_t, 3> emitted{};
        for (std::size_t position = 1; position <= segmentSize; ++position) {
            std::size_t best = target.size();
            std::int64_t bestDeficit = std::numeric_limits<std::int64_t>::min();
            for (std::size_t catalog = 0; catalog < target.size(); ++catalog) {
                if (emitted[catalog] == target[catalog]) continue;
                const std::int64_t deficit = static_cast<std::int64_t>(
                    position * target[catalog] - emitted[catalog] * segmentSize);
                if (best == target.size() || deficit > bestDeficit) {
                    best = catalog;
                    bestDeficit = deficit;
                }
            }
            if (best == target.size()
                || consumed[best] >= catalogOrder[best].size()) {
                throw std::runtime_error("training catalog ordering stalled");
            }
            trainingOrder.push_back(catalogOrder[best][consumed[best]++]);
            ++emitted[best];
        }
        previous = cumulative[milestone];
        if (trainingOrder.size() != milestones[milestone]) {
            throw std::runtime_error("training milestone size is inconsistent");
        }
    }

    std::set<std::string> identities;
    std::vector<SourceRecord> ordered;
    ordered.reserve(records.size());
    for (const SourceRecord *record : trainingOrder) {
        if (!identities.insert(record->sourceId).second) {
            throw std::runtime_error("training order contains a duplicate source");
        }
        ordered.push_back(*record);
    }
    for (const SourceRecord &record : records) {
        if (record.split != CorpusSplit::TRAIN) ordered.push_back(record);
    }
    if (ordered.size() != records.size()) {
        throw std::runtime_error("training order lost source records");
    }

    const std::string temporaryManifest = outputManifest + ".tmp-order";
    const std::string temporaryOrder = orderManifest + ".tmp";
    try {
        writeSourceManifestV2(ordered, temporaryManifest, true);
        const std::string outputDigest = hex(sha256File(temporaryManifest));
        std::ostringstream report;
        report.imbue(std::locale::classic());
        report << std::fixed << std::setprecision(10)
            << "{\n  \"format\": \"rawtherapee-tgmr-training-order-v1\",\n"
            << "  \"input_manifest_sha256\": \"" << hex(sha256File(inputManifest))
            << "\",\n  \"milestones\": [\n";
        for (std::size_t index = 0; index < milestones.size(); ++index) {
            report << "    {\"catalog_counts\": {\"openimages-cvdf-v5-boxable\": "
                << cumulative[index][0] << ", \"smithsonian-open-access\": "
                << cumulative[index][2] << ", \"wikimedia-commons\": "
                << cumulative[index][1] << "}, \"sources\": " << milestones[index]
                << '}' << (index + 1 == milestones.size() ? "\n" : ",\n");
        }
        report << "  ],\n  \"ordered_source_ids\": [\n";
        for (std::size_t index = 0; index < trainingOrder.size(); ++index) {
            report << "    " << jsonString(trainingOrder[index]->sourceId)
                << (index + 1 == trainingOrder.size() ? "\n" : ",\n");
        }
        report << "  ],\n  \"output_manifest_sha256\": \"" << outputDigest
            << "\",\n  \"strata\": {\n"
            << "    \"chroma\": [" << chromaThresholds[0] << ", "
            << chromaThresholds[1] << "],\n"
            << "    \"luminance\": [" << luminanceThresholds[0] << ", "
            << luminanceThresholds[1] << "],\n"
            << "    \"texture_gradient_rms\": [" << textureThresholds[0] << ", "
            << textureThresholds[1] << "]\n  }\n}\n";
        {
            std::ofstream output(temporaryOrder, std::ios::binary);
            output << report.str();
            output.flush();
            if (!output) throw std::runtime_error("cannot write training-order manifest");
        }
        if (force) {
            std::filesystem::remove(orderManifest);
            std::filesystem::remove(outputManifest);
        }
        // The source manifest is the completion marker: a consumer never sees
        // it before the order record it is bound to.
        std::filesystem::rename(temporaryOrder, orderManifest);
        std::filesystem::rename(temporaryManifest, outputManifest);
    } catch (...) {
        std::filesystem::remove(temporaryOrder);
        std::filesystem::remove(temporaryManifest);
        throw;
    }
}

std::string canonicalAttributionNotice(const std::vector<SourceRecord> &records)
{
    validateProductionManifest(records);
    auto lineField = [](std::string value) {
        for (char &byte : value) {
            if (static_cast<unsigned char>(byte) < 0x20U) byte = ' ';
        }
        return value;
    };
    std::ostringstream output;
    output << "RawTherapee TGMR Corpus v1 attribution notice\n"
           << "=============================================\n\n"
           << "This corpus contains derived 7x7 linear-RGB training patches. "
              "The original photographs are not redistributed.\n"
           << "Each source remains subject to the license identified below.\n\n";
    for (const SourceRecord &record : records) {
        if (!record.selected) continue;
        output << "Source-ID: " << lineField(record.sourceId) << '\n'
               << "Title: " << lineField(record.title) << '\n'
               << "Author: " << lineField(record.author) << '\n'
               << "Source: " << lineField(record.landingPage) << '\n'
               << "License: " << lineField(record.license) << " ("
               << lineField(record.licenseUrl) << ")\n\n";
    }
    return output.str();
}

std::string canonicalRightsReportJson(const std::vector<SourceRecord> &records)
{
    validateProductionManifest(records);
    std::map<std::string, std::uint64_t> catalogs;
    std::map<std::string, std::uint64_t> licenses;
    std::uint64_t people = 0;
    for (const SourceRecord &record : records) {
        if (!record.selected) continue;
        ++catalogs[record.catalogName];
        ++licenses[record.license];
        if (std::find(record.contentTags.begin(), record.contentTags.end(), "people")
            != record.contentTags.end()) {
            ++people;
        }
    }
    std::ostringstream output;
    output << "{\n  \"catalogs\": {";
    bool first = true;
    for (const auto &entry : catalogs) {
        if (!first) output << ',';
        output << "\n    " << jsonString(entry.first) << ": " << entry.second;
        first = false;
    }
    output << "\n  },\n  \"format\": \"rawtherapee-tgmr-rights-report-v1\",\n"
           << "  \"licenses\": {";
    first = true;
    for (const auto &entry : licenses) {
        if (!first) output << ',';
        output << "\n    " << jsonString(entry.first) << ": " << entry.second;
        first = false;
    }
    output << "\n  },\n  \"people_sources\": " << people
           << ",\n  \"sources\": [\n";
    bool firstSource = true;
    for (const SourceRecord &record : records) {
        if (!record.selected) continue;
        if (!firstSource) output << ",\n";
        firstSource = false;
        output << "    {\"author\":" << jsonString(record.author)
               << ",\"author_id\":" << jsonString(record.authorId)
               << ",\"catalog\":" << jsonString(record.catalogName)
               << ",\"evidence_revision\":" << jsonString(record.rightsEvidenceRevision)
               << ",\"evidence_sha256\":" << jsonString(record.rightsEvidenceSha256)
               << ",\"evidence_url\":" << jsonString(record.rightsEvidenceUrl)
               << ",\"landing_page\":" << jsonString(record.landingPage)
               << ",\"license\":" << jsonString(record.license)
               << ",\"license_url\":" << jsonString(record.licenseUrl)
               << ",\"people_review_status\":" << jsonString(record.peopleReviewStatus)
               << ",\"rights_review_status\":" << jsonString(record.rightsReviewStatus)
               << ",\"source_id\":" << jsonString(record.sourceId) << '}';
    }
    output << "\n  ],\n  \"total_sources\": "
           << std::accumulate(catalogs.begin(), catalogs.end(), std::uint64_t{0},
                [](std::uint64_t sum, const auto &entry) { return sum + entry.second; })
           << "\n}\n";
    return output.str();
}

std::string reconstructionListTsv(const std::vector<SourceRecord> &records)
{
    validateProductionManifest(records);
    auto safe = [](const std::string &value) {
        if (value.find_first_of("\t\r\n") != std::string::npos) {
            throw std::runtime_error("reconstruction list field contains control whitespace");
        }
        return value;
    };
    std::ostringstream output;
    output << "source_id\tsource_sha256\tcache_filename\tkind\turl\ttransport_sha256"
              "\tarchive_member\tmember_sha256\n";
    for (const SourceRecord &record : records) {
        if (!record.selected) continue;
        auto line = [&](const char *kind, const std::string &url,
                        const std::string &transportSha256,
                        const std::string &member, const std::string &memberSha256) {
            output << safe(record.sourceId) << '\t' << safe(record.sha256) << '\t'
                   << safe(record.cacheFilename) << '\t' << kind << '\t' << safe(url)
                   << '\t' << safe(transportSha256) << '\t' << safe(member) << '\t'
                   << safe(memberSha256) << '\n';
        };
        line("original", record.originalUrl, record.sha256, "", "");
        for (const std::string &url : record.fallbackUrls) {
            line("fallback", url, record.sha256, "", "");
        }
        for (const ArchiveFallback &archive : record.archiveFallbacks) {
            line("archive", archive.url, archive.sha256,
                 archive.member, archive.memberSha256);
        }
    }
    return output.str();
}

std::string canonicalSourceReportJson(const std::vector<SourceRecord> &records)
{
    std::map<std::string, std::uint64_t> catalogs;
    std::map<std::string, std::set<std::pair<std::string, std::string>>> catalogIdentities;
    std::map<std::string, std::uint64_t> licenses;
    std::map<std::string, std::uint64_t> rights;
    std::map<std::string, std::uint64_t> splits;
    std::map<std::string, std::uint64_t> tags;
    std::array<std::uint64_t, 16> luminance{};
    std::array<std::uint64_t, 12> hue{};
    std::array<std::uint64_t, 8> saturation{};
    std::uint64_t selected = 0;
    std::uint64_t qualityEligible = 0;
    std::uint64_t people = 0;
    for (const auto &record : records) {
        ++catalogs[record.catalogName.empty() ? "legacy-v1" : record.catalogName];
        if (record.manifestV2) {
            catalogIdentities[record.catalogName].emplace(
                record.catalogRevision, record.catalogSnapshotSha256);
        }
        ++licenses[record.license];
        ++rights[record.rightsReviewStatus.empty() ? "legacy-v1" : record.rightsReviewStatus];
        ++splits[record.splitAssigned ? splitName(record.split) : "unassigned"];
        selected += record.selected;
        qualityEligible += std::min(record.width, record.height) >= 512
            && static_cast<std::uint64_t>(record.width) * record.height >= 750000ULL;
        for (const auto &tag : record.contentTags) {
            ++tags[tag];
            people += tag == "people";
        }
        for (std::size_t index = 0; index < luminance.size(); ++index) {
            luminance[index] += record.classification.luminanceHistogram[index];
        }
        for (std::size_t index = 0; index < hue.size(); ++index) {
            hue[index] += record.classification.hueHistogram[index];
        }
        for (std::size_t index = 0; index < saturation.size(); ++index) {
            saturation[index] += record.classification.saturationHistogram[index];
        }
    }
    auto object = [](const std::map<std::string, std::uint64_t> &values) {
        std::ostringstream output;
        output << '{';
        bool first = true;
        for (const auto &entry : values) {
            if (!first) output << ',';
            first = false;
            output << '\n' << "      " << jsonString(entry.first) << ": " << entry.second;
        }
        if (!values.empty()) output << '\n' << "    ";
        output << '}';
        return output.str();
    };
    auto array = [](const auto &values) {
        std::ostringstream output;
        output << '[';
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index) output << ", ";
            output << values[index];
        }
        output << ']';
        return output.str();
    };
    std::ostringstream output;
    output << "{\n"
        << "  \"catalog_counts\": " << object(catalogs) << ",\n"
        << "  \"catalog_identities\": {";
    bool firstCatalog = true;
    for (const auto &catalog : catalogIdentities) {
        if (!firstCatalog) output << ',';
        firstCatalog = false;
        output << "\n    " << jsonString(catalog.first) << ": [";
        bool firstIdentity = true;
        for (const auto &identity : catalog.second) {
            if (!firstIdentity) output << ',';
            firstIdentity = false;
            output << "{\"revision\": " << jsonString(identity.first)
                << ", \"snapshot_sha256\": " << jsonString(identity.second) << '}';
        }
        output << ']';
    }
    if (!catalogIdentities.empty()) output << '\n';
    output << "  },\n"
        << "  \"format\": \"rawtherapee-tgmr-source-statistics-v1\",\n"
        << "  \"hue_histogram\": " << array(hue) << ",\n"
        << "  \"license_counts\": " << object(licenses) << ",\n"
        << "  \"luminance_histogram\": " << array(luminance) << ",\n"
        << "  \"people_sources\": " << people << ",\n"
        << "  \"quality_eligible_sources\": " << qualityEligible << ",\n"
        << "  \"rights_review_counts\": " << object(rights) << ",\n"
        << "  \"saturation_histogram\": " << array(saturation) << ",\n"
        << "  \"selected_sources\": " << selected << ",\n"
        << "  \"source_count\": " << records.size() << ",\n"
        << "  \"split_counts\": " << object(splits) << ",\n"
        << "  \"tag_counts\": " << object(tags) << "\n}\n";
    return output.str();
}

std::string sourceReportCsv(const std::vector<SourceRecord> &records)
{
    std::map<std::pair<std::string, std::string>, std::uint64_t> counts;
    for (const auto &record : records) {
        ++counts[{"catalog", record.catalogName.empty() ? "legacy-v1" : record.catalogName}];
        if (record.manifestV2) {
            ++counts[{"catalog_snapshot", record.catalogName + '@'
                + record.catalogRevision + '@' + record.catalogSnapshotSha256}];
        }
        ++counts[{"license", record.license}];
        ++counts[{"rights", record.rightsReviewStatus.empty()
            ? "legacy-v1" : record.rightsReviewStatus}];
        ++counts[{"split", record.splitAssigned ? splitName(record.split) : "unassigned"}];
        for (const auto &tag : record.contentTags) ++counts[{"tag", tag}];
    }
    std::ostringstream output;
    output << "category,value,count\n";
    for (const auto &entry : counts) {
        output << entry.first.first << ',' << jsonString(entry.first.second) << ','
            << entry.second << '\n';
    }
    return output.str();
}

std::string sourceReportHtml(const std::vector<SourceRecord> &records)
{
    std::string json = canonicalSourceReportJson(records);
    std::string escaped;
    escaped.reserve(json.size());
    for (char value : json) {
        if (value == '&') escaped += "&amp;";
        else if (value == '<') escaped += "&lt;";
        else if (value == '>') escaped += "&gt;";
        else escaped += value;
    }
    return "<!doctype html>\n<meta charset=\"utf-8\">\n"
        "<title>RawTherapee TGMR source-corpus report</title>\n"
        "<h1>RawTherapee TGMR source-corpus report</h1>\n<pre>" + escaped
        + "</pre>\n";
}

std::string selectProductionSources(
    const std::vector<SourceRecord> &records,
    const std::string &selectionRecipe,
    const std::string &outputManifest,
    bool force)
{
    if (!force && std::filesystem::exists(outputManifest)) {
        throw std::runtime_error("refusing to replace selected source manifest");
    }
    const std::string recipeBytes = readText(selectionRecipe);
    cJSON *recipe = cJSON_Parse(recipeBytes.c_str());
    if (!recipe || !cJSON_IsObject(recipe)) {
        cJSON_Delete(recipe);
        throw std::runtime_error("corpus selection recipe is not a JSON object");
    }
    std::string seed;
    const std::array<std::size_t, SPLITS> astronomyTargets{{12,1,1}};
    try {
        if (requiredText(recipe, "format") != "rawtherapee-tgmr-corpus-selection-v1") {
            throw std::runtime_error("wrong corpus selection recipe format");
        }
        seed = requiredText(recipe, "seed");
        if (exactSize(recipe, "author_image_cap") != 5) {
            throw std::runtime_error("selection recipe changes the frozen author cap");
        }
        const cJSON *quotas = required(recipe, "quotas");
        for (std::size_t catalog = 0; catalog < CATALOGS; ++catalog) {
            const cJSON *entry = required(quotas, CATALOG_NAMES[catalog]);
            for (std::size_t split = 0; split < SPLITS; ++split) {
                if (exactSize(entry, SPLIT_NAMES[split]) != FROZEN_QUOTAS[catalog][split]) {
                    throw std::runtime_error("selection recipe changes a frozen source quota");
                }
            }
        }
        const cJSON *tagMinimums = required(recipe, "content_tag_minimum_sources");
        const cJSON *astronomy = required(tagMinimums, "astronomy-star-field");
        for (std::size_t split = 0; split < SPLITS; ++split) {
            if (exactSize(astronomy, SPLIT_NAMES[split]) != astronomyTargets[split]) {
                throw std::runtime_error(
                    "selection recipe changes the frozen astronomy-star-field minimum");
            }
        }
    } catch (...) {
        cJSON_Delete(recipe);
        throw;
    }
    cJSON_Delete(recipe);

    std::vector<const SourceRecord *> eligible;
    eligible.reserve(records.size());
    for (const auto &record : records) {
        if (suitable(record)) eligible.push_back(&record);
    }
    std::vector<double> brightness;
    std::vector<double> chroma;
    std::vector<double> texture;
    for (const SourceRecord *record : eligible) {
        if (assignedSplit(seed, *record) != CorpusSplit::TRAIN) continue;
        brightness.push_back(record->classification.luminanceMean);
        chroma.push_back(record->classification.chromaRatioMean);
        texture.push_back(record->classification.gradientRms);
    }
    const std::array<double, 2> brightnessCuts{{
        quantile(brightness, 1.0 / 3.0), quantile(brightness, 2.0 / 3.0)}};
    const std::array<double, 2> chromaCuts{{
        quantile(chroma, 1.0 / 3.0), quantile(chroma, 2.0 / 3.0)}};
    const std::array<double, 2> textureCuts{{
        quantile(texture, 1.0 / 3.0), quantile(texture, 2.0 / 3.0)}};

    std::array<std::array<std::vector<Candidate>, SPLITS>, CATALOGS> pools;
    for (const SourceRecord *source : eligible) {
        Candidate value;
        value.record = *source;
        value.record.split = assignedSplit(seed, *source);
        value.record.splitAssigned = true;
        value.catalog = catalogIndex(value.record.catalogName);
        value.split = splitIndex(value.record.split);
        const unsigned light = tertile(value.record.classification.luminanceMean,
                                       brightnessCuts[0], brightnessCuts[1]);
        const unsigned color = tertile(value.record.classification.chromaRatioMean,
                                       chromaCuts[0], chromaCuts[1]);
        const unsigned detail = tertile(value.record.classification.gradientRms,
                                        textureCuts[0], textureCuts[1]);
        value.stratum = light * 9 + color * 3 + detail;
        value.order = stableOrder(seed, value.record.sourceId);
        pools[value.catalog][value.split].push_back(std::move(value));
    }
    for (auto &catalog : pools) {
        for (auto &pool : catalog) {
            std::sort(pool.begin(), pool.end(), [](const Candidate &left, const Candidate &right) {
                return std::tie(left.order, left.record.sourceId)
                    < std::tie(right.order, right.record.sourceId);
            });
        }
    }

    std::vector<SourceRecord> selected;
    selected.reserve(5000);
    std::map<std::string, std::size_t> authorCounts;
    std::set<std::string> sourceIdsSeen;
    std::map<std::string, std::string> bytesSeen;
    std::map<std::string, std::string> pixelsSeen;
    std::map<std::string, std::string> flickrSeen;
    std::vector<std::uint64_t> dHashes;
    std::vector<std::uint64_t> pHashes;
    std::array<std::array<std::size_t, SPLITS>, CATALOGS> counts{};
    std::array<std::size_t, SPLITS> people{};
    std::array<std::size_t, SPLITS> astronomy{};
    std::size_t authorCapRejects = 0;
    struct DuplicateRejection final {
        std::string sourceId;
        std::string conflictingSourceId;
        std::string reason;
        unsigned distance = 0;
    };
    std::vector<DuplicateRejection> duplicateRejects;
    auto accept = [&](const Candidate &candidate) {
        const auto &record = candidate.record;
        if (counts[candidate.catalog][candidate.split]
            >= FROZEN_QUOTAS[candidate.catalog][candidate.split]
            || sourceIdsSeen.find(record.sourceId) != sourceIdsSeen.end()) {
            return false;
        }
        if (authorCounts[record.authorId] >= 5) {
            ++authorCapRejects;
            return false;
        }
        auto exactDuplicate = [&](const std::map<std::string, std::string> &seen,
                                  const std::string &identity,
                                  const char *reason) {
            const auto match = seen.find(identity);
            if (identity.empty() || match == seen.end()) return false;
            duplicateRejects.push_back({record.sourceId, match->second, reason, 0});
            return true;
        };
        if (exactDuplicate(bytesSeen, record.sha256, "compressed-sha256")
            || exactDuplicate(pixelsSeen, record.decodedPixelSha256, "decoded-sha256")
            || exactDuplicate(flickrSeen, record.upstreamFlickrId, "flickr-photo-id")) {
            return false;
        }
        const std::uint64_t dHash = signature(record.perceptualHash);
        const std::uint64_t pHash = signature(record.pHash);
        for (std::size_t index = 0; index < dHashes.size(); ++index) {
            const unsigned dHashDistance = popcount64(dHash ^ dHashes[index]);
            const unsigned pHashDistance = popcount64(pHash ^ pHashes[index]);
            if (dHashDistance <= 5 || pHashDistance <= 8) {
                const bool dHashMatch = dHashDistance <= 5;
                duplicateRejects.push_back({
                    record.sourceId, selected[index].sourceId,
                    dHashMatch ? "dhash" : "phash",
                    dHashMatch ? dHashDistance : pHashDistance,
                });
                return false;
            }
        }
        selected.push_back(record);
        ++counts[candidate.catalog][candidate.split];
        ++authorCounts[record.authorId];
        sourceIdsSeen.insert(record.sourceId);
        bytesSeen.emplace(record.sha256, record.sourceId);
        pixelsSeen.emplace(record.decodedPixelSha256, record.sourceId);
        if (!record.upstreamFlickrId.empty()) {
            flickrSeen.emplace(record.upstreamFlickrId, record.sourceId);
        }
        dHashes.push_back(dHash);
        pHashes.push_back(pHash);
        if (hasTag(record, "people")) ++people[candidate.split];
        if (hasTag(record, "astronomy-star-field")) ++astronomy[candidate.split];
        return true;
    };

    // Sparse stellar detail is a known demosaicing safety class and cannot be
    // represented by the generic low-light population alone. Establish its
    // source-held-out guardrail before general stratum filling.
    for (std::size_t split = 0; split < SPLITS; ++split) {
        bool progress = true;
        while (astronomy[split] < astronomyTargets[split] && progress) {
            progress = false;
            for (std::size_t catalog = 0; catalog < CATALOGS
                 && astronomy[split] < astronomyTargets[split]; ++catalog) {
                for (const Candidate &candidate : pools[catalog][split]) {
                    if (hasTag(candidate.record, "astronomy-star-field")
                        && accept(candidate)) {
                        progress = true;
                        break;
                    }
                }
            }
        }
        if (astronomy[split] < astronomyTargets[split]) {
            throw std::runtime_error(
                "candidate pool cannot satisfy astronomy-star-field minimum");
        }
    }

    // Establish the controlled 15% people population before general filling.
    const std::array<std::size_t, SPLITS> peopleTargets{{600,75,75}};
    for (std::size_t split = 0; split < SPLITS; ++split) {
        bool progress = true;
        while (people[split] < peopleTargets[split] && progress) {
            progress = false;
            for (std::size_t catalog = 0; catalog < CATALOGS
                 && people[split] < peopleTargets[split]; ++catalog) {
                for (const Candidate &candidate : pools[catalog][split]) {
                    if (hasTag(candidate.record, "people") && accept(candidate)) {
                        progress = true;
                        break;
                    }
                }
            }
        }
        if (people[split] < peopleTargets[split]) {
            throw std::runtime_error("candidate pool cannot satisfy controlled people quota");
        }
    }

    // Round-robin the 27 brightness/chroma/texture cells for each exact
    // catalog/split quota.  Once the controlled people target is met, prefer
    // non-people records so the final share stays near rather than far above it.
    for (std::size_t catalog = 0; catalog < CATALOGS; ++catalog) {
        for (std::size_t split = 0; split < SPLITS; ++split) {
            std::array<std::size_t, 27> positions{};
            std::array<std::vector<const Candidate *>, 27> strata;
            for (const Candidate &candidate : pools[catalog][split]) {
                strata[candidate.stratum].push_back(&candidate);
            }
            bool progress = true;
            while (counts[catalog][split] < FROZEN_QUOTAS[catalog][split] && progress) {
                progress = false;
                for (unsigned stratum = 0; stratum < strata.size()
                     && counts[catalog][split] < FROZEN_QUOTAS[catalog][split]; ++stratum) {
                    auto &position = positions[stratum];
                    while (position < strata[stratum].size()) {
                        const Candidate &candidate = *strata[stratum][position++];
                        if (hasTag(candidate.record, "people")
                            && people[split] >= peopleTargets[split]) {
                            continue;
                        }
                        if (accept(candidate)) {
                            progress = true;
                            break;
                        }
                    }
                }
            }
            // If non-people preference left a deficit, use any remaining
            // reviewed candidate while preserving all safety constraints.
            if (counts[catalog][split] < FROZEN_QUOTAS[catalog][split]) {
                for (const Candidate &candidate : pools[catalog][split]) {
                    if (counts[catalog][split] >= FROZEN_QUOTAS[catalog][split]) break;
                    (void)accept(candidate);
                }
            }
            if (counts[catalog][split] != FROZEN_QUOTAS[catalog][split]) {
                throw std::runtime_error("candidate pool cannot satisfy exact source quotas; expand it");
            }
        }
    }

    std::sort(selected.begin(), selected.end(), [](const SourceRecord &left,
                                                    const SourceRecord &right) {
        return std::tie(left.split, left.catalogName, left.sourceId)
            < std::tie(right.split, right.catalogName, right.sourceId);
    });
    for (SourceRecord &record : selected) {
        record.selected = true;
        record.selectionStatus = "accepted-corpus-v1";
    }
    writeSourceManifestV2(selected, outputManifest, force);

    std::ostringstream report;
    report.imbue(std::locale::classic());
    report << std::fixed << std::setprecision(10)
        << "{\n  \"author_cap_rejections\": " << authorCapRejects
        << ",\n  \"brightness_tertiles\": [" << brightnessCuts[0] << ", "
        << brightnessCuts[1] << "],\n  \"chroma_tertiles\": [" << chromaCuts[0]
        << ", " << chromaCuts[1] << "],\n  \"deduplication_rejections\": [";
    for (std::size_t index = 0; index < duplicateRejects.size(); ++index) {
        if (index) report << ',';
        const auto &rejection = duplicateRejects[index];
        report << "\n    {\"conflicting_source_id\": "
            << jsonString(rejection.conflictingSourceId)
            << ", \"distance\": " << rejection.distance
            << ", \"reason\": " << jsonString(rejection.reason)
            << ", \"source_id\": " << jsonString(rejection.sourceId) << '}';
    }
    if (!duplicateRejects.empty()) report << '\n';
    report << "  ],\n  \"eligible_candidates\": " << eligible.size()
        << ",\n  \"format\": \"rawtherapee-tgmr-corpus-selection-report-v1\",\n"
        << "  \"astronomy_star_field\": {\"test\": " << astronomy[2]
        << ", \"train\": " << astronomy[0] << ", \"validation\": "
        << astronomy[1] << "},\n"
        << "  \"people\": {\"test\": " << people[2] << ", \"train\": "
        << people[0] << ", \"validation\": " << people[1] << "},\n"
        << "  \"recipe_sha256\": \""
        << hex(sha256(recipeBytes.data(), recipeBytes.size())) << "\",\n"
        << "  \"selected_sources\": " << selected.size()
        << ",\n  \"texture_tertiles\": [" << textureCuts[0] << ", "
        << textureCuts[1] << "]\n}\n";
    return report.str();
}

} // namespace tgmr
