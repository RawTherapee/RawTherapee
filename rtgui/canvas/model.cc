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

#include "model.h"

#include "cursormanager.h"

#include "rtengine/math/math.h"
#include "rtengine/math/pointvec.h"

using namespace rt;
using namespace rt::canvas;

Session::Session() :
    m_modifiers(GdkModifierType(0)),
    m_cursor_shape(CSArrow),
    m_pan_zoom_flags(PanZoomFlags::ALL),
    m_bound_mode(CameraBounds::NONE),
    m_zoom_mode(ZoomMode::BASIC),
    m_min_zoom(0.01),
    m_max_zoom(256.0)
{
    regenerateTransforms();
}

geom::Rect Session::cameraBBox() const
{
    WorldSize size = m_widget_to_world(m_camera.size);
    WorldVec vec = size.asVec() / 2.0;

    geom::Point top_left = static_cast<geom::Point>(m_camera.pos - vec);
    geom::Point bot_right = static_cast<geom::Point>(m_camera.pos + vec);

    return rt::geom::Rect(top_left, bot_right);
}

void Session::setCameraPos(WorldPoint pos)
{
    if (m_camera.pos == pos) return;

    m_camera.pos = pos;
    regenerateTransforms();
    m_events.signal_camera_update.emit();
    queueDraw();
}

void Session::setCameraZoom(double zoom, Session::ZoomMode mode)
{
    if (m_camera.zoom == zoom) return;

    const double old_zoom = m_camera.zoom;
    const double new_zoom = rt::clamp(zoom, m_min_zoom, m_max_zoom);

    switch (mode) {
        case ZoomMode::PRESERVE_CURSOR:
        {
            const double scale = new_zoom / old_zoom;

            WorldPoint anchor_pos = m_widget_to_world(m_cursor_pos);
            WorldVec from_center = anchor_pos - m_camera.pos;
            WorldPoint new_pos = anchor_pos - from_center / scale;

            m_camera.pos = new_pos;
            m_camera.zoom = new_zoom;
            break;
        }
        case ZoomMode::CENTER_CURSOR:
            m_camera.pos = m_widget_to_world(m_cursor_pos);
            m_camera.zoom = new_zoom;
            break;
        case ZoomMode::BASIC:
        default:
            m_camera.zoom = new_zoom;
            break;
    }

    regenerateTransforms();
    m_events.signal_camera_update.emit();
    queueDraw();
}

void Session::setCameraPosZoom(WorldPoint pos, double zoom)
{
    if (m_camera.pos == pos && m_camera.zoom == zoom) return;

    m_camera.pos = pos;
    m_camera.zoom = rt::clamp(zoom, m_min_zoom, m_max_zoom);
    regenerateTransforms();
    m_events.signal_camera_update.emit();
    queueDraw();
}

void Session::setCameraSize(WidgetSize size)
{
    if (m_camera.size == size) return;

    m_camera.size = size;
    regenerateTransforms();
    m_events.signal_camera_update.emit();
    queueDraw();
}

void Session::setDeviceScale(int device_scale)
{
    if (m_camera.device_scale == device_scale) return;

    m_camera.device_scale = device_scale;
    regenerateTransforms();
    m_events.signal_camera_update.emit();
    queueDraw();
}

void Session::setCameraBounds(const geom::IntBBox& content, CameraBounds bounds)
{
    if (m_bound_mode == bounds) return;
    m_bound_mode = bounds;
    refreshCamera(content);
}

void Session::setCamera(const geom::IntBBox& content, const CameraState& new_state)
{
    switch (m_bound_mode) {
        case CameraBounds::EDITOR:
            m_camera = adjustForEditor(content, new_state);
            break;
        case CameraBounds::INSPECTOR_PANEL:
            m_camera = adjustForInspectorPanel(content, new_state);
            break;
        case CameraBounds::INSPECTOR_WINDOW:
            m_camera = adjustForInspectorWindow(content, new_state);
            break;
        case CameraBounds::NONE:
        default:
            m_camera = new_state;
            m_camera.zoom = rt::clamp(m_camera.zoom, m_min_zoom, m_max_zoom);
            break;
    }

    regenerateTransforms();
    m_events.signal_camera_update.emit();
    queueDraw();
}

