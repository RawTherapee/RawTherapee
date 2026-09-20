/*
*  This file is part of RawTherapee.
*
*  Copyright (c) 2012 Oliver Duis <oduis@oliverduis.de>
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
#include <map>
#include <set>
#include <iostream>
#include <sstream>

#include <glibmm/ustring.h>

#include "lensprofile.h"

#include "guiutils.h"
#include "rtimage.h"
#include "options.h"
#include "widgets/basic/myexpander.h"
#include "widgets/basic/myfilechooser.h"

#include "rtengine/lcp.h"
#include "rtengine/procparams.h"
#include "rtengine/rtlensfun.h"
#include "rtengine/lensmetadata.h"

using namespace rtengine;
using namespace rtengine::procparams;

const Glib::ustring LensProfilePanel::TOOL_NAME = "lensprof";

LensProfilePanel::LensProfilePanel() :
    FoldableToolPanel(this, TOOL_NAME, M("TP_LENSPROFILE_LABEL")),
    lcModeChanged(false),
    lcpFileChanged(false),
    useDistChanged(false),
    useVignChanged(false),
    useCAChanged(false),
    useLensfunChanged(false),
    lensfunAutoChanged(false),
    lensfunCameraChanged(false),
    lensfunLensChanged(false),
    allowFocusDep(true),
    isRaw(true),
    metadata(nullptr),
    modesGrid(Gtk::manage(new Gtk::Grid())),
    distGrid(Gtk::manage((new Gtk::Grid()))),
    corrUnchangedRB(Gtk::manage((new Gtk::RadioButton(M("GENERAL_UNCHANGED"))))),
    corrOffRB(Gtk::manage((new Gtk::RadioButton(corrGroup, M("GENERAL_NONE"))))),
    corrMetadata(Gtk::manage((new Gtk::RadioButton(corrGroup, M("TP_LENSPROFILE_CORRECTION_METADATA"))))),
    corrLensfunAutoRB(Gtk::manage((new Gtk::RadioButton(corrGroup, M("TP_LENSPROFILE_CORRECTION_AUTOMATCH"))))),
    corrLensfunManualRB(Gtk::manage((new Gtk::RadioButton(corrGroup, M("TP_LENSPROFILE_CORRECTION_MANUAL"))))),
    corrLcpFileRB(Gtk::manage((new Gtk::RadioButton(corrGroup, M("TP_LENSPROFILE_CORRECTION_LCPFILE"))))),
    corrLcpFileChooser(Gtk::manage((new MyFileChooserButton(M("TP_LENSPROFILE_LABEL"), Gtk::FILE_CHOOSER_ACTION_OPEN)))),
    lensfunCamerasLbl(Gtk::manage((new Gtk::Label(M("EXIFFILTER_CAMERA"))))),
    lensfunCameras(Gtk::manage((new MyComboBox()))),
    lensfunLensesLbl(Gtk::manage((new Gtk::Label(M("EXIFFILTER_LENS"))))),
    lensfunLenses(Gtk::manage((new MyComboBox()))),
    warning(Gtk::manage(new RTImage("warning", Gtk::ICON_SIZE_LARGE_TOOLBAR))),
    ckbUseDist(Gtk::manage((new Gtk::CheckButton(M("TP_LENSPROFILE_USE_GEOMETRIC"))))),
    ckbUseVign(Gtk::manage((new Gtk::CheckButton(M("TP_LENSPROFILE_USE_VIGNETTING"))))),
    ckbUseCA(Gtk::manage((new Gtk::CheckButton(M("TP_LENSPROFILE_USE_CA")))))
{
    if (!lf) {
        lf = new LFDbHelper();
    }

    // Main containers:

    Gtk::Frame *nodesFrame = Gtk::manage(new Gtk::Frame(M("TP_LENSPROFILE_MODE_HEADER")));
    nodesFrame->set_label_align (0.025, 0.5);

    modesGrid->get_style_context()->add_class("grid-spacing");
    setExpandAlignProperties(modesGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    Gtk::Frame *distFrame = Gtk::manage(new Gtk::Frame(M("TP_LENSPROFILE_USE_HEADER")));
    distFrame->set_label_align (0.025, 0.5);

    distGrid->get_style_context()->add_class("grid-spacing");
    setExpandAlignProperties(distGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    // Mode choice widgets:

    setExpandAlignProperties(corrLcpFileChooser, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    // Manually-selected profile widgets:

    setExpandAlignProperties(lensfunCamerasLbl, false, false, Gtk::ALIGN_END, Gtk::ALIGN_CENTER);

    lensfunCameras->set_model(lf->lensfunCameraModel);
    lensfunCameras->pack_start(lf->lensfunModelCam.model);
    lensfunCameras->setPreferredWidth(50, 120);
    setExpandAlignProperties(lensfunCameras, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    Gtk::CellRendererText* const camerasCellRenderer = static_cast<Gtk::CellRendererText*>(lensfunCameras->get_first_cell());
    camerasCellRenderer->property_ellipsize() = Pango::ELLIPSIZE_MIDDLE;
    camerasCellRenderer->property_ellipsize_set() = true;

    setExpandAlignProperties(lensfunLensesLbl, false, false, Gtk::ALIGN_END, Gtk::ALIGN_CENTER);

    lensfunLenses->set_model(lf->lensfunLensModel);
    lensfunLenses->pack_start(lf->lensfunModelLens.prettylens);
    lensfunLenses->setPreferredWidth(50, 120);
    setExpandAlignProperties(lensfunLenses, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    Gtk::CellRendererText* const lensesCellRenderer = static_cast<Gtk::CellRendererText*>(lensfunLenses->get_first_cell());
    lensesCellRenderer->property_ellipsize() = Pango::ELLIPSIZE_MIDDLE;
    lensesCellRenderer->property_ellipsize_set() = true;

    warning->set_tooltip_text(M("TP_LENSPROFILE_LENS_WARNING"));
    warning->hide();

    // LCP file filter config:

    const Glib::RefPtr<Gtk::FileFilter> filterLCP = Gtk::FileFilter::create();
    filterLCP->set_name(M("FILECHOOSER_FILTER_LCP"));
    filterLCP->add_pattern("*.lcp");
    filterLCP->add_pattern("*.LCP");
    corrLcpFileChooser->add_filter(filterLCP);

    const Glib::ustring defDir = LCPStore::getInstance()->getDefaultCommonDirectory();

    auto& options = App::get().mut_options();
    if (!defDir.empty()) {
#ifdef _WIN32
        corrLcpFileChooser->set_show_hidden(true);  // ProgramData is hidden on Windows
#endif
        corrLcpFileChooser->set_current_folder(defDir);
    } else if (!options.lastLensProfileDir.empty()) {
        corrLcpFileChooser->set_current_folder(options.lastLensProfileDir);
    }

    bindCurrentFolder(*corrLcpFileChooser, options.lastLensProfileDir);

    // Choice of properties to correct, applicable to all modes:

    // Populate modes grid:

    modesGrid->attach(*corrOffRB, 0, 0, 3, 1);
    modesGrid->attach(*corrMetadata, 0, 1, 3, 1);
    modesGrid->attach(*corrLensfunAutoRB, 0, 2, 3, 1);
    modesGrid->attach(*corrLensfunManualRB, 0, 3, 3, 1);

    modesGrid->attach(*lensfunCamerasLbl, 0, 4, 1, 1);
    modesGrid->attach(*lensfunCameras, 1, 4, 1, 1);
    modesGrid->attach(*lensfunLensesLbl, 0, 5, 1, 1);
    modesGrid->attach(*lensfunLenses, 1, 5, 1, 1);
    modesGrid->attach(*warning, 2, 4, 1, 2);

    modesGrid->attach(*corrLcpFileRB, 0, 6, 1, 1);
    modesGrid->attach(*corrLcpFileChooser, 1, 6, 1, 1);

    // Populate distortions grid:

    distGrid->attach(*ckbUseDist, 0, 0, 1, 1);
    distGrid->attach(*ckbUseVign, 0, 1, 1, 1);
    distGrid->attach(*ckbUseCA, 0, 2, 1, 1);

    // Attach grids:
    nodesFrame->add(*modesGrid);
    distFrame->add(*distGrid);

    pack_start(*nodesFrame, Gtk::PACK_EXPAND_WIDGET);
    pack_start(*distFrame, Gtk::PACK_EXPAND_WIDGET);

    // Signals:

    conLCPFile = corrLcpFileChooser->signal_file_set().connect(sigc::mem_fun(*this, &LensProfilePanel::onLCPFileChanged));
    conUseDist = ckbUseDist->signal_toggled().connect(sigc::mem_fun(*this, &LensProfilePanel::onUseDistChanged));
    ckbUseVign->signal_toggled().connect(sigc::mem_fun(*this, &LensProfilePanel::onUseVignChanged));
    ckbUseCA->signal_toggled().connect(sigc::mem_fun(*this, &LensProfilePanel::onUseCAChanged));

    lensfunCameras->signal_changed().connect(sigc::mem_fun(*this, &LensProfilePanel::onLensfunCameraChanged));
    lensfunLenses->signal_changed().connect(sigc::mem_fun(*this, &LensProfilePanel::onLensfunLensChanged));
    corrOffRB->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &LensProfilePanel::onCorrModeChanged), corrOffRB));
    corrMetadata->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &LensProfilePanel::onCorrModeChanged), corrMetadata));
    corrLensfunAutoRB->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &LensProfilePanel::onCorrModeChanged), corrLensfunAutoRB));
    corrLensfunManualRB->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &LensProfilePanel::onCorrModeChanged), corrLensfunManualRB));
    corrLcpFileRB->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &LensProfilePanel::onCorrModeChanged), corrLcpFileRB));
}

void LensProfilePanel::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    disableListener();
    conUseDist.block(true);

    // corrLensfunAutoRB->set_sensitive(true);

    switch (pp->lensProf.lcMode) {
        case procparams::LensProfParams::LcMode::LCP: {
            corrLcpFileRB->set_active(true);
            setManualParamsVisibility(false);
            break;
        }

        case procparams::LensProfParams::LcMode::LENSFUNAUTOMATCH: {
            corrLensfunAutoRB->set_active(true);
            if (batchMode) {
                setManualParamsVisibility(false);
            }
            break;
        }

        case procparams::LensProfParams::LcMode::LENSFUNMANUAL: {
            corrLensfunManualRB->set_active(true);
            break;
        }

        case procparams::LensProfParams::LcMode::METADATA: {
            if (metadata) {
                auto metadataCorrection= rtengine::MetadataLensCorrectionFinder::findCorrection(metadata);
                if (metadataCorrection) {
                    corrMetadata->set_active(true);
                    corrMetadata->set_sensitive(true);
                } else {
                    corrMetadata->set_sensitive(false);
                    corrOffRB->set_active(true);
                }
            } else {
                corrMetadata->set_sensitive(false);
            }
            break;
        }

        case procparams::LensProfParams::LcMode::NONE: {
            corrOffRB->set_active(true);
            setManualParamsVisibility(false);

            ckbUseDist->set_sensitive(false);
            ckbUseVign->set_sensitive(false);
            ckbUseCA->set_sensitive(false);
            break;
        }
    }

    if (pp->lensProf.lcMode == procparams::LensProfParams::LcMode::LCP) {
        if (pp->lensProf.lcpFile.empty()) {
            const Glib::ustring lastFolder = corrLcpFileChooser->get_current_folder();
            corrLcpFileChooser->set_current_folder(lastFolder);
            corrLcpFileChooser->unselect_all();
            bindCurrentFolder(*corrLcpFileChooser, App::get().mut_options().lastLensProfileDir);
            updateLCPDisabled(false);
        }
        else if (LCPStore::getInstance()->isValidLCPFileName(pp->lensProf.lcpFile)) {
            corrLcpFileChooser->set_filename(pp->lensProf.lcpFile);

            if (corrLcpFileRB->get_active()) {
                updateLCPDisabled(true);
            }
        }
        else {
            corrLcpFileChooser->unselect_filename(corrLcpFileChooser->get_filename());
            updateLCPDisabled(false);
        }
    }

    const LFDatabase* const db = LFDatabase::getInstance();
    LFCamera c;

    if (pp->lensProf.lfAutoMatch()) {
        if (metadata) {
            c = db->findCamera(metadata->getMake(), metadata->getModel(), true);
            setLensfunCamera(c.getMake(), c.getModel());
        }
    } else if (pp->lensProf.lfManual()) {
        setLensfunCamera(pp->lensProf.lfCameraMake, pp->lensProf.lfCameraModel);
    }

    if (pp->lensProf.lfAutoMatch()) {
        if (metadata) {
            const LFLens l = db->findLens(c, metadata->getLens(), true);
            setLensfunLens(l.getLens());
        }
    } else if (pp->lensProf.lfManual()) {
        setLensfunLens(pp->lensProf.lfLens);
    }


    /*
   if (!batchMode && !checkLensfunCanCorrect(true)) {
        if (corrLensfunAutoRB->get_active()) {
            corrOffRB->set_active(true);
        }

        corrLensfunAutoRB->set_sensitive(false);
    }

    if (!batchMode && corrLensfunManualRB->get_active() && !checkLensfunCanCorrect(false)) {
        corrOffRB->set_active(true);
    }
    */

    ckbUseDist->set_active(pp->lensProf.useDist);
    ckbUseVign->set_active(pp->lensProf.useVign);
    ckbUseCA->set_active(pp->lensProf.useCA);

    if (pedited) {
        corrUnchangedRB->set_active(!pedited->lensProf.lcMode);
        ckbUseDist->set_inconsistent(!pedited->lensProf.useDist);
        ckbUseVign->set_inconsistent(!pedited->lensProf.useVign);
        ckbUseCA->set_inconsistent(!pedited->lensProf.useCA);

        if (!pedited->lensProf.lfCameraMake || !pedited->lensProf.lfCameraModel) {
            setLensfunCamera("", "");
        }
        if (!pedited->lensProf.lfLens) {
            setLensfunLens("");
        }

        ckbUseDist->set_sensitive(true);
        ckbUseVign->set_sensitive(true);
        ckbUseCA->set_sensitive(true);
    }

    lcModeChanged = lcpFileChanged = useDistChanged = useVignChanged = useCAChanged = false;
    useLensfunChanged = lensfunAutoChanged = lensfunCameraChanged = lensfunLensChanged = false;

    updateLensfunWarning();
    enableListener();
    conUseDist.block(false);
}

