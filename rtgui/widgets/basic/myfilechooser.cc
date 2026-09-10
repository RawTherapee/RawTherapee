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

#include "myfilechooser.h"

#include "guiutils.h"
#include "multilangmgr.h"
#include "rtimage.h"
#include "rtscalable.h"

class MyFileChooserWidget::Impl
{
public:
    Impl(const Glib::ustring &title, Gtk::FileChooserAction action) :
        title_(title),
        action_(action)
    {
    }

    Glib::ustring title_;
    Gtk::FileChooserAction action_;
    std::string filename_;
    std::string current_folder_;
    std::vector<Glib::RefPtr<Gtk::FileFilter>> file_filters_;
    Glib::RefPtr<Gtk::FileFilter> cur_filter_;
    std::vector<std::string> shortcut_folders_;
    bool show_hidden_{false};
    sigc::signal<void> selection_changed_;
};


MyFileChooserWidget::MyFileChooserWidget(const Glib::ustring &title, Gtk::FileChooserAction action) :
    pimpl(new Impl(title, action))
{
}


std::unique_ptr<Gtk::Image> MyFileChooserWidget::make_folder_image()
{
    return std::unique_ptr<Gtk::Image>(new RTImage("folder-open-small", Gtk::ICON_SIZE_BUTTON));
}

void MyFileChooserWidget::show_chooser(Gtk::Widget *parent)
{
    Gtk::FileChooserDialog dlg(getToplevelWindow(parent), pimpl->title_, pimpl->action_);
    dlg.add_button(M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
    dlg.add_button(M(pimpl->action_ == Gtk::FILE_CHOOSER_ACTION_SAVE ? "GENERAL_SAVE" : "GENERAL_OPEN"), Gtk::RESPONSE_OK);
    dlg.set_filename(pimpl->filename_);
    for (auto &f : pimpl->file_filters_) {
        dlg.add_filter(f);
    }
    if (pimpl->cur_filter_) {
        dlg.set_filter(pimpl->cur_filter_);
    }
    for (auto &f : pimpl->shortcut_folders_) {
        dlg.add_shortcut_folder(f);
    }
    if (!pimpl->current_folder_.empty()) {
        dlg.set_current_folder(pimpl->current_folder_);
    }
    dlg.set_show_hidden(pimpl->show_hidden_);
    int res = dlg.run();
    if (res == Gtk::RESPONSE_OK) {
        pimpl->filename_ = dlg.get_filename();
        pimpl->current_folder_ = dlg.get_current_folder();
        on_filename_set();
        pimpl->selection_changed_.emit();
    }
}


void MyFileChooserWidget::on_filename_set()
{
    // Sub-classes decide if anything needs to be done.
}


sigc::signal<void> &MyFileChooserWidget::signal_selection_changed()
{
    return pimpl->selection_changed_;
}


sigc::signal<void> &MyFileChooserWidget::signal_file_set()
{
    return pimpl->selection_changed_;
}


std::string MyFileChooserWidget::get_filename() const
{
    return pimpl->filename_;
}


bool MyFileChooserWidget::set_filename(const std::string &filename)
{
    pimpl->filename_ = filename;
    on_filename_set();
    return true;
}


void MyFileChooserWidget::add_filter(const Glib::RefPtr<Gtk::FileFilter> &filter)
{
    pimpl->file_filters_.push_back(filter);
}


void MyFileChooserWidget::remove_filter(const Glib::RefPtr<Gtk::FileFilter> &filter)
{
    auto it = std::find(pimpl->file_filters_.begin(), pimpl->file_filters_.end(), filter);
    if (it != pimpl->file_filters_.end()) {
        pimpl->file_filters_.erase(it);
    }
}


void MyFileChooserWidget::set_filter(const Glib::RefPtr<Gtk::FileFilter> &filter)
{
    pimpl->cur_filter_ = filter;
}


std::vector<Glib::RefPtr<Gtk::FileFilter>> MyFileChooserWidget::list_filters() const
{
    return pimpl->file_filters_;
}


bool MyFileChooserWidget::set_current_folder(const std::string &filename)
{
    pimpl->current_folder_ = filename;
    if (pimpl->action_ == Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER) {
        set_filename(filename);
    }
    return true;
}

std::string MyFileChooserWidget::get_current_folder() const
{
    return pimpl->current_folder_;
}


bool MyFileChooserWidget::add_shortcut_folder(const std::string &folder)
{
    pimpl->shortcut_folders_.push_back(folder);
    return true;
}


bool MyFileChooserWidget::remove_shortcut_folder(const std::string &folder)
{
    auto it = std::find(pimpl->shortcut_folders_.begin(), pimpl->shortcut_folders_.end(), folder);
    if (it != pimpl->shortcut_folders_.end()) {
        pimpl->shortcut_folders_.erase(it);
    }
    return true;
}


void MyFileChooserWidget::unselect_all()
{
    pimpl->filename_ = "";
    on_filename_set();
}


void MyFileChooserWidget::unselect_filename(const std::string &filename)
{
    if (pimpl->filename_ == filename) {
        unselect_all();
    }
}


void MyFileChooserWidget::set_show_hidden(bool yes)
{
    pimpl->show_hidden_ = yes;
}


class MyFileChooserButton::Impl
{
public:
    Gtk::Box box_;
    Gtk::Label lbl_{"", Gtk::ALIGN_START};
};

MyFileChooserButton::MyFileChooserButton(const Glib::ustring &title, Gtk::FileChooserAction action):
    MyFileChooserWidget(title, action),
    pimpl(new Impl())
{
    pimpl->lbl_.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);
    pimpl->lbl_.set_justify(Gtk::JUSTIFY_LEFT);
    on_filename_set();
    pimpl->box_.pack_start(pimpl->lbl_, true, true);
    pimpl->box_.pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL)), false, false, 5);
    pimpl->box_.pack_start(*Gtk::manage(make_folder_image().release()), false, false);
    pimpl->box_.show_all_children();
    add(pimpl->box_);
    signal_clicked().connect([this]() {
        show_chooser(this);
    });

    if (GTK_MINOR_VERSION < 20) {
        set_border_width(2); // margin doesn't work on GTK < 3.20
    }

    set_name("MyFileChooserButton");
}

