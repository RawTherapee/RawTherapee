#include "tgmr/manifest.h"

#include "tgmr/camera_matrices.h"
#include "tgmr/patch_selection.h"
#include "tgmr/sha256.h"

#include "cJSON.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace tgmr
{
namespace
{

const cJSON *field(const cJSON *object, const char *name)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!value) throw std::runtime_error(std::string("manifest field is missing: ") + name);
    return value;
}

std::string text(const cJSON *object, const char *name)
{
    const cJSON *value = field(object, name);
    if (!cJSON_IsString(value) || !value->valuestring || !*value->valuestring) {
        throw std::runtime_error(std::string("manifest field is not a non-empty string: ") + name);
    }
    return value->valuestring;
}

std::uint64_t unsignedInteger(const cJSON *object, const char *name)
{
    const cJSON *value = field(object, name);
    if (cJSON_IsString(value) && value->valuestring && *value->valuestring) {
        std::size_t consumed = 0;
        const std::string encoded(value->valuestring);
        const std::uint64_t result = std::stoull(encoded, &consumed, 0);
        if (consumed != encoded.size()) {
            throw std::runtime_error(std::string("manifest integer string is malformed: ") + name);
        }
        return result;
    }
    if (!cJSON_IsNumber(value) || value->valuedouble < 0.0
        || value->valuedouble > 9007199254740991.0
        || std::floor(value->valuedouble) != value->valuedouble) {
        throw std::runtime_error(std::string("manifest field is not an exact non-negative integer: ") + name);
    }
    return static_cast<std::uint64_t>(value->valuedouble);
}

double finiteNumber(const cJSON *object, const char *name)
{
    const cJSON *value = field(object, name);
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble)) {
        throw std::runtime_error(std::string("manifest field is not a finite number: ") + name);
    }
    return value->valuedouble;
}

template<typename T>
T boundedUnsignedInteger(const cJSON *object, const char *name)
{
    const std::uint64_t value = unsignedInteger(object, name);
    if (value > std::numeric_limits<T>::max()) {
        throw std::runtime_error(std::string("manifest integer is out of range: ") + name);
    }
    return static_cast<T>(value);
}

void requireFields(
    const cJSON *object,
    const std::set<std::string> &allowed,
    const char *context)
{
    for (const cJSON *item = object->child; item; item = item->next) {
        if (!item->string || allowed.find(item->string) == allowed.end()) {
            throw std::runtime_error(std::string(context) + " contains an unknown field: "
                                     + (item->string ? item->string : "<unnamed>"));
        }
    }
}

bool canonicalSha256(const std::string &value)
{
    return value.size() == 64
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isdigit(character) || (character >= 'a' && character <= 'f');
        });
}

unsigned hammingDistance64(std::uint64_t value)
{
    unsigned count = 0;
    while (value != 0) {
        value &= value - 1;
        ++count;
    }
    return count;
}

class NoiseRandom final
{
public:
    explicit NoiseRandom(std::uint64_t seed) : state_(seed ? seed : 1) {}

    std::uint32_t next()
    {
        std::uint64_t value = state_;
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        state_ = value;
        return static_cast<std::uint32_t>(
            (value * 0x2545f4914f6cdd1dULL) >> 32);
    }

private:
    std::uint64_t state_;
};

std::int32_t approximateGaussianQ8(NoiseRandom &random)
{
    // Twelve independent eight-bit uniforms form a bounded, integer-only
    // Irwin-Hall approximation having zero mean and sigma approximately 256.
    // Keeping this step entirely integral makes TGPC output reproducible on
    // every supported host rather than depending on libm's Gaussian sampler.
    std::int32_t sum = 0;
    for (unsigned sample = 0; sample < 12; ++sample) {
        sum += static_cast<std::int32_t>(random.next() & 255U);
    }
    return sum - 1530;
}

std::uint32_t integerSquareRoot(std::uint32_t value)
{
    std::uint32_t root = 0;
    std::uint32_t bit = 1U << 30;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

std::uint16_t addSensorNoise(
    std::uint16_t value,
    const std::array<std::uint8_t, 32> &sourceIdentity,
    const PatchSelection &selection,
    std::size_t sampleIndex)
{
    std::uint64_t seed = selection.sequence
        ^ (static_cast<std::uint64_t>(selection.x) << 16)
        ^ (static_cast<std::uint64_t>(selection.y) << 40)
        ^ (static_cast<std::uint64_t>(sampleIndex) * 0x9e3779b97f4a7c15ULL);
    for (unsigned byte = 0; byte < 8; ++byte) {
        seed ^= static_cast<std::uint64_t>(sourceIdentity[byte]) << (8 * byte);
    }
    NoiseRandom random(seed);
    const std::int64_t read = approximateGaussianQ8(random) / 32;
    const std::int64_t signal = static_cast<std::int64_t>(approximateGaussianQ8(random))
        * integerSquareRoot(value) / 1024;
    const std::int64_t noisy = static_cast<std::int64_t>(value) + read + signal;
    return static_cast<std::uint16_t>(std::max<std::int64_t>(
        0, std::min<std::int64_t>(65535, noisy)));
}

bool supportedUrl(const std::string &value)
{
    return value.rfind("https://", 0) == 0 || value.rfind("http://", 0) == 0
        || value.rfind("file://", 0) == 0;
}

CorpusSplit split(const std::string &value)
{
    if (value == "train") return CorpusSplit::TRAIN;
    if (value == "validation") return CorpusSplit::VALIDATION;
    if (value == "test") return CorpusSplit::TEST;
    throw std::runtime_error("manifest split is invalid: " + value);
}

std::filesystem::path safeCachePath(
    const std::string &cache,
    const std::string &relativeName)
{
    const std::filesystem::path name(relativeName);
    if (name.empty() || name.is_absolute()) {
        throw std::runtime_error("manifest cache filename is unsafe");
    }
    for (const auto &component : name) {
        if (component.empty() || component == "." || component == "..") {
            throw std::runtime_error("manifest cache filename is unsafe");
        }
    }
    const std::filesystem::path root = std::filesystem::weakly_canonical(cache);
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(root / name);
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end() && candidatePart != candidate.end();
         ++rootPart, ++candidatePart) {
        if (*rootPart != *candidatePart) {
            throw std::runtime_error("manifest cache filename escapes the cache");
        }
    }
    if (rootPart != root.end()) {
        throw std::runtime_error("manifest cache filename escapes the cache");
    }
    return candidate;
}

std::filesystem::path sourcePath(const std::string &cache, const SourceRecord &record)
{
    return safeCachePath(cache, record.cacheFilename);
}

double gainFromQ12(std::uint16_t value)
{
    return value / 4096.0;
}

void verifyDecodedMetadata(
    const SourceRecord &record,
    const LinearImage &image,
    const ImageClassification &classification,
    bool requireDecodedDigest)
{
    if (image.width != record.width || image.height != record.height
        || image.orientation != record.orientation || image.fileType != record.fileType
        || image.iccIdentity != record.iccIdentity) {
        throw std::runtime_error("decoded metadata differs from manifest: " + record.sourceId);
    }
    if (requireDecodedDigest
        && classification.decodedPixelSha256 != record.decodedPixelSha256) {
        throw std::runtime_error("decoded pixels differ from manifest: " + record.sourceId);
    }
}

struct ClassificationTask final {
    std::uint64_t ordinal = 0;
    std::string sourceId;
    std::string cacheFilename;
    std::string expectedSha256;
    std::filesystem::path path;
    const SourceRecord *reviewedRecord = nullptr;
};

struct ClassificationResult final {
    std::uint64_t ordinal = 0;
    std::string json;
};

std::string jsonEscape(const std::string &value)
{
    std::ostringstream output;
    for (unsigned char byte : value) {
        switch (byte) {
            case '\"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (byte < 0x20) {
                    output << "\\u00" << std::hex << std::setfill('0')
                           << std::setw(2) << static_cast<unsigned>(byte) << std::dec;
                } else {
                    output << static_cast<char>(byte);
                }
        }
    }
    return output.str();
}

std::string bindSourceSha256(std::string json, const std::string &digest)
{
    if (!canonicalSha256(digest) || json.empty() || json.front() != '{') {
        throw std::runtime_error("cannot bind source SHA-256 to classification JSON");
    }
    json.insert(1, "\"source_sha256\":\"" + digest + "\",");
    return json;
}

void durableWrite(const std::filesystem::path &path, const std::string &contents)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    std::FILE *stream = std::fopen(temporary.string().c_str(), "wb");
    if (!stream) throw std::runtime_error("cannot create durable classifier state");
    const bool wrote = contents.empty()
        || std::fwrite(contents.data(), 1, contents.size(), stream) == contents.size();
    const bool flushed = std::fflush(stream) == 0;
#if defined(_WIN32)
    const bool synced = flushed && _commit(_fileno(stream)) == 0;
#else
    const bool synced = flushed && fsync(fileno(stream)) == 0;
#endif
    const bool closed = std::fclose(stream) == 0;
    if (!wrote || !flushed || !synced || !closed) {
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("cannot flush durable classifier state");
    }
#if defined(_WIN32)
    std::filesystem::remove(path, ignored);
#endif
    if (std::rename(temporary.string().c_str(), path.string().c_str()) != 0) {
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("cannot publish durable classifier state");
    }
}

