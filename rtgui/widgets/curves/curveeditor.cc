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
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "curveeditor.h"

#include "curveeditorgroup.h"
#include "guiutils.h"
#include "multilangmgr.h"
#include "widgets/basic/popuptogglebutton.h"

#include "rtengine/LUT.h"

#include <cstring>
#include <fstream>
#include <string>

namespace {

class CurveTypePopUpButton : public PopUpToggleButton
{
public:
    CurveTypePopUpButton(const Glib::ustring& label = "") : PopUpToggleButton(label) {}

    void setPosIndexMap(const std::vector<int>& pmap) { posidxmap_ = pmap; }

protected:
    int posToIndex(int pos) const override
    {
        if (pos < 0 || size_t(pos) >= posidxmap_.size()) {
            return pos;
        }
        return posidxmap_[pos];
    }

    int indexToPos(int index) const override
    {
        if (index < 0 || size_t(index) >= posidxmap_.size()) {
            return index;
        }
        for (int i = 0, n = int(posidxmap_.size()); i < n; ++i) {
            if (posidxmap_[i] == index) {
                return i;
            }
        }
        return -1;
    }

private:
    std::vector<int> posidxmap_;
};

}  // namespace

bool CurveEditor::reset()
{
    return subGroup->curveReset(this);
}

DiagonalCurveEditor::DiagonalCurveEditor(Glib::ustring text,
                                         CurveEditorGroup* ceGroup,
                                         CurveEditorSubGroup* ceSubGroup) :
    CurveEditor::CurveEditor(text, static_cast<CurveEditorGroup*>(ceGroup), ceSubGroup)
{

    curveType->addEntry("curve-linear-small", M("CURVEEDITOR_LINEAR"));  // 0 Linear
    curveType->addEntry("curve-spline-small", M("CURVEEDITOR_CUSTOM"));  // 1 Spline
    curveType->addEntry("curve-catmullrom-small",
                        M("CURVEEDITOR_CATMULLROM"));  // 4 CatmullRom
    curveType->addEntry("curve-parametric-small",
                        M("CURVEEDITOR_PARAMETRIC"));                  // 2 Parametric
    curveType->addEntry("curve-nurbs-small", M("CURVEEDITOR_NURBS"));  // 3 NURBS
    static_cast<CurveTypePopUpButton*>(curveType)->setPosIndexMap({ 0, 1, 4, 2, 3 });
    curveType->setSelected(DCT_Linear);

    curveType->show();

    rangeLabels[0] = M("CURVEEDITOR_SHADOWS");
    rangeLabels[1] = M("CURVEEDITOR_DARKS");
    rangeLabels[2] = M("CURVEEDITOR_LIGHTS");
    rangeLabels[3] = M("CURVEEDITOR_HIGHLIGHTS");

    rangeMilestones[0] = 0.25;
    rangeMilestones[1] = 0.50;
    rangeMilestones[2] = 0.75;
}

std::vector<double> DiagonalCurveEditor::getCurve()
{
    std::vector<double> curve;

    switch (selected) {
        case (DCT_Spline):
            return curve = customCurveEd;

        case (DCT_Parametric):
            return curve = paramCurveEd;

        case (DCT_NURBS):
            return curve = NURBSCurveEd;

        case (DCT_CatumullRom):
            return curve = catmullRomCurveEd;

        default:
            // returning Linear or Unchanged
            curve.push_back((double)(selected));
            return curve;
    }
}

void DiagonalCurveEditor::setResetCurve(DiagonalCurveType cType,
                                        const std::vector<double>& resetCurve)
{
    switch (cType) {
        case (DCT_NURBS):
            if (resetCurve.size() && DiagonalCurveType(resetCurve.at(0)) == cType) {
                NURBSResetCurve = resetCurve;
            }

            break;

        case (DCT_Parametric):
            if (resetCurve.size() && DiagonalCurveType(resetCurve.at(0)) == cType) {
                paramResetCurve = resetCurve;
            }

            break;

        case (DCT_Spline):
            if (resetCurve.size() && DiagonalCurveType(resetCurve.at(0)) == cType) {
                customResetCurve = resetCurve;
            }

            break;

        case (DCT_CatumullRom):
            if (resetCurve.size() && DiagonalCurveType(resetCurve.at(0)) == cType) {
                catmullRomResetCurve = resetCurve;
            }

            break;

        default:
            break;
    }
}

void DiagonalCurveEditor::setRangeLabels(Glib::ustring r1,
                                         Glib::ustring r2,
                                         Glib::ustring r3,
                                         Glib::ustring r4)
{
    rangeLabels[0] = r1;
    rangeLabels[1] = r2;
    rangeLabels[2] = r3;
    rangeLabels[3] = r4;
}

void DiagonalCurveEditor::getRangeLabels(Glib::ustring& r1,
                                         Glib::ustring& r2,
                                         Glib::ustring& r3,
                                         Glib::ustring& r4)
{
    r1 = rangeLabels[0];
    r2 = rangeLabels[1];
    r3 = rangeLabels[2];
    r4 = rangeLabels[3];
}

