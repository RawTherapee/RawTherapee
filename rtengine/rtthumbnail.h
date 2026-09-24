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

#include <lcms2.h>

#include "image16.h"
#include "image8.h"
#include "imagefloat.h"
#include "LUT.h"

#include "rtgui/threadutils.h"

namespace Glib
{

class ustring;

}

namespace rtengine
{

enum class StandardObserver;

class Thumbnail
{

    MyMutex thumbMutex;

    cmsHPROFILE camProfile;
    double iColorMatrix[3][3];
    double cam2xyz[3][3];

    void transformPixel (int x, int y, int tran, int& tx, int& ty);

    ImageIO* thumbImg;
    double camwbRed;
    double camwbGreen;
    double camwbBlue;
    double redAWBMul, greenAWBMul, blueAWBMul;  // multipliers for auto WB
    double autoWBTemp, autoWBGreen, wbEqual, wbTempBias;    // autoWBTemp and autoWBGreen are updated each time autoWB is requested and if wbEqual has been modified
    StandardObserver wbObserver;
    LUTu aeHistogram;
    int  aeHistCompression;
    bool aeValid;
    double aeExposureCompensation;
    int aeLightness;
    int aeContrast;
    int aeBlack;
    int aeHighlightCompression;
    int aeHighlightCompressionThreshold;
    int embProfileLength;
    unsigned char* embProfileData;
    cmsHPROFILE embProfile;
    double redMultiplier;
    double greenMultiplier;
    double blueMultiplier;
    double scale;
    double defGain;
    int scaleForSave;
    bool gammaCorrected;
    double colorMatrix[3][3];
    double scaleGain;

    void processFilmNegative(const procparams::ProcParams& params, const Imagefloat* baseImg, int rwidth, int rheight);
    void processFilmNegativeV2(const procparams::ProcParams& params, const Imagefloat* baseImg, int rwidth, int rheight);

public:

    bool isRaw;
    int full_width;
    int full_height;

    ~Thumbnail ();
    Thumbnail ();

    void init ();

    Image8* processImage    (const procparams::ProcParams& pparams, eSensorType sensorType, int rheight, TypeInterpolation interp, const FramesMetaData *metadata, double& scale, bool forMonitor=true, bool forHistogramMatching = false);
    Image8* quickProcessImage   (const procparams::ProcParams& pparams, int rheight, TypeInterpolation interp);
    int     getImageWidth   (const procparams::ProcParams& pparams, int rheight, float &ratio);
    void    getDimensions   (int& w, int& h, double& scaleFac);

    static Thumbnail* loadQuickFromRaw (const Glib::ustring& fname, eSensorType &sensorType, int &w, int &h, int fixwh, bool rotate, bool inspectorMode = false, bool forHistogramMatching = false);
    static Thumbnail* loadFromRaw (const Glib::ustring& fname, eSensorType &sensorType, int &w, int &h, int fixwh, double wbEq, StandardObserver wbObserver, bool rotate, const RAWParams *rawParams, bool forHistogramMatching=false);
    static Thumbnail* loadFromImage (const Glib::ustring& fname, int &w, int &h, int fixwh, double wbEq, StandardObserver wbObserver, bool inspectorMode = false);

    void getCamWB     (double& temp, double& green, StandardObserver observer);
    void getAutoWB    (double& temp, double& green, double equal, double tempBias, StandardObserver observer);
    void getAutoWBMultipliers (double& rm, double& gm, double& bm);
    void getSpotWB    (const procparams::ProcParams& params, int x, int y, int rect, double& temp, double& green);
    void applyAutoExp (procparams::ProcParams& pparams);

    unsigned char* getGrayscaleHistEQ (int trim_width);
    bool writeImage (const Glib::ustring& fname);
    bool readImage (const Glib::ustring& fname);

    bool readData  (const Glib::ustring& fname);
    bool writeData  (const Glib::ustring& fname);

    bool readEmbProfile  (const Glib::ustring& fname);
    bool writeEmbProfile (const Glib::ustring& fname);

    unsigned char* getImage8Data();  // accessor to the 8bit image if it is one, which should be the case for the "Inspector" mode.

    // Hombre: ... let's hope that proper template can make this cleaner

    static ImageIO* resizeToSameType(int nw, int nh, TypeInterpolation interp, ImageIO* srcImg)
    {
        ImageIO* imgPtr = nullptr;

        if (srcImg->getType() == sImage8) {
            Image8* castedSrcImg = static_cast<Image8*>(srcImg);
            Image8* img8 = new Image8 (nw, nh);
            castedSrcImg->resizeImgTo(nw, nh, interp, img8);
            imgPtr = img8;
        } else if (srcImg->getType() == sImage16) {
            Image16* castedSrcImg = static_cast<Image16*>(srcImg);
            Image16* img16 = new Image16 (nw, nh);
            castedSrcImg->resizeImgTo(nw, nh, interp, img16);
            imgPtr = img16;
        } else if (srcImg->getType() == sImagefloat) {
            Imagefloat* castedSrcImg = static_cast<Imagefloat*>(srcImg);
            Imagefloat* imgfloat = new Imagefloat (nw, nh);
            castedSrcImg->resizeImgTo(nw, nh, interp, imgfloat);
            imgPtr = imgfloat;
        }

        return imgPtr;
    }

    template<class IC>
    static IC* resizeTo(int nw, int nh, TypeInterpolation interp, ImageIO* srcImg)
    {

        IC* imgPtr = new IC (nw, nh);

        // Hombre: ... let's hope that proper template can make this cleaner

        if (srcImg->getType() == sImage8) {
            Image8* castedSrcImg = static_cast<Image8*>(srcImg);
            castedSrcImg->resizeImgTo<>(nw, nh, interp, imgPtr);
        } else if (srcImg->getType() == sImage16) {
            Image16* castedSrcImg = static_cast<Image16*>(srcImg);
            castedSrcImg->resizeImgTo<>(nw, nh, interp, imgPtr);
        } else if (srcImg->getType() == sImagefloat) {
            Imagefloat* castedSrcImg = static_cast<Imagefloat*>(srcImg);
            castedSrcImg->resizeImgTo<>(nw, nh, interp, imgPtr);
        }

        return imgPtr;
    }
};
}
