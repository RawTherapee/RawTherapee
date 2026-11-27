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

#include "rtengine/math/pointvec.h"
#include "rtengine/util/newtype.h"

#include <cairomm/matrix.h>
#include <fmt/format.h>

namespace rt {
namespace canvas {

struct WorldSpace
{
};
struct CameraSpace
{
};
struct WidgetSpace
{
};

template <class Space, class T>
struct Scalar :
    NewType<Space, T>,
    new_type::Equality<Scalar<Space, T>>,
    new_type::Comparison<Scalar<Space, T>>,
    new_type::FloatingPointArithmetic<Scalar<Space, T>>
{
    // Make constructors available
    using NewType<Space, T>::NewType;

    template <class U> explicit constexpr operator Scalar<Space, U>() const
    {
        return Scalar<Space, U>(static_cast<U>(this->value()));
    }
};

using WorldScalar = Scalar<WorldSpace, double>;
using CameraScalar = Scalar<CameraSpace, double>;
using WidgetScalar = Scalar<WidgetSpace, double>;

using IntWorldScalar = Scalar<WorldSpace, int>;
using IntWidgetScalar = Scalar<WidgetSpace, int>;

static_assert(sizeof(WorldScalar) == sizeof(double), "Extra padding in Scalar<T>");
static_assert(sizeof(IntWorldScalar) == sizeof(int), "Extra padding in Scalar<T>");

template <class Space, class T> struct Vec;

template <class Space, class T> struct Point
{
    Scalar<Space, T> x;
    Scalar<Space, T> y;

    Vec<Space, T> asVec() const { return Vec<Space, T>{ x, y }; }

    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }

    // static_cast<WorldPoint>(IntWorldPoint{})
    // static_cast<IntWorldPoint>(WorldPoint{})
    template <class U> explicit operator Point<Space, U>() const
    {
        return Point<Space, U>{ static_cast<Scalar<Space, U>>(x),
                                static_cast<Scalar<Space, U>>(y) };
    }

    // static_cast<geom::IntPoint>(IntWorldPoint{})
    template <class U = T,
              typename std::enable_if<std::is_same<U, int>::value, int>::type = 0>
    explicit operator geom::IntPoint() const
    {
        return geom::IntPoint{ x.value(), y.value() };
    }

    // static_cast<geom::Point>(WorldPoint{})
    template <class U = T,
              typename std::enable_if<std::is_same<U, double>::value, int>::type = 0>
    explicit operator geom::Point() const
    {
        return geom::Point{ x.value(), y.value() };
    }

    friend Point operator+(Point point, Vec<Space, T> offset)
    {
        return Point{ point.x + offset.x, point.y + offset.y };
    }

    friend Point& operator+=(Point& point, Vec<Space, T> offset)
    {
        point.x += offset.x;
        point.y += offset.y;
        return point;
    }

    friend Vec<Space, T> operator-(Point lhs, Point rhs)
    {
        return Vec<Space, T>{ lhs.x - rhs.x, lhs.y - rhs.y };
    }

    friend Point operator-(Point point, Vec<Space, T> offset)
    {
        return Point{ point.x - offset.x, point.y - offset.y };
    }

