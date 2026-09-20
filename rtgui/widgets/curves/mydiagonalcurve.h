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

#include "rtengine/diagonalcurvetypes.h"

template <typename T> class LUT;

using LUTf = LUT<float>;

class DiagonalCurveDescr
{

public:
    DiagonalCurveType type;
    // in case of parametric curves the curve parameters are stored in vector x. In other cases these vectors store the coordinates of the bullets.
    std::vector<double> x, y;
};

class MyDiagonalCurve final : public MyCurve
{
private:
    IdleRegister idle_register;

protected:
    DiagonalCurveDescr curve;
    int grab_point;     // the point that the user is moving by mouse
    int closest_point;  // the point that is the closest from the cursor
    int lit_point;      // the point that is lit when the cursor is near it
    double clampedX;    // clamped grabbed point X coordinates in the [0;1] range
    double clampedY;    // clamped grabbed point Y coordinates in the [0;1] range
    // signed X distance of the cursor between two consecutive MOTION_NOTIFY
    double deltaX;
    // signed Y distance of the cursor between two consecutive MOTION_NOTIFY
    double deltaY;
    double distanceX;  // X distance from the cursor to the closest point
    double distanceY;  // Y distance from the cursor to the closest point
    double ugpX;       // unclamped grabbed point X coordinate in the graph
    double ugpY;       // unclamped grabbed point Y coordinate in the graph
    int activeParam;
    unsigned int* bghist;  // histogram values
    bool bghistvalid;
    double locallabRef;  // Locallab reference value to display in the background

    void updateDrawingArea(const int handle, const ::Cairo::RefPtr<Cairo::Context>& cr);
    void interpolate();
    void findClosestPoint();
    CursorShape
    motionNotify(CursorShape type, double minDistanceX, double minDistanceY, int num);
    std::vector<double> get_vector(int veclen) override;
    void get_LUT(LUTf& lut);
    // Get the cursor position and unclamped position from the curve given an X value ; BEWARE: can be time consuming, use with care
    void getCursorPositionFromCurve(float x);
    void getCursorPositionFromCurve(int x);
    // Get the cursor position and unclamped value depending on cursor's position in the graph
    void getCursorPosition(Gdk::EventType evType,
                           bool isHint,
                           int evX,
                           int evY,
                           Gdk::ModifierType modifierKey);

public:
    MyDiagonalCurve();
    ~MyDiagonalCurve() override;
    std::vector<double> getPoints() override;
    void setPoints(const std::vector<double>& p) override;
    void setType(DiagonalCurveType t);
    bool on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr) override;
    bool handleEvents(GdkEvent* event) override;
    void setActiveParam(int ac);
    void reset(const std::vector<double>& resetCurve,
               double identityValue = 0.5) override;
    void updateBackgroundHistogram(const LUTu& hist);

    void pipetteMouseOver(CurveEditor* ce,
                          EditDataProvider* provider,
                          int modifierKey) override;
    bool pipetteButton1Pressed(EditDataProvider* provider, int modifierKey) override;
    void pipetteButton1Released(EditDataProvider* provider) override;
    void pipetteDrag(EditDataProvider* provider, int modifierKey) override;
    void updateLocallabBackground(double ref);

    void setPos(double pos, int chanIdx) override;
    void stopNumericalAdjustment() override;
};
