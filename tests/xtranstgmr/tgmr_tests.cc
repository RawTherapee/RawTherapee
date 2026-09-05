#include "rtengine/xtrans_tgmr.h"
#include "rtengine/procparams.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{

constexpr int CFA[6][6] = {
    {1, 2, 1, 1, 0, 1},
    {0, 1, 0, 2, 1, 2},
    {1, 2, 1, 1, 0, 1},
    {1, 0, 1, 1, 2, 1},
    {2, 1, 2, 0, 1, 0},
    {1, 0, 1, 1, 2, 1}
};

void require(bool condition, const std::string &message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int modulo(int value, int divisor)
{
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

std::shared_ptr<const rtengine::TgmrXTransModel> loadReviewed()
{
    const char *path = std::getenv("RT_XTRANS_TGMR_MODEL");
    if (!path || !*path) {
        std::cout << "SKIP: RT_XTRANS_TGMR_MODEL is not set\n";
        std::exit(77);
    }
    const rtengine::TgmrXTransLoadResult loaded =
        rtengine::loadTgmrXTransModel(path);
    require(static_cast<bool>(loaded),
            std::string("reviewed model failed [")
                + rtengine::tgmrXTransErrorCodeName(loaded.code) + "]: "
                + loaded.message);
    const std::string digest = rtengine::tgmrXTransModelDigest(*loaded.model);
    const std::string origin = rtengine::tgmrXTransModelOrigin(*loaded.model);
    require(
        (origin == "reviewed-research-v1"
            && digest == "6279b6a593ef4b595b1aff246182682b60c7eea701373ee2eb9f80cfcb50485c")
        || origin == "custom-v2",
        "reviewed TGMR v1 or compatible custom v2 identity changed");
    const rtengine::TgmrXTransLoadResult cachedFirst =
        rtengine::loadCachedTgmrXTransModel(path);
    const rtengine::TgmrXTransLoadResult cachedSecond =
        rtengine::loadCachedTgmrXTransModel(path);
    require(cachedFirst && cachedSecond
                && cachedFirst.model.get() == cachedSecond.model.get(),
            "TGMR process cache did not reuse the authenticated model");
    return cachedFirst.model;
}

std::array<float, 3> sceneRgb(const std::string &scene, int x, int y, int width, int height)
{
    const float fx = static_cast<float>(x) / std::max(1, width - 1);
    const float fy = static_cast<float>(y) / std::max(1, height - 1);
    if (scene == "constant") {
        return {{0.42f, 0.42f, 0.42f}};
    }
    if (scene == "rgb-gradient") {
        return {{0.05f + 0.85f * fx, 0.1f + 0.75f * fy,
                 0.15f + 0.7f * (0.6f * fx + 0.4f * fy)}};
    }
    if (scene == "chromatic-gradient") {
        return {{0.1f + 0.8f * fx, 0.15f + 0.7f * (1.f - fx),
                 0.1f + 0.8f * fy}};
    }
    if (scene == "edge") {
        return x < width / 2
            ? std::array<float, 3>{{0.95f, 0.15f, 0.12f}}
            : std::array<float, 3>{{0.35f, 0.35f, 0.35f}};
    }
    if (scene == "point") {
        return std::abs(x - width / 2) + std::abs(y - height / 2) == 0
            ? std::array<float, 3>{{1.f, 0.05f, 0.9f}}
            : std::array<float, 3>{{0.08f, 0.08f, 0.08f}};
    }
    if (scene == "star") {
        const int dx = std::abs(x - width / 2);
        const int dy = std::abs(y - height / 2);
        const float peak = dx == 0 && dy == 0 ? 1.f
            : dx + dy == 1 ? 0.45f : 0.f;
        return {{0.02f + 0.85f * peak, 0.025f + 0.7f * peak,
                 0.04f + 0.95f * peak}};
    }
    const float pattern = ((x * 3 + y * 5) % 11) < 5 ? 0.85f : 0.12f;
    return {{pattern, 0.2f + 0.6f * (1.f - pattern),
             ((x + 2 * y) % 7) < 3 ? 0.8f : 0.15f}};
}

std::vector<float> mosaic(
    const std::string &scene,
    int width,
    int height,
    int originX = 0,
    int originY = 0,
    const int cfa[6][6] = CFA)
{
    std::vector<float> result(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::array<float, 3> rgb = sceneRgb(scene, x, y, width, height);
            const int channel = cfa[modulo(y + originY, 6)][modulo(x + originX, 6)];
            result[static_cast<std::size_t>(y) * width + x] = rgb[channel] * 65535.f;
        }
    }
    return result;
}

struct Output final {
    std::vector<float> red;
    std::vector<float> green;
    std::vector<float> blue;
    rtengine::TgmrXTransRunResult run;
};

Output execute(
    const std::vector<float> &input,
    int width,
    int height,
    const int cfa[6][6],
    const std::shared_ptr<const rtengine::TgmrXTransModel> &model,
    int originX,
    int originY,
    bool scalar)
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    require(input.size() == pixels, "TGMR test input size differs from geometry");
    Output result;
    result.red.resize(pixels);
    result.green.resize(pixels);
    result.blue.resize(pixels);
    result.run = rtengine::demosaicTgmrXTransReference(
        input.data(), result.red.data(), result.green.data(), result.blue.data(),
        width, height, cfa, model, originX, originY, scalar);
    require(static_cast<bool>(result.run),
            std::string("TGMR execution failed [")
                + rtengine::tgmrXTransErrorCodeName(result.run.code) + "]: "
                + result.run.message);
    return result;
}

