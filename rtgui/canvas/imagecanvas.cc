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

#include "imagecanvas.h"

#include "imagemodel.h"
#include "imagerender.h"

namespace rt::canvas {

ImageCanvas::ImageCanvas(ImageCanvasModel* model) : Canvas(model), m_renderer(nullptr) {}

ImageCanvasModel* ImageCanvas::model()
{
    return static_cast<ImageCanvasModel*>(Canvas::model());
}

const ImageCanvasModel* ImageCanvas::model() const
{
    return static_cast<const ImageCanvasModel*>(Canvas::model());
}

bool ImageCanvas::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!m_renderer) return true;

    DrawContext context(this, model(), cr);
    cr->save();
    m_renderer->onDraw(context);
    cr->restore();

    return true;
}

bool ImageCanvas::onZoomKeyPressed(guint keyval)
{
    EditorSession* session = model()->session();

    if (keyval == GDK_KEY_z) {
        session->zoom11();
        return true;
    } else if (keyval == GDK_KEY_f) {
        const ImageModel& image = model()->image();
        // TODO: Zoom to crop when alt not held down
        if (session->modifiers() & GDK_MOD1_MASK) {
            session->zoomFit(WorldPoint{}, static_cast<WorldSize>(image.fullSize()),
                             EditorSession::ZoomFitFlags::ADD_MARGIN
                                 | EditorSession::ZoomFitFlags::ALLOW_ZOOM_IN);
        } else {
            session->zoomFit(WorldPoint{}, static_cast<WorldSize>(image.fullSize()),
                             EditorSession::ZoomFitFlags::ADD_MARGIN
                                 | EditorSession::ZoomFitFlags::ALLOW_ZOOM_IN);
        }
        return true;
    } else {
        return false;
    }
}

std::optional<CursorShape> ImageCanvas::queryCursorShape() const
{
    return model()->isCursorInsideImage() ? CSCrosshair : CSArrow;
}

}  // namespace rt::canvas
