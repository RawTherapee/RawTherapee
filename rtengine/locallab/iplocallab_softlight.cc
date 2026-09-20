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

// Locallab Soft Light Tool - extracted from iplocallab.cc

#include "improcfun.h"
#include "labimage.h"
#include "rt_math.h"
#include "color.h"
#include "guidedfilter.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace rtengine {

void ImProcFunctions::softproc(const LabImage* bufcolorig, const LabImage* bufcolfin, float rad, int bfh, int bfw, float epsilmax, float epsilmin, float thres, int sk, bool multiThread, int flag)
{
    if (rad != 0.f) {
        array2D<float> ble(bfw, bfh);
        array2D<float> guid(bfw, bfh);

        if (flag == 0) {

#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int ir = 0; ir < bfh; ir++) {
                for (int jr = 0; jr < bfw; jr++) {
                    guid[ir][jr] = Color::L2Y(bufcolorig->L[ir][jr]) / 32768.f;
                    ble[ir][jr] = Color::L2Y(bufcolfin->L[ir][jr]) / 32768.f;
                }
            }

            const float aepsil = (epsilmax - epsilmin) / 100.f;
            const float bepsil = epsilmin; //epsilmax - 100.f * aepsil;
            // const float epsil = aepsil * 0.1f * rad + bepsil;
            const float epsil = aepsil * rad + bepsil;
            const float blur = 10.f / sk * (thres + 0.f * rad);

            rtengine::guidedFilter(guid, ble, ble, blur, epsil, multiThread, 4);

#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int ir = 0; ir < bfh; ir++) {
                for (int jr = 0; jr < bfw; jr++) {
                    bufcolfin->L[ir][jr] =  Color::computeXYZ2LabY(32768.f * ble[ir][jr]);
                }
            }
        } else if (flag == 1) {

#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int ir = 0; ir < bfh; ir++)
                for (int jr = 0; jr < bfw; jr++) {
                    ble[ir][jr] = bufcolfin->L[ir][jr] / 32768.f;
                    guid[ir][jr] = bufcolorig->L[ir][jr] / 32768.f;
                }

            const float aepsil = (epsilmax - epsilmin) / 1000.f;
            const float bepsil = epsilmin; //epsilmax - 100.f * aepsil;
            const float epsil = rad < 0.f ? 0.0001f : aepsil * rad + bepsil;
            const float blur = rad < 0.f ? -1.f / rad : 1.f + rad;
            const int r2 = rtengine::max(int(25 / sk * blur + 0.5f), 1);

            rtengine::guidedFilter(guid, ble, ble, r2, epsil, multiThread);

#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int ir = 0; ir < bfh; ir++) {
                for (int jr = 0; jr < bfw; jr++) {
                    bufcolfin->L[ir][jr] =  32768.f * ble[ir][jr];
                }
            }
        } else if (flag == 2) {

#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int ir = 0; ir < bfh; ir++)
                for (int jr = 0; jr < bfw; jr++) {
                    ble[ir][jr] = bufcolfin->L[ir][jr] / 32768.f;
                    guid[ir][jr] = bufcolorig->L[ir][jr] / 32768.f;
                }

            const float aepsil = (epsilmax - epsilmin) / 1000.f;
            const float bepsil = epsilmin; //epsilmax - 100.f * aepsil;
            const float epsil = rad < 0.f ? 0.0001f : aepsil * 10.f * rad + bepsil;
            //  const float epsil =  bepsil;
            const float blur = rad < 0.f ? -1.f / rad : 0.00001f + rad;
            const int r2 = rtengine::max(int(20.f / sk * blur + 0.000001f), 1);

            rtengine::guidedFilter(guid, ble, ble, r2, epsil, multiThread);

#ifdef _OPENMP
            #pragma omp parallel for if(multiThread)
#endif

            for (int ir = 0; ir < bfh; ir++) {
                for (int jr = 0; jr < bfw; jr++) {
                    bufcolfin->L[ir][jr] =  32768.f * ble[ir][jr];
                }
            }
        }
    }
}


void ImProcFunctions::softprocess(const LabImage* bufcolorig, array2D<float> &buflight, float rad, int bfh, int bfw, double epsilmax, double epsilmin,  float thres, int sk, bool multiThread)
{
    float minlig = buflight[0][0];

#ifdef _OPENMP
    #pragma omp parallel for reduction(min:minlig) schedule(dynamic,16) if (multiThread)
#endif

    for (int ir = 0; ir < bfh; ir++) {
        for (int jr = 0; jr < bfw; jr++) {
            minlig = rtengine::min(buflight[ir][jr], minlig);
        }
    }

    array2D<float> guidsoft(bfw, bfh);

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

    for (int ir = 0; ir < bfh; ir++) {
        for (int jr = 0; jr < bfw; jr++) {
            buflight[ir][jr] = LIM01((buflight[ir][jr] - minlig) / (100.f - minlig));
            guidsoft[ir][jr] = bufcolorig->L[ir][jr] / 32768.f;
        }
    }

    double aepsil = (epsilmax - epsilmin) / 90.0;
    double bepsil = epsilmax - 100.0 * aepsil;
    double epsil = aepsil * static_cast<double>(rad) + bepsil;
    float blur = 1.f / sk * (thres + 0.8f * rad);
    guidedFilter(guidsoft, buflight, buflight, blur, epsil,  multiThread, 4);


#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

    for (int ir = 0; ir < bfh; ir++) {
        for (int jr = 0; jr < bfw; jr++) {
            buflight[ir][jr] = (100.f - minlig) * buflight[ir][jr] + minlig;
        }
    }
}

} // namespace rtengine
