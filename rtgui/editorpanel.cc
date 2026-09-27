/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2010 Oliver Duis <www.oliverduis.de>
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
#include "editorpanel.h"

#include <iostream>

#include "rtengine/array2D.h"
#include "rtengine/clutstore.h"
#include "rtengine/imagesource.h"
#include "rtengine/utils.h"
#include "rtengine/iccstore.h"
#include "batchqueue.h"
#include "batchqueueentry.h"
#include "soundman.h"
#include "rtimage.h"
#include "filepanel.h"
#include "guiutils.h"
#include "options.h"
#include "navigator.h"
#include "previewwindow.h"
#include "progressconnector.h"
#include "procparamchangers.h"
#include "placesbrowser.h"
#include "pathutils.h"
#include "thumbnail.h"
#include "toolpanelcoord.h"
#include "widgets/basic/popupbutton.h"
#include "windows/rtappchooserdialog.h"
#include "windows/rtwindow.h"

#ifdef _WIN32
#include "rtengine/leanwindows.h"
#include "rtengine/winutils.h"
#endif

using namespace rtengine::procparams;

using ScopeType = Options::ScopeType;

namespace
{

void setprogressStrUI(double val, const Glib::ustring str, MyProgressBar* pProgress)
{
    if (!str.empty()) {
        pProgress->set_text(M(str));
    }

    if (val >= 0.0) {
        pProgress->set_fraction(val);
    }
}

#if !defined(__APPLE__) // monitor profile not supported on apple
bool find_default_monitor_profile (GdkWindow *rootwin, Glib::ustring &defprof, Glib::ustring &defprofname)
{
#ifdef _WIN32
    HDC hDC = GetDC (nullptr);

    if (hDC != nullptr) {
        if (SetICMMode (hDC, ICM_ON)) {
            char profileName[MAX_PATH + 1];
            DWORD profileLength = MAX_PATH;

            if (GetICMProfileA (hDC, &profileLength, profileName)) {
                defprof = Glib::ustring (profileName);
                defprofname = Glib::path_get_basename (defprof);
                size_t pos = defprofname.rfind (".");

                if (pos != Glib::ustring::npos) {
                    defprofname = defprofname.substr (0, pos);
                }

                defprof = Glib::ustring ("file:") + defprof;
                return true;
            }

            // might fail if e.g. the monitor has no profile
        }

        ReleaseDC (NULL, hDC);
    }

#else
    // taken from geeqie (image.c) and adapted
    // Originally licensed as GPL v2+, with the following copyright:
    // * Copyright (C) 2006 John Ellis
    // * Copyright (C) 2008 - 2016 The Geeqie Team
    //
    guchar *prof = nullptr;
    gint proflen;
    GdkAtom type = GDK_NONE;
    gint format = 0;

    if (gdk_property_get (rootwin, gdk_atom_intern ("_ICC_PROFILE", FALSE), GDK_NONE, 0, 64 * 1024 * 1024, FALSE, &type, &format, &proflen, &prof) && proflen > 0) {
        cmsHPROFILE p = cmsOpenProfileFromMem (prof, proflen);

        if (p) {
            defprofname = "from GDK";
            defprof = Glib::build_filename (Options::rtdir, "GDK_ICC_PROFILE.icc");

            if (cmsSaveProfileToFile (p, defprof.c_str())) {
                cmsCloseProfile (p);

                if (prof) {
                    g_free (prof);
                }

                defprof = Glib::ustring ("file:") + defprof;
                return true;
            }
        }
    }

    if (prof) {
        g_free (prof);
    }

#endif
    return false;
}
#endif

bool hasUserOnlyPermission(const Glib::ustring &dirname)
{
#if defined(__linux__) || defined(__APPLE__)
    const Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(dirname);
    const Glib::RefPtr<Gio::FileInfo> file_info = file->query_info("owner::user,unix::mode");

    if (!file_info) {
        return false;
    }

    const Glib::ustring owner = file_info->get_attribute_string("owner::user");
    const guint32 mode = file_info->get_attribute_uint32("unix::mode");

    return (mode & 0777) == 0700 && owner == Glib::get_user_name();
#elif defined(_WIN32)
    const Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(dirname);
    const Glib::RefPtr<Gio::FileInfo> file_info = file->query_info("owner::user");
    if (!file_info) {
        return false;
    }

    // Current user must be the owner.
    const Glib::ustring user_name = Glib::get_user_name();
    const Glib::ustring owner = file_info->get_attribute_string("owner::user");
    if (user_name != owner) {
        return false;
    }

    // Get security descriptor and discretionary access control list.
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sec_desc_raw_ptr = nullptr;
    auto win_error = GetNamedSecurityInfo(
        dirname.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &dacl,
        nullptr,
        &sec_desc_raw_ptr
    );
    const WinLocalPtr<PSECURITY_DESCRIPTOR> sec_desc_ptr(sec_desc_raw_ptr);
    if (win_error != ERROR_SUCCESS) {
        return false;
    }

    // Must not inherit permissions.
    SECURITY_DESCRIPTOR_CONTROL sec_desc_control;
    DWORD revision;
    if (!(
        GetSecurityDescriptorControl(sec_desc_ptr, &sec_desc_control, &revision)
        && sec_desc_control & SE_DACL_PROTECTED
    )) {
        return false;
    }

    // Check that there is one entry allowing full access.
    ULONG acl_entry_count;
    PEXPLICIT_ACCESS acl_entry_list_raw = nullptr;
    win_error = GetExplicitEntriesFromAcl(dacl, &acl_entry_count, &acl_entry_list_raw);
    const WinLocalPtr<PEXPLICIT_ACCESS> acl_entry_list(acl_entry_list_raw);
    if (win_error != ERROR_SUCCESS || acl_entry_count != 1) {
        return false;
    }
    const EXPLICIT_ACCESS &ace = acl_entry_list[0];
    if (
        ace.grfAccessMode != GRANT_ACCESS
        || (ace.grfAccessPermissions & FILE_ALL_ACCESS) != FILE_ALL_ACCESS
        || ace.Trustee.TrusteeForm != TRUSTEE_IS_SID // Should already be SID, but double check.
    ) {
        return false;
    }

    // ACE must be for the current user.
    HANDLE process_token_raw;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &process_token_raw)) {
        return false;
    }
    const WinHandle process_token(process_token_raw);
    DWORD actual_token_info_size = 0;
    GetTokenInformation(process_token, TokenUser, nullptr, 0, &actual_token_info_size);
    if (!actual_token_info_size) {
        return false;
    }
    const WinHeapPtr<PTOKEN_USER> user_token_ptr(actual_token_info_size);
    if (!user_token_ptr || !GetTokenInformation(
        process_token,
        TokenUser,
        user_token_ptr,
        actual_token_info_size,
        &actual_token_info_size
    )) {
        return false;
    }
    return EqualSid(ace.Trustee.ptstrName, user_token_ptr->User.Sid);
#endif
    return false;
}

/**
 * Sets read and write permissions, and optionally the execute permission, for
 * the user and no permissions for others.
 */
void setUserOnlyPermission(const Glib::RefPtr<Gio::File> file, bool execute)
{
#if defined(__linux__) || defined(__APPLE__)
    const Glib::RefPtr<Gio::FileInfo> file_info = file->query_info("unix::mode");
    if (!file_info) {
        return;
    }

    guint32 mode = file_info->get_attribute_uint32("unix::mode");
    mode = (mode & ~0777) | (execute ? 0700 : 0600);
    try {
        file->set_attribute_uint32("unix::mode", mode, Gio::FILE_QUERY_INFO_NONE);
    } catch (Gio::Error &) {
    }
#elif defined(_WIN32)
    // Get the current user's SID.
    HANDLE process_token_raw;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &process_token_raw)) {
        return;
    }
    const WinHandle process_token(process_token_raw);
    DWORD actual_token_info_size = 0;
    GetTokenInformation(process_token, TokenUser, nullptr, 0, &actual_token_info_size);
    if (!actual_token_info_size) {
        return;
    }
    const WinHeapPtr<PTOKEN_USER> user_token_ptr(actual_token_info_size);
    if (!user_token_ptr || !GetTokenInformation(
        process_token,
        TokenUser,
        user_token_ptr,
        actual_token_info_size,
        &actual_token_info_size
    )) {
        return;
    }
    const PSID user_sid = user_token_ptr->User.Sid;

    // Get a handle to the file.
    const Glib::ustring filename = file->get_path();
    const HANDLE file_handle_raw = CreateFile(
        filename.c_str(),
        READ_CONTROL | WRITE_DAC,
        0,
        nullptr,
        OPEN_EXISTING,
        execute ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file_handle_raw == INVALID_HANDLE_VALUE) {
        return;
    }
    const WinHandle file_handle(file_handle_raw);

    // Create the user-only permission and set it.
    EXPLICIT_ACCESS ea = {
        .grfAccessPermissions = FILE_ALL_ACCESS,
        .grfAccessMode = GRANT_ACCESS,
        .grfInheritance = NO_INHERITANCE,
        .Trustee = {},
    };
    BuildTrusteeWithSid(&(ea.Trustee), user_sid);
    PACL new_dacl_raw = nullptr;
    auto win_error = SetEntriesInAcl(1, &ea, nullptr, &new_dacl_raw);
    if (win_error != ERROR_SUCCESS) {
        return;
    }
    const WinLocalPtr<PACL> new_dacl(new_dacl_raw);
    SetSecurityInfo(
        file_handle,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        new_dacl,
        nullptr
    );
#endif
}

/**
 * Gets the path to the temp directory, creating it if necessary.
 */
Glib::ustring getTmpDirectory()
{
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    static Glib::ustring recent_dir = "";
    const Glib::ustring tmp_dir_root = Glib::get_tmp_dir();
    const Glib::ustring subdir_base =
        Glib::ustring::compose("rawtherapee-%1", Glib::get_user_name());
    Glib::ustring dir = Glib::build_filename(tmp_dir_root, subdir_base);

    // Returns true if the directory doesn't exist or has the right permissions.
    auto is_usable_dir = [](const Glib::ustring &dir_path) {
        return !Glib::file_test(dir_path, Glib::FILE_TEST_EXISTS) || (Glib::file_test(dir_path, Glib::FILE_TEST_IS_DIR) && hasUserOnlyPermission(dir_path));
    };

    if (!(is_usable_dir(dir) || recent_dir.empty())) {
        // Try to reuse the random suffix directory.
        dir = recent_dir;
    }

    if (!is_usable_dir(dir)) {
        // Create new directory with random suffix.
        gchar *const rand_dir = g_dir_make_tmp((subdir_base + "-XXXXXX").c_str(), nullptr);
        if (!rand_dir) {
            return tmp_dir_root;
        }
        dir = recent_dir = rand_dir;
        g_free(rand_dir);
        Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(dir);
        setUserOnlyPermission(file, true);
    } else if (!Glib::file_test(dir, Glib::FILE_TEST_EXISTS)) {
        // Create the directory.
        Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(dir);
        bool dir_created = file->make_directory();
        if (!dir_created) {
            return tmp_dir_root;
        }
        setUserOnlyPermission(file, true);
    }

    return dir;
#else
    return Glib::get_tmp_dir();
#endif
}
}

class EditorPanel::ColorManagementToolbar
{
private:
#if !defined(__APPLE__) // monitor profile not supported on apple
    MyComboBoxText profileBox;
#endif
    PopUpButton intentBox;
    Gtk::ToggleButton softProof;
    Gtk::ToggleButton spGamutCheck;
    sigc::connection profileConn, intentConn, softproofConn;
    Glib::ustring defprof;

