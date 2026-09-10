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
#include <iomanip>

#include "icmpanel.h"

#include "editcallbacks.h"
#include "eventmapper.h"
#include "guiutils.h"
#include "labgrid.h"
#include "options.h"
#include "pathutils.h"
#include "rtimage.h"
#include "widgets/curves/curveeditor.h"
#include "widgets/curves/curveeditorgroup.h"
#include "widgets/basic/myexpander.h"

#include "rtengine/dcp.h"
#include "rtengine/iccstore.h"
#include "rtengine/procparams.h"
#include "rtengine/utils.h"
#include "rtengine/rt_math.h"
#include "labgrid.h"

#include <unistd.h>

using namespace rtengine;
using namespace rtengine::procparams;

const Glib::ustring ICMPanel::TOOL_NAME = "icm";

ICMPanel::ICMPanel() : FoldableToolPanel(this, TOOL_NAME, M("TP_ICM_LABEL")), iunchanged(nullptr), icmplistener(nullptr)
{    
    CurveListener::setMulti(true);
    auto m = ProcEventMapper::getInstance();
    EvICMprimariMethod = m->newEvent(GAMMA, "HISTORY_MSG_ICM_OUTPUT_PRIMARIES");
    EvICMprofileMethod = m->newEvent(GAMMA, "HISTORY_MSG_ICM_OUTPUT_TYPE");
    EvICMtempMethod = m->newEvent(GAMMA, "HISTORY_MSG_ICM_OUTPUT_TEMP");
    //EvICMpredx = m->newEvent(GAMMA, "HISTORY_MSG_ICMPREDX");
    //EvICMpredy = m->newEvent(GAMMA, "HISTORY_MSG_ICMPREDY");
    //EvICMpgrex = m->newEvent(GAMMA, "HISTORY_MSG_ICMPGREX");
    //EvICMpgrey = m->newEvent(GAMMA, "HISTORY_MSG_ICMPGREY");
    //EvICMpblux = m->newEvent(GAMMA, "HISTORY_MSG_ICMPBLUX");
    //EvICMpbluy = m->newEvent(GAMMA, "HISTORY_MSG_ICMPBLUY");
    EvICMgamm = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WORKING_GAMMA");
    EvICMslop = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WORKING_SLOPE");
    EvICMtrcinMethod = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WORKING_TRC_METHOD");
    EvICMwillMethod = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WORKING_ILLUM_METHOD");
    EvICMwprimMethod = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WORKING_PRIM_METHOD");
    EvICMredx = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_REDX");
    EvICMredy = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_REDY");
    EvICMgrex = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_GREX");
    EvICMgrey = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_GREY");
    EvICMblux = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_BLUX");
    EvICMbluy = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_BLUY");
    EvICMredrot = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_REDROT");
    EvICMredsat = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_REDSAT");
    EvICMgrerot = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_GREROT");
    EvICMgresat = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_GRESAT");
    EvICMblurot = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_BLUROT");
    EvICMblusat = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_BLUSAT");
    
    EvaIntent = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_AINTENT");
    EvICMpreser = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_PRESER");
    EvICMLabGridciexy = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICL_LABGRIDCIEXY");
    EvICMfbw = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_FBW");
    EvICMgamut = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_GAMUT");
    EvICMcat = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_CAT");
    EvICMrefi = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_REFI");
    EvICMtrcExp = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_TRCEXP");
    EvICMshiftx = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_SHIFTX");
    EvICMshifty = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_SHIFTY");
    EvICMwmidtcie = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_MIDTCIE");
    EvICMwsmoothcie = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_SMOOTHCIE");
    EvICMwapsat = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_APSAT");
    EvICMwsmoothciesli = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_SMOOTHCIESLI");
    EvICMsigmatrc = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_SIGMATRC");
    EvICMoffstrc = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_OFFSTRC");
    EvICMopacityWLI  = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_OPACITYW");
    EvICMpyrwavtrc = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_PYRWAVTRC");
    EvICMresidtrc = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_RESIDTRC");
    EvICMwavExp = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WAVEXP");
    EvICMwgamut = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WGAMUT");
    EvICMwgampower = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WGAMUTPOWER");
    EvICMwgamgain = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ICM_WGAMUTGAIN");
    auto& options = App::get().mut_options();

    isBatchMode = lastToneCurve = lastApplyLookTable = lastApplyBaselineExposureOffset = lastApplyHueSatMap = false;

    ipDialog = Gtk::manage(new MyFileChooserButton(M("TP_ICM_INPUTDLGLABEL"), Gtk::FILE_CHOOSER_ACTION_OPEN));
    ipDialog->set_tooltip_text(M("TP_ICM_INPUTCUSTOM_TOOLTIP"));
    bindCurrentFolder(*ipDialog, options.lastIccDir);
    labgridcie = Gtk::manage(new LabGrid(EvICMLabGridciexy, M("TP_ICM_LABGRID_CIEXY"), true, true, false, true));


    // ------------------------------- Input profile


    Gtk::Frame *iFrame = Gtk::manage(new Gtk::Frame(M("TP_ICM_INPUTPROFILE")));
    iFrame->set_label_align(0.025, 0.5);

    iVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    inone = Gtk::manage(new Gtk::RadioButton(M("TP_ICM_INPUTNONE")));
    inone->set_tooltip_text(M("TP_ICM_INPUTNONE_TOOLTIP"));
    iVBox->pack_start(*inone, Gtk::PACK_SHRINK);

    iembedded = Gtk::manage(new Gtk::RadioButton(M("TP_ICM_INPUTEMBEDDED")));
    iembedded->set_tooltip_text(M("TP_ICM_INPUTEMBEDDED_TOOLTIP"));
    iVBox->pack_start(*iembedded, Gtk::PACK_SHRINK);

    icamera = Gtk::manage(new Gtk::RadioButton(M("TP_ICM_INPUTCAMERA")));
    icamera->set_tooltip_text(M("TP_ICM_INPUTCAMERA_TOOLTIP"));
    iVBox->pack_start(*icamera, Gtk::PACK_SHRINK);

    icameraICC = Gtk::manage(new Gtk::RadioButton(M("TP_ICM_INPUTCAMERAICC")));
    icameraICC->set_tooltip_text(M("TP_ICM_INPUTCAMERAICC_TOOLTIP"));
    iVBox->pack_start(*icameraICC, Gtk::PACK_SHRINK);

    ifromfile = Gtk::manage(new Gtk::RadioButton(M("TP_ICM_INPUTCUSTOM") + ":"));
    Gtk::Box* ffbox = Gtk::manage(new Gtk::Box());
    ifromfile->set_tooltip_text(M("TP_ICM_INPUTCUSTOM_TOOLTIP"));
    ffbox->pack_start(*ifromfile, Gtk::PACK_SHRINK);
    ffbox->pack_start(*ipDialog);

    iVBox->pack_start(*ffbox, Gtk::PACK_SHRINK);

    opts = icamera->get_group();
    icameraICC->set_group(opts);
    iembedded->set_group(opts);
    ifromfile->set_group(opts);
    inone->set_group(opts);

    dcpFrame = Gtk::manage(new Gtk::Frame("DCP"));
    dcpFrame->set_label_align(0.025, 0.5);

    Gtk::Grid* dcpGrid = Gtk::manage(new Gtk::Grid());
    dcpGrid->set_column_homogeneous(false);
    dcpGrid->set_row_homogeneous(false);
    dcpGrid->set_column_spacing(2);
    dcpGrid->set_row_spacing(2);

    Gtk::Grid* dcpIllGrid = Gtk::manage(new Gtk::Grid());
    dcpIllGrid->set_column_homogeneous(false);
    dcpIllGrid->set_row_homogeneous(false);
    dcpIllGrid->set_column_spacing(2);
    dcpIllGrid->set_row_spacing(2);

    dcpIllLabel = Gtk::manage(new Gtk::Label(M("TP_ICM_DCPILLUMINANT") + ":"));
    setExpandAlignProperties(dcpIllLabel, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    dcpIllLabel->set_tooltip_text(M("TP_ICM_DCPILLUMINANT_TOOLTIP"));
    dcpIllLabel->show();
    dcpIll = Gtk::manage(new MyComboBoxText());
    setExpandAlignProperties(dcpIll, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    dcpIll->set_tooltip_text(M("TP_ICM_DCPILLUMINANT_TOOLTIP"));
    dcpIll->append(M("TP_ICM_DCPILLUMINANT_INTERPOLATED"));
    dcpIll->append(M("TP_ICM_DCPILLUMINANT") + " 1");
    dcpIll->append(M("TP_ICM_DCPILLUMINANT") + " 2");
    dcpIll->show();
    dcpTemperatures[0] = 0;
    dcpTemperatures[1] = 0;
    dcpIllGrid->attach_next_to(*dcpIllLabel, Gtk::POS_LEFT, 1, 1);
    dcpIllGrid->attach_next_to(*dcpIll, *dcpIllLabel, Gtk::POS_RIGHT, 1, 1);

    ckbToneCurve = Gtk::manage(new Gtk::CheckButton(M("TP_ICM_TONECURVE")));
    ckbToneCurve->set_sensitive(false);
    ckbToneCurve->set_tooltip_text(M("TP_ICM_TONECURVE_TOOLTIP"));
    setExpandAlignProperties(ckbToneCurve, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    ckbApplyLookTable = Gtk::manage(new Gtk::CheckButton(M("TP_ICM_APPLYLOOKTABLE")));
    ckbApplyLookTable->set_sensitive(false);
    ckbApplyLookTable->set_tooltip_text(M("TP_ICM_APPLYLOOKTABLE_TOOLTIP"));
    setExpandAlignProperties(ckbApplyLookTable, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    ckbApplyHueSatMap = Gtk::manage(new Gtk::CheckButton(M("TP_ICM_APPLYHUESATMAP")));
    ckbApplyHueSatMap->set_sensitive(false);
    ckbApplyHueSatMap->set_tooltip_text(M("TP_ICM_APPLYHUESATMAP_TOOLTIP"));
    setExpandAlignProperties(ckbApplyHueSatMap, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    ckbApplyBaselineExposureOffset = Gtk::manage(new Gtk::CheckButton(M("TP_ICM_APPLYBASELINEEXPOSUREOFFSET")));
    ckbApplyBaselineExposureOffset->set_sensitive(false);
    ckbApplyBaselineExposureOffset->set_tooltip_text(M("TP_ICM_APPLYBASELINEEXPOSUREOFFSET_TOOLTIP"));
    setExpandAlignProperties(ckbApplyBaselineExposureOffset, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    dcpGrid->attach_next_to(*dcpIllGrid, Gtk::POS_BOTTOM, 1, 1);
    dcpGrid->attach_next_to(*ckbToneCurve, Gtk::POS_BOTTOM, 1, 1);
    dcpGrid->attach_next_to(*ckbApplyHueSatMap, Gtk::POS_BOTTOM, 1, 1);
    dcpGrid->attach_next_to(*ckbApplyLookTable, Gtk::POS_BOTTOM, 1, 1);
    dcpGrid->attach_next_to(*ckbApplyBaselineExposureOffset, Gtk::POS_BOTTOM, 1, 1);

    dcpFrame->add(*dcpGrid);
    dcpFrame->set_sensitive(false);
    iVBox->pack_start(*dcpFrame);

    saveRef = Gtk::manage(new Gtk::Button(M("TP_ICM_SAVEREFERENCE")));
    saveRef->set_image(*Gtk::manage(new RTImage("save-small", Gtk::ICON_SIZE_BUTTON)));
    saveRef->set_alignment(0.5f, 0.5f);
    saveRef->set_tooltip_markup(M("TP_ICM_SAVEREFERENCE_TOOLTIP"));
    iVBox->pack_start(*saveRef, Gtk::PACK_SHRINK);

    iFrame->add(*iVBox);
//    pack_start(*iFrame, Gtk::PACK_EXPAND_WIDGET);


    // ---------------------------- Working profile


    Gtk::Frame *wFrame = Gtk::manage(new Gtk::Frame(M("TP_ICM_WORKINGPROFILE")));
    wFrame->set_label_align(0.025, 0.5);

    Gtk::Box* wProfVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    wProfNames = Gtk::manage(new MyComboBoxText());
    wProfVBox->pack_start(*wProfNames, Gtk::PACK_SHRINK);

    std::vector<Glib::ustring> wpnames = rtengine::ICCStore::getInstance()->getWorkingProfiles();

    for (size_t i = 0; i < wpnames.size(); i++) {
        wProfNames->append(wpnames[i]);
    }

    wProfNames->set_active(0);

    wFrame->add(*wProfVBox);

    //-----------------gamma TRC working - Abstract Profile
    Gtk::Label *trcLabel = Gtk::manage(new Gtk::Label());
    setExpandAlignProperties(trcLabel, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    trcLabel->set_markup(escapeHtmlChars(M("TP_ICM_TRCFRAME")));
    trcLabel->set_tooltip_text(M("TP_ICM_TRCFRAME_TOOLTIP"));
    Gtk::Box *trcLabelBox = Gtk::manage(new Gtk::Box());
    trcLabelBox->add(*trcLabel);
    trcExp = Gtk::manage(new MyExpander(true, trcLabelBox));//expander Abstract Profile
    setExpandAlignProperties(trcExp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    Gtk::Box *trcProfVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    trcExp->signal_button_release_event().connect_notify(sigc::bind(sigc::mem_fun(this, &ICMPanel::foldAllButMe), trcExp, getExpander()));
    trcExpconn = trcExp->signal_enabled_toggled().connect(sigc::mem_fun(*this, &ICMPanel::trcExpChanged));
    Gtk::Box *trcPrimVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
  //  Gtk::Box *trcWavVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    ToolParamBlock* const trcWavFBox = Gtk::manage(new ToolParamBlock());

    Gtk::Box *trcWav2VBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    wavlocLabels = Gtk::manage(new Gtk::Label("---", Gtk::ALIGN_CENTER));
    wTRCBox = Gtk::manage(new Gtk::Box());

    //TRC gamma and slope
    wTRC = Gtk::manage(new MyComboBoxText());
    wTRCBox->pack_start(*wTRC, Gtk::PACK_EXPAND_WIDGET);
    trcProfVBox->pack_start(*wTRCBox, Gtk::PACK_EXPAND_WIDGET);
    wTRC->append(M("TP_ICM_WORKING_TRC_NONE"));
    wTRC->append(M("TP_ICM_WORKING_TRC_CUSTOM"));
    wTRC->append(M("TP_ICM_WORKING_TRC_BT709"));
    wTRC->append(M("TP_ICM_WORKING_TRC_SRGB"));
    wTRC->append(M("TP_ICM_WORKING_TRC_22"));
    wTRC->append(M("TP_ICM_WORKING_TRC_18"));
    wTRC->append(M("TP_ICM_WORKING_TRC_LIN"));
    wTRC->set_active(0);
    wTRC->set_tooltip_text(M("TP_ICM_TRC_TOOLTIP"));

    wFrame->set_tooltip_text(M("TP_ICM_WORKING_TRC_TOOLTIP"));



    wGamma = Gtk::manage(new Adjuster(M("TP_ICM_WORKING_TRC_GAMMA"), 0.40, 20.0, 0.001, 2.4));//default sRGB
    wSlope = Gtk::manage(new Adjuster(M("TP_ICM_WORKING_TRC_SLOPE"), 0., 300., 0.01, 12.92));//defautl sRGB
    wapsat = Gtk::manage(new Adjuster(M("TP_ICM_WORKING_TRC_SAT"), 0., 2., 0.1, 0.5));//saturation slider
    wmidtcie = Gtk::manage(new Adjuster(M("TP_LOCALLAB_MIDTCIEMAIN"), -100., 100., 1., 0.));
    wsmoothcie = Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_SMOOTHCIE")));//highlights
    wsmoothciesli = Gtk::manage(new Adjuster(M("TP_LOCALLAB_SMOOTHCIETH"), 0., 1.5, 0.01, 0.));
    trcProfVBox->pack_start(*wGamma, Gtk::PACK_SHRINK);
    wGamma->show();

    //wavelets variables
    sigmatrc = Gtk::manage(new Adjuster(M("TP_WAVELET_SIGMAFIN"), 0.025, 2.5, 0.01, 1.));
    offstrc = Gtk::manage(new Adjuster(M("TP_WAVELET_OFFSFIN"), 0.33, 1.66, 0.01, 1.));
    pyrwavtrc = Gtk::manage(new Adjuster(M("TP_WAVELET_PYRWAVTRC"), 1, 5, 1, 2));
    residtrc = Gtk::manage(new Adjuster(M("TP_WAVELET_RESIDTRC"), -100., 100., 1., 0.));
    trcmaxdata = Gtk::manage(new Gtk::Label("---"));
    rgbmaxdata = Gtk::manage(new Gtk::Label("---"));
    satmaxdata = Gtk::manage(new Gtk::Label("---"));

    //Gamut compression at the end of the process
    Gtk::Frame *gamutcompFrame = Gtk::manage(new Gtk::Frame(M("TP_ICM_COMPRGAMUT")));
    gamutcompFrame->set_label_align(0.025, 0.5);
    Gtk::Box* wgamVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    wgampower = Gtk::manage(new Adjuster(M("TP_ICM_COMP_POWER"), 0.70, 2.0, 0.01, 1.));
    Gtk::Box* wgam2VBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    wgamgain = Gtk::manage(new Adjuster(M("TP_ICM_COMP_GAIN"), -1., 2., 0.01, 0.));
    wgam2VBox->pack_start(*wgamgain, Gtk::PACK_EXPAND_WIDGET);

    wgamutBox = Gtk::manage(new Gtk::Box());
    wgamutlab = Gtk::manage(new Gtk::Label(M("TP_ICM_COMPRESS") + ":"));
    wgamutBox->pack_start(*wgamutlab, Gtk::PACK_SHRINK);
    wgamut = Gtk::manage(new MyComboBoxText());
    wgamutBox->pack_start(*wgamut, Gtk::PACK_EXPAND_WIDGET);
    wgamut->append(M("TP_ICM_COMPRESSGAMUT_NONE"));
    wgamut->append(M("TP_ICM_COMPRESSGAMUT_REC2020"));
    wgamut->append(M("TP_ICM_COMPRESSGAMUT_ADOBE"));
    wgamut->append(M("TP_ICM_COMPRESSGAMUT_SRGB"));
    wgamut->append(M("TP_ICM_COMPRESSGAMUT_DCIP3"));
    wgam2VBox->pack_start(*wgamutBox, Gtk::PACK_EXPAND_WIDGET);
    wgam2VBox->pack_start(*wgampower, Gtk::PACK_EXPAND_WIDGET);
    wgam2VBox->pack_start(*rgbmaxdata, Gtk::PACK_SHRINK);
    wgam2VBox->pack_start(*satmaxdata, Gtk::PACK_SHRINK);

    wgamVBox->pack_start(*wgam2VBox);
    gamutcompFrame->add(*wgamVBox);

    wgamut->set_active(0);
    wgamut->set_tooltip_text(M("TP_ICM_COMPRESS_TOOLTIP"));
    wgampower->set_tooltip_text(M("TP_COMPRESSGAMUT_POWER_TOOLTIP"));
    wgamgain->set_tooltip_text(M("TP_ICM_POWER_TOOLTIP"));

    //local contrast with wavelets
    opacityCurveEditorWLI = std::unique_ptr<CurveEditorGroup>(new CurveEditorGroup(options.lastIcmCurvesDir, M("TP_ICM_OPACITYWLI")));
    opacityCurveEditorWLI->setCurveListener(this);
    const ColorManagementParams default_params;

    opacityShapeWLI = static_cast<FlatCurveEditor*>(opacityCurveEditorWLI->addCurve(CT_Flat, "", nullptr, false, false));
    opacityShapeWLI->setIdentityValue(0.);
    opacityShapeWLI->setResetCurve(FlatCurveType(default_params.opacityCurveWLI.at(0)), default_params.opacityCurveWLI);
    opacityShapeWLI->setTooltip(M("TP_LOCALLAB_WAT_LEVELLOCCONTRAST_TOOLTIP"));
    opacityCurveEditorWLI->setTooltip(M("TP_WAVELET_PYRWAVTRC_CURVE_TOOLTIP"));
    // This will add the reset button at the end of the curveType buttons
    opacityCurveEditorWLI->curveListComplete();
    opacityCurveEditorWLI->show();

    trcProfVBox->pack_start(*wSlope, Gtk::PACK_SHRINK);
    wSlope->show();
    trcProfVBox->pack_start(*wapsat, Gtk::PACK_SHRINK);
    wapsat->show();

    trcProfVBox->pack_start(*wmidtcie, Gtk::PACK_SHRINK);
    wmidtcie->show();
    trcProfVBox->pack_start(*wsmoothciesli, Gtk::PACK_SHRINK);
    wsmoothciesli->show();

    wsmoothcieconn = wsmoothcie->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::wsmoothcieChanged));
    wsmoothcie->set_active(false);

    fbw = Gtk::manage(new Gtk::CheckButton((M("TP_ICM_FBW"))));
    fbw->set_active(true);
    trcProfVBox->pack_start(*fbw, Gtk::PACK_SHRINK);

    wavExp = Gtk::manage(new MyExpander(true, M("TP_ICM_WAVFRAME")));//expander Contrast Enhancement
    setExpandAlignProperties(wavExp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    wavExp->signal_button_release_event().connect_notify(sigc::bind(sigc::mem_fun(this, &ICMPanel::foldAllButMe), wavExp, trcExp));
    wavExpconn = wavExp->signal_enabled_toggled().connect(sigc::mem_fun(*this, &ICMPanel::wavExpChanged));

    wav2Exp = Gtk::manage(new MyExpander(false, M("TP_ICM_WAVREFI")));//expander Refinement wavelet
    setExpandAlignProperties(wav2Exp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    wav2Exp->signal_button_release_event().connect_notify(sigc::bind(sigc::mem_fun(this, &ICMPanel::foldAllButMe), wav2Exp, wavExp));
    trcWavFBox->pack_start(*pyrwavtrc, Gtk::PACK_SHRINK);
    trcWavFBox->pack_start(*wavlocLabels,  Gtk::PACK_SHRINK);
    pyrwavtrc->set_tooltip_text(M("TP_WAVELET_PYRWAVTRC_TOOLTIP"));
    trcWavFBox->pack_start(*opacityCurveEditorWLI, Gtk::PACK_SHRINK, 2);
    trcWav2VBox->pack_start(*sigmatrc, Gtk::PACK_SHRINK);
    trcWav2VBox->pack_start(*offstrc, Gtk::PACK_SHRINK);
    trcWav2VBox->pack_start(*residtrc, Gtk::PACK_SHRINK);
    sigmatrc->set_tooltip_text(M("TP_WAVELET_PYRWAVTRC_SIGMA_TOOLTIP"));
    residtrc->set_tooltip_text(M("TP_WAVELET_PYRWAVTRC_RESID_TOOLTIP"));
    offstrc->set_tooltip_text(M("TP_WAVELET_OFFSET_TOOLTIP"));
    primExp = Gtk::manage(new MyExpander(false, M("TP_ICM_PRIMFRAME")));
    setExpandAlignProperties(primExp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    primExp->signal_button_release_event().connect_notify(sigc::bind(sigc::mem_fun(this, &ICMPanel::foldAllButMe), primExp, trcExp));

    //Illuminants and Primaries
    willuBox = Gtk::manage(new Gtk::Box());
    willulab = Gtk::manage(new Gtk::Label(M("TP_ICM_WORKING_ILLU") + ":"));

    willuBox->pack_start(*willulab, Gtk::PACK_SHRINK);
    will = Gtk::manage(new MyComboBoxText());
    willuBox->pack_start(*will, Gtk::PACK_EXPAND_WIDGET);
    trcPrimVBox->pack_start(*willuBox, Gtk::PACK_EXPAND_WIDGET);
    will->append(M("TP_ICM_WORKING_ILLU_NONE"));
    will->append(M("TP_ICM_WORKING_ILLU_D41"));
    will->append(M("TP_ICM_WORKING_ILLU_D50"));
    will->append(M("TP_ICM_WORKING_ILLU_D55"));
    will->append(M("TP_ICM_WORKING_ILLU_D60"));
    will->append(M("TP_ICM_WORKING_ILLU_D65"));
    will->append(M("TP_ICM_WORKING_ILLU_D80"));
    will->append(M("TP_ICM_WORKING_ILLU_D120"));
    will->append(M("TP_ICM_WORKING_ILLU_STDA"));
    will->append(M("TP_ICM_WORKING_ILLU_2000"));
    will->append(M("TP_ICM_WORKING_ILLU_1500"));
    will->append(M("TP_ICM_WORKING_ILLU_E"));
    will->set_active(0);
    will->set_tooltip_text(M("TP_ICM_ILLUMPRIM_TOOLTIP"));


    wprimBox = Gtk::manage(new Gtk::Box());
    wprimlab = Gtk::manage(new Gtk::Label(M("TP_ICM_WORKING_PRIM") + ":"));

    wprimBox->pack_start(*wprimlab, Gtk::PACK_SHRINK);
    wprim = Gtk::manage(new MyComboBoxText());
    wprimBox->pack_start(*wprim, Gtk::PACK_EXPAND_WIDGET);
//    fbw = Gtk::manage(new Gtk::CheckButton((M("TP_ICM_FBW"))));
//    fbw->set_active(true);
    gamut = Gtk::manage(new Gtk::CheckButton((M("TP_ICM_GAMUT"))));
    gamut->set_active(true);

    trcPrimVBox->pack_start(*wprimBox, Gtk::PACK_EXPAND_WIDGET);
//    trcPrimVBox->pack_start(*fbw, Gtk::PACK_EXPAND_WIDGET);
//    trcProfVBox->pack_start(*gamut, Gtk::PACK_EXPAND_WIDGET);

    neutral = Gtk::manage (new Gtk::Button (M ("TP_ICM_NEUTRAL")));
    setExpandAlignProperties (neutral, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    RTImage *resetImg = Gtk::manage (new RTImage ("undo-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties (resetImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    neutral->set_image (*resetImg);
    neutralconn = neutral->signal_pressed().connect ( sigc::mem_fun (*this, &ICMPanel::neutral_pressed) );
    neutral->show();

    trcPrimVBox->pack_start (*neutral);


    wprim->append(M("TP_ICM_WORKING_PRIM_NONE"));
    wprim->append(M("TP_ICM_WORKING_PRIM_SRGB"));
    wprim->append(M("TP_ICM_WORKING_PRIM_ADOB"));
    wprim->append(M("TP_ICM_WORKING_PRIM_PROP"));
    wprim->append(M("TP_ICM_WORKING_PRIM_REC"));
    wprim->append(M("TP_ICM_WORKING_PRIM_ACE"));
    wprim->append(M("TP_ICM_WORKING_PRIM_WID"));
    wprim->append(M("TP_ICM_WORKING_PRIM_AC0"));
    wprim->append(M("TP_ICM_WORKING_PRIM_JDCMAX"));
    wprim->append(M("TP_ICM_WORKING_PRIM_JDCMAXSTDA"));
    wprim->append(M("TP_ICM_WORKING_PRIM_BRU"));
    wprim->append(M("TP_ICM_WORKING_PRIM_BET"));
    wprim->append(M("TP_ICM_WORKING_PRIM_BST"));
    wprim->append(M("TP_ICM_WORKING_PRIM_CUS"));
    wprim->append(M("TP_ICM_WORKING_PRIM_CUSGR"));
    wprim->append(M("TP_ICM_WORKING_PRIM_CUSPOL"));
    wprim->set_active(0);

    wprim->set_tooltip_text(M("TP_ICM_PRIMILLUM_TOOLTIP"));



    //Primaries as sliders
    redx = Gtk::manage(new Adjuster(M("TC_PRIM_REDX"), 0.41, 1.0, 0.0001, 0.7080));
    setExpandAlignProperties(redx, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    redy = Gtk::manage(new Adjuster(M("TC_PRIM_REDY"), 0.0, 0.70, 0.0001, 0.2920));
    setExpandAlignProperties(redy, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    grex = Gtk::manage(new Adjuster(M("TC_PRIM_GREX"), -0.1, 0.4, 0.0001, 0.1700));
    setExpandAlignProperties(grex, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    grey = Gtk::manage(new Adjuster(M("TC_PRIM_GREY"), 0.50, 1.0, 0.0001, 0.7970));
    setExpandAlignProperties(grey, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    blux = Gtk::manage(new Adjuster(M("TC_PRIM_BLUX"), -0.1, 0.4, 0.0001, 0.1310));
    setExpandAlignProperties(blux, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    bluy = Gtk::manage(new Adjuster(M("TC_PRIM_BLUY"), -0.1, 0.49, 0.0001, 0.0460));
    setExpandAlignProperties(bluy, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    //Primaries polar rotation and saturation
    redrot = Gtk::manage(new Adjuster(M("TC_PRIM_REDROT"), -10, 10, 0.01, 0));
    setExpandAlignProperties(redrot, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    redsat = Gtk::manage(new Adjuster(M("TC_PRIM_REDSAT"), -30, 30, 0.01, 0.));
    setExpandAlignProperties(redsat, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    grerot = Gtk::manage(new Adjuster(M("TC_PRIM_GREROT"), -10, 10, 0.01, 0.));
    setExpandAlignProperties(grerot, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    gresat = Gtk::manage(new Adjuster(M("TC_PRIM_GRESAT"), -30, 30, 0.01, 0.));
    setExpandAlignProperties(gresat, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    blurot = Gtk::manage(new Adjuster(M("TC_PRIM_BLUROT"), -10, 10, 0.01, 0));
    setExpandAlignProperties(blurot, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    blusat = Gtk::manage(new Adjuster(M("TC_PRIM_BLUSAT"), -20, 20, 0.01, 0.));
    setExpandAlignProperties(blusat, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    redx->set_tooltip_text(M("TP_ICM_PRIMRED_TOOLTIP"));
    grex->set_tooltip_text(M("TP_ICM_PRIMGRE_TOOLTIP"));
    blux->set_tooltip_text(M("TP_ICM_PRIMBLU_TOOLTIP"));

    redrot->set_tooltip_text(M("TP_ICM_PRIMREDROT_TOOLTIP"));

    redFrame = Gtk::manage(new Gtk::Frame(M("TP_ICM_REDFRAME")));
    redFrame->set_label_align(0.025, 0.5);

    colorFramecie = Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_COLORFRAME")));
    colorFramecie->set_label_align(0.025, 0.5);
    colorFramecie->set_tooltip_text(M("TP_LOCALLAB_PRECAMREFIMAIN_TOOLTIP"));

    Gtk::Box *redVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    primCoordGrid = Gtk::manage(new Gtk::Grid());
    primCoordGrid->set_column_homogeneous(true);
    primCoordGrid->attach(*redx, 0, 0, 1, 1);
    primCoordGrid->attach_next_to(*redy, *redx, Gtk::PositionType::POS_RIGHT, 1, 1);
    primCoordGrid->attach_next_to(*grex, *redx, Gtk::PositionType::POS_BOTTOM, 1, 1);
    primCoordGrid->attach_next_to(*grey, *grex, Gtk::PositionType::POS_RIGHT, 1, 1);
    primCoordGrid->attach_next_to(*blux, *grex, Gtk::PositionType::POS_BOTTOM, 1, 1);
    primCoordGrid->attach_next_to(*bluy, *blux, Gtk::PositionType::POS_RIGHT, 1, 1);
    redVBox->pack_start(*primCoordGrid, Gtk::PACK_EXPAND_WIDGET);

    Gtk::Separator* const separator1 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* const separator2 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    primCoordGrid2 = Gtk::manage(new Gtk::Grid());
    primCoordGrid2->set_column_homogeneous(true);
    primCoordGrid2->attach(*redrot, 0, 0, 1, 1);
    primCoordGrid2->attach_next_to(*redsat, *redrot, Gtk::PositionType::POS_RIGHT, 1, 1);
    primCoordGrid2->attach_next_to(*grerot, *redrot, Gtk::PositionType::POS_BOTTOM, 1, 1);
    primCoordGrid2->attach_next_to(*gresat, *grerot, Gtk::PositionType::POS_RIGHT, 1, 1);
    primCoordGrid2->attach_next_to(*blurot, *grerot, Gtk::PositionType::POS_BOTTOM, 1, 1);
    primCoordGrid2->attach_next_to(*blusat, *blurot, Gtk::PositionType::POS_RIGHT, 1, 1);

    preser = Gtk::manage(new Adjuster(M("TP_ICM_WORKING_PRESER"), 0., 100., 0.5, 0.));
    preser->setAdjusterListener(this);

    preBox = Gtk::manage(new Gtk::Box());
    preBox->pack_start(*preser, Gtk::PACK_EXPAND_WIDGET);
    redVBox->pack_start(*separator1, Gtk::PACK_SHRINK);
    redVBox->pack_start(*preBox, Gtk::PACK_EXPAND_WIDGET);
    redVBox->pack_start(*separator2, Gtk::PACK_SHRINK);

    cielab = Gtk::manage(new Gtk::Label(M("TP_ICM_WORKING_CIEDIAG") + ":"));

    redVBox->pack_start(*cielab, Gtk::PACK_SHRINK);
    cielab->set_tooltip_text(M("TP_ICM_WORKING_PRIMFRAME_TOOLTIP"));//I changed the location of this tooltip, placing it on the label of the graphic representation rather than on the entire frame redFrame. This improves usability.

    redVBox->pack_start(*labgridcie, Gtk::PACK_EXPAND_WIDGET, 4);
    redVBox->pack_start(*primCoordGrid2, Gtk::PACK_EXPAND_WIDGET);
    
    redVBox->pack_start(*gamut, Gtk::PACK_EXPAND_WIDGET);

    //Shift and refine color in CIE xy
    refi = Gtk::manage(new Adjuster(M("TC_PRIM_REFI"), -0.5, 1., 0.0001, 0.));
    shiftx = Gtk::manage(new Adjuster(M("TC_LOCALLAB_PRIM_SHIFTX"), -0.2, 0.2, 0.0001, 0.));
    shifty = Gtk::manage(new Adjuster(M("TC_LOCALLAB_PRIM_SHIFTY"), -0.2, 0.2, 0.0001, 0.));

    //Chromatic adaptation
    wcatBox = Gtk::manage(new Gtk::Box());
    wcatlab = Gtk::manage(new Gtk::Label(M("TP_ICM_WORKING_CAT") + ":"));
    wcatBox->pack_start(*wcatlab, Gtk::PACK_SHRINK);
    wcat = Gtk::manage(new MyComboBoxText());
    wcatBox->pack_start(*wcat, Gtk::PACK_EXPAND_WIDGET);

    wcat->append(M("TP_ICM_WORKING_CAT_BRAD"));
    wcat->append(M("TP_ICM_WORKING_CAT_CAT16"));
    wcat->append(M("TP_ICM_WORKING_CAT_CAT02"));
    wcat->append(M("TP_ICM_WORKING_CAT_VK"));
    wcat->append(M("TP_ICM_WORKING_CAT_XYZ"));
    wcat->set_active(0);
    redVBox->pack_start(*wcatBox, Gtk::PACK_SHRINK); 

    ToolParamBlock* const colorBox = Gtk::manage(new ToolParamBlock());

    colorBox->pack_start(*refi, Gtk::PACK_EXPAND_WIDGET);
    colorBox->pack_start(*shiftx, Gtk::PACK_EXPAND_WIDGET);
    colorBox->pack_start(*shifty, Gtk::PACK_EXPAND_WIDGET);
    colorFramecie->add(*colorBox); 
    redVBox->pack_start(*colorFramecie);
    redFrame->add(*redVBox);

    wGamma->setAdjusterListener(this);
    wSlope->setLogScale(16, 0);
    wSlope->setAdjusterListener(this);
    wapsat->setAdjusterListener(this);
    wmidtcie->setAdjusterListener(this);
    wsmoothciesli->setAdjusterListener(this);
    redx->setAdjusterListener(this);
    redy->setAdjusterListener(this);
    grex->setAdjusterListener(this);
    grey->setAdjusterListener(this);
    blux->setAdjusterListener(this);
    bluy->setAdjusterListener(this);

    redrot->setAdjusterListener(this);
    redsat->setAdjusterListener(this);
    grerot->setAdjusterListener(this);
    gresat->setAdjusterListener(this);
    blurot->setAdjusterListener(this);
    blusat->setAdjusterListener(this);

    refi->setAdjusterListener(this);
    shiftx->setAdjusterListener(this);
    shifty->setAdjusterListener(this);
    sigmatrc->setAdjusterListener(this);
    offstrc->setAdjusterListener(this);
    pyrwavtrc->setAdjusterListener(this);
    residtrc->setAdjusterListener(this);

    trcmaxdata->set_line_wrap();
    trcmaxdata->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(trcmaxdata, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);

    rgbmaxdata->set_line_wrap();
    rgbmaxdata->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(rgbmaxdata, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    rgbmaxdata->set_tooltip_text(M("TP_ICM_TRC_SAT_MAX_TOOLTIP"));

    satmaxdata->set_line_wrap();
    satmaxdata->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(satmaxdata, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    satmaxdata->set_tooltip_text(M("TP_ICM_TRC_SAT_MAX_TOOLTIP"));

    //wGamma->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));
    
    

    // wSlope->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));
    wmidtcie->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));
    wsmoothciesli->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));
    wav2Exp->add(*trcWav2VBox, false);
    wav2Exp->setLevel (2);
    trcWavFBox->pack_start(*wav2Exp, false, false);
    trcWavFBox->pack_start(*trcmaxdata, Gtk::PACK_SHRINK);
    trcmaxdata->set_tooltip_text(M("TP_ICM_TRC_MAX_TOOLTIP"));
    
    trcWavFBox->pack_start(*gamutcompFrame, Gtk::PACK_EXPAND_WIDGET);
    wgampower->setAdjusterListener(this);
    wgamgain->setAdjusterListener(this);

    wavExp->add(*trcWavFBox, false);
    wavExp->setLevel (2);
    trcProfVBox->pack_start(*wavExp, false, false);

    trcProfVBox->pack_start(*primExp, false, false);

    // Rendering intent
    riaHBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* abIntentLbl = Gtk::manage(new Gtk::Label(M("TP_ICM_PROFILEINTENT")));
    riaHBox->pack_start(*abIntentLbl, Gtk::PACK_SHRINK);
    aRendIntent.reset(new PopUpButton());
    aRendIntent->addEntry("intent-perceptual", M("PREFERENCES_INTENT_PERCEPTUAL"));
    aRendIntent->addEntry("intent-relative", M("PREFERENCES_INTENT_RELATIVE"));
    aRendIntent->addEntry("intent-saturation", M("PREFERENCES_INTENT_SATURATION"));
    aRendIntent->addEntry("intent-absolute", M("PREFERENCES_INTENT_ABSOLUTE"));
    aRendIntent->setSelected(1);
    aRendIntent->show();
    riaHBox->pack_start(*aRendIntent->buttonGroup, Gtk::PACK_EXPAND_PADDING);
    trcPrimVBox->pack_start(*redFrame, Gtk::PACK_EXPAND_WIDGET);

    primExp->add(*trcPrimVBox, false);
    primExp->setLevel (2);

    trcExp->add(*trcProfVBox, false);
    trcExp->show_all();
    trcExp->set_expanded(false);
    trcExp->set_no_show_all();
    trcExp->setLevel (2);
    pack_start(*trcExp, Gtk::PACK_EXPAND_WIDGET);

    pack_start(*wFrame, Gtk::PACK_EXPAND_WIDGET);
    pack_start(*iFrame, Gtk::PACK_EXPAND_WIDGET);

    // ---------------------------- Output profile


    Gtk::Frame *oFrame = Gtk::manage(new Gtk::Frame(M("TP_ICM_OUTPUTPROFILE")));
    oFrame->set_label_align(0.025, 0.5);
    oFrame->set_tooltip_text(M("TP_ICM_OUTPUTPROFILE_TOOLTIP"));

    Gtk::Box* oProfVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    oProfNames = Gtk::manage(new MyComboBoxText());
    oProfVBox->pack_start(*oProfNames, Gtk::PACK_SHRINK);

    oProfNames->append(M("TP_ICM_NOICM"));
    oProfNames->set_active(0);

    std::vector<Glib::ustring> opnames = ICCStore::getInstance()->getProfiles(rtengine::ICCStore::ProfileType::OUTPUT);

    for (size_t i = 0; i < opnames.size(); i++) {
        oProfNames->append(opnames[i]);
    }

    oProfNames->set_active(0);

    // Rendering intent
    Gtk::Box *riHBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* outputIntentLbl = Gtk::manage(new Gtk::Label(M("TP_ICM_PROFILEINTENT")));
    riHBox->pack_start(*outputIntentLbl, Gtk::PACK_SHRINK);
    oRendIntent.reset(new PopUpButton());
    oRendIntent->addEntry("intent-perceptual", M("PREFERENCES_INTENT_PERCEPTUAL"));
    oRendIntent->addEntry("intent-relative", M("PREFERENCES_INTENT_RELATIVE"));
    oRendIntent->addEntry("intent-saturation", M("PREFERENCES_INTENT_SATURATION"));
    oRendIntent->addEntry("intent-absolute", M("PREFERENCES_INTENT_ABSOLUTE"));
    oRendIntent->setSelected(1);
    oRendIntent->show();
    riHBox->pack_start(*oRendIntent->buttonGroup, Gtk::PACK_EXPAND_PADDING);
    oProfVBox->pack_start(*riHBox, Gtk::PACK_SHRINK);

    // Black Point Compensation
    obpc = Gtk::manage(new Gtk::CheckButton((M("TP_ICM_BPC"))));
    obpc->set_active(true);
    oProfVBox->pack_start(*obpc, Gtk::PACK_SHRINK);

    oFrame->add(*oProfVBox);

    pack_start(*oFrame, Gtk::PACK_EXPAND_WIDGET);

    // ---------------------------- Output gamma list entries

    Glib::RefPtr<Gtk::FileFilter> filter_icc = Gtk::FileFilter::create();
    filter_icc->set_name(M("FILECHOOSER_FILTER_COLPROF"));
    filter_icc->add_pattern("*.dcp");
    filter_icc->add_pattern("*.DCP");
    filter_icc->add_pattern("*.icc");
    filter_icc->add_pattern("*.icm");
    filter_icc->add_pattern("*.ICC");
    filter_icc->add_pattern("*.ICM");
    Glib::RefPtr<Gtk::FileFilter> filter_iccdng = Gtk::FileFilter::create();
    filter_iccdng->set_name(M("FILECHOOSER_FILTER_COLPROF") + " + DNG");
    filter_iccdng->add_pattern("*.dcp");
    filter_iccdng->add_pattern("*.DCP");
    filter_iccdng->add_pattern("*.dng");
    filter_iccdng->add_pattern("*.DNG");
    filter_iccdng->add_pattern("*.icc");
    filter_iccdng->add_pattern("*.icm");
    filter_iccdng->add_pattern("*.ICC");
    filter_iccdng->add_pattern("*.ICM");
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name(M("FILECHOOSER_FILTER_ANY"));
    filter_any->add_pattern("*");

    ipDialog->add_filter(filter_icc);
    ipDialog->add_filter(filter_iccdng);
    ipDialog->add_filter(filter_any);
#ifdef _WIN32
    ipDialog->set_show_hidden(true);  // ProgramData is hidden on Windows
#endif

    wprofnamesconn = wProfNames->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::wpChanged));
    oprofnamesconn = oProfNames->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::opChanged));
    orendintentconn = oRendIntent->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::oiChanged));
    arendintentconn = aRendIntent->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::aiChanged));
    dcpillconn = dcpIll->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::dcpIlluminantChanged));
    wtrcconn = wTRC->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::wtrcinChanged));
    willconn = will->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::willChanged));
    wprimconn = wprim->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::wprimChanged));
    wcatconn = wcat->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::wcatChanged));
    wgamutconn = wgamut->signal_changed().connect(sigc::mem_fun(*this, &ICMPanel::wgamutChanged));

    fbwconn = fbw->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::fbwChanged));
    gamutconn = gamut->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::gamutChanged));
    obpcconn = obpc->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::oBPCChanged));
    tcurveconn = ckbToneCurve->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::toneCurveChanged));
    ltableconn = ckbApplyLookTable->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::applyLookTableChanged));
    beoconn = ckbApplyBaselineExposureOffset->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::applyBaselineExposureOffsetChanged));
    hsmconn = ckbApplyHueSatMap->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::applyHueSatMapChanged));

    icamera->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::ipChanged));
    icameraICC->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::ipChanged));
    iembedded->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::ipChanged));
    ifromfile->signal_toggled().connect(sigc::mem_fun(*this, &ICMPanel::ipChanged));

    ipc = ipDialog->signal_selection_changed().connect(sigc::mem_fun(*this, &ICMPanel::ipSelectionChanged));
    saveRef->signal_pressed().connect(sigc::mem_fun(*this, &ICMPanel::saveReferencePressed));

    show_all();
}

