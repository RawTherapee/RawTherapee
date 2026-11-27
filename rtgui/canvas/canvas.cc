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

#include "canvas.h"

#include "model.h"

#include "guiutils.h"
#include "rtscalable.h"

#include <cairomm/matrix.h>
#include <gdk/gdkkeysyms.h>
#include <gtkmm/gesturedrag.h>
#include <gtkmm/gesturezoom.h>

#ifdef GDK_WINDOWING_QUARTZ
#include <gdk/gdkquartz.h>
#endif

using namespace rt::canvas;

namespace {

using PanZoomFlags = Session::PanZoomFlags;
using ScrollUnit = rt::gtk4::ScrollUnit;

constexpr CursorShape PAN_CURSOR = CSHandClosed;
constexpr double NATURAL_ASPECT_RATIO = 16.0 / 9.0;
constexpr double ZOOM_FACTOR = 1.5;

GdkModifierType keyvalToModifier(guint keyval)
{
    switch (keyval) {
        // TODO(GTK4): Add GDK_META_MASK and replace GDK_MOD1_MASK -> GDK_ALT_MASK
        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
            return GDK_SHIFT_MASK;
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
            return GDK_CONTROL_MASK;
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
            return GDK_MOD1_MASK;
        default:
            return GdkModifierType(0);
    }
}

double mapSliderLog(int value,
                    int slider_min,
                    int slider_max,
                    double output_min,
                    double output_max)
{
    double t = (value - slider_min) / static_cast<double>(slider_max - slider_min);
    double log_min = std::log10(output_min);
    double log_max = std::log10(output_max);
    return std::pow(10.0, log_min + t * (log_max - log_min));
}

}  // namespace

Canvas::Canvas(CanvasModel* model) :
    Gtk::Widget(),
    m_renderer(nullptr),
    m_model(model),
    m_smooth_scroll_zoom_sensitivity(1),
    m_smooth_scroll_pan_sensitivity(1),
    m_scroll_zoom_accum(0),
    m_camera_zoom_begin(1),
    m_pan(PanningInput::NONE),
    m_scroll_mode(ScrollMode::ZOOM),
    m_is_pan_zoom_enabled(false),
    m_reverse_discrete_scroll_dir(false),
    m_reverse_smooth_scroll_dir(false),
    m_is_cursor_inside_canvas(false)
{
    set_name("RtCanvas");

    m_scroll_controller = std::make_unique<rt::gtk4::EventControllerScroll>(
        rt::gtk4::EventControllerScroll::Flags::BOTH_AXES);
    m_scroll_controller->signal_scroll_begin().connect(
        sigc::mem_fun(*this, &Canvas::onScrollBegin));
    m_scroll_controller->signal_scroll().connect(
        sigc::mem_fun(*this, &Canvas::onScrollChanged));

    m_zoom_controller = Gtk::GestureZoom::create(*this);
    m_zoom_controller->signal_begin().connect(sigc::mem_fun(*this, &Canvas::onZoomBegin));
    m_zoom_controller->signal_scale_changed().connect(
        sigc::mem_fun(*this, &Canvas::onZoomChanged));

    m_mouse_gesture.connect(this);

    m_mouse_gesture.signal_drag_begin.connect(sigc::mem_fun(*this, &Canvas::onDragBegin));
    m_mouse_gesture.signal_drag_update.connect(
        sigc::mem_fun(*this, &Canvas::onDragUpdate));
    m_mouse_gesture.signal_drag_end.connect(sigc::mem_fun(*this, &Canvas::onDragEnd));

    m_mouse_gesture.signal_pending_press.connect(
        sigc::mem_fun(*this, &Canvas::onPendingPress));
    m_mouse_gesture.signal_clicked.connect(sigc::mem_fun(*this, &Canvas::onClick));
    m_mouse_gesture.signal_cancel_press.connect(
        sigc::mem_fun(*this, &Canvas::onCancelPress));

    m_mouse_gesture.signal_enter.connect(sigc::mem_fun(*this, &Canvas::onEnter));
    m_mouse_gesture.signal_motion.connect(sigc::mem_fun(*this, &Canvas::onMotion));
    m_mouse_gesture.signal_free_motion.connect(
        sigc::mem_fun(*this, &Canvas::onFreeMotion));
    m_mouse_gesture.signal_leave.connect(sigc::mem_fun(*this, &Canvas::onLeave));

    property_scale_factor().signal_changed().connect(
        sigc::mem_fun(*this, &Canvas::onScaleFactorChanged));

    Session& session = m_model->session();
    session.canvasEvents().signal_camera_update.connect(
        sigc::mem_fun(*this, &Canvas::onCameraUpdate));
    session.canvasEvents().signal_queue_draw.connect(
        sigc::mem_fun(*this, &Canvas::queue_draw));
    m_change_cursor_connection = session.canvasEvents().signal_change_cursor.connect(
        sigc::mem_fun(*this, &Canvas::changeCursor));
}