    rtengine::StagedImageProcessor* const& processor;

private:
#if !defined(__APPLE__) // monitor profile not supported on apple
    void prepareProfileBox ()
    {
        profileBox.setPreferredWidth (70, 200);
        setExpandAlignProperties (&profileBox, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

        profileBox.append (M ("PREFERENCES_PROFILE_NONE"));
        Glib::ustring defprofname;

        if (find_default_monitor_profile (profileBox.get_root_window()->gobj(), defprof, defprofname)) {
            profileBox.append (M ("MONITOR_PROFILE_SYSTEM") + " (" + defprofname + ")");

            if (App::get().options().rtSettings.autoMonitorProfile) {
                rtengine::ICCStore::getInstance()->setDefaultMonitorProfileName (defprof);
                profileBox.set_active (1);
            } else {
                profileBox.set_active (0);
            }
        } else {
            profileBox.set_active (0);
        }

        const std::vector<Glib::ustring> profiles = rtengine::ICCStore::getInstance()->getProfiles (rtengine::ICCStore::ProfileType::MONITOR);

        for (const auto& profile : profiles) {
            profileBox.append (profile);
        }

        profileBox.set_tooltip_text (profileBox.get_active_text ());
    }
#endif

    void prepareIntentBox ()
    {
        // same order as the enum
        intentBox.addEntry ("intent-perceptual", M ("PREFERENCES_INTENT_PERCEPTUAL"));
        intentBox.addEntry ("intent-relative", M ("PREFERENCES_INTENT_RELATIVE"));
        intentBox.addEntry ("intent-absolute", M ("PREFERENCES_INTENT_ABSOLUTE"));
        setExpandAlignProperties (intentBox.buttonGroup, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

        intentBox.setSelected (1);
        intentBox.show ();
    }

    void prepareSoftProofingBox ()
    {
        Gtk::Image *softProofImage = Gtk::manage (new RTImage ("gamut-softproof", Gtk::ICON_SIZE_LARGE_TOOLBAR));
        softProofImage->set_padding (0, 0);
        softProof.add (*softProofImage);
        softProof.set_relief (Gtk::RELIEF_NONE);
        softProof.set_tooltip_markup (M ("SOFTPROOF_TOOLTIP"));

        softProof.set_active (false);
        softProof.show ();

        Gtk::Image *spGamutCheckImage = Gtk::manage (new RTImage ("gamut-warning", Gtk::ICON_SIZE_LARGE_TOOLBAR));
        spGamutCheckImage->set_padding (0, 0);
        spGamutCheck.add (*spGamutCheckImage);
        spGamutCheck.set_relief (Gtk::RELIEF_NONE);
        spGamutCheck.set_tooltip_markup (M ("SOFTPROOF_GAMUTCHECK_TOOLTIP"));

        spGamutCheck.set_active (false);
        spGamutCheck.set_sensitive (true);
        spGamutCheck.show ();
    }

#if !defined(__APPLE__)
    void profileBoxChanged ()
    {
        updateParameters ();
    }
#endif

    void intentBoxChanged (int)
    {
        updateParameters ();
    }

    void softProofToggled ()
    {
        updateSoftProofParameters ();
    }

    void spGamutCheckToggled ()
    {
        updateSoftProofParameters ();
    }

    void updateParameters (bool noEvent = false)
    {
#if !defined(__APPLE__) // monitor profile not supported on apple
        ConnectionBlocker profileBlocker (profileConn);
#endif
        ConnectionBlocker intentBlocker (intentConn);

        Glib::ustring profile;
        const auto& options = App::get().options();

#if !defined(__APPLE__) // monitor profile not supported on apple

        if (!defprof.empty() && profileBox.get_active_row_number () == 1) {
            profile = defprof;

            if (profile.empty ()) {
                profile = options.rtSettings.monitorProfile;
            }

            if (profile.empty ()) {
                profile = "sRGB IEC61966-2.1";
            }
        } else if (profileBox.get_active_row_number () > 0) {
            profile = profileBox.get_active_text ();
        }

#else
        profile = options.rtSettings.srgb;
#endif

#if !defined(__APPLE__) // monitor profile not supported on apple

        if (profileBox.get_active_row_number () == 0) {

            profile.clear ();

            intentBox.set_sensitive (false);
            intentBox.setSelected (1);
            softProof.set_sensitive (false);
            spGamutCheck.set_sensitive (false);

            profileBox.set_tooltip_text ("");

        } else {
            const uint8_t supportedIntents = rtengine::ICCStore::getInstance()->getProofIntents (profile);
            const bool supportsRelativeColorimetric = supportedIntents & 1 << INTENT_RELATIVE_COLORIMETRIC;
            const bool supportsPerceptual = supportedIntents & 1 << INTENT_PERCEPTUAL;
            const bool supportsAbsoluteColorimetric = supportedIntents & 1 << INTENT_ABSOLUTE_COLORIMETRIC;

            if (supportsPerceptual || supportsRelativeColorimetric || supportsAbsoluteColorimetric) {
                intentBox.set_sensitive (true);
                intentBox.setItemSensitivity (0, supportsPerceptual);
                intentBox.setItemSensitivity (1, supportsRelativeColorimetric);
                intentBox.setItemSensitivity (2, supportsAbsoluteColorimetric);
                softProof.set_sensitive (true);
                spGamutCheck.set_sensitive (true);
            } else {
                intentBox.setItemSensitivity (0, true);
                intentBox.setItemSensitivity (1, true);
                intentBox.setItemSensitivity (2, true);
                intentBox.set_sensitive (false);
                intentBox.setSelected (1);
                softProof.set_sensitive (false);
                spGamutCheck.set_sensitive (true);
            }

            profileBox.set_tooltip_text (profileBox.get_active_text ());
        }

#endif
        rtengine::RenderingIntent intent;

        switch (intentBox.getSelected ()) {
            default:
            case 0:
                intent = rtengine::RI_PERCEPTUAL;
                break;

            case 1:
                intent = rtengine::RI_RELATIVE;
                break;

            case 2:
                intent = rtengine::RI_ABSOLUTE;
                break;
        }

        if (!processor) {
            return;
        }

        if (!noEvent) {
            processor->beginUpdateParams ();
        }

        processor->setMonitorProfile (profile, intent);
        processor->setSoftProofing (softProof.get_sensitive() && softProof.get_active(), spGamutCheck.get_sensitive() && spGamutCheck.get_active());

        if (!noEvent) {
            processor->endUpdateParams (rtengine::EvMonitorTransform);
        }
    }

    void updateSoftProofParameters (bool noEvent = false)
    {
#if !defined(__APPLE__) // monitor profile not supported on apple
        softProof.set_sensitive (profileBox.get_active_row_number () > 0);
        spGamutCheck.set_sensitive(profileBox.get_active_row_number () > 0);
#endif


#if !defined(__APPLE__) // monitor profile not supported on apple

        if (profileBox.get_active_row_number () > 0) {
#endif

            if (processor) {
                if (!noEvent) {
                    processor->beginUpdateParams ();
                }

                processor->setSoftProofing (softProof.get_sensitive() && softProof.get_active(), spGamutCheck.get_active());

                if (!noEvent) {
                    processor->endUpdateParams (rtengine::EvMonitorTransform);
                }
            }

#if !defined(__APPLE__) // monitor profile not supported on apple
        }

#endif
    }

public:
    explicit ColorManagementToolbar (rtengine::StagedImageProcessor* const& ipc) :
        intentBox (Glib::ustring (), true),
        processor (ipc)
    {
#if !defined(__APPLE__) // monitor profile not supported on apple
        prepareProfileBox ();
#endif
        prepareIntentBox ();
        prepareSoftProofingBox ();

        reset ();

        softproofConn = softProof.signal_toggled().connect (sigc::mem_fun (this, &ColorManagementToolbar::softProofToggled));
        spGamutCheck.signal_toggled().connect (sigc::mem_fun (this, &ColorManagementToolbar::spGamutCheckToggled));
#if !defined(__APPLE__) // monitor profile not supported on apple
        profileConn = profileBox.signal_changed ().connect (sigc::mem_fun (this, &ColorManagementToolbar::profileBoxChanged));
#endif
        intentConn = intentBox.signal_changed ().connect (sigc::mem_fun (this, &ColorManagementToolbar::intentBoxChanged));
    }

    void pack_right_in (Gtk::Grid* grid)
    {
#if !defined(__APPLE__) // monitor profile not supported on apple
        grid->attach_next_to (profileBox, Gtk::POS_RIGHT, 1, 1);
#endif
        grid->attach_next_to (*intentBox.buttonGroup, Gtk::POS_RIGHT, 1, 1);
        grid->attach_next_to (softProof, Gtk::POS_RIGHT, 1, 1);
        grid->attach_next_to (spGamutCheck, Gtk::POS_RIGHT, 1, 1);
    }

    void updateProcessor()
    {
        if (processor) {
            updateParameters (true);
        }
    }

    void reset ()
    {
        const auto& options = App::get().options();

        ConnectionBlocker intentBlocker (intentConn);
#if !defined(__APPLE__) // monitor profile not supported on apple
        ConnectionBlocker profileBlocker (profileConn);

        if (!defprof.empty() && options.rtSettings.autoMonitorProfile) {
            profileBox.set_active (1);
        } else {
            setActiveTextOrIndex (profileBox, options.rtSettings.monitorProfile, 0);
        }

#endif

        switch (options.rtSettings.monitorIntent) {
            default:
            case rtengine::RI_PERCEPTUAL:
                intentBox.setSelected (0);
                break;

            case rtengine::RI_RELATIVE:
                intentBox.setSelected (1);
                break;

            case rtengine::RI_ABSOLUTE:
                intentBox.setSelected (2);
                break;
        }

        updateParameters ();
    }

    void updateHistogram()
    {
      updateParameters();
    }


    void defaultMonitorProfileChanged (const Glib::ustring &profile_name, bool auto_monitor_profile)
    {
        ConnectionBlocker profileBlocker (profileConn);

        if (auto_monitor_profile && !defprof.empty()) {
            rtengine::ICCStore::getInstance()->setDefaultMonitorProfileName (defprof);
#ifndef __APPLE__
            profileBox.set_active (1);
#endif
        } else {
            rtengine::ICCStore::getInstance()->setDefaultMonitorProfileName (profile_name);
#ifndef __APPLE__
            setActiveTextOrIndex (profileBox, profile_name, 0);
#endif
        }
    }

};

namespace
{

// Returns a copy of src suitable for processing a Hald CLUT identity image.
//
// The generated LUT operates on display-referred (already tone-curved) values,
// matching how RT applies its own film simulation CLUTs: gamma sRGB is applied
// before the CLUT lookup, inverse gamma after.  The tone curve is always reset
// to neutral: it runs earlier in RT's pipeline, so including it here would
// apply it twice and produce incorrect results.
//
// Kept: Lab curves, RGB curves, HSV equalizer, vibrance, colour toning,
//       colour appearance, shadows/highlights, tone equalizer, gamut
//       compression, dehaze, soft-light, film simulation, channel mixer,
//       black & white, output colour management.
// Reset to neutral: tone curve.
// Disabled: sharpening, noise reduction, edge-preserving / Retinex tone
//           mapping, local contrast, all geometric transforms, lens/CA/vignette
//           corrections, gradient, spot removal, locallab, wavelet, dir-pyr,
//           resize, framing, film negative.
ProcParams makeLUTProcParams(const ProcParams& src)
{
    ProcParams p = src;

    // Tone curve — applied before the CLUT in RT's pipeline; including it
    // here would double the effect and break the LUT.
    p.toneCurve = ToneCurveParams{};

    // Sharpening (spatial)
    p.sharpening.enabled   = false;
    p.prsharpening.enabled = false;
    p.pdsharpening.enabled = false;
    p.sharpenEdge.enabled  = false;
    p.sharpenMicro.enabled = false;

    // Noise reduction (spatial)
    p.defringe.enabled       = false;
    p.impulseDenoise.enabled = false;
    p.dirpyrDenoise.enabled  = false;

    // Tone mapping (edge-aware / spatial)
    p.epd.enabled     = false;
    p.fattal.enabled  = false;
    p.retinex.enabled = false;

    // Local contrast (radius-based)
    p.localContrast.enabled = false;

    // Geometric transforms
    p.crop.enabled      = false;
    p.coarse.rotate     = 0;
    p.coarse.hflip      = false;
    p.coarse.vflip      = false;
    p.rotate.degree     = 0.0;
    p.distortion.amount = 0.0;
    p.distortion.defish = false;
    p.lensProf.lcMode   = LensProfParams::LcMode::NONE;
    p.perspective.horizontal = 0.0;
    p.perspective.vertical   = 0.0;
    p.perspective.render     = false;

    // Optical corrections (position-dependent)
    p.cacorrection.red   = 0.0;
    p.cacorrection.blue  = 0.0;
    p.vignetting.amount  = 0;
    p.gradient.enabled   = false;
    p.pcvignette.enabled = false;

    // Spot removal and local adjustments (position-dependent)
    p.spot.enabled     = false;
    p.locallab.enabled = false;

    // Wavelet / directional pyramid (spatial)
    p.wavelet.enabled         = false;
    p.dirpyrequalizer.enabled = false;

    // Resize, framing, film negative
    p.resize.enabled       = false;
    p.framing.enabled      = false;
    p.filmNegative.enabled = false;

    return p;
}

} // namespace

EditorPanel::EditorPanel (FilePanel* filePanel)
    : catalogPane (nullptr), realized (false), tbBeforeLock (nullptr), iHistoryShow (nullptr), iHistoryHide (nullptr),
      iTopPanel_1_Show (nullptr), iTopPanel_1_Hide (nullptr), iRightPanel_1_Show (nullptr), iRightPanel_1_Hide (nullptr),
      iBeforeLockON (nullptr), iBeforeLockOFF (nullptr),
      externalEditorChangedSignal (nullptr),
      previewHandler (nullptr), beforePreviewHandler (nullptr),
      beforeIarea (nullptr), beforeBox (nullptr), afterBox (nullptr), beforeLabel (nullptr), afterLabel (nullptr),
      beforeHeaderBox (nullptr), afterHeaderBox (nullptr), parent (nullptr), parentWindow (nullptr), openThm (nullptr),
      selectedFrame(0), isrc (nullptr), ipc (nullptr), beforeIpc (nullptr), err (0), isProcessing (false),
      histogram_observable(nullptr), histogram_scope_type(ScopeType::NONE)
{

    set_orientation(Gtk::ORIENTATION_VERTICAL);
    epih = new EditorPanelIdleHelper;
    epih->epanel = this;
    epih->destroyed = false;
    epih->pending = 0;
    //rtengine::befaf=true;
    processingStartedTime = 0;
    firstProcessingDone = false;

    // construct toolpanelcoordinator
    tpc = new ToolPanelCoordinator();
    tpc->setProgressListener(this);

    // build GUI

    // build left side panel
    leftbox = new Gtk::Paned (Gtk::ORIENTATION_VERTICAL);

    // make a subpaned to allow resizing of the histogram (if it's on the left)
    leftsubpaned = new Gtk::Paned(Gtk::ORIENTATION_VERTICAL);
    leftsubpaned->set_size_request(230, 250);

    histogramPanel = nullptr;

    profilep = Gtk::manage(new ProfilePanel());
    ppframe = Gtk::manage(new Gtk::Frame());
    ppframe->set_label_align(0.025, 0.5);
    ppframe->set_name ("ProfilePanel");
    ppframe->add (*profilep);
    ppframe->set_label(M("PROFILEPANEL_LABEL"));
    //leftsubpaned->pack_start (*ppframe, Gtk::PACK_SHRINK, 4);

    navigator = Gtk::manage(new Navigator());
    navigator->previewWindow->set_size_request(-1, RTScalable::scalePixelSize(150));
    leftsubpaned->pack1(*navigator, false, false);

    history = Gtk::manage(new History());
    leftsubpaned->pack2(*history, true, false);

    leftsubpaned->set_position(0);
    leftsubpaned->show_all();

    leftbox->pack2(*leftsubpaned, true, true);
    leftbox->show_all();

    // build the middle of the screen
    Gtk::Box* editbox = Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_VERTICAL));

    info = Gtk::manage (new Gtk::ToggleButton ());
    Gtk::Image* infoimg = Gtk::manage (new RTImage ("info", Gtk::ICON_SIZE_LARGE_TOOLBAR));
    info->add (*infoimg);
    info->set_relief (Gtk::RELIEF_NONE);
    info->set_tooltip_markup (M ("MAIN_TOOLTIP_QINFO"));

