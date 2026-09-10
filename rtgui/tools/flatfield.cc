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
#include <sstream>

#include "eventmapper.h"
#include "flatfield.h"

#include "guiutils.h"
#include "options.h"
#include "rtimage.h"
#include "widgets/basic/myfilechooser.h"

#include "rtengine/procparams.h"
#include "rtengine/rawimage.h"

using namespace rtengine;
using namespace rtengine::procparams;

const Glib::ustring FlatField::TOOL_NAME = "flatfield";

FlatField::FlatField () : FoldableToolPanel(this, TOOL_NAME, M("TP_FLATFIELD_LABEL"))
{
    auto m = ProcEventMapper::getInstance();
    EvFlatFieldFromMetaData = m->newEvent(DARKFRAME, "HISTORY_MSG_FF_FROMMETADATA");

    auto& options = App::get().mut_options();

    hbff = Gtk::manage(new Gtk::Box());
    flatFieldFile = Gtk::manage(new MyFileChooserButton(M("TP_FLATFIELD_LABEL"), Gtk::FILE_CHOOSER_ACTION_OPEN));
    bindCurrentFolder (*flatFieldFile, options.lastFlatfieldDir);
    ffLabel = Gtk::manage(new Gtk::Label(M("GENERAL_FILE")));
    flatFieldFileReset = Gtk::manage(new Gtk::Button());
    flatFieldFileReset->set_image (*Gtk::manage(new RTImage ("cancel-small", Gtk::ICON_SIZE_BUTTON)));
    hbff->pack_start(*ffLabel, Gtk::PACK_SHRINK);
    hbff->pack_start(*flatFieldFile);
    hbff->pack_start(*flatFieldFileReset, Gtk::PACK_SHRINK);
    flatFieldAutoSelect = Gtk::manage(new Gtk::CheckButton((M("TP_FLATFIELD_AUTOSELECT"))));
    flatFieldFromMetaData = Gtk::manage(new CheckBox((M("TP_FLATFIELD_FROMMETADATA")), multiImage));
    flatFieldFromMetaData->setCheckBoxListener (this);
    ffInfo = Gtk::manage(new Gtk::Label("-"));
    setExpandAlignProperties(ffInfo, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    flatFieldBlurRadius = Gtk::manage(new Adjuster (M("TP_FLATFIELD_BLURRADIUS"), 0, 200, 2, 32));
    flatFieldBlurRadius->setAdjusterListener (this);

    flatFieldBlurRadius->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    flatFieldBlurRadius->show();

    Gtk::Box* hbffbt = Gtk::manage (new Gtk::Box ());
    hbffbt->pack_start (*Gtk::manage (new Gtk::Label ( M("TP_FLATFIELD_BLURTYPE") + ":")), Gtk::PACK_SHRINK);
    flatFieldBlurType = Gtk::manage (new MyComboBoxText ());
    flatFieldBlurType->append(M("TP_FLATFIELD_BT_AREA"));
    flatFieldBlurType->append(M("TP_FLATFIELD_BT_VERTICAL"));
    flatFieldBlurType->append(M("TP_FLATFIELD_BT_HORIZONTAL"));
    flatFieldBlurType->append(M("TP_FLATFIELD_BT_VERTHORIZ"));
    flatFieldBlurType->set_active(0);
    hbffbt->pack_end (*flatFieldBlurType, Gtk::PACK_EXPAND_WIDGET);

    flatFieldClipControl = Gtk::manage (new Adjuster(M("TP_FLATFIELD_CLIPCONTROL"), 0., 100., 1., 0.));
    flatFieldClipControl->setAdjusterListener(this);
    flatFieldClipControl->addAutoButton("");

    flatFieldClipControl->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    flatFieldClipControl->show();
    flatFieldClipControl->set_tooltip_markup (M("TP_FLATFIELD_CLIPCONTROL_TOOLTIP"));

    pack_start( *flatFieldFromMetaData, Gtk::PACK_SHRINK);
    pack_start( *Gtk::manage( new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK, 0 );
    pack_start( *flatFieldAutoSelect, Gtk::PACK_SHRINK);
    pack_start( *hbff, Gtk::PACK_SHRINK);
    pack_start( *ffInfo, Gtk::PACK_SHRINK);
    pack_start( *hbffbt, Gtk::PACK_SHRINK);
    pack_start( *flatFieldBlurRadius, Gtk::PACK_SHRINK);
    pack_start( *flatFieldClipControl, Gtk::PACK_SHRINK);

    flatFieldFileconn = flatFieldFile->signal_file_set().connect ( sigc::mem_fun(*this, &FlatField::flatFieldFileChanged)); //, true);
    flatFieldFileReset->signal_clicked().connect( sigc::mem_fun(*this, &FlatField::flatFieldFile_Reset), true );
    flatFieldAutoSelectconn = flatFieldAutoSelect->signal_toggled().connect ( sigc::mem_fun(*this, &FlatField::flatFieldAutoSelectChanged), true);
    flatFieldBlurTypeconn = flatFieldBlurType->signal_changed().connect( sigc::mem_fun(*this, &FlatField::flatFieldBlurTypeChanged) );

    // Set filename filters
    b_filter_asCurrent = false;
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->add_pattern("*");
    filter_any->set_name(M("FILECHOOSER_FILTER_ANY"));
    flatFieldFile->add_filter (filter_any);

    // filters for all supported non-raw extensions
    for (size_t i = 0; i < options.parseExtensions.size(); i++) {
        if (options.parseExtensionsEnabled[i] && options.parseExtensions[i].uppercase() != "JPG" && options.parseExtensions[i].uppercase() != "JPEG" && options.parseExtensions[i].uppercase() != "PNG" && options.parseExtensions[i].uppercase() != "TIF" && options.parseExtensions[i].uppercase() != "TIFF"  ) {
            Glib::RefPtr<Gtk::FileFilter> filter_ff = Gtk::FileFilter::create();
            filter_ff->add_pattern("*." + options.parseExtensions[i]);
            filter_ff->add_pattern("*." + options.parseExtensions[i].uppercase());
            filter_ff->set_name(options.parseExtensions[i].uppercase());
            flatFieldFile->add_filter (filter_ff);
            //printf("adding filter %s \n",options.parseExtensions[i].uppercase().c_str());
        }
    }
}

FlatField::~FlatField ()
{
    idle_register.destroy();
}

void FlatField::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    disableListener ();
    flatFieldAutoSelectconn.block (true);
    flatFieldBlurTypeconn.block (true);

    //flatFieldBlurType
    for (size_t i = 0; i < procparams::RAWParams::getFlatFieldBlurTypeStrings().size(); ++i) {
        if (pp->raw.ff_BlurType == procparams::RAWParams::getFlatFieldBlurTypeStrings()[i]) {
            flatFieldBlurType->set_active(i);
            break;
        }
    }

    if (multiImage || pp->raw.ff_BlurType == procparams::RAWParams::getFlatFieldBlurTypeString(procparams::RAWParams::FlatFieldBlurType::AREA)) {
        flatFieldClipControl->show();
    } else {
        flatFieldClipControl->hide();
    }

    flatFieldAutoSelect->set_active (pp->raw.ff_AutoSelect);
    flatFieldFromMetaData->set_active (pp->raw.ff_FromMetaData);
    flatFieldBlurRadius->setValue (pp->raw.ff_BlurRadius);
    flatFieldClipControl->setValue (pp->raw.ff_clipControl);
    flatFieldClipControl->setAutoValue (pp->raw.ff_AutoClipControl);

    if(pedited ) {
        flatFieldAutoSelect->set_inconsistent (!pedited->raw.ff_AutoSelect);
        flatFieldFromMetaData->set_inconsistent (!pedited->raw.ff_FromMetaData);
        flatFieldBlurRadius->setEditedState( pedited->raw.ff_BlurRadius ? Edited : UnEdited );
        flatFieldClipControl->setEditedState( pedited->raw.ff_clipControl ? Edited : UnEdited );
        flatFieldClipControl->setAutoInconsistent(multiImage && !pedited->raw.ff_AutoClipControl);

        if( !pedited->raw.ff_BlurType ) {
            flatFieldBlurType->set_active_text(M("GENERAL_UNCHANGED"));
        }
    }

    if (Glib::file_test (pp->raw.ff_file, Glib::FILE_TEST_EXISTS)) {
        flatFieldFile->set_filename (pp->raw.ff_file);
    } else {
        flatFieldFile_Reset();
    }

    hbff->set_sensitive( !pp->raw.ff_AutoSelect );

    lastFFAutoSelect = pp->raw.ff_AutoSelect;
    lastFFAutoClipCtrl = pp->raw.ff_AutoClipControl;

    if( pp->raw.ff_AutoSelect  && ffp && !batchMode) {
        // retrieve the auto-selected ff filename
        rtengine::RawImage *img = ffp->getFF();

        if( img ) {
            ffInfo->set_text( Glib::ustring::compose("%1: f/%2", Glib::path_get_basename(img->get_filename()), img->get_aperture()) ); // !!! need to add focallength in mm and format aperture to ##.#
        } else {
            ffInfo->set_text(Glib::ustring(M("TP_PREPROCESS_NO_FOUND")));
        }
    } else {
        ffInfo->set_text("-");
    }

    ffChanged = false;

    flatFieldAutoSelectconn.block (false);
    flatFieldBlurTypeconn.block (false);
    enableListener ();

    // Add filter with the current file extension if the current file is raw
    if (ffp && !batchMode) {

        if (b_filter_asCurrent) {
            //First, remove last filter_asCurrent if it was set for a raw file
            std::vector< Glib::RefPtr<Gtk::FileFilter> > filters = flatFieldFile->list_filters();
            flatFieldFile->remove_filter(*(filters.end() - 1));
            b_filter_asCurrent = false;
        }

        Glib::ustring fname = Glib::path_get_basename(ffp->GetCurrentImageFilePath());
        Glib::ustring filetype;

        if (!fname.empty()) {
            // get image filetype, set filter to the same as current image's filetype
            std::string::size_type idx;
            idx = fname.rfind('.');

            if(idx != std::string::npos) {
                filetype = fname.substr(idx + 1);
                //exclude non-raw
                israw = filetype.uppercase() != "JPG" && filetype.uppercase() != "JPEG" && filetype.uppercase() != "PNG" && filetype.uppercase() != "TIF" && filetype.uppercase() != "TIFF";

                if (israw) {
                    b_filter_asCurrent = true; //prevent re-adding this filter on every pp3 file read
                    Glib::RefPtr<Gtk::FileFilter> filter_asCurrent = Gtk::FileFilter::create();
                    filter_asCurrent->add_pattern("*." + filetype);
                    filter_asCurrent->set_name(M("FILECHOOSER_FILTER_SAME") + " (" + filetype + ")");
                    flatFieldFile->add_filter (filter_asCurrent);
                    flatFieldFile->set_filter (filter_asCurrent);
                }
            }
        }
    }

}

void FlatField::write( rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    pp->raw.ff_file = flatFieldFile->get_filename();
    pp->raw.ff_AutoSelect = flatFieldAutoSelect->get_active();
    pp->raw.ff_FromMetaData = flatFieldFromMetaData->get_active();
    pp->raw.ff_BlurRadius = flatFieldBlurRadius->getIntValue();
    pp->raw.ff_clipControl = flatFieldClipControl->getIntValue();
    pp->raw.ff_AutoClipControl = flatFieldClipControl->getAutoValue();

    int currentRow = flatFieldBlurType->get_active_row_number();

    if( currentRow >= 0 && flatFieldBlurType->get_active_text() != M("GENERAL_UNCHANGED")) {
        pp->raw.ff_BlurType = procparams::RAWParams::getFlatFieldBlurTypeStrings()[currentRow];
    }

    if (pedited) {
        pedited->raw.ff_file = ffChanged;
        pedited->raw.ff_AutoSelect = !flatFieldAutoSelect->get_inconsistent();
        pedited->raw.ff_FromMetaData = !flatFieldFromMetaData->get_inconsistent();
        pedited->raw.ff_BlurRadius = flatFieldBlurRadius->getEditedState ();
        pedited->raw.ff_clipControl = flatFieldClipControl->getEditedState ();
        pedited->raw.ff_AutoClipControl = !flatFieldClipControl->getAutoInconsistent();
        pedited->raw.ff_BlurType = flatFieldBlurType->get_active_text() != M("GENERAL_UNCHANGED");
    }

}

void FlatField::adjusterChanged(Adjuster* a, double newval)
{
    if (listener) {
        const Glib::ustring value = a->getTextValue();

        if (a == flatFieldBlurRadius) {
            listener->panelChanged (EvFlatFieldBlurRadius,  value);
        } else if (a == flatFieldClipControl) {
            listener->panelChanged (EvFlatFieldClipControl,  value);
        }
    }
}

void FlatField::adjusterAutoToggled (Adjuster* a, bool newval)
{
    if (multiImage) {
        if (flatFieldClipControl->getAutoInconsistent()) {
            flatFieldClipControl->setAutoInconsistent(false);
            flatFieldClipControl->setAutoValue(false);
        } else if (lastFFAutoClipCtrl) {
            flatFieldClipControl->setAutoInconsistent(true);
        }

        lastFFAutoClipCtrl = flatFieldClipControl->getAutoValue();

    }

    if (listener) {
        if(a == flatFieldClipControl) {
            if (flatFieldClipControl->getAutoInconsistent()) {
                listener->panelChanged (EvFlatFieldAutoClipControl, M("GENERAL_UNCHANGED"));
            } else if (flatFieldClipControl->getAutoValue()) {
                listener->panelChanged (EvFlatFieldAutoClipControl, M("GENERAL_ENABLED"));
            } else {
                listener->panelChanged (EvFlatFieldAutoClipControl, M("GENERAL_DISABLED"));
            }
        }
    }
}

void FlatField::setBatchMode(bool batchMode)
{
    ToolPanel::setBatchMode (batchMode);
    flatFieldBlurRadius->showEditedCB ();
    flatFieldClipControl->showEditedCB ();
}

void FlatField::setAdjusterBehavior (bool clipctrladd)
{
    flatFieldClipControl->setAddMode(clipctrladd);
}

void FlatField::trimValues (rtengine::procparams::ProcParams* pp)
{
    flatFieldClipControl->trimValue(pp->raw.ff_clipControl);
}

void FlatField::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    flatFieldBlurRadius->setDefault( defParams->raw.ff_BlurRadius);
    flatFieldClipControl->setDefault( defParams->raw.ff_clipControl);

    if (pedited) {
        flatFieldBlurRadius->setDefaultEditedState( pedited->raw.ff_BlurRadius ? Edited : UnEdited);
        flatFieldClipControl->setDefaultEditedState( pedited->raw.ff_clipControl ? Edited : UnEdited);
    } else {
        flatFieldBlurRadius->setDefaultEditedState( Irrelevant );
        flatFieldClipControl->setDefaultEditedState( Irrelevant );
    }
}

