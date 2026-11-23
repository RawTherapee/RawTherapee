#include "xtrans_tgmr.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glibmm/checksum.h>
#include <glib/gstdio.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#if (defined(__GNUC__) || defined(__clang__)) && \
        (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#define RT_TGMR_HAS_TARGET_AVX2 1
#define RT_TGMR_TARGET_AVX2 __attribute__((target("avx2,fma")))
#else
#define RT_TGMR_HAS_TARGET_AVX2 0
#define RT_TGMR_TARGET_AVX2
#endif

#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#include <arm_neon.h>
#define RT_TGMR_HAS_NEON 1
#else
#define RT_TGMR_HAS_NEON 0
#endif

namespace rtengine
{
namespace
{

constexpr unsigned PATCH = 7;
constexpr unsigned AREA = PATCH * PATCH;
constexpr unsigned PHASES = 18;
constexpr unsigned COMPONENTS = 32;
constexpr unsigned SUPPORT = 3;
constexpr unsigned SUPPORT_AREA = SUPPORT * SUPPORT;
constexpr unsigned SHORTLIST = 8;
constexpr unsigned GROUPS = COMPONENTS / 8;
constexpr unsigned TILE = 128;
constexpr unsigned CHUNK = 512;
constexpr float SOURCE_SCALE = 65535.f;
constexpr std::size_t LEGACY_MODEL_BYTES = 6073164;
constexpr std::size_t PHASE_PAYLOAD_BYTES = LEGACY_MODEL_BYTES - 36;
constexpr const char *LEGACY_MODEL_SHA256 =
    "6279b6a593ef4b595b1aff246182682b60c7eea701373ee2eb9f80cfcb50485c";
constexpr std::size_t V2_HEADER_BYTES = 512;
constexpr std::size_t V2_DIRECTORY_BYTES = 96;
constexpr std::size_t V2_PAYLOAD_OFFSET = 640;
constexpr std::size_t V2_AUTHENTICATION_OFFSET = 352;
constexpr std::size_t V2_AUTHENTICATION_BYTES = 32;
constexpr std::size_t MAX_MODEL_BYTES = 64 * 1024 * 1024;
// Pinned by the experimental build to the frozen corpus/trainer artifact.
// Redistribution review is a separate release requirement. An explicit
// override may use any structurally valid v2 artifact; installed-data
// discovery accepts only this reviewed whole-file identity.
#ifndef RT_TGMR_OFFICIAL_V2_SHA256
#define RT_TGMR_OFFICIAL_V2_SHA256 ""
#endif
constexpr const char *OFFICIAL_V2_SHA256 = RT_TGMR_OFFICIAL_V2_SHA256;
std::mutex tgmrDataDirectoryMutex;
std::string tgmrDataDirectory;

class TgmrFailure final : public std::runtime_error
{
public:
    TgmrFailure(TgmrXTransErrorCode code, const std::string &message) :
        std::runtime_error(message), code(code)
    {
    }

    TgmrXTransErrorCode code;
};

struct Phase final {
    std::array<std::uint32_t, AREA> observed {{}};
    std::uint32_t sampled = 0;
    std::array<std::uint32_t, 2> targets {{}};
    std::array<float, COMPONENTS> logWeights {{}};
    std::vector<float> meansObserved;
    std::vector<float> meansTarget;
    std::vector<float> fullCholesky;
    std::array<float, COMPONENTS> fullLogdet {{}};
    std::vector<float> gains;
    std::array<std::uint32_t, SUPPORT_AREA> coarsePositions {{}};
    std::vector<float> coarseCholesky;
    std::array<float, COMPONENTS> coarseLogdet {{}};
};

struct ModelData final {
    std::array<Phase, PHASES> phases;
};

struct ParsedModel final {
    ModelData data;
    std::string digest;
    std::string origin;
    bool official = false;
};

struct PreparedPhase final {
    const Phase *source = nullptr;
    unsigned measuredPosition = 0;
    std::array<unsigned char, AREA> channels {{}};
    std::array<unsigned char, 3> channelCounts {{}};
    std::array<float, COMPONENTS> coarseScale {{}};
    std::array<float, COMPONENTS> fullScale {{}};
    std::vector<float> coarseMeans;
    std::vector<float> coarseLower;
    std::vector<float> coarseInvDiagonal;
    std::vector<float> coarseScaleAosoa;
    std::vector<float> fullInvDiagonal;
};

struct PreparedModel final {
    std::array<PreparedPhase, PHASES> phases;
    std::size_t cacheBytes = 0;
};

struct PixelWork final {
    std::array<float, AREA> centered {{}};
    std::array<unsigned char, SHORTLIST> ids {{}};
    std::array<float, SHORTLIST> weights {{}};
    std::array<std::array<float, 2>, SHORTLIST> predictions {{}};
    float dc = 0.f;
};

struct Request final {
    std::uint32_t pixel = 0;
    unsigned char slot = 0;

    Request() = default;
    Request(std::uint32_t pixelValue, unsigned char slotValue) :
        pixel(pixelValue), slot(slotValue)
    {
    }
};

struct Coordinate final {
    unsigned x = 0;
    unsigned y = 0;

    Coordinate() = default;
    Coordinate(unsigned xValue, unsigned yValue) : x(xValue), y(yValue)
    {
    }
};

struct Reader final {
    explicit Reader(std::vector<unsigned char> input) : bytes(std::move(input))
    {
    }

    void require(std::size_t count) const
    {
        if (offset > bytes.size() || count > bytes.size() - offset) {
            throw TgmrFailure(TgmrXTransErrorCode::FORMAT, "truncated TGMR model");
        }
    }

    std::uint32_t u32()
    {
        require(4);
        const std::uint32_t value =
            static_cast<std::uint32_t>(bytes[offset])
            | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
            | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
            | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        offset += 4;
        return value;
    }

    float f32()
    {
        const std::uint32_t bits = u32();
        float value = 0.f;
        static_assert(sizeof(value) == sizeof(bits), "TGMR requires IEEE-754 float32");
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) {
            throw TgmrFailure(TgmrXTransErrorCode::NONFINITE,
                              "TGMR model contains a non-finite coefficient");
        }
        return value;
    }

    std::vector<unsigned char> bytes;
    std::size_t offset = 0;
};

template <typename Container>
void readFloats(Reader &reader, Container &values)
{
    for (float &value : values) {
        value = reader.f32();
    }
}

std::vector<unsigned char> readFile(const std::string &path)
{
    if (path.empty()) {
        throw TgmrFailure(TgmrXTransErrorCode::IO,
                          "RT_XTRANS_TGMR_MODEL is unset or empty");
    }
    std::FILE *file = g_fopen(path.c_str(), "rb");
    if (!file) {
        throw TgmrFailure(TgmrXTransErrorCode::IO,
                          "cannot open TGMR model: " + path);
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        throw TgmrFailure(TgmrXTransErrorCode::IO,
                          "cannot determine TGMR model size: " + path);
    }
    const long signedSize = std::ftell(file);
    if (signedSize < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        throw TgmrFailure(TgmrXTransErrorCode::IO,
                          "cannot seek TGMR model: " + path);
    }
    const std::size_t size = static_cast<std::size_t>(signedSize);
    if (size < 8 || size > MAX_MODEL_BYTES) {
        std::fclose(file);
        throw TgmrFailure(TgmrXTransErrorCode::SIZE,
                          "TGMR model size is outside the reviewed bounds");
    }
    std::vector<unsigned char> bytes(size);
    const std::size_t count = std::fread(bytes.data(), 1, bytes.size(), file);
    const int extra = std::fgetc(file);
    const int closeResult = std::fclose(file);
    if (count != bytes.size() || extra != EOF || closeResult != 0) {
        throw TgmrFailure(TgmrXTransErrorCode::IO,
                          "short, extended, or changing TGMR model file");
    }
    return bytes;
}

std::string sha256(const std::vector<unsigned char> &bytes)
{
    Glib::Checksum checksum(Glib::Checksum::CHECKSUM_SHA256);
    checksum.update(bytes.data(), bytes.size());
    return checksum.get_string();
}

std::string sha256(const void *data, std::size_t size)
{
    Glib::Checksum checksum(Glib::Checksum::CHECKSUM_SHA256);
    checksum.update(static_cast<const guchar *>(data), size);
    return checksum.get_string();
}

std::uint16_t readU16(const std::vector<unsigned char> &bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT, "truncated TGMR v2 header");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t readU32(const std::vector<unsigned char> &bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT, "truncated TGMR v2 header");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint64_t readU64(const std::vector<unsigned char> &bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 8) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT, "truncated TGMR v2 header");
    }
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        result |= static_cast<std::uint64_t>(bytes[offset + i]) << (8 * i);
    }
    return result;
}

