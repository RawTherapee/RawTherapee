/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2024 Rawtherapee developers
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
#pragma once

#include <memory>

#include "lcp.h"
#include "metadata.h"
#include "procparams.h"
#include "rtengine.h"

namespace rtengine
{

/* MetadataLensCorrection is an abstract class for various lens correction based on raw file metadata
 this metadata is vendor dependent */
class MetadataLensCorrection : public LensCorrection,
                               public NonCopyable
{
public:
    // pixelSkip is the number of sensor pixels per working pixel along each axis.
    virtual void initCorrections(int width, int height, const procparams::CoarseTransformParams &coarse, int rawRotationDeg, double pixelSkip = 1) = 0;
};

/* CenterRadiusMetadataLensCorrection is an abstract class the extends MetadataLensCorrection to easily handle center radius based corrections */
class CenterRadiusMetadataLensCorrection : public MetadataLensCorrection
{
public:
    CenterRadiusMetadataLensCorrection(const FramesMetaData *meta);

    void process(double &x, double &y, int cx, int cy, int channel, bool dist, bool ca) const;

    void correctDistortionAndCA(double &x, double &y, int cx, int cy, int channel) const override;
    void correctDistortion(double &x, double &y, int cx, int cy) const override;
    void correctCA(double &x, double &y, int cx, int cy, int channel) const override;
    void processVignette(int width, int height, float **rawData) const override;
    void processVignette3Channels(int width, int height, float **rawData) const override;

    void processVignetteNChannels(int width, int height, float **rawData, int channels) const;
    void initCorrections(int width, int height, const procparams::CoarseTransformParams &coarse, int rawRotationDeg, double pixelSkip) override;

    /* Implementers should implement the below methods */
    virtual bool hasDistortionCorrection() const override = 0;
    virtual bool hasCACorrection() const override = 0;
    virtual bool hasVignettingCorrection() const override = 0;

    /* These methods should return the distortion correction factor (cf) for the
     * provided radius rout (radius of the output image (corrected)).
     * So rin = rout * cf
     * */
    virtual double distortionCorrectionFactor(double rout) const = 0;
    virtual double caCorrectionFactor(double rout, int channel) const = 0;
    virtual double distortionAndCACorrectionFactor(double rout, int channel) const = 0;

    /* This methods should return the vignetting correction factor (cf) for the
     * provided radius */
    virtual double vignettingCorrectionFactor(double r) const = 0;

protected:
    bool swap_xy;
    double w2;
    double h2;
    double rf;
    Exiv2Metadata metadata;
};

/* MetadataLensCorrectionFinder tries to find and return MetadataLensCorrection for the provided metadata */
class MetadataLensCorrectionFinder
{
public:
    enum class Purpose { Any, CameraFraming };
    static std::unique_ptr<MetadataLensCorrection> findCorrection(const FramesMetaData *meta, Purpose purpose = Purpose::Any);

private:
    static std::unique_ptr<MetadataLensCorrection> findNikonCorrection(const FramesMetaData *meta);
};

} // namespace rtengine
