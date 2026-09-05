#include "tgmr/model_v2.h"

#include "tgmr/xtrans_training.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace tgmr
{
namespace
{

constexpr std::array<std::uint8_t, 8> MAGIC{{'R', 'T', 'T', 'G', 'M', 'R', '2', 0}};
constexpr std::uint16_t MAJOR = 2;
constexpr std::uint16_t MINOR = 0;
constexpr std::uint32_t ENDIAN = 0x01020304;
constexpr std::uint32_t ARCHITECTURE = 1;
constexpr std::uint32_t SCALAR_FLOAT32 = 1;
constexpr std::uint32_t SECTION_PHASE_DATA = 1;
constexpr std::uint32_t ENCODING_K32_S9_Q8 = 1;
constexpr std::uint64_t PHASE_PAYLOAD_BYTES = 6073128;
constexpr std::size_t AUTHENTICATION_OFFSET = 352;
constexpr std::size_t AUTHENTICATION_BYTES = 32;

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

void putFloat(std::uint8_t *destination, float value)
{
    static_assert(sizeof(float) == 4, "TGMR v2 requires float32");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    put32(destination, bits);
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

float getFloat(const std::uint8_t *source)
{
    const std::uint32_t bits = get32(source);
    float result = 0.f;
    std::memcpy(&result, &bits, sizeof(result));
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

std::array<std::uint8_t, 32> authenticatedDigest(std::vector<std::uint8_t> bytes)
{
    if (bytes.size() < AUTHENTICATION_OFFSET + AUTHENTICATION_BYTES) {
        throw std::runtime_error("TGMR v2 input is too small for authentication");
    }
    std::fill(bytes.begin() + AUTHENTICATION_OFFSET,
              bytes.begin() + AUTHENTICATION_OFFSET + AUTHENTICATION_BYTES, 0);
    return sha256(bytes.data(), bytes.size());
}

void validateIdentity(const ModelV2Identity &identity)
{
    if (identity.modelRevision == 0) {
        throw std::runtime_error("TGMR v2 model revision zero is invalid");
    }
    const std::array<std::uint8_t, 32> zero{};
    if (identity.corpusSha256 == zero
        || identity.trainerConfigurationSha256 == zero
        || identity.attributionSha256 == zero
        || identity.trainerRevisionSha256 == zero) {
        throw std::runtime_error("TGMR v2 identity digests must be nonzero");
    }
}

} // namespace

std::vector<std::uint8_t> serializeModelV2(
    const ModelV2Identity &identity,
    const std::vector<std::uint8_t> &phasePayload)
{
    validateIdentity(identity);
    validatePhasePayload(phasePayload);
    const std::uint64_t fileBytes = TGMR_V2_PAYLOAD_OFFSET + phasePayload.size();
    std::vector<std::uint8_t> output(static_cast<std::size_t>(fileBytes), 0);
    std::copy(MAGIC.begin(), MAGIC.end(), output.begin());
    put16(output.data() + 8, MAJOR);
    put16(output.data() + 10, MINOR);
    put32(output.data() + 12, TGMR_V2_HEADER_BYTES);
    put32(output.data() + 16, ENDIAN);
    put32(output.data() + 20, 0);
    put32(output.data() + 24, ARCHITECTURE);
    put32(output.data() + 28, identity.modelRevision);
    put32(output.data() + 32, SCALAR_FLOAT32);
    put32(output.data() + 36, 7);
    put32(output.data() + 40, 18);
    put32(output.data() + 44, 32);
    put32(output.data() + 48, 3);
    put32(output.data() + 52, 8);
    put32(output.data() + 56, 49);
    put32(output.data() + 60, 2);
    putFloat(output.data() + 64, 3.f);
    putFloat(output.data() + 68, 4.f);
    putFloat(output.data() + 72, 0.0003f);
    put32(output.data() + 76, 1);
    put32(output.data() + 80, TGMR_V2_DIRECTORY_BYTES);
    put32(output.data() + 84, 0);
    put64(output.data() + 88, TGMR_V2_HEADER_BYTES);
    put64(output.data() + 96, TGMR_V2_DIRECTORY_BYTES);
    put64(output.data() + 104, TGMR_V2_PAYLOAD_OFFSET);
    put64(output.data() + 112, phasePayload.size());
    put64(output.data() + 120, fileBytes);
    std::copy(identity.corpusSha256.begin(), identity.corpusSha256.end(), output.begin() + 128);
    std::copy(identity.trainerConfigurationSha256.begin(),
              identity.trainerConfigurationSha256.end(), output.begin() + 160);
    const auto payloadDigest = sha256(phasePayload.data(), phasePayload.size());
    std::copy(payloadDigest.begin(), payloadDigest.end(), output.begin() + 192);
    std::copy(identity.attributionSha256.begin(), identity.attributionSha256.end(), output.begin() + 224);
    std::copy(identity.trainerRevisionSha256.begin(),
              identity.trainerRevisionSha256.end(), output.begin() + 256);
    static const char schema[] = "rawtherapee-xtrans-tgmr-v2-k32-s9-q8";
    const auto schemaDigest = sha256(schema, sizeof(schema) - 1);
    std::copy(schemaDigest.begin(), schemaDigest.end(), output.begin() + 288);

    std::uint8_t *directory = output.data() + TGMR_V2_HEADER_BYTES;
    put32(directory + 0, SECTION_PHASE_DATA);
    put32(directory + 4, ENCODING_K32_S9_Q8);
    put32(directory + 8, 0);
    put32(directory + 12, 0);
    put64(directory + 16, TGMR_V2_PAYLOAD_OFFSET);
    put64(directory + 24, phasePayload.size());
    put64(directory + 32, 0);
    std::copy(payloadDigest.begin(), payloadDigest.end(), directory + 40);
    std::copy(phasePayload.begin(), phasePayload.end(), output.begin() + TGMR_V2_PAYLOAD_OFFSET);
    const auto authentication = authenticatedDigest(output);
    std::copy(authentication.begin(), authentication.end(), output.begin() + AUTHENTICATION_OFFSET);
    return output;
}

ModelV2Inspection inspectModelV2(
    const std::vector<std::uint8_t> &bytes,
    std::vector<std::uint8_t> *phasePayload)
{
    if (bytes.size() != TGMR_V2_PAYLOAD_OFFSET + PHASE_PAYLOAD_BYTES) {
        throw std::runtime_error("TGMR v2 file size is outside the reviewed limit");
    }
    if (!std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin())) {
        throw std::runtime_error("wrong TGMR v2 magic");
    }
    const float nu = getFloat(bytes.data() + 64);
    const float temperature = getFloat(bytes.data() + 68);
    const float tau = getFloat(bytes.data() + 72);
    if (get16(bytes.data() + 8) != MAJOR || get16(bytes.data() + 10) != MINOR
        || get32(bytes.data() + 12) != TGMR_V2_HEADER_BYTES
        || get32(bytes.data() + 16) != ENDIAN || get32(bytes.data() + 20) != 0
        || get32(bytes.data() + 24) != ARCHITECTURE
        || get32(bytes.data() + 28) == 0
        || get32(bytes.data() + 32) != SCALAR_FLOAT32
        || get32(bytes.data() + 36) != 7 || get32(bytes.data() + 40) != 18
        || get32(bytes.data() + 44) != 32 || get32(bytes.data() + 48) != 3
        || get32(bytes.data() + 52) != 8 || get32(bytes.data() + 56) != 49
        || get32(bytes.data() + 60) != 2 || nu != 3.f || temperature != 4.f
        || std::fabs(tau - 0.0003f) > 1e-10f
        || get32(bytes.data() + 76) != 1
        || get32(bytes.data() + 80) != TGMR_V2_DIRECTORY_BYTES
        || get32(bytes.data() + 84) != 0
        || get64(bytes.data() + 88) != TGMR_V2_HEADER_BYTES
        || get64(bytes.data() + 96) != TGMR_V2_DIRECTORY_BYTES
        || get64(bytes.data() + 104) != TGMR_V2_PAYLOAD_OFFSET) {
        throw std::runtime_error("unsupported TGMR v2 architecture contract");
    }
    const std::uint64_t payloadBytes = get64(bytes.data() + 112);
    const std::uint64_t fileBytes = get64(bytes.data() + 120);
    static const char schema[] = "rawtherapee-xtrans-tgmr-v2-k32-s9-q8";
    const auto expectedSchema = sha256(schema, sizeof(schema) - 1);
    std::array<std::uint8_t, 32> actualSchema{};
    std::copy(bytes.begin() + 288, bytes.begin() + 320, actualSchema.begin());
    const std::array<std::uint8_t, 32> zero{};
    std::array<std::uint8_t, 32> corpusIdentity{};
    std::array<std::uint8_t, 32> configurationIdentity{};
    std::array<std::uint8_t, 32> attributionIdentity{};
    std::array<std::uint8_t, 32> trainerIdentity{};
    std::copy(bytes.begin() + 128, bytes.begin() + 160, corpusIdentity.begin());
    std::copy(bytes.begin() + 160, bytes.begin() + 192, configurationIdentity.begin());
    std::copy(bytes.begin() + 224, bytes.begin() + 256, attributionIdentity.begin());
    std::copy(bytes.begin() + 256, bytes.begin() + 288, trainerIdentity.begin());
    if (payloadBytes != PHASE_PAYLOAD_BYTES
        || fileBytes != bytes.size() || fileBytes != TGMR_V2_PAYLOAD_OFFSET + payloadBytes
        || actualSchema != expectedSchema || corpusIdentity == zero
        || configurationIdentity == zero || attributionIdentity == zero
        || trainerIdentity == zero
        || !allZero(bytes.data() + 320, 32) || !allZero(bytes.data() + 384, 128)) {
        throw std::runtime_error("TGMR v2 sizes or reserved header bytes are invalid");
    }
    const std::uint8_t *directory = bytes.data() + TGMR_V2_HEADER_BYTES;
    if (get32(directory + 0) != SECTION_PHASE_DATA
        || get32(directory + 4) != ENCODING_K32_S9_Q8
        || get32(directory + 8) != 0 || get32(directory + 12) != 0
        || get64(directory + 16) != TGMR_V2_PAYLOAD_OFFSET
        || get64(directory + 24) != payloadBytes || get64(directory + 32) != 0
        || !allZero(directory + 72, 24)
        || !allZero(bytes.data() + TGMR_V2_HEADER_BYTES + TGMR_V2_DIRECTORY_BYTES,
                    TGMR_V2_PAYLOAD_OFFSET - TGMR_V2_HEADER_BYTES - TGMR_V2_DIRECTORY_BYTES)) {
        throw std::runtime_error("TGMR v2 section directory is noncanonical");
    }
    ModelV2Inspection inspection;
    inspection.identity.modelRevision = get32(bytes.data() + 28);
    std::copy(bytes.begin() + 128, bytes.begin() + 160, inspection.identity.corpusSha256.begin());
    std::copy(bytes.begin() + 160, bytes.begin() + 192,
              inspection.identity.trainerConfigurationSha256.begin());
    std::copy(bytes.begin() + 224, bytes.begin() + 256,
              inspection.identity.attributionSha256.begin());
    std::copy(bytes.begin() + 256, bytes.begin() + 288,
              inspection.identity.trainerRevisionSha256.begin());
    std::copy(bytes.begin() + 192, bytes.begin() + 224, inspection.payloadSha256.begin());
    std::copy(bytes.begin() + AUTHENTICATION_OFFSET,
              bytes.begin() + AUTHENTICATION_OFFSET + AUTHENTICATION_BYTES,
              inspection.containerAuthenticationSha256.begin());
    const auto actualPayload = sha256(bytes.data() + TGMR_V2_PAYLOAD_OFFSET,
                                      static_cast<std::size_t>(payloadBytes));
    std::array<std::uint8_t, 32> directoryPayload{};
    std::copy(directory + 40, directory + 72, directoryPayload.begin());
    if (actualPayload != inspection.payloadSha256 || actualPayload != directoryPayload) {
        throw std::runtime_error("TGMR v2 phase payload SHA-256 mismatch");
    }
    if (authenticatedDigest(bytes) != inspection.containerAuthenticationSha256) {
        throw std::runtime_error("TGMR v2 container authentication mismatch");
    }
    validatePhasePayload(std::vector<std::uint8_t>(
        bytes.begin() + TGMR_V2_PAYLOAD_OFFSET, bytes.end()));
    inspection.fileSha256 = sha256(bytes.data(), bytes.size());
    inspection.payloadBytes = payloadBytes;
    inspection.fileBytes = bytes.size();
    if (phasePayload) {
        phasePayload->assign(bytes.begin() + TGMR_V2_PAYLOAD_OFFSET, bytes.end());
    }
    return inspection;
}

void writeModelV2(
    const std::string &path,
    const ModelV2Identity &identity,
    const std::vector<std::uint8_t> &phasePayload,
    bool force)
{
    std::ifstream existing(path, std::ios::binary);
    if (existing.good() && !force) {
        throw std::runtime_error("refusing to replace existing TGMR v2 model: " + path);
    }
    const auto bytes = serializeModelV2(identity, phasePayload);
    inspectModelV2(bytes);
    const std::string temporary = path + ".tmp";
    std::FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
        throw std::runtime_error("cannot create TGMR v2 temporary output");
    }
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (std::fflush(file) != 0) ok = false;
    if (std::fclose(file) != 0) ok = false;
    if (!ok) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot write complete TGMR v2 model");
    }
    if (force) std::remove(path.c_str());
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot publish TGMR v2 model: "
                                 + std::string(std::strerror(errno)));
    }
}

