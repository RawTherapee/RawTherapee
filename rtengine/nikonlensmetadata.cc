/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 RawTherapee developers
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "lensmetadata.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rtengine
{
namespace
{

struct FrameLayout {
    int rawWidth;
    int rawHeight;
    int recordedWidth;
    int recordedHeight;
};

std::optional<FrameLayout> readUncroppedFrame(const Exiv2::ExifData &exif)
{
    const auto crop = exif.findKey(Exiv2::ExifKey("Exif.Nikon3.CropHiSpeed"));
    const auto area = exif.findKey(Exiv2::ExifKey(0x0045, "Nikon3"));
    if (crop == exif.end() || crop->typeId() != Exiv2::unsignedShort || crop->count() != 7
        || area == exif.end() || area->typeId() != Exiv2::unsignedShort || area->count() != 4) {
        return std::nullopt;
    }

    // Uncropped FX (11) and native DX (12) both cover the whole sensor.
    const auto mode = to_long(crop, 0);
    const int rawWidth = to_long(crop, 1), rawHeight = to_long(crop, 2);
    if ((mode != 11 && mode != 12) || to_long(crop, 3) != rawWidth || to_long(crop, 4) != rawHeight
        || to_long(crop, 5) != 0 || to_long(crop, 6) != 0) {
        return std::nullopt;
    }

    // Both corrections use the midpoint of the raw raster and the half-diagonal of CropArea.
    const int width = to_long(area, 2), height = to_long(area, 3);
    if (width == 0 || height == 0
        || 2 * to_long(area, 0) + width != rawWidth || 2 * to_long(area, 1) + height != rawHeight) {
        return std::nullopt;
    }
    return FrameLayout{rawWidth, rawHeight, width, height};
}

// Offsets in this directory are relative to the nested TIFF header.
class NefInfo
{
public:
    explicit NefInfo(const std::vector<Exiv2::byte> &blob)
    {
        if (blob.size() < 18 || std::memcmp(blob.data(), "Nikon", 6) != 0
            || blob[6] != 1 || blob[7] < 1 || blob[7] > 3 || blob[8] || blob[9]) {
            throw std::runtime_error("unsupported Nikon NEFInfo header");
        }
        tiff = blob.data() + 10;
        size = blob.size() - 10;
        if (std::memcmp(tiff, "II", 2) == 0) {
            order = Exiv2::littleEndian;
        } else if (std::memcmp(tiff, "MM", 2) == 0) {
            order = Exiv2::bigEndian;
        } else {
            throw std::runtime_error("invalid Nikon NEFInfo byte order");
        }

        const size_t offset = Exiv2::getULong(tiff + 4, order);
        if (Exiv2::getUShort(tiff + 2, order) != 42 || offset < 8 || offset > size - 6) {
            throw std::runtime_error("invalid Nikon NEFInfo directory");
        }
        entries = Exiv2::getUShort(tiff + offset, order);
        if (entries > (size - offset - 6) / 12) {
            throw std::runtime_error("truncated Nikon NEFInfo directory");
        }
        directory = tiff + offset + 2;
    }

    const Exiv2::byte *block(unsigned tag, size_t length) const
    {
        const Exiv2::byte *result = nullptr;
        for (size_t i = 0; i < entries; ++i) {
            const auto entry = directory + 12 * i;
            if (Exiv2::getUShort(entry, order) != tag) {
                continue;
            }
            const size_t offset = Exiv2::getULong(entry + 8, order);
            if (Exiv2::getUShort(entry + 2, order) != Exiv2::undefined
                || Exiv2::getULong(entry + 4, order) != length
                || result || offset < 8 || offset > size || length > size - offset
                || (tiff + offset < directory + 12 * entries + 4 && tiff + offset + length > directory - 2)
                || std::memcmp(tiff + offset, "0100", 4) != 0) {
                throw std::runtime_error("invalid Nikon lens correction block");
            }
            result = tiff + offset;
        }
        if (!result) {
            throw std::runtime_error("missing Nikon lens correction block");
        }
        return result;
    }

    double rational(const Exiv2::byte *p) const
    {
        const auto value = Exiv2::getRational(p, order);
        if (!value.second) {
            throw std::runtime_error("invalid Nikon lens correction rational");
        }
        return double(value.first) / value.second;
    }

    Exiv2::ByteOrder order;

private:
    const Exiv2::byte *tiff, *directory;
    size_t size, entries;
};

// NEFInfo stores coefficients in descending powers of radius and omits the constant 1.
template<size_t N>
double radialPolynomial(const std::array<double, N> &coefficients, double r)
{
    double p = 0;
    for (double a : coefficients) {
        p = p * r + a;
    }
    return 1 + p * r;
}

class NikonLensCorrection final : public MetadataLensCorrection
{
public:
    NikonLensCorrection(const NefInfo &info, const FrameLayout &frame) :
        rawWidth(frame.rawWidth), rawHeight(frame.rawHeight)
    {
        const auto center = info.block(8, 24);
        offsetX = info.rational(center + 4);
        offsetY = info.rational(center + 12);
        if (std::abs(offsetX) > frame.recordedWidth / 2 || std::abs(offsetY) > frame.recordedHeight / 2) {
            throw std::runtime_error("invalid Nikon optical center");
        }
        halfDiagonal = 0.5 * std::hypot(frame.recordedWidth, frame.recordedHeight);
        const double corner = std::hypot(frame.rawWidth * 0.5 + std::abs(offsetX),
                                         frame.rawHeight * 0.5 + std::abs(offsetY));
        const double maxRadius = corner / halfDiagonal;

        try {
            readVignetting(info, maxRadius);
        } catch (const std::runtime_error &) {
            // An invalid vignetting profile can accompany valid distortion data.
        }
        try {
            readDistortion(info, corner, maxRadius);
        } catch (const std::runtime_error &) {
            if (!vignetting) {
                throw;
            }
        }
    }

private:
    void readDistortion(const NefInfo &info, double corner, double maxRadius)
    {
        const auto distortion = info.block(5, 84);
        // RT's Distortion checkbox controls application for all recognized lens statuses.
        if (distortion[4] < 1 || distortion[4] > 3
            || Exiv2::getULong(distortion + 16, info.order) != coefficients.size()) {
            throw std::runtime_error("unsupported Nikon distortion parameters");
        }
        for (size_t i = 0; i < coefficients.size(); ++i) {
            coefficients[i] = info.rational(distortion + 20 + 8 * i);
        }
        const double zoom = 1 + info.rational(distortion + 64);
        if (zoom <= 0) {
            throw std::runtime_error("invalid Nikon distortion zoom");
        }
        // Bound |P(r)| over the sensor before the resampler converts to integer coordinates.
        double bound = 0;
        for (double a : coefficients) {
            bound = bound * maxRadius + std::abs(a);
        }
        const double invZoom = 1 / zoom;
        if (corner * (1 + maxRadius * bound) * invZoom + 2 * corner >= (std::numeric_limits<int>::max)()) {
            throw std::runtime_error("Nikon distortion exceeds the coordinate range");
        }
        inverseZoom = invZoom;
    }

    void readVignetting(const NefInfo &info, double maxRadius)
    {
        const auto block = info.block(6, 116);
        std::array<double, 8> gain;
        if (block[4] < 1 || block[4] > 3 || Exiv2::getULong(block + 16, info.order) != gain.size()) {
            return;
        }
        const double strength = info.rational(block + 0x60);
        if (strength < 0) {
            return;
        }
        // A checked RT Vignetting box requests the full curve on a camera-Off file.
        const double scale = strength == 0 ? 1 : strength;
        for (size_t i = 0; i < gain.size(); ++i) {
            gain[i] = scale * info.rational(block + 20 + 8 * i);
        }
        if (gain == std::array<double, 8>{}) {
            return;
        }

        // Interval Horner evaluation bounds the gain between sample points too.
        // Subdivision tightens the bounds for polynomials with alternating signs.
        constexpr int intervals = 32;
        for (int i = 0; i < intervals; ++i) {
            const double lo = maxRadius * i / intervals;
            const double hi = maxRadius * (i + 1) / intervals;
            double lower = 0;
            double upper = 0;
            for (double a : gain) {
                lower = lower * (lower < 0 ? hi : lo) + a;
                upper = upper * (upper < 0 ? lo : hi) + a;
            }
            lower = 1 + lower * (lower < 0 ? hi : lo);
            upper = 1 + upper * (upper < 0 ? lo : hi);
            if (!(lower > 0 && upper <= (std::numeric_limits<float>::max)() / 65535.0)) {
                return;
            }
        }
        vignetting = gain;
    }

public:
    void initVignetting(int, int, const procparams::CoarseTransformParams &, int sensorLeft, int sensorTop) override
    {
        rawLeft = sensorLeft;
        rawTop = sensorTop;
    }

    void initCorrections(int, int, const procparams::CoarseTransformParams &coarse, int rawRotationDeg, double pixelSkip) override
    {
        centerX = offsetX / pixelSkip;
        centerY = offsetY / pixelSkip;
        invRadius = pixelSkip / halfDiagonal;
        switch ((rawRotationDeg + coarse.rotate) % 360) {
            case 90:
                std::swap(centerX, centerY);
                centerX = -centerX;
                break;
            case 180:
                centerX = -centerX;
                centerY = -centerY;
                break;
            case 270:
                std::swap(centerX, centerY);
                centerY = -centerY;
                break;
        }
        if (coarse.hflip) {
            centerX = -centerX;
        }
        if (coarse.vflip) {
            centerY = -centerY;
        }
    }

    void correctDistortion(double &x, double &y, int, int) const override
    {
        if (!hasDistortionCorrection()) {
            return;
        }
        // The caller already supplies coordinates relative to the raster midpoint.
        const double dx = x - centerX;
        const double dy = y - centerY;
        const double r = std::sqrt(dx * dx + dy * dy) * invRadius;
        const double factor = radialPolynomial(coefficients, r) * inverseZoom;
        x = centerX + dx * factor;
        y = centerY + dy * factor;
    }

    bool hasCameraFraming() const override { return hasDistortionCorrection(); }
    bool hasDistortionCorrection() const override { return inverseZoom > 0; }
    bool hasVignettingCorrection() const override { return vignetting.has_value(); }
    bool hasCACorrection() const override { return false; }

    void correctDistortionAndCA(double &x, double &y, int cx, int cy, int) const override { correctDistortion(x, y, cx, cy); }
    void correctCA(double &, double &, int, int, int) const override {}
    void processVignette(int width, int height, float **data) const override { applyGain(width, height, data, 1); }
    void processVignette3Channels(int width, int height, float **data) const override { applyGain(width, height, data, 3); }

private:
    void applyGain(int width, int height, float **data, int channels) const
    {
        if (!vignetting || rawLeft < 0 || rawTop < 0 || width > rawWidth - rawLeft || height > rawHeight - rawTop) {
            return;
        }
        const double cx = (rawWidth - 1) * 0.5 + offsetX - rawLeft;
        const double cy = (rawHeight - 1) * 0.5 + offsetY - rawTop;
        const double invH = 1 / halfDiagonal;
#ifdef _OPENMP
        #pragma omp parallel for
#endif
        for (int y = 0; y < height; ++y) {
            const double dy = (y - cy) * invH;
            float *pixel = data[y];
            for (int x = 0; x < width; ++x) {
                const double dx = (x - cx) * invH;
                const double r = std::sqrt(dx * dx + dy * dy);
                const float gain = radialPolynomial(*vignetting, r);
                for (int c = 0; c < channels; ++c) {
                    *pixel++ *= gain;
                }
            }
        }
    }

    std::optional<std::array<double, 8>> vignetting;
    int rawWidth;
    int rawHeight;
    int rawLeft = 0;
    int rawTop = 0;
    std::array<double, 4> coefficients{};
    double inverseZoom = 0, halfDiagonal, offsetX, offsetY;
    double centerX = 0, centerY = 0, invRadius = 0;
};

} // namespace

std::unique_ptr<MetadataLensCorrection> MetadataLensCorrectionFinder::findNikonCorrection(const FramesMetaData *meta)
{
    Exiv2Metadata metadata(meta->getFileName());
    metadata.load();
    const auto &exif = metadata.exifData();
    const auto frame = readUncroppedFrame(exif);
    if (!frame) {
        return nullptr;
    }
    for (const auto &datum : exif) {
        if (datum.tag() != 0xc7d5 || datum.typeId() != Exiv2::undefined) {
            continue;
        }
        const auto group = datum.groupName();
        if (group.compare(0, 8, "SubImage") != 0) {
            continue;
        }
        const auto photo = exif.findKey(Exiv2::ExifKey(0x0106, group));
        const auto width = exif.findKey(Exiv2::ExifKey(0x0100, group));
        const auto height = exif.findKey(Exiv2::ExifKey(0x0101, group));
        if (photo == exif.end() || photo->count() != 1 || to_long(photo) != 32803 // CFA
            || width == exif.end() || width->count() != 1 || to_long(width) != frame->rawWidth
            || height == exif.end() || height->count() != 1 || to_long(height) != frame->rawHeight) {
            continue;
        }
        // The supported metadata directories are under 1 KiB. Bound the copy
        // before trusting the size declared by the containing TIFF.
        if (datum.size() < 18 || datum.size() > 65536) {
            return nullptr;
        }
        std::vector<Exiv2::byte> bytes(datum.size());
        if (size_t(datum.value().copy(bytes.data(), Exiv2::invalidByteOrder)) != bytes.size()) {
            return nullptr;
        }
        return std::make_unique<NikonLensCorrection>(NefInfo(bytes), *frame);
    }
    return nullptr;
}

} // namespace rtengine
