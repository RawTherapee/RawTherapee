/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "angle.h"

#include <cmath>  // Unfortunately many functions not yet marked constexpr

namespace rt {
namespace geom {

struct IntPoint
{
    using ValueType = int;

    int x;
    int y;

    constexpr IntPoint() : x(0), y(0) {}
    constexpr IntPoint(int a_x, int a_y) : x(a_x), y(a_y) {}
};

struct Point
{
    using ValueType = double;

    double x;
    double y;

    constexpr Point() : x(0), y(0) {}
    constexpr Point(double a_x, double a_y) : x(a_x), y(a_y) {}
    constexpr Point(IntPoint p) : x(static_cast<double>(p.x)),
                                  y(static_cast<double>(p.y)) {}

    constexpr IntPoint floor() const
    {
        return IntPoint(static_cast<int>(x), static_cast<int>(y));
    }
};

struct Vec
{
    double x;
    double y;

    constexpr Vec() : x(0), y(0) {}
    constexpr Vec(double a_x, double a_y) : x(a_x), y(a_y) {}
    constexpr Vec(Point p) : x(p.x), y(p.y) {}

    double length() const { return std::hypot(x, y); }
    constexpr double lengthSquared() const { return x * x + y * y; }
    Radians angle() const { return Radians(std::atan2(y, x)); }

    Vec& operator*=(double scale) { x *= scale; y *= scale; return *this; }
    Vec& operator/=(double scale) { x /= scale; y /= scale; return *this; }
    friend Vec operator*(Vec v, double scale) { return v *= scale; }
    friend Vec operator/(Vec v, double scale) { return v /= scale; }
};

// --- Point ---

inline Point operator+(Point p, Vec offset)
{
    return Point(p.x + offset.x, p.y + offset.y);
}

inline Point& operator+=(Point& p, Vec offset)
{
    p.x += offset.x;
    p.y += offset.y;
    return p;
}

inline Point operator-(Point p, Vec offset)
{
    return Point(p.x - offset.x, p.y - offset.y);
}

inline Point& operator-=(Point& p, Vec offset)
{
    p.x -= offset.x;
    p.y -= offset.y;
    return p;
}

inline Vec operator-(Point lhs, Point rhs) { return Vec(lhs.x - rhs.x, lhs.y - rhs.y); }

// --- Utilities ---

inline Point midpoint(Point a, Point b)
{
    return a + (b - a) / 2.0;
}

}  // namespace geom
}  // namespace rt
