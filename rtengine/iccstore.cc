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
#include <cstring>
#include <map>
#include <string>

#include <cjson/cJSON.h>
#include <glibmm/ustring.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glib/gstdio.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <iostream>

#include "iccstore.h"

#include "iccmatrices.h"
#include "utils.h"

#include "rtgui/options.h"
#include "rtgui/threadutils.h"
#include "lcms2_plugin.h"

#include "color.h"

namespace
{

// Not recursive
void loadProfiles(
    const Glib::ustring& dirName,
    std::map<std::string, cmsHPROFILE>* profiles,
    std::map<std::string, rtengine::ProfileContent>* profileContents,
    std::map<std::string, Glib::ustring>* profileNames,
    bool nameUpper
)
{
    if (dirName.empty()) {
        return;
    }

    try {
        Glib::Dir dir(dirName);

        for (Glib::DirIterator entry = dir.begin(); entry != dir.end(); ++entry) {
            const Glib::ustring fileName = *entry;

            if (fileName.size() < 4) {
                continue;
            }

            const Glib::ustring extension = rtengine::getFileExtension(fileName);

            if (extension != "icc" && extension != "icm") {
                continue;
            }

            const Glib::ustring filePath = Glib::build_filename(dirName, fileName);

            if (!Glib::file_test(filePath, Glib::FILE_TEST_IS_REGULAR)) {
                continue;
            }

            Glib::ustring name = fileName.substr(0, fileName.size() - 4);

            if (nameUpper) {
                name = name.uppercase();
            }

            if (profiles) {
                const rtengine::ProfileContent content(filePath);
                const cmsHPROFILE profile = content.toProfile();

                if (profile) {
                    profiles->emplace(name, profile);

                    if (profileContents) {
                        profileContents->emplace(name, content);
                    }
                }
            }

            if (profileNames) {
                profileNames->emplace(name, filePath);
            }
        }
    } catch (Glib::Exception&) {
    }
}

// Version dedicated to single profile load when loadAll==false (cli version "-q" mode)
bool loadProfile(
    const Glib::ustring& profile,
    const Glib::ustring& dirName,
    std::map<std::string, cmsHPROFILE>* profiles,
    std::map<std::string, rtengine::ProfileContent>* profileContents
)
{
    if (dirName.empty() || profiles == nullptr) {
        return false;
    }

    try {
        Glib::Dir dir(dirName);

        for (Glib::DirIterator entry = dir.begin(); entry != dir.end(); ++entry) {
            const Glib::ustring fileName = *entry;

            if (fileName.size() < 4) {
                continue;
            }

            const Glib::ustring extension = rtengine::getFileExtension(fileName);

            if (extension != "icc" && extension != "icm") {
                continue;
            }

            const Glib::ustring filePath = Glib::build_filename(dirName, fileName);

            if (!Glib::file_test(filePath, Glib::FILE_TEST_IS_REGULAR)) {
                continue;
            }

            const Glib::ustring name = fileName.substr(0, fileName.size() - 4);

            if (name == profile) {
                const rtengine::ProfileContent content(filePath);
                const cmsHPROFILE profile = content.toProfile();

                if (profile) {
                    profiles->emplace(name, profile);

                    if (profileContents) {
                        profileContents->emplace(name, content);
                    }

                    return true;
                }
            }
        }
    } catch (Glib::Exception&) {
    }

    return false;
}

void getSupportedIntent(cmsHPROFILE profile, cmsUInt32Number intent, cmsUInt32Number direction, uint8_t& result)
{
    if (cmsIsIntentSupported(profile, intent, direction)) {
        result |= 1 << intent;
    }
}

uint8_t getSupportedIntents(cmsHPROFILE profile, cmsUInt32Number direction)
{
    if (!profile) {
        return 0;
    }

    uint8_t result = 0;

    getSupportedIntent(profile, INTENT_PERCEPTUAL, direction, result);
    getSupportedIntent(profile, INTENT_RELATIVE_COLORIMETRIC, direction, result);
    getSupportedIntent(profile, INTENT_SATURATION, direction, result);
    getSupportedIntent(profile, INTENT_ABSOLUTE_COLORIMETRIC, direction, result);

    return result;
}

cmsHPROFILE createXYZProfile()
{
    double mat[3][3] = { {1.0, 0, 0}, {0, 1.0, 0}, {0, 0, 1.0} };
    return rtengine::ICCStore::createFromMatrix(mat, false, "XYZ");
}

const double(*wprofiles[])[3]  = {xyz_sRGB, xyz_adobe, xyz_prophoto, xyz_widegamut, xyz_jdcmax, xyz_jdcmaxstdA, xyz_beta, xyz_best, xyz_rec2020, xyz_ACESp0, xyz_ACESp1, xyz_bruce};//
const double(*iwprofiles[])[3] = {sRGB_xyz, adobe_xyz, prophoto_xyz, widegamut_xyz, jdcmax_xyz, jdcmaxstdA_xyz, beta_xyz, best_xyz, rec2020_xyz, ACESp0_xyz, ACESp1_xyz, bruce_xyz};//
const char* wpnames[] = {"sRGB", "Adobe RGB", "ProPhoto", "WideGamut", "JDCmax", "JDCmax stdA", "Beta RGB", "BestRGB", "Rec2020", "ACESp0", "ACESp1", "BruceRGB"};//
//default = gamma inside profile
//BT709 g=2.22 s=4.5  sRGB g=2.4 s=12.92310
//linear g=1.0
//std22 g=2.2   std18 g=1.8
// high  g=1.3 s=3.35  for high dynamic images
//low  g=2.6 s=6.9  for low contrast images

//-----------------------------------------------------------------------------
// helper functions to fix V2 profiles TRCs, used in
// rtengine::ProfileContent::toProfile()
// see https://github.com/Beep6581/RawTherapee/issues/5026
// -----------------------------------------------------------------------------
bool is_RTv2_profile(cmsHPROFILE profile)
{
    if (int(cmsGetProfileVersion(profile)) != 2) {
        return false;
    }
    const cmsMLU *mlu = static_cast<const cmsMLU *>(cmsReadTag(profile, cmsSigDeviceMfgDescTag));
    if (!mlu) {
        return false;
    }
    cmsUInt32Number sz = cmsMLUgetASCII(mlu, "en", "US", nullptr, 0);
    if (!sz) {
        return false;
    }
    std::vector<char> buf(sz);
    cmsMLUgetASCII(mlu, "en", "US", &buf[0], sz);
    buf.back() = 0; // sanity
    return strcmp(&buf[0], "RawTherapee") == 0;
}


bool get_RT_gamma_slope(cmsHPROFILE profile, double &gammatag, double &slopetag)
{
    const cmsMLU *modelDescMLU = static_cast<const cmsMLU *>(cmsReadTag(profile, cmsSigDeviceModelDescTag));
    if (modelDescMLU) {
        cmsUInt32Number count = cmsMLUgetWide(modelDescMLU, "en", "US", nullptr, 0);
        if (count) {
            std::vector<wchar_t> vbuf(count);
            wchar_t *buffer = &vbuf[0];
            count = cmsMLUgetWide(modelDescMLU, "en", "US", buffer, count);
            Glib::ustring modelDesc;
#if __SIZEOF_WCHAR_T__ == 2
            char *cModelDesc = g_utf16_to_utf8((unsigned short int*)buffer, -1, nullptr, nullptr, nullptr); // convert to utf-8 in a buffer allocated by glib
            if (cModelDesc) {
                modelDesc.assign(cModelDesc);
                g_free(cModelDesc);
            }
#else
            modelDesc = utf32_to_utf8(buffer, count);
#endif
            if (!modelDesc.empty()) {
                try {
                    std::size_t pos = modelDesc.find("g");
                    std::size_t posmid = modelDesc.find("s");
                    std::size_t posend = modelDesc.find("!");
                    if (pos == std::string::npos || posmid == std::string::npos || posend == std::string::npos) {
                        return false;
                    }
                    std::string strgamma = modelDesc.substr(pos + 1, (posmid - pos));
                    gammatag = std::stod(strgamma.c_str());
                    std::string strslope = modelDesc.substr(posmid + 1, (posend - posmid));
                    slopetag = std::stod(strslope.c_str());
                    return true;
                } catch (std::invalid_argument &) {
                    return false;
                }
            }
        }
    }
    return false;
}


Glib::ustring get_profile_description(cmsHPROFILE profile)
{
    const cmsMLU *mlu = static_cast<const cmsMLU *>(cmsReadTag(profile, cmsSigProfileDescriptionTag));
    if (!mlu) {
        return "";
    }
    cmsUInt32Number sz = cmsMLUgetASCII(mlu, "en", "US", nullptr, 0);
    if (!sz) {
        return "";
    }
    std::vector<char> buf(sz);
    cmsMLUgetASCII(mlu, "en", "US", &buf[0], sz);
    buf.back() = 0; // sanity
    return std::string(&buf[0]);
}

} // namespace


