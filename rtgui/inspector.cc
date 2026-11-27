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

#include "inspector.h"

#include "canvas/canvas.h"
#include "canvas/model.h"
#include "canvas/render.h"
#include "guiutils.h"
#include "multilangmgr.h"
#include "options.h"
#include "pathutils.h"
#include "rtscalable.h"

#include "rtengine/previewimage.h"
#include "rtengine/rt_math.h"
#include "rtengine/rtapp.h"

using namespace rt;
using namespace rt::canvas;

struct InspectorBuffer
{
    Glib::ustring filepath;
    Cairo::RefPtr<Cairo::ImageSurface> surface;

    InspectorBuffer(const Glib::ustring& image_path,
                    const Cairo::RefPtr<Cairo::ImageSurface>& image_surface)
        : filepath(image_path), surface(image_surface)
    {
    }
};

Inspector::Inspector()
    : m_canvas_model(std::make_unique<CanvasModel>()),
      m_renderer(std::make_unique<InspectorRenderer>()),
      m_curr_image(nullptr),
      m_is_active(false),
      m_is_pinned(false),
      m_fit_to_screen(false),
      m_is_initialized(false),
      m_is_device_scale_initialized(false),
      m_is_window_fullscreen(false),
      m_is_window_showing(false),
      m_is_key_down(false),
      m_suppress_mouse_move(false)
{
    set_name("Inspector");

    m_canvas = rt::make_managed<Canvas>(m_canvas_model.get());
    m_canvas->setRenderer(m_renderer.get());
    onPreferencesChanged();  // Configure pan zoom based on options
    pack_start(*m_canvas, true, true);

    const auto& options = App::get().options();
    if (options.inspectorWindow) {
        m_window = std::make_unique<Gtk::Window>();

        m_window->set_name("InspectorWindow");
        m_window->set_title("RawTherapee " + M("INSPECTOR_WINDOW_TITLE"));
        m_window->set_visible(false);

        m_window->signal_hide().connect(sigc::mem_fun(*this, &Inspector::onWindowHide));
        m_window->signal_window_state_event().connect(
            sigc::mem_fun(*this, &Inspector::onWindowStateEvent));
        m_window->signal_focus_out_event().connect(
            sigc::mem_fun(*this, &Inspector::onWindowFocusOut));

        m_window->add_events(Gdk::BUTTON_PRESS_MASK | Gdk::KEY_PRESS_MASK
                             | Gdk::FOCUS_CHANGE_MASK);

        m_click_controller = rt::gtk4::GestureClick::create(m_window.get());
        m_key_controller = rt::gtk4::EventControllerKey::create(m_window.get());

        m_click_controller->signal_pressed().connect(
            sigc::mem_fun(*this, &Inspector::onButtonPressed));
        m_key_controller->set_propagation_phase(GTK_PHASE_CAPTURE);
        m_key_controller->signal_key_pressed().connect(
            sigc::mem_fun(*this, &Inspector::onKeyPressed));
        m_key_controller->signal_key_released().connect(
            sigc::mem_fun(*this, &Inspector::onKeyReleased));

        m_window->add(*this);
        m_window->set_size_request(500, 500);
        m_window->fullscreen();

        m_canvas_model->session().setCameraBounds(Session::CameraBounds::INSPECTOR_WINDOW);

        m_canvas->enablePanZoom(true);
        m_canvas->signal_pan_zoom.connect(
            sigc::mem_fun(*this, &Inspector::onCanvasPanZoom));
        m_canvas->signal_widget_size_update.connect(
            sigc::mem_fun(*this, &Inspector::onCanvasSizeChanged));
        m_canvas_model->session().canvasEvents().signal_camera_update.connect(
            sigc::mem_fun(*this, &Inspector::onCameraUpdate));

        m_is_initialized = false;  // Delay init to avoid flickering on some systems
        m_is_active = true;  // Always track inspected thumbnails
    } else {
        m_canvas_model->session().setCameraBounds(Session::CameraBounds::INSPECTOR_PANEL);
        m_renderer->setDrawFrame(true);
    }

    App::get().signal_preferences_changed().connect(
        sigc::mem_fun(*this, &Inspector::onPreferencesChanged));
}

Inspector::~Inspector() = default;

