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

#include "diagonalcurveeditorsubgroup.h"

#include "clipboard.h"
#include "curveeditor.h"
#include "guiutils.h"
#include "multilangmgr.h"
#include "mycurve.h"
#include "mydiagonalcurve.h"
#include "options.h"
#include "rtimage.h"
#include "widgets/basic/adjuster.h"
#include "widgets/basic/popuptogglebutton.h"
#include "widgets/basic/shcselector.h"

#include "rtengine/curves.h"

#include <gtkmm.h>

#include <fstream>
#include <string>

DiagonalCurveEditorSubGroup::DiagonalCurveEditorSubGroup(CurveEditorGroup* prt,
                                                         Glib::ustring& curveDir) :
    CurveEditorSubGroup(curveDir)
{
    const auto& options = App::get().options();

    editedAdjuster = nullptr;
    editedAdjusterValue = 0;

    Gtk::PositionType sideStart = options.curvebboxpos == 0 || options.curvebboxpos == 2
                                      ? Gtk::POS_LEFT
                                      : Gtk::POS_TOP;
    Gtk::PositionType sideEnd = options.curvebboxpos == 0 || options.curvebboxpos == 2
                                    ? Gtk::POS_RIGHT
                                    : Gtk::POS_BOTTOM;

    valLinear = (int)DCT_Linear;
    valUnchanged = (int)DCT_Unchanged;
    parent = prt;

    activeParamControl = -1;

    // custom curve
    customCurveGrid = new Gtk::Grid();
    customCurveGrid->set_orientation(Gtk::ORIENTATION_VERTICAL);
    customCurveGrid->get_style_context()->add_class("curve-mainbox");

    customCurve = Gtk::manage(new MyDiagonalCurve());
    customCurve->setType(DCT_Spline);

    Gtk::Grid* customCurveBox = Gtk::manage(new Gtk::Grid());
    customCurveBox->get_style_context()->add_class("curve-curvebox");
    customCurveBox->add(*customCurve);

    Gtk::Grid* custombbox =
        Gtk::manage(new Gtk::Grid());  // curvebboxpos 0=above, 1=right, 2=below, 3=left
    custombbox->get_style_context()->add_class("curve-buttonbox");

    if (options.curvebboxpos == 0) {
        custombbox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        setExpandAlignProperties(custombbox, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);
        customCurveGrid->get_style_context()->add_class("top");
    } else if (options.curvebboxpos == 2) {
        custombbox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        setExpandAlignProperties(custombbox, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);
        customCurveGrid->get_style_context()->add_class("bottom");
    } else if (options.curvebboxpos == 1) {
        custombbox->set_orientation(Gtk::ORIENTATION_VERTICAL);
        setExpandAlignProperties(custombbox, false, true, Gtk::ALIGN_CENTER,
                                 Gtk::ALIGN_FILL);
        customCurveGrid->get_style_context()->add_class("right");
    } else if (options.curvebboxpos == 3) {
        custombbox->set_orientation(Gtk::ORIENTATION_VERTICAL);
        setExpandAlignProperties(custombbox, false, true, Gtk::ALIGN_CENTER,
                                 Gtk::ALIGN_FILL);
        customCurveGrid->get_style_context()->add_class("left");
    }

    editPointCustom = Gtk::manage(new Gtk::ToggleButton());
    initButton(*editPointCustom, Glib::ustring("edit-point"), Gtk::ALIGN_START, false,
               "CURVEEDITOR_EDITPOINT_HINT");
    editCustom = Gtk::manage(new Gtk::ToggleButton());
    initButton(*editCustom, Glib::ustring("crosshair-node-curve"), Gtk::ALIGN_START,
               false, "EDIT_PIPETTE_TOOLTIP");
    editCustom->hide();
    copyCustom = Gtk::manage(new Gtk::Button());
    initButton(*copyCustom, Glib::ustring("copy"), Gtk::ALIGN_END, true);
    pasteCustom = Gtk::manage(new Gtk::Button());
    initButton(*pasteCustom, Glib::ustring("paste"), Gtk::ALIGN_END, false);
    loadCustom = Gtk::manage(new Gtk::Button());
    initButton(*loadCustom, Glib::ustring("folder-open"), Gtk::ALIGN_END, false);
    saveCustom = Gtk::manage(new Gtk::Button());
    initButton(*saveCustom, Glib::ustring("save"), Gtk::ALIGN_END, false);

    custombbox->attach_next_to(*editPointCustom, sideStart, 1, 1);
    custombbox->attach_next_to(*editCustom, sideStart, 1, 1);
    custombbox->attach_next_to(*copyCustom, sideEnd, 1, 1);
    custombbox->attach_next_to(*pasteCustom, sideEnd, 1, 1);
    custombbox->attach_next_to(*loadCustom, sideEnd, 1, 1);
    custombbox->attach_next_to(*saveCustom, sideEnd, 1, 1);

    customCoordAdjuster = Gtk::manage(new CoordinateAdjuster(customCurve, this));
    customCoordAdjuster->get_style_context()->add_class("curve-spinbuttonbox");

    // Button box position: 0=above, 1=right, 2=below, 3=left
    customCurveGrid->add(*customCurveBox);
    customCurve->set_hexpand(true);

    if (options.curvebboxpos == 0) {
        customCurveGrid->attach_next_to(*custombbox, *customCurveBox, Gtk::POS_TOP, 1, 1);
        customCurveGrid->attach_next_to(*customCoordAdjuster, *customCurveBox,
                                        Gtk::POS_BOTTOM, 1, 1);
    } else if (options.curvebboxpos == 1) {
        customCurveGrid->attach_next_to(*custombbox, *customCurveBox, Gtk::POS_RIGHT, 1,
                                        1);
        customCurveGrid->attach_next_to(*customCoordAdjuster, *customCurveBox,
                                        Gtk::POS_BOTTOM, 2, 1);
    } else if (options.curvebboxpos == 2) {
        customCurveGrid->attach_next_to(*customCoordAdjuster, *customCurveBox,
                                        Gtk::POS_BOTTOM, 1, 1);
        customCurveGrid->attach_next_to(*custombbox, *customCoordAdjuster,
                                        Gtk::POS_BOTTOM, 1, 1);
    } else if (options.curvebboxpos == 3) {
        customCurveGrid->attach_next_to(*custombbox, *customCurveBox, Gtk::POS_LEFT, 1,
                                        1);
        customCurveGrid->attach_next_to(*customCoordAdjuster, *custombbox,
                                        Gtk::POS_BOTTOM, 2, 1);
    }

    customCurveGrid->show_all();
    customCoordAdjuster->hide();

    saveCustom->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::savePressed));
    loadCustom->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::loadPressed));
    copyCustom->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::copyPressed));
    pasteCustom->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::pastePressed));
    editPointCustomConn = editPointCustom->signal_toggled().connect(
        sigc::bind(sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::editPointToggled),
                   editPointCustom));
    editCustomConn = editCustom->signal_toggled().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::editToggled), editCustom));

    saveCustom->set_tooltip_text(M("CURVEEDITOR_TOOLTIPSAVE"));
    loadCustom->set_tooltip_text(M("CURVEEDITOR_TOOLTIPLOAD"));
    copyCustom->set_tooltip_text(M("CURVEEDITOR_TOOLTIPCOPY"));
    pasteCustom->set_tooltip_text(M("CURVEEDITOR_TOOLTIPPASTE"));
    // Custom curve end

    // NURBS curve
    NURBSCurveGrid = new Gtk::Grid();
    NURBSCurveGrid->set_orientation(Gtk::ORIENTATION_VERTICAL);
    NURBSCurveGrid->get_style_context()->add_class("curve-mainbox");

    NURBSCurve = Gtk::manage(new MyDiagonalCurve());
    NURBSCurve->setType(DCT_NURBS);

    Gtk::Grid* NURBSCurveBox = Gtk::manage(new Gtk::Grid());
    NURBSCurveBox->get_style_context()->add_class("curve-curvebox");
    NURBSCurveBox->add(*NURBSCurve);

    Gtk::Grid* NURBSbbox = Gtk::manage(new Gtk::Grid());
    NURBSbbox->get_style_context()->add_class("curve-buttonbox");

    if (options.curvebboxpos == 0) {
        NURBSbbox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        setExpandAlignProperties(NURBSbbox, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);
        NURBSCurveGrid->get_style_context()->add_class("top");
    } else if (options.curvebboxpos == 2) {
        NURBSbbox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        setExpandAlignProperties(NURBSbbox, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);
        NURBSCurveGrid->get_style_context()->add_class("bottom");
    } else if (options.curvebboxpos == 1) {
        NURBSbbox->set_orientation(Gtk::ORIENTATION_VERTICAL);
        setExpandAlignProperties(NURBSbbox, false, true, Gtk::ALIGN_CENTER,
                                 Gtk::ALIGN_FILL);
        NURBSCurveGrid->get_style_context()->add_class("right");
    } else if (options.curvebboxpos == 3) {
        NURBSbbox->set_orientation(Gtk::ORIENTATION_VERTICAL);
        setExpandAlignProperties(NURBSbbox, false, true, Gtk::ALIGN_CENTER,
                                 Gtk::ALIGN_FILL);
        NURBSCurveGrid->get_style_context()->add_class("left");
    }

    editPointNURBS = Gtk::manage(new Gtk::ToggleButton());
    initButton(*editPointNURBS, Glib::ustring("edit-point"), Gtk::ALIGN_START, false,
               "CURVEEDITOR_EDITPOINT_HINT");
    editNURBS = Gtk::manage(new Gtk::ToggleButton());
    initButton(*editNURBS, Glib::ustring("crosshair-node-curve"), Gtk::ALIGN_START, false,
               "EDIT_PIPETTE_TOOLTIP");
    editNURBS->hide();
    copyNURBS = Gtk::manage(new Gtk::Button());
    initButton(*copyNURBS, Glib::ustring("copy"), Gtk::ALIGN_END, true);
    pasteNURBS = Gtk::manage(new Gtk::Button());
    initButton(*pasteNURBS, Glib::ustring("paste"), Gtk::ALIGN_END, false);
    loadNURBS = Gtk::manage(new Gtk::Button());
    initButton(*loadNURBS, Glib::ustring("folder-open"), Gtk::ALIGN_END, false);
    saveNURBS = Gtk::manage(new Gtk::Button());
    initButton(*saveNURBS, Glib::ustring("save"), Gtk::ALIGN_END, false);

    NURBSbbox->attach_next_to(*editPointNURBS, sideStart, 1, 1);
    NURBSbbox->attach_next_to(*editNURBS, sideStart, 1, 1);
    NURBSbbox->attach_next_to(*copyNURBS, sideEnd, 1, 1);
    NURBSbbox->attach_next_to(*pasteNURBS, sideEnd, 1, 1);
    NURBSbbox->attach_next_to(*loadNURBS, sideEnd, 1, 1);
    NURBSbbox->attach_next_to(*saveNURBS, sideEnd, 1, 1);

    NURBSCoordAdjuster = Gtk::manage(new CoordinateAdjuster(NURBSCurve, this));
    NURBSCoordAdjuster->get_style_context()->add_class("curve-spinbuttonbox");

    // Button box position: 0=above, 1=right, 2=below, 3=left
    NURBSCurveGrid->add(*NURBSCurveBox);
    NURBSCurve->set_hexpand(true);

    if (options.curvebboxpos == 0) {
        NURBSCurveGrid->attach_next_to(*NURBSbbox, *NURBSCurveBox, Gtk::POS_TOP, 1, 1);
        NURBSCurveGrid->attach_next_to(*NURBSCoordAdjuster, *NURBSCurveBox,
                                       Gtk::POS_BOTTOM, 1, 1);
    } else if (options.curvebboxpos == 1) {
        NURBSCurveGrid->attach_next_to(*NURBSbbox, *NURBSCurveBox, Gtk::POS_RIGHT, 1, 1);
        NURBSCurveGrid->attach_next_to(*NURBSCoordAdjuster, *NURBSCurveBox,
                                       Gtk::POS_BOTTOM, 2, 1);
    } else if (options.curvebboxpos == 2) {
        NURBSCurveGrid->attach_next_to(*NURBSCoordAdjuster, *NURBSCurveBox,
                                       Gtk::POS_BOTTOM, 1, 1);
        NURBSCurveGrid->attach_next_to(*NURBSbbox, *NURBSCoordAdjuster, Gtk::POS_BOTTOM,
                                       1, 1);
    } else if (options.curvebboxpos == 3) {
        NURBSCurveGrid->attach_next_to(*NURBSbbox, *NURBSCurveBox, Gtk::POS_LEFT, 1, 1);
        NURBSCurveGrid->attach_next_to(*NURBSCoordAdjuster, *NURBSbbox, Gtk::POS_BOTTOM,
                                       2, 1);
    }

    NURBSCurveGrid->show_all();
    NURBSCoordAdjuster->hide();

    saveNURBS->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::savePressed));
    loadNURBS->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::loadPressed));
    pasteNURBS->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::pastePressed));
    copyNURBS->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::copyPressed));
    editPointNURBSConn = editPointNURBS->signal_toggled().connect(
        sigc::bind(sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::editPointToggled),
                   editPointNURBS));
    editNURBSConn = editNURBS->signal_toggled().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::editToggled), editNURBS));

    saveNURBS->set_tooltip_text(M("CURVEEDITOR_TOOLTIPSAVE"));
    loadNURBS->set_tooltip_text(M("CURVEEDITOR_TOOLTIPLOAD"));
    pasteNURBS->set_tooltip_text(M("CURVEEDITOR_TOOLTIPPASTE"));
    copyNURBS->set_tooltip_text(M("CURVEEDITOR_TOOLTIPCOPY"));
    // NURBS curve end

    // parametric curve
    paramCurveGrid = new Gtk::Grid();
    paramCurveGrid->set_orientation(Gtk::ORIENTATION_VERTICAL);
    paramCurveGrid->get_style_context()->add_class("curve-mainbox");

    paramCurve = Gtk::manage(new MyDiagonalCurve());
    paramCurve->setType(DCT_Parametric);

    Gtk::Grid* paramCurveBox = Gtk::manage(new Gtk::Grid());
    paramCurveBox->get_style_context()->add_class("curve-curvebox");
    paramCurveBox->add(*paramCurve);

    Gtk::Grid* parambbox = Gtk::manage(new Gtk::Grid());
    parambbox->get_style_context()->add_class("curve-buttonbox");

    if (options.curvebboxpos == 0) {
        parambbox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        setExpandAlignProperties(parambbox, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);
        paramCurveGrid->get_style_context()->add_class("top");
    } else if (options.curvebboxpos == 2) {
        parambbox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        setExpandAlignProperties(parambbox, true, false, Gtk::ALIGN_FILL,
                                 Gtk::ALIGN_CENTER);
        paramCurveGrid->get_style_context()->add_class("bottom");
    } else if (options.curvebboxpos == 1) {
        parambbox->set_orientation(Gtk::ORIENTATION_VERTICAL);
        setExpandAlignProperties(parambbox, false, true, Gtk::ALIGN_CENTER,
                                 Gtk::ALIGN_FILL);
        paramCurveGrid->get_style_context()->add_class("right");
    } else if (options.curvebboxpos == 3) {
        parambbox->set_orientation(Gtk::ORIENTATION_VERTICAL);
        setExpandAlignProperties(parambbox, false, true, Gtk::ALIGN_CENTER,
                                 Gtk::ALIGN_FILL);
        paramCurveGrid->get_style_context()->add_class("left");
    }

    shcSelector = Gtk::manage(new SHCSelector());
    shcSelector->set_name("CurveSHCSelector");
    paramCurveBox->attach_next_to(*shcSelector, *paramCurve, Gtk::POS_BOTTOM, 1, 1);

    editParam = Gtk::manage(new Gtk::ToggleButton());
    initButton(*editParam, Glib::ustring("crosshair-node-curve"), Gtk::ALIGN_START, false,
               "EDIT_PIPETTE_TOOLTIP");
    editParam->hide();
    copyParam = Gtk::manage(new Gtk::Button());
    initButton(*copyParam, Glib::ustring("copy"), Gtk::ALIGN_END, true);
    pasteParam = Gtk::manage(new Gtk::Button());
    initButton(*pasteParam, Glib::ustring("paste"), Gtk::ALIGN_END, false);
    loadParam = Gtk::manage(new Gtk::Button());
    initButton(*loadParam, Glib::ustring("folder-open"), Gtk::ALIGN_END, false);
    saveParam = Gtk::manage(new Gtk::Button());
    initButton(*saveParam, Glib::ustring("save"), Gtk::ALIGN_END, false);

    parambbox->attach_next_to(*editParam, sideStart, 1, 1);
    parambbox->attach_next_to(*copyParam, sideEnd, 1, 1);
    parambbox->attach_next_to(*pasteParam, sideEnd, 1, 1);
    parambbox->attach_next_to(*loadParam, sideEnd, 1, 1);
    parambbox->attach_next_to(*saveParam, sideEnd, 1, 1);

    saveParam->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::savePressed));
    loadParam->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::loadPressed));
    pasteParam->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::pastePressed));
    copyParam->signal_clicked().connect(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::copyPressed));
    editParamConn = editParam->signal_toggled().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::editToggled), editParam));

    saveParam->set_tooltip_text(M("CURVEEDITOR_TOOLTIPSAVE"));
    loadParam->set_tooltip_text(M("CURVEEDITOR_TOOLTIPLOAD"));
    pasteParam->set_tooltip_text(M("CURVEEDITOR_TOOLTIPPASTE"));
    copyParam->set_tooltip_text(M("CURVEEDITOR_TOOLTIPCOPY"));

    highlights = Gtk::manage(new Adjuster(M("CURVEEDITOR_HIGHLIGHTS"), -100, 100, 1, 0));
    lights = Gtk::manage(new Adjuster(M("CURVEEDITOR_LIGHTS"), -100, 100, 1, 0));
    darks = Gtk::manage(new Adjuster(M("CURVEEDITOR_DARKS"), -100, 100, 1, 0));
    shadows = Gtk::manage(new Adjuster(M("CURVEEDITOR_SHADOWS"), -100, 100, 1, 0));

    Gtk::EventBox* evhighlights = Gtk::manage(new Gtk::EventBox());
    Gtk::EventBox* evlights = Gtk::manage(new Gtk::EventBox());
    Gtk::EventBox* evdarks = Gtk::manage(new Gtk::EventBox());
    Gtk::EventBox* evshadows = Gtk::manage(new Gtk::EventBox());

    evhighlights->add(*highlights);
    evlights->add(*lights);
    evdarks->add(*darks);
    evshadows->add(*shadows);

    // paramCurveSliderBox needed to set vspacing(4) between curve+shc and sliders without vspacing between each slider
    Gtk::Grid* paramCurveSliderBox = Gtk::manage(new Gtk::Grid());
    paramCurveSliderBox->set_orientation(Gtk::ORIENTATION_VERTICAL);
    paramCurveSliderBox->get_style_context()->add_class("curve-sliderbox");

    paramCurveSliderBox->attach_next_to(*evhighlights, Gtk::POS_TOP, 1, 1);
    paramCurveSliderBox->attach_next_to(*evlights, Gtk::POS_TOP, 1, 1);
    paramCurveSliderBox->attach_next_to(*evdarks, Gtk::POS_TOP, 1, 1);
    paramCurveSliderBox->attach_next_to(*evshadows, Gtk::POS_TOP, 1, 1);

    paramCurveGrid->show_all();

    // Button box position: 0=above, 1=right, 2=below, 3=left
    paramCurveGrid->add(*paramCurveBox);
    paramCurve->set_hexpand(true);

    if (options.curvebboxpos == 0) {
        paramCurveGrid->attach_next_to(*parambbox, *paramCurveBox, Gtk::POS_TOP, 1, 1);
        paramCurveGrid->attach_next_to(*paramCurveSliderBox, *paramCurveBox,
                                       Gtk::POS_BOTTOM, 1, 1);
    } else if (options.curvebboxpos == 1) {
        paramCurveGrid->attach_next_to(*parambbox, *paramCurveBox, Gtk::POS_RIGHT, 1, 1);
        paramCurveGrid->attach_next_to(*paramCurveSliderBox, *paramCurveBox,
                                       Gtk::POS_BOTTOM, 2, 1);
    } else if (options.curvebboxpos == 2) {
        paramCurveGrid->attach_next_to(*parambbox, *paramCurveBox, Gtk::POS_BOTTOM, 1, 1);
        paramCurveGrid->attach_next_to(*paramCurveSliderBox, *parambbox, Gtk::POS_BOTTOM,
                                       1, 1);
    } else if (options.curvebboxpos == 3) {
        paramCurveGrid->attach_next_to(*parambbox, *paramCurveBox, Gtk::POS_LEFT, 1, 1);
        paramCurveGrid->attach_next_to(*paramCurveSliderBox, *parambbox, Gtk::POS_BOTTOM,
                                       2, 1);
    }

    paramCurveGrid->show_all();
    // parametric curve end

    customCurveGrid->reference();
    paramCurveGrid->reference();

    customCurve->setCurveListener(parent);  // Send the message directly to the parent
    NURBSCurve->setCurveListener(parent);
    paramCurve->setCurveListener(parent);
    shcSelector->setSHCListener(this);

    highlights->setAdjusterListener(this);
    lights->setAdjusterListener(this);
    darks->setAdjusterListener(this);
    shadows->setAdjusterListener(this);

    evhighlights->set_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
    evlights->set_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
    evdarks->set_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
    evshadows->set_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
    evhighlights->signal_enter_notify_event().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterEntered), 4));
    evlights->signal_enter_notify_event().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterEntered), 5));
    evdarks->signal_enter_notify_event().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterEntered), 6));
    evshadows->signal_enter_notify_event().connect(sigc::bind(
        sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterEntered), 7));
    evhighlights->signal_leave_notify_event().connect(
        sigc::bind(sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterLeft), 4));
    evlights->signal_leave_notify_event().connect(
        sigc::bind(sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterLeft), 5));
    evdarks->signal_leave_notify_event().connect(
        sigc::bind(sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterLeft), 6));
    evshadows->signal_leave_notify_event().connect(
        sigc::bind(sigc::mem_fun(*this, &DiagonalCurveEditorSubGroup::adjusterLeft), 7));
}