rtengine::ProfileContent::ProfileContent() = default;

rtengine::ProfileContent::ProfileContent(const Glib::ustring& fileName)
{
    FILE* const f = g_fopen(fileName.c_str(), "rb");

    if (!f) {
        return;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);

    if (length > 0) {
        char* d = new char[length + 1];
        fseek(f, 0, SEEK_SET);
        length = fread(d, 1, length, f);
        d[length] = 0;
        data.assign(d, length);
        delete[] d;
    } else {
        data.clear();
    }

    fclose(f);

}

rtengine::ProfileContent::ProfileContent(cmsHPROFILE hProfile)
{
    if (hProfile != nullptr) {
        cmsUInt32Number bytesNeeded = 0;
        cmsSaveProfileToMem(hProfile, nullptr, &bytesNeeded);

        if (bytesNeeded > 0) {
            char* d = new char[bytesNeeded + 1];
            cmsSaveProfileToMem(hProfile, d, &bytesNeeded);
            data.assign(d, bytesNeeded);
            delete[] d;
        }
    }
}

cmsHPROFILE rtengine::ProfileContent::toProfile() const
{
    cmsHPROFILE profile = nullptr;
    if (!data.empty()) {
        profile = cmsOpenProfileFromMem(data.c_str(), data.size());
        // if this is a V2 profile generated by RawTherapee, we rebuild the
        // TRC. See https://github.com/Beep6581/RawTherapee/issues/5026 and
        // the references in there
        if (profile && is_RTv2_profile(profile)) {
            double gammatag, slopetag;
            if (get_RT_gamma_slope(profile, gammatag, slopetag)) {
                constexpr double eps = 0.000000001; // not divide by zero
                double pwr = 1.0 / gammatag;
                double ts = slopetag;
                double slope = slopetag == 0 ? eps : slopetag;

                GammaValues g_b; //gamma parameters
                Color::calcGamma(pwr, ts, g_b); // call to calcGamma with selected gamma and slope : return parameters for LCMS2
                cmsFloat64Number gammaParams[7]; //gamma parameters
                gammaParams[4] = g_b[3] * ts;
                gammaParams[0] = gammatag;
                gammaParams[1] = 1. / (1.0 + g_b[4]);
                gammaParams[2] = g_b[4] / (1.0 + g_b[4]);
                gammaParams[3] = 1. / slope;
                gammaParams[5] = 0.0;
                gammaParams[6] = 0.0;

                cmsToneCurve* GammaTRC;
                if (slopetag == 0.) {
                    //printf("gammatag=%f\n", gammatag);
                    GammaTRC = cmsBuildGamma(NULL, gammatag);
                } else {
                    GammaTRC = cmsBuildParametricToneCurve(nullptr, 5, gammaParams); //5 = smoother than 4
                }
                cmsWriteTag(profile, cmsSigRedTRCTag, GammaTRC);
                cmsWriteTag(profile, cmsSigGreenTRCTag, GammaTRC);
                cmsWriteTag(profile, cmsSigBlueTRCTag, GammaTRC);
                cmsFreeToneCurve(GammaTRC);

                if (settings->verbose) {
                    std::cout << "ICCStore: rebuilt TRC for RTv2 profile " << get_profile_description(profile) << ": gamma=" << gammatag << ", slope=" << slopetag << std::endl;
                }
            } else if (settings->verbose) {
                std::cout << "ICCStore: no gamma/slope info found for RTv2 profile " << get_profile_description(profile) << std::endl;
            }
        }
    }
    return profile;
}

