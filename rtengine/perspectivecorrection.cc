/* -*- C++ -*-
 *
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Alberto Griggio <alberto.griggio@gmail.com>
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
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */

// taken from darktable (src/iop/ashift.c)
/*
  This file is part of darktable,
  copyright (c) 2016 Ulrich Pegelow.

  darktable is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  darktable is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
// Inspiration to this module comes from the program ShiftN (http://www.shiftn.de) by
// Marcus Hebel.

// Thanks to Marcus for his support when implementing part of the ShiftN functionality
// to darktable.


#include "perspectivecorrection.h"
#include "improcfun.h"
#include "procparams.h"
#include "rt_math.h"
#include <string.h>
#include <math.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "rtgui/threadutils.h"
#include "colortemp.h"
#include "imagefloat.h"
#include "settings.h"

namespace rtengine { extern const Settings *settings; }

#define _(msg) (msg)
#define dt_control_log(msg) \
    if (settings->verbose) { \
        printf("%s\n", msg);       \
        fflush(stdout);            \
    }


namespace rtengine {

namespace {

inline int mat3inv(float *const dst, const float *const src)
{
    std::array<std::array<float, 3>, 3> tmpsrc;
    std::array<std::array<float, 3>, 3> tmpdst;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            tmpsrc[i][j] = src[3 * i + j];
        }
    }
    if (invertMatrix(tmpsrc, tmpdst)) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                dst[3 * i + j] = tmpdst[i][j];
            }
        }
        return 0;
    } else {
        return 1;
    }
}


// the darktable ashift iop (adapted to RT), which does most of the work
#include "ashift_dt.c"


} // namespace


namespace {

/*
std::vector<Coord2D> get_corners(int w, int h)
{
    int x1 = 0, y1 = 0;
    int x2 = w, y2 = h;

    std::vector<Coord2D> corners = {
        Coord2D(x1, y1),
        Coord2D(x1, y2),
        Coord2D(x2, y2),
        Coord2D(x2, y1)
    };
    return corners;
}
*/

void init_dt_structures(dt_iop_ashift_params_t *p, dt_iop_ashift_gui_data_t *g,
                        const procparams::PerspectiveParams *params)
{
    dt_iop_ashift_params_t dp = {
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        DEFAULT_F_LENGTH,
        1.f,
        0.0f,
        1.0f,
        ASHIFT_MODE_SPECIFIC,
        0,
        ASHIFT_CROP_OFF,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f
    };
    *p = dp;

    g->buf = NULL;
    g->buf_width = 0;
    g->buf_height = 0;
    g->buf_x_off = 0;
    g->buf_y_off = 0;
    g->buf_scale = 1.0f;
    g->buf_hash = 0;
    g->isflipped = 0;
    g->lastfit = ASHIFT_FIT_NONE;
    g->fitting = 0;
    g->lines = NULL;
    g->lines_count =0;
    g->horizontal_count = 0;
    g->vertical_count = 0;
    g->grid_hash = 0;
    g->lines_hash = 0;
    g->rotation_range = ROTATION_RANGE_SOFT;
    g->lensshift_v_range = LENSSHIFT_RANGE_SOFT;
    g->lensshift_h_range = LENSSHIFT_RANGE_SOFT;
    g->shear_range = SHEAR_RANGE_SOFT;
    g->camera_pitch_range = CAMERA_ANGLE_RANGE_SOFT;
    g->camera_yaw_range = CAMERA_ANGLE_RANGE_SOFT;
    g->lines_suppressed = 0;
    g->lines_version = 0;
    g->show_guides = 0;
    g->isselecting = 0;
    g->isdeselecting = 0;
    g->isbounding = ASHIFT_BOUNDING_OFF;
    g->near_delta = 0;
    g->selecting_lines_version = 0;
    g->points = NULL;
    g->points_idx = NULL;
    g->points_lines_count = 0;
    g->points_version = 0;
    g->jobcode = ASHIFT_JOBCODE_NONE;
    g->jobparams = 0;
    g->adjust_crop = FALSE;
    g->lastx = g->lasty = -1.0f;
    g->crop_cx = g->crop_cy = 1.0f;

    if (params) {
        p->rotation = params->camera_roll;
        p->lensshift_v = params->camera_shift_vert;
        p->lensshift_h = params->camera_shift_horiz;
        p->f_length = params->camera_focal_length;
        p->crop_factor = params->camera_crop_factor;
        p->camera_pitch = params->camera_pitch;
        p->camera_yaw = params->camera_yaw;
    }
}


