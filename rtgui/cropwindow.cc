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
#include <iomanip>

#include "cropwindow.h"

#include "cursormanager.h"
#include "drawcropguide.h"
#include "guiutils.h"
#include "imagearea.h"
#include "lockablecolorpicker.h"
#include "options.h"
#include "rtimage.h"
#include "threadutils.h"
#include "editcallbacks.h"
#include "editbuffer.h"
#include "editwidgets.h"
#include "pointermotionlistener.h"
#include "rtsurface.h"

#include "rtengine/dcrop.h"
#include "rtengine/imagesource.h"
#include "rtengine/procparams.h"
#include "rtengine/rt_math.h"

#ifdef _WIN32
#include "rtengine/leanwindows.h"
#endif // _WIN32

using namespace rtengine;

namespace {
    inline double zoomLimitToFraction(Options::MaxZoom z) {
        switch (z) {
          case Options::MaxZoom::PERCENTS_100:
            return 1.;
          case Options::MaxZoom::PERCENTS_200:
            return 2.;
          case Options::MaxZoom::PERCENTS_300:
            return 3.;
          case Options::MaxZoom::PERCENTS_400:
            return 4.;
          case Options::MaxZoom::PERCENTS_500:
            return 5.;
          case Options::MaxZoom::PERCENTS_600:
            return 6.;
          case Options::MaxZoom::PERCENTS_700:
            return 7.;
          case Options::MaxZoom::PERCENTS_800:
            return 8.;
          case Options::MaxZoom::PERCENTS_1600:
          default:
            return 16.;
        }
    }
}

bool CropWindow::initialized = false;

Glib::ustring CropWindow::zoomOuttt;
Glib::ustring CropWindow::zoomIntt;
Glib::ustring CropWindow::zoom100tt;
Glib::ustring CropWindow::closett;

CropWindow::CropWindow (ImageArea* parent, bool isLowUpdatePriority_, bool isDetailWindow)
    : ObjectMOBuffer(parent), state(SNormal), press_x(0), press_y(0), action_x(0), action_y(0), pickedObject(-1), pickModifierKey(0), rot_deg(0), onResizeArea(false), deleted(false),
      fitZoomEnabled(true), fitZoom(false), cursor_type(CSArrow), /*isLowUpdatePriority(isLowUpdatePriority_),*/ hoveredPicker(nullptr), cropLabel(Glib::ustring("100%")),
      backColor(App::get().options().bgcolor), decorated(true), isFlawnOver(false), titleHeight(30), sideBorderWidth(3), lowerBorderWidth(3),
      upperBorderWidth(1), sepWidth(2), windowPos(30, 30), windowSize(0, 0), imgAreaPos(0, 0), imgAreaSize(0, 0),
      imgPos(-1, -1), imgSize(1, 1), iarea(parent), cropZoom(0), zoomVersion(0), exposeVersion(0), cropgl(nullptr),
      pmlistener(nullptr), pmhlistener(nullptr), scrollAccum(0.0), observedCropWin(nullptr),
      crop_custom_ratio(0.f)
{
    initZoomSteps();

    Glib::RefPtr<Pango::Context> context = parent->get_pango_context () ;
    Pango::FontDescription fontd = parent->get_style_context()->get_font();
    fontd.set_weight (Pango::WEIGHT_BOLD);
    const int fontSize = 8; // pt
    // Non-absolute size is defined in "Pango units" and shall be multiplied by
    // Pango::SCALE from "pt":
    fontd.set_size (fontSize * Pango::SCALE);
    context->set_font_description (fontd);
    Glib::RefPtr<Pango::Layout> cllayout = parent->create_pango_layout("1000%");

    int iw, ih;
    cllayout->get_pixel_size (iw, ih);

    titleHeight = ih;

    if (!initialized) {
        zoomOuttt = "Zoom Out";
        zoomIntt = "Zoom In";
        zoom100tt = "Zoom 100/%";
        closett = "Close";
        initialized = true;
    }
    bZoomOut = new LWButton(std::shared_ptr<RTSurface>(new RTSurface("magnifier-minus-small", Gtk::ICON_SIZE_BUTTON)), 0, nullptr, LWButton::Left, LWButton::Center, &zoomOuttt);
    bZoomIn  = new LWButton(std::shared_ptr<RTSurface>(new RTSurface("magnifier-plus-small", Gtk::ICON_SIZE_BUTTON)), 1, nullptr, LWButton::Left, LWButton::Center, &zoomIntt);
    bZoom100 = new LWButton(std::shared_ptr<RTSurface>(new RTSurface("magnifier-1to1-small", Gtk::ICON_SIZE_BUTTON)), 2, nullptr, LWButton::Left, LWButton::Center, &zoom100tt);
     //bZoomFit = new LWButton (std::shared_ptr<RTSurface>(new RTSurface("magnifier-fit", Gtk::ICON_SIZE_BUTTON)), 3, NULL, LWButton::Left, LWButton::Center, "Zoom Fit");
    bClose   = new LWButton(std::shared_ptr<RTSurface>(new RTSurface("cancel-small", Gtk::ICON_SIZE_BUTTON)), 4, nullptr, LWButton::Right, LWButton::Center, &closett);

    buttonSet.add (bZoomOut);
    buttonSet.add (bZoomIn);
    buttonSet.add (bZoom100);
    buttonSet.add (bClose);

    buttonSet.setColors (Gdk::RGBA("black"), Gdk::RGBA("white"));
    buttonSet.setButtonListener (this);

    int bsw, bsh;
    buttonSet.getMinimalDimensions (bsw, bsh);

    if (bsh > titleHeight) {
        titleHeight = bsh;
    }

    minWidth = bsw + iw + 2 * sideBorderWidth;

    cropHandler.setDisplayHandler(this);
    cropHandler.newImage (parent->getImProcCoordinator(), isDetailWindow);
}

CropWindow::~CropWindow ()
{
    for (auto colorPicker : colorPickers) {
        delete colorPicker;
    }
}


void CropWindow::initZoomSteps()
{
    zoomSteps.push_back(ZoomStep("  1%", 0.01, 999, true));
    zoomSteps.push_back(ZoomStep("  2%", 0.02, 500, true));
    zoomSteps.push_back(ZoomStep("  5%", 0.05, 200, true));
    zoomSteps.push_back(ZoomStep("  6%", 1.0/15.0, 150, true));
    zoomSteps.push_back(ZoomStep("  8%", 1.0/12.0, 120, true));
    char lbl[64];
    for (int s = 100; s >= 11; --s) {
        float z = 10.f / s;
        snprintf(lbl, sizeof(lbl), "% 2d%%", int(z * 100));
        bool is_major = (s == s/10 * 10);
        zoomSteps.push_back(ZoomStep(lbl, z, s, is_major));
    }
    zoom11index = zoomSteps.size();
    for (int s = 1; s <= 8; ++s) {
        snprintf(lbl, sizeof(lbl), "%d00%%", s);
        zoomSteps.push_back(ZoomStep(lbl, s, s * 1000, true));
    }
    zoomSteps.push_back(ZoomStep("1600%", 16, 16000, true));
}

void CropWindow::enable()
{
    cropHandler.setEnabled (true);
}

void CropWindow::setPosition (int x, int y)
{

    if (y < 0) {
        y = 0;
    }

    windowPos.x = x;
    windowPos.y = y;

    if (decorated) {
        buttonSet.arrangeButtons (windowPos.x + sideBorderWidth, windowPos.y + upperBorderWidth, windowSize.width - 2 * sideBorderWidth, titleHeight);
    }
}

void CropWindow::getPosition (int& x, int& y)
{

    x = windowPos.x;
    y = windowPos.y;
}

void CropWindow::getCropPosition (int& x, int& y)
{
    ImageCoord cropPos = cropHandler.getPosition();

    if (state != SCropImgMove) {
        x = cropPos.x;
        y = cropPos.y;
    } else {
        x = cropPos.x + action_x;
        y = cropPos.y + action_y;
    }
}

void CropWindow::getCropRectangle (int& x, int& y, int& w, int& h) const
{
    ImageCoord pos = cropHandler.getPosition();
    x = pos.x;
    y = pos.y;
    ImageSize size = cropHandler.getSize();
    w = size.width;
    h = size.height;
}

void CropWindow::setCropPosition (int x, int y, bool update)
{

    cropHandler.setAnchorPosition (ImageCoord(x, y), update);

    for (auto listener : listeners) {
        listener->cropPositionChanged (this);
    }
}

void CropWindow::centerCrop (bool update)
{

    cropHandler.centerAnchor (update);

    for (auto listener : listeners) {
        listener->cropPositionChanged (this);
    }
}

void CropWindow::setSize (int w, int h, bool norefresh)
{

    windowSize.width = w;
    windowSize.height = h;

    fitZoom = false;

    if (windowSize.width < minWidth) {
        windowSize.width = minWidth;
    }

    if (windowSize.height < 64) {
        windowSize.height = 64;
    }

    if (decorated) {
        imgAreaPos.x = sideBorderWidth;
        imgAreaPos.y = upperBorderWidth + titleHeight + sepWidth;
        imgAreaSize.width = windowSize.width - 2 * sideBorderWidth;
        imgAreaSize.height = windowSize.height - lowerBorderWidth - titleHeight - sepWidth - upperBorderWidth;
        buttonSet.arrangeButtons (windowPos.x + sideBorderWidth, windowPos.y + upperBorderWidth, windowSize.width - 2 * sideBorderWidth, titleHeight);
    } else {
        imgAreaPos.x = imgAreaPos.y = 0;
        imgAreaSize.width = windowSize.width;
        imgAreaSize.height = windowSize.height;
    }

    if (!norefresh) {
        int deviceScale = RTScalable::getScaleForWidget(iarea);
        hidpi::ScaledDeviceSize deviceSize = imgAreaSize.scaleToDevice(deviceScale);
        ObjectMOBuffer::resize(deviceSize.width, deviceSize.height);
        cropHandler.setWSize(imgAreaSize);
        cropHandler.setDeviceScale(deviceScale);
    }
}

void CropWindow::getSize (int& w, int& h)
{

    w = windowSize.width;
    h = windowSize.height;
}

void CropWindow::getCropSize (int& w, int& h)
{

    w = imgAreaSize.width;
    h = imgAreaSize.height;
}

void CropWindow::getCropAnchorPosition (int& x, int& y)
{
    ImageCoord pos = cropHandler.getAnchorPosition();
    x = pos.x;
    y = pos.y;
}

void CropWindow::setCropAnchorPosition (int x, int y)
{
    cropHandler.setAnchorPosition(ImageCoord(x, y));
}

bool CropWindow::isInside (int x, int y)
{

    return x >= windowPos.x && x < windowPos.x + windowSize.width && y >= windowPos.y && y < windowPos.y + windowSize.height;
}

void CropWindow::leaveNotify (GdkEventCrossing* event)
{
    EditSubscriber* subscriber = iarea->getCurrSubscriber();

    if (state == SNormal && subscriber && subscriber->getEditingType() == ET_PIPETTE) {
        iarea->setPipetteVal1(-1.f);
        iarea->setPipetteVal2(-1.f);
        iarea->setPipetteVal3(-1.f);

        if (subscriber->mouseOver(0)) {
            iarea->redraw();
        }
    }
}

void CropWindow::flawnOver (bool isFlawnOver)
{
    this->isFlawnOver = isFlawnOver;
}

void CropWindow::scroll (int state, GdkScrollDirection direction, int x, int y, double deltaX, double deltaY)
{
    double delta = 0.0;
    if (std::fabs(deltaX) > std::fabs(deltaY)) {
        delta = deltaX;
    } else {
        delta = deltaY;
    }

    if (direction == GDK_SCROLL_SMOOTH) {
        scrollAccum += delta;
        //Only change zoom level if we've accumulated +/- 1.0 of deltas.  This conditional handles the previous delta=0.0 case
        if (std::fabs(scrollAccum) < 1.0) {
            return;
        }
    }
    bool isUp = direction == GDK_SCROLL_UP || (direction == GDK_SCROLL_SMOOTH && scrollAccum < 0.0);
    scrollAccum = 0.0;
    if ((state & GDK_CONTROL_MASK) && onArea(ColorPicker, x, y)) {
        // resizing a color picker
        if (isUp) {
            hoveredPicker->incSize();
            updateHoveredPicker();
            iarea->redraw ();
        } else {
            hoveredPicker->decSize();
            updateHoveredPicker();
            iarea->redraw ();
        }
    } else {
        // not over a color picker, we zoom in/out
        int newCenterX = x;
        int newCenterY = y;

        screenCoordToImage(newCenterX, newCenterY, newCenterX, newCenterY);

        if (isUp && !isMaxZoom()) {
            zoomIn (true, newCenterX, newCenterY);
        } else if (!isUp && !isMinZoom()) {
            zoomOut (true, newCenterX, newCenterY);
        }
    }
}

