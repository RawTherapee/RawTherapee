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

#include "raw.h"

#include "serdes.h"
#include "utils.h"

#include "rtgui/options.h"
#include "rtgui/paramsedited.h"

#include <glibmm/keyfile.h>

namespace rtengine {
namespace procparams {

CaptureSharpeningParams::CaptureSharpeningParams() :
    enabled(false),
    autoContrast(true),
    autoRadius(true),
    contrast(10.0),
    noisecap(0.),
    noisecapafter(0.),
    deconvradius(0.75),
    deconvradiusOffset(0.0),
    deconviter(20),
    deconvitercheck(true),
    showcap(false),
    noisecaptype(true)

{
}

bool CaptureSharpeningParams::operator ==(const CaptureSharpeningParams& other) const
{
    return
        enabled == other.enabled
        && contrast == other.contrast
        && autoContrast == other.autoContrast
        && autoRadius == other.autoRadius
        && deconvradius == other.deconvradius
        && deconvitercheck == other.deconvitercheck
        && showcap == other.showcap
        && noisecaptype == other.noisecaptype
        && noisecap == other.noisecap
        && noisecapafter == other.noisecapafter
        && deconvradiusOffset == other.deconvradiusOffset
        && deconviter == other.deconviter;
}

bool CaptureSharpeningParams::operator !=(const CaptureSharpeningParams& other) const
{
    return !(*this == other);
}

RAWParams::BayerSensor::BayerSensor() :
    method(getMethodString(Method::AMAZE)),
    border(4),
    imageNum(0),
    ccSteps(0),
    black0(0.0),
    black1(0.0),
    black2(0.0),
    black3(0.0),
    twogreen(true),
    Dehablack(false),
    linenoise(0),
    linenoiseDirection(LineNoiseDirection::BOTH),
    greenthresh(0),
    dcb_iterations(2),
    lmmse_iterations(2),
    dualDemosaicAutoContrast(true),
    dualDemosaicContrast(20),
    pixelShiftMotionCorrectionMethod(PSMotionCorrectionMethod::AUTO),
    pixelShiftEperIso(0.0),
    pixelShiftSigma(1.0),
    pixelShiftShowMotion(false),
    pixelShiftShowMotionMaskOnly(false),
    pixelShiftHoleFill(true),
    pixelShiftMedian(false),
    pixelShiftAverage(false),
    pixelShiftGreen(true),
    pixelShiftBlur(true),
    pixelShiftSmoothFactor(0.7),
    pixelShiftEqualBright(false),
    pixelShiftEqualBrightChannel(false),
    pixelShiftNonGreenCross(true),
    pixelShiftDemosaicMethod(getPSDemosaicMethodString(PSDemosaicMethod::AMAZE)),
    dcb_enhance(true),
    pdafLinesFilter(false)
{
}

bool RAWParams::BayerSensor::operator ==(const BayerSensor& other) const
{
    return
        method == other.method
        && border == other.border
        && imageNum == other.imageNum
        && ccSteps == other.ccSteps
        && black0 == other.black0
        && black1 == other.black1
        && black2 == other.black2
        && black3 == other.black3
        && twogreen == other.twogreen
        && Dehablack == other.Dehablack
        && linenoise == other.linenoise
        && linenoiseDirection == other.linenoiseDirection
        && greenthresh == other.greenthresh
        && dcb_iterations == other.dcb_iterations
        && lmmse_iterations == other.lmmse_iterations
        && dualDemosaicAutoContrast == other.dualDemosaicAutoContrast
        && dualDemosaicContrast == other.dualDemosaicContrast
        && pixelShiftMotionCorrectionMethod == other.pixelShiftMotionCorrectionMethod
        && pixelShiftEperIso == other.pixelShiftEperIso
        && pixelShiftSigma == other.pixelShiftSigma
        && pixelShiftShowMotion == other.pixelShiftShowMotion
        && pixelShiftShowMotionMaskOnly == other.pixelShiftShowMotionMaskOnly
        && pixelShiftHoleFill == other.pixelShiftHoleFill
        && pixelShiftMedian == other.pixelShiftMedian
        && pixelShiftAverage == other.pixelShiftAverage
        && pixelShiftGreen == other.pixelShiftGreen
        && pixelShiftBlur == other.pixelShiftBlur
        && pixelShiftSmoothFactor == other.pixelShiftSmoothFactor
        && pixelShiftEqualBright == other.pixelShiftEqualBright
        && pixelShiftEqualBrightChannel == other.pixelShiftEqualBrightChannel
        && pixelShiftNonGreenCross == other.pixelShiftNonGreenCross
        && pixelShiftDemosaicMethod == other.pixelShiftDemosaicMethod
        && dcb_enhance == other.dcb_enhance
        && pdafLinesFilter == other.pdafLinesFilter;
}

bool RAWParams::BayerSensor::operator !=(const BayerSensor& other) const
{
    return !(*this == other);
}

void RAWParams::BayerSensor::setPixelShiftDefaults()
{
    pixelShiftMotionCorrectionMethod = RAWParams::BayerSensor::PSMotionCorrectionMethod::AUTO;
    pixelShiftEperIso = 0.0;
    pixelShiftSigma = 1.0;
    pixelShiftHoleFill = true;
    pixelShiftMedian = false;
    pixelShiftAverage = false;
    pixelShiftGreen = true;
    pixelShiftBlur = true;
    pixelShiftSmoothFactor = 0.7;
    pixelShiftEqualBright = false;
    pixelShiftEqualBrightChannel = false;
    pixelShiftNonGreenCross = true;
    pixelShiftDemosaicMethod = getPSDemosaicMethodString(PSDemosaicMethod::AMAZE);
}

const std::vector<const char*>& RAWParams::BayerSensor::getMethodStrings()
{
    static const std::vector<const char*> method_strings {
        "amaze",
        "amazebilinear",
        "amazevng4",
        "rcd",
        "rcdbilinear",
        "rcdvng4",
        "dcb",
        "dcbbilinear",
        "dcbvng4",
        "lmmse",
        "igv",
        "ahd",
        "eahd",
        "hphd",
        "vng4",
        "fast",
        "mono",
        "pixelshift",
        "none"
    };
    return method_strings;
}

Glib::ustring RAWParams::BayerSensor::getMethodString(Method method)
{
    return getMethodStrings()[toUnderlying(method)];
}

const std::vector<const char*>& RAWParams::BayerSensor::getPSDemosaicMethodStrings()
{
    static const std::vector<const char*> method_strings {
        "amaze",
        "amazevng4",
        "rcdvng4",
        "lmmse"
    };
    return method_strings;
}

Glib::ustring RAWParams::BayerSensor::getPSDemosaicMethodString(PSDemosaicMethod method)
{
    return getPSDemosaicMethodStrings()[toUnderlying(method)];
}

RAWParams::XTransSensor::XTransSensor() :
    method(getMethodString(Method::THREE_PASS)),
    dualDemosaicAutoContrast(true),
    dualDemosaicContrast(20),
    border(7),
    ccSteps(0),
    blackred(0.0),
    blackgreen(0.0),
    blackblue(0.0),
    Dehablackx(false)

{
}

bool RAWParams::XTransSensor::operator ==(const XTransSensor& other) const
{
    return
        method == other.method
        && dualDemosaicAutoContrast == other.dualDemosaicAutoContrast
        && dualDemosaicContrast == other.dualDemosaicContrast
        && border == other.border
        && ccSteps == other.ccSteps
        && blackred == other.blackred
        && blackgreen == other.blackgreen
        && blackblue == other.blackblue
        && Dehablackx == other.Dehablackx;
}

bool RAWParams::XTransSensor::operator !=(const XTransSensor& other) const
{
    return !(*this == other);
}

const std::vector<const char*>& RAWParams::XTransSensor::getMethodStrings()
{
    static const std::vector<const char*> method_strings {
        "4-pass",
        "3-pass (best)",
        "2-pass",
        "1-pass (medium)",
        "fast",
        "mono",
        "none"
    };
    return method_strings;
}

Glib::ustring RAWParams::XTransSensor::getMethodString(Method method)
{
    return getMethodStrings()[toUnderlying(method)];
}


RAWParams::PreprocessWB::PreprocessWB() :
    mode(Mode::AUTO)
{
}

bool RAWParams::PreprocessWB::operator ==(const PreprocessWB& other) const
{
    return mode == other.mode;
}

bool RAWParams::PreprocessWB::operator !=(const PreprocessWB& other) const
{
    return !(*this == other);
}


RAWParams::RAWParams() :
    df_autoselect(false),
    ff_AutoSelect(false),
    ff_FromMetaData(false),
    ff_BlurRadius(32),
    ff_BlurType(getFlatFieldBlurTypeString(FlatFieldBlurType::AREA)),
    ff_AutoClipControl(false),
    ff_clipControl(0),
    ca_autocorrect(false),
    ca_avoidcolourshift(true),
    caautoiterations(2),
    cared(0.0),
    cablue(0.0),
    expos(1.0),
    hotPixelFilter(false),
    deadPixelFilter(false),
    hotdeadpix_thresh(100)
{
}

bool RAWParams::operator ==(const RAWParams& other) const
{
    return
        bayersensor == other.bayersensor
        && xtranssensor == other.xtranssensor
        && dark_frame == other.dark_frame
        && df_autoselect == other.df_autoselect
        && ff_file == other.ff_file
        && ff_AutoSelect == other.ff_AutoSelect
        && ff_FromMetaData == other.ff_FromMetaData
        && ff_BlurRadius == other.ff_BlurRadius
        && ff_BlurType == other.ff_BlurType
        && ff_AutoClipControl == other.ff_AutoClipControl
        && ff_clipControl == other.ff_clipControl
        && ca_autocorrect == other.ca_autocorrect
        && ca_avoidcolourshift == other.ca_avoidcolourshift
        && caautoiterations == other.caautoiterations
        && cared == other.cared
        && cablue == other.cablue
        && expos == other.expos
        && preprocessWB == other.preprocessWB
        && hotPixelFilter == other.hotPixelFilter
        && deadPixelFilter == other.deadPixelFilter
        && hotdeadpix_thresh == other.hotdeadpix_thresh;
}

bool RAWParams::operator !=(const RAWParams& other) const
{
    return !(*this == other);
}

const std::vector<const char*>& RAWParams::getFlatFieldBlurTypeStrings()
{
    static const std::vector<const char*> blur_type_strings {
        "Area Flatfield",
        "Vertical Flatfield",
        "Horizontal Flatfield",
        "V+H Flatfield"
    };
    return blur_type_strings;
}

Glib::ustring RAWParams::getFlatFieldBlurTypeString(FlatFieldBlurType type)
{
    return getFlatFieldBlurTypeStrings()[toUnderlying(type)];
}

void loadCaptureSharpeningParams(const Glib::KeyFile& keyFile,
                                 CaptureSharpeningParams& pdsharpening,
                                 ParamsEdited* pedited)
{
    if (!keyFile.has_group("PostDemosaicSharpening")) return;

    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "Enabled", pdsharpening.enabled, pedited->pdsharpening.enabled);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "Contrast", pdsharpening.contrast, pedited->pdsharpening.contrast);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "AutoContrast", pdsharpening.autoContrast, pedited->pdsharpening.autoContrast);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "AutoRadius", pdsharpening.autoRadius, pedited->pdsharpening.autoRadius);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "DeconvRadius", pdsharpening.deconvradius, pedited->pdsharpening.deconvradius);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "DeconvRadiusOffset", pdsharpening.deconvradiusOffset, pedited->pdsharpening.deconvradiusOffset);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "DeconvIterCheck", pdsharpening.deconvitercheck, pedited->pdsharpening.deconvitercheck);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "Showcap", pdsharpening.showcap, pedited->pdsharpening.showcap);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "Noisecaptype", pdsharpening.noisecaptype, pedited->pdsharpening.noisecaptype);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "Noisecap", pdsharpening.noisecap, pedited->pdsharpening.noisecap);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "Noisecapafter", pdsharpening.noisecapafter, pedited->pdsharpening.noisecapafter);
    assignFromKeyfile(keyFile, "PostDemosaicSharpening", "DeconvIterations", pdsharpening.deconviter, pedited->pdsharpening.deconviter);
}

