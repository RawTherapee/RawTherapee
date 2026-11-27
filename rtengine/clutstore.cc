#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <glib/gstdio.h>
#include <png.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "clutstore.h"

#include "colortemp.h"
#include "iccstore.h"
#include "imagefloat.h"
#include "procparams.h"
#include "stdimagesource.h"

#include "rtgui/options.h"

namespace
{

// ---------------------------------------------------------------------------
// SSE helpers used by CLUT interpolation
// ---------------------------------------------------------------------------

#if defined(__SSE2__) || defined(RT_SIMDE)
vfloat2 getHaldClutValues(const AlignedBuffer<std::uint16_t>& clut_image, size_t index)
{
    const vint v_values = _mm_loadu_si128(reinterpret_cast<const vint*>(clut_image.data + index));
#ifdef __SSE4_1__
    return {
        _mm_cvtepi32_ps(_mm_cvtepu16_epi32(v_values)),
        _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_srli_si128(v_values, 8)))
    };
#else
    const vint v_mask = _mm_set1_epi32(0x0000FFFF);

    vint v_low = _mm_shuffle_epi32(v_values, _MM_SHUFFLE(1, 0, 1, 0));
    vint v_high = _mm_shuffle_epi32(v_values, _MM_SHUFFLE(3, 2, 3, 2));
    v_low = _mm_shufflelo_epi16(v_low, _MM_SHUFFLE(1, 1, 0, 0));
    v_high = _mm_shufflelo_epi16(v_high, _MM_SHUFFLE(1, 1, 0, 0));
    v_low = _mm_shufflehi_epi16(v_low, _MM_SHUFFLE(3, 3, 2, 2));
    v_high = _mm_shufflehi_epi16(v_high, _MM_SHUFFLE(3, 3, 2, 2));
    v_low = vandm(v_low, v_mask);
    v_high = vandm(v_high, v_mask);

    return {
        _mm_cvtepi32_ps(v_low),
        _mm_cvtepi32_ps(v_high)
    };
#endif
}

vfloat getCubeClutValue(const AlignedBuffer<float>& clut_image, size_t index)
{
    return LVF(clut_image.data[index]);
}
#endif

// ---------------------------------------------------------------------------
// HaldCLUT loading helper (image-based: PNG / TIFF)
// ---------------------------------------------------------------------------

bool loadHaldFile(
    const Glib::ustring& filename,
    const Glib::ustring& working_color_space,
    AlignedBuffer<std::uint16_t>& clut_image,
    unsigned int& clut_level
)
{
    rtengine::StdImageSource img_src;

    if (!Glib::file_test(filename, Glib::FILE_TEST_EXISTS) || img_src.load(filename)) {
        return false;
    }

    int fw, fh;
    img_src.getFullSize(fw, fh, TR_NONE);

    bool res = false;

    if (fw == fh) {
        int level = 1;

        while (level * level * level < fw) {
            ++level;
        }

        if (level * level * level == fw && level > 1) {
            clut_level = level;
            res = true;
        }
    }

    if (res) {
        rtengine::ColorTemp curr_wb = img_src.getWB();
        std::unique_ptr<rtengine::Imagefloat> img_float = std::unique_ptr<rtengine::Imagefloat>(new rtengine::Imagefloat(fw, fh));
        const PreviewProps pp(0, 0, fw, fh, 1);

        rtengine::procparams::ColorManagementParams icm;
        icm.workingProfile = working_color_space;

        img_src.getImage(curr_wb, TR_NONE, img_float.get(), pp, rtengine::procparams::ToneCurveParams(), rtengine::procparams::RAWParams());

        if (!working_color_space.empty()) {
            img_src.convertColorSpace(img_float.get(), icm, curr_wb);
        }

        AlignedBuffer<std::uint16_t> image(fw * fh * 4 + 4); // getClutValues() loads one pixel in advance

        std::size_t index = 0;

        for (int y = 0; y < fh; ++y) {
            for (int x = 0; x < fw; ++x) {
                image.data[index] = img_float->r(y, x);
                ++index;
                image.data[index] = img_float->g(y, x);
                ++index;
                image.data[index] = img_float->b(y, x);
                index += 2;
            }
        }

        clut_image.swap(image);
    }

    return res;
}

} // anonymous namespace

