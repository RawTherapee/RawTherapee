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

#include <set>

#include <gtkmm.h>

#include "guiutils.h"
#include "options.h"

/*
 * Class handling the list of ThumbBrowserEntry objects and their position in it's allocated space
 */

class Inspector;
class ThumbBrowserEntryBase;

class ThumbBrowserBase :
    public Gtk::Grid
{

    class Internal :
        public Gtk::DrawingArea
    {
        //Cairo::RefPtr<Cairo::Context> cc;
        int ofsX, ofsY;
        ThumbBrowserBase* parent;
        bool dirty;

        // caching some very often used values
        Glib::RefPtr<Gtk::StyleContext> style;
        Gdk::RGBA textn;
        Gdk::RGBA texts;
        Gdk::RGBA bgn;
        Gdk::RGBA bgs;

    public:
        Internal ();
        void setParent (ThumbBrowserBase* p);
        void on_realize() override;
        void on_style_updated() override;
        bool on_configure_event(GdkEventConfigure *configure_event) override;
        bool on_draw(const ::Cairo::RefPtr< Cairo::Context> &cr) override;

        Gtk::SizeRequestMode get_request_mode_vfunc () const override;
        void get_preferred_height_vfunc (int &minimum_height, int &natural_height) const final;
        void get_preferred_width_vfunc (int &minimum_width, int &natural_width) const final;
        void get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const final;
        void get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const final;

        bool on_button_press_event (GdkEventButton* event) override;
        bool on_button_release_event (GdkEventButton* event) override;
        bool on_motion_notify_event (GdkEventMotion* event) override;
        bool on_scroll_event (GdkEventScroll* event) override;
        bool on_key_press_event (GdkEventKey* event) override;
        bool on_query_tooltip (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip);
        void setPosition (int x, int y);

        Glib::RefPtr<Gtk::StyleContext> getStyle() {
            return style;
        }
        Gdk::RGBA getNormalTextColor() {
            return textn;
        }
        Gdk::RGBA getSelectedTextColor() {
            return texts;
        }
        Gdk::RGBA getNormalBgColor() {
            return bgn;
        }
        Gdk::RGBA getSelectedBgColor() {
            return bgs;
        }

        void setDirty ()
        {
            dirty = true;
        }
        bool isDirty  ()
        {
            return dirty;
        }
    };

public:

    enum eLocation {
        THLOC_BATCHQUEUE,
        THLOC_FILEBROWSER,
        THLOC_EDITOR
    } location;

protected:
    virtual int getMaxThumbnailHeight() const
    {
        return App::get().options().maxThumbnailHeight;    // Differs between batch and file
    }
    virtual void saveThumbnailHeight (int height) = 0;
    virtual int  getThumbnailHeight () = 0;

    Internal internal;
    Gtk::Scrollbar hscroll;
    Gtk::Scrollbar vscroll;
    int lastDeviceScale;

    int inW, inH;

    Inspector *inspector;
    bool isInspectorActive;

    void resizeThumbnailArea (int w, int h);
    void internalAreaResized (Gtk::Allocation& req);
    void buttonPressed (int x, int y, int button, GdkEventType type, int state, int clx, int cly, int clw, int clh);

    void onInternalAreaDraw();

public:

    void setInspector(Inspector* inspector)
    {
        this->inspector = inspector;
    }
    Inspector* getInspector()
    {
        return inspector;
    }
    void disableInspector();
    void enableInspector();
    enum Arrangement {TB_Horizontal, TB_Vertical};
    void configScrollBars ();
    void scrollChanged ();
    void scroll (int direction, double deltaX=0.0, double deltaY=0.0);
    void scrollPage (int direction);

private:
    void selectSingle (ThumbBrowserEntryBase* clicked);
    void selectRange (ThumbBrowserEntryBase* clicked, bool additional);
    void selectSet (ThumbBrowserEntryBase* clicked);

public:
    void selectPrev (int distance, bool enlarge);
    void selectNext (int distance, bool enlarge);
    void selectFirst (bool enlarge);
    void selectLast (bool enlarge);

    virtual bool isInTabMode()
    {
        return false;
    }

    eLocation getLocation()
    {
        return location;
    }

protected:

    int eventTime;

    MyRWMutex entryRW;  // Locks access to following 'fd' AND 'selected'
    std::vector<ThumbBrowserEntryBase*> fd;
    std::vector<ThumbBrowserEntryBase*> selected;
    ThumbBrowserEntryBase* lastClicked;
    ThumbBrowserEntryBase* anchor;

    int previewHeight;
    int numOfCols;
    int lastRowHeight;

    Arrangement arrangement;

    std::set<Glib::ustring> editedFiles;

    void arrangeFiles (ThumbBrowserEntryBase* entry = nullptr);
    void zoomChanged (bool zoomIn);

public:

    ThumbBrowserBase ();

    void zoomIn ()
    {
        zoomChanged (true);
    }
    void zoomOut ()
    {
        zoomChanged (false);
    }
    int getEffectiveHeight ();

    const std::vector<ThumbBrowserEntryBase*>& getEntries ()
    {
        return fd;
    }
    void on_style_updated () override;
    void resort (); // re-apply sort method
    void redraw (ThumbBrowserEntryBase* entry = nullptr);   // arrange files and draw area
    void refreshThumbImages (); // refresh thumbnail sizes, re-generate thumbnail images, arrange and draw
    void refreshQuickThumbImages (); // refresh thumbnail sizes, re-generate thumbnail images, arrange and draw
    void refreshEditedState (const std::set<Glib::ustring>& efiles);

    void insertEntry (ThumbBrowserEntryBase* entry);

    void getScrollPosition (double& h, double& v);
    void setScrollPosition (double h, double v);

    void setArrangement (Arrangement a);
    void enableTabMode(bool enable);  // set both thumb sizes and arrangements

    virtual bool checkFilter (ThumbBrowserEntryBase* entry) const
    {
        return true;
    }
    virtual void rightClicked (int x, int y) = 0;
    virtual void doubleClicked (ThumbBrowserEntryBase* entry) {}
    virtual bool keyPressed (GdkEventKey* event)
    {
        return true;
    }
    virtual void selectionChanged () {}

    virtual void redrawNeeded (ThumbBrowserEntryBase* entry);
    virtual void thumbRearrangementNeeded () {}

    Gtk::Widget* getDrawingArea ()
    {
        return &internal;
    }

    Glib::RefPtr<Gtk::StyleContext> getStyle() {
        return internal.getStyle();
    }
    Gdk::RGBA getNormalTextColor() {
        return internal.getNormalTextColor();
    }
    Gdk::RGBA getSelectedTextColor() {
        return internal.getSelectedTextColor();
    }
    Gdk::RGBA getNormalBgColor() {
        return internal.getNormalBgColor();
    }
    Gdk::RGBA getSelectedBgColor() {
        return internal.getSelectedBgColor();
    }

};
