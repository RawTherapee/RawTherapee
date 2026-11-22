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

#include "imagefloat.h"
#include "improcfun.h"

#include "homogeneouscoordinates.h"
#include "procparams.h"
#include "rt_math.h"
#include "rtengine.h"
#include "rtlensfun.h"
#include "lensmetadata.h"
#include "sleef.h"

using namespace std;

namespace
{

float pow3 (float x)
{
    return x * x * x;
}

float pow4 (float x)
{
    return (x * x) * (x * x);
}

float pown (float x, int n)
{

    switch (n) {
        case 0:
            return 1;

        case 2:
            return x * x;

        case 4:
            return pow4 (x);

        case 6:
            return (x * x) * pow4 (x);

        case 8:
            return pow4 (x) * pow4 (x);

        default:
            return pow_F (x, n);
    }
}

float normn (float a, float b, int n)
{
    switch (n) {
        case 2:
            return sqrtf (a * a + b * b);

        case 4:
            return sqrtf (sqrtf (pow4 (a) + pow4 (b)));

        case 6:
            return sqrtf (xcbrtf (pow3 (a) * pow3 (a) + pow3 (b) * pow3 (b)));

        case 8:
            return sqrtf (sqrtf (sqrtf (pow4 (a) * pow4 (a) + pow4 (b) * pow4 (b))));

        default:
            return pow_F (pown (a, n) + pown (b, n), 1.f / n);
    }
}

void logEncode(rtengine::Imagefloat *src, rtengine::Imagefloat *dest, bool multiThread) {

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 16) if(multiThread)
#endif

    for (int y = 0; y < src->getHeight(); ++y) {
        int x = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
        for (; x < src->getWidth() - 3; x += 4) {
            STVFU(dest->r(y, x), xlogf1(LVFU(src->r(y, x))));
            STVFU(dest->g(y, x), xlogf1(LVFU(src->g(y, x))));
            STVFU(dest->b(y, x), xlogf1(LVFU(src->b(y, x))));
        }
#endif
        for (; x < src->getWidth(); ++x) {
            dest->r(y, x) = xlogf1(src->r(y, x));
            dest->g(y, x) = xlogf1(src->g(y, x));
            dest->b(y, x) = xlogf1(src->b(y, x));
        }
    }
}

#if defined(__SSE2__) || defined(RT_SIMDE)
inline void interpolateTransformCubic(rtengine::Imagefloat* src, int xs, int ys, float Dx, float Dy, float &r, float &g, float &b, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const vfloat w3Vert = F2V(t1Vert * Dy);
    const vfloat w2Vert = F2V(t1Vert * Dy - t1Vert + t2Vert);
    const vfloat w1Vert = F2V(1.f - (t1Vert * Dy) - t2Vert);
    const vfloat w0Vert = F2V(t1Vert - (t1Vert * Dy));

    const vfloat rv = (w0Vert * LVFU(src->r(ys, xs)) + w1Vert * LVFU(src->r(ys + 1, xs))) + (w2Vert * LVFU(src->r(ys + 2, xs)) + w3Vert * LVFU(src->r(ys + 3, xs)));
    const vfloat gv = (w0Vert * LVFU(src->g(ys, xs)) + w1Vert * LVFU(src->g(ys + 1, xs))) + (w2Vert * LVFU(src->g(ys + 2, xs)) + w3Vert * LVFU(src->g(ys + 3, xs)));
    const vfloat bv = (w0Vert * LVFU(src->b(ys, xs)) + w1Vert * LVFU(src->b(ys + 1, xs))) + (w2Vert * LVFU(src->b(ys + 2, xs)) + w3Vert * LVFU(src->b(ys + 3, xs)));

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const vfloat weight = _mm_set_ps(t1Hor * Dx, t1Hor * Dx - t1Hor + t2Hor, 1.f - (t1Hor * Dx) - t2Hor, t1Hor - (t1Hor * Dx)) * F2V(mul);
    r = vhadd(weight * rv);
    g = vhadd(weight * gv);
    b = vhadd(weight * bv);
}

inline void interpolateTransformCubicLog(rtengine::Imagefloat* src, int xs, int ys, float Dx, float Dy, float &r, float &g, float &b, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const vfloat w3Vert = F2V(t1Vert * Dy);
    const vfloat w2Vert = F2V(t1Vert * Dy - t1Vert + t2Vert);
    const vfloat w1Vert = F2V(1.f - (t1Vert * Dy) - t2Vert);
    const vfloat w0Vert = F2V(t1Vert - (t1Vert * Dy));

    const vfloat rv = (w0Vert * LVFU(src->r(ys, xs)) + w1Vert * LVFU(src->r(ys + 1, xs))) + (w2Vert * LVFU(src->r(ys + 2, xs)) + w3Vert * LVFU(src->r(ys + 3, xs)));
    const vfloat gv = (w0Vert * LVFU(src->g(ys, xs)) + w1Vert * LVFU(src->g(ys + 1, xs))) + (w2Vert * LVFU(src->g(ys + 2, xs)) + w3Vert * LVFU(src->g(ys + 3, xs)));
    const vfloat bv = (w0Vert * LVFU(src->b(ys, xs)) + w1Vert * LVFU(src->b(ys + 1, xs))) + (w2Vert * LVFU(src->b(ys + 2, xs)) + w3Vert * LVFU(src->b(ys + 3, xs)));

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const vfloat weight = _mm_set_ps(t1Hor * Dx, t1Hor * Dx - t1Hor + t2Hor, 1.f - (t1Hor * Dx) - t2Hor, t1Hor - (t1Hor * Dx));
    const vfloat tempv = _mm_setr_ps(vhadd(weight * rv), vhadd(weight * gv), vhadd(weight * bv), 0.f);
    const vfloat resultv = xexpf(tempv);
    r = mul * resultv[0];
    g = mul * resultv[1];
    b = mul * resultv[2];
}
#else
inline void interpolateTransformCubic(rtengine::Imagefloat* src, int xs, int ys, float Dx, float Dy, float &r, float &g, float &b, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const float w3Vert = t1Vert * Dy;
    const float w2Vert = t1Vert * Dy - t1Vert + t2Vert;
    const float w1Vert = 1.f - (t1Vert * Dy) - t2Vert;
    const float w0Vert = t1Vert - (t1Vert * Dy);

    float rv[4], gv[4], bv[4];
    for (int i = 0; i < 4; ++i) {
        rv[i] = w0Vert * src->r(ys, xs + i) + w1Vert * src->r(ys + 1, xs + i) + w2Vert * src->r(ys + 2, xs + i) + w3Vert * src->r(ys + 3, xs + i);
        gv[i] = w0Vert * src->g(ys, xs + i) + w1Vert * src->g(ys + 1, xs + i) + w2Vert * src->g(ys + 2, xs + i) + w3Vert * src->g(ys + 3, xs + i);
        bv[i] = w0Vert * src->b(ys, xs + i) + w1Vert * src->b(ys + 1, xs + i) + w2Vert * src->b(ys + 2, xs + i) + w3Vert * src->b(ys + 3, xs + i);
    }

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const float w3Hor = t1Hor * Dx;
    const float w2Hor = t1Hor * Dx - t1Hor + t2Hor;
    const float w1Hor = 1.f - (t1Hor * Dx) - t2Hor;
    const float w0Hor = t1Hor - (t1Hor * Dx);

    r = mul * (rv[0] * w0Hor + rv[1] * w1Hor + rv[2] * w2Hor + rv[3] * w3Hor);
    g = mul * (gv[0] * w0Hor + gv[1] * w1Hor + gv[2] * w2Hor + gv[3] * w3Hor);
    b = mul * (bv[0] * w0Hor + bv[1] * w1Hor + bv[2] * w2Hor + bv[3] * w3Hor);
}