std::string finalizationBinding(
    const std::string &inputManifest,
    std::size_t records)
{
    std::ostringstream output;
    output << "rawtherapee-tgmr-corpus-finalization-work-v1\n"
           << "input " << hex(sha256File(inputManifest)) << '\n'
           << "records " << records << '\n'
           << "recipe production-augmentation-v1\n";
    return output.str();
}

SourceRecord finalizeSourceRecord(SourceRecord record, const std::string &cacheDirectory)
{
    static const double exposures[] = {-2.0,-1.5,-1.0,-0.5,0.5,1.0,1.5,2.0};
    static const std::array<std::array<double, 3>, 6> whiteBalances{{
        {{2.0,1.0,0.5}}, {{0.5,1.0,2.0}},
        {{1.5,0.75,0.888888888889}}, {{0.666666666667,1.333333333333,1.125}},
        {{1.25,0.8,1.0}}, {{0.8,1.25,1.0}},
    }};
    if (!record.selected) return record;
    const auto path = sourcePath(cacheDirectory, record);
    if (hex(sha256File(path.string())) != record.sha256) {
        throw std::runtime_error("source changed before patch finalization: "
                                 + record.sourceId);
    }
    const auto image = loadLinearImage(path.string());
    const std::size_t count = record.split == CorpusSplit::TRAIN ? 256 : 128;
    const auto proposals = proposePatches(image, record.patchSamplingSeed, count);
    record.patches.clear();
    record.patches.reserve(proposals.size());
    const auto selector = sha256(record.sourceId.data(), record.sourceId.size());
    for (std::size_t index = 0; index < proposals.size(); ++index) {
        const auto &proposal = proposals[index];
        const bool identity = index % 4 == 0;
        const unsigned choice = (selector[index % selector.size()] + index) & 255U;
        PatchSelection patch;
        patch.x = proposal.x;
        patch.y = proposal.y;
        patch.coverage = proposal.coverage;
        patch.coverageClass = proposal.coverageClass;
        patch.augmentationKind = identity ? 0 : 1;
        patch.exposureStopsQ8 = static_cast<std::int16_t>(std::llround(
            (identity ? 0.0 : exposures[choice % 8]) * 256.0));
        const auto whiteBalance = identity
            ? std::array<double, 3>{{1.0,1.0,1.0}}
            : whiteBalances[(choice / 8) % whiteBalances.size()];
        for (unsigned channel = 0; channel < 3; ++channel) {
            patch.whiteBalanceQ12[channel] = static_cast<std::uint16_t>(
                std::llround(whiteBalance[channel] * 4096.0));
        }
        patch.matrixId = identity ? 0
            : record.split == CorpusSplit::TRAIN ? 1 + choice % 4
            : 101 + choice % 2;
        patch.sequence = static_cast<std::uint16_t>(index);
        record.patches.push_back(patch);
    }
    return record;
}

std::string classificationInputDigest(
    const std::vector<ClassificationTask> &tasks,
    bool proxy)
{
    Sha256 digest;
    const char *contract = "rawtherapee-tgmr-classifier-contract-v1\n";
    digest.update(contract, std::strlen(contract));
    const char *mode = proxy ? "proxy-v1\n" : "full-v1\n";
    digest.update(mode, std::strlen(mode));
    for (const auto &task : tasks) {
        const std::string record = std::to_string(task.ordinal) + '\n'
            + task.sourceId + '\n' + task.cacheFilename + '\n'
            + task.expectedSha256 + '\n';
        digest.update(record.data(), record.size());
    }
    return hex(digest.finish());
}

std::string segmentHeader(const std::string &inputDigest, bool proxy)
{
    return "# rawtherapee-tgmr-classification-segment-v1 " + inputDigest
        + (proxy ? " proxy\n" : " full\n");
}

std::pair<std::uint64_t, std::string> parseSegmentRecord(
    const std::string &line,
    const std::vector<ClassificationTask> &tasks)
{
    const std::size_t separator = line.find('\t');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= line.size()) {
        throw std::runtime_error("classifier checkpoint record is malformed");
    }
    std::size_t consumed = 0;
    const std::uint64_t ordinal = std::stoull(line.substr(0, separator), &consumed);
    if (consumed != separator || ordinal >= tasks.size()) {
        throw std::runtime_error("classifier checkpoint ordinal is invalid");
    }
    const std::string json = line.substr(separator + 1);
    cJSON *root = cJSON_Parse(json.c_str());
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        throw std::runtime_error("classifier checkpoint JSON is malformed");
    }
    try {
        if (text(root, "source_id") != tasks[ordinal].sourceId) {
            throw std::runtime_error("classifier checkpoint source identity changed");
        }
        cJSON_Delete(root);
    } catch (...) {
        cJSON_Delete(root);
        throw;
    }
    return {ordinal, json + '\n'};
}

std::vector<std::filesystem::path> discoverClassificationSegments(
    const std::filesystem::path &workDirectory,
    const std::string &inputDigest,
    bool proxy,
    const std::vector<ClassificationTask> &tasks,
    std::vector<bool> &completed,
    std::uint64_t &nextSegment)
{
    const std::regex pattern("segment-([0-9]{8})-([0-9a-f]{64})\\.jsonl");
    std::vector<std::pair<std::uint64_t, std::filesystem::path>> numbered;
    for (const auto &entry : std::filesystem::directory_iterator(workDirectory)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".tmp") continue;
        std::smatch match;
        if (!std::regex_match(name, match, pattern)) {
            if (name.rfind("segment-", 0) == 0) {
                throw std::runtime_error("classifier work directory contains a malformed segment");
            }
            continue;
        }
        const std::uint64_t sequence = std::stoull(match[1].str());
        if (hex(sha256File(entry.path().string())) != match[2].str()) {
            throw std::runtime_error("classifier checkpoint segment digest mismatch");
        }
        numbered.emplace_back(sequence, entry.path());
        nextSegment = std::max(nextSegment, sequence + 1);
    }
    std::sort(numbered.begin(), numbered.end());
    std::vector<std::filesystem::path> output;
    for (const auto &item : numbered) {
        std::ifstream stream(item.second, std::ios::binary);
        std::string line;
        if (!std::getline(stream, line)
            || line + '\n' != segmentHeader(inputDigest, proxy)) {
            throw std::runtime_error("classifier checkpoint segment binding mismatch");
        }
        std::uint64_t previous = 0;
        bool first = true;
        while (std::getline(stream, line)) {
            const auto record = parseSegmentRecord(line, tasks);
            if ((!first && record.first <= previous) || completed[record.first]) {
                throw std::runtime_error("classifier checkpoint contains duplicate/reordered results");
            }
            first = false;
            previous = record.first;
            completed[record.first] = true;
        }
        if (!stream.eof()) throw std::runtime_error("classifier checkpoint read failed");
        output.push_back(item.second);
    }
    return output;
}

std::filesystem::path publishClassificationSegment(
    const std::filesystem::path &workDirectory,
    std::uint64_t sequence,
    const std::string &inputDigest,
    bool proxy,
    std::vector<ClassificationResult> records)
{
    std::sort(records.begin(), records.end(), [](const auto &left, const auto &right) {
        return left.ordinal < right.ordinal;
    });
    std::ostringstream contents;
    contents << segmentHeader(inputDigest, proxy);
    std::uint64_t previous = 0;
    bool first = true;
    for (const auto &record : records) {
        if (!first && record.ordinal <= previous) {
            throw std::runtime_error("new classifier segment contains duplicate results");
        }
        first = false;
        previous = record.ordinal;
        contents << record.ordinal << '\t' << record.json;
        if (record.json.empty() || record.json.back() != '\n') contents << '\n';
    }
    const std::string bytes = contents.str();
    const std::string digest = hex(sha256(bytes.data(), bytes.size()));
    std::ostringstream name;
    name << "segment-" << std::setfill('0') << std::setw(8) << sequence
         << '-' << digest << ".jsonl";
    const auto path = workDirectory / name.str();
    durableWrite(path, bytes);
    return path;
}

struct MergeCursor final {
    explicit MergeCursor(const std::filesystem::path &path)
        : stream(path, std::ios::binary)
    {
        if (!stream) throw std::runtime_error("cannot open classifier merge input");
        advance();
        if (valid && line.rfind("# ", 0) == 0) advance();
    }

    void advance()
    {
        valid = static_cast<bool>(std::getline(stream, line));
        if (!valid) {
            if (!stream.eof()) throw std::runtime_error("classifier merge input read failed");
            return;
        }
        if (line.rfind("# ", 0) == 0) return;
        const std::size_t separator = line.find('\t');
        if (separator == std::string::npos) {
            throw std::runtime_error("classifier merge record is malformed");
        }
        std::size_t consumed = 0;
        ordinal = std::stoull(line.substr(0, separator), &consumed);
        if (consumed != separator) throw std::runtime_error("classifier merge ordinal is malformed");
        json = line.substr(separator + 1);
    }

    std::ifstream stream;
    std::string line;
    std::string json;
    std::uint64_t ordinal = 0;
    bool valid = false;
};