float readF32(const std::vector<unsigned char> &bytes, std::size_t offset)
{
    const std::uint32_t bits = readU32(bytes, offset);
    float result = 0.f;
    std::memcpy(&result, &bits, sizeof(result));
    if (!std::isfinite(result)) {
        throw TgmrFailure(TgmrXTransErrorCode::NONFINITE,
                          "TGMR v2 header contains a non-finite parameter");
    }
    return result;
}

bool allZero(const std::vector<unsigned char> &bytes, std::size_t offset, std::size_t size)
{
    if (offset > bytes.size() || size > bytes.size() - offset) {
        return false;
    }
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[offset + i] != 0) {
            return false;
        }
    }
    return true;
}

ModelData parsePhasePayload(Reader &reader)
{
    ModelData model;
    std::array<std::array<unsigned char, AREA>, PHASES> patterns {{}};
    for (unsigned phaseIndex = 0; phaseIndex < PHASES; ++phaseIndex) {
        Phase &phase = model.phases[phaseIndex];
        std::array<bool, AREA> spatialSeen {{}};
        for (unsigned position = 0; position < AREA; ++position) {
            const std::uint32_t value = reader.u32();
            if (value >= 3 * AREA || spatialSeen[value % AREA]) {
                throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                                  "invalid TGMR observed-index permutation");
            }
            spatialSeen[value % AREA] = true;
            patterns[phaseIndex][value % AREA] = static_cast<unsigned char>(value / AREA);
            phase.observed[position] = value;
        }
        phase.sampled = reader.u32();
        phase.targets[0] = reader.u32();
        phase.targets[1] = reader.u32();
        std::array<bool, 3> channelSeen {{}};
        if (phase.sampled > 2 || phase.targets[0] > 2 || phase.targets[1] > 2) {
            throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                              "invalid TGMR channel contract");
        }
        channelSeen[phase.sampled] = true;
        if (channelSeen[phase.targets[0]] || phase.targets[0] == phase.targets[1]) {
            throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                              "TGMR channel contract is not a permutation");
        }
        channelSeen[phase.targets[0]] = true;
        if (channelSeen[phase.targets[1]]) {
            throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                              "TGMR channel contract is not a permutation");
        }

        readFloats(reader, phase.logWeights);
        phase.meansObserved.resize(COMPONENTS * AREA);
        phase.meansTarget.resize(COMPONENTS * 2);
        phase.fullCholesky.resize(COMPONENTS * AREA * AREA);
        phase.gains.resize(COMPONENTS * 2 * AREA);
        readFloats(reader, phase.meansObserved);
        readFloats(reader, phase.meansTarget);
        readFloats(reader, phase.fullCholesky);
        readFloats(reader, phase.fullLogdet);
        readFloats(reader, phase.gains);
        std::array<bool, AREA> coarseSeen {{}};
        for (std::uint32_t &position : phase.coarsePositions) {
            position = reader.u32();
            if (position >= AREA || coarseSeen[position]) {
                throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                                  "invalid TGMR coarse-support index");
            }
            coarseSeen[position] = true;
        }
        static const std::array<std::uint32_t, SUPPORT_AREA> expectedCoarse =
            {{16, 17, 18, 23, 24, 25, 30, 31, 32}};
        if (phase.coarsePositions != expectedCoarse) {
            throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                              "TGMR coarse support is not the frozen central S9");
        }
        phase.coarseCholesky.resize(COMPONENTS * SUPPORT_AREA * SUPPORT_AREA);
        readFloats(reader, phase.coarseCholesky);
        readFloats(reader, phase.coarseLogdet);
        for (unsigned component = 0; component < COMPONENTS; ++component) {
            for (unsigned row = 0; row < AREA; ++row) {
                if (!(phase.fullCholesky[(component * AREA + row) * AREA + row] > 0.f)) {
                    throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                                      "TGMR full Cholesky diagonal is not positive");
                }
            }
            for (unsigned row = 0; row < SUPPORT_AREA; ++row) {
                if (!(phase.coarseCholesky[
                        (component * SUPPORT_AREA + row) * SUPPORT_AREA + row] > 0.f)) {
                    throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                                      "TGMR coarse Cholesky diagonal is not positive");
                }
            }
        }
        if (patterns[phaseIndex][AREA / 2] != phase.sampled) {
            throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                              "TGMR sampled-center channel is inconsistent");
        }
        for (unsigned previous = 0; previous < phaseIndex; ++previous) {
            if (patterns[phaseIndex] == patterns[previous]) {
                throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                                  "TGMR model contains duplicate phase patterns");
            }
        }
    }
    if (reader.offset != reader.bytes.size()) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                          "noncanonical trailing TGMR model bytes");
    }
    return model;
}

std::string digestSlice(
    const std::vector<unsigned char> &bytes,
    std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 32) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                          "truncated TGMR v2 digest field");
    }
    static const char digits[] = "0123456789abcdef";
    std::string result(64, '0');
    for (unsigned i = 0; i < 32; ++i) {
        result[2 * i] = digits[bytes[offset + i] >> 4];
        result[2 * i + 1] = digits[bytes[offset + i] & 15];
    }
    return result;
}

ParsedModel parseLegacyModel(std::vector<unsigned char> bytes)
{
    if (bytes.size() != LEGACY_MODEL_BYTES) {
        throw TgmrFailure(TgmrXTransErrorCode::SIZE,
                          "legacy TGMR model size is not 6,073,164 bytes");
    }
    const std::string digest = sha256(bytes);
    if (digest != LEGACY_MODEL_SHA256) {
        throw TgmrFailure(TgmrXTransErrorCode::DIGEST,
                          "legacy TGMR model SHA-256 differs from the reviewed research artifact");
    }
    Reader reader(std::move(bytes));
    reader.offset = 8;
    const std::uint32_t version = reader.u32();
    const std::uint32_t patch = reader.u32();
    const std::uint32_t phases = reader.u32();
    const std::uint32_t components = reader.u32();
    const std::uint32_t support = reader.u32();
    const std::uint32_t shortlist = reader.u32();
    const std::uint32_t reserved = reader.u32();
    if (version != 1 || patch != PATCH || phases != PHASES
            || components != COMPONENTS || support != SUPPORT
            || shortlist != SHORTLIST || reserved != 0) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                          "unsupported legacy TGMR model contract");
    }
    ParsedModel result;
    result.data = parsePhasePayload(reader);
    result.digest = digest;
    result.origin = "reviewed-research-v1";
    return result;
}

