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

#include "widgets/basic/backbuffer.h"

#include "rtengine/coord.h"

class CropWindow;

namespace rtengine
{

namespace procparams
{

struct ColorManagementParams;

}

}

class LockablePickerToolListener
{
public:
    virtual ~LockablePickerToolListener () = default;

    /// Callback on Color Picker's visibility switch
    virtual void switchPickerVisibility(bool isVisible) = 0;
};

class LockableColorPicker final : BackBuffer
{
public:
    enum class Size {
        S5=5,
        S10=10,
        S15=15,
        S20=20,
        S25=25,
        S30=30
    };
    enum class Validity {
        INSIDE,
        CROSSING,
        OUTSIDE
    };
private:
    enum class ColorPickerType {
        RGB,
        HSV,
        LAB
    };
    CropWindow* cropWindow;  // the color picker is displayed in a single cropWindow, the one that the user has clicked in
    ColorPickerType displayedValues;
    rtengine::Coord position;  // Coordinate in image space
    rtengine::Coord anchorOffset;
    Size size;
    rtengine::procparams::ColorManagementParams *color_management_params;
    Validity validity;
    float r, g, b;  // red green blue in [0;1] range
    float rpreview, gpreview, bpreview;
    float hue, sat, val;  // hue saturation value in [0;1] range
    float L, a, bb;  // L*a*b value in [0;1] range

    void updateBackBuffer ();

public:

    LockableColorPicker (CropWindow* cropWindow, rtengine::procparams::ColorManagementParams *color_management_params);

    void draw (const Cairo::RefPtr<Cairo::Context> &cr);

    // Used to update the RGB color, the HSV values will be updated accordingly
    void setPosition (const rtengine::Coord &newPos);
    void setRGB (const float R, const float G, const float B, const float previewR, const float previewG, const float previewB);
    void getImagePosition (rtengine::Coord &imgPos) const;
    void getScreenPosition (rtengine::Coord &screenPos) const;
    Size getSize () const;
    bool isOver (int x, int y);
    void setValidity (Validity isValid);
    void setSize (Size newSize);
    void rollDisplayedValues ();
    bool incSize ();
    bool decSize ();
    bool cycleRGB ();
    bool cycleHSV ();
};