Canvas::~Canvas() = default;

void Canvas::setSmoothScrollZoomSensitivity(int value, int min, int max)
{
    double mapped = mapSliderLog(value, min, max, 0.01, 100);
    m_smooth_scroll_zoom_sensitivity = mapped;
}

void Canvas::setSmoothScrollPanSensitivity(int value, int min, int max)
{
    double mapped = mapSliderLog(value, min, max, 0.001, 1000);
    m_smooth_scroll_pan_sensitivity = mapped;
}

bool Canvas::isPanning() const
{
    return rt::any(m_pan & PanningInput::ACTIVE);
}

void Canvas::onCameraUpdate()
{
    Session& session = m_model->session();

    updateCursorShape();

    for (CursorMonitor* listener : m_cursor_monitors) {
        listener->onMotion(m_model, session.cursorPos());
    }
}

void Canvas::changeCursor(std::optional<CursorShape> shape)
{
    if (shape) {
        m_cursor_manager.setCursor(*shape);
    } else {
        updateCursorShape();
    }
}

bool Canvas::on_event(GdkEvent* event)
{
    return m_scroll_controller->onEvent(event);
}

bool Canvas::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!m_renderer) return true;

    DrawContext context(this, m_model, cr);
    cr->save();
    m_renderer->onDraw(context);
    cr->restore();

    return true;
}

void Canvas::onEnter(WidgetPoint pos)
{
    m_is_cursor_inside_canvas = true;

    Session& session = m_model->session();
    session.setCursorPos(pos);
    updateCursorShape();

    for (CursorMonitor* listener : m_cursor_monitors) {
        listener->onEnter(m_model, pos);
    }
}

void Canvas::onMotion(WidgetPoint pos)
{
    Session& session = m_model->session();
    session.setCursorPos(pos);

    if (!isPanning()) {
        for (CursorMonitor* listener : m_cursor_monitors) {
            listener->onMotion(m_model, pos);
        }
    }
}

void Canvas::onFreeMotion(WidgetPoint pos)
{
    if (isPanning()) {
        updatePan(pos);
        return;
    }

    updateCursorShape();

    for (CursorMonitor* listener : m_cursor_monitors) {
        listener->onMotion(m_model, pos);
    }
}

void Canvas::onLeave()
{
    m_is_cursor_inside_canvas = false;

    if (m_is_pan_zoom_enabled) {
        m_pan &= ~PanningInput::SPACEBAR;
        if (!isPanning()) {
            updateCursorShape();
        }
    }

    for (CursorMonitor* listener : m_cursor_monitors) {
        listener->onLeave(m_model);
    }
}

void Canvas::onPendingPress(WidgetPoint pos)
{
    ClickContext context{ m_model, &m_mouse_gesture };
    if (tryPanPendingPress(context, pos)) return;
}

void Canvas::onClick(int n_press, WidgetPoint pos)
{
    if (isPanning()) {
        m_pan &= ~(PanningInput::PRIMARY | PanningInput::MIDDLE);
        if (!isPanning()) {
            updateCursorShape();
        }
    }
}

void Canvas::onCancelPress(WidgetPoint pos)
{
    if (isPanning()) {
        m_pan &= ~(PanningInput::PRIMARY | PanningInput::MIDDLE);
        if (!isPanning()) {
            updateCursorShape();
        }
        return;
    }
}

void Canvas::onDragBegin(WidgetPoint pos)
{
    m_drag_start_pos = pos;
    if (isPanning()) {
        m_prev_pan_pos = pos;
        return;
    }
}

