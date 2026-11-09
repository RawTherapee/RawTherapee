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

#include "mousegesture.h"

#include <gtkmm/gesturedrag.h>
#include <gtkmm/widget.h>

using namespace rt::canvas;

MouseGesture::MouseGesture() :
    Glib::Object(), m_widget(nullptr), m_state(GestureState::NONE)
{
}

MouseGesture::~MouseGesture() = default;

void MouseGesture::connect(Gtk::Widget* widget)
{
    m_widget = widget;

    // The ordering of event controllers is important! Event controllers are
    // called in the reverse order of when they are attached to the widget.
    //
    // Make sure GestureClick is called before GestureDrag so the order is:
    // - onButtonPress -> onDragBegin
    // - onMotion -> onDragUpdate
    // - onButtonRelease -> onDragEnd

    m_drag_controller = Gtk::GestureDrag::create(*widget);
    m_drag_controller->signal_drag_update().connect(
        sigc::mem_fun(*this, &MouseGesture::onDragUpdate));
    m_drag_controller->signal_drag_end().connect(
        sigc::mem_fun(*this, &MouseGesture::onDragEnd));
    // Enable drag controller for all mouse buttons
    m_drag_controller->set_button(0);

    m_click_controller = rt::gtk4::GestureClick::create(widget);
    m_click_controller->signal_pressed().connect(
        sigc::mem_fun(*this, &MouseGesture::onButtonPressed));
    m_click_controller->signal_released().connect(
        sigc::mem_fun(*this, &MouseGesture::onButtonReleased));
    // Enable click controller for all mouse buttons
    m_click_controller->set_button(0);

    m_motion_controller = rt::gtk4::EventControllerMotion::create(widget);
    m_motion_controller->signal_enter().connect(
        sigc::mem_fun(*this, &MouseGesture::onEnter));
    m_motion_controller->signal_motion().connect(
        sigc::mem_fun(*this, &MouseGesture::onMotion));
    m_motion_controller->signal_leave().connect(
        sigc::mem_fun(*this, &MouseGesture::onLeave));
}

void MouseGesture::onEnter(double x, double y)
{
    WidgetPoint pos{ WidgetScalar(x), WidgetScalar(y) };
    signal_enter.emit(pos);
}

void MouseGesture::onMotion(double x, double y)
{
    WidgetPoint pos{ WidgetScalar(x), WidgetScalar(y) };
    signal_motion.emit(pos);

    if (m_state == GestureState::NONE) {
        signal_free_motion.emit(pos);
    }
}

void MouseGesture::onLeave()
{
    signal_leave.emit();
}

void MouseGesture::onButtonPressed(int n_press, double x, double y)
{
    WidgetPoint pos{ WidgetScalar(x), WidgetScalar(y) };
    const guint button = m_click_controller->get_current_button();
    m_current_button = button;

    // Double+ clicks can't be drags
    if (n_press > 1) {
        m_current_button = button;
        signal_clicked.emit(n_press, pos);
        m_state = GestureState::NONE;
        return;
    }

    switch (m_state) {
        case GestureState::NONE:
            m_state = GestureState::PENDING;
            m_press_start = pos;
            signal_pending_press.emit(pos);
            break;
        case GestureState::PENDING:
        case GestureState::DRAG:
            // When pressing multiple mouse buttons at once, the drag
            // controller's drag end event is fired immediately.
            //
            // No press or release event is fired.
            break;
        default:
            break;
    }
}

void MouseGesture::onButtonReleased(int n_press, double x, double y)
{
    WidgetPoint pos{ WidgetScalar(x), WidgetScalar(y) };
    const guint button = m_click_controller->get_current_button();
    m_current_button = button;

    switch (m_state) {
        case GestureState::PENDING:
            signal_clicked.emit(n_press, pos);
            m_state = GestureState::NONE;
            break;
        case GestureState::DRAG:
            // When pressing multiple mouse buttons at once, the drag
            // controller's drag end event is fired immediately.
            //
            // No press or release event is fired.
            break;
        case GestureState::NONE:
        default:
            break;
    }
}

void MouseGesture::onDragUpdate(double dx, double dy)
{
    WidgetVec delta{ WidgetScalar(dx), WidgetScalar(dy) };
    const guint button = m_click_controller->get_current_button();
    m_current_button = button;

    switch (m_state) {
        case GestureState::PENDING:
        {
            WidgetPoint tmp = m_press_start + delta;
            int start_x = m_press_start.x.value();
            int start_y = m_press_start.y.value();
            int curr_x = tmp.x.value();
            int curr_y = tmp.y.value();

            // Use GTK's builtin drag distance threshold
            if (m_widget->drag_check_threshold(start_x, start_y, curr_x, curr_y)) {
                m_state = GestureState::DRAG;
                signal_drag_begin.emit(m_press_start);
            }
            break;
        }
        case GestureState::DRAG:
            signal_drag_update.emit(delta);
            break;
        case GestureState::NONE:
        default:
            break;
    }
}

void MouseGesture::onDragEnd(double dx, double dy)
{
    WidgetVec delta{ WidgetScalar(dx), WidgetScalar(dy) };
    const guint button = m_drag_controller->get_current_button();
    m_current_button = button;

    // When pressing multiple mouse buttons at once, the drag
    // controller's drag end event is fired immediately.
    //
    // No press or release event is fired.
    switch (m_state) {
        case GestureState::DRAG:
            signal_drag_end.emit(delta);
            m_state = GestureState::NONE;
            break;
        case GestureState::PENDING:
            signal_cancel_press.emit(m_press_start + delta);
            m_state = GestureState::NONE;
            break;
        default:
            break;
    }
}
