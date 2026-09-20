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

// Soft Light tool implementation for locallab
// This file is included from iplocallab.cc within Lab_Local()

// soft light and retinex_pde
if ((lp.strng > 1.f || lp.prevdE) && call <= 3 && lp.sfena) {
    int ystart = rtengine::max(static_cast<int>(lp.yc - lp.lyT) - cy, 0);
    int yend = rtengine::min(static_cast<int>(lp.yc + lp.ly) - cy, original->H);
    int xstart = rtengine::max(static_cast<int>(lp.xc - lp.lxL) - cx, 0);
    int xend = rtengine::min(static_cast<int>(lp.xc + lp.lx) - cx, original->W);
    int bfh = yend - ystart;
    int bfw = xend - xstart;
    //variable for fast FFTW
    int bfhr = bfh;
    int bfwr = bfw;

    if (bfw >= mSP && bfh >= mSP) {

        if (lp.softmet == 1) {
            optfft(N_fftwsize, bfh, bfw, bfhr, bfwr, lp, original->H, original->W, xstart, ystart, xend, yend, cx, cy, lp.fullim);
        }

        const std::unique_ptr<LabImage> bufexporig(new LabImage(bfw, bfh));
        const std::unique_ptr<LabImage> bufexpfin(new LabImage(bfw, bfh));

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

        for (int y = ystart; y < yend; y++) {
            for (int x = xstart; x < xend; x++) {
                bufexporig->L[y - ystart][x - xstart] = original->L[y][x];
                bufexporig->a[y - ystart][x - xstart] = original->a[y][x];
                bufexporig->b[y - ystart][x - xstart] = original->b[y][x];
            }
        }

        bufexpfin->CopyFrom(bufexporig.get(), multiThread);
        SoftLightParams softLightParams;
        softLightParams.enabled = true;
        softLightParams.strength = lp.strng;

        if (lp.softmet == 0) {
            ImProcFunctions::softLight(bufexpfin.get(), softLightParams);
        } else if (lp.softmet == 1) {
            const std::unique_ptr<float[]> datain(new float[bfwr * bfhr]);
            const std::unique_ptr<float[]> dataout(new float[bfwr * bfhr]);
            const std::unique_ptr<float[]> dE(new float[bfwr * bfhr]);

            deltaEforLaplace(dE.get(), lp.lap, bfwr, bfhr, bufexpfin.get(), hueref, chromaref, lumaref);

#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

            for (int y = 0; y < bfhr; y++) {
                for (int x = 0; x < bfwr; x++) {
                    datain[y * bfwr + x] = bufexpfin->L[y][x];
                }
            }

            const int showorig = lp.showmasksoftmet >= 5 ? 0 : lp.showmasksoftmet;
            MyMutex::MyLock lock(*fftwMutex);
            ImProcFunctions::retinex_pde(datain.get(), dataout.get(), bfwr, bfhr, 8.f * lp.strng, 1.f, dE.get(), showorig, 1, 1);
#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

            for (int y = 0; y < bfhr; y++) {
                for (int x = 0; x < bfwr; x++) {
                    bufexpfin->L[y][x] = dataout[y * bfwr + x];
                }
            }
        }

        transit_shapedetect2(sp, 0.f, 0.f, call, 3, bufexporig.get(), bufexpfin.get(), nullptr, hueref, chromaref, lumaref, sobelref, 0.f, nullptr, lp, original, transformed, nullptr, LocalLabGradientMode::STANDARD, cx, cy, sk);

        if (lp.recur) {
            original->CopyFrom(transformed, multiThread);
            float avge;
            calc_ref(sp, original, transformed, 0, 0, original->W, original->H, sk, huerefblur, chromarefblur, lumarefblur, hueref, chromaref, lumaref, sobelref, avge, locwavCurveden, locwavdenutili);
        }
    }
}
