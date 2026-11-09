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
#include <array>
#include <iostream>

#include "lensmetadata.h"

namespace rtengine
{

namespace
{

/* interpolateLinearSpline does a simple linear spline interpolation. Values
 * outside the external knots will return the value of the nearest knot without
 * any additional interpolation. */
double interpolateLinearSpline(const std::vector<double> &xi, const std::vector<double> &yi, double x)
{
    if (x < xi[0]) {
        return yi[0];
    }

    for (size_t i = 1; i < xi.size(); i++) {
        if (x >= xi[i - 1] && x <= xi[i]) {
            double dydx = (yi[i] - yi[i - 1]) / (xi[i] - xi[i - 1]);

            return yi[i - 1] + (x - xi[i - 1]) * dydx;
        }
    }

    return yi[yi.size() - 1];
}

} // namespace

CenterRadiusMetadataLensCorrection::CenterRadiusMetadataLensCorrection(const FramesMetaData *meta) :
    swap_xy(false)
{
    metadata = Exiv2Metadata(meta->getFileName());
    metadata.load();
}

void CenterRadiusMetadataLensCorrection::initCorrections(int width, int height, const procparams::CoarseTransformParams &coarse, int rawRotationDeg)
{
    if (rawRotationDeg >= 0) {
        int rot = (coarse.rotate + rawRotationDeg) % 360;
        swap_xy = (rot == 90 || rot == 270);
        if (swap_xy) {
            std::swap(width, height);
        }
    }

    w2 = width * 0.5f;
    h2 = height * 0.5f;
    rf = 1 / std::sqrt(SQR(w2) + SQR(h2));
}

void CenterRadiusMetadataLensCorrection::process(double &x, double &y, int cx, int cy, int channel, bool dist, bool ca) const
{
    double xx = x + cx;
    double yy = y + cy;
    if (swap_xy) {
        std::swap(xx, yy);
    }

    double xc = xx - w2;
    double yc = yy - h2;

    double rout = rf * std::sqrt(SQR(xc) + SQR(yc));
    double cf = 1;
    if (dist && ca) {
        cf = distortionAndCACorrectionFactor(rout, channel);
    } else if (dist) {
        cf = distortionCorrectionFactor(rout);
    } else if (ca) {
        cf = caCorrectionFactor(rout, channel);
    }

    x = cf * xc + w2;
    y = cf * yc + h2;

    if (swap_xy) {
        std::swap(x, y);
    }
    x -= cx;
    y -= cy;
}

void CenterRadiusMetadataLensCorrection::correctDistortionAndCA(double &x, double &y, int cx, int cy, int channel) const
{
    if (!hasDistortionCorrection() || !hasCACorrection()) {
        return;
    }

    process(x, y, cx, cy, channel, true, true);
}

void CenterRadiusMetadataLensCorrection::correctDistortion(double &x, double &y, int cx, int cy) const
{
    if (!hasDistortionCorrection()) {
        return;
    }

    process(x, y, cx, cy, 1, true, false);
}

void CenterRadiusMetadataLensCorrection::correctCA(double &x, double &y, int cx, int cy, int channel) const
{
    if (!hasCACorrection()) {
        return;
    }

    process(x, y, cx, cy, channel, false, true);
}

void CenterRadiusMetadataLensCorrection::processVignetteNChannels(int width, int height, float **rawData, int channels) const
{
    if (!hasVignettingCorrection()) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double xc = x - w2;
            double yc = y - h2;
            double sf = vignettingCorrectionFactor(rf * std::sqrt(SQR(xc) + SQR(yc)));
            for (int c = 0; c < channels; c++) {
                rawData[y][x + c] *= sf;
            }
        }
    }
}

void CenterRadiusMetadataLensCorrection::processVignette(int width, int height, float **rawData) const
{
    return processVignetteNChannels(width, height, rawData, 1);
}

void CenterRadiusMetadataLensCorrection::processVignette3Channels(int width, int height, float **rawData) const
{
    return processVignetteNChannels(width, height, rawData, 3);
}

/* Fuji, Sony, Olympus metadata handling and algorithms adapted from
 * - src/iop/lens.cc
 * - src/common/exif.cc
 * in darktable 4.6 */