ParsedModel parseV2Model(std::vector<unsigned char> bytes)
{
    static const char expectedMagic[8] = {'R', 'T', 'T', 'G', 'M', 'R', '2', 0};
    if (bytes.size() != V2_PAYLOAD_OFFSET + PHASE_PAYLOAD_BYTES) {
        throw TgmrFailure(TgmrXTransErrorCode::SIZE,
                          "TGMR v2 file does not contain the frozen K32/S9/q8 payload size");
    }
    if (std::memcmp(bytes.data(), expectedMagic, 8) != 0) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT, "wrong TGMR v2 magic");
    }
    if (readU16(bytes, 8) != 2 || readU16(bytes, 10) != 0
            || readU32(bytes, 12) != V2_HEADER_BYTES
            || readU32(bytes, 16) != 0x01020304 || readU32(bytes, 20) != 0
            || readU32(bytes, 24) != 1 || readU32(bytes, 28) == 0
            || readU32(bytes, 32) != 1 || readU32(bytes, 36) != PATCH
            || readU32(bytes, 40) != PHASES || readU32(bytes, 44) != COMPONENTS
            || readU32(bytes, 48) != SUPPORT || readU32(bytes, 52) != SHORTLIST
            || readU32(bytes, 56) != AREA || readU32(bytes, 60) != 2
            || readF32(bytes, 64) != 3.f || readF32(bytes, 68) != 4.f
            || std::fabs(readF32(bytes, 72) - 0.0003f) > 1e-10f
            || readU32(bytes, 76) != 1 || readU32(bytes, 80) != V2_DIRECTORY_BYTES
            || readU32(bytes, 84) != 0 || readU64(bytes, 88) != V2_HEADER_BYTES
            || readU64(bytes, 96) != V2_DIRECTORY_BYTES
            || readU64(bytes, 104) != V2_PAYLOAD_OFFSET
            || readU64(bytes, 112) != PHASE_PAYLOAD_BYTES
            || readU64(bytes, 120) != bytes.size()) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                          "unsupported TGMR v2 architecture contract");
    }
    static const char schema[] = "rawtherapee-xtrans-tgmr-v2-k32-s9-q8";
    if (digestSlice(bytes, 288) != sha256(schema, sizeof(schema) - 1)
            || allZero(bytes, 128, 32) || allZero(bytes, 160, 32)
            || allZero(bytes, 224, 32) || allZero(bytes, 256, 32)
            || !allZero(bytes, 320, 32) || !allZero(bytes, 384, 128)) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                          "TGMR v2 identity or reserved header fields are invalid");
    }
    const std::size_t directory = V2_HEADER_BYTES;
    if (readU32(bytes, directory) != 1 || readU32(bytes, directory + 4) != 1
            || readU32(bytes, directory + 8) != 0 || readU32(bytes, directory + 12) != 0
            || readU64(bytes, directory + 16) != V2_PAYLOAD_OFFSET
            || readU64(bytes, directory + 24) != PHASE_PAYLOAD_BYTES
            || readU64(bytes, directory + 32) != 0
            || !allZero(bytes, directory + 72, 24)
            || !allZero(bytes, directory + V2_DIRECTORY_BYTES,
                        V2_PAYLOAD_OFFSET - V2_HEADER_BYTES - V2_DIRECTORY_BYTES)) {
        throw TgmrFailure(TgmrXTransErrorCode::FORMAT,
                          "TGMR v2 section directory is noncanonical");
    }
    const std::string payloadDigest = sha256(
        bytes.data() + V2_PAYLOAD_OFFSET, PHASE_PAYLOAD_BYTES);
    if (payloadDigest != digestSlice(bytes, 192)
            || payloadDigest != digestSlice(bytes, directory + 40)) {
        throw TgmrFailure(TgmrXTransErrorCode::DIGEST,
                          "TGMR v2 phase payload SHA-256 mismatch");
    }
    std::vector<unsigned char> authenticated(bytes);
    std::fill(authenticated.begin() + V2_AUTHENTICATION_OFFSET,
              authenticated.begin() + V2_AUTHENTICATION_OFFSET + V2_AUTHENTICATION_BYTES, 0);
    if (sha256(authenticated) != digestSlice(bytes, V2_AUTHENTICATION_OFFSET)) {
        throw TgmrFailure(TgmrXTransErrorCode::DIGEST,
                          "TGMR v2 container authentication mismatch");
    }
    const std::string fileDigest = sha256(bytes);
    std::vector<unsigned char> payload(
        bytes.begin() + V2_PAYLOAD_OFFSET, bytes.end());
    Reader reader(std::move(payload));
    ParsedModel result;
    result.data = parsePhasePayload(reader);
    result.digest = fileDigest;
    result.official = OFFICIAL_V2_SHA256[0] != '\0' && fileDigest == OFFICIAL_V2_SHA256;
    result.origin = result.official ? "official-v2" : "custom-v2";
    return result;
}

ParsedModel parseModel(std::vector<unsigned char> bytes)
{
    static const char legacyMagic[8] = {'X', 'T', 'G', 'R', 'R', 'E', 'D', '1'};
    static const char v2Magic[8] = {'R', 'T', 'T', 'G', 'M', 'R', '2', 0};
    // Preserve the v1 security/error precedence: every file having the exact
    // legacy byte count is authenticated before any legacy structure is read.
    if (bytes.size() == LEGACY_MODEL_BYTES
            || std::memcmp(bytes.data(), legacyMagic, 8) == 0) {
        return parseLegacyModel(std::move(bytes));
    }
    if (std::memcmp(bytes.data(), v2Magic, 8) == 0) {
        return parseV2Model(std::move(bytes));
    }
    throw TgmrFailure(TgmrXTransErrorCode::FORMAT, "wrong TGMR model magic");
}

