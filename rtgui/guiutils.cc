/*
 *  This file is part of RawTherapee.
 *
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
#include <cairomm/cairomm.h>
#include "rtengine/rt_math.h"

#include "guiutils.h"

#include "options.h"
#include "rtengine/utils.h"
#include "rtimage.h"
#include "rtscalable.h"
#include "multilangmgr.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"

#include <assert.h>

using namespace std;

IdleRegister::~IdleRegister()
{
    destroy();
}

void IdleRegister::add(std::function<bool ()> function, gint priority)
{
    const auto dispatch =
        [](gpointer data) -> gboolean
        {
            DataWrapper* const data_wrapper = static_cast<DataWrapper*>(data);

            if (!data_wrapper->function()) {
                data_wrapper->self->mutex.lock();
                data_wrapper->self->ids.erase(data_wrapper);
                data_wrapper->self->mutex.unlock();

                delete data_wrapper;
                return FALSE;
            }

            return TRUE;
        };

    DataWrapper* const data_wrapper = new DataWrapper{
        this,
        std::move(function)
    };

    mutex.lock();
    ids[data_wrapper] = gdk_threads_add_idle_full(priority, dispatch, data_wrapper, nullptr);
    mutex.unlock();
}

void IdleRegister::destroy()
{
    mutex.lock();
    for (const auto& id : ids) {
        g_source_remove(id.second);
        delete id.first;
    }
    ids.clear();
    mutex.unlock();
}

BlockAdjusterEvents::BlockAdjusterEvents(Adjuster* adjuster) : adj(adjuster)
{
    if (adj) {
        adj->block(true);
    }
}

BlockAdjusterEvents::~BlockAdjusterEvents()
{
    if (adj) {
        adj->block(false);
    }
}

DisableListener::DisableListener(ToolPanel* panelToDisable) : panel(panelToDisable)
{
    if (panel) {
        panel->disableListener();
    }
}

DisableListener::~DisableListener()
{
    if (panel) {
        panel->enableListener();
    }
}

Glib::ustring escapeHtmlChars(const Glib::ustring &src)
{

    // Sources chars to be escaped
    static const Glib::ustring srcChar("&<>");

    // Destination strings, in the same order than the source
    static std::vector<Glib::ustring> dstChar(3);
    dstChar.at(0) = "&amp;";
    dstChar.at(1) = "&lt;";
    dstChar.at(2) = "&gt;";

    // Copying the original string, that will be modified
    Glib::ustring dst(src);

    // Iterating all chars of the copy of the source string
    for (size_t i = 0; i < dst.length();) {

        // Looking out if it's part of the characters to be escaped
        size_t pos = srcChar.find_first_of(dst.at(i), 0);

        if (pos != Glib::ustring::npos) {
            // If yes, replacing the char in the destination string
            dst.replace(i, 1, dstChar.at(pos));
            // ... and going forward  by the length of the new string
            i += dstChar.at(pos).length();
        } else {
            ++i;
        }
    }

    return dst;
}

void setExpandAlignProperties(Gtk::Widget *widget, bool hExpand, bool vExpand, enum Gtk::Align hAlign, enum Gtk::Align vAlign)
{
    widget->set_hexpand(hExpand);
    widget->set_vexpand(vExpand);
    widget->set_halign(hAlign);
    widget->set_valign(vAlign);
}

Gtk::Border getPadding(const Glib::RefPtr<Gtk::StyleContext> style)
{
    Gtk::Border padding;
    if (!style) {
        return padding;
    }

    padding = style->get_padding();

    if (RTScalable::getGlobalScale() > 1.0) {
        // Scale pixel border size based on DPI and Scale
        padding.set_left(RTScalable::scalePixelSize(padding.get_left()));
        padding.set_right(RTScalable::scalePixelSize(padding.get_right()));
        padding.set_top(RTScalable::scalePixelSize(padding.get_top()));
        padding.set_bottom(RTScalable::scalePixelSize(padding.get_bottom()));
    }

    return padding;
}

bool removeIfThere (Gtk::Container* cont, Gtk::Widget* w, bool increference)
{

    Glib::ListHandle<Gtk::Widget*> list = cont->get_children ();
    Glib::ListHandle<Gtk::Widget*>::iterator i = list.begin ();

    for (; i != list.end() && *i != w; ++i);

    if (i != list.end()) {
        if (increference) {
            w->reference ();
        }

        cont->remove (*w);
        return true;
    } else {
        return false;
    }
}

bool confirmOverwrite (Gtk::Window& parent, const std::string& filename)
{
    bool safe = true;

    if (Glib::file_test (filename, Glib::FILE_TEST_EXISTS)) {
        Glib::ustring msg_ = Glib::ustring ("<b>\"") + escapeHtmlChars(Glib::path_get_basename (filename)) + "\": "
                             + M("MAIN_MSG_ALREADYEXISTS") + "</b>\n" + M("MAIN_MSG_QOVERWRITE");
        Gtk::MessageDialog msgd (parent, msg_, true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
        safe = (msgd.run () == Gtk::RESPONSE_YES);
    }

    return safe;
}

void writeFailed (Gtk::Window& parent, const std::string& filename)
{
    Glib::ustring msg_ = Glib::ustring::compose(M("MAIN_MSG_WRITEFAILED"), escapeHtmlChars(filename));
    Gtk::MessageDialog msgd (parent, msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
    msgd.run ();
}

/*
 *
 * Derived class of some widgets to properly handle the scroll wheel ;
 * the user has to use the Shift key to be able to change the widget's value,
 * otherwise the mouse wheel will scroll the editor's tabs content.
 *
 */
