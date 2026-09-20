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
#include <vector>

#include "coord2d.h"
#include "gamutwarning.h"
#include "imagedimensions.h"
#include "jaggedarray.h"
#include "pipettebuffer.h"
#include "array2D.h"
#include "imagesource.h"
#include <cairomm/cairomm.h>

namespace Glib
{

class ustring;

}
template<typename T>
class LUT;

using LUTu = LUT<uint32_t>;
using LUTf = LUT<float>;

template<typename T, const size_t num>
class multi_array2D;
namespace rtengine
{

class ColorAppearance;
class ColorGradientCurve;
class DCPProfile;
class DCPProfileApplyState;
class FlatCurve;
class FramesMetaData;
class LensCorrection;
class LocCCmaskCurve;
class LocLLmaskCurve;
class LocHHmaskCurve;
class LocwavCurve;
class LocretigainCurve;
class LocretitransCurve;
class LocLHCurve;
class LocHHCurve;
class NoiseCurve;
class OpacityCurve;
class PipetteBuffer;
class ToneCurve;
class WavCurve;
class Wavblcurve;
class WavOpacityCurveBY;
class WavOpacityCurveSH;
class WavOpacityCurveRG;
class WavOpacityCurveW;
class WavOpacityCurveWL;

class CieImage;
class Image8;
class Imagefloat;
class LabImage;
class wavelet_decomposition;
class ImageSource;
class ColorTemp;

namespace procparams
{

class ProcParams;

struct SpotEntry;
struct DehazeParams;
struct FattalToneMappingParams;
struct ColorManagementParams;
struct DirPyrDenoiseParams;
struct FilmNegativeParams;
struct CGParams;
struct LocalContrastParams;
struct LocallabParams;
struct SharpeningParams;
struct SoftLightParams;
struct ToneEqualizerParams;
struct VibranceParams;
struct VignettingParams;
struct WaveletParams;

}

enum RenderingIntent : int;

// From Siril.
struct ght_compute_params {
    float qlp;//protect shadows
    float q0;
    float qwp;//protect highlights - white point
    float q1;
    float q;
    float b1;
    float a1;
    float a2;
    float b2;
    float c2;
    float d2;
    float e2;
    float a3;
    float b3;
    float c3;
    float d3;
    float e3;
    float a4;
    float b4;
    float LPT;//inverse protect shadow
    float SPT;//inverse symmetric point
    float HPT;//inverse protect highlight
};

class ImProcFunctions
{
    cmsHTRANSFORM monitorTransform;
    std::unique_ptr<GamutWarning> gamutWarning;
    Cairo::RefPtr<Cairo::ImageSurface> locImage;

    const procparams::ProcParams* params;
    double scale;
    bool multiThread;

    void calcVignettingParams(int oW, int oH, const procparams::VignettingParams& vignetting, double &w2, double &h2, double& maxRadius, double &v, double &b, double &mul);
    static void rgb2lab(const Image8 &src, int x, int y, int w, int h, float L[], float a[], float b[], const procparams::ColorManagementParams &icm, bool consider_histogram_settings, bool multithread);

    void transformLuminanceOnly(Imagefloat* original, Imagefloat* transformed, int cx, int cy, int oW, int oH, int fW, int fH);
    void transformGeneral(bool highQuality, Imagefloat *original, Imagefloat *transformed, int cx, int cy, int sx, int sy, int oW, int oH, int fW, int fH, const LensCorrection *pLCPMap, bool useOriginalBuffer);
    void transformLCPCAOnly(Imagefloat *original, Imagefloat *transformed, int cx, int cy, const LensCorrection *pLCPMap, bool useOriginalBuffer);

    bool needsCA() const;
    bool needsDistortion() const;
    bool needsRotation() const;
    bool needsPerspective() const;
    bool needsScale() const;
    bool needsScaleHorizontally() const;
    bool needsScaleVertically() const;
    bool needsScaleAny() const;
    bool needsGradient() const;
    bool needsVignetting() const;
    bool needsLCP() const;
    bool needsMetadata() const;
    bool needsLensfun() const;
//   static cmsUInt8Number* Mempro = NULL;

public:
    enum class Median {
        TYPE_3X3_SOFT,
        TYPE_3X3_STRONG,
        TYPE_5X5_SOFT,
        TYPE_5X5_STRONG,
        TYPE_7X7,
        TYPE_9X9
    };
enum class BlurType {
    OFF,
    BOX,
    GAUSS
};

    double lumimul[3];
    bool show_sharpening_mask;

    struct localpass {//pass parameters to capture sharpening SE - to improve borders
        float centrx;
        float centry;
        float lx, ly;
        float lxL, lyT;
        float xstart, xend;
        float ystart, yend;
        float sizenocoboot;
    };
    
    explicit ImProcFunctions(const procparams::ProcParams* iparams, bool imultiThread = true)
        : monitorTransform(nullptr), params(iparams), scale(1), multiThread(imultiThread), lumimul{} {}
    ~ImProcFunctions();
    bool needsLuminanceOnly() const
    {
        return !(needsCA() || needsDistortion() || needsRotation() || needsPerspective() || needsLCP() || needsLensfun() || needsMetadata()) && (needsVignetting() || needsPCVignetting() || needsGradient());
    }
    void setScale(double iscale);

    bool needsTransform(int oW, int oH, int rawRotationDeg, const FramesMetaData *metadata) const;
    bool needsPCVignetting() const;

    bool filmNegativeProcess(rtengine::Imagefloat *input, rtengine::Imagefloat *output, procparams::FilmNegativeParams &fnp,
                             const procparams::RAWParams &rawParams, const rtengine::ImageSource* imgsrc, const rtengine::ColorTemp &currWB);

    void filmNegativeProcess(rtengine::Imagefloat *input, rtengine::Imagefloat *output, const procparams::FilmNegativeParams &params);

