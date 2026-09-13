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

#include <glibmm/thread.h>
#include <glibmm/ustring.h>

#include "cieimage.h"
#include "clutstore.h"
#include "color.h"
#include "colortemp.h"
#include "curves.h"
#include "dcp.h"
#include "guidedfilter.h"
#include "iccstore.h"
#include "imagefloat.h"
#include "imagesource.h"
#include "improcfun.h"
#include "labimage.h"
#include "metadata.h"
#include "mytime.h"
#include "processingjob.h"
#include "procparams.h"
#include "rawimagesource.h"
#include "rtengine.h"
#include "utils.h"

#include "rtgui/multilangmgr.h"
#include "rtgui/options.h"

namespace rtengine
{

namespace
{

template <typename T>
void adjust_radius(const T &default_param, double scale_factor, T &param)
{
    const double delta = (param - default_param) * scale_factor;
    param = default_param + delta;
}


class ImageProcessor
{
public:
    ImageProcessor(
        ProcessingJob* pjob,
        int& errorCode,
        ProgressListener* pl,
        bool flush
    ) :
        job(static_cast<ProcessingJobImpl*>(pjob)),
        errorCode(errorCode),
        pl(pl),
        flush(flush),
        // internal state
        initialImage(nullptr),
        imgsrc(nullptr),
        fw(0),
        fh(0),
        tr(0),
        pp(0, 0, 0, 0, 0),
        calclum(nullptr),
        autoNR(0.f),
        autoNRmax(0.f),
        tilesize(0),
        overlap(0),
        ch_M(nullptr),
        max_r(nullptr),
        max_b(nullptr),
        min_b(nullptr),
        min_r(nullptr),
        lumL(nullptr),
        chromC(nullptr),
        ry(nullptr),
        sk(nullptr),
        pcsk(nullptr),
        expcomp(0.0),
        bright(0),
        contr(0),
        black(0),
        hlcompr(0),
        hlcomprthresh(0),
        baseImg(nullptr),
        labView(nullptr),
        ctColorCurve(),
        autili(false),
        butili(false)
    {
    }

    Imagefloat *operator()()
    {
        if (!job->fast) {
            return normal_pipeline();
        } else {
            return fast_pipeline();
        }
    }

private:
    Imagefloat *normal_pipeline()
    {
        if (!stage_init()) {
            return nullptr;
        }

        stage_denoise();
        stage_transform();
        return stage_finish();
    }

    Imagefloat *fast_pipeline()
    {
        if (!job->pparams.resize.enabled) {
            return normal_pipeline();
        }

        pl = nullptr;

        if (!stage_init()) {
            return nullptr;
        }

        stage_transform();
        stage_early_resize();
        stage_denoise();
        return stage_finish();
    }

    bool stage_init()
    {
        errorCode = 0;

        if (pl) {
            pl->setProgressStr("PROGRESSBAR_PROCESSING");
            pl->setProgress(0.0);
        }

        initialImage = job->initialImage;

        if (!initialImage) {
            initialImage = InitialImage::load(job->fname, job->isRaw, &errorCode);

            if (errorCode) {
                delete job;
                return false; //return nullptr;
            }
        }

        procparams::ProcParams& params = job->pparams;

        // acquire image from imagesource
        imgsrc = initialImage->getImageSource();

        tr = getCoarseBitMask(params.coarse);

        if (imgsrc->getSensorType() == ST_BAYER) {
            if (params.raw.bayersensor.method != RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::PIXELSHIFT)) {
                imgsrc->setBorder(params.raw.bayersensor.border);
            } else {
                imgsrc->setBorder(std::max(params.raw.bayersensor.border, 2));
            }
        } else if (imgsrc->getSensorType() == ST_FUJI_XTRANS) {
            imgsrc->setBorder(params.raw.xtranssensor.border);
        }

        imgsrc->getFullSize(fw, fh, tr);

        // check the crop params
        if (params.crop.x > fw || params.crop.y > fh) {
            // the crop is completely out of the image, so we disable the crop
            params.crop.enabled = false;
            // and we set the values to the defaults
            params.crop.x = 0;
            params.crop.y = 0;
            params.crop.w = fw;
            params.crop.h = fh;
        } else {
            if (params.crop.x < 0) {
                params.crop.x = 0;
            }

            if (params.crop.y < 0) {
                params.crop.y = 0;
            }

            if ((params.crop.x + params.crop.w) > fw) {
                // crop overflow in the width dimension ; we trim it
                params.crop.w = fw - params.crop.x;
            }

            if ((params.crop.y + params.crop.h) > fh) {
                // crop overflow in the height dimension ; we trim it
                params.crop.h = fh - params.crop.y;
            }
        }

//    MyTime t1,t2;
//    t1.set();

        ipf_p.reset(new ImProcFunctions(&params, true));
        ImProcFunctions &ipf = * (ipf_p.get());

        imgsrc->setCurrentFrame(params.raw.bayersensor.imageNum);
        float reddeha = 0.f;
        float greendeha = 0.f;
        float bluedeha = 0.f;
        imgsrc->preprocess(params.raw, params.lensProf, params.coarse, reddeha, greendeha, bluedeha, params.dirpyrDenoise.enabled);

        if (pl) {
            pl->setProgress(0.20);
        }

        bool autoContrast = imgsrc->getSensorType() == ST_BAYER ? params.raw.bayersensor.dualDemosaicAutoContrast : params.raw.xtranssensor.dualDemosaicAutoContrast;
        double contrastThreshold = imgsrc->getSensorType() == ST_BAYER ? params.raw.bayersensor.dualDemosaicContrast : params.raw.xtranssensor.dualDemosaicContrast;

        imgsrc->demosaic(params.raw, autoContrast, contrastThreshold, params.pdsharpening.enabled && pl);

        if (params.pdsharpening.enabled) {
            imgsrc->captureSharpening(params.pdsharpening, false, params.pdsharpening.contrast, params.pdsharpening.deconvradius);
        }


        if (pl) {
            pl->setProgress(0.30);
        }

        pp = PreviewProps(0, 0, fw, fh, 1);

        if (params.retinex.enabled) { //enabled Retinex
            LUTf cdcurve(65536, 0);
            LUTf mapcurve(65536, 0);
            RetinextransmissionCurve dehatransmissionCurve;
            RetinexgaintransmissionCurve dehagaintransmissionCurve;
            bool dehacontlutili = false;
            bool mapcontlutili = false;
            bool useHsl = false;
            multi_array2D<float, 4> conversionBuffer(1, 1);
            imgsrc->retinexPrepareBuffers(params.icm, params.retinex, conversionBuffer, dummy);
            imgsrc->retinexPrepareCurves(params.retinex, cdcurve, mapcurve, dehatransmissionCurve, dehagaintransmissionCurve, dehacontlutili, mapcontlutili, useHsl, dummy, dummy);
            float minCD, maxCD, mini, maxi, Tmean, Tsigma, Tmin, Tmax;
            imgsrc->retinex(params.icm, params.retinex, params.toneCurve, cdcurve, mapcurve, dehatransmissionCurve, dehagaintransmissionCurve, conversionBuffer, dehacontlutili, mapcontlutili, useHsl, minCD, maxCD, mini, maxi, Tmean, Tsigma, Tmin, Tmax, dummy);
        }

        if (pl) {
            pl->setProgress(0.40);
        }

        // imgsrc->HLRecovery_Global(params.toneCurve);


        if (pl) {
            pl->setProgress(0.45);
        }

        // set the color temperature
        currWB = ColorTemp(params.wb.temperature, params.wb.green, params.wb.equal, params.wb.method, params.wb.observer);
        ColorTemp currWBitc;

        if (params.wb.method == "autitcgreen") {
            imgsrc->getrgbloc(0, 0, fh, fw, 0, 0, fh, fw, params.wb);
        }
        const bool autowb = (params.wb.method == "autitcgreen" && imgsrc->isRAW());
        ColorTemp autoWB;
        int dread = 0;
        int bia = 1;
        float studgood = 1000.f;
        int nocam = 0;
        int kcam = 0;
        float minchrom = 1000.f;
        float delta = 0.f;
        int kmin  = 20;
        float minhist = 1000000000.f;
        float maxhist = -1000.f;
        double greenitc = 1.;
        float temp0 = 5000.f;
        bool extra = false;
        bool forcewbgrey = false;

        if (!params.wb.enabled) {
            currWB = ColorTemp();
        } else if (params.wb.method == "Camera" || (params.wb.method == "autitcgreen" && params.wb.compat_version >= 2 && !imgsrc->isRAW() && flush)) {//Use also Camera settings for Temperature correlation and TIF/Jpg
            currWB = imgsrc->getWB();
        } else if (params.wb.method == "autold") {//for Auto RGB
            double rm, gm, bm;
            if (params.wb.compat_version == 1 && !imgsrc->isRAW()) {
                // RGB grey compatibility version 1 used the identity
                // multipliers plus temperature bias for non-raw files.
                rm = gm = bm = 1.;
            } else {
                imgsrc->getAutoWBMultipliers(rm, gm, bm);
            }
            currWB.update(rm, gm, bm, params.wb.equal, params.wb.observer, params.wb.tempBias);

        } else if (autowb) {//for auto Itcwb - flush to enable only when batch only with Raw files
        //code similar to that present in improccoordinator.cc
                double rm;
                double gm;
                double bm;
                imgsrc->getAutoWBMultipliersItcGreen(
                    params,
                    forcewbgrey,
                    kcam,
                    greenitc,
                    extra,
                    temp0,
                    delta,
                    bia,
                    dread,
                    nocam,
                    studgood,
                    minchrom,
                    kmin,
                    minhist,
                    maxhist,
                    fh,
                    fw,
                    currWB,
                    0,
                    0.,
                    false,
                    autoWB,
                    rm,
                    gm,
                    bm);

                currWB = autoWB;
        } else if (params.wb.method == "autitcgreen" && params.wb.compat_version == 1 && !imgsrc->isRAW() && flush) {
            // ITCWB compatibility version 1 used 5000 K and observer 10 degrees
            // for non-raw files.
            currWB = ColorTemp(5000., 1., 1., params.wb.method, StandardObserver::TEN_DEGREES);
            currWB.convertObserver(params.wb.observer);
            params.wb.temperature = currWB.getTemp();
            params.wb.green = currWB.getGreen();
            params.wb.equal = currWB.getEqual();
        }

        //end WB auto

        calclum = nullptr ;
        params.dirpyrDenoise.getCurves(noiseLCurve, noiseCCurve);
        autoNR = (float) settings->nrauto;//
        autoNRmax = (float) settings->nrautomax;//

        if (settings->leveldnti == 0) {
            tilesize = 1024;
            overlap = 128;
        }

        if (settings->leveldnti == 1) {
            tilesize = 768;
            overlap = 96;
        }

        //  const int tilesize = 768;
        //  const int overlap = 96;
        int numtiles_W, numtiles_H, tilewidth, tileheight, tileWskip, tileHskip;
        ipf.Tile_calc(tilesize, overlap, 2, fw, fh, numtiles_W, numtiles_H, tilewidth, tileheight, tileWskip, tileHskip);
        int nbtl = numtiles_W * numtiles_H;

        if ((settings->leveldnautsimpl == 1 && params.dirpyrDenoise.Cmethod == "AUT") || (settings->leveldnautsimpl == 0 && params.dirpyrDenoise.C2method == "AUTO")) {
            nbtl = 9;
        }

        ch_M = new float [nbtl];//allocate memory
        max_r = new float [nbtl];
        max_b = new float [nbtl];
        min_b = new float [9];
        min_r = new float [9];
        lumL = new float [nbtl];
        chromC = new float [nbtl];
        ry = new float [nbtl];
        sk = new float [nbtl];
        pcsk = new float [nbtl];

