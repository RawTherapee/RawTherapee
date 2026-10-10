// SPDX-License-Identifier: GPL-3.0-or-later
// Production metadata preparation, JPEG regression and HEIF round trips.
#include "rtengine/image8.h"
#include "rtengine/metadata.h"
#include "rtengine/settings.h"
#include <glib/gstdio.h>
#include <giomm.h>
#include <filesystem>
#include <array>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <iostream>
#include <future>
#include <stdexcept>
#ifdef LIBHEIF
#include <libheif/heif.h>
#endif

namespace rtengine { extern const Settings* settings; }
namespace {
void require(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}
std::string value(const Exiv2::ExifData& data, const std::string& key)
{
    auto it = data.findKey(Exiv2::ExifKey(key));
    return it == data.end() ? "" : it->toString();
}
std::string value(const Exiv2::XmpData& data, const std::string& key)
{
    auto it = data.findKey(Exiv2::XmpKey(key));
    return it == data.end() ? "" : it->toString();
}
struct Listener : rtengine::ProgressListener {
    std::string message;
    void setProgress(double) override {}
    void setProgressStr(const Glib::ustring&) override {}
    void setProgressState(bool) override {}
    void error(const Glib::ustring& text) override { message = text; }
};
struct Metadata {
    Exiv2::ExifData exif;
    Exiv2::XmpData xmp;
};
#ifdef LIBHEIF
Metadata readHEIF(const std::filesystem::path& path)
{
    auto context = heif_context_alloc();
    require(context, "allocate reader");
    require(heif_context_read_from_file(context, path.u8string().c_str(), nullptr).code == heif_error_Ok, "read HEIF");
    heif_image_handle* image = nullptr;
    require(heif_context_get_primary_image_handle(context, &image).code == heif_error_Ok, "get primary");
    const int count = heif_image_handle_get_number_of_metadata_blocks(image, nullptr);
    std::vector<heif_item_id> ids(count);
    require(heif_image_handle_get_list_of_metadata_block_IDs(image, nullptr, ids.data(), count) == count, "metadata list");
    Metadata result;
    for (auto id : ids) {
        std::vector<Exiv2::byte> bytes(heif_image_handle_get_metadata_size(image, id));
        require(heif_image_handle_get_metadata(image, id, bytes.data()).code == heif_error_Ok, "read metadata");
        const std::string type = heif_image_handle_get_metadata_type(image, id);
        if (type == "Exif") {
            require(bytes.size() >= 12, "short EXIF");
            const uint32_t offset = uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 | uint32_t(bytes[2]) << 8 | bytes[3];
            require(offset <= bytes.size() - 12, "invalid EXIF offset");
            Exiv2::ExifParser::decode(result.exif, bytes.data() + 4 + offset, bytes.size() - 4 - offset);
        } else {
            require(type == "mime" && std::string(heif_image_handle_get_metadata_content_type(image, id)) == "application/rdf+xml",
                "nonstandard metadata item");
            require(Exiv2::XmpParser::decode(result.xmp, std::string(bytes.begin(), bytes.end())) == 0, "decode XMP");
        }
    }
    heif_image_handle_release(image);
    heif_context_free(context);
    return result;
}
#endif
void run(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root);
    rtengine::Image8 pixels(64, 48);
    std::fill(pixels.data, pixels.data + 64 * 48 * 3, 127);
    const auto source = root / "source.jpg";
    require(pixels.saveAsJPEG(source.u8string(), 92, 1) == IMIO_SUCCESS, "create source JPEG");
    auto input = Exiv2::ImageFactory::open(source.u8string());
    input->readMetadata();
    input->exifData()["Exif.Image.Make"] = "Canon";
    input->exifData()["Exif.Image.Model"] = "EOS metadata fixture";
    input->exifData()["Exif.Image.Artist"] = "source artist";
    input->exifData()["Exif.Image.Orientation"] = uint16_t(6);
    input->exifData()["Exif.Image.ImageWidth"] = uint32_t(9999);
    input->exifData()["Exif.Image.ImageLength"] = uint32_t(8888);
    input->exifData()["Exif.Photo.PixelXDimension"] = uint32_t(9999);
    input->exifData()["Exif.Photo.PixelYDimension"] = uint32_t(8888);
    input->exifData()["Exif.Photo.DateTimeOriginal"] = "2024:01:02 03:04:05";
    input->exifData()["Exif.Canon.SerialNumber"] = uint32_t(123456);
    Exiv2::ExifThumb(input->exifData()).setJpegThumbnail(source.u8string());
    input->iptcData()["Iptc.Envelope.CharacterSet"] = "\033%G";
    input->iptcData()["Iptc.Application2.Caption"] = "source caption";
    input->iptcData()["Iptc.Application2.City"] = "Praha";
    input->iptcData()["Iptc.Application2.Keywords"] = "first";
    Exiv2::Iptcdatum keyword(Exiv2::IptcKey("Iptc.Application2.Keywords"));
    keyword.setValue("second"); input->iptcData().add(keyword);
    input->iptcData()["Iptc.Application2.Copyright"] = "source rights";
    input->iptcData()["Iptc.Application2.Byline"] = "source creator";
    input->iptcData()["Iptc.Application2.0x00fe"] = "unmapped fixture";
    input->xmpData()["Xmp.dc.description"] = "lang=x-default explicit caption";
    input->xmpData()["Xmp.photoshop.City"] = "explicit city";
    input->xmpData()["Xmp.tiff.Orientation"] = "6";
    input->xmpData()["Xmp.exif.PixelXDimension"] = "9999";
    input->xmpData()["Xmp.exif.PixelYDimension"] = "8888";
    input->xmpData()["Xmp.xmp.Label"] = "source label";
    input->writeMetadata();

