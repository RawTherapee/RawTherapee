/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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

#include <iomanip>

#include "shcselector.h"

#include "multilangmgr.h"
#include "rtscalable.h"
#include "widgets/curves/mycurve.h"

SHCSelector::SHCSelector() :
    movingPosition(-1),
    tmpX(0.0),
    tmpPos(0.0),
    wslider(0.0),
    cl(nullptr),
    coloredBar(RTO_Left2Right)
{
    positions[0] = defaults[0] = 0.25;
    positions[1] = defaults[1] = 0.5;
    positions[2] = defaults[2] = 0.75;
    leftMargin = static_cast<int>(RTScalable::scalePixelSize(RADIUS - 1.5));
    rightMargin = static_cast<int>(RTScalable::scalePixelSize(RADIUS - 1.5));

    Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    style->add_class("drawingarea");
    style->add_class(GTK_STYLE_CLASS_TROUGH);
    //style->add_class(GTK_STYLE_CLASS_SCALE);
    //style->add_class(GTK_STYLE_CLASS_SLIDER);

    // TODO: This is a hack :) ; change this name to a specific one and create a new entry in all CSS theme files
    set_name("ThresholdSelector");
    set_can_focus(false);
    set_tooltip_text(M("SHCSELECTOR_TOOLTIP"));
}

Gtk::SizeRequestMode SHCSelector::get_request_mode_vfunc() const
{
    return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
}

void SHCSelector::get_preferred_height_vfunc(int& minimum_height,
                                             int& natural_height) const
{
    int minimumWidth = 0;
    int naturalWidth = 0;
    get_preferred_width_vfunc(minimumWidth, naturalWidth);
    get_preferred_height_for_width_vfunc(minimumWidth, minimum_height, natural_height);
}

void SHCSelector::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    minimum_width = RTScalable::scalePixelSize(100);
    natural_width = RTScalable::scalePixelSize(150);
}

void SHCSelector::get_preferred_height_for_width_vfunc(int width,
                                                       int& minimum_height,
                                                       int& natural_height) const
{
    natural_height = minimum_height = RTScalable::scalePixelSize(14);
}

void SHCSelector::get_preferred_width_for_height_vfunc(int height,
                                                       int& minimum_width,
                                                       int& natural_width) const
{
    get_preferred_width_vfunc(minimum_width, natural_width);
}

void SHCSelector::setMargins(int left, int right)
{
    leftMargin = left;
    rightMargin = right;
}

void SHCSelector::setDefaults(double spos, double cpos, double hpos)
{
    defaults[0] = spos;
    defaults[1] = cpos;
    defaults[2] = hpos;
}

void SHCSelector::setPositions(double spos, double cpos, double hpos)
{

    positions[0] = spos;
    positions[1] = cpos;
    positions[2] = hpos;

    queue_draw();
}

void SHCSelector::getPositions(double& spos, double& cpos, double& hpos)
{

    spos = positions[0];
    cpos = positions[1];
    hpos = positions[2];
}

void SHCSelector::on_realize()
{

    Gtk::DrawingArea::on_realize();

    add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK
               | Gdk::BUTTON_RELEASE_MASK);
}