PreparedModel prepareModel(const ModelData &model)
{
    PreparedModel result;
    for (unsigned phaseIndex = 0; phaseIndex < PHASES; ++phaseIndex) {
        const Phase &phase = model.phases[phaseIndex];
        PreparedPhase &out = result.phases[phaseIndex];
        out.source = &phase;
        for (unsigned position = 0; position < AREA; ++position) {
            const unsigned channel = phase.observed[position] / AREA;
            out.channels[position] = static_cast<unsigned char>(channel);
            ++out.channelCounts[channel];
            if (phase.observed[position] % AREA == AREA / 2) {
                out.measuredPosition = position;
            }
        }
        float coarseMaximum = -std::numeric_limits<float>::infinity();
        float fullMaximum = -std::numeric_limits<float>::infinity();
        for (unsigned component = 0; component < COMPONENTS; ++component) {
            coarseMaximum = std::max(coarseMaximum,
                phase.logWeights[component] - 0.5f * phase.coarseLogdet[component]);
            fullMaximum = std::max(fullMaximum,
                (phase.logWeights[component] - 0.5f * phase.fullLogdet[component]) / 4.f);
        }
        for (unsigned component = 0; component < COMPONENTS; ++component) {
            out.coarseScale[component] = std::exp((
                phase.logWeights[component] - 0.5f * phase.coarseLogdet[component]
                - coarseMaximum) / 6.f);
            out.fullScale[component] = std::exp(
                (phase.logWeights[component] - 0.5f * phase.fullLogdet[component]) / 4.f
                - fullMaximum);
        }

        out.coarseMeans.resize(GROUPS * SUPPORT_AREA * 8);
        out.coarseLower.resize(GROUPS * SUPPORT_AREA * SUPPORT_AREA * 8);
        out.coarseInvDiagonal.resize(GROUPS * SUPPORT_AREA * 8);
        out.coarseScaleAosoa.resize(GROUPS * 8);
        out.fullInvDiagonal.resize(COMPONENTS * AREA);
        for (unsigned group = 0; group < GROUPS; ++group) {
            for (unsigned lane = 0; lane < 8; ++lane) {
                const unsigned component = group * 8 + lane;
                out.coarseScaleAosoa[group * 8 + lane] = out.coarseScale[component];
                for (unsigned position = 0; position < AREA; ++position) {
                    out.fullInvDiagonal[component * AREA + position] = 1.f /
                        phase.fullCholesky[(component * AREA + position) * AREA + position];
                }
                for (unsigned row = 0; row < SUPPORT_AREA; ++row) {
                    const unsigned position = phase.coarsePositions[row];
                    out.coarseMeans[(group * SUPPORT_AREA + row) * 8 + lane] =
                        phase.meansObserved[component * AREA + position];
                    for (unsigned column = 0; column < SUPPORT_AREA; ++column) {
                        out.coarseLower[
                            ((group * SUPPORT_AREA + row) * SUPPORT_AREA + column) * 8 + lane] =
                            phase.coarseCholesky[
                                (component * SUPPORT_AREA + row) * SUPPORT_AREA + column];
                    }
                    out.coarseInvDiagonal[(group * SUPPORT_AREA + row) * 8 + lane] = 1.f /
                        phase.coarseCholesky[
                            (component * SUPPORT_AREA + row) * SUPPORT_AREA + row];
                }
            }
        }
        result.cacheBytes += (out.coarseMeans.size() + out.coarseLower.size()
            + out.coarseInvDiagonal.size() + out.coarseScaleAosoa.size()
            + out.fullInvDiagonal.size()) * sizeof(float);
    }
    return result;
}

inline int positiveModulo(int value, int modulus)
{
    const int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

std::array<unsigned char, 36> deriveResidueMap(
    const ModelData &model,
    const int xtrans[6][6])
{
    for (unsigned y = 0; y < 6; ++y) {
        for (unsigned x = 0; x < 6; ++x) {
            if (xtrans[y][x] < 0 || xtrans[y][x] > 2) {
                throw TgmrFailure(TgmrXTransErrorCode::CFA,
                                  "X-Trans matrix contains an invalid color");
            }
        }
    }
    std::array<unsigned char, 36> result {{}};
    for (unsigned ry = 0; ry < 6; ++ry) {
        for (unsigned rx = 0; rx < 6; ++rx) {
            int match = -1;
            for (unsigned phaseIndex = 0; phaseIndex < PHASES; ++phaseIndex) {
                const Phase &phase = model.phases[phaseIndex];
                bool equal = true;
                for (unsigned observedPosition = 0; observedPosition < AREA; ++observedPosition) {
                    const unsigned spatial = phase.observed[observedPosition] % AREA;
                    const int dy = static_cast<int>(spatial / PATCH) - 3;
                    const int dx = static_cast<int>(spatial % PATCH) - 3;
                    const int color = xtrans[
                        positiveModulo(static_cast<int>(ry) + dy, 6)
                    ][positiveModulo(static_cast<int>(rx) + dx, 6)];
                    if (color != static_cast<int>(phase.observed[observedPosition] / AREA)) {
                        equal = false;
                        break;
                    }
                }
                if (equal) {
                    if (match >= 0) {
                        throw TgmrFailure(TgmrXTransErrorCode::CFA,
                                          "ambiguous TGMR phase mapping");
                    }
                    match = static_cast<int>(phaseIndex);
                }
            }
            if (match < 0) {
                throw TgmrFailure(TgmrXTransErrorCode::CFA,
                                  "unsupported X-Trans CFA phase/orientation");
            }
            result[ry * 6 + rx] = static_cast<unsigned char>(match);
        }
    }
    return result;
}

unsigned mirrorIndex(int coordinate, unsigned extent)
{
    if (extent == 1) {
        return 0;
    }
    while (coordinate < 0 || coordinate >= static_cast<int>(extent)) {
        coordinate = coordinate < 0
            ? -coordinate
            : 2 * static_cast<int>(extent) - 2 - coordinate;
    }
    return static_cast<unsigned>(coordinate);
}

void centerObservation(
    const PreparedPhase &phase,
    const std::array<float, AREA> &observed,
    PixelWork &work)
{
    std::array<float, 3> sums {{0.f, 0.f, 0.f}};
    for (unsigned position = 0; position < AREA; ++position) {
        sums[phase.channels[position]] += observed[position];
    }
    work.dc = 0.f;
    for (unsigned channel = 0; channel < 3; ++channel) {
        work.dc += sums[channel] / phase.channelCounts[channel];
    }
    work.dc /= 3.f;
    for (unsigned position = 0; position < AREA; ++position) {
        work.centered[position] = observed[position] - work.dc;
    }
}

void stableInsert(
    std::array<float, SHORTLIST> &scores,
    std::array<unsigned char, SHORTLIST> &ids,
    float score,
    unsigned component)
{
    unsigned position = SHORTLIST;
    for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
        if (score > scores[slot] || (score == scores[slot] && component < ids[slot])) {
            position = slot;
            break;
        }
    }
    if (position == SHORTLIST) {
        return;
    }
    for (unsigned slot = SHORTLIST - 1; slot > position; --slot) {
        scores[slot] = scores[slot - 1];
        ids[slot] = ids[slot - 1];
    }
    scores[position] = score;
    ids[position] = static_cast<unsigned char>(component);
}

template <unsigned N>
float quadratic(
    const float *lower,
    const std::array<float, N> &residual,
    std::array<float, N> &solved)
{
    float result = 0.f;
    for (unsigned row = 0; row < N; ++row) {
        float value = residual[row];
        const float *matrixRow = lower + row * N;
        for (unsigned column = 0; column < row; ++column) {
            value -= matrixRow[column] * solved[column];
        }
        solved[row] = value / matrixRow[row];
        result += solved[row] * solved[row];
    }
    return result;
}