DiagonalCurveEditorSubGroup::~DiagonalCurveEditorSubGroup()
{
    delete customCurveGrid;
    delete paramCurveGrid;
    delete NURBSCurveGrid;
}

/*
 * Add a new curve to the curves list
 */
DiagonalCurveEditor* DiagonalCurveEditorSubGroup::addCurve(Glib::ustring curveLabel)
{
    DiagonalCurveEditor* newCE = new DiagonalCurveEditor(curveLabel, parent, this);

    // Initialization of the new curve
    storeCurveValues(newCE, getCurveFromGUI(DCT_Spline));
    storeCurveValues(newCE, getCurveFromGUI(DCT_Parametric));
    storeCurveValues(newCE, getCurveFromGUI(DCT_NURBS));
    storeCurveValues(newCE, getCurveFromGUI(DCT_CatumullRom));
    return newCE;
}

void DiagonalCurveEditorSubGroup::updateLocallabBackground(CurveEditor* ce)
{
    if (ce == parent->displayedCurve) {
        paramCurve->updateLocallabBackground(ce->locallabRef);
        customCurve->updateLocallabBackground(ce->locallabRef);
        NURBSCurve->updateLocallabBackground(ce->locallabRef);
    }
}

void DiagonalCurveEditorSubGroup::restoreLocallabBackground()
{
    if (parent->displayedCurve) {
        paramCurve->updateLocallabBackground(parent->displayedCurve->locallabRef);
        customCurve->updateLocallabBackground(parent->displayedCurve->locallabRef);
        NURBSCurve->updateLocallabBackground(parent->displayedCurve->locallabRef);
    }
}

