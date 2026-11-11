#pragma once

#include "tgmr/image.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tgmr
{

struct ProposedPatch final {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    bool coverage = false;
    std::uint8_t coverageClass = 0;
};

// Select 75% uniform spatial samples and 25% deterministic coverage samples.
// The latter cover low/high brightness, chroma, texture, contrast, clipping,
// saturation, and eight hue sectors.  Positions are always distinct.
std::vector<ProposedPatch> proposePatches(
    const LinearImage &image,
    std::uint64_t seed,
    std::size_t count);

} // namespace tgmr
