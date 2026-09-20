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

// Locallab Exposure Tool - extracted from iplocallab.cc

#include <cmath>
#include <memory>
#include <fftw3.h>

#include "improcfun.h"
#include "labimage.h"
#include "rt_math.h"
#include "curves.h"
#include "iplocallab.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace rtengine {

extern MyMutex *fftwMutex;

namespace {
constexpr float MAXSCOPE = 1.25f;
} // anonymous namespace

// This function is declared in iplocallab.h but only used by exlabLocal
void deltaEforLaplace(float *dE, float lap, int bfw, int bfh, LabImage* bufexporig,
                      float hueref, float chromaref, float lumaref)
{
    const float refa = chromaref * std::cos(hueref);
    const float refb = chromaref * std::sin(hueref);
    const float refL = lumaref;
    float maxdE = 5.f + MAXSCOPE * lap;

    float maxC = std::sqrt((SQR(refa - bufexporig->a[0][0]) + SQR(refb - bufexporig->b[0][0])) + SQR(refL - bufexporig->L[0][0])) / 327.68f;
#ifdef _OPENMP
    #pragma omp parallel for reduction(max:maxC)
#endif

    for (int y = 0; y < bfh; y++) {
        for (int x = 0; x < bfw; x++) {
            const float val = std::sqrt((SQR(refa - bufexporig->a[y][x]) + SQR(refb - bufexporig->b[y][x])) + SQR(refL - bufexporig->L[y][x])) / 327.68f;
            dE[y * bfw + x] = val;
            maxC = max(maxC, val);
        }
    }

    if (maxdE > maxC) {
        maxdE = maxC - 1.f;
    }

    const float ade = 1.f / (maxdE - maxC);

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16)
#endif

    for (int y = 0; y < bfh; y++) {
        for (int x = 0; x < bfw; x++) {
            dE[y * bfw + x] = dE[y * bfw + x] >= maxdE ? ade * (dE[y * bfw + x] - maxC) : 1.f;
        }
    }
}

void ImProcFunctions::exlabLocal(local_params& lp, float strlap, int bfh, int bfw, int bfhr, int bfwr, LabImage* bufexporig, LabImage* lab, const LUTf& hltonecurve, const LUTf& shtonecurve, const LUTf& tonecurve, const float hueref, const float lumaref, const float chromaref)
{
    //BENCHFUN
    //exposure local

    constexpr float maxran = 65536.f;

    if (lp.laplacexp == 0.f) {
        lp.linear = 0.f;
    }

    const float linear = lp.linear;
    int bw = bfw;
    int bh = bfh;

    if (linear > 0.f && lp.expcomp == 0.f) {
        lp.expcomp = 0.001f;
    }

    const bool exec = (lp.expmet == 1 && linear > 0.f && lp.laplacexp > 0.1f);

    if (!exec) { //for standard exposure

        const float cexp_scale = std::pow(2.f, lp.expcomp);
        const float ccomp = (rtengine::max(0.f, lp.expcomp) + 1.f) * lp.hlcomp / 100.f;
        const float cshoulder = ((maxran / rtengine::max(1.0f, cexp_scale)) * (lp.hlcompthr / 200.f)) + 0.1f;
        const float chlrange = maxran - cshoulder;
        const float diffde = 100.f - lp.sensex;//the more scope, the less take into account dE for Laplace

        if (!lp.invex) { // Laplacian not in inverse
            bw = bfwr;
            bh = bfhr;

            //Laplacian PDE before exposure to smooth L, algorithm exposure leads to increase L differences
            const std::unique_ptr<float[]> datain(new float[bfwr * bfhr]);
            const std::unique_ptr<float[]> dataout(new float[bfwr * bfhr]);
            const std::unique_ptr<float[]> dE(new float[bfwr * bfhr]);

            deltaEforLaplace(dE.get(), diffde, bfwr, bfhr, bufexporig, hueref, chromaref, lumaref);

            float alap = strlap * 600.f;
            float blap = strlap * 100.f;
            float aa = (alap - blap) / 50.f;
            float bb = blap - 30.f * aa;

            float lap;

            if (diffde > 80.f) {
                lap = alap;
            } else if (diffde < 30.f) {
                lap = blap;
            } else {
                lap = aa * diffde + bb;
            }

#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

            for (int y = 0; y < bfhr; y++) {
                for (int x = 0; x < bfwr; x++) {
                    datain[y * bfwr + x] = bufexporig->L[y][x];
                }
            }

            MyMutex::MyLock lock(*fftwMutex);
            ImProcFunctions::retinex_pde(datain.get(), dataout.get(), bfwr, bfhr, lap, 1.f, dE.get(), 0, 1, 1);//350 arbitrary value about 45% strength Laplacian
#ifdef _OPENMP
            #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

            for (int y = 0; y < bfhr; y++) {
                for (int x = 0; x < bfwr; x++) {
                    bufexporig->L[y][x] = dataout[y * bfwr + x];
                }
            }

        }

#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif

        for (int ir = 0; ir < bh; ir++) {//for standard with Laplacian in normal and without in inverse
            for (int jr = 0; jr < bw; jr++) {
                float L = bufexporig->L[ir][jr];
                //highlight
                const float hlfactor = (2 * L < MAXVALF ? hltonecurve[2 * L] : CurveFactory::hlcurve(cexp_scale, ccomp, chlrange, 2 * L));
                L *= hlfactor;//approximation but pretty good with Laplacian and L < mean, hl aren't call
                //shadow tone curve
                L *= shtonecurve[2 * L];
                //tonecurve
                lab->L[ir][jr] = 0.5f * tonecurve[2 * L];
            }
        }
    } else if (!lp.invex) { //for PDE algorithms
        constexpr float kl = 1.f;
        const float hlcompthr = lp.hlcompthr / 200.f;
        const float hlcomp = lp.hlcomp / 100.f;

#ifdef _OPENMP
        #pragma omp parallel for if (multiThread)
#endif

        for (int ir = 0; ir < bfh; ir++) {
            for (int jr = 0; jr < bfw; jr++) {
                float L = bufexporig->L[ir][jr];
                const float Llin = LIM01(L / 32768.f);
                const float addcomp = linear * (-kl * Llin + kl);//maximum about 1 . IL
                const float exp_scale = pow_F(2.f, lp.expcomp + addcomp);
                const float shoulder = (maxran / rtengine::max(1.0f, exp_scale)) * hlcompthr + 0.1f;
                const float comp = (rtengine::max(0.f, (lp.expcomp + addcomp)) + 1.f) * hlcomp;
                const float hlrange = maxran - shoulder;

                //highlight
                const float hlfactor = (2 * L < MAXVALF ? hltonecurve[2 * L] : CurveFactory::hlcurve(exp_scale, comp, hlrange, 2 * L));
                L *= hlfactor * pow_F(2.f, addcomp);//approximation but pretty good with Laplacian and L < mean, hl aren't call
                //shadow tone curve
                L *= shtonecurve[2 * L];
                //tonecurve
                lab->L[ir][jr] = 0.5f * tonecurve[2 * L];
            }
        }
    }
}