void SHCSelector::updateDrawingArea(const ::Cairo::RefPtr<Cairo::Context>& cr)
{

    if (!get_realized() || !get_width() || !get_height()) {
        return;
    }

    Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

    // Setup drawing
    cr->set_operator(Cairo::OPERATOR_OVER);

    // Get drawing area size
    const int w = get_width() - leftMargin - rightMargin;
    const int h = get_height();

    // Compute slider parameters
    wslider = static_cast<double>(std::max(h / 5, 10));
    const double hwslider = wslider / 2.;

    // Set the box's colors
    cr->set_line_width(1.0);
    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);
    cr->set_line_cap(Cairo::LINE_CAP_BUTT);

    int coloredBarHeight = static_cast<int>(static_cast<double>(h) * 5.5 / 7. + 0.5);
    if (is_sensitive() && coloredBar.canGetColors()) {
        // Gradient background
        coloredBar.setColoredBarSize(leftMargin + 1, 1, w - 2, coloredBarHeight - 2);
        coloredBar.updateColoredBar(cr);
    } else {
        // Style background
        style->render_background(cr, leftMargin + 1, 1, w - 2, coloredBarHeight - 2);
    }

    // Draw the box's borders
    style->render_frame(cr, leftMargin, 0, w, coloredBarHeight);

    // Draw sliders
    for (int i = 0; i < 3; i++) {
        if (i == movingPosition) {
            style->set_state(Gtk::STATE_FLAG_ACTIVE);
        } else if (!is_sensitive()) {
            style->set_state(Gtk::STATE_FLAG_INSENSITIVE);
        } else {
            style->set_state(Gtk::STATE_FLAG_NORMAL);
        }

        style->render_slider(cr,
                             static_cast<double>(leftMargin) + 1.
                                 + (static_cast<double>(w) - 2.) * positions[i]
                                 - static_cast<double>(hwslider),
                             static_cast<double>(vb), wslider,
                             static_cast<double>(h) - static_cast<double>(vb),
                             Gtk::ORIENTATION_VERTICAL);
        style->set_state(Gtk::STATE_FLAG_NORMAL);
    }

    // Draw text for the slider that is being moved
    if (movingPosition >= 0) {
        int i = movingPosition;
        int offset;
        int layout_width = 0, layout_height = 0;

        Glib::RefPtr<Pango::Context> context = get_pango_context();
        Pango::FontDescription fontd(get_style_context()->get_font());

        // Update font
        fontd.set_weight(Pango::WEIGHT_NORMAL);
        const double fontSize = static_cast<double>(h) * 0.8;  // px
        // Absolute size is defined in "Pango units" and shall be multiplied by
        // Pango::SCALE from "px":
        fontd.set_absolute_size(fontSize * static_cast<double>(Pango::SCALE));
        context->set_font_description(fontd);

        Glib::RefPtr<Pango::Layout> layout = create_pango_layout(
            Glib::ustring::format(std::setprecision(2), positions[i]));
        layout->get_pixel_size(layout_width, layout_height);
        offset = positions[i] > 0.5 ? -layout_width - 1 - hwslider : 1 + hwslider;
        cr->set_source_rgb(0., 0., 0.);

        cr->set_line_width(3.);
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);

        cr->move_to(static_cast<double>(leftMargin)
                        + static_cast<double>(w) * positions[i]
                        + static_cast<double>(offset),
                    0.);
        layout->add_to_cairo_context(cr);
        cr->stroke_preserve();
        cr->set_line_width(0.5);
        cr->set_source_rgb(1., 1., 1.);
        cr->fill();
    }
}

bool SHCSelector::on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr)
{
    // Draw drawing area
    // Note: As drawing area surface is updated inside on_draw function, hidpi is automatically supported
    updateDrawingArea(cr);

    return true;
}

bool SHCSelector::on_button_press_event(GdkEventButton* event)
{

    // check if a slider is under the cursor
    const double w = static_cast<double>(get_width() - leftMargin - rightMargin);
    movingPosition = -1;

    for (int i = 0; i < 3; i++) {
        const double currPos =
            static_cast<double>(leftMargin) + 1. + (w - 2.) * positions[i];
        const double hwslider = wslider / 2.;
        if (event->x >= currPos - hwslider && event->x <= currPos + hwslider) {
            movingPosition = i;
            tmpX = event->x;
            tmpPos = positions[i];
            break;
        }
    }

    queue_draw();
    return true;
}

bool SHCSelector::on_button_release_event(GdkEventButton* event)
{

    if (event->button == 1) {
        if (movingPosition >= 0) {
            movingPosition = -1;
            queue_draw();
        }
    } else if (event->button == 3) {
        if (movingPosition >= 0) {
            movingPosition = -1;
        }

        // right mouse button reset the selector to the stored default values
        if (reset()) {
            // rest has modified the values
            if (cl) {
                cl->shcChanged();
            }
        }
    }

    return true;
}

bool SHCSelector::on_motion_notify_event(GdkEventMotion* event)
{

    if (movingPosition >= 0) {
        const double innerw =
            static_cast<double>(get_width() - leftMargin - rightMargin) - 2.;
        positions[movingPosition] = tmpPos + (event->x - tmpX) / innerw;

        if (positions[movingPosition] < 0) {
            positions[movingPosition] = 0.0;
        }

        if (movingPosition > 0
            && positions[movingPosition]
                   < positions[movingPosition - 1] + wslider / innerw)
        {
            positions[movingPosition] = positions[movingPosition - 1] + wslider / innerw;
        }

        if (positions[movingPosition] > 1.0) {
            positions[movingPosition] = 1.0;
        }

        if (movingPosition < 2
            && positions[movingPosition]
                   > positions[movingPosition + 1] - wslider / innerw)
        {
            positions[movingPosition] = positions[movingPosition + 1] - wslider / innerw;
        }

        if (cl) {
            cl->shcChanged();
        }

        queue_draw();
    }

    return true;
}

void SHCSelector::styleChanged(const Glib::RefPtr<Gtk::StyleContext>& style)
{

    queue_draw();
}

bool SHCSelector::reset()  //  : movingPosition(-1), cl(NULL) {
{
    if (positions[0] != defaults[0] || positions[1] != defaults[1]
        || positions[2] != defaults[2])
    {

        positions[0] = defaults[0];
        positions[1] = defaults[1];
        positions[2] = defaults[2];
        queue_draw();
        return true;
    }

    return false;
}

void SHCSelector::refresh()
{
    Glib::RefPtr<Gdk::Window> win = get_window();
    if (win) {
        win->invalidate(true);
    }
}