    beforeAfter = Gtk::manage (new Gtk::ToggleButton ());
    Gtk::Image* beforeAfterIcon = Gtk::manage (new RTImage ("beforeafter", Gtk::ICON_SIZE_LARGE_TOOLBAR));
    beforeAfter->add (*beforeAfterIcon);
    beforeAfter->set_relief (Gtk::RELIEF_NONE);
    beforeAfter->set_tooltip_markup (M ("MAIN_TOOLTIP_TOGGLE"));

    iBeforeLockON = new RTImage ("padlock-locked-small", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    iBeforeLockOFF = new RTImage ("padlock-unlocked-small", Gtk::ICON_SIZE_LARGE_TOOLBAR);

    Gtk::Separator* vsept = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vsepz = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vsepi = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vseph = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));

    hidehp = Gtk::manage (new Gtk::ToggleButton ());

    iHistoryShow = new RTImage ("panel-to-right", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    iHistoryHide = new RTImage ("panel-to-left", Gtk::ICON_SIZE_LARGE_TOOLBAR);

    const auto& options = App::get().options();
    hidehp->set_relief (Gtk::RELIEF_NONE);
    hidehp->set_active (options.showHistory);
    hidehp->set_tooltip_markup (M ("MAIN_TOOLTIP_HIDEHP"));

    if (options.showHistory) {
        hidehp->set_image (*iHistoryHide);
    } else {
        hidehp->set_image (*iHistoryShow);
    }

    tbTopPanel_1 = nullptr;

    if (!App::get().isSimpleEditor() && filePanel) {
        tbTopPanel_1 = new Gtk::ToggleButton ();
        iTopPanel_1_Show = new RTImage ("panel-to-bottom", Gtk::ICON_SIZE_LARGE_TOOLBAR);
        iTopPanel_1_Hide = new RTImage ("panel-to-top", Gtk::ICON_SIZE_LARGE_TOOLBAR);
        tbTopPanel_1->set_relief (Gtk::RELIEF_NONE);
        tbTopPanel_1->set_active (true);
        tbTopPanel_1->set_tooltip_markup (M ("MAIN_TOOLTIP_SHOWHIDETP1"));
        tbTopPanel_1->set_image (*iTopPanel_1_Hide);
    }

    Gtk::Separator* vsepcl = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vsepz2 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vsepz3 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vsepz4 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));

    Gtk::Separator* vsep1 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    Gtk::Separator* vsep2 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));

    // Histogram profile toggle controls
    toggleHistogramProfile = Gtk::manage (new Gtk::ToggleButton ());
    Gtk::Image* histProfImg = Gtk::manage (new RTImage ("gamut-hist", Gtk::ICON_SIZE_LARGE_TOOLBAR));
    toggleHistogramProfile->add (*histProfImg);
    toggleHistogramProfile->set_relief (Gtk::RELIEF_NONE);
    toggleHistogramProfile->set_active (options.rtSettings.HistogramWorking);
    toggleHistogramProfile->set_tooltip_markup ( (M ("PREFERENCES_HISTOGRAM_TOOLTIP")));

    Gtk::Separator* vsep3 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));

    iareapanel = new ImageAreaPanel ();
    tpc->setEditProvider (iareapanel->imageArea);
    tpc->getToolBar()->setLockablePickerToolListener (iareapanel->imageArea);

    Gtk::Box* toolBarPanel = Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL));
    toolBarPanel->set_name ("EditorTopPanel");
    toolBarPanel->pack_start (*hidehp, Gtk::PACK_SHRINK, 1);
    toolBarPanel->pack_start (*vseph, Gtk::PACK_SHRINK, 2);
    toolBarPanel->pack_start (*info, Gtk::PACK_SHRINK, 1);
    toolBarPanel->pack_start (*beforeAfter, Gtk::PACK_SHRINK, 1);
    toolBarPanel->pack_start (*vsepi, Gtk::PACK_SHRINK, 2);
    toolBarPanel->pack_start (*tpc->getToolBar(), Gtk::PACK_SHRINK, 1);
    toolBarPanel->pack_start (*vsept, Gtk::PACK_SHRINK, 2);

    if (tbTopPanel_1) {
        Gtk::Separator* vsep = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
        toolBarPanel->pack_end   (*tbTopPanel_1, Gtk::PACK_SHRINK, 1);
        toolBarPanel->pack_end   (*vsep, Gtk::PACK_SHRINK, 2);
    }

    toolBarPanel->pack_end   (*tpc->coarse, Gtk::PACK_SHRINK, 2);
    toolBarPanel->pack_end   (*vsepcl, Gtk::PACK_SHRINK, 2);
    // Histogram profile toggle
    toolBarPanel->pack_end (*toggleHistogramProfile, Gtk::PACK_SHRINK, 1);
    toolBarPanel->pack_end (*vsep3, Gtk::PACK_SHRINK, 2);

    toolBarPanel->pack_end   (*iareapanel->imageArea->indClippedPanel, Gtk::PACK_SHRINK, 0);
    toolBarPanel->pack_end   (*vsepz, Gtk::PACK_SHRINK, 2);
    toolBarPanel->pack_end   (*iareapanel->imageArea->previewModePanel, Gtk::PACK_SHRINK, 0);
    toolBarPanel->pack_end   (*vsepz4, Gtk::PACK_SHRINK, 2);

    afterBox = Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_VERTICAL));
    afterBox->pack_start (*iareapanel);

    beforeAfterBox = Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL));
    beforeAfterBox->set_name ("BeforeAfterContainer");
    beforeAfterBox->pack_start (*afterBox);

    MyScrolledToolbar *stb1 = Gtk::manage(new MyScrolledToolbar());
    stb1->set_name("EditorToolbarTop");
    stb1->add(*toolBarPanel);
    editbox->pack_start (*stb1, Gtk::PACK_SHRINK, 2);
    editbox->pack_start (*beforeAfterBox);

    // build right side panel
    vboxright = new Gtk::Paned (Gtk::ORIENTATION_VERTICAL);

    vsubboxright = new Gtk::Box (Gtk::ORIENTATION_VERTICAL, 0);
//    int rightsize = options.fontSize * 44;
//    vsubboxright->set_size_request (rightsize, rightsize - 50);
    vsubboxright->set_size_request (300, 250);

    vsubboxright->pack_start (*ppframe, Gtk::PACK_SHRINK, 2);
    // main notebook
    vsubboxright->pack_start (*tpc->toolPanelNotebook);

    vboxright->pack2 (*vsubboxright, true, true);

    // Save buttons
    Gtk::Grid *iops = Gtk::manage(new Gtk::Grid());
    iops->set_name ("IopsPanel");
    iops->set_orientation (Gtk::ORIENTATION_HORIZONTAL);
    iops->set_row_spacing (2);
    iops->set_column_spacing (2);

    Gtk::Image *saveButtonImage =  Gtk::manage (new RTImage ("save", Gtk::ICON_SIZE_LARGE_TOOLBAR));
    saveimgas = Gtk::manage (new Gtk::Button ());
    saveimgas->set_relief(Gtk::RELIEF_NONE);
    saveimgas->add (*saveButtonImage);
    saveimgas->set_tooltip_markup (M ("MAIN_BUTTON_SAVE_TOOLTIP"));
    setExpandAlignProperties (saveimgas, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

    saveLUTBtn = Gtk::manage (new Gtk::Button ());
    saveLUTBtn->set_relief(Gtk::RELIEF_NONE);
    saveLUTBtn->add (*Gtk::manage(new Gtk::Label("LUT")));
    saveLUTBtn->set_tooltip_markup (M ("MAIN_BUTTON_SAVE_LUT_TOOLTIP"));
    setExpandAlignProperties (saveLUTBtn, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

    Gtk::Image *queueButtonImage = Gtk::manage (new RTImage ("gears", Gtk::ICON_SIZE_LARGE_TOOLBAR));
    queueimg = Gtk::manage (new Gtk::Button ());
    queueimg->set_relief(Gtk::RELIEF_NONE);
    queueimg->add (*queueButtonImage);
    queueimg->set_tooltip_markup (M ("MAIN_BUTTON_PUTTOQUEUE_TOOLTIP"));
    setExpandAlignProperties (queueimg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

    send_to_external = Gtk::manage(new PopUpButton("", false));
    send_to_external->set_tooltip_text(M("MAIN_BUTTON_SENDTOEDITOR_TOOLTIP"));
    send_to_external->setEmptyImage("palette-brush");
    setExpandAlignProperties(send_to_external->buttonGroup, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    updateExternalEditorWidget(
        options.externalEditorIndex >= 0 ? options.externalEditorIndex : options.externalEditors.size(),
        options.externalEditors
    );
    send_to_external->show();

    // Status box
    progressLabel = Gtk::manage (new MyProgressBar (300));
    progressLabel->set_show_text (true);
    setExpandAlignProperties (progressLabel, true, false, Gtk::ALIGN_START, Gtk::ALIGN_FILL);
    progressLabel->set_fraction (0.0);

    // tbRightPanel_1
    tbRightPanel_1 = Gtk::manage(new Gtk::ToggleButton());
    iRightPanel_1_Show = new RTImage ("panel-to-left", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    iRightPanel_1_Hide = new RTImage ("panel-to-right", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    tbRightPanel_1->set_relief (Gtk::RELIEF_NONE);
    tbRightPanel_1->set_active (true);
    tbRightPanel_1->set_tooltip_markup (M ("MAIN_TOOLTIP_SHOWHIDERP1"));
    tbRightPanel_1->set_image (*iRightPanel_1_Hide);
    setExpandAlignProperties (tbRightPanel_1, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

    // ShowHideSidePanels
    tbShowHideSidePanels = Gtk::manage(new Gtk::ToggleButton());
    iShowHideSidePanels = new RTImage ("crossed-arrows-out", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    iShowHideSidePanels_exit = new RTImage ("crossed-arrows-in", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    tbShowHideSidePanels->set_relief (Gtk::RELIEF_NONE);
    tbShowHideSidePanels->set_active (false);
    tbShowHideSidePanels->set_tooltip_markup (M ("MAIN_BUTTON_SHOWHIDESIDEPANELS_TOOLTIP"));
    tbShowHideSidePanels->set_image (*iShowHideSidePanels);
    setExpandAlignProperties (tbShowHideSidePanels, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

    navPrev = navNext = navSync = nullptr;

    if (!App::get().isSimpleEditor() && !options.tabbedUI) {
        // Navigation buttons
        Gtk::Image *navPrevImage = Gtk::manage (new RTImage ("arrow2-left", Gtk::ICON_SIZE_LARGE_TOOLBAR));
        navPrevImage->set_padding (0, 0);
        navPrev = Gtk::manage (new Gtk::Button ());
        navPrev->add (*navPrevImage);
        navPrev->set_relief (Gtk::RELIEF_NONE);
        navPrev->set_tooltip_markup (M ("MAIN_BUTTON_NAVPREV_TOOLTIP"));
        setExpandAlignProperties (navPrev, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

        Gtk::Image *navNextImage = Gtk::manage (new RTImage ("arrow2-right", Gtk::ICON_SIZE_LARGE_TOOLBAR));
        navNextImage->set_padding (0, 0);
        navNext = Gtk::manage (new Gtk::Button ());
        navNext->add (*navNextImage);
        navNext->set_relief (Gtk::RELIEF_NONE);
        navNext->set_tooltip_markup (M ("MAIN_BUTTON_NAVNEXT_TOOLTIP"));
        setExpandAlignProperties (navNext, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);

        Gtk::Image *navSyncImage = Gtk::manage (new RTImage ("arrow-updown", Gtk::ICON_SIZE_LARGE_TOOLBAR));
        navSyncImage->set_padding (0, 0);
        navSync = Gtk::manage (new Gtk::Button ());
        navSync->add (*navSyncImage);
        navSync->set_relief (Gtk::RELIEF_NONE);
        navSync->set_tooltip_markup (M ("MAIN_BUTTON_NAVSYNC_TOOLTIP"));
        setExpandAlignProperties (navSync, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    }

    // ==================  PACKING THE BOTTOM WIDGETS =================

    // Adding widgets from center to the left, on the left side (using Gtk::POS_LEFT)
    iops->attach_next_to (*vsep2, Gtk::POS_LEFT, 1, 1);
    iops->attach_next_to (*progressLabel, Gtk::POS_LEFT, 1, 1);
    iops->attach_next_to (*vsep1, Gtk::POS_LEFT, 1, 1);

    if (!App::get().isGimpPlugin()) {
        iops->attach_next_to(*send_to_external->buttonGroup, Gtk::POS_LEFT, 1, 1);
    }

    if (!App::get().isGimpPlugin() && !App::get().isSimpleEditor()) {
        iops->attach_next_to (*queueimg, Gtk::POS_LEFT, 1, 1);
    }

    if (!App::get().isGimpPlugin()) {
        iops->attach_next_to (*saveLUTBtn, Gtk::POS_LEFT, 1, 1);
    }

    if (!App::get().isGimpPlugin()) {
        iops->attach_next_to (*saveimgas, Gtk::POS_LEFT, 1, 1);
    }

    // Color management toolbar
    colorMgmtToolBar.reset (new ColorManagementToolbar (ipc));
    colorMgmtToolBar->pack_right_in (iops);

    if (!App::get().isSimpleEditor() && !options.tabbedUI) {
        Gtk::Separator* vsep3 = Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
        iops->attach_next_to (*vsep3, Gtk::POS_RIGHT, 1, 1);
        iops->attach_next_to (*navPrev, Gtk::POS_RIGHT, 1, 1);
        iops->attach_next_to (*navSync, Gtk::POS_RIGHT, 1, 1);
        iops->attach_next_to (*navNext, Gtk::POS_RIGHT, 1, 1);
    }

    iops->attach_next_to (*vsepz2, Gtk::POS_RIGHT, 1, 1);
    iops->attach_next_to (*iareapanel->imageArea->zoomPanel, Gtk::POS_RIGHT, 1, 1);
    iops->attach_next_to (*vsepz3, Gtk::POS_RIGHT, 1, 1);
    iops->attach_next_to (*tbShowHideSidePanels, Gtk::POS_RIGHT, 1, 1);
    iops->attach_next_to (*tbRightPanel_1, Gtk::POS_RIGHT, 1, 1);

    MyScrolledToolbar *stb2 = Gtk::manage(new MyScrolledToolbar());
    stb2->set_name("EditorToolbarBottom");
    stb2->add(*iops);

    editbox->pack_start (*stb2, Gtk::PACK_SHRINK, 0);
    editbox->show_all ();

    // build screen
    hpanedl = Gtk::manage (new Gtk::Paned (Gtk::ORIENTATION_HORIZONTAL));
    hpanedl->set_name ("EditorLeftPaned");
    hpanedr = Gtk::manage (new Gtk::Paned (Gtk::ORIENTATION_HORIZONTAL));
    hpanedr->set_name ("EditorRightPaned");
    leftbox->reference ();
    vboxright->reference ();
    vboxright->set_name ("EditorModules");

    if (options.showHistory) {
        hpanedl->pack1 (*leftbox, false, false);
        hpanedl->set_position (options.historyPanelWidth);
    }

    Gtk::Paned *viewpaned = Gtk::manage (new Gtk::Paned (Gtk::ORIENTATION_VERTICAL));
    fPanel = filePanel;

    if (filePanel) {
        catalogPane = new Gtk::Paned();
        viewpaned->pack1 (*catalogPane, false, true);
    }

    viewpaned->pack2 (*editbox, true, true);

    hpanedl->pack2 (*viewpaned, true, true);

    hpanedr->pack1 (*hpanedl, true, false);
    hpanedr->pack2 (*vboxright, false, false);
    hpanedl->signal_button_release_event().connect_notify ( sigc::mem_fun (*this, &EditorPanel::leftPaneButtonReleased) );
    hpanedr->signal_button_release_event().connect_notify ( sigc::mem_fun (*this, &EditorPanel::rightPaneButtonReleased) );

    pack_start (*hpanedr);

    updateHistogramPosition (0, options.histogramPosition);

    show_all ();
    /*
        // save as dialog
        if (Glib::file_test (options.lastSaveAsPath, Glib::FILE_TEST_IS_DIR))
            saveAsDialog = new SaveAsDialog (options.lastSaveAsPath);
        else
            saveAsDialog = new SaveAsDialog (safe_get_user_picture_dir());

        saveAsDialog->set_default_size (options.saveAsDialogWidth, options.saveAsDialogHeight);
    */
    // connect listeners
    profilep->setProfileChangeListener (tpc);
    history->setProfileChangeListener (tpc);
    history->setHistoryBeforeLineListener (this);
    tpc->addPParamsChangeListener (profilep);
    tpc->addPParamsChangeListener (history);
    tpc->addPParamsChangeListener (this);
    iareapanel->imageArea->setCropGUIListener (tpc->getCropGUIListener());
    iareapanel->imageArea->setPointerMotionListener (navigator);
    iareapanel->imageArea->setImageAreaToolListener (tpc);

    // initialize components
    info->set_active (options.showInfo);
    tpc->readOptions ();

    // connect event handlers
    info->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::info_toggled) );
    beforeAfter->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::beforeAfterToggled) );
    hidehp->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::hideHistoryActivated) );
    tbRightPanel_1->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::tbRightPanel_1_toggled) );
    saveimgas->signal_pressed().connect ( sigc::mem_fun (*this, &EditorPanel::saveAsPressed) );
    saveLUTBtn->signal_pressed().connect ( sigc::mem_fun (*this, &EditorPanel::saveLUTPressed) );
    queueimg->signal_pressed().connect ( sigc::mem_fun (*this, &EditorPanel::queueImgPressed) );
    send_to_external->signal_changed().connect(sigc::mem_fun(*this, &EditorPanel::sendToExternalChanged));
    send_to_external->signal_pressed().connect(sigc::mem_fun(*this, &EditorPanel::sendToExternalPressed));
    toggleHistogramProfile->signal_toggled().connect( sigc::mem_fun (*this, &EditorPanel::histogramProfile_toggled) );

    if (navPrev) {
        navPrev->signal_pressed().connect ( sigc::mem_fun (*this, &EditorPanel::openPreviousEditorImage) );
    }

    if (navNext) {
        navNext->signal_pressed().connect ( sigc::mem_fun (*this, &EditorPanel::openNextEditorImage) );
    }

    if (navSync) {
        navSync->signal_pressed().connect ( sigc::mem_fun (*this, &EditorPanel::syncFileBrowser) );
    }

    ShowHideSidePanelsconn = tbShowHideSidePanels->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::toggleSidePanels), true);

    if (tbTopPanel_1) {
        tbTopPanel_1->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::tbTopPanel_1_toggled) );
    }

}