/*
void get_view_size(int w, int h, const procparams::PerspectiveParams &params, double &cw, double &ch)
{
    double min_x = RT_INFINITY, max_x = -RT_INFINITY;
    double min_y = RT_INFINITY, max_y = -RT_INFINITY;

    auto corners = get_corners(w, h);

    float homo[3][3];
    homography((float *)homo, params.angle, params.vertical / 100.0, -params.horizontal / 100.0, params.shear / 100.0, params.flength * params.cropfactor, 100.f, params.aspect, w, h, ASHIFT_HOMOGRAPH_FORWARD);
    
    for (auto &c : corners) {
        float pin[3] = { float(c.x), float(c.y), 1.f };
        float pout[3];
        mat3mulv(pout, (float *)homo, pin);
        double x = pout[0] / pout[2];
        double y = pout[1] / pout[2];
        min_x = min(min_x, x);
        max_x = max(max_x, x);
        min_y = min(min_y, y);
        max_y = max(max_y, y);
    }

    cw = max_x - min_x;
    ch = max_y - min_y;
}    
*/

/**
 * Allocates a new array and populates it with ashift lines corresponding to the
 * provided control lines.
 */
std::unique_ptr<dt_iop_ashift_line_t[]> toAshiftLines(const std::vector<ControlLine> *lines)
{
    std::unique_ptr<dt_iop_ashift_line_t[]> retval(new dt_iop_ashift_line_t[lines->size()]);

    for (size_t i = 0; i < lines->size(); i++) {
        const float x1 = (*lines)[i].x1;
        const float y1 = (*lines)[i].y1;
        const float x2 = (*lines)[i].x2;
        const float y2 = (*lines)[i].y2;
        retval[i].p1[0] = x1;
        retval[i].p1[1] = y1;
        retval[i].p1[2] = 1.0f;
        retval[i].p2[0] = x2;
        retval[i].p2[1] = y2;
        retval[i].p2[2] = 1.0f;
        retval[i].length = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
        retval[i].width = 1.0f;
        retval[i].weight = retval[i].length;
        if ((*lines)[i].type == ControlLine::HORIZONTAL) {
            retval[i].type = ASHIFT_LINE_HORIZONTAL_SELECTED;
        } else if ((*lines)[i].type == ControlLine::VERTICAL) {
            retval[i].type = ASHIFT_LINE_VERTICAL_SELECTED;
        } else {
            retval[i].type = ASHIFT_LINE_IRRELEVANT;
        }
    }

    return retval;
}

} // namespace


PerspectiveCorrection::Params PerspectiveCorrection::autocompute(ImageSource *src, bool corr_pitch, bool corr_yaw, const procparams::ProcParams *pparams, const FramesMetaData *metadata, const std::vector<ControlLine> *control_lines)
{
    auto pcp = procparams::PerspectiveParams(pparams->perspective);
    procparams::PerspectiveParams dflt;
    /*
    pcp.horizontal = dflt.horizontal;
    pcp.vertical = dflt.vertical;
    pcp.angle = dflt.angle;
    pcp.shear = dflt.shear;
    */
    pcp.camera_pitch = dflt.camera_pitch;
    pcp.camera_roll = dflt.camera_roll;
    pcp.camera_yaw = dflt.camera_yaw;
    
    dt_iop_ashift_params_t p;
    dt_iop_ashift_gui_data_t g;
    init_dt_structures(&p, &g, &pparams->perspective);
    dt_iop_module_t module;
    module.gui_data = &g;
    module.is_raw = src->isRAW();

    int tr = getCoarseBitMask(pparams->coarse);
    int fw, fh;
    src->getFullSize(fw, fh, tr);
    if (control_lines == nullptr) {
        int skip = max(float(max(fw, fh)) / 900.f + 0.5f, 1.f);
        PreviewProps pp(0, 0, fw, fh, skip);
        int w, h;
        src->getSize(pp, w, h);
        std::unique_ptr<Imagefloat> img(new Imagefloat(w, h));

        ProcParams neutral;
        neutral.raw.bayersensor.method = RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::FAST);
        neutral.raw.xtranssensor.method = RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::FAST);
        neutral.icm.outputProfile = ColorManagementParams::NoICMString;    
        src->getImage(src->getWB(), tr, img.get(), pp, neutral.toneCurve, neutral.raw);
        src->convertColorSpace(img.get(), pparams->icm, src->getWB());

        neutral.commonTrans.autofill = false; // Ensures crop factor is correct.
        // TODO: Ensure image borders of rotated image do not get detected as lines.
        neutral.coarse = pparams->coarse; // getImage() has already applied this orientation.
        neutral.rotate = pparams->rotate;
        neutral.distortion = pparams->distortion;
        neutral.distortion.defish = pparams->distortion.defish;
        neutral.distortion.focal_length = pparams->distortion.focal_length;
        neutral.perspective.camera_focal_length = pparams->perspective.camera_focal_length;
        neutral.perspective.camera_crop_factor = pparams->perspective.camera_crop_factor;
        neutral.perspective.method = pparams->perspective.method;
        neutral.lensProf = pparams->lensProf;
        ImProcFunctions ipf(&neutral, true);
        if (ipf.needsTransform(w, h, src->getRotateDegree(), src->getMetaData())) {
            Imagefloat *tmp = new Imagefloat(w, h);
            ipf.transform(img.get(), tmp, 0, 0, 0, 0, w, h, w, h,
                    src->getMetaData(), src->getRotateDegree(), skip);
            img.reset(tmp);
        }

        // allocate the gui buffer
        g.buf = static_cast<float *>(malloc(sizeof(float) * w * h * 4));
        g.buf_width = w;
        g.buf_height = h;

        img->normalizeFloatTo1();