void coarseScalar(const PreparedPhase &prepared, PixelWork &work)
{
    const Phase &phase = *prepared.source;
    std::array<float, SHORTLIST> bestScores;
    bestScores.fill(-std::numeric_limits<float>::infinity());
    work.ids.fill(255);
    for (unsigned component = 0; component < COMPONENTS; ++component) {
        std::array<float, SUPPORT_AREA> residual {{}};
        for (unsigned row = 0; row < SUPPORT_AREA; ++row) {
            const unsigned position = phase.coarsePositions[row];
            residual[row] = work.centered[position]
                - phase.meansObserved[component * AREA + position];
        }
        std::array<float, SUPPORT_AREA> solved {{}};
        const float q = quadratic<SUPPORT_AREA>(
            phase.coarseCholesky.data() + component * SUPPORT_AREA * SUPPORT_AREA,
            residual, solved);
        const float score = prepared.coarseScale[component] / (1.f + q / 3.f);
        stableInsert(bestScores, work.ids, score, component);
    }
}

#if RT_TGMR_HAS_TARGET_AVX2
RT_TGMR_TARGET_AVX2
void coarseAvx2(const PreparedPhase &prepared, PixelWork &work)
{
    const Phase &phase = *prepared.source;
    alignas(32) float allScores[COMPONENTS];
    for (unsigned group = 0; group < GROUPS; ++group) {
        __m256 solved[SUPPORT_AREA];
        __m256 q = _mm256_setzero_ps();
        for (unsigned row = 0; row < SUPPORT_AREA; ++row) {
            const unsigned position = phase.coarsePositions[row];
            __m256 value = _mm256_sub_ps(
                _mm256_set1_ps(work.centered[position]),
                _mm256_loadu_ps(prepared.coarseMeans.data()
                    + (group * SUPPORT_AREA + row) * 8));
            for (unsigned column = 0; column < row; ++column) {
                value = _mm256_fnmadd_ps(
                    _mm256_loadu_ps(prepared.coarseLower.data()
                        + ((group * SUPPORT_AREA + row) * SUPPORT_AREA + column) * 8),
                    solved[column], value);
            }
            solved[row] = _mm256_mul_ps(
                value,
                _mm256_loadu_ps(prepared.coarseInvDiagonal.data()
                    + (group * SUPPORT_AREA + row) * 8));
            q = _mm256_fmadd_ps(solved[row], solved[row], q);
        }
        const __m256 denominator = _mm256_fmadd_ps(
            q, _mm256_set1_ps(1.f / 3.f), _mm256_set1_ps(1.f));
        _mm256_store_ps(allScores + group * 8, _mm256_div_ps(
            _mm256_loadu_ps(prepared.coarseScaleAosoa.data() + group * 8), denominator));
    }
    std::array<float, SHORTLIST> bestScores;
    bestScores.fill(-std::numeric_limits<float>::infinity());
    work.ids.fill(255);
    for (unsigned component = 0; component < COMPONENTS; ++component) {
        stableInsert(bestScores, work.ids, allScores[component], component);
    }
}
#endif

#if RT_TGMR_HAS_NEON
void coarseNeon(const PreparedPhase &prepared, PixelWork &work)
{
    const Phase &phase = *prepared.source;
    alignas(16) float allScores[COMPONENTS];
    for (unsigned group = 0; group < GROUPS; ++group) {
        for (unsigned half = 0; half < 2; ++half) {
            float32x4_t solved[SUPPORT_AREA];
            float32x4_t q = vdupq_n_f32(0.f);
            const unsigned laneOffset = half * 4;
            for (unsigned row = 0; row < SUPPORT_AREA; ++row) {
                const unsigned position = phase.coarsePositions[row];
                float32x4_t value = vsubq_f32(
                    vdupq_n_f32(work.centered[position]),
                    vld1q_f32(prepared.coarseMeans.data()
                        + (group * SUPPORT_AREA + row) * 8 + laneOffset));
                for (unsigned column = 0; column < row; ++column) {
                    value = vfmsq_f32(value,
                        vld1q_f32(prepared.coarseLower.data()
                            + ((group * SUPPORT_AREA + row) * SUPPORT_AREA + column) * 8
                            + laneOffset),
                        solved[column]);
                }
                solved[row] = vmulq_f32(value,
                    vld1q_f32(prepared.coarseInvDiagonal.data()
                        + (group * SUPPORT_AREA + row) * 8 + laneOffset));
                q = vfmaq_f32(q, solved[row], solved[row]);
            }
            const float32x4_t denominator = vfmaq_n_f32(vdupq_n_f32(1.f), q, 1.f / 3.f);
            vst1q_f32(allScores + group * 8 + laneOffset,
                vdivq_f32(vld1q_f32(prepared.coarseScaleAosoa.data()
                    + group * 8 + laneOffset), denominator));
        }
    }
    std::array<float, SHORTLIST> bestScores;
    bestScores.fill(-std::numeric_limits<float>::infinity());
    work.ids.fill(255);
    for (unsigned component = 0; component < COMPONENTS; ++component) {
        stableInsert(bestScores, work.ids, allScores[component], component);
    }
}
#endif

inline float positiveWeight(float scale, float quadraticValue)
{
    const float s = 1.f + quadraticValue / 3.f;
    const float s2 = s * s;
    const float s4 = s2 * s2;
    return scale / (s4 * s2 * std::sqrt(s));
}

void fullScalarSlot(const PreparedPhase &prepared, PixelWork &work, unsigned slot)
{
    const Phase &phase = *prepared.source;
    const unsigned component = work.ids[slot];
    std::array<float, AREA> residual {{}};
    for (unsigned position = 0; position < AREA; ++position) {
        residual[position] = work.centered[position]
            - phase.meansObserved[component * AREA + position];
    }
    std::array<float, AREA> solved {{}};
    const float q = quadratic<AREA>(
        phase.fullCholesky.data() + component * AREA * AREA, residual, solved);
    work.weights[slot] = positiveWeight(prepared.fullScale[component], q);
    for (unsigned target = 0; target < 2; ++target) {
        float value = phase.meansTarget[component * 2 + target] + work.dc;
        const float *gain = phase.gains.data() + (component * 2 + target) * AREA;
        for (unsigned position = 0; position < AREA; ++position) {
            value += gain[position] * residual[position];
        }
        work.predictions[slot][target] = value;
    }
}

