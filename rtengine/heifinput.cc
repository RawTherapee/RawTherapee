/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "heifinput.h"
#include "imageio.h"
#include <iostream>
#include <stdexcept>

#ifdef LIBHEIF
#include <libheif/heif.h>
#include <libheif/heif_items.h>
#include <libheif/heif_properties.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace {
template<class T, auto release> using Owner = std::unique_ptr<T, decltype(release)>;
struct InputError : std::runtime_error {
    int result;
    InputError(int code, const std::string& text) : std::runtime_error(text), result(code) {}
};
void check(heif_error error, const char* operation)
{
    if (error.code != heif_error_Ok) {
        throw InputError(IMIO_READERROR, std::string(operation) + ": " + error.message);
    }
}
void closeFile(FILE* file) { fclose(file); }
struct Lifetime {
    Lifetime() { check(heif_init(nullptr), "initialize libheif"); }
    ~Lifetime() { heif_deinit(); }
};
#ifdef _WIN32
int seekFile(FILE* file, int64_t offset, int origin) { return _fseeki64(file, offset, origin); }
int64_t tellFile(FILE* file) { return _ftelli64(file); }
#else
int seekFile(FILE* file, int64_t offset, int origin) { return fseeko(file, offset, origin); }
int64_t tellFile(FILE* file) { return ftello(file); }
#endif
// Keep the file alive until the context is destroyed: libheif reads lazily.
// GLib opens UTF-8 Windows paths; 64-bit callbacks support large containers.
struct Input {
    Lifetime lifetime;
    Owner<FILE, closeFile> file{nullptr, closeFile};
    int64_t size = 0;
    heif_reader reader{};
    Owner<heif_context, heif_context_free> context{heif_context_alloc(), heif_context_free};
    Owner<heif_image_handle, heif_image_handle_release> primary{nullptr, heif_image_handle_release};
    explicit Input(const Glib::ustring& path)
    {
        file.reset(g_fopen(path.c_str(), "rb"));
        if (!file) throw InputError(IMIO_CANNOTREADFILE, "cannot open input file");
        if (!context) throw std::bad_alloc();
        if (seekFile(file.get(), 0, SEEK_END) || (size = tellFile(file.get())) < 0 || seekFile(file.get(), 0, SEEK_SET)) {
            throw InputError(IMIO_CANNOTREADFILE, "cannot determine input size");
        }
        reader.reader_api_version = 1;
        reader.get_position = [](void* data) { return tellFile(static_cast<Input*>(data)->file.get()); };
        reader.read = [](void* out, size_t bytes, void* data) {
            return fread(out, 1, bytes, static_cast<Input*>(data)->file.get()) == bytes ? 0 : 1;
        };
        reader.seek = [](int64_t offset, void* data) {
            auto& self = *static_cast<Input*>(data);
            return offset < 0 || offset > self.size ? 1 : seekFile(self.file.get(), offset, SEEK_SET);
        };
        reader.wait_for_file_size = [](int64_t target, void* data) {
            return target >= 0 && target <= static_cast<Input*>(data)->size ?
                heif_reader_grow_status_size_reached : heif_reader_grow_status_size_beyond_eof;
        };
        heif_context_set_max_decoding_threads(context.get(), 0);
        check(heif_context_read_from_reader(context.get(), &reader, this, nullptr), "read container");
        heif_image_handle* handle = nullptr;
        check(heif_context_get_primary_image_handle(context.get(), &handle), "read primary image");
        primary.reset(handle);
        if (heif_image_handle_is_premultiplied_alpha(handle)) {
            throw InputError(IMIO_VARIANTNOTSUPPORTED, "premultiplied alpha is not supported; flatten transparency before opening");
        }
        heif_item_id id;
        check(heif_context_get_primary_image_ID(context.get(), &id), "read primary ID");
        const auto type = heif_item_get_item_type(context.get(), id);
        if (type == heif_item_type_grid) {
            heif_image_tiling tiling{};
            check(heif_image_handle_get_image_tiling(handle, 0, &tiling), "read grid");
            for (uint32_t y = 0; y < tiling.num_rows; ++y) {
                for (uint32_t x = 0; x < tiling.num_columns; ++x) {
                    heif_item_id tile;
                    check(heif_image_handle_get_grid_image_tile_id(handle, 0, x, y, &tile), "read grid tile");
                    if (heif_item_get_item_type(context.get(), tile) != heif_item_type_hvc1) {
                        throw InputError(IMIO_VARIANTNOTSUPPORTED, "only HEVC image grids are supported");
                    }
                }
            }
        } else if (type != heif_item_type_hvc1) {
            throw InputError(IMIO_VARIANTNOTSUPPORTED, "only HEVC primary still images are supported (AVIF is not HEVC)");
        }
    }
    std::pair<int, int> dimensions() const
    {
        int width = heif_image_handle_get_ispe_width(primary.get());
        int height = heif_image_handle_get_ispe_height(primary.get());
        const auto id = heif_image_handle_get_item_id(primary.get());
        const int count = heif_item_get_transformation_properties(context.get(), id, nullptr, 0);
        std::vector<heif_property_id> properties(count);
        heif_item_get_transformation_properties(context.get(), id, properties.data(), count);
        for (auto property : properties) {
            switch (heif_item_get_property_type(context.get(), id, property)) {
                case heif_item_property_type_transform_rotation:
                    if (heif_item_get_property_transform_rotation_ccw(context.get(), id, property) % 180) std::swap(width, height);
                    break;
                case heif_item_property_type_transform_crop: {
                    int left = 0, top = 0, right = 0, bottom = 0;
                    heif_item_get_property_transform_crop_borders(context.get(), id, property,
                        width, height, &left, &top, &right, &bottom);
                    // Match libheif's crop intersection with the decoded image.
                    const int64_t w = int64_t(width) - std::max(0, left) - std::max(0, right);
                    const int64_t h = int64_t(height) - std::max(0, top) - std::max(0, bottom);
                    if (w <= 0 || h <= 0 || w > width || h > height) {
                        throw InputError(IMIO_READERROR, "invalid crop dimensions");
                    }
                    width = w;
                    height = h;
                    break;
                }
                default: break; // Mirrors do not change dimensions.
            }
        }
        if (width <= 0 || height <= 0) throw InputError(IMIO_READERROR, "invalid image dimensions");
        return {width, height};
    }
    int bits() const
    {
        return std::max(heif_image_handle_get_luma_bits_per_pixel(primary.get()),
            heif_image_handle_get_chroma_bits_per_pixel(primary.get()));
    }
};
void requireDecoder()
{
    const int count = heif_get_decoder_descriptors(heif_compression_HEVC, nullptr, 0);
    std::vector<const heif_decoder_descriptor*> descriptors(count);
    heif_get_decoder_descriptors(heif_compression_HEVC, descriptors.data(), count);
    for (const auto* descriptor : descriptors) {
        const char* id = heif_decoder_descriptor_get_id_name(descriptor);
        if (id && std::strcmp(id, "libde265") == 0) return;
    }
    throw InputError(IMIO_VARIANTNOTSUPPORTED, "HEVC decoder unavailable; install or bundle the libde265 codec");
}
void validateColor(const heif_color_profile_nclx* profile)
{
    if (profile && (profile->transfer_characteristics == 16 || profile->transfer_characteristics == 18)) {
        throw InputError(IMIO_VARIANTNOTSUPPORTED, "PQ/HLG HDR input is not supported; convert to SDR before opening");
    }
    if (profile) {
        switch (profile->matrix_coefficients) {
            case 0: case 1: case 4: case 5: case 6: case 7: case 9: break;
            default: throw InputError(IMIO_VARIANTNOTSUPPORTED,
                "unsupported or unspecified YCbCr matrix; convert to a supported SDR color profile");
        }
    }
}
Owner<heif_color_profile_nclx, heif_nclx_color_profile_free> handleNclx(const heif_image_handle* handle)
{
    heif_color_profile_nclx* profile = nullptr;
    const auto error = heif_image_handle_get_nclx_color_profile(handle, &profile);
    if (error.code != heif_error_Color_profile_does_not_exist) check(error, "read NCLX");
    return {profile, heif_nclx_color_profile_free};
}
std::string makeICC(const heif_color_profile_nclx* nclx)
{
    if (!nclx) throw InputError(IMIO_VARIANTNOTSUPPORTED, "missing ICC and NCLX color description; assign an SDR color profile before opening");
    validateColor(nclx);
    const int primaries = nclx->color_primaries;
    if (primaries != 1 && primaries != 9 && primaries != 12) {
        throw InputError(IMIO_VARIANTNOTSUPPORTED, "unsupported NCLX primaries; supported SDR spaces are BT.709, BT.2020 and Display P3");
    }
    cmsToneCurve* curve = nullptr;
    switch (nclx->transfer_characteristics) {
        case 1: case 6: case 14: case 15: {
            const double alpha = nclx->transfer_characteristics == 15 ? 1.09929682680944 : 1.099;
            const double threshold = nclx->transfer_characteristics == 15 ? 0.081242858298635 : 0.081;
            const double parameters[]{1.0 / 0.45, 1.0 / alpha, (alpha - 1.0) / alpha, 1.0 / 4.5, threshold};
            curve = cmsBuildParametricToneCurve(nullptr, 4, parameters);
            break;
        }
        case 13: {
            const double parameters[]{2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045};
            curve = cmsBuildParametricToneCurve(nullptr, 4, parameters);
            break;
        }
        case 4: curve = cmsBuildGamma(nullptr, 2.2); break;
        case 5: curve = cmsBuildGamma(nullptr, 2.8); break;
        case 8: curve = cmsBuildGamma(nullptr, 1.0); break;
        default: throw InputError(IMIO_VARIANTNOTSUPPORTED, "unsupported NCLX transfer function; convert to a supported SDR color profile");
    }
    Owner<cmsToneCurve, cmsFreeToneCurve> trc(curve, cmsFreeToneCurve);
    if (!trc) throw std::bad_alloc();
    const cmsCIExyY white{nclx->color_primary_white_x, nclx->color_primary_white_y, 1.0};
    const cmsCIExyYTRIPLE colors{{nclx->color_primary_red_x, nclx->color_primary_red_y, 1.0},
        {nclx->color_primary_green_x, nclx->color_primary_green_y, 1.0},
        {nclx->color_primary_blue_x, nclx->color_primary_blue_y, 1.0}};
    cmsToneCurve* curves[]{trc.get(), trc.get(), trc.get()};
    Owner<void, cmsCloseProfile> profile(cmsCreateRGBProfile(&white, &colors, curves), cmsCloseProfile);
    if (!profile) throw InputError(IMIO_VARIANTNOTSUPPORTED, "cannot build ICC profile from NCLX");
    cmsUInt32Number size = 0;
    if (!cmsSaveProfileToMem(profile.get(), nullptr, &size)) throw InputError(IMIO_READERROR, "cannot size NCLX ICC profile");
    std::string result(size, '\0');
    if (!cmsSaveProfileToMem(profile.get(), result.data(), &size)) throw InputError(IMIO_READERROR, "cannot serialize NCLX ICC profile");
    return result;
}
} // namespace
#endif

