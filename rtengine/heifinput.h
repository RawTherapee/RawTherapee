/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glibmm/ustring.h>
#include <exiv2/exiv2.hpp>

namespace rtengine {

// Primary-image metadata, independent of Exiv2's optional BMFF reader.
struct HEIFMetadata {
    Exiv2::ExifData exif;
    Exiv2::XmpData xmp;
    int width;
    int height;
    int bits;
};

HEIFMetadata readHEIFMetadata(const Glib::ustring& filename);

} // namespace rtengine
