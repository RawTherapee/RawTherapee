/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Alberto Romei <aldrop8@gmail.com>
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
#include <iomanip>

#include "filmnegative.h"

#include "editwidgets.h"
#include "eventmapper.h"
#include "options.h"
#include <stdexcept>
#include "rtimage.h"

#include "rtengine/procparams.h"
#include "rtengine/color.h"

const Glib::ustring FilmNegative::TOOL_NAME = "filmnegative";

namespace
{

/**
 * Observer to use for displaying the temperature and tint equivalent of the
 * multipliers.
 */
constexpr rtengine::StandardObserver standard_observer = rtengine::ColorTemp::DEFAULT_OBSERVER;

double toAdjuster(double v)
{
    return CLAMP(std::log2(v), 6, 16) - 6;
}

double fromAdjuster(double v)
{
    return std::pow(2, v + 6);
}

Adjuster* createExponentAdjuster(AdjusterListener* listener, const Glib::ustring& label, double minV, double maxV, double step, double defaultVal)
{
    Adjuster* const adj = Gtk::manage(new Adjuster(label, minV, maxV, step, defaultVal));
    adj->setAdjusterListener(listener);
    adj->setLogScale(6, 1, true);

    const auto& options = App::get().options();
    adj->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    adj->show();
    return adj;
}

Adjuster* createLevelAdjuster(AdjusterListener* listener, const Glib::ustring& label)
{
//    Adjuster* const adj = Gtk::manage(new Adjuster(label, 1.0, 65535.0, 1.0, rtengine::MAXVALF / 24.));
    Adjuster* const adj = Gtk::manage(new Adjuster(label, 0.0, 10.0, 0.01, toAdjuster(rtengine::MAXVALF / 24.)));
    adj->setAdjusterListener(listener);
//    adj->setLogScale(6, 1000.0, true);

    const auto& options = App::get().options();
    adj->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    adj->show();
    return adj;
}

Adjuster* createBalanceAdjuster(AdjusterListener* listener, const Glib::ustring& label, double minV, double maxV, double defaultVal,
                                const Glib::ustring& leftIcon, const Glib::ustring& rightIcon)
{
    Adjuster* const adj = Gtk::manage(new Adjuster(label, minV, maxV, 0.01, defaultVal,
                                      Gtk::manage(new RTImage(leftIcon)), Gtk::manage(new RTImage(rightIcon))));
    adj->setAdjusterListener(listener);
    adj->setLogScale(9, 0, true);

    const auto& options = App::get().options();
    adj->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    adj->show();
    return adj;
}


Glib::ustring fmt(const RGB& rgb)
{
    if (rgb.r <= 0.f && rgb.g <= 0.f && rgb.b <= 0.f) {
        return "- - -";
    } else {
        return Glib::ustring::format(std::fixed, std::setprecision(1), rgb.r) + " " +
               Glib::ustring::format(std::fixed, std::setprecision(1), rgb.g) + " " +
               Glib::ustring::format(std::fixed, std::setprecision(1), rgb.b);
    }
}


RGB getFilmNegativeExponents(const RGB &ref1, const RGB &ref2) // , const RGB &clearValsOut, const RGB &denseValsOut)
{
    using rtengine::settings;

    RGB clearVals = ref1;
    RGB denseVals = ref2;

    // Detect which one is the dense spot, based on green channel
    if (clearVals.g < denseVals.g) {
        std::swap(clearVals, denseVals);
        //std::swap(clearValsOut, denseValsOut);
    }

    if (settings->verbose) {
        printf("Clear input values: R=%g G=%g B=%g\n", static_cast<double>(clearVals.r), static_cast<double>(clearVals.g), static_cast<double>(clearVals.b));
        printf("Dense input values: R=%g G=%g B=%g\n", static_cast<double>(denseVals.r), static_cast<double>(denseVals.g), static_cast<double>(denseVals.b));

        // printf("Clear output values: R=%g G=%g B=%g\n", static_cast<double>(clearValsOut.r), static_cast<double>(clearValsOut.g), static_cast<double>(clearValsOut.b));
        // printf("Dense output values: R=%g G=%g B=%g\n", static_cast<double>(denseValsOut.r), static_cast<double>(denseValsOut.g), static_cast<double>(denseValsOut.b));
    }

    const float denseGreenRatio = clearVals.g / denseVals.g;

    // Calculate logarithms in arbitrary base
    const auto logBase =
        [](float base, float num) -> float
        {
            return std::log(num) / std::log(base);
        };

    // const auto ratio =
    //     [](float a, float b) -> float
    //     {
    //         return a > b ? a / b : b / a;
    //     };

    RGB newExps;
    newExps.r = logBase(clearVals.r / denseVals.r, denseGreenRatio);
    newExps.g = 1.f; // logBase(ratio(clearVals.g, denseVals.g), ratio(denseValsOut.g, clearValsOut.g) );
    newExps.b = logBase(clearVals.b / denseVals.b, denseGreenRatio);



    if (settings->verbose) {
        printf("New exponents:  R=%g G=%g B=%g\n", static_cast<double>(newExps.r), static_cast<double>(newExps.g), static_cast<double>(newExps.b));
    }

    // // Re-adjust color balance based on dense spot values and new exponents
    // calcBalance(rtengine::max(static_cast<float>(params->filmNegative.refInput.g), 1.f),
    //     -newExps[0], -newExps[1], -newExps[2],
    //     denseVals[0], denseVals[1], denseVals[2],
    //     rBal, bBal);

    return newExps;

}

void temp2rgb(double outLev, double temp, double green, RGB &refOut)
{
    rtengine::ColorTemp ct = rtengine::ColorTemp(temp, green, 1., "Custom", standard_observer);

    double rm, gm, bm;
    ct.getMultipliers(rm, gm, bm);

    double maxGain = rtengine::max(rm, gm, bm);

    refOut.r = (rm / maxGain) * outLev;
    refOut.g = (gm / maxGain) * outLev;
    refOut.b = (bm / maxGain) * outLev;
}


void rgb2temp(const RGB &refOut, double &outLev, double &temp, double &green)
{
    double maxVal = rtengine::max(refOut.r, refOut.g, refOut.b);

    rtengine::ColorTemp ct = rtengine::ColorTemp(
                                 refOut.r / maxVal,
                                 refOut.g / maxVal,
                                 refOut.b / maxVal,
                                 1.,
                                 standard_observer);

    outLev = maxVal;
    temp = ct.getTemp();
    green = ct.getGreen();
}


}