    float calcGradientFactor (const struct grad_params& gp, int x, int y);
    void firstAnalysis(const Imagefloat* const working, const procparams::ProcParams &params, LUTu & vhist16);
    void updateColorProfiles(const Glib::ustring& monitorProfile, RenderingIntent monitorIntent, bool softProof, bool gamutCheck);
    void rgbProc(Imagefloat* working, LabImage* lab, PipetteBuffer *pipetteBuffer, const LUTf& hltonecurve, const LUTf& shtonecurve, const LUTf& tonecurve,
                 int sat, const LUTf& rCurve, const LUTf& gCurve, const LUTf& bCurve, float satLimit, float satLimitOpacity, const ColorGradientCurve& ctColorCurve,
                 const OpacityCurve& ctOpacityCurve, bool opautili, const LUTf& clcurve, const LUTf& cl2curve, const ToneCurve& customToneCurve1,
                 const ToneCurve& customToneCurve2, const ToneCurve& customToneCurvebw1, const ToneCurve& customToneCurvebw2,
                 double &rrm, double &ggm, double &bbm, float &autor, float &autog, float &autob, DCPProfile *dcpProf,
                 const DCPProfileApplyState& asIn, LUTu& histToneCurve, size_t chunkSize = 1, bool measure = false);
    void rgbProc(Imagefloat* working, LabImage* lab, PipetteBuffer *pipetteBuffer, const LUTf& hltonecurve, const LUTf& shtonecurve, const LUTf& tonecurve,
                 int sat, const LUTf& rCurve, const LUTf& gCurve, const LUTf& bCurve, float satLimit, float satLimitOpacity, const ColorGradientCurve& ctColorCurve,
                 const OpacityCurve& ctOpacityCurve, bool opautili, const LUTf& clcurve, const LUTf& cl2curve, const ToneCurve& customToneCurve1,
                 const ToneCurve& customToneCurve2, const ToneCurve& customToneCurvebw1, const ToneCurve& customToneCurvebw2,
                 double &rrm, double &ggm, double &bbm, float &autor, float &autog, float &autob, double expcomp, int hlcompr,
                 int hlcomprthresh, DCPProfile *dcpProf, const DCPProfileApplyState& asIn, LUTu& histToneCurve, size_t chunkSize = 1, bool measure = false);
    void labtoning(float r, float g, float b, float &ro, float &go, float &bo, int algm, int metchrom, int twoc, float satLimit, float satLimitOpacity, const ColorGradientCurve & ctColorCurve, const OpacityCurve & ctOpacityCurve, const LUTf & clToningcurve, const LUTf & cl2Toningcurve, float iplow, float iphigh, double wp[3][3], double wip[3][3]);
    void toning2col(float r, float g, float b, float &ro, float &go, float &bo, float iplow, float iphigh, float rl, float gl, float bl, float rh, float gh, float bh, float SatLow, float SatHigh, float balanS, float balanH, float reducac, int mode, int preser, float strProtect);
    void toningsmh(float r, float g, float b, float &ro, float &go, float &bo, float RedLow, float GreenLow, float BlueLow, float RedMed, float GreenMed, float BlueMed, float RedHigh, float GreenHigh, float BlueHigh, float reducac, int mode, float strProtect);
    void toningsmh2(float r, float g, float b, float &ro, float &go, float &bo, float low[3], float satLow, float med[3], float satMed, float high[3], float satHigh, float reducac, int mode, int preser);
    void secondeg_begin(float reducac, float vend, float &aam, float &bbm);
    void secondeg_end(float reducac, float vinf, float &aa, float &bb, float &cc);

    void retreavergb(float &r, float &g, float &b);
    void moyeqt(Imagefloat* working, float &moyS, float &eqty);

    void luminanceCurve(LabImage* lold, LabImage* lnew, const LUTf &curve);
    void ciecamloc_02float(struct local_params& lp, int sp, LabImage* lab, int bfw, int bfh, int call, int sk, const LUTf& cielocalcurve, bool localcieutili, const LUTf& cielocalcurve2, bool localcieutili2, const LUTf& jzlocalcurve, bool localjzutili, const LUTf& czlocalcurve, bool localczutili, const LUTf& czjzlocalcurve, bool localczjzutili, const LocCHCurve& locchCurvejz, const LocHHCurve& lochhCurve, const LocLHCurve& loclhCurve, bool HHcurvejz, bool CHcurvejz, bool LHcurvejz, 
		const LocwavCurve& locwavCurvejz, bool locwavutilijz, float &maxicam, float &comtsig, float &lightsig);

    void ciecam_02float(CieImage* ncie, float adap, int pW, int pwb, LabImage* lab, const procparams::ProcParams* params,
                        const ColorAppearance & customColCurve1, const ColorAppearance & customColCurvered, const ColorAppearance & customColCurvegreen, const ColorAppearance & customColCurveblue, const ColorAppearance & customColCurve, const ColorAppearance & customColCurve3,
                        LUTu &histLCAM, LUTu &histCCAM, LUTf & CAMBrightCurveJ, LUTf & CAMBrightCurveQ, float &mean, int Iterates, int scale, bool execsharp, float &d, float &dj, float &yb, int rtt,
                        bool showSharpMask = false);
    void clarimerge(const struct local_params& lp, float &mL, float &mC, bool &exec, LabImage *tmpresid, int wavelet_level, int sk, int numThreads);

    void chromiLuminanceCurve(PipetteBuffer *pipetteBuffer, int pW, LabImage* lold, LabImage* lnew, const LUTf& acurve, const LUTf& bcurve, const LUTf& satcurve, const LUTf& satclcurve, const LUTf& clcurve, LUTf &curve, bool utili, bool autili, bool butili, bool ccutili, bool cclutili, bool clcutili, LUTu &histCCurve, LUTu &histLurve);
    void vibrance(LabImage* lab, const procparams::VibranceParams &vibranceParams, bool highlight, const Glib::ustring &workingProfile);         //Jacques' vibrance
    void softprocess(const LabImage* bufcolorig, array2D<float> &buflight, /* float ** bufchro, float ** buf_a, float ** buf_b, */ float rad, int bfh, int bfw, double epsilmax, double epsilmin,  float thres, int sk, bool multiThread);
    void softproc(const LabImage* bufcolorig, const LabImage* bufcolfin, float rad, int bfh, int bfw, float epsilmax, float epsilmin, float thres, int sk, bool multiThread, int flag);
//    void colorCurve       (LabImage* lold, LabImage* lnew);
    void sharpening(LabImage* lab, const procparams::SharpeningParams &sharpenParam, bool showMask = false);

    void doCapture_Sharpening_SE(Imagefloat *rgb, int bfw, int bfh, struct localpass &locp, int sk, float &sharpc, bool autoshar, float capradiu,  float deconvCo, float deconvLat, bool itcheck, bool showMask, float deconvgam);
    void CaptureDeconvSharpening_SE (float** luminance, const float* const * oldLuminance, const float * const * blend, int bfw, int bfh, struct localpass &locp, float sigma, float sigmaCornerOffset, int iterations, bool checkIterStop, double startVal, double endVal);


    void sharpeningcam(CieImage* ncie, float** buffer, bool showMask = false);
    void transform(Imagefloat* original, Imagefloat* transformed, int cx, int cy, int sx, int sy, int oW, int oH, int fW, int fH, const FramesMetaData *metadata, int rawRotationDeg, bool fullImage, bool useOriginalBuffer = false);
    void lab2monitorRgb(LabImage* lab, Image8* image);

    double resizeScale(const procparams::ProcParams* params, int fw, int fh, int &imw, int &imh);
    void resize(Imagefloat* src, Imagefloat* dst, float dScale);
    void Lanczos(const LabImage* src, LabImage* dst, float scale);
    void Lanczos(const Imagefloat* src, Imagefloat* dst, float scale);

    struct FramingArgs {
        const procparams::ProcParams* params = nullptr;
        int cropWidth = 0;
        int cropHeight = 0;
        int resizeWidth = 0;
        int resizeHeight = 0;
        double resizeScale = 1.0f;
    };
    struct FramingData {
        bool enabled = false;
        int imgWidth = 0;
        int imgHeight = 0;
        double scale = 1.0;
        int framedWidth = 0;
        int framedHeight = 0;
    };
    FramingData framing(const FramingArgs& args) const;
    Imagefloat* drawFrame(Imagefloat* rgb, const procparams::FramingParams& params,
                          const FramingData& dims) const;

    void deconvsharpening(float** luminance, float** buffer, const float* const * blend, int W, int H, const procparams::SharpeningParams &sharpenParam, double Scale);
    void deconvsharpeningloc(float** luminance, float** buffer, int W, int H, float** loctemp, int damp, double radi, int ite, int amo, int contrast, double blurrad, int sk);

