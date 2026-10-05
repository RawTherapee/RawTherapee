/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Jean-Christophe FRISCH <natureh.510@gmail.com>
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

#include "editwidgets.h"

#include "editbuffer.h"
#include "editcallbacks.h"
#include "rtsurface.h"

const std::vector<double> Geometry::dash = {3., 1.5};

#define INNERGEOM_OPACITY 1.
#define OUTERGEOM_OPACITY 0.7

RGBColor Geometry::getInnerLineColor ()
{
    RGBColor color;

    if (flags & F_AUTO_COLOR) {
        if      (state == NORMAL)   {
            color.setColor (1., 1., 1.);           // White
        } else if (state == ACTIVE) {
            color.setColor (1., 1., 0.);           // Yellow
        } else if (state == PRELIGHT) {
            color.setColor (1., 100. / 255., 0.);  // Orange
        } else if (state == DRAGGED) {
            color.setColor (1., 0., 0.);           // Red
        }
    } else {
        color = innerLineColor;
    }

    return color;
}

RGBColor Geometry::getOuterLineColor ()
{
    RGBColor color;

    if (flags & F_AUTO_COLOR) {
        /*
        if      (state == NORMAL)   { color.setColor (0., 0., 0.); }  // Black
        else if (state == PRELIGHT) { color.setColor (0., 0., 0.); }  // Black
        else if (state == DRAGGED)  { color.setColor (1., 0., 0.); }  // Black
        */
        color.setColor (0., 0., 0.);  // Black
    } else {
        color = outerLineColor;
    }

    return color;
}

void Geometry::setMOChannelColor(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, unsigned short id)
{
    switch (objectBuffer->getObjectMode()) {
        case OM_255:
            /* In OM_255 mode, FORMAT_A8 format is used:
                - Alpha is represented with 8 bits (mask = 0xFF) from 0 to 255
            */
            cr->set_source_rgba (0., 0., 0., ((id + 1) & 0xFF) / 255.);
            break;
        case OM_65535:
        default:
            /* In OM_65535 mode, FORMAT_RGB16_565 format is used:
                - Red is represented with 5 bits (mask = 0xF800, left shift = 11) from 0 to 31
                - Green is represented with 6 bits (mask = 0x7E0, left shift = 5) from 0 to 63
                - Blue is represented with 5 bits (mask = 0x1F) from 0 to 31
            */
            const double red = (((id + 1) & 0xF800) >> 11) / 31.;
            const double green = (((id + 1) & 0x7E0) >> 5) / 63.;
            const double blue = ((id + 1) & 0x1F) / 31.;
            cr->set_source_rgb (red, green, blue);
    }
}

#ifdef GUIVERSION

void Circle::drawOuterGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    double lineWidth = getOuterLineWidth();
    if ((flags & F_VISIBLE) && state != INSENSITIVE && lineWidth > 0. && innerLineWidth > 0.) {
        RGBColor color;

        if (flags & F_AUTO_COLOR) {
            color = getOuterLineColor();
        } else {
            color = outerLineColor;
        }

        cr->set_source_rgba (color.getR(), color.getG(), color.getB(), OUTERGEOM_OPACITY * rtengine::min(innerLineWidth / 2.f, 1.f));
        cr->set_line_width (lineWidth);
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);

        rtengine::Coord center_ = center;
        double radius_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas(double(radius)) : double(radius);

        if (datum == IMAGE) {
            coordSystem.imageCoordToScreen (center.x, center.y, center_.x, center_.y);
        } else if (datum == CLICKED_POINT) {
            center_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            center_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        cr->arc(center_.x + 0.5, center_.y + 0.5, radius_, 0., 2.*rtengine::RT_PI);
        cr->stroke();
    }
}

