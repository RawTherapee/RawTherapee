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
#include "stdimagesource.h"

#include "color.h"
#include "iccstore.h"
#include "image8.h"
#include "image16.h"
#include "imagefloat.h"
#include "imageio.h"
#include "mytime.h"
#include "procparams.h"
#include "utils.h"

namespace rtengine
{
using namespace procparams;
ProcParams* params;

template<class T> void freeArray (T** a, int H)
{
    for (int i = 0; i < H; i++) {
        delete [] a[i];
    }

    delete [] a;
}
template<class T> T** allocArray (int W, int H)
{

    T** t = new T*[H];

    for (int i = 0; i < H; i++) {
        t[i] = new T[W];
    }

    return t;
}

StdImageSource::StdImageSource () : ImageSource(), img(nullptr), plistener(nullptr), full(false), max{}, rgbSourceModified(false)
{

    embProfile = nullptr;
    idata = nullptr;
}

StdImageSource::~StdImageSource ()
{

    delete idata;

    if (img) {
        delete img;
    }
}

void StdImageSource::getSampleFormat (const Glib::ustring &fname, IIOSampleFormat &sFormat, IIOSampleArrangement &sArrangement)
{

    sFormat = IIOSF_UNKNOWN;
    sArrangement = IIOSA_UNKNOWN;

    if (hasJpegExtension(fname)) {
        // For now, png and jpeg files are converted to unsigned short by the loader itself,
        // but there should be functions that read the sample format first, like the TIFF case below
        sFormat = IIOSF_UNSIGNED_CHAR;
        sArrangement = IIOSA_CHUNKY;
        return;
    } else if (hasPngExtension(fname)) {
        int result = ImageIO::getPNGSampleFormat (fname, sFormat, sArrangement);

        if (result == IMIO_SUCCESS) {
            return;
        }
#ifdef LIBJXL
    } else if (hasJxlExtension(fname)) {
        sFormat = IIOSF_FLOAT32;
        sArrangement = IIOSA_CHUNKY;
        return;
#endif
    } else if (hasHeifInputExtension(fname)) {
        ImageIO::getHEIFSampleFormat(fname, sFormat, sArrangement);
        return;
    } else if (hasTiffExtension(fname)) {
        int result = ImageIO::getTIFFSampleFormat (fname, sFormat, sArrangement);

        if (result == IMIO_SUCCESS) {
            return;
        }
    }

    return;
}

/*
 * This method make define the correspondence between the input image type
 * and RT's image data type (Image8, Image16 and Imagefloat), then it will
 * load the image into it
 */
int StdImageSource::load (const Glib::ustring &fname)
{

    fileName = fname;

    // First let's find out the input image's type

    IIOSampleFormat sFormat;
    IIOSampleArrangement sArrangement;
    if (hasHeifInputExtension(fname)) {
        const int error = ImageIO::getHEIFSampleFormat(fname, sFormat, sArrangement, plistener);
        if (error) return error;
    } else {
        getSampleFormat(fname, sFormat, sArrangement);
    }

    // Then create the appropriate object

    switch (sFormat) {
    case (IIOSF_UNSIGNED_CHAR): {
        img = new Image8;
        break;
    }

    case (IIOSF_UNSIGNED_SHORT): {
        img = new Image16;
        break;
    }

    case (IIOSF_LOGLUV24):
    case (IIOSF_LOGLUV32):
    case (IIOSF_FLOAT16):
    case (IIOSF_FLOAT24):
    case (IIOSF_FLOAT32): {
        img = new Imagefloat;
        break;
    }

    default:
        return IMIO_FILETYPENOTSUPPORTED;
    }

    img->setSampleFormat(sFormat);
    img->setSampleArrangement(sArrangement);

    if (plistener) {
        plistener->setProgressStr ("PROGRESSBAR_LOADING");
        plistener->setProgress (0.0);
        img->setProgressListener (plistener);
    }

    // And load the image!

    int error = img->load (fname);

    if (error) {
        delete img;
        img = nullptr;
        return error;
    }

    embProfile = img->getEmbeddedProfile ();

    idata = new FramesData (fname);

    if (idata->hasExif() && !hasHeifInputExtension(fname)) {
        int deg = 0;

        if (idata->getOrientation() == "Rotate 90 CW") {
            deg = 90;
        } else if (idata->getOrientation() == "Rotate 180") {
            deg = 180;
        } else if (idata->getOrientation() == "Rotate 270 CW") {
            deg = 270;
        }

        if (deg) {
            img->rotate(deg);
        }
    }

    if (plistener) {
        plistener->setProgressStr ("PROGRESSBAR_READY");
        plistener->setProgress (1.0);
    }

    wb = ColorTemp (1.0, 1.0, 1.0, 1.0, ColorTemp::DEFAULT_OBSERVER);
    //this is probably a mistake if embedded profile is not D65

    return 0;
}

void StdImageSource::getImage (const ColorTemp &ctemp, int tran, Imagefloat* image, const PreviewProps &pp, const ToneCurveParams &hrp, const RAWParams &raw)
{

    // the code will use OpenMP as of now.

    img->getStdImage(ctemp, tran, image, pp);

    // Hombre: we could have rotated the image here too, with just few line of code, but:
    // 1. it would require other modifications in the engine, so "do not touch that little plonker!"
    // 2. it's more optimized like this

    // Flip if needed
    if (tran & TR_HFLIP) {
        image->hflip();
    }

    if (tran & TR_VFLIP) {
        image->vflip();
    }
}

void StdImageSource::convertColorSpace(Imagefloat* image, const ColorManagementParams &cmp, const ColorTemp &wb)
{
    colorSpaceConversion (image, cmp, embProfile, img->getSampleFormat());
}

void StdImageSource::colorSpaceConversion (Imagefloat* im, const ColorManagementParams &cmp, cmsHPROFILE embedded, IIOSampleFormat sampleFormat)
{

    bool skipTransform = false;
    cmsHPROFILE in = nullptr;
    cmsHPROFILE out = ICCStore::getInstance()->workingSpace (cmp.workingProfile);

    if (cmp.inputProfile == "(embedded)" || cmp.inputProfile.empty() || cmp.inputProfile == "(camera)" || cmp.inputProfile == "(cameraICC)") {
        if (embedded) {
            in = embedded;
        } else {
            if (sampleFormat & (IIOSF_LOGLUV24 | IIOSF_LOGLUV32 | IIOSF_FLOAT16 | IIOSF_FLOAT24 | IIOSF_FLOAT32)) {
                skipTransform = true;
            } else {
                in = ICCStore::getInstance()->getsRGBProfile ();
            }
        }
    } else {
        if (cmp.inputProfile != "(none)") {
            in = ICCStore::getInstance()->getProfile (cmp.inputProfile);

            if (in == nullptr && embedded) {
                in = embedded;
            } else if (in == nullptr) {
                if (sampleFormat & (IIOSF_LOGLUV24 | IIOSF_LOGLUV32 | IIOSF_FLOAT16 | IIOSF_FLOAT24 | IIOSF_FLOAT32)) {
                    skipTransform = true;
                } else {
                    in = ICCStore::getInstance()->getsRGBProfile ();
                }
            }
        }
    }

    if (!skipTransform && in) {
        if(in == embedded && cmsGetColorSpace(in) != cmsSigRgbData) { // if embedded profile is not an RGB profile, use sRGB
            printf("embedded profile is not an RGB profile, using sRGB as input profile\n");
            in = ICCStore::getInstance()->getsRGBProfile ();
        }

        lcmsMutex->lock ();
        cmsHTRANSFORM hTransform = cmsCreateTransform (in, TYPE_RGB_FLT, out, TYPE_RGB_FLT, INTENT_RELATIVE_COLORIMETRIC,
                                   cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE);
        lcmsMutex->unlock ();

        if(hTransform) {
            // Convert to the [0.0 ; 1.0] range
            im->normalizeFloatTo1();

            im->ExecCMSTransform(hTransform);

            // Converting back to the [0.0 ; 65535.0] range
            im->normalizeFloatTo65535();

            cmsDeleteTransform(hTransform);
        } else {
            printf("Could not convert from %s to %s\n", in == embedded ? "embedded profile" : cmp.inputProfile.data(), cmp.workingProfile.data());
        }
    }
}

void StdImageSource::getFullSize (int& w, int& h, int tr)
{

    w = img->getWidth();
    h = img->getHeight();

    if ((tr & TR_ROT) == TR_R90 || (tr & TR_ROT) == TR_R270) {
        w = img->getHeight();
        h = img->getWidth();
    }
}

void StdImageSource::getSize (const PreviewProps &pp, int& w, int& h)
{
    w = pp.getWidth() / pp.getSkip() + (pp.getWidth() % pp.getSkip() > 0);
    h = pp.getHeight() / pp.getSkip() + (pp.getHeight() % pp.getSkip() > 0);
}

void StdImageSource::getAutoExpHistogram (LUTu & histogram, int& histcompr)
{
    if (img->getType() == sImage8) {
        Image8 *img_ = static_cast<Image8*>(img);
        img_->computeAutoHistogram(histogram, histcompr);
    } else if (img->getType() == sImage16) {
        Image16 *img_ = static_cast<Image16*>(img);
        img_->computeAutoHistogram(histogram, histcompr);
    } else if (img->getType() == sImagefloat) {
        Imagefloat *img_ = static_cast<Imagefloat*>(img);
        img_->computeAutoHistogram(histogram, histcompr);
    }
}

void StdImageSource::WBauto(bool extra, double &tempref, double &greenref, array2D<float> &redloc, array2D<float> &greenloc, array2D<float> &blueloc, int bfw, int bfh, double &avg_rm, double &avg_gm, double &avg_bm, double &tempitc, double &greenitc, float &temp0, float &delta,  int &bia, int &dread, int &kcam, int &nocam, float &studgood, float &minchrom, int &kmin, float &minhist, float &maxhist, bool &twotimes, const WBParams & wbpar, int begx, int begy, int yEn, int xEn, int cx, int cy, const ColorManagementParams &cmp, const RAWParams &raw, const ToneCurveParams &hrp)
{
}

void StdImageSource::getrgbloc(int begx, int begy, int yEn, int xEn, int cx, int cy, int bf_h, int bf_w, const procparams::WBParams & wbpar)
{
}

void StdImageSource::getAutoWBMultipliersitc(bool extra, double &tempref, double &greenref, double &tempitc, double &greenitc, float &temp0, float &delta, int &bia, int &dread, int &kcam, int &nocam, float &studgood, float &minchrom, int &kmin, float &minhist, float &maxhist, int begx, int begy, int yEn, int xEn, int cx, int cy, int bf_h, int bf_w, double &rm, double &gm, double &bm, const WBParams & wbpar, const ColorManagementParams &cmp, const RAWParams &raw, const ToneCurveParams &hrp)
{
    if (redAWBMul != -1.) {
        rm = redAWBMul;
        gm = greenAWBMul;
        bm = blueAWBMul;
        return;
    }

    img->getAutoWBMultipliersitc(extra, tempref, greenref, tempitc, greenitc, temp0, delta, bia, dread, kcam, nocam, studgood, minchrom, kmin, minhist, maxhist, begx, begy, yEn, xEn, cx, cy, bf_h, bf_w, rm, gm, bm, params->wb, params->icm, params->raw, params->toneCurve);

    redAWBMul   = rm;
    greenAWBMul = gm;
    blueAWBMul  = bm;
}


void StdImageSource::getAutoWBMultipliers (double &rm, double &gm, double &bm)
{
    if (redAWBMul != -1.) {
        rm = redAWBMul;
        gm = greenAWBMul;
        bm = blueAWBMul;
        return;
    }

    img->getAutoWBMultipliers(rm, gm, bm);

    redAWBMul   = rm;
    greenAWBMul = gm;
    blueAWBMul  = bm;
}

ColorTemp StdImageSource::getSpotWB (std::vector<Coord2D> &red, std::vector<Coord2D> &green, std::vector<Coord2D>& blue, int tran, double equal, StandardObserver observer)
{
    int rn, gn, bn;
    double reds, greens, blues;
    img->getSpotWBData(reds, greens, blues, rn, gn, bn, red, green, blue, tran);
    double img_r, img_g, img_b;
    wb.getMultipliers (img_r, img_g, img_b);

    if( settings->verbose ) {
        printf ("AVG: %g %g %g\n", reds / rn, greens / gn, blues / bn);
    }

    return ColorTemp (reds / rn * img_r, greens / gn * img_g, blues / bn * img_b, equal, observer);
}

void StdImageSource::flush() {
    img->allocate(0, 0);
};

void StdImageSource::wbMul2Camera(double &rm, double &gm, double &bm)
{
    rm = 1.0 / rm;
    gm = 1.0 / gm;
    bm = 1.0 / bm;
}


void StdImageSource::wbCamera2Mul(double &rm, double &gm, double &bm)
{
    rm = 1.0 / rm;
    gm = 1.0 / gm;
    bm = 1.0 / bm;
}

}