std::string canonicalModelV2Json(const ModelV2Inspection &inspection)
{
    std::ostringstream output;
    output << "{\n"
        << "  \"architecture\": \"XTRANS_TGMR_K32_S9_Q8\",\n"
        << "  \"attribution_sha256\": \"" << hex(inspection.identity.attributionSha256) << "\",\n"
        << "  \"container_authentication_sha256\": \"" << hex(inspection.containerAuthenticationSha256) << "\",\n"
        << "  \"corpus_sha256\": \"" << hex(inspection.identity.corpusSha256) << "\",\n"
        << "  \"file_bytes\": " << inspection.fileBytes << ",\n"
        << "  \"file_sha256\": \"" << hex(inspection.fileSha256) << "\",\n"
        << "  \"format\": \"rawtherapee-xtrans-tgmr-model-v2\",\n"
        << "  \"model_revision\": " << inspection.identity.modelRevision << ",\n"
        << "  \"payload_bytes\": " << inspection.payloadBytes << ",\n"
        << "  \"payload_sha256\": \"" << hex(inspection.payloadSha256) << "\",\n"
        << "  \"trainer_configuration_sha256\": \""
        << hex(inspection.identity.trainerConfigurationSha256) << "\",\n"
        << "  \"trainer_revision_sha256\": \""
        << hex(inspection.identity.trainerRevisionSha256) << "\"\n"
        << "}\n";
    return output.str();
}

} // namespace tgmr