// ===========================================================================
// CLUT — shared base implementation
// ===========================================================================

rtengine::CLUT::operator bool() const
{
    return clut_level > 0;
}

Glib::ustring rtengine::CLUT::getFilename() const
{
    return clut_filename;
}

Glib::ustring rtengine::CLUT::getProfile() const
{
    return clut_profile;
}

void rtengine::HaldCLUT::getRGB(
    float strength,
    std::size_t line_size,
    const float* r,
    const float* g,
    const float* b,
    float* out_rgbx
) const
{
    const unsigned int level = clut_level;
    const unsigned int level_square = level * level;

#if defined(__SSE2__) || defined(RT_SIMDE)
    const vfloat v_strength = F2V(strength);
#endif

    for (std::size_t column = 0; column < line_size; ++column, ++r, ++g, ++b, out_rgbx += 4) {
        const unsigned int red = std::min(flevel_minus_two, *r * flevel_minus_one);
        const unsigned int green = std::min(flevel_minus_two, *g * flevel_minus_one);
        const unsigned int blue = std::min(flevel_minus_two, *b * flevel_minus_one);

        const unsigned int color = red + green * level + blue * level_square;

#if ! defined(__SSE2__) && ! defined(RT_SIMDE)
        const float re = *r * flevel_minus_one - red;
        const float gr = *g * flevel_minus_one - green;
        const float bl = *b * flevel_minus_one - blue;

        size_t index = color * 4;

        float tmp1[4] ALIGNED16;
        tmp1[0] = intp<float>(re, clut_image.data[index + 4], clut_image.data[index]);
        tmp1[1] = intp<float>(re, clut_image.data[index + 5], clut_image.data[index + 1]);
        tmp1[2] = intp<float>(re, clut_image.data[index + 6], clut_image.data[index + 2]);

        index = (color + level) * 4;

        float tmp2[4] ALIGNED16;
        tmp2[0] = intp<float>(re, clut_image.data[index + 4], clut_image.data[index]);
        tmp2[1] = intp<float>(re, clut_image.data[index + 5], clut_image.data[index + 1]);
        tmp2[2] = intp<float>(re, clut_image.data[index + 6], clut_image.data[index + 2]);

        out_rgbx[0] = intp<float>(gr, tmp2[0], tmp1[0]);
        out_rgbx[1] = intp<float>(gr, tmp2[1], tmp1[1]);
        out_rgbx[2] = intp<float>(gr, tmp2[2], tmp1[2]);

        index = (color + level_square) * 4;

        tmp1[0] = intp<float>(re, clut_image.data[index + 4], clut_image.data[index]);
        tmp1[1] = intp<float>(re, clut_image.data[index + 5], clut_image.data[index + 1]);
        tmp1[2] = intp<float>(re, clut_image.data[index + 6], clut_image.data[index + 2]);

        index = (color + level + level_square) * 4;

        tmp2[0] = intp<float>(re, clut_image.data[index + 4], clut_image.data[index]);
        tmp2[1] = intp<float>(re, clut_image.data[index + 5], clut_image.data[index + 1]);
        tmp2[2] = intp<float>(re, clut_image.data[index + 6], clut_image.data[index + 2]);

        tmp1[0] = intp<float>(gr, tmp2[0], tmp1[0]);
        tmp1[1] = intp<float>(gr, tmp2[1], tmp1[1]);
        tmp1[2] = intp<float>(gr, tmp2[2], tmp1[2]);

        out_rgbx[0] = intp<float>(bl, tmp1[0], out_rgbx[0]);
        out_rgbx[1] = intp<float>(bl, tmp1[1], out_rgbx[1]);
        out_rgbx[2] = intp<float>(bl, tmp1[2], out_rgbx[2]);

        out_rgbx[0] = intp<float>(strength, out_rgbx[0], *r);
        out_rgbx[1] = intp<float>(strength, out_rgbx[1], *g);
        out_rgbx[2] = intp<float>(strength, out_rgbx[2], *b);
#else
        const vfloat v_in = _mm_set_ps(0.0f, *b, *g, *r);
        const vfloat v_tmp = v_in * F2V(flevel_minus_one);
        const vfloat v_rgb = v_tmp - _mm_cvtepi32_ps(_mm_cvttps_epi32(vminf(v_tmp, F2V(flevel_minus_two))));

        size_t index = color * 4;

        const vfloat v_r = PERMUTEPS(v_rgb, _MM_SHUFFLE(0, 0, 0, 0));

        vfloat2 v_clut_values = getHaldClutValues(clut_image, index);
        vfloat v_tmp1 = vintpf(v_r, v_clut_values.y, v_clut_values.x);

        index = (color + level) * 4;

        v_clut_values = getHaldClutValues(clut_image, index);
        vfloat v_tmp2 = vintpf(v_r, v_clut_values.y, v_clut_values.x);

        const vfloat v_g = PERMUTEPS(v_rgb, _MM_SHUFFLE(1, 1, 1, 1));

        vfloat v_out = vintpf(v_g, v_tmp2, v_tmp1);

        index = (color + level_square) * 4;

        v_clut_values = getHaldClutValues(clut_image, index);
        v_tmp1 = vintpf(v_r, v_clut_values.y, v_clut_values.x);

        index = (color + level + level_square) * 4;

        v_clut_values = getHaldClutValues(clut_image, index);
        v_tmp2 = vintpf(v_r, v_clut_values.y, v_clut_values.x);

        v_tmp1 = vintpf(v_g, v_tmp2, v_tmp1);

        const vfloat v_b = PERMUTEPS(v_rgb, _MM_SHUFFLE(2, 2, 2, 2));

        v_out = vintpf(v_b, v_tmp1, v_out);

        STVF(*out_rgbx, vintpf(v_strength, v_out, v_in));
#endif
    }
}

