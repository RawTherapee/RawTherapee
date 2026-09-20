/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2016-2024 Jacques Desmis <jdesmis@gmail.com>
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

// Locallab Denoise Tool - extracted from iplocallab.cc

#include <cstdlib>

#include "improcfun.h"
#include "labimage.h"
#include "rt_math.h"
#include "sleef.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace rtengine {

void ImProcFunctions::addGaNoise(LabImage *lab, LabImage *dst, const float mean, const float variance, const int sk)
{
//   BENCHFUN
//Box-Muller method.
// add luma noise to image

    srand(1);

    const float variaFactor = SQR(variance) / sk;
    constexpr float randFactor1 = 1.f / RAND_MAX;
    constexpr float randFactor2 = (2.f * rtengine::RT_PI_F) / RAND_MAX;
#ifdef _OPENMP
    #pragma omp parallel if (multiThread)
#endif
    {
        float z0, z1;
        bool generate = false;
#ifdef _OPENMP
        #pragma omp for schedule(static) // static scheduling is important to avoid artefacts
#endif

        for (int y = 0; y < lab->H; y++) {
            for (int x = 0; x < lab->W; x++) {
                generate = !generate;
                float kvar = 1.f;

                if (lab->L[y][x] < 12000.f) {
                    constexpr float ah = -0.5f / 12000.f;
                    constexpr float bh = 1.5f;
                    kvar = ah * lab->L[y][x] + bh;    //increase effect for low lights < 12000.f
                } else if (lab->L[y][x] > 20000.f) {
                    constexpr float ah = -0.5f / 12768.f;
                    constexpr float bh = 1.f - 20000.f * ah;
                    kvar = ah * lab->L[y][x] + bh;    //decrease effect for high lights > 20000.f
                    kvar = kvar < 0.5f ? 0.5f : kvar;
                }

                float varia = SQR(kvar) * variaFactor;

                if (!generate) {
                    dst->L[y][x] = LIM(lab->L[y][x] + mean + varia * z1, 0.f, 32768.f);
                    continue;
                }

                int u1 = 0;
                int u2;

                while (u1 == 0) {
                    u1 = rand();
                    u2 = rand();
                }

                float u1f = u1 * randFactor1;
                float u2f = u2 * randFactor2;

                float2 sincosval = xsincosf(2.f * rtengine::RT_PI_F * u2f);
                float factor = std::sqrt(-2.f * xlogf(u1f));
                z0 = factor * sincosval.y;
                z1 = factor * sincosval.x;

                dst->L[y][x] = LIM(lab->L[y][x] + mean + varia * z0, 0.f, 32768.f);

            }
        }
    }
}

} // namespace rtengine
