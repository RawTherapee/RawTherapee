/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "svgrender.h"

#ifdef SVG_BACKEND_RSVG
#include <librsvg/rsvg.h>
#elif SVG_BACKEND_LUNASVG
#include <lunasvg.h>
#else
#error "Unknown SVG rendering backend"
#endif

#include <cairomm/context.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace {

#ifdef SVG_BACKEND_RSVG

Cairo::RefPtr<Cairo::ImageSurface> renderWithRsvg(
    const std::string& data, int logical_width, int logical_height, int device_scale)
{
    unsigned const char* const cdata = static_cast<const unsigned char*>(
        static_cast<const void*>(data.c_str()));

    GError* error = nullptr;
    RsvgHandle* handle = rsvg_handle_new_from_data(cdata, data.size(), &error);

    if (error) {
        rtengine::SvgRenderException e(error->message);
        g_error_free(error);
        throw e;
    }

    int w = 0;
    int h = 0;

    if (logical_width < 0 || logical_height < 0) {
        // Use SVG image natural width and height
        double intrinsic_w = 0.0, intrinsic_h = 0.0;
        const bool has_dim = rsvg_handle_get_intrinsic_size_in_pixels(
            handle, &intrinsic_w, &intrinsic_h);

        if (has_dim) {
            w = std::ceil(intrinsic_w);
            h = std::ceil(intrinsic_h);
        } else {
            // Set to a default size of 16px (i.e. Gtk::ICON_SIZE_SMALL_TOOLBAR one)
            w = h = 16;
        }
    } else {
        // Use given width and height
        w = logical_width;
        h = logical_height;
    }

    // Create an upscaled surface to avoid blur effect
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32,
                                               w * device_scale,
                                               h * device_scale);

    // Render (and erase with) default surface background
    Cairo::RefPtr<Cairo::Context> c = Cairo::Context::create(surface);
    c->set_source_rgba(0., 0., 0., 0.);
    c->set_operator(Cairo::OPERATOR_CLEAR);
    c->paint();

    // Render upscaled surface based on SVG image
    error = nullptr;
    RsvgRectangle rect = {
        .x = 0.,
        .y = 0.,
        .width = static_cast<double>(w * device_scale),
        .height = static_cast<double>(h * device_scale)
    };
    c->set_operator(Cairo::OPERATOR_OVER);
    const bool success = rsvg_handle_render_document(handle, c->cobj(), &rect, &error);
    g_object_unref(handle);

    if (!success && error) {
        rtengine::SvgRenderException e(error->message);
        g_error_free(error);
        throw e;
    }

    // Set device scale to avoid blur effect
    cairo_surface_set_device_scale(surface->cobj(),
        static_cast<double>(device_scale),
        static_cast<double>(device_scale));

    return surface;
}

#elif SVG_BACKEND_LUNASVG

Cairo::RefPtr<Cairo::ImageSurface> renderWithLunaSvg(
    const std::string& data, int logical_width, int logical_height, int device_scale)
{
    std::unique_ptr<lunasvg::Document> document = lunasvg::Document::loadFromData(data);
    if (!document) {
        throw rtengine::SvgRenderException("Failed to parse SVG");
    }

    int w = 0;
    int h = 0;

    if (logical_width < 0 || logical_height < 0) {
        // Use SVG image natural width and height
        float intrinsic_w = document->width();
        float intrinsic_h = document->height();

        if (intrinsic_w > 0 && intrinsic_h > 0) {
            w = std::ceil(intrinsic_w);
            h = std::ceil(intrinsic_h);
        } else {
            // Set to a default size of 16px (i.e. Gtk::ICON_SIZE_SMALL_TOOLBAR one)
            w = h = 16;
        }
    } else {
        // Use given width and height
        w = logical_width;
        h = logical_height;
    }

    const int scaled_width = w * device_scale;
    const int scaled_height = h * device_scale;

    lunasvg::Bitmap bitmap = document->renderToBitmap(scaled_width, scaled_height);

    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, scaled_width,
                                               scaled_height);
    cairo_surface_set_device_scale(surface->cobj(),
        static_cast<double>(device_scale),
        static_cast<double>(device_scale));

    uint8_t* const bitmap_data = bitmap.data();
    surface->flush();
    unsigned char* const surface_data = surface->get_data();

    constexpr int NUM_CHANNELS = 4;
    for (int y = 0; y < scaled_height; y++) {
        const int bitmap_row = y * bitmap.stride();
        const int surface_row = y * surface->get_stride();

        for (int x = 0; x < scaled_width; x++) {
            const int pixel = NUM_CHANNELS * x;

            for (int c = 0; c < NUM_CHANNELS; c++) {
                surface_data[surface_row + pixel + c] =
                    bitmap_data[bitmap_row  + pixel + c];
            }
        }
    }

    surface->mark_dirty();
    return surface;
}

#endif // SVG_BACKEND_LUNASVG

} // namespace

namespace rtengine {

Cairo::RefPtr<Cairo::ImageSurface> renderSvg(const std::string& data, int logical_width,
                                             int logical_height, int device_scale)
{
#ifdef SVG_BACKEND_RSVG
    return renderWithRsvg(data, logical_width, logical_height, device_scale);
#elif SVG_BACKEND_LUNASVG
    return renderWithLunaSvg(data, logical_width, logical_height, device_scale);
#else
    return nullptr;
#endif
}

} // namespace rtengine
