// SPDX-License-Identifier: GPL-3.0-or-later
#include "rtengine/image16.h"
#include "rtengine/color.h"
#include "rtengine/image8.h"
#include "rtengine/stdimagesource.h"
#include "rtengine/heifinput.h"
#include "rtengine/metadata.h"
#include "rtengine/imagedata.h"
#include "rtengine/previewimage.h"
#include "rtengine/rtthumbnail.h"
#include "rtengine/settings.h"
#include "rtengine/utils.h"
#include "rtgui/options.h"
#include <cmath>
#include <giomm.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <algorithm>
#include <array>
#include <cstring>
#include <future>
#ifdef LIBHEIF
#include <libheif/heif.h>
#include <libheif/heif_items.h>
#include <libheif/heif_properties.h>
#endif
namespace rtengine { extern const Settings* settings; }
namespace {
using namespace rtengine;
using Path = std::filesystem::path;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
struct Listener : ProgressListener {
    std::string message;
    void setProgress(double) override {}
    void setProgressStr(const Glib::ustring&) override {}
    void setProgressState(bool) override {}
    void error(const Glib::ustring& s) override { message = s; }
};
#ifdef LIBHEIF
template<class T, auto release> using Owner = std::unique_ptr<T, decltype(release)>;
void check(heif_error error) { require(error.code == heif_error_Ok, error.message); }
std::string profile(bool wide = false)
{
    auto icc = cmsCreate_sRGBProfile();
    if (wide) {
        cmsCloseProfile(icc);
        const cmsCIExyY white{0.3127, 0.3290, 1.0};
        const cmsCIExyYTRIPLE primaries{{0.670, 0.320, 1.0}, {0.260, 0.700, 1.0}, {0.145, 0.060, 1.0}};
        auto* gamma = cmsBuildGamma(nullptr, 2.25);
        cmsToneCurve* curves[]{gamma, gamma, gamma};
        icc = cmsCreateRGBProfile(&white, &primaries, curves);
        cmsFreeToneCurve(gamma);
    }
    cmsUInt32Number length = 0;
    cmsSaveProfileToMem(icc, nullptr, &length);
    std::string data(length, '\0');
    cmsSaveProfileToMem(icc, data.data(), &length);
    cmsCloseProfile(icc);
    return data;
}
void fixture(const Path& path, int bits = 10, int orientation = 1, int primaries = 1,
    int transfer = 13, int matrix = 1, bool full = true, const std::string& icc = "",
    bool grid = false, bool crop = false, bool secondary = false, bool omitNclx = false)
{
    Owner<heif_context, heif_context_free> context(heif_context_alloc(), heif_context_free);
    heif_encoder* encoderRaw = nullptr;
    check(heif_context_get_encoder_for_format(context.get(), heif_compression_HEVC, &encoderRaw));
    Owner<heif_encoder, heif_encoder_release> encoder(encoderRaw, heif_encoder_release);
    check(heif_encoder_set_lossless(encoder.get(), 1));
    for (const auto& p : std::array<std::pair<const char*, const char*>, 5>{{
        {"chroma", "444"}, {"preset", "ultrafast"}, {"x265:pools", "none"},
        {"x265:frame-threads", "1"}, {"x265:wpp", "0"}}}) check(heif_encoder_set_parameter(encoder.get(), p.first, p.second));
    check(heif_encoder_set_logging_level(encoder.get(), 0));
    check(heif_encoder_set_parameter(encoder.get(), "x265:colorprim", std::to_string(primaries).c_str()));
    check(heif_encoder_set_parameter(encoder.get(), "x265:transfer", std::to_string(transfer).c_str()));
    check(heif_encoder_set_parameter(encoder.get(), "x265:colormatrix", std::to_string(matrix).c_str()));
    check(heif_encoder_set_parameter(encoder.get(), "x265:range", full ? "full" : "limited"));
    heif_image* imageRaw = nullptr;
    check(heif_image_create(64, 48, heif_colorspace_RGB, heif_chroma_444, &imageRaw));
    Owner<heif_image, heif_image_release> image(imageRaw, heif_image_release);
    for (auto channel : {heif_channel_R, heif_channel_G, heif_channel_B}) {
        check(heif_image_add_plane(image.get(), channel, 64, 48, bits));
        int stride;
        auto* plane = heif_image_get_plane(image.get(), channel, &stride);
        for (int y = 0; y < 48; ++y) for (int x = 0; x < 64; ++x) {
            const uint16_t value = (x * 13 + y * 23 + int(channel) * 47) % (1 << bits);
            if (bits == 8) plane[y * stride + x] = value;
            else std::memcpy(plane + y * stride + x * 2, &value, 2);
        }
    }
    Owner<heif_color_profile_nclx, heif_nclx_color_profile_free> nclx(heif_nclx_color_profile_alloc(), heif_nclx_color_profile_free);
    check(heif_nclx_color_profile_set_color_primaries(nclx.get(), primaries));
    check(heif_nclx_color_profile_set_transfer_characteristics(nclx.get(), transfer));
    check(heif_nclx_color_profile_set_matrix_coefficients(nclx.get(), matrix));
    nclx->full_range_flag = full;
    check(heif_image_set_nclx_color_profile(image.get(), nclx.get()));
    if (!icc.empty()) check(heif_image_set_raw_color_profile(image.get(), "prof", icc.data(), icc.size()));
    Owner<heif_encoding_options, heif_encoding_options_free> options(heif_encoding_options_alloc(), heif_encoding_options_free);
    options->output_nclx_profile = nclx.get();
    options->image_orientation = static_cast<heif_orientation>(orientation);
    options->save_two_colr_boxes_when_ICC_and_nclx_available = 1;
    options->macOS_compatibility_workaround_no_nclx_profile = omitNclx;
    heif_image_handle* rawHandle = nullptr;
    if (grid) {
        heif_image* tiles[]{image.get(), image.get(), image.get(), image.get()};
        check(heif_context_encode_grid(context.get(), tiles, 2, 2, encoder.get(), options.get(), &rawHandle));
    } else check(heif_context_encode_image(context.get(), image.get(), encoder.get(), options.get(), &rawHandle));
    Owner<heif_image_handle, heif_image_handle_release> handle(rawHandle, heif_image_handle_release);
    Exiv2::ExifData exif;
    exif["Exif.Image.Make"] = "G5 camera";
    exif["Exif.Image.Orientation"] = uint16_t(6); // deliberately stale; container wins
    Exiv2::Blob blob;
    Exiv2::ExifParser::encode(blob, Exiv2::littleEndian, exif);
    check(heif_context_add_exif_metadata(context.get(), handle.get(), blob.data(), blob.size()));
    Exiv2::XmpData xmp;
    xmp["Xmp.dc.title"] = "lang=x-default G5 primary";
    std::string packet;
    require(Exiv2::XmpParser::encode(packet, xmp) == 0, "encode XMP");
    check(heif_context_add_XMP_metadata(context.get(), handle.get(), packet.data(), packet.size()));
    if (secondary) {
        heif_image_handle* extraRaw = nullptr;
        check(heif_context_encode_image(context.get(), image.get(), encoder.get(), options.get(), &extraRaw));
        Owner<heif_image_handle, heif_image_handle_release> extra(extraRaw, heif_image_handle_release);
        exif["Exif.Image.Make"] = "wrong secondary camera";
        blob.clear(); Exiv2::ExifParser::encode(blob, Exiv2::littleEndian, exif);
        check(heif_context_add_exif_metadata(context.get(), extra.get(), blob.data(), blob.size()));
        check(heif_context_set_primary_image(context.get(), handle.get()));
    }
    check(heif_context_write_to_file(context.get(), path.u8string().c_str()));
    if (crop) {
        // Narrow the encoder's padding clap, preserving its center offset.
        // This tests a container crop rather than cropping the source pixels.
        auto data = Glib::file_get_contents(path.u8string());
        const auto clap = data.find("clap");
        require(clap != std::string::npos && clap + 36 <= data.size(), "fixture clap");
        data[clap + 7] = 40;
        data[clap + 15] = 32;
        Glib::file_set_contents(path.u8string(), data);
    }
}
std::vector<uint16_t> pixels(ImageIO& image)
{
    std::vector<uint16_t> data(size_t(image.getWidth()) * image.getHeight() * 3);
    for (int y = 0; y < image.getHeight(); ++y) image.getScanline(y,
        reinterpret_cast<unsigned char*>(data.data() + size_t(y) * image.getWidth() * 3), 16);
    return data;
}
void verify(const Path& path, int bits, int width, int height, const std::string& icc = "")
{
    StdImageSource source;
    require(source.load(path.u8string()) == 0, "load " + path.u8string());
    auto& image = *source.getImageIO();
    require(image.getWidth() == width && image.getHeight() == height, "logical geometry " + path.u8string());
    require(image.getBPS() == (bits <= 8 ? 8 : 16), "input storage precision");
    auto data = pixels(image);
    if (bits > 8) require(std::set<uint16_t>(data.begin(), data.end()).size() > 256, "8-bit intermediate quantization");
    int length; unsigned char* bytes;
    image.getEmbeddedProfileData(length, bytes);
    require(length > 0 && bytes && image.getEmbeddedProfile(), "input ICC absent");
    if (!icc.empty()) require(std::string(reinterpret_cast<char*>(bytes), length) == icc, "ICC changed");
    else {
        auto* trc = static_cast<cmsToneCurve*>(cmsReadTag(image.getEmbeddedProfile(), cmsSigRedTRCTag));
        require(trc && cmsEvalToneCurveFloat(trc, 0.5f) > 0.1f, "invalid synthesized TRC");
    }
    auto fallback = readHEIFMetadata(path.u8string());
    require(fallback.exif["Exif.Image.Make"].toString() == "G5 camera" && !fallback.xmp.empty(), "buffer metadata fallback");
    Exiv2Metadata metadata(path.u8string(), false); metadata.load();
    int mw, mh; metadata.getDimensions(mw, mh);
    require(mw == width && mh == height && metadata.exifData()["Exif.Image.Orientation"].toInt64() == 1, "metadata geometry");
    FramesData frames(path.u8string());
    require(frames.hasExif() && frames.getMake() == "G5 camera", "frame metadata");
    // Export raw loader pixels for an independent FFmpeg decoder comparison.
    std::ofstream output(path.u8string() + ".rgb16", std::ios::binary);
    output.write(reinterpret_cast<const char*>(data.data()), data.size() * 2);
    std::ofstream manifest(path.parent_path() / "manifest.tsv", std::ios::app);
    manifest << path.filename().u8string() << '\t' << bits << '\t' << width << '\t' << height << '\n';
}
void reject(const Path& path, const std::string& text)
{
    Image16 image;
    Listener listener;
    image.setProgressListener(&listener);
    require(image.load(path.u8string()) != IMIO_SUCCESS, "accepted invalid input " + path.u8string());
    require(listener.message.find(text) != std::string::npos, "wrong diagnostic: " + listener.message);
}
void run(const Path& root)
{
    std::filesystem::create_directories(root);
    std::ofstream(root / "manifest.tsv").close();
    const auto icc = profile();
    for (int bits : {8, 10, 12}) {
        auto path = root / ("icc-" + std::to_string(bits) + ".heic");
        fixture(path, bits, 1, 1, 13, 1, true, icc);
        verify(path, bits, 64, 48, icc);
    }
    const auto wide = profile(true);
    fixture(root / "icc-arbitrary.heic", 12, 1, 2, 2, 1, true, wide);
    verify(root / "icc-arbitrary.heic", 12, 64, 48, wide);
    fixture(root / "icc-only.heic", 12, 1, 2, 2, 1, true, wide, false, false, false, true);
    verify(root / "icc-only.heic", 12, 64, 48, wide);
    fixture(root / "vui-only.heic", 10, 1, 1, 13, 1, true, "", false, false, false, true);
    verify(root / "vui-only.heic", 10, 64, 48);
    for (int orientation = 1; orientation <= 8; ++orientation) {
        auto path = root / ("orientation-" + std::to_string(orientation) + ".heic");
        fixture(path, 10, orientation);
        verify(path, 10, orientation >= 5 ? 48 : 64, orientation >= 5 ? 64 : 48);
    }
    Image16 normal;
    require(normal.load((root / "orientation-1.heic").u8string()) == 0, "orientation base");
    const auto base = pixels(normal);
    for (int orientation = 1; orientation <= 8; ++orientation) {
        Image16 rotated;
        require(rotated.load((root / ("orientation-" + std::to_string(orientation) + ".heic")).u8string()) == 0, "orientation load");
        const auto data = pixels(rotated);
        for (int y = 0; y < 48; ++y) for (int x = 0; x < 64; ++x) {
            int ox = x, oy = y;
            switch (orientation) {
                case 2: ox = 63-x; break;
                case 3: ox = 63-x; oy = 47-y; break;
                case 4: oy = 47-y; break;
                case 5: ox = y; oy = x; break;
                case 6: ox = 47-y; oy = x; break;
                case 7: ox = 47-y; oy = 63-x; break;
                case 8: ox = y; oy = 63-x; break;
            }
            for (int c = 0; c < 3; ++c) require(data[(oy * rotated.getWidth() + ox)*3+c] == base[(y*64+x)*3+c], "orientation pixel placement " + std::to_string(orientation) + " at " + std::to_string(x) + "," + std::to_string(y));
        }
    }
    for (int primaries : {1, 9, 12}) for (int transfer : {1, 4, 5, 6, 8, 13, 14, 15}) for (bool full : {false, true}) {
        auto path = root / ("nclx-" + std::to_string(primaries) + "-" + std::to_string(transfer) + "-" + std::to_string(full) + ".heic");
        fixture(path, 12, 1, primaries, transfer, primaries == 9 ? 9 : 1, full);
        verify(path, 12, 64, 48);
        StdImageSource color; require(color.load(path.u8string()) == 0, "color profile load");
        auto* trc = static_cast<cmsToneCurve*>(cmsReadTag(color.getImageIO()->getEmbeddedProfile(), cmsSigRedTRCTag));
        const double midpoint = transfer == 13 ? 0.21404114048223255 : transfer == 8 ? 0.5 :
            transfer == 4 ? 0.217637640824031 : transfer == 5 ? 0.143587294374629 :
            transfer == 15 ? 0.259719437101177 : 0.25958940050628576;
        require(std::abs(cmsEvalToneCurveFloat(trc, 0.5f) - midpoint) < 0.00002, "SDR transfer midpoint");
        const auto* red = static_cast<const cmsCIEXYZ*>(cmsReadTag(color.getImageIO()->getEmbeddedProfile(), cmsSigRedColorantTag));
        const double redX = primaries == 1 ? 0.43607 : primaries == 9 ? 0.67346 : 0.51510;
        require(red && std::abs(red->X - redX) < 0.001, "SDR primary colorant");
    }
    fixture(root / "primary.heic", 10, 1, 1, 13, 1, true, "", false, false, true);
    verify(root / "primary.heic", 10, 64, 48);
    fixture(root / "grid.heif", 10, 1, 1, 13, 1, true, "", true);
    verify(root / "grid.heif", 10, 128, 96);
    fixture(root / "crop.heic", 10, 1, 1, 13, 1, true, "", false, true);
    verify(root / "crop.heic", 10, 40, 32);
    Image16 cropped, tiled;
    require(cropped.load((root / "crop.heic").u8string()) == 0 && tiled.load((root / "grid.heif").u8string()) == 0, "crop/grid load");
    const auto cropPixels = pixels(cropped), tilePixels = pixels(tiled);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 40; ++x) for (int c = 0; c < 3; ++c)
        require(cropPixels[(y*40+x)*3+c] == base[((y+8)*64+x+12)*3+c], "crop pixel placement");
    for (int y = 0; y < 96; ++y) for (int x = 0; x < 128; ++x) for (int c = 0; c < 3; ++c)
        require(tilePixels[(y*128+x)*3+c] == base[((y%48)*64+x%64)*3+c], "grid tile placement");
    const auto unicode = root / "žluťoučký-写真.HIF";
    std::filesystem::copy_file(root / "icc-10.heic", unicode, std::filesystem::copy_options::overwrite_existing);
    verify(unicode, 10, 64, 48, icc);
    // Old extension preferences gain the new types; explicit opt-outs remain.
    const auto optionsPath = root / "extensions-options";
    Glib::file_set_contents(optionsPath.u8string(), "[File Browser]\nParseExtensions=jpg;heif;\nParseExtensionsEnabled=1;0;\n");
    Options extensions; extensions.readFromFile(optionsPath.u8string());
    for (const std::string ext : {"heic", "heif", "hif"}) {
        auto position = std::find(extensions.parseExtensions.begin(), extensions.parseExtensions.end(), ext);
        require(position != extensions.parseExtensions.end(), "extension migration");
        require(extensions.parseExtensionsEnabled[position - extensions.parseExtensions.begin()] == (ext == "heif" ? 0 : 1), "extension enable preference");
    }
    // Sidecar merging operates on a writable in-memory metadata image even
    // when the installed Exiv2 cannot modify BMFF images.
    Exiv2::XmpData sidecar;
    sidecar["Xmp.dc.creator"] = "sidecar artist";
    sidecar["Xmp.tiff.Orientation"] = uint16_t(8);
    sidecar["Xmp.tiff.ImageWidth"] = uint32_t(9999);
    std::string packet; require(Exiv2::XmpParser::encode(packet, sidecar) == 0, "encode sidecar");
    const auto sidecarPath = Exiv2Metadata::xmpSidecarPath(unicode.u8string());
    Glib::file_set_contents(sidecarPath, packet);
    Exiv2Metadata merged(unicode.u8string(), true); merged.load();
    require(merged.exifData()["Exif.Image.Artist"].toString() == "sidecar artist" &&
        merged.exifData()["Exif.Image.Orientation"].toString() == "1" &&
        merged.exifData()["Exif.Image.ImageWidth"].toString() == "64", "HEIF sidecar merge geometry");
    std::filesystem::remove(std::filesystem::u8path(sidecarPath.raw()));
    for (int transfer : {16, 18, 2}) {
        auto path = root / ("reject-transfer-" + std::to_string(transfer) + ".heic");
        fixture(path, 10, 1, 9, transfer, 9);
        reject(path, transfer == 2 ? "transfer" : "HDR");
    }
    fixture(root / "reject-primaries.heic", 10, 1, 2);
    reject(root / "reject-primaries.heic", "primaries");
    fixture(root / "reject-icc.heic", 10, 1, 1, 13, 1, true, "invalid ICC");
    reject(root / "reject-icc.heic", "ICC");
    fixture(root / "reject-hdr-icc.heic", 10, 1, 9, 16, 9, true, icc);
    reject(root / "reject-hdr-icc.heic", "HDR");
    fixture(root / "reject-vui-only.heic", 10, 1, 2, 2, 1, true, "", false, false, false, true);
    reject(root / "reject-vui-only.heic", "primaries");
    fixture(root / "reject-vui-hdr.heic", 10, 1, 9, 16, 9, true, "", false, false, false, true);
    reject(root / "reject-vui-hdr.heic", "HDR");
    auto matrixBytes = Glib::file_get_contents((root / "orientation-1.heic").u8string());
    const auto nclxPosition = matrixBytes.find("nclx");
    require(nclxPosition != std::string::npos, "fixture NCLX");
    matrixBytes[nclxPosition + 9] = 2;
    Glib::file_set_contents((root / "reject-matrix.heic").u8string(), matrixBytes);
    reject(root / "reject-matrix.heic", "matrix");
    std::ofstream(root / "wrong.heic") << "not an image";
    reject(root / "wrong.heic", "container");
    auto contents = Glib::file_get_contents((root / "icc-10.heic").u8string());
    Glib::file_set_contents((root / "truncated.heic").u8string(), contents.substr(0, contents.size() / 2));
    reject(root / "truncated.heic", "container");
    // libheif extraction fallback must validate the offset before Exiv2 sees TIFF.
    contents = Glib::file_get_contents((root / "orientation-1.heic").u8string());
    const auto tiff = contents.find(std::string("II*\0", 4));
    require(tiff != std::string::npos && tiff >= 4, "find EXIF fixture payload");
    contents.replace(tiff - 4, 4, 4, '\xff');
    const auto malformed = root / "bad-exif-offset.heic";
    Glib::file_set_contents(malformed.u8string(), contents);
    bool failed = false;
    try { readHEIFMetadata(malformed.u8string()); }
    catch (const std::exception& error) { failed = std::string(error.what()).find("offset") != std::string::npos; }
    require(failed, "unbounded EXIF TIFF offset accepted");
    require(hasHeifInputExtension("photo.HEIC") && hasHeifInputExtension("photo.HEIF") && hasHeifInputExtension("photo.HIF") && !hasHeifInputExtension("photo.avif"), "input extensions");
    int w = 32, h = 32;
    std::unique_ptr<Thumbnail> thumbnail(Thumbnail::loadFromImage(unicode.u8string(), w, h, 1, 1., ColorTemp::DEFAULT_OBSERVER));
    require(thumbnail && w > 0 && h > 0, "file-browser thumbnail");
    PreviewImage preview(unicode.u8string(), "HIF", PreviewImage::PIM_EmbeddedOrRaw);
    require(preview.getImage() && preview.getImage()->get_width() == 64 && preview.getImage()->get_height() == 48, "inspector preview");
    std::vector<std::future<void>> workers;
    for (int i = 0; i < 4; ++i) workers.push_back(std::async(std::launch::async, [&] {
        StdImageSource source; require(source.load(unicode.u8string()) == 0, "concurrent input");
    }));
    for (auto& worker : workers) worker.get();
    std::cout << "PASS precision, color, orientation, grid, crop, metadata, Unicode, thumbnails, inspector and concurrent loading\n";
}
#endif
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2 || argc == 3, "expected artifact directory [--decoder-unavailable]");
        Gio::init();
        rtengine::Settings settings{};
        settings.metadata_xmp_sync = rtengine::Settings::MetadataXmpSync::NONE;
        rtengine::settings = &settings;
        rtengine::Exiv2Metadata::init();
        rtengine::Color::init();
