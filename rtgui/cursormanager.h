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

enum CursorShape {
    CSAddColPicker,
    CSArrow,
    CSCropSelect,
    CSCrosshair,
    CSEmpty,
    CSHandClosed,
    CSHandOpen,
    CSMove,
    CSMove1DH,
    CSMove1DV,
    CSMove2D,
    CSMoveLeft,
    CSMoveRight,
    CSMoveRotate,
    CSPlus,
    CSResizeBottomLeft,
    CSResizeBottomRight,
    CSResizeDiagonal,
    CSResizeHeight,
    CSResizeTopLeft,
    CSResizeTopRight,
    CSResizeWidth,
    CSSpotWB,
    CSStraighten,
    CSUndefined,
    CSWait
};

class CursorManager
{
public:
    static CursorManager& forMainWindow();
    static CursorManager& forEditWindow();

    static void setWidgetCursor(const Glib::RefPtr<Gdk::Window>& window, CursorShape shape);
    static void setCursorOfMainWindow(const Glib::RefPtr<Gdk::Window>& window, CursorShape shape);

    void init(const Glib::RefPtr<Gdk::Window>& window);
    void setCursor(CursorShape shape);

private:
    Glib::RefPtr<Gdk::Cursor> cAdd;
    Glib::RefPtr<Gdk::Cursor> cAddPicker;
    Glib::RefPtr<Gdk::Cursor> cCropDraw;
    Glib::RefPtr<Gdk::Cursor> cCrosshair;
    Glib::RefPtr<Gdk::Cursor> cHandClosed;
    Glib::RefPtr<Gdk::Cursor> cHandOpen;
    Glib::RefPtr<Gdk::Cursor> cEmpty;
    Glib::RefPtr<Gdk::Cursor> cMoveBL;
    Glib::RefPtr<Gdk::Cursor> cMoveBR;
    Glib::RefPtr<Gdk::Cursor> cMoveL;
    Glib::RefPtr<Gdk::Cursor> cMoveR;
    Glib::RefPtr<Gdk::Cursor> cMoveTL;
    Glib::RefPtr<Gdk::Cursor> cMoveTR;
    Glib::RefPtr<Gdk::Cursor> cMoveX;
    Glib::RefPtr<Gdk::Cursor> cMoveY;
    Glib::RefPtr<Gdk::Cursor> cMoveXY;
    Glib::RefPtr<Gdk::Cursor> cRotate;
    Glib::RefPtr<Gdk::Cursor> cWB;
    Glib::RefPtr<Gdk::Cursor> cWait;

    Glib::RefPtr<Gdk::Display> m_display;
    Glib::RefPtr<Gdk::Window> m_window;

    void setCursor(const Glib::RefPtr<Gdk::Window>& window, CursorShape shape);
};
