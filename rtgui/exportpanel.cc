/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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

#include "exportpanel.h"

#include "guiutils.h"
#include "multilangmgr.h"
#include "options.h"
#include "rtimage.h"
#include "rtsurface.h"

#include "rtengine/procparams.h"

using namespace rtengine;
using namespace rtengine::procparams;

ExportPanel::ExportPanel () : listener (nullptr), ornamentSurface(new RTSurface("ornament1.svg"))
{

    set_orientation(Gtk::ORIENTATION_VERTICAL);
    /*enabled = Gtk::manage ( new Gtk::CheckButton (M("EXPORT_ENABLE")) );
    pack_start(*enabled, Gtk::PACK_SHRINK, 4);
    pack_start (*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK, 2);*/

    Gtk::Label* labExportTitle = Gtk::manage ( new Gtk::Label (M ("EXPORT_FASTEXPORTOPTIONS")) );
    labExportTitle->set_use_markup (true);
    labExportTitle->set_tooltip_text (M ("EXPORT_INSTRUCTIONS"));
    labExportTitle->set_alignment (Gtk::ALIGN_START);
    pack_start (*labExportTitle, Gtk::PACK_SHRINK, 4);

    Gtk::RadioButton::Group pipeline_group;
    use_fast_pipeline       = Gtk::manage ( new Gtk::RadioButton (pipeline_group, M ("EXPORT_USE_FAST_PIPELINE")));
    use_normal_pipeline     = Gtk::manage ( new Gtk::RadioButton (pipeline_group, M ("EXPORT_USE_NORMAL_PIPELINE")));
    bypass_box = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    bypass_ALL              = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_ALL")));
    use_fast_pipeline->set_tooltip_text (M ("EXPORT_USE_FAST_PIPELINE_TOOLTIP"));
    bypass_sharpening       = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_SHARPENING")));
    bypass_sharpenEdge      = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_SHARPENEDGE")));
    bypass_sharpenMicro     = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_SHARPENMICRO")));
    //bypass_lumaDenoise      = Gtk::manage ( new Gtk::CheckButton (M("EXPORT_BYPASS_LUMADENOISE")));
    //bypass_colorDenoise     = Gtk::manage ( new Gtk::CheckButton (M("EXPORT_BYPASS_COLORDENOISE")));
    bypass_defringe         = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_DEFRINGE")));
    bypass_dirpyrDenoise    = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_DIRPYRDENOISE")));
    bypass_dirpyrequalizer  = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_DIRPYREQUALIZER")));
    bypass_wavelet  = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_EQUALIZER")));
    bypass_raw_ccSteps      = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_CCSTEPS")));
    bypass_raw_ca           = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_CA")));
    bypass_raw_df           = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_DF")));
    bypass_raw_ff           = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_FF")));

    // ---------------------- Bayer sensor frame -----------------------

    Gtk::Frame *bayerFrame = Gtk::manage ( new Gtk::Frame (M ("TP_RAW_SENSOR_BAYER_LABEL")));
    bayerFrame->set_label_align(0.025, 0.5);
    Gtk::Box* bayerFrameVBox = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    Gtk::Box* hb_raw_bayer_method = Gtk::manage (new Gtk::Box ());
    hb_raw_bayer_method->pack_start (*Gtk::manage (new Gtk::Label ( M ("EXPORT_RAW_DMETHOD") + ": ")), Gtk::PACK_SHRINK, 4);
    raw_bayer_method = Gtk::manage (new MyComboBoxText ());

    for (const auto method_string : RAWParams::BayerSensor::getMethodStrings()) {
        raw_bayer_method->append(method_string);
    }

    raw_bayer_method->set_active (0);
    hb_raw_bayer_method->pack_end (*raw_bayer_method, Gtk::PACK_EXPAND_WIDGET, 4);

    //bypass_raw_all_enhance  = Gtk::manage ( new Gtk::CheckButton (M("EXPORT_BYPASS_RAW_ALL_ENHANCE")));
    bypass_raw_bayer_linenoise    = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_LINENOISE")));
    bypass_raw_bayer_greenthresh  = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_GREENTHRESH")));
    bypass_raw_bayer_dcb_iterations = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_DCB_ITERATIONS")));
    bypass_raw_bayer_dcb_enhance    = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_DCB_ENHANCE")));
    bypass_raw_bayer_lmmse_iterations = Gtk::manage ( new Gtk::CheckButton (M ("EXPORT_BYPASS_RAW_LMMSE_ITERATIONS")));

    // ---------------------- Bayer sensor frame -----------------------

    Gtk::Frame *xtransFrame = Gtk::manage ( new Gtk::Frame (M ("TP_RAW_SENSOR_XTRANS_LABEL")));
    xtransFrame->set_label_align(0.025, 0.5);
    Gtk::Box* xtransFrameVBox = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    Gtk::Box* hb_raw_xtrans_method = Gtk::manage (new Gtk::Box ());
    hb_raw_xtrans_method->pack_start (*Gtk::manage (new Gtk::Label ( M ("EXPORT_RAW_DMETHOD") + ": ")), Gtk::PACK_SHRINK, 4);
    raw_xtrans_method = Gtk::manage (new MyComboBoxText ());

    for (const auto method_string : RAWParams::XTransSensor::getMethodStrings()) {
        raw_xtrans_method->append(method_string);
    }

    raw_xtrans_method->set_active (0);
    hb_raw_xtrans_method->pack_end (*raw_xtrans_method, Gtk::PACK_EXPAND_WIDGET, 4);

    // ----------------------------------------------------------------

    // start global packing
    Gtk::Box* lblbox = Gtk::manage (new Gtk::Box ());
    lblbox->pack_start (*Gtk::manage (new Gtk::Label (M ("EXPORT_PIPELINE"))), Gtk::PACK_SHRINK, 4);
    pack_start (*lblbox, Gtk::PACK_SHRINK, 4);
    pack_start (*use_fast_pipeline, Gtk::PACK_SHRINK, 4);
    pack_start (*use_normal_pipeline, Gtk::PACK_SHRINK, 4);

    bypass_box->pack_start (*Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK, 4);
    lblbox = Gtk::manage (new Gtk::Box ());
    lblbox->pack_start (*Gtk::manage (new Gtk::Label (M ("EXPORT_BYPASS"))), Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*lblbox, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_ALL, Gtk::PACK_SHRINK, 4);
    // bypass_box->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_sharpening, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_sharpenEdge, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_sharpenMicro, Gtk::PACK_SHRINK, 4);
    //pack_start(*bypass_lumaDenoise  , Gtk::PACK_SHRINK, 4);
    //pack_start(*bypass_colorDenoise , Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_defringe, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_dirpyrDenoise, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_dirpyrequalizer, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_wavelet, Gtk::PACK_SHRINK, 4);

    bayerFrameVBox->pack_start (*hb_raw_bayer_method, Gtk::PACK_SHRINK, 4);
    //bayerFrameVBox->pack_start(*bypass_raw_all_enhance , Gtk::PACK_SHRINK, 4);
    bayerFrameVBox->pack_start (*bypass_raw_bayer_dcb_iterations, Gtk::PACK_SHRINK, 4);
    bayerFrameVBox->pack_start (*bypass_raw_bayer_dcb_enhance, Gtk::PACK_SHRINK, 4);
    bayerFrameVBox->pack_start (*bypass_raw_bayer_lmmse_iterations, Gtk::PACK_SHRINK, 4);
    bayerFrameVBox->pack_start (*bypass_raw_bayer_linenoise, Gtk::PACK_SHRINK, 4);
    bayerFrameVBox->pack_start (*bypass_raw_bayer_greenthresh, Gtk::PACK_SHRINK, 4);
    bayerFrame->add (*bayerFrameVBox);
    bypass_box->pack_start(*bayerFrame, Gtk::PACK_SHRINK, 4);

    xtransFrameVBox->pack_start (*hb_raw_xtrans_method, Gtk::PACK_SHRINK, 4);
    xtransFrame->add (*xtransFrameVBox);
    bypass_box->pack_start(*xtransFrame, Gtk::PACK_SHRINK, 4);

    bypass_box->pack_start (*bypass_raw_ccSteps, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_raw_ca, Gtk::PACK_SHRINK, 4);

    bypass_box->pack_start (*bypass_raw_df, Gtk::PACK_SHRINK, 4);
    bypass_box->pack_start (*bypass_raw_ff, Gtk::PACK_SHRINK, 4);

    pack_start (*bypass_box, Gtk::PACK_SHRINK);

    pack_start (*Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK, 2);

    // Resize options

    Gtk::Box* rmbox = Gtk::manage (new Gtk::Box ());
    rmbox->pack_start (*Gtk::manage (new Gtk::Label (M ("TP_RESIZE_LABEL"))), Gtk::PACK_SHRINK, 4);
    pack_start (*rmbox, Gtk::PACK_SHRINK, 4);

    Gtk::Box* wbox = Gtk::manage (new Gtk::Box ());
    Gtk::Box* hbox = Gtk::manage (new Gtk::Box ());
    MaxWidth = Gtk::manage (new MySpinButton ());
    MaxHeight = Gtk::manage (new MySpinButton ());
    wbox->pack_start (*Gtk::manage (new Gtk::Label (M ("EXPORT_MAXWIDTH"))), Gtk::PACK_SHRINK, 4);
    wbox->pack_start (*MaxWidth);
    hbox->pack_start (*Gtk::manage (new Gtk::Label (M ("EXPORT_MAXHEIGHT"))), Gtk::PACK_SHRINK, 4);
    hbox->pack_start (*MaxHeight);
    pack_start (*wbox, Gtk::PACK_SHRINK, 4);
    pack_start (*hbox, Gtk::PACK_SHRINK, 4);

    const auto& options = App::get().options();
    MaxWidth->set_digits (0);
    MaxWidth->set_width_chars (5);
    MaxWidth->set_max_width_chars (5);
    MaxWidth->set_increments (1, 100);
    MaxWidth->set_value (options.fastexport_resize_width);
    MaxWidth->set_range (32, 10000);

    MaxHeight->set_digits (0);
    MaxHeight->set_width_chars (5);
    MaxHeight->set_max_width_chars (5);
    MaxHeight->set_increments (1, 100);
    MaxHeight->set_value (options.fastexport_resize_height);
    MaxHeight->set_range (32, 10000);

    // Buttons
    btnFastExport =  Gtk::manage ( new Gtk::Button () );
    btnFastExport->set_tooltip_text (M ("EXPORT_PUTTOQUEUEFAST"));
    btnFastExport->set_image (*Gtk::manage (new RTImage ("gears")));
    pack_start (*btnFastExport, Gtk::PACK_SHRINK, 4);


    // add panel ending
    Gtk::Box* vboxpe = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* hseptpe = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    Gtk::Image* peImg = Gtk::manage (new Gtk::Image (ornamentSurface->get()));
    vboxpe->pack_start (*hseptpe, Gtk::PACK_SHRINK, 4);
    vboxpe->pack_start (*peImg);
    pack_start (*vboxpe, Gtk::PACK_SHRINK, 0);


    use_fast_pipeline->signal_toggled().connect (sigc::mem_fun (*this, &ExportPanel::use_fast_pipeline_toggled));
    btnFastExport->signal_clicked().connect ( sigc::mem_fun (*this, &ExportPanel::FastExportPressed) );
    //btnExportLoadSettings->signal_clicked().connect( sigc::mem_fun(*this, &ExportPanel::LoadSettings) );
    //btnExportSaveSettings->signal_clicked().connect( sigc::mem_fun(*this, &ExportPanel::SaveSettings) );
    bypass_ALLconn = bypass_ALL->signal_toggled().connect (sigc::mem_fun (*this, &ExportPanel::bypassALL_Toggled));

    bypass_sharpeningConn           = bypass_sharpening->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_sharpenEdgeConn          = bypass_sharpenEdge->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_sharpenMicroConn         = bypass_sharpenMicro->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    //bypass_lumaDenoiseConn        = bypass_lumaDenoise->signal_toggled().connect (sigc::bind (sigc::mem_fun(*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    //bypass_colorDenoiseConn       = bypass_colorDenoise->signal_toggled().connect (sigc::bind (sigc::mem_fun(*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_defringeConn             = bypass_defringe->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_dirpyrDenoiseConn        = bypass_dirpyrDenoise->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_dirpyrequalizerConn      = bypass_dirpyrequalizer->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_waveletConn      = bypass_wavelet->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    //bypass_raw_all_enhanceConn    = bypass_raw_bayer_all_enhance->signal_toggled().connect (sigc::bind (sigc::mem_fun(*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_bayer_dcb_iterationsConn   = bypass_raw_bayer_dcb_iterations->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_bayer_dcb_enhanceConn      = bypass_raw_bayer_dcb_enhance->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_bayer_lmmse_iterationsConn = bypass_raw_bayer_lmmse_iterations->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_bayer_linenoiseConn        = bypass_raw_bayer_linenoise->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_bayer_greenthreshConn      = bypass_raw_bayer_greenthresh->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_ccStepsConn          = bypass_raw_ccSteps->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_caConn               = bypass_raw_ca->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_dfConn               = bypass_raw_df->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));
    bypass_raw_ffConn               = bypass_raw_ff->signal_toggled().connect (sigc::bind (sigc::mem_fun (*bypass_ALL, &Gtk::CheckButton::set_inconsistent), true));

    LoadDefaultSettings();
}

