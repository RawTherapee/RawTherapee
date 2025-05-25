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
 *
 *  2024-2024 Daniel Gao <daniel.gao.work@gmail.com>
 */

#include "framing.h"

#include "eventmapper.h"
#include "paramsedited.h"
#include "tools/resize.h"
#include "widgets/basic/colorpreview.h"

#include "rtengine/aspectratios.h"
#include "rtengine/color.h"
#include "rtengine/procparams.h"

#include <array>
#include <iomanip>
#include <vector>

namespace
{

using namespace rtengine;
using rtengine::procparams::FramingParams;

constexpr int EMPTY_COMBO_INDEX = -1;

// Framing method combo box data
constexpr int INDEX_STANDARD = 0;
constexpr int INDEX_BBOX = 1;
constexpr int INDEX_FIXED = 2;
constexpr int INDEX_FRAMING_METHOD_UNCHANGED = 3;
constexpr std::array<const char*, 3> FRAMING_METHODS = {
    "TP_FRAMING_METHOD_STANDARD",
    "TP_FRAMING_METHOD_BBOX",
    "TP_FRAMING_METHOD_FIXED"
};

int mapFramingMethod(FramingParams::FramingMethod framingMethod)
{
    using FramingMethod = FramingParams::FramingMethod;
    switch (framingMethod) {
        case FramingMethod::STANDARD:
            return INDEX_STANDARD;
        case FramingMethod::BBOX:
            return INDEX_BBOX;
        case FramingMethod::FIXED_SIZE:
            return INDEX_FIXED;
        default:
            return INDEX_STANDARD;
    }
}

FramingParams::FramingMethod mapFramingMethod(int comboIndex)
{
    using FramingMethod = FramingParams::FramingMethod;
    switch (comboIndex) {
        case INDEX_STANDARD:
            return FramingMethod::STANDARD;
        case INDEX_BBOX:
            return FramingMethod::BBOX;
        case INDEX_FIXED:
            return FramingMethod::FIXED_SIZE;
        default:
            return FramingMethod::STANDARD;
    }
}

// Orientation combo box data
constexpr int INDEX_AS_IMAGE = 0;
constexpr int INDEX_LANDSCAPE = 1;
constexpr int INDEX_PORTRAIT = 2;
constexpr int INDEX_ORIENTATION_UNCHANGED = 3;
constexpr std::array<const char*, 3> ORIENTATION = {
    "GENERAL_ASIMAGE",
    "GENERAL_LANDSCAPE",
    "GENERAL_PORTRAIT"
};

int mapOrientation(FramingParams::Orientation orientation)
{
    using Orientation = FramingParams::Orientation;
    switch (orientation) {
        case Orientation::AS_IMAGE:
            return INDEX_AS_IMAGE;
        case Orientation::LANDSCAPE:
            return INDEX_LANDSCAPE;
        case Orientation::PORTRAIT:
            return INDEX_PORTRAIT;
        default:
            return INDEX_AS_IMAGE;
    }
}

FramingParams::Orientation mapOrientation(int comboIndex)
{
    using Orientation = FramingParams::Orientation;
    switch (comboIndex) {
        case INDEX_AS_IMAGE:
            return Orientation::AS_IMAGE;
        case INDEX_LANDSCAPE:
            return Orientation::LANDSCAPE;
        case INDEX_PORTRAIT:
            return Orientation::PORTRAIT;
        default:
            return Orientation::AS_IMAGE;
    }
}

// Border sizing method combo box data
constexpr int INDEX_SIZE_RELATIVE = 0;
constexpr int INDEX_SIZE_UNIFORM_RELATIVE = 1;
constexpr int INDEX_SIZE_ABSOLUTE = 2;
constexpr int INDEX_SIZE_UNCHANGED = 3;
constexpr std::array<const char*, 3> BORDER_SIZE_METHODS = {
    "TP_FRAMING_BORDER_SIZE_RELATIVE",
    "TP_FRAMING_BORDER_SIZE_UNIFORM_RELATIVE",
    "TP_FRAMING_BORDER_SIZE_ABSOLUTE"
};

int mapBorderSizeMethod(FramingParams::BorderSizing sizing)
{
    using BorderSizing = FramingParams::BorderSizing;
    switch (sizing) {
        case BorderSizing::PERCENTAGE:
            return INDEX_SIZE_RELATIVE;
        case BorderSizing::UNIFORM_PERCENTAGE:
            return INDEX_SIZE_UNIFORM_RELATIVE;
        case BorderSizing::FIXED_SIZE:
            return INDEX_SIZE_ABSOLUTE;
        default:
            return INDEX_SIZE_RELATIVE;
    }
}

FramingParams::BorderSizing mapBorderSizeMethod(int comboIndex)
{
    using BorderSizing = FramingParams::BorderSizing;
    switch (comboIndex) {
        case INDEX_SIZE_RELATIVE:
            return BorderSizing::PERCENTAGE;
        case INDEX_SIZE_UNIFORM_RELATIVE:
            return BorderSizing::UNIFORM_PERCENTAGE;
        case INDEX_SIZE_ABSOLUTE:
            return BorderSizing::FIXED_SIZE;
        default:
            return BorderSizing::PERCENTAGE;
    }
}

// Relative sizing basis combo box data
constexpr int INDEX_BASIS_AUTO = 0;
constexpr int INDEX_BASIS_WIDTH = 1;
constexpr int INDEX_BASIS_HEIGHT = 2;
constexpr int INDEX_BASIS_LONG = 3;
constexpr int INDEX_BASIS_SHORT = 4;
constexpr int INDEX_BASIS_UNCHANGED = 5;
constexpr std::array<const char*, 5> BORDER_SIZE_BASIS = {
    "TP_FRAMING_BASIS_AUTO",
    "TP_FRAMING_BASIS_WIDTH",
    "TP_FRAMING_BASIS_HEIGHT",
    "TP_FRAMING_BASIS_LONG_SIDE",
    "TP_FRAMING_BASIS_SHORT_SIDE"
};

int mapBasis(FramingParams::Basis basis)
{
    using Basis = FramingParams::Basis;
    switch(basis) {
        case Basis::AUTO:
            return INDEX_BASIS_AUTO;
        case Basis::WIDTH:
            return INDEX_BASIS_WIDTH;
        case Basis::HEIGHT:
            return INDEX_BASIS_HEIGHT;
        case Basis::LONG:
            return INDEX_BASIS_LONG;
        case Basis::SHORT:
            return INDEX_BASIS_SHORT;
        default:
            return INDEX_BASIS_AUTO;
    }
}

FramingParams::Basis mapBasis(int comboIndex)
{
    using Basis = FramingParams::Basis;
    switch(comboIndex) {
        case INDEX_BASIS_AUTO:
            return Basis::AUTO;
        case INDEX_BASIS_WIDTH:
            return Basis::WIDTH;
        case INDEX_BASIS_HEIGHT:
            return Basis::HEIGHT;
        case INDEX_BASIS_LONG:
            return Basis::LONG;
        case INDEX_BASIS_SHORT:
            return Basis::SHORT;
        default:
            return Basis::AUTO;
    }
}

constexpr int INITIAL_IMG_WIDTH = 100000;
constexpr int INITIAL_IMG_HEIGHT = 100000;
constexpr int MAX_COLOR_VAL = 255;

constexpr int ROW_SPACING = 4;
constexpr float FRAME_LABEL_ALIGN_X = 0.025;
constexpr float FRAME_LABEL_ALIGN_Y = 0.5;

Gtk::Label* createGridLabel(const char* text)
{
    Gtk::Label* label = Gtk::manage(new Gtk::Label(M(text)));
    label->set_halign(Gtk::ALIGN_START);
    return label;
}

MySpinButton* createSpinButton()
{
    MySpinButton* button = Gtk::manage(new MySpinButton());
    button->set_width_chars(5);
    button->set_digits(0);
    button->set_increments(1, 100);
    setExpandAlignProperties(button, false, false, Gtk::ALIGN_END, Gtk::ALIGN_CENTER);
    return button;
}

}  // namespace

