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
#include <cstring>

#include <gdkmm/types.h>

#include "myflatcurve.h"

#include "editcallbacks.h"

#include "rtengine/curves.h"

MyFlatCurve::MyFlatCurve() :
    MyCurve(),
    clampedX(0.0),
    clampedY(0.0),
    deltaX(0.0),
    deltaY(0.0),
    distanceX(0.0),
    distanceY(0.0),
    ugpX(0.0),
    ugpY(0.0),
    leftTanX(0.0),
    rightTanX(0.0),
    preciseCursorX(0.0),
    preciseCursorY(0.0),
    minDistanceX(0.0),
    minDistanceY(0.0),
    deletedPointX(0.0),
    leftTanHandle({ 0.0, 0.0 }),
    rightTanHandle({ 0.0, 0.0 }),
    draggingElement(false),
    locallabRef(0.0)
{

    lit_point = -1;
    closest_point = 0;
    editedHandle = FCT_EditedHandle_None;
    area = FCT_Area_None;
    tanHandlesDisplayed = false;
    periodic = true;

    //bghist = new unsigned int[256];

    editedPos.resize(4);
    editedPos.at(0) = editedPos.at(1) = editedPos.at(2) = editedPos.at(3) = 0.0;

    signal_event().connect(sigc::mem_fun(*this, &MyFlatCurve::handleEvents));

    // By default, we create a curve with 6 control points
    curve.type = FCT_MinMaxCPoints;

    defaultCurve();
}

/*MyFlatCurve::~MyFlatCurve () {
}*/

std::vector<double> MyFlatCurve::get_vector(int veclen)
{

    // Create the output variable
    std::vector<double> convertedValues;

    // Get the curve control points
    std::vector<double> curveDescr = getPoints();
    rtengine::FlatCurve rtcurve(curveDescr, periodic,
                                veclen * 1.2 > 5000 ? 5000 : veclen * 1.2);

    // Create the sample values that will be converted
    std::vector<double> samples;
    samples.resize(veclen);

    for (int i = 0; i < veclen; i++) {
        samples.at(i) = (double)i / (veclen - 1.0);
    }

    // Converting the values
    rtcurve.getVal(samples, convertedValues);

    // Cleanup and return
    return convertedValues;
}

void MyFlatCurve::get_LUT(LUTf& lut)
{

    int size = lut.getSize();

    // Get the curve control points
    std::vector<double> curveDescr = getPoints();
    rtengine::FlatCurve rtcurve(
        curveDescr, periodic,
        lut.getUpperBound() * 1.2 > 5000 ? 5000 : lut.getUpperBound() * 1.2);

    double maxVal = double(lut.getUpperBound());

    for (int i = 0; i < size; i++) {
        double t = double(i) / maxVal;
        lut[i] = rtcurve.getVal(t);
    }

    return;
}

void MyFlatCurve::interpolate()
{

    prevGraphW = graphW;
    prevGraphH = graphH;
    point((unsigned int)graphW);
    get_LUT(point);
    upoint.reset();
    lpoint.reset();

    curveIsDirty = false;
}