void CropWindow::buttonPress (int button, int type, int bstate, int x, int y)
{

    bool needRedraw = true;  // most common case ; not redrawing are exceptions
    const auto editSubscriber = iarea->getCurrSubscriber();

    iarea->grabFocus (this);

    if (button == 1) {
        if (type == GDK_2BUTTON_PRESS && onArea (CropImage, x, y) && iarea->getToolMode () != TMColorPicker && (state == SNormal || state == SCropImgMove)) {
            if (fitZoomEnabled) {
                if (fitZoom) {
                    state = SNormal;
                    zoomVersion = exposeVersion;
                    screenCoordToImage (x, y, action_x, action_y);
                    changeZoom (zoom11index, true, action_x, action_y);
                    fitZoom = false;
                } else if (App::get().options().cropAutoFit) {
                    zoomFitCrop();
                } else {
                    zoomFit();
                }
            } else {
                zoom11 ();
            }

            state = SNormal;
        }
        else if (type == GDK_BUTTON_PRESS && state == SNormal) {
            if (onArea (CropToolBar, x, y)) {
                if (!decorated || !buttonSet.pressNotify (x, y)) {
                    state = SCropWinMove;
                    action_x = x;
                    action_y = y;
                    press_x = windowPos.x;
                    press_y = windowPos.y;
                }
            } else if (onArea (CropResize, x, y)) {
                state = SCropWinResize;
                action_x = x;
                action_y = y;
                press_x = windowSize.width;
                press_y = windowSize.height;
            } else {
                if (onArea (CropImage, x, y)) {  // events inside of the image domain
                    crop_custom_ratio = 0.f;
                    if ((bstate & GDK_SHIFT_MASK) && cropHandler.cropParams->w > 0 && cropHandler.cropParams->h > 0) {
                        crop_custom_ratio = float(cropHandler.cropParams->w) / float(cropHandler.cropParams->h);
                    }

                    if (iarea->getToolMode () == TMColorPicker) {
                        if (hoveredPicker) {
                            if ((bstate & GDK_CONTROL_MASK) && !(bstate & GDK_SHIFT_MASK)) {
                                hoveredPicker->decSize();
                                updateHoveredPicker();
                                needRedraw = true;
                            } else if (!(bstate & GDK_CONTROL_MASK) && (bstate & GDK_SHIFT_MASK)) {
                                hoveredPicker->rollDisplayedValues();
                                needRedraw = true;
                            } else if (!(bstate & GDK_CONTROL_MASK) && !(bstate & GDK_SHIFT_MASK)) {
                                // Color Picker drag starts
                                state = SDragPicker;
                            }
                        } else {
                            // Add a new Color Picker
                            rtengine::Coord imgPos;
                            screenCoordToImage(x, y, imgPos.x, imgPos.y);
                            LockableColorPicker *newPicker = new LockableColorPicker(this, cropHandler.colorParams.get());
                            colorPickers.push_back(newPicker);
                            hoveredPicker = newPicker;
                            updateHoveredPicker(&imgPos);
                            state = SDragPicker;
                            press_x = x;
                            press_y = y;
                            action_x = 0;
                            action_y = 0;
                            needRedraw = true;
                        }
                    } else if ((iarea->getToolMode () == TMHand
                                || iarea->getToolMode() == TMPerspective)
                               && editSubscriber
                               && cropgl
                               && cropgl->inImageArea(iarea->posImage.x, iarea->posImage.y)
                               && editSubscriber->getEditingType() == ET_OBJECTS
                               && iarea->getObject() >= 0
                              )
                    {
                            needRedraw = editSubscriber->button1Pressed(bstate);
                            if (editSubscriber->isDragging()) {
                                state = SEditDrag1;
                            } else if (editSubscriber->isPicking()) {
                                state = SEditPick1;
                                pickedObject = iarea->getObject();
                                pickModifierKey = bstate;
                            } else if (iarea->getToolMode() == TMPerspective) {
                                state = SCropImgMove;
                            }
                            press_x = x;
                            press_y = y;
                            action_x = 0;
                            action_y = 0;
                    } else if (onArea (CropTopLeft, x, y)) {
                        state = SResizeTL;
                        press_x = x;
                        action_x = cropHandler.cropParams->x;
                        press_y = y;
                        action_y = cropHandler.cropParams->y;
                    } else if (onArea (CropTopRight, x, y)) {
                        state = SResizeTR;
                        press_x = x;
                        action_x = cropHandler.cropParams->w;
                        press_y = y;
                        action_y = cropHandler.cropParams->y;
                    } else if (onArea (CropBottomLeft, x, y)) {
                        state = SResizeBL;
                        press_x = x;
                        action_x = cropHandler.cropParams->x;
                        press_y = y;
                        action_y = cropHandler.cropParams->h;
                    } else if (onArea (CropBottomRight, x, y)) {
                        state = SResizeBR;
                        press_x = x;
                        action_x = cropHandler.cropParams->w;
                        press_y = y;
                        action_y = cropHandler.cropParams->h;
                    } else if (onArea (CropTop, x, y)) {
                        state = SResizeH1;
                        press_y = y;
                        action_y = cropHandler.cropParams->y;
                    } else if (onArea (CropBottom, x, y)) {
                        state = SResizeH2;
                        press_y = y;
                        action_y = cropHandler.cropParams->h;
                    } else if (onArea (CropLeft, x, y)) {
                        state = SResizeW1;
                        press_x = x;
                        action_x = cropHandler.cropParams->x;
                    } else if (onArea (CropRight, x, y)) {
                        state = SResizeW2;
                        press_x = x;
                        action_x = cropHandler.cropParams->w;
                    } else if ((bstate & GDK_SHIFT_MASK) && onArea (CropInside, x, y)) {
                        state = SCropMove;
                        press_x = x;
                        press_y = y;
                        action_x = cropHandler.cropParams->x;
                        action_y = cropHandler.cropParams->y;
                    } else if (onArea (CropObserved, x, y)) {
                        state = SObservedMove;
                        press_x = x;
                        press_y = y;
                        action_x = 0;
                        action_y = 0;
                    } else if (iarea->getToolMode () == TMStraighten) {
                        state = SRotateSelecting;
                        press_x = x;
                        press_y = y;
                        action_x = x;
                        action_y = y;
                        rot_deg = 0;
                    } else if (iarea->getToolMode () == TMSpotWB) {
                        int spotx, spoty;
                        screenCoordToImage (x, y, spotx, spoty);
                        iarea->spotWBSelected (spotx, spoty);
                    } else if (iarea->getToolMode () == TMCropSelect && cropgl) {
                        state = SCropSelecting;
                        screenCoordToImage (x, y, press_x, press_y);
                        cropHandler.cropParams->enabled = true;
                        cropHandler.cropParams->x = press_x;
                        cropHandler.cropParams->y = press_y;
                        cropHandler.cropParams->w = cropHandler.cropParams->h = 1;
                        cropgl->cropInit (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h);
                    } else if (iarea->getToolMode () == TMHand) {
                        if (editSubscriber) {
                            if ((cropgl && cropgl->inImageArea(iarea->posImage.x, iarea->posImage.y) && (editSubscriber->getEditingType() == ET_PIPETTE && (bstate & GDK_CONTROL_MASK))) || editSubscriber->getEditingType() == ET_OBJECTS) {
                                needRedraw = editSubscriber->button1Pressed(bstate);
                                if (editSubscriber->isDragging()) {
                                    state = SEditDrag1;
                                } else if (editSubscriber->isPicking()) {
                                    state = SEditPick1;
                                    pickedObject = iarea->getObject();
                                    pickModifierKey = bstate;
                                }
                                press_x = x;
                                press_y = y;
                                action_x = 0;
                                action_y = 0;
                            }
                        }
                        if (state != SEditDrag1 && state != SEditPick1) {
                            state = SCropImgMove;
                            press_x = x;
                            press_y = y;
                            action_x = 0;
                            action_y = 0;
                        }
                    } else { // if(zoomSteps[cropZoom].zoom > cropHandler.getFitZoom()) { // only allow move when image is only partial visible
                        state = SCropImgMove;
                        press_x = x;
                        press_y = y;
                        action_x = 0;
                        action_y = 0;
                    }

                } else if (iarea->getToolMode () == TMHand || iarea->getToolMode() == TMPerspective) {  // events outside of the image domain
                    EditSubscriber *editSubscriber = iarea->getCurrSubscriber();

                    if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
                        needRedraw = editSubscriber->button1Pressed(bstate);

                        if (editSubscriber->isDragging()) {
                            state = SEditDrag1;
                        } else if (editSubscriber->isPicking()) {
                            state = SEditPick1;
                            pickedObject = iarea->getObject();
                            pickModifierKey = bstate;
                        }

                        press_x = x;
                        press_y = y;
                        action_x = 0;
                        action_y = 0;
                    }
                } else if (iarea->getToolMode () == TMColorPicker && hoveredPicker) {
                    if ((bstate & GDK_CONTROL_MASK) && !(bstate & GDK_SHIFT_MASK)) {
                        if (hoveredPicker->decSize()) {
                            updateHoveredPicker();
                            needRedraw = true;
                        }
                    } else if (!(bstate & GDK_CONTROL_MASK) && (bstate & GDK_SHIFT_MASK)) {
                        hoveredPicker->rollDisplayedValues();
                    } else if (!(bstate & GDK_CONTROL_MASK) && !(bstate & GDK_SHIFT_MASK)) {
                        // Color Picker drag starts
                        state = SDragPicker;
                    }
                }
            }
        }
    } else if (button == 2) {
        if (iarea->getToolMode () == TMHand) {
            EditSubscriber *editSubscriber = iarea->getCurrSubscriber();
            if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
                needRedraw = editSubscriber->button2Pressed(bstate);

                if (editSubscriber->isDragging()) {
                    state = SEditDrag2;
                } else if (editSubscriber->isPicking()) {
                    state = SEditPick2;
                    pickedObject = iarea->getObject();
                    pickModifierKey = bstate;
                }

                press_x = x;
                press_y = y;
                action_x = 0;
                action_y = 0;
            }
        }
    } else if (button == 3) {
        if (iarea->getToolMode () == TMHand || iarea->getToolMode() == TMPerspective) {
            EditSubscriber *editSubscriber = iarea->getCurrSubscriber();
            if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
                needRedraw = editSubscriber->button3Pressed(bstate);

                if (editSubscriber->isDragging()) {
                    state = SEditDrag3;
                } else if (editSubscriber->isPicking()) {
                    state = SEditPick3;
                    pickedObject = iarea->getObject();
                    pickModifierKey = bstate;
                }

                press_x = x;
                press_y = y;
                action_x = 0;
                action_y = 0;
            }
        }
        else if (iarea->getToolMode () == TMColorPicker && type == GDK_BUTTON_PRESS && state == SNormal) {
            if (hoveredPicker) {
                if((bstate & GDK_CONTROL_MASK) && (bstate & GDK_SHIFT_MASK)) {
                    // Deleting all pickers !
                    for (auto colorPicker : colorPickers) {
                        delete colorPicker;
                    }
                    colorPickers.clear();
                    hoveredPicker = nullptr;
                    state = SDeletePicker;
                    needRedraw = true;
                } else if ((bstate & GDK_CONTROL_MASK) && !(bstate & GDK_SHIFT_MASK)) {
                    if (hoveredPicker->incSize()) {
                        updateHoveredPicker();
                        needRedraw = true;
                    }
                } else if (!(bstate & GDK_CONTROL_MASK) && !(bstate & GDK_SHIFT_MASK)) {
                    // Deleting the hovered picker
                    for (std::vector<LockableColorPicker*>::iterator i = colorPickers.begin(); i != colorPickers.end(); ++i) {
                        if (*i == hoveredPicker) {
                            colorPickers.erase(i);
                            delete hoveredPicker;
                            hoveredPicker = nullptr;
                            state = SDeletePicker;
                            needRedraw = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (needRedraw) {
        iarea->redraw ();
    }

    updateCursor (x, y);
}

void CropWindow::buttonRelease (int button, int num, int bstate, int x, int y)
{
    auto& options = App::get().mut_options();

    EditSubscriber *editSubscriber = iarea->getCurrSubscriber();

    bool needRedraw = false;

    if (state == SCropWinResize) {
        int newWidth = press_x + x - action_x;
        int newHeight = press_y + y - action_y;
        setSize(newWidth, newHeight);

        if (decorated) {
            options.detailWindowWidth = newWidth;
            options.detailWindowHeight = newHeight;
        }

        state = SNormal;

        for (auto listener : listeners) {
            listener->cropWindowSizeChanged (this);
        }

        needRedraw = true;

        if (fitZoom && options.cropAutoFit) {
            zoomFitCrop();
        }
    } else if (state == SCropWinMove) {
        if (iarea->showColorPickers () && !colorPickers.empty()) {
            needRedraw = true;
        }
    } else if (state == SCropImgMove) {
        cropHandler.update ();

        state = SNormal;

        for (std::list<CropWindowListener*>::iterator i = listeners.begin(); i != listeners.end(); ++i) {
            (*i)->cropPositionChanged (this);
        }

        needRedraw = true;
    } else if (state == SRotateSelecting) {
        iarea->straightenReady (rot_deg);
        iarea->setToolHand ();
        needRedraw = true;
    } else if (state == SObservedMove) {
        observedCropWin->remoteMoveReady ();
        state = SNormal;
        needRedraw = true;
    } else if (state == SEditDrag1 || state == SEditDrag2 || state == SEditDrag3) {
        if (editSubscriber) {
            if (state == SEditDrag1) {
                needRedraw = editSubscriber->button1Released();
            } else if (state == SEditDrag2) {
                needRedraw = editSubscriber->button2Released();
            } else if (state == SEditDrag3) {
                needRedraw = editSubscriber->button3Released();
            }

            rtengine::Crop* crop = static_cast<rtengine::Crop*>(cropHandler.getCrop());
            Coord imgPos;
            action_x = x;
            action_y = y;
            screenCoordToImage (x, y, imgPos.x, imgPos.y);

            iarea->posImage.set(imgPos.x, imgPos.y);
            iarea->posScreen.set(x, y);

            Coord cropPos;
            if (state == SEditDrag1 && editSubscriber->getEditingType() == ET_PIPETTE) {
                screenCoordToCropBuffer (x, y, cropPos.x, cropPos.y);

                iarea->setObject(onArea (CropImage, x, y) && !onArea (CropObserved, x, y) ? 1 : 0);

                //iarea->setObject(cropgl && cropgl->inImageArea(iarea->posImage.x, iarea->posImage.y) ? 1 : 0);
                if (iarea->getObject()) {
                    crop->getPipetteData(cropPos.x, cropPos.y, iarea->getPipetteRectSize());
                    //printf("PipetteData:  %.3f  %.3f  %.3f\n", iarea->pipetteVal[0], iarea->pipetteVal[1], iarea->pipetteVal[2]);
                } else {
                    iarea->setPipetteVal1(-1.f);
                    iarea->setPipetteVal2(-1.f);
                    iarea->setPipetteVal3(-1.f);
                }
            } else if (editSubscriber->getEditingType() == ET_OBJECTS) {
                screenCoordToCropCanvas (x, y, cropPos.x, cropPos.y);
                iarea->setObject(ObjectMOBuffer::getObjectID(cropPos));
            }

            needRedraw |= editSubscriber->mouseOver(bstate);
        } else {
            iarea->setObject(0);
        }

        iarea->deltaImage.set(0, 0);
        iarea->deltaScreen.set(0, 0);
        iarea->deltaPrevImage.set(0, 0);
        iarea->deltaPrevScreen.set(0, 0);
    } else if (state == SEditPick1 || state == SEditPick2 || state == SEditPick3) {
        if (editSubscriber) {
            Coord imgPos;
            action_x = x;
            action_y = y;
            screenCoordToImage (x, y, imgPos.x, imgPos.y);

            iarea->posImage.set (imgPos.x, imgPos.y);
            iarea->posScreen.set (x, y);

            Coord cropPos;
            screenCoordToCropCanvas (x, y, cropPos.x, cropPos.y);

            iarea->setObject(ObjectMOBuffer::getObjectID(cropPos));

            int buttonMask = ((state == SEditPick1) ? GDK_BUTTON1_MASK : 0)
                           | ((state == SEditPick2) ? GDK_BUTTON2_MASK : 0)
                           | ((state == SEditPick3) ? GDK_BUTTON3_MASK : 0);
            bool elemPicked = iarea->getObject() == pickedObject && bstate == (pickModifierKey | buttonMask);

            if        (state == SEditPick1) {
                needRedraw = editSubscriber->pick1 (elemPicked);
            } else if (state == SEditPick2) {
                needRedraw = editSubscriber->pick2 (elemPicked);
            } else if (state == SEditPick3) {
                needRedraw = editSubscriber->pick3 (elemPicked);
            }

            pickedObject = -1;
            iarea->setObject(-1);
            pickModifierKey = 0;

            needRedraw |= editSubscriber->mouseOver (bstate);
        } else {
            iarea->setObject(0);
        }
    } else if (state == SDeletePicker) {
        needRedraw = true;
    } else if (state == SNormal && iarea->getToolMode() == TMColorPicker && !hoveredPicker && button == 3) {
        iarea->setToolHand ();
    }

    if (state != SEditDrag1 && state != SEditDrag2 && state != SEditDrag3) {
        iarea->deltaImage.set(0, 0);
        iarea->deltaScreen.set(0, 0);
        iarea->deltaPrevImage.set(0, 0);
        iarea->deltaPrevScreen.set(0, 0);
    }

    if (cropgl && (state == SCropSelecting || state == SResizeH1 || state == SResizeH2 || state == SResizeW1 || state == SResizeW2 || state == SResizeTL || state == SResizeTR || state == SResizeBL || state == SResizeBR || state == SCropMove)) {
        cropgl->cropManipReady ();
        iarea->setToolHand ();
        needRedraw = true;

        if (fitZoom && options.cropAutoFit) {
            zoomFitCrop();
        }
    }

    if (decorated) {
        buttonSet.releaseNotify (x, y);
    }

    if (deleted) {
        iarea->flawnOverWindow = nullptr;
        delete this;
        return;
    }

    if (state != SDeletePicker && state != SEditDrag3 && state != SEditPick3 && button == 3 && !(bstate & (GDK_SHIFT_MASK|GDK_CONTROL_MASK))) {
        iarea->setPipetteVal1(-1.f);
        iarea->setPipetteVal2(-1.f);
        iarea->setPipetteVal3(-1.f);

        needRedraw = iarea->getObject() == 1;

        if (editSubscriber && editSubscriber->getEditingType() == ET_PIPETTE) {
            editSubscriber->mouseOver(0);
        }

        iarea->setToolHand ();
    }

    state = SNormal;
    iarea->grabFocus (nullptr);

    if (needRedraw) {
        iarea->redraw ();
    }

    updateCursor (x, y);
}

void CropWindow::pointerMoved (int bstate, int x, int y)
{

    EditSubscriber *editSubscriber = iarea->getCurrSubscriber();

    if (state == SCropWinMove) {
        setPosition (press_x + x - action_x, press_y + y - action_y);
        iarea->redraw ();
    } else if (state == SCropWinResize) {
        setSize (press_x + x - action_x, press_y + y - action_y, true);

        for (auto listener : listeners) {
            listener->cropWindowSizeChanged (this);
        }

        iarea->redraw ();
    } else if (state == SCropImgMove) {
        const auto& options = App::get().options();
        // multiplier is the amplification factor ; disabled if the user selected "1" (no amplification)
        double factor = options.panAccelFactor == 1 ? 1.0 : options.panAccelFactor * zoomSteps[cropZoom].zoom;

        // never move the preview slower than the cursor
        if (factor < 1.0) {
            factor = 1.0;
        }
        factor *= cropHandler.getDeviceScale();

        int newAction_x = (press_x - x) / zoomSteps[cropZoom].zoom * factor;
        int newAction_y = (press_y - y) / zoomSteps[cropZoom].zoom * factor;

        int deltaX = newAction_x - action_x;
        int deltaY = newAction_y - action_y;

        action_x =  newAction_x;
        action_y =  newAction_y;

        cropHandler.moveAnchor(ImageCoord(deltaX, deltaY), false);

        for (auto listener : listeners) {
            listener->cropPositionChanged (this);
        }

        iarea->redraw ();
    } else if (state == SRotateSelecting) {
        action_x = x;
        action_y = y;
        iarea->redraw ();
    } else if (state == SNormal && iarea->getToolMode () == TMSpotWB) {
        action_x = x;
        action_y = y;
        iarea->redraw ();
    } else if (state == SResizeH1 && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        int oy = cropHandler.cropParams->y;
        cropHandler.cropParams->y = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->h += oy - cropHandler.cropParams->y;
        cropgl->cropHeight1Resized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeH2 && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        cropHandler.cropParams->h = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropgl->cropHeight2Resized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeW1 && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        int ox = cropHandler.cropParams->x;
        cropHandler.cropParams->x = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->w += ox - cropHandler.cropParams->x;
        cropgl->cropWidth1Resized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeW2 && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        cropHandler.cropParams->w = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        cropgl->cropWidth2Resized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeTL && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        int ox = cropHandler.cropParams->x;
        cropHandler.cropParams->x = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->w += ox - cropHandler.cropParams->x;
        int oy = cropHandler.cropParams->y;
        cropHandler.cropParams->y = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->h += oy - cropHandler.cropParams->y;
        cropgl->cropTopLeftResized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeTR && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        cropHandler.cropParams->w = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        int oy = cropHandler.cropParams->y;
        cropHandler.cropParams->y = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->h += oy - cropHandler.cropParams->y;
        cropgl->cropTopRightResized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeBL && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        int ox = cropHandler.cropParams->x;
        cropHandler.cropParams->x = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->w += ox - cropHandler.cropParams->x;
        cropHandler.cropParams->h = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropgl->cropBottomLeftResized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SResizeBR && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        cropHandler.cropParams->w = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->h = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropgl->cropBottomRightResized (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h, crop_custom_ratio);
        iarea->redraw ();
    } else if (state == SCropMove && cropgl) {
        int deviceScale = cropHandler.getDeviceScale();
        cropHandler.cropParams->x = action_x + (x - press_x) / zoomSteps[cropZoom].zoom * deviceScale;
        cropHandler.cropParams->y = action_y + (y - press_y) / zoomSteps[cropZoom].zoom * deviceScale;
        cropgl->cropMoved (cropHandler.cropParams->x, cropHandler.cropParams->y, cropHandler.cropParams->w, cropHandler.cropParams->h);
        iarea->redraw ();
    } else if (state == SCropSelecting && cropgl) {
        screenCoordToImage (x, y, action_x, action_y);
        int cx1 = press_x, cy1 = press_y;
        int cx2 = action_x, cy2 = action_y;
        cropgl->cropResized (cx1, cy1, cx2, cy2);

        if (cx2 > cx1) {
            cropHandler.cropParams->x = cx1;
            cropHandler.cropParams->w = cx2 - cx1 + 1;
        } else {
            cropHandler.cropParams->x = cx2;
            cropHandler.cropParams->w = cx1 - cx2 + 1;
        }

        if (cy2 > cy1) {
            cropHandler.cropParams->y = cy1;
            cropHandler.cropParams->h = cy2 - cy1 + 1;
        } else {
            cropHandler.cropParams->y = cy2;
            cropHandler.cropParams->h = cy1 - cy2 + 1;
        }

        iarea->redraw ();
    } else if (state == SObservedMove) {
        int new_action_x = x - press_x;
        int new_action_y = y - press_y;
        int deviceScale = cropHandler.getDeviceScale();
        observedCropWin->remoteMove ((new_action_x - action_x) / zoomSteps[cropZoom].zoom * deviceScale,
                                     (new_action_y - action_y) / zoomSteps[cropZoom].zoom * deviceScale);
        action_x = new_action_x;
        action_y = new_action_y;
        iarea->redraw ();
    } else if (state == SDragPicker) {
        Coord imgPos;
        action_x = x - press_x;
        action_y = y - press_y;
        screenCoordToImage (x, y, imgPos.x, imgPos.y);
        if (imgPos.x < 0) {
            imgPos.x = 0;
        }else if (imgPos.x >= iarea->getImProcCoordinator()->getFullWidth()) {
            imgPos.x = iarea->getImProcCoordinator()->getFullWidth()-1;
        }
        if (imgPos.y < 0) {
            imgPos.y = 0;
        }else if (imgPos.y >= iarea->getImProcCoordinator()->getFullHeight()) {
            imgPos.y = iarea->getImProcCoordinator()->getFullHeight()-1;
        }
        updateHoveredPicker (&imgPos);
        iarea->redraw ();
    } else if (state == SNormal && iarea->getToolMode () == TMColorPicker && onArea(ColorPicker, x, y)) {
        // TODO: we could set the hovered picker as Highlighted here
        // Keep this if statement, the onArea will find out the hoveredPicker and will be used to update the cursor
    } else if (editSubscriber) {
        rtengine::Crop* crop = static_cast<rtengine::Crop*>(cropHandler.getCrop());

        if (state == SNormal || state == SEditPick1 || state == SEditPick2 || state == SEditPick3) {
            Coord imgPos;
            action_x = x;
            action_y = y;
            screenCoordToImage (x, y, imgPos.x, imgPos.y);

            iarea->posImage.set(imgPos.x, imgPos.y);
            iarea->posScreen.set(x, y);

            Coord cropPos;

            if (editSubscriber->getEditingType() == ET_PIPETTE) {
                screenCoordToCropBuffer (x, y, cropPos.x, cropPos.y);

                iarea->setObject(onArea (CropImage, x, y) && !onArea (CropObserved, x, y) ? 1 : 0);

                //iarea->setObject(cropgl && cropgl->inImageArea(iarea->posImage.x, iarea->posImage.y) ? 1 : 0);
                if (iarea->getObject()) {
                    crop->getPipetteData(cropPos.x, cropPos.y, iarea->getPipetteRectSize());
                    //printf("PipetteData:  %.3f  %.3f  %.3f\n", iarea->pipetteVal[0], iarea->pipetteVal[1], iarea->pipetteVal[2]);
                } else {
                    iarea->setPipetteVal1(-1.f);
                    iarea->setPipetteVal2(-1.f);
                    iarea->setPipetteVal3(-1.f);
                }
            } else if (editSubscriber->getEditingType() == ET_OBJECTS) {
                screenCoordToCropCanvas (x, y, cropPos.x, cropPos.y);
                iarea->setObject(ObjectMOBuffer::getObjectID(cropPos));
            }

            if (editSubscriber->mouseOver(bstate)) {
                iarea->redraw ();
            }
        } else if (state == SEditDrag1 || state == SEditDrag2 || state == SEditDrag3) {
            Coord currPos;
            action_x = x;
            action_y = y;
            Coord oldPosImage = iarea->posImage + iarea->deltaImage;
            //printf(">>> IMG / ImgPrev(%d x %d) = (%d x %d) + (%d x %d)\n", oldPosImage.x, oldPosImage.y, iarea->posImage.x, iarea->posImage.y, iarea->deltaImage.x, iarea->deltaImage.y);
            screenCoordToImage (x, y, currPos.x, currPos.y);
            iarea->deltaImage     = currPos - iarea->posImage;
            iarea->deltaPrevImage = currPos - oldPosImage;
            //printf("          action_ & xy (%d x %d) -> (%d x %d) = (%d x %d) + (%d x %d) / deltaPrev(%d x %d)\n", action_x, action_y, currPos.x, currPos.y, iarea->posImage.x, iarea->posImage.y, iarea->deltaImage.x, iarea->deltaImage.y, iarea->deltaPrevImage.x, iarea->deltaPrevImage.y);

            Coord oldPosScreen = iarea->posScreen + iarea->deltaScreen;
            //printf(">>> SCR / ScrPrev(%d x %d) = (%d x %d) + (%d x %d)\n", oldPosScreen.x, oldPosScreen.y, iarea->posScreen.x, iarea->posScreen.y, iarea->deltaScreen.x, iarea->deltaScreen.y);
            currPos.set(x, y);
            iarea->deltaScreen     = currPos - iarea->posScreen;
            iarea->deltaPrevScreen = currPos - oldPosScreen;
            //printf("          action_ & xy (%d x %d) -> (%d x %d) = (%d x %d) + (%d x %d) / deltaPrev(%d x %d)\n", action_x, action_y, currPos.x, currPos.y, iarea->posScreen.x, iarea->posScreen.y, iarea->deltaScreen.x, iarea->deltaScreen.y, iarea->deltaPrevScreen.x, iarea->deltaPrevScreen.y);

            if (state == SEditDrag1) {
                if (editSubscriber->drag1(bstate)) {
                    iarea->redraw ();
                }
            } else if (state == SEditDrag2) {
                if (editSubscriber->drag2(bstate)) {
                    iarea->redraw ();
                }
            } else if (state == SEditDrag3) {
                if (editSubscriber->drag3(bstate)) {
                    iarea->redraw ();
                }
            }
        }
    }

    updateCursor (x, y);

    bool oRA = onArea (CropResize, x, y);

    if (oRA != onResizeArea) {
        onResizeArea = oRA;
        iarea->redraw ();
    }

    if (decorated) {
        buttonSet.motionNotify (x, y);
    }

    if (pmlistener) {
        int mx, my;
        screenCoordToImage (x, y, mx, my);

        MyMutex::MyLock lock(cropHandler.cimg);

        if (!onArea (CropImage, x, y) || !cropHandler.cropPixbuftrue) {
            ImageSize msize = cropHandler.getFullImageSize();
            pmlistener->pointerMoved (false, *cropHandler.colorParams, msize.width, msize.height, -1, -1, -1);

            if (pmhlistener) {
                pmhlistener->pointerMoved (false, *cropHandler.colorParams, msize.width, msize.height, -1, -1, -1);
            }

        } else {
            int vx = x - windowPos.x - imgPos.x;
            int vy = y - windowPos.y - imgPos.y;

            if(decorated) {
                vx -= sideBorderWidth;
                vy -= (titleHeight + upperBorderWidth + sepWidth);
            }

            int deviceScale = cropHandler.getDeviceScale();
            vx *= deviceScale;
            vy *= deviceScale;

            int imwidth = cropHandler.cropPixbuftrue->get_width();
            int imheight = cropHandler.cropPixbuftrue->get_height();

            if (vx < imwidth && vy < imheight) {
                guint8* pix = cropHandler.cropPixbuftrue->get_pixels() + vy * cropHandler.cropPixbuftrue->get_rowstride() + vx * 3;
                int rval = pix[0];
                int gval = pix[1];
                int bval = pix[2];
                bool isRaw = false;
                rtengine::StagedImageProcessor* ipc = iarea->getImProcCoordinator();
                if(ipc) {
                    procparams::ProcParams params;
                    ipc->getParams(&params, true);
                    isRaw = params.raw.bayersensor.method == RAWParams::BayerSensor::getMethodString(RAWParams::BayerSensor::Method::NONE) || params.raw.xtranssensor.method == RAWParams::XTransSensor::getMethodString(RAWParams::XTransSensor::Method::NONE);
                    if(isRaw) {
                        ImageSource *isrc = static_cast<ImageSource*>(ipc->getInitialImage());
                        isrc->getRawValues(mx, my, params.coarse.rotate, rval, gval, bval);
                    }
                }

                // Updates the Navigator
                // TODO: possible double color conversion if rval, gval, bval come from cropHandler.cropPixbuftrue ? see issue #4583
                pmlistener->pointerMoved (true, *cropHandler.colorParams, mx, my, rval, gval, bval, isRaw);

                if (pmhlistener) {
                    // Updates the HistogramRGBArea
                    pmhlistener->pointerMoved (true, *cropHandler.colorParams, mx, my, rval, gval, bval);
                }
            }
        }
    }
}

bool CropWindow::onArea (CursorArea a, int x, int y)
{

    int CROPRESIZEBORDER = rtengine::max<int>(9 / zoomSteps[cropZoom].zoom, 3);
    int x1, y1, w, h;

    switch (a) {
    case CropWinButtons:
        return decorated && buttonSet.inside (x, y);

    case CropToolBar:
        return x > windowPos.x && y > windowPos.y && x < windowPos.x + windowSize.width - 1 && y < windowPos.y + imgAreaPos.y;

    case CropImage:
    {
        int deviceScale = cropHandler.getDeviceScale();
        hidpi::DeviceCoord pos = hidpi::LogicalCoord(x, y).scaleToDevice(deviceScale);
        hidpi::DeviceCoord topLeft = (windowPos + imgPos + imgAreaPos).scaleToDevice(deviceScale);
        hidpi::DeviceCoord botRight = topLeft + imgSize;

        return pos.x >= topLeft.x && pos.y >= topLeft.y && pos.x < botRight.x && pos.y < botRight.y;
    }
    case ColorPicker:
        for (auto colorPicker : colorPickers) {
            if (colorPicker->isOver(x, y)) {
                hoveredPicker = colorPicker;
                return true;
            }
        }
        hoveredPicker = nullptr;
        return false;

    case CropTopLeft:
        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               y1 >= cropHandler.cropParams->y - CROPRESIZEBORDER &&
               y1 <= cropHandler.cropParams->y + CROPRESIZEBORDER &&
               y >= windowPos.y + imgPos.y &&
               x1 >= cropHandler.cropParams->x - CROPRESIZEBORDER &&
               x1 <= cropHandler.cropParams->x + CROPRESIZEBORDER &&
               x >= windowPos.x + imgPos.x;

    case CropTopRight:
    {
        int deviceScale = cropHandler.getDeviceScale();
        hidpi::DeviceCoord pos = hidpi::LogicalCoord(x, y).scaleToDevice(deviceScale);
        hidpi::DeviceCoord topLeft = (windowPos + imgPos).scaleToDevice(deviceScale);
        hidpi::DeviceCoord botRight = topLeft + imgSize;

        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               y1 >= cropHandler.cropParams->y - CROPRESIZEBORDER &&
               y1 <= cropHandler.cropParams->y + CROPRESIZEBORDER &&
               pos.y >= topLeft.y &&
               x1 >= cropHandler.cropParams->x + cropHandler.cropParams->w - 1 - CROPRESIZEBORDER &&
               x1 <= cropHandler.cropParams->x + cropHandler.cropParams->w - 1 + CROPRESIZEBORDER &&
               pos.x < botRight.x;
    }
    case CropBottomLeft:
    {
        int deviceScale = cropHandler.getDeviceScale();
        hidpi::DeviceCoord pos = hidpi::LogicalCoord(x, y).scaleToDevice(deviceScale);
        hidpi::DeviceCoord topLeft = (windowPos + imgPos).scaleToDevice(deviceScale);
        hidpi::DeviceCoord botRight = topLeft + imgSize;

        screenCoordToImage (x, y, x1, y1);

        return cropHandler.cropParams->enabled &&
               y1 >= cropHandler.cropParams->y + cropHandler.cropParams->h - 1 - CROPRESIZEBORDER &&
               y1 <= cropHandler.cropParams->y + cropHandler.cropParams->h - 1 + CROPRESIZEBORDER &&
               pos.y < botRight.y &&
               x1 >= cropHandler.cropParams->x - CROPRESIZEBORDER &&
               x1 <= cropHandler.cropParams->x + CROPRESIZEBORDER &&
               pos.x >= topLeft.x;
    }
    case CropBottomRight:
    {
        int deviceScale = cropHandler.getDeviceScale();
        hidpi::DeviceCoord pos = hidpi::LogicalCoord(x, y).scaleToDevice(deviceScale);
        hidpi::DeviceCoord botRight = (windowPos + imgPos).scaleToDevice(deviceScale) + imgSize;

        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               y1 >= cropHandler.cropParams->y + cropHandler.cropParams->h - 1 - CROPRESIZEBORDER &&
               y1 <= cropHandler.cropParams->y + cropHandler.cropParams->h - 1 + CROPRESIZEBORDER &&
               pos.y < botRight.y &&
               x1 >= cropHandler.cropParams->x + cropHandler.cropParams->w - 1 - CROPRESIZEBORDER &&
               x1 <= cropHandler.cropParams->x + cropHandler.cropParams->w - 1 + CROPRESIZEBORDER &&
               pos.x < botRight.x;
    }
    case CropTop:
        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               x1 > cropHandler.cropParams->x + CROPRESIZEBORDER &&
               x1 < cropHandler.cropParams->x + cropHandler.cropParams->w - 1 - CROPRESIZEBORDER &&
               y1 > cropHandler.cropParams->y - CROPRESIZEBORDER &&
               y1 < cropHandler.cropParams->y + CROPRESIZEBORDER &&
               y >= windowPos.y + imgPos.y;

    case CropBottom:
    {
        int deviceScale = cropHandler.getDeviceScale();
        hidpi::DeviceCoord pos = hidpi::LogicalCoord(x, y).scaleToDevice(deviceScale);
        hidpi::DeviceCoord botRight = (windowPos + imgPos).scaleToDevice(deviceScale) + imgSize;

        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               x1 > cropHandler.cropParams->x + CROPRESIZEBORDER &&
               x1 < cropHandler.cropParams->x + cropHandler.cropParams->w - 1 - CROPRESIZEBORDER &&
               y1 > cropHandler.cropParams->y + cropHandler.cropParams->h - 1 - CROPRESIZEBORDER &&
               y1 < cropHandler.cropParams->y + cropHandler.cropParams->h - 1 + CROPRESIZEBORDER &&
               pos.y < botRight.y;
    }
    case CropLeft:
        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               y1 > cropHandler.cropParams->y + CROPRESIZEBORDER &&
               y1 < cropHandler.cropParams->y + cropHandler.cropParams->h - 1 - CROPRESIZEBORDER &&
               x1 > cropHandler.cropParams->x - CROPRESIZEBORDER &&
               x1 < cropHandler.cropParams->x + CROPRESIZEBORDER &&
               x >= windowPos.x + imgPos.x;

    case CropRight:
    {
        int deviceScale = cropHandler.getDeviceScale();
        hidpi::DeviceCoord pos = hidpi::LogicalCoord(x, y).scaleToDevice(deviceScale);
        hidpi::DeviceCoord botRight = (windowPos + imgPos).scaleToDevice(deviceScale) + imgSize;

        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               y1 > cropHandler.cropParams->y + CROPRESIZEBORDER &&
               y1 < cropHandler.cropParams->y + cropHandler.cropParams->h - 1 - CROPRESIZEBORDER &&
               x1 > cropHandler.cropParams->x + cropHandler.cropParams->w - 1 - CROPRESIZEBORDER &&
               x1 < cropHandler.cropParams->x + cropHandler.cropParams->w - 1 + CROPRESIZEBORDER &&
               pos.x < botRight.x;
    }
    case CropInside:
        screenCoordToImage (x, y, x1, y1);
        return cropHandler.cropParams->enabled &&
               y1 > cropHandler.cropParams->y &&
               y1 < cropHandler.cropParams->y + cropHandler.cropParams->h - 1 &&
               x1 > cropHandler.cropParams->x &&
               x1 < cropHandler.cropParams->x + cropHandler.cropParams->w - 1;

    case CropResize:
        return decorated && x >= windowPos.x + windowSize.width - 16 && y >= windowPos.y + windowSize.height - 16 && x < windowPos.x + windowSize.width && y < windowPos.y + windowSize.height;

    case CropObserved:
        if (!observedCropWin) {
            return false;
        }

        getObservedFrameArea (x1, y1, w, h);
        return x >= x1 && x <= x1 + w && y >= y1 && y <= y1 + h;
    }

    return false;
}

void CropWindow::updateCursor (int x, int y)
{

    EditSubscriber *editSubscriber = iarea->getCurrSubscriber();
    ToolMode tm = iarea->getToolMode ();

    CursorShape newType = cursor_type;

    if (state == SNormal) {
        if (onArea (CropWinButtons, x, y)) {
            newType = CSArrow;
        } else if (onArea (CropToolBar, x, y)) {
            newType = CSMove;
        } else if (iarea->getObject() > -1 && editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
            int cursorX;
            int cursorY;
            screenCoordToImage (x, y, cursorX, cursorY);
            newType = editSubscriber->getCursor(iarea->getObject(), cursorX, cursorY);
        } else if (onArea (CropResize, x, y)) {
            newType = CSResizeDiagonal;
        } else if (tm == TMColorPicker && hoveredPicker) {
            newType = CSMove;
        } else if (tm == TMHand && (onArea (CropTopLeft, x, y))) {
            newType = CSResizeTopLeft;
        } else if (tm == TMHand && (onArea (CropTopRight, x, y))) {
            newType = CSResizeTopRight;
        } else if (tm == TMHand && (onArea (CropBottomLeft, x, y))) {
            newType = CSResizeBottomLeft;
        } else if (tm == TMHand && (onArea (CropBottomRight, x, y))) {
            newType = CSResizeBottomRight;
        } else if (tm == TMHand && (onArea (CropTop, x, y) || onArea (CropBottom, x, y))) {
            newType = CSResizeHeight;
        } else if (tm == TMHand && (onArea (CropLeft, x, y) || onArea (CropRight, x, y))) {
            newType = CSResizeWidth;
        } else if (onArea (CropImage, x, y)) {
            int objectID = -1;

            if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
                Coord cropPos;
                screenCoordToCropCanvas (iarea->posScreen.x, iarea->posScreen.y, cropPos.x, cropPos.y);
                objectID = ObjectMOBuffer::getObjectID(cropPos);
            }

            if (objectID > -1) {
                int cursorX;
                int cursorY;
                screenCoordToImage (x, y, cursorX, cursorY);
                newType = editSubscriber->getCursor(objectID, cursorX, cursorY);
            } else if (tm == TMHand) {
                if (onArea (CropObserved, x, y)) {
                    newType = CSMove;
                } else {
                    newType = CSCrosshair;
                }
            } else if (tm == TMSpotWB) {
                newType = CSSpotWB;
            } else if (tm == TMCropSelect) {
                newType = CSCropSelect;
            } else if (tm == TMStraighten) {
                newType = CSStraighten;
            } else if (tm == TMColorPicker) {
                newType = CSAddColPicker;
            }
        } else {
            int objectID = -1;

            if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
                Coord cropPos;
                screenCoordToCropCanvas (iarea->posScreen.x, iarea->posScreen.y, cropPos.x, cropPos.y);
                objectID = ObjectMOBuffer::getObjectID(cropPos);
            }

            if (objectID > -1) {
                int cursorX;
                int cursorY;
                screenCoordToImage (x, y, cursorX, cursorY);
                newType = editSubscriber->getCursor(objectID, cursorX, cursorY);
            } else {
                newType = CSArrow;
            }
        }
    } else if (state == SCropSelecting) {
        newType = CSCropSelect;
    } else if (state == SRotateSelecting) {
        newType = CSStraighten;
    } else if (state == SCropMove || state == SCropWinMove || state == SObservedMove) {
        newType = CSMove;
    } else if (state == SHandMove || state == SCropImgMove) {
        newType = CSHandClosed;
    } else if (state == SResizeW1 || state == SResizeW2) {
        newType = CSResizeWidth;
    } else if (state == SResizeH1 || state == SResizeH2) {
        newType = CSResizeHeight;
    } else if (state == SResizeTL) {
        newType = CSResizeTopLeft;
    } else if (state == SResizeTR) {
        newType = CSResizeTopRight;
    } else if (state == SResizeBL) {
        newType = CSResizeBottomLeft;
    } else if (state == SResizeBR) {
        newType = CSResizeBottomRight;
    } else if (state == SCropWinResize) {
        newType = CSResizeDiagonal;
    } else if (state == SDragPicker) {
        newType = CSMove2D;
    } else if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS) {
        int objectID = iarea->getObject();
        if (objectID > -1) {
            int cursorX;
            int cursorY;
            screenCoordToImage (x, y, cursorX, cursorY);
            newType = editSubscriber->getCursor(objectID, cursorX, cursorY);
        } else {
            newType = CSArrow;
        }
    }

    if (newType != cursor_type) {
        cursor_type = newType;
        CursorManager::setWidgetCursor(iarea->get_window(), cursor_type);
    }

}

void CropWindow::expose (Cairo::RefPtr<Cairo::Context> cr)
{
    MyMutex::MyLock lock(cropHandler.cimg);

    bool isPreviewImg = false;

    if (decorated) {
        drawDecoration (cr);
    }

    // draw the background
    backColor = iarea->previewModePanel->GetbackColor();
    Glib::RefPtr<Gtk::StyleContext> style = iarea->get_style_context();

    auto& options = App::get().mut_options();
    options.bgcolor = backColor;

    if (backColor == 0) {
        hidpi::LogicalCoord offset = windowPos + imgAreaPos;
        style->render_background(cr, offset.x, offset.y, imgAreaSize.width, imgAreaSize.height);
    } else {
        if (backColor == 1) {
            cr->set_source_rgb (0, 0, 0);
        } else if (backColor == 2) {
            cr->set_source_rgb (1, 1, 1);
        } else if (backColor == 3) {
            cr->set_source_rgb (0.467, 0.467, 0.467);
        }

        cr->set_line_width (0.);
        hidpi::LogicalCoord offset = windowPos + imgAreaPos;
        cr->rectangle (offset.x, offset.y, imgAreaSize.width, imgAreaSize.height);
        cr->stroke_preserve ();
        cr->fill ();
    }

    // draw image
    if (state == SCropImgMove || state == SCropWinResize) {
        ImageCoord cropPos = cropHandler.getPosition();
        hidpi::ScaledDeviceSize desiredSize = imgAreaSize.scaleToDevice(
            RTScalable::getScaleForWidget(iarea));

        Glib::RefPtr<Gdk::Pixbuf> rough = iarea->getPreviewHandler()->getRoughImage(
            cropPos, desiredSize, zoomSteps[cropZoom].zoom);

        if (rough) {
            hidpi::LogicalCoord offset = windowPos + imgAreaPos + imgPos;
            Gdk::Cairo::set_source_pixbuf(cr, rough, offset.x, offset.y);
            auto pattern = hidpi::getSourceForSurface(cr);
            hidpi::setDeviceScale(pattern->get_surface(), desiredSize.device_scale);
            // Contain blitting area within crop window
            cr->rectangle(offset.x, offset.y,
                          std::min(rough->get_width() / desiredSize.device_scale,
                                   imgAreaSize.width - imgPos.x),
                          std::min(rough->get_height() / desiredSize.device_scale,
                                   imgAreaSize.height - imgPos.y));
            cr->fill();
        }

        if (observedCropWin) {
            drawObservedFrame (cr);
        }
    } else {
        const auto& cropParams = *cropHandler.cropParams;
        const auto cropGuideOverride = [&]() {
            if (state == SNormal) {
                switch (options.cropGuides) {
                case Options::CROP_GUIDE_NONE:
                    return CropGuideOverride::NO_GUIDES;
                case Options::CROP_GUIDE_FRAME:
                    return CropGuideOverride::FRAME;
                default:
                    break;
                }
            }

            return (this == iarea->mainCropWindow)
                ? CropGuideOverride::DONT_TOUCH : CropGuideOverride::NO_GUIDES;
        }();

        bool useBgColor = (state == SNormal || state == SDragPicker || state == SDeletePicker || state == SEditDrag1);

        if (cropHandler.cropPixbuf) {
            imgSize.width = cropHandler.cropPixbuf->get_width();
            imgSize.height = cropHandler.cropPixbuf->get_height();
            exposeVersion++;

            const bool showR  = iarea->previewModePanel->showR(); // will show clipping if R channel is clipped
            const bool showG  = iarea->previewModePanel->showG(); // will show clipping if G channel is clipped
            const bool showB  = iarea->previewModePanel->showB(); // will show clipping if B channel is clipped
            const bool showL  = iarea->previewModePanel->showL(); // will show clipping if L value   is clipped
            const bool showFocusMask  = iarea->indClippedPanel->showFocusMask();
            bool showcs = iarea->indClippedPanel->showClippedShadows();
            bool showch = iarea->indClippedPanel->showClippedHighlights();

            // While the Right-side ALT is pressed, auto-enable highlight and shadow clipping indicators
            // TODO: Add linux/MacOS specific functions for alternative
#ifdef _WIN32

            if (GetKeyState(VK_RMENU) < 0) {
                showcs = true;
                showch = true;
            }

#endif

            if (showcs || showch || showR || showG || showB || showL || showFocusMask) {
                Glib::RefPtr<Gdk::Pixbuf> tmp = cropHandler.cropPixbuf->copy ();
                guint8* pix = tmp->get_pixels();
                guint8* pixWrkSpace = cropHandler.cropPixbuftrue->get_pixels();

                const int pixRowStride = tmp->get_rowstride ();
                const int pixWSRowStride = cropHandler.cropPixbuftrue->get_rowstride ();

                const int bHeight = tmp->get_height();
                const int bWidth = tmp->get_width();

                if (showFocusMask) { // modulate preview to display focus mask
                    const int blur_radius2 = 1;                             // radius of small kernel. 1 => 3x3 kernel
                    const int blur_dim2 = 2 * blur_radius2 + 1;             // dimension of small kernel
                    const int blur_radius = (blur_dim2 * blur_dim2) / 2;    // radius of big kernel
                    const float kernel_size = SQR(2.f * blur_radius + 1.f); // count of pixels in the big blur kernel
                    const float rkernel_size = 1.0f / kernel_size;          // reciprocal of kernel_size to avoid divisions
                    const float kernel_size2 = SQR(2.f * blur_radius2 + 1.f); // count of pixels in the small blur kernel
                    const float rkernel_size2 = 1.0f / kernel_size2;        // reciprocal of kernel_size to avoid divisions

                    // allocate buffer for precalculated Luminance
                    float* tmpL = (float*)malloc(bHeight * bWidth * sizeof(float) );
                    // allocate buffers for sums and sums of squares of small kernel
                    float* tmpLsum = (float*)malloc((bHeight) * (bWidth) * sizeof(float) );
                    float* tmpLsumSq = (float*)malloc((bHeight) * (bWidth) * sizeof(float) );
                    float* tmpstdDev2 = (float*)malloc((bHeight) * (bWidth) * sizeof(float) );
                    float maxstdDev_L2 = 0.f;

#ifdef _OPENMP
                    #pragma omp parallel
#endif
                    {
#ifdef _OPENMP
                        #pragma omp for
#endif

                        // precalculate Luminance
                        for(int i = 0; i < bHeight; i++) {
                            guint8* currWS = pixWrkSpace + i * pixWSRowStride;
                            float*  currL = tmpL + i * bWidth;

                            for(int j = 0; j < bWidth; j++) {
                                *currL = 0.299f * (currWS)[0] + 0.587f * (currWS)[1] + 0.114f * (currWS)[2];
                                currL++;
                                currWS += 3;
                            }
                        }

                        float maxthrstdDev_L2 = 0.f;
#ifdef _OPENMP
                        #pragma omp for nowait
#endif

                        // precalculate sum and sum of squares of small kernel
                        for(int i = blur_radius2; i < bHeight - blur_radius2; i++) {
                            for(int j = blur_radius2; j < bWidth - blur_radius2; j++) {
                                float sumL = 0.f;
                                float sumLSqu = 0.f;

                                for(int kh = -blur_radius2; kh <= blur_radius2; kh++) {
                                    for(int kw = -blur_radius2; kw <= blur_radius2; kw++) {
                                        float curL = tmpL[(i + kh) * bWidth + j + kw];
                                        sumL += curL;
                                        sumLSqu += SQR(curL);
                                    }
                                }

                                tmpLsum[i * bWidth + j] = sumL;
                                tmpLsumSq[i * bWidth + j] = sumLSqu;
                                float stdDev_L2 = rkernel_size2 * sqrtf(sumLSqu * kernel_size2 - sumL * sumL);

                                if(stdDev_L2 > maxthrstdDev_L2) {
                                    maxthrstdDev_L2 = stdDev_L2;
                                }

                                tmpstdDev2[i * bWidth + j] = stdDev_L2;
                            }
                        }

#ifdef _OPENMP
                        #pragma omp critical
#endif
                        {
                            if(maxthrstdDev_L2 > maxstdDev_L2) {
                                maxstdDev_L2 = maxthrstdDev_L2;
                            }
                        }
                    }

                    const float focus_thresh = 80.f;
                    maxstdDev_L2 = std::min(maxstdDev_L2, focus_thresh);
                    const float focus_threshby10 = focus_thresh / 10.f;
#ifdef _OPENMP
                    #pragma omp parallel for schedule(dynamic,16)
#endif

                    for (int i = blur_radius + 1; i < bHeight - blur_radius; i++) {
                        guint8* curr = pix + i * pixRowStride + 3 * (blur_radius + 1);
                        guint8* currWs = pixWrkSpace + i * pixWSRowStride + 3 * (blur_radius + 1);

                        for (int j = blur_radius + 1; j < bWidth - blur_radius; j++) {

                            //*************
                            // Copyright (c) 2011 Michael Ezra michael@michaelezra.com
                            // determine if pixel is in the sharp area of the image using
                            // standard deviation analysis on two different scales
                            //float focus_thresh2;
                            //float opacity = 0.9;//TODO: implement opacity
                            //TODO: evaluate effects of altering sampling frequency


                            //TODO: dynamically determine appropriate values based on image analysis

                            // calculate average in +-blur_radius pixels area around the current pixel
                            // speed up: calculate sum of squares in the same loops

                            float sum_L = 0.f;
                            float sumsq_L = 0.f;

                            // use precalculated values of small kernel to reduce number of iterations
                            for (int kh = -blur_radius + blur_radius2; kh <= blur_radius - blur_radius2; kh += blur_dim2) {
                                float* currLsum = &tmpLsum[(i + kh) * bWidth + j - blur_radius + 1];
                                float* currLsumSqu = &tmpLsumSq[(i + kh) * bWidth + j - blur_radius + 1];

                                for (int k = -blur_radius + blur_radius2; k <= blur_radius - blur_radius2; k += blur_dim2, currLsum += blur_dim2, currLsumSqu += blur_dim2) {
                                    sum_L += *currLsum;
                                    sumsq_L += *currLsumSqu;
                                }
                            }

                            //float sum_L2 = tmpLsum[i * bWidth + j];
                            //float sumsq_L2 = tmpLsumSq[i * bWidth + j];
                            //*************
                            // averages
                            // Optimized formulas to avoid divisions
                            float stdDev_L = rkernel_size * sqrtf(sumsq_L * kernel_size - sum_L * sum_L);
                            float stdDev_L2 = tmpstdDev2[i * bWidth + j];
//                          float stdDev_L2 = rkernel_size2 * sqrtf(sumsq_L2 * kernel_size2 - sum_L2 * sum_L2);

                            //TODO: try to normalize by average L of the entire (preview) image

                            //detection method 1: detect focus in features
                            //there is no strict condition between stdDev_L and stdDev_L2 themselves
                            /*                                if (stdDev_L2>focus_thresh2
                                                            && (stdDev_L <focus_thresh)){ // this excludes false positives due to high contrast edges

                                                                curr[1]=255;
                                                                curr[0]=0;
                                                                curr[2]=0;

                                                            }*/

                            //detection method 2: detect focus in texture
                            // key point is std deviation on lower scale is higher than for the larger scale
                            // plus some boundary conditions
                            if (focus_thresh >= stdDev_L2 //TODO: could vary this to bypass noise better
                                    && stdDev_L2 > stdDev_L //this is the key to select fine detail within lower contrast on larger scale
                                    && stdDev_L > focus_threshby10 //options.highlightThreshold
                               ) {
                                // transparency depends on sdtDev_L2 and maxstdDev_L2
                                float transparency = 1.f - std::min(stdDev_L2 / maxstdDev_L2, 1.0f) ;
                                // first row of circle
                                guint8* currtmp = &curr[0] + (-3 * pixRowStride);
                                guint8* currtmpWS = &currWs[0] + (-3 * pixWSRowStride);

                                for(int jj = -3; jj <= 3; jj += 3) {
                                    guint8* currtmpl = currtmp + jj;
                                    guint8* currtmpWSl = currtmpWS + jj;
                                    //transparent green
                                    currtmpl[0] = transparency * currtmpWSl[0];
                                    currtmpl[1] = transparency * currtmpWSl[1] + (1.f - transparency) * 255.f;
                                    currtmpl[2] = transparency * currtmpWSl[2];
                                }

                                // second row of circle
                                currtmp = &curr[0] + (-2 * pixRowStride);
                                currtmpWS = &currWs[0] + (-2 * pixWSRowStride);

                                for(int jj = -6; jj <= 6; jj += 3) {
                                    guint8* currtmpl = currtmp + jj;
                                    guint8* currtmpWSl = currtmpWS + jj;
                                    //transparent green
                                    currtmpl[0] = transparency * currtmpWSl[0];
                                    currtmpl[1] = transparency * currtmpWSl[1] + (1.f - transparency) * 255.f;
                                    currtmpl[2] = transparency * currtmpWSl[2];
                                }

                                // three middle row of circle
                                for(int ii = -1; ii <= 1; ii++) {
                                    currtmp = &curr[0] + (ii * pixRowStride);
                                    currtmpWS = &currWs[0] + (ii * pixWSRowStride);

                                    for(int jj = -9; jj <= 9; jj += 3) {
                                        guint8* currtmpl = currtmp + jj;
                                        guint8* currtmpWSl = currtmpWS + jj;
                                        //transparent green
                                        currtmpl[0] = transparency * currtmpWSl[0];
                                        currtmpl[1] = transparency * currtmpWSl[1] + (1.f - transparency) * 255.f;
                                        currtmpl[2] = transparency * currtmpWSl[2];
                                    }
                                }

                                // second last row of circle
                                currtmp = &curr[0] + (2 * pixRowStride);
                                currtmpWS = &currWs[0] + (2 * pixWSRowStride);

                                for(int jj = -6; jj <= 6; jj += 3) {
                                    guint8* currtmpl = currtmp + jj;
                                    guint8* currtmpWSl = currtmpWS + jj;
                                    //transparent green
                                    currtmpl[0] = transparency * currtmpWSl[0];
                                    currtmpl[1] = transparency * currtmpWSl[1] + (1.f - transparency) * 255.f;
                                    currtmpl[2] = transparency * currtmpWSl[2];
                                }

                                // last row of circle
                                currtmp = &curr[0] + (3 * pixRowStride);
                                currtmpWS = &currWs[0] + (3 * pixWSRowStride);

                                for(int jj = -3; jj <= 3; jj += 3) {
                                    guint8* currtmpl = currtmp + jj;
                                    guint8* currtmpWSl = currtmpWS + jj;
                                    //transparent green
                                    currtmpl[0] = transparency * currtmpWSl[0];
                                    currtmpl[1] = transparency * currtmpWSl[1] + (1.f - transparency) * 255.f;
                                    currtmpl[2] = transparency * currtmpWSl[2];
                                }
                            }

                            curr += 3;
                            currWs += 3;
                        }
                    }

                    free(tmpL);
                    free(tmpLsum);
                    free(tmpLsumSq);
                    free(tmpstdDev2);

                } else { // !showFocusMask

                    const int hlThreshold = options.highlightThreshold;
                    const int shThreshold = options.shadowThreshold;
                    const float ShawdowFac = 64.f / (options.shadowThreshold + 1);
                    const float HighlightFac = 64.f / (256 - options.highlightThreshold);
                    const bool showclippedAny = (!showR && !showG && !showB && !showL); // will show clipping if any (all) of RGB channels is (shadow) clipped

#ifdef _OPENMP
                    #pragma omp parallel for schedule(dynamic,16)
#endif

                    for (int i = 0; i < bHeight; i++) {
                        guint8* curr = pix + i * pixRowStride;
                        guint8* currWS = pixWrkSpace + i * pixWSRowStride;

                        for (int j = 0; j < bWidth; j++) {
                            // we must compare clippings in working space, since the cropPixbuf is in sRGB, with mon profile

                            bool changedHL = false;
                            bool changedSH = false;
                            int delta = 0;
                            // for efficiency, pre-calculate currWS_L as it may be needed in both
                            // if (showch) and if (showcs) branches
                            int currWS_L = 0;

                            if (showL && (showch || showcs)) {
                                currWS_L = (int)(0.299f * currWS[0] + 0.587f * currWS[1] + 0.114f * currWS[2]);
                            }

                            if (showch) {
                                if ((showclippedAny || showR) && currWS[0] >= hlThreshold ) {
                                    delta += 255 - currWS[0];
                                    changedHL = true;
                                }

                                if ((showclippedAny || showG) && currWS[1] >= hlThreshold ) {
                                    delta += 255 - currWS[1];
                                    changedHL = true;
                                }

                                if ((showclippedAny || showB) && currWS[2] >= hlThreshold ) {
                                    delta += 255 - currWS[2];
                                    changedHL = true;
                                }

                                if (showL && currWS_L >= hlThreshold )                     {
                                    delta += 255 - currWS_L ;
                                    changedHL = true;
                                }

                                if (changedHL) {
                                    delta *= HighlightFac;

                                    if (showclippedAny) {
                                        curr[0] = curr[1] = curr[2] = delta;    // indicate clipped highlights in gray
                                    } else {
                                        curr[0] = 255;    // indicate clipped highlights in red
                                        curr[1] = curr[2] = delta;
                                    }
                                }
                            }

                            if (showcs) {
                                bool scR = currWS[0] <= shThreshold;
                                bool scG = currWS[1] <= shThreshold;
                                bool scB = currWS[2] <= shThreshold;

                                if (((showclippedAny && (scG && scB)) || showR) && scR ) {
                                    delta += currWS[0];
                                    changedSH = true;
                                }

                                if (((showclippedAny && (scR && scB)) || showG) && scG ) {
                                    delta += currWS[1];
                                    changedSH = true;
                                }

                                if (((showclippedAny && (scR && scG)) || showB) && scB ) {
                                    delta += currWS[2];
                                    changedSH = true;
                                }

                                if (showL && currWS_L <= shThreshold )                     {
                                    delta += currWS_L ;
                                    changedSH = true;
                                }

                                if (changedSH) {
                                    if (showclippedAny) {
                                        delta = 255 - (delta * ShawdowFac);
                                        curr[0] = curr[1] = curr[2] = delta; // indicate clipped shadows in gray
                                    } else {
                                        delta *= ShawdowFac;
                                        curr[2] = 255;
                                        curr[0] = curr[1] = delta; // indicate clipped shadows in blue
                                    }
                                }
                            } //if (showcs)

                            // modulate the preview of channels & L;
                            if (!changedHL && !changedSH && !showclippedAny) {         //This condition allows clipping indicators for RGB channels to remain in color
                                if (showR) {
                                    curr[1] = curr[2] = curr[0];    //Red   channel in grayscale
                                }

                                if (showG) {
                                    curr[0] = curr[2] = curr[1];    //Green channel in grayscale
                                }

                                if (showB) {
                                    curr[0] = curr[1] = curr[2];    //Blue  channel in grayscale
                                }

                                if (showL) {                        //Luminosity
                                    // see http://en.wikipedia.org/wiki/HSL_and_HSV#Lightness for more info
                                    //int L = (int)(0.212671*curr[0]+0.715160*curr[1]+0.072169*curr[2]);
                                    int L = (int)(0.299 * curr[0] + 0.587 * curr[1] + 0.114 * curr[2]); //Lightness - this matches Luminosity mode in Photoshop CS5
                                    curr[0] = curr[1] = curr[2] = L;
                                }
                            }

                            /*
                                if (showch && (currWS[0]>=options.highlightThreshold || currWS[1]>=options.highlightThreshold || currWS[2]>=options.highlightThreshold))
                                    curr[0] = curr[1] = curr[2] = 0;
                                else if (showcs && (currWS[0]<=options.shadowThreshold || currWS[1]<=options.shadowThreshold || currWS[2]<=options.shadowThreshold))
                                    curr[0] = curr[1] = curr[2] = 255;
                                //if (showch && ((0.299*curr[0]+0.587*curr[1]+0.114*curr[2])>=options.highlightThreshold))
                                //    curr[0] = curr[1] = curr[2] = 0;
                                //else if (showcs && ((0.299*curr[0]+0.587*curr[1]+0.114*curr[2])<=options.shadowThreshold))
                                //    curr[0] = curr[1] = curr[2] = 255;
                            */

                            curr += 3;
                            currWS += 3;
                        }
                    }
                }

                hidpi::LogicalCoord offset = windowPos + imgAreaPos + imgPos;
                Gdk::Cairo::set_source_pixbuf(cr, tmp, offset.x, offset.y);
                auto pattern = hidpi::getSourceForSurface(cr);
                int deviceScale = RTScalable::getScaleForWidget(iarea);
                hidpi::setDeviceScale(pattern->get_surface(), deviceScale);
                // Contain blitting area within crop window
                cr->rectangle(offset.x, offset.y,
                              std::min(tmp->get_width() / deviceScale,
                                       imgAreaSize.width - imgPos.x),
                              std::min(tmp->get_height() / deviceScale,
                                       imgAreaSize.height - imgPos.y));
                cr->fill();
            } else {
                hidpi::LogicalCoord offset = windowPos + imgAreaPos + imgPos;
                Gdk::Cairo::set_source_pixbuf(cr, cropHandler.cropPixbuf, offset.x, offset.y);
                auto pattern = hidpi::getSourceForSurface(cr);
                int deviceScale = RTScalable::getScaleForWidget(iarea);
                hidpi::setDeviceScale(pattern->get_surface(), deviceScale);
                // Contain blitting area within crop window
                cr->rectangle(offset.x, offset.y,
                              std::min(cropHandler.cropPixbuf->get_width() / deviceScale,
                                       imgAreaSize.width - imgPos.x),
                              std::min(cropHandler.cropPixbuf->get_height() / deviceScale,
                                       imgAreaSize.height - imgPos.y));
                cr->fill();
            }

            if (cropHandler.cropParams->enabled) {
                hidpi::LogicalCoord offset = windowPos + imgAreaPos + imgPos;
                ImageCoord cropPos = cropHandler.getPosition();
                double deviceScale = RTScalable::getScaleForWidget(iarea);

                double clipWidth =
                    std::min<double>(std::ceil(imgSize.width / deviceScale),
                                     imgAreaSize.width - imgPos.x);
                double clipHeight =
                    std::min<double>(std::ceil(imgSize.height / deviceScale),
                                     imgAreaSize.height - imgPos.y);

                drawCrop(cr, offset.x, offset.y,
                         imgSize.width, imgSize.height,
                         clipWidth, clipHeight,
                         cropPos.x, cropPos.y,
                         zoomSteps[cropZoom].zoom / deviceScale,
                         deviceScale,
                         cropParams,
                         *cropHandler.cropGuideParams,
                         cropGuideOverride,
                         useBgColor,
                         cropHandler.isFullDisplay());
            }

            if (observedCropWin) {
                drawObservedFrame (cr);
            }

            EditSubscriber *editSubscriber = iarea->getCurrSubscriber();
            if (editSubscriber && editSubscriber->getEditingType() == ET_OBJECTS && bufferCreated()) {

                cr->set_line_width (0.);
                hidpi::LogicalCoord offset = windowPos + imgAreaPos;
                cr->rectangle (offset.x, offset.y, imgAreaSize.width, imgAreaSize.height);
                cr->clip();

                // drawing Subscriber's visible geometry
                const std::vector<Geometry*> visibleGeom = editSubscriber->getVisibleGeometry();
                cr->set_antialias(Cairo::ANTIALIAS_DEFAULT); // ANTIALIAS_SUBPIXEL ?
                cr->set_line_cap(Cairo::LINE_CAP_SQUARE);
                cr->set_line_join(Cairo::LINE_JOIN_ROUND);

                // drawing outer lines
                for (auto geom : visibleGeom) {
                    geom->drawOuterGeometry(cr, this, *this);
                }

                // drawing inner lines
                for (auto geom : visibleGeom) {
                    geom->drawInnerGeometry(cr, this, *this);
                }

                // drawing to the "mouse over" channel
                const auto mouseOverGeom = editSubscriber->getMouseOverGeometry();
                if (mouseOverGeom.size()) {
                    if (mouseOverGeom.size() > 255) {
                        // Once it has been switched to OM_65535, it won't return back to OM_255
                        // to avoid constant memory allocations in some particular situation.
                        // It will return to OM_255 on a new editing session
                        setObjectMode(OM_65535);
                    }

                    int deviceScale = RTScalable::getScaleForWidget(iarea);
                    hidpi::setDeviceScale(ObjectMOBuffer::getObjectMap(), deviceScale);

                    Cairo::RefPtr<Cairo::Context> crMO = Cairo::Context::create(ObjectMOBuffer::getObjectMap());
                    crMO->set_antialias(Cairo::ANTIALIAS_NONE);
                    crMO->set_line_cap(Cairo::LINE_CAP_SQUARE);
                    crMO->set_line_join(Cairo::LINE_JOIN_ROUND);
                    crMO->set_operator(Cairo::OPERATOR_SOURCE);

                    // clear the bitmap
                    crMO->set_source_rgba(0., 0., 0., 0.);
                    crMO->paint();

                    int a=0;
                    for (auto moGeom : mouseOverGeom) {
                        moGeom->drawToMOChannel(crMO, a, this, *this);
                        ++a;
                    }
                }
                if (this != iarea->mainCropWindow) {
                    cr->reset_clip();
                }
            }

            isPreviewImg = true;
        } else {
            // cropHandler.cropPixbuf is null
            ImageCoord cropPos = cropHandler.getPosition();
            hidpi::ScaledDeviceSize desiredSize = imgAreaSize.scaleToDevice(
                RTScalable::getScaleForWidget(iarea));

            Glib::RefPtr<Gdk::Pixbuf> rough = iarea->getPreviewHandler()->getRoughImage(
                cropPos, desiredSize, zoomSteps[cropZoom].zoom);

            if (rough) {
                hidpi::LogicalCoord offset = windowPos + imgAreaPos + imgPos;
                double deviceScale = desiredSize.device_scale;
                Gdk::Cairo::set_source_pixbuf(cr, rough, offset.x, offset.y);
                auto pattern = hidpi::getSourceForSurface(cr);
                hidpi::setDeviceScale(pattern->get_surface(), deviceScale);
                // Contain blitting area within crop window
                cr->rectangle(offset.x, offset.y,
                              std::min(rough->get_width() / desiredSize.device_scale,
                                       imgAreaSize.width - imgPos.x),
                              std::min(rough->get_height() / desiredSize.device_scale,
                                       imgAreaSize.height - imgPos.y));
                cr->fill();

                if (cropHandler.cropParams->enabled) {
                    double roughW = rough->get_width();
                    double roughH = rough->get_height();
                    double clipWidth =
                        std::min<double>(std::ceil(roughW / deviceScale),
                                         imgAreaSize.width - imgPos.x);
                    double clipHeight =
                        std::min<double>(std::ceil(roughH / deviceScale),
                                         imgAreaSize.height - imgPos.y);

                    drawCrop(cr, offset.x, offset.y,
                             roughW, roughH,
                             clipWidth, clipHeight,
                             cropPos.x, cropPos.y,
                             zoomSteps[cropZoom].zoom / deviceScale,
                             deviceScale,
                             cropParams,
                             *cropHandler.cropGuideParams,
                             cropGuideOverride,
                             useBgColor,
                             cropHandler.isFullDisplay());
                }

                if (observedCropWin) {
                    drawObservedFrame (cr);
                }
            }
        }
    }

    if (state == SRotateSelecting) {
        drawStraightenGuide (cr);
    }

    if (state == SNormal && isFlawnOver) {
        EditSubscriber *editSubscriber = iarea->getCurrSubscriber();

        if (iarea->getToolMode () == TMHand && editSubscriber && editSubscriber->getEditingType() == ET_PIPETTE && iarea->getObject()) {
            drawUnscaledSpotRectangle (cr, iarea->getPipetteRectSize ());
        } else if (iarea->getToolMode () == TMSpotWB) {
            drawScaledSpotRectangle (cr, iarea->getSpotWBRectSize ());
        }
    }

    {
        hidpi::LogicalCoord offset = windowPos + imgAreaPos;
        style->render_frame (cr, offset.x, offset.y, imgAreaSize.width, imgAreaSize.height);
    }

    if ((state == SNormal || state == SDragPicker) && isPreviewImg && iarea->showColorPickers()) {
        for (auto colorPicker : colorPickers) {
            colorPicker->draw(cr);
        }
    }
}

void CropWindow::setEditSubscriber (EditSubscriber* newSubscriber) {
    // Delete, create, update all buffers based upon newSubscriber's type
    if (newSubscriber) {
        int deviceScale = RTScalable::getScaleForWidget(iarea);
        hidpi::ScaledDeviceSize deviceSize = imgAreaSize.scaleToDevice(deviceScale);
        ObjectMOBuffer::resize (deviceSize.width, deviceSize.height);
    } else {
        ObjectMOBuffer::flush ();
    }
    cropHandler.setEditSubscriber(newSubscriber);
}

// zoom* is called from the zoomPanel or the scroll wheel in the preview area
void CropWindow::zoomIn (bool toCursor, int cursorX, int cursorY)
{

    int x = -1;
    int y = -1;

    if (toCursor) {
        x = cursorX;
        y = cursorY;
    } else {
        if (zoomSteps[cropZoom].zoom <= cropHandler.getFitZoom()) {
            if (cropHandler.cropParams->enabled) {
                x = cropHandler.cropParams->x + cropHandler.cropParams->w / 2;
                y = cropHandler.cropParams->y + cropHandler.cropParams->h / 2;
            } else {
                ImageSize fsize = cropHandler.getFullImageSize();
                x = fsize.width / 2;
                y = fsize.height / 2;
            }

            zoomVersion = exposeVersion;
        } else if (zoomVersion != exposeVersion) {
            int deviceScale = cropHandler.getDeviceScale();
            screenCoordToImage(windowPos.x + imgPos.x + imgSize.width / 2 / deviceScale,
                               windowPos.y + imgPos.y + imgSize.height / 2 / deviceScale,
                               x, y);

            if (cropHandler.cropParams->enabled) {
                // add some gravity towards crop center
                int x1 = cropHandler.cropParams->x + cropHandler.cropParams->w / 2;
                int y1 = cropHandler.cropParams->y + cropHandler.cropParams->h / 2;
                double cropd = sqrt(cropHandler.cropParams->h * cropHandler.cropParams->h + cropHandler.cropParams->w * cropHandler.cropParams->w) * zoomSteps[cropZoom].zoom;
                double imd = sqrt(imgSize.width * imgSize.width + imgSize.height * imgSize.height);
                double d;

                // the more we can see of the crop, the more gravity towards crop center
                if (cropd > imd) {
                    d = 0.8;
                } else if (cropd < imd * 0.5) {
                    d = 0.0;
                } else {
                    d = 1.6 * (cropd - imd * 0.5) / imd;
                }

                x = d * x + (1.0 - d) * x1;
                y = d * y + (1.0 - d) * y1;
            }

            zoomVersion = exposeVersion;
        }
    }

    int z = cropZoom + 1;
    while (z < int(zoomSteps.size()) && !zoomSteps[z].is_major) {
        ++z;
    }
    changeZoom (z, true, x, y);
    fitZoom = false;
}

void CropWindow::zoomOut (bool toCursor, int cursorX, int cursorY)
{

    int x = -1;
    int y = -1;

    if (toCursor) {
        x = cursorX;
        y = cursorY;
    } else {
        int deviceScale = cropHandler.getDeviceScale();
        screenCoordToImage(windowPos.x + imgPos.x + imgSize.width / 2 / deviceScale,
                           windowPos.y + imgPos.y + imgSize.height / 2 / deviceScale,
                           x, y);
    }

    zoomVersion = exposeVersion;
    int z = cropZoom - 1;
    while (z >= 0 && !zoomSteps[z].is_major) {
        --z;
    }
    changeZoom (z, true, x, y);
    fitZoom = false;
}

void CropWindow::zoom11 (bool notify)
{

    int x = -1;
    int y = -1;

    if (zoomSteps[cropZoom].zoom <= cropHandler.getFitZoom()) {
        if (cropHandler.cropParams->enabled) {
            x = cropHandler.cropParams->x + cropHandler.cropParams->w / 2;
            y = cropHandler.cropParams->y + cropHandler.cropParams->h / 2;
        } else {
            ImageSize fsize = cropHandler.getFullImageSize();
            x = fsize.width / 2;
            y = fsize.height / 2;
        }

        zoomVersion = exposeVersion;
    } else {
        int deviceScale = cropHandler.getDeviceScale();
        screenCoordToImage(windowPos.x + imgPos.x + imgSize.width / 2 / deviceScale,
                           windowPos.y + imgPos.y + imgSize.height / 2 / deviceScale,
                           x, y);
    }

    changeZoom (zoom11index, notify, x, y, notify);
    fitZoom = false;
}

double CropWindow::getZoom ()
{

    return zoomSteps[cropZoom].zoom;
}

bool CropWindow::isMinZoom ()
{
    return cropZoom <= 0;
}

bool CropWindow::isMaxZoom ()
{
    return cropZoom >= int(zoomSteps.size())-1;
}

void CropWindow::setZoom (double zoom)
{
    int cz = int(zoomSteps.size())-1;

    if (zoom < zoomSteps[0].zoom) {
        cz = 0;
    } else
        for (int i = 0; i < int(zoomSteps.size())-1; i++)
            if (zoomSteps[i].zoom <= zoom && zoomSteps[i + 1].zoom > zoom) {
                cz = i;
                break;
            }

    changeZoom (cz, false);
}

double CropWindow::getZoomFitVal ()
{
    double z = cropHandler.getFitZoom ();
    int cz = int(zoomSteps.size())-1;

    if (z < zoomSteps[0].zoom) {
        cz = 0;
    } else
        for (int i = 0; i < int(zoomSteps.size())-1; i++)
            if (zoomSteps[i].zoom <= z && zoomSteps[i + 1].zoom > z) {
                cz = i;
                break;
            }

    return zoomSteps[cz].zoom;
}


void CropWindow::zoomFit ()
{
    double z = cropHandler.getFitZoom ();
    int cz = int(zoomSteps.size())-1;

    if (z < zoomSteps[0].zoom) {
        cz = 0;
    } else
        for (int i = 0; i < int(zoomSteps.size())-1; i++)
            if (zoomSteps[i].zoom <= z && zoomSteps[i + 1].zoom > z) {
                cz = i;
                break;
            }

    zoomVersion = exposeVersion;
    changeZoom (cz, true, -1, -1);
    fitZoom = true;
}

void CropWindow::zoomFitCrop ()
{
    if(cropHandler.cropParams->enabled) {
        double z = cropHandler.getFitCropZoom ();
        int cz = int(zoomSteps.size())-1;

        if (z < zoomSteps[0].zoom) {
            cz = 0;
        } else
            for (int i = 0; i < int(zoomSteps.size())-1; i++)
                if (zoomSteps[i].zoom <= z && zoomSteps[i + 1].zoom > z) {
                    cz = i;
                    break;
                }

        zoomVersion = exposeVersion;
        int centerX, centerY;
        centerX = cropHandler.cropParams->x + cropHandler.cropParams->w / 2;
        centerY = cropHandler.cropParams->y + cropHandler.cropParams->h / 2;
        setCropAnchorPosition(centerX, centerY);
        changeZoom (cz, true, centerX, centerY);
        fitZoom = App::get().options().cropAutoFit;
    } else {
        zoomFit();
    }
}

void CropWindow::buttonPressed (LWButton* button, int actionCode, void* actionData, int x, int y)
{

    if (button == bZoomIn) { // zoom in
        zoomIn ();
    } else if (button == bZoomOut) { // zoom out
        zoomOut ();
    } else if (button == bZoom100) { // zoom 100
        zoom11 ();
    } else if (button == bClose) { // close
        if(iarea->getImProcCoordinator()->updateTryLock()) {
            deleted = true;
            iarea->cropWindowClosed (this);
            iarea->getImProcCoordinator()->updateUnLock();
        }
    }
}

void CropWindow::redrawNeeded (LWButton* button)
{

    iarea->redraw ();
}

void CropWindow::updateHoveredPicker (rtengine::Coord *imgPos)
{

    if (!hoveredPicker) {
        return;
    }

    rtengine::Coord cropPos;
    if (imgPos) {
        imageCoordToCropImage(imgPos->x, imgPos->y, cropPos.x, cropPos.y);
        hoveredPicker->setPosition (*imgPos);
    } else {
        rtengine::Coord imgPos2;
        hoveredPicker->getImagePosition(imgPos2);
        imageCoordToCropImage(imgPos2.x, imgPos2.y, cropPos.x, cropPos.y);
    }
    LockableColorPicker::Validity validity = checkValidity (hoveredPicker, cropPos);
    hoveredPicker->setValidity (validity);

    {
        MyMutex::MyLock lock(cropHandler.cimg);

        if (validity == LockableColorPicker::Validity::INSIDE) {
            float r=0.f, g=0.f, b=0.f;
            float rpreview=0.f, gpreview=0.f, bpreview=0.f;
            cropHandler.colorPick(cropPos, r, g, b, rpreview, gpreview, bpreview, hoveredPicker->getSize());
            hoveredPicker->setRGB (r, g, b, rpreview, gpreview, bpreview);
        }
    }
}
void CropWindow::changeZoom (int zoom, bool notify, int centerx, int centery, bool needsRedraw)
{
    if (zoom < 0) {
        zoom = 0;
    } else if (zoom > int(zoomSteps.size())-1) {
        zoom = int(zoomSteps.size())-1;
    }

    // Limit zoom according to user preferences
    double zoomLimit = zoomLimitToFraction(App::get().options().maxZoomLimit);
    while(zoomSteps[zoom].zoom > zoomLimit && zoom != 0) {
        --zoom;
    }

    cropZoom = zoom;

    cropLabel = zoomSteps[cropZoom].label;
    cropHandler.setZoom (zoomSteps[cropZoom].czoom, centerx, centery);

    if (notify)
        for (auto listener : listeners) {
            listener->cropZoomChanged (this);
        }

    if (needsRedraw)
        iarea->redraw ();
}

LockableColorPicker::Validity CropWindow::checkValidity (LockableColorPicker*  picker, const rtengine::Coord &pos)
{

    if (!cropHandler.cropPixbuftrue) {
        return LockableColorPicker::Validity::OUTSIDE;
    }

    ImageCoord cropTopLeft = cropHandler.getPosition();
    ImageSize cropSize = cropHandler.getSize();

    rtengine::Coord cropBottomRight;
    cropBottomRight.x = cropTopLeft.x + cropSize.width;
    cropBottomRight.y = cropTopLeft.y + cropSize.height;

    rtengine::Coord pickerPos, cropPickerPos;
    picker->getImagePosition(pickerPos);
    rtengine::Coord minPos(0, 0);
    rtengine::Coord maxPos(cropHandler.cropPixbuftrue->get_width(), cropHandler.cropPixbuftrue->get_height());
    rtengine::Coord halfPickerSize((int)picker->getSize()/2, (int)picker->getSize()/2);
    imageCoordToCropImage (pickerPos.x, pickerPos.y, cropPickerPos.x, cropPickerPos.y);
    imageCoordToCropImage (cropTopLeft.x, cropTopLeft.y, minPos.x, minPos.y);
    imageCoordToCropImage (cropBottomRight.x, cropBottomRight.y, maxPos.x, maxPos.y);
    rtengine::Coord pickerMinPos = cropPickerPos - halfPickerSize;
    rtengine::Coord pickerMaxPos = cropPickerPos + halfPickerSize;
    if (pickerMaxPos.x < minPos.x || pickerMaxPos.y < minPos.y || pickerMinPos.x > maxPos.x || pickerMinPos.y > maxPos.y) {
        return LockableColorPicker::Validity::OUTSIDE;
    } else if (pickerMinPos >= minPos && pickerMaxPos < maxPos) {
        return LockableColorPicker::Validity::INSIDE;
    } else {
        return LockableColorPicker::Validity::CROSSING;
    }
}

void CropWindow::deleteColorPickers ()
{
    for (auto colorPicker : colorPickers) {
        delete colorPicker;
    }
    colorPickers.clear();
}

void CropWindow::screenCoordToCropBuffer (double phyx, double phyy, int& cropx, int& cropy)
{
    double x = phyx - windowPos.x - imgPos.x - imgAreaPos.x;
    double y = phyy - windowPos.y - imgPos.y - imgAreaPos.y;

    if (zoomSteps[cropZoom].zoom > 1.) {
        x = x / zoomSteps[cropZoom].zoom;
        y = y / zoomSteps[cropZoom].zoom;
    } else {
        float czoom = float((zoomSteps[cropZoom].czoom/10) * 10) / float(zoomSteps[cropZoom].czoom);
        x = x / czoom;
        y = y / czoom;
    }
    int deviceScale = cropHandler.getDeviceScale();
    x *= deviceScale;
    y *= deviceScale;

    rtengine::Crop* crop = static_cast<rtengine::Crop*>(cropHandler.getCrop());
    x += crop->getLeftBorder();
    y += crop->getUpperBorder();

    cropx = x;
    cropy = y;
}

void CropWindow::screenCoordToImage (double phyx, double phyy, int& imgx, int& imgy)
{
    ImageCoord cropPos = cropHandler.getPosition();

    double x = (phyx - windowPos.x - imgPos.x - imgAreaPos.x) / zoomSteps[cropZoom].zoom;
    double y = (phyy - windowPos.y - imgPos.y - imgAreaPos.y) / zoomSteps[cropZoom].zoom;

    int deviceScale = cropHandler.getDeviceScale();
    imgx = std::floor(x * deviceScale + cropPos.x);
    imgy = std::floor(y * deviceScale + cropPos.y);
}

void CropWindow::screenCoordToCropCanvas (double phyx, double phyy, int& prevx, int& prevy)
{
    double x = phyx - windowPos.x - imgAreaPos.x;
    double y = phyy - windowPos.y - imgAreaPos.y;

    int deviceScale = cropHandler.getDeviceScale();
    prevx = std::floor(x * deviceScale);
    prevy = std::floor(y * deviceScale);
}

void CropWindow::imageCoordToScreen (int imgx, int imgy, int& phyx, int& phyy)
{
    ImageCoord cropPos = cropHandler.getPosition();
    int deviceScale = cropHandler.getDeviceScale();
    phyx = (imgx - cropPos.x) * zoomSteps[cropZoom].zoom / deviceScale + windowPos.x + imgPos.x + imgAreaPos.x;
    phyy = (imgy - cropPos.y) * zoomSteps[cropZoom].zoom / deviceScale + windowPos.y + imgPos.y + imgAreaPos.y;
}

void CropWindow::imageCoordToCropCanvas (int imgx, int imgy, int& phyx, int& phyy)
{
    ImageCoord cropPos = cropHandler.getPosition();
    int deviceScale = cropHandler.getDeviceScale();
    phyx = (imgx - cropPos.x) * zoomSteps[cropZoom].zoom / deviceScale + imgPos.x;
    phyy = (imgy - cropPos.y) * zoomSteps[cropZoom].zoom / deviceScale + imgPos.y;
}

void CropWindow::imageCoordToCropBuffer (int imgx, int imgy, int& phyx, int& phyy)
{
    ImageCoord cropPos = cropHandler.getPosition();
    rtengine::Crop* crop = static_cast<rtengine::Crop*>(cropHandler.getCrop());
    phyx = (imgx - cropPos.x) * zoomSteps[cropZoom].zoom + crop->getLeftBorder();
    phyy = (imgy - cropPos.y) * zoomSteps[cropZoom].zoom + crop->getUpperBorder();
}

void CropWindow::imageCoordToCropImage (int imgx, int imgy, int& phyx, int& phyy)
{
    ImageCoord cropPos = cropHandler.getPosition();
    phyx = (imgx - cropPos.x) * zoomSteps[cropZoom].zoom;
    phyy = (imgy - cropPos.y) * zoomSteps[cropZoom].zoom;
}

int CropWindow::scaleValueToImage (int value)
{
    int deviceScale = cropHandler.getDeviceScale();
    return int(double(value) / zoomSteps[cropZoom].zoom * deviceScale);
}

float CropWindow::scaleValueToImage (float value)
{
    int deviceScale = cropHandler.getDeviceScale();
    return float(double(value) / zoomSteps[cropZoom].zoom * deviceScale);
}

double CropWindow::scaleValueToImage (double value)
{
    int deviceScale = cropHandler.getDeviceScale();
    return value / zoomSteps[cropZoom].zoom * deviceScale;
}

int CropWindow::scaleValueToCanvas (int value)
{
    int deviceScale = cropHandler.getDeviceScale();
    return int(double(value) * zoomSteps[cropZoom].zoom / deviceScale);
}

float CropWindow::scaleValueToCanvas (float value)
{
    int deviceScale = cropHandler.getDeviceScale();
    return float(double(value) * zoomSteps[cropZoom].zoom / deviceScale);
}

double CropWindow::scaleValueToCanvas (double value)
{
    int deviceScale = cropHandler.getDeviceScale();
    return value * zoomSteps[cropZoom].zoom / deviceScale;
}

void CropWindow::drawDecoration (Cairo::RefPtr<Cairo::Context> cr)
{

    int x = windowPos.x, y = windowPos.y;
    // prepare label
    Glib::RefPtr<Pango::Context> context = iarea->get_pango_context () ;
    Pango::FontDescription fontd = iarea->get_style_context()->get_font();
    fontd.set_weight (Pango::WEIGHT_BOLD);
    const int fontSize = 8; // pt
    // Non-absolute size is defined in "Pango units" and shall be multiplied by
    // Pango::SCALE from "pt":
    fontd.set_size (fontSize * Pango::SCALE);
    context->set_font_description (fontd);
    Glib::RefPtr<Pango::Layout> cllayout = iarea->create_pango_layout(cropLabel);
    int iw, ih;
    cllayout->get_pixel_size (iw, ih);

    // draw decoration (border)
    int h = windowSize.height, w = windowSize.width;

    cr->set_source_rgb (0.1, 0.1, 0.1);
    cr->set_line_width (1.0);
    cr->move_to (x + 2.5, y + titleHeight + 2.5 );
    cr->line_to (x + 2.5, y + h - 2.5);
    cr->line_to (x + w - 2.5, y + h - 2.5);
    cr->line_to (x + w - 2.5, y + titleHeight + 2.5 );

    cr->set_source_rgba (0.0, 0.0, 0.0, 0.5);
    cr->rectangle (x + 2.5, y + 0.5, w - 5, titleHeight + 2);
    cr->stroke_preserve ();
    cr->fill ();

    // draw label
    cr->set_source_rgba (1, 1, 1, 0.5);
    cr->move_to (x + 10 + sideBorderWidth + bZoomIn->getIcon()->getWidth() + bZoomOut->getIcon()->getWidth() + bZoom100->getIcon()->getWidth(), y + 1 + upperBorderWidth + (titleHeight - ih) / 2);
    cllayout->add_to_cairo_context (cr);
    cr->fill ();

    buttonSet.redraw (cr);
}

void CropWindow::drawStraightenGuide (Cairo::RefPtr<Cairo::Context> cr)
{

    if (action_x != press_x || action_y != press_y) {
        double arg = (press_x - action_x) / sqrt(double((press_x - action_x) * (press_x - action_x) + (press_y - action_y) * (press_y - action_y)));
        double sol1, sol2;
        double pi = rtengine::RT_PI;

        if (press_y > action_y) {
            sol1 = acos(arg) * 180 / pi;
            sol2 = -acos(-arg) * 180 / pi;
        } else {
            sol1 = acos(-arg) * 180 / pi;
            sol2 = -acos(arg) * 180 / pi;
        }

        if (fabs(sol1) < fabs(sol2)) {
            rot_deg = sol1;
        } else {
            rot_deg = sol2;
        }

        if (rot_deg < -45) {
            rot_deg = 90.0 + rot_deg;
        } else if (rot_deg > 45) {
            rot_deg = - 90.0 + rot_deg;
        }
    } else {
        rot_deg = 0;
    }

    Glib::RefPtr<Pango::Context> context = iarea->get_pango_context () ;
    Pango::FontDescription fontd = iarea->get_style_context()->get_font();
    fontd.set_weight (Pango::WEIGHT_BOLD);
    const int fontSize = 8; // pt
    // Non-absolute size is defined in "Pango units" and shall be multiplied by
    // Pango::SCALE from "pt":
    fontd.set_size (fontSize * Pango::SCALE);
    context->set_font_description (fontd);
    Glib::RefPtr<Pango::Layout> deglayout = iarea->create_pango_layout(Glib::ustring::compose ("%1 deg", Glib::ustring::format(std::setprecision(2), rot_deg)));

    int x1 = press_x;
    int y1 = press_y;
    int y2 = action_y;
    int x2 = action_x;
    /*    if (x1<0) x1 = 0;
        if (y1<0) y1 = 0;
        if (x2<0) x2 = 0;
        if (y2<0) y2 = 0;
        if (x2>=image->getWidth()) x2 = image->getWidth()-1;
        if (y2>=image->getHeight()) y2 = image->getHeight()-1;
        if (x1>=image->getWidth()) x1 = image->getWidth()-1;
        if (y1>=image->getHeight()) y1 = image->getHeight()-1;
    */

    cr->set_line_width (1);
    cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
    cr->move_to (x1 + 0.5, y1 + 0.5);
    cr->line_to (x2 + 0.5, y2 + 0.5);
    cr->stroke ();
    cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
    std::valarray<double> ds (1);
    ds[0] = 4;
    cr->set_dash (ds, 0);
    cr->move_to (x1 + 0.5, y1 + 0.5);
    cr->line_to (x2 + 0.5, y2 + 0.5);
    cr->stroke ();

    if (press_x != action_x && press_y != action_y) {
        cr->set_source_rgb (0.0, 0.0, 0.0);
        cr->move_to ((x1 + x2) / 2 + 1, (y1 + y2) / 2 + 1);
        deglayout->add_to_cairo_context (cr);
        cr->move_to ((x1 + x2) / 2 + 1, (y1 + y2) / 2 - 1);
        deglayout->add_to_cairo_context (cr);
        cr->move_to ((x1 + x2) / 2 - 1, (y1 + y2) / 2 + 1);
        deglayout->add_to_cairo_context (cr);
        cr->move_to ((x1 + x2) / 2 + 1, (y1 + y2) / 2 + 1);
        deglayout->add_to_cairo_context (cr);
        cr->fill ();
        cr->set_source_rgb (1.0, 1.0, 1.0);
        cr->move_to ((x1 + x2) / 2, (y1 + y2) / 2);
        deglayout->add_to_cairo_context (cr);
        cr->fill ();
    }
}

void CropWindow::drawScaledSpotRectangle (Cairo::RefPtr<Cairo::Context> cr, int rectSize)
{

    int x1 = action_x / zoomSteps[cropZoom].zoom - rectSize;
    int y1 = action_y / zoomSteps[cropZoom].zoom - rectSize;
    int y2 = action_y / zoomSteps[cropZoom].zoom + rectSize;
    int x2 = action_x / zoomSteps[cropZoom].zoom + rectSize;

    cr->set_line_width (1.0);
    cr->rectangle (windowPos.x + imgPos.x + imgAreaPos.x - 0.5, windowPos.y + imgPos.y + imgAreaPos.y - 0.5, imgAreaSize.width, imgAreaSize.height);
    cr->clip ();

    cr->set_source_rgb (1.0, 1.0, 1.0);
    cr->rectangle (x1 * zoomSteps[cropZoom].zoom - 1.5, y1 * zoomSteps[cropZoom].zoom - 1.5, x2 * zoomSteps[cropZoom].zoom - x1 * zoomSteps[cropZoom].zoom + 2, y2 * zoomSteps[cropZoom].zoom - y1 * zoomSteps[cropZoom].zoom + 2);
    cr->stroke ();
    cr->set_source_rgb (0.0, 0.0, 0.0);
    cr->rectangle (x1 * zoomSteps[cropZoom].zoom - 0.5, y1 * zoomSteps[cropZoom].zoom - 0.5, x2 * zoomSteps[cropZoom].zoom - x1 * zoomSteps[cropZoom].zoom, y2 * zoomSteps[cropZoom].zoom - y1 * zoomSteps[cropZoom].zoom);
    cr->stroke ();

    cr->reset_clip ();
}

void CropWindow::drawUnscaledSpotRectangle (Cairo::RefPtr<Cairo::Context> cr, int rectSize)
{

    int x1 = action_x - rectSize;
    int y1 = action_y - rectSize;
    int y2 = action_y + rectSize;
    int x2 = action_x + rectSize;

    cr->set_line_width (1.0);
    cr->rectangle (windowPos.x + imgPos.x + imgAreaPos.x - 0.5, windowPos.y + imgPos.y + imgAreaPos.y - 0.5, imgAreaSize.width, imgAreaSize.height);
    cr->clip ();

    cr->set_source_rgb (1.0, 1.0, 1.0);
    cr->rectangle (x1 - 1.5, y1 - 1.5, x2 - x1 + 2, y2 - y1 + 2);
    cr->stroke ();
    cr->set_source_rgb (0.0, 0.0, 0.0);
    cr->rectangle (x1 - 0.5, y1 - 0.5, x2 - x1, y2 - y1);
    cr->stroke ();

    cr->reset_clip ();
}

void CropWindow::getObservedFrameArea (int& x, int& y, int& w, int& h) const
{
    ImageCoord observedPos;
    ImageSize observedSize;
    observedCropWin->getCropRectangle(observedPos.x, observedPos.y,
                                      observedSize.width, observedSize.height);
    ImageCoord mainPos;
    ImageSize mainSize;
    getCropRectangle(mainPos.x, mainPos.y, mainSize.width, mainSize.height);

    // Translate to screen coordinates
    const int deviceScale = cropHandler.getDeviceScale();

    x = windowPos.x + imgPos.x + (observedPos.x - mainPos.x) * zoomSteps[cropZoom].zoom / deviceScale;
    y = windowPos.y + imgPos.y + (observedPos.y - mainPos.y) * zoomSteps[cropZoom].zoom / deviceScale;

    w = observedSize.width * zoomSteps[cropZoom].zoom / deviceScale;
    h = observedSize.height * zoomSteps[cropZoom].zoom / deviceScale;
}

void CropWindow::drawObservedFrame (const Cairo::RefPtr<Cairo::Context>& cr)
{
    int x, y, w, h;
    getObservedFrameArea (x, y, w, h);

    // draw a black "shadow" line
    cr->set_source_rgba( 0, 0, 0, 0.65);
    cr->set_line_width (1);
    cr->rectangle (x - 0.5, y - 0.5, w + 4, h + 4);
    cr->stroke ();

    const auto& options = App::get().options();

    // draw a "frame" line. Color of frame line can be set in preferences
    cr->set_source_rgba(options.navGuideBrush[0], options.navGuideBrush[1], options.navGuideBrush[2], options.navGuideBrush[3]); //( 1, 1, 1, 1.0);
    cr->rectangle (x - 1.5, y - 1.5, w + 4, h + 4);
    cr->stroke ();
}

void CropWindow::cropImageUpdated ()
{
    MyMutex::MyLock lock(cropHandler.cimg);

    for (auto colorPicker : colorPickers) {
        Coord imgPos, cropPos;
        colorPicker->getImagePosition(imgPos);
        imageCoordToCropImage(imgPos.x, imgPos.y, cropPos.x, cropPos.y);
        float r=0.f, g=0.f, b=0.f;
        float rpreview=0.f, gpreview=0.f, bpreview=0.f;
        colorPicker->setValidity (checkValidity (colorPicker, cropPos));
        cropHandler.colorPick(cropPos, r, g, b, rpreview, gpreview, bpreview, colorPicker->getSize());
        colorPicker->setRGB (r, g, b, rpreview, gpreview, bpreview);
    }
    iarea->redraw ();
}

void CropWindow::cropWindowChanged ()
{

    if (!decorated) {
        iarea->syncBeforeAfterViews ();
    }

    iarea->redraw ();
}

void CropWindow::initialImageArrived ()
{

    for (auto listener : listeners) {
        listener->initialImageArrived();
    }
}

void CropWindow::setDisplayPosition (hidpi::LogicalCoord pos) {
    imgPos = pos;
}

void CropWindow::remoteMove (int deltaX, int deltaY)
{

    state = SCropImgMove;
    cropHandler.moveAnchor(ImageCoord(deltaX, deltaY), false);

    for (auto listener : listeners) {
        listener->cropPositionChanged (this);
    }
}

void CropWindow::remoteMoveReady ()
{

    cropHandler.update ();
    state = SNormal;

    for (auto listener : listeners) {
        listener->cropPositionChanged (this);
    }
}

void CropWindow::delCropWindowListener (CropWindowListener* l)
{

    std::list<CropWindowListener*>::iterator i = listeners.begin();

    while (i != listeners.end())
        if (*i == l) {
            i = listeners.erase (i);
        } else {
            ++i;
        }
}

ImageArea* CropWindow::getImageArea()
{
    return iarea;
}

void CropWindow::setCropGUIListener       (CropGUIListener* cgl)
{
    cropgl = cgl;
}

void CropWindow::setPointerMotionListener (PointerMotionListener* pml)
{
    pmlistener = pml;
    if (pml) {
        pml->signal_cycle_rgb().connect( sigc::mem_fun(*this, &CropWindow::cycleRGB) );
        pml->signal_cycle_hsv().connect( sigc::mem_fun(*this, &CropWindow::cycleHSV) );
    }
}

PointerMotionListener* CropWindow::getPointerMotionListener ()
{
    return pmlistener;
}

void CropWindow::setPointerMotionHListener (PointerMotionListener* pml)
{
    pmhlistener = pml;
}

// crop window listeners
void CropWindow::addCropWindowListener (CropWindowListener* l)
{
    listeners.push_back (l);
}

void CropWindow::cycleRGB ()
{
    bool redraw = false;
    for (auto colorPicker : colorPickers) {
        redraw |= colorPicker->cycleRGB ();
    }

    if (redraw) {
        iarea->redraw ();
    }
}

void CropWindow::cycleHSV ()
{
    bool redraw = false;
    for (auto colorPicker : colorPickers) {
        redraw |= colorPicker->cycleHSV ();
    }

    if (redraw) {
        iarea->redraw ();
    }
}
