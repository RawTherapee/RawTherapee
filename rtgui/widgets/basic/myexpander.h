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
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>

class RTImage;

/**
 * @brief Glue box to control visibility of the MyExpander's content ; also handle the frame around it
 */
class ExpanderBox final : public Gtk::EventBox
{
private:
    Gtk::Container *pC;

public:
    explicit ExpanderBox( Gtk::Container *p);
    ~ExpanderBox( ) override
    {
        delete pC;
    }

    void setLevel(int level);

    void show() {}
    void show_all();
    void hide() {}
    void set_visible(bool isVisible = true) {}

    void showBox();
    void hideBox();
};

/**
 * @brief A custom Expander class, that can handle widgets in the title bar
 *
 * Custom made expander for responsive widgets in the header. It also handle a "enabled/disabled" property that display
 * a different arrow depending on this boolean value.
 *
 * Warning: once you've instantiated this class with a text label or a widget label, you won't be able to revert to the other solution.
 */
class MyExpander final : public Gtk::Box
{
public:
    typedef sigc::signal<void> type_signal_enabled_toggled;
private:
    type_signal_enabled_toggled message;
    const Glib::ustring inconsistentImage; /// "inconsistent" image, displayed when useEnabled is true ; in this case, nothing will tell that an expander is opened/closed
    const Glib::ustring enabledImage;      ///      "enabled" image, displayed when useEnabled is true ; in this case, nothing will tell that an expander is opened/closed
    const Glib::ustring disabledImage;     ///     "disabled" image, displayed when useEnabled is true ; in this case, nothing will tell that an expander is opened/closed
    const Glib::ustring openedImage;       ///       "opened" image, displayed when useEnabled is false
    const Glib::ustring closedImage;       ///       "closed" image, displayed when useEnabled is false
    bool enabled;               /// Enabled feature (default to true)
    bool inconsistent;          /// True if the enabled button is inconsistent
    Gtk::EventBox *titleEvBox;  /// EventBox of the title, to get a connector from it
    Gtk::Box *headerHBox;
    bool flushEvent;            /// Flag to control the weird event mechanism of Gtk (please prove me wrong!)
    ExpanderBox* expBox;        /// Frame that includes the child and control its visibility
    Gtk::EventBox *imageEvBox;  /// Enable/Disable or Open/Close arrow event box

    using Gtk::Container::add;

    /// Triggered on opened/closed event
    bool on_toggle(GdkEventButton* event);
    /// Triggered on enabled/disabled change -> will emit a toggle event to the connected objects
    bool on_enabled_change(GdkEventButton* event);
    /// Used to handle the colored background for the whole Title
    bool on_enter_leave_title (GdkEventCrossing* event);
    /// Used to handle the colored background for the Enable button
    bool on_enter_leave_enable (GdkEventCrossing* event);

    void updateStyle();

protected:
    Gtk::Container* child;      /// Gtk::Contained to display below the expander's title
    Gtk::Widget* headerWidget;  /// Widget to display in the header, next to the arrow image ; can be NULL if the "string" version of the ctor has been used
    RTImage* statusImage;       /// Image to display the opened/closed status (if useEnabled is false) of the enabled/disabled status (if useEnabled is true)
    Gtk::Label* label;          /// Text to display in the header, next to the arrow image ; can be NULL if the "widget" version of the ctor has been used
    bool useEnabled;            /// Set whether to handle an enabled/disabled feature and display the appropriate images

public:

    /** @brief Create a custom expander with a simple header made of a label.
     * @param useEnabled Set whether to handle an enabled/disabled toggle button and display the appropriate image
     * @param titleLabel A string to display in the header. Warning: you won't be able to switch to a widget label.
     */
    MyExpander(bool useEnabled, Glib::ustring titleLabel);

    /** Create a custom expander with a custom - and responsive - widget
     * @param useEnabled Set whether to handle an enabled/disabled toggle button and display the appropriate image
     * @param titleWidget A widget to display in the header. Warning: you won't be able to switch to a string label.
     */
    MyExpander(bool useEnabled, Gtk::Widget* titleWidget);

    Glib::SignalProxy1< bool, GdkEventButton* > signal_button_release_event()
    {
        return titleEvBox->signal_button_release_event();
    };
    type_signal_enabled_toggled signal_enabled_toggled();

    /// Set the nesting level of the Expander to adapt its style accordingly
    void setLevel(int level);

    /// Set a new label string. If it has been instantiated with a Gtk::Widget, this method will do nothing
    void setLabel (Glib::ustring newLabel);
    /// Set a new label string. If it has been instantiated with a Gtk::Widget, this method will do nothing
    void setLabel (Gtk::Widget *newWidget);

    /// Get whether the enabled option is set (to true or false) or unset (i.e. undefined)
    bool get_inconsistent();
    /// Set whether the enabled option is set (to true or false) or unset (i.e. undefined)
    void set_inconsistent(bool isInconsistent);

    /// Get whether the enabled button is used or not
    bool getUseEnabled();
    /// Get whether the enabled button is on or off
    bool getEnabled();
    /// If not inconsistent, set the enabled button to true or false and emit the message if the state is different
    /// If inconsistent, set the internal value to true or false, but do not update the image and do not emit the message
    void setEnabled(bool isEnabled);
    /// Adds a tooltip to the header
    void setEnabledTooltipMarkup(const Glib::ustring& tooltipMarkup);
    void setEnabledTooltipText(const Glib::ustring& tooltipText);

    /// Get the header widget. It'll send back the Gtk::Label* if it has been instantiated with a simple text
    Gtk::Widget* getLabelWidget() const
    {
        return headerWidget ? headerWidget : label;
    }

    /// Set the collapsed/expanded state of the expander
    void set_expanded( bool expanded );

    /// Get the collapsed/expanded state of the expander
    bool get_expanded();

    /// Add a Gtk::Container for the content of the expander
    /// Warning: do not manually Show/Hide the widget, because this parameter is handled by the click on the Expander's title
    void add  (Gtk::Container& widget, bool setChild = true);

    void updateVScrollbars(bool hide);
};
