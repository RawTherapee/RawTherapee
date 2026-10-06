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

#include "popuptogglebutton.h"

/*
 * PopUpToggleButton::PopUpToggleButton (const Glib::ustring& label, bool imgRight)
 *
 * Creates a toggle button with a contextual menu where you can select an item that the button content will reflect
 *
 * Parameters:
 *      label = label displayed in the button
 */
PopUpToggleButton::PopUpToggleButton(const Glib::ustring& label) :
    Gtk::ToggleButton(), PopUpCommon(this, label)
{
}

void PopUpToggleButton::show()
{
    PopUpCommon::show();
}
void PopUpToggleButton::set_tooltip_text(const Glib::ustring& text)
{
    PopUpCommon::set_tooltip_text(text);
}
