#include "tgmr/image.h"

#include "tgmr/sha256.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <csetjmp>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" {
#include <jpeglib.h>
#include <lcms2.h>
#include <png.h>
#include <tiffio.h>
}

namespace tgmr
{
namespace
{

constexpr double PI = 3.141592653589793238462643383279502884;

struct Decoded final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint16_t orientation = 1;
    std::uint8_t components = 3;
    std::string type;
    std::vector<std::uint16_t> rgb;
    std::vector<std::uint8_t> icc;
};

struct JpegError final {
    jpeg_error_mgr base;
    std::jmp_buf jump;
    char message[JMSG_LENGTH_MAX]{};
};

std::uint16_t exif16(const std::uint8_t *data, bool littleEndian)
{
    return littleEndian
        ? static_cast<std::uint16_t>(data[0] | (data[1] << 8))
        : static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

std::uint32_t exif32(const std::uint8_t *data, bool littleEndian)
{
    return littleEndian
        ? static_cast<std::uint32_t>(data[0])
            | (static_cast<std::uint32_t>(data[1]) << 8)
            | (static_cast<std::uint32_t>(data[2]) << 16)
            | (static_cast<std::uint32_t>(data[3]) << 24)
        : (static_cast<std::uint32_t>(data[0]) << 24)
            | (static_cast<std::uint32_t>(data[1]) << 16)
            | (static_cast<std::uint32_t>(data[2]) << 8)
            | static_cast<std::uint32_t>(data[3]);
}

std::uint16_t jpegExifOrientation(const jpeg_decompress_struct &decoder)
{
    for (jpeg_saved_marker_ptr marker = decoder.marker_list; marker; marker = marker->next) {
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(marker->data);
        const std::size_t size = marker->data_length;
        if (marker->marker != JPEG_APP0 + 1 || size < 14
            || std::memcmp(bytes, "Exif\0\0", 6) != 0) {
            continue;
        }
        const std::uint8_t *tiff = bytes + 6;
        const std::size_t tiffBytes = size - 6;
        const bool littleEndian = tiff[0] == 'I' && tiff[1] == 'I';
        if ((!littleEndian && !(tiff[0] == 'M' && tiff[1] == 'M'))
            || exif16(tiff + 2, littleEndian) != 42) {
            continue;
        }
        const std::uint32_t ifdOffset = exif32(tiff + 4, littleEndian);
        if (ifdOffset > tiffBytes - 2) continue;
        const std::uint16_t entries = exif16(tiff + ifdOffset, littleEndian);
        const std::size_t directory = static_cast<std::size_t>(ifdOffset) + 2;
        if (entries > (tiffBytes - directory) / 12) continue;
        for (std::uint16_t index = 0; index < entries; ++index) {
            const std::uint8_t *entry = tiff + directory + index * 12;
            if (exif16(entry, littleEndian) == 0x0112
                && exif16(entry + 2, littleEndian) == 3
                && exif32(entry + 4, littleEndian) == 1) {
                const std::uint16_t orientation = exif16(entry + 8, littleEndian);
                return orientation >= 1 && orientation <= 8 ? orientation : 1;
            }
        }
    }
    return 1;
}

void orientTopLeft(Decoded &image)
{
    if (image.orientation < 1 || image.orientation > 8) {
        throw std::runtime_error("image orientation is outside 1..8");
    }
    if (image.orientation == 1) return;
    const std::uint32_t inputWidth = image.width;
    const std::uint32_t inputHeight = image.height;
    const bool transposed = image.orientation >= 5;
    const std::uint32_t outputWidth = transposed ? inputHeight : inputWidth;
    const std::uint32_t outputHeight = transposed ? inputWidth : inputHeight;
    std::vector<std::uint16_t> oriented(
        static_cast<std::size_t>(outputWidth) * outputHeight * image.components);
    for (std::uint32_t y = 0; y < outputHeight; ++y) {
        for (std::uint32_t x = 0; x < outputWidth; ++x) {
            std::uint32_t sourceX = x;
            std::uint32_t sourceY = y;
            switch (image.orientation) {
                case 2: sourceX = inputWidth - 1 - x; sourceY = y; break;
                case 3: sourceX = inputWidth - 1 - x; sourceY = inputHeight - 1 - y; break;
                case 4: sourceX = x; sourceY = inputHeight - 1 - y; break;
                case 5: sourceX = y; sourceY = x; break;
                case 6: sourceX = y; sourceY = inputHeight - 1 - x; break;
                case 7: sourceX = inputWidth - 1 - y; sourceY = inputHeight - 1 - x; break;
                case 8: sourceX = inputWidth - 1 - y; sourceY = x; break;
                default: break;
            }
            const std::size_t source =
                (static_cast<std::size_t>(sourceY) * inputWidth + sourceX)
                * image.components;
            const std::size_t destination =
                (static_cast<std::size_t>(y) * outputWidth + x) * image.components;
            std::copy_n(image.rgb.data() + source, image.components,
                        oriented.data() + destination);
        }
    }
    image.width = outputWidth;
    image.height = outputHeight;
    image.rgb = std::move(oriented);
}

void jpegFailure(j_common_ptr common)
{
    auto *error = reinterpret_cast<JpegError *>(common->err);
    (*common->err->format_message)(common, error->message);
    std::longjmp(error->jump, 1);
}

Decoded decodeJpegBytes(
    const std::uint8_t *bytes,
    std::size_t byteCount,
    unsigned scaleDenominator,
    std::uint32_t &sourceWidth,
    std::uint32_t &sourceHeight)
{
    if (!bytes || byteCount == 0 || byteCount > std::numeric_limits<unsigned long>::max()) {
        throw std::runtime_error("JPEG input is empty or too large");
    }
    jpeg_decompress_struct decoder{};
    JpegError error{};
    decoder.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpegFailure;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&decoder);
        throw std::runtime_error(std::string("JPEG decode failed: ") + error.message);
    }
    jpeg_create_decompress(&decoder);
    jpeg_mem_src(&decoder, bytes, static_cast<unsigned long>(byteCount));
    jpeg_save_markers(&decoder, JPEG_APP0 + 1, 0xffff);
    jpeg_save_markers(&decoder, JPEG_APP0 + 2, 0xffff);
    jpeg_read_header(&decoder, TRUE);
    JOCTET *profile = nullptr;
    unsigned int profileBytes = 0;
    Decoded output;
    output.orientation = jpegExifOrientation(decoder);
    sourceWidth = decoder.image_width;
    sourceHeight = decoder.image_height;
    if (jpeg_read_icc_profile(&decoder, &profile, &profileBytes)) {
        output.icc.assign(profile, profile + profileBytes);
        std::free(profile);
    }
    const bool cmyk = decoder.jpeg_color_space == JCS_CMYK
        || decoder.jpeg_color_space == JCS_YCCK;
    const bool invertedCmyk = cmyk && decoder.saw_Adobe_marker;
    decoder.out_color_space = cmyk ? JCS_CMYK : JCS_RGB;
    decoder.scale_num = 1;
    decoder.scale_denom = scaleDenominator;
    jpeg_start_decompress(&decoder);
    output.width = decoder.output_width;
    output.height = decoder.output_height;
    output.components = static_cast<std::uint8_t>(decoder.output_components);
    output.type = "jpeg";
    if ((!cmyk && decoder.output_components != 3)
        || (cmyk && decoder.output_components != 4)
        || output.width == 0 || output.height == 0) {
        throw std::runtime_error("JPEG did not decode to the requested color components");
    }
    output.rgb.resize(
        static_cast<std::size_t>(output.width) * output.height * output.components);
    std::vector<JSAMPLE> row(static_cast<std::size_t>(output.width) * output.components);
    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW rowPointer = row.data();
        jpeg_read_scanlines(&decoder, &rowPointer, 1);
        const std::size_t y = decoder.output_scanline - 1;
        for (std::size_t index = 0; index < row.size(); ++index) {
            const unsigned value = invertedCmyk ? 255U - row[index] : row[index];
            output.rgb[y * row.size() + index] = static_cast<std::uint16_t>(value * 257U);
        }
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    orientTopLeft(output);
    if (output.orientation >= 5) std::swap(sourceWidth, sourceHeight);
    return output;
}

