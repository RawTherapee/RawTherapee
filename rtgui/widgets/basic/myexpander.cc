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

#include "myexpander.h"

#include "guiutils.h"
#include "options.h"
#include "rtimage.h"

#include "rtengine/rtapp.h"

ExpanderBox::ExpanderBox( Gtk::Container *p): pC(p)
{
    set_name ("ExpanderBox");
}

void ExpanderBox::setLevel(int level)
{
    if (level <= 1) {
        set_name("ExpanderBox");
    } else if (level == 2) {
        set_name("ExpanderBox2");
    } else if (level >= 3) {
        set_name("ExpanderBox3");
    }
}

void ExpanderBox::show_all()
{
    // ask childs to show themselves, but not us (remain unchanged)
    Gtk::Container::show_all_children(true);
}

void ExpanderBox::showBox()
{
    Gtk::EventBox::show();
}

void ExpanderBox::hideBox()
{
    Gtk::EventBox::hide();
}

MyExpander::MyExpander(bool useEnabled, Gtk::Widget* titleWidget) :
    inconsistentImage("power-inconsistent-small"),
    enabledImage("power-on-small"),
    disabledImage("power-off-small"),
    openedImage("expander-open-small"),
    closedImage("expander-closed-small"),
    enabled(false), inconsistent(false), flushEvent(false), expBox(nullptr),
    child(nullptr), headerWidget(nullptr), statusImage(nullptr),
    label(nullptr), useEnabled(useEnabled)
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);
    set_spacing(0);
    set_name("MyExpander");
    set_can_focus(false);
    setExpandAlignProperties(this, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);

    headerHBox = Gtk::manage( new Gtk::Box());
    headerHBox->set_can_focus(false);
    setExpandAlignProperties(headerHBox, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);

    if (useEnabled) {
        get_style_context()->add_class("OnOff");
        statusImage = Gtk::manage(new RTImage(disabledImage));
        imageEvBox = Gtk::manage(new Gtk::EventBox());
        imageEvBox->set_name("MyExpanderStatus");
        imageEvBox->add(*statusImage);
        imageEvBox->set_above_child(true);
        imageEvBox->signal_button_release_event().connect( sigc::mem_fun(this, & MyExpander::on_enabled_change) );
        imageEvBox->signal_enter_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_enable), false );
        imageEvBox->signal_leave_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_enable), false );
        headerHBox->pack_start(*imageEvBox, Gtk::PACK_SHRINK, 0);
    } else {
        get_style_context()->add_class("Fold");
        statusImage = Gtk::manage(new RTImage(openedImage));
        headerHBox->pack_start(*statusImage, Gtk::PACK_SHRINK, 0);
    }

    statusImage->set_can_focus(false);

    if (titleWidget) {
        setExpandAlignProperties(titleWidget, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
        headerHBox->pack_start(*titleWidget, Gtk::PACK_EXPAND_WIDGET, 0);
        headerWidget = titleWidget;
    }

    titleEvBox = Gtk::manage(new Gtk::EventBox());
    titleEvBox->set_name("MyExpanderTitle");
    titleEvBox->set_border_width(0);
    titleEvBox->add(*headerHBox);
    titleEvBox->set_above_child(false);  // this is the key! By making it below the child, they will get the events first.
    titleEvBox->set_can_focus(false);

    pack_start(*titleEvBox, Gtk::PACK_EXPAND_WIDGET, 0);

    updateStyle();

    titleEvBox->signal_button_release_event().connect( sigc::mem_fun(this, & MyExpander::on_toggle) );
    titleEvBox->signal_enter_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_title), false);
    titleEvBox->signal_leave_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_title), false);
}