void Canvas::onDragUpdate(WidgetVec delta)
{
    if (isPanning()) {
        WidgetPoint delta_pos = m_drag_start_pos + delta;
        updatePan(delta_pos);
        return;
    }
}

void Canvas::onDragEnd(WidgetVec delta)
{
    if (isPanning()) {
        WidgetPoint delta_pos = m_drag_start_pos + delta;
        updatePan(delta_pos);

        m_pan &= ~(PanningInput::PRIMARY | PanningInput::MIDDLE);
        if (!isPanning()) {
            updateCursorShape();
        }
        return;
    }
}

void Canvas::onScrollBegin()
{
    m_scroll_zoom_accum = 0;
}

bool Canvas::onScrollChanged(double dx, double dy)
{
    WidgetVec delta{ WidgetScalar(dx), WidgetScalar(dy) };

    if (tryPanZoomScroll(delta)) return true;

    return false;
}

void Canvas::onZoomBegin(GdkEventSequence* /* sequence */)
{
    m_camera_zoom_begin = m_model->session().camera().zoom;
}

void Canvas::onZoomChanged(double scale)
{
    if (m_is_pan_zoom_enabled) {
        double x = 0, y = 0;
        m_zoom_controller->get_bounding_box_center(x, y);

        WidgetPoint new_cursor_pos{ WidgetScalar(x), WidgetScalar(y) };
        m_model->session().setCursorPos(new_cursor_pos);

        double new_zoom = scale * m_camera_zoom_begin;
        updateZoom(new_zoom);
        m_scroll_zoom_accum = 0;
    }
}

bool Canvas::onKeyPressed(guint keyval, guint keycode, GdkModifierType state)
{
    Session& session = m_model->session();
    const int updated_state = state | keyvalToModifier(keyval);
    session.setModifiers(GdkModifierType(updated_state));

    if (m_is_pan_zoom_enabled) {
        const PanZoomFlags flags = session.panZoomFlags();
        const bool allow_space_pan = rt::any(flags & PanZoomFlags::SPACE_KEY_PAN);

        if ((keyval == GDK_KEY_space) && allow_space_pan) {
            if (m_is_cursor_inside_canvas) {
                m_prev_pan_pos = session.cursorPos();
                session.changeCursorShape(PAN_CURSOR);
                m_pan |= PanningInput::SPACEBAR;
            }
            return true;
        } else if (keyval == GDK_KEY_z) {
            session.zoom11();
            return true;
        } else if (keyval == GDK_KEY_f) {
            const ImageModel& image = m_model->image();
            // TODO: Zoom to crop when alt not held down
            if (session.modifiers() & GDK_MOD1_MASK) {
                session.zoomFit(WorldPoint{}, static_cast<WorldSize>(image.fullSize()),
                                Session::ZoomFitFlags::ADD_MARGIN
                                    | Session::ZoomFitFlags::ALLOW_ZOOM_IN);
            } else {
                session.zoomFit(WorldPoint{}, static_cast<WorldSize>(image.fullSize()),
                                Session::ZoomFitFlags::ADD_MARGIN
                                    | Session::ZoomFitFlags::ALLOW_ZOOM_IN);
            }
            return true;
        }
    }

    return false;
}

void Canvas::onKeyReleased(guint keyval, guint keycode, GdkModifierType state)
{
    Session& session = m_model->session();
    const int updated_state = state & ~keyvalToModifier(keyval);
    session.setModifiers(GdkModifierType(updated_state));

    if (m_is_pan_zoom_enabled) {
        if (keyval == GDK_KEY_space) {
            m_pan &= ~PanningInput::SPACEBAR;
            if (!isPanning()) {
                updateCursorShape();
            }
            return;
        }
    }
}

Gtk::SizeRequestMode Canvas::get_request_mode_vfunc() const
{
    return Gtk::Widget::get_request_mode_vfunc();
}

void Canvas::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    minimum_width = RTScalable::scalePixelSize(64);
    natural_width = RTScalable::scalePixelSize(640);
}

void Canvas::get_preferred_height_for_width_vfunc(int width,
                                                  int& minimum_height,
                                                  int& natural_height) const
{
    minimum_height = RTScalable::scalePixelSize(64);
    natural_height = width / NATURAL_ASPECT_RATIO;
}

