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

#include "cropguilistener.h"
#include "cropwindow.h"
#include "editcallbacks.h"
#include "editenums.h"
#include "imageareapanel.h"
#include "imageareatoollistener.h"
#include "indclippedpanel.h"
#include "previewhandler.h"
#include "previewmodepanel.h"
#include "toolbar.h"
#include "zoompanel.h"
#include "widgets/basic/backbuffer.h"

class ImageAreaPanel;

class ImageArea final :
    public Gtk::DrawingArea,
    public CropWindowListener,
    public EditDataProvider,
    public LockablePickerToolListener
{

    friend class ZoomPanel;

protected:

    Glib::ustring infotext;
    Glib::RefPtr<Pango::Layout> deglayout;
    BackBuffer iBackBuffer;
    int backBufferDeviceScale;
    bool showClippedH, showClippedS;

    ImageAreaPanel* parent;

    std::list<CropWindow*> cropWins;
    PreviewHandler* previewHandler;
    rtengine::StagedImageProcessor* ipc;

    bool        dirty;
    CropWindow* focusGrabber;
    CropGUIListener* cropgl;
    PointerMotionListener* pmlistener;
    PointerMotionListener* pmhlistener;
    ImageAreaToolListener* listener;

    CropWindow* getCropWindow (int x, int y);
    Gtk::SizeRequestMode get_request_mode_vfunc () const override;
    void get_preferred_height_vfunc (int &minimum_height, int &natural_height) const override;
    void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const override;
    void get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const override;
    void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const override;

    int fullImageWidth, fullImageHeight;
public:
    CropWindow* mainCropWindow;
    CropWindow* flawnOverWindow;
    ZoomPanel* zoomPanel;
    IndicateClippedPanel* indClippedPanel;
    PreviewModePanel* previewModePanel;
    ImageArea* iLinkedImageArea; // used to set a reference to the Before image area, which is set when before/after view is enabled

    explicit ImageArea (ImageAreaPanel* p);
    ~ImageArea () override;

    rtengine::StagedImageProcessor* getImProcCoordinator() const;
    void setImProcCoordinator(rtengine::StagedImageProcessor* ipc_);
    void setPreviewModePanel(PreviewModePanel* previewModePanel_)
    {
        previewModePanel = previewModePanel_;
    };
    void setIndicateClippedPanel(IndicateClippedPanel* indClippedPanel_)
    {
        indClippedPanel = indClippedPanel_;
    };

    void getScrollImageSize (int& w, int& h);
    void getScrollPosition  (int& x, int& y);
    void setScrollPosition  (int x, int y);     // called by the imageareapanel when the scrollbars have been changed

    // enabling and setting text of info area
    void setInfoText (Glib::ustring&& text);
    void updateInfoTextBackBuffer();
    void infoEnabled (bool e);

    // widget base events
    void on_realize () override;
    bool on_draw                 (const ::Cairo::RefPtr< Cairo::Context> &cr) override;
    bool on_motion_notify_event  (GdkEventMotion* event) override;
    bool on_button_press_event   (GdkEventButton* event) override;
    bool on_button_release_event (GdkEventButton* event) override;
    bool on_scroll_event         (GdkEventScroll* event) override;
    bool on_leave_notify_event   (GdkEventCrossing* event) override;
    void on_resized              (Gtk::Allocation& req);
    void on_style_updated        () override;
    void syncBeforeAfterViews    ();

    void            setCropGUIListener       (CropGUIListener* l);
    void            setPointerMotionListener  (PointerMotionListener* pml);
    void            setPointerMotionHListener (PointerMotionListener* pml);
    void            setImageAreaToolListener (ImageAreaToolListener* l)
    {
        listener = l;
    }
    void            setPreviewHandler        (PreviewHandler* ph);
    PreviewHandler* getPreviewHandler        ()
    {
        return previewHandler;
    }

    void grabFocus          (CropWindow* cw);
    void unGrabFocus        ();
    void addCropWindow      ();
    void cropWindowSelected (CropWindow* cw);
    void cropWindowClosed   (CropWindow* cw);
    ToolMode getToolMode    ();
    bool showColorPickers   ();
    void setToolHand        ();
    void straightenReady    (double rotDeg);
    void spotWBSelected     (int x, int y);
    void sharpMaskSelected  (bool sharpMask);
    int  getSpotWBRectSize  ();
    void redraw             ();

    void zoomFit     ();
    double getZoom   ();
    void   setZoom   (double zoom);

    // EditDataProvider interface
    void subscribe(EditSubscriber *subscriber) override;
    void unsubscribe() override;
    void getImageSize (int &w, int&h) override;
    void getPreviewCenterPos(int &x, int &y) override;
    void getPreviewSize(int &w, int &h) override;

    // CropWindowListener interface
    void cropPositionChanged   (CropWindow* cw) override;
    void cropWindowSizeChanged (CropWindow* cw) override;
    void cropZoomChanged       (CropWindow* cw) override;
    void initialImageArrived   () override;

    // LockablePickerToolListener interface
    void switchPickerVisibility (bool isVisible) override;

    CropWindow* getMainCropWindow ()
    {
        return mainCropWindow;
    }
};
