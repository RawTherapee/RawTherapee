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

#include <array>
#include <bitset>
#include <map>
#include <vector>

#include <glibmm/ustring.h>
#include <lcms2.h>

#include "params/advanced.h"
#include "params/locallab.h"
#include "params/raw.h"
#include "params/threshold.h"

#include "coord.h"
#include "noncopyable.h"

struct ParamsEdited;

namespace rtengine
{

class ColorGradientCurve;
class NoiseCurve;
class OpacityCurve;
class LocretigainCurve;
class LocretigainCurverab;
class LocLHCurve;
class LocHHCurve;
class LocCHCurve;
class LocLLmaskCurve;
class LocCCmaskCurve;
class LocHHmaskCurve;
class LocLLmaskexpCurve;
class LocCCmaskexpCurve;
class LocHHmaskexpCurve;

enum class StandardObserver;

enum RenderingIntent : int {
    RI_PERCEPTUAL = INTENT_PERCEPTUAL,
    RI_RELATIVE = INTENT_RELATIVE_COLORIMETRIC,
    RI_SATURATION = INTENT_SATURATION,
    RI_ABSOLUTE = INTENT_ABSOLUTE_COLORIMETRIC,
    RI__COUNT
};

namespace procparams
{

enum class ToneCurveMode : int {
    STD,               // Standard modes, the curve is applied on all component individually
    WEIGHTEDSTD,       // Weighted standard mode
    FILMLIKE,          // Film-like mode, as defined in Adobe's reference code
    SATANDVALBLENDING, // Modify the Saturation and Value channel
    LUMINANCE,         // Modify the Luminance channel with coefficients from Rec 709's
    PERCEPTUAL         // Keep color appearance constant using perceptual modeling
};

/**
  * Parameters of the tone curve
  */
struct ToneCurveParams {
    bool autoexp;
    double clip;
    bool hrenabled; // Highlight Reconstruction enabled
    Glib::ustring method; // Highlight Reconstruction's method
    double expcomp;
    std::vector<double> curve;
    std::vector<double> curve2;
    ToneCurveMode curveMode;
    ToneCurveMode curveMode2;
    int brightness;
    int black;
    int contrast;
    int saturation;
    int shcompr;
    int hlcompr; // Highlight Recovery's compression
    int hlbl; // Highlight Recovery's compression
    double hlth; // Highlight Recovery's threshold
    int hlcomprthresh; // Highlight Recovery's threshold
    bool histmatching; // histogram matching
    bool fromHistMatching;
    bool clampOOG; // clamp out of gamut colours

    ToneCurveParams();

    bool isPanningRelatedChange(const ToneCurveParams& other) const;
    bool operator ==(const ToneCurveParams& other) const;
    bool operator !=(const ToneCurveParams& other) const;

};

/**
  * Parameters of the luminance curve
  */
struct LCurveParams {
    bool enabled;
    std::vector<double>   lcurve;
    std::vector<double>   acurve;
    std::vector<double>   bcurve;
    std::vector<double>   cccurve;
    std::vector<double>   chcurve;
    std::vector<double>   lhcurve;
    std::vector<double>   hhcurve;
    std::vector<double>   lccurve;
    std::vector<double>   clcurve;
    int     brightness;
    int     contrast;
    int     chromaticity;
    Glib::ustring gamutmunselmethod;
    double  rstprotection;
    bool    lcredsk;

    LCurveParams();

    bool operator ==(const LCurveParams& other) const;
    bool operator !=(const LCurveParams& other) const;
};


/**
 * Parameters for local contrast
 */
struct LocalContrastParams {
    bool enabled;
    int radius;
    double amount;
    double darkness;
    double lightness;

    LocalContrastParams();

    bool operator==(const LocalContrastParams &other) const;
    bool operator!=(const LocalContrastParams &other) const;
};


/**
  * Parameters of the RGB curves
  */
struct RGBCurvesParams {
    bool enabled;
    bool lumamode;
    std::vector<double>   rcurve;
    std::vector<double>   gcurve;
    std::vector<double>   bcurve;

    RGBCurvesParams();

    bool operator ==(const RGBCurvesParams& other) const;
    bool operator !=(const RGBCurvesParams& other) const;
};

/**
  * Parameters of the Color Toning
  */
struct ColorToningParams {
    bool enabled;
    bool autosat;
    std::vector<double> opacityCurve;
    std::vector<double> colorCurve;
    int satProtectionThreshold;
    int saturatedOpacity;
    int strength;
    int balance;
    Threshold<int> hlColSat;
    Threshold<int> shadowsColSat;
    std::vector<double> clcurve;
    std::vector<double> cl2curve;

    /* Can be either:
     *  Splitlr    :
     *  Splitco    :
     *  Splitbal   :
     *  Lab        :
     *  Lch        :
     *  RGBSliders :
     *  RGBCurves  :
     *  LabGrid    :
     */
    Glib::ustring method;

    /* Can be either:
     * Std   :
     * All   :
     * Separ :
     * Two   :
     */
    Glib::ustring twocolor;
    double redlow;
    double greenlow;
    double bluelow;
    double redmed;
    double greenmed;
    double bluemed;
    double redhigh;
    double greenhigh;
    double bluehigh;
    double satlow;
    double sathigh;
    bool lumamode;

