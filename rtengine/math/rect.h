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

#include "math.h"
#include "pointvec.h"

#include <optional>
#include <type_traits>

namespace rt {
namespace geom {

/**
 * An axis aligned rectangle (of double or int).
 *
 * The rectangle's bounds are inclusive on the min side and exclusive on the
 * max side. In other words, the interval along each axis is described as
 * [min, max).
 */
template <class T>
class GenericRect
{
public:
    static_assert(std::is_same<T, int>::value || std::is_same<T, double>::value,
                  "GenericRect only supports int and double values");

    using PointType = typename std::conditional<std::is_same<T, int>::value,
                                                IntPoint, Point>::type;
    using ValueType = typename PointType::ValueType;

    constexpr GenericRect(PointType a, PointType b)
        : m_min(rt::min(a.x, b.x), rt::min(a.y, b.y)),
          m_max(rt::max(a.x, b.x), rt::max(a.y, b.y))
    {
    }

    constexpr PointType min() const { return m_min; }
    constexpr PointType max() const { return m_max; }

    constexpr ValueType width() const { return m_max.x - m_min.x; }
    constexpr ValueType height() const { return m_max.y - m_min.y; }

    /**
     * Returns the corner of the rectangle for the given index in the clockwise
     * direction starting from the corner given by min().
     *
     * Since +Y is downwards, the order of corners is in the direction of
     * growing angles.
     */
    PointType corner(unsigned int i) const {
        switch (i % 4) {
            case 0:  return m_min;
            case 1:  return PointType(m_max.x, m_min.y);
            case 2:  return m_max;
            default: return PointType(m_min.x, m_max.y);
        }
    }

    bool contains(PointType p)
    {
        if (p.x < m_min.x) return false;
        else if (p.y < m_min.y) return false;
        else if (p.x >= m_max.x) return false;
        else if (p.y >= m_max.y) return false;
        else return true;
    }

    std::optional<GenericRect> intersect(const GenericRect& other) const;

private:
    PointType m_min;
    PointType m_max;
};

using Rect = GenericRect<double>;
using IntRect = GenericRect<int>;

using BBox = Rect;
using IntBBox = IntRect;

template <class T>
std::optional<GenericRect<T>>
GenericRect<T>::intersect(const GenericRect<T>& other) const
{
    T x0 = rt::max(m_min.x, other.m_min.x);
    T x1 = rt::min(m_max.x, other.m_max.x);
    if (x0 >= x1) return std::nullopt;

    T y0 = rt::max(m_min.y, other.m_min.y);
    T y1 = rt::min(m_max.y, other.m_max.y);
    if (y0 >= y1) return std::nullopt;

    return GenericRect<T>(PointType(x0, y0), PointType(x1, y1));
}

}  // namespace geom
}  // namespace rt
