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

#include "gtk4.h"

#include <glibmm/main.h>
#include <gtkmm/widget.h>

#ifdef GDK_WINDOWING_QUARTZ
    #include <gdk/gdkquartz.h>
#endif

#include <cmath>

namespace rt {
namespace gtk4 {

Glib::RefPtr<GestureClick> GestureClick::create(Gtk::Widget* widget)
{
    return Glib::RefPtr<GestureClick>(new GestureClick(widget));
}

GestureClick::GestureClick(Gtk::Widget* widget)
    : Glib::ObjectBase("RtGestureClick"),
      m_controller(gtk_gesture_multi_press_new(GTK_WIDGET(widget->gobj())))
{
    g_signal_connect(GTK_GESTURE_MULTI_PRESS(m_controller), "pressed",
                     G_CALLBACK(&GestureClick::hook_pressed), this);
    g_signal_connect(GTK_GESTURE_MULTI_PRESS(m_controller), "released",
                     G_CALLBACK(&GestureClick::hook_released), this);
    g_signal_connect(GTK_GESTURE_MULTI_PRESS(m_controller), "stopped",
                     G_CALLBACK(&GestureClick::hook_stopped), this);
}

GestureClick::~GestureClick()
{
    g_object_unref(m_controller);
    m_controller = nullptr;
}

GtkGestureMultiPress* GestureClick::gobj()
{
    return GTK_GESTURE_MULTI_PRESS(m_controller);
}

GtkPropagationPhase GestureClick::get_propagation_phase() const
{
    return gtk_event_controller_get_propagation_phase(
        GTK_EVENT_CONTROLLER(m_controller));
}

void GestureClick::set_propagation_phase(GtkPropagationPhase phase)
{
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(m_controller), phase);
}

unsigned int GestureClick::get_current_button() const
{
    return gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(m_controller));
}

unsigned int GestureClick::get_button() const
{
    return gtk_gesture_single_get_button(GTK_GESTURE_SINGLE(m_controller));
}

void GestureClick::set_button(unsigned int button)
{
    return gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(m_controller), button);
}

void GestureClick::hook_pressed(GtkGestureMultiPress*, gint n_press, gdouble x,
                                gdouble y, gpointer user_data)
{
    static_cast<GestureClick*>(user_data)->m_pressed.emit(n_press, x, y);
}

void GestureClick::hook_released(GtkGestureMultiPress*, gint n_press, gdouble x,
                                 gdouble y, gpointer user_data)
{
    static_cast<GestureClick*>(user_data)->m_released.emit(n_press, x, y);
}

void GestureClick::hook_stopped(GtkGestureMultiPress*, gpointer user_data)
{
    static_cast<GestureClick*>(user_data)->m_stopped.emit();
}

Glib::RefPtr<EventControllerKey> EventControllerKey::create(Gtk::Widget* widget)
{
    return Glib::RefPtr<EventControllerKey>(new EventControllerKey(widget));
}

EventControllerKey::EventControllerKey(Gtk::Widget* widget)
    : Glib::ObjectBase("RtEventControllerKey"),
      m_controller(gtk_event_controller_key_new(GTK_WIDGET(widget->gobj())))
{
    g_signal_connect(GTK_EVENT_CONTROLLER_KEY(m_controller), "key-pressed",
                     G_CALLBACK(&EventControllerKey::hook_pressed), this);
    g_signal_connect(GTK_EVENT_CONTROLLER_KEY(m_controller), "key-released",
                     G_CALLBACK(&EventControllerKey::hook_released), this);
    g_signal_connect(GTK_EVENT_CONTROLLER_KEY(m_controller), "modifiers",
                     G_CALLBACK(&EventControllerKey::hook_modifiers), this);
}

EventControllerKey::~EventControllerKey()
{
    g_object_unref(m_controller);
    m_controller = nullptr;
}

GtkEventControllerKey* EventControllerKey::gobj()
{
    return GTK_EVENT_CONTROLLER_KEY(m_controller);
}

GtkPropagationPhase EventControllerKey::get_propagation_phase() const
{
    return gtk_event_controller_get_propagation_phase(
        GTK_EVENT_CONTROLLER(m_controller));
}

void EventControllerKey::set_propagation_phase(GtkPropagationPhase phase)
{
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(m_controller), phase);
}