namespace rtengine {
namespace {
int report(ProgressListener* listener, int code, const std::string& error)
{
    const std::string message = "HEIF input: " + error;
    if (listener) listener->error(message);
    else std::cerr << message << '\n';
    return code;
}
}
int ImageIO::getHEIFSampleFormat(const Glib::ustring& fname, IIOSampleFormat& format,
    IIOSampleArrangement& arrangement, ProgressListener* listener)
{
    format = IIOSF_UNKNOWN;
    arrangement = IIOSA_UNKNOWN;
#ifdef LIBHEIF
    try {
        Input input(fname);
        requireDecoder();
        const auto nclx = handleNclx(input.primary.get());
        validateColor(nclx.get());
        const int bits = input.bits();
        if (bits > 16) throw InputError(IMIO_VARIANTNOTSUPPORTED, "unsupported input precision");
        // An unknown header depth uses 16-bit storage conservatively. Decoded
        // plane depths, not this hint, determine the actual scaling below.
        format = bits > 0 && bits <= 8 ? IIOSF_UNSIGNED_CHAR : IIOSF_UNSIGNED_SHORT;
        arrangement = IIOSA_CHUNKY;
        return IMIO_SUCCESS;
    } catch (const InputError& error) { return report(listener, error.result, error.what()); }
      catch (const std::exception& error) { return report(listener, IMIO_READERROR, error.what()); }
#else
    return report(listener, IMIO_FILETYPENOTSUPPORTED, "HEIF support is disabled in this build");
#endif
}
int ImageIO::loadHEIF(const Glib::ustring& fname)
{
#ifdef LIBHEIF
    try {
        Input input(fname);
        requireDecoder();
        auto sourceNclx = handleNclx(input.primary.get());
        validateColor(sourceNclx.get());
        if (pl) { pl->setProgressStr("PROGRESSBAR_LOADHEIF"); pl->setProgress(0.0); }
        Owner<heif_decoding_options, heif_decoding_options_free> options(heif_decoding_options_alloc(), heif_decoding_options_free);
        if (!options) throw std::bad_alloc();
        options->decoder_id = "libde265";
        options->strict_decoding = true;
        options->convert_hdr_to_8bit = false;
        options->output_image_nclx_profile_passthrough = true;
        options->num_codec_threads = 1;
        if (!sourceNclx) {
            // libheif 1.23.6 can drop the VUI-only NCLX description when
            // converting to planar RGB. Read it before that conversion so
            // matrix/range and HDR checks never depend on guessed defaults.
            heif_image* nativeRaw = nullptr;
            check(heif_decode_image(input.primary.get(), &nativeRaw, heif_colorspace_undefined,
                heif_chroma_undefined, options.get()), "decode source color description");
            Owner<heif_image, heif_image_release> native(nativeRaw, heif_image_release);
            heif_color_profile_nclx* profile = nullptr;
            const auto error = heif_image_get_nclx_color_profile(native.get(), &profile);
            sourceNclx.reset(profile);
            if (error.code != heif_error_Color_profile_does_not_exist) check(error, "read source color description");
            if (!sourceNclx && heif_image_get_colorspace(native.get()) != heif_colorspace_RGB) {
                throw InputError(IMIO_VARIANTNOTSUPPORTED, "missing YCbCr color description; assign an SDR color profile before opening");
            }
            validateColor(sourceNclx.get());
        }
        heif_image* raw = nullptr;
        check(heif_decode_image(input.primary.get(), &raw, heif_colorspace_RGB, heif_chroma_444, options.get()), "decode primary image");
        Owner<heif_image, heif_image_release> image(raw, heif_image_release);
        const auto* nclx = sourceNclx.get();
        const int width = heif_image_get_width(image.get(), heif_channel_R);
        const int height = heif_image_get_height(image.get(), heif_channel_R);
        const int bits = heif_image_get_bits_per_pixel_range(image.get(), heif_channel_R);
        if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() / 6 || bits < 8 || bits > 16 || (bits > 8 && getBPS() < 16)) {
            throw InputError(IMIO_VARIANTNOTSUPPORTED, "invalid dimensions or unsupported decoded precision");
        }
        std::array<const uint8_t*, 3> planes{};
        std::array<int, 3> strides{};
        const std::array<heif_channel, 3> channels{{heif_channel_R, heif_channel_G, heif_channel_B}};
        for (int c = 0; c < 3; ++c) {
            planes[c] = heif_image_get_plane_readonly(image.get(), channels[c], &strides[c]);
            if (!planes[c] || strides[c] < width * (bits > 8 ? 2 : 1) ||
                heif_image_get_width(image.get(), channels[c]) != width || heif_image_get_height(image.get(), channels[c]) != height ||
                heif_image_get_bits_per_pixel_range(image.get(), channels[c]) != bits) {
                throw InputError(IMIO_READERROR, "invalid decoded RGB planes");
            }
        }
        const size_t iccSize = heif_image_handle_get_raw_color_profile_size(input.primary.get());
        std::string icc;
        if (iccSize) {
            if (iccSize > size_t(std::numeric_limits<int>::max())) throw InputError(IMIO_READERROR, "ICC profile too large");
            icc.resize(iccSize);
            check(heif_image_handle_get_raw_color_profile(input.primary.get(), icc.data()), "read ICC");
        } else {
            icc = makeICC(nclx);
        }
        Owner<void, cmsCloseProfile> profile(cmsOpenProfileFromMem(icc.data(), icc.size()), cmsCloseProfile);
        if (!profile || cmsGetColorSpace(profile.get()) != cmsSigRgbData) {
            throw InputError(IMIO_VARIANTNOTSUPPORTED, "invalid or non-RGB ICC profile");
        }
        allocate(width, height);
        // Image16 dispatches setScanline by sampleFormat, not its bps argument.
        // Rows below are always normalized uint16_t, including 8-bit sources.
        sampleFormat = IIOSF_UNSIGNED_SHORT;
        std::vector<uint16_t> row(size_t(width) * 3);
        const uint32_t maximum = (1u << bits) - 1;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < 3; ++c) {
                    const auto* pixel = planes[c] + size_t(y) * strides[c] + size_t(x) * (bits > 8 ? 2 : 1);
                    uint16_t sample = *pixel;
                    if (bits > 8) std::memcpy(&sample, pixel, sizeof(sample));
                    if (sample > maximum) throw InputError(IMIO_READERROR, "decoded sample exceeds plane precision");
                    row[3 * x + c] = (uint32_t(sample) * 65535 + maximum / 2) / maximum;
                }
            }
            setScanline(y, reinterpret_cast<const unsigned char*>(row.data()), 16);
            if (pl && y % 100 == 0) pl->setProgress(double(y + 1) / height);
        }
        auto saved = std::make_unique<char[]>(icc.size());
        std::memcpy(saved.get(), icc.data(), icc.size());
        if (embProfile) cmsCloseProfile(embProfile);
        embProfile = profile.release();
        deleteLoadedProfileData();
        loadedProfileLength = icc.size();
        loadedProfileData = saved.release();
        sampleFormat = bits <= 8 ? IIOSF_UNSIGNED_CHAR : IIOSF_UNSIGNED_SHORT;
        sampleArrangement = IIOSA_CHUNKY;
        if (pl) { pl->setProgressStr("PROGRESSBAR_READY"); pl->setProgress(1.0); }
        return IMIO_SUCCESS;
    } catch (const InputError& error) { return report(pl, error.result, error.what()); }
      catch (const std::exception& error) { return report(pl, IMIO_READERROR, error.what()); }