void mergeClassificationRuns(
    const std::vector<std::filesystem::path> &inputs,
    const std::filesystem::path &output,
    bool finalOutput,
    std::uint64_t expectedRecords,
    bool allowMissingOrdinals = false)
{
    std::vector<std::unique_ptr<MergeCursor>> cursors;
    for (const auto &input : inputs) cursors.emplace_back(new MergeCursor(input));
    using HeapValue = std::pair<std::uint64_t, std::size_t>;
    std::priority_queue<HeapValue, std::vector<HeapValue>, std::greater<HeapValue>> heap;
    for (std::size_t index = 0; index < cursors.size(); ++index) {
        if (cursors[index]->valid) heap.emplace(cursors[index]->ordinal, index);
    }
    const std::filesystem::path temporary = output.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot create classifier merge output");
    std::uint64_t count = 0;
    std::uint64_t previous = 0;
    bool first = true;
    while (!heap.empty()) {
        const auto current = heap.top();
        heap.pop();
        if ((!first && current.first <= previous)
            || (finalOutput && !allowMissingOrdinals && current.first != count)) {
            throw std::runtime_error("classifier merge contains duplicate or missing results");
        }
        first = false;
        previous = current.first;
        const auto &cursor = cursors[current.second];
        if (finalOutput) stream << cursor->json << '\n';
        else stream << current.first << '\t' << cursor->json << '\n';
        ++count;
        cursor->advance();
        if (cursor->valid) heap.emplace(cursor->ordinal, current.second);
    }
    if (finalOutput && count != expectedRecords) {
        throw std::runtime_error("classifier merge is incomplete");
    }
    stream.flush();
    if (!stream) {
        stream.close();
        std::filesystem::remove(temporary);
        throw std::runtime_error("classifier merge output write failed");
    }
    stream.close();
    std::error_code ignored;
    if (finalOutput) std::filesystem::remove(output, ignored);
    std::filesystem::rename(temporary, output);
}

void finalizeClassificationOutput(
    std::vector<std::filesystem::path> runs,
    const std::filesystem::path &workDirectory,
    const std::string &outputJsonl,
    std::uint64_t expectedRecords,
    bool allowMissingOrdinals)
{
    std::vector<std::filesystem::path> temporaryRuns;
    unsigned pass = 0;
    while (runs.size() > 64) {
        std::vector<std::filesystem::path> next;
        for (std::size_t start = 0; start < runs.size(); start += 64) {
            const std::size_t end = std::min(runs.size(), start + 64);
            std::ostringstream name;
            name << ".merge-" << pass << '-' << (start / 64) << ".run";
            const auto path = workDirectory / name.str();
            mergeClassificationRuns(
                std::vector<std::filesystem::path>(runs.begin() + start, runs.begin() + end),
                path, false, 0);
            next.push_back(path);
            temporaryRuns.push_back(path);
        }
        runs = std::move(next);
        ++pass;
    }
    mergeClassificationRuns(
        runs, outputJsonl, true, expectedRecords, allowMissingOrdinals);
    std::error_code ignored;
    for (const auto &path : temporaryRuns) std::filesystem::remove(path, ignored);
}

void runClassificationTasks(
    const std::vector<ClassificationTask> &tasks,
    const std::string &outputJsonl,
    bool force,
    const ClassificationOptions &options)
{
    if (tasks.empty()) throw std::runtime_error("classification input contains no tasks");
    if (options.jobs == 0 || options.checkpointImages == 0
        || options.checkpointSeconds == 0 || options.progressSeconds == 0) {
        throw std::runtime_error("classifier jobs/checkpoint/progress values must be positive");
    }
    if (!force && std::filesystem::exists(outputJsonl)) {
        throw std::runtime_error("refusing to replace classification output");
    }
    const std::filesystem::path workDirectory = options.workDirectory.empty()
        ? std::filesystem::path(outputJsonl + ".work")
        : std::filesystem::path(options.workDirectory);
    std::filesystem::create_directories(workDirectory);
    const std::string inputDigest = classificationInputDigest(tasks, options.proxy);
    std::ostringstream run;
    run << "{\n  \"format\": \"rawtherapee-tgmr-classification-run-v1\",\n"
        << "  \"input_sha256\": \"" << inputDigest << "\",\n"
        << "  \"mode\": \"" << (options.proxy ? "proxy" : "full") << "\",\n"
        << "  \"task_count\": " << tasks.size() << "\n}\n";
    const auto runPath = workDirectory / "run.json";
    if (std::filesystem::exists(runPath)) {
        std::ifstream input(runPath, std::ios::binary);
        const std::string existing{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (existing != run.str()) {
            throw std::runtime_error(
                "classifier work directory belongs to different input or settings");
        }
    } else {
        durableWrite(runPath, run.str());
    }

    std::vector<bool> completed(tasks.size(), false);
    std::uint64_t nextSegment = 0;
    auto segments = discoverClassificationSegments(
        workDirectory, inputDigest, options.proxy, tasks, completed, nextSegment);
    std::vector<std::size_t> order;
    order.reserve(tasks.size());
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        if (!completed[index]) order.push_back(index);
    }
    std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        return std::tie(tasks[left].path, tasks[left].ordinal)
            < std::tie(tasks[right].path, tasks[right].ordinal);
    });

    std::atomic<std::size_t> next{0};
    std::atomic<std::uint32_t> active{options.jobs};
    std::atomic<std::uint64_t> processedBytes{0};
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<ClassificationResult> pending;
    std::vector<std::pair<std::uint64_t, std::string>> failures;
    const auto started = std::chrono::steady_clock::now();
    auto lastCheckpoint = started;
    auto lastProgress = started;

    auto worker = [&]() {
        while (true) {
            const std::size_t position = next.fetch_add(1);
            if (position >= order.size()) break;
            const ClassificationTask &task = tasks[order[position]];
            std::string error;
            bool success = false;
            for (std::uint32_t attempt = 0; attempt <= options.retries; ++attempt) {
                try {
                    LoadedLinearImage loaded = loadLinearImageAndSha256(
                        task.path.string(), options.proxy);
                    if (!task.expectedSha256.empty()
                        && loaded.fileSha256 != task.expectedSha256) {
                        throw std::runtime_error("source SHA-256 changed");
                    }
                    const ImageClassification classification = classifyImage(loaded.image);
                    if (task.reviewedRecord && !options.proxy) {
                        verifyDecodedMetadata(
                            *task.reviewedRecord, loaded.image, classification, false);
                    }
                    ClassificationResult result;
                    result.ordinal = task.ordinal;
                    result.json = options.proxy
                        ? canonicalProxyClassificationJson(
                            loaded.image, classification, task.sourceId, task.cacheFilename)
                        : canonicalClassificationJson(
                            loaded.image, classification, task.sourceId, task.cacheFilename);
                    result.json = bindSourceSha256(std::move(result.json), loaded.fileSha256);
                    processedBytes.fetch_add(loaded.fileBytes);
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        pending.push_back(std::move(result));
                    }
                    success = true;
                    changed.notify_one();
                    break;
                } catch (const std::exception &exception) {
                    error = exception.what();
                    if (attempt < options.retries) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(100 * (attempt + 1)));
                    }
                }
            }
            if (!success) {
                std::lock_guard<std::mutex> lock(mutex);
                failures.emplace_back(task.ordinal, error);
                changed.notify_one();
            }
        }
        active.fetch_sub(1);
        changed.notify_one();
    };

    std::vector<std::thread> workers;
    workers.reserve(options.jobs);
    for (std::uint32_t index = 0; index < options.jobs; ++index) {
        workers.emplace_back(worker);
    }
    const std::uint64_t resumed = tasks.size() - order.size();
    std::uint64_t checkpointed = resumed;
    std::exception_ptr coordinatorError;
    try {
        bool done = false;
        while (!done) {
            std::vector<ClassificationResult> records;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait_for(lock, std::chrono::seconds(1));
                done = active.load() == 0 && pending.empty();
                const auto now = std::chrono::steady_clock::now();
                if (pending.size() >= options.checkpointImages
                    || now - lastCheckpoint >= std::chrono::seconds(options.checkpointSeconds)
                    || active.load() == 0) {
                    records.swap(pending);
                    lastCheckpoint = now;
                    done = active.load() == 0 && pending.empty();
                }
            }
            if (!records.empty()) {
                segments.push_back(publishClassificationSegment(
                    workDirectory, nextSegment++, inputDigest, options.proxy, std::move(records)));
                std::ifstream segment(segments.back());
                std::uint64_t recordsInSegment = 0;
                std::string line;
                std::getline(segment, line);
                while (std::getline(segment, line)) ++recordsInSegment;
                checkpointed += recordsInSegment;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now - lastProgress >= std::chrono::seconds(options.progressSeconds)
                || active.load() == 0) {
                const double seconds = std::max(1e-9,
                    std::chrono::duration<double>(now - started).count());
                std::size_t failed = 0;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    failed = failures.size();
                }
                std::cerr << "TGMR classify: " << checkpointed << '/' << tasks.size()
                    << " checkpointed, " << failed << " failed, "
                    << active.load() << " workers active, "
                    << std::fixed << std::setprecision(2)
                    << ((checkpointed - resumed) / seconds) << " images/s, "
                    << (processedBytes.load() / (1024.0 * 1024.0) / seconds) << " MiB/s";
                const double rate = (checkpointed - resumed) / seconds;
                if (rate > 0.0 && checkpointed < tasks.size()) {
                    std::cerr << ", ETA " << ((tasks.size() - checkpointed) / rate) << " s";
                }
                std::cerr << '\n';
                std::ostringstream progress;
                progress << "{\n  \"checkpointed\": " << checkpointed << ",\n"
                    << "  \"failed\": " << failed << ",\n"
                    << "  \"format\": \"rawtherapee-tgmr-classification-progress-v1\",\n"
                    << "  \"input_sha256\": \"" << inputDigest << "\",\n"
                    << "  \"total\": " << tasks.size() << "\n}\n";
                durableWrite(workDirectory / "progress.json", progress.str());
                lastProgress = now;
            }
        }
    } catch (...) {
        coordinatorError = std::current_exception();
    }
    for (auto &thread : workers) thread.join();
    if (coordinatorError) std::rethrow_exception(coordinatorError);
    if (!failures.empty()) {
        std::sort(failures.begin(), failures.end());
        std::ostringstream report;
        report << "{\n  \"failures\": [\n";
        for (std::size_t index = 0; index < failures.size(); ++index) {
            if (index) report << ",\n";
            const auto &failure = failures[index];
            report << "    {\"error\": \"" << jsonEscape(failure.second)
                << "\", \"ordinal\": " << failure.first
                << ", \"source_id\": \"" << jsonEscape(tasks[failure.first].sourceId)
                << "\"}";
        }
        report << "\n  ],\n  \"format\": "
            << "\"rawtherapee-tgmr-classification-failures-v1\",\n"
            << "  \"input_sha256\": \"" << inputDigest << "\"\n}\n";
        durableWrite(workDirectory / "failures.json", report.str());
        if (!options.allowFailures) {
            throw std::runtime_error(std::to_string(failures.size())
                + " images failed classification; rerun after correcting inputs");
        }
        std::cerr << "TGMR classify: explicitly omitting " << failures.size()
            << " failed candidate image(s); see "
            << (workDirectory / "failures.json").string() << '\n';
    } else {
        std::error_code ignored;
        std::filesystem::remove(workDirectory / "failures.json", ignored);
    }
    finalizeClassificationOutput(
        segments, workDirectory, outputJsonl, tasks.size() - failures.size(),
        options.allowFailures);
}

} // namespace

