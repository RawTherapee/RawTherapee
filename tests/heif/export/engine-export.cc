#include "rtengine/image8.h"
#include "rtengine/image16.h"
#include "rtengine/imagefloat.h"
#include "rtengine/settings.h"
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
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#ifdef __unix__
#include <csignal>
#include <sys/resource.h>
#endif
#ifdef LIBHEIF
#include <libheif/heif.h>
#endif

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
constexpr int width = 1025;
constexpr int height = 257;
double sample(int x, int y, int channel)
{
    if (y < 128) {
        return double(x) / (width - 1);
    }
    const std::array<std::array<double, 3>, 8> patches{{
        {{0.05, 0.05, 0.05}}, {{0.95, 0.95, 0.95}}, {{0.85, 0.10, 0.15}},
        {{0.10, 0.80, 0.20}}, {{0.10, 0.15, 0.85}}, {{0.80, 0.65, 0.15}},
        {{0.15, 0.70, 0.80}}, {{0.70, 0.15, 0.75}}
    }};
    return patches[std::min(7, x * 8 / width)][channel];
}
std::string profile(bool wide)
{
    cmsHPROFILE handle;
    if (wide) {
        const cmsCIExyY white{0.3127, 0.3290, 1.0};
        const cmsCIExyYTRIPLE primaries{{0.670, 0.320, 1.0}, {0.260, 0.700, 1.0}, {0.145, 0.060, 1.0}};
        cmsToneCurve* gamma = cmsBuildGamma(nullptr, 2.25);
        require(gamma, "allocate gamma");
        cmsToneCurve* curves[]{gamma, gamma, gamma};
        handle = cmsCreateRGBProfile(&white, &primaries, curves);
        cmsFreeToneCurve(gamma);
    } else {
        handle = cmsCreate_sRGBProfile();
    }
    require(handle, "allocate ICC");
    cmsUInt32Number size = 0;
    require(cmsSaveProfileToMem(handle, nullptr, &size), "size ICC");
    std::string bytes(size, '\0');
    require(cmsSaveProfileToMem(handle, bytes.data(), &size), "serialize ICC");
    cmsCloseProfile(handle);
    const unsigned char date[]{0x07, 0xe8, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
    std::copy(std::begin(date), std::end(date), bytes.begin() + 24);
    std::fill(bytes.begin() + 84, bytes.begin() + 100, 0);
    return bytes;
}
void write(const std::filesystem::path& path, const std::string& bytes)
{
    require(g_file_set_contents(path.u8string().c_str(), bytes.data(), bytes.size(), nullptr), "write test fixture");
}
std::string read(const std::filesystem::path& path)
{
    gchar* data = nullptr;
    gsize size = 0;
    require(g_file_get_contents(path.u8string().c_str(), &data, &size, nullptr), "read test fixture");
    const std::string bytes(data, size);
    g_free(data);
    return bytes;
}
struct Listener : rtengine::ProgressListener {
    double progress = 0;
    std::string errorMessage;
    void setProgress(double value) override { progress = value; }
    void setProgressStr(const Glib::ustring&) override {}
    void setProgressState(bool) override {}
    void error(const Glib::ustring& message) override { errorMessage = message.raw(); }
};

void run(const std::filesystem::path& directory, const std::string& mode)
{
    std::filesystem::create_directories(directory);
    rtengine::Image8 image8(width, height);
    rtengine::Image16 image16(width, height);
    rtengine::Imagefloat imagefloat(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image8.data[y * width * 3 + x * 3] = std::lround(sample(x, y, 0) * 255);
            image8.data[y * width * 3 + x * 3 + 1] = std::lround(sample(x, y, 1) * 255);
            image8.data[y * width * 3 + x * 3 + 2] = std::lround(sample(x, y, 2) * 255);
            image16.r(y, x) = std::lround(sample(x, y, 0) * 65535);
            image16.g(y, x) = std::lround(sample(x, y, 1) * 65535);
            image16.b(y, x) = std::lround(sample(x, y, 2) * 65535);
            imagefloat.r(y, x) = sample(x, y, 0) * 65535;
            imagefloat.g(y, x) = sample(x, y, 1) * 65535;
            imagefloat.b(y, x) = sample(x, y, 2) * 65535;
        }
    }
    const std::string icc = profile(false);
    image8.setOutputProfile(icc);
    image16.setOutputProfile(icc);
    imagefloat.setOutputProfile(icc);
    rtengine::Exiv2Metadata metadata;
    metadata.exifData()["Exif.Image.Orientation"] = uint16_t(6);
    metadata.exifData()["Exif.Image.Artist"] = "Do not embed in G2";
    imagefloat.setMetadata(std::move(metadata));
    Listener listener;
    imagefloat.setSaveProgressListener(&listener);
#ifndef LIBHEIF
    const auto protectedPath = directory / "disabled.heic";
    write(protectedPath, "preserve existing file");
    for (const auto* image : std::array<const rtengine::IImage*, 3>{&image8, &image16, &imagefloat}) {
        require(image->saveAsHEIF(protectedPath.u8string()) == IMIO_FILETYPENOTSUPPORTED, "disabled return code");
        require(read(protectedPath) == "preserve existing file", "disabled build changed output");
    }
    require(!listener.errorMessage.empty(), "disabled diagnostic");
#else
    if (mode == "--missing-encoder" || mode == "--codec-smoke") {
        const auto protectedPath = directory / "codec.heic";
        const bool missing = mode == "--missing-encoder";
        for (const auto* image : std::array<const rtengine::IImage*, 3>{&image8, &image16, &imagefloat}) {
            write(protectedPath, "preserve existing file");
            const int result = image->saveAsHEIF(protectedPath.u8string());
            require(result == (missing ? IMIO_FILETYPENOTSUPPORTED : IMIO_SUCCESS), "codec availability result");
            require(missing ? read(protectedPath) == "preserve existing file" :
                read(protectedPath).substr(4, 4) == "ftyp", "codec output protection");
        }
        require(!missing || listener.errorMessage.find("HEVC encoder unavailable") != std::string::npos,
            "missing encoder diagnostic");
        std::cout << "PASS " << mode << '\n';
        return;
    }
    for (const auto& entry : std::array<std::pair<const rtengine::IImage*, const char*>, 3>{{
        {&image8, "image8"}, {&image16, "image16"}, {&imagefloat, "float"}}}) {
        for (int bps : {8, 10, 12}) {
            for (int sub : {1, 2, 3}) {
                const char* chroma[] = {"420", "422", "444"};
                const auto name = std::string(entry.second) + "-" + std::to_string(bps) + "-" + chroma[sub - 1];
                const auto path = directory / (name + ".heic");
                require(entry.first->saveAsHEIF(path.u8string(), 100, bps, sub) == IMIO_SUCCESS, "export " + name);
                write(directory / (name + ".icc"), icc);
                std::cout << "PASS " << name << '\n';
            }
        }
    }
    require(listener.progress == 1.0 && listener.errorMessage.empty(), "export progress");
    const auto wide = profile(true);
    imagefloat.setOutputProfile(wide);
    require(imagefloat.saveAsHEIF((directory / "wide.heic").u8string(), 100, 12, 3) == IMIO_SUCCESS, "wide ICC");
    write(directory / "wide.icc", wide);
    imagefloat.setOutputProfile("");
    require(imagefloat.saveAsHEIF((directory / "no-icc.heic").u8string()) == IMIO_SUCCESS, "no ICC");
    imagefloat.setOutputProfile(icc);
    rtengine::Imagefloat clamped(129, 65);
    clamped.setOutputProfile(icc);
    for (int y = 0; y < 65; ++y) {
        for (int x = 0; x < 129; ++x) {
            const float value = x < 32 ? std::numeric_limits<float>::quiet_NaN() :
                x < 64 ? -65535.f : x < 96 ? 32767.5f : std::numeric_limits<float>::infinity();
            clamped.r(y, x) = clamped.g(y, x) = clamped.b(y, x) = value;
        }
    }
    require(clamped.saveAsHEIF((directory / "clamped.heic").u8string(), 100, 12, 3) == IMIO_SUCCESS, "clamp output");
    rtengine::Image16 noise(129, 65);
    noise.setOutputProfile(icc);
    uint32_t state = 42;
    for (int y = 0; y < 65; ++y) {
        for (int x = 0; x < 129; ++x) {
            for (auto* plane : {&noise.r(y, x), &noise.g(y, x), &noise.b(y, x)}) {
                state = state * 1664525u + 1013904223u;
                *plane = state >> 16;
            }
        }
    }
    uintmax_t previousSize = 0;
    for (int quality : {0, 30, 92, 100}) {
        const auto path = directory / ("quality-" + std::to_string(quality) + ".heic");
        require(noise.saveAsHEIF(path.u8string(), quality, 10, 3) == IMIO_SUCCESS, "quality output");
        const auto size = std::filesystem::file_size(path);
        require(size > previousSize, "quality did not increase encoded size on noise fixture");
        previousSize = size;
    }
    for (int bps : {8, 10, 12}) {
        rtengine::Image16 tiny(1, 1);
        tiny.r(0, 0) = tiny.g(0, 0) = tiny.b(0, 0) = 32768;
        tiny.setOutputProfile(icc);
        require(tiny.saveAsHEIF((directory / ("tiny-" + std::to_string(bps) + ".heic")).u8string(),
            100, bps, 1) == IMIO_SUCCESS, "tiny output");
    }
    std::vector<std::future<void>> workers;
    for (int worker = 0; worker < 4; ++worker) {
        workers.push_back(std::async(std::launch::async, [&, worker] {
            rtengine::Image16 tiny(1, 1);
            tiny.r(0, 0) = tiny.g(0, 0) = tiny.b(0, 0) = 32768;
            tiny.setOutputProfile(icc);
            const auto output = directory / "concurrent" / std::to_string(worker);
            std::filesystem::create_directories(output);
            for (int repeat = 0; repeat < 4; ++repeat) {
                const auto path = output / ("tiny-10-" + std::to_string(repeat) + ".heic");
                require(tiny.saveAsHEIF(path.u8string()) == IMIO_SUCCESS, "concurrent export");
                require(read(path).substr(4, 4) == "ftyp", "concurrent container");
            }
        }));
    }
    for (auto& worker : workers) {
        worker.get();
    }
    std::cout << "PASS four concurrent exporters with repeated init/deinit\n";
    const auto protectedPath = directory / "protected.heic";
    write(protectedPath, "preserve existing file");
    for (const auto args : std::array<std::array<int, 3>, 8>{{
        {{-1, 10, 1}}, {{101, 10, 1}}, {{92, 9, 1}}, {{92, 16, 1}},
        {{92, 10, 0}}, {{92, 10, 4}}, {{92, 0, 1}}, {{92, 10, -1}}}}) {
        require(imagefloat.saveAsHEIF(protectedPath.u8string(), args[0], args[1], args[2]) == IMIO_VARIANTNOTSUPPORTED,
            "invalid arguments accepted");
        require(read(protectedPath) == "preserve existing file", "invalid arguments changed existing output");
    }
    rtengine::Imagefloat empty;
    require(empty.saveAsHEIF(protectedPath.u8string()) == IMIO_HEADERERROR, "empty image accepted");
    require(read(protectedPath) == "preserve existing file", "empty image changed output");
    require(imagefloat.saveAsHEIF((directory / "absent" / "output.heic").u8string()) == IMIO_CANNOTWRITEFILE, "absent parent");
    const auto targetDirectory = directory / "existing-directory.heic";
    std::filesystem::create_directories(targetDirectory);
    write(targetDirectory / "keep", "preserve directory");
    require(imagefloat.saveAsHEIF(targetDirectory.u8string()) == IMIO_CANNOTWRITEFILE, "directory destination accepted");
    require(read(targetDirectory / "keep") == "preserve directory", "failed publication changed directory");
    const auto unicode = directory / std::filesystem::u8path("České fotografie");
    std::filesystem::create_directories(unicode);
    require(imagefloat.saveAsHEIF((unicode / std::filesystem::u8path("Žluťoučký.heif")).u8string()) == IMIO_SUCCESS, "Unicode output");
    require(imagefloat.saveAsHEIF((unicode / (std::string(250, 'x') + ".heif")).u8string()) == IMIO_SUCCESS,
        "255-character output basename");
#ifdef __unix__
    // A real kernel write failure exercises both fwrite and buffered close
    // error handling. Keep the preexisting destination intact and clean up.
    struct rlimit original{};
    require(getrlimit(RLIMIT_FSIZE, &original) == 0, "get file limit");
    const auto oldSignal = std::signal(SIGXFSZ, SIG_IGN);
    struct rlimit limited = original;
    limited.rlim_cur = 1;
    require(setrlimit(RLIMIT_FSIZE, &limited) == 0, "set file limit");
    const int writeResult = imagefloat.saveAsHEIF(protectedPath.u8string());
    const std::string closeError = listener.errorMessage;
    noise.setSaveProgressListener(&listener);
    const int callbackResult = noise.saveAsHEIF(protectedPath.u8string(), 100, 10, 3);
    const std::string callbackError = listener.errorMessage;
    const int restoreResult = setrlimit(RLIMIT_FSIZE, &original);
    std::signal(SIGXFSZ, oldSignal);
    require(restoreResult == 0, "restore file limit");
    require(writeResult == IMIO_CANNOTWRITEFILE && closeError.find("cannot close HEIF output") != std::string::npos,
        "buffered close failure lost");
    require(callbackResult == IMIO_CANNOTWRITEFILE && callbackError.find("file write failed") != std::string::npos,
        "callback write failure lost");
    require(read(protectedPath) == "preserve existing file", "write failure destroyed destination");
    std::cout << "PASS kernel callback and buffered close failures preserved destination\n";
#endif
    require(imagefloat.saveAsHEIF(protectedPath.u8string()) == IMIO_SUCCESS, "overwrite output");
    require(read(protectedPath).substr(4, 4) == "ftyp", "overwrite did not publish HEIF");
    for (const auto& path : std::filesystem::recursive_directory_iterator(directory)) {
        require(path.path().extension() == ".heic" || path.path().extension() == ".heif" ||
            path.path().extension() == ".icc" || path.path().filename() == "keep" || path.is_directory(), "temporary output leaked");
    }
#endif
    std::cout << "PASS engine export checks\n";
}
} // namespace
int main(int argc, char** argv)
{
    try {
#ifdef G_OS_WIN32
        std::unique_ptr<gchar*, decltype(&g_strfreev)> utf8Args(g_win32_get_command_line(), g_strfreev);
        require(bool(utf8Args), "convert Windows arguments");
        argv = utf8Args.get();
        argc = g_strv_length(argv);
#endif
        require(argc == 2 || argc == 3, "expected output directory and optional codec mode");
        const std::string mode = argc == 3 ? argv[2] : "";
        require(mode.empty() || mode == "--missing-encoder" || mode == "--codec-smoke", "unknown test mode");
        run(std::filesystem::u8path(argv[1]), mode);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