void MyFlatCurve::updateDrawingArea(const ::Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!get_realized()) {
        return;
    }

    // re-calculate curve if dimensions changed
    int currLUTSize = point.getUpperBound();

    if (curveIsDirty || (currLUTSize == GRAPH_SIZE && (graphW > GRAPH_SIZE))
        || (currLUTSize > GRAPH_SIZE && (graphW <= GRAPH_SIZE || graphW != currLUTSize)))
    {
        interpolate();
    }

    Gtk::StateFlags state =
        !is_sensitive() ? Gtk::STATE_FLAG_INSENSITIVE : Gtk::STATE_FLAG_NORMAL;

    Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

    // Setup drawing
    cr->set_line_cap(Cairo::LINE_CAP_SQUARE);
    cr->set_operator(Cairo::OPERATOR_OVER);

    // Render background
    style->render_background(cr, graphX, graphY - graphH, graphW, graphH);

    Gdk::RGBA c;

    cr->set_line_width(1.0);

    // Draw Locallab reference value in the background
    if (locallabRef > 0.0) {
        cr->set_line_width(1.0);
        cr->move_to(double(graphX + 1), double(graphY - 1));
        c = style->get_color(state);
        cr->set_source_rgba(c.get_red(), c.get_green(), c.get_blue(), 0.2);
        cr->line_to(double(graphX + 1), double(graphY - 1) - double(graphH - 2));
        cr->line_to(double(graphX) + 1.5 + locallabRef * double(graphW - 2),
                    double(graphY - 1) - double(graphH - 2));
        cr->line_to(double(graphX) + 1.5 + locallabRef * double(graphW - 2),
                    double(graphY - 1));
        cr->close_path();
        cr->fill();
        cr->stroke();
    }

    // draw the left colored bar
    if (leftBar) {
        // first the background
        leftBar->setColoredBarSize(1., graphY - graphH - 0.5, CBAR_WIDTH, graphH);
        leftBar->updateColoredBar(cr);

        // now the border
        c = style->get_border_color(state);
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        cr->rectangle(0.5, graphY - graphH - 0.5 - 0.5, CBAR_WIDTH + 1,
                      (double)graphH + 1.);
        cr->stroke();
    }

    // draw the bottom colored bar
    if (bottomBar) {
        // first the background
        bottomBar->setColoredBarSize(graphX - 0.5, graphY + RADIUS + CBAR_MARGIN + 1.,
                                     graphW + 1., CBAR_WIDTH);
        bottomBar->updateColoredBar(cr);

        // now the border
        c = style->get_border_color(state);
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        cr->rectangle(graphX - 0.5 - 0.5, graphY + RADIUS + CBAR_MARGIN + 0.5,
                      graphW + 1. + 1., CBAR_WIDTH + 1.);
        cr->stroke();
    }

    // draw f(x)=0.5 line
    c = style->get_border_color(state);
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    const std::valarray<double> ds = { 4. };
    cr->set_dash(ds, 0);
    cr->move_to(graphX - 1., graphY - graphH / 2.);
    cr->rel_line_to(graphW + 2, 0.);
    cr->stroke();

    cr->unset_dash();
    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);
    cr->set_line_width(1.0);
    cr->set_line_cap(Cairo::LINE_CAP_BUTT);

    // draw the pipette values
    if (pipetteR > -1.f || pipetteG > -1.f || pipetteB > -1.f) {
        cr->set_line_width(0.75);
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        int n = 0;

        if (pipetteR > -1.f) {
            ++n;
        }

        if (pipetteG > -1.f) {
            ++n;
        }

        if (pipetteB > -1.f) {
            ++n;
        }

        if (n > 1) {
            if (pipetteR > -1.f) {
                // WARNING: assuming that red values are stored in pipetteR, which might not be the case!
                cr->set_source_rgba(1., 0., 0., 0.5);
                cr->move_to(graphX + graphW * static_cast<double>(pipetteR), graphY + 1.);
                cr->rel_line_to(0, -graphH - 1.);
                cr->stroke();
            }

            if (pipetteG > -1.f) {
                // WARNING: assuming that green values are stored in pipetteG, which might not be the case!
                cr->set_source_rgba(0., 1., 0., 0.5);
                cr->move_to(graphX + graphW * static_cast<double>(pipetteG), graphY + 1.);
                cr->rel_line_to(0, -graphH - 1.);
                cr->stroke();
            }

            if (pipetteB > -1.f) {
                // WARNING: assuming that blue values are stored in pipetteB, which might not be the case!
                cr->set_source_rgba(0., 0., 1., 0.5);
                cr->move_to(graphX + graphW * static_cast<double>(pipetteB), graphY + 1.);
                cr->rel_line_to(0, -graphH - 1.);
                cr->stroke();
            }
        }

        if (pipetteVal > -1.f) {
            cr->set_line_width(2.);
            c = style->get_color(state);
            cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
            cr->move_to(graphX + graphW * static_cast<double>(pipetteVal), graphY + 1.);
            cr->rel_line_to(0, -graphH - 1.);
            cr->stroke();
            cr->set_line_width(1.);
        }
    }

    // draw the color feedback of the control points
    if (colorProvider) {

        //if (curve.type!=FCT_Parametric)
        for (int i = 0; i < (int)curve.x.size(); ++i) {

            if (curve.x.at(i) != -1.) {
                double coloredLineWidth =
                    rtengine::min<double>(rtengine::max<double>(75., graphW) / 75., 8.);

                cr->set_line_width(coloredLineWidth);
                colorProvider->colorForValue(curve.x.at(i), curve.y.at(i),
                                             CCET_VERTICAL_BAR, colorCallerId, this);
                cr->set_source_rgb(ccRed, ccGreen, ccBlue);

                if (i == lit_point
                    && (editedHandle
                        & (FCT_EditedHandle_CPointUD | FCT_EditedHandle_CPoint
                           | FCT_EditedHandle_CPointX)))
                {
                    cr->set_line_width(2 * coloredLineWidth);
                }

                cr->move_to(graphX + graphW * curve.x.at(i), graphY + 0.5 + 0.5);
                cr->rel_line_to(0., -graphH - 1. - 1.);
                cr->stroke();
                cr->set_line_width(coloredLineWidth);

                // draw the lit_point's horizontal line
                bool drawHLine = false;

                if (edited_point > -1) {
                    if (i == edited_point) {
                        cr->set_line_width(2 * coloredLineWidth);
                        drawHLine = true;
                    }
                } else if (i == lit_point) {
                    if ((area & (FCT_Area_H | FCT_Area_V | FCT_Area_Point))
                        || editedHandle == FCT_EditedHandle_CPointUD)
                    {
                        if (editedHandle
                            & (FCT_EditedHandle_CPointUD | FCT_EditedHandle_CPoint
                               | FCT_EditedHandle_CPointY))
                        {
                            cr->set_line_width(2 * coloredLineWidth);
                            drawHLine = true;
                        }
                    }
                }

                if (drawHLine) {
                    int point = edited_point > -1 ? edited_point : lit_point;
                    colorProvider->colorForValue(curve.x.at(i), curve.y.at(i),
                                                 CCET_HORIZONTAL_BAR, colorCallerId,
                                                 this);
                    cr->set_source_rgb(ccRed, ccGreen, ccBlue);

                    cr->move_to(graphX - 0.5 - 0.5, graphY - graphH * curve.y.at(point));
                    cr->rel_line_to(graphW + 1. + 1., 0.);
                    cr->stroke();
                }
            }
        }

        // endif
        cr->set_line_width(1.0);
    } else {
        cr->set_source_rgb(0.5, 0.0, 0.0);

        if (edited_point > -1
            || ((lit_point > -1)
                && ((area & (FCT_Area_H | FCT_Area_V | FCT_Area_Point))
                    || editedHandle == FCT_EditedHandle_CPointUD)))
        {
            // draw the lit_point's vertical line
            if (edited_point > -1
                || (editedHandle
                    & (FCT_EditedHandle_CPointUD | FCT_EditedHandle_CPoint
                       | FCT_EditedHandle_CPointY)))
            {
                cr->set_line_width(2.0);
            }

            int point = edited_point > -1 ? edited_point : lit_point;
            cr->move_to(graphX + graphW * curve.x.at(point), graphY + 0.5 + 0.5);
            cr->rel_line_to(0., -graphH - 1. - 1.);
            cr->stroke();
            cr->set_line_width(1.0);

            // draw the lit_point's horizontal line
            if (editedHandle
                & (FCT_EditedHandle_CPointUD | FCT_EditedHandle_CPoint
                   | FCT_EditedHandle_CPointY))
            {
                cr->set_line_width(2.0);
            }

            cr->move_to(graphX - 0.5 - 0.5, graphY - graphH * curve.y.at(point));
            cr->rel_line_to(graphW + 1. + 1., 0.);
            cr->stroke();
            cr->set_line_width(1.0);
        }
    }

    cr->set_line_cap(Cairo::LINE_CAP_SQUARE);

    // draw the graph's borders:
    c = style->get_border_color(state);
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    cr->rectangle(graphX - 0.5 - 0.5, graphY + 0.5 + 0.5, graphW + 1. + 1.,
                  -(graphH + 1. + 1.));
    cr->stroke();

    double lineMinLength = 1. / graphW * (double)(SQUARE) * 0.9;

    if (tanHandlesDisplayed && lit_point != -1 && getHandles(lit_point)
        && curve.x.at(lit_point) != -1.)
    {
        double x = graphX + graphW * curve.x.at(lit_point);
        double y = graphY - graphH * curve.y.at(lit_point);
        double x2;
        double square;
        bool crossingTheFrame;

        // left handle is yellow
        // TODO: finding a way to set the left handle color for flat curve editor
        cr->set_source_rgb(1.0, 1.0, 0.0);

        // draw tangential vectors

        crossingTheFrame = false;

        // We display the line only if it's longer than the handle knot half-size...
        if (leftTanX < -0.00001) {
            leftTanX += 1.0;
            crossingTheFrame = true;
        }

        x2 = graphX + graphW * leftTanX;

        if (curve.x.at(lit_point) - leftTanX > lineMinLength || crossingTheFrame) {
            // The left tangential vector reappear on the right side
            // draw the line
            cr->move_to(x, y);

            if (crossingTheFrame) {
                cr->line_to(graphX - 0.5 - 0.5, y);
                cr->stroke();
                cr->move_to(graphX + graphW + 0.5 + 0.5, y);
            }

            cr->line_to(x2, y);
            cr->stroke();
        }

        // draw tangential knot
        square = (area == FCT_Area_LeftTan ? SQUARE * 2. : SQUARE);
        cr->rectangle(x2 - square, y - square, 2. * square, 2. * square);
        cr->fill();

        // right handle is blue
        // TODO: finding a way to set the right handle color for flat curve editor
        cr->set_source_rgb(0.0, 0.0, 1.0);

        // draw tangential vectors

        crossingTheFrame = false;

        // We display the line only if it's longer than the handle knot half-size...
        if (rightTanX > 1.00001) {
            rightTanX -= 1.0;
            crossingTheFrame = true;
        }

        x2 = graphX + graphW * rightTanX;

        if (rightTanX - curve.x.at(lit_point) > lineMinLength || crossingTheFrame) {
            // The left tangential vector reappear on the right side
            // draw the line
            cr->move_to(x, y);

            if (crossingTheFrame) {
                cr->line_to(graphX + graphW + 0.5 + 0.5, y);
                cr->stroke();
                cr->move_to(graphX - 0.5 - 0.5, y);
            }

            cr->line_to(x2, y);
            cr->stroke();
        }

        // draw tangential knot
        square = (area == FCT_Area_RightTan ? SQUARE * 2. : SQUARE);
        cr->rectangle(x2 - square, y - square, 2. * square, 2. * square);
        cr->fill();
    }

    // draw curve
    c = style->get_color(state);
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    cr->move_to(graphX, static_cast<double>(getVal(point, 0)) * -graphH + graphY);

    for (int i = 1; i < graphW; ++i) {
        cr->line_to((double)i + graphX,
                    static_cast<double>(getVal(point, i)) * -graphH + graphY);
    }

    cr->stroke();

    // draw bullets
    for (int i = 0; i < (int)curve.x.size(); ++i) {
        if (curve.x.at(i) != -1.) {
            if (i == edited_point) {
                cr->set_source_rgb(1.0, 0.0, 0.0);
            } else if (i == lit_point) {
                if (colorProvider && edited_point == -1) {
                    colorProvider->colorForValue(curve.x.at(i), curve.y.at(i), CCET_POINT,
                                                 colorCallerId, this);
                    cr->set_source_rgb(ccRed, ccGreen, ccBlue);
                } else {
                    cr->set_source_rgb(1.0, 0.0, 0.0);
                }
            } else if (i == snapToElmt || i == edited_point) {
                cr->set_source_rgb(1.0, 0.0, 0.0);
            } else if (curve.y.at(i) == 0.5) {
                cr->set_source_rgb(0.0, 0.5, 0.0);
            } else {
                cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
            }

            double x =
                graphX
                + graphW * curve.x.at(i);  // project (curve.x.at(i), 0, 1, graphW);
            double y =
                graphY
                - graphH * curve.y.at(i);  // project (curve.y.at(i), 0, 1, graphH);

            cr->arc(x, y, RADIUS + 0.5, 0, 2 * rtengine::RT_PI);
            cr->fill();

            if (i == edited_point) {
                cr->set_source_rgb(1.0, 0.0, 0.0);
                cr->set_line_width(2.);
                cr->arc(x, y, RADIUS + 2., 0, 2 * rtengine::RT_PI);
                cr->stroke();
                cr->set_line_width(1.);
            }
        }
    }

    // endif

    // draw the left and right tangent handles
    if (tanHandlesDisplayed) {
        double halfSquareSizeX = minDistanceX / 2.;
        double halfSquareSizeY = minDistanceY / 2.;

        // LEFT handle

        // yellow
        cr->set_source_rgb(1.0, 1.0, 0.0);
        cr->rectangle(graphX + graphW * (leftTanHandle.centerX - halfSquareSizeX),
                      graphY - graphH * (leftTanHandle.centerY + halfSquareSizeY),
                      graphW * minDistanceX, graphW * minDistanceY);
        cr->fill();

        // RIGHT handle

        // blue
        cr->set_source_rgb(0.0, 0.0, 1.0);
        cr->rectangle(graphX + graphW * (rightTanHandle.centerX - halfSquareSizeX),
                      graphY - graphH * (rightTanHandle.centerY + halfSquareSizeY),
                      graphW * minDistanceX, graphW * minDistanceY);
        cr->fill();
    }
}