std::vector<std::uint8_t> readFileBytes(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open image: " + path);
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length <= 0 || static_cast<std::uint64_t>(length) > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("image file is empty or too large: " + path);
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    stream.read(reinterpret_cast<char *>(bytes.data()), length);
    if (!stream) throw std::runtime_error("cannot read image: " + path);
    return bytes;
}

Decoded decodePng(const std::string &path)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) throw std::runtime_error("cannot open PNG: " + path);
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info) {
        if (png) png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(file);
        throw std::runtime_error("cannot create PNG decoder");
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(file);
        throw std::runtime_error("PNG decode failed");
    }
    png_init_io(png, file);
    png_read_info(png, info);
    Decoded output;
    output.width = png_get_image_width(png, info);
    output.height = png_get_image_height(png, info);
    output.type = "png";
    int color = png_get_color_type(png, info);
    int depth = png_get_bit_depth(png, info);
    png_charp profileName = nullptr;
    int compression = 0;
    png_bytep profile = nullptr;
    png_uint_32 profileBytes = 0;
    if (png_get_iCCP(png, info, &profileName, &compression, &profile, &profileBytes)) {
        output.icc.assign(profile, profile + profileBytes);
    }
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (color & PNG_COLOR_MASK_ALPHA || png_get_valid(png, info, PNG_INFO_tRNS)) png_set_strip_alpha(png);
    if (depth < 16) png_set_expand_16(png);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    png_set_swap(png);