/*
    This file is part of darktable,
    Copyright (C) 2019-2024 darktable developers.

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

class SonyMetadataLensCorrection : public CenterRadiusMetadataLensCorrection
{
public:
    SonyMetadataLensCorrection(const FramesMetaData *meta) :
        CenterRadiusMetadataLensCorrection(meta)
    {
        parse();
        setup();
    }

private:
    int nc;
    std::array<short, 16> distortion;
    std::array<short, 16> ca_r;
    std::array<short, 16> ca_b;
    std::array<short, 16> vignetting;

    std::vector<double> knots;
    std::vector<double> dist;
    std::array<std::vector<double>, 3> ca;
    std::vector<double> vig;

    void parse()
    {
        if (Exiv2::versionNumber() < EXIV2_MAKE_VERSION(0, 27, 4)) {
            throw std::runtime_error("cannot get Sony correction data, too old exiv2 version " + Exiv2::versionString());
        }

        auto &exif = metadata.exifData();

        auto posd = exif.findKey(Exiv2::ExifKey("Exif.SubImage1.DistortionCorrParams"));
        auto posc = exif.findKey(Exiv2::ExifKey("Exif.SubImage1.ChromaticAberrationCorrParams"));
        auto posv = exif.findKey(Exiv2::ExifKey("Exif.SubImage1.VignettingCorrParams"));

        /* Sony metadata corrections parameters define some splines with N knots */
        if (posd == exif.end() || posc == exif.end() || posv == exif.end()) {
            throw std::runtime_error("cannot get Sony correction data");
        }

        const int nc = to_long(posd);
        if (nc <= 16 && 2 * nc == to_long(posc) && nc == to_long(posv)) {
            this->nc = nc;
            for (int i = 0; i < nc; i++) {
                distortion[i] = to_long(posd, i + 1);
                ca_r[i] = to_long(posc, i + 1);
                ca_b[i] = to_long(posc, nc + i + 1);
                vignetting[i] = to_long(posv, i + 1);
            }
        } else {
            throw std::runtime_error("cannot get Sony correction data");
        }
    }

    void setup()
    {
        knots.resize(nc);
        dist.resize(nc);
        vig.resize(nc);
        for (int i = 0; i < 3; ++i) {
            ca[i].resize(nc);
        }

        for (int i = 0; i < this->nc; i++) {
            knots[i] = (i + 0.5) / (nc - 1);

            dist[i] = distortion[i] * powf(2, -14) + 1;

            ca[0][i] = ca[1][i] = ca[2][i] = 1.f;
            ca[0][i] *= ca_r[i] * powf(2, -21) + 1;
            ca[2][i] *= ca_b[i] * powf(2, -21) + 1;

            vig[i] = 1 / powf(2, 0.5f - powf(2, vignetting[i] * powf(2, -13) - 1));
        }
    }

    double distortionCorrectionFactor(double rout) const override
    {
        return interpolateLinearSpline(knots, dist, rout);
    }

    double caCorrectionFactor(double rout, int channel) const override
    {
        return interpolateLinearSpline(knots, ca[channel], rout);
    }

    double distortionAndCACorrectionFactor(double rout, int channel) const override
    {
        return distortionCorrectionFactor(rout) * caCorrectionFactor(rout, channel);
    }

    double vignettingCorrectionFactor(double r) const override
    {
        return interpolateLinearSpline(knots, vig, r);
    }

    bool hasDistortionCorrection() const override { return true; }
    bool hasVignettingCorrection() const override { return true; }
    bool hasCACorrection() const override { return true; }
};

class FujiMetadataLensCorrection : public CenterRadiusMetadataLensCorrection
{
public:
    FujiMetadataLensCorrection(const FramesMetaData *meta) :
        CenterRadiusMetadataLensCorrection(meta)
    {
        parse();
        setup();
    }

private:
    const static int MAXKNOTS = 16;

    int nc;
    double cropf;

    std::array<float, MAXKNOTS> fuji_knots;
    std::array<float, MAXKNOTS> fuji_distortion;
    std::array<float, MAXKNOTS> fuji_ca_r;
    std::array<float, MAXKNOTS> fuji_ca_b;
    std::array<float, MAXKNOTS> fuji_vignetting;