inline void interpolateTransformCubicLog(rtengine::Imagefloat* src, int xs, int ys, float Dx, float Dy, float &r, float &g, float &b, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const float w3Vert = t1Vert * Dy;
    const float w2Vert = t1Vert * Dy - t1Vert + t2Vert;
    const float w1Vert = 1.f - (t1Vert * Dy) - t2Vert;
    const float w0Vert = t1Vert - (t1Vert * Dy);

    float rv[4], gv[4], bv[4];
    for (int i = 0; i < 4; ++i) {
        rv[i] = w0Vert * src->r(ys, xs + i) + w1Vert * src->r(ys + 1, xs + i) + w2Vert * src->r(ys + 2, xs + i) + w3Vert * src->r(ys + 3, xs + i);
        gv[i] = w0Vert * src->g(ys, xs + i) + w1Vert * src->g(ys + 1, xs + i) + w2Vert * src->g(ys + 2, xs + i) + w3Vert * src->g(ys + 3, xs + i);
        bv[i] = w0Vert * src->b(ys, xs + i) + w1Vert * src->b(ys + 1, xs + i) + w2Vert * src->b(ys + 2, xs + i) + w3Vert * src->b(ys + 3, xs + i);
    }

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const float w3Hor = t1Hor * Dx;
    const float w2Hor = t1Hor * Dx - t1Hor + t2Hor;
    const float w1Hor = 1.f - (t1Hor * Dx) - t2Hor;
    const float w0Hor = t1Hor - (t1Hor * Dx);

    r = mul * xexpf(rv[0] * w0Hor + rv[1] * w1Hor + rv[2] * w2Hor + rv[3] * w3Hor);
    g = mul * xexpf(gv[0] * w0Hor + gv[1] * w1Hor + gv[2] * w2Hor + gv[3] * w3Hor);
    b = mul * xexpf(bv[0] * w0Hor + bv[1] * w1Hor + bv[2] * w2Hor + bv[3] * w3Hor);
}
#endif
#if defined(__SSE2__) || defined(RT_SIMDE)
inline void interpolateTransformChannelsCubic(const float* const* src, int xs, int ys, float Dx, float Dy, float& dest, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const vfloat w3Vert = F2V(t1Vert * Dy);
    const vfloat w2Vert = F2V(t1Vert * Dy - t1Vert + t2Vert);
    const vfloat w1Vert = F2V(1.f - (t1Vert * Dy) - t2Vert);
    const vfloat w0Vert = F2V(t1Vert - (t1Vert * Dy));

    const vfloat cv = (w0Vert * LVFU(src[ys][xs]) + w1Vert * LVFU(src[ys + 1][xs])) + (w2Vert * LVFU(src[ys + 2][xs]) + w3Vert * LVFU(src[ys + 3][xs]));

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const vfloat weight = _mm_set_ps(t1Hor * Dx, t1Hor * Dx - t1Hor + t2Hor, 1.f - (t1Hor * Dx) - t2Hor, t1Hor - (t1Hor * Dx));
    dest = mul * vhadd(weight * cv);
}

inline void interpolateTransformChannelsCubicLog(const float* const* src, int xs, int ys, float Dx, float Dy, float& dest, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const vfloat w3Vert = F2V(t1Vert * Dy);
    const vfloat w2Vert = F2V(t1Vert * Dy - t1Vert + t2Vert);
    const vfloat w1Vert = F2V(1.f - (t1Vert * Dy) - t2Vert);
    const vfloat w0Vert = F2V(t1Vert - (t1Vert * Dy));

    const vfloat cv = (w0Vert * LVFU(src[ys][xs]) + w1Vert * LVFU(src[ys + 1][xs])) + (w2Vert * LVFU(src[ys + 2][xs]) + w3Vert * LVFU(src[ys + 3][xs]));

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const vfloat weight = _mm_set_ps(t1Hor * Dx, t1Hor * Dx - t1Hor + t2Hor, 1.f - (t1Hor * Dx) - t2Hor, t1Hor - (t1Hor * Dx));
    dest = mul * xexpf(vhadd(weight * cv));
}
#else
inline void interpolateTransformChannelsCubic(const float* const* src, int xs, int ys, float Dx, float Dy, float& dest, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const float w3Vert = t1Vert * Dy;
    const float w2Vert = t1Vert * Dy - t1Vert + t2Vert;
    const float w1Vert = 1.f - (t1Vert * Dy) - t2Vert;
    const float w0Vert = t1Vert - (t1Vert * Dy);

    float cv[4];
    for (int i = 0; i < 4; ++i) {
        cv[i] = w0Vert * src[ys][xs + i] + w1Vert * src[ys + 1][xs + i] + w2Vert * src[ys + 2][xs + i] + w3Vert * src[ys + 3][xs + i];
    }

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const float w3Hor = t1Hor * Dx;
    const float w2Hor = t1Hor * Dx - t1Hor + t2Hor;
    const float w1Hor = 1.f - (t1Hor * Dx) - t2Hor;
    const float w0Hor = t1Hor - (t1Hor * Dx);

    dest = mul * (cv[0] * w0Hor + cv[1] * w1Hor + cv[2] * w2Hor + cv[3] * w3Hor);
}

inline void interpolateTransformChannelsCubicLog(const float* const* src, int xs, int ys, float Dx, float Dy, float& dest, float mul)
{
    constexpr float A = -0.85f;

    // Vertical
    const float t1Vert = A * (Dy - Dy * Dy);
    const float t2Vert = (3.f - 2.f * Dy) * Dy * Dy;
    const float w3Vert = t1Vert * Dy;
    const float w2Vert = t1Vert * Dy - t1Vert + t2Vert;
    const float w1Vert = 1.f - (t1Vert * Dy) - t2Vert;
    const float w0Vert = t1Vert - (t1Vert * Dy);

    float cv[4];
    for (int i = 0; i < 4; ++i) {
        cv[i] = w0Vert * src[ys][xs + i] + w1Vert * src[ys + 1][xs + i] + w2Vert * src[ys + 2][xs + i] + w3Vert * src[ys + 3][xs + i];
    }

    // Horizontal
    const float t1Hor = A * (Dx - Dx * Dx);
    const float t2Hor = (3.f - 2.f * Dx) * Dx * Dx;
    const float w3Hor = t1Hor * Dx;
    const float w2Hor = t1Hor * Dx - t1Hor + t2Hor;
    const float w1Hor = 1.f - (t1Hor * Dx) - t2Hor;
    const float w0Hor = t1Hor - (t1Hor * Dx);

    dest = mul * xexpf(cv[0] * w0Hor + cv[1] * w1Hor + cv[2] * w2Hor + cv[3] * w3Hor);
}
#endif

}