void LensProfilePanel::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    if (corrLcpFileRB->get_active()) {
        pp->lensProf.lcMode = procparams::LensProfParams::LcMode::LCP;
    }
    else if (corrMetadata->get_active()) {
        pp->lensProf.lcMode = procparams::LensProfParams::LcMode::METADATA;
    }
    else if (corrLensfunManualRB->get_active()) {
        pp->lensProf.lcMode = procparams::LensProfParams::LcMode::LENSFUNMANUAL;
    }
    else if (corrLensfunAutoRB->get_active()) {
        pp->lensProf.lcMode = procparams::LensProfParams::LcMode::LENSFUNAUTOMATCH;
    }
    else if (corrOffRB->get_active()) {
        pp->lensProf.lcMode = procparams::LensProfParams::LcMode::NONE;
    }

    if (LCPStore::getInstance()->isValidLCPFileName(corrLcpFileChooser->get_filename())) {
        pp->lensProf.lcpFile = corrLcpFileChooser->get_filename();
    } else {
        pp->lensProf.lcpFile = "";
    }

    pp->lensProf.useDist = ckbUseDist->get_active();
    pp->lensProf.useVign = ckbUseVign->get_active();
    pp->lensProf.useCA   = ckbUseCA->get_active();

    const auto itc = lensfunCameras->get_active();

    if (itc && !corrLensfunAutoRB->get_active()) {
        pp->lensProf.lfCameraMake = (*itc)[lf->lensfunModelCam.make];
        pp->lensProf.lfCameraModel = (*itc)[lf->lensfunModelCam.model];
    } else {
        pp->lensProf.lfCameraMake = "";
        pp->lensProf.lfCameraModel = "";
    }

    const auto itl = lensfunLenses->get_active();

    if (itl && !corrLensfunAutoRB->get_active()) {
        pp->lensProf.lfLens = (*itl)[lf->lensfunModelLens.lens];
    } else {
        pp->lensProf.lfLens = "";
    }

    if (pedited) {
        pedited->lensProf.lcMode = lcModeChanged;
        pedited->lensProf.lcpFile = lcpFileChanged;
        pedited->lensProf.useDist = useDistChanged;
        pedited->lensProf.useVign = useVignChanged;
        pedited->lensProf.useCA   = useCAChanged;
        pedited->lensProf.useLensfun = useLensfunChanged;
        pedited->lensProf.lfAutoMatch = lensfunAutoChanged;
        pedited->lensProf.lfCameraMake = lensfunCameraChanged;
        pedited->lensProf.lfCameraModel = lensfunCameraChanged;
        pedited->lensProf.lfLens = lensfunLensChanged;
    }
}

