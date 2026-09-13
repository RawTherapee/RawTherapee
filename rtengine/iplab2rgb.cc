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
#include <glibmm/ustring.h>

#include "alignedbuffer.h"
#include "color.h"
#include "iccmatrices.h"
#include "iccstore.h"
#include "image8.h"
#include "imagefloat.h"
#include "improcfun.h"
#include "labimage.h"
#include "procparams.h"
#include "rtengine.h"
#include "settings.h"
#include "utils.h"
#include <fmt/format.h>
#include "rtgui/labgrid.h"

namespace rtengine
{

namespace
{

inline void copyAndClampLine(const float *src, unsigned char *dst, const int W)
{
    for (int j = 0; j < W * 3; ++j) {
        dst[j] = uint16ToUint8Rounded(CLIP(src[j] * MAXVALF));
    }
}


inline void copyAndClamp(const LabImage *src, unsigned char *dst, const double rgb_xyz[3][3], bool multiThread, int pro)//int pro to switch to Working Profile histogram mode without gamma
{
    const int W = src->W;
    const int H = src->H;

    float rgb_xyzf[3][3];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            rgb_xyzf[i][j] = rgb_xyz[i][j];
        }
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat rgb_xyzv[3][3];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            rgb_xyzv[i][j] = F2V(rgb_xyzf[i][j]);
        }
    }

#endif
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

    for (int i = 0; i < H; ++i) {
        float* rL = src->L[i];
        float* ra = src->a[i];
        float* rb = src->b[i];
        int ix = i * 3 * W;

#if defined(__SSE2__) || defined(RT_SIMDE)
        float rbuffer[W] ALIGNED16;
        float gbuffer[W] ALIGNED16;
        float bbuffer[W] ALIGNED16;
        int j = 0;

        for (; j < W - 3; j += 4) {
            vfloat R, G, B;
            vfloat x_, y_, z_;
            Color::Lab2XYZ(LVFU(rL[j]), LVFU(ra[j]), LVFU(rb[j]), x_, y_, z_);
            Color::xyz2rgb(x_, y_, z_, R, G, B, rgb_xyzv);
            if(pro == 0) {
                STVF(rbuffer[j], Color::gamma2curve[R]);
                STVF(gbuffer[j], Color::gamma2curve[G]);
                STVF(bbuffer[j], Color::gamma2curve[B]);
            } else {//Working profile and gamma=1
                STVF(rbuffer[j], R);
                STVF(gbuffer[j], G);
                STVF(bbuffer[j], B);
            }
        }

        for (; j < W; ++j) {
            float R, G, B;
            float x_, y_, z_;
            Color::Lab2XYZ(rL[j], ra[j], rb[j], x_, y_, z_);
            Color::xyz2rgb(x_, y_, z_, R, G, B, rgb_xyzf);
            if(pro == 0) {
                rbuffer[j] = Color::gamma2curve[R];
                gbuffer[j] = Color::gamma2curve[G];
                bbuffer[j] = Color::gamma2curve[B];
            } else {//Working profile and gamma=1
                rbuffer[j] = R;
                gbuffer[j] = G;
                bbuffer[j] = B;              
            }
        }

        for (j = 0; j < W; ++j) {
            dst[ix++] = uint16ToUint8Rounded(rbuffer[j]);
            dst[ix++] = uint16ToUint8Rounded(gbuffer[j]);
            dst[ix++] = uint16ToUint8Rounded(bbuffer[j]);
        }

#else

        for (int j = 0; j < W; ++j) {
            float R, G, B;
            float x_, y_, z_;
            Color::Lab2XYZ(rL[j], ra[j], rb[j], x_, y_, z_);
            Color::xyz2rgb(x_, y_, z_, R, G, B, rgb_xyzf);
            if(pro == 0) {
                dst[ix++] = uint16ToUint8Rounded(Color::gamma2curve[R]);
                dst[ix++] = uint16ToUint8Rounded(Color::gamma2curve[G]);
                dst[ix++] = uint16ToUint8Rounded(Color::gamma2curve[B]);
            } else {//Working profile and gamma=1
                dst[ix++] = uint16ToUint8Rounded(R);
                dst[ix++] = uint16ToUint8Rounded(G);
                dst[ix++] = uint16ToUint8Rounded(B);               
            }
        }

#endif
    }
}

} // namespace


float gammalog(float x, float p, float s, float g3, float g4)
{
    return x <= g3 ? x * s : (1.f + g4) * xexpf(xlogf(x) / p) - g4;//continuous
}

#if defined(__SSE2__) || defined(RT_SIMDE)
vfloat gammalog(vfloat x, vfloat p, vfloat s, vfloat g3, vfloat g4)
{
    return vself(vmaskf_le(x, g3), x * s, (F2V(1.f) + g4) * xexpf(xlogf(x) / p) - g4);//continuous
}
#endif

// Used in ImProcCoordinator::updatePreviewImage  (rtengine/improccoordinator.cc)
//         Crop::update                           (rtengine/dcrop.cc)
//         Thumbnail::processImage                (rtengine/rtthumbnail.cc)
//
// If monitorTransform, divide by 327.68 then apply monitorTransform (which can integrate soft-proofing)
// otherwise divide by 327.68, convert to xyz and apply the sRGB transform, before converting with gamma2curve
void ImProcFunctions::lab2monitorRgb(LabImage* lab, Image8* image)
{
    if (monitorTransform) {

        const int W = lab->W;
        const int H = lab->H;
        unsigned char * data = image->data;

        // cmsDoTransform is relatively expensive
#ifdef _OPENMP
        #pragma omp parallel firstprivate(lab, data, W, H)
#endif
        {
            AlignedBuffer<float> pBuf(3 * lab->W);

            AlignedBuffer<float> mBuf;
            AlignedBuffer<float> gwBuf1;
            AlignedBuffer<float> gwBuf2;

            if (gamutWarning) {
                gwBuf1.resize(3 * lab->W);
                gwBuf2.resize(3 * lab->W);
                mBuf.resize(3 * lab->W);
            }

            float *buffer = pBuf.data;
            float *outbuffer = gamutWarning ? mBuf.data : pBuf.data; // make in place transformations when gamutWarning is not needed

#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16)
#endif

            for (int i = 0; i < H; i++) {

                const int ix = i * 3 * W;
                int iy = 0;

                float* rL = lab->L[i];
                float* ra = lab->a[i];
                float* rb = lab->b[i];

                for (int j = 0; j < W; j++) {
                    buffer[iy++] = rL[j] / 327.68f;
                    buffer[iy++] = ra[j] / 327.68f;
                    buffer[iy++] = rb[j] / 327.68f;
                }

                cmsDoTransform(monitorTransform, buffer, outbuffer, W);
                copyAndClampLine(outbuffer, data + ix, W);

                if (gamutWarning) {
                    gamutWarning->markLine(image, i, buffer, gwBuf1.data, gwBuf2.data);
                }
            }
        } // End of parallelization
    } else {
        copyAndClamp(lab, image->data, sRGB_xyz, multiThread, 0);//int pro = 0 always sent 'normal' to monitor 
    }
}



// Used in ImProcCoordinator::updatePreviewImage  (rtengine/improccoordinator.cc)
//         Crop::update                           (rtengine/dcrop.cc)
//
// Generate an Image8
//
// If output profile used, divide by 327.68 then apply the "profile" profile (eventually with a standard gamma)
// otherwise divide by 327.68, convert to xyz and apply the RGB transform, before converting with gamma2curve
Image8* ImProcFunctions::lab2rgb(LabImage* lab, int cx, int cy, int cw, int ch, const procparams::ColorManagementParams &icm, bool consider_histogram_settings)
{
    if (cx < 0) {
        cx = 0;
    }

    if (cy < 0) {
        cy = 0;
    }

    if (cx + cw > lab->W) {
        cw = lab->W - cx;
    }

    if (cy + ch > lab->H) {
        ch = lab->H - cy;
    }

    Image8* image = new Image8(cw, ch);
    Glib::ustring profile;

    cmsHPROFILE oprof = nullptr;
    int pro = 0;//int pro to switch to Working Profile histogram mode without gamma
    if (settings->HistogramWorking && consider_histogram_settings) {
        profile = icm.workingProfile;
        pro = 1;//no gamma for histogram and navigator panel and Lockable color picker
    } else {
        profile = icm.outputProfile;
        //pro = 0 histogram and navigator panel and Lockable color picker with gamma
        if (icm.outputProfile.empty() || icm.outputProfile == ColorManagementParams::NoICMString) {
            profile = "sRGB";
        }

        oprof = ICCStore::getInstance()->getProfile(profile);
    }

    if (oprof) {
        const cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE | (icm.outputBPC ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0); // NOCACHE is important for thread safety

        lcmsMutex->lock();
        cmsHPROFILE LabIProf  = cmsCreateLab4Profile(nullptr);
        cmsHTRANSFORM hTransform = cmsCreateTransform(LabIProf, TYPE_Lab_DBL, oprof, TYPE_RGB_FLT, icm.outputIntent, flags);
        cmsCloseProfile(LabIProf);
        lcmsMutex->unlock();

        unsigned char *data = image->data;

        // cmsDoTransform is relatively expensive
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            AlignedBuffer<double> pBuf(3 * cw);
            AlignedBuffer<float> oBuf(3 * cw);
            double *buffer = pBuf.data;
            float *outbuffer = oBuf.data;
            int condition = cy + ch;

#ifdef _OPENMP
            #pragma omp for firstprivate(lab) schedule(dynamic,16)
#endif

            for (int i = cy; i < condition; i++) {
                const int ix = i * 3 * cw;
                int iy = 0;
                float* rL = lab->L[i];
                float* ra = lab->a[i];
                float* rb = lab->b[i];

                for (int j = cx; j < cx + cw; j++) {
                    buffer[iy++] = rL[j] / 327.68f;
                    buffer[iy++] = ra[j] / 327.68f;
                    buffer[iy++] = rb[j] / 327.68f;
                }

                cmsDoTransform(hTransform, buffer, outbuffer, cw);
                copyAndClampLine(outbuffer, data + ix, cw);
            }
        } // End of parallelization

        cmsDeleteTransform(hTransform);

    } else {
        const auto xyz_rgb = ICCStore::getInstance()->workingSpaceInverseMatrix(profile);
        copyAndClamp(lab, image->data, xyz_rgb, multiThread, pro);//int pro = 1 to switch to Working Profile for histogram and navigator panel and Lockable color picker whitout gamma
    }

    return image;
}


