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

#include "guiutils.h"
#include "widgets/basic/smartscroll.h"

#include <glibmm/ustring.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/togglebutton.h>

/**
 * @brief A gui element for picking spots on an image
 */
class SpotPicker : public Gtk::Grid
{
    private:
        int _spotHalfWidth;
        Gtk::Label _spotLabel;
        MyComboBoxText _spotSizeSetter;
        Gtk::ToggleButton _spotButton;
    public:
        SpotPicker(int const defaultValue, Glib::ustring const &buttonKey, Glib::ustring const &buttonTooltip, Glib::ustring const &labelKey);
        inline bool get_active() const
        {
            return _spotButton.get_active();
        }
        void set_active(bool b)
        {
            _spotButton.set_active(b);
        }
        int get_spot_half_width() const
        {
            return _spotHalfWidth;
        }
        int get_spot_full_width() const
        {
            return _spotHalfWidth * 2;
        }
        template <class T_return, class T_obj> void add_button_toggled_event(T_return& returnv, const T_obj function)
        {
            _spotButton.signal_toggled().connect(sigc::mem_fun(returnv, function));
        }
        bool remove_if_there(Gtk::Container* cont, bool increference = true)
        {
            return removeIfThere(cont, &_spotButton, increference);
        }

    protected:
        Gtk::Label labelSetup(Glib::ustring const &key) const;
        MyComboBoxText selecterSetup() const;
        Gtk::ToggleButton spotButtonTemplate(Glib::ustring const &key, const Glib::ustring &tooltip) const;
        void spotSizeChanged();
};