        //  printf("expert=%d\n",settings->leveldnautsimpl);
        if (settings->leveldnautsimpl == 1 && params.dirpyrDenoise.Cmethod == "PON") {
            MyTime t1pone, t2pone;
            t1pone.set();
            int crW = 100; // settings->leveldnv == 0
            int crH = 100; // settings->leveldnv == 0

            if (settings->leveldnv == 1) {
                crW = 250;
                crH = 250;
            }

            if (settings->leveldnv == 2) {
                crW = int (tileWskip / 2);
                crH = int (tileHskip / 2);
            }

            //  if(settings->leveldnv ==2) {crW=int(tileWskip/2);crH=int(1.15f*(tileWskip/2));}//adapted to scale of preview
            if (settings->leveldnv == 3) {
                crW = tileWskip - 10;
                crH = tileHskip - 10;
            }

            float lowdenoise = 1.f;
            int levaut = settings->leveldnaut;

            if (levaut == 1) { //Standard
                lowdenoise = 0.7f;
            }

            //  int crW=tileWskip-10;//crop noise width
            //  int crH=tileHskip-10;//crop noise height
//      Imagefloat *origCropPart;//init auto noise
//          origCropPart = new Imagefloat (crW, crH);//allocate memory
            if (params.dirpyrDenoise.enabled) {//evaluate Noise
                LUTf gamcurve(65536, 0);
                float gam, gamthresh, gamslope;
                ipf.RGB_denoise_infoGamCurve(params.dirpyrDenoise, imgsrc->isRAW(), gamcurve, gam, gamthresh, gamslope);
#ifdef _OPENMP
                #pragma omp parallel
#endif
                {
                    Imagefloat *origCropPart;//init auto noise
                    origCropPart = new Imagefloat(crW, crH); //allocate memory
                    Imagefloat *provicalc = new Imagefloat((crW + 1) / 2, (crH + 1) / 2);  //for denoise curves
                    int skipP = 1;
#ifdef _OPENMP
                    #pragma omp for schedule(dynamic) collapse(2) nowait
#endif

                    for (int wcr = 0; wcr < numtiles_W; wcr++) {
                        for (int hcr = 0; hcr < numtiles_H; hcr++) {
                            int beg_tileW = wcr * tileWskip + tileWskip / 2.f - crW / 2.f;
                            int beg_tileH = hcr * tileHskip + tileHskip / 2.f - crH / 2.f;
                            PreviewProps ppP(beg_tileW, beg_tileH, crW, crH, skipP);
                            imgsrc->getImage(currWB, tr, origCropPart, ppP, params.toneCurve, params.raw);
                            //baseImg->getStdImage(currWB, tr, origCropPart, ppP, true, params.toneCurve);

                            // we only need image reduced to 1/4 here
                            for (int ii = 0; ii < crH; ii += 2) {
                                for (int jj = 0; jj < crW; jj += 2) {
                                    provicalc->r(ii >> 1, jj >> 1) = origCropPart->r(ii, jj);
                                    provicalc->g(ii >> 1, jj >> 1) = origCropPart->g(ii, jj);
                                    provicalc->b(ii >> 1, jj >> 1) = origCropPart->b(ii, jj);
                                }
                            }

                            imgsrc->convertColorSpace(provicalc, params.icm, currWB);  //for denoise luminance curve
                            float maxr = 0.f;
                            float maxb = 0.f;
                            float pondcorrec = 1.0f;
                            float chaut, redaut, blueaut, maxredaut, maxblueaut, minredaut, minblueaut, chromina, sigma, lumema, sigma_L, redyel, skinc, nsknc;
                            int Nb;
                            chaut = 0.f;
                            redaut = 0.f;
                            blueaut = 0.f;
                            maxredaut = 0.f;
                            maxblueaut = 0.f;
                            chromina = 0.f;
                            sigma = 0.f;
                            ipf.RGB_denoise_info(origCropPart, provicalc, imgsrc->isRAW(), gamcurve, gam, gamthresh, gamslope, params.dirpyrDenoise, imgsrc->getDirPyrDenoiseExpComp(), chaut, Nb, redaut, blueaut, maxredaut, maxblueaut, minredaut, minblueaut, chromina, sigma, lumema, sigma_L, redyel, skinc, nsknc);
                            float multip = 1.f;
                            float adjustr = 1.f;

                            if (params.icm.workingProfile == "ProPhoto")   {
                                adjustr = 1.f;   //
                            } else if (params.icm.workingProfile == "Adobe RGB")  {
                                adjustr = 1.f / 1.3f;
                            } else if (params.icm.workingProfile == "sRGB")       {
                                adjustr = 1.f / 1.3f;
                            } else if (params.icm.workingProfile == "WideGamut")  {
                                adjustr = 1.f / 1.1f;
                            } else if (params.icm.workingProfile == "Rec2020")  {
                                adjustr = 1.f / 1.1f;
                            } else if (params.icm.workingProfile == "Beta RGB")   {
                                adjustr = 1.f / 1.2f;
                            } else if (params.icm.workingProfile == "BestRGB")    {
                                adjustr = 1.f / 1.2f;
                            } else if (params.icm.workingProfile == "BruceRGB")   {
                                adjustr = 1.f / 1.2f;
                            }

                            if (!imgsrc->isRAW()) {
                                multip = 2.f;    //take into account gamma for TIF / JPG approximate value...not good for gamma=1
                            }

                            float maxmax = max(maxredaut, maxblueaut);
                            float delta;
                            int mode = 2;
                            int lissage = settings->leveldnliss;
                            ipf.calcautodn_info(chaut, delta, Nb, levaut, maxmax, lumema, chromina, mode, lissage, redyel, skinc, nsknc);

                            //    printf("PROCESS cha=%f red=%f bl=%f redM=%f bluM=%f chrom=%f sigm=%f lum=%f sigL=%f\n",chaut,redaut,blueaut, maxredaut, maxblueaut, chromina, sigma, lumema, sigma_L);
                            if (maxredaut > maxblueaut) {
                                maxr = (delta) / ((autoNRmax * multip * adjustr * lowdenoise) / 2.f);

                                if (minblueaut <= minredaut  && minblueaut < chaut) {
                                    maxb = (-chaut + minblueaut) / (autoNRmax * multip * adjustr * lowdenoise);
                                }
                            } else {
                                maxb = (delta) / ((autoNRmax * multip * adjustr * lowdenoise) / 2.f);

                                if (minredaut <= minblueaut  && minredaut < chaut) {
                                    maxr = (-chaut + minredaut) / (autoNRmax * multip * adjustr * lowdenoise);
                                }
                            }//maxb mxr - empirical evaluation red / blue

                            ch_M[hcr * numtiles_W + wcr] = pondcorrec * chaut / (autoNR * multip * adjustr * lowdenoise);
                            max_r[hcr * numtiles_W + wcr] = pondcorrec * maxr;
                            max_b[hcr * numtiles_W + wcr] = pondcorrec * maxb;
                            lumL[hcr * numtiles_W + wcr] = lumema;
                            chromC[hcr * numtiles_W + wcr] = chromina;
                            ry[hcr * numtiles_W + wcr] = redyel;
                            sk[hcr * numtiles_W + wcr] = skinc;
                            pcsk[hcr * numtiles_W + wcr] = nsknc;

                        }
                    }

                    delete provicalc;
                    delete origCropPart;
                }

                int liss = settings->leveldnliss; //smooth result around mean

                if (liss == 2 || liss == 3) {
                    // I smooth only mean and not delta (max)
                    float nchm = 0.f;
                    float koef = 0.4f; //between 0.1 to 0.9

                    if (liss == 3) {
                        koef = 0.0f;    //quasi auto for mean Ch
                    }

                    for (int wcr = 0; wcr < numtiles_W; wcr++) {
                        for (int hcr = 0; hcr < numtiles_H; hcr++) {
                            nchm += ch_M[hcr * numtiles_W + wcr];
                        }
                    }

                    nchm /= (numtiles_H * numtiles_W);

                    for (int wcr = 0; wcr < numtiles_W; wcr++) {
                        for (int hcr = 0; hcr < numtiles_H; hcr++) {
                            ch_M[hcr * numtiles_W + wcr] = nchm + (ch_M[hcr * numtiles_W + wcr] - nchm) * koef;
                        }
                    }
                }

                if (liss == 3) { //same as auto but with much cells
                    float MaxR = 0.f;
                    float MaxB = 0.f;
                    float MaxRMoy = 0.f;
                    float MaxBMoy = 0.f;

                    for (int k = 0; k < nbtl; k++) {
                        MaxBMoy += max_b[k];
                        MaxRMoy += max_r[k];

                        if (max_r[k] > MaxR) {
                            MaxR = max_r[k];
                        }

                        if (max_b[k] > MaxB) {
                            MaxB = max_b[k];
                        }

                    }

                    MaxBMoy /= nbtl;
                    MaxRMoy /= nbtl;

                    for (int k = 0; k < nbtl; k++) {
                        if (MaxR > MaxB) {
                            max_r[k] = MaxRMoy + (MaxR - MaxRMoy) * 0.66f; //#std Dev
                            //max_b[k]=MinB;
                            max_b[k] = MaxBMoy + (MaxB - MaxBMoy) * 0.66f;

                        } else {
                            max_b[k] = MaxBMoy + (MaxB - MaxBMoy) * 0.66f;
                            //max_r[k]=MinR;
                            max_r[k] = MaxRMoy + (MaxR - MaxRMoy) * 0.66f;

                        }
                    }
                }

                if (settings->verbose) {
                    t2pone.set();
                    printf("Info denoise ponderated performed in %d usec:\n", t2pone.etime(t1pone));
                }

            }
        }


        if ((settings->leveldnautsimpl == 1 && params.dirpyrDenoise.Cmethod == "AUT")  || (settings->leveldnautsimpl == 0 && params.dirpyrDenoise.C2method == "AUTO")) {
            MyTime t1aue, t2aue;
            t1aue.set();
            int crW, crH;

            if (settings->leveldnv == 0) {
                crW = 100;
                crH = 100;
            }

            if (settings->leveldnv == 1) {
                crW = 250;
                crH = 250;
            }

            if (settings->leveldnv == 2) {
                crW = int (tileWskip / 2);
                crH = int (tileHskip / 2);
            }

            //  if(settings->leveldnv ==2) {crW=int(tileWskip/2);crH=int(1.15f*(tileWskip/2));}//adapted to scale of preview
            if (settings->leveldnv == 3) {
                crW = tileWskip - 10;
                crH = tileHskip - 10;
            }

            float lowdenoise = 1.f;
            int levaut = settings->leveldnaut;

            if (levaut == 1) { //Standard
                lowdenoise = 0.7f;
            }

            if (params.dirpyrDenoise.enabled) {//evaluate Noise
                LUTf gamcurve(65536, 0);
                float gam, gamthresh, gamslope;
                ipf.RGB_denoise_infoGamCurve(params.dirpyrDenoise, imgsrc->isRAW(), gamcurve, gam, gamthresh, gamslope);
                int Nb[9];
                int  coordW[3];//coordinate of part of image to measure noise
                int  coordH[3];
                int begW = 50;
                int begH = 50;
                coordW[0] = begW;
                coordW[1] = fw / 2 - crW / 2;
                coordW[2] = fw - crW - begW;
                coordH[0] = begH;
                coordH[1] = fh / 2 - crH / 2;
                coordH[2] = fh - crH - begH;
#ifdef _OPENMP
                #pragma omp parallel
#endif
                {
                    Imagefloat *origCropPart;//init auto noise
                    origCropPart = new Imagefloat(crW, crH); //allocate memory
                    origCropPart->fill(0.f);
                    Imagefloat *provicalc = new Imagefloat((crW + 1) / 2, (crH + 1) / 2);  //for denoise curves

#ifdef _OPENMP
                    #pragma omp for schedule(dynamic) collapse(2) nowait
#endif

                    for (int wcr = 0; wcr <= 2; wcr++) {
                        for (int hcr = 0; hcr <= 2; hcr++) {
                            PreviewProps ppP(coordW[wcr], coordH[hcr], crW, crH, 1);
                            imgsrc->getImage(currWB, tr, origCropPart, ppP, params.toneCurve, params.raw);
                            //baseImg->getStdImage(currWB, tr, origCropPart, ppP, true, params.toneCurve);


                            // we only need image reduced to 1/4 here
                            for (int ii = 0; ii < crH; ii += 2) {
                                for (int jj = 0; jj < crW; jj += 2) {
                                    provicalc->r(ii >> 1, jj >> 1) = origCropPart->r(ii, jj);
                                    provicalc->g(ii >> 1, jj >> 1) = origCropPart->g(ii, jj);
                                    provicalc->b(ii >> 1, jj >> 1) = origCropPart->b(ii, jj);
                                }
                            }

                            imgsrc->convertColorSpace(provicalc, params.icm, currWB);  //for denoise luminance curve
                            int nb = 0;
                            float chaut = 0.f, redaut = 0.f, blueaut = 0.f, maxredaut = 0.f, maxblueaut = 0.f, minredaut = 0.f, minblueaut = 0.f, chromina = 0.f, sigma = 0.f, lumema = 0.f, sigma_L = 0.f, redyel = 0.f, skinc = 0.f, nsknc = 0.f;
                            ipf.RGB_denoise_info(origCropPart, provicalc, imgsrc->isRAW(), gamcurve, gam, gamthresh, gamslope,  params.dirpyrDenoise, imgsrc->getDirPyrDenoiseExpComp(), chaut, nb, redaut, blueaut, maxredaut, maxblueaut, minredaut, minblueaut, chromina, sigma, lumema, sigma_L, redyel, skinc, nsknc);
                            Nb[hcr * 3 + wcr] = nb;
                            ch_M[hcr * 3 + wcr] = chaut;
                            max_r[hcr * 3 + wcr] = maxredaut;
                            max_b[hcr * 3 + wcr] = maxblueaut;
                            min_r[hcr * 3 + wcr] = minredaut;
                            min_b[hcr * 3 + wcr] = minblueaut;
                            lumL[hcr * 3 + wcr] = lumema;
                            chromC[hcr * 3 + wcr] = chromina;
                            ry[hcr * 3 + wcr] = redyel;
                            sk[hcr * 3 + wcr] = skinc;
                            pcsk[hcr * 3 + wcr] = nsknc;
                        }
                    }

                    delete provicalc;
                    delete origCropPart;
                }
                float chM = 0.f;
                float MaxR = 0.f;
                float MaxB = 0.f;
                float MinR = 100000000.f;
                float MinB = 100000000.f;
                float maxr = 0.f;
                float maxb = 0.f;
                float multip = 1.f;
                float adjustr = 1.f;
                float Max_R[9] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
                float Max_B[9] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
                float Min_R[9];
                float Min_B[9];
                float MaxRMoy = 0.f;
                float MaxBMoy = 0.f;
                float MinRMoy = 0.f;
                float MinBMoy = 0.f;

                if (params.icm.workingProfile == "ProPhoto")   {
                    adjustr = 1.f;
                } else if (params.icm.workingProfile == "Adobe RGB")  {
                    adjustr = 1.f / 1.3f;
                } else if (params.icm.workingProfile == "sRGB")       {
                    adjustr = 1.f / 1.3f;
                } else if (params.icm.workingProfile == "WideGamut")  {
                    adjustr = 1.f / 1.1f;
                } else if (params.icm.workingProfile == "Rec2020")  {
                    adjustr = 1.f / 1.1f;
                } else if (params.icm.workingProfile == "Beta RGB")   {
                    adjustr = 1.f / 1.2f;
                } else if (params.icm.workingProfile == "BestRGB")    {
                    adjustr = 1.f / 1.2f;
                } else if (params.icm.workingProfile == "BruceRGB")   {
                    adjustr = 1.f / 1.2f;
                }

                if (!imgsrc->isRAW()) {
                    multip = 2.f;    //take into account gamma for TIF / JPG approximate value...not good for gamma=1
                }

                float delta[9];
                int mode = 1;
                int lissage = settings->leveldnliss;

                for (int k = 0; k < 9; k++) {
                    float maxmax = max(max_r[k], max_b[k]);
                    ipf.calcautodn_info(ch_M[k], delta[k], Nb[k], levaut, maxmax, lumL[k], chromC[k], mode, lissage, ry[k], sk[k], pcsk[k]);
                    //  printf("ch_M=%f delta=%f\n",ch_M[k], delta[k]);
                }

                for (int k = 0; k < 9; k++) {
                    if (max_r[k] > max_b[k]) {
                        //printf("R delta=%f  koef=%f\n",delta[k],autoNRmax*multip*adjustr*lowdenoise);
                        Max_R[k] = (delta[k]) / ((autoNRmax * multip * adjustr * lowdenoise) / 2.f);
                        Min_B[k] = - (ch_M[k] - min_b[k]) / (autoNRmax * multip * adjustr * lowdenoise);
                        Max_B[k] = 0.f;
                        Min_R[k] = 0.f;
                    } else {
                        //printf("B delta=%f  koef=%f\n",delta[k],autoNRmax*multip*adjustr*lowdenoise);
                        Max_B[k] = (delta[k]) / ((autoNRmax * multip * adjustr * lowdenoise) / 2.f);
                        Min_R[k] = - (ch_M[k] - min_r[k])   / (autoNRmax * multip * adjustr * lowdenoise);
                        Min_B[k] = 0.f;
                        Max_R[k] = 0.f;
                    }
                }

                for (int k = 0; k < 9; k++) {
                    //  printf("ch_M= %f Max_R=%f Max_B=%f min_r=%f min_b=%f\n",ch_M[k],Max_R[k], Max_B[k],Min_R[k], Min_B[k]);
                    chM += ch_M[k];
                    MaxBMoy += Max_B[k];
                    MaxRMoy += Max_R[k];
                    MinRMoy += Min_R[k];
                    MinBMoy += Min_B[k];

                    if (Max_R[k] > MaxR) {
                        MaxR = Max_R[k];
                    }

                    if (Max_B[k] > MaxB) {
                        MaxB = Max_B[k];
                    }

                    if (Min_R[k] < MinR) {
                        MinR = Min_R[k];
                    }

                    if (Min_B[k] < MinB) {
                        MinB = Min_B[k];
                    }

                }

                chM /= 9;
                MaxBMoy /= 9;
                MaxRMoy /= 9;
                MinBMoy /= 9;
                MinRMoy /= 9;

                if (MaxR > MaxB) {
                    maxr = MaxRMoy + (MaxR - MaxRMoy) * 0.66f; //#std Dev
                    //  maxb=MinB;
                    maxb = MinBMoy + (MinB - MinBMoy) * 0.66f;

                } else {
                    maxb = MaxBMoy + (MaxB - MaxBMoy) * 0.66f;
                    //  maxr=MinR;
                    maxr = MinRMoy + (MinR - MinRMoy) * 0.66f;

                }

//              printf("SIMPL cha=%f red=%f bl=%f \n",chM,maxr,maxb);

                params.dirpyrDenoise.chroma = chM / (autoNR * multip * adjustr);
                params.dirpyrDenoise.redchro = maxr;
                params.dirpyrDenoise.bluechro = maxb;
            }

            if (settings->verbose) {
                t2aue.set();
                printf("Info denoise auto performed in %d usec:\n", t2aue.etime(t1aue));
            }

            //end evaluate noise
        }

