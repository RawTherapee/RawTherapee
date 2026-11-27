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

#include <iomanip>

#include "coloredbar.h"

#include "rtengine/procparams.h"

class ThresholdSelector;

/*
 * This class let the instanciator to provide the background curve
 */
class ThresholdCurveProvider
{
public:
    virtual ~ThresholdCurveProvider() {};
    /*
     * The curve provider has to send back a list of point (at least 2 points) in the [0.0 ; 1.0] range
     * for both X and Y axis; X and Y values are streamlined ( X1, Y1, X2, Y2, X3, Y3, ...)
     */
    virtual std::vector<double> getCurvePoints(ThresholdSelector* tAdjuster) const = 0;
};

/*
 * This widget will let you select a linearly variable threshold, creating a ramp up
 * if you want to go from a null effect to a full effect
 *    0-0-ramp up-1-1
 * or a ramp down if you want the contrary
 *    1-1-ramp down-0-0
 *
 * You can optionally create a double threshold
 *    0-0-ramp up-1-1-ramp down-0-0
 * or
 *    1-1-ramp down-0-0-ramp up-1-1
 *
 * Please note that the values are related to the cursors, depending on their position
 * on the graph. E.g. the "bottomLeft" value is related to the bottom left cursor.
 *
 * It is also possible to have a threshold with 2 totally independent cursors, each one having his own range,
 * min/max/default values and precision. This let developers create their own threshold curve, that they will
 * have to provide through the ThresholdCurveProvider interface
 *
 */
class ThresholdSelector : public Gtk::DrawingArea
{

public:
    enum ThreshCursorId {
        TS_UNDEFINED = -1,
        TS_BOTTOMLEFT,
        TS_TOPLEFT,
        TS_BOTTOMRIGHT,
        TS_TOPRIGHT
    };

protected:
    sigc::signal<void> sig_val_changed;

    ThresholdCurveProvider* bgCurveProvider;

    Glib::ustring additionalTTip;
    // Label for the bottom cursor, displayed if separatedSliders==true only
    Glib::ustring separatedLabelBottom;
    // Label for the top cursor, displayed if separatedSliders==true only
    Glib::ustring separatedLabelTop;
    // If true, the Top and Bottom sliders are totally separate and can be drag through the full range; for simple threshold only!
    bool separatedSliders;
    // If true: there curve is a double threshold (0 to 1 to 0, or 1 to 0 to 1).
    bool doubleThresh;
    // If true: the curve start at 1 (top); if false: the curve start at 0 (bottom)
    bool initalEq1;
    // Decimal number if this object has to handle "double" values, for the Top slider
    unsigned int precisionTop;
    // Decimal number if this object has to handle "double" values, for the Bottom slider
    unsigned int precisionBottom;
    ThreshCursorId litCursor;
    ThreshCursorId oldLitCursor;
    double boundary1[2], boundary2[2];
    double tmpX, tmpPos;

    ThreshCursorId movedCursor, secondaryMovedCursor;
    double minValTop, maxValTop;
    double minValBottom, maxValBottom;
    double defPos[4];
    double positions[4];
    eUpdatePolicy updatePolicy;

    constexpr static double sliderWidth = 11.;  // constant must be an odd value
    constexpr static double verticalSliderPaddingFactor = 1.5 / 7.;

    void initValues();
    void findLitCursor(int posX, int posY);
    void findSecondaryMovedCursor(guint state);
    void findBoundaries(double& min, double& max);
    double to01(ThreshCursorId cursorId);

    // Internal drawing functions
    void updateTooltip();
    void updateDrawingArea(const ::Cairo::RefPtr<Cairo::Context>& cr);

    // GtkDrawingArea override functions
    void on_realize() override;
    bool on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr) override;

    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const final;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const final;
    void get_preferred_height_for_width_vfunc(int width,
                                              int& minimum_height,
                                              int& natural_height) const final;
    void get_preferred_width_for_height_vfunc(int height,
                                              int& minimum_width,
                                              int& natural_width) const final;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_leave_notify_event(GdkEventCrossing* event) override;