void FlatField::flatFieldFileChanged()
{
    ffChanged = true;

    if (listener) {
        listener->panelChanged (EvFlatFieldFile, Glib::path_get_basename(flatFieldFile->get_filename()));
    }
}

void FlatField::flatFieldFile_Reset()
{
    ffChanged = true;
    const auto& options = App::get().options();

// caution: I had to make this hack, because set_current_folder() doesn't work correctly!
//          Because szeva doesn't exist since he was committed to happy hunting ground in Issue 316
//          we can use him now for this hack
    flatFieldFile->set_filename (options.lastFlatfieldDir + "/szeva");
// end of the hack

    if (!options.lastFlatfieldDir.empty()) {
        flatFieldFile->set_current_folder(options.lastFlatfieldDir);
    }

    ffInfo->set_text("-");

    if (listener) {
        listener->panelChanged (EvFlatFieldFile, M("GENERAL_NONE") );
    }
}

void FlatField::flatFieldBlurTypeChanged ()
{
    const int curSelection = flatFieldBlurType->get_active_row_number();
    const RAWParams::FlatFieldBlurType blur_type = RAWParams::FlatFieldBlurType(curSelection);

    if (multiImage || blur_type == procparams::RAWParams::FlatFieldBlurType::AREA) {
        flatFieldClipControl->show();
    } else {
        flatFieldClipControl->hide();
    }

    if (listener && curSelection >= 0) {
        listener->panelChanged (EvFlatFieldBlurType, flatFieldBlurType->get_active_text());
    }
}

