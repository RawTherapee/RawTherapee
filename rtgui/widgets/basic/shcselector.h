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

#include "widgets/basic/coloredbar.h"

class SHCListener
{
public:
    virtual ~SHCListener() = default;
    virtual void shcChanged() = 0;
};

class SHCSelector final : public Gtk::DrawingArea
{

protected:
    int movingPosition;
    double tmpX, tmpPos;

    double defaults[3];
    double positions[3];
    double wslider;

    // left margin, essentially a workaround to take care of an eventual right colored bar (e.g. for curves)
    int leftMargin;
    // right margin, essentially a workaround to take care of an eventual right colored bar
    int rightMargin;

    const static int hb = 3;  // horizontal border
    const static int vb = 4;  // vertical border

    SHCListener* cl;

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
    void on_realize() override;
    bool on_draw(const ::Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    void updateDrawingArea(const ::Cairo::RefPtr<Cairo::Context>& cr);

public:
    ColoredBar coloredBar;

    SHCSelector();

    void setSHCListener(SHCListener* l)
    {
        cl = l;
        ;
    }

    void setMargins(int left, int right);
    void setDefaults(double spos, double cpos, double hpos);
    void setPositions(double spos, double cpos, double hpos);
    void getPositions(double& spos, double& cpos, double& hpos);
    void styleChanged(const Glib::RefPtr<Gtk::StyleContext>& style);
    bool reset();
    void refresh();
};
