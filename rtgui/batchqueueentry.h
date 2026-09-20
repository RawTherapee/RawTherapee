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

#include <memory>

#include <gtkmm.h>

#include "bqentryupdater.h"
#include "guiutils.h"
#include "options.h"
#include "thumbbrowserentrybase.h"

#include "rtengine/noncopyable.h"

class Thumbnail;
class RTSurface;

namespace rtengine
{
class ProcessingJob;

namespace procparams
{

class ProcParams;

}

}

class BatchQueueEntry final : public ThumbBrowserEntryBase, public BQEntryUpdateListener, public rtengine::NonCopyable
{

    guint8* opreview;
    int origpw, origph;
    bool opreviewDone;
    static bool iconsLoaded;
    IdleRegister idle_register;

public:

    static std::shared_ptr<RTSurface> savedAsIcon;

    rtengine::ProcessingJob* job;
    const std::unique_ptr<rtengine::procparams::ProcParams> params;
    Glib::ustring savedParamsFile;
    double progress;
    Glib::ustring outFileName;
    int sequence;
    SaveFormat saveFormat;
    bool forceFormatOpts;
    bool fast_pipeline;
    bool overwriteFile;

    BatchQueueEntry (rtengine::ProcessingJob* job, const rtengine::procparams::ProcParams& pparams, Glib::ustring fname, int prevw, int prevh, Thumbnail* thm = nullptr, bool overwrite = false);
    ~BatchQueueEntry () override;

    void refreshThumbnailImage () override;
    void calcThumbnailSize () override;

    void drawProgressBar (Glib::RefPtr<Gdk::Window> win, const Gdk::RGBA& foregr, const Gdk::RGBA& backgr, int x, int w, int y, int h) override;

    void removeButtonSet ();

    std::vector<std::shared_ptr<RTSurface>> getIconsOnImageArea () override;
    void getIconSize (int& w, int& h) const override;
    std::tuple<Glib::ustring, bool> getToolTip (int x, int y) const override;

    // bqentryupdatelistener interface
    void updateImage (guint8* img, hidpi::LogicalSize size, int deviceScale,
                      int origw, int origh, guint8* newOPreview) override;
    void _updateImage (guint8* img, hidpi::LogicalSize size, int deviceScale); // inside gtk thread
};