bool MyFlatCurve::on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr)
{
    // Draw drawing area
    // Note: As drawing area surface is updated inside on_draw function, hidpi is automatically supported
    updateDrawingArea(cr);

    return false;
}

/*
 * Return the X1, X2, Y position of the tangential handles.
 */
bool MyFlatCurve::getHandles(int n)
{
    int N = curve.x.size();
    double prevX, nextX;
    double x, leftTan, rightTan;

    if (n == -1) {
        return false;
    }

    x = curve.x.at(n);
    leftTan = curve.leftTangent.at(n);
    rightTan = curve.rightTangent.at(n);

    if (!n) {
        // first point, the left handle is then computed with the last point's right handle
        prevX = curve.x.at(N - 1) - 1.0;
        nextX = curve.x.at(1);
    } else if (n == N - 1) {
        // last point, the right handle is then computed with the first point's left handle
        prevX = curve.x.at(n - 1);
        nextX = curve.x.at(0) + 1.0;
    } else {
        // last point, the right handle is then computed with the first point's left handle
        prevX = curve.x.at(n - 1);
        nextX = curve.x.at(n + 1);
    }

    if (leftTan == 0.0) {
        leftTanX = x;
    } else if (leftTan == 1.0) {
        leftTanX = prevX;
    } else {
        leftTanX = (prevX - x) * leftTan + x;
    }

    if (rightTan == 0.0) {
        rightTanX = x;
    } else if (rightTan == 1.0) {
        rightTanX = nextX;
    } else {
        rightTanX = (nextX - x) * rightTan + x;
    }

    return true;
}

