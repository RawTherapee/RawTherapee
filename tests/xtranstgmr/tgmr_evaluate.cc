/*
 * Development-only licensed-corpus evaluator for the X-Trans TGMR model.
 *
 * TGMR's compact 7x7 patch corpus is sufficient for direct model validation,
 * but not for a meaningful Markesteijn comparison.  This tool therefore
 * rebuilds a padded camera-linear source crop around every frozen coordinate,
 * applies the exact held-out TGPC augmentation, and runs RawTherapee's real
 * three-pass Markesteijn implementation on that crop.  It first verifies a
 * deterministic crop center against a full-image run so a boundary shortcut
 * cannot silently bias the comparison.
 */

#include "xtrans_markesteijn.h"
#include "xtrans_tgmr.h"

#include "tgmr/camera_matrices.h"
#include "tgmr/corpus.h"
#include "tgmr/corpus_analysis.h"
#include "tgmr/image.h"
#include "tgmr/manifest.h"
#include "tgmr/model_v2.h"
#include "tgmr/sha256.h"
#include "tgmr/xtrans_training.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

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
constexpr std::size_t AREA = 49;

int modulo(int value, int divisor)
{
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

const char *splitName(tgmr::CorpusSplit split)
{
    switch (split) {
        case tgmr::CorpusSplit::TRAIN: return "train";
        case tgmr::CorpusSplit::VALIDATION: return "validation";
        case tgmr::CorpusSplit::TEST: return "test";
    }
    return "unknown";
}

std::vector<std::uint8_t> readFile(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open model: " + path);
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (size < 0 || size > static_cast<std::streamoff>(64 * 1024 * 1024)) {
        throw std::runtime_error("model size is invalid");
    }
    std::vector<std::uint8_t> output(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char *>(output.data()), size);
    if (!stream) throw std::runtime_error("short model read");
    return output;
}

std::filesystem::path sourcePath(
    const std::string &cache,
    const tgmr::SourceRecord &source)
{
    const std::filesystem::path name(source.cacheFilename);
    if (name.empty() || name.is_absolute()) {
        throw std::runtime_error("source cache filename is unsafe");
    }
    for (const auto &part : name) {
        if (part.empty() || part == "." || part == "..") {
            throw std::runtime_error("source cache filename is unsafe");
        }
    }
    return std::filesystem::path(cache) / name;
}

std::uint16_t transformSample(
    const tgmr::LinearImage &image,
    const tgmr::PatchRecord &record,
    std::uint32_t x,
    std::uint32_t y,
    unsigned outputChannel)
{
    const tgmr::CameraMatrix &matrix = tgmr::cameraMatrix(record.matrixId);
    const std::size_t offset =
        (static_cast<std::size_t>(y) * image.width + x) * 3;
    double value = 0.0;
    for (unsigned inputChannel = 0; inputChannel < 3; ++inputChannel) {
        value += matrix.linearSrgbToCamera[outputChannel * 3 + inputChannel]
            * image.rgb[offset + inputChannel];
    }
    value *= std::exp2(record.exposureStopsQ8 / 256.0);
    value *= record.whiteBalanceQ12[outputChannel] / 4096.0;
    value = std::max(0.0, std::min(1.0, value));
    return static_cast<std::uint16_t>(std::llround(value * 65535.0));
}

struct Crop final {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    int targetX = 0;
    int targetY = 0;
    int cfa[6][6]{};
    std::vector<float> mosaic;
};

Crop makeCrop(
    const tgmr::LinearImage &image,
    const tgmr::PatchRecord &record,
    const tgmr::PhaseContract &phase,
    int requestedEdge)
{
    Crop output;
    output.width = std::min<int>(requestedEdge, image.width);
    output.height = std::min<int>(requestedEdge, image.height);
    if (output.width < 32 || output.height < 32) {
        throw std::runtime_error("source is too small for Markesteijn evaluation");
    }
    const int sourceX = static_cast<int>(record.x) + 3;
    const int sourceY = static_cast<int>(record.y) + 3;
    output.left = std::max(0, std::min<int>(
        sourceX - output.width / 2, image.width - output.width));
    output.top = std::max(0, std::min<int>(
        sourceY - output.height / 2, image.height - output.height));
    output.targetX = sourceX - output.left;
    output.targetY = sourceY - output.top;
    const int shiftX = modulo(
        static_cast<int>(phase.originX) - (static_cast<int>(record.x) - output.left), 6);
    const int shiftY = modulo(
        static_cast<int>(phase.originY) - (static_cast<int>(record.y) - output.top), 6);
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            output.cfa[y][x] = CFA[modulo(y + shiftY, 6)][modulo(x + shiftX, 6)];
        }
    }
    output.mosaic.resize(static_cast<std::size_t>(output.width) * output.height);
    for (int y = 0; y < output.height; ++y) {
        for (int x = 0; x < output.width; ++x) {
            const unsigned channel = output.cfa[y % 6][x % 6];
            output.mosaic[static_cast<std::size_t>(y) * output.width + x] =
                transformSample(image, record, output.left + x, output.top + y, channel);
        }
    }
    return output;
}

