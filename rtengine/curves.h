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
#pragma once

#include <map>
#include <string>
#include <vector>

#include "rt_math.h"
#include "flatcurvetypes.h"
#include "diagonalcurvetypes.h"
#include "noncopyable.h"
#include "LUT.h"
#include "sleef.h"
#define CURVES_MIN_POLY_POINTS  1000

#define CLIPI(a) ((a)>0?((a)<65534?(a):65534):0)

namespace Glib
{

class ustring;

}

namespace rtengine
{

class ToneCurve;
class ColorAppearance;

inline void setUnlessOOG(float &r, float &g, float &b, const float &rr, const float &gg, const float &bb)
{
    if (!OOG(r) || !OOG(g) || !OOG(b)) {
        r = rr;
        g = gg;
        b = bb;
    }
}

#if defined(__SSE2__) || defined(RT_SIMDE)
inline vmask OOG(const vfloat val)
{
    return vorm(vmaskf_lt(val, ZEROV), vmaskf_gt(val, F2V(65535.f)));
}


inline void setUnlessOOG(vfloat &r, vfloat &g, vfloat &b, const vfloat rr, const vfloat gg, const vfloat bb)
{
    vmask cond = vandm(vandm(OOG(r), OOG(g)), OOG(b));
    r = vself(cond, r, rr);
    g = vself(cond, g, gg);
    b = vself(cond, b, bb);
}
#endif

bool sanitizeCurve(std::vector<double>& curve);

namespace curves
{

inline void setLutVal(const LUTf &lut, float &val)
{
    if (!OOG(val)) {
        val = lut[std::max(val, 0.f)];
    } else if (val < 0.f) {
        float m = lut[0.f];
        val += m;
    } else {
        float m = lut[MAXVALF];
        val += (m - MAXVALF);
    }
}


inline void setLutVal(const LUTf &lut, float &rval, float &gval, float &bval)
{
    if (!OOG(rval) || !OOG(gval) || !OOG(bval)) {
        rval = lut[std::max(rval, 0.f)];
        gval = lut[std::max(gval, 0.f)];
        bval = lut[std::max(bval, 0.f)];
    } else {
        setLutVal(lut, rval);
        setLutVal(lut, gval);
        setLutVal(lut, bval);
    }
}


inline void setLutVal(float &val, float lutval, float maxval)
{
    if (!OOG(val)) {
        val = lutval;
    } else if (val > 0.f) {
        val += maxval - MAXVALF;
    }
}


} // namespace curves

class CurveFactory
{

    friend class Curve;

protected:

    // functions calculating the parameters of the contrast curve based on the desired slope at the center
    static double solve_upper(double m, double c, double deriv);
    static double solve_lower(double m, double c, double deriv);
    static double dupper(const double b, const double m, const double c);
    static double dlower(const double b, const double m, const double c);

    // basic convex function between (0,0) and (1,1). m1 and m2 controls the slope at the start and end point
    static inline double basel(double x, double m1, double m2)
    {
        if (x == 0.0) {
            return 0.0;
        }

        double k = sqrt((m1 - 1.0) * (m1 - m2) * 0.5) / (1.0 - m2);
        double l = (m1 - m2) / (1.0 - m2) + k;
        double lx = xlog(x);
        return m2 * x + (1.0 - m2) * (2.0 - xexp(k * lx)) * xexp(l * lx);
    }
    static inline double basel_alt(double x)
    {
        return (2.0 - x) * x * x * x;
    }
    // basic concave function between (0,0) and (1,1). m1 and m2 controls the slope at the start and end point
    static inline double baseu(double x, double m1, double m2)
    {
        return 1.0 - basel(1.0 - x, m1, m2);
    }
    static inline double baseu_alt(double x)
    {
        return x * (2.0 + (x - 2.0) * x * x);
    }
    // convex curve between (0,0) and (1,1) with slope m at (0,0). hr controls the highlight recovery
    static inline double cupper(double x, double m, double hr)
    {
        if (hr > 1.0) {
            return baseu(x, m, 2.0 * (hr - 1.0) / m);
        }

        double x1 = (1.0 - hr) / m;
        double x2 = x1 + hr;

        if (x >= x2) {
            return 1.0;
        }

        if (x < x1) {
            return x * m;
        }

        return 1.0 - hr + hr * baseu((x - x1) / hr, m, 0);
    }
    // concave curve between (0,0) and (1,1) with slope m at (1,1). sr controls the shadow recovery
    static inline double clower(double x, double m, double sr)
    {
        return 1.0 - cupper(1.0 - x, m, sr);
    }
    // convex curve between (0,0) and (1,1) with slope m at (0,0). hr controls the highlight recovery
    static inline double cupper2(double x, double m, double hr)
    {
        double x1 = (1.0 - hr) / m;
        double x2 = x1 + hr;

        if (x >= x2) {
            return 1.0;
        }

        if (x < x1) {
            return x * m;
        }

        return 1.0 - hr + hr * baseu((x - x1) / hr, m, 0.3 * hr);
    }
    static inline double clower2(double x, double m, double sr)
    {
        //curve for b<0; starts with positive slope and then rolls over toward straight line to x=y=1
        double x1 = sr / 1.5 + 0.00001;

        if (x > x1 || sr < 0.001) {
            return 1 - (1 - x) * m;
        } else {
            double y1 = 1 - (1 - x1) * m;
            return y1 + m * (x - x1) - (1 - m) * SQR(SQR(1 - x / x1));
        }
    }
    // tone curve base. a: slope (from exp.comp.), b: black point normalized by 65535,
    // D: max. x value (can be>1), hr,sr: highlight,shadow recovery
    static inline double basecurve(double x, double a, double b, double D, double hr, double sr)
    {
        if (b < 0) {
            double m = 0.5;//midpoint
            double slope = 1.0 + b; //slope of straight line between (0,-b) and (1,1)
            double y = -b + m * slope; //value at midpoint

            if (x > m) {
                return y + (x - m) * slope;    //value on straight line between (m,y) and (1,1)
            } else {
                return y * clower2(x / m, slope * m / y, 2.0 - sr);
            }
        } else {
            double slope = a / (1.0 - b);
            double m = a * D > 1.0 ? b / a + (0.25) / slope : b + (1 - b) / 4;
            double y = a * D > 1.0 ? 0.25 : (m - b / a) * slope;

            if (x <= m) {
                return b == 0 ? x * slope : clower(x / m, slope * m / y, sr) * y;
            } else if (a * D > 1.0) {
                return y + (1.0 - y) * cupper2((x - m) / (D - m), slope * (D - m) / (1.0 - y), hr);
            } else {
                return y + (x - m) * slope;
            }
        }
    }
    static inline double simplebasecurve(double x, double b, double sr)
    {
        // a = 1, D = 1, hr = 0 (unused for a = D = 1)
        if (b == 0.0) {
            return x;
        } else if (b < 0) {
            double m = 0.5;//midpoint
            double slope = 1.0 + b; //slope of straight line between (0,-b) and (1,1)
            double y = -b + m * slope; //value at midpoint

            if (x > m) {
                return y + (x - m) * slope;    //value on straight line between (m,y) and (1,1)
            } else {
                return y * clower2(x / m, slope * m / y, 2.0 - sr);
            }
        } else {
            double slope = 1.0 / (1.0 - b);
            double m = b + (1 - b) * 0.25;
            double y = (m - b) * slope;

            if (x <= m) {
                return clower(x / m, slope * m / y, sr) * y;
            } else {
                return y + (x - m) * slope;
            }
        }
    }


public:
    const static double sRGBGamma;  // standard average gamma
    const static double sRGBGammaCurve;  // 2.4 in the curve


    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // accurately determine value from integer array with float as index
    //linearly interpolate from ends of range if arg is out of bounds
    static inline float interp(int *array, float f)
    {
        int index = CLIPI(floor(f));
        float part = (float)((f) - index) * (float)(array[index + 1] - array[index]);
        return (float)array[index] + part;
    }
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // accurately determine value from float array with float as index
    //linearly interpolate from ends of range if arg is out of bounds
    static inline float flinterp(float *array, float f)
    {
        int index = CLIPI(floor(f));
        float part = ((f) - (float)index) * (array[index + 1] - array[index]);
        return array[index] + part;
    }
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    static inline double centercontrast(double x, double b, double m);

