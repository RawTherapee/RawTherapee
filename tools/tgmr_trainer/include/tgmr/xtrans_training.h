#pragma once

#include "tgmr/training.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tgmr
{

struct PhaseContract final {
    std::uint32_t index = 0;
    std::uint32_t originX = 0;
    std::uint32_t originY = 0;
    std::array<std::uint32_t, 49> observedIndices{};
    std::uint32_t sampledCenterChannel = 0;
    std::array<std::uint32_t, 2> targetChannels{};
};

struct PhaseCheckpoint final {
    PhaseContract phase;
    std::uint64_t sampleCount = 0;
    FitConfiguration configuration;
    MixtureModel model;
    std::array<std::uint8_t, 32> corpusPayloadSha256{};
};

std::array<PhaseContract, 18> xtransPhaseContracts();

std::vector<double> loadPhaseTrainingMatrix(
    const std::string &corpusPath,
    const PhaseContract &phase,
    std::uint64_t &sampleCount,
    std::array<std::uint8_t, 32> &corpusPayloadSha256,
    std::uint64_t sourceLimit = 0);

void writePhaseCheckpoint(
    const std::string &path,
    const PhaseCheckpoint &checkpoint,
    bool force = false);

PhaseCheckpoint readPhaseCheckpoint(const std::string &path);

std::vector<std::uint8_t> exportPhasePayload(
    const std::array<PhaseCheckpoint, 18> &checkpoints,
    double tau = 0.0003);

// Validate the complete fixed K32/S9/q8 runtime payload independently of the
// outer TGMR v2 container.
void validatePhasePayload(const std::vector<std::uint8_t> &payload);

} // namespace tgmr