void saveCaptureSharpeningParams(Glib::KeyFile& keyFile,
                                 const CaptureSharpeningParams& pdsharpening,
                                 const ParamsEdited* pedited)
{
    saveToKeyfile(!pedited || pedited->pdsharpening.enabled, "PostDemosaicSharpening", "Enabled", pdsharpening.enabled, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.contrast, "PostDemosaicSharpening", "Contrast", pdsharpening.contrast, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.autoContrast, "PostDemosaicSharpening", "AutoContrast", pdsharpening.autoContrast, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.autoRadius, "PostDemosaicSharpening", "AutoRadius", pdsharpening.autoRadius, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.deconvradius, "PostDemosaicSharpening", "DeconvRadius", pdsharpening.deconvradius, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.deconvradiusOffset, "PostDemosaicSharpening", "DeconvRadiusOffset", pdsharpening.deconvradiusOffset, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.deconvitercheck, "PostDemosaicSharpening", "DeconvIterCheck", pdsharpening.deconvitercheck, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.showcap, "PostDemosaicSharpening", "Showcap", pdsharpening.showcap, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.noisecaptype, "PostDemosaicSharpening", "Noisecaptype", pdsharpening.noisecaptype, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.noisecap, "PostDemosaicSharpening", "Noisecap", pdsharpening.noisecap, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.noisecapafter, "PostDemosaicSharpening", "Noisecapafter", pdsharpening.noisecapafter, keyFile);
    saveToKeyfile(!pedited || pedited->pdsharpening.deconviter, "PostDemosaicSharpening", "DeconvIterations", pdsharpening.deconviter, keyFile);
}