#endif
    // png_read_image() expects the application to request all Adam7 passes
    // before png_read_update_info().  Without this call libpng warns and an
    // interlaced source may expose only one pass to the canonical decoder.
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    if (png_get_bit_depth(png, info) != 16 || png_get_channels(png, info) != 3) {
        throw std::runtime_error("PNG normalization did not produce RGB16");
    }
    const std::size_t rowBytes = png_get_rowbytes(png, info);
    std::vector<std::uint8_t> pixels(rowBytes * output.height);
    std::vector<png_bytep> rows(output.height);
    for (std::size_t y = 0; y < output.height; ++y) rows[y] = pixels.data() + y * rowBytes;
    png_read_image(png, rows.data());
    png_read_end(png, info);
    output.rgb.resize(static_cast<std::size_t>(output.width) * output.height * 3);
    for (std::size_t y = 0; y < output.height; ++y) {
        std::memcpy(output.rgb.data() + y * output.width * 3, rows[y], output.width * 6);
    }
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);
    return output;
}

Decoded decodeTiff(const std::string &path)
{
    TIFF *tiff = TIFFOpen(path.c_str(), "r");
    if (!tiff) throw std::runtime_error("cannot open TIFF: " + path);
    Decoded output;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint16_t orientation = ORIENTATION_TOPLEFT;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_ORIENTATION, &orientation);
    output.width = width;
    output.height = height;
    output.orientation = orientation;
    output.type = "tiff";
    std::uint32_t profileBytes = 0;
    void *profile = nullptr;
    if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &profileBytes, &profile)) {
        const auto *data = static_cast<const std::uint8_t *>(profile);
        output.icc.assign(data, data + profileBytes);
    }
    std::uint16_t bits = 0;
    std::uint16_t samples = 0;
    std::uint16_t planar = 0;
    std::uint16_t photometric = 0;
    TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &samples);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_PHOTOMETRIC, &photometric);
    const bool rgb = photometric == PHOTOMETRIC_RGB && samples >= 3;
    const bool gray = (photometric == PHOTOMETRIC_MINISBLACK
                       || photometric == PHOTOMETRIC_MINISWHITE) && samples >= 1;
    if ((bits != 8 && bits != 16) || planar != PLANARCONFIG_CONTIG
        || (!rgb && !gray) || width == 0 || height == 0) {
        TIFFClose(tiff);
        throw std::runtime_error(
            "TIFF must be contiguous 8/16-bit RGB or grayscale for TGMR corpus use");
    }
    const tmsize_t scanlineBytes = TIFFScanlineSize(tiff);
    const std::size_t minimumBytes = static_cast<std::size_t>(width) * samples * (bits / 8);
    if (scanlineBytes < 0 || static_cast<std::size_t>(scanlineBytes) < minimumBytes) {
        TIFFClose(tiff);
        throw std::runtime_error("TIFF scanline size is malformed");
    }
    std::vector<std::uint8_t> row(static_cast<std::size_t>(scanlineBytes));
    output.rgb.resize(static_cast<std::size_t>(width) * height * 3);
    for (std::uint32_t y = 0; y < height; ++y) {
        if (TIFFReadScanline(tiff, row.data(), y, 0) < 0) {
            TIFFClose(tiff);
            throw std::runtime_error("TIFF scanline decode failed");
        }
        for (std::uint32_t x = 0; x < width; ++x) {
            for (unsigned channel = 0; channel < 3; ++channel) {
                const unsigned inputChannel = rgb ? channel : 0;
                std::uint16_t value = 0;
                if (bits == 8) {
                    value = static_cast<std::uint16_t>(
                        row[static_cast<std::size_t>(x) * samples + inputChannel] * 257U);
                } else {
                    std::memcpy(&value, row.data()
                        + (static_cast<std::size_t>(x) * samples + inputChannel) * 2, 2);
                }
                if (gray && photometric == PHOTOMETRIC_MINISWHITE) value = 65535 - value;
                output.rgb[(static_cast<std::size_t>(y) * width + x) * 3 + channel] = value;
            }
        }
    }
    TIFFClose(tiff);
    orientTopLeft(output);
    return output;
}