    // standard srgb gamma and its inverse
    static inline double gamma2(double x)
    {
        return x <= 0.00304 ? x * 12.92310 : 1.055 * exp(log(x) / sRGBGammaCurve) - 0.055;
    }
    static inline double igamma2(double x)
    {
        return x <= 0.03928 ? x / 12.92310 : exp(log((x + 0.055) / 1.055) * sRGBGammaCurve);
    }
    static inline float gamma2(float x)
    {
        return x <= 0.00304f ? x * 12.92310f : 1.055f * expf(logf(x) / static_cast<float>(sRGBGammaCurve)) - 0.055f;
    }
    static inline float igamma2(float x)
    {
        return x <= 0.03928f ? x / 12.92310f : expf(logf((x + 0.055f) / 1.055f) * static_cast<float>(sRGBGammaCurve));
    }
    // gamma function with adjustable parameters
    static inline double gamma(double x, double gamma, double start, double slope, double mul, double add)
    {
        return (x <= start ? x*slope : exp(log(x) / gamma) * mul - add);
    }
    static inline double igamma(double x, double gamma, double start, double slope, double mul, double add)
    {
        return (x <= start * slope ? x / slope : exp(log((x + add) / mul) * gamma));
    }
    static inline float gamma(float x, float gamma, float start, float slope, float mul, float add)
    {
        return (x <= start ? x*slope : xexpf(xlogf(x) / gamma) * mul - add);
    }
    static inline float igamma(float x, float gamma, float start, float slope, float mul, float add)
    {
        return (x <= start * slope ? x / slope : xexpf(xlogf((x + add) / mul) * gamma));
    }
#if defined(__SSE2__) || defined(RT_SIMDE)
    static inline vfloat igamma(vfloat x, vfloat gamma, vfloat start, vfloat slope, vfloat mul, vfloat add)
    {
#if !defined(__clang__)
        return (x <= start * slope ? x / slope : xexpf(xlogf((x + add) / mul) * gamma));
#else
        return vself(vmaskf_le(x, start * slope), x / slope, xexpf(xlogf((x + add) / mul) * gamma));
#endif
    }
#endif
    static inline float hlcurve(const float exp_scale, const float comp, const float hlrange, float level)
    {
        if (comp > 0.f) {
            float val = level + (hlrange - 65536.f);

            if (val == 0.0f) { // to avoid division by zero
                val = 0.000001f;
            }

            float Y = val * exp_scale / hlrange;
            Y *= comp;

            if(Y <= -1.f) { // to avoid log(<=0)
                Y = -.999999f;
            }

            float R = hlrange / (val * comp);
            return log1p(Y) * R;
        } else {
            return exp_scale;
        }
    }


public:
    static void complexCurve(double ecomp, double black, double hlcompr, double hlcomprthresh, double shcompr, double br, double contr,
                             const std::vector<double>& curvePoints, const std::vector<double>& curvePoints2,
                             const LUTu & histogram, LUTf & hlCurve, LUTf & shCurve, LUTf & outCurve, LUTu & outBeforeCCurveHistogram, ToneCurve & outToneCurve, ToneCurve & outToneCurve2,
                             int skip = 1);

    static void complexCurvelocal(double ecomp, double black, double hlcompr, double hlcomprthresh, double shcompr, double br, double cont, double lumare,
                                  LUTf & hlCurve, LUTf & shCurve, LUTf & outCurve, LUTf & lightCurveloc, float avg,
                                  int skip = 1);

    static void Curvelocalhl(double ecomp, double hlcompr, double hlcomprthresh, LUTf & hlCurve);

    static void curveBW(const std::vector<double>& curvePointsbw, const std::vector<double>& curvePointsbw2, const LUTu & histogrambw, LUTu & outBeforeCCurveHistogrambw,
                        ToneCurve & customToneCurvebw1, ToneCurve & customToneCurvebw2, int skip);