void Circle::drawInnerGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_VISIBLE) {
        if (state != INSENSITIVE) {
            RGBColor color;

            if (flags & F_AUTO_COLOR) {
                color = getInnerLineColor();
            } else {
                color = innerLineColor;
            }

            cr->set_source_rgba (color.getR(), color.getG(), color.getB(), INNERGEOM_OPACITY);
        }

        cr->set_line_width(innerLineWidth);
        cr->set_line_cap(flags & F_DASHED ? Cairo::LINE_CAP_BUTT : Cairo::LINE_CAP_ROUND);

        rtengine::Coord center_ = center;
        double radius_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas(double(radius)) : double(radius);

        if (datum == IMAGE) {
            coordSystem.imageCoordToScreen (center.x, center.y, center_.x, center_.y);
        } else if (datum == CLICKED_POINT) {
            center_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            center_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if (flags & F_DASHED) {
            cr->set_dash(dash, 0.);
        }

        if (filled) {
            cr->arc(center_.x + 0.5, center_.y + 0.5, radius_, 0., 2.*rtengine::RT_PI);
            if (innerLineWidth > 0.f) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else if (innerLineWidth > 0.f) {
            cr->arc(center_.x + 0.5, center_.y + 0.5, radius_, 0., 2.*rtengine::RT_PI);
            cr->stroke();
        }

        if (flags & F_DASHED) {
            cr->unset_dash();
        }
}
}

void Circle::drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_HOVERABLE) {
        cr->set_line_width( getMouseOverLineWidth() );
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);
        rtengine::Coord center_ = center;
        double radius_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas(double(radius)) : double(radius);

        if (datum == IMAGE) {
            coordSystem.imageCoordToCropCanvas (center.x, center.y, center_.x, center_.y);
        } else if (datum == CLICKED_POINT) {
            center_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            center_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        // Setting MO Channel color according to the objet's ID
        setMOChannelColor(cr, objectBuffer, id);

        cr->arc(center_.x + 0.5, center_.y + 0.5, radius_, 0, 2.*rtengine::RT_PI);

        if (filled) {
            if (innerLineWidth > 0.f) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else {
            cr->stroke();
        }
    }
}

void Line::drawOuterGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    double lineWidth = getOuterLineWidth();
    if ((flags & F_VISIBLE) && state != INSENSITIVE && lineWidth > 0. && innerLineWidth > 0.) {
        RGBColor color;

        if (flags & F_AUTO_COLOR) {
            color = getOuterLineColor();
        } else {
            color = outerLineColor;
        }

        cr->set_source_rgba (color.getR(), color.getG(), color.getB(), OUTERGEOM_OPACITY * rtengine::min(innerLineWidth / 2.f, 1.f));
        cr->set_line_width (lineWidth);
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);

        rtengine::Coord begin_ = begin;
        rtengine::Coord end_ = end;

        if (datum == IMAGE) {
            coordSystem.imageCoordToScreen (begin.x, begin.y, begin_.x, begin_.y);
            coordSystem.imageCoordToScreen (end.x, end.y, end_.x, end_.y);
        } else if (datum == CLICKED_POINT) {
            begin_ += objectBuffer->getDataProvider()->posScreen;
            end_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            begin_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
            end_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        cr->move_to(begin_.x + 0.5, begin_.y + 0.5);
        cr->line_to(end_.x + 0.5, end_.y + 0.5);
        cr->stroke();
    }
}

void Line::drawInnerGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if ((flags & F_VISIBLE) && innerLineWidth > 0.f) {
        if (state != INSENSITIVE) {
            RGBColor color;

            if (flags & F_AUTO_COLOR) {
                color = getInnerLineColor();
            } else {
                color = innerLineColor;
            }

            cr->set_source_rgba (color.getR(), color.getG(), color.getB(), INNERGEOM_OPACITY);
        }

        cr->set_line_width(innerLineWidth);
        cr->set_line_cap(flags & F_DASHED ? Cairo::LINE_CAP_BUTT : Cairo::LINE_CAP_ROUND);

        rtengine::Coord begin_ = begin;
        rtengine::Coord end_ = end;

        if (datum == IMAGE) {
            coordSystem.imageCoordToScreen (begin.x, begin.y, begin_.x, begin_.y);
            coordSystem.imageCoordToScreen (end.x, end.y, end_.x, end_.y);
        } else if (datum == CLICKED_POINT) {
            begin_ += objectBuffer->getDataProvider()->posScreen;
            end_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            begin_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
            end_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if (flags & F_DASHED) {
            cr->set_dash(dash, 0.);
        }

        cr->move_to(begin_.x + 0.5, begin_.y + 0.5);
        cr->line_to(end_.x + 0.5, end_.y + 0.5);
        cr->stroke();

        if (flags & F_DASHED) {
            cr->unset_dash();
        }
    }
}

