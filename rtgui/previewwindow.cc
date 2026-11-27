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
#include "previewwindow.h"

#include "cursormanager.h"
#include "drawcropguide.h"
#include "guiutils.h"
#include "hidpi.h"
#include "imagearea.h"
#include "options.h"
#include "rtscalable.h"
#include "widgets/basic/backbuffer.h"

#include "rtengine/procparams.h"

PreviewWindow::PreviewWindow () : previewHandler(nullptr), mainCropWin(nullptr), imageArea(nullptr), imgW(0), imgH(0),
    zoom(0.0), press_x(0), press_y(0), isMoving(false), needsUpdate(false), cursor_type(CSUndefined)

{
    set_name("PreviewWindow");
    get_style_context()->add_class("drawingarea");
    rconn = signal_size_allocate().connect( sigc::mem_fun(*this, &PreviewWindow::on_resized) );
}

PreviewWindow::~PreviewWindow() = default;

void PreviewWindow::on_realize ()
{

    Gtk::DrawingArea::on_realize ();
    add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
}

void PreviewWindow::getObservedFrameArea (int& x, int& y, int& w, int& h)
{

    if (mainCropWin) {
        int cropX, cropY, cropW, cropH;
        mainCropWin->getCropRectangle (cropX, cropY, cropW, cropH);
        // translate it to screen coordinates
        x = round(cropX * zoom);
        y = round(cropY * zoom);
        w = round(cropW * zoom);
        h = round(cropH * zoom);
    }
}

void PreviewWindow::updatePreviewImage ()
{
    Glib::RefPtr<Gdk::Window> wind = get_window();

    if( ! wind ) {
        needsUpdate = true;
        return;
    }

    backBuffer.clear();
    if (!previewHandler) {
        needsUpdate = true;
        return;
    }

    int scale = RTScalable::getScaleForWidget(this);
    auto logical = hidpi::LogicalSize::forWidget(this);

    hidpi::DevicePixbuf result = previewHandler->getRoughImage(logical, scale, zoom);
    if (!result.pixbuf()) {
        needsUpdate = true;
        return;
    }
    
    hidpi::ScaledDeviceSize device = result.size();
    imgW = device.width;
    imgH = device.height;

    backBuffer = Cairo::RefPtr<BackBuffer> ( new BackBuffer(
        device.width, device.height, Cairo::FORMAT_ARGB32) );
    if (!backBuffer->surfaceCreated()) {
        needsUpdate = true;
        return;
    }
    Cairo::RefPtr<Cairo::ImageSurface> surface = backBuffer->getSurface();
    hidpi::setDeviceScale(surface, device.device_scale);

    Cairo::RefPtr<Cairo::Context> cc = Cairo::Context::create(surface);
    cc->set_source_rgba (0., 0., 0., 0.);
    cc->set_operator (Cairo::OPERATOR_CLEAR);
    cc->paint ();
    cc->set_operator (Cairo::OPERATOR_OVER);
    cc->set_antialias(Cairo::ANTIALIAS_NONE);
    cc->set_line_join(Cairo::LINE_JOIN_MITER);

    Gdk::Cairo::set_source_pixbuf(cc, result.pixbuf(), 0, 0);
    auto pattern = hidpi::getSourceForSurface(cc);
    hidpi::setDeviceScale(pattern->get_surface(), device.device_scale);
    cc->rectangle(0, 0, device.width, device.height);
    cc->fill();

    if (previewHandler->getCropParams().enabled) {
        const auto& cparams = previewHandler->getCropParams();

        CropGuideOverride cropGuideOverride;
        switch (App::get().options().cropGuides) {
            case Options::CROP_GUIDE_NONE:
                cropGuideOverride = CropGuideOverride::NO_GUIDES;
                break;
            case Options::CROP_GUIDE_FRAME:
                cropGuideOverride = CropGuideOverride::FRAME;
                break;
            default:
                cropGuideOverride = CropGuideOverride::DONT_TOUCH;
                break;
        };

        drawCrop(cc, 0, 0, imgW, imgH, imgW, imgH, 0, 0, zoom, device.device_scale,
                 cparams,
                 previewHandler->getCropGuideParams(),
                 cropGuideOverride,
                 false);
    }
}

void PreviewWindow::setPreviewHandler (PreviewHandler* ph)
{

    previewHandler = ph;

    if (previewHandler) {
        previewHandler->addPreviewImageListener (this);
    }
}

void PreviewWindow::on_resized (Gtk::Allocation& req)
{

    updatePreviewImage ();
    queue_draw ();
}