namespace rtengine
{
#undef CLIPTOC

#define CLIPTOC(a,b,c,d) ((a)>=(b)?((a)<=(c)?(a):(d=true,(c))):(d=true,(b)))

template<class A, class B, class C>
void track_min_max(A & min, B & max, C const val)
{
    if (val < min)
        min = val;
    if (val > max)
        max = val;
}

/**
 * Creates an inverse transformation matrix for camera-geometry-based
 * perspective correction. Unless otherwise specified, units are the same as the
 * units of the vectors which the matrix will transform. The projection_*
 * parameters are applied in the order they appear.
 * @param camera_focal_length Camera's focal length.
 * @param camera_shift_horiz Camera lens's shift to the right.
 * @param camera_shift_vert Camera lens's shift upwards.
 * @param camera_roll Camera's roll in radians. Counter-clockwise is positive.
 * @param camera_pitch Camera's pitch in radians. Up is positive.
 * @param camera_yaw Camera's yaw in radians. Right is positive.
 * Up is positive.
 * @param projection_shift_horiz Shift of perspective-corrected image to the
 * right.
 * @param projection_shift_vert Shift of perspective-corrected image upwards.
 * @param projection_rotate Rotation of perspective-corrected image
 * counter-clockwise in radians.
 * @param projection_yaw Yaw in radians of simulated perspective distortion.
 * Right is positive.
 * @param projection_pitch Pitch in radians of simulated perspective distortion.
 * Up is positive.
 * @param projection_scale Scale factor of perspective-corrected image.
 */
homogeneous::Matrix<double> perspectiveMatrix(double camera_focal_length, double
        camera_shift_horiz, double camera_shift_vert, double camera_roll, double
        camera_pitch, double camera_yaw, double projection_yaw, double
        projection_pitch, double projection_rotate, double
        projection_shift_horiz, double projection_shift_vert, double
        projection_scale)
{
    const double projection_scale_inverse = 1.0 / projection_scale;
    homogeneous::Vector<double> center;
    center[0] = 0;
    center[1] = 0;
    center[2] = camera_focal_length;
    center[3] = 1;

    // Locations of image center after rotations.
    const homogeneous::Vector<double> camera_center_yaw_pitch =
        homogeneous::rotationMatrix<double>(camera_yaw, homogeneous::Axis::Y) *
        homogeneous::rotationMatrix<double>(camera_pitch, homogeneous::Axis::X) *
        center;
    const homogeneous::Vector<double> projection_center_yaw_pitch =
        homogeneous::rotationMatrix<double>(-projection_yaw, homogeneous::Axis::Y) *
        homogeneous::rotationMatrix<double>(-projection_pitch, homogeneous::Axis::X) *
        center;

    // The following comments refer to the forward transformation.
    const homogeneous::Matrix<double> matrix =
        // Lens/sensor shift and move to z == camera_focal_length.
        homogeneous::translationMatrix<double>(-camera_shift_horiz,
                -camera_shift_vert, -camera_focal_length) *
        // Camera roll.
        homogeneous::rotationMatrix<double>(camera_roll, homogeneous::Axis::Z) *
        // Perspective correction.
        homogeneous::projectionMatrix<double>(camera_focal_length, homogeneous::Axis::Z) *
        homogeneous::rotationMatrix<double>(-camera_pitch, homogeneous::Axis::X) *
        homogeneous::rotationMatrix<double>(-camera_yaw, homogeneous::Axis::Y) *
        // Re-center after perspective rotation.
        homogeneous::translationMatrix<double>(camera_center_yaw_pitch[0],
                camera_center_yaw_pitch[1], camera_center_yaw_pitch[2] - camera_focal_length) *
        // Translate corrected image.
        homogeneous::translationMatrix<double>(-projection_shift_horiz,
                -projection_shift_vert, 0) *
        // Rotate corrected image.
        homogeneous::rotationMatrix<double>(projection_rotate, homogeneous::Axis::Z) *
        // Un-center for perspective rotation.
        homogeneous::translationMatrix<double>(projection_center_yaw_pitch[0],
                projection_center_yaw_pitch[1], camera_focal_length - projection_center_yaw_pitch[2]) *
        // Simulate perspective transformation.
        homogeneous::projectionMatrix<double>(projection_center_yaw_pitch[2], homogeneous::Axis::Z) *
        homogeneous::rotationMatrix<double>(projection_yaw, homogeneous::Axis::Y) *
        homogeneous::rotationMatrix<double>(projection_pitch, homogeneous::Axis::X) *
        // Move to z == 0.
        homogeneous::translationMatrix<double>(0, 0, camera_focal_length) *
        // Scale corrected image.
        homogeneous::scaleMatrix<double>(projection_scale_inverse,
                projection_scale_inverse, 1);

    return matrix;
}

bool ImProcFunctions::transCoord (int W, int H, const std::vector<Coord2D> &src, std::vector<Coord2D> &red,  std::vector<Coord2D> &green, std::vector<Coord2D> &blue, double ascaleDef,
                                  const LensCorrection *pLCPMap) const
{
    enum PerspType { NONE, SIMPLE, CAMERA_BASED };
    const PerspType perspectiveType = needsPerspective() ? (
            (params->perspective.method == "camera_based") ?
            PerspType::CAMERA_BASED : PerspType::SIMPLE ) : PerspType::NONE;
    bool clipped = false;

    red.clear ();
    green.clear ();
    blue.clear ();

    if (!needsCA() && !needsDistortion() && !needsRotation() && !needsPerspective() && !needsScaleAny() && (!params->lensProf.useDist || pLCPMap == nullptr)) {
        for (size_t i = 0; i < src.size(); i++) {
            red.push_back   (Coord2D (src[i].x, src[i].y));
            green.push_back (Coord2D (src[i].x, src[i].y));
            blue.push_back  (Coord2D (src[i].x, src[i].y));
        }

        return false;
    }

    const double oW = W, oH = H;
    const double w2 = (double) oW  / 2.0 - 0.5;
    const double h2 = (double) oH  / 2.0 - 0.5;
    const double maxRadius = sqrt ( (double) ( oW * oW + oH * oH ) ) / 2;

    // auxiliary variables for distortion correction
    const bool needsDist = needsDistortion();  // for performance
    const double distAmount = params->distortion.amount;

    // auxiliary variables for rotation
    const double cost = cos (params->rotate.degree * rtengine::RT_PI / 180.0);
    const double sint = sin (params->rotate.degree * rtengine::RT_PI / 180.0);

    const double ascale = ascaleDef > 0 ? ascaleDef : (params->commonTrans.autofill && params->perspective.render ? getTransformAutoFill (oW, oH, pLCPMap) : 1.0 / params->commonTrans.getScale());
    const double hscale = ascale / params->commonTrans.getScaleHorizontally();
    const double vscale = ascale / params->commonTrans.getScaleVertically();

    // auxiliary variables for perspective correction
    // Simple.
    const double vpdeg = params->perspective.vertical / 100.0 * 45.0;
    const double vpalpha = (90.0 - vpdeg) / 180.0 * rtengine::RT_PI;
    const double vpteta  = fabs (vpalpha - rtengine::RT_PI / 2) < 3e-4 ? 0.0 : acos ((vpdeg > 0 ? 1.0 : -1.0) * sqrt ((-oW * oW * tan (vpalpha) * tan (vpalpha) + (vpdeg > 0 ? 1.0 : -1.0) * oW * tan (vpalpha) * sqrt (16 * maxRadius * maxRadius + oW * oW * tan (vpalpha) * tan (vpalpha))) / (maxRadius * maxRadius * 8)));
    const double vpcospt = (vpdeg >= 0 ? 1.0 : -1.0) * cos (vpteta), vptanpt = tan (vpteta);
    const double hpdeg = params->perspective.horizontal / 100.0 * 45.0;
    const double hpalpha = (90.0 - hpdeg) / 180.0 * rtengine::RT_PI;
    const double hpteta  = fabs (hpalpha - rtengine::RT_PI / 2) < 3e-4 ? 0.0 : acos ((hpdeg > 0 ? 1.0 : -1.0) * sqrt ((-oH * oH * tan (hpalpha) * tan (hpalpha) + (hpdeg > 0 ? 1.0 : -1.0) * oH * tan (hpalpha) * sqrt (16 * maxRadius * maxRadius + oH * oH * tan (hpalpha) * tan (hpalpha))) / (maxRadius * maxRadius * 8)));
    const double hpcospt = (hpdeg >= 0 ? 1.0 : -1.0) * cos (hpteta), hptanpt = tan (hpteta);
    // Camera-based.
    const double f =
            ((params->perspective.camera_focal_length > 0) ? params->perspective.camera_focal_length : PerspectiveParams::DEFAULT_CAMERA_FOCAL_LENGTH)
            * ((params->perspective.camera_crop_factor > 0) ? params->perspective.camera_crop_factor : PerspectiveParams::DEFAULT_CAMERA_CROP_FACTOR)
            * (maxRadius / sqrt(18.0*18.0 + 12.0*12.0));
    const double f_defish =
            ((params->distortion.focal_length > 0) ? params->distortion.focal_length : DistortionParams::DEFAULT_FOCAL_LENGTH)
            * (maxRadius / sqrt(18.0*18.0 + 12.0*12.0));
    const double p_camera_yaw = params->perspective.camera_yaw / 180.0 * rtengine::RT_PI;
    const double p_camera_pitch = params->perspective.camera_pitch / 180.0 * rtengine::RT_PI;
    const double p_camera_roll = params->perspective.camera_roll * rtengine::RT_PI_180;
    const double p_camera_shift_horiz = oW / 100.0 * params->perspective.camera_shift_horiz;
    const double p_camera_shift_vert = oH / -100.0 * params->perspective.camera_shift_vert;
    const double p_projection_shift_horiz = oW / 100.0 * params->perspective.projection_shift_horiz;
    const double p_projection_shift_vert = oH / -100.0 * params->perspective.projection_shift_vert;
    const double p_projection_rotate = params->perspective.projection_rotate * rtengine::RT_PI_180;
    const double p_projection_yaw = -params->perspective.projection_yaw * rtengine::RT_PI_180;
    const double p_projection_pitch = -params->perspective.projection_pitch * rtengine::RT_PI_180;
    const double p_projection_scale = 1;
    const homogeneous::Matrix<double> p_matrix = perspectiveMatrix(f,
            p_camera_shift_horiz, p_camera_shift_vert, p_camera_roll,
            p_camera_pitch, p_camera_yaw, p_projection_yaw, p_projection_pitch,
            p_projection_rotate, p_projection_shift_horiz,
            p_projection_shift_vert, p_projection_scale);

    for (size_t i = 0; i < src.size(); i++) {
        double x_d = src[i].x, y_d = src[i].y;

        x_d = hscale * (x_d - w2);     // centering x coord & scale
        y_d = vscale * (y_d - h2);     // centering y coord & scale

        switch (perspectiveType) {
            case PerspType::NONE:
                break;
            case PerspType::SIMPLE:
                // horizontal perspective transformation
                y_d *= maxRadius / (maxRadius + x_d * hptanpt);
                x_d *= maxRadius * hpcospt / (maxRadius + x_d * hptanpt);

                // vertical perspective transformation
                x_d *= maxRadius / (maxRadius - y_d * vptanpt);
                y_d *= maxRadius * vpcospt / (maxRadius - y_d * vptanpt);
                break;
            case PerspType::CAMERA_BASED:
                const double w = p_matrix[3][0] * x_d + p_matrix[3][1] * y_d + p_matrix[3][3];
                const double xw = p_matrix[0][0] * x_d + p_matrix[0][1] * y_d + p_matrix[0][3];
                const double yw = p_matrix[1][0] * x_d + p_matrix[1][1] * y_d + p_matrix[1][3];
                x_d = xw / w;
                y_d = yw / w;
                break;
        }

        if (params->distortion.defish) {
            x_d /= f_defish;
            y_d /= f_defish;

            const double r = std::sqrt(x_d * x_d + y_d * y_d);

            const double factor = f_defish * std::atan(r) / r;

            x_d *= factor;
            y_d *= factor;
        }

        // rotate
        double Dx = x_d * cost - y_d * sint;
        double Dy = x_d * sint + y_d * cost;

        if (pLCPMap && params->lensProf.useDist && pLCPMap->hasDistortionCorrection()) {
            pLCPMap->correctDistortion(Dx, Dy, w2, h2);
        }

        // distortion correction
        double s = 1;

        if (needsDist) {
            double r = sqrt (Dx * Dx + Dy * Dy) / maxRadius; // sqrt is slow
            s = 1.0 - distAmount + distAmount * r ;
        }

        // LCP CA is not reflected in preview (and very small), so don't add it here

        red.push_back (Coord2D (Dx * (s + params->cacorrection.red) + w2, Dy * (s + params->cacorrection.red) + h2));
        green.push_back (Coord2D (Dx * s + w2, Dy * s + h2));
        blue.push_back (Coord2D (Dx * (s + params->cacorrection.blue) + w2, Dy * (s + params->cacorrection.blue) + h2));
    }

    // Clip all points and track if they were any corrections
    for (size_t i = 0; i < src.size(); i++) {
        red[i].x = CLIPTOC (red[i].x, 0, W - 1, clipped);
        red[i].y = CLIPTOC (red[i].y, 0, H - 1, clipped);
        green[i].x = CLIPTOC (green[i].x, 0, W - 1, clipped);
        green[i].y = CLIPTOC (green[i].y, 0, H - 1, clipped);
        blue[i].x = CLIPTOC (blue[i].x, 0, W - 1, clipped);
        blue[i].y = CLIPTOC (blue[i].y, 0, H - 1, clipped);
    }

    return clipped;
}

// Transform all corners and critical sidelines of an image
bool ImProcFunctions::transCoord (int W, int H, int x, int y, int w, int h, int& xv, int& yv, int& wv, int& hv, double ascaleDef, const LensCorrection *pLCPMap) const
{
    const int DivisionsPerBorder = 32;

    int x1 = x, y1 = y;
    int x2 = x1 + w - 1;
    int y2 = y1 + h - 1;

    // Build all edge points and half-way points
    std::vector<Coord2D> corners (4);
    corners[0].set (x1, y1);
    corners[1].set (x1, y2);
    corners[2].set (x2, y2);
    corners[3].set (x2, y1);

    // Add several steps inbetween
    const int xstep = std::max(1, int((x2 - x1) / DivisionsPerBorder));

    for (int i = x1 + xstep; i <= x2 - xstep; i += xstep) {
        corners.push_back (Coord2D (i, y1));
        corners.push_back (Coord2D (i, y2));
    }

    const int ystep = std::max(1, int((y2 - y1) / DivisionsPerBorder));

    for (int i = y1 + ystep; i <= y2 - ystep; i += ystep) {
        corners.push_back (Coord2D (x1, i));
        corners.push_back (Coord2D (x2, i));
    }

    std::vector<Coord2D> r, g, b;

    const bool clipped = transCoord (W, H, corners, r, g, b, ascaleDef, pLCPMap);

    // Compute bounding box by finding min/max of all coordinates
    xv = g[0].x;
    double x2d = g[0].x;

    yv = g[0].y;
    double y2d = g[0].y;

    for (size_t ii = 0; ii < r.size(); ++ii) {
        track_min_max(xv, x2d, r[ii].x);
        track_min_max(xv, x2d, g[ii].x);
        track_min_max(xv, x2d, b[ii].x);

        track_min_max(yv, y2d, r[ii].y);
        track_min_max(yv, y2d, g[ii].y);
        track_min_max(yv, y2d, b[ii].y);
    }

    int x2v = (int)ceil (x2d);
    int y2v = (int)ceil (y2d);

    wv = x2v - xv + 1;
    hv = y2v - yv + 1;

    return clipped;
}

void ImProcFunctions::transform (Imagefloat* original, Imagefloat* transformed, int cx, int cy, int sx, int sy, int oW, int oH, int fW, int fH,
                                 const FramesMetaData *metadata,
                                 int rawRotationDeg, double pixelSkip, bool useOriginalBuffer)
{
    double focalLen = metadata->getFocalLen();
    double focalLen35mm = metadata->getFocalLen35mm();
    float focusDist = metadata->getFocusDist();
    double fNumber = metadata->getFNumber();

    std::unique_ptr<const LensCorrection> pLCPMap;

    if (needsMetadata()) {
        auto corr = MetadataLensCorrectionFinder::findCorrection(metadata);
        if (corr) {
            corr->initCorrections(oW, oH, params->coarse, rawRotationDeg, pixelSkip);
            pLCPMap = std::move(corr);
        }
    } else if (needsLensfun()) {
        pLCPMap = LFDatabase::getInstance()->findModifier(params->lensProf, metadata, oW, oH, params->coarse, rawRotationDeg);
    } else if (needsLCP()) { // don't check focal length to allow distortion correction for lenses without chip
        const std::shared_ptr<LCPProfile> pLCPProf = LCPStore::getInstance()->getProfile (params->lensProf.lcpFile);

        if (pLCPProf) {
            pLCPMap.reset(
                new LCPMapper (pLCPProf, focalLen, focalLen35mm,
                               focusDist, fNumber, false,
                               false,
                               oW, oH, params->coarse, rawRotationDeg
                )
            );
        }
    }

    if (! (needsCA() || needsDistortion() || needsRotation() || needsPerspective() || needsScaleAny() || needsLCP() || needsMetadata() || needsLensfun()) && (needsVignetting() || needsPCVignetting() || needsGradient())) {
        transformLuminanceOnly (original, transformed, cx, cy, oW, oH, fW, fH);
    } else {
        bool highQuality;
        std::unique_ptr<Imagefloat> tmpimg;
        Imagefloat *dest = transformed;
        if (!needsCA() && scale != 1) {
            highQuality = false;
        } else {
            highQuality = true;
        }
        transformGeneral(highQuality, original, dest, cx, cy, sx, sy, oW, oH, fW, fH, pLCPMap.get(), useOriginalBuffer);
    }
}


// helper function
void ImProcFunctions::calcVignettingParams (int oW, int oH, const VignettingParams& vignetting, double &w2, double &h2, double& maxRadius, double &v, double &b, double &mul)
{
    // vignette center is a point with coordinates between -1 and +1
    double x = vignetting.centerX / 100.0;
    double y = vignetting.centerY / 100.0;

    // calculate vignette center in pixels
    w2 = (double) oW  / 2.0 - 0.5 + x * oW;
    h2 = (double) oH  / 2.0 - 0.5 + y * oH;

    // max vignette radius in pixels
    maxRadius = sqrt ( (double) ( oW * oW + oH * oH ) ) / 2.;

    // vignette variables with applied strength
    v = 1.0 + vignetting.strength * fabs (vignetting.amount) * 3.0 / 400.0;
    b = 1.0 + vignetting.radius * 7.0 / 100.0;
    mul = (1.0 - v) / tanh (b);
}

struct grad_params {
    bool angle_is_zero, transpose, bright_top;
    float ta, yc, xc;
    float ys, ys_inv;
    float scale, botmul, topmul;
    float top_edge_0;
    int h;
};
static void calcGradientParams (int oW, int oH, const GradientParams& gradient, struct grad_params& gp)
{
    int w = oW;
    int h = oH;
    double gradient_stops = gradient.strength;
    double gradient_span = gradient.feather / 100.0;
    double gradient_center_x = gradient.centerX / 200.0 + 0.5;
    double gradient_center_y = gradient.centerY / 200.0 + 0.5;
    double gradient_angle = gradient.degree / 180.0 * rtengine::RT_PI;
    //fprintf(stderr, "%f %f %f %f %f %d %d\n", gradient_stops, gradient_span, gradient_center_x, gradient_center_y, gradient_angle, w, h);
    // make 0.0 <= gradient_angle < 2 * rtengine::RT_PI
    gradient_angle = fmod (gradient_angle, 2 * rtengine::RT_PI);

    if (gradient_angle < 0.0) {
        gradient_angle += 2.0 * rtengine::RT_PI;
    }

    gp.bright_top = false;
    gp.transpose = false;
    gp.angle_is_zero = false;
    gp.h = h;
    double cosgrad = cos (gradient_angle);

    if (fabs (cosgrad) < 0.707) {
        // we transpose to avoid division by zero at 90 degrees
        // (actually we could transpose only for 90 degrees, but this way we avoid
        // division with extremely small numbers
        gp.transpose = true;
        gradient_angle += 0.5 * rtengine::RT_PI;
        double gxc = gradient_center_x;
        gradient_center_x = 1.0 - gradient_center_y;
        gradient_center_y = gxc;
    }

    gradient_angle = fmod (gradient_angle, 2 * rtengine::RT_PI);

    if (gradient_angle > 0.5 * rtengine::RT_PI && gradient_angle < rtengine::RT_PI) {
        gradient_angle += rtengine::RT_PI;
        gp.bright_top = true;
    } else if (gradient_angle >= rtengine::RT_PI && gradient_angle < 1.5 * rtengine::RT_PI) {
        gradient_angle -= rtengine::RT_PI;
        gp.bright_top = true;
    }

    if (fabs (gradient_angle) < 0.001 || fabs (gradient_angle - 2 * rtengine::RT_PI) < 0.001) {
        gradient_angle = 0;
        gp.angle_is_zero = true;
    }

    if (gp.transpose) {
        gp.bright_top = !gp.bright_top;
        std::swap(w, h);
    }

    gp.scale = 1.0 / pow (2, gradient_stops);

    if (gp.bright_top) {
        gp.topmul = 1.0;
        gp.botmul = gp.scale;
    } else {
        gp.topmul = gp.scale;
        gp.botmul = 1.0;
    }

    gp.ta = tan (gradient_angle);
    gp.xc = w * gradient_center_x;
    gp.yc = h * gradient_center_y;
    gp.ys = rtengine::norm2(static_cast<double>(h), static_cast<double>(w)) * (gradient_span / cos(gradient_angle));
    gp.ys_inv = 1.f / gp.ys;
    gp.top_edge_0 = gp.yc - gp.ys / 2.f;

    if (h * gp.ys < 1.f) {
        gp.ys_inv = 0;
        gp.ys = 0;
    }
}

float ImProcFunctions::calcGradientFactor (const struct grad_params& gp, int x, int y)
{
    if (gp.angle_is_zero) {
        int gy = gp.transpose ? x : y;

        if (gy < gp.top_edge_0) {
            return gp.topmul;
        } else if (gy >= gp.top_edge_0 + gp.ys) {
            return gp.botmul;
        } else {
            float val = ((float) (gy - gp.top_edge_0) * gp.ys_inv);

            if (gp.bright_top) {
                val = 1.f - val;
            }

            val *= rtengine::RT_PI_F_2;

            if (gp.scale < 1.f) {
                val = pow3 (xsinf (val));
            } else {
                val = 1.f - pow3 (xcosf (val));
            }

            return gp.scale + val * (1.f - gp.scale);
        }
    } else {
        int gy = gp.transpose ? x : y;
        int gx = gp.transpose ? gp.h - y - 1 : x;
        float top_edge = gp.top_edge_0 - gp.ta * (gx - gp.xc);

        if (gy < top_edge) {
            return gp.topmul;
        } else if (gy >= top_edge + gp.ys) {
            return gp.botmul;
        } else {
            float val = ((float) (gy - top_edge) * gp.ys_inv);

            val = gp.bright_top ? 1.f - val : val;

            val *= rtengine::RT_PI_F_2;

            if (gp.scale < 1.f) {
                val = pow3 (xsinf (val));
            } else {
                val = 1.f - pow3 (xcosf (val));
            }

            return gp.scale + val * (1.f - gp.scale);
        }
    }
}

struct pcv_params {
    float oe_a, oe_b, oe1_a, oe1_b, oe2_a, oe2_b;
    float ie_mul, ie1_mul, ie2_mul;
    float sepmix, feather;
    int w, h, x1, x2, y1, y2;
    int sep;
    bool is_super_ellipse_mode, is_portrait;
    float scale;
    float fadeout_mul;
};
static void calcPCVignetteParams (int fW, int fH, int oW, int oH, const PCVignetteParams& pcvignette, const CropParams &crop, struct pcv_params& pcv)
{

    // ellipse formula: (x/a)^2 + (y/b)^2 = 1
    float roundness = pcvignette.roundness / 100.f;
    pcv.feather = pcvignette.feather / 100.0;

    if (crop.enabled) {
        pcv.w = (crop.w * oW) / fW;
        pcv.h = (crop.h * oH) / fH;
        pcv.x1 = (crop.x * oW) / fW;
        pcv.y1 = (crop.y * oH) / fH;
        pcv.x2 = pcv.x1 + pcv.w;
        pcv.y2 = pcv.y1 + pcv.h;
    } else {
        pcv.x1 = 0, pcv.y1 = 0;
        pcv.x2 = oW, pcv.y2 = oH;
        pcv.w = oW;
        pcv.h = oH;
    }

    pcv.fadeout_mul = 20.0 / rtengine::norm2(static_cast<double>(oW), static_cast<double>(oW));
    float short_side = (pcv.w < pcv.h) ? pcv.w : pcv.h;
    float long_side =  (pcv.w > pcv.h) ? pcv.w : pcv.h;

    pcv.sep = 2;
    pcv.sepmix = 0;
    pcv.oe_a = std::sqrt(2.f) * long_side * 0.5f;
    pcv.oe_b = pcv.oe_a * short_side / long_side;
    pcv.ie_mul = (1.f - pcv.feather) / std::sqrt(2.f);
    pcv.is_super_ellipse_mode = false;
    pcv.is_portrait = (pcv.w < pcv.h);

    if (roundness < 0.5f) {
        // make super-ellipse of higher and higher degree
        pcv.is_super_ellipse_mode = true;
        float sepf = 2 + 4 * std::pow(1.f - 2 * roundness, 1.3f); // gamma 1.3 used to balance the effect in the 0.0...0.5 roundness range
        pcv.sep = ((int)sepf) & ~0x1;
        pcv.sepmix = (sepf - pcv.sep) * 0.5f; // 0.0 to 1.0
        pcv.oe1_a = std::pow(2.f, 1.f / pcv.sep) * long_side * 0.5f;
        pcv.oe1_b = pcv.oe1_a * short_side / long_side;
        pcv.ie1_mul = (1.f - pcv.feather) / std::pow(2.f, 1.f / pcv.sep);
        pcv.oe2_a = std::pow(2.f, 1.f / (pcv.sep + 2)) * long_side * 0.5f;
        pcv.oe2_b = pcv.oe2_a * short_side / long_side;
        pcv.ie2_mul = (1.f - pcv.feather) / std::pow(2.f, 1.f / (pcv.sep + 2));
    } else if (roundness > 0.5f) {
        // scale from fitted ellipse towards circle
        float rad = rtengine::norm2(static_cast<float>(pcv.w), static_cast<float>(pcv.h)) / 2.f;
        float diff_a = rad - pcv.oe_a;
        float diff_b = rad - pcv.oe_b;
        pcv.oe_a = pcv.oe_a + diff_a * 2 * (roundness - 0.5f);
        pcv.oe_b = pcv.oe_b + diff_b * 2 * (roundness - 0.5f);
    }

    pcv.scale = std::pow(2, -pcvignette.strength);

    if (pcvignette.strength >= 6.0) {
        pcv.scale = 0.0;
    }
}

static float calcPCVignetteFactor (const struct pcv_params& pcv, int x, int y)
{

    float fo = 1.f;

    if (x < pcv.x1 || x > pcv.x2 || y < pcv.y1 || y > pcv.y2) {
        /*
          The initial plan was to have 1.0 directly outside the crop box (ie no fading), but due to
          rounding/trunction here and there I didn't succeed matching up exactly on the pixel with
          the crop box. To hide that mismatch I made a fade.
         */
        int dist_x = (x < pcv.x1) ? pcv.x1 - x : x - pcv.x2;
        int dist_y = (y < pcv.y1) ? pcv.y1 - y : y - pcv.y2;

        if (dist_x < 0) {
            dist_x = 0;
        }

        if (dist_y < 0) {
            dist_y = 0;
        }

        fo = sqrtf (dist_x * dist_x + dist_y * dist_y) * pcv.fadeout_mul;

        if (fo >= 1.f) {
            return 1.f;
        }
    }

    float a = fabs ((x - pcv.x1) - pcv.w * 0.5f);
    float b = fabs ((y - pcv.y1) - pcv.h * 0.5f);

    if (pcv.is_portrait) {
        std::swap (a, b);
    }

    float dist = normn (a, b, 2);
    float dist_oe, dist_ie;
    float2 sincosval;

    if (dist == 0.0f) {
        sincosval.y = 1.0f;         // cos
        sincosval.x = 0.0f;         // sin
    } else {
        sincosval.y = a / dist;     // cos
        sincosval.x = b / dist;     // sin
    }

    if (pcv.is_super_ellipse_mode) {
        float dist_oe1 = pcv.oe1_a * pcv.oe1_b / normn (pcv.oe1_b * sincosval.y, pcv.oe1_a * sincosval.x, pcv.sep);
        float dist_oe2 = pcv.oe2_a * pcv.oe2_b / normn (pcv.oe2_b * sincosval.y, pcv.oe2_a * sincosval.x, pcv.sep + 2);
        float dist_ie1 = pcv.ie1_mul * dist_oe1;
        float dist_ie2 = pcv.ie2_mul * dist_oe2;
        dist_oe = dist_oe1 * (1.f - pcv.sepmix) + dist_oe2 * pcv.sepmix;
        dist_ie = dist_ie1 * (1.f - pcv.sepmix) + dist_ie2 * pcv.sepmix;
    } else {
        dist_oe = pcv.oe_a * pcv.oe_b / sqrtf (SQR (pcv.oe_b * sincosval.y) + SQR (pcv.oe_a * sincosval.x));
        dist_ie = pcv.ie_mul * dist_oe;
    }

    if (dist <= dist_ie) {
        return 1.f;
    }

    float val;

    if (dist >= dist_oe) {
        val = pcv.scale;
    } else {
        val = rtengine::RT_PI_F_2 * (dist - dist_ie) / (dist_oe - dist_ie);

        if (pcv.scale < 1.f) {
            val = pow4 (xcosf (val));
        } else {
            val = 1 - pow4 (xsinf (val));
        }

        val = pcv.scale + val * (1.f - pcv.scale);
    }

    if (fo < 1.f) {
        val = fo + val * (1.f - fo);
    }

    return val;
}

void ImProcFunctions::transformLuminanceOnly (Imagefloat* original, Imagefloat* transformed, int cx, int cy, int oW, int oH, int fW, int fH)
{

    const bool applyVignetting = needsVignetting();
    const bool applyGradient = needsGradient();
    const bool applyPCVignetting = needsPCVignetting();

    double vig_w2, vig_h2, maxRadius, v, b, mul;

    if (applyVignetting) {
        calcVignettingParams (oW, oH, params->vignetting, vig_w2, vig_h2, maxRadius, v, b, mul);
    }

    struct grad_params gp;

    if (applyGradient) {
        calcGradientParams (oW, oH, params->gradient, gp);
    }

    struct pcv_params pcv;

    if (applyPCVignetting) {
        //fprintf(stderr, "%d %d | %d %d | %d %d | %d %d [%d %d]\n", fW, fH, oW, oH, transformed->getWidth(), transformed->getHeight(), cx, cy, params->crop.w, params->crop.h);
        calcPCVignetteParams (fW, fH, oW, oH, params->pcvignette, params->crop, pcv);
    }

    bool darkening = (params->vignetting.amount <= 0.0);
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

    for (int y = 0; y < transformed->getHeight(); y++) {
        double vig_y_d = applyVignetting ? (double) (y + cy) - vig_h2 : 0.0;

        for (int x = 0; x < transformed->getWidth(); x++) {
            double factor = 1.0;

            if (applyVignetting) {
                double vig_x_d = (double) (x + cx) - vig_w2 ;
                double r = sqrt (vig_x_d * vig_x_d + vig_y_d * vig_y_d);

                if (darkening) {
                    factor /= std::max (v + mul * tanh(b * (maxRadius - r) / maxRadius), 0.001);
                } else {
                    factor = v + mul * tanh(b * (maxRadius - r) / maxRadius);
                }
            }

            if (applyGradient) {
                factor *= static_cast<double>(calcGradientFactor(gp, cx + x, cy + y));
            }

            if (applyPCVignetting) {
                factor *= static_cast<double>(calcPCVignetteFactor(pcv, cx + x, cy + y));
            }

            transformed->r(y, x) = static_cast<double>(original->r(y, x)) * factor;
            transformed->g(y, x) = static_cast<double>(original->g(y, x)) * factor;
            transformed->b(y, x) = static_cast<double>(original->b(y, x)) * factor;
        }
    }
}


void ImProcFunctions::transformGeneral(bool highQuality, Imagefloat *original, Imagefloat *transformed, int cx, int cy, int sx, int sy, int oW, int oH, int fW, int fH, const LensCorrection *pLCPMap, bool useOriginalBuffer)
{

    // set up stuff, depending on the mode we are
    enum PerspType { NONE, SIMPLE, CAMERA_BASED };
    const bool enableLCPDist = pLCPMap && params->lensProf.useDist && pLCPMap->hasDistortionCorrection();
    const bool enableLCPCA = pLCPMap && params->lensProf.useCA && pLCPMap->hasCACorrection();
    const bool enableCA = highQuality && needsCA();
    const bool doCACorrection = enableCA || enableLCPCA;
    const bool enableGradient = needsGradient();
    const bool enablePCVignetting = needsPCVignetting();
    const bool enableVignetting = needsVignetting();
    const bool enableDistortion = needsDistortion();
    const PerspType perspectiveType = needsPerspective() ? (
            (params->perspective.method == "camera_based") ?
            PerspType::CAMERA_BASED : PerspType::SIMPLE ) : PerspType::NONE;

    const double w2 = static_cast<double>(oW)  / 2.0 - 0.5;
    const double h2 = static_cast<double>(oH)  / 2.0 - 0.5;

    double vig_w2, vig_h2, maxRadius, v, b, mul;
    calcVignettingParams(oW, oH, params->vignetting, vig_w2, vig_h2, maxRadius, v, b, mul);

    grad_params gp;

    if (enableGradient) {
        calcGradientParams(oW, oH, params->gradient, gp);
    }

    pcv_params pcv;

    if (enablePCVignetting) {
        calcPCVignetteParams(fW, fH, oW, oH, params->pcvignette, params->crop, pcv);
    }

    const std::array<float* const*, 3> chTrans = {
        transformed->r.ptrs,
        transformed->g.ptrs,
        transformed->b.ptrs
    };

    // auxiliary variables for c/a correction
    const std::array<double, 3> chDist = {
        enableCA
            ? params->cacorrection.red
            : 0.0,
        0.0,
        enableCA
            ? params->cacorrection.blue
            : 0.0
    };

    // auxiliary variables for distortion correction
    const double distAmount = params->distortion.amount;

    // auxiliary variables for rotation
    const double cost = cos(params->rotate.degree * rtengine::RT_PI / 180.0);
    const double sint = sin(params->rotate.degree * rtengine::RT_PI / 180.0);

    // auxiliary variables for perspective correction
    // Simple.
    const double vpdeg = params->perspective.vertical / 100.0 * 45.0;
    const double vpalpha = (90.0 - vpdeg) / 180.0 * rtengine::RT_PI;
    const double vpteta = fabs(vpalpha - rtengine::RT_PI / 2) < 3e-4 ? 0.0 : acos((vpdeg > 0 ? 1.0 : -1.0) * sqrt((-SQR(oW * tan(vpalpha)) + (vpdeg > 0 ? 1.0 : -1.0) *
                          oW * tan(vpalpha) * sqrt(SQR(4 * maxRadius) + SQR(oW * tan(vpalpha)))) / (SQR(maxRadius) * 8)));
    const double vpcospt = (vpdeg >= 0 ? 1.0 : -1.0) * cos(vpteta);
    const double vptanpt = tan(vpteta);
    const double hpdeg = params->perspective.horizontal / 100.0 * 45.0;
    const double hpalpha = (90.0 - hpdeg) / 180.0 * rtengine::RT_PI;
    const double hpteta = fabs(hpalpha - rtengine::RT_PI / 2) < 3e-4 ? 0.0 : acos((hpdeg > 0 ? 1.0 : -1.0) * sqrt((-SQR(oH * tan(hpalpha)) + (hpdeg > 0 ? 1.0 : -1.0) *
                          oH * tan(hpalpha) * sqrt(SQR(4 * maxRadius) + SQR(oH * tan(hpalpha)))) / (SQR(maxRadius) * 8)));
    const double hpcospt = (hpdeg >= 0 ? 1.0 : -1.0) * cos(hpteta);
    const double hptanpt = tan(hpteta);
    // Camera-based.
    const double f =
            ((params->perspective.camera_focal_length > 0) ? params->perspective.camera_focal_length : PerspectiveParams::DEFAULT_CAMERA_FOCAL_LENGTH)
            * ((params->perspective.camera_crop_factor > 0) ? params->perspective.camera_crop_factor : PerspectiveParams::DEFAULT_CAMERA_CROP_FACTOR)
            * (maxRadius / sqrt(18.0*18.0 + 12.0*12.0));
    const double f_defish =
            ((params->distortion.focal_length > 0) ? params->distortion.focal_length : DistortionParams::DEFAULT_FOCAL_LENGTH)
            * (maxRadius / sqrt(18.0*18.0 + 12.0*12.0));
    const double p_camera_yaw = params->perspective.camera_yaw / 180.0 * rtengine::RT_PI;
    const double p_camera_pitch = params->perspective.camera_pitch / 180.0 * rtengine::RT_PI;
    const double p_camera_roll = params->perspective.camera_roll * rtengine::RT_PI_180;
    const double p_camera_shift_horiz = oW / 100.0 * params->perspective.camera_shift_horiz;
    const double p_camera_shift_vert = oH / -100.0 * params->perspective.camera_shift_vert;
    const double p_projection_shift_horiz = oW / 100.0 * params->perspective.projection_shift_horiz;
    const double p_projection_shift_vert = oH / -100.0 * params->perspective.projection_shift_vert;
    const double p_projection_rotate = params->perspective.projection_rotate * rtengine::RT_PI_180;
    const double p_projection_yaw = -params->perspective.projection_yaw * rtengine::RT_PI_180;
    const double p_projection_pitch = -params->perspective.projection_pitch * rtengine::RT_PI_180;
    const double p_projection_scale = 1;
    const homogeneous::Matrix<double> p_matrix = perspectiveMatrix(f,
            p_camera_shift_horiz, p_camera_shift_vert, p_camera_roll,
            p_camera_pitch, p_camera_yaw, p_projection_yaw, p_projection_pitch,
            p_projection_rotate, p_projection_shift_horiz,
            p_projection_shift_vert, p_projection_scale);

    const double ascale = params->commonTrans.autofill && params->perspective.render ? getTransformAutoFill(oW, oH, pLCPMap) : 1.0 / params->commonTrans.getScale();
    const double hscale = ascale / params->commonTrans.getScaleHorizontally();
    const double vscale = ascale / params->commonTrans.getScaleVertically();

    const bool darkening = (params->vignetting.amount <= 0.0);
    const bool useLog = params->commonTrans.method == "log" && highQuality;
    const double centerFactorx = cx - w2;
    const double centerFactory = cy - h2;

    std::unique_ptr<Imagefloat> tempLog;
    if (useLog) {
        if (!useOriginalBuffer) {
            tempLog.reset(new Imagefloat(original->getWidth(), original->getHeight()));
            logEncode(original, tempLog.get(), multiThread);
            original = tempLog.get();
        } else {
            logEncode(original, original, multiThread);
        }
    }

    const std::array<const float* const*, 3> chOrig = {
        original->r.ptrs,
        original->g.ptrs,
        original->b.ptrs
    };

    // main cycle
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 16) if(multiThread)
#endif