const Glib::ustring Framing::TOOL_NAME = "framing";

class Framing::AspectRatios
{
public:
    static constexpr int INDEX_CURRENT = 0;

    AspectRatios() :
        ratios{{M("GENERAL_ASIMAGE")}}
    {
        fillAspectRatios(ratios);
    }

    void fillCombo(MyComboBoxText* combo) const
    {
        for (const auto& aspectRatio : ratios) {
            combo->append(aspectRatio.label);
        }
        combo->set_active(INDEX_CURRENT);
    }

    int unchangedIndex() const { return ratios.size(); }

    double value(int index) const
    {
        return ratios.at(index).value;
    }

    int findIndex(double aspectRatio) const
    {
        if (aspectRatio == FramingParams::AS_IMAGE_ASPECT_RATIO) return INDEX_CURRENT;

        for (size_t i = 1; i < ratios.size(); i++) {
            if (ratios[i].value == aspectRatio) return i;
        }

        // Couldn't find a matching value
        return INDEX_CURRENT;
    }

private:
    std::vector<AspectRatio> ratios;
};

Framing::DimensionGui::DimensionGui(Gtk::Box* parent, const char* text)
{
    box = Gtk::manage(new Gtk::Box());
    Gtk::Label* label = Gtk::manage(new Gtk::Label(M(text)));
    setExpandAlignProperties(label, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    value = createSpinButton();
    box->pack_start(*label);
    box->pack_start(*value);
    parent->pack_start(*box);
}

void Framing::DimensionGui::connect(Framing& framing, CallbackFunc callback)
{
    connection = value->signal_value_changed().connect(sigc::mem_fun(framing, callback), true);
}

Framing::Framing() :
    FoldableToolPanel(this, TOOL_NAME, M("TP_FRAMING_LABEL"), false, true),
    aspectRatioData(new AspectRatios),
    imgWidth(INITIAL_IMG_WIDTH),
    imgHeight(INITIAL_IMG_HEIGHT),
    lastAllowUpscaling(false),
    lastMinSizeEnabled(false)
{
    setupEvents();
    setupFramingMethodGui();
    pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)));
    setupBorderSizeGui();
    pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)));
    setupBorderColorsGui();
}