EditorPanel::~EditorPanel ()
{
    idle_register.destroy();

    history->setHistoryBeforeLineListener (nullptr);
    // the order is important!
    iareapanel->setBeforeAfterViews (nullptr, iareapanel);
    delete iareapanel;
    iareapanel = nullptr;

    if (beforeIpc) {
        beforeIpc->stopProcessing ();
    }

    delete beforeIarea;
    beforeIarea = nullptr;

    if (beforeIpc) {
        beforeIpc->setPreviewImageListener (nullptr);
    }

    delete beforePreviewHandler;
    beforePreviewHandler = nullptr;

    if (beforeIpc) {
        rtengine::StagedImageProcessor::destroy (beforeIpc);
    }

    beforeIpc = nullptr;

    close ();

    if (epih->pending) {
        epih->destroyed = true;
    } else {
        delete epih;
    }

    delete tpc;

    delete leftsubpaned;
    delete leftbox;
    delete vsubboxright;
    delete vboxright;

    //delete saveAsDialog;
    delete catalogPane;
    delete iTopPanel_1_Show;
    delete iTopPanel_1_Hide;
    delete iHistoryShow;
    delete iHistoryHide;
    delete iBeforeLockON;
    delete iBeforeLockOFF;
    delete iRightPanel_1_Show;
    delete iRightPanel_1_Hide;
    delete iShowHideSidePanels_exit;
    delete iShowHideSidePanels;
}

void EditorPanel::leftPaneButtonReleased (GdkEventButton *event)
{
    if (event->button == 1) {
        // Button 1 released : it's a resize
        App::get().mut_options().historyPanelWidth = hpanedl->get_position();
    }

    /*else if (event->button == 3) {
    }*/
}

void EditorPanel::rightPaneButtonReleased (GdkEventButton *event)
{
    if (event->button == 1) {
        int winW, winH;
        parentWindow->get_size (winW, winH);
        // Button 1 released : it's a resize
        App::get().mut_options().toolPanelWidth = winW - hpanedr->get_position();
    }

    /*else if (event->button == 3) {
    }*/
}

void EditorPanel::writeOptions()
{
    if (profilep) {
        profilep->writeOptions();
    }

    if (tpc) {
        tpc->writeOptions();
    }
}


void EditorPanel::writeToolExpandedStatus (std::vector<int> &tpOpen)
{
    if (tpc) {
        tpc->writeToolExpandedStatus (tpOpen);
    }
}

void EditorPanel::updateShowtooltipVisibility (bool showtooltip)
{
    if (tpc) {
        tpc->updateShowtooltipVisibility (showtooltip);
    }
}

void EditorPanel::showTopPanel (bool show)
{
    if (tbTopPanel_1->get_active() != show) {
        tbTopPanel_1->set_active (show);
    }
}

void EditorPanel::setAspect ()
{
    const auto& options = App::get().options();
    int winW, winH;
    parentWindow->get_size (winW, winH);
    hpanedl->set_position (options.historyPanelWidth);
    hpanedr->set_position (winW - options.toolPanelWidth);

    // initialize components
    if (info->get_active() != options.showInfo) {
        info->set_active (options.showInfo);
    }
}

void EditorPanel::on_realize ()
{
    realized = true;
    Gtk::Box::on_realize ();
    // This line is needed to avoid autoexpansion of the window :-/
    //vboxright->set_size_request (options.toolPanelWidth, -1);
    tpc->updateToolState();
}

void EditorPanel::open (Thumbnail* tmb, rtengine::InitialImage* isrc)
{

    close();

    isProcessing = true; // prevents closing-on-init

    // initialize everything
    openThm = tmb;

    fname = openThm->getFileName();
    if (fPanel && fPanel->fileCatalog) {
        fPanel->fileCatalog->saveResetState();
    }
    lastSaveAsFileName = removeExtension (Glib::path_get_basename (fname));

    previewHandler = new PreviewHandler ();

    this->isrc = isrc;
    ipc = rtengine::StagedImageProcessor::create (isrc);
    ipc->setProgressListener (this);
    colorMgmtToolBar->updateProcessor();
    ipc->setPreviewImageListener (previewHandler);
    ipc->setPreviewScale (10);  // Important
    tpc->initImage (ipc, tmb->getType() == FT_Raw);
    ipc->setHistogramListener (this);
    iareapanel->imageArea->indClippedPanel->silentlyDisableSharpMask();

//    iarea->fitZoom ();   // tell to the editorPanel that the next image has to be fitted to the screen
    iareapanel->imageArea->setPreviewHandler (previewHandler);
    iareapanel->imageArea->setImProcCoordinator (ipc);
    navigator->previewWindow->setPreviewHandler (previewHandler);
    navigator->previewWindow->setImageArea (iareapanel->imageArea);

    rtengine::ImageSource* is = isrc->getImageSource();
    is->setProgressListener ( this );

    // try to load the last saved parameters from the cache or from the paramfile file
    ProcParams* ldprof = openThm->createProcParamsForUpdate (true, false); // will be freed by initProfile

    const auto& options = App::get().options();
    // initialize profile
    Glib::ustring defProf = openThm->getType() == FT_Raw ? options.defProfRaw : options.defProfImg;
    profilep->initProfile (defProf, ldprof);
    profilep->setInitialFileName (fname);

    openThm->addThumbnailListener (this);
    info_toggled ();

    if (beforeIarea) {
        beforeAfterToggled();
        beforeAfterToggled();
    }

    // If in single tab mode, the main crop window is not constructed the very first time
    // since there was no resize event
    if (iareapanel->imageArea->mainCropWindow) {
        iareapanel->imageArea->mainCropWindow->cropHandler.newImage (ipc, false);
    } else {
        Gtk::Allocation alloc;
        iareapanel->imageArea->on_resized (alloc);

        // When passing a photo as an argument to the RawTherapee executable, the user wants
        // this auto-loaded photo's thumbnail to be selected and visible in the Filmstrip.
        EditorPanel::syncFileBrowser();
    }

    history->resetSnapShotNumber();
    navigator->setInvalid(ipc->getFullWidth(),ipc->getFullHeight());
}

void EditorPanel::close ()
{
    if (ipc) {
        saveProfile ();
        // close image processor and the current thumbnail
        tpc->closeImage ();    // this call stops image processing
        tpc->writeOptions ();
        rtengine::ImageSource* is = isrc->getImageSource();
        is->setProgressListener ( nullptr );

        if (ipc) {
            ipc->setPreviewImageListener (nullptr);
        }

        if (beforeIpc) {
            beforeIpc->setPreviewImageListener (nullptr);
        }

        delete previewHandler;
        previewHandler = nullptr;

        if (iareapanel) {
            iareapanel->imageArea->setPreviewHandler (nullptr);
            iareapanel->imageArea->setImProcCoordinator (nullptr);
            tpc->editModeSwitchedOff();
        }

        rtengine::StagedImageProcessor::destroy (ipc);
        ipc = nullptr;
        navigator->previewWindow->setPreviewHandler (nullptr);

        // If the file was deleted somewhere, the openThm.descreaseRef delete the object, but we don't know here
        if (Glib::file_test (fname, Glib::FILE_TEST_EXISTS)) {
            openThm->removeThumbnailListener (this);
            openThm->decreaseRef ();
        }
    }
}

void EditorPanel::saveProfile ()
{
    if (!ipc || !openThm) {
        return;
    }

    // If the file was deleted, do not generate ghost entries
    if (Glib::file_test (fname, Glib::FILE_TEST_EXISTS)) {
        ProcParams params;
        ipc->getParams (&params);

        // Will call updateCache, which will update both the cached and sidecar files if necessary
        openThm->setProcParams (params, nullptr, EDITOR);
    }
}

Glib::ustring EditorPanel::getShortName ()
{
    if (openThm) {
        return Glib::path_get_basename (openThm->getFileName ());
    } else {
        return "";
    }
}

Glib::ustring EditorPanel::getFileName () const
{
    if (openThm) {
        return openThm->getFileName ();
    } else {
        return "";
    }
}

// TODO!!!
void EditorPanel::procParamsChanged(
    const rtengine::procparams::ProcParams* params,
    const rtengine::ProcEvent& ev,
    const Glib::ustring& descr,
    const ParamsEdited* paramsEdited
)
{

//    if (ev!=EvPhotoLoaded)
//        saveLabel->set_markup (Glib::ustring("<span foreground=\"#AA0000\" weight=\"bold\">") + M("MAIN_BUTTON_SAVE") + "</span>");

    rtengine::eSensorType sensorType = isrc->getImageSource()->getSensorType();

    selectedFrame = 0;
    if (sensorType == rtengine::ST_BAYER) {
        selectedFrame = params->raw.bayersensor.imageNum;
    //} else if (sensorType == rtengine::ST_FUJI_XTRANS) {
    //    selectedFrame = params->raw.xtranssensor.imageNum;
    }
    selectedFrame = rtengine::LIM<int>(selectedFrame, 0, isrc->getImageSource()->getMetaData()->getFrameCount() - 1);

    info_toggled();
}

void EditorPanel::clearParamChanges()
{
}

void EditorPanel::setProgress(double p)
{
    MyProgressBar* const pl = progressLabel;

    idle_register.add(
        [p, pl]() -> bool
        {
            setprogressStrUI(p, {}, pl);
            return false;
        }
    );
}

void EditorPanel::setProgressStr(const Glib::ustring& str)
{
    MyProgressBar* const pl = progressLabel;

    idle_register.add(
        [str, pl]() -> bool
        {
            setprogressStrUI(-1.0, str, pl);
            return false;
        }
    );
}

