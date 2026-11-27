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
#include <set>

#include "iptcpanel.h"

#include "clipboard.h"
#include "rtimage.h"

#include "rtengine/imagedata.h"
#include "rtengine/metadata.h"
#include "rtengine/procparams.h"

using namespace rtengine;
using namespace rtengine::procparams;

namespace {

const std::string CAPTION("Iptc.Application2.Caption");
const std::string CAPTION_WRITER("Iptc.Application2.Writer");
const std::string CATEGORY("Iptc.Application2.Category");
const std::string CITY("Iptc.Application2.City");
const std::string CODED_CHARACTER_SET("Iptc.Envelope.CharacterSet");
const std::string COPYRIGHT("Iptc.Application2.Copyright");
const std::string COUNTRY("Iptc.Application2.CountryName");
const std::string CREATOR("Iptc.Application2.Byline");
const std::string CREATOR_JOB_TITLE("Iptc.Application2.BylineTitle");
const std::string CREDIT("Iptc.Application2.Credit");
const std::string DATE_CREATED("Iptc.Application2.DateCreated");
const std::string HEADLINE("Iptc.Application2.Headline");
const std::string INSTRUCTIONS("Iptc.Application2.SpecialInstructions");
const std::string KEYWORDS("Iptc.Application2.Keywords");
const std::string PROVINCE("Iptc.Application2.ProvinceState");
const std::string SOURCE("Iptc.Application2.Source");
const std::string SUPPLEMENTAL_CATEGORIES("Iptc.Application2.SuppCategory");
const std::string TITLE("Iptc.Application2.ObjectName");
const std::string TRANS_REFERENCE("Iptc.Application2.TransmissionReference");

/**
 * The ISO 2022 control function that switches to UTF-8.
 *
 * The sequence is ESC % G (hex 1B 25 47, column-line notation 1/11 2/5 4/7).
 * See appendix C of https://www.iptc.org/std/IIM/4.2/specification/IIMV4.2.pdf.
 */
const std::string CONTROL_FUNCTION_UTF8("\x1B%G");

const std::set<std::string> iptc_keys = {
    CAPTION,
    CAPTION_WRITER,
    CATEGORY,
    CITY,
    COPYRIGHT,
    COUNTRY,
    CREATOR,
    CREATOR_JOB_TITLE,
    CREDIT,
    DATE_CREATED,
    HEADLINE,
    INSTRUCTIONS,
    KEYWORDS,
    PROVINCE,
    SOURCE,
    SUPPLEMENTAL_CATEGORIES,
    TITLE,
    TRANS_REFERENCE
};

/**
 * Returns whether the character is invalid in CP1252.
 */
bool isInvalidCp1252Char(char c)
{
    return c == '\201' || c == '\215' || c == '\217' || c == '\220' || c == '\235';
}

/**
 * Converts a string from CP1252 to UTF-8, replacing invalid characters with the
 * fallback character "�".
 */
Glib::ustring convertCp1252ToUtf8(const std::string &in_str)
{
    const std::string fallback_char = "�";
    const std::string from_charset = "CP1252";
    const std::string to_charset = "UTF-8";

    // First find all invalid input characters.
    std::vector<std::string::size_type> invalid_indices;
    for (std::string::size_type i = 0; i < in_str.size(); ++i) {
        const char c = in_str[i];
        if (isInvalidCp1252Char(c)) {
            invalid_indices.push_back(i);
        }
    }

    // If there are invalid characters, create a new string with them replaced.
    std::string sanitized_str;
    if (!invalid_indices.empty()) {
        sanitized_str = in_str;
        for (const auto index : invalid_indices) {
            sanitized_str[index] = '?';
        }
    }

    // Now use Glib to convert.
    const std::string &str_to_convert =
        invalid_indices.empty() ? in_str : sanitized_str;
    Glib::ustring converted_str;
    try {
        // Glib uses iconv/libiconv. See https://www.gnu.org/software/libiconv/
        // for supported encodings.
        converted_str = Glib::convert_with_fallback(
            str_to_convert, to_charset, from_charset, fallback_char);
    } catch (const Glib::ConvertError &e) {
        std::fprintf(stderr, "Unable to convert string %s from %s to %s: %s\n",
            in_str.c_str(), from_charset.c_str(), to_charset.c_str(), e.what().c_str());
        return "";
    }

    // Replace the placeholder characters with the fallback character.
    if (!invalid_indices.empty()) {
        // UTF-8 characters can be multiple bytes, but Glib::ustring indexes by
        // character, not byte.
        for (const auto index : invalid_indices) {
            converted_str.replace(index, 1, fallback_char);
        }
    }

    return converted_str;
}

/**
 * Updates the change list to ensure that all data that needs to be updated is
 * included. It includes user-modified data and string data that needs the
 * encoding to be changed.
 *
 * @param change_list The change list to update.
 * @param embedded_data The embedded IPTC data from the image that is
 * user-editable.
 * @param other_encoded_embedded_data The embedded IPTC data from the image that
 * is not user-editable.
 * @param is_utf8 Whether the embedded data is UTF-8 encoded.
 */
void updateEncoding(
    rtengine::procparams::IPTCPairs &change_list,
    const rtengine::procparams::IPTCPairs &embedded_data,
    const rtengine::procparams::IPTCPairs &other_encoded_embedded_data,
    bool is_utf8)
{
    if (is_utf8) {
        // Embedded data is UTF-8, which is what we want. Remove the items from
        // the change list that haven't actually changed.
        for (const auto &p : embedded_data) {
            const auto it = change_list.find(p.first);
            if (it != change_list.end() && it->second == p.second) {
                change_list.erase(it);
            }
        }
    } else {
        // Embedded data is not UTF-8, so we must add everything to the change
        // list so the data can be written as UTF-8. Also add the UTF-8 coded
        // character set to the IPTC data and non-user-editable data.
        change_list[CODED_CHARACTER_SET] = {CONTROL_FUNCTION_UTF8};
        for (const auto &p : embedded_data) {
            // Only add to the change list if not there because the change list
            // takes precedence.
            if (change_list.find(p.first) == change_list.end()) {
                change_list[p.first] = p.second;
            }
        }
        for (const auto &p : other_encoded_embedded_data) {
            // Other embedded data excludes keys managed by the change list, so
            // no need to exclude anything, unlike with embedded_data.
            change_list[p.first] = p.second;
        }
    }
}

} // namespace