CameraState Session::adjustForEditor(const geom::IntBBox& content,
                                     const CameraState& new_state)
{
    CameraState adjusted = new_state;
    adjusted.zoom = rt::clamp(new_state.zoom, m_min_zoom, m_max_zoom);

    const double content_width = content.width();
    const double content_height = content.height();
    if (content_width <= 0 || content_height <= 0) return adjusted;

    // Prevent image edge from crossing center of camera
    const double min_x = content.min().x;
    const double max_x = content.max().x;
    if (new_state.pos.x.value() < min_x) {
        adjusted.pos.x = WorldScalar(min_x);
    } else if (new_state.pos.x.value() > max_x) {
        adjusted.pos.x = WorldScalar(max_x);
    }

    const double min_y = content.min().y;
    const double max_y = content.max().y;
    if (new_state.pos.y.value() < min_y) {
        adjusted.pos.y = WorldScalar(min_y);
    } else if (new_state.pos.y.value() > max_y) {
        adjusted.pos.y = WorldScalar(max_y);
    }

    return adjusted;
}

CameraState Session::adjustForInspectorPanel(const geom::IntBBox& content,
                                             const CameraState& new_state)
{
    CameraState adjusted = new_state;
    adjusted.zoom = rt::clamp(new_state.zoom, m_min_zoom, m_max_zoom);

    const double content_width = content.width();
    const double content_height = content.height();
    if (content_width <= 0 || content_height <= 0) return adjusted;

    const double camera_width =
        new_state.size.width.value() / adjusted.zoom * new_state.device_scale;
    const double camera_height =
        new_state.size.height.value() / adjusted.zoom * new_state.device_scale;

    if (content_width <= camera_width) {
        // Center in camera
        adjusted.pos.x = WorldScalar(content.min().x + content_width / 2.0);
    } else {
        // Prevent image edge from crossing widget edges
        const double margin_x = camera_width / adjusted.zoom / 2.0;
        const double min_x = content.min().x + margin_x;
        const double max_x = content.max().x - margin_x;
        if (new_state.pos.x.value() < min_x) {
            adjusted.pos.x = WorldScalar(min_x);
        } else if (new_state.pos.x.value() > max_x) {
            adjusted.pos.x = WorldScalar(max_x);
        }
    }

    if (content_height <= camera_height) {
        // Center in camera
        adjusted.pos.y = WorldScalar(content.min().y + content_height / 2.0);
    } else {
        // Prevent image edge from crossing widget edges
        const double margin_y = camera_height / adjusted.zoom / 2.0;
        const double min_y = content.min().y + margin_y;
        const double max_y = content.max().y - margin_y;
        if (new_state.pos.y.value() < min_y) {
            adjusted.pos.y = WorldScalar(min_y);
        } else if (new_state.pos.y.value() > max_y) {
            adjusted.pos.y = WorldScalar(max_y);
        }
    }

    return adjusted;
}

CameraState Session::adjustForInspectorWindow(const geom::IntBBox& content,
                                              const CameraState& new_state)
{
    CameraState adjusted = new_state;
    adjusted.zoom = rt::clamp(new_state.zoom, m_min_zoom, m_max_zoom);

    const double content_width = content.width();
    const double content_height = content.height();
    if (content_width <= 0 || content_height <= 0) return adjusted;

    const double width = new_state.size.width.value() * new_state.device_scale;
    const double height = new_state.size.height.value() * new_state.device_scale;
    const double fit_zoom = rt::min(width / content_width, height / content_height);

    const bool limit11 = content_width <= width && content_height <= height;

    if (adjusted.zoom < 1.0) {
        if (limit11) {
            adjusted.zoom = 1.0;
        } else if (adjusted.zoom < fit_zoom) {
            adjusted.zoom = rt::clamp(fit_zoom, m_min_zoom, m_max_zoom);
        }
    }

    // Prevent image edge from crossing widget edges
    if (content_width > (width / adjusted.zoom)) {
        const double margin_x = width / adjusted.zoom / 2.0;
        const double min_x = content.min().x + margin_x;
        const double max_x = content.max().x - margin_x;
        if (new_state.pos.x.value() < min_x) {
            adjusted.pos.x = WorldScalar(min_x);
        } else if (new_state.pos.x.value() > max_x) {
            adjusted.pos.x = WorldScalar(max_x);
        }
    } else {
        adjusted.pos.x = WorldScalar(content.min().x + content_width / 2.0);
    }

    if (content_height > (height / adjusted.zoom)) {
        const double margin_y = height / adjusted.zoom / 2.0;
        const double min_y = content.min().y + margin_y;
        const double max_y = content.max().y - margin_y;
        if (new_state.pos.y.value() < min_y) {
            adjusted.pos.y = WorldScalar(min_y);
        } else if (new_state.pos.y.value() > max_y) {
            adjusted.pos.y = WorldScalar(max_y);
        }
    } else {
        adjusted.pos.y = WorldScalar(content.min().y + content_height / 2.0);
    }

    return adjusted;
}

