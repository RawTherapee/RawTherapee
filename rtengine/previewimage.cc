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

#include "previewimage.h"

#include "color.h"
#include "iimage.h"
#include "procparams.h"
#include "rawimagesource.h"
#include "rtthumbnail.h"
#include "utils.h"

using namespace rtengine;
using namespace procparams;

PreviewImage::PreviewImage (const Glib::ustring &fname, const Glib::ustring &ext, const PreviewImageMode mode)
{
    rtengine::Thumbnail* tpp = nullptr;

    if (mode == PIM_EmbeddedPreviewOnly || mode == PIM_EmbeddedOrRaw) {

        const unsigned char *data = nullptr;

        int width = -1, height = -1;

        if (ext.lowercase() == "jpg" || ext.lowercase() == "jpeg") {
            // int deg = infoFromImage (fname);
            tpp = rtengine::Thumbnail::loadFromImage (fname, width, height, 1, 1., ColorTemp::DEFAULT_OBSERVER, true);

            if (tpp) {
                data = tpp->getImage8Data();
            }
        } else if (ext.lowercase() == "png") {
            tpp = rtengine::Thumbnail::loadFromImage (fname, width, height, 1, 1., ColorTemp::DEFAULT_OBSERVER, true);

            if (tpp) {
                data = tpp->getImage8Data();
            }
        } else if (ext.lowercase() == "tif" || ext.lowercase() == "tiff") {
            // int deg = infoFromImage (fname);
            tpp = rtengine::Thumbnail::loadFromImage (fname, width, height, 1, 1., ColorTemp::DEFAULT_OBSERVER, true);

            if (tpp) {
                data = tpp->getImage8Data();
            }
        } else {
            eSensorType sensorType = rtengine::ST_NONE;
            tpp = rtengine::Thumbnail::loadQuickFromRaw (fname, sensorType, width, height, 1, true, true);

            if (tpp) {
                data = tpp->getImage8Data();
            }
        }

        if (tpp) {
            if (data) {
                int w, h;
                double scale = 1.;

                tpp->getDimensions(w, h, scale);

                previewImage = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, w, h);
                previewImage->flush();

#ifdef _OPENMP
                #pragma omp parallel for
#endif
                for (unsigned int i = 0; i < (unsigned int)(h); ++i) {
                    const unsigned char *src = data + i * w * 3;
                    unsigned char *dst = previewImage->get_data() + i * w * 4;

                    for (unsigned int j = 0; j < (unsigned int)(w); ++j) {
                        unsigned char r = *(src++);
                        unsigned char g = *(src++);
                        unsigned char b = *(src++);

                        poke255_uc(dst, r, g, b);
                    }
                }
                previewImage->mark_dirty();
            }
        }
    }

    if ((mode == PIM_EmbeddedOrRaw && !tpp) || mode == PIM_ForceRaw) {
        RawImageSource rawImage;
        int error = rawImage.load(fname);

        if (!error) {
            const unsigned char *data = nullptr;
            int fw, fh;

            procparams::ProcParams params;
            ColorTemp wb = rawImage.getWB ();
            rawImage.getFullSize (fw, fh, TR_NONE);
            PreviewProps pp (0, 0, fw, fh, 1);
            params.icm.inputProfile = Glib::ustring("(embedded)");
            params.raw.bayersensor.method = RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::FAST);
            params.raw.deadPixelFilter = false;
            params.raw.ca_autocorrect = false;
            params.raw.xtranssensor.method = RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::FAST);
            float reddeha = 0.f;
            float greendeha = 0.f;
            float bluedeha = 0.f;
            rawImage.preprocess(params.raw, params.lensProf, params.coarse,  reddeha, greendeha, bluedeha, true);
            double contrastThresholdDummy = 0.0;
            rawImage.demosaic(params.raw, false, contrastThresholdDummy);
            Imagefloat image(fw, fh);
            rawImage.getImage (wb, TR_NONE, &image, pp, params.toneCurve, params.raw);
            rtengine::Image8 output(fw, fh);
            rawImage.convertColorSpace(&image, params.icm, wb);
#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic, 10)
#endif
            for (int i = 0; i < fh; ++i)
                for (int j = 0; j < fw; ++j) {
                    image.r(i, j) = Color::gamma2curve[image.r(i, j)];
                    image.g(i, j) = Color::gamma2curve[image.g(i, j)];
                    image.b(i, j) = Color::gamma2curve[image.b(i, j)];
                }


            image.resizeImgTo<Image8>(fw, fh, TI_Nearest, &output);
            data = output.getData();


            if (data) {
                int w, h;
                w = output.getWidth();
                h = output.getHeight();
                previewImage = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, w, h);
                previewImage->flush();

#ifdef _OPENMP
                #pragma omp parallel for
#endif
                for (unsigned int i = 0; i < (unsigned int)(h); i++) {
                    const unsigned char *src = data + i * w * 3;
                    unsigned char *dst = previewImage->get_data() + i * w * 4;

                    for (unsigned int j = 0; j < (unsigned int)(w); j++) {
                        unsigned char r = *(src++);
                        unsigned char g = *(src++);
                        unsigned char b = *(src++);

                        poke255_uc(dst, r, g, b);
                    }
                }

                previewImage->mark_dirty();
            }
        }
    }

    if (tpp) {
        delete tpp;
    }

}

Cairo::RefPtr<Cairo::ImageSurface> PreviewImage::getImage()
{
    return previewImage;
}