/*bool ExportPanel::isEnabled () {

    return enabled->get_active () && is_sensitive();
}*/


void ExportPanel::FastExportPressed ()
{
    // options is the container for making these settings available globally.
    // Therefore, settings must be saved to options before they are used further
    SaveSettingsAsDefault();

    if (listener) {
        listener->exportRequested ();
    }
}

void ExportPanel::SaveSettingsAsDefault()
{
    bool changed = false;
#define FE_OPT_STORE_(n, v) \
    do {                \
        if (n != v) {   \
            n = v;      \
            changed = true;                     \
        }                                       \
    } while (false)
    auto& options = App::get().mut_options();
    // Save fast export settings to options
    FE_OPT_STORE_ (options.fastexport_bypass_sharpening, bypass_sharpening->get_active        ());
    FE_OPT_STORE_ (options.fastexport_bypass_sharpenEdge, bypass_sharpenEdge->get_active       ());
    FE_OPT_STORE_ (options.fastexport_bypass_sharpenMicro, bypass_sharpenMicro->get_active      ());
    //options.fastexport_bypass_lumaDenoise        = bypass_lumaDenoise->get_active       ();
    //options.fastexport_bypass_colorDenoise       = bypass_colorDenoise->get_active      ();
    FE_OPT_STORE_ (options.fastexport_bypass_defringe, bypass_defringe->get_active          ());
    FE_OPT_STORE_ (options.fastexport_bypass_dirpyrDenoise, bypass_dirpyrDenoise->get_active     ());
    FE_OPT_STORE_ (options.fastexport_bypass_dirpyrequalizer, bypass_dirpyrequalizer->get_active   ());
    FE_OPT_STORE_ (options.fastexport_bypass_wavelet, bypass_wavelet->get_active   ());
    //options.fastexport_bypass_raw_bayer_all_enhance    = bypass_raw_all_enhance->get_active           ();
    FE_OPT_STORE_ (options.fastexport_bypass_raw_bayer_dcb_iterations, bypass_raw_bayer_dcb_iterations->get_active  ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_bayer_dcb_enhance, bypass_raw_bayer_dcb_enhance->get_active     ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_bayer_lmmse_iterations, bypass_raw_bayer_lmmse_iterations->get_active());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_bayer_linenoise, bypass_raw_bayer_linenoise->get_active       ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_bayer_greenthresh, bypass_raw_bayer_greenthresh->get_active     ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_ccSteps, bypass_raw_ccSteps->get_active       ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_ca, bypass_raw_ca->get_active            ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_df, bypass_raw_df->get_active            ());
    FE_OPT_STORE_ (options.fastexport_bypass_raw_ff, bypass_raw_ff->get_active            ());

    //saving Bayer demosaic_method
    int currentRow = raw_bayer_method->get_active_row_number();

    if (currentRow >= 0) {
        FE_OPT_STORE_ (options.fastexport_raw_bayer_method, procparams::RAWParams::BayerSensor::getMethodStrings()[currentRow]);
    }

    //saving X-Trans demosaic_method
    currentRow = raw_xtrans_method->get_active_row_number();

    if (currentRow >= 0) {
        FE_OPT_STORE_ (options.fastexport_raw_xtrans_method, procparams::RAWParams::XTransSensor::getMethodStrings()[currentRow]);
    }