    std::vector<double> knots_dist;
    std::vector<double> dist;
    std::array<std::vector<double>, 3> ca;
    std::vector<double> knots_vig;
    std::vector<double> vig;

    void parse()
    {
        if (Exiv2::versionNumber() < EXIV2_MAKE_VERSION(0, 27, 4)) {
            throw std::runtime_error("cannot get Fuji correction data, too old exiv2 version " + Exiv2::versionString());
        }

        auto &exif = metadata.exifData();

        /* FujiFilm metadata corrections parameters define some splines with N knots */
        auto posd = exif.findKey(Exiv2::ExifKey("Exif.Fujifilm.GeometricDistortionParams"));
        auto posc = exif.findKey(Exiv2::ExifKey("Exif.Fujifilm.ChromaticAberrationParams"));
        auto posv = exif.findKey(Exiv2::ExifKey("Exif.Fujifilm.VignettingParams"));

        // X-Trans IV/V
        if (posd != exif.end() && posc != exif.end() && posv != exif.end() &&
            posd->count() == 19 && posc->count() == 29 && posv->count() == 19) {
            const int nc = 9;
            this->nc = nc;

            for (int i = 0; i < nc; i++) {
                const float kd = posd->toFloat(i + 1);
                const float kc = posc->toFloat(i + 1);
                const float kv = posv->toFloat(i + 1);

                // Check that the knots position is the same for distortion, ca and vignetting,
                if (kd != kc || kd != kv) {
                    throw std::runtime_error("cannot get Fuji correction data: unexpected data");
                }

                fuji_knots[i] = kd;
                fuji_distortion[i] = posd->toFloat(i + 10);
                fuji_ca_r[i] = posc->toFloat(i + 10);
                fuji_ca_b[i] = posc->toFloat(i + 19);
                fuji_vignetting[i] = posv->toFloat(i + 10);
            }

            // Account for the 1.25x crop modes in some Fuji cameras
            auto it = exif.findKey(Exiv2::ExifKey("Exif.Fujifilm.CropMode"));

            if (it != exif.end() && (to_long(it) == 2 || to_long(it) == 4)) {
                cropf = 1.25f;
            } else {
                cropf = 1;
            }
        }
        // X-Trans I/II/III
        else if (posd != exif.end() && posc != exif.end() && posv != exif.end() &&
                 posd->count() == 23 && posc->count() == 31 && posv->count() == 23) {
            const int nc = 11;
            this->nc = nc;

            for (int i = 0; i < nc; i++) {
                const float kd = posd->toFloat(i + 1);
                float kc = 0;
                // ca data doesn't provide first knot (0)
                if (i != 0) kc = posc->toFloat(i);
                const float kv = posv->toFloat(i + 1);
                // check that the knots position is the same for distortion, ca and vignetting,
                if (kd != kc || kd != kv) {
                    throw std::runtime_error("cannot get Fuji correction data: unexpected data");
                }

                fuji_knots[i] = kd;
                fuji_distortion[i] = posd->toFloat(i + 12);

                // ca data doesn't provide first knot (0)
                if (i == 0) {
                    fuji_ca_r[i] = 0;
                    fuji_ca_b[i] = 0;
                } else {
                    fuji_ca_r[i] = posc->toFloat(i + 10);
                    fuji_ca_b[i] = posc->toFloat(i + 20);
                }
                fuji_vignetting[i] = posv->toFloat(i + 12);
            }

            // Account for the 1.25x crop modes in some Fuji cameras
            auto it = exif.findKey(Exiv2::ExifKey("Exif.Fujifilm.CropMode"));

            if (it != exif.end() && (to_long(it) == 2 || to_long(it) == 4)) {
                cropf = 1.25f;
            } else {
                cropf = 1;
            }
        } else {
            throw std::runtime_error("cannot get Fuji correction data");
        }
    }

