#include "tgmr/camera_matrices.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace tgmr
{
namespace
{

std::array<double, 9> derive(const std::array<int, 9> &dcraw)
{
    static constexpr double xyzFromRgb[9] = {
        0.412453, 0.357580, 0.180423,
        0.212671, 0.715160, 0.072169,
        0.019334, 0.119193, 0.950227,
    };
    std::array<double, 9> result{};
    for (unsigned camera = 0; camera < 3; ++camera) {
        for (unsigned rgb = 0; rgb < 3; ++rgb) {
            for (unsigned xyz = 0; xyz < 3; ++xyz) {
                result[camera * 3 + rgb] += dcraw[camera * 3 + xyz]
                    / 10000.0 * xyzFromRgb[xyz * 3 + rgb];
            }
        }
        const double sum = result[camera * 3] + result[camera * 3 + 1]
            + result[camera * 3 + 2];
        if (!(sum > 0.0)) throw std::runtime_error("invalid frozen Fujifilm camera matrix");
        for (unsigned rgb = 0; rgb < 3; ++rgb) result[camera * 3 + rgb] /= sum;
    }
    return result;
}

const std::array<CameraMatrix, 7> &matrices()
{
    static const std::array<CameraMatrix, 7> values{{
        {0, "identity-linear-sRGB", false, {{1,0,0,0,1,0,0,0,1}}},
        {1, "FUJIFILM X-PRO1", false,
            derive({{10412,-3996,-993,-3721,11640,2361,-733,1540,6011}})},
        {2, "FUJIFILM X-T2", false,
            derive({{11434,-4948,-1210,-3746,12042,1903,-666,1479,5235}})},
        {3, "FUJIFILM X-T3", false,
            derive({{13426,-6334,-1177,-4244,12136,2371,-580,1303,5980}})},
        {4, "FUJIFILM X-H2S", false,
            derive({{12836,-5909,-1032,-3086,11132,2236,-35,872,5330}})},
        {101, "FUJIFILM X-T1", true,
            derive({{8458,-2451,-855,-4597,12447,2407,-1475,2482,6526}})},
        {102, "FUJIFILM X-T5", true,
            derive({{11809,-5358,-1141,-4248,12164,2343,-514,1097,5848}})},
    }};
    return values;
}

} // namespace

const CameraMatrix &cameraMatrix(std::uint16_t id)
{
    for (const CameraMatrix &matrix : matrices()) {
        if (matrix.id == id) return matrix;
    }
    throw std::runtime_error("unknown frozen Fujifilm camera-matrix ID");
}

std::string canonicalCameraMatrixJson()
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(12)
        << "{\n  \"format\": \"rawtherapee-tgmr-camera-matrices-v1\",\n"
        << "  \"matrices\": [\n";
    const auto &values = matrices();
    for (std::size_t index = 0; index < values.size(); ++index) {
        const CameraMatrix &matrix = values[index];
        output << "    {\"camera\": \"" << matrix.camera << "\", \"held_out\": "
            << (matrix.heldOut ? "true" : "false") << ", \"id\": " << matrix.id
            << ", \"linear_srgb_to_camera\": [";
        for (std::size_t value = 0; value < 9; ++value) {
            if (value) output << ", ";
            output << matrix.linearSrgbToCamera[value];
        }
        output << "]}" << (index + 1 == values.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    return output.str();
}

} // namespace tgmr