void Line::drawToMOChannel(Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_HOVERABLE) {
        cr->set_line_width( getMouseOverLineWidth() );
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);
        rtengine::Coord begin_ = begin;
        rtengine::Coord end_ = end;

        if (datum == IMAGE) {
            coordSystem.imageCoordToCropCanvas (begin.x, begin.y, begin_.x, begin_.y);
            coordSystem.imageCoordToCropCanvas (end.x, end.y, end_.x, end_.y);
        } else if (datum == CLICKED_POINT) {
            begin_ += objectBuffer->getDataProvider()->posScreen;
            end_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            begin_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
            end_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        // Setting MO Channel color according to the objet's ID
        setMOChannelColor(cr, objectBuffer, id);

        cr->move_to(begin_.x + 0.5, begin_.y + 0.5);
        cr->line_to(end_.x + 0.5, end_.y + 0.5);
        cr->stroke();
    }
}

void Polyline::drawOuterGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    double lineWidth = getOuterLineWidth();
    if ((flags & F_VISIBLE) && state != INSENSITIVE && points.size() > 1 && lineWidth > 0.) {
        RGBColor color;

        if (flags & F_AUTO_COLOR) {
            color = getOuterLineColor();
        } else {
            color = outerLineColor;
        }

        cr->set_source_rgba (color.getR(), color.getG(), color.getB(), OUTERGEOM_OPACITY * rtengine::min(innerLineWidth / 2.f, 1.f));
        cr->set_line_width (lineWidth);
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);

        rtengine::Coord currPos;

        for (unsigned int i = 0; i < points.size(); ++i) {
            currPos  = points.at(i);

            if      (datum == IMAGE) {
                coordSystem.imageCoordToScreen (points.at(i).x, points.at(i).y, currPos.x, currPos.y);
            } else if (datum == CLICKED_POINT) {
                currPos += objectBuffer->getDataProvider()->posScreen;
            } else if (datum == CURSOR) {
                currPos += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
            }

            if (!i) {
                cr->move_to(currPos.x + 0.5, currPos.y + 0.5);
            } else {
                cr->line_to(currPos.x + 0.5, currPos.y + 0.5);
            }
        }

        if (filled) {
            cr->fill_preserve();
            cr->stroke();
        } else {
            cr->stroke();
        }
    }
}

void Polyline::drawInnerGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if ((flags & F_VISIBLE) && points.size() > 1) {
        if (state != INSENSITIVE) {
            RGBColor color;

            if (flags & F_AUTO_COLOR) {
                color = getInnerLineColor();
            } else {
                color = innerLineColor;
            }

            cr->set_source_rgba (color.getR(), color.getG(), color.getB(), INNERGEOM_OPACITY);
        }

        cr->set_line_width(innerLineWidth);
        cr->set_line_cap(flags & F_DASHED ? Cairo::LINE_CAP_BUTT : Cairo::LINE_CAP_ROUND);
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);

        if (flags & F_DASHED) {
            cr->set_dash(dash, 0.);
        }

        if (filled && state != INSENSITIVE) {
            rtengine::Coord currPos;

            for (unsigned int i = 0; i < points.size(); ++i) {
                currPos  = points.at(i);

                if      (datum == IMAGE) {
                    coordSystem.imageCoordToScreen (points.at(i).x, points.at(i).y, currPos.x, currPos.y);
                } else if (datum == CLICKED_POINT) {
                    currPos += objectBuffer->getDataProvider()->posScreen;
                } else if (datum == CURSOR) {
                    currPos += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
                }

                if (!i) {
                    cr->move_to(currPos.x + 0.5, currPos.y + 0.5);
                } else {
                    cr->line_to(currPos.x + 0.5, currPos.y + 0.5);
                }
            }

            if (innerLineWidth > 0.f) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else if (innerLineWidth > 0.f) {
            rtengine::Coord currPos;

            for (unsigned int i = 0; i < points.size(); ++i) {
                currPos  = points.at(i);

                if (datum == IMAGE) {
                    coordSystem.imageCoordToScreen (points.at(i).x, points.at(i).y, currPos.x, currPos.y);
                } else if (datum == CLICKED_POINT) {
                    currPos += objectBuffer->getDataProvider()->posScreen;
                } else if (datum == CURSOR) {
                    currPos += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
                }

                if (!i) {
                    cr->move_to(currPos.x + 0.5, currPos.y + 0.5);
                } else {
                    cr->line_to(currPos.x + 0.5, currPos.y + 0.5);
                }
            }
            cr->stroke();
        }

        if (flags & F_DASHED) {
            cr->unset_dash();
        }
    }
}

