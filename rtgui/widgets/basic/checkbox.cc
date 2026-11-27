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

#include <gtkmm.h>

#include "checkbox.h"
#include "guiutils.h"
#include "multilangmgr.h"

CheckBox::CheckBox(Glib::ustring label, bool const& multiImageVal) :
    Gtk::CheckButton(label),
    listener(nullptr),
    lastActive(false),
    multiImage(multiImageVal)
{
    conn = signal_toggled().connect(sigc::mem_fun(*this, &CheckBox::buttonToggled));
}

void CheckBox::buttonToggled()
{

    CheckValue newValue = CheckValue::unchanged;

    if (multiImage) {
        if (get_inconsistent()) {
            set_inconsistent(false);
            ConnectionBlocker bloker(conn);
            set_active(false);
            newValue = CheckValue::off;
        } else if (getLastActive()) {
            set_inconsistent(true);
            newValue = CheckValue::unchanged;
        }
    } else {
        newValue = get_active() ? CheckValue::on : CheckValue::off;
    }
    setLastActive();

    if (listener) {
        listener->checkBoxToggled(this, newValue);
    }
}

void CheckBox::setLastActive()
{
    lastActive = get_active();
}

// return the actual bool value, ignoring the inconsistent state
bool CheckBox::getLastActive()
{
    return lastActive;
}

void CheckBox::setValue(CheckValue newValue)
{

    ConnectionBlocker blocker(conn);
    switch (newValue) {
        case CheckValue::on:
            set_inconsistent(false);
            set_active(true);
            lastActive = true;
            break;
        case CheckValue::off:
            set_inconsistent(false);
            set_active(false);
            lastActive = false;
            break;
        case CheckValue::unchanged:
            set_inconsistent(true);
            break;
        default:
            break;
    }
}

void CheckBox::setValue(bool active)
{

    ConnectionBlocker blocker(conn);
    set_inconsistent(false);
    set_active(active);
    lastActive = active;
}

CheckValue CheckBox::getValue()
{
    return (get_inconsistent() ? CheckValue::unchanged
            : get_active()     ? CheckValue::on
                               : CheckValue::off);
}

Glib::ustring CheckBox::getValueAsStr()
{
    if (get_inconsistent()) {
        return M("GENERAL_UNCHANGED");
    } else if (get_active()) {
        return M("GENERAL_ENABLED");
    } else {
        return M("GENERAL_DISABLED");
    }
}

/*
void CheckBox::set_sensitive (bool isSensitive)
{
    Gtk::CheckButton::set_sensitive(isSensitive);
}

void CheckBox::set_tooltip_text (const Glib::ustring& tooltip)
{
    Gtk::CheckButton::set_tooltip_text (tooltip);
}

void CheckBox::set_tooltip_markup (const Glib::ustring& tooltip)
{
    Gtk::CheckButton::set_tooltip_markup (tooltip);
}
*/

void CheckBox::setEdited(bool edited)
{

    ConnectionBlocker blocker(conn);
    set_inconsistent(!edited);
    if (edited) {
        set_active(lastActive);
    }
}

bool CheckBox::getEdited()
{

    return !get_inconsistent();
}

void CheckBox::setCheckBoxListener(CheckBoxListener* cblistener)
{
    listener = cblistener;
}
