/*
 *  This file is part of RawTherapee.
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

#include <cmath>
#include <iostream>
#include <memory>

#include "rtengine/color.h"
#include "rtengine/image8.h"
#include "rtengine/improcfun.h"
#include "rtengine/labimage.h"
#include "rtengine/procparams.h"
#include "rtengine/settings.h"

namespace
{

constexpr float labScale = 327.68f;
// In-gamut colors, including a neutral patch. Five pixels exercise both the
// SIMD loop and scalar tail in lab2rgb().
constexpr float colors[][3] = {
    {50.f, 0.f, 0.f},
    {55.f, 25.f, 20.f},
    {65.f, -20.f, 25.f},
    {40.f, 20.f, -25.f},
    {70.f, -10.f, -10.f}
};
constexpr int width = sizeof(colors) / sizeof(colors[0]);

bool checkColor(int pixel, float L, float a, float b)
{
    const float dL = L / labScale - colors[pixel][0];
    const float da = a / labScale - colors[pixel][1];
    const float db = b / labScale - colors[pixel][2];
    const float deltaE = std::sqrt(dL * dL + da * da + db * db);
    // Allow for the intermediate 8-bit RGB quantization.
    if (!std::isfinite(deltaE) || deltaE > 1.5f) {
        std::cerr << "Pixel " << pixel << ": round-trip deltaE76 = " << deltaE << '\n';
        return false;
    }
    return true;
}

bool checkRoundTrip(rtengine::ImProcFunctions& functions, const rtengine::procparams::ColorManagementParams& icm,
                    rtengine::LabImage& input, bool considerHistogramSettings, bool linear)
{
    std::unique_ptr<rtengine::Image8> rgb(functions.lab2rgb(&input, 0, 0, width, 1, icm, considerHistogramSettings));
    // L*=50 corresponds to Y=0.184: about 47 in linear 8-bit RGB, or 119
    // with sRGB gamma. Keep the producer's intended encoding covered too.
    const int neutral = rgb->r(0, 0);
    const int expectedNeutral = linear ? 47 : 119;
    bool ok = std::abs(neutral - expectedNeutral) <= 1;
    if (!ok) {
        std::cerr << "Unexpected neutral RGB encoding: " << neutral << '\n';
    }

    float L[width], a[width], b[width];
    functions.rgb2lab(*rgb, 0, 0, width, 1, L, a, b, icm, considerHistogramSettings);
    for (int i = 0; i < width; ++i) {
        ok = checkColor(i, L[i], a[i], b[i]) && ok;
        // Also cover the single-pixel API used by the scope pointer/pickers.
        float pixelL, pixelA, pixelB;
        rtengine::ImProcFunctions::rgb2lab(rgb->r(0, i), rgb->g(0, i), rgb->b(0, i),
                                          pixelL, pixelA, pixelB, icm, considerHistogramSettings);
        ok = checkColor(i, pixelL, pixelA, pixelB) && ok;
    }
    return ok;
}

}

int main()
{
    rtengine::Settings settings{};
    rtengine::settings = &settings;
    rtengine::Color::init();

    rtengine::LabImage input(width, 1, false, false);
    for (int i = 0; i < width; ++i) {
        input.L[0][i] = colors[i][0] * labScale;
        input.a[0][i] = colors[i][1] * labScale;
        input.b[0][i] = colors[i][2] * labScale;
    }

    rtengine::ImProcFunctions functions(nullptr, false);
    rtengine::procparams::ColorManagementParams icm;
    icm.outputProfile = rtengine::procparams::ColorManagementParams::NoICMString;
    bool ok = true;
    for (const auto profile : {"sRGB", "ProPhoto", "Adobe RGB"}) {
        icm.workingProfile = profile;
        for (const bool working : {false, true}) {
            settings.HistogramWorking = working;
            for (const bool consider : {false, true}) {
                if (!checkRoundTrip(functions, icm, input, consider, working && consider)) {
                    std::cerr << "Profile: " << profile << ", HistogramWorking: " << working
                              << ", consider_histogram_settings: " << consider << '\n';
                    ok = false;
                }
            }
        }
    }

    rtengine::Color::cleanup();
    rtengine::settings = nullptr;
    return ok ? 0 : 1;
}