void loadRawParams(const Glib::KeyFile& keyFile, RAWParams& raw,
                   ParamsEdited* pedited, const Glib::ustring& fname, int ppVersion)
{
    const auto& options = App::get().options();

    if (keyFile.has_group("RAW")) {
        if (keyFile.has_key("RAW", "DarkFrame")) {
            raw.dark_frame = expandRelativePath2(fname, options.rtSettings.darkFramesPath, "", keyFile.get_string("RAW", "DarkFrame"));

            if (pedited) {
                pedited->raw.darkFrame = true;
            }
        }
        assignFromKeyfile(keyFile, "RAW", "DarkFrameAuto", raw.df_autoselect, pedited->raw.df_autoselect);
        if (keyFile.has_key("RAW", "FlatFieldFile")) {
            raw.ff_file = expandRelativePath2(fname, options.rtSettings.flatFieldsPath, "", keyFile.get_string("RAW", "FlatFieldFile"));

            if (pedited) {
                pedited->raw.ff_file = true;
            }
        }
        assignFromKeyfile(keyFile, "RAW", "FlatFieldAutoSelect", raw.ff_AutoSelect, pedited->raw.ff_AutoSelect);
        assignFromKeyfile(keyFile, "RAW", "FlatFieldFromMetaData", raw.ff_FromMetaData, pedited->raw.ff_FromMetaData);
        assignFromKeyfile(keyFile, "RAW", "FlatFieldBlurRadius", raw.ff_BlurRadius, pedited->raw.ff_BlurRadius);
        assignFromKeyfile(keyFile, "RAW", "FlatFieldBlurType", raw.ff_BlurType, pedited->raw.ff_BlurType);
        assignFromKeyfile(keyFile, "RAW", "FlatFieldAutoClipControl", raw.ff_AutoClipControl, pedited->raw.ff_AutoClipControl);

        if (ppVersion < 328) {
            // With ppversion < 328 this value was stored as a boolean, which is nonsense.
            // To avoid annoying warnings we skip reading and assume 0.
            raw.ff_clipControl = 0;
        } else {
            assignFromKeyfile(keyFile, "RAW", "FlatFieldClipControl", raw.ff_clipControl, pedited->raw.ff_clipControl);
        }

        assignFromKeyfile(keyFile, "RAW", "CA", raw.ca_autocorrect, pedited->raw.ca_autocorrect);
        if (ppVersion >= 342) {
            assignFromKeyfile(keyFile, "RAW", "CAAutoIterations", raw.caautoiterations, pedited->raw.caautoiterations);
        } else {
            raw.caautoiterations = 1;
        }

        if (ppVersion >= 343) {
            assignFromKeyfile(keyFile, "RAW", "CAAvoidColourshift", raw.ca_avoidcolourshift, pedited->raw.ca_avoidcolourshift);
        } else {
            raw.ca_avoidcolourshift = false;
        }
        assignFromKeyfile(keyFile, "RAW", "CARed", raw.cared, pedited->raw.cared);
        assignFromKeyfile(keyFile, "RAW", "CABlue", raw.cablue, pedited->raw.cablue);
        // For compatibility to elder pp3 versions
        assignFromKeyfile(keyFile, "RAW", "HotDeadPixels", raw.hotPixelFilter, pedited->raw.hotPixelFilter);
        raw.deadPixelFilter = raw.hotPixelFilter;

        if (pedited) {
            pedited->raw.deadPixelFilter = pedited->raw.hotPixelFilter;
        }

        assignFromKeyfile(keyFile, "RAW", "HotPixelFilter", raw.hotPixelFilter, pedited->raw.hotPixelFilter);
        assignFromKeyfile(keyFile, "RAW", "DeadPixelFilter", raw.deadPixelFilter, pedited->raw.deadPixelFilter);
        assignFromKeyfile(keyFile, "RAW", "HotDeadPixelThresh", raw.hotdeadpix_thresh, pedited->raw.hotdeadpix_thresh);
        assignFromKeyfile(keyFile, "RAW", "PreExposure", raw.expos, pedited->raw.exPos);

        if (ppVersion < 320) {
            assignFromKeyfile(keyFile, "RAW", "Method", raw.bayersensor.method, pedited->raw.bayersensor.method);
            assignFromKeyfile(keyFile, "RAW", "CcSteps", raw.bayersensor.ccSteps, pedited->raw.bayersensor.ccSteps);
            assignFromKeyfile(keyFile, "RAW", "LineDenoise", raw.bayersensor.linenoise, pedited->raw.bayersensor.linenoise);
            assignFromKeyfile(keyFile, "RAW", "GreenEqThreshold", raw.bayersensor.greenthresh, pedited->raw.bayersensor.greenEq);
            assignFromKeyfile(keyFile, "RAW", "DCBIterations", raw.bayersensor.dcb_iterations, pedited->raw.bayersensor.dcbIterations);
            assignFromKeyfile(keyFile, "RAW", "DCBEnhance", raw.bayersensor.dcb_enhance, pedited->raw.bayersensor.dcbEnhance);
            assignFromKeyfile(keyFile, "RAW", "LMMSEIterations", raw.bayersensor.lmmse_iterations, pedited->raw.bayersensor.lmmseIterations);
            assignFromKeyfile(keyFile, "RAW", "PreBlackzero", raw.bayersensor.black0, pedited->raw.bayersensor.exBlack0);
            assignFromKeyfile(keyFile, "RAW", "PreBlackone", raw.bayersensor.black1, pedited->raw.bayersensor.exBlack1);
            assignFromKeyfile(keyFile, "RAW", "PreBlacktwo", raw.bayersensor.black2, pedited->raw.bayersensor.exBlack2);
            assignFromKeyfile(keyFile, "RAW", "PreBlackthree", raw.bayersensor.black3, pedited->raw.bayersensor.exBlack3);
            assignFromKeyfile(keyFile, "RAW", "PreTwoGreen", raw.bayersensor.twogreen, pedited->raw.bayersensor.exTwoGreen);
        }
    }

    if (keyFile.has_group("RAW Bayer")) {
        assignFromKeyfile(keyFile, "RAW Bayer", "Method", raw.bayersensor.method, pedited->raw.bayersensor.method);
        assignFromKeyfile(keyFile, "RAW Bayer", "Border", raw.bayersensor.border, pedited->raw.bayersensor.border);

        if (keyFile.has_key("RAW Bayer", "ImageNum")) {
            raw.bayersensor.imageNum = keyFile.get_integer("RAW Bayer", "ImageNum") - 1;

            if (pedited) {
                pedited->raw.bayersensor.imageNum = true;
            }
        }

        assignFromKeyfile(keyFile, "RAW Bayer", "CcSteps", raw.bayersensor.ccSteps, pedited->raw.bayersensor.ccSteps);
        assignFromKeyfile(keyFile, "RAW Bayer", "PreBlack0", raw.bayersensor.black0, pedited->raw.bayersensor.exBlack0);
        assignFromKeyfile(keyFile, "RAW Bayer", "PreBlack1", raw.bayersensor.black1, pedited->raw.bayersensor.exBlack1);
        assignFromKeyfile(keyFile, "RAW Bayer", "PreBlack2", raw.bayersensor.black2, pedited->raw.bayersensor.exBlack2);
        assignFromKeyfile(keyFile, "RAW Bayer", "PreBlack3", raw.bayersensor.black3, pedited->raw.bayersensor.exBlack3);
        assignFromKeyfile(keyFile, "RAW Bayer", "PreTwoGreen", raw.bayersensor.twogreen, pedited->raw.bayersensor.exTwoGreen);
        assignFromKeyfile(keyFile, "RAW Bayer", "Dehablack", raw.bayersensor.Dehablack, pedited->raw.bayersensor.Dehablack);
        assignFromKeyfile(keyFile, "RAW Bayer", "LineDenoise", raw.bayersensor.linenoise, pedited->raw.bayersensor.linenoise);

        if (keyFile.has_key("RAW Bayer", "LineDenoiseDirection")) {
            raw.bayersensor.linenoiseDirection = RAWParams::BayerSensor::LineNoiseDirection(keyFile.get_integer("RAW Bayer", "LineDenoiseDirection"));

            if (pedited) {
                pedited->raw.bayersensor.linenoiseDirection = true;
            }
        }

        assignFromKeyfile(keyFile, "RAW Bayer", "GreenEqThreshold", raw.bayersensor.greenthresh, pedited->raw.bayersensor.greenEq);
        assignFromKeyfile(keyFile, "RAW Bayer", "DCBIterations", raw.bayersensor.dcb_iterations, pedited->raw.bayersensor.dcbIterations);
        assignFromKeyfile(keyFile, "RAW Bayer", "DCBEnhance", raw.bayersensor.dcb_enhance, pedited->raw.bayersensor.dcbEnhance);
        assignFromKeyfile(keyFile, "RAW Bayer", "LMMSEIterations", raw.bayersensor.lmmse_iterations, pedited->raw.bayersensor.lmmseIterations);
        assignFromKeyfile(keyFile, "RAW Bayer", "DualDemosaicAutoContrast", raw.bayersensor.dualDemosaicAutoContrast, pedited->raw.bayersensor.dualDemosaicAutoContrast);
        if (ppVersion < 345) {
            raw.bayersensor.dualDemosaicAutoContrast = false;
            if (pedited) {
                pedited->raw.bayersensor.dualDemosaicAutoContrast = true;
            }
        }
        assignFromKeyfile(keyFile, "RAW Bayer", "DualDemosaicContrast", raw.bayersensor.dualDemosaicContrast, pedited->raw.bayersensor.dualDemosaicContrast);

        if (keyFile.has_key("RAW Bayer", "PixelShiftMotionCorrectionMethod")) {
            raw.bayersensor.pixelShiftMotionCorrectionMethod = (RAWParams::BayerSensor::PSMotionCorrectionMethod)keyFile.get_integer("RAW Bayer", "PixelShiftMotionCorrectionMethod");

            if (pedited) {
                pedited->raw.bayersensor.pixelShiftMotionCorrectionMethod = true;
            }
        }

        assignFromKeyfile(keyFile, "RAW Bayer", "PixelShiftEperIso", raw.bayersensor.pixelShiftEperIso, pedited->raw.bayersensor.pixelShiftEperIso);

        if (ppVersion < 332) {
            raw.bayersensor.pixelShiftEperIso += 1.0;
        }

        assignFromKeyfile(keyFile, "RAW Bayer", "PixelShiftSigma", raw.bayersensor.pixelShiftSigma, pedited->raw.bayersensor.pixelShiftSigma);
        assignFromKeyfile(keyFile, "RAW Bayer", "PixelShiftShowMotion", raw.bayersensor.pixelShiftShowMotion, pedited->raw.bayersensor.pixelShiftShowMotion);
        assignFromKeyfile(keyFile, "RAW Bayer", "PixelShiftShowMotionMaskOnly", raw.bayersensor.pixelShiftShowMotionMaskOnly, pedited->raw.bayersensor.pixelShiftShowMotionMaskOnly);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftHoleFill", raw.bayersensor.pixelShiftHoleFill, pedited->raw.bayersensor.pixelShiftHoleFill);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftMedian", raw.bayersensor.pixelShiftMedian, pedited->raw.bayersensor.pixelShiftMedian);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftAverage", raw.bayersensor.pixelShiftAverage, pedited->raw.bayersensor.pixelShiftAverage);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftGreen", raw.bayersensor.pixelShiftGreen, pedited->raw.bayersensor.pixelShiftGreen);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftBlur", raw.bayersensor.pixelShiftBlur, pedited->raw.bayersensor.pixelShiftBlur);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftSmoothFactor", raw.bayersensor.pixelShiftSmoothFactor, pedited->raw.bayersensor.pixelShiftSmooth);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftEqualBright", raw.bayersensor.pixelShiftEqualBright, pedited->raw.bayersensor.pixelShiftEqualBright);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftEqualBrightChannel", raw.bayersensor.pixelShiftEqualBrightChannel, pedited->raw.bayersensor.pixelShiftEqualBrightChannel);
        assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftNonGreenCross", raw.bayersensor.pixelShiftNonGreenCross, pedited->raw.bayersensor.pixelShiftNonGreenCross);

        if (ppVersion < 336) {
            if (keyFile.has_key("RAW Bayer", "pixelShiftLmmse")) {
                const bool useLmmse = keyFile.get_boolean("RAW Bayer", "pixelShiftLmmse");

                if (useLmmse) {
                    raw.bayersensor.pixelShiftDemosaicMethod = raw.bayersensor.getPSDemosaicMethodString(RAWParams::BayerSensor::PSDemosaicMethod::LMMSE);
                } else {
                    raw.bayersensor.pixelShiftDemosaicMethod = raw.bayersensor.getPSDemosaicMethodString(RAWParams::BayerSensor::PSDemosaicMethod::AMAZE);
                }

                if (pedited) {
                    pedited->raw.bayersensor.pixelShiftDemosaicMethod = true;
                }
            }
        } else {
            assignFromKeyfile(keyFile, "RAW Bayer", "pixelShiftDemosaicMethod", raw.bayersensor.pixelShiftDemosaicMethod, pedited->raw.bayersensor.pixelShiftDemosaicMethod);
        }

        assignFromKeyfile(keyFile, "RAW Bayer", "PDAFLinesFilter", raw.bayersensor.pdafLinesFilter, pedited->raw.bayersensor.pdafLinesFilter);
    }

    if (keyFile.has_group("RAW X-Trans")) {
        assignFromKeyfile(keyFile, "RAW X-Trans", "Method", raw.xtranssensor.method, pedited->raw.xtranssensor.method);
        assignFromKeyfile(keyFile, "RAW X-Trans", "DualDemosaicAutoContrast", raw.xtranssensor.dualDemosaicAutoContrast, pedited->raw.xtranssensor.dualDemosaicAutoContrast);
        if (ppVersion < 345) {
            raw.xtranssensor.dualDemosaicAutoContrast = false;
            if (pedited) {
                pedited->raw.xtranssensor.dualDemosaicAutoContrast = true;
            }
        }
        assignFromKeyfile(keyFile, "RAW X-Trans", "DualDemosaicContrast", raw.xtranssensor.dualDemosaicContrast, pedited->raw.xtranssensor.dualDemosaicContrast);
        assignFromKeyfile(keyFile, "RAW X-Trans", "Border", raw.xtranssensor.border, pedited->raw.xtranssensor.border);
        assignFromKeyfile(keyFile, "RAW X-Trans", "CcSteps", raw.xtranssensor.ccSteps, pedited->raw.xtranssensor.ccSteps);
        assignFromKeyfile(keyFile, "RAW X-Trans", "PreBlackRed", raw.xtranssensor.blackred, pedited->raw.xtranssensor.exBlackRed);
        assignFromKeyfile(keyFile, "RAW X-Trans", "PreBlackGreen", raw.xtranssensor.blackgreen, pedited->raw.xtranssensor.exBlackGreen);
        assignFromKeyfile(keyFile, "RAW X-Trans", "PreBlackBlue", raw.xtranssensor.blackblue, pedited->raw.xtranssensor.exBlackBlue);
        assignFromKeyfile(keyFile, "RAW X-Trans", "Dehablackx", raw.xtranssensor.Dehablackx, pedited->raw.xtranssensor.Dehablackx);
    }

    if (keyFile.has_group("RAW Preprocess WB")) {
        if (keyFile.has_key("RAW Preprocess WB", "Mode")) {
            raw.preprocessWB.mode = RAWParams::PreprocessWB::Mode(keyFile.get_integer("RAW Preprocess WB", "Mode"));

            if (pedited) {
                pedited->raw.preprocessWB.mode = true;
            }
        }
    }
}

