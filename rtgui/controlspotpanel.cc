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
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 *  2018 Pierre Cabrera <pierre.cab@gmail.com>
 */

#include "controlspotpanel.h"
#include "editwidgets.h"
#include "eventmapper.h"
#include "options.h"
#include "rtimage.h"
#include "widgets/basic/myexpander.h"

#include "rtengine/procparams.h"
#include "rtengine/rt_math.h"

using namespace rtengine;
using namespace procparams;

//-----------------------------------------------------------------------------
// ControlSpotPanel
//-----------------------------------------------------------------------------

ControlSpotPanel::ControlSpotPanel():
    EditSubscriber(ET_OBJECTS),
    FoldableToolPanel(this, "controlspotpanel", M("TP_LOCALLAB_SETTINGS")),

    scrolledwindow_(Gtk::manage(new Gtk::ScrolledWindow())),
    treeview_(Gtk::manage(new Gtk::TreeView())),

    button_add_(Gtk::manage(new Gtk::Button(M("TP_LOCALLAB_BUTTON_ADD")))),
    button_delete_(Gtk::manage(new Gtk::Button(M("TP_LOCALLAB_BUTTON_DEL")))),
    button_duplicate_(Gtk::manage(new Gtk::Button(M("TP_LOCALLAB_BUTTON_DUPL")))),

    button_rename_(Gtk::manage(new Gtk::Button(M("TP_LOCALLAB_BUTTON_REN")))),
    button_visibility_(Gtk::manage(new Gtk::Button(M("TP_LOCALLAB_BUTTON_VIS")))),

    prevMethod_(Gtk::manage(new MyComboBoxText())),
    shape_(Gtk::manage(new MyComboBoxText())),
    spotMethod_(Gtk::manage(new MyComboBoxText())),
    shapeMethod_(Gtk::manage(new MyComboBoxText())),
    qualityMethod_(Gtk::manage(new MyComboBoxText())),
    //complexMethod_(Gtk::manage(new MyComboBoxText())),
    wavMethod_(Gtk::manage(new MyComboBoxText())),
    avoidgamutMethod_(Gtk::manage(new MyComboBoxText())),

    sensiexclu_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSIEXCLU"), 0, 100, 1, 12))),
    structexclu_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRUCCOL"), 0, 100, 1, 0))),
    locX_(Gtk::manage(new Adjuster(M("TP_LOCAL_WIDTH"), 2, 3000, 1, 150))),
    locXL_(Gtk::manage(new Adjuster(M("TP_LOCAL_WIDTH_L"), 2, 3000, 1, 150))),
    locY_(Gtk::manage(new Adjuster(M("TP_LOCAL_HEIGHT"), 2, 3000, 1, 150))),
    locYT_(Gtk::manage(new Adjuster(M("TP_LOCAL_HEIGHT_T"), 2, 3000, 1, 150))),
    centerX_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CENTER_X"), -1000, 1000, 1, 0))),
    centerY_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CENTER_Y"), -1000, 1000, 1, 0))),
    circrad_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CIRCRADIUS"), 1.5, 150., 0.5, 18.))),
    transit_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_TRANSITVALUE"), 2., 100., 0.1, 60.))),
    transitweak_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_TRANSITWEAK"), 0.5, 25.0, 0.1, 1.0))),
    transitgrad_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_TRANSITGRAD"), -1.0, 1.0, 0.01, 0.0))),
    feather_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FEATVALUE_MASK"), 10., 100., 0.1, 25.))),
    struc_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_THRES"), 1.0, 12.0, 0.1, 4.0))),
    thresh_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_THRESDELTAE"), 0.0, 15.0, 0.1, 2.0))),
    iter_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_PROXI"), 0.2, 10.0, 0.1, 2.0))),
   // balan_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BALAN"), 0.05, 2.5, 0.05, 1.0, Gtk::manage(new RTImage("rawtherapee-logo-16")),  Gtk::manage(new RTImage("circle-white-small"))))),
    balan_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BALAN"), 0.05, 2.5, 0.05, 1.0, Gtk::manage(new RTImage("circle-yellow-small")),  Gtk::manage(new RTImage("circle-white-small"))))),
    balanh_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BALANH"), 0.2, 2.5, 0.1, 1.0, Gtk::manage(new RTImage("circle-multicolor-small")), Gtk::manage(new RTImage("circle-red-green-small"))))),
    colorde_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_COLORDE"), -15, 15, 2, 5, Gtk::manage(new RTImage("circle-blue-yellow-small")), Gtk::manage(new RTImage("circle-gray-green-small"))))),
    colorscope_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_COLORSCOPE"), 0., 100.0, 1., 30.))),
    avoidrad_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_AVOIDRAD"), 0., 30.0, 0.1, 0.))),
    scopemask_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SCOPEMASK"), 0, 100, 1, 60))),
    denoichmask_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_DENOIMASK"), 0., 100., 0.5, 0))),
    lumask_(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LUMASK"), -50, 30, 1, 10, Gtk::manage(new RTImage("circle-yellow-small")), Gtk::manage(new RTImage("circle-gray-small")) ))),

    hishow_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_PREVSHOW")))),
    activ_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ACTIVSPOT")))),
    avoidneg_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_AVOIDNEG")))),
    blwh_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_BLWH")))),
    recurs_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_RECURS")))),
    laplac_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_LAPLACC")))),
    deltae_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_DELTAEC")))),
    shortc_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_SHORTC")))),
    //savrest_(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_SAVREST")))),

    expTransGrad_(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_TRANSIT")))),
    expShapeDetect_(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_ARTIF")))),
    expSpecCases_(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SPECCASE")))),
    expMaskMerge_(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_MASFRAME")))),

    preview_(Gtk::manage(new Gtk::ToggleButton(M("TP_LOCALLAB_PREVIEW")))),
    ctboxshape(Gtk::manage(new Gtk::Box())),
    ctboxactivmethod(Gtk::manage(new Gtk::Box())),
    ctboxspotmethod(Gtk::manage(new Gtk::Box())),    
    ctboxshapemethod(Gtk::manage(new Gtk::Box())),
    ctboxgamut(Gtk::manage(new Gtk::Box())),
    artifBox2(Gtk::manage(new ToolParamBlock())),

    controlPanelListener(nullptr),
    lastObject_(-1),
    nbSpotChanged_(false),
    selSpotChanged_(false),
    nameChanged_(false),
    visibilityChanged_(false),
    eventType(None),
    excluFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_EXCLUF")))),
    maskPrevActive(false)
{
    auto m = ProcEventMapper::getInstance();
    EvLocallabavoidgamutMethod = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GAMUTMUNSEL");
    EvLocallabavoidnegative =  m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_AVOIDNEGATIVE");
    const bool showtooltip = App::get().options().showtooltip;

//    pack_start(*hishow_);

    Gtk::Box* const ctboxprevmethod = Gtk::manage(new Gtk::Box());
    prevMethod_->append(M("TP_LOCALLAB_PREVHIDE"));
    prevMethod_->append(M("TP_LOCALLAB_PREVSHOW"));
    prevMethod_->set_active(0);
    prevMethodconn_ = prevMethod_->signal_changed().connect(
                          sigc::mem_fun(
                              *this, &ControlSpotPanel::prevMethodChanged));

//    ctboxprevmethod->pack_start(*prevMethod_);
    pack_start(*ctboxprevmethod);


    Gtk::Box* const hbox1_ = Gtk::manage(new Gtk::Box());
    hbox1_->set_spacing(4);
    hbox1_->set_homogeneous(true);
    buttonaddconn_ = button_add_->signal_clicked().connect(
                         sigc::mem_fun(*this, &ControlSpotPanel::on_button_add));
    buttondeleteconn_ = button_delete_->signal_clicked().connect(
                            sigc::mem_fun(*this, &ControlSpotPanel::on_button_delete));
    buttonduplicateconn_ = button_duplicate_->signal_clicked().connect(
                               sigc::mem_fun(*this, &ControlSpotPanel::on_button_duplicate));

    hbox1_->pack_start(*button_add_);
    hbox1_->pack_start(*button_delete_);
    hbox1_->pack_start(*button_duplicate_);
    pack_start(*hbox1_);

    Gtk::Box* const hbox2_ = Gtk::manage(new Gtk::Box());
    hbox2_->set_spacing(4);
    hbox2_->set_homogeneous(true);
    buttonrenameconn_ = button_rename_->signal_clicked().connect(
                            sigc::mem_fun(*this, &ControlSpotPanel::on_button_rename));
    buttonvisibilityconn_ = button_visibility_->signal_button_release_event().connect(
                                sigc::mem_fun(*this, &ControlSpotPanel::on_button_visibility));


    if (showtooltip) {
        button_visibility_->set_tooltip_markup(M("TP_LOCALLAB_VIS_TOOLTIP"));
    }

    hbox2_->pack_start(*button_rename_);
    hbox2_->pack_start(*button_visibility_);
    pack_start(*hbox2_);

    treemodel_ = Gtk::ListStore::create(spots_);
    treeview_->set_model(treemodel_);
    treeviewconn_ = treeview_->get_selection()->signal_changed().connect(
                        sigc::mem_fun(
                            *this, &ControlSpotPanel::controlspotChanged));
    treeview_->set_grid_lines(Gtk::TREE_VIEW_GRID_LINES_VERTICAL);

    // Disable search to prevent hijacking keyboard shortcuts #5265
    treeview_->set_enable_search(false);
    treeview_->signal_key_press_event().connect(
        sigc::mem_fun(
            *this, &ControlSpotPanel::blockTreeviewSearch), false);

    // Avoid situation where no spot is selected (Ctrl+click on treeview)
    treeview_->signal_button_press_event().connect(
        sigc::mem_fun(
            *this, &ControlSpotPanel::onSpotSelectionEvent), false);

    auto cell = Gtk::manage(new Gtk::CellRendererText());
    int cols_count = treeview_->append_column(M("TP_LOCALLAB_COL_NAME"), *cell);
    auto col = treeview_->get_column(cols_count - 1);

    if (col) {
        col->set_cell_data_func(
            *cell, sigc::mem_fun(
                *this, &ControlSpotPanel::render_name));
    }

    cell = Gtk::manage(new Gtk::CellRendererText());
    cols_count = treeview_->append_column(M("TP_LOCALLAB_COL_VIS"), *cell);
    col = treeview_->get_column(cols_count - 1);

    if (col) {
        col->set_cell_data_func(
            *cell, sigc::mem_fun(
                *this, &ControlSpotPanel::render_isvisible));
    }

    scrolledwindow_->add(*treeview_);
    scrolledwindow_->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledwindow_->set_min_content_height(150);
    pack_start(*scrolledwindow_);
    pack_start(*hishow_);

   // Gtk::Box* const ctboxactivmethod = Gtk::manage(new Gtk::Box());
    ctboxactivmethod->pack_start(*activ_);
    pack_start(*ctboxactivmethod);

    Gtk::Label* const labelshape = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_SHAPETYPE") + ":"));
    ctboxshape->pack_start(*labelshape, Gtk::PACK_SHRINK, 4);
    shape_->append(M("TP_LOCALLAB_ELI"));
    shape_->append(M("TP_LOCALLAB_RECT"));
    shape_->set_active(0);
    shapeconn_ = shape_->signal_changed().connect(
                     sigc::mem_fun(
                         *this, &ControlSpotPanel::shapeChanged));
    ctboxshape->pack_start(*shape_);
    pack_start(*ctboxshape);
    if (showtooltip) {
        shape_->set_tooltip_text(M("TP_LOCALLAB_SHAPE_TOOLTIP"));
    }

    Gtk::Label* const labelspotmethod = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_EXCLUTYPE") + ":"));
    ctboxspotmethod->pack_start(*labelspotmethod, Gtk::PACK_SHRINK, 4);

    if (showtooltip) {
        ctboxspotmethod->set_tooltip_markup(M("TP_LOCALLAB_EXCLUTYPE_TOOLTIP"));
    }

    spotMethod_->append(M("TP_LOCALLAB_EXNORM"));
    spotMethod_->append(M("TP_LOCALLAB_EXECLU"));
    spotMethod_->append(M("TP_LOCALLAB_EXFULL"));
    spotMethod_->append(M("TP_LOCALLAB_EXMAIN"));//new choice Global
    spotMethod_->set_active(0);
    spotMethodconn_ = spotMethod_->signal_changed().connect(
                          sigc::mem_fun(
                              *this, &ControlSpotPanel::spotMethodChanged));
    ctboxspotmethod->pack_start(*spotMethod_);
    pack_start(*ctboxspotmethod);


    excluFrame->set_label_align(0.025, 0.5);

    if (showtooltip) {
        excluFrame->set_tooltip_text(M("TP_LOCALLAB_EXCLUF_TOOLTIP"));
    }

    ToolParamBlock* const excluBox = Gtk::manage(new ToolParamBlock());

    if (showtooltip) {
        sensiexclu_->set_tooltip_text(M("TP_LOCALLAB_SENSIEXCLU_TOOLTIP"));
    }

    sensiexclu_->setAdjusterListener(this);
    structexclu_->setAdjusterListener(this);
    structexclu_->setLogScale(10, 0);

    excluBox->pack_start(*sensiexclu_);
    excluBox->pack_start(*structexclu_);
    excluFrame->add(*excluBox);
    pack_start(*excluFrame);


    Gtk::Label* const labelshapemethod = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_STYPE") + ":"));
    ctboxshapemethod->pack_start(*labelshapemethod, Gtk::PACK_SHRINK, 4);

    if (showtooltip) {
        ctboxshapemethod->set_tooltip_markup(M("TP_LOCALLAB_STYPE_TOOLTIP"));
    }

    shapeMethod_->append(M("TP_LOCALLAB_IND"));
    shapeMethod_->append(M("TP_LOCALLAB_SYM"));
    shapeMethod_->append(M("TP_LOCALLAB_INDSL"));
    shapeMethod_->append(M("TP_LOCALLAB_SYMSL"));
    shapeMethod_->set_active(0);
    shapeMethodconn_ = shapeMethod_->signal_changed().connect(
                           sigc::mem_fun(
                               *this, &ControlSpotPanel::shapeMethodChanged));
    ctboxshapemethod->pack_start(*shapeMethod_);
//    pack_start(*ctboxshapemethod);

    pack_start(*locX_);
    locX_->setAdjusterListener(this);

    pack_start(*locXL_);
    locXL_->setAdjusterListener(this);

    pack_start(*locY_);
    locY_->setAdjusterListener(this);

    pack_start(*locYT_);
    locYT_->setAdjusterListener(this);

    pack_start(*centerX_);
    centerX_->setAdjusterListener(this);

    pack_start(*centerY_);
    centerY_->setAdjusterListener(this);

    pack_start(*circrad_);
    circrad_->setAdjusterListener(this);

    if (showtooltip) {
        circrad_->set_tooltip_text(M("TP_LOCALLAB_CIRCRAD_TOOLTIP"));
    }

    Gtk::Box* const ctboxqualitymethod = Gtk::manage(new Gtk::Box());
    Gtk::Label* const labelqualitymethod = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_QUAL_METHOD") + ":"));
    ctboxqualitymethod->pack_start(*labelqualitymethod, Gtk::PACK_SHRINK, 4);

    if (showtooltip) {
        ctboxqualitymethod->set_tooltip_markup(M("TP_LOCALLAB_METHOD_TOOLTIP"));
    }

    qualityMethod_->append(M("TP_LOCALLAB_ENH"));
    qualityMethod_->append(M("TP_LOCALLAB_ENHDEN"));
    qualityMethod_->set_active(1);
    qualityMethodconn_ = qualityMethod_->signal_changed().connect(
                             sigc::mem_fun(
                                 *this, &ControlSpotPanel::qualityMethodChanged));
    ctboxqualitymethod->pack_start(*qualityMethod_);

    if (showtooltip) {
        expTransGrad_->set_tooltip_text(M("TP_LOCALLAB_TRANSIT_TOOLTIP"));
    }

    ToolParamBlock* const transitBox = Gtk::manage(new ToolParamBlock());

    if (showtooltip) {
        transit_->set_tooltip_text(M("TP_LOCALLAB_TRANSIT_TOOLTIP"));
        transitweak_->set_tooltip_text(M("TP_LOCALLAB_TRANSITWEAK_TOOLTIP"));
        feather_->set_tooltip_text(M("TP_LOCALLAB_FEATH_TOOLTIP"));
        transitgrad_->set_tooltip_text(M("TP_LOCALLAB_TRANSITGRAD_TOOLTIP"));
        scopemask_->set_tooltip_text(M("TP_LOCALLAB_SCOPEMASK_TOOLTIP"));
        denoichmask_->set_tooltip_text(M("TP_LOCALLAB_DENOIMASK_TOOLTIP"));
    }

    transit_->setAdjusterListener(this);
    transitweak_->setAdjusterListener(this);
    transitgrad_->setAdjusterListener(this);
    feather_->setAdjusterListener(this);
    scopemask_->setAdjusterListener(this);
    denoichmask_->setAdjusterListener(this);
    transitBox->pack_start(*transit_);
    transitBox->pack_start(*transitweak_);
    transitBox->pack_start(*transitgrad_);
    //transitBox->pack_start(*feather_);
    expTransGrad_->add(*transitBox, false);
    pack_start(*expTransGrad_, false, false);

    if (showtooltip) {
        expShapeDetect_->set_tooltip_text(M("TP_LOCALLAB_ARTIF_TOOLTIP"));
    }

    ToolParamBlock* const artifBox = Gtk::manage(new ToolParamBlock());
    struc_->setAdjusterListener(this);
    thresh_->setAdjusterListener(this);
    iter_->setAdjusterListener(this);
    balan_->setAdjusterListener(this);
    balanh_->setAdjusterListener(this);
    colorde_->setAdjusterListener(this);
    colorscope_->setAdjusterListener(this);
    avoidrad_->setAdjusterListener(this);

    preview_->set_active(false);
    previewConn_ = preview_->signal_clicked().connect(
                       sigc::mem_fun(
                           *this, &ControlSpotPanel::previewChanged));

    if (showtooltip) {
        balan_->set_tooltip_text(M("TP_LOCALLAB_BALAN_TOOLTIP"));
        balanh_->set_tooltip_text(M("TP_LOCALLAB_BALAN_TOOLTIP"));
        colorde_->set_tooltip_text(M("TP_LOCALLAB_COLORDE_TOOLTIP"));
        colorscope_->set_tooltip_text(M("TP_LOCALLAB_COLORSCOPE_TOOLTIP"));
        preview_->set_tooltip_text(M("TP_LOCALLAB_COLORDEPREV_TOOLTIP"));
    }

//    artifBox->pack_start(*struc_);
    artifBox->pack_start(*thresh_);
    artifBox->pack_start(*iter_);
    artifBox->pack_start(*balan_);
    artifBox->pack_start(*balanh_);
    artifBox->pack_start(*colorde_);
    expShapeDetect_->add(*artifBox, false);
    pack_start(*expShapeDetect_, false, false);
//    ToolParamBlock* const artifBox2 = Gtk::manage(new ToolParamBlock());

    artifBox2->pack_start(*preview_);
    artifBox2->pack_start(*colorscope_);//unused with contrlspotpanel since 17 / 01 : 2024 but data used in color, vibrance, sh
    colorscope_->hide();
    pack_start(*artifBox2);
    ToolParamBlock* const specCaseBox = Gtk::manage(new ToolParamBlock());

    hishowconn_  = hishow_->signal_toggled().connect(
                      sigc::mem_fun(*this, &ControlSpotPanel::hishowChanged));

    activConn_  = activ_->signal_toggled().connect(
                     sigc::mem_fun(*this, &ControlSpotPanel::activChanged));

    Gtk::Label* const labelgamut = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_AVOID") + ":"));
    ctboxgamut->pack_start(*labelgamut, Gtk::PACK_SHRINK, 4);
    avoidgamutMethod_->append(M("TP_LOCALLAB_GAMUTNON"));
    avoidgamutMethod_->append(M("TP_LOCALLAB_GAMUTLABRELA"));
    avoidgamutMethod_->append(M("TP_LOCALLAB_GAMUTXYZABSO"));
    avoidgamutMethod_->append(M("TP_LOCALLAB_GAMUTXYZRELA"));
    avoidgamutMethod_->append(M("TP_LOCALLAB_GAMUTMUNSELL"));
    avoidgamutMethod_->set_active(4);
    avoidgamutconn_ = avoidgamutMethod_->signal_changed().connect(
                     sigc::mem_fun(
                         *this, &ControlSpotPanel::avoidgamutMethodChanged));
    ctboxgamut->pack_start(*avoidgamutMethod_);
    if (showtooltip) {
        ctboxgamut->set_tooltip_text(M("TP_LOCALLAB_AVOIDCOLORSHIFT_TOOLTIP"));
    }

 Gtk::Frame* const avFrame = Gtk::manage(new Gtk::Frame());
    ToolParamBlock* const avbox = Gtk::manage(new ToolParamBlock());
    avFrame->set_label_align(0.025, 0.5);
    avbox->pack_start(*ctboxgamut);
    avbox->pack_start(*avoidrad_);
    avFrame->add(*avbox);
    specCaseBox->pack_start(*avFrame);

    avoidnegConn_  = avoidneg_->signal_toggled().connect(
                     sigc::mem_fun(*this, &ControlSpotPanel::avoidnegChanged));

    blwhConn_  = blwh_->signal_toggled().connect(
                     sigc::mem_fun(*this, &ControlSpotPanel::blwhChanged));

    if (showtooltip) {
        blwh_->set_tooltip_text(M("TP_LOCALLAB_BLWH_TOOLTIP"));
    }

    specCaseBox->pack_start(*blwh_);
    specCaseBox->pack_start(*avoidneg_);

    recursConn_  = recurs_->signal_toggled().connect(
                       sigc::mem_fun(*this, &ControlSpotPanel::recursChanged));

    if (showtooltip) {
        recurs_->set_tooltip_text(M("TP_LOCALLAB_RECURS_TOOLTIP"));
    }

    specCaseBox->pack_start(*recurs_);
    specCaseBox->pack_start(*ctboxshapemethod);

    Gtk::Box* const ctboxwavmethod = Gtk::manage(new Gtk::Box());
    Gtk::Label* const labelwavmethod = Gtk::manage(new Gtk::Label(M("TP_WAVELET_DAUBLOCAL") + ":"));
    ctboxwavmethod->pack_start(*labelwavmethod, Gtk::PACK_SHRINK, 4);

    if (showtooltip) {
        ctboxwavmethod->set_tooltip_markup(M("TP_WAVELET_DAUB_TOOLTIP"));
    }

    wavMethod_->append(M("TP_WAVELET_DAUB2"));
    wavMethod_->append(M("TP_WAVELET_DAUB4"));
    wavMethod_->append(M("TP_WAVELET_DAUB6"));
    wavMethod_->append(M("TP_WAVELET_DAUB10"));
    wavMethod_->append(M("TP_WAVELET_DAUB14"));
    wavMethod_->append(M("TP_WAVELET_DAUB20"));
    wavMethod_->set_active(1);
    wavMethodconn_ = wavMethod_->signal_changed().connect(
                         sigc::mem_fun(
                             *this, &ControlSpotPanel::wavMethodChanged));
    ctboxwavmethod->pack_start(*wavMethod_);
    specCaseBox->pack_start(*ctboxwavmethod);


    expSpecCases_->add(*specCaseBox, false);
    pack_start(*expSpecCases_, false, false);

    if (showtooltip) {
        expMaskMerge_->set_tooltip_text(M("TP_LOCALLAB_MASFRAME_TOOLTIP"));
    }

    ToolParamBlock* const maskBox = Gtk::manage(new ToolParamBlock());
    laplacConn_  = laplac_->signal_toggled().connect(
                       sigc::mem_fun(*this, &ControlSpotPanel::laplacChanged));
    deltaeConn_  = deltae_->signal_toggled().connect(
                       sigc::mem_fun(*this, &ControlSpotPanel::deltaeChanged));
    shortcConn_  = shortc_->signal_toggled().connect(
                       sigc::mem_fun(*this, &ControlSpotPanel::shortcChanged));

    if (showtooltip) {
        shortc_->set_tooltip_text(M("TP_LOCALLAB_SHORTCMASK_TOOLTIP"));
    }

    lumask_->setAdjusterListener(this);
    //savrestConn_  = savrest_->signal_toggled().connect(
    //                    sigc::mem_fun(*this, &ControlSpotPanel::savrestChanged));

    if (showtooltip) {
        //savrest_->set_tooltip_text(M("TP_LOCALLAB_SAVREST_TOOLTIP"));
        lumask_->set_tooltip_text(M("TP_LOCALLAB_LUMASK_TOOLTIP"));
        laplac_->set_tooltip_text(M("TP_LOCALLAB_LAP_MASK_TOOLTIP"));
    }

//    maskBox->pack_start(*laplac_);
    maskBox->pack_start(*deltae_);
    maskBox->pack_start(*scopemask_);
    maskBox->pack_start(*denoichmask_);
    maskBox->pack_start(*feather_);
    // maskBox->pack_start(*shortc_);
    maskBox->pack_start(*lumask_);
    // maskBox->pack_start(*savrest_);
    expMaskMerge_->add(*maskBox, false);
    pack_start(*expMaskMerge_, false, false);

    Gtk::Separator *separatormet = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    pack_start(*separatormet, Gtk::PACK_SHRINK, 2);

    //Gtk::Box* const ctboxcomplexmethod = Gtk::manage(new Gtk::Box());

    //if (showtooltip) {
    //    ctboxcomplexmethod->set_tooltip_markup(M("TP_LOCALLAB_COMPLEXMETHOD_TOOLTIP"));
    //}

    //Gtk::Label* const labelcomplexmethod = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_COMPLEX_METHOD") + ":"));
    //ctboxcomplexmethod->pack_start(*labelcomplexmethod, Gtk::PACK_SHRINK, 4);

    //if (showtooltip) {
    //    complexMethod_->set_tooltip_markup(M("TP_LOCALLAB_COMPLEX_TOOLTIP"));
    //}

    //complexMethod_->append(M("TP_LOCALLAB_SIM"));
    //complexMethod_->append(M("TP_LOCALLAB_MED"));
    //complexMethod_->append(M("TP_LOCALLAB_ALL"));
    //complexMethod_->set_active(1);
    //complexMethodconn_ = complexMethod_->signal_changed().connect(
    //                         sigc::mem_fun(
    //                             *this, &ControlSpotPanel::complexMethodChanged));
    //ctboxcomplexmethod->pack_start(*complexMethod_);
    // pack_start(*ctboxcomplexmethod);
/*
    Gtk::Box* const ctboxwavmethod = Gtk::manage(new Gtk::Box());
    Gtk::Label* const labelwavmethod = Gtk::manage(new Gtk::Label(M("TP_WAVELET_DAUBLOCAL") + ":"));
    ctboxwavmethod->pack_start(*labelwavmethod, Gtk::PACK_SHRINK, 4);

    if (showtooltip) {
        ctboxwavmethod->set_tooltip_markup(M("TP_WAVELET_DAUB_TOOLTIP"));
    }

    wavMethod_->append(M("TP_WAVELET_DAUB2"));
    wavMethod_->append(M("TP_WAVELET_DAUB4"));
    wavMethod_->append(M("TP_WAVELET_DAUB6"));
    wavMethod_->append(M("TP_WAVELET_DAUB10"));
    wavMethod_->append(M("TP_WAVELET_DAUB14"));
    wavMethod_->set_active(1);
    wavMethodconn_ = wavMethod_->signal_changed().connect(
                         sigc::mem_fun(
                             *this, &ControlSpotPanel::wavMethodChanged));
    ctboxwavmethod->pack_start(*wavMethod_);
    pack_start(*ctboxwavmethod);
*/
    show_all();
    // Define row background color
    // Mouseovered spot (opaque orange)
    colorMouseover.set_red(1.);
    colorMouseover.set_green(100. / 255.);
    colorMouseover.set_blue(0.);
    colorMouseover.set_alpha(1.);

    colorMouseovertext.set_red(0.6);
    colorMouseovertext.set_green(100. / 255.);
    colorMouseovertext.set_blue(0.);
    colorMouseovertext.set_alpha(0.5);

    // Nominal spot (transparent black)
    colorNominal.set_red(0.);
    colorNominal.set_green(0.);
    colorNominal.set_blue(0.);
    colorNominal.set_alpha(0.);
}

ControlSpotPanel::~ControlSpotPanel()
{
    // visibleGeometry
    for (auto i = EditSubscriber::visibleGeometry.begin(); i != EditSubscriber::visibleGeometry.end(); ++i) {
        delete *i;
    }

    // mouseOverGeometry
    for (auto i = EditSubscriber::mouseOverGeometry.begin(); i != EditSubscriber::mouseOverGeometry.end(); ++i) {
        delete *i;
    }
}

void ControlSpotPanel::setEditProvider(EditDataProvider* provider)
{
    EditSubscriber::setEditProvider(provider);
}

void ControlSpotPanel::render_name(
    Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter)
{
    auto row = *iter;
    Gtk::CellRendererText *ct = static_cast<Gtk::CellRendererText *>(cell);

    // Render cell text
    ct->property_text() = row[spots_.name];

    // Render cell background color
    if (row[spots_.mouseover]) {
        ct->property_background_rgba() = colorMouseovertext;
    } else {
        ct->property_background_rgba() = colorNominal;
    }
}

void ControlSpotPanel::render_isvisible(
    Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter)
{
    auto row = *iter;
    Gtk::CellRendererText *ct = static_cast<Gtk::CellRendererText *>(cell);

    // Render cell text
    if (row[spots_.isvisible]) {
        ct->property_text() = M("TP_LOCALLAB_ROW_VIS");
    } else {
        ct->property_text() = M("TP_LOCALLAB_ROW_NVIS");
    }

    // Render cell background color
    if (row[spots_.mouseover]) {
        ct->property_background_rgba() = colorMouseovertext;
    } else {
        ct->property_background_rgba() = colorNominal;
    }
}

void ControlSpotPanel::on_button_add()
{
    // printf("on_button_add\n");

    if (!listener) {
        return;
    }

    // Raise event
    nbSpotChanged_ = true;
    selSpotChanged_ = true;
    eventType = SpotCreation;
    listener->panelChanged(EvLocallabSpotCreated, "-");
}

void ControlSpotPanel::on_button_delete()
{
    // printf("on_button_delete\n");

    if (!listener) {
        return;
    }

    // Raise event
    const int selIndex = getSelectedSpot();

    if (selIndex == -1) { // No selected spot to remove
        return;
    }

    nbSpotChanged_ = true;
    selSpotChanged_ = true;
    eventType = SpotDeletion;
    const std::unique_ptr<SpotRow> delSpotRow = getSpot(selIndex);
    listener->panelChanged(EvLocallabSpotDeleted, delSpotRow->name);
}

void ControlSpotPanel::on_button_duplicate()
{
    // printf("on_button_duplicate\n");

    if (!listener) {
        return;
    }

    // Raise event
    const int selIndex = getSelectedSpot();

    if (selIndex == -1) { // No selected spot to duplicate
        return;
    }

    nbSpotChanged_ = true;
    selSpotChanged_ = true;
    eventType = SpotDuplication;
    const std::unique_ptr<SpotRow> duplSpotRow = getSpot(selIndex);
    listener->panelChanged(EvLocallabSpotCreated, M("TP_LOCALLAB_EV_DUPL") + " "
                           + duplSpotRow->name);
}

void ControlSpotPanel::on_button_rename()
{
    // printf("on_button_rename\n");

    if (!listener) {
        return;
    }

    // Get actual control spot name
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    const Gtk::TreeModel::Row row = *iter;
    const Glib::ustring actualname = row[spots_.name];

    // Launch windows to update spot name
    RenameDialog d(actualname,
                   static_cast<Gtk::Window &>(*get_toplevel()));
    int status = d.run();

    // Update actual name and raise event
    if (status == RenameDialog::OkButton) {
        const Glib::ustring newname = d.get_new_name();

        if (newname != actualname) { // Event is only raised if name is updated
            nameChanged_ = true;
            row[spots_.name] = newname;
            treeview_->columns_autosize();
            listener->panelChanged(EvLocallabSpotName, newname);
            if (controlPanelListener) {
                controlPanelListener->spotNameChanged(newname);
            }
        }
    }
}

bool ControlSpotPanel::on_button_visibility(GdkEventButton* event)
{
    // printf("on_button_visibility\n");

    if (!listener) {
        return true;
    }

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return true;
    }

    const auto iter = s->get_selected();
    const Gtk::TreeModel::Row row = *iter;

    const int ctrl = event->state & GDK_CONTROL_MASK;

    if (event->button == 1) { // Left click on button
        if (ctrl) { // Ctrl+click case: all spots are shown/hidden
            // Get visibility of selected spot
            const bool selVisibility = row[spots_.isvisible];

            // Update visibility of all spot
            const Gtk::TreeModel::Children children = treemodel_->children();

            for (auto i = children.begin(); i != children.end(); i++) {
                Gtk::TreeModel::Row r = *i;
                r[spots_.isvisible] = !selVisibility;
                updateControlSpotCurve(r);
            }

            // Raise event
            visibilityChanged_ = true;
            eventType = SpotAllVisibilityChanged;

            if (!selVisibility) {
                listener->panelChanged(EvLocallabSpotVisibility, M("TP_LOCALLAB_EV_VIS_ALL"));
            } else {
                listener->panelChanged(EvLocallabSpotVisibility, M("TP_LOCALLAB_EV_NVIS_ALL"));
            }

            return true;
        } else { // Click case: only selected spot is shown/hidden
            // Update visibility for selected spot only
            row[spots_.isvisible] = !row[spots_.isvisible];
            updateControlSpotCurve(row);

            // Raise event
            visibilityChanged_ = true;
            const std::unique_ptr<SpotRow> spotRow = getSpot(getSelectedSpot());

            if (row[spots_.isvisible]) {
                listener->panelChanged(EvLocallabSpotVisibility, M("TP_LOCALLAB_EV_VIS") + " (" + spotRow->name + ")");
            } else {
                listener->panelChanged(EvLocallabSpotVisibility, M("TP_LOCALLAB_EV_NVIS") + " (" + spotRow->name + ")");
            }

            return true;
        }
    }

    return false;
}


bool ControlSpotPanel::blockTreeviewSearch(GdkEventKey* event)
{
    // printf("blockTreeviewSearch\n");

    if (event->state & Gdk::CONTROL_MASK) { // Ctrl
        if (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F) {
            // No action is performed to avoid activating treeview search
            return true;
        }
    }

    // Otherwise key action is transferred to treeview widget
    return false;
}

bool ControlSpotPanel::onSpotSelectionEvent(GdkEventButton* event)
{
    if (event->state & Gdk::CONTROL_MASK) { // Ctrl
        // No action is performed to avoid a situation where no spot is selected
        return true;
    }

    // Otherwise selection action is transferred to treeview widget
    return false;
}

void ControlSpotPanel::load_ControlSpot_param()
{
    // printf("load_ControlSpot_param\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    const Gtk::TreeModel::Row row = *iter;

    // Load param in selected control spot
    prevMethod_->set_active(row[spots_.prevMethod]);
    shape_->set_active(row[spots_.shape]);
    spotMethod_->set_active(row[spots_.spotMethod]);
    sensiexclu_->setValue((double)row[spots_.sensiexclu]);
    structexclu_->setValue((double)row[spots_.structexclu]);
    shapeMethod_->set_active(row[spots_.shapeMethod]);
    locX_->setValue((double)row[spots_.locX]);
    locXL_->setValue((double)row[spots_.locXL]);
    locY_->setValue((double)row[spots_.locY]);
    locYT_->setValue((double)row[spots_.locYT]);
    centerX_->setValue((double)row[spots_.centerX]);
    centerY_->setValue((double)row[spots_.centerY]);
    circrad_->setValue((double)row[spots_.circrad]);
    qualityMethod_->set_active(row[spots_.qualityMethod]);
    transit_->setValue((double)row[spots_.transit]);
    transitweak_->setValue((double)row[spots_.transitweak]);
    transitgrad_->setValue((double)row[spots_.transitgrad]);
    feather_->setValue((double)row[spots_.feather]);
    struc_->setValue((double)row[spots_.struc]);
    thresh_->setValue((double)row[spots_.thresh]);

    iter_->setValue((double)row[spots_.iter]);    
    balan_->setValue((double)row[spots_.balan]);
    balanh_->setValue((double)row[spots_.balanh]);
    colorde_->setValue((double)row[spots_.colorde]);

    
    colorscope_->setValue((double)row[spots_.colorscope]);
    avoidrad_->setValue((double)row[spots_.avoidrad]);
    hishow_->set_active(row[spots_.hishow]);
    activ_->set_active(row[spots_.activ]);
    avoidneg_->set_active(row[spots_.avoidneg]);
    blwh_->set_active(row[spots_.blwh]);
    recurs_->set_active(row[spots_.recurs]);
   // laplac_->set_active(row[spots_.laplac]);
    laplac_->set_active(true);
    deltae_->set_active(row[spots_.deltae]);
    scopemask_->setValue((double)row[spots_.scopemask]);
    denoichmask_->setValue(row[spots_.denoichmask]);
    shortc_->set_active(row[spots_.shortc]);
    lumask_->setValue((double)row[spots_.lumask]);
    //savrest_->set_active(row[spots_.savrest]);
    //complexMethod_->set_active(row[spots_.complexMethod]);
    wavMethod_->set_active(row[spots_.wavMethod]);
	avoidgamutMethod_->set_active(row[spots_.avoidgamutMethod]);

}

void ControlSpotPanel::controlspotChanged()
{
    // printf("controlspotChanged\n");

    if (!listener) {
        return;
    }

    // Raise event
    const int selIndex = getSelectedSpot();

    if (selIndex == -1) { // No selected spot
        return;
    }

    selSpotChanged_ = true;
    eventType = SpotSelection;
    const std::unique_ptr<SpotRow> spotRow = getSpot(selIndex);

    // Image area shall be regenerated if mask or deltaE preview was active when switching spot
    if (maskPrevActive || preview_->get_active()) {
        listener->panelChanged(EvLocallabSpotSelectedWithMask, spotRow->name);
    } else {
        listener->panelChanged(EvLocallabSpotSelected, spotRow->name);
    }
}

void ControlSpotPanel::shapeChanged()
{
    // printf("shapeChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    row[spots_.shape] = shape_->get_active_row_number();
    updateControlSpotCurve(row);

    // Raise event
    if (listener) {
        listener->panelChanged(EvLocallabSpotShape, shape_->get_active_text());
    }
}

void ControlSpotPanel::prevMethodChanged()
{
    // printf("prevMethodChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    row[spots_.prevMethod] = prevMethod_->get_active_row_number();
/*
    // Update Control Spot GUI according to spotMethod_ combobox state (to be compliant with updateParamVisibility function)
    if (multiImage && prevMethod_->get_active_text() == M("GENERAL_UNCHANGED")) {
        expTransGrad_->show();
        expShapeDetect_->show();
        expSpecCases_->show();
        expMaskMerge_->show();
        circrad_->show();
        ctboxshape->show();
    } else if (prevMethod_->get_active_row_number() == 0) { // Normal case
        expTransGrad_->hide();
        expShapeDetect_->hide();
        expSpecCases_->hide();
        expMaskMerge_->hide();
        circrad_->hide();
        ctboxshape->hide();
        shapeMethod_->set_active(0);

    } else { // Excluding case
        expTransGrad_->show();
        expShapeDetect_->show();
        expSpecCases_->show();
        expMaskMerge_->show();
        circrad_->show();
        ctboxshape->show();
    }
*/
    // Raise event
    if (listener) {
//        listener->panelChanged(EvLocallabSpotprevMethod, prevMethod_->get_active_text());
    }
}



void ControlSpotPanel::spotMethodChanged()
{
    //01 2024 take into account new problems linked to Global spotmethod
    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    const int oldSpotMethod = row[spots_.spotMethod];
    row[spots_.spotMethod] = spotMethod_->get_active_row_number();
    //ctboxspotmethod->show();
    hishow_->show();
    ctboxshape->show();
    artifBox2->show();
    colorscope_->hide();

    // Update Control Spot GUI according to spotMethod_ combobox state (to be compliant with updateParamVisibility function)
    if (multiImage && spotMethod_->get_active_text() == M("GENERAL_UNCHANGED")) {
        excluFrame->show();

    } else if (spotMethod_->get_active_row_number() == 0) { // Normal case
        excluFrame->hide();
        // Reset spot shape only if previous spotMethod is Full image or Global
        if (oldSpotMethod == 2  || oldSpotMethod == 3) {
            disableParamlistener(true);
            locX_->setValue(150.);
            row[spots_.locX] = locX_->getIntValue();
            locXL_->setValue(150.);
            row[spots_.locXL] = locXL_->getIntValue();
            locY_->setValue(150.);
            row[spots_.locY] = locY_->getIntValue();
            locYT_->setValue(150.);
            row[spots_.locYT] = locYT_->getIntValue();
            shape_->set_active(0);
            row[spots_.shape] = shape_->get_active_row_number();
            transit_->setValue(60.);
            row[spots_.transit] = transit_->getValue();
            disableParamlistener(false);
            updateControlSpotCurve(row);
        }
    } else if (spotMethod_->get_active_row_number() == 1) { // Excluding case
        excluFrame->show();

        // Reset spot shape only if previous spotMethod is Full image or Global
        if (oldSpotMethod == 2 || oldSpotMethod == 3) {
            disableParamlistener(true);
            locX_->setValue(150.);
            row[spots_.locX] = locX_->getIntValue();
            locXL_->setValue(150.);
            row[spots_.locXL] = locXL_->getIntValue();
            locY_->setValue(150.);
            row[spots_.locY] = locY_->getIntValue();
            locYT_->setValue(150.);
            row[spots_.locYT] = locYT_->getIntValue();
            shape_->set_active(0);
            row[spots_.shape] = shape_->get_active_row_number();
            transit_->setValue(60.);
            row[spots_.transit] = transit_->getValue();
            disableParamlistener(false);
            updateControlSpotCurve(row);
        }
    } else if (spotMethod_->get_active_row_number() == 2  || spotMethod_->get_active_row_number() == 3) { // Full image or Global case
        excluFrame->hide();
        shape_->set_active(0);

        locX_->setValue(3000.);
        row[spots_.locX] = locX_->getIntValue();
        locXL_->setValue(3000.);
        row[spots_.locXL] = locXL_->getIntValue();
        locY_->setValue(3000.);
        row[spots_.locY] = locY_->getIntValue();
        locYT_->setValue(3000.);
        row[spots_.locYT] = locYT_->getIntValue();
        shape_->set_active(1);
        row[spots_.shape] = shape_->get_active_row_number();
        transit_->setValue(100.);
        row[spots_.transit] = transit_->getValue();
        
        if(spotMethod_->get_active_row_number() == 3) { //global
            ctboxshape->hide();
            artifBox2->hide();
            hishow_->hide();
            expTransGrad_->hide();
            expShapeDetect_->hide();
            expSpecCases_->hide();
            expMaskMerge_->hide();
            circrad_->hide();
            ctboxshape->hide();
        } else {
            ctboxshape->show();
            circrad_->show();    
            artifBox2->show();
            colorscope_->hide();
           
            hishow_->show();
            if(hishow_->get_active()) {
                expTransGrad_->show();
                expShapeDetect_->show();
                expSpecCases_->show();
                expMaskMerge_->show();
            }
        }
    }

    // Raise event
    if (listener) {
        listener->panelChanged(EvLocallabSpotSpotMethod, spotMethod_->get_active_text());
    }
}

void ControlSpotPanel::avoidgamutMethodChanged()
{

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }
    const int meth = avoidgamutMethod_->get_active_row_number();
    avoidrad_->show();

    if(meth == 2 || meth == 3 || meth == 4) {
        avoidrad_->hide();
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    row[spots_.avoidgamutMethod] = avoidgamutMethod_->get_active_row_number();

    // Raise event
    if (listener) {
        listener->panelChanged(EvLocallabavoidgamutMethod, avoidgamutMethod_->get_active_text());
    }

}

void ControlSpotPanel::shapeMethodChanged()
{
    // printf("shapeMethodChanged\n");

    const int method = shapeMethod_->get_active_row_number();

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    if (!batchMode && (method == 1 || method == 3)) { // Symmetrical cases
        disableParamlistener(true);
        locXL_->setValue(locX_->getValue());
        locYT_->setValue(locY_->getValue());
        disableParamlistener(false);

        row[spots_.shapeMethod] = shapeMethod_->get_active_row_number();
        row[spots_.locXL] = locX_->getIntValue();
        row[spots_.locYT] = locY_->getIntValue();

        updateControlSpotCurve(row);
    } else { // In batch mode, sliders are always independent
        row[spots_.shapeMethod] = shapeMethod_->get_active_row_number();
    }

    // Update Control Spot GUI according to shapeMethod_ combobox state (to be compliant with updateParamVisibility function)
    if (!batchMode) {
        if (method == 1 || method == 3) { // Symmetrical cases
            locXL_->hide();
            locYT_->hide();

            if (method == 1) { // 1 = Symmetrical (mouse)
                locX_->hide();
                locY_->hide();
                centerX_->hide();
                centerY_->hide();
            } else { // 3 = Symmetrical (mouse + sliders)
                locX_->show();
                locY_->show();
                centerX_->show();
                centerY_->show();
            }
        } else { // Independent cases
            if (method == 0) { // 0 = Independent (mouse)
                locX_->hide();
                locXL_->hide();
                locY_->hide();
                locYT_->hide();
                centerX_->hide();
                centerY_->hide();
            } else { // 2 = Independent (mouse + sliders)
                locX_->show();
                locXL_->show();
                locY_->show();
                locYT_->show();
                centerX_->show();
                centerY_->show();
            }
        }
    } else { // In batch mode, sliders are necessary shown
        locX_->show();
        locXL_->show();
        locY_->show();
        locYT_->show();
        centerX_->show();
        centerY_->show();
    }

    // Raise event
    if (listener) {
        listener->panelChanged(EvLocallabSpotShapeMethod, shapeMethod_->get_active_text());
    }
}

void ControlSpotPanel::qualityMethodChanged()
{
    // printf("qualityMethodChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    row[spots_.qualityMethod] = qualityMethod_->get_active_row_number();

    // Raise event
    if (listener) {
        listener->panelChanged(EvLocallabSpotQualityMethod, qualityMethod_->get_active_text());
    }
}

//void ControlSpotPanel::complexMethodChanged()
//{
//    // printf("qualityMethodChanged\n");
//
//    // Get selected control spot
//    const auto s = treeview_->get_selection();
//
//    if (!s->count_selected_rows()) {
//        return;
//    }
//
//    const auto iter = s->get_selected();
//    Gtk::TreeModel::Row row = *iter;
//
//    row[spots_.complexMethod] = complexMethod_->get_active_row_number();
//
//    if (multiImage && complexMethod_->get_active_text() == M("GENERAL_UNCHANGED")) {
//        // excluFrame->show();
//    } else if (complexMethod_->get_active_row_number() == 0) { //sim
//        // excluFrame->hide();
//    } else if (complexMethod_->get_active_row_number() == 1) { // mod
//        // excluFrame->show();
//    } else if (complexMethod_->get_active_row_number() == 2) { // all
//        // excluFrame->show();
//    }
//
//    // Raise event
//    if (listener) {
//        listener->panelChanged(EvLocallabSpotcomplexMethod, complexMethod_->get_active_text());
//    }
//}

void ControlSpotPanel::wavMethodChanged()
{
    // printf("qualityMethodChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    row[spots_.wavMethod] = wavMethod_->get_active_row_number();

    // Raise event
    if (listener) {
        listener->panelChanged(EvLocallabSpotwavMethod, wavMethod_->get_active_text());
    }
}

void ControlSpotPanel::updateParamVisibility()
{
    // printf("updateParamVisibility\n");

    // Update Control Spot GUI according to shapeMethod_ combobox state (to be compliant with shapeMethodChanged function)
    const int method = shapeMethod_->get_active_row_number();
    const int meth = avoidgamutMethod_->get_active_row_number();

    if (!batchMode) {
        if (method == 1 || method == 3) { // Symmetrical cases
            locXL_->hide();
            locYT_->hide();

            if (method == 1) { // 1 = Symmetrical (mouse)
                locX_->hide();
                locY_->hide();
                centerX_->hide();
                centerY_->hide();
            } else { // 3 = Symmetrical (mouse + sliders)
                locX_->show();
                locY_->show();
                centerX_->show();
                centerY_->show();
            }
        } else { // Independent cases
            if (method == 0) { // 0 = Independent (mouse)
                locX_->hide();
                locXL_->hide();
                locY_->hide();
                locYT_->hide();
                centerX_->hide();
                centerY_->hide();
            } else { // 2 = Independent (mouse + sliders)
                locX_->show();
                locXL_->show();
                locY_->show();
                locYT_->show();
                centerX_->show();
                centerY_->show();
            }
        }
    } else { // In batch mode, sliders are necessary shown
        locX_->show();
        locXL_->show();
        locY_->show();
        locYT_->show();
        centerX_->show();
        centerY_->show();
    }

    if(meth == 1) {
        avoidrad_->show();
    } else {
        avoidrad_->hide();
}
   // ctboxspotmethod->show();
    hishow_->show();
    artifBox2->show();
    ctboxshape->show();
    colorscope_->hide();
    
    // Update Control Spot GUI according to spotMethod_ combobox state (to be compliant with spotMethodChanged function)
    if (multiImage && spotMethod_->get_active_text() == M("GENERAL_UNCHANGED")) {
        excluFrame->show();
    } else if (spotMethod_->get_active_row_number() == 0) { // Normal case
        excluFrame->hide();
    } else if (spotMethod_->get_active_row_number() == 1) { // Excluding case
        excluFrame->show();
    } else if (spotMethod_->get_active_row_number() == 2 || spotMethod_->get_active_row_number() == 3) {//full image or global
        excluFrame->hide();
        
        if(spotMethod_->get_active_row_number() == 3) {        
            artifBox2->hide();
            hishow_->hide();
            hishow_->set_active(false);           
            ctboxshape->hide();
            circrad_->hide();
            expTransGrad_->hide();
            expShapeDetect_->hide();
            expSpecCases_->hide();
            expMaskMerge_->hide();

        } else {
            artifBox2->show();
            colorscope_->hide();
            hishow_->show();
            ctboxshape->show();
            circrad_->show();
            if(hishow_->get_active()) {
                expTransGrad_->show();
                expShapeDetect_->show();
                expSpecCases_->show();
                expMaskMerge_->show();
            }
            
        }    

    }

/*
    if (multiImage && prevMethod_->get_active_text() == M("GENERAL_UNCHANGED")) {
        expTransGrad_->show();
        expShapeDetect_->show();
        expSpecCases_->show();
        expMaskMerge_->show();
        circrad_->show();
        ctboxshape->show();
    } else if (prevMethod_->get_active_row_number() == 0) { // Normal case
    */
    //ctboxshape->show();
   // artifBox2->show();
   
    if (!hishow_->get_active()  || spotMethod_->get_active_row_number() == 3) { // Normal case or Global 
        expTransGrad_->hide();
        expShapeDetect_->hide();
        expSpecCases_->hide();
        expMaskMerge_->hide();
        circrad_->hide();
        ctboxshape->hide();
        if(spotMethod_->get_active_row_number() == 3) {
            artifBox2->hide();
            hishow_->hide();
            ctboxshape->hide();
            circrad_->hide();
            expTransGrad_->hide();
            expShapeDetect_->hide();
            expSpecCases_->hide();
            expMaskMerge_->hide();
            
        } else {
            hishow_->show();
            artifBox2->show();
            colorscope_->hide();
            hishow_->show();
            ctboxshape->show();
            circrad_->show();
        }
        
    } else { // Excluding case
        expTransGrad_->show();
        expShapeDetect_->show();
        expSpecCases_->show();
        expMaskMerge_->show();
        circrad_->show();
        ctboxshape->show();
        hishow_->show();
        
    }


}

void ControlSpotPanel::adjusterChanged(Adjuster* a, double newval)
{
    // printf("adjusterChanged\n");

    const int method = shapeMethod_->get_active_row_number();

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    if (a == sensiexclu_) {
        row[spots_.sensiexclu] = sensiexclu_->getIntValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotSensiexclu, sensiexclu_->getTextValue());
        }
    }

    if (a == structexclu_) {
        row[spots_.structexclu] = structexclu_->getIntValue();

        if (listener) {
            listener->panelChanged(Evlocallabstructexlu, structexclu_->getTextValue());
        }
    }

    if (a == locX_) {
        row[spots_.locX] = locX_->getIntValue();

        if (!batchMode && (method == 1 || method == 3)) { // Symmetrical cases (in batch mode, sliders are always independent)
            disableParamlistener(true);
            locXL_->setValue(locX_->getValue());
            disableParamlistener(false);
            row[spots_.locXL] = locXL_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocX, locX_->getTextValue());
        }
    }

    if (a == locXL_) {
        row[spots_.locXL] = locXL_->getIntValue();

        if (!batchMode && (method == 1 || method == 3)) { // Symmetrical cases (in batch mode, sliders are always independent)
            disableParamlistener(true);
            locX_->setValue(locXL_->getValue());
            disableParamlistener(false);
            row[spots_.locX] = locX_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocXL, locXL_->getTextValue());
        }
    }

    if (a == locY_) {
        row[spots_.locY] = locY_->getIntValue();

        if (!batchMode && (method == 1 || method == 3)) { // Symmetrical cases (in batch mode, sliders are always independent)
            disableParamlistener(true);
            locYT_->setValue(locY_->getValue());
            disableParamlistener(false);
            row[spots_.locYT] = locYT_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocY, locY_->getTextValue());
        }
    }

    if (a == locYT_) {
        row[spots_.locYT] = locYT_->getIntValue();

        if (!batchMode && (method == 1 || method == 3)) { // Symmetrical cases (in batch mode, sliders are always independent)
            disableParamlistener(true);
            locY_->setValue(locYT_->getValue());
            disableParamlistener(false);
            row[spots_.locY] = locY_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocYT, locYT_->getTextValue());
        }
    }

    if (a == centerX_ || a == centerY_) {
        row[spots_.centerX] = centerX_->getIntValue();
        row[spots_.centerY] = centerY_->getIntValue();

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotCenter, "X=" + centerX_->getTextValue() + ", Y=" + centerY_->getTextValue());
        }
    }

    if (a == circrad_) {
        row[spots_.circrad] = circrad_->getValue();

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotCircrad, circrad_->getTextValue());
        }
    }

    if (a == transit_) {
        row[spots_.transit] = transit_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotTransit, transit_->getTextValue());
        }
    }

    if (a == transitweak_) {
        row[spots_.transitweak] = transitweak_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotTransitweak, transitweak_->getTextValue());
        }
    }

    if (a == transitgrad_) {
        row[spots_.transitgrad] = transitgrad_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotTransitgrad, transitgrad_->getTextValue());
        }
    }

    if (a == feather_) {
        row[spots_.feather] = feather_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotfeather, feather_->getTextValue());
        }
    }

    if (a == struc_) {
        row[spots_.struc] = struc_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotStruc, struc_->getTextValue());
        }
    }

    if (a == thresh_) {
        row[spots_.thresh] = thresh_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotThresh, thresh_->getTextValue());
        }
    }

    if (a == iter_) {
        row[spots_.iter] = iter_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotIter, iter_->getTextValue());
        }
    }

    if (a == balan_) {
        row[spots_.balan] = balan_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotbalan, balan_->getTextValue());
        }
    }

    if (a == balanh_) {
        row[spots_.balanh] = balanh_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotbalanh, balanh_->getTextValue());
        }
    }

    if (a == colorde_) {
        row[spots_.colorde] = colorde_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotcolorde, colorde_->getTextValue());
        }
    }

    if (a == colorscope_) {
        row[spots_.colorscope] = colorscope_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotcolorscope, colorscope_->getTextValue());
        }
    }

    if (a == avoidrad_) {
        row[spots_.avoidrad] = avoidrad_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotavoidrad, avoidrad_->getTextValue());
        }
    }

    if (a == scopemask_) {
        row[spots_.scopemask] = scopemask_->getIntValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotscopemask, scopemask_->getTextValue());
        }
    }

    if (a == denoichmask_) {
        row[spots_.denoichmask] = denoichmask_->getValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotdenoichmask, denoichmask_->getTextValue());
        }
    }

    if (a == lumask_) {
        row[spots_.lumask] = lumask_->getIntValue();

        if (listener) {
            listener->panelChanged(EvLocallabSpotlumask, lumask_->getTextValue());
        }
    }
}