    void MLsharpen(LabImage* lab); // Manuel's clarity / sharpening
    void MLmicrocontrast(float** luminance, int W, int H);   //Manuel's microcontrast
    void MLmicrocontrast(LabImage* lab);   //Manuel's microcontrast
    void MLmicrocontrastcam(CieImage* ncie);   //Manuel's microcontrast

    void impulsedenoise(LabImage* lab);   //Emil's impulse denoise
    void impulsedenoisecam(CieImage* ncie, float **buffers[3]);
    void impulse_nr(LabImage* lab, double thresh);
    void impulse_nrcam(CieImage* ncie, double thresh, float **buffers[3]);

    void dirpyrdenoise(LabImage* src);    //Emil's pyramid denoise
    void dirpyrequalizer(LabImage* lab, int scale);  //Emil's wavelet


    void EPDToneMapResid(float * WavCoeffs_L0, unsigned int Iterates, int skip, const struct cont_params& cp, int W_L, int H_L, float max0);
    void CompressDR(float *Source, int W_L, int H_L, float Compression, float DetailBoost);
    void Compresslevels(float **Source, int W_L, int H_L, float compression, float detailattenuator, float thres, float mean, float maxp, float meanN, float maxN, float madL);
    void ContrastResid(float * WavCoeffs_L0, const struct cont_params &cp, int W_L, int H_L, float max0);

    void EPDToneMap(LabImage *lab, unsigned int Iterates = 0, int skip = 1);
    void EPDToneMaplocal(int sp, LabImage *lab, LabImage *tmp1, unsigned int Iterates, int skip);
    void EPDToneMapCIE(CieImage *ncie, float a_w, float c_, int Wid, int Hei, float minQ, float maxQ, unsigned int Iterates = 0, int skip = 1);

    // pyramid denoise
//    procparams::DirPyrDenoiseParams dnparams;
    void dirpyr(LabImage* data_fine, LabImage* data_coarse, int level, LUTf &rangefn_L, LUTf &rangefn_ab,
                int pitch, int scale, const int luma, int chroma); 
    void idirpyr(LabImage* data_coarse, LabImage* data_fine, int level, LUTf &rangefn_L, LUTf & nrwt_l, LUTf & nrwt_ab,
                 int pitch, int scale, const int luma, const int chroma/*, LUTf & Lcurve, LUTf & abcurve*/);
    //locallab Local adjustments
    void maskcalccol(int call, bool invmask, bool pde, int bfw, int bfh, int oW, int oH, int tX, int tY, int tW, int tH, int xstart, int ystart, int xend, int yend, int sk, int cx, int cy, LabImage* bufcolorig, LabImage* bufmaskblurcol, LabImage* originalmaskcol, LabImage* original, LabImage* reserved, int inv, struct local_params & lp,
                 float strumask, bool astool,
                 const LocCCmaskCurve & locccmasCurve, bool lcmasutili, 
                 const LocLLmaskCurve & locllmasCurve, bool llmasutili, 
                 const LocHHmaskCurve & lochhmasCurve, bool lhmasutili, const LocHHmaskCurve & lochhhmasCurve, bool lhhmasutili,
                 bool multiThread, bool enaMask, bool showmaske, bool deltaE, bool modmask, bool zero, bool modif, float chrom, float rad, float lap, float gamma, float slope, float blendm, float blendmab, int shado, int highl, float amountcd, float anchorcd,
                 const LUTf& lmasklocalcurve, bool localmaskutili,
                 const LocwavCurve & loclmasCurvecolwav, bool lmasutilicolwav, int level_bl, int level_hl, int level_br, int level_hr,
                 int shortcu, bool delt, const float hueref, const float chromaref, const float lumaref,
                 float maxdE, float mindE, float maxdElim,  float mindElim, float iterat, float limscope, int scope, bool fftt, float blu_ma, float cont_ma, int indic, float &fab, int fw, int fh, float ksk, float kskx, float kbh, float kbw);

    void avoidcolshi(const struct local_params& lp, int sp, LabImage *transformed, LabImage *reserved, int cy, int cx, int sk);

    void deltaEforMask(float **rdE, int bfw, int bfh, LabImage* bufcolorig, const float hueref, const float chromaref, const float lumaref,
                          float maxdE, float mindE, float maxdElim,  float mindElim, float iterat, float limscope, int scope, float balance, float balanceh);
    void discrete_laplacian_threshold(float * data_out, const float * data_in, size_t nx, size_t ny, float t);
    void laplacian(const array2D<float> &src, array2D<float> &dst, int bfw, int bfh, float threshold, float ceiling, float factor, bool multiThread);
    void detail_mask(const array2D<float> &src, array2D<float> &mask,  int bfw, int bfh, float scaling, float threshold, float ceiling, float factor, BlurType blur_type, float blur, bool multithread);
    void NLMeans(float **img, int strength, int detail_thresh, int patch, int radius, float gam, int bfw, int bfh, float scale, bool multithread);
    void loccont(int bfw, int bfh, LabImage* tmp1, float rad, float stren, int sk);

    void rex_poisson_dct(float * data, size_t nx, size_t ny, double m);
    void mean_dt(const float * data, int size, double& mean_p, double& dt_p, double nbstd);
    float *cos_table(size_t size);

    void normalize_mean_dt(float *data, const float *ref, int size, float mod, float sigm, float mdef, float sdef, float mdef2, float sdef2, double nbstd);
    void retinex_pde(const float *datain, float * dataout, int bfw, int bfh, float thresh, float multy, float *dE, int show, int dEenable, int normalize);
    void exposure_pde(float *dataor, float *datain, float * dataout, int bfw, int bfh, float thresh, float mod);
    void fftw_convol_blur(float *input, float *output, int bfw, int bfh, float radius, int fftkern, int algo);
    void fftw_convol_blur2(float **input2, float **output2, int bfw, int bfh, float radius, int fftkern, int algo);
    void fftw_tile_blur(int GW, int GH, int tilssize , int max_numblox_W, int min_numblox_W, float **tmp1, int numThreads, double radius);

    void maskforretinex(int sp, int before, float ** luminance, float ** out, int W_L, int H_L, int skip,
         const LocCCmaskCurve & locccmasretiCurve, bool lcmasretiutili, const  LocLLmaskCurve & locllmasretiCurve, bool llmasretiutili, const  LocHHmaskCurve & lochhmasretiCurve, bool lhmasretiutili,
         int llretiMask, bool retiMasktmap, bool retiMask, float rad, float lap, bool pde, float gamm, float slop, float chro, float blend,
         const LUTf & lmaskretilocalcurve, bool localmaskretiutili,
         LabImage * bufreti, LabImage * bufmask, LabImage * buforig, LabImage * buforigmas, LabImage * bufmaskorigreti, bool multiThread,
         bool delt, const float hueref, const float chromaref, const float lumaref,
         float maxdE, float mindE, float maxdElim,  float mindElim, float iterat, float limscope, int scope, float balance, float balanceh, float lumask);

    //3 functions from Alberto Griggio, adapted J.Desmis 2019
    void filmGrain(Imagefloat *rgb, int isogr, int strengr, int scalegr,float divgr, int bfw, int bfh, int call, int fw, int fh);
    void log_encode(Imagefloat *rgb, struct local_params & lp, bool multiThread, int bfw, int bfh);
    void getAutoLogloc(int sp, ImageSource *imgsrc, float *sourceg, float *blackev, float *whiteev, bool *blackredu, bool *Autogr, float *sourceab, int *whits, int *blacks, int *whitslog, int *blackslog, int fw, int fh, float xsta, float xend, float ysta, float yend, int SCALE);

