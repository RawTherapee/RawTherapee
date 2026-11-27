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

#include "rtgui/colorprovider.h"
#include "rtgui/guiutils.h"

/*
 * Parent class for all colored bar type; a ColorProvider has to be set
 * thanks to "setColorProvider" to be able to display colors inside the bar
 *
 * WARNING: If the color has no gradient defined or can't get colors from the provider,
 *          the bar will have undefined data, and the calling class will have to draw
 *          the bar itself, i.e. use render_background (depending on its Gtk::StyleContext)
 *
 */
class ColoredBar final : public ColorCaller
{

private:
    // ColoredBar position and size parameters
    int x;
    int y;
    int w;
    int h;

protected:
    eRTOrientation orientation;
    std::vector<GradientMilestone> bgGradient;

public:
    explicit ColoredBar(eRTOrientation orient);
    // Note: updateColoredBar shall be called after to update the bar
    void
    setColoredBarSize(const int newX, const int newY, const int newW, const int newH);

    void updateColoredBar(const Cairo::RefPtr<Cairo::Context>& cr);

    bool canGetColors();

    // Method for convenience; if no Gradient provided, the ColoredBar will ask colors on a per pixel basis
    void setBgGradient(const std::vector<GradientMilestone>& milestones);
    // by clearing the gradient, the ColorProvider will have to provide colors on a per pixel basis if a ColorProvider
    // has been set, through ColorProvider::colorForValue on next ColoredBar::expose
    void clearBgGradient();
};