    double labgridALow;
    double labgridBLow;
    double labgridAHigh;
    double labgridBHigh;
    static const double LABGRID_CORR_MAX;
    static const double LABGRID_CORR_SCALE;

    struct LabCorrectionRegion {
        enum { CHAN_ALL = -1, CHAN_R, CHAN_G, CHAN_B };
        double a;
        double b;
        double saturation;
        double slope;
        double offset;
        double power;
        std::vector<double> hueMask;
        std::vector<double> chromaticityMask;
        std::vector<double> lightnessMask;
        double maskBlur;
        int channel;

        LabCorrectionRegion();
        bool operator==(const LabCorrectionRegion &other) const;
        bool operator!=(const LabCorrectionRegion &other) const;
    };
    std::vector<LabCorrectionRegion> labregions;
    int labregionsShowMask;

    ColorToningParams();

    bool operator ==(const ColorToningParams& other) const;
    bool operator !=(const ColorToningParams& other) const;

    /// @brief Transform the mixer values to their curve equivalences
    void mixerToCurve(std::vector<double>& colorCurve, std::vector<double>& opacityCurve) const;
    /// @brief Specifically transform the sliders values to their curve equivalences
    void slidersToCurve(std::vector<double>& colorCurve, std::vector<double>& opacityCurve) const;
    /// @brief Fill the ColorGradientCurve and OpacityCurve LUTf from the control points curve or sliders value
    void getCurves(ColorGradientCurve& colorCurveLUT, OpacityCurve& opacityCurveLUT, const double xyz_rgb[3][3], bool& opautili) const;
};


/**
  * Parameters of the sharpening
  */
struct SharpeningParams {
    bool           enabled;
    double         contrast;
    bool           autoContrast;
    double         blurradius;
    double         gamma;
    double         radius;
    int            amount;
    Threshold<int> threshold;
    bool           edgesonly;
    double         edges_radius;
    int            edges_tolerance;
    bool           halocontrol;
    int            halocontrol_amount;
    Glib::ustring  method;
    int            deconvamount;
    double         deconvradius;
    int            deconviter;
    int            deconvdamping;
    bool deconvAutoRadius;
    double deconvCornerBoost;
    int deconvCornerLatitude;
    Glib::ustring psf_kernel;
    double psf_iterations;

    SharpeningParams();

    bool operator ==(const SharpeningParams& other) const;
    bool operator !=(const SharpeningParams& other) const;
};

struct SharpenEdgeParams {
    bool    enabled;
    int     passes;
    double  amount;
    bool    threechannels;

    SharpenEdgeParams();

    bool operator ==(const SharpenEdgeParams& other) const;
    bool operator !=(const SharpenEdgeParams& other) const;

};


struct SharpenMicroParams {
    bool    enabled;
    bool    matrix;
    double  amount;
    double  contrast;
    int     uniformity;

    SharpenMicroParams();

    bool operator ==(const SharpenMicroParams& other) const;
    bool operator !=(const SharpenMicroParams& other) const;
};

/**
  * Parameters of the vibrance
  */
struct VibranceParams {
    bool           enabled;
    int            pastels;
    int            saturated;
    Threshold<int> psthreshold;
    bool           protectskins;
    bool           avoidcolorshift;
    bool           pastsattog;
    std::vector<double> skintonescurve;

    VibranceParams();

    bool operator ==(const VibranceParams& other) const;
    bool operator !=(const VibranceParams& other) const;
};

/**
  * Parameters of the white balance adjustments
  */
struct WBEntry {
    enum class Type {
        CAMERA,
        AUTO,
        DAYLIGHT,
        CLOUDY,
        SHADE,
        WATER,
        TUNGSTEN,
        FLUORESCENT,
        LAMP,
        FLASH,
        LED,
        // CUSTOM one must remain the last one!
        CUSTOM
    };

    Glib::ustring ppLabel;
    Type type;
    Glib::ustring GUILabel;
    int temperature;
    double green;
    double equal;
    double tempBias;
};

struct WBParams {
    static constexpr int CURRENT_COMPAT_VERSION = 2;

    bool enabled;
    Glib::ustring    method;
    int              temperature;
    double           green;
    double           equal;
    double           tempBias;
    StandardObserver observer;
    double           itcwb_green;
    int              itcwb_rgreen;
    bool             itcwb_nopurple;
    bool             itcwb_alg;
    Glib::ustring    itcwb_prim;
    bool             itcwb_sampling;
    /**
     * Used to maintain edits from previous versions of RawTherapee where
     * compatibility cannot be maintained simply by converting the parameters,
     * for example, when the output depends on the file type.
     *
     * Version 0:
     *  - Base version.
     * Version 1 (5.9):
     *  - RGB Gray fixed to (1, 1, 1) RGB multipliers before temperature bias
     *    for non-raw files.
     *  - Temperature correlation fixed to temperature 5000, green 1, and equal
     *    1 or equivalent for non-raw files.
     * Version 2 (5.10):
     *  - RGB grey restored to version 0.
     *  - Temperature correlation equivalent to method "Camera" for non-raw
     *    files.
     */
    int compat_version;