    static bool diagonalCurve2Lut(const std::vector<double>& curvePoints, LUTf& curve, int skip, const LUTu & histogram, LUTu & outBeforeCurveHistogram);
    static bool diagonalCurve2Lut(const std::vector<double>& curvePoints, LUTf& curve, int skip);

    static void complexsgnCurve(bool & autili,  bool & butili, bool & ccutili, bool & clcutili, const std::vector<double>& acurvePoints,
                                const std::vector<double>& bcurvePoints, const std::vector<double>& cccurvePoints, const std::vector<double>& lccurvePoints, LUTf & aoutCurve, LUTf & boutCurve, LUTf & satCurve, LUTf & lhskCurve,
                                int skip = 1);

    static void updatechroma(
        const std::vector<double>& cccurvePoints,
        LUTu & histogramC, LUTu & outBeforeCCurveHistogramC,//for chroma
        int skip = 1);
    static void complexLCurve(double br, double contr, const std::vector<double>& curvePoints, const LUTu & histogram, LUTf & outCurve, LUTu & outBeforeCCurveHistogram, int skip, bool & utili);

    static void curveLightBrightColor(
        const std::vector<double>& curvePoints,
        const std::vector<double>& curvePointsred,
        const std::vector<double>& curvePointsgreen,
        const std::vector<double>& curvePointsblue,
        const std::vector<double>& curvePoints2,
        const std::vector<double>& curvePoints3,
        const LUTu & histogram, LUTu & outBeforeCCurveHistogram,
        const LUTu & histogramC, LUTu & outBeforeCCurveHistogramC,
        ColorAppearance & outColCurve1,
        ColorAppearance & outColCurvered,
        ColorAppearance & outColCurvegreen,
        ColorAppearance & outColCurveblue,
        ColorAppearance & outColCurve2,
        ColorAppearance & outColCurve3,
        int skip = 1);
    static void RGBCurve(const std::vector<double>& curvePoints, LUTf & outCurve, int skip);

};

class Curve
{

    class HashEntry
    {
    public:
        unsigned short smallerValue;
        unsigned short higherValue;
    };
protected:
    int N;
    int ppn;            // targeted polyline point number
    double* x;
    double* y;
    // begin of variables used in Parametric curves only
    double mc;
    double mfc;
    double msc;
    double mhc;
    // end of variables used in Parametric curves only
    std::vector<double> poly_x;     // X points of the faceted curve
    std::vector<double> poly_y;     // Y points of the faceted curve
    std::vector<double> dyByDx;
    std::vector<HashEntry> hash;
    unsigned short hashSize;        // hash table's size, between [10, 100, 1000]

    double* ypp;

    // Fields for the elementary curve polygonisation
    double x1, y1, x2, y2, x3, y3;
    bool firstPointIncluded;
    double increment;
    int nbr_points;

    static inline double p00(double x, double prot)
    {
        return CurveFactory::clower(x, 2.0, prot);
    }
    static inline double p11(double x, double prot)
    {
        return CurveFactory::cupper(x, 2.0, prot);
    }
    static inline double p01(double x, double prot)
    {
        return x <= 0.5 ? CurveFactory::clower(x * 2, 2.0, prot) * 0.5 : 0.5 + CurveFactory::cupper((x - 0.5) * 2, 2.0, prot) * 0.5;
    }
    static inline double p10(double x, double prot)
    {
        return x <= 0.5 ? CurveFactory::cupper(x * 2, 2.0, prot) * 0.5 : 0.5 + CurveFactory::clower((x - 0.5) * 2, 2.0, prot) * 0.5;
    }
    static inline double pfull(double x, double prot, double sh, double hl)
    {
        return (1 - sh) * (1 - hl) * p00(x, prot) + sh * hl * p11(x, prot) + (1 - sh) * hl * p01(x, prot) + sh * (1 - hl) * p10(x, prot);
    }
    static inline double pfull_alt(double x, double sh, double hl)
    {
        double t = (1.0 - sh) * (1.0 - hl) * CurveFactory::basel_alt(x) + sh * hl * CurveFactory::baseu_alt(x);
        return x <= 0.5
            ? t + (1.0 - sh) * hl * CurveFactory::basel_alt(2.0 * x) * 0.5 + sh * (1.0 - hl) * CurveFactory::baseu_alt(2.0 * x) * 0.5
            : t + (1.0 - sh) * hl * (0.5 + CurveFactory::baseu_alt(2.0 * x - 1.0) * 0.5) + sh * (1.0 - hl) * (0.5 + CurveFactory::basel_alt(2.0 * x - 1.0) * 0.5);
    }

    void fillHash();
    void fillDyByDx();

public:
    Curve();
    virtual ~Curve() {};
    void AddPolygons();
    int getSize() const;  // return the number of control points
    void getControlPoint(int cpNum, double &x, double &y) const;
    virtual double getVal(double t) const = 0;
    virtual void   getVal(const std::vector<double>& t, std::vector<double>& res) const = 0;

    virtual bool   isIdentity() const = 0;
};

class DiagonalCurve final : public Curve
{

protected:
    DiagonalCurveType kind;

    void spline_cubic_set();
    void catmull_rom_set();
    void NURBS_set();

public:
    explicit DiagonalCurve(const std::vector<double>& points, int ppn = CURVES_MIN_POLY_POINTS);
    ~DiagonalCurve() override;

    double getVal(double t) const override;
    void   getVal(const std::vector<double>& t, std::vector<double>& res) const override;
    bool   isIdentity() const override
    {
        return kind == DCT_Empty;
    };
};

class FlatCurve final : public Curve
{

private:
    FlatCurveType kind;
    double* leftTangent;
    double* rightTangent;
    double identityValue;
    bool periodic;

    void CtrlPoints_set();

public:

    explicit FlatCurve(const std::vector<double>& points, bool isPeriodic = true, int ppn = CURVES_MIN_POLY_POINTS);
    ~FlatCurve() override;

    double getVal(double t) const override;
    void   getVal(const std::vector<double>& t, std::vector<double>& res) const override;
    bool   setIdentityValue(double iVal);
    bool   isIdentity() const override
    {
        return kind == FCT_Empty;
    };
};

class RetinextransmissionCurve
{
private:
    LUTf luttransmission;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    virtual ~RetinextransmissionCurve() {};
    RetinextransmissionCurve();

