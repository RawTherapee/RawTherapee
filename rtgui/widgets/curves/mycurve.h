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
#include "widgets/basic/coloredbar.h"
#include "widgets/basic/coordinateadjuster.h"

#include "rtengine/LUT.h"
#include "rtengine/noncopyable.h"

/** radius of the control points ; must be x.5 to target the center of a pixel */
#define RADIUS 3.5
/** inner width of the colored bar (border excluded) */
#define CBAR_WIDTH 10
/** spacing between the colored bar and the graph's bullet when set at 0,0 */
#define CBAR_MARGIN 1
#define SQUARE 2       /** half length of the square shape of the tangent handles */
#define MIN_DISTANCE 5 /** min distance between control points */
#define GRAPH_SIZE 150 /** size of the curve editor graphic */

/** @brief Flat or Diagonal curve type
    For compatibility and simplicity reason, order shouldn't change, and must be identical to the order specified in the curveType widget
 */
enum CurveType { CT_Flat, CT_Diagonal };

/** @brief Tells the type of element that the points snaps to
 */
enum SnapToType {
    ST_None,      /// The point is not snapped
    ST_Identity,  /// Point snapped to the identity curve
    ST_Neighbors  /// Point snapped to the neighbor points
};

class MyCurveIdleHelper;
class CurveEditor;
class EditDataProvider;

class MyCurve :
    public Gtk::DrawingArea,
    public ColorCaller,
    public CoordinateProvider,
    public rtengine::NonCopyable
{
    friend class MyCurveIdleHelper;

protected:
    /// RGB values from the PipetteDataProvider ; if a channel is set to -1.0f, it is not used
    float pipetteR, pipetteG, pipetteB;
    /// Effective pipette value, i.e. where to create the point; if a point already exist near this value, it'll be used
    float pipetteVal;

    CurveListener* listener;
    ColoredBar* leftBar;
    ColoredBar* bottomBar;
    CursorShape cursor_type;
    /// position and dimensions of the inner graphic area, excluding the graph's border and the surrounding space for the points or for the colored bar
    double graphX, graphY, graphW, graphH;
    double prevGraphW, prevGraphH;  /// previous inner width and height of the editor
    Gdk::ModifierType mod_type;
    int cursorX;  /// X coordinate in the graph of the cursor
    int cursorY;  /// Y coordinate in the graph of the cursor
    LUTf point;
    LUTf upoint;
    LUTf lpoint;
    bool buttonPressed;
    /**
     * snapToElmt, which will be used for the Y axis only,  must be interpreted like this:
     * -100     : no element (default)
     * -3       : maximum value
     * -2       : identity value
     * -1       : minimum value
     * [0;1000[ : control point that it's snapped to
     * >=1000   : moved control point which snaps to the line made by its previous and next point
     */
    int snapToElmt;
    bool snapTo;
    double snapToMinDistX, snapToMinDistY;
    double snapToValX, snapToValY;
    MyCurveIdleHelper* mcih;
    bool curveIsDirty;

    int edited_point;  // > -1 when a point is being numerically edited
    std::vector<double> editedPos;

    virtual std::vector<double> get_vector(int veclen) = 0;
    bool snapCoordinateX(double testedVal, double realVal);
    bool snapCoordinateY(double testedVal, double realVal);
    float getVal(LUTf& curve, int x);
    void calcDimensions();

public:
    MyCurve();
    ~MyCurve() override;

    void setCurveListener(CurveListener* cl) { listener = cl; }
    void setColoredBar(ColoredBar* left, ColoredBar* bottom);
    void notifyListener();
    void updateBackgroundHistogram(LUTu& hist) { return; };
    void refresh();
    void setCurveDirty() { curveIsDirty = true; }
    void on_style_updated() override;
    virtual std::vector<double> getPoints() = 0;
    virtual void setPoints(const std::vector<double>& p) = 0;
    bool on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr) override = 0;
    virtual bool handleEvents(GdkEvent* event) = 0;
    virtual void reset(const std::vector<double>& resetCurve,
                       double identityValue = 0.5) = 0;

    virtual void
    pipetteMouseOver(CurveEditor* ce, EditDataProvider* provider, int modifierKey) = 0;
    virtual bool pipetteButton1Pressed(EditDataProvider* provider, int modifierKey) = 0;
    virtual void pipetteButton1Released(EditDataProvider* provider) = 0;
    virtual void pipetteDrag(EditDataProvider* provider, int modifierKey) = 0;

    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_height_vfunc(int& minimum_height,
                                    int& natural_height) const override;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width,
                                              int& minimum_height,
                                              int& natural_height) const override;
    void get_preferred_width_for_height_vfunc(int height,
                                              int& minimum_width,
                                              int& natural_width) const override;
};

class MyCurveIdleHelper
{
public:
    MyCurve* myCurve;
    bool destroyed;
    int pending;

    void clearPixmap() { myCurve->queue_draw(); }
};
