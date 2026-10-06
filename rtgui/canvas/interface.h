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

#include "cursormanager.h"  // For CursorShape

#include "rtengine/util/enum.h"

#include <sigc++/sigc++.h>

#include <optional>

namespace Cairo {
class Context;
}

namespace Gtk {
class Widget;
}  // namespace Gtk

namespace rt {
namespace canvas {

class MouseGesture;

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

enum class ZoomMode {
    BASIC,           // Set value directly
    CENTER_CURSOR,   // Set zoom centered on cursor
    PRESERVE_CURSOR  // Set zoom but preserve relative cursor position on screen
};

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
    virtual ~Session() = default;

    virtual const CameraState& camera() const = 0;
    virtual WidgetPoint cursorPos() const = 0;
    virtual CursorShape cursorShape() const = 0;
    virtual GdkModifierType modifiers() const = 0;

    virtual PanZoomFlags panZoomFlags() const = 0;
    virtual ZoomMode zoomMode() const = 0;

    virtual double minZoom() const = 0;
    virtual double maxZoom() const = 0;

    virtual const SpaceTransform<WorldSpace, WidgetSpace>&
    worldToWidgetTransform() const = 0;
    virtual const SpaceTransform<WidgetSpace, WorldSpace>&
    widgetToWorldTransform() const = 0;

    virtual CanvasEvents& canvasEvents() = 0;

    virtual void setCameraPos(WorldPoint pos) = 0;
    virtual void setCameraZoom(double zoom, ZoomMode mode = ZoomMode::BASIC) = 0;
    virtual void setCameraPosZoom(WorldPoint pos, double zoom) = 0;
    virtual void setCameraSize(WidgetSize size) = 0;
    virtual void setDeviceScale(int device_scale) = 0;

    virtual void setCursorPos(WidgetPoint pos) = 0;
    virtual void setModifiers(GdkModifierType state) = 0;

    virtual void setPanZoomFlags(PanZoomFlags flags) = 0;
    virtual void setZoomMode(ZoomMode mode) = 0;

    virtual void changeCursorShape(std::optional<CursorShape> shape) = 0;
};

class CanvasModel
{
public:
    virtual ~CanvasModel() = default;

    virtual Session* session() = 0;
    virtual const Session* session() const = 0;

    virtual void setCameraPos(WorldPoint pos) = 0;
    virtual void setCameraZoom(double zoom, ZoomMode mode = ZoomMode::BASIC) = 0;
    virtual void setCameraPosZoom(WorldPoint pos, double zoom) = 0;
    virtual void setCameraSize(WidgetSize size) = 0;
    virtual void setDeviceScale(int device_scale) = 0;
    virtual void refreshCamera() = 0;
};

class CursorMonitor
{
public:
    virtual ~CursorMonitor() = default;

    virtual void onEnter(const CanvasModel* model, WidgetPoint pos) {}
    virtual void onMotion(const CanvasModel* model, WidgetPoint pos) {}
    virtual void onLeave(const CanvasModel* model) {}
};

}  // namespace canvas
}  // namespace rt

template <> struct rt::EnumAsBitflags<rt::canvas::PanZoomFlags> : std::true_type
{
};
