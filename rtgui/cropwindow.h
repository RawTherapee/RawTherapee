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

#include <list>

#include <gtkmm.h>

#include "cropguilistener.h"
#include "crophandler.h"
#include "cursormanager.h"
#include "editbuffer.h"
#include "editcoordsys.h"
#include "editenums.h"
#include "hidpi.h"
#include "widgets/basic/lwbutton.h"
#include "widgets/basic/lwbuttonset.h"

#include "rtengine/noncopyable.h"

namespace rtengine
{

struct Coord;

}

class CropWindow;
class PointerMotionListener;

class CropWindowListener
{
public:
    virtual ~CropWindowListener() = default;
    virtual void cropPositionChanged(CropWindow*) = 0;
    virtual void cropWindowSizeChanged(CropWindow*) = 0;
    virtual void cropZoomChanged(CropWindow*) = 0;
    virtual void initialImageArrived() = 0;
};

class ImageArea;
class CropWindow final : public LWButtonListener, public CropDisplayHandler, public EditCoordSystem, public ObjectMOBuffer, public rtengine::NonCopyable
{
    static bool initialized;

    static Glib::ustring zoomOuttt;
    static Glib::ustring zoomIntt;
    static Glib::ustring zoom100tt;
    static Glib::ustring closett;

    // state management
    ImgEditState state;                  // current state of user (see enum State)
    int press_x, press_y;               // position of the cursor in the GUI space on button press
    int action_x, action_y;             // parameter that will evolve during a pan or drag action
    int pickedObject;
    int pickModifierKey;
    double rot_deg;
    bool onResizeArea;
    bool deleted;
    bool fitZoomEnabled;
    bool fitZoom;
    //bool isLowUpdatePriority;
    CursorShape cursor_type;

    // color pickers
    std::vector<LockableColorPicker*> colorPickers;
    LockableColorPicker* hoveredPicker;

    // decoration
    LWButton *bZoomIn, *bZoomOut, *bZoom100, /**bZoomFit,*/ *bClose;
    LWButtonSet buttonSet;
    Glib::ustring cropLabel;
    int backColor;
    bool decorated;
    bool isFlawnOver;

    // crop frame description
    int titleHeight, sideBorderWidth, lowerBorderWidth, upperBorderWidth, sepWidth, minWidth;
    // size & position of the crop relative to the top left corner
    // of the main preview area
    hidpi::LogicalCoord windowPos;
    hidpi::LogicalSize windowSize;
    // size & pos of the drawable area relative to the top left corner of the crop
    hidpi::LogicalCoord imgAreaPos;
    hidpi::LogicalSize imgAreaSize;
    // size & pos of the piece of preview image relative to the top left corner of the crop
    hidpi::LogicalCoord imgPos;
    hidpi::DeviceSize imgSize;

    // image handling

    ImageArea* iarea;
    int cropZoom; // *1000
    unsigned int zoomVersion, exposeVersion;

    // crop gui listener
    CropGUIListener* cropgl;
    PointerMotionListener* pmlistener;
    PointerMotionListener* pmhlistener;
    std::list<CropWindowListener*> listeners;
    double scrollAccum;

    CropWindow* observedCropWin;  // Pointer to the currently active detail CropWindow

    float crop_custom_ratio;

    bool onArea                    (CursorArea a, int x, int y);
    void updateCursor              (int x, int y);
    void drawDecoration            (Cairo::RefPtr<Cairo::Context> cr);
    void drawStraightenGuide       (Cairo::RefPtr<Cairo::Context> cr);
    void drawScaledSpotRectangle   (Cairo::RefPtr<Cairo::Context> cr, int rectSize);
    void drawUnscaledSpotRectangle (Cairo::RefPtr<Cairo::Context> cr, int rectSize);
    void drawObservedFrame         (const Cairo::RefPtr<Cairo::Context>& cr);
    void changeZoom                (int zoom, bool notify = true, int centerx = -1, int centery = -1, bool needsRedraw = true);
    void updateHoveredPicker       (rtengine::Coord *imgPos = nullptr);
    void cycleRGB                  ();
    void cycleHSV                  ();

    LockableColorPicker::Validity checkValidity (LockableColorPicker*  picker, const rtengine::Coord &pos);

    // Used by the mainCropWindow only
    void getObservedFrameArea      (int& x, int& y, int& w, int& h) const;