std::vector<SourceRecord> readSourceManifest(const std::string &path)
{
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("cannot open source manifest: " + path);
    std::vector<SourceRecord> output;
    std::set<std::string> ids;
    std::set<std::string> filenames;
    std::map<std::string, CorpusSplit> authorSplits;
    std::map<std::string, CorpusSplit> contentSplits;
    std::map<std::string, CorpusSplit> perceptualSplits;
    std::map<std::string, CorpusSplit> pHashSplits;
    std::string line;
    std::uint64_t lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (line.empty()) throw std::runtime_error("source manifest contains a blank line");
        cJSON *root = cJSON_Parse(line.c_str());
        if (!root || !cJSON_IsObject(root)) {
            cJSON_Delete(root);
            throw std::runtime_error("source manifest JSON parse failure on line "
                                     + std::to_string(lineNumber));
        }
        try {
            const std::string format = text(root, "format");
            const bool version2 = format == "rawtherapee-tgmr-corpus-source-manifest-v2";
            if (!version2 && format != "rawtherapee-tgmr-corpus-source-manifest-v1") {
                throw std::runtime_error("wrong source manifest format");
            }
            std::set<std::string> recordFields{
                "format", "source_id", "split", "selected", "selection_status",
                "original_url", "fallback_urls", "landing_page", "author", "title",
                "license", "license_url", "advertised_checksum", "sha256",
                "decoded_pixel_sha256", "cache_filename", "file_type", "width",
                "height", "orientation", "icc_identity", "classification",
                "patch_sampling_seed", "patch_coordinates",
            };
            if (version2) {
                recordFields.insert({
                    "archive_fallbacks", "author_id", "author_url", "catalog",
                    "content_tags", "people_review_status", "rights",
                    "upstream_flickr_id", "upstream_source_id",
                });
            }
            requireFields(root, recordFields, "source manifest record");
            SourceRecord record;
            record.manifestV2 = version2;
            record.sourceId = text(root, "source_id");
            if (!std::all_of(record.sourceId.begin(), record.sourceId.end(),
                    [](unsigned char value) {
                        return std::isalnum(value) || value == '.' || value == '_'
                            || value == ':' || value == '-';
                    })) {
                throw std::runtime_error("source_id contains nonportable characters");
            }
            const std::string splitName = text(root, "split");
            record.splitAssigned = splitName != "unassigned";
            if (!record.splitAssigned && !version2) {
                throw std::runtime_error("v1 source cannot have an unassigned split");
            }
            record.split = record.splitAssigned ? split(splitName) : CorpusSplit::TRAIN;
            const cJSON *selected = field(root, "selected");
            if (!cJSON_IsBool(selected)) throw std::runtime_error("selected must be Boolean");
            record.selected = cJSON_IsTrue(selected);
            if (record.selected && !record.splitAssigned) {
                throw std::runtime_error("selected source cannot have an unassigned split");
            }
            record.selectionStatus = text(root, "selection_status");
            if (version2) {
                static const std::set<std::string> candidateStatuses{
                    "candidate-pending-review", "candidate-reviewed",
                    "candidate-pending-duplicate-review", "candidate-rejected-duplicate",
                };
                if ((record.selected && record.selectionStatus != "accepted-corpus-v1")
                    || (!record.selected
                        && candidateStatuses.find(record.selectionStatus)
                            == candidateStatuses.end())) {
                    throw std::runtime_error("v2 source selection_status is invalid");
                }
            }
            const cJSON *advertised = field(root, "advertised_checksum");
            if (cJSON_IsString(advertised) && advertised->valuestring
                && *advertised->valuestring) {
                record.advertisedChecksum = advertised->valuestring;
            } else if (!cJSON_IsNull(advertised)) {
                throw std::runtime_error("advertised_checksum must be a string or null");
            }
            record.cacheFilename = text(root, "cache_filename");
            record.originalUrl = text(root, "original_url");
            record.landingPage = text(root, "landing_page");
            record.sha256 = text(root, "sha256");
            record.decodedPixelSha256 = text(root, "decoded_pixel_sha256");
            record.classification.decodedPixelSha256 = record.decodedPixelSha256;
            record.author = text(root, "author");
            record.title = text(root, "title");
            record.license = text(root, "license");
            record.licenseUrl = text(root, "license_url");
            record.fileType = text(root, "file_type");
            record.width = boundedUnsignedInteger<std::uint32_t>(root, "width");
            record.height = boundedUnsignedInteger<std::uint32_t>(root, "height");
            record.orientation = boundedUnsignedInteger<std::uint16_t>(root, "orientation");
            record.iccIdentity = text(root, "icc_identity");
            const cJSON *classification = field(root, "classification");
            if (!cJSON_IsObject(classification)) {
                throw std::runtime_error("classification must be an object");
            }
            std::set<std::string> classificationFields{
                "channel_means", "chroma_ratio_mean", "clipped_black_fraction",
                "clipped_white_fraction", "gradient_rms", "hue_degrees",
                "jpeg_blockiness", "laplacian_rms", "local_contrast",
                "luminance_mean", "luminance_p01", "luminance_p99",
                "luminance_stddev", "perceptual_hash", "saturation_mean",
            };
            if (version2) {
                classificationFields.insert({
                    "dhash", "phash", "luminance_histogram", "hue_histogram",
                    "saturation_histogram",
                });
            }
            requireFields(classification, classificationFields, "classification");
            const cJSON *channelMeans = field(classification, "channel_means");
            if (!cJSON_IsArray(channelMeans) || cJSON_GetArraySize(channelMeans) != 3) {
                throw std::runtime_error("classification channel_means must have three values");
            }
            for (int channel = 0; channel < 3; ++channel) {
                const cJSON *mean = cJSON_GetArrayItem(channelMeans, channel);
                if (!cJSON_IsNumber(mean) || !std::isfinite(mean->valuedouble)) {
                    throw std::runtime_error("classification channel_means contains a non-finite value");
                }
                record.classification.channelMeans[channel] = mean->valuedouble;
            }
            record.classification.chromaRatioMean = finiteNumber(classification, "chroma_ratio_mean");
            record.classification.clippedBlackFraction = finiteNumber(classification, "clipped_black_fraction");
            record.classification.clippedWhiteFraction = finiteNumber(classification, "clipped_white_fraction");
            record.classification.gradientRms = finiteNumber(classification, "gradient_rms");
            record.classification.hueDegrees = finiteNumber(classification, "hue_degrees");
            record.classification.jpegBlockiness = finiteNumber(classification, "jpeg_blockiness");
            record.classification.laplacianRms = finiteNumber(classification, "laplacian_rms");
            record.classification.localContrast = finiteNumber(classification, "local_contrast");
            record.classification.luminanceMean = finiteNumber(classification, "luminance_mean");
            record.classification.luminanceP01 = finiteNumber(classification, "luminance_p01");
            record.classification.luminanceP99 = finiteNumber(classification, "luminance_p99");
            record.classification.luminanceStddev = finiteNumber(classification, "luminance_stddev");
            record.classification.saturationMean = finiteNumber(classification, "saturation_mean");
            const cJSON *perceptual = cJSON_GetObjectItemCaseSensitive(
                classification, "perceptual_hash");
            if (!cJSON_IsString(perceptual) || !perceptual->valuestring
                || std::strlen(perceptual->valuestring) != 16
                || !std::all_of(perceptual->valuestring,
                                perceptual->valuestring + 16,
                                [](unsigned char character) {
                                    return std::isdigit(character)
                                        || (character >= 'a' && character <= 'f');
                                })) {
                throw std::runtime_error("classification perceptual_hash is malformed");
            }
            record.perceptualHash = perceptual->valuestring;
            record.classification.perceptualHash = record.perceptualHash;
            if (version2) {
                auto signature = [&](const char *name) {
                    const cJSON *value = field(classification, name);
                    if (!cJSON_IsString(value) || !value->valuestring
                        || std::strlen(value->valuestring) != 16
                        || !std::all_of(value->valuestring, value->valuestring + 16,
                            [](unsigned char character) {
                                return std::isdigit(character)
                                    || (character >= 'a' && character <= 'f');
                            })) {
                        throw std::runtime_error(std::string("classification ") + name
                                                 + " is malformed");
                    }
                    return std::string(value->valuestring);
                };
                if (signature("dhash") != record.perceptualHash) {
                    throw std::runtime_error("v2 dhash and compatibility perceptual_hash differ");
                }
                record.pHash = signature("phash");
                record.classification.pHash = record.pHash;
                // Array items do not have object names, so parse them directly.
                auto histogramItems = [&](const char *name, auto &target) {
                    const cJSON *array = field(classification, name);
                    if (!cJSON_IsArray(array)
                        || cJSON_GetArraySize(array) != static_cast<int>(target.size())) {
                        throw std::runtime_error(std::string("classification ") + name
                                                 + " has the wrong size");
                    }
                    for (std::size_t index = 0; index < target.size(); ++index) {
                        const cJSON *item = cJSON_GetArrayItem(array, static_cast<int>(index));
                        if (!cJSON_IsNumber(item) || item->valuedouble < 0.0
                            || std::floor(item->valuedouble) != item->valuedouble
                            || item->valuedouble > 9007199254740991.0) {
                            throw std::runtime_error(std::string("classification ") + name
                                                     + " contains a non-integer");
                        }
                        target[index] = static_cast<std::uint64_t>(item->valuedouble);
                    }
                };
                histogramItems("luminance_histogram", record.classification.luminanceHistogram);
                histogramItems("hue_histogram", record.classification.hueHistogram);
                histogramItems("saturation_histogram", record.classification.saturationHistogram);
            } else {
                record.pHash = record.perceptualHash;
                record.classification.pHash = record.perceptualHash;
            }
            record.patchSamplingSeed = unsignedInteger(root, "patch_sampling_seed");
            if (!canonicalSha256(record.sha256)
                || !canonicalSha256(record.decodedPixelSha256)) {
                throw std::runtime_error("manifest SHA-256 field is malformed");
            }
            if (record.width < 7 || record.height < 7
                || record.orientation < 1 || record.orientation > 8) {
                throw std::runtime_error("manifest decoded image contract is invalid");
            }
            static const std::set<std::string> permittedLicenses{
                "CC0-1.0", "PDM-1.0", "CC-BY-2.0", "CC-BY-3.0", "CC-BY-4.0",
            };
            if (record.selected && permittedLicenses.find(record.license) == permittedLicenses.end()) {
                throw std::runtime_error("selected manifest source has a prohibited license");
            }
            if (!supportedUrl(record.originalUrl) || !supportedUrl(record.landingPage)
                || !supportedUrl(record.licenseUrl)) {
                throw std::runtime_error("manifest URL has an unsupported scheme");
            }
            const cJSON *fallbacks = field(root, "fallback_urls");
            if (!cJSON_IsArray(fallbacks)) throw std::runtime_error("fallback_urls must be an array");
            for (int index = 0; index < cJSON_GetArraySize(fallbacks); ++index) {
                const cJSON *url = cJSON_GetArrayItem(fallbacks, index);
                if (!cJSON_IsString(url) || !url->valuestring || !supportedUrl(url->valuestring)) {
                    throw std::runtime_error("fallback_urls contains an invalid URL");
                }
                record.fallbackUrls.emplace_back(url->valuestring);
            }
            if (version2) {
                record.authorId = text(root, "author_id");
                record.authorUrl = text(root, "author_url");
                if (!supportedUrl(record.authorUrl)) {
                    throw std::runtime_error("v2 author URL has an unsupported scheme");
                }
                const cJSON *catalog = field(root, "catalog");
                if (!cJSON_IsObject(catalog)) throw std::runtime_error("catalog must be an object");
                requireFields(catalog, {"name", "revision", "snapshot_sha256"}, "catalog");
                record.catalogName = text(catalog, "name");
                record.catalogRevision = text(catalog, "revision");
                record.catalogSnapshotSha256 = text(catalog, "snapshot_sha256");
                static const std::set<std::string> catalogs{
                    "openimages-cvdf-v5-boxable", "pass-v3", "wikimedia-commons",
                    "smithsonian-open-access",
                };
                if (catalogs.find(record.catalogName) == catalogs.end()
                    || !canonicalSha256(record.catalogSnapshotSha256)) {
                    throw std::runtime_error("v2 catalog identity is invalid");
                }
                record.upstreamSourceId = text(root, "upstream_source_id");
                const cJSON *flickr = cJSON_GetObjectItemCaseSensitive(
                    root, "upstream_flickr_id");
                if (cJSON_IsString(flickr) && flickr->valuestring && *flickr->valuestring) {
                    record.upstreamFlickrId = flickr->valuestring;
                } else if (!cJSON_IsNull(flickr)) {
                    throw std::runtime_error("upstream_flickr_id must be a string or null");
                }
                const cJSON *rights = field(root, "rights");
                if (!cJSON_IsObject(rights)) throw std::runtime_error("rights must be an object");
                requireFields(rights, {
                    "evidence_revision", "evidence_sha256", "evidence_url", "review_status",
                }, "rights");
                record.rightsEvidenceRevision = text(rights, "evidence_revision");
                record.rightsEvidenceSha256 = text(rights, "evidence_sha256");
                record.rightsEvidenceUrl = text(rights, "evidence_url");
                record.rightsReviewStatus = text(rights, "review_status");
                if (!canonicalSha256(record.rightsEvidenceSha256)
                    || !supportedUrl(record.rightsEvidenceUrl)
                    || (record.rightsReviewStatus != "approved"
                        && record.rightsReviewStatus != "rejected"
                        && record.rightsReviewStatus != "pending")) {
                    throw std::runtime_error("v2 rights evidence is invalid");
                }
                if (record.selected && record.rightsReviewStatus != "approved") {
                    throw std::runtime_error("selected v2 source lacks approved rights review");
                }
                record.peopleReviewStatus = text(root, "people_review_status");
                if (record.peopleReviewStatus != "not-applicable"
                    && record.peopleReviewStatus != "approved-no-minors-or-sensitive-content"
                    && record.peopleReviewStatus != "rejected"
                    && record.peopleReviewStatus != "pending") {
                    throw std::runtime_error("v2 people review status is invalid");
                }
                const cJSON *tags = field(root, "content_tags");
                if (!cJSON_IsArray(tags)) throw std::runtime_error("content_tags must be an array");
                static const std::set<std::string> permittedTags{
                    "people", "skin-hair-clothing", "foliage", "fur-feathers",
                    "architecture-brick", "textile-print", "metal-specular-jewelry",
                    "food", "water-sky", "low-light", "astronomy-star-field",
                    "macro-specimen",
                };
                std::set<std::string> uniqueTags;
                for (int index = 0; index < cJSON_GetArraySize(tags); ++index) {
                    const cJSON *tag = cJSON_GetArrayItem(tags, index);
                    if (!cJSON_IsString(tag) || !tag->valuestring
                        || permittedTags.find(tag->valuestring) == permittedTags.end()
                        || !uniqueTags.insert(tag->valuestring).second) {
                        throw std::runtime_error("content_tags contains an invalid value");
                    }
                    record.contentTags.emplace_back(tag->valuestring);
                }
                if (record.selected
                    && std::find(record.contentTags.begin(), record.contentTags.end(), "people")
                        != record.contentTags.end()
                    && record.peopleReviewStatus != "approved-no-minors-or-sensitive-content") {
                    throw std::runtime_error("selected people image lacks explicit review");
                }
                const cJSON *archives = field(root, "archive_fallbacks");
                if (!cJSON_IsArray(archives)) {
                    throw std::runtime_error("archive_fallbacks must be an array");
                }
                for (int index = 0; index < cJSON_GetArraySize(archives); ++index) {
                    const cJSON *archive = cJSON_GetArrayItem(archives, index);
                    if (!cJSON_IsObject(archive)) {
                        throw std::runtime_error("archive fallback must be an object");
                    }
                    requireFields(archive, {"url", "sha256", "member", "member_sha256"},
                                  "archive fallback");
                    ArchiveFallback value;
                    value.url = text(archive, "url");
                    value.sha256 = text(archive, "sha256");
                    value.member = text(archive, "member");
                    value.memberSha256 = text(archive, "member_sha256");
                    const std::filesystem::path member(value.member);
                    if (!supportedUrl(value.url) || !canonicalSha256(value.sha256)
                        || !canonicalSha256(value.memberSha256) || member.is_absolute()) {
                        throw std::runtime_error("archive fallback is invalid");
                    }
                    for (const auto &component : member) {
                        if (component == "..") {
                            throw std::runtime_error("archive fallback member is unsafe");
                        }
                    }
                    record.archiveFallbacks.push_back(std::move(value));
                }
            } else {
                record.authorId = record.author;
                record.authorUrl = record.landingPage;
                record.rightsReviewStatus = record.selected ? "approved" : "pending";
                record.peopleReviewStatus = "not-applicable";
            }
            const std::filesystem::path cacheName(record.cacheFilename);
            bool unsafeCacheName = cacheName.empty() || cacheName.is_absolute();
            for (const auto &component : cacheName) {
                unsafeCacheName = unsafeCacheName || component.empty()
                    || component == "." || component == "..";
            }
            if (unsafeCacheName) {
                throw std::runtime_error("manifest cache filename is unsafe");
            }
            const cJSON *patches = field(root, "patch_coordinates");
            if (!cJSON_IsArray(patches)) throw std::runtime_error("patch_coordinates must be an array");
            const int patchCount = cJSON_GetArraySize(patches);
            for (int index = 0; index < patchCount; ++index) {
                const cJSON *value = cJSON_GetArrayItem(patches, index);
                if (!cJSON_IsObject(value)) throw std::runtime_error("patch coordinate must be an object");
                requireFields(value,
                    {"x", "y", "coverage", "coverage_class", "augmentation"},
                    "patch coordinate");
                PatchSelection patch;
                patch.x = boundedUnsignedInteger<std::uint32_t>(value, "x");
                patch.y = boundedUnsignedInteger<std::uint32_t>(value, "y");
                const cJSON *coverage = cJSON_GetObjectItemCaseSensitive(value, "coverage");
                if (coverage) {
                    if (!cJSON_IsBool(coverage)) throw std::runtime_error("coverage must be Boolean");
                    patch.coverage = cJSON_IsTrue(coverage);
                }
                const cJSON *coverageClass = cJSON_GetObjectItemCaseSensitive(
                    value, "coverage_class");
                if (coverageClass) {
                    patch.coverageClass = boundedUnsignedInteger<std::uint8_t>(
                        value, "coverage_class");
                    if (patch.coverageClass > 16) {
                        throw std::runtime_error("coverage_class is outside 0..16");
                    }
                }
                const cJSON *augmentation = cJSON_GetObjectItemCaseSensitive(value, "augmentation");
                if (augmentation) {
                    if (!cJSON_IsObject(augmentation)) throw std::runtime_error("augmentation must be an object");
                    requireFields(augmentation,
                        {"kind", "exposure_stops", "white_balance", "matrix_id", "sequence"},
                        "patch augmentation");
                    patch.augmentationKind = boundedUnsignedInteger<std::uint8_t>(
                        augmentation, "kind");
                    const cJSON *exposure = field(augmentation, "exposure_stops");
                    if (!cJSON_IsNumber(exposure) || exposure->valuedouble < -2.0
                        || exposure->valuedouble > 2.0) {
                        throw std::runtime_error("augmentation exposure is outside -2..2");
                    }
                    patch.exposureStopsQ8 = static_cast<std::int16_t>(
                        std::llround(exposure->valuedouble * 256.0));
                    const cJSON *whiteBalance = field(augmentation, "white_balance");
                    if (!cJSON_IsArray(whiteBalance) || cJSON_GetArraySize(whiteBalance) != 3) {
                        throw std::runtime_error("augmentation white_balance must have three values");
                    }
                    for (unsigned channel = 0; channel < 3; ++channel) {
                        const cJSON *gain = cJSON_GetArrayItem(whiteBalance, channel);
                        if (!cJSON_IsNumber(gain) || gain->valuedouble < 0.5
                            || gain->valuedouble > 2.0) {
                            throw std::runtime_error("augmentation white-balance gain is outside 0.5..2");
                        }
                        patch.whiteBalanceQ12[channel] = static_cast<std::uint16_t>(
                            std::llround(gain->valuedouble * 4096.0));
                    }
                    patch.matrixId = boundedUnsignedInteger<std::uint16_t>(
                        augmentation, "matrix_id");
                    patch.sequence = boundedUnsignedInteger<std::uint16_t>(
                        augmentation, "sequence");
                }
                record.patches.push_back(patch);
            }
            if (!ids.insert(record.sourceId).second
                || !filenames.insert(record.cacheFilename).second) {
                throw std::runtime_error("duplicate manifest source ID or cache filename");
            }
            if (record.selected) {
                const auto author = authorSplits.emplace(record.authorId, record.split);
                if (!author.second && author.first->second != record.split) {
                    throw std::runtime_error("author occurs in more than one corpus split");
                }
                const auto content = contentSplits.emplace(record.decodedPixelSha256, record.split);
                if (!content.second) {
                    throw std::runtime_error("selected manifest contains an exact decoded-image duplicate");
                }
                if (!record.perceptualHash.empty()) {
                    const auto perceptualEntry = perceptualSplits.emplace(
                        record.perceptualHash, record.split);
                    if (!perceptualEntry.second
                        && perceptualEntry.first->second != record.split) {
                        throw std::runtime_error(
                            "perceptual duplicate occurs in more than one corpus split");
                    }
                }
                if (!record.pHash.empty()) {
                    const auto signature = pHashSplits.emplace(record.pHash, record.split);
                    if (!signature.second && signature.first->second != record.split) {
                        throw std::runtime_error(
                            "DCT perceptual duplicate occurs in more than one corpus split");
                    }
                }
            }
            output.push_back(std::move(record));
        } catch (...) {
            cJSON_Delete(root);
            throw;
        }
        cJSON_Delete(root);
    }
    // A 64-bit dHash equality check is insufficient for split-leakage
    // prevention: ordinary rescaling or recompression can change a few bits.
    // Five bits is the frozen corpus-v1 review threshold.  Candidate pairs at
    // or below it are rejected across author-independent splits and must be
    // resolved by source review rather than silently accepted.
    for (std::size_t left = 0; left < output.size(); ++left) {
        if (!output[left].selected) continue;
        const std::uint64_t leftHash = std::stoull(output[left].perceptualHash, nullptr, 16);
        for (std::size_t right = left + 1; right < output.size(); ++right) {
            if (!output[right].selected || output[left].split == output[right].split) continue;
            const std::uint64_t rightHash = std::stoull(
                output[right].perceptualHash, nullptr, 16);
            if (hammingDistance64(leftHash ^ rightHash) <= 5) {
                throw std::runtime_error(
                    "perceptual near-duplicate occurs in more than one corpus split");
            }
            const std::uint64_t leftPHash = std::stoull(output[left].pHash, nullptr, 16);
            const std::uint64_t rightPHash = std::stoull(output[right].pHash, nullptr, 16);
            if (hammingDistance64(leftPHash ^ rightPHash) <= 8) {
                throw std::runtime_error(
                    "DCT perceptual near-duplicate occurs in more than one corpus split");
            }
        }
    }
    return output;
}