#else
    return report(pl, IMIO_FILETYPENOTSUPPORTED, "HEIF support is disabled in this build");
#endif
}
HEIFMetadata readHEIFMetadata(const Glib::ustring& filename)
{
#ifdef LIBHEIF
    Input input(filename);
    const auto dimensions = input.dimensions();
    HEIFMetadata result{{}, {}, dimensions.first, dimensions.second, input.bits()};
    const int count = heif_image_handle_get_number_of_metadata_blocks(input.primary.get(), nullptr);
    std::vector<heif_item_id> ids(count);
    heif_image_handle_get_list_of_metadata_block_IDs(input.primary.get(), nullptr, ids.data(), count);
    for (const auto id : ids) {
        const char* type = heif_image_handle_get_metadata_type(input.primary.get(), id);
        const char* contentType = heif_image_handle_get_metadata_content_type(input.primary.get(), id);
        const bool exif = type && std::strcmp(type, "Exif") == 0;
        if (!exif && !(type && std::strcmp(type, "mime") == 0 && contentType &&
            std::strcmp(contentType, "application/rdf+xml") == 0)) continue;
        const size_t size = heif_image_handle_get_metadata_size(input.primary.get(), id);
        // Bound the additional Exiv2 parse allocation, retaining libheif's own
        // context security limits as well. Ordinary metadata is far smaller.
        if (size > 64u * 1024 * 1024) throw InputError(IMIO_READERROR, "metadata item exceeds 64 MiB limit");
        std::vector<Exiv2::byte> bytes(size);
        check(heif_image_handle_get_metadata(input.primary.get(), id, bytes.data()), "read metadata");
        if (exif) {
            if (size < 12) throw InputError(IMIO_READERROR, "truncated EXIF offset/header");
            const uint32_t offset = uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 | uint32_t(bytes[2]) << 8 | bytes[3];
            if (offset > size - 12) throw InputError(IMIO_READERROR, "EXIF TIFF offset exceeds metadata bounds");
            const auto* tiff = bytes.data() + 4 + offset;
            if (!((tiff[0] == 'I' && tiff[1] == 'I' && tiff[2] == 42 && tiff[3] == 0) ||
                  (tiff[0] == 'M' && tiff[1] == 'M' && tiff[2] == 0 && tiff[3] == 42))) {
                throw InputError(IMIO_READERROR, "invalid EXIF TIFF header");
            }
            Exiv2::ExifParser::decode(result.exif, tiff, size - 4 - offset);
        } else if (Exiv2::XmpParser::decode(result.xmp, std::string(bytes.begin(), bytes.end())) != 0) {
            throw InputError(IMIO_READERROR, "invalid XMP packet");
        }
    }
    return result;
#else
    throw std::runtime_error("HEIF support is disabled in this build");
#endif
}
} // namespace rtengine