/*
 * Switch off the edit button
 */
void DiagonalCurveEditorSubGroup::editModeSwitchedOff()
{
    // toggling off all edit buttons, even if only one is toggle on
    bool prevState;
    prevState = editCustomConn.block(true);
    editCustom->set_active(false);
    customCurve->pipetteMouseOver(nullptr, nullptr, 0);

    if (!prevState) {
        editCustomConn.block(false);
    }

    prevState = editNURBSConn.block(true);
    editNURBS->set_active(false);
    NURBSCurve->pipetteMouseOver(nullptr, nullptr, 0);

    if (!prevState) {
        editNURBSConn.block(false);
    }

    prevState = editParamConn.block(true);
    editParam->set_active(false);
    paramCurve->pipetteMouseOver(nullptr, nullptr, 0);

    if (!prevState) {
        editParamConn.block(false);
    }
}

void DiagonalCurveEditorSubGroup::pipetteMouseOver(EditDataProvider* provider,
                                                   int modifierKey)
{
    CurveEditor* curveEditor = static_cast<DiagonalCurveEditor*>(parent->displayedCurve);

    switch ((DiagonalCurveType)(curveEditor->curveType->getSelected())) {
        case (DCT_Spline):
        case (DCT_CatumullRom):
            customCurve->pipetteMouseOver(curveEditor, provider, modifierKey);
            break;

        case (DCT_Parametric):
        {
            paramCurve->pipetteMouseOver(curveEditor, provider, modifierKey);
            float pipetteVal = 0.f;
            editedAdjuster = nullptr;
            int n = 0;

            if (provider->getPipetteVal1() != -1.f) {
                pipetteVal += provider->getPipetteVal1();
                ++n;
            }

            if (provider->getPipetteVal2() != -1.f) {
                pipetteVal += provider->getPipetteVal2();
                ++n;
            }

            if (provider->getPipetteVal3() != -1.f) {
                pipetteVal += provider->getPipetteVal3();
                ++n;
            }

            if (n > 1) {
                pipetteVal /= n;
            } else if (!n) {
                pipetteVal = -1.f;
            }

            if (pipetteVal != -1.f) {
                double pos[3];
                shcSelector->getPositions(pos[0], pos[1], pos[2]);

                if (static_cast<double>(pipetteVal) >= pos[2]) {
                    editedAdjuster = highlights;
                    paramCurve->setActiveParam(4);
                } else if (static_cast<double>(pipetteVal) >= pos[1]) {
                    editedAdjuster = lights;
                    paramCurve->setActiveParam(5);
                } else if (static_cast<double>(pipetteVal) >= pos[0]) {
                    editedAdjuster = darks;
                    paramCurve->setActiveParam(6);
                } else {
                    editedAdjuster = shadows;
                    paramCurve->setActiveParam(7);
                }
            } else {
                paramCurve->setActiveParam(-1);
            }
        } break;

        case (DCT_NURBS):
            NURBSCurve->pipetteMouseOver(curveEditor, provider, modifierKey);
            break;

        default:  // (DCT_Linear, DCT_Unchanged)
            // ... do nothing
            break;
    }
}