void ControlSpotPanel::hishowChanged()
{
    // printf("avoidChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.hishow] = hishow_->get_active();


    ctboxshape->show();

    if (!hishow_->get_active()  || spotMethod_->get_active_row_number() == 3) { // Normal case or Global
        expTransGrad_->hide();
        expShapeDetect_->hide();
        expSpecCases_->hide();
        expMaskMerge_->hide();
        circrad_->hide();
        ctboxshape->hide();
        shapeMethod_->set_active(0);
        if(spotMethod_->get_active_row_number() == 3) {
            hishow_->hide();
            hishow_->set_active(false);           
            circrad_->hide();
            expTransGrad_->hide();
            expShapeDetect_->hide();
            expSpecCases_->hide();
            expMaskMerge_->hide();
            
        } else {
            hishow_->show();
            circrad_->show();
            
        }

    } else { // Excluding case
        expTransGrad_->show();
        expShapeDetect_->show();
        expSpecCases_->show();
        expMaskMerge_->show();
        circrad_->show();
        ctboxshape->show();
        hishow_->show();
        
   }

    // Raise event
    if (listener) {
        if (hishow_->get_active()) {
            listener->panelChanged(Evlocallabhishow, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallabhishow, M("GENERAL_DISABLED"));
        }
    }
}