Framing::~Framing() {
    idleRegister.destroy();
}

void Framing::setupEvents()
{
    auto m = ProcEventMapper::getInstance();

    // clang-format off
    EvFramingEnabled            = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ENABLED");
    EvFramingMethod             = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_METHOD");
    EvFramingAspectRatio        = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ASPECT_RATIO");
    EvFramingOrientation        = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ORIENTATION");
    EvFramingFramedWidth        = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_FRAMED_WIDTH");
    EvFramingFramedHeight       = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_FRAMED_HEIGHT");
    EvFramingAllowUpscaling     = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ALLOW_UPSCALING");
    EvFramingBorderSizingMethod = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_BORDER_SIZE_METHOD");
    EvFramingBasis              = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_BASIS");
    EvFramingRelativeBorderSize = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_BORDER_SIZE");
    EvFramingMinSizeEnabled     = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_MIN_SIZE_ENABLED");
    EvFramingMinWidth           = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_MIN_WIDTH");
    EvFramingMinHeight          = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_MIN_HEIGHT");
    EvFramingAbsWidth           = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ABSOLUTE_WIDTH");
    EvFramingAbsHeight          = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ABSOLUTE_HEIGHT");
    EvFramingBorderRed          = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_BORDER_RED");
    EvFramingBorderGreen        = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_BORDER_GREEN");
    EvFramingBorderBlue         = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_BORDER_BLUE");
    EvFramingAnnotation         = m->newEvent(RESIZE, "HISTORY_MSG_FRAMING_ANNOTATION");
    // clang-format on
}

void Framing::setupFramingMethodGui()
{
    Gtk::Grid* combos = Gtk::manage(new Gtk::Grid());
    combos->set_row_spacing(ROW_SPACING);

    framingMethod = Gtk::manage(new MyComboBoxText());
    for (auto label : FRAMING_METHODS) {
        framingMethod->append(M(label));
    }
    framingMethod->set_active(INDEX_STANDARD);
    framingMethod->set_hexpand();
    framingMethod->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*createGridLabel("TP_FRAMING_METHOD"), 0, 0);
    combos->attach(*framingMethod, 1, 0);

    aspectRatioLabel = createGridLabel("TP_FRAMING_ASPECT_RATIO");
    aspectRatio = Gtk::manage(new MyComboBoxText());
    aspectRatioData->fillCombo(aspectRatio);
    aspectRatio->set_hexpand();
    aspectRatio->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*aspectRatioLabel, 0, 1);
    combos->attach(*aspectRatio, 1, 1);

    orientationLabel = createGridLabel("TP_FRAMING_ORIENTATION");
    orientation = Gtk::manage(new MyComboBoxText());
    for (auto label : ORIENTATION) {
        orientation->append(M(label));
    }
    orientation->set_active(INDEX_AS_IMAGE);
    orientation->set_hexpand();
    orientation->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*orientationLabel, 0, 2);
    combos->attach(*orientation, 1, 2);
    pack_start(*combos);

    width = DimensionGui(this, "TP_FRAMING_FRAMED_WIDTH");
    width.setRange(Resize::MIN_SIZE, Resize::MAX_SCALE * imgWidth);
    width.setValue(imgWidth);
    height = DimensionGui(this, "TP_FRAMING_FRAMED_HEIGHT");
    height.setRange(Resize::MIN_SIZE, Resize::MAX_SCALE * imgHeight);
    height.setValue(imgHeight);

    allowUpscaling = Gtk::manage(new Gtk::CheckButton(M("TP_FRAMING_ALLOW_UPSCALING")));
    pack_start(*allowUpscaling);

    updateFramingMethodGui();

    framingMethodChanged = framingMethod->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onFramingMethodChanged));
    aspectRatioChanged = aspectRatio->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onAspectRatioChanged));
    orientationChanged = orientation->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onOrientationChanged));
    width.connect(*this, &Framing::onWidthChanged);
    height.connect(*this, &Framing::onHeightChanged);
    allowUpscalingConnection = allowUpscaling->signal_toggled().connect(
        sigc::mem_fun(*this, &Framing::onAllowUpscalingToggled));
}

