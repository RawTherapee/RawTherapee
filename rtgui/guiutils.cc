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
