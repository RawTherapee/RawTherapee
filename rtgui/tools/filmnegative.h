/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Alberto Romei <aldrop8@gmail.com>
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

#include <array>

#include <gtkmm.h>

#include "editcallbacks.h"
#include "guiutils.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/spotpicker.h"

#include "rtengine/colortemp.h"

namespace
{
using RGB = rtengine::procparams::FilmNegativeParams::RGB;
using ColorSpace = rtengine::procparams::FilmNegativeParams::ColorSpace;
using BackCompat = rtengine::procparams::FilmNegativeParams::BackCompat;
}

class FilmNegProvider
{
public:
    virtual ~FilmNegProvider() = default;

    virtual bool getFilmNegativeSpot(rtengine::Coord spot, int spotSize, RGB &refInput, RGB &refOutput) = 0;
};

class FilmNegative final :
    public ToolParamBlock,
    public AdjusterListener,
    public FoldableToolPanel,
    public EditSubscriber,
    public rtengine::FilmNegListener
{
public:
    static const Glib::ustring TOOL_NAME;

    FilmNegative();
    ~FilmNegative() override;

    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void setBatchMode(bool batchMode) override;

    void adjusterChanged(Adjuster* a, double newval) override;
    void enabledChanged() override;
    void colorSpaceChanged();

    void filmRefValuesChanged(const RGB &refInput, const RGB &refOutput) override;

    void setFilmNegProvider(FilmNegProvider* provider);

    void setEditProvider(EditDataProvider* provider) override;

    // EditSubscriber interface
    CursorShape getCursor(int objectID, int xPos, int yPos) const override;
    bool mouseOver(int modifierKey) override;
    bool button1Pressed(int modifierKey) override;
    bool button1Released() override;
    bool button3Pressed(int modifierKey) override;
    void switchOffEditMode() override;

private:
    void editToggled();
    void refSpotToggled();

    void readOutputSliders(RGB &refOutput);
    void writeOutputSliders(const RGB &refOutput);

    // ColorTemp value corresponding to neutral RGB multipliers (1,1,1). Should be around 6500K.
    const rtengine::ColorTemp NEUTRAL_TEMP;

    const rtengine::ProcEvent evFilmNegativeExponents;
    const rtengine::ProcEvent evFilmNegativeEnabled;
    const rtengine::ProcEvent evFilmNegativeRefSpot;
    const rtengine::ProcEvent evFilmNegativeBalance;
    const rtengine::ProcEvent evFilmNegativeColorSpace;

    std::vector<rtengine::Coord> refSpotCoords;

    RGB refInputValues;
    bool paramsUpgraded;

    /**
     * Luminance reference: these input RGB values must produce this output luminance.
     * This allows keeping constant luminance when picking several white balance  spot
     * samples in sequence; values are set on the initial white balance spot sampling,
     * and reset every time params are read, or the output sliders are changed.
     */
    struct {
        RGB input;
        float lum;
    } refLuminance;

    FilmNegProvider* fnp;

    MyComboBoxText* const colorSpace;

    Adjuster* const greenExp;
    Adjuster* const redRatio;
    Adjuster* const blueRatio;

    static constexpr int DEFAULT_SPOT_WIDTH = 8;

    SpotPicker picker;

    Gtk::Label* const refInputLabel;
    SpotPicker refPicker;

    SpotPicker* activePicker;

    Adjuster* const outputLevel;
    Adjuster* const greenBalance;
    Adjuster* const blueBalance;

    IdleRegister idle_register;

};