void Canvas::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const
{
    minimum_height = RTScalable::scalePixelSize(64);
    natural_height = RTScalable::scalePixelSize(480);
}

void Canvas::get_preferred_width_for_height_vfunc(int height,
                                                  int& minimum_width,
                                                  int& natural_width) const
{
    minimum_width = RTScalable::scalePixelSize(64);
    natural_width = height * NATURAL_ASPECT_RATIO;
}

void Canvas::on_size_allocate(Gtk::Allocation& allocation)
{
    set_allocation(allocation);

    m_model->setCameraSize(
        WidgetSize{ WidgetScalar(static_cast<double>(allocation.get_width())),
                    WidgetScalar(static_cast<double>(allocation.get_height())) });
    signal_widget_size_update.emit();

    if (m_gdk_window) {
        m_gdk_window->move_resize(allocation.get_x(), allocation.get_y(),
                                  allocation.get_width(), allocation.get_height());
    }
}

void Canvas::on_realize()
{
    set_realized();
    if (m_gdk_window) return;

    GdkWindowAttr attributes;
    memset(&attributes, 0, sizeof(attributes));

    Gtk::Allocation allocation = get_allocation();
    attributes.x = allocation.get_x();
    attributes.y = allocation.get_y();
    attributes.width = allocation.get_width();
    attributes.height = allocation.get_height();

    // clang-format off
    attributes.event_mask = get_events()
        | Gdk::EXPOSURE_MASK
        | Gdk::POINTER_MOTION_MASK
        | Gdk::BUTTON_MOTION_MASK
        | Gdk::BUTTON_PRESS_MASK
        | Gdk::BUTTON_RELEASE_MASK
        | Gdk::KEY_PRESS_MASK
        | Gdk::KEY_RELEASE_MASK
        | Gdk::ENTER_NOTIFY_MASK
        | Gdk::LEAVE_NOTIFY_MASK
        | Gdk::SCROLL_MASK
        | Gdk::SMOOTH_SCROLL_MASK;
    // clang-format on
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.wclass = GDK_INPUT_OUTPUT;

    m_gdk_window =
        Gdk::Window::create(get_parent_window(), &attributes, GDK_WA_X | GDK_WA_Y);
    set_window(m_gdk_window);
    // Receive expose events
    m_gdk_window->set_user_data(gobj());
    m_cursor_manager.init(m_gdk_window);
}

void Canvas::on_unrealize()
{
    m_gdk_window.reset();
    Gtk::Widget::on_unrealize();
}

void Canvas::onScaleFactorChanged()
{
    m_model->setDeviceScale(get_scale_factor());
    signal_widget_size_update.emit();
}

bool Canvas::tryPanPendingPress(const ClickContext& context, WidgetPoint pos)
{
    if (!m_is_pan_zoom_enabled) return false;

    Session& session = m_model->session();
    const guint button = context.controller->get_current_button();

    if (button == GDK_BUTTON_PRIMARY
        && rt::any(session.panZoomFlags() & PanZoomFlags::PRIMARY_BUTTON_PAN))
    {
        m_prev_pan_pos = pos;
        session.changeCursorShape(PAN_CURSOR);
        m_pan |= PanningInput::PRIMARY;
        return true;
    }
    if (button == GDK_BUTTON_MIDDLE
        && rt::any(session.panZoomFlags() & PanZoomFlags::MIDDLE_BUTTON_PAN))
    {
        m_prev_pan_pos = pos;
        session.changeCursorShape(PAN_CURSOR);
        m_pan |= PanningInput::MIDDLE;
        return true;
    }

    return false;
}

bool Canvas::tryPanZoomScroll(WidgetVec scroll_delta)
{
    if (!m_is_pan_zoom_enabled) return false;

    if (m_scroll_mode == ScrollMode::PAN) {
        return tryPanScroll(scroll_delta);
    } else {
        return tryZoomScroll(scroll_delta);
    }
}

