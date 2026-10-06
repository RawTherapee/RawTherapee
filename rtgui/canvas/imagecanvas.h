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

#include "canvas.h"

namespace rt::canvas {

class ImageCanvasModel;
class Renderer;

class ImageCanvas final : public Canvas
{
public:
    ImageCanvas(ImageCanvasModel* model);

    ImageCanvasModel* model();
    const ImageCanvasModel* model() const;

    void setRenderer(Renderer* renderer) { m_renderer = renderer; }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    bool onZoomKeyPressed(guint keyval) override;
    std::optional<CursorShape> queryCursorShape() const override;

private:
    Renderer* m_renderer;
};

}  // namespace rt::canvas