MyScrolledWindow::MyScrolledWindow ()
{
}

bool MyScrolledWindow::on_scroll_event (GdkEventScroll* event)
{
    if (!App::get().options().hideTPVScrollbar) {
        Gtk::ScrolledWindow::on_scroll_event (event);
        return true;
    }

    Glib::RefPtr<Gtk::Adjustment> adjust = get_vadjustment();
    Gtk::Scrollbar *scroll = get_vscrollbar();

    if (adjust && scroll) {
        const double upperBound = adjust->get_upper();
        const double lowerBound = adjust->get_lower();
        double value = adjust->get_value();
        double step  = adjust->get_step_increment();

        if (event->direction == GDK_SCROLL_DOWN) {
            const double value2 = rtengine::min<double>(value + step, upperBound);

            if (value2 != value) {
                scroll->set_value(value2);
            }
        } else if (event->direction == GDK_SCROLL_UP) {
            const double value2 = rtengine::max<double>(value - step, lowerBound);

            if (value2 != value) {
                scroll->set_value(value2);
            }
        } else if (event->direction == GDK_SCROLL_SMOOTH) {
            const double value2 = rtengine::LIM<double>(value + event->delta_y * step, lowerBound, upperBound);

            if (value2 != value) {
                scroll->set_value(value2);
            }
        }
    }

    return true;
}

void MyScrolledWindow::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    natural_width = minimum_width = RTScalable::scalePixelSize(100);
}

void MyScrolledWindow::get_preferred_height_vfunc (int &minimum_height, int &natural_height) const
{
    natural_height = minimum_height = RTScalable::scalePixelSize(50);
}

void MyScrolledWindow::get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const
{
    natural_height = minimum_height = RTScalable::scalePixelSize(50);
}

/*
 *
 * Derived class of some widgets to properly handle the scroll wheel ;
 * the user has to use the Shift key to be able to change the widget's value,
 * otherwise the mouse wheel will scroll the toolbar.
 *
 */
MyScrolledToolbar::MyScrolledToolbar ()
{
    set_policy (Gtk::POLICY_EXTERNAL, Gtk::POLICY_NEVER);
    get_style_context()->add_class("scrollableToolbar");

    // Works fine with Gtk 3.22, but a custom made get_preferred_height had to be created as a workaround
    // taken from the official Gtk3.22 source code
    //set_propagate_natural_height(true);
}

