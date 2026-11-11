#include "tgmr/corpus.h"
#include "tgmr/camera_matrices.h"
#include "tgmr/corpus_analysis.h"
#include "tgmr/image.h"
#include "tgmr/manifest.h"
#include "tgmr/model_v2.h"
#include "tgmr/patch_selection.h"
#include "tgmr/sha256.h"
#include "tgmr/training.h"
#include "tgmr/training_identity.h"
#include "tgmr/validation.h"
#include "tgmr/xtrans_training.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>
#include <jpeglib.h>
#include <lcms2.h>
#include <png.h>
#include <tiffio.h>

namespace
{

void require(bool condition, const std::string &message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string temporary(const char *suffix)
{
    char value[] = "/tmp/rt-tgmr-tooling-XXXXXX";
    const int descriptor = mkstemp(value);
    require(descriptor >= 0, "cannot create temporary path");
    close(descriptor);
    std::remove(value);
    return std::string(value) + suffix;
}

std::vector<unsigned char> read(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    require(static_cast<bool>(stream), "cannot read test artifact");
    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void appendU32(std::vector<std::uint8_t> &output, std::uint32_t value)
{
    for (unsigned byte = 0; byte < 4; ++byte) {
        output.push_back(static_cast<std::uint8_t>(value >> (8 * byte)));
    }
}

void appendFloat(std::vector<std::uint8_t> &output, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(output, bits);
}

std::vector<std::uint8_t> constantGrayValidationPayload()
{
    std::vector<std::uint8_t> output;
    const auto phases = tgmr::xtransPhaseContracts();
    const std::array<std::uint32_t, 9> coarse{{16,17,18,23,24,25,30,31,32}};
    for (const auto &phase : phases) {
        for (std::uint32_t observed : phase.observedIndices) appendU32(output, observed);
        appendU32(output, phase.sampledCenterChannel);
        appendU32(output, phase.targetChannels[0]);
        appendU32(output, phase.targetChannels[1]);
        for (unsigned component = 0; component < 32; ++component) {
            appendFloat(output, -std::log(32.f));
        }
        for (unsigned i = 0; i < 32 * 49 + 32 * 2; ++i) appendFloat(output, 0.f);
        for (unsigned component = 0; component < 32; ++component) {
            for (unsigned row = 0; row < 49; ++row) {
                for (unsigned column = 0; column < 49; ++column) {
                    appendFloat(output, row == column ? 1.f : 0.f);
                }
            }
        }
        for (unsigned component = 0; component < 32; ++component) appendFloat(output, 0.f);
        for (unsigned i = 0; i < 32 * 2 * 49; ++i) appendFloat(output, 0.f);
        for (std::uint32_t position : coarse) appendU32(output, position);
        for (unsigned component = 0; component < 32; ++component) {
            for (unsigned row = 0; row < 9; ++row) {
                for (unsigned column = 0; column < 9; ++column) {
                    appendFloat(output, row == column ? 1.f : 0.f);
                }
            }
        }
        for (unsigned component = 0; component < 32; ++component) appendFloat(output, 0.f);
    }
    return output;
}

void writePngFixture(const std::string &path, unsigned width, unsigned height)
{
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = width;
    image.height = height;
    image.format = PNG_FORMAT_RGB;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 3;
            pixels[index] = static_cast<unsigned char>(17 * x);
            pixels[index + 1] = static_cast<unsigned char>(13 * y);
            pixels[index + 2] = static_cast<unsigned char>(7 * (x + y));
        }
    }
    require(png_image_write_to_file(&image, path.c_str(), 0, pixels.data(), 0, nullptr) != 0,
            "cannot write PNG fixture");
}

void writeInterlacedPngFixture(const std::string &path, unsigned width, unsigned height)
{
    std::FILE *stream = std::fopen(path.c_str(), "wb");
    require(stream != nullptr, "cannot create interlaced PNG fixture");
    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    require(png && info, "cannot create interlaced PNG writer");
    png_init_io(png, stream);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_ADAM7, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    std::vector<png_bytep> rows(height);
    for (unsigned y = 0; y < height; ++y) {
        rows[y] = pixels.data() + static_cast<std::size_t>(y) * width * 3;
        for (unsigned x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 3;
            pixels[index] = static_cast<unsigned char>(17 * x);
            pixels[index + 1] = static_cast<unsigned char>(13 * y);
            pixels[index + 2] = static_cast<unsigned char>(7 * (x + y));
        }
    }
    png_write_info(png, info);
    png_write_image(png, rows.data());
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    require(std::fclose(stream) == 0, "cannot close interlaced PNG fixture");
}

void writeJpegFixture(const std::string &path, unsigned width, unsigned height, unsigned seed)
{
    std::FILE *stream = std::fopen(path.c_str(), "wb");
    require(stream != nullptr, "cannot create JPEG fixture");
    jpeg_compress_struct encoder{};
    jpeg_error_mgr error{};
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    jpeg_stdio_dest(&encoder, stream);
    encoder.image_width = width;
    encoder.image_height = height;
    encoder.input_components = 3;
    encoder.in_color_space = JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, 91, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    std::vector<unsigned char> row(static_cast<std::size_t>(width) * 3);
    while (encoder.next_scanline < encoder.image_height) {
        const unsigned y = encoder.next_scanline;
        for (unsigned x = 0; x < width; ++x) {
            row[x * 3] = static_cast<unsigned char>((3 * x + seed * 17) & 255);
            row[x * 3 + 1] = static_cast<unsigned char>((5 * y + seed * 11) & 255);
            row[x * 3 + 2] = static_cast<unsigned char>((x + 2 * y + seed * 7) & 255);
        }
        JSAMPROW rows[] = {row.data()};
        jpeg_write_scanlines(&encoder, rows, 1);
    }
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);
    require(std::fclose(stream) == 0, "cannot close JPEG fixture");
}

void writeGrayJpegWithIccFixture(const std::string &path, unsigned width, unsigned height)
{
    cmsCIExyY white{};
    cmsWhitePointFromTemp(&white, 6504.0);
    cmsToneCurve *curve = cmsBuildGamma(nullptr, 2.2);
    require(curve != nullptr, "cannot create grayscale ICC tone curve");
    cmsHPROFILE profile = cmsCreateGrayProfile(&white, curve);
    cmsFreeToneCurve(curve);
    require(profile != nullptr, "cannot create grayscale ICC profile");
    cmsUInt32Number profileBytes = 0;
    require(cmsSaveProfileToMem(profile, nullptr, &profileBytes) != 0 && profileBytes > 0,
            "cannot size grayscale ICC profile");
    std::vector<unsigned char> profileData(profileBytes);
    require(cmsSaveProfileToMem(profile, profileData.data(), &profileBytes) != 0,
            "cannot serialize grayscale ICC profile");
    cmsCloseProfile(profile);

    std::FILE *stream = std::fopen(path.c_str(), "wb");
    require(stream != nullptr, "cannot create grayscale JPEG fixture");
    jpeg_compress_struct encoder{};
    jpeg_error_mgr error{};
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    jpeg_stdio_dest(&encoder, stream);
    encoder.image_width = width;
    encoder.image_height = height;
    encoder.input_components = 1;
    encoder.in_color_space = JCS_GRAYSCALE;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, 91, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    jpeg_write_icc_profile(&encoder, profileData.data(), profileBytes);
    std::vector<unsigned char> row(width);
    while (encoder.next_scanline < encoder.image_height) {
        const unsigned y = encoder.next_scanline;
        for (unsigned x = 0; x < width; ++x) {
            row[x] = static_cast<unsigned char>((3 * x + 5 * y + 17) & 255);
        }
        JSAMPROW rows[] = {row.data()};
        jpeg_write_scanlines(&encoder, rows, 1);
    }
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);
    require(std::fclose(stream) == 0, "cannot close grayscale JPEG fixture");
}

cmsInt32Number sampleTestCmykProfile(
    const cmsUInt16Number input[], cmsUInt16Number output[], void *)
{
    const double c = input[0] / 65535.0;
    const double m = input[1] / 65535.0;
    const double y = input[2] / 65535.0;
    const double k = input[3] / 65535.0;
    const double r = (1.0 - c) * (1.0 - k);
    const double g = (1.0 - m) * (1.0 - k);
    const double b = (1.0 - y) * (1.0 - k);
    const cmsCIELab lab{
        100.0 * (0.2126 * r + 0.7152 * g + 0.0722 * b),
        80.0 * (r - g),
        80.0 * (g - b),
    };
    cmsFloat2LabEncoded(output, &lab);
    return 1;
}

std::vector<unsigned char> testCmykIccProfile()
{
    cmsHPROFILE profile = cmsCreateProfilePlaceholder(nullptr);
    require(profile != nullptr, "cannot create CMYK ICC profile placeholder");
    cmsSetProfileVersion(profile, 2.1);
    cmsSetDeviceClass(profile, cmsSigInputClass);
    cmsSetColorSpace(profile, cmsSigCmykData);
    cmsSetPCS(profile, cmsSigLabData);
    cmsSetHeaderRenderingIntent(profile, INTENT_RELATIVE_COLORIMETRIC);
    require(cmsWriteTag(profile, cmsSigMediaWhitePointTag, cmsD50_XYZ()) != 0,
            "cannot write CMYK ICC white point");
    cmsPipeline *pipeline = cmsPipelineAlloc(nullptr, 4, 3);
    cmsStage *clut = cmsStageAllocCLut16bit(nullptr, 5, 4, 3, nullptr);
    require(pipeline != nullptr && clut != nullptr,
            "cannot create CMYK ICC lookup table");
    require(cmsStageSampleCLut16bit(clut, sampleTestCmykProfile, nullptr, 0) != 0,
            "cannot sample CMYK ICC lookup table");
    cmsPipelineInsertStage(pipeline, cmsAT_END, clut);
    require(cmsWriteTag(profile, cmsSigAToB0Tag, pipeline) != 0
        && cmsWriteTag(profile, cmsSigAToB1Tag, pipeline) != 0,
        "cannot write CMYK ICC transform tags");
    cmsPipelineFree(pipeline);
    cmsUInt32Number bytes = 0;
    require(cmsSaveProfileToMem(profile, nullptr, &bytes) != 0 && bytes > 0,
            "cannot size CMYK ICC profile");
    std::vector<unsigned char> output(bytes);
    require(cmsSaveProfileToMem(profile, output.data(), &bytes) != 0,
            "cannot serialize CMYK ICC profile");
    cmsCloseProfile(profile);
    return output;
}

