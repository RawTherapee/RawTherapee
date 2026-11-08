// Standalone feasibility checks; no RawTherapee export implementation lives here.
#include <libheif/heif.h>
#include <lcms2.h>
#include <glib.h>
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
#include <glib/gwin32.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int width = 1025;
constexpr int height = 257;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void check(heif_error error, const char* operation)
{
    if (error.code != heif_error_Ok) {
        throw std::runtime_error(std::string(operation) + ": " +
            (error.message ? error.message : "unknown libheif error") +
            " (" + std::to_string(error.code) + "/" +
            std::to_string(error.subcode) + ")");
    }
}

template<class T, auto release>
using Owner = std::unique_ptr<T, decltype(release)>;
using Context = Owner<heif_context, heif_context_free>;
using Image = Owner<heif_image, heif_image_release>;
using Handle = Owner<heif_image_handle, heif_image_handle_release>;
using Encoder = Owner<heif_encoder, heif_encoder_release>;
using Nclx = Owner<heif_color_profile_nclx, heif_nclx_color_profile_free>;
using DecodingOptions = Owner<heif_decoding_options, heif_decoding_options_free>;

DecodingOptions decoding_options()
{
    DecodingOptions options(heif_decoding_options_alloc(), heif_decoding_options_free);
    require(bool(options), "cannot allocate decoding options");
    options->strict_decoding = 1;
    options->num_codec_threads = 1;
    options->decoder_id = "libde265";
    options->output_image_nclx_profile_passthrough = 1;
    return options;
}

struct FileCloser {
    void operator()(FILE* file) const { fclose(file); }
};
using File = std::unique_ptr<FILE, FileCloser>;

Context context()
{
    Context ctx(heif_context_alloc(), heif_context_free);
    require(bool(ctx), "cannot allocate context");
    // No tile worker pool; decoding options separately bound codec workers.
    heif_context_set_max_decoding_threads(ctx.get(), 0);
    require(heif_context_get_security_limits(ctx.get())->max_image_size_pixels != 0,
        "input security limits must remain enabled");
    return ctx;
}

void write_bytes(const std::filesystem::path& path, const void* data, size_t size)
{
    File file(g_fopen(path.u8string().c_str(), "wb"));
    require(bool(file), "cannot open " + path.u8string());
    require(fwrite(data, 1, size, file.get()) == size, "cannot write " + path.u8string());
    require(fclose(file.release()) == 0, "cannot close " + path.u8string());
}

heif_error write_callback(heif_context*, const void* data, size_t size, void* user)
{
    if (fwrite(data, 1, size, static_cast<FILE*>(user)) != size) {
        return {heif_error_Encoding_error, heif_suberror_Unspecified, "file write failed"};
    }
    return {heif_error_Ok, heif_suberror_Unspecified, nullptr};
}

void write_context(heif_context* ctx, const std::filesystem::path& path)
{
    File file(g_fopen(path.u8string().c_str(), "wb"));
    require(bool(file), "cannot open " + path.u8string());
    heif_writer writer{1, write_callback};
    check(heif_context_write(ctx, &writer, file.get()), "write container");
    require(fclose(file.release()) == 0, "cannot close container");
}

std::vector<uint8_t> read_bytes(const std::filesystem::path& path)
{
    gchar* contents = nullptr;
    gsize size = 0;
    GError* error = nullptr;
    if (!g_file_get_contents(path.u8string().c_str(), &contents, &size, &error)) {
        const std::string message = error ? error->message : "cannot read input";
        g_clear_error(&error);
        throw std::runtime_error(message);
    }
    std::unique_ptr<gchar, decltype(&g_free)> owner(contents, g_free);
    return {reinterpret_cast<uint8_t*>(contents), reinterpret_cast<uint8_t*>(contents) + size};
}

void roundtrip(const std::filesystem::path& directory, int bps, const char* chroma,
    bool dual, bool wide, bool verify_write_failures = false);