/*
 * Admittedly that this method is called just after the instantiation of this class, we set the shcselector's default values
 */
void DiagonalCurveEditor::setRangeDefaultMilestones(double m1, double m2, double m3)
{
    rangeMilestones[0] = m1;
    rangeMilestones[1] = m2;
    rangeMilestones[2] = m3;

    paramCurveEd.at(1) = m1;
    paramCurveEd.at(2) = m2;
    paramCurveEd.at(3) = m3;
}

void DiagonalCurveEditor::getRangeDefaultMilestones(double& m1, double& m2, double& m3)
{
    m1 = rangeMilestones[0];
    m2 = rangeMilestones[1];
    m3 = rangeMilestones[2];
}

FlatCurveEditor::FlatCurveEditor(Glib::ustring text,
                                 CurveEditorGroup* ceGroup,
                                 CurveEditorSubGroup* ceSubGroup,
                                 bool isPeriodic) :
    CurveEditor::CurveEditor(text, static_cast<CurveEditorGroup*>(ceGroup), ceSubGroup)
{

    periodic = isPeriodic;
    identityValue = 0.5;

    // Order set in the same order than "enum FlatCurveType". Shouldn't change, for compatibility reason
    curveType->addEntry("curve-flat-small", M("CURVEEDITOR_LINEAR"));  // 0 Linear
    curveType->addEntry("curve-controlpoints-small",
                        M("CURVEEDITOR_MINMAXCPOINTS"));  // 1 Min/Max ControlPoints
    curveType->setSelected(FCT_Linear);
    curveType->show();
}

std::vector<double> FlatCurveEditor::getCurve()
{
    std::vector<double> curve;

    switch (selected) {
        //case (Parametric):
        //    return curve = paramCurveEd;
        case (FCT_MinMaxCPoints):
            return curve = controlPointsCurveEd;

        default:
            // returning Linear or Unchanged
            curve.push_back((double)(selected));
            return curve;
    }
}

void FlatCurveEditor::setResetCurve(FlatCurveType cType,
                                    const std::vector<double>& resetCurve)
{
    switch (cType) {
        case (FCT_MinMaxCPoints):
            if (resetCurve.size() && FlatCurveType(resetCurve.at(0)) == cType) {
                controlPointsResetCurve = resetCurve;
            }

            break;

        default:
            break;
    }
}

/*
 * CurveEditor (CurveEditorGroup* ceGroup, Glib::ustring text)
 *
 * parameters:
 *      ceGroup = NULL or the address of the Widget that will receive the CurveTypeToggleButton
 *      text    = (optional) label of the curve, displayed in the CurveTypeToggleButton, next to the image
 */
CurveEditor::CurveEditor(Glib::ustring text,
                         CurveEditorGroup* ceGroup,
                         CurveEditorSubGroup* ceSubGroup) :
    EditSubscriber(ET_PIPETTE)
{

    bgHistValid = false;
    locallabRef = 0.0;
    remoteDrag = false;
    selected = DCT_Linear;
    bottomBarCP = nullptr;
    bottomBarCId = 0;
    leftBarCP = nullptr;
    leftBarCId = 0;
    curveCP = nullptr;
    curveCId = 0;
    relatedWidget = nullptr;
    expandRelatedWidget = true;

    group = ceGroup;
    subGroup = ceSubGroup;

    if (group && text.size()) {
        curveType = new CurveTypePopUpButton(text + ":");
    } else {
        curveType = new CurveTypePopUpButton();
    }

    curveType->set_tooltip_text(M("CURVEEDITOR_TYPE"));
    // TODO: Does this signal have to be blocked when on curve type change ?
    curveType->signal_toggled().connect(
        sigc::mem_fun(*this, &CurveEditor::curveTypeToggled));
    typeconn = curveType->signal_item_selected().connect(
        sigc::mem_fun(*this, &CurveEditor::typeSelectionChanged));
}

void CurveEditor::setCurve(const std::vector<double>& p)
{
    tempCurve = p;
    group->setCurveExternal(this, p);
}

CurveEditor::~CurveEditor()
{
    delete curveType;
}

void CurveEditor::typeSelectionChanged(int n)
{
    group->typeSelectionChanged(this, n);
}

void CurveEditor::curveTypeToggled()
{
    group->curveTypeToggled(this);
}

bool CurveEditor::isUnChanged()
{
    return curveType->getSelected() == subGroup->getValUnchanged();
}

void CurveEditor::setUnChanged(bool uc)
{
    group->setUnChanged(uc, this);
}

/*
 * Update the backgrounds histograms
 */
void CurveEditor::updateBackgroundHistogram(const LUTu& hist)
{
    // Copy the histogram in the curve editor cache
    if (hist) {
        histogram = hist;
        bgHistValid = true;
    } else {
        bgHistValid = false;
    }

    // Then call the curve editor group to eventually update the histogram
    subGroup->updateBackgroundHistogram(this);
}