void EditorPanel::setProgressState(bool inProcessing)
{
    epih->pending++;

    idle_register.add(
        [this, inProcessing]() -> bool
        {
            if (epih->destroyed)
            {
                if (epih->pending == 1) {
                    delete epih;
                } else {
                    --epih->pending;
                }

                return false;
            }

            epih->epanel->refreshProcessingState(inProcessing);
            --epih->pending;

            return false;
        }
    );
}

void EditorPanel::error(const Glib::ustring& descr)
{
    parent->error(descr);
}

void EditorPanel::error(const Glib::ustring& title, const Glib::ustring& descr)
{
    epih->pending++;

    idle_register.add(
        [this, descr, title]() -> bool
        {
            if (epih->destroyed) {
                if (epih->pending == 1) {
                    delete epih;
                } else {
                    --epih->pending;
                }

                return false;
            }

            epih->epanel->displayError(title, descr);
            --epih->pending;

            return false;
        }
    );
}

void EditorPanel::displayError(const Glib::ustring& title, const Glib::ustring& descr)
{
    GtkWidget* msgd = gtk_message_dialog_new_with_markup (nullptr,
                      GTK_DIALOG_DESTROY_WITH_PARENT,
                      GTK_MESSAGE_ERROR,
                      GTK_BUTTONS_OK,
                      "<b>%s</b>",
                      descr.data());
    gtk_window_set_title ((GtkWindow*)msgd, title.data());
    g_signal_connect_swapped (msgd, "response",
                              G_CALLBACK (gtk_widget_destroy),
                              msgd);
    gtk_widget_show_all (msgd);
}

// This is only called from the ThreadUI, so within the gtk thread
void EditorPanel::refreshProcessingState (bool inProcessingP)
{
    double val;
    Glib::ustring str;

    if (inProcessingP) {
        if (processingStartedTime == 0) {
            processingStartedTime = ::time (nullptr);
        }

        val = 1.0;
        str = "PROGRESSBAR_PROCESSING";
    } else {
        // Set proc params of thumbnail. It saves it into the cache and updates the file browser.
        if (ipc && openThm && tpc->getChangedState()) {
            rtengine::procparams::ProcParams pparams;
            ipc->getParams (&pparams);
            openThm->setProcParams (pparams, nullptr, EDITOR, false);
        }

        // Ring a sound if it was a long event
        if (processingStartedTime != 0) {
            time_t curTime = ::time (nullptr);

            const auto& options = App::get().options();
            if (::difftime (curTime, processingStartedTime) > options.sndLngEditProcDoneSecs) {
                SoundManager::playSoundAsync (options.sndLngEditProcDone);
            }

            processingStartedTime = 0;
        }

        // Set progress bar "done"
        val = 0.0;
        str = "PROGRESSBAR_READY";

#ifdef _WIN32

        // Maybe accessing "parent", which is a Gtk object, can justify to get the Gtk lock...
        if (!firstProcessingDone && static_cast<RTWindow*> (parent)->getIsFullscreen()) {
            parent->fullscreen();
        }

#endif
        firstProcessingDone = true;
    }

    isProcessing = inProcessingP;

    setprogressStrUI(val, str, progressLabel);
}

void EditorPanel::info_toggled ()
{

    Glib::ustring infoString;
    Glib::ustring expcomp;

    if (!ipc || !openThm) {
        return;
    }

    const rtengine::FramesMetaData* idata = ipc->getInitialImage()->getMetaData();

    if (idata && idata->hasExif()) {
        infoString = Glib::ustring::compose ("%1 + %2\n<span size=\"small\">f/</span><span size=\"large\">%3</span>  <span size=\"large\">%4</span><span size=\"small\">s</span>  <span size=\"small\">%5</span><span size=\"large\">%6</span>  <span size=\"large\">%7</span><span size=\"small\">mm</span>",
                                              escapeHtmlChars (idata->getMake() + " " + idata->getModel()),
                                              escapeHtmlChars (idata->getLens()),
                                              Glib::ustring (idata->apertureToString (idata->getFNumber())),
                                              Glib::ustring (idata->shutterToString (idata->getShutterSpeed())),
                                              M ("QINFO_ISO"), idata->getISOSpeed(),
                                              Glib::ustring::format (std::setw (3), std::fixed, std::setprecision (2), idata->getFocalLen()));

        expcomp = Glib::ustring (idata->expcompToString (idata->getExpComp(), true)); // maskZeroexpcomp

        if (!expcomp.empty ()) {
            infoString = Glib::ustring::compose ("%1  <span size=\"large\">%2</span><span size=\"small\">EV</span>",
                                                  infoString,
                                                  expcomp /*Glib::ustring(idata->expcompToString(idata->getExpComp()))*/);
        }

        infoString = Glib::ustring::compose ("%1\n<span size=\"small\">%2</span><span>%3</span>",
                                              infoString,
                                              escapeHtmlChars (Glib::path_get_dirname (openThm->getFileName())) + G_DIR_SEPARATOR_S,
                                              escapeHtmlChars (Glib::path_get_basename (openThm->getFileName()))  );

        int ww = -1, hh = -1;
        idata->getDimensions(ww, hh);
        if (ww <= 0) {
            ww = ipc->getFullWidth();
            hh = ipc->getFullHeight();
        }

        //megapixels
        infoString = Glib::ustring::compose ("%1\n<span size=\"small\">%2 MP (%3x%4)</span>",
                                             infoString,
                                             Glib::ustring::format (std::setw (4), std::fixed, std::setprecision (1), (float)ww * hh / 1000000),
                                             ww, hh);

        //adding special characteristics
        bool isHDR = idata->getHDR();
        bool isPixelShift = idata->getPixelShift();
        unsigned int numFrames = idata->getFrameCount();
        if (isHDR) {
            infoString = Glib::ustring::compose ("%1\n" + M("QINFO_HDR"), infoString, numFrames);
            if (numFrames == 1) {
                int sampleFormat = idata->getSampleFormat();
                infoString = Glib::ustring::compose ("%1 / %2", infoString, M(Glib::ustring::compose("SAMPLEFORMAT_%1", sampleFormat)));
            }
        } else if (isPixelShift) {
            infoString = Glib::ustring::compose ("%1\n" + M("QINFO_PIXELSHIFT"), infoString, numFrames);
        } else if (numFrames > 1) {
            infoString = Glib::ustring::compose ("%1\n" + M("QINFO_FRAMECOUNT"), infoString, numFrames);
        }
    } else {
        infoString = M ("QINFO_NOEXIF");
    }

    iareapanel->imageArea->setInfoText (std::move(infoString));
    iareapanel->imageArea->infoEnabled (info->get_active ());
}

void EditorPanel::hideHistoryActivated ()
{

    removeIfThere (hpanedl, leftbox, false);

    if (hidehp->get_active()) {
        hpanedl->pack1 (*leftbox, false, false);
    }

    auto& options = App::get().mut_options();
    options.showHistory = hidehp->get_active();

    if (options.showHistory) {
        hidehp->set_image (*iHistoryHide);
    } else {
        hidehp->set_image (*iHistoryShow);
    }

    tbShowHideSidePanels_managestate();
}


void EditorPanel::tbRightPanel_1_toggled ()
{
    /*
        removeIfThere (hpanedr, vboxright, false);
        if (tbRightPanel_1->get_active()){
            hpanedr->pack2(*vboxright, false, true);
            tbRightPanel_1->set_image (*iRightPanel_1_Hide);
        }
        else {
            tbRightPanel_1->set_image (*iRightPanel_1_Show);
        }
        tbShowHideSidePanels_managestate();
        */
    if (vboxright) {
        if (tbRightPanel_1->get_active()) {
            vboxright->show();
            tbRightPanel_1->set_image (*iRightPanel_1_Hide);
        } else {
            vboxright->hide();
            tbRightPanel_1->set_image (*iRightPanel_1_Show);
        }

        tbShowHideSidePanels_managestate();
    }
}

void EditorPanel::tbTopPanel_1_visible (bool visible)
{
    if (!tbTopPanel_1) {
        return;
    }

    if (visible) {
        tbTopPanel_1->show();
    } else {
        tbTopPanel_1->hide();
    }
}

void EditorPanel::tbTopPanel_1_toggled ()
{

    if (catalogPane) { // catalogPane does not exist in multitab mode

        auto& options = App::get().mut_options();
        if (tbTopPanel_1->get_active()) {
            catalogPane->show();
            tbTopPanel_1->set_image (*iTopPanel_1_Hide);
            options.editorFilmStripOpened = true;
        } else {
            catalogPane->hide();
            tbTopPanel_1->set_image (*iTopPanel_1_Show);
            options.editorFilmStripOpened = false;
        }

        tbShowHideSidePanels_managestate();
    }
}

/*
 * WARNING: Take care of the simpleEditor value when adding or modifying shortcut keys,
 *          since handleShortcutKey is now also triggered in simple editor mode
 */
bool EditorPanel::handleShortcutKey (GdkEventKey* event)
{

    bool ctrl = event->state & GDK_CONTROL_MASK;
    bool shift = event->state & GDK_SHIFT_MASK;
    bool alt = event->state & GDK_MOD1_MASK;
#ifdef __WIN32__
    bool altgr = event->state & GDK_MOD2_MASK;
#else
    bool altgr = event->state & GDK_MOD5_MASK;
#endif

    // Editor Layout
    switch (event->keyval) {
        case GDK_KEY_L:
            if (tbTopPanel_1) {
                tbTopPanel_1->set_active (!tbTopPanel_1->get_active());    // toggle top panel
            }

            if (ctrl) {
                hidehp->set_active (!hidehp->get_active());    // toggle History (left panel)
            }

            if (alt) {
                tbRightPanel_1->set_active (!tbRightPanel_1->get_active());    // toggle right panel
            }

            return true;
            break;

        case GDK_KEY_l:
            if (!shift && !alt /*&& !ctrl*/) {
                hidehp->set_active (!hidehp->get_active()); // toggle History (left panel)
                return true;
            }

            if (alt && !ctrl) { // toggle right panel
                tbRightPanel_1->set_active (!tbRightPanel_1->get_active());
                return true;
            }

            if (alt && ctrl) { // toggle left and right panels
                hidehp->set_active (!hidehp->get_active());
                tbRightPanel_1->set_active (!tbRightPanel_1->get_active());
                return true;
            }

            break;

        case GDK_KEY_m: // Maximize preview panel: hide top AND right AND history panels
            if (!ctrl && !alt) {
                toggleSidePanels();
                return true;
            }

            break;

        case GDK_KEY_M: // Maximize preview panel: hide top AND right AND history panels AND (fit image preview)
            if (!ctrl && !alt) {
                toggleSidePanelsZoomFit();
                return true;
            }

            break;
    }

#ifdef __WIN32__

    if (!alt && !ctrl && !altgr && event->hardware_keycode == 0x39 ) {
        iareapanel->imageArea->previewModePanel->togglebackColor();
        return true;
    }

#else

    if (!alt && !ctrl && !altgr && event->hardware_keycode == 0x12 ) {
        iareapanel->imageArea->previewModePanel->togglebackColor();
        return true;
    }

#endif

    if (!alt) {
        if (!ctrl) {
            // Normal
            switch (event->keyval) {
                case GDK_KEY_bracketright:
                    tpc->coarse->rotateRight();
                    return true;

                case GDK_KEY_bracketleft:
                    tpc->coarse->rotateLeft();
                    return true;

                case GDK_KEY_i:
                case GDK_KEY_I:
                    info->set_active (!info->get_active());
                    return true;

                case GDK_KEY_B:
                    beforeAfter->set_active (!beforeAfter->get_active());
                    return true;

                case GDK_KEY_plus:
                case GDK_KEY_equal:
                case GDK_KEY_KP_Add:
                    iareapanel->imageArea->zoomPanel->zoomInClicked();
                    return true;

                case GDK_KEY_minus:
                case GDK_KEY_underscore:
                case GDK_KEY_KP_Subtract:
                    iareapanel->imageArea->zoomPanel->zoomOutClicked();
                    return true;

                case GDK_KEY_z://GDK_1
                    iareapanel->imageArea->zoomPanel->zoom11Clicked();
                    return true;

                /*
                #ifndef __WIN32__
                                case GDK_KEY_9: // toggle background color of the preview
                                    iareapanel->imageArea->previewModePanel->togglebackColor();
                                    return true;
                #endif
                */
                case GDK_KEY_r: //preview mode Red
                    iareapanel->imageArea->previewModePanel->toggleR();
                    return true;

                case GDK_KEY_g: //preview mode Green
                    iareapanel->imageArea->previewModePanel->toggleG();
                    return true;

                case GDK_KEY_b: //preview mode Blue
                    iareapanel->imageArea->previewModePanel->toggleB();
                    return true;

                case GDK_KEY_p: //preview mode Sharpening Contrast mask
                    iareapanel->imageArea->indClippedPanel->toggleSharpMask();
                    return true;

                case GDK_KEY_v: //preview mode Luminosity
                    iareapanel->imageArea->previewModePanel->toggleL();
                    return true;

                case GDK_KEY_F: //preview mode Focus Mask
                    iareapanel->imageArea->indClippedPanel->toggleFocusMask();
                    return true;

                case GDK_KEY_less:
                    iareapanel->imageArea->indClippedPanel->toggleClipped (false);
                    return true;

                case GDK_KEY_greater:
                    iareapanel->imageArea->indClippedPanel->toggleClipped (true);
                    return true;

                case GDK_KEY_f:
                    iareapanel->imageArea->zoomPanel->zoomFitCropClicked();
                    return true;

                case GDK_KEY_F5:
                    openThm->openDefaultViewer ((event->state & GDK_SHIFT_MASK) ? 2 : 1);
                    return true;

                case GDK_KEY_y: // synchronize filebrowser with image in Editor
                    if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
                        fPanel->fileCatalog->selectImage (fname, false);
                        return true;
                    }

                    break; // to avoid gcc complain

                case GDK_KEY_x: // clear filters and synchronize filebrowser with image in Editor
                    if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
                        fPanel->fileCatalog->selectImage (fname, true);
                        return true;
                    }

                    break; // to avoid gcc complain
            }
        } else {
            // With control
            switch (event->keyval) {
                case GDK_KEY_S:
                    saveProfile();
                    setProgressStr (M ("PROGRESSBAR_PROCESSING_PROFILESAVED"));
                    return true;

                case GDK_KEY_s:
                    if (!App::get().isGimpPlugin()) {
                        saveAsPressed();
                    }

                    return true;

                case GDK_KEY_b:
                    if (!App::get().isGimpPlugin() && !App::get().isSimpleEditor()) {
                        queueImgPressed();
                    }

                    return true;

                case GDK_KEY_e:
                    if (!App::get().isGimpPlugin()) {
                        sendToExternalPressed();
                    }

                    return true;

                case GDK_KEY_z:
                    history->undo ();
                    return true;

                case GDK_KEY_Z:
                    history->redo ();
                    return true;

                case GDK_KEY_F5:
                    openThm->openDefaultViewer (3);
                    return true;

                case GDK_KEY_f:
                case GDK_KEY_F:
                    // No action is performed to avoid Gtk-CRITICAL due to Locallab treeview when treeview isn't focused
                    return true;
            }
        } //if (!ctrl)
    } //if (!alt)

    if (alt) {
        switch (event->keyval) {
            case GDK_KEY_s:
                history->addBookmarkPressed ();
                setProgressStr (M ("PROGRESSBAR_SNAPSHOT_ADDED"));
                return true;

            case GDK_KEY_f:
                iareapanel->imageArea->zoomPanel->zoomFitClicked();
                return true;
        }
    }

    if (shift) {
        switch (event->keyval) {
            case GDK_KEY_F3: // open Previous image from Editor's perspective
                if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
                    EditorPanel::openPreviousEditorImage();
                    return true;
                }

                break; // to avoid gcc complain

            case GDK_KEY_F4: // open next image from Editor's perspective
                if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
                    EditorPanel::openNextEditorImage();
                    return true;
                }

                break; // to avoid gcc complain
        }
    }

    if (tpc->getToolBar() && tpc->getToolBar()->handleShortcutKey (event)) {
        return true;
    }

    if (tpc->handleShortcutKey (event)) {
        return true;
    }

    if (!App::get().isSimpleEditor() && fPanel) {
        if (fPanel->handleShortcutKey (event)) {
            return true;
        }
    }

    return false;
}