        baseImg = new Imagefloat(fw, fh);
        imgsrc->getImage(currWB, tr, baseImg, pp, params.toneCurve, params.raw);

        if (pl) {
            pl->setProgress(0.50);
        }

//  LUTf Noisecurve (65536,0);
//!!!// auto exposure!!!
        expcomp = params.toneCurve.expcomp;
        bright = params.toneCurve.brightness;
        contr = params.toneCurve.contrast;
        black = params.toneCurve.black;
        hlcompr = params.toneCurve.hlcompr;
        hlcomprthresh = params.toneCurve.hlcomprthresh;


        if (params.toneCurve.autoexp) {
            LUTu aehist;
            int aehistcompr;
            imgsrc->getAutoExpHistogram(aehist, aehistcompr);
            ipf.getAutoExp(aehist, aehistcompr, params.toneCurve.clip, expcomp, bright, contr, black, hlcompr, hlcomprthresh);
        }

        if (params.toneCurve.histmatching) {
            if (!params.toneCurve.fromHistMatching) {
                imgsrc->getAutoMatchedToneCurve(params.icm, params.raw, params.wb.observer, params.toneCurve.curve);
            }

            if (params.toneCurve.autoexp) {
                params.toneCurve.expcomp = 0.0;
            }

            params.toneCurve.autoexp = false;
            params.toneCurve.curveMode = ToneCurveMode::FILMLIKE;
            params.toneCurve.curve2 = { 0 };
            params.toneCurve.brightness = 0;
            params.toneCurve.contrast = 0;
            params.toneCurve.black = 0;
        }

        // Spot Removal
        if (params.spot.enabled && !params.spot.entries.empty()) {
            ipf.removeSpots(baseImg, imgsrc, params.spot.entries, pp, currWB, nullptr, tr);
        }

        // at this stage, we can flush the raw data to free up quite an important amount of memory
        // commented out because it makes the application crash when batch processing...
        // TODO: find a better place to flush rawData and rawRGB
        if (flush) {
            imgsrc->flush();
        }

        return true;
    }

    void stage_denoise()
    {
        const procparams::ProcParams& params = job->pparams;

        DirPyrDenoiseParams denoiseParams = params.dirpyrDenoise;   // make a copy because we cheat here

        if (denoiseParams.Lmethod == "CUR") {
            if (noiseLCurve) {
                denoiseParams.luma = 0.5f;
            } else {
                denoiseParams.luma = 0.0f;
            }
        } else if (denoiseParams.Lmethod == "SLI") {
            noiseLCurve.Reset();
        }

        if (denoiseParams.enabled  && (noiseLCurve || noiseCCurve)) {
            // we only need image reduced to 1/4 here
            calclum = new Imagefloat((fw + 1) / 2, (fh + 1) / 2);  //for luminance denoise curve
#ifdef _OPENMP
            #pragma omp parallel for
#endif

            for (int ii = 0; ii < fh; ii += 2) {
                for (int jj = 0; jj < fw; jj += 2) {
                    calclum->r(ii >> 1, jj >> 1) = baseImg->r(ii, jj);
                    calclum->g(ii >> 1, jj >> 1) = baseImg->g(ii, jj);
                    calclum->b(ii >> 1, jj >> 1) = baseImg->b(ii, jj);
                }
            }

            imgsrc->convertColorSpace(calclum, params.icm, currWB);
        }

        if (denoiseParams.enabled) {
            ImProcFunctions &ipf = * (ipf_p.get());
            float nresi, highresi;
            int kall = 2;
            ipf.RGB_denoise(kall, baseImg, baseImg, calclum, ch_M, max_r, max_b, imgsrc->isRAW(), denoiseParams, imgsrc->getDirPyrDenoiseExpComp(), noiseLCurve, noiseCCurve, nresi, highresi);

        }

//  delete calclum;
        delete [] ch_M;
        delete [] max_r;
        delete [] max_b;
        delete [] min_r;
        delete [] min_b;
        delete [] lumL;
        delete [] chromC;
        delete [] ry;
        delete [] sk;
        delete [] pcsk;
    }

    void stage_transform()
    {
        const procparams::ProcParams& params = job->pparams;
        //ImProcFunctions ipf (&params, true);
        ImProcFunctions &ipf = * (ipf_p.get());

        if (params.filmNegative.enabled) {
            // Process film negative AFTER colorspace conversion if camera space is NOT selected
            if (params.filmNegative.colorSpace != FilmNegativeParams::ColorSpace::INPUT) {
                imgsrc->convertColorSpace(baseImg, params.icm, currWB);
            }

            FilmNegativeParams copy = params.filmNegative;
            ipf.filmNegativeProcess(baseImg, baseImg, copy, params.raw, imgsrc, currWB);

            // ... otherwise, process film negative BEFORE colorspace conversion
            if (params.filmNegative.colorSpace == FilmNegativeParams::ColorSpace::INPUT) {
                imgsrc->convertColorSpace(baseImg, params.icm, currWB);
            }

        } else {
            imgsrc->convertColorSpace(baseImg, params.icm, currWB);
        }

        // perform first analysis
        hist16(65536);

        if (params.cg.enabled) {//gamut compression
            float mac = 0.f;
            float mac0 = 0.f;
            float mac1 = 0.f;
            float mac2 = 0.f;
            int beginend = 0;
            int nbsegam = 0;
            float powe = 1.f;
            ipf.gamutcompr(baseImg, baseImg, beginend, powe, nbsegam, mac, mac0, mac1, mac2);
        }

        ipf.firstAnalysis(baseImg, params, hist16);


        ipf.dehaze(baseImg, params.dehaze);
        ipf.ToneMapFattal02(baseImg, params.fattal, 3, 0, nullptr, 0, 0, 0, false);

        // perform transform (excepted resizing)
        if (ipf.needsTransform(fw, fh, imgsrc->getRotateDegree(), imgsrc->getMetaData())) {
            Imagefloat* trImg = nullptr;

            if (ipf.needsLuminanceOnly()) {
                trImg = baseImg;
            } else {
                trImg = new Imagefloat(fw, fh);
            }

            ipf.transform(baseImg, trImg, 0, 0, 0, 0, fw, fh, fw, fh,
                          imgsrc->getMetaData(), imgsrc->getRotateDegree(), true, true);

            if (trImg != baseImg) {
                delete baseImg;
                baseImg = trImg;
            }
        }
    }