    void MSRLocal(int call, int sp, bool fftw, int lum, float** reducDE, LabImage * bufreti, LabImage * bufmask, LabImage * buforig, LabImage * buforigmas, LabImage * bufmaskorigreti, float** luminance, const float* const *originalLuminance,
        const int width, const int height, int bfwr, int bfhr, const procparams::LocallabParams &loc, const int skip, const LocretigainCurve &locRETgainCcurve, const LocretitransCurve &locRETtransCcurve,
        const int chrome, const int scall, const float krad, float &minCD, float &maxCD, float &mini, float &maxi, float &Tmean, float &Tsigma, float &Tmin, float &Tmax,
        const LocCCmaskCurve & locccmasretiCurve, bool lcmasretiutili, const  LocLLmaskCurve & locllmasretiCurve, bool llmasretiutili, const  LocHHmaskCurve & lochhmasretiCurve, bool lhmasretiutili, int llretiMask,
        const LUTf & lmaskretilocalcurve, bool localmaskretiutili,
        LabImage * transformed, bool retiMasktmap, bool retiMask,
        bool delt, const float hueref, const float chromaref, const float lumaref,
        float maxdE, float mindE, float maxdElim,  float mindElim, float iterat, float limscope, int scope, float balance, float balanceh, float lumask);

    void mean_sig (const float* const * const savenormL, float &meanf, float &stdf, int xStart, int xEnd, int yStart, int yEnd) const;

    void calc_ref(int sp, LabImage* original, LabImage* transformed, int cx, int cy, int oW, int oH, int sk, double &huerefblur, double &chromarefblur, double &lumarefblur, double &hueref, double &chromaref, double &lumaref, double &sobelref, float &avg, const LocwavCurve & locwavCurveden, bool locwavdenutili);
    void copy_ref(LabImage* spotbuffer, LabImage* original, LabImage* transformed, int cx, int cy, int sk, const struct local_params & lp, double &huerefspot, double &chromarefspot, double &lumarefspot);
    void paste_ref(LabImage* spotbuffer, LabImage* transformed, int cx, int cy, int sk, const struct local_params & lp);
    void Lab_Local(int call, int sp, float** shbuffer, LabImage* original, LabImage* transformed, LabImage* reserved, LabImage * savenormtm, LabImage * savenormreti, LabImage* lastorig, int fw, int fh,  int cx, int cy, int oW, int oH, int tX, int tY, int tW, int tH, int sk, const LocretigainCurve& locRETgainCcurve, const LocretitransCurve &locRETtransCcurve,
                const LUTf& lllocalcurve, bool locallutili, 
                const LUTf& cllocalcurve, bool localclutili,
                const LUTf& lclocalcurve, bool locallcutili,
                const LocLHCurve& loclhCurve, const LocHHCurve& lochhCurve, const LocCHCurve& locchCurve,
                const LocHHCurve& lochhCurvejz, const LocCHCurve& locchCurvejz, const LocLHCurve& loclhCurvejz,
                const LUTf& lmasklocalcurve, bool localmaskutili,
                const LUTf& lmaskexplocalcurve, bool localmaskexputili,
                const LUTf& lmaskSHlocalcurve, bool localmaskSHutili,
               // const LUTf& ghslocalcurve, bool localghsutili,
                const LUTf& lmaskviblocalcurve, bool localmaskvibutili,
                const LUTf& lmasktmlocalcurve, bool localmasktmutili,
                LUTf& lmaskretilocalcurve, bool localmaskretiutili,
                const LUTf& lmaskcblocalcurve, bool localmaskcbutili,
                const LUTf& lmaskbllocalcurve, bool localmaskblutili,
                const LUTf& lmasklclocalcurve, bool localmasklcutili,
                const LUTf& lmaskloglocalcurve, bool localmasklogutili,
                const LUTf& lmasklocal_curve, bool localmask_utili,
                const LUTf& lmaskcielocalcurve, bool localmaskcieutili,
                const LUTf& cielocalcurve, bool localcieutili, 
                const LUTf& cielocalcurve2, bool localcieutili2, 
                const LUTf& jzlocalcurve, bool localjzutili, 
                const LUTf& czlocalcurve, bool localczutili, 
                const LUTf& czjzlocalcurve, bool localczjzutili, 
                
                const LocCCmaskCurve& locccmasCurve, bool lcmasutili, const LocLLmaskCurve& locllmasCurve, bool llmasutili, const LocHHmaskCurve& lochhmasCurve, bool lhmasutili, const LocHHmaskCurve& llochhhmasCurve, bool lhhmasutili, const LocHHmaskCurve& llochhhmascieCurve, bool lhhmascieutili,
                const LocCCmaskCurve& locccmasexpCurve, bool lcmasexputili, const LocLLmaskCurve& locllmasexpCurve, bool llmasexputili, const LocHHmaskCurve& lochhmasexpCurve, bool lhmasexputili, 
                const LocCCmaskCurve& locccmasSHCurve, bool lcmasSHutili, const LocLLmaskCurve& locllmasSHCurve, bool llmasSHutili, const LocHHmaskCurve& lochhmasSHCurve, bool lhmasSHutili,
                const LocCCmaskCurve& locccmasvibCurve, bool lcmasvibutili, const LocLLmaskCurve& locllmasvibCurve, bool llmasvibutili, const LocHHmaskCurve& lochhmasvibCurve, bool lhmasvibutili,
                const LocCCmaskCurve& locccmascbCurve, bool lcmascbutili, const LocLLmaskCurve& locllmascbCurve, bool llmascbutili, const LocHHmaskCurve& lochhmascbCurve, bool lhmascbutili,
                const LocCCmaskCurve& locccmasretiCurve, bool lcmasretiutili, const LocLLmaskCurve& locllmasretiCurve, bool llmasretiutili, const LocHHmaskCurve& lochhmasretiCurve, bool lhmasretiutili,
                const LocCCmaskCurve& locccmastmCurve, bool lcmastmutili, const LocLLmaskCurve& locllmastmCurve, bool llmastmutili, const LocHHmaskCurve& lochhmastmCurve, bool lhmastmutili,
                const LocCCmaskCurve& locccmasblCurve, bool lcmasblutili, const LocLLmaskCurve& locllmasblCurve, bool llmasblutili, const LocHHmaskCurve& lochhmasblCurve, bool lhmasblutili,
                const LocCCmaskCurve& locccmaslcCurve, bool lcmaslcutili, const LocLLmaskCurve& locllmaslcCurve, bool llmaslcutili, const LocHHmaskCurve& lochhmaslcCurve, bool lhmaslcutili,
                const LocCCmaskCurve& locccmaslogCurve, bool lcmaslogutili, const LocLLmaskCurve& locllmaslogCurve, bool llmaslogutili, const LocHHmaskCurve& lochhmaslogCurve, bool lhmaslogutili,
                const LocCCmaskCurve& locccmas_Curve, bool lcmas_utili, const LocLLmaskCurve& locllmas_Curve, bool llmas_utili, const LocHHmaskCurve& lochhmas_Curve, bool lhmas_utili,
                const LocCCmaskCurve& locccmascieCurve, bool lcmascieutili, const LocLLmaskCurve& locllmascieCurve, bool llmascieutili, const LocHHmaskCurve& lochhmascieCurve, bool lhmascieutili,

                const LocHHmaskCurve& lochhhmas_Curve, bool lhhmas_utili,

                const LocwavCurve& loclmasCurveblwav, bool lmasutiliblwav,
                const LocwavCurve& loclmasCurvecolwav, bool lmasutilicolwav,
                const LocwavCurve& loclmasCurveciewav, bool lmasutiliciewav,
                const LocwavCurve& locwavCurve, bool locwavutili,
                const LocwavCurve& locwavCurvejz, bool locwavutilijz,
                const LocwavCurve& loclevwavCurve, bool loclevwavutili,
                const LocwavCurve& locconwavCurve, bool locconwavutili,
                const LocwavCurve& loccompwavCurve, bool loccompwavutili,
                const LocwavCurve& loccomprewavCurve, bool loccomprewavutili,
                const LocwavCurve& locwavCurvehue, bool locwavhueutili,
                const LocwavCurve& locwavCurvehuecont, bool locwavhueutilicont,
                const LocwavCurve& locwavCurveden, bool locwavdenutili,
                const LocwavCurve& locedgwavCurve, bool locedgwavutili,
                const LocwavCurve& loclmasCurve_wav, bool lmasutili_wav,
                bool LHutili, bool HHutili, bool CHutili, bool HHutilijz, bool CHutilijz, bool LHutilijz, const LUTf& cclocalcurve, bool localcutili, const LUTf& rgblocalcurve, bool localrgbutili, bool localexutili, const LUTf& exlocalcurve, const LUTf& hltonecurveloc, const LUTf& shtonecurveloc, const LUTf& tonecurveloc, const LUTf& lightCurveloc,
                double& huerefblur, double &chromarefblur, double& lumarefblur, double &hueref, double &chromaref, double &lumaref, double &sobelref, int &lastsav,
                bool prevDeltaE, int llColorMask, int llColorMaskinv, int llExpMask, int llExpMaskinv, int llSHMask, int llSHMaskinv, int llvibMask, int lllcMask, int llsharMask, int llcbMask, int llretiMask, int llsoftMask, int lltmMask, int llblMask, int lllogMask, int ll_Mask, int llcieMask,
                float &minCD, float &maxCD, float &mini, float &maxi, float &Tmean, float &Tsigma, float &Tmin, float &Tmax,
                float& meantm, float& stdtm, float& meanreti, float& stdreti, float &fab, float &maxicam, float &rdx, float &rdy, float &grx, float &gry, float &blx, float &bly, float &meanx, float &meany, float &meanxe, float &meanye, float &maxdat, int &prim, int &ill, float &contsig, float &lightsig, float &slopeg, bool &linkrgb,
                float *resi, float &sharc, float &denocont, int *ghsbpwp, float *ghsbpwpvalue, float *savmadl, float *ghsbwslider, float &ghssym, bool &ghsautsp, float *ghscolor, float &ghsmid, float &ghsmaxrgb, float &ghs3sig, float *michbwslider);