const std::string& rtengine::ProfileContent::getData() const
{
    return data;
}

class rtengine::ICCStore::Implementation
{
public:
    Implementation() :
        loadAll(true),
        xyz(createXYZProfile()),
        srgb(cmsCreate_sRGBProfile())
    {
        //cmsErrorAction(LCMS_ERROR_SHOW);

        constexpr int N = sizeof(wpnames) / sizeof(wpnames[0]);

        for (int i = 0; i < N; ++i) {
            wProfiles[wpnames[i]] = createFromMatrix(wprofiles[i]);
            // wProfilesGamma[wpnames[i]] = createFromMatrix(wprofiles[i], true);
            wMatrices[wpnames[i]] = wprofiles[i];
            iwMatrices[wpnames[i]] = iwprofiles[i];
        }
    }

    ~Implementation()
    {
        for (auto &p : wProfiles) {
            if (p.second) {
                cmsCloseProfile(p.second);
            }
        }

        // for (auto &p : wProfilesGamma) {
        //     if (p.second) {
        //         cmsCloseProfile(p.second);
        //     }
        // }
        for (auto &p : fileProfiles) {
            if (p.second) {
                cmsCloseProfile(p.second);
            }
        }

        if (srgb) {
            cmsCloseProfile(srgb);
        }

        if (xyz) {
            cmsCloseProfile(xyz);
        }
    }