// ===========================================================================
// HaldCLUT — image-based (PNG / TIFF) Hald CLUT
// ===========================================================================

bool rtengine::HaldCLUT::load(const Glib::ustring& filename)
{
    if (loadHaldFile(filename, "", clut_image, clut_level)) {
        Glib::ustring name, ext;
        splitClutFilename(filename, name, ext, clut_profile);

        clut_filename = filename;
        clut_level *= clut_level;
        flevel_minus_one = static_cast<float>(clut_level - 1) / 65535.0f;
        flevel_minus_two = static_cast<float>(clut_level - 2);
        return true;
    }

    return false;
}

Glib::ustring rtengine::HaldCLUT::createIdentityTempFile(int level)
{
    const int cube    = level * level;          // samples per axis
    const int cube_sq = cube * cube;
    const int size    = level * level * level;  // image side length
    const float den   = static_cast<float>(cube - 1);

    Imagefloat img(size, size);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int idx   = y * size + x;
            const int b_idx = idx / cube_sq;
            const int rem   = idx % cube_sq;
            const int g_idx = rem / cube;
            const int r_idx = rem % cube;

            img.r(y, x) = r_idx * 65535.0f / den;
            img.g(y, x) = g_idx * 65535.0f / den;
            img.b(y, x) = b_idx * 65535.0f / den;
        }
    }

    // Unique temp path — avoids collisions across simultaneous instances.
    // NOTE: Glib::file_open_tmp() takes a *prefix*, not an mkstemp template —
    // glibmm appends "XXXXXX" itself.  Passing "..._XXXXXX.png" produced
    // "..._XXXXXX.pngAB12CD", i.e. no .png extension, and RT then failed to
    // load the identity image.  Reserve a unique stem, add the suffix here.
    std::string tmpName;
    const int fd = Glib::file_open_tmp(tmpName, "rt_hald_identity_");
    if (fd < 0) {
        return {};
    }
    g_close(fd, nullptr);
    g_unlink(tmpName.c_str());

    // ponytail: tiny TOCTOU window between unlink and saveAsPNG; the random
    // stem makes a collision practically impossible.
    const Glib::ustring tmpPath(tmpName + ".png");
    if (img.saveAsPNG(tmpPath, 16) != 0) {
        g_unlink(tmpPath.c_str());
        return {};
    }

    return tmpPath;
}