void ControlSpotPanel::activChanged()
{
    // printf("activChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.activ] = activ_->get_active();

    // Raise event
    if (listener) {
        if (activ_->get_active()) {
            listener->panelChanged(Evlocallabactiv, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallabactiv, M("GENERAL_DISABLED"));
        }
    }
}

void ControlSpotPanel::avoidnegChanged()
{

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.avoidneg] = avoidneg_->get_active();

    // Raise event
    if (listener) {
        if (avoidneg_->get_active()) {
            listener->panelChanged(EvLocallabavoidnegative, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvLocallabavoidnegative, M("GENERAL_DISABLED"));
        }
    }
}



void ControlSpotPanel::blwhChanged()
{
    // printf("blwhChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.blwh] = blwh_->get_active();

    // Raise event
    if (listener) {
        if (blwh_->get_active()) {
            listener->panelChanged(Evlocallabblwh, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallabblwh, M("GENERAL_DISABLED"));
        }
    }
}

void ControlSpotPanel::recursChanged()
{
    // printf("recursChanged\n");

    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.recurs] = recurs_->get_active();

    // Raise event
    if (listener) {
        if (recurs_->get_active()) {
            listener->panelChanged(Evlocallabrecurs, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallabrecurs, M("GENERAL_DISABLED"));
        }
    }
}