    void setup()
    {
        std::vector<double> knots_in;
        std::vector<double> distortion_in;
        std::vector<double> ca_r_in;
        std::vector<double> ca_b_in;

        // add a knot with no corrections at 0 value if not existing
        int size = nc;
        if (fuji_knots[0] > 0.f) {
            knots_in.push_back(0);
            distortion_in.push_back(1);
            ca_r_in.push_back(0);
            ca_b_in.push_back(0);

            knots_vig.push_back(0);
            vig.push_back(1);

            size++;
        }

        knots_in.reserve(size);
        vig.reserve(size);

        for (int i = 0; i < nc; i++) {
            knots_in.push_back(cropf * fuji_knots[i]);
            distortion_in.push_back(fuji_distortion[i] / 100 + 1);
            ca_r_in.push_back(fuji_ca_r[i]);
            ca_b_in.push_back(fuji_ca_b[i]);

            // vignetting correction is applied before distortion correction. So the
            // spline is related to the source image before distortion.
            knots_vig.push_back(cropf * fuji_knots[i]);

            vig.push_back(100 / fuji_vignetting[i]);
        }

        knots_dist.resize(MAXKNOTS);
        dist.resize(MAXKNOTS);
        for (int i = 0; i < 3; ++i) {
            ca[i].resize(MAXKNOTS);
        }

        // convert from spline related to source image (input is source image
        // radius) to spline related to dest image (input is dest image radius)
        for (int i = 0; i < MAXKNOTS; i++) {
            const double rin = static_cast<double>(i) / static_cast<double>(nc - 1);
            const double m = interpolateLinearSpline(knots_in, distortion_in, rin);
            const double r = rin / m;
            knots_dist[i] = r;

            dist[i] = m;

            const double mcar = interpolateLinearSpline(knots_in, ca_r_in, rin);
            const double mcab = interpolateLinearSpline(knots_in, ca_b_in, rin);

            ca[0][i] = ca[1][i] = ca[2][i] = 1.f;
            ca[0][i] *= mcar + 1;
            ca[2][i] *= mcab + 1;
        }
    }

    double distortionCorrectionFactor(double rout) const override
    {
        return interpolateLinearSpline(knots_dist, dist, rout);
    }

    double caCorrectionFactor(double rout, int channel) const override
    {
        return interpolateLinearSpline(knots_dist, ca[channel], rout);
    }

    double distortionAndCACorrectionFactor(double rout, int channel) const override
    {
        return distortionCorrectionFactor(rout) * caCorrectionFactor(rout, channel);
    }

    double vignettingCorrectionFactor(double r) const override
    {
        return interpolateLinearSpline(knots_vig, vig, r);
    }

    bool hasDistortionCorrection() const override { return true; }
    bool hasVignettingCorrection() const override { return true; }
    bool hasCACorrection() const override { return true; }
};

class OlympusMetadataLensCorrection : public CenterRadiusMetadataLensCorrection
{
public:
    OlympusMetadataLensCorrection(const FramesMetaData *meta) :
        CenterRadiusMetadataLensCorrection(meta), has_dist(false), has_ca(false)
    {
        parse();
    }

private:
    bool has_dist, has_ca;

    double drs;
    double dk2;
    double dk4;
    double dk6;

    double car0;
    double car2;
    double car4;
    double cab0;
    double cab2;
    double cab4;

    void parse()
    {
        if (Exiv2::versionNumber() < EXIV2_MAKE_VERSION(0, 27, 4)) {
            throw std::runtime_error("cannot get Olympus correction data, too old exiv2 version " + Exiv2::versionString());
        }

        auto &exif = metadata.exifData();

        std::array<double, 4> distortion;
        std::array<double, 6> cacorr;

        auto it = exif.findKey(Exiv2::ExifKey("Exif.OlympusIp.0x150a"));
        if (it != exif.end() && it->count() == 4) {
            for (int i = 0; i < 4; ++i) {
                distortion[i] = it->toFloat(i);
            }
            has_dist = true;
        }

        it = exif.findKey(Exiv2::ExifKey("Exif.OlympusIp.0x150c"));
        if (it != exif.end() && it->count() == 6) {
            for (int i = 0; i < 6; ++i) {
                cacorr[i] = it->toFloat(i);
            }
            has_ca = true;
        }

        if (has_dist) {
            drs = distortion[3];
            dk2 = distortion[0];
            dk4 = distortion[1];
            dk6 = distortion[2];
        }

        if (has_ca) {
            car0 = cacorr[0];
            car2 = cacorr[1];
            car4 = cacorr[2];
            cab0 = cacorr[3];
            cab2 = cacorr[4];
            cab4 = cacorr[5];
        }

        if (!has_dist && !has_ca) {
            throw std::runtime_error("no Olympus correction data");
        }
    }