    friend Point& operator-=(Point& point, Vec<Space, T> offset)
    {
        point.x -= offset.x;
        point.y -= offset.y;
        return point;
    }
};

using WorldPoint = Point<WorldSpace, double>;
using CameraPoint = Point<CameraSpace, double>;
using WidgetPoint = Point<WidgetSpace, double>;

using IntWorldPoint = Point<WorldSpace, int>;

template <class Space, class T> struct Vec
{
    Scalar<Space, T> x;
    Scalar<Space, T> y;

    bool operator==(const Vec& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Vec& other) const { return !(*this == other); }

    friend Vec operator*(Vec vec, double scale)
    {
        Scalar<Space, T> s(scale);
        return Vec{ vec.x * s, vec.y * s };
    }

    friend Vec& operator*=(Vec& vec, double scale)
    {
        Scalar<Space, T> s(scale);
        vec.x *= s;
        vec.y *= s;
        return vec;
    }

    friend Vec operator/(Vec vec, double scale)
    {
        Scalar<Space, T> s(scale);
        return Vec{ vec.x / s, vec.y / s };
    }

    friend Vec& operator/=(Vec& vec, double scale)
    {
        Scalar<Space, T> s(scale);
        vec.x /= s;
        vec.y /= s;
        return vec;
    }
};

using WorldVec = Vec<WorldSpace, double>;
using CameraVec = Vec<CameraSpace, double>;
using WidgetVec = Vec<WidgetSpace, double>;

template <class Space, class T> struct Size
{
    Scalar<Space, T> width;
    Scalar<Space, T> height;

    Point<Space, T> asPoint() const { return Point<Space, T>{ width, height }; }
    Vec<Space, T> asVec() const { return Vec<Space, T>{ width, height }; }

    bool operator==(const Size& other) const
    {
        return width == other.width && height == other.height;
    }
    bool operator!=(const Size& other) const { return !(*this == other); }

    // static_cast<WorldSize>(IntWorldSize{})
    // static_cast<IntWorldSize>(WorldSize{})
    template <class U> explicit operator Size<Space, U>() const
    {
        return Size<Space, U>{ static_cast<Scalar<Space, U>>(width),
                               static_cast<Scalar<Space, U>>(height) };
    }

    friend Size operator*(Size size, double scale)
    {
        Scalar<Space, T> s(scale);
        return Size{ size.width * s, size.height * s };
    }

    friend Size& operator*=(Size& size, double scale)
    {
        Scalar<Space, T> s(scale);
        size.width *= s;
        size.height *= s;
        return size;
    }

    friend Size operator/(Size size, double scale)
    {
        Scalar<Space, T> s(scale);
        return Size{ size.width / s, size.height / s };
    }

    friend Size& operator/=(Size& size, double scale)
    {
        Scalar<Space, T> s(scale);
        size.width /= s;
        size.height /= s;
        return size;
    }
};

using WorldSize = Size<WorldSpace, double>;
using CameraSize = Size<CameraSpace, double>;
using WidgetSize = Size<WidgetSpace, double>;

using IntWorldSize = Size<WorldSpace, int>;
using IntWidgetSize = Size<WidgetSpace, int>;

struct CameraState
{
    WorldPoint pos;  // Center of camera
    WidgetSize size;
    double zoom = 1.0;
    int device_scale = 1.0;
};

template <class FromSpace, class ToSpace> class SpaceTransform
{
public:
    static SpaceTransform build(const CameraState& camera);

    SpaceTransform() : m_matrix(Cairo::identity_matrix()) {}

    const Cairo::Matrix& matrix() const { return m_matrix; }

    Point<ToSpace, double> operator()(Point<FromSpace, double> point) const
    {
        double x = point.x.value();
        double y = point.y.value();
        m_matrix.transform_point(x, y);
        return { Scalar<ToSpace, double>(x), Scalar<ToSpace, double>(y) };
    }

    Vec<ToSpace, double> operator()(Vec<FromSpace, double> vec) const
    {
        double x = vec.x.value();
        double y = vec.y.value();
        m_matrix.transform_distance(x, y);
        return { Scalar<ToSpace, double>(x), Scalar<ToSpace, double>(y) };
    }

    Size<ToSpace, double> operator()(Size<FromSpace, double> vec) const
    {
        double width = vec.width.value();
        double height = vec.height.value();
        m_matrix.transform_distance(width, height);
        return { Scalar<ToSpace, double>(width), Scalar<ToSpace, double>(height) };
    }

    Scalar<ToSpace, double> operator()(Scalar<FromSpace, double> value) const
    {
        double scaled = value.value();
        double dummy = 0;
        m_matrix.transform_distance(scaled, dummy);
        return Scalar<ToSpace, double>(scaled);
    }

    Point<ToSpace, double> operator()(Point<FromSpace, int> point) const
    {
        double x = point.x.value();
        double y = point.y.value();
        m_matrix.transform_point(x, y);
        return { Scalar<ToSpace, double>(x), Scalar<ToSpace, double>(y) };
    }

    Scalar<ToSpace, double> operator()(Scalar<FromSpace, int> value) const
    {
        double scaled = value.value();
        double dummy = 0;
        m_matrix.transform_distance(scaled, dummy);
        return Scalar<ToSpace, double>(scaled);
    }

private:
    Cairo::Matrix m_matrix;
};

inline void applyWidgetToCameraTransforms(Cairo::Matrix& m, const CameraState& camera)
{
    double scale = camera.device_scale;
    m.scale(scale, scale);
    m.translate(-camera.size.width.value() / 2.0, -camera.size.height.value() / 2.0);
}

inline void applyCameraToWidgetTransforms(Cairo::Matrix& m, const CameraState& camera)
{
    m.translate(camera.size.width.value() / 2.0, camera.size.height.value() / 2.0);
    double scale = 1.0 / camera.device_scale;
    m.scale(scale, scale);
}

inline void applyCameraToWorldTransforms(Cairo::Matrix& m, const CameraState& camera)
{
    double zoom = 1.0 / camera.zoom;
    m.translate(camera.pos.x.value(), camera.pos.y.value());
    m.scale(zoom, zoom);
}

inline void applyWorldToCameraTransforms(Cairo::Matrix& m, const CameraState& camera)
{
    double zoom = camera.zoom;
    m.scale(zoom, zoom);
    m.translate(-camera.pos.x.value(), -camera.pos.y.value());
}

template <>
inline auto SpaceTransform<WidgetSpace, CameraSpace>::build(const CameraState& camera)
    -> SpaceTransform
{
    SpaceTransform t;
    applyWidgetToCameraTransforms(t.m_matrix, camera);
    return t;
}

template <>
inline auto SpaceTransform<WidgetSpace, WorldSpace>::build(const CameraState& camera)
    -> SpaceTransform
{
    SpaceTransform t;
    applyCameraToWorldTransforms(t.m_matrix, camera);
    applyWidgetToCameraTransforms(t.m_matrix, camera);
    return t;
}

template <>
inline auto SpaceTransform<CameraSpace, WidgetSpace>::build(const CameraState& camera)
    -> SpaceTransform
{
    SpaceTransform t;
    applyCameraToWidgetTransforms(t.m_matrix, camera);
    return t;
}

template <>
inline auto SpaceTransform<CameraSpace, WorldSpace>::build(const CameraState& camera)
    -> SpaceTransform
{
    SpaceTransform t;
    applyCameraToWorldTransforms(t.m_matrix, camera);
    return t;
}

template <>
inline auto SpaceTransform<WorldSpace, WidgetSpace>::build(const CameraState& camera)
    -> SpaceTransform
{
    SpaceTransform t;
    applyCameraToWidgetTransforms(t.m_matrix, camera);
    applyWorldToCameraTransforms(t.m_matrix, camera);
    return t;
}

template <>
inline auto SpaceTransform<WorldSpace, CameraSpace>::build(const CameraState& camera)
    -> SpaceTransform
{
    SpaceTransform t;
    applyWorldToCameraTransforms(t.m_matrix, camera);
    return t;
}

inline WidgetPoint cameraToWidget(CameraPoint point, const CameraState& camera)
{
    return SpaceTransform<CameraSpace, WidgetSpace>::build(camera)(point);
}

inline WidgetPoint worldToWidget(WorldPoint point, const CameraState& camera)
{
    return SpaceTransform<WorldSpace, WidgetSpace>::build(camera)(point);
}

inline CameraPoint widgetToCamera(WidgetPoint point, const CameraState& camera)
{
    return SpaceTransform<WidgetSpace, CameraSpace>::build(camera)(point);
}

inline CameraPoint worldToCamera(WorldPoint point, const CameraState& camera)
{
    return SpaceTransform<WorldSpace, CameraSpace>::build(camera)(point);
}

inline WorldPoint widgetToWorld(WidgetPoint point, const CameraState& camera)
{
    return SpaceTransform<WidgetSpace, WorldSpace>::build(camera)(point);
}

inline WorldPoint cameraToWorld(CameraPoint point, const CameraState& camera)
{
    return SpaceTransform<CameraSpace, WorldSpace>::build(camera)(point);
}

inline WorldPoint toWorldPoint(geom::Point p)
{
    return WorldPoint{ WorldScalar(p.x), WorldScalar(p.y) };
}

}  // namespace canvas
}  // namespace rt