void ControlSpotPanel::laplacChanged()
{
    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.laplac] = laplac_->get_active();

    // Raise event
    if (listener) {
        if (laplac_->get_active()) {
            listener->panelChanged(Evlocallablaplac, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallablaplac, M("GENERAL_DISABLED"));
        }
    }
}


void ControlSpotPanel::deltaeChanged()
{
    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.deltae] = deltae_->get_active();

    // Raise event
    if (listener) {
        if (deltae_->get_active()) {
            listener->panelChanged(Evlocallabdeltae, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallabdeltae, M("GENERAL_DISABLED"));
        }
    }
}

void ControlSpotPanel::shortcChanged()
{
    // Get selected control spot
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;
    row[spots_.shortc] = shortc_->get_active();

    // Raise event
    if (listener) {
        if (shortc_->get_active()) {
            listener->panelChanged(Evlocallabshortc, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(Evlocallabshortc, M("GENERAL_DISABLED"));
        }
    }
}

//void ControlSpotPanel::savrestChanged()
//{
//    // Get selected control spot
//    const auto s = treeview_->get_selection();
//
//    if (!s->count_selected_rows()) {
//        return;
//    }
//
//    const auto iter = s->get_selected();
//    Gtk::TreeModel::Row row = *iter;
//    row[spots_.savrest] = savrest_->get_active();
//
//    // Raise event
//    if (listener) {
//        if (savrest_->get_active()) {
//            listener->panelChanged(Evlocallabsavrest, M("GENERAL_ENABLED"));
//        } else {
//            listener->panelChanged(Evlocallabsavrest, M("GENERAL_DISABLED"));
//        }
//    }
//}

void ControlSpotPanel::previewChanged()
{
    // If deltaE preview is activated, deactivate all other tool mask preview
    if (controlPanelListener) {
        controlPanelListener->resetToolMaskView();
    }

    if (listener) {
        listener->panelChanged(EvlocallabshowmaskMethod, "");
    }
}

void ControlSpotPanel::disableParamlistener(bool cond)
{
    // printf("disableParamlistener: %d\n", cond);

    treeviewconn_.block(cond);
    buttonaddconn_.block(cond);
    buttondeleteconn_.block(cond);
    buttonduplicateconn_.block(cond);
    buttonrenameconn_.block(cond);
    buttonvisibilityconn_.block(cond);
    prevMethodconn_.block(cond);
    shapeconn_.block(cond);
    spotMethodconn_.block(cond);
    sensiexclu_->block(cond);
    structexclu_->block(cond);
    shapeMethodconn_.block(cond);
    locX_->block(cond);
    locXL_->block(cond);
    locY_->block(cond);
    locYT_->block(cond);
    centerX_->block(cond);
    centerY_->block(cond);
    circrad_->block(cond);
    qualityMethodconn_.block(cond);
    transit_->block(cond);
    transitweak_->block(cond);
    transitgrad_->block(cond);
    feather_->block(cond);
    struc_->block(cond);
    thresh_->block(cond);
    iter_->block(cond);
    balan_->block(cond);
    balanh_->block(cond);
    colorde_->block(cond);
    colorscope_->block(cond);
    avoidrad_->block(cond);
    hishowconn_.block(cond);
    activConn_.block(cond);
    avoidnegConn_.block(cond);
    blwhConn_.block(cond);
    recursConn_.block(cond);
    laplacConn_.block(cond);
    deltaeConn_.block(cond);
    scopemask_->block(cond);
    denoichmask_->block(cond);
    shortcConn_.block(cond);
    lumask_->block(cond);
    //savrestConn_.block(cond);
    //complexMethodconn_.block(cond);
    wavMethodconn_.block(cond);
	avoidgamutconn_.block(cond);

}

void ControlSpotPanel::setParamEditable(bool cond)
{
    // printf("setParamEditable: %d\n", cond);

    prevMethod_->set_sensitive(cond);
    shape_->set_sensitive(cond);
    spotMethod_->set_sensitive(cond);
    sensiexclu_->set_sensitive(cond);
    structexclu_->set_sensitive(cond);
    shapeMethod_->set_sensitive(cond);
    locX_->set_sensitive(cond);
    locXL_->set_sensitive(cond);
    locY_->set_sensitive(cond);
    locYT_->set_sensitive(cond);
    centerX_->set_sensitive(cond);
    centerY_->set_sensitive(cond);
    circrad_->set_sensitive(cond);
    qualityMethod_->set_sensitive(cond);
    transit_->set_sensitive(cond);
    transitweak_->set_sensitive(cond);
    transitgrad_->set_sensitive(cond);
    feather_->set_sensitive(cond);
    struc_->set_sensitive(cond);
    thresh_->set_sensitive(cond);
    iter_->set_sensitive(cond);
    balan_->set_sensitive(cond);
    balanh_->set_sensitive(cond);
    colorde_->set_sensitive(cond);
    colorscope_->set_sensitive(cond);
    avoidrad_->set_sensitive(cond);
    hishow_->set_sensitive(cond);
    activ_->set_sensitive(cond);
    avoidneg_->set_sensitive(cond);
    blwh_->set_sensitive(cond);
    recurs_->set_sensitive(cond);
    laplac_->set_sensitive(cond);
    deltae_->set_sensitive(cond);
    scopemask_->set_sensitive(cond);
    denoichmask_->set_sensitive(cond);
    shortc_->set_sensitive(cond);
    lumask_->set_sensitive(cond);
    //savrest_->set_sensitive(cond);
    //complexMethod_->set_sensitive(cond);
    wavMethod_->set_sensitive(cond);
    preview_->set_sensitive(cond);
    avoidgamutMethod_->set_sensitive(cond);

    if (!cond) {
        // Reset complex parameters visibility to default state
        expTransGrad_->hide();
        expShapeDetect_->hide();
        expSpecCases_->hide();
        expMaskMerge_->hide();
        circrad_->hide();
        ctboxshape->hide();
        excluFrame->hide();
//        ctboxshapemethod->hide();
        locX_->hide();
        locXL_->hide();
        locY_->hide();
        locYT_->hide();
        centerX_->hide();
        centerY_->hide();
    }
}

void ControlSpotPanel::setDefaultExpanderVisibility()
{
    expTransGrad_->set_expanded(false);
    expShapeDetect_->set_expanded(false);
    expSpecCases_->set_expanded(false);
    expMaskMerge_->set_expanded(false);
}

void ControlSpotPanel::addControlSpotCurve(Gtk::TreeModel::Row& row)
{
    // printf("addControlSpotCurve\n");

    if (row[spots_.curveid] > 0) { // Row has already an associated curve
        return;
    }

    // Creation of visibleGeometry
    Circle* cirX;
    cirX = new Circle();
    cirX->radius = 4.;
    cirX->filled = true;
    cirX->datum = Geometry::IMAGE;
    Circle* cirXL;
    cirXL = new Circle();
    cirXL->radius = 4.;
    cirXL->filled = true;
    cirXL->datum = Geometry::IMAGE;
    Circle* cirY;
    cirY = new Circle();
    cirY->radius = 4.;
    cirY->filled = true;
    cirY->datum = Geometry::IMAGE;
    Circle* cirYT;
    cirYT = new Circle();
    cirYT->radius = 4.;
    cirYT->filled = true;
    cirYT->datum = Geometry::IMAGE;
    Circle* centerCircle;
    centerCircle = new Circle();
    centerCircle->datum = Geometry::IMAGE;
    centerCircle->radiusInImageSpace = true;
    Ellipse* shape_ellipse;
    shape_ellipse = new Ellipse();
    shape_ellipse->datum = Geometry::IMAGE;
    shape_ellipse->radiusInImageSpace = true;
    EditRectangle* shape_rectangle;
    shape_rectangle = new EditRectangle();
    shape_rectangle->datum = Geometry::IMAGE;
    EditSubscriber::visibleGeometry.push_back(centerCircle); // (curveid - 1) * 7
    EditSubscriber::visibleGeometry.push_back(shape_ellipse); // (curveid - 1) * 7 + 1
    EditSubscriber::visibleGeometry.push_back(shape_rectangle); // (curveid - 1) * 7 + 2
    EditSubscriber::visibleGeometry.push_back(cirX); // (curveid - 1) * 7 + 3
    EditSubscriber::visibleGeometry.push_back(cirXL); // (curveid - 1) * 7 + 4
    EditSubscriber::visibleGeometry.push_back(cirY); // (curveid - 1) * 7 + 5
    EditSubscriber::visibleGeometry.push_back(cirYT); // (curveid - 1) * 7 + 6

    // Creation of mouseOverGeometry
    cirX = new Circle();
    cirX->radius = 4.;
    cirX->filled = true;
    cirX->datum = Geometry::IMAGE;
    cirXL = new Circle();
    cirXL->radius = 4.;
    cirXL->filled = true;
    cirXL->datum = Geometry::IMAGE;
    cirY = new Circle();
    cirY->radius = 4.;
    cirY->filled = true;
    cirY->datum = Geometry::IMAGE;
    cirYT = new Circle();
    cirYT->radius = 4.;
    cirYT->filled = true;
    cirYT->datum = Geometry::IMAGE;
    centerCircle = new Circle();
    centerCircle->filled = true;
    centerCircle->datum = Geometry::IMAGE;
    centerCircle->radiusInImageSpace = true;
    shape_ellipse = new Ellipse();
    shape_ellipse->datum = Geometry::IMAGE;
    shape_ellipse->radiusInImageSpace = true;
    shape_rectangle = new EditRectangle();
    shape_rectangle->datum = Geometry::IMAGE;
    EditSubscriber::mouseOverGeometry.push_back(centerCircle);  // (curveid - 1) * 7
    EditSubscriber::mouseOverGeometry.push_back(shape_ellipse);  // (curveid - 1) * 7 + 1
    EditSubscriber::mouseOverGeometry.push_back(shape_rectangle);  // (curveid - 1) * 7 + 2
    EditSubscriber::mouseOverGeometry.push_back(cirX);  // (curveid - 1) * 7 + 3
    EditSubscriber::mouseOverGeometry.push_back(cirXL);  // (curveid - 1) * 7 + 4
    EditSubscriber::mouseOverGeometry.push_back(cirY);  // (curveid - 1) * 7 + 5
    EditSubscriber::mouseOverGeometry.push_back(cirYT);  // (curveid - 1) * 7 + 6

    row[spots_.curveid] = EditSubscriber::visibleGeometry.size() / 7;
}

void ControlSpotPanel::updateControlSpotCurve(const Gtk::TreeModel::Row& row)
{
    const int curveid_ = row[spots_.curveid];
    EditDataProvider* const dataProvider = getEditProvider();

    // printf("updateControlSpotCurve: %d\n", curveid_);

    if (curveid_ == 0 || !dataProvider) { // Row has no associated curve or there is no EditProvider
        return;
    }

    int imW = 0;
    int imH = 0;
    dataProvider->getImageSize(imW, imH);

    if (!imW || !imH) { // No image loaded
        return;
    }

    const int centerX_ = row[spots_.centerX];
    const int centerY_ = row[spots_.centerY];
    const int circrad_ = row[spots_.circrad];
    const int locX_ = row[spots_.locX];
    const int locXL_ = row[spots_.locXL];
    const int locY_ = row[spots_.locY];
    const int locYT_ = row[spots_.locYT];
    const int shape_ = row[spots_.shape];
    const bool isvisible_ = row[spots_.isvisible];

    const int decayX = (double)locX_ * (double)imW / 2000.;
    const int decayXL = (double)locXL_ * (double)imW / 2000.;
    const int decayY = (double)locY_ * (double)imH / 2000.;
    const int decayYT = (double)locYT_ * (double)imH / 2000.;
    const rtengine::Coord origin((double)imW / 2. + (double)centerX_ * (double)imW / 2000., (double)imH / 2. + (double)centerY_ * (double)imH / 2000.);

    const auto updateSelectionCircle = [&](Geometry * geometry, const int offsetX, const int offsetY) {
        const auto cir = static_cast<Circle*>(geometry);
        cir->center.x = origin.x + offsetX;
        cir->center.y = origin.y + offsetY;
    };

    const auto updateCenterCircle = [&](Geometry * geometry) {
        const auto circle = static_cast<Circle*>(geometry);
        circle->center = origin;
        circle->radius = circrad_;
    };

    const auto updateEllipse = [&](Geometry * geometry) {
        const auto ellipse = static_cast<Ellipse*>(geometry);
        ellipse->center = origin;
        ellipse->radX = decayX;
        ellipse->radXL = decayXL;
        ellipse->radY = decayY;
        ellipse->radYT = decayYT;
    };

    const auto updateRectangle = [&](Geometry * geometry) {
        const auto rectangle = static_cast<EditRectangle*>(geometry);
        rectangle->bottomRight.x = origin.x + decayX;
        rectangle->bottomRight.y = origin.y + decayY;
        rectangle->topLeft.x = origin.x - decayXL;
        rectangle->topLeft.y = origin.y - decayYT;
    };

    updateCenterCircle(visibleGeometry.at((curveid_ - 1) * 7));
    updateCenterCircle(mouseOverGeometry.at((curveid_ - 1) * 7));

    updateEllipse(visibleGeometry.at((curveid_ - 1) * 7 + 1));
    updateEllipse(mouseOverGeometry.at((curveid_ - 1) * 7 + 1));

    updateRectangle(visibleGeometry.at((curveid_ - 1) * 7 + 2));
    updateRectangle(mouseOverGeometry.at((curveid_ - 1) * 7 + 2));

    updateSelectionCircle(visibleGeometry.at((curveid_ - 1) * 7 + 3), decayX, 0.);
    updateSelectionCircle(mouseOverGeometry.at((curveid_ - 1) * 7 + 3), decayX, 0.);

    updateSelectionCircle(visibleGeometry.at((curveid_ - 1) * 7 + 4), -decayXL, 0.);
    updateSelectionCircle(mouseOverGeometry.at((curveid_ - 1) * 7 + 4), -decayXL, 0.);

    updateSelectionCircle(visibleGeometry.at((curveid_ - 1) * 7 + 5), 0., decayY);
    updateSelectionCircle(mouseOverGeometry.at((curveid_ - 1) * 7 + 5), 0., decayY);

    updateSelectionCircle(visibleGeometry.at((curveid_ - 1) * 7 + 6), 0., -decayYT);
    updateSelectionCircle(mouseOverGeometry.at((curveid_ - 1) * 7 + 6), 0., -decayYT);

    // Update Arcellipse/Rectangle visibility according to shape and visibility
    if (isvisible_) {
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7)->setActive(true); // centerCircle
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 3)->setActive(true); // cirX
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 4)->setActive(true); // cirXL
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 5)->setActive(true); // cirY
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 6)->setActive(true); // cirYT

        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7)->setActive(true); // centerCircle
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 3)->setActive(true); // cirX
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 4)->setActive(true); // cirXL
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 5)->setActive(true); // cirY
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 6)->setActive(true); // cirYT

        if (shape_ == 0) { // 0 = Ellipse
            EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 1)->setActive(true); // shape_ellipse
            EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 2)->setActive(false); // shape_rectangle

            EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 1)->setActive(true); // shape_ellipse
            EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 2)->setActive(false); // shape_rectangle
        } else { // 1 = Rectangle
            EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 1)->setActive(false); // shape_ellipse
            EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 2)->setActive(true); // shape_rectangle

            EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 1)->setActive(false); // shape_ellipse
            EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 2)->setActive(true); // shape_rectangle
        }
    } else {
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7)->setActive(false); // centerCircle
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 1)->setActive(false); // shape_ellipse
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 2)->setActive(false); // shape_rectangle
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 3)->setActive(false); // cirX
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 4)->setActive(false); // cirXL
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 5)->setActive(false); // cirY
        EditSubscriber::visibleGeometry.at((curveid_ - 1) * 7 + 6)->setActive(false); // cirYT

        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7)->setActive(false); // centerCircle
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 1)->setActive(false); // shape_ellipse
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 2)->setActive(false); // shape_rectangle
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 3)->setActive(false); // cirX
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 4)->setActive(false); // cirXL
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 5)->setActive(false); // cirY
        EditSubscriber::mouseOverGeometry.at((curveid_ - 1) * 7 + 6)->setActive(false); // cirYT
    }
}