    for (int y = 0; y < transformed->getHeight(); ++y) {
        for (int x = 0; x < transformed->getWidth(); ++x) {
            double x_d = x;
            double y_d = y;

            x_d = hscale * (x_d + centerFactorx);     // centering x coord & scale
            y_d = vscale * (y_d + centerFactory);     // centering y coord & scale

            switch (perspectiveType) {
                case PerspType::NONE:
                    break;
                case PerspType::SIMPLE:
                    // horizontal perspective transformation
                    y_d *= maxRadius / (maxRadius + x_d * hptanpt);
                    x_d *= maxRadius * hpcospt / (maxRadius + x_d * hptanpt);

                    // vertical perspective transformation
                    x_d *= maxRadius / (maxRadius - y_d * vptanpt);
                    y_d *= maxRadius * vpcospt / (maxRadius - y_d * vptanpt);
                    break;
                case PerspType::CAMERA_BASED:
                    const double w = p_matrix[3][0] * x_d + p_matrix[3][1] * y_d + p_matrix[3][3];
                    const double xw = p_matrix[0][0] * x_d + p_matrix[0][1] * y_d + p_matrix[0][3];
                    const double yw = p_matrix[1][0] * x_d + p_matrix[1][1] * y_d + p_matrix[1][3];
                    x_d = xw / w;
                    y_d = yw / w;
                    break;
            }

            if (params->distortion.defish) {
                x_d /= f_defish;
                y_d /= f_defish;

                const double r = std::sqrt(x_d * x_d + y_d * y_d);
                const double factor = f_defish * std::atan(r) / r;

                x_d *= factor;
                y_d *= factor;
            }

            // rotate
            const double Dxr = x_d * cost - y_d * sint;
            const double Dyr = x_d * sint + y_d * cost;

            for (int c = 0; c < (doCACorrection ? 3 : 1); ++c) {
                double Dx = Dxr;
                double Dy = Dyr;

                if (enableLCPDist && enableLCPCA) {
                    pLCPMap->correctDistortionAndCA(Dx, Dy, w2, h2, c);
                } else if (enableLCPDist) {
                    pLCPMap->correctDistortion(Dx, Dy, w2, h2);
                } else if (enableLCPCA) {
                    pLCPMap->correctCA(Dx, Dy, w2, h2, c);
                }

                // distortion correction
                double s = 1.0;

                if (enableDistortion) {
                    const double r = sqrt(Dx * Dx + Dy * Dy) / maxRadius;
                    s = 1.0 - distAmount + distAmount * r;
                }

                // CA correction
                Dx *= s + chDist[c];
                Dy *= s + chDist[c];

                // de-center
                Dx += w2;
                Dy += h2;

                // Extract integer and fractions of source screen coordinates
                int xc = Dx;
                Dx -= xc;
                xc -= sx;
                int yc = Dy;
                Dy -= yc;
                yc -= sy;

                // Convert only valid pixels
                if (yc >= 0 && yc < original->getHeight() && xc >= 0 && xc < original->getWidth()) {
                    // multiplier for vignetting correction
                    double vignmul = 1.0;

                    if (enableVignetting) {
                        const double vig_x_d = hscale * (x + cx - vig_w2); // centering x coord & scale
                        const double vig_y_d = vscale * (y + cy - vig_h2); // centering y coord & scale
                        const double vig_Dx = vig_x_d * cost - vig_y_d * sint;
                        const double vig_Dy = vig_x_d * sint + vig_y_d * cost;
                        const double r2 = sqrt(vig_Dx * vig_Dx + vig_Dy * vig_Dy);
                        if (darkening) {
                            vignmul /= std::max(v + mul * tanh(b * (maxRadius - s * r2) / maxRadius), 0.001);
                        } else {
                            vignmul *= (v + mul * tanh(b * (maxRadius - s * r2) / maxRadius));
                        }
                    }

                    if (enableGradient) {
                        vignmul *= static_cast<double>(calcGradientFactor(gp, cx + x, cy + y));
                    }

                    if (enablePCVignetting) {
                        vignmul *= static_cast<double>(calcPCVignetteFactor(pcv, cx + x, cy + y));
                    }

                    if (yc > 0 && yc < original->getHeight() - 2 && xc > 0 && xc < original->getWidth() - 2) {
                        // all interpolation pixels inside image
                        if (!highQuality) {
                            transformed->r(y, x) = vignmul * (original->r(yc, xc) * (1.0 - Dx) * (1.0 - Dy) + original->r(yc, xc + 1) * Dx * (1.0 - Dy) + original->r(yc + 1, xc) * (1.0 - Dx) * Dy + original->r(yc + 1, xc + 1) * Dx * Dy);
                            transformed->g(y, x) = vignmul * (original->g(yc, xc) * (1.0 - Dx) * (1.0 - Dy) + original->g(yc, xc + 1) * Dx * (1.0 - Dy) + original->g(yc + 1, xc) * (1.0 - Dx) * Dy + original->g(yc + 1, xc + 1) * Dx * Dy);
                            transformed->b(y, x) = vignmul * (original->b(yc, xc) * (1.0 - Dx) * (1.0 - Dy) + original->b(yc, xc + 1) * Dx * (1.0 - Dy) + original->b(yc + 1, xc) * (1.0 - Dx) * Dy + original->b(yc + 1, xc + 1) * Dx * Dy);
                        } else if (!useLog) {
                            if (doCACorrection) {
                                interpolateTransformChannelsCubic(chOrig[c], xc - 1, yc - 1, Dx, Dy, chTrans[c][y][x], vignmul);
                            } else {
                                interpolateTransformCubic(original, xc - 1, yc - 1, Dx, Dy, transformed->r(y, x), transformed->g(y, x), transformed->b(y, x), vignmul);
                            }
                        } else {
                            if (doCACorrection) {
                                interpolateTransformChannelsCubicLog(chOrig[c], xc - 1, yc - 1, Dx, Dy, chTrans[c][y][x], vignmul);
                            } else {
                                interpolateTransformCubicLog(original, xc - 1, yc - 1, Dx, Dy, transformed->r(y, x), transformed->g(y, x), transformed->b(y, x), vignmul);
                            }
                        }
                    } else {
                        // edge pixels
                        const int y1 = LIM(yc, 0, original->getHeight() - 1);
                        const int y2 = LIM(yc + 1, 0, original->getHeight() - 1);
                        const int x1 = LIM(xc, 0, original->getWidth() - 1);
                        const int x2 = LIM(xc + 1, 0, original->getWidth() - 1);

                        if (useLog) {
                            if (doCACorrection) {
                                chTrans[c][y][x] = vignmul * xexpf(chOrig[c][y1][x1] * (1.0 - Dx) * (1.0 - Dy) + chOrig[c][y1][x2] * Dx * (1.0 - Dy) + chOrig[c][y2][x1] * (1.0 - Dx) * Dy + chOrig[c][y2][x2] * Dx * Dy);
                            } else {
                                transformed->r(y, x) = vignmul * xexpf(original->r(y1, x1) * (1.0 - Dx) * (1.0 - Dy) + original->r(y1, x2) * Dx * (1.0 - Dy) + original->r(y2, x1) * (1.0 - Dx) * Dy + original->r(y2, x2) * Dx * Dy);
                                transformed->g(y, x) = vignmul * xexpf(original->g(y1, x1) * (1.0 - Dx) * (1.0 - Dy) + original->g(y1, x2) * Dx * (1.0 - Dy) + original->g(y2, x1) * (1.0 - Dx) * Dy + original->g(y2, x2) * Dx * Dy);
                                transformed->b(y, x) = vignmul * xexpf(original->b(y1, x1) * (1.0 - Dx) * (1.0 - Dy) + original->b(y1, x2) * Dx * (1.0 - Dy) + original->b(y2, x1) * (1.0 - Dx) * Dy + original->b(y2, x2) * Dx * Dy);
                            }
                        } else {
                            if (doCACorrection) {
                                chTrans[c][y][x] = vignmul * (chOrig[c][y1][x1] * (1.0 - Dx) * (1.0 - Dy) + chOrig[c][y1][x2] * Dx * (1.0 - Dy) + chOrig[c][y2][x1] * (1.0 - Dx) * Dy + chOrig[c][y2][x2] * Dx * Dy);
                            } else {
                                transformed->r(y, x) = vignmul * (original->r(y1, x1) * (1.0 - Dx) * (1.0 - Dy) + original->r(y1, x2) * Dx * (1.0 - Dy) + original->r(y2, x1) * (1.0 - Dx) * Dy + original->r(y2, x2) * Dx * Dy);
                                transformed->g(y, x) = vignmul * (original->g(y1, x1) * (1.0 - Dx) * (1.0 - Dy) + original->g(y1, x2) * Dx * (1.0 - Dy) + original->g(y2, x1) * (1.0 - Dx) * Dy + original->g(y2, x2) * Dx * Dy);
                                transformed->b(y, x) = vignmul * (original->b(y1, x1) * (1.0 - Dx) * (1.0 - Dy) + original->b(y1, x2) * Dx * (1.0 - Dy) + original->b(y2, x1) * (1.0 - Dx) * Dy + original->b(y2, x2) * Dx * Dy);
                            }
                        }
                    }
                } else {
                    if (doCACorrection) {
                        // not valid (source pixel x,y not inside source image, etc.)
                        chTrans[c][y][x] = 0;
                    } else {
                        transformed->r(y, x) = 0;
                        transformed->g(y, x) = 0;
                        transformed->b(y, x) = 0;
                    }
                }
            }
        }
    }
}