void failure_checks(const std::filesystem::path& input, const std::filesystem::path& directory)
{
    const auto bytes = read_bytes(input);
    require(bytes.size() > 12, "failure checks need a complete HEIF fixture");
    auto valid = context();
    check(heif_context_read_from_memory_without_copy(valid.get(), bytes.data(), bytes.size(), nullptr),
        "read failure-check fixture");

    // A truncated ftyp box must be classified as invalid input/file type,
    // independently of whether an HEVC decoder plugin is installed.
    auto malformed = context();
    const auto input_error = heif_context_read_from_memory_without_copy(
        malformed.get(), bytes.data(), 12, nullptr);
    require(input_error.code == heif_error_Invalid_input ||
        input_error.code == heif_error_Unsupported_filetype,
        "truncated container did not return an input error");
    std::cout << "failure\tmalformed-input\t" << input_error.code << '\t'
        << input_error.subcode << '\n';

    // Tighten only this context. A real decode must enforce its pixel budget;
    // the default/global limits must remain enabled and unchanged afterward.
    const auto default_limit = heif_get_global_security_limits()->max_image_size_pixels;
    auto limited = context();
    heif_context_get_security_limits(limited.get())->max_image_size_pixels = 256;
    auto limit_error = heif_context_read_from_memory_without_copy(
        limited.get(), bytes.data(), bytes.size(), nullptr);
    if (limit_error.code == heif_error_Ok) {
        heif_image_handle* raw_handle = nullptr;
        check(heif_context_get_primary_image_handle(limited.get(), &raw_handle),
            "get image for limit check");
        Handle handle(raw_handle, heif_image_handle_release);
        auto options = decoding_options();
        heif_image* raw_image = nullptr;
        limit_error = heif_decode_image(handle.get(), &raw_image, heif_colorspace_RGB,
            heif_chroma_444, options.get());
        Image image(raw_image, heif_image_release);
    }
    require(limit_error.code == heif_error_Memory_allocation_error &&
        limit_error.subcode == heif_suberror_Security_limit_exceeded,
        "input decode did not enforce its context image-size limit");
    auto fresh = context();
    require(heif_get_global_security_limits()->max_image_size_pixels == default_limit &&
        heif_context_get_security_limits(fresh.get())->max_image_size_pixels == default_limit,
        "context limit test changed global/default limits");
    std::cout << "failure\tsecurity-limit\t" << limit_error.code << '\t'
        << limit_error.subcode << '\n';

    // libheif deliberately cannot serialize a context read from a file.
    // Exercise write failures on a newly encoded context, as export will do.
    roundtrip(directory, 8, "444", false, false, true);
}

void writer_failure_checks(heif_context* encoded, const std::filesystem::path& input,
    const std::filesystem::path& directory)
{
    const auto bytes = read_bytes(input);
    // Exercise the real stdio callback with an unwritable FILE, rather than
    // injecting a successful writer or testing a fabricated error directly.
    File readonly(g_fopen(input.u8string().c_str(), "rb"));
    require(bool(readonly), "cannot open read-only failure-check fixture");
    heif_writer writer{1, write_callback};
    const auto write_error = heif_context_write(encoded, &writer, readonly.get());
    require(write_error.code == heif_error_Encoding_error && write_error.message &&
        std::string(write_error.message) == "file write failed",
        "container writer lost the file-write failure");
    require(ferror(readonly.get()) != 0, "read-only stream did not report a write failure");
    require(read_bytes(input) == bytes, "failure check modified its input fixture");
    std::cout << "failure\tfile-write\t" << write_error.code << '\t'
        << write_error.subcode << '\n';

    std::filesystem::create_directories(directory);
    bool open_failed = false;
    try {
        write_context(encoded, directory);
    } catch (const std::runtime_error& error) {
        open_failed = std::string(error.what()).find("cannot open ") == 0;
    }
    require(open_failed, "writing to a directory did not preserve the file-open error");
    std::cout << "failure\tfile-open\tcannot open output directory\n";
}