void Session::refreshCamera(const geom::IntBBox& content)
{
    setCamera(content, m_camera);
}

void Session::changeCursorShape(std::optional<CursorShape> shape)
{
    if (shape) {
        m_cursor_shape = *shape;
    }
    m_events.signal_change_cursor.emit(shape);
}

void Session::zoom11()
{
    setCameraZoom(1.0, preferredZoomMode());
}

void Session::zoomFit(WorldPoint top_left, WorldSize img_size, ZoomFitFlags flags)
{
    WorldPoint center = top_left + (img_size.asVec()) / 2.0;

    double device_scale = m_camera.device_scale;
    double bounds_x = m_camera.size.width.value();
    double bounds_y = m_camera.size.height.value();

    if (rt::any(flags & ZoomFitFlags::ADD_MARGIN)) {
        bounds_x = rt::max(bounds_x - 20, bounds_x * 0.95);
        bounds_y = rt::max(bounds_y - 20, bounds_y * 0.95);
    }

    // Convert to world space without zoom
    bounds_x *= device_scale;
    bounds_y *= device_scale;

    double zoom_x = bounds_x / img_size.width.value();
    double zoom_y = bounds_y / img_size.height.value();
    double zoom = rt::min(zoom_x, zoom_y);
    if (zoom > 1.0 && rt::none(flags & ZoomFitFlags::ALLOW_ZOOM_IN)) {
        zoom = 1.0;
    }

    setCameraPosZoom(center, zoom);
}

void Session::onWindowFocusLost(CanvasModel* model)
{
    m_modifiers = GdkModifierType(0);
}

void Session::regenerateTransforms()
{
    m_world_to_widget = SpaceTransform<WorldSpace, WidgetSpace>::build(m_camera);
    m_widget_to_world = SpaceTransform<WidgetSpace, WorldSpace>::build(m_camera);
}

bool ImageModel::isInsideImage(WorldPoint pos) const
{
    if (!m_img_surface) return false;

    geom::BBox bbox(geom::Point(0, 0),
                    geom::Point(m_img_size.width.value(), m_img_size.height.value()));
    return bbox.contains(static_cast<geom::Point>(pos));
}

bool CanvasModel::isCursorInsideImage() const
{
    WorldPoint pos = m_session.widgetToWorldTransform()(m_session.cursorPos());
    return m_image_model.isInsideImage(pos);
}

void CanvasModel::setCameraPos(WorldPoint pos)
{
    CameraState camera = m_session.camera();
    if (camera.pos == pos) return;

    camera.pos = pos;
    m_session.setCamera(buildImageBBox(), camera);
}

void CanvasModel::setCameraZoom(double zoom, Session::ZoomMode mode)
{
    if (m_session.camera().zoom == zoom) return;

    m_session.setCameraZoom(zoom, mode);
    refreshCamera();
}

void CanvasModel::setCameraPosZoom(WorldPoint pos, double zoom)
{
    CameraState camera = m_session.camera();
    if (camera.pos == pos && camera.zoom == zoom) return;

    camera.pos = pos;
    camera.zoom = zoom;
    m_session.setCamera(buildImageBBox(), camera);
}

void CanvasModel::setCameraSize(WidgetSize size)
{
    CameraState camera = m_session.camera();
    if (camera.size == size) return;

    camera.size = size;
    m_session.setCamera(buildImageBBox(), camera);
}

void CanvasModel::setDeviceScale(int device_scale)
{
    CameraState camera = m_session.camera();
    if (camera.device_scale == device_scale) return;

    camera.device_scale = device_scale;
    m_session.setCamera(buildImageBBox(), camera);
}

void CanvasModel::setCameraBounds(Session::CameraBounds bounds)
{
    if (m_session.cameraBounds() == bounds) return;
    m_session.setCameraBounds(buildImageBBox(), bounds);
}

void CanvasModel::refreshCamera()
{
    m_session.refreshCamera(buildImageBBox());
}

void CanvasModel::zoomFit(Session::ZoomFitFlags flags)
{
    if (!m_image_model.imageSurface()) return;

    m_session.zoomFit(WorldPoint{}, static_cast<WorldSize>(m_image_model.fullSize()),
                      flags);
}

geom::IntBBox CanvasModel::buildImageBBox() const
{
    return geom::IntBBox(geom::IntPoint(),
                         static_cast<geom::IntPoint>(m_image_model.fullSize().asPoint()));
}