#if RT_TGMR_HAS_TARGET_AVX2
RT_TGMR_TARGET_AVX2
void fullAvx2Group(
    const PreparedPhase &prepared,
    std::vector<PixelWork> &work,
    const Request *requests,
    unsigned count,
    unsigned component)
{
    const Phase &phase = *prepared.source;
    unsigned begin = 0;
    for (; begin + 8 <= count; begin += 8) {
        __m256 residual[AREA];
        __m256 solved[AREA];
        __m256 q = _mm256_setzero_ps();
        alignas(32) float lanes[8];
        for (unsigned position = 0; position < AREA; ++position) {
            for (unsigned lane = 0; lane < 8; ++lane) {
                lanes[lane] = work[requests[begin + lane].pixel].centered[position];
            }
            residual[position] = _mm256_sub_ps(
                _mm256_load_ps(lanes),
                _mm256_set1_ps(phase.meansObserved[component * AREA + position]));
            __m256 value = residual[position];
            const float *matrixRow = phase.fullCholesky.data()
                + (component * AREA + position) * AREA;
            for (unsigned column = 0; column < position; ++column) {
                value = _mm256_fnmadd_ps(
                    _mm256_set1_ps(matrixRow[column]), solved[column], value);
            }
            solved[position] = _mm256_mul_ps(value, _mm256_set1_ps(
                prepared.fullInvDiagonal[component * AREA + position]));
            q = _mm256_fmadd_ps(solved[position], solved[position], q);
        }
        const __m256 s = _mm256_fmadd_ps(
            q, _mm256_set1_ps(1.f / 3.f), _mm256_set1_ps(1.f));
        const __m256 s2 = _mm256_mul_ps(s, s);
        const __m256 s4 = _mm256_mul_ps(s2, s2);
        const __m256 denominator = _mm256_mul_ps(
            _mm256_mul_ps(s4, s2), _mm256_sqrt_ps(s));
        _mm256_store_ps(lanes, _mm256_div_ps(
            _mm256_set1_ps(prepared.fullScale[component]), denominator));
        for (unsigned lane = 0; lane < 8; ++lane) {
            const Request &request = requests[begin + lane];
            work[request.pixel].weights[request.slot] = lanes[lane];
        }
        for (unsigned target = 0; target < 2; ++target) {
            for (unsigned lane = 0; lane < 8; ++lane) {
                lanes[lane] = phase.meansTarget[component * 2 + target]
                    + work[requests[begin + lane].pixel].dc;
            }
            __m256 value = _mm256_load_ps(lanes);
            const float *gain = phase.gains.data() + (component * 2 + target) * AREA;
            for (unsigned position = 0; position < AREA; ++position) {
                value = _mm256_fmadd_ps(
                    _mm256_set1_ps(gain[position]), residual[position], value);
            }
            _mm256_store_ps(lanes, value);
            for (unsigned lane = 0; lane < 8; ++lane) {
                const Request &request = requests[begin + lane];
                work[request.pixel].predictions[request.slot][target] = lanes[lane];
            }
        }
    }
    for (; begin < count; ++begin) {
        const Request &request = requests[begin];
        fullScalarSlot(prepared, work[request.pixel], request.slot);
    }
}
#endif

#if RT_TGMR_HAS_NEON
void fullNeonGroup(
    const PreparedPhase &prepared,
    std::vector<PixelWork> &work,
    const Request *requests,
    unsigned count,
    unsigned component)
{
    const Phase &phase = *prepared.source;
    unsigned begin = 0;
    for (; begin + 4 <= count; begin += 4) {
        float32x4_t residual[AREA];
        float32x4_t solved[AREA];
        float32x4_t q = vdupq_n_f32(0.f);
        alignas(16) float lanes[4];
        for (unsigned position = 0; position < AREA; ++position) {
            for (unsigned lane = 0; lane < 4; ++lane) {
                lanes[lane] = work[requests[begin + lane].pixel].centered[position];
            }
            residual[position] = vsubq_f32(vld1q_f32(lanes),
                vdupq_n_f32(phase.meansObserved[component * AREA + position]));
            float32x4_t value = residual[position];
            const float *matrixRow = phase.fullCholesky.data()
                + (component * AREA + position) * AREA;
            for (unsigned column = 0; column < position; ++column) {
                value = vfmsq_n_f32(value, solved[column], matrixRow[column]);
            }
            solved[position] = vmulq_n_f32(value,
                prepared.fullInvDiagonal[component * AREA + position]);
            q = vfmaq_f32(q, solved[position], solved[position]);
        }
        const float32x4_t s = vfmaq_n_f32(vdupq_n_f32(1.f), q, 1.f / 3.f);
        const float32x4_t s2 = vmulq_f32(s, s);
        const float32x4_t s4 = vmulq_f32(s2, s2);
        const float32x4_t denominator = vmulq_f32(vmulq_f32(s4, s2), vsqrtq_f32(s));
        vst1q_f32(lanes, vdivq_f32(vdupq_n_f32(prepared.fullScale[component]), denominator));
        for (unsigned lane = 0; lane < 4; ++lane) {
            const Request &request = requests[begin + lane];
            work[request.pixel].weights[request.slot] = lanes[lane];
        }
        for (unsigned target = 0; target < 2; ++target) {
            for (unsigned lane = 0; lane < 4; ++lane) {
                lanes[lane] = phase.meansTarget[component * 2 + target]
                    + work[requests[begin + lane].pixel].dc;
            }
            float32x4_t value = vld1q_f32(lanes);
            const float *gain = phase.gains.data() + (component * 2 + target) * AREA;
            for (unsigned position = 0; position < AREA; ++position) {
                value = vfmaq_n_f32(value, residual[position], gain[position]);
            }
            vst1q_f32(lanes, value);
            for (unsigned lane = 0; lane < 4; ++lane) {
                const Request &request = requests[begin + lane];
                work[request.pixel].predictions[request.slot][target] = lanes[lane];
            }
        }
    }
    for (; begin < count; ++begin) {
        const Request &request = requests[begin];
        fullScalarSlot(prepared, work[request.pixel], request.slot);
    }
}
#endif

std::array<float, 3> finishPixel(const PreparedPhase &prepared, const PixelWork &work)
{
    float total = 0.f;
    for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
        total += work.weights[slot];
    }
    if (!(total > 0.f) || !std::isfinite(total)) {
        throw TgmrFailure(TgmrXTransErrorCode::NONFINITE,
                          "TGMR posterior has zero or non-finite weight");
    }
    std::array<float, 3> output {{0.f, 0.f, 0.f}};
    for (unsigned target = 0; target < 2; ++target) {
        float value = 0.f;
        for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
            value += work.predictions[slot][target] * (work.weights[slot] / total);
        }
        output[prepared.source->targets[target]] = value;
    }
    return output;
}

bool cpuHasAvx2Fma()
{
#if RT_TGMR_HAS_TARGET_AVX2
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return false;
#endif
}

template <typename InputAt, typename OutputAt>
void processChunk(
    const PreparedPhase &phase,
    const std::vector<Coordinate> &coordinates,
    unsigned begin,
    unsigned end,
    InputAt inputAt,
    OutputAt outputAt,
    bool useAvx2,
    bool useNeon)
{
    const unsigned count = end - begin;
    static thread_local std::vector<std::array<float, AREA>> observations;
    static thread_local std::vector<PixelWork> work;
    observations.resize(count);
    work.resize(count);
    for (unsigned local = 0; local < count; ++local) {
        const Coordinate &coordinate = coordinates[begin + local];
        std::array<float, AREA> &observed = observations[local];
        for (unsigned observedPosition = 0; observedPosition < AREA; ++observedPosition) {
            const unsigned spatial = phase.source->observed[observedPosition] % AREA;
            const int dx = static_cast<int>(spatial % PATCH) - 3;
            const int dy = static_cast<int>(spatial / PATCH) - 3;
            const float value = inputAt(
                mirrorIndex(static_cast<int>(coordinate.x) + dx, inputAt.width()),
                mirrorIndex(static_cast<int>(coordinate.y) + dy, inputAt.height()));
            if (!std::isfinite(value)) {
                throw TgmrFailure(TgmrXTransErrorCode::NONFINITE,
                                  "TGMR input contains a non-finite sample");
            }
            observed[observedPosition] = value / SOURCE_SCALE;
        }
        centerObservation(phase, observed, work[local]);
        bool coarseDone = false;
#if RT_TGMR_HAS_TARGET_AVX2
        if (useAvx2) {
            coarseAvx2(phase, work[local]);
            coarseDone = true;
        }
#endif
#if RT_TGMR_HAS_NEON
        if (useNeon) {
            coarseNeon(phase, work[local]);
            coarseDone = true;
        }
#endif
        if (!coarseDone) {
            coarseScalar(phase, work[local]);
        }
    }

#if RT_TGMR_HAS_TARGET_AVX2 || RT_TGMR_HAS_NEON
    if (useAvx2 || useNeon) {
        std::array<unsigned, COMPONENTS> counts {{}};
        for (unsigned local = 0; local < count; ++local) {
            for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
                ++counts[work[local].ids[slot]];
            }
        }
        std::array<unsigned, COMPONENTS + 1> offsets {{}};
        for (unsigned component = 0; component < COMPONENTS; ++component) {
            offsets[component + 1] = offsets[component] + counts[component];
        }
        static thread_local std::vector<Request> requests;
        requests.resize(count * SHORTLIST);
        std::array<unsigned, COMPONENTS> cursor {{}};
        std::copy(offsets.begin(), offsets.begin() + COMPONENTS, cursor.begin());
        for (unsigned local = 0; local < count; ++local) {
            for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
                const unsigned component = work[local].ids[slot];
                requests[cursor[component]++] = {
                    static_cast<std::uint32_t>(local), static_cast<unsigned char>(slot)};
            }
        }
        for (unsigned component = 0; component < COMPONENTS; ++component) {
#if RT_TGMR_HAS_TARGET_AVX2
            if (useAvx2) {
            fullAvx2Group(phase, work, requests.data() + offsets[component],
                          counts[component], component);
                continue;
            }
#endif
#if RT_TGMR_HAS_NEON
            fullNeonGroup(phase, work, requests.data() + offsets[component],
                          counts[component], component);
#endif
        }
    } else