void Framing::setupBorderSizeGui()
{
    Gtk::Grid* combos = Gtk::manage(new Gtk::Grid());
    combos->set_row_spacing(ROW_SPACING);

    borderSizeMethod = Gtk::manage(new MyComboBoxText());
    for (auto label : BORDER_SIZE_METHODS) {
        borderSizeMethod->append(M(label));
    }
    borderSizeMethod->set_active(INDEX_SIZE_RELATIVE);
    borderSizeMethod->set_hexpand();
    borderSizeMethod->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*createGridLabel("TP_FRAMING_BORDER_SIZE_METHOD"), 0, 0);
    combos->attach(*borderSizeMethod, 1, 0);

    basisLabel = createGridLabel("TP_FRAMING_BASIS");
    basis = Gtk::manage(new MyComboBoxText());
    for (auto label : BORDER_SIZE_BASIS) {
        basis->append(M(label));
    }
    basis->set_active(INDEX_BASIS_AUTO);
    basis->set_hexpand();
    basis->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*basisLabel, 0, 1);
    combos->attach(*basis, 1, 1);

    pack_start(*combos);

    relativeBorderSize = Gtk::manage(new Adjuster(M("TP_FRAMING_BORDER_SIZE"), 0, 1, 0.01, 0.1));
    pack_start(*relativeBorderSize);

    minSizeFrame = Gtk::manage(new Gtk::Frame());
    minSizeFrame->set_label_align(FRAME_LABEL_ALIGN_X, FRAME_LABEL_ALIGN_Y);
    minSizeEnabled = Gtk::manage(new Gtk::CheckButton(M("TP_FRAMING_LIMIT_MINIMUM")));
    minSizeFrame->set_label_widget(*minSizeEnabled);

    minSizeFrameContent = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    minWidth = DimensionGui(minSizeFrameContent, "TP_FRAMING_MIN_WIDTH");
    minWidth.setRange(0, imgWidth);
    minWidth.setValue(0);
    minHeight = DimensionGui(minSizeFrameContent, "TP_FRAMING_MIN_HEIGHT");
    minHeight.setRange(0, imgHeight);
    minHeight.setValue(0);

    minSizeFrame->add(*minSizeFrameContent);
    pack_start(*minSizeFrame);

    absWidth = DimensionGui(this, "TP_FRAMING_ABSOLUTE_WIDTH");
    absWidth.setRange(0, imgWidth);
    absWidth.setValue(0);
    absHeight = DimensionGui(this, "TP_FRAMING_ABSOLUTE_HEIGHT");
    absHeight.setRange(0, imgHeight);
    absHeight.setValue(0);

    updateBorderSizeGui();

    borderSizeMethodChanged = borderSizeMethod->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onBorderSizeMethodChanged));
    basisChanged = basis->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onBasisChanged));
    relativeBorderSize->setAdjusterListener(this);
    minSizeEnabledConnection = minSizeEnabled->signal_toggled().connect(
        sigc::mem_fun(*this, &Framing::onMinSizeToggled));
    minWidth.connect(*this, &Framing::onMinWidthChanged);
    minHeight.connect(*this, &Framing::onMinHeightChanged);
    absWidth.connect(*this, &Framing::onAbsWidthChanged);
    absHeight.connect(*this, &Framing::onAbsHeightChanged);
}

void Framing::setupBorderColorsGui()
{
    Gtk::Frame* const frame = Gtk::manage(new Gtk::Frame());

    Gtk::Label* const label = Gtk::manage(new Gtk::Label(M("TP_FRAMING_BORDER_COLOR")));
    frame->set_label_align(FRAME_LABEL_ALIGN_X, FRAME_LABEL_ALIGN_Y);
    frame->set_label_widget(*label);

    Gtk::Box* const box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    redAdj = Gtk::manage(new Adjuster(M("TP_FRAMING_RED"), 0, MAX_COLOR_VAL, 1, MAX_COLOR_VAL));
    box->add(*redAdj);
    greenAdj = Gtk::manage(new Adjuster(M("TP_FRAMING_GREEN"), 0, MAX_COLOR_VAL, 1, MAX_COLOR_VAL));
    box->add(*greenAdj);
    blueAdj = Gtk::manage(new Adjuster(M("TP_FRAMING_BLUE"), 0, MAX_COLOR_VAL, 1, MAX_COLOR_VAL));
    box->add(*blueAdj);

    Gtk::Frame* const colorFrame = Gtk::manage(new Gtk::Frame());
    colorPreview = Gtk::manage(new ColorPreview());
    colorFrame->add(*colorPreview);
    box->add(*colorFrame);

    Gtk::Label* const annotationLabel = Gtk::manage(new Gtk::Label(M("TP_FRAMING_ANNOTATION")));
    box->add(*annotationLabel);

    borderAnnotation = Gtk::manage(new Gtk::Entry());
    box->add(*borderAnnotation);

    frame->add(*box);

    pack_start(*frame);

    updateBorderColorGui();

    redAdj->setAdjusterListener(this);
    greenAdj->setAdjusterListener(this);
    blueAdj->setAdjusterListener(this);

    annotationChanged = borderAnnotation->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onAnnotationChanged));
}

