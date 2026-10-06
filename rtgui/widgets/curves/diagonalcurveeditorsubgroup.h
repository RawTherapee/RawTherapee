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

#include "curveeditorgroup.h"
#include "widgets/basic/adjuster.h"

#include "rtengine/noncopyable.h"

#include <gtkmm.h>

class DiagonalCurveEditor;
class MyDiagonalCurve;

class DiagonalCurveEditorSubGroup final :
    public CurveEditorSubGroup,
    public SHCListener,
    public AdjusterListener,
    public rtengine::NonCopyable
{

    friend class DiagonalCurveEditor;

protected:
    Gtk::Grid* customCurveGrid;
    Gtk::Grid* NURBSCurveGrid;
    Gtk::Grid* paramCurveGrid;

    MyDiagonalCurve* customCurve;
    MyDiagonalCurve* NURBSCurve;
    MyDiagonalCurve* paramCurve;

    SHCSelector* shcSelector;
    Adjuster* highlights;
    Adjuster* lights;
    Adjuster* darks;
    Adjuster* shadows;

    Adjuster* editedAdjuster;
    int editedAdjusterValue;

    CoordinateAdjuster* customCoordAdjuster;
    CoordinateAdjuster* NURBSCoordAdjuster;

    Gtk::Button* saveCustom;
    Gtk::Button* loadCustom;
    Gtk::Button* copyCustom;
    Gtk::Button* pasteCustom;
    Gtk::ToggleButton* editPointCustom;
    Gtk::ToggleButton* editCustom;
    sigc::connection editCustomConn, editPointCustomConn;
    Gtk::Button* saveNURBS;
    Gtk::Button* loadNURBS;
    Gtk::Button* copyNURBS;
    Gtk::Button* pasteNURBS;
    Gtk::ToggleButton* editPointNURBS;
    Gtk::ToggleButton* editNURBS;
    sigc::connection editNURBSConn, editPointNURBSConn;
    Gtk::Button* saveParam;
    Gtk::Button* loadParam;
    Gtk::Button* copyParam;
    Gtk::Button* pasteParam;
    Gtk::ToggleButton* editParam;
    sigc::connection editParamConn;

    int activeParamControl;

public:
    DiagonalCurveEditorSubGroup(CurveEditorGroup* prt, Glib::ustring& curveDir);
    ~DiagonalCurveEditorSubGroup() override;

    DiagonalCurveEditor* addCurve(Glib::ustring curveLabel = "");
    void updateBackgroundHistogram(CurveEditor* ce) override;
    void switchGUI() override;
    void refresh(CurveEditor* curveToRefresh) override;
    void editModeSwitchedOff() override;
    void pipetteMouseOver(EditDataProvider* provider, int modifierKey) override;
    bool pipetteButton1Pressed(EditDataProvider* provider, int modifierKey) override;
    void pipetteButton1Released(EditDataProvider* provider) override;
    void pipetteDrag(EditDataProvider* provider, int modifierKey) override;
    void showCoordinateAdjuster(CoordinateProvider* provider) override;
    void stopNumericalAdjustment() override;
    void updateLocallabBackground(CurveEditor* ce) override;

    bool curveReset(CurveEditor* ce) override;

protected:
    void storeCurveValues(CurveEditor* ce, const std::vector<double>& p) override;
    void storeDisplayedCurve() override;
    void restoreDisplayedHistogram() override;
    void restoreLocallabBackground() override;
    void savePressed();
    void loadPressed();
    void copyPressed();
    void pastePressed();
    void editPointToggled(Gtk::ToggleButton* button);
    void editToggled(Gtk::ToggleButton* button);
    void removeEditor() override;
    const std::vector<double> getCurveFromGUI(int type) override;
    void shcChanged() override;
    void adjusterChanged(Adjuster* a, double newval) override;
    bool adjusterEntered(GdkEventCrossing* ev, int ac);
    bool adjusterLeft(GdkEventCrossing* ev, int ac);
    void setSubGroupRangeLabels(Glib::ustring r1,
                                Glib::ustring r2,
                                Glib::ustring r3,
                                Glib::ustring r4);
    void setSubGroupBottomBarBgGradient();
};