/*
 * Update Locallab reference value displayed in the background
 */
void CurveEditor::updateLocallabBackground(double ref)
{
    // Copy Locallab reference value in the curve editor cache
    locallabRef = ref;

    // Then call the curve editor group to eventually update the histogram
    subGroup->updateLocallabBackground(this);
}

// Open up the curve if it has modifications and it's not already opened
// Returns: true if curve was non linear and opened
bool CurveEditor::openIfNonlinear()
{

    bool nonLinear = tempCurve.size() && (tempCurve[0] > subGroup->getValLinear())
                     && (tempCurve[0] < subGroup->getValUnchanged());

    if (nonLinear && !curveType->get_active()) {
        // Will trigger the signal_clicked event doing the display
        curveType->set_active(true);
    }

    return nonLinear;
}

// Handles markup tooltips
void CurveEditor::setTooltip(Glib::ustring ttip)
{
    curveType->set_tooltip_text(
        ttip.empty()
            ? Glib::ustring::compose("<b>%1</b> ", M("CURVEEDITOR_TYPE"))
            : Glib::ustring::compose("%1\n<b>%2</b>", ttip, M("CURVEEDITOR_TYPE")));
}

void CurveEditor::setLeftBarColorProvider(ColorProvider* cp, int callerId)
{
    leftBarCP = cp;
    leftBarCId = callerId;
}

void CurveEditor::setBottomBarColorProvider(ColorProvider* cp, int callerId)
{
    bottomBarCP = cp;
    bottomBarCId = callerId;
}

void CurveEditor::setLeftBarBgGradient(const std::vector<GradientMilestone>& milestones)
{
    leftBarBgGradient = milestones;
}

void CurveEditor::setBottomBarBgGradient(const std::vector<GradientMilestone>& milestones)
{
    bottomBarBgGradient = milestones;
}

void CurveEditor::refresh()
{
    subGroup->refresh(this);
}

void CurveEditor::setCurveColorProvider(ColorProvider* cp, int callerId)
{
    curveCP = cp;
    curveCId = callerId;
}

ColorProvider* CurveEditor::getLeftBarColorProvider()
{
    return leftBarCP;
}

ColorProvider* CurveEditor::getBottomBarColorProvider()
{
    return bottomBarCP;
}

ColorProvider* CurveEditor::getCurveColorProvider()
{
    return curveCP;
}

int CurveEditor::getLeftBarCallerId()
{
    return leftBarCId;
}

int CurveEditor::getBottomBarCallerId()
{
    return bottomBarCId;
}

int CurveEditor::getCurveCallerId()
{
    return curveCId;
}

std::vector<GradientMilestone> CurveEditor::getBottomBarBgGradient() const
{
    return bottomBarBgGradient;
}

std::vector<GradientMilestone> CurveEditor::getLeftBarBgGradient() const
{
    return leftBarBgGradient;
}

sigc::signal<void> CurveEditor::signal_curvegraph_enter()
{
    return sig_curvegraph_enter;
}

sigc::signal<void> CurveEditor::signal_curvegraph_leave()
{
    return sig_curvegraph_leave;
}

sigc::signal<void> CurveEditor::signal_curvepoint_click()
{
    return sig_curvepoint_click;
}

sigc::signal<void> CurveEditor::signal_curvepoint_release()
{
    return sig_curvepoint_release;
}

void CurveEditor::switchOffEditMode()
{
    if (EditSubscriber::getEditID() != EUID_None) {
        // switching off the toggle button
        if (group->displayedCurve == this) {
            subGroup->editModeSwitchedOff();
        }
    }

    EditSubscriber::switchOffEditMode();  // disconnect
}

bool CurveEditor::mouseOver(int modifierKey)
{
    EditDataProvider* provider = getEditProvider();
    subGroup->pipetteMouseOver(provider, modifierKey);
    subGroup->refresh(this);
    return true;  // return true will ask the preview to be redrawn, for the cursor
}

bool CurveEditor::button1Pressed(int modifierKey)
{
    EditDataProvider* provider = getEditProvider();

    if (provider->getObject()) {
        remoteDrag = subGroup->pipetteButton1Pressed(provider, modifierKey);
    }

    if (remoteDrag) {
        action = EditSubscriber::Action::DRAGGING;
    }

    subGroup->refresh(this);
    return true;
}

bool CurveEditor::button1Released()
{
    EditDataProvider* provider = getEditProvider();
    subGroup->pipetteButton1Released(provider);
    remoteDrag = false;
    subGroup->refresh(this);
    return true;
}

bool CurveEditor::drag1(int modifierKey)
{
    EditDataProvider* provider = getEditProvider();
    subGroup->pipetteDrag(provider, modifierKey);
    subGroup->refresh(this);
    return false;
}

CursorShape CurveEditor::getCursor(int objectID, int xPos, int yPos) const
{
    if (remoteDrag) {
        return CSResizeHeight;
    }

    return CSHandOpen;
}