    void Reset();
    void Set(const Curve *pCurve);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return luttransmission[index];
    }

    operator bool (void) const
    {
        return luttransmission;
    }
};

class RetinexgaintransmissionCurve
{
private:
    LUTf lutgaintransmission;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    virtual ~RetinexgaintransmissionCurve() {};
    RetinexgaintransmissionCurve();

    void Reset();
    void Set(const Curve *pCurve);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutgaintransmission[index];
    }

    operator bool (void) const
    {
        return lutgaintransmission;
    }
};



class ToneCurve
{
public:
    LUTf lutToneCurve;  // 0xffff range

    virtual ~ToneCurve() {};

    void Reset();
    void Set(const Curve &pCurve, float gamma = 0);
    operator bool (void) const
    {
        return lutToneCurve;
    }
};

class OpacityCurve
{
public:
    LUTf lutOpacityCurve;  // 0xffff range

    virtual ~OpacityCurve() {};

    void Reset();
    void Set(const Curve *pCurve);
    void Set(const std::vector<double> &curvePoints, bool &opautili);

    // TODO: transfer this method to the Color class...
    float blend(float x, float lower, float upper) const
    {
        return (upper - lower) * lutOpacityCurve[x * 500.f] + lower;
    }
    void blend3f(float x, float lower1, float upper1, float &result1, float lower2, float upper2, float &result2, float lower3, float upper3, float &result3) const
    {
        float opacity = lutOpacityCurve[x * 500.f];
        result1 = (upper1 - lower1) * opacity + lower1;
        result2 = (upper2 - lower2) * opacity + lower2;
        result3 = (upper3 - lower3) * opacity + lower3;
    }

    operator bool (void) const
    {
        return lutOpacityCurve;
    }
};

class LocLHCurve
{
private:
    LUTf lutLocLHCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLHCurve() {};
    LocLHCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLHCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLHCurve;
    }
};

class LocHHmaskblCurve
{
private:
    LUTf lutLocHHmaskblCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmaskblCurve() {};
    LocHHmaskblCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & lhmasblutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmaskblCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmaskblCurve;
    }
};

class LocCCmaskblCurve
{
private:
    LUTf lutLocCCmaskblCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmaskblCurve() {};
    LocCCmaskblCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints,  bool & lcmasblutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmaskblCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmaskblCurve;
    }
};

class LocLLmaskblCurve
{
private:
    LUTf lutLocLLmaskblCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmaskblCurve() {};
    LocLLmaskblCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & llmasblutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmaskblCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmaskblCurve;
    }
};



class LocHHmasktmCurve
{
private:
    LUTf lutLocHHmasktmCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmasktmCurve() {};
    LocHHmasktmCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & lhmastmutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmasktmCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmasktmCurve;
    }
};


class LocCCmasktmCurve
{
private:
    LUTf lutLocCCmasktmCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmasktmCurve() {};
    LocCCmasktmCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints,  bool & lcmastmutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmasktmCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmasktmCurve;
    }
};

class LocLLmasktmCurve
{
private:
    LUTf lutLocLLmasktmCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmasktmCurve() {};
    LocLLmasktmCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & llmastmutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmasktmCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmasktmCurve;
    }
};



class LocHHmaskretiCurve
{
private:
    LUTf lutLocHHmaskretiCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmaskretiCurve() {};
    LocHHmaskretiCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & lhmasretiutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmaskretiCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmaskretiCurve;
    }
};


class LocCCmaskretiCurve
{
private:
    LUTf lutLocCCmaskretiCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmaskretiCurve() {};
    LocCCmaskretiCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints,  bool & lcmasretiutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmaskretiCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmaskretiCurve;
    }
};

class LocLLmaskretiCurve
{
private:
    LUTf lutLocLLmaskretiCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmaskretiCurve() {};
    LocLLmaskretiCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & llmasretiutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmaskretiCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmaskretiCurve;
    }
};





class LocHHmaskcbCurve
{
private:
    LUTf lutLocHHmaskcbCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmaskcbCurve() {};
    LocHHmaskcbCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & lhmascbutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmaskcbCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmaskcbCurve;
    }
};


class LocCCmaskcbCurve
{
private:
    LUTf lutLocCCmaskcbCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmaskcbCurve() {};
    LocCCmaskcbCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints,  bool & lcmascbutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmaskcbCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmaskcbCurve;
    }
};

class LocLLmaskcbCurve
{
private:
    LUTf lutLocLLmaskcbCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmaskcbCurve() {};
    LocLLmaskcbCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & llmascbutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmaskcbCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmaskcbCurve;
    }
};




class LocHHmaskexpCurve
{
private:
    LUTf lutLocHHmaskexpCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmaskexpCurve() {};
    LocHHmaskexpCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & lhmasexputili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmaskexpCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmaskexpCurve;
    }
};


class LocCCmaskexpCurve
{
private:
    LUTf lutLocCCmaskexpCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmaskexpCurve() {};
    LocCCmaskexpCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints,  bool & lcmasexputili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmaskexpCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmaskexpCurve;
    }
};

class LocLLmaskexpCurve
{
private:
    LUTf lutLocLLmaskexpCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmaskexpCurve() {};
    LocLLmaskexpCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & llmasexputili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmaskexpCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmaskexpCurve;
    }
};


class LocHHmaskSHCurve
{
private:
    LUTf lutLocHHmaskSHCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmaskSHCurve() {};
    LocHHmaskSHCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & lhmasSHutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmaskSHCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmaskSHCurve;
    }
};


class LocCCmaskSHCurve
{
private:
    LUTf lutLocCCmaskSHCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmaskSHCurve() {};
    LocCCmaskSHCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints,  bool & lcmasSHutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmaskSHCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmaskSHCurve;
    }
};

class LocLLmaskSHCurve
{
private:
    LUTf lutLocLLmaskSHCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmaskSHCurve() {};
    LocLLmaskSHCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints, bool & llmasSHutili);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmaskSHCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmaskSHCurve;
    }
};




class LocHHmaskCurve
{
private:
    LUTf lutLocHHmaskCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHmaskCurve() {};
    LocHHmaskCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHmaskCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHmaskCurve;
    }
};