    Imagefloat *stage_finish()
    {
        procparams::ProcParams& params = job->pparams;
        //ImProcFunctions ipf (&params, true);
        ImProcFunctions &ipf = * (ipf_p.get());

        for (int sp = 0; sp < (int)params.locallab.spots.size(); sp++) {
            if (params.locallab.spots.at(sp).expsharp  && params.dirpyrequalizer.cbdlMethod == "bef") {
                if (params.locallab.spots.at(sp).shardamping < 1) {
                    params.locallab.spots.at(sp).shardamping = 1;
                }
            }
        }
        bool execcam = false;

        //execcam => work around for pre-ciecam in LA: about 0.1 second
        for (int sp = 0; sp < (int)params.locallab.spots.size(); sp++) {
            if (params.locallab.spots.at(sp).expprecam) {
                execcam = true;
            }
        }
        if ((params.dirpyrequalizer.cbdlMethod == "bef") && (params.dirpyrequalizer.enabled || execcam) && !params.colorappearance.enabled) {
            if (execcam  && !params.dirpyrequalizer.enabled) {
                params.dirpyrequalizer.enabled = true;

                if (params.dirpyrequalizer.mult[0] == 1.) {
                    params.dirpyrequalizer.mult[0] = 1.01;
                }
            }
            const int W = baseImg->getWidth();
            const int H = baseImg->getHeight();
            LabImage labcbdl(W, H);
            ipf.rgb2lab(*baseImg, labcbdl, params.icm.workingProfile);
            ipf.dirpyrequalizer(&labcbdl, 1);
            ipf.lab2rgb(labcbdl, *baseImg, params.icm.workingProfile);
        }

        // RGB processing

        labView = new LabImage(fw, fh);

        if (params.locallab.enabled && params.locallab.spots.size() > 0) {
            ipf.rgb2lab(*baseImg, *labView, params.icm.workingProfile);

            MyTime t1, t2;
            t1.set();

            const std::unique_ptr<LabImage> reservView(new LabImage(*labView, true));
            const std::unique_ptr<LabImage> lastorigView(new LabImage(*labView, true));
            std::unique_ptr<LabImage> savenormtmView;
            std::unique_ptr<LabImage> savenormretiView;
            LocretigainCurve locRETgainCurve;
            LocretitransCurve locRETtransCurve;
            LocLHCurve loclhCurve;
            LocHHCurve lochhCurve;
            LocCHCurve locchCurve;
            LocHHCurve lochhCurvejz;
            LocCHCurve locchCurvejz;
            LocLHCurve loclhCurvejz;
            LocCCmaskCurve locccmasCurve;
            LocLLmaskCurve locllmasCurve;
            LocHHmaskCurve lochhmasCurve;
            LocHHmaskCurve lochhhmasCurve;
            LocHHmaskCurve lochhhmascieCurve;
            LocCCmaskCurve locccmasexpCurve;
            LocLLmaskCurve locllmasexpCurve;
            LocHHmaskCurve lochhmasexpCurve;
            LocCCmaskCurve locccmasSHCurve;
            LocLLmaskCurve locllmasSHCurve;
            LocHHmaskCurve lochhmasSHCurve;
            LocCCmaskCurve locccmasvibCurve;
            LocLLmaskCurve locllmasvibCurve;
            LocHHmaskCurve lochhmasvibCurve;
            LocCCmaskCurve locccmaslcCurve;
            LocLLmaskCurve locllmaslcCurve;
            LocHHmaskCurve lochhmaslcCurve;
            LocCCmaskCurve locccmascbCurve;
            LocLLmaskCurve locllmascbCurve;
            LocHHmaskCurve lochhmascbCurve;
            LocCCmaskCurve locccmasretiCurve;
            LocLLmaskCurve locllmasretiCurve;
            LocHHmaskCurve lochhmasretiCurve;
            LocCCmaskCurve locccmastmCurve;
            LocLLmaskCurve locllmastmCurve;
            LocHHmaskCurve lochhmastmCurve;
            LocCCmaskCurve locccmasblCurve;
            LocLLmaskCurve locllmasblCurve;
            LocHHmaskCurve lochhmasblCurve;
            LocCCmaskCurve locccmaslogCurve;
            LocLLmaskCurve locllmaslogCurve;
            LocHHmaskCurve lochhmaslogCurve;
            LocCCmaskCurve locccmascieCurve;
            LocLLmaskCurve locllmascieCurve;
            LocHHmaskCurve lochhmascieCurve;

            LocCCmaskCurve locccmas_Curve;
            LocLLmaskCurve locllmas_Curve;
            LocHHmaskCurve lochhmas_Curve;
            LocHHmaskCurve lochhhmas_Curve;

            LocwavCurve loclmasCurveblwav;
            LocwavCurve loclmasCurvecolwav;
            LocwavCurve loclmasCurveciewav;
            LocwavCurve loclmasCurve_wav;
            LocwavCurve locwavCurve;
            LocwavCurve locwavCurvejz;
            LocwavCurve loclevwavCurve;
            LocwavCurve locconwavCurve;
            LocwavCurve loccompwavCurve;
            LocwavCurve loccomprewavCurve;
            LocwavCurve locedgwavCurve;
            LocwavCurve locwavCurvehue;
            LocwavCurve locwavCurvehuecont;
            LocwavCurve locwavCurveden;
            LUTf lllocalcurve(65536, LUT_CLIP_OFF);
            LUTf lclocalcurve(65536, LUT_CLIP_OFF);
            LUTf cllocalcurve(65536, LUT_CLIP_OFF);
            LUTf cclocalcurve(65536, LUT_CLIP_OFF);
            LUTf rgblocalcurve(65536, LUT_CLIP_OFF);
            LUTf hltonecurveloc(65536, LUT_CLIP_OFF);
            LUTf shtonecurveloc(65536, LUT_CLIP_OFF);
            LUTf tonecurveloc(65536, LUT_CLIP_OFF);
            LUTf lightCurveloc(32770, LUT_CLIP_OFF);
            LUTf exlocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmasklocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskexplocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskSHlocalcurve(65536, LUT_CLIP_OFF);
          //  LUTf ghslocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskviblocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmasktmlocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskretilocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskcblocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskbllocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmasklclocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmaskloglocalcurve(65536, LUT_CLIP_OFF);
            LUTf lmasklocal_curve(65536, LUT_CLIP_OFF);
            LUTf lmaskcielocalcurve(65536, LUT_CLIP_OFF);
            LUTf cielocalcurve(65536, LUT_CLIP_OFF);
            LUTf cielocalcurve2(65536, LUT_CLIP_OFF);
            LUTf jzlocalcurve(65536, LUT_CLIP_OFF);
            LUTf czlocalcurve(65536, LUT_CLIP_OFF);
            LUTf czjzlocalcurve(65536, LUT_CLIP_OFF);
            LUTf redlocalcurve(65536, LUT_CLIP_OFF);
            LUTf greenlocalcurve(65536, LUT_CLIP_OFF);
            LUTf bluelocalcurve(65536, LUT_CLIP_OFF);

            array2D<float> shbuffer;

            for (size_t sp = 0; sp < params.locallab.spots.size(); sp++) {
                if (params.locallab.spots.at(sp).inverssha) {
                    shbuffer(fw, fh);
                    break;
                }
            }

            for (size_t sp = 0; sp < params.locallab.spots.size(); sp++) {

                // Set local curves of current spot to LUT
                locRETgainCurve.Set(params.locallab.spots.at(sp).localTgaincurve);
                locRETtransCurve.Set(params.locallab.spots.at(sp).localTtranscurve);
                const bool LHutili = loclhCurve.Set(params.locallab.spots.at(sp).LHcurve);
                const bool HHutili = lochhCurve.Set(params.locallab.spots.at(sp).HHcurve);
                const bool CHutili = locchCurve.Set(params.locallab.spots.at(sp).CHcurve);
                const bool HHutilijz = lochhCurvejz.Set(params.locallab.spots.at(sp).HHcurvejz);
                const bool CHutilijz = locchCurvejz.Set(params.locallab.spots.at(sp).CHcurvejz);
                const bool LHutilijz = loclhCurvejz.Set(params.locallab.spots.at(sp).LHcurvejz);
                const bool lcmasutili = locccmasCurve.Set(params.locallab.spots.at(sp).CCmaskcurve);
                const bool llmasutili = locllmasCurve.Set(params.locallab.spots.at(sp).LLmaskcurve);
                const bool lhmasutili = lochhmasCurve.Set(params.locallab.spots.at(sp).HHmaskcurve);
                const bool lhhmasutili = lochhhmasCurve.Set(params.locallab.spots.at(sp).HHhmaskcurve);
                const bool lhhmascieutili = lochhhmascieCurve.Set(params.locallab.spots.at(sp).HHhmaskciecurve);
                const bool lcmasexputili = locccmasexpCurve.Set(params.locallab.spots.at(sp).CCmaskexpcurve);
                const bool llmasexputili = locllmasexpCurve.Set(params.locallab.spots.at(sp).LLmaskexpcurve);
                const bool lhmasexputili = lochhmasexpCurve.Set(params.locallab.spots.at(sp).HHmaskexpcurve);
                const bool lcmasSHutili = locccmasSHCurve.Set(params.locallab.spots.at(sp).CCmaskSHcurve);
                const bool llmasSHutili = locllmasSHCurve.Set(params.locallab.spots.at(sp).LLmaskSHcurve);
                const bool lhmasSHutili = lochhmasSHCurve.Set(params.locallab.spots.at(sp).HHmaskSHcurve);
                const bool lcmasvibutili = locccmasvibCurve.Set(params.locallab.spots.at(sp).CCmaskvibcurve);
                const bool llmasvibutili = locllmasvibCurve.Set(params.locallab.spots.at(sp).LLmaskvibcurve);
                const bool lhmasvibutili = lochhmasvibCurve.Set(params.locallab.spots.at(sp).HHmaskvibcurve);
                const bool lcmascbutili = locccmascbCurve.Set(params.locallab.spots.at(sp).CCmaskcbcurve);
                const bool llmascbutili = locllmascbCurve.Set(params.locallab.spots.at(sp).LLmaskcbcurve);
                const bool lhmascbutili = lochhmascbCurve.Set(params.locallab.spots.at(sp).HHmaskcbcurve);
                const bool lcmasretiutili = locccmasretiCurve.Set(params.locallab.spots.at(sp).CCmaskreticurve);
                const bool llmasretiutili = locllmasretiCurve.Set(params.locallab.spots.at(sp).LLmaskreticurve);
                const bool lhmasretiutili = lochhmasretiCurve.Set(params.locallab.spots.at(sp).HHmaskreticurve);
                const bool lcmastmutili = locccmastmCurve.Set(params.locallab.spots.at(sp).CCmasktmcurve);
                const bool lhmaslcutili = lochhmaslcCurve.Set(params.locallab.spots.at(sp).HHmasklccurve);
                const bool llmastmutili = locllmastmCurve.Set(params.locallab.spots.at(sp).LLmasktmcurve);
                const bool lhmastmutili = lochhmastmCurve.Set(params.locallab.spots.at(sp).HHmasktmcurve);
                const bool lcmasblutili = locccmasblCurve.Set(params.locallab.spots.at(sp).CCmaskblcurve);
                const bool llmasblutili = locllmasblCurve.Set(params.locallab.spots.at(sp).LLmaskblcurve);
                const bool lhmasblutili = lochhmasblCurve.Set(params.locallab.spots.at(sp).HHmaskblcurve);
                const bool lcmaslogutili = locccmaslogCurve.Set(params.locallab.spots.at(sp).CCmaskcurveL);
                const bool llmaslogutili = locllmaslogCurve.Set(params.locallab.spots.at(sp).LLmaskcurveL);
                const bool lhmaslogutili = lochhmaslogCurve.Set(params.locallab.spots.at(sp).HHmaskcurveL);
                const bool lcmascieutili = locccmascieCurve.Set(params.locallab.spots.at(sp).CCmaskciecurve);
                const bool llmascieutili = locllmascieCurve.Set(params.locallab.spots.at(sp).LLmaskciecurve);
                const bool lhmascieutili = lochhmascieCurve.Set(params.locallab.spots.at(sp).HHmaskciecurve);

                const bool lcmas_utili = locccmas_Curve.Set(params.locallab.spots.at(sp).CCmask_curve);
                const bool llmas_utili = locllmas_Curve.Set(params.locallab.spots.at(sp).LLmask_curve);
                const bool lhmas_utili = lochhmas_Curve.Set(params.locallab.spots.at(sp).HHmask_curve);
                const bool lhhmas_utili = lochhhmas_Curve.Set(params.locallab.spots.at(sp).HHhmask_curve);
                const bool lmasutiliblwav = loclmasCurveblwav.Set(params.locallab.spots.at(sp).LLmaskblcurvewav);
                const bool lmasutilicolwav = loclmasCurvecolwav.Set(params.locallab.spots.at(sp).LLmaskcolcurvewav);
                const bool lmasutiliciewav = loclmasCurveciewav.Set(params.locallab.spots.at(sp).LLmaskciecurvewav);
                const bool lcmaslcutili = locccmaslcCurve.Set(params.locallab.spots.at(sp).CCmasklccurve);
                const bool llmaslcutili = locllmaslcCurve.Set(params.locallab.spots.at(sp).LLmasklccurve);
                const bool lmasutili_wav = loclmasCurve_wav.Set(params.locallab.spots.at(sp).LLmask_curvewav);
                const bool locwavutili = locwavCurve.Set(params.locallab.spots.at(sp).locwavcurve);
                const bool locwavutilijz = locwavCurvejz.Set(params.locallab.spots.at(sp).locwavcurvejz);
                const bool locwavhueutili = locwavCurvehue.Set(params.locallab.spots.at(sp).locwavcurvehue);
                const bool locwavhueutilicont = locwavCurvehuecont.Set(params.locallab.spots.at(sp).locwavcurvehuecont);
                const bool locwavdenutili = locwavCurveden.Set(params.locallab.spots.at(sp).locwavcurveden);
                const bool loclevwavutili = loclevwavCurve.Set(params.locallab.spots.at(sp).loclevwavcurve);
                const bool locconwavutili = locconwavCurve.Set(params.locallab.spots.at(sp).locconwavcurve);
                const bool loccompwavutili = loccompwavCurve.Set(params.locallab.spots.at(sp).loccompwavcurve);
                const bool loccomprewavutili = loccomprewavCurve.Set(params.locallab.spots.at(sp).loccomprewavcurve);
                const bool locedgwavutili = locedgwavCurve.Set(params.locallab.spots.at(sp).locedgwavcurve);
                const bool locallutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).llcurve, lllocalcurve, 1);
                const bool localclutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).clcurve, cllocalcurve, 1);
                const bool locallcutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).lccurve, lclocalcurve, 1);
                const bool localcutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).cccurve, cclocalcurve, 1);
                const bool localrgbutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).rgbcurve, rgblocalcurve, 1);
                const bool localexutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).excurve, exlocalcurve, 1);
                const bool localmaskutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskcurve, lmasklocalcurve, 1);
                const bool localmaskexputili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskexpcurve, lmaskexplocalcurve, 1);
                const bool localmaskSHutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).LmaskSHcurve, lmaskSHlocalcurve, 1);
             //   const bool localghsutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).ghscurve, ghslocalcurve, 1);
                const bool localmaskvibutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskvibcurve, lmaskviblocalcurve, 1);
                const bool localmasktmutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmasktmcurve, lmasktmlocalcurve, 1);
                const bool localmaskretiutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskreticurve, lmaskretilocalcurve, 1);
                const bool localmaskcbutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskcbcurve, lmaskcblocalcurve, 1);
                const bool localmaskblutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskblcurve, lmaskbllocalcurve, 1);
                const bool localmasklcutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmasklccurve, lmasklclocalcurve, 1);
                const bool localmasklogutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).LmaskcurveL, lmaskloglocalcurve, 1);
                const bool localmask_utili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmask_curve, lmasklocal_curve, 1);
                const bool localmaskcieutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).Lmaskciecurve, lmaskcielocalcurve, 1);
                const bool localcieutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).ciecurve, cielocalcurve, 1);
                const bool localcieutili2 = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).ciecurve2, cielocalcurve2, 1);
                const bool localjzutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).jzcurve, jzlocalcurve, 1);
                const bool localczutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).czcurve, czlocalcurve, 1);
                const bool localczjzutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).czjzcurve, czjzlocalcurve, 1);
                const bool localredutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).redcurve, redlocalcurve, 1);
                const bool localgreenutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).greencurve, greenlocalcurve, 1);
                const bool localblueutili = CurveFactory::diagonalCurve2Lut(params.locallab.spots.at(sp).bluecurve, bluelocalcurve, 1);

                //provisory
                double ecomp = params.locallab.spots.at(sp).expcomp;
                double lblack = params.locallab.spots.at(sp).black;
                double lhlcompr = params.locallab.spots.at(sp).hlcompr;
                double lhlcomprthresh = params.locallab.spots.at(sp).hlcomprthresh;
                double shcompr = params.locallab.spots.at(sp).shcompr;
                double br = params.locallab.spots.at(sp).lightness;
                double cont = params.locallab.spots.at(sp).contrast;

                if (lblack < 0. && params.locallab.spots.at(sp).expMethod == "pde") {
                    lblack *= 1.5;
                }

                float contsig = params.locallab.spots.at(sp).contsigqcie;

                float lightsig = params.locallab.spots.at(sp).lightsigqcie;

                // Reference parameters computation
                double huere, chromare, lumare, huerefblu, chromarefblu, lumarefblu, sobelre;
                int lastsav;
                float avge;
                float meantme;
                float stdtme;
                float meanretie;
                float stdretie;
                float fab = 1.f;
                float maxicam = -1000.f;
                float rdx, rdy, grx, gry, blx, bly = 0.f;
                float meanx, meany, meanxe, meanye, maxdat = 0.f;
                int ill = 2;
                int prim = 3;

                if (params.locallab.spots.at(sp).spotMethod == "exc") {
                    ipf.calc_ref(sp, reservView.get(), reservView.get(), 0, 0, fw, fh, 1, huerefblu, chromarefblu, lumarefblu, huere, chromare, lumare, sobelre, avge, locwavCurveden, locwavdenutili);
                } else {
                    ipf.calc_ref(sp, labView, labView, 0, 0, fw, fh, 1, huerefblu, chromarefblu, lumarefblu, huere, chromare, lumare, sobelre, avge, locwavCurveden, locwavdenutili);
                }

                CurveFactory::complexCurvelocal(ecomp, lblack / 65535., lhlcompr, lhlcomprthresh, shcompr, br, cont, lumare,
                                                hltonecurveloc, shtonecurveloc, tonecurveloc, lightCurveloc, avge,
                                                1);
                float minCD;
                float maxCD;
                float mini;
                float maxi;
                float Tmean;
                float Tsigma;
                float Tmin;
                float Tmax;
                float resi[8];
               
                float sharc = 0.f;
                float denocont = 0.f;
                int ghsbpwp[2];
                ghsbpwp[0] = 0;
                ghsbpwp[1] = 0;
                float ghsbpwpvalue[2];
                ghsbpwpvalue[0] = 0.f;
                ghsbpwpvalue[1] = 1.f;
                float savmadl[21]  = {100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f, 100.f}; 
                float ghsbwslider[2];
                ghsbwslider[0] = 0.f;
                ghsbwslider[1] = 1.f;
                float michbwslider[2];//added to facilitate a possible modification requested by users, but is not currently in use
                michbwslider[0] = 0.f;
                michbwslider[1] = 1.f;
                float maxdatend2 = 0.f;
                float satdatend2 = 0.f;
                bool gamaut2 = false;
                float ghscolor[4];
                float ghssym = 0.f;
                float ghsmid = 0.f;
                float ghsmaxrgb = 0.f;
                float ghs3sig = 0.f;
                bool ghsautsp = false;
                float slopeg = 1.f;
                bool linkrgb = true;
                // No Locallab mask is shown in exported picture
                ipf.Lab_Local(2, sp, shbuffer, labView, labView, reservView.get(), savenormtmView.get(), savenormretiView.get(), lastorigView.get(), fw, fh, 0, 0, fw, fh, fw, fh, fw, fh,  1, locRETgainCurve, locRETtransCurve,
                              lllocalcurve, locallutili,
                              cllocalcurve, localclutili,
                              lclocalcurve, locallcutili,
                              loclhCurve, lochhCurve, locchCurve,
                              lochhCurvejz, locchCurvejz, loclhCurvejz,
                              lmasklocalcurve, localmaskutili,
                              lmaskexplocalcurve, localmaskexputili,
                              lmaskSHlocalcurve, localmaskSHutili,
                              lmaskviblocalcurve, localmaskvibutili,
                              lmasktmlocalcurve, localmasktmutili,
                              lmaskretilocalcurve, localmaskretiutili,
                              lmaskcblocalcurve, localmaskcbutili,
                              lmaskbllocalcurve, localmaskblutili,
                              lmasklclocalcurve, localmasklcutili,
                              lmaskloglocalcurve, localmasklogutili,
                              lmasklocal_curve, localmask_utili,
                              lmaskcielocalcurve, localmaskcieutili,
                              cielocalcurve, localcieutili,
                              cielocalcurve2, localcieutili2,
                              jzlocalcurve, localjzutili,
                              czlocalcurve, localczutili,
                              czjzlocalcurve, localczjzutili,
                              redlocalcurve, localredutili,
                              greenlocalcurve, localgreenutili,
                              bluelocalcurve, localblueutili,

                              locccmasCurve, lcmasutili, locllmasCurve, llmasutili, lochhmasCurve, lhmasutili, lochhhmasCurve, lhhmasutili, lochhhmascieCurve, lhhmascieutili, locccmasexpCurve, lcmasexputili, locllmasexpCurve, llmasexputili, lochhmasexpCurve, lhmasexputili,
                              locccmasSHCurve, lcmasSHutili, locllmasSHCurve, llmasSHutili, lochhmasSHCurve, lhmasSHutili,
                              locccmasvibCurve, lcmasvibutili, locllmasvibCurve, llmasvibutili, lochhmasvibCurve, lhmasvibutili,
                              locccmascbCurve, lcmascbutili, locllmascbCurve, llmascbutili, lochhmascbCurve, lhmascbutili,
                              locccmasretiCurve, lcmasretiutili, locllmasretiCurve, llmasretiutili, lochhmasretiCurve, lhmasretiutili,
                              locccmastmCurve, lcmastmutili, locllmastmCurve, llmastmutili, lochhmastmCurve, lhmastmutili,
                              locccmasblCurve, lcmasblutili, locllmasblCurve, llmasblutili, lochhmasblCurve, lhmasblutili,
                              locccmaslcCurve, lcmaslcutili, locllmaslcCurve, llmaslcutili, lochhmaslcCurve, lhmaslcutili,
                              locccmaslogCurve, lcmaslogutili, locllmaslogCurve, llmaslogutili, lochhmaslogCurve, lhmaslogutili,

                              locccmas_Curve, lcmas_utili, locllmas_Curve, llmas_utili, lochhmas_Curve, lhmas_utili,
                              locccmascieCurve, lcmascieutili, locllmascieCurve, llmascieutili, lochhmascieCurve, lhmascieutili,
                              lochhhmas_Curve, lhhmas_utili,
                              loclmasCurveblwav, lmasutiliblwav,
                              loclmasCurvecolwav, lmasutilicolwav,
                              loclmasCurveciewav, lmasutiliciewav,
                              locwavCurve, locwavutili,
                              locwavCurvejz, locwavutilijz,
                              loclevwavCurve, loclevwavutili,
                              locconwavCurve, locconwavutili,
                              loccompwavCurve, loccompwavutili,
                              loccomprewavCurve, loccomprewavutili,
                              locwavCurvehue, locwavhueutili,
                              locwavCurvehuecont, locwavhueutilicont,
                              locwavCurveden, locwavdenutili,
                              locedgwavCurve, locedgwavutili,
                              loclmasCurve_wav, lmasutili_wav,
                              LHutili, HHutili, CHutili, HHutilijz, CHutilijz, LHutilijz, cclocalcurve, localcutili, rgblocalcurve, localrgbutili, localexutili, exlocalcurve, hltonecurveloc, shtonecurveloc, tonecurveloc, lightCurveloc,
                              huerefblu, chromarefblu, lumarefblu, huere, chromare, lumare, sobelre, lastsav, false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              minCD, maxCD, mini, maxi, Tmean, Tsigma, Tmin, Tmax,
                              meantme, stdtme, meanretie, stdretie, fab, maxicam, rdx, rdy, grx, gry, blx, bly, meanx, meany, meanxe, meanye, maxdat, prim, ill, contsig, lightsig, slopeg, linkrgb,
                              resi, sharc, denocont, ghsbpwp, ghsbpwpvalue, savmadl, ghsbwslider, ghssym, ghsautsp, ghscolor, ghsmid, ghsmaxrgb, ghs3sig, michbwslider, maxdatend2, satdatend2, gamaut2);

                if (sp + 1u < params.locallab.spots.size()) {
                    // do not copy for last spot as it is not needed anymore
                    lastorigView->CopyFrom(labView);
                }

                if (params.locallab.spots.at(sp).spotMethod == "exc") {
                    ipf.calc_ref(sp, reservView.get(), reservView.get(), 0, 0, fw, fh, 1, huerefblu, chromarefblu, lumarefblu, huere, chromare, lumare, sobelre, avge, locwavCurveden, locwavdenutili);
                } else {
                    ipf.calc_ref(sp, labView, labView, 0, 0, fw, fh, 1, huerefblu, chromarefblu, lumarefblu, huere, chromare, lumare, sobelre, avge, locwavCurveden, locwavdenutili);
                }
            }

            t2.set();
            ipf.lab2rgb(*labView, *baseImg, params.icm.workingProfile);

            if (settings->verbose) {
                printf("Total local:- %d usec\n", t2.etime(t1));
            }

        }


        curve1(65536);
        curve2(65536);
        curve(65536, 0);
        satcurve(65536, 0);
        lhskcurve(65536, 0);
        lumacurve(32770, 0);  // lumacurve[32768] and lumacurve[32769] will be set to 32768 and 32769 later to allow linear interpolation
        clcurve(65536, 0);
        wavclCurve(65536, 0);

        //if(params.blackwhite.enabled) params.toneCurve.hrenabled=false;

        CurveFactory::complexCurve(expcomp, black / 65535.0, hlcompr, hlcomprthresh, params.toneCurve.shcompr, bright, contr,
                                   params.toneCurve.curve, params.toneCurve.curve2,
                                   hist16, curve1, curve2, curve, dummy, customToneCurve1, customToneCurve2);

        CurveFactory::RGBCurve(params.rgbCurves.rcurve, rCurve, 1);
        CurveFactory::RGBCurve(params.rgbCurves.gcurve, gCurve, 1);
        CurveFactory::RGBCurve(params.rgbCurves.bcurve, bCurve, 1);

        bool opautili = false;

        if (params.colorToning.enabled) {
            TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params.icm.workingProfile);
            double wp[3][3] = {
                {wprof[0][0], wprof[0][1], wprof[0][2]},
                {wprof[1][0], wprof[1][1], wprof[1][2]},
                {wprof[2][0], wprof[2][1], wprof[2][2]}
            };
            params.colorToning.getCurves(ctColorCurve, ctOpacityCurve, wp, opautili);
            clToningcurve(65536, 0);
            CurveFactory::diagonalCurve2Lut(params.colorToning.clcurve, clToningcurve, 1);
            cl2Toningcurve(65536, 0);
            CurveFactory::diagonalCurve2Lut(params.colorToning.cl2curve, cl2Toningcurve, 1);
        }

