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

#include "improcfun.h"

#include "alignedbuffer.h"
#include "color.h"
#include "imagefloat.h"
#include "labimage.h"
#include "opthelper.h"
#include "rt_math.h"
#include "procparams.h"
#include "sleef.h"
#include "text.h"

//#define PROFILE

#ifdef PROFILE
#   include <iostream>
#endif

namespace
{

using ProcParams = rtengine::procparams::ProcParams;
using FramingParams = rtengine::procparams::FramingParams;

using Basis = FramingParams::Basis;
using BorderSizing = FramingParams::BorderSizing;
using FramingMethod = FramingParams::FramingMethod;

enum class Orientation { LANDSCAPE, PORTRAIT };
enum class Side { WIDTH, HEIGHT };

struct Dimensions
{
    double width;
    double height;

    Dimensions() : width(0), height(0) {}
    Dimensions(double w, double h) : width(w), height(h) {}

    bool isDegenerate() const { return width <= 0.0 || height <= 0.0; }

    double aspectRatio() const {
        if (isDegenerate()) return 1.0;
        else return static_cast<double>(width) / static_cast<double>(height);
    }

    Orientation orient() const {
        return width >= height ? Orientation::LANDSCAPE : Orientation::PORTRAIT;
    }

    bool inside(const Dimensions& other) const {
        return width <= other.width && height <= other.height;
    }

    void rotate(Orientation newOrient) {
        if (newOrient != orient()) {
            std::swap(width, height);
        }
    }

    bool operator==(const Dimensions& other) const {
        return width == other.width && height == other.height;
    }
    bool operator!=(const Dimensions& other) const { return !(*this == other); }

    Dimensions intersect(const Dimensions& other) const {
        return Dimensions(std::min(width, other.width), std::min(height, other.height));
    }

    void debug(const char* prefix) const {
        printf("%s w=%f h=%f ar=%f\n", prefix, width, height, aspectRatio());
    }
};

struct ResizeArgs
{
    Dimensions size;
    double scale = 1.0;

    ResizeArgs(const Dimensions& aSize, double aScale) : size(aSize), scale(aScale) {}
};

class Framing
{
public:
    Framing(const ProcParams& params, int fullWidth, int fullHeight);

    ResizeArgs adjustResizeForFraming(const ResizeArgs& resize) const;
    Dimensions computeFramedSize(const Dimensions& imgSize) const;

private:
    Dimensions clampResize(const Dimensions& imgSize, const Dimensions& bounds) const;
    ResizeArgs adjustResize(const ResizeArgs& resize, const Dimensions& newSize) const;
    Dimensions computeRelativeImageBBoxInFrame(const Dimensions& imgSize,
                                               const Dimensions& framedSize) const;
    Dimensions computeUniformRelativeImageBBox(const Dimensions& imgSize,
                                               const Dimensions& framedSize) const;
    ResizeArgs resizeForFixedFrame(const ResizeArgs& resize) const;
    ResizeArgs resizeForBBox(const ResizeArgs& resize) const;
    Dimensions computeSizeWithBorders(const Dimensions& imgSize) const;