void FlatField::checkBoxToggled (CheckBox* c, CheckValue newval)
{
    if (listener && c == flatFieldFromMetaData) {
        listener->panelChanged (EvFlatFieldFromMetaData, flatFieldFromMetaData->getLastActive() ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }
}

void FlatField::flatFieldAutoSelectChanged()
{
    if (batchMode) {
        if (flatFieldAutoSelect->get_inconsistent()) {
            flatFieldAutoSelect->set_inconsistent (false);
            flatFieldAutoSelectconn.block (true);
            flatFieldAutoSelect->set_active (false);
            flatFieldAutoSelectconn.block (false);
        } else if (lastFFAutoSelect) {
            flatFieldAutoSelect->set_inconsistent (true);
        }

        lastFFAutoSelect = flatFieldAutoSelect->get_active ();
    }

    hbff->set_sensitive( !flatFieldAutoSelect->get_active() );

    if( flatFieldAutoSelect->get_active()  && ffp && !batchMode) {
        // retrieve the auto-selected ff filename
        rtengine::RawImage *img = ffp->getFF();

        if( img ) {
            ffInfo->set_text( Glib::ustring::compose("%1: f/%2", Glib::path_get_basename(img->get_filename()), img->get_aperture()) ); // !!! need to add focallength in mm and format aperture to ##.#
        } else {
            ffInfo->set_text(Glib::ustring(M("TP_PREPROCESS_NO_FOUND")));
        }
    } else {
        ffInfo->set_text("-");
    }

    if (listener) {
        listener->panelChanged (EvFlatFieldAutoSelect, flatFieldAutoSelect->get_active() ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }

}

void FlatField::setShortcutPath(const Glib::ustring& path)
{
    if (path.empty ()) {
        return;
    }

    try {

        if (!lastShortcutPath.empty ()) {
            flatFieldFile->remove_shortcut_folder (lastShortcutPath);
        }

        flatFieldFile->add_shortcut_folder (path);

        lastShortcutPath = path;

    } catch (Glib::Error&) {}
}

void FlatField::flatFieldAutoClipValueChanged(int n)
{
    idle_register.add(
        [this, n]() -> bool
        {
            disableListener();
            flatFieldClipControl->setValue(n);
            enableListener();
            return false;
        }
    );
}

void FlatField::setGainMap(bool enabled) {
    flatFieldFromMetaData->set_sensitive(enabled);
    if (!enabled) {
        idle_register.add(
            [this]() -> bool
            {
                disableListener();
                flatFieldFromMetaData->setValue(false);
                enableListener();
                return false;
            }
        );
    }
}
