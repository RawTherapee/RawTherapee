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
#include <array>
#include <utility>

#include "saveformatpanel.h"
#include "rtengine/imageio.h"
#include "multilangmgr.h"
#include "guiutils.h"

namespace
{

const std::array<std::pair<const char*, SaveFormat>, 7> sf_templates = {{
     {"JPEG (8-bit)", SaveFormat("jpg", 8, 8, false)},
     {"TIFF (8-bit)", SaveFormat("tif", 8, 8, false)},
     {"TIFF (16-bit)", SaveFormat("tif", 8, 16, false)},
     {"TIFF (16-bit float)", SaveFormat("tif", 8, 16, true)},
     {"TIFF (32-bit float)", SaveFormat("tif", 8, 32, true)},
     {"PNG (8-bit)", SaveFormat("png", 8, 8, false)},
     {"PNG (16-bit)", SaveFormat("png", 16, 8, false)}
}};

}

SaveFormatPanel::SaveFormatPanel () : templates(sf_templates.begin(), sf_templates.end()),
    heifAvailable(rtengine::ImageIO::isHEIFExportAvailable()), listener (nullptr)
{


    // ---------------------  FILE FORMAT SELECTOR


    Gtk::Grid* hb1 = Gtk::manage (new Gtk::Grid ());
    hb1->set_column_spacing(5);
    hb1->set_row_spacing(5);
    setExpandAlignProperties(hb1, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    Gtk::Label* flab = Gtk::manage (new Gtk::Label (M("SAVEDLG_FILEFORMAT") + ":"));
    setExpandAlignProperties(flab, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    format = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties(format, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    format->signal_changed ().connect (sigc::mem_fun (*this, &SaveFormatPanel::formatChanged));

    if (heifAvailable) {
        for (int bits : {8, 10, 12}) {
            SaveFormat sf;
            sf.format = "heic";
            sf.heifBits = bits;
            templates.emplace_back("HEIF (" + std::to_string(bits) + "-bit)", sf);
        }
    }
    for (const auto& sf_template : templates) {
        format->append(sf_template.first);
    }

    hb1->attach (*flab, 0, 0, 1, 1);
    hb1->attach (*format, 1, 0, 1, 1);
    hb1->show_all();

    // ---------------------  JPEG OPTIONS


    jpegOpts = Gtk::manage (new Gtk::Grid ());
    jpegOpts->set_column_spacing(15);
    jpegOpts->set_row_spacing(5);
    setExpandAlignProperties(jpegOpts, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    jpegQual = Gtk::manage (new Adjuster (M("SAVEDLG_JPEGQUAL"), 0, 100, 1, 100) );
    setExpandAlignProperties(jpegQual, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    jpegQual->setAdjusterListener (this);

    jpegSubSampLabel = Gtk::manage (new Gtk::Label (M("SAVEDLG_SUBSAMP") + Glib::ustring(":")) );
    setExpandAlignProperties(jpegSubSampLabel, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    jpegSubSamp = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties(jpegSubSamp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    jpegSubSamp->append (M("SAVEDLG_SUBSAMP_1"));
    jpegSubSamp->append (M("SAVEDLG_SUBSAMP_2"));
    jpegSubSamp->append (M("SAVEDLG_SUBSAMP_3"));
    jpegSubSamp->set_tooltip_text (M("SAVEDLG_SUBSAMP_TOOLTIP"));
    jpegSubSamp->set_active (2);
    jpegSubSamp->signal_changed().connect( sigc::mem_fun(*this, &SaveFormatPanel::formatChanged) );

    jpegOpts->attach(*jpegQual, 0, 0, 1, 2);
    jpegOpts->attach(*jpegSubSampLabel, 1, 0, 1, 1);
    jpegOpts->attach(*jpegSubSamp, 1, 1, 1, 1);
    jpegOpts->show_all ();

    // ---------------------  HEIF OPTIONS
    heifOpts = Gtk::manage(new Gtk::Grid());
    heifOpts->set_column_spacing(15);
    heifOpts->set_row_spacing(5);
    heifQual = Gtk::manage(new Adjuster(M("SAVEDLG_HEIFQUAL"), 0, 100, 1, 92));
    setExpandAlignProperties(heifQual, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    heifQual->setAdjusterListener(this);
    heifQual->set_tooltip_text(M("SAVEDLG_HEIFQUAL_TOOLTIP"));
    auto* heifSubSampLabel = Gtk::manage(new Gtk::Label(M("SAVEDLG_SUBSAMP") + ":"));
    heifSubSamp = Gtk::manage(new MyComboBoxText());
    for (const auto* key : {"SAVEDLG_SUBSAMP_1", "SAVEDLG_SUBSAMP_2", "SAVEDLG_SUBSAMP_3"}) {
        heifSubSamp->append(M(key));
    }
    heifSubSamp->set_active(0);
    heifSubSamp->signal_changed().connect(sigc::mem_fun(*this, &SaveFormatPanel::formatChanged));
    heifOpts->attach(*heifQual, 0, 0, 1, 2);
    heifOpts->attach(*heifSubSampLabel, 1, 0, 1, 1);
    heifOpts->attach(*heifSubSamp, 1, 1, 1, 1);
    heifUnavailable = Gtk::manage(new Gtk::Label(M("SAVEDLG_HEIFUNAVAILABLE")));
    heifUnavailable->set_line_wrap(true);

    // ---------------------  TIFF OPTIONS


    tiffUncompressed = Gtk::manage (new Gtk::CheckButton (M("SAVEDLG_TIFFUNCOMPRESSED")) );
    setExpandAlignProperties(tiffUncompressed, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    tiffUncompressed->signal_toggled().connect( sigc::mem_fun(*this, &SaveFormatPanel::formatChanged));
    tiffUncompressed->show_all();

    bigTiff = Gtk::manage (new Gtk::CheckButton (M("SAVEDLG_BIGTIFF")) );
    setExpandAlignProperties(bigTiff, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    bigTiff->signal_toggled().connect( sigc::mem_fun(*this, &SaveFormatPanel::formatChanged));
    bigTiff->show_all();


    // ---------------------  MAIN BOX


    savesPP = Gtk::manage (new Gtk::CheckButton (M("SAVEDLG_SAVESPP")));
    savesPP->signal_toggled().connect( sigc::mem_fun(*this, &SaveFormatPanel::formatChanged));
    savesPP->show_all();

    set_column_spacing(5);
    set_row_spacing(5);

    attach (*hb1, 0, 0, 1, 1);
    attach (*jpegOpts, 0, 1, 1, 1);
    attach (*tiffUncompressed, 0, 2, 1, 1);
    attach (*bigTiff, 0, 3, 1, 1);
    attach (*savesPP, 0, 4, 1, 2);
    attach (*heifOpts, 0, 6, 1, 1);
    attach (*heifUnavailable, 0, 7, 1, 1);
}

SaveFormatPanel::~SaveFormatPanel () = default;

void SaveFormatPanel::init (const SaveFormat &sf)
{
    FormatChangeListener* const tmp = listener;
    listener = nullptr;

    // Keep an unavailable saved HEIF selection visible and intact. Never map
    // it to a JPEG/TIFF/PNG preset just because its codec is missing.
    if (!heifAvailable && sf.format == "heic") {
        if (templates.size() == sf_templates.size()) {
            templates.emplace_back(M("SAVEDLG_HEIFUNAVAILABLE_FORMAT"), sf);
            format->append(templates.back().first);
        } else {
            templates.back().second = sf;
        }
    }
    std::pair<int, std::size_t> index{};

    for (std::size_t i = 0; i < templates.size(); ++i) {
        // Without relating the other SaveFormat fields to the
        // SaveFormat::format by additional logic the best
        // way is computing a weight for fitting the input
        // to one of the sf_templates.
        // The format field must match exactly, tiffBits,
        // tiffFloat, and pngBits fields all weigh the same.
        // By providing sane sets of parameters in getFormat()
        // we have perfect matches. If the parameters were
        // tampered with, some entry within SaveFormat::format
        // will be selected, which will be consistent again.

        const int weight =
            10 * (sf.format == templates[i].second.format)
            + (sf.tiffBits == templates[i].second.tiffBits)
            + (sf.tiffFloat == templates[i].second.tiffFloat)
            + (sf.pngBits == templates[i].second.pngBits)
            + (sf.heifBits == templates[i].second.heifBits);

        if (weight > index.first) {
            index = {weight, i};
        }
    }

    format->set_active(index.second);

    heifSubSamp->set_active(sf.heifSubSamp - 1);
    heifQual->setValue(sf.heifQuality);
    jpegSubSamp->set_active(sf.jpegSubSamp - 1);
    jpegQual->setValue(sf.jpegQuality);
    savesPP->set_active(sf.saveParams);
    tiffUncompressed->set_active(sf.tiffUncompressed);
    bigTiff->set_active(sf.bigTiff);

    formatChanged();
    listener = tmp;
}

SaveFormat SaveFormatPanel::getFormat ()
{
    SaveFormat sf;

    const unsigned int sel = format->get_active_row_number();

    if (sel < templates.size()) {
        sf = templates[sel].second;
    }

    sf.heifQuality = heifQual->getValue();
    sf.heifSubSamp = heifSubSamp->get_active_row_number() + 1;
    sf.jpegQuality = jpegQual->getValue();
    sf.jpegSubSamp = jpegSubSamp->get_active_row_number() + 1;
    sf.tiffUncompressed = tiffUncompressed->get_active();
    sf.bigTiff = bigTiff->get_active();
    sf.saveParams = savesPP->get_active();

    return sf;
}

void SaveFormatPanel::formatChanged ()
{
    const unsigned int act = format->get_active_row_number();

    if (act >= templates.size()) {
        return;
    }

    const Glib::ustring& fr = templates[act].second.format;

    if (fr == "heic") {
        heifOpts->show_all();
        if (heifAvailable) {
            heifUnavailable->hide();
        } else {
            heifUnavailable->show();
        }
    } else {
        heifOpts->hide();
        heifUnavailable->hide();
    }

    if (fr == "jpg") {
        jpegOpts->show_all();
        tiffUncompressed->hide();
        bigTiff->hide();
    } else if (fr == "png" || fr == "heic") {
        jpegOpts->hide();
        tiffUncompressed->hide();
        bigTiff->hide();
    } else if (fr == "tif") {
        jpegOpts->hide();
        tiffUncompressed->show_all();
        bigTiff->show_all();
    }

    if (listener) {
        listener->formatChanged (fr);
    }
}

void SaveFormatPanel::adjusterChanged(Adjuster* a, double newval)
{
    const unsigned int act = format->get_active_row_number();

    if (act >= templates.size()) {
        return;
    }

    if (listener) {
        listener->formatChanged(templates[act].second.format);
    }
}