void ICMPanel::foldAllButMe(GdkEventButton *event, MyExpander *expander, const MyExpander *parent)
{
    if (event->button == 3) {
        const auto set_expanded = [expander](MyExpander *expander_to_modify) {
            expander_to_modify->set_expanded(expander_to_modify == expander);
        };

        if (parent == getExpander()) {
            set_expanded(trcExp);
        } else if (parent == trcExp) {
            // Abstract Profile sub-expanders.
            set_expanded(wavExp);
            set_expanded(primExp);
        } else if (parent == wavExp) {
            // Contrast Enhancement sub-expanders.
            set_expanded(wav2Exp);
        }
    }
}

void ICMPanel::wavlocChanged(double nlevel, double nmax, bool curveloc)
{
    if (!batchMode) {
        idle_register.add(
        [this, nlevel, nmax, curveloc]() -> bool {
            if(nlevel != nmax && curveloc) {
                wavlocLabels->show();
                wavlocLabels->set_text(
                    Glib::ustring::compose(
                        M("TP_WAVELET_LEVLOCLABEL"),
                        Glib::ustring::format(std::fixed, std::setprecision(0), nlevel)//not use but keep in case of
                    )
                );
            } else {
                wavlocLabels->hide();
            }
            return false;
        }
        );
    }
}

