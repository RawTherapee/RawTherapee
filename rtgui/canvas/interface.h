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

#include <cairomm/refptr.h>

namespace Cairo {
class Context;
}

namespace Gtk {

class Widget;

}  // namespace Gtk

namespace rt {
namespace canvas {

class CanvasModel;
class MouseGesture;

struct ClickContext
{
    CanvasModel* model;
    const MouseGesture* controller;

    ClickContext(CanvasModel* m, const MouseGesture* c) : model(m), controller(c) {}
};

struct KeyContext
{
    CanvasModel* model;

    KeyContext(CanvasModel* m) : model(m) {}
};

struct DrawContext
{
    const CanvasModel* model;
    Cairo::RefPtr<Cairo::Context> cr;
    Gtk::Widget* canvas;

    DrawContext(Gtk::Widget* widget,
                CanvasModel* m,
                const Cairo::RefPtr<Cairo::Context>& cairo) :
        model(m), cr(cairo), canvas(widget)
    {
    }
};

class CursorMonitor
{
public:
    virtual ~CursorMonitor() = default;

    virtual void onEnter(const CanvasModel* model, WidgetPoint pos) {}
    virtual void onMotion(const CanvasModel* model, WidgetPoint pos) {}
    virtual void onLeave(const CanvasModel* model) {}
};

class Renderer
{
public:
    virtual ~Renderer() = default;

    virtual void onDraw(const DrawContext& context) = 0;
};

}  // namespace canvas
}  // namespace rt