    enum class GHTStrType {
        NORMAL,
        INVERSE,
    };

    static ght_compute_params GHT_setup(float in_B, float D, float LP, float SP, float HP, GHTStrType strtype);
    static float GHT(float x, float B, float D, float LP, float SP, float HP, ght_compute_params c, GHTStrType strtype);
    static float Midtone_Tranfer_Function (float x, float mm);
    static float Inv_Midtone_Tranfer_Function (float x, float mm);

    void tone_eqcam2(ImProcFunctions *ipf, Imagefloat *rgb, int whits, int blacks, const Glib::ustring &workingProfile, double scale, bool multithread);
    void tone_eqdehaz(ImProcFunctions *ipf, Imagefloat *rgb, int whits, int blacks, const Glib::ustring &workingProfile, double scale, bool multithread);
    void tone_eqcam(ImProcFunctions *ipf, Imagefloat *rgb, int midtone, const Glib::ustring &workingProfile, double scale, bool multithread);
    void tonemapFreeman(float target_slope, float target_sloper, float target_slopeg , float target_slopeb, float white_point, float black_point, float mid_gray_scene, float mid_gray_view, bool rolloff, float smooththreshold, bool limslope, LUTf& lut, LUTf& lutr, LUTf& lutg, LUTf& lutb, int mode, bool scale, bool takeyb);
    void tonemapFreemanQ(float Q, float &Qout, float target_slope, float white_point, float black_point, float mid_gray_scene, float mid_gray_view, bool rolloff, bool takeyb);

    float get_freeman_parameters(float x, bool rolloff_, float mid_gray_scene, float gamma, float slopelim, float dr, float b, float c, float kmid);
    float rolloff_freeman_function(float x, float dr, float b, float c, float kmid);
    float scene_referred_contrast(float x, float mid_gray_scene, float gamma);
    void sigmoid_main(float r, float g, float b, float &rout, float &gout, float &bout, float middle_grey_contrast, float contrast_skewness, /* float white_point,*/ float middle_grey, float black_point, float white_point_disp);
    void sigmoid_QJ(float Q, float &Qout, float middle_grey_contrast, float contrast_skewness, float middle_grey, float black_point, float white_point_disp);

    void addGaNoise(LabImage *lab, LabImage *dst, const float mean, const float variance, const int sk);
    void BlurNoise_Localold(int call, const struct local_params& lp, LabImage* original, LabImage* transformed, const LabImage* const tmp1, int cx, int cy);
    void InverseBlurNoise_Local(LabImage * originalmask, const struct local_params& lp, const float hueref, const float chromaref,  const float lumaref, LabImage* original, LabImage* transformed, const LabImage* const tmp1, int cx, int cy, int sk);
    void InverseReti_Local(const struct local_params& lp, const float hueref, const float chromaref,  const float lumaref, LabImage* original, LabImage* transformed, const LabImage* const tmp1, int cx, int cy, int chro, int sk);
    void BlurNoise_Local(LabImage* tmp1, LabImage * originalmask, const float hueref, const float chromaref, const float lumaref, local_params& lp, LabImage* original, LabImage* transformed, int cx, int cy, int sk);
    static void strcurv_data(std::string retistr, int *s_datc, int &siz);
    void blendstruc(int bfw, int bfh, LabImage* bufcolorig, float radius, float stru, array2D<float> & blend2, int sk, bool multiThread);

    void wavcontrast4(int call, struct local_params& lp, float ** tmp, float ** tmpa, float ** tmpb, float contrast, float radblur, float radlevblur, int bfw, int bfh, int oW, int oH, int tX, int tY, int tW, int tH, int level_bl, int level_hl, int level_br, int level_hr, int sk, int numThreads, const LocwavCurve & locwavCurve, bool locwavutili, bool wavcurve,
        const LocwavCurve & loclevwavCurve, bool loclevwavutili, bool wavcurvelev, 
        const LocwavCurve & locconwavCurve, bool locconwavutili, bool wavcurvecon, 
        const LocwavCurve & loccompwavCurve, bool loccompwavutili, bool wavcurvecomp, 
        const LocwavCurve & loccomprewavCurve, bool loccomprewavutili, bool wavcurvecompre, 
        const LocwavCurve & locedgwavCurve, bool locedgwavutili,
        float sigm, float offs,int & maxlvl, float sigmadc, float deltad, float chromalev, float chromablu, bool blurlc, bool blurena, bool levelena, bool comprena, bool compreena, float compress, float thres, int fw, int fh, int cx, int cy, float ksk, float kskx, float kbh, float kbw);
        