    double distortionCorrectionFactor(double rout) const override
    {
        // The distortion polynomial maps a radius Rout in the output
        // (undistorted) image, where the corner is defined as Rout=1, to a
        // radius in the input (distorted) image, where the corner is defined
        // as Rin=1.
        // Rin = Rout*drs * (1 + dk2 * (Rout*drs)^2 + dk4 * (Rout*drs)^4 + dk6 * (Rout*drs)^6)
        //
        // cf is Rin / Rout.

        const double rs2 = std::pow(rout * drs, 2);
        const double cf = drs * (1 + rs2 * (dk2 + rs2 * (dk4 + rs2 * dk6)));

        return cf;
    }

    double caCorrectionFactor(double rout, int channel) const override
    {
        // ca corrects only red and blue channel
        if (channel != 0 && channel != 2) return 1;

        // CA correction is applied as:
        // Rin = Rout * ((1 + car0) + car2 * Rout^2 + car4 * Rout^4)
        //
        // cf is Rin / Rout.

        const double r2 = powf(rout, 2);
        if (channel == 0) {
            return 1 + (car0 + r2 * (car2 + r2 * car4));
        } else if (channel == 2) {
            return 1 + (cab0 + r2 * (cab2 + r2 * cab4));
        }

        return 1;
    }

    double distortionAndCACorrectionFactor(double rout, int channel) const override
    {
        return distortionCorrectionFactor(rout) * caCorrectionFactor(rout, channel);
    }

    double vignettingCorrectionFactor(double r) const override
    {
        return 1;
    }

    bool hasDistortionCorrection() const override { return has_dist; }
    // Olympus cameras have a shading correction option that fixes vignetting
    // already in the raw file. Looks like they don't report vignetting
    // correction parameters inside metadata even if shading correction is
    // disabled.
    bool hasVignettingCorrection() const override { return false; }
    bool hasCACorrection() const override { return has_ca; }
};

/* Panasonic metadata lens correction
 * Currently disabled since the algorithm is not stable and works well with only some lenses.
 *
 * Data extraction and distortion correction formula from:
 * https://web.archive.org/web/20120701131817/https://syscall.eu/#pana
 *
 * TODO(sgotti)
 * * CA corrections not yet reverse engineered.
 * * From a post on the exiftool forum:
 * https://exiftool.org/forum/index.php?topic=9366.0
 * looks like there's another additional tag that provides similar data and it's
 * used by newer cameras.
 */
class PanasonicMetadataLensCorrection : public CenterRadiusMetadataLensCorrection
{
public:
    PanasonicMetadataLensCorrection(const FramesMetaData *meta) :
        CenterRadiusMetadataLensCorrection(meta), has_dist(false), a(0), b(0), c(0)
    {
        // Currently disabled since the algorithm is not stable and works well with only some lenses.
        throw std::runtime_error("panasonic correction disabled as it's not yet working properly");

        // parse();
    }

private:
    bool has_dist;
    double scale, a, b, c;

    void parse()
    {
        if (Exiv2::versionNumber() < EXIV2_MAKE_VERSION(0, 27, 4)) {
            throw std::runtime_error("cannot get Panasonic correction data, too old exiv2 version " + Exiv2::versionString());
        }

        auto &exif = metadata.exifData();

        auto it = exif.findKey(Exiv2::ExifKey("Exif.PanasonicRaw.0x0119"));
        if (it != exif.end()) {
            std::vector<Exiv2::byte> buf;
            buf.resize(it->value().size());
            it->value().copy(buf.data(), Exiv2::littleEndian);

            const Exiv2::byte *data = buf.data();
            // n is currently unused
            // uint32_t n = Exiv2::getShort(data + 24, Exiv2::littleEndian);
            scale = 1.0f / (1.0f + Exiv2::getShort(data + 10, Exiv2::littleEndian) / 32768.0f);
            a = Exiv2::getShort(data + 16, Exiv2::littleEndian) / 32768.0f;
            b = Exiv2::getShort(data + 8, Exiv2::littleEndian) / 32768.0f;
            c = Exiv2::getShort(data + 22, Exiv2::littleEndian) / 32768.0f;

            has_dist = true;
        }
    }