std::string lowercaseExtension(const std::string &path)
{
    const std::size_t dot = path.find_last_of('.');
    std::string extension = dot == std::string::npos ? std::string() : path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

cmsHPROFILE linearSrgbProfile()
{
    cmsCIExyY white{};
    cmsWhitePointFromTemp(&white, 6504.0);
    cmsCIExyYTRIPLE primaries{
        {0.6400, 0.3300, 1.0},
        {0.3000, 0.6000, 1.0},
        {0.1500, 0.0600, 1.0},
    };
    cmsToneCurve *curve = cmsBuildGamma(nullptr, 1.0);
    cmsToneCurve *curves[3] = {curve, curve, curve};
    cmsHPROFILE result = cmsCreateRGBProfile(&white, &primaries, curves);
    cmsFreeToneCurve(curve);
    return result;
}

double clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

double luminance(const double *rgb)
{
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
}

std::vector<double> convertToLinearSrgb(const Decoded &decoded)
{
    if (decoded.components == 4 && decoded.icc.empty()) {
        throw std::runtime_error("CMYK image lacks an embedded ICC profile");
    }
    cmsHPROFILE input = decoded.icc.empty()
        ? cmsCreate_sRGBProfile()
        : cmsOpenProfileFromMem(decoded.icc.data(), decoded.icc.size());
    cmsHPROFILE outputProfile = linearSrgbProfile();
    if (!input || !outputProfile) {
        if (input) cmsCloseProfile(input);
        if (outputProfile) cmsCloseProfile(outputProfile);
        throw std::runtime_error("cannot create source/linear-sRGB color profile");
    }

    const cmsColorSpaceSignature colorSpace = cmsGetColorSpace(input);
    cmsUInt32Number inputFormat = 0;
    const void *inputPixels = decoded.rgb.data();
    std::vector<std::uint16_t> gray;
    if (colorSpace == cmsSigRgbData && decoded.components == 3) {
        inputFormat = TYPE_RGB_16;
    } else if (colorSpace == cmsSigGrayData && decoded.components == 3) {
        // libjpeg expands a grayscale JPEG to RGB for the common decode path,
        // but an embedded grayscale ICC profile still expects one component.
        // Feed LittleCMS the original repeated gray sample rather than
        // incorrectly pairing a GRAY profile with TYPE_RGB_16.
        gray.resize(static_cast<std::size_t>(decoded.width) * decoded.height);
        for (std::size_t index = 0; index < gray.size(); ++index) {
            gray[index] = decoded.rgb[index * 3];
        }
        inputFormat = TYPE_GRAY_16;
        inputPixels = gray.data();
    } else if (colorSpace == cmsSigCmykData && decoded.components == 4) {
        inputFormat = TYPE_CMYK_16;
    } else {
        cmsCloseProfile(input);
        cmsCloseProfile(outputProfile);
        throw std::runtime_error("unsupported input ICC color space");
    }

    cmsHTRANSFORM transform = cmsCreateTransform(
        input, inputFormat, outputProfile, TYPE_RGB_DBL,
        INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_BLACKPOINTCOMPENSATION);
    if (!transform) {
        cmsCloseProfile(input);
        cmsCloseProfile(outputProfile);
        throw std::runtime_error("cannot create linear-sRGB color transform");
    }
    std::vector<double> output(
        static_cast<std::size_t>(decoded.width) * decoded.height * 3);
    cmsDoTransform(transform, inputPixels, output.data(),
                   static_cast<cmsUInt32Number>(decoded.width * decoded.height));
    cmsDeleteTransform(transform);
    cmsCloseProfile(input);
    cmsCloseProfile(outputProfile);
    if (!std::all_of(output.begin(), output.end(),
                     [](double value) { return std::isfinite(value); })) {
        throw std::runtime_error("color management produced a non-finite pixel");
    }
    return output;
}

} // namespace