void requireNativeSamples(
    const std::vector<float> &input,
    const Output &output,
    int width,
    int height,
    const int cfa[6][6],
    int originX,
    int originY)
{
    const std::array<const std::vector<float> *, 3> planes =
        {{&output.red, &output.green, &output.blue}};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            const int channel = cfa[modulo(y + originY, 6)][modulo(x + originX, 6)];
            require((*planes[channel])[index] == input[index],
                    "TGMR changed a physical CFA sample");
        }
    }
}

void compare(const Output &left, const Output &right, double rmsLimit, double maxLimit)
{
    double sum = 0.0;
    double maximum = 0.0;
    std::size_t count = 0;
    const std::array<const std::vector<float> *, 3> a =
        {{&left.red, &left.green, &left.blue}};
    const std::array<const std::vector<float> *, 3> b =
        {{&right.red, &right.green, &right.blue}};
    for (unsigned channel = 0; channel < 3; ++channel) {
        require(a[channel]->size() == b[channel]->size(), "TGMR output sizes differ");
        for (std::size_t index = 0; index < a[channel]->size(); ++index) {
            const double difference =
                (static_cast<double>((*a[channel])[index]) - (*b[channel])[index]) / 65535.0;
            sum += difference * difference;
            maximum = std::max(maximum, std::abs(difference));
            ++count;
        }
    }
    const double rms = std::sqrt(sum / count);
    require(rms <= rmsLimit && maximum <= maxLimit,
            "TGMR parity exceeded tolerance: rms=" + std::to_string(rms)
                + " max=" + std::to_string(maximum));
}