    void init(const Glib::ustring& usrICCDir, const Glib::ustring& rtICCDir, bool loadAll)
    {
        // Reads all profiles from the given profiles dir

        MyMutex::MyLock lock(mutex);

        this->loadAll = loadAll;

        // RawTherapee's profiles take precedence if a user's profile of the same name exists
        profilesDir = Glib::build_filename(rtICCDir, "output");
        userICCDir = usrICCDir;
        fileProfiles.clear();
        fileProfileContents.clear();

        const auto& options = App::get().options();

        if (loadAll) {
            loadProfiles(profilesDir, &fileProfiles, &fileProfileContents, nullptr, false);
            loadProfiles(userICCDir, &fileProfiles, &fileProfileContents, nullptr, false);
            Glib::ustring user_output_icc_dir = Glib::build_filename(options.rtdir, "iccprofiles", "output");
            loadProfiles(user_output_icc_dir, &fileProfiles, &fileProfileContents, nullptr, false);
        }

        // Input profiles
        // Load these to different areas, since the short name(e.g. "NIKON D700" may overlap between system/user and RT dir)
        stdProfilesDir = Glib::build_filename(rtICCDir, "input");
        fileStdProfiles.clear();
        fileStdProfilesFileNames.clear();

        if (loadAll) {
            loadProfiles(stdProfilesDir, nullptr, nullptr, &fileStdProfilesFileNames, true);
            Glib::ustring user_input_icc_dir = Glib::build_filename(options.rtdir, "iccprofiles", "input");
            loadProfiles(user_input_icc_dir, nullptr, nullptr, &fileStdProfilesFileNames, true);
        }

        defaultMonitorProfile = settings->monitorProfile;

        loadWorkingSpaces(rtICCDir);
        loadWorkingSpaces(userICCDir);

        // initialize the alarm colours for lcms gamut checking -- we use bright green
        cmsUInt16Number cms_alarm_codes[cmsMAXCHANNELS] = { 0, 65535, 65535 };
        cmsSetAlarmCodes(cms_alarm_codes);
    }

    cmsHPROFILE workingSpace(const Glib::ustring& name) const
    {
        const ProfileMap::const_iterator r = wProfiles.find(name);

        return
            r != wProfiles.end()
            ? r->second
            : wProfiles.find("sRGB")->second;
    }

    // cmsHPROFILE workingSpaceGamma(const Glib::ustring& name) const
    // {

    //     const ProfileMap::const_iterator r = wProfilesGamma.find(name);

    //     return
    //         r != wProfilesGamma.end()
    //             ? r->second
    //             : wProfilesGamma.find("sRGB")->second;
    // }

    TMatrix workingSpaceMatrix(const Glib::ustring& name) const
    {
        const MatrixMap::const_iterator r = wMatrices.find(name);

        return
            r != wMatrices.end()
            ? r->second
            : wMatrices.find("sRGB")->second;
    }

    TMatrix workingSpaceInverseMatrix(const Glib::ustring& name) const
    {

        const MatrixMap::const_iterator r = iwMatrices.find(name);

        return
            r != iwMatrices.end()
            ? r->second
            : iwMatrices.find("sRGB")->second;
    }

    bool outputProfileExist(const Glib::ustring& name) const
    {
        MyMutex::MyLock lock(mutex);
        return fileProfiles.find(name) != fileProfiles.end();
    }

    cmsHPROFILE getProfile(const Glib::ustring& name)
    {
        MyMutex::MyLock lock(mutex);

        const ProfileMap::const_iterator r = fileProfiles.find(name);

        if (r != fileProfiles.end()) {
            return r->second;
        }

        if (!name.compare(0, 5, "file:")) {
            const ProfileContent content(name.substr(5));
            const cmsHPROFILE profile = content.toProfile();

            if (profile) {
                fileProfiles.emplace(name, profile);
                fileProfileContents.emplace(name, content);

                return profile;
            }
        } else if (!loadAll) {
            // Look for a standard profile
            if (!loadProfile(name, profilesDir, &fileProfiles, &fileProfileContents)) {
                loadProfile(name, userICCDir, &fileProfiles, &fileProfileContents);
            }

            const ProfileMap::const_iterator r = fileProfiles.find(name);

            if (r != fileProfiles.end()) {
                return r->second;
            }
        }

        return nullptr;
    }

    cmsHPROFILE getStdProfile(const Glib::ustring& name)
    {
        const Glib::ustring nameUpper = name.uppercase();

        MyMutex::MyLock lock(mutex);

        const ProfileMap::const_iterator r = fileStdProfiles.find(nameUpper);

        // Return profile from store
        if (r != fileStdProfiles.end()) {
            return r->second;
        } else if (!loadAll) {
            // Directory not scanned, so looking and adding now...
            if (!loadProfile(name, profilesDir, &fileProfiles, &fileProfileContents)) {
                loadProfile(name, userICCDir, &fileProfiles, &fileProfileContents);
            }

            const ProfileMap::const_iterator r = fileProfiles.find(name);

            if (r != fileProfiles.end()) {
                return r->second;
            }
        }

        // Profile is not yet in store
        const NameMap::const_iterator f = fileStdProfilesFileNames.find(nameUpper);

        // Profile does not exist
        if (f == fileStdProfilesFileNames.end()) {
            return nullptr;
        }

        // But there exists one --> load it
        const ProfileContent content(f->second);
        const cmsHPROFILE profile = content.toProfile();

        if (profile) {
            fileStdProfiles.emplace(f->first, profile);
        }

        // Profile invalid or stored now --> remove entry from fileStdProfilesFileNames
        fileStdProfilesFileNames.erase(f);
        return profile;
    }