MyExpander::MyExpander(bool useEnabled, Glib::ustring titleLabel) :
    inconsistentImage("power-inconsistent-small"),
    enabledImage("power-on-small"),
    disabledImage("power-off-small"),
    openedImage("expander-open-small"),
    closedImage("expander-closed-small"),
    enabled(false), inconsistent(false), flushEvent(false), expBox(nullptr),
    child(nullptr), headerWidget(nullptr),
    label(nullptr), useEnabled(useEnabled)
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);
    set_spacing(0);
    set_name("MyExpander");
    set_can_focus(false);
    setExpandAlignProperties(this, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);

    headerHBox = Gtk::manage( new Gtk::Box());
    headerHBox->set_can_focus(false);
    setExpandAlignProperties(headerHBox, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);

    if (useEnabled) {
        get_style_context()->add_class("OnOff");
        statusImage = Gtk::manage(new RTImage(disabledImage));
        imageEvBox = Gtk::manage(new Gtk::EventBox());
        imageEvBox->set_name("MyExpanderStatus");
        imageEvBox->add(*statusImage);
        imageEvBox->set_above_child(true);
        imageEvBox->signal_button_release_event().connect( sigc::mem_fun(this, & MyExpander::on_enabled_change) );
        imageEvBox->signal_enter_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_enable), false );
        imageEvBox->signal_leave_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_enable), false );
        headerHBox->pack_start(*imageEvBox, Gtk::PACK_SHRINK, 0);
    } else {
        get_style_context()->add_class("Fold");
        statusImage = Gtk::manage(new RTImage(openedImage));
        headerHBox->pack_start(*statusImage, Gtk::PACK_SHRINK, 0);
    }

    statusImage->set_can_focus(false);

    label = Gtk::manage(new Gtk::Label());
    setExpandAlignProperties(label, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    label->set_markup(escapeHtmlChars(titleLabel));
    headerHBox->pack_start(*label, Gtk::PACK_EXPAND_WIDGET, 0);

    titleEvBox = Gtk::manage(new Gtk::EventBox());
    titleEvBox->set_name("MyExpanderTitle");
    titleEvBox->set_border_width(0);
    titleEvBox->add(*headerHBox);
    titleEvBox->set_above_child(false);  // this is the key! By make it below the child, they will get the events first.
    titleEvBox->set_can_focus(false);

    pack_start(*titleEvBox, Gtk::PACK_EXPAND_WIDGET, 0);

    updateStyle();

    titleEvBox->signal_button_release_event().connect( sigc::mem_fun(this, & MyExpander::on_toggle));
    titleEvBox->signal_enter_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_title), false);
    titleEvBox->signal_leave_notify_event().connect( sigc::mem_fun(this, & MyExpander::on_enter_leave_title), false);
}

bool MyExpander::on_enter_leave_title (GdkEventCrossing* event)
{
    if (is_sensitive()) {
        if (event->type == GDK_ENTER_NOTIFY) {
            titleEvBox->set_state(Gtk::STATE_PRELIGHT);
            queue_draw();
        } else if (event->type == GDK_LEAVE_NOTIFY) {
            titleEvBox->set_state(Gtk::STATE_NORMAL);
            queue_draw();
        }
    }

    return true;
}

bool MyExpander::on_enter_leave_enable (GdkEventCrossing* event)
{
    if (is_sensitive()) {
        if (event->type == GDK_ENTER_NOTIFY) {
            imageEvBox->set_state(Gtk::STATE_PRELIGHT);
            queue_draw();
        } else if (event->type == GDK_LEAVE_NOTIFY) {
            imageEvBox->set_state(Gtk::STATE_NORMAL);
            queue_draw();
        }
    }

    return true;
}

void MyExpander::updateStyle()
{
    updateVScrollbars(App::get().options().hideTPVScrollbar);
}

void MyExpander::updateVScrollbars(bool hide)
{
    if (hide) {
        get_style_context()->remove_class("withScrollbar");
    } else {
        get_style_context()->add_class("withScrollbar");
    }
}

void MyExpander::setLevel (int level)
{
    if (expBox) {
        expBox->setLevel(level);
    }
}

void MyExpander::setLabel (Glib::ustring newLabel)
{
    if (label) {
        label->set_markup(escapeHtmlChars(newLabel));
    }
}