FilmNegative::FilmNegative() :
    FoldableToolPanel(this, TOOL_NAME, M("TP_FILMNEGATIVE_LABEL"), false, true),
    EditSubscriber(ET_OBJECTS),
    useAi(Gtk::manage(new Gtk::Button(M("AI_USE")))),
    evFilmNegativeAI(ProcEventMapper::getInstance()->newEvent(ALLNORAW, "HISTORY_MSG_FILMNEGATIVE_AI")),
    NEUTRAL_TEMP(rtengine::ColorTemp(1., 1., 1., 1., rtengine::ColorTemp::DEFAULT_OBSERVER)),
    evFilmNegativeExponents(ProcEventMapper::getInstance()->newEvent(ALLNORAW, "HISTORY_MSG_FILMNEGATIVE_VALUES")),
    evFilmNegativeEnabled(ProcEventMapper::getInstance()->newEvent(ALLNORAW, "HISTORY_MSG_FILMNEGATIVE_ENABLED")),
    evFilmNegativeRefSpot(ProcEventMapper::getInstance()->newEvent(ALLNORAW, "HISTORY_MSG_FILMNEGATIVE_REF_SPOT")),
    evFilmNegativeBalance(ProcEventMapper::getInstance()->newEvent(ALLNORAW, "HISTORY_MSG_FILMNEGATIVE_BALANCE")),
    evFilmNegativeColorSpace(ProcEventMapper::getInstance()->newEvent(ALLNORAW, "HISTORY_MSG_FILMNEGATIVE_COLORSPACE")),
    refInputValues({0.f, 0.f, 0.f}),
    paramsUpgraded(false),
    refLuminance({{0.f, 0.f, 0.f}, 0.f}),
    fnp(nullptr),
    colorSpace(Gtk::manage(new MyComboBoxText())),
    greenExp(createExponentAdjuster(this, M("TP_FILMNEGATIVE_GREEN"), 0.3, 4, 0.01, 1.5)),  // master exponent (green channel)
    redRatio(createExponentAdjuster(this, M("TP_FILMNEGATIVE_RED"), 0.3, 5, 0.01, (2.04 / 1.5))), // ratio of red exponent to master exponent
    blueRatio(createExponentAdjuster(this, M("TP_FILMNEGATIVE_BLUE"), 0.3, 5, 0.01, (1.29 / 1.5))), // ratio of blue exponent to master exponent
    picker(DEFAULT_SPOT_WIDTH, M("TP_FILMNEGATIVE_PICK"), M("TP_FILMNEGATIVE_GUESS_TOOLTIP"), M("TP_FILMNEGATIVE_PICK_SIZE")),
    refInputLabel(Gtk::manage(new Gtk::Label(Glib::ustring::compose(M("TP_FILMNEGATIVE_REF_LABEL"), "- - -")))),
    refPicker(DEFAULT_SPOT_WIDTH, M("TP_FILMNEGATIVE_REF_PICK"), M("TP_FILMNEGATIVE_REF_TOOLTIP"), M("TP_FILMNEGATIVE_REF_SIZE")),
    activePicker(&picker),
    outputLevel(createLevelAdjuster(this, M("TP_FILMNEGATIVE_OUT_LEVEL"))),  // ref level
    greenBalance(createBalanceAdjuster(this, M("TP_FILMNEGATIVE_GREENBALANCE"), -3.0, 3.0, 0.0, "circle-magenta-small", "circle-green-small")),  // green balance
    blueBalance(createBalanceAdjuster(this, M("TP_FILMNEGATIVE_BLUEBALANCE"), -3.0, 3.0, 0.0, "circle-blue-small", "circle-yellow-small"))  // blue balance
{
    setExpandAlignProperties(refInputLabel, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
//    refInputLabel->set_justify(Gtk::Justification::JUSTIFY_CENTER);
//    refInputLabel->set_line_wrap(true);


    colorSpace->append(M("TP_FILMNEGATIVE_COLORSPACE_INPUT"));
    colorSpace->append(M("TP_FILMNEGATIVE_COLORSPACE_WORKING"));
    setExpandAlignProperties(colorSpace, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    colorSpace->set_tooltip_markup(M("TP_FILMNEGATIVE_COLORSPACE_TOOLTIP"));

    Gtk::Grid* csGrid = Gtk::manage(new Gtk::Grid());
    Gtk::Label* csLabel = Gtk::manage(new Gtk::Label(M("TP_FILMNEGATIVE_COLORSPACE")));
    csGrid->attach(*csLabel, 0, 0, 1, 1);
    csGrid->attach(*colorSpace, 1, 0, 1, 1);

    pack_start(*csGrid);

    colorSpace->set_active((int)ColorSpace::WORKING);
    colorSpace->signal_changed().connect(sigc::mem_fun(*this, &FilmNegative::colorSpaceChanged));
    colorSpace->show();

    useAi->set_tooltip_text(M("AI_USE_TOOLTIP"));
    useAi->signal_clicked().connect([this] { if (fnp) fnp->useFilmNegativeAI(); });
    pack_start(*useAi, Gtk::PACK_SHRINK);
    pack_start(*greenExp, Gtk::PACK_SHRINK, 0);
    pack_start(*redRatio, Gtk::PACK_SHRINK, 0);
    pack_start(*blueRatio, Gtk::PACK_SHRINK, 0);
    pack_start(picker, Gtk::PACK_SHRINK, 0);

    Gtk::Separator* const sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    sep->get_style_context()->add_class("grid-row-separator");
    pack_start(*sep, Gtk::PACK_SHRINK, 0);

//    Gtk::Grid* const fbGrid = Gtk::manage(new Gtk::Grid());
//    fbGrid->attach(*refInputLabel, 0, 0, 1, 1);
//    fbGrid->attach(*filmBaseValuesLabel, 1, 0, 1, 1);
//    pack_start(*fbGrid, Gtk::PACK_SHRINK, 0);
    pack_start(*refInputLabel, Gtk::PACK_SHRINK, 0);

    pack_start(*outputLevel, Gtk::PACK_SHRINK, 0);
    pack_start(*blueBalance, Gtk::PACK_SHRINK, 0);
    pack_start(*greenBalance, Gtk::PACK_SHRINK, 0);

    pack_start(refPicker, Gtk::PACK_SHRINK, 0);

    picker.add_button_toggled_event(*this, &FilmNegative::editToggled);
    refPicker.add_button_toggled_event(*this, &FilmNegative::refSpotToggled);

    // Editing geometry; create the spot rectangle
    // TODO: Change behaviour to match that of the white balance spot picker (rectangle disappears behind right toolbar)
    EditRectangle* const spotRect = new EditRectangle();
    spotRect->filled = false;

    visibleGeometry.push_back(spotRect);

    // Stick a dummy rectangle over the whole image in mouseOverGeometry.
    // This is to make sure the getCursor() call is fired everywhere.
    EditRectangle* const imgRect = new EditRectangle();
    imgRect->filled = true;

    mouseOverGeometry.push_back(imgRect);
}

FilmNegative::~FilmNegative()
{
    idle_register.destroy();

    for (auto geometry : visibleGeometry) {
        delete geometry;
    }

    for (auto geometry : mouseOverGeometry) {
        delete geometry;
    }
}


void FilmNegative::readOutputSliders(RGB &refOut)
{
    temp2rgb(fromAdjuster(outputLevel->getValue()),
             NEUTRAL_TEMP.getTemp() / std::pow(2., blueBalance->getValue()),
             NEUTRAL_TEMP.getGreen() / std::pow(2., greenBalance->getValue()),
             refOut);
}

void FilmNegative::writeOutputSliders(const RGB &refOut)
{
    double outLev, cTemp, green;
    rgb2temp(refOut, outLev, cTemp, green);

    outputLevel->setValue(toAdjuster(outLev));
    blueBalance->setValue(std::log2(NEUTRAL_TEMP.getTemp() / cTemp));
    greenBalance->setValue(std::log2(NEUTRAL_TEMP.getGreen() / green));
}


void FilmNegative::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    disableListener();
    paramsUpgraded = false;

    if (pedited) {
        redRatio->setEditedState(pedited->filmNegative.redRatio ? Edited : UnEdited);
        greenExp->setEditedState(pedited->filmNegative.greenExp ? Edited : UnEdited);
        blueRatio->setEditedState(pedited->filmNegative.blueRatio ? Edited : UnEdited);
        outputLevel->setEditedState(pedited->filmNegative.refOutput  ? Edited : UnEdited);
        greenBalance->setEditedState(pedited->filmNegative.refOutput ? Edited : UnEdited);
        blueBalance->setEditedState(pedited->filmNegative.refOutput ? Edited : UnEdited);
        set_inconsistent(multiImage && !pedited->filmNegative.enabled);
    }

    setEnabled(pp->filmNegative.enabled);

    // Reset luminance reference each time params are read
    refLuminance.lum = 0.f;

    colorSpace->set_active(CLAMP((int)pp->filmNegative.colorSpace, 0, 1));
    redRatio->setValue(pp->filmNegative.redRatio);
    greenExp->setValue(pp->filmNegative.greenExp);
    blueRatio->setValue(pp->filmNegative.blueRatio);

    refInputValues = pp->filmNegative.refInput;

    // If reference input values are not set in params, estimated values will be passed in later
    // (after processing) via FilmNegListener
    refInputLabel->set_markup(
        Glib::ustring::compose(M("TP_FILMNEGATIVE_REF_LABEL"), fmt(refInputValues)));

    if (pp->filmNegative.backCompat == BackCompat::CURRENT) {
        outputLevel->show();
        blueBalance->show();
        greenBalance->show();
    } else {
        outputLevel->hide();
        blueBalance->hide();
        greenBalance->hide();
    }

    // If reference output values are not set in params, set the default output
    // chosen for median estimation: gray 1/24th of max
    if (pp->filmNegative.refOutput.r <= 0) {
        float gray = rtengine::MAXVALF / 24.f;
        writeOutputSliders({gray, gray, gray});
    } else {
        writeOutputSliders(pp->filmNegative.refOutput);
    }

    enableListener();
}

void FilmNegative::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    if (colorSpace->get_active_row_number() != 3) {  // UNCHANGED entry, see setBatchMode
        pp->filmNegative.colorSpace = rtengine::procparams::FilmNegativeParams::ColorSpace(colorSpace->get_active_row_number());
    }

    pp->filmNegative.redRatio = redRatio->getValue();
    pp->filmNegative.greenExp = greenExp->getValue();
    pp->filmNegative.blueRatio = blueRatio->getValue();

    pp->filmNegative.enabled = getEnabled();

    if (pedited) {
        pedited->filmNegative.colorSpace = colorSpace->get_active_row_number() != 3; // UNCHANGED entry, see setBatchMode
        pedited->filmNegative.redRatio = redRatio->getEditedState();
        pedited->filmNegative.greenExp = greenExp->getEditedState();
        pedited->filmNegative.blueRatio = blueRatio->getEditedState();
        pedited->filmNegative.refOutput = outputLevel->getEditedState() ||  greenBalance->getEditedState() || blueBalance->getEditedState();
        // In batch mode, make sure refinput is always updated together with the balance sliders
        pedited->filmNegative.refInput = pedited->filmNegative.refOutput || (refInputValues != pp->filmNegative.refInput);
        pedited->filmNegative.enabled = !get_inconsistent();
    }

    pp->filmNegative.refInput = refInputValues;

    readOutputSliders(pp->filmNegative.refOutput);

    if (paramsUpgraded) {
        pp->filmNegative.backCompat = BackCompat::CURRENT;
    }

}

