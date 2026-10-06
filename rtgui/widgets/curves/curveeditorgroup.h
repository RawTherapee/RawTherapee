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

#include "guiutils.h"
#include "mycurve.h"
#include "widgets/basic/shcselector.h"

#include "rtengine/diagonalcurvetypes.h"
#include "rtengine/flatcurvetypes.h"

#include <gtkmm.h>

#include <fstream>
#include <string>

class CurveEditor;
class DiagonalCurveEditorSubGroup;
class FlatCurveEditorSubGroup;

/*
 * This class handle the curve widgets, shared between any number curve
 * - to add a curve to the list, use the 'addCurve' method
 * - to start a new line of curve button, use the 'newLine' method
 * - if you add more than one curve, you must add a "CurveEditor* ce" parameter to your listener
 */
class CurveEditorGroup final : public Gtk::Grid, public CurveListener
{

    friend class CurveEditor;
    friend class CurveEditorSubGroup;
    friend class DiagonalCurveEditorSubGroup;
    friend class FlatCurveEditorSubGroup;

private:
    Glib::ustring& curveDir;
    int line;

protected:
    Gtk::Label* curveGroupLabel;
    Gtk::Button* curve_reset;
    std::vector<CurveEditor*> curveEditors;
    CurveEditor* displayedCurve;
    FlatCurveEditorSubGroup* flatSubGroup;
    DiagonalCurveEditorSubGroup* diagonalSubGroup;

    CurveListener* cl;

    unsigned int numberOfPackedCurve;

public:
    /**
     * @param curveDir The folder used by load and save dialogs for the curve.
     *                 This variable will be updated with actions in the
     *                 dialogs.
     */

    explicit CurveEditorGroup(Glib::ustring& curveDir,
                              const Glib::ustring& groupLabel = "",
                              int blank = 0);
    ~CurveEditorGroup() override;
    void newLine();
    void curveListComplete();
    void setBatchMode(bool batchMode);
    void setCurveExternal(CurveEditor* ce, const std::vector<double>& c);
    void setCurveListener(CurveListener* l) { cl = l; }
    void setTooltip(Glib::ustring ttip);
    CurveEditor* getDisplayedCurve() { return displayedCurve; }
    //void on_realize ();
    CurveEditor* addCurve(CurveType cType,
                          Glib::ustring curveLabel,
                          Gtk::Widget* relatedWidget = nullptr,
                          bool expandRelatedWidget = true,
                          bool periodic = true);
    void attachCurve(Gtk::Grid* curve);

protected:
    //void curveTypeToggled ();
    void curveTypeToggled(CurveEditor* ce);
    //void typeSelectionChanged (int n);
    void typeSelectionChanged(CurveEditor* ce, int n);
    void hideCurrentCurve();
    void updateGUI(CurveEditor* ce);
    void curveResetPressed();
    void curveChanged() override;
    float
    blendPipetteValues(CurveEditor* ce, float chan1, float chan2, float chan3) override;
    void setUnChanged(bool uc, CurveEditor* ce);
};

class CoordinateProvider;

class CurveEditorSubGroup
{

    friend class CurveEditorGroup;

private:
    Glib::ustring& curveDir;
    Glib::ustring lastFilename;

protected:
    int valLinear;
    int valUnchanged;
    CurveEditorGroup* parent;

    ColoredBar* leftBar;
    ColoredBar* bottomBar;

    void initButton(Gtk::Button& button,
                    const Glib::ustring& iconName,
                    Gtk::Align align,
                    bool separatorButton,
                    const Glib::ustring& tooltip = {});

public:
    virtual ~CurveEditorSubGroup();
    int getValUnchanged() { return valUnchanged; }
    int getValLinear() { return valLinear; }
    void updateEditButton(CurveEditor* curve,
                          Gtk::ToggleButton* button,
                          sigc::connection& connection);
    virtual void updateBackgroundHistogram(CurveEditor* ce) {}
    virtual void updateLocallabBackground(CurveEditor* ce) {};
    virtual void switchGUI() = 0;
    virtual void refresh(CurveEditor* curveToRefresh) = 0;
    virtual void editModeSwitchedOff() = 0;

    virtual void showCoordinateAdjuster(CoordinateProvider* provider) = 0;
    virtual void stopNumericalAdjustment() = 0;

    virtual void pipetteMouseOver(EditDataProvider* provider, int modifierKey) = 0;
    virtual bool pipetteButton1Pressed(EditDataProvider* provider, int modifierKey) = 0;
    virtual void pipetteButton1Released(EditDataProvider* provider) = 0;
    virtual void pipetteDrag(EditDataProvider* provider, int modifierKey) = 0;

    // Reset a curve editor, return TRUE if successful (curve changed)
    virtual bool curveReset(CurveEditor* ce) = 0;

protected:
    /**
     * @param curveDir The folder used by load and save dialogs for the curve.
     *                 This variable will be updated with actions in the
     *                 dialogs.
     */
    explicit CurveEditorSubGroup(Glib::ustring& curveDir);

    Glib::ustring outputFile();
    Glib::ustring inputFile();

    virtual void storeCurveValues(CurveEditor* ce, const std::vector<double>& p) = 0;
    virtual void storeDisplayedCurve() = 0;
    virtual void restoreDisplayedHistogram() {};
    virtual void restoreLocallabBackground() {};
    virtual void removeEditor() = 0;
    virtual const std::vector<double> getCurveFromGUI(int type) = 0;
};
