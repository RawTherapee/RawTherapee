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

#include <cassert>
#include <cmath>

#include "thresholdselector.h"

#include "multilangmgr.h"
#include "rtscalable.h"
#include "widgets/curves/mycurve.h"

#include "rtengine/procparams.h"

ThresholdSelector::ThresholdSelector(double minValueBottom,
                                     double maxValueBottom,
                                     double defBottom,
                                     Glib::ustring labelBottom,
                                     unsigned int precisionBottom,
                                     double minValueTop,
                                     double maxValueTop,
                                     double defTop,
                                     Glib::ustring labelTop,
                                     unsigned int precisionTop,
                                     ThresholdCurveProvider* curveProvider) :
    separatedLabelBottom(std::move(labelBottom)),
    separatedLabelTop(std::move(labelTop)),
    coloredBar(RTO_Left2Right)
{
    positions[TS_BOTTOMLEFT] = defPos[TS_BOTTOMLEFT] = defBottom;
    positions[TS_TOPLEFT] = defPos[TS_TOPLEFT] = defTop;
    positions[TS_BOTTOMRIGHT] = defPos[TS_BOTTOMRIGHT] = 0;  // unused
    positions[TS_TOPRIGHT] = defPos[TS_TOPRIGHT] = 0;        // unused
    this->precisionTop = precisionTop;
    this->precisionBottom = precisionBottom;
    doubleThresh = false;

    bgCurveProvider = curveProvider;
    separatedSliders = true;
    initalEq1 = false;  // unused
    minValBottom = minValueBottom;
    maxValBottom = maxValueBottom;
    minValTop = minValueTop;
    maxValTop = maxValueTop;

    initValues();
}

ThresholdSelector::ThresholdSelector(double minValue,
                                     double maxValue,
                                     double defBottom,
                                     double defTop,
                                     unsigned int precision,
                                     bool startAtOne) :
    coloredBar(RTO_Left2Right)
{
    positions[TS_BOTTOMLEFT] = defPos[TS_BOTTOMLEFT] = defBottom;
    positions[TS_TOPLEFT] = defPos[TS_TOPLEFT] = defTop;
    positions[TS_BOTTOMRIGHT] = defPos[TS_BOTTOMRIGHT] = maxValue;
    positions[TS_TOPRIGHT] = defPos[TS_TOPRIGHT] = maxValue;
    this->precisionTop = precision;
    this->precisionBottom = precision;
    doubleThresh = false;

#ifndef NDEBUG

    if (startAtOne) {
        assert(defBottom >= defTop);
        assert(defTop >= minValue);
        assert(defBottom <= maxValue);
    } else {
        assert(defTop >= defBottom);
        assert(defBottom >= minValue);
        assert(defTop <= maxValue);
    }

    assert(minValue < maxValue);
#endif

    bgCurveProvider = nullptr;
    separatedSliders = false;
    initalEq1 = startAtOne;
    minValTop = minValBottom = minValue;
    maxValTop = maxValBottom = maxValue;

    initValues();
}

ThresholdSelector::ThresholdSelector(double minValue,
                                     double maxValue,
                                     double defBottomLeft,
                                     double defTopLeft,
                                     double defBottomRight,
                                     double defTopRight,
                                     unsigned int precision,
                                     bool startAtOne) :
    coloredBar(RTO_Left2Right)
{
    positions[TS_BOTTOMLEFT] = defPos[TS_BOTTOMLEFT] = defBottomLeft;
    positions[TS_TOPLEFT] = defPos[TS_TOPLEFT] = defTopLeft;
    positions[TS_BOTTOMRIGHT] = defPos[TS_BOTTOMRIGHT] = defBottomRight;
    positions[TS_TOPRIGHT] = defPos[TS_TOPRIGHT] = defTopRight;
    this->precisionTop = precision;
    this->precisionBottom = precision;
    doubleThresh = true;

#ifndef NDEBUG

    if (startAtOne) {
        assert(minValue <= defTopLeft);
        assert(defTopLeft <= defBottomLeft);
        assert(defBottomLeft <= defBottomRight);
        assert(defBottomRight <= defTopRight);
        assert(defTopRight <= maxValue);
    } else {
        assert(minValue <= defBottomLeft);
        assert(defBottomLeft <= defTopLeft);
        assert(defTopLeft <= defTopRight);
        assert(defTopRight <= defBottomRight);
        assert(defBottomRight <= maxValue);
    }

    assert(minValue < maxValue);
#endif

    bgCurveProvider = nullptr;
    separatedSliders = false;
    initalEq1 = startAtOne;
    minValTop = minValBottom = minValue;
    maxValTop = maxValBottom = maxValue;

    initValues();
}