IPTCPanel::IPTCPanel():
    changeList(new rtengine::procparams::IPTCPairs),
    defChangeList(new rtengine::procparams::IPTCPairs),
    embeddedData(new rtengine::procparams::IPTCPairs),
    otherEncodedEmbeddedData(new rtengine::procparams::IPTCPairs())
{

    set_orientation(Gtk::ORIENTATION_VERTICAL);
    set_spacing(4);

    Gtk::Grid* iptc = Gtk::manage(new Gtk::Grid());
    setExpandAlignProperties(iptc, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    iptc->set_row_spacing(3);

    int row = 0;

    Gtk::Label* capl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_DESCRIPTION") + ":"));
    setExpandAlignProperties(capl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    captionText = Gtk::TextBuffer::create();
    captionView = Gtk::manage(new Gtk::TextView(captionText));
    setExpandAlignProperties(captionView, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    Gtk::ScrolledWindow* scrolledWindowc = Gtk::manage(new Gtk::ScrolledWindow());
    setExpandAlignProperties(scrolledWindowc, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    scrolledWindowc->set_min_content_height(100);
    scrolledWindowc->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scrolledWindowc->add(*captionView);
    capl->set_tooltip_text(M("IPTCPANEL_DESCRIPTIONHINT"));
    captionView->set_tooltip_text(M("IPTCPANEL_DESCRIPTIONHINT"));
    captionView->set_size_request(35, 95);
    iptc->attach(*capl, 0, row++, 1, 1);
    iptc->attach(*scrolledWindowc, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* capwl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_DESCRIPTIONWRITER") + ":"));
    setExpandAlignProperties(capwl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    captionWriter = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(captionWriter, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    capwl->set_tooltip_text(M("IPTCPANEL_DESCRIPTIONWRITERHINT"));
    captionWriter->set_tooltip_text(M("IPTCPANEL_DESCRIPTIONWRITERHINT"));
    iptc->attach(*capwl, 0, row++, 1, 1);
    iptc->attach(*captionWriter, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* headl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_HEADLINE") + ":"));
    setExpandAlignProperties(headl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    headline = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(headline, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_BASELINE);
    headl->set_tooltip_text(M("IPTCPANEL_HEADLINEHINT"));
    headline->set_tooltip_text(M("IPTCPANEL_HEADLINEHINT"));
    iptc->attach(*headl, 0, row++, 1, 1);
    iptc->attach(*headline, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* instl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_INSTRUCTIONS") + ":"));
    setExpandAlignProperties(instl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    instructions = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(instructions, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    instl->set_tooltip_text(M("IPTCPANEL_INSTRUCTIONSHINT"));
    instructions->set_tooltip_text(M("IPTCPANEL_INSTRUCTIONSHINT"));
    iptc->attach(*instl, 0, row++, 1, 1);
    iptc->attach(*instructions, 0, row++, 1, 1);

    // --------------------------

    Gtk::Separator* hsep1 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    setExpandAlignProperties(hsep1, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    iptc->attach(*hsep1, 0, row++, 2, 1);

    // --------------------------

    Gtk::Label* keyl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_KEYWORDS") + ":"));
    setExpandAlignProperties(keyl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    keyl->set_tooltip_text(M("IPTCPANEL_KEYWORDSHINT"));
    keywords = Gtk::manage(new Gtk::ListViewText(1, false, Gtk::SELECTION_MULTIPLE));
    setExpandAlignProperties(keywords, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    keywords->set_headers_visible(false);
    keywords->set_size_request(50, 95);
    Gtk::ScrolledWindow* scrolledWindowkw = Gtk::manage(new Gtk::ScrolledWindow());
    setExpandAlignProperties(scrolledWindowkw, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    scrolledWindowkw->set_min_content_height(100);
    scrolledWindowkw->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scrolledWindowkw->add(*keywords);
    keyword  = Gtk::manage(new MyComboBoxText(true));
    setExpandAlignProperties(keyword, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    keyword->set_size_request(75);
    keywords->set_tooltip_text(M("IPTCPANEL_KEYWORDSHINT"));
    keyword->set_tooltip_text(M("IPTCPANEL_KEYWORDSHINT"));
    addKW = Gtk::manage(new Gtk::Button());
    setExpandAlignProperties(addKW, false, true, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    delKW = Gtk::manage(new Gtk::Button());
    setExpandAlignProperties(delKW, false, true, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    Gtk::Image* const addKWImg = Gtk::manage(new RTImage("add-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties(addKWImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    Gtk::Image* const delKWImg = Gtk::manage(new RTImage("remove-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties(delKWImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    addKW->add(*addKWImg);
    delKW->add(*delKWImg);
    Gtk::Grid* kwgrid = Gtk::manage(new Gtk::Grid());
    setExpandAlignProperties(kwgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    kwgrid->attach(*keyword, 0, 0, 1, 1);
    kwgrid->attach(*addKW, 1, 0, 1, 1);
    kwgrid->attach(*delKW, 2, 0, 1, 1);
    iptc->attach(*keyl, 0, row++, 1, 1);
    iptc->attach(*kwgrid, 0, row++, 1, 1);
    // --------------------------
    iptc->attach(*scrolledWindowkw, 0, row++, 2, 1);
    // --------------------------

    Gtk::Separator* hsep2 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    setExpandAlignProperties(hsep2, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    iptc->attach(*hsep2, 0, row++, 2, 1);
    // --------------------------

    Gtk::Label* catl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_CATEGORY") + ":"));
    setExpandAlignProperties(catl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    category = Gtk::manage(new MyComboBoxText(true));
    category->set_size_request(75);
    setExpandAlignProperties(category, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    catl->set_tooltip_text(M("IPTCPANEL_CATEGORYHINT"));
    category->set_tooltip_text(M("IPTCPANEL_CATEGORYHINT"));
    Gtk::Label* scl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_SUPPCATEGORIES") + ":"));
    setExpandAlignProperties(scl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    suppCategories = Gtk::manage(new Gtk::ListViewText(1, false, Gtk::SELECTION_MULTIPLE));
    setExpandAlignProperties(suppCategories, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    suppCategories->set_headers_visible(false);
    suppCategories->set_size_request(50, 95);
    Gtk::ScrolledWindow* scrolledWindowsc = Gtk::manage(new Gtk::ScrolledWindow());
    setExpandAlignProperties(scrolledWindowsc, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    scrolledWindowsc->set_min_content_height(100);
    scrolledWindowsc->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scrolledWindowsc->add(*suppCategories);
    suppCategory  = Gtk::manage(new MyComboBoxText(true));
    suppCategory->set_size_request(75);
    setExpandAlignProperties(suppCategory, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    scl->set_tooltip_text(M("IPTCPANEL_SUPPCATEGORIESHINT"));
    suppCategories->set_tooltip_text(M("IPTCPANEL_SUPPCATEGORIESHINT"));
    suppCategory->set_tooltip_text(M("IPTCPANEL_SUPPCATEGORIESHINT"));
    addSC = Gtk::manage(new Gtk::Button());
    setExpandAlignProperties(addSC, false, true, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    delSC = Gtk::manage(new Gtk::Button());
    setExpandAlignProperties(delSC, false, true, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    Gtk::Image* const addSCImg = Gtk::manage(new RTImage("add-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties(addSCImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    Gtk::Image* const delSCImg = Gtk::manage(new RTImage("remove-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties(delSCImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    addSC->add(*addSCImg);
    delSC->add(*delSCImg);
    Gtk::Grid* scgrid = Gtk::manage(new Gtk::Grid());
    setExpandAlignProperties(scgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    scgrid->attach(*suppCategory, 0, 0, 1, 1);
    scgrid->attach(*addSC, 1, 0, 1, 1);
    scgrid->attach(*delSC, 2, 0, 1, 1);
    iptc->attach(*catl, 0, row++, 1, 1);
    iptc->attach(*category, 0, row++, 1, 1);
    // --------------------------
    iptc->attach(*scl, 0, row++, 1, 1);
    iptc->attach(*scgrid, 0, row++, 1, 1);
    // --------------------------
    iptc->attach(*scrolledWindowsc, 0, row++, 2, 1);
    // --------------------------

    Gtk::Separator* hsep3 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    setExpandAlignProperties(hsep3, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    iptc->attach(*hsep3, 0, row++, 2, 1);
    // --------------------------

    Gtk::Label* creatorLbl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_CREATOR") + ":"));
    setExpandAlignProperties(creatorLbl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    creator = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(creator, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    creatorLbl->set_tooltip_text(M("IPTCPANEL_CREATORHINT"));
    creator->set_tooltip_text(M("IPTCPANEL_CREATORHINT"));
    iptc->attach(*creatorLbl, 0, row++, 1, 1);
    iptc->attach(*creator, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* creatorJobTitleLbl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_CREATORJOBTITLE") + ":"));
    setExpandAlignProperties(creatorJobTitleLbl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    creatorJobTitle = Gtk::manage(  new Gtk::Entry());
    setExpandAlignProperties(creatorJobTitle, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    creatorJobTitleLbl->set_tooltip_text(M("IPTCPANEL_CREATORJOBTITLEHINT"));
    creatorJobTitle->set_tooltip_text(M("IPTCPANEL_CREATORJOBTITLEHINT"));
    iptc->attach(*creatorJobTitleLbl, 0, row++, 1, 1);
    iptc->attach(*creatorJobTitle, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* credl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_CREDIT") + ":"));
    setExpandAlignProperties(credl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    credit = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(credit, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    credl->set_tooltip_text(M("IPTCPANEL_CREDITHINT"));
    credit->set_tooltip_text(M("IPTCPANEL_CREDITHINT"));
    iptc->attach(*credl, 0, row++, 1, 1);
    iptc->attach(*credit, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* sourl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_SOURCE") + ":"));
    setExpandAlignProperties(sourl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    source = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(source, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    sourl->set_tooltip_text(M("IPTCPANEL_SOURCEHINT"));
    source->set_tooltip_text(M("IPTCPANEL_SOURCEHINT"));
    iptc->attach(*sourl, 0, row++, 1, 1);
    iptc->attach(*source, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* cprl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_COPYRIGHT") + ":"));
    setExpandAlignProperties(cprl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    copyright = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(copyright, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    cprl->set_tooltip_text(M("IPTCPANEL_COPYRIGHTHINT"));
    copyright->set_tooltip_text(M("IPTCPANEL_COPYRIGHTHINT"));
    iptc->attach(*cprl, 0, row++, 1, 1);
    iptc->attach(*copyright, 0, row++, 1, 1);

    // --------------------------

    Gtk::Separator* hsep4 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    setExpandAlignProperties(hsep4, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    iptc->attach(*hsep4, 0, row++, 2, 1);

    // --------------------------

    Gtk::Label* cityl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_CITY") + ":"));
    setExpandAlignProperties(cityl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    city = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(city, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    cityl->set_tooltip_text(M("IPTCPANEL_CITYHINT"));
    city->set_tooltip_text(M("IPTCPANEL_CITYHINT"));
    iptc->attach(*cityl, 0, row++, 1, 1);
    iptc->attach(*city, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* provl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_PROVINCE") + ":"));
    setExpandAlignProperties(provl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    province = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(province, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    provl->set_tooltip_text(M("IPTCPANEL_PROVINCEHINT"));
    province->set_tooltip_text(M("IPTCPANEL_PROVINCEHINT"));
    iptc->attach(*provl, 0, row++, 1, 1);
    iptc->attach(*province, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* ctrl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_COUNTRY") + ":"));
    setExpandAlignProperties(ctrl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    country = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(country, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    ctrl->set_tooltip_text(M("IPTCPANEL_COUNTRYHINT"));
    country->set_tooltip_text(M("IPTCPANEL_COUNTRYHINT"));
    iptc->attach(*ctrl, 0, row++, 1, 1);
    iptc->attach(*country, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* titll = Gtk::manage(new Gtk::Label(M("IPTCPANEL_TITLE") + ":"));
    setExpandAlignProperties(titll, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    title = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(title, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    titll->set_tooltip_text(M("IPTCPANEL_TITLEHINT"));
    title->set_tooltip_text(M("IPTCPANEL_TITLEHINT"));
    iptc->attach(*titll, 0, row++, 1, 1);
    iptc->attach(*title, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* dcl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_DATECREATED") + ":"));
    setExpandAlignProperties(dcl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    dateCreated = Gtk::manage(  new Gtk::Entry());
    setExpandAlignProperties(dateCreated, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    dcl->set_tooltip_text(M("IPTCPANEL_DATECREATEDHINT"));
    dateCreated->set_tooltip_text(M("IPTCPANEL_DATECREATEDHINT"));
    iptc->attach(*dcl, 0, row++, 1, 1);
    iptc->attach(*dateCreated, 0, row++, 1, 1);

    // --------------------------

    Gtk::Label* trl = Gtk::manage(new Gtk::Label(M("IPTCPANEL_TRANSREFERENCE") + ":"));
    setExpandAlignProperties(trl, true, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    transReference = Gtk::manage(new Gtk::Entry());
    setExpandAlignProperties(transReference, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    trl->set_tooltip_text(M("IPTCPANEL_TRANSREFERENCEHINT"));
    transReference->set_tooltip_text(M("IPTCPANEL_TRANSREFERENCEHINT"));
    iptc->attach(*trl, 0, row++, 1, 1);
    iptc->attach(*transReference, 0, row++, 1, 1);

    // --------------------------

    Gtk::ScrolledWindow* scrolledWindow = Gtk::manage(new Gtk::ScrolledWindow());
    setExpandAlignProperties(scrolledWindow, false, true, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    scrolledWindow->set_shadow_type(Gtk::SHADOW_NONE);
    scrolledWindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scrolledWindow->property_window_placement().set_value(Gtk::CORNER_TOP_RIGHT);
    scrolledWindow->add(*iptc);

    pack_start(*scrolledWindow);

    Gtk::Grid* bbox = Gtk::manage(new Gtk::Grid());
    setExpandAlignProperties(bbox, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);

    reset = Gtk::manage(new Gtk::Button());  // M("IPTCPANEL_RESET")
    reset->get_style_context()->add_class("Left");
    reset->set_image(*Gtk::manage(new RTImage("undo", Gtk::ICON_SIZE_BUTTON)));
    setExpandAlignProperties(reset, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    bbox->attach_next_to(*reset, Gtk::POS_LEFT, 1, 1);

    file = Gtk::manage(new Gtk::Button());  // M("IPTCPANEL_EMBEDDED")
    file->get_style_context()->add_class("MiddleH");
    file->set_image(*Gtk::manage(new RTImage("folder-open", Gtk::ICON_SIZE_BUTTON)));
    setExpandAlignProperties(file, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    bbox->attach_next_to(*file, Gtk::POS_RIGHT, 1, 1);

    copy = Gtk::manage(new Gtk::Button());
    copy->get_style_context()->add_class("MiddleH");
    copy->set_image(*Gtk::manage(new RTImage("copy", Gtk::ICON_SIZE_BUTTON)));
    setExpandAlignProperties(copy, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    bbox->attach_next_to(*copy, Gtk::POS_RIGHT, 1, 1);

    paste = Gtk::manage(new Gtk::Button());
    paste->get_style_context()->add_class("Right");
    paste->set_image(*Gtk::manage(new RTImage("paste", Gtk::ICON_SIZE_BUTTON)));
    setExpandAlignProperties(paste, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    bbox->attach_next_to(*paste, Gtk::POS_RIGHT, 1, 1);

    pack_end(*bbox, Gtk::PACK_SHRINK, 2);

    reset->set_tooltip_text(M("IPTCPANEL_RESETHINT"));
    file->set_tooltip_text(M("IPTCPANEL_EMBEDDEDHINT"));
    copy->set_tooltip_text(M("IPTCPANEL_COPYHINT"));
    paste->set_tooltip_text(M("IPTCPANEL_PASTEHINT"));

    reset->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::resetClicked));
    file->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::fileClicked));
    copy->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::copyClicked));
    paste->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::pasteClicked));


    addKW->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::addKeyWord));
    delKW->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::delKeyWord));
    addSC->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::addSuppCategory));
    delSC->signal_clicked().connect(sigc::mem_fun(*this, &IPTCPanel::delSuppCategory));
    keyword->get_entry()->signal_activate().connect(sigc::mem_fun(*this, &IPTCPanel::addKeyWord));
    suppCategory->get_entry()->signal_activate().connect(sigc::mem_fun(*this, &IPTCPanel::addSuppCategory));

    conns[0] = captionText->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[1] = captionWriter->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[2] = headline->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[3] = instructions->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[4] = category->get_entry()->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[5] = creator->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[6] = creatorJobTitle->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[7] = credit->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[8] = source->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[9] = copyright->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[10] = city->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[11] = province->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[12] = country->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[13] = title->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[14] = dateCreated->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));
    conns[15] = transReference->signal_changed().connect(sigc::mem_fun(*this, &IPTCPanel::updateChangeList));

    category->get_entry()->set_max_length(3);
    keyword->get_entry()->set_max_length(64);
    captionWriter->set_max_length(32);
    instructions->set_max_length(256);
    creator->set_max_length(32);
    creatorJobTitle->set_max_length(32);
    credit->set_max_length(32);
    source->set_max_length(32);
    copyright->set_max_length(128);
    city->set_max_length(32);
    province->set_max_length(32);
    country->set_max_length(64);
    title->set_max_length(64);
    dateCreated->set_max_length(10);
    transReference->set_max_length(32);

    show_all();
}


void IPTCPanel::read (const ProcParams* pp, const ParamsEdited* pedited)
{
    disableListener();
    changeList->clear();

    if (!pp->metadata.iptc.empty()) {
        *changeList = pp->metadata.iptc;
        changelist_valid_ = true;
    } else {
        *changeList = *embeddedData;
        changelist_valid_ = false;
    }

    applyChangeList();
    enableListener();
}


void IPTCPanel::write (ProcParams* pp, ParamsEdited* pedited)
{
    if (changelist_valid_) {
        pp->metadata.iptc = *changeList;
    } else {
        pp->metadata.iptc.clear();
    }
}


void IPTCPanel::setDefaults (const ProcParams* defParams, const ParamsEdited* pedited)
{
    *defChangeList = defParams->metadata.iptc;
}


void IPTCPanel::setImageData(const FramesMetaData* id)
{
    embeddedData->clear();
    otherEncodedEmbeddedData->clear();
    embedded_data_is_utf8 = false;
    if (id) {
        try {
            rtengine::Exiv2Metadata meta(id->getFileName());
            meta.load();
            auto& iptc = meta.iptcData();
            // First we need to check the encoding.
            // See https://www.iptc.org/std/IIM/4.2/specification/IIMV4.2.pdf
            // chapter 5, 1:90 and appendix C.
            //
            // Note 1: The default encoding is ISO 646 IRV or ISO 4873 DV, which
            // only has 7 bits of characters. Glib::convert() and friends cannot
            // handle invalid byte sequences in the input, so we use CP1252
            // (similar to Latin1, Latin-1, or ISO-8859-1, which has control
            // characters in place of some graphical characters) which basically
            // is an extension of ISO 646 IRV or ISO 4873 DV with 8 bits of
            // characters. Files without a specified coded character set but
            // with 8-bit characters have been seen in the wild (see
            // https://github.com/RawTherapee/RawTherapee/issues/7742). CP1252
            // decoding works well in that example.
            //
            // Note 2: The encoding is specified with ISO 2022 control
            // functions, which is too complex to handle without a library.
            // Instead of full support, we will only check for UTF-8 or fall
            // back to CP1252. This should work in the vast majority of cases.
            const auto coded_character_set_iter =
                iptc.findKey(Exiv2::IptcKey(CODED_CHARACTER_SET));
            if (coded_character_set_iter != iptc.end()
                && coded_character_set_iter->toString() == CONTROL_FUNCTION_UTF8) {
                embedded_data_is_utf8 = true;
            }
            for (const auto& tag : iptc) {
                if (iptc_keys.find(tag.key()) != iptc_keys.end()) {
                    Glib::ustring value =
                        embedded_data_is_utf8 ? Glib::ustring(tag.toString())
                                              : convertCp1252ToUtf8(tag.toString());
                    (*embeddedData)[tag.key()].emplace_back(std::move(value));
               } else if (!embedded_data_is_utf8 && tag.record() >= 2 && tag.record() <= 6 && tag.typeId() == Exiv2::TypeId::string) {
                    // For UTF-8 conversion, capture the data in records 2-6.
                    Glib::ustring value = convertCp1252ToUtf8(tag.toString());
                    (*otherEncodedEmbeddedData)[tag.key()].emplace_back(std::move(value));
                }
            }
        } catch (const std::exception& exc) {
            embeddedData->clear();
            otherEncodedEmbeddedData->clear();
        }
    }

    file->set_sensitive(!embeddedData->empty());
}


void IPTCPanel::notifyListener()
{
    if (listener) {
        listener->panelChanged(EvIPTC, M("HISTORY_CHANGED"));
    }
}


void IPTCPanel::addKeyWord()
{
    keyword->get_entry()->select_region(0, keyword->get_entry()->get_text().size());

    for (unsigned int i = 0; i < keywords->size(); i++) {
        if (keywords->get_text(i) == keyword->get_entry()->get_text()) {
            return;
        }
    }

    keywords->append(keyword->get_entry()->get_text());
    keyword->prepend(keyword->get_entry()->get_text());
    std::vector<Glib::ustring> items;

    for (Gtk::TreeModel::iterator i = keyword->get_model()->children().begin(); i != keyword->get_model()->children().end(); ++i) {
        Glib::ustring s;
        i->get_value(0, s);
        items.push_back(s);
    }

    keyword->remove_all();

    for (unsigned int i = 0; i < 10 && i < items.size(); i++) {
        keyword->append(items[i]);
    }

    keywords->scroll_to_row(keywords->get_model()->get_path(--keywords->get_model()->children().end()));

    updateChangeList();
}


void IPTCPanel::delKeyWord()
{
    std::vector<int> selection = keywords->get_selected();

    if (!selection.empty()) {
        std::vector<Glib::ustring> keep;

        for (unsigned int i = 0; i < keywords->size(); i++)
            if (std::find(selection.begin(), selection.end(), i) == selection.end()) {
                keep.push_back(keywords->get_text(i));
            }

        keywords->clear_items();

        for(unsigned int i = 0; i < keep.size(); i++) {
            keywords->append(keep[i]);
        }
    }

    updateChangeList();
}

void IPTCPanel::addSuppCategory()
{

    for (unsigned int i = 0; i < suppCategories->size(); i++)
        if (suppCategories->get_text(i) == suppCategory->get_entry()->get_text()) {
            return;
        }

    suppCategories->append(suppCategory->get_entry()->get_text());
    suppCategory->prepend(suppCategory->get_entry()->get_text());
    std::vector<Glib::ustring> items;

    for (Gtk::TreeModel::iterator i = suppCategory->get_model()->children().begin(); i != suppCategory->get_model()->children().end(); ++i) {
        Glib::ustring s;
        i->get_value(0, s);
        items.push_back(s);
    }

    suppCategory->remove_all();

    for (unsigned int i = 0; i < 10 && i < items.size(); i++) {
        suppCategory->append(items[i]);
    }

    suppCategories->scroll_to_row(suppCategories->get_model()->get_path(--suppCategories->get_model()->children().end()));
    suppCategory->get_entry()->select_region(0, suppCategory->get_entry()->get_text().size());

    updateChangeList();
}

void IPTCPanel::delSuppCategory()
{

    std::vector<int> selection = suppCategories->get_selected();

    if (!selection.empty()) {
        std::vector<Glib::ustring> keep;

        for (unsigned int i = 0; i < suppCategories->size(); i++)
            if (std::find(selection.begin(), selection.end(), i) == selection.end()) {
                keep.push_back(suppCategories->get_text(i));
            }

        suppCategories->clear_items();

        for (unsigned int i = 0; i < keep.size(); i++) {
            suppCategories->append(keep[i]);
        }
    }

    updateChangeList();
}

void IPTCPanel::updateChangeList()
{
    changelist_valid_ = true;
    changeList->clear();
    (*changeList)[CAPTION].push_back(captionText->get_text());
    (*changeList)[CAPTION_WRITER].push_back(captionWriter->get_text());
    (*changeList)[HEADLINE].push_back(headline->get_text());
    (*changeList)[INSTRUCTIONS].push_back(instructions->get_text());

    std::set<Glib::ustring> sset;
    sset.clear();
    for (unsigned int i = 0; i < keywords->size(); i++) {
        sset.insert(keywords->get_text(i));
    }
    for (auto &s : sset) {
        (*changeList)[KEYWORDS].push_back(s);
    }

    (*changeList)[CATEGORY].push_back(category->get_entry()->get_text());

    sset.clear();
    for (unsigned int i = 0; i < suppCategories->size(); i++) {
        sset.insert(suppCategories->get_text(i));
    }
    for (auto &s : sset) {
        (*changeList)[SUPPLEMENTAL_CATEGORIES].push_back(s);
    }

    (*changeList)[CREATOR].push_back(creator->get_text());
    (*changeList)[CREATOR_JOB_TITLE].push_back(creatorJobTitle->get_text());
    (*changeList)[CREDIT].push_back(credit->get_text());
    (*changeList)[SOURCE].push_back(source->get_text());
    (*changeList)[COPYRIGHT].push_back(copyright->get_text());
    (*changeList)[CITY].push_back(city->get_text());
    (*changeList)[PROVINCE].push_back(province->get_text());
    (*changeList)[COUNTRY].push_back(country->get_text());
    (*changeList)[TITLE].push_back(title->get_text());
    (*changeList)[DATE_CREATED].push_back(dateCreated->get_text());
    (*changeList)[TRANS_REFERENCE].push_back(transReference->get_text());

    updateEncoding(*changeList, *embeddedData, *otherEncodedEmbeddedData, embedded_data_is_utf8);

    notifyListener();
}


void IPTCPanel::applyChangeList()
{
    for (int i = 0; i < 16; i++) {
        conns[i].block(true);
    }

    captionText->set_text("");
    captionWriter->set_text("");
    headline->set_text("");
    instructions->set_text("");
    keywords->clear_items();
    category->get_entry()->set_text("");
    suppCategories->clear_items();
    creator->set_text("");
    creatorJobTitle->set_text("");
    credit->set_text("");
    source->set_text("");
    copyright->set_text("");
    city->set_text("");
    province->set_text("");
    country->set_text("");
    title->set_text("");
    dateCreated->set_text("");
    transReference->set_text("");
    keyword->get_entry()->set_text("");
    suppCategory->get_entry()->set_text("");

    for (rtengine::procparams::IPTCPairs::const_iterator i = changeList->begin(); i != changeList->end(); ++i) {
        if (i->first == CAPTION && !i->second.empty()) {
            captionText->set_text(i->second.at(0));
        } else if (i->first == CAPTION_WRITER && !i->second.empty()) {
            captionWriter->set_text(i->second.at(0));
        } else if (i->first == HEADLINE && !i->second.empty()) {
            headline->set_text(i->second.at(0));
        } else if (i->first == INSTRUCTIONS && !i->second.empty()) {
            instructions->set_text(i->second.at(0));
        } else if (i->first == KEYWORDS) {
            std::set<Glib::ustring> sset;
            for (unsigned int j = 0; j < i->second.size(); j++) {
                sset.insert(i->second[j]);
            }
            for (auto &s : sset) {
                keywords->append(s);
            }
        } else if (i->first == CATEGORY && !i->second.empty()) {
            category->get_entry()->set_text(i->second.at(0));
        } else if (i->first == SUPPLEMENTAL_CATEGORIES) {
            std::set<Glib::ustring> sset;
            for (unsigned int j = 0; j < i->second.size(); j++) {
                sset.insert(i->second[j]);
            }
            for (auto &s : sset) {
                suppCategories->append(s);
            }
        } else if (i->first == CREATOR && !i->second.empty()) {
            creator->set_text(i->second.at(0));
        } else if (i->first == CREATOR_JOB_TITLE && !i->second.empty()) {
            creatorJobTitle->set_text(i->second.at(0));
        } else if (i->first == CREDIT && !i->second.empty()) {
            credit->set_text(i->second.at(0));
        } else if (i->first == SOURCE && !i->second.empty()) {
            source->set_text(i->second.at(0));
        } else if (i->first == COPYRIGHT && !i->second.empty()) {
            copyright->set_text(i->second.at(0));
        } else if (i->first == CITY && !i->second.empty()) {
            city->set_text(i->second.at(0));
        } else if (i->first == PROVINCE && !i->second.empty()) {
            province->set_text(i->second.at(0));
        } else if (i->first == COUNTRY && !i->second.empty()) {
            country->set_text(i->second.at(0));
        } else if (i->first == TITLE && !i->second.empty()) {
            title->set_text(i->second.at(0));
        } else if (i->first == DATE_CREATED && !i->second.empty()) {
            dateCreated->set_text(i->second.at(0));
        } else if (i->first == TRANS_REFERENCE && !i->second.empty()) {
            transReference->set_text(i->second.at(0));
        }
    }

    for (int i = 0; i < 16; i++) {
        conns[i].block(false);
    }
}

void IPTCPanel::resetClicked()
{
    disableListener();
    *changeList = *defChangeList;
    changelist_valid_ = false;
    applyChangeList();
    enableListener();
    notifyListener();
}

void IPTCPanel::fileClicked()
{

    disableListener();
    *changeList = *embeddedData;
    changelist_valid_ = false;
    applyChangeList();
    enableListener();
    notifyListener();
}

void IPTCPanel::copyClicked()
{

    clipboard.setIPTC(*changeList);
}

void IPTCPanel::pasteClicked()
{

    disableListener();
    *changeList = clipboard.getIPTC();
    applyChangeList();
    updateEncoding(*changeList, *embeddedData, *otherEncodedEmbeddedData, embedded_data_is_utf8);
    enableListener();
    notifyListener();
}
