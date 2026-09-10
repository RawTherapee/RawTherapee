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

#include "spotpicker.h"

SpotPicker::SpotPicker(int const defaultValue, Glib::ustring const &buttonKey, Glib::ustring const &buttonTooltip, Glib::ustring const &labelKey) :
    Gtk::Grid(),
    _spotHalfWidth(defaultValue),
    _spotLabel(labelSetup(labelKey)),
    _spotSizeSetter(MyComboBoxText(selecterSetup())),
    _spotButton(spotButtonTemplate(buttonKey, buttonTooltip))

{
    this->get_style_context()->add_class("grid-spacing");
    setExpandAlignProperties(this, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    this->attach (_spotButton, 0, 0, 1, 1);
    this->attach (_spotLabel, 1, 0, 1, 1);
    this->attach (_spotSizeSetter, 2, 0, 1, 1);
    _spotSizeSetter.signal_changed().connect( sigc::mem_fun(*this, &SpotPicker::spotSizeChanged));
}

Gtk::Label SpotPicker::labelSetup(Glib::ustring const &key) const
{
    Gtk::Label label(key);
    setExpandAlignProperties(&label, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    return label;
}

MyComboBoxText SpotPicker::selecterSetup() const
{
    MyComboBoxText spotSize = MyComboBoxText();
    setExpandAlignProperties(&spotSize, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    spotSize.append ("2");
    if (_spotHalfWidth == 2) {
        spotSize.set_active(0);
    }

    spotSize.append ("4");

    if (_spotHalfWidth == 4) {
        spotSize.set_active(1);
    }

    spotSize.append ("8");

    if (_spotHalfWidth == 8) {
        spotSize.set_active(2);
    }

    spotSize.append ("16");

    if (_spotHalfWidth == 16) {
        spotSize.set_active(3);
    }

    spotSize.append ("32");

    if (_spotHalfWidth == 32) {
        spotSize.set_active(4);
    }
    return spotSize;
}

Gtk::ToggleButton SpotPicker::spotButtonTemplate(Glib::ustring const &key, const Glib::ustring &tooltip) const
{
    Gtk::ToggleButton spotButton = Gtk::ToggleButton(key);
    setExpandAlignProperties(&spotButton, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    spotButton.get_style_context()->add_class("independent");
    spotButton.set_tooltip_text(tooltip);
    spotButton.set_image_from_icon_name("color-picker-small");
    return spotButton;
}

void SpotPicker::spotSizeChanged()
{
    _spotHalfWidth = atoi(_spotSizeSetter.get_active_text().c_str());
}