bool EventControllerKey::hook_pressed(
    GtkEventControllerKey*, guint keyval, guint keycode, GdkModifierType state,
    gpointer user_data)
{
    return static_cast<EventControllerKey*>(user_data)
        ->m_pressed.emit(keyval, keycode, state);
}

void EventControllerKey::hook_released(
    GtkEventControllerKey*, guint keyval, guint keycode, GdkModifierType state,
    gpointer user_data)
{
    static_cast<EventControllerKey*>(user_data)->m_released.emit(keyval, keycode, state);
}

bool EventControllerKey::hook_modifiers(GtkEventControllerKey*, GdkModifierType state,
                                        gpointer user_data)
{
    return static_cast<EventControllerKey*>(user_data)->m_modifiers.emit(state);
}

Glib::RefPtr<EventControllerMotion> EventControllerMotion::create(Gtk::Widget* widget)
{
    return Glib::RefPtr<EventControllerMotion>(new EventControllerMotion(widget));
}

EventControllerMotion::EventControllerMotion(Gtk::Widget* widget)
    : Glib::ObjectBase("RtEventControllerMotion"),
      m_controller(gtk_event_controller_motion_new(GTK_WIDGET(widget->gobj())))
{
    g_signal_connect(GTK_EVENT_CONTROLLER_MOTION(m_controller), "enter",
                     G_CALLBACK(&EventControllerMotion::hook_enter), this);
    g_signal_connect(GTK_EVENT_CONTROLLER_MOTION(m_controller), "motion",
                     G_CALLBACK(&EventControllerMotion::hook_motion), this);
    g_signal_connect(GTK_EVENT_CONTROLLER_MOTION(m_controller), "leave",
                     G_CALLBACK(&EventControllerMotion::hook_leave), this);
}

EventControllerMotion::~EventControllerMotion()
{
    g_object_unref(m_controller);
    m_controller = nullptr;
}

GtkEventControllerMotion* EventControllerMotion::gobj()
{
    return GTK_EVENT_CONTROLLER_MOTION(m_controller);
}

GtkPropagationPhase EventControllerMotion::get_propagation_phase() const
{
    return gtk_event_controller_get_propagation_phase(
        GTK_EVENT_CONTROLLER(m_controller));
}

void EventControllerMotion::set_propagation_phase(GtkPropagationPhase phase)
{
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(m_controller), phase);
}

void EventControllerMotion::hook_enter(GtkEventControllerMotion*, gdouble x, gdouble y,
                                       gpointer user_data)
{
    static_cast<EventControllerMotion*>(user_data)->m_enter.emit(x, y);
}

void EventControllerMotion::hook_motion(GtkEventControllerMotion*, gdouble x, gdouble y,
                                        gpointer user_data)
{
    static_cast<EventControllerMotion*>(user_data)->m_motion.emit(x, y);
}

void EventControllerMotion::hook_leave(GtkEventControllerMotion*, gpointer user_data)
{
    static_cast<EventControllerMotion*>(user_data)->m_leave.emit();
}

EventControllerScroll::EventControllerScroll(Flags flags)
    : Glib::ObjectBase("RtEventControllerScroll"),
      m_flags(flags),
      m_scroll_unit(ScrollUnit::WHEEL),
      m_dx_accum(0),
      m_dy_accum(0),
      m_is_active(false)
{
}

void EventControllerScroll::beginSmoothScrollIfNeeded()
{
    if (m_is_active) return;

    m_signal_begin.emit();
    m_is_active = true;
}

void EventControllerScroll::refreshTimeout()
{
    if (m_timeout.connected()) {
        m_timeout.disconnect();
    }

    constexpr unsigned int TIMEOUT_MS = 100;
    m_timeout = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &EventControllerScroll::onTimeout), TIMEOUT_MS);
}

bool EventControllerScroll::onTimeout()
{
    m_signal_end.emit();
    m_is_active = false;
    m_dx_accum = 0;
    m_dy_accum = 0;
    return false;
}

void EventControllerScroll::endScroll()
{
    m_timeout.disconnect();
    m_signal_end.emit();
    m_is_active = false;
    m_dx_accum = 0;
    m_dy_accum = 0;
}