void writeCmykJpegFixture(
    const std::string &path, unsigned width, unsigned height, bool includeProfile)
{
    const std::vector<unsigned char> profile = includeProfile
        ? testCmykIccProfile() : std::vector<unsigned char>{};
    std::FILE *stream = std::fopen(path.c_str(), "wb");
    require(stream != nullptr, "cannot create CMYK JPEG fixture");
    jpeg_compress_struct encoder{};
    jpeg_error_mgr error{};
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    jpeg_stdio_dest(&encoder, stream);
    encoder.image_width = width;
    encoder.image_height = height;
    encoder.input_components = 4;
    encoder.in_color_space = JCS_CMYK;
    jpeg_set_defaults(&encoder);
    encoder.write_Adobe_marker = FALSE;
    jpeg_set_quality(&encoder, 91, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    if (!profile.empty()) {
        jpeg_write_icc_profile(&encoder, profile.data(), profile.size());
    }
    std::vector<unsigned char> row(static_cast<std::size_t>(width) * 4);
    while (encoder.next_scanline < encoder.image_height) {
        const unsigned rowIndex = encoder.next_scanline;
        for (unsigned x = 0; x < width; ++x) {
            row[x * 4] = static_cast<unsigned char>((3 * x + 17) & 127);
            row[x * 4 + 1] = static_cast<unsigned char>((5 * rowIndex + 11) & 127);
            row[x * 4 + 2] = static_cast<unsigned char>((x + rowIndex + 7) & 127);
            row[x * 4 + 3] = static_cast<unsigned char>((x + 2 * rowIndex) & 63);
        }
        JSAMPROW rows[] = {row.data()};
        jpeg_write_scanlines(&encoder, rows, 1);
    }
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);
    require(std::fclose(stream) == 0, "cannot close CMYK JPEG fixture");
}

void writeTiffFixture(const std::string &path, unsigned width, unsigned height)
{
    TIFF *tiff = TIFFOpen(path.c_str(), "w");
    require(tiff != nullptr, "cannot create TIFF fixture");
    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_RIGHTTOP);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, height);
    std::vector<std::uint16_t> row(static_cast<std::size_t>(width) * 3);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            row[x * 3] = static_cast<std::uint16_t>(1000 + 100 * x);
            row[x * 3 + 1] = static_cast<std::uint16_t>(2000 + 100 * y);
            row[x * 3 + 2] = static_cast<std::uint16_t>(3000 + 10 * (x + y));
        }
        require(TIFFWriteScanline(tiff, row.data(), y, 0) >= 0,
                "cannot write TIFF fixture scanline");
    }
    TIFFClose(tiff);
}

