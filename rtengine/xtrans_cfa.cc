#include "xtrans_cfa.h"

#include <algorithm>
#include <array>
#include <limits>

namespace rtengine
{

bool findCanonicalXTransTransform(
    const int actual[6][6],
    XTransCfaTransform &result)
{
    return findCanonicalXTransTransform(actual, CANONICAL_XTRANS_CFA, result);
}

bool findCanonicalXTransTransform(
    const int actual[6][6],
    const int canonical[6][6],
    XTransCfaTransform &result)
{
    // Keep this order stable. The canonical matrix has symmetries, so more
    // than one transform may match a representation of it.
    constexpr int matrices[8][4] = {
        { 1,  0,  0,  1}, { 0, -1,  1,  0},
        {-1,  0,  0, -1}, { 0,  1, -1,  0},
        {-1,  0,  0,  1}, { 1,  0,  0, -1},
        { 0,  1,  1,  0}, { 0, -1, -1,  0}
    };

    for (const auto &matrix : matrices) {
        for (int oy = 0; oy < 6; ++oy) {
            for (int ox = 0; ox < 6; ++ox) {
                bool matches = true;

                for (int y = 0; y < 6 && matches; ++y) {
                    for (int x = 0; x < 6; ++x) {
                        const int cx = positiveModulo(matrix[0] * x + matrix[1] * y + ox, 6);
                        const int cy = positiveModulo(matrix[2] * x + matrix[3] * y + oy, 6);

                        if (actual[y][x] != canonical[cy][cx]) {
                            matches = false;
                            break;
                        }
                    }
                }

                if (matches) {
                    result = {matrix[0], matrix[1], matrix[2], matrix[3], ox, oy};
                    return true;
                }
            }
        }
    }

    return false;
}

XTransCfaView::XTransCfaView(
    const XTransCfaTransform &transform,
    int actualWidth,
    int actualHeight) :
    XTransCfaView(transform, actualWidth, actualHeight, CANONICAL_XTRANS_CFA)
{
}

XTransCfaView::XTransCfaView(
    const XTransCfaTransform &transform,
    int actualWidth,
    int actualHeight,
    const int canonical[6][6]) :
    transform_(transform)
{
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            canonical_[y][x] = canonical[y][x];
        }
    }
    if (actualWidth <= 0 || actualHeight <= 0) {
        return;
    }

    const int determinant = transform.a * transform.d - transform.b * transform.c;
    if (determinant != 1 && determinant != -1) {
        return;
    }

    inverseA_ = determinant * transform.d;
    inverseB_ = -determinant * transform.b;
    inverseC_ = -determinant * transform.c;
    inverseD_ = determinant * transform.a;

    const std::array<std::array<int, 2>, 4> corners{{
        {{0, 0}},
        {{actualWidth - 1, 0}},
        {{0, actualHeight - 1}},
        {{actualWidth - 1, actualHeight - 1}}
    }};
    int maximumX = std::numeric_limits<int>::min();
    int maximumY = std::numeric_limits<int>::min();
    minimumX_ = std::numeric_limits<int>::max();
    minimumY_ = std::numeric_limits<int>::max();

    for (const auto &corner : corners) {
        const int x = transform.a * corner[0] + transform.b * corner[1] + transform.ox;
        const int y = transform.c * corner[0] + transform.d * corner[1] + transform.oy;
        minimumX_ = std::min(minimumX_, x);
        minimumY_ = std::min(minimumY_, y);
        maximumX = std::max(maximumX, x);
        maximumY = std::max(maximumY, y);
    }

    width_ = maximumX - minimumX_ + 1;
    height_ = maximumY - minimumY_ + 1;
    valid_ = width_ > 0 && height_ > 0;
}

bool XTransCfaView::valid() const
{
    return valid_;
}

int XTransCfaView::width() const
{
    return width_;
}

int XTransCfaView::height() const
{
    return height_;
}

int XTransCfaView::minimumX() const
{
    return minimumX_;
}

int XTransCfaView::minimumY() const
{
    return minimumY_;
}

void XTransCfaView::actualToCanonical(int x, int y, int &u, int &v) const
{
    u = transform_.a * x + transform_.b * y + transform_.ox - minimumX_;
    v = transform_.c * x + transform_.d * y + transform_.oy - minimumY_;
}

void XTransCfaView::canonicalToActual(int u, int v, int &x, int &y) const
{
    const int canonicalX = u + minimumX_ - transform_.ox;
    const int canonicalY = v + minimumY_ - transform_.oy;
    x = inverseA_ * canonicalX + inverseB_ * canonicalY;
    y = inverseC_ * canonicalX + inverseD_ * canonicalY;
}

int XTransCfaView::colorAtCanonical(int u, int v) const
{
    const int x = positiveModulo(u + minimumX_, 6);
    const int y = positiveModulo(v + minimumY_, 6);
    return canonical_[y][x];
}

} // namespace rtengine
