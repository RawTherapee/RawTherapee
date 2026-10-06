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

#include "coordinateadjuster.h"

#include "multilangmgr.h"
#include "widgets/curves/curveeditorgroup.h"

#include <cassert>

Axis::Axis() :
    decimal(5),
    increment(0.001),
    pageIncrement(0.01),
    rangeLowerBound(0.),
    rangeUpperBound(1.)
{
}

Axis::Axis(Glib::ustring label,
           unsigned int decimal,
           double increment,
           double pageIncrement,
           double valMin = 0.0,
           double valMax = 1.0) :
    label(label),
    decimal(decimal),
    increment(increment),
    pageIncrement(pageIncrement),
    rangeLowerBound(valMin),
    rangeUpperBound(valMax)
{
}

void Axis::setValues(Glib::ustring label,
                     unsigned int decimal,
                     double increment,
                     double pageIncrement,
                     double valMin,
                     double valMax)
{
    this->label = label;
    this->decimal = decimal;
    this->increment = increment;
    this->pageIncrement = pageIncrement;
    this->rangeLowerBound = valMin;
    this->rangeUpperBound = valMax;
}

CoordinateAdjuster::AxisAdjuster::AxisAdjuster(CoordinateAdjuster* parent,
                                               const Axis* axis,
                                               char index) :
    idx(index), parent(parent), rangeLowerBound(0.f), rangeUpperBound(0.f)
{

    label = Gtk::manage(new Gtk::Label(axis->label));
    spinButton = Gtk::manage(new Gtk::SpinButton());
    spinButton->set_name("AxisAdjuster");
    spinButton->set_digits(axis->decimal);
    spinButton->set_increments(axis->increment, axis->pageIncrement);
    spinButton->set_range(axis->rangeLowerBound, axis->rangeUpperBound);
    spinButton->set_sensitive(false);
    spinButtonConn = spinButton->signal_value_changed().connect(
        sigc::mem_fun(*this, &CoordinateAdjuster::AxisAdjuster::valueChanged));
    //spinButton->signal_key_press_event().connect( sigc::mem_fun(*this, &CoordinateAdjuster::AxisAdjuster::keyPressed) );
}

void CoordinateAdjuster::AxisAdjuster::updateGUI(const Axis& axis)
{
    label->set_text(axis.label);
    spinButton->set_digits(axis.decimal);
    spinButton->set_increments(axis.increment, axis.pageIncrement);
    spinButton->set_range(axis.rangeLowerBound, axis.rangeUpperBound);
    spinButton->set_sensitive(false);
    rangeLowerBound = axis.rangeLowerBound;
    rangeUpperBound = axis.rangeUpperBound;
}

void CoordinateAdjuster::AxisAdjuster::setValue(double newValue)
{
    float range = rangeUpperBound - rangeLowerBound;
    spinButtonConn.block(true);
    spinButton->set_value(static_cast<float>(newValue) * range + rangeLowerBound);
    spinButtonConn.block(false);
}

void CoordinateAdjuster::AxisAdjuster::valueChanged()
{
    float range = rangeUpperBound - rangeLowerBound;
    parent->updatePos(idx, (static_cast<float>(spinButton->get_value()) - rangeLowerBound)
                               / range);
}

CoordinateAdjuster::CoordinateAdjuster(CoordinateProvider* provider,
                                       CurveEditorSubGroup* parent,
                                       const std::vector<Axis>& axis) :
    status(CA_STATUS_IDLE), parent(parent), coordinateProvider(provider)
{
    provider->setListener(this);
    createWidgets(axis);
}

CoordinateAdjuster::CoordinateAdjuster(CoordinateProvider* provider,
                                       CurveEditorSubGroup* parent) :
    status(CA_STATUS_IDLE), parent(parent), coordinateProvider(provider)
{
    std::vector<Axis> defaultAxis;
    Axis X(M("CURVEEDITOR_AXIS_IN"), 3, 0.1, 1., 0., 100.);
    Axis Y(M("CURVEEDITOR_AXIS_OUT"), 3, 0.1, 1., 0., 100.);
    defaultAxis.push_back(X);
    defaultAxis.push_back(Y);

    provider->setListener(this);
    createWidgets(defaultAxis);
}

CoordinateAdjuster::~CoordinateAdjuster()
{
    for (std::vector<AxisAdjuster*>::iterator iterator = axisAdjusters.begin();
         iterator != axisAdjusters.end(); ++iterator)
        delete *iterator;
}