    ProfileContent getContent(const Glib::ustring& name) const
    {
        MyMutex::MyLock lock(mutex);

        const ContentMap::const_iterator r = fileProfileContents.find(name);

        return
            r != fileProfileContents.end()
            ? r->second
            : ProfileContent();
    }

    cmsHPROFILE getXYZProfile() const
    {
        return xyz;
    }

    cmsHPROFILE getsRGBProfile() const
    {
        return srgb;
    }

    std::vector<Glib::ustring> getProfiles(ProfileType type) const
    {
        std::vector<Glib::ustring> res;

        MyMutex::MyLock lock(mutex);

        for (const auto& profile : fileProfiles) {
            if (
                (
                    type == ICCStore::ProfileType::MONITOR
                    && cmsGetDeviceClass(profile.second) == cmsSigDisplayClass
                    && cmsGetColorSpace(profile.second) == cmsSigRgbData
                )
                || (
                    type == ICCStore::ProfileType::PRINTER
                    && cmsGetDeviceClass(profile.second) == cmsSigOutputClass
                )
                || (
                    type == ICCStore::ProfileType::OUTPUT
                    && (cmsGetDeviceClass(profile.second) == cmsSigDisplayClass
                        || cmsGetDeviceClass(profile.second) == cmsSigInputClass
                        || cmsGetDeviceClass(profile.second) == cmsSigOutputClass)
                    && cmsGetColorSpace(profile.second) == cmsSigRgbData
                )
            ) {
                res.push_back(profile.first);
            }
        }

        return res;
    }

    std::vector<Glib::ustring> getProfilesFromDir(const Glib::ustring& dirName) const
    {
        std::vector<Glib::ustring> res;
        ProfileMap profiles;

        MyMutex::MyLock lock(mutex);

        loadProfiles(profilesDir, &profiles, nullptr, nullptr, false);
        loadProfiles(dirName, &profiles, nullptr, nullptr, false);

        for (const auto& profile : profiles) {
            res.push_back(profile.first);
        }

        return res;
    }

    std::uint8_t getInputIntents(cmsHPROFILE profile)
    {
        MyMutex::MyLock lock(mutex);

        return getSupportedIntents(profile, LCMS_USED_AS_INPUT);
    }

    std::uint8_t getOutputIntents(cmsHPROFILE profile)
    {
        MyMutex::MyLock lock(mutex);

        return getSupportedIntents(profile, LCMS_USED_AS_OUTPUT);
    }

    std::uint8_t getProofIntents(cmsHPROFILE profile)
    {
        MyMutex::MyLock lock(mutex);

        return getSupportedIntents(profile, LCMS_USED_AS_PROOF);
    }

    std::uint8_t getInputIntents(const Glib::ustring &name)
    {
        return getInputIntents(getProfile(name));
    }

    std::uint8_t getOutputIntents(const Glib::ustring &name)
    {
        return getOutputIntents(getProfile(name));
    }

    std::uint8_t getProofIntents(const Glib::ustring &name)
    {
        return getProofIntents(getProfile(name));
    }

    Glib::ustring getDefaultMonitorProfileName() const
    {
        return defaultMonitorProfile;
    }

    void setDefaultMonitorProfileName(const Glib::ustring &name)
    {
        MyMutex::MyLock lock(mutex);
        defaultMonitorProfile = name;
    }

    std::vector<Glib::ustring> getWorkingProfiles()
    {
        std::vector<Glib::ustring> res;

        // for (unsigned int i = 0; i < sizeof(wpnames) / sizeof(wpnames[0]); i++) {
        //     res.push_back(wpnames[i]);
        // }
        for (const auto &p : wProfiles) {
            res.push_back(p.first);
        }

        return res;
    }

private:
    using CVector = std::array<double, 3>;
    using CMatrix = std::array<CVector, 3>;
    struct PMatrix {
        double matrix[3][3];
        PMatrix(): matrix{} {}
        explicit PMatrix(const CMatrix &m)
        {
            set(m);
        }

        CMatrix toMatrix() const
        {
            CMatrix ret;

            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    ret[i][j] = matrix[i][j];
                }
            }