bool MyFlatCurve::handleEvents(GdkEvent* event)
{

    CursorShape new_type = cursor_type;
    std::vector<double>::iterator itx, ity, itlt, itrt;

    snapToElmt = -100;
    bool retval = false;
    int num = (int)curve.x.size();

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    if ((graphW < 0) || (graphH < 0)) {
        return false;
    }

    minDistanceX = double(MIN_DISTANCE) / graphW;
    minDistanceY = double(MIN_DISTANCE) / graphH;

    switch (event->type) {

        case GDK_BUTTON_PRESS:
            if (edited_point == -1) {  //curve.type!=FCT_Parametric) {
                if (event->button.button == 1) {
                    buttonPressed = true;
                    add_modal_grab();

                    // get the pointer position
                    getCursorPosition(Gdk::EventType(event->type),
                                      event->motion.is_hint != 0, int(event->button.x),
                                      int(event->button.y),
                                      Gdk::ModifierType(event->button.state));
                    getMouseOverArea();

                    // hide the tangent handles
                    tanHandlesDisplayed = false;

                    // Action on BUTTON_PRESS and no edited point
                    switch (area) {
                        case (FCT_Area_Insertion):
                            // Shown when adding a new node in a blank area, both click and drag.
                            new_type = CSMove2D;

                            /* insert a new control point */
                            if (num > 0) {
                                if (clampedX > curve.x.at(closest_point)) {
                                    ++closest_point;
                                }
                            }

                            itx = curve.x.begin();
                            ity = curve.y.begin();
                            itlt = curve.leftTangent.begin();
                            itrt = curve.rightTangent.begin();

                            for (int i = 0; i < closest_point; i++) {
                                ++itx;
                                ++ity;
                                ++itlt;
                                ++itrt;
                            }

                            curve.x.insert(itx, 0);
                            curve.y.insert(ity, 0);
                            curve.leftTangent.insert(itlt, 0);
                            curve.rightTangent.insert(itrt, 0);

                            if (mod_type & GDK_CONTROL_MASK) {
                                clampedY = point.getVal01(clampedX);
                            }

                            // the graph is refreshed only if a new point is created
                            curve.x.at(closest_point) = clampedX;
                            curve.y.at(closest_point) = clampedY;
                            curve.leftTangent.at(closest_point) = 0.35;
                            curve.rightTangent.at(closest_point) = 0.35;

                            curveIsDirty = true;
                            queue_draw();
                            notifyListener();

                            lit_point = closest_point;

                            // point automatically activated
                            editedHandle = FCT_EditedHandle_CPoint;
                            ugpX = curve.x.at(lit_point);
                            ugpY = curve.y.at(lit_point);
                            break;

                        case (FCT_Area_Point):
                            new_type = CSMove2D;  // Shown when node clicked and dragged.
                            editedHandle = FCT_EditedHandle_CPoint;
                            ugpX = curve.x.at(lit_point);
                            ugpY = curve.y.at(lit_point);
                            break;

                        case (FCT_Area_H):
                        case (FCT_Area_V):
                            // Shown when vertical line clicked, not dragged.
                            new_type = CSMove2D;
                            editedHandle = FCT_EditedHandle_CPointUD;
                            ugpX = curve.x.at(lit_point);
                            ugpY = curve.y.at(lit_point);
                            break;

                        case (FCT_Area_LeftTan):
                            new_type = CSEmpty;
                            editedHandle = FCT_EditedHandle_LeftTan;
                            ugpX = curve.leftTangent.at(lit_point);
                            break;

                        case (FCT_Area_RightTan):
                            new_type = CSEmpty;
                            editedHandle = FCT_EditedHandle_RightTan;
                            ugpX = curve.rightTangent.at(lit_point);
                            break;

                        default:
                            break;
                    }
                } else if (event->button.button == 3) {
                    /*  get the pointer position  */
                    getCursorPosition(Gdk::EventType(event->type),
                                      event->motion.is_hint != 0, int(event->button.x),
                                      int(event->button.y),
                                      Gdk::ModifierType(event->button.state));
                    getMouseOverArea();

                    if (lit_point > -1 && lit_point != edited_point) {
                        if (editedHandle == FCT_EditedHandle_None) {
                            if (area == FCT_Area_Point || area == FCT_Area_V) {
                                // the cursor is close to an existing point
                                if (!coordinateAdjuster->is_visible()) {
                                    coordinateAdjuster->showMe(this);
                                }

                                new_type = CSArrow;
                                tanHandlesDisplayed = false;
                                edited_point = lit_point;
                                queue_draw();
                                std::vector<CoordinateAdjuster::Boundaries> newBoundaries(
                                    4);
                                int size = curve.x.size();

                                if (edited_point == 0) {
                                    newBoundaries.at(0).minVal = 0.;
                                    newBoundaries.at(0).maxVal = curve.x.at(1);
                                } else if (edited_point == size - 1) {
                                    newBoundaries.at(0).minVal =
                                        curve.x.at(edited_point - 1);
                                    newBoundaries.at(0).maxVal = 1.;
                                } else if (curve.x.size() > 2) {
                                    newBoundaries.at(0).minVal =
                                        curve.x.at(edited_point - 1);
                                    newBoundaries.at(0).maxVal =
                                        curve.x.at(edited_point + 1);
                                }

                                newBoundaries.at(1).minVal = 0.;
                                newBoundaries.at(1).maxVal = 1.;
                                newBoundaries.at(2).minVal = 0.;
                                newBoundaries.at(2).maxVal = 1.;
                                newBoundaries.at(3).minVal = 0.;
                                newBoundaries.at(3).maxVal = 1.;
                                editedPos.at(0) = curve.x.at(edited_point);
                                editedPos.at(1) = curve.y.at(edited_point);
                                editedPos.at(2) = curve.leftTangent.at(edited_point);
                                editedPos.at(3) = curve.rightTangent.at(edited_point);
                                coordinateAdjuster->setPos(editedPos);
                                coordinateAdjuster->startNumericalAdjustment(
                                    newBoundaries);
                            }
                        }
                    }

                    retval = true;
                }

                if (buttonPressed) {
                    retval = true;
                }
            } else {  // if (edited_point > -1)
                if (event->button.button == 3) {
                    // do we edit another point?
                    /*  get the pointer position  */
                    getCursorPosition(Gdk::EventType(event->type),
                                      event->motion.is_hint != 0, int(event->button.x),
                                      int(event->button.y),
                                      Gdk::ModifierType(event->button.state));
                    getMouseOverArea();

                    if (area == FCT_Area_Point || area == FCT_Area_V) {
                        // the cursor is close to an existing point
                        if (lit_point != edited_point) {
                            edited_point = lit_point;
                            queue_draw();
                            std::vector<CoordinateAdjuster::Boundaries> newBoundaries(4);
                            int size = curve.x.size();

                            if (edited_point == 0) {
                                newBoundaries.at(0).minVal = 0.;
                                newBoundaries.at(0).maxVal = curve.x.at(1);
                            } else if (edited_point == size - 1) {
                                newBoundaries.at(0).minVal = curve.x.at(edited_point - 1);
                                newBoundaries.at(0).maxVal = 1.;
                            } else if (curve.x.size() > 2) {
                                newBoundaries.at(0).minVal = curve.x.at(edited_point - 1);
                                newBoundaries.at(0).maxVal = curve.x.at(edited_point + 1);
                            }

                            newBoundaries.at(1).minVal = 0.;
                            newBoundaries.at(1).maxVal = 1.;
                            newBoundaries.at(2).minVal = 0.;
                            newBoundaries.at(2).maxVal = 1.;
                            newBoundaries.at(3).minVal = 0.;
                            newBoundaries.at(3).maxVal = 1.;
                            editedPos.at(0) = curve.x.at(edited_point);
                            editedPos.at(1) = curve.y.at(edited_point);
                            editedPos.at(2) = curve.leftTangent.at(edited_point);
                            editedPos.at(3) = curve.rightTangent.at(edited_point);
                            coordinateAdjuster->switchAdjustedPoint(editedPos,
                                                                    newBoundaries);
                            retval = true;
                        }
                    } else if (area == FCT_Area_Insertion) {
                        // the cursor is inside the graph but away from existing points
                        new_type = CSPlus;
                        curveIsDirty = true;
                        stopNumericalAdjustment();
                    }
                }
            }

            break;

        case GDK_BUTTON_RELEASE:
            if (edited_point == -1) {  //curve.type!=FCT_Parametric) {
                if (buttonPressed && event->button.button == 1) {
                    buttonPressed = false;
                    remove_modal_grab();

                    // Removing any deleted point if we were previously modifying the point position
                    if (editedHandle
                        & (FCT_EditedHandle_CPoint | FCT_EditedHandle_CPointX
                           | FCT_EditedHandle_CPointY))
                    {
                        /* delete inactive points: */
                        int src, dst;
                        itx = curve.x.begin();
                        ity = curve.y.begin();
                        itlt = curve.leftTangent.begin();
                        itrt = curve.rightTangent.begin();

                        for (src = dst = 0; src < num; ++src)
                            if (curve.x.at(src) >= 0.0) {
                                curve.x.at(dst) = curve.x.at(src);
                                curve.y.at(dst) = curve.y.at(src);
                                curve.leftTangent.at(dst) = curve.leftTangent.at(src);
                                curve.rightTangent.at(dst) = curve.rightTangent.at(src);
                                ++dst;
                                ++itx;
                                ++ity;
                                ++itlt;
                                ++itrt;
                            }

                        if (dst < src) {

                            // curve cleanup
                            curve.x.erase(itx, curve.x.end());
                            curve.y.erase(ity, curve.y.end());
                            curve.leftTangent.erase(itlt, curve.leftTangent.end());
                            curve.rightTangent.erase(itrt, curve.rightTangent.end());

                            if (curve.x.empty()) {
                                curve.x.push_back(0.5);
                                curve.y.push_back(0.5);
                                curve.leftTangent.push_back(0.3);
                                curve.rightTangent.push_back(0.3);
                                curveIsDirty = true;
                            }
                        }
                    }

                    editedHandle = FCT_EditedHandle_None;
                    lit_point = -1;

                    // get the pointer position
                    getCursorPosition(Gdk::EventType(event->type),
                                      event->motion.is_hint != 0, int(event->button.x),
                                      int(event->button.y),
                                      Gdk::ModifierType(event->button.state));
                    getMouseOverArea();

                    switch (area) {

                        case (FCT_Area_Insertion):
                            new_type = CSArrow;
                            break;

                        case (FCT_Area_Point):
                            new_type = CSMove2D;  // Shown when node released.
                            break;

                        case (FCT_Area_H):
                            new_type = CSResizeHeight;
                            break;

                        case (FCT_Area_V):
                            new_type = CSMove2D;  // Shown when line released.
                            break;

                        case (FCT_Area_LeftTan):
                            new_type = CSEmpty;
                            break;

                        case (FCT_Area_RightTan):
                            new_type = CSEmpty;
                            break;

                        default:
                            break;
                    }

                    queue_draw();
                    retval = true;
                    //notifyListener ();
                }
            }

            break;

        case GDK_MOTION_NOTIFY:
            if (curve.type == FCT_Linear || curve.type == FCT_MinMaxCPoints) {

                int previous_lit_point = lit_point;
                enum MouseOverAreas prevArea = area;

                snapToMinDistY = snapToMinDistX = 10.;
                snapToValY = snapToValX = 0.;
                snapToElmt = -100;

                // get the pointer position
                getCursorPosition(Gdk::EventType(event->type), event->motion.is_hint != 0,
                                  int(event->button.x), int(event->button.y),
                                  Gdk::ModifierType(event->button.state));
                getMouseOverArea();

                if (editedHandle == FCT_EditedHandle_CPointUD) {
                    double dX = deltaX;
                    double dY = deltaY;

                    if (dX < 0.) {
                        dX = -dX;
                    }

                    if (dY < 0.) {
                        dY = -dY;
                    }

                    if (dX > dY) {
                        editedHandle = FCT_EditedHandle_CPointX;
                        area = FCT_Area_V;
                        new_type = CSResizeWidth;
                    } else {
                        editedHandle = FCT_EditedHandle_CPointY;
                        area = FCT_Area_H;
                        new_type = CSResizeHeight;
                    }
                }

                switch (editedHandle) {

                    case (FCT_EditedHandle_None):
                    {

                        if ((lit_point != -1 && previous_lit_point != lit_point)
                            && (area & (FCT_Area_V | FCT_Area_Point))
                            && edited_point == -1)
                        {

                            bool sameSide = false;

                            // display the handles
                            tanHandlesDisplayed = true;

                            if (curve.x.at(lit_point) < 3. * minDistanceX) {
                                // lit_point too near of the left border -> both handles are displayed on the right of the vertical line
                                rightTanHandle.centerX = leftTanHandle.centerX =
                                    curve.x.at(lit_point) + 2. * minDistanceX;
                                sameSide = true;
                            } else if (curve.x.at(lit_point) > 1. - 3. * minDistanceX) {
                                // lit_point too near of the left border -> both handles are displayed on the right of the vertical line
                                rightTanHandle.centerX = leftTanHandle.centerX =
                                    curve.x.at(lit_point) - 2. * minDistanceX;
                                sameSide = true;
                            } else {
                                leftTanHandle.centerX =
                                    curve.x.at(lit_point) - 2. * minDistanceX;
                                rightTanHandle.centerX =
                                    curve.x.at(lit_point) + 2. * minDistanceX;
                            }

                            if (sameSide) {
                                if (clampedY > 1. - 2. * minDistanceY) {
                                    // lit_point too near of the top border
                                    leftTanHandle.centerY = 1. - minDistanceY;
                                    rightTanHandle.centerY = 1. - 3. * minDistanceY;
                                } else if (clampedY < 2. * minDistanceY) {
                                    // lit_point too near of the bottom border
                                    leftTanHandle.centerY = 3. * minDistanceY;
                                    rightTanHandle.centerY = minDistanceY;
                                } else {
                                    leftTanHandle.centerY = clampedY + minDistanceY;
                                    rightTanHandle.centerY = clampedY - minDistanceY;
                                }
                            } else {
                                if (clampedY > 1. - minDistanceY) {
                                    rightTanHandle.centerY = leftTanHandle.centerY =
                                        1. - minDistanceY;
                                } else if (clampedY < minDistanceY) {
                                    rightTanHandle.centerY = leftTanHandle.centerY =
                                        minDistanceY;
                                } else {
                                    rightTanHandle.centerY = leftTanHandle.centerY =
                                        clampedY;
                                }
                            }
                        } else if (lit_point == -1 || edited_point > -1) {
                            tanHandlesDisplayed = false;
                        }

                        if (edited_point == -1) {
                            switch (area) {

                                case (FCT_Area_Insertion):
                                    new_type = CSPlus;
                                    break;

                                case (FCT_Area_Point):

                                //new_type = CSMove;
                                //break;
                                case (FCT_Area_V):
                                    // Shown when hovering over vertical line.
                                    new_type = CSPlus;
                                    break;

                                case (FCT_Area_H):
                                    new_type = CSResizeHeight;
                                    break;

                                case (FCT_Area_LeftTan):
                                    new_type = CSMoveLeft;
                                    break;

                                case (FCT_Area_RightTan):
                                    new_type = CSMoveRight;
                                    break;

                                case (FCT_Area_None):
                                default:
                                    new_type = CSArrow;
                                    break;
                            }
                        }

                        if ((lit_point != previous_lit_point) || (prevArea != area)) {
                            queue_draw();
                        }

                        if (coordinateAdjuster->is_visible() && edited_point == -1) {
                            if (lit_point > -1) {
                                if (lit_point != previous_lit_point) {
                                    editedPos.at(0) = curve.x.at(lit_point);
                                    editedPos.at(1) = curve.y.at(lit_point);
                                    editedPos.at(2) = curve.leftTangent.at(lit_point);
                                    editedPos.at(3) = curve.rightTangent.at(lit_point);
                                }

                                coordinateAdjuster->setPos(editedPos);
                            } else if (area == FCT_Area_Insertion) {
                                editedPos.at(0) = clampedX;
                                editedPos.at(1) = clampedY;
                                editedPos.at(2) = 0.;
                                editedPos.at(3) = 0.;
                                coordinateAdjuster->setPos(editedPos);
                            } else {
                                editedPos.at(0) = editedPos.at(1) = editedPos.at(2) =
                                    editedPos.at(3) = 0;
                                coordinateAdjuster->setPos(editedPos);
                            }
                        }

                        break;
                    }

                    case (FCT_EditedHandle_CPoint):
                        movePoint(true, true);

                        if (coordinateAdjuster->is_visible()) {
                            editedPos.at(0) = curve.x.at(lit_point);
                            editedPos.at(1) = curve.y.at(lit_point);
                            coordinateAdjuster->setPos(editedPos);
                        }

                        break;

                    case (FCT_EditedHandle_CPointX):
                        movePoint(true, false);

                        if (coordinateAdjuster->is_visible()) {
                            editedPos.at(0) = curve.x.at(lit_point);
                            coordinateAdjuster->setPos(editedPos);
                        }

                        break;

                    case (FCT_EditedHandle_CPointY):
                        movePoint(false, true);

                        if (coordinateAdjuster->is_visible()) {
                            editedPos.at(1) = curve.y.at(lit_point);
                            coordinateAdjuster->setPos(editedPos);
                        }

                        break;

                    case (FCT_EditedHandle_LeftTan):
                    {
                        double prevValue = curve.leftTangent.at(lit_point);

                        ugpX -= deltaX * 3;
                        ugpX = CLAMP(ugpX, 0., 1.);

                        if (snapTo) {
                            // since this handle can only move in one direction, we can reuse the snapCoordinateX mechanism
                            snapCoordinateX(0.0, ugpX);
                            snapCoordinateX(0.35, ugpX);
                            snapCoordinateX(0.5, ugpX);
                            snapCoordinateX(1.0, ugpX);
                            curve.leftTangent.at(lit_point) = snapToValX;
                        } else {
                            curve.leftTangent.at(lit_point) = ugpX;
                        }

                        if (curve.leftTangent.at(lit_point) != prevValue) {
                            curveIsDirty = true;
                            queue_draw();
                            notifyListener();

                            if (coordinateAdjuster->is_visible()) {
                                editedPos.at(2) = curve.leftTangent.at(lit_point);
                                coordinateAdjuster->setPos(editedPos);
                            }
                        }

                        break;
                    }

                    case (FCT_EditedHandle_RightTan):
                    {
                        double prevValue = curve.rightTangent.at(lit_point);

                        ugpX += deltaX * 3;
                        ugpX = CLAMP(ugpX, 0., 1.);

                        if (snapTo) {
                            // since this handle can only move in one direction, we can reuse the snapCoordinateX mechanism
                            snapCoordinateX(0.0, ugpX);
                            snapCoordinateX(0.35, ugpX);
                            snapCoordinateX(0.5, ugpX);
                            snapCoordinateX(1.0, ugpX);
                            curve.rightTangent.at(lit_point) = snapToValX;
                        } else {
                            curve.rightTangent.at(lit_point) = ugpX;
                        }

                        if (curve.rightTangent.at(lit_point) != prevValue) {
                            curveIsDirty = true;
                            queue_draw();
                            notifyListener();
                            editedPos.at(3) = curve.rightTangent.at(lit_point);
                            coordinateAdjuster->setPos(editedPos);
                        }

                        break;
                    }

                        // already processed before the "switch" instruction
                        //case (FCT_EditedHandle_CPointUD):

                    default:
                        break;
                }

                if (edited_point == -1) {
                    if (lit_point == -1) {
                        editedPos.at(0) = editedPos.at(1) = editedPos.at(2) =
                            editedPos.at(3) = 0;
                    } else if (editedPos.at(0) != curve.x.at(lit_point)
                               || editedPos.at(1) != curve.y.at(lit_point)
                               || editedPos.at(2) != curve.leftTangent.at(lit_point)
                               || editedPos.at(3) != curve.rightTangent.at(lit_point))
                    {
                        editedPos.at(0) = curve.x.at(lit_point);
                        editedPos.at(1) = curve.y.at(lit_point);
                        editedPos.at(2) = curve.leftTangent.at(lit_point);
                        editedPos.at(3) = curve.rightTangent.at(lit_point);
                        coordinateAdjuster->setPos(editedPos);
                    }
                }
            }

            retval = true;
            break;

        case GDK_LEAVE_NOTIFY:

            // Pointer can LEAVE even when dragging the point, so we don't modify the cursor in this case
            // The cursor will have to LEAVE another time after the drag...
            if (editedHandle == FCT_EditedHandle_None) {
                new_type = CSArrow;
                lit_point = -1;
                tanHandlesDisplayed = false;
                pipetteR = pipetteG = pipetteB = -1.f;
                queue_draw();
            }

            retval = true;
            break;

        default:
            break;
    }

    if (new_type != cursor_type) {
        cursor_type = new_type;
        CursorManager::setCursorOfMainWindow(get_window(), cursor_type);
    }

    return retval;
}