void Framing::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    DisableListener disableListener(this);

    std::array<ConnectionBlocker, 14> blockers = {
        ConnectionBlocker(framingMethodChanged),
        ConnectionBlocker(aspectRatioChanged),
        ConnectionBlocker(orientationChanged),
        ConnectionBlocker(width.connection),
        ConnectionBlocker(height.connection),
        ConnectionBlocker(allowUpscalingConnection),
        ConnectionBlocker(borderSizeMethodChanged),
        ConnectionBlocker(basisChanged),
        ConnectionBlocker(minSizeEnabledConnection),
        ConnectionBlocker(minWidth.connection),
        ConnectionBlocker(minHeight.connection),
        ConnectionBlocker(absWidth.connection),
        ConnectionBlocker(absHeight.connection),
        ConnectionBlocker(annotationChanged),
    };

    BlockAdjusterEvents blockRelative(relativeBorderSize);
    BlockAdjusterEvents blockRed(redAdj);
    BlockAdjusterEvents blockGreen(greenAdj);
    BlockAdjusterEvents blockBlue(blueAdj);

    readParams(pp);
    readEdited(pedited);

    updateFramingMethodGui();
    updateBorderSizeGui();
    updateBorderColorGui();
    setDimensions();
}

void Framing::readParams(const rtengine::procparams::ProcParams* pp)
{
    const rtengine::procparams::FramingParams& params = pp->framing;

    setEnabled(params.enabled);

    framingMethod->set_active(mapFramingMethod(params.framingMethod));
    aspectRatio->set_active(aspectRatioData->findIndex(params.aspectRatio));
    orientation->set_active(mapOrientation(params.orientation));
    width.setValue(params.framedWidth);
    width.isDirty = false;
    height.setValue(params.framedHeight);
    height.isDirty = false;
    allowUpscaling->set_active(params.allowUpscaling);
    lastAllowUpscaling = params.allowUpscaling;

    borderSizeMethod->set_active(mapBorderSizeMethod(params.borderSizingMethod));
    basis->set_active(mapBasis(params.basis));
    relativeBorderSize->setValue(params.relativeBorderSize);
    minSizeEnabled->set_active(params.minSizeEnabled);
    lastMinSizeEnabled = params.minSizeEnabled;
    minWidth.setValue(params.minWidth);
    minWidth.isDirty = false;
    minHeight.setValue(params.minHeight);
    minHeight.isDirty = false;
    absWidth.setValue(params.absWidth);
    absWidth.isDirty = false;
    absHeight.setValue(params.absHeight);
    absHeight.isDirty = false;

    redAdj->setValue(params.borderRed);
    greenAdj->setValue(params.borderGreen);
    blueAdj->setValue(params.borderBlue);

    borderAnnotation->set_text(params.borderAnnotation);
}

void Framing::readEdited(const ParamsEdited* pedited)
{
    if (!pedited) return;

    const FramingParamsEdited& edits = pedited->framing;

    set_inconsistent(multiImage && !edits.enabled);

    if (!edits.framingMethod) {
        framingMethod->set_active(EMPTY_COMBO_INDEX);
    }
    if (!edits.aspectRatio) {
        aspectRatio->set_active(EMPTY_COMBO_INDEX);
    }
    if (!edits.orientation) {
        orientation->set_active(EMPTY_COMBO_INDEX);
    }
    width.isDirty = edits.framedWidth;
    height.isDirty = edits.framedHeight;
    allowUpscaling->set_inconsistent(edits.allowUpscaling);

    if (!edits.borderSizingMethod) {
        borderSizeMethod->set_active(EMPTY_COMBO_INDEX);
    }
    if (!edits.basis) {
        basis->set_active(EMPTY_COMBO_INDEX);
    }
    relativeBorderSize->setEditedState(edits.relativeBorderSize ? Edited : UnEdited);
    minSizeEnabled->set_inconsistent(edits.minSizeEnabled);
    minWidth.isDirty = edits.minWidth;
    minHeight.isDirty = edits.minHeight;
    absWidth.isDirty = edits.absWidth;
    absHeight.isDirty = edits.absHeight;

    redAdj->setEditedState(edits.borderRed ? Edited : UnEdited);
    greenAdj->setEditedState(edits.borderGreen ? Edited : UnEdited);
    blueAdj->setEditedState(edits.borderBlue ? Edited : UnEdited);
}

void Framing::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    writeParams(pp);
    writeEdited(pedited);
}