void EditorPanel::procParamsChanged (Thumbnail* thm, int whoChangedIt, bool upgradeHint)
{

    if (whoChangedIt != EDITOR) {
        PartialProfile pp (true);
        pp.set (true);
        * (pp.pparams) = openThm->getProcParams();
        pp.pedited->locallab.spots.resize(pp.pparams->locallab.spots.size(), LocallabParamsEdited::LocallabSpotEdited(true));
        tpc->profileChange (&pp, rtengine::EvProfileChangeNotification, M ("PROGRESSDLG_PROFILECHANGEDINBROWSER"));
        pp.deleteInstance();
    }
}

bool EditorPanel::idle_saveImage (ProgressConnector<rtengine::IImagefloat*> *pc, Glib::ustring fname, SaveFormat sf, rtengine::procparams::ProcParams &pparams)
{
    rtengine::IImagefloat* img = pc->returnValue();
    delete pc;

    if ( img ) {
        setProgressStr (M ("GENERAL_SAVE"));
        setProgress (0.9f);

        ProgressConnector<int> *ld = new ProgressConnector<int>();
        img->setSaveProgressListener (parent->getProgressListener());

        if (sf.format == "tif")
            ld->startFunc (sigc::bind (sigc::mem_fun (img, &rtengine::IImagefloat::saveAsTIFF), fname, sf.tiffBits, sf.tiffFloat, sf.tiffUncompressed, sf.bigTiff),
                           sigc::bind (sigc::mem_fun (*this, &EditorPanel::idle_imageSaved), ld, img, fname, sf, pparams));
        else if (sf.format == "png")
            ld->startFunc (sigc::bind (sigc::mem_fun (img, &rtengine::IImagefloat::saveAsPNG), fname, sf.pngBits),
                           sigc::bind (sigc::mem_fun (*this, &EditorPanel::idle_imageSaved), ld, img, fname, sf, pparams));
        else if (sf.format == "jpg")
            ld->startFunc (sigc::bind (sigc::mem_fun (img, &rtengine::IImagefloat::saveAsJPEG), fname, sf.jpegQuality, sf.jpegSubSamp),
                           sigc::bind (sigc::mem_fun (*this, &EditorPanel::idle_imageSaved), ld, img, fname, sf, pparams));
        else {
            delete ld;
        }
    } else {
        Glib::ustring msg_ = Glib::ustring ("<b>") + escapeHtmlChars(fname) + ": Error during image processing\n</b>";
        Gtk::MessageDialog msgd (*parent, msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        msgd.run ();

        saveimgas->set_sensitive (true);
        send_to_external->set_sensitive(send_to_external->getEntryCount());
        isProcessing = false;

    }

    rtengine::ImageSource* imgsrc = isrc->getImageSource ();
    imgsrc->setProgressListener (this);
    return false;
}

bool EditorPanel::idle_imageSaved (ProgressConnector<int> *pc, rtengine::IImagefloat* img, Glib::ustring fname, SaveFormat sf, rtengine::procparams::ProcParams &pparams)
{
    delete img;

    if (! pc->returnValue() ) {
        openThm->imageDeveloped ();

        // save processing parameters, if needed
        if (sf.saveParams) {
            // We keep the extension to avoid overwriting the profile when we have
            // the same output filename with different extension
            pparams.save (fname + ".out" + App::PARAM_FILE_EXTENSION);
        }
    } else {
        error (M ("MAIN_MSG_CANNOTSAVE"), fname);
    }

    saveimgas->set_sensitive (true);
    send_to_external->set_sensitive(send_to_external->getEntryCount());

    parent->setProgressStr ("");
    parent->setProgress (0.);

    setProgressState (false);

    delete pc;
    SoundManager::playSoundAsync (App::get().options().sndBatchQueueDone);
    isProcessing = false;
    return false;
}

BatchQueueEntry* EditorPanel::createBatchQueueEntry ()
{

    rtengine::procparams::ProcParams pparams;
    ipc->getParams (&pparams);
    //rtengine::ProcessingJob* job = rtengine::ProcessingJob::create (ipc->getInitialImage(), pparams);
    rtengine::ProcessingJob* job = rtengine::ProcessingJob::create (openThm->getFileName (), openThm->getType() == FT_Raw, pparams);
    int fullW = 0, fullH = 0;
    isrc->getImageSource()->getFullSize (fullW, fullH, pparams.coarse.rotate == 90 || pparams.coarse.rotate == 270 ? TR_R90 : TR_NONE);
    int prevh = BatchQueue::calcMaxThumbnailHeight();
    int prevw = int ((size_t)fullW * (size_t)prevh / (size_t)fullH);
    return new BatchQueueEntry (job, pparams, openThm->getFileName(), prevw, prevh, openThm, App::get().options().overwriteOutputFile);
}



void EditorPanel::saveAsPressed ()
{
    if (!ipc || !openThm) {
        return;
    }

    bool fnameOK = false;
    Glib::ustring fnameOut;

    SaveAsDialog* saveAsDialog;
    auto toplevel = static_cast<Gtk::Window*> (get_toplevel ());

    auto& options = App::get().mut_options();
    if (Glib::file_test (options.lastSaveAsPath, Glib::FILE_TEST_IS_DIR)) {
        saveAsDialog = new SaveAsDialog (options.lastSaveAsPath, toplevel);
    } else {
        saveAsDialog = new SaveAsDialog (PlacesBrowser::userPicturesDir (), toplevel);
    }

    saveAsDialog->set_default_size (options.saveAsDialogWidth, options.saveAsDialogHeight);
    saveAsDialog->setInitialFileName (lastSaveAsFileName);
    saveAsDialog->setImagePath (fname);

    do {
        int result = saveAsDialog->run ();

        // The SaveAsDialog ensure that a filename has been specified
        fnameOut = saveAsDialog->getFileName ();

        options.lastSaveAsPath = saveAsDialog->getDirectory ();
        saveAsDialog->get_size (options.saveAsDialogWidth, options.saveAsDialogHeight);
        options.autoSuffix = saveAsDialog->getAutoSuffix ();
        options.saveMethodNum = saveAsDialog->getSaveMethodNum ();
        lastSaveAsFileName = Glib::path_get_basename (removeExtension (fnameOut));
        SaveFormat sf = saveAsDialog->getFormat ();
        options.saveFormat = sf;
        options.forceFormatOpts = saveAsDialog->getForceFormatOpts ();

        if (result != Gtk::RESPONSE_OK) {
            break;
        }

        if (saveAsDialog->getImmediately ()) {
            // separate filename and the path to the destination directory
            Glib::ustring dstdir = Glib::path_get_dirname (fnameOut);
            Glib::ustring dstfname = Glib::path_get_basename (removeExtension (fnameOut));
            Glib::ustring dstext = getExtension (fnameOut);

            if (saveAsDialog->getAutoSuffix()) {

                Glib::ustring fnameTemp;

                for (int tries = 0; tries < 100; tries++) {
                    if (tries == 0) {
                        fnameTemp = Glib::ustring::compose ("%1.%2", Glib::build_filename (dstdir,  dstfname), dstext);
                    } else {
                        fnameTemp = Glib::ustring::compose ("%1-%2.%3", Glib::build_filename (dstdir,  dstfname), tries, dstext);
                    }

                    if (!Glib::file_test (fnameTemp, Glib::FILE_TEST_EXISTS)) {
                        fnameOut = fnameTemp;
                        fnameOK = true;
                        break;
                    }
                }
            }

            // check if it exists
            if (!fnameOK) {
                fnameOK = confirmOverwrite (*saveAsDialog, fnameOut);
            }

            if (fnameOK) {
                isProcessing = true;
                // save image
                rtengine::procparams::ProcParams pparams;
                ipc->getParams (&pparams);
                rtengine::ProcessingJob* job = rtengine::ProcessingJob::create (ipc->getInitialImage(), pparams);

                ProgressConnector<rtengine::IImagefloat*> *ld = new ProgressConnector<rtengine::IImagefloat*>();
                ld->startFunc (sigc::bind (sigc::ptr_fun (&rtengine::processImage), job, err, parent->getProgressListener(), false ),
                               sigc::bind (sigc::mem_fun ( *this, &EditorPanel::idle_saveImage ), ld, fnameOut, sf, pparams));
                saveimgas->set_sensitive (false);
                send_to_external->set_sensitive(false);
            }
        } else {
            BatchQueueEntry* bqe = createBatchQueueEntry ();
            bqe->outFileName = fnameOut;
            bqe->saveFormat = saveAsDialog->getFormat ();
            bqe->overwriteFile = !saveAsDialog->getAutoSuffix();
            bqe->forceFormatOpts = saveAsDialog->getForceFormatOpts ();
            parent->addBatchQueueJob (bqe, saveAsDialog->getToHeadOfQueue ());
            fnameOK = true;
        }

        // ask parent to redraw file browser
        // ... or does it automatically when the tab is switched to it
    } while (!fnameOK);

    saveAsDialog->hide();

    delete saveAsDialog;
}

void EditorPanel::queueImgPressed ()
{
    if (!ipc || !openThm) {
        return;
    }

    saveProfile ();
    parent->addBatchQueueJob (createBatchQueueEntry ());
}

void EditorPanel::sendToExternal()
{
    if (!ipc || !openThm) {
        return;
    }

    // develop image
    rtengine::procparams::ProcParams pparams;
    ipc->getParams (&pparams);
    if (App::get().options().editor_bypass_output_profile) {
        pparams.icm.outputProfile = rtengine::procparams::ColorManagementParams::NoProfileString;
    }

    if (!cached_exported_filename.empty() && cached_exported_image == ipc->getInitialImage() && pparams == cached_exported_pparams && Glib::file_test(cached_exported_filename, Glib::FILE_TEST_IS_REGULAR)) {
        idle_sentToGimp(nullptr, nullptr, cached_exported_filename);
        return;
    }

    cached_exported_image = ipc->getInitialImage();
    cached_exported_pparams = pparams;
    cached_exported_filename.clear();
    rtengine::ProcessingJob* job = rtengine::ProcessingJob::create (ipc->getInitialImage(), pparams);
    ProgressConnector<rtengine::IImagefloat*> *ld = new ProgressConnector<rtengine::IImagefloat*>();
    ld->startFunc (sigc::bind (sigc::ptr_fun (&rtengine::processImage), job, err, parent->getProgressListener(), false ),
                   sigc::bind (sigc::mem_fun ( *this, &EditorPanel::idle_sendToGimp ), ld, openThm->getFileName() ));
    saveimgas->set_sensitive (false);
    send_to_external->set_sensitive(false);
}

void EditorPanel::sendToExternalChanged(int)
{
    int index = send_to_external->getSelected();
    auto& options = App::get().mut_options();
    if (index >= 0 && static_cast<unsigned>(index) == options.externalEditors.size()) {
        index = -1;
    }
    options.externalEditorIndex = index;
    if (externalEditorChangedSignal) {
        externalEditorChangedSignal->emit();
    }
}

void EditorPanel::sendToExternalPressed()
{
    const auto& options = App::get().options();
    if (options.externalEditorIndex == -1) {
        // "Other" external editor. Show app chooser dialog to let user pick.
        RTAppChooserDialog *dialog = getAppChooserDialog();
        dialog->show();
    } else {
        struct ExternalEditor editor = options.externalEditors.at(options.externalEditorIndex);
        external_editor_info = {
            editor.name,
            editor.command,
            editor.native_command};
        sendToExternal();
    }
}


bool EditorPanel::saveImmediately (const Glib::ustring &filename, const SaveFormat &sf)
{
    rtengine::procparams::ProcParams pparams;
    ipc->getParams (&pparams);

    rtengine::ProcessingJob *job = rtengine::ProcessingJob::create (ipc->getInitialImage(), pparams);

    // save immediately
    rtengine::IImagefloat *img = rtengine::processImage (job, err, nullptr, false);

    int err = 0;

    if (App::get().isGimpPlugin()) {
        err = img->saveAsTIFF (filename, 32, true, true);
    } else if (sf.format == "tif") {
        err = img->saveAsTIFF (filename, sf.tiffBits, sf.tiffFloat, sf.tiffUncompressed, sf.bigTiff);
    } else if (sf.format == "png") {
        err = img->saveAsPNG (filename, sf.pngBits);
    } else if (sf.format == "jpg") {
        err = img->saveAsJPEG (filename, sf.jpegQuality, sf.jpegSubSamp);
    } else {
        err = 1;
    }

    delete img;
    return !err;
}


void EditorPanel::openPreviousEditorImage()
{
    if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
        fPanel->fileCatalog->openNextPreviousEditorImage (fname, NAV_PREVIOUS);
    }
}