bool MyScrolledToolbar::on_scroll_event (GdkEventScroll* event)
{
    Glib::RefPtr<Gtk::Adjustment> adjust = get_hadjustment();
    Gtk::Scrollbar *scroll = get_hscrollbar();

    if (adjust && scroll) {
        const double upperBound = adjust->get_upper();
        const double lowerBound = adjust->get_lower();
        double value = adjust->get_value();
        double step  = adjust->get_step_increment() * 2;
        double value2 = 0.;

//        printf("MyScrolledToolbar::on_scroll_event / delta_x=%.5f, delta_y=%.5f, direction=%d, type=%d, send_event=%d\n",
//                event->delta_x, event->delta_y, (int)event->direction, (int)event->type, event->send_event);

        if (event->direction == GDK_SCROLL_DOWN) {
            value2 = rtengine::min<double>(value + step, upperBound);
            if (value2 != value) {
                scroll->set_value(value2);
            }
        } else if (event->direction == GDK_SCROLL_UP) {
            value2 = rtengine::max<double>(value - step, lowerBound);
            if (value2 != value) {
                scroll->set_value(value2);
            }
        } else if (event->direction == GDK_SCROLL_SMOOTH) {
            if (event->delta_x) {  // if the user use a pad, it can scroll horizontally
                value2 = rtengine::LIM<double>(value + (event->delta_x > 0 ? 30 : -30), lowerBound, upperBound);
            } else if (event->delta_y) {
                value2 = rtengine::LIM<double>(value + (event->delta_y > 0 ? 30 : -30), lowerBound, upperBound);
            }
            if (value2 != value) {
                scroll->set_value(value2);
            }
        }
    }

    return true;
}

void MyScrolledToolbar::get_preferred_height_vfunc (int &minimumHeight, int &naturalHeight) const
{
    int currMinHeight = 0;
    int currNatHeight = 0;
    std::vector<const Widget*> childs = get_children();
    minimumHeight = naturalHeight = 0;

    for (auto child : childs)
    {
        if(child->is_visible()) {
            child->get_preferred_height(currMinHeight, currNatHeight);
            minimumHeight = rtengine::max(currMinHeight, minimumHeight);
            naturalHeight = rtengine::max(currNatHeight, naturalHeight);
        }
    }
}

MyComboBoxText::MyComboBoxText (bool has_entry) : Gtk::ComboBoxText(has_entry)
{
    minimumWidth = naturalWidth = RTScalable::scalePixelSize(70);
    Gtk::CellRendererText* cellRenderer = dynamic_cast<Gtk::CellRendererText*>(get_first_cell());
    cellRenderer->property_ellipsize() = Pango::ELLIPSIZE_MIDDLE;
    add_events(Gdk::SCROLL_MASK|Gdk::SMOOTH_SCROLL_MASK);
}

bool MyComboBoxText::on_scroll_event (GdkEventScroll* event)
{

//    printf("MyComboboxText::on_scroll_event / delta_x=%.5f, delta_y=%.5f, direction=%d, type=%d, send_event=%d\n",
//            event->delta_x, event->delta_y, (int)event->direction, (int)event->type, event->send_event);
    // If Shift is pressed, the widget is modified
    if (event->state & GDK_SHIFT_MASK) {
        Gtk::ComboBoxText::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

void MyComboBoxText::setPreferredWidth (int minimum_width, int natural_width)
{
    if (natural_width == -1 && minimum_width == -1) {
        naturalWidth = minimumWidth = RTScalable::scalePixelSize(70);
    } else if (natural_width == -1) {
        naturalWidth =  minimumWidth = minimum_width;
    } else if (minimum_width == -1) {
        naturalWidth = natural_width;
        minimumWidth = rtengine::max(naturalWidth / 2, RTScalable::scalePixelSize(20));
        minimumWidth = rtengine::min(naturalWidth, minimumWidth);
    } else {
        naturalWidth = natural_width;
        minimumWidth = minimum_width;
    }
}

void MyComboBoxText::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    natural_width = rtengine::max(naturalWidth, RTScalable::scalePixelSize(10));
    minimum_width = rtengine::max(minimumWidth, RTScalable::scalePixelSize(10));
}

void MyComboBoxText::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    natural_width = rtengine::max(naturalWidth, RTScalable::scalePixelSize(10));
    minimum_width = rtengine::max(minimumWidth, RTScalable::scalePixelSize(10));
}