void Inspector::showWindow(bool pinned, bool scaled)
{
    if (!m_is_active || !m_window || m_is_window_showing) return;

    if (!m_is_initialized) {
        m_window->show_all();
        m_is_initialized = true;
        // If onBrowserDeviceScaleChanged() has not been called already by now,
        // there is no device scale change needed.
        m_is_device_scale_initialized = true;
    }

    // The window must be set to visible before calling switchImage() otherwise
    // an internal check ignores the update...
    m_window->present();
    m_is_window_showing = true;

    m_is_pinned = pinned;
    m_fit_to_screen = scaled;

    // Update content when becoming visible
    clearCanvas();
    switchImage(m_next_image_path);
}

void Inspector::onButtonPressed(int n_press, double x, double y)
{
    if (!m_window) return;

    if (!m_is_pinned) {
        // Pin window with mouse click
        m_is_pinned = true;
    }
    m_suppress_mouse_move = false;
}

bool Inspector::onKeyPressed(guint keyval, guint keycode, GdkModifierType state)
{
    if (!m_window) return false;
    if (m_is_key_down) return true;

    switch (keyval) {
        case GDK_KEY_z:
        case GDK_KEY_F:
            m_is_key_down = true;
            if (m_is_pinned) {
                if (m_fit_to_screen) {
                    m_canvas_model->setCameraPosZoom(m_last_camera_pos, 1.0);
                } else {
                    m_canvas_model->setCameraZoom(
                        1.0, m_canvas_model->session().preferredZoomMode());
                }
                recordObservedRect();
                m_canvas_model->session().queueDraw();
            }
            m_fit_to_screen = false;
            return true;
        case GDK_KEY_f:
            m_is_key_down = true;
            m_fit_to_screen = true;
            if (m_is_pinned) {
                m_canvas_model->zoomFit();
                recordObservedRect();
                m_canvas_model->session().queueDraw();
            }
            return true;
        case GDK_KEY_F11:
            // Toggle fullscreen
            m_is_key_down = true;
            if (m_is_window_fullscreen) {
                m_window->unfullscreen();
            } else {
                m_window->fullscreen();
            }
            m_is_window_fullscreen = !m_is_window_fullscreen;
            return true;
        case GDK_KEY_Escape:
            // Hide window
            m_is_key_down = true;
            m_is_pinned = false;
            m_window->set_visible(false);
            return true;
    }

    if (m_is_pinned) {
        return m_canvas->onKeyPressed(keyval, keycode, state);
    }

    return false;
}

void Inspector::onKeyReleased(guint keyval, guint keycode, GdkModifierType state)
{
    m_is_key_down = false;

    if (!m_window) return;

    if (!m_is_pinned) {
        switch (keyval) {
            case GDK_KEY_f:
            case GDK_KEY_F:
            case GDK_KEY_z:
                m_suppress_mouse_move = false;
                m_window->set_visible(false);
            default:
                break;
        }
    } else {
        m_canvas->onKeyReleased(keyval, keycode, state);
    }
}

void Inspector::onWindowHide()
{
    m_is_window_showing = false;
    m_is_pinned = false;
    m_is_key_down = false;
    m_suppress_mouse_move = false;
}

bool Inspector::onWindowStateEvent(GdkEventWindowState* event)
{
    if (!m_window->get_window() || m_window->get_window()->gobj() != event->window) {
        return false;
    }

    m_is_window_fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

    return true;
}

bool Inspector::onWindowFocusOut(GdkEventFocus* event)
{
    // Losing focus means the key release event that would reset m_is_key_down
    // is lost. Reset the value here so that the first button press is not
    // ignored when the window is opened again afterwards.
    m_is_key_down = false;
    m_canvas_model->session().onWindowFocusLost(m_canvas_model.get());
    return false;
}

void Inspector::onBrowserDeviceScaleChanged(int device_scale)
{
    // In GTK 3, the device scale is only updated after the widget is mapped.
    // In some cases, there is flickering caused by 1 frame being drawn at the
    // fallback device scale and then the device scale being updated. This only
    // happens the first time the inspector window is opened.
    //
    // Since the inspector window opens on the same display as the browser
    // window initially and the browser must have already been mapped, we can
    // preload the device scale to prevent the flicker.
    if (m_window && !m_is_device_scale_initialized) {
        m_canvas_model->session().setDeviceScale(device_scale);
        m_is_device_scale_initialized = true;
    }
}

