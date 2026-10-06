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

#include <type_traits>

namespace rt {

/**
 * Enable bit flags for an enum class by specializing this trait.
 *
 * namespace ns {
 *     enum class Flags {
 *         NONE = 0,
 *         FLAG_ONE = (1 << 0),
 *         FLAG_TWO = (1 << 1),
 *         ALL = FLAG_ONE | FLAG_TWO
 *     };
 * }  // namespace ns
 *
 * // In global namespace
 * template <> struct rt::EnumAsBitflags<ns::Flags> : std::true_type {};
 *
 * void foo(Flags lhs, Flags rhs) {
 *     Flags result = lhs | rhs;
 *     bool flags_set = rt::any(result);
 *     bool no_flags_set = rt::none(result);
 * }
 */
template <class T> struct EnumAsBitflags : std::false_type
{
};

template <class T, class Result = typename std::remove_cv<T>::type>
using EnableIfFlags =
    typename std::enable_if<EnumAsBitflags<typename std::remove_cv<T>::type>::value,
                            Result>::type;

template <class T> constexpr rt::EnableIfFlags<T, bool> any(T value)
{
    using U = typename std::underlying_type<T>::type;
    return static_cast<bool>(static_cast<U>(value));
}

template <class T> constexpr rt::EnableIfFlags<T, bool> none(T value)
{
    return !any(value);
}

}  // namespace rt

template <class T> constexpr rt::EnableIfFlags<T> operator|(T lhs, T rhs)
{
    using U = typename std::underlying_type<T>::type;
    return static_cast<T>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <class T> constexpr rt::EnableIfFlags<T> operator&(T lhs, T rhs)
{
    using U = typename std::underlying_type<T>::type;
    return static_cast<T>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <class T> constexpr rt::EnableIfFlags<T> operator^(T lhs, T rhs)
{
    using U = typename std::underlying_type<T>::type;
    return static_cast<T>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

template <class T> constexpr rt::EnableIfFlags<T> operator~(T val)
{
    using U = typename std::underlying_type<T>::type;
    return static_cast<T>(~static_cast<U>(val));
}

template <class T> constexpr rt::EnableIfFlags<T>& operator|=(T& lhs, T rhs)
{
    return lhs = lhs | rhs;
}

template <class T> constexpr rt::EnableIfFlags<T>& operator&=(T& lhs, T rhs)
{
    return lhs = lhs & rhs;
}

template <class T> constexpr rt::EnableIfFlags<T>& operator^=(T& lhs, T rhs)
{
    return lhs = lhs ^ rhs;
}