int contract()
{
    require(std::strcmp(rtengine::TGMR_XTRANS_METHOD, "tgmr") == 0,
            "TGMR method identifier changed");
    rtengine::setTgmrXTransDataDirectory("/tmp/rawtherapee-data");
    require(rtengine::tgmrXTransDefaultModelPath()
                == "/tmp/rawtherapee-data/models/xtrans-tgmr-v2.tgmr",
            "TGMR installed model path changed");
    for (const rtengine::TgmrXTransErrorCode code : {
            rtengine::TgmrXTransErrorCode::NONE,
            rtengine::TgmrXTransErrorCode::IO,
            rtengine::TgmrXTransErrorCode::SIZE,
            rtengine::TgmrXTransErrorCode::DIGEST,
            rtengine::TgmrXTransErrorCode::FORMAT,
            rtengine::TgmrXTransErrorCode::CFA,
            rtengine::TgmrXTransErrorCode::ALLOCATION,
            rtengine::TgmrXTransErrorCode::NONFINITE,
            rtengine::TgmrXTransErrorCode::INTERNAL}) {
        require(std::strlen(rtengine::tgmrXTransErrorCodeName(code)) > 0,
                "TGMR error code has no stable name");
    }
    const rtengine::TgmrXTransLoadResult unset =
        rtengine::loadTgmrXTransModel("");
    require(!unset && unset.code == rtengine::TgmrXTransErrorCode::IO,
            "empty TGMR path did not return IO");
    const rtengine::TgmrXTransLoadResult missing =
        rtengine::loadTgmrXTransModel("/this/path/does/not/exist/tgmr.bin");
    require(!missing && missing.code == rtengine::TgmrXTransErrorCode::IO,
            "missing TGMR model did not return IO");

    char path[] = "/tmp/rawtherapee-tgmr-size-XXXXXX";
    const int descriptor = mkstemp(path);
    require(descriptor >= 0, "cannot create TGMR size test fixture");
    const unsigned char byte = 0;
    require(write(descriptor, &byte, 1) == 1 && close(descriptor) == 0,
            "cannot write TGMR size test fixture");
    const rtengine::TgmrXTransLoadResult shortModel =
        rtengine::loadTgmrXTransModel(path);
    std::remove(path);
    require(!shortModel && shortModel.code == rtengine::TgmrXTransErrorCode::SIZE,
            "short TGMR model did not return SIZE");

    char wrongPath[] = "/tmp/rawtherapee-tgmr-digest-XXXXXX";
    const int wrongDescriptor = mkstemp(wrongPath);
    require(wrongDescriptor >= 0, "cannot create TGMR digest test fixture");
    require(ftruncate(wrongDescriptor, 6073164) == 0
                && close(wrongDescriptor) == 0,
            "cannot size TGMR digest test fixture");
    const rtengine::TgmrXTransLoadResult wrongModel =
        rtengine::loadTgmrXTransModel(wrongPath);
    std::remove(wrongPath);
    require(!wrongModel && wrongModel.code == rtengine::TgmrXTransErrorCode::DIGEST,
            "wrong TGMR artifact did not return DIGEST");

    char profilePath[] = "/tmp/rawtherapee-tgmr-profile-XXXXXX";
    const int profileDescriptor = mkstemp(profilePath);
    require(profileDescriptor >= 0 && close(profileDescriptor) == 0,
            "cannot create TGMR profile test path");
    rtengine::procparams::ProcParams saved;
    saved.raw.xtranssensor.method = rtengine::TGMR_XTRANS_METHOD;
    require(saved.save(profilePath) == 0, "cannot save TGMR PP3 profile");
    rtengine::procparams::ProcParams loadedProfile;
    require(loadedProfile.load(profilePath) == 0,
            "cannot reload TGMR PP3 profile");
    std::remove(profilePath);
    require(loadedProfile.raw.xtranssensor.method == rtengine::TGMR_XTRANS_METHOD,
            "TGMR PP3 method did not round-trip");
    return 0;
}

int reviewedParity()
{
    const auto model = loadReviewed();
    const std::array<std::string, 7> scenes = {{
        "constant", "rgb-gradient", "chromatic-gradient", "edge",
        "point", "star", "periodic"}};
    for (const std::string &scene : scenes) {
        const int width = 141;
        const int height = 133;
        const std::vector<float> input = mosaic(scene, width, height);
        const Output scalar = execute(input, width, height, CFA, model, 0, 0, true);
        const Output optimized = execute(input, width, height, CFA, model, 0, 0, false);
        compare(scalar, optimized, 1e-6, 1e-5);
        requireNativeSamples(input, scalar, width, height, CFA, 0, 0);
        requireNativeSamples(input, optimized, width, height, CFA, 0, 0);
    }

    constexpr int matrices[8][4] = {
        {1, 0, 0, 1}, {0, -1, 1, 0}, {-1, 0, 0, -1}, {0, 1, -1, 0},
        {-1, 0, 0, 1}, {1, 0, 0, -1}, {0, 1, 1, 0}, {0, -1, -1, 0}
    };
    std::set<std::array<int, 36>> variants;
    for (const auto &matrix : matrices) {
        for (int oy = 0; oy < 6; ++oy) {
            for (int ox = 0; ox < 6; ++ox) {
                std::array<int, 36> variant {{}};
                for (int y = 0; y < 6; ++y) {
                    for (int x = 0; x < 6; ++x) {
                        variant[static_cast<std::size_t>(y * 6 + x)] = CFA[
                            modulo(matrix[2] * x + matrix[3] * y + oy, 6)
                        ][modulo(matrix[0] * x + matrix[1] * y + ox, 6)];
                    }
                }
                variants.insert(variant);
            }
        }
    }
    require(variants.size() == 18, "X-Trans test did not derive 18 unique matrices");
    for (const auto &variant : variants) {
        int cfa[6][6];
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 6; ++x) {
                cfa[y][x] = variant[static_cast<std::size_t>(y * 6 + x)];
            }
        }
        const std::vector<float> input = mosaic("constant", 7, 7, 0, 0, cfa);
        const Output output = execute(input, 7, 7, cfa, model, 0, 0, false);
        requireNativeSamples(input, output, 7, 7, cfa, 0, 0);
    }

    std::vector<float> nonfinite = mosaic("constant", 9, 9);
    nonfinite[40] = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> red(81), green(81), blue(81);
    const rtengine::TgmrXTransRunResult failed =
        rtengine::demosaicTgmrXTransReference(
            nonfinite.data(), red.data(), green.data(), blue.data(), 9, 9,
            CFA, model);
    require(!failed && failed.code == rtengine::TgmrXTransErrorCode::NONFINITE,
            "non-finite TGMR input did not fail safely");
    return 0;
}