void ICMPanel::neutral_pressed ()
{   //find working profile and set the same destination profile
    if (wProfNames->get_active_text() == "Rec2020") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::REC2020));
    } else if (wProfNames->get_active_text() == "sRGB") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::SRGB));
    } else if (wProfNames->get_active_text() == "Adobe RGB") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::ADOBE_RGB));
    } else if (wProfNames->get_active_text() == "ProPhoto") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::PRO_PHOTO));
    } else if (wProfNames->get_active_text() == "ACESp1") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::ACES_P1));
    } else if (wProfNames->get_active_text() == "WideGamut") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::WIDE_GAMUT));
    } else if (wProfNames->get_active_text() == "ACESp0") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::ACES_P0));
    } else if (wProfNames->get_active_text() == "JDCmax") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::JDC_MAX));
    } else if (wProfNames->get_active_text() == "JDCmax stdA") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::JDC_MAXSTDA));
    } else if (wProfNames->get_active_text() == "BruceRGB") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::BRUCE_RGB));
    } else if (wProfNames->get_active_text() == "Beta RGB") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::BETA_RGB));
    } else if (wProfNames->get_active_text() == "BestRGB") {
        wprim->set_active(toUnderlying(ColorManagementParams::Primaries::BEST_RGB));
    }
    resetpolar ();
    const ColorManagementParams defPar;
}