#ifdef _OPENMP
#   pragma omp parallel for
#endif
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = (y * w + x) * 4;
                g.buf[i] = img->r(y, x);
                g.buf[i+1] = img->g(y, x);
                g.buf[i+2] = img->b(y, x);
                g.buf[i+3] = 1.f;
            }
        }
    }

    dt_iop_ashift_fitaxis_t fitaxis = ASHIFT_FIT_NONE;
    if (corr_pitch && corr_yaw) {
        fitaxis = ASHIFT_FIT_BOTH_SHEAR;
    } else if (corr_pitch) {
        fitaxis = ASHIFT_FIT_VERTICALLY;
    } else if (corr_yaw) {
        fitaxis = ASHIFT_FIT_HORIZONTALLY;
    }

    // reset the pseudo-random seed for repeatability -- ashift_dt uses rand()
    // internally!
    srand(1);
    
    bool res;
    if (control_lines == nullptr) {
        res = do_get_structure(&module, &p, ASHIFT_ENHANCE_EDGES) && do_fit(&module, &p, fitaxis);
    } else {
        std::unique_ptr<dt_iop_ashift_line_t[]> ashift_lines = toAshiftLines(control_lines);
        dt_iop_ashift_gui_data_t *g = module.gui_data;
        g->lines_count = control_lines->size();
        g->lines = ashift_lines.get();
        g->lines_in_height = fh;
        g->lines_in_width = fw;
        update_lines_count(g->lines, g->lines_count, &(g->vertical_count), &(g->horizontal_count));
        res = do_fit(&module, &p, fitaxis, 2);
        g->lines = nullptr;
    }
    Params retval = {
        .angle = p.rotation,
        .pitch = p.camera_pitch,
        .yaw = p.camera_yaw
    };

    // cleanup the gui
    if (g.lines) free(g.lines);
    if (g.points) free(g.points);
    if (g.points_idx) free(g.points_idx);
    if (g.buf) free(g.buf);

    if (!res) {
        retval.angle = pparams->perspective.camera_roll;
        retval.pitch = pparams->perspective.camera_pitch;
        retval.yaw = pparams->perspective.camera_yaw;
    }
    return retval;
}


/*
void PerspectiveCorrection::autocrop(int width, int height, bool fixratio, const procparams::PerspectiveParams &params, const FramesMetaData *metadata, int &x, int &y, int &w, int &h)
{
    auto pp = import_meta(params, metadata);
    double cw, ch;
    get_view_size(width, height, params, cw, ch);
    double s = min(double(width)/cw, double(height)/ch);
    dt_iop_ashift_params_t p;
    dt_iop_ashift_gui_data_t g;
    init_dt_structures(&p, &g, &pp);
    dt_iop_module_t module = { &g, false };
    g.buf_width = width;
    g.buf_height = height;
    p.cropmode = fixratio ? ASHIFT_CROP_ASPECT : ASHIFT_CROP_LARGEST;
    do_crop(&module, &p);
    cw *= s;
    ch *= s;
    double ox = p.cl * cw;
    double oy = p.ct * ch;
    x = ox - (cw - width)/2.0 + 0.5;
    y = oy - (ch - height)/2.0 + 0.5;
    w = (p.cr - p.cl) * cw;
    h = (p.cb - p.ct) * ch;
}
*/

} // namespace rtengine