//        labView = new LabImage(fw, fh);

        if (params.blackwhite.enabled) {
            CurveFactory::curveBW(params.blackwhite.beforeCurve, params.blackwhite.afterCurve, hist16, dummy, customToneCurvebw1, customToneCurvebw2, 1);
        }

        double rrm, ggm, bbm;
        float autor, autog, autob;
        float satLimit = float (params.colorToning.satProtectionThreshold) / 100.f * 0.7f + 0.3f;
        float satLimitOpacity = 1.f - (float (params.colorToning.saturatedOpacity) / 100.f);

        if (params.colorToning.enabled  && params.colorToning.autosat && params.colorToning.method != "LabGrid") { //for colortoning evaluation of saturation settings
            float moyS = 0.f;
            float eqty = 0.f;
            ipf.moyeqt(baseImg, moyS, eqty); //return image : mean saturation and standard dev of saturation
            float satp = ((moyS + 1.5f * eqty) - 0.3f) / 0.7f; //1.5 sigma ==> 93% pixels with high saturation -0.3 / 0.7 convert to Hombre scale

            if (satp >= 0.92f) {
                satp = 0.92f;    //avoid values too high (out of gamut)
            }

            if (satp <= 0.15f) {
                satp = 0.15f;    //avoid too low values
            }

            satLimit = 100.f * satp;

            satLimitOpacity = 100.f * (moyS - 0.85f * eqty); //-0.85 sigma==>20% pixels with low saturation
        }

        autor = -9000.f; // This will ask to compute the "auto" values for the B&W tool (have to be inferior to -5000)
        DCPProfileApplyState as;
        DCPProfile *dcpProf = imgsrc->getDCP(params.icm, as);

        LUTu histToneCurve;

        const auto& options = App::get().options();
        ipf.rgbProc(baseImg, labView, nullptr, curve1, curve2, curve, params.toneCurve.saturation, rCurve, gCurve, bCurve, satLimit, satLimitOpacity, ctColorCurve, ctOpacityCurve, opautili, clToningcurve, cl2Toningcurve, customToneCurve1, customToneCurve2, customToneCurvebw1, customToneCurvebw2, rrm, ggm, bbm, autor, autog, autob, expcomp, hlcompr, hlcomprthresh, dcpProf, as, histToneCurve, options.chunkSizeRGB, options.measure);

        if (settings->verbose) {
            printf("Output image / Auto B&W coefs:   R=%.2f   G=%.2f   B=%.2f\n", static_cast<double>(autor), static_cast<double>(autog), static_cast<double>(autob));
        }

        // if clut was used and size of clut cache == 1 we free the memory used by the clutstore (default clut cache size = 1 for 32 bit OS)
        if (params.filmSimulation.enabled && !params.filmSimulation.clutFilename.empty() && options.clutCacheSize == 1) {
            CLUTStore::getInstance().clearCache();
        }

        // freeing up some memory
        customToneCurve1.Reset();
        customToneCurve2.Reset();
        ctColorCurve.Reset();
        ctOpacityCurve.Reset();
        noiseLCurve.Reset();
        noiseCCurve.Reset();
        customToneCurvebw1.Reset();
        customToneCurvebw2.Reset();

        // Freeing baseImg because not used anymore
        delete baseImg;
        baseImg = nullptr;

        if (pl) {
            pl->setProgress(0.55);
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // start tile processing...???


        if (params.labCurve.contrast != 0) { //only use hist16 for contrast
            hist16.clear();

#ifdef _OPENMP
            #pragma omp parallel
#endif
            {
                LUTu hist16thr(hist16.getSize());   // one temporary lookup table per thread
                hist16thr.clear();
#ifdef _OPENMP
                #pragma omp for schedule(static) nowait
#endif

                for (int i = 0; i < fh; i++)
                    for (int j = 0; j < fw; j++) {
                        hist16thr[(int)((labView->L[i][j]))]++;
                    }

#ifdef _OPENMP
                #pragma omp critical
#endif
                {
                    hist16 += hist16thr;
                }
            }
        }

        bool utili;
        CurveFactory::complexLCurve(params.labCurve.brightness, params.labCurve.contrast, params.labCurve.lcurve, hist16, lumacurve, dummy, 1, utili);

        const bool clcutili = CurveFactory::diagonalCurve2Lut(params.labCurve.clcurve, clcurve, 1);

        bool ccutili, cclutili;
        CurveFactory::complexsgnCurve(autili, butili, ccutili, cclutili, params.labCurve.acurve, params.labCurve.bcurve, params.labCurve.cccurve,
                                      params.labCurve.lccurve, curve1, curve2, satcurve, lhskcurve, 1);


        if (params.colorToning.enabled && params.colorToning.method == "LabGrid") {
            ipf.colorToningLabGrid(labView, 0, labView->W, 0, labView->H, false);
        }

        ipf.shadowsHighlights(labView, params.sh.enabled, params.sh.lab, params.sh.highlights, params.sh.shadows, params.sh.radius, 1, params.sh.htonalwidth, params.sh.stonalwidth);

        if (params.localContrast.enabled) {
            // Alberto's local contrast
            ipf.localContrast(labView, labView->L, params.localContrast, false, 1);//scale);
        }

        ipf.chromiLuminanceCurve(nullptr, 1, labView, labView, curve1, curve2, satcurve, lhskcurve, clcurve, lumacurve, utili, autili, butili, ccutili, cclutili, clcutili, dummy, dummy);

        const bool cam02 = params.colorappearance.modelmethod == "02" && params.colorappearance.enabled;

    //    if ((params.colorappearance.enabled && !params.colorappearance.tonecie) || (!params.colorappearance.enabled)) {
        if ((params.colorappearance.enabled && !params.colorappearance.tonecie) || (!cam02)) {
            ipf.EPDToneMap(labView, 0, 1);
        }


        ipf.vibrance(labView, params.vibrance, params.toneCurve.hrenabled, params.icm.workingProfile);
        ipf.labColorCorrectionRegions(labView);

        // for all treatments Defringe, Sharpening, Contrast detail ,Microcontrast they are activated if "CIECAM" function are disabled

      //  if ((params.colorappearance.enabled && !settings->autocielab) || (!params.colorappearance.enabled)) {
        if ((params.colorappearance.enabled && !settings->autocielab) || (!cam02)) {
            ipf.impulsedenoise(labView);
            ipf.defringe(labView);
        }

        if (params.sharpenEdge.enabled) {
            ipf.MLsharpen(labView);
        }

        if (params.sharpenMicro.enabled) {
          //  if ((params.colorappearance.enabled && !settings->autocielab) || (!params.colorappearance.enabled)) {
            if ((params.colorappearance.enabled && !settings->autocielab) || (!cam02)) {
                ipf.MLmicrocontrast(labView);     //!params.colorappearance.sharpcie
            }
        }

      //  if (((params.colorappearance.enabled && !settings->autocielab) || (!params.colorappearance.enabled)) && params.sharpening.enabled) {
        if (((params.colorappearance.enabled && !settings->autocielab) || (!cam02)) && params.sharpening.enabled) {
            ipf.sharpening(labView, params.sharpening);

        }



        // directional pyramid wavelet
        if (params.dirpyrequalizer.cbdlMethod == "aft") {
         //   if ((params.colorappearance.enabled && !settings->autocielab)  || !params.colorappearance.enabled) {
            if ((params.colorappearance.enabled && !settings->autocielab)  || !cam02) {
                ipf.dirpyrequalizer(labView, 1);     //TODO: this is the luminance tonecurve, not the RGB one
            }
        }

        if ((params.wavelet.enabled)){
            LabImage *unshar = nullptr;
            WaveletParams WaveParams = params.wavelet;
            WavCurve wavCLVCurve;
            WavCurve wavdenoise;
            WavCurve wavdenoiseh;
            Wavblcurve wavblcurve;
            WavOpacityCurveRG waOpacityCurveRG;
            WavOpacityCurveSH waOpacityCurveSH;
            WavOpacityCurveBY waOpacityCurveBY;
            WavOpacityCurveW waOpacityCurveW;
            WavOpacityCurveWL waOpacityCurveWL;
            LabImage *provradius = nullptr;
            bool procont = WaveParams.expcontrast;
            bool prochro = WaveParams.expchroma;
            bool proedge = WaveParams.expedge;
            bool profin = WaveParams.expfinal;
            bool proton = WaveParams.exptoning;
            bool pronois = WaveParams.expnoise;

            if (WaveParams.softrad > 0.f) {
                provradius = new LabImage(*labView, true);
            }

            params.wavelet.getCurves(wavCLVCurve, wavdenoise, wavdenoiseh, wavblcurve, waOpacityCurveRG, waOpacityCurveSH, waOpacityCurveBY, waOpacityCurveW, waOpacityCurveWL);

            CurveFactory::diagonalCurve2Lut(params.wavelet.wavclCurve, wavclCurve, 1);

            if ((WaveParams.ushamethod == "sharp" || WaveParams.ushamethod == "clari") && WaveParams.expclari && WaveParams.CLmethod != "all") {
                const Glib::ustring provis = params.wavelet.CLmethod;
                params.wavelet.CLmethod = "all";
                ipf.ip_wavelet(labView, labView, 2, WaveParams, wavCLVCurve, wavdenoise, wavdenoiseh, wavblcurve, waOpacityCurveRG, waOpacityCurveSH, waOpacityCurveBY, waOpacityCurveW,  waOpacityCurveWL, wavclCurve, 1);
                unshar = new LabImage(*labView, true);
                params.wavelet.CLmethod = provis;

                WaveParams.expcontrast = false;
                WaveParams.expchroma = false;
                WaveParams.expedge = false;
                WaveParams.expfinal = false;
                WaveParams.exptoning = false;
                WaveParams.expnoise = false;
            }

            ipf.ip_wavelet(labView, labView, 2, WaveParams, wavCLVCurve, wavdenoise, wavdenoiseh, wavblcurve, waOpacityCurveRG, waOpacityCurveSH, waOpacityCurveBY, waOpacityCurveW,  waOpacityCurveWL, wavclCurve, 1);

            if ((WaveParams.ushamethod == "sharp" || WaveParams.ushamethod == "clari") && WaveParams.expclari && WaveParams.CLmethod != "all") {
                WaveParams.expcontrast = procont;
                WaveParams.expchroma = prochro;
                WaveParams.expedge = proedge;
                WaveParams.expfinal = profin;
                WaveParams.exptoning = proton;
                WaveParams.expnoise = pronois;

                if (WaveParams.softrad > 0.f) {
                    array2D<float> ble(fw, fh);
                    array2D<float> guid(fw, fh);
                    Imagefloat *tmpImage = nullptr;
                    tmpImage = new Imagefloat(fw, fh);
#ifdef _OPENMP
                    #pragma omp parallel for
#endif

                    for (int ir = 0; ir < fh; ir++)
                        for (int jr = 0; jr < fw; jr++) {
                            float X, Y, Z;
                            float L = provradius->L[ir][jr];
                            float a = provradius->a[ir][jr];
                            float b = provradius->b[ir][jr];
                            Color::Lab2XYZ(L, a, b, X, Y, Z);

                            guid[ir][jr] = Y / 32768.f;
                            float La = labView->L[ir][jr];
                            float aa = labView->a[ir][jr];
                            float ba = labView->b[ir][jr];
                            Color::Lab2XYZ(La, aa, ba, X, Y, Z);
                            tmpImage->r(ir, jr) = X;
                            tmpImage->g(ir, jr) = Y;
                            tmpImage->b(ir, jr) = Z;
                            ble[ir][jr] = Y / 32768.f;
                        }

                    double epsilmax = 0.0001;
                    double epsilmin = 0.00001;
                    double aepsil = (epsilmax - epsilmin) / 100.f;
                    double bepsil = epsilmin; //epsilmax - 100.f * aepsil;
                    double epsil = aepsil * WaveParams.softrad + bepsil;

                    float blur = 10.f / 1 * (0.5f + 0.8f * WaveParams.softrad);
                    // rtengine::guidedFilter(guid, ble, ble, blur, 0.001, multiTh);
                    rtengine::guidedFilter(guid, ble, ble, blur, epsil, false);



#ifdef _OPENMP
                    #pragma omp parallel for
#endif

                    for (int ir = 0; ir < fh; ir++)
                        for (int jr = 0; jr < fw; jr++) {
                            float X = tmpImage->r(ir, jr);
                            float Y = 32768.f * ble[ir][jr];
                            float Z = tmpImage->b(ir, jr);
                            float L, a, b;
                            Color::XYZ2Lab(X, Y, Z, L, a, b);
                            labView->L[ir][jr] = L;
                        }

                    delete tmpImage;
                }

            }

            if ((WaveParams.ushamethod == "sharp" || WaveParams.ushamethod == "clari") && WaveParams.expclari && WaveParams.CLmethod != "all") {
                float mL = (float)(WaveParams.mergeL / 100.f);
                float mC = (float)(WaveParams.mergeC / 100.f);
                float mL0;
                float mC0;

                if ((WaveParams.CLmethod == "one" || WaveParams.CLmethod == "inf")  && WaveParams.Backmethod == "black") {
                    mL0 = mC0 = 0.f;
                    mL = -1.5f * mL;
                    mC = -mC;
                } else if (WaveParams.CLmethod == "sup" && WaveParams.Backmethod == "resid") {
                    mL0 = mL;
                    mC0 = mC;
                } else {
                    mL0 = mL = mC0 = mC = 0.f;
                }


#ifdef _OPENMP
                #pragma omp parallel for
#endif

                for (int x = 0; x < fh; x++)
                    for (int y = 0; y < fw; y++) {
                        labView->L[x][y] = LIM((1.f + mL0) * (unshar->L[x][y]) - mL * labView->L[x][y], 0.f, 32768.f);
                        labView->a[x][y] = (1.f + mC0) * (unshar->a[x][y]) - mC * labView->a[x][y];
                        labView->b[x][y] = (1.f + mC0) * (unshar->b[x][y]) - mC * labView->b[x][y];
                    }

                delete unshar;
                unshar    = NULL;

                if (WaveParams.softrad > 0.f) {
                    delete provradius;
                    provradius    = NULL;
                }

            }

            wavCLVCurve.Reset();
        }

        ipf.softLight(labView, params.softlight);


        if (params.icm.workingTRC != ColorManagementParams::WorkingTrc::NONE  && params.icm.trcExp) {
            const int GW = labView->W;
            const int GH = labView->H;
            std::unique_ptr<LabImage> provis;
            const float pres = 0.01f * params.icm.preser;
            if(params.icm.trcExp) {//local contrast
                int level_hr = 7;
                int maxlevpo = 9;
                bool wavcurvecont = false;
                WaveletParams WaveParams = params.wavelet;
                ColorManagementParams Colparams = params.icm;
                WavOpacityCurveWL icmOpacityCurveWL;
                Colparams.getCurves(icmOpacityCurveWL);
                ipf.complete_local_contrast(labView, labView, WaveParams, Colparams, icmOpacityCurveWL, 1, level_hr, maxlevpo, wavcurvecont);

            }

            if (pres > 0.f && params.icm.wprim != ColorManagementParams::Primaries::DEFAULT) {
                provis.reset(new LabImage(GW, GH));
                provis->CopyFrom(labView);
            }

            const std::unique_ptr<Imagefloat> tmpImage1(new Imagefloat(GW, GH));
            const std::unique_ptr<Imagefloat> tmpImage2(new Imagefloat(GW, GH));

            ipf.lab2rgb(*labView, *tmpImage1, params.icm.workingProfile);
            tmpImage1.get()->copyData(tmpImage2.get());

            const float gamtone = params.icm.wGamma;
            const float slotone = params.icm.wSlope;

            int illum = toUnderlying(params.icm.will);
            const int prim = toUnderlying(params.icm.wprim);

            Glib::ustring prof = params.icm.workingProfile;

            cmsHTRANSFORM dummy = nullptr;
            int ill = 0;
            bool gamutcontrol = params.icm.gamut;
            int catc = toUnderlying(params.icm.wcat);
            int locprim = 0;
            const int midton = params.icm.wmidtcie;
            if(midton != 0) {
                ToneEqualizerParams params;
                params.enabled = true;
                params.regularization = 0.f;
                params.pivot = 0.f;
                params.bands[0] = 0;
                params.bands[2] = midton;
                params.bands[4] = 0;
                params.bands[5] = 0;
                int mid = abs(midton);
                int threshmid = 50;
                if(mid > threshmid) {
                    params.bands[1] = sign(midton) * (mid - threshmid);
                    params.bands[3] = sign(midton) * (mid - threshmid);     
                }
                ipf.toneEqualizer(tmpImage1.get(), params, prof, 1, false);
            }
            
            
            float rdx, rdy, grx, gry, blx, bly = 0.f;
            float meanx, meany, meanxe, meanye, maxdat = 0.f;
            double p[6] = {0., 0., 0., 0., 0., 0.};

            ipf.workingtrc(0, tmpImage1.get(), tmpImage1.get(), GW, GH, -5, prof, 2.4, 12.92310, 0, ill, 0, 0, rdx, rdy, grx, gry, blx, bly, meanx, meany, meanxe, meanye, maxdat, p, dummy, true, false, false, false);
            ipf.workingtrc(0, tmpImage1.get(), tmpImage1.get(), GW, GH, 5, prof, gamtone, slotone, catc, illum, prim, locprim, rdx, rdy, grx, gry, blx, bly, meanx, meany, meanxe, meanye, maxdat,  p, dummy, false, true, true, gamutcontrol);
            float satu = params.icm.wapsat;

            if(satu > 0.f) {
                ipf.apsatur(0, tmpImage1.get(), tmpImage2.get(), GW, GH, satu) ;
            }

            const float smoothisli = params.icm.wsmoothciesli;

            if(smoothisli > 0.f) {
                ToneEqualizerParams params;
                params.enabled = true;
                params.regularization = 0.f;
                params.pivot = 0.f;
                params.bands[0] = 0;
                params.bands[1] = 0;
                params.bands[2] = 0;
                params.bands[3] = 0;
                params.bands[4] = -40;//arbitrary value to adapt with WhiteEvjz - here White Ev # 10
                params.bands[5] = -80;//8 Ev and above
                bool Evsix = true;
                if(Evsix) {//EV = 6 majority of images
                    params.bands[4] = -30 * smoothisli;
                    float smmothsli5 = std::min(smoothisli, 1.f);
                    params.bands[5] = -80 * smmothsli5;                    
                }
                
                ipf.toneEqualizer(tmpImage1.get(), params, prof, 1, false);
            }

            ipf.rgb2lab(*tmpImage1, *labView, params.icm.workingProfile);

            // labView and provis
            if (provis) {
                ipf.preserv(labView, provis.get(), GW, GH);
            }

            if (params.icm.fbw) {
#ifdef _OPENMP
                #pragma omp parallel for
#endif

                for (int x = 0; x < GH; x++)
                    for (int y = 0; y < GW; y++) {
                        labView->a[x][y] = 0.f;
                        labView->b[x][y] = 0.f;
                    }
            }
        }

        //Colorappearance and tone-mapping associated

        int f_w = 1, f_h = 1;

        if (params.colorappearance.tonecie || params.colorappearance.enabled) {
            f_w = fw;
            f_h = fh;
        }

        CieImage *cieView = new CieImage(f_w, (f_h));

        CurveFactory::curveLightBrightColor(
            params.colorappearance.curve,
            params.colorappearance.curvered,
            params.colorappearance.curvegreen,
            params.colorappearance.curveblue,
            params.colorappearance.curve2,
            params.colorappearance.curve3,
            hist16, dummy,
            dummy, dummy,
            customColCurve1,
            customColCurvered,
            customColCurvegreen,
            customColCurveblue,
            customColCurve2,
            customColCurve3,
            1);

        if (params.colorappearance.enabled) {
            double adap;

            const float fnum = imgsrc->getMetaData()->getFNumber();         // F number
            const float fiso = imgsrc->getMetaData()->getISOSpeed() ;       // ISO
            const float fspeed = imgsrc->getMetaData()->getShutterSpeed() ; // Speed
            const float fcomp = imgsrc->getMetaData()->getExpComp();        // Compensation + -

            if (fnum < 0.3f || fiso < 5.f || fspeed < 0.00001f) {
                adap = 2000.;
            }//if no exif data or wrong
            else {
                double E_V = fcomp + log2((fnum * fnum) / fspeed / (fiso / 100.f));
                double kexp = 0.;
                E_V += kexp * params.toneCurve.expcomp;// exposure compensation in tonecurve ==> direct EV
                E_V += 0.5 * log2(params.raw.expos); // exposure raw white point ; log2 ==> linear to EV
                adap = std::pow(2.0, E_V - 3.0); //cd / m2
            }

            LUTf CAMBrightCurveJ;
            LUTf CAMBrightCurveQ;
            float CAMMean = NAN;

            float d, dj, yb;
            ipf.ciecam_02float(cieView, float (adap), 1, 2, labView, &params, customColCurve1, customColCurvered, customColCurvegreen, customColCurveblue, customColCurve2, customColCurve3, dummy, dummy, CAMBrightCurveJ, CAMBrightCurveQ, CAMMean, 0, 1, true, d, dj, yb, 1);
        }

        delete cieView;
        cieView = nullptr;

        bool exec = params.icm.wgamut != ColorManagementParams::Wwgamut::NONE  || params.icm.wgamgain != 0.f;

        if (params.icm.workingTRC != ColorManagementParams::WorkingTrc::NONE && params.icm.trcExp  && exec) {

            //compression gamut and gain at the end of process
            const int GW = labView->W;
            const int GH = labView->H;
            TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params.icm.workingProfile);
                const double wp[3][3] = {
                    {wprof[0][0], wprof[0][1], wprof[0][2]},
                    {wprof[1][0], wprof[1][1], wprof[1][2]},
                    {wprof[2][0], wprof[2][1], wprof[2][2]}
                };
            TMatrix wiprof = ICCStore::getInstance()->workingSpaceInverseMatrix(params.icm.workingProfile);
                const double wip[3][3] = {//improve precision with double
                    {wiprof[0][0], wiprof[0][1], wiprof[0][2]},
                    {wiprof[1][0], wiprof[1][1], wiprof[1][2]},
                    {wiprof[2][0], wiprof[2][1], wiprof[2][2]}
                };
            std::unique_ptr<Imagefloat> provcomp(new Imagefloat(GW, GH));

#ifdef _OPENMP
        #   pragma omp parallel for
#endif
                for (int i = 0; i < GH; ++i){
                    for (int j = 0; j < GW; ++j) {
                        float X, Y, Z = 0.f;
                        Color::Lab2XYZ(labView->L[i][j], labView->a[i][j], labView->b[i][j] , X, Y, Z);
                        Color::xyz2rgb(X, Y, Z, provcomp->r(i, j), provcomp->g(i, j), provcomp->b(i, j), wp);
                    }
                }

                const float gainev = pow_F(2.f, (float) params.icm.wgamgain);
                if (params.icm.wgamgain != 0.f) {//Final gain in Ev
           
#ifdef _OPENMP
        #   pragma omp parallel for
#endif
                    for (int i = 0; i < GH; ++i){
                        for (int j = 0; j < GW; ++j) {
                            provcomp->r(i, j) *= gainev;
                            provcomp->g(i, j) *= gainev;
                            provcomp->b(i, j) *= gainev;
                        }
                    }
                }

                float mac = 0.f;
                float mac0 = 0.f;
                float mac1 = 0.f;
                float mac2 = 0.f;
                int beginend = 1;
                int nbsegam = 0;
                float powe = 1.f;
                if (params.icm.wgamut != ColorManagementParams::Wwgamut::NONE) {
                    ipf.gamutcompr(provcomp.get(), provcomp.get(), beginend, powe, nbsegam, mac, mac0, mac1, mac2);
                }

#ifdef _OPENMP
        #   pragma omp parallel for
#endif
                for (int i = 0; i < GH; ++i){
                    for (int j = 0; j < GW; ++j) {
                        float x, y, z = 0.f;
                        Color::rgbxyz (provcomp->r(i, j), provcomp->g(i, j), provcomp->b(i, j), x, y, z, wip);
                        Color::XYZ2Lab(x, y, z, labView->L[i][j], labView->a[i][j], labView->b[i][j]);
                    }
                }
        }


        // end tile processing...???
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        if (pl) {
            pl->setProgress(0.60);
        }

        // crop and convert to rgb16
        int cx = 0, cy = 0, cw = labView->W, ch = labView->H;
        if (params.crop.enabled) {
            cx = params.crop.x;
            cy = params.crop.y;
            cw = params.crop.w;
            ch = params.crop.h;
        }

        ImProcFunctions::FramingArgs framingArgs;
        framingArgs.params = &params;
        framingArgs.cropWidth = cw;
        framingArgs.cropHeight = ch;
        {
            int imw, imh;
            double tmpScale = ipf.resizeScale(&params, fw, fh, imw, imh);
            framingArgs.resizeWidth = imw;
            framingArgs.resizeHeight = imh;
            framingArgs.resizeScale = tmpScale;

            // If upscaling is not permitted, keep original sizing
            if ((cw < imw || ch < imh) && !params.resize.allowUpscaling) {
                framingArgs.resizeWidth = cw;
                framingArgs.resizeHeight = ch;
                framingArgs.resizeScale = 1.0;
            }
        }

        // If framing is not enabled, resize values simply pass through to output
        ImProcFunctions::FramingData framingData = ipf.framing(framingArgs);
        if (settings->verbose) {
            printf("Framing Parameters (enabled=%s)\n", framingData.enabled ? "yes" : "no");
            printf("  Crop: w=%d h=%d\n", cw, ch);
            printf("  Original resize: w=%d h=%d s=%f\n",
                   framingArgs.resizeWidth, framingArgs.resizeHeight, framingArgs.resizeScale);
            printf("  Framed image size: w=%d h=%d s=%f\n",
                   framingData.imgWidth, framingData.imgHeight, framingData.scale);
            printf("  Total size: w=%d h=%d\n",
                   framingData.framedWidth, framingData.framedHeight);
        }

        bool labResize = params.resize.enabled && params.resize.method != "Nearest" &&
            (framingData.scale != 1.0 || params.prsharpening.enabled || framingData.enabled);

        LabImage *tmplab = nullptr;
        if (params.crop.enabled) {
            if (labResize) { // crop lab data
                tmplab = new LabImage(cw, ch);

                for (int row = 0; row < ch; row++) {
                    for (int col = 0; col < cw; col++) {
                        tmplab->L[row][col] = labView->L[row + cy][col + cx];
                        tmplab->a[row][col] = labView->a[row + cy][col + cx];
                        tmplab->b[row][col] = labView->b[row + cy][col + cx];
                    }
                }

                delete labView;
                labView = tmplab;
                cx = 0;
                cy = 0;
            }
        }

        if (labResize) { // resize lab data
            int imw = framingData.imgWidth;
            int imh = framingData.imgHeight;
            if (labView->W != imw || labView->H != imh) {
                // resize image
                tmplab = new LabImage(imw, imh);
                ipf.Lanczos(labView, tmplab, framingData.scale);
                delete labView;
                labView = tmplab;
            }

            cw = labView->W;
            ch = labView->H;

            if (params.prsharpening.enabled) {
                for (int i = 0; i < ch; i++) {
                    for (int j = 0; j < cw; j++) {
                        labView->L[i][j] = labView->L[i][j] < 0.f ? 0.f : labView->L[i][j];
                    }
                }

                ipf.sharpening(labView, params.prsharpening);
            }
        }

      //  bool bwonly = params.blackwhite.enabled && !params.colorToning.enabled && !autili && !butili && !params.colorappearance.enabled;
        bool bwonly = params.blackwhite.enabled && !params.colorToning.enabled && !autili && !butili && !cam02;

        ///////////// Custom output gamma has been removed, the user now has to create
        ///////////// a new output profile with the ICCProfileCreator

        // if Default gamma mode: we use the profile selected in the "Output profile" combobox;
        // gamma come from the selected profile, otherwise it comes from "Free gamma" tool

        Imagefloat* readyImg = ipf.lab2rgbOut(labView, cx, cy, cw, ch, params.icm);

        if (settings->verbose) {
            printf("Output profile_: \"%s\"\n", params.icm.outputProfile.c_str());
        }

        delete labView;
        labView = nullptr;

        if (bwonly) { //force BW r=g=b
            if (settings->verbose) {
                printf("Force BW\n");
            }

            for (int ccw = 0; ccw < cw; ccw++) {
                for (int cch = 0; cch < ch; cch++) {
                    readyImg->r(cch, ccw) = readyImg->g(cch, ccw);
                    readyImg->b(cch, ccw) = readyImg->g(cch, ccw);
                }
            }
        }

        if (pl) {
            pl->setProgress(0.70);
        }

        if (framingData.scale != 1.0 && params.resize.method == "Nearest") {
            int imw = framingData.imgWidth;
            int imh = framingData.imgHeight;
            if (readyImg->getWidth() != imw || readyImg->getHeight() != imh) {
                // resize rgb data (gamma applied)
                Imagefloat* tempImage = new Imagefloat(imw, imh);
                ipf.resize(readyImg, tempImage, framingData.scale);
                delete readyImg;
                readyImg = tempImage;
            }
        }

        if (framingData.enabled) {
            readyImg = ipf.drawFrame(readyImg, params.framing, framingData);
        }

        Exiv2Metadata info(imgsrc->getFileName());

        switch (params.metadata.mode) {
            case MetaDataParams::TUNNEL:
                readyImg->setMetadata(std::move(info));
                break;

            case MetaDataParams::EDIT:
                info.setExif(params.metadata.exif);
                info.setIptc(params.metadata.iptc);

                if (!(params.metadata.exifKeys.size() == 1 && params.metadata.exifKeys[0] == "*")) {
                    info.setExifKeys(&(params.metadata.exifKeys));
                }

                readyImg->setMetadata(std::move(info));
                break;

            default: // case MetaDataParams::STRIP
                // nothing to do
                break;
        }


        // Setting the output curve to readyImg
        // use the selected output profile if present, otherwise use LCMS2 profile generate by lab2rgb16 w/ gamma

        if (!params.icm.outputProfile.empty() && params.icm.outputProfile != ColorManagementParams::NoICMString) {

            // if ICCStore::getInstance()->getProfile send back an object, then ICCStore::getInstance()->getContent will do too
            cmsHPROFILE jprof = ICCStore::getInstance()->getProfile(params.icm.outputProfile);  //get outProfile

            if (jprof == nullptr) {
                if (settings->verbose) {
                    printf("\"%s\" ICC output profile not found!\n - use LCMS2 substitution\n", params.icm.outputProfile.c_str());
                }
            } else {
                if (settings->verbose) {
                    printf("Using \"%s\" output profile\n", params.icm.outputProfile.c_str());
                }

                ProfileContent pc = ICCStore::getInstance()->getContent(params.icm.outputProfile);
                readyImg->setOutputProfile(pc.getData());
            }
        } else {
            // No ICM
            readyImg->setOutputProfile({});
        }