/** @brief Convert the final Lab image to the output RGB color space
 *
 * Used in processImage   (rtengine/simpleprocess.cc)
 *
 * Provide a pointer to a 7 floats array for "ga" (uninitialized ; this array will be filled with the gamma values) if you want
 * to use the custom gamma scenario. Those gamma values will correspond to the ones of the chosen standard output profile
 * (Prophoto if non standard output profile given)
 *
 * If "ga" is NULL, then we're considering standard gamma with the chosen output profile.
 *
 * Generate an Image16
 *
 * If a custom gamma profile can be created, divide by 327.68, convert to xyz and apply the custom gamma transform
 * otherwise divide by 327.68, convert to xyz and apply the sRGB transform, before converting with gamma2curve
 */
Imagefloat* ImProcFunctions::lab2rgbOut(LabImage* lab, int cx, int cy, int cw, int ch, const procparams::ColorManagementParams &icm)
{

    if (cx < 0) {
        cx = 0;
    }

    if (cy < 0) {
        cy = 0;
    }

    if (cx + cw > lab->W) {
        cw = lab->W - cx;
    }

    if (cy + ch > lab->H) {
        ch = lab->H - cy;
    }

    Imagefloat* image = new Imagefloat(cw, ch);
    cmsHPROFILE oprof = ICCStore::getInstance()->getProfile(icm.outputProfile);

    if (oprof) {
        cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE;

        if (icm.outputBPC) {
            flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
        }

        lcmsMutex->lock();
        cmsHPROFILE iprof = cmsCreateLab4Profile(nullptr);
        cmsHTRANSFORM hTransform = cmsCreateTransform(iprof, TYPE_Lab_FLT, oprof, TYPE_RGB_FLT, icm.outputIntent, flags);
        lcmsMutex->unlock();

        image->ExecCMSTransform(hTransform, *lab, cx, cy);
        cmsDeleteTransform(hTransform);
        cmsCloseProfile(iprof);
        image->normalizeFloatTo65535();
    } else {

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

        for (int i = cy; i < cy + ch; i++) {
            float R, G, B;
            float* rL = lab->L[i];
            float* ra = lab->a[i];
            float* rb = lab->b[i];

            for (int j = cx; j < cx + cw; j++) {

                float fy = (Color::c1By116 * rL[j]) / 327.68f + Color::c16By116; // (L+16)/116
                float fx = (0.002f * ra[j]) / 327.68f + fy;
                float fz = fy - (0.005f * rb[j]) / 327.68f;
                float LL = rL[j] / 327.68f;

                float x_ = 65535.0f * Color::f2xyz(fx) * Color::D50x;
                //float y_ = 65535.0 * Color::f2xyz(fy);
                float z_ = 65535.0f * Color::f2xyz(fz) * Color::D50z;
                float y_ = (LL > (float)Color::epskap) ? 65535.0f * fy * fy * fy : 65535.0f * LL / (float)Color::kappa;

                Color::xyz2srgb(x_, y_, z_, R, G, B);

                image->r(i - cy, j - cx) = Color::gamma2curve[CLIP(R)];
                image->g(i - cy, j - cx) = Color::gamma2curve[CLIP(G)];
                image->b(i - cy, j - cx) = Color::gamma2curve[CLIP(B)];
            }
        }
    }

    return image;
}

void ImProcFunctions::preserv(LabImage *nprevl, LabImage *provis, int cw, int ch)
{
    //avoid too strong in middle values chroma when changing primaries
    float pres = 0.01f * params->icm.preser;
    float neutral = 2000000000.f;//if a2 + b2 < 200000000 scale 0..100 a and b about : 140 > a & b > -140  decrease effect
    float medneutral = 10000000.f;//plein effect 10 > a & b > -10
    float aaneu = 1.f / (medneutral - neutral);
    float bbneu = - aaneu * neutral;
#ifdef _OPENMP
    #pragma omp for schedule(dynamic, 16) nowait
#endif

    for (int i = 0; i < ch; ++i)
        for (int j = 0; j < cw; ++j) {
            float neu = SQR(provis->a[i][j]) + SQR(provis->b[i][j]);

            if (neu < medneutral) {//plein effect
                nprevl->a[i][j] = intp(pres, provis->a[i][j], nprevl->a[i][j]);
                nprevl->b[i][j] = intp(pres, provis->b[i][j], nprevl->b[i][j]);
            } else if (neu < neutral) {//decrease effect
                float presred = aaneu * neu + bbneu;
                nprevl->a[i][j] = intp(pres * presred, provis->a[i][j], nprevl->a[i][j]);
                nprevl->b[i][j] = intp(pres * presred, provis->b[i][j], nprevl->b[i][j]);
            }
        }
}



// ACES-style gamut compression
//
// tweaked from the original from https://github.com/jedypod/gamut-compress
// tweaked from CTL in ART thanks to Alberto Griggio

//from ACES https://docs.acescentral.com/specifications/rgc/#appendix-c-illustrations
// https://docs.acescentral.com/specifications/rgc/#appendix-d-ctl-reference-implementation
// https://docs.acescentral.com/specifications/rgc/
// Distance from achromatic which will be compressed to the gamut boundary
// Values calculated to encompass the encoding gamuts of common digital cinema cameras
//const float LIM_CYAN =  1.147;
//const float LIM_MAGENTA = 1.264;
//const float LIM_YELLOW = 1.312;

//Percentage of the core gamut to protect
// Values calculated to protect all the colors of the ColorChecker Classic 24 as given by
// ISO 17321-1 and Ohta (1997)
//const float THR_CYAN = 0.815;
//const float THR_MAGENTA = 0.803;
//const float THR_YELLOW = 0.880;

// Aggressiveness of the compression curve
//const float PWR = 1.2;

