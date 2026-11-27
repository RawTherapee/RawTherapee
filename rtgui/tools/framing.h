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

#include "guiutils.h"
#include "toolpanel.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/smartscroll.h"

#include "rtengine/procevents.h"

#include <memory>

class ColorPreview;

class Framing final :
    public ToolParamBlock,
    public AdjusterListener,
    public FoldableToolPanel
{
public:
    static const Glib::ustring TOOL_NAME;

    Framing();
    ~Framing();

    // FoldableToolPanel
    void read(const rtengine::procparams::ProcParams* pp,
              const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp,
               ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams,
                     const ParamsEdited* pedited = nullptr) override;
    void trimValues(rtengine::procparams::ProcParams* pp) override;
    void setBatchMode(bool batchMode) override;
    void enabledChanged() override;

    void update(int originalWidth = 0, int originalHeight = 0);
    void setAdjusterBehavior(bool addRelativeBorderSize, bool addRed, bool addGreen, bool addBlue);

    // AdjusterListener
    void adjusterChanged(Adjuster* adj, double newVal) override;

    // Signal connections
    void onFramingMethodChanged();
    void onAspectRatioChanged();
    void onOrientationChanged();
    void onWidthChanged();
    void onHeightChanged();
    void onAllowUpscalingToggled();
    void onBorderSizeMethodChanged();
    void onBasisChanged();
    void onMinSizeToggled();
    void onMinWidthChanged();
    void onMinHeightChanged();
    void onAbsWidthChanged();
    void onAbsHeightChanged();

private:
    class AspectRatios;

    // Helper struct for repeated patterns
    struct DimensionGui
    {
        using CallbackFunc = void(Framing::*)();

        DimensionGui() = default;
        DimensionGui(Gtk::Box* parent, const char* text);

        void setValue(int newValue) { value->set_value(newValue); }
        void setRange(int min, int max) { value->set_range(min, max); }
        void connect(Framing& framing, CallbackFunc callback);

        void show() { box->show(); }
        void hide() { box->hide(); }

        Gtk::Box* box;
        MySpinButton* value;
        sigc::connection connection;
        bool isDirty;
    };

    void setupEvents();
    void setupFramingMethodGui();
    void setupBorderSizeGui();
    void setupBorderColorsGui();

    void readParams(const rtengine::procparams::ProcParams* pp);
    void readEdited(const ParamsEdited* pedited);
    void writeParams(rtengine::procparams::ProcParams* pp);
    void writeEdited(ParamsEdited* pedited);

    void setDimensions();
    void updateFramingMethodGui();
    void updateBorderSizeGui();
    void updateBorderColorGui();

    // Framing method
    MyComboBoxText* framingMethod;
    sigc::connection framingMethodChanged;
    Gtk::Label* aspectRatioLabel;
    MyComboBoxText* aspectRatio;
    sigc::connection aspectRatioChanged;
    Gtk::Label* orientationLabel;
    MyComboBoxText* orientation;
    sigc::connection orientationChanged;
    DimensionGui width;
    DimensionGui height;
    Gtk::CheckButton* allowUpscaling;
    sigc::connection allowUpscalingConnection;

    // Border sizing
    MyComboBoxText* borderSizeMethod;
    sigc::connection borderSizeMethodChanged;
    Gtk::Label* basisLabel;
    MyComboBoxText* basis;
    sigc::connection basisChanged;
    Adjuster* relativeBorderSize;
    Gtk::Frame* minSizeFrame;
    Gtk::Box* minSizeFrameContent;
    Gtk::CheckButton* minSizeEnabled;
    sigc::connection minSizeEnabledConnection;
    DimensionGui minWidth;
    DimensionGui minHeight;
    DimensionGui absWidth;
    DimensionGui absHeight;

    // Border colors
    Adjuster* redAdj;
    Adjuster* greenAdj;
    Adjuster* blueAdj;
    ColorPreview* colorPreview;

    // Events
    rtengine::ProcEvent EvFramingEnabled;
    rtengine::ProcEvent EvFramingMethod;
    rtengine::ProcEvent EvFramingAspectRatio;
    rtengine::ProcEvent EvFramingOrientation;
    rtengine::ProcEvent EvFramingFramedWidth;
    rtengine::ProcEvent EvFramingFramedHeight;
    rtengine::ProcEvent EvFramingAllowUpscaling;
    rtengine::ProcEvent EvFramingBorderSizingMethod;
    rtengine::ProcEvent EvFramingBasis;
    rtengine::ProcEvent EvFramingRelativeBorderSize;
    rtengine::ProcEvent EvFramingMinSizeEnabled;
    rtengine::ProcEvent EvFramingMinWidth;
    rtengine::ProcEvent EvFramingMinHeight;
    rtengine::ProcEvent EvFramingAbsWidth;
    rtengine::ProcEvent EvFramingAbsHeight;
    rtengine::ProcEvent EvFramingBorderRed;
    rtengine::ProcEvent EvFramingBorderGreen;
    rtengine::ProcEvent EvFramingBorderBlue;

    IdleRegister idleRegister;
    std::unique_ptr<AspectRatios> aspectRatioData;

    int imgWidth;
    int imgHeight;
    bool lastAllowUpscaling;
    bool lastMinSizeEnabled;
};