public:
    ColoredBar coloredBar;
    sigc::signal<void> signal_value_changed();

    ThresholdSelector(double minValueBottom,
                      double maxValueBottom,
                      double defBottom,
                      Glib::ustring labelBottom,
                      unsigned int precisionBottom,
                      double minValueTop,
                      double maxValueTop,
                      double defTop,
                      Glib::ustring labelTop,
                      unsigned int precisionTop,
                      ThresholdCurveProvider* curveProvider);
    ThresholdSelector(double minValue,
                      double maxValue,
                      double defBottom,
                      double defTop,
                      unsigned int precision,
                      bool startAtOne);
    ThresholdSelector(double minValue,
                      double maxValue,
                      double defBottomLeft,
                      double defTopLeft,
                      double defBottomRight,
                      double defTopRight,
                      unsigned int precision,
                      bool startAtOne);

    double shapePositionValue(ThreshCursorId cursorId);
    template <typename T> void setDefaults(const rtengine::procparams::Threshold<T>& t)
    {
        defPos[TS_BOTTOMLEFT] =
            double(t.getBottomLeft());  // should we use shapeValue() ?
        defPos[TS_TOPLEFT] = double(t.getTopLeft());

        if (doubleThresh) {
            defPos[TS_BOTTOMRIGHT] = double(t.getBottomRight());
            defPos[TS_TOPRIGHT] = double(t.getTopRight());
        }
    }
    void setDefaults(double bottom, double top);
    void
    setDefaults(double bottomLeft, double topLeft, double bottomRight, double topRight);
    template <typename T>
    void setPositions(const rtengine::procparams::Threshold<T>& tValues)
    {
        positions[TS_BOTTOMLEFT] = static_cast<double>(tValues.getBottomLeft());
        positions[TS_TOPLEFT] = static_cast<double>(tValues.getTopLeft());

        if (tValues.isDouble()) {
            positions[TS_BOTTOMRIGHT] = static_cast<double>(tValues.getBottomRight());
            positions[TS_TOPRIGHT] = static_cast<double>(tValues.getTopRight());
        }

        updateTooltip();
        queue_draw();
    }
    void setPositions(double bottom, double top);
    void
    setPositions(double bottomLeft, double topLeft, double bottomRight, double topRight);

    template <typename T> rtengine::procparams::Threshold<T> getPositions()
    {
        if (doubleThresh) {
            rtengine::procparams::Threshold<T> rThresh(
                static_cast<T>(shapePositionValue(TS_BOTTOMLEFT)),
                static_cast<T>(shapePositionValue(TS_TOPLEFT)),
                static_cast<T>(shapePositionValue(TS_BOTTOMRIGHT)),
                static_cast<T>(shapePositionValue(TS_TOPRIGHT)), initalEq1);
            return rThresh;
        } else {
            rtengine::procparams::Threshold<T> rThresh(
                static_cast<T>(shapePositionValue(TS_BOTTOMLEFT)),
                static_cast<T>(shapePositionValue(TS_TOPLEFT)), initalEq1);
            return rThresh;
        }
    }

    template <typename T> void getPositions(T& bottom, T& top)
    {
        bottom = static_cast<T>(shapePositionValue(TS_BOTTOMLEFT));
        top = static_cast<T>(shapePositionValue(TS_TOPLEFT));
    }

    template <typename T>
    void getPositions(T& bottomLeft, T& topLeft, T& bottomRight, T& topRight)
    {
        bottomLeft = static_cast<T>(shapePositionValue(TS_BOTTOMLEFT));
        topLeft = static_cast<T>(shapePositionValue(TS_TOPLEFT));
        bottomRight = static_cast<T>(shapePositionValue(TS_BOTTOMRIGHT));
        topRight = static_cast<T>(shapePositionValue(TS_TOPRIGHT));
    }

    void setSeparatedSliders(bool separated);
    bool getSeparatedSliders();
    void setBgCurveProvider(ThresholdCurveProvider* provider);
    bool isStartAtOne() { return initalEq1; }
    bool isDouble() { return doubleThresh; }
    void styleChanged(const Glib::RefPtr<Gtk::StyleContext>& style);
    unsigned int getPrecision() { return precisionTop; }
    void reset();
    void setUpdatePolicy(eUpdatePolicy policy) { updatePolicy = policy; }
    void set_tooltip_markup(const Glib::ustring& markup);
    // this set_tooltip_text method is to set_tooltip_markup, and text can contain markups
    void set_tooltip_text(const Glib::ustring& text);
};

template <>
inline void ThresholdSelector::getPositions<Glib::ustring>(Glib::ustring& bottom,
                                                           Glib::ustring& top)
{
    bottom = Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                   shapePositionValue(TS_BOTTOMLEFT));
    top = Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                shapePositionValue(TS_TOPLEFT));
}

template <>
inline void ThresholdSelector::getPositions<Glib::ustring>(Glib::ustring& bottomLeft,
                                                           Glib::ustring& topLeft,
                                                           Glib::ustring& bottomRight,
                                                           Glib::ustring& topRight)
{

    bottomLeft = Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                       shapePositionValue(TS_BOTTOMLEFT));
    topLeft = Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                    shapePositionValue(TS_TOPLEFT));
    bottomRight = Glib::ustring::format(std::fixed, std::setprecision(precisionBottom),
                                        shapePositionValue(TS_BOTTOMRIGHT));
    topRight = Glib::ustring::format(std::fixed, std::setprecision(precisionTop),
                                     shapePositionValue(TS_TOPRIGHT));
}
