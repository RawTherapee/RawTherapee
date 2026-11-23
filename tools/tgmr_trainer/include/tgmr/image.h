#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tgmr
{

struct LinearImage final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // The dimensions after applying orientation but before an optional JPEG
    // proxy scale.  They equal width/height for canonical full decoding.
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    std::uint16_t orientation = 1;
    std::string fileType;
    std::string iccIdentity;
    bool assumedSrgb = false;
    // Interleaved, row-major, linear-sRGB float64 values.  Values are not
    // clipped after the color transform.
    std::vector<double> rgb;
};

struct LoadedLinearImage final {
    LinearImage image;
    std::string fileSha256;
    std::uint64_t fileBytes = 0;
    bool proxy = false;
};

struct ImageClassification final {
    std::array<double, 3> channelMeans{};
    double luminanceMean = 0.0;
    double luminanceStddev = 0.0;
    double luminanceP01 = 0.0;
    double luminanceP99 = 0.0;
    double chromaRatioMean = 0.0;
    double hueDegrees = 0.0;
    double saturationMean = 0.0;
    double clippedBlackFraction = 0.0;
    double clippedWhiteFraction = 0.0;
    double gradientRms = 0.0;
    double laplacianRms = 0.0;
    double localContrast = 0.0;
    double jpegBlockiness = 0.0;
    std::array<std::uint64_t, 16> luminanceHistogram{};
    std::array<std::uint64_t, 12> hueHistogram{};
    std::array<std::uint64_t, 8> saturationHistogram{};
    // v1 called the 64-bit difference hash perceptual_hash.  Keep that field
    // for compatibility while naming both signatures explicitly in v2.
    std::string perceptualHash;
    std::string pHash;
    std::string decodedPixelSha256;
};

LinearImage loadLinearImage(const std::string &path);
// Batch intake reads JPEG bytes once, authenticates those exact bytes, and
// decodes either the full image or libjpeg's 1/8-resolution proxy.  Other
// supported formats retain the legacy decode path and are intended for final
// corpus intake rather than the Open Images proxy pass.
LoadedLinearImage loadLinearImageAndSha256(const std::string &path, bool jpegProxy = false);
ImageClassification classifyImage(const LinearImage &image);
std::string canonicalClassificationJson(
    const LinearImage &image,
    const ImageClassification &classification,
    const std::string &sourceId,
    const std::string &cacheFilename = std::string());
std::string canonicalProxyClassificationJson(
    const LinearImage &image,
    const ImageClassification &classification,
    const std::string &sourceId,
    const std::string &cacheFilename = std::string());

} // namespace tgmr