void LensProfilePanel::setRawMeta(bool raw, const rtengine::FramesMetaData* pMeta)
{
    if ((!raw || pMeta->getFocusDist() <= 0) && !batchMode) {
        disableListener();

        // CA is very focus layer dependent, otherwise it might even worsen things
        allowFocusDep = false;

        enableListener();
    }

    corrMetadata->set_sensitive(false);
    if (pMeta) {
        metadataCorrection = MetadataLensCorrectionFinder::findCorrection(pMeta);
        if (metadataCorrection) {
            corrMetadata->set_sensitive(true);
        }
    }

    isRaw = raw;
    metadata = pMeta;
}

void LensProfilePanel::onLCPFileChanged()
{
    lcpFileChanged = true;
    const bool valid = LCPStore::getInstance()->isValidLCPFileName(corrLcpFileChooser->get_filename());
    updateLCPDisabled(valid);

    if (listener) {
        if (valid) {
            disableListener();
            corrLcpFileRB->set_active(true);
            enableListener();
        }

        listener->panelChanged(EvLCPFile, Glib::path_get_basename(corrLcpFileChooser->get_filename()));
    }
}

void LensProfilePanel::onUseDistChanged()
{
    useDistChanged = true;
    if (ckbUseDist->get_inconsistent()) {
        ckbUseDist->set_inconsistent(false);
        ckbUseDist->set_active(false);
    }

    if (listener) {
        listener->panelChanged(EvLCPUseDist, ckbUseDist->get_active() ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }
}

void LensProfilePanel::onUseVignChanged()
{
    useVignChanged = true;
    if (ckbUseVign->get_inconsistent()) {
        ckbUseVign->set_inconsistent(false);
        ckbUseVign->set_active(false);
    }

    if (listener) {
        listener->panelChanged(EvLCPUseVign, ckbUseVign->get_active() ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }
}

void LensProfilePanel::onUseCAChanged()
{
    useCAChanged = true;
    if (ckbUseCA->get_inconsistent()) {
        ckbUseCA->set_inconsistent(false);
        ckbUseCA->set_active(false);
    }

    if (listener) {
        listener->panelChanged(EvLCPUseCA, ckbUseCA->get_active() ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }
}

void LensProfilePanel::setBatchMode(bool yes)
{
    FoldableToolPanel::setBatchMode(yes);

    corrUnchangedRB->set_group(corrGroup);
    modesGrid->attach_next_to(*corrUnchangedRB, Gtk::POS_TOP, 3, 1);
    corrUnchangedRB->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &LensProfilePanel::onCorrModeChanged), corrUnchangedRB));
    corrUnchangedRB->set_active(true);
    corrUnchangedRB->show();
}