    double distortionCorrectionFactor(double rout) const override
    {
        const double rs2 = std::pow(rout, 2);

        const double rin = scale * rout * (1 + rs2 * (a + rs2 * (b + rs2 * c)));
        const double cf = rout / rin;

        return cf;
    }

    double caCorrectionFactor(double rout, int channel) const override
    {
        return 1;
    }

    double distortionAndCACorrectionFactor(double rout, int channel) const override
    {
        return distortionCorrectionFactor(rout);
    }

    double vignettingCorrectionFactor(double r) const override
    {
        return 1;
    }

    bool hasDistortionCorrection() const override { return has_dist; }
    // Panasonic cameras have a shading correction option that fixes vignetting
    // already in the raw file. Looks like they don't report vignetting
    // correction parameters inside metadata even if shading correction is
    // disabled.
    bool hasVignettingCorrection() const override { return false; }
    bool hasCACorrection() const override { return false; }
};

// Class DNGMetadatalensCorrection handles OpcodeList3 operations: operations to
// be done after demosaicing.
// OpcodeList1 is already handled by rawimagesource.cc. OpcodeList2 is not yet
// handled by rawtherapee.
// TODO(sgotti): dng spec provides clear rules on how and when to process the
// various opcodeList1/2/3 and the order of the various operations that should
// be done.
// Currently we only handle a subset of all the available opcodes and only one
// WarpRectilinar for distortion and FixVignetteRadial for vignetting (that's
// usually the case with Leica DNGs and ADC generated DNGs).
// This should be extended to support more exotic operations lists (i.e.
// multiple WarpRectilinear)
class DNGMetadataLensCorrection : public MetadataLensCorrection
{
public:
    DNGMetadataLensCorrection(const FramesMetaData *meta) :
        MetadataLensCorrection(), swap_xy(false)
    {
        metadata = Exiv2Metadata(meta->getFileName());
        metadata.load();

        parse();
    }

private:
    Exiv2Metadata metadata;

    bool swap_xy;
    int width, height;

    double crx_d;
    double cry_d;
    double crx_v;
    double cry_v;

    double cx_d;
    double cy_d;
    double m_v;

    double cx_v;
    double cy_v;
    double m_d;

    int planes;

    bool has_dist, has_ca, has_vign;
    std::array<std::array<double, 6>, 3> warp_rectilinear;
    std::array<double, 5> vignette_radial;

    void initCorrections(int width, int height, const procparams::CoarseTransformParams &coarse, int rawRotationDeg) override
    {
        if (rawRotationDeg >= 0) {
            int rot = (coarse.rotate + rawRotationDeg) % 360;
            swap_xy = (rot == 90 || rot == 270);
            if (swap_xy) {
                std::swap(width, height);
            }
        }

        this->width = width;
        this->height = height;

        setup();
    }