    WBParams();

    bool isPanningRelatedChange(const WBParams& other) const;
    bool operator ==(const WBParams& other) const;
    bool operator !=(const WBParams& other) const;

    static const std::vector<WBEntry>& getWbEntries();
};

/**
 * Parameters of defringing
 */
struct DefringeParams {
    bool    enabled;
    double  radius;
    int     threshold;
    std::vector<double> huecurve;

    DefringeParams();

    bool operator ==(const DefringeParams& other) const;
    bool operator !=(const DefringeParams& other) const;
};

/**
  * Parameters of impulse denoising
  */
struct ImpulseDenoiseParams {
    bool    enabled;
    int     thresh;

    ImpulseDenoiseParams();

    bool operator ==(const ImpulseDenoiseParams& other) const;
    bool operator !=(const ImpulseDenoiseParams& other) const;
};

/**
 * Parameters of the directional pyramid denoising
 */
struct DirPyrDenoiseParams {
    std::vector<double>   lcurve;
    std::vector<double>   cccurve;

    bool    enabled;
    bool    enhance;
    bool    median;

    bool    perform;
    double  luma;
    double  Ldetail;
    double  chroma;
    double  redchro;
    double  bluechro;
    bool autoGain;
    double  gamma;
    Glib::ustring dmethod;
    Glib::ustring Lmethod;
    Glib::ustring Cmethod;
    Glib::ustring C2method;
    Glib::ustring smethod;
    Glib::ustring medmethod;
    Glib::ustring methodmed;
    Glib::ustring rgbmethod;
    int  passes;

    DirPyrDenoiseParams();

    bool operator ==(const DirPyrDenoiseParams& other) const;
    bool operator !=(const DirPyrDenoiseParams& other) const;

    void getCurves(NoiseCurve& lCurve, NoiseCurve& cCurve) const;
};

// EPD related parameters.
struct EPDParams {
    bool   enabled;
    double strength;
    double gamma;
    double edgeStopping;
    double scale;
    int    reweightingIterates;

    EPDParams();

    bool operator ==(const EPDParams& other) const;
    bool operator !=(const EPDParams& other) const;
};

// Fattal02 Tone-Mapping parameters
struct FattalToneMappingParams {
    bool enabled;
    int threshold;
    int amount;
    int anchor;

    FattalToneMappingParams();

    bool operator ==(const FattalToneMappingParams& other) const;
    bool operator !=(const FattalToneMappingParams& other) const;
};

/**
  * Parameters of the shadow/highlight enhancement
  */
struct SHParams {
    bool    enabled;
    int     highlights;
    int     htonalwidth;
    int     shadows;
    int     stonalwidth;
    int     radius;
    bool    lab;

    SHParams();

    bool operator ==(const SHParams& other) const;
    bool operator !=(const SHParams& other) const;
};

/**
  * Parameters of the compression gamut
  */
struct CGParams {
    bool    enabled;
    double  th_c;
    double  th_m;
    double  th_y;
    double  d_c;
    bool autodc;
    double  d_m;
    bool autodm;
    double  d_y;
    bool autody;
    double  pwr;
    Glib::ustring colorspace;
    bool rolloff;

    CGParams();

    bool operator ==(const CGParams& other) const;
    bool operator !=(const CGParams& other) const;
};


/**
 * Tone equalizer parameters.
 */
struct ToneEqualizerParams {
    bool enabled;
    std::array<int, 6> bands;
    int regularization;
    bool show_colormap;
    double pivot;

    ToneEqualizerParams();

    bool operator ==(const ToneEqualizerParams &other) const;
    bool operator !=(const ToneEqualizerParams &other) const;
};

/**
  * Parameters of the cropping
  */
struct CropParams {
    bool enabled;
    int x;
    int y;
    int w;
    int h;
    bool fixratio;
    Glib::ustring ratio;
    Glib::ustring orientation;

    CropParams();

    bool operator ==(const CropParams& other) const;
    bool operator !=(const CropParams& other) const;

    void mapToResized(int resizedWidth, int resizedHeight, int scale, int& x1, int& x2, int& y1, int& y2) const;
};

struct CropGuideParams {
    // If values are added/removed, make sure to update NUM_PRESETS and
    // CropGuideParamsEdited.presets
    enum PresetIndex : size_t {
        RULE_OF_THIRDS = 0,
        RULE_OF_DIAGONALS,
        HARMONIC_MEANS,
        CROSSHAIR,
        GRID,
        GOLDEN_TRIANGLE,
        GOLDEN_RATIO,
        EPASSPORT,
        CENTERED_SQUARE
    };
    static constexpr size_t NUM_PRESETS = 9;
    static_assert(NUM_PRESETS == PresetIndex::CENTERED_SQUARE + 1);

    enum class Basis { SCALE, WIDTH, HEIGHT, LONG, SHORT };

    struct PresetParams {
        bool enabled;
        double red;
        double green;
        double blue;
        double alpha;

        PresetParams();