void Polyline::drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if ((flags & F_HOVERABLE) && points.size() > 1) {
        rtengine::Coord currPos;

        // Setting MO Channel color according to the objet's ID
        setMOChannelColor(cr, objectBuffer, id);

        cr->set_line_width( getMouseOverLineWidth() );
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);

        for (unsigned int i = 0; i < points.size(); ++i) {
            currPos  = points.at(i);

            if      (datum == IMAGE) {
                coordSystem.imageCoordToCropCanvas (points.at(i).x, points.at(i).y, currPos.x, currPos.y);
            } else if (datum == CLICKED_POINT) {
                currPos += objectBuffer->getDataProvider()->posScreen;
            } else if (datum == CURSOR) {
                currPos += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
            }

            if (!i) {
                cr->move_to(currPos.x + 0.5, currPos.y + 0.5);
            } else {
                cr->line_to(currPos.x + 0.5, currPos.y + 0.5);
            }
        }

        if (filled) {
            if (innerLineWidth > 0.f) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else {
            cr->stroke();
        }
    }
}

void EditRectangle::setXYWH(int left, int top, int width, int height)
{
    topLeft.set(left, top);
    bottomRight.set(left + width, top + height);
}

void EditRectangle::setXYXY(int left, int top, int right, int bottom)
{
    topLeft.set(left, top);
    bottomRight.set(right, bottom);
}

void EditRectangle::setXYWH(rtengine::Coord topLeft, rtengine::Coord widthHeight)
{
    this->topLeft = topLeft;
    this->bottomRight = topLeft + widthHeight;
}

void EditRectangle::setXYXY(rtengine::Coord topLeft, rtengine::Coord bottomRight)
{
    this->topLeft = topLeft;
    this->bottomRight = bottomRight;
}