MyComboBox::MyComboBox ()
{
    minimumWidth = naturalWidth = RTScalable::scalePixelSize(70);
}

bool MyComboBox::on_scroll_event (GdkEventScroll* event)
{

    // If Shift is pressed, the widget is modified
    if (event->state & GDK_SHIFT_MASK) {
        Gtk::ComboBox::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

void MyComboBox::setPreferredWidth (int minimum_width, int natural_width)
{
    if (natural_width == -1 && minimum_width == -1) {
        naturalWidth = minimumWidth = RTScalable::scalePixelSize(70);
    } else if (natural_width == -1) {
        naturalWidth =  minimumWidth = minimum_width;
    } else if (minimum_width == -1) {
        naturalWidth = natural_width;
        minimumWidth = rtengine::max(naturalWidth / 2, RTScalable::scalePixelSize(20));
        minimumWidth = rtengine::min(naturalWidth, minimumWidth);
    } else {
        naturalWidth = natural_width;
        minimumWidth = minimum_width;
    }
}

void MyComboBox::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    natural_width = rtengine::max(naturalWidth, RTScalable::scalePixelSize(10));
    minimum_width = rtengine::max(minimumWidth, RTScalable::scalePixelSize(10));
}

void MyComboBox::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    natural_width = rtengine::max(naturalWidth, RTScalable::scalePixelSize(10));
    minimum_width = rtengine::max(minimumWidth, RTScalable::scalePixelSize(10));
}

MySpinButton::MySpinButton ()
{
    Gtk::Border border;
    border.set_bottom(0);
    border.set_top(0);
    border.set_left(3);
    border.set_right(3);
    set_inner_border(border);
    set_numeric(true);
    set_wrap(false);
    set_alignment(Gtk::ALIGN_END);
    set_update_policy(Gtk::SpinButtonUpdatePolicy::UPDATE_IF_VALID); // Avoid updating text if input is not a numeric
}

void MySpinButton::updateSize()
{
    double vMin, vMax;
    int maxAbs;
    unsigned int digits, digits2;
    unsigned int maxLen;

    get_range(vMin, vMax);

    digits = get_digits();
    maxAbs = (int)(fmax(fabs(vMin), fabs(vMax)) + 0.000001);

    if (maxAbs == 0) {
        digits2 = 1;
    } else {
        digits2 = (int)(log10(double(maxAbs)) + 0.000001);
        digits2++;
    }

    maxLen = digits + digits2 + (vMin < 0 ? 1 : 0) + (digits > 0 ? 1 : 0);
    set_max_length(maxLen);
    set_width_chars(maxLen);
    set_max_width_chars(maxLen);
}

bool MySpinButton::on_key_press_event (GdkEventKey* event)
{
    double vMin, vMax;
    get_range(vMin, vMax);

    if ((event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z)
            || (event->keyval >= GDK_KEY_A && event->keyval <= GDK_KEY_Z)
            || event->keyval == GDK_KEY_equal || event->keyval == GDK_KEY_underscore
            || event->keyval == GDK_KEY_plus || (event->keyval == GDK_KEY_minus && vMin >= 0)) {
        return false; // Event is propagated further
    } else {
        if (event->keyval == GDK_KEY_comma || event->keyval == GDK_KEY_KP_Decimal) {
            set_text(get_text() + ".");
            set_position(get_text().length()); // When setting text, cursor position is reset at text start. Avoiding this with this code
            return true; // Event is not propagated further
        }

        return Gtk::SpinButton::on_key_press_event(event); // Event is propagated normally
    }
}

