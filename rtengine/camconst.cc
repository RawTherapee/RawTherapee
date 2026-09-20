/*
 *  This file is part of RawTherapee.
 */
#include "camconst.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <memory>
#include <vector>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>

#include "settings.h"
#include "rt_math.h"

// cJSON is a very minimal JSON parser lib in C, not for threaded stuff etc, so if we're going to use JSON more than just
// here we should probably replace cJSON with something beefier.
#include "cJSON.h"

namespace rtengine
{

CameraConst::CameraConst() : pdafOffset(0)
{
    memset(dcraw_matrix, 0, sizeof(dcraw_matrix));
    white_max = 0;
    globalGreenEquilibration = -1;
}


bool CameraConst::parseApertureScaling(CameraConst *cc, const void *ji_)
{
    const cJSON *ji = static_cast<const cJSON *>(ji_);

    if (ji->type != cJSON_Array) {
        fprintf(stderr, "\"ranges\":\"aperture_scaling\" must be an array\n");
        return false;
    }

    for (ji = ji->child; ji; ji = ji->next) {
        const cJSON *js = cJSON_GetObjectItem(ji, "aperture");

        if (!js) {
            fprintf(stderr, "missing \"ranges\":\"aperture_scaling\":\"aperture\" object item.\n");
            return false;
        }

        if (js->type != cJSON_Number) {
            fprintf(stderr, "\"ranges\":\"aperture_scaling\":\"aperture\" must be a number.\n");
            return false;
        }

        const float aperture = js->valuedouble;
        js = cJSON_GetObjectItem(ji, "scale_factor");

        if (!js) {
            fprintf(stderr, "missing \"ranges\":\"aperture_scaling\":\"scale_factor\" object item.\n");
            return false;
        }

        if (js->type != cJSON_Number) {
            fprintf(stderr, "\"ranges\":\"aperture_scaling\":\"scale_factor\" must be a number.\n");
            return false;
        }

        const float scale_factor = js->valuedouble;
        cc->mApertureScaling.emplace(aperture, scale_factor);
    }

    return true;
}

bool CameraConst::parseLevels(CameraConst *cc, int bw, const void *ji_)
{
    const cJSON *ji = static_cast<const cJSON *>(ji_);

    if (ji->type == cJSON_Number) {
        camera_const_levels lvl;
        lvl.levels[0] = lvl.levels[1] = lvl.levels[2] = lvl.levels[3] = ji->valueint;
        cc->mLevels[bw].emplace(0, lvl);
        return true;
    }

    if (ji->type != cJSON_Array) {
        fprintf(stderr, "\"ranges\":\"%s\" must be a number or an array\n", bw ? "white" : "black");
        return false;
    }

    if (ji->child->type == cJSON_Number) {
        camera_const_levels lvl;
        int i;
        const cJSON *js;

        for (js = ji->child, i = 0; js && i < 4; js = js->next, i++) {
            lvl.levels[i] = js->valueint;
        }

        if (i == 3) {
            lvl.levels[3] = lvl.levels[1]; // G2 = G1
        } else if (i == 1) {
            lvl.levels[3] = lvl.levels[2] = lvl.levels[1] = lvl.levels[0];
        } else if (i != 4 || js) {
            fprintf(stderr, "\"ranges\":\"%s\" array must have 1, 3 or 4 numbers.\n", bw ? "white" : "black");
            return false;
        }

        cc->mLevels[bw].emplace(0, lvl);
        return true;
    }

    for (ji = ji->child; ji; ji = ji->next) {
        const cJSON *js = cJSON_GetObjectItem(ji, "iso");

        if (!js) {
            fprintf(stderr, "missing \"ranges\":\"%s\":\"iso\" object item.\n", bw ? "white" : "black");
            return false;
        }

        std::vector<int> isos;

        if (js->type == cJSON_Number) {
            isos.push_back(js->valueint);
        } else if (js->type == cJSON_Array) {
            for (js = js->child; js; js = js->next) {
                if (js->type != cJSON_Number) {
                    fprintf(stderr, "\"ranges\":\"%s\":\"iso\" must be a number or an array of numbers.\n", bw ? "white" : "black");
                    return false;
                }

                isos.push_back(js->valueint);
            }
        } else {
            fprintf(stderr, "\"ranges\":\"%s\":\"iso\" must be an array or a number.\n", bw ? "white" : "black");
            return false;
        }

        js = cJSON_GetObjectItem(ji, "levels");

        if (!js) {
            fprintf(stderr, "missing \"ranges\":\"%s\":\"levels\".\n", bw ? "white" : "black");
            return false;
        }

        camera_const_levels lvl;

        if (js->type == cJSON_Number) {
            lvl.levels[0] = lvl.levels[1] = lvl.levels[2] = lvl.levels[3] = js->valueint;
        } else if (js->type == cJSON_Array) {
            int i;

            for (js = js->child, i = 0; js && i < 4; js = js->next, i++) {
                if (js->type != cJSON_Number) {
                    fprintf(stderr, "\"ranges\":\"%s\":\"levels\" must be a number or an array of numbers.\n", bw ? "white" : "black");
                    return false;
                }

                lvl.levels[i] = js->valueint;
            }

            if (i == 3) {
                lvl.levels[3] = lvl.levels[1]; // G2 = G1
            } else if (i == 1) {
                lvl.levels[3] = lvl.levels[2] = lvl.levels[1] = lvl.levels[0];
            } else if (i != 4 || js) {
                fprintf(stderr, "\"ranges\":\"%s\":\"levels\" array must have 1, 3 or 4 numbers.\n", bw ? "white" : "black");
                return false;
            }
        } else {
            fprintf(stderr, "\"ranges\":\"%s\":\"levels\" must be a number or an array of numbers.\n", bw ? "white" : "black");
            return false;
        }

        for (auto iso : isos) {
            cc->mLevels[bw].emplace(iso, lvl);
        }
    }

    return true;
}

CameraConst* CameraConst::parseEntry(const void *cJSON_, const char *make_model)
{
    const cJSON *js = static_cast<const cJSON*>(cJSON_);

    std::unique_ptr<CameraConst> cc(new CameraConst);
    cc->make_model = make_model;

    const auto get_raw_crop =
        [](int w, int h, const cJSON *ji, CameraConst *cc) -> bool
        {
            std::array<int, 4> rc;

            if (ji->type != cJSON_Array) {
                //fprintf(stderr, "\"raw_crop\" must be an array\n");
                return false;
            }

            int i;

            for (i = 0, ji = ji->child; i < 4 && ji != nullptr; i++, ji = ji->next) {
                if (ji->type != cJSON_Number) {
                    //fprintf(stderr, "\"raw_crop\" array must contain numbers\n");
                    return false;
                }

                //cc->raw_crop[i] = ji->valueint;
                rc[i] = ji->valueint;
            }

            if (i != 4 || ji != nullptr) {
                //fprintf(stderr, "\"raw_crop\" must contain 4 numbers\n");
                return false;
            }

            cc->raw_crop[std::make_pair(w, h)] = rc;
            return true;
        };

    const auto get_masked_areas =
        [](int w, int h, const cJSON *ji, CameraConst *cc) -> bool
        {
            std::array<std::array<int, 4>, 2> rm = {};

            if (ji->type != cJSON_Array) {
                //fprintf(stderr, "\"masked_areas\" must be an array\n");
                return false;
            }

            int i;

            for (i = 0, ji = ji->child; i < 2 * 4 && ji != nullptr; i++, ji = ji->next) {
                if (ji->type != cJSON_Number) {
                    //fprintf(stderr, "\"masked_areas\" array must contain numbers\n");
                    return false;
                }

                //cc->raw_mask[i / 4][i % 4] = ji->valueint;
                rm[i / 4][i % 4] = ji->valueint;
            }

            if (i % 4 != 0) {
                //fprintf(stderr, "\"masked_areas\" array length must be divisable by 4\n");
                return false;
            }

            cc->raw_mask[std::make_pair(w, h)] = rm;
            return true;
        };

    const cJSON *ji = cJSON_GetObjectItem(js, "dcraw_matrix");

    if (ji) {
        if (ji->type != cJSON_Array) {
            fprintf(stderr, "\"dcraw_matrix\" must be an array\n");
            return nullptr;
        }

        int i;

        for (i = 0, ji = ji->child; i < 12 && ji; i++, ji = ji->next) {
            if (ji->type != cJSON_Number) {
                fprintf(stderr, "\"dcraw_matrix\" array must contain numbers\n");
                return nullptr;
            }

            cc->dcraw_matrix[i] = ji->valueint;
        }
    }

    ji = cJSON_GetObjectItem(js, "raw_crop");

    if (ji) {
        if (ji->type != cJSON_Array) {
            fprintf(stderr, "invalid entry for raw_crop.\n");
            return nullptr;
        } else if (!get_raw_crop(0, 0, ji, cc.get())) {
            cJSON *je;
            cJSON_ArrayForEach(je, ji) {
                if (!cJSON_IsObject(je)) {
                    fprintf(stderr, "invalid entry for raw_crop.\n");
                    return nullptr;
                } else {
                    auto js = cJSON_GetObjectItem(je, "frame");
                    if (!js || js->type != cJSON_Array ||
                        cJSON_GetArraySize(js) != 2 ||
                        !cJSON_IsNumber(cJSON_GetArrayItem(js, 0)) ||
                        !cJSON_IsNumber(cJSON_GetArrayItem(js, 1))) {
                        fprintf(stderr, "invalid entry for raw_crop.\n");
                        return nullptr;
                    }
                    int w = cJSON_GetArrayItem(js, 0)->valueint;
                    int h = cJSON_GetArrayItem(js, 1)->valueint;
                    js = cJSON_GetObjectItem(je, "crop");
                    if (!js || !get_raw_crop(w, h, js, cc.get())) {
                        fprintf(stderr, "invalid entry for raw_crop.\n");
                        return nullptr;
                    }
                }
            }
        }
    }

    ji = cJSON_GetObjectItem(js, "masked_areas");

    if (ji) {
        if (ji->type != cJSON_Array) {
            fprintf(stderr, "invalid entry for masked_areas.\n");
            return nullptr;
        } else if (!get_masked_areas(0, 0, ji, cc.get())) {
            cJSON *je;
            cJSON_ArrayForEach(je, ji) {
                if (!cJSON_IsObject(je)) {
                    fprintf(stderr, "invalid entry for masked_areas.\n");
                    return nullptr;
                } else {
                    auto js = cJSON_GetObjectItem(je, "frame");
                    if (!js || js->type != cJSON_Array ||
                        cJSON_GetArraySize(js) != 2 ||
                        !cJSON_IsNumber(cJSON_GetArrayItem(js, 0)) ||
                        !cJSON_IsNumber(cJSON_GetArrayItem(js, 1))) {
                        fprintf(stderr, "invalid entry for masked_areas.\n");
                        return nullptr;
                    }
                    int w = cJSON_GetArrayItem(js, 0)->valueint;
                    int h = cJSON_GetArrayItem(js, 1)->valueint;
                    js = cJSON_GetObjectItem(je, "areas");
                    if (!js || !get_masked_areas(w, h, js, cc.get())) {
                        fprintf(stderr, "invalid entry for masked_areas.\n");
                        return nullptr;
                    }
                }
            }
        }
    }

    const cJSON *jranges = cJSON_GetObjectItem(js, "ranges");

    if (jranges) {
        ji = cJSON_GetObjectItem(jranges, "black");

        if (ji && !parseLevels(cc.get(), 0, ji)) {
            return nullptr;
        }

        ji = cJSON_GetObjectItem(jranges, "white");

        if (ji && !parseLevels(cc.get(), 1, ji)) {
            return nullptr;
        }

        ji = cJSON_GetObjectItem(jranges, "white_max");

        if (ji) {
            if (ji->type != cJSON_Number) {
                fprintf(stderr, "\"ranges\":\"white_max\" must be a number\n");
                return nullptr;
            }

            cc->white_max = ji->valueint;
        }

        ji = cJSON_GetObjectItem(jranges, "aperture_scaling");

        if (ji && !parseApertureScaling(cc.get(), ji)) {
            return nullptr;
        }
    }

    for (int bw = 0; bw < 2; bw++) {
        camera_const_levels lvl;

        if (!cc->get_Levels(lvl, bw, 0, 0)) {
            const auto it = cc->mLevels[bw].cbegin();

            if (it != cc->mLevels[bw].cend()) {
                // insert levels with lowest iso as the default (iso 0)
                cc->mLevels[bw].emplace(0, it->second);
            }
        }
    }

    ji = cJSON_GetObjectItem(js, "pdaf_pattern");

    if (ji) {
        if (ji->type != cJSON_Array) {
            fprintf(stderr, "\"pdaf_pattern\" must be an array\n");
            return nullptr;
        }

        for (ji = ji->child; ji; ji = ji->next) {
            if (ji->type != cJSON_Number) {
                fprintf(stderr, "\"pdaf_pattern\" array must contain numbers\n");
                return nullptr;
            }

            cc->pdafPattern.push_back(ji->valueint);
        }
    }

    ji = cJSON_GetObjectItem(js, "pdaf_offset");

    if (ji) {
        if (ji->type != cJSON_Number) {
            fprintf(stderr, "\"pdaf_offset\" must contain a number\n");
            return nullptr;
        }

        cc->pdafOffset = ji->valueint;
    }

    ji = cJSON_GetObjectItem(js, "global_green_equilibration");

    if (ji) {
        if (ji->type != cJSON_False && ji->type != cJSON_True) {
            fprintf(stderr, "\"global_green_equilibration\" must be a boolean\n");
            return nullptr;
        }

        cc->globalGreenEquilibration = (ji->type == cJSON_True);
    }
    
    return cc.release();
}

bool CameraConst::has_dcrawMatrix() const
{
    return dcraw_matrix[0] != 0;
}

void CameraConst::update_dcrawMatrix(const short *other)
{
    if (!other) {
        return;
    }

    for (int i = 0; i < 12; ++i) {
        dcraw_matrix[i] = other[i];
    }
}

const short* CameraConst::get_dcrawMatrix() const
{
    if (!has_dcrawMatrix()) {
        return nullptr;
    }

    return dcraw_matrix;
}

const std::vector<int>& CameraConst::get_pdafPattern() const
{
    return pdafPattern;
}

void CameraConst::update_pdafPattern(const std::vector<int> &other)
{
    if (other.empty()) {
        return;
    }

    pdafPattern = other;
}

void CameraConst::update_pdafOffset(int other)
{
    if (other == 0) {
        return;
    }

    pdafOffset = other;
}


bool CameraConst::has_rawCrop(int raw_width, int raw_height) const
{
    return raw_crop.find(std::make_pair(raw_width, raw_height)) != raw_crop.end() || raw_crop.find(std::make_pair(0, 0)) != raw_crop.end();
}


void CameraConst::get_rawCrop(int raw_width, int raw_height, int &left_margin, int &top_margin, int &width, int &height) const
{
    auto it = raw_crop.find(std::make_pair(raw_width, raw_height));
    if (it == raw_crop.end()) {
        it = raw_crop.find(std::make_pair(0, 0));
    }
    if (it != raw_crop.end()) {
        left_margin = it->second[0];
        top_margin = it->second[1];
        width = it->second[2];
        height = it->second[3];
    } else {
        left_margin = top_margin = width = height = 0;
    }
}


bool CameraConst::has_rawMask(int raw_width, int raw_height, int idx) const
{
    if (idx < 0 || idx > 1) {
        return false;
    }

    auto it = raw_mask.find(std::make_pair(raw_width, raw_height));
    if (it == raw_mask.end()) {
        it = raw_mask.find(std::make_pair(0, 0));
    }
    if (it != raw_mask.end()) {
        return (it->second[idx][0] | it->second[idx][1] | it->second[idx][2] | it->second[idx][3]) != 0;
    } else {
        return false;
    }
}


void CameraConst::get_rawMask(int raw_width, int raw_height, int idx, int &top, int &left, int &bottom, int &right) const
{
    top = left = bottom = right = 0;

    if (idx < 0 || idx > 1) {
        return;
    }

    auto it = raw_mask.find(std::make_pair(raw_width, raw_height));
    if (it == raw_mask.end()) {
        it = raw_mask.find(std::make_pair(0, 0));
    }

    if (it != raw_mask.end()) {
        top = it->second[idx][0];
        left = it->second[idx][1];
        bottom = it->second[idx][2];
        right = it->second[idx][3];
    }
}

void CameraConst::update_Levels(const CameraConst *other)
{
    if (!other) {
        return;
    }

    if (!other->mLevels[0].empty()) {
        mLevels[0] = other->mLevels[0];
    }

    if (!other->mLevels[1].empty()) {
        mLevels[1] = other->mLevels[1];
    }

    if (!other->mApertureScaling.empty()) {
        mApertureScaling = other->mApertureScaling;
    }

    if (other->white_max) {
        white_max = other->white_max;
    }
}

void CameraConst::update_Crop(CameraConst *other)
{
    if (!other) {
        return;
    }

    raw_crop.insert(other->raw_crop.begin(), other->raw_crop.end());
}

bool CameraConst::get_Levels(camera_const_levels & lvl, int bw, int iso, float fnumber) const
{
    std::map<int, camera_const_levels>::const_iterator it = mLevels[bw].find(iso);

    if (it == mLevels[bw].end()) {
        auto best_it = mLevels[bw].cbegin();

        if (iso > 0) {
            for (it = mLevels[bw].begin(); it != mLevels[bw].end(); ++it) {
                if (std::abs(it->first - iso) <= std::abs(best_it->first - iso)) {
                    best_it = it;
                } else {
                    break;
                }
            }
        }

        it = best_it;

        if (it == mLevels[bw].end()) {
            return false;
        }
    }

    lvl = it->second;

    if (bw == 1 && fnumber > 0 && !mApertureScaling.empty()) {
        std::map<float, float>::const_iterator scaleIt = mApertureScaling.find(fnumber);

        if (scaleIt == mApertureScaling.end()) {
            // fnumber may be an exact aperture, eg 1.414, or a rounded eg 1.4. In our map we
            // should have rounded numbers so we translate and retry the lookup

            // table with traditional 1/3 stop f-number rounding used by most cameras, we only
            // have in the range 0.7 - 10.0, but aperture scaling rarely happen past f/4.0
            constexpr float fn_tab[8][3] = {
                { 0.7f, 0.8f, 0.9f },
                { 1.f, 1.1f, 1.2f },
                { 1.4f, 1.6f, 1.8f },
                { 2.f, 2.2f, 2.5f },
                { 2.8f, 3.2f, 3.5f },
                { 4.f, 4.5f, 5.f },
                { 5.6f, 6.3f, 7.1f },
                { 8.f, 9.f, 10.f }
            };

            for (int avh = 0; avh < 8; avh++) {
                for (int k = 0; k < 3; k++) {
                    const float av = (avh - 1) + k / 3.f;
                    const float aperture = std::sqrt(std::pow(2.f, av));

                    if (fnumber > aperture * 0.97f && fnumber < aperture / 0.97f) {
                        fnumber = fn_tab[avh][k];
                        scaleIt = mApertureScaling.find(fnumber);
                        avh = 7;
                        break;
                    }
                }
            }
        }

        float scaling = 1.f;

        if (scaleIt == mApertureScaling.end()) {
            for (auto entry = mApertureScaling.crbegin(); entry != mApertureScaling.crend(); ++entry) {
                if (entry->first > fnumber) {
                    scaling = entry->second;
                } else {
                    break;
                }
            }
        } else {
            scaling = scaleIt->second;
        }

        if (scaling > 1.f) {
            for (int i = 0; i < 4; i++) {
                lvl.levels[i] *= scaling;

                if (white_max > 0 && lvl.levels[i] > white_max) {
                    lvl.levels[i] = white_max;
                }
            }
        }
    }

    return true;
}

int CameraConst::get_BlackLevel(const int idx, const int iso_speed) const
{
    assert(idx >= 0 && idx <= 3);
    camera_const_levels lvl;

    if (!get_Levels(lvl, 0, iso_speed, 0.f)) {
        return -1;
    }

    return lvl.levels[idx];
}

int CameraConst::get_WhiteLevel(const int idx, const int iso_speed, const float fnumber) const
{
    assert(idx >= 0 && idx <= 3);
    camera_const_levels lvl;

    if (!get_Levels(lvl, 1, iso_speed, fnumber)) {
        return -1;
    }

    return lvl.levels[idx];
}

bool CameraConst::has_globalGreenEquilibration() const
{
    return globalGreenEquilibration >= 0;
}

bool CameraConst::get_globalGreenEquilibration() const
{
    return globalGreenEquilibration > 0;
}

void CameraConst::update_globalGreenEquilibration(bool other)
{
    globalGreenEquilibration = (other ? 1 : 0);
}

bool CameraConstantsStore::parse_camera_constants_file(const Glib::ustring& filename_)
{
    // read the file into a single long string
    const char *filename = filename_.c_str();
    FILE *stream = fopen(filename, "rt");

    if (!stream) {
        fprintf(stderr, "Could not open camera constants file \"%s\": %s\n", filename, strerror(errno));
        return false;
    }

    size_t bufsize = 262144;
    size_t increment = bufsize;
    size_t datasize = 0, ret;
    char *buf = (char *)malloc(bufsize);

    while ((ret = fread(&buf[datasize], 1, bufsize - datasize - 1, stream)) != 0) {
        datasize += ret;

        if (datasize == bufsize - 1) { // we need more memory
            bufsize += increment;
            void *temp = realloc(buf, bufsize); // try to realloc buffer with new size
            if (!temp) { // realloc failed
                temp = malloc(bufsize); // alloc now buffer
                if (temp) { // alloc worked
                    memcpy(temp, buf, bufsize - increment); // copy old buffer content to new buffer
                    free(buf); // free old buffer
                } else { // alloc didn't work, break
                    break;
                }
            }
            buf = (char *)temp; // assign new buffer
            increment *= 2; // double increment
        }
    }

    if (!feof(stream)) {
        fclose(stream);
        free(buf);
        fprintf(stderr, "Failed to read camera constants file \"%s\"\n", filename);
        return false;
    }

    fclose(stream);

    buf[datasize] = '\0';

    // remove comments
    cJSON_Minify(buf);

    // parse
    cJSON* const jsroot = cJSON_Parse(buf);

    if (!jsroot) {
        char str[128];
        const char *ep = cJSON_GetErrorPtr() - 10;

        if ((uintptr_t)ep < (uintptr_t)buf) {
            ep = buf;
        }

        strncpy(str, ep, sizeof(str) - 1);
        str[sizeof(str) - 1] = '\0';
        fprintf(stderr, "JSON parse error in file \"%s\" near '%s'\n", filename, str);
        free(buf);
        return false;
    }

    free(buf);

    const cJSON *js = cJSON_GetObjectItem(jsroot, "camera_constants");

    if (!js) {
        fprintf(stderr, "missing \"camera_constants\" object item\n");
        goto parse_error;
    }

    for (js = js->child; js; js = js->next) {
        const cJSON *ji = cJSON_GetObjectItem(js, "make_model");

        if (!ji) {
            fprintf(stderr, "missing \"make_model\" object item\n");
            goto parse_error;
        }

        bool is_array = false;

        if (ji->type == cJSON_Array) {
            ji = ji->child;
            is_array = true;
        }

        while (ji) {
            if (ji->type != cJSON_String) {
                fprintf(stderr, "\"make_model\" must be a string or an array of strings\n");
                goto parse_error;
            }

            CameraConst* const cc = CameraConst::parseEntry((const void *)js, ji->valuestring);

            if (!cc) {
                goto parse_error;
            }

            std::string make_model(ji->valuestring);
            std::transform(make_model.begin(), make_model.end(), make_model.begin(), ::toupper);            

            const auto entry = mCameraConstants.emplace(make_model, cc);

            if (entry.second) { // entry inserted into map
                if (settings->verbose) {
                    printf("Add camera constants for \"%s\"\n", make_model.c_str());
                }
            } else {
                // The CameraConst already exist for this camera make/model -> we merge the values
                CameraConst* const existingcc = entry.first->second;

                // updating the dcraw matrix
                existingcc->update_dcrawMatrix(cc->get_dcrawMatrix());
                // deleting all the existing levels, replaced by the new ones
                existingcc->update_Levels(cc);
                existingcc->update_Crop(cc);
                existingcc->update_pdafPattern(cc->get_pdafPattern());
                existingcc->update_pdafOffset(cc->get_pdafOffset());
                if (cc->has_globalGreenEquilibration()) {
                    existingcc->update_globalGreenEquilibration(cc->get_globalGreenEquilibration());
                }

                if (settings->verbose) {
                    printf("Merging camera constants for \"%s\"\n", make_model.c_str());
                }
            }

            if (is_array) {
                ji = ji->next;
            } else {
                ji = nullptr;
            }
        }
    }

    cJSON_Delete(jsroot);
    return true;

parse_error:
    fprintf(stderr, "failed to parse camera constants file \"%s\"\n", filename);
    mCameraConstants.clear();
    cJSON_Delete(jsroot);
    return false;
}

CameraConstantsStore::CameraConstantsStore()
{
}


CameraConstantsStore::~CameraConstantsStore()
{
    for (auto &p : mCameraConstants) {
        delete p.second;
    }
}

void CameraConstantsStore::init(const Glib::ustring& baseDir, const Glib::ustring& userSettingsDir)
{
    // list of built-in files with camera constants. Besides camconst.json, we
    // now have 3 more locations where camera matrices are stored:
    //
    // - dcraw.json, with matrices copied from dcraw.cc
    // - rt.json, with matrices copied from rawimage.cc
    // - cammatrices.json, with matrices rebuilt from the Adobe DNG converter
    //
    // the first two are essentially for backwards compatibility only. We want
    // to store all the new matrices in cammatrices.json
    //
    // note that the order is relevant, later files ones override earlier ones
    static const char *builtin_files[] = {
        "dcraw.json",
        "rt.json",
        "camconst.json",
        "cammatrices.json"
    };
    for (size_t i = 0; i < sizeof(builtin_files)/sizeof(const char *); ++i) {
        Glib::ustring f(Glib::build_filename(baseDir, builtin_files[i]));
        if (Glib::file_test(f, Glib::FILE_TEST_EXISTS)) {
            parse_camera_constants_file(f);
        }
    }

    const Glib::ustring userFile(Glib::build_filename(userSettingsDir, "camconst.json"));

    if (Glib::file_test(userFile, Glib::FILE_TEST_EXISTS)) {
        parse_camera_constants_file(userFile);
    }
}

CameraConstantsStore* CameraConstantsStore::getInstance()
{
    static CameraConstantsStore instance_;
    return &instance_;
}

const CameraConst* CameraConstantsStore::get(const char make[], const char model[]) const
{
    std::string key(make);
    key += " ";
    key += model;
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);
    const auto it = mCameraConstants.find(key);

    if (it == mCameraConstants.end()) {
        return nullptr;
    }

    return it->second;
}

} // namespace rtengine