void LensProfilePanel::onLensfunCameraChanged()
{
    const auto iter = lensfunCameras->get_active();

    if (iter) {
        lensfunCameraChanged = true;

        if (listener) {
            disableListener();
            corrLensfunManualRB->set_active(true);
            enableListener();

            const Glib::ustring name = (*iter)[lf->lensfunModelCam.model];
            listener->panelChanged(EvLensCorrLensfunCamera, name);
        }
    }

    updateLensfunWarning();
}

void LensProfilePanel::onLensfunLensChanged()
{
    const auto iter = lensfunLenses->get_active();

    if (iter) {
        lensfunLensChanged = true;

        if (listener) {
            disableListener();
            corrLensfunManualRB->set_active(true);
            enableListener();

            const Glib::ustring name = (*iter)[lf->lensfunModelLens.prettylens];
            listener->panelChanged(EvLensCorrLensfunLens, name);
        }
    }

    updateLensfunWarning();
}

void LensProfilePanel::onCorrModeChanged(const Gtk::RadioButton* rbChanged)
{
    if (rbChanged->get_active()) {
        // because the method gets called for the enabled AND the disabled RadioButton, we do the processing only for the enabled one
        Glib::ustring mode;

        if (rbChanged == corrOffRB) {
            lcModeChanged = true;
            useLensfunChanged = true;
            lensfunAutoChanged = true;
            lcpFileChanged = false;

            ckbUseDist->set_sensitive(false);
            ckbUseVign->set_sensitive(false);
            ckbUseCA->set_sensitive(false);

            mode = M("GENERAL_NONE");

        } else if (rbChanged == corrLensfunAutoRB) {
            lcModeChanged = true;
            useLensfunChanged = true;
            lensfunAutoChanged = true;
            lensfunCameraChanged = true;
            lensfunLensChanged = true;
            lcpFileChanged = false;

            ckbUseDist->set_sensitive(true);
            ckbUseVign->set_sensitive(true);
            ckbUseCA->set_sensitive(true);


            const bool disabled = disableListener();
            if (batchMode) {
                setLensfunCamera("", "");
                setLensfunLens("");
            } else if (metadata) {
                const LFDatabase* const db = LFDatabase::getInstance();
                const LFCamera c = db->findCamera(metadata->getMake(), metadata->getModel(), true);
                const LFLens l = db->findLens(c, metadata->getLens(), true);
                setLensfunCamera(c.getMake(), c.getModel());
                setLensfunLens(l.getLens());
            }
            if (disabled) {
                enableListener();
            }

            mode = M("TP_LENSPROFILE_CORRECTION_AUTOMATCH");

        } else if (rbChanged == corrLensfunManualRB) {
            lcModeChanged = true;
            useLensfunChanged = true;
            lensfunAutoChanged = true;
            lensfunCameraChanged = true;
            lensfunLensChanged = true;
            lcpFileChanged = false;

            ckbUseDist->set_sensitive(true);
            ckbUseVign->set_sensitive(true);
            ckbUseCA->set_sensitive(false);

            mode = M("TP_LENSPROFILE_CORRECTION_MANUAL");

        } else if (rbChanged == corrLcpFileRB) {
            lcModeChanged = true;
            useLensfunChanged = true;
            lensfunAutoChanged = true;
            lcpFileChanged = true;

            updateLCPDisabled(true);

            mode = M("TP_LENSPROFILE_CORRECTION_LCPFILE");

        } else if (rbChanged == corrMetadata) {
            lcModeChanged = true;
            useLensfunChanged = true;
            lensfunAutoChanged = true;
            lcpFileChanged = true;

            updateMetadataDisabled();

            mode = M("TP_LENSPROFILE_CORRECTION_METADATA");

        } else if (rbChanged == corrUnchangedRB) {
            lcModeChanged = false;
            useLensfunChanged = false;
            lensfunAutoChanged = false;
            lcpFileChanged = false;
            lensfunCameraChanged = false;
            lensfunLensChanged = false;

            ckbUseDist->set_sensitive(true);
            ckbUseVign->set_sensitive(true);
            ckbUseCA->set_sensitive(true);

            mode = M("GENERAL_UNCHANGED");
        }

        updateLensfunWarning();

        if (rbChanged == corrLensfunManualRB || (!batchMode && rbChanged == corrLensfunAutoRB)) {
            setManualParamsVisibility(true);
        } else {
            setManualParamsVisibility(false);
        }

        if (listener) {
            listener->panelChanged(EvLensCorrMode, mode);
        }
    }
}