bool DiagonalCurveEditorSubGroup::pipetteButton1Pressed(EditDataProvider* provider,
                                                        int modifierKey)
{
    CurveEditor* curveEditor = static_cast<DiagonalCurveEditor*>(parent->displayedCurve);

    bool isDragging = false;

    switch ((DiagonalCurveType)(curveEditor->curveType->getSelected())) {
        case (DCT_Spline):
        case (DCT_CatumullRom):
            isDragging = customCurve->pipetteButton1Pressed(provider, modifierKey);
            break;

        case (DCT_Parametric):
            if (editedAdjuster) {
                editedAdjusterValue = editedAdjuster->getIntValue();
            }

            break;

        case (DCT_NURBS):
            isDragging = NURBSCurve->pipetteButton1Pressed(provider, modifierKey);
            break;

        default:  // (DCT_Linear, DCT_Unchanged)
            // ... do nothing
            break;
    }

    return isDragging;
}

void DiagonalCurveEditorSubGroup::pipetteButton1Released(EditDataProvider* provider)
{
    CurveEditor* curveEditor = static_cast<DiagonalCurveEditor*>(parent->displayedCurve);

    switch ((DiagonalCurveType)(curveEditor->curveType->getSelected())) {
        case (DCT_Spline):
        case (DCT_CatumullRom):
            customCurve->pipetteButton1Released(provider);
            break;

        case (DCT_Parametric):
            editedAdjuster = nullptr;
            break;

        case (DCT_NURBS):
            NURBSCurve->pipetteButton1Released(provider);
            break;

        default:  // (DCT_Linear, DCT_Unchanged)
            // ... do nothing
            break;
    }
}

