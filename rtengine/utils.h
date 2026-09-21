/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <functional>
#include <vector>

#include <type_traits>
#include <glibmm/ustring.h>

namespace rtengine
{

/**
 * A function object that supplies a value and returns the same value for
 * subsequent calls.
 */
template <typename T>
struct MemoizingSupplier {
    using Supplier = std::function<T()>;

    /**
     * @param supplier The delegate supplier.
     */
    explicit MemoizingSupplier(const Supplier &supplier) :
        supplier(supplier)
    {
    }

    T operator()() const
    {
        if (!is_cached) {
            value = supplier();
            is_cached = true;
        }
        return value;
    }

private:
    const Supplier supplier;
    mutable T value;
    mutable bool is_cached{false};
};

// Update a point of a Cairo::Surface by accessing the raw data
void poke255_uc(unsigned char*& dest, unsigned char r, unsigned char g, unsigned char b);
// Update a point of a Cairo::Surface by accessing the raw data
void poke01_d(unsigned char*& dest, double r, double g, double b);
// Update a point of a Cairo::Surface by accessing the raw data
void poke01_f(unsigned char*& dest, float r, float g, float b);

void bilinearInterp(const unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh);
void nearestInterp(const unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh);
void rotate(unsigned char* img, int& w, int& h, int deg);
void hflip(unsigned char* img, int w, int h);
void vflip(unsigned char* img, int w, int h);

template<typename ENUM>
constexpr typename std::underlying_type<ENUM>::type toUnderlying(ENUM value)
{
    return static_cast<typename std::underlying_type<ENUM>::type>(value);
}

std::vector<std::uint8_t> getFileData(const Glib::ustring &filename);

// Return lower case extension without the "." or "" if the given name contains no "."
Glib::ustring getFileExtension(const Glib::ustring& filename);
// Return true if file has .jpeg or .jpg extension (ignoring case)
bool hasJpegExtension(const Glib::ustring& filename);
// Return true if file has .tiff or .tif extension (ignoring case)
bool hasTiffExtension(const Glib::ustring& filename);
// Return true if file has .png extension (ignoring case)
bool hasPngExtension(const Glib::ustring& filename);
// Return true if file has .avif extension (ignoring case)
bool hasAvifExtension(const Glib::ustring& filename);

#ifdef LIBJXL
// Return true if file has .jxl extension (ignoring case)
bool hasJxlExtension(const Glib::ustring& filename);
#endif

void swab(const void* from, void* to, ssize_t n);

}

#if __SIZEOF_WCHAR_T__ == 4
Glib::ustring utf32_to_utf8(wchar_t* UTF32Buffer, size_t sizeOfUTF32Buffer);
#endif

