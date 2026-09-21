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

#include "procparams.h"

#include <memory>

#include <locale.h>

#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

#include "params/serdes.h"

#include "aspectratios.h"
#include "cJSON.h"
#include "color.h"
#include "colortemp.h"
#include "curves.h"
#include "utils.h"

#include "rtgui/multilangmgr.h"
#include "rtgui/options.h"
#include "rtgui/paramsedited.h"
#include "rtgui/ppversion.h"
#include "rtgui/version.h"

using namespace std;

namespace
{

void getFromKeyfile(
    const Glib::KeyFile& keyfile,
    const Glib::ustring& group_name,
    const Glib::ustring& key,
    rtengine::procparams::FilmNegativeParams::RGB& value
)
{
    const std::vector<double> v = keyfile.get_double_list(group_name, key);
    if(v.size() >= 3) {
        value.r = v[0];
        value.g = v[1];
        value.b = v[2];
    }
}

bool assignRgbFromKeyfile(
    const Glib::KeyFile& keyfile,
    const Glib::ustring& group_name,
    const Glib::ustring& key,
    rtengine::procparams::FilmNegativeParams::RGB& value,
    bool& params_edited_value
)
{
    if (keyfile.has_key(group_name, key)) {
        getFromKeyfile(keyfile, group_name, key, value);
        params_edited_value = true;
        return true;
    }
    return false;
}

void putToKeyfile(
    const Glib::ustring& group_name,
    const Glib::ustring& key,
    const rtengine::procparams::FilmNegativeParams::RGB& value,
    Glib::KeyFile& keyfile
)
{
    const std::vector<double> vec = { value.r, value.g, value.b };
    keyfile.set_double_list(group_name, key, vec);
}

bool saveRgbToKeyfile(
    bool save,
    const Glib::ustring& group_name,
    const Glib::ustring& key,
    const rtengine::procparams::FilmNegativeParams::RGB& value,
    Glib::KeyFile& keyfile
)
{
    if (save) {
        putToKeyfile(group_name, key, value, keyfile);
        return true;
    }
    return false;
}

namespace ProfileKeys {

#define DEFINE_KEY(VAR, NAME) \
    constexpr const char* VAR = NAME;

DEFINE_KEY(TOOL_ENABLED, "Enabled");

namespace CropGuide {
    DEFINE_KEY(TOOL_NAME, "CropGuide");

    DEFINE_KEY(RULE_OF_THIRDS, "RuleOfThirds");
    DEFINE_KEY(RULE_OF_DIAGONALS, "RuleOfDiagonals");
    DEFINE_KEY(HARMONIC_MEANS, "HarmonicMeans");
    DEFINE_KEY(CROSSHAIR, "Crosshair");
    DEFINE_KEY(GRID, "Grid");
    DEFINE_KEY(GOLDEN_TRIANGLE, "GoldenTriangle");
    DEFINE_KEY(GOLDEN_RATIO, "GoldenRatio");
    DEFINE_KEY(EPASSPORT, "Epassport");
    DEFINE_KEY(CENTERED_SQUARE, "CenteredSquare");

    DEFINE_KEY(GOLDEN_TRIANGLE_MIRROR, "GoldenTriangleMirror");
    DEFINE_KEY(GOLDEN_RATIO_ROTATE, "GoldenRatioRotate");
    DEFINE_KEY(GOLDEN_RATIO_MIRROR, "GoldenRatioMirror");
    DEFINE_KEY(ASPECT_RATIOS, "AspectRatios");
    DEFINE_KEY(BLEED, "Bleed");
    DEFINE_KEY(BASIS, "Basis");
    DEFINE_KEY(BASIS_SCALE, "Scale");
    DEFINE_KEY(BASIS_WIDTH, "Width");
    DEFINE_KEY(BASIS_HEIGHT, "Height");
    DEFINE_KEY(BASIS_LONG, "Long");
    DEFINE_KEY(BASIS_SHORT, "Short");

    DEFINE_KEY(NAME, "Name");
    DEFINE_KEY(IS_PORTRAIT, "IsPortrait");
    DEFINE_KEY(RED, "Red");
    DEFINE_KEY(GREEN, "Green");
    DEFINE_KEY(BLUE, "Blue");
    DEFINE_KEY(ALPHA, "Alpha");
}  // namespace CropGuide

namespace Framing
{
    DEFINE_KEY(TOOL_NAME, "Framing");

    // Fields
    DEFINE_KEY(FRAMING_METHOD, "FramingMethod");
    DEFINE_KEY(ASPECT_RATIO, "AspectRatio");
    DEFINE_KEY(ORIENTATION, "Orientation");
    DEFINE_KEY(FRAMED_WIDTH, "FramedWidth");
    DEFINE_KEY(FRAMED_HEIGHT, "FramedHeight");
    DEFINE_KEY(ALLOW_UPSCALING, "AllowUpscaling");

    DEFINE_KEY(BORDER_SIZING_METHOD, "BorderSizingMethod");
    DEFINE_KEY(BASIS, "Basis");
    DEFINE_KEY(RELATIVE_BORDER_SIZE, "RelativeBorderSize");
    DEFINE_KEY(MIN_SIZE_ENABLED, "MinSizeEnabled");
    DEFINE_KEY(MIN_WIDTH, "MinWidth");
    DEFINE_KEY(MIN_HEIGHT, "MinHeight");
    DEFINE_KEY(ABS_WIDTH, "AbsWidth");
    DEFINE_KEY(ABS_HEIGHT, "AbsHeight");

    DEFINE_KEY(BORDER_RED, "BorderRed");
    DEFINE_KEY(BORDER_GREEN, "BorderGreen");
    DEFINE_KEY(BORDER_BLUE, "BorderBlue");

    // Enum mappings
    DEFINE_KEY(FRAMING_METHOD_STANDARD, "Standard");
    DEFINE_KEY(FRAMING_METHOD_BBOX, "BoundingBox");
    DEFINE_KEY(FRAMING_METHOD_FIXED_SIZE, "FixedSize");
    DEFINE_KEY(ORIENT_AS_IMAGE, "AsImage");
    DEFINE_KEY(ORIENT_LANDSCAPE, "Landscape");
    DEFINE_KEY(ORIENT_PORTRAIT, "Portrait");
    DEFINE_KEY(BORDER_SIZING_PERCENTAGE, "Percentage");
    DEFINE_KEY(BORDER_SIZING_UNIFORM_PERCENTAGE, "UniformPercentage");
    DEFINE_KEY(BORDER_SIZING_FIXED_SIZE, "FixedSize");
    DEFINE_KEY(BASIS_AUTO, "Auto");
    DEFINE_KEY(BASIS_WIDTH, "Width");
    DEFINE_KEY(BASIS_HEIGHT, "Height");
    DEFINE_KEY(BASIS_LONG, "Long");
    DEFINE_KEY(BASIS_SHORT, "Short");
}  // namespace Framing

}  // namespace ProfileKeys

void loadFramingParams(
    const Glib::KeyFile& keyFile,
    rtengine::procparams::FramingParams& params,
    FramingParamsEdited& edited
)
{
    using namespace ProfileKeys;
    using namespace ProfileKeys::Framing;
    using FramingParams = rtengine::procparams::FramingParams;

    const Glib::ustring group{TOOL_NAME};
    if (!keyFile.has_group(group)) return;

    assignFromKeyfile(keyFile, group, TOOL_ENABLED, params.enabled, edited.enabled);

    using FramingMethod = FramingParams::FramingMethod;
    const std::map<std::string, FramingMethod> framingMethodMapping = {
        {FRAMING_METHOD_STANDARD, FramingMethod::STANDARD},
        {FRAMING_METHOD_BBOX, FramingMethod::BBOX},
        {FRAMING_METHOD_FIXED_SIZE, FramingMethod::FIXED_SIZE}
    };
    assignFromKeyfile(keyFile, group, FRAMING_METHOD, framingMethodMapping, params.framingMethod, edited.framingMethod);
    assignFromKeyfile(keyFile, group, ASPECT_RATIO, params.aspectRatio, edited.aspectRatio);
    using Orientation = FramingParams::Orientation;
    const std::map<std::string, Orientation> orientationMapping = {
        {ORIENT_AS_IMAGE, Orientation::AS_IMAGE},
        {ORIENT_LANDSCAPE, Orientation::LANDSCAPE},
        {ORIENT_PORTRAIT, Orientation::PORTRAIT},
    };
    assignFromKeyfile(keyFile, group, ORIENTATION, orientationMapping, params.orientation, edited.orientation);
    assignFromKeyfile(keyFile, group, FRAMED_WIDTH, params.framedWidth, edited.framedWidth);
    assignFromKeyfile(keyFile, group, FRAMED_HEIGHT, params.framedHeight, edited.framedHeight);
    assignFromKeyfile(keyFile, group, ALLOW_UPSCALING, params.allowUpscaling, edited.allowUpscaling);

    using BorderSizing = FramingParams::BorderSizing;
    const std::map<std::string, BorderSizing> borderSizingMapping = {
        {BORDER_SIZING_PERCENTAGE, BorderSizing::PERCENTAGE},
        {BORDER_SIZING_UNIFORM_PERCENTAGE, BorderSizing::UNIFORM_PERCENTAGE},
        {BORDER_SIZING_FIXED_SIZE, BorderSizing::FIXED_SIZE}
    };
    assignFromKeyfile(keyFile, group, BORDER_SIZING_METHOD, borderSizingMapping, params.borderSizingMethod, edited.borderSizingMethod);
    using Basis = FramingParams::Basis;
    const std::map<std::string, Basis> basisMapping = {
        {BASIS_AUTO, Basis::AUTO},
        {BASIS_WIDTH, Basis::WIDTH},
        {BASIS_HEIGHT, Basis::HEIGHT},
        {BASIS_LONG, Basis::LONG},
        {BASIS_SHORT, Basis::SHORT}
    };
    assignFromKeyfile(keyFile, group, BASIS, basisMapping, params.basis, edited.basis);
    assignFromKeyfile(keyFile, group, RELATIVE_BORDER_SIZE, params.relativeBorderSize, edited.relativeBorderSize);
    assignFromKeyfile(keyFile, group, MIN_SIZE_ENABLED, params.minSizeEnabled, edited.minSizeEnabled);
    assignFromKeyfile(keyFile, group, MIN_WIDTH, params.minWidth, edited.minWidth);
    assignFromKeyfile(keyFile, group, MIN_HEIGHT, params.minHeight, edited.minHeight);
    assignFromKeyfile(keyFile, group, ABS_WIDTH, params.absWidth, edited.absWidth);
    assignFromKeyfile(keyFile, group, ABS_HEIGHT, params.absHeight, edited.absHeight);

    assignFromKeyfile(keyFile, group, BORDER_RED, params.borderRed, edited.borderRed);
    assignFromKeyfile(keyFile, group, BORDER_GREEN, params.borderGreen, edited.borderGreen);
    assignFromKeyfile(keyFile, group, BORDER_BLUE, params.borderBlue, edited.borderBlue);
}

void saveFramingParams(
    Glib::KeyFile& keyFile,
    const rtengine::procparams::FramingParams& params,
    const ParamsEdited* pedited
)
{
    using namespace ProfileKeys;
    using namespace ProfileKeys::Framing;
    using FramingParams = rtengine::procparams::FramingParams;

    const Glib::ustring group{TOOL_NAME};

    const FramingParamsEdited& edited = pedited->framing;

    saveToKeyfile(!pedited || edited.enabled, group, TOOL_ENABLED, params.enabled, keyFile);

    using FramingMethod = FramingParams::FramingMethod;
    const std::map<FramingMethod, const char*> framingMethodMapping = {
        {FramingMethod::STANDARD, FRAMING_METHOD_STANDARD},
        {FramingMethod::BBOX, FRAMING_METHOD_BBOX},
        {FramingMethod::FIXED_SIZE, FRAMING_METHOD_FIXED_SIZE}
    };
    saveToKeyfile(!pedited || edited.framingMethod, group, FRAMING_METHOD, framingMethodMapping, params.framingMethod, keyFile);
    saveToKeyfile(!pedited || edited.aspectRatio, group, ASPECT_RATIO, params.aspectRatio, keyFile);
    using Orientation = FramingParams::Orientation;
    const std::map<Orientation, const char*> orientationMapping = {
        {Orientation::AS_IMAGE, ORIENT_AS_IMAGE},
        {Orientation::LANDSCAPE, ORIENT_LANDSCAPE},
        {Orientation::PORTRAIT, ORIENT_PORTRAIT},
    };
    saveToKeyfile(!pedited || edited.orientation, group, ORIENTATION, orientationMapping, params.orientation, keyFile);
    saveToKeyfile(!pedited || edited.framedWidth, group, FRAMED_WIDTH, params.framedWidth, keyFile);
    saveToKeyfile(!pedited || edited.framedHeight, group, FRAMED_HEIGHT, params.framedHeight, keyFile);
    saveToKeyfile(!pedited || edited.allowUpscaling, group, ALLOW_UPSCALING, params.allowUpscaling, keyFile);

    using BorderSizing = FramingParams::BorderSizing;
    const std::map<BorderSizing, const char*> borderSizingMapping = {
        {BorderSizing::PERCENTAGE, BORDER_SIZING_PERCENTAGE},
        {BorderSizing::UNIFORM_PERCENTAGE, BORDER_SIZING_UNIFORM_PERCENTAGE},
        {BorderSizing::FIXED_SIZE, BORDER_SIZING_FIXED_SIZE}
    };
    saveToKeyfile(!pedited || edited.borderSizingMethod, group, BORDER_SIZING_METHOD, borderSizingMapping, params.borderSizingMethod, keyFile);
    using Basis = FramingParams::Basis;
    const std::map<Basis, const char*> basisMapping = {
        {Basis::AUTO, BASIS_AUTO},
        {Basis::WIDTH, BASIS_WIDTH},
        {Basis::HEIGHT, BASIS_HEIGHT},
        {Basis::LONG, BASIS_LONG},
        {Basis::SHORT, BASIS_SHORT}
    };
    saveToKeyfile(!pedited || edited.basis, group, BASIS, basisMapping, params.basis, keyFile);
    saveToKeyfile(!pedited || edited.relativeBorderSize, group, RELATIVE_BORDER_SIZE, params.relativeBorderSize, keyFile);
    saveToKeyfile(!pedited || edited.minSizeEnabled, group, MIN_SIZE_ENABLED, params.minSizeEnabled, keyFile);
    saveToKeyfile(!pedited || edited.minWidth, group, MIN_WIDTH, params.minWidth, keyFile);
    saveToKeyfile(!pedited || edited.minHeight, group, MIN_HEIGHT, params.minHeight, keyFile);
    saveToKeyfile(!pedited || edited.absWidth, group, ABS_WIDTH, params.absWidth, keyFile);
    saveToKeyfile(!pedited || edited.absHeight, group, ABS_HEIGHT, params.absHeight, keyFile);

    saveToKeyfile(!pedited || edited.borderRed, group, BORDER_RED, params.borderRed, keyFile);
    saveToKeyfile(!pedited || edited.borderGreen, group, BORDER_GREEN, params.borderGreen, keyFile);
    saveToKeyfile(!pedited || edited.borderBlue, group, BORDER_BLUE, params.borderBlue, keyFile);
}

void loadCropGuideParams(
    const Glib::KeyFile& keyFile,
    rtengine::procparams::CropGuideParams& params,
    CropGuideParamsEdited& edited
)
{
    using namespace ProfileKeys;
    using namespace ProfileKeys::CropGuide;

    using CropGuideParams = rtengine::procparams::CropGuideParams;
    using PresetIndex = CropGuideParams::PresetIndex;

    const Glib::ustring group{TOOL_NAME};
    if (!keyFile.has_group(group)) return;

    assignFromKeyfile(keyFile, group, TOOL_ENABLED, params.enabled, edited.enabled);

    auto parse_color = [&](const cJSON* obj, const char* key, double& out) {
        const cJSON* entry = cJSON_GetObjectItemCaseSensitive(obj, key);
        if (cJSON_IsNumber(entry)) {
            out = entry->valuedouble;
        } else {
            out = 1.0;
        }
    };

    auto load = [&](PresetIndex index, const char* key) {
        if (!keyFile.has_key(group, key)) return;

        Glib::ustring data = keyFile.get_string(group, key);
        cJSON* json = cJSON_Parse(data.c_str());
        if (!json) return;
        if (!cJSON_IsObject(json)) {
            cJSON_Delete(json);
            return;
        }

        CropGuideParams::PresetParams& preset = params.presets[index];
        edited.presets[index] = true;

        const cJSON* enabled =
            cJSON_GetObjectItemCaseSensitive(json, TOOL_ENABLED);
        preset.enabled = cJSON_IsTrue(enabled);

        parse_color(json, RED, preset.red);
        parse_color(json, GREEN, preset.green);
        parse_color(json, BLUE, preset.blue);
        parse_color(json, ALPHA, preset.alpha);

        cJSON_Delete(json);
    };

    load(PresetIndex::RULE_OF_THIRDS, RULE_OF_THIRDS);
    load(PresetIndex::RULE_OF_DIAGONALS, RULE_OF_DIAGONALS);
    load(PresetIndex::HARMONIC_MEANS, HARMONIC_MEANS);
    load(PresetIndex::CROSSHAIR, CROSSHAIR);
    load(PresetIndex::GRID, GRID);
    load(PresetIndex::GOLDEN_TRIANGLE, GOLDEN_TRIANGLE);
    load(PresetIndex::GOLDEN_RATIO, GOLDEN_RATIO);
    load(PresetIndex::EPASSPORT, EPASSPORT);
    load(PresetIndex::CENTERED_SQUARE, CENTERED_SQUARE);

    assignFromKeyfile(keyFile, group, GOLDEN_TRIANGLE_MIRROR,
                      params.mirror_golden_triangle, edited.mirror_golden_triangle);
    assignFromKeyfile(keyFile, group, GOLDEN_RATIO_ROTATE,
                      params.rotate_golden_ratio, edited.rotate_golden_ratio);
    assignFromKeyfile(keyFile, group, GOLDEN_RATIO_MIRROR,
                      params.mirror_golden_ratio, edited.mirror_golden_ratio);
    assignFromKeyfile(keyFile, group, BLEED, params.bleed, edited.bleed);

    using Basis = CropGuideParams::Basis;
    const std::map<std::string, Basis> basis_mapping = {
        {BASIS_SCALE, Basis::SCALE},
        {BASIS_WIDTH, Basis::WIDTH},
        {BASIS_HEIGHT, Basis::HEIGHT},
        {BASIS_LONG, Basis::LONG},
        {BASIS_SHORT, Basis::SHORT}
    };
    assignFromKeyfile(keyFile, group, BASIS, basis_mapping, params.basis, edited.basis);

    auto parse_aspect_ratios = [&](const char* data) {
        cJSON* json = cJSON_Parse(data);
        if (!json) return;
        if (!cJSON_IsArray(json)) {
            cJSON_Delete(json);
            return;
        }

        auto num_objects = cJSON_GetArraySize(json);

        edited.aspect_ratios = true;
        params.aspect_ratios.clear();
        if (num_objects == 0) {
            cJSON_Delete(json);
            return;
        }

        params.aspect_ratios.reserve(num_objects);
        const auto& presets = getAspectRatios();

        auto find_index = [&](const char* value, size_t& result) -> bool {
            for (size_t i = 0; i < presets.size(); i++) {
                if (presets[i].label == value) {
                    result = i;
                    return true;
                }
            }
            return false;
        };

        const cJSON* obj = nullptr;
        cJSON_ArrayForEach(obj, json) {
            const cJSON* name = cJSON_GetObjectItemCaseSensitive(obj, NAME);
            if (cJSON_IsString(name) && (name->valuestring != nullptr)) {
                size_t preset_index = 0;
                if (find_index(name->valuestring, preset_index)) {
                    CropGuideParams::AspectRatioParams entry(preset_index);
                    edited.aspect_ratios = true;

                    const cJSON* enabled =
                        cJSON_GetObjectItemCaseSensitive(obj, TOOL_ENABLED);
                    entry.enabled = cJSON_IsTrue(enabled);

                    const cJSON* is_portrait =
                        cJSON_GetObjectItemCaseSensitive(obj, IS_PORTRAIT);
                    entry.is_portrait = cJSON_IsTrue(is_portrait);

                    parse_color(obj, RED, entry.red);
                    parse_color(obj, GREEN, entry.green);
                    parse_color(obj, BLUE, entry.blue);
                    parse_color(obj, ALPHA, entry.alpha);
                    params.aspect_ratios.push_back(std::move(entry));
                }
            }
        }

        cJSON_Delete(json);
    };

    if (keyFile.has_key(group, ASPECT_RATIOS)) {
        Glib::ustring aspect_ratios = keyFile.get_string(group, ASPECT_RATIOS);
        parse_aspect_ratios(aspect_ratios.c_str());
    }
}

void saveCropGuideParams(
    Glib::KeyFile& keyFile,
    const rtengine::procparams::CropGuideParams& params,
    const ParamsEdited* pedited
)
{
    using namespace ProfileKeys;
    using namespace ProfileKeys::CropGuide;

    using CropGuideParams = rtengine::procparams::CropGuideParams;
    using PresetIndex = CropGuideParams::PresetIndex;

    const Glib::ustring group{TOOL_NAME};

    saveToKeyfile(!pedited || pedited->cropGuide.enabled, group, TOOL_ENABLED,
                  params.enabled, keyFile);

    auto save = [&](PresetIndex index, const char* key) {
        if (pedited && !pedited->cropGuide.presets[index]) return;

        cJSON* json = cJSON_CreateObject();
        if (!json) return;

        const CropGuideParams::PresetParams& preset = params.presets[index];
        if (preset.enabled) {
            cJSON_AddTrueToObject(json, TOOL_ENABLED);
        } else {
            cJSON_AddFalseToObject(json, TOOL_ENABLED);
        }
        cJSON_AddNumberToObject(json, RED, preset.red);
        cJSON_AddNumberToObject(json, GREEN, preset.green);
        cJSON_AddNumberToObject(json, BLUE, preset.blue);
        cJSON_AddNumberToObject(json, ALPHA, preset.alpha);

        char* serialized_json = cJSON_PrintUnformatted(json);
        if (serialized_json) {
            keyFile.set_string(group, key, serialized_json);
            free(serialized_json);
        }

        cJSON_Delete(json);
    };

    save(PresetIndex::RULE_OF_THIRDS, RULE_OF_THIRDS);
    save(PresetIndex::RULE_OF_DIAGONALS, RULE_OF_DIAGONALS);
    save(PresetIndex::HARMONIC_MEANS, HARMONIC_MEANS);
    save(PresetIndex::CROSSHAIR, CROSSHAIR);
    save(PresetIndex::GRID, GRID);
    save(PresetIndex::GOLDEN_TRIANGLE, GOLDEN_TRIANGLE);
    save(PresetIndex::GOLDEN_RATIO, GOLDEN_RATIO);
    save(PresetIndex::EPASSPORT, EPASSPORT);
    save(PresetIndex::CENTERED_SQUARE, CENTERED_SQUARE);

    saveToKeyfile(!pedited || pedited->cropGuide.mirror_golden_triangle, group,
                  GOLDEN_TRIANGLE_MIRROR, params.mirror_golden_triangle, keyFile);
    saveToKeyfile(!pedited || pedited->cropGuide.rotate_golden_ratio, group,
                  GOLDEN_RATIO_ROTATE, params.rotate_golden_ratio, keyFile);
    saveToKeyfile(!pedited || pedited->cropGuide.mirror_golden_ratio, group,
                  GOLDEN_RATIO_MIRROR, params.mirror_golden_ratio, keyFile);
    saveToKeyfile(!pedited || pedited->cropGuide.bleed, group, BLEED, params.bleed,
                  keyFile);

    using Basis = CropGuideParams::Basis;
    const std::map<Basis, const char*> basis_mapping = {
        {Basis::SCALE, BASIS_SCALE},
        {Basis::WIDTH, BASIS_WIDTH},
        {Basis::HEIGHT, BASIS_HEIGHT},
        {Basis::LONG, BASIS_LONG},
        {Basis::SHORT, BASIS_SHORT},
    };
    saveToKeyfile(!pedited || pedited->cropGuide.basis, group, BASIS, basis_mapping, params.basis, keyFile);

    auto dump_aspect_ratios = [&]() {
        cJSON* serialized_aspect_ratios = cJSON_CreateArray();
        if (!serialized_aspect_ratios) return;

        for (const auto& entry : params.aspect_ratios) {
            cJSON* obj = cJSON_CreateObject();
            if (!obj) continue;

            if (entry.enabled) {
                cJSON_AddTrueToObject(obj, TOOL_ENABLED);
            } else {
                cJSON_AddFalseToObject(obj, TOOL_ENABLED);
            }
            if (entry.is_portrait) {
                cJSON_AddTrueToObject(obj, IS_PORTRAIT);
            } else {
                cJSON_AddFalseToObject(obj, IS_PORTRAIT);
            }

            auto name = getAspectRatioLabel(entry.preset_index);
            cJSON_AddStringToObject(obj, NAME, name.c_str());

            cJSON_AddNumberToObject(obj, RED, entry.red);
            cJSON_AddNumberToObject(obj, GREEN, entry.green);
            cJSON_AddNumberToObject(obj, BLUE, entry.blue);
            cJSON_AddNumberToObject(obj, ALPHA, entry.alpha);

            cJSON_AddItemToArray(serialized_aspect_ratios, obj);
        }

        char* serialized_json = cJSON_PrintUnformatted(serialized_aspect_ratios);
        if (serialized_json) {
            keyFile.set_string(group, ASPECT_RATIOS, serialized_json);
            free(serialized_json);
        }
        cJSON_Delete(serialized_aspect_ratios);
    };

    if (!pedited || pedited->cropGuide.aspect_ratios) {
        dump_aspect_ratios();
    }
}

} // namespace