struct Prediction final {
    std::array<double, 3> truth{};
    std::array<double, 3> tgmr{};
    std::array<double, 3> markesteijn{};
};

Prediction predict(
    const tgmr::LinearImage &image,
    const tgmr::PatchRecord &record,
    const tgmr::PhaseContract &phase,
    const std::shared_ptr<const rtengine::TgmrXTransModel> &model,
    int cropEdge)
{
    Prediction result;
    std::array<float, AREA> tgmrMosaic{};
    for (std::size_t position = 0; position < AREA; ++position) {
        tgmrMosaic[position] = record.rgb[phase.observedIndices[position]];
    }
    std::array<float, AREA> tgmrRed{};
    std::array<float, AREA> tgmrGreen{};
    std::array<float, AREA> tgmrBlue{};
    const auto tgmrRun = rtengine::demosaicTgmrXTransReference(
        tgmrMosaic.data(), tgmrRed.data(), tgmrGreen.data(), tgmrBlue.data(),
        7, 7, CFA, model, phase.originX, phase.originY, false);
    if (!tgmrRun) {
        throw std::runtime_error(std::string("TGMR failed [")
            + rtengine::tgmrXTransErrorCodeName(tgmrRun.code) + "]: " + tgmrRun.message);
    }

    const Crop crop = makeCrop(image, record, phase, cropEdge);
    const std::size_t pixels = static_cast<std::size_t>(crop.width) * crop.height;
    std::vector<float> red(pixels), green(pixels), blue(pixels);
    const auto markRun = rtengine::demosaicMarkesteijnXTransReference(
        crop.mosaic.data(), red.data(), green.data(), blue.data(),
        crop.width, crop.height, crop.cfa);
    if (!markRun) {
        throw std::runtime_error(std::string("Markesteijn failed [")
            + rtengine::markesteijnXTransErrorCodeName(markRun.code) + "]: "
            + markRun.message);
    }
    const std::size_t center =
        static_cast<std::size_t>(crop.targetY) * crop.width + crop.targetX;
    const std::array<const std::array<float, AREA> *, 3> tgmrPlanes =
        {{&tgmrRed, &tgmrGreen, &tgmrBlue}};
    const std::array<const std::vector<float> *, 3> markPlanes =
        {{&red, &green, &blue}};
    for (unsigned channel = 0; channel < 3; ++channel) {
        result.truth[channel] = record.rgb[channel * AREA + AREA / 2] / 65535.0;
        result.tgmr[channel] = (*tgmrPlanes[channel])[AREA / 2] / 65535.0;
        result.markesteijn[channel] = (*markPlanes[channel])[center] / 65535.0;
        if (!std::isfinite(result.tgmr[channel])
            || !std::isfinite(result.markesteijn[channel])) {
            throw std::runtime_error("demosaic evaluation produced a non-finite value");
        }
    }
    const unsigned sampled = phase.sampledCenterChannel;
    if (result.tgmr[sampled] != result.truth[sampled]
        || result.markesteijn[sampled] != result.truth[sampled]) {
        throw std::runtime_error("demosaicer changed the native center sample");
    }
    return result;
}

double psnr(double mse)
{
    return mse == 0.0 ? std::numeric_limits<double>::infinity()
                      : -10.0 * std::log10(mse);
}

