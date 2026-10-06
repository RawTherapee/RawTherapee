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

#include "rtengine/math/math.h"
#include "rtengine/util/newtype.h"

namespace rt {
namespace geom {

struct Degrees;
struct Radians;

template <class T>
struct AngleOperations : rt::new_type::Addition<T>,
                         rt::new_type::Subtraction<T>,
                         rt::new_type::Equality<T>,
                         rt::new_type::Comparison<T>
{
    friend T& operator*=(T& lhs, double rhs)
    {
        lhs.value() *= rhs;
        return lhs;
    }

    friend T operator*(const T& lhs, double rhs)
    {
        return T(lhs.value() * rhs);
    }

    friend T& operator/=(T& lhs, double rhs)
    {
        lhs.value() /= rhs;
        return lhs;
    }

    friend T operator/(const T& lhs, double rhs)
    {
        return T(lhs.value() / rhs);
    }
};

struct Radians : rt::NewType<Radians, double>, AngleOperations<Radians>
{
    using NewType::NewType;

    Radians(Degrees d);
};

struct Degrees : rt::NewType<Degrees, double>, AngleOperations<Degrees>
{
    using NewType::NewType;

    Degrees(Radians r);
};

inline Radians::Radians(Degrees d) : NewType(d.value() * numbers::pi / 180.0) {}

inline Degrees::Degrees(Radians r) : NewType(r.value() * 180.0 / numbers::pi) {}

}  // namespace geom
}  // namespace rt