bool Canvas::tryZoomScroll(WidgetVec scroll_delta)
{
    const Session& session = m_model->session();
    const CameraState& camera = session.camera();
    const PanZoomFlags flags = session.panZoomFlags();
    const GdkModifierType state = session.modifiers();
    const bool is_smooth = m_scroll_controller->get_scroll_unit() == ScrollUnit::SURFACE;

    auto allow = [&](PanZoomFlags test) { return rt::any(flags & test); };

    if (is_smooth) {
        if (allow(PanZoomFlags::PAN_WITH_MOD_SCROLL)) {
            if (state & GDK_CONTROL_MASK) {
                scroll_delta.x = WidgetScalar(0);
                updatePanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (state & GDK_SHIFT_MASK) {
                updateHorizontalPanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (state & GDK_MOD1_MASK) {
                updatePanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }

        if (allow(PanZoomFlags::ZOOM_WITH_SCROLL)) {
            m_scroll_zoom_accum +=
                scroll_delta.y.value() * m_smooth_scroll_zoom_sensitivity;

            if (m_scroll_zoom_accum >= 1.0) {
                updateZoom(camera.zoom / ZOOM_FACTOR);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (m_scroll_zoom_accum <= -1.0) {
                updateZoom(camera.zoom * ZOOM_FACTOR);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }
    } else {
        if (allow(PanZoomFlags::PAN_WITH_MOD_SCROLL)) {
            if (state & GDK_CONTROL_MASK) {
                scroll_delta.x = WidgetScalar(0);
                updatePanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (state & GDK_SHIFT_MASK) {
                updateHorizontalPanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }

        if (allow(PanZoomFlags::ZOOM_WITH_SCROLL)) {
            // In GTK 4, high resolution mice can emit discrete scroll events
            // that are less than 1.0.
            m_scroll_zoom_accum += scroll_delta.y.value();

            if (m_scroll_zoom_accum >= 1.0) {
                updateZoom(camera.zoom / ZOOM_FACTOR);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (m_scroll_zoom_accum <= -1.0) {
                updateZoom(camera.zoom * ZOOM_FACTOR);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }
    }

    return false;
}

bool Canvas::tryPanScroll(WidgetVec scroll_delta)
{
    const Session& session = m_model->session();
    const CameraState& camera = session.camera();
    const PanZoomFlags flags = session.panZoomFlags();
    const GdkModifierType state = session.modifiers();
    const bool is_smooth = m_scroll_controller->get_scroll_unit() == ScrollUnit::SURFACE;

    auto allow = [&](PanZoomFlags test) { return rt::any(flags & test); };

    if (is_smooth) {
        if (allow(PanZoomFlags::ZOOM_WITH_MOD_SCROLL)) {
            if (state & GDK_MOD1_MASK) {
                m_scroll_zoom_accum +=
                    scroll_delta.y.value() * m_smooth_scroll_zoom_sensitivity;

                if (m_scroll_zoom_accum >= 1.0) {
                    updateZoom(camera.zoom / ZOOM_FACTOR, false);
                    m_scroll_zoom_accum = 0;
                    return true;
                } else if (m_scroll_zoom_accum <= -1.0) {
                    updateZoom(camera.zoom * ZOOM_FACTOR, false);
                    m_scroll_zoom_accum = 0;
                    return true;
                }
            }
        }

        if (allow(PanZoomFlags::PAN_WITH_MOD_SCROLL)) {
            if (state & GDK_CONTROL_MASK) {
                scroll_delta.x = WidgetScalar(0);
                updatePanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (state & GDK_SHIFT_MASK) {
                updateHorizontalPanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }

        if (allow(PanZoomFlags::PAN_WITH_SCROLL)) {
            // Don't trigger a pan while ALT is held down. Otherwise, zooming
            // is accompanied by an undesired pan.
            if (!(state & GDK_MOD1_MASK)) {
                updatePanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }
    } else {
        if (allow(PanZoomFlags::ZOOM_WITH_MOD_SCROLL)) {
            if (state & GDK_MOD1_MASK) {
                // In GTK 4, high resolution mice can emit discrete scroll
                // events that are less than 1.0.
                m_scroll_zoom_accum += scroll_delta.y.value();

                if (m_scroll_zoom_accum >= 1.0) {
                    updateZoom(camera.zoom / ZOOM_FACTOR);
                    m_scroll_zoom_accum = 0;
                    return true;
                } else if (m_scroll_zoom_accum <= -1.0) {
                    updateZoom(camera.zoom * ZOOM_FACTOR);
                    m_scroll_zoom_accum = 0;
                    return true;
                }
            }
        }

        if (allow(PanZoomFlags::PAN_WITH_MOD_SCROLL)) {
            if (state & GDK_CONTROL_MASK) {
                scroll_delta.x = WidgetScalar(0);
                updatePanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            } else if (state & GDK_SHIFT_MASK) {
                updateHorizontalPanWithScroll(scroll_delta);
                m_scroll_zoom_accum = 0;
                return true;
            }
        }

        if (allow(PanZoomFlags::PAN_WITH_SCROLL)) {
            updatePanWithScroll(scroll_delta);
            m_scroll_zoom_accum = 0;
            return true;
        }
    }

    return false;
}

void Canvas::updatePan(WidgetPoint delta_pos)
{
    Session& session = m_model->session();
    const CameraState& camera = session.camera();

    WidgetVec widget_offset = delta_pos - m_prev_pan_pos;

    WorldVec adjustment = session.widgetToWorldTransform()(widget_offset);
    WorldPoint new_pos = camera.pos - adjustment;

    m_prev_pan_pos = delta_pos;
    signal_pan_zoom.emit();
    m_model->setCameraPos(new_pos);
}

void Canvas::updatePanWithScroll(WidgetVec delta)
{
    Session& session = m_model->session();
    const CameraState& camera = session.camera();

    WorldVec bounds = session.widgetToWorldTransform()(camera.size.asVec());
    double step = rt::min(bounds.x.value(), bounds.y.value());
    double pan_sensitivity = 0.1;

    if (m_scroll_controller->get_scroll_unit() == ScrollUnit::SURFACE) {
        // Due to limitations in GTK 3, mouse wheel detents are considered
        // as smooth scrolls with surface units. This causes the scrolling
        // sensitivity and direction to conflict between mouse wheel and
        // touchpad. There is no way to fix this without moving to GTK 4.
        //
        // The default for touchpads should be to apply the -1 factor to have
        // natural scrolling like how scrolling on a phone works.
        //
        // The default for mouse wheel detents should be to NOT apply the -1
        // factor to match the behaviour in desktop apps and browsers.
        pan_sensitivity = m_smooth_scroll_pan_sensitivity;

        if (m_reverse_smooth_scroll_dir) {
            pan_sensitivity *= -1;
        }
    } else {
        if (m_reverse_discrete_scroll_dir) {
            pan_sensitivity *= -1;
        }
    }

    step *= pan_sensitivity;

    WorldPoint new_pos = camera.pos;
    new_pos.x += WorldScalar(step * delta.x.value());
    new_pos.y += WorldScalar(step * delta.y.value());

    signal_pan_zoom.emit();
    m_model->setCameraPos(new_pos);
}

void Canvas::updateHorizontalPanWithScroll(WidgetVec delta)
{
    // On MacOS, Shift + Scroll emits a horizontal scroll as a builtin feature
    // of the OS. A touchpad can also emit horizontal deltas. Only replace the
    // horizontal delta for vertical scrolls (e.g. with scroll wheel).
    if (delta.x.value() == 0) {
        delta.x = delta.y;
    }
    delta.y = WidgetScalar(0);
    updatePanWithScroll(delta);
}

void Canvas::updateZoom(double new_zoom, bool preserve_cursor)
{
    Session& session = m_model->session();
    const CameraState& camera = session.camera();

    if (new_zoom <= camera.zoom && camera.zoom <= session.minZoom()) return;
    if (new_zoom >= camera.zoom && camera.zoom >= session.maxZoom()) return;

    signal_pan_zoom.emit();
    auto mode =
        preserve_cursor ? Session::ZoomMode::PRESERVE_CURSOR : Session::ZoomMode::BASIC;
    m_model->setCameraZoom(new_zoom, mode);
}

void Canvas::updateCursorShape()
{
    Session& session = m_model->session();

    std::optional<CursorShape> shape;
    if (isPanning()) {
        shape = PAN_CURSOR;
    }
    if (!shape) {
        shape = m_model->isCursorInsideImage() ? CSCrosshair : CSArrow;
    }

    // Prevent recursive cursor shape updates
    ConnectionBlocker blocker(m_change_cursor_connection);
    if (*shape != session.cursorShape()) {
        session.changeCursorShape(*shape);
        m_cursor_manager.setCursor(*shape);
    }
}