void FilmNegative::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    redRatio->setValue(defParams->filmNegative.redRatio);
    greenExp->setValue(defParams->filmNegative.greenExp);
    blueRatio->setValue(defParams->filmNegative.blueRatio);

    float gray = rtengine::MAXVALF / 24.f;
    writeOutputSliders({gray, gray, gray});

    if (pedited) {
        redRatio->setDefaultEditedState(pedited->filmNegative.redRatio ? Edited : UnEdited);
        greenExp->setDefaultEditedState(pedited->filmNegative.greenExp ? Edited : UnEdited);
        blueRatio->setDefaultEditedState(pedited->filmNegative.blueRatio ? Edited : UnEdited);

        outputLevel->setDefaultEditedState(pedited->filmNegative.refOutput ? Edited : UnEdited);
        greenBalance->setDefaultEditedState(pedited->filmNegative.refOutput ? Edited : UnEdited);
        blueBalance->setDefaultEditedState(pedited->filmNegative.refOutput ? Edited : UnEdited);
    } else {
        redRatio->setDefaultEditedState(Irrelevant);
        greenExp->setDefaultEditedState(Irrelevant);
        blueRatio->setDefaultEditedState(Irrelevant);
        outputLevel->setDefaultEditedState(Irrelevant);
        greenBalance->setDefaultEditedState(Irrelevant);
        blueBalance->setDefaultEditedState(Irrelevant);
    }
}

