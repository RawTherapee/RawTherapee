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

#include "colorpreview.h"

#include "rtscalable.h"

ColorPreview::ColorPreview() : color_red(1.0), color_green(1.0), color_blue(1.0) {}

void ColorPreview::setRgb(double r, double g, double b)
{
    color_red = r;
    color_green = g;
    color_blue = b;

    queue_draw();
}

bool ColorPreview::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    cr->set_source_rgb(color_red, color_green, color_blue);
    cr->paint();

    return true;
}

void ColorPreview::get_preferred_height_vfunc(int& minimum_height,
                                              int& natural_height) const
{
    minimum_height = RTScalable::scalePixelSize(8);
    natural_height = RTScalable::scalePixelSize(16);
}

void ColorPreview::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    minimum_width = RTScalable::scalePixelSize(8);
    natural_width = RTScalable::scalePixelSize(16);
}

void ColorPreview::get_preferred_height_for_width_vfunc(int width,
                                                        int& minimum_height,
                                                        int& natural_height) const
{
    get_preferred_height_vfunc(minimum_height, natural_height);
}

void ColorPreview::get_preferred_width_for_height_vfunc(int height,
                                                        int& minimum_width,
                                                        int& natural_width) const
{
    get_preferred_width_vfunc(minimum_width, natural_width);
}
