#pragma once

#include <memory>
#include <cstdint>

#include <glibmm/ustring.h>

#include "cache.h"
#include "alignedbuffer.h"
#include "noncopyable.h"

namespace rtengine
{

/**
 * Abstract base class for 3D colour look-up tables used by Film Simulation.
 *
 * Concrete subclasses implement loading and interpolation for their respective
 * file formats and own their format-specific storage.
 */
class CLUT3D :
    public NonCopyable
{
public:
    virtual ~CLUT3D() = default;

    virtual bool load(const Glib::ustring& filename) = 0;

    explicit operator bool() const;

    Glib::ustring getFilename() const;
    Glib::ustring getProfile() const;

    virtual void getRGB(
        float strength,
        std::size_t line_size,
        const float* r,
        const float* g,
        const float* b,
        float* out_rgbx
    ) const = 0;

protected:
    unsigned int clut_level = 0;
    float flevel_minus_one = 0.f;
    float flevel_minus_two = 0.f;
    Glib::ustring clut_filename;
    Glib::ustring clut_profile = "sRGB";
};

/**
 * Hald CLUT — loads square PNG / TIFF image files where the image dimensions
 * encode the cube size as width == height == level³, and uses trilinear
 * interpolation.
 */
class HaldCLUT final :
    public CLUT3D
{
public:
    bool load(const Glib::ustring& filename) override;

    void getRGB(
        float strength,
        std::size_t line_size,
        const float* r,
        const float* g,
        const float* b,
        float* out_rgbx
    ) const override;

    /** Split a CLUT filename into name, extension and optional ICC profile. */
    static void splitClutFilename(
        const Glib::ustring& filename,
        Glib::ustring& name,
        Glib::ustring& extension,
        Glib::ustring& profile_name,
        bool checkProfile = true
    );

private:
    AlignedBuffer<std::uint16_t> clut_image;
};

/**
 * Cube LUT — loads text-based .cube files (Adobe / DaVinci Resolve format)
 * and uses tetrahedral interpolation.
 * Supports LUT_3D_SIZE, DOMAIN_MIN / DOMAIN_MAX and comment lines.
 * The colour profile defaults to sRGB; like HaldCLUT, a suffix in the
 * filename can override it (e.g. "MyLUT_ProPhoto.cube").
 */
class CubeLUT final :
    public CLUT3D
{
public:
    bool load(const Glib::ustring& filename) override;

    void getRGB(
        float strength,
        std::size_t line_size,
        const float* r,
        const float* g,
        const float* b,
        float* out_rgbx
    ) const override;

private:
    AlignedBuffer<float> clut_image;
    float domain_scale[3] = {};
    float domain_offset[3] = {};
};

class CLUTStore final :
    public NonCopyable
{
public:
    static CLUTStore& getInstance();

    /** Returns a CLUT3D for the given filename, creating and caching it on
     *  first access.  The concrete type (HaldCLUT or CubeLUT) is chosen
     *  automatically from the file extension. */
    std::shared_ptr<CLUT3D> getClut(const Glib::ustring& filename) const;

    void clearCache();

private:
    CLUTStore();

    mutable Cache<Glib::ustring, std::shared_ptr<CLUT3D>> cache;
};

}