void MyFlatCurve::pipetteMouseOver(CurveEditor* ce,
                                   EditDataProvider* provider,
                                   int modifierKey)
{
    if (!provider) {
        // occurs when leaving the preview area -> cleanup the curve editor
        pipetteR = pipetteG = pipetteB = -1.f;
        lit_point = -1;
        editedHandle = FCT_EditedHandle_None;
        return;
    }

    pipetteR = provider->getPipetteVal1();
    pipetteG = provider->getPipetteVal2();
    pipetteB = provider->getPipetteVal3();
    pipetteVal = 0.f;

    if (listener) {
        pipetteVal = listener->blendPipetteValues(ce, pipetteR, pipetteG, pipetteB);
    } else {
        int n = 0;

        if (pipetteR != -1.f) {
            pipetteVal += pipetteR;
            ++n;
        }

        if (pipetteG != -1.f) {
            pipetteVal += pipetteG;
            ++n;
        }

        if (pipetteB != -1.f) {
            pipetteVal += pipetteB;
            ++n;
        }

        if (n > 1) {
            pipetteVal /= n;
        } else if (!n) {
            pipetteVal = -1.f;
        }
    }

    snapToElmt = -100;

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    if ((graphW < 0) || (graphH < 0)) {
        return;
    }

    int previous_lit_point = lit_point;

    // hide the handles
    tanHandlesDisplayed = false;

    // get the pointer position
    // WARNING: converting pipetteVal from float to int, precision loss here!
    int px = graphX + int(float(graphW) * pipetteVal);
    getCursorPosition(Gdk::EventType(Gdk::BUTTON_PRESS), false, px, graphY,
                      Gdk::ModifierType(modifierKey));

    if (edited_point == -1) {
        getMouseOverArea();
    }

    if (area == FCT_Area_Point) {
        area = FCT_Area_V;
    }

    snapToMinDistY = snapToMinDistX = 10.;
    snapToValY = snapToValX = 0.;

    if (edited_point == -1) {
        if (editedHandle == FCT_EditedHandle_None && lit_point != previous_lit_point) {
            queue_draw();
        }
    } else {
        queue_draw();
    }

    if (edited_point == -1) {
        editedPos.at(0) = pipetteVal;
        editedPos.at(1) = point.getVal01(pipetteVal);
        coordinateAdjuster->setPos(editedPos);
    }
}

