/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2017 Jean-Christophe FRISCH <natureh.510@gmail.com>
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

#include <gtkmm.h>

#include "editedstate.h"
#include "guiutils.h"

class CheckBox;

enum class CheckValue { on, off, unchanged };

class CheckBoxListener
{
public:
    virtual ~CheckBoxListener() = default;
    virtual void checkBoxToggled(CheckBox* c, CheckValue newval) = 0;
};

/**
 * @brief subclass of Gtk::CheckButton for convenience
 */
class CheckBox :
    // Should ideally be private, but in this case build fail on the instantiation
    public Gtk::CheckButton
{

    CheckBoxListener* listener;
    bool lastActive;
    bool const& multiImage;
    sigc::connection conn;
    void buttonToggled();
    void setLastActive();

public:
    //using CheckButton::CheckButton;
    explicit CheckBox(Glib::ustring label, bool const& multiImageVal);
    bool getLastActive();
    void setValue(CheckValue newValue);
    void setValue(bool active);
    CheckValue getValue();
    void setEdited(bool edited);
    bool getEdited();
    Glib::ustring getValueAsStr();

    void setCheckBoxListener(CheckBoxListener* cblistener);

    /* Used if the Gtk::CheckButton parent class can be private
     *
    void set_sensitive (bool isSensitive = true);
    void set_tooltip_text (const Glib::ustring& tooltip);
    void set_tooltip_markup (const Glib::ustring& tooltip);
    */
};