void ControlSpotPanel::deleteControlSpotCurve(Gtk::TreeModel::Row& row)
{
    const int curveid_ = row[spots_.curveid];

    // printf("deleteControlSpotCurve: %d\n", curveid_);

    if (curveid_ == 0) { // Row has no associated curve
        return;
    }

    // visibleGeometry
    for (int i = 6; i >= 0; i--) {
        delete *(EditSubscriber::visibleGeometry.begin() + (curveid_ - 1) * 7 + i);
        EditSubscriber::visibleGeometry.erase(EditSubscriber::visibleGeometry.begin() + (curveid_ - 1) * 7 + i);
    }

    // mouseOverGeometry
    for (int i = 6; i >= 0; i--) {
        delete *(EditSubscriber::mouseOverGeometry.begin() + (curveid_ - 1) * 7 + i);
        EditSubscriber::mouseOverGeometry.erase(EditSubscriber::mouseOverGeometry.begin() + (curveid_ - 1) * 7 + i);
    }

    row[spots_.curveid] = 0; // Reset associated curve id

    // Reordering curve id
    const Gtk::TreeModel::Children children = treemodel_->children();

    for (auto iter = children.begin(); iter != children.end(); iter++) {
        Gtk::TreeModel::Row r = *iter;

        if (r[spots_.curveid] > curveid_) {
            r[spots_.curveid] = r[spots_.curveid] - 1;
        }
    }
}