void validateProductionManifest(const std::vector<SourceRecord> &records)
{
    std::array<std::uint64_t, 3> sourceCounts{};
    std::array<std::array<std::uint64_t, 3>, 4> catalogCounts{};
    std::array<std::uint64_t, 3> peopleCounts{};
    std::array<std::uint64_t, 3> astronomyCounts{};
    std::map<std::string, std::size_t> authorCounts;
    static const std::array<std::string, 4> catalogs{{
        "openimages-cvdf-v5-boxable", "pass-v3", "wikimedia-commons",
        "smithsonian-open-access",
    }};
    for (const SourceRecord &record : records) {
        if (!record.selected) continue;
        const unsigned splitIndex = static_cast<unsigned>(record.split) - 1;
        ++sourceCounts[splitIndex];
        if (!record.manifestV2 || record.rightsReviewStatus != "approved") {
            throw std::runtime_error(
                "production source must use v2 provenance with approved rights: "
                + record.sourceId);
        }
        const auto catalog = std::find(catalogs.begin(), catalogs.end(), record.catalogName);
        if (catalog == catalogs.end()) {
            throw std::runtime_error("production source has an unsupported catalog");
        }
        ++catalogCounts[static_cast<std::size_t>(catalog - catalogs.begin())][splitIndex];
        if (++authorCounts[record.authorId] > 5) {
            throw std::runtime_error("production manifest exceeds the five-image author cap");
        }
        if (std::find(record.contentTags.begin(), record.contentTags.end(), "people")
            != record.contentTags.end()) {
            if (record.peopleReviewStatus != "approved-no-minors-or-sensitive-content") {
                throw std::runtime_error("production people source lacks explicit approval");
            }
            ++peopleCounts[splitIndex];
        }
        if (std::find(record.contentTags.begin(), record.contentTags.end(),
                      "astronomy-star-field") != record.contentTags.end()) {
            ++astronomyCounts[splitIndex];
        }
        const std::size_t required = record.split == CorpusSplit::TRAIN ? 256 : 128;
        if (record.patches.size() != required) {
            throw std::runtime_error("selected source has the wrong frozen patch count: "
                                     + record.sourceId);
        }
        std::set<std::pair<std::uint32_t, std::uint32_t>> positions;
        std::size_t coverage = 0;
        std::size_t identity = 0;
        for (std::size_t index = 0; index < record.patches.size(); ++index) {
            const PatchSelection &patch = record.patches[index];
            if (!positions.emplace(patch.x, patch.y).second || patch.sequence != index) {
                throw std::runtime_error("source has duplicate patches or noncanonical sequence: "
                                         + record.sourceId);
            }
            coverage += patch.coverage;
            const bool isIdentity = patch.augmentationKind == 0;
            identity += isIdentity;
            if (isIdentity && (patch.matrixId != 0 || patch.exposureStopsQ8 != 0
                || patch.whiteBalanceQ12 != std::array<std::uint16_t, 3>{{4096,4096,4096}})) {
                throw std::runtime_error("identity augmentation changes a patch: " + record.sourceId);
            }
            if (!isIdentity && patch.matrixId == 0) {
                throw std::runtime_error("augmented patch lacks a camera matrix: " + record.sourceId);
            }
        }
        if (coverage != required / 4 || identity != required / 4) {
            throw std::runtime_error("source does not have frozen 75/25 coverage and identity ratios: "
                                     + record.sourceId);
        }
    }
    if (sourceCounts != std::array<std::uint64_t, 3>{{4000,500,500}}) {
        throw std::runtime_error(
            "production manifest must contain exactly 4000/500/500 selected sources");
    }
    const std::array<std::array<std::uint64_t, 3>, 4> expectedCatalogs{{
        {{3200,400,400}}, {{0,0,0}}, {{480,60,60}}, {{320,40,40}},
    }};
    if (catalogCounts != expectedCatalogs) {
        throw std::runtime_error("production manifest does not match frozen source quotas");
    }
    if (peopleCounts[0] < 600 || peopleCounts[1] < 75 || peopleCounts[2] < 75) {
        throw std::runtime_error("production manifest lacks the controlled people share");
    }
    if (astronomyCounts[0] < 12 || astronomyCounts[1] < 1
        || astronomyCounts[2] < 1) {
        throw std::runtime_error(
            "production manifest lacks the astronomy star-field guardrail");
    }
}