void EditRectangle::drawOuterGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    double lineWidth = getOuterLineWidth();
    if ((flags & F_VISIBLE) && state != INSENSITIVE && lineWidth > 0. && innerLineWidth > 0.) {
        RGBColor color;

        if (flags & F_AUTO_COLOR) {
            color = getOuterLineColor();
        } else {
            color = outerLineColor;
        }

        cr->set_source_rgba (color.getR(), color.getG(), color.getB(), OUTERGEOM_OPACITY * rtengine::min(innerLineWidth / 2.f, 1.f));
        cr->set_line_width (lineWidth);
        cr->set_line_join(Cairo::LINE_JOIN_BEVEL);

        rtengine::Coord tl, br;

        if      (datum == IMAGE) {
            coordSystem.imageCoordToScreen (topLeft.x, topLeft.y, tl.x, tl.y);
        } else if (datum == CLICKED_POINT) {
            tl = topLeft + objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            tl = topLeft + objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if      (datum == IMAGE) {
            coordSystem.imageCoordToScreen (bottomRight.x, bottomRight.y, br.x, br.y);
        } else if (datum == CLICKED_POINT) {
            br = bottomRight + objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            br = bottomRight + objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        cr->rectangle(tl.x + 0.5, tl.y + 0.5, br.x - tl.x, br.y - tl.y);

        if (filled) {
            cr->fill_preserve();
            cr->stroke();
        } else {
            cr->stroke();
        }
    }
}

void EditRectangle::drawInnerGeometry(Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_VISIBLE) {
        if (state != INSENSITIVE) {
            RGBColor color;

            if (flags & F_AUTO_COLOR) {
                color = getInnerLineColor();
            } else {
                color = innerLineColor;
            }

            cr->set_source_rgba (color.getR(), color.getG(), color.getB(), INNERGEOM_OPACITY);
        }

        cr->set_line_width(innerLineWidth);
        cr->set_line_join(Cairo::LINE_JOIN_BEVEL);

        rtengine::Coord tl, br;

        if      (datum == IMAGE) {
            coordSystem.imageCoordToScreen (topLeft.x, topLeft.y, tl.x, tl.y);
        } else if (datum == CLICKED_POINT) {
            tl = topLeft + objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            tl = topLeft + objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if      (datum == IMAGE) {
            coordSystem.imageCoordToScreen (bottomRight.x, bottomRight.y, br.x, br.y);
        } else if (datum == CLICKED_POINT) {
            br = bottomRight + objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            br = bottomRight + objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if (flags & F_DASHED) {
            cr->set_dash(dash, 0.);
        }

        if (filled) {
            cr->rectangle(tl.x + 0.5, tl.y + 0.5, br.x - tl.x, br.y - tl.y);

            if (innerLineWidth > 0.f) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else if (innerLineWidth > 0.f) {
            cr->rectangle(tl.x + 0.5, tl.y + 0.5, br.x - tl.x, br.y - tl.y);
            cr->stroke();
        }

        if (flags & F_DASHED) {
            cr->unset_dash();
        }
    }
}

void EditRectangle::drawToMOChannel(Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_HOVERABLE) {
        cr->set_line_width( getMouseOverLineWidth() );
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);

        rtengine::Coord tl, br;

        if      (datum == IMAGE) {
            coordSystem.imageCoordToCropCanvas (topLeft.x, topLeft.y, tl.x, tl.y);
        } else if (datum == CLICKED_POINT) {
            tl = topLeft + objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            tl = topLeft + objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if      (datum == IMAGE) {
            coordSystem.imageCoordToCropCanvas (bottomRight.x, bottomRight.y, br.x, br.y);
        } else if (datum == CLICKED_POINT) {
            br = bottomRight + objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            br = bottomRight + objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        // Setting MO Channel color according to the objet's ID
        setMOChannelColor(cr, objectBuffer, id);

        cr->rectangle(tl.x + 0.5, tl.y + 0.5, br.x - tl.x, br.y - tl.y);

        if (filled) {
            if (innerLineWidth > 0.f) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else {
            cr->stroke();
        }
    }
}

void Ellipse::drawEllipsePath (Cairo::RefPtr<Cairo::Context> &cr, const rtengine::Coord &center_, double radYT_, double radY_, double radXL_, double radX_)
{
    // The ellipse is drawn quadrant by quadrant: a circle of radius 1 is translated to the
    // center, rotated by the ellipse angle then scaled with the corresponding pair of
    // half-radii. The path persists across save/restore, so the four arcs join seamlessly.
    const double rad = angle * rtengine::RT_PI / 180.;

    const auto drawQuadrant = [&cr, &center_, rad](double radx, double rady, double phase0, double phase1) {
        cr->save();
        cr->translate(center_.x, center_.y);
        cr->rotate(rad);
        cr->scale(radx, rady);
        cr->arc(0., 0., 1.0, phase0, phase1);
        cr->restore();
    };

    drawQuadrant(radX_, radY_, 0.0, rtengine::RT_PI_2); // Bottom-right part
    drawQuadrant(radXL_, radY_, rtengine::RT_PI_2, rtengine::RT_PI); // Bottom-left part
    drawQuadrant(radXL_, radYT_, rtengine::RT_PI, 3. * rtengine::RT_PI_2); // Top-left part
    drawQuadrant(radX_, radYT_, 3. * rtengine::RT_PI_2, 2. * rtengine::RT_PI); // Top-right part
}

void Ellipse::drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if ((flags & F_VISIBLE) && state != INSENSITIVE) {
        RGBColor color;

        if (flags & F_AUTO_COLOR) {
            color = getOuterLineColor();
        } else {
            color = outerLineColor;
        }

        cr->set_source_rgba (color.getR(), color.getG(), color.getB(), opacity / 100.);
        cr->set_line_width ( getOuterLineWidth() );

        rtengine::Coord center_ = center;
        double radYT_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radYT)) : double (radYT);
        double radY_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radY)) : double (radY);
        double radXL_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radXL)) : double (radXL);
        double radX_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radX)) : double (radX);

        if (datum == IMAGE) {
            coordSystem.imageCoordToScreen (center.x, center.y, center_.x, center_.y);
        } else if (datum == CLICKED_POINT) {
            center_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            center_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if (radYT_ > 0 && radY_ > 0 && radXL_ > 0 && radX_ > 0) {
            drawEllipsePath(cr, center_, radYT_, radY_, radXL_, radX_);
            cr->stroke ();
        }
    }
}