void MyFileChooserButton::on_filename_set()
{
    if (Glib::file_test(get_filename(), Glib::FILE_TEST_EXISTS)) {
        pimpl->lbl_.set_label(Glib::path_get_basename(get_filename()));
    } else {
        pimpl->lbl_.set_label(Glib::ustring("(") + M("GENERAL_NONE") + ")");
    }
}


// For an unknown reason (a bug ?), it doesn't work when action = FILE_CHOOSER_ACTION_SELECT_FOLDER !
bool MyFileChooserButton::on_scroll_event (GdkEventScroll* event)
{

    // If Shift is pressed, the widget is modified
    if (event->state & GDK_SHIFT_MASK) {
        Gtk::Button::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

void MyFileChooserButton::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    minimum_width = natural_width = RTScalable::scalePixelSize(35);
}

void MyFileChooserButton::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    minimum_width = natural_width = RTScalable::scalePixelSize(35);
}


class MyFileChooserEntry::Impl
{
public:
    Gtk::Entry entry;
    Gtk::Button file_chooser_button;
};


MyFileChooserEntry::MyFileChooserEntry(const Glib::ustring &title, Gtk::FileChooserAction action) :
    MyFileChooserWidget(title, action),
    pimpl(new Impl())
{
    const auto on_text_changed = [this]() {
        set_filename(pimpl->entry.get_text());
    };
    pimpl->entry.get_buffer()->signal_deleted_text().connect([on_text_changed](guint, guint) { on_text_changed(); });
    pimpl->entry.get_buffer()->signal_inserted_text().connect([on_text_changed](guint, const gchar *, guint) { on_text_changed(); });

    pimpl->file_chooser_button.set_image(*Gtk::manage(make_folder_image().release()));
    pimpl->file_chooser_button.signal_clicked().connect([this]() {
        const auto &filename = get_filename();
        if (Glib::file_test(filename, Glib::FILE_TEST_IS_DIR)) {
            set_current_folder(filename);
        }
        show_chooser(this);
    });

    pack_start(pimpl->entry, true, true);
    pack_start(pimpl->file_chooser_button, false, false);
}


Glib::ustring MyFileChooserEntry::get_placeholder_text() const
{
    return pimpl->entry.get_placeholder_text();
}


void MyFileChooserEntry::set_placeholder_text(const Glib::ustring &text)
{
    pimpl->entry.set_placeholder_text(text);
}


void MyFileChooserEntry::on_filename_set()
{
    if (pimpl->entry.get_text() != get_filename()) {
        pimpl->entry.set_text(get_filename());
    }
}