//Jacques Desmis March 2026
void ImProcFunctions::gamutcompr( Imagefloat *src, Imagefloat *dst, int beginend, float powe, int nbsegam,  float &mac, float &mac0, float &mac1, float &mac2) const
{
     if (settings->verbose) {
        printf("Apply compression gamut \n");
     }

    using Triple = std::array<double, 3>;

    using Matrix = std::array<Triple, 3>;

    const TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);

    Matrix wpro = {}; //working profile set in Matrix format
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            wpro[r][c] = wprof[r][c];
        }
    }
    //dcip3 Rec2020, srgb, prophoto, acesp1 - Compression gamut matrix profile
    Matrix dcip3 = {};
        //in fact the white point is "special" - 0.314 - 0.351 Theater
        dcip3[0][0] = 0.4861607;//0.4451698;(original) //0.4861607 with chromatic adaptation D63 => D50
        dcip3[0][1] = 0.3238514;//0.2771344;(original)//0.3238514 with chromatic adaptation D63 => D50
        dcip3[0][2] = 0.1541879;//0.1722827;(original//0.1541879 with chromatic adaptation D63 => D50
        dcip3[1][0] = 0.2266839;//0.2094917;(original//0.2266839 with chromatic adaptation D63 => D50
        dcip3[1][1] = 0.7103336;//0.7215953;(original//0.7103336 with chromatic adaptation D63 => D50
        dcip3[1][2] = 0.0629826;//0.0689131;(original// 0.0629826 with chromatic adaptation D63 => D50
        dcip3[2][0] = -0.0008016;//0.0;(original//-0.0008016 with chromatic adaptation D63 => D50
        dcip3[2][1] =  0.0432353;//0.0470606;(original// 0.0432353 with chromatic adaptation D63 => D50
        dcip3[2][2] =  0.7824663;//0.9073554;(original// 0.7824663 with chromatic adaptation D63 => D50

    Matrix Rec2020 = {};
        Rec2020[0][0] = 0.6734241;
        Rec2020[0][1] = 0.1656411;
        Rec2020[0][2] = 0.1251286;
        Rec2020[1][0] = 0.2790177;
        Rec2020[1][1] = 0.6753402;
        Rec2020[1][2] = 0.0456377;
        Rec2020[2][0] = -0.0019300;
        Rec2020[2][1] = 0.0299784;
        Rec2020[2][2] = 0.7973330;

    Matrix srgb = {};
        srgb[0][0] = 0.4360747;
        srgb[0][1] = 0.3850649;
        srgb[0][2] = 0.1430804;
        srgb[1][0] = 0.2225045;
        srgb[1][1] = 0.7168786;
        srgb[1][2] = 0.0606169;
        srgb[2][0] = 0.0139322;
        srgb[2][1] = 0.0971045;
        srgb[2][2] = 0.7141733;

    Matrix adobe = {};
        adobe[0][0] = 0.6097559;
        adobe[0][1] = 0.2052401;
        adobe[0][2] = 0.1492240;
        adobe[1][0] = 0.3111242;
        adobe[1][1] = 0.6256560;
        adobe[1][2] = 0.0632197;
        adobe[2][0] = 0.0194811;
        adobe[2][1] = 0.0608902;
        adobe[2][2] = 0.7448387;

    Matrix prophoto = {};//prophoto
        prophoto[0][0] = 0.7976749;
        prophoto[0][1] = 0.1351917;
        prophoto[0][2] = 0.0313534;
        prophoto[1][0] = 0.2880402;
        prophoto[1][1] = 0.7118741;
        prophoto[1][2] = 0.0000857;
        prophoto[2][0] = 0.0;
        prophoto[2][1] = 0.0;
        prophoto[2][2] = 1.2118128;

    Matrix acesp1 = {};//aces P1
        acesp1[0][0] = 0.689697;
        acesp1[0][1] = 0.149944;
        acesp1[0][2] = 0.124559;
        acesp1[1][0] = 0.284448;
        acesp1[1][1] = 0.671758;
        acesp1[1][2] = 0.043794;
        acesp1[2][0] = -0.006043;
        acesp1[2][1] = 0.009998;
        acesp1[2][2] = 0.820945;

    Matrix beta = {};//Beta RGB near Pointers'gamut
        beta[0][0] = 0.6712537;
        beta[0][1] = 0.1745834;
        beta[0][2] = 0.1183829;
        beta[1][0] = 0.3032726;
        beta[1][1] = 0.6637861;
        beta[1][2] = 0.0329413;
        beta[2][0] = 0.0000000;
        beta[2][1] = 0.0407010;
        beta[2][2] = 0.7845090;

    Matrix out = {};
    if (beginend == 0) {//at the beginning of the process
        if (params->cg.colorspace == "rec2020") {
            out = Rec2020;
        } else if  (params->cg.colorspace == "prophoto") {
            out = prophoto;
        } else if  (params->cg.colorspace == "adobe") {
            out = adobe;
        } else if  (params->cg.colorspace == "srgb") {
            out = srgb;
        } else if  (params->cg.colorspace == "dcip3") {
            out = dcip3;
        } else if  (params->cg.colorspace == "acesp1") {
            out = acesp1;
        } else if  (params->cg.colorspace == "beta") {
            out = beta;
        } else {
            out = acesp1;// Should never happen, but just in case.
        }
    } else if (beginend == 1) {//at the end of the process. Only 4 cases, which are the cases, at this stage, actually possible 
        if (params->icm.wgamut == ColorManagementParams::Wwgamut::REC2020) {
            out = Rec2020;
        } else if (params->icm.wgamut == ColorManagementParams::Wwgamut::ADOBE) {
            out = adobe;
        } else if (params->icm.wgamut == ColorManagementParams::Wwgamut::SRGB) {
            out = srgb;
        } else if (params->icm.wgamut == ColorManagementParams::Wwgamut::DCIP3) {
            out = dcip3;
        }
        
    } else if (beginend == 2) {//at the end of the process Selective Editing CAM16. Only 4 cases, which are the cases, at this stage, actually possible 
        if (nbsegam == 1) {
            out = Rec2020;
        } else if (nbsegam == 2) {
            out = adobe;
        } else if (nbsegam == 3) {
            out = srgb;
        } else if (nbsegam == 4) {
            out = dcip3;
        }
    }

    Matrix inv_out = {};
    if (!rtengine::invertMatrix(out, inv_out)) {//invert matrix
        printf("Matrix is not invertible, skipping\n");
    }

    Matrix Rprov = {};
    Color::multip(inv_out, wpro, Rprov);//multiply matrix
    Matrix to_out = {};

    Color::transpose(Rprov, to_out);//transpose Matrix for output

    Matrix from_out = {};//inverse to output
    if (!rtengine::invertMatrix(to_out, from_out)) {
        printf("Matrix is not invertible, skipping\n");

    }

    //parameters from GUI
    /* 
    //old manner to do with only 1 case 'beginning'
    const auto thc = static_cast<float>(params->cg.th_c);
    const auto thm = static_cast<float>(params->cg.th_m);
    const auto thy = static_cast<float>(params->cg.th_y);
    const auto dc = static_cast<float>(params->cg.d_c);
    const auto dm = static_cast<float>(params->cg.d_m);
    const auto dy = static_cast<float>(params->cg.d_y);
    const auto pw = static_cast<float>(params->cg.pwr);
    const bool roll = params->cg.rolloff;
    */
    //beginning of process with GUI compressgamut.cc
    float thc = static_cast<float>(params->cg.th_c);
    float thm = static_cast<float>(params->cg.th_m);
    float thy = static_cast<float>(params->cg.th_y);
    float dc = static_cast<float>(params->cg.d_c);
    float dm = static_cast<float>(params->cg.d_m);
    float dy = static_cast<float>(params->cg.d_y);
    float pw = static_cast<float>(params->cg.pwr);
    bool roll = params->cg.rolloff;
    
    if (beginend == 1) {//with GUI Icmpanel.cc
        //take values from ART CTL - odt.ctl - Copyright (c) 2023 Thatcher Freeman
        //It's considerably simpler at the end of the process. We don't have (at least in theory) the problems related to data completely out of gamut (Sunset, LEDs).
        //This needs to be confirmed by testing and possibly changed.
        //I made small changes
        thc = 0.8f;
        thm = 0.75f;
        thy = 0.85f;
        dc = 1.15f;
        dm = 1.25f;
        dy = 1.5f;
        pw = params->icm.wgampower;
        roll = true;
    }

    if (beginend == 2) {//with GUI Selective editing CAM16
        //take values from ART CTL - odt.ctl - Copyright (c) 2023 Thatcher Freeman
        //It's considerably simpler at the end of the process. We don't have (at least in theory) the problems related to data completely out of gamut (Sunset, LEDs).
        //This needs to be confirmed by testing and possibly changed.
        //I made small changes
        thc = 0.8f;
        thm = 0.75f;
        thy = 0.85f;
        dc = 1.15f;
        dm = 1.25f;
        dy = 1.5f;
        pw = powe;
        roll = true;
    }


    const std::array<float, 3> th{thc, thm, thy};//set parameter GUI in th
    const std::array<float, 3> dl{dc, dm, dy};//set parameter GUI in dl

    const int height = src->getHeight();
    const int width = src->getWidth();

    constexpr float range = 65535.f;
    float maxac = 0.f;
    float maxac0 = 0.f;
    float maxac1 = 0.f;
    float maxac2 = 0.f;

#ifdef _OPENMP
        #   pragma omp parallel for reduction(max:maxac, maxac0, maxac1, maxac2) schedule(dynamic,16) if (multiThread)
#endif

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            const float r = src->r(i, j) / range;//in interval 0.. 1
            const float g = src->g(i, j) / range;
            const float b = src->b(i, j) / range;
            std::array<float, 3> rgb_in{r, g, b};
            float rout = 0.f;
            float gout = 0.f;
            float bout = 0.f;
            float ac = 0.f;
            float ac0 = 0.f;
            float ac1 = 0.f;
            float ac2 = 0.f;
            //find maximum achromatic for red, green, blue

            Color::aces_reference_gamut_compression(rgb_in, th, dl, to_out, from_out, pw, roll, rout, gout, bout, ac, ac0, ac1, ac2);

            if (ac  > maxac)  {
                maxac  = ac;
            }
            if (ac0 > maxac0) {
                maxac0 = ac0;
            }
            if (ac1 > maxac1) {
                maxac1 = ac1;
            }
            if (ac2 > maxac2) {
                maxac2 = ac2;
            }
            dst->r(i, j) = range * rout;//in interval 0..65535
            dst->g(i, j) = range * gout;
            dst->b(i, j) = range * bout;
        }
    }
    mac = maxac;
    mac0 = maxac0;
    mac1 = maxac1;
    mac2 = maxac2; 
}



inline float power_norm2(float r, float g, float b)
{
    r = std::abs(r);
    g = std::abs(g);
    b = std::abs(b);

    float r2 = SQR(r);
    float g2 = SQR(g);
    float b2 = SQR(b);
    float d = r2 + g2 + b2;
    float n = r * r2 + g * g2 + b * b2;

    return n / std::max(d, 1e-12f);
}