std::vector<uint8_t> profile(bool wide)
{
    cmsHPROFILE raw = nullptr;
    if (wide) {
        const cmsCIExyY white{0.3127, 0.3290, 1.0};
        // Deliberately custom: do not use Display P3 or another CICP gamut.
        const cmsCIExyYTRIPLE primaries{{0.670, 0.320, 1.0},
            {0.260, 0.700, 1.0}, {0.145, 0.060, 1.0}};
        cmsToneCurve* gamma = cmsBuildGamma(nullptr, 2.25);
        require(gamma != nullptr, "cannot create test transfer curve");
        cmsToneCurve* curves[]{gamma, gamma, gamma};
        raw = cmsCreateRGBProfile(&white, &primaries, curves);
        cmsFreeToneCurve(gamma);
    } else {
        raw = cmsCreate_sRGBProfile();
    }
    std::unique_ptr<void, decltype(&cmsCloseProfile)> owner(raw, cmsCloseProfile);
    require(raw != nullptr, "cannot create ICC profile");
    cmsUInt32Number size = 0;
    require(cmsSaveProfileToMem(raw, nullptr, &size), "cannot size ICC profile");
    std::vector<uint8_t> bytes(size);
    require(cmsSaveProfileToMem(raw, bytes.data(), &size), "cannot serialize ICC profile");
    // Fixed creation date and absent optional profile ID make fixtures reproducible.
    const uint8_t date[]{0x07, 0xe8, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
    std::copy(std::begin(date), std::end(date), bytes.begin() + 24);
    std::fill(bytes.begin() + 84, bytes.begin() + 100, 0);
    return bytes;
}

double sample(int x, int y, int channel)
{
    const double ramp = double(x) / (width - 1);
    if (y < 128) {
        return ramp;
    }
    const std::array<std::array<double, 3>, 8> patches{{
        {{0.05, 0.05, 0.05}}, {{0.95, 0.95, 0.95}}, {{0.85, 0.10, 0.15}},
        {{0.10, 0.80, 0.20}}, {{0.10, 0.15, 0.85}}, {{0.80, 0.65, 0.15}},
        {{0.15, 0.70, 0.80}}, {{0.70, 0.15, 0.75}}
    }};
    return patches[std::min(7, x * 8 / width)][channel];
}

void set_sample(uint8_t* row, int x, int bps, uint16_t value)
{
    if (bps == 8) {
        row[x] = static_cast<uint8_t>(value);
    } else {
        std::memcpy(row + x * 2, &value, sizeof(value));
    }
}

uint16_t get_sample(const uint8_t* row, int x, int bps)
{
    if (bps == 8) {
        return row[x];
    }
    uint16_t value = 0;
    std::memcpy(&value, row + x * 2, sizeof(value));
    return value;
}

const heif_encoder_descriptor* x265_descriptor()
{
    const heif_encoder_descriptor* descriptor = nullptr;
    return heif_get_encoder_descriptors(heif_compression_HEVC, "x265", &descriptor, 1)
        == 1 ? descriptor : nullptr;
}

bool libde265_available()
{
    const int count = heif_get_decoder_descriptors(heif_compression_HEVC, nullptr, 0);
    std::vector<const heif_decoder_descriptor*> descriptors(count);
    heif_get_decoder_descriptors(heif_compression_HEVC, descriptors.data(), count);
    return std::any_of(descriptors.begin(), descriptors.end(), [](const auto* descriptor) {
        return std::strcmp(heif_decoder_descriptor_get_id_name(descriptor), "libde265") == 0;
    });
}

void capabilities()
{
    std::cout << "libheif\t" << heif_get_version() << '\n'
        << "hevc_encoder\t" << heif_have_encoder_for_format(heif_compression_HEVC) << '\n'
        << "hevc_decoder\t" << heif_have_decoder_for_format(heif_compression_HEVC) << '\n'
        << "selected_x265_available\t" << bool(x265_descriptor()) << '\n'
        << "selected_libde265_available\t" << libde265_available() << '\n'
        << "tile_worker_threads\t0\nx265_pool\tnone\nx265_frame_threads\t1\n";
    const int enc_count = heif_get_encoder_descriptors(heif_compression_HEVC, nullptr, nullptr, 0);
    std::vector<const heif_encoder_descriptor*> encoders(enc_count);
    heif_get_encoder_descriptors(heif_compression_HEVC, nullptr, encoders.data(), enc_count);
    for (const auto* descriptor : encoders) {
        std::cout << "encoder\t" << heif_encoder_descriptor_get_id_name(descriptor)
            << '\t' << heif_encoder_descriptor_get_name(descriptor) << '\n';
    }
    const int dec_count = heif_get_decoder_descriptors(heif_compression_HEVC, nullptr, 0);
    std::vector<const heif_decoder_descriptor*> decoders(dec_count);
    heif_get_decoder_descriptors(heif_compression_HEVC, decoders.data(), dec_count);
    for (const auto* descriptor : decoders) {
        std::cout << "decoder\t" << heif_decoder_descriptor_get_id_name(descriptor)
            << '\t' << heif_decoder_descriptor_get_name(descriptor) << '\n';
    }
}

void roundtrip(const std::filesystem::path& directory, int bps, const char* chroma,
    bool dual, bool wide, bool verify_write_failures)
{
    std::filesystem::create_directories(directory);
    const std::string name = std::to_string(bps) + "-" + chroma + "-" +
        (wide ? "wide-icc" : dual ? "icc-nclx" : "icc-only");
    const auto path = directory / (name + ".heic");
    const auto icc = profile(wide);
    write_bytes(directory / (name + ".icc"), icc.data(), icc.size());
    auto ctx = context();
    heif_encoder* raw_encoder = nullptr;
    check(heif_context_get_encoder(ctx.get(), x265_descriptor(), &raw_encoder), "get x265 encoder");
    Encoder encoder(raw_encoder, heif_encoder_release);
    check(heif_encoder_set_lossy_quality(encoder.get(), 100), "set quality");
    check(heif_encoder_set_parameter_string(encoder.get(), "chroma", chroma), "set chroma");
    check(heif_encoder_set_parameter_string(encoder.get(), "preset", "fast"), "set preset");
    check(heif_encoder_set_parameter(encoder.get(), "x265:pools", "none"), "limit worker pools");
    check(heif_encoder_set_parameter(encoder.get(), "x265:frame-threads", "1"), "limit frame threads");
    check(heif_encoder_set_parameter(encoder.get(), "x265:wpp", "0"), "disable wavefront workers");
    check(heif_encoder_set_logging_level(encoder.get(), 0), "disable encoder logging");

    heif_image* raw_image = nullptr;
    check(heif_image_create(width, height, heif_colorspace_RGB, heif_chroma_444, &raw_image),
        "create RGB image");
    Image image(raw_image, heif_image_release);
    const std::array<heif_channel, 3> channels{heif_channel_R, heif_channel_G, heif_channel_B};
    const int maximum = (1 << bps) - 1;
    for (size_t channel = 0; channel < channels.size(); ++channel) {
        check(heif_image_add_plane(image.get(), channels[channel], width, height, bps), "allocate plane");
        int stride = 0;
        uint8_t* plane = heif_image_get_plane(image.get(), channels[channel], &stride);
        require(plane != nullptr && stride >= width * (bps == 8 ? 1 : 2), "invalid RGB plane");
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                set_sample(plane + y * stride, x, bps,
                    static_cast<uint16_t>(std::lround(sample(x, y, channel) * maximum)));
            }
        }
    }
    check(heif_image_set_raw_color_profile(image.get(), "prof", icc.data(), icc.size()), "attach ICC");
    Nclx nclx(heif_nclx_color_profile_alloc(), heif_nclx_color_profile_free);
    require(bool(nclx), "cannot allocate NCLX");
    // The wide ICC deliberately has no equivalent NCLX: leave primaries and
    // transfer unspecified while retaining explicit YCbCr conversion semantics.
    check(heif_nclx_color_profile_set_color_primaries(nclx.get(), wide ? 2 : 1), "set primaries");
    check(heif_nclx_color_profile_set_transfer_characteristics(nclx.get(), wide ? 2 : 13), "set transfer");
    check(heif_nclx_color_profile_set_matrix_coefficients(nclx.get(), 1), "set matrix");
    nclx->full_range_flag = 1;
    // libheif's conversion defaults can replace unspecified primaries/transfer
    // with sRGB. Override the HEVC VUI explicitly for an ICC without CICP mapping.
    check(heif_encoder_set_parameter(encoder.get(), "x265:colorprim", wide ? "2" : "1"),
        "set bitstream primaries");
    check(heif_encoder_set_parameter(encoder.get(), "x265:transfer", wide ? "2" : "13"),
        "set bitstream transfer");
    check(heif_encoder_set_parameter(encoder.get(), "x265:colormatrix", "1"),
        "set bitstream matrix");
    check(heif_encoder_set_parameter(encoder.get(), "x265:range", "full"),
        "set bitstream range");
    check(heif_image_set_nclx_color_profile(image.get(), nclx.get()), "attach conversion NCLX");
    Owner<heif_encoding_options, heif_encoding_options_free> options(
        heif_encoding_options_alloc(), heif_encoding_options_free);
    require(bool(options), "cannot allocate encoding options");
    options->output_nclx_profile = nclx.get();
    options->save_two_colr_boxes_when_ICC_and_nclx_available = dual;
    options->macOS_compatibility_workaround_no_nclx_profile = !dual;
    heif_image_handle* raw_handle = nullptr;
    check(heif_context_encode_image(ctx.get(), image.get(), encoder.get(), options.get(), &raw_handle),
        "encode actual image");
    Handle handle(raw_handle, heif_image_handle_release);
    write_context(ctx.get(), path);
    if (verify_write_failures) {
        writer_failure_checks(ctx.get(), path, directory / "unwritable-output-directory");
    }

    const auto bytes = read_bytes(path);
    auto read_ctx = context();
    check(heif_context_read_from_memory_without_copy(read_ctx.get(), bytes.data(), bytes.size(), nullptr),
        "read exported image");
    heif_image_handle* raw_read_handle = nullptr;
    check(heif_context_get_primary_image_handle(read_ctx.get(), &raw_read_handle), "get primary image");
    Handle read_handle(raw_read_handle, heif_image_handle_release);
    require(heif_image_handle_get_width(read_handle.get()) == width &&
        heif_image_handle_get_height(read_handle.get()) == height, "odd dimensions changed");
    std::vector<uint8_t> read_icc(heif_image_handle_get_raw_color_profile_size(read_handle.get()));
    check(heif_image_handle_get_raw_color_profile(read_handle.get(), read_icc.data()), "read ICC");
    require(read_icc == icc, "ICC bytes changed");
    require(heif_image_handle_get_number_of_metadata_blocks(read_handle.get(), nullptr) == 0,
        "unexpected EXIF/XMP metadata in export-only fixture");
    heif_colorspace coded_colorspace;
    heif_chroma coded_chroma;
    check(heif_image_handle_get_preferred_decoding_colorspace(read_handle.get(),
        &coded_colorspace, &coded_chroma), "read coded colorspace");
    const heif_chroma expected_chroma = std::string(chroma) == "420" ? heif_chroma_420 :
        std::string(chroma) == "422" ? heif_chroma_422 : heif_chroma_444;
    require(coded_colorspace == heif_colorspace_YCbCr && coded_chroma == expected_chroma,
        "encoder silently changed requested chroma");
    require(heif_image_handle_get_luma_bits_per_pixel(read_handle.get()) == bps &&
        heif_image_handle_get_chroma_bits_per_pixel(read_handle.get()) == bps,
        "encoder silently changed requested precision");
    heif_color_profile_nclx* raw_read_nclx = nullptr;
    const auto nclx_error = heif_image_handle_get_nclx_color_profile(read_handle.get(), &raw_read_nclx);
    Nclx read_nclx(raw_read_nclx, heif_nclx_color_profile_free);
    if (dual) {
        check(nclx_error, "read NCLX");
        require(read_nclx->color_primaries == nclx->color_primaries &&
            read_nclx->transfer_characteristics == nclx->transfer_characteristics &&
            read_nclx->matrix_coefficients == nclx->matrix_coefficients &&
            read_nclx->full_range_flag == nclx->full_range_flag, "NCLX fields changed");
    } else {
        require(nclx_error.code == heif_error_Color_profile_does_not_exist,
            "ICC-only output unexpectedly has a container NCLX profile");
    }

    auto decoding = decoding_options();
    heif_image* raw_decoded = nullptr;
    check(heif_decode_image(read_handle.get(), &raw_decoded, heif_colorspace_RGB,
        heif_chroma_444, decoding.get()), "decode exported image");
    Image decoded(raw_decoded, heif_image_release);
    double sum = 0;
    double max_error = 0;
    double interior_error = 0;
    std::array<int, 3> worst{};
    std::set<uint16_t> ramp_values;
    for (size_t channel = 0; channel < channels.size(); ++channel) {
        const int decoded_bps = heif_image_get_bits_per_pixel_range(decoded.get(), channels[channel]);
        require(decoded_bps == bps, "decoded precision changed");
        int stride = 0;
        const uint8_t* plane = heif_image_get_plane_readonly(decoded.get(), channels[channel], &stride);
        require(plane != nullptr && stride >= width * (bps == 8 ? 1 : 2), "invalid decoded plane");
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto value = get_sample(plane + y * stride, x, bps);
                require(value <= maximum, "decoded sample outside selected precision");
                const double difference = std::abs(double(value) / maximum - sample(x, y, channel));
                sum += difference;
                if (difference > max_error) {
                    max_error = difference;
                    worst = {x, y, int(channel)};
                }
                // Subsampled chroma can bleed across sharp patch boundaries.
                // Test conversion accuracy away from those edges separately.
                if (x > 8 && x < width - 9 && y > 8 && y < height - 9 &&
                    std::abs(y - 128) > 8 &&
                    (y < 128 || (x * 8 % width > 72 && x * 8 % width < width - 72))) {
                    interior_error = std::max(interior_error, difference);
                }
                if (channel == 0 && y == 64) {
                    ramp_values.insert(value);
                }
            }
        }
    }
    const double mean_error = sum / (width * height * 3);
    require(mean_error < 0.035, "mean RGB error exceeds predeclared 3.5 percent bound");
    require(interior_error < 0.035, "interior RGB error exceeds predeclared 3.5 percent bound");
    require(bps == 8 || ramp_values.size() > 256, "higher precision collapsed to 8-bit");
    std::cout << "case\t" << name << '\t' << width << '\t' << height << '\t' << bps << '\t'
        << chroma << '\t' << icc.size() << '\t' << dual << '\t'
        << std::setprecision(9) << mean_error << '\t' << max_error << '\t' << ramp_values.size()
        << '\t' << interior_error << '\t' << worst[0] << ',' << worst[1] << ',' << worst[2] << '\n';
}