bool MySpinButton::on_scroll_event (GdkEventScroll* event)
{
    // If Shift is pressed, the widget is modified
    if (event->state & GDK_SHIFT_MASK) {
        Gtk::SpinButton::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

bool MyHScale::on_scroll_event (GdkEventScroll* event)
{

//    printf("MyHScale::on_scroll_event / delta_x=%.5f, delta_y=%.5f, direction=%d, type=%d, send_event=%d\n",
//            event->delta_x, event->delta_y, (int)event->direction, (int)event->type, event->send_event);
    // If Shift is pressed, the widget is modified
    if (event->state & GDK_SHIFT_MASK) {
        Gtk::Scale::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

bool MyHScale::on_key_press_event (GdkEventKey* event)
{

    if ( event->string[0] == '+' || event->string[0] == '-' ) {
        return false;
    } else {
        return Gtk::Widget::on_key_press_event(event);
    }
}

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


TextOrIcon::TextOrIcon (const Glib::ustring &icon_name, const Glib::ustring &labelTx, const Glib::ustring &tooltipTx)
{

    RTImage *img = Gtk::manage(new RTImage(icon_name, Gtk::ICON_SIZE_LARGE_TOOLBAR));
    pack_start(*img, Gtk::PACK_SHRINK, 0);
    set_tooltip_markup("<span font_size=\"large\" font_weight=\"bold\">" + labelTx  + "</span>\n" + tooltipTx);

    set_name("TextOrIcon");
    show_all();

}

class ImageAndLabel::Impl
{
public:
    RTImage* image;
    Gtk::Label* label;

    Impl(RTImage* image, Gtk::Label* label) : image(image), label(label) {}
    static std::unique_ptr<RTImage> createImage(const Glib::ustring& iconName);
};

std::unique_ptr<RTImage> ImageAndLabel::Impl::createImage(const Glib::ustring& iconName)
{
    if (iconName.empty()) {
        return nullptr;
    }
    return std::unique_ptr<RTImage>(new RTImage(iconName, Gtk::ICON_SIZE_LARGE_TOOLBAR));
}

ImageAndLabel::ImageAndLabel(const Glib::ustring& label, const Glib::ustring& iconName) :
    ImageAndLabel(label, Gtk::manage(Impl::createImage(iconName).release()))
{
}

ImageAndLabel::ImageAndLabel(const Glib::ustring& label, RTImage *image) :
    pimpl(new Impl(image, Gtk::manage(new Gtk::Label(label))))
{
    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_orientation(Gtk::ORIENTATION_HORIZONTAL);

    if (image) {
        grid->attach_next_to(*image, Gtk::POS_LEFT, 1, 1);
    }

    grid->attach_next_to(*(pimpl->label), Gtk::POS_RIGHT, 1, 1);
    grid->set_column_spacing(4);
    grid->set_row_spacing(0);
    pack_start(*grid, Gtk::PACK_SHRINK, 0);
}

const RTImage* ImageAndLabel::getImage() const
{
    return pimpl->image;
}

const Gtk::Label* ImageAndLabel::getLabel() const
{
    return pimpl->label;
}

class MyImageMenuItem::Impl
{
private:
    std::unique_ptr<ImageAndLabel> widget;

public:
    Impl(const Glib::ustring &label, const Glib::ustring &iconName) :
        widget(new ImageAndLabel(label, iconName)) {}
    Impl(const Glib::ustring &label, RTImage *itemImage) :
        widget(new ImageAndLabel(label, itemImage)) {}
    ImageAndLabel* getWidget() const { return widget.get(); }
};

MyImageMenuItem::MyImageMenuItem(const Glib::ustring& label, const Glib::ustring& iconName) :
    pimpl(new Impl(label, iconName))
{
    add(*(pimpl->getWidget()));
}

MyImageMenuItem::MyImageMenuItem(const Glib::ustring& label, RTImage* itemImage) :
    pimpl(new Impl(label, itemImage))
{
    add(*(pimpl->getWidget()));
}

const RTImage *MyImageMenuItem::getImage () const
{
    return pimpl->getWidget()->getImage();
}

const Gtk::Label* MyImageMenuItem::getLabel () const
{
    return pimpl->getWidget()->getLabel();
}

class MyRadioImageMenuItem::Impl
{
    std::unique_ptr<ImageAndLabel> widget;

public:
    Impl(const Glib::ustring &label, RTImage *image) :
        widget(new ImageAndLabel(label, image)) {}
    ImageAndLabel* getWidget() const { return widget.get(); }
};

MyRadioImageMenuItem::MyRadioImageMenuItem(const Glib::ustring& label, RTImage *image, Gtk::RadioButton::Group& group) :
    Gtk::RadioMenuItem(group),
    pimpl(new Impl(label, image))
{
    add(*(pimpl->getWidget()));
}

const Gtk::Label* MyRadioImageMenuItem::getLabel() const
{
    return pimpl->getWidget()->getLabel();
}

MyProgressBar::MyProgressBar(int width) : w(rtengine::max(width, RTScalable::scalePixelSize(10))) {}
MyProgressBar::MyProgressBar() : w(RTScalable::scalePixelSize(200)) {}

void MyProgressBar::setPreferredWidth(int width)
{
    w = rtengine::max(width, RTScalable::scalePixelSize(10));
}

void MyProgressBar::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    minimum_width = rtengine::max(w / 2, RTScalable::scalePixelSize(50));
    natural_width = rtengine::max(w, RTScalable::scalePixelSize(50));
}

void MyProgressBar::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    get_preferred_width_vfunc (minimum_width, natural_width);
}

BackBuffer::BackBuffer() : x(0), y(0), w(0), h(0), offset(0, 0), dirty(true) {}
BackBuffer::BackBuffer(int width, int height, Cairo::Format format) : x(0), y(0), w(width), h(height), offset(0, 0), dirty(true)
{
    if (w > 0 && h > 0) {
        surface = Cairo::ImageSurface::create(format, w, h);
    } else {
        w = h = 0;
    }
}

void BackBuffer::setDestPosition(int x, int y)
{
    // values will be clamped when used...
    this->x = x;
    this->y = y;
}

void BackBuffer::setSrcOffset(int x, int y)
{
    // values will be clamped when used...
    offset.set(x, y);
}

void BackBuffer::setSrcOffset(const rtengine::Coord &newOffset)
{
    // values will be clamped when used...
    offset = newOffset;
}

void BackBuffer::getSrcOffset(int &x, int &y)
{
    // values will be clamped when used...
    offset.get(x, y);
}

void BackBuffer::getSrcOffset(rtengine::Coord &offset)
{
    // values will be clamped when used...
    offset = this->offset;
}

// Note: newW & newH must be > 0
bool BackBuffer::setDrawRectangle(Glib::RefPtr<Gdk::Window> window, Gdk::Rectangle &rectangle, bool updateBackBufferSize)
{
    return setDrawRectangle(window, rectangle.get_x(), rectangle.get_y(), rectangle.get_width(), rectangle.get_height(), updateBackBufferSize);
}

// Note: newW & newH must be > 0
bool BackBuffer::setDrawRectangle(Glib::RefPtr<Gdk::Window> window, int newX, int newY, int newW, int newH, bool updateBackBufferSize)
{
    assert(newW && newH);

    bool newSize = (newW > 0 && w != newW) || (newH > 0 && h != newH);

    x = newX;
    y = newY;
    if (newW > 0) {
        w = newW;
    }
    if (newH > 0) {
        h = newH;
    }

    // WARNING: we're assuming that the surface type won't change during all the execution time of RT. I guess it may be wrong when the user change the gfx card display settings!?
    if (((updateBackBufferSize && newSize) || !surface) && window) {
        // allocate a new Surface
        surface.clear();  // ... don't know if this is necessary?
        surface = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, w, h);
        dirty = true;
    }

    return dirty;
}

// Note: newW & newH must be > 0
bool BackBuffer::setDrawRectangle(Cairo::Format format, Gdk::Rectangle &rectangle, bool updateBackBufferSize)
{
    return setDrawRectangle(format, rectangle.get_x(), rectangle.get_y(), rectangle.get_width(), rectangle.get_height(), updateBackBufferSize);
}

// Note: newW & newH must be > 0
bool BackBuffer::setDrawRectangle(Cairo::Format format, int newX, int newY, int newW, int newH, bool updateBackBufferSize)
{
    assert(newW && newH);

    bool newSize = (newW > 0 && w != newW) || (newH > 0 && h != newH);

    x = newX;
    y = newY;
    if (newW > 0) {
        w = newW;
    }
    if (newH > 0) {
        h = newH;
    }

    // WARNING: we're assuming that the surface type won't change during all the execution time of RT. I guess it may be wrong when the user change the gfx card display settings!?
    if ((updateBackBufferSize && newSize) || !surface) {
        // allocate a new Surface
        surface.clear();  // ... don't know if this is necessary?
        surface = Cairo::ImageSurface::create(format, w, h);
        dirty = true;
    }

    return dirty;
}

/*
 * Copy uint8 RGB raw data to an ImageSurface. We're assuming that the source contains enough data for the given srcX, srcY, srcW, srcH -> no error checking!
 */
void BackBuffer::copyRGBCharData(const unsigned char *srcData, int srcX, int srcY, int srcW, int srcH, int srcRowStride, int dstX, int dstY)
{
    unsigned char r, g, b;

    if (!surface) {
        return;
    }

    //printf("copyRGBCharData:    src: (X:%d Y:%d, W:%d H:%d)  /  dst: (X: %d Y:%d)\n", srcX, srcY, srcW, srcH, dstX, dstY);

    unsigned char *dstData = surface->get_data();
    int surfW = surface->get_width();
    int surfH = surface->get_height();

    if (!srcData || dstX >= surfW || dstY >= surfH || srcW <= 0 || srcH <= 0 || srcX < 0 || srcY < 0) {
        return;
    }

    for (int i = 0; i < srcH; ++i) {
        if (dstY + i >= surfH) {
            break;
        }

        const unsigned char *src = srcData + i * srcRowStride;
        unsigned char *dst = dstData + ((dstY + i) * surfW + dstX) * 4;

        for (int j = 0; j < srcW; ++j) {
            if (dstX + j >= surfW) {
                break;
            }

            r = *(src++);
            g = *(src++);
            b = *(src++);

            rtengine::poke255_uc(dst, r, g, b);
        }
    }

    surface->mark_dirty();

}

/*
 * Copy the backbuffer to a Gdk::Window
 */
void BackBuffer::copySurface(Glib::RefPtr<Gdk::Window> window, Gdk::Rectangle *destRectangle)
{
    if (surface && window) {
        // TODO: look out if window can be different on each call, and if not, store a reference to the window
        Cairo::RefPtr<Cairo::Context> crSrc = window->create_cairo_context();
        Cairo::RefPtr<Cairo::Surface> destSurface = crSrc->get_target();

        // compute the source offset
        int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
        int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());

        // now copy the off-screen Surface to the destination Surface
        Cairo::RefPtr<Cairo::Context> crDest = Cairo::Context::create(destSurface);
        crDest->set_line_width(0.);

        if (destRectangle) {
            crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
            int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
            int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
            //printf("BackBuffer::copySurface / rectangle1(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            //printf("BackBuffer::copySurface / rectangle1\n");
        } else {
            crDest->set_source(surface, -offsetX + x, -offsetY + y);
            //printf("BackBuffer::copySurface / rectangle2(%d, %d, %d, %d)\n", x, y, w, h);
            crDest->rectangle(x, y, w, h);
            //printf("BackBuffer::copySurface / rectangle2\n");
        }

        crDest->fill();
    }
}