//    t2.set();
//    if( settings->verbose )
//           printf("Total:- %d usec\n", t2.etime(t1));

        if (!job->initialImage) {
            initialImage->decreaseRef();
        }

        delete job;

        if (pl) {
            pl->setProgress(0.75);
        }

        /*  curve1.reset();curve2.reset();
            curve.reset();
            satcurve.reset();
            lhskcurve.reset();

            rCurve.reset();
            gCurve.reset();
            bCurve.reset();
            hist16.reset();
            hist16C.reset();
        */
        return readyImg;
    }

    void stage_early_resize()
    {
        procparams::ProcParams& params = job->pparams;
        //ImProcFunctions ipf (&params, true);
        ImProcFunctions &ipf = * (ipf_p.get());

        int imw, imh;
        double scale_factor = ipf.resizeScale(&params, fw, fh, imw, imh);

        std::unique_ptr<LabImage> tmplab(new LabImage(fw, fh));
        ipf.rgb2lab(*baseImg, *tmplab, params.icm.workingProfile);

        if (params.crop.enabled) {
            int cx = params.crop.x;
            int cy = params.crop.y;
            int cw = params.crop.w;
            int ch = params.crop.h;

            std::unique_ptr<LabImage> cropped(new LabImage(cw, ch));

            for (int row = 0; row < ch; row++) {
                for (int col = 0; col < cw; col++) {
                    cropped->L[row][col] = tmplab->L[row + cy][col + cx];
                    cropped->a[row][col] = tmplab->a[row + cy][col + cx];
                    cropped->b[row][col] = tmplab->b[row + cy][col + cx];
                }
            }

            tmplab = std::move(cropped);
        }

        assert(params.resize.enabled);

        // resize image
        if (params.resize.allowUpscaling || (imw <= fw && imh <= fh)) {
            std::unique_ptr<LabImage> resized(new LabImage(imw, imh));
            ipf.Lanczos(tmplab.get(), resized.get(), scale_factor);
            tmplab = std::move(resized);
        }

        adjust_procparams(scale_factor);

        fw = imw;
        fh = imh;

        delete baseImg;
        baseImg = new Imagefloat(fw, fh);
        ipf.lab2rgb(*tmplab, *baseImg, params.icm.workingProfile);
    }

    void adjust_procparams(double scale_factor)
    {
        procparams::ProcParams &params = job->pparams;
        procparams::ProcParams defaultparams;

        params.resize.enabled = false;
        params.crop.enabled = false;

        if (params.prsharpening.enabled) {
            params.sharpening = params.prsharpening;
        } else {
            params.sharpening.radius *= scale_factor;
            params.sharpening.deconvradius *= scale_factor;
        }

        params.impulseDenoise.thresh *= scale_factor;

        if (scale_factor < 0.5) {
            params.impulseDenoise.enabled = false;
        }

        params.wavelet.strength *= scale_factor;
        double noise_factor = (1.0 - scale_factor);
        params.dirpyrDenoise.luma *= noise_factor; // * scale_factor;
        //params.dirpyrDenoise.Ldetail += (100 - params.dirpyrDenoise.Ldetail) * scale_factor;
        auto &lcurve = params.dirpyrDenoise.lcurve;

        for (size_t i = 2; i < lcurve.size(); i += 4) {
            lcurve[i] *= min(noise_factor /* * scale_factor*/, 1.0);
        }

        noiseLCurve.Set(lcurve);
        const char *medmethods[] = { "soft", "33", "55soft", "55", "77", "99" };

        if (params.dirpyrDenoise.median) {
            auto &key = params.dirpyrDenoise.methodmed == "RGB" ? params.dirpyrDenoise.rgbmethod : params.dirpyrDenoise.medmethod;

            for (int i = 1; i < int (sizeof(medmethods) / sizeof(const char *)); ++i) {
                if (key == medmethods[i]) {
                    int j = i - int (1.0 / scale_factor);

                    if (j < 0) {
                        params.dirpyrDenoise.median = false;
                    } else {
                        key = medmethods[j];
                    }

                    break;
                }
            }
        }

        params.epd.scale *= scale_factor;
        //params.epd.edgeStopping *= scale_factor;

        const double dirpyreq_scale = min(scale_factor * 1.5, 1.0);

        for (int i = 0; i < 6; ++i) {
            adjust_radius(defaultparams.dirpyrequalizer.mult[i], dirpyreq_scale,
                          params.dirpyrequalizer.mult[i]);
        }

        params.dirpyrequalizer.threshold *= scale_factor;

        adjust_radius(defaultparams.defringe.radius, scale_factor,
                      params.defringe.radius);
        params.sh.radius *= scale_factor;
        params.localContrast.radius *= scale_factor;

        if (params.raw.xtranssensor.method == procparams::RAWParams::XTransSensor::getMethodString(procparams::RAWParams::XTransSensor::Method::THREE_PASS)) {
            params.raw.xtranssensor.method = procparams::RAWParams::XTransSensor::getMethodString(procparams::RAWParams::XTransSensor::Method::ONE_PASS);
        }

        if (params.raw.bayersensor.method == procparams::RAWParams::BayerSensor::getMethodString(procparams::RAWParams::BayerSensor::Method::PIXELSHIFT)) {
            params.raw.bayersensor.method = procparams::RAWParams::BayerSensor::getMethodString(procparams::RAWParams::BayerSensor::Method::RCD);
        }

        // Use Rcd instead of Amaze for fast export
        if (params.raw.bayersensor.method == procparams::RAWParams::BayerSensor::getMethodString(procparams::RAWParams::BayerSensor::Method::AMAZE)) {
            params.raw.bayersensor.method = procparams::RAWParams::BayerSensor::getMethodString(procparams::RAWParams::BayerSensor::Method::RCD);
        }
    }