LinearImage loadLinearImage(const std::string &path)
{
    const std::string extension = lowercaseExtension(path);
    Decoded decoded;
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    if (extension == ".jpg" || extension == ".jpeg") {
        const auto bytes = readFileBytes(path);
        decoded = decodeJpegBytes(bytes.data(), bytes.size(), 1, sourceWidth, sourceHeight);
    }
    else if (extension == ".png") decoded = decodePng(path);
    else if (extension == ".tif" || extension == ".tiff") decoded = decodeTiff(path);
    else throw std::runtime_error("unsupported image type: " + extension);
    if (sourceWidth == 0) sourceWidth = decoded.width;
    if (sourceHeight == 0) sourceHeight = decoded.height;
    if (decoded.width < 7 || decoded.height < 7
        || decoded.rgb.size() != static_cast<std::size_t>(decoded.width)
            * decoded.height * decoded.components) {
        throw std::runtime_error("decoded image is too small or malformed");
    }
    LinearImage result;
    result.width = decoded.width;
    result.height = decoded.height;
    result.sourceWidth = sourceWidth;
    result.sourceHeight = sourceHeight;
    result.orientation = decoded.orientation;
    result.fileType = decoded.type;
    result.assumedSrgb = decoded.icc.empty();
    result.iccIdentity = decoded.icc.empty()
        ? "assumed-srgb"
        : "sha256:" + hex(sha256(decoded.icc.data(), decoded.icc.size()));
    result.rgb = convertToLinearSrgb(decoded);
    return result;
}

LoadedLinearImage loadLinearImageAndSha256(const std::string &path, bool jpegProxy)
{
    LoadedLinearImage result;
    const std::string extension = lowercaseExtension(path);
    if (extension != ".jpg" && extension != ".jpeg") {
        if (jpegProxy) {
            throw std::runtime_error("proxy classification accepts JPEG input only");
        }
        result.fileSha256 = hex(sha256File(path, &result.fileBytes));
        result.image = loadLinearImage(path);
        return result;
    }

    const auto bytes = readFileBytes(path);
    result.fileBytes = bytes.size();
    result.fileSha256 = hex(sha256(bytes.data(), bytes.size()));
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    Decoded decoded = decodeJpegBytes(
        bytes.data(), bytes.size(), jpegProxy ? 8 : 1, sourceWidth, sourceHeight);
    if (decoded.width < 7 || decoded.height < 7) {
        throw std::runtime_error("decoded JPEG proxy is too small");
    }

    result.image.width = decoded.width;
    result.image.height = decoded.height;
    result.image.sourceWidth = sourceWidth;
    result.image.sourceHeight = sourceHeight;
    result.image.orientation = decoded.orientation;
    result.image.fileType = decoded.type;
    result.image.assumedSrgb = decoded.icc.empty();
    result.image.iccIdentity = decoded.icc.empty()
        ? "assumed-srgb"
        : "sha256:" + hex(sha256(decoded.icc.data(), decoded.icc.size()));
    result.image.rgb = convertToLinearSrgb(decoded);
    result.proxy = jpegProxy;
    return result;
}