void Framing::writeParams(rtengine::procparams::ProcParams* pp)
{
    rtengine::procparams::FramingParams& params = pp->framing;

    params.enabled = getEnabled();

    params.framingMethod = mapFramingMethod(framingMethod->get_active_row_number());
    params.aspectRatio = aspectRatioData->value(aspectRatio->get_active_row_number());
    params.orientation = mapOrientation(orientation->get_active_row_number());
    params.framedWidth = width.value->get_value_as_int();
    params.framedHeight = height.value->get_value_as_int();
    params.allowUpscaling = allowUpscaling->get_active();

    params.borderSizingMethod = mapBorderSizeMethod(borderSizeMethod->get_active_row_number());
    params.basis = mapBasis(basis->get_active_row_number());
    params.relativeBorderSize = relativeBorderSize->getValue();
    params.minSizeEnabled = minSizeEnabled->get_active();
    params.minWidth = minWidth.value->get_value_as_int();
    params.minHeight = minHeight.value->get_value_as_int();
    params.absWidth = absWidth.value->get_value_as_int();
    params.absHeight = absHeight.value->get_value_as_int();

    params.borderRed = redAdj->getValue();
    params.borderGreen = greenAdj->getValue();
    params.borderBlue = blueAdj->getValue();

    params.borderAnnotation = borderAnnotation->get_buffer()->get_text();
}

void Framing::writeEdited(ParamsEdited* pedited)
{
    if (!pedited) return;

    FramingParamsEdited& edits = pedited->framing;

    edits.enabled = !get_inconsistent();

    edits.framingMethod = framingMethod->get_active_row_number() != INDEX_FRAMING_METHOD_UNCHANGED;
    edits.aspectRatio = aspectRatio->get_active_row_number() != aspectRatioData->unchangedIndex();
    edits.orientation = orientation->get_active_row_number() != INDEX_ORIENTATION_UNCHANGED;
    edits.framedWidth = width.isDirty;
    edits.framedHeight = height.isDirty;
    edits.allowUpscaling = !allowUpscaling->get_inconsistent();

    edits.borderSizingMethod = borderSizeMethod->get_active_row_number() != INDEX_SIZE_UNCHANGED;
    edits.basis = basis->get_active_row_number() != INDEX_BASIS_UNCHANGED;
    edits.relativeBorderSize = relativeBorderSize->getEditedState();
    edits.minSizeEnabled = !minSizeEnabled->get_inconsistent();
    edits.minWidth = minWidth.isDirty;
    edits.minHeight = minHeight.isDirty;
    edits.absWidth = absWidth.isDirty;
    edits.absHeight = absHeight.isDirty;

    edits.borderRed = redAdj->getEditedState();
    edits.borderGreen = greenAdj->getEditedState();
    edits.borderBlue = blueAdj->getEditedState();
}

void Framing::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const FramingParams& params = defParams->framing;

    relativeBorderSize->setDefault(params.relativeBorderSize);
    redAdj->setDefault(params.borderRed);
    greenAdj->setDefault(params.borderGreen);
    blueAdj->setDefault(params.borderBlue);
    borderAnnotation->set_text(params.borderAnnotation);

    if (pedited) {
        const FramingParamsEdited& edits = pedited->framing;

        relativeBorderSize->setDefaultEditedState(edits.relativeBorderSize ? Edited : UnEdited);
        redAdj->setDefaultEditedState(edits.borderRed ? Edited : UnEdited);
        greenAdj->setDefaultEditedState(edits.borderGreen ? Edited : UnEdited);
        blueAdj->setDefaultEditedState(edits.borderBlue ? Edited : UnEdited);
    } else {
        relativeBorderSize->setDefaultEditedState(Irrelevant);
        redAdj->setDefaultEditedState(Irrelevant);
        greenAdj->setDefaultEditedState(Irrelevant);
        blueAdj->setDefaultEditedState(Irrelevant);
    }
}

void Framing::trimValues(rtengine::procparams::ProcParams* pp)
{
    relativeBorderSize->trimValue(pp->framing.relativeBorderSize);
    redAdj->trimValue(pp->framing.borderRed);
    greenAdj->trimValue(pp->framing.borderGreen);
    blueAdj->trimValue(pp->framing.borderBlue);
}

void Framing::setBatchMode(bool batchMode)
{
    framingMethod->append(M("GENERAL_UNCHANGED"));
    aspectRatio->append(M("GENERAL_UNCHANGED"));
    orientation->append(M("GENERAL_UNCHANGED"));
    borderSizeMethod->append(M("GENERAL_UNCHANGED"));
    basis->append(M("GENERAL_UNCHANGED"));

    ToolPanel::setBatchMode(batchMode);
    relativeBorderSize->showEditedCB();
    redAdj->showEditedCB();
    greenAdj->showEditedCB();
    blueAdj->showEditedCB();
}

void Framing::enabledChanged()
{
    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged(EvFramingEnabled, M("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged(EvFramingEnabled, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvFramingEnabled, M("GENERAL_DISABLED"));
        }
    }
}

void Framing::update(int originalWidth, int originalHeight)
{
    // This is how it is checked in resize.cc
    if (originalWidth && originalHeight) {
        imgWidth = originalWidth;
        imgHeight = originalHeight;
    }
}

void Framing::setAdjusterBehavior(bool addRelativeBorderSize, bool addRed, bool addGreen,
                                  bool addBlue)
{
    relativeBorderSize->setAddMode(addRelativeBorderSize);
    redAdj->setAddMode(addRed);
    greenAdj->setAddMode(addGreen);
    blueAdj->setAddMode(addBlue);
}