SourceVerification verifySources(
    const std::vector<SourceRecord> &records,
    const std::string &cacheDirectory)
{
    SourceVerification result;
    for (const SourceRecord &record : records) {
        if (!record.selected) continue;
        ++result.selected;
        const auto path = sourcePath(cacheDirectory, record);
        if (!std::filesystem::is_regular_file(path)) {
            ++result.missing;
            continue;
        }
        if (hex(sha256File(path.string())) == record.sha256) ++result.authenticated;
        else ++result.changed;
    }
    return result;
}

void finalizeSources(
    const std::vector<SourceRecord> &inputRecords,
    const std::string &inputManifest,
    const std::string &cacheDirectory,
    const std::string &outputManifest,
    const FinalizationOptions &options,
    bool force)
{
    if (options.progressSeconds == 0) {
        throw std::runtime_error("finalization progress interval must be positive");
    }
    if (std::filesystem::exists(outputManifest) && !force) {
        throw std::runtime_error("refusing to replace source manifest");
    }
    std::vector<SourceRecord> records = inputRecords;
    const auto manifestRecords = readSourceManifest(inputManifest);
    if (manifestRecords.size() != records.size()) {
        throw std::runtime_error("finalization records do not match the input manifest");
    }
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (canonicalSourceRecordV2(manifestRecords[index])
            != canonicalSourceRecordV2(records[index])) {
            throw std::runtime_error("finalization records do not match the input manifest");
        }
    }
    for (const SourceRecord &record : records) {
        if (!record.manifestV2 || !record.selected || !record.splitAssigned) {
            throw std::runtime_error("corpus finalization requires selected assigned v2 records");
        }
    }
    const std::filesystem::path workDirectory = options.workDirectory.empty()
        ? std::filesystem::path(outputManifest + ".work")
        : std::filesystem::path(options.workDirectory);
    std::filesystem::create_directories(workDirectory);
    const auto bindingPath = workDirectory / "binding.txt";
    const auto progressPath = workDirectory / "progress.json";
    if (force) {
        std::error_code ignored;
        std::filesystem::remove(bindingPath, ignored);
        std::filesystem::remove(progressPath, ignored);
        for (const auto &entry : std::filesystem::directory_iterator(workDirectory)) {
            if (entry.is_regular_file()
                && entry.path().filename().string().rfind("record-", 0) == 0) {
                std::filesystem::remove(entry.path(), ignored);
            }
        }
    }
    const std::string binding = finalizationBinding(inputManifest, records.size());
    if (std::filesystem::exists(bindingPath)) {
        std::ifstream input(bindingPath, std::ios::binary);
        const std::string existing{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (input.bad() || existing != binding) {
            throw std::runtime_error("finalization work directory belongs to another input");
        }
    } else {
        durableWrite(bindingPath, binding);
    }

    std::vector<bool> completed(records.size(), false);
    const std::regex pattern("record-([0-9]{8})-([0-9a-f]{64})\\.jsonl");
    for (const auto &entry : std::filesystem::directory_iterator(workDirectory)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".tmp") continue;
        std::smatch match;
        if (!std::regex_match(name, match, pattern)) {
            if (name.rfind("record-", 0) == 0) {
                throw std::runtime_error("finalization work directory contains a malformed record");
            }
            continue;
        }
        const std::size_t ordinal = static_cast<std::size_t>(std::stoull(match[1].str()));
        if (ordinal >= records.size() || completed[ordinal]
            || hex(sha256File(entry.path().string())) != match[2].str()) {
            throw std::runtime_error("finalization checkpoint record authentication failed");
        }
        const auto loaded = readSourceManifest(entry.path().string());
        if (loaded.size() != 1 || loaded[0].sourceId != records[ordinal].sourceId) {
            throw std::runtime_error("finalization checkpoint source identity changed");
        }
        SourceRecord expectedBase = records[ordinal];
        SourceRecord actualBase = loaded[0];
        expectedBase.patches.clear();
        actualBase.patches.clear();
        if (canonicalSourceRecordV2(expectedBase) != canonicalSourceRecordV2(actualBase)) {
            throw std::runtime_error("finalization checkpoint source metadata changed");
        }
        records[ordinal] = loaded[0];
        completed[ordinal] = true;
    }

    std::size_t finished = static_cast<std::size_t>(std::count(
        completed.begin(), completed.end(), true));
    const std::size_t resumedSources = finished;
    const auto started = std::chrono::steady_clock::now();
    auto lastProgress = started;
    auto progress = [&](const char *status, const std::string &error = std::string()) {
        std::ostringstream output;
        output << "{\n  \"completed_sources\": " << finished << ",\n";
        if (!error.empty()) output << "  \"error\": \"" << jsonEscape(error) << "\",\n";
        output << "  \"format\": \"rawtherapee-tgmr-corpus-finalization-progress-v1\",\n"
               << "  \"status\": \"" << status << "\",\n"
               << "  \"total_sources\": " << records.size() << "\n}\n";
        durableWrite(progressPath, output.str());
    };

    for (std::size_t ordinal = 0; ordinal < records.size(); ++ordinal) {
        if (completed[ordinal]) continue;
        try {
            records[ordinal] = finalizeSourceRecord(records[ordinal], cacheDirectory);
            const std::string contents = canonicalSourceRecordV2(records[ordinal]);
            const std::string digest = hex(sha256(contents.data(), contents.size()));
            std::ostringstream name;
            name << "record-" << std::setfill('0') << std::setw(8) << ordinal
                 << '-' << digest << ".jsonl";
            durableWrite(workDirectory / name.str(), contents);
            completed[ordinal] = true;
            ++finished;
        } catch (const std::exception &exception) {
            progress("interrupted", exception.what());
            throw;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastProgress >= std::chrono::seconds(options.progressSeconds)) {
            const double seconds = std::max(1e-9,
                std::chrono::duration<double>(now - started).count());
            const double rate = (finished - resumedSources) / seconds;
            std::cerr << "TGMR finalize: " << finished << '/' << records.size()
                      << " sources checkpointed";
            if (rate > 0.0 && finished < records.size()) {
                std::cerr << ", ETA " << ((records.size() - finished) / rate) << " s";
            }
            std::cerr << '\n';
            progress("running");
            lastProgress = now;
        }
    }
    writeSourceManifestV2(records, outputManifest, force);
    progress("complete");
}