struct Metrics final {
    struct Cell final {
        double squared = 0.0;
        std::uint64_t patches = 0;
        std::uint64_t values = 0;
    };
    double squared = 0.0;
    std::uint64_t values = 0;
    std::vector<double> patchRms;
    std::map<std::uint32_t, std::pair<double, std::uint64_t>> sources;
    std::array<std::array<Cell, 3>, 3> strata{};
};

void add(
    Metrics &metrics,
    std::uint32_t source,
    const tgmr::PatchRecord &record,
    const std::array<double, 3> &truth,
    const std::array<double, 3> &value)
{
    double squared = 0.0;
    for (unsigned channel = 0; channel < 3; ++channel) {
        const double difference = value[channel] - truth[channel];
        squared += difference * difference;
    }
    metrics.squared += squared;
    metrics.values += 3;
    metrics.patchRms.push_back(std::sqrt(squared / 3.0));
    metrics.sources[source].first += squared;
    metrics.sources[source].second += 3;
    const tgmr::PatchSignalStrata strata = tgmr::fixedCorpusV1PatchStrata(record);
    const std::array<unsigned, 3> levels{{
        strata.brightness, strata.chroma, strata.texture}};
    for (unsigned metric = 0; metric < levels.size(); ++metric) {
        Metrics::Cell &cell = metrics.strata[metric][levels[metric]];
        cell.squared += squared;
        ++cell.patches;
        cell.values += 3;
    }
}

struct Summary final {
    double mse = 0.0;
    double psnrValue = 0.0;
    double p99 = 0.0;
    double worst = 0.0;
    double medianSourcePsnr = 0.0;
    std::array<std::array<Metrics::Cell, 3>, 3> strata{};
};

Summary summarize(Metrics metrics)
{
    if (metrics.values == 0 || metrics.patchRms.empty() || metrics.sources.empty()) {
        throw std::runtime_error("evaluation contains no samples");
    }
    Summary output;
    output.mse = metrics.squared / metrics.values;
    output.psnrValue = psnr(output.mse);
    std::sort(metrics.patchRms.begin(), metrics.patchRms.end());
    const std::size_t p99 = std::min(metrics.patchRms.size() - 1,
        static_cast<std::size_t>(std::floor(metrics.patchRms.size() * 0.99)));
    output.p99 = metrics.patchRms[p99];
    output.worst = metrics.patchRms.back();
    std::vector<double> sources;
    for (const auto &entry : metrics.sources) {
        sources.push_back(psnr(entry.second.first / entry.second.second));
    }
    std::sort(sources.begin(), sources.end());
    output.medianSourcePsnr = sources[sources.size() / 2];
    output.strata = metrics.strata;
    return output;
}

void emitSummary(std::ostream &output, const Summary &summary)
{
    output << "{\"median_source_psnr\": " << summary.medianSourcePsnr
        << ", \"mse\": " << summary.mse
        << ", \"patch_rms_p99\": " << summary.p99
        << ", \"psnr\": " << summary.psnrValue
        << ", \"strata\": {";
    static const char *metricNames[] = {"brightness", "chroma", "texture"};
    static const char *levelNames[] = {"low", "middle", "high"};
    for (unsigned metric = 0; metric < summary.strata.size(); ++metric) {
        if (metric) output << ", ";
        output << '\"' << metricNames[metric] << "\": [";
        for (unsigned level = 0; level < summary.strata[metric].size(); ++level) {
            if (level) output << ", ";
            const Metrics::Cell &cell = summary.strata[metric][level];
            const double mse = cell.values == 0 ? 0.0 : cell.squared / cell.values;
            output << "{\"level\": \"" << levelNames[level]
                << "\", \"mse\": " << mse
                << ", \"patches\": " << cell.patches
                << ", \"psnr\": " << (cell.values == 0 ? 0.0 : psnr(mse)) << '}';
        }
        output << ']';
    }
    output << "}, \"worst_patch_rms\": " << summary.worst << '}';
}

