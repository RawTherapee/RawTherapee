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
    int rawWidth, rawHeight;
    int recordedWidth, recordedHeight;
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

    // The distortion map uses the midpoint of the raw raster and the half-diagonal of CropArea.
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

class NikonLensCorrection final : public MetadataLensCorrection
{
public:
    NikonLensCorrection(const NefInfo &info, const FrameLayout &frame)
    {
        const auto distortion = info.block(5, 84);
        // Optional, Off and Required all carry usable coefficients. RT's
        // Distortion checkbox decides whether to apply them.
        if (distortion[4] < 1 || distortion[4] > 3
            || Exiv2::getULong(distortion + 16, info.order) != coefficients.size()) {
            throw std::runtime_error("unsupported Nikon distortion parameters");
        }
        for (size_t i = 0; i < coefficients.size(); ++i) {
            coefficients[i] = info.rational(distortion + 20 + 8 * i);
        }
        const double zoom = 1 + info.rational(distortion + 64);
        const auto center = info.block(8, 24);
        offsetX = info.rational(center + 4);
        offsetY = info.rational(center + 12);
        if (zoom <= 0 || std::abs(offsetX) > frame.recordedWidth / 2
            || std::abs(offsetY) > frame.recordedHeight / 2) {
            throw std::runtime_error("invalid Nikon lens framing");
        }
        inverseZoom = 1 / zoom;
        halfDiagonal = 0.5 * std::hypot(frame.recordedWidth, frame.recordedHeight);

        // Bound |P(r)| over the sensor before the resampler converts to integer coordinates.
        const double corner = std::hypot(frame.rawWidth * 0.5 + std::abs(offsetX),
                                         frame.rawHeight * 0.5 + std::abs(offsetY));
        const double maxRadius = corner / halfDiagonal;
        double bound = 0;
        for (double a : coefficients) {
            bound = bound * maxRadius + std::abs(a);
        }
        if (corner * (1 + maxRadius * bound) * inverseZoom + 2 * corner >= std::numeric_limits<int>::max()) {
            throw std::runtime_error("Nikon distortion exceeds the coordinate range");
        }
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
        // The caller already supplies coordinates relative to the raster midpoint.
        const double dx = x - centerX;
        const double dy = y - centerY;
        const double r = std::sqrt(dx * dx + dy * dy) * invRadius;
        double p = 0;
        for (const double a : coefficients) {
            p = p * r + a;
        }
        const double factor = (p * r + 1) * inverseZoom;
        x = centerX + dx * factor;
        y = centerY + dy * factor;
    }

    bool hasCameraFraming() const override { return true; }
    bool hasDistortionCorrection() const override { return true; }
    bool hasVignettingCorrection() const override { return false; }
    bool hasCACorrection() const override { return false; }

    void correctDistortionAndCA(double &x, double &y, int cx, int cy, int) const override { correctDistortion(x, y, cx, cy); }
    void correctCA(double &, double &, int, int, int) const override {}
    void processVignette(int, int, float **) const override {}
    void processVignette3Channels(int, int, float **) const override {}

private:
    std::array<double, 4> coefficients;
    double inverseZoom, halfDiagonal, offsetX, offsetY;
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