            return ret;
        }

        void set(const CMatrix &m)
        {
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    matrix[i][j] = m[i][j];
                }
            }
        }
    };

    bool computeWorkingSpaceMatrix(const Glib::ustring &path, const Glib::ustring &filename, PMatrix &out)
    {
        Glib::ustring fullpath = filename;

        if (!Glib::path_is_absolute(fullpath)) {
            fullpath = Glib::build_filename(path, filename);
        }

        ProfileContent content(fullpath);
        cmsHPROFILE prof = content.toProfile();

        if (!prof) {
            return false;
        }

        if (cmsGetColorSpace(prof) != cmsSigRgbData) {
            cmsCloseProfile(prof);
            return false;
        }

        if (!cmsIsMatrixShaper(prof)) {
            cmsCloseProfile(prof);
            return false;
        }

        cmsCIEXYZ *red = static_cast<cmsCIEXYZ *>(cmsReadTag(prof, cmsSigRedMatrixColumnTag));
        cmsCIEXYZ *green  = static_cast<cmsCIEXYZ *>(cmsReadTag(prof, cmsSigGreenMatrixColumnTag));
        cmsCIEXYZ *blue  = static_cast<cmsCIEXYZ *>(cmsReadTag(prof, cmsSigBlueMatrixColumnTag));

        if (!red || !green || !blue) {
            cmsCloseProfile(prof);
            return false;
        }

        CMatrix m = {
            CVector({ red->X, green->X, blue->X }),
            CVector({ red->Y, green->Y, blue->Y }),
            CVector({ red->Z, green->Z, blue->Z })
        };
        m[1][0] = red->Y;
        m[1][1] = green->Y;
        m[1][2] = blue->Y;
        m[2][0] = red->Z;
        m[2][1] = green->Z;
        m[2][2] = blue->Z;
        out.set(m);

        cmsCloseProfile(prof);
        return true;
    }

    bool loadWorkingSpaces(const Glib::ustring &path)
    {
        Glib::ustring fileName = Glib::build_filename(path, "workingspaces.json");
        FILE* const f = g_fopen(fileName.c_str(), "r");

        if (settings->verbose) {
            std::cout << "trying to load extra working spaces from " << fileName << std::flush;
        }

        if (!f) {
            if (settings->verbose) {
                std::cout << " FAIL" << std::endl;
            }

            return false;
        }

        fseek(f, 0, SEEK_END);
        long length = ftell(f);

        if (length <= 0) {
            if (settings->verbose) {
                std::cout << " FAIL" << std::endl;
            }

            fclose(f);
            return false;
        }

        char *buf = new char[length + 1];
        fseek(f, 0, SEEK_SET);
        length = fread(buf, 1, length, f);
        buf[length] = 0;

        fclose(f);

        cJSON_Minify(buf);
        cJSON *root = cJSON_Parse(buf);

        if (!root) {
            if (settings->verbose) {
                std::cout << " FAIL" << std::endl;
            }

            return false;
        }

        delete[] buf;

        cJSON *js = cJSON_GetObjectItem(root, "working_spaces");

        if (!js) {
            goto parse_error;
        }

        for (js = js->child; js != nullptr; js = js->next) {
            cJSON *ji = cJSON_GetObjectItem(js, "name");
            std::unique_ptr<PMatrix> m(new PMatrix);
            std::string name;

            if (!ji || ji->type != cJSON_String) {
                goto parse_error;
            }

            name = ji->valuestring;

            if (wProfiles.find(name) != wProfiles.end()) {
                continue; // already there -- ignore
            }

            bool found_matrix = false;

            ji = cJSON_GetObjectItem(js, "matrix");

            if (ji) {
                if (ji->type != cJSON_Array) {
                    goto parse_error;
                }

                ji = ji->child;

                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j, ji = ji->next) {
                        if (!ji || ji->type != cJSON_Number) {
                            goto parse_error;
                        }

                        m->matrix[i][j] = ji->valuedouble;
                    }
                }

                if (ji) {
                    goto parse_error;
                }

                found_matrix = true;
            } else {
                ji = cJSON_GetObjectItem(js, "file");

                if (!ji || ji->type != cJSON_String) {
                    goto parse_error;
                }

                found_matrix = computeWorkingSpaceMatrix(path, ji->valuestring, *m);
            }

            if (!found_matrix) {
                if (settings->verbose) {
                    std::cout << "Could not find suitable matrix for working space: " << name << std::endl;
                }

                continue;
            }

            pMatrices.emplace_back(std::move(m));
            TMatrix w = pMatrices.back()->matrix;

            CMatrix b = {};

            if (!rtengine::invertMatrix(pMatrices.back()->toMatrix(), b)) {
                if (settings->verbose) {
                    std::cout << "Matrix for working space: " << name << " is not invertible, skipping" << std::endl;
                }

                pMatrices.pop_back();
            } else {
                wMatrices[name] = w;
                pMatrices.emplace_back(new PMatrix(b));
                TMatrix iw = pMatrices.back()->matrix;
                iwMatrices[name] = iw;
                wProfiles[name] = createFromMatrix(w);

                if (settings->verbose) {
                    std::cout << "Added working space: " << name << std::endl;
                    std::cout << "  matrix: [";

                    for (int i = 0; i < 3; ++i) {
                        std::cout << " [";

                        for (int j = 0; j < 3; ++j) {
                            std::cout << " " << w[i][j];
                        }

                        std::cout << "]";
                    }

                    std::cout << " ]" << std::endl;
                }
            }
        }

        cJSON_Delete(root);

        if (settings->verbose) {
            std::cout << " OK" << std::endl;
        }

        return true;