    const ProcParams& allParams;
    const FramingParams framing;  // Make a copy to sanitize inputs
    const Dimensions postCropImageSize;
    const Dimensions maxUpscalingBBox;
};

// Downscaling limit is 32x32px
constexpr double MIN_DOWNSCALE_PX = 32.0;
// Upscaling limit is 16x image size
constexpr double MAX_UPSCALE_FACTOR = 16.0;

int computeSize(int dim, double scale)
{
    return static_cast<int>(static_cast<double>(dim) * scale + 0.5);
}

std::pair<double, double> computeImgAndBorderSize(double frameSize, double scale)
{
    // frame_len = img_len + 2 * scale * img_len = (1 + 2 * scale) * img_len
    double imgFrameScale = (1.0 + 2.0 * scale);
    double imgSize = frameSize / imgFrameScale;
    double borderSize = scale * imgSize;

    return {imgSize, borderSize};
}

Orientation orient(const FramingParams& params, const Dimensions& imgSize)
{
    switch (params.orientation) {
        case FramingParams::Orientation::LANDSCAPE:
            return Orientation::LANDSCAPE;
        case FramingParams::Orientation::PORTRAIT:
            return Orientation::PORTRAIT;
        case FramingParams::Orientation::AS_IMAGE:
        default:
            return imgSize.orient();
    }
}

double flipAspectRatioByOrientation(double aspectRatio, Orientation orient)
{
    switch (orient) {
        case Orientation::LANDSCAPE:
            return aspectRatio >= 1.0 ? aspectRatio : 1.0 / aspectRatio;
        case Orientation::PORTRAIT:
            return aspectRatio <= 1.0 ? aspectRatio : 1.0 / aspectRatio;
        default:
            return aspectRatio;
    }
}

Side autoPickBasis(const FramingParams& params, const Dimensions& imgSize)
{
    if (imgSize.isDegenerate()) {
        if (imgSize.width <= 0) return Side::HEIGHT;
        else return Side::WIDTH;
    }

    Orientation imgOrient = imgSize.orient();
    double imgAspectRatio = imgSize.aspectRatio();
    Orientation frameOrient = orient(params, imgSize);
    double frameAspectRatio = flipAspectRatioByOrientation(params.aspectRatio, frameOrient);

    if (frameOrient == imgOrient) {
        // Pick the more constrained side (i.e. hits 0 border width first)
        return imgAspectRatio >= frameAspectRatio ? Side::WIDTH : Side::HEIGHT;
    } else if (imgOrient == Orientation::LANDSCAPE) {
        // Image in landscape, frame in portrait
        return Side::WIDTH;
    } else {
        // Image in portrait, frame in landscape
        return Side::HEIGHT;
    }
}

Side pickReferenceSide(const FramingParams& params, const Dimensions& imgSize)
{
    switch (params.basis) {
        case Basis::WIDTH:
            return Side::WIDTH;
        case Basis::HEIGHT:
            return Side::HEIGHT;
        case Basis::LONG:
            return imgSize.width >= imgSize.height ? Side::WIDTH : Side::HEIGHT;
        case Basis::SHORT:
            return imgSize.width <= imgSize.height ? Side::WIDTH : Side::HEIGHT;
        case Basis::AUTO:
        default:
            return autoPickBasis(params, imgSize);
    }
}

constexpr bool INSIDE_BBOX = true;
constexpr bool OUTSIDE_BBOX = false;

Dimensions clampToBBox(const Dimensions& img, const Dimensions& bbox, bool clampInside)
{
    double widthScale = 1.0;
    double heightScale = 1.0;
    if (bbox.width > 0) {
        widthScale = img.width / bbox.width;
    }
    if (bbox.height > 0) {
        heightScale = img.height / bbox.height;
    }

    Dimensions newSize = img;

    if (clampInside) {
        // If a side exceeds the bbox, scale down to bbox
        double scale = std::max(widthScale, heightScale);
        if (scale > 1.0) {
            if (widthScale >= heightScale) {
                newSize.width = bbox.width;
                newSize.height = img.height / widthScale;
            } else {
                newSize.width = img.width / heightScale;
                newSize.height = bbox.height;
            }
        }
    } else {
        // If a side is within the bbox, scale up to bbox
        double scale = std::min(widthScale, heightScale);
        if (scale < 1.0) {
            if (widthScale <= heightScale) {
                newSize.width = bbox.width;
                newSize.height = img.height / widthScale;
            } else {
                newSize.width = img.width / heightScale;
                newSize.height = bbox.height;
            }
        }
    }

    return newSize;
}

Dimensions downscaleToTouchBBox(const Dimensions& img, const Dimensions& bbox)
{
    if (bbox.isDegenerate()) return Dimensions(0, 0);
    if (!bbox.inside(img)) return img;

    double widthScale = img.width / bbox.width;
    double heightScale = img.height / bbox.height;

    Dimensions downscaled;
    if (widthScale <= heightScale) {
        downscaled.width = bbox.width;
        downscaled.height = img.height / widthScale;
    } else {
        downscaled.height = bbox.height;
        downscaled.width = img.width / heightScale;
    }
    return downscaled;
}

Dimensions upscaleToBBox(const Dimensions& img, const Dimensions& bbox)
{
    if (bbox.isDegenerate()) return Dimensions(0, 0);
    if (!img.inside(bbox)) return img;

    double widthScale = img.width / bbox.width;
    double heightScale = img.height / bbox.height;

    Dimensions upscaled;
    if (widthScale >= heightScale) {
        upscaled.width = bbox.width;
        upscaled.height = img.height / widthScale;
    } else {
        upscaled.height = bbox.height;
        upscaled.width = img.width / heightScale;
    }

    return upscaled;
}

double orientAspectRatio(const FramingParams& framing, const Dimensions& imgSize)
{
    double aspectRatio = framing.aspectRatio;
    if (aspectRatio == FramingParams::AS_IMAGE_ASPECT_RATIO) {
        aspectRatio = imgSize.aspectRatio();
    }

    Orientation borderOrient = orient(framing, imgSize);
    if ((borderOrient == Orientation::PORTRAIT && aspectRatio > 1.0) ||
            (borderOrient == Orientation::LANDSCAPE && aspectRatio < 1.0)) {
        aspectRatio = 1.0 / aspectRatio;
    }
    return aspectRatio;
}

Dimensions fromAspectRatio(const Dimensions& size, double aspectRatio)
{
    Dimensions result;
    if (aspectRatio >= 1.0) {
        result.height = size.height;
        result.width = result.height * aspectRatio;
    } else {
        result.width = size.width;
        result.height = result.width / aspectRatio;
    }
    return result;
}

FramingParams sanitize(const FramingParams& dirty)
{
    FramingParams framing = dirty;
    framing.framedWidth = std::max(static_cast<int>(MIN_DOWNSCALE_PX), framing.framedWidth);
    framing.framedHeight = std::max(static_cast<int>(MIN_DOWNSCALE_PX), framing.framedHeight);
    framing.relativeBorderSize = std::max(0.0, std::min(1.0, framing.relativeBorderSize));
    framing.minWidth = std::max(0, framing.minWidth);
    framing.minHeight = std::max(0, framing.minHeight);
    framing.absWidth = std::max(0, framing.absWidth);
    framing.absHeight = std::max(0, framing.absHeight);
    return framing;
}

Framing::Framing(const ProcParams& params, int fullWidth, int fullHeight) :
    allParams(params),
    framing(sanitize(params.framing)),
    postCropImageSize(params.crop.enabled ?
        Dimensions(params.crop.w, params.crop.h) :
        Dimensions(fullWidth, fullHeight)),
    maxUpscalingBBox(Dimensions(
        computeSize(postCropImageSize.width, MAX_UPSCALE_FACTOR),
        computeSize(postCropImageSize.height, MAX_UPSCALE_FACTOR)))
{
}

Dimensions Framing::clampResize(const Dimensions& imgSize, const Dimensions& bounds) const
{
    // Don't adjust above upscaling limit.
    //
    // If the upscaling limit is contained inside the target bounds, scale
    // down the bounds to outside the upscaling limit. This is needed since
    // scaling the bounds to fit inside the upscaling bbox may artificially
    // reduce the upscaling limit due to aspect ratio differences.
    Dimensions clampedBounds = maxUpscalingBBox.inside(bounds) ?
        downscaleToTouchBBox(bounds, maxUpscalingBBox) :
        clampToBBox(bounds, maxUpscalingBBox, INSIDE_BBOX);

    if (!imgSize.inside(clampedBounds)) {
        // Downscale large images to fit inside bounds (only if above limit)

        Dimensions minSize(MIN_DOWNSCALE_PX, MIN_DOWNSCALE_PX);
        if (!minSize.inside(imgSize)) {
            // Skip images below downscaling limit
            return imgSize;
        } else if (!minSize.inside(clampedBounds)) {
            // Go as small as possible without exceeding downscaling limit
            return downscaleToTouchBBox(imgSize, minSize);
        } else {
            // Downscale large images to fit inside bounds
            return clampToBBox(imgSize, clampedBounds, INSIDE_BBOX);
        }
    } else {
        // Consider upscaling...
        if (!framing.allowUpscaling ||
                imgSize.width == clampedBounds.width ||
                imgSize.height == clampedBounds.height) {
            return imgSize;
        } else {
            return upscaleToBBox(imgSize, clampedBounds);
        }
    }
}

ResizeArgs Framing::adjustResize(const ResizeArgs& resize, const Dimensions& bbox) const
{
    Dimensions newSize = clampResize(resize.size, bbox);
    double newScale = newSize.width / postCropImageSize.width;
    return ResizeArgs(newSize, newScale);
}

Dimensions Framing::computeRelativeImageBBoxInFrame(const Dimensions& imgSize,
                                                    const Dimensions& framedSize) const
{
    if (imgSize.isDegenerate() || framedSize.isDegenerate()) {
        return Dimensions(0, 0);
    }

    double imgAspectRatio = imgSize.aspectRatio();

    Side side = pickReferenceSide(framing, imgSize);
    double scale = framing.relativeBorderSize;

    // Compute image and border lengths on basis side
    double frameBasis = side == Side::WIDTH ? framedSize.width : framedSize.height;
    double frameOther = side == Side::WIDTH ? framedSize.height : framedSize.width;

    auto computedSizes = computeImgAndBorderSize(frameBasis, scale);
    double imgBasis = computedSizes.first;

    // Compute image and border lengths for the non-basis side
    double imgBasisToOther = side == Side::WIDTH ? 1.0 / imgAspectRatio : imgAspectRatio;
    double imgOther = imgBasis * imgBasisToOther;

    // Find the maximum allowed image size considering min size limits
    double maxImageBasis = frameBasis;
    double maxImageOther = frameOther;
    if (framing.minSizeEnabled) {
        double minBorderBasis = static_cast<double>(
            side == Side::WIDTH ? framing.minWidth : framing.minHeight);
        double minBorderOther = static_cast<double>(
            side == Side::WIDTH ? framing.minHeight : framing.minWidth);

        maxImageOther = std::floor(frameOther - 2.0 * minBorderOther);
        maxImageBasis = std::floor(frameBasis - 2.0 * minBorderBasis);
    }

    // Image is too large to satisfy requirements:
    //   a. Min border size limit not satisfied
    //   b. Basis size is too small for the requested aspect ratio
    //      (i.e. original image clipped)
    //
    // Resize the image so that it fits in bounds
    if (imgOther > maxImageOther) {
        imgOther = maxImageOther;
        imgBasis = imgOther / imgBasisToOther;
    }
    if (imgBasis > maxImageBasis) {
        imgBasis = maxImageBasis;
        imgOther = imgBasis * imgBasisToOther;
    }

    if (side == Side::WIDTH) {
        return Dimensions(imgBasis, imgOther);
    } else {
        return Dimensions(imgOther, imgBasis);
    }
}

Dimensions Framing::computeUniformRelativeImageBBox(const Dimensions& imgSize,
                                                    const Dimensions& framedSize) const
{
    if (imgSize.isDegenerate() || framedSize.isDegenerate()) {
        return Dimensions(0, 0);
    }

    Side side = pickReferenceSide(framing, imgSize);
    double scale = framing.relativeBorderSize;

    // Compute image and border lengths on basis side
    double frameBasis = side == Side::WIDTH ? framedSize.width : framedSize.height;
    double frameOther = side == Side::WIDTH ? framedSize.height : framedSize.width;

    auto computedSizes = computeImgAndBorderSize(frameBasis, scale);
    double imgBasis = computedSizes.first;
    double border = computedSizes.second;

    // Compute image and border lengths for the non-basis side
    double imgAspectRatio = imgSize.aspectRatio();
    double imgBasisToOther = side == Side::WIDTH ? 1.0 / imgAspectRatio : imgAspectRatio;
    double imgOther = imgBasis * imgBasisToOther;

    // If the frame doesn't constrain the non-basis side length, we just need
    // to check the border minimum size. However, if the non-basis side is
    // constrained, we need to adjust the image size to fit while still
    // maintaining the border scale w.r.t. the basis side.
    double totalOther = imgOther + 2.0 * border;
    if (totalOther > frameOther) {
        // Let:
        // imgOther = imgBasis * imgBasisToOther
        // border = imgBasis * scale
        //
        // Want:
        // frameOther = imgOther + 2 * border
        //            = imgBasis * imgBasisToOther + 2 * scale * imgBasis
        //            = imgBasis * (imgBasisToOther + 2 * scale)
        //
        // Rearrange:
        // imgBasis = frameOther / (imgBasisToOther + 2 * scale)
        imgBasis = frameOther / (imgBasisToOther + 2.0 * scale);
        imgOther = imgBasis * imgBasisToOther;
        border = imgBasis * scale;
    }

    // Find the maximum allowed image size considering min size limits
    double maxImageBasis = frameBasis;
    double maxImageOther = frameOther;
    if (framing.minSizeEnabled) {
        double minBorder = static_cast<double>(
            side == Side::WIDTH ? framing.minWidth : framing.minHeight);

        maxImageBasis = std::floor(frameBasis - 2.0 * minBorder);
        maxImageOther = std::floor(frameOther - 2.0 * minBorder);
    }

    if (imgOther > maxImageOther) {
        imgOther = maxImageOther;
        imgBasis = imgOther / imgBasisToOther;
    }
    if (imgBasis > maxImageBasis) {
        imgBasis = maxImageBasis;
        imgOther = imgBasis * imgBasisToOther;
    }

    if (side == Side::WIDTH) {
        return Dimensions(imgBasis, imgOther);
    } else {
        return Dimensions(imgOther, imgBasis);
    }
}

ResizeArgs Framing::adjustResizeForFraming(const ResizeArgs& resize) const
{
    if (!framing.enabled) return resize;

    switch (framing.framingMethod) {
        case FramingMethod::BBOX:
            return resizeForBBox(resize);
        case FramingMethod::FIXED_SIZE:
            return resizeForFixedFrame(resize);
        case FramingMethod::STANDARD:
        default:
            // No limits on framed size so do nothing
            return resize;
    }
}

ResizeArgs Framing::resizeForFixedFrame(const ResizeArgs& args) const
{
    double framedWidth = framing.framedWidth;
    double framedHeight = framing.framedHeight;
    Dimensions frameSize(framedWidth, framedHeight);

    Dimensions bbox;
    if (framing.borderSizingMethod == BorderSizing::FIXED_SIZE) {
        auto length = [](double frame, double border) {
            return std::max(0.0, frame - 2.0 * border);
        };

        bbox = {
            length(framedWidth, framing.absWidth),
            length(framedHeight, framing.absHeight)
        };
    } else if (framing.borderSizingMethod == BorderSizing::UNIFORM_PERCENTAGE) {
        bbox = computeUniformRelativeImageBBox(args.size, frameSize);
    } else {
        bbox = computeRelativeImageBBoxInFrame(args.size, frameSize);
    }

    return adjustResize(args, bbox);
}

ResizeArgs Framing::resizeForBBox(const ResizeArgs& args) const
{
    Dimensions boundary(framing.framedWidth, framing.framedHeight);

    Dimensions bbox;
    if (framing.borderSizingMethod == BorderSizing::FIXED_SIZE) {
        auto length = [](double frame, double border) {
            return std::max(0.0, frame - 2.0 * border);
        };

        bbox = {
            length(boundary.width, framing.absWidth),
            length(boundary.height, framing.absHeight)
        };
    } else if (framing.borderSizingMethod == BorderSizing::UNIFORM_PERCENTAGE) {
        bbox = computeUniformRelativeImageBBox(args.size, boundary);
    } else {
        // For the requested aspect ratio, it must fit inside the requested
        // bounding box
        double aspectRatio = orientAspectRatio(framing, args.size);
        Dimensions ratioBBox = fromAspectRatio(boundary, aspectRatio);
        ratioBBox = clampToBBox(ratioBBox, boundary, INSIDE_BBOX);

        // Now we have the true max bounds for the framed image. Determine how the
        // original image fits inside these bounds. This process is the same as
        // in the fixed frame mode.
        bbox = computeRelativeImageBBoxInFrame(args.size, ratioBBox);
    }

    return adjustResize(args, bbox);
}

Dimensions Framing::computeFramedSize(const Dimensions& imgSize) const
{
    if (!framing.enabled) return imgSize;

    // For constrained frame sizes, check if the image size (without frame)
    // exceeds the constrained size. This indicates that a combination of
    // parameters caused the downscaling limit to be hit. In which case,
    // just return the original image size (i.e. don't insert border).
    //
    // If the image fits the constrained size, assume that previous resize
    // calculations were correct and trim off any excess borders. The excess
    // may be from rounding errors or hitting some downscaling limit.
    switch (framing.framingMethod) {
        case FramingMethod::BBOX:
        {
            Dimensions fixed(framing.framedWidth, framing.framedHeight);
            if (imgSize.inside(fixed)) {
                Dimensions framedSize = computeSizeWithBorders(imgSize);
                return clampToBBox(framedSize, fixed, INSIDE_BBOX);
            } else {
                return imgSize;
            }
        }
        case FramingMethod::FIXED_SIZE:
        {
            Dimensions fixed(framing.framedWidth, framing.framedHeight);
            return imgSize.inside(fixed) ? fixed : imgSize;
        }
        case FramingMethod::STANDARD:
        default:
            return computeSizeWithBorders(imgSize);
    }
}

Dimensions Framing::computeSizeWithBorders(const Dimensions& imgSize) const
{
    if (framing.borderSizingMethod == BorderSizing::FIXED_SIZE) {
        return Dimensions(imgSize.width + 2.0 * framing.absWidth,
                          imgSize.height + 2.0 * framing.absHeight);
    }

    Side side = pickReferenceSide(framing, imgSize);
    double scale = framing.relativeBorderSize;

    if (framing.borderSizingMethod == BorderSizing::UNIFORM_PERCENTAGE) {
        double borderSize = 0;
        if (side == Side::WIDTH) {
            borderSize = scale * imgSize.width;
            if (framing.minSizeEnabled && borderSize < framing.minWidth) {
                borderSize = framing.minWidth;
            }
        } else {
            borderSize = scale * imgSize.height;
            if (framing.minSizeEnabled && borderSize < framing.minHeight) {
                borderSize = framing.minHeight;
            }
        }

        return Dimensions(imgSize.width + 2.0 * borderSize,
                          imgSize.height + 2.0 * borderSize);
    }

    double aspectRatio = orientAspectRatio(framing, imgSize);

    Dimensions framedSize;
    if (side == Side::WIDTH) {
        framedSize.width = (1.0 + 2.0 * scale) * imgSize.width;
        framedSize.height = framedSize.width / aspectRatio;
    } else {
        framedSize.height = (1.0 + 2.0 * scale) * imgSize.height;
        framedSize.width = framedSize.height * aspectRatio;
    }

    // Check if the computed frame size satsifies the requested aspect ratio
    // without cutting off the original image. If the image is cut off, use
    // the smallest frame that preserves the original image and still
    // satisfies the requested aspect ratio.
    Dimensions minFramedSize = fromAspectRatio(imgSize, aspectRatio);
    Dimensions limit = imgSize;
    if (framing.minSizeEnabled) {
        limit.width += 2.0 * framing.minWidth;
        limit.height += 2.0 * framing.minHeight;
    }
    minFramedSize = clampToBBox(minFramedSize, limit, OUTSIDE_BBOX);

    if (minFramedSize.inside(framedSize)) {
        return framedSize;
    } else {
        return minFramedSize;
    }
}

}  // namespace