void ControlSpotPanel::updateCurveOpacity(const Gtk::TreeModel::Row& selectedRow)
{
    const int curveid_ = selectedRow[spots_.curveid];

    // printf("updateCurveOpacity: %d\n", curveid_);

    if (curveid_ == 0) { // Row has no associated curve
        return;
    }

    for (int it_ = 0; it_ < (int) EditSubscriber::visibleGeometry.size(); it_++) {
        if ((it_ < ((curveid_ - 1) * 7)) || (it_ > ((curveid_ - 1) * 7) + 6)) { // it_ does not belong to selected curve
            EditSubscriber::visibleGeometry.at(it_)->opacity = 25.;
        } else {
            EditSubscriber::visibleGeometry.at(it_)->opacity = 75.;
        }
    }
}

CursorShape ControlSpotPanel::getCursor(int objectID, int xPos, int yPos) const
{
    // printf("Object ID: %d\n", objectID);

    // When there is no control spot (i.e. no selected row), objectID can unexpectedly be different from -1 and produced not desired behavior
    const auto s = treeview_->get_selection();

    if (!s->count_selected_rows()) {
        return CSHandOpen;
    }

    const int rem_ = objectID % 7;

    switch (rem_) {
        case (0): // centerCircle: (curveid_ - 1) * 7
            return CSMove2D;

        case (1): // shape_ellipse: (curveid_ - 1) * 7 + 1
            return CSMove2D;

        case (2): // shape_rectangle: (curveid_ - 1) * 7 + 2
            return CSMove2D;

        case (3): // cirX: (curveid_ - 1) * 7 + 3
            return CSMove1DH;

        case (4): // cirXL: (curveid_ - 1) * 7 + 4
            return CSMove1DH;

        case (5): // cirY: (curveid_ - 1) * 7 + 5
            return CSMove1DV;

        case (6): // cirYT: (curveid_ - 1) * 7 + 6
            return CSMove1DV;

        default:
            return CSHandOpen;
    }
}