void Ellipse::drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_VISIBLE) {
        if (state != INSENSITIVE) {
            RGBColor color;

            if (flags & F_AUTO_COLOR) {
                color = getInnerLineColor();
            } else {
                color = innerLineColor;
            }

            cr->set_source_rgba (color.getR(), color.getG(), color.getB(), opacity / 100.);
        }

        cr->set_line_width ( innerLineWidth );

        rtengine::Coord center_ = center;
        double radYT_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radYT)) : double (radYT);
        double radY_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radY)) : double (radY);
        double radXL_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radXL)) : double (radXL);
        double radX_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radX)) : double (radX);

        if (datum == IMAGE) {
            coordSystem.imageCoordToScreen (center.x, center.y, center_.x, center_.y);
        } else if (datum == CLICKED_POINT) {
            center_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            center_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        if (filled && state != INSENSITIVE) {
            if (radYT_ > 0 && radY_ > 0 && radXL_ > 0 && radX_ > 0) {
                drawEllipsePath(cr, center_, radYT_, radY_, radXL_, radX_);
                cr->stroke ();
            }

            if (innerLineWidth > 0.) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else if (innerLineWidth > 0.) {
            if (radYT_ > 0 && radY_ > 0 && radXL_ > 0 && radX_ > 0) {
                drawEllipsePath(cr, center_, radYT_, radY_, radXL_, radX_);
                cr->stroke ();
            }

            if (state == INSENSITIVE) {
                std::valarray<double> ds (1);
                ds[0] = 4;
                cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
                cr->stroke_preserve();
                cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
                cr->set_dash (ds, 0);
                cr->stroke();
                ds.resize (0);
                cr->set_dash (ds, 0);
            } else {
                cr->stroke();
            }
        }
    }
}

void Ellipse::drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_HOVERABLE) {
        cr->set_line_width ( getMouseOverLineWidth() );

        rtengine::Coord center_ = center;
        double radYT_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radYT)) : double (radYT);
        double radY_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radY)) : double (radY);
        double radXL_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radXL)) : double (radXL);
        double radX_ = radiusInImageSpace ? coordSystem.scaleValueToCanvas (double (radX)) : double (radX);

        if (datum == IMAGE) {
            coordSystem.imageCoordToCropCanvas (center.x, center.y, center_.x, center_.y);
        } else if (datum == CLICKED_POINT) {
            center_ += objectBuffer->getDataProvider()->posScreen;
        } else if (datum == CURSOR) {
            center_ += objectBuffer->getDataProvider()->posScreen + objectBuffer->getDataProvider()->deltaScreen;
        }

        // Setting MO Channel color according to the objet's ID
        setMOChannelColor(cr, objectBuffer, id);

        if (radYT_ > 0 && radY_ > 0 && radXL_ > 0 && radX_ > 0) {
            drawEllipsePath(cr, center_, radYT_, radY_, radXL_, radX_);
            cr->stroke ();
        }

        if (filled) {
            if (innerLineWidth > 0.) {
                cr->fill_preserve();
                cr->stroke();
            } else {
                cr->fill();
            }
        } else {
            cr->stroke();
        }
    }
}