//  options.fastexport_icm_input        = icm_input       ;
//  options.fastexport_icm_working      = icm_working     ;
//  options.fastexport_icm_output       = icm_output      ;
//  options.fastexport_icm_gamma        = icm_gamma       ;
//  options.fastexport_resize_enabled   = resize_enabled  ;
//  options.fastexport_resize_scale     = resize_scale    ;
//  options.fastexport_resize_appliesTo = resize_appliesTo;
//  options.fastexport_resize_dataspec  = resize_dataspec ;

    FE_OPT_STORE_ (options.fastexport_resize_method, "Lanczos");
    FE_OPT_STORE_ (options.fastexport_resize_width, MaxWidth->get_value_as_int ());
    FE_OPT_STORE_ (options.fastexport_resize_height, MaxHeight->get_value_as_int ());

    FE_OPT_STORE_ (options.fastexport_use_fast_pipeline, use_fast_pipeline->get_active());
#undef FE_OPT_STORE_

    if (changed) {
        try {
            Options::save();
        } catch (Options::Error &e) {
            Gtk::MessageDialog msgd (getToplevelWindow (this), e.get_msg(), true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE, true);
            msgd.run();
        }
    }
}

void ExportPanel::LoadDefaultSettings()
{
    const auto& options = App::get().options();
    // Load fast export settings from options
    bypass_sharpening->set_active        (options.fastexport_bypass_sharpening         );
    bypass_sharpenEdge->set_active       (options.fastexport_bypass_sharpenEdge        );
    bypass_sharpenMicro->set_active      (options.fastexport_bypass_sharpenMicro       );
    //bypass_lumaDenoise->set_active     (options.fastexport_bypass_lumaDenoise        );
    //bypass_colorDenoise->set_active    (options.fastexport_bypass_colorDenoise       );
    bypass_defringe->set_active          (options.fastexport_bypass_defringe           );
    bypass_dirpyrDenoise->set_active     (options.fastexport_bypass_dirpyrDenoise      );
    bypass_dirpyrequalizer->set_active   (options.fastexport_bypass_dirpyrequalizer    );
    bypass_wavelet->set_active   (options.fastexport_bypass_wavelet    );
    //bypass_raw_bayer_all_enhance->set_active   (options.fastexport_bypass_raw_bayer_all_enhance     );
    bypass_raw_bayer_dcb_iterations->set_active  (options.fastexport_bypass_raw_bayer_dcb_iterations  );
    bypass_raw_bayer_dcb_enhance->set_active     (options.fastexport_bypass_raw_bayer_dcb_enhance     );
    bypass_raw_bayer_lmmse_iterations->set_active (options.fastexport_bypass_raw_bayer_lmmse_iterations);
    bypass_raw_bayer_linenoise->set_active       (options.fastexport_bypass_raw_bayer_linenoise       );
    bypass_raw_bayer_greenthresh->set_active     (options.fastexport_bypass_raw_bayer_greenthresh     );
    bypass_raw_ccSteps->set_active       (options.fastexport_bypass_raw_ccSteps        );
    bypass_raw_ca->set_active            (options.fastexport_bypass_raw_ca             );
    bypass_raw_df->set_active            (options.fastexport_bypass_raw_df             );
    bypass_raw_ff->set_active            (options.fastexport_bypass_raw_ff             );

    // Bayer demosaic method
    raw_bayer_method->set_active(0);

    for (size_t i = 0; i < RAWParams::BayerSensor::getMethodStrings().size(); ++i)
        if (options.fastexport_raw_bayer_method == procparams::RAWParams::BayerSensor::getMethodStrings()[i]) {
            raw_bayer_method->set_active(i);
            break;
        }

    // X-Trans demosaic method
    raw_xtrans_method->set_active(0);

    for (size_t i = 0; i < procparams::RAWParams::XTransSensor::getMethodStrings().size(); ++i)
        if (options.fastexport_raw_xtrans_method == procparams::RAWParams::XTransSensor::getMethodStrings()[i]) {
            raw_xtrans_method->set_active(i);
            break;
        }

//    icm_input        = options.fastexport_icm_input       ;
//    icm_working      = options.fastexport_icm_working     ;
//    icm_output       = options.fastexport_icm_output      ;
//    icm_gamma        = options.fastexport_icm_gamma       ;
//    resize_enabled   = options.fastexport_resize_enabled  ;
//    resize_scale     = options.fastexport_resize_scale    ;
//    resize_appliesTo = options.fastexport_resize_appliesTo;
//    resize_dataspec  = options.fastexport_resize_dataspec ;

    MaxWidth->set_value (options.fastexport_resize_width);
    MaxHeight->set_value (options.fastexport_resize_height);

    if (options.fastexport_use_fast_pipeline) {
        use_fast_pipeline->set_active (true);
        bypass_box->set_sensitive (false);
    } else {
        use_normal_pipeline->set_active (true);
    }
}

