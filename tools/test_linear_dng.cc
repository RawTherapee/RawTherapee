// Pixel-level companion to test_linear_dng.py. Uses the production RAW loader.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glibmm/init.h>

#include "rtengine/rawimage.h"
#include "rtengine/settings.h"
#include "rtgui/threadutils.h"

namespace rtengine {
extern MyMutex* librawMutex;
}

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

unsigned read16(std::istream& stream)
{
    unsigned char bytes[2];
    stream.read(reinterpret_cast<char*>(bytes), 2);
    require(bool(stream), "Truncated expected pixel data");
    return unsigned(bytes[0]) | unsigned(bytes[1]) << 8;
}

class TestRawImage : public rtengine::RawImage {
public:
    using RawImage::RawImage;

    const char* decoder_name() const
    {
        switch (decoder) {
            case Decoder::DCRAW: return "dcraw";
            case Decoder::LIBRAW: return "libraw";
        }
        return "unknown";
    }
};

// Native Foveon buffers keep the stored row stride even when iwidth is smaller.
class FoveonImage : public rtengine::RawImage {
public:
    FoveonImage() : RawImage("")
    {
        std::strcpy(make, "Sigma");
        is_foveon = 1;
        filters = dng_version = 0;
        colors = 3;
        raw_width = 64;
        raw_height = 48;
        width = iwidth = 54;
        height = iheight = 40;
        left_margin = 5;
        top_margin = 3;
        image = static_cast<dcrawImage_t>(std::calloc(raw_width * raw_height, sizeof(*image)));
        require(image != nullptr, "Allocation failed");
        for (unsigned y = 0; y < raw_height; ++y) {
            for (unsigned x = 0; x < raw_width; ++x) {
                for (unsigned c = 0; c < 3; ++c) {
                    image[y * raw_width + x][c] = y * 100 + x + c;
                }
            }
        }
    }

    void check()
    {
        compress_image(0);
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                for (unsigned c = 0; c < 3; ++c) {
                    require(data[y][3 * x + c] == (y + 3) * 100 + x + 5 + c,
                            "Native Foveon row stride changed");
                }
            }
        }
    }
};

} // namespace

int main(int argc, char** argv)
{
    try {
        require(argc == 4, "Usage: test-linear-dng dcraw|libraw input.dng expected.bin");
        const std::string requested_decoder = argv[1];
        require(requested_decoder == "dcraw" || requested_decoder == "libraw", "Unknown decoder");
        Glib::init();
        rtengine::Settings config{};
        config.enableLibRaw = requested_decoder == "libraw";
        rtengine::settings = &config;
        MyMutex mutex;
        rtengine::librawMutex = &mutex;
        FoveonImage().check();

        std::ifstream expected(argv[3], std::ios::binary);
        const unsigned width = read16(expected);
        const unsigned height = read16(expected);
        std::vector<unsigned> pixels(width * height * 3);
        for (auto& value : pixels) {
            value = read16(expected);
        }

        TestRawImage raw(argv[2]);
        const int error = raw.loadRaw(true);
        if (error) {
            throw std::runtime_error("RAW loading failed: " + std::to_string(error));
        }
        const char* actual_decoder = raw.decoder_name();
        if (requested_decoder != actual_decoder) {
            throw std::runtime_error("Expected decoder " + requested_decoder + ", got " + actual_decoder);
        }
        require(raw.get_width() == int(width) && raw.get_height() == int(height),
                "Wrong active-area dimensions");
        require(raw.get_iwidth() == int(width) && raw.get_iheight() == int(height),
                "Wrong decoded image stride");
        require(raw.get_leftmargin() == 0 && raw.get_topmargin() == 0,
                "Crop offsets were not consumed");
        const auto image = raw.get_image();
        require(image != nullptr, "Missing integer RGB image");
        for (unsigned i = 0; i < width * height; ++i) {
            for (unsigned c = 0; c < 3; ++c) {
                require(image[i][c] == pixels[3 * i + c], "Wrong decoded active-area pixel");
            }
        }

        raw.compress_image(0);
        require(raw.get_width() == int(width) && raw.get_height() == int(height),
                "compress_image cropped the image again");
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                for (unsigned c = 0; c < 3; ++c) {
                    require(raw.data[y][3 * x + c] == pixels[3 * (y * width + x) + c],
                            "Wrong compressed active-area pixel");
                }
            }
        }
        std::cout << actual_decoder << ": " << width << 'x' << height
                  << " active-area pixels and native Foveon stride verified\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
