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
 *  Class created by Jean-Christophe FRISCH, aka 'Hombre'
 */

#include "curveeditorgroup.h"

#include "curveeditor.h"
#include "diagonalcurveeditorsubgroup.h"
#include "flatcurveeditorsubgroup.h"
#include "multilangmgr.h"
#include "options.h"
#include "pathutils.h"
#include "rtimage.h"
#include "rtscalable.h"
#include "widgets/basic/popuptogglebutton.h"

CurveEditorGroup::CurveEditorGroup(Glib::ustring& curveDir,
                                   const Glib::ustring& groupLabel,
                                   int blank) :
    curveDir(curveDir),
    line(0),
    curve_reset(nullptr),
    displayedCurve(nullptr),
    flatSubGroup(nullptr),
    diagonalSubGroup(nullptr),
    cl(nullptr),
    numberOfPackedCurve(0)
{

    // We set the label to the one provided as parameter, even if it's an empty string
    if (blank == 0) {
        curveGroupLabel = Gtk::manage(new Gtk::Label(groupLabel + ":", Gtk::ALIGN_START));
    } else if (blank == 1) {
        curveGroupLabel = Gtk::manage(new Gtk::Label(groupLabel, Gtk::ALIGN_START));
    }

    setExpandAlignProperties(curveGroupLabel, false, false, Gtk::ALIGN_START,
                             Gtk::ALIGN_CENTER);
    set_row_spacing(RTScalable::scalePixelSize(1));
}

CurveEditorGroup::~CurveEditorGroup()
{
    for (std::vector<CurveEditor*>::iterator i = curveEditors.begin();
         i != curveEditors.end(); ++i)
    {
        delete *i;
    }

    delete flatSubGroup;
    delete diagonalSubGroup;
}

void CurveEditorGroup::hideCurrentCurve()
{
    // Setting the curve type to 'Unchanged' hide the CurveEditor
    if (diagonalSubGroup) {
        diagonalSubGroup->stopNumericalAdjustment();
    }

    if (flatSubGroup) {
        flatSubGroup->stopNumericalAdjustment();
    }

    if (displayedCurve) {
        displayedCurve->curveType->set_active(false);
    }
}

/*
 * Add a new curve to the curves list
 *
 * Parameters:
 *     cType:         enum saying which kind of curve type has to be created
 *     curveLabel:    Name of the curve that will be inserted in the toggle button, before the image.
 *                    If empty, no text will prepend the image
 *     relatedWidget: pointer to a widget (or NULL) that will be inserted next to the curve's toggle button.
 *                    if a smart pointer created by Gtk::manage is passed in, the widget will be deleted by the destructor,
 *                    otherwise it'll have to be delete it manually
 *     periodic:      for FlatCurve only, ask the curve to be periodic (default: True)
 *
 */
CurveEditor* CurveEditorGroup::addCurve(CurveType cType,
                                        Glib::ustring curveLabel,
                                        Gtk::Widget* relatedWidget,
                                        bool expandRelatedWidget,
                                        bool periodic)
{
    switch (cType) {
        case (CT_Diagonal):
        {
            if (!diagonalSubGroup) {
                diagonalSubGroup = new DiagonalCurveEditorSubGroup(this, curveDir);
            }

            // We add it to the curve editor list
            DiagonalCurveEditor* newCE = diagonalSubGroup->addCurve(curveLabel);
            newCE->relatedWidget = relatedWidget;
            newCE->expandRelatedWidget = expandRelatedWidget;
            curveEditors.push_back(newCE);
            return (newCE);
        }

        case (CT_Flat):
        {
            if (!flatSubGroup) {
                flatSubGroup = new FlatCurveEditorSubGroup(this, curveDir);
            }

            // We add it to the curve editor list
            FlatCurveEditor* newCE = flatSubGroup->addCurve(curveLabel, periodic);
            newCE->relatedWidget = relatedWidget;
            newCE->expandRelatedWidget = expandRelatedWidget;
            curveEditors.push_back(newCE);
            return (newCE);
        }

        default:
            return (static_cast<CurveEditor*>(nullptr));
            break;
    }

    return nullptr;  // to avoid complains from Gcc
}

/*
 * Use this method to start a new line of button
 */