void ExportPanel::LoadSettings()
{
    // load settings from file
}

void ExportPanel::SaveSettings()
{
    // save settings to file

}

void ExportPanel::bypassALL_Toggled()
{
    bypass_sharpeningConn.block         (true);
    bypass_sharpenEdgeConn.block        (true);
    bypass_sharpenMicroConn.block       (true);
    //bypass_lumaDenoiseConn.block        (true);
    //bypass_colorDenoiseConn.block       (true);
    bypass_defringeConn.block           (true);
    bypass_dirpyrDenoiseConn.block      (true);
    bypass_dirpyrequalizerConn.block    (true);
    bypass_waveletConn.block    (true);
    //bypass_raw_bayer_all_enhanceConn.block    (true);
    bypass_raw_bayer_dcb_iterationsConn.block   (true);
    bypass_raw_bayer_dcb_enhanceConn.block      (true);
    bypass_raw_bayer_lmmse_iterationsConn.block (true);
    bypass_raw_bayer_linenoiseConn.block        (true);
    bypass_raw_bayer_greenthreshConn.block      (true);
    bypass_raw_ccStepsConn.block        (true);
    bypass_raw_caConn.block             (true);
    bypass_raw_dfConn.block             (true);
    bypass_raw_ffConn.block             (true);

    bypass_ALL->set_inconsistent (false);

    bypass_sharpening->set_active (bypass_ALL->get_active());
    bypass_sharpenEdge->set_active (bypass_ALL->get_active());
    bypass_sharpenMicro->set_active (bypass_ALL->get_active());
    //bypass_lumaDenoise->set_active(bypass_ALL->get_active());
    //bypass_colorDenoise->set_active(bypass_ALL->get_active());
    bypass_defringe->set_active (bypass_ALL->get_active());
    bypass_dirpyrDenoise->set_active (bypass_ALL->get_active());
    bypass_dirpyrequalizer->set_active (bypass_ALL->get_active());
    bypass_wavelet->set_active (bypass_ALL->get_active());
    //bypass_raw_bayer_all_enhance->set_active(bypass_ALL->get_active());
    bypass_raw_bayer_dcb_iterations->set_active (bypass_ALL->get_active());
    bypass_raw_bayer_dcb_enhance->set_active (bypass_ALL->get_active());
    bypass_raw_bayer_lmmse_iterations->set_active (bypass_ALL->get_active());
    bypass_raw_bayer_linenoise->set_active (bypass_ALL->get_active());
    bypass_raw_bayer_greenthresh->set_active (bypass_ALL->get_active());
    bypass_raw_ccSteps->set_active (bypass_ALL->get_active());
    bypass_raw_ca->set_active (bypass_ALL->get_active());
    bypass_raw_df->set_active (bypass_ALL->get_active());
    bypass_raw_ff->set_active (bypass_ALL->get_active());

    bypass_sharpeningConn.block           (false);
    bypass_sharpenEdgeConn.block          (false);
    bypass_sharpenMicroConn.block         (false);
    //bypass_lumaDenoiseConn.block        (false);
    //bypass_colorDenoiseConn.block       (false);
    bypass_defringeConn.block             (false);
    bypass_dirpyrDenoiseConn.block        (false);
    bypass_dirpyrequalizerConn.block      (false);
    bypass_waveletConn.block      (false);
    //bypass_raw_bayer_all_enhanceConn.block    (false);
    bypass_raw_bayer_dcb_iterationsConn.block   (false);
    bypass_raw_bayer_dcb_enhanceConn.block      (false);
    bypass_raw_bayer_lmmse_iterationsConn.block (false);
    bypass_raw_bayer_linenoiseConn.block        (false);
    bypass_raw_bayer_greenthreshConn.block      (false);
    bypass_raw_ccStepsConn.block          (false);
    bypass_raw_caConn.block               (false);
    bypass_raw_dfConn.block               (false);
    bypass_raw_ffConn.block               (false);
}