int run(int argc, char** argv)
{
    if (argc == 4 && std::string(argv[1]) == "--failure-checks") {
        failure_checks(std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]));
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--capabilities") {
        capabilities();
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--expect-codecs") {
        capabilities();
        require(heif_have_encoder_for_format(heif_compression_HEVC) == std::stoi(argv[2]),
            "unexpected encoder availability");
        require(heif_have_decoder_for_format(heif_compression_HEVC) == std::stoi(argv[3]),
            "unexpected decoder availability");
        return 0;
    }
    if (argc != 3 || std::string(argv[1]) != "--roundtrip") {
        std::cerr << "Usage: heif-feasibility --capabilities | --expect-codecs <encoder> <decoder>"
            " | --roundtrip <artifact-directory>"
            " | --failure-checks <valid-fixture> <output-directory>\n";
        return 1;
    }
    if (!x265_descriptor()) {
        std::cerr << "HEVC encoder unavailable: install or bundle the x265 codec\n";
        return 2;
    }
    if (!libde265_available()) {
        std::cerr << "HEVC decoder unavailable: install or bundle the libde265 codec\n";
        return 3;
    }
    const auto directory = std::filesystem::u8path(argv[2]);
    std::filesystem::create_directories(directory);
    capabilities();
    for (const int bps : {8, 10, 12}) {
        for (const auto* chroma : {"420", "422", "444"}) {
            roundtrip(directory, bps, chroma, false, false);
            roundtrip(directory, bps, chroma, true, false);
        }
    }
    roundtrip(directory, 12, "444", false, true);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    int result = 1;
    try {
        check(heif_init(nullptr), "initialize libheif");
        struct Lifetime {
            ~Lifetime() { heif_deinit(); }
        } lifetime;
#ifdef G_OS_WIN32
        // Windows narrow argv need not be UTF-8; GLib's conversion is explicit.
        (void)argc;
        (void)argv;
        std::unique_ptr<gchar*, decltype(&g_strfreev)> utf8_argv(
            g_win32_get_command_line(), g_strfreev);
        require(bool(utf8_argv), "cannot convert command line to UTF-8");
        result = run(g_strv_length(utf8_argv.get()), utf8_argv.get());
#else
        result = run(argc, argv);
#endif
    } catch (const std::exception& error) {
        std::cerr << "HEIF feasibility failure: " << error.what() << '\n';
    }
    return result;
}
