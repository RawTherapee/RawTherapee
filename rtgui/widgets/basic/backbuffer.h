/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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

#include "rtengine/coord.h"

#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <gdkmm/window.h>

class RefCount
{
private:
    int refCount;
public:
    RefCount() : refCount(1) {}
    virtual ~RefCount() {}

    void reference()
    {
        ++refCount;
    }
    void unreference()
    {
        --refCount;

        if (!refCount) {
            delete this;
        }
    }
};

/**
 * @brief Handle back buffers as automatically as possible, and suitable to be used with Glib::RefPtr
 */
class BackBuffer : public RefCount
{

protected:
    int x, y, w, h;  // Rectangle where the colored bar has to be drawn
    rtengine::Coord offset;  // Offset of the source region to draw, relative to the top left corner
    Cairo::RefPtr<Cairo::ImageSurface> surface;
    bool dirty;  // mean that the Surface has to be (re)allocated

public:
    BackBuffer();
    BackBuffer(int w, int h, Cairo::Format format = Cairo::FORMAT_RGB24);

    // set the destination drawing rectangle; return true if the dimensions are different
    // Note: newW & newH must be > 0
    bool setDrawRectangle(Glib::RefPtr<Gdk::Window> window, Gdk::Rectangle &rectangle, bool updateBackBufferSize = true);
    bool setDrawRectangle(Glib::RefPtr<Gdk::Window> window, int newX, int newY, int newW, int newH, bool updateBackBufferSize = true);
    bool setDrawRectangle(Cairo::Format format, Gdk::Rectangle &rectangle, bool updateBackBufferSize = true);
    bool setDrawRectangle(Cairo::Format format, int newX, int newY, int newW, int newH, bool updateBackBufferSize = true);
    // set the destination drawing location, do not modify other parameters like size and offset. Use setDrawRectangle to set all parameters at the same time
    void setDestPosition(int x, int y);
    void setSrcOffset(int x, int y);
    void setSrcOffset(const rtengine::Coord &newOffset);
    void getSrcOffset(int &x, int &y);
    void getSrcOffset(rtengine::Coord &offset);

    void copyRGBCharData(const unsigned char *srcData, int srcX, int srcY, int srcW, int srcH, int srcRowStride, int dstX, int dstY);
    void copySurface(Glib::RefPtr<Gdk::Window> window, Gdk::Rectangle *rectangle = nullptr);
    void copySurface(BackBuffer *destBackBuffer, Gdk::Rectangle *rectangle = nullptr);
    void copySurface(const Cairo::RefPtr<Cairo::ImageSurface>& destSurface, Gdk::Rectangle *rectangle = nullptr);
    void copySurface(const Cairo::RefPtr<Cairo::Context>& crDest, Gdk::Rectangle *destRectangle = nullptr);

    void setDirty(bool isDirty)
    {
        dirty = isDirty;

        if (!dirty && !surface) {
            dirty = true;
        }
    }
    bool isDirty()
    {
        return dirty;
    }
    // you have to check if the surface is created thanks to surfaceCreated before starting to draw on it
    bool surfaceCreated()
    {
        return static_cast<bool>(surface);
    }
    Cairo::RefPtr<Cairo::ImageSurface> getSurface()
    {
        return surface;
    }
    void setSurface(Cairo::RefPtr<Cairo::ImageSurface> surf)
    {
        surface = surf;
    }
    void deleteSurface()
    {
        if (surface) {
            surface.clear();
        }

        dirty = true;
    }
    // will let you get a Cairo::Context for Cairo drawing operations
    Cairo::RefPtr<Cairo::Context> getContext()
    {
        return Cairo::Context::create(surface);
    }
    int getWidth()
    {
        return surface ? surface->get_width() : 0;    // sending back the allocated width
    }
    int getHeight()
    {
        return surface ? surface->get_height() : 0;    // sending back the allocated height
    }
};