void ExportPanel::use_fast_pipeline_toggled()
{
    bypass_box->set_sensitive (!use_fast_pipeline->get_active());
}

/*
fastexport_bypass_sharpening
fastexport_bypass_sharpenEdge
fastexport_bypass_sharpenMicro
fastexport_bypass_lumaDenoise
fastexport_bypass_colorDenoise
fastexport_bypass_defringe
fastexport_bypass_dirpyrDenoise
fastexport_bypass_dirpyrequalizer
fastexport_raw_bayer_method
fastexport_bypass_raw_bayer_all_enhance
fastexport_bypass_raw_bayer_dcb_iterations
fastexport_bypass_raw_bayer_dcb_enhance
fastexport_bypass_raw_bayer_linenoise
fastexport_bypass_raw_bayer_greenthresh
fastexport_raw_xtrans_method
fastexport_bypass_raw_ccSteps
fastexport_bypass_raw_ca
fastexport_bypass_raw_df
fastexport_bypass_raw_ff
fastexport_icm_input
fastexport_icm_working
fastexport_icm_output
fastexport_icm_gamma
fastexport_resize_enabled
fastexport_resize_scale
fastexport_resize_appliesTo
fastexport_resize_method
fastexport_resize_dataspec
fastexport_resize_width
fastexport_resize_height
*/