/*
 * Copy the BackBuffer to another BackBuffer
 */
void BackBuffer::copySurface(BackBuffer *destBackBuffer, Gdk::Rectangle *destRectangle)
{
    if (surface && destBackBuffer) {
        Cairo::RefPtr<Cairo::ImageSurface> destSurface = destBackBuffer->getSurface();

        if (!destSurface) {
            return;
        }

        // compute the source offset
        int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
        int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());

        // now copy the off-screen Surface to the destination Surface
        Cairo::RefPtr<Cairo::Context> crDest = Cairo::Context::create(destSurface);
        crDest->set_line_width(0.);

        if (destRectangle) {
            crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
            int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
            int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
            //printf("BackBuffer::copySurface / rectangle3(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            //printf("BackBuffer::copySurface / rectangle3\n");
        } else {
            crDest->set_source(surface, -offsetX + x, -offsetY + y);
            //printf("BackBuffer::copySurface / rectangle4(%d, %d, %d, %d)\n", x, y, w, h);
            crDest->rectangle(x, y, w, h);
            //printf("BackBuffer::copySurface / rectangle4\n");
        }

        crDest->fill();
    }
}

/*
 * Copy the BackBuffer to another Cairo::Surface
 */
void BackBuffer::copySurface(const Cairo::RefPtr<Cairo::ImageSurface>& destSurface,
                             Gdk::Rectangle *destRectangle)
{
    if (surface && destSurface) {
        // compute the source offset
        int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
        int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());

        // now copy the off-screen Surface to the destination Surface
        Cairo::RefPtr<Cairo::Context> crDest = Cairo::Context::create(destSurface);
        crDest->set_line_width(0.);

        if (destRectangle) {
            crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
            int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
            int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
            //printf("BackBuffer::copySurface / rectangle5(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            //printf("BackBuffer::copySurface / rectangle5\n");
        } else {
            crDest->set_source(surface, -offsetX + x, -offsetY + y);
            //printf("BackBuffer::copySurface / rectangle6(%d, %d, %d, %d)\n", x, y, w, h);
            crDest->rectangle(x, y, w, h);
            //printf("BackBuffer::copySurface / rectangle6\n");
        }

        crDest->fill();
    }
}

