#include "tgmr/corpus.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <zlib.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace tgmr
{
namespace
{

constexpr std::array<std::uint8_t, 8> MAGIC{{'R', 'T', 'T', 'G', 'P', 'C', '1', 0}};
constexpr std::uint16_t MAJOR = 1;
constexpr std::uint16_t MINOR = 0;
constexpr std::uint32_t SCALAR_UINT16_LE = 1;
constexpr std::uint64_t MAX_RECORDS = 16'000'000;

void put16(std::uint8_t *destination, std::uint16_t value)
{
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::uint8_t *destination, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) {
        destination[i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

void put64(std::uint8_t *destination, std::uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) {
        destination[i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

std::uint16_t get16(const std::uint8_t *source)
{
    return static_cast<std::uint16_t>(source[0])
        | (static_cast<std::uint16_t>(source[1]) << 8);
}

std::uint32_t get32(const std::uint8_t *source)
{
    return static_cast<std::uint32_t>(source[0])
        | (static_cast<std::uint32_t>(source[1]) << 8)
        | (static_cast<std::uint32_t>(source[2]) << 16)
        | (static_cast<std::uint32_t>(source[3]) << 24);
}

std::uint64_t get64(const std::uint8_t *source)
{
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        result |= static_cast<std::uint64_t>(source[i]) << (8 * i);
    }
    return result;
}

bool allZero(const std::uint8_t *data, std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i) {
        if (data[i] != 0) {
            return false;
        }
    }
    return true;
}

bool endsWith(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

class Input final
{
public:
    explicit Input(const std::string &path) : compressed_(endsWith(path, ".gz"))
    {
        if (compressed_) {
            gzip_ = gzopen(path.c_str(), "rb");
            if (!gzip_) {
                throw std::runtime_error("cannot open gzip corpus: " + path);
            }
        } else {
            file_ = std::fopen(path.c_str(), "rb");
            if (!file_) {
                throw std::runtime_error("cannot open corpus: " + path);
            }
        }
    }

    ~Input()
    {
        if (gzip_) {
            gzclose(gzip_);
        }
        if (file_) {
            std::fclose(file_);
        }
    }

    void exact(void *destination, std::size_t size)
    {
        auto *output = static_cast<std::uint8_t *>(destination);
        while (size != 0) {
            const unsigned request = static_cast<unsigned>(std::min<std::size_t>(
                size, static_cast<std::size_t>(std::numeric_limits<int>::max())));
            const int amount = compressed_
                ? gzread(gzip_, output, request)
                : static_cast<int>(std::fread(output, 1, request, file_));
            if (amount <= 0) {
                throw std::runtime_error("truncated TGPC stream");
            }
            output += amount;
            size -= static_cast<std::size_t>(amount);
            uncompressedBytes_ += static_cast<std::size_t>(amount);
        }
    }

    bool atEnd()
    {
        std::uint8_t value = 0;
        const int amount = compressed_
            ? gzread(gzip_, &value, 1)
            : static_cast<int>(std::fread(&value, 1, 1, file_));
        if (amount > 0) {
            return false;
        }
        if (compressed_) {
            int code = Z_OK;
            gzerror(gzip_, &code);
            if (code != Z_OK && code != Z_STREAM_END) {
                throw std::runtime_error("gzip corpus read failed");
            }
        } else if (std::ferror(file_)) {
            throw std::runtime_error("corpus read failed");
        }
        return true;
    }

    std::uint64_t bytes() const { return uncompressedBytes_; }
    bool compressed() const { return compressed_; }

private:
    bool compressed_ = false;
    std::FILE *file_ = nullptr;
    gzFile gzip_ = nullptr;
    std::uint64_t uncompressedBytes_ = 0;
};

std::array<std::uint8_t, TGPC_RECORD_BYTES> encodeRecord(const PatchRecord &record)
{
    std::array<std::uint8_t, TGPC_RECORD_BYTES> output{};
    std::copy(record.sourceIdSha256.begin(), record.sourceIdSha256.end(), output.begin());
    put32(output.data() + 32, record.sourceOrdinal);
    put32(output.data() + 36, record.x);
    put32(output.data() + 40, record.y);
    output[44] = static_cast<std::uint8_t>(record.split);
    output[45] = record.augmentationKind;
    output[46] = record.orientation;
    put16(output.data() + 48, static_cast<std::uint16_t>(record.exposureStopsQ8));
    for (unsigned channel = 0; channel < 3; ++channel) {
        put16(output.data() + 50 + 2 * channel, record.whiteBalanceQ12[channel]);
    }
    put16(output.data() + 56, record.matrixId);
    put16(output.data() + 58, record.augmentationSequence);
    put64(output.data() + 60, record.patchSeed);
    for (std::size_t i = 0; i < record.rgb.size(); ++i) {
        put16(output.data() + 68 + 2 * i, record.rgb[i]);
    }
    return output;
}

PatchRecord decodeRecord(const std::array<std::uint8_t, TGPC_RECORD_BYTES> &input)
{
    if (!allZero(input.data() + 362, 22) || input[47] != 0) {
        throw std::runtime_error("TGPC record contains nonzero reserved bytes");
    }
    PatchRecord output;
    std::copy(input.begin(), input.begin() + 32, output.sourceIdSha256.begin());
    output.sourceOrdinal = get32(input.data() + 32);
    output.x = get32(input.data() + 36);
    output.y = get32(input.data() + 40);
    if (input[44] < 1 || input[44] > 3 || input[46] < 1 || input[46] > 8) {
        throw std::runtime_error("TGPC record has an invalid split or orientation");
    }
    output.split = static_cast<CorpusSplit>(input[44]);
    output.augmentationKind = input[45];
    output.orientation = input[46];
    output.exposureStopsQ8 = static_cast<std::int16_t>(get16(input.data() + 48));
    for (unsigned channel = 0; channel < 3; ++channel) {
        output.whiteBalanceQ12[channel] = get16(input.data() + 50 + 2 * channel);
        if (output.whiteBalanceQ12[channel] == 0) {
            throw std::runtime_error("TGPC record has a zero white-balance gain");
        }
    }
    output.matrixId = get16(input.data() + 56);
    output.augmentationSequence = get16(input.data() + 58);
    output.patchSeed = get64(input.data() + 60);
    for (std::size_t i = 0; i < output.rgb.size(); ++i) {
        output.rgb[i] = get16(input.data() + 68 + 2 * i);
    }
    return output;
}

std::array<std::uint8_t, TGPC_HEADER_BYTES> encodeHeader(const CorpusHeader &header)
{
    std::array<std::uint8_t, TGPC_HEADER_BYTES> output{};
    std::copy(MAGIC.begin(), MAGIC.end(), output.begin());
    put16(output.data() + 8, MAJOR);
    put16(output.data() + 10, MINOR);
    put32(output.data() + 12, TGPC_HEADER_BYTES);
    put32(output.data() + 16, TGPC_RECORD_BYTES);
    put32(output.data() + 20, TGPC_PATCH_EDGE);
    put32(output.data() + 24, 3);
    put32(output.data() + 28, SCALAR_UINT16_LE);
    put64(output.data() + 40, header.recordCount);
    for (unsigned split = 0; split < 3; ++split) {
        put64(output.data() + 48 + split * 8, header.splitCounts[split]);
    }
    put64(output.data() + 72, TGPC_HEADER_BYTES);
    put64(output.data() + 80, header.recordCount * TGPC_RECORD_BYTES);
    put64(output.data() + 88,
          TGPC_HEADER_BYTES + header.recordCount * TGPC_RECORD_BYTES);
    std::copy(header.manifestSha256.begin(), header.manifestSha256.end(), output.begin() + 96);
    std::copy(header.payloadSha256.begin(), header.payloadSha256.end(), output.begin() + 128);
    std::copy(header.configurationSha256.begin(), header.configurationSha256.end(), output.begin() + 160);
    return output;
}

CorpusHeader decodeHeader(const std::array<std::uint8_t, TGPC_HEADER_BYTES> &input)
{
    if (!std::equal(MAGIC.begin(), MAGIC.end(), input.begin())) {
        throw std::runtime_error("wrong TGPC magic");
    }
    if (get16(input.data() + 8) != MAJOR || get16(input.data() + 10) != MINOR
        || get32(input.data() + 12) != TGPC_HEADER_BYTES
        || get32(input.data() + 16) != TGPC_RECORD_BYTES
        || get32(input.data() + 20) != TGPC_PATCH_EDGE
        || get32(input.data() + 24) != 3
        || get32(input.data() + 28) != SCALAR_UINT16_LE
        || get32(input.data() + 32) != 0
        || get32(input.data() + 36) != 0) {
        throw std::runtime_error("unsupported TGPC contract");
    }
    CorpusHeader output;
    output.recordCount = get64(input.data() + 40);
    if (output.recordCount > MAX_RECORDS) {
        throw std::runtime_error("TGPC record count exceeds the reviewed limit");
    }
    for (unsigned split = 0; split < 3; ++split) {
        output.splitCounts[split] = get64(input.data() + 48 + split * 8);
    }
    if (get64(input.data() + 72) != TGPC_HEADER_BYTES
        || get64(input.data() + 80) != output.recordCount * TGPC_RECORD_BYTES
        || get64(input.data() + 88)
            != TGPC_HEADER_BYTES + output.recordCount * TGPC_RECORD_BYTES
        || !allZero(input.data() + 192, 64)) {
        throw std::runtime_error("TGPC layout is noncanonical");
    }
    std::copy(input.begin() + 96, input.begin() + 128, output.manifestSha256.begin());
    std::copy(input.begin() + 128, input.begin() + 160, output.payloadSha256.begin());
    std::copy(input.begin() + 160, input.begin() + 192, output.configurationSha256.begin());
    return output;
}

std::string temporaryName(const std::string &path)
{
    return path + ".tmp";
}

void refuseExisting(const std::string &path, bool force)
{
    std::ifstream existing(path, std::ios::binary);
    if (existing.good() && !force) {
        throw std::runtime_error("refusing to replace existing output: " + path);
    }
}

void publish(const std::string &temporary, const std::string &destination, bool force)
{
    if (force) std::remove(destination.c_str());
    if (std::rename(temporary.c_str(), destination.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot publish output " + destination + ": "
                                 + std::strerror(errno));
    }
}

void syncFile(std::FILE *file, const char *description)
{
    if (std::fflush(file) != 0) {
        throw std::runtime_error(std::string("cannot flush ") + description);
    }
#if defined(_WIN32)
    if (_commit(_fileno(file)) != 0) {
#else
    if (fsync(fileno(file)) != 0) {
#endif
        throw std::runtime_error(std::string("cannot synchronize ") + description);
    }
}

void durableText(const std::filesystem::path &path, const std::string &contents)
{
    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    std::FILE *file = std::fopen(temporary.string().c_str(), "wb");
    if (!file) throw std::runtime_error("cannot create corpus checkpoint state");
    bool okay = contents.empty()
        || std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
    try {
        if (okay) syncFile(file, "corpus checkpoint state");
    } catch (...) {
        std::fclose(file);
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    okay = okay && std::fclose(file) == 0;
    if (!okay) {
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("cannot write corpus checkpoint state");
    }
#if defined(_WIN32)
    std::filesystem::remove(path, ignored);
#endif
    std::filesystem::rename(temporary, path);
}

std::string resumeBinding(
    const std::array<std::uint8_t, 32> &manifest,
    const std::array<std::uint8_t, 32> &configuration,
    std::uint64_t expectedRecords)
{
    std::ostringstream output;
    output << "rawtherapee-tgmr-corpus-pack-work-v1\n"
           << "manifest " << hex(manifest) << '\n'
           << "configuration " << hex(configuration) << '\n'
           << "records " << expectedRecords << '\n';
    return output.str();
}

struct CorpusResumeState final {
    std::uint64_t records = 0;
    std::uint64_t bytes = TGPC_HEADER_BYTES;
    std::array<std::uint64_t, 3> splits{};
    std::string payloadSha256;
};

std::string checkpointText(const CorpusResumeState &state)
{
    std::ostringstream output;
    output << "rawtherapee-tgmr-corpus-pack-checkpoint-v1\n"
           << "records " << state.records << '\n'
           << "bytes " << state.bytes << '\n'
           << "payload " << state.payloadSha256 << '\n'
           << "splits " << state.splits[0] << ' ' << state.splits[1] << ' '
           << state.splits[2] << '\n';
    return output.str();
}

CorpusResumeState readCheckpoint(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    std::string recordsLabel;
    std::string bytesLabel;
    std::string payloadLabel;
    std::string splitsLabel;
    CorpusResumeState state;
    if (!(input >> magic >> recordsLabel >> state.records >> bytesLabel >> state.bytes
          >> payloadLabel >> state.payloadSha256 >> splitsLabel
          >> state.splits[0] >> state.splits[1] >> state.splits[2])
        || magic != "rawtherapee-tgmr-corpus-pack-checkpoint-v1"
        || recordsLabel != "records" || bytesLabel != "bytes"
        || payloadLabel != "payload" || splitsLabel != "splits"
        || state.payloadSha256.size() != 64) {
        throw std::runtime_error("corpus pack checkpoint is malformed");
    }
    std::string trailing;
    if (input >> trailing) throw std::runtime_error("corpus pack checkpoint has trailing data");
    (void)parseSha256(state.payloadSha256);
    return state;
}

struct RebuiltPayload final {
    Sha256 digest;
    std::array<std::uint64_t, 3> splits{};
};

RebuiltPayload rebuildPartialPayload(
    const std::string &partPath,
    const CorpusResumeState &checkpoint)
{
    if (checkpoint.records > MAX_RECORDS
        || checkpoint.bytes != TGPC_HEADER_BYTES
            + checkpoint.records * TGPC_RECORD_BYTES) {
        throw std::runtime_error("corpus pack checkpoint range is invalid");
    }
    std::error_code error;
    const std::uint64_t actual = std::filesystem::file_size(partPath, error);
    if (error || actual < checkpoint.bytes) {
        throw std::runtime_error("corpus pack partial file is missing or truncated");
    }
    if (actual != checkpoint.bytes) {
        std::filesystem::resize_file(partPath, checkpoint.bytes);
    }
    std::ifstream input(partPath, std::ios::binary);
    std::array<std::uint8_t, TGPC_HEADER_BYTES> header{};
    input.read(reinterpret_cast<char *>(header.data()), header.size());
    if (!input) throw std::runtime_error("corpus pack partial header is truncated");
    // The header is derived from authenticated checkpoint state at completion.
    // Discard any zero, partial, or already-complete header left by an
    // interruption and rebuild it only after the payload has been revalidated.
    RebuiltPayload output;
    for (std::uint64_t index = 0; index < checkpoint.records; ++index) {
        std::array<std::uint8_t, TGPC_RECORD_BYTES> encoded{};
        input.read(reinterpret_cast<char *>(encoded.data()), encoded.size());
        if (!input) throw std::runtime_error("corpus pack partial payload is truncated");
        const PatchRecord record = decodeRecord(encoded);
        const unsigned split = static_cast<unsigned>(record.split);
        ++output.splits[split - 1];
        output.digest.update(encoded.data(), encoded.size());
    }
    Sha256 snapshot = output.digest;
    if (output.splits != checkpoint.splits
        || hex(snapshot.finish()) != checkpoint.payloadSha256) {
        throw std::runtime_error("corpus pack partial payload authentication failed");
    }
    return output;
}

} // namespace

void writeCorpus(
    const std::string &path,
    const std::array<std::uint8_t, 32> &manifestSha256,
    const std::array<std::uint8_t, 32> &configurationSha256,
    const std::vector<PatchRecord> &records,
    bool force)
{
    writeCorpusStream(path, manifestSha256, configurationSha256,
        [&](const CorpusRecordSink &sink) {
            for (const PatchRecord &record : records) sink(record);
        }, force);
}

void writeCorpusStream(
    const std::string &path,
    const std::array<std::uint8_t, 32> &manifestSha256,
    const std::array<std::uint8_t, 32> &configurationSha256,
    const CorpusRecordProducer &producer,
    bool force)
{
    if (!producer) throw std::runtime_error("TGPC record producer is empty");
    refuseExisting(path, force);
    const std::string temporary = temporaryName(path);
    std::FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
        throw std::runtime_error("cannot create TGPC output: " + temporary);
    }
    std::array<std::uint8_t, TGPC_HEADER_BYTES> placeholder{};
    if (std::fwrite(placeholder.data(), 1, placeholder.size(), file) != placeholder.size()) {
        std::fclose(file);
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot reserve TGPC header");
    }
    CorpusHeader header;
    header.manifestSha256 = manifestSha256;
    header.configurationSha256 = configurationSha256;
    Sha256 payload;
    bool writeFailed = false;
    try {
        producer([&](const PatchRecord &record) {
            if (header.recordCount >= MAX_RECORDS) {
                throw std::runtime_error("too many TGPC records");
            }
            const unsigned split = static_cast<unsigned>(record.split);
            if (split < 1 || split > 3) {
                throw std::runtime_error("invalid TGPC split while writing");
            }
            const auto encoded = encodeRecord(record);
            if (std::fwrite(encoded.data(), 1, encoded.size(), file) != encoded.size()) {
                writeFailed = true;
                throw std::runtime_error("TGPC record write failed");
            }
            payload.update(encoded.data(), encoded.size());
            ++header.splitCounts[split - 1];
            ++header.recordCount;
        });
        header.payloadSha256 = payload.finish();
        const auto headerBytes = encodeHeader(header);
        if (std::fseek(file, 0, SEEK_SET) != 0
            || std::fwrite(headerBytes.data(), 1, headerBytes.size(), file) != headerBytes.size()
            || std::fflush(file) != 0) {
            writeFailed = true;
        }
    } catch (...) {
        std::fclose(file);
        std::remove(temporary.c_str());
        throw;
    }
    if (std::fclose(file) != 0) writeFailed = true;
    if (writeFailed) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot write complete TGPC output");
    }
    publish(temporary, path, force);
}

void writeCorpusStreamResumable(
    const std::string &path,
    const std::array<std::uint8_t, 32> &manifestSha256,
    const std::array<std::uint8_t, 32> &configurationSha256,
    std::uint64_t expectedRecords,
    const ResumableCorpusRecordProducer &producer,
    const CorpusWriteOptions &options,
    bool force)
{
    if (!producer) throw std::runtime_error("TGPC resumable record producer is empty");
    if (expectedRecords == 0 || expectedRecords > MAX_RECORDS
        || options.checkpointRecords == 0 || options.progressSeconds == 0) {
        throw std::runtime_error("invalid resumable TGPC write limits");
    }
    const std::filesystem::path workDirectory = options.workDirectory.empty()
        ? std::filesystem::path(path + ".work")
        : std::filesystem::path(options.workDirectory);
    const std::filesystem::path bindingPath = workDirectory / "binding.txt";
    const std::filesystem::path checkpointPath = workDirectory / "checkpoint.txt";
    const std::filesystem::path progressPath = workDirectory / "progress.json";
    const std::string partPath = temporaryName(path);
    std::filesystem::create_directories(workDirectory);

    if (std::filesystem::exists(path) && !force) {
        throw std::runtime_error("refusing to replace existing output: " + path);
    }
    if (force) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(partPath, ignored);
        std::filesystem::remove(checkpointPath, ignored);
        std::filesystem::remove(progressPath, ignored);
        std::filesystem::remove(bindingPath, ignored);
    }
    const std::string binding = resumeBinding(
        manifestSha256, configurationSha256, expectedRecords);
    if (std::filesystem::exists(bindingPath)) {
        std::ifstream input(bindingPath, std::ios::binary);
        const std::string existing{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (input.bad() || existing != binding) {
            throw std::runtime_error("corpus pack work directory belongs to another input");
        }
    } else {
        durableText(bindingPath, binding);
    }

    CorpusResumeState state;
    Sha256 payload;
    if (std::filesystem::exists(checkpointPath)) {
        state = readCheckpoint(checkpointPath);
        if (state.records > expectedRecords) {
            throw std::runtime_error("corpus pack checkpoint exceeds expected records");
        }
        RebuiltPayload rebuilt = rebuildPartialPayload(partPath, state);
        payload = rebuilt.digest;
    } else {
        std::error_code ignored;
        std::filesystem::remove(partPath, ignored);
        std::FILE *initial = std::fopen(partPath.c_str(), "wb");
        if (!initial) throw std::runtime_error("cannot create resumable TGPC output");
        std::array<std::uint8_t, TGPC_HEADER_BYTES> placeholder{};
        const bool wrote = std::fwrite(
            placeholder.data(), 1, placeholder.size(), initial) == placeholder.size();
        try {
            if (wrote) syncFile(initial, "resumable TGPC header");
        } catch (...) {
            std::fclose(initial);
            throw;
        }
        const bool closed = std::fclose(initial) == 0;
        if (!wrote || !closed) throw std::runtime_error("cannot initialize resumable TGPC output");
    }

    std::FILE *file = std::fopen(partPath.c_str(), "r+b");
    if (!file || std::fseek(file, 0, SEEK_END) != 0) {
        if (file) std::fclose(file);
        throw std::runtime_error("cannot reopen resumable TGPC output");
    }
    const auto started = std::chrono::steady_clock::now();
    const std::uint64_t resumedRecords = state.records;
    auto lastCheckpoint = started;
    auto lastProgress = started;
    std::uint64_t lastCheckpointRecords = state.records;

    auto publishProgress = [&](const char *status) {
        std::ostringstream output;
        output << "{\n  \"completed_records\": " << state.records << ",\n"
               << "  \"expected_records\": " << expectedRecords << ",\n"
               << "  \"format\": \"rawtherapee-tgmr-corpus-pack-progress-v1\",\n"
               << "  \"status\": \"" << status << "\"\n}\n";
        durableText(progressPath, output.str());
    };
    auto checkpoint = [&]() {
        syncFile(file, "resumable TGPC payload");
        Sha256 snapshot = payload;
        state.bytes = TGPC_HEADER_BYTES + state.records * TGPC_RECORD_BYTES;
        state.payloadSha256 = hex(snapshot.finish());
        durableText(checkpointPath, checkpointText(state));
        lastCheckpointRecords = state.records;
        lastCheckpoint = std::chrono::steady_clock::now();
    };
    try {
        producer(state.records, [&](const PatchRecord &record) {
            if (state.records >= expectedRecords) {
                throw std::runtime_error("TGPC producer emitted too many records");
            }
            const unsigned split = static_cast<unsigned>(record.split);
            if (split < 1 || split > 3) {
                throw std::runtime_error("invalid TGPC split while writing");
            }
            const auto encoded = encodeRecord(record);
            if (std::fwrite(encoded.data(), 1, encoded.size(), file) != encoded.size()) {
                throw std::runtime_error("resumable TGPC record write failed");
            }
            payload.update(encoded.data(), encoded.size());
            ++state.splits[split - 1];
            ++state.records;
            const auto now = std::chrono::steady_clock::now();
            if (state.records - lastCheckpointRecords >= options.checkpointRecords
                || now - lastCheckpoint >= std::chrono::seconds(options.progressSeconds)) {
                checkpoint();
            }
            if (now - lastProgress >= std::chrono::seconds(options.progressSeconds)) {
                const double seconds = std::max(1e-9,
                    std::chrono::duration<double>(now - started).count());
                const double rate = (state.records - resumedRecords) / seconds;
                std::cerr << "TGMR pack: " << state.records << '/' << expectedRecords
                          << " records written";
                if (rate > 0.0 && state.records < expectedRecords) {
                    std::cerr << ", ETA " << ((expectedRecords - state.records) / rate) << " s";
                }
                std::cerr << '\n';
                publishProgress("running");
                lastProgress = now;
            }
        });
        if (state.records != expectedRecords) {
            throw std::runtime_error("TGPC producer emitted the wrong record count");
        }
        checkpoint();
        CorpusHeader header;
        header.recordCount = state.records;
        header.splitCounts = state.splits;
        header.manifestSha256 = manifestSha256;
        header.configurationSha256 = configurationSha256;
        header.payloadSha256 = parseSha256(state.payloadSha256);
        const auto encodedHeader = encodeHeader(header);
        if (std::fseek(file, 0, SEEK_SET) != 0
            || std::fwrite(encodedHeader.data(), 1, encodedHeader.size(), file)
                != encodedHeader.size()) {
            throw std::runtime_error("cannot publish resumable TGPC header");
        }
        syncFile(file, "complete TGPC output");
        if (std::fclose(file) != 0) {
            file = nullptr;
            throw std::runtime_error("cannot close complete TGPC output");
        }
        file = nullptr;
        if (std::rename(partPath.c_str(), path.c_str()) != 0) {
            throw std::runtime_error("cannot publish completed TGPC output: "
                                     + std::string(std::strerror(errno)));
        }
        std::error_code ignored;
        std::filesystem::remove(checkpointPath, ignored);
        publishProgress("complete");
    } catch (...) {
        try {
            if (file && state.records != lastCheckpointRecords) checkpoint();
            publishProgress("interrupted");
        } catch (...) {
        }
        if (file) std::fclose(file);
        throw;
    }
}

CorpusInspection inspectCorpus(
    const std::string &path,
    const std::function<void(const PatchRecord &, std::uint64_t)> &visitor)
{
    Input input(path);
    std::array<std::uint8_t, TGPC_HEADER_BYTES> headerBytes{};
    input.exact(headerBytes.data(), headerBytes.size());
    CorpusInspection inspection;
    inspection.header = decodeHeader(headerBytes);
    inspection.compressed = input.compressed();
    Sha256 payload;
    std::array<Sha256, 3> splitPayload;
    for (std::uint64_t index = 0; index < inspection.header.recordCount; ++index) {
        std::array<std::uint8_t, TGPC_RECORD_BYTES> bytes{};
        input.exact(bytes.data(), bytes.size());
        payload.update(bytes.data(), bytes.size());
        const PatchRecord record = decodeRecord(bytes);
        const unsigned split = static_cast<unsigned>(record.split) - 1;
        ++inspection.observedSplitCounts[split];
        splitPayload[split].update(bytes.data(), bytes.size());
        if (visitor) {
            visitor(record, index);
        }
    }
    if (!input.atEnd()) {
        throw std::runtime_error("TGPC stream contains trailing bytes");
    }
    inspection.fileBytes = input.bytes();
    if (inspection.observedSplitCounts != inspection.header.splitCounts) {
        throw std::runtime_error("TGPC split counts do not match its records");
    }
    if (payload.finish() != inspection.header.payloadSha256) {
        throw std::runtime_error("TGPC payload SHA-256 mismatch");
    }
    for (unsigned split = 0; split < splitPayload.size(); ++split) {
        inspection.splitPayloadSha256[split] = splitPayload[split].finish();
    }
    return inspection;
}

void deterministicGzip(
    const std::string &inputPath,
    const std::string &outputPath,
    int level,
    bool force)
{
    if (level < 0 || level > 9) {
        throw std::runtime_error("gzip level must be between 0 and 9");
    }
    refuseExisting(outputPath, force);
    std::FILE *input = std::fopen(inputPath.c_str(), "rb");
    if (!input) {
        throw std::runtime_error("cannot open gzip input: " + inputPath);
    }
    const std::string temporary = temporaryName(outputPath);
    std::FILE *output = std::fopen(temporary.c_str(), "wb");
    if (!output) {
        std::fclose(input);
        throw std::runtime_error("cannot create gzip output: " + temporary);
    }
    z_stream stream{};
    if (deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::fclose(input);
        std::fclose(output);
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot initialize deterministic gzip encoder");
    }
    gz_header gzipHeader{};
    gzipHeader.time = 0;
    gzipHeader.os = 255;
    if (deflateSetHeader(&stream, &gzipHeader) != Z_OK) {
        deflateEnd(&stream);
        std::fclose(input);
        std::fclose(output);
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot set canonical gzip header");
    }
    std::array<std::uint8_t, 1 << 16> source{};
    std::array<std::uint8_t, 1 << 16> destination{};
    bool ok = true;
    int flush = Z_NO_FLUSH;
    do {
        stream.avail_in = static_cast<uInt>(std::fread(source.data(), 1, source.size(), input));
        if (std::ferror(input)) {
            ok = false;
            break;
        }
        stream.next_in = source.data();
        flush = std::feof(input) ? Z_FINISH : Z_NO_FLUSH;
        do {
            stream.avail_out = static_cast<uInt>(destination.size());
            stream.next_out = destination.data();
            const int code = deflate(&stream, flush);
            if (code != Z_OK && code != Z_STREAM_END) {
                ok = false;
                break;
            }
            const std::size_t produced = destination.size() - stream.avail_out;
            if (std::fwrite(destination.data(), 1, produced, output) != produced) {
                ok = false;
                break;
            }
        } while (stream.avail_out == 0);
    } while (ok && flush != Z_FINISH);
    if (deflateEnd(&stream) != Z_OK) ok = false;
    if (std::fclose(input) != 0) ok = false;
    if (std::fflush(output) != 0) ok = false;
    if (std::fclose(output) != 0) ok = false;
    if (!ok) {
        std::remove(temporary.c_str());
        throw std::runtime_error("deterministic gzip encoding failed");
    }
    publish(temporary, outputPath, force);
}

std::string canonicalInspectionJson(const CorpusInspection &inspection)
{
    std::ostringstream output;
    output << "{\n"
        << "  \"compressed\": " << (inspection.compressed ? "true" : "false") << ",\n"
        << "  \"configuration_sha256\": \"" << hex(inspection.header.configurationSha256) << "\",\n"
        << "  \"format\": \"rawtherapee-tgpc-v1\",\n"
        << "  \"manifest_sha256\": \"" << hex(inspection.header.manifestSha256) << "\",\n"
        << "  \"patch\": {\"channels\": 3, \"edge\": 7, \"scalar\": \"uint16-le\"},\n"
        << "  \"payload_sha256\": \"" << hex(inspection.header.payloadSha256) << "\",\n"
        << "  \"record_count\": " << inspection.header.recordCount << ",\n"
        << "  \"record_size\": " << TGPC_RECORD_BYTES << ",\n"
        << "  \"split_counts\": {\"test\": " << inspection.header.splitCounts[2]
        << ", \"train\": " << inspection.header.splitCounts[0]
        << ", \"validation\": " << inspection.header.splitCounts[1] << "},\n"
        << "  \"split_payload_sha256\": {\"test\": \""
        << hex(inspection.splitPayloadSha256[2]) << "\", \"train\": \""
        << hex(inspection.splitPayloadSha256[0]) << "\", \"validation\": \""
        << hex(inspection.splitPayloadSha256[1]) << "\"},\n"
        << "  \"uncompressed_bytes\": " << inspection.fileBytes << "\n"
        << "}\n";
    return output.str();
}

} // namespace tgmr