        bool operator==(const PresetParams& other) const;
        bool operator!=(const PresetParams& other) const {
            return !(*this == other);
        }
    };

    struct AspectRatioParams {
        bool enabled;
        bool is_portrait;
        size_t preset_index;
        double red;
        double green;
        double blue;
        double alpha;

        AspectRatioParams(size_t preset_index);

        bool operator==(const AspectRatioParams& other) const;
        bool operator!=(const AspectRatioParams& other) const {
            return !(*this == other);
        }
    };

    std::vector<AspectRatioParams> aspect_ratios;
    std::array<PresetParams, NUM_PRESETS> presets;
    bool enabled;
    bool mirror_golden_triangle;
    bool rotate_golden_ratio;
    bool mirror_golden_ratio;
    int bleed;
    Basis basis;

    CropGuideParams();

    bool operator==(const CropGuideParams& other) const;
    bool operator!=(const CropGuideParams& other) const { return !(*this == other); }
};

/**
  * Parameters of the coarse transformations like 90 deg rotations and h/v flipping
  */
struct CoarseTransformParams {
    int     rotate;
    bool    hflip;
    bool    vflip;

    CoarseTransformParams();

    bool operator ==(const CoarseTransformParams& other) const;
    bool operator !=(const CoarseTransformParams& other) const;
};

/**
  * Common transformation parameters
  */
struct CommonTransformParams {
    Glib::ustring method = "log";
    bool autofill = true;
    double scale = 1.0;
    double scale_horizontally = 1.0;
    double scale_vertically = 1.0;

    CommonTransformParams();

    double getScale() const;
    double getScaleHorizontally() const;
    double getScaleVertically() const;

    bool operator ==(const CommonTransformParams& other) const;
    bool operator !=(const CommonTransformParams& other) const;
};

/**
  * Parameters of the rotation
  */
struct RotateParams {
    double  degree;

    RotateParams();

    bool operator ==(const RotateParams& other) const;
    bool operator !=(const RotateParams& other) const;
};

/**
  * Parameters of the distortion correction
  */
struct DistortionParams {
    static constexpr double DEFAULT_FOCAL_LENGTH = 12;
    double amount = 0.0;
    bool defish = false;
    double focal_length = DEFAULT_FOCAL_LENGTH;

    DistortionParams();

    bool operator ==(const DistortionParams& other) const;
    bool operator !=(const DistortionParams& other) const;
};

// Lens profile correction parameters
struct LensProfParams {
    enum class LcMode {
        NONE,               // No lens correction
        LENSFUNAUTOMATCH,   // Lens correction using auto matched lensfun database entry
        LENSFUNMANUAL,      // Lens correction using manually selected lensfun database entry
        LCP,                // Lens correction using lcp file
        METADATA    // Lens correction using embedded metadata
    };

    LcMode lcMode;
    Glib::ustring lcpFile;
    bool useDist, useVign, useCA;
    Glib::ustring lfCameraMake;
    Glib::ustring lfCameraModel;
    Glib::ustring lfLens;

    LensProfParams();

    bool operator ==(const LensProfParams& other) const;
    bool operator !=(const LensProfParams& other) const;

    bool useLensfun() const;
    bool lfAutoMatch() const;
    bool useLcp() const;
    bool lfManual() const;
    bool useMetadata() const;

    const std::vector<const char*>& getMethodStrings() const;
    Glib::ustring getMethodString(LcMode mode) const;
    LcMode getMethodNumber(const Glib::ustring& mode) const;
};


/**
  * Parameters of the perspective correction
  */
struct PerspectiveParams {
    static constexpr double DEFAULT_CAMERA_CROP_FACTOR = 1;
    static constexpr double DEFAULT_CAMERA_FOCAL_LENGTH = 24;

    Glib::ustring method;
    bool    render;
    double  horizontal;
    double  vertical;
    /**
     * Negative and zero values indicate an unspecified crop factor and should
     * be interpreted with {@link #DEFAULT_CAMERA_CROP_FACTOR}.
     */
    double  camera_crop_factor;
    /**
     * Negative and zero values indicate an unspecified focal length and should
     * be interpreted with {@link #DEFAULT_CAMERA_FOCAL_LENGTH}.
     */
    double  camera_focal_length;
    double  camera_pitch;
    double  camera_roll;
    double  camera_shift_horiz;
    double  camera_shift_vert;
    double  camera_yaw;
    double  projection_pitch;
    double  projection_rotate;
    double  projection_shift_horiz;
    double  projection_shift_vert;
    double  projection_yaw;
    /** A line is stored as 4 integers in this order: x1, y1, x2, y2 */
    std::vector<int> control_line_values;
    /** 0 is vertical, 1 is horizontal, undefined otherwise. */
    std::vector<int> control_line_types;

    PerspectiveParams();

    bool operator ==(const PerspectiveParams& other) const;
    bool operator !=(const PerspectiveParams& other) const;
};

/**
  * Parameters of the gradient filter
  */
struct GradientParams {
    bool   enabled;
    double degree;
    int    feather;
    double strength;
    int    centerX;
    int    centerY;

    GradientParams();