void DiagonalCurveEditorSubGroup::pipetteDrag(EditDataProvider* provider, int modifierKey)
{
    CurveEditor* curveEditor = static_cast<DiagonalCurveEditor*>(parent->displayedCurve);

    switch ((DiagonalCurveType)(curveEditor->curveType->getSelected())) {
        case (DCT_Spline):
        case (DCT_CatumullRom):
            customCurve->pipetteDrag(provider, modifierKey);
            break;

        case (DCT_Parametric):
            if (editedAdjuster) {
                int trimmedValue = editedAdjusterValue - (provider->deltaScreen.y / 2);
                editedAdjuster->trimValue(trimmedValue);

                if (trimmedValue != editedAdjuster->getIntValue()) {
                    editedAdjuster->setValue(trimmedValue);
                    adjusterChanged(editedAdjuster, trimmedValue);
                }
            }

            break;

        case (DCT_NURBS):
            NURBSCurve->pipetteDrag(provider, modifierKey);
            break;

        default:  // (DCT_Linear, DCT_Unchanged)
            // ... do nothing
            break;
    }
}

void DiagonalCurveEditorSubGroup::showCoordinateAdjuster(CoordinateProvider* provider)
{
    if (provider == customCurve) {
        if (!editPointCustom->get_active()) {
            editPointCustom->set_active(true);
        }
    } else if (provider == NURBSCurve) {
        if (!editPointNURBS->get_active()) {
            editPointNURBS->set_active(true);
        }
    }
}

void DiagonalCurveEditorSubGroup::stopNumericalAdjustment()
{
    customCurve->stopNumericalAdjustment();
    NURBSCurve->stopNumericalAdjustment();
}

/*
 * Force the resize of the curve editor, if the displayed one is the requested one
 */
void DiagonalCurveEditorSubGroup::refresh(CurveEditor* curveToRefresh)
{
    if (curveToRefresh != nullptr
        && curveToRefresh == static_cast<DiagonalCurveEditor*>(parent->displayedCurve))
    {
        switch ((DiagonalCurveType)(curveToRefresh->curveType->getSelected())) {
            case (DCT_Spline):
            case (DCT_CatumullRom):
                customCurve->refresh();
                break;

            case (DCT_Parametric):
                paramCurve->refresh();
                shcSelector->refresh();
                break;

            case (DCT_NURBS):
                NURBSCurve->refresh();
                break;

            default:  // (DCT_Linear, DCT_Unchanged)
                // ... do nothing
                break;
        }
    }
}

/*
 * Switch the editor widgets to the currently edited curve
 */