class LocCCmaskCurve
{
private:
    LUTf lutLocCCmaskCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCCmaskCurve() {};
    LocCCmaskCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCCmaskCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCCmaskCurve;
    }
};

class LocLLmaskCurve
{
private:
    LUTf lutLocLLmaskCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocLLmaskCurve() {};
    LocLLmaskCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocLLmaskCurve[index];
    }
    operator bool (void) const
    {
        return lutLocLLmaskCurve;
    }
};


class LocHHCurve
{
private:
    LUTf lutLocHHCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocHHCurve() {};
    LocHHCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocHHCurve[index];
    }
    operator bool (void) const
    {
        return lutLocHHCurve;
    }
};


class LocCHCurve
{
private:
    LUTf lutLocCHCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocCHCurve() {};
    LocCHCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocCHCurve[index];
    }
    operator bool (void) const
    {
        return lutLocCHCurve;
    }
};

class LocretigainCurve
{
private:
    LUTf lutLocretigainCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocretigainCurve() {};
    LocretigainCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocretigainCurve[index];
    }
    operator bool (void) const
    {
        return lutLocretigainCurve;
    }
};

class LocretitransCurve
{
private:
    LUTf lutLocretitransCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocretitransCurve() {};
    LocretitransCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocretitransCurve[index];
    }
    operator bool (void) const
    {
        return lutLocretitransCurve;
    }
};


class LocwavCurve
{
private:
    LUTf lutLocwavCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocwavCurve() {};
    LocwavCurve();
    void Reset();
    bool Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocwavCurve[index];
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat operator[](vfloat index) const
    {
        return lutLocwavCurve[index];
    }
#endif

    operator bool (void) const
    {
        return lutLocwavCurve;
    }
};


class LocretigainCurverab
{
private:
    LUTf lutLocretigainCurverab;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~LocretigainCurverab() {};
    LocretigainCurverab();
    void Reset();
    void Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutLocretigainCurverab[index];
    }
    operator bool (void) const
    {
        return lutLocretigainCurverab;
    }
};


class WavCurve
{
private:
    LUTf lutWavCurve;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    float sum;

    virtual ~WavCurve() {};
    WavCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints);
    float getSum() const
    {
        return sum;
    }

    float operator[](float index) const
    {
        return lutWavCurve[index];
    }
    operator bool (void) const
    {
        return lutWavCurve;
    }
};

class Wavblcurve
{
private:
    LUTf lutblcurve;  // 0xffff range
    void Set(const Curve &pCurve);
public:
    virtual ~Wavblcurve() {};
    Wavblcurve();

    void Reset();
    //  void Set(const std::vector<double> &curvePoints, bool &opautili);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutblcurve[index];
    }

    operator bool (void) const
    {
        return lutblcurve;
    }
};


class WavOpacityCurveRG
{
private:
    LUTf lutOpacityCurveRG;  // 0xffff range
    void Set(const Curve &pCurve);
public:
    virtual ~WavOpacityCurveRG() {};
    WavOpacityCurveRG();

    void Reset();
    //  void Set(const std::vector<double> &curvePoints, bool &opautili);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutOpacityCurveRG[index];
    }

    operator bool (void) const
    {
        return lutOpacityCurveRG;
    }
};

class WavOpacityCurveSH
{
private:
    LUTf lutOpacityCurveSH;  // 0xffff range
    void Set(const Curve &pCurve);
public:
    virtual ~WavOpacityCurveSH() {};
    WavOpacityCurveSH();

    void Reset();
    //  void Set(const std::vector<double> &curvePoints, bool &opautili);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutOpacityCurveSH[index];
    }

    operator bool (void) const
    {
        return lutOpacityCurveSH;
    }
};



class WavOpacityCurveBY
{
private:
    LUTf lutOpacityCurveBY;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    virtual ~WavOpacityCurveBY() {};
    WavOpacityCurveBY();

    void Reset();
    void Set(const Curve *pCurve);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutOpacityCurveBY[index];
    }

    operator bool (void) const
    {
        return lutOpacityCurveBY;
    }
};
class WavOpacityCurveW
{
private:
    LUTf lutOpacityCurveW;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    virtual ~WavOpacityCurveW() {};
    WavOpacityCurveW();

    void Reset();
    void Set(const Curve *pCurve);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutOpacityCurveW[index];
    }

    operator bool (void) const
    {
        return lutOpacityCurveW;
    }
};

class WavOpacityCurveWL
{
private:
    LUTf lutOpacityCurveWL;  // 0xffff range
    void Set(const Curve &pCurve);

public:
    virtual ~WavOpacityCurveWL() {};
    WavOpacityCurveWL();

    void Reset();
    void Set(const Curve *pCurve);
    void Set(const std::vector<double> &curvePoints);
    float operator[](float index) const
    {
        return lutOpacityCurveWL[index];
    }

    operator bool (void) const
    {
        return lutOpacityCurveWL;
    }
};

class NoiseCurve
{
private:
    LUTf lutNoiseCurve;  // 0xffff range
    float sum;
    void Set(const Curve &pCurve);

public:
    virtual ~NoiseCurve() {};
    NoiseCurve();
    void Reset();
    void Set(const std::vector<double> &curvePoints);

    float getSum() const
    {
        return sum;
    }
    float operator[](float index) const
    {
        return lutNoiseCurve[index];
    }
    operator bool (void) const
    {
        return lutNoiseCurve;
    }
};

class ColorGradientCurve
{
public:
    LUTf   lut1;    // [0.;1.] range (float values)
    LUTf   lut2;  // [0.;1.] range (float values)
    LUTf   lut3;   // [0.;1.] range (float values)
    double low;
    double high;

    virtual ~ColorGradientCurve() {};

    void Reset();
    void SetXYZ(const Curve *pCurve, const double xyz_rgb[3][3], float satur, float lumin);
    void SetXYZ(const std::vector<double> &curvePoints, const double xyz_rgb[3][3], float satur, float lumin);
    void SetRGB(const Curve *pCurve);
    void SetRGB(const std::vector<double> &curvePoints);