// returns true if a point is being dragged
bool MyFlatCurve::pipetteButton1Pressed(EditDataProvider* provider, int modifierKey)
{
    if (edited_point > -1) {
        return false;
    }

    buttonPressed = true;

    // get the pointer position
    // WARNING: converting pipetteVal from float to int, precision loss here!
    int px = graphX + int(float(graphW) * pipetteVal);
    getCursorPosition(Gdk::EventType(Gdk::BUTTON_PRESS), false, px, graphY,
                      Gdk::ModifierType(modifierKey));
    getMouseOverArea();

    // hide the tangent handles
    tanHandlesDisplayed = false;

    // Action on BUTTON_PRESS and no edited point
    switch (area) {

        case (FCT_Area_Insertion):
        {
            rtengine::FlatCurve rtCurve(getPoints(), true, GRAPH_SIZE);

            std::vector<double>::iterator itx, ity, itlt, itrt;
            int num = (int)curve.x.size();

            /* insert a new control point */
            if (num > 0) {
                if (clampedX > curve.x.at(closest_point)) {
                    ++closest_point;
                }
            }

            itx = curve.x.begin();
            ity = curve.y.begin();
            itlt = curve.leftTangent.begin();
            itrt = curve.rightTangent.begin();

            for (int i = 0; i < closest_point; i++) {
                ++itx;
                ++ity;
                ++itlt;
                ++itrt;
            }

            curve.x.insert(itx, 0);
            curve.y.insert(ity, 0);
            curve.leftTangent.insert(itlt, 0);
            curve.rightTangent.insert(itrt, 0);
            num++;

            // the graph is refreshed only if a new point is created
            curve.x.at(closest_point) = clampedX;
            curve.y.at(closest_point) = clampedY = rtCurve.getVal(pipetteVal);
            curve.leftTangent.at(closest_point) = 0.35;
            curve.rightTangent.at(closest_point) = 0.35;

            curveIsDirty = true;
            queue_draw();
            notifyListener();

            lit_point = closest_point;

            // point automatically activated
            editedHandle = FCT_EditedHandle_CPointY;
            ugpX = curve.x.at(lit_point);
            ugpY = curve.y.at(lit_point);
            break;
        }

        case (FCT_Area_V):
            editedHandle = FCT_EditedHandle_CPointY;
            ugpX = curve.x.at(lit_point);
            ugpY = curve.y.at(lit_point);
            break;

        default:
            break;
    }

    return true;
}

void MyFlatCurve::pipetteButton1Released(EditDataProvider* provider)
{
    if (edited_point > -1) {
        return;
    }

    buttonPressed = false;
    remove_modal_grab();

    editedHandle = FCT_EditedHandle_None;
    lit_point = -1;

    // get the pointer position
    // WARNING: converting pipetteVal from float to int, precision loss here!
    int px = graphX + int(float(graphW) * pipetteVal);
    getCursorPosition(Gdk::EventType(Gdk::BUTTON_PRESS), false, px, graphY,
                      Gdk::ModifierType(0));
    getMouseOverArea();

    queue_draw();
    //notifyListener ();
}

void MyFlatCurve::pipetteDrag(EditDataProvider* provider, int modifierKey)
{
    if (edited_point > -1) {
        return;
    }

    snapToMinDistY = snapToMinDistX = 10.;
    snapToValY = snapToValX = 0.;
    snapToElmt = -100;

    // get the pointer position
    getCursorPosition(Gdk::MOTION_NOTIFY, false, cursorX + graphX,
                      graphY + provider->deltaScreen.y, Gdk::ModifierType(modifierKey));
    getMouseOverArea();

    if (editedHandle == FCT_EditedHandle_CPointY) {
        movePoint(false, true, true);
    }
}

