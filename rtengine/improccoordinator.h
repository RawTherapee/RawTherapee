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

#include <memory>

#include "array2D.h"
#include "colortemp.h"
#include "curves.h"
#include "dcrop.h"
#include "imagesource.h"
#include "improcfun.h"
#include "LUT.h"
#include "rtengine.h"

#include "rtgui/threadutils.h"

namespace Glib
{
class Thread;
}

namespace rtengine
{

using namespace procparams;

class Crop;
class TweakOperator;

/** @brief Manages the image processing, espc. of the preview windows
  *
  * There is one ImProcCoordinator per edit panel.
  *
  * The ImProcCoordinator handle an sized down image representation of the full image, that is used when paning
  * and in the Navigator object.
  *
  * Each ImProcCoordinator handles an rtengine::Crop list, which process images too with their own pipeline,
  * but using this class' LUT and other precomputed parameters. The main preview area is displaying a non framed Crop object,
  * while detail windows are framed Crop objects.
  */
class ImProcCoordinator final : public StagedImageProcessor, public HistogramObservable
{

    friend class Crop;

protected:
    Imagefloat *orig_prev;
    Imagefloat *oprevi;
    Imagefloat *spotprev;
    LabImage *oprevl;
    LabImage *nprevl;
    Imagefloat *fattal_11_dcrop_cache; // global cache for ToneMapFattal02 used in 1:1 detail windows (except when denoise is active)
    Image8 *previmg;  // displayed image in monitor color space, showing the output profile as well (soft-proofing enabled, which then correspond to workimg) or not
    Image8 *workimg;  // internal image in output color space for analysis
    CieImage *ncie;

    ImageSource* imgsrc;

    ColorTemp currWB;
    ColorTemp currWBcust;
    ColorTemp autoWB;
    ColorTemp currWBloc;
    ColorTemp autoWBloc;

    double lastAwbEqual;
    StandardObserver lastAwbObserver{ColorTemp::DEFAULT_OBSERVER};
    double lastAwbTempBias;
    Glib::ustring lastAwbauto;

    Glib::ustring monitorProfile;
    RenderingIntent monitorIntent;
    bool softProof;
    bool gamutCheck;
    bool sharpMask;
    bool sharpMaskChanged;
    int scale;
    bool highDetailPreprocessComputed;
    bool highDetailRawComputed;
    bool allocated;

    void freeAll();

    // Precomputed values used by DetailedCrop ----------------------------------------------

    float bwAutoR, bwAutoG, bwAutoB;
    float CAMMean;
    LUTf hltonecurve;
    LUTf shtonecurve;
    LUTf tonecurve;

    LUTf lumacurve;
    LUTf chroma_acurve;
    LUTf chroma_bcurve;
    LUTf satcurve;
    LUTf lhskcurve;
    LUTf clcurve;
//    multi_array2D<float, 3> conversionBuffer;
    multi_array2D<float, 4> conversionBuffer;
    LUTf wavclCurve;
    LUTf clToningcurve;
    LUTf cl2Toningcurve;
    LUTf Noisecurve;
    LUTf NoiseCCcurve;

    LUTu vhist16, vhist16bw;
    LUTu lhist16CAM;
    LUTu lhist16CCAM;
    LUTu lhist16RETI;
    LUTu lhist16CLlad, lhist16LClad;
    LUTu histRed, histRedRaw;
    LUTu histGreen, histGreenRaw;
    LUTu histBlue, histBlueRaw;
    LUTu histLuma, histToneCurve, histToneCurveBW, histLCurve, histCCurve;
    LUTu histLLCurve, histLCAM, histCCAM, histClad, bcabhist, histChroma, histLRETI;
    bool hist_lrgb_dirty;
    /// Used to simulate a lazy update of the raw histogram.
    bool hist_raw_dirty;
    int vectorscopeScale;
    bool vectorscope_hc_dirty, vectorscope_hs_dirty;
    array2D<int> vectorscope_hc, vectorscope_hs;
    /// Waveform's intensity. Same as height of reference image.
    int waveformScale;
    bool waveform_dirty;
    array2D<int> waveformRed, waveformGreen, waveformBlue, waveformLuma;

