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
#pragma once

#include <vector>

#include <gtkmm.h>

#include "cursormanager.h"
#include "curvelistener.h"
#include "mycurve.h"
#include "rtengine/flatcurvetypes.h"

enum MouseOverAreas {
    FCT_Area_None = 1 << 0,       // over a zone that don't have any
    FCT_Area_Insertion = 1 << 1,  // where the user can insert a new point
    FCT_Area_Point = 1 << 2,      // over an existing point
    // cursor is on the horizontal line going through the edited control point
    FCT_Area_H = 1 << 3,
    // cursor is on the vertical line going through the edited control point
    FCT_Area_V = 1 << 4,
    FCT_Area_LeftTan = 1 << 5,  // in the left tangent edition area
    FCT_Area_RightTan = 1 << 6  // in the right tangent edition area
};

enum EditedHandle {
    FCT_EditedHandle_None = 1 << 0,
    FCT_EditedHandle_CPointUD = 1 << 1,  // UD stands for Unknown Direction
    FCT_EditedHandle_CPoint = 1 << 2,
    FCT_EditedHandle_CPointX = 1 << 3,
    FCT_EditedHandle_CPointY = 1 << 4,
    FCT_EditedHandle_LeftTan = 1 << 5,
    FCT_EditedHandle_RightTan = 1 << 6
};

class FlatCurveDescr
{

public:
    FlatCurveType type;
    std::vector<double> x;  // Range: [0.0 - 1.0]
    std::vector<double> y;  // Range: [0.0 - 1.0], default value = 0.5
    // Range: [0.0 - 1.0], where 1.0 = distance from previous to this point
    std::vector<double> leftTangent;
    // Range: [0.0 - 1.0], where 1.0 = distance from this to next point
    std::vector<double> rightTangent;
};

class HandlePosition
{

public:
    double centerX;
    double centerY;
};

class MyFlatCurve final : public MyCurve
{
private:
    IdleRegister idle_register;

protected:
    FlatCurveDescr curve;
    int closest_point;  // the point that is the closest from the cursor
    int lit_point;      // the point that is lit when the cursor is near it
    double clampedX;    // clamped grabbed point X coordinates in the [0;1] range
    double clampedY;    // clamped grabbed point Y coordinates in the [0;1] range
    // signed X distance of the cursor between two consecutive MOTION_NOTIFY
    double deltaX;
    // signed Y distance of the cursor between two consecutive MOTION_NOTIFY
    double deltaY;
    double distanceX;       // X distance from the cursor to the closest point
    double distanceY;       // Y distance from the cursor to the closest point
    double ugpX;            // unclamped grabbed point X coordinate in the graph
    double ugpY;            // unclamped grabbed point Y coordinate in the graph
    double leftTanX;        // X position of the left tangent handle
    double rightTanX;       // X position of the right tangent handle
    double preciseCursorX;  // X coordinate in the graph of the cursor, as a double value
    double preciseCursorY;  // Y coordinate in the graph of the cursor, as a double value
    double minDistanceX;    // X minimal distance before point suppression
    double minDistanceY;    // Y minimal distance before point suppression
    // Backup of the X value of the edited point, when deleted while being dragged
    double deletedPointX;
    // XY coordinate if the upper left and bottom right corner of the left tangent handle
    HandlePosition leftTanHandle;
    // XY coordinate if the upper left and bottom right corner of the right tangent handle
    HandlePosition rightTanHandle;
    bool tanHandlesDisplayed;  // True if the tangent handles are displayed
    bool periodic;             // Flat curves are periodic by default
    enum EditedHandle editedHandle;
    bool draggingElement;
    enum MouseOverAreas area;
    double locallabRef;  // Locallab reference value to display in the background

    void updateDrawingArea(const ::Cairo::RefPtr<Cairo::Context>& cr);
    void movePoint(bool moveX, bool moveY, bool pipetteDrag = false);
    void defaultCurve(double iVal = 0.5);
    void interpolate();
    void getCursorPosition(Gdk::EventType evType,
                           bool isHint,
                           int evX,
                           int evY,
                           Gdk::ModifierType modifier);
    void getMouseOverArea();
    bool getHandles(int n);
    CursorShape
    motionNotify(CursorShape type, double minDistanceX, double minDistanceY, int num);
    std::vector<double> get_vector(int veclen) override;
    void get_LUT(LUTf& lut);

public:
    MyFlatCurve();
    //~MyFlatCurve ();
    std::vector<double> getPoints() override;
    void setPeriodicity(bool isPeriodic) { periodic = isPeriodic; };
    void setPoints(const std::vector<double>& p) override;
    void setType(FlatCurveType t);
    bool on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr) override;
    bool handleEvents(GdkEvent* event) override;
    void reset(const std::vector<double>& resetCurve,
               double identityValue = 0.5) override;
    //void updateBackgroundHistogram (unsigned int* hist);

    void pipetteMouseOver(CurveEditor* ce,
                          EditDataProvider* provider,
                          int modifierKey) override;
    bool pipetteButton1Pressed(EditDataProvider* provider, int modifierKey) override;
    void pipetteButton1Released(EditDataProvider* provider) override;
    void pipetteDrag(EditDataProvider* provider, int modifierKey) override;

    void setPos(double pos, int chanIdx) override;
    void stopNumericalAdjustment() override;

    void updateLocallabBackground(double ref);
};
