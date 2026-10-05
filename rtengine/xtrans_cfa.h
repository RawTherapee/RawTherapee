/*
 * Shared canonicalization for Fujifilm's 6x6 X-Trans CFA.
 */
#pragma once

#include <cstdint>

namespace rtengine
{

// This is the paper coordinate system used by Rafinazari and Dubois (ICIP
// 2014), and by the dissertation's spectral derivation in Figure 3.1 and
// Equation 3.15.  Gharbi's pinned X-Trans mosaic.py uses the same cell, so a
// single reviewed transform can serve both demosaicers without changing RGB
// channel meaning.
constexpr int CANONICAL_XTRANS_CFA[6][6] = {
    {1, 2, 1, 1, 0, 1},
    {0, 1, 0, 2, 1, 2},
    {1, 2, 1, 1, 0, 1},
    {1, 0, 1, 1, 2, 1},
    {2, 1, 2, 0, 1, 0},
    {1, 0, 1, 1, 2, 1}
};

struct XTransCfaTransform final {
    int a;
    int b;
    int c;
    int d;
    int ox;
    int oy;
};

inline int positiveModulo(int value, int modulus)
{
    const int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

bool findCanonicalXTransTransform(
    const int actual[6][6],
    XTransCfaTransform &result);

bool findCanonicalXTransTransform(
    const int actual[6][6],
    const int canonical[6][6],
    XTransCfaTransform &result);

class XTransCfaView final
{
public:
    XTransCfaView(
        const XTransCfaTransform &transform,
        int actualWidth,
        int actualHeight);

    XTransCfaView(
        const XTransCfaTransform &transform,
        int actualWidth,
        int actualHeight,
        const int canonical[6][6]);

    bool valid() const;
    int width() const;
    int height() const;
    int minimumX() const;
    int minimumY() const;

    void actualToCanonical(int x, int y, int &u, int &v) const;
    void canonicalToActual(int u, int v, int &x, int &y) const;
    int colorAtCanonical(int u, int v) const;

private:
    XTransCfaTransform transform_;
    int inverseA_ = 1;
    int inverseB_ = 0;
    int inverseC_ = 0;
    int inverseD_ = 1;
    int minimumX_ = 0;
    int minimumY_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool valid_ = false;
    int canonical_[6][6] = {};
};

} // namespace rtengine