    LUTf CAMBrightCurveJ, CAMBrightCurveQ;

    LUTf rCurve;
    LUTf gCurve;
    LUTf bCurve;
    ToneCurve customToneCurve1;
    ToneCurve customToneCurve2;
    ColorGradientCurve ctColorCurve;
    OpacityCurve ctOpacityCurve;
    NoiseCurve noiseLCurve;
    NoiseCurve noiseCCurve;
    WavCurve wavCLVCurve;
    WavCurve wavdenoise;
    WavCurve wavdenoiseh;
    Wavblcurve wavblcurve;
    WavOpacityCurveRG waOpacityCurveRG;
    WavOpacityCurveSH waOpacityCurveSH;
    WavOpacityCurveBY waOpacityCurveBY;
    WavOpacityCurveW waOpacityCurveW;
    WavOpacityCurveWL waOpacityCurveWL;
    WavOpacityCurveWL icmOpacityCurveWL;
    RetinextransmissionCurve dehatransmissionCurve;
    RetinexgaintransmissionCurve dehagaintransmissionCurve;

    ColorAppearance customColCurve1;
    ColorAppearance customColCurvered;
    ColorAppearance customColCurvegreen;
    ColorAppearance customColCurveblue;
    ColorAppearance customColCurve2;
    ColorAppearance customColCurve3;
    ToneCurve beforeToneCurveBW;
    ToneCurve afterToneCurveBW;

    LUTu rcurvehist, rcurvehistCropped, rbeforehist;
    LUTu gcurvehist, gcurvehistCropped, gbeforehist;
    LUTu bcurvehist, bcurvehistCropped, bbeforehist;

    // ------------------------------------------------------------------------------------

    int fw, fh, tr, fullw, fullh;
    int pW, pH;

    ProgressListener* plistener;
    PreviewImageListener* imageListener;
    AutoExpListener* aeListener;
    AutoCamListener* acListener;
    AutoBlackListener* ablListener;
    AutoBlackxListener* ablxListener;
    AutoBWListener* abwListener;
    AutoWBListener* awbListener;
    FlatFieldAutoClipListener *flatFieldAutoClipListener;
    AutoContrastListener *bayerAutoContrastListener;
    AutoContrastListener *xtransAutoContrastListener;
    AutoContrastListener *pdSharpenAutoContrastListener;
    AutoRadiusListener *pdSharpenAutoRadiusListener;
    FrameCountListener *frameCountListener;
    ImageTypeListener *imageTypeListener;
    FilmNegListener *filmNegListener;
    AutoColorTonListener* actListener;
    AutoprimListener* primListener;
    AutoChromaListener* adnListener;
    WaveletListener* awavListener;
    RetinexListener* dehaListener;
    CompgamutListener* acmaxListener;
  
//    LocallabListener* locallListener;

    
    HistogramListener* hListener;
    std::vector<SizeListener*> sizeListeners;

    std::vector<Crop*> crops;

    bool resultValid;

    MyMutex minit;  // to gain mutually exclusive access to ... to what exactly?

    void backupParams();
    void restoreParams();
    void allocCache (Imagefloat* &imgfloat);
    void notifyHistogramChanged();
    void reallocAll();
    /// Updates L, R, G, and B histograms. Returns true unless not updated.
    bool updateLRGBHistograms();
    /// Updates the H-C vectorscope. Returns true unless not updated.
    bool updateVectorscopeHC();
    /// Updates the H-S vectorscope. Returns true unless not updated.
    bool updateVectorscopeHS();
    /// Updates all waveforms. Returns true unless not updated.
    bool updateWaveforms();
    void setScale(int prevscale);
    void updatePreviewImage (int todo, bool panningRelatedChange);

    MyMutex mProcessing;
    const std::unique_ptr<ProcParams> params;  // used for the rendering, can be eventually tweaked
    std::unique_ptr<ProcParams> paramsBackup;  // backup of the untweaked procparams
    TweakOperator* tweakOperator;