void ThresholdSelector::initValues()
{

    updatePolicy = RTUP_STATIC;
    additionalTTip = "";
    oldLitCursor = litCursor = TS_UNDEFINED;
    movedCursor = TS_UNDEFINED;
    secondaryMovedCursor = TS_UNDEFINED;
    Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

    style->add_class("drawingarea");
    style->add_class(GTK_STYLE_CLASS_TROUGH);
    //style->add_class(GTK_STYLE_CLASS_SCALE);
    style->add_class(GTK_STYLE_CLASS_SLIDER);

    set_name("ThresholdSelector");
    set_can_focus(false);
    set_app_paintable(true);
    updateTooltip();
}

Gtk::SizeRequestMode ThresholdSelector::get_request_mode_vfunc() const
{
    return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
}

void ThresholdSelector::get_preferred_height_vfunc(int& minimum_height,
                                                   int& natural_height) const
{
    int minimumWidth = 0;
    int naturalWidth = 0;
    get_preferred_width_vfunc(minimumWidth, naturalWidth);
    get_preferred_height_for_width_vfunc(minimumWidth, minimum_height, natural_height);
}

void ThresholdSelector::get_preferred_width_vfunc(int& minimum_width,
                                                  int& natural_width) const
{
    const int s = RTScalable::scalePixelSize(1);
    Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    Gtk::Border padding = getPadding(style);  // already scaled
    int margins = padding.get_left() + padding.get_right();
    minimum_width = 60 * s + margins;
    natural_width = 150 * s + margins;
}

void ThresholdSelector::get_preferred_height_for_width_vfunc(int width,
                                                             int& minimum_height,
                                                             int& natural_height) const
{
    const int s = RTScalable::scalePixelSize(1);
    Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    Gtk::Border padding = getPadding(style);  // already scaled
    int margins = padding.get_left() + padding.get_right();
    natural_height = minimum_height = 26 * s + margins;
}

void ThresholdSelector::get_preferred_width_for_height_vfunc(int height,
                                                             int& minimum_width,
                                                             int& natural_width) const
{
    get_preferred_width_vfunc(minimum_width, natural_width);
}

/*
 * Set the position of the sliders without telling it to the listener
 */
void ThresholdSelector::setPositions(double bottom, double top)
{

    setPositions(bottom, top, maxValBottom, maxValTop);

    if (updatePolicy == RTUP_DYNAMIC) {
        queue_draw();
    }
}

/*
 * Set the position of the sliders without telling it to the listener
 */
void ThresholdSelector::setPositions(double bottomLeft,
                                     double topLeft,
                                     double bottomRight,
                                     double topRight)
{

    bool different =
        ((positions[TS_TOPLEFT] != topLeft) || (positions[TS_TOPRIGHT] != topRight)
         || (positions[TS_BOTTOMLEFT] != bottomLeft)
         || (positions[TS_BOTTOMRIGHT] != bottomRight));
    positions[TS_BOTTOMLEFT] = bottomLeft;
    positions[TS_TOPLEFT] = topLeft;
    positions[TS_BOTTOMRIGHT] = bottomRight;
    positions[TS_TOPRIGHT] = topRight;

    if (different) {
        sig_val_changed.emit();
        updateTooltip();
        queue_draw();
    }
}

void ThresholdSelector::setDefaults(double bottom, double top)
{

    setDefaults(bottom, top, maxValBottom, maxValTop);
}