void classifySources(
    const std::vector<SourceRecord> &records,
    const std::string &cacheDirectory,
    const std::string &outputJsonl,
    bool force,
    bool includeUnselected,
    const ClassificationOptions &options)
{
    std::vector<ClassificationTask> tasks;
    for (const SourceRecord &record : records) {
        if (!record.selected && !includeUnselected) continue;
        ClassificationTask task;
        task.ordinal = tasks.size();
        task.sourceId = record.sourceId;
        task.cacheFilename = record.cacheFilename;
        task.expectedSha256 = record.sha256;
        task.path = sourcePath(cacheDirectory, record);
        task.reviewedRecord = &record;
        tasks.push_back(std::move(task));
    }
    runClassificationTasks(tasks, outputJsonl, force, options);
}

void classifyFetchedCandidates(
    const std::string &fetchedCandidateJsonl,
    const std::string &cacheDirectory,
    const std::string &outputJsonl,
    bool force,
    const ClassificationOptions &options)
{
    std::ifstream input(fetchedCandidateJsonl);
    if (!input) throw std::runtime_error("cannot open fetched candidate JSONL");
    std::vector<ClassificationTask> tasks;
    std::set<std::string> identities;
    std::string line;
    std::uint64_t lineNumber = 0;
    const bool cvdfLayout = !options.openImagesCvdfSplit.empty();
    if (cvdfLayout && options.openImagesCvdfSplit != "train"
        && options.openImagesCvdfSplit != "validation"
        && options.openImagesCvdfSplit != "test") {
        throw std::runtime_error("Open Images CVDF split must be train, validation, or test");
    }
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty()) throw std::runtime_error("fetched candidates contain a blank line");
        cJSON *root = cJSON_Parse(line.c_str());
        if (!root || !cJSON_IsObject(root)) {
            cJSON_Delete(root);
            throw std::runtime_error("fetched candidate JSON parse failure on line "
                                     + std::to_string(lineNumber));
        }
        try {
            const std::string expectedFormat = cvdfLayout
                ? "rawtherapee-tgmr-catalog-candidate-v1"
                : "rawtherapee-tgmr-fetched-candidate-v1";
            if (text(root, "format") != expectedFormat) {
                throw std::runtime_error("wrong fetched candidate format");
            }
            const std::string catalog = text(root, "catalog");
            const std::string upstream = text(root, "upstream_source_id");
            if (cvdfLayout && (catalog != "openimages-cvdf-v5-boxable"
                || upstream.size() != 16
                || !std::all_of(upstream.begin(), upstream.end(), [](unsigned char value) {
                    return std::isdigit(value) || (value >= 'a' && value <= 'f');
                }))) {
                throw std::runtime_error("CVDF input is not a canonical Open Images identity");
            }
            std::string sourceId = catalog + ':' + upstream;
            for (char &value : sourceId) {
                const unsigned char byte = static_cast<unsigned char>(value);
                if (std::isalnum(byte) || value == '.' || value == '_'
                    || value == ':' || value == '-') {
                } else {
                    value = '-';
                }
            }
            sourceId.erase(std::unique(sourceId.begin(), sourceId.end(),
                [](char left, char right) { return left == '-' && right == '-'; }),
                sourceId.end());
            while (!sourceId.empty() && sourceId.front() == '-') sourceId.erase(sourceId.begin());
            while (!sourceId.empty() && sourceId.back() == '-') sourceId.pop_back();
            if (sourceId.empty() || !identities.insert(sourceId).second) {
                throw std::runtime_error("duplicate or empty fetched candidate identity");
            }
            const std::string cacheFilename = cvdfLayout
                ? options.openImagesCvdfSplit + '/' + upstream[0] + '/'
                    + upstream[1] + '/' + upstream[2] + '/' + upstream + ".jpg"
                : text(root, "cache_filename");
            const std::string expected = cvdfLayout ? std::string() : text(root, "sha256");
            if (!expected.empty() && !canonicalSha256(expected)) {
                throw std::runtime_error("fetched candidate SHA-256 is malformed");
            }
            ClassificationTask task;
            task.ordinal = tasks.size();
            task.sourceId = sourceId;
            task.cacheFilename = cacheFilename;
            task.expectedSha256 = expected;
            task.path = safeCachePath(cacheDirectory, cacheFilename);
            tasks.push_back(std::move(task));
            cJSON_Delete(root);
        } catch (...) {
            cJSON_Delete(root);
            throw;
        }
    }
    if (!input.eof()) throw std::runtime_error("fetched candidate classification I/O failed");
    runClassificationTasks(tasks, outputJsonl, force, options);
}

