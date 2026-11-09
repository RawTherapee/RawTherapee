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
#include "interface.h"

#include "rtengine/math/rect.h"
#include "rtengine/util/enum.h"

#include <cairomm/surface.h>
#include <gdk/gdk.h>
#include <sigc++/sigc++.h>

#include <memory>
#include <optional>

namespace rt {
namespace canvas {

class CanvasModel;

struct CanvasEvents
{
    using CameraUpdateSignal = sigc::signal<void()>;
    using QueueDrawSignal = sigc::signal<void()>;
    using ChangeCursorSignal = sigc::signal<void(std::optional<CursorShape>)>;

    CameraUpdateSignal signal_camera_update;
    QueueDrawSignal signal_queue_draw;
    ChangeCursorSignal signal_change_cursor;
};

class Session
{
public:
    // clang-format off
    enum class PanZoomFlags {
        NONE = 0,
        PRIMARY_BUTTON_PAN   = (1 << 0),
        MIDDLE_BUTTON_PAN    = (1 << 1),
        SPACE_KEY_PAN        = (1 << 2),
        PAN_WITH_SCROLL      = (1 << 3),
        PAN_WITH_MOD_SCROLL  = (1 << 4),
        ZOOM_WITH_SCROLL     = (1 << 5),
        ZOOM_WITH_MOD_SCROLL = (1 << 6),
        // Aggregate masks
        PAN    = PRIMARY_BUTTON_PAN | MIDDLE_BUTTON_PAN | SPACE_KEY_PAN
                 | PAN_WITH_SCROLL | PAN_WITH_MOD_SCROLL,

        ZOOM   = ZOOM_WITH_SCROLL | ZOOM_WITH_MOD_SCROLL,

        SCROLL = PAN_WITH_SCROLL | PAN_WITH_MOD_SCROLL
                 | ZOOM_WITH_SCROLL | ZOOM_WITH_MOD_SCROLL,

        ALL    = PAN | ZOOM
    };
    // clang-format on

    enum class CameraBounds {
        // No bounds
        NONE,
        // Keep camera center inside image bounds
        EDITOR,
        // Keep camera bounds inside image bounds unless image is smaller.
        // Center on axis if an image edge is smaller than the camera edge.
        INSPECTOR_PANEL,
        // In addition to INSPECTOR_PANEL rules, if the 1:1 image is larger
        // than the camera area and zoomed out, restrict the zoom out to fit
        // the screen.
        INSPECTOR_WINDOW
    };

    enum class ZoomMode {
        BASIC,           // Set value directly
        CENTER_CURSOR,   // Set zoom centered on cursor
        PRESERVE_CURSOR  // Set zoom but preserve relative cursor position on screen
    };

    enum class ZoomFitFlags { NONE = 0, ADD_MARGIN = (1 << 0), ALLOW_ZOOM_IN = (1 << 1) };

    Session();

    rt::geom::Rect cameraBBox() const;

    const CameraState& camera() const { return m_camera; }
    WidgetPoint cursorPos() const { return m_cursor_pos; }
    GdkModifierType modifiers() const { return m_modifiers; }
    CursorShape cursorShape() const { return m_cursor_shape; }
    PanZoomFlags panZoomFlags() const { return m_pan_zoom_flags; }
    CameraBounds cameraBounds() const { return m_bound_mode; }
    ZoomMode preferredZoomMode() const { return m_zoom_mode; }

    double minZoom() const { return m_min_zoom; }
    double maxZoom() const { return m_max_zoom; }

    const SpaceTransform<WorldSpace, WidgetSpace>& worldToWidgetTransform() const
    {
        return m_world_to_widget;
    }
    const SpaceTransform<WidgetSpace, WorldSpace>& widgetToWorldTransform() const
    {
        return m_widget_to_world;
    }

