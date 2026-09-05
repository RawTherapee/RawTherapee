#pragma once

#include "tgmr/sha256.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tgmr
{

constexpr std::uint32_t TGMR_V2_HEADER_BYTES = 512;
constexpr std::uint32_t TGMR_V2_DIRECTORY_BYTES = 96;
constexpr std::uint64_t TGMR_V2_PAYLOAD_OFFSET = 640;

struct ModelV2Identity final {
    std::uint32_t modelRevision = 1;
    std::array<std::uint8_t, 32> corpusSha256{};
    std::array<std::uint8_t, 32> trainerConfigurationSha256{};
    std::array<std::uint8_t, 32> attributionSha256{};
    std::array<std::uint8_t, 32> trainerRevisionSha256{};
};

struct ModelV2Inspection final {
    ModelV2Identity identity;
    std::array<std::uint8_t, 32> payloadSha256{};
    std::array<std::uint8_t, 32> containerAuthenticationSha256{};
    std::array<std::uint8_t, 32> fileSha256{};
    std::uint64_t payloadBytes = 0;
    std::uint64_t fileBytes = 0;
};

std::vector<std::uint8_t> serializeModelV2(
    const ModelV2Identity &identity,
    const std::vector<std::uint8_t> &phasePayload);

ModelV2Inspection inspectModelV2(
    const std::vector<std::uint8_t> &bytes,
    std::vector<std::uint8_t> *phasePayload = nullptr);

void writeModelV2(
    const std::string &path,
    const ModelV2Identity &identity,
    const std::vector<std::uint8_t> &phasePayload,
    bool force = false);

std::string canonicalModelV2Json(const ModelV2Inspection &inspection);

} // namespace tgmr
