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

#include "editcallbacks.h"
#include "guiutils.h"

#include "rtengine/LUT.h"
#include "rtengine/diagonalcurvetypes.h"
#include "rtengine/flatcurvetypes.h"
#include "rtengine/noncopyable.h"

class CurveEditorGroup;
class CurveEditorSubGroup;
class ColorProvider;
class PopUpToggleButton;

/*
 *********************** Curve Editor ***********************
 */

/** @brief This class is an interface between RT and the curve editor group
 * It handles the methods related to a specific curve. It is created by CurveEditorGroup::addCurve
 */
class CurveEditor : public EditSubscriber, public rtengine::NonCopyable
{

    friend class CurveEditorGroup;
    friend class CurveEditorSubGroup;
    friend class DiagonalCurveEditorSubGroup;
    friend class FlatCurveEditorSubGroup;
    friend class DiagonalCurveEditor;
    friend class FlatCurveEditor;

protected:
    /*
     * The curve editor contains only one widget (the curve type button) to receive the signals
     * but it's co-handled by the CurveEditorGroup too
    */

    PopUpToggleButton* curveType;
    LUTu histogram;  // histogram values
    bool bgHistValid;
    double locallabRef;  // Locallab reference value
    bool remoteDrag;

    int selected;

    CurveEditorGroup* group;
    CurveEditorSubGroup* subGroup;
    Gtk::Widget* relatedWidget;
    bool expandRelatedWidget;

    std::vector<double> tempCurve;
    sigc::connection typeconn;

    ColorProvider* bottomBarCP;
    ColorProvider* leftBarCP;
    ColorProvider* curveCP;
    int bottomBarCId;
    int leftBarCId;
    int curveCId;
    std::vector<GradientMilestone> bottomBarBgGradient;
    std::vector<GradientMilestone> leftBarBgGradient;

    sigc::signal<void> sig_curvegraph_enter;
    sigc::signal<void> sig_curvegraph_leave;
    sigc::signal<void> sig_curvepoint_click;
    sigc::signal<void> sig_curvepoint_release;

public:
    CurveEditor(Glib::ustring text,
                CurveEditorGroup* ceGroup,
                CurveEditorSubGroup* ceSubGroup);
    ~CurveEditor() override;
    void typeSelectionChanged(int n);
    void curveTypeToggled();
    bool isUnChanged();
    void setUnChanged(bool uc);
    void updateBackgroundHistogram(const LUTu& hist);
    void updateLocallabBackground(double ref);
    void setLeftBarColorProvider(ColorProvider* cp, int callerId);
    void setBottomBarColorProvider(ColorProvider* cp, int callerId);
    void setCurveColorProvider(ColorProvider* cp, int callerId);
    void setBottomBarBgGradient(const std::vector<GradientMilestone>& milestones);
    void setLeftBarBgGradient(const std::vector<GradientMilestone>& milestones);
    ColorProvider* getLeftBarColorProvider();
    ColorProvider* getBottomBarColorProvider();
    ColorProvider* getCurveColorProvider();
    int getLeftBarCallerId();
    int getBottomBarCallerId();
    int getCurveCallerId();
    std::vector<GradientMilestone> getBottomBarBgGradient() const;
    std::vector<GradientMilestone> getLeftBarBgGradient() const;

    // refresh the display of the CurveEditor (e.g. when a ColoredBar has been changed from the outside)
    void refresh();
    // Open up the curve if it has modifications and it's not already opened
    bool openIfNonlinear();

    void setCurve(const std::vector<double>& p);
    virtual void setIdentityValue(const double iValue = 0.5) {};
    virtual double getIdentityValue() { return 0.5; };
    virtual std::vector<double> getCurve() = 0;
    bool reset();
    void setTooltip(Glib::ustring ttip);

    sigc::signal<void> signal_curvegraph_enter();
    sigc::signal<void> signal_curvegraph_leave();
    sigc::signal<void> signal_curvepoint_click();
    sigc::signal<void> signal_curvepoint_release();

    void switchOffEditMode() override;
    bool mouseOver(int modifierKey) override;
    bool button1Pressed(int modifierKey) override;
    bool button1Released() override;
    bool drag1(int modifierKey) override;
    CursorShape getCursor(int objectID, int xPos, int yPos) const override;
};

/*
 ******************** Diagonal Curve Editor ********************
 */

class DiagonalCurveEditor final : public CurveEditor
{

    friend class DiagonalCurveEditorSubGroup;

protected:
    // reflects the buttonType active selection ; used as a pre-'selectionChange' reminder value
    std::vector<double> customCurveEd;
    std::vector<double> customResetCurve;
    std::vector<double> paramCurveEd;
    std::vector<double> paramResetCurve;
    std::vector<double> NURBSCurveEd;
    std::vector<double> NURBSResetCurve;
    std::vector<double> catmullRomCurveEd;
    std::vector<double> catmullRomResetCurve;
    Glib::ustring rangeLabels[4];
    double rangeMilestones[3];

public:
    DiagonalCurveEditor(Glib::ustring text,
                        CurveEditorGroup* ceGroup,
                        CurveEditorSubGroup* ceSubGroup);
    std::vector<double> getCurve() override;
    void setRangeLabels(Glib::ustring r1,
                        Glib::ustring r2,
                        Glib::ustring r3,
                        Glib::ustring r4);
    void getRangeLabels(Glib::ustring& r1,
                        Glib::ustring& r2,
                        Glib::ustring& r3,
                        Glib::ustring& r4);
    void setRangeDefaultMilestones(double m1, double m2, double m3);
    void getRangeDefaultMilestones(double& m1, double& m2, double& m3);

    // set the reset curve for a given curve type. This is optional; all curve type have a default reset curve
    void setResetCurve(DiagonalCurveType cType, const std::vector<double>& resetCurve);
};

/*
 ********************** Flat Curve Editor **********************
 */

class FlatCurveEditor final : public CurveEditor
{
    friend class FlatCurveEditorSubGroup;

protected:
    // reflects the buttonType active selection ; used as a pre-'selectionChange' reminder value
    std::vector<double> controlPointsCurveEd;
    std::vector<double> controlPointsResetCurve;
    bool periodic;
    double identityValue;

public:
    FlatCurveEditor(Glib::ustring text,
                    CurveEditorGroup* ceGroup,
                    CurveEditorSubGroup* ceSubGroup,
                    bool isPeriodic = true);
    void setIdentityValue(const double iValue = 0.5) override { identityValue = iValue; }
    double getIdentityValue() override { return identityValue; };
    std::vector<double> getCurve() override;

    // set the reset curve for a given curve type. This is optional; all curve type have a default reset curve
    void setResetCurve(FlatCurveType cType, const std::vector<double>& resetCurve);
};