    void wavcont(const struct local_params& lp, float ** tmp, wavelet_decomposition &wdspot, int level_bl, int maxlvl, 
                const LocwavCurve & loclevwavCurve, bool loclevwavutili, 
                const LocwavCurve & loccompwavCurve, bool loccompwavutili,
                const LocwavCurve & loccomprewavCurve, bool loccomprewavutili,
                float radlevblur, int process, float chromablu, float thres, float sigmadc, float deltad);

    void wavlc(wavelet_decomposition& wdspot, int level_bl, int level_hl, int maxlvl, int level_hr, int level_br, float ahigh, float bhigh, float alow, float blow, float sigmalc, float offslc, float strength, const LocwavCurve & locwavCurve, int numThreads);

    void wavcbd(wavelet_decomposition &wdspot, int level_bl, int maxlvl,
                const LocwavCurve& locconwavCurve, bool locconwavutili, float sigm, float offs, float chromalev, int sk);

    enum class LocalLabGradientMode {
        STANDARD,
        PLAIN_IMAGE,
    };

    void transit_shapedetect2(int sp, float meantm, float stdtm, int call, int senstype, const LabImage * bufexporig, const LabImage * bufexpfin, LabImage * originalmask, const float hueref, const float chromaref, const float lumaref, float sobelref, float meansobel, float ** blend2, struct local_params & lp, LabImage * original, LabImage * transformed, const LabImage *tmp1, LocalLabGradientMode grad, int cx, int cy, int sk);

    void transit_shapedetect_retinex(int call, int senstype, LabImage * bufexporig, LabImage * bufexpfin, LabImage * bufmask, LabImage * buforigmas, float **buflight, float **bufchro, const float hueref, const float chromaref,  const float lumaref, struct local_params & lp, LabImage * original, LabImage * transformed, int cx, int cy, int sk);
    void transit_shapedetect(int senstype, const LabImage *bufexporig, LabImage * originalmask, float **bufchro, bool HHutili, const float hueref, const float chromaref, const float lumaref, float sobelref, float meansobel, float ** blend2, const struct local_params & lp, LabImage * original, LabImage * transformed, int cx, int cy, int sk);
    void exlabLocal(local_params& lp, float strlap, int bfh, int bfw, int bfhr, int bfwr, LabImage* bufexporig, LabImage* lab, const LUTf& hltonecurve, const LUTf& shtonecurve, const LUTf& tonecurve,  const float hueref, const float lumaref, const float chromaref);
    void Exclude_Local(float **deltaso, float hueref, float chromaref, float lumaref, float sobelref, float meansobel, const struct local_params & lp, const LabImage * original, LabImage * transformed, const LabImage * rsv, const LabImage * reserv, int cx, int cy, int sk);

    void DeNoise_Local(int call, const struct local_params& lp, LabImage* originalmask, int levred, float hueref, float lumaref, float chromaref, LabImage* original, LabImage* transformed, const LabImage &tmp1, int cx, int cy, int sk);
    void DeNoise_Local2(const struct local_params& lp, LabImage* originalmask, int levred, float hueref, float lumaref, float chromaref, LabImage* original, LabImage* transformed, const LabImage &tmp1, int cx, int cy, int sk);

    void DeNoise(int sp, int call, int aut, bool noiscfactiv, const struct local_params& lp, LabImage* originalmaskbl, LabImage *  bufmaskblurbl, int levred, float huerefblur, float lumarefblur, float chromarefblur, LabImage* original, LabImage* transformed, 
        int cx, int cy, int sk, const LocwavCurve& locwavCurvehue, bool locwavhueutili, const LocwavCurve& locwavCurvehuecont, bool locwavhueutilicont, float *resi, float &denocont, float *savmadl);

   // void recovm(const struct local_params & lp, int bfw, int bfh,  int xstart, int ystart, int sk, LabImage *bufmaskblurbl, LabImage *tmp1, LabImage *tmp3);
    void recovm(float highrec, float lowrec, float thrrec, bool invrec, int bfw, int bfh, int xstart, int ystart, float decay, int sk,  LabImage *bufmaskblurbl, LabImage *tmp1, LabImage *tmp3, bool multiThread);

    void fftw_denoise(int sk, int GW, int GH, int max_numblox_W, int min_numblox_W, float **tmp1, array2D<float> *Lin,  int numThreads, const struct local_params & lp, int chrom);

    void ColorLight_Local(float moddE, float powdE, int call, LabImage * bufcolorig, LabImage * originalmask, float **buflight, float **bufchro, float **bufchroslid, float ** bufhh, float ** buflightslid, bool &LHutili, bool &HHutili, const float hueplus, const float huemoins, const float hueref, const float dhue, const float chromaref, const float lumaref, float sobelref, float ** blend2, LUTf & lllocalcurve, const LocLHCurve & loclhCurve, const LocHHCurve & lochhCurve, LUTf & lightCurveloc, const local_params& lp, LabImage* original, LabImage* transformed, int cx, int cy, int sk);
    void InverseColorLight_Local(bool tonequ, bool tonecurv, int sp, int senstype, struct local_params& lp, LabImage * originalmask, const LUTf& lightCurveloc, const LUTf& hltonecurveloc, const LUTf& shtonecurveloc, const LUTf& tonecurveloc, const LUTf& exlocalcurve, const LUTf& cclocalcurve, float adjustr, bool localcutili, const LUTf& lllocalcurve, bool locallutili, LabImage* original, LabImage* transformed, int cx, int cy, const float hueref, const float chromaref, const float lumaref, int sk);
    void Sharp_Local(int call, float **loctemp,  int senstype, const float hueref,  const float chromaref, const float lumaref, local_params & lp, LabImage * original, LabImage * transformed, int cx, int cy, int sk);

    void InverseSharp_Local(float **loctemp, const float hueref, const float lumaref, const float chromaref, local_params& lp, LabImage* original, LabImage* transformed, int cx, int cy, int sk);

//Wavelet and denoise
    void Tile_calc(int tilesize, int overlap, int kall, int imwidth, int imheight, int &numtiles_W, int &numtiles_H, int &tilewidth, int &tileheight, int &tileWskip, int &tileHskip);
    void ip_wavelet(LabImage * lab, LabImage * dst, int kall, const procparams::WaveletParams & waparams, const WavCurve & wavCLVCcurve, const WavCurve & wavdenoise, WavCurve & wavdenoiseh, const Wavblcurve & wavblcurve, const WavOpacityCurveRG & waOpacityCurveRG, const WavOpacityCurveSH & waOpacityCurveSH, const WavOpacityCurveBY & waOpacityCurveBY,  const WavOpacityCurveW & waOpacityCurveW, const WavOpacityCurveWL & waOpacityCurveWL, const LUTf &wavclCurve, int skip);