    bool operator ==(const GradientParams& other) const;
    bool operator !=(const GradientParams& other) const;
};

/**
  * Parameters of the post-crop vignette filter
  */
struct PCVignetteParams {
    bool   enabled;
    double strength;
    int    feather;
    int    roundness;

    PCVignetteParams();

    bool operator ==(const PCVignetteParams& other) const;
    bool operator !=(const PCVignetteParams& other) const;
};

/**
  * Parameters of the vignetting correction
  */
struct VignettingParams {
    int  amount;
    int  radius;
    int  strength;
    int  centerX;
    int  centerY;

    VignettingParams();

    bool operator ==(const VignettingParams& other) const;
    bool operator !=(const VignettingParams& other) const;
};

/**
  * Parameters of the color mixer
  */
struct ChannelMixerParams {
    bool enabled;
    int red[3];
    int green[3];
    int blue[3];

    ChannelMixerParams();

    bool operator ==(const ChannelMixerParams& other) const;
    bool operator !=(const ChannelMixerParams& other) const;
};

struct BlackWhiteParams {
    enum class TcMode {
        STD_BW,               // Standard modes, the curve is applied on all component individually
        WEIGHTEDSTD_BW,       // Weighted standard mode
        FILMLIKE_BW,          // Film-like mode, as defined in Adobe's reference code
        SATANDVALBLENDING_BW  // Modify the Saturation and Value channel
    };

    std::vector<double> beforeCurve;
    TcMode beforeCurveMode;
    std::vector<double> afterCurve;
    TcMode afterCurveMode;
    Glib::ustring algo;

    std::vector<double> luminanceCurve;
    bool autoc;
    bool enabledcc;
    bool enabled;
    Glib::ustring filter;
    Glib::ustring setting;
    Glib::ustring method;
    int mixerRed;
    int mixerOrange;
    int mixerYellow;
    int mixerGreen;
    int mixerCyan;
    int mixerBlue;
    int mixerMagenta;
    int mixerPurple;
    int gammaRed;
    int gammaGreen;
    int gammaBlue;

    BlackWhiteParams();

    bool operator ==(const BlackWhiteParams& other) const;
    bool operator !=(const BlackWhiteParams& other) const;
};

/**
  * Parameters of the c/a correction
  */
struct CACorrParams {
    double red;
    double blue;

    CACorrParams();

    bool operator ==(const CACorrParams& other) const;
    bool operator !=(const CACorrParams& other) const;
};

/**
  * Parameters of the resizing
  */
struct ResizeParams {
    bool enabled;
    double scale;
    Glib::ustring appliesTo;
    Glib::ustring method;
    int dataspec;
    int width;
    int height;
    int longedge;
    int shortedge;
    bool allowUpscaling;

    ResizeParams();

    bool operator ==(const ResizeParams& other) const;
    bool operator !=(const ResizeParams& other) const;
};

struct FramingParams {
    // How is framed size determined?
    enum class FramingMethod {
        STANDARD,   // Unconstrained framed size
        BBOX,       // Framed size within bounding box
        FIXED_SIZE  // Fixed framed size
    };

    // Orientation of framed image
    enum class Orientation {
        AS_IMAGE,
        LANDSCAPE,
        PORTRAIT
    };

    // How to size border?
    enum class BorderSizing {
        PERCENTAGE,          // Percentage of image size
        UNIFORM_PERCENTAGE,  // Percentage of image size (ignore aspect ratio)
        FIXED_SIZE           // Fixed pixel dimensions
    };

    // Which dimension to use for percentage based border sizing?
    enum class Basis {
        AUTO,    // Determine by aspect ratio of image and frame
        WIDTH,
        HEIGHT,
        LONG,    // Use long side of image
        SHORT    // Use short side of image
    };

    // Indicates to use the image aspect ratio for border
    static constexpr double AS_IMAGE_ASPECT_RATIO = 0.0;

    FramingParams();

    bool enabled;

    FramingMethod framingMethod;
    double aspectRatio;
    Orientation orientation;
    int framedWidth;
    int framedHeight;
    bool allowUpscaling;

    BorderSizing borderSizingMethod;
    Basis basis;
    double relativeBorderSize;
    bool minSizeEnabled;
    int minWidth;
    int minHeight;
    int absWidth;
    int absHeight;

    int borderRed;
    int borderGreen;
    int borderBlue;

    bool operator ==(const FramingParams& other) const;
    bool operator !=(const FramingParams& other) const;
};

/**
  * Parameters entry
  */
struct SpotEntry {
    Coord sourcePos;
    Coord targetPos;
    int radius;
    float feather;
    float opacity;

    SpotEntry();
    float getFeatherRadius() const;

    bool operator ==(const SpotEntry& other) const;
    bool operator !=(const SpotEntry& other) const;
};

/**
  * Parameters of the dust removal tool
  */
struct SpotParams {
    bool enabled;
    std::vector<SpotEntry> entries;

    // the following constant can be used for experimentation before the final merge
    static const short minRadius;
    static const short maxRadius;

    SpotParams();