void Framing::setDimensions()
{
    idleRegister.add([this]() -> bool {
        std::array<ConnectionBlocker, 6> blockers = {
            ConnectionBlocker(width.connection),
            ConnectionBlocker(height.connection),
            ConnectionBlocker(minWidth.connection),
            ConnectionBlocker(minHeight.connection),
            ConnectionBlocker(absWidth.connection),
            ConnectionBlocker(absHeight.connection)
        };

        // 16x the full image size is probably a reasonable max
        width.value->set_range(Resize::MIN_SIZE, Resize::MAX_SCALE * imgWidth);
        height.value->set_range(Resize::MIN_SIZE, Resize::MAX_SCALE * imgHeight);
        minWidth.value->set_range(0, Resize::MAX_SCALE * imgWidth);
        minHeight.value->set_range(0, Resize::MAX_SCALE * imgHeight);
        absWidth.value->set_range(0, Resize::MAX_SCALE * imgWidth);
        absHeight.value->set_range(0, Resize::MAX_SCALE * imgHeight);

        return false;
    });
}

void Framing::updateFramingMethodGui()
{
    if (batchMode) {
        aspectRatioLabel->show();
        aspectRatio->show();
        orientationLabel->show();
        orientation->show();
        width.show();
        height.show();
        allowUpscaling->show();
        return;
    }

    int activeRow = framingMethod->get_active_row_number();
    if (activeRow == INDEX_STANDARD) {
        aspectRatioLabel->show();
        aspectRatio->show();
        orientationLabel->show();
        orientation->show();
        width.hide();
        height.hide();
        allowUpscaling->hide();
    } else if (activeRow == INDEX_BBOX) {
        aspectRatioLabel->show();
        aspectRatio->show();
        orientationLabel->show();
        orientation->show();
        width.show();
        height.show();
        allowUpscaling->show();
    } else if (activeRow == INDEX_FIXED) {
        aspectRatioLabel->hide();
        aspectRatio->hide();
        orientationLabel->hide();
        orientation->hide();
        width.show();
        height.show();
        allowUpscaling->show();
    }
}

void Framing::updateBorderSizeGui()
{
    if (batchMode) {
        basisLabel->show();
        basis->show();
        relativeBorderSize->show();
        minSizeFrame->show();
        absWidth.show();
        absHeight.show();

        aspectRatio->set_sensitive(true);
        orientation->set_sensitive(true);

        minSizeFrameContent->set_sensitive(true);
        return;
    }

    int activeRow = borderSizeMethod->get_active_row_number();
    if (activeRow == INDEX_SIZE_RELATIVE) {
        basisLabel->show();
        basis->show();
        relativeBorderSize->show();
        minSizeFrame->show();
        absWidth.hide();
        absHeight.hide();

        aspectRatio->set_sensitive(true);
        orientation->set_sensitive(true);
    } else if (activeRow == INDEX_SIZE_UNIFORM_RELATIVE) {
        basisLabel->show();
        basis->show();
        relativeBorderSize->show();
        minSizeFrame->show();
        absWidth.hide();
        absHeight.hide();

        aspectRatio->set_sensitive(false);
        orientation->set_sensitive(false);
    } else if (activeRow == INDEX_SIZE_ABSOLUTE) {
        basisLabel->hide();
        basis->hide();
        relativeBorderSize->hide();
        minSizeFrame->hide();
        absWidth.show();
        absHeight.show();

        aspectRatio->set_sensitive(false);
        orientation->set_sensitive(false);
    }

    minSizeFrameContent->set_sensitive(minSizeEnabled->get_active());
}

void Framing::updateBorderColorGui()
{
    auto gamma = [](double val) {
        // adjuster is [0.0, 255.0]
        // gamma2curve expects [0, 65535]
        // setRgb expects [0.0, 1.0]
        return Color::gamma2curve[val * (MAX_COLOR_VAL + 1)] / 65535.0;
    };
    double r = gamma(redAdj->getValue());
    double g = gamma(greenAdj->getValue());
    double b = gamma(blueAdj->getValue());
    colorPreview->setRgb(r, g, b);
}

void Framing::adjusterChanged(Adjuster* adj, double newVal)
{
    if (adj == redAdj || adj == greenAdj || adj == blueAdj) {
        updateBorderColorGui();
    }

    if (listener && (getEnabled() || batchMode)) {
        Glib::ustring costr;
        if (adj == relativeBorderSize) {
            costr = Glib::ustring::format(std::setw(3), std::fixed, std::setprecision(2),
                                          adj->getValue());
        } else {
            costr = Glib::ustring::format(static_cast<int>(adj->getValue()));
        }

        if (adj == relativeBorderSize) {
            listener->panelChanged(EvFramingRelativeBorderSize, costr);
        } else if (adj == redAdj) {
            listener->panelChanged(EvFramingBorderRed, costr);
        } else if (adj == greenAdj) {
            listener->panelChanged(EvFramingBorderGreen, costr);
        } else if (adj == blueAdj) {
            listener->panelChanged(EvFramingBorderBlue, costr);
        }
    }
}

