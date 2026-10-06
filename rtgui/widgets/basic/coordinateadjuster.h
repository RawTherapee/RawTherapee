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

#include <gtkmm.h>

class CurveEditorSubGroup;

class Axis
{
public:
    Glib::ustring label;
    unsigned int decimal;
    double increment;
    double pageIncrement;
    double rangeLowerBound;
    double rangeUpperBound;

    Axis();
    Axis(Glib::ustring label,
         unsigned int decimal,
         double increment,
         double pageIncrement,
         double valMin,
         double valMax);
    void setValues(Glib::ustring label,
                   unsigned int decimal,
                   double increment,
                   double pageIncrement,
                   double valMin,
                   double valMax);
};

class CoordinateAdjuster;
/**
 * @brief Object that will emit NewCoordinates events
 */
class CoordinateProvider
{
protected:
    CoordinateAdjuster* coordinateAdjuster;

public:
    CoordinateProvider() : coordinateAdjuster(nullptr) {}
    virtual ~CoordinateProvider() {}
    void setListener(CoordinateAdjuster* adjuster) { coordinateAdjuster = adjuster; }

    /** @brief Update the position of the edited point ; will trigger events
     *
     * @param pos New position
     * @param chanIdx Channel index as given in the std::vector upon instantiation
     */
    virtual void setPos(double pos, int chanIdx) = 0;
    virtual void stopNumericalAdjustment() = 0;
};

/**
 * @brief Widget that displays spin buttons to adjust coordinates
 *
 * You can set up to 4 axis that will be displayed on a single line, so keep the labels short!
 *
 * The position of the Axis in the vector will be used in the communication between the Adjuster and the Provider to identify the Axis
 */
class CoordinateAdjuster final : public Gtk::FlowBox
{

public:
    //-------------------------------- AxisAdjuster -------------------
    class AxisAdjuster
    {
    private:
        char idx;

    public:
        CoordinateAdjuster* parent;
        Gtk::Label* label;
        Gtk::SpinButton* spinButton;
        sigc::connection spinButtonConn;
        float rangeLowerBound;
        float rangeUpperBound;

        AxisAdjuster(CoordinateAdjuster* parent, const Axis* axis, char index);

        // used to update the AxisAdjuster's parameters
        void updateGUI(const Axis& axis);
        // used to update the displayed value
        void setValue(double newValue);
        //bool keyPressed(GdkEventKey* event);
        void valueChanged();
    };
    //----------------------------------------------------------------

    //-------------------------------- Boundaries  -------------------
    class Boundaries
    {
    public:
        double minVal;
        double maxVal;
    };
    //---------------------------------------------------------------

private:
    typedef enum { CA_STATUS_IDLE, CA_STATUS_EDITING, CA_STATUS_END_EDITING } Status;

    std::vector<AxisAdjuster*> axisAdjusters;
    Status status;
    CurveEditorSubGroup* parent;

    void createWidgets(const std::vector<Axis>& axis);

protected:
    friend class AxisAdjuster;

    CoordinateProvider* coordinateProvider;

    void updatePos(char index, double value);

public:
    /// Basic X/Y adjuster, in the [0-1] range
    CoordinateAdjuster(CoordinateProvider* provider, CurveEditorSubGroup* parent);
    /// For more complex adjuster
    CoordinateAdjuster(CoordinateProvider* provider,
                       CurveEditorSubGroup* parent,
                       const std::vector<Axis>& axis);

    ~CoordinateAdjuster() override;

    // Update the Axis list, e.g. on Curve change, but MUST have the same axis count
    void setAxis(const std::vector<Axis>& axis);

    /** @brief Update the numbers in the spin buttons ; doesn't trigger any event
     *
     * @param pos Vector that gives the values of each channels
     */
    void setPos(std::vector<double>& pos);

    /// Start the adjustment session (enable the widget)
    void startNumericalAdjustment(const std::vector<Boundaries>& newBoundaries);

    /// Edit another point
    void switchAdjustedPoint(std::vector<double>& pos,
                             const std::vector<Boundaries>& newBoundaries);

    /// Trigger the event to show the CoordinateAdjuster
    void showMe(CoordinateProvider* provider);

    /// Stop the adjustment session (disable the widget, i.e. you won't be able to edit the values)
    void stopNumericalAdjustment();
};
