#pragma once

#include <memory>
#include <cstdint>

#include "cache.h"
#include "alignedbuffer.h"
#include "noncopyable.h"

namespace rtengine
{

class HaldCLUT final :
    public NonCopyable
{
public:
    HaldCLUT();
    ~HaldCLUT();

    bool load(const Glib::ustring& filename);

    explicit operator bool() const;

    Glib::ustring getFilename() const;
    Glib::ustring getProfile() const;

    void getRGB(
        float strength,
        std::size_t line_size,
        const float* r,
        const float* g,
        const float* b,
        float* out_rgbx
    ) const;

    static void splitClutFilename(
        const Glib::ustring& filename,
        Glib::ustring& name,
        Glib::ustring& extension,
        Glib::ustring& profile_name,
        bool checkProfile = true
    );

    // Generate a Hald CLUT identity image at the given level, save it to a
    // temporary PNG file (16-bit), and return the path.  Returns an empty
    // string on failure.  The caller is responsible for deleting the file.
    static Glib::ustring createIdentityTempFile(int level);

private:
    AlignedBuffer<std::uint16_t> clut_image;
    unsigned int clut_level;
    float flevel_minus_one;
    float flevel_minus_two;
    Glib::ustring clut_filename;
    Glib::ustring clut_profile;
};

class CLUTStore final :
    public NonCopyable
{
public:
    static CLUTStore& getInstance();

    std::shared_ptr<HaldCLUT> getClut(const Glib::ustring& filename) const;

    void clearCache();

private:
    CLUTStore();

    mutable Cache<Glib::ustring, std::shared_ptr<HaldCLUT>> cache;
};

}
