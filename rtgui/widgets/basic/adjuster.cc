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

#include "adjuster.h"

#include "guiutils.h"
#include "multilangmgr.h"
#include "options.h"
#include "rtimage.h"
#include "rtscalable.h"

#include "rtengine/rt_math.h"

#include <gtkmm/checkbutton.h>

namespace {

constexpr int MIN_RESET_BUTTON_HEIGHT = 17;

double one2one(double val)
{
    return val;
}
}

Adjuster::Adjuster(
    Glib::ustring vlabel,
    double vmin,
    double vmax,
    double vstep,
    double vdefault,
    Gtk::Image *imgIcon1,
    Gtk::Image *imgIcon2,
    double2double_fun slider2value,
    double2double_fun value2slider
) :
    adjustmentName(std::move(vlabel)),
    grid(nullptr),
    label(nullptr),
    imageIcon1(imgIcon1),
    imageIcon2(imgIcon2),
    automatic(nullptr),
    adjusterListener(nullptr),
    spinChange(App::get().options().adjusterMinDelay, App::get().options().adjusterMaxDelay),
    sliderChange(App::get().options().adjusterMinDelay, App::get().options().adjusterMaxDelay),
    editedCheckBox(nullptr),
    afterReset(false),
    blocked(false),
    addMode(false),
    vMin(vmin),
    vMax(vmax),
    vStep(vstep),
    logBase(0),
    logPivot(0),
    logAnchorMiddle(false),
    value2slider(value2slider ? value2slider : &one2one),
    slider2value(slider2value ? slider2value : &one2one)

