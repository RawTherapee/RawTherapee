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

#include "curveeditorgroup.h"

#include "rtengine/noncopyable.h"

class FlatCurveEditor;
class MyFlatCurve;

class FlatCurveEditorSubGroup final :
    public CurveEditorSubGroup,
    public rtengine::NonCopyable
{

    friend class FlatCurveEditor;

protected:
    Gtk::Grid* CPointsCurveGrid;

    MyFlatCurve* CPointsCurve;

    CoordinateAdjuster* CPointsCoordAdjuster;

    Gtk::Button* saveCPoints;
    Gtk::Button* loadCPoints;
    Gtk::Button* copyCPoints;
    Gtk::Button* pasteCPoints;
    Gtk::ToggleButton* editPointCPoints;
    Gtk::ToggleButton* editCPoints;
    sigc::connection editCPointsConn, editPointCPointsConn;

public:
    FlatCurveEditorSubGroup(CurveEditorGroup* prt, Glib::ustring& curveDir);
    ~FlatCurveEditorSubGroup() override;

    FlatCurveEditor* addCurve(Glib::ustring curveLabel = "", bool periodic = true);
    //virtual void updateBackgroundHistogram (CurveEditor* ce);
    void updateLocallabBackground(CurveEditor* ce) override;
    void switchGUI() override;
    void refresh(CurveEditor* curveToRefresh) override;
    void editModeSwitchedOff() override;
    void pipetteMouseOver(EditDataProvider* provider, int modifierKey) override;
    bool pipetteButton1Pressed(EditDataProvider* provider, int modifierKey) override;
    void pipetteButton1Released(EditDataProvider* provider) override;
    void pipetteDrag(EditDataProvider* provider, int modifierKey) override;
    void showCoordinateAdjuster(CoordinateProvider* provider) override;
    void stopNumericalAdjustment() override;

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
    void removeEditor() override;
    const std::vector<double> getCurveFromGUI(int type) override;
    void editPointToggled(Gtk::ToggleButton* button);
    void editToggled(Gtk::ToggleButton* button);
};