parse_error:

        if (settings->verbose) {
            std::cout << " ERROR in parsing " << fileName << std::endl;
        }

        cJSON_Delete(root);
        return false;
    }

    using ProfileMap = std::map<std::string, cmsHPROFILE>;
    using MatrixMap = std::map<std::string, TMatrix>;
    using ContentMap = std::map<std::string, ProfileContent>;
    using NameMap = std::map<std::string, Glib::ustring>;

    ProfileMap wProfiles;
    // ProfileMap wProfilesGamma;
    MatrixMap wMatrices;
    MatrixMap iwMatrices;

    std::vector<std::unique_ptr<PMatrix>> pMatrices;

    // These contain profiles from user/system directory(supplied on init)
    Glib::ustring profilesDir;
    Glib::ustring userICCDir;
    ProfileMap fileProfiles;
    ContentMap fileProfileContents;

    //These contain standard profiles from RT. Keys are all in uppercase.
    Glib::ustring stdProfilesDir;
    NameMap fileStdProfilesFileNames;
    ProfileMap fileStdProfiles;

    Glib::ustring defaultMonitorProfile;

    bool loadAll;

    const cmsHPROFILE xyz;
    const cmsHPROFILE srgb;

    mutable MyMutex mutex;
};

rtengine::ICCStore* rtengine::ICCStore::getInstance()
{
    static rtengine::ICCStore instance;
    return &instance;
}

void rtengine::ICCStore::init(const Glib::ustring& usrICCDir, const Glib::ustring& stdICCDir, bool loadAll)
{
    implementation->init(usrICCDir, stdICCDir, loadAll);
}

cmsHPROFILE rtengine::ICCStore::workingSpace(const Glib::ustring& name) const
{
    return implementation->workingSpace(name);
}

// cmsHPROFILE rtengine::ICCStore::workingSpaceGamma(const Glib::ustring& name) const
// {
//     return implementation->workingSpaceGamma(name);
// }

rtengine::TMatrix rtengine::ICCStore::workingSpaceMatrix(const Glib::ustring& name) const
{
    return implementation->workingSpaceMatrix(name);
}

rtengine::TMatrix rtengine::ICCStore::workingSpaceInverseMatrix(const Glib::ustring& name) const
{
    return implementation->workingSpaceInverseMatrix(name);
}

bool rtengine::ICCStore::outputProfileExist(const Glib::ustring& name) const
{
    return implementation->outputProfileExist(name);
}

cmsHPROFILE rtengine::ICCStore::getProfile(const Glib::ustring& name) const
{
    return implementation->getProfile(name);
}

cmsHPROFILE rtengine::ICCStore::getStdProfile(const Glib::ustring& name) const
{
    return implementation->getStdProfile(name);
}

rtengine::ProfileContent rtengine::ICCStore::getContent(const Glib::ustring& name) const
{
    return implementation->getContent(name);
}


Glib::ustring rtengine::ICCStore::getDefaultMonitorProfileName() const
{
    return implementation->getDefaultMonitorProfileName();
}


void rtengine::ICCStore::setDefaultMonitorProfileName(const Glib::ustring &name)
{
    implementation->setDefaultMonitorProfileName(name);
}

cmsHPROFILE rtengine::ICCStore::getXYZProfile() const
{
    return implementation->getXYZProfile();
}

cmsHPROFILE rtengine::ICCStore::getsRGBProfile() const
{
    return implementation->getsRGBProfile();
}

std::vector<Glib::ustring> rtengine::ICCStore::getProfiles(ProfileType type) const
{
    return implementation->getProfiles(type);
}

std::vector<Glib::ustring> rtengine::ICCStore::getProfilesFromDir(const Glib::ustring& dirName) const
{
    return implementation->getProfilesFromDir(dirName);
}

std::uint8_t rtengine::ICCStore::getInputIntents(cmsHPROFILE profile) const
{
    return implementation->getInputIntents(profile);
}

std::uint8_t rtengine::ICCStore::getOutputIntents(cmsHPROFILE profile) const
{
    return implementation->getOutputIntents(profile);
}

std::uint8_t rtengine::ICCStore::getProofIntents(cmsHPROFILE profile) const
{
    return implementation->getProofIntents(profile);
}

std::uint8_t rtengine::ICCStore::getInputIntents(const Glib::ustring& name) const
{
    return implementation->getInputIntents(name);
}

std::uint8_t rtengine::ICCStore::getOutputIntents(const Glib::ustring& name) const
{
    return implementation->getOutputIntents(name);
}