void CurveEditorGroup::newLine()
{

    if (curveEditors.size() > numberOfPackedCurve) {
        Gtk::Grid* currLine = Gtk::manage(new Gtk::Grid());
        setExpandAlignProperties(currLine, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_START);
        currLine->set_column_spacing(RTScalable::scalePixelSize(1));

        bool isHeader = false;
        int x = 0;

        if (!numberOfPackedCurve) {
            isHeader = true;
            currLine->attach(*curveGroupLabel, x++, 0, 1, 1);
        }

        bool rwe = false;

        for (int i = numberOfPackedCurve; i < (int)(curveEditors.size()); ++i) {
            if (curveEditors[i]->relatedWidget != nullptr
                && curveEditors[i]->expandRelatedWidget)
            {
                rwe = true;
            }
        }

        for (int i = numberOfPackedCurve; i < (int)(curveEditors.size()); ++i) {
            setExpandAlignProperties(curveEditors[i]->curveType->buttonGroup, !rwe, true,
                                     Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
            currLine->attach(*curveEditors[i]->curveType->buttonGroup, x++, 0, 1, 1);

            if (curveEditors[i]->relatedWidget != nullptr) {
                setExpandAlignProperties(curveEditors[i]->relatedWidget,
                                         curveEditors[i]->expandRelatedWidget, true,
                                         Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
                currLine->attach(*curveEditors[i]->relatedWidget, x++, 0, 1, 1);
            }

            numberOfPackedCurve++;
        }

        if (isHeader) {
            curve_reset = Gtk::manage(new Gtk::Button());
            setExpandAlignProperties(curve_reset, false, false, Gtk::ALIGN_CENTER,
                                     Gtk::ALIGN_FILL);
            curve_reset->add(
                *Gtk::manage(new RTImage("undo-small", Gtk::ICON_SIZE_BUTTON)));
            curve_reset->set_relief(Gtk::RELIEF_NONE);
            curve_reset->set_tooltip_text(M("CURVEEDITOR_TOOLTIPLINEAR"));
            curve_reset->signal_clicked().connect(
                sigc::mem_fun(*this, &CurveEditorGroup::curveResetPressed));

            currLine->attach(*curve_reset, x++, 0, 1, 1);
        }

        attach(*currLine, 0, line++, 1, 1);
    }
}

void CurveEditorGroup::attachCurve(Gtk::Grid* curve)
{
    attach(*curve, 0, line, 1, 1);
}

/*
 * Create all the widgets now that the curve list is complete
 * This method should handle all curve number correctly, i.e. eventually display the curve type buttons
 * in a grid (or table)
 */
void CurveEditorGroup::curveListComplete()
{
    newLine();

    // We check the length of the label ; if it contains only one char (':'), we set it to the right default string
    if (curveGroupLabel->get_label().size() == 1) {
        curveGroupLabel->set_label(
            M(curveEditors.size() > 1 ? "CURVEEDITOR_CURVES" : "CURVEEDITOR_CURVE")
            + ":");
    }

    if (curveEditors.size() > 1) {
        cl->setMulti(true);
    }
}

/*
 * Callback method used when a curve type button has changed ;
 * it will activate the button, and so emit 'signal_toggled' (-> curveTypeToggled here under)
 */
void CurveEditorGroup::typeSelectionChanged(CurveEditor* ce, int n)
{
    // Same curve and same type : do nothing
    if (ce == displayedCurve && n == (int)ce->selected) {
        return;
    }

    if (n < ce->subGroup->valUnchanged) {
        ce->selected = n;
    }

    // The user selected a new type from a toggled off button
    if (ce != displayedCurve)
    // We toggle off the other curve: it will emit the toggle off signal
    {
        hideCurrentCurve();
    }

    // If the button was not pressed before
    if (!ce->curveType->get_active()) {
        ce->subGroup->storeDisplayedCurve();
        // We set it pressed : it will emit the toggle on signal and update the GUI
        ce->curveType->set_active(n > ce->subGroup->valLinear
                                  && n < ce->subGroup->valUnchanged);

        if (n == ce->subGroup->valLinear || n == ce->subGroup->valUnchanged) {
            // Since we do not activate the curve when the user switch the toggled off button to 'Linear', we have to
            // to call the curve listener manually, because 'curveChanged' uses displayedCurve...
            if (cl) {
                if (cl->isMulti()) {
                    cl->curveChanged(ce);
                } else {
                    cl->curveChanged();
                }
            }
        } else {
            curveChanged();
        }
    } else {
        // The button is already pressed so we switch the GUI ourselves
        ce->subGroup->switchGUI();
        curveChanged();
    }
}

/*
 * Callback method used when a button has been toggled on/off
 * It then hide any other displayed curve and display it's curve
 */
void CurveEditorGroup::curveTypeToggled(CurveEditor* ce)
{
    bool curveRestored = false;

    if (displayedCurve) {
        EditDataProvider* editProvider = displayedCurve->getEditProvider();

        if (editProvider && editProvider->getCurrSubscriber() == displayedCurve) {
            displayedCurve->switchOffEditMode();
        }
    }

    // Looking for the button state
    if (ce->curveType->get_active()) {
        // The button is now pressed, so we have to first hide all other CurveEditor
        hideCurrentCurve();

        displayedCurve = ce;

        if (ce->curveType->getSelected() == ce->subGroup->valUnchanged) {
            curveRestored = true;
            ce->curveType->setSelected(ce->selected);
        }

        // then show this CurveEditor
        int ct = ce->curveType->getSelected();

        if (ct < ce->subGroup->valUnchanged) {
            ce->subGroup->restoreDisplayedHistogram();
            ce->subGroup->restoreLocallabBackground();
        }
    } else {
        // The button is now released, so we have to hide this CurveEditor
        displayedCurve = nullptr;
    }

    ce->subGroup->switchGUI();

    if (curveRestored) {
        curveChanged();
    }
}

/*
 * Update the GUI if the given curveEditor is currently displayed
 */
void CurveEditorGroup::updateGUI(CurveEditor* ce)
{
    if (!ce) {
        return;
    }

    // we update the curve type button to the corresponding curve type, only if it is not currently set to 'Unchanged'
    if (ce->curveType->getSelected() < ce->subGroup->valUnchanged) {
        ce->curveType->setSelected(ce->selected);
    }

    // if not displayed or "unchanged" is selected, do not change gui
    if (ce == displayedCurve && ce->curveType->getSelected() < ce->subGroup->valUnchanged)
    {
        ce->subGroup->switchGUI();
    }
}

/*
 * Called from the outside to set the curve type & values
 */
void CurveEditorGroup::setCurveExternal(CurveEditor* ce, const std::vector<double>& c)
{
    if (!c.empty()) {
        ce->subGroup->storeCurveValues(ce,
                                       c);  // The new curve is saved in the CurveEditor
        (ce)->selected = c
            [0];  // We set the selected curve type in the CurveEditor to the one of the specified curve
    }

    updateGUI(static_cast<CurveEditor*>(ce));  // And we update the GUI if necessary
}

/*
 * Listener called when the user has modified the curve
 */
void CurveEditorGroup::curveChanged()
{

    displayedCurve->subGroup->storeDisplayedCurve();

    if (cl) {
        if (cl->isMulti()) {
            cl->curveChanged(displayedCurve);
        } else {
            cl->curveChanged();
        }
    }
}

/*
 * Listener called when the user has modified the curve
 */
float CurveEditorGroup::blendPipetteValues(CurveEditor* ce,
                                           float chan1,
                                           float chan2,
                                           float chan3)
{

    if (cl) {
        return cl->blendPipetteValues(ce, chan1, chan2, chan3);
    }

    return -1.f;
}

/*
 * Call back method when the reset button is pressed :
 * reset the currently toggled on curve editor
 */
void CurveEditorGroup::curveResetPressed()
{
    if (displayedCurve) {
        if (displayedCurve->subGroup->curveReset(displayedCurve)) {
            curveChanged();
        }
    }
}

/*
 * Set the tooltip text of the label of the curve group
 */
void CurveEditorGroup::setTooltip(Glib::ustring ttip)
{
    curveGroupLabel->set_tooltip_text(ttip);
}

void CurveEditorGroup::setBatchMode(bool batchMode)
{
    for (std::vector<CurveEditor*>::iterator i = curveEditors.begin();
         i != curveEditors.end(); ++i)
    {
        (*i)->curveType->addEntry("template-24", M("GENERAL_UNCHANGED"));
        (*i)->curveType->show();
    }
}

void CurveEditorGroup::setUnChanged(bool uc, CurveEditor* ce)
{
    if (uc) {
        // the user selected several thumbnails, so we hide the editors and set the curveEditor selection to 'Unchanged'
        //ce->typeconn.block(true);
        // we hide the editor widgets
        hideCurrentCurve();
        // the curve type selected option is set to unchanged
        ce->curveType->setSelected(ce->subGroup->valUnchanged);
        //ce->typeconn.block(false);
    } else {
        // we want it to use back the 'CurveEditor::setCurve' memorized in CurveEditor::tempCurve
        //ce->typeconn.block(true);
        // we switch back the curve type selected option to the one of the used curve
        ce->curveType->setSelected(ce->selected);
        updateGUI(ce);
        //ce->typeconn.block(false);
    }
}

CurveEditorSubGroup::CurveEditorSubGroup(Glib::ustring& curveDir) :
    curveDir(curveDir), valLinear(0), valUnchanged(0), parent(nullptr)
{
    leftBar = nullptr;
    bottomBar = nullptr;
}

CurveEditorSubGroup::~CurveEditorSubGroup()
{
    if (leftBar) {
        delete leftBar;
    }

    if (bottomBar) {
        delete bottomBar;
    }
}

void CurveEditorSubGroup::initButton(Gtk::Button& button,
                                     const Glib::ustring& iconName,
                                     Gtk::Align align,
                                     bool separatorButton,
                                     const Glib::ustring& tooltip)
{
    const auto& options = App::get().options();
    bool hExpand, vExpand;
    if (separatorButton) {
        hExpand = vExpand = true;
    } else {
        vExpand = options.curvebboxpos == 0 || options.curvebboxpos == 2;
        hExpand = !vExpand;
    }
    Gtk::Align hAlign, vAlign;
    if (align == Gtk::ALIGN_START) {
        hAlign = options.curvebboxpos == 0 || options.curvebboxpos == 2 ? Gtk::ALIGN_START
                                                                        : Gtk::ALIGN_FILL;
        vAlign = options.curvebboxpos == 0 || options.curvebboxpos == 2
                     ? Gtk::ALIGN_FILL
                     : Gtk::ALIGN_START;
    } else {
        hAlign = options.curvebboxpos == 0 || options.curvebboxpos == 2 ? Gtk::ALIGN_END
                                                                        : Gtk::ALIGN_FILL;
        vAlign = options.curvebboxpos == 0 || options.curvebboxpos == 2 ? Gtk::ALIGN_FILL
                                                                        : Gtk::ALIGN_END;
    }

    button.add(*Gtk::manage(new RTImage(iconName, Gtk::ICON_SIZE_LARGE_TOOLBAR)));
    button.get_style_context()->add_class(GTK_STYLE_CLASS_FLAT);
    if (!tooltip.empty()) {
        button.set_tooltip_text(M(tooltip));
    }
    setExpandAlignProperties(&button, hExpand, vExpand, hAlign, vAlign);
}

void CurveEditorSubGroup::updateEditButton(CurveEditor* curve,
                                           Gtk::ToggleButton* button,
                                           sigc::connection& connection)
{
    if (!curve->getEditProvider() || curve->getEditID() == EUID_None) {
        button->hide();
    } else {
        button->show();
        bool prevstate = connection.block(true);

        if (curve->isCurrentSubscriber()) {
            button->set_active(true);
        } else {
            button->set_active(false);
        }

        if (!prevstate) {
            connection.block(false);
        }
    }
}

Glib::ustring CurveEditorSubGroup::outputFile()
{

    Gtk::FileChooserDialog dialog(getToplevelWindow(parent),
                                  M("CURVEEDITOR_SAVEDLGLABEL"),
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    bindCurrentFolder(dialog, curveDir);
    dialog.set_current_name(lastFilename);

    dialog.add_button(M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
    dialog.add_button(M("GENERAL_SAVE"), Gtk::RESPONSE_APPLY);

    Glib::RefPtr<Gtk::FileFilter> filter_pp = Gtk::FileFilter::create();
    filter_pp->set_name(M("FILECHOOSER_FILTER_CURVE"));
    filter_pp->add_pattern("*.rtc");
    dialog.add_filter(filter_pp);

    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name(M("FILECHOOSER_FILTER_ANY"));
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    //dialog.set_do_overwrite_confirmation (true);

    Glib::ustring fname;

    do {
        if (dialog.run() == Gtk::RESPONSE_APPLY) {
            fname = dialog.get_filename();

            if (getExtension(fname) != "rtc") {
                fname += ".rtc";
            }

            if (confirmOverwrite(dialog, fname)) {
                lastFilename = Glib::path_get_basename(fname);
                break;
            }
        } else {
            fname = "";
            break;
        }
    } while (1);

    return fname;
}

Glib::ustring CurveEditorSubGroup::inputFile()
{

    Gtk::FileChooserDialog dialog(getToplevelWindow(parent),
                                  M("CURVEEDITOR_LOADDLGLABEL"),
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    bindCurrentFolder(dialog, curveDir);

    dialog.add_button(M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
    dialog.add_button(M("GENERAL_APPLY"), Gtk::RESPONSE_APPLY);

    Glib::RefPtr<Gtk::FileFilter> filter_pp = Gtk::FileFilter::create();
    filter_pp->set_name(M("FILECHOOSER_FILTER_CURVE"));
    filter_pp->add_pattern("*.rtc");
    dialog.add_filter(filter_pp);

    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name(M("FILECHOOSER_FILTER_ANY"));
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    int result = dialog.run();

    Glib::ustring fname;

    if (result == Gtk::RESPONSE_APPLY) {
        fname = dialog.get_filename();

        if (Glib::file_test(fname, Glib::FILE_TEST_EXISTS)) {
            return fname;
        }
    }

    fname = "";
    return fname;
}