bool rtengine::HaldCLUT::saveAsHaldFile(const IImagefloat* img,
                                         const Glib::ustring& destPath)
{
    // Hald CLUT image must be square (side = level^3).
    if (!img || img->getWidth() != img->getHeight() || img->getWidth() <= 0) {
        return false;
    }
    const int size = img->getWidth();

    FILE* file = g_fopen(destPath.c_str(), "wb");
    if (!file) {
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              nullptr, nullptr, nullptr);
    if (!png) {
        fclose(file);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        fclose(file);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(file);
        return false;
    }

    png_init_io(png, file);
    png_set_IHDR(png, info, size, size, 16, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_BASE);
    // gAMA=1.0 forces RT's loader (imageio.cc) to skip gamma decoding so the raw
    // 16-bit values reach the CLUT lookup unchanged. NOTE: file content is
    // actually display-referred sRGB (matching the .cube flow), so the gAMA tag
    // is intentionally inconsistent with the data — this PNG is meant for RT
    // round-tripping, not for use as a portable HALD CLUT in external software.
    png_set_gAMA(png, info, 1.0);
    png_write_info(png, info);
    png_set_swap(png);  // host byte order → big-endian required by PNG 16-bit

    std::vector<uint16_t> row(size * 3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            row[x * 3 + 0] = static_cast<uint16_t>(
                std::max(0.f, std::min(65535.f, img->r(y, x))));
            row[x * 3 + 1] = static_cast<uint16_t>(
                std::max(0.f, std::min(65535.f, img->g(y, x))));
            row[x * 3 + 2] = static_cast<uint16_t>(
                std::max(0.f, std::min(65535.f, img->b(y, x))));
        }
        png_write_row(png, reinterpret_cast<png_bytep>(row.data()));
    }

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(file);
    return true;
}

void rtengine::HaldCLUT::splitClutFilename(
    const Glib::ustring& filename,
    Glib::ustring& name,
    Glib::ustring& extension,
    Glib::ustring& profile_name,
    bool checkProfile
)
{
    Glib::ustring basename = Glib::path_get_basename(filename);

    const Glib::ustring::size_type last_dot_pos = basename.rfind('.');

    if (last_dot_pos != Glib::ustring::npos) {
        name.assign(basename, 0, last_dot_pos);
        extension.assign(basename, last_dot_pos + 1, Glib::ustring::npos);
    } else {
        name = basename;
    }

    if (checkProfile) {
        profile_name = "sRGB";

        if (!name.empty()) {
            for (const auto& working_profile : rtengine::ICCStore::getInstance()->getWorkingProfiles()) {
                if (
                    !working_profile.empty()
                    && std::search(name.rbegin(), name.rend(), working_profile.rbegin(), working_profile.rend()) == name.rbegin()
                ) {
                    profile_name = working_profile;
                    name.erase(name.size() - working_profile.size());
                    break;
                }
            }
        }
    }
}

// ===========================================================================
// CubeLUT — text-based .cube 3D LUT (Adobe / DaVinci Resolve format)
// ===========================================================================

