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

#include "guiutils.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/checkbox.h"
#include "widgets/basic/smartscroll.h"

class BayerProcess final :
    public ToolParamBlock,
    public AdjusterListener,
    public CheckBoxListener,
    public FoldableToolPanel,
    public rtengine::FrameCountListener,
    public rtengine::AutoContrastListener
{

protected:

    MyComboBoxText* method;
    Gtk::Box* borderbox;
    Gtk::Box *imageNumberBox;
    Adjuster* border;
    MyComboBoxText* imageNumber;
    Adjuster* ccSteps;
    Gtk::Box *dcbOptions;
    Adjuster* dcbIterations;
    CheckBox* dcbEnhance;
    Gtk::Box *lmmseOptions;
    Adjuster* lmmseIterations;
    Gtk::Frame *pixelShiftFrame;
    Gtk::Box *pixelShiftOptions;
    MyComboBoxText* pixelShiftMotionMethod;
    MyComboBoxText* pixelShiftDemosaicMethod;
    CheckBox* pixelShiftShowMotion;
    CheckBox* pixelShiftShowMotionMaskOnly;
    CheckBox* pixelShiftNonGreenCross;
    CheckBox* pixelShiftGreen;
    CheckBox* pixelShiftBlur;
    CheckBox* pixelShiftHoleFill;
    CheckBox* pixelShiftMedian;
    CheckBox* pixelShiftAverage;
    CheckBox* pixelShiftEqualBright;
    CheckBox* pixelShiftEqualBrightChannel;
    Adjuster* pixelShiftSmooth;
    Adjuster* pixelShiftEperIso;
    Adjuster* pixelShiftSigma;
    Gtk::Box *dualDemosaicOptions;
    Adjuster* dualDemosaicContrast;
    int oldMethod;
    bool lastAutoContrast;
    IdleRegister idle_register;

    rtengine::ProcEvent EvDemosaicBorder;
    rtengine::ProcEvent EvDemosaicAutoContrast;
    rtengine::ProcEvent EvDemosaicContrast;
    rtengine::ProcEvent EvDemosaicPixelshiftDemosaicMethod;
    rtengine::ProcEvent EvPixelshiftAverage;
public:
    static const Glib::ustring TOOL_NAME;

    BayerProcess ();
    ~BayerProcess () override;

    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setAdjusterBehavior(bool falsecoloradd, bool iteradd, bool dualdemozecontrastadd, bool pssigmaadd, bool pssmoothadd, bool pseperisoadd);
    void trimValues(rtengine::procparams::ProcParams* pp) override;
    void setBatchMode(bool batchMode) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;

    void methodChanged();
    void imageNumberChanged();
    void adjusterChanged(Adjuster* a, double newval) override;
    void adjusterAutoToggled (Adjuster* a, bool newval) override;
    void checkBoxToggled(CheckBox* c, CheckValue newval) override;
    void pixelShiftMotionMethodChanged();
    void pixelShiftDemosaicMethodChanged();
    void autoContrastChanged (double autoContrast) override;
    void FrameCountChanged(int n, int frameNum) override;
};