    auto sidecar = Exiv2::ImageFactory::create(Exiv2::ImageType::xmp, (root / "source.xmp").u8string());
    sidecar->xmpData()["Xmp.xmp.Label"] = "sidecar label";
    sidecar->xmpData()["Xmp.dc.description"] = "lang=x-default sidecar caption";
    sidecar->xmpData()["Xmp.tiff.Artist"] = "sidecar artist";
    sidecar->writeMetadata();

    for (const std::string mode : {"copy", "edit", "selected", "sidecar", "geometry"}) {
        rtengine::Exiv2Metadata info(source.u8string(), mode == "sidecar");
        info.load();
        if (mode == "edit") {
            rtengine::procparams::ExifPairs exif;
            exif["Exif.Image.Artist"] = "edited artist";
            info.setExif(exif);
            rtengine::procparams::IPTCPairs iptc;
            iptc["Iptc.Application2.Caption"] = {"Žluťoučký kůň 📷"};
            iptc["Iptc.Application2.Copyright"] = {"© 2026 Červený"};
            iptc["Iptc.Application2.Byline"] = {"Jana Nováková", "Petr Černý"};
            iptc["Iptc.Application2.City"] = {"České Budějovice"};
            iptc["Iptc.Application2.Keywords"] = {"kůň", "léto", "kůň"};
            info.setIptc(iptc);
        }
        std::vector<std::string> keys{"Exif.Image.Make"};
        if (mode == "geometry") keys = {"Exif.Image.Make", "Exif.Image.Orientation", "Exif.Image.ImageWidth", "Exif.Image.ImageLength", "Exif.Photo.PixelXDimension", "Exif.Photo.PixelYDimension"};
        if (mode == "selected" || mode == "geometry") info.setExifKeys(&keys);
        auto prepared = info.prepareExportMetadata(false);
        if (mode != "geometry") require(value(prepared.exif, "Exif.Image.Orientation").empty(), "stale orientation retained");
        if (mode != "geometry") require(value(prepared.exif, "Exif.Image.ImageWidth").empty(), "stale width retained");
        if (mode != "geometry") require(value(prepared.exif, "Exif.Image.ImageLength").empty(), "stale height retained");
        require(value(prepared.exif, "Exif.Thumbnail.JPEGInterchangeFormat").empty(), "stale thumbnail retained");
        require(value(prepared.exif, "Exif.Image.Software").find("RawTherapee ") == 0, "software");
        require(!value(prepared.exif, "Exif.Image.DateTime").empty(), "timestamp");
        if (mode == "selected") require(value(prepared.exif, "Exif.Image.Model").empty(), "selection ignored");
        pixels.setMetadata(std::move(info));
        const auto jpeg = root / (mode + ".jpg");
        require(pixels.saveAsJPEG(jpeg.u8string(), 92, 1) == IMIO_SUCCESS, "JPEG metadata export");
        auto written = Exiv2::ImageFactory::open(jpeg.u8string()); written->readMetadata();
        require(value(written->exifData(), "Exif.Image.Make") == "Canon", "JPEG make regression");
        if (mode == "edit") require(value(written->exifData(), "Exif.Image.Artist") == "edited artist", "JPEG edit regression");
#ifdef LIBHEIF
        require(pixels.saveAsHEIF((root / (mode + ".heic")).u8string()) == IMIO_SUCCESS, "HEIF metadata export");
        const auto heif = readHEIF(root / (mode + ".heic"));
        // The formats use identical prepared EXIF and intrusive serialization.
        for (const auto& datum : written->exifData()) {
            const auto key = datum.key();
            if (key == "Exif.Image.DateTime" || key == "Exif.Image.Orientation" || key == "Exif.Image.ImageWidth" || key == "Exif.Image.ImageLength" || key == "Exif.Photo.PixelXDimension" || key == "Exif.Photo.PixelYDimension") continue;
            require(value(heif.exif, datum.key()) == datum.toString(), "JPEG/HEIF EXIF mismatch " + datum.key());
        }
        require(value(heif.exif, "Exif.Image.Orientation") == (mode == "geometry" ? "1" : ""), "HEIF orientation");
        require(value(heif.xmp, "Xmp.tiff.Orientation") == "1", "stale XMP orientation");
        require(value(heif.xmp, "Xmp.exif.PixelXDimension") == "64" && value(heif.xmp, "Xmp.exif.PixelYDimension") == "48", "stale XMP dimensions");
        if (mode != "selected") require(value(heif.exif, "Exif.Photo.PixelXDimension") == "64" && value(heif.exif, "Exif.Photo.PixelYDimension") == "48", "stale EXIF dimensions");
        if (mode != "selected" && mode != "geometry") require(value(heif.exif, "Exif.Canon.SerialNumber") == "123456", "MakerNote lost");
        if (mode == "edit") {
            require(value(heif.xmp, "Xmp.dc.description").find("Žluťoučký kůň 📷") != std::string::npos, "Unicode caption edit lost");
            require(value(heif.xmp, "Xmp.photoshop.City") == "České Budějovice", "city edit lost");
            auto keywords = heif.xmp.findKey(Exiv2::XmpKey("Xmp.dc.subject"));
            require(keywords != heif.xmp.end() && keywords->count() == 3 && keywords->toString(0) == "kůň" && keywords->toString(2) == "kůň", "repeated keywords lost");
            require(value(heif.xmp, "Xmp.dc.rights").find("© 2026 Červený") != std::string::npos, "copyright lost");
            auto creator = heif.xmp.findKey(Exiv2::XmpKey("Xmp.dc.creator"));
            require(creator != heif.xmp.end() && creator->count() == 2 && creator->toString(1) == "Petr Černý", "creators lost");
        } else {
            require(value(heif.xmp, "Xmp.dc.description").find("explicit caption") != std::string::npos, "explicit XMP precedence");
        }
        if (mode == "sidecar") {
            require(value(heif.xmp, "Xmp.xmp.Label") == "sidecar label", "sidecar XMP lost");
            require(value(heif.exif, "Exif.Image.ImageDescription") == "sidecar caption", "sidecar EXIF conversion lost");
        }
#endif
        std::cout << "PASS " << mode << " metadata and JPEG regression\n";
    }
    pixels.setMetadata(rtengine::Exiv2Metadata());
#ifdef LIBHEIF
    require(pixels.saveAsHEIF((root / "strip.heic").u8string()) == IMIO_SUCCESS, "strip export");
    auto stripped = readHEIF(root / "strip.heic");
    require(stripped.exif.empty() && stripped.xmp.empty(), "strip leaked metadata");
    Listener listener; pixels.setProgressListener(&listener);
    const auto protectedPath = root / "protected.heic";
    require(g_file_set_contents(protectedPath.u8string().c_str(), "previous output", -1, nullptr), "protected fixture");
    auto failed = [&]() {
        require(pixels.saveAsHEIF(protectedPath.u8string()) == IMIO_CANNOTWRITEFILE && !listener.message.empty(), "metadata failure not visible");
        gchar* bytes = nullptr; gsize size = 0;
        require(g_file_get_contents(protectedPath.u8string().c_str(), &bytes, &size, nullptr), "read protected");
        const std::string preserved(bytes, size); g_free(bytes);
        require(preserved == "previous output", "metadata failure replaced destination");
    };
    const auto corrupt = root / "corrupt.jpg";
    require(g_file_set_contents(corrupt.u8string().c_str(), "not an image", -1, nullptr), "corrupt fixture");
    pixels.setMetadata(rtengine::Exiv2Metadata(corrupt.u8string(), false)); failed();
    pixels.setMetadata(rtengine::Exiv2Metadata((root / "missing.jpg").u8string(), false)); failed();
    const auto brokenSidecar = root / "corrupt.xmp";
    require(g_file_set_contents(brokenSidecar.u8string().c_str(), "invalid XMP", -1, nullptr), "broken sidecar fixture");
    // Use a valid JPEG with a malformed associated sidecar.
    const auto sidecarSource = root / "broken-sidecar.jpg";
    std::filesystem::copy_file(source, sidecarSource, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(brokenSidecar, root / "broken-sidecar.xmp", std::filesystem::copy_options::overwrite_existing);
    pixels.setMetadata(rtengine::Exiv2Metadata(sidecarSource.u8string(), true)); failed();
    rtengine::Exiv2Metadata::init(); // Discard cached source data mutated by failure fixtures.
    rtengine::Exiv2Metadata invalidXmp(source.u8string(), false); invalidXmp.load();
    invalidXmp.xmpData()["Xmp.xmp.Label"] = std::string(1, char(0xff));
    pixels.setMetadata(std::move(invalidXmp)); failed();
    require(listener.message.find("XMP serialization") != std::string::npos, "missing XMP serialization error");
    rtengine::Exiv2Metadata::init();
    rtengine::Exiv2Metadata large(source.u8string(), false); large.load();
    for (int i = 0; i < 25; ++i) {
        char key[32]; std::snprintf(key, sizeof(key), "Exif.Image.0x%04x", 0xc100 + i);
        auto text = Exiv2::Value::create(Exiv2::asciiString); text->read(std::string(3000, 'x'));
        large.exifData().add(Exiv2::ExifKey(key), text.get());
    }
    auto tooLarge = large.prepareExportMetadata(false);
    Exiv2::Blob blob; Exiv2::ExifParser::encode(blob, Exiv2::littleEndian, tooLarge.exif);
    require(blob.size() > 65527, "fixture did not exceed serializer limit");
    pixels.setMetadata(std::move(large)); failed();
    require(listener.message.find("JPEG limit") != std::string::npos, "missing size error");
    // Known oversized tags are filtered by the same JPEG serializer.
    rtengine::Exiv2Metadata::init();
    rtengine::Exiv2Metadata filtered(source.u8string(), false); filtered.load();
    filtered.exifData()["Exif.Photo.UserComment"] = "charset=Ascii " + std::string(80000, 'x');
    pixels.setMetadata(std::move(filtered));
    require(pixels.saveAsJPEG((root / "large-filtered.jpg").u8string(), 92, 1) == IMIO_SUCCESS, "large JPEG");
    require(pixels.saveAsHEIF((root / "large-filtered.heic").u8string()) == IMIO_SUCCESS, "large HEIF");
    require(value(readHEIF(root / "large-filtered.heic").exif, "Exif.Photo.UserComment").empty(), "serializer filtering changed");
    // XMP has no JPEG APP1 ceiling in HEIF; preserve a larger packet.
    rtengine::Exiv2Metadata::init();
    rtengine::Exiv2Metadata largeXmp(source.u8string(), false); largeXmp.load();
    largeXmp.xmpData()["Xmp.xmp.Label"] = std::string(100000, 'a');
    pixels.setMetadata(std::move(largeXmp));
    require(pixels.saveAsHEIF((root / "large-xmp.heic").u8string()) == IMIO_SUCCESS, "large XMP");
    require(value(readHEIF(root / "large-xmp.heic").xmp, "Xmp.xmp.Label").size() == 100000, "large XMP truncated");
    // Cache reuse after a metadata-free image was loaded for HEIF must not
    // change the default JPEG load contract.
    pixels.setMetadata(rtengine::Exiv2Metadata());
    const auto png = root / "no-exif.png";
    require(pixels.saveAsPNG(png.u8string(), 8) == IMIO_SUCCESS, "metadata-free source");
    rtengine::Exiv2Metadata empty(png.u8string(), false); empty.load(false);
    bool rejected = false;
    try { rtengine::Exiv2Metadata legacy(png.u8string(), false); legacy.load(); }
    catch (const std::exception&) { rejected = true; }
    require(rejected, "HEIF cache changed default JPEG load contract");
    pixels.setMetadata(std::move(empty));
    require(pixels.saveAsHEIF((root / "no-exif.heic").u8string()) == IMIO_SUCCESS, "metadata-free HEIF source");
    require(value(readHEIF(root / "no-exif.heic").exif, "Exif.Image.Software").find("RawTherapee ") == 0, "metadata-free source software");
    rtengine::Exiv2Metadata::init();
    std::vector<std::future<void>> workers;
    for (int i = 0; i < 4; ++i) {
        workers.push_back(std::async(std::launch::async, [&, i] {
            rtengine::Image8 image(64, 48);
            std::fill(image.data, image.data + 64 * 48 * 3, 127);
            rtengine::Exiv2Metadata info(source.u8string(), false);
            rtengine::procparams::IPTCPairs iptc;
            iptc["Iptc.Application2.Keywords"] = {"worker " + std::to_string(i)};
            info.setIptc(iptc);
            image.setMetadata(std::move(info));
            require(image.saveAsHEIF((root / ("worker-" + std::to_string(i) + ".heic")).u8string()) == IMIO_SUCCESS, "concurrent metadata export");
        }));
    }
    for (auto& worker : workers) worker.get();
    for (int i = 0; i < 4; ++i) require(value(readHEIF(root / ("worker-" + std::to_string(i) + ".heic")).xmp,
        "Xmp.dc.subject") == "worker " + std::to_string(i), "concurrent edit cross-contamination");
    std::cout << "PASS strip, metadata read/EXIF size/XMP serialization failures, large EXIF filtering, large XMP, cache and concurrency\n";
    for (const auto& entry : std::filesystem::directory_iterator(root)) require(entry.path().filename().u8string().find(".rawtherapee-heif-") != 0, "temporary output leaked");
#endif
}
} // namespace
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "expected artifact directory");
        Gio::init();
        rtengine::Settings settings{};
        settings.xmp_sidecar_style = rtengine::Settings::XmpSidecarStyle::STD;
        settings.metadata_xmp_sync = rtengine::Settings::MetadataXmpSync::NONE;
        rtengine::settings = &settings;
        rtengine::Exiv2Metadata::init();
        run(std::filesystem::u8path(argv[1]));
        rtengine::Exiv2Metadata::cleanup();
        std::cout << "PASS export metadata checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
