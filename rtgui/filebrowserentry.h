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

#include <atomic>
#include <memory>

#include <gtkmm.h>

#include "editenums.h"
#include "filethumbnailbuttonset.h"
#include "imageareatoollistener.h"
#include "thumbbrowserentrybase.h"
#include "thumbimageupdater.h"
#include "thumbnaillistener.h"

#include "rtengine/noncopyable.h"
#include "rtengine/rtengine.h"

class FileBrowserEntry;
class Thumbnail;
class RTSurface;

struct FileBrowserEntryIdleHelper {
    FileBrowserEntry* fbentry;
    bool destroyed;
    std::atomic<int> pending;
};

class FileThumbnailButtonSet;
class FileBrowserEntry final : public ThumbBrowserEntryBase,
    public ThumbnailListener,
    public ThumbImageUpdateListener,
    public rtengine::NonCopyable
{

    double scale;
    bool wasInside;
    ImageAreaToolListener* iatlistener;
    int press_x, press_y, action_x, action_y;
    double rot_deg;
    bool landscape;
    const std::unique_ptr<rtengine::procparams::CropParams> cropParams;
    const std::unique_ptr<rtengine::procparams::CropGuideParams> cropGuideParams;
    CropGUIListener* cropgl;
    FileBrowserEntryIdleHelper* feih;

    ImgEditState state;
    float crop_custom_ratio;

    IdleRegister idle_register;

    bool onArea (CursorArea a, int x, int y);
    void updateCursor (int x, int y);
    void drawStraightenGuide (Cairo::RefPtr<Cairo::Context> c);
    void customBackBufferUpdate (Cairo::RefPtr<Cairo::Context> c) override;
    void customDraw(const Cairo::RefPtr<Cairo::Context>& c) override;
    void refreshThumbnailImage(bool upgradeHint);

public:

    static std::shared_ptr<RTSurface> editedIcon;
    static std::shared_ptr<RTSurface> recentlySavedIcon;
    static std::shared_ptr<RTSurface> enqueuedIcon;
    static std::shared_ptr<RTSurface> hdr;
    static std::shared_ptr<RTSurface> ps;

    FileBrowserEntry (Thumbnail* thm, const Glib::ustring& fname);
    ~FileBrowserEntry () override;
    static void init ();
    void draw (Cairo::RefPtr<Cairo::Context> cc) override;

    void setImageAreaToolListener (ImageAreaToolListener* l)
    {
        iatlistener = l;
    }

    FileThumbnailButtonSet* getThumbButtonSet ();

    void refreshThumbnailImage () override;
    void refreshQuickThumbnailImage () override;
    void calcThumbnailSize () override;

    std::vector<std::shared_ptr<RTSurface>> getIconsOnImageArea () override;
    std::vector<std::shared_ptr<RTSurface>> getSpecificityIconsOnImageArea () override;
    void getIconSize (int& w, int& h) const override;

    // thumbnaillistener interface
    void procParamsChanged (Thumbnail* thm, int whoChangedIt, bool upgradeHint) override;
    // thumbimageupdatelistener interface
    void updateImage(const ThumbImageUpdateListener::ImageUpdate& update) override;
    void _updateImage(const ThumbImageUpdateListener::ImageUpdate& update); // inside gtk thread

    bool    motionNotify  (int x, int y) override;
    bool    pressNotify   (int button, int type, int bstate, int x, int y) override;
    bool    releaseNotify (int button, int type, int bstate, int x, int y) override;
};