private:
    ProcessingJobImpl* job;
    int& errorCode;
    ProgressListener* pl;
    bool flush;

    // internal state
    std::unique_ptr<ImProcFunctions> ipf_p;
    InitialImage *initialImage;
    ImageSource *imgsrc;
    int fw;
    int fh;

    int tr;
    PreviewProps pp;

    NoiseCurve noiseLCurve;
    NoiseCurve noiseCCurve;
    Imagefloat *calclum;
    float autoNR;
    float autoNRmax;
    int tilesize;
    int overlap;

    float *ch_M;
    float *max_r;
    float *max_b;
    float *min_b;
    float *min_r;
    float *lumL;
    float *chromC;
    float *ry;
    float *sk;
    float *pcsk;

    double expcomp;
    int bright;
    int contr;
    int black;
    int hlcompr;
    int hlcomprthresh;

    ColorTemp currWB;
    Imagefloat *baseImg;
    LabImage* labView;

    LUTu hist16;

    LUTf curve1;
    LUTf curve2;
    LUTf curve;
    LUTf satcurve;
    LUTf lhskcurve;
    LUTf lumacurve;
    LUTf clcurve;
    LUTf clToningcurve;
    LUTf cl2Toningcurve;
    LUTf wavclCurve;

    LUTf rCurve;
    LUTf gCurve;
    LUTf bCurve;
    LUTu dummy;

    ToneCurve customToneCurve1, customToneCurve2;
    ColorGradientCurve ctColorCurve;
    OpacityCurve ctOpacityCurve;
    ColorAppearance customColCurve1, customColCurvered, customColCurvegreen, customColCurveblue, customColCurve2, customColCurve3 ;
    ToneCurve customToneCurvebw1;
    ToneCurve customToneCurvebw2;

    bool autili, butili;
};

} // namespace


IImagefloat* processImage(ProcessingJob* pjob, int& errorCode, ProgressListener* pl, bool flush)
{
    ImageProcessor proc(pjob, errorCode, pl, flush);
    return proc();
}

void batchProcessingThread(ProcessingJob* job, BatchProcessingListener* bpl)
{
    ProcessingJob* currentJob = job;

    while (currentJob) {
        int errorCode;
        IImagefloat* img = processImage(currentJob, errorCode, bpl, true);

        if (errorCode) {
            bpl->error(M("MAIN_MSG_CANNOTLOAD"));
            currentJob = nullptr;
        } else {
            try {
                currentJob = bpl->imageReady(img);
            } catch (Glib::Exception& ex) {
                bpl->error(ex.what());
                currentJob = nullptr;
            }
        }
    }
}

void startBatchProcessing(ProcessingJob* job, BatchProcessingListener* bpl)
{

    if (bpl) {
        Glib::Thread::create(sigc::bind(sigc::ptr_fun(batchProcessingThread), job, bpl), 0, true, true, Glib::THREAD_PRIORITY_LOW);
    }

}

}