void FilmNegative::setBatchMode(bool batchMode)
{
    ToolPanel::setBatchMode(batchMode);
    useAi->set_sensitive(!batchMode);

    if (batchMode) {
        picker.remove_if_there(this, false);
        refPicker.remove_if_there(this, false);
        colorSpace->append(M("GENERAL_UNCHANGED"));
        colorSpace->set_active_text(M("GENERAL_UNCHANGED"));
        redRatio->showEditedCB();
        greenExp->showEditedCB();
        blueRatio->showEditedCB();
        removeIfThere(this, refInputLabel, false);
        removeIfThere(this, outputLevel, false);
        removeIfThere(this, greenBalance, false);
        removeIfThere(this, blueBalance, false);
    }
}

void FilmNegative::adjusterChanged(Adjuster* a, double newval)
{
    if (listener && getEnabled()) {
        if (a == redRatio || a == greenExp || a == blueRatio) {
            listener->panelChanged(
                evFilmNegativeExponents,
                Glib::ustring::compose(
                    "Ref=%1\nR=%2\nB=%3",
                    greenExp->getValue(),
                    redRatio->getValue(),
                    blueRatio->getValue()
                )
            );
        } else if (a == outputLevel || a == greenBalance || a == blueBalance) {

            // Reset luminance reference when output level/color sliders are changed
            refLuminance.lum = 0.f;

            listener->panelChanged(
                evFilmNegativeBalance,
                Glib::ustring::compose(
                    "Lev=%1 G=%2 B=%3",
                    outputLevel->getValue(),
                    greenBalance->getValue(),
                    blueBalance->getValue()
                )
            );
        }
    }
}