int cropPhase()
{
    const auto model = loadReviewed();
    const int fullWidth = 151;
    const int fullHeight = 149;
    const std::vector<float> input = mosaic("periodic", fullWidth, fullHeight);
    const Output full = execute(input, fullWidth, fullHeight, CFA, model, 0, 0, false);
    const std::array<std::array<int, 2>, 5> origins =
        {{{{1, 0}}, {{0, 1}}, {{3, 3}}, {{5, 5}}, {{7, 11}}}};
    double allSquaredDifference = 0.0;
    double allMaximumDifference = 0.0;
    std::size_t allDifferenceCount = 0;
    for (const auto &origin : origins) {
        const int cropWidth = fullWidth - origin[0] - 7;
        const int cropHeight = fullHeight - origin[1] - 7;
        std::vector<float> crop(static_cast<std::size_t>(cropWidth) * cropHeight);
        for (int y = 0; y < cropHeight; ++y) {
            for (int x = 0; x < cropWidth; ++x) {
                crop[static_cast<std::size_t>(y) * cropWidth + x] = input[
                    static_cast<std::size_t>(y + origin[1]) * fullWidth + x + origin[0]];
            }
        }
        const Output cropped = execute(
            crop, cropWidth, cropHeight, CFA, model, origin[0], origin[1], false);
        requireNativeSamples(crop, cropped, cropWidth, cropHeight, CFA,
                             origin[0], origin[1]);
        const std::array<const std::vector<float> *, 3> fullPlanes =
            {{&full.red, &full.green, &full.blue}};
        const std::array<const std::vector<float> *, 3> cropPlanes =
            {{&cropped.red, &cropped.green, &cropped.blue}};
        double squaredDifference = 0.0;
        double maximumDifference = 0.0;
        std::size_t differenceCount = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            for (int y = 3; y < cropHeight - 3; ++y) {
                for (int x = 3; x < cropWidth - 3; ++x) {
                    const float expected = (*fullPlanes[channel])[
                        static_cast<std::size_t>(y + origin[1]) * fullWidth
                        + x + origin[0]];
                    const float actual = (*cropPlanes[channel])[
                        static_cast<std::size_t>(y) * cropWidth + x];
                    const double difference =
                        (static_cast<double>(expected) - actual) / 65535.0;
                    squaredDifference += difference * difference;
                    maximumDifference = std::max(maximumDifference,
                                                 std::abs(difference));
                    ++differenceCount;
                }
            }
        }
        const double rms = std::sqrt(squaredDifference / differenceCount);
        require(rms <= 1e-7 && maximumDifference <= 1e-6,
                "TGMR crop phase/interior parity exceeded tolerance: origin="
                    + std::to_string(origin[0]) + "," + std::to_string(origin[1])
                    + " rms=" + std::to_string(rms)
                    + " max=" + std::to_string(maximumDifference));
        allSquaredDifference += squaredDifference;
        allMaximumDifference = std::max(allMaximumDifference, maximumDifference);
        allDifferenceCount += differenceCount;
    }
    std::cout << "crop_parity_rms="
              << std::sqrt(allSquaredDifference / allDifferenceCount)
              << " crop_parity_max=" << allMaximumDifference << '\n';
    return 0;
}

