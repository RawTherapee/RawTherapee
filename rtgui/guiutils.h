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

#include <functional>
#include <map>
#include <type_traits>

#include <gtkmm.h>

#include <cairomm/cairomm.h>

#include "threadutils.h"

#include "rtengine/coord.h"
#include "rtengine/noncopyable.h"

class Adjuster;
class RTImage;
class ToolPanel;

Glib::ustring escapeHtmlChars(const Glib::ustring &src);
bool removeIfThere (Gtk::Container* cont, Gtk::Widget* w, bool increference = true);
bool confirmOverwrite (Gtk::Window& parent, const std::string& filename);
void writeFailed (Gtk::Window& parent, const std::string& filename);
gboolean acquireGUI(void* data);
void setExpandAlignProperties(Gtk::Widget *widget, bool hExpand, bool vExpand, enum Gtk::Align hAlign, enum Gtk::Align vAlign);
Gtk::Border getPadding(const Glib::RefPtr<Gtk::StyleContext> style);

/**
 * @class IdleRegister
 * 
 * @brief A helper class for registering functions to be called asynchronously when there are no higher priority events pending.
 * Purpose of the IdleRegister is to make sure in-flight idle functions queued by `IdleRegister::add()` are unregistered and not
 * called after destruction.
 * 
 * Uses gdk_threads_add_idle_full
 * 
 * Notes:
 * It's best to call `IdleRegister::destroy()` in the destructor of the class owning the `IdleRegister` instance.
 * Otherwise make sure, it is the last member which will be deleted first.
 */
class IdleRegister final :
    public rtengine::NonCopyable
{
public:
    ~IdleRegister();

    /**
     * Registers a function to be called from the GTK event main loop later when there are no higher priority events pending.
     * If the registered function returns false, it is automatically cleared from the list of event sources and will not be called again.
     */
    void add(std::function<bool ()> function, gint priority = G_PRIORITY_DEFAULT_IDLE);
    void destroy();

private:
    struct DataWrapper {
        IdleRegister* const self;
        std::function<bool ()> function;
    };

    std::map<const DataWrapper*, guint> ids;
    MyMutex mutex;
};

struct ScopedEnumHash {
    template<typename T, typename std::enable_if<std::is_enum<T>::value && !std::is_convertible<T, int>::value, int>::type = 0>
    size_t operator ()(T val) const noexcept
    {
        using type = typename std::underlying_type<T>::type;

        return std::hash<type>{}(static_cast<type>(val));
    }
};


// TODO: The documentation says gdk_threads_enter and gdk_threads_leave should be replaced
// by g_main_context_invoke(), g_idle_add() and related functions, but this will require more extensive changes.
// We silence those warnings until then so that we notice the others.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/**
 * @brief Lock GTK for critical section.
 *
 * Will unlock on destruction. To use:
 *
 *   <code>
 *     {
 *       GThreadLock lock;
 *       // critical code
 *     }
 *   </code>
 */
class GThreadLock final
{
public:
    GThreadLock()
    {
        gdk_threads_enter();
    }
    ~GThreadLock()
    {
        gdk_threads_leave();
    }
};

/**
 * @brief Unlock GTK critical section.
 *
 * Will relock on destruction.
 */
class GThreadUnLock final
{
public:
    GThreadUnLock()
    {
        gdk_threads_leave();
    }
    ~GThreadUnLock()
    {
        gdk_threads_enter();
    }
};

#pragma GCC diagnostic pop

class ConnectionBlocker final
{
public:
    explicit ConnectionBlocker (Gtk::Widget *associatedWidget, sigc::connection& connection) : connection (associatedWidget ? &connection : nullptr), wasBlocked(false)
    {
        if (this->connection) {
            wasBlocked = connection.block();
        }
    }
    explicit ConnectionBlocker (sigc::connection& connection) : connection (&connection)
    {
            wasBlocked = connection.block();
    }
    ~ConnectionBlocker ()
    {
        if (connection) {
            connection->block(wasBlocked);
        }
    }
private:
    sigc::connection *connection;
    bool wasBlocked;
};

class BlockAdjusterEvents
{
public:
    explicit BlockAdjusterEvents(Adjuster* adjuster);
    ~BlockAdjusterEvents();

private:
    Adjuster* adj;
};

class DisableListener
{
public:
    explicit DisableListener(ToolPanel* panelToDisable);
    ~DisableListener();

private:
    ToolPanel* panel;
};