    /**
    * @brief Get the value of Red, Green and Blue corresponding to the requested index
    * @param index value in the [0 ; 1] range
    * @param r corresponding red value [0 ; 65535] (return value)
    * @param g corresponding green value [0 ; 65535] (return value)
    * @param b corresponding blue value [0 ; 65535] (return value)
    */
    void getVal(float index, float &r, float &g, float &b) const;
    operator bool (void) const
    {
        return lut1 && lut2 && lut3;
    }
};

class ColorAppearance
{
public:
    LUTf lutColCurve;  // 0xffff range

    virtual ~ColorAppearance() {};

    void Reset();
    void Set(const Curve &pCurve);
    operator bool (void) const
    {
        return lutColCurve;
    }
};

class Lightcurve : public ColorAppearance
{
public:
    void Apply(float& Li) const;
};

//lightness curve
inline void Lightcurve::Apply(float& Li) const
{

    assert(lutColCurve);

    curves::setLutVal(lutColCurve, Li);
}

class Brightcurve : public ColorAppearance
{
public:
    void Apply(float& Br) const;
};

//brightness curve
inline void Brightcurve::Apply(float& Br) const
{

    assert(lutColCurve);

    curves::setLutVal(lutColCurve, Br);
}

class Chromacurve : public ColorAppearance
{
public:
    void Apply(float& Cr) const;
};

//Chroma curve
inline void Chromacurve::Apply(float& Cr) const
{

    assert(lutColCurve);

    curves::setLutVal(lutColCurve, Cr);
}
class Saturcurve : public ColorAppearance
{
public:
    void Apply(float& Sa) const;
};

//Saturation curve
inline void Saturcurve::Apply(float& Sa) const
{

    assert(lutColCurve);

    curves::setLutVal(lutColCurve, Sa);
}

class Colorfcurve : public ColorAppearance
{
public:
    void Apply(float& Cf) const;
};

//Colorfullness curve
inline void Colorfcurve::Apply(float& Cf) const
{

    assert(lutColCurve);

    curves::setLutVal(lutColCurve, Cf);
}


class StandardToneCurve : public ToneCurve
{
public:
    void Apply(float& r, float& g, float& b) const;

    // Applies the tone curve to `r`, `g`, `b` arrays, starting at `r[start]`
    // and ending at `r[end]` (and respectively for `b` and `g`). Uses SSE
    // and requires that `r`, `g`, and `b` pointers have the same alignment.
    void BatchApply(
        const size_t start, const size_t end,
        float *r, float *g, float *b) const;
};

class AdobeToneCurve : public ToneCurve
{
private:
    void RGBTone(float& r, float& g, float& b) const;  // helper for tone curve
#if defined(__SSE2__) || defined(RT_SIMDE)
    void RGBTone(vfloat& r, vfloat& g, vfloat& b) const;  // helper for tone curve
#endif
public:
    void Apply(float& r, float& g, float& b) const;
    void BatchApply(
            const size_t start, const size_t end,
            float *r, float *g, float *b) const;
};

class WeightedStdToneCurve : public ToneCurve
{
private:
    float Triangle(float refX, float refY, float X2) const;
#if defined(__SSE2__) || defined(RT_SIMDE)
    vfloat Triangle(vfloat refX, vfloat refY, vfloat X2) const;
#endif
public:
    void Apply(float& r, float& g, float& b) const;
    void BatchApply(const size_t start, const size_t end, float *r, float *g, float *b) const;
};

class LuminanceToneCurve : public ToneCurve
{
public:
    void Apply(float& r, float& g, float& b) const;
};

class PerceptualToneCurveState
{
public:
    float Working2Prophoto[3][3];
    float Prophoto2Working[3][3];
    float cmul_contrast;
    bool isProphoto;
};

// Tone curve whose purpose is to keep the color appearance constant, that is the curve changes contrast
// but colors appears to have the same hue and saturation as before. As contrast and saturation is tightly
// coupled in human vision saturation is modulated based on the curve's contrast, and that way the appearance
// can be kept perceptually constant (within limits).
class PerceptualToneCurve : public ToneCurve
{
private:
    static float cf_range[2];
    static float cf[1000];
    // for ciecam02
    static float f, c, nc, yb, la, xw, yw, zw;
    static float n, d, nbb, ncb, cz, aw, wh, pfl, fl, pow1;

    static void cubic_spline(const float x[], const float y[], const int len, const float out_x[], float out_y[], const int out_len);
    static float find_minimum_interval_halving(float (*func)(float x, void *arg), void *arg, float a, float b, float tol, int nmax);
    static float find_tc_slope_fun(float k, void *arg);
    static float get_curve_val(float x, float range[2], float lut[], size_t lut_size);
    float calculateToneCurveContrastValue() const;
public:
    static void init();
    void initApplyState(PerceptualToneCurveState & state, const Glib::ustring& workingSpace) const;
    void BatchApply(const size_t start, const size_t end, float *r, float *g, float *b, const PerceptualToneCurveState &state) const;
};

// Standard tone curve
inline void StandardToneCurve::Apply(float& r, float& g, float& b) const
{

    assert(lutToneCurve);

    curves::setLutVal(lutToneCurve, r, g, b);
}