void ThresholdSelector::setDefaults(double bottomLeft,
                                    double topLeft,
                                    double bottomRight,
                                    double topRight)
{

    defPos[TS_BOTTOMLEFT] = bottomLeft;
    defPos[TS_TOPLEFT] = topLeft;

    if (doubleThresh) {
        defPos[TS_BOTTOMRIGHT] = bottomRight;
        defPos[TS_TOPRIGHT] = topRight;
    }
}

void ThresholdSelector::on_realize()
{
    Gtk::DrawingArea::on_realize();

    add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK
               | Gdk::BUTTON_RELEASE_MASK | Gdk::LEAVE_NOTIFY_MASK);
}

void ThresholdSelector::updateDrawingArea(const ::Cairo::RefPtr<Cairo::Context>& cr)
{
    // on_realize has to be called before
    if (!get_realized() || !get_allocated_width() || !get_allocated_height()) {
        return;
    }

    // Get border padding
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    Gtk::Border padding = getPadding(style);

    // Setup drawing
    cr->set_operator(Cairo::OPERATOR_OVER);

    // Get widget size
    const int w = get_allocated_width();
    const int h = get_allocated_height();

    // Compute slider parameters
    const double wslider = sliderWidth;  // constant must be an odd value
    const double hwslider = wslider / 2.;
    const double verticalSliderPadding =
        std::floor((static_cast<double>(h) - static_cast<double>(padding.get_top())
                    - static_cast<double>(padding.get_bottom()))
                       * verticalSliderPaddingFactor
                   + 0.5);

    // Get threshold selector positions
    const double positions01[4] = { to01(TS_BOTTOMLEFT), to01(TS_TOPLEFT),
                                    to01(TS_BOTTOMRIGHT), to01(TS_TOPRIGHT) };

    // Compute internal background position and size
    const double innerBarX = static_cast<double>(padding.get_left()) + hwslider - 0.5;
    const double innerBarY =
        verticalSliderPadding + 1. + static_cast<double>(padding.get_top());
    const double innerBarW = static_cast<double>(w) - innerBarX
                             - static_cast<double>(padding.get_right()) - hwslider - 0.5;
    const double innerBarH = static_cast<double>(h) - innerBarY - verticalSliderPadding
                             - 1. - static_cast<double>(padding.get_bottom());

    // Render background (style one or colored bar one)
    if (is_sensitive() && coloredBar.canGetColors()) {
        coloredBar.setColoredBarSize(innerBarX, innerBarY, innerBarW, innerBarH);
        coloredBar.updateColoredBar(cr);
    } else {
        style->render_background(cr, innerBarX, innerBarY, innerBarW, innerBarH);
    }

    // Render curve
    const double yStart = innerBarY + innerBarH - 1.;
    const double yEnd = innerBarY + 1.;
    const double xStart = innerBarX;
    const double xEnd = innerBarX + innerBarW;
    const double iw = xEnd - xStart;
    const double ih = yEnd - yStart;

    if (bgCurveProvider) {

        std::vector<double> pts = bgCurveProvider->getCurvePoints(
            this);  // the values sent by the provider are not checked (assumed to be correct)

        if (pts.size() >= 4) {
            std::vector<double>::iterator i = pts.begin();
            ++i;
            double y = *i;
            ++i;
            cr->move_to(xStart, ih * y + yStart);

            for (; i < pts.end();) {
                double x = *i;
                ++i;
                y = *i;
                ++i;
                cr->line_to(xStart + iw * x, ih * y + yStart);
            }
        } else {
            // Draw a straight line because not enough points has been sent
            cr->move_to(xStart, yEnd);
            cr->rel_line_to(iw, 0.);
        }

    } else {
        if (!separatedSliders) {
            ThreshCursorId p[4];
            double yStart_ = yStart;
            double yEnd_ = yEnd;

            if (initalEq1) {
                std::swap(yStart_, yEnd_);

                p[0] = TS_TOPLEFT;
                p[1] = TS_BOTTOMLEFT;
                p[2] = TS_BOTTOMRIGHT;
                p[3] = TS_TOPRIGHT;
            } else {
                p[0] = TS_BOTTOMLEFT;
                p[1] = TS_TOPLEFT;
                p[2] = TS_TOPRIGHT;
                p[3] = TS_BOTTOMRIGHT;
            }

            if (positions[p[1]] > minValTop)
            {  // we use minValTop since if this block is executed, it means that we are in a simple Threshold where both bottom and top range are the same
                cr->move_to(innerBarX, yStart_);
            } else {
                cr->move_to(innerBarX, yEnd_);
            }

            if (positions[p[0]] > minValTop) {
                cr->line_to(xStart + iw * positions01[p[0]], yStart_);
            }

            if (positions[p[1]] > minValTop) {
                cr->line_to(xStart + iw * positions01[p[1]], yEnd_);
            }

            cr->line_to(xStart + iw * positions01[p[2]], yEnd_);

            if (doubleThresh && positions[p[2]] < maxValTop) {
                cr->line_to(xStart + iw * positions01[p[3]], yStart_);

                if (positions[p[3]] < maxValTop) {
                    cr->line_to(xEnd, yStart_);
                }
            }
        }
    }

    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);
    cr->set_line_cap(Cairo::LINE_CAP_BUTT);
    cr->set_line_join(Cairo::LINE_JOIN_BEVEL);

    // Render surrounding curve (black)
    if (is_sensitive()) {

        cr->set_source_rgb(0., 0., 0.);
        cr->set_line_width(4.);
        cr->stroke_preserve();
    }

    // Render inner curve (white)
    if (is_sensitive()) {
        cr->set_source_rgb(1., 1., 1.);
    } else {
        cr->set_source_rgba(0., 0., 0., 0.5);
    }
    cr->set_line_width(2.);
    cr->stroke();

    // Render the box's borders
    style->render_frame(cr, innerBarX - 1., innerBarY - 1., innerBarW + 2.,
                        innerBarH + 2.);

    // Render sliders
    Gtk::StateFlags currState = style->get_state();

    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);
    cr->set_line_cap(Cairo::LINE_CAP_ROUND);

    for (int i = 0; i < (doubleThresh ? 4 : 2); ++i) {
        if (!is_sensitive()) {
            style->set_state(Gtk::STATE_FLAG_INSENSITIVE);
        } else if (i == movedCursor) {
            style->set_state(Gtk::STATE_FLAG_ACTIVE);
        } else if (i == litCursor) {
            style->set_state(Gtk::STATE_FLAG_PRELIGHT);
        } else {
            style->set_state(Gtk::STATE_FLAG_NORMAL);
        }

        const double posX = xStart + iw * positions01[i];
        const double arrowY = i == 0 || i == 2 ? yStart - 3. : yEnd + 3.;
        const double baseY =
            i == 0 || i == 2
                ? static_cast<double>(h) - static_cast<double>(padding.get_bottom()) - 0.5
                : static_cast<double>(padding.get_top()) + 0.5;

        style->render_slider(cr, posX - hwslider, i == 0 || i == 2 ? arrowY : baseY,
                             wslider, i == 0 || i == 2 ? baseY - arrowY : arrowY - baseY,
                             Gtk::ORIENTATION_HORIZONTAL);
    }

    style->set_state(currState);
}