void ImProcFunctions::exposure_pde(float * dataor, float * datain, float * dataout, int bfw, int bfh, float thresh, float mod)
/* Jacques Desmis July 2019
** adapted from Ipol Copyright 2009-2011 IPOL Image Processing On Line http://www.ipol.im/
*/
{

    //BENCHFUN
#ifdef RT_FFTW3F_OMP
    if (multiThread) {
        fftwf_init_threads();
        fftwf_plan_with_nthreads(omp_get_max_threads());
    }

#endif
    float *data_fft, *data_tmp, *data;

    if (NULL == (data_tmp = (float *) fftwf_malloc(sizeof(float) * bfw * bfh))) {
        fprintf(stderr, "allocation error\n");
        abort();
    }

    ImProcFunctions::discrete_laplacian_threshold(data_tmp, datain, bfw, bfh, thresh);

    if (NULL == (data_fft = (float *) fftwf_malloc(sizeof(float) * bfw * bfh))) {
        fprintf(stderr, "allocation error\n");
        abort();
    }

    if (NULL == (data = (float *) fftwf_malloc(sizeof(float) * bfw * bfh))) {
        fprintf(stderr, "allocation error\n");
        abort();
    }

    const auto dct_fw = fftwf_plan_r2r_2d(bfh, bfw, data_tmp, data_fft, FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);
    fftwf_execute(dct_fw);

    fftwf_free(data_tmp);

    /* solve the Poisson PDE in Fourier space */
    /* 1. / (float) (bfw * bfh)) is the DCT normalisation term, see libfftw */
    ImProcFunctions::rex_poisson_dct(data_fft, bfw, bfh, 1. / (double)(bfw * bfh));

    const auto dct_bw = fftwf_plan_r2r_2d(bfh, bfw, data_fft, data, FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);
    fftwf_execute(dct_bw);
    fftwf_destroy_plan(dct_fw);
    fftwf_destroy_plan(dct_bw);
    fftwf_free(data_fft);
    fftwf_cleanup();

#ifdef RT_FFTW3F_OMP

    if (multiThread) {
        fftwf_cleanup_threads();
    }

#endif

    normalize_mean_dt(data, dataor, bfw * bfh, mod, 1.f, 0.f, 0.f, 0.f, 0.f, 1.);
    {

#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int y = 0; y < bfh ; y++) {
            for (int x = 0; x < bfw; x++) {
                dataout[y * bfw + x] = locallab::clipLoc(data[y * bfw + x]);
            }
        }
    }

    fftwf_free(data);
}

} // namespace rtengine