inline void StandardToneCurve::BatchApply(
    const size_t start, const size_t end,
    float *r, float *g, float *b) const
{
    assert(lutToneCurve);
    assert(lutToneCurve.getClip() & LUT_CLIP_BELOW);
    assert(lutToneCurve.getClip() & LUT_CLIP_ABOVE);

    // All pointers must have the same alignment for SSE usage. In the loop body below,
    // we will only check `r`, assuming that the same result would hold for `g` and `b`.
    assert(reinterpret_cast<uintptr_t>(r) % 16 == reinterpret_cast<uintptr_t>(g) % 16);
    assert(reinterpret_cast<uintptr_t>(g) % 16 == reinterpret_cast<uintptr_t>(b) % 16);

    size_t i = start;

    while (true) {
        if (i >= end) {
            // If we get to the end before getting to an aligned address, just return.
            // (Or, for non-SSE mode, if we get to the end.)
            return;
#if defined(__SSE2__) || defined(RT_SIMDE)
        } else if (reinterpret_cast<uintptr_t>(&r[i]) % 16 == 0) {
            // Otherwise, we get to the first aligned address; go to the SSE part.
            break;
#endif
        }

        setUnlessOOG(r[i], g[i], b[i], lutToneCurve[r[i]], lutToneCurve[g[i]], lutToneCurve[b[i]]);
        i++;
    }

#if defined(__SSE2__) || defined(RT_SIMDE)

    for (; i + 3 < end; i += 4) {
        vfloat r_val = LVF(r[i]);
        vfloat g_val = LVF(g[i]);
        vfloat b_val = LVF(b[i]);
        setUnlessOOG(r_val, g_val, b_val, lutToneCurve[r_val], lutToneCurve[g_val], lutToneCurve[b_val]);
        STVF(r[i], r_val);
        STVF(g[i], g_val);
        STVF(b[i], b_val);
    }

    // Remainder in non-SSE.
    for (; i < end; ++i) {
        setUnlessOOG(r[i], g[i], b[i], lutToneCurve[r[i]], lutToneCurve[g[i]], lutToneCurve[b[i]]);
    }

#endif
}

// Tone curve according to Adobe's reference implementation
// values in 0xffff space
// inlined to make sure there will be no cache flush when used
inline void AdobeToneCurve::Apply(float& ir, float& ig, float& ib) const
{

    assert(lutToneCurve);
    float r = CLIP(ir);
    float g = CLIP(ig);
    float b = CLIP(ib);

    if (r >= g) {
        if (g > b) {
            RGBTone(r, g, b);     // Case 1: r >= g >  b
        } else if (b > r) {
            RGBTone(b, r, g);     // Case 2: b >  r >= g
        } else if (b > g) {
            RGBTone(r, b, g);     // Case 3: r >= b >  g
        } else {                           // Case 4: r == g == b
            r = lutToneCurve[r];
            g = lutToneCurve[g];
            b = g;
        }
    } else {
        if (r >= b) {
            RGBTone(g, r, b);     // Case 5: g >  r >= b
        } else if (b >  g) {
            RGBTone(b, g, r);     // Case 6: b >  g >  r
        } else {
            RGBTone(g, b, r);     // Case 7: g >= b >  r
        }
    }

    setUnlessOOG(ir, ig, ib, r, g, b);
}

inline void AdobeToneCurve::BatchApply(
        const size_t start, const size_t end,
        float *r, float *g, float *b) const {
    assert (lutToneCurve);
    assert (lutToneCurve.getClip() & LUT_CLIP_BELOW);
    assert (lutToneCurve.getClip() & LUT_CLIP_ABOVE);

    // All pointers must have the same alignment for SSE usage. In the loop body below,
    // we will only check `r`, assuming that the same result would hold for `g` and `b`.
    assert (reinterpret_cast<uintptr_t>(r) % 16 == reinterpret_cast<uintptr_t>(g) % 16);
    assert (reinterpret_cast<uintptr_t>(g) % 16 == reinterpret_cast<uintptr_t>(b) % 16);

    size_t i = start;
    while (true) {
        if (i >= end) {
            // If we get to the end before getting to an aligned address, just return.
            // (Or, for non-SSE mode, if we get to the end.)
            return;
#if defined(__SSE2__) || defined(RT_SIMDE)
        } else if (reinterpret_cast<uintptr_t>(&r[i]) % 16 == 0) {
            // Otherwise, we get to the first aligned address; go to the SSE part.
            break;
#endif
        }
        Apply(r[i], g[i], b[i]);
        i++;
    }
#if defined(__SSE2__) || defined(RT_SIMDE)
    const vfloat upperv = F2V(MAXVALF);
    for (; i + 3 < end; i += 4) {

        vfloat rc = vclampf(LVF(r[i]), ZEROV, upperv);
        vfloat gc = vclampf(LVF(g[i]), ZEROV, upperv);
        vfloat bc = vclampf(LVF(b[i]), ZEROV, upperv);

        vfloat minval = vminf(vminf(rc, gc), bc);
        vfloat maxval = vmaxf(vmaxf(rc, gc), bc);
        vfloat medval = vmaxf(vminf(rc, gc), vminf(bc, vmaxf(rc, gc)));

        const vfloat minvalold = minval;
        const vfloat maxvalold = maxval;

        RGBTone(maxval, medval, minval);

        const vfloat nr = vself(vmaskf_eq(rc, maxvalold), maxval, vself(vmaskf_eq(rc, minvalold), minval, medval));
        const vfloat ng = vself(vmaskf_eq(gc, maxvalold), maxval, vself(vmaskf_eq(gc, minvalold), minval, medval));
        const vfloat nb = vself(vmaskf_eq(bc, maxvalold), maxval, vself(vmaskf_eq(bc, minvalold), minval, medval));

        rc = LVF(r[i]);
        gc = LVF(g[i]);
        bc = LVF(b[i]);
        setUnlessOOG(rc, gc, bc, nr, ng, nb);
        STVF(r[i], rc);
        STVF(g[i], gc);
        STVF(b[i], bc);
    }
    // Remainder in non-SSE.
    for (; i < end; ++i) {
        Apply(r[i], g[i], b[i]);
    }
#endif
}

inline void AdobeToneCurve::RGBTone (float& maxval, float& medval, float& minval) const
{
    float minvalold = minval, medvalold = medval, maxvalold = maxval;

    maxval = lutToneCurve[maxvalold];
    minval = lutToneCurve[minvalold];
    medval = minval + ((maxval - minval) * (medvalold - minvalold) / (maxvalold - minvalold));
}
#if defined(__SSE2__) || defined(RT_SIMDE)
inline void AdobeToneCurve::RGBTone (vfloat& maxval, vfloat& medval, vfloat& minval) const
{
    const vfloat minvalold = minval, maxvalold = maxval;

    maxval = lutToneCurve[maxvalold];
    minval = lutToneCurve[minvalold];
    medval = minval + ((maxval - minval) * (medval - minvalold) / (maxvalold - minvalold));
    medval = vself(vmaskf_eq(minvalold, maxvalold), minval, medval);
}
#endif
// Modifying the Luminance channel only
inline void LuminanceToneCurve::Apply(float &ir, float &ig, float &ib) const
{
    assert(lutToneCurve);

    float r = CLIP(ir);
    float g = CLIP(ig);
    float b = CLIP(ib);

    float currLuminance = r * 0.2126729f + g * 0.7151521f + b * 0.0721750f;

    const float newLuminance = lutToneCurve[currLuminance];
    currLuminance = currLuminance == 0.f ? 0.00001f : currLuminance;
    const float coef = newLuminance / currLuminance;
    r = LIM<float> (r * coef, 0.f, 65535.f);
    g = LIM<float> (g * coef, 0.f, 65535.f);
    b = LIM<float> (b * coef, 0.f, 65535.f);

    setUnlessOOG(ir, ig, ib, r, g, b);
}