std::unique_ptr<const LensCorrection> ImProcFunctions::getCameraFraming(int width, int height, const FramesMetaData *metadata, int rawRotationDeg, int pixelSkip) const
{
    if (width > 0 && height > 0 && needsMetadata() && params->lensProf.useDist) {
        auto correction = MetadataLensCorrectionFinder::findCorrection(metadata, MetadataLensCorrectionFinder::Purpose::CameraFraming);
        if (correction) {
            correction->initCorrections(width, height, params->coarse, rawRotationDeg, pixelSkip);
            return correction;
        }
    }
    return nullptr;
}

double ImProcFunctions::getTransformAutoFill (int oW, int oH, const LensCorrection *pLCPMap) const
{
    if (!needsCA() && !needsDistortion() && !needsRotation() && !needsPerspective() && (!params->lensProf.useDist || pLCPMap == nullptr)) {
        return 1;
    }

    double scaleU = 2, scaleL = 0.001;  // upper and lower border, iterate inbetween

    if (params->lensProf.useDist && pLCPMap && pLCPMap->hasCameraFraming()) {
        scaleU = 1;
        int x, y, w, h;
        if (!transCoord(oW, oH, 0, 0, oW, oH, x, y, w, h, scaleU, pLCPMap)) {
            return scaleU;
        }
    }

    do {
        double scale = (scaleU + scaleL) * 0.5;

        int orx, ory, orw, orh;
        bool clipped = transCoord (oW, oH, 0, 0, oW, oH, orx, ory, orw, orh, scale, pLCPMap);

        if (clipped) {
            scaleU = scale;
        } else {
            scaleL = scale;
        }
    } while (scaleU - scaleL > 0.001);

    return scaleL;
}