ImageClassification classifyImage(const LinearImage &image)
{
    if (image.width == 0 || image.height == 0
        || image.rgb.size() != static_cast<std::size_t>(image.width) * image.height * 3) {
        throw std::runtime_error("cannot classify malformed image");
    }
    ImageClassification result;
    const std::size_t pixels = static_cast<std::size_t>(image.width) * image.height;
    const std::size_t stride = std::max<std::size_t>(1, pixels / 1'000'000);
    std::vector<double> sampledLuminance;
    sampledLuminance.reserve((pixels + stride - 1) / stride);
    double luminanceSum = 0.0;
    double luminanceSquared = 0.0;
    double chroma = 0.0;
    double saturation = 0.0;
    double hueX = 0.0;
    double hueY = 0.0;
    std::uint64_t black = 0;
    std::uint64_t white = 0;
    std::size_t selected = 0;
    Sha256 canonical;
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const double *rgb = image.rgb.data() + pixel * 3;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const std::uint16_t value = static_cast<std::uint16_t>(
                std::llround(clamp01(rgb[channel]) * 65535.0));
            const std::uint8_t littleEndian[2] = {
                static_cast<std::uint8_t>(value),
                static_cast<std::uint8_t>(value >> 8),
            };
            canonical.update(littleEndian, sizeof(littleEndian));
        }
        if (pixel % stride != 0) continue;
        ++selected;
        const double r = clamp01(rgb[0]);
        const double g = clamp01(rgb[1]);
        const double b = clamp01(rgb[2]);
        result.channelMeans[0] += r;
        result.channelMeans[1] += g;
        result.channelMeans[2] += b;
        const double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        ++result.luminanceHistogram[std::min<std::size_t>(15,
            static_cast<std::size_t>(y * 16.0))];
        sampledLuminance.push_back(y);
        luminanceSum += y;
        luminanceSquared += y * y;
        const double maximum = std::max({r, g, b});
        const double minimum = std::min({r, g, b});
        const double chromaValue = maximum - minimum;
        chroma += std::sqrt((r - g) * (r - g) + (g - b) * (g - b)
                            + (b - r) * (b - r)) / (y + 1e-4);
        const double sat = maximum > 0.0 ? chromaValue / maximum : 0.0;
        saturation += sat;
        ++result.saturationHistogram[std::min<std::size_t>(7,
            static_cast<std::size_t>(sat * 8.0))];
        if (chromaValue > 1e-12) {
            double hue = 0.0;
            if (maximum == r) hue = (g - b) / chromaValue;
            else if (maximum == g) hue = 2.0 + (b - r) / chromaValue;
            else hue = 4.0 + (r - g) / chromaValue;
            hue *= PI / 3.0;
            hueX += std::cos(hue) * sat;
            hueY += std::sin(hue) * sat;
            double normalizedHue = hue;
            while (normalizedHue < 0.0) normalizedHue += 2.0 * PI;
            while (normalizedHue >= 2.0 * PI) normalizedHue -= 2.0 * PI;
            ++result.hueHistogram[std::min<std::size_t>(11,
                static_cast<std::size_t>(normalizedHue * 6.0 / PI))];
        }
        black += maximum <= 1.0 / 65535.0;
        white += maximum >= 1.0 - 1.0 / 65535.0;
    }
    for (double &mean : result.channelMeans) mean /= selected;
    result.luminanceMean = luminanceSum / selected;
    result.luminanceStddev = std::sqrt(std::max(
        0.0, luminanceSquared / selected - result.luminanceMean * result.luminanceMean));
    result.chromaRatioMean = chroma / selected;
    result.saturationMean = saturation / selected;
    result.hueDegrees = std::atan2(hueY, hueX) * 180.0 / PI;
    if (result.hueDegrees < 0.0) result.hueDegrees += 360.0;
    result.clippedBlackFraction = static_cast<double>(black) / selected;
    result.clippedWhiteFraction = static_cast<double>(white) / selected;
    std::sort(sampledLuminance.begin(), sampledLuminance.end());
    result.luminanceP01 = sampledLuminance[static_cast<std::size_t>(
        0.01 * (sampledLuminance.size() - 1))];
    result.luminanceP99 = sampledLuminance[static_cast<std::size_t>(
        0.99 * (sampledLuminance.size() - 1))];
    result.decodedPixelSha256 = hex(canonical.finish());

    double gradient = 0.0;
    double laplacian = 0.0;
    double contrast = 0.0;
    double block = 0.0;
    double ordinary = 0.0;
    std::uint64_t gradientCount = 0;
    std::uint64_t laplacianCount = 0;
    std::uint64_t blockCount = 0;
    std::uint64_t ordinaryCount = 0;
    auto yAt = [&](std::size_t x, std::size_t y) {
        return luminance(image.rgb.data() + (y * image.width + x) * 3);
    };
    for (std::size_t y = 1; y + 1 < image.height; ++y) {
        for (std::size_t x = 1; x + 1 < image.width; ++x) {
            const double center = yAt(x, y);
            const double dx = yAt(x + 1, y) - center;
            const double dy = yAt(x, y + 1) - center;
            gradient += dx * dx + dy * dy;
            ++gradientCount;
            const double lap = 4.0 * center - yAt(x - 1, y) - yAt(x + 1, y)
                - yAt(x, y - 1) - yAt(x, y + 1);
            laplacian += lap * lap;
            ++laplacianCount;
            double localMean = 0.0;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox) localMean += yAt(x + ox, y + oy);
            contrast += std::abs(center - localMean / 9.0);
            const double horizontal = std::abs(center - yAt(x - 1, y));
            const double vertical = std::abs(center - yAt(x, y - 1));
            if (x % 8 == 0 || y % 8 == 0) {
                block += horizontal + vertical;
                ++blockCount;
            } else {
                ordinary += horizontal + vertical;
                ++ordinaryCount;
            }
        }
    }
    result.gradientRms = std::sqrt(gradient / std::max<std::uint64_t>(1, gradientCount));
    result.laplacianRms = std::sqrt(laplacian / std::max<std::uint64_t>(1, laplacianCount));
    result.localContrast = contrast / std::max<std::uint64_t>(1, laplacianCount);
    const double blockMean = block / std::max<std::uint64_t>(1, blockCount);
    const double ordinaryMean = ordinary / std::max<std::uint64_t>(1, ordinaryCount);
    result.jpegBlockiness = blockMean / (ordinaryMean + 1e-12);

    std::uint64_t hash = 0;
    unsigned bit = 0;
    for (unsigned y = 0; y < 8; ++y) {
        const std::size_t yy = std::min<std::size_t>(image.height - 1,
            (2 * y + 1) * image.height / 16);
        for (unsigned x = 0; x < 8; ++x) {
            const std::size_t left = std::min<std::size_t>(image.width - 1,
                (2 * x + 1) * image.width / 18);
            const std::size_t right = std::min<std::size_t>(image.width - 1,
                (2 * (x + 1) + 1) * image.width / 18);
            if (yAt(right, yy) > yAt(left, yy)) hash |= std::uint64_t(1) << bit;
            ++bit;
        }
    }
    std::ostringstream hashText;
    hashText << std::hex << std::setfill('0') << std::setw(16) << hash;
    result.perceptualHash = hashText.str();

    // Deterministic 64-bit DCT pHash.  Sampling at cell centers avoids image
    // resampling dependencies and gives the corpus selector a signature whose
    // failure modes differ from the historical horizontal dHash.
    std::array<double, 32 * 32> sample{};
    for (unsigned y = 0; y < 32; ++y) {
        const std::size_t yy = std::min<std::size_t>(image.height - 1,
            (2 * y + 1) * image.height / 64);
        for (unsigned x = 0; x < 32; ++x) {
            const std::size_t xx = std::min<std::size_t>(image.width - 1,
                (2 * x + 1) * image.width / 64);
            sample[y * 32 + x] = yAt(xx, yy);
        }
    }
    std::array<double, 64> dct{};
    for (unsigned v = 0; v < 8; ++v) {
        for (unsigned u = 0; u < 8; ++u) {
            double coefficient = 0.0;
            for (unsigned y = 0; y < 32; ++y) {
                const double cy = std::cos(PI * (2.0 * y + 1.0) * v / 64.0);
                for (unsigned x = 0; x < 32; ++x) {
                    coefficient += sample[y * 32 + x] * cy
                        * std::cos(PI * (2.0 * x + 1.0) * u / 64.0);
                }
            }
            dct[v * 8 + u] = coefficient;
        }
    }
    std::array<double, 63> nonDc{};
    std::copy(dct.begin() + 1, dct.end(), nonDc.begin());
    std::nth_element(nonDc.begin(), nonDc.begin() + nonDc.size() / 2, nonDc.end());
    const double median = nonDc[nonDc.size() / 2];
    std::uint64_t pHash = 0;
    for (unsigned index = 0; index < dct.size(); ++index) {
        if (dct[index] > median) pHash |= std::uint64_t(1) << index;
    }
    std::ostringstream pHashText;
    pHashText << std::hex << std::setfill('0') << std::setw(16) << pHash;
    result.pHash = pHashText.str();
    return result;
}