void Inspector::onCanvasPanZoom()
{
    m_fit_to_screen = false;
    recordObservedRect();
}

void Inspector::onCanvasSizeChanged()
{
    showImageOnCanvas();
}

void Inspector::onCameraUpdate()
{
    if (!m_fit_to_screen) {
        const CameraState& camera = m_canvas_model->session().camera();
        m_last_camera_pos = camera.pos;
    }
}

void Inspector::onPreferencesChanged()
{
    const auto& options = App::get().options();

    m_canvas->setScrollMode(options.zoomOnScroll ? ScrollMode::ZOOM : ScrollMode::PAN);
    m_canvas->setReverseDiscreteScrollDirection(options.reverseDiscreteScrollDir);
    m_canvas->setReverseSmoothScrollDirection(options.reverseSmoothScrollDir);
    m_canvas->setSmoothScrollZoomSensitivity(
        options.smoothScrollZoomSensitivity,
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_MIN,
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_MAX);
    m_canvas->setSmoothScrollPanSensitivity(
        options.smoothScrollPanSensitivity,
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_MIN,
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_MAX);

    switch (options.zoom11Mode) {
        case Options::Zoom11Mode::CENTER_CURSOR:
            m_canvas_model->session().setZoomMode(Session::ZoomMode::CENTER_CURSOR);
            break;
        case Options::Zoom11Mode::PRESERVE_CURSOR:
            m_canvas_model->session().setZoomMode(Session::ZoomMode::PRESERVE_CURSOR);
            break;
        case Options::Zoom11Mode::BASIC:
        default:
            m_canvas_model->session().setZoomMode(Session::ZoomMode::BASIC);
            break;
    }
}

void Inspector::mouseMove(rtengine::Coord2D pos)
{
    if (!m_is_active) return;
    if (m_suppress_mouse_move) return;

    m_next_image_pos = pos;

    // Skip actual update of content when not visible
    if (m_window && !m_window->get_visible()) return;
    if (!m_curr_image || !m_curr_image->surface) return;

    double x = static_cast<double>(m_curr_image->surface->get_width());
    double y = static_cast<double>(m_curr_image->surface->get_height());

    x *= rtengine::LIM01(pos.x);
    y *= rtengine::LIM01(pos.y);

    m_canvas_model->setCameraPos(WorldPoint{WorldScalar(x), WorldScalar(y)});
    recordObservedRect();
    m_canvas_model->session().queueDraw();
}

void Inspector::switchImage(const Glib::ustring& full_path)
{
    if (!m_is_active) return;

    if (m_delay_connection.connected()) {
        m_delay_connection.disconnect();
    }

    m_next_image_path = full_path;
    clearCanvas();

    // Skip actual update of content when not visible
    if (m_window && !m_window->get_visible()) return;

    const auto& options = App::get().options();
    if (!options.inspectorDelay) {
        doSwitchImage();
    } else {
        m_delay_connection = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &Inspector::doSwitchImage), options.inspectorDelay);
    }
}


