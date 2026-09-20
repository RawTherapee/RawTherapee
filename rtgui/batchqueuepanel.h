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

#include <gtkmm.h>

#include "batchqueue.h"
#include "guiutils.h"
#include "saveformatpanel.h"

class FileCatalog;
class MyFileChooserButton;
class RTWindow;
class Thumbnail;

class BatchQueuePanel : public Gtk::Box,
    public BatchQueueListener,
    public FormatChangeListener
{

    Gtk::Button* zoomInButton;
    Gtk::Button* zoomOutButton;
    Gtk::Switch* qStartStop;
    sigc::connection qStartStopConn;
    Gtk::CheckButton* qAutoStart;

    Gtk::Entry* outdirTemplate;
    Gtk::Label* destinationPreviewLabel;
    MyFileChooserButton* outdirFolder;
    Gtk::Button* outdirFolderButton;
    Gtk::RadioButton* useTemplate;
    Gtk::RadioButton* useFolder;
    SaveFormatPanel* saveFormatPanel;
    Gtk::Frame *fdir, *fformat;

    RTWindow* parent;
    BatchQueue* batchQueue;
    Gtk::TextView* templateHelpTextView;
    Gtk::ScrolledWindow* scrolledTemplateHelpWindow;
    Gtk::ToggleButton* templateHelpButton;
    Gtk::Box* bottomBox;
    Gtk::Box* topBox;
    Gtk::Paned* middleSplitPane;

    std::atomic<bool> queueShouldRun;

    IdleRegister idle_register;

public:
    explicit BatchQueuePanel (FileCatalog* aFileCatalog);
    ~BatchQueuePanel() override;

    void init (RTWindow* parent);

    void addBatchQueueJobs(const std::vector<BatchQueueEntry*>& entries , bool head = false);
    void saveOptions ();

    bool handleShortcutKey (GdkEventKey* event);

    // batchqueuelistener interface
    void queueSizeChanged(int qsize, bool queueRunning, bool queueError, const Glib::ustring& queueErrorMessage) override;
    bool canStartNext() override;
    void setDestinationPreviewText(const Glib::ustring& destinationPath) override;

private:
    void startBatchProc ();
    void stopBatchProc ();
    void startOrStopBatchProc();
    void setGuiFromBatchState(bool queueRunning, int qsize);
    void templateHelpButtonToggled();
    void populateTemplateHelpBuffer(Glib::RefPtr<Gtk::TextBuffer> buffer);

    void pathFolderChanged ();
    void pathFolderButtonPressed ();
    void formatChanged(const Glib::ustring& format) override;
    void updateTab (int qsize, int forceOrientation = 0); // forceOrientation=0: base on options / 1: horizontal / 2: vertical
};