void OPIcon::drivenPointToRectangle(const rtengine::Coord &pos,
                                    rtengine::Coord &topLeft, rtengine::Coord &bottomRight, int W, int H)
{
    switch (drivenPoint) {
    case (DP_CENTERCENTER):
        topLeft.x = pos.x - W / 2;
        topLeft.y = pos.y - H / 2;
        break;

    case (DP_TOPLEFT):
        topLeft.x = pos.x;
        topLeft.y = pos.y;
        break;

    case (DP_TOPCENTER):
        topLeft.x = pos.x - W / 2;
        topLeft.y = pos.y;
        break;

    case (DP_TOPRIGHT):
        topLeft.x = pos.x - W;
        topLeft.y = pos.y;
        break;

    case (DP_CENTERRIGHT):
        topLeft.x = pos.x - W;
        topLeft.y = pos.y - H / 2;
        break;

    case (DP_BOTTOMRIGHT):
        topLeft.x = pos.x - W;
        topLeft.y = pos.y - H;
        break;

    case (DP_BOTTOMCENTER):
        topLeft.x = pos.x - W / 2;
        topLeft.y = pos.y - H;
        break;

    case (DP_BOTTOMLEFT):
        topLeft.x = pos.x;
        topLeft.y = pos.y - H;
        break;

    case (DP_CENTERLEFT):
        topLeft.x = pos.x;
        topLeft.y = pos.y - H / 2;
        break;
    }

    bottomRight.x = topLeft.x + W - 1;
    bottomRight.y = topLeft.y + H - 1;
}

OPIcon::OPIcon(const std::shared_ptr<RTSurface> &normal,
               const std::shared_ptr<RTSurface> &active,
               const std::shared_ptr<RTSurface> &prelight,
               const std::shared_ptr<RTSurface> &dragged,
               const std::shared_ptr<RTSurface>&insensitive,
               DrivenPoint drivenPoint) :
    drivenPoint(drivenPoint)
{
    if (normal) {
        normalImg = normal;
    }

    if (prelight) {
        prelightImg = prelight;
    }

    if (active) {
        activeImg = active;
    }

    if (dragged) {
        draggedImg = dragged;
    }

    if (insensitive) {
        insensitiveImg = insensitive;
    }
}

OPIcon::OPIcon(Glib::ustring normalImage, Glib::ustring activeImage, Glib::ustring prelightImage,
               Glib::ustring  draggedImage, Glib::ustring insensitiveImage, DrivenPoint drivenPoint) : drivenPoint(drivenPoint)
{
    if (!normalImage.empty()) {
        normalImg = std::shared_ptr<RTSurface>(new RTSurface(normalImage, Gtk::ICON_SIZE_MENU));
    }

    if (!prelightImage.empty()) {
        prelightImg = std::shared_ptr<RTSurface>(new RTSurface(prelightImage, Gtk::ICON_SIZE_MENU));
    }

    if (!activeImage.empty()) {
        activeImg = std::shared_ptr<RTSurface>(new RTSurface(activeImage, Gtk::ICON_SIZE_MENU));
    }

    if (!draggedImage.empty()) {
        draggedImg = std::shared_ptr<RTSurface>(new RTSurface(draggedImage, Gtk::ICON_SIZE_MENU));
    }

    if (!insensitiveImage.empty()) {
        insensitiveImg = std::shared_ptr<RTSurface>(new RTSurface(insensitiveImage, Gtk::ICON_SIZE_MENU));
    }
}

const std::shared_ptr<RTSurface> OPIcon::getNormalImg()
{
    return normalImg;
}
const std::shared_ptr<RTSurface> OPIcon::getPrelightImg()
{
    return prelightImg;
}
const std::shared_ptr<RTSurface> OPIcon::getActiveImg()
{
    return activeImg;
}
const std::shared_ptr<RTSurface> OPIcon::getDraggedImg()
{
    return draggedImg;
}
const std::shared_ptr<RTSurface> OPIcon::getInsensitiveImg()
{
    return insensitiveImg;
}