bool ThresholdSelector::on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr)
{
    // Draw drawing area
    // Note: As drawing area surface is updated inside on_draw function, hidpi is automatically supported
    updateDrawingArea(cr);

    return true;
}

bool ThresholdSelector::on_button_press_event(GdkEventButton* event)
{

    if (event->button == 1) {
        movedCursor = litCursor;
        findSecondaryMovedCursor(event->state);
        tmpX = event->x;

        queue_draw();
    }

    grab_focus();
    return true;
}

bool ThresholdSelector::on_button_release_event(GdkEventButton* event)
{

    if (event->button == 1) {
        findLitCursor(event->x, event->y);
        movedCursor = TS_UNDEFINED;
        secondaryMovedCursor = TS_UNDEFINED;
        queue_draw();
    }

    return true;
}

bool ThresholdSelector::on_leave_notify_event(GdkEventCrossing* event)
{
    if (movedCursor == TS_UNDEFINED) {
        litCursor = TS_UNDEFINED;
        oldLitCursor = TS_UNDEFINED;
        queue_draw();
    }

    return true;
}

bool ThresholdSelector::on_motion_notify_event(GdkEventMotion* event)
{
    const int w = get_allocated_width();
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    const Gtk::Border padding = getPadding(style);

    const double wslider = sliderWidth;  // constant must be an odd value
    const double hwslider = wslider / 2.;

    const double innerBarX = static_cast<double>(padding.get_left()) + hwslider - 0.5;
    const double innerBarW = static_cast<double>(w) - innerBarX
                             - static_cast<double>(padding.get_right()) - hwslider - 0.5;

    const double xStart = innerBarX + 0.5;
    const double xEnd = innerBarX + innerBarW - 0.5;
    const double iw = xEnd - xStart;

    findLitCursor(event->x, event->y);

    if (movedCursor != TS_UNDEFINED) {
        // user is moving a cursor or two
        double minBound, maxBound, dRange;

        findSecondaryMovedCursor(event->state);

        // computing the boundaries
        findBoundaries(minBound, maxBound);

        if (movedCursor == TS_BOTTOMLEFT || movedCursor == TS_BOTTOMRIGHT) {
            dRange = maxValBottom - minValBottom;
        } else {
            dRange = maxValTop - minValTop;
        }

        double dX = ((event->x - tmpX) * dRange) / iw;

        // slow motion if CTRL is pressed
        if (event->state & Gdk::CONTROL_MASK) {
            dX *= 0.05;
        }

        // get the new X value, inside bounds
        double newX = positions[movedCursor] + dX;

        if (newX > maxBound) {
            newX = maxBound;
        } else if (newX < minBound) {
            newX = minBound;
        }

        // compute the effective dX
        dX = newX - positions[movedCursor];
        // set the new position of the moved cursor
        positions[movedCursor] = newX;

        // apply the decay to the secondary moved cursor, if necessary
        if (secondaryMovedCursor != TS_UNDEFINED) {
            positions[secondaryMovedCursor] += dX;
        }

        // set the new reference value for the next move
        tmpX = event->x;

        // update the tooltip
        updateTooltip();

        queue_draw();

        sig_val_changed.emit();
    } else {
        if (litCursor != oldLitCursor) {
            queue_draw();
        }

        oldLitCursor = litCursor;
    }

    return true;
}