bool Inspector::doSwitchImage()
{
    // Update buffer cache based on preferences
    const size_t max_cache_size = []() {
        int val = App::get().options().maxInspectorBuffers;
        return val > 0 ? static_cast<size_t>(val) : 1;
    }();

    if (m_images.size() > max_cache_size) {
        const size_t num_dropped = m_images.size() - max_cache_size;

        // Drop oldest entries
        for (size_t i = 0; i < num_dropped; i++) {
            if (m_curr_image == m_images.at(i).get()) {
                // Shouldn't be possible but guard for memory safety
                changeCurrImage(nullptr);
            }
            m_images.at(i) = std::move(m_images.at(num_dropped + i));
        }
        m_images.resize(max_cache_size);
    }

    if (m_next_image_path.empty()) {
        m_curr_image = nullptr;
        clearCanvas();
        return true;
    }

    for (size_t i = 0; i < m_images.size(); ++i) {
        if (!m_images[i] || m_images[i]->filepath != m_next_image_path) continue;

        // Rotate towards front by 1
        std::unique_ptr<InspectorBuffer> tmp = std::move(m_images[i]);
        for (size_t j = i; j < m_images.size() - 1; ++j) {
            m_images.at(j) = std::move(m_images.at(j + 1));
        }

        // Move the last used image to the tail
        changeCurrImage(tmp.get());
        m_images.back() = std::move(tmp);
        return true;
    }

    // Loading a new image
    Glib::ustring ext = getExtension(m_next_image_path);
    if (ext.empty()) {
        changeCurrImage(nullptr);
        m_next_image_path.clear();
        return true;
    }

    rtengine::PreviewImage pi(m_next_image_path, ext,
                              rtengine::PreviewImage::PIM_EmbeddedOrRaw);
    Cairo::RefPtr<Cairo::ImageSurface> surface = pi.getImage();
    if (!surface) {
        changeCurrImage(nullptr);
        m_next_image_path.clear();
        return true;
    }

    // Add loaded image to tail
    auto buffer = std::make_unique<InspectorBuffer>(m_next_image_path, surface);
    changeCurrImage(buffer.get());
    if (m_images.size() == max_cache_size) {
        m_images.erase(m_images.begin());  // Delete the oldest entry
    }
    m_images.emplace_back(std::move(buffer));
    return true;
}

void Inspector::changeCurrImage(InspectorBuffer* buffer)
{
    m_curr_image = buffer;
    showImageOnCanvas();
}

void Inspector::showImageOnCanvas()
{
    if (!m_curr_image || !m_curr_image->surface) {
        clearCanvas();
        return;
    }

    IntWorldSize img_size;
    img_size.width = IntWorldScalar(m_curr_image->surface->get_width());
    img_size.height = IntWorldScalar(m_curr_image->surface->get_height());
    m_canvas_model->image().setImageSurface(m_curr_image->surface, img_size);

    if (m_fit_to_screen) {
        m_canvas_model->zoomFit();
    } else {
        double x = static_cast<double>(m_curr_image->surface->get_width());
        double y = static_cast<double>(m_curr_image->surface->get_height());

        x *= rtengine::LIM01(m_next_image_pos.x);
        y *= rtengine::LIM01(m_next_image_pos.y);

        WorldPoint new_pos{WorldScalar(x), WorldScalar(y)};
        m_canvas_model->setCameraPosZoom(new_pos, 1.0);
    }

    m_last_image_path = m_curr_image->filepath;
    recordObservedRect();
    m_canvas_model->session().queueDraw();
}

void Inspector::clearCanvas()
{
    m_canvas_model->image().setImageSurface(
        Cairo::RefPtr<Cairo::ImageSurface>{}, IntWorldSize{});
    m_canvas_model->setCameraPosZoom(WorldPoint{}, 1.0);
    m_canvas_model->session().queueDraw();
}

void Inspector::recordObservedRect()
{
    if (!App::get().options().showInspectorObservedArea) return;

    IntWorldSize img = m_canvas_model->image().fullSize();
    int width = img.width.value();
    int height = img.height.value();
    if (width <= 0 || height <= 0) {
        m_last_image_observed_rect = std::nullopt;
        return;
    }

    geom::Rect img_bbox(geom::Point(), geom::Point(width, height));

    geom::Rect cam_bbox = m_canvas_model->session().cameraBBox();

    std::optional<geom::Rect> observed_bbox = cam_bbox.intersect(img_bbox);
    if (!observed_bbox) {
        m_last_image_observed_rect = std::nullopt;
        return;
    }

    double min_x = observed_bbox->min().x / width;
    double min_y = observed_bbox->min().y / height;
    double max_x = observed_bbox->max().x / width;
    double max_y = observed_bbox->max().y / height;

    m_last_image_observed_rect = geom::Rect(geom::Point(min_x, min_y),
                                            geom::Point(max_x, max_y));
    signal_observed_area_changed.emit();
}

void Inspector::flushBuffers()
{
    changeCurrImage(nullptr);
    m_images.clear();
}

void Inspector::setActive(bool state)
{
    if (!state) {
        flushBuffers();

        m_last_image_path = "";
        m_last_image_observed_rect = std::nullopt;
    }

    if (!m_window) {
        m_is_active = state;
    }
}

void Inspector::clearObservedArea()
{
    m_last_image_observed_rect = std::nullopt;
    signal_observed_area_changed.emit();
}