namespace rtengine
{

static inline float Lanc (float x, float a)
{
    if (x * x < 1e-6f) {
        return 1.0f;
    } else if (x * x > a * a) {
        return 0.0f;
    } else {
        x = static_cast<float> (rtengine::RT_PI) * x;
        return a * xsinf (x) * xsinf (x / a) / (x * x);
    }
}

void ImProcFunctions::Lanczos (const Imagefloat* src, Imagefloat* dst, float scale)
{

    const float delta = 1.0f / scale;
    const float a = 3.0f;
    const float sc = min (scale, 1.0f);
    const int support = static_cast<int> (2.0f * a / sc) + 1;

#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        // storage for precomputed parameters for horisontal interpolation
        float * wwh = new float[support * dst->getWidth()];
        int * jj0 = new int[dst->getWidth()];
        int * jj1 = new int[dst->getWidth()];

        // temporal storage for vertically-interpolated row of pixels
        float * lr = new float[src->getWidth()];
        float * lg = new float[src->getWidth()];
        float * lb = new float[src->getWidth()];

        // Phase 1: precompute coefficients for horisontal interpolation

        for (int j = 0; j < dst->getWidth(); j++) {

            // x coord of the center of pixel on src image
            float x0 = (static_cast<float> (j) + 0.5f) * delta - 0.5f;

            // weights for interpolation in horisontal direction
            float * w = wwh + j * support;

            // sum of weights used for normalization
            float ws = 0.0f;

            jj0[j] = max (0, static_cast<int> (floorf (x0 - a / sc)) + 1);
            jj1[j] = min (src->getWidth(), static_cast<int> (floorf (x0 + a / sc)) + 1);

            // calculate weights
            for (int jj = jj0[j]; jj < jj1[j]; jj++) {
                int k = jj - jj0[j];
                float z = sc * (x0 - static_cast<float> (jj));
                w[k] = Lanc (z, a);
                ws += w[k];
            }

            // normalize weights
            for (int k = 0; k < support; k++) {
                w[k] /= ws;
            }
        }

        // Phase 2: do actual interpolation
#ifdef _OPENMP
        #pragma omp for
#endif

        for (int i = 0; i < dst->getHeight(); i++) {

            // y coord of the center of pixel on src image
            float y0 = (static_cast<float> (i) + 0.5f) * delta - 0.5f;

            // weights for interpolation in y direction
            float w[support];
            for (auto& f : w) {
                f = 0.f;
            }

            // sum of weights used for normalization
            float ws = 0.0f;

            int ii0 = max (0, static_cast<int> (floorf (y0 - a / sc)) + 1);
            int ii1 = min (src->getHeight(), static_cast<int> (floorf (y0 + a / sc)) + 1);

            // calculate weights for vertical interpolation
            for (int ii = ii0; ii < ii1; ii++) {
                int k = ii - ii0;
                float z = sc * (y0 - static_cast<float> (ii));
                w[k] = Lanc (z, a);
                ws += w[k];
            }

            // normalize weights
            for (int k = 0; k < support; k++) {
                w[k] /= ws;
            }

            // Do vertical interpolation. Store results.
            for (int j = 0; j < src->getWidth(); j++) {

                float r = 0.0f, g = 0.0f, b = 0.0f;

                for (int ii = ii0; ii < ii1; ii++) {
                    int k = ii - ii0;

                    r += w[k] * src->r (ii, j);
                    g += w[k] * src->g (ii, j);
                    b += w[k] * src->b (ii, j);
                }

                lr[j] = r;
                lg[j] = g;
                lb[j] = b;
            }

            // Do horizontal interpolation
            for (int j = 0; j < dst->getWidth(); j++) {

                float * wh = wwh + support * j;

                float r = 0.0f, g = 0.0f, b = 0.0f;

                for (int jj = jj0[j]; jj < jj1[j]; jj++) {
                    int k = jj - jj0[j];

                    r += wh[k] * lr[jj];
                    g += wh[k] * lg[jj];
                    b += wh[k] * lb[jj];
                }

                dst->r (i, j) = /*CLIP*/ (r);//static_cast<int> (r));
                dst->g (i, j) = /*CLIP*/ (g);//static_cast<int> (g));
                dst->b (i, j) = /*CLIP*/ (b);//static_cast<int> (b));
            }
        }

        delete[] wwh;
        delete[] jj0;
        delete[] jj1;
        delete[] lr;
        delete[] lg;
        delete[] lb;
    }
}


