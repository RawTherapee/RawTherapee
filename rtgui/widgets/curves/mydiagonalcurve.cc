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

#include "mydiagonalcurve.h"

#include "editcallbacks.h"

#include "rtengine/curves.h"

MyDiagonalCurve::MyDiagonalCurve() :
    MyCurve(),
    closest_point(0),
    clampedX(0.0),
    clampedY(0.0),
    deltaX(0.0),
    deltaY(0.0),
    distanceX(0.0),
    distanceY(0.0),
    ugpX(0.0),
    ugpY(0.0),
    activeParam(-1),
    bghistvalid(false),
    locallabRef(0.0)

{

    grab_point = -1;
    lit_point = -1;

    bghist = new unsigned int[256];

    editedPos.resize(2);
    editedPos.at(0) = editedPos.at(1) = 0.0;

    signal_event().connect(sigc::mem_fun(*this, &MyDiagonalCurve::handleEvents));

    curve.type = DCT_Spline;

    curve.x.push_back(0.);
    curve.y.push_back(0.);
    curve.x.push_back(1.);
    curve.y.push_back(1.);
}

MyDiagonalCurve::~MyDiagonalCurve()
{
    idle_register.destroy();

    delete[] bghist;
}

std::vector<double> MyDiagonalCurve::get_vector(int veclen)
{

    std::vector<double> vector;
    vector.resize(veclen);

    if (curve.type != DCT_Parametric) {
        // count active points:
        double prev = -1.0;
        int active = 0;
        int firstact = -1;

        for (int i = 0; i < (int)curve.x.size(); ++i)
            if (curve.x.at(i) > prev) {
                if (firstact < 0) {
                    firstact = i;
                }

                prev = curve.x.at(i);
                ++active;
            }

        // handle degenerate case:
        if (active < 2) {
            double ry;

            if (active > 0) {
                ry = curve.y.at(firstact);
            } else {
                ry = 0.0;
            }

            if (ry < 0.0) {
                ry = 0.0;
            }

            if (ry > 1.0) {
                ry = 1.0;
            }

            for (int x = 0; x < veclen; ++x) {
                vector.at(x) = ry;
            }

            return vector;
        }
    }

    // calculate remaining points
    std::vector<double> curveDescr = getPoints();
    rtengine::DiagonalCurve rtcurve(curveDescr, veclen * 1.2);
    std::vector<double> t;
    t.resize(veclen);

    for (int i = 0; i < veclen; i++) {
        t[i] = (double)i / (veclen - 1.0);
    }

    rtcurve.getVal(t, vector);
    return vector;
}

void MyDiagonalCurve::updateLocallabBackground(double ref)
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

void MyDiagonalCurve::get_LUT(LUTf& lut)
{

    int size = lut.getSize();

    if (curve.type != DCT_Parametric) {
        // count active points:
        double prev = -1.0;
        int active = 0;
        int firstact = -1;

        for (int i = 0; i < (int)curve.x.size(); ++i)
            if (curve.x.at(i) > prev) {
                if (firstact < 0) {
                    firstact = i;
                }

                prev = curve.x.at(i);
                ++active;
            }

        // handle degenerate case:
        if (active < 2) {
            double ry;

            if (active > 0) {
                ry = curve.y.at(firstact);
            } else {
                ry = 0.0;
            }

            if (ry < 0.0) {
                ry = 0.0;
            }

            if (ry > 1.0) {
                ry = 1.0;
            }

            for (int x = 0; x < size; ++x) {
                lut[x] = ry;
            }

            return;
        }
    }

    // calculate remaining points
    std::vector<double> curveDescr = getPoints();
    rtengine::DiagonalCurve rtcurve(curveDescr, lut.getUpperBound() * 1.2);

    double maxVal = double(lut.getUpperBound());

    for (int i = 0; i < size; i++) {
        double t = double(i) / maxVal;
        lut[i] = rtcurve.getVal(t);
    }

    return;
}

void MyDiagonalCurve::interpolate()
{

    prevGraphW = graphW;
    prevGraphH = graphH;
    unsigned int nbPoints = (unsigned int)graphW;
    point(nbPoints);
    get_LUT(point);
    upoint.reset();
    lpoint.reset();

    if (curve.type == DCT_Parametric && activeParam > 0) {
        double tmp = curve.x.at(activeParam - 1);

        if (activeParam >= 4) {
            upoint(nbPoints);
            lpoint(nbPoints);
            curve.x.at(activeParam - 1) = 100;
            get_LUT(upoint);
            curve.x.at(activeParam - 1) = -100;
            get_LUT(lpoint);
            curve.x.at(activeParam - 1) = tmp;
        }
    }

    curveIsDirty = false;
}