void ThresholdSelector::findLitCursor(int posX, int posY)
{
    const int w = get_allocated_width();
    const int h = get_allocated_height();
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    const Gtk::Border padding = getPadding(style);

    const double wslider = sliderWidth;  // constant must be an odd value
    const double hwslider = wslider / 2.;

    const double innerBarX = static_cast<double>(padding.get_left()) + hwslider - 0.5;
    const double innerBarW = static_cast<double>(w) - innerBarX
                             - static_cast<double>(padding.get_right()) - hwslider - 0.5;

    litCursor = TS_UNDEFINED;

    if (posY >= 0 && posY <= h / 2) {
        if (posX >= static_cast<int>(innerBarX - hwslider)
            && posX <= static_cast<int>(innerBarX + innerBarW + hwslider))
        {
            litCursor = TS_TOPLEFT;

            if (doubleThresh) {
                // we use minValTop since if this block is executed, it means that we are in a simple Threshold where both bottom and top range are the same
                const double cursorX = (static_cast<double>(posX) - innerBarX)
                                           * (maxValTop - minValTop) / innerBarW
                                       + minValTop;

                if (cursorX > positions[TS_TOPRIGHT]
                    || std::fabs(cursorX - positions[TS_TOPRIGHT])
                           < std::fabs(cursorX - positions[TS_TOPLEFT]))
                {
                    litCursor = TS_TOPRIGHT;
                }
            }
        }
    } else if (posY > h / 2 && posY < h) {
        if (posX >= static_cast<int>(innerBarX - hwslider)
            && posX <= static_cast<int>(innerBarX + innerBarW + hwslider))
        {
            litCursor = TS_BOTTOMLEFT;

            if (doubleThresh) {
                // we use minValTop since if this block is executed, it means that we are in a simple Threshold where both bottom and top range are the same
                double cursorX = (static_cast<double>(posX) - innerBarX)
                                     * (maxValTop - minValTop) / innerBarW
                                 + minValTop;

                if (cursorX > positions[TS_BOTTOMRIGHT]
                    || std::fabs(cursorX - positions[TS_BOTTOMRIGHT])
                           < std::fabs(cursorX - positions[TS_BOTTOMLEFT]))
                {
                    litCursor = TS_BOTTOMRIGHT;
                }
            }
        }
    }
}