bool ImProcFunctions::needsCA () const
{
    return fabs (params->cacorrection.red) > 1e-15 || fabs (params->cacorrection.blue) > 1e-15;
}

bool ImProcFunctions::needsDistortion () const
{
    return
            params->distortion.defish ||
            fabs (params->distortion.amount) > 1e-15;
}

bool ImProcFunctions::needsRotation () const
{
    return fabs (params->rotate.degree) > 1e-15;
}

bool ImProcFunctions::needsPerspective () const
{
    return ( (params->perspective.method == "simple") &&
            (params->perspective.horizontal || params->perspective.vertical) )
        || ( (params->perspective.method == "camera_based") &&
             params->perspective.render && (
                    params->perspective.camera_pitch ||
                    params->perspective.camera_roll ||
                    params->perspective.camera_shift_horiz ||
                    params->perspective.camera_shift_vert ||
                    params->perspective.camera_yaw ||
                    params->perspective.projection_pitch ||
                    params->perspective.projection_rotate ||
                    params->perspective.projection_shift_horiz ||
                    params->perspective.projection_shift_vert ||
                    params->perspective.projection_yaw));
}

bool ImProcFunctions::needsScale () const
{
  return std::abs(1.0 - params->commonTrans.getScale()) > 1e-6;
}

bool ImProcFunctions::needsScaleHorizontally() const
{
  return std::abs(1.0 - params->commonTrans.getScaleHorizontally()) > 1e-6;
}

