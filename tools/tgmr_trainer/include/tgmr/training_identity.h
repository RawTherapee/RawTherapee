#pragma once

#include "tgmr/training.h"

#include <array>
#include <cstdint>
#include <string>

namespace tgmr
{

const char *trainingBackendName(TrainingBackend backend);

// Canonical, language-neutral description of every fitting option that can
// affect a checkpoint.  Model export hashes this representation directly from
// the authenticated phase checkpoints rather than trusting a caller-supplied
// label.
std::string canonicalFitConfigurationJson(const FitConfiguration &configuration);
std::array<std::uint8_t, 32> fitConfigurationSha256(
    const FitConfiguration &configuration);

// SHA-256 over a sorted path/digest manifest of the trainer CMake input,
// sources, headers, and the cJSON source compiled into the standalone tool.
std::array<std::uint8_t, 32> trainerRevisionSha256();

std::string canonicalTrainingIdentityJson(const FitConfiguration &configuration);

} // namespace tgmr