void MyDiagonalCurve::updateDrawingArea(const int handle,
                                        const ::Cairo::RefPtr<Cairo::Context>& cr)
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

    currLUTSize = point.getUpperBound();

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
                      (double)graphH + 1. + 1.);
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
        cr->rectangle(graphX - 0.5 - 0.5, graphY + CBAR_MARGIN + 0.5, graphW + 1. + 0.5,
                      CBAR_WIDTH + 1.);
        cr->stroke();
    }

    // histogram in the background
    if (bghistvalid) {
        // find highest bin
        unsigned int valMax = 0;

        for (int i = 0; i < 256; i++)
            if (bghist[i] > valMax) {
                valMax = bghist[i];
            }

        // draw histogram
        cr->set_line_width(1.0);
        double stepSize = graphW / 255.0;
        cr->move_to(graphX, graphY);
        c = style->get_color(state);
        cr->set_source_rgba(c.get_red(), c.get_green(), c.get_blue(), 0.2);

        for (int i = 0; i < 256; i++) {
            double val = double(bghist[i]) * double(graphH - 2) / double(valMax);
            /*
            if (val>graphH-2)
                val = graphH-2;
            */
            //if (i>0)
            cr->line_to(graphX + double(i) * stepSize, graphY - val);
        }

        cr->line_to(graphX + 255. * stepSize, graphY);
        cr->close_path();
        cr->fill();
    }

    // draw the grid lines:
    cr->set_line_width(1.0);
    c = style->get_border_color(state);
    cr->set_source_rgba(c.get_red(), c.get_green(), c.get_blue(), 0.3);
    cr->set_antialias(Cairo::ANTIALIAS_NONE);

    for (int i = 0; i <= 10; i++) {
        // horizontal lines
        cr->move_to(graphX - 0.5 - 0.5,
                    graphY + 0.5 + 0.5 - (graphH + 1. + 1.) * (double)i / 10.);
        cr->rel_line_to(graphW + 1. + 1., 0.);
        // vertical lines
        cr->move_to(graphX - 0.5 - 0.5 + (graphW + 1. + 1.) * (double)i / 10.,
                    graphY + 0.5 + 0.5);
        cr->rel_line_to(0., -graphH - 1. - 1.);
    }

    cr->stroke();

    // draw f(x)=x line
    if (snapToElmt == -2) {
        cr->set_source_rgb(1.0, 0.0, 0.0);
    } else {
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    }

    const std::valarray<double> ds = { 4. };
    cr->set_dash(ds, 0);
    cr->move_to(graphX - 0.5 - 0.5, graphY + 0.5 + 0.5);
    cr->rel_line_to(graphW + 1. + 1., -(graphH + 1. + 1.));
    cr->stroke();
    cr->unset_dash();

    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);
    cr->set_line_width(1.0);

    // draw upper and lower bounds
    if (curve.type == DCT_Parametric && activeParam > 0 && lpoint.getUpperBound() > 1
        && upoint.getUpperBound() > 1)
    {
        cr->set_source_rgba(1.0, 1.0, 1.0, 0.1);
        cr->move_to(graphX, static_cast<double>(getVal(upoint, 0)) * -graphH + graphY);

        for (int i = 1; i < graphW - 2; ++i) {
            cr->line_to((double)i + graphX,
                        static_cast<double>(getVal(upoint, i)) * -graphH + graphY);
        }

        for (int i = graphW - 3; i >= 0; --i) {
            cr->line_to((double)i + graphX,
                        static_cast<double>(getVal(lpoint, i)) * -graphH + graphY);
        }

        cr->fill();
    }

    // draw the pipette values
    if (pipetteR > -1.f || pipetteG > -1.f || pipetteB > -1.f) {
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

    c = style->get_color(state);

    // draw the cage of the NURBS curve
    if (curve.type == DCT_NURBS) {
        unsigned int nbPoints;
        const std::valarray<double> ch_ds = { 2. };
        cr->set_dash(ch_ds, 0);
        cr->set_line_width(0.75);
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        std::vector<double> points = getPoints();
        nbPoints = ((int)points.size() - 1) / 2;

        for (unsigned int i = 1; i < nbPoints; i++) {
            int pos = i * 2 + 1;

            // project (curve.at(i), 0, 1, graphW);
            double x1 = graphX + graphW * points[pos - 2];
            // project (curve.y.at(i)i], 0, 1, graphH);
            double y1 = graphY - graphH * points[pos - 1];
            // project (curve.at(i), 0, 1, graphW);
            double x2 = graphX + graphW * points[pos];
            // project (curve.y.at(i), 0, 1, graphH);
            double y2 = graphY - graphH * points[pos + 1];

            // set the color of the line when the point is snapped to the cage
            if (curve.x.size() == nbPoints && snapToElmt >= 1000
                && ((int(i) == (snapToElmt - 1000)) || (int(i) == (snapToElmt - 999))))
            {
                cr->set_source_rgb(1.0, 0.0, 0.0);
            } else {
                cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
            }

            cr->move_to(x1, y1);
            cr->line_to(x2, y2);
            cr->stroke();
        }

        cr->unset_dash();
        cr->set_line_width(1.0);
    }

    // draw curve
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    cr->move_to(graphX, static_cast<double>(getVal(point, 0)) * -graphH + graphY);

    for (int i = 1; i < graphW; ++i) {
        cr->line_to((double)i + graphX, (double)getVal(point, i) * -graphH + graphY);
    }

    cr->stroke();

    // draw bullets
    if (curve.type != DCT_Parametric) {
        c = style->get_color(state);

        for (int i = 0; i < (int)curve.x.size(); ++i) {
            if (curve.x.at(i) == -1) {
                continue;
            }

            if (snapToElmt >= 1000) {
                int pt = snapToElmt - 1000;

                if (i >= (pt - 1) && i <= (pt + 1)) {
                    cr->set_source_rgb(1.0, 0.0, 0.0);
                } else {
                    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
                }
            } else {
                if (i == handle || i == snapToElmt || i == edited_point) {
                    cr->set_source_rgb(1.0, 0.0, 0.0);
                } else {
                    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
                }
            }

            // project (curve.x.at(i), 0, 1, graphW);
            double x = graphX + graphW * curve.x.at(i);
            // project (curve.y.at(i), 0, 1, graphH);
            double y = graphY - graphH * curve.y.at(i);

            cr->arc(x, y, RADIUS + 0.5, 0, 2 * rtengine::RT_PI);
            cr->fill();

            if (i == edited_point) {
                cr->set_line_width(2.);
                cr->arc(x, y, RADIUS + 2., 0, 2 * rtengine::RT_PI);
                cr->stroke();
                cr->set_line_width(1.);
            }
        }
    }
}