    // for optimization purpose, the output profile, output rendering intent and
    // output BPC will trigger a regeneration of the profile on parameter change only
    // and automatically
    Glib::ustring lastOutputProfile;
    RenderingIntent lastOutputIntent;
    bool lastOutputBPC;

    // members of the updater:
    Glib::Thread* thread;
    MyMutex updaterThreadStart;
    MyMutex paramsUpdateMutex;
    int  changeSinceLast;
    bool updaterRunning;
    const std::unique_ptr<ProcParams> nextParams;
    bool destroying;
    bool utili;
    bool autili;
    bool butili;
    bool ccutili;
    bool cclutili;
    bool clcutili;
    bool opautili;
    bool wavcontlutili;
    void startProcessing();
    void process();
    float colourToningSatLimit;
    float colourToningSatLimitOpacity;
    bool highQualityComputed;
    cmsHTRANSFORM customTransformIn;
    cmsHTRANSFORM customTransformOut;
    ImProcFunctions ipf;
    
    //locallab
    LocallabListener* locallListener;
    LUTf lllocalcurve;
    LUTf cllocalcurve;
    LUTf lclocalcurve;
    LUTf cclocalcurve;
    LUTf rgblocalcurve;
    LUTf exlocalcurve;
    LUTf hltonecurveloc;
    LUTf shtonecurveloc;
    LUTf tonecurveloc;
    LUTf lightCurveloc;
    LUTf lmasklocalcurve;
    LUTf lmaskexplocalcurve;
    LUTf lmaskSHlocalcurve;
    LUTf ghslocalcurve;
    LUTf lmaskviblocalcurve;
    LUTf lmasktmlocalcurve;
    LUTf lmaskretilocalcurve;
    LUTf lmaskcblocalcurve;
    LUTf lmaskbllocalcurve;
    LUTf lmasklclocalcurve;
    LUTf lmaskloglocalcurve;
    LUTf lmasklocal_curve;
    LUTf lmaskcielocalcurve;
    LUTf cielocalcurve;
    LUTf cielocalcurve2;
    LUTf jzlocalcurve;
    LUTf czlocalcurve;
    LUTf czjzlocalcurve;
    LUTf redlocalcurve;
    LUTf greenlocalcurve;
    LUTf bluelocalcurve;
    
    LocretigainCurve locRETgainCurve;
    LocretitransCurve locRETtransCurve;
    LocretigainCurverab locRETgainCurverab;
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
    LocCCmaskCurve locccmas_Curve;
    LocLLmaskCurve locllmas_Curve;
    LocHHmaskCurve lochhmas_Curve;
    LocHHmaskCurve lochhhmas_Curve;
    LocCCmaskCurve locccmaslogCurve;
    LocLLmaskCurve locllmaslogCurve;
    LocHHmaskCurve lochhmaslogCurve;
    LocCCmaskCurve locccmascieCurve;
    LocLLmaskCurve locllmascieCurve;
    LocHHmaskCurve lochhmascieCurve;
    
    LocwavCurve locwavCurve;
    LocwavCurve loclmasCurveblwav;
    LocwavCurve loclmasCurvecolwav;
    LocwavCurve loclmasCurveciewav;
    LocwavCurve loclevwavCurve;
    LocwavCurve locconwavCurve;
    LocwavCurve loccompwavCurve;
    LocwavCurve loccomprewavCurve;
    LocwavCurve locwavCurveden;
    LocwavCurve locedgwavCurve;
    LocwavCurve loclmasCurve_wav;
    LocwavCurve locwavCurvehue;
    LocwavCurve locwavCurvehuecont;
    LocwavCurve locwavCurvejz;

    std::vector<float> huerefs;
    std::vector<float> huerefblurs;
    std::vector<float> chromarefblurs;
    std::vector<float> lumarefblurs;
    std::vector<float> chromarefs;
    std::vector<float> lumarefs;
    std::vector<float> sobelrefs;
    std::vector<float> avgs;
    std::vector<float> meantms;
    std::vector<float> stdtms;
    std::vector<float> meanretis;
    std::vector<float> stdretis;
    