/*
 * Copy the BackBuffer to another Cairo::Surface
 */
void BackBuffer::copySurface(const Cairo::RefPtr<Cairo::Context>& crDest,
                             Gdk::Rectangle *destRectangle)
{
    if (surface && crDest) {
        // compute the source offset
        int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
        int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());

        // now copy the off-screen Surface to the destination Surface
        // int srcSurfW = surface->get_width();
        // int srcSurfH = surface->get_height();
        //printf("srcSurf:  w: %d, h: %d\n", srcSurfW, srcSurfH);
        crDest->set_line_width(0.);

        if (destRectangle) {
            crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
            int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
            int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
            //printf("BackBuffer::copySurface / rectangle7(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
            //printf("BackBuffer::copySurface / rectangle7\n");
        } else {
            crDest->set_source(surface, -offsetX + x, -offsetY + y);
            //printf("BackBuffer::copySurface / rectangle8(%d, %d, %d, %d)\n", x, y, w, h);
            crDest->rectangle(x, y, w, h);
            //printf("BackBuffer::copySurface / rectangle8\n");
        }

        crDest->fill();
    }
}

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

// OptionalRadioButtonGroup class

void OptionalRadioButtonGroup::onButtonToggled(Gtk::ToggleButton *button)
{
    if (!button) {
        return;
    }

    if (button->get_active()) {
        if (active_button == button) {
            // Same button, noting to do.
        } else if (active_button) {
            // Deactivate the other button.
            active_button->set_active(false);
        }
        active_button = button;
    } else {
        if (active_button == button) {
            // Active button got deactivated.
            active_button = nullptr;
        } else {
            // No effect on other buttons.
        }
    }
}

Gtk::ToggleButton *OptionalRadioButtonGroup::getActiveButton() const
{
    return active_button;
}

void OptionalRadioButtonGroup::register_button(Gtk::ToggleButton &button)
{
    button.signal_toggled().connect(sigc::bind(
        sigc::mem_fun(this, &OptionalRadioButtonGroup::onButtonToggled),
        &button));
    onButtonToggled(&button);
}