void ICMPanel::updateRenderingIntent(const Glib::ustring &profile)
{
    const uint8_t supportedIntents = rtengine::ICCStore::getInstance()->getOutputIntents(profile);
    const bool supportsPerceptual = supportedIntents & 1 << INTENT_PERCEPTUAL;
    const bool supportsRelative   = supportedIntents & 1 << INTENT_RELATIVE_COLORIMETRIC;
    const bool supportsSaturation = supportedIntents & 1 << INTENT_SATURATION;
    const bool supportsAbsolute   = supportedIntents & 1 << INTENT_ABSOLUTE_COLORIMETRIC;

    //printf("Intents: %d / Perceptual: %d  Relative: %d  Saturation: %d  Absolute: %d\n", supportedIntents, supportsPerceptual, supportsRelative, supportsSaturation, supportsAbsolute);

    if (!profile.empty() && (supportsPerceptual || supportsRelative || supportsSaturation || supportsAbsolute)) {
        oRendIntent->set_sensitive(true);
        oRendIntent->setItemSensitivity(0, supportsPerceptual);
        oRendIntent->setItemSensitivity(1, supportsRelative);
        oRendIntent->setItemSensitivity(2, supportsSaturation);
        oRendIntent->setItemSensitivity(3, supportsAbsolute);

        aRendIntent->set_sensitive(true);
        aRendIntent->setItemSensitivity(0, supportsPerceptual);
        aRendIntent->setItemSensitivity(1, supportsRelative);
        aRendIntent->setItemSensitivity(2, supportsSaturation);
        aRendIntent->setItemSensitivity(3, supportsAbsolute);

    } else {
        oRendIntent->setItemSensitivity(0, true);
        oRendIntent->setItemSensitivity(1, true);
        oRendIntent->setItemSensitivity(2, true);
        oRendIntent->setItemSensitivity(3, true);
        oRendIntent->set_sensitive(false);
        oRendIntent->setSelected(1);

        aRendIntent->setItemSensitivity(0, true);
        aRendIntent->setItemSensitivity(1, true);
        aRendIntent->setItemSensitivity(2, true);
        aRendIntent->setItemSensitivity(3, true);
        aRendIntent->set_sensitive(false);
        aRendIntent->setSelected(1);

    }
}

ICMPanel::~ICMPanel()
{
    idle_register.destroy();

}

void ICMPanel::primChanged (float rx, float ry, float bx, float by, float gx, float gy)
{ //update sliders R G B Ciexy
    nextrx = rx;
    nextry = ry;
    nextbx = bx;
    nextby = by;
    nextgx = gx;
    nextgy = gy;

    idle_register.add(
        [this]() -> bool
        {
            disableListener();
            redx->setValue(nextrx);
            redy->setValue(nextry);
            blux->setValue(nextbx);
            bluy->setValue(nextby);
            grex->setValue(nextgx);
            grey->setValue(nextgy);

            enableListener();
            return false;
        }
    );
}

void ICMPanel::maxdatawtrc(float m_data)
{
    idle_register.add(
    [this, m_data]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
            trcmaxdata->set_text(
                Glib::ustring::compose(M("TP_ICM_TRC_MAX"),
                    Glib::ustring::format(std::fixed, std::setprecision(3), m_data))
            );
        return false;
    }
   );
}

void ICMPanel::maxdataend(float m_rgb, float m_sat, bool gamgain)
{
    idle_register.add(
    [this, m_rgb, m_sat, gamgain]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
            if (gamgain) {
                rgbmaxdata->set_text(//RGB maximum
                    Glib::ustring::compose(M("TP_ICM_TRC_MAX_END"),
                        Glib::ustring::format(std::fixed, std::setprecision(3), m_rgb))
                );
            } else {
                rgbmaxdata->set_text(M("TP_ICM_TRC_MAX_END_NO"));
            }
            if (gamgain) {
                satmaxdata->set_text(//Saturation maximum
                    Glib::ustring::compose(M("TP_ICM_TRC_SAT_MAX"),
                        Glib::ustring::format(std::fixed, std::setprecision(3), m_sat))
                );
            } else {
                satmaxdata->set_text(M("TP_ICM_TRC_MAX_END_NO"));
            }
        return false;
    }
   );
}


void ICMPanel::iprimChanged (float r_x, float r_y, float b_x, float b_y, float g_x, float g_y, float w_x, float w_y, float m_x, float m_y)
{//update CIE xy graph
    nextrx = r_x;
    nextry = r_y;
    nextbx = b_x;
    nextby = b_y;
    nextgx = g_x;
    nextgy = g_y;
    nextwx = w_x;
    nextwy = w_y;
    nextmx = m_x;
    nextmy = m_y;

    //convert xy datas in datas for labgrid areas
    nextrx = OFFSET_MODIFIER * (nextrx + CIExy_MARGIN) - 1.f;
    nextry = OFFSET_MODIFIER * (nextry + CIExy_MARGIN) - 1.f;
    nextbx = OFFSET_MODIFIER * (nextbx + CIExy_MARGIN) - 1.f;
    nextby = OFFSET_MODIFIER * (nextby + CIExy_MARGIN) - 1.f;
    nextgx = OFFSET_MODIFIER * (nextgx + CIExy_MARGIN) - 1.f;
    nextgy = OFFSET_MODIFIER * (nextgy + CIExy_MARGIN) - 1.f;
    nextwx = OFFSET_MODIFIER * (nextwx + CIExy_MARGIN) - 1.f;
    nextwy = OFFSET_MODIFIER * (nextwy + CIExy_MARGIN) - 1.f;
    nextmx = OFFSET_MODIFIER * (nextmx + CIExy_MARGIN) - 1.f;
    nextmy = OFFSET_MODIFIER * (nextmy + CIExy_MARGIN) - 1.f;

    idle_register.add(
        [this]() -> bool
        {
            disableListener();
            labgridcie->setParams(nextrx, nextry, nextbx, nextby, nextgx, nextgy, nextwx, nextwy, nextmx, nextmy, false);
            enableListener();
            return false;
        }
    );
}


void ICMPanel::setEditProvider(EditDataProvider *provider)
{
    opacityShapeWLI->setEditProvider(provider);

}

void ICMPanel::setListener(ToolPanelListener *tpl)
{//enable listener Toolpanel and Labgridcie
        ToolPanel::setListener(tpl);
        labgridcie->setListener(tpl);
}

void ICMPanel::updateDCP(int dcpIlluminant, Glib::ustring dcp_name)
{
    ConnectionBlocker dcpillconn_(dcpillconn);

    if (isBatchMode) {
        dcpFrame->set_sensitive(true);
        ckbToneCurve->set_sensitive(true);
        ckbApplyLookTable->set_sensitive(true);
        ckbApplyBaselineExposureOffset->set_sensitive(true);
        ckbApplyHueSatMap->set_sensitive(true);
        dcpIllLabel->set_sensitive(true);
        dcpIll->set_sensitive(true);

        if (dcpTemperatures[0] != 0 || dcpTemperatures[1] != 0) {
            int curr_active = dcpIll->get_active_row_number();
            dcpIll->remove_all();
            dcpIll->append(M("TP_ICM_DCPILLUMINANT_INTERPOLATED"));
            dcpIll->append(M("TP_ICM_DCPILLUMINANT") + " 1");
            dcpIll->append(M("TP_ICM_DCPILLUMINANT") + " 2");
            dcpIll->append(M("GENERAL_UNCHANGED"));
            dcpTemperatures[0] = 0;
            dcpTemperatures[1] = 0;
            dcpIll->set_active(curr_active);
        }

        if (dcpIll->get_active_row_number() == -1 && dcpIlluminant == -1) {
            dcpIll->set_active(0);
        } else if (dcpIlluminant >= 0 && dcpIlluminant != dcpIll->get_active_row_number()) {
            dcpIll->set_active(dcpIlluminant);
        }

        dcpIll->set_sensitive(true);
        dcpIllLabel->set_sensitive(true);
        return;
    }

    ckbToneCurve->set_sensitive(false);
    ckbApplyLookTable->set_sensitive(false);
    ckbApplyBaselineExposureOffset->set_sensitive(false);
    ckbApplyHueSatMap->set_sensitive(false);
    dcpIllLabel->set_sensitive(false);
    dcpIll->set_sensitive(false);
    dcpFrame->set_sensitive(false);

    DCPProfile* dcp = nullptr;

    if (dcp_name == "(cameraICC)") {
        dcp = DCPStore::getInstance()->getStdProfile(camName);
    } else if (dcp_name == "(embedded)") {
        dcp = DCPStore::getInstance()->getProfile(filename);
    } else if (ifromfile->get_active() && DCPStore::getInstance()->isValidDCPFileName(dcp_name)) {
        dcp = DCPStore::getInstance()->getProfile(dcp_name);
    }

    if (dcp) {
        dcpFrame->set_sensitive(true);

        if (dcp->getHasToneCurve()) {
            ckbToneCurve->set_sensitive(true);
        }

        if (dcp->getHasLookTable()) {
            ckbApplyLookTable->set_sensitive(true);
        }

        if (dcp->getHasBaselineExposureOffset()) {
            ckbApplyBaselineExposureOffset->set_sensitive(true);
        }

        if (dcp->getHasHueSatMap()) {
            ckbApplyHueSatMap->set_sensitive(true);
        }

        const DCPProfile::Illuminants illuminants = dcp->getIlluminants();

        if (illuminants.will_interpolate) {
            if (dcpTemperatures[0] != illuminants.temperature_1 || dcpTemperatures[1] != illuminants.temperature_2) {
                char tempstr1[64], tempstr2[64];
                snprintf(tempstr1, sizeof(tempstr1), "%.0fK", illuminants.temperature_1);
                snprintf(tempstr2, sizeof(tempstr2), "%.0fK", illuminants.temperature_2);
                int curr_active = dcpIll->get_active_row_number();
                dcpIll->remove_all();
                dcpIll->append(M("TP_ICM_DCPILLUMINANT_INTERPOLATED"));
                dcpIll->append(tempstr1);
                dcpIll->append(tempstr2);
                dcpTemperatures[0] = illuminants.temperature_1;
                dcpTemperatures[1] = illuminants.temperature_2;
                dcpIll->set_active(curr_active);
            }

            if (dcpIlluminant > 2) {
                dcpIlluminant = 0;
            }

            if (dcpIll->get_active_row_number() == -1 && dcpIlluminant == -1) {
                dcpIll->set_active(0);
            } else if (dcpIlluminant >= 0 && dcpIlluminant != dcpIll->get_active_row_number()) {
                dcpIll->set_active(dcpIlluminant);
            }

            dcpIll->set_sensitive(true);
            dcpIllLabel->set_sensitive(true);
        } else {
            if (dcpIll->get_active_row_number() != -1) {
                dcpIll->set_active(-1);
            }
        }
    }

    if (!dcpIllLabel->get_sensitive() && dcpIll->get_active_row_number() != 0) {
        if (dcpTemperatures[0] != 0 || dcpTemperatures[1] != 0) {
            int curr_active = dcpIll->get_active_row_number();
            dcpIll->remove_all();
            dcpIll->append(M("TP_ICM_DCPILLUMINANT_INTERPOLATED"));
            dcpIll->append(M("TP_ICM_DCPILLUMINANT") + " 1");
            dcpIll->append(M("TP_ICM_DCPILLUMINANT") + " 2");

            if (isBatchMode) {
                dcpIll->append(M("GENERAL_UNCHANGED"));
            }

            dcpTemperatures[0] = 0;
            dcpTemperatures[1] = 0;
            dcpIll->set_active(curr_active);
        }
    }
}

void ICMPanel::resetpolar () {
    redrot->setValue(0.);
    redsat->setValue(0.);
    grerot->setValue(0.);
    gresat->setValue(0.);
    blurot->setValue(0.);
    blusat->setValue(0.);
}