void FilmNegative::enabledChanged()
{
    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged(evFilmNegativeEnabled, M("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged(evFilmNegativeEnabled, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(evFilmNegativeEnabled, M("GENERAL_DISABLED"));
        }
    }
}

void FilmNegative::colorSpaceChanged()
{
    if (listener) {
        listener->panelChanged(evFilmNegativeColorSpace, colorSpace->get_active_text());
    }
}

void FilmNegative::filmRefValuesChanged(const RGB &refInput, const RGB &refOutput)
{

    idle_register.add(
        [this, refInput, refOutput]() -> bool {
            refInputValues = refInput;
            paramsUpgraded = true;

            disableListener();

            refInputLabel->set_markup(
                Glib::ustring::compose(M("TP_FILMNEGATIVE_REF_LABEL"), fmt(refInputValues)));

            writeOutputSliders(refOutput);

            outputLevel->show();
            blueBalance->show();
            greenBalance->show();

            enableListener();
            return false;
        }
    );

}

void FilmNegative::setFilmNegProvider(FilmNegProvider* provider)
{
    fnp = provider;
}

void FilmNegative::setEditProvider(EditDataProvider* provider)
{
    EditSubscriber::setEditProvider(provider);
}

CursorShape FilmNegative::getCursor(int objectID, int xPos, int yPos) const
{
    return CSSpotWB;
}

