/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2012 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2012 Michael Ezra <michael@michaelezra.com>
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

#include "widgets/basic/smartscroll.h"

class RTSurface;
class ExportPanelListener
{
public:
    virtual ~ExportPanelListener() = default;

    virtual void exportRequested() = 0;
};

class ExportPanel : public Gtk::Box
{

protected:

    Gtk::Box* bypass_box;
    //Gtk::CheckButton* enabled;
    Gtk::RadioButton* use_fast_pipeline;
    Gtk::RadioButton* use_normal_pipeline;
    Gtk::CheckButton* bypass_ALL;
    Gtk::CheckButton* bypass_sharpenEdge;
    Gtk::CheckButton* bypass_sharpenMicro;
    Gtk::CheckButton* bypass_sharpening;
    //Gtk::CheckButton* bypass_lumaDenoise;
    //Gtk::CheckButton* bypass_colorDenoise;
    Gtk::CheckButton* bypass_defringe;
    Gtk::CheckButton* bypass_dirpyrDenoise;

    /*      icm_input   = "(camera)";
            icm_working = "sRGB";
            icm_output  = options.rtSettings.srgb;
            icm_gamma   = "default";
    */
    Gtk::CheckButton* bypass_dirpyrequalizer; // also could leave untouched but disable only small radius adjustments
    //Gtk::CheckButton* bypass_raw_all_enhance;
    Gtk::CheckButton* bypass_wavelet; // also could leave untouched but disable only small radius adjustments

    MyComboBoxText* raw_bayer_method;

    Gtk::CheckButton* bypass_raw_bayer_dcb_iterations;
    Gtk::CheckButton* bypass_raw_bayer_dcb_enhance;
    Gtk::CheckButton* bypass_raw_bayer_lmmse_iterations;
    //Gtk::CheckButton* bypass_raw_bayer_all_enhance;
    Gtk::CheckButton* bypass_raw_bayer_linenoise;
    Gtk::CheckButton* bypass_raw_bayer_greenthresh;

    Gtk::CheckButton* bypass_raw_ccSteps;
    Gtk::CheckButton* bypass_raw_ca; //wraps raw.cared, raw.cablue, raw.ca_autocorrect
    Gtk::CheckButton* bypass_raw_df; //wraps raw.dark_frame, raw.df_AutoSelect
    Gtk::CheckButton* bypass_raw_ff; //wraps raw.ff_file, raw.ff_AutoSelect

    MyComboBoxText* raw_xtrans_method;

    Gtk::Button* btnFastExport;

    MySpinButton* MaxWidth;
    MySpinButton* MaxHeight;

    sigc::connection enabledconn, bypass_ALLconn, FastExportconn, ExportLoadSettingsconn, ExportSaveSettingsconn;
    sigc::connection bypass_sharpeningConn        ;
    sigc::connection bypass_sharpenEdgeConn       ;
    sigc::connection bypass_sharpenMicroConn      ;
    //sigc::connection bypass_lumaDenoiseConn     ;
    //sigc::connection bypass_colorDenoiseConn    ;
    sigc::connection bypass_defringeConn          ;
    sigc::connection bypass_dirpyrDenoiseConn     ;
    sigc::connection bypass_dirpyrequalizerConn   ;
    sigc::connection bypass_waveletConn   ;
    //sigc::connection bypass_raw_bayer_all_enhanceConn   ;
    sigc::connection bypass_raw_bayer_dcb_iterationsConn  ;
    sigc::connection bypass_raw_bayer_dcb_enhanceConn     ;
    sigc::connection bypass_raw_bayer_lmmse_iterationsConn;
    sigc::connection bypass_raw_bayer_linenoiseConn       ;
    sigc::connection bypass_raw_bayer_greenthreshConn     ;
    sigc::connection bypass_raw_ccStepsConn       ;
    sigc::connection bypass_raw_caConn            ;
    sigc::connection bypass_raw_dfConn            ;
    sigc::connection bypass_raw_ffConn            ;


    ExportPanelListener* listener;

    std::shared_ptr<RTSurface> ornamentSurface;

    void bypassALL_Toggled();
    void use_fast_pipeline_toggled();
    void SaveSettingsAsDefault();
    void LoadDefaultSettings();
    void LoadSettings();
    void SaveSettings();

public:
    ExportPanel ();

    void FastExportPressed ();
    //bool isEnabled ();

    void setExportPanelListener (ExportPanelListener* l)
    {
        listener = l;
    }
};