bool PreviewWindow::on_draw(const ::Cairo::RefPtr< Cairo::Context> &cr)
{
    if (!backBuffer) return true;

    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    style->render_background(cr, 0, 0, get_width(), get_height());

    int bufferW, bufferH;
    bufferW = backBuffer->getWidth();
    bufferH = backBuffer->getHeight();

    if (!mainCropWin && imageArea) {
        mainCropWin = imageArea->getMainCropWindow ();

        if (mainCropWin) {
            mainCropWin->addCropWindowListener (this);
        }
    }

    auto deviceSize = hidpi::ScaledDeviceSize::forWidget(this);
    const int scale = deviceSize.device_scale;

    if ((deviceSize.width != bufferW && deviceSize.height != bufferH) || needsUpdate) {
        needsUpdate = false;
        updatePreviewImage ();
    }
    // updatePreviewImage may clear the buffer
    if (!backBuffer || !backBuffer->surfaceCreated()) {
        needsUpdate = true;
        return true;
    }

    cr->save();

    int x_offset = static_cast<double>(deviceSize.width - bufferW) / scale / 2;
    int y_offset = static_cast<double>(deviceSize.height - bufferH) / scale / 2;
    cr->translate(x_offset, y_offset);

    backBuffer->copySurface(cr, nullptr);

    if (mainCropWin && zoom > 0.0) {
        int x, y, w, h;
        getObservedFrameArea (x, y, w, h);
        if (x>0 || y>0 || w < imgW || h < imgH) {
            const double s = scale;
            double rectX = x + 0.5 * s;
            double rectY = y + 0.5 * s;
            double rectW = std::min(w, (int)(imgW - x)) - 1 * s;
            double rectH = std::min(h, (int)(imgH - y)) - 1 * s;

            // draw a black "shadow" line
            cr->set_source_rgba (0.0, 0.0, 0.0, 0.65);
            cr->set_line_width (1 * s);
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
            cr->rectangle (rectX + 1 * s, rectY + 1 * s, rectW - 2 * s, rectH - 2 * s);
            cr->stroke ();

            const auto& options = App::get().options();
            // draw a "frame" line. Color of frame line can be set in preferences
            cr->set_source_rgba(options.navGuideBrush[0], options.navGuideBrush[1], options.navGuideBrush[2], options.navGuideBrush[3]); //( 1.0, 1.0, 1.0, 1.0);
            cr->rectangle (rectX, rectY, rectW, rectH);
            cr->stroke ();
        }
    }

    cr->restore();

    style->render_frame (cr, 0, 0, get_width(), get_height());

    return true;
}

void PreviewWindow::previewImageChanged ()
{

    updatePreviewImage ();
    queue_draw ();
}

void PreviewWindow::setImageArea (ImageArea* ia)
{

    imageArea = ia;
    mainCropWin = ia->getMainCropWindow ();

    if (mainCropWin) {
        mainCropWin->addCropWindowListener (this);
    }
}

void PreviewWindow::cropPositionChanged(CropWindow* w)
{
    queue_draw ();
}

void PreviewWindow::cropWindowSizeChanged(CropWindow* w)
{
    queue_draw ();
}

void PreviewWindow::cropZoomChanged(CropWindow* w)
{
    queue_draw ();
}

void PreviewWindow::initialImageArrived()
{
}

bool PreviewWindow::on_motion_notify_event (GdkEventMotion* event)
{

    if (!mainCropWin) {
        return true;
    }

    int x, y, w, h;
    getObservedFrameArea (x, y, w, h);
    if (x>0 || y>0 || w < imgW || h < imgH) {
        bool inside =     event->x > x - 6 && event->x < x + w - 1 + 6 && event->y > y - 6 && event->y < y + h - 1 + 6;

        CursorShape newType;

        if (isMoving) {
            mainCropWin->remoteMove ((event->x - press_x) / zoom, (event->y - press_y) / zoom);
            press_x = event->x;
            press_y = event->y;
            newType = CSHandClosed;
        } else if (inside) {
            newType = CSHandOpen;
        } else {
            newType = CSArrow;
        }

        if (newType != cursor_type) {
            cursor_type = newType;
            CursorManager::setWidgetCursor(get_window(), cursor_type);
        }
    }

    return true;
}

bool PreviewWindow::on_button_press_event (GdkEventButton* event)
{

    if (!mainCropWin) {
        return true;
    }

    int x, y, w, h;
    getObservedFrameArea (x, y, w, h);
    if (x>0 || y>0 || w < imgW || h < imgH) {

        if (!isMoving) {
            isMoving = true;

            press_x = event->x;
            press_y = event->y;

            if (cursor_type != CSHandClosed) {
                cursor_type = CSHandClosed;
                CursorManager::setWidgetCursor(get_window(), cursor_type);
            }
        }
    }

    return true;
}

bool PreviewWindow::on_button_release_event (GdkEventButton* event)
{

    if (!mainCropWin) {
        return true;
    }

    if (isMoving) {
        isMoving = false;

        if (cursor_type != CSArrow) {
            cursor_type = CSArrow;
            CursorManager::setWidgetCursor(get_window(), cursor_type);
        }

        mainCropWin->remoteMoveReady ();
    }

    return true;
}

Gtk::SizeRequestMode PreviewWindow::get_request_mode_vfunc () const
{
    return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
}

void PreviewWindow::get_preferred_height_vfunc (int &minimum_height, int &natural_height) const
{
    minimum_height= RTScalable::scalePixelSize(50);
    natural_height = RTScalable::scalePixelSize(100);
}

void PreviewWindow::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    minimum_width = RTScalable::scalePixelSize(80);
    natural_width = RTScalable::scalePixelSize(120);
}

void PreviewWindow::get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const
{
    get_preferred_height_vfunc(minimum_height, natural_height);
}

void PreviewWindow::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    get_preferred_width_vfunc (minimum_width, natural_width);
}
