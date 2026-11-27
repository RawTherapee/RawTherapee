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

namespace rt {
namespace canvas {

class ImageRenderer final : public Renderer
{
public:
    void onDraw(const DrawContext& context) override;
};

class DebugRenderer final : public Renderer
{
public:
    enum Flags {
        GRID = (1 << 0),
        WORLD_ORIGIN = (1 << 1),
        CAMERA_ORIGIN = (1 << 2),
        ALL = GRID | WORLD_ORIGIN | CAMERA_ORIGIN,
    };

    DebugRenderer(Flags flags) : m_flags(flags) {}

    void onDraw(const DrawContext& context) override;

private:
    Flags m_flags;
};

class InspectorRenderer final : public Renderer
{
public:
    void onDraw(const DrawContext& context) override;

    void setDrawFrame(bool value) { m_draw_frame = value; }

private:
    ImageRenderer m_image_renderer;
    bool m_draw_frame = false;
};

class EditorRenderer final : public Renderer
{
public:
    EditorRenderer(ImageRenderer* img) : m_image_renderer(img) {}

    void setDebugRenderer(DebugRenderer* r) { m_debug_renderer = r; }

    void onDraw(const DrawContext& context) override;

private:
    ImageRenderer* m_image_renderer;
    DebugRenderer* m_debug_renderer;
};

}  // namespace canvas
}  // namespace rt