    bool lastspotdup;
    bool previewDeltaE;
    int locallColorMask;
    int locallColorMaskinv;
    int locallExpMask;
    int locallExpMaskinv;
    int locallSHMask;
    int locallSHMaskinv;
    int locallvibMask;
    int localllcMask;
    int locallcbMask;
    int locallretiMask;
    int locallsoftMask;
    int localltmMask;
    int locallblMask;
    int locallsharMask;
    int localllogMask;
    int locall_Mask;
    int locallcieMask;

public:

    ImProcCoordinator ();
    ~ImProcCoordinator () override;
    void assign     (ImageSource* imgsrc);

    void        getParams (procparams::ProcParams* dst, bool tweaked=false) override;

    void        startProcessing (int changeCode) override;
    ProcParams* beginUpdateParams () override;
    void        endUpdateParams (ProcEvent change) override;  // must be called after beginUpdateParams, triggers update
    void        endUpdateParams (int changeFlags) override;
    void        stopProcessing () override;

    std::string *retistrsav;

    void setPreviewScale    (int scale) override
    {
        setScale(scale);
    }
    int  getPreviewScale    () override
    {
        return scale;
    }

    //void fullUpdatePreviewImage  ();

    int getFullWidth () override
    {
        return fullw;
    }
    int getFullHeight () override
    {
        return fullh;
    }

    int getPreviewWidth () override
    {
        return pW;
    }
    int getPreviewHeight () override
    {
        return pH;
    }

    DetailedCrop* createCrop  (::EditDataProvider *editDataProvider, bool isDetailWindow) override;

    void setTweakOperator (TweakOperator *tOperator) override;
    void unsetTweakOperator (TweakOperator *tOperator) override;
    bool getAutoWB   (double& temp, double& green, double equal, StandardObserver observer, double tempBias) override;
    void getCamWB    (double& temp, double& green, StandardObserver observer) override;
    void getSpotWB   (int x, int y, int rectSize, double& temp, double& green) override;
    bool getFilmNegativeSpot(int x, int y, int spotSize, FilmNegativeParams::RGB &refInput, FilmNegativeParams::RGB &refOutput) override;
    void getAutoCrop (double ratio, int &x, int &y, int &w, int &h) override;
    bool getHighQualComputed() override;
    void setHighQualComputed() override;
    void setMonitorProfile (const Glib::ustring& profile, RenderingIntent intent) override;
    void getMonitorProfile (Glib::ustring& profile, RenderingIntent& intent) const override;
    void setSoftProofing   (bool softProof, bool gamutCheck) override;
    void getSoftProofing   (bool &softProof, bool &gamutCheck) override;
    ProcEvent setSharpMask (bool sharpMask) override;
    bool updateTryLock () override
    {
        return updaterThreadStart.trylock();
    }
    void updateUnLock () override
    {
        updaterThreadStart.unlock();
    }

    void setLocallabMaskVisibility(bool previewDeltaE, int locallColorMask, int locallColorMaskinv, int locallExpMask, int locallExpMaskinv, int locallSHMask, int locallSHMaskinv, int locallvibMask, int locallsoftMask, int locallblMask, int localltmMask, int locallretiMask, int locallsharMask, int localllcMask, int locallcbMask, int localllogMask, int locall_Mask, int locallcieMask) override
    {
        this->previewDeltaE = previewDeltaE;
        this->locallColorMask = locallColorMask;
        this->locallColorMaskinv = locallColorMaskinv;
        this->locallExpMask = locallExpMask;
        this->locallExpMaskinv = locallExpMaskinv;
        this->locallSHMask = locallSHMask;
        this->locallSHMaskinv = locallSHMaskinv;
        this->locallvibMask = locallvibMask;
        this->locallsoftMask = locallsoftMask;
        this->locallblMask = locallblMask;
        this->localltmMask = localltmMask;
        this->locallretiMask = locallretiMask;
        this->locallsharMask = locallsharMask;
        this->localllcMask = localllcMask;
        this->locallcbMask = locallcbMask;
        this->localllogMask = localllogMask;
        this->locall_Mask = locall_Mask;
        this->locallcieMask = locallcieMask;
    }