    void parse()
    {
        std::set<int> processed_opcodes;

        has_dist = has_ca = has_vign = false;

        auto &exif = metadata.exifData();

        auto it = exif.findKey(Exiv2::ExifKey("Exif.SubImage1.OpcodeList3"));

        if (it != exif.end()) {
            std::vector<Exiv2::byte> buf;
            buf.resize(it->value().size());
            it->value().copy(buf.data(), Exiv2::invalidByteOrder);

            const Exiv2::byte *data = buf.data();
            uint32_t num_entries = Exiv2::getULong(data, Exiv2::bigEndian);
            size_t idx = 4;

            for (size_t i = 0; i < num_entries && idx < buf.size(); ++i) {
                uint32_t opcodeID = Exiv2::getULong(data + idx, Exiv2::bigEndian);
                idx += 4;
                idx += 4; // version
                uint32_t flags = Exiv2::getULong(data + idx, Exiv2::bigEndian);
                idx += 4;
                size_t paramSize = Exiv2::getULong(data + idx, Exiv2::bigEndian);
                idx += 4;

                if (idx + paramSize > buf.size()) {
                    throw std::runtime_error("error parsing DNG OpcodeList3");
                }

                if (processed_opcodes.find(opcodeID) != processed_opcodes.end()) {
                    // we currently handle only one opcode per type and ignore next ones if provided.
                    if (settings->verbose) {
                        std::printf("DNG OpcodeList3 %s opcode %d already processed\n", flags & 1 ? "optional" : "mandatory", opcodeID);
                    }

                    idx += paramSize;
                    continue;
                }

                processed_opcodes.insert(opcodeID);

                // we currently handle only one dist correction
                if (opcodeID == 1 && !has_dist) { // WarpRectilinear

                    planes = Exiv2::getULong(data + idx, Exiv2::bigEndian);

                    if ((planes != 1) && (planes != 3)) {
                        throw std::runtime_error("cannot parse DNG WarpRectilinear");
                    }

                    for (int p = 0; p < planes; p++) {
                        for (int i = 0; i < 6; i++) {
                            warp_rectilinear[p][i] = Exiv2::getDouble(data + idx + 4 + 8 * (i + p * 6), Exiv2::bigEndian);
                        }
                    }

                    crx_d = Exiv2::getDouble(data + idx + 4 + 8 * (0 + planes * 6), Exiv2::bigEndian);
                    cry_d = Exiv2::getDouble(data + idx + 4 + 8 * (1 + planes * 6), Exiv2::bigEndian);

                    has_dist = true;
                    if (planes == 3) {
                        has_ca = true;
                    }

                    // we currently handle only one vignetting correction
                } else if (opcodeID == 3 && !has_vign) { // FixVignetteRadial
                    size_t start = idx;
                    size_t end = idx + 7 * 8;
                    if (end > buf.size()) {
                        throw std::runtime_error("cannot parse DNG FixVignetteRadial");
                    }
                    for (int j = 0; j < 5; j++) {
                        vignette_radial[j] = Exiv2::getDouble(data + start, Exiv2::bigEndian);
                        start += 8;
                    }
                    crx_v = Exiv2::getDouble(data + start, Exiv2::bigEndian);
                    start += 8;
                    cry_v = Exiv2::getDouble(data + start, Exiv2::bigEndian);
                    has_vign = true;

                } else {
                    if (settings->verbose) {
                        std::printf("DNG OpcodeList3 has unsupported %s opcode %d\n", flags & 1 ? "optional" : "mandatory", opcodeID);
                    }
                }

                idx += paramSize;
            }
        }

        if (!has_dist && !has_vign) {
            throw std::runtime_error("no known DNG correction data");
        }
    }

    void setup()
    {
        cx_d = crx_d * width;
        cy_d = cry_d * height;
        cx_v = crx_v * width;
        cy_v = cry_v * height;
        double mx_d = std::max(cx_d, width - cx_d);
        double my_d = std::max(cy_d, height - cy_d);
        m_d = std::sqrt(SQR(mx_d) + SQR(my_d));
        double mx_v = std::max(cx_v, width - cx_v);
        double my_v = std::max(cy_v, height - cy_v);
        m_v = std::sqrt(SQR(mx_v) + SQR(my_v));
    }

    void correctPlaneDistortion(double &x, double &y, int cx, int cy, int plane) const
    {
        if (plane < 0 || plane > 2 || plane > planes) {
            return;
        }

        double xx = x + cx;
        double yy = y + cy;
        if (swap_xy) {
            std::swap(xx, yy);
        }

        const double cx1 = cx_d;
        const double cy1 = cy_d;
        const double m = m_d;

        const double dx = (xx - cx1) / m;
        const double dy = (yy - cy1) / m;
        const double dx2 = SQR(dx);
        const double dy2 = SQR(dy);
        const double r2 = dx2 + dy2;
        const double f = warp_rectilinear[plane][0] + r2 * (warp_rectilinear[plane][1] + r2 * (warp_rectilinear[plane][2] + r2 * warp_rectilinear[plane][3]));
        const double dx_r = f * dx;
        const double dy_r = f * dy;
        const double dxdy2 = 2 * dx * dy;
        const double dx_t = warp_rectilinear[plane][4] * dxdy2 + warp_rectilinear[plane][5] * (r2 + 2 * dx2);
        const double dy_t = warp_rectilinear[plane][5] * dxdy2 + warp_rectilinear[plane][4] * (r2 + 2 * dy2);

        x = cx1 + m * (dx_r + dx_t);
        y = cy1 + m * (dy_r + dy_t);

        if (swap_xy) {
            std::swap(x, y);
        }
        x -= cx;
        y -= cy;
    }

