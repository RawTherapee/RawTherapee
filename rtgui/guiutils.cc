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

#include "guiutils.h"

#include "multilangmgr.h"
#include "options.h"
#include "rtimage.h"
#include "rtscalable.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"

#include "rtengine/rt_math.h"
#include "rtengine/utils.h"

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