void EditorPanel::openNextEditorImage()
{
    if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
        fPanel->fileCatalog->openNextPreviousEditorImage (fname, NAV_NEXT);
    }
}

void EditorPanel::syncFileBrowser()   // synchronize filebrowser with image in Editor
{
    if (!App::get().isSimpleEditor() && fPanel && !fname.empty()) {
        fPanel->fileCatalog->selectImage (fname, false);
    }
}

ExternalEditorChangedSignal * EditorPanel::getExternalEditorChangedSignal()
{
    return externalEditorChangedSignal;
}

void EditorPanel::setExternalEditorChangedSignal(ExternalEditorChangedSignal *signal)
{
    if (externalEditorChangedSignal) {
        externalEditorChangedSignalConnection.disconnect();
    }
    externalEditorChangedSignal = signal;
    if (signal) {
        externalEditorChangedSignalConnection = signal->connect(
                sigc::mem_fun(*this, &EditorPanel::updateExternalEditorSelection));
    }
}

void EditorPanel::histogramProfile_toggled()
{
    auto& options = App::get().mut_options();
    options.rtSettings.HistogramWorking = toggleHistogramProfile->get_active();
    colorMgmtToolBar->updateHistogram();
}

bool EditorPanel::idle_sendToGimp ( ProgressConnector<rtengine::IImagefloat*> *pc, Glib::ustring fname)
{

    rtengine::IImagefloat* img = pc->returnValue();
    delete pc;

    const auto& options = App::get().options();
    if (img) {
        // get file name base
        Glib::ustring shortname = removeExtension(Glib::path_get_basename(fname));
        Glib::ustring dirname;
        switch (options.editor_out_dir) {
        case Options::EDITOR_OUT_DIR_CURRENT:
            dirname = Glib::path_get_dirname(fname);
            break;
        case Options::EDITOR_OUT_DIR_CUSTOM:
            dirname = options.editor_custom_out_dir;
            break;
        default: // Options::EDITOR_OUT_DIR_TEMP
            dirname = getTmpDirectory();
            break;
        }
        Glib::ustring fullFileName = Glib::build_filename(dirname, shortname);

        SaveFormat sf;
        sf.format = "tif";
        if (options.editor_float32) {
            sf.tiffBits = 32;
            sf.tiffFloat = true;
        } else {
            sf.tiffBits = 16;
            sf.tiffFloat = false;
        }

        sf.tiffUncompressed = true;
        sf.saveParams = true;

        Glib::ustring fileName = Glib::ustring::compose ("%1.%2", fullFileName, sf.format);

        // TODO: Just list all file with a suitable name instead of brute force...
        int tries = 1;
        while (Glib::file_test (fileName, Glib::FILE_TEST_EXISTS) && tries < 1000) {
            fileName = Glib::ustring::compose ("%1-%2.%3", fullFileName, tries, sf.format);
            tries++;
        }

        if (tries == 1000) {
            delete img;
            return false;
        }

        ProgressConnector<int> *ld = new ProgressConnector<int>();
        img->setSaveProgressListener (parent->getProgressListener());
        ld->startFunc (sigc::bind (sigc::mem_fun (img, &rtengine::IImagefloat::saveAsTIFF), fileName, sf.tiffBits, sf.tiffFloat, sf.tiffUncompressed, sf.bigTiff),
                       sigc::bind (sigc::mem_fun (*this, &EditorPanel::idle_sentToGimp), ld, img, fileName));
    } else {
        Glib::ustring msg_ = Glib::ustring ("<b> Error during image processing\n</b>");
        Gtk::MessageDialog msgd (*parent, msg_, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        msgd.run ();
        saveimgas->set_sensitive (true);
        send_to_external->set_sensitive(send_to_external->getEntryCount());
    }

    return false;
}

bool EditorPanel::idle_sentToGimp (ProgressConnector<int> *pc, rtengine::IImagefloat* img, Glib::ustring filename)
{
    if (img) {
        delete img;
        cached_exported_filename = filename;
    }
    int errore = 0;
    setProgressState(false);
    if (pc) {
        errore = pc->returnValue();
        delete pc;
    }

    if ((!img && Glib::file_test(filename, Glib::FILE_TEST_IS_REGULAR)) || (img && !errore)) {
        saveimgas->set_sensitive (true);
        send_to_external->set_sensitive(send_to_external->getEntryCount());
        parent->setProgressStr ("");
        parent->setProgress (0.);
        bool success = false;

        setUserOnlyPermission(Gio::File::create_for_path(filename), false);

        success = ExtProgStore::openInExternalEditor(filename, external_editor_info);

        if (!success) {
            Gtk::MessageDialog msgd (*parent, M ("MAIN_MSG_CANNOTSTARTEDITOR"), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            msgd.set_secondary_text (M ("MAIN_MSG_CANNOTSTARTEDITOR_SECONDARY"));
            msgd.set_title (M ("MAIN_BUTTON_SENDTOEDITOR"));
            msgd.run ();
        }
    }

    return false;
}

RTAppChooserDialog *EditorPanel::getAppChooserDialog()
{
    if (!app_chooser_dialog.get()) {
        app_chooser_dialog.reset(new RTAppChooserDialog("image/tiff"));
        app_chooser_dialog->signal_response().connect(
            sigc::mem_fun(*this, &EditorPanel::onAppChooserDialogResponse)
        );
        app_chooser_dialog->set_modal();
    }

    return app_chooser_dialog.get();
}

void EditorPanel::onAppChooserDialogResponse(int responseId)
{
    switch (responseId) {
        case Gtk::RESPONSE_OK: {
            getAppChooserDialog()->close();
            const auto app_info = getAppChooserDialog()->get_app_info();
            external_editor_info = {
                app_info->get_name(),
                app_info->get_commandline(),
                false};
            sendToExternal();
            break;
        }
        case Gtk::RESPONSE_CANCEL:
        case Gtk::RESPONSE_CLOSE:
            getAppChooserDialog()->close();
            break;
        default:
            break;
    }
}

void EditorPanel::updateExternalEditorSelection()
{
    const auto& options = App::get().options();
    int index = send_to_external->getSelected();
    if (index >= 0 && static_cast<unsigned>(index) == options.externalEditors.size()) {
        index = -1;
    }
    if (options.externalEditorIndex != index) {
        send_to_external->setSelected(
            options.externalEditorIndex >= 0 ? options.externalEditorIndex : options.externalEditors.size());
    }
}

void EditorPanel::historyBeforeLineChanged (const rtengine::procparams::ProcParams& params)
{

    if (beforeIpc) {
        ProcParams* pparams = beforeIpc->beginUpdateParams ();
        *pparams = params;
        beforeIpc->endUpdateParams (rtengine::EvProfileChanged);  // starts the IPC processing
    }
}

void EditorPanel::beforeAfterToggled ()
{

    if (!ipc) {
        return;
    }

    removeIfThere (beforeAfterBox,  beforeBox, false);
    removeIfThere (afterBox,  afterHeaderBox, false);

    if (beforeIarea) {
        if (beforeIpc) {
            beforeIpc->stopProcessing ();
        }

        iareapanel->setBeforeAfterViews (nullptr, iareapanel);
        iareapanel->imageArea->iLinkedImageArea = nullptr;
        delete beforeIarea;
        beforeIarea = nullptr;

        if (beforeIpc) {
            beforeIpc->setPreviewImageListener (nullptr);
        }

        delete beforePreviewHandler;
        beforePreviewHandler = nullptr;

        if (beforeIpc) {
            rtengine::StagedImageProcessor::destroy (beforeIpc);
        }

        beforeIpc = nullptr;
    }

    if (beforeAfter->get_active ()) {

        int errorCode = 0;
        rtengine::InitialImage *beforeImg = rtengine::InitialImage::load ( isrc->getImageSource ()->getFileName(),  openThm->getType() == FT_Raw, &errorCode, nullptr);

        if ( !beforeImg || errorCode ) {
            return;
        }

        beforeIarea = new ImageAreaPanel ();

        int HeaderBoxHeight = 17;

        beforeLabel = Gtk::manage (new Gtk::Label ());
        beforeLabel->set_markup (Glib::ustring ("<b>") + M ("GENERAL_BEFORE") + "</b>");
        tbBeforeLock = Gtk::manage (new Gtk::ToggleButton ());
        tbBeforeLock->set_relief(Gtk::RELIEF_NONE);
        tbBeforeLock->set_tooltip_markup (M ("MAIN_TOOLTIP_BEFOREAFTERLOCK"));
        tbBeforeLock->signal_toggled().connect ( sigc::mem_fun (*this, &EditorPanel::tbBeforeLock_toggled) );
        beforeHeaderBox = Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL));
        beforeHeaderBox->get_style_context()->add_class("smallbuttonbox");
        beforeHeaderBox->pack_end (*tbBeforeLock, Gtk::PACK_SHRINK, 2);
        beforeHeaderBox->pack_end (*beforeLabel, Gtk::PACK_SHRINK, 2);
        beforeHeaderBox->set_size_request (0, HeaderBoxHeight);

        history->blistenerLock ? tbBeforeLock->set_image (*iBeforeLockON) : tbBeforeLock->set_image (*iBeforeLockOFF);
        tbBeforeLock->set_active (history->blistenerLock);

        beforeBox = Gtk::manage (new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
        beforeBox->pack_start (*beforeHeaderBox, Gtk::PACK_SHRINK, 2);
        beforeBox->pack_start (*beforeIarea);

        afterLabel = Gtk::manage (new Gtk::Label ());
        afterLabel->set_markup (Glib::ustring ("<b>") + M ("GENERAL_AFTER") + "</b>");
        afterHeaderBox = Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL));
        afterHeaderBox->set_size_request (0, HeaderBoxHeight);
        afterHeaderBox->pack_end (*afterLabel, Gtk::PACK_SHRINK, 2);
        afterBox->pack_start (*afterHeaderBox, Gtk::PACK_SHRINK, 2);
        afterBox->reorder_child (*afterHeaderBox, 0);

        beforeAfterBox->pack_start (*beforeBox);
        beforeAfterBox->reorder_child (*beforeBox, 0);
        beforeAfterBox->show_all ();

        beforePreviewHandler = new PreviewHandler ();

        beforeIpc = rtengine::StagedImageProcessor::create (beforeImg);
        beforeIpc->setPreviewScale (10);
        beforeIpc->setPreviewImageListener (beforePreviewHandler);
        Glib::ustring monitorProfile;
        rtengine::RenderingIntent intent;
        ipc->getMonitorProfile(monitorProfile, intent);
        beforeIpc->setMonitorProfile(monitorProfile, intent);

        beforeIarea->imageArea->setPreviewHandler (beforePreviewHandler);
        beforeIarea->imageArea->setImProcCoordinator (beforeIpc);

        beforeIarea->imageArea->setPreviewModePanel (iareapanel->imageArea->previewModePanel);
        beforeIarea->imageArea->setIndicateClippedPanel (iareapanel->imageArea->indClippedPanel);
        iareapanel->imageArea->iLinkedImageArea = beforeIarea->imageArea;

        iareapanel->setBeforeAfterViews (beforeIarea, iareapanel);
        beforeIarea->setBeforeAfterViews (beforeIarea, iareapanel);

        rtengine::procparams::ProcParams params;

        if (history->getBeforeLineParams (params)) {
            historyBeforeLineChanged (params);
        }
    }
}

void EditorPanel::tbBeforeLock_toggled ()
{
    history->blistenerLock = tbBeforeLock->get_active();
    tbBeforeLock->get_active() ? tbBeforeLock->set_image (*iBeforeLockON) : tbBeforeLock->set_image (*iBeforeLockOFF);
}

void EditorPanel::histogramChanged(
    const LUTu& histRed,
    const LUTu& histGreen,
    const LUTu& histBlue,
    const LUTu& histLuma,
    const LUTu& histToneCurve,
    const LUTu& histLCurve,
    const LUTu& histCCurve,
    const LUTu& histLCAM,
    const LUTu& histCCAM,
    const LUTu& histRedRaw,
    const LUTu& histGreenRaw,
    const LUTu& histBlueRaw,
    const LUTu& histChroma,
    const LUTu& histLRETI,
    int vectorscopeScale,
    const array2D<int>& vectorscopeHC,
    const array2D<int>& vectorscopeHS,
    int waveformScale,
    const array2D<int>& waveformRed,
    const array2D<int>& waveformGreen,
    const array2D<int>& waveformBlue,
    const array2D<int>& waveformLuma
)
{
    if (histogramPanel) {
        histogramPanel->histogramChanged(histRed, histGreen, histBlue, histLuma, histChroma, histRedRaw, histGreenRaw, histBlueRaw, vectorscopeScale, vectorscopeHC, vectorscopeHS, waveformScale, waveformRed, waveformGreen, waveformBlue, waveformLuma);
    }

    tpc->updateCurveBackgroundHistogram(histToneCurve, histLCurve, histCCurve, histLCAM, histCCAM, histRed, histGreen, histBlue, histLuma, histLRETI);
}

void EditorPanel::setObservable(rtengine::HistogramObservable* observable)
{
    histogram_observable = observable;
}