void EventControllerScroll::populateDeltasByDirection(
    GdkScrollDirection dir, double& dx, double& dy)
{
    switch (dir) {
        case GDK_SCROLL_UP:
            dx = 0;
            dy = -1;
            break;
        case GDK_SCROLL_DOWN:
            dx = 0;
            dy = 1;
            break;
        case GDK_SCROLL_LEFT:
            dx = -1;
            dy = 0;
            break;
        case GDK_SCROLL_RIGHT:
            dx = 1;
            dy = 0;
            break;
        default:
            dx = 0;
            dy = 0;
            break;
    }

    filterDeltas(dx, dy);
}

void EventControllerScroll::filterDeltas(double& dx, double& dy)
{
    if (rt::none(m_flags & Flags::VERTICAL)) dy = 0;
    if (rt::none(m_flags & Flags::HORIZONTAL)) dx = 0;
}

bool EventControllerScroll::onEvent(GdkEvent* event)
{
    if (gdk_event_get_event_type(event) != GDK_SCROLL) return false;
    if (rt::none(m_flags & Flags::BOTH_AXES)) return false;

    // clang-format off
#ifdef GDK_WINDOWING_QUARTZ
    if (GDK_IS_QUARTZ_DISPLAY(gdk_display_get_default())) {
        return handleEventMacOS(event);
    }
    else
#endif
    {
        return handleEvent(event);
    }
    // clang-format on
}

// Reference: https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/quartz/gdkevents-quartz.c
//
// Implementation based on gtk-3-24 gdkevents-quartz.c fill_scroll_event()
bool EventControllerScroll::handleEventMacOS(const GdkEvent* event)
{
    auto scroll_event = reinterpret_cast<const GdkEventScroll*>(event);

    double dx = 0;
    double dy = 0;

    if (scroll_event->direction == GDK_SCROLL_SMOOTH) {
        // Touchpad and Magic Mouse send smooth scroll events
        m_scroll_unit = ScrollUnit::SURFACE;

        beginSmoothScrollIfNeeded();

        dx = scroll_event->delta_x;
        dy = scroll_event->delta_y;

        filterDeltas(dx, dy);
    } else {
        // Normal mice send discrete scroll events
        m_scroll_unit = ScrollUnit::WHEEL;

        populateDeltasByDirection(scroll_event->direction, dx, dy);
        // In MacOS, the deltas are absolute values for discrete mouse wheel
        // detents. These values represent the "speed" of a scroll. If a user
        // scrolls quickly, MacOS interprets that as the user wanting to scroll
        // more in a single detent.
        dx *= scroll_event->delta_x;
        dy *= scroll_event->delta_y;
    }

    bool handled = false;

    if (dx != 0 || dy != 0) {
        handled = m_signal_scroll.emit(dx, dy);
    }

    // Touchpad will always have a stop event
    if (m_is_active && scroll_event->is_stop) {
        endScroll();
        // Allow scroll stop to propagate up widget hierarchy
        handled = false;
    }

    return handled;
}

bool EventControllerScroll::handleEvent(const GdkEvent* event)
{
    auto scroll_event = reinterpret_cast<const GdkEventScroll*>(event);

    double dx = 0;
    double dy = 0;

    if (scroll_event->direction == GDK_SCROLL_SMOOTH) {
        m_scroll_unit = ScrollUnit::SURFACE;

        // Since mouse wheel detents are bunched with touchpad smooth scrolls
        // and they don't emit scroll stop events, m_is_active can get stuck
        // at true. Add a timeout to properly reset m_is_active.
        beginSmoothScrollIfNeeded();
        refreshTimeout();

        dx = scroll_event->delta_x;
        dy = scroll_event->delta_y;

        filterDeltas(dx, dy);
    } else {
        m_scroll_unit = ScrollUnit::WHEEL;

        populateDeltasByDirection(scroll_event->direction, dx, dy);
    }

    bool handled = false;

    if (dx != 0 || dy != 0) {
        handled = m_signal_scroll.emit(dx, dy);
    }

    // Propagate the stop even if the scroll was deactivated by timeout
    if (scroll_event->is_stop) {
        if (m_is_active) {
            endScroll();
        }
        // Allow scroll stop to propagate up widget hierarchy
        handled = false;
    }

    return handled;
}

}  // namespace gtk4
}  // namespace rt