bool FilmNegative::mouseOver(int modifierKey)
{
    EditDataProvider* const provider = getEditProvider();
    EditRectangle* const spotRect = static_cast<EditRectangle*>(visibleGeometry.at(0));
    spotRect->setXYWH(provider->posImage.x - activePicker->get_spot_half_width(), provider->posImage.y - activePicker->get_spot_half_width() ,activePicker->get_spot_full_width(), activePicker->get_spot_full_width());

    return true;
}

bool FilmNegative::button1Pressed(int modifierKey)
{
    EditDataProvider* const provider = getEditProvider();

    EditSubscriber::action = EditSubscriber::Action::NONE;

    if (listener) {
        if (picker.get_active()) {

            refSpotCoords.push_back(provider->posImage);

            if (refSpotCoords.size() == 2) {
                // User has selected 2 reference gray spots. Calculating new exponents
                // from channel values and updating parameters.

                RGB ref1, ref2, dummy;

                if (fnp->getFilmNegativeSpot(refSpotCoords[0], picker.get_spot_full_width(), ref1, dummy) &&
                        fnp->getFilmNegativeSpot(refSpotCoords[1], picker.get_spot_full_width(), ref2, dummy)) {

                    disableListener();

                    RGB newExps = getFilmNegativeExponents(ref1, ref2);

                    // Leaving green exponent unchanged, setting red and blue exponents based on
                    // the ratios between newly calculated exponents.
                    redRatio->setValue(newExps.r / newExps.g);
                    blueRatio->setValue(newExps.b / newExps.g);

                    enableListener();

                    if (getEnabled()) {
                        listener->panelChanged(
                            evFilmNegativeExponents,
                            Glib::ustring::compose(
                                "Ref=%1\nR=%2\nB=%3",
                                greenExp->getValue(),
                                redRatio->getValue(),
                                blueRatio->getValue()
                            )
                        );
                    }

                }

                switchOffEditMode();

            }


        } else if (refPicker.get_active()) {

            disableListener();

            // If the luminance reference is not set, copy the current reference input
            // values, and the corresponding output luminance
            if(refLuminance.lum <= 0.f) {
                RGB out;
                readOutputSliders(out);
                refLuminance.input = refInputValues;
                refLuminance.lum = rtengine::Color::rgbLuminance(out.r, out.g, out.b);
            }

            RGB refOut;
            fnp->getFilmNegativeSpot(provider->posImage, refPicker.get_spot_full_width(), refInputValues, refOut);

            // Output luminance of the sampled spot
            float spotLum = rtengine::Color::rgbLuminance(refOut.r, refOut.g, refOut.b);

            float rexp = -(greenExp->getValue() * redRatio->getValue());
            float gexp = -greenExp->getValue();
            float bexp = -(greenExp->getValue() * blueRatio->getValue());

            RGB mult = {
                spotLum / pow_F(rtengine::max(refInputValues.r, 1.f), rexp),
                spotLum / pow_F(rtengine::max(refInputValues.g, 1.f), gexp),
                spotLum / pow_F(rtengine::max(refInputValues.b, 1.f), bexp)
            };

            // Calculate the new luminance of the initial luminance reference spot, by
            // applying current multipliers
            float newRefLum = rtengine::Color::rgbLuminance(
                mult.r * pow_F(rtengine::max(refLuminance.input.r, 1.f), rexp),
                mult.g * pow_F(rtengine::max(refLuminance.input.g, 1.f), gexp),
                mult.b * pow_F(rtengine::max(refLuminance.input.b, 1.f), bexp));

            // Choose a suitable gray value for the sampled spot, so that luminance
            // of the initial reference spot is preserved.
            float gray = spotLum * (refLuminance.lum / newRefLum);

            writeOutputSliders({gray, gray, gray});

            refInputLabel->set_text(
                Glib::ustring::compose(M("TP_FILMNEGATIVE_REF_LABEL"), fmt(refInputValues)));

            enableListener();

            listener->panelChanged(
                evFilmNegativeRefSpot,
                Glib::ustring::compose(
                    "%1, %2, %3",
                    round(refInputValues.r), round(refInputValues.g), round(refInputValues.b)
                )
            );

        }
    }

    return true;
}

