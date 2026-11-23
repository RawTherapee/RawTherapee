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
#include "whitebalance.h"

#include <iomanip>

#include "rtimage.h"
#include "rtsurface.h"
#include "options.h"
#include "eventmapper.h"

#include "rtengine/colortemp.h"

#define MINTEMP 1500   //1200
#define MAXTEMP 60000  //12000
#define CENTERTEMP 4750
#define MINGREEN 0.02
#define MAXGREEN 100.0
#define MINEQUAL 0.5
#define MAXEQUAL 2.

using namespace rtengine;
using namespace rtengine::procparams;
const Glib::ustring WhiteBalance::TOOL_NAME = "whitebalance";

static double wbSlider2Temp(double sval)
{

    // slider range: 0 - 10000
    double temp;

    if (sval <= 5000) {
        // linear below center-temp
        temp = MINTEMP + (sval / 5000.0) * (CENTERTEMP - MINTEMP);
    } else {
        const double slope = (double)(CENTERTEMP - MINTEMP) / (MAXTEMP - CENTERTEMP);
        double x = (sval - 5000) / 5000; // x 0..1
        double y = x * slope + (1.0 - slope) * pow(x, 4.0);
        //double y = pow(x, 4.0);
        temp = CENTERTEMP + y * (MAXTEMP - CENTERTEMP);
    }

    if (temp < MINTEMP) {
        temp = MINTEMP;
    }

    if (temp > MAXTEMP) {
        temp = MAXTEMP;
    }

    return temp;
}

static double wbTemp2Slider(double temp)
{

    double sval;

    if (temp <= CENTERTEMP) {
        sval = ((temp - MINTEMP) / (CENTERTEMP - MINTEMP)) * 5000.0;
    } else {
        const double slope = (double)(CENTERTEMP - MINTEMP) / (MAXTEMP - CENTERTEMP);
        const double y = (temp - CENTERTEMP) / (MAXTEMP - CENTERTEMP);
        double x = pow(y, 0.25); // rough guess of x, will be a little lower
        double k = 0.1;
        bool add = true;

        // the y=f(x) function is a mess to invert, therefore we have this trial-refinement loop instead.
        // from tests, worst case is about 20 iterations, ie no problem
        for (;;) {
            double y1 = x * slope + (1.0 - slope) * pow(x, 4.0);

            if (5000 * fabs(y1 - y) < 0.1) {
                break;
            }

            if (y1 < y) {
                if (!add) {
                    k /= 2;
                }

                x += k;
                add = true;
            } else {
                if (add) {
                    k /= 2;
                }

                x -= k;
                add = false;
            }
        }

        sval = 5000.0 + x * 5000.0;
    }

    if (sval < 0) {
        sval = 0;
    }

    if (sval > 10000) {
        sval = 10000;
    }

    return sval;
}