    void correctDistortionAndCA(double &x, double &y, int cx, int cy, int channel) const override
    {
        if (!hasDistortionCorrection() || !hasCACorrection()) {
            return;
        }

        correctPlaneDistortion(x, y, cx, cy, channel);
    }

    void correctDistortion(double &x, double &y, int cx, int cy) const override
    {
        if (!hasDistortionCorrection()) {
            return;
        }

        int plane = 1; // 3 planes correction, use plane 1 (green)
        if (planes == 1) {
            plane = 0; // 1 single plane correction
        }

        correctPlaneDistortion(x, y, cx, cy, plane);
    }

    void correctCA(double &x, double &y, int cx, int cy, int channel) const override
    {
        if (!hasCACorrection()) {
            return;
        }

        // we use plane 0 (red) and plane 2 (blue) for ca correction
        if (channel != 0 && channel != 2) return;
        if (planes != 3) return;

        double xgreen = x, ygreen = y;
        correctPlaneDistortion(xgreen, ygreen, cx, cy, 1);

        double xch = x, ych = y;
        correctPlaneDistortion(xch, ych, cx, cy, channel);

        // Calculate diff from green plane
        x += xch - xgreen;
        y += ych - ygreen;
    }

    void processVignetteNChannels(int width, int height, float **rawData, int channels) const
    {
        if (!hasVignettingCorrection()) {
            return;
        }

        const double cx = cx_v;
        const double cy = cy_v;
        const double m2 = 1.f / SQR(m_v);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const double r2 = m2 * (SQR(x - cx) + SQR(y - cy));
                const double g = 1.f + r2 * (vignette_radial[0] + r2 * (vignette_radial[1] + r2 * (vignette_radial[2] + r2 * (vignette_radial[3] + r2 * vignette_radial[4]))));
                for (int c = 0; c < channels; ++c) {
                    rawData[y][x*channels + c] *= g;
                }
            }
        }
    }

    void processVignette(int width, int height, float **rawData) const override
    {
        return processVignetteNChannels(width, height, rawData, 1);
    }

    void processVignette3Channels(int width, int height, float **rawData) const override
    {
        return processVignetteNChannels(width, height, rawData, 3);
    }

    bool isCACorrectionAvailable() const
    {
        return hasCACorrection();
    }

    bool hasDistortionCorrection() const override { return has_dist; }
    bool hasVignettingCorrection() const override { return has_vign; }
    bool hasCACorrection() const override { return has_ca; }
};

std::unique_ptr<MetadataLensCorrection> MetadataLensCorrectionFinder::findCorrection(const FramesMetaData *meta)
{
    static const std::unordered_set<std::string> makers = {
        "SONY",
        "FUJIFILM",
        "PANASONIC",
    };

    std::string make = Glib::ustring(meta->getMake()).uppercase();
    const bool is_olympus = isOlympusMake(make);

    if (!meta->getDNG() && !is_olympus && makers.find(make) == makers.end()) {
        return nullptr;
    }

    std::unique_ptr<MetadataLensCorrection> correction;

    try {
        if (meta->getDNG()) {
            correction.reset(new DNGMetadataLensCorrection(meta));
        } else if (make == "SONY") {
            correction.reset(new SonyMetadataLensCorrection(meta));
        } else if (make == "FUJIFILM") {
            correction.reset(new FujiMetadataLensCorrection(meta));
        } else if (is_olympus) {
            correction.reset(new OlympusMetadataLensCorrection(meta));
        } else if (make == "PANASONIC") {
            correction.reset(new PanasonicMetadataLensCorrection(meta));
        }
    } catch (std::exception &exc) {
        if (settings->verbose) {
            std::cerr << "error parsing lens metadata: " << exc.what() << std::endl;
        }

        correction.reset(nullptr);
    }

    return correction;
}

} // namespace rtengine