void OPIcon::drawImage(std::shared_ptr<RTSurface> &img,
                       Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer,
                       EditCoordSystem &coordSystem)
{
    int imgW = img->getWidth();
    int imgH = img->getHeight();

    rtengine::Coord pos;

    if (datum == IMAGE) {
        coordSystem.imageCoordToScreen(position.x, position.y, pos.x, pos.y);
    } else if (datum == CLICKED_POINT) {
        pos = position + objectBuffer->getDataProvider()->posScreen;
    } else if (datum == CURSOR)
        pos = position + objectBuffer->getDataProvider()->posScreen
              + objectBuffer->getDataProvider()->deltaScreen;

    rtengine::Coord tl, br; // Coordinate of the rectangle in the CropBuffer coordinate system
    drivenPointToRectangle(pos, tl, br, imgW, imgH);

    cr->set_source(img->get(), tl.x, tl.y);
    cr->set_line_width(0.);
    cr->rectangle(tl.x, tl.y, imgW, imgH);
    cr->fill();
}

void OPIcon::drawMOImage(std::shared_ptr<RTSurface> &img, Cairo::RefPtr<Cairo::Context> &cr,
                         unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    // test of F_HOVERABLE has already been done

    int imgW = img->getWidth();
    int imgH = img->getHeight();

    rtengine::Coord pos;

    if (datum == IMAGE)
        coordSystem.imageCoordToCropCanvas (position.x, position.y, pos.x, pos.y);
    else if (datum == CLICKED_POINT) {
        pos = position + objectBuffer->getDataProvider()->posScreen;
    } else if (datum == CURSOR)
        pos = position + objectBuffer->getDataProvider()->posScreen
              + objectBuffer->getDataProvider()->deltaScreen;

    rtengine::Coord tl, br; // Coordinate of the rectangle in the CropBuffer coordinate system
    drivenPointToRectangle(pos, tl, br, imgW, imgH);

    // Setting MO Channel color according to the objet's ID
    setMOChannelColor(cr, objectBuffer, id);

    cr->set_line_width(0.);
    cr->rectangle(tl.x, tl.y, imgW, imgH);
    cr->fill();
}

void OPIcon::drawOuterGeometry(Cairo::RefPtr<Cairo::Context> &cr,
                               ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) {}

void OPIcon::drawInnerGeometry(Cairo::RefPtr<Cairo::Context> &cr,
                               ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_VISIBLE) {
        // Here we will handle fall-back solutions

        State tmpState = state;  // can be updated through the successive test

        if (tmpState == INSENSITIVE) {
            if (!insensitiveImg) {
                tmpState = NORMAL;
            } else {
                OPIcon::drawImage(insensitiveImg, cr, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == DRAGGED) {
            if (!draggedImg) {
                tmpState = ACTIVE;
            } else {
                OPIcon::drawImage(draggedImg, cr, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == ACTIVE) {
            if (!activeImg) {
                tmpState = PRELIGHT;
            } else {
                OPIcon::drawImage(activeImg, cr, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == PRELIGHT) {
            if (!prelightImg) {
                tmpState = NORMAL;
            } else {
                OPIcon::drawImage(prelightImg, cr, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == NORMAL && normalImg) {
            OPIcon::drawImage(normalImg, cr, objectBuffer, coordSystem);
        }
    }
}

void OPIcon::drawToMOChannel(Cairo::RefPtr<Cairo::Context> &cr, unsigned short id,
                             ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem)
{
    if (flags & F_HOVERABLE) {
        // Here we will handle fallback solutions
        State tmpState = state;

        if (tmpState == INSENSITIVE) {
            if (!insensitiveImg) {
                tmpState = NORMAL;
            } else {
                OPIcon::drawMOImage(insensitiveImg, cr, id, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == DRAGGED) {
            if (!draggedImg) {
                tmpState = ACTIVE;
            } else {
                OPIcon::drawMOImage(draggedImg, cr, id, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == ACTIVE) {
            if (!activeImg) {
                tmpState = PRELIGHT;
            } else {
                OPIcon::drawMOImage(activeImg, cr, id, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == PRELIGHT) {
            if (!prelightImg) {
                tmpState = NORMAL;
            } else {
                OPIcon::drawMOImage(prelightImg, cr, id, objectBuffer, coordSystem);
                return;
            }
        }

        if (tmpState == NORMAL && normalImg) {
            OPIcon::drawMOImage(normalImg, cr, id, objectBuffer, coordSystem);
        }
    }
}

#endif