    bool operator ==(const SpotParams& other) const;
    bool operator !=(const SpotParams& other) const;
};


/**
  * Parameters of the color spaces used during the processing
  */
struct ColorManagementParams {
    enum class WorkingTrc {
        NONE,
        CUSTOM,
        BT709,
        SRGB,
        GAMMA_2_2,
        GAMMA_1_8,
        LINEAR
    };

    enum class Wwgamut {
        NONE,
        REC2020,
        ADOBE,
        SRGB,
        DCIP3
    };

    enum class Illuminant {
        DEFAULT,
        D41,
        D50,
        D55,
        D60,
        D65,
        D80,
        D120,
        STDA,
        TUNGSTEN_2000K,
        TUNGSTEN_1500K,
        E
    };

    enum class Primaries {
        DEFAULT,
        SRGB,
        ADOBE_RGB,
        PRO_PHOTO,
        REC2020,
        ACES_P1,
        WIDE_GAMUT,
        ACES_P0,
        JDC_MAX,
        JDC_MAXSTDA,
        BRUCE_RGB,
        BETA_RGB,
        BEST_RGB,
        CUSTOM,
        CUSTOM_GRID,
        CUSTOM_POL
    };

    enum class Cat {
        BRAD,
        CAT16,
        CAT02,
        CAT_VK,
        CAT_XYZ
    };

    Glib::ustring inputProfile;
    bool toneCurve;
    bool applyLookTable;
    bool applyBaselineExposureOffset;
    bool applyHueSatMap;
    int dcpIlluminant;

    Glib::ustring workingProfile;
    WorkingTrc workingTRC;
    Wwgamut wgamut;
    Illuminant will;
    Primaries wprim;
    Cat wcat;
    double wGamma;
    double wSlope;
    double wapsat;
    double wmidtcie;
    double sigmatrc;
    double offstrc;
    double residtrc;
    double wgampower;
    double wgamgain;
    int pyrwavtrc;
    std::vector<double> opacityCurveWLI;   
    bool wsmoothcie;
    double wsmoothciesli;
    double redx;
    double redy;
    double grex;
    double grey;
    double blux;
    double bluy;
    
    double redrot;
    double redsat;
    double grerot;
    double gresat;
    double blurot;
    double blusat;
    
    double refi;
    double shiftx;
    double shifty;
    double preser;
    bool fbw;
    bool trcExp;
    bool wavExp;
    bool gamut;
    double labgridcieALow;
    double labgridcieBLow;
    double labgridcieAHigh;
    double labgridcieBHigh;
    double labgridcieGx;
    double labgridcieGy;
    double labgridcieWx;
    double labgridcieWy;
    double labgridcieMx;
    double labgridcieMy;
    RenderingIntent aRendIntent;

    Glib::ustring outputProfile;
    RenderingIntent outputIntent;
    bool outputBPC;

    static const Glib::ustring NoICMString;
    static const Glib::ustring NoProfileString;

    ColorManagementParams();

    bool operator ==(const ColorManagementParams& other) const;
    bool operator !=(const ColorManagementParams& other) const;
    
    void getCurves(
    WavOpacityCurveWL& opacityCurveLUTWLI
    ) const;

};

/**
  * Minimal wrapper allowing forward declaration for representing a key/value for the exif metadata information
  */
class ExifPairs final
{
private:
    using Pairs = std::map<Glib::ustring, Glib::ustring>;

public:
    using const_iterator = Pairs::const_iterator;
    using size_type = Pairs::size_type;

    const_iterator find(const Glib::ustring& key) const
    {
        return pairs.find(key);
    }

    const_iterator begin() const
    {
        return pairs.begin();
    }

    const_iterator end() const
    {
        return pairs.end();
    }

    void clear()
    {
        pairs.clear();
    }

    size_type erase(const Glib::ustring& key)
    {
        return pairs.erase(key);
    }

    bool empty() const
    {
        return pairs.empty();
    }

    Glib::ustring& operator[](const Glib::ustring& key)
    {
        return pairs[key];
    }

    bool operator ==(const ExifPairs& other) const
    {
        return pairs == other.pairs;
    }

private:
    Pairs pairs;
};

/**
  * The IPTC key/value pairs
  */
class IPTCPairs final
{
public:
    using iterator = std::map<Glib::ustring, std::vector<Glib::ustring>>::iterator;
    using const_iterator = std::map<Glib::ustring, std::vector<Glib::ustring>>::const_iterator;

    iterator find(const Glib::ustring& key)
    {
        return pairs.find(key);
    }

    const_iterator begin() const
    {
        return pairs.begin();
    }

    const_iterator end() const
    {
        return pairs.end();
    }

    bool empty() const
    {
        return pairs.empty();
    }

    iterator erase(const const_iterator& key)
    {
        return pairs.erase(key);
    }

    void clear()
    {
        pairs.clear();
    }

    std::vector<Glib::ustring>& operator[](const Glib::ustring& key)
    {
        return pairs[key];
    }

    bool operator ==(const IPTCPairs& other) const
    {
        return pairs == other.pairs;
    }

private:
    std::map<Glib::ustring, std::vector<Glib::ustring>> pairs;
};

/**
  * Parameters for metadata handling
  */
struct MetaDataParams {
    enum Mode {
        TUNNEL,
        EDIT,
        STRIP
    };
    Mode mode;
    std::vector<std::string> exifKeys;
    ExifPairs exif;
    IPTCPairs iptc;