#endif
    {
        for (unsigned local = 0; local < count; ++local) {
            for (unsigned slot = 0; slot < SHORTLIST; ++slot) {
                fullScalarSlot(phase, work[local], slot);
            }
        }
    }

    for (unsigned local = 0; local < count; ++local) {
        const Coordinate &coordinate = coordinates[begin + local];
        std::array<float, 3> rgb = finishPixel(phase, work[local]);
        for (float &value : rgb) {
            value *= SOURCE_SCALE;
            if (!std::isfinite(value)) {
                throw TgmrFailure(TgmrXTransErrorCode::NONFINITE,
                                  "TGMR reconstruction produced a non-finite sample");
            }
        }
        // Division and multiplication by 65535 need not preserve every float
        // bit.  Restore the physical center sample from the original engine
        // plane after inference so the CFA measurement is exact.
        rgb[phase.source->sampled] = inputAt(coordinate.x, coordinate.y);
        outputAt(coordinate.x, coordinate.y, rgb);
    }
}

template <typename InputAt, typename OutputAt>
TgmrXTransRunResult runDemosaic(
    InputAt inputAt,
    OutputAt outputAt,
    int width,
    int height,
    const int xtrans[6][6],
    const TgmrXTransModel &model,
    int originX,
    int originY,
    bool forceScalar);

} // namespace

class TgmrXTransModel final
{
public:
    explicit TgmrXTransModel(ParsedModel input) :
        data(std::move(input.data)), prepared(prepareModel(data)),
        digest(std::move(input.digest)), origin(std::move(input.origin)),
        official(input.official)
    {
    }

    ModelData data;
    PreparedModel prepared;
    std::string digest;
    std::string origin;
    bool official = false;
};

namespace
{

template <typename InputAt, typename OutputAt>
TgmrXTransRunResult runDemosaic(
    InputAt inputAt,
    OutputAt outputAt,
    int width,
    int height,
    const int xtrans[6][6],
    const TgmrXTransModel &model,
    int originX,
    int originY,
    bool forceScalar)
{
    TgmrXTransRunResult result;
    if (width <= 0 || height <= 0 || inputAt.width() != static_cast<unsigned>(width)
            || inputAt.height() != static_cast<unsigned>(height)) {
        result.code = TgmrXTransErrorCode::SIZE;
        result.message = "TGMR requires non-empty matching input/output dimensions";
        return result;
    }
    if (static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height)
            > std::numeric_limits<std::size_t>::max()) {
        result.code = TgmrXTransErrorCode::SIZE;
        result.message = "TGMR image dimensions overflow addressable memory";
        return result;
    }
    try {
        const std::array<unsigned char, 36> residueMap =
            deriveResidueMap(model.data, xtrans);
        struct TileJob final {
            unsigned x0, y0, x1, y1;
        };
        std::vector<TileJob> tiles;
        for (unsigned y = 0; y < static_cast<unsigned>(height); y += TILE) {
            for (unsigned x = 0; x < static_cast<unsigned>(width); x += TILE) {
                tiles.push_back({x, y,
                    std::min<unsigned>(width, x + TILE),
                    std::min<unsigned>(height, y + TILE)});
            }
        }
        result.pixelCount = static_cast<std::uint64_t>(width) * height;
        result.tileCount = tiles.size();
        result.avx2 = !forceScalar && cpuHasAvx2Fma();
#if RT_TGMR_HAS_NEON
        result.neon = !forceScalar;
#endif
#ifdef _OPENMP
        result.workerCount = static_cast<std::uint32_t>(std::max(
            1, std::min<int>(omp_get_max_threads(), static_cast<int>(tiles.size()))));
#else
        result.workerCount = 1;
#endif
        result.workingBytesPerWorker =
            static_cast<std::uint64_t>(TILE) * TILE * sizeof(Coordinate)
            + static_cast<std::uint64_t>(CHUNK)
                * (sizeof(std::array<float, AREA>) + sizeof(PixelWork)
                   + SHORTLIST * sizeof(Request));
        std::atomic<bool> failed(false);
        TgmrXTransErrorCode failureCode = TgmrXTransErrorCode::NONE;
        std::string failureMessage;
        std::mutex failureMutex;
        const auto started = std::chrono::steady_clock::now();
#ifdef _OPENMP
#pragma omp parallel for schedule(static) num_threads(result.workerCount)
#endif
        for (int tileIndex = 0; tileIndex < static_cast<int>(tiles.size()); ++tileIndex) {
            if (failed.load(std::memory_order_relaxed)) {
                continue;
            }
            try {
                const TileJob &tile = tiles[static_cast<std::size_t>(tileIndex)];
                std::array<std::vector<Coordinate>, PHASES> coordinates;
                for (unsigned y = tile.y0; y < tile.y1; ++y) {
                    for (unsigned x = tile.x0; x < tile.x1; ++x) {
                        const unsigned residueX = static_cast<unsigned>(
                            positiveModulo(static_cast<int>(x) + originX, 6));
                        const unsigned residueY = static_cast<unsigned>(
                            positiveModulo(static_cast<int>(y) + originY, 6));
                        coordinates[residueMap[residueY * 6 + residueX]].push_back({x, y});
                    }
                }
                for (unsigned phase = 0; phase < PHASES; ++phase) {
                    for (unsigned begin = 0; begin < coordinates[phase].size(); begin += CHUNK) {
                        const unsigned end = static_cast<unsigned>(std::min<std::size_t>(
                            coordinates[phase].size(), static_cast<std::size_t>(begin) + CHUNK));
                        processChunk(model.prepared.phases[phase], coordinates[phase],
                                     begin, end, inputAt, outputAt,
                                     result.avx2, result.neon);
                    }
                }
            } catch (const TgmrFailure &error) {
                if (!failed.exchange(true)) {
                    std::lock_guard<std::mutex> lock(failureMutex);
                    failureCode = error.code;
                    failureMessage = error.what();
                }
            } catch (const std::bad_alloc &) {
                if (!failed.exchange(true)) {
                    std::lock_guard<std::mutex> lock(failureMutex);
                    failureCode = TgmrXTransErrorCode::ALLOCATION;
                    failureMessage = "TGMR tile workspace allocation failed";
                }
            } catch (const std::exception &error) {
                if (!failed.exchange(true)) {
                    std::lock_guard<std::mutex> lock(failureMutex);
                    failureCode = TgmrXTransErrorCode::INTERNAL;
                    failureMessage = error.what();
                }
            }
        }
        result.elapsedMicroseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count());
        if (failed.load()) {
            result.code = failureCode;
            result.message = failureMessage;
        }
    } catch (const TgmrFailure &error) {
        result.code = error.code;
        result.message = error.what();
    } catch (const std::bad_alloc &) {
        result.code = TgmrXTransErrorCode::ALLOCATION;
        result.message = "TGMR setup allocation failed";
    } catch (const std::exception &error) {
        result.code = TgmrXTransErrorCode::INTERNAL;
        result.message = error.what();
    }
    return result;
}