    void setProgressListener (ProgressListener* pl) override
    {
        plistener = pl;
    }
    void setPreviewImageListener    (PreviewImageListener* il) override
    {
        imageListener = il;
    }
    void setSizeListener     (SizeListener* il) override
    {
        sizeListeners.push_back(il);
    }
    void delSizeListener     (SizeListener* il) override
    {
        std::vector<SizeListener*>::iterator it = std::find(sizeListeners.begin(), sizeListeners.end(), il);

        if (it != sizeListeners.end()) {
            sizeListeners.erase(it);
        }
    }
    void setAutoExpListener  (AutoExpListener* ael) override
    {
        aeListener = ael;
    }
    void setHistogramListener (HistogramListener *h) override
    {
        if (hListener) {
            hListener->setObservable(nullptr);
        }
        hListener = h;
        if (h) {
            h->setObservable(this);
        }
    }
    void setAutoCamListener  (AutoCamListener* acl) override
    {
        acListener = acl;
    }
    void setAutoBlackListener  (AutoBlackListener* abl) override
    {
        ablListener = abl;
    }
    void setAutoBlackxListener  (AutoBlackxListener* ablx) override
    {
        ablxListener = ablx;
    }
    
    void setAutoBWListener   (AutoBWListener* abw) override
    {
        abwListener = abw;
    }
    void setAutoWBListener   (AutoWBListener* awb) override
    {
        awbListener = awb;
    }
    void setAutoColorTonListener   (AutoColorTonListener* bwct) override
    {
        actListener = bwct;
    }
    void setAutoprimListener   (AutoprimListener* pri) override
    {
        primListener = pri;
    }
    void setAutoChromaListener  (AutoChromaListener* adn) override
    {
        adnListener = adn;
    }
    void setCompgamutListener  (CompgamutListener* cop) override
    {
        acmaxListener = cop;
    }
    void setRetinexListener  (RetinexListener* adh) override
    {
        dehaListener = adh;
    }
    void setLocallabListener  (LocallabListener* lla) override
    {
        locallListener = lla;
    }
    void setWaveletListener  (WaveletListener* awa) override
    {
        awavListener = awa;
    }

    void setFrameCountListener  (FrameCountListener* fcl) override
    {
        frameCountListener = fcl;
    }

    void setFlatFieldAutoClipListener  (FlatFieldAutoClipListener* ffacl) override
    {
        flatFieldAutoClipListener = ffacl;
    }
    void setBayerAutoContrastListener  (AutoContrastListener* acl) override
    {
        bayerAutoContrastListener = acl;
    }

    void setXtransAutoContrastListener  (AutoContrastListener* acl) override
    {
        xtransAutoContrastListener = acl;
    }

    void setpdSharpenAutoRadiusListener  (AutoRadiusListener* acl) override
    {
        pdSharpenAutoRadiusListener = acl;
    }

    void setpdSharpenAutoContrastListener  (AutoContrastListener* acl) override
    {
        pdSharpenAutoContrastListener = acl;
    }

    void setImageTypeListener  (ImageTypeListener* itl) override
    {
        imageTypeListener = itl;
    }

    void setFilmNegListener  (FilmNegListener* fnl) override
    {
        filmNegListener = fnl;
    }

    void saveInputICCReference (const Glib::ustring& fname, bool apply_wb) override;

    InitialImage*  getInitialImage () override
    {
        return imgsrc;
    }

    cmsHTRANSFORM& getCustomTransformIn ()
    {
        return customTransformIn;
    }

    cmsHTRANSFORM& getCustomTransformOut ()
    {
        return customTransformOut;
    }

    struct DenoiseInfoStore {
        DenoiseInfoStore() : chM(0), max_r{}, max_b{}, ch_M{}, valid(false)  {}
        float chM;
        float max_r[9];
        float max_b[9];
        float ch_M[9];
        bool valid;

    } denoiseInfoStore;

    void requestUpdateHistogram() override;
    void requestUpdateHistogramRaw() override;
    void requestUpdateVectorscopeHC() override;
    void requestUpdateVectorscopeHS() override;
    void requestUpdateWaveform() override;
};

}