void DiagonalCurveEditorSubGroup::switchGUI()
{

    removeEditor();

    DiagonalCurveEditor* dCurve =
        static_cast<DiagonalCurveEditor*>(parent->displayedCurve);

    if (dCurve) {

        // Initializing GUI values + attaching the appropriated widget
        //dCurve->typeconn.block(true);

        // first we update the colored bar

        ColorProvider* barColorProvider = dCurve->getLeftBarColorProvider();
        std::vector<GradientMilestone> bgGradient = dCurve->getLeftBarBgGradient();

        if (barColorProvider == nullptr && bgGradient.size() == 0) {
            // dCurve has no left colored bar, so we delete the object
            if (leftBar) {
                delete leftBar;
                leftBar = nullptr;
            }
        } else {
            // dCurve ave a ColorProvider or a background gradient defined, so we create/update the object
            if (!leftBar) {
                leftBar = new ColoredBar(RTO_Bottom2Top);
            }

            if (barColorProvider) {
                bgGradient.clear();
                leftBar->setColorProvider(barColorProvider, dCurve->getLeftBarCallerId());
                leftBar->setBgGradient(bgGradient);
            } else {
                leftBar->setColorProvider(nullptr, -1);
                leftBar->setBgGradient(bgGradient);
            }
        }

        barColorProvider = dCurve->getBottomBarColorProvider();
        bgGradient = dCurve->getBottomBarBgGradient();

        if (barColorProvider == nullptr && bgGradient.size() == 0) {
            // dCurve has no bottom colored bar, so we delete the object
            if (bottomBar) {
                delete bottomBar;
                bottomBar = nullptr;
            }
        } else {
            // dCurve has a ColorProvider or a background gradient defined, so we create/update the object
            if (!bottomBar) {
                bottomBar = new ColoredBar(RTO_Left2Right);
            }

            if (barColorProvider) {
                bgGradient.clear();
                bottomBar->setColorProvider(barColorProvider,
                                            dCurve->getBottomBarCallerId());
                bottomBar->setBgGradient(bgGradient);
            } else {
                bottomBar->setColorProvider(nullptr, -1);
                bottomBar->setBgGradient(bgGradient);
            }
        }

        switch (auto tp = (DiagonalCurveType)(dCurve->curveType->getSelected())) {
            case (DCT_Spline):
            case (DCT_CatumullRom):
                customCurve->setPoints(tp == DCT_Spline ? dCurve->customCurveEd
                                                        : dCurve->catmullRomCurveEd);
                customCurve->setColorProvider(dCurve->getCurveColorProvider(),
                                              dCurve->getCurveCallerId());
                customCurve->setColoredBar(leftBar, bottomBar);
                customCurve->queue_resize_no_redraw();
                updateEditButton(dCurve, editCustom, editCustomConn);
                parent->attachCurve(customCurveGrid);
                customCurveGrid->check_resize();
                break;

            case (DCT_Parametric):
            {
                Glib::ustring label[4];
                dCurve->getRangeLabels(label[0], label[1], label[2], label[3]);
                double mileStone[3];
                dCurve->getRangeDefaultMilestones(mileStone[0], mileStone[1],
                                                  mileStone[2]);
                paramCurve->setPoints(dCurve->paramCurveEd);
                shcSelector->setDefaults(mileStone[0], mileStone[1], mileStone[2]);
                shcSelector->setPositions(dCurve->paramCurveEd.at(1),
                                          dCurve->paramCurveEd.at(2),
                                          dCurve->paramCurveEd.at(3));

                highlights->setValue(dCurve->paramCurveEd.at(4));
                highlights->setLabel(label[3]);
                lights->setValue(dCurve->paramCurveEd.at(5));
                lights->setLabel(label[2]);
                darks->setValue(dCurve->paramCurveEd.at(6));
                darks->setLabel(label[1]);
                shadows->setValue(dCurve->paramCurveEd.at(7));
                shadows->setLabel(label[0]);
                shcSelector->coloredBar.setColorProvider(barColorProvider,
                                                         dCurve->getBottomBarCallerId());
                shcSelector->coloredBar.setBgGradient(bgGradient);
                shcSelector->setMargins(
                    (int)((leftBar
                               ? (double)CBAR_WIDTH + 2. + (double)CBAR_MARGIN + RADIUS
                               : RADIUS)
                          - 1.5),
                    (int)(RADIUS - 1.5));
                paramCurve->setColoredBar(leftBar, nullptr);
                paramCurve->queue_resize_no_redraw();
                updateEditButton(dCurve, editParam, editParamConn);
                parent->attachCurve(paramCurveGrid);
                break;
            }

            case (DCT_NURBS):
                NURBSCurve->setPoints(dCurve->NURBSCurveEd);
                NURBSCurve->setColorProvider(dCurve->getCurveColorProvider(),
                                             dCurve->getCurveCallerId());
                NURBSCurve->setColoredBar(leftBar, bottomBar);
                NURBSCurve->queue_resize_no_redraw();
                updateEditButton(dCurve, editNURBS, editNURBSConn);
                parent->attachCurve(NURBSCurveGrid);
                NURBSCurveGrid->check_resize();
                break;

            default:  // (DCT_Linear, DCT_Unchanged)
                // ... do nothing
                break;
        }

        //dCurve->typeconn.block(false);
    }
}

void DiagonalCurveEditorSubGroup::savePressed()
{

    Glib::ustring fname = outputFile();

    if (fname.size()) {
        std::ofstream f(fname.c_str());
        std::vector<double> p;
        //std::vector<double> p = customCurve->getPoints ();

        switch (parent->displayedCurve->selected) {
            case DCT_Spline:  // custom
            case DCT_CatumullRom:
                p = customCurve->getPoints();
                break;

            case DCT_NURBS:  // NURBS
                p = NURBSCurve->getPoints();
                break;

            case DCT_Parametric:
                p = paramCurve->getPoints();
                break;

            default:
                break;
        }

        int ix = 0;

        if (p[ix] == (double)(DCT_Linear)) {
            f << "Linear" << std::endl;
        } else if (p[ix] == (double)(DCT_Spline)) {
            f << "Spline" << std::endl;
        } else if (p[ix] == (double)(DCT_CatumullRom)) {
            f << "CatmullRom" << std::endl;
        } else if (p[ix] == (double)(DCT_NURBS)) {
            f << "NURBS" << std::endl;
        } else if (p[ix] == (double)(DCT_Parametric)) {
            f << "Parametric" << std::endl;
        }

        if (p[ix] == (double)(DCT_Parametric)) {
            ix++;

            for (unsigned int i = 0; i < p.size() - 1; i++, ix++) {
                f << p[ix] << std::endl;
            }
        } else {
            ix++;

            for (unsigned int i = 0; i < p.size() / 2; i++, ix += 2) {
                f << p[ix] << ' ' << p[ix + 1] << std::endl;
            }
        }

        f.close();
    }
}

