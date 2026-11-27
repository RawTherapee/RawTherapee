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
#pragma once

#include <gtkmm/button.h>

#include "popupcommon.h"

class PopUpButton final : public Gtk::Button, public PopUpCommon
{

public:
    PopUpButton(const Glib::ustring& label = Glib::ustring(), bool nextOnClicked = false);
    void show();
    void set_tooltip_text(const Glib::ustring& text);
    void set_sensitive(bool isSensitive = true);

protected:
    bool on_button_release_event(GdkEventButton* event) override;

private:
    bool nextOnClicked;
};