{
    set_hexpand(true);
    set_vexpand(false);

    if (imageIcon1) {
        setExpandAlignProperties(imageIcon1, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    }

    if (imageIcon2) {
        setExpandAlignProperties(imageIcon2, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    }

    set_column_spacing(0);
    set_column_homogeneous(false);
    set_row_spacing(0);
    set_row_homogeneous(false);

    if (!adjustmentName.empty()) {
        label = Gtk::manage(new Gtk::Label(adjustmentName));
        setExpandAlignProperties(label, true, false, Gtk::ALIGN_START, Gtk::ALIGN_BASELINE);
    }

    reset = Gtk::manage(new Gtk::Button());

    reset->add(*Gtk::manage(new RTImage("undo-small", Gtk::ICON_SIZE_BUTTON)));
    setExpandAlignProperties(reset, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    reset->set_relief(Gtk::RELIEF_NONE);
    reset->set_tooltip_markup(M("ADJUSTER_RESET_TO_DEFAULT"));
    reset->get_style_context()->add_class(GTK_STYLE_CLASS_FLAT);
    reset->set_can_focus(false);

    spin = Gtk::manage(new MySpinButton());

    setExpandAlignProperties(spin, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    spin->set_input_purpose(Gtk::INPUT_PURPOSE_DIGITS);

    reset->set_size_request(-1, RTScalable::scalePixelSize(spin->get_height() > MIN_RESET_BUTTON_HEIGHT ? spin->get_height() : MIN_RESET_BUTTON_HEIGHT));
    slider = Gtk::manage(new MyHScale());
    setExpandAlignProperties(slider, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    slider->set_draw_value(false);
    //slider->set_has_origin(false);  // ------------------ This will remove the colored part on the left of the slider's knob

    setLimits(vmin, vmax, vstep, vdefault);

    if (adjustmentName.empty()) {
        // No label, everything goes in a single row
        attach_next_to(*slider, Gtk::POS_LEFT, 1, 1);

        if (imageIcon1) {
            attach_next_to(*imageIcon1, *slider, Gtk::POS_LEFT, 1, 1);
        }

        if (imageIcon2) {
            attach_next_to(*imageIcon2, *slider, Gtk::POS_RIGHT, 1, 1);
            attach_next_to(*spin, *imageIcon2, Gtk::POS_RIGHT, 1, 1);
        } else {
            attach_next_to(*spin, *slider, Gtk::POS_RIGHT, 1, 1);
        }

        attach_next_to(*reset, *spin, Gtk::POS_RIGHT, 1, 1);
    } else {
        // A label is provided, spreading the widgets in 2 rows
        attach_next_to(*label, Gtk::POS_LEFT, 1, 1);
        attach_next_to(*spin, Gtk::POS_RIGHT, 1, 1);
        // A second Grid is necessary
        grid = Gtk::manage(new Gtk::Grid());
        grid->attach_next_to(*slider, Gtk::POS_LEFT, 1, 1);

        if (imageIcon1) {
            grid->attach_next_to(*imageIcon1, *slider, Gtk::POS_LEFT, 1, 1);
        }

        if (imageIcon2) {
            grid->attach_next_to(*imageIcon2, Gtk::POS_RIGHT, 1, 1);
            grid->attach_next_to(*reset, *imageIcon2, Gtk::POS_RIGHT, 1, 1);
        } else {
            grid->attach_next_to(*reset, *slider, Gtk::POS_RIGHT, 1, 1);
        }

        attach_next_to(*grid, *label, Gtk::POS_BOTTOM, 2, 1);
    }

    defaultVal = ctorDefaultVal = shapeValue(vdefault);
    editedState = defEditedState = Irrelevant;

    spinChange.connect(
        spin->signal_value_changed(),
        sigc::mem_fun(*this, &Adjuster::spinChanged),
        [this]()
        {
            sliderChange.block(true);
            setSliderValue(addMode ? spin->get_value() : this->value2slider(spin->get_value()));
            sliderChange.block(false);
        }
    );
    sliderChange.connect(
        slider->signal_value_changed(),
        sigc::mem_fun(*this, &Adjuster::sliderChanged),
        [this]()
        {
            spinChange.block();
            const double v = shapeValue(getSliderValue());
            spin->set_value(addMode ? v : this->slider2value(v));
            spinChange.unblock();
        }
    );
    reset->signal_button_release_event().connect_notify( sigc::mem_fun(*this, &Adjuster::resetPressed) );

    show_all();
}

Adjuster::~Adjuster ()
{

    sliderChange.block();
    spinChange.block();
    adjusterListener = nullptr;

}

void Adjuster::addAutoButton (const Glib::ustring &tooltip)
{
    if (!automatic) {
        automatic = Gtk::manage(new Gtk::CheckButton());
        //automatic->add (*Gtk::manage (new RTImage ("gears")));
        automatic->set_tooltip_markup(tooltip.length() ? Glib::ustring::compose("<b>%1</b>\n\n%2", M("GENERAL_AUTO"), tooltip) : M("GENERAL_AUTO"));
        setExpandAlignProperties(automatic, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
        autoChange = automatic->signal_toggled().connect( sigc::mem_fun(*this, &Adjuster::autoToggled) );

        if (grid) {
            // Hombre, adding the checkbox next to the reset button because adding it next to the spin button (as before)
            // would diminish the available size for the label and would require a much heavier reorganization of the grid !
            grid->attach_next_to(*automatic, *reset, Gtk::POS_RIGHT, 1, 1);
        } else {
            attach_next_to(*automatic, *reset, Gtk::POS_RIGHT, 1, 1);
        }
        automatic->show();
    }
}

void Adjuster::delAutoButton ()
{
    if (automatic) {
        removeIfThere(grid, automatic);
        delete automatic;
        automatic = nullptr;
    }
}

void Adjuster::throwOnButtonRelease(bool throwOnBRelease)
{

    if (throwOnBRelease) {
        if (!buttonReleaseSlider.connected()) {
            buttonReleaseSlider = slider->signal_button_release_event().connect_notify( sigc::mem_fun(*this, &Adjuster::sliderReleased) );
        }

        if (!buttonReleaseSpin.connected()) {
            buttonReleaseSpin = spin->signal_button_release_event().connect_notify( sigc::mem_fun(*this, &Adjuster::spinReleased) );    // Use the same callback hook
        }
    } else {
        if (buttonReleaseSlider.connected()) {
            buttonReleaseSlider.disconnect();
        }

        if (buttonReleaseSpin.connected()) {
            buttonReleaseSpin.disconnect();
        }
    }
}

void Adjuster::setDefault (double def)
{

    defaultVal = shapeValue(def);
}

void Adjuster::setDefaultEditedState (EditedState eState)
{

    defEditedState = eState;
}

void Adjuster::autoToggled ()
{

    if (adjusterListener && !blocked) {
        adjusterListener->adjusterAutoToggled(this, automatic->get_active());
    }
}

void Adjuster::sliderReleased (GdkEventButton* event)
{

    if ((event != nullptr) && (event->button == 1)) {
        sliderChange.cancel();

        notifyListener();
    }
}

void Adjuster::spinReleased (GdkEventButton* event)
{

    if (event) {
        spinChange.cancel();

        notifyListener();
    }
}

void Adjuster::resetValue (bool toInitial)
{
    if (editedState != Irrelevant) {
        editedState = defEditedState;

        if (editedCheckBox) {
            editedChange.block(true);
            editedCheckBox->set_active(defEditedState == Edited);
            editedChange.block(false);
        }

    }

    afterReset = true;

    if (toInitial) {
        // resetting to the initial editing value, when the image has been loaded
        setSliderValue(addMode ? defaultVal : value2slider(defaultVal));
    } else {
        // resetting to the slider default value
        if (addMode) {
            setSliderValue(0.);
        } else {
            setSliderValue(value2slider(ctorDefaultVal));
        }
    }
}

// Please note that it won't change the "Auto" CheckBox's state, if there
void Adjuster::resetPressed (GdkEventButton* event)
{

    if ((event != nullptr) && (event->state & GDK_CONTROL_MASK) && (event->button == 1)) {
        resetValue(true);
    } else {
        resetValue(false);
    }
}

double Adjuster::shapeValue (double a) const
{
    const double pow10 = std::pow(10.0, digits);
    const double val = std::round(a * pow10) / pow10;
    return val == -0.0 ? 0.0 : val;
}

void Adjuster::setLimits (double vmin, double vmax, double vstep, double vdefault)
{
    sliderChange.block(true);
    spinChange.block(true);

    double pow10 = vstep;
    for (digits = 0; std::fabs(pow10 - floor(pow10)) > 0.000000000001; digits++, pow10 *= 10.0);

    const double shapeVal = shapeValue(vdefault);
    spin->set_digits(digits);
    spin->set_increments(vstep, 2.0 * vstep);
    spin->set_range(vmin, vmax);
    spin->updateSize();
    spin->set_value(shapeVal);

    slider->set_digits(digits);
    slider->set_increments(vstep, 2.0 * vstep);
    slider->set_range(addMode ? vmin : value2slider(vmin), addMode ? vmax : value2slider(vmax));
    setSliderValue(addMode ? shapeVal : value2slider(shapeVal));

    sliderChange.block(false);
    spinChange.block(false);
}

void Adjuster::setAddMode(bool addM)
{
    if (addM != addMode) {
        // Switching the Adjuster to the new mode
        addMode = addM;

        if (addM) {
            // Switching to the relative mode
            double range = -vMin + vMax;

            if (range < 0.) {
                range = -range;
            }

            setLimits(-range, range, vStep, 0);
        } else {
            // Switching to the absolute mode
            setLimits(vMin, vMax, vStep, defaultVal);
        }
    }
}

void Adjuster::spinChanged()
{
    if (adjusterListener && !blocked) {
        if (!buttonReleaseSlider.connected() || afterReset) {
            if (automatic) {
                setAutoValue(false);
            }
            adjusterListener->adjusterChanged(this, spin->get_value());
        }
    }

    if (editedState == UnEdited) {
        editedState = Edited;

        if (editedCheckBox) {
            editedChange.block(true);
            editedCheckBox->set_active(true);
            editedChange.block(false);
        }
    }

    afterReset = false;
}

void Adjuster::sliderChanged ()
{
    if (adjusterListener && !blocked) {
        if (!buttonReleaseSlider.connected() || afterReset) {
            if (automatic) {
                setAutoValue(false);
            }
            adjusterListener->adjusterChanged(this, spin->get_value());
        }
    }

    if (!afterReset && editedState == UnEdited) {
        editedState = Edited;

        if (editedCheckBox) {
            editedChange.block(true);
            editedCheckBox->set_active(true);
            editedChange.block(false);
        }
    }

    afterReset = false;
}

void Adjuster::setValue (double a)
{
    spinChange.block();
    sliderChange.block(true);
    spin->set_value(shapeValue(a));
    setSliderValue(addMode ? shapeValue(a) : value2slider(shapeValue(a)));
    sliderChange.block(false);
    spinChange.unblock();
    afterReset = false;
}

void Adjuster::setAutoValue (bool a)
{
    if (automatic) {
        const bool oldVal = autoChange.block(true);
        automatic->set_active(a);
        autoChange.block(oldVal);
    }
}

bool Adjuster::notifyListener ()
{
    if (adjusterListener != nullptr && !blocked) {
        if (automatic) {
            setAutoValue(false);
        }
        adjusterListener->adjusterChanged(this, spin->get_value());
    }

    return false;
}

bool Adjuster::notifyListenerAutoToggled ()
{

    if (adjusterListener != nullptr && !blocked) {
        adjusterListener->adjusterAutoToggled(this, automatic->get_active());
    }

    return false;
}

void Adjuster::setEnabled (bool enabled)
{

    const bool autoVal = automatic && !editedCheckBox ? automatic->get_active() : true;
    spin->set_sensitive(enabled && autoVal);
    slider->set_sensitive(enabled && autoVal);

    if (automatic) {
        automatic->set_sensitive(enabled);
    }
}

void Adjuster::setEditedState (EditedState eState)
{

    if (editedState != eState) {
        if (editedCheckBox) {
            editedChange.block(true);
            editedCheckBox->set_active(eState == Edited);
            editedChange.block(false);
        }

        editedState = eState;
    }
}

EditedState Adjuster::getEditedState ()
{

    if (editedState != Irrelevant && editedCheckBox) {
        editedState = editedCheckBox->get_active() ? Edited : UnEdited;
    }

    return editedState;
}

void Adjuster::showEditedCB ()
{

    if (label) {
        removeIfThere(this, label, false);
    }

    if (!editedCheckBox) {
        editedCheckBox = Gtk::manage(new Gtk::CheckButton(adjustmentName));
        editedCheckBox->set_vexpand(false);

        if (grid) {
            editedCheckBox->set_hexpand(true);
            editedCheckBox->set_halign(Gtk::ALIGN_START);
            editedCheckBox->set_valign(Gtk::ALIGN_CENTER);
            attach_next_to(*editedCheckBox, *spin, Gtk::POS_LEFT, 1, 1);
        } else {
            editedCheckBox->set_hexpand(false);
            editedCheckBox->set_halign(Gtk::ALIGN_START);
            editedCheckBox->set_valign(Gtk::ALIGN_CENTER);

            if (imageIcon1) {
                attach_next_to(*editedCheckBox, *imageIcon1, Gtk::POS_LEFT, 1, 1);
            } else {
                attach_next_to(*editedCheckBox, *slider, Gtk::POS_LEFT, 1, 1);
            }
        }

        editedChange = editedCheckBox->signal_toggled().connect( sigc::mem_fun(*this, &Adjuster::editedToggled) );
        editedCheckBox->show();
    }
}

void Adjuster::editedToggled ()
{
    if (adjusterListener && !blocked) {
        if (automatic) {
            setAutoValue(false);
        }
        adjusterListener->adjusterChanged(this, spin->get_value());
    }
}

void Adjuster::trimValue (double &val) const
{
    val = rtengine::LIM(val, vMin, vMax);
}

void Adjuster::trimValue (int &val) const
{
    val = rtengine::LIM<int>(val, vMin, vMax);
}

void Adjuster::trimValue (float &val) const
{
    val = rtengine::LIM<float>(val, vMin, vMax);
}

double Adjuster::getSliderValue() const
{
    double val = slider->get_value();
    if (logBase) {
        if (logAnchorMiddle) {
            double mid = (vMax - vMin) / 2;
            double mmid = vMin + mid;
            if (val >= mmid) {
                double range = vMax - mmid;
                double x = (val - mmid) / range;
                val = logPivot + (std::pow(logBase, x) - 1.0) / (logBase - 1.0) * (vMax - logPivot);
            } else {
                double range = mmid - vMin;
                double x = (mmid - val) / range;
                val = logPivot - (std::pow(logBase, x) - 1.0) / (logBase - 1.0) * (logPivot - vMin);
            }
        } else {
            if (val >= logPivot) {
                double range = vMax - logPivot;
                double x = (val - logPivot) / range;
                val = logPivot + (std::pow(logBase, x) - 1.0) / (logBase - 1.0) * range;
            } else {
                double range = logPivot - vMin;
                double x = (logPivot - val) / range;
                val = logPivot - (std::pow(logBase, x) - 1.0) / (logBase - 1.0) * range;
            }
        }
    }
    return val;
}

void Adjuster::setSliderValue(double val)
{
    if (logBase) {
        if (logAnchorMiddle) {
            double mid = (vMax - vMin) / 2;
            if (val >= logPivot) {
                double range = vMax - logPivot;
                double x = (val - logPivot) / range;
                val = (vMin + mid) + std::log1p(x * (logBase - 1.0)) / std::log(logBase) * mid;
            } else {
                double range = logPivot - vMin;
                double x = (logPivot - val) / range;
                val = (vMin + mid) - std::log1p(x * (logBase - 1.0)) / std::log(logBase) * mid;
            }
        } else {
            if (val >= logPivot) {
                double range = vMax - logPivot;
                double x = (val - logPivot) / range;
                val = logPivot + std::log1p(x * (logBase - 1.0)) / std::log(logBase) * range;
            } else {
                double range = logPivot - vMin;
                double x = (logPivot - val) / range;
                val = logPivot - std::log1p(x * (logBase - 1.0)) / std::log(logBase) * range;
            }
        }
    }
    slider->set_value(val);
}

void Adjuster::setLogScale(double base, double pivot, bool anchorMiddle)
{
    spinChange.block(true);
    sliderChange.block(true);

    const double cur = getSliderValue();
    logBase = base;
    logPivot = pivot;
    logAnchorMiddle = anchorMiddle;
    setSliderValue(cur);

    sliderChange.block(false);
    spinChange.block(false);
}

bool Adjuster::getAutoValue() const
{
    return automatic ? automatic->get_active() : false;
}

void Adjuster::setAutoInconsistent(bool i)
{
    if (automatic) {
        automatic->set_inconsistent(i);
    }
}

bool Adjuster::getAutoInconsistent() const
{
    return automatic ? automatic->get_inconsistent() : true /* we have to return something */;
}

void Adjuster::setAdjusterListener (AdjusterListener* alistener)
{
    adjusterListener = alistener;
}

double Adjuster::getValue() const
{
    return shapeValue(spin->get_value());
}

int Adjuster::getIntValue() const
{
    return spin->get_value_as_int();
}

Glib::ustring Adjuster::getTextValue() const
{
    if (addMode) {
        return Glib::ustring::compose("<i>%1</i>", spin->get_text());
    } else {
        return spin->get_text();
    }
}

void Adjuster::setLabel(const Glib::ustring &lbl)
{
    label->set_label(lbl);
}

bool Adjuster::block(bool isBlocked)
{
    bool oldValue = blocked;
    blocked = isBlocked;
    return oldValue;
}

bool Adjuster::getAddMode() const
{
    return addMode;
}

void Adjuster::setDelay(unsigned int min_delay_ms, unsigned int max_delay_ms)
{
    spinChange.setDelay(min_delay_ms, max_delay_ms);
    sliderChange.setDelay(min_delay_ms, max_delay_ms);
}

void Adjuster::showIcons(bool yes)
{
    if (imageIcon1) {
        imageIcon1->set_visible(yes);
        imageIcon1->set_no_show_all(!yes);
    }
    if (imageIcon2) {
        imageIcon2->set_visible(yes);
        imageIcon2->set_no_show_all(!yes);
    }
}
