/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "coord.h"

#include "gtk4.h"

#include <glibmm/object.h>
#include <glibmm/refptr.h>
#include <sigc++/sigc++.h>

namespace Gtk {

class GestureDrag;
class Widget;

}  // namespace Gtk

namespace rt {
namespace canvas {

enum class GestureState { NONE, PENDING, CLICK, DRAG };

/**
 * A wrapper around GestureClick, GestureDrag, and EventControllerMotion that
 * sanitizes the various events such that button press/release events are
 * mutually exclusive with drag begin/update/end events.
 *
 * See the exposed signals for details on the sanitized version of events.
 *
 * As with the underlying GestureClick/Drag controllers, this does not support
 * multiple buttons being pressed at the same time.
 */
class MouseGesture : public Glib::Object
{
public:
    MouseGesture();
    ~MouseGesture();

    // Connect event controllers to widget
    void connect(Gtk::Widget* widget);

    rt::gtk4::EventControllerMotion* motionController() const
    {
        return m_motion_controller.get();
    }
    rt::gtk4::GestureClick* clickGesture() const { return m_click_controller.get(); }
    Gtk::GestureDrag* dragGesture() const { return m_drag_controller.get(); }

    guint get_current_button() const { return m_current_button; }

    // Same semantics as GTK event controllers
    sigc::signal<void(WidgetPoint)> signal_enter;
    sigc::signal<void(WidgetPoint)> signal_motion;
    sigc::signal<void()> signal_leave;

    sigc::signal<void(WidgetPoint)> signal_drag_begin;
    sigc::signal<void(WidgetVec)> signal_drag_update;
    sigc::signal<void(WidgetVec)> signal_drag_end;

    // --- Sanitized events ---

    /**
     * Like the Gtk.EventControllerMotion::motion event, but only fires when
     * no buttons are held down. When buttons are held down, signal_drag_update()
     * is emitted instead.
     */
    sigc::signal<void(WidgetPoint)> signal_free_motion;
    /**
     * A transient event indicating that a button was pressed, but we have not
     * yet resolved the press into a "click" or a "drag". Double/multi clicks
     * do not trigger this event.
     *
     * This is useful mostly for updating cursors without lag.
     */
    sigc::signal<void(WidgetPoint)> signal_pending_press;
    /**
     * User clicked the given location (i.e. button press then released).
     */
    sigc::signal<void(int, WidgetPoint)> signal_clicked;
    /**
     * A potential click was interrupted by another button begin pressed. Since
     * we only support one button at a time, the interaction is cancelled.
     */
    sigc::signal<void(WidgetPoint)> signal_cancel_press;

private:
    void onButtonPressed(int n_press, double x, double y);
    void onButtonReleased(int n_press, double x, double y);
    void onEnter(double x, double y);
    void onMotion(double x, double y);
    void onLeave();
    void onDragUpdate(double dx, double dy);
    void onDragEnd(double dx, double dy);

    Glib::RefPtr<rt::gtk4::EventControllerMotion> m_motion_controller;
    Glib::RefPtr<rt::gtk4::GestureClick> m_click_controller;
    Glib::RefPtr<Gtk::GestureDrag> m_drag_controller;
    Gtk::Widget* m_widget;

    GestureState m_state;
    WidgetPoint m_press_start;
    guint m_current_button;
};

}  // namespace canvas
}  // namespace rt