void DiagonalCurveEditorSubGroup::loadPressed()
{

    Glib::ustring fname = inputFile();

    if (fname.size()) {
        std::ifstream f(fname.c_str());

        if (f) {
            std::vector<double> p;
            std::string s;
            f >> s;

            if (s == "Linear") {
                p.push_back((double)(DCT_Linear));
            } else if (s == "Spline") {
                p.push_back((double)(DCT_Spline));
            } else if (s == "CatmullRom") {
                p.push_back((double)(DCT_CatumullRom));
            } else if (s == "NURBS") {
                p.push_back((double)(DCT_NURBS));
            } else if (s == "Parametric") {
                p.push_back((double)(DCT_Parametric));
            } else {
                return;
            }

            double x;

            while (f) {
                f >> x;

                if (f) {
                    p.push_back(x);
                }
            }

            rtengine::sanitizeCurve(p);

            if (p[0] == (double)(DCT_Spline) || p[0] == (double)(DCT_CatumullRom)) {
                customCurve->setPoints(p);
                customCurve->notifyListener();
            } else if (p[0] == (double)(DCT_NURBS)) {
                NURBSCurve->setPoints(p);
                NURBSCurve->notifyListener();
            } else if (p[0] == (double)(DCT_Parametric)) {
                shcSelector->setPositions(p[1], p[2], p[3]);
                highlights->setValue(p[4]);
                lights->setValue(p[5]);
                darks->setValue(p[6]);
                shadows->setValue(p[7]);
                paramCurve->setPoints(p);
                paramCurve->notifyListener();
            }
        }
    }
}

void DiagonalCurveEditorSubGroup::copyPressed()
{
    // For compatibility use enum DiagonalCurveType here

    std::vector<double> curve;

    switch (parent->displayedCurve->selected) {
        case DCT_Spline:  // custom
            curve = customCurve->getPoints();
            clipboard.setDiagonalCurveData(curve, DCT_Spline);
            break;

        case DCT_Parametric:  // parametric
            // ... do something, first add save/load functions
            curve = paramCurve->getPoints();
            clipboard.setDiagonalCurveData(curve, DCT_Parametric);
            break;

        case DCT_NURBS:  // NURBS
            curve = NURBSCurve->getPoints();
            clipboard.setDiagonalCurveData(curve, DCT_NURBS);
            break;

        case DCT_CatumullRom:
            curve = customCurve->getPoints();
            curve[0] = DCT_CatumullRom;
            clipboard.setDiagonalCurveData(curve, DCT_CatumullRom);
            break;

        default:  // (DCT_Linear, DCT_Unchanged)
            // ... do nothing
            break;
    }
}

void DiagonalCurveEditorSubGroup::pastePressed()
{
    // For compatibility use enum DiagonalCurveType here

    std::vector<double> curve;
    DiagonalCurveType type;

    type = clipboard.hasDiagonalCurveData();

    if (type == (DiagonalCurveType)parent->displayedCurve->selected) {
        curve = clipboard.getDiagonalCurveData();

        switch (type) {
            case DCT_Spline:  // custom
            case DCT_CatumullRom:
                customCurve->setPoints(curve);
                customCurve->notifyListener();
                break;

            case DCT_Parametric:  // parametric
                // ... do something, first add save/load functions
                shcSelector->setPositions(curve[1], curve[2], curve[3]);
                highlights->setValue(curve[4]);
                lights->setValue(curve[5]);
                darks->setValue(curve[6]);
                shadows->setValue(curve[7]);
                paramCurve->setPoints(curve);
                paramCurve->notifyListener();
                break;

            case DCT_NURBS:  // NURBS
                NURBSCurve->setPoints(curve);
                NURBSCurve->notifyListener();
                break;

            default:  // (DCT_Linear, DCT_Unchanged)
                // ... do nothing
                break;
        }
    }

    return;
}

void DiagonalCurveEditorSubGroup::editPointToggled(Gtk::ToggleButton* button)
{
    if (button->get_active()) {
        customCoordAdjuster->show();
        NURBSCoordAdjuster->show();
    } else {
        if (customCoordAdjuster) {
            customCurve->stopNumericalAdjustment();
            customCoordAdjuster->hide();
            NURBSCurve->stopNumericalAdjustment();
            NURBSCoordAdjuster->hide();
        }
    }

    if (button == editPointCustom) {
        editPointNURBSConn.block(true);
        editPointNURBS->set_active(!editPointNURBS->get_active());
        editPointNURBSConn.block(false);
    } else {
        editPointCustomConn.block(true);
        editPointCustom->set_active(!editPointCustom->get_active());
        editPointCustomConn.block(false);
    }
}

void DiagonalCurveEditorSubGroup::editToggled(Gtk::ToggleButton* button)
{
    DiagonalCurveEditor* dCurve =
        static_cast<DiagonalCurveEditor*>(parent->displayedCurve);

    if (!dCurve)
    // should never happen!
    {
        return;
    }

    if (button->get_active()) {
        dCurve->subscribe();

        if (button == editCustom) {
            customCurve->notifyListener();
        } else if (button == editNURBS) {
            NURBSCurve->notifyListener();
        } else if (button == editParam) {
            paramCurve->notifyListener();
        }

        /*
        if (button != editCustom) {
            editCustomConn.block(true);
            editCustom->set_active(true);
            editCustomConn.block(false);
        }
        else {
            // will throw the event of curveChanged, which will now build the edit's buffer
            customCurve->notifyListener ();
        }
        if (button != editNURBS) {
            editNURBSConn.block(true);
            editNURBS->set_active(true);
            editNURBSConn.block(false);
        }
        else {
            NURBSCurve->notifyListener ();
        }
        if (button != editParam) {
            editParamConn.block(true);
            editParam->set_active(true);
            editParamConn.block(false);
        }
        else {
            paramCurve->notifyListener ();
        }
        */
    } else {
        dCurve->unsubscribe();
        /*
        if (button != editCustom ) { editCustomConn.block(true);  editCustom->set_active(false);  editCustomConn.block(false); }
        if (button != editNURBS  ) { editNURBSConn.block(true);   editNURBS->set_active(false);   editNURBSConn.block(false);  }
        if (button != editParam  ) { editParamConn.block(true);   editParam->set_active(false);   editParamConn.block(false);  }
        */
    }
}

/*
 * Store the curves of the currently displayed type from the widgets to the CurveEditor object
 */
void DiagonalCurveEditorSubGroup::storeDisplayedCurve()
{
    if (parent->displayedCurve) {
        switch (parent->displayedCurve->selected) {
            case (DCT_Spline):
                storeCurveValues(parent->displayedCurve, getCurveFromGUI(DCT_Spline));
                break;

            case (DCT_Parametric):
                storeCurveValues(parent->displayedCurve, getCurveFromGUI(DCT_Parametric));
                break;

            case (DCT_NURBS):
                storeCurveValues(parent->displayedCurve, getCurveFromGUI(DCT_NURBS));
                break;

            case (DCT_CatumullRom):
                storeCurveValues(parent->displayedCurve,
                                 getCurveFromGUI(DCT_CatumullRom));
                break;

            default:
                break;
        }
    }
}