std::uint8_t rtengine::ICCStore::getProofIntents(const Glib::ustring& name) const
{
    return implementation->getProofIntents(name);
}

rtengine::ICCStore::ICCStore() :
    implementation(new Implementation)
{
}

rtengine::ICCStore::~ICCStore() = default;

std::vector<Glib::ustring> rtengine::ICCStore::getWorkingProfiles()
{
    return implementation->getWorkingProfiles();
}

cmsHPROFILE rtengine::ICCStore::createFromMatrix(const double matrix[3][3], bool gamma, const Glib::ustring& name)
{

    static const unsigned phead[] = {
        1024, 0, 0x2100000, 0x6d6e7472, 0x52474220, 0x58595a20, 0, 0, 0,
        0x61637370, 0, 0, 0, 0, 0, 0, 0, 0xf6d6, 0x10000, 0xd32d
    };
    unsigned pbody[] = {
        10, 0x63707274, 0, 36,  /* cprt */
        0x64657363, 0, 40,  /* desc */
        0x77747074, 0, 20,  /* wtpt */
        0x626b7074, 0, 20,  /* bkpt */
        0x72545243, 0, 14,  /* rTRC */
        0x67545243, 0, 14,  /* gTRC */
        0x62545243, 0, 14,  /* bTRC */
        0x7258595a, 0, 20,  /* rXYZ */
        0x6758595a, 0, 20,  /* gXYZ */
        0x6258595a, 0, 20
    };    /* bXYZ */
    static const unsigned pwhite[] = { 0xf351, 0x10000, 0x116cc };//D65
    //static const unsigned pwhite[] = { 0xf6d6, 0x10000, 0xd340 };//D50

    // 0x63757276 : curveType, 0 : reserved, 1 : entries(1=gamma, 0=identity), 0x1000000=1.0
    unsigned pcurve[] = { 0x63757276, 0, 0, 0x1000000 };
//    unsigned pcurve[] = { 0x63757276, 0, 1, 0x1000000 };

    if (gamma) {
        pcurve[2] = 1;
        // pcurve[3] = 0x1f00000;// pcurve for gamma BT709 : g=2.22 s=4.5
        // normalize gamma in RT, default(Emil's choice = sRGB)
        pcurve[3] = 0x2390000;//pcurve for gamma sRGB : g:2.4 s=12.92310

    } else {
        // lcms2 up to 2.4 has a bug with linear gamma causing precision loss(banding)
        // of floating point data when a normal icc encoding of linear gamma is used
        //(i e 0 table entries), but by encoding a gamma curve which is 1.0 the
        // floating point path is taken within lcms2 so no precision loss occurs and
        // gamma is still 1.0.
        pcurve[2] = 1;
        pcurve[3] = 0x1000000; //pcurve for gamma 1
    }

    // constructing profile header
    unsigned* oprof = new unsigned [phead[0] / sizeof(unsigned)];
    memset(oprof, 0, phead[0]);
    memcpy(oprof, phead, sizeof(phead));

    oprof[0] = 132 + 12 * pbody[0];

    // constructing tag directory(pointers inside the file), and types
    // 0x74657874 : text
    // 0x64657363 : description tag
    for (unsigned int i = 0; i < pbody[0]; i++) {
        oprof[oprof[0] / 4] = i ? (i > 1 ? 0x58595a20 : 0x64657363) : 0x74657874;
        pbody[i * 3 + 2] = oprof[0];
        oprof[0] += (pbody[i * 3 + 3] + 3) & -4;
    }

    memcpy(oprof + 32, pbody, sizeof(pbody));

    // wtpt
    memcpy((char *)oprof + pbody[8] + 8, pwhite, sizeof(pwhite));

    // r/g/b TRC
    for (int i = 4; i < 7; i++) {
        memcpy((char *)oprof + pbody[i * 3 + 2], pcurve, sizeof(pcurve));
    }

    // r/g/b XYZ
//    pseudoinverse((double(*)[3]) out_rgb[output_color-1], inverse, 3);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            oprof[pbody[j * 3 + 23] / 4 + i + 2] = matrix[i][j] * 0x10000 + 0.5;
//      for (num = k=0; k < 3; k++)
//        num += xyzd50_srgb[i][k] * inverse[j][k];
        }

    // convert to network byte order
    for (unsigned int i = 0; i < phead[0] / 4; i++) {
        oprof[i] = htonl(oprof[i]);
    }

    // cprt
    strcpy((char *)oprof + pbody[2] + 8, "--rawtherapee profile--");

    // desc
    oprof[pbody[5] / 4 + 2] = name.size() + 1;
    strcpy((char *)oprof + pbody[5] + 12, name.c_str());


    cmsHPROFILE p = cmsOpenProfileFromMem(oprof, ntohl(oprof[0]));
    delete [] oprof;
    return p;
}