void packSources(
    const std::vector<SourceRecord> &records,
    const std::string &manifestPath,
    const std::string &cacheDirectory,
    const std::string &outputTgpc,
    PackNoiseRecipe noiseRecipe,
    bool force)
{
    PackOptions options;
    options.noise = noiseRecipe;
    packSourcesWithOptions(
        records, manifestPath, cacheDirectory, outputTgpc, options, force);
}

void packSourcesWithOptions(
    const std::vector<SourceRecord> &records,
    const std::string &manifestPath,
    const std::string &cacheDirectory,
    const std::string &outputTgpc,
    const PackOptions &options,
    bool force)
{
    if (options.trainingAugmentation == TrainingAugmentationRecipe::IDENTITY_ONLY
        && options.noise != PackNoiseRecipe::NONE) {
        throw std::runtime_error(
            "identity-only training cannot be combined with sensor noise");
    }
    const auto manifestDigest = sha256File(manifestPath);
    const char *configuration = nullptr;
    if (options.trainingAugmentation == TrainingAugmentationRecipe::IDENTITY_ONLY) {
        configuration =
            "tgpc-v1:linear-srgb:7x7:chw:uint16:clip:train-identity:eval-augmentation-v1:noise-none";
    } else if (options.noise == PackNoiseRecipe::SENSOR_V1) {
        configuration =
            "tgpc-v1:linear-srgb:7x7:chw:uint16:clip:camera-matrix:exposure-wb:matrix-set-v1:noise-sensor-v1-train-only-read8-shot64";
    } else {
        // Preserve the original production-v1/no-noise identity byte for byte.
        configuration =
            "tgpc-v1:linear-srgb:7x7:chw:uint16:clip:camera-matrix:exposure-wb:matrix-set-v1:noise-none";
    }
    const std::uint64_t expectedRecords = std::accumulate(
        records.begin(), records.end(), std::uint64_t{0},
        [](std::uint64_t count, const SourceRecord &record) {
            return count + (record.selected ? record.patches.size() : 0U);
        });
    writeCorpusStreamResumable(outputTgpc, manifestDigest,
        sha256(configuration, std::strlen(configuration)),
        expectedRecords,
        [&](std::uint64_t skipRecords, const CorpusRecordSink &sink) {
            std::uint32_t sourceOrdinal = 0;
            std::uint64_t recordOrdinal = 0;
            for (const SourceRecord &record : records) {
                if (!record.selected) continue;
                if (recordOrdinal + record.patches.size() <= skipRecords) {
                    recordOrdinal += record.patches.size();
                    ++sourceOrdinal;
                    continue;
                }
                const auto path = sourcePath(cacheDirectory, record);
                if (hex(sha256File(path.string())) != record.sha256) {
                    throw std::runtime_error("source changed before packing: " + record.sourceId);
                }
                const LinearImage image = loadLinearImage(path.string());
                const ImageClassification classification = classifyImage(image);
                verifyDecodedMetadata(record, image, classification, true);
                for (const PatchSelection &selection : record.patches) {
                    if (recordOrdinal++ < skipRecords) continue;
                    if (selection.x + 7 > image.width || selection.y + 7 > image.height) {
                        throw std::runtime_error(
                            "manifest patch is outside decoded image: " + record.sourceId);
                    }
                    const bool identityTraining = record.split == CorpusSplit::TRAIN
                        && options.trainingAugmentation
                            == TrainingAugmentationRecipe::IDENTITY_ONLY;
                    const std::uint16_t matrixId = identityTraining ? 0 : selection.matrixId;
                    const std::int16_t exposureStopsQ8 = identityTraining
                        ? 0 : selection.exposureStopsQ8;
                    const std::array<std::uint16_t, 3> whiteBalanceQ12 = identityTraining
                        ? std::array<std::uint16_t, 3>{{4096,4096,4096}}
                        : selection.whiteBalanceQ12;
                    const std::uint8_t augmentationKind = identityTraining
                        ? 0 : selection.augmentationKind;
                    const CameraMatrix &matrix = cameraMatrix(matrixId);
                    if (matrixId != 0
                        && matrix.heldOut == (record.split == CorpusSplit::TRAIN)) {
                        throw std::runtime_error(
                            "camera-matrix augmentation crosses the train/evaluation boundary");
                    }
                    PatchRecord patch;
                    patch.sourceIdSha256 = sha256(record.sourceId.data(), record.sourceId.size());
                    patch.sourceOrdinal = sourceOrdinal;
                    patch.x = selection.x;
                    patch.y = selection.y;
                    patch.split = record.split;
                    patch.augmentationKind = augmentationKind == 0 ? 0
                        : options.noise == PackNoiseRecipe::SENSOR_V1
                            && record.split == CorpusSplit::TRAIN ? 2 : 1;
                    patch.orientation = static_cast<std::uint8_t>(image.orientation);
                    patch.exposureStopsQ8 = exposureStopsQ8;
                    patch.whiteBalanceQ12 = whiteBalanceQ12;
                    patch.matrixId = matrixId;
                    patch.augmentationSequence = selection.sequence;
                    patch.patchSeed = record.patchSamplingSeed;
                    const double exposure = std::exp2(exposureStopsQ8 / 256.0);
                    for (unsigned y = 0; y < 7; ++y) {
                        for (unsigned x = 0; x < 7; ++x) {
                            const std::size_t input = ((selection.y + y) * image.width
                                + selection.x + x) * 3;
                            for (unsigned channel = 0; channel < 3; ++channel) {
                                double transformed = 0.0;
                                for (unsigned source = 0; source < 3; ++source) {
                                    transformed += matrix.linearSrgbToCamera[channel * 3 + source]
                                        * image.rgb[input + source];
                                }
                                const double value = std::max(0.0, std::min(1.0,
                                    transformed * exposure
                                    * gainFromQ12(whiteBalanceQ12[channel])));
                                const std::size_t sampleIndex = channel * 49 + y * 7 + x;
                                std::uint16_t quantized = static_cast<std::uint16_t>(
                                    std::llround(value * 65535.0));
                                if (options.noise == PackNoiseRecipe::SENSOR_V1
                                    && record.split == CorpusSplit::TRAIN
                                    && augmentationKind != 0) {
                                    quantized = addSensorNoise(
                                        quantized, patch.sourceIdSha256, selection, sampleIndex);
                                }
                                patch.rgb[sampleIndex] = quantized;
                            }
                        }
                    }
                    sink(patch);
                }
                ++sourceOrdinal;
            }
        }, options.work, force);
}

} // namespace tgmr