bool MyDiagonalCurve::on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr)
{
    // Draw drawing area
    // Note: As drawing area surface is updated inside on_draw function, hidpi is automatically supported
    updateDrawingArea(lit_point, cr);

    return false;
}

bool MyDiagonalCurve::handleEvents(GdkEvent* event)
{

    CursorShape new_type = cursor_type;

    bool retval = false;
    int num = (int)curve.x.size();

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    if ((graphW < 0) || (graphH < 0)) {
        return false;
    }

    double minDistanceX = double(MIN_DISTANCE) / graphW;
    double minDistanceY = double(MIN_DISTANCE) / graphH;

    switch (event->type) {
        case GDK_BUTTON_PRESS:
            snapToElmt = -100;

            if (curve.type != DCT_Parametric) {
                if (edited_point == -1) {
                    if (event->button.button == 1) {
                        std::vector<double>::iterator itx, ity;
                        buttonPressed = true;
                        add_modal_grab();

                        // get the pointer position
                        getCursorPosition(Gdk::EventType(event->type),
                                          event->motion.is_hint != 0,
                                          int(event->button.x), int(event->button.y),
                                          Gdk::ModifierType(event->button.state));
                        findClosestPoint();

                        new_type = CSMove2D;  // Shown when dragging a node.

                        if (distanceX > minDistanceX) {
                            if (mod_type & GDK_CONTROL_MASK) {
                                clampedY = point.getVal01(clampedX);
                            }

                            /* insert a new control point */
                            if (num > 0) {
                                if (clampedX > curve.x.at(closest_point)) {
                                    ++closest_point;
                                }
                            }

                            itx = curve.x.begin();
                            ity = curve.y.begin();

                            for (int i = 0; i < closest_point; i++) {
                                ++itx;
                                ++ity;
                            }

                            curve.x.insert(itx, 0);
                            curve.y.insert(ity, 0);

                            // the graph is refreshed only if a new point is created
                            curve.x.at(closest_point) = clampedX;
                            curve.y.at(closest_point) = clampedY;

                            curveIsDirty = true;
                            queue_draw();
                            notifyListener();
                        }

                        grab_point = closest_point;
                        lit_point = closest_point;
                        ugpX = curve.x.at(closest_point);
                        ugpY = curve.y.at(closest_point);
                    } else if (event->button.button == 3) {
                        if (lit_point > -1 && grab_point == -1) {
                            if (!coordinateAdjuster->is_visible()) {
                                coordinateAdjuster->showMe(this);
                            }

                            edited_point = lit_point;
                            std::vector<CoordinateAdjuster::Boundaries> newBoundaries(2);
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
                            editedPos.at(0) = curve.x.at(edited_point);
                            editedPos.at(1) = curve.y.at(edited_point);
                            coordinateAdjuster->setPos(editedPos);
                            coordinateAdjuster->startNumericalAdjustment(newBoundaries);
                            queue_draw();
                            new_type = CSArrow;
                        }
                    }
                } else {  // if (edited_point > -1)
                    if (event->button.button == 3) {
                        // do we edit another point?
                        if (edited_point > -1 && grab_point == -1) {
                            /*  get the pointer position  */
                            getCursorPosition(Gdk::EventType(event->type),
                                              event->motion.is_hint != 0,
                                              int(event->button.x), int(event->button.y),
                                              Gdk::ModifierType(event->button.state));
                            findClosestPoint();

                            if (cursorX >= 0 && cursorX <= graphW && cursorY >= 0
                                && cursorY <= graphH)
                            {
                                if (distanceX <= minDistanceX) {
                                    // the cursor is close to an existing point
                                    lit_point = closest_point;

                                    if (lit_point != edited_point) {
                                        edited_point = lit_point;
                                        curveIsDirty = true;
                                        queue_draw();
                                        std::vector<CoordinateAdjuster::Boundaries>
                                            newBoundaries;
                                        newBoundaries.resize(2);
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
                                        editedPos.at(0) = curve.x.at(edited_point);
                                        editedPos.at(1) = curve.y.at(edited_point);
                                        coordinateAdjuster->switchAdjustedPoint(
                                            editedPos, newBoundaries);
                                    }
                                } else {
                                    // the cursor is inside the graph but away from existing points
                                    new_type = CSPlus;
                                    curveIsDirty = true;
                                    stopNumericalAdjustment();
                                }
                            }
                        }
                    }
                }
                retval = true;
            }

            break;

        case GDK_BUTTON_RELEASE:
            snapToElmt = -100;

            if (curve.type != DCT_Parametric && edited_point == -1) {
                if (buttonPressed && event->button.button == 1) {
                    std::vector<double>::iterator itx, ity;
                    int src, dst;
                    buttonPressed = false;
                    /*  get the pointer position  */
                    getCursorPosition(Gdk::EventType(event->type),
                                      event->motion.is_hint != 0, int(event->button.x),
                                      int(event->button.y),
                                      Gdk::ModifierType(event->button.state));
                    findClosestPoint();

                    remove_modal_grab();
                    int previous_lit_point = lit_point;
                    /* delete inactive points: */
                    itx = curve.x.begin();
                    ity = curve.y.begin();

                    for (src = dst = 0; src < num; ++src)
                        if (curve.x.at(src) >= 0.0) {
                            curve.x.at(dst) = curve.x.at(src);
                            curve.y.at(dst) = curve.y.at(src);
                            ++dst;
                            ++itx;
                            ++ity;
                        }

                    if (dst < src) {
                        curve.x.erase(itx, curve.x.end());
                        curve.y.erase(ity, curve.y.end());

                        if (curve.x.empty()) {
                            curve.x.push_back(0);
                            curve.y.push_back(0);
                            curveIsDirty = true;
                            queue_draw();
                        }
                    }

                    if (distanceX <= minDistanceX) {
                        new_type = CSMove2D;  // Shown on node release.
                        lit_point = closest_point;
                    } else {
                        new_type = CSPlus;
                        lit_point = -1;
                    }

                    if (lit_point != previous_lit_point) {
                        queue_draw();
                    }

                    grab_point = -1;
                    retval = true;
                    notifyListener();
                }
            }

            break;

        case GDK_LEAVE_NOTIFY:

            // Pointer can LEAVE even when dragging the point, so we don't modify the cursor in this case
            // The cursor will have to LEAVE another time after the drag...
            if (!buttonPressed) {
                if (grab_point == -1) {
                    new_type = CSArrow;
                    lit_point = -1;
                    pipetteR = pipetteG = pipetteB = -1.f;
                    queue_draw();
                }
            }

            retval = true;
            break;

        case GDK_MOTION_NOTIFY:
            snapToElmt = -100;

            if (curve.type == DCT_Linear || curve.type == DCT_Spline
                || curve.type == DCT_NURBS || curve.type == DCT_CatumullRom)
            {

                snapToMinDistY = snapToMinDistX = 10.;
                snapToValY = snapToValX = 0.;
                snapToElmt = -100;

                // get the pointer position
                getCursorPosition(Gdk::EventType(event->type), event->motion.is_hint != 0,
                                  int(event->button.x), int(event->button.y),
                                  Gdk::ModifierType(event->button.state));

                if (grab_point == -1) {
                    if (edited_point == -1) {
                        // there's no point currently being moved
                        int previous_lit_point = lit_point;
                        findClosestPoint();

                        {
                            int extendedGraphW = graphW + RADIUS + 1;
                            int extendedGraphH = graphH + RADIUS + 1;
                            if (cursorX < -RADIUS || cursorX > extendedGraphW
                                || cursorY < -RADIUS || cursorY > extendedGraphH)
                            {
                                // the cursor has left the graph area
                                new_type = CSArrow;
                                lit_point = -1;
                            } else if (distanceX <= minDistanceX) {
                                // the cursor is close to an existing point
                                new_type =
                                    CSPlus;  // Shown when hovering over node snapping distance (not necessarily over node).
                                lit_point = closest_point;
                            } else {
                                // the cursor is inside the graph but away from existing points
                                new_type = CSPlus;
                                lit_point = -1;
                            }
                        }

                        if (lit_point != previous_lit_point) {
                            queue_draw();

                            if (lit_point > -1) {
                                editedPos.at(0) = curve.x.at(lit_point);
                                editedPos.at(1) = curve.y.at(lit_point);
                            }

                            coordinateAdjuster->setPos(editedPos);
                        }

                        if (lit_point == -1 && new_type == CSPlus) {
                            editedPos.at(0) = clampedX;
                            editedPos.at(1) = clampedY;
                            coordinateAdjuster->setPos(editedPos);
                        }
                    } else {  // if (edited_point > -1)
                        // there's no point currently being moved
                        int previous_lit_point = lit_point;
                        findClosestPoint();

                        if (distanceX <= minDistanceX) {
                            // the cursor is close to an existing point
                            lit_point = closest_point;
                        } else {
                            // the cursor is outside the graph or inside the graph but away from existing points
                            lit_point = -1;
                        }

                        if (lit_point != previous_lit_point) {
                            queue_draw();
                        }
                    }
                } else {
                    // a point is being moved

                    // bounds of the grabbed point
                    double leftBound =
                        (grab_point == 0) ? 0. : curve.x.at(grab_point - 1);
                    double rightBound =
                        (grab_point == num - 1) ? 1. : curve.x.at(grab_point + 1);
                    double const bottomBound = 0.;
                    double const topBound = 1.;

                    double leftDeletionBound = leftBound - minDistanceX;
                    double rightDeletionBound = rightBound + minDistanceX;
                    double bottomDeletionBound = bottomBound - minDistanceY;
                    double topDeletionBound = topBound + minDistanceY;

                    // we memorize the previous position of the point, for optimization purpose
                    double prevPosX = curve.x.at(grab_point);
                    double prevPosY = curve.y.at(grab_point);

                    // we memorize the previous position of the point, for optimization purpose
                    ugpX += deltaX;
                    ugpY += deltaY;

                    // the unclamped grabbed point is brought back in the range when snapTo is active
                    if (snapTo) {
                        ugpY = CLAMP(ugpY, 0.0, 1.0);
                    }

                    // handling limitations along X axis
                    if (ugpX >= rightDeletionBound
                        && (grab_point > 0 && grab_point < (num - 1)))
                    {
                        curve.x.at(grab_point) = -1.;
                    } else if (ugpX <= leftDeletionBound
                               && (grab_point > 0 && grab_point < (num - 1)))
                    {
                        curve.x.at(grab_point) = -1.;
                    } else
                    // nextPosX is in bounds
                    {
                        curve.x.at(grab_point) = CLAMP(ugpX, leftBound, rightBound);
                    }

                    // Handling limitations along Y axis
                    if (ugpY >= topDeletionBound && grab_point != 0
                        && grab_point != num - 1)
                    {
                        curve.x.at(grab_point) = -1.;
                    } else if (ugpY <= bottomDeletionBound && grab_point != 0
                               && grab_point != num - 1)
                    {
                        curve.x.at(grab_point) = -1.;
                    } else {
                        // snapping point to specific values
                        if (snapTo && curve.x.at(grab_point) != -1.) {
                            if (grab_point > 0
                                && unsigned(grab_point) < (curve.y.size() - 1))
                            {
                                double prevX = curve.x.at(grab_point - 1);
                                double prevY = curve.y.at(grab_point - 1);
                                double nextX = curve.x.at(grab_point + 1);
                                double nextY = curve.y.at(grab_point + 1);

                                double ratio =
                                    (curve.x.at(grab_point) - prevX) / (nextX - prevX);
                                double y = (nextY - prevY) * ratio + prevY;

                                if (snapCoordinateY(y, ugpY)) {
                                    snapToElmt = 1000 + grab_point;
                                }
                            }

                            if (grab_point > 0) {
                                int prevP = grab_point - 1;

                                if (snapCoordinateY(curve.y.at(prevP), ugpY)) {
                                    snapToElmt = prevP;
                                }
                            }

                            if (grab_point < int(curve.y.size() - 1)) {
                                int nextP = grab_point + 1;

                                if (snapCoordinateY(curve.y.at(nextP), ugpY)) {
                                    snapToElmt = nextP;
                                }
                            }

                            if (snapCoordinateY(1.0, ugpY)) {
                                snapToElmt = -3;
                            }

                            if (snapCoordinateY(curve.x.at(grab_point), ugpY)) {
                                snapToElmt = -2;
                            }

                            if (snapCoordinateY(0.0, ugpY)) {
                                snapToElmt = -1;
                            }

                            curve.y.at(grab_point) = snapToValY;
                        } else {
                            // nextPosY is in the bounds
                            curve.y.at(grab_point) = CLAMP(ugpY, 0.0, 1.0);
                        }
                    }

                    if (curve.x.at(grab_point) != prevPosX
                        || curve.y.at(grab_point) != prevPosY)
                    {
                        // we recalculate the curve only if we have to
                        curveIsDirty = true;
                        queue_draw();
                        notifyListener();

                        if (coordinateAdjuster->is_visible()) {
                            editedPos.at(0) = curve.x.at(grab_point);
                            editedPos.at(1) = curve.y.at(grab_point);
                            coordinateAdjuster->setPos(editedPos);
                        }
                    }
                }
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

CursorShape MyDiagonalCurve::motionNotify(CursorShape type,
                                          double minDistanceX,
                                          double minDistanceY,
                                          int num)
{
    CursorShape new_type = type;

    return new_type;
}

void MyDiagonalCurve::pipetteMouseOver(CurveEditor* ce,
                                       EditDataProvider* provider,
                                       int modifierKey)
{
    if (!provider) {
        // occurs when leaving the preview area -> cleanup the curve editor
        pipetteR = pipetteG = pipetteB = -1.f;
        lit_point = -1;
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

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    if (graphW < 0. || graphH < 0.) {
        return;
    }

    double minDistanceX = MIN_DISTANCE / graphW;

    if (curve.type == DCT_Linear || curve.type == DCT_Spline || curve.type == DCT_NURBS
        || curve.type == DCT_CatumullRom)
    {
        // get the pointer position
        getCursorPositionFromCurve(pipetteVal);

        if (edited_point == -1) {
            if (grab_point == -1) {
                // there's no point currently being moved
                int previous_lit_point = lit_point;
                findClosestPoint();

                if (cursorX < 0 || cursorX > graphW || cursorY < 0 || cursorY > graphH) {
                    // the cursor has left the graph area
                    lit_point = -1;
                } else if (distanceX <= minDistanceX) {
                    lit_point = closest_point;
                } else {
                    lit_point = -1;
                }

                if (lit_point != previous_lit_point) {
                    queue_draw();
                }
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
}

// returns true if a point is being dragged
bool MyDiagonalCurve::pipetteButton1Pressed(EditDataProvider* provider, int modifierKey)
{
    if (edited_point > 1) {
        return false;
    }

    int num = (int)curve.x.size();

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    if (graphW < 0. || graphH < 0.) {
        return false;
    }

    double minDistanceX = double(MIN_DISTANCE) / graphW;

    snapToElmt = -100;

    if (curve.type != DCT_Parametric) {
        std::vector<double>::iterator itx, ity;
        buttonPressed = true;

        // get the pointer position
        getCursorPositionFromCurve(pipetteVal);
        findClosestPoint();

        if (distanceX > minDistanceX) {
            /* insert a new control point */
            if (num > 0) {
                if (clampedX > curve.x.at(closest_point)) {
                    ++closest_point;
                }
            }

            itx = curve.x.begin();
            ity = curve.y.begin();

            for (int i = 0; i < closest_point; i++) {
                ++itx;
                ++ity;
            }

            lit_point = closest_point;
            curve.x.insert(itx, 0);
            curve.y.insert(ity, 0);

            // the graph is refreshed only if a new point is created (snapped to a pixel)
            if (lit_point >= 0) {
                curve.x.at(lit_point) = clampedX;
                curve.y.at(lit_point) = clampedY;
            }

            if (lit_point > -1 && grab_point == -1 && coordinateAdjuster->is_visible()) {
                std::vector<double> position;
                position.resize(2);
                position.at(0) = clampedX;
                position.at(1) = clampedY;
                coordinateAdjuster->setPos(position);
            }

            curveIsDirty = true;
            queue_draw();
            notifyListener();
        }

        grab_point = closest_point;
        lit_point = closest_point;
        ugpX = curve.x.at(closest_point);
        ugpY = curve.y.at(closest_point);

        return true;
    }

    return false;
}

void MyDiagonalCurve::pipetteButton1Released(EditDataProvider* provider)
{
    if (edited_point > 1) {
        return;
    }

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    if (graphW < 0. || graphH < 0.) {
        return;
    }

    double minDistanceX = double(MIN_DISTANCE) / graphW;

    snapToElmt = -100;

    if (curve.type != DCT_Parametric) {
        buttonPressed = false;
        /*  get the pointer position  */
        getCursorPosition(Gdk::EventType(Gdk::BUTTON_RELEASE), false, graphY, 0,
                          Gdk::ModifierType(0));
        findClosestPoint();

        int previous_lit_point = lit_point;

        if (distanceX <= minDistanceX) {
            lit_point = closest_point;
        } else {
            lit_point = -1;
        }

        if (lit_point != previous_lit_point) {
            queue_draw();
        }

        grab_point = -1;
        //notifyListener ();
    }
}

void MyDiagonalCurve::pipetteDrag(EditDataProvider* provider, int modifierKey)
{
    if (edited_point > -1 || curve.type == DCT_Parametric || graphW < 0 || graphH < 0) {
        return;
    }

    snapToMinDistY = snapToMinDistX = 10.;
    snapToValY = snapToValX = 0.;
    snapToElmt = -100;

    /* graphW and graphH are the size of the graph */
    calcDimensions();

    getCursorPosition(Gdk::MOTION_NOTIFY, false, cursorX + graphX,
                      graphY - cursorY + provider->deltaPrevScreen.y,
                      Gdk::ModifierType(modifierKey));

    // we memorize the previous position of the point, for optimization purpose
    double prevPosX = curve.x.at(grab_point);
    double prevPosY = curve.y.at(grab_point);

    // we memorize the previous position of the point, for optimization purpose
    ugpX += deltaX;
    ugpY += deltaY;

    // the unclamped grabbed point is brought back in the range
    ugpY = CLAMP(ugpY, 0.0, 1.0);

    // snapping point to specific values
    if (snapTo && curve.x.at(grab_point) != -1.) {
        if (grab_point > 0 && unsigned(grab_point) < (curve.y.size() - 1)) {
            double prevX = curve.x.at(grab_point - 1);
            double prevY = curve.y.at(grab_point - 1);
            double nextX = curve.x.at(grab_point + 1);
            double nextY = curve.y.at(grab_point + 1);

            double ratio = (curve.x.at(grab_point) - prevX) / (nextX - prevX);
            double y = (nextY - prevY) * ratio + prevY;

            if (snapCoordinateY(y, ugpY)) {
                snapToElmt = 1000 + grab_point;
            }
        }

        if (grab_point > 0) {
            int prevP = grab_point - 1;

            if (snapCoordinateY(curve.y.at(prevP), ugpY)) {
                snapToElmt = prevP;
            }
        }

        if (grab_point < int(curve.y.size() - 1)) {
            int nextP = grab_point + 1;

            if (snapCoordinateY(curve.y.at(nextP), ugpY)) {
                snapToElmt = nextP;
            }
        }

        if (snapCoordinateY(1.0, ugpY)) {
            snapToElmt = -3;
        }

        if (snapCoordinateY(curve.x.at(grab_point), ugpY)) {
            snapToElmt = -2;
        }

        if (snapCoordinateY(0.0, ugpY)) {
            snapToElmt = -1;
        }

        curve.y.at(grab_point) = snapToValY;
    } else {
        // nextPosY is in the bounds
        curve.y.at(grab_point) = ugpY;
    }

    if (curve.x.at(grab_point) != prevPosX || curve.y.at(grab_point) != prevPosY) {
        // we recalculate the curve only if we have to
        curveIsDirty = true;
        queue_draw();
        notifyListener();

        if (lit_point > -1 && coordinateAdjuster->is_visible()) {
            std::vector<double> position;
            position.resize(2);
            position.at(0) = curve.x.at(grab_point);
            position.at(1) = curve.y.at(grab_point);
            coordinateAdjuster->setPos(position);
        }
    }
}

void MyDiagonalCurve::getCursorPositionFromCurve(float x)
{

    // the graph is refreshed only if a new point is created (snapped to a pixel)
    clampedX = x;
    clampedY = point.getVal01(x);

    cursorX = (int)(clampedX * graphW + graphX);
    cursorY = (int)(graphY - clampedY * graphH);
}

// x = cursor position found in the event
void MyDiagonalCurve::getCursorPositionFromCurve(int x)
{

    // the graph is refreshed only if a new point is created (snapped to a pixel)
    cursorX = x - graphX;
    clampedX = (double)cursorX / graphW;
    clampedY = point.getVal01(clampedX);
    cursorY = (int)(graphY - (1. - clampedY) * graphH);
}

void MyDiagonalCurve::getCursorPosition(Gdk::EventType evType,
                                        bool isHint,
                                        int evX,
                                        int evY,
                                        Gdk::ModifierType modifierKey)
{
    int tx, ty;
    int prevCursorX, prevCursorY;
    double incrementX = 1. / graphW;
    double incrementY = 1. / graphH;

    // getting the cursor position
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

    if (grab_point != -1) {
        prevCursorX = cursorX;
        prevCursorY = cursorY;
    }

    cursorX = tx - graphX;
    cursorY = graphY - ty;

    snapTo = ST_None;

    // update deltaX/Y if the user drags a point
    if (grab_point != -1) {
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

        deltaX = double(cursorX - prevCursorX) * incrementX;
        deltaY = double(cursorY - prevCursorY) * incrementY;
    }
    // otherwise set the position of the new point (modifier keys has no effect here)
    else
    {
        double tempCursorX = cursorX * incrementX;
        double tempCursorY = cursorY * incrementY;
        clampedX = CLAMP(tempCursorX, 0.,
                         1.);  // X position of the pointer from the origin of the graph
        clampedY = CLAMP(tempCursorY, 0.,
                         1.);  // Y position of the pointer from the origin of the graph
    }
}

void MyDiagonalCurve::findClosestPoint()
{
    distanceX = 10.0;
    distanceY = 10.0;
    closest_point = -1;

    if (curve.type != DCT_Parametric) {
        for (int i = 0; i < (int)curve.x.size(); i++) {
            double dX = curve.x.at(i) - clampedX;
            double dY = curve.y.at(i) - clampedY;
            double currDistX = dX < 0. ? -dX : dX;  //abs (dX);
            double currDistY = dY < 0. ? -dY : dY;  //abs (dY);

            if (currDistX < distanceX) {
                distanceX = currDistX;
                distanceY = currDistY;
                closest_point = i;
            } else if (currDistX == distanceX && currDistY < distanceY) {
                // there is more than 1 point for that X coordinate, we select the closest point to the cursor
                distanceY = currDistY;
                closest_point = i;
            }
        }
    }
}

std::vector<double> MyDiagonalCurve::getPoints()
{
    std::vector<double> result;

    if (curve.type == DCT_Parametric) {
        result.push_back((double)(DCT_Parametric));

        for (int i = 0; i < (int)curve.x.size(); i++) {
            result.push_back(curve.x.at(i));
        }
    } else {
        // the first value gives the type of the curve
        if (curve.type == DCT_Linear) {
            result.push_back(double(DCT_Linear));
        } else if (curve.type == DCT_Spline) {
            result.push_back(double(DCT_Spline));
        } else if (curve.type == DCT_NURBS) {
            result.push_back(double(DCT_NURBS));
        } else if (curve.type == DCT_CatumullRom) {
            result.push_back(double(DCT_CatumullRom));
        }

        // then we push all the points coordinate
        for (int i = 0; i < (int)curve.x.size(); i++) {
            if (curve.x.at(i) >= 0) {
                result.push_back(curve.x.at(i));
                result.push_back(curve.y.at(i));
            }
        }
    }

    return result;
}

void MyDiagonalCurve::setPoints(const std::vector<double>& p)
{
    int ix = 0;
    stopNumericalAdjustment();
    DiagonalCurveType t = (DiagonalCurveType)p[ix++];
    curve.type = t;

    if (t == DCT_Parametric) {
        curve.x.clear();
        curve.y.clear();

        for (size_t i = 1; i < p.size(); i++) {
            curve.x.push_back(p[ix++]);
        }
    } else {
        curve.x.clear();
        curve.y.clear();

        for (size_t i = 0; i < p.size() / 2; i++) {
            curve.x.push_back(p[ix++]);
            curve.y.push_back(p[ix++]);
        }

        activeParam = -1;
    }

    curveIsDirty = true;
    queue_draw();
}

void MyDiagonalCurve::setPos(double pos, int chanIdx)
{
    assert(edited_point > -1);

    if (chanIdx == 0) {
        curve.x.at(edited_point) = pos;
    } else if (chanIdx == 1) {
        curve.y.at(edited_point) = pos;
    }

    curveIsDirty = true;
    queue_draw();
    notifyListener();
}

void MyDiagonalCurve::stopNumericalAdjustment()
{
    if (edited_point > -1) {
        edited_point = grab_point = lit_point = -1;
        coordinateAdjuster->stopNumericalAdjustment();
        queue_draw();
    }
}

void MyDiagonalCurve::setType(DiagonalCurveType t)
{

    curve.type = t;
    queue_draw();
}

void MyDiagonalCurve::setActiveParam(int ac)
{
    activeParam = ac;
    queue_draw();
}

void MyDiagonalCurve::updateBackgroundHistogram(const LUTu& hist)
{
    if (hist) {
        //memcpy (bghist, hist, 256*sizeof(unsigned int));
        for (int i = 0; i < 256; i++) {
            bghist[i] = hist[i];
        }

        //hist = bghist;
        bghistvalid = true;
    } else {
        bghistvalid = false;
    }

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

void MyDiagonalCurve::reset(const std::vector<double>& resetCurve, double identityValue)
{
    stopNumericalAdjustment();

    if (!resetCurve.empty()) {
        setPoints(resetCurve);
        return;
    }

    switch (curve.type) {
        case DCT_Spline:
        case DCT_NURBS:
        case DCT_CatumullRom:
            curve.x.resize(2);
            curve.y.resize(2);
            curve.x.at(0) = 0.;
            curve.y.at(0) = 0.;
            curve.x.at(1) = 1.;
            curve.y.at(1) = 1.;
            grab_point = -1;
            lit_point = -1;
            curveIsDirty = true;
            break;

        case DCT_Parametric:
            curve.x.resize(7);
            curve.y.clear();
            // the SHCSelector values doesn't really matter for the identity curve display
            curve.x.at(0) = 0.25;
            curve.x.at(1) = 0.50;
            curve.x.at(2) = 0.75;
            curve.x.at(3) = 0.00;
            curve.x.at(4) = 0.00;
            curve.x.at(5) = 0.00;
            curve.x.at(6) = 0.00;
            grab_point = -1;  // not sure that it's necessary
            lit_point = -1;   // not sure that it's necessary
            curveIsDirty = true;
            break;

        default:
            break;
    }

    queue_draw();
}
