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

namespace rt {

// Similar to C++20's <numbers>
namespace numbers {

constexpr double pi = 3.14159265358979323846;

}  // namespace numbers

// Remove need to include <algorithm> for utility functions
template <class T>
constexpr const T& min(const T& lhs, const T& rhs) { return lhs < rhs ? lhs : rhs; }

template <class T>
constexpr const T& max(const T& lhs, const T& rhs) { return lhs > rhs ? lhs : rhs; }

template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    return (v < lo) ? lo : ((hi < v) ? hi : v);
}

}  // namespace rt