WhiteBalance::WhiteBalance () : FoldableToolPanel(this, TOOL_NAME, M("TP_WBALANCE_LABEL"), true, true), wbp(nullptr), wblistener(nullptr)
{
    // Assign icon name to wbIcons
    wbIcons[toUnderlying(WBEntry::Type::CAMERA)]      = "wb-camera-small";
    wbIcons[toUnderlying(WBEntry::Type::AUTO)]        = "wb-auto-small";
    wbIcons[toUnderlying(WBEntry::Type::DAYLIGHT)]    = "wb-sun-small";
    wbIcons[toUnderlying(WBEntry::Type::CLOUDY)]      = "wb-cloudy-small";
    wbIcons[toUnderlying(WBEntry::Type::SHADE)]       = "wb-shade-small";
    wbIcons[toUnderlying(WBEntry::Type::WATER)]       = "wb-water-small";
    wbIcons[toUnderlying(WBEntry::Type::TUNGSTEN)]    = "wb-tungsten-small";
    wbIcons[toUnderlying(WBEntry::Type::FLUORESCENT)] = "wb-fluorescent-small";
    wbIcons[toUnderlying(WBEntry::Type::LAMP)]        = "wb-lamp-small";
    wbIcons[toUnderlying(WBEntry::Type::FLASH)]       = "wb-flash-small";
    wbIcons[toUnderlying(WBEntry::Type::LED)]         = "wb-led-small";
    wbIcons[toUnderlying(WBEntry::Type::CUSTOM)]      = "wb-custom-small";

    Gtk::Grid* methodgrid = Gtk::manage(new Gtk::Grid());
    methodgrid->get_style_context()->add_class("grid-spacing");
    setExpandAlignProperties(methodgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    Gtk::Label* lab = Gtk::manage (new Gtk::Label (M("TP_WBALANCE_METHOD") + ":"));
    setExpandAlignProperties(lab, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    // Create the Tree model
    refTreeModel = Gtk::TreeStore::create(methodColumns);
    // Create the Combobox
    method = Gtk::manage (new MyComboBox ());
    setExpandAlignProperties(method, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    // Assign the model to the Combobox
    method->set_model(refTreeModel);
    method->clear(); // Clear default cell layout to add custom one
    Gtk::CellRendererPixbuf* const renderer_icon = Gtk::manage(new Gtk::CellRendererPixbuf());
    renderer_icon->property_stock_size() = Gtk::ICON_SIZE_MENU;
    method->pack_start(*renderer_icon, false);
    method->add_attribute(*renderer_icon, "icon-name", methodColumns.colIcon);
    Gtk::CellRendererText* const renderer_label = Gtk::manage(new Gtk::CellRendererText());
    renderer_label->property_ellipsize() = Pango::ELLIPSIZE_MIDDLE;
    method->pack_start(*renderer_label, true);
    method->add_attribute(*renderer_label, "markup", methodColumns.colLabel);

    WBEntry::Type oldType = WBParams::getWbEntries()[0].type;
    WBEntry::Type currType;
    Gtk::TreeModel::Row row, childrow;

    for (unsigned int i = 0; i < WBParams::getWbEntries().size(); i++) {
        if (oldType != (currType = WBParams::getWbEntries()[i].type)) {
            // New entry type
            if (currType == WBEntry::Type::FLUORESCENT) {
                // Creating the Fluorescent subcategory header
                row = *(refTreeModel->append());
                row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
                row[methodColumns.colLabel] = M("TP_WBALANCE_FLUO_HEADER");
                row[methodColumns.colId] = i + 100;
            }

            if (currType == WBEntry::Type::AUTO) {
                // Creating the auto category
                row = *(refTreeModel->append());
                row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
                row[methodColumns.colLabel] = M("TP_WBALANCE_AUTO_HEADER");
                row[methodColumns.colId] = i + 100;
            }

            if (currType == WBEntry::Type::WATER) {
                // Creating the under water subcategory header
                row = *(refTreeModel->append());
                row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
                row[methodColumns.colLabel] = M("TP_WBALANCE_WATER_HEADER");
                row[methodColumns.colId] = i + 100;
            }

            if (currType == WBEntry::Type::LAMP) {
                // Creating the Lamp subcategory header
                row = *(refTreeModel->append());
                row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
                row[methodColumns.colLabel] = M("TP_WBALANCE_LAMP_HEADER");
                row[methodColumns.colId] = i + 100;
            }

            if (currType == WBEntry::Type::LED) {
                // Creating the LED subcategory header
                row = *(refTreeModel->append());
                row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
                row[methodColumns.colLabel] = M("TP_WBALANCE_LED_HEADER");
                row[methodColumns.colId] = i + 100;
            }

            if (currType == WBEntry::Type::FLASH) {
                // Creating the Flash subcategory header
                row = *(refTreeModel->append());
                row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
                row[methodColumns.colLabel] = M("TP_WBALANCE_FLASH_HEADER");
                row[methodColumns.colId] = i + 100;
            }
        }

        if (currType == WBEntry::Type::FLUORESCENT
                || currType == WBEntry::Type::LAMP
                || currType == WBEntry::Type::WATER
                || currType == WBEntry::Type::FLASH
                || currType == WBEntry::Type::LED
                || currType == WBEntry::Type::AUTO
           ) {
            childrow = *(refTreeModel->append(row.children()));
            childrow[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
            childrow[methodColumns.colLabel] = WBParams::getWbEntries()[i].GUILabel;
            childrow[methodColumns.colId] = i;
        } else {
            row = *(refTreeModel->append());
            row[methodColumns.colIcon] = wbIcons[toUnderlying(currType)];
            row[methodColumns.colLabel] = WBParams::getWbEntries()[i].GUILabel;
            row[methodColumns.colId] = i;
        }

        oldType = currType;

        custom_green = 1.0;
        custom_equal = 1.0;
    }

    auto m = ProcEventMapper::getInstance();
    EvWBObserver10 = m->newEvent(WB, "HISTORY_MSG_WBALANCE_OBSERVER10");
    EvWBitcwbprim = m->newEvent(WB, "HISTORY_MSG_WBITC_PRIM");
    EvWBitcwbalg = m->newEvent(WB, "HISTORY_MSG_WBITC_OBS");
    EvWBitcwgreen = m->newEvent(WB, "HISTORY_MSG_WBITC_GREEN");

    resetButton = Gtk::manage (new Gtk::Button()); // No label, keep it short
    setExpandAlignProperties(resetButton, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    resetButton->set_relief(Gtk::RELIEF_NONE);
    resetButton->get_style_context()->add_class(GTK_STYLE_CLASS_FLAT);
    resetButton->set_image (*Gtk::manage (new RTImage ("undo-small", Gtk::ICON_SIZE_BUTTON)));

    method->set_active (0); // Camera
    methodgrid->attach (*lab, 0, 0, 1, 1);
    methodgrid->attach (*method, 1, 0, 1, 1);
    methodgrid->attach (*resetButton, 2, 0, 1, 1);
    pack_start (*methodgrid, Gtk::PACK_SHRINK, 0 );
    opt = 0;

    Gtk::Grid* spotgrid = Gtk::manage(new Gtk::Grid());
    spotgrid->get_style_context()->add_class("grid-spacing");
    setExpandAlignProperties(spotgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    spotbutton = Gtk::manage (new Gtk::Button (M("TP_WBALANCE_PICKER")));
    setExpandAlignProperties(spotbutton, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    spotbutton->get_style_context()->add_class("independent");
    spotbutton->set_tooltip_text(M("TP_WBALANCE_SPOTWB"));
    spotbutton->set_image (*Gtk::manage (new RTImage ("color-picker-small", Gtk::ICON_SIZE_BUTTON)));

    Gtk::Label* slab = Gtk::manage (new Gtk::Label (M("TP_WBALANCE_SIZE")));
    setExpandAlignProperties(slab, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    Gtk::Grid* wbsizehelper = Gtk::manage(new Gtk::Grid());
    wbsizehelper->set_name("WB-Size-Helper");
    setExpandAlignProperties(wbsizehelper, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    spotsize = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties(spotsize, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    spotsize->append ("2");

    const auto& options = App::get().options();
    if (options.whiteBalanceSpotSize == 2) {
        spotsize->set_active(0);
    }

    spotsize->append ("4");

    if (options.whiteBalanceSpotSize == 4) {
        spotsize->set_active(1);
    }

    spotsize->append ("8");

    if (options.whiteBalanceSpotSize == 8) {
        spotsize->set_active(2);
    }

    spotsize->append ("16");

    if (options.whiteBalanceSpotSize == 16) {
        spotsize->set_active(3);
    }

    spotsize->append ("32");

    if (options.whiteBalanceSpotSize == 32) {
        spotsize->set_active(4);
    }

    wbsizehelper->attach (*spotsize, 0, 0, 1, 1);

    spotgrid->attach (*spotbutton, 0, 0, 1, 1);
    spotgrid->attach (*slab, 1, 0, 1, 1);
    spotgrid->attach (*wbsizehelper, 2, 0, 1, 1);
    pack_start (*spotgrid, Gtk::PACK_SHRINK, 0 );

    Gtk::Separator *separator = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    separator->get_style_context()->add_class("grid-row-separator");
    pack_start (*separator, Gtk::PACK_SHRINK, 0);

    Gtk::Image* itempL =  Gtk::manage (new RTImage ("circle-blue-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* itempR =  Gtk::manage (new RTImage ("circle-yellow-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* igreenL = Gtk::manage (new RTImage ("circle-magenta-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* igreenR = Gtk::manage (new RTImage ("circle-green-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* iblueredL = Gtk::manage (new RTImage ("circle-blue-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* iblueredR = Gtk::manage (new RTImage ("circle-red-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* itempbiasL =  Gtk::manage (new RTImage ("circle-blue-small", Gtk::ICON_SIZE_BUTTON));
    Gtk::Image* itempbiasR =  Gtk::manage (new RTImage ("circle-yellow-small", Gtk::ICON_SIZE_BUTTON));

    StudLabel = Gtk::manage(new Gtk::Label("---", Gtk::ALIGN_CENTER));
    StudLabel->set_tooltip_text(M("TP_WBALANCE_STUDLABEL_TOOLTIP"));
    PatchLabel = Gtk::manage(new Gtk::Label("---", Gtk::ALIGN_CENTER));
    PatchLabel->set_tooltip_text(M("TP_WBALANCE_PATCHLABEL_TOOLTIP"));
    PatchlevelLabel = Gtk::manage(new Gtk::Label("---", Gtk::ALIGN_CENTER));
    PatchlevelLabel->set_tooltip_text(M("TP_WBALANCE_PATCHLEVELLABEL_TOOLTIP"));

    mulLabel = Gtk::manage(new Gtk::Label("---", Gtk::ALIGN_CENTER));
    mulLabel->set_tooltip_text(M("TP_WBALANCE_MULLABEL_TOOLTIP"));
    mulLabel->show();

    temp = Gtk::manage (new Adjuster (M("TP_WBALANCE_TEMPERATURE"), MINTEMP, MAXTEMP, 5, CENTERTEMP, itempL, itempR, &wbSlider2Temp, &wbTemp2Slider));
    green = Gtk::manage (new Adjuster (M("TP_WBALANCE_GREEN"), MINGREEN, MAXGREEN, 0.001, 1.0, igreenL, igreenR));
    equal = Gtk::manage (new Adjuster (M("TP_WBALANCE_EQBLUERED"), MINEQUAL, MAXEQUAL, 0.001, 1.0, iblueredL, iblueredR));
    tempBias = Gtk::manage (new Adjuster(M("TP_WBALANCE_TEMPBIAS"), -1.1, 1.1, 0.005, 0.0, itempbiasL, itempbiasR));
    observer10 = Gtk::manage(new CheckBox(M("TP_WBALANCE_OBSERVER10"), multiImage));

    cache_customTemp (0);
    cache_customGreen (0);
    cache_customEqual (0);
    equal->set_tooltip_markup (M("TP_WBALANCE_EQBLUERED_TOOLTIP"));
    tempBias->set_tooltip_markup (M("TP_WBALANCE_TEMPBIAS_TOOLTIP"));
    observer10->set_tooltip_text(M("TP_WBALANCE_OBSERVER10_TOOLTIP"));
    temp->show ();
    green->show ();
    equal->show ();
    tempBias->show ();
    observer10->show();

    itcwbFrame = Gtk::manage(new Gtk::Frame(M("TP_WBALANCE_ITCWB_FRA")));

    itcwbFrame->set_label_align(0.025, 0.5);
    itcwbFrame->set_tooltip_markup (M("PREFERENCES_WBACORR_TOOLTIP"));

    ToolParamBlock* const itcwbBox = Gtk::manage(new ToolParamBlock());


    itcwb_green = Gtk::manage (new Adjuster (M("TP_WBALANCE_ITCWGREEN"), -0.35, 0.35, 0.005, 0.));
    itcwb_green ->set_tooltip_markup (M("TP_WBALANCE_ITCWGREEN_TOOLTIP"));

    itcwb_alg = Gtk::manage (new Gtk::CheckButton (M("TP_WBALANCE_ITCWB_ALG")));
    itcwb_alg ->set_tooltip_markup (M("TP_WBALANCE_ITCWALG_TOOLTIP"));
    itcwb_alg ->set_active (false);



    itcwb_prim = Gtk::manage (new MyComboBoxText ());
    itcwb_prim->append(M("TP_WBALANCE_ITCWB_PRIM_SRGB"));
    itcwb_prim->append(M("TP_WBALANCE_ITCWB_PRIM_BETA"));
    itcwb_prim->append(M("TP_WBALANCE_ITCWB_PRIM_XYZCAM"));
    itcwb_prim->append(M("TP_WBALANCE_ITCWB_PRIM_JDCMAX"));
    itcwb_prim->set_active(1);
    itcwb_primconn = itcwb_prim->signal_changed().connect(sigc::mem_fun(*this, &WhiteBalance::itcwb_prim_changed));
    itcwb_prim ->set_tooltip_markup (M("TP_WBALANCE_ITCWPRIM_TOOLTIP"));

    compatVersionAdjuster.reset(new Adjuster("", 0., procparams::WBParams::CURRENT_COMPAT_VERSION, 1., procparams::WBParams::CURRENT_COMPAT_VERSION));

    /*  Gtk::Box* boxgreen = Gtk::manage (new Gtk::Box ());
    boxgreen->show ();

    boxgreen->pack_start(*igreenL);
    boxgreen->pack_start(*green);
    boxgreen->pack_start(*igreenR);*/
    pack_start(*mulLabel);
    pack_start(*StudLabel);
    pack_start(*PatchLabel);
    pack_start(*PatchlevelLabel);
    green->setLogScale(MAXGREEN / MINGREEN, MINGREEN);
    pack_start (*temp);
    //pack_start (*boxgreen);
    pack_start (*green);
    pack_start (*equal);
    pack_start (*tempBias);
    pack_start(*observer10);


    itcwbBox->pack_start (*itcwb_green);
    itcwbBox->pack_start (*itcwb_alg);
    itcwbBox->pack_start (*itcwb_prim);
    itcwbFrame->add(*itcwbBox);
    pack_start(*itcwbFrame);

    if(options.rtSettings.itcwb_enable) {
        itcwb_green->show();
        itcwb_alg->show();
        itcwb_prim->show();
        itcwbFrame->show();
    } else {
        itcwb_green->show();
        itcwb_alg->hide();
        itcwb_prim->hide();
    }
    temp->setAdjusterListener (this);
    green->setAdjusterListener (this);
    equal->setAdjusterListener (this);
    tempBias->setAdjusterListener (this);
    observer10->setCheckBoxListener(this);
    itcwb_green->setAdjusterListener (this);

    spotbutton->signal_pressed().connect( sigc::mem_fun(*this, &WhiteBalance::spotPressed) );
    methconn = method->signal_changed().connect( sigc::mem_fun(*this, &WhiteBalance::optChanged) );
    itcwb_algconn = itcwb_alg->signal_toggled().connect( sigc::mem_fun(*this, &WhiteBalance::itcwb_alg_toggled) );

    resetButton->signal_pressed().connect( sigc::mem_fun(*this, &WhiteBalance::resetWB) );
    spotsize->signal_changed().connect( sigc::mem_fun(*this, &WhiteBalance::spotSizeChanged) );
}

WhiteBalance::~WhiteBalance()
{
    idle_register.destroy();
}

void WhiteBalance::enabledChanged()
{
    invalidateWBFeedback();
    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged(EvWBEnabled, M("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged(EvWBEnabled, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvWBEnabled, M("GENERAL_DISABLED"));
        }
    }
}
void WhiteBalance::itcwb_prim_changed ()
{
    invalidateWBFeedback();
    if (listener && getEnabled()) {
        listener->panelChanged(EvWBitcwbprim, M("GENERAL_ENABLED"));
    }
}


void WhiteBalance::itcwb_alg_toggled ()
{
    invalidateWBFeedback();
    if (batchMode) {
        if (itcwb_alg->get_inconsistent()) {
            itcwb_alg->set_inconsistent (false);
            itcwb_algconn.block (true);
            itcwb_alg->set_active (false);
            itcwb_algconn.block (false);
        } else if (lastitcwb_alg) {
            itcwb_alg->set_inconsistent (true);
        }

        lastitcwb_alg = itcwb_alg->get_active ();
    }
    if (listener && getEnabled()) {
        if (itcwb_alg->get_active ()) {
            listener->panelChanged (EvWBitcwbalg, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged (EvWBitcwbalg, M("GENERAL_DISABLED"));
        }
    }
}

void WhiteBalance::adjusterChangedImmediately(Adjuster* a, double newval)
{
    invalidateWBFeedback();
    int tVal = (int)temp->getValue();
    double gVal = green->getValue();
    double eVal = equal->getValue();
    Gtk::TreeModel::Row row = getActiveMethod();

    if (row == refTreeModel->children().end()) {
        return;
    }

    const std::pair<bool, const WBEntry&> ppMethod = findWBEntry (row[methodColumns.colLabel], WBLT_GUI);
    const std::pair<bool, const WBEntry&> wbCustom = findWBEntry ("Custom", WBLT_PP);

    if (
        !ppMethod.first
        || (
            ppMethod.second.ppLabel != wbCustom.second.ppLabel
            && !(
                (
                    a == equal
                    || a == tempBias
                    || a == itcwb_green
                )
                && ppMethod.second.type == WBEntry::Type::AUTO
            )
        )
    ) {
        methconn.block(true);
        opt = setActiveMethod(wbCustom.second.GUILabel);
        tempBias->set_sensitive(false);

        cache_customWB (tVal, gVal);
        if (a != equal) {
            cache_customEqual(eVal);
        }
        methconn.block(false);
    }

    //cache custom WB setting to allow its recall
    if (a == temp) {
        cache_customTemp (tVal);
    } else if (a == green) {
        cache_customGreen (gVal);
    } else if (a == equal) {
        cache_customEqual (eVal);
    }
}

void WhiteBalance::adjusterChanged(Adjuster* a, double newval)
{
    // Only processing/history notification is delayed. Method and custom-value
    // state already reflect the edit, even if another tool submits first.
    if (listener && getEnabled()) {
        if (a == temp) {
            listener->panelChanged (EvWBTemp, Glib::ustring::format ((int)a->getValue()));
            itcwbFrame->set_sensitive(false);
        } else if (a == green) {
            listener->panelChanged (EvWBGreen, Glib::ustring::format (std::setw(4), std::fixed, std::setprecision(3), a->getValue()));
            itcwbFrame->set_sensitive(false);
        } else if (a == equal) {
            listener->panelChanged (EvWBequal, Glib::ustring::format (std::setw(4), std::fixed, std::setprecision(3), a->getValue()));
        } else if (a == tempBias) {
            listener->panelChanged (EvWBtempBias, Glib::ustring::format (std::setw(4), std::fixed, std::setprecision(2), a->getValue()));
        } else if (a == itcwb_green) {
            listener->panelChanged (EvWBitcwgreen, Glib::ustring::format (std::setw(4), std::fixed, std::setprecision(2), a->getValue()));
        }
    }
}

void WhiteBalance::checkBoxToggled(CheckBox* c, CheckValue newval)
{
    invalidateWBFeedback();
    if (!(getEnabled() && listener)) {
        return;
    }

    if (c == observer10) {
        // If camera WB, update the temperature and tint according to observer.
        const Gtk::TreeModel::Row row = getActiveMethod();
        unsigned int methodId = findWBEntryId(row[methodColumns.colLabel], WBLT_GUI);
        const WBEntry &currMethod = WBParams::getWbEntries()[methodId];
        if (row[methodColumns.colLabel] != M("GENERAL_UNCHANGED") && currMethod.type == WBEntry::Type::CAMERA && wbp) {
            double ctemp, cgreen;
            wbp->getCamWB(ctemp, cgreen,
                observer10->getValue() == CheckValue::off
                    ? rtengine::StandardObserver::TWO_DEGREES
                    : rtengine::StandardObserver::TEN_DEGREES);
            temp->setValue(temp->getAddMode() ? 0.0 : static_cast<int>(ctemp));
            green->setValue(green->getAddMode() ? 0.0 : cgreen);
        }

        listener->panelChanged(
            EvWBObserver10,
            c->getValue() == CheckValue::on ? M("GENERAL_ENABLED")
            : c->getValue() == CheckValue::off
                ? M("GENERAL_DISABLED")
                : M("GENERAL_UNCHANGED"));
    }
}


void WhiteBalance::optChanged ()
{
    Gtk::TreeModel::Row row = getActiveMethod();

    if (row == refTreeModel->children().end()) {
        return;
    }

    if (row[methodColumns.colId] >= 100) {
        // "Header" solutions are trapped ; the combo is then set to the previous value
        bool prevState = methconn.block(true);
        method->set_active(opt);
        methconn.block(prevState);
        return;
    }
    StudLabel->hide();
    mulLabel->show();
    PatchLabel->hide();
    PatchlevelLabel->hide();

    if (opt != row[methodColumns.colId]) {
        invalidateWBFeedback();

        opt = row[methodColumns.colId];

        if (row[methodColumns.colLabel] == M("GENERAL_UNCHANGED")) {
            temp->setEditedState (UnEdited);
            green->setEditedState (UnEdited);
            equal->setEditedState (UnEdited);
            tempBias->setEditedState (UnEdited);
            observer10->setEdited(false);
            compatVersionAdjuster->setEditedState(UnEdited);
        } else {
            unsigned int methodId = findWBEntryId (row[methodColumns.colLabel], WBLT_GUI);
            const WBEntry& currMethod = WBParams::getWbEntries()[methodId];

            tempBias->set_sensitive(currMethod.type == WBEntry::Type::AUTO);
            bool autit = (currMethod.ppLabel == "autitcgreen");
            if (autit) {
                StudLabel->show();
                PatchLabel->show();
                PatchlevelLabel->show();
                equal->hide();
                itcwbFrame->set_sensitive(true);
            } else {
                StudLabel->hide();
                PatchLabel->hide();
                PatchlevelLabel->hide();
                equal->show();
                itcwbFrame->set_sensitive(false);
            }

            switch (currMethod.type) {
            case WBEntry::Type::CAMERA:
                if (wbp) {
                    double ctemp, cgreen;
                    wbp->getCamWB(ctemp, cgreen,
                        observer10->getValue() == CheckValue::off
                            ? rtengine::StandardObserver::TWO_DEGREES
                            : rtengine::StandardObserver::TEN_DEGREES);
                    temp->setValue (temp->getAddMode() ? 0.0 : (int)ctemp);
                    green->setValue (green->getAddMode() ? 0.0 : cgreen);
                    equal->setValue (equal->getAddMode() ? 0.0 : 1.0);

                    if (batchMode) {
                        temp->setEditedState (UnEdited);
                        green->setEditedState (UnEdited);
                        equal->setEditedState (UnEdited);
                        observer10->setEdited(false);
                    }
                }

                break;

            case WBEntry::Type::AUTO:
                if (wbp) {
                    if (batchMode) {
                        temp->setEditedState (UnEdited);
                        green->setEditedState (UnEdited);
                        // equal and observer remain as is
                    }

                    // Recomputing AutoWB will happen in improccoordinator.cc
                }

                break;

            case WBEntry::Type::CUSTOM:
                if (custom_temp > 0) {
                    temp->setValue (temp->getAddMode() ? 0.0 : custom_temp);
                    green->setValue (green->getAddMode() ? 0.0 : custom_green);
                    equal->setValue (equal->getAddMode() ? 0.0 : custom_equal);
                } else {
                    cache_customTemp (temp->getValue());
                    cache_customGreen (green->getValue());
                    cache_customEqual (equal->getValue());
                }

                if (batchMode) {
                    temp->setEditedState (Edited);
                    green->setEditedState (Edited);
                    equal->setEditedState (Edited);
                    observer10->setEdited(true);
                }

                break;

            /* All other solution are the default cases
            case WBEntry::Type::DAYLIGHT:
            case WBEntry::Type::CLOUDY:
            case WBEntry::Type::SHADE:
            case WBEntry::Type::TUNGSTEN:
            case WBEntry::Type::FLUORESCENT:
            case WBEntry::Type::LAMP:
            case WBEntry::Type::FLASH:
            case WBEntry::Type::LED:*/
            default:
                // recall custom WB settings if it exists, set to 1.0 otherwise
                temp->setValue  ( temp->getAddMode() ? 0.0 : (double)(currMethod.temperature));
                green->setValue (green->getAddMode() ? 0.0 : (double)(currMethod.green));
                equal->setValue (equal->getAddMode() ? 0.0 : (double)(currMethod.equal));

                if (batchMode) {
                    temp->setEditedState (Edited);
                    green->setEditedState (Edited);
                    equal->setEditedState (Edited);
                    observer10->setEdited(true);
                }

                break;
            }

            if (compatVersionAdjuster->getIntValue() == 1 &&
                (!batchMode || currMethod.type != WBEntry::Type::AUTO)) {
                // Safe to upgrade version because method changed. In batch
                // mode, this method may be called even if there is no change,
                // so it's only safe to upgrade if the new method is not auto.
                compatVersionAdjuster->setValue(procparams::WBParams::CURRENT_COMPAT_VERSION);
                if (batchMode) {
                    compatVersionAdjuster->setEditedState(Edited);
                }
            }
        }

        if (listener && getEnabled()) {
            listener->panelChanged (EvWBMethod, row[methodColumns.colLabel]);
        }
    }
}

void WhiteBalance::spotPressed ()
{
    StudLabel->hide();
    mulLabel->show();
    PatchLabel->hide();
    PatchlevelLabel->hide();

    if (wblistener) {
        wblistener->spotWBRequested (getSize());
    }
}

void WhiteBalance::spotSizeChanged ()
{
    App::get().mut_options().whiteBalanceSpotSize = getSize();

    if (wblistener) {
        wblistener->spotWBRequested (getSize());
    }
}

void WhiteBalance::read (const ProcParams* pp, const ParamsEdited* pedited)
{
    invalidateWBFeedback();
    disableListener ();

    methconn.block (true);
    equal->setValue (pp->wb.equal);
    observer10->setValue(rtengine::StandardObserver::TEN_DEGREES == pp->wb.observer);
    tempBias->setValue (pp->wb.tempBias);
    tempBias->set_sensitive(true);

    itcwb_algconn.block (true);
    itcwb_alg->set_active (pp->wb.itcwb_alg);
    itcwb_algconn.block (false);
    lastitcwb_alg = pp->wb.itcwb_alg;
    itcwb_green->setValue (pp->wb.itcwb_green);

    compatVersionAdjuster->setValue(pp->wb.compat_version);

    itcwb_primconn.block (true);

    if (pp->wb.itcwb_prim == "srgb") {
        itcwb_prim->set_active(0);
    } else if (pp->wb.itcwb_prim == "beta") {
        itcwb_prim->set_active(1);
     } else if (pp->wb.itcwb_prim == "XYZcam") {
        itcwb_prim->set_active(2);
    } else if (pp->wb.itcwb_prim == "jdcmax") {
        itcwb_prim->set_active(3);
    }
    itcwb_primconn.block (false);



    if(App::get().options().rtSettings.itcwb_enable) {
        itcwb_green->show();
        itcwb_alg->show();
        itcwb_prim->show();
        itcwbFrame->show();

    } else {
        itcwb_green->hide();
        itcwb_alg->hide();
        itcwb_prim->hide();
        itcwbFrame->hide();
    }

    if (pedited) {
        // By default, temperature and green are said "UnEdited", but it may change later
        temp->setEditedState (UnEdited);
        green->setEditedState (UnEdited);
        equal->setEditedState (pedited->wb.equal ? Edited : UnEdited);
        tempBias->setEditedState (pedited->wb.tempBias ? Edited : UnEdited);
        observer10->setEdited(pedited->wb.observer);
        itcwb_alg->set_inconsistent (!pedited->wb.itcwb_alg);
        itcwb_green->setEditedState (pedited->wb.itcwb_green ? Edited : UnEdited);
        compatVersionAdjuster->setEditedState(pedited->wb.compat_version ? Edited : UnEdited);
    }

    if (pedited && !pedited->wb.method) {
        opt = setActiveMethod(M("GENERAL_UNCHANGED"));
    } else {
        const WBEntry& wbValues =
            [this, pp]() -> const WBEntry&
            {
                const std::pair<bool, const WBEntry&> res = findWBEntry(pp->wb.method, WBLT_PP);
                return
                    !res.first
                        ? findWBEntry("Camera", WBLT_PP).second
                        : res.second;
            }();

        opt = setActiveMethod(wbValues.GUILabel);

        // temperature is reset to the associated temperature, or 0.0 if addMode is set.
        switch (wbValues.type) {
        case WBEntry::Type::CUSTOM:
            temp->setValue (temp->getAddMode() ? 0.0 : pp->wb.temperature);
            green->setValue (green->getAddMode() ? 0.0 : pp->wb.green);
            equal->setValue (equal->getAddMode() ? 0.0 : pp->wb.equal);
            tempBias->setValue (tempBias->getAddMode() ? 0.0 : pp->wb.tempBias);
            cache_customTemp (pp->wb.temperature);
            cache_customGreen (pp->wb.green);
            cache_customEqual (pp->wb.equal);

            if (pedited) {
                // The user may have changed the temperature and green value
                temp->setEditedState (pedited->wb.temperature ? Edited : UnEdited);
                green->setEditedState (pedited->wb.green ? Edited : UnEdited);
            }

            break;

        case WBEntry::Type::CAMERA:
            if (wbp) {
                double ctemp = -1.0;
                double cgreen = -1.0;
                wbp->getCamWB (ctemp, cgreen, pp->wb.observer);

                if (ctemp != -1.0) {
                    // Set the camera's temperature value, or 0.0 if in ADD mode
                    temp->setValue (temp->getAddMode() ? 0.0 : ctemp);
                    // Set the camera's green value, or 0.0 if in ADD mode
                    green->setValue (green->getAddMode() ? 0.0 : cgreen);
                    equal->setValue (equal->getAddMode() ? 0.0 : 1.);
                } else {
                    temp->setValue (temp->getAddMode() ? 0.0 : pp->wb.temperature);
                    green->setValue (green->getAddMode() ? 0.0 : pp->wb.green);
                    equal->setValue (equal->getAddMode() ? 0.0 : pp->wb.equal);
                }
                tempBias->setValue (equal->getAddMode() ? 0.0 : pp->wb.tempBias);
            }

            break;

        case WBEntry::Type::AUTO:
            // the equalizer's value is restored for the AutoWB
            equal->setValue (equal->getAddMode() ? 0.0 : pp->wb.equal);
            tempBias->setValue (tempBias->getAddMode() ? 0.0 : pp->wb.tempBias);

            // set default values first if in ADD mode, otherwise keep the current ones
            if (temp->getAddMode() ) {
                temp->setValue (0.0);
            }

            if (green->getAddMode()) {
                green->setValue (0.0);
            }

            // Recomputing AutoWB will happen in improccoordinator.cc

            break;

        /*
        All those types are the "default" case:
        case WBEntry::Type::DAYLIGHT:
        case WBEntry::Type::CLOUDY:
        case WBEntry::Type::SHADE:
        case WBEntry::Type::TUNGSTEN:
        case WBEntry::Type::FLUORESCENT:
        case WBEntry::Type::LAMP:
        case WBEntry::Type::FLASH:
        case WBEntry::Type::LED:
        */
        default:
            // Set the associated temperature, or 0.0 if in ADD mode
            temp->setValue(temp->getAddMode() ? 0.0 : (double)wbValues.temperature);
            // Set the stored temperature, or 0.0 if in ADD mode
            green->setValue(green->getAddMode() ? 0.0 : pp->wb.green);
            equal->setValue(equal->getAddMode() ? 0.0 : pp->wb.equal);
            tempBias->setValue(equal->getAddMode() ? 0.0 : pp->wb.tempBias);

            // The user may have changed the green value even for predefined WB values
            if (pedited) {
                green->setEditedState (pedited->wb.green ? Edited : UnEdited);
                equal->setEditedState (pedited->wb.equal ? Edited : UnEdited);
                tempBias->setEditedState (pedited->wb.tempBias ? Edited : UnEdited);
            }

            //cache_customGreen (pp->wb.green);
            break;
        }

        tempBias->set_sensitive(wbValues.type == WBEntry::Type::AUTO);
        bool autit = (wbValues.ppLabel == "autitcgreen");
        if (autit) {
            StudLabel->show();
            PatchLabel->show();
            PatchlevelLabel->show();
            equal->hide();
            if(pp->wb.itcwb_sampling) {
                tempBias->set_sensitive(false);
            }
            itcwbFrame->set_sensitive(!pp->wb.itcwb_sampling);
            itcwb_prim_changed ();
        } else {
            StudLabel->hide();
            PatchLabel->hide();
            PatchlevelLabel->hide();
            mulLabel->show();
            equal->show();
            itcwbFrame->set_sensitive(false);
        }

    }

    setEnabled(pp->wb.enabled);
    if (pedited) {
        set_inconsistent(multiImage && !pedited->wb.enabled);
    }


    methconn.block (false);
    enableListener ();
}

void WhiteBalance::write (ProcParams* pp, ParamsEdited* pedited)
{

    Gtk::TreeModel::Row row = getActiveMethod();

    if (pedited) {
        pedited->wb.temperature = temp->getEditedState ();
        pedited->wb.green = green->getEditedState ();
        pedited->wb.equal = equal->getEditedState ();
        pedited->wb.tempBias = tempBias->getEditedState ();
        pedited->wb.observer = observer10->getEdited();
        pedited->wb.itcwb_alg = !itcwb_alg->get_inconsistent();
        pedited->wb.method = row[methodColumns.colLabel] != M("GENERAL_UNCHANGED");
        pedited->wb.enabled = !get_inconsistent();
        pedited->wb.itcwb_prim  = itcwb_prim->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->wb.itcwb_green = itcwb_green->getEditedState ();
        pedited->wb.compat_version = compatVersionAdjuster->getEditedState();
    }

    pp->wb.enabled = getEnabled();
    if (itcwb_prim->get_active_row_number() == 0) {
        pp->wb.itcwb_prim = "srgb";
    } else if (itcwb_prim->get_active_row_number() == 1){
        pp->wb.itcwb_prim = "beta";
    } else if (itcwb_prim->get_active_row_number() == 2){
        pp->wb.itcwb_prim = "XYZcam";
    } else if (itcwb_prim->get_active_row_number() == 3){
        pp->wb.itcwb_prim = "jdcmax";
    }

    const std::pair<bool, const WBEntry&> ppMethod = findWBEntry (row[methodColumns.colLabel], WBLT_GUI);

    if (ppMethod.first) {
        pp->wb.method = ppMethod.second.ppLabel;
        if (pp->wb.method != "autitcgreen") {
            // Prepare migration to new ITCWB algorithm.
            pp->wb.itcwb_sampling = false;
        }
    }

    pp->wb.temperature = temp->getIntValue ();
    pp->wb.green = green->getValue ();
    pp->wb.equal = equal->getValue ();
    pp->wb.observer =
        observer10->getValue() == CheckValue::on
            ? rtengine::StandardObserver::TEN_DEGREES
        : observer10->getValue() == CheckValue::off
            ? rtengine::StandardObserver::TWO_DEGREES
            : pp->wb.observer;
    pp->wb.itcwb_alg = itcwb_alg->get_active ();
    pp->wb.tempBias = tempBias->getValue ();
    pp->wb.itcwb_green = itcwb_green->getValue ();
    pp->wb.compat_version = compatVersionAdjuster->getIntValue();
}

void WhiteBalance::setDefaults (const ProcParams* defParams, const ParamsEdited* pedited)
{

    equal->setDefault (defParams->wb.equal);
    tempBias->setDefault (defParams->wb.tempBias);
    itcwb_green->setDefault (defParams->wb.itcwb_green);

    if (wbp && defParams->wb.method == "Camera") {
        double ctemp;
        double cgreen;
        wbp->getCamWB (ctemp, cgreen, defParams->wb.observer);

        // FIXME: Seems to be always -1.0, called too early? Broken!
        if (ctemp != -1.0) {
            temp->setDefault (temp->getAddMode() ? 0 : (int)ctemp);
            green->setDefault (green->getAddMode() ? 0 : cgreen);
        }
    } else {
        temp->setDefault (defParams->wb.temperature);
        green->setDefault (defParams->wb.green);
    }
    // Recomputing AutoWB if it's the current method will happen in improccoordinator.cc

    if (pedited) {
        temp->setDefaultEditedState (pedited->wb.temperature ? Edited : UnEdited);
        green->setDefaultEditedState (pedited->wb.green ? Edited : UnEdited);
        equal->setDefaultEditedState (pedited->wb.equal ? Edited : UnEdited);
        tempBias->setDefaultEditedState (pedited->wb.tempBias ? Edited : UnEdited);
        itcwb_green->setDefaultEditedState (pedited->wb.itcwb_green ? Edited : UnEdited);
    } else {
        temp->setDefaultEditedState (Irrelevant);
        green->setDefaultEditedState (Irrelevant);
        equal->setDefaultEditedState (Irrelevant);
        tempBias->setDefaultEditedState (Irrelevant);
        itcwb_green->setDefaultEditedState (Irrelevant);
    }
}

void WhiteBalance::setBatchMode (bool batchMode)
{

    ToolPanel::setBatchMode (batchMode);
    temp->showEditedCB ();
    green->showEditedCB ();
    equal->showEditedCB ();
    tempBias->showEditedCB ();
    compatVersionAdjuster->showEditedCB();
    Gtk::TreeModel::Row row = *(refTreeModel->append());
    row[methodColumns.colId] = WBParams::getWbEntries().size();
    row[methodColumns.colLabel] = M("GENERAL_UNCHANGED");

}

int WhiteBalance::getSize ()
{

    return atoi(spotsize->get_active_text().c_str());
}

void WhiteBalance::setWB (int vtemp, double vgreen)
{
    invalidateWBFeedback();

    methconn.block(true);
    const std::pair<bool, const WBEntry&> wbValues = findWBEntry("Custom", WBLT_PP);
    setEnabled(true);
    temp->setValue (vtemp);
    green->setValue (vgreen);
    opt = setActiveMethod(wbValues.second.GUILabel);
    cache_customWB (vtemp, vgreen); // sequence in which this call is made is important; must be before "method->set_active (2);"
    cache_customEqual(equal->getValue());
    temp->setEditedState (Edited);
    green->setEditedState (Edited);
    methconn.block(false);

    if (listener) {
        listener->panelChanged (EvWBTemp, Glib::ustring::compose("%1, %2", (int)temp->getValue(), Glib::ustring::format (std::setw(4), std::fixed, std::setprecision(3), green->getValue())));
    }

}

void WhiteBalance::resetWB ()
{
    setActiveMethod(M("TP_WBALANCE_CAMERA"));
}

void WhiteBalance::setAdjusterBehavior (bool tempadd, bool greenadd, bool equaladd, bool tempbiasadd)
{

    temp->setAddMode(tempadd);
    green->setAddMode(greenadd);
    equal->setAddMode(equaladd);
    tempBias->setAddMode(tempbiasadd);
}

void WhiteBalance::trimValues (rtengine::procparams::ProcParams* pp)
{

    temp->trimValue(pp->wb.temperature);
    green->trimValue(pp->wb.green);
    equal->trimValue(pp->wb.equal);
    tempBias->trimValue(pp->wb.tempBias);
}

inline void WhiteBalance::cache_customTemp(int temp)
{
    custom_temp = temp;
}

void WhiteBalance::cache_customGreen(double green)
{
    custom_green = green;
}
void WhiteBalance::cache_customEqual(double equal)
{
    custom_equal = equal;
}

void WhiteBalance::cache_customWB(int temp, double green)
{
    cache_customTemp (temp);
    cache_customGreen (green);
}

unsigned int WhiteBalance::findWBEntryId (const Glib::ustring& label, enum WB_LabelType lblType)
{
    for (unsigned int i = 0; i < WBParams::getWbEntries().size(); i++) {
        if (label == (lblType == WBLT_GUI ? WBParams::getWbEntries()[i].GUILabel : WBParams::getWbEntries()[i].ppLabel)) {
            return i;
        }
    }

    return 0; // default to camera wb
}

std::pair<bool, const WBEntry&> WhiteBalance::findWBEntry(const Glib::ustring& label, enum WB_LabelType lblType)
{
    for (unsigned int i = 0; i < WBParams::getWbEntries().size(); ++i) {
        if (label == (lblType == WBLT_GUI ? WBParams::getWbEntries()[i].GUILabel : WBParams::getWbEntries()[i].ppLabel)) {
            return {true, WBParams::getWbEntries()[i]};
        }
    }

    return {false, WBParams::getWbEntries()[0]};
}

int WhiteBalance::_setActiveMethod(Glib::ustring &label, Gtk::TreeModel::Children &children)
{
    int found = -1;

    for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end() && found == -1; ++iter) {
        Gtk::TreeModel::Row row = *iter;

        if (row[methodColumns.colLabel] == label) {
            method->set_active(iter);
            found = row[methodColumns.colId];
        }

        if (found != -1) {
            return found;
        }

        Gtk::TreeModel::Children childs = row.children();

        if (childs.size()) {
            found = _setActiveMethod(label, childs);

            if (found != -1) {
                return found;
            }
        }
    }

    // Entry not found
    return -1;
}

int WhiteBalance::setActiveMethod(Glib::ustring label)
{
    Gtk::TreeModel::Children children = refTreeModel->children();
    return _setActiveMethod(label, children);
}

inline Gtk::TreeRow WhiteBalance::getActiveMethod ()
{
    return *(method->get_active());
}

void WhiteBalance::WBChanged(std::uint64_t revision, double temperature, double greenVal, double rw, double gw, double bw, float temp0, float delta, int bia, int dread, float studgood, float minchrom, int kmin, float histmin, float histmax, AWBMode aWBMode)
{
    idle_register.add(
        [this, revision, temperature, greenVal, rw, gw, bw, temp0, delta,  bia, dread, studgood, minchrom, kmin, histmin, histmax, aWBMode]() -> bool
        {
            // Check on the GUI thread, after any edits made while this idle
            // callback was queued. Reject the entire obsolete result.
            if (revision != feedbackRevision) {
                return false;
            }
            const Gtk::TreeModel::Row row = getActiveMethod();
            if (row == refTreeModel->children().end()) {
                return false;
            }
            const auto method = findWBEntry(row[methodColumns.colLabel], WBLT_GUI);
            if (!method.first) {
                return false;
            }
            const bool wasEnabled = disableListener();
            if (method.second.type == WBEntry::Type::AUTO || method.second.type == WBEntry::Type::CAMERA) {
                temp->setValue(temperature);
                green->setValue(greenVal);
            }
            double stud;
            stud = studgood;
            if(studgood < 0.0001) {
                stud = 0.0001;
            }
            int bia2 = bia;
            mulLabel->set_text(
            Glib::ustring::compose(M("TP_WBALANCE_MULLABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), rw),
                                   Glib::ustring::format(std::fixed, std::setprecision(2), gw),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), bw))
            );

            if(bia == 3) {
                bia2 = bia - 1;
                StudLabel->set_text(
                    Glib::ustring::compose(M("TP_WBALANCE_STUDLABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), stud),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), bia2),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), temp0))
                );
            } else if(bia == 2) {
                StudLabel->set_text(
                    Glib::ustring::compose(M("TP_WBALANCE_STUDLABEL1"),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), stud),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), bia),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), temp0))
                );
            } else {
                StudLabel->set_text(
                    Glib::ustring::compose(M("TP_WBALANCE_STUDLABEL0"),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), stud),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), bia),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), temp0))
                );
            }
            PatchLabel->set_text(
                Glib::ustring::compose(M("TP_WBALANCE_PATCHLABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), dread),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), minchrom),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), kmin))
            );
            PatchlevelLabel->set_text(
                Glib::ustring::compose(M("TP_WBALANCE_PATCHLEVELLABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(4), delta),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), histmin),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), histmax))
            );
            if (aWBMode == AWBMode::TEMP_CORRELATION_RAW) {
                itcwb_green->set_sensitive(true);
                tempBias->set_sensitive(true);
                itcwb_alg->set_sensitive(true);
                itcwb_prim->set_sensitive(true);
            } else if (aWBMode == AWBMode::RGB_GREY) {
                itcwb_green->set_sensitive(false);
                tempBias->set_sensitive(true);
                itcwb_alg->set_sensitive(false);
                itcwb_prim->set_sensitive(false);
            } else {
                itcwb_green->set_sensitive(false);
                tempBias->set_sensitive(false);
                itcwb_alg->set_sensitive(false);
                itcwb_prim->set_sensitive(false);
           }
            temp->setDefault(temperature);
            green->setDefault(greenVal);
            if (wasEnabled) {
                enableListener();
            }

            return false;
        }
    );
}
