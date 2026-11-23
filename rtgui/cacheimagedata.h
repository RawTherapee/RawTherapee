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

#include <glibmm/ustring.h>

#include "options.h"

#include "rtengine/dnggainmap.h"
#include "rtengine/imageformat.h"
#include "rtengine/rtengine.h"

class CacheImageData :
    public rtengine::FramesMetaData
{
public:

    // basic information
    Glib::ustring sourceFile;  // Current image path, supplied by the browser.
    Glib::ustring  md5;
    Glib::ustring  version;
    bool  supported;
    ThFileType  format;
    char  rankOld; // old implementation of rank
    bool  inTrashOld; // old implementation of inTrash
    bool  recentlySaved;

    // XMP sidecar info.
    Glib::ustring xmpSidecarMd5;

    // time/date info
    bool  timeValid;
    short year;
    char  month;
    char  day;
    char  hour;
    char  min;
    char  sec;
    // exif info
    bool  exifValid;
    unsigned short frameCount;
    double fnumber;
    double shutter;
    double focalLen, focalLen35mm;
    float focusDist;
    unsigned iso;
    int rating;
    int colorLabel;
    bool isHDR;
    bool isDNG;
    bool isPixelShift;
    int sensortype;
    rtengine::IIO_Sample_Format sampleFormat;
    Glib::ustring lens;
    Glib::ustring camMake;
    Glib::ustring camModel;
    Glib::ustring filetype;
    Glib::ustring expcomp;

    // store a copy of the autoWB's multipliers computed in Thumbnail::_generateThumbnailImage
    // they are not stored in the cache file by this class, but by rtengine::Thumbnail
    // -1 = Unknown
    double redAWBMul, greenAWBMul, blueAWBMul;

    // additional info on raw images
    int   rotate;
    int   thumbImgType;

    enum {
        FULL_THUMBNAIL = 0,  // was the thumbnail generated from whole file
        QUICK_THUMBNAIL = 1  // was the thumbnail generated from embedded jpeg
    };

    int width;
    int height;

    CacheImageData ();

    int load (const Glib::ustring& fname);
    int save (const Glib::ustring& fname);

    //-------------------------------------------------------------------------
    // FramesMetaData interface
    //-------------------------------------------------------------------------

    unsigned int getFrameCount () const override { return frameCount; }
    bool hasExif() const override  { return false; }
    tm getDateTime() const override { return tm{}; }
    time_t getDateTimeAsTS() const override { return time_t(-1); }
    int getISOSpeed() const override { return iso; }
    double getFNumber() const override { return fnumber; }
    double getFocalLen() const override { return focalLen; }
    double getFocalLen35mm() const override { return focalLen35mm; }
    float getFocusDist() const override { return focusDist; }
    double getShutterSpeed() const override { return shutter; }
    double getExpComp() const override { return atof(expcomp.c_str()); }
    std::string getMake() const override { return camMake; }
    std::string getModel() const override { return camModel; }
    std::string getLens() const override { return lens; }
    std::string getOrientation() const override { return ""; } // TODO
    Glib::ustring getFileName() const override { return sourceFile; }
    int getRating () const override { return rating; } // FIXME-piotr : missing rating
    int getColorLabel() const override { return colorLabel; }
    bool getPixelShift () const override { return isPixelShift; }
    bool getHDR() const override { return isHDR; }
    bool getDNG() const override { return isDNG; }
    std::string getImageType() const override { return isPixelShift ? "PS" : isHDR ? "HDR" : "STD"; }
    rtengine::IIOSampleFormat getSampleFormat() const override { return sampleFormat; }
    std::uint32_t getFixBadPixelsConstant() const override;
    bool hasFixBadPixelsConstant() const override;
    std::vector<GainMap> getGainMaps() const override;
    void getDimensions(int &w, int &h) const override
    {
        w = width;
        h = height;
    }
};
