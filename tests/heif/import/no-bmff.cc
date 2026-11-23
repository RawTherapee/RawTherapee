// SPDX-License-Identifier: GPL-3.0-or-later
// Linux test interposer: simulate Exiv2 without a native HEIF reader. Buffer
// parsers and in-memory images remain the real installed Exiv2 implementation.
#include <exiv2/exiv2.hpp>
Exiv2::Image::UniquePtr Exiv2::ImageFactory::open(const std::string&, bool)
{
    throw Exiv2::Error(Exiv2::ErrorCode::kerErrorMessage, "native BMFF reader unavailable (test)");
}