std::uint32_t mix(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}

int researchDump(int width, int height, const char *path, bool scalar)
{
    require(width > 0 && height > 0, "TGMR dump dimensions must be positive");
    const auto model = loadReviewed();
    std::vector<float> input(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint32_t bits = mix(
                static_cast<std::uint32_t>(static_cast<std::size_t>(y) * width + x)
                ^ 0x517cc1b7U);
            input[static_cast<std::size_t>(y) * width + x] = 65535.f *
                (0.05f + 0.9f * static_cast<float>(bits & 0x00ffffffU)
                    / static_cast<float>(0x01000000U));
        }
    }
    const Output output = execute(input, width, height, CFA, model, 0, 0, scalar);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(stream), "cannot create TGMR integration dump");
    for (std::size_t pixel = 0; pixel < input.size(); ++pixel) {
        const std::array<float, 3> rgb = {{
            output.red[pixel] / 65535.f,
            output.green[pixel] / 65535.f,
            output.blue[pixel] / 65535.f}};
        stream.write(reinterpret_cast<const char *>(rgb.data()), sizeof(rgb));
    }
    require(static_cast<bool>(stream), "cannot write TGMR integration dump");
    return 0;
}

int engineBenchmark(int width, int height, int repetitions)
{
    require(width > 0 && height > 0 && repetitions > 0,
            "TGMR benchmark arguments must be positive");
    const auto model = loadReviewed();
    std::vector<float> input(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint32_t bits = mix(
                static_cast<std::uint32_t>(static_cast<std::size_t>(y) * width + x)
                ^ 0x517cc1b7U);
            input[static_cast<std::size_t>(y) * width + x] = 65535.f *
                (0.05f + 0.9f * static_cast<float>(bits & 0x00ffffffU)
                    / static_cast<float>(0x01000000U));
        }
    }
    execute(input, width, height, CFA, model, 0, 0, false);
    std::vector<std::uint64_t> timings;
    rtengine::TgmrXTransRunResult last;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const Output output = execute(input, width, height, CFA, model, 0, 0, false);
        last = output.run;
        timings.push_back(last.elapsedMicroseconds);
    }
    std::sort(timings.begin(), timings.end());
    const std::uint64_t median = timings[timings.size() / 2];
    const double throughput = static_cast<double>(last.pixelCount) / median;
    std::cout << "{\"avx2\":" << (last.avx2 ? "true" : "false")
              << ",\"height\":" << height
              << ",\"median_seconds\":" << median / 1e6
              << ",\"megapixels_per_second\":" << throughput
              << ",\"pixels\":" << last.pixelCount
              << ",\"threads\":" << last.workerCount
              << ",\"tile\":128,\"width\":" << width << "}\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        require(argc >= 2, "usage: rawtherapee-xtrans-tgmr-tests MODE");
        const std::string mode = argv[1];
        if (mode == "contract") {
            return contract();
        }
        if (mode == "reviewed-parity") {
            return reviewedParity();
        }
        if (mode == "crop-phase") {
            require(argc == 2, "crop-phase accepts no arguments");
            return cropPhase();
        }
        if (mode == "research-dump") {
            require(argc == 6,
                    "usage: rawtherapee-xtrans-tgmr-tests research-dump WIDTH HEIGHT OUTPUT scalar|optimized");
            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const std::string implementation = argv[5];
            require(implementation == "scalar" || implementation == "optimized",
                    "research-dump implementation must be scalar or optimized");
            return researchDump(width, height, argv[4], implementation == "scalar");
        }
        if (mode == "benchmark") {
            require(argc == 5,
                    "usage: rawtherapee-xtrans-tgmr-tests benchmark WIDTH HEIGHT REPETITIONS");
            return engineBenchmark(std::stoi(argv[2]), std::stoi(argv[3]),
                                   std::stoi(argv[4]));
        }
        throw std::runtime_error("unknown TGMR test mode: " + mode);
    } catch (const std::exception &error) {
        std::cerr << "xtrans TGMR test error: " << error.what() << '\n';
        return 2;
    }
}