namespace rtengine
{

namespace procparams
{

const short SpotParams::minRadius = 1;
const short SpotParams::maxRadius = 400;


ToneCurveParams::ToneCurveParams() :
    autoexp(false),
    clip(0.02),
    hrenabled(false),
    method("Coloropp"),
    expcomp(0),
    curve{
        DCT_Linear
    },
    curve2{
        DCT_Linear
    },
    curveMode(ToneCurveMode::STD),
    curveMode2(ToneCurveMode::STD),
    brightness(0),
    black(0),
    contrast(0),
    saturation(0),
    shcompr(50),
    hlcompr(0),
    hlbl(0),
    hlth(1.0),
    hlcomprthresh(0),
    histmatching(false),
    fromHistMatching(false),
    clampOOG(true)
{
}

bool ToneCurveParams::isPanningRelatedChange(const ToneCurveParams& other) const
{
    return !
        (autoexp == other.autoexp
        && clip == other.clip
        && hrenabled == other.hrenabled
        && method == other.method
        && expcomp == other.expcomp
        && curve == other.curve
        && curve2 == other.curve2
        && curveMode == other.curveMode
        && curveMode2 == other.curveMode2
        && brightness == other.brightness
        && black == other.black
        && contrast == other.contrast
        && saturation == other.saturation
        && shcompr == other.shcompr
        && hlcompr == other.hlcompr
        && hlbl == other.hlbl
        && hlth == other.hlth
        && hlcomprthresh == other.hlcomprthresh
        && histmatching == other.histmatching
        && clampOOG == other.clampOOG);
}

bool ToneCurveParams::operator ==(const ToneCurveParams& other) const
{
    return
        autoexp == other.autoexp
        && clip == other.clip
        && hrenabled == other.hrenabled
        && method == other.method
        && expcomp == other.expcomp
        && curve == other.curve
        && curve2 == other.curve2
        && curveMode == other.curveMode
        && curveMode2 == other.curveMode2
        && brightness == other.brightness
        && black == other.black
        && contrast == other.contrast
        && saturation == other.saturation
        && shcompr == other.shcompr
        && hlcompr == other.hlcompr
        && hlbl == other.hlbl
        && hlth == other.hlth
        && hlcomprthresh == other.hlcomprthresh
        && histmatching == other.histmatching
        && fromHistMatching == other.fromHistMatching
        && clampOOG == other.clampOOG;
}

bool ToneCurveParams::operator !=(const ToneCurveParams& other) const
{
    return !(*this == other);
}

LCurveParams::LCurveParams() :
    enabled(false),
    lcurve{
        DCT_Linear
    },
    acurve{
        DCT_Linear
    },
    bcurve{
        DCT_Linear
    },
    cccurve{
        DCT_Linear
    },
    chcurve{
        FCT_Linear
    },
    lhcurve{
        FCT_Linear
    },
    hhcurve{
        FCT_Linear
    },
    lccurve{
        DCT_Linear
    },
    clcurve{
        DCT_Linear
    },
    brightness(0),
    contrast(0),
    chromaticity(0),
    gamutmunselmethod("MUN"),
    rstprotection(0),
    lcredsk(true)
{
}

bool LCurveParams::operator ==(const LCurveParams& other) const
{
    return
        enabled == other.enabled
        && lcurve == other.lcurve
        && acurve == other.acurve
        && bcurve == other.bcurve
        && cccurve == other.cccurve
        && chcurve == other.chcurve
        && lhcurve == other.lhcurve
        && hhcurve == other.hhcurve
        && lccurve == other.lccurve
        && clcurve == other.clcurve
        && brightness == other.brightness
        && contrast == other.contrast
        && chromaticity == other.chromaticity
        && gamutmunselmethod == other.gamutmunselmethod
        && rstprotection == other.rstprotection
        && lcredsk == other.lcredsk;
}

bool LCurveParams::operator !=(const LCurveParams& other) const
{
    return !(*this == other);
}

RGBCurvesParams::RGBCurvesParams() :
    enabled(false),
    lumamode(false),
    rcurve{
        DCT_Linear
    },
    gcurve{
        DCT_Linear
    },
    bcurve{
        DCT_Linear
    }
{
}

bool RGBCurvesParams::operator ==(const RGBCurvesParams& other) const
{
    return
        enabled == other.enabled
        && lumamode == other.lumamode
        && rcurve == other.rcurve
        && gcurve == other.gcurve
        && bcurve == other.bcurve;
}

bool RGBCurvesParams::operator !=(const RGBCurvesParams& other) const
{
    return !(*this == other);
}

LocalContrastParams::LocalContrastParams():
    enabled(false),
    radius(80),
    amount(0.2),
    darkness(1.0),
    lightness(1.0)
{
}

bool LocalContrastParams::operator==(const LocalContrastParams &other) const
{
    return
        enabled == other.enabled
        && radius == other.radius
        && amount == other.amount
        && darkness == other.darkness
        && lightness == other.lightness;
}

bool LocalContrastParams::operator!=(const LocalContrastParams &other) const
{
    return !(*this == other);
}

const double ColorToningParams::LABGRID_CORR_MAX = 12000.f;
const double ColorToningParams::LABGRID_CORR_SCALE = 3.f;

ColorToningParams::LabCorrectionRegion::LabCorrectionRegion():
    a(0),
    b(0),
    saturation(0),
    slope(1),
    offset(0),
    power(1),
    hueMask{
        FCT_MinMaxCPoints,
        0.166666667,
        1.,
        0.35,
        0.35,
        0.8287775246,
        1.,
        0.35,
        0.35
    },
    chromaticityMask{
        FCT_MinMaxCPoints,
        0.,
        1.,
        0.35,
        0.35,
        1.,
        1.,
        0.35,
        0.35
    },
    lightnessMask{
        FCT_MinMaxCPoints,
        0.,
        1.,
        0.35,
        0.35,
        1.,
        1.,
        0.35,
        0.35
    },
    maskBlur(0),
    channel(ColorToningParams::LabCorrectionRegion::CHAN_ALL)
{
}

bool ColorToningParams::LabCorrectionRegion::operator==(const LabCorrectionRegion &other) const
{
    return
        a == other.a
        && b == other.b
        && saturation == other.saturation
        && slope == other.slope
        && offset == other.offset
        && power == other.power
        && hueMask == other.hueMask
        && chromaticityMask == other.chromaticityMask
        && lightnessMask == other.lightnessMask
        && maskBlur == other.maskBlur
        && channel == other.channel;
}

bool ColorToningParams::LabCorrectionRegion::operator!=(const LabCorrectionRegion &other) const
{
    return !(*this == other);
}

ColorToningParams::ColorToningParams() :
    enabled(false),
    autosat(true),
    opacityCurve{
        FCT_MinMaxCPoints,
        0.00,
        0.3,
        0.35,
        0.00,
        0.25,
        0.8,
        0.35,
        0.35,
        0.70,
        0.8,
        0.35,
        0.35,
        1.00,
        0.3,
        0.00,
        0.00
    },
    colorCurve{
        FCT_MinMaxCPoints,
        0.050,
        0.62,
        0.25,
        0.25,
        0.585,
        0.11,
        0.25,
        0.25
    },
    satProtectionThreshold(30),
    saturatedOpacity(80),
    strength(50),
    balance(0),
    hlColSat(60, 80, false),
    shadowsColSat(80, 208, false),
    clcurve{
        DCT_NURBS,
        0.00,
        0.00,
        0.35,
        0.65,
        1.00,
        1.00
    },
    cl2curve{
        DCT_NURBS,
        0.00,
        0.00,
        0.35,
        0.65,
        1.00,
        1.00
    },
    method("LabRegions"),
    twocolor("Std"),
    redlow(0.0),
    greenlow(0.0),
    bluelow(0.0),
    redmed(0.0),
    greenmed(0.0),
    bluemed(0.0),
    redhigh(0.0),
    greenhigh(0.0),
    bluehigh(0.0),
    satlow(0.0),
    sathigh(0.0),
    lumamode(true),
    labgridALow(0.0),
    labgridBLow(0.0),
    labgridAHigh(0.0),
    labgridBHigh(0.0),
    labregions{LabCorrectionRegion()},
    labregionsShowMask(-1)
{
}

bool ColorToningParams::operator ==(const ColorToningParams& other) const
{
    return
        enabled == other.enabled
        && autosat == other.autosat
        && opacityCurve == other.opacityCurve
        && colorCurve == other.colorCurve
        && satProtectionThreshold == other.satProtectionThreshold
        && saturatedOpacity == other.saturatedOpacity
        && strength == other.strength
        && balance == other.balance
        && hlColSat == other.hlColSat
        && shadowsColSat == other.shadowsColSat
        && clcurve == other.clcurve
        && cl2curve == other.cl2curve
        && method == other.method
        && twocolor == other.twocolor
        && redlow == other.redlow
        && greenlow == other.greenlow
        && bluelow == other.bluelow
        && redmed == other.redmed
        && greenmed == other.greenmed
        && bluemed == other.bluemed
        && redhigh == other.redhigh
        && greenhigh == other.greenhigh
        && bluehigh == other.bluehigh
        && satlow == other.satlow
        && sathigh == other.sathigh
        && lumamode == other.lumamode
        && labgridALow == other.labgridALow
        && labgridBLow == other.labgridBLow
        && labgridAHigh == other.labgridAHigh
        && labgridBHigh == other.labgridBHigh
        && labregions == other.labregions
        && labregionsShowMask == other.labregionsShowMask;
}

bool ColorToningParams::operator !=(const ColorToningParams& other) const
{
    return !(*this == other);
}

void ColorToningParams::mixerToCurve(std::vector<double>& colorCurve, std::vector<double>& opacityCurve) const
{
    // check if non null first
    if (!redlow && !greenlow && !bluelow && !redmed && !greenmed && !bluemed && !redhigh && !greenhigh && !bluehigh) {
        colorCurve.resize(1);
        colorCurve.at(0) = FCT_Linear;
        opacityCurve.resize(1);
        opacityCurve.at(0) = FCT_Linear;
        return;
    }

    float low[3]; // RGB color for shadows
    float med[3]; // RGB color for mid-tones
    float high[3]; // RGB color for highlights
    float lowSat = 0.f;
    float medSat = 0.f;
    float highSat = 0.f;
    float minTmp, maxTmp;

    // Fill the shadow mixer values of the Color TOning tool
    low[0] = float (redlow) / 100.f;  // [-1. ; +1.]
    low[1] = float (greenlow) / 100.f; // [-1. ; +1.]
    low[2] = float (bluelow) / 100.f;  // [-1. ; +1.]
    minTmp = min<float> (low[0], low[1], low[2]);
    maxTmp = max<float> (low[0], low[1], low[2]);

    if (maxTmp - minTmp > 0.005f) {
        float v[3];
        lowSat = (maxTmp - minTmp) / 2.f;

        if (low[0] == minTmp) {
            v[0] = 0.f;
        } else if (low[1] == minTmp) {
            v[1] = 0.f;
        } else if (low[2] == minTmp) {
            v[2] = 0.f;
        }

        if (low[0] == maxTmp) {
            v[0] = 1.f;
        } else if (low[1] == maxTmp) {
            v[1] = 1.f;
        } else if (low[2] == maxTmp) {
            v[2] = 1.f;
        }

        if (low[0] != minTmp && low[0] != maxTmp) {
            v[0] = (low[0] - minTmp) / (maxTmp - minTmp);
        } else if (low[1] != minTmp && low[1] != maxTmp) {
            v[1] = (low[1] - minTmp) / (maxTmp - minTmp);
        } else if (low[2] != minTmp && low[2] != maxTmp) {
            v[2] = (low[2] - minTmp) / (maxTmp - minTmp);
        }

        low[0] = v[0];
        low[1] = v[1];
        low[2] = v[2];
    } else {
        low[0] = low[1] = low[2] = 1.f;
    }

    // Fill the mid-tones mixer values of the Color TOning tool
    med[0] = float (redmed) / 100.f;  // [-1. ; +1.]
    med[1] = float (greenmed) / 100.f; // [-1. ; +1.]
    med[2] = float (bluemed) / 100.f;  // [-1. ; +1.]
    minTmp = min<float> (med[0], med[1], med[2]);
    maxTmp = max<float> (med[0], med[1], med[2]);

    if (maxTmp - minTmp > 0.005f) {
        float v[3];
        medSat = (maxTmp - minTmp) / 2.f;

        if (med[0] == minTmp) {
            v[0] = 0.f;
        } else if (med[1] == minTmp) {
            v[1] = 0.f;
        } else if (med[2] == minTmp) {
            v[2] = 0.f;
        }

        if (med[0] == maxTmp) {
            v[0] = 1.f;
        } else if (med[1] == maxTmp) {
            v[1] = 1.f;
        } else if (med[2] == maxTmp) {
            v[2] = 1.f;
        }

        if (med[0] != minTmp && med[0] != maxTmp) {
            v[0] = (med[0] - minTmp) / (maxTmp - minTmp);
        } else if (med[1] != minTmp && med[1] != maxTmp) {
            v[1] = (med[1] - minTmp) / (maxTmp - minTmp);
        } else if (med[2] != minTmp && med[2] != maxTmp) {
            v[2] = (med[2] - minTmp) / (maxTmp - minTmp);
        }

        med[0] = v[0];
        med[1] = v[1];
        med[2] = v[2];
    } else {
        med[0] = med[1] = med[2] = 1.f;
    }

    // Fill the highlight mixer values of the Color TOning tool
    high[0] = float (redhigh) / 100.f;   // [-1. ; +1.]
    high[1] = float (greenhigh) / 100.f; // [-1. ; +1.]
    high[2] = float (bluehigh) / 100.f;  // [-1. ; +1.]
    minTmp = min<float> (high[0], high[1], high[2]);
    maxTmp = max<float> (high[0], high[1], high[2]);

    if (maxTmp - minTmp > 0.005f) {
        float v[3];
        highSat = (maxTmp - minTmp) / 2.f;

        if (high[0] == minTmp) {
            v[0] = 0.f;
        } else if (high[1] == minTmp) {
            v[1] = 0.f;
        } else if (high[2] == minTmp) {
            v[2] = 0.f;
        }

        if (high[0] == maxTmp) {
            v[0] = 1.f;
        } else if (high[1] == maxTmp) {
            v[1] = 1.f;
        } else if (high[2] == maxTmp) {
            v[2] = 1.f;
        }

        if (high[0] != minTmp && high[0] != maxTmp) {
            v[0] = (high[0] - minTmp) / (maxTmp - minTmp);
        } else if (high[1] != minTmp && high[1] != maxTmp) {
            v[1] = (high[1] - minTmp) / (maxTmp - minTmp);
        } else if (high[2] != minTmp && high[2] != maxTmp) {
            v[2] = (high[2] - minTmp) / (maxTmp - minTmp);
        }

        high[0] = v[0];
        high[1] = v[1];
        high[2] = v[2];
    } else {
        high[0] = high[1] = high[2] = 1.f;
    }

    const double xPosLow  = 0.1;
    const double xPosMed  = 0.4;
    const double xPosHigh = 0.7;

    colorCurve.resize(medSat != 0.f ? 13 : 9);
    colorCurve.at(0) = FCT_MinMaxCPoints;
    opacityCurve.resize(13);
    opacityCurve.at(0) = FCT_MinMaxCPoints;

    float h, s, l;
    int idx = 1;

    if (lowSat == 0.f) {
        if (medSat != 0.f) {
            Color::rgb2hsl(med[0], med[1], med[2], h, s, l);
        } else { // highSat can't be null if the 2 other ones are!
            Color::rgb2hsl(high[0], high[1], high[2], h, s, l);
        }
    } else {
        Color::rgb2hsl(low[0], low[1], low[2], h, s, l);
    }

    colorCurve.at(idx++) = xPosLow;
    colorCurve.at(idx++) = h;
    colorCurve.at(idx++) = 0.35;
    colorCurve.at(idx++) = 0.35;

    if (medSat != 0.f) {
        Color::rgb2hsl(med[0], med[1], med[2], h, s, l);
        colorCurve.at(idx++) = xPosMed;
        colorCurve.at(idx++) = h;
        colorCurve.at(idx++) = 0.35;
        colorCurve.at(idx++) = 0.35;
    }

    if (highSat == 0.f) {
        if (medSat != 0.f) {
            Color::rgb2hsl(med[0], med[1], med[2], h, s, l);
        } else { // lowSat can't be null if the 2 other ones are!
            Color::rgb2hsl(low[0], low[1], low[2], h, s, l);
        }
    } else {
        Color::rgb2hsl(high[0], high[1], high[2], h, s, l);
    }

    colorCurve.at(idx++) = xPosHigh;
    colorCurve.at(idx++) = h;
    colorCurve.at(idx++) = 0.35;
    colorCurve.at(idx)   = 0.35;

    opacityCurve.at(1)  = xPosLow;
    opacityCurve.at(2)  = double (lowSat);
    opacityCurve.at(3)  = 0.35;
    opacityCurve.at(4)  = 0.35;
    opacityCurve.at(5)  = xPosMed;
    opacityCurve.at(6)  = double (medSat);
    opacityCurve.at(7)  = 0.35;
    opacityCurve.at(8)  = 0.35;
    opacityCurve.at(9)  = xPosHigh;
    opacityCurve.at(10) = double (highSat);
    opacityCurve.at(11) = 0.35;
    opacityCurve.at(12) = 0.35;
}

void ColorToningParams::slidersToCurve(std::vector<double>& colorCurve, std::vector<double>& opacityCurve) const
{
    if (hlColSat.getBottom() == 0 && shadowsColSat.getBottom() == 0) { // if both opacity are null, set both curves to Linear
        colorCurve.resize(1);
        colorCurve.at(0) = FCT_Linear;
        opacityCurve.resize(1);
        opacityCurve.at(0) = FCT_Linear;
        return;
    }

    colorCurve.resize(9);
    colorCurve.at(0) = FCT_MinMaxCPoints;
    colorCurve.at(1) = 0.26 + 0.12 * double (balance) / 100.;
    colorCurve.at(2) = double (shadowsColSat.getTop()) / 360.;
    colorCurve.at(3) = 0.35;
    colorCurve.at(4) = 0.35;
    colorCurve.at(5) = 0.64 + 0.12 * double (balance) / 100.;
    colorCurve.at(6) = double (hlColSat.getTop()) / 360.;
    colorCurve.at(7) = 0.35;
    colorCurve.at(8) = 0.35;

    opacityCurve.resize(9);
    opacityCurve.at(0) = FCT_MinMaxCPoints;
    opacityCurve.at(1) = colorCurve.at(1);
    opacityCurve.at(2) = double (shadowsColSat.getBottom()) / 100.;
    opacityCurve.at(3) = 0.35;
    opacityCurve.at(4) = 0.35;
    opacityCurve.at(5) = colorCurve.at(5);
    opacityCurve.at(6) = double (hlColSat.getBottom()) / 100.;
    opacityCurve.at(7) = 0.35;
    opacityCurve.at(8) = 0.35;
}

void ColorToningParams::getCurves(ColorGradientCurve& colorCurveLUT, OpacityCurve& opacityCurveLUT, const double xyz_rgb[3][3], bool& opautili) const
{
    float satur = 0.8f;
    float lumin = 0.5f; //middle of luminance for optimization of gamut - no real importance...as we work in XYZ and gamut control

    // Transform slider values to control points
    std::vector<double> cCurve, oCurve;

    if (method == "RGBSliders" || method == "Splitlr") {
        slidersToCurve(cCurve, oCurve);
    } else if (method == "Splitco") {
        mixerToCurve(cCurve, oCurve);
    } else {
        cCurve = this->colorCurve;
        oCurve = this->opacityCurve;
    }

    if (method == "Lab") {
        if (twocolor == "Separ") {
            satur = 0.9f;
        }

        if (twocolor == "All" || twocolor == "Two") {
            satur = 0.9f;
        }

        colorCurveLUT.SetXYZ(cCurve, xyz_rgb, satur, lumin);
        opacityCurveLUT.Set(oCurve, opautili);
    } else if (method == "Splitlr" || method == "Splitco") {
        colorCurveLUT.SetXYZ(cCurve, xyz_rgb, satur, lumin);
        opacityCurveLUT.Set(oCurve, opautili);
    } else if (method.substr(0, 3) == "RGB") {
        colorCurveLUT.SetRGB(cCurve);
        opacityCurveLUT.Set(oCurve, opautili);
    }
}

SharpeningParams::SharpeningParams() :
    enabled(false),
    contrast(20.0),
    autoContrast(false),
    blurradius(0.2),
    gamma(1.0),
    radius(0.5),
    amount(200),
    threshold(20, 80, 2000, 1200, false),
    edgesonly(false),
    edges_radius(1.9),
    edges_tolerance(1800),
    halocontrol(false),
    halocontrol_amount(85),
    method("usm"),
    deconvamount(100),
    deconvradius(0.75),
    deconviter(30),
    deconvdamping(0)
{
}

bool SharpeningParams::operator ==(const SharpeningParams& other) const
{
    return
        enabled == other.enabled
        && contrast == other.contrast
        && blurradius == other.blurradius
        && gamma == other.gamma
        && radius == other.radius
        && amount == other.amount
        && threshold == other.threshold
        && autoContrast == other.autoContrast
        && edgesonly == other.edgesonly
        && edges_radius == other.edges_radius
        && edges_tolerance == other.edges_tolerance
        && halocontrol == other.halocontrol
        && halocontrol_amount == other.halocontrol_amount
        && method == other.method
        && deconvamount == other.deconvamount
        && deconvradius == other.deconvradius
        && deconviter == other.deconviter
        && deconvdamping == other.deconvdamping;
}

bool SharpeningParams::operator !=(const SharpeningParams& other) const
{
    return !(*this == other);
}

SharpenEdgeParams::SharpenEdgeParams() :
    enabled(false),
    passes(2),
    amount(50.0),
    threechannels(false)
{
}

bool SharpenEdgeParams::operator ==(const SharpenEdgeParams& other) const
{
    return
        enabled == other.enabled
        && passes == other.passes
        && amount == other.amount
        && threechannels == other.threechannels;
}

bool SharpenEdgeParams::operator !=(const SharpenEdgeParams& other) const
{
    return !(*this == other);
}

SharpenMicroParams::SharpenMicroParams() :
    enabled(false),
    matrix(false),
    amount(20.0),
    contrast(20.0),
    uniformity(5)
{
}

bool SharpenMicroParams::operator ==(const SharpenMicroParams& other) const
{
    return
        enabled == other.enabled
        && matrix == other.matrix
        && amount == other.amount
        && contrast == other.contrast
        && uniformity == other.uniformity;
}

bool SharpenMicroParams::operator !=(const SharpenMicroParams& other) const
{
    return !(*this == other);
}

VibranceParams::VibranceParams() :
    enabled(false),
    pastels(0),
    saturated(0),
    psthreshold(0, 75, false),
    protectskins(false),
    avoidcolorshift(true),
    pastsattog(true),
    skintonescurve{
        DCT_Linear
    }
{
}

bool VibranceParams::operator ==(const VibranceParams& other) const
{
    return
        enabled == other.enabled
        && pastels == other.pastels
        && saturated == other.saturated
        && psthreshold == other.psthreshold
        && protectskins == other.protectskins
        && avoidcolorshift == other.avoidcolorshift
        && pastsattog == other.pastsattog
        && skintonescurve == other.skintonescurve;
}

bool VibranceParams::operator !=(const VibranceParams& other) const
{
    return !(*this == other);
}

WBParams::WBParams() :
    enabled(true),
    method("Camera"),
    temperature(6504),
    green(1.0),
    equal(1.0),
    tempBias(0.0),
    observer(ColorTemp::DEFAULT_OBSERVER),
    itcwb_green(0.),//slider
    itcwb_rgreen(1),//keep for settings
    itcwb_nopurple(false),//keep for settings
    itcwb_alg(false),//checkbox
    itcwb_prim("beta"),//combobox
    itcwb_sampling(false),//keep for 5.9 and for settings
    compat_version(WBParams::CURRENT_COMPAT_VERSION)

{
}

bool WBParams::isPanningRelatedChange(const WBParams& other) const
{
    return
        !(
            enabled == other.enabled
            && (
                (
                    method == "Camera"
                    && other.method == "Camera"
                )
            || (
                method == other.method
                && temperature == other.temperature
                && green == other.green
                && equal == other.equal
                && tempBias == other.tempBias
                && observer == other.observer
                && itcwb_green == other.itcwb_green
                && itcwb_prim == other.itcwb_prim
                && itcwb_alg == other.itcwb_alg
                && compat_version == other.compat_version

            )
        )
    );
}

bool WBParams::operator ==(const WBParams& other) const
{
    return
        enabled == other.enabled
        && method == other.method
        && temperature == other.temperature
        && green == other.green
        && equal == other.equal
        && tempBias == other.tempBias
        && observer == other.observer
        && itcwb_green == other.itcwb_green
        && itcwb_rgreen == other.itcwb_rgreen
        && itcwb_nopurple == other.itcwb_nopurple
        && itcwb_alg == other.itcwb_alg
        && itcwb_prim == other.itcwb_prim
        && itcwb_sampling == other.itcwb_sampling
        && compat_version == other.compat_version;

}

bool WBParams::operator !=(const WBParams& other) const
{
    return !(*this == other);
}

const std::vector<WBEntry>& WBParams::getWbEntries()
{
    static const std::vector<WBEntry> wb_entries = {
        {"Camera",               WBEntry::Type::CAMERA,      M("TP_WBALANCE_CAMERA"),         0, 1.f,   1.f,   0.f},
        {"autitcgreen",       	 WBEntry::Type::AUTO,        M("TP_WBALANCE_AUTOITCGREEN"),   0, 1.f,    1.f,    0.f},
        {"autold",               WBEntry::Type::AUTO,        M("TP_WBALANCE_AUTOOLD"),        0, 1.f,   1.f,   0.f},
        {"Daylight",             WBEntry::Type::DAYLIGHT,    M("TP_WBALANCE_DAYLIGHT"),    5300, 1.f,   1.f,   0.f},
        {"Cloudy",               WBEntry::Type::CLOUDY,      M("TP_WBALANCE_CLOUDY"),      6200, 1.f,   1.f,   0.f},
        {"Shade",                WBEntry::Type::SHADE,       M("TP_WBALANCE_SHADE"),       7600, 1.f,   1.f,   0.f},
        {"Water 1",              WBEntry::Type::WATER,       M("TP_WBALANCE_WATER1"),     35000, 0.3f,  1.1f,  0.f},
        {"Water 2",              WBEntry::Type::WATER,       M("TP_WBALANCE_WATER2"),     48000, 0.63f, 1.38f, 0.f},
        {"Tungsten",             WBEntry::Type::TUNGSTEN,    M("TP_WBALANCE_TUNGSTEN"),    2856, 1.f,   1.f,   0.f},
        {"Fluo F1",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO1"),       6430, 1.f,   1.f,   0.f},
        {"Fluo F2",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO2"),       4230, 1.f,   1.f,   0.f},
        {"Fluo F3",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO3"),       3450, 1.f,   1.f,   0.f},
        {"Fluo F4",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO4"),       2940, 1.f,   1.f,   0.f},
        {"Fluo F5",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO5"),       6350, 1.f,   1.f,   0.f},
        {"Fluo F6",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO6"),       4150, 1.f,   1.f,   0.f},
        {"Fluo F7",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO7"),       6500, 1.f,   1.f,   0.f},
        {"Fluo F8",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO8"),       5020, 1.f,   1.f,   0.f},
        {"Fluo F9",              WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO9"),       4330, 1.f,   1.f,   0.f},
        {"Fluo F10",             WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO10"),      5300, 1.f,   1.f,   0.f},
        {"Fluo F11",             WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO11"),      4000, 1.f,   1.f,   0.f},
        {"Fluo F12",             WBEntry::Type::FLUORESCENT, M("TP_WBALANCE_FLUO12"),      3000, 1.f,   1.f,   0.f},
        {"HMI Lamp",             WBEntry::Type::LAMP,        M("TP_WBALANCE_HMI"),         4800, 1.f,   1.f,   0.f},
        {"GTI Lamp",             WBEntry::Type::LAMP,        M("TP_WBALANCE_GTI"),         5000, 1.f,   1.f,   0.f},
        {"JudgeIII Lamp",        WBEntry::Type::LAMP,        M("TP_WBALANCE_JUDGEIII"),    5100, 1.f,   1.f,   0.f},
        {"Solux Lamp 3500K",     WBEntry::Type::LAMP,        M("TP_WBALANCE_SOLUX35"),     3480, 1.f,   1.f,   0.f},
        {"Solux Lamp 4100K",     WBEntry::Type::LAMP,        M("TP_WBALANCE_SOLUX41"),     3930, 1.f,   1.f,   0.f},
        {"Solux Lamp 4700K",     WBEntry::Type::LAMP,        M("TP_WBALANCE_SOLUX47"),     4700, 1.f,   1.f,   0.f},
        {"NG Solux Lamp 4700K",  WBEntry::Type::LAMP,        M("TP_WBALANCE_SOLUX47_NG"),  4480, 1.f,   1.f,   0.f},
        {"LED LSI Lumelex 2040", WBEntry::Type::LED,         M("TP_WBALANCE_LED_LSI"),     2970, 1.f,   1.f,   0.f},
        {"LED CRS SP12 WWMR16",  WBEntry::Type::LED,         M("TP_WBALANCE_LED_CRS"),     3050, 1.f,   1.f,   0.f},
        {"Flash 5500K",          WBEntry::Type::FLASH,       M("TP_WBALANCE_FLASH55"),     5500, 1.f,   1.f,   0.f},
        {"Flash 6000K",          WBEntry::Type::FLASH,       M("TP_WBALANCE_FLASH60"),     6000, 1.f,   1.f,   0.f},
        {"Flash 6500K",          WBEntry::Type::FLASH,       M("TP_WBALANCE_FLASH65"),     6500, 1.f,   1.f,   0.f},
        // Should remain the last one
        {"Custom",               WBEntry::Type::CUSTOM,      M("TP_WBALANCE_CUSTOM"),        0, 1.f,   1.f,   0.f}
    };
    return wb_entries;
}

DefringeParams::DefringeParams() :
    enabled(false),
    radius(2.0),
    threshold(13),
    huecurve{
        FCT_MinMaxCPoints,
        0.166666667,
        0.,
        0.35,
        0.35,
        0.347,
        0.,
        0.35,
        0.35,
        0.513667426,
        0,
        0.35,
        0.35,
        0.668944571,
        0.,
        0.35,
        0.35,
        0.8287775246,
        0.97835991,
        0.35,
        0.35,
        0.9908883827,
        0.,
        0.35,
        0.35
    }
{
}

bool DefringeParams::operator ==(const DefringeParams& other) const
{
    return
        enabled == other.enabled
        && radius == other.radius
        && threshold == other.threshold
        && huecurve == other.huecurve;
}

bool DefringeParams::operator !=(const DefringeParams& other) const
{
    return !(*this == other);
}

ImpulseDenoiseParams::ImpulseDenoiseParams() :
    enabled(false),
    thresh(50)
{
}

bool ImpulseDenoiseParams::operator ==(const ImpulseDenoiseParams& other) const
{
    return
        enabled == other.enabled
        && thresh == other.thresh;
}

bool ImpulseDenoiseParams::operator !=(const ImpulseDenoiseParams& other) const
{
    return !(*this == other);
}

DirPyrDenoiseParams::DirPyrDenoiseParams() :
    lcurve{
        FCT_MinMaxCPoints,
        0.05,
        0.15,
        0.35,
        0.35,
        0.55,
        0.04,
        0.35,
        0.35
    },
    cccurve{
        FCT_MinMaxCPoints,
        0.05,
        0.50,
        0.35,
        0.35,
        0.35,
        0.05,
        0.35,
        0.35
    },
    enabled(false),
    enhance(false),
    median(false),
    perform(false),
    luma(0),
    Ldetail(0),
    chroma(15),
    redchro(0),
    bluechro(0),
    autoGain(true),
    gamma(1.7),
    dmethod("Lab"),
    Lmethod("SLI"),
    Cmethod("MAN"),
    C2method("AUTO"),
    smethod("shal"),
    medmethod("soft"),
    methodmed("none"),
    rgbmethod("soft"),
    passes(1)
{
}

bool DirPyrDenoiseParams::operator ==(const DirPyrDenoiseParams& other) const
{
    return
        lcurve == other.lcurve
        && cccurve == other.cccurve
        && enabled == other.enabled
        && enhance == other.enhance
        && median == other.median
        && perform == other.perform
        && luma == other.luma
        && Ldetail == other.Ldetail
        && chroma == other.chroma
        && redchro == other.redchro
        && bluechro == other.bluechro
        && autoGain == other.autoGain
        && gamma == other.gamma
        && dmethod == other.dmethod
        && Lmethod == other.Lmethod
        && Cmethod == other.Cmethod
        && C2method == other.C2method
        && smethod == other.smethod
        && medmethod == other.medmethod
        && methodmed == other.methodmed
        && rgbmethod == other.rgbmethod
        && passes == other.passes;
}

bool DirPyrDenoiseParams::operator !=(const DirPyrDenoiseParams& other) const
{
    return !(*this == other);
}

void DirPyrDenoiseParams::getCurves(NoiseCurve &lCurve, NoiseCurve &cCurve) const
{
    lCurve.Set(this->lcurve);
    cCurve.Set(this->cccurve);
}

EPDParams::EPDParams() :
    enabled(false),
    strength(0.5),
    gamma(1.0),
    edgeStopping(1.4),
    scale(1.0),
    reweightingIterates(0)
{
}

bool EPDParams::operator ==(const EPDParams& other) const
{
    return
        enabled == other.enabled
        && strength == other.strength
        && gamma == other.gamma
        && edgeStopping == other.edgeStopping
        && scale == other.scale
        && reweightingIterates == other.reweightingIterates;
}

bool EPDParams::operator !=(const EPDParams& other) const
{
    return !(*this == other);
}

SpotEntry::SpotEntry() :
    radius(25),
    feather(1.f),
    opacity(1.f)
{
}

float SpotEntry::getFeatherRadius() const
{
    return radius * (1.f + feather);
}

bool SpotEntry::operator ==(const SpotEntry& other) const
{
    return other.sourcePos == sourcePos && other.targetPos == targetPos &&
           other.radius == radius && other.feather == feather && other.opacity == opacity;
}

bool SpotEntry::operator !=(const SpotEntry& other) const
{
    return other.sourcePos != sourcePos || other.targetPos != targetPos ||
           other.radius != radius || other.feather != feather || other.opacity != opacity;
}

SpotParams::SpotParams() :
    enabled(false)
{
    entries.clear ();
}

bool SpotParams::operator ==(const SpotParams& other) const
{
    return enabled == other.enabled && entries == other.entries;
}

bool SpotParams::operator !=(const SpotParams& other) const
{
    return !(*this == other);
}


FattalToneMappingParams::FattalToneMappingParams() :
    enabled(false),
    threshold(30),
    amount(20),
    anchor(50)
{
}

bool FattalToneMappingParams::operator ==(const FattalToneMappingParams& other) const
{
    return
        enabled == other.enabled
        && threshold == other.threshold
        && amount == other.amount
        && anchor == other.anchor;
}

bool FattalToneMappingParams::operator !=(const FattalToneMappingParams& other) const
{
    return !(*this == other);
}

SHParams::SHParams() :
    enabled(false),
    highlights(0),
    htonalwidth(70),
    shadows(0),
    stonalwidth(30),
    radius(40),
    lab(false)
{
}

bool SHParams::operator ==(const SHParams& other) const
{
    return
        enabled == other.enabled
        && highlights == other.highlights
        && htonalwidth == other.htonalwidth
        && shadows == other.shadows
        && stonalwidth == other.stonalwidth
        && radius == other.radius
        && lab == other.lab;
}

bool SHParams::operator !=(const SHParams& other) const
{
    return !(*this == other);
}



CGParams::CGParams() :
    enabled(false),
    th_c(0.815),
    th_m(0.803),
    th_y(0.880),
    d_c(1.147),
    autodc(true),
    d_m(1.264),
    autodm(true),
    d_y(1.312),
    autody(true),
    pwr(1.2),
    colorspace("dcip3"),
    rolloff(true)

{
}

bool CGParams::operator ==(const CGParams& other) const
{
    return
        enabled == other.enabled
        && th_c == other.th_c
        && th_m == other.th_m
        && th_y == other.th_y
        && d_c == other.d_c
        && autodc == other.autodc
        && d_m == other.d_m
        && autodm == other.autodm
        && d_y == other.d_y
        && autody == other.autody
        && pwr == other.pwr
        && colorspace == other.colorspace
        && rolloff == other.rolloff;
}

bool CGParams::operator !=(const CGParams& other) const
{
    return !(*this == other);
}



///
ToneEqualizerParams::ToneEqualizerParams() :
    enabled(false),
    bands{0, 0, 0, 0, 0, 0},
    regularization(0),
    show_colormap(false),
    pivot(0)
{
}

bool ToneEqualizerParams::operator ==(const ToneEqualizerParams &other) const
{
    return
        enabled == other.enabled
        && bands == other.bands
        && regularization == other.regularization
        && show_colormap == other.show_colormap
        && pivot == other.pivot;
}

bool ToneEqualizerParams::operator !=(const ToneEqualizerParams &other) const
{
    return !(*this == other);
}

CropParams::CropParams() :
    enabled(false),
    x(-1),
    y(-1),
    w(15000),
    h(15000),
    fixratio(true),
    ratio("As Image"),
    orientation("As Image")
{
}

bool CropParams::operator ==(const CropParams& other) const
{
    return
        enabled == other.enabled
        && x == other.x
        && y == other.y
        && w == other.w
        && h == other.h
        && fixratio == other.fixratio
        && ratio == other.ratio
        && orientation == other.orientation;
}

bool CropParams::operator !=(const CropParams& other) const
{
    return !(*this == other);
}

void CropParams::mapToResized(int resizedWidth, int resizedHeight, int scale, int& x1, int& x2, int& y1, int& y2) const
{
    x1 = 0, x2 = resizedWidth, y1 = 0, y2 = resizedHeight;

    if (enabled) {
        x1 = min(resizedWidth - 1, max(0, x / scale));
        y1 = min(resizedHeight - 1, max(0, y / scale));
        x2 = min(resizedWidth, max(0, (x + w) / scale));
        y2 = min(resizedHeight, max(0, (y + h) / scale));
    }
}

CropGuideParams::PresetParams::PresetParams()
    : enabled(false), red(1.0), green(1.0), blue(1.0), alpha(0.618)
{
}

bool CropGuideParams::PresetParams::operator==(const PresetParams& other) const
{
    return enabled == other.enabled
        && red == other.red
        && green == other.green
        && blue == other.blue
        && alpha == other.alpha;
}

CropGuideParams::AspectRatioParams::AspectRatioParams(size_t preset_index)
    : enabled(false),
      is_portrait(false),
      preset_index(preset_index),
      red(1.0),
      green(1.0),
      blue(1.0),
      alpha(0.618)
{
}

bool CropGuideParams::AspectRatioParams::operator==(const AspectRatioParams& other) const
{
    return enabled == other.enabled
        && is_portrait == other.is_portrait
        && preset_index == other.preset_index
        && red == other.red
        && green == other.green
        && blue == other.blue
        && alpha == other.alpha;
}

CropGuideParams::CropGuideParams()
    : enabled(true),
      mirror_golden_triangle(false),
      rotate_golden_ratio(false),
      mirror_golden_ratio(false),
      bleed(0),
      basis(Basis::SCALE)
{
}

bool CropGuideParams::operator==(const CropGuideParams& other) const
{
    return enabled == other.enabled
        && presets == other.presets
        && mirror_golden_triangle == other.mirror_golden_triangle
        && rotate_golden_ratio == other.rotate_golden_ratio
        && mirror_golden_ratio == other.mirror_golden_ratio
        && aspect_ratios == other.aspect_ratios
        && bleed == other.bleed
        && basis == other.basis;
}

CoarseTransformParams::CoarseTransformParams() :
    rotate(0),
    hflip(false),
    vflip(false)
{
}

bool CoarseTransformParams::operator ==(const CoarseTransformParams& other) const
{
    return
        rotate == other.rotate
        && hflip == other.hflip
        && vflip == other.vflip;
}

bool CoarseTransformParams::operator !=(const CoarseTransformParams& other) const
{
    return !(*this == other);
}

CommonTransformParams::CommonTransformParams() {}

double CommonTransformParams::getScale() const
{
  return autofill ? 1.0 : scale;
}

double CommonTransformParams::getScaleHorizontally() const
{
  return autofill ? 1.0 : scale_horizontally;
}

double CommonTransformParams::getScaleVertically() const
{
  return autofill ? 1.0 : scale_vertically;
}

bool CommonTransformParams::operator ==(const CommonTransformParams& other) const
{
    return method == other.method && autofill == other.autofill && std::abs(scale - other.scale) < 1e-6 && std::abs(scale_horizontally - other.scale_horizontally) < 1e-6 && std::abs(scale_vertically - other.scale_vertically) < 1e-6;
}

bool CommonTransformParams::operator !=(const CommonTransformParams& other) const
{
    return !(*this == other);
}

RotateParams::RotateParams() :
    degree(0.0)
{
}

bool RotateParams::operator ==(const RotateParams& other) const
{
    return degree == other.degree;
}

bool RotateParams::operator !=(const RotateParams& other) const
{
    return !(*this == other);
}

DistortionParams::DistortionParams() {}

bool DistortionParams::operator ==(const DistortionParams& other) const
{
    return amount == other.amount && defish == other.defish && focal_length == other.focal_length;
}

bool DistortionParams::operator !=(const DistortionParams& other) const
{
    return !(*this == other);
}

LensProfParams::LensProfParams() :
    lcMode(LcMode::NONE),
    useDist(true),
    useVign(true),
    useCA(false)
{
}

bool LensProfParams::operator ==(const LensProfParams& other) const
{
    return
        lcMode == other.lcMode
        && lcpFile == other.lcpFile
        && useCA == other.useCA
        && lfCameraMake == other.lfCameraMake
        && lfCameraModel == other.lfCameraModel
        && lfLens == other.lfLens
        && useDist == other.useDist
        && useVign == other.useVign;
}

bool LensProfParams::operator !=(const LensProfParams& other) const
{
    return !(*this == other);
}

bool LensProfParams::useLensfun() const
{
    return lcMode == LcMode::LENSFUNAUTOMATCH || lcMode == LcMode::LENSFUNMANUAL;
}

bool LensProfParams::lfAutoMatch() const
{
    return lcMode == LcMode::LENSFUNAUTOMATCH;
}

bool LensProfParams::useLcp() const
{
    return lcMode == LcMode::LCP && lcpFile.length() > 0;
}

bool LensProfParams::lfManual() const
{
    return lcMode == LcMode::LENSFUNMANUAL;
}

bool LensProfParams::useMetadata() const
{
    return lcMode == LcMode::METADATA;
}

const std::vector<const char*>& LensProfParams::getMethodStrings() const
{
    static const std::vector<const char*> method_strings = {
        "none",
        "lfauto",
        "lfmanual",
        "lcp",
        "metadata"
    };
    return method_strings;
}

Glib::ustring LensProfParams::getMethodString(LcMode mode) const
{
    return getMethodStrings()[toUnderlying(mode)];
}

LensProfParams::LcMode LensProfParams::getMethodNumber(const Glib::ustring& mode) const
{
    for (std::vector<const char*>::size_type i = 0; i < getMethodStrings().size(); ++i) {
        if (getMethodStrings()[i] == mode) {
            return static_cast<LcMode>(i);
        }
    }

    return LcMode::NONE;
}

PerspectiveParams::PerspectiveParams() :
    method("simple"),
    render(true),
    horizontal(0.0),
    vertical(0.0),
    camera_crop_factor(0.0),
    camera_focal_length(0.0),
    camera_pitch(0.0),
    camera_roll(0.0),
    camera_shift_horiz(0.0),
    camera_shift_vert(0.0),
    camera_yaw(0.0),
    projection_pitch(0.0),
    projection_rotate(0.0),
    projection_shift_horiz(0.0),
    projection_shift_vert(0.0),
    projection_yaw(0.0)
{
}

bool PerspectiveParams::operator ==(const PerspectiveParams& other) const
{
    return
        method == other.method
        && render == other.render
        && horizontal == other.horizontal
        && vertical == other.vertical
        && camera_focal_length == other.camera_focal_length
        && camera_crop_factor == other.camera_crop_factor
        && camera_pitch == other.camera_pitch
        && camera_roll == other.camera_roll
        && camera_shift_horiz == other.camera_shift_horiz
        && camera_shift_vert == other.camera_shift_vert
        && camera_yaw == other.camera_yaw
        && projection_shift_horiz == other.projection_shift_horiz
        && projection_shift_vert == other.projection_shift_vert
        && projection_rotate == other.projection_rotate
        && projection_pitch == other.projection_pitch
        && projection_yaw == other.projection_yaw
        // Lines could still be equivalent if the vectors aren't, but this is
        // rare and a small issue. Besides, a proper comparison requires lots
        // more code which introduces clutter.
        && control_line_values == other.control_line_values
        && control_line_types == other.control_line_types;
}

bool PerspectiveParams::operator !=(const PerspectiveParams& other) const
{
    return !(*this == other);
}

GradientParams::GradientParams() :
    enabled(false),
    degree(0.0),
    feather(25),
    strength(0.60),
    centerX(0),
    centerY(0)
{
}

bool GradientParams::operator ==(const GradientParams& other) const
{
    return
        enabled == other.enabled
        && degree == other.degree
        && feather == other.feather
        && strength == other.strength
        && centerX == other.centerX
        && centerY == other.centerY;
}

bool GradientParams::operator !=(const GradientParams& other) const
{
    return !(*this == other);
}

PCVignetteParams::PCVignetteParams() :
    enabled(false),
    strength(0.60),
    feather(50),
    roundness(50)
{
}

bool PCVignetteParams::operator ==(const PCVignetteParams& other) const
{
    return
        enabled == other.enabled
        && strength == other.strength
        && feather == other.feather
        && roundness == other.roundness;
}

bool PCVignetteParams::operator !=(const PCVignetteParams& other) const
{
    return !(*this == other);
}

VignettingParams::VignettingParams() :
    amount(0),
    radius(50),
    strength(1),
    centerX(0),
    centerY(0)
{
}

bool VignettingParams::operator ==(const VignettingParams& other) const
{
    return
        amount == other.amount
        && radius == other.radius
        && strength == other.strength
        && centerX == other.centerX
        && centerY == other.centerY;
}

bool VignettingParams::operator !=(const VignettingParams& other) const
{
    return !(*this == other);
}

ChannelMixerParams::ChannelMixerParams() :
    enabled(false),
    red{
        1000,
        0,
        0
    },
    green{
        0,
        1000,
        0
    },
    blue{
        0,
        0,
        1000
    }
{
}

bool ChannelMixerParams::operator ==(const ChannelMixerParams& other) const
{
    if (enabled != other.enabled) {
        return false;
    }

    for (unsigned int i = 0; i < 3; ++i) {
        if (
            red[i] != other.red[i]
            || green[i] != other.green[i]
            || blue[i] != other.blue[i]
        ) {
            return false;
        }
    }

    return true;
}

bool ChannelMixerParams::operator !=(const ChannelMixerParams& other) const
{
    return !(*this == other);
}

BlackWhiteParams::BlackWhiteParams() :
    beforeCurve{
        DCT_Linear
    },
    beforeCurveMode(BlackWhiteParams::TcMode::STD_BW),
    afterCurve{
        DCT_Linear
    },
    afterCurveMode(BlackWhiteParams::TcMode::STD_BW),
    algo("SP"),
    luminanceCurve{
        FCT_Linear
    },
    autoc(false),
    enabledcc(true),
    enabled(false),
    filter("None"),
    setting("RGB-Rel"),
    method("Desaturation"),
    mixerRed(33),
    mixerOrange(33),
    mixerYellow(33),
    mixerGreen(33),
    mixerCyan(33),
    mixerBlue(33),
    mixerMagenta(33),
    mixerPurple(33),
    gammaRed(0),
    gammaGreen(0),
    gammaBlue(0)
{
}

bool BlackWhiteParams::operator ==(const BlackWhiteParams& other) const
{
    return
        beforeCurve == other.beforeCurve
        && beforeCurveMode == other.beforeCurveMode
        && afterCurve == other.afterCurve
        && afterCurveMode == other.afterCurveMode
        && algo == other.algo
        && luminanceCurve == other.luminanceCurve
        && autoc == other.autoc
        && enabledcc == other.enabledcc
        && enabled == other.enabled
        && filter == other.filter
        && setting == other.setting
        && method == other.method
        && mixerRed == other.mixerRed
        && mixerOrange == other.mixerOrange
        && mixerYellow == other.mixerYellow
        && mixerGreen == other.mixerGreen
        && mixerCyan == other.mixerCyan
        && mixerBlue == other.mixerBlue
        && mixerMagenta == other.mixerMagenta
        && mixerPurple == other.mixerPurple
        && gammaRed == other.gammaRed
        && gammaGreen == other.gammaGreen
        && gammaBlue == other.gammaBlue;
}

bool BlackWhiteParams::operator !=(const BlackWhiteParams& other) const
{
    return !(*this == other);
}

CACorrParams::CACorrParams() :
    red(0.0),
    blue(0.0)
{
}

bool CACorrParams::operator ==(const CACorrParams& other) const
{
    return
        red == other.red
        && blue == other.blue;
}

bool CACorrParams::operator !=(const CACorrParams& other) const
{
    return !(*this == other);
}

ResizeParams::ResizeParams() :
    enabled(false),
    scale(1.0),
    appliesTo("Cropped area"),
    method("Lanczos"),
    dataspec(3),
    width(900),
    height(900),
    longedge(900),
    shortedge(900),
    allowUpscaling(false)
{
}

bool ResizeParams::operator ==(const ResizeParams& other) const
{
    return
        enabled == other.enabled
        && scale == other.scale
        && appliesTo == other.appliesTo
        && method == other.method
        && dataspec == other.dataspec
        && width == other.width
        && height == other.height
        && longedge == other.longedge
        && shortedge == other.shortedge
        && allowUpscaling == other.allowUpscaling;
}

bool ResizeParams::operator !=(const ResizeParams& other) const
{
    return !(*this == other);
}

FramingParams::FramingParams() :
    enabled(false),
    framingMethod(FramingMethod::STANDARD),
    aspectRatio(0),
    orientation(Orientation::AS_IMAGE),
    framedWidth(800),
    framedHeight(600),
    allowUpscaling(false),
    borderSizingMethod(BorderSizing::PERCENTAGE),
    basis(Basis::AUTO),
    relativeBorderSize(0.1),
    minSizeEnabled(false),
    minWidth(0),
    minHeight(0),
    absWidth(0),
    absHeight(0),
    borderRed(255),
    borderGreen(255),
    borderBlue(255)
{
}

bool FramingParams::operator ==(const FramingParams& other) const
{
    return
        enabled == other.enabled
        && framingMethod == other.framingMethod
        && aspectRatio == other.aspectRatio
        && orientation == other.orientation
        && framedWidth == other.framedWidth
        && framedHeight == other.framedHeight
        && allowUpscaling == other.allowUpscaling
        && borderSizingMethod == other.borderSizingMethod
        && basis == other.basis
        && relativeBorderSize == other.relativeBorderSize
        && minSizeEnabled == other.minSizeEnabled
        && minWidth == other.minWidth
        && minHeight == other.minHeight
        && absWidth == other.absWidth
        && absHeight == other.absHeight
        && borderRed == other.borderRed
        && borderGreen == other.borderGreen
        && borderBlue == other.borderBlue;
}

bool FramingParams::operator !=(const FramingParams& other) const
{
    return !(*this == other);
}

const Glib::ustring ColorManagementParams::NoICMString = Glib::ustring("No ICM: sRGB output");
const Glib::ustring ColorManagementParams::NoProfileString = Glib::ustring("(none)");

ColorManagementParams::ColorManagementParams() :
    inputProfile("(cameraICC)"),
    toneCurve(false),
    applyLookTable(false),
    applyBaselineExposureOffset(true),
    applyHueSatMap(true),
    dcpIlluminant(0),
    workingProfile("Rec2020"),
    workingTRC(WorkingTrc::NONE),
    wgamut(Wwgamut::NONE),
    will(Illuminant::DEFAULT),
    wprim(Primaries::DEFAULT),
    wcat(Cat::BRAD),
    wGamma(2.4),//gamma sRGB
    wSlope(12.92),
    wapsat(0.5),
    wmidtcie(0.),
    sigmatrc(1.),
    offstrc(1.),
    residtrc(0.),
    wgampower(1.),
    wgamgain(0.),
    pyrwavtrc(2),
    opacityCurveWLI{
        static_cast<double>(FCT_MinMaxCPoints),
        0.0,
        0.50,
        0.35,
        0.35,
        0.50,
        0.70,
        0.35,
        0.35,
        1.00,
        0.50,
        0.35,
        0.35
    },
    wsmoothcie(false),
    wsmoothciesli(0.),
    // These chromaticities correspond to Rec.2020 with a D65 white point.
    // Earlier versions of RawTherapee used ProPhoto RGB (D50) as the default
    // This increases compatibility because 'resetting' with Prophoto's (in 'Abstract profiles') values ​​will cause confusion.
    // This is used by 'Abstract profile' to adjust the primaries and illuminants, but this is not where the Working Profile is selected.
    redx(0.7080),
    redy(0.2920),
    grex(0.1700),
    grey(0.7970),
    blux(0.1310),
    bluy(0.0460),
    redrot(0.),
    redsat(0.),
    grerot(0.),
    gresat(0.),
    blurot(0.),
    blusat(0.),
    refi(0.),
    shiftx(0.),
    shifty(0.),
    preser(0.),
    fbw(false),
    trcExp(false),
    wavExp(false),
    gamut(true),

    labgridcieALow(0.469089),//Rec2020 red = (0.7080+0.1) * 1.81818 - 1
    labgridcieBLow(-0.287273),
    labgridcieAHigh(-0.58000),//Rec2020 blue
    labgridcieBHigh(-0.8180),
    labgridcieGx(-0.509009),//Rec2020 green
    labgridcieGy(0.63090),//
    labgridcieWx(-0.24959),//D65 0.3127 0.329
    labgridcieWy(-0.22000),
    labgridcieMx(0.),
    labgridcieMy(0.),
    aRendIntent(RI_RELATIVE),
    outputProfile(App::get().options().rtSettings.srgb),
    outputIntent(RI_RELATIVE),
    outputBPC(true),
    outputPaperwhiteNits(203.0),
    outputMaxNits(10000.0)
{
}

bool ColorManagementParams::operator ==(const ColorManagementParams& other) const
{
    return
        inputProfile == other.inputProfile
        && toneCurve == other.toneCurve
        && applyLookTable == other.applyLookTable
        && applyBaselineExposureOffset == other.applyBaselineExposureOffset
        && applyHueSatMap == other.applyHueSatMap
        && dcpIlluminant == other.dcpIlluminant
        && workingProfile == other.workingProfile
        && workingTRC == other.workingTRC
        && wgamut == other.wgamut
        && will == other.will
        && wprim == other.wprim
        && wcat == other.wcat
        && wGamma == other.wGamma
        && wSlope == other.wSlope
        && wapsat == other.wapsat
        && wmidtcie == other.wmidtcie
        && sigmatrc == other.sigmatrc
        && offstrc == other.offstrc
        && pyrwavtrc == other.pyrwavtrc
        && wgampower == other.wgampower
        && wgamgain == other.wgamgain
        && residtrc == other.residtrc
        && opacityCurveWLI == other.opacityCurveWLI
        && wsmoothcie == other.wsmoothcie
        && wsmoothciesli == other.wsmoothciesli
        && redx == other.redx
        && redy == other.redy
        && grex == other.grex
        && grey == other.grey
        && blux == other.blux
        && bluy == other.bluy
        && redrot == other.redrot
        && redsat == other.redsat
        && grerot == other.grerot
        && gresat == other.gresat
        && blurot == other.blurot
        && blusat == other.blusat
        
        && refi == other.refi
        && shiftx == other.shiftx
        && shifty == other.shifty
        && labgridcieALow == other.labgridcieALow
        && labgridcieBLow == other.labgridcieBLow
        && labgridcieAHigh == other.labgridcieAHigh
        && labgridcieBHigh == other.labgridcieBHigh
        && labgridcieGx == other.labgridcieGx
        && labgridcieGy == other.labgridcieGy
        && labgridcieWx == other.labgridcieWx
        && labgridcieWy == other.labgridcieWy
        && labgridcieMx == other.labgridcieMx
        && labgridcieMy == other.labgridcieMy
        && preser == other.preser
        && fbw == other.fbw
        && trcExp == other.trcExp
        && wavExp == other.wavExp
        && gamut == other.gamut
        && aRendIntent == other.aRendIntent
        && outputProfile == other.outputProfile
        && outputIntent == other.outputIntent
        && outputBPC == other.outputBPC
        && outputPaperwhiteNits == other.outputPaperwhiteNits
        && outputMaxNits == other.outputMaxNits;
}

void ColorManagementParams::getCurves(
    WavOpacityCurveWL& opacityCurveLUTWLI
) const
{
    opacityCurveLUTWLI.Set(this->opacityCurveWLI);
}

bool ColorManagementParams::operator !=(const ColorManagementParams& other) const
{
    return !(*this == other);
}

DirPyrEqualizerParams::DirPyrEqualizerParams() :
    enabled(false),
    gamutlab(false),
    mult{
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0
    },
    threshold(0.2),
    skinprotect(0.0),
    hueskin(-5, 25, 170, 120, false),
    cbdlMethod("bef")
{
}

bool DirPyrEqualizerParams::operator ==(const DirPyrEqualizerParams& other) const
{
    return
        enabled == other.enabled
        && gamutlab == other.gamutlab
        && [this, &other]() -> bool
            {
                for (unsigned int i = 0; i < 6; ++i) {
                    if (mult[i] != other.mult[i]) {
                        return false;
                    }
                }
                return true;
            }()
        && threshold == other.threshold
        && skinprotect == other.skinprotect
        && hueskin == other.hueskin
        && cbdlMethod == other.cbdlMethod;
}

bool DirPyrEqualizerParams::operator !=(const DirPyrEqualizerParams& other) const
{
    return !(*this == other);
}

HSVEqualizerParams::HSVEqualizerParams() :
    enabled(false),
    hcurve{
        FCT_Linear
    },
    scurve{
        FCT_Linear
    },
    vcurve{
        FCT_Linear
    }
{
}

bool HSVEqualizerParams::operator ==(const HSVEqualizerParams& other) const
{
    return
        enabled == other.enabled
        && hcurve == other.hcurve
        && scurve == other.scurve
        && vcurve == other.vcurve;
}

bool HSVEqualizerParams::operator !=(const HSVEqualizerParams& other) const
{
    return !(*this == other);
}

FilmSimulationParams::FilmSimulationParams() :
    enabled(false),
    strength(100)
{
}

bool FilmSimulationParams::operator ==(const FilmSimulationParams& other) const
{
    return
        enabled == other.enabled
        && clutFilename == other.clutFilename
        && strength == other.strength;
}

bool FilmSimulationParams::operator !=(const FilmSimulationParams& other) const
{
    return !(*this == other);
}


SoftLightParams::SoftLightParams() :
    enabled(false),
    strength(30)
{
}

bool SoftLightParams::operator ==(const SoftLightParams& other) const
{
    return
        enabled == other.enabled
        && strength == other.strength;
}

bool SoftLightParams::operator !=(const SoftLightParams& other) const
{
    return !(*this == other);
}


DehazeParams::DehazeParams() :
    enabled(false),
    strength(50),
    saturation(50),
    showDepthMap(false),
    depth(25)
{
}

bool DehazeParams::operator ==(const DehazeParams& other) const
{
    return
        enabled == other.enabled
        && strength == other.strength
        && showDepthMap == other.showDepthMap
        && depth == other.depth
        && saturation == other.saturation;
}

bool DehazeParams::operator !=(const DehazeParams& other) const
{
    return !(*this == other);
}

FilmNegativeParams::FilmNegativeParams() :
    enabled(false),
    redRatio(1.36),
    greenExp(1.5),
    blueRatio(0.86),
    refInput({0.0, 0.0, 0.0}),
    refOutput({0.0, 0.0, 0.0}),
    colorSpace(ColorSpace::WORKING),
    backCompat(BackCompat::CURRENT)
{
}

bool FilmNegativeParams::RGB::operator ==(const FilmNegativeParams::RGB& other) const
{
    return
        r == other.r
        && g == other.g
        && b == other.b;
}

bool FilmNegativeParams::RGB::operator !=(const FilmNegativeParams::RGB& other) const
{
    return !(*this == other);
}

FilmNegativeParams::RGB FilmNegativeParams::RGB::operator *(const FilmNegativeParams::RGB& other) const
{
    return {
        (*this).r * other.r,
        (*this).g * other.g,
        (*this).b * other.b
    };
}

bool FilmNegativeParams::operator ==(const FilmNegativeParams& other) const
{
    return
        enabled == other.enabled
        && redRatio == other.redRatio
        && greenExp == other.greenExp
        && blueRatio == other.blueRatio
        && refInput == other.refInput
        && refOutput == other.refOutput
        && colorSpace == other.colorSpace
        && backCompat == other.backCompat;
}

bool FilmNegativeParams::operator !=(const FilmNegativeParams& other) const
{
    return !(*this == other);
}


namespace {

const std::map<std::string, std::string> exif_keys = {
    {"Copyright", "Exif.Image.Copyright"},
    {"Artist", "Exif.Image.Artist"},
    {"ImageDescription", "Exif.Image.ImageDescription"},
    {"Exif.UserComment", "Exif.Photo.UserComment"},
    {"ISOSpeed", "Exif.Photo.ISOSpeedRatings"},
    {"FNumber", "Exif.Photo.FNumber"},
    {"ShutterSpeed", "Exif.Photo.ExposureTime"},
    {"FocalLength", "Exif.Photo.FocalLength"},
    {"ExpComp", "Exif.Photo.ExposureBiasValue"},
    {"Flash", "Exif.Photo.Flash"},
    {"Make", "Exif.Image.Make"},
    {"Model", "Exif.Image.Model"},
    {"Lens", "Exif.Photo.LensModel"},
    {"DateTime", "Exif.Photo.DateTimeOriginal"},
    {"XResolution", "Exif.Image.XResolution"},
    {"YResolution", "Exif.Image.YResolution"}
};

const std::map<std::string, std::string> iptc_keys = {
    {"Title", "Iptc.Application2.ObjectName"},
    {"Category", "Iptc.Application2.Category"},
    {"SupplementalCategories", "Iptc.Application2.SuppCategory"},
    {"Keywords", "Iptc.Application2.Keywords"},
    {"Instructions", "Iptc.Application2.SpecialInstructions"},
    {"DateCreated", "Iptc.Application2.DateCreated"},
    {"Creator", "Iptc.Application2.Byline"},
    {"CreatorJobTitle", "Iptc.Application2.BylineTitle"},
    {"City", "Iptc.Application2.City"},
    {"Province", "Iptc.Application2.ProvinceState"},
    {"Country", "Iptc.Application2.CountryName"},
    {"TransReference", "Iptc.Application2.TransmissionReference"},
    {"Headline", "Iptc.Application2.Headline"},
    {"Credit", "Iptc.Application2.Credit"},
    {"Source", "Iptc.Application2.Source"},
    {"Copyright", "Iptc.Application2.Copyright"},
    {"Caption", "Iptc.Application2.Caption"},
    {"CaptionWriter", "Iptc.Application2.Writer"}
};

} // namespace


std::vector<std::string> MetaDataParams::basicExifKeys = {
    "Exif.Image.Copyright",
    "Exif.Image.Artist",
    "Exif.Image.ImageDescription",
    "Exif.Photo.UserComment",
    "Exif.Image.Make",
    "Exif.Image.Model",
    "Exif.Photo.LensModel",
    "Exif.Photo.FNumber",
    "Exif.Photo.ExposureTime",
    "Exif.Photo.FocalLength",
    "Exif.Photo.ISOSpeedRatings",
    "Exif.Photo.ExposureBiasValue",
    "Exif.Photo.Flash",
    "Exif.Photo.DateTimeOriginal",
    "Exif.Image.XResolution",
    "Exif.Image.YResolution"
};


const std::vector<std::string> additional_default_exif_keys = {
    "Exif.Photo.OffsetTimeDigitized",
    "Exif.Photo.OffsetTimeOriginal",
};


MetaDataParams::MetaDataParams():
    mode(MetaDataParams::EDIT),
    exifKeys{},
    exif{},
    iptc{}
{
    exifKeys = basicExifKeys;
    exifKeys.insert(exifKeys.end(), additional_default_exif_keys.begin(), additional_default_exif_keys.end());
}


bool MetaDataParams::operator==(const MetaDataParams &other) const
{
    return mode == other.mode
        && exifKeys == other.exifKeys
        && exif == other.exif
        && iptc == other.iptc;
}

bool MetaDataParams::operator!=(const MetaDataParams &other) const
{
    return !(*this == other);
}


ProcParams::ProcParams()
{
    setDefaults();
}

void ProcParams::setDefaults()
{
    toneCurve = {};

    labCurve = {};

    rgbCurves = {};

    localContrast = {};

    colorToning = {};

    sharpenEdge = {};

    sharpenMicro = {};

    sharpening = {};

    prsharpening = {};
    prsharpening.contrast = 15.0;
    prsharpening.method = "rld";
    prsharpening.deconvamount = 100;
    prsharpening.deconvradius = 0.45;
    prsharpening.deconviter = 100;
    prsharpening.deconvdamping = 0;

    pdsharpening = {};

    vibrance = {};

    wb = {};

    colorappearance = {};

    defringe = {};

    impulseDenoise = {};

    dirpyrDenoise = {};

    epd = {};

    fattal = {};

    sh = {};

    toneEqualizer = {};

    crop = {};
    cropGuide = {};

    coarse = {};

    commonTrans = {};

    rotate = {};

    distortion = {};

    lensProf = {};

    perspective = {};

    gradient = {};

    pcvignette = {};

    vignetting = {};

    locallab = {};

    chmixer = {};

    blackwhite = {};

    cacorrection = {};

    resize = {};

    framing = {};

    icm = {};

    wavelet = {};

    dirpyrequalizer = {};

    hsvequalizer = {};

    filmSimulation = {};

    softlight = {};

    dehaze = {};

    raw = {};

    metadata = {};
    //exif.clear();
    //iptc.clear();

    // -1 means that there's no pp3 data with rank yet. In this case, the
    // embedded Rating metadata should take precedence. -1 should never be
    // written to pp3 on disk.
    rank = -1;
    colorlabel = 0;
    inTrash = false;

    ppVersion = PPVERSION;
}

int ProcParams::save(const Glib::ustring& fname, const Glib::ustring& fname2, bool fnameAbsolute, ParamsEdited* pedited)
{
    if (fname.empty() && fname2.empty()) {
        return 0;
    }

    const auto& options = App::get().options();

    Glib::ustring sPParams;

    try {
        Glib::KeyFile keyFile;

// Version
        keyFile.set_string("Version", "AppVersion", RTVERSION);
        keyFile.set_integer("Version", "Version", PPVERSION);

        if (rank >= 0) {
            saveToKeyfile(!pedited || pedited->general.rank, "General", "Rank", rank, keyFile);
        }
        saveToKeyfile(!pedited || pedited->general.colorlabel, "General", "ColorLabel", colorlabel, keyFile);
        saveToKeyfile(!pedited || pedited->general.intrash, "General", "InTrash", inTrash, keyFile);

// Tone curve
        saveToKeyfile(!pedited || pedited->toneCurve.autoexp, "Exposure", "Auto", toneCurve.autoexp, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.clip, "Exposure", "Clip", toneCurve.clip, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.expcomp, "Exposure", "Compensation", toneCurve.expcomp, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.brightness, "Exposure", "Brightness", toneCurve.brightness, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.contrast, "Exposure", "Contrast", toneCurve.contrast, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.saturation, "Exposure", "Saturation", toneCurve.saturation, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.black, "Exposure", "Black", toneCurve.black, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.hlcompr, "Exposure", "HighlightCompr", toneCurve.hlcompr, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.hlcomprthresh, "Exposure", "HighlightComprThreshold", toneCurve.hlcomprthresh, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.shcompr, "Exposure", "ShadowCompr", toneCurve.shcompr, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.histmatching, "Exposure", "HistogramMatching", toneCurve.histmatching, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.fromHistMatching, "Exposure", "CurveFromHistogramMatching", toneCurve.fromHistMatching, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.clampOOG, "Exposure", "ClampOOG", toneCurve.clampOOG, keyFile);

// Highlight recovery
        saveToKeyfile(!pedited || pedited->toneCurve.hrenabled, "HLRecovery", "Enabled", toneCurve.hrenabled, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.method, "HLRecovery", "Method", toneCurve.method, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.hlbl, "HLRecovery", "Hlbl", toneCurve.hlbl, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.hlth, "HLRecovery", "Hlth", toneCurve.hlth, keyFile);

        const std::map<ToneCurveMode, const char*> tc_mapping = {
            {ToneCurveMode::STD, "Standard"},
            {ToneCurveMode::FILMLIKE, "FilmLike"},
            {ToneCurveMode::SATANDVALBLENDING, "SatAndValueBlending"},
            {ToneCurveMode::WEIGHTEDSTD, "WeightedStd"},
            {ToneCurveMode::LUMINANCE, "Luminance"},
            {ToneCurveMode::PERCEPTUAL, "Perceptual"}
        };

        saveToKeyfile(!pedited || pedited->toneCurve.curveMode, "Exposure", "CurveMode", tc_mapping, toneCurve.curveMode, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.curveMode2, "Exposure", "CurveMode2", tc_mapping, toneCurve.curveMode2, keyFile);

        saveToKeyfile(!pedited || pedited->toneCurve.curve, "Exposure", "Curve", toneCurve.curve, keyFile);
        saveToKeyfile(!pedited || pedited->toneCurve.curve2, "Exposure", "Curve2", toneCurve.curve2, keyFile);

        saveRetinexParams(keyFile, retinex, pedited);

// Local contrast
        saveToKeyfile(!pedited || pedited->localContrast.enabled, "Local Contrast", "Enabled", localContrast.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->localContrast.radius, "Local Contrast", "Radius", localContrast.radius, keyFile);
        saveToKeyfile(!pedited || pedited->localContrast.amount, "Local Contrast", "Amount", localContrast.amount, keyFile);
        saveToKeyfile(!pedited || pedited->localContrast.darkness, "Local Contrast", "Darkness", localContrast.darkness, keyFile);
        saveToKeyfile(!pedited || pedited->localContrast.lightness, "Local Contrast", "Lightness", localContrast.lightness, keyFile);


// Channel mixer
        saveToKeyfile(!pedited || pedited->chmixer.enabled, "Channel Mixer", "Enabled", chmixer.enabled, keyFile);

        if (!pedited || pedited->chmixer.red[0] || pedited->chmixer.red[1] || pedited->chmixer.red[2]) {
            Glib::ArrayHandle<int> rmix(chmixer.red, 3, Glib::OWNERSHIP_NONE);
            keyFile.set_integer_list("Channel Mixer", "Red", rmix);
        }

        if (!pedited || pedited->chmixer.green[0] || pedited->chmixer.green[1] || pedited->chmixer.green[2]) {
            Glib::ArrayHandle<int> gmix(chmixer.green, 3, Glib::OWNERSHIP_NONE);
            keyFile.set_integer_list("Channel Mixer", "Green", gmix);
        }

        if (!pedited || pedited->chmixer.blue[0] || pedited->chmixer.blue[1] || pedited->chmixer.blue[2]) {
            Glib::ArrayHandle<int> bmix(chmixer.blue, 3, Glib::OWNERSHIP_NONE);
            keyFile.set_integer_list("Channel Mixer", "Blue", bmix);
        }

// Black & White
        saveToKeyfile(!pedited || pedited->blackwhite.enabled, "Black & White", "Enabled", blackwhite.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.method, "Black & White", "Method", blackwhite.method, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.autoc, "Black & White", "Auto", blackwhite.autoc, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.enabledcc, "Black & White", "ComplementaryColors", blackwhite.enabledcc, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.setting, "Black & White", "Setting", blackwhite.setting, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.filter, "Black & White", "Filter", blackwhite.filter, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerRed, "Black & White", "MixerRed", blackwhite.mixerRed, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerOrange, "Black & White", "MixerOrange", blackwhite.mixerOrange, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerYellow, "Black & White", "MixerYellow", blackwhite.mixerYellow, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerGreen, "Black & White", "MixerGreen", blackwhite.mixerGreen, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerCyan, "Black & White", "MixerCyan", blackwhite.mixerCyan, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerBlue, "Black & White", "MixerBlue", blackwhite.mixerBlue, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerMagenta, "Black & White", "MixerMagenta", blackwhite.mixerMagenta, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.mixerPurple, "Black & White", "MixerPurple", blackwhite.mixerPurple, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.gammaRed, "Black & White", "GammaRed", blackwhite.gammaRed, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.gammaGreen, "Black & White", "GammaGreen", blackwhite.gammaGreen, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.gammaBlue, "Black & White", "GammaBlue", blackwhite.gammaBlue, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.algo, "Black & White", "Algorithm", blackwhite.algo, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.luminanceCurve, "Black & White", "LuminanceCurve", blackwhite.luminanceCurve, keyFile);
        saveToKeyfile(
            !pedited || pedited->blackwhite.beforeCurveMode,
            "Black & White",
            "BeforeCurveMode",
            {
                {BlackWhiteParams::TcMode::STD_BW, "Standard"},
                {BlackWhiteParams::TcMode::FILMLIKE_BW, "FilmLike"},
                {BlackWhiteParams::TcMode::SATANDVALBLENDING_BW, "SatAndValueBlending"},
                {BlackWhiteParams::TcMode::WEIGHTEDSTD_BW, "WeightedStd"}

            },
            blackwhite.beforeCurveMode,
            keyFile
        );
        saveToKeyfile(
            !pedited || pedited->blackwhite.afterCurveMode,
            "Black & White",
            "AfterCurveMode",
            {
                {BlackWhiteParams::TcMode::STD_BW, "Standard"},
                {BlackWhiteParams::TcMode::WEIGHTEDSTD_BW, "WeightedStd"}

            },
            blackwhite.afterCurveMode,
            keyFile
        );
        saveToKeyfile(!pedited || pedited->blackwhite.beforeCurve, "Black & White", "BeforeCurve", blackwhite.beforeCurve, keyFile);
        saveToKeyfile(!pedited || pedited->blackwhite.afterCurve, "Black & White", "AfterCurve", blackwhite.afterCurve, keyFile);

// Luma curve
        saveToKeyfile(!pedited || pedited->labCurve.enabled, "Luminance Curve", "Enabled", labCurve.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.brightness, "Luminance Curve", "Brightness", labCurve.brightness, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.contrast, "Luminance Curve", "Contrast", labCurve.contrast, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.chromaticity, "Luminance Curve", "Chromaticity", labCurve.chromaticity, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.gamutmunselmethod, "Luminance Curve", "Gamutmunse", labCurve.gamutmunselmethod, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.rstprotection, "Luminance Curve", "RedAndSkinTonesProtection", labCurve.rstprotection, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.lcredsk, "Luminance Curve", "LCredsk", labCurve.lcredsk, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.lcurve, "Luminance Curve", "LCurve", labCurve.lcurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.acurve, "Luminance Curve", "aCurve", labCurve.acurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.bcurve, "Luminance Curve", "bCurve", labCurve.bcurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.cccurve, "Luminance Curve", "ccCurve", labCurve.cccurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.chcurve, "Luminance Curve", "chCurve", labCurve.chcurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.lhcurve, "Luminance Curve", "lhCurve", labCurve.lhcurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.hhcurve, "Luminance Curve", "hhCurve", labCurve.hhcurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.lccurve, "Luminance Curve", "LcCurve", labCurve.lccurve, keyFile);
        saveToKeyfile(!pedited || pedited->labCurve.clcurve, "Luminance Curve", "ClCurve", labCurve.clcurve, keyFile);

// Sharpening
        saveToKeyfile(!pedited || pedited->sharpening.enabled, "Sharpening", "Enabled", sharpening.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.contrast, "Sharpening", "Contrast", sharpening.contrast, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.method, "Sharpening", "Method", sharpening.method, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.radius, "Sharpening", "Radius", sharpening.radius, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.blurradius, "Sharpening", "BlurRadius", sharpening.blurradius, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.amount, "Sharpening", "Amount", sharpening.amount, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.threshold, "Sharpening", "Threshold", sharpening.threshold.toVector(), keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.edgesonly, "Sharpening", "OnlyEdges", sharpening.edgesonly, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.edges_radius, "Sharpening", "EdgedetectionRadius", sharpening.edges_radius, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.edges_tolerance, "Sharpening", "EdgeTolerance", sharpening.edges_tolerance, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.halocontrol, "Sharpening", "HalocontrolEnabled", sharpening.halocontrol, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.halocontrol_amount, "Sharpening", "HalocontrolAmount", sharpening.halocontrol_amount, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.deconvradius, "Sharpening", "DeconvRadius", sharpening.deconvradius, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.deconvamount, "Sharpening", "DeconvAmount", sharpening.deconvamount, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.deconvdamping, "Sharpening", "DeconvDamping", sharpening.deconvdamping, keyFile);
        saveToKeyfile(!pedited || pedited->sharpening.deconviter, "Sharpening", "DeconvIterations", sharpening.deconviter, keyFile);

// Vibrance
        saveToKeyfile(!pedited || pedited->vibrance.enabled, "Vibrance", "Enabled", vibrance.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.pastels, "Vibrance", "Pastels", vibrance.pastels, keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.saturated, "Vibrance", "Saturated", vibrance.saturated, keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.psthreshold, "Vibrance", "PSThreshold", vibrance.psthreshold.toVector(), keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.protectskins, "Vibrance", "ProtectSkins", vibrance.protectskins, keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.avoidcolorshift, "Vibrance", "AvoidColorShift", vibrance.avoidcolorshift, keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.pastsattog, "Vibrance", "PastSatTog", vibrance.pastsattog, keyFile);
        saveToKeyfile(!pedited || pedited->vibrance.skintonescurve, "Vibrance", "SkinTonesCurve", vibrance.skintonescurve, keyFile);

// Edge sharpening
        saveToKeyfile(!pedited || pedited->sharpenEdge.enabled, "SharpenEdge", "Enabled", sharpenEdge.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenEdge.passes, "SharpenEdge", "Passes", sharpenEdge.passes, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenEdge.amount, "SharpenEdge", "Strength", sharpenEdge.amount, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenEdge.threechannels, "SharpenEdge", "ThreeChannels", sharpenEdge.threechannels, keyFile);

// Micro-contrast sharpening
        saveToKeyfile(!pedited || pedited->sharpenMicro.enabled, "SharpenMicro", "Enabled", sharpenMicro.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenMicro.matrix, "SharpenMicro", "Matrix", sharpenMicro.matrix, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenMicro.amount, "SharpenMicro", "Strength", sharpenMicro.amount, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenMicro.contrast, "SharpenMicro", "Contrast", sharpenMicro.contrast, keyFile);
        saveToKeyfile(!pedited || pedited->sharpenMicro.uniformity, "SharpenMicro", "Uniformity", sharpenMicro.uniformity, keyFile);

// WB
        saveToKeyfile(!pedited || pedited->wb.enabled, "White Balance", "Enabled", wb.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->wb.method, "White Balance", "Setting", wb.method, keyFile);
        saveToKeyfile(!pedited || pedited->wb.temperature, "White Balance", "Temperature", wb.temperature, keyFile);
        saveToKeyfile(!pedited || pedited->wb.green, "White Balance", "Green", wb.green, keyFile);
        saveToKeyfile(!pedited || pedited->wb.equal, "White Balance", "Equal", wb.equal, keyFile);
        saveToKeyfile(!pedited || pedited->wb.tempBias, "White Balance", "TemperatureBias", wb.tempBias, keyFile);
        saveToKeyfile(!pedited || pedited->wb.observer, "White Balance", "StandardObserver", Glib::ustring(wb.observer == StandardObserver::TWO_DEGREES ? "TWO_DEGREES" : "TEN_DEGREES"), keyFile);
        saveToKeyfile(!pedited || pedited->wb.itcwb_green, "White Balance", "Itcwb_green", wb.itcwb_green, keyFile);
        saveToKeyfile(!pedited || pedited->wb.itcwb_rgreen, "White Balance", "Itcwb_rangegreen", wb.itcwb_rgreen, keyFile);
        saveToKeyfile(!pedited || pedited->wb.itcwb_nopurple, "White Balance", "Itcwb_nopurple", wb.itcwb_nopurple, keyFile);
        saveToKeyfile(!pedited || pedited->wb.itcwb_alg, "White Balance", "Itcwb_alg", wb.itcwb_alg, keyFile);
        saveToKeyfile(!pedited || pedited->wb.itcwb_prim, "White Balance", "Itcwb_prim", wb.itcwb_prim, keyFile);
        saveToKeyfile(!pedited || pedited->wb.itcwb_sampling, "White Balance", "Itcwb_sampling", wb.itcwb_sampling, keyFile);
        saveToKeyfile(!pedited || pedited->wb.compat_version, "White Balance", "CompatibilityVersion", wb.compat_version, keyFile);

        saveColorAppearanceParams(keyFile, colorappearance, pedited);

// Impulse denoise
        saveToKeyfile(!pedited || pedited->impulseDenoise.enabled, "Impulse Denoising", "Enabled", impulseDenoise.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->impulseDenoise.thresh, "Impulse Denoising", "Threshold", impulseDenoise.thresh, keyFile);

// Defringe
        saveToKeyfile(!pedited || pedited->defringe.enabled, "Defringing", "Enabled", defringe.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->defringe.radius, "Defringing", "Radius", defringe.radius, keyFile);
        saveToKeyfile(!pedited || pedited->defringe.threshold, "Defringing", "Threshold", defringe.threshold, keyFile);
        saveToKeyfile(!pedited || pedited->defringe.huecurve, "Defringing", "HueCurve", defringe.huecurve, keyFile);

// Dehaze
        saveToKeyfile(!pedited || pedited->dehaze.enabled, "Dehaze", "Enabled", dehaze.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->dehaze.strength, "Dehaze", "Strength", dehaze.strength, keyFile);
        saveToKeyfile(!pedited || pedited->dehaze.showDepthMap, "Dehaze", "ShowDepthMap", dehaze.showDepthMap, keyFile);
        saveToKeyfile(!pedited || pedited->dehaze.depth, "Dehaze", "Depth", dehaze.depth, keyFile);
        saveToKeyfile(!pedited || pedited->dehaze.depth, "Dehaze", "Saturation", dehaze.saturation, keyFile);

// Directional pyramid denoising
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.enabled, "Directional Pyramid Denoising", "Enabled", dirpyrDenoise.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.enhance, "Directional Pyramid Denoising", "Enhance", dirpyrDenoise.enhance, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.median, "Directional Pyramid Denoising", "Median", dirpyrDenoise.median, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.luma, "Directional Pyramid Denoising", "Luma", dirpyrDenoise.luma, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.Ldetail, "Directional Pyramid Denoising", "Ldetail", dirpyrDenoise.Ldetail, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.chroma, "Directional Pyramid Denoising", "Chroma", dirpyrDenoise.chroma, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.dmethod, "Directional Pyramid Denoising", "Method", dirpyrDenoise.dmethod, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.Lmethod, "Directional Pyramid Denoising", "LMethod", dirpyrDenoise.Lmethod, keyFile);

        if (dirpyrDenoise.Cmethod == "PRE") {
            dirpyrDenoise.Cmethod = "MAN"; // Never save 'auto chroma preview mode' to pp3
        }

        saveToKeyfile(!pedited || pedited->dirpyrDenoise.Cmethod, "Directional Pyramid Denoising", "CMethod", dirpyrDenoise.Cmethod, keyFile);

        if (dirpyrDenoise.C2method == "PREV") {
            dirpyrDenoise.C2method = "MANU";
        }

        saveToKeyfile(!pedited || pedited->dirpyrDenoise.C2method, "Directional Pyramid Denoising", "C2Method", dirpyrDenoise.C2method, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.smethod, "Directional Pyramid Denoising", "SMethod", dirpyrDenoise.smethod, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.medmethod, "Directional Pyramid Denoising", "MedMethod", dirpyrDenoise.medmethod, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.rgbmethod, "Directional Pyramid Denoising", "RGBMethod", dirpyrDenoise.rgbmethod, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.methodmed, "Directional Pyramid Denoising", "MethodMed", dirpyrDenoise.methodmed, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.redchro, "Directional Pyramid Denoising", "Redchro", dirpyrDenoise.redchro, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.bluechro, "Directional Pyramid Denoising", "Bluechro", dirpyrDenoise.bluechro, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.gain, "Directional Pyramid Denoising", "AutoGain", dirpyrDenoise.autoGain, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.gamma, "Directional Pyramid Denoising", "Gamma", dirpyrDenoise.gamma, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.passes, "Directional Pyramid Denoising", "Passes", dirpyrDenoise.passes, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.lcurve, "Directional Pyramid Denoising", "LCurve", dirpyrDenoise.lcurve, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrDenoise.cccurve, "Directional Pyramid Denoising", "CCCurve", dirpyrDenoise.cccurve, keyFile);

// EPD
        saveToKeyfile(!pedited || pedited->epd.enabled, "EPD", "Enabled", epd.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->epd.strength, "EPD", "Strength", epd.strength, keyFile);
        saveToKeyfile(!pedited || pedited->epd.gamma, "EPD", "Gamma", epd.gamma, keyFile);
        saveToKeyfile(!pedited || pedited->epd.edgeStopping, "EPD", "EdgeStopping", epd.edgeStopping, keyFile);
        saveToKeyfile(!pedited || pedited->epd.scale, "EPD", "Scale", epd.scale, keyFile);
        saveToKeyfile(!pedited || pedited->epd.reweightingIterates, "EPD", "ReweightingIterates", epd.reweightingIterates, keyFile);

// Fattal
        saveToKeyfile(!pedited || pedited->fattal.enabled, "FattalToneMapping", "Enabled", fattal.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->fattal.threshold, "FattalToneMapping", "Threshold", fattal.threshold, keyFile);
        saveToKeyfile(!pedited || pedited->fattal.amount, "FattalToneMapping", "Amount", fattal.amount, keyFile);
        saveToKeyfile(!pedited || pedited->fattal.anchor, "FattalToneMapping", "Anchor", fattal.anchor, keyFile);

// Shadows & highlights
        saveToKeyfile(!pedited || pedited->sh.enabled, "Shadows & Highlights", "Enabled", sh.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->sh.highlights, "Shadows & Highlights", "Highlights", sh.highlights, keyFile);
        saveToKeyfile(!pedited || pedited->sh.htonalwidth, "Shadows & Highlights", "HighlightTonalWidth", sh.htonalwidth, keyFile);
        saveToKeyfile(!pedited || pedited->sh.shadows, "Shadows & Highlights", "Shadows", sh.shadows, keyFile);
        saveToKeyfile(!pedited || pedited->sh.stonalwidth, "Shadows & Highlights", "ShadowTonalWidth", sh.stonalwidth, keyFile);
        saveToKeyfile(!pedited || pedited->sh.radius, "Shadows & Highlights", "Radius", sh.radius, keyFile);
        saveToKeyfile(!pedited || pedited->sh.lab, "Shadows & Highlights", "Lab", sh.lab, keyFile);

//compression gamut
        saveToKeyfile(!pedited || pedited->cg.enabled, "Compression gamut", "Enabled", cg.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->cg.th_c, "Compression gamut", "th_c", cg.th_c, keyFile);
        saveToKeyfile(!pedited || pedited->cg.th_m, "Compression gamut", "th_m", cg.th_m, keyFile);
        saveToKeyfile(!pedited || pedited->cg.th_y, "Compression gamut", "th_y", cg.th_y, keyFile);
        saveToKeyfile(!pedited || pedited->cg.d_c, "Compression gamut", "d_c", cg.d_c, keyFile);
        saveToKeyfile(!pedited || pedited->cg.autodc, "Compression gamut", "Autodc", cg.autodc, keyFile);
        saveToKeyfile(!pedited || pedited->cg.d_m, "Compression gamut", "d_m", cg.d_m, keyFile);
        saveToKeyfile(!pedited || pedited->cg.autodm, "Compression gamut", "Autodm", cg.autodm, keyFile);
        saveToKeyfile(!pedited || pedited->cg.d_y, "Compression gamut", "d_y", cg.d_y, keyFile);
        saveToKeyfile(!pedited || pedited->cg.autody, "Compression gamut", "Autody", cg.autody, keyFile);
        saveToKeyfile(!pedited || pedited->cg.pwr, "Compression gamut", "pwr", cg.pwr, keyFile);
        saveToKeyfile(!pedited || pedited->cg.colorspace, "Compression gamut", "colorspace", cg.colorspace, keyFile);
        saveToKeyfile(!pedited || pedited->cg.rolloff, "Compression gamut", "rolloff", cg.rolloff, keyFile);

// Tone equalizer
        saveToKeyfile(!pedited || pedited->toneEqualizer.enabled, "ToneEqualizer", "Enabled", toneEqualizer.enabled, keyFile);
        for (size_t i = 0; i < toneEqualizer.bands.size(); ++i) {
            saveToKeyfile(!pedited || pedited->toneEqualizer.bands[i], "ToneEqualizer", "Band" + std::to_string(i), toneEqualizer.bands[i], keyFile);
        }
        saveToKeyfile(!pedited || pedited->toneEqualizer.regularization, "ToneEqualizer", "Regularization", toneEqualizer.regularization, keyFile);
        saveToKeyfile(!pedited || pedited->toneEqualizer.pivot, "ToneEqualizer", "Pivot", toneEqualizer.pivot, keyFile);

// Crop
        saveToKeyfile(!pedited || pedited->crop.enabled, "Crop", "Enabled", crop.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->crop.x, "Crop", "X", crop.x, keyFile);
        saveToKeyfile(!pedited || pedited->crop.y, "Crop", "Y", crop.y, keyFile);
        saveToKeyfile(!pedited || pedited->crop.w, "Crop", "W", crop.w, keyFile);
        saveToKeyfile(!pedited || pedited->crop.h, "Crop", "H", crop.h, keyFile);
        saveToKeyfile(!pedited || pedited->crop.fixratio, "Crop", "FixedRatio", crop.fixratio, keyFile);
        saveToKeyfile(!pedited || pedited->crop.ratio, "Crop", "Ratio", crop.ratio, keyFile);
        saveToKeyfile(!pedited || pedited->crop.orientation, "Crop", "Orientation", crop.orientation, keyFile);

        saveCropGuideParams(keyFile, cropGuide, pedited);

// Coarse transformation
        saveToKeyfile(!pedited || pedited->coarse.rotate, "Coarse Transformation", "Rotate", coarse.rotate, keyFile);
        saveToKeyfile(!pedited || pedited->coarse.hflip, "Coarse Transformation", "HorizontalFlip", coarse.hflip, keyFile);
        saveToKeyfile(!pedited || pedited->coarse.vflip, "Coarse Transformation", "VerticalFlip", coarse.vflip, keyFile);

// Common properties for transformations
        saveToKeyfile(!pedited || pedited->commonTrans.method, "Common Properties for Transformations", "Method", commonTrans.method, keyFile);
        saveToKeyfile(!pedited || pedited->commonTrans.scale, "Common Properties for Transformations", "Scale", commonTrans.scale, keyFile);
        saveToKeyfile(!pedited || pedited->commonTrans.scale_horizontally, "Common Properties for Transformations", "Scale horizontally", commonTrans.scale_horizontally, keyFile);
        saveToKeyfile(!pedited || pedited->commonTrans.scale_vertically, "Common Properties for Transformations", "Scale vertically", commonTrans.scale_vertically, keyFile);
        saveToKeyfile(!pedited || pedited->commonTrans.autofill, "Common Properties for Transformations", "AutoFill", commonTrans.autofill, keyFile);

// Rotation
        saveToKeyfile(!pedited || pedited->rotate.degree, "Rotation", "Degree", rotate.degree, keyFile);

// Distortion
        saveToKeyfile(!pedited || pedited->distortion.amount, "Distortion", "Amount", distortion.amount, keyFile);
        saveToKeyfile(!pedited || pedited->distortion.focal_length, "Distortion", "FocalLength", distortion.focal_length, keyFile);
        saveToKeyfile(!pedited || pedited->distortion.defish, "Distortion", "Defish", distortion.defish, keyFile);

// Lens profile
        saveToKeyfile(!pedited || pedited->lensProf.lcMode, "LensProfile", "LcMode", lensProf.getMethodString(lensProf.lcMode), keyFile);
		saveToKeyfile(!pedited || pedited->lensProf.lcpFile, "LensProfile", "LCPFile", relativePathIfInside2(fname, options.rtSettings.lensProfilesPath, fnameAbsolute, lensProf.lcpFile), keyFile);
        saveToKeyfile(!pedited || pedited->lensProf.useDist, "LensProfile", "UseDistortion", lensProf.useDist, keyFile);
        saveToKeyfile(!pedited || pedited->lensProf.useVign, "LensProfile", "UseVignette", lensProf.useVign, keyFile);
        saveToKeyfile(!pedited || pedited->lensProf.useCA, "LensProfile", "UseCA", lensProf.useCA, keyFile);
        saveToKeyfile(!pedited || pedited->lensProf.lfCameraMake, "LensProfile", "LFCameraMake", lensProf.lfCameraMake, keyFile);
        saveToKeyfile(!pedited || pedited->lensProf.lfCameraModel, "LensProfile", "LFCameraModel", lensProf.lfCameraModel, keyFile);
        saveToKeyfile(!pedited || pedited->lensProf.lfLens, "LensProfile", "LFLens", lensProf.lfLens, keyFile);

// Perspective correction
        saveToKeyfile(!pedited || pedited->perspective.method, "Perspective", "Method", perspective.method, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.horizontal, "Perspective", "Horizontal", perspective.horizontal, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.vertical, "Perspective", "Vertical", perspective.vertical, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_crop_factor, "Perspective", "CameraCropFactor", perspective.camera_crop_factor, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_focal_length, "Perspective", "CameraFocalLength", perspective.camera_focal_length, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_pitch, "Perspective", "CameraPitch", perspective.camera_pitch, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_roll, "Perspective", "CameraRoll", perspective.camera_roll, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_shift_horiz, "Perspective", "CameraShiftHorizontal", perspective.camera_shift_horiz, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_shift_vert, "Perspective", "CameraShiftVertical", perspective.camera_shift_vert, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.camera_yaw, "Perspective", "CameraYaw", perspective.camera_yaw, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.projection_shift_horiz, "Perspective", "ProjectionShiftHorizontal", perspective.projection_shift_horiz, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.projection_pitch, "Perspective", "ProjectionPitch", perspective.projection_pitch, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.projection_rotate, "Perspective", "ProjectionRotate", perspective.projection_rotate, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.projection_shift_horiz, "Perspective", "ProjectionShiftHorizontal", perspective.projection_shift_horiz, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.projection_shift_vert, "Perspective", "ProjectionShiftVertical", perspective.projection_shift_vert, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.projection_yaw, "Perspective", "ProjectionYaw", perspective.projection_yaw, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.control_lines, "Perspective", "ControlLineValues", perspective.control_line_values, keyFile);
        saveToKeyfile(!pedited || pedited->perspective.control_lines, "Perspective", "ControlLineTypes", perspective.control_line_types, keyFile);

// Gradient
        saveToKeyfile(!pedited || pedited->gradient.enabled, "Gradient", "Enabled", gradient.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->gradient.degree, "Gradient", "Degree", gradient.degree, keyFile);
        saveToKeyfile(!pedited || pedited->gradient.feather, "Gradient", "Feather", gradient.feather, keyFile);
        saveToKeyfile(!pedited || pedited->gradient.strength, "Gradient", "Strength", gradient.strength, keyFile);
        saveToKeyfile(!pedited || pedited->gradient.centerX, "Gradient", "CenterX", gradient.centerX, keyFile);
        saveToKeyfile(!pedited || pedited->gradient.centerY, "Gradient", "CenterY", gradient.centerY, keyFile);

        saveLocalLabParams(keyFile, locallab, pedited);

// Post-crop vignette
        saveToKeyfile(!pedited || pedited->pcvignette.enabled, "PCVignette", "Enabled", pcvignette.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->pcvignette.strength, "PCVignette", "Strength", pcvignette.strength, keyFile);
        saveToKeyfile(!pedited || pedited->pcvignette.feather, "PCVignette", "Feather", pcvignette.feather, keyFile);
        saveToKeyfile(!pedited || pedited->pcvignette.roundness, "PCVignette", "Roundness", pcvignette.roundness, keyFile);

// C/A correction
        saveToKeyfile(!pedited || pedited->cacorrection.red, "CACorrection", "Red", cacorrection.red, keyFile);
        saveToKeyfile(!pedited || pedited->cacorrection.blue, "CACorrection", "Blue", cacorrection.blue, keyFile);

// Vignetting correction
        saveToKeyfile(!pedited || pedited->vignetting.amount, "Vignetting Correction", "Amount", vignetting.amount, keyFile);
        saveToKeyfile(!pedited || pedited->vignetting.radius, "Vignetting Correction", "Radius", vignetting.radius, keyFile);
        saveToKeyfile(!pedited || pedited->vignetting.strength, "Vignetting Correction", "Strength", vignetting.strength, keyFile);
        saveToKeyfile(!pedited || pedited->vignetting.centerX, "Vignetting Correction", "CenterX", vignetting.centerX, keyFile);
        saveToKeyfile(!pedited || pedited->vignetting.centerY, "Vignetting Correction", "CenterY", vignetting.centerY, keyFile);

// Resize
        saveToKeyfile(!pedited || pedited->resize.enabled, "Resize", "Enabled", resize.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->resize.scale, "Resize", "Scale", resize.scale, keyFile);
        saveToKeyfile(!pedited || pedited->resize.appliesTo, "Resize", "AppliesTo", resize.appliesTo, keyFile);
        saveToKeyfile(!pedited || pedited->resize.method, "Resize", "Method", resize.method, keyFile);
        saveToKeyfile(!pedited || pedited->resize.dataspec, "Resize", "DataSpecified", resize.dataspec, keyFile);
        saveToKeyfile(!pedited || pedited->resize.width, "Resize", "Width", resize.width, keyFile);
        saveToKeyfile(!pedited || pedited->resize.height, "Resize", "Height", resize.height, keyFile);
        saveToKeyfile(!pedited || pedited->resize.longedge, "Resize", "LongEdge", resize.longedge, keyFile);
        saveToKeyfile(!pedited || pedited->resize.shortedge, "Resize", "ShortEdge", resize.shortedge, keyFile);
        saveToKeyfile(!pedited || pedited->resize.allowUpscaling, "Resize", "AllowUpscaling", resize.allowUpscaling, keyFile);

        saveFramingParams(keyFile, framing, pedited);

        saveCaptureSharpeningParams(keyFile, pdsharpening, pedited);

// Post resize sharpening
        saveToKeyfile(!pedited || pedited->prsharpening.enabled, "PostResizeSharpening", "Enabled", prsharpening.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.contrast, "PostResizeSharpening", "Contrast", prsharpening.contrast, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.method, "PostResizeSharpening", "Method", prsharpening.method, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.radius, "PostResizeSharpening", "Radius", prsharpening.radius, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.amount, "PostResizeSharpening", "Amount", prsharpening.amount, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.threshold, "PostResizeSharpening", "Threshold", prsharpening.threshold.toVector(), keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.edgesonly, "PostResizeSharpening", "OnlyEdges", prsharpening.edgesonly, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.edges_radius, "PostResizeSharpening", "EdgedetectionRadius", prsharpening.edges_radius, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.edges_tolerance, "PostResizeSharpening", "EdgeTolerance", prsharpening.edges_tolerance, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.halocontrol, "PostResizeSharpening", "HalocontrolEnabled", prsharpening.halocontrol, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.halocontrol_amount, "PostResizeSharpening", "HalocontrolAmount", prsharpening.halocontrol_amount, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.deconvradius, "PostResizeSharpening", "DeconvRadius", prsharpening.deconvradius, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.deconvamount, "PostResizeSharpening", "DeconvAmount", prsharpening.deconvamount, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.deconvdamping, "PostResizeSharpening", "DeconvDamping", prsharpening.deconvdamping, keyFile);
        saveToKeyfile(!pedited || pedited->prsharpening.deconviter, "PostResizeSharpening", "DeconvIterations", prsharpening.deconviter, keyFile);

// Color management
		saveToKeyfile(!pedited || pedited->icm.inputProfile, "Color Management", "InputProfile", relativePathIfInside2(fname, options.rtSettings.cameraProfilesPath, fnameAbsolute, icm.inputProfile), keyFile);
        saveToKeyfile(!pedited || pedited->icm.toneCurve, "Color Management", "ToneCurve", icm.toneCurve, keyFile);
        saveToKeyfile(!pedited || pedited->icm.applyLookTable, "Color Management", "ApplyLookTable", icm.applyLookTable, keyFile);
        saveToKeyfile(!pedited || pedited->icm.applyBaselineExposureOffset, "Color Management", "ApplyBaselineExposureOffset", icm.applyBaselineExposureOffset, keyFile);
        saveToKeyfile(!pedited || pedited->icm.applyHueSatMap, "Color Management", "ApplyHueSatMap", icm.applyHueSatMap, keyFile);
        saveToKeyfile(!pedited || pedited->icm.dcpIlluminant, "Color Management", "DCPIlluminant", icm.dcpIlluminant, keyFile);
        saveToKeyfile(!pedited || pedited->icm.workingProfile, "Color Management", "WorkingProfile", icm.workingProfile, keyFile);
        saveToKeyfile(
            !pedited || pedited->icm.workingTRC,
            "Color Management",
            "WorkingTRC",
            {
                {ColorManagementParams::WorkingTrc::NONE, "none"},
                {ColorManagementParams::WorkingTrc::CUSTOM, "Custom"},
                {ColorManagementParams::WorkingTrc::BT709, "bt709"},
                {ColorManagementParams::WorkingTrc::SRGB, "srgb"},
                {ColorManagementParams::WorkingTrc::GAMMA_2_2, "22"},
                {ColorManagementParams::WorkingTrc::GAMMA_1_8, "18"},
                {ColorManagementParams::WorkingTrc::LINEAR, "lin"}
            },
            icm.workingTRC,
            keyFile
        );
        saveToKeyfile(
            !pedited || pedited->icm.wgamut,
            "Color Management",
            "Wwgamut",
            {
                {ColorManagementParams::Wwgamut::NONE, "none"},
                {ColorManagementParams::Wwgamut::REC2020, "rec2020"},
                {ColorManagementParams::Wwgamut::ADOBE, "adob"},
                {ColorManagementParams::Wwgamut::SRGB, "rgb"},
                {ColorManagementParams::Wwgamut::DCIP3, "dci"}

            },
            icm.wgamut,
            keyFile
        );
        
        saveToKeyfile(
            !pedited || pedited->icm.will,
            "Color Management",
            "Will",
            {
                {ColorManagementParams::Illuminant::DEFAULT, "def"},
                {ColorManagementParams::Illuminant::D41, "D41"},
                {ColorManagementParams::Illuminant::D50, "D50"},
                {ColorManagementParams::Illuminant::D55, "D55"},
                {ColorManagementParams::Illuminant::D60, "D60"},
                {ColorManagementParams::Illuminant::D65, "D65"},
                {ColorManagementParams::Illuminant::D80, "D80"},
                {ColorManagementParams::Illuminant::D120, "D120"},
                {ColorManagementParams::Illuminant::STDA, "stda"},
                {ColorManagementParams::Illuminant::TUNGSTEN_2000K, "2000"},
                {ColorManagementParams::Illuminant::TUNGSTEN_1500K, "1500"},
                {ColorManagementParams::Illuminant::E, "E"}
            },
            icm.will,
            keyFile
        );
        saveToKeyfile(
            !pedited || pedited->icm.wprim,
            "Color Management",
            "Wprim",
            {
                {ColorManagementParams::Primaries::DEFAULT, "def"},
                {ColorManagementParams::Primaries::SRGB, "srgb"},
                {ColorManagementParams::Primaries::ADOBE_RGB, "adob"},
                {ColorManagementParams::Primaries::PRO_PHOTO, "prop"},
                {ColorManagementParams::Primaries::REC2020, "rec"},
                {ColorManagementParams::Primaries::ACES_P1, "aces"},
                {ColorManagementParams::Primaries::WIDE_GAMUT, "wid"},
                {ColorManagementParams::Primaries::ACES_P0, "ac0"},
                {ColorManagementParams::Primaries::JDC_MAX, "jdcmax"},
                {ColorManagementParams::Primaries::JDC_MAXSTDA, "jdcmaxstdA"},
                {ColorManagementParams::Primaries::BRUCE_RGB, "bru"},
                {ColorManagementParams::Primaries::BETA_RGB, "bet"},
                {ColorManagementParams::Primaries::BEST_RGB, "bst"},
                {ColorManagementParams::Primaries::CUSTOM, "cus"},
                {ColorManagementParams::Primaries::CUSTOM_GRID, "cusgr"},
                {ColorManagementParams::Primaries::CUSTOM_POL, "cuspol"}
                
            },
            icm.wprim,
            keyFile
        );
        saveToKeyfile(
            !pedited || pedited->icm.wcat,
            "Color Management",
            "Wcat",
            {
                {ColorManagementParams::Cat::BRAD, "brad"},
                {ColorManagementParams::Cat::CAT16, "cat16"},
                {ColorManagementParams::Cat::CAT02, "cat02"},
                {ColorManagementParams::Cat::CAT_VK, "cat_vk"},
                {ColorManagementParams::Cat::CAT_XYZ, "cat_xyz"}
            },
            icm.wcat,
            keyFile
        );
        
        saveToKeyfile(!pedited || pedited->icm.opacityCurveWLI, "Color Management", "OpacityCurveWLI", icm.opacityCurveWLI, keyFile);
        
        saveToKeyfile(!pedited || pedited->icm.wGamma, "Color Management", "WorkingTRCGamma", icm.wGamma, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wSlope, "Color Management", "WorkingTRCSlope", icm.wSlope, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wapsat, "Color Management", "WorkingTRCsat", icm.wapsat, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wmidtcie, "Color Management", "Wmidtcie", icm.wmidtcie, keyFile);
        saveToKeyfile(!pedited || pedited->icm.sigmatrc, "Color Management", "Sigmatrc", icm.sigmatrc, keyFile);
        saveToKeyfile(!pedited || pedited->icm.offstrc, "Color Management", "Offstrc", icm.offstrc, keyFile);
        saveToKeyfile(!pedited || pedited->icm.residtrc, "Color Management", "Residtrc", icm.residtrc, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wgampower, "Color Management", "Wgampower", icm.wgampower, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wgamgain, "Color Management", "Wgamgain", icm.wgamgain, keyFile);
        saveToKeyfile(!pedited || pedited->icm.pyrwavtrc, "Color Management", "Pyrwavtrc", icm.pyrwavtrc, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wsmoothcie, "Color Management", "Wsmoothcie", icm.wsmoothcie, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wsmoothciesli, "Color Management", "Wsmoothciesli", icm.wsmoothciesli, keyFile);
        saveToKeyfile(!pedited || pedited->icm.redx, "Color Management", "Redx", icm.redx, keyFile);
        saveToKeyfile(!pedited || pedited->icm.redy, "Color Management", "Redy", icm.redy, keyFile);
        saveToKeyfile(!pedited || pedited->icm.grex, "Color Management", "Grex", icm.grex, keyFile);
        saveToKeyfile(!pedited || pedited->icm.grey, "Color Management", "Grey", icm.grey, keyFile);
        saveToKeyfile(!pedited || pedited->icm.blux, "Color Management", "Blux", icm.blux, keyFile);
        saveToKeyfile(!pedited || pedited->icm.bluy, "Color Management", "Bluy", icm.bluy, keyFile);
        
        saveToKeyfile(!pedited || pedited->icm.redrot, "Color Management", "Redrot", icm.redrot, keyFile);
        saveToKeyfile(!pedited || pedited->icm.redsat, "Color Management", "Redsat", icm.redsat, keyFile);
        saveToKeyfile(!pedited || pedited->icm.grerot, "Color Management", "Grerot", icm.grerot, keyFile);
        saveToKeyfile(!pedited || pedited->icm.gresat, "Color Management", "Gresat", icm.gresat, keyFile);
        saveToKeyfile(!pedited || pedited->icm.blurot, "Color Management", "Blurot", icm.blurot, keyFile);
        saveToKeyfile(!pedited || pedited->icm.blusat, "Color Management", "Blusat", icm.blusat, keyFile);
        
        saveToKeyfile(!pedited || pedited->icm.refi, "Color Management", "Refi", icm.refi, keyFile);
        saveToKeyfile(!pedited || pedited->icm.shiftx, "Color Management", "Shiftx", icm.shiftx, keyFile);
        saveToKeyfile(!pedited || pedited->icm.shifty, "Color Management", "Shifty", icm.shifty, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieALow, "Color Management", "LabGridcieALow", icm.labgridcieALow, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieBLow, "Color Management", "LabGridcieBLow", icm.labgridcieBLow, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieAHigh, "Color Management", "LabGridcieAHigh", icm.labgridcieAHigh, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieBHigh, "Color Management", "LabGridcieBHigh", icm.labgridcieBHigh, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieGx, "Color Management", "LabGridcieGx", icm.labgridcieGx, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieGy, "Color Management", "LabGridcieGy", icm.labgridcieGy, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieWx, "Color Management", "LabGridcieWx", icm.labgridcieWx, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieWy, "Color Management", "LabGridcieWy", icm.labgridcieWy, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieMx, "Color Management", "LabGridcieMx", icm.labgridcieMx, keyFile);
        saveToKeyfile(!pedited || pedited->icm.labgridcieMy, "Color Management", "LabGridcieMy", icm.labgridcieMy, keyFile);
        saveToKeyfile(!pedited || pedited->icm.preser, "Color Management", "Preser", icm.preser, keyFile);
        saveToKeyfile(!pedited || pedited->icm.fbw, "Color Management", "Fbw", icm.fbw, keyFile);
        saveToKeyfile(!pedited || pedited->icm.trcExp, "Color Management", "TrcExp", icm.trcExp, keyFile);
        saveToKeyfile(!pedited || pedited->icm.wavExp, "Color Management", "WavExp", icm.wavExp, keyFile);
        saveToKeyfile(!pedited || pedited->icm.gamut, "Color Management", "Gamut", icm.gamut, keyFile);
        saveToKeyfile(!pedited || pedited->icm.outputProfile, "Color Management", "OutputProfile", icm.outputProfile, keyFile);
        saveToKeyfile(
            !pedited || pedited->icm.aRendIntent,
            "Color Management",
            "aIntent",
            {
                {RI_PERCEPTUAL, "Perceptual"},
                {RI_RELATIVE, "Relative"},
                {RI_SATURATION, "Saturation"},
                {RI_ABSOLUTE, "Absolute"}
            },
            icm.aRendIntent,
            keyFile
        );

        saveToKeyfile(
            !pedited || pedited->icm.outputIntent,
            "Color Management",
            "OutputProfileIntent",
            {
                {RI_PERCEPTUAL, "Perceptual"},
                {RI_RELATIVE, "Relative"},
                {RI_SATURATION, "Saturation"},
                {RI_ABSOLUTE, "Absolute"}
            },
            icm.outputIntent,
            keyFile
        );
        saveToKeyfile(!pedited || pedited->icm.outputBPC, "Color Management", "OutputBPC", icm.outputBPC, keyFile);
        saveToKeyfile(!pedited || pedited->icm.outputPaperwhiteNits, "Color Management", "OutputPaperwhiteNits", icm.outputPaperwhiteNits, keyFile);
        saveToKeyfile(!pedited || pedited->icm.outputMaxNits, "Color Management", "OutputMaxNits", icm.outputMaxNits, keyFile);

        saveWaveletParams(keyFile, wavelet, pedited);

//Spot removal
        saveToKeyfile(!pedited || pedited->spot.enabled, "Spot removal", "Enabled", spot.enabled, keyFile);
        for (size_t i = 0; i < spot.entries.size (); ++i) {
            std::vector<double> entry(7);

            entry[0] = double (spot.entries.at (i).sourcePos.x);
            entry[1] = double (spot.entries.at (i).sourcePos.y);
            entry[2] = double (spot.entries.at (i).targetPos.x);
            entry[3] = double (spot.entries.at (i).targetPos.y);
            entry[4] = double (spot.entries.at (i).radius);
            entry[5] = double (spot.entries.at (i).feather);
            entry[6] = double (spot.entries.at (i).opacity);

            std::stringstream ss;
            ss << "Spot" << (i + 1);

            saveToKeyfile(!pedited || pedited->spot.entries, "Spot removal", ss.str(), entry, keyFile);
        }

// Directional pyramid equalizer
        saveToKeyfile(!pedited || pedited->dirpyrequalizer.enabled, "Directional Pyramid Equalizer", "Enabled", dirpyrequalizer.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrequalizer.gamutlab, "Directional Pyramid Equalizer", "Gamutlab", dirpyrequalizer.gamutlab, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrequalizer.cbdlMethod, "Directional Pyramid Equalizer", "cbdlMethod", dirpyrequalizer.cbdlMethod, keyFile);

        for (int i = 0; i < 6; i++) {
            std::stringstream ss;
            ss << "Mult" << i;

            saveToKeyfile(!pedited || pedited->dirpyrequalizer.mult[i], "Directional Pyramid Equalizer", ss.str(), dirpyrequalizer.mult[i], keyFile);
        }

        saveToKeyfile(!pedited || pedited->dirpyrequalizer.threshold, "Directional Pyramid Equalizer", "Threshold", dirpyrequalizer.threshold, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrequalizer.skinprotect, "Directional Pyramid Equalizer", "Skinprotect", dirpyrequalizer.skinprotect, keyFile);
        saveToKeyfile(!pedited || pedited->dirpyrequalizer.hueskin, "Directional Pyramid Equalizer", "Hueskin", dirpyrequalizer.hueskin.toVector(), keyFile);

// HSV Equalizer
        saveToKeyfile(!pedited || pedited->hsvequalizer.enabled, "HSV Equalizer", "Enabled", hsvequalizer.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->hsvequalizer.hcurve, "HSV Equalizer", "HCurve", hsvequalizer.hcurve, keyFile);
        saveToKeyfile(!pedited || pedited->hsvequalizer.scurve, "HSV Equalizer", "SCurve", hsvequalizer.scurve, keyFile);
        saveToKeyfile(!pedited || pedited->hsvequalizer.vcurve, "HSV Equalizer", "VCurve", hsvequalizer.vcurve, keyFile);

// Soft Light
        saveToKeyfile(!pedited || pedited->softlight.enabled, "SoftLight", "Enabled", softlight.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->softlight.strength, "SoftLight", "Strength", softlight.strength, keyFile);

// Film simulation
        saveToKeyfile(!pedited || pedited->filmSimulation.enabled, "Film Simulation", "Enabled", filmSimulation.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->filmSimulation.clutFilename, "Film Simulation", "ClutFilename", filmSimulation.clutFilename, keyFile);
        saveToKeyfile(!pedited || pedited->filmSimulation.strength, "Film Simulation", "Strength", filmSimulation.strength, keyFile);

        saveToKeyfile(!pedited || pedited->rgbCurves.enabled, "RGB Curves", "Enabled", rgbCurves.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->rgbCurves.lumamode, "RGB Curves", "LumaMode", rgbCurves.lumamode, keyFile);
        saveToKeyfile(!pedited || pedited->rgbCurves.rcurve, "RGB Curves", "rCurve", rgbCurves.rcurve, keyFile);
        saveToKeyfile(!pedited || pedited->rgbCurves.gcurve, "RGB Curves", "gCurve", rgbCurves.gcurve, keyFile);
        saveToKeyfile(!pedited || pedited->rgbCurves.bcurve, "RGB Curves", "bCurve", rgbCurves.bcurve, keyFile);

// Color toning
        saveToKeyfile(!pedited || pedited->colorToning.enabled, "ColorToning", "Enabled", colorToning.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.method, "ColorToning", "Method", colorToning.method, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.lumamode, "ColorToning", "Lumamode", colorToning.lumamode, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.twocolor, "ColorToning", "Twocolor", colorToning.twocolor, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.redlow, "ColorToning", "Redlow", colorToning.redlow, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.greenlow, "ColorToning", "Greenlow", colorToning.greenlow, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.bluelow, "ColorToning", "Bluelow", colorToning.bluelow, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.satlow, "ColorToning", "Satlow", colorToning.satlow, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.balance, "ColorToning", "Balance", colorToning.balance, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.sathigh, "ColorToning", "Sathigh", colorToning.sathigh, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.redmed, "ColorToning", "Redmed", colorToning.redmed, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.greenmed, "ColorToning", "Greenmed", colorToning.greenmed, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.bluemed, "ColorToning", "Bluemed", colorToning.bluemed, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.redhigh, "ColorToning", "Redhigh", colorToning.redhigh, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.greenhigh, "ColorToning", "Greenhigh", colorToning.greenhigh, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.bluehigh, "ColorToning", "Bluehigh", colorToning.bluehigh, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.autosat, "ColorToning", "Autosat", colorToning.autosat, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.opacityCurve, "ColorToning", "OpacityCurve", colorToning.opacityCurve, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.colorCurve, "ColorToning", "ColorCurve", colorToning.colorCurve, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.satprotectionthreshold, "ColorToning", "SatProtectionThreshold", colorToning.satProtectionThreshold, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.saturatedopacity, "ColorToning", "SaturatedOpacity", colorToning.saturatedOpacity, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.strength, "ColorToning", "Strength", colorToning.strength, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.hlColSat, "ColorToning", "HighlightsColorSaturation", colorToning.hlColSat.toVector(), keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.shadowsColSat, "ColorToning", "ShadowsColorSaturation", colorToning.shadowsColSat.toVector(), keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.clcurve, "ColorToning", "ClCurve", colorToning.clcurve, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.cl2curve, "ColorToning", "Cl2Curve", colorToning.cl2curve, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.labgridALow, "ColorToning", "LabGridALow", colorToning.labgridALow, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.labgridBLow, "ColorToning", "LabGridBLow", colorToning.labgridBLow, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.labgridAHigh, "ColorToning", "LabGridAHigh", colorToning.labgridAHigh, keyFile);
        saveToKeyfile(!pedited || pedited->colorToning.labgridBHigh, "ColorToning", "LabGridBHigh", colorToning.labgridBHigh, keyFile);
        if (!pedited || pedited->colorToning.labregions) {
            for (size_t j = 0; j < colorToning.labregions.size(); ++j) {
                std::string n = std::to_string(j+1);
                auto &l = colorToning.labregions[j];
                putToKeyfile("ColorToning", Glib::ustring("LabRegionA_") + n, l.a, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionB_") + n, l.b, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionSaturation_") + n, l.saturation, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionSlope_") + n, l.slope, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionOffset_") + n, l.offset, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionPower_") + n, l.power, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionHueMask_") + n, l.hueMask, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionChromaticityMask_") + n, l.chromaticityMask, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionLightnessMask_") + n, l.lightnessMask, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionMaskBlur_") + n, l.maskBlur, keyFile);
                putToKeyfile("ColorToning", Glib::ustring("LabRegionChannel_") + n, l.channel, keyFile);
            }
        }
        saveToKeyfile(!pedited || pedited->colorToning.labregionsShowMask, "ColorToning", "LabRegionsShowMask", colorToning.labregionsShowMask, keyFile);

        saveRawParams(keyFile, raw, pedited, fname, fnameAbsolute);

// MetaData
        saveToKeyfile(!pedited || pedited->metadata.mode, "MetaData", "Mode", metadata.mode, keyFile);
        saveToKeyfile(!pedited || pedited->metadata.exifKeys, "MetaData", "ExifKeys", metadata.exifKeys, keyFile);

// Film negative
        saveToKeyfile(!pedited || pedited->filmNegative.enabled, "Film Negative", "Enabled", filmNegative.enabled, keyFile);
        saveToKeyfile(!pedited || pedited->filmNegative.redRatio, "Film Negative", "RedRatio", filmNegative.redRatio, keyFile);
        saveToKeyfile(!pedited || pedited->filmNegative.greenExp, "Film Negative", "GreenExponent", filmNegative.greenExp, keyFile);
        saveToKeyfile(!pedited || pedited->filmNegative.blueRatio, "Film Negative", "BlueRatio", filmNegative.blueRatio, keyFile);

        // FIXME to be removed: only for backwards compatibility with an intermediate dev version
        if (filmNegative.backCompat == FilmNegativeParams::BackCompat::V2) {
            saveToKeyfile(!pedited || pedited->filmNegative.refInput, "Film Negative", "RedBase", filmNegative.refInput.r, keyFile);
            saveToKeyfile(!pedited || pedited->filmNegative.refInput, "Film Negative", "GreenBase", filmNegative.refInput.g, keyFile);
            saveToKeyfile(!pedited || pedited->filmNegative.refInput, "Film Negative", "BlueBase", filmNegative.refInput.b, keyFile);
        }

        saveToKeyfile(!pedited || pedited->filmNegative.colorSpace, "Film Negative", "ColorSpace", toUnderlying(filmNegative.colorSpace), keyFile);
        saveRgbToKeyfile(!pedited || pedited->filmNegative.refInput, "Film Negative", "RefInput", filmNegative.refInput, keyFile);
        saveRgbToKeyfile(!pedited || pedited->filmNegative.refOutput, "Film Negative", "RefOutput", filmNegative.refOutput, keyFile);
        // Only save the "backCompat" key if the filmneg params are not already upgraded to CURRENT.
        // Also, avoid saving backCompat if the "enabled" key was not saved (most probably the entire key group is excluded).
        saveToKeyfile(filmNegative.backCompat != FilmNegativeParams::BackCompat::CURRENT &&
            (!pedited || pedited->filmNegative.enabled), "Film Negative", "BackCompat", toUnderlying(filmNegative.backCompat), keyFile);

// EXIF change list
        if (!pedited || pedited->exif) {
            std::map<Glib::ustring, Glib::ustring> m;
            for (auto &p : exif_keys) {
                m[p.second] = p.first;
            }
            for (auto &p : metadata.exif) {
                auto it = m.find(p.first);
                if (it != m.end()) {
                    keyFile.set_string("Exif", it->second, p.second);
                }
            }
        }

// IPTC change list
        if (!pedited || pedited->iptc) {
            std::map<std::string, std::string> m;
            for (auto &p : iptc_keys) {
                m[p.second] = p.first;
            }
            for (auto &p : metadata.iptc) {
                auto it = m.find(p.first);
                if (it != m.end()) {
                    Glib::ArrayHandle<Glib::ustring> values = p.second;
                    keyFile.set_string_list("IPTC", it->second, values);
                }
            }
        }

        sPParams = keyFile.to_data();

    } catch (Glib::KeyFileError&) {}

    if (sPParams.empty()) {
        return 1;
    }

    int error1, error2;
    error1 = write(fname, sPParams);

    if (!fname2.empty()) {

        error2 = write(fname2, sPParams);
        // If at least one file has been saved, it's a success
        return error1 & error2;
    } else {
        return error1;
    }
}

int ProcParams::load(const Glib::ustring& fname, ParamsEdited* pedited)
{
    setlocale(LC_NUMERIC, "C");  // to set decimal point to "."

    if (fname.empty()) {
        return 1;
    }

    const auto& options = App::get().options();

    Glib::KeyFile keyFile;

    try {
        std::unique_ptr<ParamsEdited> dummy_pedited;

        if (pedited) {
            pedited->set(false);
        } else {
            dummy_pedited.reset(new ParamsEdited());

            pedited = dummy_pedited.get();
        }

        if (!Glib::file_test(fname, Glib::FILE_TEST_EXISTS) ||
                !keyFile.load_from_file(fname)) {
            return 1;
        }

        ppVersion = PPVERSION;
        appVersion = RTVERSION;

        if (keyFile.has_group("Version")) {
            if (keyFile.has_key("Version", "AppVersion")) {
                appVersion = keyFile.get_string("Version", "AppVersion");
            }

            if (keyFile.has_key("Version", "Version")) {
                ppVersion = keyFile.get_integer("Version", "Version");
            }
        }

        if (keyFile.has_group("General")) {
            assignFromKeyfile(keyFile, "General", "Rank", rank, pedited->general.rank);
            assignFromKeyfile(keyFile, "General", "ColorLabel", colorlabel, pedited->general.colorlabel);
            assignFromKeyfile(keyFile, "General", "InTrash", inTrash, pedited->general.intrash);
        }

        if (keyFile.has_group("Exposure")) {
            if (ppVersion < PPVERSION_AEXP) {
                toneCurve.autoexp = false; // prevent execution of autoexp when opening file created with earlier versions of autoexp algorithm
            } else {
                assignFromKeyfile(keyFile, "Exposure", "Auto", toneCurve.autoexp, pedited->toneCurve.autoexp);
            }

            assignFromKeyfile(keyFile, "Exposure", "Clip", toneCurve.clip, pedited->toneCurve.clip);
            assignFromKeyfile(keyFile, "Exposure", "Compensation", toneCurve.expcomp, pedited->toneCurve.expcomp);
            assignFromKeyfile(keyFile, "Exposure", "Brightness", toneCurve.brightness, pedited->toneCurve.brightness);
            assignFromKeyfile(keyFile, "Exposure", "Contrast", toneCurve.contrast, pedited->toneCurve.contrast);
            assignFromKeyfile(keyFile, "Exposure", "Saturation", toneCurve.saturation, pedited->toneCurve.saturation);
            assignFromKeyfile(keyFile, "Exposure", "Black", toneCurve.black, pedited->toneCurve.black);
            assignFromKeyfile(keyFile, "Exposure", "HighlightCompr", toneCurve.hlcompr, pedited->toneCurve.hlcompr);
            assignFromKeyfile(keyFile, "Exposure", "HighlightComprThreshold", toneCurve.hlcomprthresh, pedited->toneCurve.hlcomprthresh);
            assignFromKeyfile(keyFile, "Exposure", "ShadowCompr", toneCurve.shcompr, pedited->toneCurve.shcompr);

            if (toneCurve.shcompr > 100) {
                toneCurve.shcompr = 100; // older pp3 files can have values above 100.
            }

            const std::map<std::string, ToneCurveMode> tc_mapping = {
                {"Standard", ToneCurveMode::STD},
                {"FilmLike", ToneCurveMode::FILMLIKE},
                {"SatAndValueBlending", ToneCurveMode::SATANDVALBLENDING},
                {"WeightedStd", ToneCurveMode::WEIGHTEDSTD},
                {"Luminance", ToneCurveMode::LUMINANCE},
                {"Perceptual", ToneCurveMode::PERCEPTUAL}
            };

            assignFromKeyfile(keyFile, "Exposure", "CurveMode", tc_mapping, toneCurve.curveMode, pedited->toneCurve.curveMode);
            assignFromKeyfile(keyFile, "Exposure", "CurveMode2", tc_mapping, toneCurve.curveMode2, pedited->toneCurve.curveMode2);

            if (ppVersion > 200) {
                assignFromKeyfile(keyFile, "Exposure", "Curve", toneCurve.curve, pedited->toneCurve.curve);
                assignFromKeyfile(keyFile, "Exposure", "Curve2", toneCurve.curve2, pedited->toneCurve.curve2);
            }

            assignFromKeyfile(keyFile, "Exposure", "HistogramMatching", toneCurve.histmatching, pedited->toneCurve.histmatching);
            if (ppVersion < 340) {
                toneCurve.fromHistMatching = false;
                if (pedited) {
                    pedited->toneCurve.fromHistMatching = true;
                }
            } else {
                assignFromKeyfile(keyFile, "Exposure", "CurveFromHistogramMatching", toneCurve.fromHistMatching, pedited->toneCurve.fromHistMatching);
            }
            assignFromKeyfile(keyFile, "Exposure", "ClampOOG", toneCurve.clampOOG, pedited->toneCurve.clampOOG);
        }

        if (keyFile.has_group("HLRecovery")) {
            assignFromKeyfile(keyFile, "HLRecovery", "Enabled", toneCurve.hrenabled, pedited->toneCurve.hrenabled);
            assignFromKeyfile(keyFile, "HLRecovery", "Method", toneCurve.method, pedited->toneCurve.method);
            assignFromKeyfile(keyFile, "HLRecovery", "Hlbl", toneCurve.hlbl, pedited->toneCurve.hlbl);
            assignFromKeyfile(keyFile, "HLRecovery", "Hlth", toneCurve.hlth, pedited->toneCurve.hlth);
        }

        if (keyFile.has_group("Channel Mixer")) {
            if (ppVersion >= 329) {
                assignFromKeyfile(keyFile, "Channel Mixer", "Enabled", chmixer.enabled, pedited->chmixer.enabled);
            } else {
                chmixer.enabled = true;

                if (pedited) {
                    pedited->chmixer.enabled = true;
                }
            }

            if (keyFile.has_key("Channel Mixer", "Red") && keyFile.has_key("Channel Mixer", "Green") && keyFile.has_key("Channel Mixer", "Blue")) {
                const std::vector<int> rmix = keyFile.get_integer_list("Channel Mixer", "Red");
                const std::vector<int> gmix = keyFile.get_integer_list("Channel Mixer", "Green");
                const std::vector<int> bmix = keyFile.get_integer_list("Channel Mixer", "Blue");

                if (rmix.size() == 3 && gmix.size() == 3 && bmix.size() == 3) {
                    memcpy(chmixer.red,   rmix.data(), 3 * sizeof(int));
                    memcpy(chmixer.green, gmix.data(), 3 * sizeof(int));
                    memcpy(chmixer.blue,  bmix.data(), 3 * sizeof(int));
                }
                if (ppVersion < 338) {
                    for (int i = 0; i < 3; ++i) {
                        chmixer.red[i] *= 10;
                        chmixer.green[i] *= 10;
                        chmixer.blue[i] *= 10;
                    }
                }

                if (pedited) {
                    pedited->chmixer.red[0] =   pedited->chmixer.red[1] =   pedited->chmixer.red[2] = true;
                    pedited->chmixer.green[0] = pedited->chmixer.green[1] = pedited->chmixer.green[2] = true;
                    pedited->chmixer.blue[0] =  pedited->chmixer.blue[1] =  pedited->chmixer.blue[2] = true;
                }
            }
        }

        if (keyFile.has_group("Black & White")) {
            assignFromKeyfile(keyFile, "Black & White", "Enabled", blackwhite.enabled, pedited->blackwhite.enabled);
            assignFromKeyfile(keyFile, "Black & White", "Method", blackwhite.method, pedited->blackwhite.method);
            assignFromKeyfile(keyFile, "Black & White", "Auto", blackwhite.autoc, pedited->blackwhite.autoc);
            assignFromKeyfile(keyFile, "Black & White", "ComplementaryColors", blackwhite.enabledcc, pedited->blackwhite.enabledcc);
            assignFromKeyfile(keyFile, "Black & White", "MixerRed", blackwhite.mixerRed, pedited->blackwhite.mixerRed);
            assignFromKeyfile(keyFile, "Black & White", "MixerOrange", blackwhite.mixerOrange, pedited->blackwhite.mixerOrange);
            assignFromKeyfile(keyFile, "Black & White", "MixerYellow", blackwhite.mixerYellow, pedited->blackwhite.mixerYellow);
            assignFromKeyfile(keyFile, "Black & White", "MixerGreen", blackwhite.mixerGreen, pedited->blackwhite.mixerGreen);
            assignFromKeyfile(keyFile, "Black & White", "MixerCyan", blackwhite.mixerCyan, pedited->blackwhite.mixerCyan);
            assignFromKeyfile(keyFile, "Black & White", "MixerBlue", blackwhite.mixerBlue, pedited->blackwhite.mixerBlue);
            assignFromKeyfile(keyFile, "Black & White", "MixerMagenta", blackwhite.mixerMagenta, pedited->blackwhite.mixerMagenta);
            assignFromKeyfile(keyFile, "Black & White", "MixerPurple", blackwhite.mixerPurple, pedited->blackwhite.mixerPurple);
            assignFromKeyfile(keyFile, "Black & White", "GammaRed", blackwhite.gammaRed, pedited->blackwhite.gammaRed);
            assignFromKeyfile(keyFile, "Black & White", "GammaGreen", blackwhite.gammaGreen, pedited->blackwhite.gammaGreen);
            assignFromKeyfile(keyFile, "Black & White", "GammaBlue", blackwhite.gammaBlue, pedited->blackwhite.gammaBlue);
            assignFromKeyfile(keyFile, "Black & White", "Filter", blackwhite.filter, pedited->blackwhite.filter);
            assignFromKeyfile(keyFile, "Black & White", "Setting", blackwhite.setting, pedited->blackwhite.setting);
            assignFromKeyfile(keyFile, "Black & White", "LuminanceCurve", blackwhite.luminanceCurve, pedited->blackwhite.luminanceCurve);

            assignFromKeyfile(keyFile, "Black & White", "BeforeCurve", blackwhite.beforeCurve, pedited->blackwhite.beforeCurve);

            assignFromKeyfile(keyFile, "Black & White", "Algorithm", blackwhite.algo, pedited->blackwhite.algo);
            assignFromKeyfile(
                keyFile,
                "Black & White",
                "BeforeCurveMode",
                {
                    {"Standard", BlackWhiteParams::TcMode::STD_BW},
                    {"FilmLike", BlackWhiteParams::TcMode::FILMLIKE_BW},
                    {"SatAndValueBlending", BlackWhiteParams::TcMode::SATANDVALBLENDING_BW},
                    {"WeightedStd", BlackWhiteParams::TcMode::WEIGHTEDSTD_BW}
                },
                blackwhite.beforeCurveMode,
                pedited->blackwhite.beforeCurveMode
            );

            assignFromKeyfile(keyFile, "Black & White", "AfterCurve", blackwhite.afterCurve, pedited->blackwhite.afterCurve);
            assignFromKeyfile(
                keyFile,
                "Black & White",
                "AfterCurveMode",
                {
                    {"Standard", BlackWhiteParams::TcMode::STD_BW},
                    {"WeightedStd", BlackWhiteParams::TcMode::WEIGHTEDSTD_BW}
                },
                blackwhite.afterCurveMode,
                pedited->blackwhite.afterCurveMode
            );
        }

        loadRetinexParams(keyFile, retinex, pedited);

        if (keyFile.has_group("Local Contrast")) {
            assignFromKeyfile(keyFile, "Local Contrast", "Enabled", localContrast.enabled, pedited->localContrast.enabled);
            assignFromKeyfile(keyFile, "Local Contrast", "Radius", localContrast.radius, pedited->localContrast.radius);
            assignFromKeyfile(keyFile, "Local Contrast", "Amount", localContrast.amount, pedited->localContrast.amount);
            assignFromKeyfile(keyFile, "Local Contrast", "Darkness", localContrast.darkness, pedited->localContrast.darkness);
            assignFromKeyfile(keyFile, "Local Contrast", "Lightness", localContrast.lightness, pedited->localContrast.lightness);
        }

        if (keyFile.has_group("Luminance Curve")) {
            if (ppVersion >= 329) {
                assignFromKeyfile(keyFile, "Luminance Curve", "Enabled", labCurve.enabled, pedited->labCurve.enabled);
            } else {
                labCurve.enabled = true;

                if (pedited) {
                    pedited->labCurve.enabled = true;
                }
            }

            assignFromKeyfile(keyFile, "Luminance Curve", "Brightness", labCurve.brightness, pedited->labCurve.brightness);
            assignFromKeyfile(keyFile, "Luminance Curve", "Contrast", labCurve.contrast, pedited->labCurve.contrast);

            if (ppVersion < 303) {
                // transform Saturation into Chromaticity
                // if Saturation == 0, should we set BWToning on?
                assignFromKeyfile(keyFile, "Luminance Curve", "Saturation", labCurve.chromaticity, pedited->labCurve.chromaticity);
                // transform AvoidColorClipping into AvoidColorShift
//                assignFromKeyfile(keyFile, "Luminance Curve", "AvoidColorClipping", labCurve.avoidcolorshift, pedited->labCurve.avoidcolorshift);
            } else {
                if (keyFile.has_key("Luminance Curve", "Chromaticity")) {
                    labCurve.chromaticity = keyFile.get_integer("Luminance Curve", "Chromaticity");

                    if (ppVersion >= 303 && ppVersion < 314 && labCurve.chromaticity == -100) {
                        blackwhite.enabled = true;
                    }

                    if (pedited) {
                        pedited->labCurve.chromaticity = true;
                    }
                }

                assignFromKeyfile(keyFile, "Luminance Curve", "RedAndSkinTonesProtection", labCurve.rstprotection, pedited->labCurve.rstprotection);
            }

            assignFromKeyfile(keyFile, "Luminance Curve", "LCredsk", labCurve.lcredsk, pedited->labCurve.lcredsk);

            if (ppVersion < 314) {
                // Backward compatibility: If BWtoning is true, Chromaticity has to be set to -100, which will produce the same effect
                // and will enable the b&w toning mode ('a' & 'b' curves)
                if (keyFile.has_key("Luminance Curve", "BWtoning")) {
                    if (keyFile.get_boolean("Luminance Curve", "BWtoning")) {
                        labCurve.chromaticity = -100;

                        if (pedited) {
                            pedited->labCurve.chromaticity = true;
                        }
                    }
                }
            }

            assignFromKeyfile(keyFile, "Luminance Curve", "LCurve", labCurve.lcurve, pedited->labCurve.lcurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "aCurve", labCurve.acurve, pedited->labCurve.acurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "bCurve", labCurve.bcurve, pedited->labCurve.bcurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "ccCurve", labCurve.cccurve, pedited->labCurve.cccurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "chCurve", labCurve.chcurve, pedited->labCurve.chcurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "lhCurve", labCurve.lhcurve, pedited->labCurve.lhcurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "hhCurve", labCurve.hhcurve, pedited->labCurve.hhcurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "LcCurve", labCurve.lccurve, pedited->labCurve.lccurve);
            assignFromKeyfile(keyFile, "Luminance Curve", "ClCurve", labCurve.clcurve, pedited->labCurve.clcurve);
            if (keyFile.has_key("Luminance Curve", "Gamutmunse")) {
                assignFromKeyfile(keyFile, "Luminance Curve", "Gamutmunse", labCurve.gamutmunselmethod, pedited->labCurve.gamutmunselmethod);
            } else {
                if (ppVersion < 303) {
                    if (keyFile.has_key("Luminance Curve", "AvoidColorClipping")) {
                        labCurve.gamutmunselmethod =
                            keyFile.get_boolean("Luminance Curve", "AvoidColorClipping") ? "LAB" : "NONE";
                        if (pedited) {
                           pedited->labCurve.gamutmunselmethod = true;
                        }
                    }
                } else if (keyFile.has_key("Luminance Curve", "AvoidColorShift")) {
                    labCurve.gamutmunselmethod =
                        keyFile.get_boolean("Luminance Curve", "AvoidColorShift") ? "LAB" : "NONE";
                   if (pedited) {
                       pedited->labCurve.gamutmunselmethod = true;
                   }
                }
           }
        }

        if (keyFile.has_group("Sharpening")) {
            assignFromKeyfile(keyFile, "Sharpening", "Enabled", sharpening.enabled, pedited->sharpening.enabled);

            if (ppVersion >= 334) {
                assignFromKeyfile(keyFile, "Sharpening", "Contrast", sharpening.contrast, pedited->sharpening.contrast);
            } else {
                sharpening.contrast = 0;

                if (pedited) {
                    pedited->sharpening.contrast = true;
                }
            }

            assignFromKeyfile(keyFile, "Sharpening", "Radius", sharpening.radius, pedited->sharpening.radius);
            assignFromKeyfile(keyFile, "Sharpening", "BlurRadius", sharpening.blurradius, pedited->sharpening.blurradius);
            assignFromKeyfile(keyFile, "Sharpening", "Amount", sharpening.amount, pedited->sharpening.amount);

            if (keyFile.has_key("Sharpening", "Threshold")) {
                if (ppVersion < 302) {
                    int thresh = min(keyFile.get_integer("Sharpening", "Threshold"), 2000);
                    sharpening.threshold.setValues(thresh, thresh, 2000, 2000);  // TODO: 2000 is the maximum value and is taken of rtgui/sharpening.cc ; should be changed by the tool modularization
                } else {
                    const std::vector<int> thresh = keyFile.get_integer_list("Sharpening", "Threshold");

                    if (thresh.size() >= 4) {
                        sharpening.threshold.setValues(thresh[0], thresh[1], min(thresh[2], 2000), min(thresh[3], 2000));
                    }
                }

                if (pedited) {
                    pedited->sharpening.threshold = true;
                }
            }

            assignFromKeyfile(keyFile, "Sharpening", "OnlyEdges", sharpening.edgesonly, pedited->sharpening.edgesonly);
            assignFromKeyfile(keyFile, "Sharpening", "EdgedetectionRadius", sharpening.edges_radius, pedited->sharpening.edges_radius);
            assignFromKeyfile(keyFile, "Sharpening", "EdgeTolerance", sharpening.edges_tolerance, pedited->sharpening.edges_tolerance);
            assignFromKeyfile(keyFile, "Sharpening", "HalocontrolEnabled", sharpening.halocontrol, pedited->sharpening.halocontrol);
            assignFromKeyfile(keyFile, "Sharpening", "HalocontrolAmount", sharpening.halocontrol_amount, pedited->sharpening.halocontrol_amount);
            assignFromKeyfile(keyFile, "Sharpening", "Method", sharpening.method, pedited->sharpening.method);
            assignFromKeyfile(keyFile, "Sharpening", "DeconvRadius", sharpening.deconvradius, pedited->sharpening.deconvradius);
            assignFromKeyfile(keyFile, "Sharpening", "DeconvAmount", sharpening.deconvamount, pedited->sharpening.deconvamount);
            assignFromKeyfile(keyFile, "Sharpening", "DeconvDamping", sharpening.deconvdamping, pedited->sharpening.deconvdamping);
            assignFromKeyfile(keyFile, "Sharpening", "DeconvIterations", sharpening.deconviter, pedited->sharpening.deconviter);
        }

        if (keyFile.has_group("SharpenEdge")) {
            assignFromKeyfile(keyFile, "SharpenEdge", "Enabled", sharpenEdge.enabled, pedited->sharpenEdge.enabled);
            assignFromKeyfile(keyFile, "SharpenEdge", "Passes", sharpenEdge.passes, pedited->sharpenEdge.passes);
            assignFromKeyfile(keyFile, "SharpenEdge", "Strength", sharpenEdge.amount, pedited->sharpenEdge.amount);
            assignFromKeyfile(keyFile, "SharpenEdge", "ThreeChannels", sharpenEdge.threechannels, pedited->sharpenEdge.threechannels);
        }

        if (keyFile.has_group("SharpenMicro")) {
            assignFromKeyfile(keyFile, "SharpenMicro", "Enabled", sharpenMicro.enabled, pedited->sharpenMicro.enabled);
            assignFromKeyfile(keyFile, "SharpenMicro", "Matrix", sharpenMicro.matrix, pedited->sharpenMicro.matrix);
            assignFromKeyfile(keyFile, "SharpenMicro", "Strength", sharpenMicro.amount, pedited->sharpenMicro.amount);

            if (ppVersion >= 334) {
                assignFromKeyfile(keyFile, "SharpenMicro", "Contrast", sharpenMicro.contrast, pedited->sharpenMicro.contrast);
            } else {
                sharpenMicro.contrast = 0;

                if (pedited) {
                    pedited->sharpenMicro.contrast = true;
                }
            }
            if (ppVersion >= 346) {
                assignFromKeyfile(keyFile, "SharpenMicro", "Uniformity", sharpenMicro.uniformity, pedited->sharpenMicro.uniformity);
            } else {
                double temp = 50.0;
                assignFromKeyfile(keyFile, "SharpenMicro", "Uniformity", temp, pedited->sharpenMicro.uniformity);
                sharpenMicro.uniformity = temp / 10;
            }
        }

        if (keyFile.has_group("Vibrance")) {
            assignFromKeyfile(keyFile, "Vibrance", "Enabled", vibrance.enabled, pedited->vibrance.enabled);
            assignFromKeyfile(keyFile, "Vibrance", "Pastels", vibrance.pastels, pedited->vibrance.pastels);
            assignFromKeyfile(keyFile, "Vibrance", "Saturated", vibrance.saturated, pedited->vibrance.saturated);

            if (keyFile.has_key("Vibrance", "PSThreshold")) {
                if (ppVersion < 302) {
                    int thresh = keyFile.get_integer("Vibrance", "PSThreshold");
                    vibrance.psthreshold.setValues(thresh, thresh);
                } else {
                    const std::vector<int> thresh = keyFile.get_integer_list("Vibrance", "PSThreshold");

                    if (thresh.size() >= 2) {
                        vibrance.psthreshold.setValues(thresh[0], thresh[1]);
                    }
                }

                if (pedited) {
                    pedited->vibrance.psthreshold = true;
                }
            }

            assignFromKeyfile(keyFile, "Vibrance", "ProtectSkins", vibrance.protectskins, pedited->vibrance.protectskins);
            assignFromKeyfile(keyFile, "Vibrance", "AvoidColorShift", vibrance.avoidcolorshift, pedited->vibrance.avoidcolorshift);
            assignFromKeyfile(keyFile, "Vibrance", "PastSatTog", vibrance.pastsattog, pedited->vibrance.pastsattog);
            assignFromKeyfile(keyFile, "Vibrance", "SkinTonesCurve", vibrance.skintonescurve, pedited->vibrance.skintonescurve);
        }
        if (ppVersion <= 346) { // 5.8 and earlier.
            wb.observer = StandardObserver::TWO_DEGREES;
            if (pedited) {
                pedited->wb.observer = true;
            }
        } else if (ppVersion <= 349) { // 5.9
            wb.observer = StandardObserver::TEN_DEGREES;
            if (pedited) {
                pedited->wb.observer = true;
            }
        }
        if (keyFile.has_group("White Balance")) {
            assignFromKeyfile(keyFile, "White Balance", "Enabled", wb.enabled, pedited->wb.enabled);
            assignFromKeyfile(keyFile, "White Balance", "Setting", wb.method, pedited->wb.method);
            if (wb.method == "Auto") {
                wb.method = "autitcgreen"; //"autold";
            }
            assignFromKeyfile(keyFile, "White Balance", "Temperature", wb.temperature, pedited->wb.temperature);
            assignFromKeyfile(keyFile, "White Balance", "Green", wb.green, pedited->wb.green);
            assignFromKeyfile(keyFile, "White Balance", "Equal", wb.equal, pedited->wb.equal);
            assignFromKeyfile(keyFile, "White Balance", "TemperatureBias", wb.tempBias, pedited->wb.tempBias);
            Glib::ustring standard_observer;
            assignFromKeyfile(keyFile, "White Balance", "StandardObserver", standard_observer, pedited->wb.observer);
            if (standard_observer == "TEN_DEGREES") {
                wb.observer = StandardObserver::TEN_DEGREES;
            } else if (standard_observer == "TWO_DEGREES") {
                wb.observer = StandardObserver::TWO_DEGREES;
            }
            assignFromKeyfile(keyFile, "White Balance", "Itcwb_green", wb.itcwb_green, pedited->wb.itcwb_green);
            assignFromKeyfile(keyFile, "White Balance", "Itcwb_rangegreen", wb.itcwb_rgreen, pedited->wb.itcwb_rgreen);
            assignFromKeyfile(keyFile, "White Balance", "Itcwb_nopurple", wb.itcwb_nopurple, pedited->wb.itcwb_nopurple);
            assignFromKeyfile(keyFile, "White Balance", "Itcwb_alg", wb.itcwb_alg, pedited->wb.itcwb_alg);
            assignFromKeyfile(keyFile, "White Balance", "Itcwb_prim", wb.itcwb_prim, pedited->wb.itcwb_prim);
            if (ppVersion <= 349) { // 5.9 and earlier.
                wb.itcwb_sampling = true;
                if (pedited) {
                    pedited->wb.itcwb_sampling = true;
                }
            }
            assignFromKeyfile(keyFile, "White Balance", "Itcwb_sampling", wb.itcwb_sampling, pedited->wb.itcwb_sampling);
            if (!assignFromKeyfile(keyFile, "White Balance", "CompatibilityVersion", wb.compat_version, pedited->wb.compat_version)) {
                bool compat_version_edited = true;
                if (ppVersion <= 346) { // 5.8 and earlier.
                    wb.compat_version = 0;
                } else if (ppVersion <= 349) { // 5.9.
                    wb.compat_version = 1;
                } else {
                    compat_version_edited = false;
                }
                if (pedited) {
                    pedited->wb.compat_version = pedited->wb.compat_version || compat_version_edited;
                }
            }
        }

        if (keyFile.has_group("Defringing")) {
            assignFromKeyfile(keyFile, "Defringing", "Enabled", defringe.enabled, pedited->defringe.enabled);
            assignFromKeyfile(keyFile, "Defringing", "Radius", defringe.radius, pedited->defringe.radius);

            if (keyFile.has_key("Defringing", "Threshold")) {
                defringe.threshold = (float)keyFile.get_integer("Defringing", "Threshold");

                if (pedited) {
                    pedited->defringe.threshold = true;
                }
            }

            if (ppVersion < 310) {
                defringe.threshold = sqrt(defringe.threshold * 33.f / 5.f);
            }

            assignFromKeyfile(keyFile, "Defringing", "HueCurve", defringe.huecurve, pedited->defringe.huecurve);
        }

        loadColorAppearanceParams(keyFile, colorappearance, pedited, ppVersion);

        if (keyFile.has_group("Impulse Denoising")) {
            assignFromKeyfile(keyFile, "Impulse Denoising", "Enabled", impulseDenoise.enabled, pedited->impulseDenoise.enabled);
            assignFromKeyfile(keyFile, "Impulse Denoising", "Threshold", impulseDenoise.thresh, pedited->impulseDenoise.thresh);
        }

        if (keyFile.has_group("Directional Pyramid Denoising")) { //TODO: No longer an accurate description for FT denoise
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Enabled", dirpyrDenoise.enabled, pedited->dirpyrDenoise.enabled);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Enhance", dirpyrDenoise.enhance, pedited->dirpyrDenoise.enhance);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Median", dirpyrDenoise.median, pedited->dirpyrDenoise.median);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Luma", dirpyrDenoise.luma, pedited->dirpyrDenoise.luma);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Ldetail", dirpyrDenoise.Ldetail, pedited->dirpyrDenoise.Ldetail);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Chroma", dirpyrDenoise.chroma, pedited->dirpyrDenoise.chroma);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Method", dirpyrDenoise.dmethod, pedited->dirpyrDenoise.dmethod);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "LMethod", dirpyrDenoise.Lmethod, pedited->dirpyrDenoise.Lmethod);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "CMethod", dirpyrDenoise.Cmethod, pedited->dirpyrDenoise.Cmethod);

            if (dirpyrDenoise.Cmethod == "PRE") {
                dirpyrDenoise.Cmethod = "MAN"; // Never load 'auto chroma preview mode' from pp3
            }

            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "C2Method", dirpyrDenoise.C2method, pedited->dirpyrDenoise.C2method);

            if (dirpyrDenoise.C2method == "PREV") {
                dirpyrDenoise.C2method = "MANU";
            }

            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "SMethod", dirpyrDenoise.smethod, pedited->dirpyrDenoise.smethod);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "MedMethod", dirpyrDenoise.medmethod, pedited->dirpyrDenoise.medmethod);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "MethodMed", dirpyrDenoise.methodmed, pedited->dirpyrDenoise.methodmed);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "RGBMethod", dirpyrDenoise.rgbmethod, pedited->dirpyrDenoise.rgbmethod);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "LCurve", dirpyrDenoise.lcurve, pedited->dirpyrDenoise.lcurve);

            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "CCCurve", dirpyrDenoise.cccurve, pedited->dirpyrDenoise.cccurve);

            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Redchro", dirpyrDenoise.redchro, pedited->dirpyrDenoise.redchro);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Bluechro", dirpyrDenoise.bluechro, pedited->dirpyrDenoise.bluechro);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "AutoGain", dirpyrDenoise.autoGain, pedited->dirpyrDenoise.gain);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Gamma", dirpyrDenoise.gamma, pedited->dirpyrDenoise.gamma);
            assignFromKeyfile(keyFile, "Directional Pyramid Denoising", "Passes", dirpyrDenoise.passes, pedited->dirpyrDenoise.passes);
        }

        if (keyFile.has_group("EPD")) {
            assignFromKeyfile(keyFile, "EPD", "Enabled", epd.enabled, pedited->epd.enabled);
            assignFromKeyfile(keyFile, "EPD", "Strength", epd.strength, pedited->epd.strength);
            assignFromKeyfile(keyFile, "EPD", "Gamma", epd.gamma, pedited->epd.gamma);
            assignFromKeyfile(keyFile, "EPD", "EdgeStopping", epd.edgeStopping, pedited->epd.edgeStopping);
            assignFromKeyfile(keyFile, "EPD", "Scale", epd.scale, pedited->epd.scale);
            assignFromKeyfile(keyFile, "EPD", "ReweightingIterates", epd.reweightingIterates, pedited->epd.reweightingIterates);
        }

        if (keyFile.has_group("FattalToneMapping")) {
            assignFromKeyfile(keyFile, "FattalToneMapping", "Enabled", fattal.enabled, pedited->fattal.enabled);
            assignFromKeyfile(keyFile, "FattalToneMapping", "Threshold", fattal.threshold, pedited->fattal.threshold);
            assignFromKeyfile(keyFile, "FattalToneMapping", "Amount", fattal.amount, pedited->fattal.amount);
            assignFromKeyfile(keyFile, "FattalToneMapping", "Anchor", fattal.anchor, pedited->fattal.anchor);
        }

        if (keyFile.has_group("Compression gamut")) {
            assignFromKeyfile(keyFile, "Compression gamut", "Enabled", cg.enabled, pedited->cg.enabled);
            assignFromKeyfile(keyFile, "Compression gamut", "th_c", cg.th_c, pedited->cg.th_c);
            assignFromKeyfile(keyFile, "Compression gamut", "th_m", cg.th_m, pedited->cg.th_m);
            assignFromKeyfile(keyFile, "Compression gamut", "th_y", cg.th_y, pedited->cg.th_y);
            assignFromKeyfile(keyFile, "Compression gamut", "d_c", cg.d_c, pedited->cg.d_c);
            assignFromKeyfile(keyFile, "Compression gamut", "Autodc", cg.autodc, pedited->cg.autodc);
            assignFromKeyfile(keyFile, "Compression gamut", "d_m", cg.d_m, pedited->cg.d_m);
            assignFromKeyfile(keyFile, "Compression gamut", "Autodm", cg.autodm, pedited->cg.autodm);
            assignFromKeyfile(keyFile, "Compression gamut", "d_y", cg.d_y, pedited->cg.d_y);
            assignFromKeyfile(keyFile, "Compression gamut", "Autody", cg.autody, pedited->cg.autody);
            assignFromKeyfile(keyFile, "Compression gamut", "pwr", cg.pwr, pedited->cg.pwr);
            assignFromKeyfile(keyFile, "Compression gamut", "colorspace", cg.colorspace, pedited->cg.colorspace);
            assignFromKeyfile(keyFile, "Compression gamut", "rolloff", cg.rolloff, pedited->cg.rolloff);
        }

        if (keyFile.has_group("Shadows & Highlights") && ppVersion >= 333) {
            assignFromKeyfile(keyFile, "Shadows & Highlights", "Enabled", sh.enabled, pedited->sh.enabled);
            assignFromKeyfile(keyFile, "Shadows & Highlights", "Highlights", sh.highlights, pedited->sh.highlights);
            assignFromKeyfile(keyFile, "Shadows & Highlights", "HighlightTonalWidth", sh.htonalwidth, pedited->sh.htonalwidth);
            assignFromKeyfile(keyFile, "Shadows & Highlights", "Shadows", sh.shadows, pedited->sh.shadows);
            assignFromKeyfile(keyFile, "Shadows & Highlights", "ShadowTonalWidth", sh.stonalwidth, pedited->sh.stonalwidth);
            assignFromKeyfile(keyFile, "Shadows & Highlights", "Radius", sh.radius, pedited->sh.radius);
            if (ppVersion >= 344) {
                assignFromKeyfile(keyFile, "Shadows & Highlights", "Lab", sh.lab, pedited->sh.lab);
            } else {
                sh.lab = true;
            }

            if (keyFile.has_key("Shadows & Highlights", "LocalContrast") && ppVersion < 329) {
                int lc = keyFile.get_integer("Shadows & Highlights", "LocalContrast");
                localContrast.amount = float(lc) / 30.f;

                if (pedited) {
                    pedited->localContrast.amount = true;
                }

                localContrast.enabled = sh.enabled;

                if (pedited) {
                    pedited->localContrast.enabled = true;
                }

                localContrast.radius = sh.radius;

                if (pedited) {
                    pedited->localContrast.radius = true;
                }
            }
        }

        if (keyFile.has_group("ToneEqualizer")) {
            assignFromKeyfile(keyFile, "ToneEqualizer", "Enabled", toneEqualizer.enabled, pedited->toneEqualizer.enabled);
            for (size_t i = 0; i < toneEqualizer.bands.size(); ++i) {
                assignFromKeyfile(keyFile, "ToneEqualizer", "Band" + std::to_string(i), toneEqualizer.bands[i], pedited->toneEqualizer.bands[i]);
            }
            assignFromKeyfile(keyFile, "ToneEqualizer", "Regularization", toneEqualizer.regularization, pedited->toneEqualizer.regularization);
            assignFromKeyfile(keyFile, "ToneEqualizer", "Pivot", toneEqualizer.pivot, pedited->toneEqualizer.pivot);
        }

        if (keyFile.has_group("Crop")) {
            assignFromKeyfile(keyFile, "Crop", "Enabled", crop.enabled, pedited->crop.enabled);
            assignFromKeyfile(keyFile, "Crop", "X", crop.x, pedited->crop.x);
            assignFromKeyfile(keyFile, "Crop", "Y", crop.y, pedited->crop.y);

            if (keyFile.has_key("Crop", "W")) {
                crop.w = std::max(keyFile.get_integer("Crop", "W"), 1);

                if (pedited) {
                    pedited->crop.w = true;
                }
            }

            if (keyFile.has_key("Crop", "H")) {
                crop.h = std::max(keyFile.get_integer("Crop", "H"), 1);

                if (pedited) {
                    pedited->crop.h = true;
                }
            }

            assignFromKeyfile(keyFile, "Crop", "FixedRatio", crop.fixratio, pedited->crop.fixratio);

            if (assignFromKeyfile(keyFile, "Crop", "Ratio", crop.ratio, pedited->crop.ratio)) {
                //backwards compatibility for crop.ratio
                if (crop.ratio == "DIN") {
                    crop.ratio = "1.414 - DIN EN ISO 216";
                }

                if (crop.ratio == "8.5:11") {
                    crop.ratio = "8.5:11 - US Letter";
                }

                if (crop.ratio == "11:17") {
                    crop.ratio = "11:17 - Tabloid";
                }
            }

            assignFromKeyfile(keyFile, "Crop", "Orientation", crop.orientation, pedited->crop.orientation);
        }

        loadCropGuideParams(keyFile, cropGuide, pedited->cropGuide);

        if (keyFile.has_group("Coarse Transformation")) {
            assignFromKeyfile(keyFile, "Coarse Transformation", "Rotate", coarse.rotate, pedited->coarse.rotate);
            assignFromKeyfile(keyFile, "Coarse Transformation", "HorizontalFlip", coarse.hflip, pedited->coarse.hflip);
            assignFromKeyfile(keyFile, "Coarse Transformation", "VerticalFlip", coarse.vflip, pedited->coarse.vflip);
        }

        if (keyFile.has_group("Rotation")) {
            assignFromKeyfile(keyFile, "Rotation", "Degree", rotate.degree, pedited->rotate.degree);
        }

        if (keyFile.has_group("Common Properties for Transformations")) {
            if (keyFile.has_key("Common Properties for Transformations", "Method")) {
                assignFromKeyfile(keyFile, "Common Properties for Transformations", "Method", commonTrans.method, pedited->commonTrans.method);
            } else {
                commonTrans.method = "lin";
            }
            if (keyFile.has_key("Common Properties for Transformations", "Scale")) {
                assignFromKeyfile(keyFile, "Common Properties for Transformations", "Scale", commonTrans.scale, pedited->commonTrans.scale);
            }
            if (keyFile.has_key("Common Properties for Transformations", "Scale horizontally")) {
                assignFromKeyfile(keyFile, "Common Properties for Transformations", "Scale horizontally", commonTrans.scale_horizontally, pedited->commonTrans.scale_horizontally);
            }
            if (keyFile.has_key("Common Properties for Transformations", "Scale vertically")) {
                assignFromKeyfile(keyFile, "Common Properties for Transformations", "Scale vertically", commonTrans.scale_vertically, pedited->commonTrans.scale_vertically);
            }
            assignFromKeyfile(keyFile, "Common Properties for Transformations", "AutoFill", commonTrans.autofill, pedited->commonTrans.autofill);
        }

        if (keyFile.has_group("Distortion")) {
            assignFromKeyfile(keyFile, "Distortion", "Amount", distortion.amount, pedited->distortion.amount);
            assignFromKeyfile(keyFile, "Distortion", "Defish", distortion.defish, pedited->distortion.defish);
            assignFromKeyfile(keyFile, "Distortion", "FocalLength", distortion.focal_length, pedited->distortion.focal_length);
        }

        if (keyFile.has_group("LensProfile")) {
            if (keyFile.has_key("LensProfile", "LcMode")) {
                lensProf.lcMode = lensProf.getMethodNumber(keyFile.get_string("LensProfile", "LcMode"));

                if (pedited) {
                    pedited->lensProf.lcMode = true;
                }
            }

            if (keyFile.has_key("LensProfile", "LCPFile")) {
				lensProf.lcpFile = expandRelativePath2(fname, options.rtSettings.lensProfilesPath, "", keyFile.get_string("LensProfile", "LCPFile"));

                if (pedited) {
                    pedited->lensProf.lcpFile = true;
                }

                if (ppVersion < 327 && !lensProf.lcpFile.empty()) {
                    lensProf.lcMode = LensProfParams::LcMode::LCP;
                }
            }

            assignFromKeyfile(keyFile, "LensProfile", "UseDistortion", lensProf.useDist, pedited->lensProf.useDist);
            assignFromKeyfile(keyFile, "LensProfile", "UseVignette", lensProf.useVign, pedited->lensProf.useVign);
            assignFromKeyfile(keyFile, "LensProfile", "UseCA", lensProf.useCA, pedited->lensProf.useCA);

            if (keyFile.has_key("LensProfile", "LFCameraMake")) {
                lensProf.lfCameraMake = keyFile.get_string("LensProfile", "LFCameraMake");

                if (pedited) {
                    pedited->lensProf.lfCameraMake = true;
                }
            }

            if (keyFile.has_key("LensProfile", "LFCameraModel")) {
                lensProf.lfCameraModel = keyFile.get_string("LensProfile", "LFCameraModel");

                if (pedited) {
                    pedited->lensProf.lfCameraModel = true;
                }
            }

            if (keyFile.has_key("LensProfile", "LFLens")) {
                lensProf.lfLens = keyFile.get_string("LensProfile", "LFLens");

                if (pedited) {
                    pedited->lensProf.lfLens = true;
                }
            }
        }

        if (keyFile.has_group("Perspective")) {
            assignFromKeyfile(keyFile, "Perspective", "Method", perspective.method, pedited->perspective.method);
            assignFromKeyfile(keyFile, "Perspective", "Horizontal", perspective.horizontal, pedited->perspective.horizontal);
            assignFromKeyfile(keyFile, "Perspective", "Vertical", perspective.vertical, pedited->perspective.vertical);
            assignFromKeyfile(keyFile, "Perspective", "CameraShiftHorizontal", perspective.camera_shift_horiz, pedited->perspective.camera_shift_horiz);
            assignFromKeyfile(keyFile, "Perspective", "CameraShiftVertical", perspective.camera_shift_vert, pedited->perspective.camera_shift_vert);
            assignFromKeyfile(keyFile, "Perspective", "CameraPitch", perspective.camera_pitch, pedited->perspective.camera_pitch);
            assignFromKeyfile(keyFile, "Perspective", "CameraRoll", perspective.camera_roll, pedited->perspective.camera_roll);
            assignFromKeyfile(keyFile, "Perspective", "CameraCropFactor", perspective.camera_crop_factor, pedited->perspective.camera_crop_factor);
            assignFromKeyfile(keyFile, "Perspective", "CameraFocalLength", perspective.camera_focal_length, pedited->perspective.camera_focal_length);
            assignFromKeyfile(keyFile, "Perspective", "CameraYaw", perspective.camera_yaw, pedited->perspective.camera_yaw);
            assignFromKeyfile(keyFile, "Perspective", "ProjectionPitch", perspective.projection_pitch, pedited->perspective.projection_pitch);
            assignFromKeyfile(keyFile, "Perspective", "ProjectionRotate", perspective.projection_rotate, pedited->perspective.projection_rotate);
            assignFromKeyfile(keyFile, "Perspective", "ProjectionShiftHorizontal", perspective.projection_shift_horiz, pedited->perspective.projection_shift_horiz);
            assignFromKeyfile(keyFile, "Perspective", "ProjectionShiftVertical", perspective.projection_shift_vert, pedited->perspective.projection_shift_vert);
            assignFromKeyfile(keyFile, "Perspective", "ProjectionYaw", perspective.projection_yaw, pedited->perspective.projection_yaw);
            if (keyFile.has_key("Perspective", "ControlLineValues") && keyFile.has_key("Perspective", "ControlLineTypes")) {
                perspective.control_line_values = keyFile.get_integer_list("Perspective", "ControlLineValues");
                perspective.control_line_types = keyFile.get_integer_list("Perspective", "ControlLineTypes");
                if (pedited) {
                    pedited->perspective.control_lines = true;
                }
            }
        }

        if (keyFile.has_group("Gradient")) {
            assignFromKeyfile(keyFile, "Gradient", "Enabled", gradient.enabled, pedited->gradient.enabled);
            assignFromKeyfile(keyFile, "Gradient", "Degree", gradient.degree, pedited->gradient.degree);
            assignFromKeyfile(keyFile, "Gradient", "Feather", gradient.feather, pedited->gradient.feather);
            assignFromKeyfile(keyFile, "Gradient", "Strength", gradient.strength, pedited->gradient.strength);
            assignFromKeyfile(keyFile, "Gradient", "CenterX", gradient.centerX, pedited->gradient.centerX);
            assignFromKeyfile(keyFile, "Gradient", "CenterY", gradient.centerY, pedited->gradient.centerY);
        }

        loadLocalLabParams(keyFile, locallab, pedited, ppVersion);

        if (keyFile.has_group("PCVignette")) {
            assignFromKeyfile(keyFile, "PCVignette", "Enabled", pcvignette.enabled, pedited->pcvignette.enabled);
            assignFromKeyfile(keyFile, "PCVignette", "Strength", pcvignette.strength, pedited->pcvignette.strength);
            assignFromKeyfile(keyFile, "PCVignette", "Feather", pcvignette.feather, pedited->pcvignette.feather);
            assignFromKeyfile(keyFile, "PCVignette", "Roundness", pcvignette.roundness, pedited->pcvignette.roundness);
        }

        if (keyFile.has_group("CACorrection")) {
            assignFromKeyfile(keyFile, "CACorrection", "Red", cacorrection.red, pedited->cacorrection.red);
            assignFromKeyfile(keyFile, "CACorrection", "Blue", cacorrection.blue, pedited->cacorrection.blue);
        }

        if (keyFile.has_group("Vignetting Correction")) {
            assignFromKeyfile(keyFile, "Vignetting Correction", "Amount", vignetting.amount, pedited->vignetting.amount);
            assignFromKeyfile(keyFile, "Vignetting Correction", "Radius", vignetting.radius, pedited->vignetting.radius);
            assignFromKeyfile(keyFile, "Vignetting Correction", "Strength", vignetting.strength, pedited->vignetting.strength);
            assignFromKeyfile(keyFile, "Vignetting Correction", "CenterX", vignetting.centerX, pedited->vignetting.centerX);
            assignFromKeyfile(keyFile, "Vignetting Correction", "CenterY", vignetting.centerY, pedited->vignetting.centerY);
        }

        if (keyFile.has_group("Resize")) {
            assignFromKeyfile(keyFile, "Resize", "Enabled", resize.enabled, pedited->resize.enabled);
            assignFromKeyfile(keyFile, "Resize", "Scale", resize.scale, pedited->resize.scale);
            assignFromKeyfile(keyFile, "Resize", "AppliesTo", resize.appliesTo, pedited->resize.appliesTo);
            assignFromKeyfile(keyFile, "Resize", "Method", resize.method, pedited->resize.method);
            assignFromKeyfile(keyFile, "Resize", "DataSpecified", resize.dataspec, pedited->resize.dataspec);
            assignFromKeyfile(keyFile, "Resize", "Width", resize.width, pedited->resize.width);
            assignFromKeyfile(keyFile, "Resize", "Height", resize.height, pedited->resize.height);
            assignFromKeyfile(keyFile, "Resize", "LongEdge", resize.longedge, pedited->resize.longedge);
            assignFromKeyfile(keyFile, "Resize", "ShortEdge", resize.shortedge, pedited->resize.shortedge);
            if (ppVersion >= 339) {
                assignFromKeyfile(keyFile, "Resize", "AllowUpscaling", resize.allowUpscaling, pedited->resize.allowUpscaling);
            } else {
                resize.allowUpscaling = false;
                if (pedited) {
                    pedited->resize.allowUpscaling = true;
                }
            }
        }

        loadFramingParams(keyFile, framing, pedited->framing);

        if (keyFile.has_group ("Spot removal")) {
            assignFromKeyfile(keyFile, "Spot removal", "Enabled", spot.enabled, pedited->spot.enabled);
            int i = 0;
            do {
                std::stringstream ss;
                ss << "Spot" << (i++ + 1);

                if (keyFile.has_key ("Spot removal", ss.str())) {
                    Glib::ArrayHandle<double> entry = keyFile.get_double_list ("Spot removal", ss.str());
                    const double epsilon = 0.001;  // to circumvent rounding of integer saved as double
                    SpotEntry se;

                    se.sourcePos.set(int(entry.data()[0] + epsilon), int(entry.data()[1] + epsilon));
                    se.targetPos.set(int(entry.data()[2] + epsilon), int(entry.data()[3] + epsilon));
                    se.radius  = LIM<int>(int  (entry.data()[4] + epsilon), SpotParams::minRadius, SpotParams::maxRadius);
                    se.feather = float(entry.data()[5]);
                    se.opacity = float(entry.data()[6]);
                    spot.entries.push_back(se);

                    if (pedited) {
                        pedited->spot.entries = true;
                    }
                } else {
                    break;
                }
            } while (1);
        }

        loadCaptureSharpeningParams(keyFile, pdsharpening, pedited);

        if (keyFile.has_group("PostResizeSharpening")) {
            assignFromKeyfile(keyFile, "PostResizeSharpening", "Enabled", prsharpening.enabled, pedited->prsharpening.enabled);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "Contrast", prsharpening.contrast, pedited->prsharpening.contrast);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "Radius", prsharpening.radius, pedited->prsharpening.radius);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "Amount", prsharpening.amount, pedited->prsharpening.amount);

            if (keyFile.has_key("PostResizeSharpening", "Threshold")) {
                if (ppVersion < 302) {
                    int thresh = min(keyFile.get_integer("PostResizeSharpening", "Threshold"), 2000);
                    prsharpening.threshold.setValues(thresh, thresh, 2000, 2000);  // TODO: 2000 is the maximum value and is taken of rtgui/sharpening.cc ; should be changed by the tool modularization
                } else {
                    const std::vector<int> thresh = keyFile.get_integer_list("PostResizeSharpening", "Threshold");

                    if (thresh.size() >= 4) {
                        prsharpening.threshold.setValues(thresh[0], thresh[1], min(thresh[2], 2000), min(thresh[3], 2000));
                    }
                }

                if (pedited) {
                    pedited->prsharpening.threshold = true;
                }
            }

            assignFromKeyfile(keyFile, "PostResizeSharpening", "OnlyEdges", prsharpening.edgesonly, pedited->prsharpening.edgesonly);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "EdgedetectionRadius", prsharpening.edges_radius, pedited->prsharpening.edges_radius);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "EdgeTolerance", prsharpening.edges_tolerance, pedited->prsharpening.edges_tolerance);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "HalocontrolEnabled", prsharpening.halocontrol, pedited->prsharpening.halocontrol);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "HalocontrolAmount", prsharpening.halocontrol_amount, pedited->prsharpening.halocontrol_amount);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "Method", prsharpening.method, pedited->prsharpening.method);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "DeconvRadius", prsharpening.deconvradius, pedited->prsharpening.deconvradius);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "DeconvAmount", prsharpening.deconvamount, pedited->prsharpening.deconvamount);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "DeconvDamping", prsharpening.deconvdamping, pedited->prsharpening.deconvdamping);
            assignFromKeyfile(keyFile, "PostResizeSharpening", "DeconvIterations", prsharpening.deconviter, pedited->prsharpening.deconviter);
        }

        if (keyFile.has_group("Color Management")) {
            if (keyFile.has_key("Color Management", "InputProfile")) {
				icm.inputProfile = expandRelativePath2(fname, options.rtSettings.cameraProfilesPath, "file:", keyFile.get_string("Color Management", "InputProfile"));

                if (pedited) {
                    pedited->icm.inputProfile = true;
                }
			}
            assignFromKeyfile(keyFile, "Color Management", "ToneCurve", icm.toneCurve, pedited->icm.toneCurve);
            assignFromKeyfile(keyFile, "Color Management", "ApplyLookTable", icm.applyLookTable, pedited->icm.applyLookTable);
            assignFromKeyfile(keyFile, "Color Management", "ApplyBaselineExposureOffset", icm.applyBaselineExposureOffset, pedited->icm.applyBaselineExposureOffset);
            assignFromKeyfile(keyFile, "Color Management", "ApplyHueSatMap", icm.applyHueSatMap, pedited->icm.applyHueSatMap);
            assignFromKeyfile(keyFile, "Color Management", "DCPIlluminant", icm.dcpIlluminant, pedited->icm.dcpIlluminant);
            assignFromKeyfile(keyFile, "Color Management", "WorkingProfile", icm.workingProfile, pedited->icm.workingProfile);
            assignFromKeyfile(keyFile, "Color Management", "OpacityCurveWLI", icm.opacityCurveWLI, pedited->icm.opacityCurveWLI);

            if (
                !assignFromKeyfile(
                    keyFile,
                    "Color Management",
                    "WorkingTRC",
                    {
                        {"none", ColorManagementParams::WorkingTrc::NONE},
                        {"Custom", ColorManagementParams::WorkingTrc::CUSTOM},
                        {"bt709", ColorManagementParams::WorkingTrc::BT709},
                        {"srgb", ColorManagementParams::WorkingTrc::SRGB},
                        {"22", ColorManagementParams::WorkingTrc::GAMMA_2_2},
                        {"18", ColorManagementParams::WorkingTrc::GAMMA_1_8},
                        {"lin", ColorManagementParams::WorkingTrc::LINEAR}
                    },
                    icm.workingTRC,
                    pedited->icm.workingTRC
                )
            ) {
               icm.workingTRC = ColorManagementParams::WorkingTrc::NONE;
               if (pedited) {
                   pedited->icm.workingTRC = true;
               }
            }

            if (
                !assignFromKeyfile(
                    keyFile,
                    "Color Management",
                    "Wwgamut",
                    {
                        {"none", ColorManagementParams::Wwgamut::NONE},
                        {"rec2020", ColorManagementParams::Wwgamut::REC2020},
                        {"adob", ColorManagementParams::Wwgamut::ADOBE},
                        {"rgb", ColorManagementParams::Wwgamut::SRGB},
                        {"dci", ColorManagementParams::Wwgamut::DCIP3}

                    },
                    icm.wgamut,
                    pedited->icm.wgamut
                )
            ) {
               icm.wgamut = ColorManagementParams::Wwgamut::NONE;
               if (pedited) {
                   pedited->icm.wgamut = true;
               }
            }

            
            if (
                !assignFromKeyfile(
                    keyFile,
                    "Color Management",
                    "Will",
                    {
                        {"def", ColorManagementParams::Illuminant::DEFAULT},
                        {"D41", ColorManagementParams::Illuminant::D41},
                        {"D50", ColorManagementParams::Illuminant::D50},
                        {"D55", ColorManagementParams::Illuminant::D55},
                        {"D60", ColorManagementParams::Illuminant::D60},
                        {"D65", ColorManagementParams::Illuminant::D65},
                        {"D80", ColorManagementParams::Illuminant::D80},
                        {"D120", ColorManagementParams::Illuminant::D120},
                        {"stda", ColorManagementParams::Illuminant::STDA},
                        {"2000", ColorManagementParams::Illuminant::TUNGSTEN_2000K},
                        {"1500", ColorManagementParams::Illuminant::TUNGSTEN_1500K},
                        {"E", ColorManagementParams::Illuminant::E}
                    },
                    icm.will,
                    pedited->icm.will
                )
            ) {
                icm.will = ColorManagementParams::Illuminant::DEFAULT;
                if (pedited) {
                    pedited->icm.will = true;
                }
            }
            if (
                !assignFromKeyfile(
                    keyFile,
                    "Color Management",
                    "Wprim",
                    {
                        {"def", ColorManagementParams::Primaries::DEFAULT},
                        {"srgb", ColorManagementParams::Primaries::SRGB},
                        {"adob", ColorManagementParams::Primaries::ADOBE_RGB},
                        {"prop", ColorManagementParams::Primaries::PRO_PHOTO},
                        {"rec", ColorManagementParams::Primaries::REC2020},
                        {"aces", ColorManagementParams::Primaries::ACES_P1},
                        {"wid", ColorManagementParams::Primaries::WIDE_GAMUT},
                        {"ac0", ColorManagementParams::Primaries::ACES_P0},
                        {"jdcmax", ColorManagementParams::Primaries::JDC_MAX},
                        {"jdcmaxstdA", ColorManagementParams::Primaries::JDC_MAXSTDA},
                        {"bru", ColorManagementParams::Primaries::BRUCE_RGB},
                        {"bet", ColorManagementParams::Primaries::BETA_RGB},
                        {"bst", ColorManagementParams::Primaries::BEST_RGB},
                        {"cus", ColorManagementParams::Primaries::CUSTOM},
                        {"cusgr", ColorManagementParams::Primaries::CUSTOM_GRID},
                        {"cuspol", ColorManagementParams::Primaries::CUSTOM_POL}
                        
                    },
                    icm.wprim,
                    pedited->icm.wprim
                )
            ) {
                icm.wprim = ColorManagementParams::Primaries::DEFAULT;
                if (pedited) {
                    pedited->icm.wprim = true;
                }
            }
            if (
                !assignFromKeyfile(
                    keyFile,
                    "Color Management",
                    "Wcat",
                    {
                        {"brad", ColorManagementParams::Cat::BRAD},
                        {"cat16", ColorManagementParams::Cat::CAT16},
                        {"cat02", ColorManagementParams::Cat::CAT02},
                        {"cat_vk", ColorManagementParams::Cat::CAT_VK},
                        {"cat_xyz", ColorManagementParams::Cat::CAT_XYZ}
                    },
                    icm.wcat,
                    pedited->icm.wcat
                )
            ){
                icm.wcat = ColorManagementParams::Cat::BRAD;
                if (pedited) {
                    pedited->icm.wcat = true;
                }
            }
            
            assignFromKeyfile(keyFile, "Color Management", "Gamut", icm.gamut, pedited->icm.gamut);
            assignFromKeyfile(keyFile, "Color Management", "WorkingTRCSlope", icm.wSlope, pedited->icm.wSlope);
            assignFromKeyfile(keyFile, "Color Management", "WorkingTRCsat", icm.wapsat, pedited->icm.wapsat);
            assignFromKeyfile(keyFile, "Color Management", "WorkingTRCGamma", icm.wGamma, pedited->icm.wGamma);
            assignFromKeyfile(keyFile, "Color Management", "Wmidtcie", icm.wmidtcie, pedited->icm.wmidtcie);
            assignFromKeyfile(keyFile, "Color Management", "Wsmoothcie", icm.wsmoothcie, pedited->icm.wsmoothcie);
            if (ppVersion >= 353) {
                assignFromKeyfile(keyFile, "Color Management", "Wsmoothciesli", icm.wsmoothciesli, pedited->icm.wsmoothciesli);
            } else {
                if(icm.wsmoothcie == true) {
                    icm.wsmoothciesli = 0.5;
                }
                if (pedited) {
                    pedited->icm.wsmoothciesli = true;
                }
            }
            assignFromKeyfile(keyFile, "Color Management", "Wgampower", icm.wgampower, pedited->icm.wgampower);
            assignFromKeyfile(keyFile, "Color Management", "Wgamgain", icm.wgamgain, pedited->icm.wgamgain);

            assignFromKeyfile(keyFile, "Color Management", "Sigmatrc", icm.sigmatrc, pedited->icm.sigmatrc);
            assignFromKeyfile(keyFile, "Color Management", "Offstrc", icm.offstrc, pedited->icm.offstrc);
            assignFromKeyfile(keyFile, "Color Management", "Pyrwavtrc", icm.pyrwavtrc, pedited->icm.pyrwavtrc);
            assignFromKeyfile(keyFile, "Color Management", "Residtrc", icm.residtrc, pedited->icm.residtrc);

            assignFromKeyfile(keyFile, "Color Management", "Redx", icm.redx, pedited->icm.redx);
            assignFromKeyfile(keyFile, "Color Management", "Redy", icm.redy, pedited->icm.redy);
            assignFromKeyfile(keyFile, "Color Management", "Grex", icm.grex, pedited->icm.grex);
            assignFromKeyfile(keyFile, "Color Management", "Grey", icm.grey, pedited->icm.grey);
            assignFromKeyfile(keyFile, "Color Management", "Blux", icm.blux, pedited->icm.blux);
            assignFromKeyfile(keyFile, "Color Management", "Bluy", icm.bluy, pedited->icm.bluy);
            
            assignFromKeyfile(keyFile, "Color Management", "Redrot", icm.redrot, pedited->icm.redrot);
            assignFromKeyfile(keyFile, "Color Management", "Redsat", icm.redsat, pedited->icm.redsat);
            assignFromKeyfile(keyFile, "Color Management", "Grerot", icm.grerot, pedited->icm.grerot);
            assignFromKeyfile(keyFile, "Color Management", "Gresat", icm.gresat, pedited->icm.gresat);
            assignFromKeyfile(keyFile, "Color Management", "Blurot", icm.blurot, pedited->icm.blurot);
            assignFromKeyfile(keyFile, "Color Management", "Blusat", icm.blusat, pedited->icm.blusat);
            
            assignFromKeyfile(keyFile, "Color Management", "Refi", icm.refi, pedited->icm.refi);
            assignFromKeyfile(keyFile, "Color Management", "Shiftx", icm.shiftx, pedited->icm.shiftx);
            assignFromKeyfile(keyFile, "Color Management", "Shifty", icm.shifty, pedited->icm.shifty);
            assignFromKeyfile(keyFile, "Color Management", "Preser", icm.preser, pedited->icm.preser);
            assignFromKeyfile(keyFile, "Color Management", "Fbw", icm.fbw, pedited->icm.fbw);
            assignFromKeyfile(keyFile, "Color Management", "TrcExp", icm.trcExp, pedited->icm.trcExp);
            assignFromKeyfile(keyFile, "Color Management", "WavExp", icm.wavExp, pedited->icm.wavExp);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieALow", icm.labgridcieALow, pedited->icm.labgridcieALow);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieBLow", icm.labgridcieBLow, pedited->icm.labgridcieBLow);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieAHigh", icm.labgridcieAHigh, pedited->icm.labgridcieAHigh);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieBHigh", icm.labgridcieBHigh, pedited->icm.labgridcieBHigh);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieGx", icm.labgridcieGx, pedited->icm.labgridcieGx);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieGy", icm.labgridcieGy, pedited->icm.labgridcieGy);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieWx", icm.labgridcieWx, pedited->icm.labgridcieWx);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieWy", icm.labgridcieWy, pedited->icm.labgridcieWy);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieMx", icm.labgridcieMx, pedited->icm.labgridcieMx);
            assignFromKeyfile(keyFile, "Color Management", "LabGridcieMy", icm.labgridcieMy, pedited->icm.labgridcieMy);
            if (keyFile.has_key("Color Management", "aIntent")) {
                Glib::ustring intent = keyFile.get_string("Color Management", "aIntent");

                if (intent == "Perceptual") {
                    icm.aRendIntent = RI_PERCEPTUAL;
                } else if (intent == "Relative") {
                    icm.aRendIntent = RI_RELATIVE;
                } else if (intent == "Saturation") {
                    icm.aRendIntent = RI_SATURATION;
                } else if (intent == "Absolute") {
                    icm.aRendIntent = RI_ABSOLUTE;
                }

                if (pedited) {
                    pedited->icm.aRendIntent = true;
                }
            }

            assignFromKeyfile(keyFile, "Color Management", "OutputProfile", icm.outputProfile, pedited->icm.outputProfile);
            if (ppVersion < 341) {
                if (icm.outputProfile == "RT_Medium_gsRGB") {
                    icm.outputProfile = "RTv4_Medium";
                } else if (icm.outputProfile == "RT_Large_gBT709" || icm.outputProfile == "RT_Large_g10" || icm.outputProfile == "RT_Large_gsRGB") {
                    icm.outputProfile = "RTv4_Large";
                } else if (icm.outputProfile == "WideGamutRGB") {
                    icm.outputProfile = "RTv4_Wide";
                } else if (icm.outputProfile == "RT_sRGB_gBT709" || icm.outputProfile == "RT_sRGB_g10" || icm.outputProfile == "RT_sRGB") {
                    icm.outputProfile = "RTv4_sRGB";
                } else if (icm.outputProfile == "BetaRGB") { // Have we ever provided this profile ? Should we convert this filename ?
                    icm.outputProfile = "RTv4_Beta";
                } else if (icm.outputProfile == "BestRGB") { // Have we ever provided this profile ? Should we convert this filename ?
                    icm.outputProfile = "RTv4_Best";
                } else if (icm.outputProfile == "Rec2020") {
                    icm.outputProfile = "RTv4_Rec2020";
                } else if (icm.outputProfile == "Bruce") { // Have we ever provided this profile ? Should we convert this filename ?
                    icm.outputProfile = "RTv4_Bruce";
                } else if (icm.outputProfile == "ACES") {
                    icm.outputProfile = "RTv4_ACES-AP0";
                }
            }
            if (keyFile.has_key("Color Management", "OutputProfileIntent")) {
                Glib::ustring intent = keyFile.get_string("Color Management", "OutputProfileIntent");

                if (intent == "Perceptual") {
                    icm.outputIntent = RI_PERCEPTUAL;
                } else if (intent == "Relative") {
                    icm.outputIntent = RI_RELATIVE;
                } else if (intent == "Saturation") {
                    icm.outputIntent = RI_SATURATION;
                } else if (intent == "Absolute") {
                    icm.outputIntent = RI_ABSOLUTE;
                }

                if (pedited) {
                    pedited->icm.outputIntent = true;
                }
            }
            assignFromKeyfile(keyFile, "Color Management", "OutputBPC", icm.outputBPC, pedited->icm.outputBPC);
            assignFromKeyfile(keyFile, "Color Management", "OutputPaperwhiteNits", icm.outputPaperwhiteNits, pedited->icm.outputPaperwhiteNits);
            assignFromKeyfile(keyFile, "Color Management", "OutputMaxNits", icm.outputMaxNits, pedited->icm.outputMaxNits);
        }

        loadWaveletParams(keyFile, wavelet, pedited, ppVersion);

        if (keyFile.has_group("Directional Pyramid Equalizer")) {
            assignFromKeyfile(keyFile, "Directional Pyramid Equalizer", "Enabled", dirpyrequalizer.enabled, pedited->dirpyrequalizer.enabled);
            assignFromKeyfile(keyFile, "Directional Pyramid Equalizer", "Gamutlab", dirpyrequalizer.gamutlab, pedited->dirpyrequalizer.gamutlab);
            assignFromKeyfile(keyFile, "Directional Pyramid Equalizer", "cbdlMethod", dirpyrequalizer.cbdlMethod, pedited->dirpyrequalizer.cbdlMethod);

            if (keyFile.has_key("Directional Pyramid Equalizer", "Hueskin")) {
                const std::vector<int> thresh = keyFile.get_integer_list("Directional Pyramid Equalizer", "Hueskin");

                if (thresh.size() >= 4) {
                    dirpyrequalizer.hueskin.setValues(thresh[0], thresh[1], min(thresh[2], 300), min(thresh[3], 300));
                }

                if (pedited) {
                    pedited->dirpyrequalizer.hueskin = true;
                }
            }

            if (ppVersion < 316) {
                for (int i = 0; i < 5; i ++) {
                    std::stringstream ss;
                    ss << "Mult" << i;

                    if (keyFile.has_key("Directional Pyramid Equalizer", ss.str())) {
                        if (i == 4) {
                            dirpyrequalizer.threshold = keyFile.get_double("Directional Pyramid Equalizer", ss.str());

                            if (pedited) {
                                pedited->dirpyrequalizer.threshold = true;
                            }
                        } else {
                            dirpyrequalizer.mult[i] = keyFile.get_double("Directional Pyramid Equalizer", ss.str());

                            if (pedited) {
                                pedited->dirpyrequalizer.mult[i] = true;
                            }
                        }
                    }
                }

                dirpyrequalizer.mult[4] = 1.0;
            } else {
                // 5 level wavelet + dedicated threshold parameter
                for (int i = 0; i < 6; i ++) {
                    std::stringstream ss;
                    ss << "Mult" << i;

                    if (keyFile.has_key("Directional Pyramid Equalizer", ss.str())) {
                        dirpyrequalizer.mult[i] = keyFile.get_double("Directional Pyramid Equalizer", ss.str());

                        if (pedited) {
                            pedited->dirpyrequalizer.mult[i] = true;
                        }
                    }
                }

                assignFromKeyfile(keyFile, "Directional Pyramid Equalizer", "Threshold", dirpyrequalizer.threshold, pedited->dirpyrequalizer.threshold);
                assignFromKeyfile(keyFile, "Directional Pyramid Equalizer", "Skinprotect", dirpyrequalizer.skinprotect, pedited->dirpyrequalizer.skinprotect);
            }
        }

        if (keyFile.has_group("SoftLight")) {
            assignFromKeyfile(keyFile, "SoftLight", "Enabled", softlight.enabled, pedited->softlight.enabled);
            assignFromKeyfile(keyFile, "SoftLight", "Strength", softlight.strength, pedited->softlight.strength);
        }

        if (keyFile.has_group("Dehaze")) {
            assignFromKeyfile(keyFile, "Dehaze", "Enabled", dehaze.enabled, pedited->dehaze.enabled);
            assignFromKeyfile(keyFile, "Dehaze", "Strength", dehaze.strength, pedited->dehaze.strength);
            assignFromKeyfile(keyFile, "Dehaze", "ShowDepthMap", dehaze.showDepthMap, pedited->dehaze.showDepthMap);
            assignFromKeyfile(keyFile, "Dehaze", "Depth", dehaze.depth, pedited->dehaze.depth);
            if (ppVersion < 349 && dehaze.enabled && keyFile.has_key("Dehaze", "Luminance")) {
                const bool luminance = keyFile.get_boolean("Dehaze", "Luminance");
                dehaze.saturation = luminance ? 0 : 100;
                if (pedited) {
                    pedited->dehaze.saturation = true;
                }
            } else {
                assignFromKeyfile(keyFile, "Dehaze", "Saturation", dehaze.saturation, pedited->dehaze.saturation);
            }
        }

        if (keyFile.has_group("Film Simulation")) {
            assignFromKeyfile(keyFile, "Film Simulation", "Enabled", filmSimulation.enabled, pedited->filmSimulation.enabled);
			assignFromKeyfile(keyFile, "Film Simulation", "ClutFilename", filmSimulation.clutFilename, pedited->filmSimulation.clutFilename);
			#if defined (_WIN32)
			// if this is Windows, replace any "/" in the filename with "\\"
			size_t pos = filmSimulation.clutFilename.find("/");
			while (pos != string::npos) {
				filmSimulation.clutFilename.replace(pos, 1, "\\");
				pos = filmSimulation.clutFilename.find("/", pos);
			}
			#endif
			#if !defined (_WIN32)
			// if this is not Windows, replace any "\\" in the filename with "/"
			size_t pos = filmSimulation.clutFilename.find("\\");
			while (pos != string::npos) {
				filmSimulation.clutFilename.replace(pos, 1, "/");
				pos = filmSimulation.clutFilename.find("\\", pos);
			}
			#endif

            if (keyFile.has_key("Film Simulation", "Strength")) {
                if (ppVersion < 321) {
                    filmSimulation.strength = keyFile.get_double("Film Simulation", "Strength") * 100 + 0.1;
                } else {
                    filmSimulation.strength = keyFile.get_integer("Film Simulation", "Strength");
                }

                if (pedited) {
                    pedited->filmSimulation.strength = true;
                }
            }
        }

        if (keyFile.has_group("HSV Equalizer")) {
            if (ppVersion >= 329) {
                assignFromKeyfile(keyFile, "HSV Equalizer", "Enabled", hsvequalizer.enabled, pedited->hsvequalizer.enabled);
            } else {
                hsvequalizer.enabled = true;

                if (pedited) {
                    pedited->hsvequalizer.enabled = true;
                }
            }

            if (ppVersion >= 300) {
                assignFromKeyfile(keyFile, "HSV Equalizer", "HCurve", hsvequalizer.hcurve, pedited->hsvequalizer.hcurve);
                assignFromKeyfile(keyFile, "HSV Equalizer", "SCurve", hsvequalizer.scurve, pedited->hsvequalizer.scurve);
                assignFromKeyfile(keyFile, "HSV Equalizer", "VCurve", hsvequalizer.vcurve, pedited->hsvequalizer.vcurve);
            }
        }

        if (keyFile.has_group("RGB Curves")) {
            if (ppVersion >= 329) {
                assignFromKeyfile(keyFile, "RGB Curves", "Enabled", rgbCurves.enabled, pedited->rgbCurves.enabled);
            } else {
                rgbCurves.enabled = true;

                if (pedited) {
                    pedited->rgbCurves.enabled = true;
                }
            }

            assignFromKeyfile(keyFile, "RGB Curves", "LumaMode", rgbCurves.lumamode, pedited->rgbCurves.lumamode);
            assignFromKeyfile(keyFile, "RGB Curves", "rCurve", rgbCurves.rcurve, pedited->rgbCurves.rcurve);
            assignFromKeyfile(keyFile, "RGB Curves", "gCurve", rgbCurves.gcurve, pedited->rgbCurves.gcurve);
            assignFromKeyfile(keyFile, "RGB Curves", "bCurve", rgbCurves.bcurve, pedited->rgbCurves.bcurve);
        }

        if (keyFile.has_group("ColorToning")) {
            assignFromKeyfile(keyFile, "ColorToning", "Enabled", colorToning.enabled, pedited->colorToning.enabled);
            assignFromKeyfile(keyFile, "ColorToning", "Method", colorToning.method, pedited->colorToning.method);
            assignFromKeyfile(keyFile, "ColorToning", "Lumamode", colorToning.lumamode, pedited->colorToning.lumamode);
            assignFromKeyfile(keyFile, "ColorToning", "Twocolor", colorToning.twocolor, pedited->colorToning.twocolor);
            assignFromKeyfile(keyFile, "ColorToning", "OpacityCurve", colorToning.opacityCurve, pedited->colorToning.opacityCurve);
            assignFromKeyfile(keyFile, "ColorToning", "ColorCurve", colorToning.colorCurve, pedited->colorToning.colorCurve);
            assignFromKeyfile(keyFile, "ColorToning", "Autosat", colorToning.autosat, pedited->colorToning.autosat);
            assignFromKeyfile(keyFile, "ColorToning", "SatProtectionThreshold", colorToning.satProtectionThreshold, pedited->colorToning.satprotectionthreshold);
            assignFromKeyfile(keyFile, "ColorToning", "SaturatedOpacity", colorToning.saturatedOpacity, pedited->colorToning.saturatedopacity);
            assignFromKeyfile(keyFile, "ColorToning", "Strength", colorToning.strength, pedited->colorToning.strength);

            if (keyFile.has_key("ColorToning", "HighlightsColorSaturation")) {
                const std::vector<int> thresh = keyFile.get_integer_list("ColorToning", "HighlightsColorSaturation");

                if (thresh.size() >= 2) {
                    colorToning.hlColSat.setValues(thresh[0], thresh[1]);
                }

                if (pedited) {
                    pedited->colorToning.hlColSat = true;
                }
            }

            if (keyFile.has_key("ColorToning", "ShadowsColorSaturation")) {
                const std::vector<int> thresh = keyFile.get_integer_list("ColorToning", "ShadowsColorSaturation");

                if (thresh.size() >= 2) {
                    colorToning.shadowsColSat.setValues(thresh[0], thresh[1]);
                }

                if (pedited) {
                    pedited->colorToning.shadowsColSat = true;
                }
            }

            assignFromKeyfile(keyFile, "ColorToning", "ClCurve", colorToning.clcurve, pedited->colorToning.clcurve);
            assignFromKeyfile(keyFile, "ColorToning", "Cl2Curve", colorToning.cl2curve, pedited->colorToning.cl2curve);
            assignFromKeyfile(keyFile, "ColorToning", "Redlow", colorToning.redlow, pedited->colorToning.redlow);
            assignFromKeyfile(keyFile, "ColorToning", "Greenlow", colorToning.greenlow, pedited->colorToning.greenlow);
            assignFromKeyfile(keyFile, "ColorToning", "Bluelow", colorToning.bluelow, pedited->colorToning.bluelow);
            assignFromKeyfile(keyFile, "ColorToning", "Satlow", colorToning.satlow, pedited->colorToning.satlow);
            assignFromKeyfile(keyFile, "ColorToning", "Balance", colorToning.balance, pedited->colorToning.balance);
            assignFromKeyfile(keyFile, "ColorToning", "Sathigh", colorToning.sathigh, pedited->colorToning.sathigh);
            assignFromKeyfile(keyFile, "ColorToning", "Redmed", colorToning.redmed, pedited->colorToning.redmed);
            assignFromKeyfile(keyFile, "ColorToning", "Greenmed", colorToning.greenmed, pedited->colorToning.greenmed);
            assignFromKeyfile(keyFile, "ColorToning", "Bluemed", colorToning.bluemed, pedited->colorToning.bluemed);
            assignFromKeyfile(keyFile, "ColorToning", "Redhigh", colorToning.redhigh, pedited->colorToning.redhigh);
            assignFromKeyfile(keyFile, "ColorToning", "Greenhigh", colorToning.greenhigh, pedited->colorToning.greenhigh);
            assignFromKeyfile(keyFile, "ColorToning", "Bluehigh", colorToning.bluehigh, pedited->colorToning.bluehigh);

            assignFromKeyfile(keyFile, "ColorToning", "LabGridALow", colorToning.labgridALow, pedited->colorToning.labgridALow);
            assignFromKeyfile(keyFile, "ColorToning", "LabGridBLow", colorToning.labgridBLow, pedited->colorToning.labgridBLow);
            assignFromKeyfile(keyFile, "ColorToning", "LabGridAHigh", colorToning.labgridAHigh, pedited->colorToning.labgridAHigh);
            assignFromKeyfile(keyFile, "ColorToning", "LabGridBHigh", colorToning.labgridBHigh, pedited->colorToning.labgridBHigh);
            if (ppVersion < 337) {
                const double scale = ColorToningParams::LABGRID_CORR_SCALE;
                colorToning.labgridALow *= scale;
                colorToning.labgridAHigh *= scale;
                colorToning.labgridBLow *= scale;
                colorToning.labgridBHigh *= scale;
            }
            std::vector<ColorToningParams::LabCorrectionRegion> lg;
            bool found = false;
            bool done = false;
            for (int i = 1; !done; ++i) {
                ColorToningParams::LabCorrectionRegion cur;
                done = true;
                std::string n = std::to_string(i);
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionA_") + n, cur.a, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionB_") + n, cur.b, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionSaturation_") + n, cur.saturation, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionSlope_") + n, cur.slope, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionOffset_") + n, cur.offset, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionPower_") + n, cur.power, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionHueMask_") + n, cur.hueMask, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionChromaticityMask_") + n, cur.chromaticityMask, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionLightnessMask_") + n, cur.lightnessMask, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionMaskBlur_") + n, cur.maskBlur, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (assignFromKeyfile(keyFile, "ColorToning", Glib::ustring("LabRegionChannel_") + n, cur.channel, pedited->colorToning.labregions)) {
                    found = true;
                    done = false;
                }
                if (!done) {
                    lg.emplace_back(cur);
                }
            }
            if (found) {
                colorToning.labregions = std::move(lg);
            }
            assignFromKeyfile(keyFile, "ColorToning", "LabRegionsShowMask", colorToning.labregionsShowMask, pedited->colorToning.labregionsShowMask);
        }

        loadRawParams(keyFile, raw, pedited, fname, ppVersion);

        if (keyFile.has_group("Film Negative")) {
            assignFromKeyfile(keyFile, "Film Negative", "Enabled", filmNegative.enabled, pedited->filmNegative.enabled);
            assignFromKeyfile(keyFile, "Film Negative", "RedRatio", filmNegative.redRatio, pedited->filmNegative.redRatio);
            assignFromKeyfile(keyFile, "Film Negative", "GreenExponent", filmNegative.greenExp, pedited->filmNegative.greenExp);
            assignFromKeyfile(keyFile, "Film Negative", "BlueRatio", filmNegative.blueRatio, pedited->filmNegative.blueRatio);

            if (ppVersion < 347) {
                // Backwards compatibility with RT v5.8
                filmNegative.colorSpace = FilmNegativeParams::ColorSpace::INPUT;
                filmNegative.backCompat = FilmNegativeParams::BackCompat::V1;
                if (pedited) {
                    pedited->filmNegative.refInput = true;
                    pedited->filmNegative.refOutput = true;
                    pedited->filmNegative.colorSpace = true;
                }

            } else if (!keyFile.has_key("Film Negative", "RefInput")) {
                // Backwards compatibility with intermediate dev version (after v5.8) using film base values
                bool r, g, b;
                assignFromKeyfile(keyFile, "Film Negative", "RedBase", filmNegative.refInput.r, r);
                assignFromKeyfile(keyFile, "Film Negative", "GreenBase", filmNegative.refInput.g, g);
                assignFromKeyfile(keyFile, "Film Negative", "BlueBase", filmNegative.refInput.b, b);
                if (pedited) {
                    pedited->filmNegative.refInput = r || g || b;
                    pedited->filmNegative.refOutput = r || g || b;
                    pedited->filmNegative.colorSpace = true;
                }

                filmNegative.colorSpace = FilmNegativeParams::ColorSpace::INPUT;
                // Special value -1 used to mean that this should be treated as a v5.8 profile
                filmNegative.backCompat = (filmNegative.refInput.r == -1.f)
                    ? FilmNegativeParams::BackCompat::V1
                    : FilmNegativeParams::BackCompat::V2;

            } else { // current version

                assignRgbFromKeyfile(keyFile, "Film Negative", "RefInput", filmNegative.refInput, pedited->filmNegative.refInput);
                assignRgbFromKeyfile(keyFile, "Film Negative", "RefOutput", filmNegative.refOutput, pedited->filmNegative.refOutput);

                int cs = toUnderlying(filmNegative.colorSpace);
                assignFromKeyfile(keyFile, "Film Negative", "ColorSpace", cs, pedited->filmNegative.colorSpace);
                filmNegative.colorSpace = static_cast<FilmNegativeParams::ColorSpace>(cs);

                if (keyFile.has_key("Film Negative", "BackCompat")) {
                    filmNegative.backCompat = FilmNegativeParams::BackCompat(keyFile.get_integer("Film Negative", "BackCompat"));
                }

            }
        }

        if (keyFile.has_group("MetaData")) {
            int mode = int(MetaDataParams::EDIT);
            assignFromKeyfile(keyFile, "MetaData", "Mode", mode, pedited->metadata.mode);

            if (mode >= int(MetaDataParams::TUNNEL) && mode <= int(MetaDataParams::STRIP)) {
                metadata.mode = static_cast<MetaDataParams::Mode>(mode);
            }

            assignFromKeyfile(keyFile, "MetaData", "ExifKeys", metadata.exifKeys, pedited->metadata.exifKeys);
        }

        if (keyFile.has_group("Exif")) {
            for (const auto& key : keyFile.get_keys("Exif")) {
                auto it = exif_keys.find(key);
                if (it != exif_keys.end()) {
                    metadata.exif[it->second] = keyFile.get_string("Exif", key);

                    if (pedited) {
                        pedited->exif = true;
                    }
                }
            }
        }

        /*
         * Load iptc change settings
         *
         * Existing values are preserved, and the stored values
         * are added to the list. To reset a field, the user has to
         * save the profile with the field leaved empty, but still
         * terminated by a semi-column ";"
         *
         * Please note that the old Keywords and SupplementalCategories
         * tag content is fully replaced by the new one,
         * i.e. they don't merge
         */
        if (keyFile.has_group("IPTC")) {
            for (const auto& key : keyFile.get_keys("IPTC")) {
                // does this key already exist?
                auto it = iptc_keys.find(key);
                if (it == iptc_keys.end()) {
                    continue;
                }

                auto kk = it->second;
                const IPTCPairs::iterator element = metadata.iptc.find(kk);

                if (element != metadata.iptc.end()) {
                    // it already exist so we cleanup the values
                    element->second.clear();
                }

                // TODO: look out if merging Keywords and SupplementalCategories from the procparams chain would be interesting
                for (const auto& currLoadedTagValue : keyFile.get_string_list("IPTC", key)) {
                    metadata.iptc[kk].push_back(currLoadedTagValue);
                }

                if (pedited) {
                    pedited->iptc = true;
                }
            }
        }

        return 0;
    } catch (const Glib::Error& e) {
        printf("-->%s\n", e.what().c_str());
        setDefaults();
        return 1;
    } catch (...) {
        printf("-->unknown exception!\n");
        setDefaults();
        return 1;
    }

    return 0;
}