double cropFullParity(
    const tgmr::LinearImage &image,
    const tgmr::PatchRecord &record,
    const tgmr::PhaseContract &phase,
    int cropEdge)
{
    const Crop crop = makeCrop(image, record, phase, cropEdge);
    const int fullShiftX = modulo(
        static_cast<int>(phase.originX) - static_cast<int>(record.x), 6);
    const int fullShiftY = modulo(
        static_cast<int>(phase.originY) - static_cast<int>(record.y), 6);
    int fullCfa[6][6];
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            fullCfa[y][x] = CFA[modulo(y + fullShiftY, 6)][modulo(x + fullShiftX, 6)];
        }
    }
    const std::size_t fullPixels = static_cast<std::size_t>(image.width) * image.height;
    std::vector<float> fullMosaic(fullPixels);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const unsigned channel = fullCfa[y % 6][x % 6];
            fullMosaic[static_cast<std::size_t>(y) * image.width + x] =
                transformSample(image, record, x, y, channel);
        }
    }
    std::vector<float> fullRed(fullPixels), fullGreen(fullPixels), fullBlue(fullPixels);
    const auto fullRun = rtengine::demosaicMarkesteijnXTransReference(
        fullMosaic.data(), fullRed.data(), fullGreen.data(), fullBlue.data(),
        image.width, image.height, fullCfa);
    if (!fullRun) throw std::runtime_error("full-image Markesteijn parity run failed");
    const std::size_t cropPixels = static_cast<std::size_t>(crop.width) * crop.height;
    std::vector<float> cropRed(cropPixels), cropGreen(cropPixels), cropBlue(cropPixels);
    const auto cropRun = rtengine::demosaicMarkesteijnXTransReference(
        crop.mosaic.data(), cropRed.data(), cropGreen.data(), cropBlue.data(),
        crop.width, crop.height, crop.cfa);
    if (!cropRun) throw std::runtime_error("cropped Markesteijn parity run failed");
    const std::size_t fullCenter =
        (static_cast<std::size_t>(record.y) + 3) * image.width + record.x + 3;
    const std::size_t cropCenter =
        static_cast<std::size_t>(crop.targetY) * crop.width + crop.targetX;
    const std::array<const std::vector<float> *, 3> full =
        {{&fullRed, &fullGreen, &fullBlue}};
    const std::array<const std::vector<float> *, 3> cropped =
        {{&cropRed, &cropGreen, &cropBlue}};
    double maximum = 0.0;
    for (unsigned channel = 0; channel < 3; ++channel) {
        maximum = std::max(maximum, std::abs(
            ((*full[channel])[fullCenter] - (*cropped[channel])[cropCenter]) / 65535.0));
    }
    return maximum;
}

struct Options final {
    tgmr::CorpusSplit split = tgmr::CorpusSplit::TEST;
    std::uint64_t limit = 0;
    int cropEdge = 96;
    int jobs = 4;
};

Options parseOptions(int argc, char **argv)
{
    Options options;
    for (int index = 5; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--limit" && index + 1 < argc) {
            options.limit = std::stoull(argv[++index]);
        } else if (option == "--jobs" && index + 1 < argc) {
            options.jobs = std::stoi(argv[++index]);
            if (options.jobs < 1 || options.jobs > 64) {
                throw std::runtime_error("--jobs must be between 1 and 64");
            }
        } else if (option == "--crop-edge" && index + 1 < argc) {
            options.cropEdge = std::stoi(argv[++index]);
            if (options.cropEdge < 48 || options.cropEdge > 512) {
                throw std::runtime_error("--crop-edge must be between 48 and 512");
            }
        } else if (option == "--split" && index + 1 < argc) {
            const std::string split = argv[++index];
            if (split == "validation") options.split = tgmr::CorpusSplit::VALIDATION;
            else if (split == "test") options.split = tgmr::CorpusSplit::TEST;
            else throw std::runtime_error("--split must be validation or test");
        } else {
            throw std::runtime_error("unknown evaluator option: " + option);
        }
    }
    return options;
}

