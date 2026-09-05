#pragma once

#include "tgmr/sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tgmr
{

constexpr std::uint32_t TGPC_HEADER_BYTES = 256;
constexpr std::uint32_t TGPC_RECORD_BYTES = 384;
constexpr std::uint32_t TGPC_PATCH_EDGE = 7;
constexpr std::uint32_t TGPC_PATCH_VALUES = 7 * 7 * 3;

enum class CorpusSplit : std::uint8_t {
    TRAIN = 1,
    VALIDATION = 2,
    TEST = 3,
};

struct CorpusHeader final {
    std::uint64_t recordCount = 0;
    std::array<std::uint64_t, 3> splitCounts{};
    std::array<std::uint8_t, 32> manifestSha256{};
    std::array<std::uint8_t, 32> payloadSha256{};
    std::array<std::uint8_t, 32> configurationSha256{};
};

struct PatchRecord final {
    std::array<std::uint8_t, 32> sourceIdSha256{};
    std::uint32_t sourceOrdinal = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    CorpusSplit split = CorpusSplit::TRAIN;
    std::uint8_t augmentationKind = 0;
    std::uint8_t orientation = 1;
    std::int16_t exposureStopsQ8 = 0;
    std::array<std::uint16_t, 3> whiteBalanceQ12{{4096, 4096, 4096}};
    std::uint16_t matrixId = 0;
    std::uint16_t augmentationSequence = 0;
    std::uint64_t patchSeed = 0;
    std::array<std::uint16_t, TGPC_PATCH_VALUES> rgb{};
};

struct CorpusInspection final {
    CorpusHeader header;
    std::array<std::uint64_t, 3> observedSplitCounts{};
    std::array<std::array<std::uint8_t, 32>, 3> splitPayloadSha256{};
    std::uint64_t fileBytes = 0;
    bool compressed = false;
};

// Write a canonical uncompressed TGPC file.  Publication is atomic and an
// existing destination is never replaced unless force is true.
void writeCorpus(
    const std::string &path,
    const std::array<std::uint8_t, 32> &manifestSha256,
    const std::array<std::uint8_t, 32> &configurationSha256,
    const std::vector<PatchRecord> &records,
    bool force = false);

using CorpusRecordSink = std::function<void(const PatchRecord &)>;
using CorpusRecordProducer = std::function<void(const CorpusRecordSink &)>;
using ResumableCorpusRecordProducer =
    std::function<void(std::uint64_t, const CorpusRecordSink &)>;

struct CorpusWriteOptions final {
    std::uint64_t checkpointRecords = 8192;
    std::uint32_t progressSeconds = 15;
    std::string workDirectory;
};

// Streaming variant used by the production packer.  The producer is invoked
// once and must emit records in canonical order.  Only the current encoded
// record and SHA-256 state are retained in memory.
void writeCorpusStream(
    const std::string &path,
    const std::array<std::uint8_t, 32> &manifestSha256,
    const std::array<std::uint8_t, 32> &configurationSha256,
    const CorpusRecordProducer &producer,
    bool force = false);

// Restartable variant for long production packing runs. The partial TGPC is
// retained beside the destination, while small authenticated checkpoint and
// progress records live in workDirectory. On resume the producer is told how
// many canonical records have already been authenticated and must skip them
// without reopening their source images.
void writeCorpusStreamResumable(
    const std::string &path,
    const std::array<std::uint8_t, 32> &manifestSha256,
    const std::array<std::uint8_t, 32> &configurationSha256,
    std::uint64_t expectedRecords,
    const ResumableCorpusRecordProducer &producer,
    const CorpusWriteOptions &options = CorpusWriteOptions{},
    bool force = false);

// Parse and authenticate a TGPC or TGPC.GZ stream.  The visitor is called in
// canonical record order.  Padding, split counts, file size, and payload hash
// are validated before success is returned.
CorpusInspection inspectCorpus(
    const std::string &path,
    const std::function<void(const PatchRecord &, std::uint64_t)> &visitor = {});

// Produce a deterministic gzip member (mtime=0, no original filename/comment,
// OS=255) without loading the corpus into memory.
void deterministicGzip(
    const std::string &inputPath,
    const std::string &outputPath,
    int level = 9,
    bool force = false);

std::string canonicalInspectionJson(const CorpusInspection &inspection);

} // namespace tgmr