std::string canonicalClassificationJson(
    const LinearImage &image,
    const ImageClassification &classification,
    const std::string &sourceId,
    const std::string &cacheFilename)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(10) << '{';
    if (!cacheFilename.empty()) {
        output << "\"cache_filename\":\"" << cacheFilename << "\",";
    }
    output << "\"classification\":{"
        << "\"channel_means\":[" << classification.channelMeans[0] << ','
        << classification.channelMeans[1] << ',' << classification.channelMeans[2] << "],"
        << "\"chroma_ratio_mean\":" << classification.chromaRatioMean << ','
        << "\"clipped_black_fraction\":" << classification.clippedBlackFraction << ','
        << "\"clipped_white_fraction\":" << classification.clippedWhiteFraction << ','
        << "\"dhash\":\"" << classification.perceptualHash << "\","
        << "\"gradient_rms\":" << classification.gradientRms << ','
        << "\"hue_degrees\":" << classification.hueDegrees << ','
        << "\"hue_histogram\":[";
    for (std::size_t index = 0; index < classification.hueHistogram.size(); ++index) {
        if (index) output << ',';
        output << classification.hueHistogram[index];
    }
    output << "],"
        << "\"jpeg_blockiness\":" << classification.jpegBlockiness << ','
        << "\"laplacian_rms\":" << classification.laplacianRms << ','
        << "\"local_contrast\":" << classification.localContrast << ','
        << "\"luminance_histogram\":[";
    for (std::size_t index = 0; index < classification.luminanceHistogram.size(); ++index) {
        if (index) output << ',';
        output << classification.luminanceHistogram[index];
    }
    output << "],"
        << "\"luminance_mean\":" << classification.luminanceMean << ','
        << "\"luminance_p01\":" << classification.luminanceP01 << ','
        << "\"luminance_p99\":" << classification.luminanceP99 << ','
        << "\"luminance_stddev\":" << classification.luminanceStddev << ','
        << "\"perceptual_hash\":\"" << classification.perceptualHash << "\","
        << "\"phash\":\"" << classification.pHash << "\","
        << "\"saturation_histogram\":[";
    for (std::size_t index = 0; index < classification.saturationHistogram.size(); ++index) {
        if (index) output << ',';
        output << classification.saturationHistogram[index];
    }
    output << "],"
        << "\"saturation_mean\":" << classification.saturationMean << "},"
        << "\"decoded_pixel_sha256\":\"" << classification.decodedPixelSha256 << "\","
        << "\"file_type\":\"" << image.fileType << "\","
        << "\"height\":" << image.height << ','
        << "\"icc_identity\":\"" << image.iccIdentity << "\","
        << "\"orientation\":" << image.orientation << ','
        << "\"source_id\":\"" << sourceId << "\","
        << "\"width\":" << image.width << "}\n";
    return output.str();
}