    void WaveletcontAllL(LabImage * lab, float **varhue, float **varchrom, wavelet_decomposition& WaveletCoeffs_L, const Wavblcurve & wavblcurve,
            struct cont_params &cp, int skip, float *mean, float *sigma, float *MaxP, float *MaxN,  const WavCurve & wavCLVCcurve, const WavOpacityCurveW & waOpacityCurveW, const WavOpacityCurveSH & waOpacityCurveSH, FlatCurve* ChCurve, bool Chutili);
    void WaveletcontAllLfinal(wavelet_decomposition& WaveletCoeffs_L, const cont_params &cp, float *mean, float *sigma, float *MaxP, const WavOpacityCurveWL & waOpacityCurveWL);
    void WaveletcontAllAB(LabImage * lab, float **varhue, float **varchrom, wavelet_decomposition& WaveletCoeffs_a, const Wavblcurve & wavblcurve, const WavOpacityCurveW & waOpacityCurveW,
            struct cont_params &cp, const bool useChannelA, int skip, float *meanab, float *sigmaab);
    void WaveletAandBAllAB(wavelet_decomposition& WaveletCoeffs_a, wavelet_decomposition& WaveletCoeffs_b,
            const cont_params &cp, FlatCurve* hhcurve, bool hhutili);
    void ContAllL(float** koeLi, float maxkoeLi, bool lipschitz, int maxlvl, LabImage * lab, const float* const* varhue, const float* const* varchrom, float* const* WavCoeffs_L, float * WavCoeffs_L0, int level, int dir, struct cont_params &cp,
            int W_L, int H_L, int skip, float *mean, float *sigma, float *MaxP, float *MaxN,  const WavCurve & wavCLVCcurve, const WavOpacityCurveW & waOpacityCurveW, const WavOpacityCurveSH & waOpacityCurveSH, FlatCurve* ChCurve, bool Chutili);
    void finalContAllL(float* const* WavCoeffs_L, float * WavCoeffs_L0, int level, int dir, const cont_params &cp,
            int W_L, int H_L, float *mean, float *sigma, float *MaxP, const WavOpacityCurveWL & waOpacityCurveWL);
    void ContAllAB(LabImage * lab, int maxlvl, float **varhue, float **varchrom, float* const* WavCoeffs_a, float * WavCoeffs_a0, int level, int dir, const WavOpacityCurveW & waOpacityCurveW, struct cont_params &cp,
            int W_ab, int H_ab, const bool useChannelA, float *meanab, float *sigmaab);
    void Evaluate2(const wavelet_decomposition &WaveletCoeffs_L, float *mean, float *meanN, float *sigma, float *sigmaN, float *MaxP, float *MaxN, int numThreads);
    void Eval2(const float* const* WavCoeffs_L, int level, int W_L, int H_L, float *mean, float *meanN, float *sigma, float *sigmaN, float *MaxP, float *MaxN, int numThreads);
    void complete_local_contrast (LabImage * lab, LabImage * dst, const procparams::WaveletParams & waparams, const procparams::ColorManagementParams & cmparams, const WavOpacityCurveWL & cmOpacityCurveWL, int skip, int &level_hr, int &maxlevpo, bool &wavcurvecont);

    void calceffect(int level, float *mean, float *sigma, float *mea, float effect, float offs);
    std::unique_ptr<LUTf> buildMeaLut(const float inVals[11], const float mea[10], float& lutFactor);
    void Aver(const float* HH_Coeffs, int datalen, float &averagePlus, float &averageNeg, float &max, float &min, int numThreads);
    void Sigma(const float* HH_Coeffs, int datalen, float averagePlus, float averageNeg, float &sigmaPlus, float &sigmaNeg, int numThreads);
    void calckoe(const float* WavCoeffs_LL, float gradw, float tloww, float *koeLi, int level, int W_L, int H_L, float edd, float &maxkoeLi, float **tmC, bool multiThread = false);

    static void Median_Denoise(float **src, float **dst, int width, int height, Median medianType, int iterations, int numThreads, float **buffer = nullptr);
    void Median_Denoise(float **src, float **dst, float upperBound, int width, int height, Median medianType, int iterations, int numThreads, float **buffer = nullptr);
    void RGB_denoise(int kall, Imagefloat * src, Imagefloat * dst, Imagefloat * calclum, float * ch_M, float *max_r, float *max_b, bool isRAW, const procparams::DirPyrDenoiseParams & dnparams, const double expcomp, const NoiseCurve & noiseLCurve, const NoiseCurve & noiseCCurve, float &nresi, float &highresi);
    void RGB_denoise_infoGamCurve(const procparams::DirPyrDenoiseParams & dnparams, const bool isRAW, LUTf &gamcurve, float &gam, float &gamthresh, float &gamslope);
    void RGB_denoise_info(Imagefloat * src, Imagefloat * provicalc, bool isRAW, const LUTf &gamcurve, float gam, float gamthresh, float gamslope, const procparams::DirPyrDenoiseParams & dnparams, const double expcomp, float &chaut, int &Nb, float &redaut, float &blueaut, float &maxredaut, float & maxblueaut, float &minredaut, float & minblueaut, float &chromina, float &sigma, float &lumema, float &sigma_L, float &redyel, float &skinc, float &nsknc, bool multiThread = false);
    void RGBtile_denoise(float * fLblox, int hblproc, float noisevar_Ldetail);     //for DCT
    void RGBoutput_tile_row(float *bloxrow_L, float ** Ldetail, float ** tilemask_out, int height, int width, int top);

    void WaveletDenoiseAll_info(int levwav, const wavelet_decomposition &WaveletCoeffs_a,
                                const wavelet_decomposition &WaveletCoeffs_b, float **noisevarlum, float **noisevarchrom, float **noisevarhue, float &chaut, int &Nb, float &redaut, float &blueaut, float &maxredaut, float &maxblueaut, float &minredaut, float & minblueaut, int schoice, float &chromina, float &sigma, float &lumema, float &sigma_L, float &redyel, float &skinc, float &nsknc,
                                float &maxchred, float &maxchblue, float &minchred, float &minchblue, int &nb, float &chau, float &chred, float &chblue, bool denoiseMethodRgb);

    bool WaveletDenoiseAllL(wavelet_decomposition& WaveletCoeffs_L, float *noisevarlum, float madL[8][3], float * vari, int edge, int denoiseNestedLevels);
    bool WaveletDenoiseAllAB(wavelet_decomposition& WaveletCoeffs_L, wavelet_decomposition& WaveletCoeffs_ab, float *noisevarchrom, float madL[8][3], float *variC, int local, float noisevar_ab, const bool useNoiseCCurve, bool autoch, bool denoiseMethodRgb, int denoiseNestedLevels);

    bool WaveletDenoiseAll_BiShrinkL(wavelet_decomposition& WaveletCoeffs_L, float *noisevarlum, float madL[8][3], float * vari, int edge, int denoiseNestedLevels);
    bool WaveletDenoiseAll_BiShrinkAB(wavelet_decomposition& WaveletCoeffs_L, wavelet_decomposition& WaveletCoeffs_ab, float *noisevarchrom, float madL[8][3], float *variC, int local, float noisevar_ab, const bool useNoiseCCurve,  bool autoch, bool denoiseMethodRgb, int denoiseNestedLevels);