void ImProcFunctions::Lanczos (const LabImage* src, LabImage* dst, float scale)
{
    const float delta = 1.0f / scale;
    constexpr float a = 3.0f;
    const float sc = min(scale, 1.0f);
    const int support = static_cast<int> (2.0f * a / sc) + 1;

    // storage for precomputed parameters for horizontal interpolation
    float* const wwh = new float[support * dst->W];
    int* const jj0 = new int[dst->W];
    int* const jj1 = new int[dst->W];

    // Phase 1: precompute coefficients for horizontal interpolation
    for (int j = 0; j < dst->W; j++) {

        // x coord of the center of pixel on src image
        float x0 = (static_cast<float> (j) + 0.5f) * delta - 0.5f;

        // weights for interpolation in horizontal direction
        float * w = wwh + j * support;

        // sum of weights used for normalization
        float ws = 0.0f;

        jj0[j] = max (0, static_cast<int> (floorf (x0 - a / sc)) + 1);
        jj1[j] = min (src->W, static_cast<int> (floorf (x0 + a / sc)) + 1);

        // calculate weights
        for (int jj = jj0[j]; jj < jj1[j]; jj++) {
            int k = jj - jj0[j];
            float z = sc * (x0 - static_cast<float> (jj));
            w[k] = Lanc (z, a);
            ws += w[k];
        }

        // normalize weights
        for (int k = 0; k < support; k++) {
            w[k] /= ws;
        }
    }

#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        // temporal storage for vertically-interpolated row of pixels
        AlignedBuffer<float> aligned_buffer_ll(src->W);
        AlignedBuffer<float> aligned_buffer_la(src->W);
        AlignedBuffer<float> aligned_buffer_lb(src->W);
        float* const lL = aligned_buffer_ll.data;
        float* const la = aligned_buffer_la.data;
        float* const lb = aligned_buffer_lb.data;
        // weights for interpolation in y direction
        float w[support] ALIGNED64;
        memset(w, 0, sizeof(w));

        // Phase 2: do actual interpolation
#ifdef _OPENMP
        #pragma omp for
#endif

        for (int i = 0; i < dst->H; i++) {
            // y coord of the center of pixel on src image
            float y0 = (static_cast<float> (i) + 0.5f) * delta - 0.5f;

            // sum of weights used for normalization
            float ws = 0.0f;

            int ii0 = max (0, static_cast<int> (floorf (y0 - a / sc)) + 1);
            int ii1 = min (src->H, static_cast<int> (floorf (y0 + a / sc)) + 1);

            // calculate weights for vertical interpolation
            for (int ii = ii0; ii < ii1; ii++) {
                int k = ii - ii0;
                float z = sc * (y0 - static_cast<float> (ii));
                w[k] = Lanc (z, a);
                ws += w[k];
            }

            // normalize weights
            for (int k = 0; k < support; k++) {
                w[k] /= ws;
            }

            // Do vertical interpolation. Store results.
            int j = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)
            __m128 Lv, av, bv, wkv;

            for (j = 0; j < src->W - 3; j += 4) {
                Lv = ZEROV;
                av = ZEROV;
                bv = ZEROV;

                for (int ii = ii0; ii < ii1; ii++) {
                    int k = ii - ii0;
                    wkv = F2V(w[k]);
                    Lv += wkv * LVFU(src->L[ii][j]);
                    av += wkv * LVFU(src->a[ii][j]);
                    bv += wkv * LVFU(src->b[ii][j]);
                }

                STVF(lL[j], Lv);
                STVF(la[j], av);
                STVF(lb[j], bv);
            }
#endif

            for (; j < src->W; ++j) {
                float Ll = 0.0f, La = 0.0f, Lb = 0.0f;

                for (int ii = ii0; ii < ii1; ++ii) {
                    int k = ii - ii0;

                    Ll += w[k] * src->L[ii][j];
                    La += w[k] * src->a[ii][j];
                    Lb += w[k] * src->b[ii][j];
                }

                lL[j] = Ll;
                la[j] = La;
                lb[j] = Lb;
            }

            // Do horizontal interpolation
            for (int x = 0; x < dst->W; ++x) {
                float * wh = wwh + support * x;
                float Ll = 0.0f, La = 0.0f, Lb = 0.0f;

                for (int jj = jj0[x]; jj < jj1[x]; ++jj) {
                    int k = jj - jj0[x];

                    Ll += wh[k] * lL[jj];
                    La += wh[k] * la[jj];
                    Lb += wh[k] * lb[jj];
                }

                dst->L[i][x] = Ll;
                dst->a[i][x] = La;
                dst->b[i][x] = Lb;
            }
        }
    }
    delete[] jj0;
    delete[] jj1;
    delete[] wwh;
}

