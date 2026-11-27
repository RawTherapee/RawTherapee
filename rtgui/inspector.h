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

#include "canvas/coord.h"
#include "gtk4.h"

#include "rtengine/coord2d.h"
#include "rtengine/math/rect.h"

#include <gtkmm/box.h>
#include <gtkmm/window.h>

#include <memory>
#include <optional>

namespace rt {
namespace canvas {

class Canvas;
class CanvasModel;
class InspectorRenderer;

}  // namespace canvas
}  // namespace rt

struct InspectorBuffer;

class Inspector final : public Gtk::Box
{
private:
    std::unique_ptr<Gtk::Window> m_window;
    std::unique_ptr<rt::canvas::CanvasModel> m_canvas_model;
    std::unique_ptr<rt::canvas::InspectorRenderer> m_renderer;
    rt::canvas::Canvas* m_canvas;

    Glib::RefPtr<rt::gtk4::GestureClick> m_click_controller;
    Glib::RefPtr<rt::gtk4::EventControllerKey> m_key_controller;

    std::vector<std::unique_ptr<InspectorBuffer>> m_images;
    InspectorBuffer* m_curr_image;

    Glib::ustring m_next_image_path;
    rtengine::Coord2D m_next_image_pos;
    rt::canvas::WorldPoint m_last_camera_pos;
    sigc::connection m_delay_connection;

    Glib::ustring m_last_image_path;
    std::optional<rt::geom::Rect> m_last_image_observed_rect;

    bool m_is_active;
    bool m_is_pinned;
    bool m_fit_to_screen;
    bool m_is_initialized;
    bool m_is_device_scale_initialized;
    bool m_is_window_fullscreen;
    bool m_is_window_showing;
    bool m_is_key_down;
    bool m_suppress_mouse_move;

    void onWindowHide();
    bool onWindowStateEvent(GdkEventWindowState* event);
    bool onWindowFocusOut(GdkEventFocus* event);
    void onCanvasSizeChanged();
    void onCanvasPanZoom();
    void onCameraUpdate();
    void onPreferencesChanged();

    bool doSwitchImage();
    void changeCurrImage(InspectorBuffer* buffer);
    void showImageOnCanvas();
    void clearCanvas();
    void recordObservedRect();

    void onButtonPressed(int n_press, double x, double y);
    bool onKeyPressed(guint keyval, guint keycode, GdkModifierType state);
    void onKeyReleased(guint keyval, guint keycode, GdkModifierType state);

public:
    sigc::signal<void()> signal_observed_area_changed;

    Inspector();
    ~Inspector();

    /** @brief Show or hide window
     * @param pinned pin window
     * @param scaled fit image into window
     */
    void showWindow(bool pinned, bool scaled = true);

    /**
     * Hide the window.
     */
    void hideWindow() { if (m_window) m_window->set_visible(false); }

    /** @brief Mouse movement to a new position
     * @param pos Location of the mouse, in percentage (i.e. [0;1] range) relative to the full size image ; -1,-1 == out of the image
     */
    void mouseMove(rtengine::Coord2D pos);

    /** @brief A new image is being flown over
     * @param full_path Full path of the image that is being hovered inspect, or an empty string if out of any image.
     */
    void switchImage(const Glib::ustring& full_path);

    /** @brief Use this method to flush all image buffer whenever the Inspector panel is hidden
     */
    void flushBuffers();

    /** @brief Set the inspector on/off
     * @param state true if to activate the Inspector, false to disable it and flush the buffers
     */
    void setActive(bool state);
    bool isActive() const { return m_is_active; };

    const Glib::ustring& lastImageFilepath() const { return m_last_image_path; }
    // Values are normalized to [0, 1] over the image's dimensions
    const std::optional<rt::geom::Rect>&
    lastImageObservedRect() const { return m_last_image_observed_rect; }

    void clearObservedArea();

    /**
     * When the inspector window is opened, there may still be unprocessed
     * motion events. When the events get processed, it causes a flickering/
     * jump in the image position. Suppress mouse motion processing while the
     * window is not pinned.
     */
    void suppressMouseMove(bool state) { m_suppress_mouse_move = state; }

    void onBrowserDeviceScaleChanged(int device_scale);
};