inline float WeightedStdToneCurve::Triangle(float a, float a1, float b) const
{
    if (a != b) {
        float b1;
        float a2 = a1 - a;

        if (b < a) {
            b1 = b + a2 *      b  /     a ;
        } else       {
            b1 = b + a2 * (65535.f - b) / (65535.f - a);
        }

        return b1;
    }

    return a1;
}

#if defined(__SSE2__) || defined(RT_SIMDE)
inline vfloat WeightedStdToneCurve::Triangle(vfloat a, vfloat a1, vfloat b) const
{
    vmask eqmask = vmaskf_eq(b, a);
    vfloat a2 = a1 - a;
    vmask cmask = vmaskf_lt(b, a);
    vfloat b3 = vself(cmask, b, F2V(65535.f) - b);
    vfloat a3 = vself(cmask, a, F2V(65535.f) - a);
    return vself(eqmask, a1, b + a2 * b3 / a3);
}
#endif

// Tone curve modifying the value channel only, preserving hue and saturation
// values in 0xffff space
inline void WeightedStdToneCurve::Apply(float& ir, float& ig, float& ib) const
{

    assert(lutToneCurve);

    float r = CLIP(ir);
    float g = CLIP(ig);
    float b = CLIP(ib);
    float r1 = lutToneCurve[r];
    float g1 = Triangle(r, r1, g);
    float b1 = Triangle(r, r1, b);

    float g2 = lutToneCurve[g];
    float r2 = Triangle(g, g2, r);
    float b2 = Triangle(g, g2, b);

    float b3 = lutToneCurve[b];
    float r3 = Triangle(b, b3, r);
    float g3 = Triangle(b, b3, g);

    r = CLIP<float>(r1 * 0.50f + r2 * 0.25f + r3 * 0.25f);
    g = CLIP<float> (g1 * 0.25f + g2 * 0.50f + g3 * 0.25f);
    b = CLIP<float> (b1 * 0.25f + b2 * 0.25f + b3 * 0.50f);

    setUnlessOOG(ir, ig, ib, r, g, b);
}

inline void WeightedStdToneCurve::BatchApply(const size_t start, const size_t end, float *r, float *g, float *b) const
{
    assert(lutToneCurve);
    assert(lutToneCurve.getClip() & LUT_CLIP_BELOW);
    assert(lutToneCurve.getClip() & LUT_CLIP_ABOVE);

    // All pointers must have the same alignment for SSE usage. In the loop body below,
    // we will only check `r`, assuming that the same result would hold for `g` and `b`.
    assert(reinterpret_cast<uintptr_t>(r) % 16 == reinterpret_cast<uintptr_t>(g) % 16);
    assert(reinterpret_cast<uintptr_t>(g) % 16 == reinterpret_cast<uintptr_t>(b) % 16);

    size_t i = start;

    while (true) {
        if (i >= end) {
            // If we get to the end before getting to an aligned address, just return.
            // (Or, for non-SSE mode, if we get to the end.)
            return;
#if defined(__SSE2__) || defined(RT_SIMDE)
        } else if (reinterpret_cast<uintptr_t>(&r[i]) % 16 == 0) {
            // Otherwise, we get to the first aligned address; go to the SSE part.
            break;
#endif
        }

        Apply(r[i], g[i], b[i]);
        i++;
    }

#if defined(__SSE2__) || defined(RT_SIMDE)
    const vfloat c65535v = F2V(65535.f);
    const vfloat zd5v = F2V(0.5f);
    const vfloat zd25v = F2V(0.25f);

    for (; i + 3 < end; i += 4) {
        vfloat r_val = vclampf(LVF(r[i]), ZEROV, c65535v);
        vfloat g_val = vclampf(LVF(g[i]), ZEROV, c65535v);
        vfloat b_val = vclampf(LVF(b[i]), ZEROV, c65535v);
        vfloat r1 = lutToneCurve[r_val];
        vfloat g1 = Triangle(r_val, r1, g_val);
        vfloat b1 = Triangle(r_val, r1, b_val);

        vfloat g2 = lutToneCurve[g_val];
        vfloat r2 = Triangle(g_val, g2, r_val);
        vfloat b2 = Triangle(g_val, g2, b_val);

        vfloat b3 = lutToneCurve[b_val];
        vfloat r3 = Triangle(b_val, b3, r_val);
        vfloat g3 = Triangle(b_val, b3, g_val);

        vfloat r_old = LVF(r[i]);
        vfloat g_old = LVF(g[i]);
        vfloat b_old = LVF(b[i]);
        vfloat r_new = vclampf(r1 * zd5v + r2 * zd25v + r3 * zd25v, ZEROV, c65535v);
        vfloat g_new = vclampf(g1 * zd25v + g2 * zd5v + g3 * zd25v, ZEROV, c65535v);
        vfloat b_new = vclampf(b1 * zd25v + b2 * zd25v + b3 * zd5v, ZEROV, c65535v);
        setUnlessOOG(r_old, g_old, b_old, r_new, g_new, b_new);
        STVF(r[i], r_old);
        STVF(g[i], g_old);
        STVF(b[i], b_old);
    }

    // Remainder in non-SSE.
    for (; i < end; ++i) {
        Apply(r[i], g[i], b[i]);
    }

#endif
}

}

#undef CLIPI