void MyFlatCurve::movePoint(bool moveX, bool moveY, bool pipetteDrag)
{

    // bounds of the grabbed point
    double leftBound;
    double rightBound;
    double const bottomBound = 0.;
    double const topBound = 1.;

    // we memorize the previous position of the point, for optimization purpose
    double prevPosX = curve.x.at(lit_point);
    double prevPosY = curve.y.at(lit_point);

    int nbPoints = (int)curve.x.size();

    // left and right bound rely on curve periodicity
    leftBound = (lit_point == 0)
                    ? (periodic && !snapTo ? curve.x.at(nbPoints - 1) - 1. : 0.)
                    : curve.x.at(lit_point - 1);
    rightBound = (lit_point == nbPoints - 1)
                     ? (periodic && !snapTo ? curve.x.at(0) + 1. : 1.)
                     : curve.x.at(lit_point + 1);

    double leftDeletionBound = leftBound - minDistanceX;
    double rightDeletionBound = rightBound + minDistanceX;
    double bottomDeletionBound = bottomBound - minDistanceY;
    double topDeletionBound = topBound + minDistanceY;

    if (moveX) {
        // we memorize the previous position of the point, for optimization purpose
        ugpX += deltaX;

        // handling periodicity (the first and last point can reappear at the other side of the X range)
        if (periodic) {
            if (snapTo) {
                if (lit_point == 0) {
                    snapCoordinateX(0.0, ugpX);
                    curve.x.at(0) = snapToValX;
                } else if (lit_point == (nbPoints - 1)) {
                    snapCoordinateX(1.0, ugpX);
                    curve.x.at(nbPoints - 1) = snapToValX;
                }
            } else if (lit_point == 0 && ugpX < 0.) {
                // the first point has to be placed at the tail of the point list
                std::vector<double>::iterator itx, ity, itlt, itrt;

                ugpX += 1.;
                leftBound += 1.;
                rightBound += 1.;
                leftDeletionBound += 1.;
                rightDeletionBound += 1.;

                // adding a copy of the first point to the tail of the list
                curve.x.push_back(curve.x.at(0));
                curve.y.push_back(curve.y.at(0));
                curve.leftTangent.push_back(curve.leftTangent.at(0));
                curve.rightTangent.push_back(curve.rightTangent.at(0));

                // deleting the first point
                itx = curve.x.begin();
                ity = curve.y.begin();
                itlt = curve.leftTangent.begin();
                itrt = curve.rightTangent.begin();

                curve.x.erase(itx);
                curve.y.erase(ity);
                curve.leftTangent.erase(itlt);
                curve.rightTangent.erase(itrt);

                lit_point = nbPoints - 1;
            } else if (lit_point == (nbPoints - 1) && ugpX > 1.) {
                // the last point has to be placed at the head of the point list
                std::vector<double>::iterator itx, ity, itlt, itrt;

                ugpX -= 1.;
                leftBound -= 1.;
                rightBound -= 1.;
                leftDeletionBound -= 1.;
                rightDeletionBound -= 1.;

                // adding a copy of the last point to the head of the list
                itx = curve.x.begin();
                ity = curve.y.begin();
                itlt = curve.leftTangent.begin();
                itrt = curve.rightTangent.begin();

                curve.x.insert(itx, 0);
                curve.y.insert(ity, 0);
                curve.leftTangent.insert(itlt, 0);
                curve.rightTangent.insert(itrt, 0);

                curve.x.at(0) = curve.x.at(nbPoints);
                curve.y.at(0) = curve.y.at(nbPoints);
                curve.leftTangent.at(0) = curve.leftTangent.at(nbPoints);
                curve.rightTangent.at(0) = curve.rightTangent.at(nbPoints);

                // deleting the last point
                curve.x.pop_back();
                curve.y.pop_back();
                curve.leftTangent.pop_back();
                curve.rightTangent.pop_back();

                lit_point = 0;
            }
        }

        // handling limitations along X axis
        if (ugpX >= rightDeletionBound && nbPoints > 2 && !snapTo) {
            curve.x.at(lit_point) = -1.;
        } else if (ugpX <= leftDeletionBound && nbPoints > 2 && !snapTo) {
            curve.x.at(lit_point) = -1.;
        } else
        // nextPosX is in bounds
        {
            curve.x.at(lit_point) = CLAMP(ugpX, leftBound, rightBound);
        }
    }

    if (moveY) {

        // we memorize the previous position of the point, for optimization purpose
        ugpY += deltaY;

        // the points stay in the bounds (and can't be deleted) in pipette drag mode
        if (pipetteDrag) {
            ugpY = CLAMP(ugpY, 0.0, 1.0);
        }

        // snapping point to specific values
        if (snapTo && curve.x.at(lit_point) != -1) {

            // the unclamped grabbed point is brought back in the range
            ugpY = CLAMP(ugpY, 0.0, 1.0);

            if (lit_point == 0) {
                int prevP = curve.y.size() - 1;

                if (snapCoordinateY(curve.y.at(prevP), ugpY)) {
                    snapToElmt = prevP;
                }
            } else {
                int prevP = lit_point - 1;

                if (snapCoordinateY(curve.y.at(prevP), ugpY)) {
                    snapToElmt = prevP;
                }
            }

            if (curve.y.size() > 2) {
                if (lit_point == int(curve.y.size()) - 1) {
                    if (snapCoordinateY(curve.y.at(0), ugpY)) {
                        snapToElmt = 0;
                    }
                } else {
                    int nextP = lit_point + 1;

                    if (snapCoordinateY(curve.y.at(nextP), ugpY)) {
                        snapToElmt = nextP;
                    }
                }
            }

            if (snapCoordinateY(1.0, ugpY)) {
                snapToElmt = -3;
            }

            if (snapCoordinateY(0.5, ugpY)) {
                snapToElmt = -2;
            }

            if (snapCoordinateY(0.0, ugpY)) {
                snapToElmt = -1;
            }

            curve.y.at(lit_point) = snapToValY;
        }

        // Handling limitations along Y axis
        if (ugpY >= topDeletionBound && nbPoints > 2) {
            if (curve.x.at(lit_point) != -1.) {
                deletedPointX = curve.x.at(lit_point);
                curve.x.at(lit_point) = -1.;
                curve.y.at(lit_point) =
                    ugpY;  // This is only to force the redraw of the curve
            }
        } else if (ugpY <= bottomDeletionBound && nbPoints > 2) {
            if (curve.x.at(lit_point) != -1.) {
                deletedPointX = curve.x.at(lit_point);
                curve.x.at(lit_point) = -1.;
                curve.y.at(lit_point) =
                    ugpY;  // This is only to force the redraw of the curve
            }
        } else {
            // nextPosY is in the bounds
            if (!snapTo) {
                curve.y.at(lit_point) = CLAMP(ugpY, 0.0, 1.0);
            }

            if (!moveX && curve.x.at(lit_point) == -1.) {
                // bring back the X value of the point if it reappear
                curve.x.at(lit_point) = deletedPointX;
            }
        }
    }

    if (curve.x.at(lit_point) != prevPosX || curve.y.at(lit_point) != prevPosY) {
        // we recompute the curve only if we have to
        curveIsDirty = true;
        queue_draw();
        notifyListener();
    }
}