int run(int argc, char **argv)
{
    if (argc < 5) {
        std::cerr << "Usage: rawtherapee-xtrans-tgmr-evaluate MODEL CORPUS "
                     "SOURCE-MANIFEST CACHE [--split test|validation] "
                     "[--limit N] [--crop-edge N] [--jobs N]\n";
        return 2;
    }
    const Options options = parseOptions(argc, argv);
    const std::vector<std::uint8_t> modelBytes = readFile(argv[1]);
    const tgmr::ModelV2Inspection modelInspection = tgmr::inspectModelV2(modelBytes);
    const auto load = rtengine::loadTgmrXTransModel(argv[1]);
    if (!load) {
        throw std::runtime_error(std::string("cannot load TGMR model [")
            + rtengine::tgmrXTransErrorCodeName(load.code) + "]: " + load.message);
    }
    const std::vector<tgmr::SourceRecord> manifest = tgmr::readSourceManifest(argv[3]);
    tgmr::validateProductionManifest(manifest);
    std::vector<const tgmr::SourceRecord *> selected;
    for (const auto &source : manifest) if (source.selected) selected.push_back(&source);

    std::vector<tgmr::PatchRecord> records;
    const tgmr::CorpusInspection corpus = tgmr::inspectCorpus(argv[2],
        [&](const tgmr::PatchRecord &record, std::uint64_t) {
            if (record.split == options.split
                && (options.limit == 0 || records.size() < options.limit)) {
                records.push_back(record);
            }
        });
    if (corpus.header.manifestSha256 != tgmr::sha256File(argv[3])) {
        throw std::runtime_error("TGPC is not bound to the supplied source manifest");
    }
    if (modelInspection.identity.corpusSha256 != corpus.header.payloadSha256) {
        throw std::runtime_error("model is not bound to the supplied TGPC payload");
    }
    if (records.empty()) throw std::runtime_error("selected split contains no records");
    for (const auto &record : records) {
        if (record.sourceOrdinal >= selected.size()) {
            throw std::runtime_error("TGPC source ordinal exceeds the source manifest");
        }
        if (record.sourceIdSha256 != tgmr::sha256(
                selected[record.sourceOrdinal]->sourceId.data(),
                selected[record.sourceOrdinal]->sourceId.size())) {
            throw std::runtime_error("TGPC source identity differs from the source manifest");
        }
    }

    const auto phases = tgmr::xtransPhaseContracts();
    std::size_t parityRecord = 0;
    std::uint64_t parityArea = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto &source = *selected[records[index].sourceOrdinal];
        const std::uint64_t area = static_cast<std::uint64_t>(source.width) * source.height;
        if (area < parityArea) {
            parityArea = area;
            parityRecord = index;
        }
    }
    const auto &paritySource = *selected[records[parityRecord].sourceOrdinal];
    const auto parityPath = sourcePath(argv[4], paritySource);
    if (tgmr::hex(tgmr::sha256File(parityPath.string())) != paritySource.sha256) {
        throw std::runtime_error("crop-parity source authentication failed");
    }
    const tgmr::LinearImage parityImage = tgmr::loadLinearImage(parityPath.string());
    const double parityMaximum = cropFullParity(
        parityImage, records[parityRecord], phases[parityRecord % phases.size()],
        options.cropEdge);
    if (parityMaximum > 1e-6) {
        throw std::runtime_error("cropped Markesteijn center differs from full-image result");
    }

    Metrics tgmrMetrics;
    Metrics markMetrics;
    std::array<std::uint64_t, 18> phaseCounts{};
    std::size_t begin = 0;
    while (begin < records.size()) {
        std::size_t end = begin + 1;
        while (end < records.size()
            && records[end].sourceOrdinal == records[begin].sourceOrdinal) ++end;
        const auto &source = *selected[records[begin].sourceOrdinal];
        const auto path = sourcePath(argv[4], source);
        if (tgmr::hex(tgmr::sha256File(path.string())) != source.sha256) {
            throw std::runtime_error("evaluation source authentication failed: "
                                     + source.sourceId);
        }
        const tgmr::LinearImage image = tgmr::loadLinearImage(path.string());
        if (image.width != source.width || image.height != source.height
            || image.orientation != source.orientation) {
            throw std::runtime_error("evaluation source metadata changed: " + source.sourceId);
        }
        std::vector<Prediction> predictions(end - begin);
        std::vector<std::string> errors(end - begin);
#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic) num_threads(options.jobs)
#endif
        for (std::int64_t local = 0;
             local < static_cast<std::int64_t>(end - begin); ++local) {
            const std::size_t index = begin + static_cast<std::size_t>(local);
            const std::size_t phase = index % phases.size();
            try {
                predictions[local] = predict(
                    image, records[index], phases[phase], load.model, options.cropEdge);
            } catch (const std::exception &error) {
                errors[local] = error.what();
            } catch (...) {
                errors[local] = "unknown parallel evaluation failure";
            }
        }
        for (std::size_t local = 0; local < predictions.size(); ++local) {
            if (!errors[local].empty()) throw std::runtime_error(errors[local]);
            const std::size_t index = begin + local;
            const std::size_t phase = index % phases.size();
            const Prediction &value = predictions[local];
            add(tgmrMetrics, records[index].sourceOrdinal, records[index],
                value.truth, value.tgmr);
            add(markMetrics, records[index].sourceOrdinal, records[index],
                value.truth, value.markesteijn);
            ++phaseCounts[phase];
        }
        begin = end;
    }

    const Summary tgmrSummary = summarize(tgmrMetrics);
    const Summary markSummary = summarize(markMetrics);
    std::vector<std::pair<std::uint32_t, double>> sourceDeltas;
    for (const auto &entry : tgmrMetrics.sources) {
        const auto mark = markMetrics.sources.find(entry.first);
        if (mark == markMetrics.sources.end()) {
            throw std::runtime_error("method source sets differ");
        }
        sourceDeltas.emplace_back(entry.first,
            psnr(entry.second.first / entry.second.second)
                - psnr(mark->second.first / mark->second.second));
    }
    std::vector<double> sortedDeltas;
    sortedDeltas.reserve(sourceDeltas.size());
    for (const auto &entry : sourceDeltas) sortedDeltas.push_back(entry.second);
    std::sort(sortedDeltas.begin(), sortedDeltas.end());
    const double medianDelta = sortedDeltas[sortedDeltas.size() / 2];
    const double worstDelta = sortedDeltas.front();

    std::cout.imbue(std::locale::classic());
    std::cout << std::fixed << std::setprecision(12)
        << "{\n  \"corpus_payload_sha256\": \""
        << tgmr::hex(corpus.header.payloadSha256) << "\",\n"
        << "  \"crop_edge\": " << options.cropEdge << ",\n"
        << "  \"crop_full_markesteijn_max_error\": " << parityMaximum << ",\n"
        << "  \"format\": \"rawtherapee-xtrans-tgmr-population-evaluation-v1\",\n"
        << "  \"jobs\": " << options.jobs << ",\n"
        << "  \"manifest_sha256\": \"" << tgmr::hex(corpus.header.manifestSha256)
        << "\",\n  \"markesteijn\": ";
    emitSummary(std::cout, markSummary);
    std::cout << ",\n  \"median_source_psnr_delta\": " << medianDelta
        << ",\n  \"model_sha256\": \"" << tgmr::hex(modelInspection.fileSha256)
        << "\",\n  \"patches\": " << records.size() << ",\n"
        << "  \"phase_assignment\": \"balanced-record-index-modulo-18\",\n"
        << "  \"phase_counts\": [";
    for (std::size_t phase = 0; phase < phaseCounts.size(); ++phase) {
        if (phase) std::cout << ", ";
        std::cout << phaseCounts[phase];
    }
    std::cout << "],\n  \"source_psnr_deltas\": [";
    for (std::size_t index = 0; index < sourceDeltas.size(); ++index) {
        if (index) std::cout << ", ";
        std::cout << "{\"delta\": " << sourceDeltas[index].second
            << ", \"source_ordinal\": " << sourceDeltas[index].first << '}';
    }
    std::cout << "],\n  \"sources\": " << sourceDeltas.size()
        << ",\n  \"split\": \"" << splitName(options.split) << "\",\n"
        << "  \"tgmr\": ";
    emitSummary(std::cout, tgmrSummary);
    std::cout << ",\n  \"worst_source_psnr_delta\": " << worstDelta << "\n}\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        return run(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << "TGMR population evaluation error: " << error.what() << '\n';
        return 2;
    }
}