#ifdef LIBHEIF
        check(heif_init(nullptr));
        if (argc == 3 && std::string(argv[2]) == "--read-only") {
            for (const auto* name : {"icc-10.heic", "grid.heif", "orientation-6.heic", "primary.heic"}) {
                const auto path = (Path(argv[1]) / name).u8string();
                StdImageSource source; require(source.load(path) == 0, "read-only input");
                FramesData frames(path);
                require(frames.hasExif() && frames.getMake() == "G5 camera", "read-only frame metadata");
                Exiv2Metadata metadata(path, false); metadata.load();
                require(!metadata.xmpData().empty(), "read-only XMP");
            }
            std::cout << "PASS primary image and metadata loading without native BMFF or encoder\n";
        } else if (argc == 3) reject(Path(argv[1]) / "icc-10.heic", "libde265");
        else run(Path(argv[1]));
        heif_deinit();
#else
        rtengine::Image16 image;
        Listener listener; image.setProgressListener(&listener);
        require(image.load("disabled.heic") == IMIO_FILETYPENOTSUPPORTED && listener.message.find("disabled") != std::string::npos, "disabled input");
        std::cout << "PASS disabled input diagnostic\n";
#endif
        rtengine::Exiv2Metadata::cleanup();
        rtengine::Color::cleanup();
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
