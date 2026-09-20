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
#include "lwbuttonset.h"
#include "lwbutton.h"
#include "rtscalable.h"

LWButtonSet::LWButtonSet() : aw(0), ah(0), ax(-1), ay(-1) {}

LWButtonSet::~LWButtonSet()
{
    for (const auto entry : buttons) {
        delete entry;
    }
}

void LWButtonSet::add(LWButton* b)
{
    buttons.push_back(b);
}

void LWButtonSet::getMinimalDimensions(int& w, int& h) const
{
    w = 0;
    h = 0;

    for (const auto entry : buttons) {
        int bw, bh;
        entry->getSize(bw, bh);
        w += bw;
        h = std::max(bh, h);
    }
}

void LWButtonSet::arrangeButtons(int x, int y, int w, int h)
{

    if (x == ax && y == ay && w == aw && (h == -1 || h == ah)) {
        return;
    }

    int mw, mh;
    getMinimalDimensions(mw, mh);

    if (w < 0) {
        w = mw;
    }

    if (h < 0) {
        h = mh;
    }

    int begx = x;
    int endx = x + w - 1;

    for (size_t i = 0; i < buttons.size(); i++) {
        LWButton::Alignment halign, valign;
        int bx = 0, by = 0, bw = 0, bh = 0;
        buttons[i]->getSize(bw, bh);
        buttons[i]->getAlignment(halign, valign);

        if (halign == LWButton::Left) {
            bx = begx;
            begx += bw;
        } else if (halign == LWButton::Right) {
            bx = endx - bw;
            endx -= bw;
        }

        if (valign == LWButton::Top) {
            by = y;
        } else if (valign == LWButton::Bottom) {
            by = y + h - bh - 1;
        } else if (valign == LWButton::Center) {
            by = y + (h - bh) / 2;
        }

        buttons[i]->setPosition(bx, by);
    }

    aw = w;
    ah = h;
    ax = x;
    ay = y;
}

void LWButtonSet::move(int nx, int ny)
{
    for (const auto entry : buttons) {
        entry->addPosition(nx - ax, ny - ay);
    }
    ax = nx;
    ay = ny;
}

void LWButtonSet::redraw(Cairo::RefPtr<Cairo::Context> context)
{
    for (const auto entry : buttons) {
        entry->redraw(context);
    }
}

bool LWButtonSet::motionNotify(int x, int y)
{
    bool res = false;
    for (const auto entry : buttons) {
        res = entry->motionNotify(x, y) || res;
    }
    return res;
}

bool LWButtonSet::pressNotify(int x, int y)
{
    bool res = false;
    for (const auto entry : buttons) {
        res = entry->pressNotify(x, y) || res;
    }
    return res;
}

bool LWButtonSet::releaseNotify(int x, int y)
{
    bool res = false;
    for (const auto entry : buttons) {
        res = entry->releaseNotify(x, y) || res;
    }
    return res;
}

bool LWButtonSet::inside(int x, int y) const
{

    for (const auto entry : buttons) {
        if (entry->inside(x, y)) {
            return true;
        }
    }
    return false;
}

void LWButtonSet::setButtonListener(LWButtonListener* bl)
{
    for (const auto entry : buttons) {
        entry->setButtonListener(bl);
    }
}

void LWButtonSet::getAllocatedDimensions(int& w, int& h) const
{
    w = aw;
    h = ah;
}

void LWButtonSet::setColors(const Gdk::RGBA& bg, const Gdk::RGBA& fg)
{
    for (const auto entry : buttons) {
        entry->setColors(bg, fg);
    }
}

Glib::ustring LWButtonSet::getToolTip(int x, int y) const
{
    for (const auto entry : buttons) {
        const auto ttip = entry->getToolTip(x, y);

        if (!ttip.empty()) {
            return ttip;
        }
    }
    return {};
}