std::string canonicalProxyClassificationJson(
    const LinearImage &image,
    const ImageClassification &classification,
    const std::string &sourceId,
    const std::string &cacheFilename)
{
    std::string full = canonicalClassificationJson(
        image, classification, sourceId, cacheFilename);
    // Reuse the exact metric encoding, while making proxy dimensions and the
    // noncanonical decoded-pixel identity impossible to mistake for final
    // corpus metadata.
    const std::string decoded = "\"decoded_pixel_sha256\":";
    const std::size_t decodedPosition = full.find(decoded);
    if (decodedPosition == std::string::npos) {
        throw std::runtime_error("internal proxy JSON construction failure");
    }
    full.replace(decodedPosition, decoded.size(), "\"proxy_decoded_pixel_sha256\":");
    const std::string dimensions = "\"height\":" + std::to_string(image.height) + ',';
    const std::size_t dimensionsPosition = full.find(dimensions);
    if (dimensionsPosition == std::string::npos) {
        throw std::runtime_error("internal proxy JSON dimensions failure");
    }
    std::ostringstream replacement;
    replacement << "\"height\":" << image.sourceHeight << ','
        << "\"proxy_height\":" << image.height << ','
        << "\"proxy_scale_denominator\":8,";
    full.replace(dimensionsPosition, dimensions.size(), replacement.str());
    const std::string width = "\"width\":" + std::to_string(image.width) + '}';
    const std::size_t widthPosition = full.rfind(width);
    if (widthPosition == std::string::npos) {
        throw std::runtime_error("internal proxy JSON width failure");
    }
    std::ostringstream widthReplacement;
    widthReplacement << "\"proxy_width\":" << image.width << ','
        << "\"width\":" << image.sourceWidth << '}';
    full.replace(widthPosition, width.size(), widthReplacement.str());
    full.insert(1, "\"format\":\"rawtherapee-tgmr-image-proxy-classification-v1\",");
    return full;
}

} // namespace tgmr
