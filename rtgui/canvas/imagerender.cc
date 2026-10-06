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

#include "imagerender.h"

#include "imagemodel.h"

#include "rtengine/math/math.h"

#include <cairomm/context.h>
#include <gtkmm/widget.h>

using namespace rt;
using namespace rt::canvas;

namespace {

template <class F> void draw(const DrawContext& context, F&& draw_func)
{
    context.cr->save();
    draw_func();
    context.cr->restore();
}

void drawBackground(const DrawContext& context)
{
    const ImageCanvasModel* model = context.model;
    const auto& cr = context.cr;

    WidgetSize allocated_size = model->session()->camera().size;

    context.canvas->get_style_context()->render_background(
        cr, 0, 0, allocated_size.width.value(), allocated_size.height.value());
}

void drawFrame(const DrawContext& context)
{
    const ImageCanvasModel* model = context.model;
    const auto& cr = context.cr;

    WidgetSize allocated_size = model->session()->camera().size;

    auto color =
        context.canvas->get_style_context()->get_border_color(Gtk::STATE_FLAG_NORMAL);

    cr->set_source_rgb(color.get_red(), color.get_green(), color.get_blue());
    cr->set_line_width(1);
    cr->rectangle(0.5, 0.5, allocated_size.width.value() - 1,
                  allocated_size.height.value() - 1);
    cr->stroke();
}

}  // namespace

void ImageRenderer::onDraw(const DrawContext& context)
{
    const ImageCanvasModel* model = context.model;
    const auto& cr = context.cr;

    if (!model->image().imageSurface()) return;

    const Session* session = model->session();

    cr->transform(session->worldToWidgetTransform().matrix());

    auto pattern = Cairo::SurfacePattern::create(model->image().imageSurface());
    if (session->camera().zoom >= 1.0) {
        pattern->set_filter(Cairo::FILTER_NEAREST);
        pattern->set_extend(Cairo::EXTEND_NONE);
        cr->set_antialias(Cairo::ANTIALIAS_NONE);
    } else {
        pattern->set_filter(Cairo::FILTER_BILINEAR);
    }
    cr->set_source(pattern);
    cr->paint();
}

void DebugRenderer::onDraw(const DrawContext& context)
{
    const ImageCanvasModel* model = context.model;
    const auto& cr = context.cr;

    const Session* session = model->session();

    cr->save();
    cr->transform(session->worldToWidgetTransform().matrix());

    if (m_flags & Flags::GRID) {
        cr->set_source_rgba(0, 0, 0, 0.4);
        for (int i = -1000; i <= 1000; i += 100) {
            cr->move_to(i, -1000);
            cr->line_to(i, 1000);

            cr->move_to(-1000, i);
            cr->line_to(1000, i);
        }
        cr->stroke();
    }

    if (m_flags & Flags::WORLD_ORIGIN) {
        // X axis (red)
        cr->set_source_rgb(1, 0, 0);
        cr->move_to(0, 0);
        cr->line_to(100, 0);
        cr->stroke();

        // Y axis (green)
        cr->set_source_rgb(0, 1, 0);
        cr->move_to(0, 0);
        cr->line_to(0, 100);
        cr->stroke();

        // Origin
        cr->arc(0, 0, 3, 0, 2 * rt::numbers::pi);
        cr->set_source_rgb(1, 1, 0);
        cr->fill();
    }

    cr->restore();

    if (m_flags & Flags::CAMERA_ORIGIN) {
        const CameraState& camera = session->camera();

        cr->transform(session->worldToWidgetTransform().matrix());
        cr->arc(camera.pos.x.value(), camera.pos.y.value(), 3 / camera.zoom, 0,
                2 * rt::numbers::pi);
        cr->set_source_rgb(0, 1, 1);
        cr->fill();
    }
}

void InspectorRenderer::onDraw(const DrawContext& context)
{
    draw(context, [&]() { drawBackground(context); });
    draw(context, [&]() { m_image_renderer.onDraw(context); });

    if (m_draw_frame) {
        draw(context, [&]() { drawFrame(context); });
    }
}

void EditorRenderer::onDraw(const DrawContext& context)
{
    draw(context, [&]() { drawBackground(context); });
    draw(context, [&]() {
        if (m_image_renderer) {
            m_image_renderer->onDraw(context);
        }
    });
    draw(context, [&]() {
        if (m_debug_renderer) {
            m_debug_renderer->onDraw(context);
        }
    });
}