bool ControlSpotPanel::mouseOver(int modifierKey)
{
    EditDataProvider* editProvider_ = getEditProvider();
    const auto s = treeview_->get_selection();

    if (!editProvider_ || !s->count_selected_rows()) { // When there is no control spot (i.e. no selected row), objectID can unexpectedly be different from -1 and produced not desired behavior
        return false;
    }

    // Get selected row
    const auto selIter = s->get_selected();
    const Gtk::TreeModel::Row selRow = *selIter;

    const int object_ = editProvider_->object;

    if (object_ != lastObject_) {
        if (object_ == -1) {
            // Reset mouseOver preview for visibleGeometry
            for (size_t it_ = 0; it_ < EditSubscriber::visibleGeometry.size(); it_++) {
                EditSubscriber::visibleGeometry.at(it_)->state = Geometry::NORMAL;
            }

            // Reset mouseOver preview for TreeView
            const Gtk::TreeModel::Children children = treemodel_->children();

            for (auto iter = children.begin(); iter != children.end(); iter++) {
                Gtk::TreeModel::Row row = *iter;
                row[spots_.mouseover] = false;
            }

            // Actualize lastObject_
            lastObject_ = object_;
            return false;
        }

        const int curveId_ = object_ / 7 + 1;
        const int rem = object_ % 7;

        // Manage mouseOver preview for TreeView
        const Gtk::TreeModel::Children children = treemodel_->children();

        for (auto iter = children.begin(); iter != children.end(); iter++) {
            Gtk::TreeModel::Row row = *iter;

            if (row[spots_.curveid] == curveId_ && *row != *selRow) {
                row[spots_.mouseover] = true;
            } else {
                row[spots_.mouseover] = false;
            }
        }

        for (int it_ = 0; it_ < (int) EditSubscriber::visibleGeometry.size(); it_++) {
            if ((it_ < ((curveId_ - 1) * 7)) || (it_ > ((curveId_ - 1) * 7) + 6)) { // it_ does not belong to cursor pointed curve
                EditSubscriber::visibleGeometry.at(it_)->state = Geometry::NORMAL;
            }
        }

        const int method = shapeMethod_->get_active_row_number();

        // Circle, Arcellipses and Rectangle
        if (rem >= 0 && rem < 3) {
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7)->state = Geometry::PRELIGHT;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 1)->state = Geometry::PRELIGHT;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 2)->state = Geometry::PRELIGHT;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 3)->state = Geometry::PRELIGHT;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 4)->state = Geometry::PRELIGHT;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 5)->state = Geometry::PRELIGHT;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 6)->state = Geometry::PRELIGHT;
        } else {
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7)->state = Geometry::NORMAL;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 2)->state = Geometry::NORMAL;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 3)->state = Geometry::NORMAL;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 4)->state = Geometry::NORMAL;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 4)->state = Geometry::NORMAL;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 5)->state = Geometry::NORMAL;
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 6)->state = Geometry::NORMAL;
        }

        // cirX
        if (rem == 3) {
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 3)->state = Geometry::PRELIGHT;

            if (method == 1 || method == 3) { // Symmetrical cases
                EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 4)->state = Geometry::PRELIGHT;
            }
        }

        // cirXL
        if (rem == 4) {
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 4)->state = Geometry::PRELIGHT;

            if (method == 1 || method == 3) { // Symmetrical cases
                EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 3)->state = Geometry::PRELIGHT;
            }
        }

        // cirY
        if (rem == 5) {
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 5)->state = Geometry::PRELIGHT;

            if (method == 1 || method == 3) { // Symmetrical cases
                EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 6)->state = Geometry::PRELIGHT;
            }
        }

        // cirYT
        if (rem == 6) {
            EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 6)->state = Geometry::PRELIGHT;

            if (method == 1 || method == 3) { // Symmetrical cases
                EditSubscriber::visibleGeometry.at((curveId_ - 1) * 7 + 5)->state = Geometry::PRELIGHT;
            }
        }

        lastObject_ = object_;
        return true;
    }

    return false;
}

bool ControlSpotPanel::button1Pressed(int modifierKey)
{
    // printf("button1Pressed\n");

    EditDataProvider *provider = getEditProvider();
    const auto s = treeview_->get_selection();

    if (!provider || lastObject_ == -1 || !s->count_selected_rows()) { // When there is no control spot (i.e. no selected row), objectID can unexpectedly be different from -1 and produced not desired behavior
        return false;
    }

    // Select associated control spot
    const int curveId_ = lastObject_ / 7 + 1;
    Gtk::TreeModel::Children children = treemodel_->children();

    for (auto iter = children.begin(); iter != children.end(); iter++) {
        const Gtk::TreeModel::Row r = *iter;

        if (r[spots_.curveid] == curveId_) {
            treeview_->set_cursor(treemodel_->get_path(r));
            break;
        }
    }

    lastCoord_.set(provider->posImage.x + provider->deltaImage.x, provider->posImage.y + provider->deltaImage.y);
    EditSubscriber::action = EditSubscriber::Action::DRAGGING;
    return true;
}

bool ControlSpotPanel::button1Released()
{
    // printf("button1Released\n");
    EditSubscriber::action = EditSubscriber::Action::NONE;
    return true;
}

bool ControlSpotPanel::drag1(int modifierKey)
{
    // printf("drag1\n");

    EditDataProvider *provider = getEditProvider();
    const auto s = treeview_->get_selection();

    if (!provider || lastObject_ == -1 || !s->count_selected_rows()) { // When there is no control spot (i.e. no selected row), objectID can unexpectedly be different from -1 and produced not desired behavior
        return false;
    }

    const auto iter = s->get_selected();
    Gtk::TreeModel::Row row = *iter;

    int imW, imH;
    provider->getImageSize(imW, imH);
    const int rem = lastObject_ % 7;
    const int method = shapeMethod_->get_active_row_number();
    Coord newCoord = Coord(provider->posImage.x + provider->deltaImage.x, provider->posImage.y + provider->deltaImage.y);

    // Circle, Ellipses and Rectangle
    if (rem >= 0 && rem < 3) {
        double deltaX = (double (newCoord.x) - double (lastCoord_.x)) * 2000. / double (imW);
        double deltaY = (double (newCoord.y) - double (lastCoord_.y)) * 2000. / double (imH);
        centerX_->setValue(centerX_->getValue() + deltaX);
        centerY_->setValue(centerY_->getValue() + deltaY);
        row[spots_.centerX] = centerX_->getIntValue();
        row[spots_.centerY] = centerY_->getIntValue();

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotCenter, "X=" + centerX_->getTextValue() + ", Y=" + centerY_->getTextValue());
        }
    }

    // cirX
    if (rem == 3) {
        double deltaX = (double (newCoord.x) - double (lastCoord_.x)) * 2000. / double (imW);
        locX_->setValue(locX_->getValue() + deltaX);
        row[spots_.locX] = locX_->getIntValue();

        if (method == 1 || method == 3) { // Symmetrical cases
            disableParamlistener(true);
            locXL_->setValue(locX_->getValue());
            disableParamlistener(false);
            row[spots_.locXL] = locXL_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocX, locX_->getTextValue());
        }
    }

    // cirXL
    if (rem == 4) {
        double deltaXL = (double (lastCoord_.x) - double (newCoord.x)) * 2000. / double (imW);
        locXL_->setValue(locXL_->getValue() + deltaXL);
        row[spots_.locXL] = locXL_->getIntValue();

        if (method == 1 || method == 3) { // Symmetrical cases
            disableParamlistener(true);
            locX_->setValue(locXL_->getValue());
            disableParamlistener(false);
            row[spots_.locX] = locX_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocXL, locXL_->getTextValue());
        }
    }

    // cirY
    if (rem == 5) {
        double deltaY = (double (newCoord.y) - double (lastCoord_.y)) * 2000. / double (imH);
        locY_->setValue(locY_->getValue() + deltaY);
        row[spots_.locY] = locY_->getIntValue();

        if (method == 1 || method == 3) { // Symmetrical cases
            disableParamlistener(true);
            locYT_->setValue(locY_->getValue());
            disableParamlistener(false);
            row[spots_.locYT] = locYT_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocY, locY_->getTextValue());
        }
    }

    // cirYT
    if (rem == 6) {
        double deltaYT = (double (lastCoord_.y) - double (newCoord.y)) * 2000. / double (imH);
        locYT_->setValue(locYT_->getValue() + deltaYT);
        row[spots_.locYT] = locYT_->getIntValue();

        if (method == 1 || method == 3) { // Symmetrical cases
            disableParamlistener(true);
            locY_->setValue(locYT_->getValue());
            disableParamlistener(false);
            row[spots_.locY] = locY_->getIntValue();
        }

        updateControlSpotCurve(row);

        if (listener) {
            listener->panelChanged(EvLocallabSpotLocYT, locYT_->getTextValue());
        }
    }

    lastCoord_.set(newCoord.x, newCoord.y);
    return true;
}

int ControlSpotPanel::getEventType()
{
    const int tmp = eventType;
    eventType = None; // Re-initialization at "None" if event type gotten
    return tmp;
}

