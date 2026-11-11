#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace tgmr
{

struct CameraMatrix final {
    std::uint16_t id = 0;
    const char *camera = nullptr;
    bool heldOut = false;
    std::array<double, 9> linearSrgbToCamera{};
};

// ID zero is the required identity transform.  IDs 1..4 are production
// training augmentations; IDs 101..102 are held out from fitting and reserved
// for validation/test augmentation.  The coefficients are derived from the
// named dcraw matrices in rtdata/cammatrices.json using the same XYZ/sRGB
// multiplication and per-camera-channel normalization as dcraw.
const CameraMatrix &cameraMatrix(std::uint16_t id);
std::string canonicalCameraMatrixJson();

} // namespace tgmr