struct ArrayInput final {
    const array2D<float> &value;
    unsigned imageWidth;
    unsigned imageHeight;
    unsigned width() const { return imageWidth; }
    unsigned height() const { return imageHeight; }
    float operator()(unsigned x, unsigned y) const { return value[y][x]; }
};

struct ArrayOutput final {
    array2D<float> &red;
    array2D<float> &green;
    array2D<float> &blue;
    void operator()(unsigned x, unsigned y, const std::array<float, 3> &rgb) const
    {
        red[y][x] = rgb[0];
        green[y][x] = rgb[1];
        blue[y][x] = rgb[2];
    }
};

struct FlatInput final {
    const float *value;
    unsigned imageWidth;
    unsigned imageHeight;
    unsigned width() const { return imageWidth; }
    unsigned height() const { return imageHeight; }
    float operator()(unsigned x, unsigned y) const
    {
        return value[static_cast<std::size_t>(y) * imageWidth + x];
    }
};

struct FlatOutput final {
    float *red;
    float *green;
    float *blue;
    unsigned width;
    void operator()(unsigned x, unsigned y, const std::array<float, 3> &rgb) const
    {
        const std::size_t index = static_cast<std::size_t>(y) * width + x;
        red[index] = rgb[0];
        green[index] = rgb[1];
        blue[index] = rgb[2];
    }
};

} // namespace

const char *tgmrXTransErrorCodeName(TgmrXTransErrorCode code)
{
    switch (code) {
        case TgmrXTransErrorCode::NONE: return "NONE";
        case TgmrXTransErrorCode::IO: return "IO";
        case TgmrXTransErrorCode::SIZE: return "SIZE";
        case TgmrXTransErrorCode::DIGEST: return "DIGEST";
        case TgmrXTransErrorCode::FORMAT: return "FORMAT";
        case TgmrXTransErrorCode::CFA: return "CFA";
        case TgmrXTransErrorCode::ALLOCATION: return "ALLOCATION";
        case TgmrXTransErrorCode::NONFINITE: return "NONFINITE";
        case TgmrXTransErrorCode::INTERNAL: return "INTERNAL";
    }
    return "INTERNAL";
}

TgmrXTransLoadResult loadTgmrXTransModel(const std::string &path)
{
    TgmrXTransLoadResult result;
    try {
        result.model = std::make_shared<const TgmrXTransModel>(
            parseModel(readFile(path)));
    } catch (const TgmrFailure &error) {
        result.code = error.code;
        result.message = error.what();
    } catch (const std::bad_alloc &) {
        result.code = TgmrXTransErrorCode::ALLOCATION;
        result.message = "TGMR model allocation failed";
    } catch (const std::exception &error) {
        result.code = TgmrXTransErrorCode::INTERNAL;
        result.message = error.what();
    }
    return result;
}

TgmrXTransLoadResult loadCachedTgmrXTransModel(const std::string &path)
{
    static std::mutex mutex;
    static std::map<std::string, std::shared_ptr<const TgmrXTransModel>> cache;
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = cache.find(path);
    if (found != cache.end()) {
        TgmrXTransLoadResult result;
        result.model = found->second;
        return result;
    }
    TgmrXTransLoadResult result = loadTgmrXTransModel(path);
    if (result) {
        cache[path] = result.model;
    }
    return result;
}

const std::string &tgmrXTransModelDigest(const TgmrXTransModel &model)
{
    return model.digest;
}

const char *tgmrXTransModelOrigin(const TgmrXTransModel &model)
{
    return model.origin.c_str();
}

bool tgmrXTransModelIsOfficial(const TgmrXTransModel &model)
{
    return model.official;
}

void setTgmrXTransDataDirectory(const std::string &path)
{
    std::lock_guard<std::mutex> lock(tgmrDataDirectoryMutex);
    tgmrDataDirectory = path;
}

std::string tgmrXTransDefaultModelPath()
{
    std::lock_guard<std::mutex> lock(tgmrDataDirectoryMutex);
    if (tgmrDataDirectory.empty()) {
        return std::string();
    }
    const char separator = tgmrDataDirectory.back() == '/'
            || tgmrDataDirectory.back() == '\\' ? '\0' : '/';
    return tgmrDataDirectory + (separator ? std::string(1, separator) : std::string())
        + "models/xtrans-tgmr-v2.tgmr";
}

TgmrXTransRunResult demosaicTgmrXTrans(
    const array2D<float> &rawData,
    array2D<float> &red,
    array2D<float> &green,
    array2D<float> &blue,
    int width,
    int height,
    const int xtrans[6][6],
    const std::shared_ptr<const TgmrXTransModel> &model,
    int originX,
    int originY,
    bool forceScalar)
{
    if (!model) {
        TgmrXTransRunResult result;
        result.code = TgmrXTransErrorCode::FORMAT;
        result.message = "TGMR model is null";
        return result;
    }
    return runDemosaic(
        ArrayInput{rawData, static_cast<unsigned>(width), static_cast<unsigned>(height)},
        ArrayOutput{red, green, blue}, width, height, xtrans, *model,
        originX, originY, forceScalar);
}

TgmrXTransRunResult demosaicTgmrXTransReference(
    const float *rawData,
    float *red,
    float *green,
    float *blue,
    int width,
    int height,
    const int xtrans[6][6],
    const std::shared_ptr<const TgmrXTransModel> &model,
    int originX,
    int originY,
    bool forceScalar)
{
    if (!rawData || !red || !green || !blue || !model) {
        TgmrXTransRunResult result;
        result.code = TgmrXTransErrorCode::FORMAT;
        result.message = "TGMR reference requires non-null input, output, and model";
        return result;
    }
    return runDemosaic(
        FlatInput{rawData, static_cast<unsigned>(width), static_cast<unsigned>(height)},
        FlatOutput{red, green, blue, static_cast<unsigned>(width)},
        width, height, xtrans, *model, originX, originY, forceScalar);
}

} // namespace rtengine