void saveRawParams(Glib::KeyFile& keyFile,
                   const RAWParams& raw,
                   const ParamsEdited* pedited,
                   const Glib::ustring& fname,
                   bool fnameAbsolute)
{
    const auto& options = App::get().options();

    saveToKeyfile(!pedited || pedited->raw.darkFrame, "RAW", "DarkFrame", relativePathIfInside2(fname, options.rtSettings.darkFramesPath, fnameAbsolute, raw.dark_frame), keyFile);
    saveToKeyfile(!pedited || pedited->raw.df_autoselect, "RAW", "DarkFrameAuto", raw.df_autoselect, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_file, "RAW", "FlatFieldFile", relativePathIfInside2(fname, options.rtSettings.flatFieldsPath, fnameAbsolute, raw.ff_file), keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_AutoSelect, "RAW", "FlatFieldAutoSelect", raw.ff_AutoSelect, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_FromMetaData, "RAW", "FlatFieldFromMetaData", raw.ff_FromMetaData, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_BlurRadius, "RAW", "FlatFieldBlurRadius", raw.ff_BlurRadius, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_BlurType, "RAW", "FlatFieldBlurType", raw.ff_BlurType, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_AutoClipControl, "RAW", "FlatFieldAutoClipControl", raw.ff_AutoClipControl, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ff_clipControl, "RAW", "FlatFieldClipControl", raw.ff_clipControl, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ca_autocorrect, "RAW", "CA", raw.ca_autocorrect, keyFile);
    saveToKeyfile(!pedited || pedited->raw.ca_avoidcolourshift, "RAW", "CAAvoidColourshift", raw.ca_avoidcolourshift, keyFile);
    saveToKeyfile(!pedited || pedited->raw.caautoiterations, "RAW", "CAAutoIterations", raw.caautoiterations, keyFile);
    saveToKeyfile(!pedited || pedited->raw.cared, "RAW", "CARed", raw.cared, keyFile);
    saveToKeyfile(!pedited || pedited->raw.cablue, "RAW", "CABlue", raw.cablue, keyFile);
    saveToKeyfile(!pedited || pedited->raw.hotPixelFilter, "RAW", "HotPixelFilter", raw.hotPixelFilter, keyFile);
    saveToKeyfile(!pedited || pedited->raw.deadPixelFilter, "RAW", "DeadPixelFilter", raw.deadPixelFilter, keyFile);
    saveToKeyfile(!pedited || pedited->raw.hotdeadpix_thresh, "RAW", "HotDeadPixelThresh", raw.hotdeadpix_thresh, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.method, "RAW Bayer", "Method", raw.bayersensor.method, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.border, "RAW Bayer", "Border", raw.bayersensor.border, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.imageNum, "RAW Bayer", "ImageNum", raw.bayersensor.imageNum + 1, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.ccSteps, "RAW Bayer", "CcSteps", raw.bayersensor.ccSteps, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.exBlack0, "RAW Bayer", "PreBlack0", raw.bayersensor.black0, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.exBlack1, "RAW Bayer", "PreBlack1", raw.bayersensor.black1, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.exBlack2, "RAW Bayer", "PreBlack2", raw.bayersensor.black2, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.exBlack3, "RAW Bayer", "PreBlack3", raw.bayersensor.black3, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.exTwoGreen, "RAW Bayer", "PreTwoGreen", raw.bayersensor.twogreen, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.Dehablack, "RAW Bayer", "Dehablack", raw.bayersensor.Dehablack, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.linenoise, "RAW Bayer", "LineDenoise", raw.bayersensor.linenoise, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.linenoiseDirection, "RAW Bayer", "LineDenoiseDirection", toUnderlying(raw.bayersensor.linenoiseDirection), keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.greenEq, "RAW Bayer", "GreenEqThreshold", raw.bayersensor.greenthresh, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.dcbIterations, "RAW Bayer", "DCBIterations", raw.bayersensor.dcb_iterations, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.dcbEnhance, "RAW Bayer", "DCBEnhance", raw.bayersensor.dcb_enhance, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.lmmseIterations, "RAW Bayer", "LMMSEIterations", raw.bayersensor.lmmse_iterations, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.dualDemosaicAutoContrast, "RAW Bayer", "DualDemosaicAutoContrast", raw.bayersensor.dualDemosaicAutoContrast, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.dualDemosaicContrast, "RAW Bayer", "DualDemosaicContrast", raw.bayersensor.dualDemosaicContrast, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftMotionCorrectionMethod, "RAW Bayer", "PixelShiftMotionCorrectionMethod", toUnderlying(raw.bayersensor.pixelShiftMotionCorrectionMethod), keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftEperIso, "RAW Bayer", "PixelShiftEperIso", raw.bayersensor.pixelShiftEperIso, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftSigma, "RAW Bayer", "PixelShiftSigma", raw.bayersensor.pixelShiftSigma, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftShowMotion, "RAW Bayer", "PixelShiftShowMotion", raw.bayersensor.pixelShiftShowMotion, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftShowMotionMaskOnly, "RAW Bayer", "PixelShiftShowMotionMaskOnly", raw.bayersensor.pixelShiftShowMotionMaskOnly, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftHoleFill, "RAW Bayer", "pixelShiftHoleFill", raw.bayersensor.pixelShiftHoleFill, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftAverage, "RAW Bayer", "pixelShiftAverage", raw.bayersensor.pixelShiftAverage, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftMedian, "RAW Bayer", "pixelShiftMedian", raw.bayersensor.pixelShiftMedian, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftGreen, "RAW Bayer", "pixelShiftGreen", raw.bayersensor.pixelShiftGreen, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftBlur, "RAW Bayer", "pixelShiftBlur", raw.bayersensor.pixelShiftBlur, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftSmooth, "RAW Bayer", "pixelShiftSmoothFactor", raw.bayersensor.pixelShiftSmoothFactor, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftEqualBright, "RAW Bayer", "pixelShiftEqualBright", raw.bayersensor.pixelShiftEqualBright, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftEqualBrightChannel, "RAW Bayer", "pixelShiftEqualBrightChannel", raw.bayersensor.pixelShiftEqualBrightChannel, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftNonGreenCross, "RAW Bayer", "pixelShiftNonGreenCross", raw.bayersensor.pixelShiftNonGreenCross, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pixelShiftDemosaicMethod, "RAW Bayer", "pixelShiftDemosaicMethod", raw.bayersensor.pixelShiftDemosaicMethod, keyFile);
    saveToKeyfile(!pedited || pedited->raw.bayersensor.pdafLinesFilter, "RAW Bayer", "PDAFLinesFilter", raw.bayersensor.pdafLinesFilter, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.method, "RAW X-Trans", "Method", raw.xtranssensor.method, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.dualDemosaicAutoContrast, "RAW X-Trans", "DualDemosaicAutoContrast", raw.xtranssensor.dualDemosaicAutoContrast, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.dualDemosaicContrast, "RAW X-Trans", "DualDemosaicContrast", raw.xtranssensor.dualDemosaicContrast, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.border, "RAW X-Trans", "Border", raw.xtranssensor.border, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.ccSteps, "RAW X-Trans", "CcSteps", raw.xtranssensor.ccSteps, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.exBlackRed, "RAW X-Trans", "PreBlackRed", raw.xtranssensor.blackred, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.exBlackGreen, "RAW X-Trans", "PreBlackGreen", raw.xtranssensor.blackgreen, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.exBlackBlue, "RAW X-Trans", "PreBlackBlue", raw.xtranssensor.blackblue, keyFile);
    saveToKeyfile(!pedited || pedited->raw.xtranssensor.Dehablackx, "RAW X-Trans", "Dehablackx", raw.xtranssensor.Dehablackx, keyFile);

    // Raw exposition
    saveToKeyfile(!pedited || pedited->raw.exPos, "RAW", "PreExposure", raw.expos, keyFile);

    // Preprocess WB
    saveToKeyfile(!pedited || pedited->raw.preprocessWB.mode, "RAW Preprocess WB", "Mode", toUnderlying(raw.preprocessWB.mode), keyFile);
}

}  // namespace procparams
}  // namespace rtengine