    MetaDataParams();

    bool operator ==(const MetaDataParams &other) const;
    bool operator !=(const MetaDataParams &other) const;

    static std::vector<std::string> basicExifKeys;
};

/**
* Directional pyramid equalizer params
*/
struct DirPyrEqualizerParams {
    bool enabled;
    bool gamutlab;
    double mult[6];
    double threshold;
    double skinprotect;
    Threshold<int> hueskin;
    Glib::ustring cbdlMethod;

    DirPyrEqualizerParams();

    bool operator ==(const DirPyrEqualizerParams& other) const;
    bool operator !=(const DirPyrEqualizerParams& other) const;
};

/**
 * HSV equalizer params
 */
struct HSVEqualizerParams {
    bool enabled;
    std::vector<double> hcurve;
    std::vector<double> scurve;
    std::vector<double> vcurve;

    HSVEqualizerParams();

    bool operator ==(const HSVEqualizerParams& other) const;
    bool operator !=(const HSVEqualizerParams& other) const;
};

/**
 *  Film simulation params
 */
struct FilmSimulationParams {
    bool enabled;
    Glib::ustring clutFilename;
    int strength;

    FilmSimulationParams();

    bool operator ==(const FilmSimulationParams& other) const;
    bool operator !=(const FilmSimulationParams& other) const;
};

struct SoftLightParams {
    bool enabled;
    int strength;

    SoftLightParams();

    bool operator==(const SoftLightParams &other) const;
    bool operator!=(const SoftLightParams &other) const;
};


struct DehazeParams {
    bool enabled;
    int strength;
    int saturation;
    bool showDepthMap;
    int depth;

    DehazeParams();

    bool operator==(const DehazeParams &other) const;
    bool operator!=(const DehazeParams &other) const;
};

/**
  * Parameters of film negative
  */
struct FilmNegativeParams {
    bool enabled;
    double redRatio;
    double greenExp;
    double blueRatio;

    struct RGB {
        float r, g, b;

        bool operator ==(const RGB& other) const;
        bool operator !=(const RGB& other) const;
        RGB operator *(const RGB& other) const;
    };

    RGB refInput;
    RGB refOutput;

    enum class ColorSpace {
        INPUT = 0,
        WORKING
        // TODO : add support for custom color profile
    };

    ColorSpace colorSpace;

    enum class BackCompat { CURRENT = 0, V1, V2 };
    BackCompat backCompat;

    FilmNegativeParams();

    bool operator ==(const FilmNegativeParams& other) const;
    bool operator !=(const FilmNegativeParams& other) const;
};

/**
  * This class holds all the processing parameters applied on the images
  */
class ProcParams
{

public:
    ToneCurveParams         toneCurve;       ///< Tone curve parameters
    LCurveParams            labCurve;        ///< CIELAB luminance curve parameters
    RetinexParams           retinex;         ///< Retinex parameters
    LocalContrastParams     localContrast;   ////< Local contrast parameters
    RGBCurvesParams         rgbCurves;       ///< RGB curves parameters
    ColorToningParams       colorToning;     ///< Color Toning parameters
    SharpeningParams        sharpening;      ///< Sharpening parameters
    SharpeningParams        prsharpening;    ///< Sharpening parameters for post resize sharpening
    CaptureSharpeningParams pdsharpening;    ///< Sharpening parameters for post demosaic sharpening
    SharpenEdgeParams       sharpenEdge;     ///< Sharpen edge parameters
    SharpenMicroParams      sharpenMicro;    ///< Sharpen microcontrast parameters
    VibranceParams          vibrance;        ///< Vibrance parameters
    WBParams                wb;              ///< White balance parameters
    ColorAppearanceParams   colorappearance;
    DefringeParams          defringe;        ///< Defringing parameters
    ImpulseDenoiseParams    impulseDenoise;  ///< Impulse denoising parameters
    DirPyrDenoiseParams     dirpyrDenoise;   ///< Directional Pyramid denoising parameters
    EPDParams               epd;             ///< Edge Preserving Decomposition parameters
    FattalToneMappingParams fattal;          ///< Fattal02 tone mapping
    SHParams                sh;              ///< Shadow/highlight enhancement parameters
    CGParams                cg;              ///< Compression gamut
    ToneEqualizerParams     toneEqualizer;   ///< Tone equalizer parameters
    CropParams              crop;            ///< Crop parameters
    CropGuideParams         cropGuide;       ///< Crop guide parameters
    CoarseTransformParams   coarse;          ///< Coarse transformation (90, 180, 270 deg rotation, h/v flipping) parameters
    CommonTransformParams   commonTrans;     ///< Common transformation parameters (autofill)
    RotateParams            rotate;          ///< Rotation parameters
    DistortionParams        distortion;      ///< Lens distortion correction parameters
    LensProfParams          lensProf;        ///< Lens correction profile parameters
    PerspectiveParams       perspective;     ///< Perspective correction parameters
    GradientParams          gradient;        ///< Gradient filter parameters
    LocallabParams          locallab;        ///< Local lab parameters
    PCVignetteParams        pcvignette;      ///< Post-crop vignette filter parameters
    CACorrParams            cacorrection;    ///< Lens c/a correction parameters
    VignettingParams        vignetting;      ///< Lens vignetting correction parameters
    ChannelMixerParams      chmixer;         ///< Channel mixer parameters
    BlackWhiteParams        blackwhite;      ///< Black&  White parameters
    ResizeParams            resize;          ///< Resize parameters
    FramingParams           framing;         ///< Framing parameters
    SpotParams              spot;            ///< Spot removal tool
    ColorManagementParams   icm;             ///< profiles/color spaces used during the image processing
    RAWParams               raw;             ///< RAW parameters before demosaicing
    WaveletParams           wavelet;         ///< Wavelet parameters
    DirPyrEqualizerParams   dirpyrequalizer; ///< directional pyramid wavelet parameters
    HSVEqualizerParams      hsvequalizer;    ///< hsv wavelet parameters
    FilmSimulationParams    filmSimulation;  ///< film simulation parameters
    SoftLightParams         softlight;       ///< softlight parameters
    DehazeParams            dehaze;          ///< dehaze parameters
    FilmNegativeParams      filmNegative;    ///< Film negative parameters
    int                     rank;            ///< Custom image quality ranking
    int                     colorlabel;      ///< Custom color label
    bool                    inTrash;         ///< Marks deleted image
    Glib::ustring           appVersion;      ///< Version of the application that generated the parameters
    int                     ppVersion;       ///< Version of the PP file from which the parameters have been read