bool EditorPanel::updateHistogram(void) const
{
    return histogram_scope_type == ScopeType::HISTOGRAM
        || histogram_scope_type == ScopeType::NONE;
}

bool EditorPanel::updateHistogramRaw(void) const
{
    return histogram_scope_type == ScopeType::HISTOGRAM_RAW
        || histogram_scope_type == ScopeType::NONE;
}

bool EditorPanel::updateVectorscopeHC(void) const
{
    return
        histogram_scope_type == ScopeType::VECTORSCOPE_HC
        || histogram_scope_type == ScopeType::NONE;
}

bool EditorPanel::updateVectorscopeHS(void) const
{
    return
        histogram_scope_type == ScopeType::VECTORSCOPE_HS
        || histogram_scope_type == ScopeType::NONE;
}

bool EditorPanel::updateWaveform(void) const
{
    return histogram_scope_type == ScopeType::WAVEFORM
        || histogram_scope_type == ScopeType::PARADE
        || histogram_scope_type == ScopeType::NONE;
}

void EditorPanel::scopeTypeChanged(ScopeType new_type)
{
    histogram_scope_type = new_type;

    if (!histogram_observable) {
        return;
    }

    // Make sure the new scope is updated since we only actively update the
    // current scope.
    switch (new_type) {
        case ScopeType::HISTOGRAM:
            histogram_observable->requestUpdateHistogram();
            break;
        case ScopeType::HISTOGRAM_RAW:
            histogram_observable->requestUpdateHistogramRaw();
            break;
        case ScopeType::VECTORSCOPE_HC:
            histogram_observable->requestUpdateVectorscopeHC();
            break;
        case ScopeType::VECTORSCOPE_HS:
            histogram_observable->requestUpdateVectorscopeHS();
            break;
        case ScopeType::PARADE:
        case ScopeType::WAVEFORM:
            histogram_observable->requestUpdateWaveform();
            break;
        case ScopeType::NONE:
            break;
    }
}

bool EditorPanel::CheckSidePanelsVisibility()
{
    if (tbTopPanel_1) {
        return tbTopPanel_1->get_active() || tbRightPanel_1->get_active() || hidehp->get_active();
    }

    return tbRightPanel_1->get_active() || hidehp->get_active();
}

void EditorPanel::toggleSidePanels()
{
    // Maximize preview panel:
    // toggle top AND right AND history panels

    bool bAllSidePanelsVisible;
    bAllSidePanelsVisible = CheckSidePanelsVisibility();

    if (tbTopPanel_1) {
        tbTopPanel_1->set_active (!bAllSidePanelsVisible);
    }

    tbRightPanel_1->set_active (!bAllSidePanelsVisible);
    hidehp->set_active (!bAllSidePanelsVisible);

    if (!bAllSidePanelsVisible) {
        tbShowHideSidePanels->set_image (*iShowHideSidePanels);
    } else {
        tbShowHideSidePanels->set_image (*iShowHideSidePanels_exit);
    }
}

void EditorPanel::toggleSidePanelsZoomFit()
{
    toggleSidePanels();

    // fit image preview
    // !!! TODO this does not want to work... seems to have an effect on a subsequent key press
    // iarea->imageArea->zoomPanel->zoomFitClicked();
}

void EditorPanel::tbShowHideSidePanels_managestate()
{
    bool bAllSidePanelsVisible;
    bAllSidePanelsVisible = CheckSidePanelsVisibility();
    ShowHideSidePanelsconn.block (true);

    tbShowHideSidePanels->set_active (!bAllSidePanelsVisible);

    ShowHideSidePanelsconn.block (false);
}

void EditorPanel::updateExternalEditorWidget(int selectedIndex, const std::vector<ExternalEditor> &editors)
{
    // Remove the editors.
    while (send_to_external->getEntryCount()) {
        send_to_external->removeEntry(send_to_external->getEntryCount() - 1);
    }

    // Create new radio button group because they cannot be reused: https://developer-old.gnome.org/gtkmm/3.16/classGtk_1_1RadioButtonGroup.html#details.
    send_to_external_radio_group = Gtk::RadioButtonGroup();

    // Add the editors.
    for (unsigned i = 0; i < editors.size(); i++) {
        const auto & name = editors[i].name.empty() ? Glib::ustring(" ") : editors[i].name;
        if (!editors[i].icon_serialized.empty()) {
            Glib::RefPtr<Gio::Icon> gioIcon;
            GError *e = nullptr;
            GVariant *icon_variant = g_variant_parse(
                nullptr, editors[i].icon_serialized.c_str(), nullptr, nullptr, &e);

            if (e) {
                std::cerr
                    << "Error loading external editor icon from \""
                    << editors[i].icon_serialized << "\": " << e->message
                    << std::endl;
                gioIcon = Glib::RefPtr<Gio::Icon>();
            } else {
                gioIcon = Gio::Icon::deserialize(Glib::VariantBase(icon_variant));
            }

            send_to_external->insertEntry(i, gioIcon, name, &send_to_external_radio_group);
        } else {
            send_to_external->insertEntry(i, "palette-brush", name, &send_to_external_radio_group);
        }
    }

#ifndef __APPLE__
    send_to_external->addEntry("palette-brush", M("GENERAL_OTHER"), &send_to_external_radio_group);
#endif
    send_to_external->set_sensitive(send_to_external->getEntryCount());
    send_to_external->setSelected(selectedIndex);
    send_to_external->show();
}

void EditorPanel::updateProfiles (const Glib::ustring &printerProfile, rtengine::RenderingIntent printerIntent, bool printerBPC)
{
}

void EditorPanel::updateTPVScrollbar (bool hide)
{
    tpc->updateTPVScrollbar (hide);
}

void EditorPanel::updateHistogramPosition (int oldPosition, int newPosition)
{
    const auto& options = App::get().options();

    switch (newPosition) {
        case 0:

            // No histogram
            if (!oldPosition) {
                // An histogram actually exist, we delete it
                delete histogramPanel;
                histogramPanel = nullptr;
            }

            // else no need to create it
            break;

        case 1:

            // Histogram on the left pane
            if (oldPosition == 0) {
                // There was no Histogram before, so we create it
                histogramPanel = Gtk::manage (new HistogramPanel ());
                leftbox->pack1(*histogramPanel, false, false);
            } else if (oldPosition == 2) {
                // The histogram was on the right side, so we move it to the left
                histogramPanel->reference();
                removeIfThere (vboxright, histogramPanel, false);
                leftbox->pack1(*histogramPanel, false, false);
                histogramPanel->unreference();
            }

            leftbox->set_position(options.histogramHeight);
            histogramPanel->reorder (Gtk::POS_LEFT);
            break;

        case 2:
        default:

            // Histogram on the right pane
            if (oldPosition == 0) {
                // There was no Histogram before, so we create it
                histogramPanel = Gtk::manage (new HistogramPanel ());
                vboxright->pack1 (*histogramPanel, false, false);
            } else if (oldPosition == 1) {
                // The histogram was on the left side, so we move it to the right
                histogramPanel->reference();
                removeIfThere (leftbox, histogramPanel, false);
                vboxright->pack1 (*histogramPanel, false, false);
                histogramPanel->unreference();
            }

            vboxright->set_position(options.histogramHeight);
            histogramPanel->reorder (Gtk::POS_RIGHT);
            break;
    }

    if (histogramPanel) {
        histogramPanel->setPanelListener(this);
    }

    iareapanel->imageArea->setPointerMotionHListener (histogramPanel);

}

void EditorPanel::updateToolPanelToolLocations(
    const std::vector<Glib::ustring> &favorites, bool cloneFavoriteTools)
{
    if (tpc) {
        tpc->updateToolLocations(favorites, cloneFavoriteTools);
    }
}

void EditorPanel::defaultMonitorProfileChanged (const Glib::ustring &profile_name, bool auto_monitor_profile)
{
    colorMgmtToolBar->defaultMonitorProfileChanged (profile_name, auto_monitor_profile);
}

void EditorPanel::saveLUTPressed ()
{
    if (!ipc || !openThm) {
        return;
    }

    auto* toplevel = static_cast<Gtk::Window*>(get_toplevel());

    // Build the dialog using the same pattern as SaveAsDialog.
    Gtk::Dialog dialog(M("MAIN_BUTTON_SAVE_LUT_DIALOG_TITLE"), *toplevel);

    Gtk::FileChooserWidget* fchooser = Gtk::manage(
        new Gtk::FileChooserWidget(Gtk::FILE_CHOOSER_ACTION_SAVE));

    auto& opts = App::get().mut_options();
    if (Glib::file_test(opts.lastSaveAsPath, Glib::FILE_TEST_IS_DIR)) {
        fchooser->set_current_folder(opts.lastSaveAsPath);
    }
    fchooser->set_current_name("lut.png");

    auto filter_png = Gtk::FileFilter::create();
    filter_png->set_name("PNG");
    filter_png->add_pattern("*.png");
    filter_png->add_pattern("*.PNG");
    fchooser->set_filter(filter_png);

    fchooser->signal_file_activated().connect([&dialog]() {
        dialog.response(Gtk::RESPONSE_OK);
    });

    Gtk::Button* ok     = Gtk::manage(new Gtk::Button(M("GENERAL_OK")));
    Gtk::Button* cancel = Gtk::manage(new Gtk::Button(M("GENERAL_CANCEL")));
    ok->signal_clicked().connect([&dialog]()     { dialog.response(Gtk::RESPONSE_OK); });
    cancel->signal_clicked().connect([&dialog]() { dialog.response(Gtk::RESPONSE_CANCEL); });

    dialog.get_content_area()->pack_start(*fchooser);
    dialog.get_action_area()->pack_end(*ok,     Gtk::PACK_SHRINK, 4);
    dialog.get_action_area()->pack_end(*cancel, Gtk::PACK_SHRINK, 4);
    dialog.show_all_children();

    if (dialog.run() != Gtk::RESPONSE_OK) {
        return;
    }

    Glib::ustring destPath = fchooser->get_filename();
    if (destPath.empty()) {
        destPath = Glib::build_filename(
            fchooser->get_current_folder(), fchooser->get_current_name());
    }

    // Append .png if the user omitted it.
    if (rtengine::getFileExtension(destPath).lowercase() != "png") {
        destPath += ".png";
    }

    opts.lastSaveAsPath = Glib::path_get_dirname(destPath);

    // Ask before overwriting an existing file.
    if (Glib::file_test(destPath, Glib::FILE_TEST_EXISTS)) {
        Gtk::MessageDialog confirm(*toplevel,
            escapeHtmlChars(destPath) + "\n" + M("MAIN_MSG_ALREADYEXISTS") + " " + M("MAIN_MSG_QOVERWRITE"),
            true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
        if (confirm.run() != Gtk::RESPONSE_YES) {
            return;
        }
    }

    // Generate a Hald 12 identity PNG to a temp file.
    const Glib::ustring tmpPath = rtengine::HaldCLUT::createIdentityTempFile(12);
    if (tmpPath.empty()) {
        Gtk::MessageDialog msgd(*toplevel,
            "<b>" + M("MAIN_BUTTON_SAVE_LUT_ERR_IDENTITY") + "</b>",
            true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        msgd.run();
        return;
    }

    // Build the processing parameters for the LUT (tone curve always excluded).
    ProcParams pparams;
    ipc->getParams(&pparams);
    const ProcParams lutParams = makeLUTProcParams(pparams);

    // Process the identity image asynchronously; route progress to this panel.
    rtengine::ProcessingJob* job =
        rtengine::ProcessingJob::create(tmpPath, false, lutParams);

    ProgressConnector<rtengine::IImagefloat*>* ld =
        new ProgressConnector<rtengine::IImagefloat*>();
    ld->startFunc(
        sigc::bind(sigc::ptr_fun(&rtengine::processImage),
                   job, err, static_cast<rtengine::ProgressListener*>(this), false),
        sigc::bind(sigc::mem_fun(*this, &EditorPanel::idle_saveLUTImage),
                   ld, destPath, tmpPath));

    saveLUTBtn->set_sensitive(false);
}

bool EditorPanel::idle_saveLUTImage (ProgressConnector<rtengine::IImagefloat*>* pc,
                                      Glib::ustring destPath, Glib::ustring tmpPath)
{
    rtengine::IImagefloat* img = pc->returnValue();
    delete pc;

    // Remove the temporary identity file regardless of outcome.
    try {
        Gio::File::create_for_path(tmpPath)->remove();
    } catch (...) {}

    if (img) {
        setProgressStr(M("GENERAL_SAVE"));
        setProgress(0.9f);

        ProgressConnector<int>* ld = new ProgressConnector<int>();
        img->setSaveProgressListener(parent->getProgressListener());
        ld->startFunc(
            sigc::bind(sigc::mem_fun(img, &rtengine::IImagefloat::saveAsPNG),
                       destPath, 8),
            sigc::bind(sigc::mem_fun(*this, &EditorPanel::idle_saveLUTSaved),
                       ld, img, destPath));
    } else {
        auto* toplevel = static_cast<Gtk::Window*>(get_toplevel());
        Gtk::MessageDialog msgd(*toplevel,
            "<b>" + M("MAIN_BUTTON_SAVE_LUT_ERR_PROCESSING") + "</b>",
            true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        msgd.run();
        setProgressState(false);
        saveLUTBtn->set_sensitive(true);
    }

    return false;
}

bool EditorPanel::idle_saveLUTSaved (ProgressConnector<int>* pc,
                                      rtengine::IImagefloat* img, Glib::ustring destPath)
{
    const int result = pc->returnValue();
    delete pc;
    delete img;

    if (result != 0) {
        auto* toplevel = static_cast<Gtk::Window*>(get_toplevel());
        Glib::ustring msg =
            Glib::ustring("<b>") + M("MAIN_MSG_CANNOTSAVE") + ": "
            + escapeHtmlChars(destPath) + "</b>";
        Gtk::MessageDialog msgd(*toplevel, msg, true,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        msgd.run();
    }

    parent->setProgressStr("");
    parent->setProgress(0.);
    setProgressState(false);
    saveLUTBtn->set_sensitive(true);
    return false;
}