void ThresholdSelector::findBoundaries(double& min, double& max)
{

    switch (movedCursor) {
        case (TS_BOTTOMLEFT):
            if (separatedSliders) {
                min = minValBottom;
                max = maxValBottom;
            } else if (initalEq1) {
                min =
                    secondaryMovedCursor == TS_UNDEFINED
                        ? positions[TS_TOPLEFT]
                        : minValTop + (positions[TS_BOTTOMLEFT] - positions[TS_TOPLEFT]);
                max = positions[TS_BOTTOMRIGHT];
            } else {
                min = minValTop;
                max = secondaryMovedCursor == TS_UNDEFINED
                          ? positions[TS_TOPLEFT]
                          : positions[TS_TOPRIGHT]
                                - (positions[TS_TOPLEFT] - positions[TS_BOTTOMLEFT]);
            }

            break;

        case (TS_TOPLEFT):
            if (separatedSliders) {
                min = minValTop;
                max = maxValTop;
            } else if (initalEq1) {
                min = minValTop;
                max = secondaryMovedCursor == TS_UNDEFINED
                          ? positions[TS_BOTTOMLEFT]
                          : positions[TS_BOTTOMRIGHT]
                                - (positions[TS_BOTTOMLEFT] - positions[TS_TOPLEFT]);
            } else {
                min =
                    secondaryMovedCursor == TS_UNDEFINED
                        ? positions[TS_BOTTOMLEFT]
                        : minValTop + (positions[TS_TOPLEFT] - positions[TS_BOTTOMLEFT]);
                max = positions[TS_TOPRIGHT];
            }

            break;

        case (TS_BOTTOMRIGHT):
            if (initalEq1) {
                min = positions[TS_BOTTOMLEFT];
                max = secondaryMovedCursor == TS_UNDEFINED
                          ? positions[TS_TOPRIGHT]
                          : maxValTop
                                - (positions[TS_TOPRIGHT] - positions[TS_BOTTOMRIGHT]);
            } else {
                min = secondaryMovedCursor == TS_UNDEFINED
                          ? positions[TS_TOPRIGHT]
                          : positions[TS_TOPLEFT]
                                + (positions[TS_BOTTOMRIGHT] - positions[TS_TOPRIGHT]);
                max = maxValTop;
            }

            break;

        case (TS_TOPRIGHT):
            if (initalEq1) {
                min = secondaryMovedCursor == TS_UNDEFINED
                          ? positions[TS_BOTTOMRIGHT]
                          : positions[TS_BOTTOMLEFT]
                                + (positions[TS_TOPRIGHT] - positions[TS_BOTTOMRIGHT]);
                max = maxValTop;
            } else {
                min = positions[TS_TOPLEFT];
                max = secondaryMovedCursor == TS_UNDEFINED
                          ? positions[TS_BOTTOMRIGHT]
                          : maxValTop
                                - (positions[TS_BOTTOMRIGHT] - positions[TS_TOPRIGHT]);
            }

            break;

        default:
            min = minValTop;
            max = maxValTop;
            break;
    }
}

void ThresholdSelector::findSecondaryMovedCursor(guint state)
{
    secondaryMovedCursor = TS_UNDEFINED;

    if (!separatedSliders && !(state & Gdk::SHIFT_MASK)) {
        switch (movedCursor) {
            case (TS_BOTTOMLEFT):
                secondaryMovedCursor = TS_TOPLEFT;
                break;

            case (TS_TOPLEFT):
                secondaryMovedCursor = TS_BOTTOMLEFT;
                break;

            case (TS_BOTTOMRIGHT):
                secondaryMovedCursor = TS_TOPRIGHT;
                break;

            case (TS_TOPRIGHT):
                secondaryMovedCursor = TS_BOTTOMRIGHT;
                break;

            default:
                secondaryMovedCursor = TS_UNDEFINED;
                break;
        }
    }
}