bool rtengine::CubeLUT::load(const Glib::ustring& filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        return false;
    }

    int size = 0;
    float domain_min[3] = {0.f, 0.f, 0.f};
    float domain_max[3] = {1.f, 1.f, 1.f};
    std::vector<std::array<float, 3>> entries;
    std::size_t entry_limit = 0;

    std::string line;
    while (std::getline(file, line)) {
        // Strip Windows-style carriage return
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // Strip leading whitespace so keyword detection works on indented lines
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) {
            continue;
        }
        if (first > 0) {
            line.erase(0, first);
        }
        // Skip comments
        if (line[0] == '#') {
            continue;
        }

        if (line.rfind("LUT_3D_SIZE", 0) == 0) {
            std::istringstream ss(line.substr(11));
            size = 0;
            ss >> size;
            entry_limit = 0;
            if (size >= 2 && size <= 256) {
                const std::size_t cube_size = static_cast<std::size_t>(size);
                entry_limit = cube_size * cube_size * cube_size;
                entries.reserve(entry_limit);
            }
        } else if (line.rfind("DOMAIN_MIN", 0) == 0) {
            std::istringstream ss(line.substr(10));
            ss >> domain_min[0] >> domain_min[1] >> domain_min[2];
        } else if (line.rfind("DOMAIN_MAX", 0) == 0) {
            std::istringstream ss(line.substr(10));
            ss >> domain_max[0] >> domain_max[1] >> domain_max[2];
        } else if (line.rfind("TITLE", 0) == 0
                   || line.rfind("LUT_1D_SIZE", 0) == 0
                   || line.rfind("LUT_1D_INPUT_TABLE", 0) == 0
                   || line.rfind("LUT_3D_INPUT_TABLE", 0) == 0
                   || line.rfind("LUT_3D_INPUT_RANGE", 0) == 0
                   || line.rfind("LUT_1D_INPUT_RANGE", 0) == 0) {
            // header-only keywords — nothing to do
        } else {
            float r, g, b;
            std::istringstream ss(line);
            if (ss >> r >> g >> b) {
                if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
                    return false;
                }

                if (entry_limit == 0 || entries.size() >= entry_limit) {
                    return false;
                }

                entries.push_back({r, g, b});
            }
        }
    }

    for (int c = 0; c < 3; ++c) {
        if (
            !std::isfinite(domain_min[c])
            || !std::isfinite(domain_max[c])
            || !(domain_min[c] < domain_max[c])
        ) {
            return false;
        }
    }

    if (size < 2 || size > 256) {
        return false;
    }

    const std::size_t total = entry_limit;
    if (entries.size() != total) {
        return false;
    }

    clut_level = size;

    AlignedBuffer<float> image(total * 4);
    if (image.isEmpty()) {
        return false;
    }

    for (std::size_t i = 0; i < total; ++i) {
        for (int c = 0; c < 3; ++c) {
            // DOMAIN_MIN and DOMAIN_MAX describe input coordinates, not LUT output values.
            const float value = entries[i][c] * MAXVALF;
            if (!std::isfinite(value)) {
                return false;
            }
            image.data[i * 4 + c] = value;
        }
        image.data[i * 4 + 3] = 0;
    }

    clut_image.swap(image);

    // Determine colour profile from filename suffix (same convention as HaldCLUT)
    Glib::ustring name, ext;
    HaldCLUT::splitClutFilename(filename, name, ext, clut_profile);

    clut_filename = filename;
    flevel_minus_one = static_cast<float>(clut_level - 1) / 65535.0f;
    flevel_minus_two = static_cast<float>(clut_level - 2);

    for (int c = 0; c < 3; ++c) {
        const float range = domain_max[c] - domain_min[c];
        domain_scale[c] = flevel_minus_one / range;
        domain_offset[c] = -domain_min[c] * static_cast<float>(clut_level - 1) / range;
    }

    return true;
}

