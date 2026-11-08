/* -*- C++ -*-
 *
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Alberto Griggio <alberto.griggio@gmail.com>
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
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glibmm.h>
#include <exiv2/exiv2.hpp>
#include <memory>
#include <unordered_set>
#include "procparams.h"
#include "cache.h"

namespace rtengine {

class Exiv2Metadata final
{
public:
    Exiv2Metadata();
    explicit Exiv2Metadata(const Glib::ustring& path);
    Exiv2Metadata(const Glib::ustring& path, bool merge_xmp_sidecar);

    void load(bool require_exif = true) const;
    
    Exiv2::ExifData& exifData();
    const Exiv2::ExifData& exifData() const;
    
    Exiv2::IptcData& iptcData();
    const Exiv2::IptcData& iptcData() const;
    
    Exiv2::XmpData& xmpData();
    const Exiv2::XmpData& xmpData() const;

    const Glib::ustring& filename() const;
    const rtengine::procparams::ExifPairs& exif() const;
    const rtengine::procparams::IPTCPairs& iptc() const;
    void setExif(const rtengine::procparams::ExifPairs& exif);
    void setIptc(const rtengine::procparams::IPTCPairs& iptc);
    
    struct ExportMetadata {
        Exiv2::ExifData exif;
        Exiv2::IptcData iptc;
        Exiv2::XmpData xmp;
    };
    // Caller loads the source first. Retain format-generated destination EXIF.
    ExportMetadata prepareExportMetadata(bool preserve_all_tags,
        const Exiv2::ExifData& destination_exif = {}, bool strict_sidecar_errors = false) const;
    Exiv2::XmpData prepareHEIFXmp(const ExportMetadata& metadata) const;

    void saveToImage(const Glib::ustring& path, bool preserve_all_tags) const;
    void saveToXmp(const Glib::ustring& path) const;

    void setExifKeys(const std::vector<std::string> *keys);

    void getDimensions(int &w, int &h) const;

    Exiv2::ExifData getOutputExifData() const;

    static Glib::ustring xmpSidecarPath(const Glib::ustring& path);
    static Exiv2::XmpData getXmpSidecar(const Glib::ustring& path);

    static void init();
    static void cleanup();
   
private:
    void do_merge_xmp(Exiv2::Image* dst, bool keep_all) const;
    void do_merge_xmp(ExportMetadata& dst, bool keep_all, bool strict_errors = false) const;
    void import_exif_pairs(Exiv2::ExifData& out) const;
    void import_iptc_pairs(Exiv2::IptcData& out) const;
    void remove_unwanted(Exiv2::ExifData& dst) const;
    
    Glib::ustring src_;
    bool merge_xmp_;
    mutable std::shared_ptr<Exiv2::Image> image_;
    std::unique_ptr<rtengine::procparams::ExifPairs> exif_;
    std::unique_ptr<rtengine::procparams::IPTCPairs> iptc_;
    Exiv2::ExifData exif_data_;
    Exiv2::IptcData iptc_data_;
    Exiv2::XmpData xmp_data_;

    std::shared_ptr<std::unordered_set<std::string>> exif_keys_;

    struct CacheVal {
        std::shared_ptr<Exiv2::Image> image;
        Glib::TimeVal image_mtime;
        Glib::TimeVal xmp_mtime;
        bool use_xmp;
        CacheVal(): image(nullptr), image_mtime(), xmp_mtime(), use_xmp(false) {}
    };
    //typedef std::pair<std::shared_ptr<Exiv2::Image>, Glib::TimeVal> CacheVal;
    typedef Cache<Glib::ustring, CacheVal> ImageCache;
    static std::unique_ptr<ImageCache> cache_;
};

template <typename Iterator, typename Integer = std::size_t>
auto to_long(const Iterator &iter, Integer n = Integer{0}) -> decltype(
#if EXIV2_TEST_VERSION(0,28,0)
    iter->toInt64()
) {
    return iter->toInt64(n);
#else
    iter->toLong()
) {
    return iter->toLong(n);
#endif
}


} // namespace rtengine
