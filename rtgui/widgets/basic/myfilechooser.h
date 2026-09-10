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

#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
// #include <gtkmm/filefilter.h>
#include <gtkmm/filechooser.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

class MyFileChooserWidget
{
public:
    virtual ~MyFileChooserWidget() = default;

    sigc::signal<void> &signal_selection_changed();
    sigc::signal<void> &signal_file_set();

    std::string get_filename() const;
    bool set_filename(const std::string &filename);

    void add_filter(const Glib::RefPtr<Gtk::FileFilter> &filter);
    void remove_filter(const Glib::RefPtr<Gtk::FileFilter> &filter);
    void set_filter(const Glib::RefPtr<Gtk::FileFilter> &filter);
    std::vector<Glib::RefPtr<Gtk::FileFilter>> list_filters() const;

    bool set_current_folder(const std::string &filename);
    std::string get_current_folder() const;

    bool add_shortcut_folder(const std::string &folder);
    bool remove_shortcut_folder(const std::string &folder);

    void unselect_all();
    void unselect_filename(const std::string &filename);

    void set_show_hidden(bool yes);

protected:
    explicit MyFileChooserWidget(const Glib::ustring &title, Gtk::FileChooserAction action=Gtk::FILE_CHOOSER_ACTION_OPEN);

    static std::unique_ptr<Gtk::Image> make_folder_image();

    void show_chooser(Gtk::Widget *parent);
    virtual void on_filename_set();

private:
    class Impl;

    std::unique_ptr<Impl> pimpl;
};

/**
 * @brief subclass of Gtk::FileChooserButton in order to handle the scrollwheel
 */
class MyFileChooserButton final : public Gtk::Button, public MyFileChooserWidget
{
private:
    class Impl;

    std::unique_ptr<Impl> pimpl;

protected:
    bool on_scroll_event (GdkEventScroll* event) override;
    void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const override;
    void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const override;

    void on_filename_set() override;

public:
    explicit MyFileChooserButton(const Glib::ustring &title, Gtk::FileChooserAction action=Gtk::FILE_CHOOSER_ACTION_OPEN);
};

class MyFileChooserEntry : public Gtk::Box, public MyFileChooserWidget
{
public:
    explicit MyFileChooserEntry(const Glib::ustring &title, Gtk::FileChooserAction action = Gtk::FILE_CHOOSER_ACTION_OPEN);

    Glib::ustring get_placeholder_text() const;
    void set_placeholder_text(const Glib::ustring &text);

protected:
    void on_filename_set() override;

private:
    class Impl;

    std::unique_ptr<Impl> pimpl;
};
