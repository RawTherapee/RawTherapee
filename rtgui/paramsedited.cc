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
#include <cstring>

#include "paramsedited.h"

#include "addsetids.h"
#include "options.h"

#include "rtengine/procparams.h"

namespace
{

using namespace rtengine::procparams;

void setAll(CropGuideParamsEdited& guide, bool v)
{
    if (v) {
        guide.presets.set();
    } else {
        guide.presets.reset();
    }
    guide.enabled = v;
    guide.mirror_golden_triangle = v;
    guide.rotate_golden_ratio = v;
    guide.mirror_golden_ratio = v;
    guide.aspect_ratios = v;
    guide.bleed = v;
    guide.basis = v;
}

void initFrom(CropGuideParamsEdited& edits, const ProcParams& params,
              const ProcParams& otherParams)
{
    const CropGuideParams& curr = params.cropGuide;
    const CropGuideParams& other = otherParams.cropGuide;

    for (size_t i = 0; i < edits.presets.size(); i++) {
        bool is_edited = edits.presets[i];
        is_edited &= curr.presets[i] == other.presets[i];
        edits.presets[i] = is_edited;
    }
    edits.enabled &= curr.enabled == other.enabled;
    edits.mirror_golden_triangle &=
        curr.mirror_golden_triangle == other.mirror_golden_triangle;
    edits.rotate_golden_ratio &=
        curr.rotate_golden_ratio == other.rotate_golden_ratio;
    edits.mirror_golden_ratio &=
        curr.mirror_golden_ratio == other.mirror_golden_ratio;
    edits.aspect_ratios &=
        curr.aspect_ratios == other.aspect_ratios;
    edits.bleed &= curr.bleed == other.bleed;
    edits.basis &= curr.basis == other.basis;
}

void combine(CropGuideParams& toEdit, const CropGuideParams& mod,
             const CropGuideParamsEdited& edits)
{
    for (size_t i = 0; i < edits.presets.size(); i++) {
        if (edits.presets[i]) {
            toEdit.presets[i] = mod.presets[i];
        }
    }
    if (edits.enabled) {
        toEdit.enabled = mod.enabled;
    }
    if (edits.mirror_golden_triangle) {
        toEdit.mirror_golden_triangle = mod.mirror_golden_triangle;
    }
    if (edits.rotate_golden_ratio) {
        toEdit.rotate_golden_ratio = mod.rotate_golden_ratio;
    }
    if (edits.mirror_golden_ratio) {
        toEdit.mirror_golden_ratio = mod.mirror_golden_ratio;
    }
    if (edits.aspect_ratios) {
        toEdit.aspect_ratios = mod.aspect_ratios;
    }
    if (edits.bleed) {
        toEdit.bleed = mod.bleed;
    }
    if (edits.basis) {
        toEdit.basis = mod.basis;
    }
}

void setAll(FramingParamsEdited& framing, bool v)
{
    framing.enabled = v;
    framing.framingMethod = v;
    framing.aspectRatio = v;
    framing.orientation = v;
    framing.framedWidth = v;
    framing.framedHeight = v;
    framing.allowUpscaling = v;

    framing.borderSizingMethod = v;
    framing.basis = v;
    framing.relativeBorderSize = v;
    framing.minSizeEnabled = v;
    framing.minWidth = v;
    framing.minHeight = v;
    framing.absWidth = v;
    framing.absHeight = v;

    framing.borderRed = v;
    framing.borderGreen = v;
    framing.borderBlue = v;
}

void initFrom(FramingParamsEdited& edits, const ProcParams& params, const ProcParams& otherParams)
{
    const FramingParams& curr = params.framing;
    const FramingParams& other = otherParams.framing;

    edits.enabled &= curr.enabled == other.enabled;
    edits.framingMethod &= curr.framingMethod == other.framingMethod;
    edits.aspectRatio &= curr.aspectRatio == other.aspectRatio;
    edits.orientation &= curr.orientation == other.orientation;
    edits.framedWidth &= curr.framedWidth == other.framedWidth;
    edits.framedHeight &= curr.framedHeight == other.framedHeight;
    edits.allowUpscaling &= curr.allowUpscaling == other.allowUpscaling;

    edits.borderSizingMethod &= curr.borderSizingMethod == other.borderSizingMethod;
    edits.basis &= curr.basis == other.basis;
    edits.relativeBorderSize &= curr.relativeBorderSize == other.relativeBorderSize;
    edits.minSizeEnabled &= curr.minSizeEnabled == other.minSizeEnabled;
    edits.minWidth &= curr.minWidth == other.minWidth;
    edits.minHeight &= curr.minHeight == other.minHeight;
    edits.absWidth &= curr.absWidth == other.absWidth;
    edits.absHeight &= curr.absHeight == other.absHeight;

    edits.borderRed &= curr.borderRed == other.borderRed;
    edits.borderGreen &= curr.borderGreen == other.borderGreen;
    edits.borderBlue &= curr.borderBlue == other.borderBlue;
}

void combine(FramingParams& toEdit, const FramingParams& mod, const FramingParamsEdited& edits,
             bool dontForceSet)
{
    const auto& options = App::get().options();

    if (edits.enabled) {
        toEdit.enabled = mod.enabled;
    }
    if (edits.framingMethod) {
        toEdit.framingMethod = mod.framingMethod;
    }
    if (edits.aspectRatio) {
        toEdit.aspectRatio = mod.aspectRatio;
    }
    if (edits.orientation) {
        toEdit.orientation = mod.orientation;
    }
    if (edits.framedWidth) {
        toEdit.framedWidth = mod.framedWidth;
    }
    if (edits.framedHeight) {
        toEdit.framedHeight = mod.framedHeight;
    }
    if (edits.allowUpscaling) {
        toEdit.allowUpscaling = mod.allowUpscaling;
    }

    if (edits.borderSizingMethod) {
        toEdit.borderSizingMethod = mod.borderSizingMethod;
    }
    if (edits.basis) {
        toEdit.basis = mod.basis;
    }
    if (edits.relativeBorderSize) {
        toEdit.relativeBorderSize =
            dontForceSet && options.baBehav[ADDSET_FRAMING_RELATIVE_SCALE] ?
                toEdit.relativeBorderSize + mod.relativeBorderSize :
                mod.relativeBorderSize;
    }
    if (edits.minSizeEnabled) {
        toEdit.minSizeEnabled = mod.minSizeEnabled;
    }
    if (edits.minWidth) {
        toEdit.minWidth = mod.minWidth;
    }
    if (edits.minHeight) {
        toEdit.minHeight = mod.minHeight;
    }
    if (edits.absWidth) {
        toEdit.absWidth = mod.absWidth;
    }
    if (edits.absHeight) {
        toEdit.absHeight = mod.absHeight;
    }

    if (edits.borderRed) {
        toEdit.borderRed = dontForceSet && options.baBehav[ADDSET_FRAMING_BORDER_RED] ?
            toEdit.borderRed + mod.borderRed :
            mod.borderRed;
    }
    if (edits.borderGreen) {
        toEdit.borderGreen = dontForceSet && options.baBehav[ADDSET_FRAMING_BORDER_GREEN] ?
            toEdit.borderGreen + mod.borderGreen :
            mod.borderGreen;
    }
    if (edits.borderBlue) {
        toEdit.borderBlue = dontForceSet && options.baBehav[ADDSET_FRAMING_BORDER_BLUE] ?
            toEdit.borderBlue + mod.borderBlue :
            mod.borderBlue;
    }
}

}  // namespace

ParamsEdited::ParamsEdited(bool value)
{

    set(value);
}

void ParamsEdited::set(bool v)
{

    general.rank         = v;
    general.colorlabel   = v;
    general.intrash      = v;
    toneCurve.curve      = v;
    toneCurve.curve2     = v;
    toneCurve.curveMode  = v;
    toneCurve.curveMode2 = v;
    toneCurve.brightness = v;
    toneCurve.black      = v;
    toneCurve.contrast   = v;
    toneCurve.saturation = v;
    toneCurve.shcompr    = v;
    toneCurve.hlcompr    = v;
    toneCurve.hlbl    = v;
    toneCurve.hlth    = v;
    toneCurve.hlcomprthresh = v;
    toneCurve.autoexp    = v;
    toneCurve.clip       = v;
    toneCurve.expcomp    = v;
    toneCurve.hrenabled   = v;
    toneCurve.method    = v;
    toneCurve.histmatching = v;
    toneCurve.fromHistMatching = v;
    toneCurve.clampOOG = v;
    retinex.cdcurve    = v;
    retinex.mapcurve    = v;
    retinex.cdHcurve    = v;
    retinex.lhcurve    = v;
    retinex.complexmethod    = v;
    retinex.retinexMethod    = v;
    retinex.mapMethod    = v;
    retinex.viewMethod    = v;
    retinex.retinexcolorspace    = v;
    retinex.gammaretinex    = v;
    retinex.enabled    = v;
    retinex.str    = v;
    retinex.scal    = v;
    retinex.iter    = v;
    retinex.grad    = v;
    retinex.grads    = v;
    retinex.gam    = v;
    retinex.slope    = v;
    retinex.neigh    = v;
    retinex.offs    = v;
    retinex.vart    = v;
    retinex.limd    = v;
    retinex.highl    = v;
    retinex.skal    = v;
    retinex.medianmap = v;
    retinex.transmissionCurve   = v;
    retinex.gaintransmissionCurve   = v;
    retinex.highlights    = v;
    retinex.htonalwidth   = v;
    retinex.shadows       = v;
    retinex.stonalwidth   = v;
    retinex.radius        = v;

    retinex.retinex = v;
    labCurve.enabled = v;
    labCurve.lcurve      = v;
    labCurve.acurve      = v;
    labCurve.bcurve      = v;
    labCurve.cccurve     = v;
    labCurve.chcurve     = v;
    labCurve.lhcurve     = v;
    labCurve.hhcurve     = v;
    labCurve.lccurve    = v;
    labCurve.clcurve    = v;
    labCurve.brightness  = v;
    labCurve.contrast    = v;
    labCurve.chromaticity    = v;
    labCurve.gamutmunselmethod = v;
    labCurve.rstprotection   = v;
    labCurve.lcredsk         = v;
    localContrast.enabled = v;
    localContrast.radius = v;
    localContrast.amount = v;
    localContrast.darkness = v;
    localContrast.lightness = v;
    rgbCurves.enabled = v;
    rgbCurves.lumamode       = v;
    rgbCurves.rcurve         = v;
    rgbCurves.gcurve         = v;
    rgbCurves.bcurve         = v;
    colorToning.enabled      = v;
    colorToning.autosat      = v;
    colorToning.opacityCurve = v;
    colorToning.colorCurve   = v;
    colorToning.satprotectionthreshold = v;
    colorToning.saturatedopacity       = v;
    colorToning.strength               = v;
    colorToning.shadowsColSat          = v;
    colorToning.hlColSat   = v;
    colorToning.balance    = v;
    colorToning.clcurve    = v;
    colorToning.method     = v;
    colorToning.twocolor   = v;
    colorToning.cl2curve   = v;
    colorToning.redlow     = v;
    colorToning.greenlow   = v;
    colorToning.bluelow    = v;
    colorToning.satlow     = v;
    colorToning.sathigh    = v;
    colorToning.redmed     = v;
    colorToning.greenmed   = v;
    colorToning.bluemed    = v;
    colorToning.redhigh    = v;
    colorToning.greenhigh  = v;
    colorToning.bluehigh   = v;
    colorToning.lumamode   = v;
    colorToning.labgridALow = v;
    colorToning.labgridBLow = v;
    colorToning.labgridAHigh = v;
    colorToning.labgridBHigh = v;
    colorToning.labregions = v;
    colorToning.labregionsShowMask = v;

    sharpening.enabled            = v;
    sharpening.contrast           = v;
    sharpening.radius             = v;
    sharpening.blurradius         = v;
    sharpening.gamma         = v;
    sharpening.amount             = v;
    sharpening.threshold          = v;
    sharpening.edgesonly          = v;
    sharpening.edges_radius       = v;
    sharpening.edges_tolerance    = v;
    sharpening.halocontrol        = v;
    sharpening.halocontrol_amount = v;
    sharpening.method         = v;
    sharpening.deconvamount   = v;
    sharpening.deconvradius   = v;
    sharpening.deconviter     = v;
    sharpening.deconvdamping  = v;
    pdsharpening.enabled            = v;
    pdsharpening.contrast           = v;
    pdsharpening.autoContrast           = v;
    pdsharpening.autoRadius           = v;
    pdsharpening.deconvradius   = v;
    pdsharpening.deconvradiusOffset   = v;
    pdsharpening.deconviter     = v;
    pdsharpening.deconvitercheck     = v;
    pdsharpening.showcap     = v;
    pdsharpening.noisecap     = v;
    pdsharpening.noisecaptype     = v;
    pdsharpening.noisecapafter     = v;
    prsharpening.enabled            = v;
    prsharpening.contrast           = v;
    prsharpening.radius             = v;
    prsharpening.amount             = v;
    prsharpening.threshold          = v;
    prsharpening.edgesonly          = v;
    prsharpening.edges_radius       = v;
    prsharpening.edges_tolerance    = v;
    prsharpening.halocontrol        = v;
    prsharpening.halocontrol_amount = v;
    prsharpening.method         = v;
    prsharpening.deconvamount   = v;
    prsharpening.deconvradius   = v;
    prsharpening.deconviter     = v;
    prsharpening.deconvdamping  = v;
    sharpenEdge.enabled       = v;
    sharpenEdge.passes        = v;
    sharpenEdge.amount        = v;
    sharpenEdge.threechannels = v;
    sharpenMicro.enabled      = v;
    sharpenMicro.matrix       = v;
    sharpenMicro.contrast     = v;
    sharpenMicro.amount       = v;
    sharpenMicro.uniformity   = v;
    vibrance.enabled          = v;
    vibrance.pastels          = v;
    vibrance.saturated        = v;
    vibrance.psthreshold      = v;
    vibrance.protectskins     = v;
    vibrance.avoidcolorshift  = v;
    vibrance.pastsattog       = v;
    vibrance.skintonescurve   = v;
    colorappearance.enabled    = v;
    colorappearance.degree     = v;
    colorappearance.autodegree = v;
    colorappearance.degreeout     = v;
    colorappearance.autodegreeout = v;
    colorappearance.surround     = v;
    colorappearance.surrsrc     = v;
    colorappearance.adapscen    = v;
    colorappearance.autoadapscen = v;
    colorappearance.ybscen    = v;
    colorappearance.autoybscen = v;
    colorappearance.adaplum    = v;
    colorappearance.badpixsl    = v;
    colorappearance.wbmodel    = v;
    colorappearance.illum    = v;
    colorappearance.algo    = v;

    colorappearance.jlight     = v;
    colorappearance.qbright     = v;
    colorappearance.chroma     = v;
    colorappearance.schroma     = v;
    colorappearance.schromared     = v;
    colorappearance.schromagreen     = v;
    colorappearance.schromablue   = v;
    colorappearance.mchroma     = v;
    colorappearance.contrast     = v;
    colorappearance.qcontrast     = v;
    colorappearance.colorh     = v;
    colorappearance.colorhred     = v;
    colorappearance.colorhgreen     = v;
    colorappearance.colorhblue     = v;
    colorappearance.rstprotection     = v;
    colorappearance.surrsource = v;
    colorappearance.gamut = v;
//  colorappearance.badpix = v;
    colorappearance.datacie = v;
    colorappearance.tonecie = v;
//  colorappearance.sharpcie = v;
    colorappearance.curve      = v;
    colorappearance.curvered   = v;
    colorappearance.curvegreen   = v;
    colorappearance.curveblue   = v;
    colorappearance.curve2     = v;
    colorappearance.curve3     = v;
    colorappearance.curveMode  = v;
    colorappearance.curveMode2 = v;
    colorappearance.curveMode3 = v;
    colorappearance.complexmethod = v;
    colorappearance.modelmethod = v;
    colorappearance.catmethod = v;
    colorappearance.tempout     = v;
    colorappearance.autotempout = v;
    colorappearance.greenout     = v;
    colorappearance.ybout     = v;
    colorappearance.tempsc     = v;
    colorappearance.greensc     = v;

    //colorBoost.amount         = v;
    //colorBoost.avoidclip      = v;
    //colorBoost.enable_saturationlimiter = v;
    //colorBoost.saturationlimit = v;
    wb.enabled                 = v;
    wb.method                  = v;
    wb.green                   = v;
    wb.temperature             = v;
    wb.equal                   = v;
    wb.tempBias                = v;
    wb.observer                = v;
    wb.itcwb_green                = v;
    wb.itcwb_rgreen                = v;
    wb.itcwb_nopurple             = v;
    wb.itcwb_alg             = v;
    wb.itcwb_prim                = v;
    wb.itcwb_sampling         = v;
    wb.compat_version          = v;
    //colorShift.a               = v;
    //colorShift.b               = v;
    //lumaDenoise.enabled        = v;
    //lumaDenoise.radius         = v;
    //lumaDenoise.edgetolerance  = v;
    //colorDenoise.enabled       = v;
    //colorDenoise.amount        = v;
    defringe.enabled           = v;
    defringe.radius            = v;
    defringe.threshold         = v;
    defringe.huecurve          = v;
    impulseDenoise.enabled     = v;
    impulseDenoise.thresh      = v;
    dirpyrDenoise.enabled      = v;
    dirpyrDenoise.enhance      = v;
//  dirpyrDenoise.perform      = v;
    dirpyrDenoise.lcurve      = v;
    dirpyrDenoise.cccurve      = v;
    dirpyrDenoise.median      = v;
    dirpyrDenoise.luma         = v;
    dirpyrDenoise.Ldetail      = v;
    dirpyrDenoise.chroma       = v;
    dirpyrDenoise.redchro      = v;
    dirpyrDenoise.bluechro     = v;
    dirpyrDenoise.gain = v;
    dirpyrDenoise.gamma        = v;
    dirpyrDenoise.passes        = v;
    dirpyrDenoise.dmethod      = v;
    dirpyrDenoise.Lmethod      = v;
    dirpyrDenoise.Cmethod      = v;
    dirpyrDenoise.C2method      = v;
    dirpyrDenoise.smethod      = v;
    dirpyrDenoise.medmethod      = v;
    dirpyrDenoise.methodmed      = v;
    dirpyrDenoise.rgbmethod      = v;
    epd.enabled                = v;
    epd.strength            = v;
    epd.gamma            = v;
    epd.edgeStopping        = v;
    epd.scale               = v;
    epd.reweightingIterates = v;
    fattal.enabled   = v;
    fattal.threshold = v;
    fattal.amount    = v;
    fattal.anchor    = v;
    sh.enabled       = v;
    sh.highlights    = v;
    sh.htonalwidth   = v;
    sh.shadows       = v;
    sh.stonalwidth   = v;
    sh.radius        = v;
    sh.lab           = v;
    cg.enabled = v;
    cg.th_c = v;
    cg.th_m = v;
    cg.th_y = v;
    cg.d_c = v;
    cg.autodc = v;
    cg.d_m = v;
    cg.autodm = v;
    cg.d_y = v;
    cg.autody = v;
    cg.pwr = v;
    cg.colorspace = v;
    cg.rolloff = v;

    toneEqualizer.enabled        = v;
    toneEqualizer.bands.fill(v);
    toneEqualizer.regularization = v;
    toneEqualizer.show_colormap  = v;
    toneEqualizer.pivot  = v;
    crop.enabled = v;
    crop.x       = v;
    crop.y       = v;
    crop.w       = v;
    crop.h       = v;
    crop.fixratio = v;
    crop.ratio   = v;
    crop.orientation = v;
    ::setAll(cropGuide, v);
    coarse.rotate = v;
    coarse.hflip = v;
    coarse.vflip = v;
    commonTrans.method = v;
    commonTrans.autofill = v;
    commonTrans.scale = v;
    commonTrans.scale_horizontally = v;
    commonTrans.scale_vertically = v;
    rotate.degree = v;
    distortion.amount = v;
    distortion.defish = v;
    distortion.focal_length = v;
    lensProf.lcMode = v;
    lensProf.lcpFile = v;
    lensProf.useDist = v;
    lensProf.useVign = v;
    lensProf.useCA = v;
    lensProf.useLensfun = v;
    lensProf.lfAutoMatch = v;
    lensProf.lfCameraMake = v;
    lensProf.lfCameraModel = v;
    lensProf.lfLens = v;
    perspective.method = v;
    perspective.horizontal = v;
    perspective.vertical = v;
    perspective.camera_crop_factor = v;
    perspective.camera_focal_length = v;
    perspective.camera_pitch = v;
    perspective.camera_roll = v;
    perspective.camera_shift_horiz = v;
    perspective.camera_shift_vert = v;
    perspective.camera_yaw = v;
    perspective.projection_pitch = v;
    perspective.projection_rotate = v;
    perspective.projection_shift_horiz = v;
    perspective.projection_shift_vert = v;
    perspective.projection_yaw = v;
    perspective.control_lines = v;
    gradient.enabled = v;
    gradient.degree = v;
    gradient.feather = v;
    gradient.strength = v;
    gradient.centerX = v;
    gradient.centerY = v;

    locallab.enabled = v;
    locallab.selspot = v;

    for (size_t i = 0; i < locallab.spots.size(); i++) {
        locallab.spots.at(i).set(v);
    }

    pcvignette.enabled = v;
    pcvignette.strength = v;
    pcvignette.feather = v;
    pcvignette.roundness = v;
    cacorrection.red = v;
    cacorrection.blue = v;
    vignetting.amount = v;
    vignetting.radius = v;
    vignetting.strength = v;
    vignetting.centerX = v;
    vignetting.centerY = v;
    chmixer.enabled = v;
    chmixer.red[0] = v;
    chmixer.red[1] = v;
    chmixer.red[2] = v;
    chmixer.green[0] = v;
    chmixer.green[1] = v;
    chmixer.green[2] = v;
    chmixer.blue[0] = v;
    chmixer.blue[1] = v;
    chmixer.blue[2] = v;
    blackwhite.enabled   = v;
    blackwhite.enabledcc   = v;
    blackwhite.mixerRed   = v;
    blackwhite.mixerOrange   = v;
    blackwhite.mixerYellow   = v;
    blackwhite.mixerGreen   = v;
    blackwhite.mixerCyan   = v;
    blackwhite.mixerBlue   = v;
    blackwhite.mixerMagenta   = v;
    blackwhite.mixerPurple   = v;
    blackwhite.gammaRed   = v;
    blackwhite.gammaGreen   = v;
    blackwhite.gammaBlue   = v;
    blackwhite.filter   = v;
    blackwhite.setting   = v;
    blackwhite.method   = v;
    blackwhite.luminanceCurve = v;
    blackwhite.beforeCurve      = v;
    blackwhite.beforeCurveMode  = v;
    blackwhite.afterCurve      = v;
    blackwhite.afterCurveMode  = v;
    blackwhite.autoc    = v;
    blackwhite.algo    = v;

    resize.scale     = v;
    resize.appliesTo = v;
    resize.method    = v;
    resize.dataspec  = v;
    resize.width     = v;
    resize.height    = v;
    resize.longedge  = v;
    resize.shortedge = v;
    resize.enabled   = v;
    resize.allowUpscaling = v;

    ::setAll(framing, v);

    spot.enabled = v;
    spot.entries = v;

    icm.inputProfile = v;
    icm.toneCurve = v;
    icm.applyLookTable = v;
    icm.applyBaselineExposureOffset = v;
    icm.applyHueSatMap = v;
    icm.dcpIlluminant = v;
    icm.workingProfile = v;
    icm.outputProfile = v;
    icm.outputIntent = v;
    icm.outputBPC = v;
    icm.wGamma = v;
    icm.wSlope = v;
    icm.wmidtcie = v;
    icm.sigmatrc = v;
    icm.offstrc = v;
    icm.residtrc = v;
    icm.wgampower = v;
    icm.wgamgain = v;
    icm.pyrwavtrc = v;
    icm.opacityCurveWLI = v;
    icm.wapsat = v;
    icm.wsmoothcie = v;
    icm.wsmoothciesli = v;
    icm.redx = v;
    icm.redy = v;
    icm.grex = v;
    icm.grey = v;
    icm.blux = v;
    icm.bluy = v;
    
    icm.redrot = v;
    icm.redsat = v;
    icm.grerot = v;
    icm.gresat = v;
    icm.blurot = v;
    icm.blusat = v;
    
    icm.refi = v;
    icm.shiftx = v;
    icm.shifty = v;
    icm.preser = v;
    icm.fbw = v;
    icm.trcExp = v;
    icm.wavExp = v;
    icm.gamut = v;
    icm.labgridcieALow = v;
    icm.labgridcieBLow = v;
    icm.labgridcieAHigh = v;
    icm.labgridcieBHigh = v;
    icm.labgridcieGx = v;
    icm.labgridcieGy = v;
    icm.labgridcieWx = v;
    icm.labgridcieWy = v;
    icm.labgridcieMx = v;
    icm.labgridcieMy = v;
    icm.aRendIntent = v;
    icm.workingTRC = v;
    icm.wgamut = v;
    icm.will = v;
    icm.wprim = v;
    icm.wcat = v;
    raw.bayersensor.method = v;
    raw.bayersensor.border = v;
    raw.bayersensor.imageNum = v;
    raw.bayersensor.ccSteps = v;
    raw.bayersensor.exBlack0 = v;
    raw.bayersensor.exBlack1 = v;
    raw.bayersensor.exBlack2 = v;
    raw.bayersensor.exBlack3 = v;
    raw.bayersensor.exTwoGreen = v;
    raw.bayersensor.Dehablack = v;
    raw.bayersensor.dcbIterations = v;
    raw.bayersensor.dcbEnhance = v;
    //raw.bayersensor.allEnhance = v;
    raw.bayersensor.lmmseIterations = v;
    raw.bayersensor.dualDemosaicAutoContrast = v;
    raw.bayersensor.dualDemosaicContrast = v;
    raw.bayersensor.pixelShiftMotionCorrectionMethod = v;
    raw.bayersensor.pixelShiftEperIso = v;
    raw.bayersensor.pixelShiftSigma = v;
    raw.bayersensor.pixelShiftShowMotion = v;
    raw.bayersensor.pixelShiftShowMotionMaskOnly = v;
    raw.bayersensor.pixelShiftHoleFill = v;
    raw.bayersensor.pixelShiftMedian = v;
    raw.bayersensor.pixelShiftAverage = v;
    raw.bayersensor.pixelShiftGreen = v;
    raw.bayersensor.pixelShiftBlur = v;
    raw.bayersensor.pixelShiftSmooth = v;
    raw.bayersensor.pixelShiftEqualBright = v;
    raw.bayersensor.pixelShiftEqualBrightChannel = v;
    raw.bayersensor.pixelShiftNonGreenCross = v;
    raw.bayersensor.pixelShiftDemosaicMethod = v;
    raw.bayersensor.greenEq = v;
    raw.bayersensor.linenoise = v;
    raw.bayersensor.linenoiseDirection = v;
    raw.bayersensor.pdafLinesFilter = v;
    raw.xtranssensor.method = v;
    raw.xtranssensor.dualDemosaicAutoContrast = v;
    raw.xtranssensor.dualDemosaicContrast = v;
    raw.xtranssensor.border = v;
    raw.xtranssensor.ccSteps = v;
    raw.xtranssensor.exBlackRed = v;
    raw.xtranssensor.exBlackGreen = v;
    raw.xtranssensor.exBlackBlue = v;
    raw.xtranssensor.Dehablackx = v;
    raw.ca_autocorrect = v;
    raw.ca_avoidcolourshift = v;
    raw.caautoiterations  = v;
    raw.cablue  = v;
    raw.cared   = v;
    raw.hotPixelFilter = v;
    raw.deadPixelFilter = v;
    raw.hotdeadpix_thresh = v;
    raw.darkFrame = v;
    raw.df_autoselect = v;
    raw.ff_file = v;
    raw.ff_AutoSelect = v;
    raw.ff_FromMetaData = v;
    raw.ff_BlurRadius = v;
    raw.ff_BlurType = v;
    raw.ff_AutoClipControl = v;
    raw.ff_clipControl = v;
    raw.exPos = v;
    wavelet.enabled = v;
    wavelet.strength = v;
    wavelet.balance = v;
    wavelet.iter = v;
    wavelet.sigmafin = v;
    wavelet.sigmaton = v;
    wavelet.sigmacol = v;
    wavelet.sigmadir = v;
    wavelet.rangeab = v;
    wavelet.protab = v;
    wavelet.median = v;
    wavelet.medianlev = v;
    wavelet.linkedg = v;
    wavelet.cbenab = v;
    wavelet.greenhigh = v;
    wavelet.greenmed = v;
    wavelet.greenlow = v;
    wavelet.bluehigh = v;
    wavelet.bluemed = v;
    wavelet.bluelow = v;
    wavelet.lipst = v;
    wavelet.ballum = v;
    wavelet.sigm = v;
    wavelet.levden = v;
    wavelet.thrden = v;
    wavelet.limden = v;
    wavelet.balchrom = v;
    wavelet.chromfi = v;
    wavelet.chromco = v;
    wavelet.mergeL = v;
    wavelet.mergeC = v;
    wavelet.softrad = v;
    wavelet.softradend = v;
    wavelet.strend = v;
    wavelet.detend = v;
    wavelet.thrend = v;
    wavelet.Medgreinf = v;
    wavelet.ushamethod = v;
    wavelet.avoid = v;
    wavelet.showmask = v;
    wavelet.oldsh = v;
    wavelet.tmr = v;
    wavelet.Lmethod = v;
    wavelet.CLmethod = v;
    wavelet.Backmethod = v;
    wavelet.Tilesmethod = v;
    wavelet.complexmethod = v;
    //wavelet.denmethod = v;
    wavelet.mixmethod = v;
    wavelet.slimethod = v;
    wavelet.quamethod = v;
    wavelet.daubcoeffmethod = v;
    wavelet.CHmethod = v;
    wavelet.CHSLmethod = v;
    wavelet.EDmethod = v;
    wavelet.NPmethod = v;
    wavelet.BAmethod = v;
    wavelet.TMmethod = v;
    wavelet.HSmethod = v;
    wavelet.Dirmethod = v;
    wavelet.sigma = v;
    wavelet.offset = v;
    wavelet.lowthr = v;
    wavelet.rescon = v;
    wavelet.resconH = v;
    wavelet.reschro = v;
    wavelet.resblur = v;
    wavelet.resblurc = v;
    wavelet.tmrs = v;
    wavelet.edgs = v;
    wavelet.scale = v;
    wavelet.gamma = v;
    wavelet.sup = v;
    wavelet.sky = v;
    wavelet.thres = v;
    wavelet.threshold = v;
    wavelet.threshold2 = v;
    wavelet.edgedetect = v;
    wavelet.edgedetectthr = v;
    wavelet.edgedetectthr2 = v;
    wavelet.edgesensi = v;
    wavelet.edgeampli = v;
    wavelet.chroma = v;
    wavelet.chro = v;
    wavelet.contrast = v;
    wavelet.edgrad = v;
    wavelet.edgeffect = v;
    wavelet.edgval = v;
    wavelet.edgthresh = v;
    wavelet.thr = v;
    wavelet.thrH = v;
    wavelet.radius = v;
    wavelet.skinprotect = v;
    wavelet.hueskin = v;
    wavelet.hueskin2 = v;
    wavelet.hllev = v;
    wavelet.bllev = v;
    wavelet.edgcont = v;
    wavelet.chrwav = v;
    wavelet.bluwav = v;
    wavelet.level0noise = v;
    wavelet.level1noise = v;
    wavelet.level2noise = v;
    wavelet.level3noise = v;
    wavelet.leveldenoise = v;
    wavelet.levelsigm = v;
    wavelet.ccwcurve = v;
    wavelet.blcurve = v;
    //wavelet.opacityCurveSH   = v;
    wavelet.opacityCurveRG   = v;
    wavelet.opacityCurveBY   = v;
    wavelet.wavdenoise   = v;
    wavelet.wavdenoiseh   = v;
    wavelet.opacityCurveW   = v;
    wavelet.opacityCurveWL   = v;
    wavelet.hhcurve     = v;
    wavelet.wavguidcurve     = v;
    wavelet.wavhuecurve     = v;
    wavelet.Chcurve     = v;
    wavelet.wavclCurve     = v;

    wavelet.pastlev = v;
    wavelet.satlev = v;
//  wavelet.enacont = v;
//  wavelet.enachrom = v;
//  wavelet.enaedge = v;
//  wavelet.enares = v;
    wavelet.expfinal = v;
    wavelet.expclari = v;
    wavelet.expcontrast = v;
    wavelet.expchroma = v;
    wavelet.expedge = v;
    wavelet.expbl = v;
    wavelet.expresid = v;
    wavelet.exptoning = v;
    wavelet.expnoise = v;
    wavelet.labgridALow = v;
    wavelet.labgridBLow = v;
    wavelet.labgridAHigh = v;
    wavelet.labgridBHigh = v;

    for (int i = 0; i < 9; i++) {
        wavelet.c[i] = v;
    }

    for (int i = 0; i < 9; i++) {
        wavelet.ch[i] = v;
    }

    dirpyrequalizer.enabled = v;
    dirpyrequalizer.gamutlab = v;
    dirpyrequalizer.cbdlMethod = v;


    for (int i = 0; i < 6; i++) {
        dirpyrequalizer.mult[i] = v;
    }

    dirpyrequalizer.threshold = v;
    dirpyrequalizer.skinprotect = v;
    dirpyrequalizer.hueskin = v;
    //dirpyrequalizer.algo = v;
    hsvequalizer.enabled = v;
    hsvequalizer.hcurve = v;
    hsvequalizer.scurve = v;
    hsvequalizer.vcurve = v;
    filmSimulation.enabled = v;
    filmSimulation.clutFilename = v;
    filmSimulation.strength = v;
    softlight.enabled = v;
    softlight.strength = v;
    dehaze.enabled = v;
    dehaze.strength = v;
    dehaze.showDepthMap = v;
    dehaze.depth = v;
    dehaze.saturation = v;
    metadata.mode = v;
    metadata.exifKeys = v;
    filmNegative.enabled = v;
    filmNegative.redRatio = v;
    filmNegative.greenExp = v;
    filmNegative.blueRatio = v;
    filmNegative.refInput = v;
    filmNegative.refOutput = v;
    filmNegative.colorSpace = v;
    raw.preprocessWB.mode = v;

    exif = v;
    iptc = v;
}

using namespace rtengine;
using namespace rtengine::procparams;

void ParamsEdited::initFrom(const std::vector<rtengine::procparams::ProcParams>& src)
{

    set(true);

    if (src.empty()) {
        return;
    }

    const ProcParams& p = src[0];

    // Resize LocallabSpotEdited according to src[0]
    locallab.spots.clear();
    locallab.spots.resize(p.locallab.spots.size(), LocallabParamsEdited::LocallabSpotEdited(true));

    // Variable used to determined if Locallab spots number is equal and so spots can be combined
    bool isSpotNumberEqual = true;

    for (size_t i = 1; i < src.size(); i++) {
        const ProcParams& other = src[i];
        toneCurve.curve = toneCurve.curve && p.toneCurve.curve == other.toneCurve.curve;
        toneCurve.curve2 = toneCurve.curve2 && p.toneCurve.curve2 == other.toneCurve.curve2;
        toneCurve.curveMode = toneCurve.curveMode && p.toneCurve.curveMode == other.toneCurve.curveMode;
        toneCurve.curveMode2 = toneCurve.curveMode2 && p.toneCurve.curveMode2 == other.toneCurve.curveMode2;
        toneCurve.brightness = toneCurve.brightness && p.toneCurve.brightness == other.toneCurve.brightness;
        toneCurve.black = toneCurve.black && p.toneCurve.black == other.toneCurve.black;
        toneCurve.contrast = toneCurve.contrast && p.toneCurve.contrast == other.toneCurve.contrast;
        toneCurve.saturation = toneCurve.saturation && p.toneCurve.saturation == other.toneCurve.saturation;
        toneCurve.shcompr = toneCurve.shcompr && p.toneCurve.shcompr == other.toneCurve.shcompr;
        toneCurve.hlcompr = toneCurve.hlcompr && p.toneCurve.hlcompr == other.toneCurve.hlcompr;
        toneCurve.hlbl = toneCurve.hlbl && p.toneCurve.hlbl == other.toneCurve.hlbl;
        toneCurve.hlth = toneCurve.hlth && p.toneCurve.hlth == other.toneCurve.hlth;
        toneCurve.hlcomprthresh = toneCurve.hlcomprthresh && p.toneCurve.hlcomprthresh == other.toneCurve.hlcomprthresh;
        toneCurve.autoexp = toneCurve.autoexp && p.toneCurve.autoexp == other.toneCurve.autoexp;
        toneCurve.clip = toneCurve.clip && p.toneCurve.clip == other.toneCurve.clip;
        toneCurve.expcomp = toneCurve.expcomp && p.toneCurve.expcomp == other.toneCurve.expcomp;
        toneCurve.hrenabled = toneCurve.hrenabled && p.toneCurve.hrenabled == other.toneCurve.hrenabled;
        toneCurve.method = toneCurve.method && p.toneCurve.method == other.toneCurve.method;
        toneCurve.histmatching = toneCurve.histmatching && p.toneCurve.histmatching == other.toneCurve.histmatching;
        toneCurve.fromHistMatching = toneCurve.fromHistMatching && p.toneCurve.fromHistMatching == other.toneCurve.fromHistMatching;
        toneCurve.clampOOG = toneCurve.clampOOG && p.toneCurve.clampOOG == other.toneCurve.clampOOG;
        retinex.cdcurve = retinex.cdcurve && p.retinex.cdcurve == other.retinex.cdcurve;
        retinex.mapcurve = retinex.mapcurve && p.retinex.mapcurve == other.retinex.mapcurve;
        retinex.cdHcurve = retinex.cdHcurve && p.retinex.cdHcurve == other.retinex.cdHcurve;
        retinex.lhcurve = retinex.lhcurve && p.retinex.lhcurve == other.retinex.lhcurve;
        retinex.transmissionCurve = retinex.transmissionCurve && p.retinex.transmissionCurve == other.retinex.transmissionCurve;
        retinex.gaintransmissionCurve = retinex.gaintransmissionCurve && p.retinex.gaintransmissionCurve == other.retinex.gaintransmissionCurve;
        retinex.complexmethod = retinex.complexmethod && p.retinex.complexmethod == other.retinex.complexmethod;
        retinex.retinexMethod = retinex.retinexMethod && p.retinex.retinexMethod == other.retinex.retinexMethod;
        retinex.mapMethod = retinex.mapMethod && p.retinex.mapMethod == other.retinex.mapMethod;
        retinex.viewMethod = retinex.viewMethod && p.retinex.viewMethod == other.retinex.viewMethod;
        retinex.retinexcolorspace = retinex.retinexcolorspace && p.retinex.retinexcolorspace == other.retinex.retinexcolorspace;
        retinex.gammaretinex = retinex.gammaretinex && p.retinex.gammaretinex == other.retinex.gammaretinex;
        retinex.str = retinex.str && p.retinex.str == other.retinex.str;
        retinex.scal = retinex.scal && p.retinex.scal == other.retinex.scal;
        retinex.iter = retinex.iter && p.retinex.iter == other.retinex.iter;
        retinex.grad = retinex.grad && p.retinex.grad == other.retinex.grad;
        retinex.grads = retinex.grads && p.retinex.grads == other.retinex.grads;
        retinex.gam = retinex.gam && p.retinex.gam == other.retinex.gam;
        retinex.slope = retinex.slope && p.retinex.slope == other.retinex.slope;
        retinex.neigh = retinex.neigh && p.retinex.neigh == other.retinex.neigh;
        retinex.offs = retinex.offs && p.retinex.offs == other.retinex.offs;
        retinex.vart = retinex.vart && p.retinex.vart == other.retinex.vart;
        retinex.limd = retinex.limd && p.retinex.limd == other.retinex.limd;
        retinex.highl = retinex.highl && p.retinex.highl == other.retinex.highl;
        retinex.skal = retinex.skal && p.retinex.skal == other.retinex.skal;
        retinex.medianmap = retinex.medianmap && p.retinex.medianmap == other.retinex.medianmap;
        retinex.highlights = retinex.highlights && p.retinex.highlights == other.retinex.highlights;
        retinex.htonalwidth = retinex.htonalwidth && p.retinex.htonalwidth == other.retinex.htonalwidth;
        retinex.shadows = retinex.shadows && p.retinex.shadows == other.retinex.shadows;
        retinex.stonalwidth = retinex.stonalwidth && p.retinex.stonalwidth == other.retinex.stonalwidth;
        retinex.radius = retinex.radius && p.retinex.radius == other.retinex.radius;

        retinex.enabled = retinex.enabled && p.retinex.enabled == other.retinex.enabled;
        labCurve.enabled = labCurve.enabled && p.labCurve.enabled == other.labCurve.enabled;
        labCurve.lcurve = labCurve.lcurve && p.labCurve.lcurve == other.labCurve.lcurve;
        labCurve.acurve = labCurve.acurve && p.labCurve.acurve == other.labCurve.acurve;
        labCurve.bcurve = labCurve.bcurve && p.labCurve.bcurve == other.labCurve.bcurve;
        labCurve.cccurve = labCurve.cccurve && p.labCurve.cccurve == other.labCurve.cccurve;
        labCurve.chcurve = labCurve.chcurve && p.labCurve.chcurve == other.labCurve.chcurve;
        labCurve.lhcurve = labCurve.lhcurve && p.labCurve.lhcurve == other.labCurve.lhcurve;
        labCurve.hhcurve = labCurve.hhcurve && p.labCurve.hhcurve == other.labCurve.hhcurve;
        labCurve.lccurve = labCurve.lccurve && p.labCurve.lccurve == other.labCurve.lccurve;
        labCurve.clcurve = labCurve.clcurve && p.labCurve.clcurve == other.labCurve.clcurve;
        labCurve.brightness = labCurve.brightness && p.labCurve.brightness == other.labCurve.brightness;
        labCurve.contrast = labCurve.contrast && p.labCurve.contrast == other.labCurve.contrast;
        labCurve.chromaticity = labCurve.chromaticity && p.labCurve.chromaticity == other.labCurve.chromaticity;
        labCurve.gamutmunselmethod = labCurve.gamutmunselmethod && p.labCurve.gamutmunselmethod == other.labCurve.gamutmunselmethod;
        labCurve.rstprotection = labCurve.rstprotection && p.labCurve.rstprotection == other.labCurve.rstprotection;
        labCurve.lcredsk = labCurve.lcredsk && p.labCurve.lcredsk == other.labCurve.lcredsk;

        localContrast.enabled = localContrast.enabled && p.localContrast.enabled == other.localContrast.enabled;
        localContrast.radius = localContrast.radius && p.localContrast.radius == other.localContrast.radius;
        localContrast.amount = localContrast.amount && p.localContrast.amount == other.localContrast.amount;
        localContrast.darkness = localContrast.darkness && p.localContrast.darkness == other.localContrast.darkness;
        localContrast.lightness = localContrast.lightness && p.localContrast.lightness == other.localContrast.lightness;

        rgbCurves.enabled = rgbCurves.enabled && p.rgbCurves.enabled == other.rgbCurves.enabled;
        rgbCurves.lumamode = rgbCurves.lumamode && p.rgbCurves.lumamode == other.rgbCurves.lumamode;
        rgbCurves.rcurve = rgbCurves.rcurve && p.rgbCurves.rcurve == other.rgbCurves.rcurve;
        rgbCurves.gcurve = rgbCurves.gcurve && p.rgbCurves.gcurve == other.rgbCurves.gcurve;
        rgbCurves.bcurve = rgbCurves.bcurve && p.rgbCurves.bcurve == other.rgbCurves.bcurve;
        colorToning.enabled = colorToning.enabled && p.colorToning.enabled == other.colorToning.enabled;
        colorToning.twocolor = colorToning.twocolor && p.colorToning.twocolor == other.colorToning.twocolor;
        colorToning.opacityCurve = colorToning.opacityCurve && p.colorToning.opacityCurve == other.colorToning.opacityCurve;
        colorToning.colorCurve = colorToning.colorCurve && p.colorToning.colorCurve == other.colorToning.colorCurve;
        colorToning.autosat = colorToning.autosat && p.colorToning.autosat == other.colorToning.autosat;
        colorToning.satprotectionthreshold = colorToning.satprotectionthreshold && p.colorToning.satProtectionThreshold == other.colorToning.satProtectionThreshold;
        colorToning.saturatedopacity = colorToning.saturatedopacity && p.colorToning.saturatedOpacity == other.colorToning.saturatedOpacity;
        colorToning.strength = colorToning.strength && p.colorToning.strength == other.colorToning.strength;
        colorToning.shadowsColSat = colorToning.shadowsColSat && p.colorToning.shadowsColSat == other.colorToning.shadowsColSat;
        colorToning.hlColSat = colorToning.hlColSat && p.colorToning.hlColSat == other.colorToning.hlColSat;
        colorToning.balance = colorToning.balance && p.colorToning.balance == other.colorToning.balance;
        colorToning.clcurve = colorToning.clcurve && p.colorToning.clcurve == other.colorToning.clcurve;
        colorToning.cl2curve = colorToning.cl2curve && p.colorToning.cl2curve == other.colorToning.cl2curve;
        colorToning.method = colorToning.method && p.colorToning.method == other.colorToning.method;
        colorToning.redlow = colorToning.redlow && p.colorToning.redlow == other.colorToning.redlow;
        colorToning.greenlow = colorToning.greenlow && p.colorToning.greenlow == other.colorToning.greenlow;
        colorToning.bluelow = colorToning.bluelow && p.colorToning.bluelow == other.colorToning.bluelow;
        colorToning.satlow = colorToning.satlow && p.colorToning.satlow == other.colorToning.satlow;
        colorToning.sathigh = colorToning.sathigh && p.colorToning.sathigh == other.colorToning.sathigh;
        colorToning.redmed = colorToning.redmed && p.colorToning.redmed == other.colorToning.redmed;
        colorToning.greenmed = colorToning.greenmed && p.colorToning.greenmed == other.colorToning.greenmed;
        colorToning.bluemed = colorToning.bluemed && p.colorToning.bluemed == other.colorToning.bluemed;
        colorToning.redhigh = colorToning.redhigh && p.colorToning.redhigh == other.colorToning.redhigh;
        colorToning.greenhigh = colorToning.greenhigh && p.colorToning.greenhigh == other.colorToning.greenhigh;
        colorToning.bluehigh = colorToning.bluehigh && p.colorToning.bluehigh == other.colorToning.bluehigh;
        colorToning.lumamode = colorToning.lumamode && p.colorToning.lumamode == other.colorToning.lumamode;
        colorToning.labgridALow = colorToning.labgridALow && p.colorToning.labgridALow == other.colorToning.labgridALow;
        colorToning.labgridBLow = colorToning.labgridBLow && p.colorToning.labgridBLow == other.colorToning.labgridBLow;
        colorToning.labgridAHigh = colorToning.labgridAHigh && p.colorToning.labgridAHigh == other.colorToning.labgridAHigh;
        colorToning.labgridBHigh = colorToning.labgridBHigh && p.colorToning.labgridBHigh == other.colorToning.labgridBHigh;
        colorToning.labregions = colorToning.labregions && p.colorToning.labregions == other.colorToning.labregions;
        colorToning.labregionsShowMask = colorToning.labregionsShowMask && p.colorToning.labregionsShowMask == other.colorToning.labregionsShowMask;
        sharpenEdge.enabled = sharpenEdge.enabled && p.sharpenEdge.enabled == other.sharpenEdge.enabled;
        sharpenEdge.passes = sharpenEdge.passes && p.sharpenEdge.passes == other.sharpenEdge.passes;
        sharpenEdge.amount = sharpenEdge.amount && p.sharpenEdge.amount == other.sharpenEdge.amount;
        sharpenEdge.threechannels = sharpenEdge.threechannels && p.sharpenEdge.threechannels == other.sharpenEdge.threechannels;
        sharpenMicro.enabled = sharpenMicro.enabled && p.sharpenMicro.enabled == other.sharpenMicro.enabled;
        sharpenMicro.matrix = sharpenMicro.matrix && p.sharpenMicro.matrix == other.sharpenMicro.matrix;
        sharpenMicro.amount = sharpenMicro.amount && p.sharpenMicro.amount == other.sharpenMicro.amount;
        sharpenMicro.contrast = sharpenMicro.contrast && p.sharpenMicro.contrast == other.sharpenMicro.contrast;
        sharpenMicro.uniformity = sharpenMicro.uniformity && p.sharpenMicro.uniformity == other.sharpenMicro.uniformity;
        sharpening.enabled = sharpening.enabled && p.sharpening.enabled == other.sharpening.enabled;
        sharpening.contrast = sharpening.contrast && p.sharpening.contrast == other.sharpening.contrast;
        sharpening.radius = sharpening.radius && p.sharpening.radius == other.sharpening.radius;
        sharpening.blurradius = sharpening.blurradius && p.sharpening.blurradius == other.sharpening.blurradius;
        sharpening.gamma = sharpening.gamma && p.sharpening.gamma == other.sharpening.gamma;
        sharpening.amount = sharpening.amount && p.sharpening.amount == other.sharpening.amount;
        sharpening.threshold = sharpening.threshold && p.sharpening.threshold == other.sharpening.threshold;
        sharpening.edgesonly = sharpening.edgesonly && p.sharpening.edgesonly == other.sharpening.edgesonly;
        sharpening.edges_radius = sharpening.edges_radius && p.sharpening.edges_radius == other.sharpening.edges_radius;
        sharpening.edges_tolerance = sharpening.edges_tolerance && p.sharpening.edges_tolerance == other.sharpening.edges_tolerance;
        sharpening.halocontrol = sharpening.halocontrol && p.sharpening.halocontrol == other.sharpening.halocontrol;
        sharpening.halocontrol_amount = sharpening.halocontrol_amount && p.sharpening.halocontrol_amount == other.sharpening.halocontrol_amount;
        sharpening.method = sharpening.method && p.sharpening.method == other.sharpening.method;
        sharpening.deconvamount = sharpening.deconvamount && p.sharpening.deconvamount == other.sharpening.deconvamount;
        sharpening.deconvradius = sharpening.deconvradius && p.sharpening.deconvradius == other.sharpening.deconvradius;
        sharpening.deconviter = sharpening.deconviter && p.sharpening.deconviter == other.sharpening.deconviter;
        sharpening.deconvdamping = sharpening.deconvdamping && p.sharpening.deconvdamping == other.sharpening.deconvdamping;
        pdsharpening.enabled = pdsharpening.enabled && p.pdsharpening.enabled == other.pdsharpening.enabled;
        pdsharpening.contrast = pdsharpening.contrast && p.pdsharpening.contrast == other.pdsharpening.contrast;
        pdsharpening.noisecap = pdsharpening.noisecap && p.pdsharpening.noisecap == other.pdsharpening.noisecap;
        pdsharpening.noisecapafter = pdsharpening.noisecapafter && p.pdsharpening.noisecapafter == other.pdsharpening.noisecapafter;
        pdsharpening.autoContrast = pdsharpening.autoContrast && p.pdsharpening.autoContrast == other.pdsharpening.autoContrast;
        pdsharpening.autoRadius = pdsharpening.autoRadius && p.pdsharpening.autoRadius == other.pdsharpening.autoRadius;
        pdsharpening.deconvradius = pdsharpening.deconvradius && p.pdsharpening.deconvradius == other.pdsharpening.deconvradius;
        pdsharpening.deconvradiusOffset = pdsharpening.deconvradiusOffset && p.pdsharpening.deconvradiusOffset == other.pdsharpening.deconvradiusOffset;
        pdsharpening.deconviter = pdsharpening.deconviter && p.pdsharpening.deconviter == other.pdsharpening.deconviter;
        pdsharpening.deconvitercheck = pdsharpening.deconvitercheck && p.pdsharpening.deconvitercheck == other.pdsharpening.deconvitercheck;
        pdsharpening.showcap = pdsharpening.showcap && p.pdsharpening.showcap == other.pdsharpening.showcap;
        pdsharpening.noisecaptype = pdsharpening.noisecaptype && p.pdsharpening.noisecaptype == other.pdsharpening.noisecaptype;
        prsharpening.enabled = prsharpening.enabled && p.prsharpening.enabled == other.prsharpening.enabled;
        prsharpening.contrast = prsharpening.contrast && p.prsharpening.contrast == other.prsharpening.contrast;
        prsharpening.radius = prsharpening.radius && p.prsharpening.radius == other.prsharpening.radius;
        prsharpening.amount = prsharpening.amount && p.prsharpening.amount == other.prsharpening.amount;
        prsharpening.threshold = prsharpening.threshold && p.prsharpening.threshold == other.prsharpening.threshold;
        prsharpening.edgesonly = prsharpening.edgesonly && p.prsharpening.edgesonly == other.prsharpening.edgesonly;
        prsharpening.edges_radius = prsharpening.edges_radius && p.prsharpening.edges_radius == other.prsharpening.edges_radius;
        prsharpening.edges_tolerance = prsharpening.edges_tolerance && p.prsharpening.edges_tolerance == other.prsharpening.edges_tolerance;
        prsharpening.halocontrol = prsharpening.halocontrol && p.prsharpening.halocontrol == other.prsharpening.halocontrol;
        prsharpening.halocontrol_amount = prsharpening.halocontrol_amount && p.prsharpening.halocontrol_amount == other.prsharpening.halocontrol_amount;
        prsharpening.method = prsharpening.method && p.prsharpening.method == other.prsharpening.method;
        prsharpening.deconvamount = prsharpening.deconvamount && p.prsharpening.deconvamount == other.prsharpening.deconvamount;
        prsharpening.deconvradius = prsharpening.deconvradius && p.prsharpening.deconvradius == other.prsharpening.deconvradius;
        prsharpening.deconviter = prsharpening.deconviter && p.prsharpening.deconviter == other.prsharpening.deconviter;
        prsharpening.deconvdamping = prsharpening.deconvdamping && p.prsharpening.deconvdamping == other.prsharpening.deconvdamping;
        vibrance.enabled = vibrance.enabled && p.vibrance.enabled == other.vibrance.enabled;
        vibrance.pastels = vibrance.pastels && p.vibrance.pastels == other.vibrance.pastels;
        vibrance.saturated = vibrance.saturated && p.vibrance.saturated == other.vibrance.saturated;
        vibrance.psthreshold = vibrance.psthreshold && p.vibrance.psthreshold == other.vibrance.psthreshold;
        vibrance.protectskins = vibrance.protectskins && p.vibrance.protectskins == other.vibrance.protectskins;
        vibrance.avoidcolorshift = vibrance.avoidcolorshift && p.vibrance.avoidcolorshift == other.vibrance.avoidcolorshift;
        vibrance.pastsattog = vibrance.pastsattog && p.vibrance.pastsattog == other.vibrance.pastsattog;
        vibrance.skintonescurve = vibrance.skintonescurve && p.vibrance.skintonescurve == other.vibrance.skintonescurve;
        colorappearance.enabled = colorappearance.enabled && p.colorappearance.enabled == other.colorappearance.enabled;
        colorappearance.degree = colorappearance.degree && p.colorappearance.degree == other.colorappearance.degree;
        colorappearance.autodegree = colorappearance.autodegree && p.colorappearance.autodegree == other.colorappearance.autodegree;
        colorappearance.degreeout = colorappearance.degreeout && p.colorappearance.degreeout == other.colorappearance.degreeout;
        colorappearance.autodegreeout = colorappearance.autodegreeout && p.colorappearance.autodegreeout == other.colorappearance.autodegreeout;
        colorappearance.surround = colorappearance.surround && p.colorappearance.surround == other.colorappearance.surround;
        colorappearance.surrsrc = colorappearance.surrsrc && p.colorappearance.surrsrc == other.colorappearance.surrsrc;
        colorappearance.adapscen = colorappearance.adapscen && p.colorappearance.adapscen == other.colorappearance.adapscen;
        colorappearance.autoadapscen = colorappearance.autoadapscen && p.colorappearance.autoadapscen == other.colorappearance.autoadapscen;
        colorappearance.ybscen = colorappearance.ybscen && p.colorappearance.ybscen == other.colorappearance.ybscen;
        colorappearance.autoybscen = colorappearance.autoybscen && p.colorappearance.autoybscen == other.colorappearance.autoybscen;
        colorappearance.adaplum = colorappearance.adaplum && p.colorappearance.adaplum == other.colorappearance.adaplum;
        colorappearance.badpixsl = colorappearance.badpixsl && p.colorappearance.badpixsl == other.colorappearance.badpixsl;
        colorappearance.wbmodel = colorappearance.wbmodel && p.colorappearance.wbmodel == other.colorappearance.wbmodel;
        colorappearance.illum = colorappearance.illum && p.colorappearance.illum == other.colorappearance.illum;
        colorappearance.algo = colorappearance.algo && p.colorappearance.algo == other.colorappearance.algo;
        colorappearance.jlight = colorappearance.jlight && p.colorappearance.jlight == other.colorappearance.jlight;
        colorappearance.qbright = colorappearance.qbright && p.colorappearance.qbright == other.colorappearance.qbright;
        colorappearance.chroma = colorappearance.chroma && p.colorappearance.chroma == other.colorappearance.chroma;
        colorappearance.schroma = colorappearance.schroma && p.colorappearance.schroma == other.colorappearance.schroma;
        colorappearance.schromared = colorappearance.schromared && p.colorappearance.schromared == other.colorappearance.schromared;
        colorappearance.schromagreen = colorappearance.schromagreen && p.colorappearance.schromagreen == other.colorappearance.schromagreen;
        colorappearance.schromablue = colorappearance.schromablue && p.colorappearance.schromablue == other.colorappearance.schromablue;
        colorappearance.mchroma = colorappearance.mchroma && p.colorappearance.mchroma == other.colorappearance.mchroma;
        colorappearance.rstprotection = colorappearance.rstprotection && p.colorappearance.rstprotection == other.colorappearance.rstprotection;
        colorappearance.contrast = colorappearance.contrast && p.colorappearance.contrast == other.colorappearance.contrast;
        colorappearance.qcontrast = colorappearance.qcontrast && p.colorappearance.qcontrast == other.colorappearance.qcontrast;
        colorappearance.colorh = colorappearance.colorh && p.colorappearance.colorh == other.colorappearance.colorh;
        colorappearance.colorhred = colorappearance.colorhred && p.colorappearance.colorhred == other.colorappearance.colorhred;
        colorappearance.colorhgreen = colorappearance.colorhgreen && p.colorappearance.colorhgreen == other.colorappearance.colorhgreen;
        colorappearance.colorhblue = colorappearance.colorhblue && p.colorappearance.colorhblue == other.colorappearance.colorhblue;
        colorappearance.surrsource = colorappearance.surrsource && p.colorappearance.surrsource == other.colorappearance.surrsource;
        colorappearance.gamut = colorappearance.gamut && p.colorappearance.gamut == other.colorappearance.gamut;
//       colorappearance.badpix = colorappearance.badpix && p.colorappearance.badpix == other.colorappearance.badpix;
        colorappearance.datacie = colorappearance.datacie && p.colorappearance.datacie == other.colorappearance.datacie;
        colorappearance.tonecie = colorappearance.tonecie && p.colorappearance.tonecie == other.colorappearance.tonecie;
        //     colorappearance.sharpcie = colorappearance.sharpcie && p.colorappearance.sharpcie == other.colorappearance.sharpcie;
        colorappearance.curve = colorappearance.curve && p.colorappearance.curve == other.colorappearance.curve;
        colorappearance.curvered = colorappearance.curvered && p.colorappearance.curvered == other.colorappearance.curvered;
        colorappearance.curvegreen = colorappearance.curvegreen && p.colorappearance.curvegreen == other.colorappearance.curvegreen;
        colorappearance.curveblue = colorappearance.curveblue && p.colorappearance.curveblue == other.colorappearance.curveblue;
        colorappearance.curve3 = colorappearance.curve3 && p.colorappearance.curve3 == other.colorappearance.curve3;
        colorappearance.curve2 = colorappearance.curve2 && p.colorappearance.curve2 == other.colorappearance.curve2;
        colorappearance.curveMode = colorappearance.curveMode && p.colorappearance.curveMode == other.colorappearance.curveMode;
        colorappearance.curveMode2 = colorappearance.curveMode2 && p.colorappearance.curveMode2 == other.colorappearance.curveMode2;
        colorappearance.curveMode3 = colorappearance.curveMode3 && p.colorappearance.curveMode3 == other.colorappearance.curveMode3;
        colorappearance.complexmethod = colorappearance.complexmethod && p.colorappearance.complexmethod == other.colorappearance.complexmethod;
        colorappearance.modelmethod = colorappearance.modelmethod && p.colorappearance.modelmethod == other.colorappearance.modelmethod;
        colorappearance.catmethod = colorappearance.catmethod && p.colorappearance.catmethod == other.colorappearance.catmethod;
        colorappearance.tempout = colorappearance.tempout && p.colorappearance.tempout == other.colorappearance.tempout;
        colorappearance.autotempout = colorappearance.autotempout && p.colorappearance.autotempout == other.colorappearance.autotempout;
        colorappearance.greenout = colorappearance.greenout && p.colorappearance.greenout == other.colorappearance.greenout;
        colorappearance.ybout = colorappearance.ybout && p.colorappearance.ybout == other.colorappearance.ybout;
        colorappearance.tempsc = colorappearance.tempsc && p.colorappearance.tempsc == other.colorappearance.tempsc;
        colorappearance.greensc = colorappearance.greensc && p.colorappearance.greensc == other.colorappearance.greensc;

        //colorBoost.amount = colorBoost.amount && p.colorBoost.amount == other.colorBoost.amount;
        //colorBoost.avoidclip = colorBoost.avoidclip && p.colorBoost.avoidclip == other.colorBoost.avoidclip;
        //colorBoost.enable_saturationlimiter = colorBoost.enable_saturationlimiter && p.colorBoost.enable_saturationlimiter == other.colorBoost.enable_saturationlimiter;
        //colorBoost.saturationlimit = colorBoost.saturationlimit && p.colorBoost.saturationlimit == other.colorBoost.saturationlimit;
        wb.enabled = wb.enabled && p.wb.enabled == other.wb.enabled;
        wb.method = wb.method && p.wb.method == other.wb.method;
        wb.green = wb.green && p.wb.green == other.wb.green;
        wb.equal = wb.equal && p.wb.equal == other.wb.equal;
        wb.temperature = wb.temperature && p.wb.temperature == other.wb.temperature;
        wb.tempBias = wb.tempBias && p.wb.tempBias == other.wb.tempBias;
        wb.observer = wb.observer && p.wb.observer == other.wb.observer;
        wb.itcwb_green = wb.itcwb_green && p.wb.itcwb_green == other.wb.itcwb_green;
        wb.itcwb_rgreen = wb.itcwb_rgreen && p.wb.itcwb_rgreen == other.wb.itcwb_rgreen;
        wb.itcwb_nopurple = wb.itcwb_nopurple && p.wb.itcwb_nopurple == other.wb.itcwb_nopurple;
        wb.itcwb_alg = wb.itcwb_alg && p.wb.itcwb_alg == other.wb.itcwb_alg;
        wb.itcwb_prim = wb.itcwb_prim && p.wb.itcwb_prim == other.wb.itcwb_prim;
        wb.itcwb_sampling = wb.itcwb_sampling && p.wb.itcwb_sampling == other.wb.itcwb_sampling;
        wb.compat_version = wb.compat_version && p.wb.compat_version == other.wb.compat_version;
        //colorShift.a = colorShift.a && p.colorShift.a == other.colorShift.a;
        //colorShift.b = colorShift.b && p.colorShift.b == other.colorShift.b;
        //lumaDenoise.enabled = lumaDenoise.enabled && p.lumaDenoise.enabled == other.lumaDenoise.enabled;
        //lumaDenoise.radius = lumaDenoise.radius && p.lumaDenoise.radius == other.lumaDenoise.radius;
        //lumaDenoise.edgetolerance = lumaDenoise.edgetolerance && p.lumaDenoise.edgetolerance == other.lumaDenoise.edgetolerance;
        //colorDenoise.enabled = colorDenoise.enabled && p.colorDenoise.enabled == other.colorDenoise.enabled;
        //colorDenoise.amount = colorDenoise.amount && p.colorDenoise.amount == other.colorDenoise.amount;
        defringe.enabled = defringe.enabled && p.defringe.enabled == other.defringe.enabled;
        defringe.radius = defringe.radius && p.defringe.radius == other.defringe.radius;
        defringe.threshold = defringe.threshold && p.defringe.threshold == other.defringe.threshold;
        defringe.huecurve = defringe.huecurve && p.defringe.huecurve == other.defringe.huecurve;

        impulseDenoise.enabled = impulseDenoise.enabled && p.impulseDenoise.enabled == other.impulseDenoise.enabled;
        impulseDenoise.thresh = impulseDenoise.thresh && p.impulseDenoise.thresh == other.impulseDenoise.thresh;

        dirpyrDenoise.enabled = dirpyrDenoise.enabled && p.dirpyrDenoise.enabled == other.dirpyrDenoise.enabled;
        dirpyrDenoise.enhance = dirpyrDenoise.enhance && p.dirpyrDenoise.enhance == other.dirpyrDenoise.enhance;
        dirpyrDenoise.median = dirpyrDenoise.median && p.dirpyrDenoise.median == other.dirpyrDenoise.median;
//       dirpyrDenoise.perform = dirpyrDenoise.perform && p.dirpyrDenoise.perform == other.dirpyrDenoise.perform;
        dirpyrDenoise.luma = dirpyrDenoise.luma && p.dirpyrDenoise.luma == other.dirpyrDenoise.luma;
        dirpyrDenoise.lcurve = dirpyrDenoise.lcurve && p.dirpyrDenoise.lcurve == other.dirpyrDenoise.lcurve;
        dirpyrDenoise.cccurve = dirpyrDenoise.cccurve && p.dirpyrDenoise.cccurve == other.dirpyrDenoise.cccurve;
        dirpyrDenoise.Ldetail = dirpyrDenoise.Ldetail && p.dirpyrDenoise.Ldetail == other.dirpyrDenoise.Ldetail;
        dirpyrDenoise.chroma = dirpyrDenoise.chroma && p.dirpyrDenoise.chroma == other.dirpyrDenoise.chroma;
        dirpyrDenoise.redchro = dirpyrDenoise.redchro && p.dirpyrDenoise.redchro == other.dirpyrDenoise.redchro;
        dirpyrDenoise.bluechro = dirpyrDenoise.bluechro && p.dirpyrDenoise.bluechro == other.dirpyrDenoise.bluechro;
        dirpyrDenoise.gain = dirpyrDenoise.gain && p.dirpyrDenoise.autoGain == other.dirpyrDenoise.autoGain;
        dirpyrDenoise.gamma = dirpyrDenoise.gamma && p.dirpyrDenoise.gamma == other.dirpyrDenoise.gamma;
        dirpyrDenoise.passes = dirpyrDenoise.passes && p.dirpyrDenoise.passes == other.dirpyrDenoise.passes;
        dirpyrDenoise.dmethod = dirpyrDenoise.dmethod && p.dirpyrDenoise.dmethod == other.dirpyrDenoise.dmethod;
        dirpyrDenoise.Lmethod = dirpyrDenoise.Lmethod && p.dirpyrDenoise.Lmethod == other.dirpyrDenoise.Lmethod;
        dirpyrDenoise.Cmethod = dirpyrDenoise.Cmethod && p.dirpyrDenoise.Cmethod == other.dirpyrDenoise.Cmethod;
        dirpyrDenoise.C2method = dirpyrDenoise.C2method && p.dirpyrDenoise.C2method == other.dirpyrDenoise.C2method;
        dirpyrDenoise.smethod = dirpyrDenoise.smethod && p.dirpyrDenoise.smethod == other.dirpyrDenoise.smethod;
        dirpyrDenoise.medmethod = dirpyrDenoise.medmethod && p.dirpyrDenoise.medmethod == other.dirpyrDenoise.medmethod;
        dirpyrDenoise.methodmed = dirpyrDenoise.methodmed && p.dirpyrDenoise.methodmed == other.dirpyrDenoise.methodmed;
        dirpyrDenoise.rgbmethod = dirpyrDenoise.rgbmethod && p.dirpyrDenoise.rgbmethod == other.dirpyrDenoise.rgbmethod;

        epd.enabled = epd.enabled && p.epd.enabled == other.epd.enabled;
        epd.strength = epd.strength && p.epd.strength == other.epd.strength;
        epd.gamma = epd.gamma && p.epd.gamma == other.epd.gamma;
        epd.edgeStopping = epd.edgeStopping && p.epd.edgeStopping == other.epd.edgeStopping;
        epd.scale = epd.scale && p.epd.scale == other.epd.scale;
        epd.reweightingIterates = epd.reweightingIterates && p.epd.reweightingIterates == other.epd.reweightingIterates;

        fattal.enabled = fattal.enabled && p.fattal.enabled == other.fattal.enabled;
        fattal.threshold = fattal.threshold && p.fattal.threshold == other.fattal.threshold;
        fattal.amount = fattal.amount && p.fattal.amount == other.fattal.amount;
        fattal.anchor = fattal.anchor && p.fattal.anchor == other.fattal.anchor;

        sh.enabled = sh.enabled && p.sh.enabled == other.sh.enabled;
        sh.highlights = sh.highlights && p.sh.highlights == other.sh.highlights;
        sh.htonalwidth = sh.htonalwidth && p.sh.htonalwidth == other.sh.htonalwidth;
        sh.shadows = sh.shadows && p.sh.shadows == other.sh.shadows;
        sh.stonalwidth = sh.stonalwidth && p.sh.stonalwidth == other.sh.stonalwidth;
        sh.radius = sh.radius && p.sh.radius == other.sh.radius;
        sh.lab = sh.lab && p.sh.lab == other.sh.lab;

        cg.enabled = cg.enabled && p.cg.enabled == other.cg.enabled;
        cg.th_c = cg.th_c && p.cg.th_c == other.cg.th_c;   
        cg.th_m = cg.th_m && p.cg.th_m == other.cg.th_m;
        cg.th_y = cg.th_y && p.cg.th_y == other.cg.th_y;
        cg.d_c = cg.d_c && p.cg.d_c == other.cg.d_c;
        cg.autodc = cg.autodc && p.cg.autodc == other.cg.autodc;
        cg.d_m = cg.d_m && p.cg.d_m == other.cg.d_m;
        cg.autodm = cg.autodm && p.cg.autodm == other.cg.autodm;
        cg.d_y = cg.d_y && p.cg.d_y == other.cg.d_y;
        cg.autody = cg.autody && p.cg.autody == other.cg.autody;
        cg.pwr = cg.pwr && p.cg.pwr == other.cg.pwr;
        cg.colorspace = cg.colorspace && p.cg.colorspace == other.cg.colorspace;
        cg.rolloff = cg.rolloff && p.cg.rolloff == other.cg.rolloff;

        crop.enabled = crop.enabled && p.crop.enabled == other.crop.enabled;
        crop.x = crop.x && p.crop.x == other.crop.x;
        crop.y = crop.y && p.crop.y == other.crop.y;
        crop.w = crop.w && p.crop.w == other.crop.w;
        crop.h = crop.h && p.crop.h == other.crop.h;
        crop.fixratio = crop.fixratio && p.crop.fixratio == other.crop.fixratio;
        crop.ratio = crop.ratio && p.crop.ratio == other.crop.ratio;
        crop.orientation = crop.orientation && p.crop.orientation == other.crop.orientation;
        ::initFrom(cropGuide, p, other);
        toneEqualizer.enabled = toneEqualizer.enabled && p.toneEqualizer.enabled == other.toneEqualizer.enabled;
        for (size_t i = 0; i < toneEqualizer.bands.size(); ++i) {
            toneEqualizer.bands[i] = toneEqualizer.bands[i] && p.toneEqualizer.bands[i] == other.toneEqualizer.bands[i];
        }
        toneEqualizer.regularization = toneEqualizer.regularization && p.toneEqualizer.regularization == other.toneEqualizer.regularization;
        toneEqualizer.show_colormap = toneEqualizer.show_colormap && p.toneEqualizer.show_colormap == other.toneEqualizer.show_colormap;
        toneEqualizer.pivot = toneEqualizer.pivot && p.toneEqualizer.pivot == other.toneEqualizer.pivot;
        coarse.rotate = coarse.rotate && p.coarse.rotate == other.coarse.rotate;
        coarse.hflip = coarse.hflip && p.coarse.hflip == other.coarse.hflip;
        coarse.vflip = coarse.vflip && p.coarse.vflip == other.coarse.vflip;
        commonTrans.method = commonTrans.method && p.commonTrans.method == other.commonTrans.method;
        commonTrans.scale = commonTrans.scale && p.commonTrans.scale == other.commonTrans.scale;
        commonTrans.scale_horizontally = commonTrans.scale_horizontally && p.commonTrans.scale_horizontally == other.commonTrans.scale_horizontally;
        commonTrans.scale_vertically = commonTrans.scale_vertically && p.commonTrans.scale_vertically == other.commonTrans.scale_vertically;
        commonTrans.autofill = commonTrans.autofill && p.commonTrans.autofill == other.commonTrans.autofill;
        rotate.degree = rotate.degree && p.rotate.degree == other.rotate.degree;
        distortion.amount = distortion.amount && p.distortion.amount == other.distortion.amount;
        distortion.defish = distortion.defish && p.distortion.defish == other.distortion.defish;
        distortion.focal_length = distortion.focal_length && p.distortion.focal_length == other.distortion.focal_length;
        lensProf.lcMode = lensProf.lcMode && p.lensProf.lcMode == other.lensProf.lcMode;
        lensProf.lcpFile = lensProf.lcpFile && p.lensProf.lcpFile == other.lensProf.lcpFile;
        lensProf.useDist = lensProf.useDist && p.lensProf.useDist == other.lensProf.useDist;
        lensProf.useVign = lensProf.useVign && p.lensProf.useVign == other.lensProf.useVign;
        lensProf.useCA = lensProf.useCA && p.lensProf.useCA == other.lensProf.useCA;
        lensProf.useLensfun = lensProf.useLensfun && p.lensProf.useLensfun() == other.lensProf.useLensfun();
        lensProf.lfAutoMatch = lensProf.lfAutoMatch && p.lensProf.lfAutoMatch() == other.lensProf.lfAutoMatch();
        lensProf.lfCameraMake = lensProf.lfCameraMake && p.lensProf.lfCameraMake == other.lensProf.lfCameraMake;
        lensProf.lfCameraModel = lensProf.lfCameraModel && p.lensProf.lfCameraModel == other.lensProf.lfCameraModel;
        lensProf.lfLens = lensProf.lfLens && p.lensProf.lfLens == other.lensProf.lfLens;
        perspective.method = perspective.method && p.perspective.method == other.perspective.method;
        perspective.horizontal = perspective.horizontal && p.perspective.horizontal == other.perspective.horizontal;
        perspective.vertical = perspective.vertical && p.perspective.vertical == other.perspective.vertical;
        perspective.camera_crop_factor = perspective.camera_crop_factor && p.perspective.camera_crop_factor == other.perspective.camera_crop_factor;
        perspective.camera_focal_length = perspective.camera_focal_length && p.perspective.camera_focal_length == other.perspective.camera_focal_length;
        perspective.camera_pitch = perspective.camera_pitch && p.perspective.camera_pitch == other.perspective.camera_pitch;
        perspective.camera_roll = perspective.camera_roll && p.perspective.camera_roll == other.perspective.camera_roll;
        perspective.camera_shift_horiz = perspective.camera_shift_horiz && p.perspective.camera_shift_horiz == other.perspective.camera_shift_horiz;
        perspective.camera_shift_vert = perspective.camera_shift_vert && p.perspective.camera_shift_vert == other.perspective.camera_shift_vert;
        perspective.projection_pitch = perspective.projection_pitch && p.perspective.projection_pitch == other.perspective.projection_pitch;
        perspective.camera_yaw = perspective.camera_yaw && p.perspective.camera_yaw == other.perspective.camera_yaw;
        perspective.projection_rotate = perspective.projection_rotate && p.perspective.projection_rotate == other.perspective.projection_rotate;
        perspective.projection_shift_horiz = perspective.projection_shift_horiz && p.perspective.projection_shift_horiz == other.perspective.projection_shift_horiz;
        perspective.projection_shift_vert = perspective.projection_shift_vert && p.perspective.projection_shift_vert == other.perspective.projection_shift_vert;
        perspective.projection_yaw = perspective.projection_yaw && p.perspective.projection_yaw == other.perspective.projection_yaw;
        perspective.control_lines = perspective.control_lines && p.perspective.control_line_values == other.perspective.control_line_values && p.perspective.control_line_types == other.perspective.control_line_types;
        gradient.enabled = gradient.enabled && p.gradient.enabled == other.gradient.enabled;
        gradient.degree = gradient.degree && p.gradient.degree == other.gradient.degree;
        gradient.feather = gradient.feather && p.gradient.feather == other.gradient.feather;
        gradient.strength = gradient.strength && p.gradient.strength == other.gradient.strength;
        gradient.centerX = gradient.centerX && p.gradient.centerX == other.gradient.centerX;
        gradient.centerY = gradient.centerY && p.gradient.centerY == other.gradient.centerY;

        locallab.enabled = locallab.enabled && p.locallab.enabled == other.locallab.enabled;
        isSpotNumberEqual = isSpotNumberEqual && p.locallab.spots.size() == other.locallab.spots.size();
        locallab.selspot = locallab.selspot && p.locallab.selspot == other.locallab.selspot;

        if (isSpotNumberEqual) {
            for (size_t j = 0; j < locallab.spots.size() && j < p.locallab.spots.size() && j < other.locallab.spots.size(); j++) {
                const LocallabParams::LocallabSpot& pSpot = p.locallab.spots.at(j);
                const LocallabParams::LocallabSpot& otherSpot = other.locallab.spots.at(j);
                // Control spot settings
                locallab.spots.at(j).name = locallab.spots.at(j).name && pSpot.name == otherSpot.name;
                locallab.spots.at(j).isvisible = locallab.spots.at(j).isvisible && pSpot.isvisible == otherSpot.isvisible;
                locallab.spots.at(j).prevMethod = locallab.spots.at(j).prevMethod && pSpot.prevMethod == otherSpot.prevMethod;
                locallab.spots.at(j).shape = locallab.spots.at(j).shape && pSpot.shape == otherSpot.shape;
                locallab.spots.at(j).spotMethod = locallab.spots.at(j).spotMethod && pSpot.spotMethod == otherSpot.spotMethod;
                locallab.spots.at(j).wavMethod = locallab.spots.at(j).wavMethod && pSpot.wavMethod == otherSpot.wavMethod;
                locallab.spots.at(j).sensiexclu = locallab.spots.at(j).sensiexclu && pSpot.sensiexclu == otherSpot.sensiexclu;
                locallab.spots.at(j).structexclu = locallab.spots.at(j).structexclu && pSpot.structexclu == otherSpot.structexclu;
                locallab.spots.at(j).struc = locallab.spots.at(j).struc && pSpot.struc == otherSpot.struc;
                locallab.spots.at(j).shapeMethod = locallab.spots.at(j).shapeMethod && pSpot.shapeMethod == otherSpot.shapeMethod;
                locallab.spots.at(j).avoidgamutMethod = locallab.spots.at(j).avoidgamutMethod && pSpot.avoidgamutMethod == otherSpot.avoidgamutMethod;
                locallab.spots.at(j).loc = locallab.spots.at(j).loc && pSpot.loc == otherSpot.loc;
                locallab.spots.at(j).centerX = locallab.spots.at(j).centerX && pSpot.centerX == otherSpot.centerX;
                locallab.spots.at(j).centerY = locallab.spots.at(j).centerY && pSpot.centerY == otherSpot.centerY;
                locallab.spots.at(j).spotangle = locallab.spots.at(j).spotangle && pSpot.spotangle == otherSpot.spotangle;
                locallab.spots.at(j).circrad = locallab.spots.at(j).circrad && pSpot.circrad == otherSpot.circrad;
                locallab.spots.at(j).qualityMethod = locallab.spots.at(j).qualityMethod && pSpot.qualityMethod == otherSpot.qualityMethod;
                locallab.spots.at(j).complexMethod = locallab.spots.at(j).complexMethod && pSpot.complexMethod == otherSpot.complexMethod;
                locallab.spots.at(j).transit = locallab.spots.at(j).transit && pSpot.transit == otherSpot.transit;
                locallab.spots.at(j).feather = locallab.spots.at(j).feather && pSpot.feather == otherSpot.feather;
                locallab.spots.at(j).thresh = locallab.spots.at(j).thresh && pSpot.thresh == otherSpot.thresh;
                locallab.spots.at(j).iter = locallab.spots.at(j).iter && pSpot.iter == otherSpot.iter;
                locallab.spots.at(j).balan = locallab.spots.at(j).balan && pSpot.balan == otherSpot.balan;
                locallab.spots.at(j).balanh = locallab.spots.at(j).balanh && pSpot.balanh == otherSpot.balanh;
                locallab.spots.at(j).colorde = locallab.spots.at(j).colorde && pSpot.colorde == otherSpot.colorde;
                locallab.spots.at(j).colorscope = locallab.spots.at(j).colorscope && pSpot.colorscope == otherSpot.colorscope;
                locallab.spots.at(j).avoidrad = locallab.spots.at(j).avoidrad && pSpot.avoidrad == otherSpot.avoidrad;
                locallab.spots.at(j).transitweak = locallab.spots.at(j).transitweak && pSpot.transitweak == otherSpot.transitweak;
                locallab.spots.at(j).transitgrad = locallab.spots.at(j).transitgrad && pSpot.transitgrad == otherSpot.transitgrad;
                locallab.spots.at(j).hishow = locallab.spots.at(j).hishow && pSpot.hishow == otherSpot.hishow;
                locallab.spots.at(j).activ = locallab.spots.at(j).activ && pSpot.activ == otherSpot.activ;
                locallab.spots.at(j).avoidneg = locallab.spots.at(j).avoidneg && pSpot.avoidneg == otherSpot.avoidneg;
                locallab.spots.at(j).blwh = locallab.spots.at(j).blwh && pSpot.blwh == otherSpot.blwh;
                locallab.spots.at(j).recurs = locallab.spots.at(j).recurs && pSpot.recurs == otherSpot.recurs;
                locallab.spots.at(j).laplac = locallab.spots.at(j).laplac && pSpot.laplac == otherSpot.laplac;
                locallab.spots.at(j).deltae = locallab.spots.at(j).deltae && pSpot.deltae == otherSpot.deltae;
                locallab.spots.at(j).shortc = locallab.spots.at(j).shortc && pSpot.shortc == otherSpot.shortc;
                locallab.spots.at(j).savrest = locallab.spots.at(j).savrest && pSpot.savrest == otherSpot.savrest;
                locallab.spots.at(j).scopemask = locallab.spots.at(j).scopemask && pSpot.scopemask == otherSpot.scopemask;
                locallab.spots.at(j).denoichmask = locallab.spots.at(j).denoichmask && pSpot.denoichmask == otherSpot.denoichmask;
                locallab.spots.at(j).lumask = locallab.spots.at(j).lumask && pSpot.lumask == otherSpot.lumask;
                // Color & Light
                locallab.spots.at(j).visicolor = locallab.spots.at(j).visicolor && pSpot.visicolor == otherSpot.visicolor;
                locallab.spots.at(j).expcolor = locallab.spots.at(j).expcolor && pSpot.expcolor == otherSpot.expcolor;
                locallab.spots.at(j).complexcolor = locallab.spots.at(j).complexcolor && pSpot.complexcolor == otherSpot.complexcolor;
                locallab.spots.at(j).curvactiv = locallab.spots.at(j).curvactiv && pSpot.curvactiv == otherSpot.curvactiv;
                locallab.spots.at(j).reparcol = locallab.spots.at(j).reparcol && pSpot.reparcol == otherSpot.reparcol;
                locallab.spots.at(j).gamc = locallab.spots.at(j).gamc && pSpot.gamc == otherSpot.gamc;
                locallab.spots.at(j).lightness = locallab.spots.at(j).lightness && pSpot.lightness == otherSpot.lightness;
                locallab.spots.at(j).contrast = locallab.spots.at(j).contrast && pSpot.contrast == otherSpot.contrast;
                locallab.spots.at(j).chroma = locallab.spots.at(j).chroma && pSpot.chroma == otherSpot.chroma;
                locallab.spots.at(j).labgridALow = locallab.spots.at(j).labgridALow && pSpot.labgridALow == otherSpot.labgridALow;
                locallab.spots.at(j).labgridBLow = locallab.spots.at(j).labgridBLow && pSpot.labgridBLow == otherSpot.labgridBLow;
                locallab.spots.at(j).labgridAHigh = locallab.spots.at(j).labgridAHigh && pSpot.labgridAHigh == otherSpot.labgridAHigh;
                locallab.spots.at(j).labgridBHigh = locallab.spots.at(j).labgridBHigh && pSpot.labgridBHigh == otherSpot.labgridBHigh;
                locallab.spots.at(j).labgridALowmerg = locallab.spots.at(j).labgridALowmerg && pSpot.labgridALowmerg == otherSpot.labgridALowmerg;
                locallab.spots.at(j).labgridBLowmerg = locallab.spots.at(j).labgridBLowmerg && pSpot.labgridBLowmerg == otherSpot.labgridBLowmerg;
                locallab.spots.at(j).labgridAHighmerg = locallab.spots.at(j).labgridAHighmerg && pSpot.labgridAHighmerg == otherSpot.labgridAHighmerg;
                locallab.spots.at(j).labgridBHighmerg = locallab.spots.at(j).labgridBHighmerg && pSpot.labgridBHighmerg == otherSpot.labgridBHighmerg;
                locallab.spots.at(j).strengthgrid = locallab.spots.at(j).strengthgrid && pSpot.strengthgrid == otherSpot.strengthgrid;
                locallab.spots.at(j).sensi = locallab.spots.at(j).sensi && pSpot.sensi == otherSpot.sensi;
                locallab.spots.at(j).structcol = locallab.spots.at(j).structcol && pSpot.structcol == otherSpot.structcol;
                locallab.spots.at(j).strcol = locallab.spots.at(j).strcol && pSpot.strcol == otherSpot.strcol;
                locallab.spots.at(j).strcolab = locallab.spots.at(j).strcolab && pSpot.strcolab == otherSpot.strcolab;
                locallab.spots.at(j).strcolh = locallab.spots.at(j).strcolh && pSpot.strcolh == otherSpot.strcolh;
                locallab.spots.at(j).angcol = locallab.spots.at(j).angcol && pSpot.angcol == otherSpot.angcol;
                locallab.spots.at(j).feathercol = locallab.spots.at(j).feathercol && pSpot.feathercol == otherSpot.feathercol;
                locallab.spots.at(j).blurcolde = locallab.spots.at(j).blurcolde && pSpot.blurcolde == otherSpot.blurcolde;
                locallab.spots.at(j).blurcol = locallab.spots.at(j).blurcol && pSpot.blurcol == otherSpot.blurcol;
                locallab.spots.at(j).contcol = locallab.spots.at(j).contcol && pSpot.contcol == otherSpot.contcol;
                locallab.spots.at(j).blendmaskcol = locallab.spots.at(j).blendmaskcol && pSpot.blendmaskcol == otherSpot.blendmaskcol;
                locallab.spots.at(j).radmaskcol = locallab.spots.at(j).radmaskcol && pSpot.radmaskcol == otherSpot.radmaskcol;
                locallab.spots.at(j).chromaskcol = locallab.spots.at(j).chromaskcol && pSpot.chromaskcol == otherSpot.chromaskcol;
                locallab.spots.at(j).gammaskcol = locallab.spots.at(j).gammaskcol && pSpot.gammaskcol == otherSpot.gammaskcol;
                locallab.spots.at(j).slomaskcol = locallab.spots.at(j).slomaskcol && pSpot.slomaskcol == otherSpot.slomaskcol;
                locallab.spots.at(j).shadmaskcol = locallab.spots.at(j).shadmaskcol && pSpot.shadmaskcol == otherSpot.shadmaskcol;
                locallab.spots.at(j).strumaskcol = locallab.spots.at(j).strumaskcol && pSpot.strumaskcol == otherSpot.strumaskcol;
                locallab.spots.at(j).lapmaskcol = locallab.spots.at(j).lapmaskcol && pSpot.lapmaskcol == otherSpot.lapmaskcol;
                locallab.spots.at(j).qualitycurveMethod = locallab.spots.at(j).qualitycurveMethod && pSpot.qualitycurveMethod == otherSpot.qualitycurveMethod;
                locallab.spots.at(j).gridMethod = locallab.spots.at(j).gridMethod && pSpot.gridMethod == otherSpot.gridMethod;
                locallab.spots.at(j).merMethod = locallab.spots.at(j).merMethod && pSpot.merMethod == otherSpot.merMethod;
                locallab.spots.at(j).toneMethod = locallab.spots.at(j).toneMethod && pSpot.toneMethod == otherSpot.toneMethod;
                locallab.spots.at(j).mergecolMethod = locallab.spots.at(j).mergecolMethod && pSpot.mergecolMethod == otherSpot.mergecolMethod;
                locallab.spots.at(j).llcurve = locallab.spots.at(j).llcurve && pSpot.llcurve == otherSpot.llcurve;
                locallab.spots.at(j).lccurve = locallab.spots.at(j).lccurve && pSpot.lccurve == otherSpot.lccurve;
                locallab.spots.at(j).cccurve = locallab.spots.at(j).cccurve && pSpot.cccurve == otherSpot.cccurve;
                locallab.spots.at(j).clcurve = locallab.spots.at(j).clcurve && pSpot.clcurve == otherSpot.clcurve;
                locallab.spots.at(j).rgbcurve = locallab.spots.at(j).rgbcurve && pSpot.rgbcurve == otherSpot.rgbcurve;
                locallab.spots.at(j).LHcurve = locallab.spots.at(j).LHcurve && pSpot.LHcurve == otherSpot.LHcurve;
                locallab.spots.at(j).HHcurve = locallab.spots.at(j).HHcurve && pSpot.HHcurve == otherSpot.HHcurve;
                locallab.spots.at(j).CHcurve = locallab.spots.at(j).CHcurve && pSpot.CHcurve == otherSpot.CHcurve;
                locallab.spots.at(j).invers = locallab.spots.at(j).invers && pSpot.invers == otherSpot.invers;
                locallab.spots.at(j).special = locallab.spots.at(j).special && pSpot.special == otherSpot.special;
                locallab.spots.at(j).toolcol = locallab.spots.at(j).toolcol && pSpot.toolcol == otherSpot.toolcol;
                locallab.spots.at(j).enaColorMask = locallab.spots.at(j).enaColorMask && pSpot.enaColorMask == otherSpot.enaColorMask;
                locallab.spots.at(j).fftColorMask = locallab.spots.at(j).fftColorMask && pSpot.fftColorMask == otherSpot.fftColorMask;
                locallab.spots.at(j).CCmaskcurve = locallab.spots.at(j).CCmaskcurve && pSpot.CCmaskcurve == otherSpot.CCmaskcurve;
                locallab.spots.at(j).LLmaskcurve = locallab.spots.at(j).LLmaskcurve && pSpot.LLmaskcurve == otherSpot.LLmaskcurve;
                locallab.spots.at(j).HHmaskcurve = locallab.spots.at(j).HHmaskcurve && pSpot.HHmaskcurve == otherSpot.HHmaskcurve;
                locallab.spots.at(j).HHhmaskcurve = locallab.spots.at(j).HHhmaskcurve && pSpot.HHhmaskcurve == otherSpot.HHhmaskcurve;
                locallab.spots.at(j).softradiuscol = locallab.spots.at(j).softradiuscol && pSpot.softradiuscol == otherSpot.softradiuscol;
                locallab.spots.at(j).opacol = locallab.spots.at(j).opacol && pSpot.opacol == otherSpot.opacol;
                locallab.spots.at(j).mercol = locallab.spots.at(j).mercol && pSpot.mercol == otherSpot.mercol;
                locallab.spots.at(j).merlucol = locallab.spots.at(j).merlucol && pSpot.merlucol == otherSpot.merlucol;
                locallab.spots.at(j).conthrcol = locallab.spots.at(j).conthrcol && pSpot.conthrcol == otherSpot.conthrcol;
                locallab.spots.at(j).Lmaskcurve = locallab.spots.at(j).Lmaskcurve && pSpot.Lmaskcurve == otherSpot.Lmaskcurve;
                locallab.spots.at(j).LLmaskcolcurvewav = locallab.spots.at(j).LLmaskcolcurvewav && pSpot.LLmaskcolcurvewav == otherSpot.LLmaskcolcurvewav;
                locallab.spots.at(j).csthresholdcol = locallab.spots.at(j).csthresholdcol && pSpot.csthresholdcol == otherSpot.csthresholdcol;
                locallab.spots.at(j).recothresc = locallab.spots.at(j).recothresc && pSpot.recothresc == otherSpot.recothresc;
                locallab.spots.at(j).lowthresc = locallab.spots.at(j).lowthresc && pSpot.lowthresc == otherSpot.lowthresc;
                locallab.spots.at(j).higthresc = locallab.spots.at(j).higthresc && pSpot.higthresc == otherSpot.higthresc;
                locallab.spots.at(j).decayc = locallab.spots.at(j).decayc && pSpot.decayc == otherSpot.decayc;
                // Exposure
                locallab.spots.at(j).visiexpose = locallab.spots.at(j).visiexpose && pSpot.visiexpose == otherSpot.visiexpose;
                locallab.spots.at(j).expexpose = locallab.spots.at(j).expexpose && pSpot.expexpose == otherSpot.expexpose;
                locallab.spots.at(j).complexexpose = locallab.spots.at(j).complexexpose && pSpot.complexexpose == otherSpot.complexexpose;
                locallab.spots.at(j).expcomp = locallab.spots.at(j).expcomp && pSpot.expcomp == otherSpot.expcomp;
                locallab.spots.at(j).hlcompr = locallab.spots.at(j).hlcompr && pSpot.hlcompr == otherSpot.hlcompr;
                locallab.spots.at(j).hlcomprthresh = locallab.spots.at(j).hlcomprthresh && pSpot.hlcomprthresh == otherSpot.hlcomprthresh;
                locallab.spots.at(j).black = locallab.spots.at(j).black && pSpot.black == otherSpot.black;
                locallab.spots.at(j).shadex = locallab.spots.at(j).shadex && pSpot.shadex == otherSpot.shadex;
                locallab.spots.at(j).shcompr = locallab.spots.at(j).shcompr && pSpot.shcompr == otherSpot.shcompr;
                locallab.spots.at(j).expchroma = locallab.spots.at(j).expchroma && pSpot.expchroma == otherSpot.expchroma;
                locallab.spots.at(j).sensiex = locallab.spots.at(j).sensiex && pSpot.sensiex == otherSpot.sensiex;
                locallab.spots.at(j).structexp = locallab.spots.at(j).structexp && pSpot.structexp == otherSpot.structexp;
                locallab.spots.at(j).gamex = locallab.spots.at(j).gamex && pSpot.gamex == otherSpot.gamex;
                locallab.spots.at(j).blurexpde = locallab.spots.at(j).blurexpde && pSpot.blurexpde == otherSpot.blurexpde;
                locallab.spots.at(j).strexp = locallab.spots.at(j).strexp && pSpot.strexp == otherSpot.strexp;
                locallab.spots.at(j).angexp = locallab.spots.at(j).angexp && pSpot.angexp == otherSpot.angexp;
                locallab.spots.at(j).featherexp = locallab.spots.at(j).featherexp && pSpot.featherexp == otherSpot.featherexp;
                locallab.spots.at(j).excurve = locallab.spots.at(j).excurve && pSpot.excurve == otherSpot.excurve;
                locallab.spots.at(j).norm = locallab.spots.at(j).norm && pSpot.norm == otherSpot.norm;
                locallab.spots.at(j).inversex = locallab.spots.at(j).inversex && pSpot.inversex == otherSpot.inversex;
                locallab.spots.at(j).enaExpMask = locallab.spots.at(j).enaExpMask && pSpot.enaExpMask == otherSpot.enaExpMask;
                locallab.spots.at(j).enaExpMaskaft = locallab.spots.at(j).enaExpMaskaft && pSpot.enaExpMaskaft == otherSpot.enaExpMaskaft;
                locallab.spots.at(j).CCmaskexpcurve = locallab.spots.at(j).CCmaskexpcurve && pSpot.CCmaskexpcurve == otherSpot.CCmaskexpcurve;
                locallab.spots.at(j).LLmaskexpcurve = locallab.spots.at(j).LLmaskexpcurve && pSpot.LLmaskexpcurve == otherSpot.LLmaskexpcurve;
                locallab.spots.at(j).HHmaskexpcurve = locallab.spots.at(j).HHmaskexpcurve && pSpot.HHmaskexpcurve == otherSpot.HHmaskexpcurve;
                locallab.spots.at(j).blendmaskexp = locallab.spots.at(j).blendmaskexp && pSpot.blendmaskexp == otherSpot.blendmaskexp;
                locallab.spots.at(j).radmaskexp = locallab.spots.at(j).radmaskexp && pSpot.radmaskexp == otherSpot.radmaskexp;
                locallab.spots.at(j).chromaskexp = locallab.spots.at(j).chromaskexp && pSpot.chromaskexp == otherSpot.chromaskexp;
                locallab.spots.at(j).gammaskexp = locallab.spots.at(j).gammaskexp && pSpot.gammaskexp == otherSpot.gammaskexp;
                locallab.spots.at(j).slomaskexp = locallab.spots.at(j).slomaskexp && pSpot.slomaskexp == otherSpot.slomaskexp;
                locallab.spots.at(j).lapmaskexp = locallab.spots.at(j).lapmaskexp && pSpot.lapmaskexp == otherSpot.lapmaskexp;
                locallab.spots.at(j).strmaskexp = locallab.spots.at(j).strmaskexp && pSpot.strmaskexp == otherSpot.strmaskexp;
                locallab.spots.at(j).angmaskexp = locallab.spots.at(j).angmaskexp && pSpot.angmaskexp == otherSpot.angmaskexp;
                locallab.spots.at(j).softradiusexp = locallab.spots.at(j).softradiusexp && pSpot.softradiusexp == otherSpot.softradiusexp;
                locallab.spots.at(j).Lmaskexpcurve = locallab.spots.at(j).Lmaskexpcurve && pSpot.Lmaskexpcurve == otherSpot.Lmaskexpcurve;
                locallab.spots.at(j).expMethod = locallab.spots.at(j).expMethod && pSpot.expMethod == otherSpot.expMethod;
                locallab.spots.at(j).exnoiseMethod = locallab.spots.at(j).exnoiseMethod && pSpot.exnoiseMethod == otherSpot.exnoiseMethod;
                locallab.spots.at(j).laplacexp = locallab.spots.at(j).laplacexp && pSpot.laplacexp == otherSpot.laplacexp;
                locallab.spots.at(j).reparexp = locallab.spots.at(j).reparexp && pSpot.reparexp == otherSpot.reparexp;
                locallab.spots.at(j).balanexp = locallab.spots.at(j).balanexp && pSpot.balanexp == otherSpot.balanexp;
                locallab.spots.at(j).linear = locallab.spots.at(j).linear && pSpot.linear == otherSpot.linear;
                locallab.spots.at(j).gamm = locallab.spots.at(j).gamm && pSpot.gamm == otherSpot.gamm;
                locallab.spots.at(j).fatamount = locallab.spots.at(j).fatamount && pSpot.fatamount == otherSpot.fatamount;
                locallab.spots.at(j).fatdetail = locallab.spots.at(j).fatdetail && pSpot.fatdetail == otherSpot.fatdetail;
                locallab.spots.at(j).fatsatur = locallab.spots.at(j).fatsatur && pSpot.fatsatur == otherSpot.fatsatur;
                locallab.spots.at(j).fatanchor = locallab.spots.at(j).fatanchor && pSpot.fatanchor == otherSpot.fatanchor;
                locallab.spots.at(j).fatlevel = locallab.spots.at(j).fatlevel && pSpot.fatlevel == otherSpot.fatlevel;
                locallab.spots.at(j).recothrese = locallab.spots.at(j).recothrese && pSpot.recothrese == otherSpot.recothrese;
                locallab.spots.at(j).lowthrese = locallab.spots.at(j).lowthrese && pSpot.lowthrese == otherSpot.lowthrese;
                locallab.spots.at(j).higthrese = locallab.spots.at(j).higthrese && pSpot.higthrese == otherSpot.higthrese;
                locallab.spots.at(j).decaye = locallab.spots.at(j).decaye && pSpot.decaye == otherSpot.decaye;
                // Shadow highlight
                locallab.spots.at(j).visishadhigh = locallab.spots.at(j).visishadhigh && pSpot.visishadhigh == otherSpot.visishadhigh;
                locallab.spots.at(j).expshadhigh = locallab.spots.at(j).expshadhigh && pSpot.expshadhigh == otherSpot.expshadhigh;
                locallab.spots.at(j).complexshadhigh = locallab.spots.at(j).complexshadhigh && pSpot.complexshadhigh == otherSpot.complexshadhigh;
                locallab.spots.at(j).shMethod = locallab.spots.at(j).shMethod && pSpot.shMethod == otherSpot.shMethod;
                locallab.spots.at(j).ghsMethod = locallab.spots.at(j).ghsMethod && pSpot.ghsMethod == otherSpot.ghsMethod;
                locallab.spots.at(j).ghsMatmet = locallab.spots.at(j).ghsMatmet && pSpot.ghsMatmet == otherSpot.ghsMatmet;
                locallab.spots.at(j).ghsMode = locallab.spots.at(j).ghsMode && pSpot.ghsMode == otherSpot.ghsMode;
                locallab.spots.at(j).ghs_D = locallab.spots.at(j).ghs_D && pSpot.ghs_D == otherSpot.ghs_D;
                locallab.spots.at(j).ghs_slope = locallab.spots.at(j).ghs_slope && pSpot.ghs_slope == otherSpot.ghs_slope;
                locallab.spots.at(j).ghs_chro = locallab.spots.at(j).ghs_chro && pSpot.ghs_chro == otherSpot.ghs_chro;
                locallab.spots.at(j).ghs_B = locallab.spots.at(j).ghs_B && pSpot.ghs_B == otherSpot.ghs_B;
                locallab.spots.at(j).ghs_SP = locallab.spots.at(j).ghs_SP && pSpot.ghs_SP == otherSpot.ghs_SP;
                locallab.spots.at(j).SPAutoRadius = locallab.spots.at(j).SPAutoRadius && pSpot.SPAutoRadius == otherSpot.SPAutoRadius;
                locallab.spots.at(j).ghs_LP = locallab.spots.at(j).ghs_LP && pSpot.ghs_LP == otherSpot.ghs_LP;
                locallab.spots.at(j).ghs_HP = locallab.spots.at(j).ghs_HP && pSpot.ghs_HP == otherSpot.ghs_HP;
                locallab.spots.at(j).ghs_LC = locallab.spots.at(j).ghs_LC && pSpot.ghs_LC == otherSpot.ghs_LC;
                locallab.spots.at(j).ghs_MID = locallab.spots.at(j).ghs_MID && pSpot.ghs_MID == otherSpot.ghs_MID;
                locallab.spots.at(j).ghs_BLP = locallab.spots.at(j).ghs_BLP && pSpot.ghs_BLP == otherSpot.ghs_BLP;
                locallab.spots.at(j).ghs_HLP = locallab.spots.at(j).ghs_HLP && pSpot.ghs_HLP == otherSpot.ghs_HLP;
                locallab.spots.at(j).ghs_autobw = locallab.spots.at(j).ghs_autobw && pSpot.ghs_autobw == otherSpot.ghs_autobw;
                locallab.spots.at(j).ghs_agx = locallab.spots.at(j).ghs_agx && pSpot.ghs_agx == otherSpot.ghs_agx;
                locallab.spots.at(j).ghs_smooth = locallab.spots.at(j).ghs_smooth && pSpot.ghs_smooth == otherSpot.ghs_smooth;
                locallab.spots.at(j).ghs_inv = locallab.spots.at(j).ghs_inv && pSpot.ghs_inv == otherSpot.ghs_inv;

                locallab.spots.at(j).mich_exp = locallab.spots.at(j).mich_exp && pSpot.mich_exp == otherSpot.mich_exp;
                locallab.spots.at(j).mich_spar = locallab.spots.at(j).mich_spar && pSpot.mich_spar == otherSpot.mich_spar;
                locallab.spots.at(j).mich_kpar = locallab.spots.at(j).mich_kpar && pSpot.mich_kpar == otherSpot.mich_kpar;
                locallab.spots.at(j).mich_sat = locallab.spots.at(j).mich_sat && pSpot.mich_sat == otherSpot.mich_sat;
                locallab.spots.at(j).mich_out = locallab.spots.at(j).mich_out && pSpot.mich_out == otherSpot.mich_out;
                locallab.spots.at(j).mich_black = locallab.spots.at(j).mich_black && pSpot.mich_black == otherSpot.mich_black;
                locallab.spots.at(j).mich_white = locallab.spots.at(j).mich_white && pSpot.mich_white == otherSpot.mich_white;
                locallab.spots.at(j).mich_high = locallab.spots.at(j).mich_high && pSpot.mich_high == otherSpot.mich_high;
                locallab.spots.at(j).mich_jdx = locallab.spots.at(j).mich_jdx && pSpot.mich_jdx == otherSpot.mich_jdx;

                for (int k = 0; k < 6; k++) {
                    locallab.spots.at(j).multsh[k] = locallab.spots.at(j).multsh[k] && pSpot.multsh[k] == otherSpot.multsh[k];
                }

                locallab.spots.at(j).highlights = locallab.spots.at(j).highlights && pSpot.highlights == otherSpot.highlights;
                locallab.spots.at(j).h_tonalwidth = locallab.spots.at(j).h_tonalwidth && pSpot.h_tonalwidth == otherSpot.h_tonalwidth;
                locallab.spots.at(j).shadows = locallab.spots.at(j).shadows && pSpot.shadows == otherSpot.shadows;
                locallab.spots.at(j).s_tonalwidth = locallab.spots.at(j).s_tonalwidth && pSpot.s_tonalwidth == otherSpot.s_tonalwidth;
                locallab.spots.at(j).sh_radius = locallab.spots.at(j).sh_radius && pSpot.sh_radius == otherSpot.sh_radius;
                locallab.spots.at(j).sensihs = locallab.spots.at(j).sensihs && pSpot.sensihs == otherSpot.sensihs;
                locallab.spots.at(j).enaSHMask = locallab.spots.at(j).enaSHMask && pSpot.enaSHMask == otherSpot.enaSHMask;
                locallab.spots.at(j).CCmaskSHcurve = locallab.spots.at(j).CCmaskSHcurve && pSpot.CCmaskSHcurve == otherSpot.CCmaskSHcurve;
                locallab.spots.at(j).LLmaskSHcurve = locallab.spots.at(j).LLmaskSHcurve && pSpot.LLmaskSHcurve == otherSpot.LLmaskSHcurve;
                locallab.spots.at(j).HHmaskSHcurve = locallab.spots.at(j).HHmaskSHcurve && pSpot.HHmaskSHcurve == otherSpot.HHmaskSHcurve;
                locallab.spots.at(j).blendmaskSH = locallab.spots.at(j).blendmaskSH && pSpot.blendmaskSH == otherSpot.blendmaskSH;
                locallab.spots.at(j).radmaskSH = locallab.spots.at(j).radmaskSH && pSpot.radmaskSH == otherSpot.radmaskSH;
                locallab.spots.at(j).blurSHde = locallab.spots.at(j).blurSHde && pSpot.blurSHde == otherSpot.blurSHde;
                locallab.spots.at(j).strSH = locallab.spots.at(j).strSH && pSpot.strSH == otherSpot.strSH;
                locallab.spots.at(j).angSH = locallab.spots.at(j).angSH && pSpot.angSH == otherSpot.angSH;
                locallab.spots.at(j).featherSH = locallab.spots.at(j).featherSH && pSpot.featherSH == otherSpot.featherSH;
                locallab.spots.at(j).inverssh = locallab.spots.at(j).inverssh && pSpot.inverssh == otherSpot.inverssh;
                locallab.spots.at(j).chromaskSH = locallab.spots.at(j).chromaskSH && pSpot.chromaskSH == otherSpot.chromaskSH;
                locallab.spots.at(j).gammaskSH = locallab.spots.at(j).gammaskSH && pSpot.gammaskSH == otherSpot.gammaskSH;
                locallab.spots.at(j).slomaskSH = locallab.spots.at(j).slomaskSH && pSpot.slomaskSH == otherSpot.slomaskSH;
                locallab.spots.at(j).lapmaskSH = locallab.spots.at(j).lapmaskSH && pSpot.lapmaskSH == otherSpot.lapmaskSH;
                locallab.spots.at(j).detailSH = locallab.spots.at(j).detailSH && pSpot.detailSH == otherSpot.detailSH;
                locallab.spots.at(j).tePivot = locallab.spots.at(j).tePivot && pSpot.tePivot == otherSpot.tePivot;
                locallab.spots.at(j).reparsh = locallab.spots.at(j).reparsh && pSpot.reparsh == otherSpot.reparsh;
                locallab.spots.at(j).LmaskSHcurve = locallab.spots.at(j).LmaskSHcurve && pSpot.LmaskSHcurve == otherSpot.LmaskSHcurve;
                locallab.spots.at(j).fatamountSH = locallab.spots.at(j).fatamountSH && pSpot.fatamountSH == otherSpot.fatamountSH;
                locallab.spots.at(j).fatanchorSH = locallab.spots.at(j).fatanchorSH && pSpot.fatanchorSH == otherSpot.fatanchorSH;
                locallab.spots.at(j).gamSH = locallab.spots.at(j).gamSH && pSpot.gamSH == otherSpot.gamSH;
                locallab.spots.at(j).sloSH = locallab.spots.at(j).sloSH && pSpot.sloSH == otherSpot.sloSH;
                locallab.spots.at(j).recothress = locallab.spots.at(j).recothress && pSpot.recothress == otherSpot.recothress;
                locallab.spots.at(j).lowthress = locallab.spots.at(j).lowthress && pSpot.lowthress == otherSpot.lowthress;
                locallab.spots.at(j).higthress = locallab.spots.at(j).higthress && pSpot.higthress == otherSpot.higthress;
                locallab.spots.at(j).decays = locallab.spots.at(j).decays && pSpot.decays == otherSpot.decays;
                // Vibrance
                locallab.spots.at(j).visivibrance = locallab.spots.at(j).visivibrance && pSpot.visivibrance == otherSpot.visivibrance;
                locallab.spots.at(j).expvibrance = locallab.spots.at(j).expvibrance && pSpot.expvibrance == otherSpot.expvibrance;
                locallab.spots.at(j).complexvibrance = locallab.spots.at(j).complexvibrance && pSpot.complexvibrance == otherSpot.complexvibrance;
                locallab.spots.at(j).saturated = locallab.spots.at(j).saturated && pSpot.saturated == otherSpot.saturated;
                locallab.spots.at(j).pastels = locallab.spots.at(j).pastels && pSpot.pastels == otherSpot.pastels;
                locallab.spots.at(j).vibgam = locallab.spots.at(j).vibgam && pSpot.vibgam == otherSpot.vibgam;
                locallab.spots.at(j).warm = locallab.spots.at(j).warm && pSpot.warm == otherSpot.warm;
                locallab.spots.at(j).psthreshold = locallab.spots.at(j).psthreshold && pSpot.psthreshold == otherSpot.psthreshold;
                locallab.spots.at(j).protectskins = locallab.spots.at(j).protectskins && pSpot.protectskins == otherSpot.protectskins;
                locallab.spots.at(j).avoidcolorshift = locallab.spots.at(j).avoidcolorshift && pSpot.avoidcolorshift == otherSpot.avoidcolorshift;
                locallab.spots.at(j).pastsattog = locallab.spots.at(j).pastsattog && pSpot.pastsattog == otherSpot.pastsattog;
                locallab.spots.at(j).sensiv = locallab.spots.at(j).sensiv && pSpot.sensiv == otherSpot.sensiv;
                locallab.spots.at(j).skintonescurve = locallab.spots.at(j).skintonescurve && pSpot.skintonescurve == otherSpot.skintonescurve;
                locallab.spots.at(j).CCmaskvibcurve = locallab.spots.at(j).CCmaskvibcurve && pSpot.CCmaskvibcurve == otherSpot.CCmaskvibcurve;
                locallab.spots.at(j).LLmaskvibcurve = locallab.spots.at(j).LLmaskvibcurve && pSpot.LLmaskvibcurve == otherSpot.LLmaskvibcurve;
                locallab.spots.at(j).HHmaskvibcurve = locallab.spots.at(j).HHmaskvibcurve && pSpot.HHmaskvibcurve == otherSpot.HHmaskvibcurve;
                locallab.spots.at(j).enavibMask = locallab.spots.at(j).enavibMask && pSpot.enavibMask == otherSpot.enavibMask;
                locallab.spots.at(j).blendmaskvib = locallab.spots.at(j).blendmaskvib && pSpot.blendmaskvib == otherSpot.blendmaskvib;
                locallab.spots.at(j).radmaskvib = locallab.spots.at(j).radmaskvib && pSpot.radmaskvib == otherSpot.radmaskvib;
                locallab.spots.at(j).chromaskvib = locallab.spots.at(j).chromaskvib && pSpot.chromaskvib == otherSpot.chromaskvib;
                locallab.spots.at(j).gammaskvib = locallab.spots.at(j).gammaskvib && pSpot.gammaskvib == otherSpot.gammaskvib;
                locallab.spots.at(j).slomaskvib = locallab.spots.at(j).slomaskvib && pSpot.slomaskvib == otherSpot.slomaskvib;
                locallab.spots.at(j).lapmaskvib = locallab.spots.at(j).lapmaskvib && pSpot.lapmaskvib == otherSpot.lapmaskvib;
                locallab.spots.at(j).strvib = locallab.spots.at(j).strvib && pSpot.strvib == otherSpot.strvib;
                locallab.spots.at(j).strvibab = locallab.spots.at(j).strvibab && pSpot.strvibab == otherSpot.strvibab;
                locallab.spots.at(j).strvibh = locallab.spots.at(j).strvibh && pSpot.strvibh == otherSpot.strvibh;
                locallab.spots.at(j).angvib = locallab.spots.at(j).angvib && pSpot.angvib == otherSpot.angvib;
                locallab.spots.at(j).feathervib = locallab.spots.at(j).feathervib && pSpot.feathervib == otherSpot.feathervib;
                locallab.spots.at(j).Lmaskvibcurve = locallab.spots.at(j).Lmaskvibcurve && pSpot.Lmaskvibcurve == otherSpot.Lmaskvibcurve;
                locallab.spots.at(j).recothresv = locallab.spots.at(j).recothresv && pSpot.recothresv == otherSpot.recothresv;
                locallab.spots.at(j).lowthresv = locallab.spots.at(j).lowthresv && pSpot.lowthresv == otherSpot.lowthresv;
                locallab.spots.at(j).higthresv = locallab.spots.at(j).higthresv && pSpot.higthresv == otherSpot.higthresv;
                locallab.spots.at(j).decayv = locallab.spots.at(j).decayv && pSpot.decayv == otherSpot.decayv;
                // Soft Light
                locallab.spots.at(j).visisoft = locallab.spots.at(j).visisoft && pSpot.visisoft == otherSpot.visisoft;
                locallab.spots.at(j).expsoft = locallab.spots.at(j).expsoft && pSpot.expsoft == otherSpot.expsoft;
                locallab.spots.at(j).complexsoft = locallab.spots.at(j).complexsoft && pSpot.complexsoft == otherSpot.complexsoft;
                locallab.spots.at(j).streng = locallab.spots.at(j).streng && pSpot.streng == otherSpot.streng;
                locallab.spots.at(j).sensisf = locallab.spots.at(j).sensisf && pSpot.sensisf == otherSpot.sensisf;
                locallab.spots.at(j).laplace = locallab.spots.at(j).laplace && pSpot.laplace == otherSpot.laplace;
                locallab.spots.at(j).softMethod = locallab.spots.at(j).softMethod && pSpot.softMethod == otherSpot.softMethod;
                // Blur & Noise
                locallab.spots.at(j).visiblur = locallab.spots.at(j).visiblur && pSpot.visiblur == otherSpot.visiblur;
                locallab.spots.at(j).expblur = locallab.spots.at(j).expblur && pSpot.expblur == otherSpot.expblur;
                locallab.spots.at(j).complexblur = locallab.spots.at(j).complexblur && pSpot.complexblur == otherSpot.complexblur;
                locallab.spots.at(j).radius = locallab.spots.at(j).radius && pSpot.radius == otherSpot.radius;
                locallab.spots.at(j).strength = locallab.spots.at(j).strength && pSpot.strength == otherSpot.strength;
                locallab.spots.at(j).sensibn = locallab.spots.at(j).sensibn && pSpot.sensibn == otherSpot.sensibn;
                locallab.spots.at(j).itera = locallab.spots.at(j).itera && pSpot.itera == otherSpot.itera;
                locallab.spots.at(j).guidbl = locallab.spots.at(j).guidbl && pSpot.guidbl == otherSpot.guidbl;
                locallab.spots.at(j).strbl = locallab.spots.at(j).strbl && pSpot.strbl == otherSpot.strbl;
                locallab.spots.at(j).recothres = locallab.spots.at(j).recothres && pSpot.recothres == otherSpot.recothres;
                locallab.spots.at(j).lowthres = locallab.spots.at(j).lowthres && pSpot.lowthres == otherSpot.lowthres;
                locallab.spots.at(j).higthres = locallab.spots.at(j).higthres && pSpot.higthres == otherSpot.higthres;
                locallab.spots.at(j).recothresd = locallab.spots.at(j).recothresd && pSpot.recothresd == otherSpot.recothresd;
                locallab.spots.at(j).lowthresd = locallab.spots.at(j).lowthresd && pSpot.lowthresd == otherSpot.lowthresd;
                locallab.spots.at(j).midthresd = locallab.spots.at(j).midthresd && pSpot.midthresd == otherSpot.midthresd;
                locallab.spots.at(j).midthresdch = locallab.spots.at(j).midthresdch && pSpot.midthresdch == otherSpot.midthresdch;
                locallab.spots.at(j).higthresd = locallab.spots.at(j).higthresd && pSpot.higthresd == otherSpot.higthresd;
                locallab.spots.at(j).decayd = locallab.spots.at(j).decayd && pSpot.decayd == otherSpot.decayd;
                locallab.spots.at(j).isogr = locallab.spots.at(j).isogr && pSpot.isogr == otherSpot.isogr;
                locallab.spots.at(j).strengr = locallab.spots.at(j).strengr && pSpot.strengr == otherSpot.strengr;
                locallab.spots.at(j).scalegr = locallab.spots.at(j).scalegr && pSpot.scalegr == otherSpot.scalegr;
                locallab.spots.at(j).divgr = locallab.spots.at(j).divgr && pSpot.divgr == otherSpot.divgr;
                locallab.spots.at(j).epsbl = locallab.spots.at(j).epsbl && pSpot.epsbl == otherSpot.epsbl;
                locallab.spots.at(j).blMethod = locallab.spots.at(j).blMethod && pSpot.blMethod == otherSpot.blMethod;
                locallab.spots.at(j).chroMethod = locallab.spots.at(j).chroMethod && pSpot.chroMethod == otherSpot.chroMethod;
                locallab.spots.at(j).quamethod = locallab.spots.at(j).quamethod && pSpot.quamethod == otherSpot.quamethod;
                locallab.spots.at(j).blurMethod = locallab.spots.at(j).blurMethod && pSpot.blurMethod == otherSpot.blurMethod;
                locallab.spots.at(j).usemask = locallab.spots.at(j).usemask && pSpot.usemask == otherSpot.usemask;
                locallab.spots.at(j).invmaskd = locallab.spots.at(j).invmaskd && pSpot.invmaskd == otherSpot.invmaskd;
                locallab.spots.at(j).invmask = locallab.spots.at(j).invmask && pSpot.invmask == otherSpot.invmask;
                locallab.spots.at(j).levelthr = locallab.spots.at(j).levelthr && pSpot.levelthr == otherSpot.levelthr;
                locallab.spots.at(j).lnoiselow = locallab.spots.at(j).lnoiselow && pSpot.lnoiselow == otherSpot.lnoiselow;
                locallab.spots.at(j).levelthrlow = locallab.spots.at(j).levelthrlow && pSpot.levelthrlow == otherSpot.levelthrlow;
                locallab.spots.at(j).medMethod = locallab.spots.at(j).medMethod && pSpot.medMethod == otherSpot.medMethod;
                locallab.spots.at(j).activlum = locallab.spots.at(j).activlum && pSpot.activlum == otherSpot.activlum;
                for (int k = 0; k < 21; k++) {
                    locallab.spots.at(j).madlsav[k] = locallab.spots.at(j).madlsav[k] && pSpot.madlsav[k] == otherSpot.madlsav[k];
                }
                locallab.spots.at(j).noiselumf = locallab.spots.at(j).noiselumf && pSpot.noiselumf == otherSpot.noiselumf;
                locallab.spots.at(j).noiselumf0 = locallab.spots.at(j).noiselumf0 && pSpot.noiselumf0 == otherSpot.noiselumf0;
                locallab.spots.at(j).noiselumf2 = locallab.spots.at(j).noiselumf2 && pSpot.noiselumf2 == otherSpot.noiselumf2;
                locallab.spots.at(j).noiselumc = locallab.spots.at(j).noiselumc && pSpot.noiselumc == otherSpot.noiselumc;
                locallab.spots.at(j).noiselumdetail = locallab.spots.at(j).noiselumdetail && pSpot.noiselumdetail == otherSpot.noiselumdetail;
                locallab.spots.at(j).noiselequal = locallab.spots.at(j).noiselequal && pSpot.noiselequal == otherSpot.noiselequal;
                locallab.spots.at(j).noisegam = locallab.spots.at(j).noisegam && pSpot.noisegam == otherSpot.noisegam;
                locallab.spots.at(j).noisechrof = locallab.spots.at(j).noisechrof && pSpot.noisechrof == otherSpot.noisechrof;
                locallab.spots.at(j).noisechroc = locallab.spots.at(j).noisechroc && pSpot.noisechroc == otherSpot.noisechroc;
                locallab.spots.at(j).noisechrodetail = locallab.spots.at(j).noisechrodetail && pSpot.noisechrodetail == otherSpot.noisechrodetail;
                locallab.spots.at(j).adjblur = locallab.spots.at(j).adjblur && pSpot.adjblur == otherSpot.adjblur;
                locallab.spots.at(j).bilateral = locallab.spots.at(j).bilateral && pSpot.bilateral == otherSpot.bilateral;
                locallab.spots.at(j).nlstr = locallab.spots.at(j).nlstr && pSpot.nlstr == otherSpot.nlstr;
                locallab.spots.at(j).nldet = locallab.spots.at(j).nldet && pSpot.nldet == otherSpot.nldet;
                locallab.spots.at(j).nlpat = locallab.spots.at(j).nlpat && pSpot.nlpat == otherSpot.nlpat;
                locallab.spots.at(j).nlrad = locallab.spots.at(j).nlrad && pSpot.nlrad == otherSpot.nlrad;
                locallab.spots.at(j).nlgam = locallab.spots.at(j).nlgam && pSpot.nlgam == otherSpot.nlgam;
                locallab.spots.at(j).nliter = locallab.spots.at(j).nliter && pSpot.nliter == otherSpot.nliter;
                locallab.spots.at(j).sensiden = locallab.spots.at(j).sensiden && pSpot.sensiden == otherSpot.sensiden;
                locallab.spots.at(j).reparden = locallab.spots.at(j).reparden && pSpot.reparden == otherSpot.reparden;
                locallab.spots.at(j).detailthr = locallab.spots.at(j).detailthr && pSpot.detailthr == otherSpot.detailthr;
                locallab.spots.at(j).locwavcurveden = locallab.spots.at(j).locwavcurveden && pSpot.locwavcurveden == otherSpot.locwavcurveden;
                locallab.spots.at(j).locwavcurvehue = locallab.spots.at(j).locwavcurvehue && pSpot.locwavcurvehue == otherSpot.locwavcurvehue;
                locallab.spots.at(j).locwavcurvehuecont = locallab.spots.at(j).locwavcurvehuecont && pSpot.locwavcurvehuecont == otherSpot.locwavcurvehuecont;
                locallab.spots.at(j).showmaskblMethodtyp = locallab.spots.at(j).showmaskblMethodtyp && pSpot.showmaskblMethodtyp == otherSpot.showmaskblMethodtyp;
                locallab.spots.at(j).CCmaskblcurve = locallab.spots.at(j).CCmaskblcurve && pSpot.CCmaskblcurve == otherSpot.CCmaskblcurve;
                locallab.spots.at(j).LLmaskblcurve = locallab.spots.at(j).LLmaskblcurve && pSpot.LLmaskblcurve == otherSpot.LLmaskblcurve;
                locallab.spots.at(j).HHmaskblcurve = locallab.spots.at(j).HHmaskblcurve && pSpot.HHmaskblcurve == otherSpot.HHmaskblcurve;
                locallab.spots.at(j).enablMask = locallab.spots.at(j).enablMask && pSpot.enablMask == otherSpot.enablMask;
                locallab.spots.at(j).fftwbl = locallab.spots.at(j).fftwbl && pSpot.fftwbl == otherSpot.fftwbl;
                locallab.spots.at(j).invbl = locallab.spots.at(j).invbl && pSpot.invbl == otherSpot.invbl;
                locallab.spots.at(j).toolbl = locallab.spots.at(j).toolbl && pSpot.toolbl == otherSpot.toolbl;
                locallab.spots.at(j).blendmaskbl = locallab.spots.at(j).blendmaskbl && pSpot.blendmaskbl == otherSpot.blendmaskbl;
                locallab.spots.at(j).radmaskbl = locallab.spots.at(j).radmaskbl && pSpot.radmaskbl == otherSpot.radmaskbl;
                locallab.spots.at(j).chromaskbl = locallab.spots.at(j).chromaskbl && pSpot.chromaskbl == otherSpot.chromaskbl;
                locallab.spots.at(j).gammaskbl = locallab.spots.at(j).gammaskbl && pSpot.gammaskbl == otherSpot.gammaskbl;
                locallab.spots.at(j).slomaskbl = locallab.spots.at(j).slomaskbl && pSpot.slomaskbl == otherSpot.slomaskbl;
                locallab.spots.at(j).lapmaskbl = locallab.spots.at(j).lapmaskbl && pSpot.lapmaskbl == otherSpot.lapmaskbl;
                locallab.spots.at(j).shadmaskbl = locallab.spots.at(j).shadmaskbl && pSpot.shadmaskbl == otherSpot.shadmaskbl;
                locallab.spots.at(j).shadmaskblsha = locallab.spots.at(j).shadmaskblsha && pSpot.shadmaskblsha == otherSpot.shadmaskblsha;
                locallab.spots.at(j).strumaskbl = locallab.spots.at(j).strumaskbl && pSpot.strumaskbl == otherSpot.strumaskbl;
                locallab.spots.at(j).Lmaskblcurve = locallab.spots.at(j).Lmaskblcurve && pSpot.Lmaskblcurve == otherSpot.Lmaskblcurve;
                locallab.spots.at(j).LLmaskblcurvewav = locallab.spots.at(j).LLmaskblcurvewav && pSpot.LLmaskblcurvewav == otherSpot.LLmaskblcurvewav;
                locallab.spots.at(j).csthresholdblur = locallab.spots.at(j).csthresholdblur && pSpot.csthresholdblur == otherSpot.csthresholdblur;
                locallab.spots.at(j).denocontrast = locallab.spots.at(j).denocontrast && pSpot.denocontrast == otherSpot.denocontrast;
                locallab.spots.at(j).denoAutocontrast = locallab.spots.at(j).denoAutocontrast && pSpot.denoAutocontrast == otherSpot.denoAutocontrast;
                locallab.spots.at(j).contrshow = locallab.spots.at(j).contrshow && pSpot.contrshow == otherSpot.contrshow;
                locallab.spots.at(j).lockmadl = locallab.spots.at(j).lockmadl && pSpot.lockmadl == otherSpot.lockmadl;
                locallab.spots.at(j).madllock = locallab.spots.at(j).madllock && pSpot.madllock == otherSpot.madllock;
                locallab.spots.at(j).enacontrast = locallab.spots.at(j).enacontrast && pSpot.enacontrast == otherSpot.enacontrast;
                locallab.spots.at(j).denoratio = locallab.spots.at(j).denoratio && pSpot.denoratio == otherSpot.denoratio;
                locallab.spots.at(j).denomask = locallab.spots.at(j).denomask && pSpot.denomask == otherSpot.denomask;
                // Tone Mapping
                locallab.spots.at(j).visitonemap = locallab.spots.at(j).visitonemap && pSpot.visitonemap == otherSpot.visitonemap;
                locallab.spots.at(j).exptonemap = locallab.spots.at(j).exptonemap && pSpot.exptonemap == otherSpot.exptonemap;
                locallab.spots.at(j).complextonemap = locallab.spots.at(j).complextonemap && pSpot.complextonemap == otherSpot.complextonemap;
                locallab.spots.at(j).stren = locallab.spots.at(j).stren && pSpot.stren == otherSpot.stren;
                locallab.spots.at(j).gamma = locallab.spots.at(j).gamma && pSpot.gamma == otherSpot.gamma;
                locallab.spots.at(j).estop = locallab.spots.at(j).estop && pSpot.estop == otherSpot.estop;
                locallab.spots.at(j).scaltm = locallab.spots.at(j).scaltm && pSpot.scaltm == otherSpot.scaltm;
                locallab.spots.at(j).repartm = locallab.spots.at(j).repartm && pSpot.repartm == otherSpot.repartm;
                locallab.spots.at(j).rewei = locallab.spots.at(j).rewei && pSpot.rewei == otherSpot.rewei;
                locallab.spots.at(j).satur = locallab.spots.at(j).satur && pSpot.satur == otherSpot.satur;
                locallab.spots.at(j).sensitm = locallab.spots.at(j).sensitm && pSpot.sensitm == otherSpot.sensitm;
                locallab.spots.at(j).softradiustm = locallab.spots.at(j).softradiustm && pSpot.softradiustm == otherSpot.softradiustm;
                locallab.spots.at(j).amount = locallab.spots.at(j).amount && pSpot.amount == otherSpot.amount;
                locallab.spots.at(j).equiltm = locallab.spots.at(j).equiltm && pSpot.equiltm == otherSpot.equiltm;
                locallab.spots.at(j).CCmasktmcurve = locallab.spots.at(j).CCmasktmcurve && pSpot.CCmasktmcurve == otherSpot.CCmasktmcurve;
                locallab.spots.at(j).LLmasktmcurve = locallab.spots.at(j).LLmasktmcurve && pSpot.LLmasktmcurve == otherSpot.LLmasktmcurve;
                locallab.spots.at(j).HHmasktmcurve = locallab.spots.at(j).HHmasktmcurve && pSpot.HHmasktmcurve == otherSpot.HHmasktmcurve;
                locallab.spots.at(j).enatmMask = locallab.spots.at(j).enatmMask && pSpot.enatmMask == otherSpot.enatmMask;
                locallab.spots.at(j).enatmMaskaft = locallab.spots.at(j).enatmMaskaft && pSpot.enatmMaskaft == otherSpot.enatmMaskaft;
                locallab.spots.at(j).blendmasktm = locallab.spots.at(j).blendmasktm && pSpot.blendmasktm == otherSpot.blendmasktm;
                locallab.spots.at(j).radmasktm = locallab.spots.at(j).radmasktm && pSpot.radmasktm == otherSpot.radmasktm;
                locallab.spots.at(j).chromasktm = locallab.spots.at(j).chromasktm && pSpot.chromasktm == otherSpot.chromasktm;
                locallab.spots.at(j).gammasktm = locallab.spots.at(j).gammasktm && pSpot.gammasktm == otherSpot.gammasktm;
                locallab.spots.at(j).slomasktm = locallab.spots.at(j).slomasktm && pSpot.slomasktm == otherSpot.slomasktm;
                locallab.spots.at(j).lapmasktm = locallab.spots.at(j).lapmasktm && pSpot.lapmasktm == otherSpot.lapmasktm;
                locallab.spots.at(j).Lmasktmcurve = locallab.spots.at(j).Lmasktmcurve && pSpot.Lmasktmcurve == otherSpot.Lmasktmcurve;
                locallab.spots.at(j).recothrest = locallab.spots.at(j).recothrest && pSpot.recothrest == otherSpot.recothrest;
                locallab.spots.at(j).lowthrest = locallab.spots.at(j).lowthrest && pSpot.lowthrest == otherSpot.lowthrest;
                locallab.spots.at(j).higthrest = locallab.spots.at(j).higthrest && pSpot.higthrest == otherSpot.higthrest;
                locallab.spots.at(j).decayt = locallab.spots.at(j).decayt && pSpot.decayt == otherSpot.decayt;
                // Retinex
                locallab.spots.at(j).visireti = locallab.spots.at(j).visireti && pSpot.visireti == otherSpot.visireti;
                locallab.spots.at(j).expreti = locallab.spots.at(j).expreti && pSpot.expreti == otherSpot.expreti;
                locallab.spots.at(j).complexreti = locallab.spots.at(j).complexreti && pSpot.complexreti == otherSpot.complexreti;
                locallab.spots.at(j).retinexMethod = locallab.spots.at(j).retinexMethod && pSpot.retinexMethod == otherSpot.retinexMethod;
                locallab.spots.at(j).str = locallab.spots.at(j).str && pSpot.str == otherSpot.str;
                locallab.spots.at(j).chrrt = locallab.spots.at(j).chrrt && pSpot.chrrt == otherSpot.chrrt;
                locallab.spots.at(j).neigh = locallab.spots.at(j).neigh && pSpot.neigh == otherSpot.neigh;
                locallab.spots.at(j).vart = locallab.spots.at(j).vart && pSpot.vart == otherSpot.vart;
                locallab.spots.at(j).offs = locallab.spots.at(j).offs && pSpot.offs == otherSpot.offs;
                locallab.spots.at(j).dehaz = locallab.spots.at(j).dehaz && pSpot.dehaz == otherSpot.dehaz;
                locallab.spots.at(j).depth = locallab.spots.at(j).depth && pSpot.depth == otherSpot.depth;
                locallab.spots.at(j).sensih = locallab.spots.at(j).sensih && pSpot.sensih == otherSpot.sensih;
                locallab.spots.at(j).localTgaincurve = locallab.spots.at(j).localTgaincurve && pSpot.localTgaincurve == otherSpot.localTgaincurve;
                locallab.spots.at(j).localTtranscurve = locallab.spots.at(j).localTtranscurve && pSpot.localTtranscurve == otherSpot.localTtranscurve;
                locallab.spots.at(j).inversret = locallab.spots.at(j).inversret && pSpot.inversret == otherSpot.inversret;
                locallab.spots.at(j).equilret = locallab.spots.at(j).equilret && pSpot.equilret == otherSpot.equilret;
                locallab.spots.at(j).loglin = locallab.spots.at(j).loglin && pSpot.loglin == otherSpot.loglin;
                locallab.spots.at(j).dehazeSaturation = locallab.spots.at(j).dehazeSaturation && pSpot.dehazeSaturation == otherSpot.dehazeSaturation;
                locallab.spots.at(j).dehazeblack = locallab.spots.at(j).dehazeblack && pSpot.dehazeblack == otherSpot.dehazeblack;
                locallab.spots.at(j).softradiusret = locallab.spots.at(j).softradiusret && pSpot.softradiusret == otherSpot.softradiusret;
                locallab.spots.at(j).CCmaskreticurve = locallab.spots.at(j).CCmaskreticurve && pSpot.CCmaskreticurve == otherSpot.CCmaskreticurve;
                locallab.spots.at(j).LLmaskreticurve = locallab.spots.at(j).LLmaskreticurve && pSpot.LLmaskreticurve == otherSpot.LLmaskreticurve;
                locallab.spots.at(j).HHmaskreticurve = locallab.spots.at(j).HHmaskreticurve && pSpot.HHmaskreticurve == otherSpot.HHmaskreticurve;
                locallab.spots.at(j).enaretiMask = locallab.spots.at(j).enaretiMask && pSpot.enaretiMask == otherSpot.enaretiMask;
                locallab.spots.at(j).enaretiMasktmap = locallab.spots.at(j).enaretiMasktmap && pSpot.enaretiMasktmap == otherSpot.enaretiMasktmap;
                locallab.spots.at(j).blendmaskreti = locallab.spots.at(j).blendmaskreti && pSpot.blendmaskreti == otherSpot.blendmaskreti;
                locallab.spots.at(j).radmaskreti = locallab.spots.at(j).radmaskreti && pSpot.radmaskreti == otherSpot.radmaskreti;
                locallab.spots.at(j).chromaskreti = locallab.spots.at(j).chromaskreti && pSpot.chromaskreti == otherSpot.chromaskreti;
                locallab.spots.at(j).gammaskreti = locallab.spots.at(j).gammaskreti && pSpot.gammaskreti == otherSpot.gammaskreti;
                locallab.spots.at(j).slomaskreti = locallab.spots.at(j).slomaskreti && pSpot.slomaskreti == otherSpot.slomaskreti;
                locallab.spots.at(j).lapmaskreti = locallab.spots.at(j).lapmaskreti && pSpot.lapmaskreti == otherSpot.lapmaskreti;
                locallab.spots.at(j).scalereti = locallab.spots.at(j).scalereti && pSpot.scalereti == otherSpot.scalereti;
                locallab.spots.at(j).darkness = locallab.spots.at(j).darkness && pSpot.darkness == otherSpot.darkness;
                locallab.spots.at(j).lightnessreti = locallab.spots.at(j).lightnessreti && pSpot.lightnessreti == otherSpot.lightnessreti;
                locallab.spots.at(j).limd = locallab.spots.at(j).limd && pSpot.limd == otherSpot.limd;
                locallab.spots.at(j).cliptm = locallab.spots.at(j).cliptm && pSpot.cliptm == otherSpot.cliptm;
                locallab.spots.at(j).fftwreti = locallab.spots.at(j).fftwreti && pSpot.fftwreti == otherSpot.fftwreti;
                locallab.spots.at(j).Lmaskreticurve = locallab.spots.at(j).Lmaskreticurve && pSpot.Lmaskreticurve == otherSpot.Lmaskreticurve;
                locallab.spots.at(j).recothresr = locallab.spots.at(j).recothresr && pSpot.recothresr == otherSpot.recothresr;
                locallab.spots.at(j).lowthresr = locallab.spots.at(j).lowthresr && pSpot.lowthresr == otherSpot.lowthresr;
                locallab.spots.at(j).higthresr = locallab.spots.at(j).higthresr && pSpot.higthresr == otherSpot.higthresr;
                locallab.spots.at(j).decayr = locallab.spots.at(j).decayr && pSpot.decayr == otherSpot.decayr;
                // Sharpening
                locallab.spots.at(j).visisharp = locallab.spots.at(j).visisharp && pSpot.visisharp == otherSpot.visisharp;
                locallab.spots.at(j).expsharp = locallab.spots.at(j).expsharp && pSpot.expsharp == otherSpot.expsharp;
                locallab.spots.at(j).complexsharp = locallab.spots.at(j).complexsharp && pSpot.complexsharp == otherSpot.complexsharp;
                locallab.spots.at(j).sharcontrast = locallab.spots.at(j).sharcontrast && pSpot.sharcontrast == otherSpot.sharcontrast;
                locallab.spots.at(j).deconvAutoshar = locallab.spots.at(j).deconvAutoshar && pSpot.deconvAutoshar == otherSpot.deconvAutoshar;
                locallab.spots.at(j).sharradius = locallab.spots.at(j).sharradius && pSpot.sharradius == otherSpot.sharradius;
                locallab.spots.at(j).sharamount = locallab.spots.at(j).sharamount && pSpot.sharamount == otherSpot.sharamount;
                locallab.spots.at(j).shardamping = locallab.spots.at(j).shardamping && pSpot.shardamping == otherSpot.shardamping;
                locallab.spots.at(j).shariter = locallab.spots.at(j).shariter && pSpot.shariter == otherSpot.shariter;
                locallab.spots.at(j).sharblur = locallab.spots.at(j).sharblur && pSpot.sharblur == otherSpot.sharblur;
                locallab.spots.at(j).shargam = locallab.spots.at(j).shargam && pSpot.shargam == otherSpot.shargam;
                locallab.spots.at(j).sensisha = locallab.spots.at(j).sensisha && pSpot.sensisha == otherSpot.sensisha;
                locallab.spots.at(j).inverssha = locallab.spots.at(j).inverssha && pSpot.inverssha == otherSpot.inverssha;
                locallab.spots.at(j).sharshow = locallab.spots.at(j).sharshow && pSpot.sharshow == otherSpot.sharshow;
                locallab.spots.at(j).itercheck = locallab.spots.at(j).itercheck && pSpot.itercheck == otherSpot.itercheck;
                locallab.spots.at(j).methodcap = locallab.spots.at(j).methodcap && pSpot.methodcap == otherSpot.methodcap;
                locallab.spots.at(j).capradius = locallab.spots.at(j).capradius && pSpot.capradius == otherSpot.capradius;
                locallab.spots.at(j).deconvAutoRadius = locallab.spots.at(j).deconvAutoRadius && pSpot.deconvAutoRadius == otherSpot.deconvAutoRadius;
                locallab.spots.at(j).deconvCoBoost = locallab.spots.at(j).deconvCoBoost && pSpot.deconvCoBoost == otherSpot.deconvCoBoost;
                locallab.spots.at(j).deconvCoProt = locallab.spots.at(j).deconvCoProt && pSpot.deconvCoProt == otherSpot.deconvCoProt;
                locallab.spots.at(j).deconvCoLat = locallab.spots.at(j).deconvCoLat && pSpot.deconvCoLat == otherSpot.deconvCoLat;
                locallab.spots.at(j).deconvCogam = locallab.spots.at(j).deconvCogam && pSpot.deconvCogam == otherSpot.deconvCogam;
                locallab.spots.at(j).reparsha = locallab.spots.at(j).reparsha && pSpot.reparsha == otherSpot.reparsha;

                // Local Contrast
                locallab.spots.at(j).visicontrast = locallab.spots.at(j).visicontrast && pSpot.visicontrast == otherSpot.visicontrast;
                locallab.spots.at(j).expcontrast = locallab.spots.at(j).expcontrast && pSpot.expcontrast == otherSpot.expcontrast;
                locallab.spots.at(j).complexcontrast = locallab.spots.at(j).complexcontrast && pSpot.complexcontrast == otherSpot.complexcontrast;
                locallab.spots.at(j).lcradius = locallab.spots.at(j).lcradius && pSpot.lcradius == otherSpot.lcradius;
                locallab.spots.at(j).lcamount = locallab.spots.at(j).lcamount && pSpot.lcamount == otherSpot.lcamount;
                locallab.spots.at(j).lcdarkness = locallab.spots.at(j).lcdarkness && pSpot.lcdarkness == otherSpot.lcdarkness;
                locallab.spots.at(j).lclightness = locallab.spots.at(j).lclightness && pSpot.lclightness == otherSpot.lclightness;
                locallab.spots.at(j).sigmalc = locallab.spots.at(j).sigmalc && pSpot.sigmalc == otherSpot.sigmalc;
                locallab.spots.at(j).offslc = locallab.spots.at(j).offslc && pSpot.offslc == otherSpot.offslc;
                locallab.spots.at(j).levelwav = locallab.spots.at(j).levelwav && pSpot.levelwav == otherSpot.levelwav;
                locallab.spots.at(j).residcont = locallab.spots.at(j).residcont && pSpot.residcont == otherSpot.residcont;
                locallab.spots.at(j).residsha = locallab.spots.at(j).residsha && pSpot.residsha == otherSpot.residsha;
                locallab.spots.at(j).residshathr = locallab.spots.at(j).residshathr && pSpot.residshathr == otherSpot.residshathr;
                locallab.spots.at(j).residhi = locallab.spots.at(j).residhi && pSpot.residhi == otherSpot.residhi;
                locallab.spots.at(j).residhithr = locallab.spots.at(j).residhithr && pSpot.residhithr == otherSpot.residhithr;
                locallab.spots.at(j).gamlc = locallab.spots.at(j).gamlc && pSpot.gamlc == otherSpot.gamlc;
                locallab.spots.at(j).residgam = locallab.spots.at(j).residgam && pSpot.residgam == otherSpot.residgam;
                locallab.spots.at(j).residslop = locallab.spots.at(j).residslop && pSpot.residslop == otherSpot.residslop;
                locallab.spots.at(j).residblur = locallab.spots.at(j).residblur && pSpot.residblur == otherSpot.residblur;
                locallab.spots.at(j).levelblur = locallab.spots.at(j).levelblur && pSpot.levelblur == otherSpot.levelblur;
                locallab.spots.at(j).sigmabl = locallab.spots.at(j).sigmabl && pSpot.sigmabl == otherSpot.sigmabl;
                locallab.spots.at(j).residchro = locallab.spots.at(j).residchro && pSpot.residchro == otherSpot.residchro;
                locallab.spots.at(j).residcomp = locallab.spots.at(j).residcomp && pSpot.residcomp == otherSpot.residcomp;
                locallab.spots.at(j).sigma = locallab.spots.at(j).sigma && pSpot.sigma == otherSpot.sigma;
                locallab.spots.at(j).offset = locallab.spots.at(j).offset && pSpot.offset == otherSpot.offset;
                locallab.spots.at(j).sigmadr = locallab.spots.at(j).sigmadr && pSpot.sigmadr == otherSpot.sigmadr;
                locallab.spots.at(j).threswav = locallab.spots.at(j).threswav && pSpot.threswav == otherSpot.threswav;
                locallab.spots.at(j).chromalev = locallab.spots.at(j).chromalev && pSpot.chromalev == otherSpot.chromalev;
                locallab.spots.at(j).chromablu = locallab.spots.at(j).chromablu && pSpot.chromablu == otherSpot.chromablu;
                locallab.spots.at(j).sigmadc = locallab.spots.at(j).sigmadc && pSpot.sigmadc == otherSpot.sigmadc;
                locallab.spots.at(j).deltad = locallab.spots.at(j).deltad && pSpot.deltad == otherSpot.deltad;
                locallab.spots.at(j).fatres = locallab.spots.at(j).fatres && pSpot.fatres == otherSpot.fatres;
                locallab.spots.at(j).clarilres = locallab.spots.at(j).clarilres && pSpot.clarilres == otherSpot.clarilres;
                locallab.spots.at(j).claricres = locallab.spots.at(j).claricres && pSpot.claricres == otherSpot.claricres;
                locallab.spots.at(j).clarisoft = locallab.spots.at(j).clarisoft && pSpot.clarisoft == otherSpot.clarisoft;
                locallab.spots.at(j).sigmalc2 = locallab.spots.at(j).sigmalc2 && pSpot.sigmalc2 == otherSpot.sigmalc2;
                locallab.spots.at(j).strwav = locallab.spots.at(j).strwav && pSpot.strwav == otherSpot.strwav;
                locallab.spots.at(j).angwav = locallab.spots.at(j).angwav && pSpot.angwav == otherSpot.angwav;
                locallab.spots.at(j).featherwav = locallab.spots.at(j).featherwav && pSpot.featherwav == otherSpot.featherwav;
                locallab.spots.at(j).strengthw = locallab.spots.at(j).strengthw && pSpot.strengthw == otherSpot.strengthw;
                locallab.spots.at(j).sigmaed = locallab.spots.at(j).sigmaed && pSpot.sigmaed == otherSpot.sigmaed;
                locallab.spots.at(j).radiusw = locallab.spots.at(j).radiusw && pSpot.radiusw == otherSpot.radiusw;
                locallab.spots.at(j).detailw = locallab.spots.at(j).detailw && pSpot.detailw == otherSpot.detailw;
                locallab.spots.at(j).gradw = locallab.spots.at(j).gradw && pSpot.gradw == otherSpot.gradw;
                locallab.spots.at(j).tloww = locallab.spots.at(j).tloww && pSpot.tloww == otherSpot.tloww;
                locallab.spots.at(j).thigw = locallab.spots.at(j).thigw && pSpot.thigw == otherSpot.thigw;
                locallab.spots.at(j).edgw = locallab.spots.at(j).edgw && pSpot.edgw == otherSpot.edgw;
                locallab.spots.at(j).basew = locallab.spots.at(j).basew && pSpot.basew == otherSpot.basew;
                locallab.spots.at(j).sensilc = locallab.spots.at(j).sensilc && pSpot.sensilc == otherSpot.sensilc;
                locallab.spots.at(j).reparw = locallab.spots.at(j).reparw && pSpot.reparw == otherSpot.reparw;
                locallab.spots.at(j).fftwlc = locallab.spots.at(j).fftwlc && pSpot.fftwlc == otherSpot.fftwlc;
                locallab.spots.at(j).blurlc = locallab.spots.at(j).blurlc && pSpot.blurlc == otherSpot.blurlc;
                locallab.spots.at(j).wavblur = locallab.spots.at(j).wavblur && pSpot.wavblur == otherSpot.wavblur;
                locallab.spots.at(j).wavedg = locallab.spots.at(j).wavedg && pSpot.wavedg == otherSpot.wavedg;
                locallab.spots.at(j).waveshow = locallab.spots.at(j).waveshow && pSpot.waveshow == otherSpot.waveshow;
                locallab.spots.at(j).wavcont = locallab.spots.at(j).wavcont && pSpot.wavcont == otherSpot.wavcont;
                locallab.spots.at(j).wavcomp = locallab.spots.at(j).wavcomp && pSpot.wavcomp == otherSpot.wavcomp;
                locallab.spots.at(j).wavgradl = locallab.spots.at(j).wavgradl && pSpot.wavgradl == otherSpot.wavgradl;
                locallab.spots.at(j).wavcompre = locallab.spots.at(j).wavcompre && pSpot.wavcompre == otherSpot.wavcompre;
                locallab.spots.at(j).origlc = locallab.spots.at(j).origlc && pSpot.origlc == otherSpot.origlc;
                locallab.spots.at(j).processwav = locallab.spots.at(j).processwav && pSpot.processwav == otherSpot.processwav;
                locallab.spots.at(j).localcontMethod = locallab.spots.at(j).localcontMethod && pSpot.localcontMethod == otherSpot.localcontMethod;
                locallab.spots.at(j).localedgMethod = locallab.spots.at(j).localedgMethod && pSpot.localedgMethod == otherSpot.localedgMethod;
                locallab.spots.at(j).localneiMethod = locallab.spots.at(j).localneiMethod && pSpot.localneiMethod == otherSpot.localneiMethod;
                locallab.spots.at(j).locwavcurve = locallab.spots.at(j).locwavcurve && pSpot.locwavcurve == otherSpot.locwavcurve;
                locallab.spots.at(j).csthreshold = locallab.spots.at(j).csthreshold && pSpot.csthreshold == otherSpot.csthreshold;
                locallab.spots.at(j).loclevwavcurve = locallab.spots.at(j).loclevwavcurve && pSpot.loclevwavcurve == otherSpot.loclevwavcurve;
                locallab.spots.at(j).locconwavcurve = locallab.spots.at(j).locconwavcurve && pSpot.locconwavcurve == otherSpot.locconwavcurve;
                locallab.spots.at(j).loccompwavcurve = locallab.spots.at(j).loccompwavcurve && pSpot.loccompwavcurve == otherSpot.loccompwavcurve;
                locallab.spots.at(j).loccomprewavcurve = locallab.spots.at(j).loccomprewavcurve && pSpot.loccomprewavcurve == otherSpot.loccomprewavcurve;
                locallab.spots.at(j).locedgwavcurve = locallab.spots.at(j).locedgwavcurve && pSpot.locedgwavcurve == otherSpot.locedgwavcurve;
                locallab.spots.at(j).CCmasklccurve = locallab.spots.at(j).CCmasklccurve && pSpot.CCmasklccurve == otherSpot.CCmasklccurve;
                locallab.spots.at(j).LLmasklccurve = locallab.spots.at(j).LLmasklccurve && pSpot.LLmasklccurve == otherSpot.LLmasklccurve;
                locallab.spots.at(j).HHmasklccurve = locallab.spots.at(j).HHmasklccurve && pSpot.HHmasklccurve == otherSpot.HHmasklccurve;
                locallab.spots.at(j).enalcMask = locallab.spots.at(j).enalcMask && pSpot.enalcMask == otherSpot.enalcMask;
                locallab.spots.at(j).blendmasklc = locallab.spots.at(j).blendmasklc && pSpot.blendmasklc == otherSpot.blendmasklc;
                locallab.spots.at(j).radmasklc = locallab.spots.at(j).radmasklc && pSpot.radmaskcb == otherSpot.radmasklc;
                locallab.spots.at(j).chromasklc = locallab.spots.at(j).chromasklc && pSpot.chromasklc == otherSpot.chromasklc;
                locallab.spots.at(j).Lmasklccurve = locallab.spots.at(j).Lmasklccurve && pSpot.Lmasklccurve == otherSpot.Lmasklccurve;
                locallab.spots.at(j).recothresw = locallab.spots.at(j).recothresw && pSpot.recothresw == otherSpot.recothresw;
                locallab.spots.at(j).lowthresw = locallab.spots.at(j).lowthresw && pSpot.lowthresw == otherSpot.lowthresw;
                locallab.spots.at(j).higthresw = locallab.spots.at(j).higthresw && pSpot.higthresw == otherSpot.higthresw;
                locallab.spots.at(j).decayw = locallab.spots.at(j).decayw && pSpot.decayw == otherSpot.decayw;
                // Contrast by detail levels
                locallab.spots.at(j).visicbdl = locallab.spots.at(j).visicbdl && pSpot.visicbdl == otherSpot.visicbdl;
                locallab.spots.at(j).expcbdl = locallab.spots.at(j).expcbdl && pSpot.expcbdl == otherSpot.expcbdl;
                locallab.spots.at(j).complexcbdl = locallab.spots.at(j).complexcbdl && pSpot.complexcbdl == otherSpot.complexcbdl;

                for (int k = 0; k < 6; k++) {
                    locallab.spots.at(j).mult[k] = locallab.spots.at(j).mult[k] && pSpot.mult[k] == otherSpot.mult[k];
                }

                locallab.spots.at(j).chromacbdl = locallab.spots.at(j).chromacbdl && pSpot.chromacbdl == otherSpot.chromacbdl;
                locallab.spots.at(j).threshold = locallab.spots.at(j).threshold && pSpot.threshold == otherSpot.threshold;
                locallab.spots.at(j).sensicb = locallab.spots.at(j).sensicb && pSpot.sensicb == otherSpot.sensicb;
                locallab.spots.at(j).clarityml = locallab.spots.at(j).clarityml && pSpot.clarityml == otherSpot.clarityml;
                locallab.spots.at(j).contresid = locallab.spots.at(j).contresid && pSpot.contresid == otherSpot.contresid;
                locallab.spots.at(j).softradiuscb = locallab.spots.at(j).softradiuscb && pSpot.softradiuscb == otherSpot.softradiuscb;
                locallab.spots.at(j).enacbMask = locallab.spots.at(j).enacbMask && pSpot.enacbMask == otherSpot.enacbMask;
                locallab.spots.at(j).CCmaskcbcurve = locallab.spots.at(j).CCmaskcbcurve && pSpot.CCmaskcbcurve == otherSpot.CCmaskcbcurve;
                locallab.spots.at(j).LLmaskcbcurve = locallab.spots.at(j).LLmaskcbcurve && pSpot.LLmaskcbcurve == otherSpot.LLmaskcbcurve;
                locallab.spots.at(j).HHmaskcbcurve = locallab.spots.at(j).HHmaskcbcurve && pSpot.HHmaskcbcurve == otherSpot.HHmaskcbcurve;
                locallab.spots.at(j).blendmaskcb = locallab.spots.at(j).blendmaskcb && pSpot.blendmaskcb == otherSpot.blendmaskcb;
                locallab.spots.at(j).radmaskcb = locallab.spots.at(j).radmaskcb && pSpot.radmaskcb == otherSpot.radmaskcb;
                locallab.spots.at(j).chromaskcb = locallab.spots.at(j).chromaskcb && pSpot.chromaskcb == otherSpot.chromaskcb;
                locallab.spots.at(j).gammaskcb = locallab.spots.at(j).gammaskcb && pSpot.gammaskcb == otherSpot.gammaskcb;
                locallab.spots.at(j).slomaskcb = locallab.spots.at(j).slomaskcb && pSpot.slomaskcb == otherSpot.slomaskcb;
                locallab.spots.at(j).lapmaskcb = locallab.spots.at(j).lapmaskcb && pSpot.lapmaskcb == otherSpot.lapmaskcb;
                locallab.spots.at(j).Lmaskcbcurve = locallab.spots.at(j).Lmaskcbcurve && pSpot.Lmaskcbcurve == otherSpot.Lmaskcbcurve;
                locallab.spots.at(j).recothrescb = locallab.spots.at(j).recothrescb && pSpot.recothrescb == otherSpot.recothrescb;
                locallab.spots.at(j).lowthrescb = locallab.spots.at(j).lowthrescb && pSpot.lowthrescb == otherSpot.lowthrescb;
                locallab.spots.at(j).higthrescb = locallab.spots.at(j).higthrescb && pSpot.higthrescb == otherSpot.higthrescb;
                locallab.spots.at(j).decaycb = locallab.spots.at(j).decaycb && pSpot.decaycb == otherSpot.decaycb;
                // Log encoding
                locallab.spots.at(j).visilog = locallab.spots.at(j).visilog && pSpot.visilog == otherSpot.visilog;
                locallab.spots.at(j).explog = locallab.spots.at(j).explog && pSpot.explog == otherSpot.explog;
                locallab.spots.at(j).complexlog = locallab.spots.at(j).complexlog && pSpot.complexlog == otherSpot.complexlog;
                locallab.spots.at(j).autocompute = locallab.spots.at(j).autocompute && pSpot.autocompute == otherSpot.autocompute;
                locallab.spots.at(j).sourceGray = locallab.spots.at(j).sourceGray && pSpot.sourceGray == otherSpot.sourceGray;
                locallab.spots.at(j).sourceabs = locallab.spots.at(j).sourceabs && pSpot.sourceabs == otherSpot.sourceabs;
                locallab.spots.at(j).targabs = locallab.spots.at(j).targabs && pSpot.targabs == otherSpot.targabs;
                locallab.spots.at(j).targetGray = locallab.spots.at(j).targetGray && pSpot.targetGray == otherSpot.targetGray;
                locallab.spots.at(j).catad = locallab.spots.at(j).catad && pSpot.catad == otherSpot.catad;
                locallab.spots.at(j).saturl = locallab.spots.at(j).saturl && pSpot.saturl == otherSpot.saturl;
                locallab.spots.at(j).chroml = locallab.spots.at(j).chroml && pSpot.chroml == otherSpot.chroml;
                locallab.spots.at(j).lightl = locallab.spots.at(j).lightl && pSpot.lightl == otherSpot.lightl;
                locallab.spots.at(j).lightq = locallab.spots.at(j).lightq && pSpot.lightq == otherSpot.lightq;
                locallab.spots.at(j).contl = locallab.spots.at(j).contl && pSpot.contl == otherSpot.contl;
                locallab.spots.at(j).contthres = locallab.spots.at(j).contthres && pSpot.contthres == otherSpot.contthres;
                locallab.spots.at(j).contq = locallab.spots.at(j).contq && pSpot.contq == otherSpot.contq;
                locallab.spots.at(j).colorfl = locallab.spots.at(j).colorfl && pSpot.colorfl == otherSpot.colorfl;
                locallab.spots.at(j).LcurveL = locallab.spots.at(j).LcurveL && pSpot.LcurveL == otherSpot.LcurveL;
                locallab.spots.at(j).Autogray = locallab.spots.at(j).Autogray && pSpot.Autogray == otherSpot.Autogray;
                locallab.spots.at(j).fullimage = locallab.spots.at(j).fullimage && pSpot.fullimage == otherSpot.fullimage;
                locallab.spots.at(j).ciecam = locallab.spots.at(j).ciecam && pSpot.ciecam == otherSpot.ciecam;
                locallab.spots.at(j).satlog = locallab.spots.at(j).satlog && pSpot.satlog == otherSpot.satlog;
                locallab.spots.at(j).enaLMask = locallab.spots.at(j).enaLMask && pSpot.enaLMask == otherSpot.enaLMask;
                locallab.spots.at(j).repar = locallab.spots.at(j).repar && pSpot.repar == otherSpot.repar;
                locallab.spots.at(j).blackEv = locallab.spots.at(j).blackEv && pSpot.blackEv == otherSpot.blackEv;
                locallab.spots.at(j).whiteEv = locallab.spots.at(j).whiteEv && pSpot.whiteEv == otherSpot.whiteEv;
                locallab.spots.at(j).whiteslog = locallab.spots.at(j).whiteslog && pSpot.whiteslog == otherSpot.whiteslog;
                locallab.spots.at(j).blackslog = locallab.spots.at(j).blackslog && pSpot.blackslog == otherSpot.blackslog;
                locallab.spots.at(j).comprlog = locallab.spots.at(j).comprlog && pSpot.comprlog == otherSpot.comprlog;
                locallab.spots.at(j).strelog = locallab.spots.at(j).strelog && pSpot.strelog == otherSpot.strelog;
                locallab.spots.at(j).detail = locallab.spots.at(j).detail && pSpot.detail == otherSpot.detail;
                locallab.spots.at(j).sursour = locallab.spots.at(j).sursour && pSpot.sursour == otherSpot.sursour;
                locallab.spots.at(j).surround = locallab.spots.at(j).surround && pSpot.surround == otherSpot.surround;
                locallab.spots.at(j).sensilog = locallab.spots.at(j).sensilog && pSpot.sensilog == otherSpot.sensilog;
                locallab.spots.at(j).baselog = locallab.spots.at(j).baselog && pSpot.baselog == otherSpot.baselog;
                locallab.spots.at(j).strlog = locallab.spots.at(j).strlog && pSpot.strlog == otherSpot.strlog;
                locallab.spots.at(j).anglog = locallab.spots.at(j).anglog && pSpot.anglog == otherSpot.anglog;
                locallab.spots.at(j).featherlog = locallab.spots.at(j).featherlog && pSpot.featherlog == otherSpot.featherlog;
                locallab.spots.at(j).CCmaskcurveL = locallab.spots.at(j).CCmaskcurveL && pSpot.CCmaskcurveL == otherSpot.CCmaskcurveL;
                locallab.spots.at(j).LLmaskcurveL = locallab.spots.at(j).LLmaskcurveL && pSpot.LLmaskcurveL == otherSpot.LLmaskcurveL;
                locallab.spots.at(j).HHmaskcurveL = locallab.spots.at(j).HHmaskcurveL && pSpot.HHmaskcurveL == otherSpot.HHmaskcurveL;
                locallab.spots.at(j).blendmaskL = locallab.spots.at(j).blendmaskL && pSpot.blendmaskL == otherSpot.blendmaskL;
                locallab.spots.at(j).radmaskL = locallab.spots.at(j).radmaskL && pSpot.radmaskL == otherSpot.radmaskL;
                locallab.spots.at(j).chromaskL = locallab.spots.at(j).chromaskL && pSpot.chromaskL == otherSpot.chromaskL;
                locallab.spots.at(j).LmaskcurveL = locallab.spots.at(j).LmaskcurveL && pSpot.LmaskcurveL == otherSpot.LmaskcurveL;
                locallab.spots.at(j).recothresl = locallab.spots.at(j).recothresl && pSpot.recothresl == otherSpot.recothresl;
                locallab.spots.at(j).lowthresl = locallab.spots.at(j).lowthresl && pSpot.lowthresl == otherSpot.lowthresl;
                locallab.spots.at(j).higthresl = locallab.spots.at(j).higthresl && pSpot.higthresl == otherSpot.higthresl;
                locallab.spots.at(j).decayl = locallab.spots.at(j).decayl && pSpot.decayl == otherSpot.decayl;


                //mask
                locallab.spots.at(j).visimask = locallab.spots.at(j).visimask && pSpot.visimask == otherSpot.visimask;
                locallab.spots.at(j).complexmask = locallab.spots.at(j).complexmask && pSpot.complexmask == otherSpot.complexmask;
                locallab.spots.at(j).expmask = locallab.spots.at(j).expmask && pSpot.expmask == otherSpot.expmask;
                locallab.spots.at(j).sensimask = locallab.spots.at(j).sensimask && pSpot.sensimask == otherSpot.sensimask;
                locallab.spots.at(j).blendmask = locallab.spots.at(j).blendmask && pSpot.blendmask == otherSpot.blendmask;
                locallab.spots.at(j).blendmaskab = locallab.spots.at(j).blendmaskab && pSpot.blendmaskab == otherSpot.blendmaskab;
                locallab.spots.at(j).softradiusmask = locallab.spots.at(j).softradiusmask && pSpot.softradiusmask == otherSpot.softradiusmask;
                locallab.spots.at(j).enamask = locallab.spots.at(j).enamask && pSpot.enamask == otherSpot.enamask;
                locallab.spots.at(j).fftmask = locallab.spots.at(j).fftmask && pSpot.fftmask == otherSpot.fftmask;
                locallab.spots.at(j).blurmask = locallab.spots.at(j).blurmask && pSpot.blurmask == otherSpot.blurmask;
                locallab.spots.at(j).contmask = locallab.spots.at(j).contmask && pSpot.contmask == otherSpot.contmask;
                locallab.spots.at(j).CCmask_curve = locallab.spots.at(j).CCmask_curve && pSpot.CCmask_curve == otherSpot.CCmask_curve;
                locallab.spots.at(j).LLmask_curve = locallab.spots.at(j).LLmask_curve && pSpot.LLmask_curve == otherSpot.LLmask_curve;
                locallab.spots.at(j).HHmask_curve = locallab.spots.at(j).HHmask_curve && pSpot.HHmask_curve == otherSpot.HHmask_curve;
                locallab.spots.at(j).strumaskmask = locallab.spots.at(j).strumaskmask && pSpot.strumaskmask == otherSpot.strumaskmask;
                locallab.spots.at(j).toolmask = locallab.spots.at(j).toolmask && pSpot.toolmask == otherSpot.toolmask;
                locallab.spots.at(j).radmask = locallab.spots.at(j).radmask && pSpot.radmask == otherSpot.radmask;
                locallab.spots.at(j).lapmask = locallab.spots.at(j).lapmask && pSpot.lapmask == otherSpot.lapmask;
                locallab.spots.at(j).chromask = locallab.spots.at(j).chromask && pSpot.chromask == otherSpot.chromask;
                locallab.spots.at(j).gammask = locallab.spots.at(j).gammask && pSpot.gammask == otherSpot.gammask;
                locallab.spots.at(j).slopmask = locallab.spots.at(j).slopmask && pSpot.slopmask == otherSpot.slopmask;
                locallab.spots.at(j).shadmask = locallab.spots.at(j).shadmask && pSpot.shadmask == otherSpot.shadmask;
                locallab.spots.at(j).str_mask = locallab.spots.at(j).str_mask && pSpot.str_mask == otherSpot.str_mask;
                locallab.spots.at(j).ang_mask = locallab.spots.at(j).ang_mask && pSpot.ang_mask == otherSpot.ang_mask;
                locallab.spots.at(j).feather_mask = locallab.spots.at(j).feather_mask && pSpot.feather_mask == otherSpot.feather_mask;
                locallab.spots.at(j).HHhmask_curve = locallab.spots.at(j).HHhmask_curve && pSpot.HHhmask_curve == otherSpot.HHhmask_curve;
                locallab.spots.at(j).Lmask_curve = locallab.spots.at(j).Lmask_curve && pSpot.Lmask_curve == otherSpot.Lmask_curve;
                locallab.spots.at(j).LLmask_curvewav = locallab.spots.at(j).LLmask_curvewav && pSpot.LLmask_curvewav == otherSpot.LLmask_curvewav;
                locallab.spots.at(j).csthresholdmask = locallab.spots.at(j).csthresholdmask && pSpot.csthresholdmask == otherSpot.csthresholdmask;

                //ciecam
                locallab.spots.at(j).visicie = locallab.spots.at(j).visicie && pSpot.visicie == otherSpot.visicie;
                locallab.spots.at(j).expcie = locallab.spots.at(j).expcie && pSpot.expcie == otherSpot.expcie;
                locallab.spots.at(j).expprecam = locallab.spots.at(j).expprecam && pSpot.expprecam == otherSpot.expprecam;
                locallab.spots.at(j).complexcie = locallab.spots.at(j).complexcie && pSpot.complexcie == otherSpot.complexcie;
                locallab.spots.at(j).reparcie = locallab.spots.at(j).reparcie && pSpot.reparcie == otherSpot.reparcie;
                locallab.spots.at(j).sensicie = locallab.spots.at(j).sensicie && pSpot.sensicie == otherSpot.sensicie;
                locallab.spots.at(j).Autograycie = locallab.spots.at(j).Autograycie && pSpot.Autograycie == otherSpot.Autograycie;
                locallab.spots.at(j).sigybjz12 = locallab.spots.at(j).sigybjz12 && pSpot.sigybjz12 == otherSpot.sigybjz12;
                locallab.spots.at(j).qtoj = locallab.spots.at(j).qtoj && pSpot.qtoj == otherSpot.qtoj;
                locallab.spots.at(j).jabcie = locallab.spots.at(j).jabcie && pSpot.jabcie == otherSpot.jabcie;
                locallab.spots.at(j).comprcieauto = locallab.spots.at(j).comprcieauto && pSpot.comprcieauto == otherSpot.comprcieauto;
                locallab.spots.at(j).normcie12 = locallab.spots.at(j).normcie12 && pSpot.normcie12 == otherSpot.normcie12;
                locallab.spots.at(j).normcie = locallab.spots.at(j).normcie && pSpot.normcie == otherSpot.normcie;
                locallab.spots.at(j).gamutcie = locallab.spots.at(j).gamutcie && pSpot.gamutcie == otherSpot.gamutcie;
                locallab.spots.at(j).bwcie = locallab.spots.at(j).bwcie && pSpot.bwcie == otherSpot.bwcie;
                locallab.spots.at(j).sigcie = locallab.spots.at(j).sigcie && pSpot.sigcie == otherSpot.sigcie;
                locallab.spots.at(j).logcie = locallab.spots.at(j).logcie && pSpot.logcie == otherSpot.logcie;
                locallab.spots.at(j).satcie = locallab.spots.at(j).satcie && pSpot.satcie == otherSpot.satcie;
                locallab.spots.at(j).logcieq = locallab.spots.at(j).logcieq && pSpot.logcieq == otherSpot.logcieq;
                locallab.spots.at(j).smoothcie = locallab.spots.at(j).smoothcie && pSpot.smoothcie == otherSpot.smoothcie;
                locallab.spots.at(j).smoothcietrc = locallab.spots.at(j).smoothcietrc && pSpot.smoothcietrc == otherSpot.smoothcietrc;
                locallab.spots.at(j).smoothcietrcrel = locallab.spots.at(j).smoothcietrcrel && pSpot.smoothcietrcrel == otherSpot.smoothcietrcrel;
                locallab.spots.at(j).smoothcieyb = locallab.spots.at(j).smoothcieyb && pSpot.smoothcieyb == otherSpot.smoothcieyb;
                locallab.spots.at(j).smoothcielum = locallab.spots.at(j).smoothcielum && pSpot.smoothcielum == otherSpot.smoothcielum;
                locallab.spots.at(j).smoothciehigh = locallab.spots.at(j).smoothciehigh && pSpot.smoothciehigh == otherSpot.smoothciehigh;
                locallab.spots.at(j).smoothcielnk = locallab.spots.at(j).smoothcielnk && pSpot.smoothcielnk == otherSpot.smoothcielnk;
                locallab.spots.at(j).smoothcieinv = locallab.spots.at(j).smoothcieinv && pSpot.smoothcieinv == otherSpot.smoothcieinv;
                locallab.spots.at(j).logjz = locallab.spots.at(j).logjz && pSpot.logjz == otherSpot.logjz;
                locallab.spots.at(j).sigjz12 = locallab.spots.at(j).sigjz12 && pSpot.sigjz12 == otherSpot.sigjz12;
                locallab.spots.at(j).sigjz = locallab.spots.at(j).sigjz && pSpot.sigjz == otherSpot.sigjz;
                locallab.spots.at(j).forcebw = locallab.spots.at(j).forcebw && pSpot.forcebw == otherSpot.forcebw;
                locallab.spots.at(j).sigq12 = locallab.spots.at(j).sigq12 && pSpot.sigq12 == otherSpot.sigq12;
                locallab.spots.at(j).sigq = locallab.spots.at(j).sigq && pSpot.sigq == otherSpot.sigq;
                locallab.spots.at(j).chjzcie = locallab.spots.at(j).chjzcie && pSpot.chjzcie == otherSpot.chjzcie;
                locallab.spots.at(j).sourceGraycie = locallab.spots.at(j).sourceGraycie && pSpot.sourceGraycie == otherSpot.sourceGraycie;
                locallab.spots.at(j).sourceabscie = locallab.spots.at(j).sourceabscie && pSpot.sourceabscie == otherSpot.sourceabscie;
                locallab.spots.at(j).sursourcie = locallab.spots.at(j).sursourcie && pSpot.sursourcie == otherSpot.sursourcie;
                locallab.spots.at(j).modecam = locallab.spots.at(j).modecam && pSpot.modecam == otherSpot.modecam;
                locallab.spots.at(j).modeQJ = locallab.spots.at(j).modeQJ && pSpot.modeQJ == otherSpot.modeQJ;
                locallab.spots.at(j).bwevMethod12 = locallab.spots.at(j).bwevMethod12 && pSpot.bwevMethod12 == otherSpot.bwevMethod12;
                locallab.spots.at(j).bwevMethod = locallab.spots.at(j).bwevMethod && pSpot.bwevMethod == otherSpot.bwevMethod;
                locallab.spots.at(j).modecie = locallab.spots.at(j).modecie && pSpot.modecie == otherSpot.modecie;
                locallab.spots.at(j).saturlcie = locallab.spots.at(j).saturlcie && pSpot.saturlcie == otherSpot.saturlcie;
                locallab.spots.at(j).rstprotectcie = locallab.spots.at(j).rstprotectcie && pSpot.rstprotectcie == otherSpot.rstprotectcie;
                locallab.spots.at(j).chromlcie = locallab.spots.at(j).chromlcie && pSpot.chromlcie == otherSpot.chromlcie;
                locallab.spots.at(j).huecie = locallab.spots.at(j).huecie && pSpot.huecie == otherSpot.huecie;
                locallab.spots.at(j).toneMethodcie = locallab.spots.at(j).toneMethodcie && pSpot.toneMethodcie == otherSpot.toneMethodcie;
                locallab.spots.at(j).ciecurve = locallab.spots.at(j).ciecurve && pSpot.ciecurve == otherSpot.ciecurve;
                locallab.spots.at(j).toneMethodcie2 = locallab.spots.at(j).toneMethodcie2 && pSpot.toneMethodcie2 == otherSpot.toneMethodcie2;
                locallab.spots.at(j).ciecurve2 = locallab.spots.at(j).ciecurve2 && pSpot.ciecurve2 == otherSpot.ciecurve2;
                locallab.spots.at(j).chromjzcie = locallab.spots.at(j).chromjzcie && pSpot.chromjzcie == otherSpot.chromjzcie;
                locallab.spots.at(j).saturjzcie = locallab.spots.at(j).saturjzcie && pSpot.saturjzcie == otherSpot.saturjzcie;
                locallab.spots.at(j).huejzcie = locallab.spots.at(j).huejzcie && pSpot.huejzcie == otherSpot.huejzcie;
                locallab.spots.at(j).softjzcie = locallab.spots.at(j).softjzcie && pSpot.softjzcie == otherSpot.softjzcie;
                locallab.spots.at(j).strsoftjzcie = locallab.spots.at(j).strsoftjzcie && pSpot.strsoftjzcie == otherSpot.strsoftjzcie;
                locallab.spots.at(j).thrhjzcie = locallab.spots.at(j).thrhjzcie && pSpot.thrhjzcie == otherSpot.thrhjzcie;
                locallab.spots.at(j).jzcurve = locallab.spots.at(j).jzcurve && pSpot.jzcurve == otherSpot.jzcurve;
                locallab.spots.at(j).czcurve = locallab.spots.at(j).czcurve && pSpot.czcurve == otherSpot.czcurve;
                locallab.spots.at(j).czjzcurve = locallab.spots.at(j).czjzcurve && pSpot.czjzcurve == otherSpot.czjzcurve;
                locallab.spots.at(j).invcurve = locallab.spots.at(j).invcurve && pSpot.invcurve == otherSpot.invcurve;
                locallab.spots.at(j).HHcurvejz = locallab.spots.at(j).HHcurvejz && pSpot.HHcurvejz == otherSpot.HHcurvejz;
                locallab.spots.at(j).CHcurvejz = locallab.spots.at(j).CHcurvejz && pSpot.CHcurvejz == otherSpot.CHcurvejz;
                locallab.spots.at(j).LHcurvejz = locallab.spots.at(j).LHcurvejz && pSpot.LHcurvejz == otherSpot.LHcurvejz;
                locallab.spots.at(j).lightlcie = locallab.spots.at(j).lightlcie && pSpot.lightlcie == otherSpot.lightlcie;
                locallab.spots.at(j).lightjzcie = locallab.spots.at(j).lightjzcie && pSpot.lightjzcie == otherSpot.lightjzcie;
                locallab.spots.at(j).lightqcie = locallab.spots.at(j).lightqcie && pSpot.lightqcie == otherSpot.lightqcie;
                locallab.spots.at(j).lightsigqcie = locallab.spots.at(j).lightsigqcie && pSpot.lightsigqcie == otherSpot.lightsigqcie;
                locallab.spots.at(j).contlcie = locallab.spots.at(j).contlcie && pSpot.contlcie == otherSpot.contlcie;
                locallab.spots.at(j).contjzcie = locallab.spots.at(j).contjzcie && pSpot.contjzcie == otherSpot.contjzcie;
                locallab.spots.at(j).detailciejz = locallab.spots.at(j).detailciejz && pSpot.detailciejz == otherSpot.detailciejz;
                locallab.spots.at(j).adapjzcie = locallab.spots.at(j).adapjzcie && pSpot.adapjzcie == otherSpot.adapjzcie;
                locallab.spots.at(j).jz100 = locallab.spots.at(j).jz100 && pSpot.jz100 == otherSpot.jz100;
                locallab.spots.at(j).pqremap = locallab.spots.at(j).pqremap && pSpot.pqremap == otherSpot.pqremap;
                locallab.spots.at(j).pqremapcam16 = locallab.spots.at(j).pqremapcam16 && pSpot.pqremapcam16 == otherSpot.pqremapcam16;
                locallab.spots.at(j).hljzcie = locallab.spots.at(j).hljzcie && pSpot.hljzcie == otherSpot.hljzcie;
                locallab.spots.at(j).hlthjzcie = locallab.spots.at(j).hlthjzcie && pSpot.hlthjzcie == otherSpot.hlthjzcie;
                locallab.spots.at(j).shjzcie = locallab.spots.at(j).shjzcie && pSpot.shjzcie == otherSpot.shjzcie;
                locallab.spots.at(j).shthjzcie = locallab.spots.at(j).shthjzcie && pSpot.shthjzcie == otherSpot.shthjzcie;
                locallab.spots.at(j).radjzcie = locallab.spots.at(j).radjzcie && pSpot.radjzcie == otherSpot.radjzcie;
                locallab.spots.at(j).contthrescie = locallab.spots.at(j).contthrescie && pSpot.contthrescie == otherSpot.contthrescie;
                locallab.spots.at(j).blackEvjz = locallab.spots.at(j).blackEvjz && pSpot.blackEvjz == otherSpot.blackEvjz;
                locallab.spots.at(j).whiteEvjz = locallab.spots.at(j).whiteEvjz && pSpot.whiteEvjz == otherSpot.whiteEvjz;
                locallab.spots.at(j).targetjz = locallab.spots.at(j).targetjz && pSpot.targetjz == otherSpot.targetjz;
                locallab.spots.at(j).sigmoidldacie12 = locallab.spots.at(j).sigmoidldacie12 && pSpot.sigmoidldacie12 == otherSpot.sigmoidldacie12;
                locallab.spots.at(j).sigmoidthcie12 = locallab.spots.at(j).sigmoidthcie12 && pSpot.sigmoidthcie12 == otherSpot.sigmoidthcie12;
                locallab.spots.at(j).sigmoidblcie12 = locallab.spots.at(j).sigmoidblcie12 && pSpot.sigmoidblcie12 == otherSpot.sigmoidblcie12;

                locallab.spots.at(j).sigmoidldacie = locallab.spots.at(j).sigmoidldacie && pSpot.sigmoidldacie == otherSpot.sigmoidldacie;
                locallab.spots.at(j).sigmoidthcie = locallab.spots.at(j).sigmoidthcie && pSpot.sigmoidthcie == otherSpot.sigmoidthcie;
                locallab.spots.at(j).sigmoidsenscie = locallab.spots.at(j).sigmoidsenscie && pSpot.sigmoidsenscie == otherSpot.sigmoidsenscie;
                locallab.spots.at(j).sigmoidblcie = locallab.spots.at(j).sigmoidblcie && pSpot.sigmoidblcie == otherSpot.sigmoidblcie;

                locallab.spots.at(j).comprcie = locallab.spots.at(j).comprcie && pSpot.comprcie == otherSpot.comprcie;
                locallab.spots.at(j).strcielog = locallab.spots.at(j).strcielog && pSpot.strcielog == otherSpot.strcielog;
                locallab.spots.at(j).comprcieth = locallab.spots.at(j).comprcieth && pSpot.comprcieth == otherSpot.comprcieth;
                locallab.spots.at(j).gamjcie = locallab.spots.at(j).gamjcie && pSpot.gamjcie == otherSpot.gamjcie;
                locallab.spots.at(j).smoothcieth = locallab.spots.at(j).smoothcieth && pSpot.smoothcieth == otherSpot.smoothcieth;
                locallab.spots.at(j).smoothciethtrc = locallab.spots.at(j).smoothciethtrc && pSpot.smoothciethtrc == otherSpot.smoothciethtrc;
                locallab.spots.at(j).slopjcie = locallab.spots.at(j).slopjcie && pSpot.slopjcie == otherSpot.slopjcie;
                locallab.spots.at(j).satjcie = locallab.spots.at(j).satjcie && pSpot.satjcie == otherSpot.satjcie;
                locallab.spots.at(j).slopesmo = locallab.spots.at(j).slopesmo && pSpot.slopesmo == otherSpot.slopesmo;
                locallab.spots.at(j).slopesmoq = locallab.spots.at(j).slopesmoq && pSpot.slopesmoq == otherSpot.slopesmoq;
                locallab.spots.at(j).slopesmor = locallab.spots.at(j).slopesmor && pSpot.slopesmor == otherSpot.slopesmor;
                locallab.spots.at(j).slopesmog = locallab.spots.at(j).slopesmog && pSpot.slopesmog == otherSpot.slopesmog;
                locallab.spots.at(j).slopesmob = locallab.spots.at(j).slopesmob && pSpot.slopesmob == otherSpot.slopesmob;
                locallab.spots.at(j).contsig = locallab.spots.at(j).contsig && pSpot.contsig == otherSpot.contsig;
                locallab.spots.at(j).skewsig = locallab.spots.at(j).skewsig && pSpot.skewsig == otherSpot.skewsig;
                locallab.spots.at(j).whitsig = locallab.spots.at(j).whitsig && pSpot.whitsig == otherSpot.whitsig;

                locallab.spots.at(j).kslopesmor = locallab.spots.at(j).kslopesmor && pSpot.kslopesmor == otherSpot.kslopesmor;
                locallab.spots.at(j).kslopesmog = locallab.spots.at(j).kslopesmog && pSpot.kslopesmog == otherSpot.kslopesmog;
                locallab.spots.at(j).kslopesmob = locallab.spots.at(j).kslopesmob && pSpot.kslopesmob == otherSpot.kslopesmob;
                locallab.spots.at(j).midtciemet = locallab.spots.at(j).midtciemet && pSpot.midtciemet == otherSpot.midtciemet;
                locallab.spots.at(j).midtcie = locallab.spots.at(j).midtcie && pSpot.midtcie == otherSpot.midtcie;
                locallab.spots.at(j).grexl = locallab.spots.at(j).grexl && pSpot.grexl == otherSpot.grexl;
                locallab.spots.at(j).greyl = locallab.spots.at(j).greyl && pSpot.greyl == otherSpot.greyl;
                locallab.spots.at(j).redxl = locallab.spots.at(j).redxl && pSpot.redxl == otherSpot.redxl;
                locallab.spots.at(j).redyl = locallab.spots.at(j).redyl && pSpot.redyl == otherSpot.redyl;
                locallab.spots.at(j).bluxl = locallab.spots.at(j).bluxl && pSpot.bluxl == otherSpot.bluxl;
                locallab.spots.at(j).bluyl = locallab.spots.at(j).bluyl && pSpot.bluyl == otherSpot.bluyl;
                locallab.spots.at(j).refi = locallab.spots.at(j).refi && pSpot.refi == otherSpot.refi;
                locallab.spots.at(j).shiftxl = locallab.spots.at(j).shiftxl && pSpot.shiftxl == otherSpot.shiftxl;
                locallab.spots.at(j).shiftyl = locallab.spots.at(j).shiftyl && pSpot.shiftyl == otherSpot.shiftyl;
                locallab.spots.at(j).labgridcieALow = locallab.spots.at(j).labgridcieALow && pSpot.labgridcieALow == otherSpot.labgridcieALow;
                locallab.spots.at(j).labgridcieBLow = locallab.spots.at(j).labgridcieBLow && pSpot.labgridcieBLow == otherSpot.labgridcieBLow;
                locallab.spots.at(j).labgridcieAHigh = locallab.spots.at(j).labgridcieAHigh && pSpot.labgridcieAHigh == otherSpot.labgridcieAHigh;
                locallab.spots.at(j).labgridcieBHigh = locallab.spots.at(j).labgridcieBHigh && pSpot.labgridcieBHigh == otherSpot.labgridcieBHigh;
                locallab.spots.at(j).labgridcieGx = locallab.spots.at(j).labgridcieGx && pSpot.labgridcieGx == otherSpot.labgridcieGx;
                locallab.spots.at(j).labgridcieGy = locallab.spots.at(j).labgridcieGy && pSpot.labgridcieGy == otherSpot.labgridcieGy;
                locallab.spots.at(j).labgridcieWx = locallab.spots.at(j).labgridcieWx && pSpot.labgridcieWx == otherSpot.labgridcieWx;
                locallab.spots.at(j).labgridcieWy = locallab.spots.at(j).labgridcieWy && pSpot.labgridcieWy == otherSpot.labgridcieWy;
                locallab.spots.at(j).labgridcieMx = locallab.spots.at(j).labgridcieMx && pSpot.labgridcieMx == otherSpot.labgridcieMx;
                locallab.spots.at(j).labgridcieMy = locallab.spots.at(j).labgridcieMy && pSpot.labgridcieMy == otherSpot.labgridcieMy;

                locallab.spots.at(j).whitescie = locallab.spots.at(j).whitescie && pSpot.whitescie == otherSpot.whitescie;
                locallab.spots.at(j).blackscie = locallab.spots.at(j).blackscie && pSpot.blackscie == otherSpot.blackscie;
                locallab.spots.at(j).illMethod = locallab.spots.at(j).illMethod && pSpot.illMethod == otherSpot.illMethod;
                locallab.spots.at(j).smoothciemet = locallab.spots.at(j).smoothciemet && pSpot.smoothciemet == otherSpot.smoothciemet;
                locallab.spots.at(j).primMethod = locallab.spots.at(j).primMethod && pSpot.primMethod == otherSpot.primMethod;
                locallab.spots.at(j).catMethod = locallab.spots.at(j).catMethod && pSpot.catMethod == otherSpot.catMethod;
                locallab.spots.at(j).sigmoidldajzcie12 = locallab.spots.at(j).sigmoidldajzcie12 && pSpot.sigmoidldajzcie12 == otherSpot.sigmoidldajzcie12;
                locallab.spots.at(j).sigmoidthjzcie12 = locallab.spots.at(j).sigmoidthjzcie12 && pSpot.sigmoidthjzcie12 == otherSpot.sigmoidthjzcie12;
                locallab.spots.at(j).sigmoidbljzcie12 = locallab.spots.at(j).sigmoidbljzcie12 && pSpot.sigmoidbljzcie12 == otherSpot.sigmoidbljzcie12;

                locallab.spots.at(j).sigmoidldajzcie = locallab.spots.at(j).sigmoidldajzcie && pSpot.sigmoidldajzcie == otherSpot.sigmoidldajzcie;
                locallab.spots.at(j).sigmoidthjzcie = locallab.spots.at(j).sigmoidthjzcie && pSpot.sigmoidthjzcie == otherSpot.sigmoidthjzcie;
                locallab.spots.at(j).sigmoidbljzcie = locallab.spots.at(j).sigmoidbljzcie && pSpot.sigmoidbljzcie == otherSpot.sigmoidbljzcie;

                locallab.spots.at(j).contqcie = locallab.spots.at(j).contqcie && pSpot.contqcie == otherSpot.contqcie;
                locallab.spots.at(j).contsigqcie = locallab.spots.at(j).contsigqcie && pSpot.contsigqcie == otherSpot.contsigqcie;
                locallab.spots.at(j).colorflcie = locallab.spots.at(j).colorflcie && pSpot.colorflcie == otherSpot.colorflcie;
                locallab.spots.at(j).targabscie = locallab.spots.at(j).targabscie && pSpot.targabscie == otherSpot.targabscie;
                locallab.spots.at(j).targetGraycie = locallab.spots.at(j).targetGraycie && pSpot.targetGraycie == otherSpot.targetGraycie;
                locallab.spots.at(j).catadcie = locallab.spots.at(j).catadcie && pSpot.catadcie == otherSpot.catadcie;
                locallab.spots.at(j).detailcie = locallab.spots.at(j).detailcie && pSpot.detailcie == otherSpot.detailcie;
                locallab.spots.at(j).surroundcie = locallab.spots.at(j).surroundcie && pSpot.surroundcie == otherSpot.surroundcie;
                
                locallab.spots.at(j).strgradcie = locallab.spots.at(j).strgradcie && pSpot.strgradcie == otherSpot.strgradcie;
                locallab.spots.at(j).anggradcie = locallab.spots.at(j).anggradcie && pSpot.anggradcie == otherSpot.anggradcie;
                locallab.spots.at(j).feathercie = locallab.spots.at(j).feathercie && pSpot.feathercie == otherSpot.feathercie;

                locallab.spots.at(j).enacieMask = locallab.spots.at(j).enacieMask && pSpot.enacieMask == otherSpot.enacieMask;
                locallab.spots.at(j).enacieMaskall = locallab.spots.at(j).enacieMaskall && pSpot.enacieMaskall == otherSpot.enacieMaskall;
                locallab.spots.at(j).CCmaskciecurve = locallab.spots.at(j).CCmaskciecurve && pSpot.CCmaskciecurve == otherSpot.CCmaskciecurve;
                locallab.spots.at(j).LLmaskciecurve = locallab.spots.at(j).LLmaskciecurve && pSpot.LLmaskciecurve == otherSpot.LLmaskciecurve;
                locallab.spots.at(j).HHmaskciecurve = locallab.spots.at(j).HHmaskciecurve && pSpot.HHmaskciecurve == otherSpot.HHmaskciecurve;
                locallab.spots.at(j).HHhmaskciecurve = locallab.spots.at(j).HHhmaskciecurve && pSpot.HHhmaskciecurve == otherSpot.HHhmaskciecurve;
                locallab.spots.at(j).blendmaskcie = locallab.spots.at(j).blendmaskcie && pSpot.blendmaskcie == otherSpot.blendmaskcie;
                locallab.spots.at(j).radmaskcie = locallab.spots.at(j).radmaskcie && pSpot.radmaskcie == otherSpot.radmaskcie;
                locallab.spots.at(j).chromaskcie = locallab.spots.at(j).chromaskcie && pSpot.chromaskcie == otherSpot.chromaskcie;
                locallab.spots.at(j).lapmaskcie = locallab.spots.at(j).lapmaskcie && pSpot.lapmaskcie == otherSpot.lapmaskcie;
                locallab.spots.at(j).gammaskcie = locallab.spots.at(j).gammaskcie && pSpot.gammaskcie == otherSpot.gammaskcie;
                locallab.spots.at(j).slomaskcie = locallab.spots.at(j).slomaskcie && pSpot.slomaskcie == otherSpot.slomaskcie;
                locallab.spots.at(j).Lmaskciecurve = locallab.spots.at(j).Lmaskciecurve && pSpot.Lmaskciecurve == otherSpot.Lmaskciecurve;
                locallab.spots.at(j).recothrescie = locallab.spots.at(j).recothrescie && pSpot.recothrescie == otherSpot.recothrescie;
                locallab.spots.at(j).lowthrescie = locallab.spots.at(j).lowthrescie && pSpot.lowthrescie == otherSpot.lowthrescie;
                locallab.spots.at(j).higthrescie = locallab.spots.at(j).higthrescie && pSpot.higthrescie == otherSpot.higthrescie;
                locallab.spots.at(j).decaycie = locallab.spots.at(j).decaycie && pSpot.decaycie == otherSpot.decaycie;
                locallab.spots.at(j).locwavcurvejz = locallab.spots.at(j).locwavcurvejz && pSpot.locwavcurvejz == otherSpot.locwavcurvejz;
                locallab.spots.at(j).csthresholdjz = locallab.spots.at(j).csthresholdjz && pSpot.csthresholdjz == otherSpot.csthresholdjz;
                locallab.spots.at(j).sigmalcjz = locallab.spots.at(j).sigmalcjz && pSpot.sigmalcjz == otherSpot.sigmalcjz;
                locallab.spots.at(j).clarilresjz = locallab.spots.at(j).clarilresjz && pSpot.clarilresjz == otherSpot.clarilresjz;
                locallab.spots.at(j).claricresjz = locallab.spots.at(j).claricresjz && pSpot.claricresjz == otherSpot.claricresjz;
                locallab.spots.at(j).clarisoftjz = locallab.spots.at(j).clarisoftjz && pSpot.clarisoftjz == otherSpot.clarisoftjz;
                locallab.spots.at(j).strumaskcie = locallab.spots.at(j).strumaskcie && pSpot.strumaskcie == otherSpot.strumaskcie;
                locallab.spots.at(j).toolcie = locallab.spots.at(j).toolcie && pSpot.toolcie == otherSpot.toolcie;
                locallab.spots.at(j).fftcieMask = locallab.spots.at(j).fftcieMask && pSpot.fftcieMask == otherSpot.fftcieMask;
                locallab.spots.at(j).blurcie = locallab.spots.at(j).blurcie && pSpot.blurcie == otherSpot.blurcie;
                locallab.spots.at(j).contcie = locallab.spots.at(j).contcie && pSpot.contcie == otherSpot.contcie;
                locallab.spots.at(j).highmaskcie = locallab.spots.at(j).highmaskcie && pSpot.highmaskcie == otherSpot.highmaskcie;
                locallab.spots.at(j).shadmaskcie = locallab.spots.at(j).shadmaskcie && pSpot.shadmaskcie == otherSpot.shadmaskcie;
                locallab.spots.at(j).LLmaskciecurvewav = locallab.spots.at(j).LLmaskciecurvewav && pSpot.LLmaskciecurvewav == otherSpot.LLmaskciecurvewav;
                locallab.spots.at(j).csthresholdcie = locallab.spots.at(j).csthresholdcie && pSpot.csthresholdcie == otherSpot.csthresholdcie;


            }
        }

        if (!isSpotNumberEqual) {
            // All LocallabSpotEdited are set to false because cannot be combined
            locallab.spots.clear();
            locallab.spots.resize(p.locallab.spots.size(), LocallabParamsEdited::LocallabSpotEdited(false));
        }

        pcvignette.enabled = pcvignette.enabled && p.pcvignette.enabled == other.pcvignette.enabled;
        pcvignette.strength = pcvignette.strength && p.pcvignette.strength == other.pcvignette.strength;
        pcvignette.feather = pcvignette.feather && p.pcvignette.feather == other.pcvignette.feather;
        pcvignette.roundness = pcvignette.roundness && p.pcvignette.roundness == other.pcvignette.roundness;
        cacorrection.red = cacorrection.red && p.cacorrection.red == other.cacorrection.red;
        cacorrection.blue = cacorrection.blue && p.cacorrection.blue == other.cacorrection.blue;
        vignetting.amount = vignetting.amount && p.vignetting.amount == other.vignetting.amount;
        vignetting.radius = vignetting.radius && p.vignetting.radius == other.vignetting.radius;
        vignetting.strength = vignetting.strength && p.vignetting.strength == other.vignetting.strength;
        vignetting.centerX = vignetting.centerX && p.vignetting.centerX == other.vignetting.centerX;
        vignetting.centerY = vignetting.centerY && p.vignetting.centerY == other.vignetting.centerY;
        chmixer.enabled = chmixer.enabled && p.chmixer.enabled == other.chmixer.enabled;
        chmixer.red[0] = chmixer.red[0] && p.chmixer.red[0] == other.chmixer.red[0];
        chmixer.red[1] = chmixer.red[1] && p.chmixer.red[1] == other.chmixer.red[1];
        chmixer.red[2] = chmixer.red[2] && p.chmixer.red[2] == other.chmixer.red[2];
        chmixer.green[0] = chmixer.green[0] && p.chmixer.green[0] == other.chmixer.green[0];
        chmixer.green[1] = chmixer.green[1] && p.chmixer.green[1] == other.chmixer.green[1];
        chmixer.green[2] = chmixer.green[2] && p.chmixer.green[2] == other.chmixer.green[2];
        chmixer.blue[0] = chmixer.blue[0] && p.chmixer.blue[0] == other.chmixer.blue[0];
        chmixer.blue[1] = chmixer.blue[1] && p.chmixer.blue[1] == other.chmixer.blue[1];
        chmixer.blue[2] = chmixer.blue[2] && p.chmixer.blue[2] == other.chmixer.blue[2];
        blackwhite.enabledcc = blackwhite.enabledcc && p.blackwhite.enabledcc == other.blackwhite.enabledcc;
        blackwhite.enabled = blackwhite.enabled && p.blackwhite.enabled == other.blackwhite.enabled;
        blackwhite.mixerRed = blackwhite.mixerRed && p.blackwhite.mixerRed == other.blackwhite.mixerRed;
        blackwhite.mixerOrange = blackwhite.mixerOrange && p.blackwhite.mixerOrange == other.blackwhite.mixerOrange;
        blackwhite.mixerYellow = blackwhite.mixerYellow && p.blackwhite.mixerYellow == other.blackwhite.mixerYellow;
        blackwhite.mixerGreen = blackwhite.mixerGreen && p.blackwhite.mixerGreen == other.blackwhite.mixerGreen;
        blackwhite.mixerCyan = blackwhite.mixerCyan && p.blackwhite.mixerCyan == other.blackwhite.mixerCyan;
        blackwhite.mixerBlue = blackwhite.mixerBlue && p.blackwhite.mixerBlue == other.blackwhite.mixerBlue;
        blackwhite.mixerMagenta = blackwhite.mixerMagenta && p.blackwhite.mixerMagenta == other.blackwhite.mixerMagenta;
        blackwhite.mixerPurple = blackwhite.mixerPurple && p.blackwhite.mixerPurple == other.blackwhite.mixerPurple;
        blackwhite.gammaRed = blackwhite.gammaRed && p.blackwhite.gammaRed == other.blackwhite.gammaRed;
        blackwhite.gammaGreen = blackwhite.gammaGreen && p.blackwhite.gammaGreen == other.blackwhite.gammaGreen;
        blackwhite.gammaBlue = blackwhite.gammaBlue && p.blackwhite.gammaBlue == other.blackwhite.gammaBlue;
        blackwhite.filter = blackwhite.filter && p.blackwhite.filter == other.blackwhite.filter;
        blackwhite.setting = blackwhite.setting && p.blackwhite.setting == other.blackwhite.setting;
        blackwhite.luminanceCurve = blackwhite.luminanceCurve && p.blackwhite.luminanceCurve == other.blackwhite.luminanceCurve;
        blackwhite.method = blackwhite.method && p.blackwhite.method == other.blackwhite.method;
        blackwhite.beforeCurve = blackwhite.beforeCurve && p.blackwhite.beforeCurve == other.blackwhite.beforeCurve;
        blackwhite.beforeCurveMode = blackwhite.beforeCurveMode && p.blackwhite.beforeCurveMode == other.blackwhite.beforeCurveMode;
        blackwhite.afterCurve = blackwhite.afterCurve && p.blackwhite.afterCurve == other.blackwhite.afterCurve;
        blackwhite.afterCurveMode = blackwhite.afterCurveMode && p.blackwhite.afterCurveMode == other.blackwhite.afterCurveMode;
        blackwhite.autoc = blackwhite.autoc && p.blackwhite.autoc == other.blackwhite.autoc;
        blackwhite.algo = blackwhite.algo && p.blackwhite.algo == other.blackwhite.algo;
        resize.scale = resize.scale && p.resize.scale == other.resize.scale;
        resize.appliesTo = resize.appliesTo && p.resize.appliesTo == other.resize.appliesTo;
        resize.method = resize.method && p.resize.method == other.resize.method;
        resize.dataspec = resize.dataspec && p.resize.dataspec == other.resize.dataspec;
        resize.width = resize.width && p.resize.width == other.resize.width;
        resize.height = resize.height && p.resize.height == other.resize.height;
        resize.longedge = resize.longedge && p.resize.longedge == other.resize.longedge;
        resize.shortedge = resize.shortedge && p.resize.shortedge == other.resize.shortedge;
        resize.enabled = resize.enabled && p.resize.enabled == other.resize.enabled;
        resize.allowUpscaling = resize.allowUpscaling && p.resize.allowUpscaling == other.resize.allowUpscaling;

        ::initFrom(framing, p, other);

        spot.enabled = spot.enabled && p.spot.enabled == other.spot.enabled;
        spot.entries = spot.entries && p.spot.entries == other.spot.entries;
        icm.inputProfile = icm.inputProfile && p.icm.inputProfile == other.icm.inputProfile;
        icm.toneCurve = icm.toneCurve && p.icm.toneCurve == other.icm.toneCurve;
        icm.applyLookTable = icm.applyLookTable && p.icm.applyLookTable == other.icm.applyLookTable;
        icm.applyBaselineExposureOffset = icm.applyBaselineExposureOffset && p.icm.applyBaselineExposureOffset == other.icm.applyBaselineExposureOffset;
        icm.applyHueSatMap = icm.applyHueSatMap && p.icm.applyHueSatMap == other.icm.applyHueSatMap;
        icm.dcpIlluminant = icm.dcpIlluminant && p.icm.dcpIlluminant == other.icm.dcpIlluminant;
        icm.workingProfile = icm.workingProfile && p.icm.workingProfile == other.icm.workingProfile;
        icm.outputProfile = icm.outputProfile && p.icm.outputProfile == other.icm.outputProfile;
        icm.outputIntent = icm.outputIntent && p.icm.outputIntent == other.icm.outputIntent;
        icm.outputBPC = icm.outputBPC && p.icm.outputBPC == other.icm.outputBPC ;
        icm.wGamma = icm.wGamma && p.icm.wGamma == other.icm.wGamma;
        icm.wSlope = icm.wSlope && p.icm.wSlope == other.icm.wSlope;
        icm.wmidtcie = icm.wmidtcie && p.icm.wmidtcie == other.icm.wmidtcie;
        icm.sigmatrc = icm.sigmatrc && p.icm.sigmatrc == other.icm.sigmatrc;
        icm.offstrc = icm.offstrc && p.icm.offstrc == other.icm.offstrc;
        icm.residtrc = icm.residtrc && p.icm.residtrc == other.icm.residtrc;
        icm.wgampower = icm.wgampower && p.icm.wgampower == other.icm.wgampower;
        icm.wgamgain = icm.wgamgain && p.icm.wgamgain == other.icm.wgamgain;
        icm.pyrwavtrc = icm.pyrwavtrc && p.icm.pyrwavtrc == other.icm.pyrwavtrc;
        icm.opacityCurveWLI = icm.opacityCurveWLI && p.icm.opacityCurveWLI == other.icm.opacityCurveWLI;
        icm.wsmoothcie = icm.wsmoothcie && p.icm.wsmoothcie == other.icm.wsmoothcie;
        icm.wapsat = icm.wapsat && p.icm.wapsat == other.icm.wapsat;
        icm.wsmoothciesli = icm.wsmoothciesli && p.icm.wsmoothciesli == other.icm.wsmoothciesli;
        icm.redx = icm.redx && p.icm.redx == other.icm.redx;
        icm.redy = icm.redy && p.icm.redy == other.icm.redy;
        icm.grex = icm.grex && p.icm.grex == other.icm.grex;
        icm.grey = icm.grey && p.icm.grey == other.icm.grey;
        icm.blux = icm.blux && p.icm.blux == other.icm.blux;
        icm.bluy = icm.bluy && p.icm.bluy == other.icm.bluy;
        
        icm.redrot = icm.redrot && p.icm.redrot == other.icm.redrot;
        icm.redsat = icm.redsat && p.icm.redsat == other.icm.redsat;
        icm.grerot = icm.grerot && p.icm.grerot == other.icm.grerot;
        icm.gresat = icm.gresat && p.icm.gresat == other.icm.gresat;
        icm.blurot = icm.blurot && p.icm.blurot == other.icm.blurot;
        icm.blusat = icm.blusat && p.icm.blusat == other.icm.blusat;
        
        icm.refi = icm.refi && p.icm.refi == other.icm.refi;
        icm.shiftx = icm.shiftx && p.icm.shiftx == other.icm.shiftx;
        icm.shifty = icm.shifty && p.icm.shifty == other.icm.shifty;
        icm.labgridcieALow = icm.labgridcieALow && p.icm.labgridcieALow == other.icm.labgridcieALow;
        icm.labgridcieBLow = icm.labgridcieBLow && p.icm.labgridcieBLow == other.icm.labgridcieBLow;
        icm.labgridcieAHigh = icm.labgridcieAHigh && p.icm.labgridcieAHigh == other.icm.labgridcieAHigh;
        icm.labgridcieBHigh = icm.labgridcieBHigh && p.icm.labgridcieBHigh == other.icm.labgridcieBHigh;
        icm.labgridcieGx = icm.labgridcieGx && p.icm.labgridcieGx == other.icm.labgridcieGx;
        icm.labgridcieGy = icm.labgridcieGy && p.icm.labgridcieGy == other.icm.labgridcieGy;
        icm.labgridcieWx = icm.labgridcieWx && p.icm.labgridcieWx == other.icm.labgridcieWx;
        icm.labgridcieWy = icm.labgridcieWy && p.icm.labgridcieWy == other.icm.labgridcieWy;
        icm.labgridcieMx = icm.labgridcieMx && p.icm.labgridcieMx == other.icm.labgridcieMx;
        icm.labgridcieMy = icm.labgridcieMy && p.icm.labgridcieMy == other.icm.labgridcieMy;
        icm.preser = icm.preser && p.icm.preser == other.icm.preser;
        icm.fbw = icm.fbw && p.icm.fbw == other.icm.fbw;
        icm.trcExp = icm.trcExp && p.icm.trcExp == other.icm.trcExp;
        icm.wavExp = icm.wavExp && p.icm.wavExp == other.icm.wavExp;
        icm.gamut = icm.gamut && p.icm.gamut == other.icm.gamut;
        icm.aRendIntent = icm.aRendIntent && p.icm.aRendIntent == other.icm.aRendIntent;
        icm.workingTRC = icm.workingTRC && p.icm.workingTRC == other.icm.workingTRC;
        icm.wgamut = icm.wgamut && p.icm.wgamut == other.icm.wgamut;
        icm.will = icm.will && p.icm.will == other.icm.will;
        icm.wprim = icm.wprim && p.icm.wprim == other.icm.wprim;
        icm.wcat = icm.wcat && p.icm.wcat == other.icm.wcat;
        raw.bayersensor.method = raw.bayersensor.method && p.raw.bayersensor.method == other.raw.bayersensor.method;
        raw.bayersensor.border = raw.bayersensor.border && p.raw.bayersensor.border == other.raw.bayersensor.border;
        raw.bayersensor.imageNum = raw.bayersensor.imageNum && p.raw.bayersensor.imageNum == other.raw.bayersensor.imageNum;
        raw.bayersensor.ccSteps = raw.bayersensor.ccSteps && p.raw.bayersensor.ccSteps == other.raw.bayersensor.ccSteps;
        raw.bayersensor.exBlack0 = raw.bayersensor.exBlack0 && p.raw.bayersensor.black0 == other.raw.bayersensor.black0;
        raw.bayersensor.exBlack1 = raw.bayersensor.exBlack1 && p.raw.bayersensor.black1 == other.raw.bayersensor.black1;
        raw.bayersensor.exBlack2 = raw.bayersensor.exBlack2 && p.raw.bayersensor.black2 == other.raw.bayersensor.black2;
        raw.bayersensor.exBlack3 = raw.bayersensor.exBlack3 && p.raw.bayersensor.black3 == other.raw.bayersensor.black3;
        raw.bayersensor.exTwoGreen = raw.bayersensor.exTwoGreen && p.raw.bayersensor.twogreen == other.raw.bayersensor.twogreen;
        raw.bayersensor.Dehablack = raw.bayersensor.Dehablack && p.raw.bayersensor.Dehablack == other.raw.bayersensor.Dehablack;
        raw.bayersensor.dcbIterations = raw.bayersensor.dcbIterations && p.raw.bayersensor.dcb_iterations == other.raw.bayersensor.dcb_iterations;
        raw.bayersensor.dcbEnhance = raw.bayersensor.dcbEnhance && p.raw.bayersensor.dcb_enhance == other.raw.bayersensor.dcb_enhance;
        //raw.bayersensor.allEnhance = raw.bayersensor.allEnhance && p.raw.bayersensor.all_enhance == other.raw.bayersensor.all_enhance;
        raw.bayersensor.lmmseIterations = raw.bayersensor.lmmseIterations && p.raw.bayersensor.lmmse_iterations == other.raw.bayersensor.lmmse_iterations;
        raw.bayersensor.dualDemosaicAutoContrast = raw.bayersensor.dualDemosaicAutoContrast && p.raw.bayersensor.dualDemosaicAutoContrast == other.raw.bayersensor.dualDemosaicAutoContrast;
        raw.bayersensor.dualDemosaicContrast = raw.bayersensor.dualDemosaicContrast && p.raw.bayersensor.dualDemosaicContrast == other.raw.bayersensor.dualDemosaicContrast;
        raw.bayersensor.pixelShiftMotionCorrectionMethod = raw.bayersensor.pixelShiftMotionCorrectionMethod && p.raw.bayersensor.pixelShiftMotionCorrectionMethod == other.raw.bayersensor.pixelShiftMotionCorrectionMethod;
        raw.bayersensor.pixelShiftEperIso = raw.bayersensor.pixelShiftEperIso && p.raw.bayersensor.pixelShiftEperIso == other.raw.bayersensor.pixelShiftEperIso;
        raw.bayersensor.pixelShiftSigma = raw.bayersensor.pixelShiftSigma && p.raw.bayersensor.pixelShiftSigma == other.raw.bayersensor.pixelShiftSigma;
        raw.bayersensor.pixelShiftShowMotion = raw.bayersensor.pixelShiftShowMotion && p.raw.bayersensor.pixelShiftShowMotion == other.raw.bayersensor.pixelShiftShowMotion;
        raw.bayersensor.pixelShiftShowMotionMaskOnly = raw.bayersensor.pixelShiftShowMotionMaskOnly && p.raw.bayersensor.pixelShiftShowMotionMaskOnly == other.raw.bayersensor.pixelShiftShowMotionMaskOnly;
        raw.bayersensor.pixelShiftHoleFill = raw.bayersensor.pixelShiftHoleFill && p.raw.bayersensor.pixelShiftHoleFill == other.raw.bayersensor.pixelShiftHoleFill;
        raw.bayersensor.pixelShiftMedian = raw.bayersensor.pixelShiftMedian && p.raw.bayersensor.pixelShiftMedian == other.raw.bayersensor.pixelShiftMedian;
        raw.bayersensor.pixelShiftAverage = raw.bayersensor.pixelShiftAverage && p.raw.bayersensor.pixelShiftAverage == other.raw.bayersensor.pixelShiftAverage;
        raw.bayersensor.pixelShiftGreen = raw.bayersensor.pixelShiftGreen && p.raw.bayersensor.pixelShiftGreen == other.raw.bayersensor.pixelShiftGreen;
        raw.bayersensor.pixelShiftBlur = raw.bayersensor.pixelShiftBlur && p.raw.bayersensor.pixelShiftBlur == other.raw.bayersensor.pixelShiftBlur;
        raw.bayersensor.pixelShiftSmooth = raw.bayersensor.pixelShiftSmooth && p.raw.bayersensor.pixelShiftSmoothFactor == other.raw.bayersensor.pixelShiftSmoothFactor;
        raw.bayersensor.pixelShiftEqualBright = raw.bayersensor.pixelShiftEqualBright && p.raw.bayersensor.pixelShiftEqualBright == other.raw.bayersensor.pixelShiftEqualBright;
        raw.bayersensor.pixelShiftEqualBrightChannel = raw.bayersensor.pixelShiftEqualBrightChannel && p.raw.bayersensor.pixelShiftEqualBrightChannel == other.raw.bayersensor.pixelShiftEqualBrightChannel;
        raw.bayersensor.pixelShiftNonGreenCross = raw.bayersensor.pixelShiftNonGreenCross && p.raw.bayersensor.pixelShiftNonGreenCross == other.raw.bayersensor.pixelShiftNonGreenCross;
        raw.bayersensor.pixelShiftDemosaicMethod = raw.bayersensor.pixelShiftDemosaicMethod && p.raw.bayersensor.pixelShiftDemosaicMethod == other.raw.bayersensor.pixelShiftDemosaicMethod;
        raw.bayersensor.greenEq = raw.bayersensor.greenEq && p.raw.bayersensor.greenthresh == other.raw.bayersensor.greenthresh;
        raw.bayersensor.linenoise = raw.bayersensor.linenoise && p.raw.bayersensor.linenoise == other.raw.bayersensor.linenoise;
        raw.bayersensor.linenoiseDirection = raw.bayersensor.linenoiseDirection && p.raw.bayersensor.linenoiseDirection == other.raw.bayersensor.linenoiseDirection;
        raw.bayersensor.pdafLinesFilter = raw.bayersensor.pdafLinesFilter && p.raw.bayersensor.pdafLinesFilter == other.raw.bayersensor.pdafLinesFilter;
        raw.xtranssensor.method = raw.xtranssensor.method && p.raw.xtranssensor.method == other.raw.xtranssensor.method;
        raw.xtranssensor.dualDemosaicAutoContrast = raw.xtranssensor.dualDemosaicAutoContrast && p.raw.xtranssensor.dualDemosaicAutoContrast == other.raw.xtranssensor.dualDemosaicAutoContrast;
        raw.xtranssensor.dualDemosaicContrast = raw.xtranssensor.dualDemosaicContrast && p.raw.xtranssensor.dualDemosaicContrast == other.raw.xtranssensor.dualDemosaicContrast;
        raw.xtranssensor.border = raw.xtranssensor.border && p.raw.xtranssensor.border == other.raw.xtranssensor.border;
        raw.xtranssensor.ccSteps = raw.xtranssensor.ccSteps && p.raw.xtranssensor.ccSteps == other.raw.xtranssensor.ccSteps;
        raw.xtranssensor.exBlackRed = raw.xtranssensor.exBlackRed && p.raw.xtranssensor.blackred == other.raw.xtranssensor.blackred;
        raw.xtranssensor.exBlackGreen = raw.xtranssensor.exBlackGreen && p.raw.xtranssensor.blackgreen == other.raw.xtranssensor.blackgreen;
        raw.xtranssensor.exBlackBlue = raw.xtranssensor.exBlackBlue && p.raw.xtranssensor.blackblue == other.raw.xtranssensor.blackblue;
        raw.xtranssensor.Dehablackx = raw.xtranssensor.Dehablackx && p.raw.xtranssensor.Dehablackx == other.raw.xtranssensor.Dehablackx;
        raw.ca_autocorrect = raw.ca_autocorrect && p.raw.ca_autocorrect == other.raw.ca_autocorrect;
        raw.ca_avoidcolourshift = raw.ca_avoidcolourshift && p.raw.ca_avoidcolourshift == other.raw.ca_avoidcolourshift;
        raw.caautoiterations = raw.caautoiterations && p.raw.caautoiterations == other.raw.caautoiterations;
        raw.cared = raw.cared && p.raw.cared == other.raw.cared;
        raw.cablue = raw.cablue && p.raw.cablue == other.raw.cablue;
        raw.hotPixelFilter = raw.hotPixelFilter && p.raw.hotPixelFilter == other.raw.hotPixelFilter;
        raw.deadPixelFilter = raw.deadPixelFilter && p.raw.deadPixelFilter == other.raw.deadPixelFilter;
        raw.hotdeadpix_thresh = raw.hotdeadpix_thresh && p.raw.hotdeadpix_thresh == other.raw.hotdeadpix_thresh;
        raw.darkFrame = raw.darkFrame && p.raw.dark_frame == other.raw.dark_frame;
        raw.df_autoselect = raw.df_autoselect && p.raw.df_autoselect == other.raw.df_autoselect;
        raw.ff_file = raw.ff_file && p.raw.ff_file == other.raw.ff_file;
        raw.ff_AutoSelect = raw.ff_AutoSelect && p.raw.ff_AutoSelect == other.raw.ff_AutoSelect;
        raw.ff_FromMetaData = raw.ff_FromMetaData && p.raw.ff_FromMetaData == other.raw.ff_FromMetaData;
        raw.ff_BlurRadius = raw.ff_BlurRadius && p.raw.ff_BlurRadius == other.raw.ff_BlurRadius;
        raw.ff_BlurType = raw.ff_BlurType && p.raw.ff_BlurType == other.raw.ff_BlurType;
        raw.ff_AutoClipControl = raw.ff_AutoClipControl && p.raw.ff_AutoClipControl == other.raw.ff_AutoClipControl;
        raw.ff_clipControl = raw.ff_clipControl && p.raw.ff_clipControl == other.raw.ff_clipControl;
        raw.exPos = raw.exPos && p.raw.expos == other.raw.expos;
        wavelet.enabled = wavelet.enabled && p.wavelet.enabled == other.wavelet.enabled;
        wavelet.strength = wavelet.strength && p.wavelet.strength == other.wavelet.strength;
        wavelet.balance = wavelet.balance && p.wavelet.balance == other.wavelet.balance;
        wavelet.iter = wavelet.iter && p.wavelet.iter == other.wavelet.iter;
        wavelet.sigmafin = wavelet.sigmafin && p.wavelet.sigmafin == other.wavelet.sigmafin;
        wavelet.sigmaton = wavelet.sigmaton && p.wavelet.sigmaton == other.wavelet.sigmaton;
        wavelet.sigmacol = wavelet.sigmacol && p.wavelet.sigmacol == other.wavelet.sigmacol;
        wavelet.sigmadir = wavelet.sigmadir && p.wavelet.sigmadir == other.wavelet.sigmadir;
        wavelet.rangeab = wavelet.rangeab && p.wavelet.rangeab == other.wavelet.rangeab;
        wavelet.protab = wavelet.protab && p.wavelet.protab == other.wavelet.protab;
        wavelet.median = wavelet.median && p.wavelet.median == other.wavelet.median;
        wavelet.medianlev = wavelet.medianlev && p.wavelet.medianlev == other.wavelet.medianlev;
        wavelet.linkedg = wavelet.linkedg && p.wavelet.linkedg == other.wavelet.linkedg;
        wavelet.cbenab = wavelet.cbenab && p.wavelet.cbenab == other.wavelet.cbenab;
        wavelet.greenmed = wavelet.greenmed && p.wavelet.greenmed == other.wavelet.greenmed;
        wavelet.bluemed = wavelet.bluemed && p.wavelet.bluemed == other.wavelet.bluemed;
        wavelet.greenhigh = wavelet.greenhigh && p.wavelet.greenhigh == other.wavelet.greenhigh;
        wavelet.bluehigh = wavelet.bluehigh && p.wavelet.bluehigh == other.wavelet.bluehigh;
        wavelet.greenlow = wavelet.greenlow && p.wavelet.greenlow == other.wavelet.greenlow;
        wavelet.bluelow = wavelet.bluelow && p.wavelet.bluelow == other.wavelet.bluelow;
        wavelet.lipst = wavelet.lipst && p.wavelet.lipst == other.wavelet.lipst;
        wavelet.bluehigh = wavelet.bluehigh && p.wavelet.bluehigh == other.wavelet.bluehigh;
        wavelet.ballum = wavelet.ballum && p.wavelet.ballum == other.wavelet.ballum;
        wavelet.sigm = wavelet.sigm && p.wavelet.sigm == other.wavelet.sigm;
        wavelet.levden = wavelet.levden && p.wavelet.levden == other.wavelet.levden;
        wavelet.thrden = wavelet.thrden && p.wavelet.thrden == other.wavelet.thrden;
        wavelet.limden = wavelet.limden && p.wavelet.limden == other.wavelet.limden;
        wavelet.balchrom = wavelet.balchrom && p.wavelet.balchrom == other.wavelet.balchrom;
        wavelet.chromfi = wavelet.chromfi && p.wavelet.chromfi == other.wavelet.chromfi;
        wavelet.chromco = wavelet.chromco && p.wavelet.chromco == other.wavelet.chromco;
        wavelet.mergeL = wavelet.mergeL && p.wavelet.mergeL == other.wavelet.mergeL;
        wavelet.mergeC = wavelet.mergeC && p.wavelet.mergeC == other.wavelet.mergeC;
        wavelet.softrad = wavelet.softrad && p.wavelet.softrad == other.wavelet.softrad;
        wavelet.softradend = wavelet.softradend && p.wavelet.softradend == other.wavelet.softradend;
        wavelet.strend = wavelet.strend && p.wavelet.strend == other.wavelet.strend;
        wavelet.detend = wavelet.detend && p.wavelet.detend == other.wavelet.detend;
        wavelet.thrend = wavelet.thrend && p.wavelet.thrend == other.wavelet.thrend;
        wavelet.ushamethod = wavelet.ushamethod && p.wavelet.ushamethod == other.wavelet.ushamethod;
        wavelet.avoid = wavelet.avoid && p.wavelet.avoid == other.wavelet.avoid;
        wavelet.showmask = wavelet.showmask && p.wavelet.showmask == other.wavelet.showmask;
        wavelet.oldsh = wavelet.oldsh && p.wavelet.oldsh == other.wavelet.oldsh;
        wavelet.tmr = wavelet.tmr && p.wavelet.tmr == other.wavelet.tmr;
        wavelet.Lmethod = wavelet.Lmethod && p.wavelet.Lmethod == other.wavelet.Lmethod;
        wavelet.CLmethod = wavelet.CLmethod && p.wavelet.CLmethod == other.wavelet.CLmethod;
        wavelet.Backmethod = wavelet.Backmethod && p.wavelet.Backmethod == other.wavelet.Backmethod;
        wavelet.Tilesmethod = wavelet.Tilesmethod && p.wavelet.Tilesmethod == other.wavelet.Tilesmethod;
        wavelet.complexmethod = wavelet.complexmethod && p.wavelet.complexmethod == other.wavelet.complexmethod;
        //wavelet.denmethod = wavelet.denmethod && p.wavelet.denmethod == other.wavelet.denmethod;
        wavelet.mixmethod = wavelet.mixmethod && p.wavelet.mixmethod == other.wavelet.mixmethod;
        wavelet.slimethod = wavelet.slimethod && p.wavelet.slimethod == other.wavelet.slimethod;
        wavelet.quamethod = wavelet.quamethod && p.wavelet.quamethod == other.wavelet.quamethod;
        wavelet.daubcoeffmethod = wavelet.daubcoeffmethod && p.wavelet.daubcoeffmethod == other.wavelet.daubcoeffmethod;
        wavelet.CHmethod = wavelet.CHmethod && p.wavelet.CHmethod == other.wavelet.CHmethod;
        wavelet.CHSLmethod = wavelet.CHSLmethod && p.wavelet.CHSLmethod == other.wavelet.CHSLmethod;
        wavelet.EDmethod = wavelet.EDmethod && p.wavelet.EDmethod == other.wavelet.EDmethod;
        wavelet.NPmethod = wavelet.NPmethod && p.wavelet.NPmethod == other.wavelet.NPmethod;
        wavelet.BAmethod = wavelet.BAmethod && p.wavelet.BAmethod == other.wavelet.BAmethod;
        wavelet.TMmethod = wavelet.TMmethod && p.wavelet.TMmethod == other.wavelet.TMmethod;
        wavelet.HSmethod = wavelet.HSmethod && p.wavelet.HSmethod == other.wavelet.HSmethod;
        wavelet.Dirmethod = wavelet.Dirmethod && p.wavelet.Dirmethod == other.wavelet.Dirmethod;
        wavelet.sigma = wavelet.sigma && p.wavelet.sigma == other.wavelet.sigma;
        wavelet.offset = wavelet.offset && p.wavelet.offset == other.wavelet.offset;
        wavelet.lowthr = wavelet.lowthr && p.wavelet.lowthr == other.wavelet.lowthr;
        wavelet.rescon = wavelet.rescon && p.wavelet.rescon == other.wavelet.rescon;
        wavelet.resconH = wavelet.resconH && p.wavelet.resconH == other.wavelet.resconH;
        wavelet.reschro = wavelet.reschro && p.wavelet.reschro == other.wavelet.reschro;
        wavelet.resblur = wavelet.resblur && p.wavelet.resblur == other.wavelet.resblur;
        wavelet.resblurc = wavelet.resblurc && p.wavelet.resblurc == other.wavelet.resblurc;
        wavelet.tmrs = wavelet.tmrs && p.wavelet.tmrs == other.wavelet.tmrs;
        wavelet.edgs = wavelet.edgs && p.wavelet.edgs == other.wavelet.edgs;
        wavelet.scale = wavelet.scale && p.wavelet.scale == other.wavelet.scale;
        wavelet.gamma = wavelet.gamma && p.wavelet.gamma == other.wavelet.gamma;
        wavelet.sup = wavelet.sup && p.wavelet.sup == other.wavelet.sup;
        wavelet.sky = wavelet.sky && p.wavelet.sky == other.wavelet.sky;
        wavelet.threshold = wavelet.threshold && p.wavelet.threshold == other.wavelet.threshold;
        wavelet.threshold2 = wavelet.threshold2 && p.wavelet.threshold2 == other.wavelet.threshold2;
        wavelet.edgedetect = wavelet.edgedetect && p.wavelet.edgedetect == other.wavelet.edgedetect;
        wavelet.edgedetectthr = wavelet.edgedetectthr && p.wavelet.edgedetectthr == other.wavelet.edgedetectthr;
        wavelet.edgedetectthr2 = wavelet.edgedetectthr2 && p.wavelet.edgedetectthr2 == other.wavelet.edgedetectthr2;
        wavelet.edgesensi = wavelet.edgesensi && p.wavelet.edgesensi == other.wavelet.edgesensi;
        wavelet.edgeampli = wavelet.edgeampli && p.wavelet.edgeampli == other.wavelet.edgeampli;
        wavelet.thres = wavelet.thres && p.wavelet.thres == other.wavelet.thres;
        wavelet.chroma = wavelet.chroma && p.wavelet.chroma == other.wavelet.chroma;
        wavelet.chro = wavelet.chro && p.wavelet.chro == other.wavelet.chro;
        wavelet.contrast = wavelet.contrast && p.wavelet.contrast == other.wavelet.contrast;
        wavelet.edgrad = wavelet.edgrad && p.wavelet.edgrad == other.wavelet.edgrad;
        wavelet.edgeffect = wavelet.edgeffect && p.wavelet.edgeffect == other.wavelet.edgeffect;
        wavelet.edgval = wavelet.edgval && p.wavelet.edgval == other.wavelet.edgval;
        wavelet.edgthresh = wavelet.edgthresh && p.wavelet.edgthresh == other.wavelet.edgthresh;
        wavelet.thr = wavelet.thr && p.wavelet.thr == other.wavelet.thr;
        wavelet.thrH = wavelet.thrH && p.wavelet.thrH == other.wavelet.thrH;
        wavelet.radius = wavelet.radius && p.wavelet.radius == other.wavelet.radius;
        wavelet.hueskin = wavelet.hueskin && p.wavelet.hueskin == other.wavelet.hueskin;
        wavelet.hueskin2 = wavelet.hueskin2 && p.wavelet.hueskin2 == other.wavelet.hueskin2;
        wavelet.hllev = wavelet.hllev && p.wavelet.hllev == other.wavelet.hllev;
        wavelet.bllev = wavelet.bllev && p.wavelet.bllev == other.wavelet.bllev;
        wavelet.edgcont = wavelet.edgcont && p.wavelet.edgcont == other.wavelet.edgcont;
        wavelet.chrwav = wavelet.chrwav && p.wavelet.chrwav == other.wavelet.chrwav;
        wavelet.bluwav = wavelet.bluwav && p.wavelet.bluwav == other.wavelet.bluwav;
        wavelet.level0noise = wavelet.level0noise && p.wavelet.level0noise == other.wavelet.level0noise;
        wavelet.level1noise = wavelet.level1noise && p.wavelet.level1noise == other.wavelet.level1noise;
        wavelet.level2noise = wavelet.level2noise && p.wavelet.level2noise == other.wavelet.level2noise;
        wavelet.level3noise = wavelet.level3noise && p.wavelet.level3noise == other.wavelet.level3noise;
        wavelet.leveldenoise = wavelet.leveldenoise && p.wavelet.leveldenoise == other.wavelet.leveldenoise;
        wavelet.levelsigm = wavelet.levelsigm && p.wavelet.levelsigm == other.wavelet.levelsigm;
        wavelet.pastlev = wavelet.pastlev && p.wavelet.pastlev == other.wavelet.pastlev;
        wavelet.satlev = wavelet.satlev && p.wavelet.satlev == other.wavelet.satlev;
        wavelet.ccwcurve = wavelet.ccwcurve && p.wavelet.ccwcurve == other.wavelet.ccwcurve;
        wavelet.blcurve = wavelet.blcurve && p.wavelet.blcurve == other.wavelet.blcurve;
        //wavelet.opacityCurveSH = wavelet.opacityCurveSH && p.wavelet.opacityCurveSH == other.wavelet.opacityCurveSH;
        wavelet.opacityCurveRG = wavelet.opacityCurveRG && p.wavelet.opacityCurveRG == other.wavelet.opacityCurveRG;
        wavelet.opacityCurveBY = wavelet.opacityCurveBY && p.wavelet.opacityCurveBY == other.wavelet.opacityCurveBY;
        wavelet.wavdenoise = wavelet.wavdenoise && p.wavelet.wavdenoise == other.wavelet.wavdenoise;
        wavelet.wavdenoiseh = wavelet.wavdenoiseh && p.wavelet.wavdenoiseh == other.wavelet.wavdenoiseh;
        wavelet.opacityCurveW = wavelet.opacityCurveW && p.wavelet.opacityCurveW == other.wavelet.opacityCurveW;
        wavelet.opacityCurveWL = wavelet.opacityCurveWL && p.wavelet.opacityCurveWL == other.wavelet.opacityCurveWL;
        wavelet.wavclCurve = wavelet.wavclCurve && p.wavelet.wavclCurve == other.wavelet.wavclCurve;
        wavelet.hhcurve = wavelet.hhcurve && p.wavelet.hhcurve == other.wavelet.hhcurve;
        wavelet.wavguidcurve = wavelet.wavguidcurve && p.wavelet.wavguidcurve == other.wavelet.wavguidcurve;
        wavelet.wavhuecurve = wavelet.wavhuecurve && p.wavelet.wavhuecurve == other.wavelet.wavhuecurve;
        wavelet.Chcurve = wavelet.Chcurve && p.wavelet.Chcurve == other.wavelet.Chcurve;
        wavelet.skinprotect = wavelet.skinprotect && p.wavelet.skinprotect == other.wavelet.skinprotect;
        //    wavelet.enacont = wavelet.enacont && p.wavelet.enacont == other.wavelet.enacont;
        wavelet.expcontrast = wavelet.expcontrast && p.wavelet.expcontrast == other.wavelet.expcontrast;
        wavelet.expchroma = wavelet.expchroma && p.wavelet.expchroma == other.wavelet.expchroma;
        wavelet.expedge = wavelet.expedge && p.wavelet.expedge == other.wavelet.expedge;
        wavelet.expbl = wavelet.expbl && p.wavelet.expbl == other.wavelet.expbl;
        wavelet.expresid = wavelet.expresid && p.wavelet.expresid == other.wavelet.expresid;
        wavelet.expfinal = wavelet.expfinal && p.wavelet.expfinal == other.wavelet.expfinal;
        wavelet.exptoning = wavelet.exptoning && p.wavelet.exptoning == other.wavelet.exptoning;
        wavelet.expnoise = wavelet.expnoise && p.wavelet.expnoise == other.wavelet.expnoise;
        wavelet.expclari = wavelet.expclari && p.wavelet.expclari == other.wavelet.expclari;
        wavelet.labgridALow = wavelet.labgridALow && p.wavelet.labgridALow == other.wavelet.labgridALow;
        wavelet.labgridBLow = wavelet.labgridBLow && p.wavelet.labgridBLow == other.wavelet.labgridBLow;
        wavelet.labgridAHigh = wavelet.labgridAHigh && p.wavelet.labgridAHigh == other.wavelet.labgridAHigh;
        wavelet.labgridBHigh = wavelet.labgridBHigh && p.wavelet.labgridBHigh == other.wavelet.labgridBHigh;

        for (int level = 0; level < 9; ++level) {
            wavelet.c[level] = wavelet.c[level] && p.wavelet.c[level] == other.wavelet.c[level];
            wavelet.ch[level] = wavelet.ch[level] && p.wavelet.ch[level] == other.wavelet.ch[level];
        }

        dirpyrequalizer.enabled = dirpyrequalizer.enabled && p.dirpyrequalizer.enabled == other.dirpyrequalizer.enabled;
        dirpyrequalizer.gamutlab = dirpyrequalizer.gamutlab && p.dirpyrequalizer.gamutlab == other.dirpyrequalizer.gamutlab;
        dirpyrequalizer.cbdlMethod = dirpyrequalizer.cbdlMethod && p.dirpyrequalizer.cbdlMethod == other.dirpyrequalizer.cbdlMethod;

        for (int level = 0; level < 6; ++level) {
            dirpyrequalizer.mult[level] = dirpyrequalizer.mult[level] && p.dirpyrequalizer.mult[level] == other.dirpyrequalizer.mult[level];
        }

        dirpyrequalizer.threshold = dirpyrequalizer.threshold && p.dirpyrequalizer.threshold == other.dirpyrequalizer.threshold;
        dirpyrequalizer.skinprotect = dirpyrequalizer.skinprotect && p.dirpyrequalizer.skinprotect == other.dirpyrequalizer.skinprotect;
        //    dirpyrequalizer.algo = dirpyrequalizer.algo && p.dirpyrequalizer.algo == other.dirpyrequalizer.algo;
        dirpyrequalizer.hueskin = dirpyrequalizer.hueskin && p.dirpyrequalizer.hueskin == other.dirpyrequalizer.hueskin;
        hsvequalizer.enabled = hsvequalizer.enabled && p.hsvequalizer.enabled == other.hsvequalizer.enabled;
        hsvequalizer.hcurve = hsvequalizer.hcurve && p.hsvequalizer.hcurve == other.hsvequalizer.hcurve;
        hsvequalizer.scurve = hsvequalizer.scurve && p.hsvequalizer.scurve == other.hsvequalizer.scurve;
        hsvequalizer.vcurve = hsvequalizer.vcurve && p.hsvequalizer.vcurve == other.hsvequalizer.vcurve;
        filmSimulation.enabled = filmSimulation.enabled && p.filmSimulation.enabled == other.filmSimulation.enabled;
        filmSimulation.clutFilename = filmSimulation.clutFilename && p.filmSimulation.clutFilename == other.filmSimulation.clutFilename;
        filmSimulation.strength = filmSimulation.strength && p.filmSimulation.strength == other.filmSimulation.strength;
        softlight.enabled = softlight.enabled && p.softlight.enabled == other.softlight.enabled;
        softlight.strength = softlight.strength && p.softlight.strength == other.softlight.strength;
        dehaze.enabled = dehaze.enabled && p.dehaze.enabled == other.dehaze.enabled;
        dehaze.strength = dehaze.strength && p.dehaze.strength == other.dehaze.strength;
        dehaze.showDepthMap = dehaze.showDepthMap && p.dehaze.showDepthMap == other.dehaze.showDepthMap;
        dehaze.depth = dehaze.depth && p.dehaze.depth == other.dehaze.depth;
        dehaze.saturation = dehaze.saturation && p.dehaze.saturation == other.dehaze.saturation;
        metadata.mode = metadata.mode && p.metadata.mode == other.metadata.mode;
        metadata.exifKeys = metadata.exifKeys && p.metadata.exifKeys == other.metadata.exifKeys;
        filmNegative.enabled = filmNegative.enabled && p.filmNegative.enabled == other.filmNegative.enabled;
        filmNegative.redRatio = filmNegative.redRatio && p.filmNegative.redRatio == other.filmNegative.redRatio;
        filmNegative.greenExp = filmNegative.greenExp && p.filmNegative.greenExp == other.filmNegative.greenExp;
        filmNegative.blueRatio = filmNegative.blueRatio && p.filmNegative.blueRatio == other.filmNegative.blueRatio;
        filmNegative.refInput = filmNegative.refInput && p.filmNegative.refInput == other.filmNegative.refInput;
        filmNegative.refOutput = filmNegative.refOutput && p.filmNegative.refOutput == other.filmNegative.refOutput;
        filmNegative.colorSpace = filmNegative.colorSpace && p.filmNegative.colorSpace == other.filmNegative.colorSpace;
        raw.preprocessWB.mode  = raw.preprocessWB.mode  && p.raw.preprocessWB.mode  == other.raw.preprocessWB.mode;

//      How the hell can we handle that???
//      exif = exif && p.exif==other.exif
//      iptc = other.iptc;
    }
}

void ParamsEdited::combine(rtengine::procparams::ProcParams& toEdit, const rtengine::procparams::ProcParams& mods, bool forceSet)
{
    const Options& options = App::get().options();

    bool dontforceSet = !forceSet;

    if (toneCurve.curve) {
        toEdit.toneCurve.curve = mods.toneCurve.curve;
    }

    if (toneCurve.curve2) {
        toEdit.toneCurve.curve2 = mods.toneCurve.curve2;
    }

    if (toneCurve.curveMode) {
        toEdit.toneCurve.curveMode = mods.toneCurve.curveMode;
    }

    if (toneCurve.curveMode2) {
        toEdit.toneCurve.curveMode2 = mods.toneCurve.curveMode2;
    }

    if (toneCurve.brightness) {
        toEdit.toneCurve.brightness = dontforceSet && options.baBehav[ADDSET_TC_BRIGHTNESS] ? toEdit.toneCurve.brightness + mods.toneCurve.brightness : mods.toneCurve.brightness;
    }

    if (toneCurve.black) {
        toEdit.toneCurve.black = dontforceSet && options.baBehav[ADDSET_TC_BLACKLEVEL] ? toEdit.toneCurve.black + mods.toneCurve.black : mods.toneCurve.black;
    }

    if (toneCurve.contrast) {
        toEdit.toneCurve.contrast = dontforceSet && options.baBehav[ADDSET_TC_CONTRAST] ? toEdit.toneCurve.contrast + mods.toneCurve.contrast : mods.toneCurve.contrast;
    }

    if (toneCurve.saturation) {
        toEdit.toneCurve.saturation = dontforceSet && options.baBehav[ADDSET_TC_SATURATION] ? toEdit.toneCurve.saturation + mods.toneCurve.saturation : mods.toneCurve.saturation;
    }

    if (toneCurve.shcompr) {
        toEdit.toneCurve.shcompr = dontforceSet && options.baBehav[ADDSET_TC_SHCOMP] ? toEdit.toneCurve.shcompr + mods.toneCurve.shcompr : mods.toneCurve.shcompr;
    }

    if (toneCurve.autoexp) {
        toEdit.toneCurve.autoexp = mods.toneCurve.autoexp;
    }

    if (toneCurve.clip) {
        toEdit.toneCurve.clip = mods.toneCurve.clip;
    }

    if (toneCurve.expcomp) {
        toEdit.toneCurve.expcomp = dontforceSet && options.baBehav[ADDSET_TC_EXPCOMP] ? toEdit.toneCurve.expcomp + mods.toneCurve.expcomp : mods.toneCurve.expcomp;
    }

    if (toneCurve.hlcompr) {
        toEdit.toneCurve.hlcompr = dontforceSet && options.baBehav[ADDSET_TC_HLCOMPAMOUNT] ? toEdit.toneCurve.hlcompr + mods.toneCurve.hlcompr : mods.toneCurve.hlcompr;
    }

    if (toneCurve.hlcomprthresh) {
        toEdit.toneCurve.hlcomprthresh = dontforceSet && options.baBehav[ADDSET_TC_HLCOMPTHRESH] ? toEdit.toneCurve.hlcomprthresh + mods.toneCurve.hlcomprthresh : mods.toneCurve.hlcomprthresh;
    }

    if (toneCurve.hrenabled) {
        toEdit.toneCurve.hrenabled = mods.toneCurve.hrenabled;
    }

    if (toneCurve.method) {
        toEdit.toneCurve.method = mods.toneCurve.method;
    }

    if (toneCurve.hlbl) {
        toEdit.toneCurve.hlbl = mods.toneCurve.hlbl;
    }

    if (toneCurve.hlth) {
        toEdit.toneCurve.hlth = mods.toneCurve.hlth;
    }

    if (toneCurve.histmatching) {
        toEdit.toneCurve.histmatching = mods.toneCurve.histmatching;
    }

    if (toneCurve.fromHistMatching) {
        toEdit.toneCurve.fromHistMatching = mods.toneCurve.fromHistMatching;
    }

    if (toneCurve.clampOOG) {
        toEdit.toneCurve.clampOOG = mods.toneCurve.clampOOG;
    }

    if (retinex.enabled) {
        toEdit.retinex.enabled = mods.retinex.enabled;
    }

    if (retinex.cdcurve) {
        toEdit.retinex.cdcurve = mods.retinex.cdcurve;
    }

    if (retinex.mapcurve) {
        toEdit.retinex.mapcurve = mods.retinex.mapcurve;
    }

    if (retinex.cdHcurve) {
        toEdit.retinex.cdHcurve = mods.retinex.cdHcurve;
    }

    if (retinex.lhcurve) {
        toEdit.retinex.lhcurve = mods.retinex.lhcurve;
    }

    if (retinex.transmissionCurve) {
        toEdit.retinex.transmissionCurve = mods.retinex.transmissionCurve;
    }

    if (retinex.gaintransmissionCurve) {
        toEdit.retinex.gaintransmissionCurve = mods.retinex.gaintransmissionCurve;
    }

    if (retinex.complexmethod) {
        toEdit.retinex.complexmethod = mods.retinex.complexmethod;
    }

    if (retinex.retinexMethod) {
        toEdit.retinex.retinexMethod = mods.retinex.retinexMethod;
    }

    if (retinex.mapMethod) {
        toEdit.retinex.mapMethod = mods.retinex.mapMethod;
    }

    if (retinex.viewMethod) {
        toEdit.retinex.viewMethod = mods.retinex.viewMethod;
    }

    if (retinex.retinexcolorspace) {
        toEdit.retinex.retinexcolorspace = mods.retinex.retinexcolorspace;
    }

    if (retinex.gammaretinex) {
        toEdit.retinex.gammaretinex = mods.retinex.gammaretinex;
    }

    if (retinex.gam) {
        toEdit.retinex.gam = dontforceSet && options.baBehav[ADDSET_RETI_GAM] ? toEdit.retinex.gam + mods.retinex.gam : mods.retinex.gam;
    }

    if (retinex.slope) {
        toEdit.retinex.slope = dontforceSet && options.baBehav[ADDSET_RETI_SLO] ? toEdit.retinex.slope + mods.retinex.slope : mods.retinex.slope;
    }

    if (retinex.str) {
        toEdit.retinex.str = dontforceSet && options.baBehav[ADDSET_RETI_STR] ? toEdit.retinex.str + mods.retinex.str : mods.retinex.str;
    }

    if (retinex.scal) {
        toEdit.retinex.scal = mods.retinex.scal;
    }

    if (retinex.iter) {
        toEdit.retinex.iter = mods.retinex.iter;
    }

    if (retinex.grad) {
        toEdit.retinex.grad = mods.retinex.grad;
    }

    if (retinex.grads) {
        toEdit.retinex.grads = mods.retinex.grads;
    }

//    if (retinex.scal) {
//        toEdit.retinex.scal = dontforceSet && options.baBehav[ADDSET_RETI_SCAL] ? toEdit.retinex.scal + mods.retinex.scal : mods.retinex.scal;
//    }

    if (retinex.medianmap) {
        toEdit.retinex.medianmap = mods.retinex.medianmap;
    }

    if (retinex.neigh) {
        toEdit.retinex.neigh = dontforceSet && options.baBehav[ADDSET_RETI_NEIGH] ? toEdit.retinex.neigh + mods.retinex.neigh : mods.retinex.neigh;
    }

    if (retinex.limd) {
        toEdit.retinex.limd = dontforceSet && options.baBehav[ADDSET_RETI_LIMD] ? toEdit.retinex.limd + mods.retinex.limd : mods.retinex.limd;
    }

    if (retinex.highl) {
        toEdit.retinex.highl = mods.retinex.highl;
    }

    if (retinex.skal) {
        toEdit.retinex.skal = mods.retinex.skal;
    }

    if (retinex.offs) {
        toEdit.retinex.offs = dontforceSet && options.baBehav[ADDSET_RETI_OFFS] ? toEdit.retinex.offs + mods.retinex.offs : mods.retinex.offs;
    }

    if (retinex.vart) {
        toEdit.retinex.vart = dontforceSet && options.baBehav[ADDSET_RETI_VART] ? toEdit.retinex.vart + mods.retinex.vart : mods.retinex.vart;
    }

    if (retinex.highlights) {
        toEdit.retinex.highlights = mods.retinex.highlights;
    }

    if (retinex.htonalwidth) {
        toEdit.retinex.htonalwidth = mods.retinex.htonalwidth;
    }

    if (retinex.shadows) {
        toEdit.retinex.shadows = mods.retinex.shadows;

    }

    if (retinex.stonalwidth) {
        toEdit.retinex.stonalwidth = mods.retinex.stonalwidth;
    }

    if (retinex.radius) {
        toEdit.retinex.radius = mods.retinex.radius;
    }


    if (labCurve.enabled) {
        toEdit.labCurve.enabled = mods.labCurve.enabled;
    }

    if (labCurve.lcurve) {
        toEdit.labCurve.lcurve = mods.labCurve.lcurve;
    }

    if (labCurve.acurve) {
        toEdit.labCurve.acurve = mods.labCurve.acurve;
    }

    if (labCurve.bcurve) {
        toEdit.labCurve.bcurve = mods.labCurve.bcurve;
    }

    if (labCurve.cccurve) {
        toEdit.labCurve.cccurve = mods.labCurve.cccurve;
    }

    if (labCurve.chcurve) {
        toEdit.labCurve.chcurve = mods.labCurve.chcurve;
    }

    if (labCurve.lhcurve) {
        toEdit.labCurve.lhcurve = mods.labCurve.lhcurve;
    }

    if (labCurve.hhcurve) {
        toEdit.labCurve.hhcurve = mods.labCurve.hhcurve;
    }

    if (labCurve.lccurve) {
        toEdit.labCurve.lccurve = mods.labCurve.lccurve;
    }

    if (labCurve.clcurve) {
        toEdit.labCurve.clcurve = mods.labCurve.clcurve;
    }

    if (labCurve.brightness) {
        toEdit.labCurve.brightness = dontforceSet && options.baBehav[ADDSET_LC_BRIGHTNESS] ? toEdit.labCurve.brightness + mods.labCurve.brightness : mods.labCurve.brightness;
    }

    if (labCurve.contrast) {
        toEdit.labCurve.contrast = dontforceSet && options.baBehav[ADDSET_LC_CONTRAST] ? toEdit.labCurve.contrast + mods.labCurve.contrast : mods.labCurve.contrast;
    }

    if (labCurve.chromaticity) {
        toEdit.labCurve.chromaticity = dontforceSet && options.baBehav[ADDSET_LC_CHROMATICITY] ? toEdit.labCurve.chromaticity + mods.labCurve.chromaticity : mods.labCurve.chromaticity;
    }

    if (labCurve.gamutmunselmethod) {
        toEdit.labCurve.gamutmunselmethod = mods.labCurve.gamutmunselmethod;
    }

    if (labCurve.rstprotection) {
        toEdit.labCurve.rstprotection = mods.labCurve.rstprotection;
    }

    if (labCurve.lcredsk) {
        toEdit.labCurve.lcredsk = mods.labCurve.lcredsk;
    }

    if (localContrast.enabled) {
        toEdit.localContrast.enabled = mods.localContrast.enabled;
    }

#define ADDSETVAL_(v, i)                                                                        \
    do {                                                                                        \
        if ( v ) {                                                                              \
            toEdit. v = dontforceSet && options.baBehav[ i ] ? toEdit. v + mods. v : mods. v ;  \
        }                                                                                       \
    } while (false)

    ADDSETVAL_(localContrast.radius, ADDSET_LOCALCONTRAST_RADIUS);
    ADDSETVAL_(localContrast.amount, ADDSET_LOCALCONTRAST_AMOUNT);
    ADDSETVAL_(localContrast.darkness, ADDSET_LOCALCONTRAST_DARKNESS);
    ADDSETVAL_(localContrast.lightness, ADDSET_LOCALCONTRAST_LIGHTNESS);
#undef ADDSETVAL_

    if (rgbCurves.enabled) {
        toEdit.rgbCurves.enabled = mods.rgbCurves.enabled;
    }

    if (rgbCurves.lumamode) {
        toEdit.rgbCurves.lumamode = mods.rgbCurves.lumamode;
    }

    if (rgbCurves.rcurve) {
        toEdit.rgbCurves.rcurve = mods.rgbCurves.rcurve;
    }

    if (rgbCurves.gcurve) {
        toEdit.rgbCurves.gcurve = mods.rgbCurves.gcurve;
    }

    if (rgbCurves.bcurve) {
        toEdit.rgbCurves.bcurve = mods.rgbCurves.bcurve;
    }

    if (colorToning.enabled) {
        toEdit.colorToning.enabled = mods.colorToning.enabled;
    }

    if (colorToning.twocolor) {
        toEdit.colorToning.twocolor = mods.colorToning.twocolor;
    }

    if (colorToning.opacityCurve) {
        toEdit.colorToning.opacityCurve = mods.colorToning.opacityCurve;
    }

    if (colorToning.colorCurve) {
        toEdit.colorToning.colorCurve = mods.colorToning.colorCurve;
    }

    if (colorToning.enabled) {
        toEdit.colorToning.enabled = mods.colorToning.enabled;
    }

    if (colorToning.opacityCurve) {
        toEdit.colorToning.opacityCurve = mods.colorToning.opacityCurve;
    }

    if (colorToning.satprotectionthreshold) {
        toEdit.colorToning.satProtectionThreshold = dontforceSet && options.baBehav[ADDSET_COLORTONING_SATTHRESHOLD] ? toEdit.colorToning.satProtectionThreshold + mods.colorToning.satProtectionThreshold : mods.colorToning.satProtectionThreshold;
    }

    if (colorToning.autosat) {
        toEdit.colorToning.autosat = mods.colorToning.autosat;
    }

    if (colorToning.saturatedopacity) {
        toEdit.colorToning.saturatedOpacity = dontforceSet && options.baBehav[ADDSET_COLORTONING_SATOPACITY] ? toEdit.colorToning.saturatedOpacity + mods.colorToning.saturatedOpacity : mods.colorToning.saturatedOpacity;
    }

    if (colorToning.strength) {
        toEdit.colorToning.strength = dontforceSet && options.baBehav[ADDSET_COLORTONING_STRENGTH] ? toEdit.colorToning.strength + mods.colorToning.strength : mods.colorToning.strength;
    }

    if (colorToning.shadowsColSat) {
        toEdit.colorToning.shadowsColSat = mods.colorToning.shadowsColSat;
    }

    if (colorToning.hlColSat) {
        toEdit.colorToning.hlColSat = mods.colorToning.hlColSat;
    }

    if (colorToning.balance) {
        toEdit.colorToning.balance = dontforceSet && options.baBehav[ADDSET_COLORTONING_BALANCE] ? toEdit.colorToning.balance + mods.colorToning.balance : mods.colorToning.balance;
    }

    if (colorToning.clcurve) {
        toEdit.colorToning.clcurve = mods.colorToning.clcurve;
    }

    if (colorToning.method) {
        toEdit.colorToning.method = mods.colorToning.method;
    }

    if (colorToning.cl2curve) {
        toEdit.colorToning.cl2curve = mods.colorToning.cl2curve;
    }

    if (colorToning.lumamode) {
        toEdit.colorToning.lumamode = mods.colorToning.lumamode;
    }

    if (colorToning.satlow) {
        toEdit.colorToning.satlow = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.satlow + mods.colorToning.satlow : mods.colorToning.satlow;
    }

    if (colorToning.sathigh) {
        toEdit.colorToning.sathigh = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.sathigh + mods.colorToning.sathigh : mods.colorToning.sathigh;
    }

    if (colorToning.redlow) {
        toEdit.colorToning.redlow = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.redlow + mods.colorToning.redlow : mods.colorToning.redlow;
    }

    if (colorToning.greenlow) {
        toEdit.colorToning.greenlow = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.greenlow + mods.colorToning.greenlow : mods.colorToning.greenlow;
    }

    if (colorToning.bluelow) {
        toEdit.colorToning.bluelow = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.bluelow + mods.colorToning.bluelow : mods.colorToning.bluelow;
    }

    if (colorToning.redmed) {
        toEdit.colorToning.redmed = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.redmed + mods.colorToning.redmed : mods.colorToning.redmed;
    }

    if (colorToning.greenmed) {
        toEdit.colorToning.greenmed = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.greenmed + mods.colorToning.greenmed : mods.colorToning.greenmed;
    }

    if (colorToning.bluemed) {
        toEdit.colorToning.bluemed = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.bluemed + mods.colorToning.bluemed : mods.colorToning.bluemed;
    }

    if (colorToning.redhigh) {
        toEdit.colorToning.redhigh = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.redhigh + mods.colorToning.redhigh : mods.colorToning.redhigh;
    }

    if (colorToning.greenhigh) {
        toEdit.colorToning.greenhigh = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.greenhigh + mods.colorToning.greenhigh : mods.colorToning.greenhigh;
    }

    if (colorToning.bluehigh) {
        toEdit.colorToning.bluehigh = dontforceSet && options.baBehav[ADDSET_COLORTONING_SPLIT] ? toEdit.colorToning.bluehigh + mods.colorToning.bluehigh : mods.colorToning.bluehigh;
    }

    if (colorToning.labgridALow) {
        toEdit.colorToning.labgridALow = mods.colorToning.labgridALow;
    }

    if (colorToning.labgridBLow) {
        toEdit.colorToning.labgridBLow = mods.colorToning.labgridBLow;
    }

    if (colorToning.labgridAHigh) {
        toEdit.colorToning.labgridAHigh = mods.colorToning.labgridAHigh;
    }

    if (colorToning.labgridBHigh) {
        toEdit.colorToning.labgridBHigh = mods.colorToning.labgridBHigh;
    }

    if (colorToning.labregions) {
        toEdit.colorToning.labregions = mods.colorToning.labregions;
    }

    if (colorToning.labregionsShowMask) {
        toEdit.colorToning.labregionsShowMask = mods.colorToning.labregionsShowMask;
    }

    if (sharpenEdge.enabled) {
        toEdit.sharpenEdge.enabled = mods.sharpenEdge.enabled;
    }

    if (sharpenEdge.passes) {
        toEdit.sharpenEdge.passes = dontforceSet && options.baBehav[ADDSET_SHARPENEDGE_PASS] ? toEdit.sharpenEdge.passes + mods.sharpenEdge.passes : mods.sharpenEdge.passes;
    }

    if (sharpenEdge.amount) {
        toEdit.sharpenEdge.amount = dontforceSet && options.baBehav[ADDSET_SHARPENEDGE_AMOUNT] ? toEdit.sharpenEdge.amount + mods.sharpenEdge.amount : mods.sharpenEdge.amount;
    }

    if (sharpenEdge.threechannels) {
        toEdit.sharpenEdge.threechannels = mods.sharpenEdge.threechannels;
    }

    if (sharpenMicro.enabled) {
        toEdit.sharpenMicro.enabled = mods.sharpenMicro.enabled;
    }

    if (sharpenMicro.matrix) {
        toEdit.sharpenMicro.matrix = mods.sharpenMicro.matrix;
    }

    if (sharpenMicro.amount) {
        toEdit.sharpenMicro.amount = dontforceSet && options.baBehav[ADDSET_SHARPENMICRO_AMOUNT] ? toEdit.sharpenMicro.amount + mods.sharpenMicro.amount : mods.sharpenMicro.amount;
    }

    if (sharpenMicro.contrast) {
        toEdit.sharpenMicro.contrast = dontforceSet && options.baBehav[ADDSET_SHARPENMICRO_CONTRAST] ? toEdit.sharpenMicro.contrast + mods.sharpenMicro.contrast : mods.sharpenMicro.contrast;
    }

    if (sharpenMicro.uniformity) {
        toEdit.sharpenMicro.uniformity = dontforceSet && options.baBehav[ADDSET_SHARPENMICRO_UNIFORMITY] ? toEdit.sharpenMicro.uniformity + mods.sharpenMicro.uniformity : mods.sharpenMicro.uniformity;
    }

    if (sharpening.enabled) {
        toEdit.sharpening.enabled = mods.sharpening.enabled;
    }

    if (sharpening.contrast) {
        toEdit.sharpening.contrast = dontforceSet && options.baBehav[ADDSET_SHARP_CONTRAST] ? toEdit.sharpening.contrast + mods.sharpening.contrast : mods.sharpening.contrast;
    }

    if (sharpening.radius) {
        toEdit.sharpening.radius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.sharpening.radius + mods.sharpening.radius : mods.sharpening.radius;
    }

    if (sharpening.blurradius) {
        toEdit.sharpening.blurradius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.sharpening.blurradius + mods.sharpening.blurradius : mods.sharpening.blurradius;
    }

    if (sharpening.gamma) {
        toEdit.sharpening.gamma = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.sharpening.gamma + mods.sharpening.gamma : mods.sharpening.gamma;
    }

    if (sharpening.amount) {
        toEdit.sharpening.amount = dontforceSet && options.baBehav[ADDSET_SHARP_AMOUNT] ? toEdit.sharpening.amount + mods.sharpening.amount : mods.sharpening.amount;
    }

    if (sharpening.threshold) {
        toEdit.sharpening.threshold = mods.sharpening.threshold;
    }

    if (sharpening.edgesonly) {
        toEdit.sharpening.edgesonly = mods.sharpening.edgesonly;
    }

    if (sharpening.edges_radius) {
        toEdit.sharpening.edges_radius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.sharpening.edges_radius + mods.sharpening.edges_radius : mods.sharpening.edges_radius;
    }

    if (sharpening.edges_tolerance) {
        toEdit.sharpening.edges_tolerance = dontforceSet && options.baBehav[ADDSET_SHARP_EDGETOL] ? toEdit.sharpening.edges_tolerance + mods.sharpening.edges_tolerance : mods.sharpening.edges_tolerance;
    }

    if (sharpening.halocontrol) {
        toEdit.sharpening.halocontrol = mods.sharpening.halocontrol;
    }

    if (sharpening.halocontrol_amount) {
        toEdit.sharpening.halocontrol_amount = dontforceSet && options.baBehav[ADDSET_SHARP_HALOCTRL] ? toEdit.sharpening.halocontrol_amount + mods.sharpening.halocontrol_amount : mods.sharpening.halocontrol_amount;
    }

    if (sharpening.method) {
        toEdit.sharpening.method = mods.sharpening.method;
    }

    if (sharpening.deconvamount) {
        toEdit.sharpening.deconvamount = dontforceSet && options.baBehav[ADDSET_SHARP_AMOUNT] ? toEdit.sharpening.deconvamount + mods.sharpening.deconvamount : mods.sharpening.deconvamount;
    }

    if (sharpening.deconvradius) {
        toEdit.sharpening.deconvradius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.sharpening.deconvradius + mods.sharpening.deconvradius : mods.sharpening.deconvradius;
    }

    if (sharpening.deconviter) {
        toEdit.sharpening.deconviter = dontforceSet && options.baBehav[ADDSET_SHARP_ITER] ? toEdit.sharpening.deconviter + mods.sharpening.deconviter : mods.sharpening.deconviter;
    }

    if (sharpening.deconvdamping) {
        toEdit.sharpening.deconvdamping = dontforceSet && options.baBehav[ADDSET_SHARP_DAMPING] ? toEdit.sharpening.deconvdamping + mods.sharpening.deconvdamping : mods.sharpening.deconvdamping;
    }

    if (pdsharpening.enabled) {
        toEdit.pdsharpening.enabled = mods.pdsharpening.enabled;
    }

    if (pdsharpening.contrast) {
        toEdit.pdsharpening.contrast = dontforceSet && options.baBehav[ADDSET_SHARP_CONTRAST] ? toEdit.pdsharpening.contrast + mods.pdsharpening.contrast : mods.pdsharpening.contrast;
    }

    if (pdsharpening.noisecap) {
        toEdit.pdsharpening.noisecap = mods.pdsharpening.noisecap;
    }

    if (pdsharpening.noisecapafter) {
        toEdit.pdsharpening.noisecapafter = mods.pdsharpening.noisecapafter;
    }

    if (pdsharpening.autoContrast) {
        toEdit.pdsharpening.autoContrast = mods.pdsharpening.autoContrast;
    }

    if (pdsharpening.autoRadius) {
        toEdit.pdsharpening.autoRadius = mods.pdsharpening.autoRadius;
    }

    if (pdsharpening.deconvradius) {
        toEdit.pdsharpening.deconvradius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.pdsharpening.deconvradius + mods.pdsharpening.deconvradius : mods.pdsharpening.deconvradius;
    }

    if (pdsharpening.deconvradiusOffset) {
        toEdit.pdsharpening.deconvradiusOffset = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.pdsharpening.deconvradiusOffset + mods.pdsharpening.deconvradiusOffset : mods.pdsharpening.deconvradiusOffset;
    }

    if (pdsharpening.deconviter) {
        toEdit.pdsharpening.deconviter = dontforceSet && options.baBehav[ADDSET_SHARP_ITER] ? toEdit.pdsharpening.deconviter + mods.pdsharpening.deconviter : mods.pdsharpening.deconviter;
    }

    if (pdsharpening.deconvitercheck) {
        toEdit.pdsharpening.deconvitercheck =  mods.pdsharpening.deconvitercheck;
    }

    if (pdsharpening.showcap) {
        toEdit.pdsharpening.showcap =  mods.pdsharpening.showcap;
    }

    if (pdsharpening.noisecaptype) {
        toEdit.pdsharpening.noisecaptype =  mods.pdsharpening.noisecaptype;
    }

    if (prsharpening.enabled) {
        toEdit.prsharpening.enabled = mods.prsharpening.enabled;
    }

    if (prsharpening.contrast) {
        toEdit.prsharpening.contrast = dontforceSet && options.baBehav[ADDSET_SHARP_CONTRAST] ? toEdit.prsharpening.contrast + mods.prsharpening.contrast : mods.prsharpening.contrast;
    }

    if (prsharpening.radius) {
        toEdit.prsharpening.radius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.prsharpening.radius + mods.prsharpening.radius : mods.prsharpening.radius;
    }

    if (prsharpening.amount) {
        toEdit.prsharpening.amount = dontforceSet && options.baBehav[ADDSET_SHARP_AMOUNT] ? toEdit.prsharpening.amount + mods.prsharpening.amount : mods.prsharpening.amount;
    }

    if (prsharpening.threshold) {
        toEdit.prsharpening.threshold = mods.prsharpening.threshold;
    }

    if (prsharpening.edgesonly) {
        toEdit.prsharpening.edgesonly = mods.prsharpening.edgesonly;
    }

    if (prsharpening.edges_radius) {
        toEdit.prsharpening.edges_radius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.prsharpening.edges_radius + mods.prsharpening.edges_radius : mods.prsharpening.edges_radius;
    }

    if (prsharpening.edges_tolerance) {
        toEdit.prsharpening.edges_tolerance = dontforceSet && options.baBehav[ADDSET_SHARP_EDGETOL] ? toEdit.prsharpening.edges_tolerance + mods.prsharpening.edges_tolerance : mods.prsharpening.edges_tolerance;
    }

    if (prsharpening.halocontrol) {
        toEdit.prsharpening.halocontrol = mods.prsharpening.halocontrol;
    }

    if (prsharpening.halocontrol_amount) {
        toEdit.prsharpening.halocontrol_amount = dontforceSet && options.baBehav[ADDSET_SHARP_HALOCTRL] ? toEdit.prsharpening.halocontrol_amount + mods.prsharpening.halocontrol_amount : mods.prsharpening.halocontrol_amount;
    }

    if (prsharpening.method) {
        toEdit.prsharpening.method = mods.prsharpening.method;
    }

    if (prsharpening.deconvamount) {
        toEdit.prsharpening.deconvamount = dontforceSet && options.baBehav[ADDSET_SHARP_AMOUNT] ? toEdit.prsharpening.deconvamount + mods.prsharpening.deconvamount : mods.prsharpening.deconvamount;
    }

    if (prsharpening.deconvradius) {
        toEdit.prsharpening.deconvradius = dontforceSet && options.baBehav[ADDSET_SHARP_RADIUS] ? toEdit.prsharpening.deconvradius + mods.prsharpening.deconvradius : mods.prsharpening.deconvradius;
    }

    if (prsharpening.deconviter) {
        toEdit.prsharpening.deconviter = dontforceSet && options.baBehav[ADDSET_SHARP_ITER] ? toEdit.prsharpening.deconviter + mods.prsharpening.deconviter : mods.prsharpening.deconviter;
    }

    if (prsharpening.deconvdamping) {
        toEdit.prsharpening.deconvdamping = dontforceSet && options.baBehav[ADDSET_SHARP_DAMPING] ? toEdit.prsharpening.deconvdamping + mods.prsharpening.deconvdamping : mods.prsharpening.deconvdamping;
    }

    if (vibrance.enabled) {
        toEdit.vibrance.enabled = mods.vibrance.enabled;
    }

    if (vibrance.pastels) {
        toEdit.vibrance.pastels = dontforceSet && options.baBehav[ADDSET_VIBRANCE_PASTELS] ? toEdit.vibrance.pastels + mods.vibrance.pastels : mods.vibrance.pastels;
    }

    if (vibrance.saturated) {
        toEdit.vibrance.saturated = dontforceSet && options.baBehav[ADDSET_VIBRANCE_SATURATED] ? toEdit.vibrance.saturated + mods.vibrance.saturated : mods.vibrance.saturated;
    }

    if (vibrance.psthreshold) {
        toEdit.vibrance.psthreshold = mods.vibrance.psthreshold;
    }

    if (vibrance.protectskins) {
        toEdit.vibrance.protectskins = mods.vibrance.protectskins;
    }

    if (vibrance.avoidcolorshift) {
        toEdit.vibrance.avoidcolorshift = mods.vibrance.avoidcolorshift;
    }

    if (vibrance.pastsattog) {
        toEdit.vibrance.pastsattog = mods.vibrance.pastsattog;
    }

    if (vibrance.skintonescurve) {
        toEdit.vibrance.skintonescurve = mods.vibrance.skintonescurve;
    }

    //if (colorBoost.amount)                    toEdit.colorBoost.amount = dontforceSet && options.baBehav[ADDSET_CBOOST_AMOUNT] ? toEdit.colorBoost.amount + mods.colorBoost.amount : mods.colorBoost.amount;
    //if (colorBoost.avoidclip)             toEdit.colorBoost.avoidclip = mods.colorBoost.avoidclip;
    //if (colorBoost.enable_saturationlimiter)toEdit.colorBoost.enable_saturationlimiter = mods.colorBoost.enable_saturationlimiter;
    //if (colorBoost.saturationlimit)           toEdit.colorBoost.saturationlimit = mods.colorBoost.saturationlimit;
    if (wb.enabled) {
        toEdit.wb.enabled = mods.wb.enabled;
    }

    if (wb.method) {
        toEdit.wb.method = mods.wb.method;
    }

    if (wb.equal) {
        toEdit.wb.equal = dontforceSet && options.baBehav[ADDSET_WB_EQUAL] ? toEdit.wb.equal + mods.wb.equal : mods.wb.equal;
    }

    if (wb.tempBias) {
        toEdit.wb.tempBias = dontforceSet && options.baBehav[ADDSET_WB_TEMPBIAS] ? toEdit.wb.tempBias + mods.wb.tempBias : mods.wb.tempBias;
    }

    if (wb.observer) {
        toEdit.wb.observer = mods.wb.observer;
    }

    if (wb.itcwb_green) {
        toEdit.wb.itcwb_green = mods.wb.itcwb_green;
    }

    if (wb.itcwb_rgreen) {
        toEdit.wb.itcwb_rgreen = mods.wb.itcwb_rgreen;
    }

    if (wb.itcwb_nopurple) {
        toEdit.wb.itcwb_nopurple = mods.wb.itcwb_nopurple;
    }

    if (wb.itcwb_alg) {
        toEdit.wb.itcwb_alg = mods.wb.itcwb_alg;
    }

    if (wb.itcwb_prim) {
        toEdit.wb.itcwb_prim = mods.wb.itcwb_prim;
    }

    if (wb.itcwb_sampling) {
        toEdit.wb.itcwb_sampling = mods.wb.itcwb_sampling;
    }

    if (wb.green) {
        toEdit.wb.green = dontforceSet && options.baBehav[ADDSET_WB_GREEN] ? toEdit.wb.green + mods.wb.green : mods.wb.green;
    }

    if (wb.temperature) {
        toEdit.wb.temperature = dontforceSet && options.baBehav[ADDSET_WB_TEMPERATURE] ? toEdit.wb.temperature + mods.wb.temperature : mods.wb.temperature;
    }

    if (wb.compat_version) {
        toEdit.wb.compat_version = mods.wb.compat_version;
    }

    //if (colorShift.a)                     toEdit.colorShift.a = dontforceSet && options.baBehav[ADDSET_CS_BLUEYELLOW] ? toEdit.colorShift.a + mods.colorShift.a : mods.colorShift.a;
    //if (colorShift.b)                     toEdit.colorShift.b = dontforceSet && options.baBehav[ADDSET_CS_GREENMAGENTA] ? toEdit.colorShift.b + mods.colorShift.b : mods.colorShift.b;
    //if (lumaDenoise.enabled)              toEdit.lumaDenoise.enabled = mods.lumaDenoise.enabled;
    //if (lumaDenoise.radius)                   toEdit.lumaDenoise.radius = mods.lumaDenoise.radius;
    //if (lumaDenoise.edgetolerance)            toEdit.lumaDenoise.edgetolerance = dontforceSet && options.baBehav[ADDSET_LD_EDGETOLERANCE] ? toEdit.lumaDenoise.edgetolerance + mods.lumaDenoise.edgetolerance : mods.lumaDenoise.edgetolerance;
    //if (colorDenoise.enabled)             toEdit.colorDenoise.enabled = mods.colorDenoise.enabled;
    //if (colorDenoise.amount)              toEdit.colorDenoise.amount = mods.colorDenoise.amount;

    if (defringe.enabled) {
        toEdit.defringe.enabled = mods.defringe.enabled;
    }

    if (defringe.radius) {
        toEdit.defringe.radius = mods.defringe.radius;
    }

    if (defringe.threshold) {
        toEdit.defringe.threshold = mods.defringe.threshold;
    }

    if (defringe.huecurve) {
        toEdit.defringe.huecurve = mods.defringe.huecurve;
    }

    if (colorappearance.curve) {
        toEdit.colorappearance.curve = mods.colorappearance.curve;
    }

    if (colorappearance.curvered) {
        toEdit.colorappearance.curvered = mods.colorappearance.curvered;
    }

    if (colorappearance.curvegreen) {
        toEdit.colorappearance.curvegreen = mods.colorappearance.curvegreen;
    }

    if (colorappearance.curveblue) {
        toEdit.colorappearance.curveblue = mods.colorappearance.curveblue;
    }

    if (colorappearance.curve2) {
        toEdit.colorappearance.curve2 = mods.colorappearance.curve2;
    }

    if (colorappearance.curve3) {
        toEdit.colorappearance.curve3 = mods.colorappearance.curve3;
    }

    if (colorappearance.curveMode) {
        toEdit.colorappearance.curveMode = mods.colorappearance.curveMode;
    }

    if (colorappearance.curveMode2) {
        toEdit.colorappearance.curveMode2 = mods.colorappearance.curveMode2;
    }

    if (colorappearance.curveMode3) {
        toEdit.colorappearance.curveMode3 = mods.colorappearance.curveMode3;
    }

    if (colorappearance.complexmethod) {
        toEdit.colorappearance.complexmethod = mods.colorappearance.complexmethod;
    }

    if (colorappearance.modelmethod) {
        toEdit.colorappearance.modelmethod = mods.colorappearance.modelmethod;
    }

    if (colorappearance.catmethod) {
        toEdit.colorappearance.catmethod = mods.colorappearance.catmethod;
    }

    if (colorappearance.enabled) {
        toEdit.colorappearance.enabled = mods.colorappearance.enabled;
    }

    if (colorappearance.degree) {
        toEdit.colorappearance.degree = dontforceSet && options.baBehav[ADDSET_CAT_DEGREE] ? toEdit.colorappearance.degree + mods.colorappearance.degree : mods.colorappearance.degree;
    }

    if (colorappearance.autodegree) {
        toEdit.colorappearance.autodegree = mods.colorappearance.autodegree;
    }

    if (colorappearance.degreeout) {
        toEdit.colorappearance.degreeout = dontforceSet && options.baBehav[ADDSET_CAT_DEGREEOUT] ? toEdit.colorappearance.degreeout + mods.colorappearance.degreeout : mods.colorappearance.degreeout;
    }

    if (colorappearance.autodegreeout) {
        toEdit.colorappearance.autodegreeout = mods.colorappearance.autodegreeout;
    }

    if (colorappearance.surround) {
        toEdit.colorappearance.surround = mods.colorappearance.surround;
    }

    if (colorappearance.surrsrc) {
        toEdit.colorappearance.surrsrc = mods.colorappearance.surrsrc;
    }

    if (colorappearance.autoadapscen) {
        toEdit.colorappearance.autoadapscen = mods.colorappearance.autoadapscen;
    }

    if (colorappearance.adapscen) {
        toEdit.colorappearance.adapscen = dontforceSet && options.baBehav[ADDSET_CAT_ADAPTSCENE] ? toEdit.colorappearance.adapscen + mods.colorappearance.adapscen : mods.colorappearance.adapscen;
    }

    if (colorappearance.autoybscen) {
        toEdit.colorappearance.autoybscen = mods.colorappearance.autoybscen;
    }

    if (colorappearance.ybscen) {
        toEdit.colorappearance.ybscen = mods.colorappearance.ybscen;
    }

    if (colorappearance.adaplum) {
        toEdit.colorappearance.adaplum = dontforceSet && options.baBehav[ADDSET_CAT_ADAPTVIEWING] ? toEdit.colorappearance.adaplum + mods.colorappearance.adaplum : mods.colorappearance.adaplum;
    }

    if (colorappearance.badpixsl) {
        toEdit.colorappearance.badpixsl = dontforceSet && options.baBehav[ADDSET_CAT_BADPIX] ? toEdit.colorappearance.badpixsl + mods.colorappearance.badpixsl : mods.colorappearance.badpixsl;
    }

    if (colorappearance.wbmodel) {
        toEdit.colorappearance.wbmodel = mods.colorappearance.wbmodel;
    }

    if (colorappearance.illum) {
        toEdit.colorappearance.illum = mods.colorappearance.illum;
    }

    if (colorappearance.algo) {
        toEdit.colorappearance.algo = mods.colorappearance.algo;
    }

    if (colorappearance.tempout) {
        toEdit.colorappearance.tempout = dontforceSet && options.baBehav[ADDSET_CAT_TEMPOUT] ? toEdit.colorappearance.tempout + mods.colorappearance.tempout : mods.colorappearance.tempout;
    }

    if (colorappearance.autotempout) {
        toEdit.colorappearance.autotempout = mods.colorappearance.autotempout;
    }

    if (colorappearance.greenout) {
        toEdit.colorappearance.greenout = mods.colorappearance.greenout;
    }

    if (colorappearance.tempsc) {
        toEdit.colorappearance.tempsc = mods.colorappearance.tempsc;
    }

    if (colorappearance.greensc) {
        toEdit.colorappearance.greensc = mods.colorappearance.greensc;
    }

    if (colorappearance.ybout) {
        toEdit.colorappearance.ybout = mods.colorappearance.ybout;
    }

    if (colorappearance.jlight) {
        toEdit.colorappearance.jlight = dontforceSet && options.baBehav[ADDSET_CAT_LIGHT] ? toEdit.colorappearance.jlight + mods.colorappearance.jlight : mods.colorappearance.jlight;
    }

    if (colorappearance.qbright) {
        toEdit.colorappearance.qbright = dontforceSet && options.baBehav[ADDSET_CAT_BRIGHT] ? toEdit.colorappearance.qbright + mods.colorappearance.qbright : mods.colorappearance.qbright;
    }

    if (colorappearance.chroma) {
        toEdit.colorappearance.chroma = dontforceSet && options.baBehav[ADDSET_CAT_CHROMA] ? toEdit.colorappearance.chroma + mods.colorappearance.chroma : mods.colorappearance.chroma;
    }

    if (colorappearance.schroma) {
        toEdit.colorappearance.schroma = dontforceSet && options.baBehav[ADDSET_CAT_CHROMA_S] ? toEdit.colorappearance.schroma + mods.colorappearance.schroma : mods.colorappearance.schroma;
    }

    if (colorappearance.schromared) {
        toEdit.colorappearance.schromared = dontforceSet && options.baBehav[ADDSET_CAT_CHROMA_S_RED] ? toEdit.colorappearance.schromared + mods.colorappearance.schromared : mods.colorappearance.schromared;
    }

    if (colorappearance.schromagreen) {
        toEdit.colorappearance.schromagreen = dontforceSet && options.baBehav[ADDSET_CAT_CHROMA_S_GREEN] ? toEdit.colorappearance.schromagreen + mods.colorappearance.schromagreen : mods.colorappearance.schromagreen;
    }

    if (colorappearance.schromablue) {
        toEdit.colorappearance.schromablue = dontforceSet && options.baBehav[ADDSET_CAT_CHROMA_S_BLUE] ? toEdit.colorappearance.schromablue + mods.colorappearance.schromablue : mods.colorappearance.schromablue;
    }

    if (colorappearance.mchroma) {
        toEdit.colorappearance.mchroma = dontforceSet && options.baBehav[ADDSET_CAT_CHROMA_M] ? toEdit.colorappearance.mchroma + mods.colorappearance.mchroma : mods.colorappearance.mchroma;
    }

    if (colorappearance.contrast) {
        toEdit.colorappearance.contrast = dontforceSet && options.baBehav[ADDSET_CAT_CONTRAST] ? toEdit.colorappearance.contrast + mods.colorappearance.contrast : mods.colorappearance.contrast;
    }

    if (colorappearance.qcontrast) {
        toEdit.colorappearance.qcontrast = dontforceSet && options.baBehav[ADDSET_CAT_CONTRAST_Q] ? toEdit.colorappearance.qcontrast + mods.colorappearance.qcontrast : mods.colorappearance.qcontrast;
    }

    if (colorappearance.colorh) {
        toEdit.colorappearance.colorh = dontforceSet && options.baBehav[ADDSET_CAT_HUE] ? toEdit.colorappearance.colorh + mods.colorappearance.colorh : mods.colorappearance.colorh;
    }

    if (colorappearance.colorhred) {
        toEdit.colorappearance.colorhred = dontforceSet && options.baBehav[ADDSET_CAT_HUE_RED] ? toEdit.colorappearance.colorhred + mods.colorappearance.colorhred : mods.colorappearance.colorhred;
    }

    if (colorappearance.colorhgreen) {
        toEdit.colorappearance.colorhgreen = dontforceSet && options.baBehav[ADDSET_CAT_HUE_GREEN] ? toEdit.colorappearance.colorhgreen + mods.colorappearance.colorhgreen : mods.colorappearance.colorhgreen;
    }

    if (colorappearance.colorhblue) {
        toEdit.colorappearance.colorhblue = dontforceSet && options.baBehav[ADDSET_CAT_HUE_BLUE] ? toEdit.colorappearance.colorhblue + mods.colorappearance.colorhblue : mods.colorappearance.colorhblue;
    }

    if (colorappearance.rstprotection) {
        toEdit.colorappearance.rstprotection = dontforceSet && options.baBehav[ADDSET_CAT_RSTPRO] ? toEdit.colorappearance.rstprotection + mods.colorappearance.rstprotection : mods.colorappearance.rstprotection;
    }

    if (colorappearance.surrsource) {
        toEdit.colorappearance.surrsource = mods.colorappearance.surrsource;
    }

    if (colorappearance.gamut) {
        toEdit.colorappearance.gamut = mods.colorappearance.gamut;
    }

//  if (colorappearance.badpix)             toEdit.colorappearance.badpix = mods.colorappearance.badpix;
    if (colorappearance.datacie) {
        toEdit.colorappearance.datacie = mods.colorappearance.datacie;
    }

    if (colorappearance.tonecie) {
        toEdit.colorappearance.tonecie = mods.colorappearance.tonecie;
    }


//  if (colorappearance.sharpcie)           toEdit.colorappearance.sharpcie = mods.colorappearance.sharpcie;
    if (impulseDenoise.enabled) {
        toEdit.impulseDenoise.enabled = mods.impulseDenoise.enabled;
    }

    if (impulseDenoise.thresh) {
        toEdit.impulseDenoise.thresh = mods.impulseDenoise.thresh;
    }

    if (dirpyrDenoise.enabled) {
        toEdit.dirpyrDenoise.enabled = mods.dirpyrDenoise.enabled;
    }

    if (dirpyrDenoise.enhance) {
        toEdit.dirpyrDenoise.enhance = mods.dirpyrDenoise.enhance;
    }

    if (dirpyrDenoise.median) {
        toEdit.dirpyrDenoise.median = mods.dirpyrDenoise.median;
    }

    if (dirpyrDenoise.luma) {
        toEdit.dirpyrDenoise.luma = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_LUMA] ? toEdit.dirpyrDenoise.luma + mods.dirpyrDenoise.luma : mods.dirpyrDenoise.luma;
    }

    if (dirpyrDenoise.lcurve) {
        toEdit.dirpyrDenoise.lcurve = mods.dirpyrDenoise.lcurve;
    }

    if (dirpyrDenoise.cccurve) {
        toEdit.dirpyrDenoise.cccurve = mods.dirpyrDenoise.cccurve;
    }

    if (dirpyrDenoise.Ldetail) {
        toEdit.dirpyrDenoise.Ldetail = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_LUMDET] ? toEdit.dirpyrDenoise.Ldetail + mods.dirpyrDenoise.Ldetail : mods.dirpyrDenoise.Ldetail;
    }

    if (dirpyrDenoise.chroma) {
        toEdit.dirpyrDenoise.chroma = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_CHROMA] ? toEdit.dirpyrDenoise.chroma + mods.dirpyrDenoise.chroma : mods.dirpyrDenoise.chroma;
    }

    if (dirpyrDenoise.redchro) {
        toEdit.dirpyrDenoise.redchro = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_CHROMARED] ? toEdit.dirpyrDenoise.redchro + mods.dirpyrDenoise.redchro : mods.dirpyrDenoise.redchro;
    }

    if (dirpyrDenoise.bluechro) {
        toEdit.dirpyrDenoise.bluechro = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_CHROMABLUE] ? toEdit.dirpyrDenoise.bluechro + mods.dirpyrDenoise.bluechro : mods.dirpyrDenoise.bluechro;
    }

    if (dirpyrDenoise.gain) {
        toEdit.dirpyrDenoise.autoGain = mods.dirpyrDenoise.autoGain;
    }

    if (dirpyrDenoise.gamma) {
        toEdit.dirpyrDenoise.gamma = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_GAMMA] ? toEdit.dirpyrDenoise.gamma + mods.dirpyrDenoise.gamma : mods.dirpyrDenoise.gamma;
    }

    if (dirpyrDenoise.passes) {
        toEdit.dirpyrDenoise.passes = dontforceSet && options.baBehav[ADDSET_DIRPYRDN_PASSES] ? toEdit.dirpyrDenoise.passes + mods.dirpyrDenoise.passes : mods.dirpyrDenoise.passes;
    }

//  if (dirpyrDenoise.perform)              toEdit.dirpyrDenoise.perform = mods.dirpyrDenoise.perform;
    if (dirpyrDenoise.dmethod) {
        toEdit.dirpyrDenoise.dmethod = mods.dirpyrDenoise.dmethod;
    }

    if (dirpyrDenoise.Lmethod) {
        toEdit.dirpyrDenoise.Lmethod = mods.dirpyrDenoise.Lmethod;
    }

    if (dirpyrDenoise.Cmethod) {
        toEdit.dirpyrDenoise.Cmethod = mods.dirpyrDenoise.Cmethod;
    }

    if (dirpyrDenoise.C2method) {
        toEdit.dirpyrDenoise.C2method = mods.dirpyrDenoise.C2method;
    }

    if (dirpyrDenoise.smethod) {
        toEdit.dirpyrDenoise.smethod = mods.dirpyrDenoise.smethod;
    }

    if (dirpyrDenoise.medmethod) {
        toEdit.dirpyrDenoise.medmethod = mods.dirpyrDenoise.medmethod;
    }

    if (dirpyrDenoise.methodmed) {
        toEdit.dirpyrDenoise.methodmed = mods.dirpyrDenoise.methodmed;
    }

    if (dirpyrDenoise.rgbmethod) {
        toEdit.dirpyrDenoise.rgbmethod = mods.dirpyrDenoise.rgbmethod;
    }

    if (epd.enabled) {
        toEdit.epd.enabled = mods.epd.enabled;
    }

    if (epd.strength) {
        toEdit.epd.strength = mods.epd.strength;
    }

    if (epd.gamma) {
        toEdit.epd.gamma = mods.epd.gamma;
    }

    if (epd.edgeStopping) {
        toEdit.epd.edgeStopping = mods.epd.edgeStopping;
    }

    if (epd.scale) {
        toEdit.epd.scale = mods.epd.scale;
    }

    if (epd.reweightingIterates) {
        toEdit.epd.reweightingIterates = mods.epd.reweightingIterates;
    }

    if (fattal.enabled) {
        toEdit.fattal.enabled = mods.fattal.enabled;
    }

    if (fattal.threshold) {
        toEdit.fattal.threshold = mods.fattal.threshold;
    }

    if (fattal.amount) {
        toEdit.fattal.amount = mods.fattal.amount;
    }

    if (fattal.anchor) {
        toEdit.fattal.anchor = mods.fattal.anchor;
    }

    if (sh.enabled) {
        toEdit.sh.enabled = mods.sh.enabled;
    }

    if (sh.highlights) {
        toEdit.sh.highlights = dontforceSet && options.baBehav[ADDSET_SH_HIGHLIGHTS] ? toEdit.sh.highlights + mods.sh.highlights : mods.sh.highlights;
    }

    if (sh.htonalwidth) {
        toEdit.sh.htonalwidth = mods.sh.htonalwidth;
    }

    if (sh.shadows) {
        toEdit.sh.shadows = dontforceSet && options.baBehav[ADDSET_SH_SHADOWS] ? toEdit.sh.shadows + mods.sh.shadows : mods.sh.shadows;
    }

    if (sh.stonalwidth) {
        toEdit.sh.stonalwidth = mods.sh.stonalwidth;
    }

    if (sh.radius) {
        toEdit.sh.radius = mods.sh.radius;
    }

    if (sh.lab) {
        toEdit.sh.lab = mods.sh.lab;
    }

    if (cg.th_c) {
        toEdit.cg.th_c = mods.cg.th_c;
    }

    if (cg.th_m) {
        toEdit.cg.th_m = mods.cg.th_m;
    }

    if (cg.th_y) {
        toEdit.cg.th_y = mods.cg.th_y;
    }

    if (cg.d_c) {
        toEdit.cg.d_c = mods.cg.d_c;
    }

    if (cg.autodc) {
        toEdit.cg.autodc = mods.cg.autodc;
    }

    if (cg.d_m) {
        toEdit.cg.d_m = mods.cg.d_m;
    }

    if (cg.autodm) {
        toEdit.cg.autodm = mods.cg.autodm;
    }
   
    if (cg.d_y) {
        toEdit.cg.d_y = mods.cg.d_y;
    }

    if (cg.autody) {
        toEdit.cg.autody = mods.cg.autody;
    }   

    if (cg.colorspace) {
        toEdit.cg.colorspace = mods.cg.colorspace;
    }

    if (cg.pwr) {
        toEdit.cg.pwr = mods.cg.pwr;
    }

    if (cg.rolloff) {
        toEdit.cg.rolloff = mods.cg.rolloff;
    }

    if (cg.enabled) {
        toEdit.cg.enabled = mods.cg.enabled;
    }

    if (toneEqualizer.enabled) {
        toEdit.toneEqualizer.enabled = mods.toneEqualizer.enabled;
    }

    for (size_t i = 0; i < toneEqualizer.bands.size(); ++i) {
        if (toneEqualizer.bands[i]) {
            toEdit.toneEqualizer.bands[i] =
                dontforceSet && options.baBehav[ADDSET_TONE_EQUALIZER_BANDS]
                    ? toEdit.toneEqualizer.bands[i] + mods.toneEqualizer.bands[i]
                    : mods.toneEqualizer.bands[i];
        }
    }

    if (toneEqualizer.regularization) {
        toEdit.toneEqualizer.regularization =
            dontforceSet && options.baBehav[ADDSET_TONE_EQUALIZER_REGULARIZATION]
                ? toEdit.toneEqualizer.regularization + mods.toneEqualizer.regularization
                : mods.toneEqualizer.regularization;
    }

    if (toneEqualizer.show_colormap) {
        toEdit.toneEqualizer.show_colormap = mods.toneEqualizer.show_colormap;
    }

    if (toneEqualizer.pivot) {
        toEdit.toneEqualizer.pivot =
            dontforceSet && options.baBehav[ADDSET_TONE_EQUALIZER_PIVOT]
                ? toEdit.toneEqualizer.pivot + mods.toneEqualizer.pivot
                : mods.toneEqualizer.pivot;
    }

    if (crop.enabled) {
        toEdit.crop.enabled = mods.crop.enabled;
    }

    if (crop.x) {
        toEdit.crop.x = mods.crop.x;
    }

    if (crop.y) {
        toEdit.crop.y = mods.crop.y;
    }

    if (crop.w) {
        toEdit.crop.w = mods.crop.w;
    }

    if (crop.h) {
        toEdit.crop.h = mods.crop.h;
    }

    if (crop.fixratio) {
        toEdit.crop.fixratio = mods.crop.fixratio;
    }

    if (crop.ratio) {
        toEdit.crop.ratio = mods.crop.ratio;
    }

    if (crop.orientation) {
        toEdit.crop.orientation = mods.crop.orientation;
    }

    ::combine(toEdit.cropGuide, mods.cropGuide, cropGuide);

    if (coarse.rotate) {
        toEdit.coarse.rotate = mods.coarse.rotate;
    }

    if (coarse.hflip) {
        toEdit.coarse.hflip = mods.coarse.hflip;
    }

    if (coarse.vflip) {
        toEdit.coarse.vflip = mods.coarse.vflip;
    }

    if (commonTrans.method) {
        toEdit.commonTrans.method = mods.commonTrans.method;
    }

    if (commonTrans.scale) {
        toEdit.commonTrans.scale = dontforceSet && options.baBehav[ADDSET_LENSGEOM_SCALE] ? toEdit.commonTrans.scale + mods.commonTrans.scale : mods.commonTrans.scale;
    }

    if (commonTrans.scale_horizontally) {
        toEdit.commonTrans.scale_horizontally = dontforceSet && options.baBehav[ADDSET_LENSGEOM_SCALE_HORIZONTALLY] ? toEdit.commonTrans.scale_horizontally + mods.commonTrans.scale_horizontally : mods.commonTrans.scale_horizontally;
    }

    if (commonTrans.scale_vertically) {
        toEdit.commonTrans.scale_vertically = dontforceSet && options.baBehav[ADDSET_LENSGEOM_SCALE_VERTICALLY] ? toEdit.commonTrans.scale_vertically + mods.commonTrans.scale_vertically : mods.commonTrans.scale_vertically;
    }

    if (commonTrans.autofill) {
        toEdit.commonTrans.autofill = mods.commonTrans.autofill;
    }

    if (rotate.degree) {
        toEdit.rotate.degree = dontforceSet && options.baBehav[ADDSET_ROTATE_DEGREE] ? toEdit.rotate.degree + mods.rotate.degree : mods.rotate.degree;
    }

    if (distortion.amount) {
        toEdit.distortion.amount = dontforceSet && options.baBehav[ADDSET_DIST_AMOUNT] ? toEdit.distortion.amount + mods.distortion.amount : mods.distortion.amount;
    }

    if (distortion.defish) {
        toEdit.distortion.defish = mods.distortion.defish;
    }

    if (distortion.focal_length) {
        toEdit.distortion.focal_length = dontforceSet && options.baBehav[ADDSET_DIST_FOCAL_LENGTH] ? toEdit.distortion.focal_length + mods.distortion.focal_length : mods.distortion.focal_length;
    }

    if (lensProf.lcMode) {
        toEdit.lensProf.lcMode = mods.lensProf.lcMode;
    }

    if (lensProf.lcpFile) {
        toEdit.lensProf.lcpFile = mods.lensProf.lcpFile;
    }

    if (lensProf.useDist) {
        toEdit.lensProf.useDist = mods.lensProf.useDist;
    }

    if (lensProf.useVign) {
        toEdit.lensProf.useVign = mods.lensProf.useVign;
    }

    if (lensProf.useCA) {
        toEdit.lensProf.useCA = mods.lensProf.useCA;
    }

    if (lensProf.lfCameraMake) {
        toEdit.lensProf.lfCameraMake = mods.lensProf.lfCameraMake;
    }

    if (lensProf.lfCameraModel) {
        toEdit.lensProf.lfCameraModel = mods.lensProf.lfCameraModel;
    }

    if (lensProf.lfLens) {
        toEdit.lensProf.lfLens = mods.lensProf.lfLens;
    }

    if (perspective.method) {
        toEdit.perspective.method = mods.perspective.method;
    }

    if (perspective.horizontal) {
        toEdit.perspective.horizontal = dontforceSet && options.baBehav[ADDSET_PERSPECTIVE] ? toEdit.perspective.horizontal + mods.perspective.horizontal : mods.perspective.horizontal;
    }

    if (perspective.vertical) {
        toEdit.perspective.vertical = dontforceSet && options.baBehav[ADDSET_PERSPECTIVE] ? toEdit.perspective.vertical + mods.perspective.vertical : mods.perspective.vertical;
    }

    if (perspective.camera_crop_factor) {
        toEdit.perspective.camera_crop_factor = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_FOCAL_LENGTH] ? toEdit.perspective.camera_crop_factor + mods.perspective.camera_crop_factor : mods.perspective.camera_crop_factor;
    }

    if (perspective.camera_focal_length) {
        toEdit.perspective.camera_focal_length = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_FOCAL_LENGTH] ? toEdit.perspective.camera_focal_length + mods.perspective.camera_focal_length : mods.perspective.camera_focal_length;
    }

    if (perspective.camera_pitch) {
        toEdit.perspective.camera_pitch = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_ANGLE] ? toEdit.perspective.camera_pitch + mods.perspective.camera_pitch : mods.perspective.camera_pitch;
    }

    if (perspective.camera_roll) {
        toEdit.perspective.camera_roll = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_ANGLE] ? toEdit.perspective.camera_roll + mods.perspective.camera_roll : mods.perspective.camera_roll;
    }

    if (perspective.camera_shift_horiz) {
        toEdit.perspective.camera_shift_horiz = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_SHIFT] ? toEdit.perspective.camera_shift_horiz + mods.perspective.camera_shift_horiz : mods.perspective.camera_shift_horiz;
    }

    if (perspective.camera_shift_vert) {
        toEdit.perspective.camera_shift_vert = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_SHIFT] ? toEdit.perspective.camera_shift_vert + mods.perspective.camera_shift_vert : mods.perspective.camera_shift_vert;
    }

    if (perspective.camera_yaw) {
        toEdit.perspective.camera_yaw = dontforceSet && options.baBehav[ADDSET_PERSP_CAM_ANGLE] ? toEdit.perspective.camera_yaw + mods.perspective.camera_yaw : mods.perspective.camera_yaw;
    }

    if (perspective.projection_pitch) {
        toEdit.perspective.projection_pitch = dontforceSet && options.baBehav[ADDSET_PERSP_PROJ_ANGLE] ? toEdit.perspective.projection_pitch + mods.perspective.projection_pitch : mods.perspective.projection_pitch;
    }

    if (perspective.projection_rotate) {
        toEdit.perspective.projection_rotate = dontforceSet && options.baBehav[ADDSET_PERSP_PROJ_ROTATE] ? toEdit.perspective.projection_rotate + mods.perspective.projection_rotate : mods.perspective.projection_rotate;
    }

    if (perspective.projection_shift_horiz) {
        toEdit.perspective.projection_shift_horiz = dontforceSet && options.baBehav[ADDSET_PERSP_PROJ_SHIFT] ? toEdit.perspective.projection_shift_horiz + mods.perspective.projection_shift_horiz : mods.perspective.projection_shift_horiz;
    }

    if (perspective.projection_shift_vert) {
        toEdit.perspective.projection_shift_vert = dontforceSet && options.baBehav[ADDSET_PERSP_PROJ_SHIFT] ? toEdit.perspective.projection_shift_vert + mods.perspective.projection_shift_vert : mods.perspective.projection_shift_vert;
    }

    if (perspective.projection_yaw) {
        toEdit.perspective.projection_yaw = dontforceSet && options.baBehav[ADDSET_PERSP_PROJ_ANGLE] ? toEdit.perspective.projection_yaw + mods.perspective.projection_yaw : mods.perspective.projection_yaw;
    }

    if (perspective.control_lines) {
        toEdit.perspective.control_line_values = mods.perspective.control_line_values;
        toEdit.perspective.control_line_types = mods.perspective.control_line_types;
    }

    if (gradient.enabled) {
        toEdit.gradient.enabled = mods.gradient.enabled;
    }

    if (gradient.degree) {
        toEdit.gradient.degree = dontforceSet && options.baBehav[ADDSET_GRADIENT_DEGREE] ? toEdit.gradient.degree + mods.gradient.degree : mods.gradient.degree;
    }

    if (gradient.feather) {
        toEdit.gradient.feather = dontforceSet && options.baBehav[ADDSET_GRADIENT_FEATHER] ? toEdit.gradient.feather + mods.gradient.feather : mods.gradient.feather;
    }

    if (gradient.strength) {
        toEdit.gradient.strength = dontforceSet && options.baBehav[ADDSET_GRADIENT_STRENGTH] ? toEdit.gradient.strength + mods.gradient.strength : mods.gradient.strength;
    }

    if (gradient.centerX) {
        toEdit.gradient.centerX = dontforceSet && options.baBehav[ADDSET_GRADIENT_CENTER] ? toEdit.gradient.centerX + mods.gradient.centerX : mods.gradient.centerX;
    }

    if (gradient.centerY) {
        toEdit.gradient.centerY = dontforceSet && options.baBehav[ADDSET_GRADIENT_CENTER] ? toEdit.gradient.centerY + mods.gradient.centerY : mods.gradient.centerY;
    }


    if (locallab.enabled) {
        toEdit.locallab.enabled   = mods.locallab.enabled;

        // In that case, locallab is impacted by the combine:
        // Resizing locallab spots vector according to pedited
        toEdit.locallab.spots.resize(locallab.spots.size());
    }

    if (locallab.selspot) {
        toEdit.locallab.selspot   = mods.locallab.selspot;
    }

    // Updating each locallab spot according to pedited
    for (size_t i = 0; i < toEdit.locallab.spots.size() && i < mods.locallab.spots.size() && i < locallab.spots.size(); i++) {
        // Control spot settings
        if (locallab.spots.at(i).name) {
            toEdit.locallab.spots.at(i).name = mods.locallab.spots.at(i).name;
        }

        if (locallab.spots.at(i).isvisible) {
            toEdit.locallab.spots.at(i).isvisible = mods.locallab.spots.at(i).isvisible;
        }

        if (locallab.spots.at(i).prevMethod) {
            toEdit.locallab.spots.at(i).prevMethod = mods.locallab.spots.at(i).prevMethod;
        }

        if (locallab.spots.at(i).shape) {
            toEdit.locallab.spots.at(i).shape = mods.locallab.spots.at(i).shape;
        }

        if (locallab.spots.at(i).spotMethod) {
            toEdit.locallab.spots.at(i).spotMethod = mods.locallab.spots.at(i).spotMethod;
        }

        if (locallab.spots.at(i).wavMethod) {
            toEdit.locallab.spots.at(i).wavMethod = mods.locallab.spots.at(i).wavMethod;
        }

        if (locallab.spots.at(i).sensiexclu) {
            toEdit.locallab.spots.at(i).sensiexclu = mods.locallab.spots.at(i).sensiexclu;
        }

        if (locallab.spots.at(i).structexclu) {
            toEdit.locallab.spots.at(i).structexclu = mods.locallab.spots.at(i).structexclu;
        }

        if (locallab.spots.at(i).struc) {
            toEdit.locallab.spots.at(i).struc = mods.locallab.spots.at(i).struc;
        }

        if (locallab.spots.at(i).shapeMethod) {
            toEdit.locallab.spots.at(i).shapeMethod = mods.locallab.spots.at(i).shapeMethod;
        }

        if (locallab.spots.at(i).avoidgamutMethod) {
            toEdit.locallab.spots.at(i).avoidgamutMethod = mods.locallab.spots.at(i).avoidgamutMethod;
        }

        if (locallab.spots.at(i).loc) {
            toEdit.locallab.spots.at(i).loc = mods.locallab.spots.at(i).loc;
        }

        if (locallab.spots.at(i).centerX) {
            toEdit.locallab.spots.at(i).centerX = mods.locallab.spots.at(i).centerX;
        }

        if (locallab.spots.at(i).centerY) {
            toEdit.locallab.spots.at(i).centerY = mods.locallab.spots.at(i).centerY;
        }

        if (locallab.spots.at(i).spotangle) {
            toEdit.locallab.spots.at(i).spotangle = mods.locallab.spots.at(i).spotangle;
        }

        if (locallab.spots.at(i).circrad) {
            toEdit.locallab.spots.at(i).circrad = mods.locallab.spots.at(i).circrad;
        }

        if (locallab.spots.at(i).qualityMethod) {
            toEdit.locallab.spots.at(i).qualityMethod = mods.locallab.spots.at(i).qualityMethod;
        }

        if (locallab.spots.at(i).complexMethod) {
            toEdit.locallab.spots.at(i).complexMethod = mods.locallab.spots.at(i).complexMethod;
        }

        if (locallab.spots.at(i).transit) {
            toEdit.locallab.spots.at(i).transit = mods.locallab.spots.at(i).transit;
        }

        if (locallab.spots.at(i).feather) {
            toEdit.locallab.spots.at(i).feather = mods.locallab.spots.at(i).feather;
        }

        if (locallab.spots.at(i).thresh) {
            toEdit.locallab.spots.at(i).thresh = mods.locallab.spots.at(i).thresh;
        }

        if (locallab.spots.at(i).iter) {
            toEdit.locallab.spots.at(i).iter = mods.locallab.spots.at(i).iter;
        }

        if (locallab.spots.at(i).balan) {
            toEdit.locallab.spots.at(i).balan = mods.locallab.spots.at(i).balan;
        }

        if (locallab.spots.at(i).balanh) {
            toEdit.locallab.spots.at(i).balanh = mods.locallab.spots.at(i).balanh;
        }

        if (locallab.spots.at(i).colorde) {
            toEdit.locallab.spots.at(i).colorde = mods.locallab.spots.at(i).colorde;
        }

        if (locallab.spots.at(i).colorscope) {
            toEdit.locallab.spots.at(i).colorscope = mods.locallab.spots.at(i).colorscope;
        }

        if (locallab.spots.at(i).avoidrad) {
            toEdit.locallab.spots.at(i).avoidrad = mods.locallab.spots.at(i).avoidrad;
        }

        if (locallab.spots.at(i).transitweak) {
            toEdit.locallab.spots.at(i).transitweak = mods.locallab.spots.at(i).transitweak;
        }

        if (locallab.spots.at(i).transitgrad) {
            toEdit.locallab.spots.at(i).transitgrad = mods.locallab.spots.at(i).transitgrad;
        }

        if (locallab.spots.at(i).hishow) {
            toEdit.locallab.spots.at(i).hishow = mods.locallab.spots.at(i).hishow;
        }

        if (locallab.spots.at(i).activ) {
            toEdit.locallab.spots.at(i).activ = mods.locallab.spots.at(i).activ;
        }

        if (locallab.spots.at(i).avoidneg) {
            toEdit.locallab.spots.at(i).avoidneg = mods.locallab.spots.at(i).avoidneg;
        }

        if (locallab.spots.at(i).blwh) {
            toEdit.locallab.spots.at(i).blwh = mods.locallab.spots.at(i).blwh;
        }

        if (locallab.spots.at(i).recurs) {
            toEdit.locallab.spots.at(i).recurs = mods.locallab.spots.at(i).recurs;
        }

        if (locallab.spots.at(i).laplac) {
            toEdit.locallab.spots.at(i).laplac = mods.locallab.spots.at(i).laplac;
        }

        if (locallab.spots.at(i).deltae) {
            toEdit.locallab.spots.at(i).deltae = mods.locallab.spots.at(i).deltae;
        }

        if (locallab.spots.at(i).shortc) {
            toEdit.locallab.spots.at(i).shortc = mods.locallab.spots.at(i).shortc;
        }

        if (locallab.spots.at(i).savrest) {
            toEdit.locallab.spots.at(i).savrest = mods.locallab.spots.at(i).savrest;
        }

        if (locallab.spots.at(i).denoichmask) {
            toEdit.locallab.spots.at(i).denoichmask = mods.locallab.spots.at(i).denoichmask;
        }

        if (locallab.spots.at(i).scopemask) {
            toEdit.locallab.spots.at(i).scopemask = mods.locallab.spots.at(i).scopemask;
        }

        if (locallab.spots.at(i).lumask) {
            toEdit.locallab.spots.at(i).lumask = mods.locallab.spots.at(i).lumask;
        }

        // Color & Light
        if (locallab.spots.at(i).visicolor) {
            toEdit.locallab.spots.at(i).visicolor = mods.locallab.spots.at(i).visicolor;
        }

        if (locallab.spots.at(i).expcolor) {
            toEdit.locallab.spots.at(i).expcolor = mods.locallab.spots.at(i).expcolor;
        }

        if (locallab.spots.at(i).complexcolor) {
            toEdit.locallab.spots.at(i).complexcolor = mods.locallab.spots.at(i).complexcolor;
        }

        if (locallab.spots.at(i).curvactiv) {
            toEdit.locallab.spots.at(i).curvactiv = mods.locallab.spots.at(i).curvactiv;
        }

        if (locallab.spots.at(i).lightness) {
            toEdit.locallab.spots.at(i).lightness = mods.locallab.spots.at(i).lightness;
        }

        if (locallab.spots.at(i).reparcol) {
            toEdit.locallab.spots.at(i).reparcol = mods.locallab.spots.at(i).reparcol;
        }

        if (locallab.spots.at(i).gamc) {
            toEdit.locallab.spots.at(i).gamc = mods.locallab.spots.at(i).gamc;
        }

        if (locallab.spots.at(i).contrast) {
            toEdit.locallab.spots.at(i).contrast = mods.locallab.spots.at(i).contrast;
        }

        if (locallab.spots.at(i).chroma) {
            toEdit.locallab.spots.at(i).chroma = mods.locallab.spots.at(i).chroma;
        }

        if (locallab.spots.at(i).labgridALow) {
            toEdit.locallab.spots.at(i).labgridALow = mods.locallab.spots.at(i).labgridALow;
        }

        if (locallab.spots.at(i).labgridBLow) {
            toEdit.locallab.spots.at(i).labgridBLow = mods.locallab.spots.at(i).labgridBLow;
        }

        if (locallab.spots.at(i).labgridAHigh) {
            toEdit.locallab.spots.at(i).labgridAHigh = mods.locallab.spots.at(i).labgridAHigh;
        }

        if (locallab.spots.at(i).labgridBHigh) {
            toEdit.locallab.spots.at(i).labgridBHigh = mods.locallab.spots.at(i).labgridBHigh;
        }

        if (locallab.spots.at(i).labgridALowmerg) {
            toEdit.locallab.spots.at(i).labgridALowmerg = mods.locallab.spots.at(i).labgridALowmerg;
        }

        if (locallab.spots.at(i).labgridBLowmerg) {
            toEdit.locallab.spots.at(i).labgridBLowmerg = mods.locallab.spots.at(i).labgridBLowmerg;
        }

        if (locallab.spots.at(i).labgridAHighmerg) {
            toEdit.locallab.spots.at(i).labgridAHighmerg = mods.locallab.spots.at(i).labgridAHighmerg;
        }

        if (locallab.spots.at(i).labgridBHighmerg) {
            toEdit.locallab.spots.at(i).labgridBHighmerg = mods.locallab.spots.at(i).labgridBHighmerg;
        }

        if (locallab.spots.at(i).strengthgrid) {
            toEdit.locallab.spots.at(i).strengthgrid = mods.locallab.spots.at(i).strengthgrid;
        }

        if (locallab.spots.at(i).sensi) {
            toEdit.locallab.spots.at(i).sensi = mods.locallab.spots.at(i).sensi;
        }

        if (locallab.spots.at(i).structcol) {
            toEdit.locallab.spots.at(i).structcol = mods.locallab.spots.at(i).structcol;
        }

        if (locallab.spots.at(i).strcol) {
            toEdit.locallab.spots.at(i).strcol = mods.locallab.spots.at(i).strcol;
        }

        if (locallab.spots.at(i).strcolab) {
            toEdit.locallab.spots.at(i).strcolab = mods.locallab.spots.at(i).strcolab;
        }

        if (locallab.spots.at(i).strcolh) {
            toEdit.locallab.spots.at(i).strcolh = mods.locallab.spots.at(i).strcolh;
        }

        if (locallab.spots.at(i).angcol) {
            toEdit.locallab.spots.at(i).angcol = mods.locallab.spots.at(i).angcol;
        }

        if (locallab.spots.at(i).feathercol) {
            toEdit.locallab.spots.at(i).feathercol = mods.locallab.spots.at(i).feathercol;
        }

        if (locallab.spots.at(i).blurcolde) {
            toEdit.locallab.spots.at(i).blurcolde = mods.locallab.spots.at(i).blurcolde;
        }

        if (locallab.spots.at(i).blurcol) {
            toEdit.locallab.spots.at(i).blurcol = mods.locallab.spots.at(i).blurcol;
        }

        if (locallab.spots.at(i).contcol) {
            toEdit.locallab.spots.at(i).contcol = mods.locallab.spots.at(i).contcol;
        }

        if (locallab.spots.at(i).blendmaskcol) {
            toEdit.locallab.spots.at(i).blendmaskcol = mods.locallab.spots.at(i).blendmaskcol;
        }

        if (locallab.spots.at(i).radmaskcol) {
            toEdit.locallab.spots.at(i).radmaskcol = mods.locallab.spots.at(i).radmaskcol;
        }

        if (locallab.spots.at(i).chromaskcol) {
            toEdit.locallab.spots.at(i).chromaskcol = mods.locallab.spots.at(i).chromaskcol;
        }

        if (locallab.spots.at(i).gammaskcol) {
            toEdit.locallab.spots.at(i).gammaskcol = mods.locallab.spots.at(i).gammaskcol;
        }

        if (locallab.spots.at(i).slomaskcol) {
            toEdit.locallab.spots.at(i).slomaskcol = mods.locallab.spots.at(i).slomaskcol;
        }

        if (locallab.spots.at(i).shadmaskcol) {
            toEdit.locallab.spots.at(i).shadmaskcol = mods.locallab.spots.at(i).shadmaskcol;
        }

        if (locallab.spots.at(i).strumaskcol) {
            toEdit.locallab.spots.at(i).strumaskcol = mods.locallab.spots.at(i).strumaskcol;
        }

        if (locallab.spots.at(i).lapmaskcol) {
            toEdit.locallab.spots.at(i).lapmaskcol = mods.locallab.spots.at(i).lapmaskcol;
        }

        if (locallab.spots.at(i).qualitycurveMethod) {
            toEdit.locallab.spots.at(i).qualitycurveMethod = mods.locallab.spots.at(i).qualitycurveMethod;
        }

        if (locallab.spots.at(i).gridMethod) {
            toEdit.locallab.spots.at(i).gridMethod = mods.locallab.spots.at(i).gridMethod;
        }

        if (locallab.spots.at(i).merMethod) {
            toEdit.locallab.spots.at(i).merMethod = mods.locallab.spots.at(i).merMethod;
        }

        if (locallab.spots.at(i).toneMethod) {
            toEdit.locallab.spots.at(i).toneMethod = mods.locallab.spots.at(i).toneMethod;
        }

        if (locallab.spots.at(i).mergecolMethod) {
            toEdit.locallab.spots.at(i).mergecolMethod = mods.locallab.spots.at(i).mergecolMethod;
        }

        if (locallab.spots.at(i).llcurve) {
            toEdit.locallab.spots.at(i).llcurve = mods.locallab.spots.at(i).llcurve;
        }

        if (locallab.spots.at(i).lccurve) {
            toEdit.locallab.spots.at(i).lccurve = mods.locallab.spots.at(i).lccurve;
        }

        if (locallab.spots.at(i).cccurve) {
            toEdit.locallab.spots.at(i).cccurve = mods.locallab.spots.at(i).cccurve;
        }

        if (locallab.spots.at(i).clcurve) {
            toEdit.locallab.spots.at(i).clcurve = mods.locallab.spots.at(i).clcurve;
        }

        if (locallab.spots.at(i).rgbcurve) {
            toEdit.locallab.spots.at(i).rgbcurve = mods.locallab.spots.at(i).rgbcurve;
        }

        if (locallab.spots.at(i).LHcurve) {
            toEdit.locallab.spots.at(i).LHcurve = mods.locallab.spots.at(i).LHcurve;
        }

        if (locallab.spots.at(i).HHcurve) {
            toEdit.locallab.spots.at(i).HHcurve = mods.locallab.spots.at(i).HHcurve;
        }

        if (locallab.spots.at(i).CHcurve) {
            toEdit.locallab.spots.at(i).CHcurve = mods.locallab.spots.at(i).CHcurve;
        }

        if (locallab.spots.at(i).invers) {
            toEdit.locallab.spots.at(i).invers = mods.locallab.spots.at(i).invers;
        }

        if (locallab.spots.at(i).special) {
            toEdit.locallab.spots.at(i).special = mods.locallab.spots.at(i).special;
        }

        if (locallab.spots.at(i).toolcol) {
            toEdit.locallab.spots.at(i).toolcol = mods.locallab.spots.at(i).toolcol;
        }

        if (locallab.spots.at(i).enaColorMask) {
            toEdit.locallab.spots.at(i).enaColorMask = mods.locallab.spots.at(i).enaColorMask;
        }

        if (locallab.spots.at(i).fftColorMask) {
            toEdit.locallab.spots.at(i).fftColorMask = mods.locallab.spots.at(i).fftColorMask;
        }

        if (locallab.spots.at(i).CCmaskcurve) {
            toEdit.locallab.spots.at(i).CCmaskcurve = mods.locallab.spots.at(i).CCmaskcurve;
        }

        if (locallab.spots.at(i).LLmaskcurve) {
            toEdit.locallab.spots.at(i).LLmaskcurve = mods.locallab.spots.at(i).LLmaskcurve;
        }

        if (locallab.spots.at(i).HHmaskcurve) {
            toEdit.locallab.spots.at(i).HHmaskcurve = mods.locallab.spots.at(i).HHmaskcurve;
        }

        if (locallab.spots.at(i).HHhmaskcurve) {
            toEdit.locallab.spots.at(i).HHhmaskcurve = mods.locallab.spots.at(i).HHhmaskcurve;
        }

        if (locallab.spots.at(i).softradiuscol) {
            toEdit.locallab.spots.at(i).softradiuscol = mods.locallab.spots.at(i).softradiuscol;
        }

        if (locallab.spots.at(i).opacol) {
            toEdit.locallab.spots.at(i).opacol = mods.locallab.spots.at(i).opacol;
        }

        if (locallab.spots.at(i).mercol) {
            toEdit.locallab.spots.at(i).mercol = mods.locallab.spots.at(i).mercol;
        }

        if (locallab.spots.at(i).merlucol) {
            toEdit.locallab.spots.at(i).merlucol = mods.locallab.spots.at(i).merlucol;
        }

        if (locallab.spots.at(i).conthrcol) {
            toEdit.locallab.spots.at(i).conthrcol = mods.locallab.spots.at(i).conthrcol;
        }

        if (locallab.spots.at(i).Lmaskcurve) {
            toEdit.locallab.spots.at(i).Lmaskcurve = mods.locallab.spots.at(i).Lmaskcurve;
        }

        if (locallab.spots.at(i).LLmaskcolcurvewav) {
            toEdit.locallab.spots.at(i).LLmaskcolcurvewav = mods.locallab.spots.at(i).LLmaskcolcurvewav;
        }

        if (locallab.spots.at(i).csthresholdcol) {
            toEdit.locallab.spots.at(i).csthresholdcol = mods.locallab.spots.at(i).csthresholdcol;
        }

        if (locallab.spots.at(i).recothresc) {
            toEdit.locallab.spots.at(i).recothresc = mods.locallab.spots.at(i).recothresc;
        }

        if (locallab.spots.at(i).lowthresc) {
            toEdit.locallab.spots.at(i).lowthresc = mods.locallab.spots.at(i).lowthresc;
        }

        if (locallab.spots.at(i).higthresc) {
            toEdit.locallab.spots.at(i).higthresc = mods.locallab.spots.at(i).higthresc;
        }

        if (locallab.spots.at(i).decayc) {
            toEdit.locallab.spots.at(i).decayc = mods.locallab.spots.at(i).decayc;
        }

        // Exposure
        if (locallab.spots.at(i).visiexpose) {
            toEdit.locallab.spots.at(i).visiexpose = mods.locallab.spots.at(i).visiexpose;
        }

        if (locallab.spots.at(i).expexpose) {
            toEdit.locallab.spots.at(i).expexpose = mods.locallab.spots.at(i).expexpose;
        }

        if (locallab.spots.at(i).complexexpose) {
            toEdit.locallab.spots.at(i).complexexpose = mods.locallab.spots.at(i).complexexpose;
        }

        if (locallab.spots.at(i).expcomp) {
            toEdit.locallab.spots.at(i).expcomp = mods.locallab.spots.at(i).expcomp;
        }

        if (locallab.spots.at(i).hlcompr) {
            toEdit.locallab.spots.at(i).hlcompr = mods.locallab.spots.at(i).hlcompr;
        }

        if (locallab.spots.at(i).hlcomprthresh) {
            toEdit.locallab.spots.at(i).hlcomprthresh = mods.locallab.spots.at(i).hlcomprthresh;
        }

        if (locallab.spots.at(i).black) {
            toEdit.locallab.spots.at(i).black = mods.locallab.spots.at(i).black;
        }

        if (locallab.spots.at(i).shadex) {
            toEdit.locallab.spots.at(i).shadex = mods.locallab.spots.at(i).shadex;
        }

        if (locallab.spots.at(i).shcompr) {
            toEdit.locallab.spots.at(i).shcompr = mods.locallab.spots.at(i).shcompr;
        }

        if (locallab.spots.at(i).expchroma) {
            toEdit.locallab.spots.at(i).expchroma = mods.locallab.spots.at(i).expchroma;
        }

        if (locallab.spots.at(i).sensiex) {
            toEdit.locallab.spots.at(i).sensiex = mods.locallab.spots.at(i).sensiex;
        }

        if (locallab.spots.at(i).structexp) {
            toEdit.locallab.spots.at(i).structexp = mods.locallab.spots.at(i).structexp;
        }

        if (locallab.spots.at(i).gamex) {
            toEdit.locallab.spots.at(i).gamex = mods.locallab.spots.at(i).gamex;
        }

        if (locallab.spots.at(i).blurexpde) {
            toEdit.locallab.spots.at(i).blurexpde = mods.locallab.spots.at(i).blurexpde;
        }

        if (locallab.spots.at(i).gamex) {
            toEdit.locallab.spots.at(i).gamex = mods.locallab.spots.at(i).gamex;
        }

        if (locallab.spots.at(i).strexp) {
            toEdit.locallab.spots.at(i).strexp = mods.locallab.spots.at(i).strexp;
        }

        if (locallab.spots.at(i).angexp) {
            toEdit.locallab.spots.at(i).angexp = mods.locallab.spots.at(i).angexp;
        }

        if (locallab.spots.at(i).featherexp) {
            toEdit.locallab.spots.at(i).featherexp = mods.locallab.spots.at(i).featherexp;
        }

        if (locallab.spots.at(i).excurve) {
            toEdit.locallab.spots.at(i).excurve = mods.locallab.spots.at(i).excurve;
        }

        if (locallab.spots.at(i).norm) {
            toEdit.locallab.spots.at(i).norm = mods.locallab.spots.at(i).norm;
        }

        if (locallab.spots.at(i).inversex) {
            toEdit.locallab.spots.at(i).inversex = mods.locallab.spots.at(i).inversex;
        }

        if (locallab.spots.at(i).enaExpMask) {
            toEdit.locallab.spots.at(i).enaExpMask = mods.locallab.spots.at(i).enaExpMask;
        }

        if (locallab.spots.at(i).enaExpMaskaft) {
            toEdit.locallab.spots.at(i).enaExpMaskaft = mods.locallab.spots.at(i).enaExpMaskaft;
        }

        if (locallab.spots.at(i).CCmaskexpcurve) {
            toEdit.locallab.spots.at(i).CCmaskexpcurve = mods.locallab.spots.at(i).CCmaskexpcurve;
        }

        if (locallab.spots.at(i).LLmaskexpcurve) {
            toEdit.locallab.spots.at(i).LLmaskexpcurve = mods.locallab.spots.at(i).LLmaskexpcurve;
        }

        if (locallab.spots.at(i).HHmaskexpcurve) {
            toEdit.locallab.spots.at(i).HHmaskexpcurve = mods.locallab.spots.at(i).HHmaskexpcurve;
        }

        if (locallab.spots.at(i).blendmaskexp) {
            toEdit.locallab.spots.at(i).blendmaskexp = mods.locallab.spots.at(i).blendmaskexp;
        }

        if (locallab.spots.at(i).radmaskexp) {
            toEdit.locallab.spots.at(i).radmaskexp = mods.locallab.spots.at(i).radmaskexp;
        }

        if (locallab.spots.at(i).chromaskexp) {
            toEdit.locallab.spots.at(i).chromaskexp = mods.locallab.spots.at(i).chromaskexp;
        }

        if (locallab.spots.at(i).gammaskexp) {
            toEdit.locallab.spots.at(i).gammaskexp = mods.locallab.spots.at(i).gammaskexp;
        }

        if (locallab.spots.at(i).slomaskexp) {
            toEdit.locallab.spots.at(i).slomaskexp = mods.locallab.spots.at(i).slomaskexp;
        }

        if (locallab.spots.at(i).lapmaskexp) {
            toEdit.locallab.spots.at(i).lapmaskexp = mods.locallab.spots.at(i).lapmaskexp;
        }

        if (locallab.spots.at(i).strmaskexp) {
            toEdit.locallab.spots.at(i).strmaskexp = mods.locallab.spots.at(i).strmaskexp;
        }

        if (locallab.spots.at(i).angmaskexp) {
            toEdit.locallab.spots.at(i).angmaskexp = mods.locallab.spots.at(i).angmaskexp;
        }

        if (locallab.spots.at(i).softradiusexp) {
            toEdit.locallab.spots.at(i).softradiusexp = mods.locallab.spots.at(i).softradiusexp;
        }

        if (locallab.spots.at(i).Lmaskexpcurve) {
            toEdit.locallab.spots.at(i).Lmaskexpcurve = mods.locallab.spots.at(i).Lmaskexpcurve;
        }

        if (locallab.spots.at(i).expMethod) {
            toEdit.locallab.spots.at(i).expMethod = mods.locallab.spots.at(i).expMethod;
        }

        if (locallab.spots.at(i).exnoiseMethod) {
            toEdit.locallab.spots.at(i).exnoiseMethod = mods.locallab.spots.at(i).exnoiseMethod;
        }

        if (locallab.spots.at(i).laplacexp) {
            toEdit.locallab.spots.at(i).laplacexp = mods.locallab.spots.at(i).laplacexp;
        }

        if (locallab.spots.at(i).laplacexp) {
            toEdit.locallab.spots.at(i).laplacexp = mods.locallab.spots.at(i).laplacexp;
        }

        if (locallab.spots.at(i).reparexp) {
            toEdit.locallab.spots.at(i).reparexp = mods.locallab.spots.at(i).reparexp;
        }

        if (locallab.spots.at(i).linear) {
            toEdit.locallab.spots.at(i).linear = mods.locallab.spots.at(i).linear;
        }

        if (locallab.spots.at(i).gamm) {
            toEdit.locallab.spots.at(i).gamm = mods.locallab.spots.at(i).gamm;
        }

        if (locallab.spots.at(i).fatamount) {
            toEdit.locallab.spots.at(i).fatamount = mods.locallab.spots.at(i).fatamount;
        }

        if (locallab.spots.at(i).fatdetail) {
            toEdit.locallab.spots.at(i).fatdetail = mods.locallab.spots.at(i).fatdetail;
        }

        if (locallab.spots.at(i).fatsatur) {
            toEdit.locallab.spots.at(i).fatsatur = mods.locallab.spots.at(i).fatsatur;
        }

        if (locallab.spots.at(i).fatanchor) {
            toEdit.locallab.spots.at(i).fatanchor = mods.locallab.spots.at(i).fatanchor;
        }

        if (locallab.spots.at(i).fatlevel) {
            toEdit.locallab.spots.at(i).fatlevel = mods.locallab.spots.at(i).fatlevel;
        }

        if (locallab.spots.at(i).recothrese) {
            toEdit.locallab.spots.at(i).recothrese = mods.locallab.spots.at(i).recothrese;
        }

        if (locallab.spots.at(i).lowthrese) {
            toEdit.locallab.spots.at(i).lowthrese = mods.locallab.spots.at(i).lowthrese;
        }

        if (locallab.spots.at(i).higthrese) {
            toEdit.locallab.spots.at(i).higthrese = mods.locallab.spots.at(i).higthrese;
        }

        if (locallab.spots.at(i).decaye) {
            toEdit.locallab.spots.at(i).decaye = mods.locallab.spots.at(i).decaye;
        }

        // Shadow highlight
        if (locallab.spots.at(i).visishadhigh) {
            toEdit.locallab.spots.at(i).visishadhigh = mods.locallab.spots.at(i).visishadhigh;
        }

        if (locallab.spots.at(i).expshadhigh) {
            toEdit.locallab.spots.at(i).expshadhigh = mods.locallab.spots.at(i).expshadhigh;
        }

        if (locallab.spots.at(i).complexshadhigh) {
            toEdit.locallab.spots.at(i).complexshadhigh = mods.locallab.spots.at(i).complexshadhigh;
        }

        if (locallab.spots.at(i).shMethod) {
            toEdit.locallab.spots.at(i).shMethod = mods.locallab.spots.at(i).shMethod;
        }

        if (locallab.spots.at(i).ghsMethod) {
            toEdit.locallab.spots.at(i).ghsMethod = mods.locallab.spots.at(i).ghsMethod;
        }

        if (locallab.spots.at(i).ghsMatmet) {
            toEdit.locallab.spots.at(i).ghsMatmet = mods.locallab.spots.at(i).ghsMatmet;
        }

        if (locallab.spots.at(i).ghsMode) {
            toEdit.locallab.spots.at(i).ghsMode = mods.locallab.spots.at(i).ghsMode;
        }

        if (locallab.spots.at(i).ghs_D) {
            toEdit.locallab.spots.at(i).ghs_D = mods.locallab.spots.at(i).ghs_D;
        }

        if (locallab.spots.at(i).ghs_slope) {
            toEdit.locallab.spots.at(i).ghs_slope = mods.locallab.spots.at(i).ghs_slope;
        }

        if (locallab.spots.at(i).ghs_chro) {
            toEdit.locallab.spots.at(i).ghs_chro = mods.locallab.spots.at(i).ghs_chro;
        }

        if (locallab.spots.at(i).ghs_B) {
            toEdit.locallab.spots.at(i).ghs_B = mods.locallab.spots.at(i).ghs_B;
        }

        if (locallab.spots.at(i).ghs_SP) {
            toEdit.locallab.spots.at(i).ghs_SP = mods.locallab.spots.at(i).ghs_SP;
        }

        if (locallab.spots.at(i).SPAutoRadius) {
            toEdit.locallab.spots.at(i).SPAutoRadius = mods.locallab.spots.at(i).SPAutoRadius;
        }

        if (locallab.spots.at(i).ghs_LP) {
            toEdit.locallab.spots.at(i).ghs_LP = mods.locallab.spots.at(i).ghs_LP;
        }

        if (locallab.spots.at(i).ghs_HP) {
            toEdit.locallab.spots.at(i).ghs_HP = mods.locallab.spots.at(i).ghs_HP;
        }

        if (locallab.spots.at(i).ghs_LC) {
            toEdit.locallab.spots.at(i).ghs_LC = mods.locallab.spots.at(i).ghs_LC;
        }

        if (locallab.spots.at(i).ghs_MID) {
            toEdit.locallab.spots.at(i).ghs_MID = mods.locallab.spots.at(i).ghs_MID;
        }

        if (locallab.spots.at(i).ghs_BLP) {
            toEdit.locallab.spots.at(i).ghs_BLP = mods.locallab.spots.at(i).ghs_BLP;
        }

        if (locallab.spots.at(i).ghs_HLP) {
            toEdit.locallab.spots.at(i).ghs_HLP = mods.locallab.spots.at(i).ghs_HLP;
        }

        if (locallab.spots.at(i).ghs_autobw) {
            toEdit.locallab.spots.at(i).ghs_autobw = mods.locallab.spots.at(i).ghs_autobw;
        }

        if (locallab.spots.at(i).ghs_agx) {
            toEdit.locallab.spots.at(i).ghs_agx = mods.locallab.spots.at(i).ghs_agx;
        }

        if (locallab.spots.at(i).ghs_smooth) {
            toEdit.locallab.spots.at(i).ghs_smooth = mods.locallab.spots.at(i).ghs_smooth;
        }

        if (locallab.spots.at(i).ghs_inv) {
            toEdit.locallab.spots.at(i).ghs_inv = mods.locallab.spots.at(i).ghs_inv;
        }

        if (locallab.spots.at(i).mich_exp) {
            toEdit.locallab.spots.at(i).mich_exp = mods.locallab.spots.at(i).mich_exp;
        }

        if (locallab.spots.at(i).mich_spar) {
            toEdit.locallab.spots.at(i).mich_spar = mods.locallab.spots.at(i).mich_spar;
        }

        if (locallab.spots.at(i).mich_kpar) {
            toEdit.locallab.spots.at(i).mich_kpar = mods.locallab.spots.at(i).mich_kpar;
        }

        if (locallab.spots.at(i).mich_sat) {
            toEdit.locallab.spots.at(i).mich_sat = mods.locallab.spots.at(i).mich_sat;
        }

        if (locallab.spots.at(i).mich_out) {
            toEdit.locallab.spots.at(i).mich_out = mods.locallab.spots.at(i).mich_out;
        }

        if (locallab.spots.at(i).mich_high) {
            toEdit.locallab.spots.at(i).mich_high = mods.locallab.spots.at(i).mich_high;
        }

        if (locallab.spots.at(i).mich_black) {
            toEdit.locallab.spots.at(i).mich_black = mods.locallab.spots.at(i).mich_black;
        }

        if (locallab.spots.at(i).mich_white) {
            toEdit.locallab.spots.at(i).mich_white = mods.locallab.spots.at(i).mich_white;
        }

        if (locallab.spots.at(i).mich_jdx) {
            toEdit.locallab.spots.at(i).mich_jdx = mods.locallab.spots.at(i).mich_jdx;
        }

        for (int j = 0; j < 6; j++) {
            if (locallab.spots.at(i).multsh[j]) {
                toEdit.locallab.spots.at(i).multsh[j] = mods.locallab.spots.at(i).multsh[j];
            }
        }

        if (locallab.spots.at(i).highlights) {
            toEdit.locallab.spots.at(i).highlights = mods.locallab.spots.at(i).highlights;
        }

        if (locallab.spots.at(i).h_tonalwidth) {
            toEdit.locallab.spots.at(i).h_tonalwidth = mods.locallab.spots.at(i).h_tonalwidth;
        }

        if (locallab.spots.at(i).shadows) {
            toEdit.locallab.spots.at(i).shadows = mods.locallab.spots.at(i).shadows;
        }

        if (locallab.spots.at(i).s_tonalwidth) {
            toEdit.locallab.spots.at(i).s_tonalwidth = mods.locallab.spots.at(i).s_tonalwidth;
        }

        if (locallab.spots.at(i).sh_radius) {
            toEdit.locallab.spots.at(i).sh_radius = mods.locallab.spots.at(i).sh_radius;
        }

        if (locallab.spots.at(i).sensihs) {
            toEdit.locallab.spots.at(i).sensihs = mods.locallab.spots.at(i).sensihs;
        }

        if (locallab.spots.at(i).enaSHMask) {
            toEdit.locallab.spots.at(i).enaSHMask = mods.locallab.spots.at(i).enaSHMask;
        }

        if (locallab.spots.at(i).CCmaskSHcurve) {
            toEdit.locallab.spots.at(i).CCmaskSHcurve = mods.locallab.spots.at(i).CCmaskSHcurve;
        }

        if (locallab.spots.at(i).LLmaskSHcurve) {
            toEdit.locallab.spots.at(i).LLmaskSHcurve = mods.locallab.spots.at(i).LLmaskSHcurve;
        }

        if (locallab.spots.at(i).HHmaskSHcurve) {
            toEdit.locallab.spots.at(i).HHmaskSHcurve = mods.locallab.spots.at(i).HHmaskSHcurve;
        }

        if (locallab.spots.at(i).blendmaskSH) {
            toEdit.locallab.spots.at(i).blendmaskSH = mods.locallab.spots.at(i).blendmaskSH;
        }

        if (locallab.spots.at(i).radmaskSH) {
            toEdit.locallab.spots.at(i).radmaskSH = mods.locallab.spots.at(i).radmaskSH;
        }

        if (locallab.spots.at(i).blurSHde) {
            toEdit.locallab.spots.at(i).blurSHde = mods.locallab.spots.at(i).blurSHde;
        }

        if (locallab.spots.at(i).strSH) {
            toEdit.locallab.spots.at(i).strSH = mods.locallab.spots.at(i).strSH;
        }

        if (locallab.spots.at(i).angSH) {
            toEdit.locallab.spots.at(i).angSH = mods.locallab.spots.at(i).angSH;
        }

        if (locallab.spots.at(i).featherSH) {
            toEdit.locallab.spots.at(i).featherSH = mods.locallab.spots.at(i).featherSH;
        }

        if (locallab.spots.at(i).inverssh) {
            toEdit.locallab.spots.at(i).inverssh = mods.locallab.spots.at(i).inverssh;
        }

        if (locallab.spots.at(i).chromaskSH) {
            toEdit.locallab.spots.at(i).chromaskSH = mods.locallab.spots.at(i).chromaskSH;
        }

        if (locallab.spots.at(i).gammaskSH) {
            toEdit.locallab.spots.at(i).gammaskSH = mods.locallab.spots.at(i).gammaskSH;
        }

        if (locallab.spots.at(i).slomaskSH) {
            toEdit.locallab.spots.at(i).slomaskSH = mods.locallab.spots.at(i).slomaskSH;
        }

        if (locallab.spots.at(i).lapmaskSH) {
            toEdit.locallab.spots.at(i).lapmaskSH = mods.locallab.spots.at(i).lapmaskSH;
        }

        if (locallab.spots.at(i).detailSH) {
            toEdit.locallab.spots.at(i).detailSH = mods.locallab.spots.at(i).detailSH;
        }

        if (locallab.spots.at(i).tePivot) {
            toEdit.locallab.spots.at(i).tePivot = mods.locallab.spots.at(i).tePivot;
        }

        if (locallab.spots.at(i).reparsh) {
            toEdit.locallab.spots.at(i).reparsh = mods.locallab.spots.at(i).reparsh;
        }

        if (locallab.spots.at(i).LmaskSHcurve) {
            toEdit.locallab.spots.at(i).LmaskSHcurve = mods.locallab.spots.at(i).LmaskSHcurve;
        }

        if (locallab.spots.at(i).fatamountSH) {
            toEdit.locallab.spots.at(i).fatamountSH = mods.locallab.spots.at(i).fatamountSH;
        }

        if (locallab.spots.at(i).fatanchorSH) {
            toEdit.locallab.spots.at(i).fatanchorSH = mods.locallab.spots.at(i).fatanchorSH;
        }

        if (locallab.spots.at(i).gamSH) {
            toEdit.locallab.spots.at(i).gamSH = mods.locallab.spots.at(i).gamSH;
        }

        if (locallab.spots.at(i).sloSH) {
            toEdit.locallab.spots.at(i).sloSH = mods.locallab.spots.at(i).sloSH;
        }

        if (locallab.spots.at(i).recothress) {
            toEdit.locallab.spots.at(i).recothress = mods.locallab.spots.at(i).recothress;
        }

        if (locallab.spots.at(i).lowthress) {
            toEdit.locallab.spots.at(i).lowthress = mods.locallab.spots.at(i).lowthress;
        }

        if (locallab.spots.at(i).higthress) {
            toEdit.locallab.spots.at(i).higthress = mods.locallab.spots.at(i).higthress;
        }

        if (locallab.spots.at(i).decays) {
            toEdit.locallab.spots.at(i).decays = mods.locallab.spots.at(i).decays;
        }

        // Vibrance
        if (locallab.spots.at(i).visivibrance) {
            toEdit.locallab.spots.at(i).visivibrance = mods.locallab.spots.at(i).visivibrance;
        }

        if (locallab.spots.at(i).expvibrance) {
            toEdit.locallab.spots.at(i).expvibrance = mods.locallab.spots.at(i).expvibrance;
        }

        if (locallab.spots.at(i).complexvibrance) {
            toEdit.locallab.spots.at(i).complexvibrance = mods.locallab.spots.at(i).complexvibrance;
        }

        if (locallab.spots.at(i).saturated) {
            toEdit.locallab.spots.at(i).saturated = mods.locallab.spots.at(i).saturated;
        }

        if (locallab.spots.at(i).pastels) {
            toEdit.locallab.spots.at(i).pastels = mods.locallab.spots.at(i).pastels;
        }

        if (locallab.spots.at(i).vibgam) {
            toEdit.locallab.spots.at(i).vibgam = mods.locallab.spots.at(i).vibgam;
        }

        if (locallab.spots.at(i).warm) {
            toEdit.locallab.spots.at(i).warm = mods.locallab.spots.at(i).warm;
        }

        if (locallab.spots.at(i).psthreshold) {
            toEdit.locallab.spots.at(i).psthreshold = mods.locallab.spots.at(i).psthreshold;
        }

        if (locallab.spots.at(i).protectskins) {
            toEdit.locallab.spots.at(i).protectskins = mods.locallab.spots.at(i).protectskins;
        }

        if (locallab.spots.at(i).avoidcolorshift) {
            toEdit.locallab.spots.at(i).avoidcolorshift = mods.locallab.spots.at(i).avoidcolorshift;
        }

        if (locallab.spots.at(i).pastsattog) {
            toEdit.locallab.spots.at(i).pastsattog = mods.locallab.spots.at(i).pastsattog;
        }

        if (locallab.spots.at(i).sensiv) {
            toEdit.locallab.spots.at(i).sensiv = mods.locallab.spots.at(i).sensiv;
        }

        if (locallab.spots.at(i).skintonescurve) {
            toEdit.locallab.spots.at(i).skintonescurve = mods.locallab.spots.at(i).skintonescurve;
        }

        if (locallab.spots.at(i).CCmaskvibcurve) {
            toEdit.locallab.spots.at(i).CCmaskvibcurve = mods.locallab.spots.at(i).CCmaskvibcurve;
        }

        if (locallab.spots.at(i).LLmaskvibcurve) {
            toEdit.locallab.spots.at(i).LLmaskvibcurve = mods.locallab.spots.at(i).LLmaskvibcurve;
        }

        if (locallab.spots.at(i).HHmaskvibcurve) {
            toEdit.locallab.spots.at(i).HHmaskvibcurve = mods.locallab.spots.at(i).HHmaskvibcurve;
        }

        if (locallab.spots.at(i).enavibMask) {
            toEdit.locallab.spots.at(i).enavibMask = mods.locallab.spots.at(i).enavibMask;
        }

        if (locallab.spots.at(i).blendmaskvib) {
            toEdit.locallab.spots.at(i).blendmaskvib = mods.locallab.spots.at(i).blendmaskvib;
        }

        if (locallab.spots.at(i).radmaskvib) {
            toEdit.locallab.spots.at(i).radmaskvib = mods.locallab.spots.at(i).radmaskvib;
        }

        if (locallab.spots.at(i).chromaskvib) {
            toEdit.locallab.spots.at(i).chromaskvib = mods.locallab.spots.at(i).chromaskvib;
        }

        if (locallab.spots.at(i).gammaskvib) {
            toEdit.locallab.spots.at(i).gammaskvib = mods.locallab.spots.at(i).gammaskvib;
        }

        if (locallab.spots.at(i).slomaskvib) {
            toEdit.locallab.spots.at(i).slomaskvib = mods.locallab.spots.at(i).slomaskvib;
        }

        if (locallab.spots.at(i).lapmaskvib) {
            toEdit.locallab.spots.at(i).lapmaskvib = mods.locallab.spots.at(i).lapmaskvib;
        }

        if (locallab.spots.at(i).strvib) {
            toEdit.locallab.spots.at(i).strvib = mods.locallab.spots.at(i).strvib;
        }

        if (locallab.spots.at(i).strvibab) {
            toEdit.locallab.spots.at(i).strvibab = mods.locallab.spots.at(i).strvibab;
        }

        if (locallab.spots.at(i).strvibh) {
            toEdit.locallab.spots.at(i).strvibh = mods.locallab.spots.at(i).strvibh;
        }

        if (locallab.spots.at(i).angvib) {
            toEdit.locallab.spots.at(i).angvib = mods.locallab.spots.at(i).angvib;
        }

        if (locallab.spots.at(i).feathervib) {
            toEdit.locallab.spots.at(i).feathervib = mods.locallab.spots.at(i).feathervib;
        }

        if (locallab.spots.at(i).Lmaskvibcurve) {
            toEdit.locallab.spots.at(i).Lmaskvibcurve = mods.locallab.spots.at(i).Lmaskvibcurve;
        }

        if (locallab.spots.at(i).recothresv) {
            toEdit.locallab.spots.at(i).recothresv = mods.locallab.spots.at(i).recothresv;
        }

        if (locallab.spots.at(i).lowthresv) {
            toEdit.locallab.spots.at(i).lowthresv = mods.locallab.spots.at(i).lowthresv;
        }

        if (locallab.spots.at(i).higthresv) {
            toEdit.locallab.spots.at(i).higthresv = mods.locallab.spots.at(i).higthresv;
        }

        if (locallab.spots.at(i).decayv) {
            toEdit.locallab.spots.at(i).decayv = mods.locallab.spots.at(i).decayv;
        }

        // Soft Light
        if (locallab.spots.at(i).visisoft) {
            toEdit.locallab.spots.at(i).visisoft = mods.locallab.spots.at(i).visisoft;
        }

        if (locallab.spots.at(i).expsoft) {
            toEdit.locallab.spots.at(i).expsoft = mods.locallab.spots.at(i).expsoft;
        }

        if (locallab.spots.at(i).complexsoft) {
            toEdit.locallab.spots.at(i).complexsoft = mods.locallab.spots.at(i).complexsoft;
        }

        if (locallab.spots.at(i).streng) {
            toEdit.locallab.spots.at(i).streng = mods.locallab.spots.at(i).streng;
        }

        if (locallab.spots.at(i).sensisf) {
            toEdit.locallab.spots.at(i).sensisf = mods.locallab.spots.at(i).sensisf;
        }

        if (locallab.spots.at(i).laplace) {
            toEdit.locallab.spots.at(i).laplace = mods.locallab.spots.at(i).laplace;
        }

        if (locallab.spots.at(i).softMethod) {
            toEdit.locallab.spots.at(i).softMethod = mods.locallab.spots.at(i).softMethod;
        }

        // Blur & Noise
        if (locallab.spots.at(i).visiblur) {
            toEdit.locallab.spots.at(i).visiblur = mods.locallab.spots.at(i).visiblur;
        }

        if (locallab.spots.at(i).expblur) {
            toEdit.locallab.spots.at(i).expblur = mods.locallab.spots.at(i).expblur;
        }

        if (locallab.spots.at(i).complexblur) {
            toEdit.locallab.spots.at(i).complexblur = mods.locallab.spots.at(i).complexblur;
        }

        if (locallab.spots.at(i).radius) {
            toEdit.locallab.spots.at(i).radius = mods.locallab.spots.at(i).radius;
        }

        if (locallab.spots.at(i).strength) {
            toEdit.locallab.spots.at(i).strength = mods.locallab.spots.at(i).strength;
        }

        if (locallab.spots.at(i).sensibn) {
            toEdit.locallab.spots.at(i).sensibn = mods.locallab.spots.at(i).sensibn;
        }

        if (locallab.spots.at(i).itera) {
            toEdit.locallab.spots.at(i).itera = mods.locallab.spots.at(i).itera;
        }

        if (locallab.spots.at(i).guidbl) {
            toEdit.locallab.spots.at(i).guidbl = mods.locallab.spots.at(i).guidbl;
        }

        if (locallab.spots.at(i).strbl) {
            toEdit.locallab.spots.at(i).strbl = mods.locallab.spots.at(i).strbl;
        }

        if (locallab.spots.at(i).recothres) {
            toEdit.locallab.spots.at(i).recothres = mods.locallab.spots.at(i).recothres;
        }

        if (locallab.spots.at(i).lowthres) {
            toEdit.locallab.spots.at(i).lowthres = mods.locallab.spots.at(i).lowthres;
        }

        if (locallab.spots.at(i).higthres) {
            toEdit.locallab.spots.at(i).higthres = mods.locallab.spots.at(i).higthres;
        }

        if (locallab.spots.at(i).recothresd) {
            toEdit.locallab.spots.at(i).recothresd = mods.locallab.spots.at(i).recothresd;
        }

        if (locallab.spots.at(i).lowthresd) {
            toEdit.locallab.spots.at(i).lowthresd = mods.locallab.spots.at(i).lowthresd;
        }

        if (locallab.spots.at(i).midthresd) {
            toEdit.locallab.spots.at(i).midthresd = mods.locallab.spots.at(i).midthresd;
        }

        if (locallab.spots.at(i).midthresdch) {
            toEdit.locallab.spots.at(i).midthresdch = mods.locallab.spots.at(i).midthresdch;
        }

        if (locallab.spots.at(i).higthresd) {
            toEdit.locallab.spots.at(i).higthresd = mods.locallab.spots.at(i).higthresd;
        }

        if (locallab.spots.at(i).decayd) {
            toEdit.locallab.spots.at(i).decayd = mods.locallab.spots.at(i).decayd;
        }

        if (locallab.spots.at(i).isogr) {
            toEdit.locallab.spots.at(i).isogr = mods.locallab.spots.at(i).isogr;
        }

        if (locallab.spots.at(i).strengr) {
            toEdit.locallab.spots.at(i).strengr = mods.locallab.spots.at(i).strengr;
        }

        if (locallab.spots.at(i).scalegr) {
            toEdit.locallab.spots.at(i).scalegr = mods.locallab.spots.at(i).scalegr;
        }

        if (locallab.spots.at(i).divgr) {
            toEdit.locallab.spots.at(i).divgr = mods.locallab.spots.at(i).divgr;
        }

        if (locallab.spots.at(i).epsbl) {
            toEdit.locallab.spots.at(i).epsbl = mods.locallab.spots.at(i).epsbl;
        }

        if (locallab.spots.at(i).blMethod) {
            toEdit.locallab.spots.at(i).blMethod = mods.locallab.spots.at(i).blMethod;
        }

        if (locallab.spots.at(i).chroMethod) {
            toEdit.locallab.spots.at(i).chroMethod = mods.locallab.spots.at(i).chroMethod;
        }

        if (locallab.spots.at(i).quamethod) {
            toEdit.locallab.spots.at(i).quamethod = mods.locallab.spots.at(i).quamethod;
        }

        if (locallab.spots.at(i).blurMethod) {
            toEdit.locallab.spots.at(i).blurMethod = mods.locallab.spots.at(i).blurMethod;
        }

        if (locallab.spots.at(i).usemask) {
            toEdit.locallab.spots.at(i).usemask = mods.locallab.spots.at(i).usemask;
        }

        if (locallab.spots.at(i).invmaskd) {
            toEdit.locallab.spots.at(i).invmaskd = mods.locallab.spots.at(i).invmaskd;
        }

        if (locallab.spots.at(i).invmask) {
            toEdit.locallab.spots.at(i).invmask = mods.locallab.spots.at(i).invmask;
        }

        if (locallab.spots.at(i).levelthr) {
            toEdit.locallab.spots.at(i).levelthr = mods.locallab.spots.at(i).levelthr;
        }

        if (locallab.spots.at(i).lnoiselow) {
            toEdit.locallab.spots.at(i).lnoiselow = mods.locallab.spots.at(i).lnoiselow;
        }

        if (locallab.spots.at(i).levelthrlow) {
            toEdit.locallab.spots.at(i).levelthrlow = mods.locallab.spots.at(i).levelthrlow;
        }

        if (locallab.spots.at(i).medMethod) {
            toEdit.locallab.spots.at(i).medMethod = mods.locallab.spots.at(i).medMethod;
        }

        if (locallab.spots.at(i).activlum) {
            toEdit.locallab.spots.at(i).activlum = mods.locallab.spots.at(i).activlum;
        }
        
        for (int j = 0; j < 21; j++) {
            if (locallab.spots.at(i).madlsav[j]) {
                toEdit.locallab.spots.at(i).madlsav[j] = mods.locallab.spots.at(i).madlsav[j];
            }
        }

        if (locallab.spots.at(i).noiselumf) {
            toEdit.locallab.spots.at(i).noiselumf = mods.locallab.spots.at(i).noiselumf;
        }

        if (locallab.spots.at(i).noiselumf0) {
            toEdit.locallab.spots.at(i).noiselumf0 = mods.locallab.spots.at(i).noiselumf0;
        }

        if (locallab.spots.at(i).noiselumf2) {
            toEdit.locallab.spots.at(i).noiselumf2 = mods.locallab.spots.at(i).noiselumf2;
        }

        if (locallab.spots.at(i).noiselumc) {
            toEdit.locallab.spots.at(i).noiselumc = mods.locallab.spots.at(i).noiselumc;
        }

        if (locallab.spots.at(i).noiselumdetail) {
            toEdit.locallab.spots.at(i).noiselumdetail = mods.locallab.spots.at(i).noiselumdetail;
        }

        if (locallab.spots.at(i).noiselequal) {
            toEdit.locallab.spots.at(i).noiselequal = mods.locallab.spots.at(i).noiselequal;
        }

        if (locallab.spots.at(i).noisegam) {
            toEdit.locallab.spots.at(i).noisegam = mods.locallab.spots.at(i).noisegam;
        }

        if (locallab.spots.at(i).noisechrof) {
            toEdit.locallab.spots.at(i).noisechrof = mods.locallab.spots.at(i).noisechrof;
        }

        if (locallab.spots.at(i).noisechroc) {
            toEdit.locallab.spots.at(i).noisechroc = mods.locallab.spots.at(i).noisechroc;
        }

        if (locallab.spots.at(i).noisechrodetail) {
            toEdit.locallab.spots.at(i).noisechrodetail = mods.locallab.spots.at(i).noisechrodetail;
        }

        if (locallab.spots.at(i).adjblur) {
            toEdit.locallab.spots.at(i).adjblur = mods.locallab.spots.at(i).adjblur;
        }

        if (locallab.spots.at(i).bilateral) {
            toEdit.locallab.spots.at(i).bilateral = mods.locallab.spots.at(i).bilateral;
        }

        if (locallab.spots.at(i).nlstr) {
            toEdit.locallab.spots.at(i).nlstr = mods.locallab.spots.at(i).nlstr;
        }

        if (locallab.spots.at(i).nldet) {
            toEdit.locallab.spots.at(i).nldet = mods.locallab.spots.at(i).nldet;
        }

        if (locallab.spots.at(i).nlpat) {
            toEdit.locallab.spots.at(i).nlpat = mods.locallab.spots.at(i).nlpat;
        }

        if (locallab.spots.at(i).nlrad) {
            toEdit.locallab.spots.at(i).nlrad = mods.locallab.spots.at(i).nlrad;
        }

        if (locallab.spots.at(i).nlgam) {
            toEdit.locallab.spots.at(i).nlgam = mods.locallab.spots.at(i).nlgam;
        }

        if (locallab.spots.at(i).nliter) {
            toEdit.locallab.spots.at(i).nliter = mods.locallab.spots.at(i).nliter;
        }

        if (locallab.spots.at(i).sensiden) {
            toEdit.locallab.spots.at(i).sensiden = mods.locallab.spots.at(i).sensiden;
        }

        if (locallab.spots.at(i).reparden) {
            toEdit.locallab.spots.at(i).reparden = mods.locallab.spots.at(i).reparden;
        }

        if (locallab.spots.at(i).detailthr) {
            toEdit.locallab.spots.at(i).detailthr = mods.locallab.spots.at(i).detailthr;
        }

        if (locallab.spots.at(i).locwavcurveden) {
            toEdit.locallab.spots.at(i).locwavcurveden = mods.locallab.spots.at(i).locwavcurveden;
        }

        if (locallab.spots.at(i).locwavcurvehue) {
            toEdit.locallab.spots.at(i).locwavcurvehue = mods.locallab.spots.at(i).locwavcurvehue;
        }

        if (locallab.spots.at(i).locwavcurvehuecont) {
            toEdit.locallab.spots.at(i).locwavcurvehuecont = mods.locallab.spots.at(i).locwavcurvehuecont;
        }

        if (locallab.spots.at(i).showmaskblMethodtyp) {
            toEdit.locallab.spots.at(i).showmaskblMethodtyp = mods.locallab.spots.at(i).showmaskblMethodtyp;
        }

        if (locallab.spots.at(i).CCmaskblcurve) {
            toEdit.locallab.spots.at(i).CCmaskblcurve = mods.locallab.spots.at(i).CCmaskblcurve;
        }

        if (locallab.spots.at(i).LLmaskblcurve) {
            toEdit.locallab.spots.at(i).LLmaskblcurve = mods.locallab.spots.at(i).LLmaskblcurve;
        }

        if (locallab.spots.at(i).HHmaskblcurve) {
            toEdit.locallab.spots.at(i).HHmaskblcurve = mods.locallab.spots.at(i).HHmaskblcurve;
        }

        if (locallab.spots.at(i).enablMask) {
            toEdit.locallab.spots.at(i).enablMask = mods.locallab.spots.at(i).enablMask;
        }

        if (locallab.spots.at(i).fftwbl) {
            toEdit.locallab.spots.at(i).fftwbl = mods.locallab.spots.at(i).fftwbl;
        }

        if (locallab.spots.at(i).invbl) {
            toEdit.locallab.spots.at(i).invbl = mods.locallab.spots.at(i).invbl;
        }

        if (locallab.spots.at(i).toolbl) {
            toEdit.locallab.spots.at(i).toolbl = mods.locallab.spots.at(i).toolbl;
        }

        if (locallab.spots.at(i).blendmaskbl) {
            toEdit.locallab.spots.at(i).blendmaskbl = mods.locallab.spots.at(i).blendmaskbl;
        }

        if (locallab.spots.at(i).radmaskbl) {
            toEdit.locallab.spots.at(i).radmaskbl = mods.locallab.spots.at(i).radmaskbl;
        }

        if (locallab.spots.at(i).chromaskbl) {
            toEdit.locallab.spots.at(i).chromaskbl = mods.locallab.spots.at(i).chromaskbl;
        }

        if (locallab.spots.at(i).gammaskbl) {
            toEdit.locallab.spots.at(i).gammaskbl = mods.locallab.spots.at(i).gammaskbl;
        }

        if (locallab.spots.at(i).slomaskbl) {
            toEdit.locallab.spots.at(i).slomaskbl = mods.locallab.spots.at(i).slomaskbl;
        }

        if (locallab.spots.at(i).lapmaskbl) {
            toEdit.locallab.spots.at(i).lapmaskbl = mods.locallab.spots.at(i).lapmaskbl;
        }

        if (locallab.spots.at(i).shadmaskbl) {
            toEdit.locallab.spots.at(i).shadmaskbl = mods.locallab.spots.at(i).shadmaskbl;
        }

        if (locallab.spots.at(i).shadmaskblsha) {
            toEdit.locallab.spots.at(i).shadmaskblsha = mods.locallab.spots.at(i).shadmaskblsha;
        }

        if (locallab.spots.at(i).strumaskbl) {
            toEdit.locallab.spots.at(i).strumaskbl = mods.locallab.spots.at(i).strumaskbl;
        }

        if (locallab.spots.at(i).Lmaskblcurve) {
            toEdit.locallab.spots.at(i).Lmaskblcurve = mods.locallab.spots.at(i).Lmaskblcurve;
        }

        if (locallab.spots.at(i).LLmaskblcurvewav) {
            toEdit.locallab.spots.at(i).LLmaskblcurvewav = mods.locallab.spots.at(i).LLmaskblcurvewav;
        }


        if (locallab.spots.at(i).denocontrast) {
            toEdit.locallab.spots.at(i).denocontrast = mods.locallab.spots.at(i).denocontrast;
        }

        if (locallab.spots.at(i).denoAutocontrast) {
            toEdit.locallab.spots.at(i).denoAutocontrast = mods.locallab.spots.at(i).denoAutocontrast;
        }

        if (locallab.spots.at(i).contrshow) {
            toEdit.locallab.spots.at(i).contrshow = mods.locallab.spots.at(i).contrshow;
        }

        if (locallab.spots.at(i).lockmadl) {
            toEdit.locallab.spots.at(i).lockmadl = mods.locallab.spots.at(i).lockmadl;
        }

        if (locallab.spots.at(i).madllock) {
            toEdit.locallab.spots.at(i).madllock = mods.locallab.spots.at(i).madllock;
        }

        if (locallab.spots.at(i).enacontrast) {
            toEdit.locallab.spots.at(i).enacontrast = mods.locallab.spots.at(i).enacontrast;
        }

        if (locallab.spots.at(i).denoratio) {
            toEdit.locallab.spots.at(i).denoratio = mods.locallab.spots.at(i).denoratio;
        }

        if (locallab.spots.at(i).denomask) {
            toEdit.locallab.spots.at(i).denomask = mods.locallab.spots.at(i).denomask;
        }

        // Tone Mapping
        if (locallab.spots.at(i).visitonemap) {
            toEdit.locallab.spots.at(i).visitonemap = mods.locallab.spots.at(i).visitonemap;
        }

        if (locallab.spots.at(i).exptonemap) {
            toEdit.locallab.spots.at(i).exptonemap = mods.locallab.spots.at(i).exptonemap;
        }

        if (locallab.spots.at(i).complextonemap) {
            toEdit.locallab.spots.at(i).complextonemap = mods.locallab.spots.at(i).complextonemap;
        }

        if (locallab.spots.at(i).stren) {
            toEdit.locallab.spots.at(i).stren = mods.locallab.spots.at(i).stren;
        }

        if (locallab.spots.at(i).gamma) {
            toEdit.locallab.spots.at(i).gamma = mods.locallab.spots.at(i).gamma;
        }

        if (locallab.spots.at(i).estop) {
            toEdit.locallab.spots.at(i).estop = mods.locallab.spots.at(i).estop;
        }

        if (locallab.spots.at(i).scaltm) {
            toEdit.locallab.spots.at(i).scaltm = mods.locallab.spots.at(i).scaltm;
        }

        if (locallab.spots.at(i).repartm) {
            toEdit.locallab.spots.at(i).repartm = mods.locallab.spots.at(i).repartm;
        }

        if (locallab.spots.at(i).rewei) {
            toEdit.locallab.spots.at(i).rewei = mods.locallab.spots.at(i).rewei;
        }

        if (locallab.spots.at(i).satur) {
            toEdit.locallab.spots.at(i).satur = mods.locallab.spots.at(i).satur;
        }

        if (locallab.spots.at(i).sensitm) {
            toEdit.locallab.spots.at(i).sensitm = mods.locallab.spots.at(i).sensitm;
        }

        if (locallab.spots.at(i).softradiustm) {
            toEdit.locallab.spots.at(i).softradiustm = mods.locallab.spots.at(i).softradiustm;
        }

        if (locallab.spots.at(i).amount) {
            toEdit.locallab.spots.at(i).amount = mods.locallab.spots.at(i).amount;
        }

        if (locallab.spots.at(i).equiltm) {
            toEdit.locallab.spots.at(i).equiltm = mods.locallab.spots.at(i).equiltm;
        }

        if (locallab.spots.at(i).CCmasktmcurve) {
            toEdit.locallab.spots.at(i).CCmasktmcurve = mods.locallab.spots.at(i).CCmasktmcurve;
        }

        if (locallab.spots.at(i).LLmasktmcurve) {
            toEdit.locallab.spots.at(i).LLmasktmcurve = mods.locallab.spots.at(i).LLmasktmcurve;
        }

        if (locallab.spots.at(i).HHmasktmcurve) {
            toEdit.locallab.spots.at(i).HHmasktmcurve = mods.locallab.spots.at(i).HHmasktmcurve;
        }

        if (locallab.spots.at(i).enatmMask) {
            toEdit.locallab.spots.at(i).enatmMask = mods.locallab.spots.at(i).enatmMask;
        }

        if (locallab.spots.at(i).enatmMaskaft) {
            toEdit.locallab.spots.at(i).enatmMaskaft = mods.locallab.spots.at(i).enatmMaskaft;
        }

        if (locallab.spots.at(i).blendmasktm) {
            toEdit.locallab.spots.at(i).blendmasktm = mods.locallab.spots.at(i).blendmasktm;
        }

        if (locallab.spots.at(i).radmasktm) {
            toEdit.locallab.spots.at(i).radmasktm = mods.locallab.spots.at(i).radmasktm;
        }

        if (locallab.spots.at(i).chromasktm) {
            toEdit.locallab.spots.at(i).chromasktm = mods.locallab.spots.at(i).chromasktm;
        }

        if (locallab.spots.at(i).gammasktm) {
            toEdit.locallab.spots.at(i).gammasktm = mods.locallab.spots.at(i).gammasktm;
        }

        if (locallab.spots.at(i).slomasktm) {
            toEdit.locallab.spots.at(i).slomasktm = mods.locallab.spots.at(i).slomasktm;
        }

        if (locallab.spots.at(i).lapmasktm) {
            toEdit.locallab.spots.at(i).lapmasktm = mods.locallab.spots.at(i).lapmasktm;
        }

        if (locallab.spots.at(i).Lmasktmcurve) {
            toEdit.locallab.spots.at(i).Lmasktmcurve = mods.locallab.spots.at(i).Lmasktmcurve;
        }

        if (locallab.spots.at(i).recothrest) {
            toEdit.locallab.spots.at(i).recothrest = mods.locallab.spots.at(i).recothrest;
        }

        if (locallab.spots.at(i).lowthrest) {
            toEdit.locallab.spots.at(i).lowthrest = mods.locallab.spots.at(i).lowthrest;
        }

        if (locallab.spots.at(i).higthrest) {
            toEdit.locallab.spots.at(i).higthrest = mods.locallab.spots.at(i).higthrest;
        }

        if (locallab.spots.at(i).decayt) {
            toEdit.locallab.spots.at(i).decayt = mods.locallab.spots.at(i).decayt;
        }

        // Retinex
        if (locallab.spots.at(i).visireti) {
            toEdit.locallab.spots.at(i).visireti = mods.locallab.spots.at(i).visireti;
        }

        if (locallab.spots.at(i).expreti) {
            toEdit.locallab.spots.at(i).expreti = mods.locallab.spots.at(i).expreti;
        }

        if (locallab.spots.at(i).complexreti) {
            toEdit.locallab.spots.at(i).complexreti = mods.locallab.spots.at(i).complexreti;
        }

        if (locallab.spots.at(i).retinexMethod) {
            toEdit.locallab.spots.at(i).retinexMethod = mods.locallab.spots.at(i).retinexMethod;
        }

        if (locallab.spots.at(i).str) {
            toEdit.locallab.spots.at(i).str = mods.locallab.spots.at(i).str;
        }

        if (locallab.spots.at(i).chrrt) {
            toEdit.locallab.spots.at(i).chrrt = mods.locallab.spots.at(i).chrrt;
        }

        if (locallab.spots.at(i).neigh) {
            toEdit.locallab.spots.at(i).neigh = mods.locallab.spots.at(i).neigh;
        }

        if (locallab.spots.at(i).vart) {
            toEdit.locallab.spots.at(i).vart = mods.locallab.spots.at(i).vart;
        }

        if (locallab.spots.at(i).offs) {
            toEdit.locallab.spots.at(i).offs = mods.locallab.spots.at(i).offs;
        }

        if (locallab.spots.at(i).dehaz) {
            toEdit.locallab.spots.at(i).dehaz = mods.locallab.spots.at(i).dehaz;
        }

        if (locallab.spots.at(i).depth) {
            toEdit.locallab.spots.at(i).depth = mods.locallab.spots.at(i).depth;
        }

        if (locallab.spots.at(i).sensih) {
            toEdit.locallab.spots.at(i).sensih = mods.locallab.spots.at(i).sensih;
        }

        if (locallab.spots.at(i).localTgaincurve) {
            toEdit.locallab.spots.at(i).localTgaincurve = mods.locallab.spots.at(i).localTgaincurve;
        }

        if (locallab.spots.at(i).localTtranscurve) {
            toEdit.locallab.spots.at(i).localTtranscurve = mods.locallab.spots.at(i).localTtranscurve;
        }

        if (locallab.spots.at(i).inversret) {
            toEdit.locallab.spots.at(i).inversret = mods.locallab.spots.at(i).inversret;
        }

        if (locallab.spots.at(i).equilret) {
            toEdit.locallab.spots.at(i).equilret = mods.locallab.spots.at(i).equilret;
        }

        if (locallab.spots.at(i).loglin) {
            toEdit.locallab.spots.at(i).loglin = mods.locallab.spots.at(i).loglin;
        }

        if (locallab.spots.at(i).dehazeSaturation) {
            toEdit.locallab.spots.at(i).dehazeSaturation = mods.locallab.spots.at(i).dehazeSaturation;
        }

        if (locallab.spots.at(i).dehazeblack) {
            toEdit.locallab.spots.at(i).dehazeblack = mods.locallab.spots.at(i).dehazeblack;
        }

        if (locallab.spots.at(i).softradiusret) {
            toEdit.locallab.spots.at(i).softradiusret = mods.locallab.spots.at(i).softradiusret;
        }

        if (locallab.spots.at(i).CCmaskreticurve) {
            toEdit.locallab.spots.at(i).CCmaskreticurve = mods.locallab.spots.at(i).CCmaskreticurve;
        }

        if (locallab.spots.at(i).LLmaskreticurve) {
            toEdit.locallab.spots.at(i).LLmaskreticurve = mods.locallab.spots.at(i).LLmaskreticurve;
        }

        if (locallab.spots.at(i).HHmaskreticurve) {
            toEdit.locallab.spots.at(i).HHmaskreticurve = mods.locallab.spots.at(i).HHmaskreticurve;
        }

        if (locallab.spots.at(i).enaretiMask) {
            toEdit.locallab.spots.at(i).enaretiMask = mods.locallab.spots.at(i).enaretiMask;
        }

        if (locallab.spots.at(i).enaretiMasktmap) {
            toEdit.locallab.spots.at(i).enaretiMasktmap = mods.locallab.spots.at(i).enaretiMasktmap;
        }

        if (locallab.spots.at(i).blendmaskreti) {
            toEdit.locallab.spots.at(i).blendmaskreti = mods.locallab.spots.at(i).blendmaskreti;
        }

        if (locallab.spots.at(i).radmaskreti) {
            toEdit.locallab.spots.at(i).radmaskreti = mods.locallab.spots.at(i).radmaskreti;
        }

        if (locallab.spots.at(i).chromaskreti) {
            toEdit.locallab.spots.at(i).chromaskreti = mods.locallab.spots.at(i).chromaskreti;
        }

        if (locallab.spots.at(i).gammaskreti) {
            toEdit.locallab.spots.at(i).gammaskreti = mods.locallab.spots.at(i).gammaskreti;
        }

        if (locallab.spots.at(i).slomaskreti) {
            toEdit.locallab.spots.at(i).slomaskreti = mods.locallab.spots.at(i).slomaskreti;
        }

        if (locallab.spots.at(i).lapmaskreti) {
            toEdit.locallab.spots.at(i).lapmaskreti = mods.locallab.spots.at(i).lapmaskreti;
        }

        if (locallab.spots.at(i).scalereti) {
            toEdit.locallab.spots.at(i).scalereti = mods.locallab.spots.at(i).scalereti;
        }

        if (locallab.spots.at(i).darkness) {
            toEdit.locallab.spots.at(i).darkness = mods.locallab.spots.at(i).darkness;
        }

        if (locallab.spots.at(i).lightnessreti) {
            toEdit.locallab.spots.at(i).lightnessreti = mods.locallab.spots.at(i).lightnessreti;
        }

        if (locallab.spots.at(i).limd) {
            toEdit.locallab.spots.at(i).limd = mods.locallab.spots.at(i).limd;
        }

        if (locallab.spots.at(i).cliptm) {
            toEdit.locallab.spots.at(i).cliptm = mods.locallab.spots.at(i).cliptm;
        }

        if (locallab.spots.at(i).fftwreti) {
            toEdit.locallab.spots.at(i).fftwreti = mods.locallab.spots.at(i).fftwreti;
        }

        if (locallab.spots.at(i).Lmaskreticurve) {
            toEdit.locallab.spots.at(i).Lmaskreticurve = mods.locallab.spots.at(i).Lmaskreticurve;
        }

        if (locallab.spots.at(i).recothresr) {
            toEdit.locallab.spots.at(i).recothresr = mods.locallab.spots.at(i).recothresr;
        }

        if (locallab.spots.at(i).lowthresr) {
            toEdit.locallab.spots.at(i).lowthresr = mods.locallab.spots.at(i).lowthresr;
        }

        if (locallab.spots.at(i).higthresr) {
            toEdit.locallab.spots.at(i).higthresr = mods.locallab.spots.at(i).higthresr;
        }

        if (locallab.spots.at(i).decayr) {
            toEdit.locallab.spots.at(i).decayr = mods.locallab.spots.at(i).decayr;
        }

        // Sharpening
        if (locallab.spots.at(i).visisharp) {
            toEdit.locallab.spots.at(i).visisharp = mods.locallab.spots.at(i).visisharp;
        }

        if (locallab.spots.at(i).expsharp) {
            toEdit.locallab.spots.at(i).expsharp = mods.locallab.spots.at(i).expsharp;
        }

        if (locallab.spots.at(i).complexsharp) {
            toEdit.locallab.spots.at(i).complexsharp = mods.locallab.spots.at(i).complexsharp;
        }

        if (locallab.spots.at(i).sharcontrast) {
            toEdit.locallab.spots.at(i).sharcontrast    = mods.locallab.spots.at(i).sharcontrast;
        }

        if (locallab.spots.at(i).sharradius) {
            toEdit.locallab.spots.at(i).sharradius = mods.locallab.spots.at(i).sharradius;
        }

        if (locallab.spots.at(i).sharamount) {
            toEdit.locallab.spots.at(i).sharamount = mods.locallab.spots.at(i).sharamount;
        }

        if (locallab.spots.at(i).shardamping) {
            toEdit.locallab.spots.at(i).shardamping = mods.locallab.spots.at(i).shardamping;
        }

        if (locallab.spots.at(i).shariter) {
            toEdit.locallab.spots.at(i).shariter = mods.locallab.spots.at(i).shariter;
        }

        if (locallab.spots.at(i).sharblur) {
            toEdit.locallab.spots.at(i).sharblur    = mods.locallab.spots.at(i).sharblur;
        }

        if (locallab.spots.at(i).shargam) {
            toEdit.locallab.spots.at(i).shargam    = mods.locallab.spots.at(i).shargam;
        }

        if (locallab.spots.at(i).sensisha) {
            toEdit.locallab.spots.at(i).sensisha = mods.locallab.spots.at(i).sensisha;
        }

        if (locallab.spots.at(i).inverssha) {
            toEdit.locallab.spots.at(i).inverssha = mods.locallab.spots.at(i).inverssha;
        }

        if (locallab.spots.at(i).sharshow) {
            toEdit.locallab.spots.at(i).sharshow = mods.locallab.spots.at(i).sharshow;
        }

        if (locallab.spots.at(i).itercheck) {
            toEdit.locallab.spots.at(i).itercheck = mods.locallab.spots.at(i).itercheck;
        }

        if (locallab.spots.at(i).methodcap) {
            toEdit.locallab.spots.at(i).methodcap = mods.locallab.spots.at(i).methodcap;
        }

        if (locallab.spots.at(i).capradius) {
            toEdit.locallab.spots.at(i).capradius = mods.locallab.spots.at(i).capradius;
        }

        if (locallab.spots.at(i).deconvCoBoost) {
            toEdit.locallab.spots.at(i).deconvCoBoost = mods.locallab.spots.at(i).deconvCoBoost;
        }

        if (locallab.spots.at(i).deconvCoProt) {
            toEdit.locallab.spots.at(i).deconvCoProt = mods.locallab.spots.at(i).deconvCoProt;
        }

        if (locallab.spots.at(i).deconvCoLat) {
            toEdit.locallab.spots.at(i).deconvCoLat = mods.locallab.spots.at(i).deconvCoLat;
        }

        if (locallab.spots.at(i).deconvCogam) {
            toEdit.locallab.spots.at(i).deconvCogam = mods.locallab.spots.at(i).deconvCogam;
        }

        if (locallab.spots.at(i).reparsha) {
            toEdit.locallab.spots.at(i).reparsha = mods.locallab.spots.at(i).reparsha;
        }

        if (locallab.spots.at(i).deconvAutoRadius) {
            toEdit.locallab.spots.at(i).deconvAutoRadius = mods.locallab.spots.at(i).deconvAutoRadius;
        }
          
        if (locallab.spots.at(i).deconvAutoshar) {
            toEdit.locallab.spots.at(i).deconvAutoshar = mods.locallab.spots.at(i).deconvAutoshar;
        }

        // Local Contrast
        if (locallab.spots.at(i).visicontrast) {
            toEdit.locallab.spots.at(i).visicontrast   = mods.locallab.spots.at(i).visicontrast;
        }

        if (locallab.spots.at(i).expcontrast) {
            toEdit.locallab.spots.at(i).expcontrast   = mods.locallab.spots.at(i).expcontrast;
        }

        if (locallab.spots.at(i).complexcontrast) {
            toEdit.locallab.spots.at(i).complexcontrast   = mods.locallab.spots.at(i).complexcontrast;
        }

        if (locallab.spots.at(i).lcradius) {
            toEdit.locallab.spots.at(i).lcradius   = mods.locallab.spots.at(i).lcradius;
        }

        if (locallab.spots.at(i).lcamount) {
            toEdit.locallab.spots.at(i).lcamount   = mods.locallab.spots.at(i).lcamount;
        }

        if (locallab.spots.at(i).lcdarkness) {
            toEdit.locallab.spots.at(i).lcdarkness   = mods.locallab.spots.at(i).lcdarkness;
        }

        if (locallab.spots.at(i).lclightness) {
            toEdit.locallab.spots.at(i).lclightness   = mods.locallab.spots.at(i).lclightness;
        }

        if (locallab.spots.at(i).sigmalc) {
            toEdit.locallab.spots.at(i).sigmalc   = mods.locallab.spots.at(i).sigmalc;
        }

        if (locallab.spots.at(i).offslc) {
            toEdit.locallab.spots.at(i).offslc   = mods.locallab.spots.at(i).offslc;
        }

        if (locallab.spots.at(i).levelwav) {
            toEdit.locallab.spots.at(i).levelwav   = mods.locallab.spots.at(i).levelwav;
        }

        if (locallab.spots.at(i).residcont) {
            toEdit.locallab.spots.at(i).residcont   = mods.locallab.spots.at(i).residcont;
        }

        if (locallab.spots.at(i).residsha) {
            toEdit.locallab.spots.at(i).residsha   = mods.locallab.spots.at(i).residsha;
        }

        if (locallab.spots.at(i).residshathr) {
            toEdit.locallab.spots.at(i).residshathr   = mods.locallab.spots.at(i).residshathr;
        }

        if (locallab.spots.at(i).residhi) {
            toEdit.locallab.spots.at(i).residhi   = mods.locallab.spots.at(i).residhi;
        }

        if (locallab.spots.at(i).residhithr) {
            toEdit.locallab.spots.at(i).residhithr   = mods.locallab.spots.at(i).residhithr;
        }

        if (locallab.spots.at(i).gamlc) {
            toEdit.locallab.spots.at(i).gamlc   = mods.locallab.spots.at(i).gamlc;
        }

        if (locallab.spots.at(i).residgam) {
            toEdit.locallab.spots.at(i).residgam   = mods.locallab.spots.at(i).residgam;
        }

        if (locallab.spots.at(i).residslop) {
            toEdit.locallab.spots.at(i).residslop   = mods.locallab.spots.at(i).residslop;
        }

        if (locallab.spots.at(i).residblur) {
            toEdit.locallab.spots.at(i).residblur   = mods.locallab.spots.at(i).residblur;
        }

        if (locallab.spots.at(i).levelblur) {
            toEdit.locallab.spots.at(i).levelblur   = mods.locallab.spots.at(i).levelblur;
        }

        if (locallab.spots.at(i).sigmabl) {
            toEdit.locallab.spots.at(i).sigmabl   = mods.locallab.spots.at(i).sigmabl;
        }

        if (locallab.spots.at(i).residchro) {
            toEdit.locallab.spots.at(i).residchro   = mods.locallab.spots.at(i).residchro;
        }

        if (locallab.spots.at(i).residcomp) {
            toEdit.locallab.spots.at(i).residcomp   = mods.locallab.spots.at(i).residcomp;
        }


        if (locallab.spots.at(i).sigma) {
            toEdit.locallab.spots.at(i).sigma   = mods.locallab.spots.at(i).sigma;
        }

        if (locallab.spots.at(i).offset) {
            toEdit.locallab.spots.at(i).offset   = mods.locallab.spots.at(i).offset;
        }

        if (locallab.spots.at(i).sigmadr) {
            toEdit.locallab.spots.at(i).sigmadr   = mods.locallab.spots.at(i).sigmadr;
        }

        if (locallab.spots.at(i).threswav) {
            toEdit.locallab.spots.at(i).threswav   = mods.locallab.spots.at(i).threswav;
        }

        if (locallab.spots.at(i).chromalev) {
            toEdit.locallab.spots.at(i).chromalev   = mods.locallab.spots.at(i).chromalev;
        }

        if (locallab.spots.at(i).chromablu) {
            toEdit.locallab.spots.at(i).chromablu   = mods.locallab.spots.at(i).chromablu;
        }

        if (locallab.spots.at(i).sigmadc) {
            toEdit.locallab.spots.at(i).sigmadc   = mods.locallab.spots.at(i).sigmadc;
        }

        if (locallab.spots.at(i).deltad) {
            toEdit.locallab.spots.at(i).deltad   = mods.locallab.spots.at(i).deltad;
        }

        if (locallab.spots.at(i).fatres) {
            toEdit.locallab.spots.at(i).fatres   = mods.locallab.spots.at(i).fatres;
        }

        if (locallab.spots.at(i).clarilres) {
            toEdit.locallab.spots.at(i).clarilres   = mods.locallab.spots.at(i).clarilres;
        }

        if (locallab.spots.at(i).claricres) {
            toEdit.locallab.spots.at(i).claricres   = mods.locallab.spots.at(i).claricres;
        }

        if (locallab.spots.at(i).clarisoft) {
            toEdit.locallab.spots.at(i).clarisoft   = mods.locallab.spots.at(i).clarisoft;
        }

        if (locallab.spots.at(i).sigmalc2) {
            toEdit.locallab.spots.at(i).sigmalc2   = mods.locallab.spots.at(i).sigmalc2;
        }

        if (locallab.spots.at(i).strwav) {
            toEdit.locallab.spots.at(i).strwav   = mods.locallab.spots.at(i).strwav;
        }

        if (locallab.spots.at(i).angwav) {
            toEdit.locallab.spots.at(i).angwav   = mods.locallab.spots.at(i).angwav;
        }

        if (locallab.spots.at(i).featherwav) {
            toEdit.locallab.spots.at(i).featherwav   = mods.locallab.spots.at(i).featherwav;
        }

        if (locallab.spots.at(i).strengthw) {
            toEdit.locallab.spots.at(i).strengthw   = mods.locallab.spots.at(i).strengthw;
        }

        if (locallab.spots.at(i).sigmaed) {
            toEdit.locallab.spots.at(i).sigmaed   = mods.locallab.spots.at(i).sigmaed;
        }

        if (locallab.spots.at(i).radiusw) {
            toEdit.locallab.spots.at(i).radiusw   = mods.locallab.spots.at(i).radiusw;
        }

        if (locallab.spots.at(i).detailw) {
            toEdit.locallab.spots.at(i).detailw   = mods.locallab.spots.at(i).detailw;
        }

        if (locallab.spots.at(i).gradw) {
            toEdit.locallab.spots.at(i).gradw   = mods.locallab.spots.at(i).gradw;
        }

        if (locallab.spots.at(i).tloww) {
            toEdit.locallab.spots.at(i).tloww   = mods.locallab.spots.at(i).tloww;
        }

        if (locallab.spots.at(i).thigw) {
            toEdit.locallab.spots.at(i).thigw   = mods.locallab.spots.at(i).thigw;
        }

        if (locallab.spots.at(i).edgw) {
            toEdit.locallab.spots.at(i).edgw   = mods.locallab.spots.at(i).edgw;
        }

        if (locallab.spots.at(i).basew) {
            toEdit.locallab.spots.at(i).basew   = mods.locallab.spots.at(i).basew;
        }

        if (locallab.spots.at(i).sensilc) {
            toEdit.locallab.spots.at(i).sensilc   = mods.locallab.spots.at(i).sensilc;
        }

        if (locallab.spots.at(i).reparw) {
            toEdit.locallab.spots.at(i).reparw   = mods.locallab.spots.at(i).reparw;
        }

        if (locallab.spots.at(i).fftwlc) {
            toEdit.locallab.spots.at(i).fftwlc = mods.locallab.spots.at(i).fftwlc;
        }

        if (locallab.spots.at(i).blurlc) {
            toEdit.locallab.spots.at(i).blurlc = mods.locallab.spots.at(i).blurlc;
        }

        if (locallab.spots.at(i).wavblur) {
            toEdit.locallab.spots.at(i).wavblur = mods.locallab.spots.at(i).wavblur;
        }

        if (locallab.spots.at(i).wavedg) {
            toEdit.locallab.spots.at(i).wavedg = mods.locallab.spots.at(i).wavedg;
        }

        if (locallab.spots.at(i).waveshow) {
            toEdit.locallab.spots.at(i).waveshow = mods.locallab.spots.at(i).waveshow;
        }

        if (locallab.spots.at(i).wavcont) {
            toEdit.locallab.spots.at(i).wavcont = mods.locallab.spots.at(i).wavcont;
        }

        if (locallab.spots.at(i).wavcomp) {
            toEdit.locallab.spots.at(i).wavcomp = mods.locallab.spots.at(i).wavcomp;
        }

        if (locallab.spots.at(i).wavgradl) {
            toEdit.locallab.spots.at(i).wavgradl = mods.locallab.spots.at(i).wavgradl;
        }

        if (locallab.spots.at(i).wavcompre) {
            toEdit.locallab.spots.at(i).wavcompre = mods.locallab.spots.at(i).wavcompre;
        }

        if (locallab.spots.at(i).origlc) {
            toEdit.locallab.spots.at(i).origlc = mods.locallab.spots.at(i).origlc;
        }

        if (locallab.spots.at(i).processwav) {
            toEdit.locallab.spots.at(i).processwav = mods.locallab.spots.at(i).processwav;
        }

        if (locallab.spots.at(i).localcontMethod) {
            toEdit.locallab.spots.at(i).localcontMethod = mods.locallab.spots.at(i).localcontMethod;
        }

        if (locallab.spots.at(i).localedgMethod) {
            toEdit.locallab.spots.at(i).localedgMethod = mods.locallab.spots.at(i).localedgMethod;
        }

        if (locallab.spots.at(i).localneiMethod) {
            toEdit.locallab.spots.at(i).localneiMethod = mods.locallab.spots.at(i).localneiMethod;
        }

        if (locallab.spots.at(i).locwavcurve) {
            toEdit.locallab.spots.at(i).locwavcurve = mods.locallab.spots.at(i).locwavcurve;
        }

        if (locallab.spots.at(i).csthreshold) {
            toEdit.locallab.spots.at(i).csthreshold = mods.locallab.spots.at(i).csthreshold;
        }

        if (locallab.spots.at(i).loclevwavcurve) {
            toEdit.locallab.spots.at(i).loclevwavcurve = mods.locallab.spots.at(i).loclevwavcurve;
        }

        if (locallab.spots.at(i).locconwavcurve) {
            toEdit.locallab.spots.at(i).locconwavcurve = mods.locallab.spots.at(i).locconwavcurve;
        }

        if (locallab.spots.at(i).loccompwavcurve) {
            toEdit.locallab.spots.at(i).loccompwavcurve = mods.locallab.spots.at(i).loccompwavcurve;
        }

        if (locallab.spots.at(i).loccomprewavcurve) {
            toEdit.locallab.spots.at(i).loccomprewavcurve = mods.locallab.spots.at(i).loccomprewavcurve;
        }

        if (locallab.spots.at(i).locedgwavcurve) {
            toEdit.locallab.spots.at(i).locedgwavcurve = mods.locallab.spots.at(i).locedgwavcurve;
        }

        if (locallab.spots.at(i).CCmasklccurve) {
            toEdit.locallab.spots.at(i).CCmasklccurve = mods.locallab.spots.at(i).CCmasklccurve;
        }

        if (locallab.spots.at(i).LLmasklccurve) {
            toEdit.locallab.spots.at(i).LLmasklccurve = mods.locallab.spots.at(i).LLmasklccurve;
        }

        if (locallab.spots.at(i).HHmasklccurve) {
            toEdit.locallab.spots.at(i).HHmasklccurve = mods.locallab.spots.at(i).HHmasklccurve;
        }

        if (locallab.spots.at(i).enalcMask) {
            toEdit.locallab.spots.at(i).enalcMask = mods.locallab.spots.at(i).enalcMask;
        }

        if (locallab.spots.at(i).blendmasklc) {
            toEdit.locallab.spots.at(i).blendmasklc = mods.locallab.spots.at(i).blendmasklc;
        }

        if (locallab.spots.at(i).radmasklc) {
            toEdit.locallab.spots.at(i).radmasklc = mods.locallab.spots.at(i).radmasklc;
        }

        if (locallab.spots.at(i).chromasklc) {
            toEdit.locallab.spots.at(i).chromasklc = mods.locallab.spots.at(i).chromasklc;
        }

        if (locallab.spots.at(i).Lmasklccurve) {
            toEdit.locallab.spots.at(i).Lmasklccurve = mods.locallab.spots.at(i).Lmasklccurve;
        }

        if (locallab.spots.at(i).recothresw) {
            toEdit.locallab.spots.at(i).recothresw = mods.locallab.spots.at(i).recothresw;
        }

        if (locallab.spots.at(i).lowthresw) {
            toEdit.locallab.spots.at(i).lowthresw = mods.locallab.spots.at(i).lowthresw;
        }

        if (locallab.spots.at(i).higthresw) {
            toEdit.locallab.spots.at(i).higthresw = mods.locallab.spots.at(i).higthresw;
        }

        if (locallab.spots.at(i).decayw) {
            toEdit.locallab.spots.at(i).decayw = mods.locallab.spots.at(i).decayw;
        }

        // Contrast by detail levels
        if (locallab.spots.at(i).visicbdl) {
            toEdit.locallab.spots.at(i).visicbdl = mods.locallab.spots.at(i).visicbdl;
        }

        if (locallab.spots.at(i).expcbdl) {
            toEdit.locallab.spots.at(i).expcbdl = mods.locallab.spots.at(i).expcbdl;
        }

        if (locallab.spots.at(i).complexcbdl) {
            toEdit.locallab.spots.at(i).complexcbdl = mods.locallab.spots.at(i).complexcbdl;
        }

        for (int j = 0; j < 6; j++) {
            if (locallab.spots.at(i).mult[j]) {
                toEdit.locallab.spots.at(i).mult[j] = mods.locallab.spots.at(i).mult[j];
            }
        }

        if (locallab.spots.at(i).chromacbdl) {
            toEdit.locallab.spots.at(i).chromacbdl = mods.locallab.spots.at(i).chromacbdl;
        }

        if (locallab.spots.at(i).threshold) {
            toEdit.locallab.spots.at(i).threshold = mods.locallab.spots.at(i).threshold;
        }

        if (locallab.spots.at(i).sensicb) {
            toEdit.locallab.spots.at(i).sensicb = mods.locallab.spots.at(i).sensicb;
        }

        if (locallab.spots.at(i).clarityml) {
            toEdit.locallab.spots.at(i).clarityml = mods.locallab.spots.at(i).clarityml;
        }

        if (locallab.spots.at(i).contresid) {
            toEdit.locallab.spots.at(i).contresid = mods.locallab.spots.at(i).contresid;
        }

        if (locallab.spots.at(i).softradiuscb) {
            toEdit.locallab.spots.at(i).softradiuscb = mods.locallab.spots.at(i).softradiuscb;
        }

        if (locallab.spots.at(i).enacbMask) {
            toEdit.locallab.spots.at(i).enacbMask = mods.locallab.spots.at(i).enacbMask;
        }

        if (locallab.spots.at(i).CCmaskcbcurve) {
            toEdit.locallab.spots.at(i).CCmaskcbcurve = mods.locallab.spots.at(i).CCmaskcbcurve;
        }

        if (locallab.spots.at(i).LLmaskcbcurve) {
            toEdit.locallab.spots.at(i).LLmaskcbcurve = mods.locallab.spots.at(i).LLmaskcbcurve;
        }

        if (locallab.spots.at(i).HHmaskcbcurve) {
            toEdit.locallab.spots.at(i).HHmaskcbcurve = mods.locallab.spots.at(i).HHmaskcbcurve;
        }

        if (locallab.spots.at(i).blendmaskcb) {
            toEdit.locallab.spots.at(i).blendmaskcb = mods.locallab.spots.at(i).blendmaskcb;
        }

        if (locallab.spots.at(i).radmaskcb) {
            toEdit.locallab.spots.at(i).radmaskcb = mods.locallab.spots.at(i).radmaskcb;
        }

        if (locallab.spots.at(i).chromaskcb) {
            toEdit.locallab.spots.at(i).chromaskcb = mods.locallab.spots.at(i).chromaskcb;
        }

        if (locallab.spots.at(i).gammaskcb) {
            toEdit.locallab.spots.at(i).gammaskcb = mods.locallab.spots.at(i).gammaskcb;
        }

        if (locallab.spots.at(i).slomaskcb) {
            toEdit.locallab.spots.at(i).slomaskcb = mods.locallab.spots.at(i).slomaskcb;
        }

        if (locallab.spots.at(i).lapmaskcb) {
            toEdit.locallab.spots.at(i).lapmaskcb = mods.locallab.spots.at(i).lapmaskcb;
        }

        if (locallab.spots.at(i).Lmaskcbcurve) {
            toEdit.locallab.spots.at(i).Lmaskcbcurve = mods.locallab.spots.at(i).Lmaskcbcurve;
        }

        if (locallab.spots.at(i).recothrescb) {
            toEdit.locallab.spots.at(i).recothrescb = mods.locallab.spots.at(i).recothrescb;
        }

        if (locallab.spots.at(i).lowthrescb) {
            toEdit.locallab.spots.at(i).lowthrescb = mods.locallab.spots.at(i).lowthrescb;
        }

        if (locallab.spots.at(i).higthrescb) {
            toEdit.locallab.spots.at(i).higthrescb = mods.locallab.spots.at(i).higthrescb;
        }

        if (locallab.spots.at(i).decaycb) {
            toEdit.locallab.spots.at(i).decaycb = mods.locallab.spots.at(i).decaycb;
        }

        // Log encoding
        if (locallab.spots.at(i).visilog) {
            toEdit.locallab.spots.at(i).visilog = mods.locallab.spots.at(i).visilog;
        }

        if (locallab.spots.at(i).explog) {
            toEdit.locallab.spots.at(i).explog = mods.locallab.spots.at(i).explog;
        }

        if (locallab.spots.at(i).complexlog) {
            toEdit.locallab.spots.at(i).complexlog = mods.locallab.spots.at(i).complexlog;
        }

        if (locallab.spots.at(i).autocompute) {
            toEdit.locallab.spots.at(i).autocompute = mods.locallab.spots.at(i).autocompute;
        }

        if (locallab.spots.at(i).sourceGray) {
            toEdit.locallab.spots.at(i).sourceGray = mods.locallab.spots.at(i).sourceGray;
        }

        if (locallab.spots.at(i).sourceabs) {
            toEdit.locallab.spots.at(i).sourceabs = mods.locallab.spots.at(i).sourceabs;
        }

        if (locallab.spots.at(i).targabs) {
            toEdit.locallab.spots.at(i).targabs = mods.locallab.spots.at(i).targabs;
        }

        if (locallab.spots.at(i).targetGray) {
            toEdit.locallab.spots.at(i).targetGray = mods.locallab.spots.at(i).targetGray;
        }

        if (locallab.spots.at(i).catad) {
            toEdit.locallab.spots.at(i).catad = mods.locallab.spots.at(i).catad;
        }

        if (locallab.spots.at(i).saturl) {
            toEdit.locallab.spots.at(i).saturl = mods.locallab.spots.at(i).saturl;
        }

        if (locallab.spots.at(i).chroml) {
            toEdit.locallab.spots.at(i).chroml = mods.locallab.spots.at(i).chroml;
        }

        if (locallab.spots.at(i).lightl) {
            toEdit.locallab.spots.at(i).lightl = mods.locallab.spots.at(i).lightl;
        }

        if (locallab.spots.at(i).lightq) {
            toEdit.locallab.spots.at(i).lightq = mods.locallab.spots.at(i).lightq;
        }

        if (locallab.spots.at(i).contl) {
            toEdit.locallab.spots.at(i).contl = mods.locallab.spots.at(i).contl;
        }

        if (locallab.spots.at(i).contthres) {
            toEdit.locallab.spots.at(i).contthres = mods.locallab.spots.at(i).contthres;
        }

        if (locallab.spots.at(i).contq) {
            toEdit.locallab.spots.at(i).contq = mods.locallab.spots.at(i).contq;
        }

        if (locallab.spots.at(i).colorfl) {
            toEdit.locallab.spots.at(i).colorfl = mods.locallab.spots.at(i).colorfl;
        }

        if (locallab.spots.at(i).LcurveL) {
            toEdit.locallab.spots.at(i).LcurveL = mods.locallab.spots.at(i).LcurveL;
        }

        if (locallab.spots.at(i).Autogray) {
            toEdit.locallab.spots.at(i).Autogray = mods.locallab.spots.at(i).Autogray;
        }

        if (locallab.spots.at(i).fullimage) {
            toEdit.locallab.spots.at(i).fullimage = mods.locallab.spots.at(i).fullimage;
        }

        if (locallab.spots.at(i).ciecam) {
            toEdit.locallab.spots.at(i).ciecam = mods.locallab.spots.at(i).ciecam;
        }

        if (locallab.spots.at(i).satlog) {
            toEdit.locallab.spots.at(i).satlog = mods.locallab.spots.at(i).satlog;
        }

        if (locallab.spots.at(i).enaLMask) {
            toEdit.locallab.spots.at(i).enaLMask = mods.locallab.spots.at(i).enaLMask;
        }

        if (locallab.spots.at(i).repar) {
            toEdit.locallab.spots.at(i).repar = mods.locallab.spots.at(i).repar;
        }

        if (locallab.spots.at(i).blackEv) {
            toEdit.locallab.spots.at(i).blackEv = mods.locallab.spots.at(i).blackEv;
        }

        if (locallab.spots.at(i).whiteEv) {
            toEdit.locallab.spots.at(i).whiteEv = mods.locallab.spots.at(i).whiteEv;
        }

        if (locallab.spots.at(i).whiteslog) {
            toEdit.locallab.spots.at(i).whiteslog = mods.locallab.spots.at(i).whiteslog;
        }

        if (locallab.spots.at(i).blackslog) {
            toEdit.locallab.spots.at(i).blackslog = mods.locallab.spots.at(i).blackslog;
        }

        if (locallab.spots.at(i).comprlog) {
            toEdit.locallab.spots.at(i).comprlog = mods.locallab.spots.at(i).comprlog;
        }

        if (locallab.spots.at(i).strelog) {
            toEdit.locallab.spots.at(i).strelog = mods.locallab.spots.at(i).strelog;
        }

        if (locallab.spots.at(i).detail) {
            toEdit.locallab.spots.at(i).detail = mods.locallab.spots.at(i).detail;
        }

        if (locallab.spots.at(i).sensilog) {
            toEdit.locallab.spots.at(i).sensilog = mods.locallab.spots.at(i).sensilog;
        }

        if (locallab.spots.at(i).baselog) {
            toEdit.locallab.spots.at(i).baselog = mods.locallab.spots.at(i).baselog;
        }

        if (locallab.spots.at(i).sursour) {
            toEdit.locallab.spots.at(i).sursour = mods.locallab.spots.at(i).sursour;
        }

        if (locallab.spots.at(i).surround) {
            toEdit.locallab.spots.at(i).surround = mods.locallab.spots.at(i).surround;
        }

        if (locallab.spots.at(i).strlog) {
            toEdit.locallab.spots.at(i).strlog   = mods.locallab.spots.at(i).strlog;
        }

        if (locallab.spots.at(i).anglog) {
            toEdit.locallab.spots.at(i).anglog   = mods.locallab.spots.at(i).anglog;
        }

        if (locallab.spots.at(i).featherlog) {
            toEdit.locallab.spots.at(i).featherlog   = mods.locallab.spots.at(i).featherlog;
        }

        if (locallab.spots.at(i).CCmaskcurveL) {
            toEdit.locallab.spots.at(i).CCmaskcurveL = mods.locallab.spots.at(i).CCmaskcurveL;
        }

        if (locallab.spots.at(i).LLmaskcurveL) {
            toEdit.locallab.spots.at(i).LLmaskcurveL = mods.locallab.spots.at(i).LLmaskcurveL;
        }

        if (locallab.spots.at(i).HHmaskcurveL) {
            toEdit.locallab.spots.at(i).HHmaskcurveL = mods.locallab.spots.at(i).HHmaskcurveL;
        }

        if (locallab.spots.at(i).blendmaskL) {
            toEdit.locallab.spots.at(i).blendmaskL   = mods.locallab.spots.at(i).blendmaskL;
        }

        if (locallab.spots.at(i).radmaskL) {
            toEdit.locallab.spots.at(i).radmaskL   = mods.locallab.spots.at(i).radmaskL;
        }

        if (locallab.spots.at(i).chromaskL) {
            toEdit.locallab.spots.at(i).chromaskL   = mods.locallab.spots.at(i).chromaskL;
        }

        if (locallab.spots.at(i).LmaskcurveL) {
            toEdit.locallab.spots.at(i).LmaskcurveL = mods.locallab.spots.at(i).LmaskcurveL;
        }

        if (locallab.spots.at(i).recothresl) {
            toEdit.locallab.spots.at(i).recothresl = mods.locallab.spots.at(i).recothresl;
        }

        if (locallab.spots.at(i).lowthresl) {
            toEdit.locallab.spots.at(i).lowthresl = mods.locallab.spots.at(i).lowthresl;
        }

        if (locallab.spots.at(i).higthresl) {
            toEdit.locallab.spots.at(i).higthresl = mods.locallab.spots.at(i).higthresl;
        }

        if (locallab.spots.at(i).decayl) {
            toEdit.locallab.spots.at(i).decayl = mods.locallab.spots.at(i).decayl;
        }

        // mask
        if (locallab.spots.at(i).visimask) {
            toEdit.locallab.spots.at(i).visimask = mods.locallab.spots.at(i).visimask;
        }

        if (locallab.spots.at(i).complexmask) {
            toEdit.locallab.spots.at(i).complexmask = mods.locallab.spots.at(i).complexmask;
        }

        if (locallab.spots.at(i).expmask) {
            toEdit.locallab.spots.at(i).expmask = mods.locallab.spots.at(i).expmask;
        }

        if (locallab.spots.at(i).sensimask) {
            toEdit.locallab.spots.at(i).sensimask = mods.locallab.spots.at(i).sensimask;
        }

        if (locallab.spots.at(i).blendmask) {
            toEdit.locallab.spots.at(i).blendmask = mods.locallab.spots.at(i).blendmask;
        }

        if (locallab.spots.at(i).blendmaskab) {
            toEdit.locallab.spots.at(i).blendmaskab = mods.locallab.spots.at(i).blendmaskab;
        }

        if (locallab.spots.at(i).softradiusmask) {
            toEdit.locallab.spots.at(i).softradiusmask = mods.locallab.spots.at(i).softradiusmask;
        }

        if (locallab.spots.at(i).enamask) {
            toEdit.locallab.spots.at(i).enamask = mods.locallab.spots.at(i).enamask;
        }

        if (locallab.spots.at(i).fftmask) {
            toEdit.locallab.spots.at(i).fftmask = mods.locallab.spots.at(i).fftmask;
        }

        if (locallab.spots.at(i).blurmask) {
            toEdit.locallab.spots.at(i).blurmask = mods.locallab.spots.at(i).blurmask;
        }

        if (locallab.spots.at(i).contmask) {
            toEdit.locallab.spots.at(i).contmask = mods.locallab.spots.at(i).contmask;
        }

        if (locallab.spots.at(i).CCmask_curve) {
            toEdit.locallab.spots.at(i).CCmask_curve = mods.locallab.spots.at(i).CCmask_curve;
        }

        if (locallab.spots.at(i).LLmask_curve) {
            toEdit.locallab.spots.at(i).LLmask_curve = mods.locallab.spots.at(i).LLmask_curve;
        }

        if (locallab.spots.at(i).HHmask_curve) {
            toEdit.locallab.spots.at(i).HHmask_curve = mods.locallab.spots.at(i).HHmask_curve;
        }

        if (locallab.spots.at(i).strumaskmask) {
            toEdit.locallab.spots.at(i).strumaskmask = mods.locallab.spots.at(i).strumaskmask;
        }

        if (locallab.spots.at(i).toolmask) {
            toEdit.locallab.spots.at(i).toolmask = mods.locallab.spots.at(i).toolmask;
        }

        if (locallab.spots.at(i).radmask) {
            toEdit.locallab.spots.at(i).radmask = mods.locallab.spots.at(i).radmask;
        }

        if (locallab.spots.at(i).lapmask) {
            toEdit.locallab.spots.at(i).lapmask = mods.locallab.spots.at(i).lapmask;
        }

        if (locallab.spots.at(i).chromask) {
            toEdit.locallab.spots.at(i).chromask = mods.locallab.spots.at(i).chromask;
        }

        if (locallab.spots.at(i).gammask) {
            toEdit.locallab.spots.at(i).gammask = mods.locallab.spots.at(i).gammask;
        }

        if (locallab.spots.at(i).slopmask) {
            toEdit.locallab.spots.at(i).slopmask = mods.locallab.spots.at(i).slopmask;
        }

        if (locallab.spots.at(i).shadmask) {
            toEdit.locallab.spots.at(i).shadmask = mods.locallab.spots.at(i).shadmask;
        }

        if (locallab.spots.at(i).str_mask) {
            toEdit.locallab.spots.at(i).str_mask = mods.locallab.spots.at(i).str_mask;
        }

        if (locallab.spots.at(i).ang_mask) {
            toEdit.locallab.spots.at(i).ang_mask = mods.locallab.spots.at(i).ang_mask;
        }

        if (locallab.spots.at(i).feather_mask) {
            toEdit.locallab.spots.at(i).feather_mask = mods.locallab.spots.at(i).feather_mask;
        }

        if (locallab.spots.at(i).HHhmask_curve) {
            toEdit.locallab.spots.at(i).HHhmask_curve = mods.locallab.spots.at(i).HHhmask_curve;
        }

        if (locallab.spots.at(i).Lmask_curve) {
            toEdit.locallab.spots.at(i).Lmask_curve = mods.locallab.spots.at(i).Lmask_curve;
        }

        if (locallab.spots.at(i).LLmask_curvewav) {
            toEdit.locallab.spots.at(i).LLmask_curvewav = mods.locallab.spots.at(i).LLmask_curvewav;
        }

        if (locallab.spots.at(i).csthresholdmask) {
            toEdit.locallab.spots.at(i).csthresholdmask = mods.locallab.spots.at(i).csthresholdmask;
        }

        //ciecam
        if (locallab.spots.at(i).visicie) {
            toEdit.locallab.spots.at(i).visicie = mods.locallab.spots.at(i).visicie;
        }

        if (locallab.spots.at(i).expcie) {
            toEdit.locallab.spots.at(i).expcie = mods.locallab.spots.at(i).expcie;
        }

        if (locallab.spots.at(i).expprecam) {
            toEdit.locallab.spots.at(i).expprecam = mods.locallab.spots.at(i).expprecam;
        }

        if (locallab.spots.at(i).complexcie) {
            toEdit.locallab.spots.at(i).complexcie = mods.locallab.spots.at(i).complexcie;
        }

        if (locallab.spots.at(i).reparcie) {
            toEdit.locallab.spots.at(i).reparcie = mods.locallab.spots.at(i).reparcie;
        }

        if (locallab.spots.at(i).sensicie) {
            toEdit.locallab.spots.at(i).sensicie = mods.locallab.spots.at(i).sensicie;
        }

        if (locallab.spots.at(i).Autograycie) {
            toEdit.locallab.spots.at(i).Autograycie = mods.locallab.spots.at(i).Autograycie;
        }

        if (locallab.spots.at(i).sigybjz12) {
            toEdit.locallab.spots.at(i).sigybjz12 = mods.locallab.spots.at(i).sigybjz12;
        }

        if (locallab.spots.at(i).qtoj) {
            toEdit.locallab.spots.at(i).qtoj = mods.locallab.spots.at(i).qtoj;
        }

        if (locallab.spots.at(i).jabcie) {
            toEdit.locallab.spots.at(i).jabcie = mods.locallab.spots.at(i).jabcie;
        }

        if (locallab.spots.at(i).comprcieauto) {
            toEdit.locallab.spots.at(i).comprcieauto = mods.locallab.spots.at(i).comprcieauto;
        }

        if (locallab.spots.at(i).normcie12) {
            toEdit.locallab.spots.at(i).normcie12 = mods.locallab.spots.at(i).normcie12;
        }

        if (locallab.spots.at(i).normcie) {
            toEdit.locallab.spots.at(i).normcie = mods.locallab.spots.at(i).normcie;
        }

        if (locallab.spots.at(i).gamutcie) {
            toEdit.locallab.spots.at(i).gamutcie = mods.locallab.spots.at(i).gamutcie;
        }

        if (locallab.spots.at(i).bwcie) {
            toEdit.locallab.spots.at(i).bwcie = mods.locallab.spots.at(i).bwcie;
        }

        if (locallab.spots.at(i).sigcie) {
            toEdit.locallab.spots.at(i).sigcie = mods.locallab.spots.at(i).sigcie;
        }

        if (locallab.spots.at(i).logcie) {
            toEdit.locallab.spots.at(i).logcie = mods.locallab.spots.at(i).logcie;
        }

        if (locallab.spots.at(i).satcie) {
            toEdit.locallab.spots.at(i).satcie = mods.locallab.spots.at(i).satcie;
        }

        if (locallab.spots.at(i).logcieq) {
            toEdit.locallab.spots.at(i).logcieq = mods.locallab.spots.at(i).logcieq;
        }

        if (locallab.spots.at(i).smoothcie) {
            toEdit.locallab.spots.at(i).smoothcie = mods.locallab.spots.at(i).smoothcie;
        }

        if (locallab.spots.at(i).smoothcietrc) {
            toEdit.locallab.spots.at(i).smoothcietrc = mods.locallab.spots.at(i).smoothcietrc;
        }

        if (locallab.spots.at(i).smoothcietrcrel) {
            toEdit.locallab.spots.at(i).smoothcietrcrel = mods.locallab.spots.at(i).smoothcietrcrel;
        }

        if (locallab.spots.at(i).smoothcieyb) {
            toEdit.locallab.spots.at(i).smoothcieyb = mods.locallab.spots.at(i).smoothcieyb;
        }

        if (locallab.spots.at(i).smoothcielum) {
            toEdit.locallab.spots.at(i).smoothcielum = mods.locallab.spots.at(i).smoothcielum;
        }

        if (locallab.spots.at(i).smoothciehigh) {
            toEdit.locallab.spots.at(i).smoothciehigh = mods.locallab.spots.at(i).smoothciehigh;
        }

        if (locallab.spots.at(i).smoothcielnk) {
            toEdit.locallab.spots.at(i).smoothcielnk = mods.locallab.spots.at(i).smoothcielnk;
        }

        if (locallab.spots.at(i).smoothcieinv) {
            toEdit.locallab.spots.at(i).smoothcieinv = mods.locallab.spots.at(i).smoothcieinv;
        }

        if (locallab.spots.at(i).logjz) {
            toEdit.locallab.spots.at(i).logjz = mods.locallab.spots.at(i).logjz;
        }

        if (locallab.spots.at(i).sigjz12) {
            toEdit.locallab.spots.at(i).sigjz12 = mods.locallab.spots.at(i).sigjz12;
        }

        if (locallab.spots.at(i).sigjz) {
            toEdit.locallab.spots.at(i).sigjz = mods.locallab.spots.at(i).sigjz;
        }

        if (locallab.spots.at(i).forcebw) {
            toEdit.locallab.spots.at(i).forcebw = mods.locallab.spots.at(i).forcebw;
        }

        if (locallab.spots.at(i).sigq12) {
            toEdit.locallab.spots.at(i).sigq12 = mods.locallab.spots.at(i).sigq12;
        }

        if (locallab.spots.at(i).sigq) {
            toEdit.locallab.spots.at(i).sigq = mods.locallab.spots.at(i).sigq;
        }

        if (locallab.spots.at(i).chjzcie) {
            toEdit.locallab.spots.at(i).chjzcie = mods.locallab.spots.at(i).chjzcie;
        }

        if (locallab.spots.at(i).sourceGraycie) {
            toEdit.locallab.spots.at(i).sourceGraycie = mods.locallab.spots.at(i).sourceGraycie;
        }

        if (locallab.spots.at(i).sourceabscie) {
            toEdit.locallab.spots.at(i).sourceabscie = mods.locallab.spots.at(i).sourceabscie;
        }

        if (locallab.spots.at(i).sursourcie) {
            toEdit.locallab.spots.at(i).sursourcie = mods.locallab.spots.at(i).sursourcie;
        }

        if (locallab.spots.at(i).modecam) {
            toEdit.locallab.spots.at(i).modecam = mods.locallab.spots.at(i).modecam;
        }

        if (locallab.spots.at(i).modeQJ) {
            toEdit.locallab.spots.at(i).modeQJ = mods.locallab.spots.at(i).modeQJ;
        }

        if (locallab.spots.at(i).bwevMethod12) {
            toEdit.locallab.spots.at(i).bwevMethod12 = mods.locallab.spots.at(i).bwevMethod12;
        }

        if (locallab.spots.at(i).bwevMethod) {
            toEdit.locallab.spots.at(i).bwevMethod = mods.locallab.spots.at(i).bwevMethod;
        }

        if (locallab.spots.at(i).modecie) {
            toEdit.locallab.spots.at(i).modecie = mods.locallab.spots.at(i).modecie;
        }

        if (locallab.spots.at(i).saturlcie) {
            toEdit.locallab.spots.at(i).saturlcie = mods.locallab.spots.at(i).saturlcie;
        }

        if (locallab.spots.at(i).rstprotectcie) {
            toEdit.locallab.spots.at(i).rstprotectcie = mods.locallab.spots.at(i).rstprotectcie;
        }

        if (locallab.spots.at(i).chromlcie) {
            toEdit.locallab.spots.at(i).chromlcie = mods.locallab.spots.at(i).chromlcie;
        }

        if (locallab.spots.at(i).huecie) {
            toEdit.locallab.spots.at(i).huecie = mods.locallab.spots.at(i).huecie;
        }

        if (locallab.spots.at(i).toneMethodcie) {
            toEdit.locallab.spots.at(i).toneMethodcie = mods.locallab.spots.at(i).toneMethodcie;
        }

        if (locallab.spots.at(i).toneMethodcie2) {
            toEdit.locallab.spots.at(i).toneMethodcie2 = mods.locallab.spots.at(i).toneMethodcie2;
        }

        if (locallab.spots.at(i).chromjzcie) {
            toEdit.locallab.spots.at(i).chromjzcie = mods.locallab.spots.at(i).chromjzcie;
        }

        if (locallab.spots.at(i).saturjzcie) {
            toEdit.locallab.spots.at(i).saturjzcie = mods.locallab.spots.at(i).saturjzcie;
        }

        if (locallab.spots.at(i).huejzcie) {
            toEdit.locallab.spots.at(i).huejzcie = mods.locallab.spots.at(i).huejzcie;
        }

        if (locallab.spots.at(i).softjzcie) {
            toEdit.locallab.spots.at(i).softjzcie = mods.locallab.spots.at(i).softjzcie;
        }

        if (locallab.spots.at(i).strsoftjzcie) {
            toEdit.locallab.spots.at(i).strsoftjzcie = mods.locallab.spots.at(i).strsoftjzcie;
        }

        if (locallab.spots.at(i).thrhjzcie) {
            toEdit.locallab.spots.at(i).thrhjzcie = mods.locallab.spots.at(i).thrhjzcie;
        }

        if (locallab.spots.at(i).ciecurve) {
            toEdit.locallab.spots.at(i).ciecurve = mods.locallab.spots.at(i).ciecurve;
        }

        if (locallab.spots.at(i).ciecurve2) {
            toEdit.locallab.spots.at(i).ciecurve2 = mods.locallab.spots.at(i).ciecurve2;
        }

        if (locallab.spots.at(i).jzcurve) {
            toEdit.locallab.spots.at(i).jzcurve = mods.locallab.spots.at(i).jzcurve;
        }

        if (locallab.spots.at(i).czjzcurve) {
            toEdit.locallab.spots.at(i).czjzcurve = mods.locallab.spots.at(i).czjzcurve;
        }

        if (locallab.spots.at(i).invcurve) {
            toEdit.locallab.spots.at(i).invcurve = mods.locallab.spots.at(i).invcurve;
        }

        if (locallab.spots.at(i).HHcurvejz) {
            toEdit.locallab.spots.at(i).HHcurvejz = mods.locallab.spots.at(i).HHcurvejz;
        }

        if (locallab.spots.at(i).CHcurvejz) {
            toEdit.locallab.spots.at(i).CHcurvejz = mods.locallab.spots.at(i).CHcurvejz;
        }

        if (locallab.spots.at(i).LHcurvejz) {
            toEdit.locallab.spots.at(i).LHcurvejz = mods.locallab.spots.at(i).LHcurvejz;
        }

        if (locallab.spots.at(i).lightlcie) {
            toEdit.locallab.spots.at(i).lightlcie = mods.locallab.spots.at(i).lightlcie;
        }

        if (locallab.spots.at(i).lightjzcie) {
            toEdit.locallab.spots.at(i).lightjzcie = mods.locallab.spots.at(i).lightjzcie;
        }

        if (locallab.spots.at(i).lightqcie) {
            toEdit.locallab.spots.at(i).lightqcie = mods.locallab.spots.at(i).lightqcie;
        }

        if (locallab.spots.at(i).lightsigqcie) {
            toEdit.locallab.spots.at(i).lightsigqcie = mods.locallab.spots.at(i).lightsigqcie;
        }

        if (locallab.spots.at(i).contlcie) {
            toEdit.locallab.spots.at(i).contlcie = mods.locallab.spots.at(i).contlcie;
        }

        if (locallab.spots.at(i).contjzcie) {
            toEdit.locallab.spots.at(i).contjzcie = mods.locallab.spots.at(i).contjzcie;
        }

        if (locallab.spots.at(i).detailciejz) {
            toEdit.locallab.spots.at(i).detailciejz = mods.locallab.spots.at(i).detailciejz;
        }

        if (locallab.spots.at(i).adapjzcie) {
            toEdit.locallab.spots.at(i).adapjzcie = mods.locallab.spots.at(i).adapjzcie;
        }

        if (locallab.spots.at(i).jz100) {
            toEdit.locallab.spots.at(i).jz100 = mods.locallab.spots.at(i).jz100;
        }

        if (locallab.spots.at(i).pqremap) {
            toEdit.locallab.spots.at(i).pqremap = mods.locallab.spots.at(i).pqremap;
        }

        if (locallab.spots.at(i).pqremapcam16) {
            toEdit.locallab.spots.at(i).pqremapcam16 = mods.locallab.spots.at(i).pqremapcam16;
        }

        if (locallab.spots.at(i).hljzcie) {
            toEdit.locallab.spots.at(i).hljzcie = mods.locallab.spots.at(i).hljzcie;
        }

        if (locallab.spots.at(i).hlthjzcie) {
            toEdit.locallab.spots.at(i).hlthjzcie = mods.locallab.spots.at(i).hlthjzcie;
        }

        if (locallab.spots.at(i).shjzcie) {
            toEdit.locallab.spots.at(i).shjzcie = mods.locallab.spots.at(i).shjzcie;
        }

        if (locallab.spots.at(i).shthjzcie) {
            toEdit.locallab.spots.at(i).shthjzcie = mods.locallab.spots.at(i).shthjzcie;
        }

        if (locallab.spots.at(i).radjzcie) {
            toEdit.locallab.spots.at(i).radjzcie = mods.locallab.spots.at(i).radjzcie;
        }

        if (locallab.spots.at(i).contthrescie) {
            toEdit.locallab.spots.at(i).contthrescie = mods.locallab.spots.at(i).contthrescie;
        }

        if (locallab.spots.at(i).blackEvjz) {
            toEdit.locallab.spots.at(i).blackEvjz = mods.locallab.spots.at(i).blackEvjz;
        }

        if (locallab.spots.at(i).whiteEvjz) {
            toEdit.locallab.spots.at(i).whiteEvjz = mods.locallab.spots.at(i).whiteEvjz;
        }

        if (locallab.spots.at(i).targetjz) {
            toEdit.locallab.spots.at(i).targetjz = mods.locallab.spots.at(i).targetjz;
        }

        if (locallab.spots.at(i).sigmoidldacie12) {
            toEdit.locallab.spots.at(i).sigmoidldacie12 = mods.locallab.spots.at(i).sigmoidldacie12;
        }

        if (locallab.spots.at(i).sigmoidthcie12) {
            toEdit.locallab.spots.at(i).sigmoidthcie12 = mods.locallab.spots.at(i).sigmoidthcie12;
        }

        if (locallab.spots.at(i).sigmoidblcie12) {
            toEdit.locallab.spots.at(i).sigmoidblcie12 = mods.locallab.spots.at(i).sigmoidblcie12;
        }


        if (locallab.spots.at(i).sigmoidldacie) {
            toEdit.locallab.spots.at(i).sigmoidldacie = mods.locallab.spots.at(i).sigmoidldacie;
        }

        if (locallab.spots.at(i).sigmoidthcie) {
            toEdit.locallab.spots.at(i).sigmoidthcie = mods.locallab.spots.at(i).sigmoidthcie;
        }

        if (locallab.spots.at(i).sigmoidsenscie) {
            toEdit.locallab.spots.at(i).sigmoidsenscie = mods.locallab.spots.at(i).sigmoidsenscie;
        }

        if (locallab.spots.at(i).sigmoidblcie) {
            toEdit.locallab.spots.at(i).sigmoidblcie = mods.locallab.spots.at(i).sigmoidblcie;
        }

        if (locallab.spots.at(i).comprcie) {
            toEdit.locallab.spots.at(i).comprcie = mods.locallab.spots.at(i).comprcie;
        }

        if (locallab.spots.at(i).strcielog) {
            toEdit.locallab.spots.at(i).strcielog = mods.locallab.spots.at(i).strcielog;
        }

        if (locallab.spots.at(i).comprcieth) {
            toEdit.locallab.spots.at(i).comprcieth = mods.locallab.spots.at(i).comprcieth;
        }

        if (locallab.spots.at(i).gamjcie) {
            toEdit.locallab.spots.at(i).gamjcie = mods.locallab.spots.at(i).gamjcie;
        }

        if (locallab.spots.at(i).smoothcieth) {
            toEdit.locallab.spots.at(i).smoothcieth = mods.locallab.spots.at(i).smoothcieth;
        }

        if (locallab.spots.at(i).smoothciethtrc) {
            toEdit.locallab.spots.at(i).smoothciethtrc = mods.locallab.spots.at(i).smoothciethtrc;
        }

        if (locallab.spots.at(i).slopjcie) {
            toEdit.locallab.spots.at(i).slopjcie = mods.locallab.spots.at(i).slopjcie;
        }

        if (locallab.spots.at(i).satjcie) {
            toEdit.locallab.spots.at(i).satjcie = mods.locallab.spots.at(i).satjcie;
        }

        if (locallab.spots.at(i).contsig) {
            toEdit.locallab.spots.at(i).contsig = mods.locallab.spots.at(i).contsig;
        }

        if (locallab.spots.at(i).skewsig) {
            toEdit.locallab.spots.at(i).skewsig = mods.locallab.spots.at(i).skewsig;
        }

        if (locallab.spots.at(i).whitsig) {
            toEdit.locallab.spots.at(i).whitsig = mods.locallab.spots.at(i).whitsig;
        }

        if (locallab.spots.at(i).slopesmo) {
            toEdit.locallab.spots.at(i).slopesmo = mods.locallab.spots.at(i).slopesmo;
        }

        if (locallab.spots.at(i).slopesmoq) {
            toEdit.locallab.spots.at(i).slopesmoq = mods.locallab.spots.at(i).slopesmoq;
        }

        if (locallab.spots.at(i).slopesmor) {
            toEdit.locallab.spots.at(i).slopesmor = mods.locallab.spots.at(i).slopesmor;
        }

        if (locallab.spots.at(i).slopesmog) {
            toEdit.locallab.spots.at(i).slopesmog = mods.locallab.spots.at(i).slopesmog;
        }

        if (locallab.spots.at(i).slopesmob) {
            toEdit.locallab.spots.at(i).slopesmob = mods.locallab.spots.at(i).slopesmob;
        }

        if (locallab.spots.at(i).kslopesmor) {
            toEdit.locallab.spots.at(i).kslopesmor = mods.locallab.spots.at(i).kslopesmor;
        }

        if (locallab.spots.at(i).kslopesmog) {
            toEdit.locallab.spots.at(i).kslopesmog = mods.locallab.spots.at(i).kslopesmog;
        }

        if (locallab.spots.at(i).kslopesmob) {
            toEdit.locallab.spots.at(i).kslopesmob = mods.locallab.spots.at(i).kslopesmob;
        }

        if (locallab.spots.at(i).midtciemet) {
            toEdit.locallab.spots.at(i).midtciemet = mods.locallab.spots.at(i).midtciemet;
        }

        if (locallab.spots.at(i).midtcie) {
            toEdit.locallab.spots.at(i).midtcie = mods.locallab.spots.at(i).midtcie;
        }

        if (locallab.spots.at(i).grexl) {
            toEdit.locallab.spots.at(i).grexl = mods.locallab.spots.at(i).grexl;
        }

        if (locallab.spots.at(i).greyl) {
            toEdit.locallab.spots.at(i).greyl = mods.locallab.spots.at(i).greyl;
        }

        if (locallab.spots.at(i).redxl) {
            toEdit.locallab.spots.at(i).redxl = mods.locallab.spots.at(i).redxl;
        }

        if (locallab.spots.at(i).redyl) {
            toEdit.locallab.spots.at(i).redyl = mods.locallab.spots.at(i).redyl;
        }

        if (locallab.spots.at(i).bluxl) {
            toEdit.locallab.spots.at(i).bluxl = mods.locallab.spots.at(i).bluxl;
        }

        if (locallab.spots.at(i).bluyl) {
            toEdit.locallab.spots.at(i).bluyl = mods.locallab.spots.at(i).bluyl;
        }

        if (locallab.spots.at(i).refi) {
            toEdit.locallab.spots.at(i).refi = mods.locallab.spots.at(i).refi;
        }

        if (locallab.spots.at(i).shiftxl) {
            toEdit.locallab.spots.at(i).shiftxl = mods.locallab.spots.at(i).shiftxl;
        }

        if (locallab.spots.at(i).shiftyl) {
            toEdit.locallab.spots.at(i).shiftyl = mods.locallab.spots.at(i).shiftyl;
        }

        if (locallab.spots.at(i).labgridcieALow) {
            toEdit.locallab.spots.at(i).labgridcieALow = mods.locallab.spots.at(i).labgridcieALow;
        }

        if (locallab.spots.at(i).labgridcieBLow) {
            toEdit.locallab.spots.at(i).labgridcieBLow = mods.locallab.spots.at(i).labgridcieBLow;
        }

        if (locallab.spots.at(i).labgridcieAHigh) {
            toEdit.locallab.spots.at(i).labgridcieAHigh = mods.locallab.spots.at(i).labgridcieAHigh;
        }

        if (locallab.spots.at(i).labgridcieBHigh) {
            toEdit.locallab.spots.at(i).labgridcieBHigh = mods.locallab.spots.at(i).labgridcieBHigh;
        }

        if (locallab.spots.at(i).labgridcieGx) {
            toEdit.locallab.spots.at(i).labgridcieGx = mods.locallab.spots.at(i).labgridcieGx;
        }

        if (locallab.spots.at(i).labgridcieGy) {
            toEdit.locallab.spots.at(i).labgridcieGy = mods.locallab.spots.at(i).labgridcieGy;
        }

        if (locallab.spots.at(i).labgridcieWx) {
            toEdit.locallab.spots.at(i).labgridcieWx = mods.locallab.spots.at(i).labgridcieWx;
        }

        if (locallab.spots.at(i).labgridcieWy) {
            toEdit.locallab.spots.at(i).labgridcieWy = mods.locallab.spots.at(i).labgridcieWy;
        }

        if (locallab.spots.at(i).labgridcieMx) {
            toEdit.locallab.spots.at(i).labgridcieMx = mods.locallab.spots.at(i).labgridcieMx;
        }

        if (locallab.spots.at(i).labgridcieMy) {
            toEdit.locallab.spots.at(i).labgridcieMy = mods.locallab.spots.at(i).labgridcieMy;
        }

        if (locallab.spots.at(i).whitescie) {
            toEdit.locallab.spots.at(i).whitescie = mods.locallab.spots.at(i).whitescie;
        }

        if (locallab.spots.at(i).blackscie) {
            toEdit.locallab.spots.at(i).blackscie = mods.locallab.spots.at(i).blackscie;
        }

        if (locallab.spots.at(i).illMethod) {
            toEdit.locallab.spots.at(i).illMethod = mods.locallab.spots.at(i).illMethod;
        }

        if (locallab.spots.at(i).smoothciemet) {
            toEdit.locallab.spots.at(i).smoothciemet = mods.locallab.spots.at(i).smoothciemet;
        }

        if (locallab.spots.at(i).primMethod) {
            toEdit.locallab.spots.at(i).primMethod = mods.locallab.spots.at(i).primMethod;
        }

        if (locallab.spots.at(i).catMethod) {
            toEdit.locallab.spots.at(i).catMethod = mods.locallab.spots.at(i).catMethod;
        }

        if (locallab.spots.at(i).sigmoidldajzcie12) {
            toEdit.locallab.spots.at(i).sigmoidldajzcie12 = mods.locallab.spots.at(i).sigmoidldajzcie12;
        }

        if (locallab.spots.at(i).sigmoidthjzcie12) {
            toEdit.locallab.spots.at(i).sigmoidthjzcie12 = mods.locallab.spots.at(i).sigmoidthjzcie12;
        }

        if (locallab.spots.at(i).sigmoidbljzcie12) {
            toEdit.locallab.spots.at(i).sigmoidbljzcie12 = mods.locallab.spots.at(i).sigmoidbljzcie12;
        }

        if (locallab.spots.at(i).sigmoidldajzcie) {
            toEdit.locallab.spots.at(i).sigmoidldajzcie = mods.locallab.spots.at(i).sigmoidldajzcie;
        }

        if (locallab.spots.at(i).sigmoidthjzcie) {
            toEdit.locallab.spots.at(i).sigmoidthjzcie = mods.locallab.spots.at(i).sigmoidthjzcie;
        }

        if (locallab.spots.at(i).sigmoidbljzcie) {
            toEdit.locallab.spots.at(i).sigmoidbljzcie = mods.locallab.spots.at(i).sigmoidbljzcie;
        }


        if (locallab.spots.at(i).contqcie) {
            toEdit.locallab.spots.at(i).contqcie = mods.locallab.spots.at(i).contqcie;
        }

        if (locallab.spots.at(i).contsigqcie) {
            toEdit.locallab.spots.at(i).contsigqcie = mods.locallab.spots.at(i).contsigqcie;
        }

        if (locallab.spots.at(i).colorflcie) {
            toEdit.locallab.spots.at(i).colorflcie = mods.locallab.spots.at(i).colorflcie;
        }
        if (locallab.spots.at(i).targabscie) {
            toEdit.locallab.spots.at(i).targabscie = mods.locallab.spots.at(i).targabscie;
        }

        if (locallab.spots.at(i).targetGraycie) {
            toEdit.locallab.spots.at(i).targetGraycie = mods.locallab.spots.at(i).targetGraycie;
        }

        if (locallab.spots.at(i).catadcie) {
            toEdit.locallab.spots.at(i).catadcie = mods.locallab.spots.at(i).catadcie;
        }

        if (locallab.spots.at(i).locwavcurvejz) {
            toEdit.locallab.spots.at(i).locwavcurvejz = mods.locallab.spots.at(i).locwavcurvejz;
        }

        if (locallab.spots.at(i).csthresholdjz) {
            toEdit.locallab.spots.at(i).csthresholdjz = mods.locallab.spots.at(i).csthresholdjz;
        }

        if (locallab.spots.at(i).sigmalcjz) {
            toEdit.locallab.spots.at(i).sigmalcjz = mods.locallab.spots.at(i).sigmalcjz;
        }

        if (locallab.spots.at(i).clarilresjz) {
            toEdit.locallab.spots.at(i).clarilresjz = mods.locallab.spots.at(i).clarilresjz;
        }

        if (locallab.spots.at(i).claricresjz) {
            toEdit.locallab.spots.at(i).claricresjz = mods.locallab.spots.at(i).claricresjz;
        }

        if (locallab.spots.at(i).clarisoftjz) {
            toEdit.locallab.spots.at(i).clarisoftjz = mods.locallab.spots.at(i).clarisoftjz;
        }

        if (locallab.spots.at(i).detailcie) {
            toEdit.locallab.spots.at(i).detailcie = mods.locallab.spots.at(i).detailcie;
        }

        if (locallab.spots.at(i).surroundcie) {
            toEdit.locallab.spots.at(i).surroundcie = mods.locallab.spots.at(i).surroundcie;
        }

        if (locallab.spots.at(i).strgradcie) {
            toEdit.locallab.spots.at(i).strgradcie = mods.locallab.spots.at(i).strgradcie;
        }

        if (locallab.spots.at(i).anggradcie) {
            toEdit.locallab.spots.at(i).anggradcie = mods.locallab.spots.at(i).anggradcie;
        }

        if (locallab.spots.at(i).feathercie) {
            toEdit.locallab.spots.at(i).feathercie = mods.locallab.spots.at(i).feathercie;
        }

        if (locallab.spots.at(i).enacieMask) {
            toEdit.locallab.spots.at(i).enacieMask = mods.locallab.spots.at(i).enacieMask;
        }

        if (locallab.spots.at(i).enacieMaskall) {
            toEdit.locallab.spots.at(i).enacieMaskall = mods.locallab.spots.at(i).enacieMaskall;
        }

        if (locallab.spots.at(i).CCmaskciecurve) {
            toEdit.locallab.spots.at(i).CCmaskciecurve = mods.locallab.spots.at(i).CCmaskciecurve;
        }

        if (locallab.spots.at(i).LLmaskciecurve) {
            toEdit.locallab.spots.at(i).LLmaskciecurve = mods.locallab.spots.at(i).LLmaskciecurve;
        }

        if (locallab.spots.at(i).HHmaskciecurve) {
            toEdit.locallab.spots.at(i).HHmaskciecurve = mods.locallab.spots.at(i).HHmaskciecurve;
        }

        if (locallab.spots.at(i).HHhmaskciecurve) {
            toEdit.locallab.spots.at(i).HHhmaskciecurve = mods.locallab.spots.at(i).HHhmaskciecurve;
        }

        if (locallab.spots.at(i).blendmaskcie) {
            toEdit.locallab.spots.at(i).blendmaskcie = mods.locallab.spots.at(i).blendmaskcie;
        }

        if (locallab.spots.at(i).radmaskcie) {
            toEdit.locallab.spots.at(i).radmaskcie = mods.locallab.spots.at(i).radmaskcie;
        }

        if (locallab.spots.at(i).chromaskcie) {
            toEdit.locallab.spots.at(i).chromaskcie = mods.locallab.spots.at(i).chromaskcie;
        }

        if (locallab.spots.at(i).lapmaskcie) {
            toEdit.locallab.spots.at(i).lapmaskcie = mods.locallab.spots.at(i).lapmaskcie;
        }

        if (locallab.spots.at(i).gammaskcie) {
            toEdit.locallab.spots.at(i).gammaskcie = mods.locallab.spots.at(i).gammaskcie;
        }

        if (locallab.spots.at(i).slomaskcie) {
            toEdit.locallab.spots.at(i).slomaskcie = mods.locallab.spots.at(i).slomaskcie;
        }

        if (locallab.spots.at(i).Lmaskciecurve) {
            toEdit.locallab.spots.at(i).Lmaskciecurve = mods.locallab.spots.at(i).Lmaskciecurve;
        }

        if (locallab.spots.at(i).recothrescie) {
            toEdit.locallab.spots.at(i).recothrescie = mods.locallab.spots.at(i).recothrescie;
        }

        if (locallab.spots.at(i).lowthrescie) {
            toEdit.locallab.spots.at(i).lowthrescie = mods.locallab.spots.at(i).lowthrescie;
        }

        if (locallab.spots.at(i).higthrescie) {
            toEdit.locallab.spots.at(i).higthrescie = mods.locallab.spots.at(i).higthrescie;
        }

        if (locallab.spots.at(i).decaycie) {
            toEdit.locallab.spots.at(i).decaycie = mods.locallab.spots.at(i).decaycie;
        }

        if (locallab.spots.at(i).strumaskcie) {
            toEdit.locallab.spots.at(i).strumaskcie= mods.locallab.spots.at(i).strumaskcie;
        }

        if (locallab.spots.at(i).toolcie) {
            toEdit.locallab.spots.at(i).toolcie = mods.locallab.spots.at(i).toolcie;
        }

        if (locallab.spots.at(i).fftcieMask) {
            toEdit.locallab.spots.at(i).fftcieMask = mods.locallab.spots.at(i).fftcieMask;
        }

        if (locallab.spots.at(i).blurcie) {
            toEdit.locallab.spots.at(i).blurcie = mods.locallab.spots.at(i).blurcie;
        }

        if (locallab.spots.at(i).contcie) {
            toEdit.locallab.spots.at(i).contcie = mods.locallab.spots.at(i).contcie;
        }

        if (locallab.spots.at(i).highmaskcie) {
            toEdit.locallab.spots.at(i).highmaskcie = mods.locallab.spots.at(i).highmaskcie;
        }

        if (locallab.spots.at(i).shadmaskcie) {
            toEdit.locallab.spots.at(i).shadmaskcie = mods.locallab.spots.at(i).shadmaskcie;
        }
 
		if (locallab.spots.at(i).LLmaskciecurvewav) {
            toEdit.locallab.spots.at(i).LLmaskciecurvewav = mods.locallab.spots.at(i).LLmaskciecurvewav;
        }

        if (locallab.spots.at(i).csthresholdcie) {
            toEdit.locallab.spots.at(i).csthresholdcie = mods.locallab.spots.at(i).csthresholdcie;
        }

    }

    if (spot.enabled) {
        toEdit.spot.enabled   = mods.spot.enabled;
    }

    if (spot.entries) {
        toEdit.spot.entries   = mods.spot.entries;
    }

    if (pcvignette.enabled) {
        toEdit.pcvignette.enabled = mods.pcvignette.enabled;
    }

    if (pcvignette.strength) {
        toEdit.pcvignette.strength = dontforceSet && options.baBehav[ADDSET_PCVIGNETTE_STRENGTH] ? toEdit.pcvignette.strength + mods.pcvignette.strength : mods.pcvignette.strength;
    }

    if (pcvignette.feather) {
        toEdit.pcvignette.feather = dontforceSet && options.baBehav[ADDSET_PCVIGNETTE_FEATHER] ? toEdit.pcvignette.feather + mods.pcvignette.feather : mods.pcvignette.feather;
    }

    if (pcvignette.roundness) {
        toEdit.pcvignette.roundness = dontforceSet && options.baBehav[ADDSET_PCVIGNETTE_ROUNDNESS] ? toEdit.pcvignette.roundness + mods.pcvignette.roundness : mods.pcvignette.roundness;
    }

    if (cacorrection.red) {
        toEdit.cacorrection.red = dontforceSet && options.baBehav[ADDSET_CA] ? toEdit.cacorrection.red + mods.cacorrection.red : mods.cacorrection.red;
    }

    if (cacorrection.blue) {
        toEdit.cacorrection.blue = dontforceSet && options.baBehav[ADDSET_CA] ? toEdit.cacorrection.blue + mods.cacorrection.blue : mods.cacorrection.blue;
    }

    if (vignetting.amount) {
        toEdit.vignetting.amount = dontforceSet && options.baBehav[ADDSET_VIGN_AMOUNT] ? toEdit.vignetting.amount + mods.vignetting.amount : mods.vignetting.amount;
    }

    if (vignetting.radius) {
        toEdit.vignetting.radius = dontforceSet && options.baBehav[ADDSET_VIGN_RADIUS] ? toEdit.vignetting.radius + mods.vignetting.radius : mods.vignetting.radius;
    }

    if (vignetting.strength) {
        toEdit.vignetting.strength = dontforceSet && options.baBehav[ADDSET_VIGN_STRENGTH] ? toEdit.vignetting.strength + mods.vignetting.strength : mods.vignetting.strength;
    }

    if (vignetting.centerX) {
        toEdit.vignetting.centerX = dontforceSet && options.baBehav[ADDSET_VIGN_CENTER] ? toEdit.vignetting.centerX + mods.vignetting.centerX : mods.vignetting.centerX;
    }

    if (vignetting.centerY) {
        toEdit.vignetting.centerY = dontforceSet && options.baBehav[ADDSET_VIGN_CENTER] ? toEdit.vignetting.centerY + mods.vignetting.centerY : mods.vignetting.centerY;
    }

    if (chmixer.enabled) {
        toEdit.chmixer.enabled = mods.chmixer.enabled;
    }

    for (int i = 0; i < 3; i++) {
        if (chmixer.red[i]) {
            toEdit.chmixer.red[i] = dontforceSet && options.baBehav[ADDSET_CHMIXER] ? toEdit.chmixer.red[i] + mods.chmixer.red[i] : mods.chmixer.red[i];
        }

        if (chmixer.green[i]) {
            toEdit.chmixer.green[i] = dontforceSet && options.baBehav[ADDSET_CHMIXER] ? toEdit.chmixer.green[i] + mods.chmixer.green[i] : mods.chmixer.green[i];
        }

        if (chmixer.blue[i]) {
            toEdit.chmixer.blue[i] = dontforceSet && options.baBehav[ADDSET_CHMIXER] ? toEdit.chmixer.blue[i] + mods.chmixer.blue[i] : mods.chmixer.blue[i];
        }
    }

    if (blackwhite.enabled) {
        toEdit.blackwhite.enabled = mods.blackwhite.enabled;
    }

    if (blackwhite.method) {
        toEdit.blackwhite.method = mods.blackwhite.method;
    }

    if (blackwhite.luminanceCurve) {
        toEdit.blackwhite.luminanceCurve = mods.blackwhite.luminanceCurve;
    }

    if (blackwhite.autoc) {
        toEdit.blackwhite.autoc = mods.blackwhite.autoc;
    }

    if (blackwhite.setting) {
        toEdit.blackwhite.setting = mods.blackwhite.setting;
    }

    if (blackwhite.enabledcc) {
        toEdit.blackwhite.enabledcc = mods.blackwhite.enabledcc;
    }

    if (blackwhite.filter) {
        toEdit.blackwhite.filter = mods.blackwhite.filter;
    }

    if (blackwhite.mixerRed) {
        toEdit.blackwhite.mixerRed = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerRed + mods.blackwhite.mixerRed : mods.blackwhite.mixerRed;
    }

    if (blackwhite.mixerOrange) {
        toEdit.blackwhite.mixerOrange = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerOrange + mods.blackwhite.mixerOrange : mods.blackwhite.mixerOrange;
    }

    if (blackwhite.mixerYellow) {
        toEdit.blackwhite.mixerYellow = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerYellow + mods.blackwhite.mixerYellow : mods.blackwhite.mixerYellow;
    }

    if (blackwhite.mixerGreen) {
        toEdit.blackwhite.mixerGreen = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerGreen + mods.blackwhite.mixerGreen : mods.blackwhite.mixerGreen;
    }

    if (blackwhite.mixerCyan) {
        toEdit.blackwhite.mixerCyan = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerCyan + mods.blackwhite.mixerCyan : mods.blackwhite.mixerCyan;
    }

    if (blackwhite.mixerBlue) {
        toEdit.blackwhite.mixerBlue = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerBlue + mods.blackwhite.mixerBlue : mods.blackwhite.mixerBlue;
    }

    if (blackwhite.mixerMagenta) {
        toEdit.blackwhite.mixerMagenta = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerMagenta + mods.blackwhite.mixerMagenta : mods.blackwhite.mixerMagenta;
    }

    if (blackwhite.mixerPurple) {
        toEdit.blackwhite.mixerPurple = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_HUES] ? toEdit.blackwhite.mixerPurple + mods.blackwhite.mixerPurple : mods.blackwhite.mixerPurple;
    }

    if (blackwhite.gammaRed) {
        toEdit.blackwhite.gammaRed = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_GAMMA] ? toEdit.blackwhite.gammaRed + mods.blackwhite.gammaRed : mods.blackwhite.gammaRed;
    }

    if (blackwhite.gammaGreen) {
        toEdit.blackwhite.gammaGreen = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_GAMMA] ? toEdit.blackwhite.gammaGreen + mods.blackwhite.gammaGreen : mods.blackwhite.gammaGreen;
    }

    if (blackwhite.gammaBlue) {
        toEdit.blackwhite.gammaBlue = dontforceSet && options.baBehav[ADDSET_BLACKWHITE_GAMMA] ? toEdit.blackwhite.gammaBlue + mods.blackwhite.gammaBlue : mods.blackwhite.gammaBlue;
    }

    if (blackwhite.beforeCurve) {
        toEdit.blackwhite.beforeCurve = mods.blackwhite.beforeCurve;
    }

    if (blackwhite.beforeCurveMode) {
        toEdit.blackwhite.beforeCurveMode = mods.blackwhite.beforeCurveMode;
    }

    if (blackwhite.afterCurve) {
        toEdit.blackwhite.afterCurve = mods.blackwhite.afterCurve;
    }

    if (blackwhite.afterCurveMode) {
        toEdit.blackwhite.afterCurveMode = mods.blackwhite.afterCurveMode;
    }

    if (blackwhite.algo) {
        toEdit.blackwhite.algo = mods.blackwhite.algo;
    }

    if (resize.scale) {
        toEdit.resize.scale = dontforceSet && options.baBehav[ADDSET_RESIZE_SCALE] ? toEdit.resize.scale + mods.resize.scale : mods.resize.scale;
    }

    if (resize.appliesTo) {
        toEdit.resize.appliesTo = mods.resize.appliesTo;
    }

    if (resize.method) {
        toEdit.resize.method = mods.resize.method;
    }

    if (resize.dataspec) {
        toEdit.resize.dataspec = mods.resize.dataspec;
    }

    if (resize.width) {
        toEdit.resize.width = mods.resize.width;
    }

    if (resize.height) {
        toEdit.resize.height = mods.resize.height;
    }

    if (resize.longedge) {
        toEdit.resize.longedge = mods.resize.longedge;
    }

    if (resize.shortedge) {
        toEdit.resize.shortedge = mods.resize.shortedge;
    }

    if (resize.enabled) {
        toEdit.resize.enabled = mods.resize.enabled;
    }

    if (resize.allowUpscaling) {
        toEdit.resize.allowUpscaling = mods.resize.allowUpscaling;
    }

    ::combine(toEdit.framing, mods.framing, framing, dontforceSet);

    if (icm.inputProfile) {
        toEdit.icm.inputProfile = mods.icm.inputProfile;
    }

    if (icm.toneCurve) {
        toEdit.icm.toneCurve = mods.icm.toneCurve;
    }

    if (icm.applyLookTable) {
        toEdit.icm.applyLookTable = mods.icm.applyLookTable;
    }

    if (icm.applyBaselineExposureOffset) {
        toEdit.icm.applyBaselineExposureOffset = mods.icm.applyBaselineExposureOffset;
    }

    if (icm.applyHueSatMap) {
        toEdit.icm.applyHueSatMap = mods.icm.applyHueSatMap;
    }

    if (icm.dcpIlluminant) {
        toEdit.icm.dcpIlluminant = mods.icm.dcpIlluminant;
    }

    if (icm.workingProfile) {
        toEdit.icm.workingProfile = mods.icm.workingProfile;
    }

    if (icm.outputProfile) {
        toEdit.icm.outputProfile = mods.icm.outputProfile;
    }

    if (icm.outputIntent) {
        toEdit.icm.outputIntent = mods.icm.outputIntent;
    }

    if (icm.outputBPC) {
        toEdit.icm.outputBPC = mods.icm.outputBPC;
    }

    if (icm.wGamma) {
        toEdit.icm.wGamma = mods.icm.wGamma;
    }

    if (icm.wSlope) {
        toEdit.icm.wSlope = mods.icm.wSlope;
    }

    if (icm.wmidtcie) {
        toEdit.icm.wmidtcie = mods.icm.wmidtcie;
    }

    if (icm.sigmatrc) {
        toEdit.icm.sigmatrc = mods.icm.sigmatrc;
    }

    if (icm.offstrc) {
        toEdit.icm.offstrc = mods.icm.offstrc;
    }

    if (icm.wgampower) {
        toEdit.icm.wgampower = mods.icm.wgampower;
    }

    if (icm.wgamgain) {
        toEdit.icm.wgamgain = mods.icm.wgamgain;
    }

    if (icm.residtrc) {
        toEdit.icm.residtrc = mods.icm.residtrc;
    }

    if (icm.pyrwavtrc) {
        toEdit.icm.pyrwavtrc = mods.icm.pyrwavtrc;
    }

    if (icm.opacityCurveWLI) {
        toEdit.icm.opacityCurveWLI = mods.icm.opacityCurveWLI;
    }

    if (icm.wsmoothcie) {
        toEdit.icm.wsmoothcie = mods.icm.wsmoothcie;
    }

    if (icm.wapsat) {
        toEdit.icm.wapsat = mods.icm.wapsat;
    }

    if (icm.wsmoothciesli) {
        toEdit.icm.wsmoothciesli = mods.icm.wsmoothciesli;
    }

    if (icm.redx) {
        toEdit.icm.redx = mods.icm.redx;
    }

    if (icm.redy) {
        toEdit.icm.redy = mods.icm.redy;
    }

    if (icm.grex) {
        toEdit.icm.grex = mods.icm.grex;
    }

    if (icm.grey) {
        toEdit.icm.grey = mods.icm.grey;
    }

    if (icm.blux) {
        toEdit.icm.blux = mods.icm.blux;
    }

    if (icm.bluy) {
        toEdit.icm.bluy = mods.icm.bluy;
    }

    if (icm.redrot) {
        toEdit.icm.redrot = mods.icm.redrot;
    }

    if (icm.redsat) {
        toEdit.icm.redsat = mods.icm.redsat;
    }

    if (icm.grerot) {
        toEdit.icm.grerot = mods.icm.grerot;
    }

    if (icm.gresat) {
        toEdit.icm.gresat = mods.icm.gresat;
    }

    if (icm.blurot) {
        toEdit.icm.blurot = mods.icm.blurot;
    }

    if (icm.blusat) {
        toEdit.icm.blusat = mods.icm.blusat;
    }


    if (icm.refi) {
        toEdit.icm.refi = mods.icm.refi;
    }

    if (icm.shiftx) {
        toEdit.icm.shiftx = mods.icm.shiftx;
    }

    if (icm.shifty) {
        toEdit.icm.shifty = mods.icm.shifty;
    }

    if (icm.preser) {
        toEdit.icm.preser = mods.icm.preser;
    }

    if (icm.fbw) {
        toEdit.icm.fbw = mods.icm.fbw;
    }

    if (icm.trcExp) {
        toEdit.icm.trcExp = mods.icm.trcExp;
    }

    if (icm.wavExp) {
        toEdit.icm.wavExp = mods.icm.wavExp;
    }

    if (icm.gamut) {
        toEdit.icm.gamut = mods.icm.gamut;
    }

    if (icm.labgridcieALow) {
        toEdit.icm.labgridcieALow = mods.icm.labgridcieALow;
    }

    if (icm.labgridcieBLow) {
        toEdit.icm.labgridcieBLow = mods.icm.labgridcieBLow;
    }

    if (icm.labgridcieAHigh) {
        toEdit.icm.labgridcieAHigh = mods.icm.labgridcieAHigh;
    }

    if (icm.labgridcieBHigh) {
        toEdit.icm.labgridcieBHigh = mods.icm.labgridcieBHigh;
    }

    if (icm.labgridcieGx) {
        toEdit.icm.labgridcieGx = mods.icm.labgridcieGx;
    }

    if (icm.labgridcieGy) {
        toEdit.icm.labgridcieGy = mods.icm.labgridcieGy;
    }

    if (icm.labgridcieWx) {
        toEdit.icm.labgridcieWx = mods.icm.labgridcieWx;
    }

    if (icm.labgridcieWy) {
        toEdit.icm.labgridcieWy = mods.icm.labgridcieWy;
    }

    if (icm.labgridcieMx) {
        toEdit.icm.labgridcieMx = mods.icm.labgridcieMx;
    }

    if (icm.labgridcieMy) {
        toEdit.icm.labgridcieMy = mods.icm.labgridcieMy;
    }

    if (icm.aRendIntent) {
        toEdit.icm.aRendIntent = mods.icm.aRendIntent;
    }

    if (icm.workingTRC) {
        toEdit.icm.workingTRC = mods.icm.workingTRC;
    }

    if (icm.wgamut) {
        toEdit.icm.wgamut = mods.icm.wgamut;
    }

    if (icm.will) {
        toEdit.icm.will = mods.icm.will;
    }

    if (icm.wprim) {
        toEdit.icm.wprim = mods.icm.wprim;
    }

    if (icm.wcat) {
        toEdit.icm.wcat = mods.icm.wcat;
    }

    if (raw.bayersensor.method) {
        toEdit.raw.bayersensor.method = mods.raw.bayersensor.method;
    }

    if (raw.bayersensor.border) {
        toEdit.raw.bayersensor.border = mods.raw.bayersensor.border;
    }

    if (raw.bayersensor.imageNum) {
        toEdit.raw.bayersensor.imageNum = mods.raw.bayersensor.imageNum;
    }

    if (raw.bayersensor.ccSteps) {
        toEdit.raw.bayersensor.ccSteps = mods.raw.bayersensor.ccSteps;
    }

    if (raw.bayersensor.exBlack0) {
        toEdit.raw.bayersensor.black0 = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.bayersensor.black0 + mods.raw.bayersensor.black0 : mods.raw.bayersensor.black0;
    }

    if (raw.bayersensor.exBlack1) {
        toEdit.raw.bayersensor.black1 = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.bayersensor.black1 + mods.raw.bayersensor.black1 : mods.raw.bayersensor.black1;
    }

    if (raw.bayersensor.exBlack2) {
        toEdit.raw.bayersensor.black2 = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.bayersensor.black2 + mods.raw.bayersensor.black2 : mods.raw.bayersensor.black2;
    }

    if (raw.bayersensor.exBlack3) {
        toEdit.raw.bayersensor.black3 = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.bayersensor.black3 + mods.raw.bayersensor.black3 : mods.raw.bayersensor.black3;
    }

    if (raw.bayersensor.exTwoGreen) {
        toEdit.raw.bayersensor.twogreen = mods.raw.bayersensor.twogreen;
    }

    if (raw.bayersensor.Dehablack) {
        toEdit.raw.bayersensor.Dehablack = mods.raw.bayersensor.Dehablack;
    }

    if (raw.bayersensor.dcbIterations) {
        toEdit.raw.bayersensor.dcb_iterations = mods.raw.bayersensor.dcb_iterations;
    }

    if (raw.bayersensor.dcbEnhance) {
        toEdit.raw.bayersensor.dcb_enhance = mods.raw.bayersensor.dcb_enhance;
    }

    if (raw.bayersensor.lmmseIterations) {
        toEdit.raw.bayersensor.lmmse_iterations = mods.raw.bayersensor.lmmse_iterations;
    }

    if (raw.bayersensor.dualDemosaicAutoContrast) {
        toEdit.raw.bayersensor.dualDemosaicAutoContrast = mods.raw.bayersensor.dualDemosaicAutoContrast;
    }

    if (raw.bayersensor.dualDemosaicContrast) {
        toEdit.raw.bayersensor.dualDemosaicContrast = mods.raw.bayersensor.dualDemosaicContrast;
    }

    if (raw.bayersensor.pixelShiftMotionCorrectionMethod) {
        toEdit.raw.bayersensor.pixelShiftMotionCorrectionMethod = mods.raw.bayersensor.pixelShiftMotionCorrectionMethod;
    }

    if (raw.bayersensor.pixelShiftEperIso) {
        toEdit.raw.bayersensor.pixelShiftEperIso = mods.raw.bayersensor.pixelShiftEperIso;
    }

    if (raw.bayersensor.pixelShiftSigma) {
        toEdit.raw.bayersensor.pixelShiftSigma = mods.raw.bayersensor.pixelShiftSigma;
    }

    if (raw.bayersensor.pixelShiftShowMotion) {
        toEdit.raw.bayersensor.pixelShiftShowMotion = mods.raw.bayersensor.pixelShiftShowMotion;
    }

    if (raw.bayersensor.pixelShiftShowMotionMaskOnly) {
        toEdit.raw.bayersensor.pixelShiftShowMotionMaskOnly = mods.raw.bayersensor.pixelShiftShowMotionMaskOnly;
    }

    if (raw.bayersensor.pixelShiftHoleFill) {
        toEdit.raw.bayersensor.pixelShiftHoleFill = mods.raw.bayersensor.pixelShiftHoleFill;
    }

    if (raw.bayersensor.pixelShiftMedian) {
        toEdit.raw.bayersensor.pixelShiftMedian = mods.raw.bayersensor.pixelShiftMedian;
    }

    if (raw.bayersensor.pixelShiftAverage) {
        toEdit.raw.bayersensor.pixelShiftAverage = mods.raw.bayersensor.pixelShiftAverage;
    }

    if (raw.bayersensor.pixelShiftGreen) {
        toEdit.raw.bayersensor.pixelShiftGreen = mods.raw.bayersensor.pixelShiftGreen;
    }

    if (raw.bayersensor.pixelShiftBlur) {
        toEdit.raw.bayersensor.pixelShiftBlur = mods.raw.bayersensor.pixelShiftBlur;
    }

    if (raw.bayersensor.pixelShiftSmooth) {
        toEdit.raw.bayersensor.pixelShiftSmoothFactor = mods.raw.bayersensor.pixelShiftSmoothFactor;
    }

    if (raw.bayersensor.pixelShiftEqualBright) {
        toEdit.raw.bayersensor.pixelShiftEqualBright = mods.raw.bayersensor.pixelShiftEqualBright;
    }

    if (raw.bayersensor.pixelShiftEqualBrightChannel) {
        toEdit.raw.bayersensor.pixelShiftEqualBrightChannel = mods.raw.bayersensor.pixelShiftEqualBrightChannel;
    }

    if (raw.bayersensor.pixelShiftNonGreenCross) {
        toEdit.raw.bayersensor.pixelShiftNonGreenCross = mods.raw.bayersensor.pixelShiftNonGreenCross;
    }

    if (raw.bayersensor.pixelShiftDemosaicMethod) {
        toEdit.raw.bayersensor.pixelShiftDemosaicMethod = mods.raw.bayersensor.pixelShiftDemosaicMethod;
    }

    if (raw.bayersensor.greenEq) {
        toEdit.raw.bayersensor.greenthresh = dontforceSet && options.baBehav[ADDSET_PREPROCESS_GREENEQUIL] ? toEdit.raw.bayersensor.greenthresh + mods.raw.bayersensor.greenthresh : mods.raw.bayersensor.greenthresh;
    }

    if (raw.bayersensor.linenoise) {
        toEdit.raw.bayersensor.linenoise = dontforceSet && options.baBehav[ADDSET_PREPROCESS_LINEDENOISE] ? toEdit.raw.bayersensor.linenoise + mods.raw.bayersensor.linenoise : mods.raw.bayersensor.linenoise;
    }

    if (raw.bayersensor.linenoiseDirection) {
        toEdit.raw.bayersensor.linenoiseDirection = mods.raw.bayersensor.linenoiseDirection;
    }

    if (raw.bayersensor.pdafLinesFilter) {
        toEdit.raw.bayersensor.pdafLinesFilter = mods.raw.bayersensor.pdafLinesFilter;
    }

    if (raw.xtranssensor.method) {
        toEdit.raw.xtranssensor.method = mods.raw.xtranssensor.method;
    }

    if (raw.xtranssensor.dualDemosaicAutoContrast) {
        toEdit.raw.xtranssensor.dualDemosaicAutoContrast = mods.raw.xtranssensor.dualDemosaicAutoContrast;
    }

    if (raw.xtranssensor.dualDemosaicContrast) {
        toEdit.raw.xtranssensor.dualDemosaicContrast = mods.raw.xtranssensor.dualDemosaicContrast;
    }

    if (raw.xtranssensor.ccSteps) {
        toEdit.raw.xtranssensor.ccSteps = mods.raw.xtranssensor.ccSteps;
    }

    if (raw.xtranssensor.border) {
        toEdit.raw.xtranssensor.border = mods.raw.xtranssensor.border;
    }

    if (raw.xtranssensor.exBlackRed) {
        toEdit.raw.xtranssensor.blackred = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.xtranssensor.blackred + mods.raw.xtranssensor.blackred : mods.raw.xtranssensor.blackred;
    }

    if (raw.xtranssensor.exBlackGreen) {
        toEdit.raw.xtranssensor.blackgreen = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.xtranssensor.blackgreen + mods.raw.xtranssensor.blackgreen : mods.raw.xtranssensor.blackgreen;
    }

    if (raw.xtranssensor.exBlackBlue) {
        toEdit.raw.xtranssensor.blackblue = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_BLACKS] ? toEdit.raw.xtranssensor.blackblue + mods.raw.xtranssensor.blackblue : mods.raw.xtranssensor.blackblue;
    }

    if (raw.xtranssensor.Dehablackx) {
        toEdit.raw.xtranssensor.Dehablackx = mods.raw.xtranssensor.Dehablackx;
    }

    if (raw.ca_autocorrect) {
        toEdit.raw.ca_autocorrect = mods.raw.ca_autocorrect;
    }

    if (raw.ca_avoidcolourshift) {
        toEdit.raw.ca_avoidcolourshift = mods.raw.ca_avoidcolourshift;
    }

    if (raw.caautoiterations) {
        toEdit.raw.caautoiterations = dontforceSet && options.baBehav[ADDSET_RAWCACORR] ? toEdit.raw.caautoiterations + mods.raw.caautoiterations : mods.raw.caautoiterations;
    }

    if (raw.cared) {
        toEdit.raw.cared = dontforceSet && options.baBehav[ADDSET_RAWCACORR] ? toEdit.raw.cared + mods.raw.cared : mods.raw.cared;
    }

    if (raw.cablue) {
        toEdit.raw.cablue = dontforceSet && options.baBehav[ADDSET_RAWCACORR] ? toEdit.raw.cablue + mods.raw.cablue : mods.raw.cablue;
    }

    if (raw.exPos) {
        toEdit.raw.expos = dontforceSet && options.baBehav[ADDSET_RAWEXPOS_LINEAR] ? toEdit.raw.expos + mods.raw.expos : mods.raw.expos;
    }

    if (raw.hotPixelFilter) {
        toEdit.raw.hotPixelFilter = mods.raw.hotPixelFilter;
    }

    if (raw.deadPixelFilter) {
        toEdit.raw.deadPixelFilter = mods.raw.deadPixelFilter;
    }

    if (raw.hotdeadpix_thresh) {
        toEdit.raw.hotdeadpix_thresh = mods.raw.hotdeadpix_thresh;
    }

    if (raw.darkFrame) {
        toEdit.raw.dark_frame = mods.raw.dark_frame;
    }

    if (raw.df_autoselect) {
        toEdit.raw.df_autoselect = mods.raw.df_autoselect;
    }

    if (raw.ff_file) {
        toEdit.raw.ff_file = mods.raw.ff_file;
    }

    if (raw.ff_AutoSelect) {
        toEdit.raw.ff_AutoSelect = mods.raw.ff_AutoSelect;
    }

    if (raw.ff_FromMetaData) {
        toEdit.raw.ff_FromMetaData = mods.raw.ff_FromMetaData;
    }

    if (raw.ff_BlurRadius) {
        toEdit.raw.ff_BlurRadius = mods.raw.ff_BlurRadius;
    }

    if (raw.ff_BlurType) {
        toEdit.raw.ff_BlurType = mods.raw.ff_BlurType;
    }

    if (raw.ff_AutoClipControl) {
        toEdit.raw.ff_AutoClipControl = mods.raw.ff_AutoClipControl;
    }

    if (raw.ff_clipControl) {
        toEdit.raw.ff_clipControl = dontforceSet && options.baBehav[ADDSET_RAWFFCLIPCONTROL] ? toEdit.raw.ff_clipControl + mods.raw.ff_clipControl : mods.raw.ff_clipControl;
    }

    if (wavelet.enabled) {
        toEdit.wavelet.enabled = mods.wavelet.enabled;
    }

    if (wavelet.labgridALow) {
        toEdit.wavelet.labgridALow = mods.wavelet.labgridALow;
    }

    if (wavelet.labgridBLow) {
        toEdit.wavelet.labgridBLow = mods.wavelet.labgridBLow;
    }

    if (wavelet.labgridAHigh) {
        toEdit.wavelet.labgridAHigh = mods.wavelet.labgridAHigh;
    }

    if (wavelet.labgridBHigh) {
        toEdit.wavelet.labgridBHigh = mods.wavelet.labgridBHigh;
    }

    if (wavelet.strength) {
        toEdit.wavelet.strength = mods.wavelet.strength;
    }

    if (wavelet.balance) {
        toEdit.wavelet.balance = mods.wavelet.balance;
    }

    if (wavelet.sigmafin) {
        toEdit.wavelet.sigmafin = mods.wavelet.sigmafin;
    }

    if (wavelet.sigmaton) {
        toEdit.wavelet.sigmaton = mods.wavelet.sigmaton;
    }

    if (wavelet.sigmacol) {
        toEdit.wavelet.sigmacol = mods.wavelet.sigmacol;
    }

    if (wavelet.sigmadir) {
        toEdit.wavelet.sigmadir = mods.wavelet.sigmadir;
    }

    if (wavelet.rangeab) {
        toEdit.wavelet.rangeab = mods.wavelet.rangeab;
    }

    if (wavelet.protab) {
        toEdit.wavelet.protab = mods.wavelet.protab;
    }

    if (wavelet.iter) {
        toEdit.wavelet.iter = mods.wavelet.iter;
    }

    if (wavelet.median) {
        toEdit.wavelet.median = mods.wavelet.median;
    }

    if (wavelet.medianlev) {
        toEdit.wavelet.medianlev = mods.wavelet.medianlev;
    }

    if (wavelet.linkedg) {
        toEdit.wavelet.linkedg = mods.wavelet.linkedg;
    }

    if (wavelet.cbenab) {
        toEdit.wavelet.cbenab = mods.wavelet.cbenab;
    }

    if (wavelet.greenhigh) {
        toEdit.wavelet.greenhigh = mods.wavelet.greenhigh;
    }

    if (wavelet.bluehigh) {
        toEdit.wavelet.bluehigh = mods.wavelet.bluehigh;
    }

    if (wavelet.greenmed) {
        toEdit.wavelet.greenmed = mods.wavelet.greenmed;
    }

    if (wavelet.bluemed) {
        toEdit.wavelet.bluemed = mods.wavelet.bluemed;
    }

    if (wavelet.greenlow) {
        toEdit.wavelet.greenlow = mods.wavelet.greenlow;
    }

    if (wavelet.bluelow) {
        toEdit.wavelet.bluelow = mods.wavelet.bluelow;
    }

    if (wavelet.ballum) {
        toEdit.wavelet.ballum   = mods.wavelet.ballum;
    }

    if (wavelet.sigm) {
        toEdit.wavelet.sigm   = mods.wavelet.sigm;
    }

    if (wavelet.levden) {
        toEdit.wavelet.levden   = mods.wavelet.levden;
    }

    if (wavelet.thrden) {
        toEdit.wavelet.thrden   = mods.wavelet.thrden;
    }

    if (wavelet.limden) {
        toEdit.wavelet.limden   = mods.wavelet.limden;
    }

    if (wavelet.balchrom) {
        toEdit.wavelet.balchrom   = mods.wavelet.balchrom;
    }

    if (wavelet.chromfi) {
        toEdit.wavelet.chromfi   = mods.wavelet.chromfi;
    }

    if (wavelet.chromco) {
        toEdit.wavelet.chromco   = mods.wavelet.chromco;
    }

    if (wavelet.mergeL) {
        toEdit.wavelet.mergeL   = mods.wavelet.mergeL;
    }

    if (wavelet.mergeC) {
        toEdit.wavelet.mergeC   = mods.wavelet.mergeC;
    }

    if (wavelet.softrad) {
        toEdit.wavelet.softrad   = mods.wavelet.softrad;
    }

    if (wavelet.softradend) {
        toEdit.wavelet.softradend   = mods.wavelet.softradend;
    }

    if (wavelet.strend) {
        toEdit.wavelet.strend   = mods.wavelet.strend;
    }

    if (wavelet.detend) {
        toEdit.wavelet.detend   = mods.wavelet.detend;
    }

    if (wavelet.thrend) {
        toEdit.wavelet.thrend   = mods.wavelet.thrend;
    }

    if (wavelet.lipst) {
        toEdit.wavelet.lipst = mods.wavelet.lipst;
    }

    if (wavelet.Medgreinf) {
        toEdit.wavelet.Medgreinf = mods.wavelet.Medgreinf;
    }

    if (wavelet.ushamethod) {
        toEdit.wavelet.ushamethod   = mods.wavelet.ushamethod;
    }

    if (wavelet.avoid) {
        toEdit.wavelet.avoid = mods.wavelet.avoid;
    }

    if (wavelet.showmask) {
        toEdit.wavelet.showmask   = mods.wavelet.showmask;
    }

    if (wavelet.oldsh) {
        toEdit.wavelet.oldsh   = mods.wavelet.oldsh;
    }

    if (wavelet.tmr) {
        toEdit.wavelet.tmr = mods.wavelet.tmr;
    }

    if (wavelet.Lmethod) {
        toEdit.wavelet.Lmethod = mods.wavelet.Lmethod;
    }

    if (wavelet.CLmethod) {
        toEdit.wavelet.CLmethod = mods.wavelet.CLmethod;
    }

    if (wavelet.Backmethod) {
        toEdit.wavelet.Backmethod = mods.wavelet.Backmethod;
    }

    if (wavelet.Tilesmethod) {
        toEdit.wavelet.Tilesmethod = mods.wavelet.Tilesmethod;
    }

    if (wavelet.complexmethod) {
        toEdit.wavelet.complexmethod = mods.wavelet.complexmethod;
    }

    //if (wavelet.denmethod) {
    //    toEdit.wavelet.denmethod = mods.wavelet.denmethod;
    //}

    if (wavelet.mixmethod) {
        toEdit.wavelet.mixmethod = mods.wavelet.mixmethod;
    }

    if (wavelet.slimethod) {
        toEdit.wavelet.slimethod = mods.wavelet.slimethod;
    }

    if (wavelet.quamethod) {
        toEdit.wavelet.quamethod = mods.wavelet.quamethod;
    }

    if (wavelet.daubcoeffmethod) {
        toEdit.wavelet.daubcoeffmethod = mods.wavelet.daubcoeffmethod;
    }

    if (wavelet.CHmethod) {
        toEdit.wavelet.CHmethod = mods.wavelet.CHmethod;
    }

    if (wavelet.CHSLmethod) {
        toEdit.wavelet.CHSLmethod = mods.wavelet.CHSLmethod;
    }

    if (wavelet.EDmethod) {
        toEdit.wavelet.EDmethod = mods.wavelet.EDmethod;
    }

    if (wavelet.NPmethod) {
        toEdit.wavelet.NPmethod = mods.wavelet.NPmethod;
    }

    if (wavelet.BAmethod) {
        toEdit.wavelet.BAmethod = mods.wavelet.BAmethod;
    }

    if (wavelet.TMmethod) {
        toEdit.wavelet.TMmethod = mods.wavelet.TMmethod;
    }

    if (wavelet.HSmethod) {
        toEdit.wavelet.HSmethod = mods.wavelet.HSmethod;
    }

    if (wavelet.Dirmethod) {
        toEdit.wavelet.Dirmethod = mods.wavelet.Dirmethod;
    }

    if (wavelet.edgthresh) {
        toEdit.wavelet.edgthresh = mods.wavelet.edgthresh;
    }

    if (wavelet.sky) {
        toEdit.wavelet.sky = dontforceSet && options.baBehav[ADDSET_WA_SKYPROTECT] ? toEdit.wavelet.sky + mods.wavelet.sky : mods.wavelet.sky;
    }

    if (wavelet.thr) {
        toEdit.wavelet.thr = dontforceSet && options.baBehav[ADDSET_WA_THRR] ? toEdit.wavelet.thr + mods.wavelet.thr : mods.wavelet.thr;
    }

    if (wavelet.thrH) {
        toEdit.wavelet.thrH = dontforceSet && options.baBehav[ADDSET_WA_THRRH] ? toEdit.wavelet.thrH + mods.wavelet.thrH : mods.wavelet.thrH;
    }

    if (wavelet.radius) {
        toEdit.wavelet.radius = dontforceSet && options.baBehav[ADDSET_WA_RADIUS] ? toEdit.wavelet.radius + mods.wavelet.radius : mods.wavelet.radius;
    }

    if (wavelet.sup) {
        toEdit.wavelet.sup = mods.wavelet.sup;
    }

    if (wavelet.hllev) {
        toEdit.wavelet.hllev = mods.wavelet.hllev;
    }

    if (wavelet.bllev) {
        toEdit.wavelet.bllev = mods.wavelet.bllev;
    }

    if (wavelet.edgcont) {
        toEdit.wavelet.edgcont = mods.wavelet.edgcont;
    }

    if (wavelet.chrwav) {
        toEdit.wavelet.chrwav = mods.wavelet.chrwav;
    }

    if (wavelet.bluwav) {
        toEdit.wavelet.bluwav = mods.wavelet.bluwav;
    }

    if (wavelet.level0noise) {
        toEdit.wavelet.level0noise = mods.wavelet.level0noise;
    }

    if (wavelet.level1noise) {
        toEdit.wavelet.level1noise = mods.wavelet.level1noise;
    }

    if (wavelet.level2noise) {
        toEdit.wavelet.level2noise = mods.wavelet.level2noise;
    }

    if (wavelet.level3noise) {
        toEdit.wavelet.level3noise = mods.wavelet.level3noise;
    }

    if (wavelet.leveldenoise) {
        toEdit.wavelet.leveldenoise = mods.wavelet.leveldenoise;
    }

    if (wavelet.levelsigm) {
        toEdit.wavelet.levelsigm = mods.wavelet.levelsigm;
    }

    if (wavelet.pastlev) {
        toEdit.wavelet.pastlev = mods.wavelet.pastlev;
    }

    if (wavelet.satlev) {
        toEdit.wavelet.satlev = mods.wavelet.satlev;
    }

    if (wavelet.ccwcurve) {
        toEdit.wavelet.ccwcurve = mods.wavelet.ccwcurve;
    }

    if (wavelet.blcurve) {
        toEdit.wavelet.blcurve = mods.wavelet.blcurve;
    }

    //if (wavelet.opacityCurveSH) {
    //    toEdit.wavelet.opacityCurveSH = mods.wavelet.opacityCurveSH;
    //}

    if (wavelet.opacityCurveRG) {
        toEdit.wavelet.opacityCurveRG = mods.wavelet.opacityCurveRG;
    }

    if (wavelet.opacityCurveBY) {
        toEdit.wavelet.opacityCurveBY = mods.wavelet.opacityCurveBY;
    }

    if (wavelet.wavdenoise) {
        toEdit.wavelet.wavdenoise = mods.wavelet.wavdenoise;
    }

    if (wavelet.wavdenoiseh) {
        toEdit.wavelet.wavdenoiseh = mods.wavelet.wavdenoiseh;
    }

    if (wavelet.opacityCurveW) {
        toEdit.wavelet.opacityCurveW = mods.wavelet.opacityCurveW;
    }

    if (wavelet.opacityCurveWL) {
        toEdit.wavelet.opacityCurveWL = mods.wavelet.opacityCurveWL;
    }

    if (wavelet.hhcurve) {
        toEdit.wavelet.hhcurve = mods.wavelet.hhcurve;
    }

    if (wavelet.wavguidcurve) {
        toEdit.wavelet.wavguidcurve = mods.wavelet.wavguidcurve;
    }

    if (wavelet.wavhuecurve) {
        toEdit.wavelet.wavhuecurve = mods.wavelet.wavhuecurve;
    }

    if (wavelet.Chcurve) {
        toEdit.wavelet.Chcurve = mods.wavelet.Chcurve;
    }

    if (wavelet.wavclCurve) {
        toEdit.wavelet.wavclCurve = mods.wavelet.wavclCurve;
    }

    //if (wavelet.enacont)  toEdit.wavelet.enacont = mods.wavelet.enacont;
    if (wavelet.expcontrast) {
        toEdit.wavelet.expcontrast = mods.wavelet.expcontrast;
    }

    if (wavelet.expchroma) {
        toEdit.wavelet.expchroma = mods.wavelet.expchroma;
    }

    if (wavelet.expedge) {
        toEdit.wavelet.expedge = mods.wavelet.expedge;
    }

    if (wavelet.expbl) {
        toEdit.wavelet.expbl = mods.wavelet.expbl;
    }

    if (wavelet.expresid) {
        toEdit.wavelet.expresid = mods.wavelet.expresid;
    }

    if (wavelet.expfinal) {
        toEdit.wavelet.expfinal = mods.wavelet.expfinal;
    }

    if (wavelet.expclari) {
        toEdit.wavelet.expclari   = mods.wavelet.expclari;
    }

    if (wavelet.exptoning) {
        toEdit.wavelet.exptoning = mods.wavelet.exptoning;
    }

    if (wavelet.expnoise) {
        toEdit.wavelet.expnoise = mods.wavelet.expnoise;
    }

    for (int i = 0; i < 9; i++) {
        if (wavelet.c[i]) {
            toEdit.wavelet.c[i] = dontforceSet && options.baBehav[ADDSET_WA] ? toEdit.wavelet.c[i] + mods.wavelet.c[i] : mods.wavelet.c[i];
        }
    }

    for (int i = 0; i < 9; i++) {
        if (wavelet.ch[i]) {
            toEdit.wavelet.ch[i] = dontforceSet && options.baBehav[ADDSET_WA] ? toEdit.wavelet.ch[i] + mods.wavelet.ch[i] : mods.wavelet.ch[i];
        }
    }

    if (wavelet.skinprotect) {
        toEdit.wavelet.skinprotect = dontforceSet && options.baBehav[ADDSET_WA_SKINPROTECT] ? toEdit.wavelet.skinprotect + mods.wavelet.skinprotect : mods.wavelet.skinprotect;
    }

    if (wavelet.hueskin) {
        toEdit.wavelet.hueskin = mods.wavelet.hueskin;
    }

    if (wavelet.hueskin2) {
        toEdit.wavelet.hueskin2 = mods.wavelet.hueskin2;
    }

    if (wavelet.edgesensi) {
        toEdit.wavelet.edgesensi = mods.wavelet.edgesensi;
    }

    if (wavelet.edgeampli) {
        toEdit.wavelet.edgeampli = mods.wavelet.edgeampli;
    }

    if (wavelet.sigma) {
        toEdit.wavelet.sigma = mods.wavelet.sigma;
    }

    if (wavelet.offset) {
        toEdit.wavelet.offset = mods.wavelet.offset;
    }

    if (wavelet.lowthr) {
        toEdit.wavelet.lowthr = mods.wavelet.lowthr;
    }

    if (wavelet.resblur) {
        toEdit.wavelet.resblur = mods.wavelet.resblur;
    }

    if (wavelet.edgeffect) {
        toEdit.wavelet.edgeffect = mods.wavelet.edgeffect;
    }

    if (wavelet.resblurc) {
        toEdit.wavelet.resblurc = mods.wavelet.resblurc;
    }

    if (wavelet.resconH) {
        toEdit.wavelet.resconH = dontforceSet && options.baBehav[ADDSET_WA_RESCONH] ? toEdit.wavelet.resconH + mods.wavelet.resconH : mods.wavelet.resconH;
    }

    if (wavelet.reschro) {
        toEdit.wavelet.reschro = dontforceSet && options.baBehav[ADDSET_WA_RESCHRO] ? toEdit.wavelet.reschro + mods.wavelet.reschro : mods.wavelet.reschro;
    }

    if (wavelet.tmrs) {
        toEdit.wavelet.tmrs = dontforceSet && options.baBehav[ADDSET_WA_TMRS] ? toEdit.wavelet.tmrs + mods.wavelet.tmrs : mods.wavelet.tmrs;
    }

    if (wavelet.edgs) {
        toEdit.wavelet.edgs = dontforceSet && options.baBehav[ADDSET_WA_EDGS] ? toEdit.wavelet.edgs + mods.wavelet.edgs : mods.wavelet.edgs;
    }

    if (wavelet.scale) {
        toEdit.wavelet.scale = dontforceSet && options.baBehav[ADDSET_WA_SCALE] ? toEdit.wavelet.scale + mods.wavelet.scale : mods.wavelet.scale;
    }

    if (wavelet.gamma) {
        toEdit.wavelet.gamma = dontforceSet && options.baBehav[ADDSET_WA_GAMMA] ? toEdit.wavelet.gamma + mods.wavelet.gamma : mods.wavelet.gamma;
    }

    if (wavelet.rescon) {
        toEdit.wavelet.rescon = dontforceSet && options.baBehav[ADDSET_WA_RESCON] ? toEdit.wavelet.rescon + mods.wavelet.rescon : mods.wavelet.rescon;
    }

    if (wavelet.thres) {
        toEdit.wavelet.thres = dontforceSet && options.baBehav[ADDSET_WA_THRES] ? toEdit.wavelet.thres + mods.wavelet.thres : mods.wavelet.thres;
    }

    if (wavelet.threshold) {
        toEdit.wavelet.threshold = dontforceSet && options.baBehav[ADDSET_WA_THRESHOLD] ? toEdit.wavelet.threshold + mods.wavelet.threshold : mods.wavelet.threshold;
    }

    if (wavelet.threshold2) {
        toEdit.wavelet.threshold2 = dontforceSet && options.baBehav[ADDSET_WA_THRESHOLD2] ? toEdit.wavelet.threshold2 + mods.wavelet.threshold2 : mods.wavelet.threshold2;
    }

    if (wavelet.edgedetect) {
        toEdit.wavelet.edgedetect = dontforceSet && options.baBehav[ADDSET_WA_EDGEDETECT] ? toEdit.wavelet.edgedetect + mods.wavelet.edgedetect : mods.wavelet.edgedetect;
    }

    if (wavelet.edgedetectthr) {
        toEdit.wavelet.edgedetectthr = dontforceSet && options.baBehav[ADDSET_WA_EDGEDETECTTHR] ? toEdit.wavelet.edgedetectthr + mods.wavelet.edgedetectthr : mods.wavelet.edgedetectthr;
    }

    if (wavelet.edgedetectthr2) {
        toEdit.wavelet.edgedetectthr2 = dontforceSet && options.baBehav[ADDSET_WA_EDGEDETECTTHR2] ? toEdit.wavelet.edgedetectthr2 + mods.wavelet.edgedetectthr2 : mods.wavelet.edgedetectthr2;
    }

    if (wavelet.chro) {
        toEdit.wavelet.chro = dontforceSet && options.baBehav[ADDSET_WA_CHRO] ? toEdit.wavelet.chro + mods.wavelet.chro : mods.wavelet.chro;
    }

    if (wavelet.chroma) {
        toEdit.wavelet.chroma = dontforceSet && options.baBehav[ADDSET_WA_CHROMA] ? toEdit.wavelet.chroma + mods.wavelet.chroma : mods.wavelet.chroma;
    }

    if (wavelet.contrast) {
        toEdit.wavelet.contrast = dontforceSet && options.baBehav[ADDSET_WA_CONTRAST] ? toEdit.wavelet.contrast + mods.wavelet.contrast : mods.wavelet.contrast;
    }

    if (wavelet.edgrad) {
        toEdit.wavelet.edgrad = dontforceSet && options.baBehav[ADDSET_WA_EDGRAD] ? toEdit.wavelet.edgrad + mods.wavelet.edgrad : mods.wavelet.edgrad;
    }

    if (wavelet.edgval) {
        toEdit.wavelet.edgval = dontforceSet && options.baBehav[ADDSET_WA_EDGVAL] ? toEdit.wavelet.edgval + mods.wavelet.edgval : mods.wavelet.edgval;
    }

    if (wavelet.strength) {
        toEdit.wavelet.strength = dontforceSet && options.baBehav[ADDSET_WA_STRENGTH] ? toEdit.wavelet.strength + mods.wavelet.strength : mods.wavelet.strength;
    }


    if (dirpyrequalizer.enabled) {
        toEdit.dirpyrequalizer.enabled = mods.dirpyrequalizer.enabled;
    }

    if (dirpyrequalizer.gamutlab) {
        toEdit.dirpyrequalizer.gamutlab = mods.dirpyrequalizer.gamutlab;
    }

    if (dirpyrequalizer.cbdlMethod) {
        toEdit.dirpyrequalizer.cbdlMethod = mods.dirpyrequalizer.cbdlMethod;
    }

    for (int i = 0; i < 6; i++) {
        if (dirpyrequalizer.mult[i]) {
            toEdit.dirpyrequalizer.mult[i] = dontforceSet && options.baBehav[ADDSET_DIRPYREQ] ? toEdit.dirpyrequalizer.mult[i] + mods.dirpyrequalizer.mult[i] : mods.dirpyrequalizer.mult[i];
        }
    }

    if (dirpyrequalizer.threshold) {
        toEdit.dirpyrequalizer.threshold = dontforceSet && options.baBehav[ADDSET_DIRPYREQ_THRESHOLD] ? toEdit.dirpyrequalizer.threshold + mods.dirpyrequalizer.threshold : mods.dirpyrequalizer.threshold;
    }

    if (dirpyrequalizer.skinprotect) {
        toEdit.dirpyrequalizer.skinprotect = dontforceSet && options.baBehav[ADDSET_DIRPYREQ_SKINPROTECT] ? toEdit.dirpyrequalizer.skinprotect + mods.dirpyrequalizer.skinprotect : mods.dirpyrequalizer.skinprotect;
    }

    if (dirpyrequalizer.hueskin) {
        toEdit.dirpyrequalizer.hueskin = mods.dirpyrequalizer.hueskin;
    }

//  if (dirpyrequalizer.algo)       toEdit.dirpyrequalizer.algo = mods.dirpyrequalizer.algo;
    if (hsvequalizer.enabled) {
        toEdit.hsvequalizer.enabled = mods.hsvequalizer.enabled;
    }

    if (hsvequalizer.hcurve) {
        toEdit.hsvequalizer.hcurve = mods.hsvequalizer.hcurve;
    }

    if (hsvequalizer.scurve) {
        toEdit.hsvequalizer.scurve = mods.hsvequalizer.scurve;
    }

    if (hsvequalizer.vcurve) {
        toEdit.hsvequalizer.vcurve = mods.hsvequalizer.vcurve;
    }

    if (filmSimulation.enabled) {
        toEdit.filmSimulation.enabled = mods.filmSimulation.enabled;
    }

    if (filmSimulation.clutFilename) {
        toEdit.filmSimulation.clutFilename = mods.filmSimulation.clutFilename;
    }

    if (filmSimulation.strength) {
        toEdit.filmSimulation.strength = dontforceSet && options.baBehav[ADDSET_FILMSIMULATION_STRENGTH] ? toEdit.filmSimulation.strength + mods.filmSimulation.strength : mods.filmSimulation.strength;
    }

    if (softlight.enabled) {
        toEdit.softlight.enabled = mods.softlight.enabled;
    }

    if (softlight.strength) {
        toEdit.softlight.strength = dontforceSet && options.baBehav[ADDSET_SOFTLIGHT_STRENGTH] ? toEdit.softlight.strength + mods.softlight.strength : mods.softlight.strength;
    }

    if (dehaze.enabled) {
        toEdit.dehaze.enabled = mods.dehaze.enabled;
    }

    if (dehaze.strength) {
        toEdit.dehaze.strength = dontforceSet && options.baBehav[ADDSET_DEHAZE_STRENGTH] ? toEdit.dehaze.strength + mods.dehaze.strength : mods.dehaze.strength;
    }

    if (dehaze.depth) {
        toEdit.dehaze.depth = mods.dehaze.depth;
    }

    if (dehaze.showDepthMap) {
        toEdit.dehaze.showDepthMap = mods.dehaze.showDepthMap;
    }

    if (dehaze.saturation) {
        toEdit.dehaze.saturation = mods.dehaze.saturation;
    }

    if (metadata.mode) {
        toEdit.metadata.mode = mods.metadata.mode;
    }

    if (metadata.exifKeys) {
        toEdit.metadata.exifKeys = mods.metadata.exifKeys;
    }

    if (filmNegative.enabled) {
        toEdit.filmNegative.enabled = mods.filmNegative.enabled;
    }

    if (filmNegative.redRatio) {
        toEdit.filmNegative.redRatio = mods.filmNegative.redRatio;
    }

    if (filmNegative.greenExp) {
        toEdit.filmNegative.greenExp = mods.filmNegative.greenExp;
    }

    if (filmNegative.blueRatio) {
        toEdit.filmNegative.blueRatio = mods.filmNegative.blueRatio;
    }

    if (filmNegative.refInput) {
        toEdit.filmNegative.refInput = mods.filmNegative.refInput;
    }

    if (filmNegative.refOutput) {
        toEdit.filmNegative.refOutput = mods.filmNegative.refOutput;
    }

    if (filmNegative.colorSpace) {
        toEdit.filmNegative.colorSpace = mods.filmNegative.colorSpace;
    }

    // BackCompat param cannot be managed via the GUI, and it's always copied
    toEdit.filmNegative.backCompat = mods.filmNegative.backCompat;

    if (raw.preprocessWB.mode) {
        toEdit.raw.preprocessWB.mode = mods.raw.preprocessWB.mode;
    }

    // Exif changes are added to the existing ones
    if (exif) {
        for (procparams::ExifPairs::const_iterator i = mods.metadata.exif.begin(); i != mods.metadata.exif.end(); ++i) {
            toEdit.metadata.exif[i->first] = i->second;
        }
    }

    // IPTC changes are added to the existing ones
    if (iptc) {
        for (procparams::IPTCPairs::const_iterator i = mods.metadata.iptc.begin(); i != mods.metadata.iptc.end(); ++i) {
            toEdit.metadata.iptc[i->first] = i->second;
        }
    }
}

bool RAWParamsEdited::BayerSensor::isUnchanged() const
{
    return  method && border && imageNum && dcbIterations && dcbEnhance && lmmseIterations && dualDemosaicAutoContrast && dualDemosaicContrast /*&& allEnhance*/ &&  greenEq
            && pixelShiftMotionCorrectionMethod && pixelShiftEperIso && pixelShiftSigma && pixelShiftShowMotion && pixelShiftShowMotionMaskOnly
            && pixelShiftHoleFill && pixelShiftMedian && pixelShiftAverage && pixelShiftNonGreenCross && pixelShiftDemosaicMethod && pixelShiftGreen && pixelShiftBlur && pixelShiftSmooth && pixelShiftEqualBright && pixelShiftEqualBrightChannel
            && linenoise && linenoiseDirection && pdafLinesFilter && exBlack0 && exBlack1 && exBlack2 && exBlack3 && exTwoGreen && Dehablack;
}

bool RAWParamsEdited::XTransSensor::isUnchanged() const
{
    return method && border && exBlackRed && exBlackGreen && exBlackBlue && dualDemosaicAutoContrast && dualDemosaicContrast;
}

bool RAWParamsEdited::isUnchanged() const
{
    return  bayersensor.isUnchanged() && xtranssensor.isUnchanged() && ca_autocorrect && ca_avoidcolourshift && caautoiterations && cared && cablue && hotPixelFilter && deadPixelFilter && hotdeadpix_thresh && darkFrame
            && df_autoselect && ff_file && ff_AutoSelect && ff_FromMetaData && ff_BlurRadius && ff_BlurType && exPos && ff_AutoClipControl && ff_clipControl;
}

bool LensProfParamsEdited::isUnchanged() const
{
    return lcMode && lcpFile && useVign && lfLens;
}

bool RetinexParamsEdited::isUnchanged() const
{
    return enabled && retinexcolorspace && gammaretinex && gam && slope;
}

bool FilmNegativeParamsEdited::isUnchanged() const
{
    return enabled && redRatio && greenExp && blueRatio && refInput && refOutput && colorSpace;
}

LocallabParamsEdited::LocallabSpotEdited::LocallabSpotEdited(bool v) :
    // Control spot settings
    name(v),
    isvisible(v),
    prevMethod(v),
    shape(v),
    spotMethod(v),
    wavMethod(v),
    sensiexclu(v),
    structexclu(v),
    struc(v),
    shapeMethod(v),
    avoidgamutMethod(v),
    loc(v),
    centerX(v),
    centerY(v),
    spotangle(v),
    circrad(v),
    qualityMethod(v),
    complexMethod(v),
    transit(v),
    feather(v),
    thresh(v),
    iter(v),
    balan(v),
    balanh(v),
    colorde(v),
    colorscope(v),
    avoidrad(v),
    transitweak(v),
    transitgrad(v),
    hishow(v),
    activ(v),
    avoidneg(v),
    blwh(v),
    recurs(v),
    laplac(v),
    deltae(v),
    shortc(v),
    savrest(v),
    scopemask(v),
    denoichmask(v),
    lumask(v),
    // Color & Light
    visicolor(v),
    expcolor(v),
    complexcolor(v),
    curvactiv(v),
    lightness(v),
    reparcol(v),
    gamc(v),
    contrast(v),
    chroma(v),
    labgridALow(v),
    labgridBLow(v),
    labgridAHigh(v),
    labgridBHigh(v),
    labgridALowmerg(v),
    labgridBLowmerg(v),
    labgridAHighmerg(v),
    labgridBHighmerg(v),
    strengthgrid(v),
    sensi(v),
    structcol(v),
    strcol(v),
    strcolab(v),
    strcolh(v),
    angcol(v),
    feathercol(v),
    blurcolde(v),
    blurcol(v),
    contcol(v),
    blendmaskcol(v),
    radmaskcol(v),
    chromaskcol(v),
    gammaskcol(v),
    slomaskcol(v),
    shadmaskcol(v),
    strumaskcol(v),
    lapmaskcol(v),
    qualitycurveMethod(v),
    gridMethod(v),
    merMethod(v),
    toneMethod(v),
    mergecolMethod(v),
    llcurve(v),
    lccurve(v),
    cccurve(v),
    clcurve(v),
    rgbcurve(v),
    LHcurve(v),
    HHcurve(v),
    CHcurve(v),
    invers(v),
    special(v),
    toolcol(v),
    enaColorMask(v),
    fftColorMask(v),
    CCmaskcurve(v),
    LLmaskcurve(v),
    HHmaskcurve(v),
    HHhmaskcurve(v),
    softradiuscol(v),
    opacol(v),
    mercol(v),
    merlucol(v),
    conthrcol(v),
    Lmaskcurve(v),
    LLmaskcolcurvewav(v),
    csthresholdcol(v),
    recothresc(v),
    lowthresc(v),
    higthresc(v),
    decayc(v),
    // Exposure
    visiexpose(v),
    expexpose(v),
    complexexpose(v),
    expcomp(v),
    hlcompr(v),
    hlcomprthresh(v),
    black(v),
    shadex(v),
    shcompr(v),
    expchroma(v),
    sensiex(v),
    structexp(v),
    gamex(v),
    blurexpde(v),
    strexp(v),
    angexp(v),
    featherexp(v),
    excurve(v),
    norm(v),
    inversex(v),
    enaExpMask(v),
    enaExpMaskaft(v),
    CCmaskexpcurve(v),
    LLmaskexpcurve(v),
    HHmaskexpcurve(v),
    blendmaskexp(v),
    radmaskexp(v),
    chromaskexp(v),
    gammaskexp(v),
    slomaskexp(v),
    lapmaskexp(v),
    strmaskexp(v),
    angmaskexp(v),
    softradiusexp(v),
    Lmaskexpcurve(v),
    expMethod(v),
    exnoiseMethod(v),
    laplacexp(v),
    reparexp(v),
    balanexp(v),
    linear(v),
    gamm(v),
    fatamount(v),
    fatdetail(v),
    fatanchor(v),
    fatlevel(v),
    fatsatur(v),
    recothrese(v),
    lowthrese(v),
    higthrese(v),
    decaye(v),
    // Shadow highlight
    visishadhigh(v),
    expshadhigh(v),
    complexshadhigh(v),
    shMethod(v),
    ghsMethod(v),
    ghsMatmet(v),
    ghsMode(v),
    ghs_D(v),
    ghs_slope(v),
    ghs_chro(v),
    ghs_B(v),
    ghs_SP(v),
    SPAutoRadius(v),
    ghs_LP(v),
    ghs_HP(v),
    ghs_LC(v),
    ghs_MID(v),
    ghs_BLP(v),
    ghs_HLP(v),
    ghs_autobw(v),
    ghs_agx(v),
    ghs_smooth(v),
    ghs_inv(v),
    mich_exp(v),
    mich_spar(v),
    mich_kpar(v),
    mich_sat(v),
    mich_out(v),
    mich_black(v),
    mich_white(v),
    mich_high(v),
    mich_jdx(v),

    multsh{v, v, v, v, v, v, v},
    highlights(v),
    h_tonalwidth(v),
    shadows(v),
    s_tonalwidth(v),
    sh_radius(v),
    sensihs(v),
    enaSHMask(v),
    CCmaskSHcurve(v),
    LLmaskSHcurve(v),
    HHmaskSHcurve(v),
    blendmaskSH(v),
    radmaskSH(v),
    blurSHde(v),
    strSH(v),
    angSH(v),
    featherSH(v),
    inverssh(v),
    chromaskSH(v),
    gammaskSH(v),
    slomaskSH(v),
    lapmaskSH(v),
    detailSH(v),
    tePivot(v),
    reparsh(v),
    LmaskSHcurve(v),
    fatamountSH(v),
    fatanchorSH(v),
    gamSH(v),
    sloSH(v),
    recothress(v),
    lowthress(v),
    higthress(v),
    decays(v),
    // Vibrance
    visivibrance(v),
    expvibrance(v),
    complexvibrance(v),
    saturated(v),
    pastels(v),
    vibgam(v),
    warm(v),
    psthreshold(v),
    protectskins(v),
    avoidcolorshift(v),
    pastsattog(v),
    sensiv(v),
    skintonescurve(v),
    CCmaskvibcurve(v),
    LLmaskvibcurve(v),
    HHmaskvibcurve(v),
    enavibMask(v),
    blendmaskvib(v),
    radmaskvib(v),
    chromaskvib(v),
    gammaskvib(v),
    slomaskvib(v),
    lapmaskvib(v),
    strvib(v),
    strvibab(v),
    strvibh(v),
    angvib(v),
    feathervib(v),
    Lmaskvibcurve(v),
    recothresv(v),
    lowthresv(v),
    higthresv(v),
    decayv(v),
    // Soft Light
    visisoft(v),
    expsoft(v),
    complexsoft(v),
    streng(v),
    sensisf(v),
    laplace(v),
    softMethod(v),
    // Blur & Noise
    visiblur(v),
    expblur(v),
    complexblur(v),
    radius(v),
    strength(v),
    sensibn(v),
    itera(v),
    guidbl(v),
    strbl(v),
    recothres(v),
    lowthres(v),
    higthres(v),
    recothresd(v),
    lowthresd(v),
    midthresd(v),
    midthresdch(v),
    higthresd(v),
    decayd(v),
    isogr(v),
    strengr(v),
    scalegr(v),
    divgr(v),
    epsbl(v),
    blMethod(v),
    chroMethod(v),
    quamethod(v),
    usemask(v),
    invmaskd(v),
    invmask(v),
    levelthr(v),
    lnoiselow(v),
    levelthrlow(v),
    blurMethod(v),
    medMethod(v),
    activlum(v),
    madlsav{v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v},
    noiselumf(v),
    noiselumf0(v),
    noiselumf2(v),
    noiselumc(v),
    noiselumdetail(v),
    noiselequal(v),
    noisegam(v),
    noisechrof(v),
    noisechroc(v),
    noisechrodetail(v),
    adjblur(v),
    bilateral(v),
    nlstr(v),
    nldet(v),
    nlpat(v),
    nlrad(v),
    nlgam(v),
    nliter(v),
    sensiden(v),
    reparden(v),
    detailthr(v),
    locwavcurveden(v),
    locwavcurvehue(v),
    locwavcurvehuecont(v),
    showmaskblMethodtyp(v),
    CCmaskblcurve(v),
    LLmaskblcurve(v),
    HHmaskblcurve(v),
    enablMask(v),
    fftwbl(v),
    invbl(v),
    toolbl(v),
    blendmaskbl(v),
    radmaskbl(v),
    chromaskbl(v),
    gammaskbl(v),
    slomaskbl(v),
    lapmaskbl(v),
    shadmaskbl(v),
    shadmaskblsha(v),
    strumaskbl(v),
    Lmaskblcurve(v),
    LLmaskblcurvewav(v),
    csthresholdblur(v),
    denocontrast(v),
    denoAutocontrast(v),
    contrshow(v),
    lockmadl(v),
    madllock(v),
    enacontrast(v),
    denoratio(v),
    denomask(v),
    // Tone Mapping
    visitonemap(v),
    exptonemap(v),
    complextonemap(v),
    stren(v),
    gamma(v),
    estop(v),
    scaltm(v),
    repartm(v),
    rewei(v),
    satur(v),
    sensitm(v),
    softradiustm(v),
    amount(v),
    equiltm(v),
    CCmasktmcurve(v),
    LLmasktmcurve(v),
    HHmasktmcurve(v),
    enatmMask(v),
    enatmMaskaft(v),
    blendmasktm(v),
    radmasktm(v),
    chromasktm(v),
    gammasktm(v),
    slomasktm(v),
    lapmasktm(v),
    Lmasktmcurve(v),
    recothrest(v),
    lowthrest(v),
    higthrest(v),
    decayt(v),
    // Retinex
    visireti(v),
    expreti(v),
    complexreti(v),
    retinexMethod(v),
    str(v),
    chrrt(v),
    neigh(v),
    vart(v),
    offs(v),
    dehaz(v),
    depth(v),
    sensih(v),
    localTgaincurve(v),
    localTtranscurve(v),
    inversret(v),
    equilret(v),
    loglin(v),
    dehazeSaturation(v),
    dehazeblack(v),
    softradiusret(v),
    CCmaskreticurve(v),
    LLmaskreticurve(v),
    HHmaskreticurve(v),
    enaretiMask(v),
    enaretiMasktmap(v),
    blendmaskreti(v),
    radmaskreti(v),
    chromaskreti(v),
    gammaskreti(v),
    slomaskreti(v),
    lapmaskreti(v),
    scalereti(v),
    darkness(v),
    lightnessreti(v),
    limd(v),
    cliptm(v),
    fftwreti(v),
    Lmaskreticurve(v),
    recothresr(v),
    lowthresr(v),
    higthresr(v),
    decayr(v),
    // Sharpening
    visisharp(v),
    expsharp(v),
    complexsharp(v),
    sharcontrast(v),
    deconvAutoshar(v),
    sharradius(v),
    sharamount(v),
    shardamping(v),
    shariter(v),
    sharblur(v),
    shargam(v),
    sensisha(v),
    inverssha(v),
    sharshow(v),
    itercheck(v),
    methodcap(v),
    capradius(v),
    deconvAutoRadius(v),
    deconvCoBoost(v),
    deconvCoProt(v),
    deconvCoLat(v),
    deconvCogam(v),
    reparsha(v),
    // Local Contrast
    visicontrast(v),
    expcontrast(v),
    complexcontrast(v),
    lcradius(v),
    lcamount(v),
    lcdarkness(v),
    lclightness(v),
    sigmalc(v),
    offslc(v),
    levelwav(v),
    residcont(v),
    residsha(v),
    residshathr(v),
    residhi(v),
    residhithr(v),
    gamlc(v),
    residgam(v),
    residslop(v),
    residblur(v),
    levelblur(v),
    sigmabl(v),
    residchro(v),
    residcomp(v),
    sigma(v),
    offset(v),
    sigmadr(v),
    threswav(v),
    chromalev(v),
    chromablu(v),
    sigmadc(v),
    deltad(v),
    fatres(v),
    clarilres(v),
    claricres(v),
    clarisoft(v),
    sigmalc2(v),
    strwav(v),
    angwav(v),
    featherwav(v),
    strengthw(v),
    sigmaed(v),
    radiusw(v),
    detailw(v),
    gradw(v),
    tloww(v),
    thigw(v),
    edgw(v),
    basew(v),
    sensilc(v),
    reparw(v),
    fftwlc(v),
    blurlc(v),
    wavblur(v),
    wavedg(v),
    waveshow(v),
    wavcont(v),
    wavcomp(v),
    wavgradl(v),
    wavcompre(v),
    origlc(v),
    processwav(v),
    localcontMethod(v),
    localedgMethod(v),
    localneiMethod(v),
    locwavcurve(v),
    csthreshold(v),
    loclevwavcurve(v),
    locconwavcurve(v),
    loccompwavcurve(v),
    loccomprewavcurve(v),
    locedgwavcurve(v),
    CCmasklccurve(v),
    LLmasklccurve(v),
    HHmasklccurve(v),
    enalcMask(v),
    blendmasklc(v),
    radmasklc(v),
    chromasklc(v),
    Lmasklccurve(v),
    recothresw(v),
    lowthresw(v),
    higthresw(v),
    decayw(v),
    // Contrast by detail levels
    visicbdl(v),
    expcbdl(v),
    complexcbdl(v),
    mult{v, v, v, v, v, v},
    chromacbdl(v),
    threshold(v),
    sensicb(v),
    clarityml(v),
    contresid(v),
    softradiuscb(v),
    enacbMask(v),
    CCmaskcbcurve(v),
    LLmaskcbcurve(v),
    HHmaskcbcurve(v),
    blendmaskcb(v),
    radmaskcb(v),
    chromaskcb(v),
    gammaskcb(v),
    slomaskcb(v),
    lapmaskcb(v),
    Lmaskcbcurve(v),
    recothrescb(v),
    lowthrescb(v),
    higthrescb(v),
    decaycb(v),
    // Log encoding
    visilog(v),
    explog(v),
    complexlog(v),
    autocompute(v),
    sourceGray(v),
    sourceabs(v),
    targabs(v),
    targetGray(v),
    catad(v),
    saturl(v),
    chroml(v),
    lightl(v),
    lightq(v),
    contl(v),
    contthres(v),
    contq(v),
    colorfl(v),
    LcurveL(v),
    Autogray(v),
    fullimage(v),
    repar(v),
    ciecam(v),
    satlog(v),
    blackEv(v),
    whiteEv(v),
    whiteslog(v),
    blackslog(v),
    comprlog(v),
    strelog(v),
    detail(v),
    sursour(v),
    surround(v),
    sensilog(v),
    baselog(v),
    strlog(v),
    anglog(v),
    featherlog(v),
    CCmaskcurveL(v),
    LLmaskcurveL(v),
    HHmaskcurveL(v),
    enaLMask(v),
    blendmaskL(v),
    radmaskL(v),
    chromaskL(v),
    LmaskcurveL(v),
    recothresl(v),
    lowthresl(v),
    higthresl(v),
    decayl(v),
    // mask
    visimask(v),
    complexmask(v),
    expmask(v),
    sensimask(v),
    blendmask(v),
    blendmaskab(v),
    softradiusmask(v),
    enamask(v),
    fftmask(v),
    blurmask(v),
    contmask(v),
    CCmask_curve(v),
    LLmask_curve(v),
    HHmask_curve(v),
    strumaskmask(v),
    toolmask(v),
    radmask(v),
    lapmask(v),
    chromask(v),
    gammask(v),
    slopmask(v),
    shadmask(v),
    str_mask(v),
    ang_mask(v),
    feather_mask(v),
    HHhmask_curve(v),
    Lmask_curve(v),
    LLmask_curvewav(v),
    csthresholdmask(v),
    //ciecam
    visicie(v),
    complexcie(v),
    expcie(v),
    expprecam(v),
    reparcie(v),
    sensicie(v),
    Autograycie(v),
    sigybjz12(v),
    qtoj(v),
    jabcie(v),
    comprcieauto(v),
    normcie12(v),
    normcie(v),
    gamutcie(v),
    bwcie(v),
    sigcie(v),
    logcie(v),
    satcie(v),
    logcieq(v),
    smoothcie(v),
    smoothcietrc(v),
    smoothcietrcrel(v),
    smoothcieyb(v),
    smoothcielum(v),
    smoothciehigh(v),
    smoothcielnk(v),
    smoothcieinv(v),
    logjz(v),
    sigjz12(v),
    sigjz(v),
    forcebw(v),
    sigq12(v),
    sigq(v),
    chjzcie(v),
    sourceGraycie(v),
    sourceabscie(v),
    sursourcie(v),
    modecam(v),
    modeQJ(v),
    bwevMethod12(v),
    bwevMethod(v),
    modecie(v),
    saturlcie(v),
    rstprotectcie(v),
    chromlcie(v),
    huecie(v),
    toneMethodcie(v),
    ciecurve(v),
    toneMethodcie2(v),
    ciecurve2(v),
    chromjzcie(v),
    saturjzcie(v),
    huejzcie(v),
    softjzcie(v),
    strsoftjzcie(v),
    thrhjzcie(v),
    jzcurve(v),
    czcurve(v),
    czjzcurve(v),
    invcurve(v),
    HHcurvejz(v),
    CHcurvejz(v),
    LHcurvejz(v),
    lightlcie(v),
    lightjzcie(v),
    lightqcie(v),
    lightsigqcie(v),
    contlcie(v),
    contjzcie(v),
    detailciejz(v),
    adapjzcie(v),
    jz100(v),
    pqremap(v),
    pqremapcam16(v),
    hljzcie(v),
    hlthjzcie(v),
    shjzcie(v),
    shthjzcie(v),
    radjzcie(v),
    contthrescie(v),
    blackEvjz(v),
    whiteEvjz(v),
    targetjz(v),
    sigmoidldacie12(v),
    sigmoidthcie12(v),
    sigmoidblcie12(v),
    sigmoidldacie(v),
    sigmoidthcie(v),
    sigmoidsenscie(v),
    sigmoidblcie(v),
    comprcie(v),
    strcielog(v),
    comprcieth(v),
    gamjcie(v),
    smoothcieth(v),
    smoothciethtrc(v),
    slopjcie(v),
    satjcie(v),
    contsig(v),
    skewsig(v),
    whitsig(v),
    slopesmo(v),
    slopesmoq(v),
    slopesmor(v),
    slopesmog(v),
    slopesmob(v),
    kslopesmor(v),
    kslopesmog(v),
    kslopesmob(v),
    midtciemet(v),
    midtcie(v),
    redxl(v),
    redyl(v),
    grexl(v),
    greyl(v),
    bluxl(v),
    bluyl(v),
    refi(v),
    shiftxl(v),
    shiftyl(v),
    labgridcieALow(v),
    labgridcieBLow(v),
    labgridcieAHigh(v),
    labgridcieBHigh(v),
    labgridcieGx(v),
    labgridcieGy(v),
    labgridcieWx(v),
    labgridcieWy(v),       
    labgridcieMx(v),
    labgridcieMy(v),       
    whitescie(v),
    blackscie(v),
    illMethod(v),
    smoothciemet(v),
    primMethod(v),
    catMethod(v),
    sigmoidldajzcie12(v),
    sigmoidthjzcie12(v),
    sigmoidbljzcie12(v),
    sigmoidldajzcie(v),
    sigmoidthjzcie(v),
    sigmoidbljzcie(v),
    contqcie(v),
    contsigqcie(v),
    colorflcie(v),
    targabscie(v),
    targetGraycie(v),
    catadcie(v),
    detailcie(v),
    surroundcie(v),
    strgradcie(v),
    anggradcie(v),
    feathercie(v),
    enacieMask(v),
    enacieMaskall(v),
    CCmaskciecurve(v),
    LLmaskciecurve(v),
    HHmaskciecurve(v),
    HHhmaskciecurve(v),
    blendmaskcie(v),
    radmaskcie(v),
    sigmalcjz(v),
    clarilresjz(v),
    claricresjz(v),
    clarisoftjz(v),
    locwavcurvejz(v),
    csthresholdjz(v),
    chromaskcie(v),
    lapmaskcie(v),
    gammaskcie(v),
    slomaskcie(v),
    Lmaskciecurve(v),
    recothrescie(v),
    lowthrescie(v),
    higthrescie(v),
    decaycie(v),
    strumaskcie(v),
    toolcie(v),
    fftcieMask(v),
	contcie(v),
	blurcie(v),
	highmaskcie(v),
	shadmaskcie(v),
    LLmaskciecurvewav(v),
    csthresholdcie(v)
	
{
}

void LocallabParamsEdited::LocallabSpotEdited::set(bool v)
{
    name = v;
    isvisible = v;
    prevMethod = v;
    shape = v;
    spotMethod = v;
    wavMethod = v;
    sensiexclu = v;
    structexclu = v;
    struc = v;
    shapeMethod = v;
    avoidgamutMethod = v;
    loc = v;
    centerX = v;
    centerY = v;
    spotangle = v;
    circrad = v;
    qualityMethod = v;
    complexMethod = v;
    transit = v;
    feather = v;
    thresh = v;
    iter = v;
    balan = v;
    balanh = v;
    colorde = v;
    colorscope = v;
    avoidrad = v;
    transitweak = v;
    transitgrad = v;
    hishow = v;
    activ = v;
    avoidneg = v;
    blwh = v;
    recurs = v;
    laplac = v;
    deltae = v;
    shortc = v;
    savrest = v;
    scopemask = v;
    denoichmask = v;
    lumask = v;
    // Color & Light
    visicolor = v;
    expcolor = v;
    complexcolor = v;
    curvactiv = v;
    lightness = v;
    reparcol = v;
    gamc = v;
    contrast = v;
    chroma = v;
    labgridALow = v;
    labgridBLow = v;
    labgridAHigh = v;
    labgridBHigh = v;
    labgridALowmerg = v;
    labgridBLowmerg = v;
    labgridAHighmerg = v;
    labgridBHighmerg = v;
    strengthgrid = v;
    sensi = v;
    structcol = v;
    strcol = v;
    strcolab = v;
    strcolh = v;
    angcol = v;
    feathercol = v;
    blurcolde = v;
    blurcol = v;
    contcol = v;
    blendmaskcol = v;
    radmaskcol = v;
    chromaskcol = v;
    gammaskcol = v;
    slomaskcol = v;
    shadmaskcol = v;
    strumaskcol = v;
    lapmaskcol = v;
    qualitycurveMethod = v;
    gridMethod = v;
    merMethod = v;
    toneMethod = v;
    mergecolMethod = v;
    llcurve = v;
    lccurve = v;
    cccurve = v;
    clcurve = v;
    rgbcurve = v;
    LHcurve = v;
    HHcurve = v;
    CHcurve = v;
    invers = v;
    special = v;
    toolcol = v;
    enaColorMask = v;
    fftColorMask = v;
    CCmaskcurve = v;
    LLmaskcurve = v;
    HHmaskcurve = v;
    HHhmaskcurve = v;
    softradiuscol = v;
    opacol = v;
    mercol = v;
    merlucol = v;
    conthrcol = v;
    Lmaskcurve = v;
    LLmaskcolcurvewav = v;
    csthresholdcol = v;
    recothresc = v;
    lowthresc = v;
    higthresc = v;
    decayc = v;
    // Exposure
    visiexpose = v;
    expexpose = v;
    complexexpose = v;
    expcomp = v;
    hlcompr = v;
    hlcomprthresh = v;
    black = v;
    shadex = v;
    shcompr = v;
    expchroma = v;
    sensiex = v;
    structexp = v;
    gamex = v;
    blurexpde = v;
    strexp = v;
    angexp = v;
    featherexp = v;
    excurve = v;
    norm = v;
    inversex = v;
    enaExpMask = v;
    enaExpMaskaft = v;
    CCmaskexpcurve = v;
    LLmaskexpcurve = v;
    HHmaskexpcurve = v;
    blendmaskexp = v;
    radmaskexp = v;
    chromaskexp = v;
    gammaskexp = v;
    slomaskexp = v;
    lapmaskexp = v;
    strmaskexp = v;
    angmaskexp = v;
    softradiusexp = v;
    Lmaskexpcurve = v;
    expMethod = v;
    exnoiseMethod = v;
    laplacexp = v;
    reparexp = v;
    balanexp = v;
    linear = v;
    gamm = v;
    fatamount = v;
    fatdetail = v;
    fatsatur = v;
    fatanchor = v;
    fatlevel = v;
    recothrese = v;
    lowthrese = v;
    higthrese = v;
    decaye = v;
    // Shadow highlight
    visishadhigh = v;
    expshadhigh = v;
    complexshadhigh = v;
    shMethod = v;
    ghsMethod = v;
    ghsMatmet = v;
    ghsMode = v;
    ghs_D = v;
    ghs_slope = v;
    ghs_chro = v;
    ghs_B = v;
    ghs_SP = v;
    SPAutoRadius = v;
    ghs_LP = v;
    ghs_HP = v;
    ghs_LC = v;
    ghs_MID = v;
    ghs_BLP = v;
    ghs_HLP = v;
    ghs_autobw = v;
    ghs_agx = v;
    ghs_smooth = v;
    ghs_inv = v;
    mich_exp = v;
    mich_spar = v;
    mich_kpar = v;
    mich_sat = v;
    mich_out = v;
    mich_black = v;
    mich_white = v;
    mich_high = v;
    mich_jdx = v;

    for (int i = 0; i < 6; i++) {
        multsh[i] = v;
    }

    highlights = v;
    h_tonalwidth = v;
    shadows = v;
    s_tonalwidth = v;
    sh_radius = v;
    sensihs = v;
    enaSHMask = v;
    CCmaskSHcurve = v;
    LLmaskSHcurve = v;
    HHmaskSHcurve = v;
    blendmaskSH = v;
    radmaskSH = v;
    blurSHde = v;
    strSH = v;
    angSH = v;
    featherSH = v;
    inverssh = v;
    chromaskSH = v;
    gammaskSH = v;
    slomaskSH = v;
    lapmaskSH = v;
    detailSH = v;
    tePivot = v;
    reparsh = v;
    LmaskSHcurve = v;
    fatamountSH = v;
    fatanchorSH = v;
    gamSH = v;
    sloSH = v;
    recothress = v;
    lowthress = v;
    higthress = v;
    decays = v;
    // Vibrance
    visivibrance = v;
    expvibrance = v;
    complexvibrance = v;
    saturated = v;
    pastels = v;
    vibgam = v;
    warm = v;
    psthreshold = v;
    protectskins = v;
    avoidcolorshift = v;
    pastsattog = v;
    sensiv = v;
    skintonescurve = v;
    CCmaskvibcurve = v;
    LLmaskvibcurve = v;
    HHmaskvibcurve = v;
    enavibMask = v;
    blendmaskvib = v;
    radmaskvib = v;
    chromaskvib = v;
    gammaskvib = v;
    slomaskvib = v;
    lapmaskvib = v;
    strvib = v;
    strvibab = v;
    strvibh = v;
    angvib = v;
    feathervib = v;
    Lmaskvibcurve = v;
    recothresv = v;
    lowthresv = v;
    higthresv = v;
    decayv = v;
    // Soft Light
    visisoft = v;
    expsoft = v;
    complexsoft = v;
    streng = v;
    sensisf = v;
    laplace = v;
    softMethod = v;
    // Blur & Noise
    visiblur = v;
    expblur = v;
    complexblur = v;
    radius = v;
    strength = v;
    sensibn = v;
    itera = v;
    guidbl = v;
    strbl = v;
    recothres = v;
    lowthres = v;
    higthres = v;
    recothresd = v;
    lowthresd = v;
    midthresd = v;
    midthresdch = v;
    higthresd = v;
    decayd = v;
    isogr = v;
    strengr = v;
    scalegr = v;
    divgr = v;
    epsbl = v;
    blMethod = v;
    chroMethod = v;
    quamethod = v;
    usemask = v;
    invmaskd = v;
    invmask = v;
    levelthr = v;
    lnoiselow = v;
    levelthrlow = v;
    blurMethod = v;
    medMethod = v;
    activlum = v;
    for (int i = 0; i < 21; i++) {
        madlsav[i] = v;
    }
    noiselumf = v;
    noiselumf0 = v;
    noiselumf2 = v;
    noiselumc = v;
    noiselumdetail = v;
    noiselequal = v;
    noisegam = v;
    noisechrof = v;
    noisechroc = v;
    noisechrodetail = v;
    adjblur = v;
    bilateral = v;
    nlstr = v;
    nldet = v;
    nlpat = v;
    nlrad = v;
    nlgam = v;
    nliter = v;
    sensiden = v;
    reparden = v;
    detailthr = v;
    locwavcurveden = v;
    locwavcurvehue = v;
    locwavcurvehuecont = v;
    showmaskblMethodtyp = v;
    CCmaskblcurve = v;
    LLmaskblcurve = v;
    HHmaskblcurve = v;
    enablMask = v;
    fftwbl = v;
    invbl = v;
    toolbl = v;
    blendmaskbl = v;
    radmaskbl = v;
    chromaskbl = v;
    gammaskbl = v;
    slomaskbl = v;
    lapmaskbl = v;
    shadmaskbl = v;
    shadmaskblsha = v;
    strumaskbl = v;
    Lmaskblcurve = v;
    LLmaskblcurvewav = v;
    csthresholdblur = v;
    denocontrast = v;
    denoAutocontrast = v;
    contrshow = v;
    lockmadl = v;
    madllock = v;
    enacontrast = v;
    denoratio = v;
    denomask = v;
    // Tone Mapping
    visitonemap = v;
    exptonemap = v;
    complextonemap = v;
    stren = v;
    gamma = v;
    estop = v;
    scaltm = v;
    repartm = v;
    rewei = v;
    satur = v;
    sensitm = v;
    softradiustm = v;
    amount = v;
    equiltm = v;
    CCmasktmcurve = v;
    LLmasktmcurve = v;
    HHmasktmcurve = v;
    enatmMask = v;
    enatmMaskaft = v;
    blendmasktm = v;
    radmasktm = v;
    chromasktm = v;
    gammasktm = v;
    slomasktm = v;
    lapmasktm = v;
    Lmasktmcurve = v;
    recothrest = v;
    lowthrest = v;
    higthrest = v;
    decayt = v;
    // Retinex
    visireti = v;
    expreti = v;
    complexreti = v;
    retinexMethod = v;
    str = v;
    chrrt = v;
    neigh = v;
    vart = v;
    offs = v;
    dehaz = v;
    depth = v;
    sensih = v;
    localTgaincurve = v;
    localTtranscurve = v;
    inversret = v;
    equilret = v;
    loglin = v;
    dehazeSaturation = v;
    dehazeblack = v;
    softradiusret = v;
    CCmaskreticurve = v;
    LLmaskreticurve = v;
    HHmaskreticurve = v;
    enaretiMask = v;
    enaretiMasktmap = v;
    blendmaskreti = v;
    radmaskreti = v;
    chromaskreti = v;
    gammaskreti = v;
    slomaskreti = v;
    lapmaskreti = v;
    scalereti = v;
    darkness = v;
    lightnessreti = v;
    limd = v;
    cliptm = v;
    fftwreti = v;
    Lmaskreticurve = v;
    recothresr = v;
    lowthresr = v;
    higthresr = v;
    decayr = v;
    // Sharpening
    visisharp = v;
    expsharp = v;
    complexsharp = v;
    sharcontrast = v;
    deconvAutoshar = v;
    sharradius = v;
    sharamount = v;
    shardamping = v;
    shariter = v;
    sharblur = v;
    shargam = v;
    sensisha = v;
    inverssha = v;
    sharshow = v;
    sharshow = v;
    itercheck = v;
    capradius = v;
    deconvAutoRadius = v;
    deconvCoBoost = v;
    deconvCoProt = v;
    deconvCoLat = v;
    deconvCogam = v;
    reparsha = v;

    // Local Contrast
    visicontrast = v;
    expcontrast = v;
    complexcontrast = v;
    lcradius = v;
    lcamount = v;
    lcdarkness = v;
    lclightness = v;
    sigmalc = v;
    offslc = v;
    levelwav = v;
    residcont = v;
    residsha = v;
    residshathr = v;
    residhi = v;
    residhithr = v;
    gamlc = v;
    residgam = v;
    residslop = v;
    residblur = v;
    levelblur = v;
    sigmabl = v;
    residchro = v;
    residcomp = v;
    sigma = v;
    offset = v;
    sigmadr = v;
    threswav = v;
    chromalev = v;
    chromablu = v;
    sigmadc = v;
    deltad = v;
    fatres = v;
    clarilres = v;
    claricres = v;
    clarisoft = v;
    sigmalc2 = v;
    strwav = v;
    angwav = v;
    featherwav = v;
    strengthw = v;
    sigmaed = v;
    radiusw = v;
    detailw = v;
    gradw = v;
    tloww = v;
    thigw = v;
    edgw = v;
    basew = v;
    sensilc = v;
    reparw = v;
    fftwlc = v;
    blurlc = v;
    wavblur = v;
    wavedg = v;
    waveshow = v;
    wavcont = v;
    wavcomp = v;
    wavgradl = v;
    wavcompre = v;
    origlc = v;
    processwav = v;
    localcontMethod = v;
    localedgMethod = v;
    localneiMethod = v;
    locwavcurve = v;
    csthreshold = v;
    loclevwavcurve = v;
    locconwavcurve = v;
    loccompwavcurve = v;
    loccomprewavcurve = v;
    locedgwavcurve = v;
    CCmasklccurve = v;
    LLmasklccurve = v;
    HHmasklccurve = v;
    enalcMask = v;
    blendmasklc = v;
    radmasklc = v;
    chromasklc = v;
    Lmasklccurve = v;
    recothresw = v;
    lowthresw = v;
    higthresw = v;
    decayw = v;
    // Contrast by detail levels
    visicbdl = v;
    expcbdl = v;
    complexcbdl = v;

    for (int i = 0; i < 6; i++) {
        mult[i] = v;
    }

    chromacbdl = v;
    threshold = v;
    sensicb = v;
    clarityml = v;
    contresid = v;
    softradiuscb = v;
    enacbMask = v;
    CCmaskcbcurve = v;
    LLmaskcbcurve = v;
    HHmaskcbcurve = v;
    blendmaskcb = v;
    radmaskcb = v;
    chromaskcb = v;
    gammaskcb = v;
    slomaskcb = v;
    lapmaskcb = v;
    Lmaskcbcurve = v;
    recothrescb = v;
    lowthrescb = v;
    higthrescb = v;
    decaycb = v;
    // Log encoding
    visilog = v;
    explog = v;
    complexlog = v;
    autocompute = v;
    sourceGray = v;
    sourceabs = v;
    targabs = v;
    targetGray = v;
    catad = v;
    saturl = v;
    chroml = v;
    lightl = v;
    lightq = v;
    contl = v;
    contthres = v;
    contq = v;
    colorfl = v;
    LcurveL = v;
    Autogray = v;
    fullimage = v;
    repar = v;
    ciecam = v;
    satlog = v;
    blackEv = v;
    whiteEv = v;
    whiteslog = v;
    blackslog = v;
    comprlog = v;
    strelog = v;
    detail = v;
    sursour = v;
    surround = v;
    sensilog = v;
    baselog = v;
    strlog = v;
    anglog = v;
    featherlog = v;
    CCmaskcurveL = v;
    LLmaskcurveL = v;
    HHmaskcurveL = v;
    enaLMask = v;
    blendmaskL = v;
    radmaskL = v;
    chromaskL = v;
    LmaskcurveL = v;
    recothresl = v;
    lowthresl = v;
    higthresl = v;
    decayl = v;
    // mask
    visimask = v;
    complexmask = v;
    expmask = v;
    sensimask = v;
    blendmask = v;
    blendmaskab = v;
    softradiusmask = v;
    enamask = v;
    fftmask = v;
    blurmask = v;
    contmask = v;
    CCmask_curve = v;
    LLmask_curve = v;
    HHmask_curve = v;
    strumaskmask = v;
    toolmask = v;
    radmask = v;
    lapmask = v;
    chromask = v;
    gammask = v;
    slopmask = v;
    shadmask = v;
    str_mask = v;
    ang_mask = v;
    feather_mask = v;
    HHhmask_curve = v;
    Lmask_curve = v;
    LLmask_curvewav = v;
    csthresholdmask = v;
    //ciecam
    visicie= v;
    complexcie= v;
    expcie = v;
    expprecam = v;
    reparcie = v;
    sensicie = v;
    Autograycie = v;
    sigybjz12 = v;
    qtoj = v;
    jabcie = v;
    comprcieauto = v;
    normcie12 = v;
    normcie = v;
    gamutcie = v;
    bwcie = v;
    sigcie = v;
    logcie = v;
    satcie = v;
    logcieq = v;
    smoothcie = v;
    smoothcietrc = v;
    smoothcietrcrel = v;
    smoothcieyb = v;
    smoothcielum = v;
    smoothciehigh = v;
    smoothcielnk = v;
    smoothcieinv = v;
    logjz = v;
    sigjz12 = v;
    sigjz = v;
    forcebw = v;
    sigq12 = v;
    sigq = v;
    chjzcie = v;
    sourceGraycie = v;
    sourceabscie = v;
    sursourcie = v;
    modecam = v;
    modeQJ = v;
    bwevMethod12 = v;
    bwevMethod = v;
    modecie = v;
    saturlcie = v;
    rstprotectcie = v;
    chromlcie = v;
    huecie = v;
    toneMethodcie = v;
    ciecurve = v;
    toneMethodcie2 = v;
    ciecurve2 = v;
    chromjzcie = v;
    saturjzcie = v;
    huejzcie = v;
    softjzcie = v;
    strsoftjzcie = v;
    thrhjzcie = v;
    jzcurve = v;
    czcurve = v;
    czjzcurve = v;
    invcurve = v;
    HHcurvejz = v;
    CHcurvejz = v;
    LHcurvejz = v;
    lightlcie = v;
    lightjzcie = v;
    lightqcie = v;
    lightsigqcie = v;
    contlcie = v;
    contjzcie = v;
    detailciejz = v;
    adapjzcie = v;
    jz100 = v;
    pqremap = v;
    pqremapcam16 = v;
    hljzcie = v;
    hlthjzcie = v;
    shjzcie = v;
    shthjzcie = v;
    radjzcie = v;
    contthrescie = v;
    blackEvjz = v;
    whiteEvjz = v;
    targetjz = v;
    sigmoidldacie12 = v;
    sigmoidthcie12 = v;
    sigmoidblcie12 = v;

    sigmoidldacie = v;
    sigmoidthcie = v;
    sigmoidsenscie = v;
    sigmoidblcie = v;
    comprcie = v;
    strcielog = v;
    comprcieth = v;
    gamjcie = v;
    smoothcieth = v;
    smoothciethtrc = v;
    slopjcie = v;
    satjcie = v;
    contsig = v;
    skewsig = v;
    whitsig = v;
    slopesmo = v;
    slopesmoq = v;
    slopesmor = v;
    slopesmog = v;
    slopesmob = v;
    kslopesmor = v;
    kslopesmog = v;
    kslopesmob = v;
    midtciemet = v;
    midtcie = v;
    redxl = v;
    redyl = v;
    grexl = v;
    greyl = v;
    bluxl = v;
    bluyl = v;
    refi = v;
    shiftxl = v;
    shiftyl = v;
    labgridcieALow = v;
    labgridcieBLow  = v;
    labgridcieAHigh= v;
    labgridcieBHigh = v;
    labgridcieGx = v;
    labgridcieGy = v;
    labgridcieWx = v;
    labgridcieWy = v;         
    labgridcieMx = v;
    labgridcieMy = v;         
    whitescie = v;
    blackscie = v;
    illMethod = v;
    smoothciemet = v;
    primMethod = v;
    catMethod = v;
    sigmoidldajzcie12 = v;
    sigmoidthjzcie12 = v;
    sigmoidbljzcie12 = v;
    sigmoidldajzcie = v;
    sigmoidthjzcie = v;
    sigmoidbljzcie = v;
    contqcie = v;
    contsigqcie = v;
    colorflcie = v;
    targabscie = v;
    targetGraycie = v;
    catadcie = v;
    detailcie = v;
    surroundcie = v;
    anggradcie =  v;
    feathercie =  v;
    strgradcie =  v;
    enacieMask = v;
    enacieMaskall = v;
    CCmaskciecurve = v;
    LLmaskciecurve = v;
    HHmaskciecurve = v;
    HHhmaskciecurve = v;
    blendmaskcie = v;
    radmaskcie = v;
    sigmalcjz = v;
    clarilresjz = v;
    claricresjz = v;
    clarisoftjz = v;
    locwavcurvejz = v;
    csthresholdjz = v;
    chromaskcie = v;
    lapmaskcie = v;
    gammaskcie = v;
    slomaskcie = v;
    Lmaskciecurve = v;
    recothrescie = v;
    lowthrescie = v;
    higthrescie = v;
    decaycie = v;
    strumaskcie = v;
    toolcie = v;
    fftcieMask = v;
	contcie = v;
	blurcie = v;
	highmaskcie = v;
	shadmaskcie = v;
    LLmaskciecurvewav = v;
    csthresholdcie = v;

}

bool CaptureSharpeningParamsEdited::isUnchanged() const
{
    return enabled && contrast && autoContrast && autoRadius && deconvradius && deconvradiusOffset && deconviter && deconvitercheck  && showcap  && noisecaptype && noisecap && noisecapafter;;
}

bool RAWParamsEdited::PreprocessWBParamsEdited::isUnchanged() const
{
    return mode;
}