bool FilmNegative::button1Released()
{
    EditSubscriber::action = EditSubscriber::Action::NONE;
    return true;
}

bool FilmNegative::button3Pressed(int modifierKey)
{
    EditSubscriber::action = EditSubscriber::Action::NONE;
    switchOffEditMode();
    return true;
}

void FilmNegative::switchOffEditMode()
{
    refSpotCoords.clear();
    unsubscribe();
    picker.set_active(false);
    refPicker.set_active(false);
}

void FilmNegative::editToggled()
{
    if (picker.get_active()) {

        refPicker.set_active(false);
        refSpotCoords.clear();
        activePicker = &picker;

        subscribe();

        int w, h;
        getEditProvider()->getImageSize(w, h);

        // Stick a dummy rectangle over the whole image in mouseOverGeometry.
        // This is to make sure the getCursor() call is fired everywhere.
        EditRectangle* const imgRect = static_cast<EditRectangle*>(mouseOverGeometry.at(0));
        imgRect->setXYWH(0, 0, w, h);
    } else {
        refSpotCoords.clear();
        unsubscribe();
    }
}


void FilmNegative::refSpotToggled()
{
    if (refPicker.get_active()) {

        picker.set_active(false);
        refSpotCoords.clear();
        activePicker = &refPicker;

        subscribe();

        int w, h;
        getEditProvider()->getImageSize(w, h);

        // Stick a dummy rectangle over the whole image in mouseOverGeometry.
        // This is to make sure the getCursor() call is fired everywhere.
        EditRectangle* const imgRect = static_cast<EditRectangle*>(mouseOverGeometry.at(0));
        imgRect->setXYWH(0, 0, w, h);

    } else {
        refSpotCoords.clear();
        unsubscribe();
    }
}

ai_negative::Settings FilmNegative::getAiSettings()
{
    if (!getEnabled()) throw ai_negative::Error("AI_ERROR_ENABLE");
    if (!outputLevel->get_visible()) throw ai_negative::Error("AI_ERROR_LEGACY");
    if (refInputValues.r <= 0 || refInputValues.g <= 0 || refInputValues.b <= 0)
        throw ai_negative::Error("AI_ERROR_REFERENCE_PENDING");
    ai_negative::Settings settings;
    settings.greenExp = greenExp->getValue();
    settings.redRatio = redRatio->getValue();
    settings.blueRatio = blueRatio->getValue();
    settings.outputLevel = outputLevel->getValue();
    settings.blueBalance = blueBalance->getValue();
    settings.greenBalance = greenBalance->getValue();
    settings.refInput = {{refInputValues.r, refInputValues.g, refInputValues.b}};
    ai_negative::validateSettings(settings);
    return settings;
}

void FilmNegative::applyAiSettings(const ai_negative::Settings& settings)
{
    ai_negative::validateSettings(settings);
    switchOffEditMode();
    disableListener();
    greenExp->setValue(settings.greenExp);
    redRatio->setValue(settings.redRatio);
    blueRatio->setValue(settings.blueRatio);
    outputLevel->setValue(settings.outputLevel);
    blueBalance->setValue(settings.blueBalance);
    greenBalance->setValue(settings.greenBalance);
    refInputValues = {static_cast<float>(settings.refInput[0]), static_cast<float>(settings.refInput[1]), static_cast<float>(settings.refInput[2])};
    refInputLabel->set_markup(Glib::ustring::compose(M("TP_FILMNEGATIVE_REF_LABEL"), fmt(refInputValues)));
    refLuminance.lum = 0.f;
    paramsUpgraded = true;
    enableListener();
    if (listener) listener->panelChanged(evFilmNegativeAI, M("AI_APPLIED"));
}