void CoordinateAdjuster::createWidgets(const std::vector<Axis>& axis)
{
    unsigned int count = axis.size();

    if (!count) {
        printf("CoordinateAdjuster - Error: the Axis list is empty!\n");
        return;
    }

    assert(count <= 4);

    axisAdjusters.resize(axis.size());

    for (unsigned int i = 0; i < count; ++i) {
        const Axis* currAxis = &(axis.at(i));
        axisAdjusters.at(i) = new AxisAdjuster(this, currAxis, i);
        AxisAdjuster* currAdjuster = axisAdjusters.at(i);
        currAdjuster->rangeLowerBound = currAxis->rangeLowerBound;
        currAdjuster->rangeUpperBound = currAxis->rangeUpperBound;

        Gtk::Grid* box = Gtk::manage(new Gtk::Grid());
        box->set_orientation(Gtk::ORIENTATION_HORIZONTAL);

        setExpandAlignProperties(currAdjuster->label, false, false, Gtk::ALIGN_START,
                                 Gtk::ALIGN_CENTER);
        setExpandAlignProperties(currAdjuster->spinButton, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);

        box->attach_next_to(*(currAdjuster->spinButton), Gtk::POS_LEFT, 1, 1);
        box->attach_next_to(*(currAdjuster->label), Gtk::POS_LEFT, 1, 1);

        Gtk::FlowBoxChild* fbChild = Gtk::manage(new Gtk::FlowBoxChild());
        fbChild->set_can_focus(false);
        fbChild->add(*box);

        add(*fbChild);
    }
}

void CoordinateAdjuster::updatePos(char index, double value)
{
    coordinateProvider->setPos(value, index);
}

void CoordinateAdjuster::setAxis(const std::vector<Axis>& axis)
{
    assert(axis.size() == axisAdjusters.size());

    for (size_t i = 0; i < axisAdjusters.size(); ++i) {
        axisAdjusters.at(i)->updateGUI(axis.at(i));
    }
}

void CoordinateAdjuster::setPos(std::vector<double>& pos)
{
    if (is_visible()) {
        for (size_t i = 0; i < pos.size(); ++i) {
            axisAdjusters.at(i)->setValue(pos.at(i));
        }
    }
}

void CoordinateAdjuster::startNumericalAdjustment(
    const std::vector<Boundaries>& newBoundaries)
{
    for (size_t i = 0; i < axisAdjusters.size(); ++i) {
        Gtk::SpinButton* currSpinButton = axisAdjusters.at(i)->spinButton;
        currSpinButton->set_sensitive(true);
        float range =
            axisAdjusters.at(i)->rangeUpperBound - axisAdjusters.at(i)->rangeLowerBound;
        currSpinButton->set_range(
            newBoundaries.at(i).minVal * static_cast<double>(range)
                + static_cast<double>(axisAdjusters.at(i)->rangeLowerBound),
            newBoundaries.at(i).maxVal * static_cast<double>(range)
                + static_cast<double>(axisAdjusters.at(i)->rangeLowerBound));
    }

    axisAdjusters.at(0)->spinButton->grab_focus();
    status = CA_STATUS_EDITING;
}

void CoordinateAdjuster::switchAdjustedPoint(std::vector<double>& pos,
                                             const std::vector<Boundaries>& newBoundaries)
{
    if (status != CA_STATUS_EDITING) {
        return;
    }

    for (size_t i = 0; i < axisAdjusters.size(); ++i) {
        AxisAdjuster* currAxis = axisAdjusters.at(i);

        // disable events
        currAxis->spinButtonConn.block(true);

        // To avoid trimmed values, we have to...

        // ...enlarge range to the maximum
        currAxis->spinButton->set_range(axisAdjusters.at(i)->rangeLowerBound,
                                        axisAdjusters.at(i)->rangeUpperBound);

        // ...set the new value
        currAxis->setValue(pos.at(i));

        // ...narrow the range to the new interval
        float range =
            axisAdjusters.at(i)->rangeUpperBound - axisAdjusters.at(i)->rangeLowerBound;
        currAxis->spinButton->set_range(
            newBoundaries.at(i).minVal * static_cast<double>(range)
                + static_cast<double>(axisAdjusters.at(i)->rangeLowerBound),
            newBoundaries.at(i).maxVal * static_cast<double>(range)
                + static_cast<double>(axisAdjusters.at(i)->rangeLowerBound));

        // enable events
        currAxis->spinButtonConn.block(false);
    }

    axisAdjusters.at(0)->spinButton->grab_focus();
    status = CA_STATUS_EDITING;
}

void CoordinateAdjuster::showMe(CoordinateProvider* provider)
{
    parent->showCoordinateAdjuster(provider);
}

void CoordinateAdjuster::stopNumericalAdjustment()
{
    for (size_t i = 0; i < axisAdjusters.size(); ++i) {
        axisAdjusters.at(i)->spinButtonConn.block(true);
        axisAdjusters.at(i)->spinButton->set_sensitive(false);
        axisAdjusters.at(i)->spinButton->set_range(axisAdjusters.at(i)->rangeLowerBound,
                                                   axisAdjusters.at(i)->rangeUpperBound);
        axisAdjusters.at(i)->spinButtonConn.block(false);
    }

    status = CA_STATUS_IDLE;
}