/*
 * Restore the histogram to all types from the CurveEditor object to the widgets
 */
void DiagonalCurveEditorSubGroup::restoreDisplayedHistogram()
{
    if (parent->displayedCurve /*&& initslope==1*/) {
        paramCurve->updateBackgroundHistogram(parent->displayedCurve->histogram);
        customCurve->updateBackgroundHistogram(parent->displayedCurve->histogram);
        NURBSCurve->updateBackgroundHistogram(parent->displayedCurve->histogram);
    }
}

void DiagonalCurveEditorSubGroup::storeCurveValues(CurveEditor* ce,
                                                   const std::vector<double>& p)
{
    if (!p.empty()) {
        DiagonalCurveType t = (DiagonalCurveType)p[0];

        switch (t) {
            case (DCT_Spline):
                (static_cast<DiagonalCurveEditor*>(ce))->customCurveEd = p;
                break;

            case (DCT_Parametric):
                (static_cast<DiagonalCurveEditor*>(ce))->paramCurveEd = p;
                break;

            case (DCT_NURBS):
                (static_cast<DiagonalCurveEditor*>(ce))->NURBSCurveEd = p;
                break;

            case (DCT_CatumullRom):
                (static_cast<DiagonalCurveEditor*>(ce))->catmullRomCurveEd = p;
                break;

            default:
                break;
        }
    }
}

/*
 * Called to update the parametric curve graph with new slider values
 */
const std::vector<double> DiagonalCurveEditorSubGroup::getCurveFromGUI(int type)
{
    switch ((DiagonalCurveType)type) {
        case (DCT_Parametric):
        {
            std::vector<double> lcurve(8);
            lcurve[0] = (double)(DCT_Parametric);
            shcSelector->getPositions(lcurve[1], lcurve[2], lcurve[3]);
            lcurve[4] = highlights->getValue();
            lcurve[5] = lights->getValue();
            lcurve[6] = darks->getValue();
            lcurve[7] = shadows->getValue();
            return lcurve;
        }

        case (DCT_Spline):
            return customCurve->getPoints();

        case (DCT_NURBS):
            return NURBSCurve->getPoints();

        case (DCT_CatumullRom):
        {
            auto ret = customCurve->getPoints();
            if (!ret.empty()) {
                ret[0] = DCT_CatumullRom;
            }
            return ret;
        }

        default:
        {
            // linear and other solutions
            std::vector<double> lcurve(1);
            lcurve[0] = (double)(DCT_Linear);
            return lcurve;
        }
    }
}

/*
 * Unlink the tree editor widgets from their parent box to hide them
 */
void DiagonalCurveEditorSubGroup::removeEditor()
{
    removeIfThere(parent, customCurveGrid, false);
    removeIfThere(parent, paramCurveGrid, false);
    removeIfThere(parent, NURBSCurveGrid, false);
}

bool DiagonalCurveEditorSubGroup::curveReset(CurveEditor* ce)
{
    if (!ce) {
        return false;
    }

    DiagonalCurveEditor* dce = static_cast<DiagonalCurveEditor*>(ce);

    switch (ce->selected) {
        case (DCT_NURBS):  // = Control cage
            NURBSCurve->reset(dce->NURBSResetCurve, dce->getIdentityValue());
            return true;

        case (DCT_Spline):  // = Custom
            customCurve->reset(dce->customResetCurve, dce->getIdentityValue());
            return true;

        case (DCT_CatumullRom):
            customCurve->reset(dce->catmullRomResetCurve, dce->getIdentityValue());
            return true;

        case (DCT_Parametric):
        {
            DiagonalCurveEditor* dCurve =
                static_cast<DiagonalCurveEditor*>(parent->displayedCurve);
            double mileStone[3];
            dCurve->getRangeDefaultMilestones(mileStone[0], mileStone[1], mileStone[2]);

            highlights->resetPressed(nullptr);
            lights->resetPressed(nullptr);
            darks->resetPressed(nullptr);
            shadows->resetPressed(nullptr);
            shcSelector->setDefaults(mileStone[0], mileStone[1], mileStone[2]);
            shcSelector->reset();
            paramCurve->reset(dce->paramResetCurve, dce->getIdentityValue());
            return true;
        }

        default:
            return false;
    }

    return true;
}

/*
 * Listener
 */
void DiagonalCurveEditorSubGroup::shcChanged()
{

    paramCurve->setPoints(getCurveFromGUI(DCT_Parametric));
    storeDisplayedCurve();
    parent->curveChanged();
}

/*
 * Listener
 */
void DiagonalCurveEditorSubGroup::adjusterChanged(Adjuster* a, double newval)
{
    paramCurve->setPoints(getCurveFromGUI(DCT_Parametric));
    storeDisplayedCurve();
    parent->curveChanged();
}

/*
 * Listener called when the mouse is over a parametric curve's slider
 */
bool DiagonalCurveEditorSubGroup::adjusterEntered(GdkEventCrossing* ev, int ac)
{

    if (ev->detail != GDK_NOTIFY_INFERIOR) {
        activeParamControl = ac;
        paramCurve->setActiveParam(activeParamControl);
    }

    return true;
}

/*
 * Listener called when the mouse left the parametric curve's slider
 */
bool DiagonalCurveEditorSubGroup::adjusterLeft(GdkEventCrossing* ev, int ac)
{

    if (ev->detail != GDK_NOTIFY_INFERIOR) {
        activeParamControl = -1;
        paramCurve->setActiveParam(activeParamControl);
    }

    return true;
}

void DiagonalCurveEditorSubGroup::updateBackgroundHistogram(CurveEditor* ce)
{
    if (ce == parent->displayedCurve /*&&  initslope==1*/) {
        paramCurve->updateBackgroundHistogram(ce->histogram);
        customCurve->updateBackgroundHistogram(ce->histogram);
        NURBSCurve->updateBackgroundHistogram(ce->histogram);
    }
}

void DiagonalCurveEditorSubGroup::setSubGroupRangeLabels(Glib::ustring r1,
                                                         Glib::ustring r2,
                                                         Glib::ustring r3,
                                                         Glib::ustring r4)
{
    shadows->setLabel(r1);
    darks->setLabel(r2);
    lights->setLabel(r3);
    highlights->setLabel(r4);
}