void testSha256()
{
    require(tgmr::hex(tgmr::sha256("", 0))
        == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "empty SHA-256 vector failed");
    require(tgmr::hex(tgmr::sha256("abc", 3))
        == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "abc SHA-256 vector failed");
    const auto parsed = tgmr::parseSha256(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    require(tgmr::hex(parsed)
        == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 hexadecimal round trip failed");
}

void testCameraMatrices()
{
    const auto &identity = tgmr::cameraMatrix(0);
    require(identity.linearSrgbToCamera == std::array<double, 9>{{1,0,0,0,1,0,0,0,1}},
            "identity camera matrix changed");
    for (std::uint16_t id : {1, 2, 3, 4, 101, 102}) {
        const auto &matrix = tgmr::cameraMatrix(id);
        for (unsigned row = 0; row < 3; ++row) {
            const double sum = matrix.linearSrgbToCamera[row * 3]
                + matrix.linearSrgbToCamera[row * 3 + 1]
                + matrix.linearSrgbToCamera[row * 3 + 2];
            require(std::abs(sum - 1.0) < 1e-12,
                    "camera matrix does not preserve neutral linear RGB");
        }
    }
    require(tgmr::cameraMatrix(4).heldOut == false
        && tgmr::cameraMatrix(101).heldOut,
        "camera matrix train/evaluation partition changed");
    require(tgmr::canonicalCameraMatrixJson().find("FUJIFILM X-H2S") != std::string::npos,
            "camera matrix manifest changed");
}

void testCorpus()
{
    std::vector<tgmr::PatchRecord> records(3);
    for (std::size_t index = 0; index < records.size(); ++index) {
        auto &record = records[index];
        record.sourceIdSha256 = tgmr::sha256(&index, sizeof(index));
        record.sourceOrdinal = static_cast<std::uint32_t>(index + 10);
        record.x = static_cast<std::uint32_t>(17 + index);
        record.y = static_cast<std::uint32_t>(31 + index);
        record.split = static_cast<tgmr::CorpusSplit>(index + 1);
        record.augmentationKind = static_cast<std::uint8_t>(index);
        record.exposureStopsQ8 = static_cast<std::int16_t>(index * 128 - 128);
        record.patchSeed = 0x525454474d520000ULL + index;
        for (std::size_t value = 0; value < record.rgb.size(); ++value) {
            record.rgb[value] = static_cast<std::uint16_t>(value * 17 + index);
        }
    }
    const auto manifest = tgmr::sha256("manifest", 8);
    const auto config = tgmr::sha256("configuration", 13);
    const std::string corpus = temporary(".tgpc");
    const std::string gzipA = temporary("-a.tgpc.gz");
    const std::string gzipB = temporary("-b.tgpc.gz");
    tgmr::writeCorpus(corpus, manifest, config, records);
    std::size_t visited = 0;
    const auto plain = tgmr::inspectCorpus(corpus,
        [&](const tgmr::PatchRecord &record, std::uint64_t index) {
            require(index == visited, "visitor order changed");
            require(record.rgb[20] == static_cast<std::uint16_t>(20 * 17 + index),
                    "record pixels changed");
            ++visited;
        });
    require(visited == records.size(), "not all records were visited");
    require(plain.header.recordCount == 3
        && plain.header.splitCounts == std::array<std::uint64_t, 3>{{1, 1, 1}},
        "corpus counts changed");
    require(plain.splitPayloadSha256[0] != plain.splitPayloadSha256[1]
        && plain.splitPayloadSha256[1] != plain.splitPayloadSha256[2]
        && tgmr::canonicalInspectionJson(plain).find("split_payload_sha256")
            != std::string::npos,
        "corpus split payload identities are absent or collapsed");
    require(plain.fileBytes == tgmr::TGPC_HEADER_BYTES
        + records.size() * tgmr::TGPC_RECORD_BYTES,
        "corpus size changed");
    tgmr::deterministicGzip(corpus, gzipA, 9);
    tgmr::deterministicGzip(corpus, gzipB, 9);
    require(read(gzipA) == read(gzipB), "deterministic gzip output changed");
    const auto compressed = tgmr::inspectCorpus(gzipA);
    require(compressed.compressed
        && compressed.header.payloadSha256 == plain.header.payloadSha256
        && compressed.splitPayloadSha256 == plain.splitPayloadSha256,
            "compressed/uncompressed corpus parity failed");
    const auto statistics = tgmr::analyzeCorpus(corpus);
    const auto derivedStatistics = tgmr::analyzeCorpusTrainingTertiles(corpus);
    require(statistics.uniqueSources == std::array<std::uint64_t, 3>{{1, 1, 1}},
            "corpus source counts changed");
    require(tgmr::canonicalCorpusReportJson(statistics).find(
        "rawtherapee-tgmr-corpus-statistics-v1") != std::string::npos,
        "corpus JSON report contract changed");
    require(tgmr::corpusReportCsv(statistics).find("brightness,train") != std::string::npos,
        "corpus CSV report contract changed");
    require(tgmr::corpusReportHtml(statistics).find("<!doctype html>") == 0,
        "corpus HTML report contract changed");
    require(tgmr::canonicalBalanceJson(statistics, 1, 1).find(
        "\"status\": \"deficient\"") != std::string::npos,
        "corpus balance calculation changed");
    require(std::isfinite(derivedStatistics.brightnessThresholds[0])
        && std::isfinite(derivedStatistics.chromaThresholds[1])
        && std::isfinite(derivedStatistics.textureThresholds[1]),
        "training-derived corpus strata are not finite");

    auto corrupt = read(corpus);
    corrupt.back() ^= 1;
    const std::string corruptPath = temporary("-corrupt.tgpc");
    {
        std::ofstream stream(corruptPath, std::ios::binary);
        stream.write(reinterpret_cast<const char *>(corrupt.data()), corrupt.size());
    }
    bool rejected = false;
    try {
        tgmr::inspectCorpus(corruptPath);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "corrupt corpus was accepted");
    std::remove(corpus.c_str());
    std::remove(gzipA.c_str());
    std::remove(gzipB.c_str());
    std::remove(corruptPath.c_str());
}

void testResumableCorpusWriter()
{
    const std::string resumed = temporary("-resumed.tgpc");
    const std::string reference = temporary("-reference.tgpc");
    const std::string work = temporary("-pack-work");
    std::vector<tgmr::PatchRecord> records(7);
    for (std::size_t index = 0; index < records.size(); ++index) {
        auto &record = records[index];
        record.sourceOrdinal = static_cast<std::uint32_t>(index / 2);
        record.x = static_cast<std::uint32_t>(index);
        record.y = static_cast<std::uint32_t>(index + 1);
        record.split = index < 4 ? tgmr::CorpusSplit::TRAIN
            : index < 6 ? tgmr::CorpusSplit::VALIDATION : tgmr::CorpusSplit::TEST;
        record.rgb.fill(static_cast<std::uint16_t>(1000 + index));
    }
    const auto manifest = tgmr::sha256("resume-manifest", 15);
    const auto configuration = tgmr::sha256("resume-configuration", 20);
    tgmr::CorpusWriteOptions options;
    options.workDirectory = work;
    options.checkpointRecords = 2;
    options.progressSeconds = 3600;
    bool interrupted = false;
    try {
        tgmr::writeCorpusStreamResumable(
            resumed, manifest, configuration, records.size(),
            [&](std::uint64_t skip, const tgmr::CorpusRecordSink &sink) {
                require(skip == 0, "new resumable writer did not start at zero");
                for (std::size_t index = 0; index < 3; ++index) sink(records[index]);
                throw std::runtime_error("injected interruption");
            }, options);
    } catch (const std::runtime_error &) {
        interrupted = true;
    }
    require(interrupted && std::filesystem::is_regular_file(resumed + ".tmp")
        && std::filesystem::is_regular_file(
            std::filesystem::path(work) / "checkpoint.txt"),
        "resumable writer did not retain authenticated interruption state");
    std::uint64_t observedSkip = 0;
    tgmr::writeCorpusStreamResumable(
        resumed, manifest, configuration, records.size(),
        [&](std::uint64_t skip, const tgmr::CorpusRecordSink &sink) {
            observedSkip = skip;
            for (std::size_t index = static_cast<std::size_t>(skip);
                 index < records.size(); ++index) {
                sink(records[index]);
            }
        }, options);
    require(observedSkip == 3,
        "resumable writer did not restart at the durable record boundary");
    tgmr::writeCorpus(reference, manifest, configuration, records);
    require(read(resumed) == read(reference)
        && !std::filesystem::exists(
            std::filesystem::path(work) / "checkpoint.txt"),
        "resumed TGPC differs from uninterrupted canonical output");

    const std::string corrupt = temporary("-corrupt-resume.tgpc");
    const std::string corruptWork = temporary("-corrupt-pack-work");
    options.workDirectory = corruptWork;
    try {
        tgmr::writeCorpusStreamResumable(
            corrupt, manifest, configuration, records.size(),
            [&](std::uint64_t, const tgmr::CorpusRecordSink &sink) {
                sink(records[0]);
                throw std::runtime_error("injected interruption");
            }, options);
    } catch (const std::runtime_error &) {
    }
    {
        std::fstream partial(corrupt + ".tmp", std::ios::in | std::ios::out | std::ios::binary);
        require(static_cast<bool>(partial), "cannot mutate resumable test payload");
        partial.seekp(tgmr::TGPC_HEADER_BYTES + 70);
        const char value = '\x7f';
        partial.write(&value, 1);
    }
    bool rejectedCorruption = false;
    try {
        tgmr::writeCorpusStreamResumable(
            corrupt, manifest, configuration, records.size(),
            [&](std::uint64_t, const tgmr::CorpusRecordSink &) {}, options);
    } catch (const std::runtime_error &) {
        rejectedCorruption = true;
    }
    require(rejectedCorruption,
        "resumable writer accepted a payload that changed after its checkpoint");

    std::remove(resumed.c_str());
    std::remove(reference.c_str());
    std::remove((corrupt + ".tmp").c_str());
    std::filesystem::remove_all(work);
    std::filesystem::remove_all(corruptWork);
}

void testSourceLimitedTrainingMatrix()
{
    std::vector<tgmr::PatchRecord> records(3);
    for (std::size_t source = 0; source < records.size(); ++source) {
        auto &record = records[source];
        record.sourceIdSha256 = tgmr::sha256(&source, sizeof(source));
        record.sourceOrdinal = static_cast<std::uint32_t>(source);
        record.split = tgmr::CorpusSplit::TRAIN;
        for (std::size_t value = 0; value < record.rgb.size(); ++value) {
            record.rgb[value] = static_cast<std::uint16_t>(1000 * source + value);
        }
    }
    const std::string corpus = temporary("-source-limit.tgpc");
    tgmr::writeCorpus(corpus, tgmr::sha256("manifest", 8),
                      tgmr::sha256("configuration", 13), records);
    std::uint64_t samples = 0;
    std::array<std::uint8_t, 32> digest{};
    const auto phase = tgmr::xtransPhaseContracts()[0];
    const auto limited = tgmr::loadPhaseTrainingMatrix(
        corpus, phase, samples, digest, 2);
    require(samples == 2 && limited.size() == 2 * 51,
            "source-limited matrix did not select the first two sources");
    const auto complete = tgmr::loadPhaseTrainingMatrix(
        corpus, phase, samples, digest);
    require(samples == 3 && complete.size() == 3 * 51,
            "unlimited matrix did not retain every training source");
    bool rejected = false;
    try {
        tgmr::loadPhaseTrainingMatrix(corpus, phase, samples, digest, 4);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "source limit larger than the corpus was accepted");
    std::remove(corpus.c_str());
}

void testImageManifestAndPack()
{
    const std::string imagePath = temporary(".png");
    const std::string interlacedImagePath = temporary("-interlaced.png");
    const std::string manifestPath = temporary(".jsonl");
    const std::string classificationPath = temporary("-classification.jsonl");
    const std::string corpusPath = temporary(".tgpc");
    const std::string legacyWriterCorpus = temporary("-legacy-writer.tgpc");
    const std::string noisyCorpusA = temporary("-noise-a.tgpc");
    const std::string noisyCorpusB = temporary("-noise-b.tgpc");
    const std::string identityCorpus = temporary("-identity.tgpc");
    const std::string multiPlainCorpus = temporary("-multi-plain.tgpc");
    const std::string multiNoiseCorpus = temporary("-multi-noise.tgpc");
    const std::string multiIdentityCorpus = temporary("-multi-identity.tgpc");
    writePngFixture(imagePath, 10, 10);
    writeInterlacedPngFixture(interlacedImagePath, 10, 10);
    const auto image = tgmr::loadLinearImage(imagePath);
    const auto interlacedImage = tgmr::loadLinearImage(interlacedImagePath);
    const auto classification = tgmr::classifyImage(image);
    const auto interlacedClassification = tgmr::classifyImage(interlacedImage);
    require(image.width == 10 && image.height == 10 && image.fileType == "png",
            "PNG fixture decoded incorrectly");
    require(classification.decodedPixelSha256.size() == 64
        && classification.perceptualHash.size() == 16,
        "image classification identities are malformed");
    require(interlacedImage.rgb == image.rgb
        && interlacedClassification.decodedPixelSha256
            == classification.decodedPixelSha256,
        "Adam7 PNG decoding differs from the equivalent non-interlaced image");
    const auto proposedA = tgmr::proposePatches(image, 1234, 12);
    const auto proposedB = tgmr::proposePatches(image, 1234, 12);
    require(proposedA.size() == 12 && proposedB.size() == 12,
            "patch proposal count changed");
    std::set<std::pair<std::uint32_t, std::uint32_t>> proposedPositions;
    std::size_t coverage = 0;
    for (std::size_t index = 0; index < proposedA.size(); ++index) {
        require(proposedA[index].x == proposedB[index].x
            && proposedA[index].y == proposedB[index].y
            && proposedA[index].coverage == proposedB[index].coverage,
            "patch proposal is not deterministic");
        proposedPositions.emplace(proposedA[index].x, proposedA[index].y);
        coverage += proposedA[index].coverage;
    }
    require(proposedPositions.size() == 12 && coverage == 3,
            "patch proposal is not 75% uniform/25% coverage or contains duplicates");
    const std::filesystem::path cache = std::filesystem::path(imagePath).parent_path();
    const std::string cacheName = std::filesystem::path(imagePath).filename().string();
    std::ofstream manifest(manifestPath, std::ios::binary);
    manifest << '{'
        << "\"advertised_checksum\":null,"
        << "\"author\":\"TGMR test author\","
        << "\"cache_filename\":\"" << cacheName << "\","
        << "\"classification\":{"
        << "\"channel_means\":[0,0,0],"
        << "\"chroma_ratio_mean\":0,"
        << "\"clipped_black_fraction\":0,"
        << "\"clipped_white_fraction\":0,"
        << "\"gradient_rms\":0,"
        << "\"hue_degrees\":0,"
        << "\"jpeg_blockiness\":0,"
        << "\"laplacian_rms\":0,"
        << "\"local_contrast\":0,"
        << "\"luminance_mean\":0,"
        << "\"luminance_p01\":0,"
        << "\"luminance_p99\":0,"
        << "\"luminance_stddev\":0,"
        << "\"perceptual_hash\":\"" << classification.perceptualHash << "\","
        << "\"saturation_mean\":0},"
        << "\"decoded_pixel_sha256\":\"" << classification.decodedPixelSha256 << "\","
        << "\"fallback_urls\":[],"
        << "\"file_type\":\"png\","
        << "\"format\":\"rawtherapee-tgmr-corpus-source-manifest-v1\","
        << "\"height\":10,"
        << "\"icc_identity\":\"assumed-srgb\","
        << "\"landing_page\":\"https://example.invalid/fixture\","
        << "\"license\":\"CC0-1.0\","
        << "\"license_url\":\"https://creativecommons.org/publicdomain/zero/1.0/\","
        << "\"orientation\":1,"
        << "\"original_url\":\"file://" << imagePath << "\","
        << "\"patch_coordinates\":[{\"augmentation\":{\"exposure_stops\":0,"
           "\"kind\":0,\"matrix_id\":0,\"sequence\":0,"
           "\"white_balance\":[1,1,1]},\"coverage\":false,\"x\":1,\"y\":1},"
           "{\"augmentation\":{\"exposure_stops\":1,\"kind\":1,"
           "\"matrix_id\":1,\"sequence\":1,\"white_balance\":[1.25,1,0.8]},"
           "\"coverage\":true,\"coverage_class\":3,\"x\":2,\"y\":2}],"
        << "\"patch_sampling_seed\":\"0x52545447\","
        << "\"selected\":true,"
        << "\"selection_status\":\"accepted-corpus-v1\","
        << "\"sha256\":\"" << tgmr::hex(tgmr::sha256File(imagePath)) << "\","
        << "\"source_id\":\"fixture-png-1\","
        << "\"split\":\"train\","
        << "\"title\":\"fixture\","
        << "\"width\":10}\n";
    manifest.close();
    const auto records = tgmr::readSourceManifest(manifestPath);
    require(records.size() == 1 && records[0].patchSamplingSeed == 0x52545447,
            "source manifest did not preserve the reviewed contract");
    const auto verified = tgmr::verifySources(records, cache.string());
    require(verified.authenticated == 1 && verified.selected == 1,
            "source verification failed");
    tgmr::classifySources(records, cache.string(), classificationPath);
    require(read(classificationPath).size() > 100,
            "source classification output is unexpectedly empty");
    tgmr::packSources(records, manifestPath, cache.string(), corpusPath);
    const auto packed = tgmr::inspectCorpus(corpusPath);
    require(packed.header.recordCount == 2 && packed.header.splitCounts[0] == 2,
            "manifest packing produced the wrong corpus counts");
    tgmr::packSources(records, manifestPath, cache.string(), noisyCorpusA,
                      tgmr::PackNoiseRecipe::SENSOR_V1);
    tgmr::packSources(records, manifestPath, cache.string(), noisyCorpusB,
                      tgmr::PackNoiseRecipe::SENSOR_V1);
    require(read(noisyCorpusA) == read(noisyCorpusB),
            "sensor-v1 augmentation is not byte deterministic");
    // The only fixture patch is identity.  Noise must not alter its samples,
    // although the corpus configuration identity still distinguishes recipes.
    std::vector<tgmr::PatchRecord> plainRecords;
    std::vector<tgmr::PatchRecord> noisyRecords;
    tgmr::inspectCorpus(corpusPath, [&](const tgmr::PatchRecord &value, std::uint64_t) {
        plainRecords.push_back(value);
    });
    const auto noisyInspection = tgmr::inspectCorpus(
        noisyCorpusA, [&](const tgmr::PatchRecord &value, std::uint64_t) {
            noisyRecords.push_back(value);
        });
    static const char productionConfiguration[] =
        "tgpc-v1:linear-srgb:7x7:chw:uint16:clip:camera-matrix:exposure-wb:matrix-set-v1:noise-none";
    tgmr::writeCorpusStream(
        legacyWriterCorpus, tgmr::sha256File(manifestPath),
        tgmr::sha256(productionConfiguration, std::strlen(productionConfiguration)),
        [&](const tgmr::CorpusRecordSink &sink) {
            for (const auto &record : plainRecords) sink(record);
        });
    require(plainRecords.size() == 2 && noisyRecords.size() == 2
        && plainRecords[0].rgb == noisyRecords[0].rgb
        && noisyRecords[0].augmentationKind == 0
        && plainRecords[1].rgb != noisyRecords[1].rgb
        && plainRecords[1].augmentationKind == 1
        && noisyRecords[1].augmentationKind == 2
        && packed.header.configurationSha256 != noisyInspection.header.configurationSha256,
        "noise recipe did not preserve identity, perturb augmentation, or bind configuration");
    require(read(corpusPath) == read(legacyWriterCorpus),
        "restartable production-v1 pack changed canonical TGPC bytes");

    tgmr::PackOptions identityOptions;
    identityOptions.trainingAugmentation =
        tgmr::TrainingAugmentationRecipe::IDENTITY_ONLY;
    tgmr::packSourcesWithOptions(
        records, manifestPath, cache.string(), identityCorpus, identityOptions);
    std::vector<tgmr::PatchRecord> identityRecords;
    tgmr::inspectCorpus(identityCorpus,
        [&](const tgmr::PatchRecord &value, std::uint64_t) {
            identityRecords.push_back(value);
        });
    require(identityRecords.size() == 2
        && identityRecords[0].rgb == plainRecords[0].rgb
        && identityRecords[1].rgb != plainRecords[1].rgb
        && identityRecords[1].augmentationKind == 0
        && identityRecords[1].matrixId == 0
        && identityRecords[1].exposureStopsQ8 == 0
        && identityRecords[1].whiteBalanceQ12
            == std::array<std::uint16_t, 3>{{4096,4096,4096}},
        "identity-only training did not remove every train-time augmentation");
    tgmr::PackOptions contradictory = identityOptions;
    contradictory.noise = tgmr::PackNoiseRecipe::SENSOR_V1;
    bool rejectedContradictoryRecipe = false;
    try {
        tgmr::packSourcesWithOptions(records, manifestPath, cache.string(),
                                     temporary("-contradictory.tgpc"), contradictory);
    } catch (const std::runtime_error &) {
        rejectedContradictoryRecipe = true;
    }
    require(rejectedContradictoryRecipe,
        "packer accepted identity-only training combined with sensor noise");

    std::vector<tgmr::SourceRecord> multiSplitRecords = records;
    tgmr::SourceRecord validation = records.front();
    validation.sourceId = "fixture-png-validation";
    validation.split = tgmr::CorpusSplit::VALIDATION;
    validation.patches[1].matrixId = 101;
    multiSplitRecords.push_back(validation);
    tgmr::SourceRecord test = validation;
    test.sourceId = "fixture-png-test";
    test.split = tgmr::CorpusSplit::TEST;
    test.patches[1].matrixId = 102;
    multiSplitRecords.push_back(test);
    tgmr::PackOptions productionOptions;
    tgmr::packSourcesWithOptions(multiSplitRecords, manifestPath, cache.string(),
                                 multiPlainCorpus, productionOptions);
    tgmr::PackOptions noiseOptions;
    noiseOptions.noise = tgmr::PackNoiseRecipe::SENSOR_V1;
    tgmr::packSourcesWithOptions(multiSplitRecords, manifestPath, cache.string(),
                                 multiNoiseCorpus, noiseOptions);
    tgmr::packSourcesWithOptions(multiSplitRecords, manifestPath, cache.string(),
                                 multiIdentityCorpus, identityOptions);
    auto loadRecords = [](const std::string &path) {
        std::vector<tgmr::PatchRecord> output;
        tgmr::inspectCorpus(path,
            [&](const tgmr::PatchRecord &value, std::uint64_t) {
                output.push_back(value);
            });
        return output;
    };
    const auto multiPlain = loadRecords(multiPlainCorpus);
    const auto multiNoise = loadRecords(multiNoiseCorpus);
    const auto multiIdentity = loadRecords(multiIdentityCorpus);
    require(multiPlain.size() == 6 && multiNoise.size() == 6
        && multiIdentity.size() == 6,
        "multi-split augmentation fixture changed record count");
    for (std::size_t index = 2; index < 6; ++index) {
        require(multiPlain[index].rgb == multiNoise[index].rgb
            && multiPlain[index].rgb == multiIdentity[index].rgb
            && multiPlain[index].augmentationKind == multiNoise[index].augmentationKind
            && multiPlain[index].augmentationKind == multiIdentity[index].augmentationKind
            && multiPlain[index].matrixId == multiNoise[index].matrixId
            && multiPlain[index].matrixId == multiIdentity[index].matrixId,
            "validation/test corpus changed across training augmentation recipes");
    }
    const auto multiPlainBytes = read(multiPlainCorpus);
    const auto multiNoiseBytes = read(multiNoiseCorpus);
    const auto multiIdentityBytes = read(multiIdentityCorpus);
    const std::size_t evaluationOffset = tgmr::TGPC_HEADER_BYTES
        + 2 * tgmr::TGPC_RECORD_BYTES;
    require(std::equal(multiPlainBytes.begin() + evaluationOffset,
                       multiPlainBytes.end(), multiNoiseBytes.begin() + evaluationOffset)
        && std::equal(multiPlainBytes.begin() + evaluationOffset,
                      multiPlainBytes.end(), multiIdentityBytes.begin() + evaluationOffset),
        "validation/test TGPC record bytes differ across training recipes");
    require(multiPlain[1].rgb != multiNoise[1].rgb
        && multiPlain[1].rgb != multiIdentity[1].rgb,
        "training variants did not produce distinct train patches");
    std::remove(imagePath.c_str());
    std::remove(interlacedImagePath.c_str());
    std::remove(manifestPath.c_str());
    std::remove(classificationPath.c_str());
    std::filesystem::remove_all(classificationPath + ".work");
    std::remove(corpusPath.c_str());
    std::remove(legacyWriterCorpus.c_str());
    std::remove(noisyCorpusA.c_str());
    std::remove(noisyCorpusB.c_str());
    std::remove(identityCorpus.c_str());
    std::remove(multiPlainCorpus.c_str());
    std::remove(multiNoiseCorpus.c_str());
    std::remove(multiIdentityCorpus.c_str());
    std::filesystem::remove_all(corpusPath + ".work");
    std::filesystem::remove_all(noisyCorpusA + ".work");
    std::filesystem::remove_all(noisyCorpusB + ".work");
    std::filesystem::remove_all(identityCorpus + ".work");
    std::filesystem::remove_all(multiPlainCorpus + ".work");
    std::filesystem::remove_all(multiNoiseCorpus + ".work");
    std::filesystem::remove_all(multiIdentityCorpus + ".work");
}

void testTiff16Orientation()
{
    const std::string path = temporary(".tiff");
    writeTiffFixture(path, 9, 7);
    const auto image = tgmr::loadLinearImage(path);
    require(image.fileType == "tiff" && image.orientation == ORIENTATION_RIGHTTOP,
            "TIFF orientation metadata was not preserved");
    require(image.width == 7 && image.height == 9 && image.rgb.size() == 7 * 9 * 3,
            "TIFF orientation was not applied to RGB16 pixels");
    require(std::isfinite(image.rgb.front()) && std::isfinite(image.rgb.back()),
            "TIFF RGB16 conversion produced non-finite values");
    std::remove(path.c_str());
}

void testResumableSourceFinalization()
{
    const std::string cachePath = temporary("-finalize-cache");
    const std::filesystem::path cache(cachePath);
    std::filesystem::create_directories(cache);
    const auto firstImage = cache / "first.png";
    const auto secondImage = cache / "second.png";
    const std::string stagedSecondImage = temporary("-finalize-second.png");
    writePngFixture(firstImage.string(), 30, 30);
    writePngFixture(stagedSecondImage, 31, 30);
    auto source = [&](const char *id, const char *filename, tgmr::CorpusSplit split,
                      std::uint64_t seed, const std::string &imagePath) {
        const auto image = tgmr::loadLinearImage(imagePath);
        const auto classification = tgmr::classifyImage(image);
        tgmr::SourceRecord record;
        record.manifestV2 = true;
        record.sourceId = id;
        record.split = split;
        record.splitAssigned = true;
        record.selected = true;
        record.selectionStatus = "accepted-corpus-v1";
        record.cacheFilename = filename;
        record.originalUrl = std::string("https://example.invalid/") + filename;
        record.landingPage = std::string("https://example.invalid/source/") + id;
        record.sha256 = tgmr::hex(tgmr::sha256File(imagePath));
        record.decodedPixelSha256 = classification.decodedPixelSha256;
        record.author = "Finalization fixture author";
        record.authorId = std::string("fixture-author:") + id;
        record.authorUrl = "https://example.invalid/author";
        record.title = "Finalization fixture";
        record.license = "CC0-1.0";
        record.licenseUrl = "https://creativecommons.org/publicdomain/zero/1.0/";
        record.fileType = image.fileType;
        record.width = image.width;
        record.height = image.height;
        record.orientation = image.orientation;
        record.iccIdentity = image.iccIdentity;
        record.perceptualHash = classification.perceptualHash;
        record.pHash = classification.pHash;
        record.classification = classification;
        if (split == tgmr::CorpusSplit::TRAIN) {
            record.perceptualHash = record.classification.perceptualHash =
                "0000000000000000";
            record.pHash = record.classification.pHash = "0000000000000000";
        } else {
            record.perceptualHash = record.classification.perceptualHash =
                "ffffffffffffffff";
            record.pHash = record.classification.pHash = "ffffffffffffffff";
        }
        record.catalogName = "smithsonian-open-access";
        record.catalogRevision = "fixture-v1";
        record.catalogSnapshotSha256 = std::string(64, '1');
        record.upstreamSourceId = id;
        record.rightsEvidenceUrl = record.landingPage;
        record.rightsEvidenceRevision = "fixture-v1";
        record.rightsEvidenceSha256 = std::string(64, '2');
        record.rightsReviewStatus = "approved";
        record.peopleReviewStatus = "not-applicable";
        record.contentTags = {"macro-specimen"};
        record.patchSamplingSeed = seed;
        return record;
    };
    const std::vector<tgmr::SourceRecord> records{
        source("finalize-first", "first.png", tgmr::CorpusSplit::TRAIN, 1,
               firstImage.string()),
        source("finalize-second", "second.png", tgmr::CorpusSplit::VALIDATION, 2,
               stagedSecondImage),
    };
    const std::string input = temporary("-finalize-input.jsonl");
    const std::string resumed = temporary("-finalize-resumed.jsonl");
    const std::string clean = temporary("-finalize-clean.jsonl");
    const std::string resumeWork = temporary("-finalize-resume-work");
    const std::string cleanWork = temporary("-finalize-clean-work");
    tgmr::writeSourceManifestV2(records, input);
    tgmr::FinalizationOptions options;
    options.workDirectory = resumeWork;
    options.progressSeconds = 3600;
    bool interrupted = false;
    try {
        tgmr::finalizeSources(records, input, cache.string(), resumed, options);
    } catch (const std::runtime_error &) {
        interrupted = true;
    }
    require(interrupted
        && std::filesystem::is_regular_file(
            std::filesystem::path(resumeWork) / "progress.json"),
        "source finalization did not retain interruption progress");
    std::filesystem::copy_file(stagedSecondImage, secondImage);
    tgmr::finalizeSources(records, input, cache.string(), resumed, options);
    tgmr::FinalizationOptions cleanOptions;
    cleanOptions.workDirectory = cleanWork;
    cleanOptions.progressSeconds = 3600;
    tgmr::finalizeSources(records, input, cache.string(), clean, cleanOptions);
    const auto finalized = tgmr::readSourceManifest(resumed);
    require(read(resumed) == read(clean) && finalized.size() == 2
        && finalized[0].patches.size() == 256
        && finalized[1].patches.size() == 128,
        "resumed source finalization differs from a clean deterministic run");

    std::filesystem::remove_all(cache);
    std::filesystem::remove_all(resumeWork);
    std::filesystem::remove_all(cleanWork);
    std::remove(stagedSecondImage.c_str());
    std::remove(input.c_str());
    std::remove(resumed.c_str());
    std::remove(clean.c_str());
}

void testGrayJpegIccProfile()
{
    const std::string path = temporary("-gray-icc.jpg");
    writeGrayJpegWithIccFixture(path, 80, 72);
    const auto full = tgmr::loadLinearImageAndSha256(path, false);
    const auto proxy = tgmr::loadLinearImageAndSha256(path, true);
    require(full.image.width == 80 && full.image.height == 72
        && full.image.iccIdentity.rfind("sha256:", 0) == 0
        && !full.image.assumedSrgb,
        "grayscale JPEG ICC profile was not retained");
    require(proxy.proxy && proxy.image.width > 0 && proxy.image.height > 0,
        "grayscale JPEG ICC profile failed proxy conversion");
    for (std::size_t index = 0; index < full.image.rgb.size(); index += 3) {
        require(std::isfinite(full.image.rgb[index])
            && std::abs(full.image.rgb[index] - full.image.rgb[index + 1]) < 5e-5
            && std::abs(full.image.rgb[index] - full.image.rgb[index + 2]) < 5e-5,
            "grayscale ICC conversion did not produce neutral linear RGB");
    }
    std::remove(path.c_str());
}

void testCmykJpegIccProfile()
{
    const std::string profiled = temporary("-cmyk-icc.jpg");
    const std::string unprofiled = temporary("-cmyk-no-icc.jpg");
    writeCmykJpegFixture(profiled, 80, 72, true);
    writeCmykJpegFixture(unprofiled, 80, 72, false);
    const auto full = tgmr::loadLinearImageAndSha256(profiled, false);
    const auto proxy = tgmr::loadLinearImageAndSha256(profiled, true);
    require(full.image.width == 80 && full.image.height == 72
        && full.image.iccIdentity.rfind("sha256:", 0) == 0
        && !full.image.assumedSrgb && proxy.proxy,
        "profiled CMYK JPEG was not converted to linear RGB");
    require(std::all_of(full.image.rgb.begin(), full.image.rgb.end(),
            [](double value) { return std::isfinite(value); }),
        "profiled CMYK JPEG produced a non-finite value");
    bool rejected = false;
    try {
        (void)tgmr::loadLinearImage(unprofiled);
    } catch (const std::runtime_error &error) {
        rejected = std::string(error.what()) == "CMYK image lacks an embedded ICC profile";
    }
    require(rejected, "unprofiled CMYK JPEG was not rejected deterministically");
    std::remove(profiled.c_str());
    std::remove(unprofiled.c_str());
}

void testParallelClassificationAndProxy()
{
    const std::string base = temporary("-classifier");
    const std::filesystem::path cache = std::filesystem::path(base) / "cache";
    std::filesystem::create_directories(cache / "train/a/b/c");
    const std::string input = base + "-fetched.jsonl";
    const std::string catalogInput = base + "-catalog.jsonl";
    std::ofstream manifest(input, std::ios::binary);
    std::ofstream catalogManifest(catalogInput, std::ios::binary);
    for (unsigned index = 0; index < 6; ++index) {
        std::ostringstream name;
        name << "train/a/b/c/abcdef012345678" << index << ".jpg";
        const auto path = cache / name.str();
        writeJpegFixture(path.string(), 80 + index, 72 + index, index + 1);
        manifest << "{\"cache_filename\":\"" << name.str()
            << "\",\"catalog\":\"openimages-cvdf-v5-boxable\","
            << "\"format\":\"rawtherapee-tgmr-fetched-candidate-v1\","
            << "\"sha256\":\"" << tgmr::hex(tgmr::sha256File(path.string()))
            << "\",\"upstream_source_id\":\"abcdef012345678" << index << "\"}\n";
        catalogManifest << "{\"catalog\":\"openimages-cvdf-v5-boxable\","
            << "\"format\":\"rawtherapee-tgmr-catalog-candidate-v1\","
            << "\"upstream_source_id\":\"abcdef012345678" << index << "\"}\n";
    }
    manifest.close();
    catalogManifest.close();

    tgmr::ClassificationOptions serial;
    serial.jobs = 1;
    serial.checkpointImages = 2;
    serial.checkpointSeconds = 1;
    serial.progressSeconds = 1;
    serial.retries = 0;
    serial.workDirectory = base + "-serial-work";
    const std::string serialOutput = base + "-serial.jsonl";
    tgmr::classifyFetchedCandidates(input, cache.string(), serialOutput, false, serial);

    tgmr::ClassificationOptions parallel = serial;
    parallel.jobs = 4;
    parallel.workDirectory = base + "-parallel-work";
    const std::string parallelOutput = base + "-parallel.jsonl";
    tgmr::classifyFetchedCandidates(input, cache.string(), parallelOutput, false, parallel);
    require(read(serialOutput) == read(parallelOutput),
            "parallel classification changed canonical output ordering or values");

    // A completed checkpoint set is itself a valid resume point.  A stale
    // incomplete temporary segment must be ignored.
    std::remove(parallelOutput.c_str());
    {
        std::ofstream incomplete(
            std::filesystem::path(parallel.workDirectory) / "segment-stale.tmp");
        incomplete << "incomplete";
    }
    tgmr::classifyFetchedCandidates(input, cache.string(), parallelOutput, false, parallel);
    require(read(serialOutput) == read(parallelOutput),
            "checkpoint resume did not reproduce canonical classifier output");

    bool rejectedMismatchedResume = false;
    const std::string changedInput = base + "-changed-fetched.jsonl";
    {
        std::ifstream original(input);
        std::string contents{
            std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>()};
        const std::size_t identity = contents.find("abcdef0123456780");
        require(identity != std::string::npos, "cannot mutate classifier input fixture");
        contents.replace(identity, 16, "abcdef0123456790");
        std::ofstream changed(changedInput);
        changed << contents;
    }
    try {
        tgmr::classifyFetchedCandidates(
            changedInput, cache.string(), base + "-changed.jsonl", false, parallel);
    } catch (const std::exception &) {
        rejectedMismatchedResume = true;
    }
    require(rejectedMismatchedResume,
            "classifier resumed a work directory with a different input identity");

    tgmr::ClassificationOptions proxy = parallel;
    proxy.proxy = true;
    proxy.openImagesCvdfSplit = "train";
    proxy.workDirectory = base + "-proxy-work";
    const std::string proxyOutput = base + "-proxy.jsonl";
    tgmr::classifyFetchedCandidates(
        catalogInput, cache.string(), proxyOutput, false, proxy);
    const auto proxyBytes = read(proxyOutput);
    const std::string proxyJson(proxyBytes.begin(), proxyBytes.end());
    require(proxyJson.find("rawtherapee-tgmr-image-proxy-classification-v1")
                != std::string::npos
            && proxyJson.find("\"proxy_scale_denominator\":8") != std::string::npos
            && proxyJson.find("\"proxy_width\":10") != std::string::npos
            && proxyJson.find("\"width\":80") != std::string::npos
            && proxyJson.find("\"source_sha256\":\"") != std::string::npos,
            "JPEG proxy classification lacks its distinct identity or dimensions");

    bool rejectedTraversal = false;
    const std::string traversal = base + "-traversal.jsonl";
    {
        std::ofstream invalid(traversal);
        invalid << "{\"cache_filename\":\"../escape.jpg\","
            << "\"catalog\":\"openimages-cvdf-v5-boxable\","
            << "\"format\":\"rawtherapee-tgmr-fetched-candidate-v1\","
            << "\"sha256\":\"" << std::string(64, '0') << "\","
            << "\"upstream_source_id\":\"escape\"}\n";
    }
    try {
        tgmr::classifyFetchedCandidates(
            traversal, cache.string(), base + "-escape.jsonl", false, serial);
    } catch (const std::exception &) {
        rejectedTraversal = true;
    }
    require(rejectedTraversal, "classifier accepted a cache path traversal");

    const std::string retryInput = base + "-retry.jsonl";
    {
        std::ifstream all(input);
        std::string first;
        std::getline(all, first);
        std::ofstream one(retryInput);
        one << first << '\n';
    }
    const auto retrySource = cache / "train/a/b/c/abcdef0123456780.jpg";
    const auto heldSource = cache / "train/a/b/c/abcdef0123456780.jpg.held";
    std::filesystem::rename(retrySource, heldSource);
    tgmr::ClassificationOptions retry = serial;
    retry.workDirectory = base + "-retry-work";
    bool reportedMissing = false;
    try {
        tgmr::classifyFetchedCandidates(
            retryInput, cache.string(), base + "-retry-output.jsonl", false, retry);
    } catch (const std::exception &) {
        reportedMissing = std::filesystem::is_regular_file(
            std::filesystem::path(retry.workDirectory) / "failures.json");
    }
    tgmr::ClassificationOptions allowFailures = serial;
    allowFailures.allowFailures = true;
    allowFailures.workDirectory = base + "-allow-failures-work";
    const std::string partialOutput = base + "-partial-output.jsonl";
    tgmr::classifyFetchedCandidates(
        input, cache.string(), partialOutput, false, allowFailures);
    {
        std::ifstream partial(partialOutput);
        std::size_t lines = 0;
        std::string line;
        while (std::getline(partial, line)) ++lines;
        require(lines == 5 && std::filesystem::is_regular_file(
                std::filesystem::path(allowFailures.workDirectory) / "failures.json"),
            "explicit candidate failure omission did not preserve five successes and audit");
    }
    std::filesystem::rename(heldSource, retrySource);
    require(reportedMissing, "classifier did not checkpoint a missing-input failure");
    tgmr::classifyFetchedCandidates(
        retryInput, cache.string(), base + "-retry-output.jsonl", false, retry);
    require(std::filesystem::is_regular_file(base + "-retry-output.jsonl"),
            "classifier cached a failure instead of retrying it on resume");

    std::filesystem::remove_all(base);
    std::remove(input.c_str());
    std::remove(catalogInput.c_str());
    std::remove(serialOutput.c_str());
    std::remove(parallelOutput.c_str());
    std::remove(proxyOutput.c_str());
    std::remove(traversal.c_str());
    std::remove(changedInput.c_str());
    std::remove(retryInput.c_str());
    std::remove((base + "-retry-output.jsonl").c_str());
    std::remove(partialOutput.c_str());
    std::filesystem::remove_all(serial.workDirectory);
    std::filesystem::remove_all(parallel.workDirectory);
    std::filesystem::remove_all(proxy.workDirectory);
    std::filesystem::remove_all(retry.workDirectory);
    std::filesystem::remove_all(allowFailures.workDirectory);
}

void testManifestNearDuplicateLeakage()
{
    const std::string path = temporary("-near-duplicate.jsonl");
    auto line = [](const char *id, const char *split, const char *author,
                   char decodedDigit, const char *perceptual) {
        std::ostringstream output;
        output << "{\"advertised_checksum\":null,\"author\":\"" << author
            << "\",\"cache_filename\":\"" << id << ".png\","
            << "\"classification\":{\"channel_means\":[0,0,0],"
            << "\"chroma_ratio_mean\":0,\"clipped_black_fraction\":0,"
            << "\"clipped_white_fraction\":0,\"gradient_rms\":0,"
            << "\"hue_degrees\":0,\"jpeg_blockiness\":0,"
            << "\"laplacian_rms\":0,\"local_contrast\":0,"
            << "\"luminance_mean\":0,\"luminance_p01\":0,"
            << "\"luminance_p99\":0,\"luminance_stddev\":0,"
            << "\"perceptual_hash\":\"" << perceptual << "\","
            << "\"saturation_mean\":0},\"decoded_pixel_sha256\":\""
            << std::string(64, decodedDigit) << "\",\"fallback_urls\":[],"
            << "\"file_type\":\"png\","
            << "\"format\":\"rawtherapee-tgmr-corpus-source-manifest-v1\","
            << "\"height\":7,\"icc_identity\":\"assumed-srgb\","
            << "\"landing_page\":\"https://example.invalid/" << id << "\","
            << "\"license\":\"CC0-1.0\","
            << "\"license_url\":\"https://creativecommons.org/publicdomain/zero/1.0/\","
            << "\"orientation\":1,\"original_url\":\"https://example.invalid/"
            << id << ".png\",\"patch_coordinates\":[],"
            << "\"patch_sampling_seed\":1,\"selected\":true,"
            << "\"selection_status\":\"accepted-corpus-v1\",\"sha256\":\""
            << std::string(64, decodedDigit) << "\",\"source_id\":\"" << id
            << "\",\"split\":\"" << split << "\",\"title\":\"fixture\","
            << "\"width\":7}\n";
        return output.str();
    };
    {
        std::ofstream output(path, std::ios::binary);
        output << line("source-a", "train", "author-a", '1', "0123456789abcdef")
               << line("source-b", "test", "author-b", '2', "0123456789abcdee");
    }
    bool rejected = false;
    try {
        (void)tgmr::readSourceManifest(path);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "C++ manifest parser accepted a cross-split perceptual near-duplicate");
    std::remove(path.c_str());
}

void testProductionSourceSelection()
{
    static const std::array<const char *, 4> catalogs{{
        "openimages-cvdf-v5-boxable", "pass-v3", "wikimedia-commons",
        "smithsonian-open-access",
    }};
    std::vector<tgmr::SourceRecord> candidates;
    candidates.reserve(20'000);
    for (std::size_t catalog = 0; catalog < catalogs.size(); ++catalog) {
        for (std::size_t index = 0; index < 5'000; ++index) {
            tgmr::SourceRecord record;
            record.manifestV2 = true;
            record.sourceId = std::string(catalogs[catalog]) + ":fixture-"
                + std::to_string(index);
            record.selected = false;
            record.splitAssigned = false;
            record.selectionStatus = "candidate-reviewed";
            if (catalog == 0 && index == 0) {
                record.selectionStatus = "candidate-pending-duplicate-review";
            }
            record.advertisedChecksum = "sha1:" + std::string(40, 'a');
            record.cacheFilename = "fixture-" + std::to_string(catalog) + '-'
                + std::to_string(index) + ".jpg";
            record.originalUrl = "https://example.invalid/" + record.cacheFilename;
            record.landingPage = "https://example.invalid/source/" + record.sourceId;
            record.author = "Fixture Author " + std::to_string(catalog) + '-'
                + std::to_string(index);
            record.authorId = "fixture-author:" + std::to_string(catalog) + ':'
                + std::to_string(index);
            record.authorUrl = "https://example.invalid/author/" + std::to_string(index);
            record.title = "Fixture source";
            record.license = catalog == 3 ? "CC0-1.0" : "CC-BY-4.0";
            record.licenseUrl = catalog == 3
                ? "https://creativecommons.org/publicdomain/zero/1.0/"
                : "https://creativecommons.org/licenses/by/4.0/";
            record.fileType = "jpeg";
            record.width = 1200;
            record.height = 800;
            record.orientation = 1;
            record.iccIdentity = "assumed-srgb";
            record.catalogName = catalogs[catalog];
            record.catalogRevision = "fixture-revision";
            record.catalogSnapshotSha256 = std::string(64, '1' + catalog);
            record.upstreamSourceId = "fixture-" + std::to_string(index);
            record.rightsEvidenceUrl = record.landingPage;
            record.rightsEvidenceRevision = "fixture-review";
            record.rightsEvidenceSha256 = std::string(64, '5' + catalog);
            record.rightsReviewStatus = "approved";
            const bool people = (catalog == 0 || catalog == 2) && index % 4 == 0;
            record.peopleReviewStatus = people
                ? "approved-no-minors-or-sensitive-content" : "not-applicable";
            if (people) record.contentTags = {"people", "skin-hair-clothing"};
            else record.contentTags = {index % 2 ? "foliage" : "architecture-brick"};
            if (catalog == 2 && index % 3 == 1) {
                record.contentTags.emplace_back("astronomy-star-field");
            }
            const auto identity = tgmr::sha256(
                record.sourceId.data(), record.sourceId.size());
            const std::string identityText = tgmr::hex(identity);
            record.sha256 = tgmr::hex(tgmr::sha256(
                (record.sourceId + ":bytes").data(), record.sourceId.size() + 6));
            const std::string pixelSeed = record.sourceId + ":pixels";
            record.decodedPixelSha256 = tgmr::hex(tgmr::sha256(
                pixelSeed.data(), pixelSeed.size()));
            record.perceptualHash = identityText.substr(0, 16);
            record.pHash = identityText.substr(16, 16);
            record.classification.perceptualHash = record.perceptualHash;
            record.classification.pHash = record.pHash;
            record.classification.decodedPixelSha256 = record.decodedPixelSha256;
            record.classification.channelMeans = {{0.2,0.3,0.4}};
            record.classification.luminanceMean = 0.05 + 0.9 * (index % 97) / 96.0;
            record.classification.luminanceStddev = 0.1;
            record.classification.luminanceP01 = 0.01;
            record.classification.luminanceP99 = 0.99;
            record.classification.chromaRatioMean = 0.01 + 0.3 * (index % 89) / 88.0;
            record.classification.hueDegrees = index % 360;
            record.classification.saturationMean = 0.2;
            record.classification.gradientRms = 0.001 + 0.2 * (index % 83) / 82.0;
            record.classification.laplacianRms = 0.02;
            record.classification.localContrast = 0.03;
            record.classification.jpegBlockiness = 1.0;
            record.classification.luminanceHistogram.fill(1);
            record.classification.hueHistogram.fill(1);
            record.classification.saturationHistogram.fill(1);
            record.patchSamplingSeed = index + 1;
            candidates.push_back(std::move(record));
        }
    }
    const std::string recipe = temporary("-selection.json");
    const std::string output = temporary("-selected.jsonl");
    const std::string outputSecond = temporary("-selected-second.jsonl");
    const std::string orderedOutput = temporary("-ordered.jsonl");
    const std::string orderedOutputSecond = temporary("-ordered-second.jsonl");
    const std::string orderManifest = temporary("-training-order.json");
    const std::string orderManifestSecond = temporary("-training-order-second.json");
    {
        std::ofstream stream(recipe, std::ios::binary);
        stream << "{\"author_image_cap\":5,"
            "\"content_tag_minimum_sources\":{"
            "\"astronomy-star-field\":{\"test\":1,\"train\":12,\"validation\":1}},"
            "\"format\":\"rawtherapee-tgmr-corpus-selection-v1\","
            "\"quotas\":{"
            "\"openimages-cvdf-v5-boxable\":{\"test\":400,\"train\":3200,\"validation\":400},"
            "\"pass-v3\":{\"test\":0,\"train\":0,\"validation\":0},"
            "\"smithsonian-open-access\":{\"test\":40,\"train\":320,\"validation\":40},"
            "\"wikimedia-commons\":{\"test\":60,\"train\":480,\"validation\":60}},"
            "\"seed\":\"rawtherapee-tgmr-corpus-v1-selection\"}\n";
    }
    const std::string report = tgmr::selectProductionSources(
        candidates, recipe, output);
    require(report.find("\"selected_sources\": 5000") != std::string::npos
        && report.find("\"deduplication_rejections\": [") != std::string::npos,
            "production selector did not fill the complete source population");
    const std::string reportSecond = tgmr::selectProductionSources(
        candidates, recipe, outputSecond);
    require(reportSecond == report
        && tgmr::sha256File(outputSecond) == tgmr::sha256File(output),
        "production source selection is not byte deterministic");
    const auto selected = tgmr::readSourceManifest(output);
    require(selected.size() == 5000,
            "production selector emitted the wrong number of records");
    std::array<std::uint64_t, 3> splits{};
    std::array<std::uint64_t, 3> people{};
    std::array<std::uint64_t, 3> astronomy{};
    std::map<std::string, std::size_t> authors;
    for (const auto &record : selected) {
        require(record.sourceId != "openimages-cvdf-v5-boxable:fixture-0",
                "production selector accepted unresolved duplicate review");
        require(record.selected && record.manifestV2
            && record.rightsReviewStatus == "approved",
            "selected source lost v2 provenance or rights approval");
        const std::size_t split = static_cast<unsigned>(record.split) - 1U;
        ++splits[split];
        ++authors[record.authorId];
        if (std::find(record.contentTags.begin(), record.contentTags.end(), "people")
            != record.contentTags.end()) ++people[split];
        if (std::find(record.contentTags.begin(), record.contentTags.end(),
                      "astronomy-star-field") != record.contentTags.end()) {
            ++astronomy[split];
        }
    }
    require(splits == std::array<std::uint64_t, 3>{{4000,500,500}}
        && people[0] >= 600 && people[1] >= 75 && people[2] >= 75,
        "production selector changed split or people quotas");
    require(astronomy[0] >= 12 && astronomy[1] >= 1 && astronomy[2] >= 1,
        "production selector did not preserve the astronomy star-field guardrail");
    require(std::all_of(authors.begin(), authors.end(), [](const auto &entry) {
        return entry.second <= 5;
    }), "production selector exceeded its author cap");

    tgmr::freezeProductionTrainingOrder(
        selected, output, orderedOutput, orderManifest);
    tgmr::freezeProductionTrainingOrder(
        selected, output, orderedOutputSecond, orderManifestSecond);
    require(read(orderedOutput) == read(orderedOutputSecond)
        && read(orderManifest) == read(orderManifestSecond),
        "production training order is not byte deterministic");
    const auto orderedSelected = tgmr::readSourceManifest(orderedOutput);
    const auto orderBytes = read(orderManifest);
    const std::string orderText(orderBytes.begin(), orderBytes.end());
    require(orderedSelected.size() == selected.size()
        && orderText.find("rawtherapee-tgmr-training-order-v1")
            != std::string::npos
        && orderText.find(tgmr::hex(tgmr::sha256File(output)))
            != std::string::npos,
        "training-order manifest omitted its contract or input binding");
    static const std::array<std::size_t, 5> milestoneSizes{{250,500,1000,2000,4000}};
    static const std::array<std::array<std::size_t, 3>, 5> milestoneCatalogs{{
        {{200,30,20}}, {{400,60,40}}, {{800,120,80}},
        {{1600,240,160}}, {{3200,480,320}},
    }};
    std::set<std::string> orderedIds;
    for (std::size_t index = 0; index < 4000; ++index) {
        require(orderedSelected[index].split == tgmr::CorpusSplit::TRAIN
            && orderedIds.insert(orderedSelected[index].sourceId).second,
            "training order is not a unique training-only prefix");
        const auto milestone = std::find(
            milestoneSizes.begin(), milestoneSizes.end(), index + 1);
        if (milestone != milestoneSizes.end()) {
            const std::size_t milestoneIndex = static_cast<std::size_t>(
                milestone - milestoneSizes.begin());
            std::array<std::size_t, 3> counts{};
            for (std::size_t source = 0; source <= index; ++source) {
                const std::string &catalog = orderedSelected[source].catalogName;
                if (catalog == "openimages-cvdf-v5-boxable") ++counts[0];
                else if (catalog == "wikimedia-commons") ++counts[1];
                else if (catalog == "smithsonian-open-access") ++counts[2];
            }
            require(counts == milestoneCatalogs[milestoneIndex],
                "training-order milestone changed its proportional catalog quota");
        }
    }
    const std::string sourceReport = tgmr::canonicalSourceReportJson(selected);
    require(sourceReport.find("rawtherapee-tgmr-source-statistics-v1")
            != std::string::npos
        && sourceReport.find("\"catalog_identities\"") != std::string::npos
        && tgmr::sourceReportCsv(selected).find("catalog_snapshot") != std::string::npos
        && tgmr::sourceReportHtml(selected).find("<!doctype html>") == 0,
        "source-corpus report formats changed");

    std::vector<tgmr::SourceRecord> finalized = orderedSelected;
    for (auto &record : finalized) {
        const std::size_t count = record.split == tgmr::CorpusSplit::TRAIN ? 256 : 128;
        record.patches.resize(count);
        for (std::size_t index = 0; index < count; ++index) {
            auto &patch = record.patches[index];
            patch.x = static_cast<std::uint32_t>(index);
            patch.y = 0;
            patch.coverage = index >= 3 * count / 4;
            patch.coverageClass = patch.coverage ? 1 : 0;
            patch.sequence = static_cast<std::uint16_t>(index);
            if (index % 4 != 0) {
                patch.augmentationKind = 1;
                patch.matrixId = record.split == tgmr::CorpusSplit::TRAIN ? 1 : 101;
            }
        }
    }
    tgmr::validateProductionManifest(finalized);
    const std::string attribution = tgmr::canonicalAttributionNotice(finalized);
    const std::string rightsReport = tgmr::canonicalRightsReportJson(finalized);
    const std::string reconstruction = tgmr::reconstructionListTsv(finalized);
    require(attribution.find("RawTherapee TGMR Corpus v1 attribution notice") == 0
        && attribution.find("Fixture Author") != std::string::npos,
        "canonical attribution notice omitted source attribution");
    require(rightsReport.find("rawtherapee-tgmr-rights-report-v1")
            != std::string::npos
        && rightsReport.find("\"total_sources\": 5000") != std::string::npos,
        "canonical rights report omitted its identity or complete source count");
    require(reconstruction.find(
            "source_id\tsource_sha256\tcache_filename\tkind\turl\ttransport_sha256"
            "\tarchive_member\tmember_sha256\n")
            == 0
        && reconstruction.find("\toriginal\thttps://example.invalid/")
            != std::string::npos,
        "canonical reconstruction list omitted its contract or source URL");
    require(attribution == tgmr::canonicalAttributionNotice(finalized)
        && rightsReport == tgmr::canonicalRightsReportJson(finalized)
        && reconstruction == tgmr::reconstructionListTsv(finalized),
        "corpus release-file generators are not deterministic");
    std::remove(recipe.c_str());
    std::remove(output.c_str());
    std::remove(outputSecond.c_str());
    std::remove(orderedOutput.c_str());
    std::remove(orderedOutputSecond.c_str());
    std::remove(orderManifest.c_str());
    std::remove(orderManifestSecond.c_str());
}

void testModelV2()
{
    tgmr::ModelV2Identity identity;
    identity.modelRevision = 7;
    identity.corpusSha256 = tgmr::sha256("corpus", 6);
    identity.trainerConfigurationSha256 = tgmr::sha256("config", 6);
    identity.attributionSha256 = tgmr::sha256("attribution", 11);
    identity.trainerRevisionSha256 = tgmr::sha256("revision", 8);
    const std::vector<std::uint8_t> payload = constantGrayValidationPayload();
    const auto first = tgmr::serializeModelV2(identity, payload);
    const auto second = tgmr::serializeModelV2(identity, payload);
    require(first == second, "TGMR v2 serialization is not deterministic");
    std::vector<std::uint8_t> extracted;
    const auto inspection = tgmr::inspectModelV2(first, &extracted);
    require(extracted == payload, "TGMR v2 payload changed");
    require(inspection.identity.modelRevision == 7
        && inspection.fileBytes == tgmr::TGMR_V2_PAYLOAD_OFFSET + payload.size(),
        "TGMR v2 identity or size changed");
    auto corrupt = first;
    corrupt.back() ^= 0x80;
    bool rejected = false;
    try {
        tgmr::inspectModelV2(corrupt);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "corrupt TGMR v2 payload was accepted");
    corrupt = first;
    corrupt[129] ^= 1;
    rejected = false;
    try {
        tgmr::inspectModelV2(corrupt);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "unauthenticated TGMR v2 metadata was accepted");
}

void testValidation()
{
    // These pairs are emitted by a real full validation run.  Their underlying
    // unrounded values are consistent, but independently rounding MSE and PSNR
    // to twelve decimal places creates a PSNR discrepancy far above 2e-9 dB.
    require(tgmr::canonicalMsePsnrConsistent(
                0.000502141202, 32.991741417391)
            && tgmr::canonicalMsePsnrConsistent(
                0.000016980741, 47.700433509911),
        "canonical validation rounding was rejected");
    require(!tgmr::canonicalMsePsnrConsistent(
                0.000502141202, 32.991841417391)
            && !tgmr::canonicalMsePsnrConsistent(0.0, 0.0),
        "materially inconsistent validation metrics were accepted");

    const std::string corpusPath = temporary("-validation.tgpc");
    const std::string modelPath = temporary("-validation.tgmr");
    tgmr::PatchRecord patch;
    patch.split = tgmr::CorpusSplit::VALIDATION;
    patch.sourceOrdinal = 7;
    patch.rgb.fill(16384);
    tgmr::writeCorpus(corpusPath, tgmr::sha256("manifest", 8),
                      tgmr::sha256("configuration", 13), {patch});
    tgmr::ModelV2Identity identity;
    identity.corpusSha256 = tgmr::inspectCorpus(corpusPath).header.payloadSha256;
    identity.trainerConfigurationSha256 = tgmr::sha256("config", 6);
    identity.attributionSha256 = tgmr::sha256("attribution", 11);
    identity.trainerRevisionSha256 = tgmr::sha256("revision", 8);
    const auto payload = constantGrayValidationPayload();
    require(payload.size() == 6073128, "synthetic validation payload size changed");
    tgmr::writeModelV2(modelPath, identity, payload);
    const auto report = tgmr::validateModelOnCorpus(
        modelPath, corpusPath, tgmr::CorpusSplit::VALIDATION);
    require(report.metrics.patches == 1 && report.metrics.scalarValues == 54,
            "TGMR validation did not exercise every phase/channel");
    require(report.metrics.sources.size() == 1
            && report.metrics.sources[0].sourceOrdinal == 7
            && report.metrics.sources[0].patches == 1,
            "TGMR validation did not preserve per-source metrics");
    require(report.metrics.strata[1][0].patches == 1
            && report.metrics.strata[0][1].patches == 1
            && report.metrics.strata[2][0].patches == 1,
            "TGMR validation did not preserve fixed signal strata");
    require(report.metrics.mse < 1e-14 && report.metrics.patchRmsP99 < 1e-7,
            "constant-gray TGMR validation fixture was not reconstructed exactly");
    const std::string validationJson = tgmr::canonicalValidationJson(report);
    require(validationJson.find("rawtherapee-tgmr-validation-report-v1")
            != std::string::npos
            && validationJson.find("\"phase_psnr_spread\"") != std::string::npos
            && validationJson.find("\"strata\"") != std::string::npos
            && validationJson.find("\"source_ordinal\": 7") != std::string::npos,
        "TGMR validation JSON contract changed");
    const std::string wrongModelPath = temporary("-wrong-corpus.tgmr");
    identity.corpusSha256 = tgmr::sha256("different-corpus", 16);
    tgmr::writeModelV2(wrongModelPath, identity, payload);
    bool rejected = false;
    try {
        tgmr::validateModelOnCorpus(
            wrongModelPath, corpusPath, tgmr::CorpusSplit::VALIDATION);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "TGMR validation accepted a corpus other than the model training corpus");
    std::remove(corpusPath.c_str());
    std::remove(modelPath.c_str());
    std::remove(wrongModelPath.c_str());
}

void testTraining()
{
    tgmr::FitConfiguration identityConfiguration;
    const std::string identityJson = tgmr::canonicalTrainingIdentityJson(
        identityConfiguration);
    const auto identityDigest = tgmr::fitConfigurationSha256(identityConfiguration);
    tgmr::FitConfiguration limitedIdentity = identityConfiguration;
    limitedIdentity.sourceLimit = 250;
    require(identityJson.find("rawtherapee-tgmr-training-identity-v1")
            != std::string::npos
        && identityJson.find(tgmr::hex(identityDigest)) != std::string::npos
        && tgmr::fitConfigurationSha256(limitedIdentity) != identityDigest
        && tgmr::trainerRevisionSha256()
            != std::array<std::uint8_t, 32>{},
        "canonical training configuration or trainer revision identity changed");

    const double matrix[] = {4.0, 2.0, 2.0, 3.0};
    double lower[4];
    require(tgmr::choleskyLower(matrix, lower, 2), "Cholesky factorization failed");
    require(std::abs(lower[0] - 2.0) < 1e-14
        && std::abs(lower[2] - 1.0) < 1e-14
        && std::abs(lower[3] - std::sqrt(2.0)) < 1e-14,
        "Cholesky factor is incorrect");

    std::vector<double> samples;
    for (int index = 0; index < 200; ++index) {
        const double cluster = index < 100 ? -1.0 : 1.0;
        const double x = cluster + 0.08 * ((index * 17) % 13 - 6);
        const double y = 0.5 * cluster + 0.05 * ((index * 23) % 11 - 5);
        samples.push_back(x);
        samples.push_back(y);
    }
    tgmr::FitConfiguration configuration;
    configuration.components = 2;
    configuration.gaussianIterations = 3;
    configuration.studentIterations = 4;
    configuration.covarianceFloor = 1e-5;
    configuration.backend = tgmr::TrainingBackend::CANONICAL;
    const auto first = tgmr::fitStudentTMixture(samples, 200, 2, configuration);
    const auto second = tgmr::fitStudentTMixture(samples, 200, 2, configuration);
    require(first.weights == second.weights && first.means == second.means
        && first.scales == second.scales,
        "canonical Student-t fitting is not deterministic");
    require(first.weights.size() == 2 && std::abs(first.weights[0] - 0.5) < 0.05
        && std::abs(first.weights[1] - 0.5) < 0.05,
        "Student-t fitting did not recover cluster populations");
    require((first.means[0] > 0.5 && first.means[2] < -0.5)
        || (first.means[2] > 0.5 && first.means[0] < -0.5),
        "Student-t fitting did not retain both cluster centers");
    require(std::isfinite(first.meanLogLikelihood),
        "Student-t fitting produced a non-finite likelihood");

    tgmr::FitConfiguration partialConfiguration = configuration;
    partialConfiguration.studentIterations = 2;
    auto resumed = tgmr::fitStudentTMixture(
        samples, 200, 2, partialConfiguration);
    resumed = tgmr::continueStudentTMixture(
        samples, 200, configuration, std::move(resumed), 0, 2);
    require(resumed.weights == first.weights && resumed.means == first.means
        && resumed.scales == first.scales
        && resumed.meanLogLikelihood == first.meanLogLikelihood,
        "canonical checkpoint-style continuation differs from uninterrupted fitting");

    configuration.backend = tgmr::TrainingBackend::CPU;
    const auto parallel = tgmr::fitStudentTMixture(samples, 200, 2, configuration);
    require(std::abs(parallel.meanLogLikelihood - first.meanLogLikelihood) < 1e-10,
        "CPU and canonical fitting diverged on the synthetic mixture");

    tgmr::FitConfiguration one;
    one.components = 1;
    one.gaussianIterations = 1;
    one.studentIterations = 0;
    one.covarianceFloor = 1e-6;
    one.backend = tgmr::TrainingBackend::CANONICAL;
    const std::vector<double> gaussianSamples{1.0, 2.0, 3.0, 4.0, 5.0, 8.0};
    one.batchSize = 1;
    const auto oneAtATime = tgmr::fitStudentTMixture(gaussianSamples, 3, 2, one);
    one.batchSize = 4096;
    const auto oneBatch = tgmr::fitStudentTMixture(gaussianSamples, 3, 2, one);
    require(oneAtATime.weights == oneBatch.weights
        && oneAtATime.means == oneBatch.means
        && oneAtATime.scales == oneBatch.scales,
        "canonical fitting changed with batch size");
    require(std::abs(oneBatch.means[0] - 3.0) < 1e-14
        && std::abs(oneBatch.means[1] - 14.0 / 3.0) < 1e-14,
        "K=1 Gaussian conditional mean parity failed");
    require(std::abs(oneBatch.scales[0] - (8.0 / 3.0 + 1e-6)) < 1e-12
        && std::abs(oneBatch.scales[1] - 4.0) < 1e-12
        && std::abs(oneBatch.scales[2] - 4.0) < 1e-12
        && std::abs(oneBatch.scales[3] - (56.0 / 9.0 + 1e-6)) < 1e-12,
        "K=1 Gaussian covariance parity failed");

    // Frozen independent NumPy evaluation of one fixed-nu Student-t ECM step:
    // q=(x-mu)' Sigma^-1 (x-mu), u=(nu+d)/(nu+q), followed by the
    // weighted mean and scale updates. This checks the robust latent-weight
    // equation separately from C++ self-consistency and OpenMP parity.
    one.gaussianIterations = 0;
    one.studentIterations = 1;
    const auto oneStudent = tgmr::fitStudentTMixture(
        gaussianSamples, 3, 2, one);
    require(std::abs(oneStudent.means[0] - 2.999999100013366) < 1e-12
        && std::abs(oneStudent.means[1] - 4.666664366696863) < 1e-12,
        "one-step Student-t mean differs from the independent reference");
    const std::array<double, 4> expectedStudentScale{{
        2.6666718666124924, 4.000005999921439,
        4.000005999921439, 6.22223275543155,
    }};
    for (std::size_t value = 0; value < expectedStudentScale.size(); ++value) {
        require(std::abs(oneStudent.scales[value] - expectedStudentScale[value]) < 1e-11,
            "one-step Student-t covariance differs from the independent reference");
    }
}

void testXTransTrainingContract()
{
    const auto phases = tgmr::xtransPhaseContracts();
    std::set<std::array<std::uint32_t, 49>> patterns;
    for (std::size_t phase = 0; phase < phases.size(); ++phase) {
        const auto &contract = phases[phase];
        require(contract.index == phase, "X-Trans phase order changed");
        require(patterns.insert(contract.observedIndices).second,
                "X-Trans phase contract is duplicated");
        require(contract.observedIndices[24] / 49 == contract.sampledCenterChannel,
                "X-Trans center channel is inconsistent");
        require(contract.targetChannels[0] < contract.targetChannels[1]
            && contract.targetChannels[0] != contract.sampledCenterChannel
            && contract.targetChannels[1] != contract.sampledCenterChannel,
            "X-Trans target channels are inconsistent");
    }

    tgmr::PhaseCheckpoint source;
    source.phase = phases[3];
    source.sampleCount = 200;
    source.configuration.components = 2;
    source.configuration.covarianceFloor = 1e-6;
    source.configuration.degreesOfFreedom = 3.0;
    source.configuration.seed = 123;
    source.configuration.device = 2;
    source.configuration.gpuYieldMilliseconds = 7;
    source.configuration.batchSize = 1234;
    source.configuration.maximumBytes = 12345678;
    source.configuration.sourceLimit = 250;
    source.model.dimension = 51;
    source.model.components = 2;
    source.model.gaussianIterations = 4;
    source.model.studentIterations = 7;
    source.model.meanLogLikelihood = -12.5;
    source.model.weights = {0.6, 0.4};
    source.model.means.resize(2 * 51);
    source.model.scales.assign(2 * 51 * 51, 0.0);
    source.model.effectiveCounts = {120, 80};
    source.corpusPayloadSha256 = tgmr::sha256("tgpc", 4);
    for (std::size_t component = 0; component < 2; ++component) {
        for (std::size_t row = 0; row < 51; ++row) {
            source.model.means[component * 51 + row] = 0.001 * (component * 51 + row);
            source.model.scales[(component * 51 + row) * 51 + row] = 0.01 + 0.001 * component;
        }
    }
    const std::string path = temporary(".tgmrc");
    tgmr::writePhaseCheckpoint(path, source);
    const auto loaded = tgmr::readPhaseCheckpoint(path);
    require(loaded.phase.index == source.phase.index
        && loaded.sampleCount == source.sampleCount
        && loaded.model.weights == source.model.weights
        && loaded.model.means == source.model.means
        && loaded.model.scales == source.model.scales
        && loaded.configuration.device == source.configuration.device
        && loaded.configuration.gpuYieldMilliseconds
            == source.configuration.gpuYieldMilliseconds
        && loaded.configuration.batchSize == source.configuration.batchSize
        && loaded.configuration.maximumBytes == source.configuration.maximumBytes
        && loaded.configuration.sourceLimit == source.configuration.sourceLimit,
        "phase checkpoint round trip changed model state");
    std::remove(path.c_str());

    std::array<tgmr::PhaseCheckpoint, 18> exportModels;
    for (std::size_t phase = 0; phase < exportModels.size(); ++phase) {
        auto &checkpoint = exportModels[phase];
        checkpoint.phase = phases[phase];
        checkpoint.sampleCount = 1000;
        checkpoint.configuration.components = 32;
        checkpoint.configuration.degreesOfFreedom = 3.0;
        checkpoint.model.dimension = 51;
        checkpoint.model.components = 32;
        checkpoint.model.weights.assign(32, 1.0 / 32.0);
        checkpoint.model.means.assign(32 * 51, 0.0);
        checkpoint.model.scales.assign(32 * 51 * 51, 0.0);
        checkpoint.model.effectiveCounts.assign(32, 31.25);
        for (std::size_t component = 0; component < 32; ++component) {
            for (std::size_t row = 0; row < 51; ++row) {
                checkpoint.model.scales[(component * 51 + row) * 51 + row]
                    = 0.01 + component * 1e-5;
            }
        }
    }
    const auto payload = tgmr::exportPhasePayload(exportModels);
    require(payload.size() == 6073164 - 36,
            "exported K32/S9/q8 phase payload size changed");

    exportModels[7].configuration.sourceLimit = 500;
    bool rejectedMixedTraining = false;
    try {
        (void)tgmr::exportPhasePayload(exportModels);
    } catch (const std::runtime_error &) {
        rejectedMixedTraining = true;
    }
    require(rejectedMixedTraining,
            "export accepted phase checkpoints with different training settings");
    exportModels[7].configuration.sourceLimit = 0;

    exportModels[7].configuration.batchSize += 1;
    rejectedMixedTraining = false;
    try {
        (void)tgmr::exportPhasePayload(exportModels);
    } catch (const std::runtime_error &) {
        rejectedMixedTraining = true;
    }
    require(rejectedMixedTraining,
            "export accepted phase checkpoints with different fit identities");
    exportModels[7].configuration.batchSize -= 1;

    exportModels[11].model.means[0] =
        std::numeric_limits<double>::quiet_NaN();
    bool rejectedNonfinite = false;
    try {
        (void)tgmr::exportPhasePayload(exportModels);
    } catch (const std::runtime_error &) {
        rejectedNonfinite = true;
    }
    require(rejectedNonfinite,
            "export accepted a non-finite phase checkpoint");
}

} // namespace

int main()
{
    try {
        testSha256();
        testCameraMatrices();
        testCorpus();
        testResumableCorpusWriter();
        testSourceLimitedTrainingMatrix();
        testImageManifestAndPack();
        testResumableSourceFinalization();
        testTiff16Orientation();
        testGrayJpegIccProfile();
        testCmykJpegIccProfile();
        testParallelClassificationAndProxy();
        testManifestNearDuplicateLeakage();
        testProductionSourceSelection();
        testModelV2();
        testValidation();
        testTraining();
        testXTransTrainingContract();
        std::cout << "TGMR tooling tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "TGMR tooling test failure: " << error.what() << '\n';
        return 1;
    }
}