    MetaDataParams          metadata;        ///< Metadata parameters
    // ExifPairs               exif;            ///< List of modifications appplied on the exif tags of the input image
    // IPTCPairs               iptc;            ///< The IPTC tags and values to be saved to the output image

    /**
      * The constructor only sets the hand-wired defaults.
      */
    ProcParams();
    /**
      * Sets the hand-wired defaults parameters.
      */
    void setDefaults();
    /**
      * Saves the parameters to possibly two files. This is a performance improvement if a function has to
      * save the same file in two different location, i.e. the cache and the image's directory
      * @param fname   the name of the first file (can be an empty string)
      * @param fname2  the name of the second file (can be an empty string) (optional)
      * @param fnameAbsolute set to false if embedded filenames (if any, darkframe/flatfield) should be stored as relative
      * filenames if they are inside the same directory or in a sub-directory to fname's directory.
      * @param pedited pointer to a ParamsEdited object (optional) to store which values has to be saved
      * @return Error code (=0 if all supplied filenames where created correctly)
      */
    int save(const Glib::ustring& fname, const Glib::ustring& fname2 = Glib::ustring(), bool fnameAbsolute = true, ParamsEdited* pedited = nullptr);
    /**
      * Loads the parameters from a file.
      * @param fname the name of the file
      * @params pedited pointer to a ParamsEdited object (optional) to store which values has been loaded
      * @return Error code (=0 if no error)
      */
    int load(const Glib::ustring& fname, ParamsEdited* pedited = nullptr);

    /** Creates a new instance of ProcParams.
      * @return a pointer to the new ProcParams instance. */
    static ProcParams* create();

    /** Destroys an instance of ProcParams.
      * @param pp a pointer to the ProcParams instance to destroy. */
    static void destroy(ProcParams* pp);

    static void init() {}
    static void cleanup() {}

    bool operator ==(const ProcParams& other) const;
    bool operator !=(const ProcParams& other) const { return !(*this == other); }

private:
    /** Write the ProcParams's text in the file of the given name.
    * @param fname the name of the file
    * @param content the text to write
    * @return Error code (=0 if no error)
    * */
    int write(const Glib::ustring& fname, const Glib::ustring& content) const;
};

/**
  * This class associate a ProcParams object and a ParamEdited object through a pointer
  * to instance of each type in order to handle partial pp3 file loading (and later maybe
  * saving too)
  *
  * PartialProfile is not responsible of ProcParams and ParamsEdited object creation
  * and hence is not responsible of their destructions. The function that instantiate
  * PartialProfile object has to handle all this itself.
  */
class PartialProfile :
    public NonCopyable
{
public:
    PartialProfile(bool createInstance = false, bool paramsEditedValue = false);
    explicit PartialProfile(ProcParams* pp, ParamsEdited* pe = nullptr, bool fullCopy = false);
    explicit PartialProfile(const ProcParams* pp, const ParamsEdited* pe = nullptr);
    void deleteInstance();
    void clearGeneral();
    int  load(const Glib::ustring& fName);
    void set(bool v);
    void applyTo(ProcParams* destParams, bool fromLastSaved = false) const ;

    rtengine::procparams::ProcParams* pparams;
    ParamsEdited* pedited;
};

/**
  * This class automatically create the pparams and pedited instance in the constructor,
  * and automatically delete them in the destructor. This class has been mostly created
  * to be used with vectors, which use the default constructor/destructor
  */
class AutoPartialProfile :
    public PartialProfile
{
public:
    AutoPartialProfile();
    ~AutoPartialProfile();
};

}
}
