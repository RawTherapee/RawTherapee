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

#include "metadata.h"

#include "imagedata.h"
#include "rtapp.h"
#include "settings.h"

#include "rtgui/pathutils.h"
#include "rtgui/version.h"

#include <giomm.h>
#include <glib/gstdio.h>

#include <ctime>
#include <iostream>
#include <set>
#include <stdio.h>


#if EXIV2_TEST_VERSION(0,28,0)
using Exiv2Error = Exiv2::Error;
#else
using Exiv2Error = Exiv2::AnyError;
#endif


namespace rtengine {

extern const Settings *settings;

std::unique_ptr<Exiv2Metadata::ImageCache> Exiv2Metadata::cache_(nullptr);

namespace {

class Error: public Exiv2Error {
public:
    Error(const std::string &msg):
#if EXIV2_TEST_VERSION(0,28,0)
        Exiv2Error(Exiv2::ErrorCode::kerGeneralError),
#endif
        msg_(msg) {}
    const char *what() const throw() { return msg_.c_str(); }
    int code() const throw() { return 0; }

private:
    std::string msg_;
};


constexpr size_t IMAGE_CACHE_SIZE = 200;

std::unique_ptr<Exiv2::Image> open_exiv2(const Glib::ustring& fname,
                                         bool check_exif)
{
#ifdef EXV_UNICODE_PATH
    glong ws_size = 0;
    gunichar2* const ws = g_utf8_to_utf16(fname.c_str(), -1, nullptr, &ws_size, nullptr);
    std::wstring wfname;
    wfname.reserve(ws_size);
    for (glong i = 0; i < ws_size; ++i) {
        wfname.push_back(ws[i]);
    }
    g_free(ws);
    auto image = Exiv2::ImageFactory::open(wfname);
#else
    auto image = Exiv2::ImageFactory::open(Glib::filename_from_utf8(fname));
#endif
    image->readMetadata();
    if (!image->good() || (check_exif && image->exifData().empty())) {
#if EXIV2_TEST_VERSION(0,27,0)
        auto error_code = Exiv2::ErrorCode::kerErrorMessage;
#else
        auto error_code = 1;
#endif
        throw Exiv2::Error(error_code, "exiv2: invalid image");
    }
    std::unique_ptr<Exiv2::Image> ret(image.release());
    return ret;
}


template <class Data, class Key>
void clear_metadata_key(Data &data, const Key &key)
{
    while (true) {
        auto it = data.findKey(key);
        if (it == data.end()) {
            break;
        } else {
            data.erase(it);
        }
    }
}

} // namespace


Exiv2Metadata::Exiv2Metadata():
    src_(""),
    merge_xmp_(false),
    image_(nullptr),
    exif_(new rtengine::procparams::ExifPairs),
    iptc_(new rtengine::procparams::IPTCPairs)
{
}


Exiv2Metadata::Exiv2Metadata(const Glib::ustring &path):
    src_(path),
    merge_xmp_(settings->metadata_xmp_sync != Settings::MetadataXmpSync::NONE),
    image_(nullptr),
    exif_(new rtengine::procparams::ExifPairs),
    iptc_(new rtengine::procparams::IPTCPairs)
{
}


Exiv2Metadata::Exiv2Metadata(const Glib::ustring &path, bool merge_xmp_sidecar):
    src_(path),
    merge_xmp_(merge_xmp_sidecar),
    image_(nullptr),
    exif_(new rtengine::procparams::ExifPairs),
    iptc_(new rtengine::procparams::IPTCPairs)
{
}


void Exiv2Metadata::load() const
{
    if (!src_.empty() && !image_.get() && Glib::file_test(src_.c_str(), Glib::FILE_TEST_EXISTS)) {
        CacheVal val;
        auto finfo = Gio::File::create_for_path(src_)->query_info(G_FILE_ATTRIBUTE_TIME_MODIFIED);
        Glib::TimeVal xmp_mtime(0, 0);
        if (merge_xmp_) {
            auto xmpname = xmpSidecarPath(src_);
            if (Glib::file_test(xmpname.c_str(), Glib::FILE_TEST_EXISTS)) {
                xmp_mtime = Gio::File::create_for_path(xmpname)->query_info(G_FILE_ATTRIBUTE_TIME_MODIFIED)->modification_time();
            }
        }

        if (cache_ && cache_->get(src_, val) && val.image_mtime >= finfo->modification_time() && val.use_xmp == merge_xmp_ && val.xmp_mtime >= xmp_mtime) {
            image_ = val.image;
        } else {
            auto img = open_exiv2(src_, true);
            image_.reset(img.release());
            if (merge_xmp_) {
                do_merge_xmp(image_.get(), false);
            }
            if (cache_) {
                val.image = image_;
                val.image_mtime = finfo->modification_time();
                val.xmp_mtime = xmp_mtime;
                val.use_xmp = merge_xmp_;
                cache_->set(src_, val);
            }
        }
    }
}

Exiv2::ExifData& Exiv2Metadata::exifData()
{
    return image_.get() ? image_->exifData() : exif_data_;
}

const Exiv2::ExifData& Exiv2Metadata::exifData() const
{
    return const_cast<Exiv2Metadata *>(this)->exifData();
}

Exiv2::IptcData& Exiv2Metadata::iptcData()
{
    return image_.get() ? image_->iptcData() : iptc_data_;
}

const Exiv2::IptcData& Exiv2Metadata::iptcData() const
{
    return const_cast<Exiv2Metadata *>(this)->iptcData();
}

Exiv2::XmpData& Exiv2Metadata::xmpData()
{
    return image_.get() ? image_->xmpData() : xmp_data_;
}

const Exiv2::XmpData& Exiv2Metadata::xmpData() const
{
    return const_cast<Exiv2Metadata *>(this)->xmpData();
}

const Glib::ustring& Exiv2Metadata::filename() const
{
    return src_;
}

const rtengine::procparams::ExifPairs& Exiv2Metadata::exif() const
{
    return *exif_;
}

const rtengine::procparams::IPTCPairs& Exiv2Metadata::iptc() const
{
    return *iptc_;
}

void Exiv2Metadata::setExif(const rtengine::procparams::ExifPairs &exif)
{
    *exif_ = exif;
}

void Exiv2Metadata::setIptc(const rtengine::procparams::IPTCPairs &iptc)
{
    *iptc_ = iptc;
}

void Exiv2Metadata::do_merge_xmp(Exiv2::Image *dst, bool keep_all) const
{
    try { 
        auto xmp = getXmpSidecar(src_);
        Exiv2::ExifData exif;
        Exiv2::IptcData iptc;
        Exiv2::copyXmpToIptc(xmp, iptc);
        Exiv2::moveXmpToExif(xmp, exif);
        std::unordered_map<std::string, std::unordered_set<std::string>> seen;

        if (!keep_all) {
            remove_unwanted(exif);
        }
        
        for (auto &datum : exif) {
            dst->exifData()[datum.key()] = datum;
        }
        for (auto &datum : iptc) {
            auto &s = seen[datum.key()];
            if (s.empty()) {
                clear_metadata_key(dst->iptcData(), Exiv2::IptcKey(datum.key()));
                dst->iptcData()[datum.key()] = datum;
                s.insert(datum.toString());
            } else if (s.insert(datum.toString()).second) {
                dst->iptcData().add(datum);
            }
        }
        seen.clear();
        for (auto &datum : xmp) {
            auto &s = seen[datum.key()];
            if (s.empty()) {
                clear_metadata_key(dst->xmpData(), Exiv2::XmpKey(datum.key()));
                dst->xmpData()[datum.key()] = datum;
                s.insert(datum.toString());
            } else if (s.insert(datum.toString()).second) {
                dst->xmpData().add(datum);
            }
        }
    } catch (std::exception &exc) {
        if (settings->verbose) {
            std::cerr << "Error loading metadata from XMP sidecar: "
                      << exc.what() << std::endl;
        }
    }
}


void Exiv2Metadata::saveToImage(const Glib::ustring &path, bool preserve_all_tags) const
{
    auto dst = open_exiv2(path, false);
    if (image_.get()) {
        dst->setIptcData(image_->iptcData());
        dst->setXmpData(image_->xmpData());
        if (merge_xmp_) {
            do_merge_xmp(dst.get(), preserve_all_tags);
        }
        auto srcexif = image_->exifData();
        if (!preserve_all_tags) {
            remove_unwanted(srcexif);
        }
        //dst->setExifData(srcexif);
        for (auto &tag : srcexif) {
            if (tag.count() > 0) {
                dst->exifData()[tag.key()] = tag;
            }
        }
    } else {
        dst->setExifData(exif_data_);
        dst->setIptcData(iptc_data_);
        dst->setXmpData(xmp_data_);
    }

    dst->exifData()["Exif.Image.Software"] = App::VERSIONED_SOFTWARE_NAME;

    std::time_t t = std::time(nullptr);
    char mbstr[20];
    if (std::strftime(mbstr, sizeof(mbstr), "%Y:%m:%d %H:%M:%S", std::localtime(&t))) {
        dst->exifData()["Exif.Image.DateTime"] = mbstr;
    }
    
    import_exif_pairs(dst->exifData());
    import_iptc_pairs(dst->iptcData());
    bool xmp_tried = false;
    bool iptc_tried = false;
    for (int i = 0; i < 3; ++i) {
        try {
            dst->writeMetadata();
            return;
        } catch (Exiv2::Error &exc) {
            if (exc.code() == Exiv2::ErrorCode::kerTooLargeJpegSegment) {
                std::string msg = exc.what();
                if (msg.find("XMP") != std::string::npos &&
                    !dst->xmpData().empty()) {
                    dst->xmpData().clear();
                    if (!xmp_tried && merge_xmp_) {
                        do_merge_xmp(dst.get(), preserve_all_tags);
                        xmp_tried = true;
                    }
                } else if (msg.find("IPTC") != std::string::npos &&
                           !dst->iptcData().empty()) {
                    dst->iptcData().clear();
                    if (!iptc_tried) {
                        import_iptc_pairs(dst->iptcData());
                        iptc_tried = true;
                    }
                }
            } else {
                throw exc;
            }
        }
    }
}


void Exiv2Metadata::remove_unwanted(Exiv2::ExifData &dst) const
{                
    Exiv2::ExifThumb thumb(dst);
    thumb.erase();

    static const std::set<std::string> badtags = {
        "Exif.Image.Orientation",
        "Exif.Image2.JPEGInterchangeFormat",
        "Exif.Image2.JPEGInterchangeFormatLength",
        "Exif.Image.NewSubfileType",
        "Exif.Image.SubfileType",
        "Exif.Image.ImageWidth",
        "Exif.Image.ImageLength",
        "Exif.Image.BitsPerSample",
        "Exif.Image.Compression",
        "Exif.Image.PhotometricInterpretation",
        "Exif.Image.Thresholding",
        "Exif.Image.CellWidth",
        "Exif.Image.CellLength",
        "Exif.Image.FillOrder",
        "Exif.Image.StripOffsets",
        "Exif.Image.Orientation",
        "Exif.Image.SamplesPerPixel",
        "Exif.Image.RowsPerStrip",
        "Exif.Image.StripByteCounts",
        "Exif.Image.XResolution",
        "Exif.Image.YResolution",
        "Exif.Image.PlanarConfiguration",
        "Exif.Image.GrayResponseUnit",
        "Exif.Image.GrayResponseCurve",
        "Exif.Image.T4Options",
        "Exif.Image.T6Options",
        "Exif.Image.ResolutionUnit",
        "Exif.Image.PageNumber",
        "Exif.Image.Predictor",
        "Exif.Image.TileWidth",
        "Exif.Image.TileLength",
        "Exif.Image.TileOffsets",
        "Exif.Image.TileByteCounts",
        "Exif.Image.SubIFDs",
        "Exif.Image.ExtraSamples",
        "Exif.Image.SampleFormat",
        "Exif.Image.SMinSampleValue",
        "Exif.Image.SMaxSampleValue",
        "Exif.Image.Indexed",
        "Exif.Image.JPEGTables",
        "Exif.Image.OPIProxy",
        "Exif.Image.JPEGProc",
        "Exif.Image.JPEGInterchangeFormat",
        "Exif.Image.JPEGInterchangeFormatLength",
        "Exif.Image.JPEGRestartInterval",
        "Exif.Image.JPEGLosslessPredictors",
        "Exif.Image.JPEGPointTransforms",
        "Exif.Image.JPEGQTables",
        "Exif.Image.JPEGDCTables",
        "Exif.Image.JPEGACTables",
        "Exif.Image.TIFFEPStandardID",
        "Exif.Image.DNGVersion",
        "Exif.Image.DNGBackwardVersion",
        "Exif.Image.DNGPrivateData",
        "Exif.Image.OriginalRawFileData",
        "Exif.Image.SubTileBlockSize",
        "Exif.Image.RowInterleaveFactor",
        "Exif.Photo.ComponentsConfiguration",
        "Exif.Photo.CompressedBitsPerPixel"
    };

    static const std::vector<std::string> badpatterns = {
        "Exif.SubImage"
    };

    if (exif_keys_ && !src_.empty()) {
        try {
            FramesData fd(src_);
            fd.fillBasicTags(dst);
        } catch (std::exception &exc) {
            std::cout << "Error reading metadata from " << src_
                      << std::endl;
        }
    }
    
    for (auto it = dst.begin(); it != dst.end(); ) {
        int relevant = exif_keys_ ? (exif_keys_->find(it->key()) != exif_keys_->end() ? 1 : 0) : -1;
        if (badtags.find(it->key()) != badtags.end() && relevant != 1) {
            it = dst.erase(it);
        } else if (relevant == 0) {
            it = dst.erase(it);
        } else {
            bool found = false;
            for (auto &p : badpatterns) {
                if (it->key().find(p) == 0) {
                    it = dst.erase(it);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ++it;
            }
        }
    }    
}


void Exiv2Metadata::import_exif_pairs(Exiv2::ExifData &out) const
{
    for (auto &p : *exif_) {
        try {
            out[p.first] = p.second;
        } catch (std::exception &exc) {
            if (settings->verbose) {
                std::cout << "Error setting " << p.first << " to " << p.second
                          << ": " << exc.what() << std::endl;
            }
        }
    }
}


void Exiv2Metadata::import_iptc_pairs(Exiv2::IptcData &out) const
{
    for (auto &p : *iptc_) {
        try {
            auto &v = p.second;
            if (v.size() >= 1) {
                clear_metadata_key(out, Exiv2::IptcKey(p.first));
                Exiv2::Iptcdatum d(Exiv2::IptcKey(p.first));
                d.setValue(v[0]);
                out[p.first] = d;
                for (size_t j = 1; j < v.size(); ++j) {
                    d.setValue(v[j]);
                    out.add(d);
                }
            }
        } catch (std::exception &exc) {
            if (settings->verbose) {
                std::cout << "Error setting " << p.first
                          << ": " << exc.what() << std::endl;
            }
        }
    }
}


void Exiv2Metadata::saveToXmp(const Glib::ustring &path) const
{
    Exiv2::XmpData xmp;
    Exiv2::copyExifToXmp(exifData(), xmp);
    Exiv2::copyIptcToXmp(iptcData(), xmp);
    for (auto &datum : xmpData()) {
        xmp[datum.key()] = datum;
    }
    Exiv2::ExifData exif;
    Exiv2::IptcData iptc;
    import_exif_pairs(exif);
    import_iptc_pairs(iptc);
    Exiv2::copyExifToXmp(exif, xmp);
    Exiv2::copyIptcToXmp(iptc, xmp);

    std::string data;
    bool err = false;
    if (Exiv2::XmpParser::encode(data, xmp, Exiv2::XmpParser::omitPacketWrapper|Exiv2::XmpParser::useCompactFormat) != 0) {
        err = true;
    } else {
        FILE *out = g_fopen(path.c_str(), "wb");
        if (!out || fputs(data.c_str(), out) == EOF) {
            err = true;
        }
        if (out) {
            fclose(out);
        }
    }

    if (err) {
        throw Error("error saving XMP sidecar " + path);
    }
}


void Exiv2Metadata::setExifKeys(const std::vector<std::string> *keys)
{
    exif_keys_.reset();
    if (keys) {
        exif_keys_ = std::make_shared<std::unordered_set<std::string>>();
        exif_keys_->insert(keys->begin(), keys->end());
    }
}


void Exiv2Metadata::getDimensions(int &w, int &h) const
{
    if (image_) {
        if (dynamic_cast<const Exiv2::XmpSidecar *>(image_.get())) {
            auto &exif = image_->exifData();
            auto itw = exif.findKey(Exiv2::ExifKey("Exif.Image.ImageWidth"));
            auto ith = exif.findKey(Exiv2::ExifKey("Exif.Image.ImageLength"));
            if (itw != exif.end() && ith != exif.end()) {
                w = to_long(itw);
                h = to_long(ith);
            } else {
                w = h = -1;
            }
        } else {
            w = image_->pixelWidth();
            h = image_->pixelHeight();
        }
    } else {
        w = h = -1;
    }
}


Glib::ustring Exiv2Metadata::xmpSidecarPath(const Glib::ustring &path)
{
    Glib::ustring fn = path;
    if (settings->xmp_sidecar_style == Settings::XmpSidecarStyle::STD) {
        fn = removeExtension(fn);
    }
    return fn + ".xmp";
}


Exiv2::XmpData Exiv2Metadata::getXmpSidecar(const Glib::ustring &path)
{
    Exiv2::XmpData ret;
    auto fname = xmpSidecarPath(path);
    if (Glib::file_test(fname, Glib::FILE_TEST_EXISTS)) {
        auto image = open_exiv2(fname, false);
        ret = image->xmpData();
    }
    return ret;
}


void Exiv2Metadata::init()
{
    cache_.reset(new ImageCache(IMAGE_CACHE_SIZE));
    Exiv2::XmpParser::initialize();
#if defined EXV_ENABLE_BMFF && !EXIV2_TEST_VERSION(0, 28, 3)
    Exiv2::enableBMFF(true);
#endif
}


void Exiv2Metadata::cleanup()
{
    Exiv2::XmpParser::terminate();
}


Exiv2::ExifData Exiv2Metadata::getOutputExifData() const
{
    Exiv2::ExifData exif = exifData();
    try {
        auto xmp = getXmpSidecar(src_);
        Exiv2::moveXmpToExif(xmp, exif);
    } catch (std::exception &exc) {
        if (settings->verbose) {
            std::cerr << "Error loading metadata from XMP sidecar: "
                      << exc.what() << std::endl;
        }
    }
    remove_unwanted(exif);
    import_exif_pairs(exif);
    for (auto it = exif.begin(); it != exif.end(); ) {
        if (it->count() > 0) {
            ++it;
        } else {
            it = exif.erase(it);
        }
    }
    return exif;
}


} // namespace rtengine