void ICMPanel::read(const ProcParams* pp, const ParamsEdited* pedited)
{

    disableListener();

    ConnectionBlocker obpcconn_(obpcconn);
    ConnectionBlocker fbwconn_(fbwconn);
    ConnectionBlocker gamutconn_(gamutconn);
    ConnectionBlocker wsmoothcieconn_(wsmoothcieconn);
    ConnectionBlocker ipc_(ipc);
    ConnectionBlocker tcurveconn_(tcurveconn);
    ConnectionBlocker ltableconn_(ltableconn);
    ConnectionBlocker beoconn_(beoconn);
    ConnectionBlocker hsmconn_(hsmconn);
    ConnectionBlocker wprofnamesconn_(wprofnamesconn);
    ConnectionBlocker oprofnamesconn_(oprofnamesconn);
    ConnectionBlocker orendintentconn_(orendintentconn);
    ConnectionBlocker arendintentconn_(arendintentconn);
    ConnectionBlocker dcpillconn_(dcpillconn);
    ConnectionBlocker wtrcconn_(wtrcconn);
    ConnectionBlocker wgamutconn_(wgamutconn);
    ConnectionBlocker willconn_(willconn);
    ConnectionBlocker wprimconn_(wprimconn);
    ConnectionBlocker wcatconn_(wcatconn);
    ConnectionBlocker trcExpconn_(trcExpconn);
    ConnectionBlocker wavExpconn_(wavExpconn);

    if (pp->icm.inputProfile.substr(0, 5) != "file:" && !ipDialog->get_filename().empty()) {
        ipDialog->set_filename(pp->icm.inputProfile);
    }

    if (pp->icm.inputProfile == "(none)") {
        inone->set_active(true);
        updateDCP(pp->icm.dcpIlluminant, "");
    } else if (pp->icm.inputProfile == "(embedded)" || ((pp->icm.inputProfile == "(camera)" || pp->icm.inputProfile.empty()) && icamera->get_state() == Gtk::STATE_INSENSITIVE)) {
        iembedded->set_active(true);
        updateDCP(pp->icm.dcpIlluminant, "(embedded)");
    } else if ((pp->icm.inputProfile == "(cameraICC)") && icameraICC->get_state() != Gtk::STATE_INSENSITIVE) {
        icameraICC->set_active(true);
        updateDCP(pp->icm.dcpIlluminant, "(cameraICC)");
    } else if ((pp->icm.inputProfile == "(cameraICC)") && icamera->get_state() != Gtk::STATE_INSENSITIVE && icameraICC->get_state() == Gtk::STATE_INSENSITIVE) {
        // this is the case when (cameraICC) is instructed by packaged profiles, but ICC file is not found
        // therefore falling back UI to explicitly reflect the (camera) option
        icamera->set_active(true);
        updateDCP(pp->icm.dcpIlluminant, "");
    } else if ((pp->icm.inputProfile == "(cameraICC)") && icamera->get_state() == Gtk::STATE_INSENSITIVE && icameraICC->get_state() == Gtk::STATE_INSENSITIVE) {
        // If neither (camera) nor (cameraICC) are available, as is the case when loading a non-raw, activate (embedded).
        iembedded->set_active(true);
        updateDCP(pp->icm.dcpIlluminant, "(cameraICC)");
    } else if ((pp->icm.inputProfile == "(camera)" || pp->icm.inputProfile.empty()) && icamera->get_state() != Gtk::STATE_INSENSITIVE) {
        icamera->set_active(true);
        updateDCP(pp->icm.dcpIlluminant, "");
    } else {
        ifromfile->set_active(true);
        oldip = pp->icm.inputProfile.substr(5);  // cut of "file:"
        ipDialog->set_filename(pp->icm.inputProfile.substr(5));
        updateDCP(pp->icm.dcpIlluminant, pp->icm.inputProfile.substr(5));
    }

    wProfNames->set_active_text(pp->icm.workingProfile);

    wTRC->set_active(rtengine::toUnderlying(pp->icm.workingTRC));

    wgamut->set_active(rtengine::toUnderlying(pp->icm.wgamut));

    will->set_active(rtengine::toUnderlying(pp->icm.will));

    wprim->set_active(rtengine::toUnderlying(pp->icm.wprim));
    wcat->set_active(rtengine::toUnderlying(pp->icm.wcat));

    wtrcinChanged();
    wgamutChanged();
    willChanged();
    wprimChanged();
    wcatChanged();
    gamutChanged();
    wsmoothcieChanged();

    if (pp->icm.outputProfile == ColorManagementParams::NoICMString) {
        oProfNames->set_active_text(M("TP_ICM_NOICM"));
    } else {
        oProfNames->set_active_text(pp->icm.outputProfile);
    }

    if (oProfNames->get_active_row_number() == -1) {
        oProfNames->set_active_text(M("TP_ICM_NOICM"));
    }

    oRendIntent->setSelected(pp->icm.outputIntent);
    aRendIntent->setSelected(pp->icm.aRendIntent);
    opacityShapeWLI->setCurve(pp->icm.opacityCurveWLI);

    obpc->set_active(pp->icm.outputBPC);
    fbw->set_active(pp->icm.fbw);
    trcExp->setEnabled(pp->icm.trcExp);
    wavExp->setEnabled(pp->icm.wavExp);
    gamut->set_active(pp->icm.gamut);
    wsmoothcie->set_active(pp->icm.wsmoothcie);
    ckbToneCurve->set_active(pp->icm.toneCurve);
    lastToneCurve = pp->icm.toneCurve;
    ckbApplyLookTable->set_active(pp->icm.applyLookTable);
    lastApplyLookTable = pp->icm.applyLookTable;
    ckbApplyBaselineExposureOffset->set_active(pp->icm.applyBaselineExposureOffset);
    lastApplyBaselineExposureOffset = pp->icm.applyBaselineExposureOffset;
    ckbApplyHueSatMap->set_active(pp->icm.applyHueSatMap);
    lastApplyHueSatMap = pp->icm.applyHueSatMap;

  //  wGamma->setValue(pp->icm.workingTRCGamma);
  //  wSlope->setValue(pp->icm.workingTRCSlope);
    wGamma->setValue(pp->icm.wGamma);
    wSlope->setValue(pp->icm.wSlope);
    wapsat->setValue(pp->icm.wapsat);
    wmidtcie->setValue(pp->icm.wmidtcie);
    wsmoothciesli->setValue(pp->icm.wsmoothciesli);
    sigmatrc->setValue(pp->icm.sigmatrc);
    offstrc->setValue(pp->icm.offstrc);
    residtrc->setValue(pp->icm.residtrc);
    pyrwavtrc->setValue(pp->icm.pyrwavtrc);
    wgampower->setValue(pp->icm.wgampower);
    wgamgain->setValue(pp->icm.wgamgain);

    redx->setValue(pp->icm.redx);
    redy->setValue(pp->icm.redy);
    grex->setValue(pp->icm.grex);
    grey->setValue(pp->icm.grey);
    blux->setValue(pp->icm.blux);
    bluy->setValue(pp->icm.bluy);

    redrot->setValue(pp->icm.redrot);
    redsat->setValue(pp->icm.redsat);
    grerot->setValue(pp->icm.grerot);
    gresat->setValue(pp->icm.gresat);
    blurot->setValue(pp->icm.blurot);
    blusat->setValue(pp->icm.blusat);

    refi->setValue(pp->icm.refi);
    shiftx->setValue(pp->icm.shiftx);
    shifty->setValue(pp->icm.shifty);
    preser->setValue(pp->icm.preser);
    labgridcie->setParams(pp->icm.labgridcieALow, pp->icm.labgridcieBLow, pp->icm.labgridcieAHigh, pp->icm.labgridcieBHigh, pp->icm.labgridcieGx, pp->icm.labgridcieGy, pp->icm.labgridcieWx, pp->icm.labgridcieWy, pp->icm.labgridcieMx, pp->icm.labgridcieMy, false);

    if (pedited) {
        iunchanged->set_active(!pedited->icm.inputProfile);
        obpc->set_inconsistent(!pedited->icm.outputBPC);
        fbw->set_inconsistent(!pedited->icm.fbw);
        trcExp->set_inconsistent(!pedited->icm.trcExp);
        wavExp->set_inconsistent(!pedited->icm.wavExp);
        gamut->set_inconsistent(!pedited->icm.gamut);
        wsmoothcie->set_inconsistent(!pedited->icm.wsmoothcie);
        ckbToneCurve->set_inconsistent(!pedited->icm.toneCurve);
        ckbApplyLookTable->set_inconsistent(!pedited->icm.applyLookTable);
        ckbApplyBaselineExposureOffset->set_inconsistent(!pedited->icm.applyBaselineExposureOffset);
        ckbApplyHueSatMap->set_inconsistent(!pedited->icm.applyHueSatMap);
        opacityShapeWLI->setUnChanged(!pedited->icm.opacityCurveWLI);

        if (!pedited->icm.workingProfile) {
            wProfNames->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->icm.outputProfile) {
            oProfNames->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->icm.outputIntent) {
            oRendIntent->setSelected(4);
        }

        if (!pedited->icm.aRendIntent) {
            aRendIntent->setSelected(4);
        }

        if (!pedited->icm.dcpIlluminant) {
            dcpIll->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->icm.workingTRC) {
            wTRC->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->icm.wgamut) {
            wgamut->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->icm.will) {
            will->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->icm.wprim) {
            wprim->set_active_text(M("GENERAL_UNCHANGED"));
        }
        
        if (!pedited->icm.wcat) {
            wcat->set_active_text(M("GENERAL_UNCHANGED"));
        }
        
        labgridcie->setEdited(pedited->icm.labgridcieALow || pedited->icm.labgridcieBLow || pedited->icm.labgridcieAHigh || pedited->icm.labgridcieBHigh  || pedited->icm.labgridcieGx  || pedited->icm.labgridcieGy || pedited->icm.labgridcieWx  || pedited->icm.labgridcieWy || pedited->icm.labgridcieMx || pedited->icm.labgridcieMy);
        opacityShapeWLI->setCurve(pp->icm.opacityCurveWLI);

     //   wGamma->setEditedState(pedited->icm.workingTRCGamma ? Edited : UnEdited);
     //   wSlope->setEditedState(pedited->icm.workingTRCSlope  ? Edited : UnEdited);
        wGamma->setEditedState(pedited->icm.wGamma ? Edited : UnEdited);
        wSlope->setEditedState(pedited->icm.wSlope  ? Edited : UnEdited);
        wapsat->setEditedState(pedited->icm.wapsat  ? Edited : UnEdited);
        wmidtcie->setEditedState(pedited->icm.wmidtcie  ? Edited : UnEdited);
        wsmoothciesli->setEditedState(pedited->icm.wsmoothciesli  ? Edited : UnEdited);
        sigmatrc->setEditedState(pedited->icm.sigmatrc  ? Edited : UnEdited);
        offstrc->setEditedState(pedited->icm.offstrc  ? Edited : UnEdited);
        residtrc->setEditedState(pedited->icm.residtrc  ? Edited : UnEdited);
        wgampower->setEditedState(pedited->icm.wgampower ? Edited : UnEdited);
        wgamgain->setEditedState(pedited->icm.wgamgain ? Edited : UnEdited);
        pyrwavtrc->setEditedState(pedited->icm.pyrwavtrc  ? Edited : UnEdited);
        redx->setEditedState(pedited->icm.redx  ? Edited : UnEdited);
        redy->setEditedState(pedited->icm.redy  ? Edited : UnEdited);
        grex->setEditedState(pedited->icm.grex  ? Edited : UnEdited);
        grey->setEditedState(pedited->icm.grey  ? Edited : UnEdited);
        blux->setEditedState(pedited->icm.blux  ? Edited : UnEdited);
        bluy->setEditedState(pedited->icm.bluy  ? Edited : UnEdited);

        redrot->setEditedState(pedited->icm.redrot  ? Edited : UnEdited);
        redsat->setEditedState(pedited->icm.redsat  ? Edited : UnEdited);
        grerot->setEditedState(pedited->icm.grerot  ? Edited : UnEdited);
        gresat->setEditedState(pedited->icm.gresat  ? Edited : UnEdited);
        blurot->setEditedState(pedited->icm.blurot  ? Edited : UnEdited);
        blusat->setEditedState(pedited->icm.blusat  ? Edited : UnEdited);

        refi->setEditedState(pedited->icm.refi  ? Edited : UnEdited);
        preser->setEditedState(pedited->icm.preser  ? Edited : UnEdited);
        shiftx->setEditedState(pedited->icm.shiftx  ? Edited : UnEdited);
        shifty->setEditedState(pedited->icm.shifty  ? Edited : UnEdited);

    }

    switch (ColorManagementParams::WorkingTrc(wTRC->get_active_row_number())) {
        case ColorManagementParams::WorkingTrc::NONE: {
            wSlope->set_sensitive(false);
            wGamma->set_sensitive(false);
            wmidtcie->set_sensitive(false);
            wsmoothciesli->set_sensitive(false);
            sigmatrc->set_sensitive(false);
            offstrc->set_sensitive(false);
            residtrc->set_sensitive(false);
            pyrwavtrc->set_sensitive(false);
            will->set_sensitive(false);
            willulab->set_sensitive(false);
            wprim->set_sensitive(false);
            wcat->set_sensitive(false);
            wcatlab->set_sensitive(false);
            fbw->set_sensitive(false);
            gamut->set_sensitive(false);
            wsmoothcie->set_sensitive(false);
            wprimlab->set_sensitive(false);
            riaHBox->set_sensitive(false);
            redFrame->hide();
            break;
        }

        case ColorManagementParams::WorkingTrc::CUSTOM: {
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            if (gamut->get_active()) {
                wcatBox->set_sensitive(true);
            } else {
                wcatBox->set_sensitive(false);
            }
            
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            
            wprimlab->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                colorFramecie->set_sensitive(true);

                if (
                    ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM
                    && ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM_GRID
                    && ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM_POL
                    
                ) {
                    will->set_sensitive(false);
                    primCoordGrid->show();
                    primCoordGrid2->show();
                    primCoordGrid->set_sensitive(false);
                    primCoordGrid2->set_sensitive(false);
                    labgridcie->set_sensitive(false);

                } else {
                    will->set_sensitive(false);
                    if ((ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM)
                        || (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM_POL))
                    {
                        will->set_sensitive(true);
                        colorFramecie->set_sensitive(true);
                        primCoordGrid->show();
                        primCoordGrid2->show();
                    }
                    if ((ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM)) {
                        primCoordGrid->show();
                        primCoordGrid->set_sensitive(true);
                        primCoordGrid2->set_sensitive(false);
                        resetpolar();
                    } else {
                        primCoordGrid->hide();
                        primCoordGrid->set_sensitive(false);
                        primCoordGrid2->set_sensitive(true);
                    }
                    labgridcie->set_sensitive(true);
                    colorFramecie->set_sensitive(false);

                }

            }
            riaHBox->set_sensitive(true);

            if (pp->icm.wGamma <= 1.) {
                wGamma->set_sensitive(true);
                wSlope->set_sensitive(false);
                wmidtcie->set_sensitive(true);
            } else {
                wGamma->set_sensitive(true);
                wSlope->set_sensitive(true);
                wmidtcie->set_sensitive(true);
            }
            break;
        }

        case ColorManagementParams::WorkingTrc::BT709:
            wGamma->setValue(2.222);
            wSlope->setValue(4.5);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            if (gamut->get_active()) {
                wcatBox->set_sensitive(true);
            } else {
                wcatBox->set_sensitive(false);
            }
            
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
            }
            riaHBox->set_sensitive(true);
            break;
        case ColorManagementParams::WorkingTrc::SRGB:
            wGamma->setValue(2.4);
            wSlope->setValue(12.92);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            if (gamut->get_active()) {
                wcatBox->set_sensitive(true);
            } else {
                wcatBox->set_sensitive(false);
            }
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
            }
            break;
        case ColorManagementParams::WorkingTrc::GAMMA_2_2:
            wGamma->setValue(2.2);
            wSlope->setValue(0.);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            if (gamut->get_active()) {
                wcatBox->set_sensitive(true);
            } else {
                wcatBox->set_sensitive(false);
            }
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            redFrame->show();
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
            }
            break;
        case ColorManagementParams::WorkingTrc::GAMMA_1_8:
            wGamma->setValue(1.8);
            wSlope->setValue(0.);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            if (gamut->get_active()) {
                wcatBox->set_sensitive(true);
            } else {
                wcatBox->set_sensitive(false);
            }
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
            }
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            break;
        case ColorManagementParams::WorkingTrc::LINEAR:
            wGamma->setValue(1.);
            wSlope->setValue(1.);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
             if (gamut->get_active()) {
                wcatBox->set_sensitive(true);
            } else {
                wcatBox->set_sensitive(false);
            }
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
            }
            break;
    }
    colorFramecie->set_sensitive(true);

    switch (ColorManagementParams::Primaries(wprim->get_active_row_number())) {
        case ColorManagementParams::Primaries::DEFAULT:
        case ColorManagementParams::Primaries::SRGB:
        case ColorManagementParams::Primaries::ADOBE_RGB:
        case ColorManagementParams::Primaries::PRO_PHOTO:
        case ColorManagementParams::Primaries::REC2020:
        case ColorManagementParams::Primaries::ACES_P1:
        case ColorManagementParams::Primaries::WIDE_GAMUT:
        case ColorManagementParams::Primaries::ACES_P0:
        case ColorManagementParams::Primaries::JDC_MAX:
        case ColorManagementParams::Primaries::JDC_MAXSTDA:
        case ColorManagementParams::Primaries::BRUCE_RGB:
        case ColorManagementParams::Primaries::BETA_RGB:
        case ColorManagementParams::Primaries::BEST_RGB: {
            labgridcie->set_sensitive(false);
            will->set_sensitive(false);
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM: {
            will->set_sensitive(true);
            labgridcie->set_sensitive(false);
            colorFramecie->set_sensitive(true);
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(true);
            primCoordGrid2->set_sensitive(false);
            resetpolar();
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM_POL: {
            will->set_sensitive(true);
            labgridcie->set_sensitive(false);
            colorFramecie->set_sensitive(true);
            primCoordGrid2->show();
            primCoordGrid2->set_sensitive(true);
            primCoordGrid->hide();
            primCoordGrid->set_sensitive(false);
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM_GRID: {
            labgridcie->set_sensitive(true);
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(false);
            primCoordGrid2->set_sensitive(false);
            will->set_sensitive(false);
            colorFramecie->set_sensitive(false);
            resetpolar();
            break;
        }
    }

    enableListener();
}

void ICMPanel::write(ProcParams* pp, ParamsEdited* pedited)
{

    if (inone->get_active()) {
        pp->icm.inputProfile = "(none)";
    } else if (iembedded->get_active()) {
        pp->icm.inputProfile = "(embedded)";
    } else if (icamera->get_active()) {
        pp->icm.inputProfile = "(camera)";
    } else if (icameraICC->get_active()) {
        pp->icm.inputProfile = "(cameraICC)";
    } else {
        if (Glib::file_test(ipDialog->get_filename(), Glib::FILE_TEST_EXISTS) && !Glib::file_test(ipDialog->get_filename(), Glib::FILE_TEST_IS_DIR)) {
            pp->icm.inputProfile = "file:" + ipDialog->get_filename();
        } else {
            pp->icm.inputProfile = "";    // just a directory
        }
    }

    pp->icm.workingProfile = wProfNames->get_active_text();
    pp->icm.dcpIlluminant = rtengine::max<int>(dcpIll->get_active_row_number(), 0);


    labgridcie->getParams(pp->icm.labgridcieALow, pp->icm.labgridcieBLow, pp->icm.labgridcieAHigh, pp->icm.labgridcieBHigh, pp->icm.labgridcieGx, pp->icm.labgridcieGy, pp->icm.labgridcieWx, pp->icm.labgridcieWy, pp->icm.labgridcieMx, pp->icm.labgridcieMy);

    if (oProfNames->get_active_text() == M("TP_ICM_NOICM")) {
        pp->icm.outputProfile  = ColorManagementParams::NoICMString;
    } else {
        pp->icm.outputProfile  = oProfNames->get_active_text();
    }

    int ointentVal = oRendIntent->getSelected();

    if (ointentVal >= 0 && ointentVal < RI__COUNT) {
        pp->icm.outputIntent  = static_cast<RenderingIntent>(ointentVal);
    } else {
        pp->icm.outputIntent  = rtengine::RI_RELATIVE;
    }

    int aintentVal = aRendIntent->getSelected();

    if (aintentVal >= 0 && aintentVal < RI__COUNT) {
        pp->icm.aRendIntent  = static_cast<RenderingIntent>(aintentVal);
    } else {
        pp->icm.aRendIntent  = rtengine::RI_RELATIVE;
    }

    pp->icm.workingTRC = ColorManagementParams::WorkingTrc(wTRC->get_active_row_number());
    pp->icm.wgamut = ColorManagementParams::Wwgamut(wgamut->get_active_row_number());
    pp->icm.will = ColorManagementParams::Illuminant(will->get_active_row_number());
    pp->icm.wprim = ColorManagementParams::Primaries(wprim->get_active_row_number());
    pp->icm.wcat = ColorManagementParams::Cat(wcat->get_active_row_number());

    pp->icm.toneCurve = ckbToneCurve->get_active();
    pp->icm.applyLookTable = ckbApplyLookTable->get_active();
    pp->icm.applyBaselineExposureOffset = ckbApplyBaselineExposureOffset->get_active();
    pp->icm.applyHueSatMap = ckbApplyHueSatMap->get_active();
    pp->icm.outputBPC = obpc->get_active();
    pp->icm.fbw = fbw->get_active();
    pp->icm.trcExp = trcExp->getEnabled();
    pp->icm.wavExp = wavExp->getEnabled();
    pp->icm.gamut = gamut->get_active();
    pp->icm.wsmoothcie = wsmoothcie->get_active();
 //   pp->icm.workingTRCGamma =  wGamma->getValue();
 //   pp->icm.workingTRCSlope =  wSlope->getValue();
    pp->icm.wGamma =  wGamma->getValue();
    pp->icm.wSlope =  wSlope->getValue();
    pp->icm.wapsat =  wapsat->getValue();
    pp->icm.wmidtcie =  wmidtcie->getValue();
    pp->icm.wsmoothciesli =  wsmoothciesli->getValue();
    pp->icm.sigmatrc =  sigmatrc->getValue();
    pp->icm.offstrc =  offstrc->getValue();
    pp->icm.wgampower =  wgampower->getValue();
    pp->icm.wgamgain =  wgamgain->getValue();
    pp->icm.residtrc =  residtrc->getValue();
    pp->icm.pyrwavtrc =  pyrwavtrc->getIntValue();
    pp->icm.redx =  redx->getValue();
    pp->icm.redy =  redy->getValue();
    pp->icm.grex =  grex->getValue();
    pp->icm.grey =  grey->getValue();
    pp->icm.blux =  blux->getValue();
    pp->icm.bluy =  bluy->getValue();

    pp->icm.redrot =  redrot->getValue();
    pp->icm.redsat =  redsat->getValue();
    pp->icm.grerot =  grerot->getValue();
    pp->icm.gresat =  gresat->getValue();
    pp->icm.blurot =  blurot->getValue();
    pp->icm.blusat =  blusat->getValue();

    pp->icm.refi =  refi->getValue();
    pp->icm.shiftx =  shiftx->getValue();
    pp->icm.shifty =  shifty->getValue();
    pp->toneCurve.fromHistMatching = false;
    pp->icm.preser =  preser->getValue();
    pp->icm.opacityCurveWLI = opacityShapeWLI->getCurve();

    if (pedited) {
        pedited->icm.inputProfile = !iunchanged->get_active();
        pedited->icm.workingProfile = wProfNames->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.outputProfile = oProfNames->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.outputIntent = oRendIntent->getSelected() < 4;
        pedited->icm.aRendIntent = aRendIntent->getSelected() < 4;
        pedited->icm.outputBPC = !obpc->get_inconsistent();
        pedited->icm.fbw = !fbw->get_inconsistent();
        pedited->icm.trcExp = !trcExp->get_inconsistent();
        pedited->icm.wavExp = !wavExp->get_inconsistent();
        pedited->icm.gamut = !gamut->get_inconsistent();
        pedited->icm.wsmoothcie = !wsmoothcie->get_inconsistent();
        pedited->icm.dcpIlluminant = dcpIll->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.toneCurve = !ckbToneCurve->get_inconsistent();
        pedited->icm.applyLookTable = !ckbApplyLookTable->get_inconsistent();
        pedited->icm.applyBaselineExposureOffset = !ckbApplyBaselineExposureOffset->get_inconsistent();
        pedited->icm.applyHueSatMap = !ckbApplyHueSatMap->get_inconsistent();
      //  pedited->icm.workingTRCGamma = wGamma->getEditedState();
       // pedited->icm.workingTRCSlope = wSlope->getEditedState();
        pedited->icm.wGamma = wGamma->getEditedState();
        pedited->icm.wSlope = wSlope->getEditedState();
        pedited->icm.wapsat = wapsat->getEditedState();
        pedited->icm.wmidtcie = wmidtcie->getEditedState();
        pedited->icm.wsmoothciesli = wsmoothciesli->getEditedState();
        pedited->icm.wgampower = wgampower->getEditedState();
        pedited->icm.wgamgain = wgamgain->getEditedState();
        pedited->icm.sigmatrc = sigmatrc->getEditedState();
        pedited->icm.offstrc = offstrc->getEditedState();
        pedited->icm.residtrc = residtrc->getEditedState();
        pedited->icm.pyrwavtrc = pyrwavtrc->getEditedState();
        pedited->icm.workingTRC = wTRC->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.wgamut = wgamut->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.will = will->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.wprim = wprim->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.wcat = wcat->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->icm.redx = redx->getEditedState();
        pedited->icm.redy = redy->getEditedState();
        pedited->icm.redrot = redrot->getEditedState();
        pedited->icm.redsat = redsat->getEditedState();
        pedited->icm.labgridcieALow = pedited->icm.labgridcieBLow = pedited->icm.labgridcieAHigh = pedited->icm.labgridcieBHigh = pedited->icm.labgridcieGx = pedited->icm.labgridcieGy = pedited->icm.labgridcieWx = pedited->icm.labgridcieWy = pedited->icm.labgridcieMx = pedited->icm.labgridcieMy = labgridcie->getEdited();
        pedited->icm.opacityCurveWLI  = !opacityShapeWLI->isUnChanged();

   }
}

void ICMPanel::curveChanged(CurveEditor* ce)
{

    if (listener) {
        if (ce == opacityShapeWLI) {
            listener->panelChanged(EvICMopacityWLI, M("HISTORY_CUSTOMCURVE"));
        } 
    }
}
void ICMPanel::setDefaults(const ProcParams* defParams, const ParamsEdited* pedited)
{
   // wGamma->setDefault(defParams->icm.workingTRCGamma);
   // wSlope->setDefault(defParams->icm.workingTRCSlope);
    wGamma->setDefault(defParams->icm.wGamma);
    wSlope->setDefault(defParams->icm.wSlope);
    wapsat->setDefault(defParams->icm.wapsat);
    wmidtcie->setDefault(defParams->icm.wmidtcie);
    wsmoothciesli->setDefault(defParams->icm.wsmoothciesli);
    wgampower->setDefault(defParams->icm.wgampower);
    wgamgain->setDefault(defParams->icm.wgamgain);
    sigmatrc->setDefault(defParams->icm.sigmatrc);
    offstrc->setDefault(defParams->icm.offstrc);
    residtrc->setDefault(defParams->icm.residtrc);
    pyrwavtrc->setDefault(defParams->icm.pyrwavtrc);
    redx->setDefault(defParams->icm.redx);
    redy->setDefault(defParams->icm.redy);
    grex->setDefault(defParams->icm.grex);
    grey->setDefault(defParams->icm.grey);
    blux->setDefault(defParams->icm.blux);
    bluy->setDefault(defParams->icm.bluy);

    redrot->setDefault(defParams->icm.redrot);
    redsat->setDefault(defParams->icm.redsat);
    grerot->setDefault(defParams->icm.grerot);
    gresat->setDefault(defParams->icm.gresat);
    blurot->setDefault(defParams->icm.blurot);
    blusat->setDefault(defParams->icm.blusat);

    refi->setDefault(defParams->icm.refi);
    shiftx->setDefault(defParams->icm.shiftx);
    shifty->setDefault(defParams->icm.shifty);
    preser->setDefault(defParams->icm.preser);


    labgridcie->setDefault(defParams->icm.labgridcieALow, defParams->icm.labgridcieBLow , defParams->icm.labgridcieAHigh, defParams->icm.labgridcieBHigh, defParams->icm.labgridcieGx, defParams->icm.labgridcieGy, defParams->icm.labgridcieWx, defParams->icm.labgridcieWy, defParams->icm.labgridcieMx, defParams->icm.labgridcieMy);

    if (pedited) {
     //   wGamma->setDefaultEditedState(pedited->icm.workingTRCGamma ? Edited : UnEdited);
     //   wSlope->setDefaultEditedState(pedited->icm.workingTRCSlope ? Edited : UnEdited);
        wGamma->setDefaultEditedState(pedited->icm.wGamma ? Edited : UnEdited);
        wSlope->setDefaultEditedState(pedited->icm.wSlope ? Edited : UnEdited);
        wapsat->setDefaultEditedState(pedited->icm.wapsat ? Edited : UnEdited);
        wmidtcie->setDefaultEditedState(pedited->icm.wmidtcie ? Edited : UnEdited);
        wsmoothciesli->setDefaultEditedState(pedited->icm.wsmoothciesli ? Edited : UnEdited);
        wgampower->setDefaultEditedState(pedited->icm.wgampower ? Edited : UnEdited);
        wgamgain->setDefaultEditedState(pedited->icm.wgamgain ? Edited : UnEdited);
        sigmatrc->setDefaultEditedState(pedited->icm.sigmatrc ? Edited : UnEdited);
        offstrc->setDefaultEditedState(pedited->icm.offstrc ? Edited : UnEdited);
        residtrc->setDefaultEditedState(pedited->icm.residtrc ? Edited : UnEdited);
        pyrwavtrc->setDefaultEditedState(pedited->icm.pyrwavtrc ? Edited : UnEdited);
        redx->setDefaultEditedState(pedited->icm.redx ? Edited : UnEdited);
        redy->setDefaultEditedState(pedited->icm.redy ? Edited : UnEdited);
        grex->setDefaultEditedState(pedited->icm.grex ? Edited : UnEdited);
        grey->setDefaultEditedState(pedited->icm.grey ? Edited : UnEdited);
        blux->setDefaultEditedState(pedited->icm.blux ? Edited : UnEdited);
        bluy->setDefaultEditedState(pedited->icm.bluy ? Edited : UnEdited);

        redrot->setDefaultEditedState(pedited->icm.redrot ? Edited : UnEdited);
        redsat->setDefaultEditedState(pedited->icm.redsat ? Edited : UnEdited);
        grerot->setDefaultEditedState(pedited->icm.grerot ? Edited : UnEdited);
        gresat->setDefaultEditedState(pedited->icm.gresat ? Edited : UnEdited);
        blurot->setDefaultEditedState(pedited->icm.blurot ? Edited : UnEdited);
        blusat->setDefaultEditedState(pedited->icm.blusat ? Edited : UnEdited);

        refi->setDefaultEditedState(pedited->icm.refi ? Edited : UnEdited);
        shiftx->setDefaultEditedState(pedited->icm.shiftx ? Edited : UnEdited);
        shifty->setDefaultEditedState(pedited->icm.shifty ? Edited : UnEdited);
        labgridcie->setEdited((pedited->icm.labgridcieALow || pedited->icm.labgridcieBLow || pedited->icm.labgridcieAHigh || pedited->icm.labgridcieBHigh || pedited->icm.labgridcieGx || pedited->icm.labgridcieGy || pedited->icm.labgridcieWx || pedited->icm.labgridcieWy || pedited->icm.labgridcieMx || pedited->icm.labgridcieMy) ? Edited : UnEdited);
        preser->setDefaultEditedState(pedited->icm.preser ? Edited : UnEdited);

    } else {
        wGamma->setDefaultEditedState(Irrelevant);
        wSlope->setDefaultEditedState(Irrelevant);
        wapsat->setDefaultEditedState(Irrelevant);
        wmidtcie->setDefaultEditedState(Irrelevant);
        wsmoothciesli->setDefaultEditedState(Irrelevant);
        sigmatrc->setDefaultEditedState(Irrelevant);
        offstrc->setDefaultEditedState(Irrelevant);
        residtrc->setDefaultEditedState(Irrelevant);
        wgampower->setDefaultEditedState(Irrelevant);
        wgamgain->setDefaultEditedState(Irrelevant);
        pyrwavtrc->setDefaultEditedState(Irrelevant);
        redx->setDefaultEditedState(Irrelevant);
        redy->setDefaultEditedState(Irrelevant);
        grex->setDefaultEditedState(Irrelevant);
        grey->setDefaultEditedState(Irrelevant);
        blux->setDefaultEditedState(Irrelevant);
        bluy->setDefaultEditedState(Irrelevant);

        redrot->setDefaultEditedState(Irrelevant);
        redsat->setDefaultEditedState(Irrelevant);
        grerot->setDefaultEditedState(Irrelevant);
        gresat->setDefaultEditedState(Irrelevant);
        blurot->setDefaultEditedState(Irrelevant);
        blusat->setDefaultEditedState(Irrelevant);

        refi->setDefaultEditedState(Irrelevant);
        shiftx->setDefaultEditedState(Irrelevant);
        shifty->setDefaultEditedState(Irrelevant);
        preser->setDefaultEditedState(Irrelevant);
        labgridcie->setEdited(Edited);

    }
}

void ICMPanel::adjusterChanged(Adjuster* a, double newval)
{
    if (listener) {
        Glib::ustring costr2 = Glib::ustring::format(std::setw(3), std::fixed, std::setprecision(2), newval);

        if (a == wGamma) {
            if (wGamma->getValue() <= 1.) {
                wSlope->set_sensitive(false);
            } else {
                wSlope->set_sensitive(true);
            }
            listener->panelChanged(EvICMgamm, costr2);
        } else if (a == wSlope) {
            listener->panelChanged(EvICMslop, costr2);
        } else if (a == wapsat) {
            listener->panelChanged(EvICMwapsat, costr2);
        } else if (a == wmidtcie) {
            listener->panelChanged(EvICMwmidtcie, costr2);
        } else if (a == wsmoothciesli) {
            listener->panelChanged(EvICMwsmoothciesli, costr2);
        } else if (a == sigmatrc) {
            listener->panelChanged(EvICMsigmatrc, costr2);
        } else if (a == offstrc) {
            listener->panelChanged(EvICMoffstrc, costr2);
        } else if (a == residtrc) {
            listener->panelChanged(EvICMresidtrc, costr2);
        } else if (a == wgampower) {
            listener->panelChanged(EvICMwgampower, costr2);
        } else if (a == wgamgain) {
            listener->panelChanged(EvICMwgamgain, costr2);
        } else if (a == pyrwavtrc) {
            listener->panelChanged(EvICMpyrwavtrc, costr2);
        } else if (a == redx) {
            listener->panelChanged(EvICMredx, costr2);
        } else if (a == redy) {
            listener->panelChanged(EvICMredy, costr2);
        } else if (a == grex) {
            listener->panelChanged(EvICMgrex, costr2);
        } else if (a == grey) {
            listener->panelChanged(EvICMgrey, costr2);
        } else if (a == blux) {
            listener->panelChanged(EvICMblux, costr2);
        } else if (a == bluy) {
            listener->panelChanged(EvICMbluy, costr2);
        } else if (a == redrot) {
            listener->panelChanged(EvICMredrot, costr2);
        } else if (a == redsat) {
            listener->panelChanged(EvICMredsat, costr2);
        } else if (a == grerot) {
            listener->panelChanged(EvICMgrerot, costr2);
        } else if (a == gresat) {
            listener->panelChanged(EvICMgresat, costr2);
        } else if (a == blurot) {
            listener->panelChanged(EvICMblurot, costr2);
        } else if (a == blusat) {
            listener->panelChanged(EvICMblusat, costr2);
        } else if (a == preser) {
            listener->panelChanged(EvICMpreser, costr2);
        } else if (a == refi) {
            listener->panelChanged(EvICMrefi, costr2);
        } else if (a == shiftx) {
            listener->panelChanged(EvICMshiftx, costr2);
        } else if (a == shifty) {
            listener->panelChanged(EvICMshifty, costr2);
        }

    }
}

void ICMPanel::wpChanged()
{
    if (listener) {
        listener->panelChanged(EvWProfile, wProfNames->get_active_text());
    }
}

void ICMPanel::wgamutChanged()
{
    if (listener) {
        listener->panelChanged(EvICMwgamut, wgamut->get_active_text());
    }
}

void ICMPanel::wtrcinChanged()
{
    switch (ColorManagementParams::WorkingTrc(wTRC->get_active_row_number())) {
        case ColorManagementParams::WorkingTrc::NONE: {
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            wmidtcie->set_sensitive(false);
            wsmoothciesli->set_sensitive(false);
            sigmatrc->set_sensitive(false);
            offstrc->set_sensitive(false);
            residtrc->set_sensitive(false);
            pyrwavtrc->set_sensitive(false);
            will->set_sensitive(false);
            willulab->set_sensitive(false);
            wprim->set_sensitive(false);
            fbw->set_sensitive(false);
            gamut->set_sensitive(false);
            wsmoothcie->set_sensitive(false);
            wprimlab->set_sensitive(false);
            redFrame->hide();
            riaHBox->set_sensitive(false);
            break;
        }

        case ColorManagementParams::WorkingTrc::CUSTOM: {
            will->set_sensitive(false);
            wprim->set_sensitive(true);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            willulab->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                upgateGUI_lin_pol_graph();
            }
            riaHBox->set_sensitive(true);
            if (wGamma->getValue() <= 1.) {
                wGamma->set_sensitive(true);
                wSlope->set_sensitive(false);
            } else {
                wGamma->set_sensitive(true);
                wSlope->set_sensitive(true);
            }
            break;
        }

        case ColorManagementParams::WorkingTrc::BT709: {
            wGamma->setValue(2.222);
            wSlope->setValue(4.5);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                upgateGUI_lin_pol_graph();
            }
            riaHBox->set_sensitive(true);
            break;
        }

        case ColorManagementParams::WorkingTrc::SRGB: {
            wGamma->setValue(2.4);
            wSlope->setValue(12.92);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                upgateGUI_lin_pol_graph();
            }
            break;
        }

        case ColorManagementParams::WorkingTrc::GAMMA_2_2: {
            wGamma->setValue(2.2);
            wSlope->setValue(0.);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                upgateGUI_lin_pol_graph();
            }
            break;
        }

        case ColorManagementParams::WorkingTrc::GAMMA_1_8: {
            wGamma->setValue(1.8);
            wSlope->setValue(0.);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                upgateGUI_lin_pol_graph();
            }
            break;
        }

        case ColorManagementParams::WorkingTrc::LINEAR: {
            wGamma->setValue(1.0);
            wSlope->setValue(1.);
            wmidtcie->set_sensitive(true);
            sigmatrc->set_sensitive(true);
            offstrc->set_sensitive(true);
            residtrc->set_sensitive(true);
            pyrwavtrc->set_sensitive(true);
            will->set_sensitive(false);
            willulab->set_sensitive(true);
            wprim->set_sensitive(true);
            wcat->set_sensitive(true);
            wcatlab->set_sensitive(true);
            fbw->set_sensitive(true);
            gamut->set_sensitive(true);
            wsmoothcie->set_sensitive(true);
            wsmoothciesli->set_sensitive(true);
            wprimlab->set_sensitive(true);
            wGamma->set_sensitive(false);
            wSlope->set_sensitive(false);
            riaHBox->set_sensitive(true);
            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
                redFrame->hide();
            } else {
                redFrame->show();
                upgateGUI_lin_pol_graph();
            }
            break;
        }
    }
    wprimChanged();
    wcatChanged();

    switch (ColorManagementParams::Primaries(wprim->get_active_row_number())) {
        case ColorManagementParams::Primaries::DEFAULT:
        case ColorManagementParams::Primaries::SRGB:
        case ColorManagementParams::Primaries::ADOBE_RGB:
        case ColorManagementParams::Primaries::PRO_PHOTO:
        case ColorManagementParams::Primaries::REC2020:
        case ColorManagementParams::Primaries::ACES_P1:
        case ColorManagementParams::Primaries::WIDE_GAMUT:
        case ColorManagementParams::Primaries::ACES_P0:
        case ColorManagementParams::Primaries::JDC_MAX:
        case ColorManagementParams::Primaries::JDC_MAXSTDA:
        case ColorManagementParams::Primaries::BRUCE_RGB:
        case ColorManagementParams::Primaries::BETA_RGB:
        case ColorManagementParams::Primaries::BEST_RGB: {
            labgridcie->set_sensitive(false);
            will->set_sensitive(false);
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM: {
            will->set_sensitive(true);
            labgridcie->set_sensitive(false);
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(true);
            primCoordGrid2->set_sensitive(false);
            resetpolar();
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM_POL: {
            will->set_sensitive(true);
            labgridcie->set_sensitive(false);
            primCoordGrid2->show();
            primCoordGrid2->set_sensitive(true);
            primCoordGrid->hide();
            primCoordGrid->set_sensitive(false);
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM_GRID: {
            labgridcie->set_sensitive(true);
            will->set_sensitive(false);
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid2->set_sensitive(false);
            primCoordGrid->set_sensitive(false);
            resetpolar();
            break;
        }
    }

    if (ColorManagementParams::WorkingTrc(wTRC->get_active_row_number()) == ColorManagementParams::WorkingTrc::NONE) {
        redFrame->hide();
        will->set_sensitive(false);
        
    }

    if (listener) {
        listener->panelChanged(EvICMtrcinMethod, wTRC->get_active_text());
    }
}

void ICMPanel::upgateGUI_lin_pol_graph()
{
    if (
        ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM
        && ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM_GRID
        && ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM_POL
    )   {
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(false);
            primCoordGrid2->set_sensitive(false);
    } else {
        if(ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM) {
            primCoordGrid->show();
            primCoordGrid->set_sensitive(true);
            primCoordGrid2->set_sensitive(false);
            resetpolar();
        }
        if(ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM_POL) {
            primCoordGrid->hide();
            primCoordGrid2->set_sensitive(true);
            primCoordGrid->set_sensitive(false);
        }
    }
}


void ICMPanel::willChanged()
{
    colorFramecie->set_sensitive(true);

    switch (ColorManagementParams::Primaries(wprim->get_active_row_number() )) {
        case ColorManagementParams::Primaries::DEFAULT:
        case ColorManagementParams::Primaries::SRGB:
        case ColorManagementParams::Primaries::ADOBE_RGB:
        case ColorManagementParams::Primaries::PRO_PHOTO:
        case ColorManagementParams::Primaries::REC2020:
        case ColorManagementParams::Primaries::ACES_P1:
        case ColorManagementParams::Primaries::WIDE_GAMUT:
        case ColorManagementParams::Primaries::ACES_P0:
        case ColorManagementParams::Primaries::JDC_MAX:
        case ColorManagementParams::Primaries::JDC_MAXSTDA:
        case ColorManagementParams::Primaries::BRUCE_RGB:
        case ColorManagementParams::Primaries::BETA_RGB:
        case ColorManagementParams::Primaries::BEST_RGB: {
            labgridcie->set_sensitive(false);
            will->set_sensitive(false);
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM: {
            will->set_sensitive(true);
            labgridcie->set_sensitive(false);
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(true);
            primCoordGrid2->set_sensitive(false);
            resetpolar();
            break;
        }

        case ColorManagementParams::Primaries::CUSTOM_POL: {
            will->set_sensitive(true);
            labgridcie->set_sensitive(false);
            primCoordGrid2->set_sensitive(true);
            primCoordGrid2->show();
            primCoordGrid->hide();
            primCoordGrid->set_sensitive(false);

            break;
        }

        case ColorManagementParams::Primaries::CUSTOM_GRID: {
            labgridcie->set_sensitive(true);
            will->set_sensitive(false);
            primCoordGrid->show();
            primCoordGrid2->show();
            colorFramecie->set_sensitive(false);
            primCoordGrid2->set_sensitive(false);
            primCoordGrid->set_sensitive(false);
            resetpolar();
            break;
        }
    }

    if (listener) {
        listener->panelChanged(EvICMwillMethod, will->get_active_text());
    }
}



void ICMPanel::wprimChanged()
{
    colorFramecie->set_sensitive(true);

    switch (ColorManagementParams::Primaries(wprim->get_active_row_number())) {
        case ColorManagementParams::Primaries::DEFAULT:
        case ColorManagementParams::Primaries::CUSTOM:
        case ColorManagementParams::Primaries::CUSTOM_POL:
        case ColorManagementParams::Primaries::CUSTOM_GRID: {
            break;
        }

        case ColorManagementParams::Primaries::SRGB: {
            redx->setValue(0.64);
            redy->setValue(0.33);
            grex->setValue(0.30);
            grey->setValue(0.60);
            blux->setValue(0.15);
            bluy->setValue(0.06);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
            break;
        }

        case ColorManagementParams::Primaries::ADOBE_RGB: {
            redx->setValue(0.64);
            redy->setValue(0.33);
            grex->setValue(0.21);
            grey->setValue(0.71);
            blux->setValue(0.15);
            bluy->setValue(0.06);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
            break;
        }

        case ColorManagementParams::Primaries::PRO_PHOTO: {
            redx->setValue(0.7347);
            redy->setValue(0.2653);
            grex->setValue(0.1596);
            grey->setValue(0.8404);
            blux->setValue(0.0366);
            bluy->setValue(0.0001);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
            break;
        }

        case ColorManagementParams::Primaries::REC2020: {
            redx->setValue(0.708);
            redy->setValue(0.292);
            grex->setValue(0.17);
            grey->setValue(0.797);
            blux->setValue(0.131);
            bluy->setValue(0.046);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
            break;
        }

        case ColorManagementParams::Primaries::ACES_P1: {
            redx->setValue(0.713);
            redy->setValue(0.293);
            grex->setValue(0.165);
            grey->setValue(0.830);
            blux->setValue(0.128);
            bluy->setValue(0.044);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D60));
            break;
        }

        case ColorManagementParams::Primaries::WIDE_GAMUT: {
            redx->setValue(0.735);
            redy->setValue(0.265);
            grex->setValue(0.115);
            grey->setValue(0.826);
            blux->setValue(0.1570);
            bluy->setValue(0.018);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
            break;
        }

        case ColorManagementParams::Primaries::ACES_P0: {
            redx->setValue(0.7347);
            redy->setValue(0.2653);
            grex->setValue(0.);
            grey->setValue(1.0);
            blux->setValue(0.0001);
            bluy->setValue(-0.077);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D60));
            break;
        }

        case ColorManagementParams::Primaries::JDC_MAX: {
            redx->setValue(0.734702);
            redy->setValue(0.265302);
            grex->setValue(0.021908);
            grey->setValue(0.930288);
            blux->setValue(0.120593);
            bluy->setValue(0.001583);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
            break;
        }

        case ColorManagementParams::Primaries::JDC_MAXSTDA: {
            redx->setValue(0.734702);
            redy->setValue(0.265302);
            grex->setValue(0.021908);
            grey->setValue(0.930288);
            blux->setValue(0.120593);
            bluy->setValue(0.001583);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::STDA));
            break;
        }

        case ColorManagementParams::Primaries::BRUCE_RGB: {
            redx->setValue(0.64);
            redy->setValue(0.33);
            grex->setValue(0.28);
            grey->setValue(0.65);
            blux->setValue(0.15);
            bluy->setValue(0.06);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
            break;
        }

        case ColorManagementParams::Primaries::BETA_RGB: {
            redx->setValue(0.6888);
            redy->setValue(0.3112);
            grex->setValue(0.1986);
            grey->setValue(0.7551);
            blux->setValue(0.1265);
            bluy->setValue(0.0352);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
            break;
        }

        case ColorManagementParams::Primaries::BEST_RGB: {
            redx->setValue(0.7347);
            redy->setValue(0.2653);
            grex->setValue(0.2150);
            grey->setValue(0.7750);
            blux->setValue(0.130);
            bluy->setValue(0.035);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
            break;
        }
    }


    if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::DEFAULT) {
        if (wProfNames->get_active_text() == "Rec2020") {
            redx->setValue(0.708);
            redy->setValue(0.292);
            grex->setValue(0.17);
            grey->setValue(0.797);
            blux->setValue(0.131);
            bluy->setValue(0.046);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
        } else if (wProfNames->get_active_text() == "sRGB") {
            redx->setValue(0.64);
            redy->setValue(0.33);
            grex->setValue(0.30);
            grey->setValue(0.60);
            blux->setValue(0.15);
            bluy->setValue(0.06);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
        } else if (wProfNames->get_active_text() == "Adobe RGB") {
            redx->setValue(0.64);
            redy->setValue(0.33);
            grex->setValue(0.21);
            grey->setValue(0.71);
            blux->setValue(0.15);
            bluy->setValue(0.06);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65));
        } else if (wProfNames->get_active_text() == "ProPhoto") {
            redx->setValue(0.7347);
            redy->setValue(0.2653);
            grex->setValue(0.1596);
            grey->setValue(0.8404);
            blux->setValue(0.0366);
            bluy->setValue(0.0001);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
        } else if (wProfNames->get_active_text() == "ACESp1") {
            redx->setValue(0.713);
            redy->setValue(0.293);
            grex->setValue(0.165);
            grey->setValue(0.830);
            blux->setValue(0.128);
            bluy->setValue(0.044);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D60));
        } else if (wProfNames->get_active_text() == "WideGamut") {
            redx->setValue(0.735);
            redy->setValue(0.265);
            grex->setValue(0.115);
            grey->setValue(0.826);
            blux->setValue(0.1570);
            bluy->setValue(0.018);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
        } else if (wProfNames->get_active_text() == "ACESp0") {
            redx->setValue(0.7347);
            redy->setValue(0.2653);
            grex->setValue(0.);
            grey->setValue(1.0);
            blux->setValue(0.0001);
            bluy->setValue(-0.077);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D60));
        } else if (wProfNames->get_active_text() == "JDCmax") {
            redx->setValue(0.734702);
            redy->setValue(0.265302);
            grex->setValue(0.021908);
            grey->setValue(0.930288);
            blux->setValue(0.120593);
            bluy->setValue(0.001583);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));//D50
        } else if (wProfNames->get_active_text() == "JDCmax stdA") {
            redx->setValue(0.734702);
            redy->setValue(0.265302);
            grex->setValue(0.021908);
            grey->setValue(0.930288);
            blux->setValue(0.120593);
            bluy->setValue(0.001583);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::STDA));
        } else if (wProfNames->get_active_text() == "BruceRGB") {
            redx->setValue(0.64);
            redy->setValue(0.33);
            grex->setValue(0.28);
            grey->setValue(0.65);
            blux->setValue(0.15);
            bluy->setValue(0.06);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D65)); 
        } else if (wProfNames->get_active_text() == "Beta RGB") {
            redx->setValue(0.6888);
            redy->setValue(0.3112);
            grex->setValue(0.1986);
            grey->setValue(0.7551);
            blux->setValue(0.1265);
            bluy->setValue(0.0352);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
        } else if (wProfNames->get_active_text() == "BestRGB") {
            redx->setValue(0.7347);
            redy->setValue(0.2653);
            grex->setValue(0.2150);
            grey->setValue(0.7750);
            blux->setValue(0.130);
            bluy->setValue(0.035);
            will->set_active(toUnderlying(ColorManagementParams::Illuminant::D50));
        }

        redFrame->hide();
    } else {
        redFrame->show();

        if ((ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM) 
            && (ColorManagementParams::Primaries(wprim->get_active_row_number()) != ColorManagementParams::Primaries::CUSTOM_POL))
        {
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(false);
            primCoordGrid2->set_sensitive(false);
            labgridcie->set_sensitive(false);
            will->set_sensitive(false);
            colorFramecie->set_sensitive(true);

            if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM_GRID) {
                labgridcie->set_sensitive(true);
                colorFramecie->set_sensitive(false);
                resetpolar();
            }
        } else if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM) {
            primCoordGrid->show();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(true);
            primCoordGrid2->set_sensitive(false);
            labgridcie->set_sensitive(false);
            will->set_sensitive(true);
            colorFramecie->set_sensitive(true);
            resetpolar();
        } else if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM_POL) {
            primCoordGrid->hide();
            primCoordGrid2->show();
            primCoordGrid->set_sensitive(false);
            primCoordGrid2->set_sensitive(true);
            labgridcie->set_sensitive(false);
            will->set_sensitive(true);
            colorFramecie->set_sensitive(true);
        }

    }
    willChanged ();

    if (ColorManagementParams::Primaries(wprim->get_active_row_number()) == ColorManagementParams::Primaries::CUSTOM_GRID) {
        primCoordGrid->show();
        primCoordGrid2->show();
        primCoordGrid->set_sensitive(false);
        primCoordGrid2->set_sensitive(false);
        labgridcie->set_sensitive(true);
        colorFramecie->set_sensitive(false);
        resetpolar();
    } else {
        labgridcie->set_sensitive(false);
        colorFramecie->set_sensitive(true);
    }

    if (listener) {
        listener->panelChanged(EvICMwprimMethod, wprim->get_active_text());
    }
}