ProcParams* ProcParams::create()
{
    return new ProcParams();
}

void ProcParams::destroy(ProcParams* pp)
{
    delete pp;
}

bool ProcParams::operator ==(const ProcParams& other) const
{
    return
        toneCurve == other.toneCurve
        && retinex == other.retinex
        && localContrast == other.localContrast
        && labCurve == other.labCurve
        && sharpenEdge == other.sharpenEdge
        && sharpenMicro == other.sharpenMicro
        && sharpening == other.sharpening
        && prsharpening == other.prsharpening
        && vibrance == other.vibrance
        && wb == other.wb
        && colorappearance == other.colorappearance
        && impulseDenoise == other.impulseDenoise
        && dirpyrDenoise == other.dirpyrDenoise
        && epd == other.epd
        && fattal == other.fattal
        && defringe == other.defringe
        && sh == other.sh
        && toneEqualizer == other.toneEqualizer
        && crop == other.crop
        && cropGuide == other.cropGuide
        && coarse == other.coarse
        && rotate == other.rotate
        && commonTrans == other.commonTrans
        && distortion == other.distortion
        && lensProf == other.lensProf
        && perspective == other.perspective
        && gradient == other.gradient
        && locallab == other.locallab
        && pcvignette == other.pcvignette
        && cacorrection == other.cacorrection
        && vignetting == other.vignetting
        && chmixer == other.chmixer
        && blackwhite == other.blackwhite
        && resize == other.resize
        && framing == other.framing
        && spot == other.spot
        && raw == other.raw
        && icm == other.icm
        && wavelet == other.wavelet
        && dirpyrequalizer == other.dirpyrequalizer
        && hsvequalizer == other.hsvequalizer
        && filmSimulation == other.filmSimulation
        && softlight == other.softlight
        && rgbCurves == other.rgbCurves
        && colorToning == other.colorToning
        && metadata == other.metadata
        && dehaze == other.dehaze
        && filmNegative == other.filmNegative;
}