Glib::ustring rtengine::CubeLUT::createIdentityTempFile(int size)
{
    if (size < 2) {
        return {};
    }

    // Layout: width = size*size, height = size.
    // Pixel (y=b, x=g*size+r) encodes input colour (r/(size-1), g/(size-1), b/(size-1)).
    const float den = static_cast<float>(size - 1);
    Imagefloat img(size * size, size);

    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                const int x = g * size + r;
                img.r(b, x) = r * 65535.f / den;
                img.g(b, x) = g * 65535.f / den;
                img.b(b, x) = b * 65535.f / den;
            }
        }
    }

    // Unique temp path — avoids collisions across simultaneous instances.
    // NOTE: Glib::file_open_tmp() takes a *prefix*, not an mkstemp template —
    // glibmm appends "XXXXXX" itself.  Passing "..._XXXXXX.png" produced
    // "..._XXXXXX.pngAB12CD", i.e. no .png extension, and RT then failed to
    // load the identity image.  Reserve a unique stem, add the suffix here.
    std::string tmpName;
    const int fd = Glib::file_open_tmp(tmpName, "rt_cube_identity_");
    if (fd < 0) {
        return {};
    }
    g_close(fd, nullptr);
    g_unlink(tmpName.c_str());

    // ponytail: tiny TOCTOU window between unlink and saveAsPNG; the random
    // stem makes a collision practically impossible.
    const Glib::ustring tmpPath(tmpName + ".png");
    if (img.saveAsPNG(tmpPath, 16) != 0) {
        g_unlink(tmpPath.c_str());
        return {};
    }

    return tmpPath;
}

bool rtengine::CubeLUT::saveAsCubeFile(const IImagefloat* img, int size,
                                        const Glib::ustring& destPath)
{
    // Identity layout: width = size*size, height = size.
    if (!img || size < 2
        || img->getHeight() != size || img->getWidth() != size * size) {
        return false;
    }

    std::ofstream file(destPath.c_str());
    if (!file.is_open()) {
        return false;
    }

    file << "TITLE \"RawTherapee Export\"\n\n";
    file << "LUT_3D_SIZE " << size << "\n\n";
    file << "DOMAIN_MIN 0.0 0.0 0.0\n";
    file << "DOMAIN_MAX 1.0 1.0 1.0\n\n";
    file << std::fixed << std::setprecision(6);

    // .cube order: R fastest, then G, then B.
    // Our image layout: pixel (y=b, x=g*size+r) → entry for input (r, g, b).
    const float scale = 1.f / 65535.f;
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                const int x = g * size + r;
                const float rv = std::max(0.f, std::min(1.f, img->r(b, x) * scale));
                const float gv = std::max(0.f, std::min(1.f, img->g(b, x) * scale));
                const float bv = std::max(0.f, std::min(1.f, img->b(b, x) * scale));
                file << rv << ' ' << gv << ' ' << bv << '\n';
            }
        }
    }

    return file.good();
}

