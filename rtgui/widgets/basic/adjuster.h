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

#include "editedstate.h"
#include "delayed.h"
#include "widgets/basic/smartscroll.h"

#include <gtkmm/grid.h>

namespace Gtk {

class CheckButton;

}  // namespace Gtk

class Adjuster;

class AdjusterListener
{
public:
    virtual ~AdjusterListener() = default;
    virtual void adjusterChanged (Adjuster* a, double newval) = 0;
    virtual void adjusterAutoToggled (Adjuster* a, bool newval) {}
};

typedef double(*double2double_fun)(double val);

class Adjuster final : public Gtk::Grid
{
protected:
    Glib::ustring adjustmentName;
    Gtk::Grid* grid;
    Gtk::Label* label;
    Gtk::Image *imageIcon1;
    Gtk::Image *imageIcon2;
    MyHScale* slider;
    MySpinButton* spin;
    Gtk::Button* reset;
    Gtk::CheckButton* automatic;
    AdjusterListener* adjusterListener;
    DelayedConnection<> spinChange;
    DelayedConnection<> sliderChange;
    sigc::connection editedChange;
    sigc::connection autoChange;
    sigc::connection buttonReleaseSlider;
    sigc::connection buttonReleaseSpin;
    double defaultVal;          // current default value (it can change when switching from ADD or SET mode)
    double ctorDefaultVal;      // default value at construction time
    EditedState editedState;
    EditedState defEditedState;
    int digits;
    Gtk::CheckButton* editedCheckBox;
    bool afterReset;
    bool blocked;
    bool addMode;
    double vMin;
    double vMax;
    double vStep;

    double logBase;
    double logPivot;
    bool logAnchorMiddle;

    double shapeValue (double a) const;
    double2double_fun value2slider, slider2value;

    double getSliderValue() const;
    void setSliderValue(double val);

public:
    Adjuster(
        Glib::ustring vlabel,
        double vmin,
        double vmax,
        double vstep,
        double vdefault,
        Gtk::Image *imgIcon1 = nullptr,
        Gtk::Image *imgIcon2 = nullptr,
        double2double_fun slider2value = nullptr,
        double2double_fun value2slider = nullptr
    );
    ~Adjuster() override;

    // Add an "Automatic" checkbox next to the reset button.
    void addAutoButton(const Glib::ustring &tooltip = "");
    // Send back the value of og the Auto checkbox
    void delAutoButton();
    bool getAutoValue() const;
    void setAutoValue(bool a);
    bool notifyListenerAutoToggled();
    void autoToggled();
    void setAutoInconsistent(bool i);
    bool getAutoInconsistent() const;
    void setAdjusterListener(AdjusterListener* alistener);
    // return the value trimmed to the limits at construction time
    double getValue() const;
    // return the value trimmed to the limits at construction time
    int getIntValue() const;
    // return the value trimmed to the limits at construction time,
    // method only used by the history manager, so decoration is added if addMode=true
    Glib::ustring getTextValue() const;
    void setLabel (const Glib::ustring &lbl);
    void setValue (double a);
    void setLimits (double vmin, double vmax, double vstep, double vdefault);
    void setEnabled (bool enabled);
    void setDefault (double def);
    // will let the adjuster throw it's "changed" signal when the mouse button is released. Can work altogether with the delay value.
    void throwOnButtonRelease(bool throwOnBRelease = true);
    void setEditedState (EditedState eState);
    EditedState getEditedState ();
    void setDefaultEditedState (EditedState eState);
    void showEditedCB ();
    bool block(bool isBlocked);
    void setAddMode(bool addM);
    bool getAddMode() const;
    void spinChanged ();
    void sliderChanged ();
    bool notifyListener ();
    void sliderReleased (GdkEventButton* event);
    void spinReleased (GdkEventButton* event);
    void resetValue (bool toInitial);
    void resetPressed (GdkEventButton* event);
    void editedToggled ();
    void trimValue (double &val) const;
    void trimValue (float &val) const;
    void trimValue (int &val) const;
    void setLogScale(double base, double pivot, bool anchorMiddle = false);
    void setDelay(unsigned int min_delay_ms, unsigned int max_delay_ms = 0);
    void showIcons(bool yes);
};