int ProcParams::write(const Glib::ustring& fname, const Glib::ustring& content) const
{
    int error = 0;

    if (fname.length()) {
        FILE *f;
        f = g_fopen(fname.c_str(), "wt");

        if (f == nullptr) {
            error = 1;
        } else {
            fprintf(f, "%s", content.c_str());
            fclose(f);
        }
    }

    return error;
}

PartialProfile::PartialProfile(bool createInstance, bool paramsEditedValue)
{
    if (createInstance) {
        pparams = new ProcParams();
        pedited = new ParamsEdited(paramsEditedValue);
    } else {
        pparams = nullptr;
        pedited = nullptr;
    }
}

PartialProfile::PartialProfile(ProcParams* pp, ParamsEdited* pe, bool fullCopy)
{
    if (fullCopy && pp) {
        pparams = new ProcParams(*pp);
    } else {
        pparams = pp;
    }

    if (fullCopy && pe) {
        pedited = new ParamsEdited(*pe);
    } else {
        pedited = pe;
    }
}

PartialProfile::PartialProfile(const ProcParams* pp, const ParamsEdited* pe)
{
    if (pp) {
        pparams = new ProcParams(*pp);
    } else {
        pparams = nullptr;
    }

    if (pe) {
        pedited = new ParamsEdited(*pe);
    } else {
        pedited = nullptr;
    }
}