template <class Space, class T>
struct fmt::formatter<rt::canvas::Point<Space, T>> :
    fmt::formatter<rt::canvas::Scalar<Space, T>>
{
    auto format(rt::canvas::Point<Space, T> p,
                fmt::format_context& ctx) const -> fmt::format_context::iterator
    {
        using Underlying = fmt::formatter<rt::canvas::Scalar<Space, T>>;
        auto out = ctx.out();
        out = fmt::format_to(out, "(");
        out = Underlying::format(p.x, ctx);
        out = fmt::format_to(out, ", ");
        out = Underlying::format(p.y, ctx);
        out = fmt::format_to(out, ")");
        return out;
    }
};

template <class Space, class T>
struct fmt::formatter<rt::canvas::Vec<Space, T>> :
    fmt::formatter<rt::canvas::Scalar<Space, T>>
{
    auto format(rt::canvas::Vec<Space, T> v,
                fmt::format_context& ctx) const -> fmt::format_context::iterator
    {
        using Underlying = fmt::formatter<rt::canvas::Scalar<Space, T>>;
        auto out = ctx.out();
        out = fmt::format_to(out, "(");
        out = Underlying::format(v.x, ctx);
        out = fmt::format_to(out, ", ");
        out = Underlying::format(v.y, ctx);
        out = fmt::format_to(out, ")");
        return out;
    }
};

template <class Space, class T>
struct fmt::formatter<rt::canvas::Size<Space, T>> :
    fmt::formatter<rt::canvas::Scalar<Space, T>>
{
    auto format(rt::canvas::Size<Space, T> s,
                fmt::format_context& ctx) const -> fmt::format_context::iterator
    {
        using Underlying = fmt::formatter<rt::canvas::Scalar<Space, T>>;
        auto out = ctx.out();
        out = fmt::format_to(out, "(");
        out = Underlying::format(s.width, ctx);
        out = fmt::format_to(out, " x ");
        out = Underlying::format(s.height, ctx);
        out = fmt::format_to(out, ")");
        return out;
    }
};