//-----------------------------------------------------------------------------
// LFDbHelper
//-----------------------------------------------------------------------------

LensProfilePanel::LFDbHelper::LFDbHelper()
{
    lensfunCameraModel = Gtk::TreeStore::create(lensfunModelCam);
    lensfunLensModel = Gtk::TreeStore::create(lensfunModelLens);

#ifdef _OPENMP
#pragma omp parallel sections if (!settings->verbose)
#endif
    {
#ifdef _OPENMP
#pragma omp section
#endif
        {
            fillLensfunCameras();
        }
#ifdef _OPENMP
#pragma omp section
#endif
        {
            fillLensfunLenses();
        }
    }
}

void LensProfilePanel::LFDbHelper::fillLensfunCameras()
{
    if (settings->verbose) {
        std::cout << "LENSFUN, scanning cameras:" << std::endl;
    }

    std::map<Glib::ustring, std::set<Glib::ustring>> camnames;
    const auto camlist = LFDatabase::getInstance()->getCameras();

    for (const auto& c : camlist) {
        camnames[c.getMake()].insert(c.getModel());

        if (settings->verbose) {
            std::cout << "  found: " << c.getDisplayString().c_str() << std::endl;
        }
    }

    for (const auto& p : camnames) {
        Gtk::TreeModel::Row row = *(lensfunCameraModel->append());
        row[lensfunModelCam.make] = p.first;
        row[lensfunModelCam.model] = p.first;

        for (const auto& c : p.second) {
            Gtk::TreeModel::Row child = *(lensfunCameraModel->append(row.children()));
            child[lensfunModelCam.make] = p.first;
            child[lensfunModelCam.model] = c;
        }
    }
}

