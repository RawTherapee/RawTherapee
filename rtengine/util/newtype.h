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
#include <utility>

namespace rt {

/**
 * Wrapper class for adding compiler type checking to type aliases.
 *
 * Normally, the compiler will happily accept aliases of different underlying
 * types even if the types have different semantic meanings. This leads to a
 * certain class of bugs:
 *
 * ```
 * using Year = int;
 * using Month = int;
 *
 * void fun(Year y, Month m);
 *
 * Year y = 2000;
 * Month m = 1;
 *
 * // This code compiles but is clearly a bug
 * fun(m, y);
 * ```
 *
 * Using this wrapper class allows the compiler to enforce compile-time type
 * checking for your new type aliases. Use the provided operator mixins to add
 * appropriate operator semantics to the new type.
 *
 * ```
 * struct Year : rt::NewType<Year, int>,
 *               rt::new_type::Addition<Year>,
 *               rt::new_type::Subtraction<Year>
 * {
 *     // Make constructors available
 *     using NewType::NewType;
 * };
 *
 * ```
 *
 * The limitation of not being a native feature in the language means this is
 * only effective for underlying types with operators or very standard member
 * functions (e.g. primitives). Otherwise, you'll need to roll your own
 * dedicated template wrapper class using phantom types as shown below.
 *
 * ```
 * template <class Tag>
 * struct Point
 * {
 *     double x = 0;
 *     double y = 0;
 * };
 *
 * struct WorldSpace {};
 * struct ScreenSpace {};
 *
 * using WorldPoint = Point<WorldSpace>;
 * using ScreenPoint = Point<ScreenSpace>;
 * ```
 *
 * @see https://www.foonathan.net/2016/10/strong-typedefs/
 * @see https://doc.rust-lang.org/rust-by-example/generics/new_types.html
 */
template <class Tag, class T> class NewType
{
public:
    using TagType = Tag;
    using ValueType = T;

    NewType() : m_value() {}

    explicit NewType(const T& value) : m_value(value) {}

    explicit NewType(T&& value) noexcept(std::is_nothrow_move_constructible<T>::value) :
        m_value(std::move(value))
    {
    }

    T& value() noexcept { return m_value; }
    const T& value() const noexcept { return m_value; }

    explicit operator T&() noexcept { return m_value; }
    explicit operator const T&() const noexcept { return m_value; }

    friend void swap(NewType& a, NewType& b) noexcept
    {
        using std::swap;
        swap(static_cast<T&>(a), static_cast<T&>(b));
    }

private:
    T m_value;
};

template <class Tag, class T> const T& format_as(const NewType<Tag, T>& v)
{
    return v.value();
}

namespace new_type {

template <class T> struct Addition
{
    friend T& operator+=(T& lhs, const T& rhs)
    {
        lhs.value() += rhs.value();
        return lhs;
    }

    friend T operator+(const T& lhs, const T& rhs)
    {
        return T(lhs.value() + rhs.value());
    }
};

template <class T> struct Subtraction
{
    friend T& operator-=(T& lhs, const T& rhs)
    {
        lhs.value() -= rhs.value();
        return lhs;
    }

    friend T operator-(const T& lhs, const T& rhs)
    {
        return T(lhs.value() - rhs.value());
    }
};

template <class T> struct Multiplication
{
    friend T& operator*=(T& lhs, const T& rhs)
    {
        lhs.value() *= rhs.value();
        return lhs;
    }

    friend T operator*(const T& lhs, const T& rhs)
    {
        return T(lhs.value() * rhs.value());
    }
};

template <class T> struct Division
{
    friend T& operator/=(T& lhs, const T& rhs)
    {
        lhs.value() /= rhs.value();
        return lhs;
    }

    friend T operator/(const T& lhs, const T& rhs)
    {
        return T(lhs.value() / rhs.value());
    }
};

template <class T>
struct FloatingPointArithmetic :
    Addition<T>,
    Subtraction<T>,
    Multiplication<T>,
    Division<T>
{
};

template <class T> struct Equality
{
    friend bool operator==(const T& lhs, const T& rhs)
    {
        return lhs.value() == rhs.value();
    }
    friend bool operator!=(const T& lhs, const T& rhs)
    {
        return lhs.value() != rhs.value();
    }
};

template <class T> struct Comparison
{
    friend bool operator<(const T& lhs, const T& rhs)
    {
        return lhs.value() < rhs.value();
    }
    friend bool operator<=(const T& lhs, const T& rhs)
    {
        return lhs.value() <= rhs.value();
    }
    friend bool operator>(const T& lhs, const T& rhs)
    {
        return lhs.value() > rhs.value();
    }
    friend bool operator>=(const T& lhs, const T& rhs)
    {
        return lhs.value() >= rhs.value();
    }
};

}  // namespace new_type
}  // namespace rt