/**
 * @brief A helper method to connect the current folder property of a file chooser to an arbitrary variable.
 */
template <class FileChooser>
void bindCurrentFolder (FileChooser& chooser, Glib::ustring& variable)
{
    chooser.signal_selection_changed ().connect ([&]()
    {
        const auto current_folder = chooser.get_current_folder ();

        if (!current_folder.empty ())
            variable = current_folder;
    });

    if (!variable.empty ())
        chooser.set_current_folder (variable);
}

typedef enum RTUpdatePolicy {
    RTUP_STATIC,
    RTUP_DYNAMIC
} eUpdatePolicy;

typedef enum RTOrientation {
    RTO_Left2Right,
    RTO_Bottom2Top,
    RTO_Right2Left,
    RTO_Top2Bottom
} eRTOrientation;

typedef enum RTNav {
    NAV_NONE,
    NAV_NEXT,
    NAV_PREVIOUS
} eRTNav;

/**
 * @brief Handle the switch between text and image to be displayed in the HBox (to be used in a button/toolpanel)
 */
class TextOrIcon final : public Gtk::Box
{

public:
    TextOrIcon (const Glib::ustring &icon_name, const Glib::ustring &labelTx, const Glib::ustring &tooltipTx);
};

/**
 * Widget with image and label placed horizontally.
 */
class ImageAndLabel final : public Gtk::Box
{
    class Impl;
    std::unique_ptr<Impl> pimpl;

public:
    ImageAndLabel(const Glib::ustring& label, const Glib::ustring& iconName);
    ImageAndLabel(const Glib::ustring& label, RTImage* image);
    const RTImage* getImage() const;
    const Gtk::Label* getLabel() const;
};

/**
 * Menu item with an image and label.
 */
class MyImageMenuItemInterface
{
public:
    virtual const Gtk::Label* getLabel() const = 0;
};

/**
 * Basic image menu item.
 */
class MyImageMenuItem final : public Gtk::MenuItem, public MyImageMenuItemInterface
{
    class Impl;
    std::unique_ptr<Impl> pimpl;

public:
    MyImageMenuItem (const Glib::ustring& label, const Glib::ustring& iconName);
    MyImageMenuItem (const Glib::ustring& label, RTImage* image);
    const RTImage *getImage () const;
    const Gtk::Label* getLabel() const override;
};

/**
 * Image menu item with radio selector.
 */
class MyRadioImageMenuItem final : public Gtk::RadioMenuItem, public MyImageMenuItemInterface
{
    class Impl;
    std::unique_ptr<Impl> pimpl;

public:
    MyRadioImageMenuItem(const Glib::ustring& label, RTImage* image, Gtk::RadioButton::Group& group);
    const Gtk::Label* getLabel() const override;
};

class MyProgressBar final : public Gtk::ProgressBar
{
private:
    int w;

    void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const override;
    void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const override;

public:
    explicit MyProgressBar(int width);
    MyProgressBar();

    void setPreferredWidth(int width);
};

/**
 * @brief Define a gradient milestone
 */
class GradientMilestone final
{
public:
    double position;
    double r;
    double g;
    double b;
    double a;

    GradientMilestone(double _p = 0., double _r = 0., double _g = 0., double _b = 0., double _a = 0.)
    {
        position = _p;
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }
};

/**
 * Enforces the rule that zero or one registered toggle button is enabled at any
 * given time.
 */
class OptionalRadioButtonGroup
{
    Gtk::ToggleButton *active_button{nullptr};

    void onButtonToggled(Gtk::ToggleButton *button);

public:
    /**
     * Returns the toggle button that is active, or null if none are active.
     */
    Gtk::ToggleButton *getActiveButton() const;
    /**
     * Adds a toggle button to this group.
     *
     * If the provided button is active, any existing active button in this
     * group will be deactivated.
     */
    void register_button(Gtk::ToggleButton &button);
};

inline void setActiveTextOrIndex(Gtk::ComboBoxText &comboBox, const Glib::ustring &text, int index)
{
    bool valueSet = false;
    if (!text.empty()) {
        comboBox.set_active_text (text);
        valueSet = true;
    }

    if (!valueSet || comboBox.get_active_row_number () < 0) {
        comboBox.set_active (index);
    }
}

inline Gtk::Window& getToplevelWindow (Gtk::Widget* widget)
{
    return *static_cast<Gtk::Window*> (widget->get_toplevel ());
}