// Set data relative to cursor position
void MyFlatCurve::getCursorPosition(Gdk::EventType evType,
                                    bool isHint,
                                    int evX,
                                    int evY,
                                    Gdk::ModifierType modifierKey)
{
    int tx, ty;
    int prevCursorX = cursorX;
    int prevCursorY = cursorY;
    double incrementX = 1. / double(graphW);
    double incrementY = 1. / double(graphH);

    switch (evType) {
        case (Gdk::MOTION_NOTIFY):
            if (isHint) {
                get_window()->get_pointer(tx, ty, mod_type);
            } else {
                tx = evX;
                ty = evY;
                mod_type = modifierKey;
            }

            break;

        case (Gdk::BUTTON_PRESS):
        case (Gdk::BUTTON_RELEASE):
            tx = evX;
            ty = evY;
            mod_type = modifierKey;
            break;

        default:
            // The cursor position is not available
            return;
            break;
    }

    cursorX = tx - graphX;
    cursorY = graphY - ty;

    preciseCursorX = cursorX * incrementX;
    preciseCursorY = cursorY * incrementY;

    snapTo = false;

    // update deltaX/Y if the user drags a point
    if (editedHandle != FCT_EditedHandle_None) {
        // set the dragging factor
        int control_key = mod_type & GDK_CONTROL_MASK;
        int shift_key = mod_type & GDK_SHIFT_MASK;

        // the increment get smaller if modifier key are used, and "snap to" may be enabled
        if (control_key) {
            incrementX *= 0.05;
            incrementY *= 0.05;
        }

        if (shift_key) {
            snapTo = true;
        }

        deltaX = (double)(cursorX - prevCursorX) * incrementX;
        deltaY = (double)(cursorY - prevCursorY) * incrementY;
    }
    // otherwise set the position of the new point (modifier keys has no effect here)
    else
    {
        // X position of the pointer from the origin of the graph
        clampedX = CLAMP(preciseCursorX, 0., 1.);
        // Y position of the pointer from the origin of the graph
        clampedY = CLAMP(preciseCursorY, 0., 1.);
    }
}

// Find out the active area under the cursor
void MyFlatCurve::getMouseOverArea()
{

    // When dragging an element, editedHandle keep its value
    if (editedHandle == FCT_EditedHandle_None) {  // && curve.type!=Parametric

        double minDist =
            1000;  // used to find out the point pointed by the cursor (over it)
        double minDistX = 1000;  // used to find out the closest point
        double dX, dY;
        double absDX;
        double dist;
        bool aboveVLine = false;

        // NB: this function assume that the graph's shape is a square

        // Check if the cursor is over a tangent handle
        if (tanHandlesDisplayed) {

            if (preciseCursorX >= (leftTanHandle.centerX - minDistanceX)
                && preciseCursorX <= (leftTanHandle.centerX + minDistanceX + 0.00001)
                && preciseCursorY >= (leftTanHandle.centerY - minDistanceY)
                && preciseCursorY <= (leftTanHandle.centerY + minDistanceY))
            {
                area = FCT_Area_LeftTan;
                return;
            }

            if (preciseCursorX >= (rightTanHandle.centerX - minDistanceX - 0.00001)
                && preciseCursorX <= (rightTanHandle.centerX + minDistanceX)
                && preciseCursorY >= (rightTanHandle.centerY - minDistanceY)
                && preciseCursorY <= (rightTanHandle.centerY + minDistanceY))
            {
                area = FCT_Area_RightTan;
                return;
            }
        }

        area = FCT_Area_None;
        closest_point = 0;
        lit_point = -1;

        for (int i = 0; i < (int)curve.x.size(); i++) {
            if (curve.x.at(i) != -1) {
                dX = curve.x.at(i) - preciseCursorX;
                absDX = dX > 0 ? dX : -dX;

                if (absDX < minDistX) {
                    minDistX = absDX;
                    closest_point = i;
                    lit_point = i;
                }

                if (absDX <= minDistanceX) {
                    aboveVLine = true;
                    dY = curve.y.at(i) - preciseCursorY;
                    dist = sqrt(dX * dX + dY * dY);

                    if (dist < minDist) {
                        minDist = dist;
                    }
                }
            }
        }

        if (minDist <= minDistanceX) {
            // the cursor is over the point
            area = FCT_Area_Point;
        } else if (aboveVLine) {
            area = FCT_Area_V;
        } else {
            // Check if the cursor is in an insertion area
            lit_point = -1;

            if (preciseCursorX >= 0.0 && preciseCursorX <= 1.0 && preciseCursorY >= 0.0
                && preciseCursorY <= 1.0)
            {
                area = FCT_Area_Insertion;
            }
        }
    }
}

std::vector<double> MyFlatCurve::getPoints()
{
    std::vector<double> result;

    /*if (curve.type==FCT_Parametric) {
        result.push_back ((double)(Parametric));
        for (int i=0; i<(int)curve.x.size(); i++) {
            result.push_back (curve.x.at(i));
        }
    }
    else {*/
    // the first value gives the type of the curve
    if (curve.type == FCT_Linear) {
        result.push_back((double)(FCT_Linear));
    } else if (curve.type == FCT_MinMaxCPoints) {
        result.push_back((double)(FCT_MinMaxCPoints));
    }

    // then we push all the points coordinate
    for (int i = 0; i < (int)curve.x.size(); i++) {
        if (curve.x.at(i) >= 0) {
            result.push_back(curve.x.at(i));
            result.push_back(curve.y.at(i));
            result.push_back(curve.leftTangent.at(i));
            result.push_back(curve.rightTangent.at(i));
        }
    }

    //}
    return result;
}

void MyFlatCurve::setPoints(const std::vector<double>& p)
{
    int ix = 0;
    stopNumericalAdjustment();
    FlatCurveType t = (FlatCurveType)p[ix++];
    curve.type = t;
    lit_point = -1;

    if (t == FCT_MinMaxCPoints) {
        curve.x.clear();
        curve.y.clear();
        curve.leftTangent.clear();
        curve.rightTangent.clear();

        for (int i = 0; i < (int)p.size() / 4; i++) {
            curve.x.push_back(p[ix++]);
            curve.y.push_back(p[ix++]);
            curve.leftTangent.push_back(p[ix++]);
            curve.rightTangent.push_back(p[ix++]);
        }
    }

    curveIsDirty = true;
    queue_draw();
}

void MyFlatCurve::setPos(double pos, int chanIdx)
{
    assert(edited_point > -1);

    switch (chanIdx) {
        case (0):
            curve.x.at(edited_point) = pos;
            break;

        case (1):
            curve.y.at(edited_point) = pos;
            break;

        case (2):
            curve.leftTangent.at(edited_point) = pos;
            break;

        case (3):
            curve.rightTangent.at(edited_point) = pos;
            break;
    }

    curveIsDirty = true;
    queue_draw();
    notifyListener();
}

void MyFlatCurve::stopNumericalAdjustment()
{
    if (edited_point > -1) {
        edited_point = lit_point = -1;
        area = FCT_Area_None;
        coordinateAdjuster->stopNumericalAdjustment();
        queue_draw();
    }
}

void MyFlatCurve::updateLocallabBackground(double ref)
{
    locallabRef = ref;

    mcih->pending++;

    idle_register.add([this]() -> bool {
        if (mcih->destroyed) {
            if (mcih->pending == 1) {
                delete mcih;
            } else {
                --mcih->pending;
            }

            return false;
        }

        mcih->clearPixmap();

        --mcih->pending;

        return false;
    });
}

void MyFlatCurve::setType(FlatCurveType t)
{

    curve.type = t;
    queue_draw();
}

void MyFlatCurve::reset(const std::vector<double>& resetCurve, double identityValue)
{
    calcDimensions();

    stopNumericalAdjustment();

    // If a resetCurve exist (non empty)
    if (!resetCurve.empty()) {
        setPoints(resetCurve);
        return;
    }

    switch (curve.type) {
        case FCT_MinMaxCPoints:
            defaultCurve(identityValue);
            lit_point = -1;
            curveIsDirty = true;
            break;

        //case Parametric :
        // Nothing to do (?)
        default:
            break;
    }

    queue_draw();
}

void MyFlatCurve::defaultCurve(double iVal)
{

    curve.x.resize(6);
    curve.y.resize(6);
    curve.leftTangent.resize(6);
    curve.rightTangent.resize(6);

    // Point for RGBCMY colors
    for (int i = 0; i < 6; i++) {
        curve.x.at(i) = (1. / 6.) * i;
        curve.y.at(i) = iVal;
        curve.leftTangent.at(i) = 0.35;
        curve.rightTangent.at(i) = 0.35;
    }
}
