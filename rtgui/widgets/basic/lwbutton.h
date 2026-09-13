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
#include "rtsurface.h"
class LWButton;

class LWButtonListener
{
public:
    virtual ~LWButtonListener() = default;
    virtual void buttonPressed(LWButton* button, int actionCode, void* actionData, int x, int y)  = 0;
    virtual void redrawNeeded(LWButton* button) = 0;
};

class LWButton
{

public:
    enum Alignment {Left, Right, Top, Bottom, Center};
    enum State { Normal, Over, Pressed_In, Pressed_Out};

private:
    int xpos, ypos, w, h;
    Alignment halign, valign;
    std::shared_ptr<RTSurface> icon;
    double bgr, bgg, bgb;
    double fgr, fgg, fgb;
    State state;
    LWButtonListener* listener;
    int actionCode;
    void* actionData;
    Glib::ustring* toolTip;

public:
    LWButton (std::shared_ptr<RTSurface> i, int aCode, void* aData, Alignment ha = Left, Alignment va = Center, Glib::ustring* tooltip = nullptr);

    void    getSize             (int& minw, int& minh) const;
    void    getAlignment        (Alignment& ha, Alignment& va) const;
    void    setPosition         (int x, int y);
    void    addPosition         (int x, int y);
    void    getPosition         (int& x, int& y) const;
    bool    inside              (int x, int y) const;
    void    setIcon             (std::shared_ptr<RTSurface> i);
    std::shared_ptr<RTSurface>  getIcon () const;
    void    setColors           (const Gdk::RGBA& bg, const Gdk::RGBA& fg);
    void    setToolTip          (Glib::ustring* tooltip);

    bool    motionNotify        (int x, int y);
    bool    pressNotify         (int x, int y);
    bool    releaseNotify       (int x, int y);

    Glib::ustring getToolTip (int x, int y) const;

    void    setButtonListener   (LWButtonListener* bl)
    {
        listener = bl;
    }

    void    redraw              (Cairo::RefPtr<Cairo::Context> context);
};
