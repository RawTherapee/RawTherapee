/*
 *  This file is part of RawTherapee.
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
#include <numeric>

#include <glibmm/ustring.h>

#include "hidpi.h"
#include "inspector.h"
#include "multilangmgr.h"
#include "options.h"
#include "rtscalable.h"
#include "thumbbrowserbase.h"
#include "thumbbrowserentrybase.h"

#include "rtengine/rt_math.h"

using namespace std;

ThumbBrowserBase::ThumbBrowserBase ()
    : location(THLOC_FILEBROWSER), inspector(nullptr), isInspectorActive(false), eventTime(0), lastClicked(nullptr), anchor(nullptr), previewHeight(App::get().options().thumbSize), numOfCols(1), lastRowHeight(0), arrangement(TB_Horizontal)
{
    lastDeviceScale = 0;
    inW = -1;
    inH = -1;

    hscroll.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    vscroll.set_orientation(Gtk::ORIENTATION_VERTICAL);

    setExpandAlignProperties(&internal, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    setExpandAlignProperties(&hscroll, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(&vscroll, false, true, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    attach (internal, 0, 0, 1, 1);
    attach (vscroll, 1, 0, 1, 1);
    attach (hscroll, 0, 1, 1, 1);

    internal.setParent (this);

    show_all ();

    vscroll.get_adjustment()->set_lower(0);
    hscroll.get_adjustment()->set_lower(0);
    vscroll.signal_value_changed().connect( sigc::mem_fun(*this, &ThumbBrowserBase::scrollChanged) );
    hscroll.signal_value_changed().connect( sigc::mem_fun(*this, &ThumbBrowserBase::scrollChanged) );

    internal.signal_size_allocate().connect( sigc::mem_fun(*this, &ThumbBrowserBase::internalAreaResized) );
}

void ThumbBrowserBase::scrollChanged ()
{
    {
        MYWRITERLOCK(l, entryRW);

        for (size_t i = 0; i < fd.size(); i++) {
            fd[i]->setOffset ((int)(hscroll.get_value()), (int)(vscroll.get_value()));
        }
    }

    internal.setPosition ((int)(hscroll.get_value()), (int)(vscroll.get_value()));

    if (!internal.isDirty()) {
        internal.setDirty ();
        internal.queue_draw ();
    }
}

void ThumbBrowserBase::scroll (int direction, double deltaX, double deltaY)
{
    double delta = 0.0;
    if (abs(deltaX) > abs(deltaY)) {
        delta = deltaX;
    } else {
        delta = deltaY;
    }
    if (direction == GDK_SCROLL_SMOOTH && delta == 0.0) {
        // sometimes this case happens. To avoid scrolling the wrong direction in this case, we just do nothing
        // This is probably no longer necessary now that coef is no longer quantized to +/-1.0 but why waste CPU cycles?
        return;
    }
    //GDK_SCROLL_SMOOTH can come in as many events with small deltas, don't quantize these to +/-1.0 so trackpads work well
    double coef;
    double scroll_unit;
    if (arrangement == TB_Vertical) {
        scroll_unit = vscroll.get_adjustment()->get_step_increment();
    } else {
        scroll_unit = hscroll.get_adjustment()->get_step_increment();
    }
    if(direction == GDK_SCROLL_SMOOTH) {
#ifdef GDK_WINDOWING_QUARTZ
        scroll_unit = 1.0;
#endif
        coef = delta;
    } else if (direction == GDK_SCROLL_DOWN) {
        coef = +1.0;
    } else {
        coef = -1.0;
    }

    // GUI already acquired when here
    if (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_DOWN || direction == GDK_SCROLL_SMOOTH) {
        if (arrangement == TB_Vertical) {
            double currValue = vscroll.get_value();
            double newValue = rtengine::LIM<double>(currValue + coef * scroll_unit,
                                                    vscroll.get_adjustment()->get_lower (),
                                                    vscroll.get_adjustment()->get_upper());
            if (newValue != currValue) {
                vscroll.set_value (newValue);
            }
        } else {
            double currValue = hscroll.get_value();
            double newValue = rtengine::LIM<double>(currValue + coef * scroll_unit,
                                                    hscroll.get_adjustment()->get_lower(),
                                                    hscroll.get_adjustment()->get_upper());
            if (newValue != currValue) {
                hscroll.set_value (newValue);
            }
        }
    }
}

void ThumbBrowserBase::scrollPage (int direction)
{
    // GUI already acquired when here
    // GUI already acquired when here
    if (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_DOWN) {
        if (arrangement == TB_Vertical) {
            double currValue = vscroll.get_value();
            double newValue = rtengine::LIM<double>(currValue + (direction == GDK_SCROLL_DOWN ? +1 : -1) * vscroll.get_adjustment()->get_page_increment(),
                                                    vscroll.get_adjustment()->get_lower(),
                                                    vscroll.get_adjustment()->get_upper());
            if (newValue != currValue) {
                vscroll.set_value (newValue);
            }
        } else {
            double currValue = hscroll.get_value();
            double newValue = rtengine::LIM<double>(currValue + (direction == GDK_SCROLL_DOWN ? +1 : -1) * hscroll.get_adjustment()->get_page_increment(),
                                                    hscroll.get_adjustment()->get_lower(),
                                                    hscroll.get_adjustment()->get_upper());
            if (newValue != currValue) {
                hscroll.set_value (newValue);
            }
        }
    }
}

namespace
{

typedef std::vector<ThumbBrowserEntryBase*> ThumbVector;
typedef ThumbVector::iterator ThumbIterator;

inline void clearSelection (ThumbVector& selected)
{
    for (ThumbIterator thumb = selected.begin (); thumb != selected.end (); ++thumb)
        (*thumb)->selected = false;

    selected.clear ();
}

inline void addToSelection (ThumbBrowserEntryBase* entry, ThumbVector& selected)
{
    if (entry->selected || entry->filtered)
        return;

    entry->selected = true;
    selected.push_back (entry);
}

inline void removeFromSelection (const ThumbIterator& iterator, ThumbVector& selected)
{
    (*iterator)->selected = false;
    selected.erase (iterator);
}

}

void ThumbBrowserBase::selectSingle (ThumbBrowserEntryBase* clicked)
{
    clearSelection(selected);
    anchor = clicked;

    if (clicked) {
        addToSelection(clicked, selected);
    }
}

void ThumbBrowserBase::selectRange (ThumbBrowserEntryBase* clicked, bool additional)
{
    if (!anchor) {
        anchor = clicked;
        if (selected.empty()) {
            addToSelection(clicked, selected);
            return;
        }
    }

    if (!additional || !lastClicked) {
        // Extend the current range w.r.t to first selected entry.
        ThumbIterator back = std::find(fd.begin(), fd.end(), clicked);
        ThumbIterator front = anchor == clicked ? back : std::find(fd.begin(), fd.end(), anchor);

        if (front > back) {
            std::swap(front, back);
        }

        clearSelection(selected);

        for (; front <= back && front != fd.end(); ++front) {
            addToSelection(*front, selected);
        }
    } else {
        // Add an additional range w.r.t. the last clicked entry.
        ThumbIterator last = std::find(fd.begin(), fd.end(), lastClicked);
        ThumbIterator current = std::find(fd.begin(), fd.end(), clicked);

        if (last > current) {
            std::swap(last, current);
        }

        for (; last <= current && last != fd.end(); ++last) {
            addToSelection(*last, selected);
        }
    }
}

void ThumbBrowserBase::selectSet (ThumbBrowserEntryBase* clicked)
{
    const ThumbIterator iterator = std::find(selected.begin(), selected.end(), clicked);

    if (iterator != selected.end()) {
        removeFromSelection(iterator, selected);
    } else {
        addToSelection(clicked, selected);
    }
    anchor = clicked;
}

static void scrollToEntry (double& h, double& v, int iw, int ih, ThumbBrowserEntryBase* entry)
{
    const int hMin = entry->getX();
    const int hMax = hMin + entry->getEffectiveWidth() - iw;
    const int vMin = entry->getY();
    const int vMax = vMin + entry->getEffectiveHeight() - ih;

    if (hMin < 0) {
        h += hMin;
    } else if (hMax > 0) {
        h += hMax;
    }

    if (vMin < 0) {
        v += vMin;
    } else if (vMax > 0) {
        v += vMax;
    }
}

void ThumbBrowserBase::selectPrev (int distance, bool enlarge)
{
    double h, v;
    getScrollPosition (h, v);

    {
        MYWRITERLOCK(l, entryRW);

        if (!selected.empty ()) {
            std::vector<ThumbBrowserEntryBase*>::iterator front = std::find (fd.begin (), fd.end (), selected.front ());
            std::vector<ThumbBrowserEntryBase*>::iterator back = std::find (fd.begin (), fd.end (), selected.back ());
            std::vector<ThumbBrowserEntryBase*>::iterator last = std::find (fd.begin (), fd.end (), lastClicked);

            if (front > back) {
                std::swap(front, back);
            }

            std::vector<ThumbBrowserEntryBase*>::iterator& curr = last == front ? front : back;

            // find next thumbnail at filtered distance before current
            for (; curr >= fd.begin (); --curr) {
                if (!(*curr)->filtered) {
                    if (distance-- == 0) {
                        // clear current selection
                        for (size_t i = 0; i < selected.size (); ++i) {
                            selected[i]->selected = false;
                            redrawNeeded (selected[i]);
                        }

                        selected.clear ();

                        // make sure the newly selected thumbnail is visible and make it current
                        scrollToEntry (h, v, internal.get_width (), internal.get_height (), *curr);
                        lastClicked = *curr;

                        // either enlarge current selection or set new selection
                        if(enlarge) {
                            // reverse direction if distance is too large
                            if(front > back) {
                                std::swap(front, back);
                            }

                            for (; front <= back; ++front) {
                                if (!(*front)->filtered) {
                                    (*front)->selected = true;
                                    redrawNeeded (*front);
                                    selected.push_back (*front);
                                }
                            }
                        } else {
                            (*curr)->selected = true;
                            redrawNeeded (*curr);
                            selected.push_back (*curr);
                        }

                        break;
                    }
                }
            }
        }

        MYWRITERLOCK_RELEASE(l);
        selectionChanged ();
    }

    setScrollPosition (h, v);
}

void ThumbBrowserBase::selectNext (int distance, bool enlarge)
{
    double h, v;
    getScrollPosition (h, v);

    {
        MYWRITERLOCK(l, entryRW);

        if (!selected.empty ()) {
            std::vector<ThumbBrowserEntryBase*>::iterator front = std::find (fd.begin (), fd.end (), selected.front ());
            std::vector<ThumbBrowserEntryBase*>::iterator back = std::find (fd.begin (), fd.end (), selected.back ());
            std::vector<ThumbBrowserEntryBase*>::iterator last = std::find (fd.begin (), fd.end (), lastClicked);

            if (front > back) {
                std::swap(front, back);
            }

            std::vector<ThumbBrowserEntryBase*>::iterator& curr = last == back ? back : front;

            // find next thumbnail at filtered distance after current
            for (; curr < fd.end (); ++curr) {
                if (!(*curr)->filtered) {
                    if (distance-- == 0) {
                        // clear current selection
                        for (size_t i = 0; i < selected.size (); ++i) {
                            selected[i]->selected = false;
                            redrawNeeded (selected[i]);
                        }

                        selected.clear ();

                        // make sure the newly selected thumbnail is visible and make it current
                        scrollToEntry (h, v, internal.get_width (), internal.get_height (), *curr);
                        lastClicked = *curr;

                        // either enlarge current selection or set new selection
                        if(enlarge) {
                            // reverse direction if distance is too large
                            if(front > back) {
                                std::swap(front, back);
                            }

                            for (; front <= back && front != fd.end(); ++front) {
                                if (!(*front)->filtered) {
                                    (*front)->selected = true;
                                    redrawNeeded (*front);
                                    selected.push_back (*front);
                                }
                            }
                        } else {
                            (*curr)->selected = true;
                            redrawNeeded (*curr);
                            selected.push_back (*curr);
                        }

                        break;
                    }
                }
            }
        }

        MYWRITERLOCK_RELEASE(l);
        selectionChanged ();
    }

    setScrollPosition (h, v);
}

void ThumbBrowserBase::selectFirst (bool enlarge)
{
    double h, v;
    getScrollPosition (h, v);

    {
        MYWRITERLOCK(l, entryRW);

        if (!fd.empty ()) {
            // find first unfiltered entry
            std::vector<ThumbBrowserEntryBase*>::iterator first = fd.begin ();

            for (; first < fd.end (); ++first) {
                if (!(*first)->filtered) {
                    break;
                }
            }

            scrollToEntry (h, v, internal.get_width (), internal.get_height (), *first);

            ThumbBrowserEntryBase* lastEntry = lastClicked;
            lastClicked = *first;

            if(selected.empty ()) {
                (*first)->selected = true;
                redrawNeeded (*first);
                selected.push_back (*first);
            } else {
                std::vector<ThumbBrowserEntryBase*>::iterator back = std::find (fd.begin (), fd.end (), lastEntry ? lastEntry : selected.back ());

                if (first > back) {
                    std::swap(first, back);
                }

                // clear current selection
                for (size_t i = 0; i < selected.size (); ++i) {
                    selected[i]->selected = false;
                    redrawNeeded (selected[i]);
                }

                selected.clear ();

                // either enlarge current selection or set new selection
                for (; first <= back; ++first) {
                    if (!(*first)->filtered) {
                        (*first)->selected = true;
                        redrawNeeded (*first);
                        selected.push_back (*first);
                    }

                    if (!enlarge) {
                        break;
                    }
                }
            }
        }

        MYWRITERLOCK_RELEASE(l);
        selectionChanged ();
    }

    setScrollPosition (h, v);
}

void ThumbBrowserBase::selectLast (bool enlarge)
{
    double h, v;
    getScrollPosition (h, v);

    {
        MYWRITERLOCK(l, entryRW);

        if (!fd.empty ()) {
            // find last unfiltered entry
            std::vector<ThumbBrowserEntryBase*>::iterator last = fd.end () - 1;

            for (; last >= fd.begin (); --last) {
                if (!(*last)->filtered) {
                    break;
                }
            }

            scrollToEntry (h, v, internal.get_width (), internal.get_height (), *last);

            ThumbBrowserEntryBase* lastEntry = lastClicked;
            lastClicked = *last;

            if(selected.empty()) {
                (*last)->selected = true;
                redrawNeeded (*last);
                selected.push_back (*last);
            } else {
                std::vector<ThumbBrowserEntryBase*>::iterator front = std::find (fd.begin (), fd.end (), lastEntry ? lastEntry : selected.front ());

                if (last < front) {
                    std::swap(last, front);
                }

                // clear current selection
                for (size_t i = 0; i < selected.size (); ++i) {
                    selected[i]->selected = false;
                    redrawNeeded (selected[i]);
                }

                selected.clear ();

                // either enlarge current selection or set new selection
                for (; front <= last; --last) {
                    if (!(*last)->filtered) {
                        (*last)->selected = true;
                        redrawNeeded (*last);
                        selected.push_back (*last);
                    }

                    if (!enlarge) {
                        break;
                    }
                }

                std::reverse(selected.begin (), selected.end ());
            }
        }

        MYWRITERLOCK_RELEASE(l);
        selectionChanged ();
    }

    setScrollPosition (h, v);
}

void ThumbBrowserBase::resizeThumbnailArea (int w, int h)
{

    inW = w;
    inH = h;

    if (hscroll.get_value() + internal.get_width() > inW) {
        hscroll.set_value (inW - internal.get_width());
    }

    if (vscroll.get_value() + internal.get_height() > inH) {
        vscroll.set_value (inH - internal.get_height());
    }

    configScrollBars ();
}

void ThumbBrowserBase::internalAreaResized (Gtk::Allocation& req)
{

    if (inW > 0 && inH > 0) {
        configScrollBars ();
        redraw ();
    }
}

void ThumbBrowserBase::onInternalAreaDraw()
{
    int deviceScale = RTScalable::getScaleForWidget(this);
    if (deviceScale == lastDeviceScale) return;

    lastDeviceScale = deviceScale;

    MYWRITERLOCK(l, entryRW);
    for (auto& entry : fd) {
        entry->onDeviceScaleChanged(deviceScale);
    }
}

void ThumbBrowserBase::configScrollBars ()
{

    // HOMBRE:DELETE ME?
    GThreadLock tLock; // Acquire the GUI

    if (inW > 0 && inH > 0) {
        int ih = internal.get_height();
        if (arrangement == TB_Horizontal) {
            auto ha = hscroll.get_adjustment();
            int iw = internal.get_width();
            ha->set_upper(inW);
            ha->set_step_increment(!fd.empty() ? fd[0]->getEffectiveWidth() : 0);
            ha->set_page_increment(iw);
            ha->set_page_size(iw);
            if (iw >= inW) {
                hscroll.hide();
            } else {
                hscroll.show();
            }
        } else {
            hscroll.hide();
        }

        auto va = vscroll.get_adjustment();
        va->set_upper(inH);
        const auto height = !fd.empty() ? fd[0]->getEffectiveHeight() : 0;
        va->set_step_increment(height);
        va->set_page_increment(height == 0 ? ih : (ih / height) * height);
        va->set_page_size(ih);

        if (ih >= inH) {
            vscroll.hide();
        } else {
            vscroll.show();
        }
    }
}

void ThumbBrowserBase::arrangeFiles(ThumbBrowserEntryBase* entry)
{

    if (fd.empty()) {
        // nothing to arrange
        resizeThumbnailArea(0, 0);
        return;
    }
    if(entry && entry->filtered) {
        // a filtered entry was added, nothing to arrange, but has to be marked not drawable
        MYREADERLOCK(l, entryRW);
        entry->drawable = false;
        MYREADERLOCK_RELEASE(l);
        return;
    }

    MYREADERLOCK(l, entryRW);

    // GUI already locked by ::redraw, the only caller of this method for now.
    // We could lock it one more time, there's no harm excepted (negligible) speed penalty
    //GThreadLock lock;

    int rowHeight = 0;
    if (entry) {
        // we got the reference to the added entry, makes calculation of rowHeight O(1)
        lastRowHeight = rowHeight = std::max(lastRowHeight, entry->getMinimalHeight());
    } else {

        lastRowHeight = 0;
        for (const auto thumb : fd) {
            // apply filter
            thumb->filtered = !checkFilter(thumb);
            // compute max rowHeight
            if (!thumb->filtered) {
                rowHeight = std::max(thumb->getMinimalHeight(), rowHeight);
            }
        }
    }

    if (arrangement == TB_Horizontal) {
        numOfCols = 1;

        int currx = 0;

        for (unsigned int ct = 0; ct < fd.size(); ++ct) {
            // arrange items in the column

            for (; ct < fd.size() && fd[ct]->filtered; ++ct) {
                fd[ct]->drawable = false;
            }

            if (ct < fd.size()) {
                const int maxw = fd[ct]->getMinimalWidth();

                fd[ct]->setPosition(currx, 0, maxw, rowHeight);
                fd[ct]->drawable = true;
                currx += maxw;
            }
        }

        MYREADERLOCK_RELEASE(l);
        // This will require a Writer access
        resizeThumbnailArea(currx, !fd.empty() ? fd[0]->getEffectiveHeight() : rowHeight);
    } else {
        const int availWidth = internal.get_width();

        // initial number of columns
        int oldNumOfCols = numOfCols;
        numOfCols = 0;
        int colsWidth = 0;

        for (unsigned int i = 0; i < fd.size(); ++i) {
            if (!fd[i]->filtered && colsWidth + fd[i]->getMinimalWidth() <= availWidth) {
                colsWidth += fd[i]->getMinimalWidth();
                ++numOfCols;
                if(colsWidth > availWidth) {
                    --numOfCols;
                    break;
                }
            }
        }

        if (numOfCols < 1) {
            numOfCols = 1;
        }

        std::vector<int> colWidths;

        for (; numOfCols > 0; --numOfCols) {
            // compute column widths
            colWidths.assign(numOfCols, 0);

            for (unsigned int i = 0, j = 0; i < fd.size(); ++i) {
                if (!fd[i]->filtered && fd[i]->getMinimalWidth() > colWidths[j % numOfCols]) {
                    colWidths[j % numOfCols] = fd[i]->getMinimalWidth();
                }

                if (!fd[i]->filtered) {
                    ++j;
                }
            }

            // if not wider than the space available, arrange it and we are ready
            colsWidth = std::accumulate(colWidths.begin(), colWidths.end(), 0);

            if (numOfCols == 1 || colsWidth < availWidth) {
                break;
            }
        }

        // arrange files
        int curry = 0;
        size_t ct = 0;
        if (entry) {
            std::vector<int> oldColWidths;
            if (oldNumOfCols == numOfCols) {
                for (; oldNumOfCols > 0; --oldNumOfCols) {
                    // compute old column widths
                    oldColWidths.assign(oldNumOfCols, 0);

                    for (unsigned int i = 0, j = 0; i < fd.size(); ++i) {
                        if (fd[i] != entry && !fd[i]->filtered && fd[i]->getMinimalWidth() > oldColWidths[j % oldNumOfCols]) {
                            oldColWidths[j % oldNumOfCols] = fd[i]->getMinimalWidth();
                        }

                        if (fd[i] != entry && !fd[i]->filtered) {
                            ++j;
                        }
                    }
                    if (oldNumOfCols == 1 || std::accumulate(oldColWidths.begin(), oldColWidths.end(), 0) < availWidth) {
                        break;
                    }
                }
            }

            bool arrangeAll = true;
            if (oldNumOfCols == numOfCols) {
                arrangeAll = false;
                for (int i = 0; i < numOfCols; ++i) {
                    if(colWidths[i] != oldColWidths[i]) {
                        arrangeAll = true;
                        break;
                    }
                }
            }
            if (!arrangeAll) {
                int j = 0;
                // Find currently added entry
                for (; ct < fd.size() && fd[ct] != entry; j += !fd[ct]->filtered, ++ct) {
                }
                //Calculate the position of currently added entry
                const int row = j / numOfCols;
                const int col = j % numOfCols;
                curry = row * rowHeight;
                int currx = 0;
                for (int c = 0; c < col; ++c) {
                    currx += colWidths[c];
                }
                // arrange all entries in the row beginning with the currently added one
                for (int i = col; ct < fd.size() && i < numOfCols; ++i, ++ct) {
                    for (; ct < fd.size() && fd[ct]->filtered; ++ct) {
                        fd[ct]->drawable = false;
                    }
                    if (ct < fd.size()) {
                        fd[ct]->setPosition(currx, curry, colWidths[i], rowHeight);
                        fd[ct]->drawable = true;
                        currx += colWidths[i];
                    }
                }

                if (currx > 0) { // there were thumbnails placed in the row
                    curry += rowHeight;
                }
            }
        }

        // arrange remaining entries, if any, that's the most expensive part
        for (; ct < fd.size();) {

            // arrange items in the row
            int currx = 0;

            for (int i = 0; ct < fd.size() && i < numOfCols; ++i, ++ct) {
                for (; ct < fd.size() && fd[ct]->filtered; ++ct) {
                    // Thumbs that are not going be drawn should also have a minimum height and width. Cause
                    // the properties might be used in other parts of the code. The position is just set to be
                    // zero as a default.
                    fd[ct]->setPosition(0, 0, colWidths[i], rowHeight);

                    fd[ct]->drawable = false;
                }

                if (ct < fd.size()) {
                    fd[ct]->setPosition(currx, curry, colWidths[i], rowHeight);
                    fd[ct]->drawable = true;
                    currx += colWidths[i];
                }
            }

            if (currx > 0) { // there were thumbnails placed in the row
                curry += rowHeight;
            }
        }

        MYREADERLOCK_RELEASE(l);
        // This will require a Writer access
        resizeThumbnailArea(colsWidth, curry);
    }
}

void ThumbBrowserBase::disableInspector()
{
    if (inspector) {
        inspector->setActive(false);
    }
}

void ThumbBrowserBase::enableInspector()
{
    if (inspector) {
        inspector->setActive(true);
    }
}

bool ThumbBrowserBase::Internal::on_configure_event(GdkEventConfigure *configure_event)
{
    return true;
}

void ThumbBrowserBase::Internal::on_style_updated()
{
    style = get_style_context ();
    textn = style->get_color(Gtk::STATE_FLAG_NORMAL);
    texts = style->get_color(Gtk::STATE_FLAG_SELECTED);
    bgn = style->get_background_color(Gtk::STATE_FLAG_NORMAL);
    bgs = style->get_background_color(Gtk::STATE_FLAG_SELECTED);
}

void ThumbBrowserBase::Internal::on_realize()
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    Cairo::FontOptions cfo;
    cfo.set_antialias (Cairo::ANTIALIAS_SUBPIXEL);
    get_pango_context()->set_cairo_font_options (cfo);

    Gtk::DrawingArea::on_realize();

    style = get_style_context ();
    textn = style->get_color(Gtk::STATE_FLAG_NORMAL);
    texts = style->get_color(Gtk::STATE_FLAG_SELECTED);
    bgn = style->get_background_color(Gtk::STATE_FLAG_NORMAL);
    bgs = style->get_background_color(Gtk::STATE_FLAG_SELECTED);

    set_can_focus(true);
    add_events(Gdk::EXPOSURE_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK | Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK | Gdk::KEY_PRESS_MASK);
    set_has_tooltip (true);
    signal_query_tooltip().connect( sigc::mem_fun(*this, &ThumbBrowserBase::Internal::on_query_tooltip) );
}

bool ThumbBrowserBase::Internal::on_query_tooltip (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    Glib::ustring ttip;
    bool useMarkup = false;
    {
        MYREADERLOCK(l, parent->entryRW);

        for (size_t i = 0; i < parent->fd.size(); i++)
            if (parent->fd[i]->drawable && parent->fd[i]->inside (x, y)) {
                std::tie(ttip, useMarkup) = parent->fd[i]->getToolTip (x, y);
                break;
            }
    }

    if (!ttip.empty()) {
        if (useMarkup) {
            tooltip->set_markup(ttip);
        } else {
            tooltip->set_text(ttip);
        }
        return true;
    } else {
        return false;
    }
}

void ThumbBrowserBase::on_style_updated ()
{
    // GUI will be acquired by refreshThumbImages
    refreshThumbImages ();
}

ThumbBrowserBase::Internal::Internal () : ofsX(0), ofsY(0), parent(nullptr), dirty(true)
{
    set_name("FileCatalog");
}

void ThumbBrowserBase::Internal::setParent (ThumbBrowserBase* p)
{
    parent = p;
}

void ThumbBrowserBase::Internal::setPosition (int x, int y)
{
    ofsX = x;
    ofsY = y;
}

bool ThumbBrowserBase::Internal::on_key_press_event (GdkEventKey* event)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    return parent->keyPressed (event);
}

bool ThumbBrowserBase::Internal::on_button_press_event (GdkEventButton* event)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    grab_focus ();

    parent->eventTime = event->time;

    parent->buttonPressed ((int)event->x, (int)event->y, event->button, event->type, event->state, 0, 0, get_width(), get_height());
    Glib::RefPtr<Gdk::Window> window = get_window();

    GdkRectangle rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = window->get_width();
    rect.height = window->get_height();

    gdk_window_invalidate_rect (window->gobj(), &rect, true);
    gdk_window_process_updates (window->gobj(), true);

    return true;
}

void ThumbBrowserBase::buttonPressed (int x, int y, int button, GdkEventType type, int state, int clx, int cly, int clw, int clh)
{
    // GUI already acquired

    ThumbBrowserEntryBase* fileDescr = nullptr;
    bool handled = false;

    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < fd.size(); i++)
            if (fd[i]->drawable) {
                if (fd[i]->inside (x, y) && fd[i]->insideWindow (clx, cly, clw, clh)) {
                    fileDescr = fd[i];
                }

                bool b = fd[i]->pressNotify (button, type, state, x, y);
                handled = handled || b;
            }
    }

    if (handled || (fileDescr && fileDescr->processing)) {
        return;
    }

    {
        MYWRITERLOCK(l, entryRW);

        if (selected.size() == 1 && type == GDK_2BUTTON_PRESS && button == 1) {
            doubleClicked (selected[0]);
        } else if (button == 1 && type == GDK_BUTTON_PRESS) {
            if (fileDescr && (state & GDK_SHIFT_MASK))
                selectRange (fileDescr, state & GDK_CONTROL_MASK);
            else if (fileDescr && (state & GDK_CONTROL_MASK))
                selectSet (fileDescr);
            else
                selectSingle (fileDescr);

            lastClicked = fileDescr;
            MYWRITERLOCK_RELEASE(l);
            selectionChanged ();
        } else if (fileDescr && button == 3 && type == GDK_BUTTON_PRESS) {
            if (!fileDescr->selected) {
                selectSingle (fileDescr);

                lastClicked = fileDescr;
                MYWRITERLOCK_RELEASE(l);
                selectionChanged ();
            }

            MYWRITERLOCK_RELEASE(l);
            rightClicked (x, y);
        }
    } // end of MYWRITERLOCK(l, entryRW);

}

bool ThumbBrowserBase::Internal::on_draw(const ::Cairo::RefPtr< Cairo::Context> &cr)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)

    dirty = false;

    parent->onInternalAreaDraw();
    auto logical = hidpi::LogicalSize::forWidget(this);

    // draw thumbnails

    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    cr->set_line_join(Cairo::LINE_JOIN_MITER);
    style->render_background(cr, 0., 0., logical.width, logical.height);
    Glib::RefPtr<Pango::Context> context = get_pango_context ();
    context->set_font_description (style->get_font());

    {
        MYWRITERLOCK(l, parent->entryRW);

        for (size_t i = 0; i < parent->fd.size() && !dirty; i++) { // if dirty meanwhile, cancel and wait for next redraw
            if (!parent->fd[i]->drawable || !parent->fd[i]->insideWindow (0, 0, logical.width, logical.height)) {
                parent->fd[i]->updatepriority = false;
            } else {
                parent->fd[i]->updatepriority = true;
                parent->fd[i]->draw (cr);
            }
        }
    }
    style->render_frame(cr, 0., 0., logical.width, logical.height);

    return true;
}

Gtk::SizeRequestMode ThumbBrowserBase::Internal::get_request_mode_vfunc () const
{
    return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
}

void ThumbBrowserBase::Internal::get_preferred_height_vfunc (int &minimum_height, int &natural_height) const
{
    minimum_height = RTScalable::scalePixelSize(20);
    natural_height = RTScalable::scalePixelSize(80);
}

void ThumbBrowserBase::Internal::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    minimum_width = RTScalable::scalePixelSize(200);
    natural_width = RTScalable::scalePixelSize(1000);
}

void ThumbBrowserBase::Internal::get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const
{
    get_preferred_height_vfunc(minimum_height, natural_height);
}

void ThumbBrowserBase::Internal::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    get_preferred_width_vfunc (minimum_width, natural_width);
}


bool ThumbBrowserBase::Internal::on_button_release_event (GdkEventButton* event)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    int w = get_width();
    int h = get_height();

    MYREADERLOCK(l, parent->entryRW);

    for (size_t i = 0; i < parent->fd.size(); i++)
        if (parent->fd[i]->drawable && parent->fd[i]->insideWindow (0, 0, w, h)) {
            ThumbBrowserEntryBase* tbe = parent->fd[i];
            MYREADERLOCK_RELEASE(l);
            // This will require a Writer access...
            tbe->releaseNotify (event->button, event->type, event->state, (int)event->x, (int)event->y);
            MYREADERLOCK_ACQUIRE(l);
        }

    return true;
}

bool ThumbBrowserBase::Internal::on_motion_notify_event (GdkEventMotion* event)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    int w = get_width();
    int h = get_height();

    MYREADERLOCK(l, parent->entryRW);

    for (size_t i = 0; i < parent->fd.size(); i++)
        if (parent->fd[i]->drawable && parent->fd[i]->insideWindow (0, 0, w, h)) {
            parent->fd[i]->motionNotify ((int)event->x, (int)event->y);
        }

    return true;
}

bool ThumbBrowserBase::Internal::on_scroll_event (GdkEventScroll* event)
{
    // Gtk signals automatically acquire the GUI (i.e. this method is enclosed by gdk_thread_enter and gdk_thread_leave)
    parent->scroll (event->direction, event->delta_x, event->delta_y);
    return true;
}

void ThumbBrowserBase::resort ()
{
    {
        MYWRITERLOCK(l, entryRW);

        const auto& options = App::get().options();
        std::sort(
            fd.begin(),
            fd.end(),
            [&](const ThumbBrowserEntryBase* a, const ThumbBrowserEntryBase* b)
            {
                if (!options.sortDescending) {
                    return a->compare(*b, options.sortMethod);
                } else {
                    return b->compare(*a, options.sortMethod);
                }
            }
        );
    }

    redraw ();
}

void ThumbBrowserBase::redraw (ThumbBrowserEntryBase* entry)
{

    GThreadLock lock;
    arrangeFiles(entry);
    queue_draw();
}

void ThumbBrowserBase::zoomChanged (bool zoomIn)
{

    int newHeight = 0;
    int optThumbSize = getThumbnailHeight();

    const auto& options = App::get().options();
    if (zoomIn)
        for (size_t i = 0; i < options.thumbnailZoomRatios.size(); i++) {
            newHeight = (int)(options.thumbnailZoomRatios[i] * getMaxThumbnailHeight());

            if (newHeight > optThumbSize) {
                break;
            }
        }
    else
        for (size_t i = options.thumbnailZoomRatios.size() - 1; i > 0; i--) {
            newHeight = (int)(options.thumbnailZoomRatios[i] * getMaxThumbnailHeight());

            if (newHeight < optThumbSize) {
                break;
            }
        }

    previewHeight = newHeight;

    saveThumbnailHeight(newHeight);

    {
        MYWRITERLOCK(l, entryRW);

        for (size_t i = 0; i < fd.size(); i++) {
            fd[i]->resize (previewHeight);
        }
    }

    redraw ();
}

void ThumbBrowserBase::refreshThumbImages ()
{

    int previewHeight = getThumbnailHeight();
    {
        MYWRITERLOCK(l, entryRW);

        for (size_t i = 0; i < fd.size(); i++) {
            fd[i]->resize (previewHeight);
        }
    }

    redraw ();
}

void ThumbBrowserBase::refreshQuickThumbImages ()
{
    MYWRITERLOCK(l, entryRW);

    for (size_t i = 0; i < fd.size(); ++i) {
        fd[i]->refreshQuickThumbnailImage ();
    }
}

void ThumbBrowserBase::refreshEditedState (const std::set<Glib::ustring>& efiles)
{

    editedFiles = efiles;
    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < fd.size(); i++) {
            fd[i]->framed = editedFiles.find (fd[i]->filename) != editedFiles.end();
        }
    }

    queue_draw ();
}

void ThumbBrowserBase::setArrangement (Arrangement a)
{

    arrangement = a;
    redraw ();
}

void ThumbBrowserBase::enableTabMode(bool enable)
{
    location = enable ? THLOC_EDITOR : THLOC_FILEBROWSER;
    arrangement = enable ? ThumbBrowserBase::TB_Horizontal : ThumbBrowserBase::TB_Vertical;

    const auto& options = App::get().options();
    if ((!options.sameThumbSize && (options.thumbSizeTab != options.thumbSize)) || (options.showFileNames || options.filmStripShowFileNames)) {

        MYWRITERLOCK(l, entryRW);

        for (size_t i = 0; i < fd.size(); i++) {
            fd[i]->resize (getThumbnailHeight());
        }
    }

    redraw ();

    // Scroll to selected position if going into ribbon mode or back
    // Tab mode is horizontal, file browser is vertical
    {
        MYREADERLOCK(l, entryRW);

        if (!selected.empty()) {
            if (enable) {
                double h = selected[0]->getStartX();
                MYREADERLOCK_RELEASE(l);
                hscroll.set_value (min(h, hscroll.get_adjustment()->get_upper()));
            } else {
                double v = selected[0]->getStartY();
                MYREADERLOCK_RELEASE(l);
                vscroll.set_value (min(v, vscroll.get_adjustment()->get_upper()));
            }
        }
    }
}

void ThumbBrowserBase::insertEntry (ThumbBrowserEntryBase* entry)
{
    // find place in sort order
    {
        MYWRITERLOCK(l, entryRW);

        const auto& options = App::get().options();
        entry->onDeviceScaleChanged(lastDeviceScale);

        fd.insert(
            std::lower_bound(
                fd.begin(),
                fd.end(),
                entry,
                [&](const ThumbBrowserEntryBase* a, const ThumbBrowserEntryBase* b)
                {
                    if (!options.sortDescending) {
                        return a->compare(*b, options.sortMethod);
                    } else {
                        return b->compare(*a, options.sortMethod);
                    }
                }
            ),
            entry
        );

        entry->setOffset ((int)(hscroll.get_value()), (int)(vscroll.get_value()));
    }

    redraw ();
}

void ThumbBrowserBase::getScrollPosition (double& h, double& v)
{
    h = hscroll.get_value ();
    v = vscroll.get_value ();
}

void ThumbBrowserBase::setScrollPosition (double h, double v)
{
    hscroll.set_value (h > hscroll.get_adjustment()->get_upper() ? hscroll.get_adjustment()->get_upper() : h);
    vscroll.set_value (v > vscroll.get_adjustment()->get_upper() ? vscroll.get_adjustment()->get_upper() : v);
}

// needed for auto-height in single tab
int ThumbBrowserBase::getEffectiveHeight()
{
    int h = hscroll.get_height() + 2; // have 2 pixels rounding error for scroll bars to appear

    MYREADERLOCK(l, entryRW);

    // Filtered items do not change in size, so take a non-filtered
    for (size_t i = 0; i < fd.size(); i++)
        if (!fd[i]->filtered) {
            h += fd[i]->getEffectiveHeight();
            break;
        }

    return h;
}

void ThumbBrowserBase::redrawNeeded (ThumbBrowserEntryBase* entry)
{

    // HOMBRE:DELETE ME?
    GThreadLock tLock; // Acquire the GUI

    if (entry->insideWindow (0, 0, internal.get_width(), internal.get_height())) {
        if (!internal.isDirty ()) {
            internal.setDirty ();
            internal.queue_draw ();
        }
    }
}


