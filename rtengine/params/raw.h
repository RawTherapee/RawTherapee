/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "threshold.h"

#include <glibmm/ustring.h>

#include <vector>

namespace Glib {
class KeyFile;
}

class ParamsEdited;

namespace rtengine {
namespace procparams {

struct CaptureSharpeningParams {
    bool           enabled;
    bool           autoContrast;
    bool           autoRadius;
    double         contrast;
    double         noisecap;
    double         noisecapafter;
    double         deconvradius;
    double         deconvradiusOffset;
    int            deconviter;
    bool           deconvitercheck;
    bool           showcap;
    bool           noisecaptype;

    CaptureSharpeningParams();

    bool operator ==(const CaptureSharpeningParams& other) const;
    bool operator !=(const CaptureSharpeningParams& other) const;
};

/**
  * Parameters for RAW demosaicing, common to all sensor type
  */
struct RAWParams {
    /**
     * Parameters for RAW demosaicing specific to Bayer sensors
     */
    struct BayerSensor {
        enum class Method {
            AMAZE,
            AMAZEBILINEAR,
            AMAZEVNG4,
            RCD,
            RCDBILINEAR,
            RCDVNG4,
            DCB,
            DCBBILINEAR,
            DCBVNG4,
            LMMSE,
            IGV,
            AHD,
            EAHD,
            HPHD,
            VNG4,
            FAST,
            MONO,
            PIXELSHIFT,
            NONE
        };

        enum class PSMotionCorrectionMethod {
            OFF,
            AUTO,
            CUSTOM
        };

        enum class PSDemosaicMethod {
            AMAZE,
            AMAZEVNG4,
            RCDVNG4,
            LMMSE
        };

        Glib::ustring method;
        int border;
        int imageNum;
        int ccSteps;
        double black0;
        double black1;
        double black2;
        double black3;
        bool twogreen;
        bool Dehablack;
        int linenoise;
        enum class LineNoiseDirection {
            HORIZONTAL = 1,
            VERTICAL,
            BOTH,
            PDAF_LINES = 5
        };
        LineNoiseDirection linenoiseDirection;
        int greenthresh;
        int dcb_iterations;
        int lmmse_iterations;
        bool dualDemosaicAutoContrast;
        double dualDemosaicContrast;
        PSMotionCorrectionMethod pixelShiftMotionCorrectionMethod;
        double pixelShiftEperIso;
        double pixelShiftSigma;
        bool pixelShiftShowMotion;
        bool pixelShiftShowMotionMaskOnly;
        bool pixelShiftHoleFill;
        bool pixelShiftMedian;
        bool pixelShiftAverage;
        bool pixelShiftGreen;
        bool pixelShiftBlur;
        double pixelShiftSmoothFactor;
        bool pixelShiftEqualBright;
        bool pixelShiftEqualBrightChannel;
        bool pixelShiftNonGreenCross;
        Glib::ustring pixelShiftDemosaicMethod;
        bool dcb_enhance;
        bool pdafLinesFilter;

        BayerSensor();

        bool operator ==(const BayerSensor& other) const;
        bool operator !=(const BayerSensor& other) const;

        void setPixelShiftDefaults();

        static const std::vector<const char*>& getMethodStrings();
        static Glib::ustring getMethodString(Method method);

        static const std::vector<const char*>& getPSDemosaicMethodStrings();
        static Glib::ustring getPSDemosaicMethodString(PSDemosaicMethod method);
    };

    /**
     * Parameters for RAW demosaicing specific to X-Trans sensors
     */
    struct XTransSensor {
        enum class Method {
            FOUR_PASS,
            THREE_PASS,
            TWO_PASS,
            ONE_PASS,
            FAST,
            MONO,
            NONE
        };

        Glib::ustring method;
        bool dualDemosaicAutoContrast;
        double dualDemosaicContrast;
        int border;
        int ccSteps;
        double blackred;
        double blackgreen;
        double blackblue;
        bool Dehablackx;

        XTransSensor();

        bool operator ==(const XTransSensor& other) const;
        bool operator !=(const XTransSensor& other) const;

        static const std::vector<const char*>& getMethodStrings();
        static Glib::ustring getMethodString(Method method);
    };

    BayerSensor bayersensor;         ///< RAW parameters for Bayer sensors
    XTransSensor xtranssensor;       ///< RAW parameters for X-Trans sensors

    enum class FlatFieldBlurType {
        AREA,
        V,
        H,
        VH,
    };

    Glib::ustring dark_frame;
    bool df_autoselect;

    Glib::ustring ff_file;
    bool ff_AutoSelect;
    bool ff_FromMetaData;
    int ff_BlurRadius;
    Glib::ustring ff_BlurType;
    bool ff_AutoClipControl;
    int ff_clipControl;

    bool ca_autocorrect;
    bool ca_avoidcolourshift;
    int caautoiterations;
    double cared;
    double cablue;

    // exposure before interpolation
    double expos;

    struct PreprocessWB {
        enum class Mode {
            CAMERA = 0,
            AUTO
        };

        Mode mode;

        PreprocessWB();

        bool operator ==(const PreprocessWB& other) const;
        bool operator !=(const PreprocessWB& other) const;
    };

    PreprocessWB preprocessWB;

    bool hotPixelFilter;
    bool deadPixelFilter;
    int hotdeadpix_thresh;

    RAWParams();

    bool operator ==(const RAWParams& other) const;
    bool operator !=(const RAWParams& other) const;

    static const std::vector<const char*>& getFlatFieldBlurTypeStrings();
    static Glib::ustring getFlatFieldBlurTypeString(FlatFieldBlurType type);
};

void loadCaptureSharpeningParams(const Glib::KeyFile& keyFile,
                                 CaptureSharpeningParams& pdsharpening,
                                 ParamsEdited* pedited);

void saveCaptureSharpeningParams(Glib::KeyFile& keyFile,
                                 const CaptureSharpeningParams& pdsharpening,
                                 const ParamsEdited* pedited);

void loadRawParams(const Glib::KeyFile& keyFile, RAWParams& raw,
                   ParamsEdited* pedited, const Glib::ustring& fname, int ppVersion);

void saveRawParams(Glib::KeyFile& keyFile,
                   const RAWParams& raw,
                   const ParamsEdited* pedited,
                   const Glib::ustring& fname,
                   bool fnameAbsolute);

}  // namespace procparams
}  // namespace rtengine