void LensProfilePanel::LFDbHelper::fillLensfunLenses()
{
    if (settings->verbose) {
        std::cout << "LENSFUN, scanning lenses:" << std::endl;
    }

    std::map<Glib::ustring, std::set<Glib::ustring>> lenses;
    const auto lenslist = LFDatabase::getInstance()->getLenses();

    for (const auto& l : lenslist) {
        const auto& name = l.getLens();
        const auto& make = l.getMake();
        lenses[make].insert(name);

        if (settings->verbose) {
            std::cout << "  found: " << l.getDisplayString().c_str() << std::endl;
        }
    }

    for (const auto& p : lenses) {
        Gtk::TreeModel::Row row = *(lensfunLensModel->append());
        row[lensfunModelLens.lens] = p.first;
        row[lensfunModelLens.prettylens] = p.first;

        for (auto &c : p.second) {
            Gtk::TreeModel::Row child = *(lensfunLensModel->append(row.children()));
            child[lensfunModelLens.lens] = c;

            if (c.find(p.first, p.first.size() + 1) == p.first.size() + 1) {
                child[lensfunModelLens.prettylens] = c.substr(p.first.size() + 1);
            } else {
                child[lensfunModelLens.prettylens] = c;
            }
        }
    }
}

void LensProfilePanel::updateLCPDisabled(bool enable)
{
    if (!batchMode) {
        ckbUseDist->set_sensitive(enable);
        ckbUseVign->set_sensitive(enable && isRaw);
        ckbUseCA->set_sensitive(enable && allowFocusDep);
    }
}