void rtengine::CubeLUT::getRGB(
    float strength,
    std::size_t line_size,
    const float* r,
    const float* g,
    const float* b,
    float* out_rgbx
) const
{
    const unsigned int level = clut_level;
    const unsigned int level_square = level * level;
    const unsigned int last_offset = 1 + level + level_square;
    const float level_minus_one = static_cast<float>(level - 1);

#if defined(__SSE2__) || defined(RT_SIMDE)
    const vfloat v_strength = F2V(strength);
#endif

    for (std::size_t column = 0; column < line_size; ++column, ++r, ++g, ++b, out_rgbx += 4) {
        const float scaled_red = std::max(0.f, std::min(level_minus_one, *r * domain_scale[0] + domain_offset[0]));
        const float scaled_green = std::max(0.f, std::min(level_minus_one, *g * domain_scale[1] + domain_offset[1]));
        const float scaled_blue = std::max(0.f, std::min(level_minus_one, *b * domain_scale[2] + domain_offset[2]));

        const unsigned int red = std::min(flevel_minus_two, scaled_red);
        const unsigned int green = std::min(flevel_minus_two, scaled_green);
        const unsigned int blue = std::min(flevel_minus_two, scaled_blue);

        const float re = scaled_red - red;
        const float gr = scaled_green - green;
        const float bl = scaled_blue - blue;

        unsigned int first_offset;
        unsigned int second_offset;
        float first_fraction;
        float second_fraction;
        float third_fraction;

        if (re >= gr) {
            if (gr >= bl) { // r >= g >= b
                first_offset = 1;
                second_offset = 1 + level;
                first_fraction = re;
                second_fraction = gr;
                third_fraction = bl;
            } else if (re >= bl) { // r >= b > g
                first_offset = 1;
                second_offset = 1 + level_square;
                first_fraction = re;
                second_fraction = bl;
                third_fraction = gr;
            } else { // b > r >= g
                first_offset = level_square;
                second_offset = level_square + 1;
                first_fraction = bl;
                second_fraction = re;
                third_fraction = gr;
            }
        } else if (re >= bl) { // g > r >= b
            first_offset = level;
            second_offset = level + 1;
            first_fraction = gr;
            second_fraction = re;
            third_fraction = bl;
        } else if (gr >= bl) { // g >= b > r
            first_offset = level;
            second_offset = level + level_square;
            first_fraction = gr;
            second_fraction = bl;
            third_fraction = re;
        } else { // b > g > r
            first_offset = level_square;
            second_offset = level_square + level;
            first_fraction = bl;
            second_fraction = gr;
            third_fraction = re;
        }

        const unsigned int color = red + green * level + blue * level_square;
        const std::size_t first_index = color * 4;
        const std::size_t second_index = (color + first_offset) * 4;
        const std::size_t third_index = (color + second_offset) * 4;
        const std::size_t last_index = (color + last_offset) * 4;

#if ! defined(__SSE2__) && ! defined(RT_SIMDE)
        const float input[3] = {*r, *g, *b};

        for (int channel = 0; channel < 3; ++channel) {
            const float first = clut_image.data[first_index + channel];
            const float second = clut_image.data[second_index + channel];
            const float third = clut_image.data[third_index + channel];
            const float last = clut_image.data[last_index + channel];
            const float value =
                first
                + first_fraction * (second - first)
                + second_fraction * (third - second)
                + third_fraction * (last - third);

            out_rgbx[channel] = intp<float>(strength, value, input[channel]);
        }
#else
        const vfloat v_first = getCubeClutValue(clut_image, first_index);
        const vfloat v_second = getCubeClutValue(clut_image, second_index);
        const vfloat v_third = getCubeClutValue(clut_image, third_index);
        const vfloat v_last = getCubeClutValue(clut_image, last_index);

        vfloat v_value = v_first + F2V(first_fraction) * (v_second - v_first);
        v_value = v_value + F2V(second_fraction) * (v_third - v_second);
        v_value = v_value + F2V(third_fraction) * (v_last - v_third);

        const vfloat v_input = _mm_set_ps(0.0f, *b, *g, *r);
        STVF(*out_rgbx, vintpf(v_strength, v_value, v_input));
#endif
    }
}

// ===========================================================================
// CLUTStore — factory + LRU cache
// ===========================================================================

rtengine::CLUTStore& rtengine::CLUTStore::getInstance()
{
    static CLUTStore instance;
    return instance;
}

std::shared_ptr<rtengine::CLUT> rtengine::CLUTStore::getClut(const Glib::ustring& filename) const
{
    std::shared_ptr<rtengine::CLUT> result;

    const Glib::ustring full_filename =
        !Glib::path_is_absolute(filename)
            ? Glib::ustring(Glib::build_filename(App::get().options().clutsDir, filename))
            : filename;

    if (!cache.get(full_filename, result)) {
        // Choose concrete class from file extension
        Glib::ustring name, ext, dummy;
        HaldCLUT::splitClutFilename(full_filename, name, ext, dummy, false);
        ext = ext.casefold();

        std::unique_ptr<CLUT> clut;
        if (ext == "cube") {
            clut.reset(new CubeLUT());
        } else {
            clut.reset(new HaldCLUT());
        }

        if (clut->load(full_filename)) {
            result = std::move(clut);
            cache.insert(full_filename, result);
        }
    }

    return result;
}

void rtengine::CLUTStore::clearCache()
{
    cache.clear();
}

rtengine::CLUTStore::CLUTStore() :
    cache(App::get().options().clutCacheSize)
{
}
