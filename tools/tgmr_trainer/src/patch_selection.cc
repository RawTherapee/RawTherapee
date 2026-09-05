#include "tgmr/patch_selection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace tgmr
{
namespace
{

constexpr double PI = 3.141592653589793238462643383279502884;

class Random final
{
public:
    explicit Random(std::uint64_t seed) : state_(seed ? seed : 1) {}
    std::uint64_t next()
    {
        std::uint64_t value = state_;
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        state_ = value;
        return value * 0x2545f4914f6cdd1dULL;
    }
private:
    std::uint64_t state_;
};

struct Metrics final {
    double luminance = 0.0;
    double chroma = 0.0;
    double saturation = 0.0;
    double texture = 0.0;
    double contrast = 0.0;
    double clipping = 0.0;
    double hue = 0.0;
};

Metrics metrics(const LinearImage &image, std::uint32_t left, std::uint32_t top)
{
    Metrics result;
    std::array<double, 49> luma{};
    double hueX = 0.0;
    double hueY = 0.0;
    for (unsigned y = 0; y < 7; ++y) {
        for (unsigned x = 0; x < 7; ++x) {
            const double *rgb = image.rgb.data()
                + ((static_cast<std::size_t>(top + y) * image.width + left + x) * 3);
            const double r = std::max(0.0, std::min(1.0, rgb[0]));
            const double g = std::max(0.0, std::min(1.0, rgb[1]));
            const double b = std::max(0.0, std::min(1.0, rgb[2]));
            const double maximum = std::max({r, g, b});
            const double minimum = std::min({r, g, b});
            const double chroma = maximum - minimum;
            const double saturation = maximum > 0.0 ? chroma / maximum : 0.0;
            const double value = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            luma[y * 7 + x] = value;
            result.luminance += value;
            result.chroma += chroma;
            result.saturation += saturation;
            result.clipping += maximum >= 1.0 - 1.0 / 65535.0
                || maximum <= 1.0 / 65535.0;
            if (chroma > 1e-12) {
                double hue = maximum == r ? (g - b) / chroma
                    : maximum == g ? 2.0 + (b - r) / chroma
                    : 4.0 + (r - g) / chroma;
                hue *= PI / 3.0;
                hueX += std::cos(hue) * saturation;
                hueY += std::sin(hue) * saturation;
            }
        }
    }
    result.luminance /= 49.0;
    result.chroma /= 49.0;
    result.saturation /= 49.0;
    result.clipping /= 49.0;
    result.hue = std::atan2(hueY, hueX);
    for (unsigned y = 0; y < 7; ++y) {
        for (unsigned x = 0; x < 7; ++x) {
            const double value = luma[y * 7 + x];
            result.contrast += std::abs(value - result.luminance);
            if (x != 0) {
                const double difference = value - luma[y * 7 + x - 1];
                result.texture += difference * difference;
            }
            if (y != 0) {
                const double difference = value - luma[(y - 1) * 7 + x];
                result.texture += difference * difference;
            }
        }
    }
    result.contrast /= 49.0;
    result.texture = std::sqrt(result.texture / 84.0);
    return result;
}

double score(const Metrics &value, unsigned coverageClass)
{
    switch (coverageClass) {
        case 0: return -value.luminance;
        case 1: return value.luminance;
        case 2: return -value.chroma;
        case 3: return value.chroma;
        case 4: return -value.texture;
        case 5: return value.texture;
        case 6: return value.contrast;
        case 7: return value.clipping;
        case 8: return value.saturation;
        default: {
            const unsigned sector = coverageClass - 9;
            const double target = (sector + 0.5) * (2.0 * PI / 8.0) - PI;
            return value.saturation * std::cos(value.hue - target);
        }
    }
}

} // namespace

std::vector<ProposedPatch> proposePatches(
    const LinearImage &image,
    std::uint64_t seed,
    std::size_t count)
{
    if (image.width < 7 || image.height < 7
        || image.rgb.size() != static_cast<std::size_t>(image.width) * image.height * 3) {
        throw std::runtime_error("cannot sample malformed linear image");
    }
    const std::uint64_t columns = image.width - 6;
    const std::uint64_t rows = image.height - 6;
    const std::uint64_t possible = columns * rows;
    if (count > possible) throw std::runtime_error("requested more distinct patches than image permits");
    Random random(seed);
    std::set<std::uint64_t> used;
    std::vector<ProposedPatch> output;
    output.reserve(count);
    const std::size_t uniform = count - count / 4;
    while (output.size() < uniform) {
        const std::uint64_t index = random.next() % possible;
        if (!used.insert(index).second) continue;
        output.push_back({static_cast<std::uint32_t>(index % columns),
                          static_cast<std::uint32_t>(index / columns), false, 0});
    }
    const std::size_t candidateTarget = std::min<std::uint64_t>(
        possible, std::max<std::size_t>(4096, count * 32));
    std::vector<std::uint64_t> candidates;
    std::set<std::uint64_t> candidateSet;
    candidates.reserve(candidateTarget);
    while (candidates.size() < candidateTarget) {
        const std::uint64_t index = random.next() % possible;
        if (used.find(index) == used.end() && candidateSet.insert(index).second) {
            candidates.push_back(index);
        }
        if (used.size() + candidateSet.size() == possible) break;
    }
    std::vector<Metrics> candidateMetrics;
    candidateMetrics.reserve(candidates.size());
    for (std::uint64_t index : candidates) {
        candidateMetrics.push_back(metrics(image,
            static_cast<std::uint32_t>(index % columns),
            static_cast<std::uint32_t>(index / columns)));
    }
    for (std::size_t selection = uniform; selection < count; ++selection) {
        const unsigned coverageClass = static_cast<unsigned>((selection - uniform) % 17);
        std::size_t best = candidates.size();
        double bestScore = -std::numeric_limits<double>::infinity();
        for (std::size_t candidate = 0; candidate < candidates.size(); ++candidate) {
            if (used.find(candidates[candidate]) != used.end()) continue;
            const double candidateScore = score(candidateMetrics[candidate], coverageClass);
            if (candidateScore > bestScore
                || (candidateScore == bestScore
                    && (best == candidates.size() || candidates[candidate] < candidates[best]))) {
                bestScore = candidateScore;
                best = candidate;
            }
        }
        if (best == candidates.size()) {
            for (std::uint64_t index = 0; index < possible; ++index) {
                if (used.find(index) == used.end()) {
                    candidates.push_back(index);
                    candidateMetrics.push_back(metrics(image,
                        static_cast<std::uint32_t>(index % columns),
                        static_cast<std::uint32_t>(index / columns)));
                    best = candidates.size() - 1;
                    break;
                }
            }
        }
        const std::uint64_t index = candidates[best];
        used.insert(index);
        output.push_back({static_cast<std::uint32_t>(index % columns),
                          static_cast<std::uint32_t>(index / columns), true,
                          static_cast<std::uint8_t>(coverageClass)});
    }
    return output;
}

} // namespace tgmr