void ICMPanel::wcatChanged()
{
    if (listener) {
        listener->panelChanged(EvICMcat, wcat->get_active_text());
    }
    
}

void ICMPanel::dcpIlluminantChanged()
{
    if (listener) {
        listener->panelChanged(EvDCPIlluminant, dcpIll->get_active_text());
    }
}

void ICMPanel::toneCurveChanged()
{
    if (multiImage) {
        if (ckbToneCurve->get_inconsistent()) {
            ckbToneCurve->set_inconsistent(false);
            tcurveconn.block(true);
            ckbToneCurve->set_active(false);
            tcurveconn.block(false);
        } else if (lastToneCurve) {
            ckbToneCurve->set_inconsistent(true);
        }

        lastToneCurve = ckbToneCurve->get_active();
    }

    if (listener) {
        if (ckbToneCurve->get_inconsistent()) {
            listener->panelChanged(EvDCPToneCurve, M("GENERAL_UNCHANGED"));
        } else if (ckbToneCurve->get_active()) {
            listener->panelChanged(EvDCPToneCurve, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvDCPToneCurve, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::applyLookTableChanged()
{
    if (multiImage) {
        if (ckbApplyLookTable->get_inconsistent()) {
            ckbApplyLookTable->set_inconsistent(false);
            ltableconn.block(true);
            ckbApplyLookTable->set_active(false);
            ltableconn.block(false);
        } else if (lastApplyLookTable) {
            ckbApplyLookTable->set_inconsistent(true);
        }

        lastApplyLookTable = ckbApplyLookTable->get_active();
    }

    if (listener) {
        if (ckbApplyLookTable->get_inconsistent()) {
            listener->panelChanged(EvDCPApplyLookTable, M("GENERAL_UNCHANGED"));
        } else if (ckbApplyLookTable->get_active()) {
            listener->panelChanged(EvDCPApplyLookTable, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvDCPApplyLookTable, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::applyBaselineExposureOffsetChanged()
{
    if (multiImage) {
        if (ckbApplyBaselineExposureOffset->get_inconsistent()) {
            ckbApplyBaselineExposureOffset->set_inconsistent(false);
            beoconn.block(true);
            ckbApplyBaselineExposureOffset->set_active(false);
            beoconn.block(false);
        } else if (lastApplyBaselineExposureOffset) {
            ckbApplyBaselineExposureOffset->set_inconsistent(true);
        }

        lastApplyBaselineExposureOffset = ckbApplyBaselineExposureOffset->get_active();
    }

    if (listener) {
        if (ckbApplyBaselineExposureOffset->get_inconsistent()) {
            listener->panelChanged(EvDCPApplyBaselineExposureOffset, M("GENERAL_UNCHANGED"));
        } else if (ckbApplyBaselineExposureOffset->get_active()) {
            listener->panelChanged(EvDCPApplyBaselineExposureOffset, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvDCPApplyBaselineExposureOffset, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::applyHueSatMapChanged()
{
    if (multiImage) {
        if (ckbApplyHueSatMap->get_inconsistent()) {
            ckbApplyHueSatMap->set_inconsistent(false);
            hsmconn.block(true);
            ckbApplyHueSatMap->set_active(false);
            hsmconn.block(false);
        } else if (lastApplyHueSatMap) {
            ckbApplyHueSatMap->set_inconsistent(true);
        }

        lastApplyHueSatMap = ckbApplyHueSatMap->get_active();
    }

    if (listener) {
        if (ckbApplyHueSatMap->get_inconsistent()) {
            listener->panelChanged(EvDCPApplyHueSatMap, M("GENERAL_UNCHANGED"));
        } else if (ckbApplyHueSatMap->get_active()) {
            listener->panelChanged(EvDCPApplyHueSatMap, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvDCPApplyHueSatMap, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::ipChanged()
{

    Glib::ustring profname;
    Glib::ustring localized_profname;

    if (inone->get_active()) {
        profname = "(none)";
        localized_profname = inone->get_label();
    } else if (iembedded->get_active()) {
        profname = "(embedded)";
        localized_profname = iembedded->get_label();
    } else if (icamera->get_active()) {
        profname = "(camera)";
        localized_profname = icamera->get_label();
    } else if (icameraICC->get_active()) {
        profname = "(cameraICC)";
        localized_profname = icameraICC->get_label();
    } else {
        profname = ipDialog->get_filename();
        localized_profname = profname;
    }

    updateDCP(-1, profname);

    if (listener && profname != oldip) {
        listener->panelChanged(EvIProfile, localized_profname);
    }

    oldip = profname;
}

void ICMPanel::opChanged()
{
    if (!batchMode) {
        updateRenderingIntent(oProfNames->get_active_text());
    }

    if (listener) {
        listener->panelChanged(EvOProfile, oProfNames->get_active_text());
    }
}

void ICMPanel::oiChanged(int n)
{

    if (listener) {
        Glib::ustring str;

        switch (n) {
            case 0:
                str = M("PREFERENCES_INTENT_PERCEPTUAL");
                break;

            case 1:
                str = M("PREFERENCES_INTENT_RELATIVE");
                break;

            case 2:
                str = M("PREFERENCES_INTENT_SATURATION");
                break;

            case 3:
                str = M("PREFERENCES_INTENT_ABSOLUTE");
                break;

            case 4:
            default:
                str = M("GENERAL_UNCHANGED");
                break;
        }

        listener->panelChanged(EvOIntent, str);
    }
}

void ICMPanel::aiChanged(int n)
{

    if (listener) {
        Glib::ustring str;

        switch (n) {
            case 0:
                str = M("PREFERENCES_INTENT_PERCEPTUAL");
                break;

            case 1:
                str = M("PREFERENCES_INTENT_RELATIVE");
                break;

            case 2:
                str = M("PREFERENCES_INTENT_SATURATION");
                break;

            case 3:
                str = M("PREFERENCES_INTENT_ABSOLUTE");
                break;

            case 4:
            default:
                str = M("GENERAL_UNCHANGED");
                break;
        }

        listener->panelChanged(EvaIntent, str);
    }
}

void ICMPanel::oBPCChanged()
{
    if (multiImage) {
        if (obpc->get_inconsistent()) {
            obpc->set_inconsistent(false);
            obpcconn.block(true);
            obpc->set_active(false);
            obpcconn.block(false);
        } else if (lastobpc) {
            obpc->set_inconsistent(true);
        }

        lastobpc = obpc->get_active();
    }

    if (listener) {
        if (obpc->get_inconsistent()) {
            listener->panelChanged(EvOBPCompens, M("GENERAL_UNCHANGED"));
        } else if (obpc->get_active()) {
            listener->panelChanged(EvOBPCompens, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvOBPCompens, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::trcExpChanged()
{

    if (listener) {
        if (trcExp->getEnabled()) {
            listener->panelChanged(EvICMtrcExp, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvICMtrcExp, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::wavExpChanged()
{

    if (listener) {
        if (wavExp->getEnabled()) {
            listener->panelChanged(EvICMwavExp, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvICMwavExp, M("GENERAL_DISABLED"));
        }
    }
}


void ICMPanel::fbwChanged()
{
    if (multiImage) {
        if (fbw->get_inconsistent()) {
            fbw->set_inconsistent(false);
            fbwconn.block(true);
            fbw->set_active(false);
            fbwconn.block(false);
        } else if (lastfbw) {
            fbw->set_inconsistent(true);
        }

        lastfbw = fbw->get_active();
    }

    if (listener) {
        if (fbw->get_inconsistent()) {
            listener->panelChanged(EvICMfbw, M("GENERAL_UNCHANGED"));
        } else if (fbw->get_active()) {
            listener->panelChanged(EvICMfbw, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvICMfbw, M("GENERAL_DISABLED"));
        }
    }
}

void ICMPanel::gamutChanged()
{
    if (multiImage) {
        if (gamut->get_inconsistent()) {
            gamut->set_inconsistent(false);
            gamutconn.block(true);
            gamut->set_active(false);
            gamutconn.block(false);
        } else if (lastgamut) {
            gamut->set_inconsistent(true);
        }

        lastgamut = gamut->get_active();
    }
    
    if (gamut->get_active()) {
        wcatBox->set_sensitive(true);
    } else {
        wcatBox->set_sensitive(false);
    }
    
    if (listener) {
        if (gamut->get_inconsistent()) {
            listener->panelChanged(EvICMgamut, M("GENERAL_UNCHANGED"));
        } else if (gamut->get_active()) {
            listener->panelChanged(EvICMgamut, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvICMgamut, M("GENERAL_DISABLED"));
        }
    }
}


void ICMPanel::wsmoothcieChanged()
{
    if (multiImage) {
        if (wsmoothcie->get_inconsistent()) {
            wsmoothcie->set_inconsistent(false);
            wsmoothcieconn.block(true);
            wsmoothcie->set_active(false);
            wsmoothcieconn.block(false);
        } else if (lastwsmoothcie) {
            wsmoothcie->set_inconsistent(true);
        }

        lastwsmoothcie = wsmoothcie->get_active();
    }
    
    
    if (listener) {
        if (wsmoothcie->get_inconsistent()) {
            listener->panelChanged(EvICMwsmoothcie, M("GENERAL_UNCHANGED"));
        } else if (wsmoothcie->get_active()) {
            listener->panelChanged(EvICMwsmoothcie, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvICMwsmoothcie, M("GENERAL_DISABLED"));
        }
    }
}


void ICMPanel::setRawMeta(bool raw, const rtengine::FramesData* pMeta)
{

    disableListener();

    icamera->set_active(raw);
    iembedded->set_active(!raw);
    icamera->set_sensitive(raw);
    camName = pMeta->getCamera();
    filename = pMeta->getFileName();
    icameraICC->set_sensitive(raw && (ICCStore::getInstance()->getStdProfile(pMeta->getCamera()) != nullptr || DCPStore::getInstance()->getStdProfile(pMeta->getCamera()) != nullptr));
    iembedded->set_sensitive(!raw || DCPStore::getInstance()->getProfile(filename));

    enableListener();
}

void ICMPanel::ipSelectionChanged()
{

    if (ipDialog->get_filename().empty()) {
        return;
    }

    ipChanged();
}



void ICMPanel::saveReferencePressed()
{

    if (!icmplistener) {
        return;
    }

    Gtk::FileChooserDialog dialog(getToplevelWindow(this), M("TP_ICM_SAVEREFERENCE"), Gtk::FILE_CHOOSER_ACTION_SAVE);
    bindCurrentFolder(dialog, App::get().mut_options().lastProfilingReferenceDir);
    dialog.set_current_name(lastRefFilename);

    dialog.add_button(M("GENERAL_CANCEL"), Gtk::RESPONSE_CANCEL);
    dialog.add_button(M("GENERAL_SAVE"), Gtk::RESPONSE_OK);

    Gtk::CheckButton applyWB(M("TP_ICM_SAVEREFERENCE_APPLYWB"));
    applyWB.set_tooltip_text(M("TP_ICM_SAVEREFERENCE_APPLYWB_TOOLTIP"));
    Gtk::Box* hbox = Gtk::manage(new Gtk::Box());
    hbox->pack_end(applyWB, Gtk::PACK_SHRINK, 2);
    Gtk::Box *box = dialog.get_content_area();
    box->pack_end(*hbox, Gtk::PACK_SHRINK, 2);

    Glib::RefPtr<Gtk::FileFilter> filter_tif = Gtk::FileFilter::create();
    filter_tif->set_name(M("FILECHOOSER_FILTER_TIFF"));
    filter_tif->add_pattern("*.tif");
    filter_tif->add_pattern("*.tiff");
    dialog.add_filter(filter_tif);

    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name(M("FILECHOOSER_FILTER_ANY"));
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.show_all_children();
    //dialog.set_do_overwrite_confirmation (true);

    bool done = false;

    do {
        int result = dialog.run();

        if (result != Gtk::RESPONSE_OK) {
            done = true;
        } else {
            std::string fname = dialog.get_filename();
            Glib::ustring ext = getExtension(fname);

            if (ext != "tif" && ext != "tiff") {
                fname += ".tif";
            }

            if (confirmOverwrite(dialog, fname)) {
                icmplistener->saveInputICCReference(fname, applyWB.get_active());
                lastRefFilename = Glib::path_get_basename(fname);
                done = true;
            }
        }
    } while (!done);

    return;
}

void ICMPanel::setBatchMode(bool batchMode)
{

    isBatchMode = true;
    ToolPanel::setBatchMode(batchMode);
    iunchanged = Gtk::manage(new Gtk::RadioButton(M("GENERAL_UNCHANGED")));
    iunchanged->set_group(opts);
    iVBox->pack_start(*iunchanged, Gtk::PACK_SHRINK, 4);
    iVBox->reorder_child(*iunchanged, 5);
    opacityCurveEditorWLI->setBatchMode(batchMode);

    removeIfThere(this, saveRef);
    oProfNames->append(M("GENERAL_UNCHANGED"));
    oRendIntent->addEntry("template-24", M("GENERAL_UNCHANGED"));
    oRendIntent->show();
    aRendIntent->addEntry("template-24", M("GENERAL_UNCHANGED"));
    aRendIntent->show();
    wProfNames->append(M("GENERAL_UNCHANGED"));
    wTRC->append(M("GENERAL_UNCHANGED"));
    wgamut->append(M("GENERAL_UNCHANGED"));
    will->append(M("GENERAL_UNCHANGED"));
    wprim->append(M("GENERAL_UNCHANGED"));
    dcpIll->append(M("GENERAL_UNCHANGED"));
    wGamma->showEditedCB();
    wSlope->showEditedCB();
    wmidtcie->showEditedCB();
    sigmatrc->showEditedCB();
    offstrc->showEditedCB();
    residtrc->showEditedCB();
    wgampower->showEditedCB();
    wgamgain->showEditedCB();
    pyrwavtrc->showEditedCB();
    redx->showEditedCB();
    redy->showEditedCB();
    grex->showEditedCB();
    grey->showEditedCB();
    blux->showEditedCB();
    bluy->showEditedCB();
    redrot->showEditedCB();
    redsat->showEditedCB();
    grerot->showEditedCB();
    gresat->showEditedCB();
    blurot->showEditedCB();
    blusat->showEditedCB();
    refi->showEditedCB();
    shiftx->showEditedCB();
    shifty->showEditedCB();
    preser->showEditedCB();
}