double ImProcFunctions::resizeScale (const ProcParams* params, int fw, int fh, int &imw, int &imh)
{
    imw = fw;
    imh = fh;

    if (!params || !params->resize.enabled) {
        return 1.0;
    }

    // get the resize parameters
    int refw, refh;
    double dScale;

    if (params->crop.enabled && params->resize.appliesTo == "Cropped area") {
        // the resize values applies to the crop dimensions
        refw = params->crop.w;
        refh = params->crop.h;
    } else {
        // the resize values applies to the image dimensions
        // if a crop exists, it will be resized to the calculated scale
        refw = fw;
        refh = fh;
    }

    switch (params->resize.dataspec) {
        case (1):
            // Width
            dScale = (double)params->resize.width / (double)refw;
            break;

        case (2):
            // Height
            dScale = (double)params->resize.height / (double)refh;
            break;

        case (3):

            // FitBox
            if ((double)refw / (double)refh > (double)params->resize.width / (double)params->resize.height) {
                dScale = (double)params->resize.width / (double)refw;
            } else {
                dScale = (double)params->resize.height / (double)refh;
            }
            dScale = (dScale > 1.0 && !params->resize.allowUpscaling) ? 1.0 : dScale;

            break;
            
        case (4):
        
            // Long Edge
            if (refw > refh) {
                dScale = (double)params->resize.longedge / (double)refw;
            } else {
                dScale = (double)params->resize.longedge / (double)refh;
            }
            break;
            
        case (5):
        
            // Short Edge
            if (refw > refh) {
                dScale = (double)params->resize.shortedge / (double)refh;
            } else {
                dScale = (double)params->resize.shortedge / (double)refw;
            }
            break;

        default:
            // Scale
            dScale = params->resize.scale;
            break;
    }

    if (params->crop.enabled && params->resize.appliesTo == "Full image") {
        imw = params->crop.w;
        imh = params->crop.h;
    } else {
        imw = refw;
        imh = refh;
    }

    if (fabs (dScale - 1.0) <= 1e-5) {
        return 1.0;
    } else {
        imw = computeSize(imw, dScale);
        imh = computeSize(imh, dScale);
        return dScale;
    }
}

