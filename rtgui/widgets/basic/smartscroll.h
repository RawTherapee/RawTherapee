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

#include <gtkmm/combobox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>

/**
 * @brief subclass of Gtk::ScrolledWindow in order to handle the scrollwheel
 */
class MyScrolledWindow final : public Gtk::ScrolledWindow
{

    bool on_scroll_event (GdkEventScroll* event) override;
    void get_preferred_width_vfunc (int& minimum_width, int& natural_width) const override;
    void get_preferred_height_vfunc (int& minimum_height, int& natural_height) const override;
    void get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const override;

public:
    MyScrolledWindow();
};

/**
 * @brief subclass of Gtk::ScrolledWindow in order to handle the large toolbars (wider than available space)
 */
class MyScrolledToolbar final : public Gtk::ScrolledWindow
{

    bool on_scroll_event (GdkEventScroll* event) override;
    void get_preferred_height_vfunc (int& minimum_height, int& natural_height) const override;

public:
    MyScrolledToolbar();
};

/**
 * @brief subclass of Gtk::ComboBox in order to handle the scrollwheel
 */
class MyComboBox : public Gtk::ComboBox
{
    int naturalWidth, minimumWidth;

    bool on_scroll_event (GdkEventScroll* event) override;
    void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const override;
    void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const override;

public:
    MyComboBox ();

    void setPreferredWidth (int minimum_width, int natural_width);
};

/**
 * @brief subclass of Gtk::ComboBoxText in order to handle the scrollwheel
 */
class MyComboBoxText final : public Gtk::ComboBoxText
{
    int naturalWidth, minimumWidth;
    sigc::connection myConnection;

    bool on_scroll_event (GdkEventScroll* event) override;
    void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const override;
    void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const override;

public:
    explicit MyComboBoxText (bool has_entry = false);

    void setPreferredWidth (int minimum_width, int natural_width);
    void connect(const sigc::connection &connection) { myConnection = connection; }
    void block(bool blocked) { myConnection.block(blocked); }
};

/**
 * @brief subclass of Gtk::SpinButton in order to handle the scrollwheel
 */
class MySpinButton final : public Gtk::SpinButton
{

protected:
    bool on_scroll_event (GdkEventScroll* event) override;
    bool on_key_press_event (GdkEventKey* event) override;

public:
    MySpinButton ();
    void updateSize();
};

/**
 * @brief subclass of Gtk::Scale in order to handle the scrollwheel
 */
class MyHScale final : public Gtk::Scale
{

protected:
    bool on_scroll_event (GdkEventScroll* event) override;
    bool on_key_press_event (GdkEventKey* event) override;

};