void LensProfilePanel::updateMetadataDisabled()
{
    if (!batchMode) {
        if (metadataCorrection) {
            ckbUseDist->set_sensitive(metadataCorrection->hasDistortionCorrection());
            ckbUseVign->set_sensitive(metadataCorrection->hasVignettingCorrection());
            ckbUseCA->set_sensitive(metadataCorrection->hasCACorrection());
        } else {
            ckbUseDist->set_sensitive(false);
            ckbUseVign->set_sensitive(false);
            ckbUseCA->set_sensitive(false);
        }
    }
}

bool LensProfilePanel::setLensfunCamera(const Glib::ustring& make, const Glib::ustring& model)
{
    if (!make.empty() && !model.empty()) {
        const auto camera_it = lensfunCameras->get_active();

        if (camera_it && (*camera_it)[lf->lensfunModelCam.make] == make && (*camera_it)[lf->lensfunModelCam.model] == model) {
            return true;
        }

        // search for the active row
        for (const auto& row : lf->lensfunCameraModel->children()) {
            if (row[lf->lensfunModelCam.make] == make) {
                const auto& c = row.children();

                for (auto model_it = c.begin(), end = c.end(); model_it != end; ++model_it) {
                    const auto& childrow = *model_it;

                    if (childrow[lf->lensfunModelCam.model] == model) {
                        lensfunCameras->set_active(model_it);
                        return true;
                    }
                }

                break;
            }
        }
    }

    lensfunCameras->set_active(-1);
    return false;
}

