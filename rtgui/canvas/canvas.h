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

#include "interface.h"
#include "mousegesture.h"

#include "cursormanager.h"
#include "gtk4.h"

#include "rtengine/util/enum.h"

#include <cairomm/refptr.h>
#include <glibmm/refptr.h>
#include <gtkmm/widget.h>

#include <optional>
#include <vector>

namespace Gtk {

class GestureDrag;
class GestureZoom;

}  // namespace Gtk

namespace rt {
namespace canvas {

class CanvasModel;

enum class PanningInput {
    NONE = 0,
    PRIMARY = (1 << 0),
    MIDDLE = (1 << 1),
    SPACEBAR = (1 << 2),
    // Helpers
    MOUSE = PRIMARY | MIDDLE,
    ACTIVE = PRIMARY | MIDDLE | SPACEBAR
};

enum class ScrollMode {
    ZOOM,  // Scrolling initiates zoom by default
    PAN    // Scrolling initiates panning by default
};

class Canvas : public Gtk::Widget
{
public:
    using PanZoomSignal = sigc::signal<void()>;
    using WidgetSizeUpdateSignal = sigc::signal<void()>;

    Canvas(CanvasModel* model);
    ~Canvas();

    CanvasModel* model() { return m_model; }
    const CanvasModel* model() const { return m_model; }

    void enablePanZoom(bool value) { m_is_pan_zoom_enabled = value; }
    void setSmoothScrollZoomSensitivity(int value, int min, int max);
    void setSmoothScrollPanSensitivity(int value, int min, int max);
    void setReverseDiscreteScrollDirection(bool value)
    {
        m_reverse_discrete_scroll_dir = value;
    }
    void setReverseSmoothScrollDirection(bool value)
    {
        m_reverse_smooth_scroll_dir = value;
    }
    void setScrollMode(ScrollMode mode) { m_scroll_mode = mode; }

    void addCursorMonitor(CursorMonitor* listener)
    {
        m_cursor_monitors.push_back(listener);
    }

    void changeCursor(std::optional<CursorShape> shape);

    bool onKeyPressed(guint keyval, guint keycode, GdkModifierType state);
    void onKeyReleased(guint keyval, guint keycode, GdkModifierType state);

    /**
     * This signal is emitted whenever a user initiates a pan/zoom and the
     * event is handled by the canvas. The signal is emitted before CameraState
     * is updated.
     */
    PanZoomSignal signal_pan_zoom;
    /**
     * This signal is emitted whenever the allocated size or device scale of
     * the canvas changes. The signal occurs after CameraState is updated.
     */
    WidgetSizeUpdateSignal signal_widget_size_update;

protected:
    // Custom widget implementation
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width,
                                              int& minimum_height,
                                              int& natural_height) const override;
    void get_preferred_height_vfunc(int& minimum_height,
                                    int& natural_height) const override;
    void get_preferred_width_for_height_vfunc(int height,
                                              int& minimum_width,
                                              int& natural_width) const override;
    void on_size_allocate(Gtk::Allocation& allocation) override;
    // TODO(GTK4): on_realize and on_unrealize are no longer needed since
    //             Gdk.Window was removed for snapshots
    void on_realize() override;
    void on_unrealize() override;
    bool on_event(GdkEvent* event) override;

    // Canvas behaviour customization

    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override
    {
        return false;
    }

    /**
     * Check for zoom key shortcuts and trigger if detected.
     *
     * @return true if a zoom key was pressed
     */
    virtual bool onZoomKeyPressed(guint keyval) { return false; }
    virtual std::optional<CursorShape> queryCursorShape() const { return std::nullopt; }

private:
    // Event controller slots
    void onEnter(WidgetPoint pos);
    void onMotion(WidgetPoint pos);
    void onFreeMotion(WidgetPoint pos);
    void onLeave();
    void onPendingPress(WidgetPoint pos);
    void onClick(int n_press, WidgetPoint pos);
    void onCancelPress(WidgetPoint pos);
    void onDragBegin(WidgetPoint pos);
    void onDragUpdate(WidgetVec delta);
    void onDragEnd(WidgetVec delta);
    void onScrollBegin();
    bool onScrollChanged(double dx, double dy);
    void onZoomBegin(GdkEventSequence* sequence);
    void onZoomChanged(double scale);

    void onScaleFactorChanged();

    void onCameraUpdate();

    bool isPanning() const;
    bool tryPanPendingPress(WidgetPoint pos);
    bool tryPanZoomScroll(WidgetVec scroll_delta);
    bool tryZoomScroll(WidgetVec scroll_delta);
    bool tryPanScroll(WidgetVec scroll_delta);
    void updatePan(WidgetPoint delta_pos);
    void updatePanWithScroll(WidgetVec delta);
    void updateHorizontalPanWithScroll(WidgetVec delta);
    void updateZoom(double new_zoom, bool preserve_cursor = true);
    void updateCursorShape();

    CursorManager m_cursor_manager;
    MouseGesture m_mouse_gesture;

    Glib::RefPtr<Gdk::Window> m_gdk_window;

    Glib::RefPtr<Gtk::GestureZoom> m_zoom_controller;
    std::unique_ptr<rt::gtk4::EventControllerScroll> m_scroll_controller;
    sigc::connection m_change_cursor_connection;

    std::vector<CursorMonitor*> m_cursor_monitors;
    CanvasModel* m_model;

    // Pan/zoom state
    WidgetPoint m_prev_pan_pos;
    WidgetPoint m_drag_start_pos;
    double m_smooth_scroll_zoom_sensitivity;
    double m_smooth_scroll_pan_sensitivity;
    double m_scroll_zoom_accum;
    double m_camera_zoom_begin;
    PanningInput m_pan;
    ScrollMode m_scroll_mode;
    bool m_is_pan_zoom_enabled;
    bool m_reverse_discrete_scroll_dir;
    bool m_reverse_smooth_scroll_dir;
    bool m_is_cursor_inside_canvas;
};

}  // namespace canvas
}  // namespace rt

template <> struct rt::EnumAsBitflags<rt::canvas::PanningInput> : std::true_type
{
};
