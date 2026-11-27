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
 *  Class created by Jean-Christophe FRISCH, aka 'Hombre'
 */

#include "popupbutton.h"

#include <gtkmm/grid.h>

/*
 * PopUpButton::PopUpButton (const Glib::ustring& label, bool imgRight)
 *
 * Creates a button with a contextual menu where you can select an item that the button content will reflect
 *
 * Parameters:
 *      label = label displayed in the button
 *      nextOnClicked = selects the next entry if the button is clicked
 */
PopUpButton::PopUpButton(const Glib::ustring& label, bool nextOnClicked) :
    Gtk::Button(), PopUpCommon(this, label), nextOnClicked(nextOnClicked)
{
}

void PopUpButton::show()
{
    PopUpCommon::show();
}
void PopUpButton::set_tooltip_text(const Glib::ustring& text)
{
    PopUpCommon::set_tooltip_text(text);
}

void PopUpButton::set_sensitive(bool isSensitive)
{
    buttonGroup->set_sensitive(isSensitive);
}

bool PopUpButton::on_button_release_event(GdkEventButton* event)
{
    if (nextOnClicked && getEntryCount() > 1) {
        const int last = getEntryCount() - 1;
        int next = getSelected();

        if (event->state & GDK_SHIFT_MASK) {
            next = next > 0 ? next - 1 : last;
        } else {
            next = next < last ? next + 1 : 0;
        }

        entrySelected(next);
    }

    return Gtk::Button::on_button_release_event(event);
}