    void setCameraPos(WorldPoint pos);
    void setCameraZoom(double zoom, ZoomMode mode = ZoomMode::BASIC);
    void setCameraPosZoom(WorldPoint pos, double zoom);
    void setCameraSize(WidgetSize size);
    void setDeviceScale(int device_scale);
    void setCursorPos(WidgetPoint pos) { m_cursor_pos = pos; }
    void setModifiers(GdkModifierType state) { m_modifiers = state; }
    void setPanZoomFlags(PanZoomFlags flags) { m_pan_zoom_flags = flags; }
    void setCameraBounds(CameraBounds bounds) { m_bound_mode = bounds; }
    void setZoomMode(ZoomMode mode) { m_zoom_mode = mode; }

    // Apply camera bounds before update
    void setCameraBounds(const geom::IntBBox& content, CameraBounds mode);
    void setCamera(const geom::IntBBox& content, const CameraState& new_state);
    void refreshCamera(const geom::IntBBox& content);

    void zoom11();
    void zoomFit(WorldPoint top_left,
                 WorldSize img_size,
                 ZoomFitFlags flags = ZoomFitFlags::NONE);

    void queueDraw() { m_events.signal_queue_draw.emit(); }
    void changeCursorShape(std::optional<CursorShape> shape);

    void onWindowFocusLost(CanvasModel* model);

    CanvasEvents& canvasEvents() { return m_events; }

private:
    CameraState adjustForEditor(const rt::geom::IntBBox& content,
                                const CameraState& new_state);
    CameraState adjustForInspectorPanel(const rt::geom::IntBBox& content,
                                        const CameraState& new_state);
    CameraState adjustForInspectorWindow(const rt::geom::IntBBox& content,
                                         const CameraState& new_state);
    void regenerateTransforms();

    CanvasEvents m_events;

    CameraState m_camera;
    SpaceTransform<WorldSpace, WidgetSpace> m_world_to_widget;
    SpaceTransform<WidgetSpace, WorldSpace> m_widget_to_world;

    WidgetPoint m_cursor_pos;
    GdkModifierType m_modifiers;

    CursorShape m_cursor_shape;
    PanZoomFlags m_pan_zoom_flags;
    CameraBounds m_bound_mode;
    ZoomMode m_zoom_mode;
    double m_min_zoom;
    double m_max_zoom;
};

class ImageModel
{
public:
    const Cairo::RefPtr<Cairo::ImageSurface>& imageSurface() const
    {
        return m_img_surface;
    }

    IntWorldSize fullSize() const { return m_img_size; }

    bool isInsideImage(WorldPoint pos) const;

    void setImageSurface(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                         IntWorldSize img_size)
    {
        m_img_surface = surface;
        m_img_size = img_size;
    }

private:
    Cairo::RefPtr<Cairo::ImageSurface> m_img_surface;
    IntWorldSize m_img_size;
};

class CanvasModel
{
public:
    Session& session() { return m_session; }
    const Session& session() const { return m_session; }

    ImageModel& image() { return m_image_model; }
    const ImageModel& image() const { return m_image_model; }

    bool isCursorInsideImage() const;

    void setCameraPos(WorldPoint pos);
    void setCameraZoom(double zoom, Session::ZoomMode mode = Session::ZoomMode::BASIC);
    void setCameraPosZoom(WorldPoint pos, double zoom);
    void setCameraSize(WidgetSize size);
    void setDeviceScale(int device_scale);
    void setCameraBounds(Session::CameraBounds bounds);
    void refreshCamera();

    void zoomFit(Session::ZoomFitFlags flags = Session::ZoomFitFlags::NONE);

private:
    rt::geom::IntBBox buildImageBBox() const;

    Session m_session;
    ImageModel m_image_model;
};

}  // namespace canvas
}  // namespace rt

template <> struct rt::EnumAsBitflags<rt::canvas::Session::PanZoomFlags> : std::true_type
{
};

template <> struct rt::EnumAsBitflags<rt::canvas::Session::ZoomFitFlags> : std::true_type
{
};