    static void ShrinkAllL(wavelet_decomposition& WaveletCoeffs_L, float **buffer, int level, int dir, float *noisevarlum, float * madL, float * vari, int edge);
    void ShrinkAllAB(wavelet_decomposition& WaveletCoeffs_L, wavelet_decomposition& WaveletCoeffs_ab, float **buffer, int level, int dir,
                     float *noisevarchrom, float noisevar_ab, const bool useNoiseCCurve, bool autoch, bool denoiseMethodRgb, float * madL, float * variC, int local, float * madaab = nullptr, bool madCalculated = false);


    void ShrinkAll_info(const float* const* WavCoeffs_a, const float* const* WavCoeffs_b,
                        int W_ab, int H_ab, float **noisevarlum, float **noisevarchrom, float **noisevarhue, float &chaut, int &Nb, float &redaut, float &blueaut, float &maxredaut, float &maxblueaut, float &minredaut, float &minblueaut, int schoice, int lvl, float &chromina, float &sigma, float &lumema, float &sigma_L, float &redyel, float &skinc, float &nsknc,
                        float &maxchred, float &maxchblue, float &minchred, float &minchblue, int &nb, float &chau, float &chred, float &chblue, bool denoiseMethodRgb);
    void Noise_residualAB(const wavelet_decomposition &WaveletCoeffs_ab, float &chresid, float &chmaxresid, bool denoiseMethodRgb, int beg, int end);
    void calcautodn_info(float &chaut, float &delta, int Nb, int levaut, float maxmax, float lumema, float chromina, int mode, int lissage, float redyel, float skinc, float nsknc);
    float Mad(const float * DataList, int datalen);
    static float MadRgb(const float * DataList, int datalen);

    // spot removal tool
    void removeSpots (rtengine::Imagefloat* img, rtengine::ImageSource* imgsrc, const std::vector<procparams::SpotEntry> &entries, const PreviewProps &pp, const rtengine::ColorTemp &currWB, const procparams::ColorManagementParams *cmp, int tr);

    // pyramid wavelet
    void cbdl_local_temp(float ** src, float ** loctemp, int srcwidth, int srcheight, const float * mult, float kchro, const double dirpyrThreshold, const float mergeL, const float contres, const double skinprot, const bool gamutlab, float b_l, float t_l, float t_r, float b_r,  int choice, int scale, bool multiThread);
    void dirpyr_equalizer(const float * const * src, float ** dst, int srcwidth, int srcheight, const float * const * l_a, const float * const * l_b, const double * mult, double dirpyrThreshold, double skinprot, float b_l, float t_l, float t_r, int scale);    //Emil's directional pyramid wavelet
    void dirpyr_equalizercam(const CieImage* ncie, float ** src, float ** dst, int srcwidth, int srcheight, const float * const * h_p, const float * const * C_p,  const double * mult, const double dirpyrThreshold, const double skinprot, float b_l, float t_l, float t_r, int scale);    //Emil's directional pyramid wavelet
    void defringe(LabImage* lab);
    void defringecam(CieImage* ncie);
    void badpixcam(CieImage* ncie, double rad, int thr, int mode, float chrom, bool hotbad);
    void badpixlab(LabImage* lab, double rad, int thr, float chrom);

    void PF_correct_RT(LabImage * lab, double radius, int thresh);
    void PF_correct_RTcam(CieImage * ncie, double radius, int thresh);
    void Badpixelscam(CieImage * ncie, double radius, int thresh, int mode, float chrom, bool hotbad);
    void BadpixelsLab(LabImage * lab, double radius, int thresh, float chrom);

    void dehaze(Imagefloat *rgb, const procparams::DehazeParams &dehazeParams);
    void dehazeloc(Imagefloat *rgb, const procparams::DehazeParams &dehazeParams, int sk, int sp);
    void ToneMapFattal02(Imagefloat *rgb, const procparams::FattalToneMappingParams &fatParams, int detail_level, int Lalone, float **Lum, int WW, int HH, int algo, bool sat);
    void localContrast(LabImage *lab, float **destination, const procparams::LocalContrastParams &localContrastParams, bool fftwlc, double scale);
    void colorToningLabGrid(LabImage *lab, int xstart, int xend, int ystart, int yend, bool MultiThread);
    //void shadowsHighlights(LabImage *lab);
    void shadowsHighlights(LabImage *lab, bool ena, int labmode, int hightli, int shado, int rad, int scal, int hltonal, int shtonal);
    void toneEqualizer(Imagefloat *rgb);
    void toneEqualizer(Imagefloat *rgb, const procparams::ToneEqualizerParams &params, const Glib::ustring &workingProfile, double scale, bool multiThread);
    void softLight(LabImage *lab, const procparams::SoftLightParams &softLightParams);
    void labColorCorrectionRegions(LabImage *lab);

    Image8*     lab2rgb(LabImage* lab, int cx, int cy, int cw, int ch, const procparams::ColorManagementParams &icm, bool consider_histogram_settings = true);
    void rgb2lab(const Image8 &src, int x, int y, int w, int h, float L[], float a[], float b[], const procparams::ColorManagementParams &icm, bool consider_histogram_settings = true) const;
    static void rgb2lab(std::uint8_t red, std::uint8_t green, std::uint8_t blue, float &L, float &a, float &b, const procparams::ColorManagementParams &icm, bool consider_histogram_settings = true);
    Imagefloat*    lab2rgbOut(LabImage* lab, int cx, int cy, int cw, int ch, const procparams::ColorManagementParams &icm);
    // CieImage *ciec;
    
    void gamutcompr( rtengine::Imagefloat *src, rtengine::Imagefloat *dst, int beginend, float &mac, float &mac0, float &mac1, float &mac2) const;
    void apsatur(int sp, Imagefloat* src, Imagefloat* dst, int bfw, int bfh, float satu);

    void workingtrc(int sp, Imagefloat* src, Imagefloat* dst, int cw, int ch , int mul, Glib::ustring &profile, double gampos, double slpos, int cat, int &illum, int prim, int locprim, 
        float &rdx, float &rdy, float &grx, float &gry, float &blx, float &bly, float &meanx, float &meany, float &meanxe, float &meanye, float &maxdat, double *p,
        cmsHTRANSFORM &transform, bool normalizeIn = true, bool normalizeOut = true, bool keepTransForm = false, bool gamutcontrol = false) const;
    void preserv(LabImage *nprevl, LabImage *provis, int cw, int ch);

    bool transCoord(int W, int H, int x, int y, int w, int h, int& xv, int& yv, int& wv, int& hv, double ascaleDef = -1, const LensCorrection *pLCPMap = nullptr) const;
    bool transCoord(int W, int H, const std::vector<Coord2D> &src, std::vector<Coord2D> &red,  std::vector<Coord2D> &green, std::vector<Coord2D> &blue, double ascaleDef = -1, const LensCorrection *pLCPMap = nullptr) const;
    static void getAutoExp(const LUTu & histogram, int histcompr, double clip, double& expcomp, int& bright, int& contr, int& black, int& hlcompr, int& hlcomprthresh);
    static double getAutoDistor(const Glib::ustring& fname, int thumb_size);
    double getTransformAutoFill(int oW, int oH, const LensCorrection *pLCPMap = nullptr) const;
    void rgb2lab(const Imagefloat &src, LabImage &dst, const Glib::ustring &workingSpace);
    void lab2rgb(const LabImage &src, Imagefloat &dst, const Glib::ustring &workingSpace);
};

}