bool LensProfilePanel::setLensfunLens(const Glib::ustring& lens)
{
    if (!lens.empty()) {
        const auto lens_it = lensfunLenses->get_active();

        if (lens_it && (*lens_it)[lf->lensfunModelLens.lens] == lens) {
            return true;
        }

        bool first_maker_found = false;

        for (const auto& row : lf->lensfunLensModel->children()) {
            if (lens.find(row[lf->lensfunModelLens.lens]) == 0) {
                const auto& c = row.children();

                for (auto model_it = c.begin(), end = c.end(); model_it != end; ++model_it) {
                    const auto& childrow = *model_it;

                    if (childrow[lf->lensfunModelLens.lens] == lens) {
                        lensfunLenses->set_active(model_it);
                        return true;
                    }
                }

                // we do not break immediately here, because there might be multiple makers
                // sharing the same prefix (e.g. "Leica" and "Leica Camera AG").
                // therefore, we break below when the lens doesn't match any of them
                first_maker_found = true;
            } else if (first_maker_found) {
                break;
            }
        }
    }

    lensfunLenses->set_active(-1);
    return false;
}

bool LensProfilePanel::checkLensfunCanCorrect(bool automatch)
{
    if (!metadata) {
        return false;
    }

    rtengine::procparams::ProcParams lpp;
    write(&lpp);
    const std::unique_ptr<LFModifier> mod(LFDatabase::getInstance()->findModifier(lpp.lensProf, metadata, 100, 100, lpp.coarse, -1));
    return static_cast<bool>(mod);
}

void LensProfilePanel::setManualParamsVisibility(bool setVisible)
{
    if (setVisible) {
        lensfunCamerasLbl->show();
        lensfunCameras->show();
        lensfunLensesLbl->show();
        lensfunLenses->show();
        updateLensfunWarning();
    } else {
        lensfunCamerasLbl->hide();
        lensfunCameras->hide();
        lensfunLensesLbl->hide();
        lensfunLenses->hide();
        warning->hide();
    }
}

void LensProfilePanel::updateLensfunWarning()
{
    warning->hide();

    if (corrLensfunManualRB->get_active() || corrLensfunAutoRB->get_active()) {
        const LFDatabase* const db = LFDatabase::getInstance();

        const auto itc = lensfunCameras->get_active();

        if (!itc) {
            return;
        }

        const LFCamera c = db->findCamera((*itc)[lf->lensfunModelCam.make], (*itc)[lf->lensfunModelCam.model], false);
        const auto itl = lensfunLenses->get_active();

        if (!itl) {
            return;
        }

        const LFLens l = db->findLens(c, (*itl)[lf->lensfunModelLens.lens], false);
        const float lenscrop = l.getCropFactor();
        const float camcrop = c.getCropFactor();

        if (lenscrop <= 0 || camcrop <= 0 || lenscrop / camcrop >= 1.01f) {
            warning->show();
        }

        ckbUseVign->set_sensitive(l.hasVignettingCorrection());
        ckbUseDist->set_sensitive(l.hasDistortionCorrection());
        ckbUseCA->set_sensitive(l.hasCACorrection());

        if (!isRaw || !l.hasVignettingCorrection()) {
            ckbUseVign->set_active(false);
        }

        if (!l.hasDistortionCorrection()) {
            ckbUseDist->set_active(false);
        }

        if (!l.hasCACorrection()) {
            ckbUseCA->set_active(false);
        }
    }
}

LensProfilePanel::LFDbHelper* LensProfilePanel::lf(nullptr);