std::unique_ptr<ControlSpotPanel::SpotRow> ControlSpotPanel::getSpot(const int index)
{
    // TODO: Return an std::optional<ControlSpotPanel::SpotRow> after upgrading
    // to C++17.

    // printf("getSpot: %d\n", index);

    MyMutex::MyLock lock(mTreeview);

    std::unique_ptr<SpotRow> r(new SpotRow());

    int i = -1;

    for (auto &row : treemodel_->children()) {
        i++;

        if (i == index) {
            r->name = row[spots_.name];
            r->isvisible = row[spots_.isvisible];
            r->prevMethod = row[spots_.prevMethod];
            r->shape = row[spots_.shape];
            r->spotMethod = row[spots_.spotMethod];
//           r->mergeMethod = row[spots_.mergeMethod];
            r->sensiexclu = row[spots_.sensiexclu];
            r->structexclu = row[spots_.structexclu];
            r->struc = row[spots_.struc];
            r->shapeMethod = row[spots_.shapeMethod];
            r->locX = row[spots_.locX];
            r->locXL = row[spots_.locXL];
            r->locY = row[spots_.locY];
            r->locYT = row[spots_.locYT];
            r->centerX = row[spots_.centerX];
            r->centerY = row[spots_.centerY];
            r->circrad = row[spots_.circrad];
            r->qualityMethod = row[spots_.qualityMethod];
            r->complexMethod = row[spots_.complexMethod];
            r->transit = row[spots_.transit];
            r->feather = row[spots_.feather];
            r->thresh = row[spots_.thresh];
            r->iter = row[spots_.iter];
            r->balan = row[spots_.balan];
            r->balanh = row[spots_.balanh];
            r->colorde = row[spots_.colorde];
            r->colorscope = row[spots_.colorscope];
            r->avoidrad = row[spots_.avoidrad];
            r->transitweak = row[spots_.transitweak];
            r->transitgrad = row[spots_.transitgrad];
            r->scopemask = row[spots_.scopemask];
            r->denoichmask = row[spots_.denoichmask];
            r->lumask = row[spots_.lumask];
            r->hishow = row[spots_.hishow];
            r->activ = row[spots_.activ];
            r->avoidneg = row[spots_.avoidneg];
            r->blwh = row[spots_.blwh];
            r->recurs = row[spots_.recurs];
            r->laplac = row[spots_.laplac];
            r->deltae = row[spots_.deltae];
            r->shortc = row[spots_.shortc];
            //r->savrest = row[spots_.savrest];
            r->wavMethod = row[spots_.wavMethod];
            r->avoidgamutMethod = row[spots_.avoidgamutMethod];

            return r;
        }
    }

    return nullptr;
}

int ControlSpotPanel::getSpotNumber()
{
    // printf("getSpotNumber\n");

    return (int)treemodel_->children().size();
}

int ControlSpotPanel::getSelectedSpot()
{
    // printf("getSelectedSpot\n");

    MyMutex::MyLock lock(mTreeview);

    const auto s = treeview_->get_selection();

    // Check if treeview has row, otherwise return 0
    if (!s->count_selected_rows()) {
        return -1;
    }

    const auto selRow = s->get_selected();

    // Get selected spot index
    int index = -1;

    for (auto i : treemodel_->children()) {
        index++;

        if (selRow == i) {
            return index;
        }
    }

    return -1;
}

bool ControlSpotPanel::setSelectedSpot(const int index)
{
    // printf("setSelectedSpot: %d\n", index);

    MyMutex::MyLock lock(mTreeview);

    int i = -1;

    for (auto &row : treemodel_->children()) {
        i++;

        if (i == index) {
            disableParamlistener(true);

            treeview_->set_cursor(treemodel_->get_path(row));
            load_ControlSpot_param();
            updateParamVisibility();
            updateCurveOpacity(row);

            disableParamlistener(false);

            return true;
        }
    }

    return false;
}

bool ControlSpotPanel::isDeltaEPrevActive()
{
    return (preview_->get_active());
}

void ControlSpotPanel::resetDeltaEPreview()
{
    previewConn_.block(true);
    preview_->set_active(false);
    previewConn_.block(false);
}

void ControlSpotPanel::addControlSpot(const SpotRow &newSpot)
{
    // printf("addControlSpot: %d\n", newSpot.name);

    MyMutex::MyLock lock(mTreeview);

    disableParamlistener(true);
    Gtk::TreeModel::Row row = *(treemodel_->append());
    row[spots_.mouseover] = false;
    row[spots_.name] = newSpot.name;
    row[spots_.isvisible] = newSpot.isvisible;
    row[spots_.curveid] = 0; // No associated curve
    row[spots_.prevMethod] = newSpot.prevMethod;
    row[spots_.shape] = newSpot.shape;
    row[spots_.spotMethod] = newSpot.spotMethod;
    row[spots_.sensiexclu] = newSpot.sensiexclu;
    row[spots_.structexclu] = newSpot.structexclu;
    row[spots_.shapeMethod] = newSpot.shapeMethod;
    row[spots_.locX] = newSpot.locX;
    row[spots_.locXL] = newSpot.locXL;
    row[spots_.locY] = newSpot.locY;
    row[spots_.locYT] = newSpot.locYT;
    row[spots_.centerX] = newSpot.centerX;
    row[spots_.centerY] = newSpot.centerY;
    row[spots_.circrad] = newSpot.circrad;
    row[spots_.qualityMethod] = newSpot.qualityMethod;
    row[spots_.transit] = newSpot.transit;
    row[spots_.transitweak] = newSpot.transitweak;
    row[spots_.transitgrad] = newSpot.transitgrad;
    row[spots_.feather] = newSpot.feather;
    row[spots_.struc] = newSpot.struc;
    row[spots_.thresh] = newSpot.thresh;
    row[spots_.iter] = newSpot.iter;
    row[spots_.balan] = newSpot.balan;
    row[spots_.balanh] = newSpot.balanh;
    row[spots_.colorde] = newSpot.colorde;
    row[spots_.colorscope] = newSpot.colorscope;
    row[spots_.avoidrad] = newSpot.avoidrad;
    row[spots_.hishow] = newSpot.hishow;
    row[spots_.activ] = newSpot.activ;
    row[spots_.avoidneg] = newSpot.avoidneg;
    row[spots_.blwh] = newSpot.blwh;
    row[spots_.recurs] = newSpot.recurs;
    row[spots_.laplac] = newSpot.laplac;
    row[spots_.deltae] = newSpot.deltae;
    row[spots_.scopemask] = newSpot.scopemask;
    row[spots_.denoichmask] = newSpot.denoichmask;
    row[spots_.shortc] = newSpot.shortc;
    row[spots_.lumask] = newSpot.lumask;
    //row[spots_.savrest] = newSpot.savrest;
    row[spots_.complexMethod] = newSpot.complexMethod;
    row[spots_.wavMethod] = newSpot.wavMethod;
    row[spots_.avoidgamutMethod] = newSpot.avoidgamutMethod;
    updateParamVisibility();
    disableParamlistener(false);

    // Add associated control spot curve
    addControlSpotCurve(row);
    updateControlSpotCurve(row);
}

void ControlSpotPanel::deleteControlSpot(const int index)
{
    // printf("deleteControlSpot: %d\n", index);

    MyMutex::MyLock lock(mTreeview);

    disableParamlistener(true);

    int i = -1;

    for (auto iter : treemodel_->children()) {
        i++;

        if (i == index) {
            Gtk::TreeModel::Row row = *iter;
            deleteControlSpotCurve(row);
            treemodel_->erase(*row);
            break;
        }
    }

    disableParamlistener(false);
}

//new function linked to Global and options 
void ControlSpotPanel::updateguiset(int spottype, bool iscolor, bool issh, bool isvib, bool isexpos, bool issoft, bool isblur, bool istom, bool isret, bool issharp, bool iscont, bool iscbdl, bool islog, bool ismas, bool isci)
{
    {  //with this function we can 1) activate Settings SpotMethod
        // also if need GUI for mask ,  todo...
        idle_register.add(
        [this, spottype, iscolor, issh , isvib, isexpos, issoft, isblur, istom, isret, issharp, iscont, iscbdl, islog, ismas, isci]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();
            if(spottype >= 2  && App::get().options().spotmet >= 2) {//optimize update
                spotMethodChanged();
            }
            
            if((iscolor || issh || isvib || isexpos || istom || iscont || islog || ismas || isci)
                && !issharp && !issoft && !isret && !isblur  & !iscbdl) {
                preview_->hide();               
            } else if (issoft || isblur || isret || issharp || iscbdl) {
                preview_->show(); 
            }
            enableListener();

        return false;
        }
        );
    }
   
}
//new function linked to change scope
void ControlSpotPanel::updateguiscopeset(int scope)
{
    {  //with this function we can disabled old values scope 
        idle_register.add(
        [this, scope]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            disableListener();
            colorscope_->setValue(scope);
            adjusterChanged(colorscope_, 0.);
            enableListener();

        return false;
        }
        );
    }
   
}


void ControlSpotPanel::setDefaults(const rtengine::procparams::ProcParams * defParams, const ParamsEdited * pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot defSpot = defParams->locallab.spots.at(index);

        // Set default values for adjuster widgets
        sensiexclu_->setDefault((double)defSpot.sensiexclu);
        structexclu_->setDefault((double)defSpot.structexclu);
        locX_->setDefault((double)defSpot.loc.at(0));
        locXL_->setDefault((double)defSpot.loc.at(1));
        locY_->setDefault((double)defSpot.loc.at(2));
        locYT_->setDefault((double)defSpot.loc.at(3));
        centerX_->setDefault((double)defSpot.centerX);
        centerY_->setDefault((double)defSpot.centerY);
        circrad_->setDefault((double)defSpot.circrad);
        transit_->setDefault(defSpot.transit);
        transitweak_->setDefault(defSpot.transitweak);
        transitgrad_->setDefault(defSpot.transitgrad);
        feather_->setDefault(defSpot.feather);
        struc_->setDefault(defSpot.struc);
        thresh_->setDefault(defSpot.thresh);
        iter_->setDefault(defSpot.iter);
        balan_->setDefault(defSpot.balan);
        balanh_->setDefault(defSpot.balanh);
        colorde_->setDefault(defSpot.colorde);
        colorscope_->setDefault(defSpot.colorscope);
        avoidrad_->setDefault(defSpot.avoidrad);
        scopemask_->setDefault((double)defSpot.scopemask);
        denoichmask_->setDefault((double)defSpot.denoichmask);
        lumask_->setDefault((double)defSpot.lumask);
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

//-----------------------------------------------------------------------------
// ControlSpots
//-----------------------------------------------------------------------------

ControlSpotPanel::ControlSpots::ControlSpots()
{
    add(mouseover);
    add(name);
    add(isvisible);
    add(curveid);
    add(prevMethod);
    add(shape);
    add(spotMethod);
    add(sensiexclu);
    add(structexclu);
    add(shapeMethod);
    add(locX);
    add(locXL);
    add(locYT);
    add(locY);
    add(centerX);
    add(centerY);
    add(circrad);
    add(qualityMethod);
    add(transit);
    add(transitweak);
    add(transitgrad);
    add(feather);
    add(struc);
    add(thresh);
    add(iter);
    add(balan);
    add(balanh);
    add(colorde);
    add(colorscope);
    add(avoidrad);
    add(hishow);
    add(activ);
    add(avoidneg);
    add(blwh);
    add(recurs);
    add(laplac);
    add(deltae);
    add(scopemask);
    add(denoichmask);
    add(shortc);
    add(lumask);
    //add(savrest);
    add(complexMethod);
    add(wavMethod);
	add(avoidgamutMethod);
}

//-----------------------------------------------------------------------------
// RenameDialog
//-----------------------------------------------------------------------------

ControlSpotPanel::RenameDialog::RenameDialog(const Glib::ustring &actualname, Gtk::Window &parent):
    Gtk::Dialog(M("TP_LOCALLAB_REN_DIALOG_NAME"), parent),

    newname_(Gtk::manage(new Gtk::Entry()))
{
    // Entry widget
    Gtk::Box* const hb = Gtk::manage(new Gtk::Box());
    hb->pack_start(*Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_REN_DIALOG_LAB"))), false, false, 4);
    newname_->set_text(actualname);
    hb->pack_start(*newname_);
    get_content_area()->pack_start(*hb, Gtk::PACK_SHRINK, 4);

    // OK/CANCEL buttons
    add_button(M("GENERAL_OK"), OkButton);
    add_button(M("GENERAL_CANCEL"), CancelButton);

    // Set OK button as default one when pressing enter
    newname_->set_activates_default();
    set_default_response(OkButton);

    show_all_children();
}

Glib::ustring ControlSpotPanel::RenameDialog::get_new_name()
{
    return newname_->get_text();
}