void ThresholdSelector::styleChanged(const Glib::RefPtr<Gtk::StyleContext>& style)
{

    queue_draw();
}

void ThresholdSelector::reset()
{

    positions[0] = defPos[0];
    positions[1] = defPos[1];
    positions[2] = defPos[2];
    positions[3] = defPos[3];

    updateTooltip();
    queue_draw();
}

double ThresholdSelector::to01(ThreshCursorId cursorId)
{

    double rVal;

    if (cursorId == TS_BOTTOMLEFT || cursorId == TS_BOTTOMRIGHT) {
        rVal = (positions[cursorId] - minValBottom) / (maxValBottom - minValBottom);
    } else {
        rVal = (positions[cursorId] - minValTop) / (maxValTop - minValTop);
    }

    if (rVal < 0.) {
        rVal = 0.;
    } else if (rVal > 1.) {
        rVal = 1.;
    }

    return rVal;
}

void ThresholdSelector::setBgCurveProvider(ThresholdCurveProvider* provider)
{
    bgCurveProvider = provider;
}

void ThresholdSelector::setSeparatedSliders(bool separated)
{
    separatedSliders = separated;
}

bool ThresholdSelector::getSeparatedSliders()
{
    return separatedSliders;
}

void ThresholdSelector::updateTooltip()
{

    Glib::ustring tTip;

    if (doubleThresh) {
        tTip = Glib::ustring::compose(
            "<b>%1:</b> %2     <b>%3:</b> %4\n<b>%5:</b> %6     <b>%7:</b> %8",
            M("THRESHOLDSELECTOR_TL"),
            Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                  positions[TS_TOPLEFT]),
            M("THRESHOLDSELECTOR_TR"),
            Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                  positions[TS_TOPRIGHT]),
            M("THRESHOLDSELECTOR_BL"),
            Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                  positions[TS_BOTTOMLEFT]),
            M("THRESHOLDSELECTOR_BR"),
            Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                  positions[TS_BOTTOMRIGHT]));

        if (!additionalTTip.empty()) {
            tTip += Glib::ustring::compose("\n\n%1", additionalTTip);
        }

        tTip += Glib::ustring::compose("\n\n%1", M("THRESHOLDSELECTOR_HINT"));
    } else if (separatedSliders) {
        tTip = Glib::ustring::compose(
            "<b>%1:</b> %2\n<b>%3:</b> %4", separatedLabelTop,
            Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                  positions[TS_TOPLEFT]),
            separatedLabelBottom,
            Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                  positions[TS_BOTTOMLEFT]));

        if (!additionalTTip.empty()) {
            tTip += Glib::ustring::compose("\n\n%1", additionalTTip);
        }
    } else {
        tTip = Glib::ustring::compose(
            "<b>%1:</b> %2\n<b>%3:</b> %4", M("THRESHOLDSELECTOR_T"),
            Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                  positions[TS_TOPLEFT]),
            M("THRESHOLDSELECTOR_B"),
            Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                  positions[TS_BOTTOMLEFT]));

        if (!additionalTTip.empty()) {
            tTip += Glib::ustring::compose("\n\n%1", additionalTTip);
        }

        tTip += Glib::ustring::compose("\n\n%1", M("THRESHOLDSELECTOR_HINT"));
    }

    Gtk::Widget::set_tooltip_markup(tTip);
}

sigc::signal<void> ThresholdSelector::signal_value_changed()
{
    return sig_val_changed;
}

double ThresholdSelector::shapePositionValue(ThreshCursorId cursorId)
{
    unsigned int precision = (cursorId == TS_BOTTOMLEFT || cursorId == TS_BOTTOMRIGHT)
                                 ? precisionBottom
                                 : precisionTop;
    return round(positions[cursorId] * pow(double(10), precision))
           / pow(double(10), precision);
}

void ThresholdSelector::set_tooltip_markup(const Glib::ustring& markup)
{
    additionalTTip = markup;
    updateTooltip();
}

void ThresholdSelector::set_tooltip_text(const Glib::ustring& text)
{
    additionalTTip = text;
    updateTooltip();
}
