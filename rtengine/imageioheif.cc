/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 RawTherapee contributors
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
#include "imageio.h"

#ifdef LIBHEIF
#include <libheif/heif.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

template<class T, auto release>
using HeifOwner = std::unique_ptr<T, decltype(release)>;

struct ExportError : std::runtime_error {
    int result;
    ExportError(int code, const std::string& message) : std::runtime_error(message), result(code) {}
};

void checkHeif(heif_error error, const char* operation)
{
    if (error.code != heif_error_Ok) {
        throw ExportError(error.code == heif_error_Memory_allocation_error ? IMIO_HEADERERROR :
            IMIO_VARIANTNOTSUPPORTED, std::string(operation) + ": " +
            (error.message ? error.message : "unknown libheif error"));
    }
}

struct HeifLifetime {
    HeifLifetime() { checkHeif(heif_init(nullptr), "initialize libheif"); }
    ~HeifLifetime() { heif_deinit(); }
};

// Create beside the destination so publication stays on the same filesystem.
// Never truncate the destination until encoding, writes, and close succeed.
class HeifOutput {
    std::string path;
    FILE* file = nullptr;

public:
    explicit HeifOutput(const Glib::ustring& destination)
    {
        std::unique_ptr<gchar, decltype(&g_free)> directory(g_path_get_dirname(destination.c_str()), g_free);
        path = std::string(directory.get()) + G_DIR_SEPARATOR_S + ".rawtherapee-heif-XXXXXX";
        int flags = O_RDWR;
#ifdef _WIN32
        flags |= O_BINARY;
#endif
        const int fd = g_mkstemp_full(path.data(), flags, 0666);
        if (fd < 0) {
            throw ExportError(IMIO_CANNOTWRITEFILE, "cannot create temporary output file");
        }
#ifdef _WIN32
        file = _fdopen(fd, "wb");
#else
        file = fdopen(fd, "wb");
#endif
        if (!file) {
#ifdef _WIN32
            _close(fd);
#else
            close(fd);
#endif
            g_remove(path.c_str());
            throw ExportError(IMIO_CANNOTWRITEFILE, "cannot open temporary output stream");
        }
    }

    ~HeifOutput()
    {
        if (file) {
            fclose(file);
        }
        if (!path.empty()) {
            g_remove(path.c_str());
        }
    }

    HeifOutput(const HeifOutput&) = delete;
    HeifOutput& operator=(const HeifOutput&) = delete;

    static heif_error write(heif_context*, const void* bytes, size_t size, void* user)
    {
        auto& output = *static_cast<HeifOutput*>(user);
        if (fwrite(bytes, 1, size, output.file) != size) {
            return {heif_error_Encoding_error, heif_suberror_Unspecified, "file write failed"};
        }
        return {heif_error_Ok, heif_suberror_Unspecified, nullptr};
    }

    void publish(heif_context* context, const Glib::ustring& destination)
    {
        heif_writer writer{1, write};
        const auto error = heif_context_write(context, &writer, this);
        if (error.code != heif_error_Ok) {
            throw ExportError(IMIO_CANNOTWRITEFILE,
                error.message ? error.message : "cannot write HEIF container");
        }
        FILE* completed = file;
        file = nullptr;
        if (fclose(completed) != 0) {
            throw ExportError(IMIO_CANNOTWRITEFILE, "cannot close HEIF output");
        }
        // GLib uses Unicode paths and replacement semantics on Windows too.
        if (g_rename(path.c_str(), destination.c_str()) != 0) {
            throw ExportError(IMIO_CANNOTWRITEFILE, "cannot publish HEIF output");
        }
        path.clear();
    }
};

} // namespace
#endif

bool rtengine::ImageIO::isHEIFExportAvailable()
{
#ifdef LIBHEIF
    try {
        HeifLifetime lifetime;
        const heif_encoder_descriptor* descriptor = nullptr;
        return heif_get_encoder_descriptors(heif_compression_HEVC, "x265", &descriptor, 1) == 1;
    } catch (const ExportError&) {
        return false;
    }
#else
    return false;
#endif
}

