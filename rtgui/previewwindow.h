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

#include <gtkmm.h>

#include "cropwindow.h"
#include "cursormanager.h"
#include "guiutils.h"
#include "previewhandler.h"

class BackBuffer;

class PreviewWindow :
    public Gtk::DrawingArea,
    public PreviewListener,
    public CropWindowListener
{

private:
    Cairo::RefPtr<BackBuffer> backBuffer;
    PreviewHandler* previewHandler;
    sigc::connection rconn;
    CropWindow* mainCropWin;
    ImageArea* imageArea;
    int imgW, imgH;
    double zoom;
    double press_x, press_y;
    bool isMoving;
    bool needsUpdate;
    CursorShape cursor_type;

    void updatePreviewImage     ();
    void getObservedFrameArea   (int& x, int& y, int& w, int& h);

public:
    PreviewWindow();
    ~PreviewWindow();

    void setPreviewHandler  (PreviewHandler* ph);
    void setImageArea       (ImageArea* ia);

    void on_realize             () override;
    void on_resized             (Gtk::Allocation& req);
    bool on_draw                (const ::Cairo::RefPtr< Cairo::Context> &cr) override;
    bool on_motion_notify_event (GdkEventMotion* event) override;
    bool on_button_press_event  (GdkEventButton* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
    Gtk::SizeRequestMode get_request_mode_vfunc () const override;
    void get_preferred_height_vfunc (int& minimum_height, int& natural_height) const override;
    void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const override;
    void get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const override;
    void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const override;

    // PreviewListener interface
    void previewImageChanged () override;

    // CropWindowListener interface
    void cropPositionChanged(CropWindow* w) override;
    void cropWindowSizeChanged(CropWindow* w) override;
    void cropZoomChanged(CropWindow* w) override;
    void initialImageArrived() override;
};
