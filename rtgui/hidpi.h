/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2024 Daniel Gao <daniel.gao.work@gmail.com>
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

#include <cairomm/refptr.h>
#include <glibmm/refptr.h>

namespace Cairo {
class Context;
class ImageSurface;
class Surface;
class SurfacePattern;
}

namespace Gdk {
class Pixbuf;
}

namespace Gtk {
class Widget;
class Window;
}

namespace hidpi {

struct DeviceCoord;
struct DeviceSize;
struct ScaledDeviceSize;

enum class PixelSpace { LOGICAL, PHYSICAL };

struct LogicalCoord {
    int x = 0;
    int y = 0;

    LogicalCoord() = default;
    LogicalCoord(int logical_x, int logical_y) : x(logical_x), y(logical_y) {}

    DeviceCoord scaleToDevice(int device_scale) const;

    constexpr PixelSpace pixelSpace() const { return PixelSpace::LOGICAL; }

    LogicalCoord operator+(const LogicalCoord& other) const;
    LogicalCoord operator-(const LogicalCoord& other) const;
};

struct DeviceCoord {
    int x = 0;
    int y = 0;

    DeviceCoord() = default;
    DeviceCoord(int device_x, int device_y) : x(device_x), y(device_y) {}

    constexpr PixelSpace pixelSpace() const { return PixelSpace::PHYSICAL; }

    DeviceCoord operator+(const DeviceSize& other) const;
    DeviceCoord operator-(const DeviceSize& other) const;
};

struct LogicalSize {
    int width = 0;
    int height = 0;

    static LogicalSize forWidget(const Gtk::Widget* widget);

    LogicalSize() = default;
    LogicalSize(int logical_width, int logical_height)
            : width(logical_width), height(logical_height) {}

    ScaledDeviceSize scaleToDevice(int device_scale) const;

    constexpr PixelSpace pixelSpace() const { return PixelSpace::LOGICAL; }

    bool operator==(const LogicalSize& other) const {
        return width == other.width && height == other.height;
    }
};

struct DeviceSize {
    int width = 0;
    int height = 0;

    DeviceSize() : width(0), height(0) {}
    DeviceSize(int device_width, int device_height)
            : width(device_width), height(device_height) {}

    constexpr PixelSpace pixelSpace() const { return PixelSpace::PHYSICAL; }
};

struct ScaledDeviceSize {
    int width = 0;
    int height = 0;
    int device_scale = 1;

    static ScaledDeviceSize forWidget(const Gtk::Widget* widget);

    constexpr PixelSpace pixelSpace() const { return PixelSpace::PHYSICAL; }
};

class DevicePixbuf {
public:
    DevicePixbuf();
    DevicePixbuf(const Glib::RefPtr<Gdk::Pixbuf>& ptr, int device_scale);

    DevicePixbuf(const DevicePixbuf& other);
    DevicePixbuf& operator=(const DevicePixbuf& other);
    DevicePixbuf(DevicePixbuf&& other);
    DevicePixbuf& operator=(DevicePixbuf&& other);

    operator bool() const { return static_cast<bool>(m_pixbuf); }

    const Glib::RefPtr<Gdk::Pixbuf>& pixbuf() const { return m_pixbuf; }
    ScaledDeviceSize size() const;

    friend void swap(DevicePixbuf& lhs, DevicePixbuf& rhs);

private:
    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;
    int m_device_scale;
};

// TODO: Replace with Cairo::Context::get_source_for_surface() when cairomm
//       dependency provides the same function.
Cairo::RefPtr<Cairo::SurfacePattern>
getSourceForSurface(const Cairo::RefPtr<Cairo::Context>& context);

void getDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    double& x_scale, double& y_scale);
void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    int x_scale, int y_scale);
inline void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface, int scale) {
    setDeviceScale(surface, scale, scale);
}

void getDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    double& x_scale, double& y_scale);
void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    int x_scale, int y_scale);
inline void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface, int scale) {
    setDeviceScale(surface, scale, scale);
}

}  // namespace hidpi

// Position in image pixel coordinates
struct ImageCoord {
    int x = 0;
    int y = 0;

    ImageCoord() = default;
    ImageCoord(int pos_x, int pos_y) : x(pos_x), y(pos_y) {}
};

// Pixel aligned size in image pixels
struct ImageSize {
    int width = 0;
    int height = 0;

    ImageSize() = default;
    ImageSize(int w, int h) : width(w), height(h) {}
};