    struct ZoomStep {
        Glib::ustring label;
        double zoom;
        int czoom;
        bool is_major;

        explicit ZoomStep(const Glib::ustring &l="", double z=0.0,
                          int cz=0, bool m=false):
            label(l), zoom(z), czoom(cz), is_major(m) {}
    };
    std::vector<ZoomStep> zoomSteps;
    size_t zoom11index;

    void initZoomSteps();
    
public:
    CropHandler cropHandler;
    CropWindow (ImageArea* parent, bool isLowUpdatePriority_, bool isDetailWindow);
    ~CropWindow () override;

    void setDecorated       (bool decorated)
    {
        this->decorated = decorated;
    }
    void setFitZoomEnabled  (bool fze)
    {
        fitZoomEnabled = fze;
    }
    void setObservedCropWin (CropWindow* cw)
    {
        observedCropWin = cw;
    }
    void deleteColorPickers ();

    void screenCoordToCropBuffer (double phyx, double phyy, int& cropx, int& cropy) override;
    void screenCoordToImage (double phyx, double phyy, int& imgx, int& imgy) override;
    void screenCoordToCropCanvas (double phyx, double phyy, int& prevx, int& prevy);
    void imageCoordToCropCanvas (int imgx, int imgy, int& phyx, int& phyy) override;
    void imageCoordToScreen (int imgx, int imgy, int& phyx, int& phyy) override;
    void imageCoordToCropBuffer (int imgx, int imgy, int& phyx, int& phyy) override;
    void imageCoordToCropImage (int imgx, int imgy, int& phyx, int& phyy) override;
    int scaleValueToImage (int value) override;
    float scaleValueToImage (float value) override;
    double scaleValueToImage (double value) override;
    int scaleValueToCanvas (int value) override;
    float scaleValueToCanvas (float value) override;
    double scaleValueToCanvas (double value) override;
    double getZoomFitVal ();
    void setPosition (int x, int y);
    void getPosition (int& x, int& y);
    void setSize     (int w, int h, bool norefresh = false);
    void getSize     (int& w, int& h);
    void enable      ();

    void leaveNotify (GdkEventCrossing* event);
    void flawnOver   (bool isFlawnOver);

    // zoomlistener interface
    void zoomIn      (bool toCursor = false, int cursorX = -1, int cursorY = -1);
    void zoomOut     (bool toCursor = false, int cursorX = -1, int cursorY = -1);
    void zoom11      (bool notify = true);
    void zoomFit     ();
    void zoomFitCrop ();
    double getZoom   ();
    bool isMinZoom   ();
    bool isMaxZoom   ();
    void setZoom     (double zoom);

    bool isInside    (int x, int y);


    void scroll        (int state, GdkScrollDirection direction, int x, int y, double deltaX=0.0, double deltaY=0.0);
    void buttonPress   (int button, int num, int state, int x, int y);
    void buttonRelease (int button, int num, int state, int x, int y);
    void pointerMoved  (int bstate, int x, int y);

    void expose        (Cairo::RefPtr<Cairo::Context> cr);

    void setEditSubscriber (EditSubscriber* newSubscriber);

    // interface lwbuttonlistener
    void buttonPressed (LWButton* button, int actionCode, void* actionData, int x, int y) override;
    void redrawNeeded  (LWButton* button) override;

    // crop handling
    void getCropRectangle      (int& x, int& y, int& w, int& h) const;
    void getCropPosition       (int& x, int& y);
    void setCropPosition       (int x, int y, bool update = true);
    void centerCrop            (bool update = true);
    void getCropSize           (int& w, int& h);
    void getCropAnchorPosition (int& w, int& h);
    void setCropAnchorPosition (int w, int h);

    // listeners
    void setCropGUIListener       (CropGUIListener* cgl);
    void setPointerMotionListener (PointerMotionListener* pml);
    PointerMotionListener* getPointerMotionListener ();
    void setPointerMotionHListener (PointerMotionListener* pml);

    // crop window listeners
    void addCropWindowListener (CropWindowListener* l);
    void delCropWindowListener (CropWindowListener* l);

    // crophandlerlistener interface
    void cropImageUpdated () override;
    void cropWindowChanged () override;
    void initialImageArrived () override;
    void setDisplayPosition (hidpi::LogicalCoord pos) override;

    void remoteMove      (int deltaX, int deltaY);
    void remoteMoveReady ();

    ImageArea* getImageArea();
};