void ImProcFunctions::resize (Imagefloat* src, Imagefloat* dst, float dScale)
{
#ifdef PROFILE
    time_t t1 = clock();
#endif

    if (params->resize.method != "Nearest" ) {
        Lanczos (src, dst, dScale);
    } else {
        // Nearest neighbour algorithm
#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif

        for (int i = 0; i < dst->getHeight(); i++) {
            int sy = i / dScale;
            sy = LIM (sy, 0, src->getHeight() - 1);

            for (int j = 0; j < dst->getWidth(); j++) {
                int sx = j / dScale;
                sx = LIM (sx, 0, src->getWidth() - 1);
                dst->r (i, j) = src->r (sy, sx);
                dst->g (i, j) = src->g (sy, sx);
                dst->b (i, j) = src->b (sy, sx);
            }
        }
    }

#ifdef PROFILE
    time_t t2 = clock();
    std::cout << "Resize: " << params->resize.method << ": "
              << (float) (t2 - t1) / CLOCKS_PER_SEC << std::endl;
#endif
}

ImProcFunctions::FramingData ImProcFunctions::framing(const FramingArgs& args) const
{
    FramingData result;
    result.enabled = false;
    result.imgWidth = args.resizeWidth;
    result.imgHeight = args.resizeHeight;
    result.scale = args.resizeScale;
    result.framedWidth = args.resizeWidth;
    result.framedHeight = args.resizeHeight;

    if (!args.params || !args.params->resize.enabled) return result;
    if (!args.params->framing.enabled) return result;

    // For these calculations, try to keep everything as doubles to minimize
    // rounding errors from intermediate steps!

    Framing util(*params, args.cropWidth, args.cropHeight);
    ResizeArgs resize(Dimensions(args.resizeWidth, args.resizeHeight), args.resizeScale);
    ResizeArgs adjusted = util.adjustResizeForFraming(resize);
    Dimensions framedSize = util.computeFramedSize(adjusted.size);

    result.enabled = true;
    result.imgWidth = std::round(adjusted.size.width);
    result.imgHeight = std::round(adjusted.size.height);
    result.scale = adjusted.scale;
    result.framedWidth = std::round(framedSize.width);
    result.framedHeight = std::round(framedSize.height);

    return result;
}