bool ImProcFunctions::needsScaleVertically() const
{
  return std::abs(1.0 - params->commonTrans.getScaleVertically()) > 1e-6;
}

bool ImProcFunctions::needsScaleAny() const
{
  return needsScale() || needsScaleHorizontally() || needsScaleVertically();
}

bool ImProcFunctions::needsGradient () const
{
    return params->gradient.enabled && fabs (params->gradient.strength) > 1e-15;
}

bool ImProcFunctions::needsPCVignetting () const
{
    return params->pcvignette.enabled && fabs (params->pcvignette.strength) > 1e-15;
}

bool ImProcFunctions::needsVignetting () const
{
    return params->vignetting.amount;
}

bool ImProcFunctions::needsMetadata () const
{
    return params->lensProf.useMetadata();
}

bool ImProcFunctions::needsLCP () const
{
    return params->lensProf.useLcp();
}

bool ImProcFunctions::needsLensfun() const
{
    return params->lensProf.useLensfun();
}

bool ImProcFunctions::needsTransform (int oW, int oH, int rawRotationDeg, const FramesMetaData *metadata) const
{
    bool needsLf = needsLensfun();
    if (needsLf) {
        std::unique_ptr<const LensCorrection> pLCPMap = LFDatabase::getInstance()->findModifier(params->lensProf, metadata, oW, oH, params->coarse, rawRotationDeg);
        needsLf = pLCPMap.get();
    }
    return needsCA () || needsDistortion () || needsRotation () || needsPerspective () || needsScaleAny() || needsGradient () || needsPCVignetting () || needsVignetting () || needsLCP() || needsMetadata() || needsLf;
}


}
