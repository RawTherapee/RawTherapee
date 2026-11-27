/** -*- C++ -*-
 *
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2017 Alberto Griggio <alberto.griggio@gmail.com>
 *  adapted for Rawtherapee J.Desmis december 2024
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

// adapted from the "color correction" module of Darktable. Original copyright follows
/*
    copyright (c) 2009--2010 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <gtkmm.h>

#include "rtengine/improcfun.h"
#include "rtengine/color.h"

#include "eventmapper.h"
#include "toolpanel.h"

class LabGridArea final : public Gtk::DrawingArea {
public:

    struct FunctionParams {
        using Function = std::function<double(double)>;
        using ResolutionFunction = std::function<int(int)>;

        /** x-value of the left side. */
        double x_min;
        /** x-value of the right side. */
        double x_max;
        /** y-value of the left side. */
        double y_min;
        /** y-value of the right side. */
        double y_max;
        /**
         * The function itself, which takes an x-value and returns the y-value.
         */
        Function function;
        /**
         * A function returning the resolution of the plot.
         *
         * It takes the width of the plot, in pixels, and returns the number of
         * line segments that should be used to plot the function.
         */
        ResolutionFunction resolution_function{[](int width) { return width; }};

        FunctionParams() = default;
        FunctionParams(double x_min, double x_max, double y_min, double y_max,
            const Function &function,
            const ResolutionFunction & resolution_function) :
            x_min(x_min),
            x_max(x_max),
            y_min(y_min),
            y_max(y_max),
            function(function),
            resolution_function(resolution_function)
        {
        }

        bool is_valid() const;
    };

private:
    rtengine::ProcEvent evt;
    Glib::ustring evtMsg;

    enum State { NONE, HIGH, LOW, GRE};
    State litPoint;
    double low_a;
    double high_a;
    double low_b;
    double high_b;
    double gre_x;
    double gre_y;
    double whi_x;
    double whi_y;
    double me_x;
    double me_y;
    FunctionParams function_params;
    
    double defaultLow_a;
    double defaultHigh_a;
    double defaultLow_b;
    double defaultHigh_b;
    double defaultgre_x;
    double defaultgre_y;
    double defaultwhi_x;
    double defaultwhi_y;
    double defaultme_x;
    double defaultme_y;

    ToolPanelListener *listener;
    bool edited;
    bool isDragged;
    sigc::connection delayconn;
    static const int inset = 5;

    bool low_enabled;
    bool ciexy_enabled;
    bool ghs_enabled;
    bool mous_enabled;

    bool notifyListener();
    void getLitPoint();

public:
    LabGridArea(rtengine::ProcEvent evt, const Glib::ustring &msg, bool enable_low=true, bool ciexy=false, bool ghs=false, bool mous=false);

    void getParams(double &la, double &lb, double &ha, double &hb, double &gx, double &gy, double &wx, double &wy, double &mx, double &my) const;
    void setParams(double la, double lb, double ha, double hb, double gx, double gy, double wx, double wy, double mx, double my, bool notify);
    void setFunctionParams(const FunctionParams &params);
    void setDefault (double la, double lb, double ha, double hb, double gx, double gy, double wx, double wy, double mx, double my);
    void setEdited(bool yes);
    bool getEdited() const;
    void reset(bool toInitial);
    void setListener(ToolPanelListener *l);

    bool lowEnabled() const;
    void setLowEnabled(bool yes);
    bool ciexyEnabled() const;
    void setciexyEnabled(bool yes);
    bool ghsEnabled() const;
    void setghsEnabled(bool yes);
    bool mousEnabled() const;
    void setmousEnabled(bool yes);
 
    bool on_draw(const ::Cairo::RefPtr<Cairo::Context> &cr) override;
    void on_style_updated () override;
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_button_release_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *event) override;
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_width_vfunc(int &minimum_width, int &natural_width) const override;
    void get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const override;
    
};


class LabGrid: public Gtk::Box {
private:
    LabGridArea grid;

    bool resetPressed(GdkEventButton *event);

public:
    LabGrid(rtengine::ProcEvent evt, const Glib::ustring &msg, bool enable_low=true, bool ciexy=false, bool ghs=false, bool mous=true);

    void getParams(double &la, double &lb, double &ha, double &hb, double &gx, double &gy, double &wx, double &wy, double &mx, double &my)
        const { return grid.getParams(la, lb, ha, hb, gx, gy, wx, wy, mx, my); }
    void setParams(double la, double lb, double ha, double hb, double gx, double gy, double wx, double wy, double mx, double my, bool notify)
        { grid.setParams(la, lb, ha, hb, gx, gy, wx, wy, mx, my, notify); }
    void setFunctionParams(const LabGridArea::FunctionParams &params)
    {
        grid.setFunctionParams(params);
    }
    void setDefault (double la, double lb, double ha, double hb, double gx, double gy, double wx, double wy, double mx, double my)
        { grid.setDefault(la, lb, ha, hb, gx, gy, wx, wy, mx, my); }
    void setEdited(bool yes) { grid.setEdited(yes); }
    bool getEdited() const { return grid.getEdited(); }
    void reset(bool toInitial) { grid.reset(toInitial); }
    void setListener(ToolPanelListener *l) { grid.setListener(l); }
    bool lowEnabled() const { return grid.lowEnabled(); }
    void setLowEnabled(bool yes) { grid.setLowEnabled(yes); }
    bool ciexyEnabled() const { return grid.ciexyEnabled(); }
    void setciexyEnabled(bool yes) { grid.setciexyEnabled(yes); }
    bool ghsEnabled() const { return grid.ghsEnabled(); }
    void setghsEnabled(bool yes) { grid.setghsEnabled(yes); }
    bool mousEnabled() const { return grid.mousEnabled(); }
    void setmousEnabled(bool yes) { grid.setmousEnabled(yes); }

};
