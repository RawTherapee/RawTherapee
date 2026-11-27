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
 *
 *  2024-2024 Daniel Gao <daniel.gao.work@gmail.com>
 */

#pragma once

#include <gtkmm/drawingarea.h>

/**
 * This widget displays a singular color as its contents.
 */
class ColorPreview : public Gtk::DrawingArea
{
public:
    ColorPreview();

    // Values between 0.0 and 1.0 as in
    // Cairo::Context::set_source_rgb()
    void setRgb(double r, double g, double b);

    // Gtk::DrawingArea
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    // Gtk::Widget
    void get_preferred_height_vfunc(int& minimum_height,
                                    int& natural_height) const override;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width,
                                              int& minimum_height,
                                              int& natural_height) const override;
    void get_preferred_width_for_height_vfunc(int height,
                                              int& minimum_width,
                                              int& natural_width) const override;

private:
    double color_red;
    double color_green;
    double color_blue;
};