// Draws the border around the input image.
// It should be called after gamma correction.
Imagefloat* ImProcFunctions::drawFrame(Imagefloat* rgb, const FramingParams& params,
                                       const FramingData& dims) const
{
    if (rgb->getWidth() > dims.framedWidth || rgb->getHeight() >  dims.framedHeight) {
        return rgb;
    }
    if (rgb->getWidth() == dims.framedWidth && rgb->getHeight() == dims.framedHeight) {
        return rgb;
    }

    Imagefloat* framed = new Imagefloat(dims.framedWidth, dims.framedHeight);

    // Color::gamma2curve expects a 16-bit value, but the GUI sliders are
    // using 8-bit values. Step up the user value to 16-bits.
    auto clip = [](int v) -> int {
        int sanitized = std::max(0, std::min(v, 255));

        double normalized = static_cast<double>(sanitized) / 255.0;
        return normalized * 65535.0;
    };

    float r = Color::gamma2curve[clip(params.borderRed)];
    float g = Color::gamma2curve[clip(params.borderGreen)];
    float b = Color::gamma2curve[clip(params.borderBlue)];

#ifdef _OPENMP
    #pragma omp parallel for if (multiThread)
#endif
    for (int i = 0; i < framed->getHeight(); i++) {
        for (int j = 0; j < framed->getWidth(); j++) {
            framed->r(i, j) = r;
            framed->g(i, j) = g;
            framed->b(i, j) = b;
        }
    }

    auto offset = [](int inner, int outer) {
        double u = inner;
        double v = outer;
        return static_cast<int>(std::round((v - u) / 2.0));
    };
    int rowOffset = offset(rgb->getHeight(), framed->getHeight());
    int colOffset = offset(rgb->getWidth(), framed->getWidth());

#ifdef _OPENMP
    #pragma omp parallel for if (multiThread)
#endif
    for (int i = 0; i < rgb->getHeight(); i++) {
        for (int j = 0; j < rgb->getWidth(); j++) {
            int row = i + rowOffset;
            int col = j + colOffset;

            framed->r(row, col) = rgb->r(i, j);
            framed->g(row, col) = rgb->g(i, j);
            framed->b(row, col) = rgb->b(i, j);
        }
    }

    const char* anno = params.borderAnnotation.c_str();
    int textlen = (int)strlen(anno);
    int fw = framed->getWidth();
    int fh = framed->getHeight();
    int line_stride = (int) (framed->r(1) - framed->r(0));
    if (fh >= FONTH+2 && textlen>0) {
        float* channels[3] = {
            framed->r(fh - FONTH - 2),
            framed->g(fh - FONTH - 2),
            framed->b(fh - FONTH - 2)
        };
        const int margin = (framed->getWidth() - rgb->getWidth()) / 2;
        const int xpos = fw - textlen * FONTXPITCH - margin;
        const float avgval = (r+g+b)/3;
        // write the text in black or white, depending on border colour.
        const float value = avgval > 32768.0f ? 0.0f : 65535.0f;
#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif
        for (int chan=0; chan<3; ++chan) {
            float* buf = channels[chan];
            text_write_line(anno, value, buf, fw, fh, xpos, line_stride);
        }
    }

    delete rgb;
    return framed;
}

}  // namespace rtengine