void MyExpander::setLabel (Gtk::Widget *newWidget)
{
    if (headerWidget) {
        removeIfThere(headerHBox, headerWidget, false);
        headerHBox->pack_start(*newWidget, Gtk::PACK_EXPAND_WIDGET, 0);
    }
}

bool MyExpander::get_inconsistent()
{
    return inconsistent;
}

void MyExpander::set_inconsistent(bool isInconsistent)
{
    if (inconsistent != isInconsistent) {
        inconsistent = isInconsistent;

        if (useEnabled) {
            if (isInconsistent) {
                statusImage->set_from_icon_name(inconsistentImage);
            } else {
                if (enabled) {
                    statusImage->set_from_icon_name(enabledImage);
                    get_style_context()->add_class("enabledTool");
                } else {
                    statusImage->set_from_icon_name(disabledImage);
                    get_style_context()->remove_class("enabledTool");
                }
            }
        }

    }
}

bool MyExpander::getUseEnabled()
{
    return useEnabled;
}

bool MyExpander::getEnabled()
{
    return enabled;
}

void MyExpander::setEnabled(bool isEnabled)
{
    if (isEnabled != enabled) {
        if (useEnabled) {
            if (enabled) {
                enabled = false;

                if (!inconsistent) {
                    statusImage->set_from_icon_name(disabledImage);
                    get_style_context()->remove_class("enabledTool");
                    message.emit();
                }
            } else {
                enabled = true;

                if (!inconsistent) {
                    statusImage->set_from_icon_name(enabledImage);
                    get_style_context()->add_class("enabledTool");
                    message.emit();
                }
            }
        }
    }
}

void MyExpander::setEnabledTooltipMarkup(const Glib::ustring& tooltipMarkup)
{
    headerHBox->set_tooltip_markup(tooltipMarkup);
}

void MyExpander::setEnabledTooltipText(const Glib::ustring& tooltipText)
{
    headerHBox->set_tooltip_text(tooltipText);
}

void MyExpander::set_expanded( bool expanded )
{
    if (!expBox) {
        return;
    }

    if (!useEnabled) {
        if (expanded ) {
            statusImage->set_from_icon_name(openedImage);
        } else {
            statusImage->set_from_icon_name(closedImage);
        }
    }

    if (expanded) {
        expBox->showBox();
    } else {
        expBox->hideBox();
    }
}

bool MyExpander::get_expanded()
{
    return expBox ? expBox->get_visible() : false;
}

void MyExpander::add  (Gtk::Container& widget, bool setChild)
{
    if(setChild) {
        child = &widget;
    }
    expBox = Gtk::manage (new ExpanderBox (child));
    expBox->add (widget);
    pack_start(*expBox, Gtk::PACK_SHRINK, 0);
    widget.show();
    expBox->hideBox();
}

bool MyExpander::on_toggle(GdkEventButton* event)
{
    if (flushEvent) {
        flushEvent = false;
        return false;
    }

    if (!expBox || event->button != 1) {
        return false;
    }

    bool isVisible = expBox->is_visible();

    if (!useEnabled) {
        if (isVisible) {
            statusImage->set_from_icon_name(closedImage);
        } else {
            statusImage->set_from_icon_name(openedImage);
        }
    }

    if (isVisible) {
        expBox->hideBox();
    } else {
        expBox->showBox();
    }

    return false;
}

// used to connect a function to the enabled_toggled signal
MyExpander::type_signal_enabled_toggled MyExpander::signal_enabled_toggled()
{
    return message;
}

// internal use ; when the user clicks on the toggle button, it calls this method that will emit an enabled_change event
bool MyExpander::on_enabled_change(GdkEventButton* event)
{
    if (event->button == 1) {
        if (enabled) {
            enabled = false;
            statusImage->set_from_icon_name(disabledImage);
            get_style_context()->remove_class("enabledTool");
        } else {
            enabled = true;
            statusImage->set_from_icon_name(enabledImage);
            get_style_context()->add_class("enabledTool");
        }

        message.emit();
        flushEvent = true;
    }

    return false;
}
