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

#include "rtengine/math/rect.h"
#include "rtengine/util/enum.h"

#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <gdk/gdk.h>

#include <memory>
#include <optional>

namespace rt {
namespace canvas {

class ImageCanvasModel;

class EditorSession : public Session
{
public:
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

    enum class ZoomFitFlags { NONE = 0, ADD_MARGIN = (1 << 0), ALLOW_ZOOM_IN = (1 << 1) };

    EditorSession();

    rt::geom::Rect cameraBBox() const;

    const CameraState& camera() const override { return m_camera; }
    WidgetPoint cursorPos() const override { return m_cursor_pos; }
    GdkModifierType modifiers() const override { return m_modifiers; }
    CursorShape cursorShape() const override { return m_cursor_shape; }
    PanZoomFlags panZoomFlags() const override { return m_pan_zoom_flags; }
    ZoomMode zoomMode() const override { return m_zoom_mode; }

    double minZoom() const override { return m_min_zoom; }
    double maxZoom() const override { return m_max_zoom; }

    CameraBounds cameraBounds() const { return m_bound_mode; }

    const SpaceTransform<WorldSpace, WidgetSpace>& worldToWidgetTransform() const override
    {
        return m_world_to_widget;
    }
    const SpaceTransform<WidgetSpace, WorldSpace>& widgetToWorldTransform() const override
    {
        return m_widget_to_world;
    }

    void setCameraPos(WorldPoint pos) override;
    void setCameraZoom(double zoom, ZoomMode mode = ZoomMode::BASIC) override;
    void setCameraPosZoom(WorldPoint pos, double zoom) override;
    void setCameraSize(WidgetSize size) override;
    void setDeviceScale(int device_scale) override;
    void setCursorPos(WidgetPoint pos) override { m_cursor_pos = pos; }
    void setModifiers(GdkModifierType state) override { m_modifiers = state; }
    void setPanZoomFlags(PanZoomFlags flags) override { m_pan_zoom_flags = flags; }
    void setZoomMode(ZoomMode mode) override { m_zoom_mode = mode; }

    void setCameraBounds(CameraBounds bounds) { m_bound_mode = bounds; }
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

class ImageCanvasModel : public CanvasModel
{
public:
    EditorSession* session() override { return &m_session; }
    const EditorSession* session() const override { return &m_session; }

    ImageModel& image() { return m_image_model; }
    const ImageModel& image() const { return m_image_model; }

    bool isCursorInsideImage() const;

    void setCameraPos(WorldPoint pos) override;
    void setCameraZoom(double zoom, ZoomMode mode = ZoomMode::BASIC) override;
    void setCameraPosZoom(WorldPoint pos, double zoom) override;
    void setCameraSize(WidgetSize size) override;
    void setDeviceScale(int device_scale) override;
    void refreshCamera() override;

    void setCameraBounds(EditorSession::CameraBounds bounds);

    void zoomFit(EditorSession::ZoomFitFlags flags = EditorSession::ZoomFitFlags::NONE);

private:
    rt::geom::IntBBox buildImageBBox() const;

    EditorSession m_session;
    ImageModel m_image_model;
};

}  // namespace canvas
}  // namespace rt

template <>
struct rt::EnumAsBitflags<rt::canvas::EditorSession::ZoomFitFlags> : std::true_type
{
};