void Framing::onFramingMethodChanged()
{
    updateFramingMethodGui();

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingMethod, framingMethod->get_active_text());
    }
}

void Framing::onAspectRatioChanged()
{
    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingAspectRatio, aspectRatio->get_active_text());
    }
}

void Framing::onOrientationChanged()
{
    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingOrientation, orientation->get_active_text());
    }
}

void Framing::onWidthChanged()
{
    width.isDirty = true;

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingFramedWidth,
                               Glib::ustring::format(width.value->get_value_as_int()));
    }
}

void Framing::onHeightChanged()
{
    height.isDirty = true;

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingFramedHeight,
                               Glib::ustring::format(height.value->get_value_as_int()));
    }
}

void Framing::onAllowUpscalingToggled()
{
    if (batchMode) {
        if (allowUpscaling->get_inconsistent()) {
            allowUpscaling->set_inconsistent(false);
            ConnectionBlocker block(allowUpscalingConnection);
            allowUpscaling->set_active(false);
        } else if (lastAllowUpscaling) {
            allowUpscaling->set_inconsistent(true);
        }

        lastAllowUpscaling = allowUpscaling->get_active();
    }

    if (listener && (getEnabled() || batchMode)) {
        if (allowUpscaling->get_inconsistent()) {
            listener->panelChanged(EvFramingAllowUpscaling, M("GENERAL_UNCHANGED"));
        } else if (allowUpscaling->get_active()) {
            listener->panelChanged(EvFramingAllowUpscaling, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvFramingAllowUpscaling, M("GENERAL_DISABLED"));
        }
    }
}

void Framing::onBorderSizeMethodChanged()
{
    if (borderSizeMethod->get_active_row_number() == INDEX_SIZE_UNIFORM_RELATIVE) {
        ConnectionBlocker block(minHeight.connection);
        minHeight.isDirty = true;
        minHeight.value->set_value(minWidth.value->get_value_as_int());
    }

    updateBorderSizeGui();

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingBorderSizingMethod, borderSizeMethod->get_active_text());
    }
}

void Framing::onBasisChanged()
{
    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingBasis, basis->get_active_text());
    }
}

void Framing::onMinSizeToggled()
{
    if (batchMode) {
        if (minSizeEnabled->get_inconsistent()) {
            minSizeEnabled->set_inconsistent(false);
            ConnectionBlocker block(minSizeEnabledConnection);
            minSizeEnabled->set_active(false);
        } else if (lastMinSizeEnabled) {
            minSizeEnabled->set_inconsistent(true);
        }

        lastMinSizeEnabled = minSizeEnabled->get_active();
    }

    updateBorderSizeGui();

    if (listener && (getEnabled() || batchMode)) {
        if (minSizeEnabled->get_inconsistent()) {
            listener->panelChanged(EvFramingMinSizeEnabled, M("GENERAL_UNCHANGED"));
        } else if (minSizeEnabled->get_active()) {
            listener->panelChanged(EvFramingMinSizeEnabled, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvFramingMinSizeEnabled, M("GENERAL_DISABLED"));
        }
    }
}

void Framing::onMinWidthChanged()
{
    minWidth.isDirty = true;
    int value = minWidth.value->get_value_as_int();

    if (borderSizeMethod->get_active_row_number() == INDEX_SIZE_UNIFORM_RELATIVE) {
        ConnectionBlocker block(minHeight.connection);
        minHeight.isDirty = true;
        minHeight.value->set_value(value);
    }

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingMinWidth, Glib::ustring::format(value));
    }
}

void Framing::onMinHeightChanged()
{
    minHeight.isDirty = true;
    int value = minHeight.value->get_value_as_int();

    if (borderSizeMethod->get_active_row_number() == INDEX_SIZE_UNIFORM_RELATIVE) {
        ConnectionBlocker block(minWidth.connection);
        minWidth.isDirty = true;
        minWidth.value->set_value(value);
    }

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingMinHeight, Glib::ustring::format(value));
    }
}

void Framing::onAbsWidthChanged()
{
    absWidth.isDirty = true;

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingAbsWidth,
                               Glib::ustring::format(absWidth.value->get_value_as_int()));
    }
}

void Framing::onAbsHeightChanged()
{
    absHeight.isDirty = true;

    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingAbsHeight,
                               Glib::ustring::format(absHeight.value->get_value_as_int()));
    }
}

void Framing::onAnnotationChanged()
{
    if (listener && (getEnabled() || batchMode)) {
        listener->panelChanged(EvFramingAnnotation,
                               borderAnnotation->get_buffer()->get_text());
    }
}