inline float norm3(float r, float g, float b, TMatrix ws)
{
    constexpr float hi = std::numeric_limits<float>::max() / 100.f;
    return std::min(hi, power_norm2(r, g, b) / 2.f + Color::rgbLuminance(r, g, b, ws) / 2.f);
}


void ImProcFunctions::apsatur(int sp, Imagefloat* tmpImage, Imagefloat* tmpImage2, int bfw, int bfh, float satu) 
//act on saturation RGB after Abstract Profile - main and Selective Editing
{
    const TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);
    
    const float noise = pow_F(2.f, -16.f);

    const auto sfcie =
        [=](float s, float c) -> float
            {
                if (c > noise) {
                    return 1.f - min(std::abs(s) / c, 1.f);
                } else {
                    return 0.f;
                }
            };
      
    const auto apply_satcie =
        [&](float &r, float &g, float &b, float f, float ll, float satu) -> void
            {
                float rl = r - ll;
                float gl = g - ll;
                float bl = b - ll;
                float s = intp(max(sfcie(rl, r), sfcie(gl, g), sfcie(bl, b)), pow_F(f, 0.3f * satu) * (0.6f) + (0.4f), 1.f);
                r = ll + s * rl;
                g = ll + s * gl;
                b = ll + s * bl;
            };
            
#ifdef _OPENMP
            #pragma omp parallel for if (multiThread)
#endif
            for (int y = 0; y < bfh; ++y)
                for (int x = 0; x < bfw; ++x) {
                    float R0 = tmpImage->r(y, x) / 65535.f;
                    float G0 = tmpImage->g(y, x) / 65535.f;
                    float B0 = tmpImage->b(y, x) / 65535.f;
                    float R2 = tmpImage2->r(y, x) /65535.f;
                    float G2 = tmpImage2->g(y, x) / 65535.f;
                    float B2 = tmpImage2->b(y, x) / 65535.f;
                    float fcie2 = norm3(R2, G2, B2, wprof);                                       
                    float fcie0 = norm3(R0, G0, B0, wprof);
                    fcie0 = rtengine::max(fcie0, noise);
                    fcie2 = rtengine::max(fcie2, noise);
                    float amp = fcie0 / fcie2;
                    apply_satcie(R0, G0, B0, amp, fcie2, satu);//always apply saturation
                    tmpImage->r(y, x) = R0 * 65535.f;
                    tmpImage->g(y, x) = G0 * 65535.f;
                    tmpImage->b(y, x) = B0 * 65535.f;
                }               
}


// code taken from Darktable - and adapted to Rawtherapee
// It was only used to calculate polar coordinates. Why reinvent the wheel when it's already been done elsewhere?
constexpr float determinant(float a, float b, float c, float d)
{
    return a * d - b * c;
}


float intersect_line_segments(float x1, float y1,
                              float x2, float y2,
                              float x3, float y3,
                              float x4, float y4)
{
    const float denominator = determinant(x1 - x2, x3 - x4, y1 - y2, y3 - y4);
    if (denominator == 0.0) {
        return FLT_MAX; // lines don't intersect
    }

    const float t = determinant(x1 - x3, x3 - x4, y1 - y3, y3 - y4) / denominator;
    if (t >= 0.0) {
        return t;
    }
    return FLT_MAX; // intersection is in the wrong direction
}


float find_distance_to_edge(float primaries[3][2], float cos_angle, float sin_angle, cmsCIExyY xyd)
{
    const float whitepoint[2] = {(float) xyd.x, (float) xyd.y};
    const float x1 = whitepoint[0];
    const float y1 = whitepoint[1];
    const float x2 = x1 + cos_angle;
    const float y2 = y1 + sin_angle;

    float distance_to_edge = FLT_MAX;
    for (int i = 0; i < 3; i = i+1) {
        const int next_i = (i + 1) % 3;
        const float x3 = primaries[i][0];
        const float y3 = primaries[i][1];
        const float x4 = primaries[next_i][0];
        const float y4 = primaries[next_i][1];
        const float distance = intersect_line_segments(x1, y1, x2, y2, x3, y3, x4, y4);
        if (distance < distance_to_edge) {
            distance_to_edge = distance;
        }
    }

    return distance_to_edge;
}

void rotate_and_scale_primary(float primaries[3][2], float scaling, float rotation, int primary_index, float *newprimxy, cmsCIExyY xyd)
{
    const float whitepoint[2] = {(float) xyd.x, (float) xyd.y};//I change white point, and I use the default one (or user choice).

    // Generate a custom set of tone mapping primaries by scaling
    // and rotating the primaries of the given profile.
    const float px = primaries[primary_index][0];
    const float py = primaries[primary_index][1];
    const float dx = px - whitepoint[0];
    const float dy = py - whitepoint[1];
    const float angle = atan2(dy, dx) + rotation;
    const float cos_angle = cos(angle);
    const float sin_angle = sin(angle);
    const float distance_to_edge = find_distance_to_edge(primaries, cos_angle, sin_angle, xyd);
    const float dx_new = scaling * distance_to_edge * cos_angle;
    const float dy_new = scaling * distance_to_edge * sin_angle;
    newprimxy[0] = dx_new + whitepoint[0];
    newprimxy[1] = dy_new + whitepoint[1];
}
//end code Darktable

/* 
 * Working TRC - Abstract Profile
 * Copyright Jacques Desmis jdesmis@gmail.com 
 * 2021 - 2026
*/ 

void ImProcFunctions::workingtrc(int sp, Imagefloat* src, Imagefloat* dst, int cw, int ch, int mul, Glib::ustring &profile, double gampos, double slpos, int cat, int &illum, int prim, int locprim,
                                 float &rdx, float &rdy, float &grx, float &gry, float &blx, float &bly, float &meanx, float &meany, float &meanxe, float &meanye, float &maxdat, double *p,
                                 cmsHTRANSFORM &transform, bool normalizeIn, bool normalizeOut, bool keepTransForm, bool gamutcontrol) const
{
    const TMatrix wprof = ICCStore::getInstance()->workingSpaceMatrix(params->icm.workingProfile);

    double wb2[3][3];
    constexpr float epsilon =  0.00001f;//To avoid negative values and / or set a minimum RGB float value to avoid incorrect calculations. We can change epsilon if need.

#ifdef _OPENMP
        #pragma omp parallel for
#endif
        for (int i = 0; i < ch; ++i)
            for (int j = 0; j < cw; ++j) {
                src->r(i, j) = (float) rtengine::max(src->r(i, j), epsilon);
                src->g(i, j) = (float) rtengine::max(src->g(i, j), epsilon);
                src->b(i, j) = (float) rtengine::max(src->b(i, j), epsilon); 
            }



    if (mul == 5) {//only second pass workingtrc - avoid this code first pass
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                wb2[r][c] = wprof[r][c];
            }
        }
        
        //provis  - samme approach as in WB itcwb
        //we can add others function on colors ...others than mean (actually)
        const int precision = 3;
        const int bfw = cw / precision + ((cw % precision) > 0 ? 1 : 0);
        const int bfh = ch / precision + ((ch % precision) > 0 ? 1 : 0);

        std::unique_ptr<Imagefloat> provis(new Imagefloat(bfw, bfh));//cw, ch

#ifdef _OPENMP
        #pragma omp parallel for
#endif

        for (int i = 0; i < bfh ; ++i) {
            const int ii = i * precision;

            if (ii < ch) {
                for (int j = 0, jj = 0; j < bfw ; ++j, jj += precision) {
                    provis->r(i, j) = src->r(ii, jj);
                    provis->g(i, j) = src->g(ii, jj);
                    provis->b(i, j) = src->b(ii, jj);
                }
            }
        }


// I try to find the dominant color by a simple way (average of x and y)
// It is probably intellectually more relevant to place this algorithm at the end, but it is complex at the GUI level (at least for me).
// The errors made are relatively minimal and result seems good enough
        meanx = 0.f;
        meany = 0.f;

#ifdef _OPENMP
        #pragma omp parallel for reduction(+:meanx, meany) if(multiThread)