void PartialProfile::deleteInstance()
{
    if (pparams) {
        delete pparams;
        pparams = nullptr;
    }

    if (pedited) {
        delete pedited;
        pedited = nullptr;
    }
}

void PartialProfile::clearGeneral()
{
    if (pedited) {
        pedited->general.colorlabel = false;
        pedited->general.intrash = false;
        pedited->general.rank = false;
    }
}

int PartialProfile::load(const Glib::ustring& fName)
{
    if (!pparams) {
        pparams = new ProcParams();
    }

    if (!pedited) {
        pedited = new ParamsEdited();
    }

    if (fName == DEFPROFILE_INTERNAL) {
        return 0;
    } else if (fName == DEFPROFILE_DYNAMIC) {
        return -1; // should not happen here
    } else {
        return pparams->load(fName, pedited);
    }
}

/*
 * Set the all values of the General section to false
 * in order to preserve them in applyTo
 */
void PartialProfile::set(bool v)
{
    if (pedited) {
        pedited->set(v);
    }
}

void PartialProfile::applyTo(ProcParams* destParams, bool fromLastSave) const
{
    if (destParams && pparams && pedited) {
        bool fromHistMatching = fromLastSave && destParams->toneCurve.histmatching && pparams->toneCurve.histmatching;
        pedited->combine(*destParams, *pparams, true);
        if (!fromLastSave) {
            destParams->toneCurve.fromHistMatching = fromHistMatching;
        }
    }
}

AutoPartialProfile::AutoPartialProfile() :
    PartialProfile(true)
{
}

AutoPartialProfile::~AutoPartialProfile()
{
    deleteInstance();
}

}

}