int rtengine::ImageIO::saveHEIF(const Glib::ustring& fname, int quality, int bps, int subSamp) const
{
#ifndef LIBHEIF
    if (pl) {
        pl->error("HEIF export is disabled in this build");
    }
    return IMIO_FILETYPENOTSUPPORTED;
#else
    try {
        if (getWidth() < 1 || getHeight() < 1 || getWidth() > std::numeric_limits<int>::max() / 6) {
            throw ExportError(IMIO_HEADERERROR, "invalid image dimensions");
        }
        if (quality < 0 || quality > 100 || (bps != 8 && bps != 10 && bps != 12) ||
            subSamp < 1 || subSamp > 3) {
            throw ExportError(IMIO_VARIANTNOTSUPPORTED, "invalid quality, bit depth, or chroma subsampling");
        }
        HeifLifetime lifetime;
        HeifOwner<heif_context, heif_context_free> context(heif_context_alloc(), heif_context_free);
        if (!context) {
            throw ExportError(IMIO_HEADERERROR, "cannot allocate libheif context");
        }
        heif_context_set_max_decoding_threads(context.get(), 0);
        const heif_encoder_descriptor* descriptor = nullptr;
        if (heif_get_encoder_descriptors(heif_compression_HEVC, "x265", &descriptor, 1) != 1) {
            throw ExportError(IMIO_FILETYPENOTSUPPORTED, "HEVC encoder unavailable; install or bundle the x265 codec");
        }
        heif_encoder* rawEncoder = nullptr;
        checkHeif(heif_context_get_encoder(context.get(), descriptor, &rawEncoder), "get x265 encoder");
        HeifOwner<heif_encoder, heif_encoder_release> encoder(rawEncoder, heif_encoder_release);
        checkHeif(heif_encoder_set_lossy_quality(encoder.get(), quality), "set quality");
        const char* chroma[] = {"420", "422", "444"};
        checkHeif(heif_encoder_set_parameter_string(encoder.get(), "chroma", chroma[subSamp - 1]), "set chroma");
        checkHeif(heif_encoder_set_parameter_string(encoder.get(), "preset", "fast"), "set preset");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:pools", "none"), "limit worker pools");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:frame-threads", "1"), "limit frame threads");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:wpp", "0"), "disable wavefront workers");
        checkHeif(heif_encoder_set_logging_level(encoder.get(), 0), "disable encoder logging");

        const int width = getWidth();
        const int height = getHeight();
        const auto limit = heif_context_get_security_limits(context.get())->max_image_size_pixels;
        if (limit && uint64_t(width) * uint64_t(height) > limit) {
            throw ExportError(IMIO_HEADERERROR, "image exceeds libheif pixel limit");
        }
        heif_image* rawImage = nullptr;
        checkHeif(heif_image_create(width, height, heif_colorspace_RGB, heif_chroma_444, &rawImage), "create RGB image");
        HeifOwner<heif_image, heif_image_release> image(rawImage, heif_image_release);
        const std::array<heif_channel, 3> channels{heif_channel_R, heif_channel_G, heif_channel_B};
        std::array<uint8_t*, 3> planes{};
        std::array<int, 3> strides{};
        for (size_t channel = 0; channel < channels.size(); ++channel) {
            checkHeif(heif_image_add_plane(image.get(), channels[channel], width, height, bps), "allocate RGB plane");
            planes[channel] = heif_image_get_plane(image.get(), channels[channel], &strides[channel]);
            if (!planes[channel] || strides[channel] < width * (bps == 8 ? 1 : 2)) {
                throw ExportError(IMIO_HEADERERROR, "invalid RGB plane stride");
            }
        }
        const bool floating = getBPS() == 32;
        std::vector<float> floatRow(floating ? size_t(width) * 3 : 0);
        std::vector<uint16_t> integerRow(floating ? 0 : size_t(width) * 3);
        const unsigned maximum = (1u << bps) - 1;
        if (pl) {
            pl->setProgressStr("PROGRESSBAR_SAVEHEIF");
            pl->setProgress(0.0);
        }
        for (int y = 0; y < height; ++y) {
            if (floating) {
                getScanline(y, reinterpret_cast<unsigned char*>(floatRow.data()), 32, true);
            } else {
                getScanline(y, reinterpret_cast<unsigned char*>(integerRow.data()), 16);
            }
            for (size_t channel = 0; channel < channels.size(); ++channel) {
                auto* row = planes[channel] + size_t(y) * strides[channel];
                for (int x = 0; x < width; ++x) {
                    const size_t offset = size_t(x) * 3 + channel;
                    uint16_t value;
                    if (floating) {
                        const float sample = floatRow[offset];
                        // NaN and negative values become black; +infinity becomes white.
                        value = !(sample > 0.f) ? 0 : sample >= 1.f ? maximum :
                            static_cast<uint16_t>(std::lround(double(sample) * maximum));
                    } else {
                        value = (uint32_t(integerRow[offset]) * maximum + 32767u) / 65535u;
                    }
                    if (bps == 8) {
                        row[x] = static_cast<uint8_t>(value);
                    } else {
                        // Planar >8-bit samples use native-endian uint16_t, with
                        // their significant bits in the low part of the word.
                        std::memcpy(row + size_t(x) * 2, &value, sizeof(value));
                    }
                }
            }
            if (pl && y % 100 == 0) {
                pl->setProgress(0.5 * (y + 1) / height);
            }
        }
        if (!profileData.empty()) {
            checkHeif(heif_image_set_raw_color_profile(image.get(), "prof", profileData.data(), profileData.size()), "attach ICC");
        }
        HeifOwner<heif_color_profile_nclx, heif_nclx_color_profile_free> nclx(
            heif_nclx_color_profile_alloc(), heif_nclx_color_profile_free);
        HeifOwner<heif_encoding_options, heif_encoding_options_free> options(
            heif_encoding_options_alloc(), heif_encoding_options_free);
        if (!nclx || !options) {
            throw ExportError(IMIO_HEADERERROR, "cannot allocate encoding options");
        }
        // The rendered ICC is authoritative. Do not guess its primaries or
        // transfer function; tag only the RGB-to-YCbCr matrix and full range.
        checkHeif(heif_nclx_color_profile_set_color_primaries(nclx.get(), 2), "set primaries");
        checkHeif(heif_nclx_color_profile_set_transfer_characteristics(nclx.get(), 2), "set transfer");
        checkHeif(heif_nclx_color_profile_set_matrix_coefficients(nclx.get(), 1), "set matrix");
        nclx->full_range_flag = 1;
        checkHeif(heif_image_set_nclx_color_profile(image.get(), nclx.get()), "set conversion profile");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:colorprim", "2"), "set bitstream primaries");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:transfer", "2"), "set bitstream transfer");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:colormatrix", "1"), "set bitstream matrix");
        checkHeif(heif_encoder_set_parameter(encoder.get(), "x265:range", "full"), "set bitstream range");
        options->output_nclx_profile = nclx.get();
        options->save_two_colr_boxes_when_ICC_and_nclx_available = false;
        options->macOS_compatibility_workaround_no_nclx_profile = true;
        options->image_orientation = heif_orientation_normal;
        heif_image_handle* rawHandle = nullptr;
        checkHeif(heif_context_encode_image(context.get(), image.get(), encoder.get(), options.get(), &rawHandle), "encode HEIF image");
        HeifOwner<heif_image_handle, heif_image_handle_release> handle(rawHandle, heif_image_handle_release);
        if (pl) {
            pl->setProgress(0.9);
        }
        if (!metadataInfo.filename().empty()) {
            // Read valid images even when they have no EXIF (e.g. a PNG).
            // STRIP uses an empty source and bypasses all metadata preparation.
            metadataInfo.load(false);
            auto metadata = metadataInfo.prepareExportMetadata(false, {}, true);
            // Explicit EXIF selection may retain source geometry. Describe the
            // rendered primary image, whose container orientation is normal.
            if (metadata.exif.findKey(Exiv2::ExifKey("Exif.Image.Orientation")) != metadata.exif.end()) {
                metadata.exif["Exif.Image.Orientation"] = uint16_t(1);
            }
            for (const auto& field : std::array<std::pair<const char*, int>, 4>{{
                {"Exif.Image.ImageWidth", width}, {"Exif.Image.ImageLength", height},
                {"Exif.Photo.PixelXDimension", width}, {"Exif.Photo.PixelYDimension", height}}}) {
                if (metadata.exif.findKey(Exiv2::ExifKey(field.first)) != metadata.exif.end()) {
                    metadata.exif[field.first] = uint32_t(field.second);
                }
            }
            Exiv2::Blob exif;
            Exiv2::ExifParser::encode(exif, Exiv2::littleEndian, metadata.exif);
            // This serializer deliberately follows Exiv2's JPEG-compatible
            // filtering. It can still return a block larger than APP1 permits.
            if (exif.empty() || exif.size() > 65527) {
                throw ExportError(IMIO_CANNOTWRITEFILE, "EXIF serialization failed or exceeds the Exiv2 JPEG limit");
            }
            checkHeif(heif_context_add_exif_metadata(context.get(), handle.get(),
                exif.data(), static_cast<int>(exif.size())), "attach EXIF");
            auto xmp = metadataInfo.prepareHEIFXmp(metadata);
            for (const auto& field : std::array<std::pair<const char*, int>, 5>{{
                {"Xmp.tiff.Orientation", 1}, {"Xmp.tiff.ImageWidth", width},
                {"Xmp.tiff.ImageLength", height}, {"Xmp.exif.PixelXDimension", width},
                {"Xmp.exif.PixelYDimension", height}}}) {
                if (xmp.findKey(Exiv2::XmpKey(field.first)) != xmp.end()) {
                    xmp[field.first] = std::to_string(field.second);
                }
            }
            if (!xmp.empty()) {
                std::string packet;
                if (Exiv2::XmpParser::encode(packet, xmp,
                    Exiv2::XmpParser::useCompactFormat) != 0 || packet.empty() ||
                    packet.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                    throw ExportError(IMIO_CANNOTWRITEFILE, "XMP serialization failed or exceeds the libheif size limit");
                }
                checkHeif(heif_context_add_XMP_metadata(context.get(), handle.get(),
                    packet.data(), static_cast<int>(packet.size())), "attach XMP");
            }
        }
        HeifOutput output(fname);
        output.publish(context.get(), fname);
        if (pl) {
            pl->setProgressStr("PROGRESSBAR_READY");
            pl->setProgress(1.0);
        }
        return IMIO_SUCCESS;
    } catch (const ExportError& error) {
        if (pl) {
            pl->error(std::string("HEIF export: ") + error.what());
        }
        return error.result;
    } catch (const std::bad_alloc&) {
        if (pl) {
            pl->error("HEIF export: cannot allocate image memory");
        }
        return IMIO_HEADERERROR;
    } catch (const std::exception& error) {
        if (pl) {
            pl->error(std::string("HEIF export metadata: ") + error.what());
        }
        return IMIO_CANNOTWRITEFILE;
    }
#endif
}