#endif

        for (int y = 0; y < bfh ; ++y) {
            for (int x = 0; x < bfw ; ++x) {
                const float RR = provis->r(y, x);
                const float GG = provis->g(y, x);
                const float BB = provis->b(y, x);
                float xcb, ycb, zcb;
                Color::rgbxyz(RR, GG, BB, xcb, ycb, zcb, wb2);
                float X_r = xcb;
                float Y_r = ycb;
                float Z_r = zcb;
                if(gamutcontrol) {
                    Color::gamutmap(X_r, Y_r, Z_r, wb2);//gamut control
                }
                const float som = X_r + Y_r + Z_r;
                X_r = X_r / som;
                Y_r = Y_r / som;
                meanx += X_r;
                meany += Y_r;
            }
        }

        meanx /= (bfh * bfw);
        meany /= (bfh * bfw);
        meanx += 0.005f;
        meany += 0.005f; //empirical mean delta with value end in process

        if (settings->verbose) {
            printf("Estimation dominant color : x=%f y=%f\n", (double) meanx, (double) meany);
        }

    }

    double wprofprim[3][3];//store primaries to XYZ

    const float toxyz[3][3] = {
        {
            static_cast<float>(wprof[0][0] / ((normalizeIn ? 65535.0 : 1.0))), //I have suppressed / Color::D50x
            static_cast<float>(wprof[0][1] / ((normalizeIn ? 65535.0 : 1.0))),
            static_cast<float>(wprof[0][2] / ((normalizeIn ? 65535.0 : 1.0)))
        }, {
            static_cast<float>(wprof[1][0] / (normalizeIn ? 65535.0 : 1.0)),
            static_cast<float>(wprof[1][1] / (normalizeIn ? 65535.0 : 1.0)),
            static_cast<float>(wprof[1][2] / (normalizeIn ? 65535.0 : 1.0))
        }, {
            static_cast<float>(wprof[2][0] / ((normalizeIn ? 65535.0 : 1.0))), //I have suppressed / Color::D50z
            static_cast<float>(wprof[2][1] / ((normalizeIn ? 65535.0 : 1.0))),
            static_cast<float>(wprof[2][2] / ((normalizeIn ? 65535.0 : 1.0)))
        }
    };

    if (profile == "sRGB" || profile == "Adobe RGB" || profile == "ProPhoto" || profile == "WideGamut"  || profile == "BruceRGB" || profile == "Beta RGB" || profile == "BestRGB" || profile == "Rec2020" || profile == "ACESp0" || profile == "ACESp1" || profile == "JDCmax" || profile == "JDCmax stdA") {
        if (settings->verbose) {
            printf("Profile=%s\n", profile.c_str());
        }
    } else {
        if (settings->verbose) {
            printf("profile not accepted\n");
        }

        return;
    }

    if (mul == -5 &&  gampos == 2.4 && slpos == 12.92310) {//must be change if we change settings RT sRGB
        //only in this case we can shortcut..all process..no gamut control..because we reduce...leads to very small differences, but big speedup
#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic, 16) if (multiThread)
#endif

        for (int i = 0; i < ch; ++i)
            for (int j = 0; j < cw; ++j) {
                double r = (double) src->r(i, j);
                double g = (double) src->g(i, j);
                double b = (double) src->b(i, j);
                r = (Color::igammatab_srgb[r]) / 65535.;
                g = (Color::igammatab_srgb[g]) / 65535.;
                b = (Color::igammatab_srgb[b]) / 65535.;
                dst->r(i, j) =  r;
                dst->g(i, j) =  g;
                dst->b(i, j) =  b;
            }

        return;

    }

    if (mul == 1) { // || (params->icm.wprim == ColorManagementParams::Primaries::DEFAULT && params->icm.will == ColorManagementParams::Illuminant::DEFAULT)) { //shortcut and speedup when no call primaries and illuminant - no gamut control...in this case be careful
        GammaValues g_a; //gamma parameters
        const double pwr = 1.0 / static_cast<double>(gampos);
        Color::calcGamma(pwr, slpos, g_a); // call to calcGamma with selected gamma and slope

#ifdef _OPENMP
        #   pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

        for (int y = 0; y < ch; ++y) {
            int x = 0;
#if defined(__SSE2__) || defined(RT_SIMDE)

            for (; x < cw - 3; x += 4) {
                STVFU(dst->r(y, x), F2V(65536.f) * gammalog(LVFU(src->r(y, x)), F2V(gampos), F2V(slpos), F2V(g_a[3]), F2V(g_a[4])));
                STVFU(dst->g(y, x), F2V(65536.f) * gammalog(LVFU(src->g(y, x)), F2V(gampos), F2V(slpos), F2V(g_a[3]), F2V(g_a[4])));
                STVFU(dst->b(y, x), F2V(65536.f) * gammalog(LVFU(src->b(y, x)), F2V(gampos), F2V(slpos), F2V(g_a[3]), F2V(g_a[4])));
            }

#endif

            for (; x < cw; ++x) {
                dst->r(y, x) = 65536.f * gammalog(src->r(y, x), gampos, slpos, g_a[3], g_a[4]);
                dst->g(y, x) = 65536.f * gammalog(src->g(y, x), gampos, slpos, g_a[3], g_a[4]);
                dst->b(y, x) = 65536.f * gammalog(src->b(y, x), gampos, slpos, g_a[3], g_a[4]);
            }
        }

        return;
    }


    float redxx = params->icm.redx;
    float redyy = params->icm.redy;
    float bluxx = params->icm.blux;
    float bluyy = params->icm.bluy;
    float grexx = params->icm.grex;
    float greyy = params->icm.grey;
    constexpr float epsil = 0.0001f;

    double Wx = 1.0;
    double Wz = 1.0;
    cmsCIExyY xyD;

    if (locprim == 1  && mul == 5) {//Selective editing
        rdx = params->locallab.spots.at(sp).redxl;
        rdy = params->locallab.spots.at(sp).redyl;
        grx = params->locallab.spots.at(sp).grexl;
        gry = params->locallab.spots.at(sp).greyl;
        blx = params->locallab.spots.at(sp).bluxl;
        bly = params->locallab.spots.at(sp).bluyl;

        if (params->locallab.spots.at(sp).illMethod == "d50") {
            illum = 2;
            xyD = {0.3457, 0.3585, 1.0}; // near LCMS values but not perfect... it's a compromise!!
            Wx = 0.964295676;
            Wz = 0.825104603;
        } else if (params->locallab.spots.at(sp).illMethod == "d60") {
            illum = 4;
            Wx = 0.952646075;
            Wz = 1.008825184;
            xyD = {0.32168, 0.33767, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "d65") {
            illum = 5;
            Wx = 0.95045471;
            Wz = 1.08905029;
            xyD = {0.312700492, 0.329000939, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "d41") {
            illum = 1;
            Wx = 0.991488263;
            Wz = 0.631604625;
            xyD = {0.376137, 0.374021, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "d55") {
            illum = 3;
            Wx = 0.956565934;
            Wz = 0.920253249;
            xyD = {0.332424, 0.347426, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "d80") {
            illum = 6;
            Wx = 0.950095542;
            Wz = 1.284213976;
            xyD = {0.293756, 0.309185, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "d120") {
            illum = 7;
            Wx = 0.979182;
            Wz = 1.623623;
            xyD = {0.269669, 0.28078, 1.0};            
        } else if (params->locallab.spots.at(sp).illMethod == "stda") {
            illum = 8;
            Wx = 1.098500393;
            Wz = 0.355848714;
            xyD = {0.447573, 0.407440, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "T2000") {
            illum = 9;
            Wx = 1.274335;
            Wz = 0.145233;
            xyD = {0.526591, 0.41331, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "T1500") {
            illum = 10;
            Wx = 1.489921;
            Wz = 0.053826;
            xyD = {0.585703, 0.393157, 1.0};
        } else if (params->locallab.spots.at(sp).illMethod == "iE") {
            illum = 20;
            Wx = 1.;
            Wz = 1.;
            xyD = {0.333333, 0.333333, 1.0};
        }
    }

    if (prim == 14 && locprim == 0 && mul == 5) {//convert data area to xy - Abstract Profile
        const float redgraphx =  params->icm.labgridcieALow;
        const float redgraphy =  params->icm.labgridcieBLow;
        const float blugraphx =  params->icm.labgridcieAHigh;
        const float blugraphy =  params->icm.labgridcieBHigh;
        const float gregraphx =  params->icm.labgridcieGx;
        const float gregraphy =  params->icm.labgridcieGy;
        constexpr float INV_OFFSET_MODIFIER = 1.f / OFFSET_MODIFIER;

        redxx = INV_OFFSET_MODIFIER * (redgraphx + 1.f) - CIExy_MARGIN;
        redxx = rtengine::LIM(redxx, 0.41f, 1.f);//limit values for xy (arbitrary)
        redyy = INV_OFFSET_MODIFIER * (redgraphy + 1.f) - CIExy_MARGIN;
        redyy = rtengine::LIM(redyy, 0.f, 0.7f);
        bluxx = INV_OFFSET_MODIFIER * (blugraphx + 1.f) - CIExy_MARGIN;
        bluxx = rtengine::LIM(bluxx, -0.1f, 0.5f);
        bluyy = INV_OFFSET_MODIFIER * (blugraphy + 1.f) - CIExy_MARGIN;
        bluyy = rtengine::LIM(bluyy, -0.1f, 0.49f);
        grexx = INV_OFFSET_MODIFIER * (gregraphx + 1.f) - CIExy_MARGIN;
        grexx = rtengine::LIM(grexx, -0.1f, 0.4f);
        greyy = INV_OFFSET_MODIFIER * (gregraphy + 1.f) - CIExy_MARGIN;
        greyy = rtengine::LIM(greyy, 0.5f, 1.f);
    }

    //fixed crash when there is no space or too small..just a line...Possible if bx, by aligned with Gx,Gy Rx,Ry
    //fix crash if user select 0 for redyy, bluyy, greyy
    if (redyy == 0.f) {
        redyy = epsil;
    }

    if (bluyy == 0.f) {
        bluyy = epsil;
    }

    if (greyy == 0.f) {
        greyy = epsil;
    }

    //fix crash if  grexx - redxx = 0
    float grered = 1.f;
    grered = grexx - redxx;

    if (grered == 0.f) {
        grered = epsil;
    }

    float ac = (greyy - redyy) / grered;
    float bc = greyy - ac * grexx;
    float yc = ac * bluxx + bc;

    if ((bluyy < yc + 0.0004f) && (bluyy > yc - 0.0004f)) { //under 0.0004 in some case crash because space too small
        return;
    }

    enum class ColorTemp {
        D50 = 5003, // for Widegamut, ProPhoto Best, Beta -> D50
        D65 = 6504, // for sRGB, AdobeRGB, Bruce Rec2020  -> D65
        D60 = 6005  // for ACES AP0 and AP1
    };
    double tempv4 = 5003.;

    if (locprim == 0 && mul == 5) {//Abstract profile
        switch (ColorManagementParams::Primaries(prim)) {
            case ColorManagementParams::Primaries::DEFAULT: {
                break;
            }

            case ColorManagementParams::Primaries::SRGB: {
                profile = "sRGB";
                break;
            }

            case ColorManagementParams::Primaries::ADOBE_RGB: {
                profile = "Adobe RGB";
                break;
            }

            case ColorManagementParams::Primaries::PRO_PHOTO: {
                profile = "ProPhoto";
                break;
            }

            case ColorManagementParams::Primaries::REC2020: {
                profile = "Rec2020";
                break;
            }

            case ColorManagementParams::Primaries::ACES_P1: {
                profile = "ACESp1";
                break;
            }

            case ColorManagementParams::Primaries::WIDE_GAMUT: {
                profile = "WideGamut";
                break;
            }

            case ColorManagementParams::Primaries::ACES_P0: {
                profile = "ACESp0";
                break;
            }

            case ColorManagementParams::Primaries::JDC_MAX: {
                profile = "JDCmax";
                break;
            }

            case ColorManagementParams::Primaries::JDC_MAXSTDA: {
                profile = "JDCmax stdA";
                break;
            }

            case ColorManagementParams::Primaries::BRUCE_RGB: {
                profile = "BruceRGB";
                break;
            }

            case ColorManagementParams::Primaries::BETA_RGB: {
                profile = "Beta RGB";
                break;
            }

            case ColorManagementParams::Primaries::BEST_RGB: {
                profile = "BestRGB";
                break;
            }

            case ColorManagementParams::Primaries::CUSTOM: {
                profile = "Custom";
                break;
            }

            case ColorManagementParams::Primaries::CUSTOM_POL: {
                profile = "Custompol";
                break;
            }

            case ColorManagementParams::Primaries::CUSTOM_GRID: {
                profile = "Custom";
                break;
            }
        }
    } else if (locprim == 1 && mul == 5) {//Selective Editing
        //local primaries
        if (prim == 1) {
            p[0] = 0.6400;    // sRGB primaries
            p[1] = 0.3300;
            p[2] = 0.3000;
            p[3] = 0.6000;
            p[4] = 0.1500;
            p[5] = 0.0600;
            tempv4 = 6504.;
            Wx = 0.95045471;
            Wz = 1.08905029;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];

        } else if (prim == 2) {
            p[0] = 0.6400;    //Adobe primaries
            p[1] = 0.3300;
            p[2] = 0.2100;
            p[3] = 0.7100;
            p[4] = 0.1500;
            p[5] = 0.0600;
            tempv4 = 6504.;
            Wx = 0.95045471;
            Wz = 1.08905029;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];

        } else if (prim == 3) {
            p[0] = 0.7347;    //ProPhoto 
            p[1] = 0.2653;
            p[2] = 0.1596;
            p[3] = 0.8404;
            p[4] = 0.0366;
            p[5] = 0.0001;
            Wx = 0.964295676;
            Wz = 0.825104603;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];

        } else if (prim == 4) {
            p[0] = 0.7080;    // Rec2020 and default primaries
            p[1] = 0.2920;
            p[2] = 0.1700;
            p[3] = 0.7970;
            p[4] = 0.1310;
            p[5] = 0.0460;
            tempv4 = 6504.;
            Wx = 0.95045471;
            Wz = 1.08905029;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];

        } else if (prim == 5) {
            p[0] = 0.713;    // ACES P1 primaries
            p[1] = 0.293;
            p[2] = 0.165;
            p[3] = 0.830;
            p[4] = 0.128;
            p[5] = 0.044;
            tempv4 = 6004.;
            Wx = 0.952646075;
            Wz = 1.008825184;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];

        } else if (prim == 6) {
            p[0] = 0.7350;    //Widegamut primaries
            p[1] = 0.2650;
            p[2] = 0.1150;
            p[3] = 0.8260;
            p[4] = 0.1570;
            p[5] = 0.0180;
            Wx = 0.964295676;
            Wz = 0.825104603;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        } else if (prim == 7) {
            p[0] = 0.7347;    //ACESp0 primaries
            p[1] = 0.2653;
            p[2] = 0.;
            p[3] = 1.0;
            p[4] = 0.0001;
            p[5] = -0.0770;
            tempv4 = 6004.;
            Wx = 0.952646075;
            Wz = 1.008825184;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        } else if (prim == 8) {
            p[0] = 0.734702;    // JDC max primaries
            p[1] = 0.265302;
            p[2] = 0.021908;
            p[3] = 0.930288;
            p[4] = 0.120593;
            p[5] = 0.001583;
            Wx = 0.964295676;
            Wz = 0.825104603;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        } else if (prim == 9) {
            p[0] = 0.734702;    // JDC max primaries
            p[1] = 0.265302;
            p[2] = 0.021908;
            p[3] = 0.930288;
            p[4] = 0.120593;
            p[5] = 0.001583;
            Wx = 1.098500393;
            Wz = 0.355848714;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        } else if (prim == 10) {
            p[0] = 0.64;    // Bruce primaries
            p[1] = 0.33;
            p[2] = 0.28;
            p[3] = 0.65;
            p[4] = 0.15;
            p[5] = 0.06;
            Wx = 0.95045471;
            Wz = 1.08905029;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        } else if (prim == 11) {
            p[0] = 0.6888;    // Beta primaries
            p[1] = 0.3112;
            p[2] = 0.1986;
            p[3] = 0.7551;
            p[4] = 0.1265;
            p[5] = 0.0352;
            Wx = 0.964295676;
            Wz = 0.825104603;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        } else if (prim == 12) {
            p[0] = 0.7347;    // Best primaries
            p[1] = 0.2653;
            p[2] = 0.2150;
            p[3] = 0.7750;
            p[4] = 0.13;
            p[5] = 0.0350;
            Wx = 0.964295676;
            Wz = 0.825104603;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
       } else if (prim == 15) {//not used in Selective Editing 
            p[0] = rdx;
            p[1] = rdy;
            p[2] = grx;
            p[3] = gry;
            p[4] = blx;
            p[5] = bly;
        } else {
            p[0] = 0.7080;  // Rec2020 and default primaries - normaly never used.
            p[1] = 0.2920;
            p[2] = 0.1700;
            p[3] = 0.7970;
            p[4] = 0.1310;
            p[5] = 0.0460;
            Wx = 0.95045471;
            Wz = 1.08905029;
            rdx = p[0];
            rdy = p[1];
            grx = p[2];
            gry = p[3];
            blx = p[4];
            bly = p[5];
        }
    }

    if (settings->verbose  && prim != 0) {
        printf("prim=%i Profile Destination=%s\n", prim, profile.c_str());
    }

    cmsHTRANSFORM hTransform = nullptr;

    if (transform) {
        hTransform = transform;
    } else {
        double pwr = 1.0 / gampos;
        double ts = slpos;
        int five = mul;


        if (gampos < 1.0) {
            pwr = gampos;
            gampos = 1. / gampos;
            five = -mul;
        }

        constexpr double eps = 0.000000001; // not divide by zero

        //primaries for 10 working profiles ==> output profiles
        if (locprim == 0  && mul ==5) {
            if (profile == "WideGamut") {
                p[0] = 0.7350;    //Widegamut primaries
                p[1] = 0.2650;
                p[2] = 0.1150;
                p[3] = 0.8260;
                p[4] = 0.1570;
                p[5] = 0.0180;
                illum = toUnderlying(ColorManagementParams::Illuminant::D50);
                Wx = 0.964295676;
                Wz = 0.825104603;

            } else if (profile == "Adobe RGB") {
                p[0] = 0.6400;    //Adobe primaries
                p[1] = 0.3300;
                p[2] = 0.2100;
                p[3] = 0.7100;
                p[4] = 0.1500;
                p[5] = 0.0600;
                tempv4 = 6504.;
                illum = toUnderlying(ColorManagementParams::Illuminant::D65);
                Wx = 0.95045471;
                Wz = 1.08905029;

            } else if (profile == "sRGB") {
                p[0] = 0.6400;    // sRGB primaries
                p[1] = 0.3300;
                p[2] = 0.3000;
                p[3] = 0.6000;
                p[4] = 0.1500;
                p[5] = 0.0600;
                tempv4 = 6504.;
                illum = toUnderlying(ColorManagementParams::Illuminant::D65);
                Wx = 0.95045471;
                Wz = 1.08905029;

            } else if (profile == "BruceRGB") {
                p[0] = 0.6400;    // Bruce primaries
                p[1] = 0.3300;
                p[2] = 0.2800;
                p[3] = 0.6500;
                p[4] = 0.1500;
                p[5] = 0.0600;
                tempv4 = 6504.;
                illum = toUnderlying(ColorManagementParams::Illuminant::D65);
                Wx = 0.95045471;
                Wz = 1.08905029;

            } else if (profile == "Beta RGB") {
                p[0] = 0.6888;    // Beta primaries
                p[1] = 0.3112;
                p[2] = 0.1986;
                p[3] = 0.7551;
                p[4] = 0.1265;
                p[5] = 0.0352;
                illum = toUnderlying(ColorManagementParams::Illuminant::D50);
                Wx = 0.964295676;
                Wz = 0.825104603;

            } else if (profile == "BestRGB") {
                p[0] = 0.7347;    // Best primaries
                p[1] = 0.2653;
                p[2] = 0.2150;
                p[3] = 0.7750;
                p[4] = 0.1300;
                p[5] = 0.0350;
                illum = toUnderlying(ColorManagementParams::Illuminant::D50);
                Wx = 0.964295676;
                Wz = 0.825104603;

            } else if (profile == "Rec2020") {
                p[0] = 0.7080;    // Rec2020 primaries and default
                p[1] = 0.2920;
                p[2] = 0.1700;
                p[3] = 0.7970;
                p[4] = 0.1310;
                p[5] = 0.0460;
                tempv4 = 6504.;
                illum = toUnderlying(ColorManagementParams::Illuminant::D65);
                Wx = 0.95045471;
                Wz = 1.08905029;

            } else if (profile == "ACESp0") {
                p[0] = 0.7347;    // ACES P0 primaries
                p[1] = 0.2653;
                p[2] = 0.0000;
                p[3] = 1.0;
                p[4] = 0.0001;
                p[5] = -0.0770;
                tempv4 = 6004.;
                illum = toUnderlying(ColorManagementParams::Illuminant::D60);
                Wx = 0.952646075;
                Wz = 1.008825184;

            } else if (profile == "JDCmax") {
                p[0] = 0.734702;    // JDC max primaries
                p[1] = 0.265302;
                p[2] = 0.021908;
                p[3] = 0.930288;
                p[4] = 0.120593;
                p[5] = 0.001583;
                illum = toUnderlying(ColorManagementParams::Illuminant::D50);
                Wx = 0.964295676;
                Wz = 0.825104603;

            } else if (profile == "JDCmax stdA") {
                p[0] = 0.734702;    // JDC max primaries and stdA
                p[1] = 0.265302;
                p[2] = 0.021908;
                p[3] = 0.930288;
                p[4] = 0.120593;
                p[5] = 0.001583;
                illum = toUnderlying(ColorManagementParams::Illuminant::STDA);
                Wx = 1.098500393;
                Wz = 0.355848714;

            } else if (profile == "ACESp1") {
                p[0] = 0.713;    // ACES P1 primaries
                p[1] = 0.293;
                p[2] = 0.165;
                p[3] = 0.830;
                p[4] = 0.128;
                p[5] = 0.044;
                tempv4 = 6004.;
                illum = toUnderlying(ColorManagementParams::Illuminant::D60);
                Wx = 0.952646075;
                Wz = 1.008825184;

            } else if (profile == "ProPhoto") {
                p[0] = 0.7347;    //ProPhoto
                p[1] = 0.2653;
                p[2] = 0.1596;
                p[3] = 0.8404;
                p[4] = 0.0366;
                p[5] = 0.0001;
                illum = toUnderlying(ColorManagementParams::Illuminant::D50);
                Wx = 0.964295676;
                Wz = 0.825104603;

            } else if (profile == "Custom" || profile == "Custompol") {
                p[0] = redxx;
                p[1] = redyy;
                p[2] = grexx;
                p[3] = greyy;
                p[4] = bluxx;
                p[5] = bluyy;

            } else {
                p[0] = 0.7080;    // default Rec2020 primaries always unused
                p[1] = 0.2920;
                p[2] = 0.1700;
                p[3] = 0.7970;
                p[4] = 0.1310;
                p[5] = 0.0460;
                illum = toUnderlying(ColorManagementParams::Illuminant::D65);
                Wx = 0.95045471;
                Wz = 1.08905029;
            }
        }


        if (slpos == 0) {
            slpos = eps;
        }

        GammaValues g_a; //gamma parameters
        Color::calcGamma(pwr, ts, g_a); // call to calcGamma with selected gamma and slope : return parameters for LCMS2


        cmsFloat64Number gammaParams[7];
        gammaParams[4] = g_a[3] * ts;
        gammaParams[0] = gampos;
        gammaParams[1] = 1. / (1.0 + g_a[4]);
        gammaParams[2] = g_a[4] / (1.0 + g_a[4]);
        gammaParams[3] = 1. / slpos;
        gammaParams[5] = 0.0;
        gammaParams[6] = 0.0;
        if (rtengine::settings->verbose) {         
            printf("ga0=%f ga1=%f ga2=%f ga3=%f ga4=%f\n", gammaParams[0], gammaParams[1], gammaParams[2], gammaParams[3], gammaParams[4]);
        }

        // 7 parameters for smoother curves

        Glib::ustring ills = "D50";

        if (locprim == 0  && mul == 5) {

            switch (ColorManagementParams::Illuminant(illum)) {
                case ColorManagementParams::Illuminant::DEFAULT:
                case ColorManagementParams::Illuminant::STDA:
                case ColorManagementParams::Illuminant::TUNGSTEN_2000K:
                case ColorManagementParams::Illuminant::TUNGSTEN_1500K:
                case ColorManagementParams::Illuminant::E:{
                    break;
                }

                case ColorManagementParams::Illuminant::D41: {
                    tempv4 = 4100.;
                    ills = "D41";
                    break;
                }

                case ColorManagementParams::Illuminant::D50: {
                    tempv4 = 5003.;
                    ills = "D50";
                    break;
                }

                case ColorManagementParams::Illuminant::D55: {
                    tempv4 = 5500.;
                    ills = "D55";
                    break;
                }

                case ColorManagementParams::Illuminant::D60: {
                    tempv4 = 6004.;
                    ills = "D60";
                    break;
                }

                case ColorManagementParams::Illuminant::D65: {
                    tempv4 = 6504.;
                    ills = "D65";
                    break;
                }

                case ColorManagementParams::Illuminant::D80: {
                    tempv4 = 8000.;
                    ills = "D80";
                    break;
                }

                case ColorManagementParams::Illuminant::D120: {
                    tempv4 = 12000.;
                    ills = "D120";
                    break;
                }
            }

            cmsWhitePointFromTemp(&xyD, tempv4);

            switch (ColorManagementParams::Illuminant(illum)) {
                case ColorManagementParams::Illuminant::DEFAULT: {
                    break;
                }

                case ColorManagementParams::Illuminant::D55: {
                    Wx = 0.956565934;
                    Wz = 0.920253249;
                    break;
                }

                case ColorManagementParams::Illuminant::D80: {
                    Wx = 0.950095542;
                    Wz = 1.284213976;
                    break;
                }

                case ColorManagementParams::Illuminant::D41: {
                    Wx = 0.991488263;
                    Wz = 0.631604625;
                    break;
                }

                case ColorManagementParams::Illuminant::D50: {
                    xyD = {0.3457, 0.3585, 1.0}; // near LCMS values but not perfect... it's a compromise!!
                    Wx = 0.964295676;
                    Wz = 0.825104603;
                    break;
                }

                case ColorManagementParams::Illuminant::D60: {
                    Wx = 0.952646075;
                    Wz = 1.008825184;
                    xyD = {0.32168, 0.33767, 1.0};
                    break;
                }

                case ColorManagementParams::Illuminant::D65: {
                    Wx = 0.95045471;
                    Wz = 1.08905029;
                    xyD = {0.312700492, 0.329000939, 1.0};
                    break;
                }

                case ColorManagementParams::Illuminant::D120: {
                    Wx = 0.979182;
                    Wz = 1.623623;
                    xyD = {0.269669, 0.28078, 1.0};
                    break;
                }

                case ColorManagementParams::Illuminant::STDA: {
                    Wx = 1.098500393;
                    Wz = 0.355848714;
                    xyD = {0.447573, 0.407440, 1.0};
                    ills = "stdA 2875K";
                    break;
                }

                case ColorManagementParams::Illuminant::TUNGSTEN_2000K: {
                    Wx = 1.274335;
                    Wz = 0.145233;
                    xyD = {0.526591, 0.41331, 1.0};
                    ills = "Tungsten 2000K";
                    break;
                }

                case ColorManagementParams::Illuminant::TUNGSTEN_1500K: {
                    Wx = 1.489921;
                    Wz = 0.053826;
                    xyD = {0.585703, 0.393157, 1.0};
                    ills = "Tungsten 1500K";
                    break;
                }

                case ColorManagementParams::Illuminant::E: {
                    Wx = 1.;
                    Wz = 1.;
                    xyD = {0.33333, 0.33333, 1.0};
                    ills = "E";
                    break;
                }

            }
        }
        if (profile == "Custompol") {//rotation and saturation primaries only with Abstract Profile
            float primaries[3][2];
            primaries[0][0] = p[0];
            primaries[0][1] = p[1];
            primaries[1][0] = p[2];
            primaries[1][1] = p[3];
            primaries[2][0] = p[4];
            primaries[2][1] = p[5];
            const float r_inset = params->icm.redsat;
            const float r_rotation = params->icm.redrot;
            const float g_inset = params->icm.gresat;
            const float g_rotation = params->icm.grerot;
            const float b_inset =  params->icm.blusat;
            const float b_rotation = params->icm.blurot;
            const float inset[3] = { r_inset / 100, g_inset / 100, b_inset / 100 };
            constexpr float rad = RT_PI / 180.0;
            const float rotation[3] = { r_rotation * rad, g_rotation * rad, b_rotation * rad };
            float newprimxy[2];
            for (int i = 0; i < 3; i++) { 
                newprimxy[0] = 0.f;
                newprimxy[1] = 0.f;
                rotate_and_scale_primary(primaries, 1.f - inset[i], rotation[i], i , newprimxy, xyD);//xyD takes into account the default illuminant or the one chosen by the user
                if (i == 0) {
                    p[0] = newprimxy[0];
                    p[1] = newprimxy[1];
                    if(rtengine::settings->verbose) {
                        fmt::println("newRx_prim={} newRy_prim={}", newprimxy[0], newprimxy[1]);
                    }
                } else if (i == 1) {
                    p[2] = newprimxy[0];
                    p[3] = newprimxy[1];
                    if(rtengine::settings->verbose) {
                        fmt::println("newGx_prim={} newGy_prim={}", newprimxy[0], newprimxy[1]);
                    }
                } else if (i == 2) {
                    p[4] = newprimxy[0];
                    p[5] = newprimxy[1];
                    if(rtengine::settings->verbose) {
                        fmt::println("newBx_prim={} newBy_prim={}", newprimxy[0], newprimxy[1]);
                    }
                }
            }
        }

        //xyD
        //meanx, meany
        // adjust refinement (purity) with a simple algorithm
        if (mul == 5) {
            double refin = 0.;

            if (locprim == 1) {
                refin = params->locallab.spots.at(sp).refi;
                meanx += params->locallab.spots.at(sp).shiftxl;
                meany += params->locallab.spots.at(sp).shiftyl;
            } else if (locprim == 0) {
                refin = params->icm.refi;
                meanx += params->icm.shiftx;
                meany += params->icm.shifty;
            }

            double arefi = (xyD.y - meany) / (xyD.x - meanx);
            double brefi = xyD.y - arefi * xyD.x;
            double scalrefi = 0.98 * (meanx - xyD.x);
            xyD.x = xyD.x + scalrefi * refin;
            xyD.y = xyD.x * arefi + brefi;
            // recalculate Wx Wy
            Wx = xyD.x / xyD.y;
            Wz = (1. - xyD.x - xyD.y) / xyD.y;
        }

        double wprofpri[9];

        //xyz in function primaries and illuminant
        Color::primaries_to_xyz(p, Wx, Wz, wprofpri, cat);

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                wprofprim[i][j] = (double) wprofpri[j * 3 + i];
                //xyz in TMatrix format
            }
        }
        //White point xy
        //D41  0.377984  0.381229
        //D55  0.332424  0.347426
        //D80  0.293755  0.309185
        //D75  0.299021  0.314852
        cmsToneCurve* GammaTRC[3];
        GammaTRC[0] = GammaTRC[1] = GammaTRC[2] = cmsBuildParametricToneCurve(NULL, five, gammaParams);//5 = more smoother than 4
        cmsHPROFILE oprofdef = nullptr;

        const cmsCIExyYTRIPLE Primaries = {
            {p[0], p[1], 1.0}, // red
            {p[2], p[3], 1.0}, // green
            {p[4], p[5], 1.0}  // blue
        };
        oprofdef = cmsCreateRGBProfile(&xyD, &Primaries, GammaTRC);
        cmsWriteTag(oprofdef, cmsSigRedTRCTag, GammaTRC[0]);
        cmsWriteTag(oprofdef, cmsSigGreenTRCTag, GammaTRC[1]);
        cmsWriteTag(oprofdef, cmsSigBlueTRCTag, GammaTRC[2]);

        //to read XYZ values and illuminant
        if (rtengine::settings->verbose  && mul == 5) {
            cmsCIEXYZ *redT = static_cast<cmsCIEXYZ*>(cmsReadTag(oprofdef, cmsSigRedMatrixColumnTag));
            cmsCIEXYZ *greenT  = static_cast<cmsCIEXYZ*>(cmsReadTag(oprofdef, cmsSigGreenMatrixColumnTag));
            cmsCIEXYZ *blueT  = static_cast<cmsCIEXYZ*>(cmsReadTag(oprofdef, cmsSigBlueMatrixColumnTag));

            if (locprim == 0) {
                printf("Illuminant=%s\n", ills.c_str());
            } else {
                printf("Illuminant=%s\n", params->locallab.spots.at(sp).illMethod.c_str());
            }

            printf("rX=%f gX=%f bX=%f\n", redT->X, greenT->X, blueT->X);
            printf("rY=%f gY=%f bY=%f\n", redT->Y, greenT->Y, blueT->Y);
            printf("rZ=%f gZ=%f bZ=%f\n", redT->Z, greenT->Z, blueT->Z);
        }

        cmsFreeToneCurve(GammaTRC[0]);

        if (oprofdef) {
            constexpr cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE | cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_GAMUTCHECK;
            const cmsHPROFILE iprof = ICCStore::getInstance()->getXYZProfile();
            lcmsMutex->lock();
            hTransform = cmsCreateTransform(iprof, TYPE_RGB_FLT, oprofdef, TYPE_RGB_FLT, params->icm.aRendIntent, flags);
            lcmsMutex->unlock();
        }
    }

    if (hTransform) {


#ifdef _OPENMP
        #pragma omp parallel if (multiThread)
#endif
        {
            AlignedBuffer<float> pBuf(cw * 3);
            const float normalize = normalizeOut ? 65535.f : 1.f;

#ifdef _OPENMP
            #pragma omp for schedule(dynamic, 16) nowait
#endif

            for (int i = 0; i < ch; ++i)
            {
                float *p = pBuf.data;

                for (int j = 0; j < cw; ++j) {
                    const float r = src->r(i, j);
                    const float g = src->g(i, j);
                    const float b = src->b(i, j);
                    float X = toxyz[0][0] * r + toxyz[0][1] * g + toxyz[0][2] * b;
                    float Y = toxyz[1][0] * r + toxyz[1][1] * g + toxyz[1][2] * b;
                    float Z = toxyz[2][0] * r + toxyz[2][1] * g + toxyz[2][2] * b;

                    if (gamutcontrol) {
                        Color::gamutmap(X, Y, Z, wprofprim);//gamut control
                    }

                    *(p++) = X;
                    *(p++) = Y;
                    *(p++) = Z;
                }

                p = pBuf.data;
                cmsDoTransform(hTransform, p, p, cw);

                for (int j = 0; j < cw; ++j) {
                    dst->r(i, j) = *(p++) * normalize;
                    dst->g(i, j) = *(p++) * normalize;
                    dst->b(i, j) = *(p++) * normalize;
                }
            }
        }

// alternative to find dominant color xy
// Not use :
//  1) GUI complex at least for me
//  2) small difference for meanxe, meanye with meanx , meany above in most cases
        /*
                if (locprim == 1) {
                    meanxe = 0.f;
                    meanye = 0.f;

        #ifdef _OPENMP
                    #pragma omp parallel for reduction(+:meanxe, meanye) if(multiThread)
        #endif
                    for (int y = 0; y < ch ; ++y) {
                        for (int x = 0; x < cw ; ++x) {
                            const float RR = dst->r(y,x);
                            const float GG = dst->g(y,x);
                            const float BB = dst->b(y,x);
                            float xcb, ycb, zcb;
                            Color::rgbxyz(RR, GG, BB, xcb, ycb, zcb, wb2);//use sRGB Adobe Rec2020 ACESp0

                            float X_r = xcb;
                            float Y_r = ycb;
                            float Z_r = zcb;
                            Color::gamutmap(X_r, Y_r, Z_r, wb2);//gamut control
                            const float som = X_r + Y_r + Z_r;
                            X_r = X_r / som;
                            Y_r = Y_r / som;
                            meanxe += X_r;
                            meanye += Y_r;
                        }
                    }
                    meanxe /= (ch*cw);
                    meanye /= (ch*cw);
                    printf("DiffmeanxE=%f DiffmeanyE=%f \n", (double) (meanxe - meanx), (double) (meanye - meany));
                }
        */
        if (!keepTransForm) {
            cmsDeleteTransform(hTransform);
            hTransform = nullptr;
        }

        transform = hTransform;
    }
}


}
