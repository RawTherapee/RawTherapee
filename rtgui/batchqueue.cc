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
#include <glibmm/ustring.h>
#include <glib/gstdio.h>
#include <cstring>
#include <functional>
#include "rtengine/imagedata.h"
#include "rtengine/rt_math.h"
#include "rtengine/procparams.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "cachemanager.h"
#include "thumbnail.h"
#include "batchqueue.h"
#include "batchqueueentry.h"
#include "multilangmgr.h"
#include "filecatalog.h"
#include "batchqueuebuttonset.h"
#include "guiutils.h"
#include "pathutils.h"
#include "rtimage.h"
#include <sys/time.h>

using namespace std;
using namespace rtengine;

#ifdef _WIN32
#define PATH_SEPARATOR '\\';
#else
#define PATH_SEPARATOR '/';
#endif

namespace   // local helper functions
{
    // Look for N or -N in templateText at position ix, meaning "index from end" and "index from start".
    // For N, return Nth index from the end, and for -N return the Nth index from the start.
    // N is a digit 1 through 9. The returned value is not range-checked, so it may be >=numPathElements.
    // or negative. The caller performs any required range-checking.
    int decodePathIndex(unsigned int& ix, const Glib::ustring& templateText, size_t numPathElements)
    {
        int pathIndex = static_cast<int>(numPathElements);    // a value that means input was invalid
        bool fromStart = false;
        if (ix < templateText.size()) {
            if (templateText[ix] == '-') {
                fromStart = true;   // minus sign means N is from the start rather than the end of the path
                ix++;
            }
        }
        if (ix < templateText.size()) {
            pathIndex = templateText[ix] - '1';
            if (!fromStart) {
                pathIndex = numPathElements - pathIndex - 1;
            }
        }
        return pathIndex;
    }

    // Extract the initial characters from a canonical absolute path, and append
    // those to a path string. Initial characters are '/' for Unix/Linux paths and
    // '\\' or '//' for UNC paths. A single backslash is also accepted, for driveless
    // Windows paths.
    void appendAbsolutePathPrefix(Glib::ustring& path, const Glib::ustring& absolutePath)
    {
        if (absolutePath[0] == '/') {
            if (absolutePath.size() > 1 && absolutePath[1] == '/') {
                path += "//"; // Start of a Samba UNC path
            } else {
                path += '/';    // Start of a Unix/Linux path
            }
        } else if (absolutePath[0] == '\\') {
            if (absolutePath.size() > 1 && absolutePath[1] == '\\') {
                path += "\\\\"; // Start of a UNC path
            } else {
                path += '\\';   // Start of a Windows path that does not include a drive letter
            }
        }
    }

    // Look in templateText at index ix for quoted string containing a time format string, and
    // use that string to format dateTime. Append the formatted time to path.
    void appendFormattedTime(Glib::ustring& path, unsigned int& ix, const Glib::ustring& templateText, const Glib::DateTime& dateTime)
    {
        constexpr gunichar quoteMark('"');
        if ((ix + 1) < templateText.size() && templateText[ix] == quoteMark) {
            const auto endPos = templateText.find_first_of(quoteMark, ++ix);
            if (endPos != Glib::ustring::npos) {
                Glib::ustring formatString(templateText, ix, endPos-ix);
                path += dateTime.format(formatString);
                ix = endPos;
            }
        }
    }
}

BatchQueue::BatchQueue (FileCatalog* aFileCatalog) : processing(nullptr), fileCatalog(aFileCatalog), sequence(0), listener(nullptr)
{

    location = THLOC_BATCHQUEUE;

    int p = 0;

    pmenu.attach (*Gtk::manage(open = new Gtk::MenuItem (M("FILEBROWSER_POPUPOPENINEDITOR"))), 0, 1, p, p + 1);
    p++;
    pmenu.attach (*Gtk::manage(selall = new Gtk::MenuItem (M("FILEBROWSER_POPUPSELECTALL"))), 0, 1, p, p + 1);
    p++;
    pmenu.attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;

    pmenu.attach (*Gtk::manage(head = new MyImageMenuItem (M("FILEBROWSER_POPUPMOVEHEAD"), "goto-start-small")), 0, 1, p, p + 1);
    p++;

    pmenu.attach (*Gtk::manage(tail = new MyImageMenuItem (M("FILEBROWSER_POPUPMOVEEND"), "goto-end-small")), 0, 1, p, p + 1);
    p++;

    pmenu.attach (*Gtk::manage(new Gtk::SeparatorMenuItem ()), 0, 1, p, p + 1);
    p++;

    pmenu.attach (*Gtk::manage(cancel = new MyImageMenuItem (M("FILEBROWSER_POPUPCANCELJOB"), "cancel-small")), 0, 1, p, p + 1);

    pmenu.show_all ();

    // Accelerators
    pmaccelgroup = Gtk::AccelGroup::create ();
    pmenu.set_accel_group (pmaccelgroup);
    open->add_accelerator ("activate", pmaccelgroup, GDK_KEY_e, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    selall->add_accelerator ("activate", pmaccelgroup, GDK_KEY_a, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    head->add_accelerator ("activate", pmaccelgroup, GDK_KEY_Home, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    tail->add_accelerator ("activate", pmaccelgroup, GDK_KEY_End, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    cancel->add_accelerator ("activate", pmaccelgroup, GDK_KEY_Delete, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);

    open->signal_activate().connect(sigc::mem_fun(*this, &BatchQueue::openLastSelectedItemInEditor));
    cancel->signal_activate().connect (std::bind (&BatchQueue::cancelItems, this, std::ref (selected)));
    head->signal_activate().connect (std::bind (&BatchQueue::headItems, this, std::ref (selected)));
    tail->signal_activate().connect (std::bind (&BatchQueue::tailItems, this, std::ref (selected)));
    selall->signal_activate().connect (sigc::mem_fun(*this, &BatchQueue::selectAll));

    setArrangement (ThumbBrowserBase::TB_Vertical);
}

BatchQueue::~BatchQueue ()
{
    std::set<BatchQueueEntry*> removable_bqes;

    mutex_removable_batch_queue_entries.lock();
    removable_batch_queue_entries.swap(removable_bqes);
    mutex_removable_batch_queue_entries.unlock();

    for (const auto entry : removable_bqes) {
        ::g_remove(entry->savedParamsFile.c_str());
        delete entry;
    }

    idle_register.destroy();

    MYWRITERLOCK(l, entryRW);

    // The listener merges parameters with old values, so delete afterwards
    for (size_t i = 0; i < fd.size(); i++) {
        delete fd.at(i);
    }

    fd.clear ();
}

void BatchQueue::resizeLoadedQueue()
{
    MYWRITERLOCK(l, entryRW);

    const auto height = getThumbnailHeight ();

    for (const auto entry : fd)
        entry->resize(height);
}

// Reduce the max size of a thumb, since thumb is processed synchronously on adding to queue
// leading to very long waiting when adding more images
int BatchQueue::calcMaxThumbnailHeight()
{
    return std::min(App::get().options().maxThumbnailHeight, 200);
}

// Function for virtual override in thumbbrowser base
int BatchQueue::getMaxThumbnailHeight() const
{
    return calcMaxThumbnailHeight();
}

void BatchQueue::saveThumbnailHeight (int height)
{
    App::get().mut_options().thumbSizeQueue = height;
}

int BatchQueue::getThumbnailHeight ()
{
    // The user could have manually forced the option to a too big value
    return std::max(std::min(App::get().options().thumbSizeQueue, 200), 10);
}

void BatchQueue::rightClicked (int x, int y)
{
    const Gdk::Rectangle rect(x, y, 1, 1);
    pmenu.popup_at_rect(internal.get_window(), rect, Gdk::GRAVITY_NORTH_WEST, Gdk::GRAVITY_NORTH_WEST, nullptr);
}

void BatchQueue::doubleClicked(ThumbBrowserEntryBase* entry)
{
    openItemInEditor(entry);
}

bool BatchQueue::keyPressed (GdkEventKey* event)
{
    bool ctrl  = event->state & GDK_CONTROL_MASK;

    if ((event->keyval == GDK_KEY_A || event->keyval == GDK_KEY_a) && ctrl) {
        selectAll ();
        return true;
    } else if ((event->keyval == GDK_KEY_E || event->keyval == GDK_KEY_e) && ctrl) {
        openLastSelectedItemInEditor();
        return true;
    } else if (event->keyval == GDK_KEY_Home) {
        headItems (selected);
        return true;
    } else if (event->keyval == GDK_KEY_End) {
        tailItems (selected);
        return true;
    } else if (event->keyval == GDK_KEY_Delete) {
        cancelItems (selected);
        return true;
    }

    return false;
}

void BatchQueue::addEntries (const std::vector<BatchQueueEntry*>& entries, bool head, bool save)
{
    {
        MYWRITERLOCK(l, entryRW);

        for (const auto entry : entries) {

            entry->setParent (this);

            // BatchQueueButtonSet have to be added before resizing to take them into account
            const auto bqbs = new BatchQueueButtonSet (entry);
            bqbs->setButtonListener (this);
            entry->addButtonSet (bqbs);

            // batch queue might have smaller, restricted size
            entry->resize (getThumbnailHeight());

            // recovery save
            const auto tempFile = getTempFilenameForParams (entry->filename);

            if (!entry->params->save (tempFile))
                entry->savedParamsFile = tempFile;

            entry->selected = false;

            // insert either at the end, or before the first non-processing entry
            auto pos = fd.end ();

            if (head)
                pos = std::find_if (fd.begin (), fd.end (), [] (const ThumbBrowserEntryBase* fdEntry) { return !fdEntry->processing; });

            fd.insert (pos, entry);

            if (entry->thumbnail)
                entry->thumbnail->imageEnqueued ();
        }
    }

    if (save)
        saveBatchQueue ();

    redraw ();
    notifyListener ();
}

bool BatchQueue::saveBatchQueue ()
{
    const auto fileName = Glib::build_filename (App::get().options().rtdir, "batch", "queue.csv");

    std::ofstream file (fileName, std::ios::binary | std::ios::trunc);

    if (!file.is_open ())
        return false;

    {
        MYREADERLOCK(l, entryRW);

        if (fd.empty ())
            return true;

        // The column's header is mandatory (the first line will be skipped when loaded)
        file << "input image full path|param file full path|output image full path|file format|jpeg quality|jpeg subsampling|"
             << "png bit depth|png compression|tiff bit depth|tiff is float|uncompressed tiff|save output params|force format options|fast export|<end of line>"
             << std::endl;

        // method is already running with entryLock, so no need to lock again
        for (const auto fdEntry : fd) {

            const auto entry = static_cast<BatchQueueEntry*> (fdEntry);
            const auto& saveFormat = entry->saveFormat;

            // Warning: for code's simplicity in loadBatchQueue, each field must end by the '|' character, safer than ';' or ',' since it can't be used in paths
#ifdef _WIN32
            // on windows it crashes if we don't use c_str() and filename etc. contain special (e.g. chinese) characters, see issue 3387
            file << entry->filename.c_str() << '|' << entry->savedParamsFile.c_str() << '|' << entry->outFileName.c_str() << '|' << saveFormat.format << '|'
#else
            file << entry->filename << '|' << entry->savedParamsFile << '|' << entry->outFileName << '|' << saveFormat.format << '|'
#endif
                 << saveFormat.jpegQuality << '|' << saveFormat.jpegSubSamp << '|'
                 << saveFormat.pngBits << '|'
                 << saveFormat.tiffBits << '|'  << (saveFormat.tiffFloat ? 1 : 0) << '|'  << saveFormat.tiffUncompressed << '|'
                 << saveFormat.saveParams << '|' << entry->forceFormatOpts << '|'
                 << entry->fast_pipeline << '|'
                 << saveFormat.bigTiff << '|'
                 << std::endl;
        }
    }

    return true;
}

bool BatchQueue::loadBatchQueue ()
{
    const auto& options = App::get().options();
    const auto fileName = Glib::build_filename (options.rtdir, "batch", "queue.csv");

    std::ifstream file (fileName, std::ios::binary);

    if (file.is_open ()) {
        // Yes, it's better to get the lock for the whole file reading,
        // to update the list in one shot without any other concurrent access!
        MYWRITERLOCK(l, entryRW);

        std::string row, column;
        std::vector<std::string> values;

        // skipping the first row
        std::getline (file, row);

        while (std::getline (file, row)) {

            std::istringstream line (row);

            values.clear ();

            while (std::getline(line, column, '|')) {
                values.push_back (column);
            }

            auto value = values.begin ();

            const auto nextStringOr = [&] (const Glib::ustring& defaultValue) -> Glib::ustring
            {
                return value != values.end () ? Glib::ustring(*value++) : defaultValue;
            };
            const auto nextIntOr = [&] (int defaultValue) -> int
            {
                try {
                    return value != values.end () ? std::stoi(*value++) : defaultValue;
                }
                catch (std::exception&) {
                    return defaultValue;
                }
            };

            const auto source = nextStringOr (Glib::ustring ());
            const auto paramsFile = nextStringOr (Glib::ustring ());

            if (source.empty () || paramsFile.empty ())
                continue;

            const auto outputFile = nextStringOr (Glib::ustring ());
            const auto saveFmt = nextStringOr (options.saveFormat.format);
            const auto jpegQuality = nextIntOr (options.saveFormat.jpegQuality);
            const auto jpegSubSamp = nextIntOr (options.saveFormat.jpegSubSamp);
            const auto pngBits = nextIntOr (options.saveFormat.pngBits);
            const auto tiffBits = nextIntOr (options.saveFormat.tiffBits);
            const auto tiffFloat = nextIntOr (options.saveFormat.tiffFloat);
            const auto tiffUncompressed = nextIntOr (options.saveFormat.tiffUncompressed);
            const auto saveParams = nextIntOr (options.saveFormat.saveParams);
            const auto forceFormatOpts = nextIntOr (options.forceFormatOpts);
            const auto fast = nextIntOr(false);
            const auto bigTiff = nextIntOr (options.saveFormat.bigTiff);

            rtengine::procparams::ProcParams pparams;

            if (pparams.load (paramsFile))
                continue;

            auto thumb = CacheManager::getInstance ()->getEntry (source);

            if (!thumb)
                continue;

            auto job = rtengine::ProcessingJob::create (source, thumb->getType () == FT_Raw, pparams, fast);

            auto prevh = getMaxThumbnailHeight();
            auto prevw = prevh;
            thumb->getThumbnailSize(prevw, prevh, &pparams);

            auto entry = new BatchQueueEntry (job, pparams, source, prevw, prevh, thumb, options.overwriteOutputFile);
            thumb->decreaseRef ();  // Removing the refCount acquired by cacheMgr->getEntry
            entry->setParent (this);

            // BatchQueueButtonSet have to be added before resizing to take them into account
            auto bqbs = new BatchQueueButtonSet (entry);
            bqbs->setButtonListener (this);
            entry->addButtonSet (bqbs);

            entry->savedParamsFile = paramsFile;
            entry->selected = false;
            entry->outFileName = outputFile;

            if (!outputFile.empty ()) {
                auto& saveFormat = entry->saveFormat;
                saveFormat.format = saveFmt;
                saveFormat.jpegQuality = jpegQuality;
                saveFormat.jpegSubSamp = jpegSubSamp;
                saveFormat.pngBits = pngBits;
                saveFormat.tiffBits = tiffBits;
                saveFormat.tiffFloat = tiffFloat == 1;
                saveFormat.tiffUncompressed = tiffUncompressed != 0;
                saveFormat.bigTiff = bigTiff != 0;
                saveFormat.saveParams = saveParams != 0;
                entry->forceFormatOpts = forceFormatOpts != 0;
            } else {
                entry->forceFormatOpts = false;
            }

            fd.push_back (entry);
        }
    }

    redraw ();
    notifyListener ();

    return !fd.empty ();
}

Glib::ustring BatchQueue::getTempFilenameForParams( const Glib::ustring &filename )
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    char mseconds[11];
    snprintf(mseconds, sizeof(mseconds), "%d", static_cast<int>((tv.tv_usec / 1000)));
    time_t rawtime;
    struct tm *timeinfo;
    char stringTimestamp [80];
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    strftime (stringTimestamp, sizeof(stringTimestamp), "_%Y%m%d%H%M%S_", timeinfo);
    Glib::ustring savedParamPath;
    savedParamPath = App::get().options().rtdir + "/batch/";
    g_mkdir_with_parents (savedParamPath.c_str (), 0755);
    savedParamPath += Glib::path_get_basename (filename);
    savedParamPath += stringTimestamp;
    savedParamPath += mseconds;
    savedParamPath += App::PARAM_FILE_EXTENSION;
    return savedParamPath;
}

void BatchQueue::cancelItems (const std::vector<ThumbBrowserEntryBase*>& items)
{
    std::set<BatchQueueEntry*> removable_bqes;

    {
        MYWRITERLOCK(l, entryRW);

        for (const auto item : items) {

            const auto entry = static_cast<BatchQueueEntry*> (item);

            if (entry->processing)
                continue;

            const auto pos = std::find (fd.begin (), fd.end (), entry);

            if (pos == fd.end ())
                continue;

            fd.erase (pos);

            rtengine::ProcessingJob::destroy (entry->job);

            if (entry->thumbnail)
                entry->thumbnail->imageRemovedFromQueue ();

            removable_bqes.insert(entry);
        }

        for (const auto entry : fd)
            entry->selected = false;

        lastClicked = nullptr;
        selected.clear ();
    }

    if (!removable_bqes.empty()) {
        mutex_removable_batch_queue_entries.lock();
        removable_batch_queue_entries.insert(removable_bqes.begin(), removable_bqes.end());
        mutex_removable_batch_queue_entries.unlock();

        idle_register.add(
            [this]() -> bool
            {
                std::set<BatchQueueEntry*> removable_bqes;

                mutex_removable_batch_queue_entries.lock();
                removable_batch_queue_entries.swap(removable_bqes);
                mutex_removable_batch_queue_entries.unlock();

                for (const auto entry : removable_bqes) {
                    ::g_remove(entry->savedParamsFile.c_str());
                    delete entry;
                }

                return false;
            }
        );
    }

    saveBatchQueue ();

    redraw ();
    notifyListener ();
}

void BatchQueue::headItems (const std::vector<ThumbBrowserEntryBase*>& items)
{
    {
        MYWRITERLOCK(l, entryRW);

        for (auto item = items.rbegin(); item != items.rend(); ++item) {

            const auto entry = static_cast<BatchQueueEntry*> (*item);

            if (entry->processing)
                continue;

            const auto pos = std::find (fd.begin (), fd.end (), entry);

            if (pos == fd.end () || pos == fd.begin ())
                continue;

            fd.erase (pos);

            // find the first item that is not under processing
            const auto newPos = std::find_if (fd.begin (), fd.end (), [] (const ThumbBrowserEntryBase* fdEntry) { return !fdEntry->processing; });

            fd.insert (newPos, entry);
        }
    }

    saveBatchQueue ();

    redraw ();
}

void BatchQueue::tailItems (const std::vector<ThumbBrowserEntryBase*>& items)
{
    {
        MYWRITERLOCK(l, entryRW);

        for (const auto item : items) {

            const auto entry = static_cast<BatchQueueEntry*> (item);

            if (entry->processing)
                continue;

            const auto pos = std::find (fd.begin (), fd.end (), entry);

            if (pos == fd.end ())
                continue;

            fd.erase (pos);

            fd.push_back (entry);
        }
    }

    saveBatchQueue ();

    redraw ();
}

void BatchQueue::selectAll ()
{
    {
        MYWRITERLOCK(l, entryRW);

        lastClicked = nullptr;
        selected.clear ();

        for (size_t i = 0; i < fd.size(); i++) {
            if (fd[i]->processing) {
                continue;
            }

            fd[i]->selected = true;
            selected.push_back (fd[i]);
        }
    }
    queue_draw ();
}

void BatchQueue::openLastSelectedItemInEditor()
{
    {
        MYREADERLOCK(l, entryRW);

        if (!selected.empty()) {
            openItemInEditor(selected.back());
        }
    }
}

void BatchQueue::updateDestinationPathPreview()
{
    MYWRITERLOCK(l, entryRW);

    if (!selected.empty()) {
        auto& entry = *selected.at(0);
        int sequence = 0;   // Sequence during subsequent queue processing can't be determined here
        const auto& options = App::get().options();
        Glib::ustring baseDestination = calcAutoFileNameBase(entry.filename, sequence, options.saveFormatBatch.format);
        Glib::ustring destination = Glib::ustring::compose ("%1.%2", baseDestination, options.saveFormatBatch.format);

        if (listener) {
            listener->setDestinationPreviewText(destination);
        }
    }
}

void BatchQueue::openItemInEditor(ThumbBrowserEntryBase* item)
{
    if (item) {
        std::vector< ::Thumbnail*> requestedItem;
        requestedItem.push_back(item->thumbnail);
        fileCatalog->openRequested(requestedItem);
    }
}


void BatchQueue::startProcessing ()
{

    if (!processing) {
        MYWRITERLOCK(l, entryRW);

        if (!fd.empty()) {
            BatchQueueEntry* next;

            next = static_cast<BatchQueueEntry*>(fd[0]);
            // tag it as processing and set sequence
            next->processing = true;
            next->sequence = sequence = 1;
            processing = next;

            // remove from selection
            if (processing->selected) {
                std::vector<ThumbBrowserEntryBase*>::iterator pos = std::find (selected.begin(), selected.end(), processing);

                if (pos != selected.end()) {
                    selected.erase (pos);
                }

                processing->selected = false;
            }

            MYWRITERLOCK_RELEASE(l);

            // remove button set
            next->removeButtonSet ();

            // start batch processing
            rtengine::startBatchProcessing (next->job, this);
            queue_draw ();

            notifyListener();
        }
    }
}

void BatchQueue::setProgress(double p)
{
    if (processing) {
        processing->progress = p;
    }

    // No need to acquire the GUI, setProgressUI will do it
    idle_register.add(
        [this]() -> bool
        {
            redraw();
            return false;
        }
    );
}

void BatchQueue::setProgressStr(const Glib::ustring& str)
{
}

void BatchQueue::setProgressState(bool inProcessing)
{
}

void BatchQueue::error(const Glib::ustring& descr)
{
    if (processing && processing->processing) {
        // restore failed thumb
        BatchQueueButtonSet* bqbs = new BatchQueueButtonSet (processing);
        bqbs->setButtonListener (this);
        processing->addButtonSet (bqbs);
        processing->processing = false;
        processing->job = rtengine::ProcessingJob::create(processing->filename, processing->thumbnail->getType() == FT_Raw, *processing->params);
        processing = nullptr;
        redraw ();
    }

    if (listener) {
        BatchQueueListener* const bql = listener;

        idle_register.add(
            [bql, descr]() -> bool
            {
                bql->queueSizeChanged(0, false, true, descr);
                return false;
            }
        );
    }
}

rtengine::ProcessingJob* BatchQueue::imageReady(rtengine::IImagefloat* img)
{
    // save image img
    Glib::ustring fname;
    SaveFormat saveFormat;

    const auto& options = App::get().options();
    if (processing->outFileName.empty()) { // auto file name
        saveFormat = options.saveFormatBatch;
        Glib::ustring s = calcAutoFileNameBase (processing->filename, processing->sequence, saveFormat.format);
        fname = autoCompleteFileName (s, saveFormat.format);
    } else { // use the save-as filename with automatic completion for uniqueness
        if (processing->forceFormatOpts) {
            saveFormat = processing->saveFormat;
        } else {
            saveFormat = options.saveFormatBatch;
        }

        // The output filename's extension is forced to the current or selected output format,
        // despite what the user have set in the filename's field of the "Save as" dialog box
        fname = autoCompleteFileName (removeExtension(processing->outFileName), saveFormat.format);
        //fname = autoCompleteFileName (removeExtension(processing->outFileName), getExtension(processing->outFileName));
    }

    //printf ("fname=%s, %s\n", fname.c_str(), removeExtension(fname).c_str());

    if (img && !fname.empty()) {
        int err = 0;

        if (saveFormat.format == "tif") {
            err = img->saveAsTIFF (
                fname,
                saveFormat.tiffBits,
                saveFormat.tiffFloat,
                saveFormat.tiffUncompressed,
                saveFormat.bigTiff
            );
        } else if (saveFormat.format == "png") {
            err = img->saveAsPNG (fname, saveFormat.pngBits);
        } else if (saveFormat.format == "jpg") {
            err = img->saveAsJPEG (fname, saveFormat.jpegQuality, saveFormat.jpegSubSamp);
        }

        delete img;

        if (err) {
            throw Glib::FileError(Glib::FileError::FAILED, M("MAIN_MSG_CANNOTSAVE") + "\n" + fname);
        }

        if (saveFormat.saveParams) {
            // We keep the extension to avoid overwriting the profile when we have
            // the same output filename with different extension
            //processing->params.save (removeExtension(fname) + App::PARAM_FILE_EXTENSION);
            processing->params->save (fname + ".out" + App::PARAM_FILE_EXTENSION);
        }

        if (processing->thumbnail) {
            processing->thumbnail->imageDeveloped ();
            processing->thumbnail->imageRemovedFromQueue ();
        }
    }

    // save temporary params file name: delete as last thing
    Glib::ustring processedParams = processing->savedParamsFile;

    // delete from the queue
    bool remove_button_set = false;

    {
        MYWRITERLOCK(l, entryRW);

        delete processing;
        processing = nullptr;

        fd.erase (fd.begin());

        // return next job
        if (!fd.empty() && listener && listener->canStartNext ()) {
            BatchQueueEntry* next = static_cast<BatchQueueEntry*>(fd[0]);
            // tag it as selected and set sequence
            next->processing = true;
            next->sequence = ++sequence;
            processing = next;

            // remove from selection
            if (processing->selected) {
                std::vector<ThumbBrowserEntryBase*>::iterator pos = std::find (selected.begin(), selected.end(), processing);

                if (pos != selected.end()) {
                    selected.erase (pos);
                }

                processing->selected = false;
            }

            // remove button set
            remove_button_set = true;
        }
    }

    if (remove_button_set) {
        // ButtonSet have Cairo::Surface which might be rendered while we're trying to delete them
        GThreadLock lock;
        processing->removeButtonSet ();
    }

    if (saveBatchQueue ()) {
        ::g_remove (processedParams.c_str ());

        // Delete all files in directory batch when finished, just to be sure to remove zombies
        auto isEmpty = false;

        {
            MYREADERLOCK(l, entryRW);
            isEmpty = fd.empty();
        }

        if (isEmpty) {

            const auto batchdir = Glib::build_filename (options.rtdir, "batch");

            try {

                auto dir = Gio::File::create_for_path (batchdir);
                auto enumerator = dir->enumerate_children ("standard::name");

                while (auto file = enumerator->next_file ()) {
                    ::g_remove (Glib::build_filename (batchdir, file->get_name ()).c_str ());
                }

            } catch (Glib::Exception&) {}
        }
    }

    redraw ();
    notifyListener ();

    return processing ? processing->job : nullptr;
}

// Calculates automatic filename of processed batch entry, but just the base name
// example output: "c:\out\converted\dsc0121"
// format is the output file format, e.g. "jpg", "tif", "png", etc.  It is used to put format in file path using "%e"
Glib::ustring BatchQueue::calcAutoFileNameBase (const Glib::ustring& origFileName, int sequence, const Glib::ustring& format)
{

    std::vector<Glib::ustring> da;

    for (size_t i = 0; i < origFileName.size(); i++) {
        while ((i < origFileName.size()) && (origFileName[i] == '\\' || origFileName[i] == '/')) {
            i++;
        }

        if (i >= origFileName.size()) {
            break;
        }

        Glib::ustring tok;

        while ((i < origFileName.size()) && !(origFileName[i] == '\\' || origFileName[i] == '/')) {
            tok = tok + origFileName[i++];
        }

        if (i < origFileName.size()) {  // omit the last token, which is the file name
            da.push_back (tok);
        }
    }

//    for (unsigned i=0; i<da.size(); i++)
//        printf ("da[%u]: \"%s\"\n", i, da[i].c_str());

    // extracting filebase
    Glib::ustring filename;

    int extpos = origFileName.size() - 1;

    for (; extpos >= 0 && origFileName[extpos] != '.'; extpos--);

    for (int k = extpos - 1; k >= 0 && origFileName[k] != '/' && origFileName[k] != '\\'; k--) {
        filename = origFileName[k] + filename;
    }

    const auto& options = App::get().options();

//    printf ("%d, |%s|\n", extpos, filename.c_str());

    // constructing full output path
//    printf ("path=|%s|\n", options.savePath.c_str());

    Glib::ustring path;

    if (options.saveUsePathTemplate) {
        unsigned int ix = 0;

        while (ix < options.savePathTemplate.size()) {
            if (options.savePathTemplate[ix] == '%') {
                ix++;

                if (options.savePathTemplate[ix] == 'P') {
                    // insert path elements from given index to the end
                    ix++;
                    int n = decodePathIndex(ix, options.savePathTemplate, da.size());
                    if (n < 0) {
                        n = 0;  // if too many elements specified, return all available elements
                    }
                    if (n < static_cast<int>(da.size())) {
                        if (n == 0) {
                            appendAbsolutePathPrefix(path, origFileName);
                        }
                        for (unsigned int i = static_cast<unsigned int>(n); i < da.size(); i++) {
                            path += da[i] + PATH_SEPARATOR;
                        }
                    }
                    // If the next template character is a separator, skip it, because path already has one
                    ix++;
                    if (ix < options.savePathTemplate.size() && options.savePathTemplate[ix] != '/' && options.savePathTemplate[ix] != '\\') {
                        ix--;
                    }
                } else if (options.savePathTemplate[ix] == 'p') {
                    // insert path elements from the start of the path up to the given index
                    ix++;
                    int n = decodePathIndex(ix, options.savePathTemplate, da.size());
                    if (n >= 0) {
                        appendAbsolutePathPrefix(path, origFileName);
                    }
                    for (unsigned int i=0; static_cast<int>(i) <= n && i < da.size(); i++) {
                        path += da[i] + PATH_SEPARATOR;
                    }
                    // If the next template character is a separator, skip it, because path already has one
                    ix++;
                    if (ix < options.savePathTemplate.size() && options.savePathTemplate[ix] != '/' && options.savePathTemplate[ix] != '\\') {
                        ix--;
                    }
                } else if (options.savePathTemplate[ix] == 'd') {
                    // insert a single directory name from the file's path
                    ix++;
                    int n = decodePathIndex(ix, options.savePathTemplate, da.size());
                    if (n >= 0 && n < static_cast<int>(da.size())) {
                        path += da[n];
                    }
                } else if (options.savePathTemplate[ix] == 'f') {
                    path += filename;
                } else if (options.savePathTemplate[ix] == 'e') { // file format from input args
                    path += format;
                } else if (options.savePathTemplate[ix] == 'r') { // rank from pparams
                    char rank;
                    rtengine::procparams::ProcParams pparams;

                    if( pparams.load(origFileName + App::PARAM_FILE_EXTENSION) == 0 ) {
                        if (!pparams.inTrash) {
                            rank = pparams.rank + '0';
                        } else {
                            rank = 'x';
                        }
                    } else {
                        rank = '0';    // if param file not loaded (e.g. does not exist), default to rank=0
                    }

                    path += rank;
                } else if (options.savePathTemplate[ix] == 's') { // sequence
                    std::ostringstream seqstr;

                    int w = options.savePathTemplate[ix + 1] - '0';

                    if (w >= 1 && w <= 9) {
                        ix++;
                        seqstr << std::setw (w) << std::setfill ('0');
                    }

                    seqstr << sequence;
                    path += seqstr.str ();
                } else if (options.savePathTemplate[ix] == 't') {
                    // Insert formatted date/time value. Character after 't' defines time source
                    if (++ix < options.savePathTemplate.size()) {
                        Glib::DateTime dateTime;
                        switch(options.savePathTemplate[ix++])
                        {
                            case 'E':   // (approximate) time when export started
                            {
                                dateTime = Glib::DateTime::create_now_local();
                                break;
                            }
                            case 'F': // time when file was last saved
                            {
                                Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(origFileName);
                                if (file) {
                                    Glib::RefPtr<Gio::FileInfo> info = file->query_info(G_FILE_ATTRIBUTE_TIME_MODIFIED);
                                    if (info) {
                                        dateTime = info->get_modification_date_time();
                                    }
                                }
                                break;
                            }
                            case 'P':   // time when picture was taken
                            {
                                const auto timestamp = FramesData(origFileName).getDateTimeAsTS();
                                dateTime = Glib::DateTime::create_now_local(timestamp);
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                        if (dateTime) {
                            appendFormattedTime(path, ix, options.savePathTemplate, dateTime);
                        }
                    }
                }
            }

            else {
                path += options.savePathTemplate[ix];
            }

            ix++;
        }
    } else {
        path = Glib::build_filename (options.savePathFolder, filename);
    }

    return path;
}

Glib::ustring BatchQueue::autoCompleteFileName (const Glib::ustring& fileName, const Glib::ustring& format)
{

    // separate filename and the path to the destination directory
    Glib::ustring dstdir = Glib::path_get_dirname (fileName);
    Glib::ustring dstfname = Glib::path_get_basename (fileName);
    Glib::ustring fname;

    // create directory, if does not exist
    if (g_mkdir_with_parents (dstdir.c_str (), 0755)) {
        return Glib::ustring ();
    }

    // In overwrite mode we TRY to delete the old file first.
    // if that's not possible (e.g. locked by viewer, R/O), we revert to the standard naming scheme
    bool inOverwriteMode = processing->overwriteFile;

    for (int tries = 0; tries < 100; tries++) {
        if (tries == 0) {
            fname = Glib::ustring::compose ("%1.%2", Glib::build_filename (dstdir,  dstfname), format);
        } else {
            fname = Glib::ustring::compose ("%1-%2.%3", Glib::build_filename (dstdir,  dstfname), tries, format);
        }

        int fileExists = Glib::file_test (fname, Glib::FILE_TEST_EXISTS);

        if (inOverwriteMode && fileExists) {
            if (::g_remove (fname.c_str ()) != 0) {
                inOverwriteMode = false;    // failed to delete- revert to old naming scheme
            } else {
                fileExists = false;    // deleted now
            }
        }

        if (!fileExists) {
            return fname;
        }
    }

    return "";
}

void BatchQueue::buttonPressed (LWButton* button, int actionCode, void* actionData, int x, int y)
{
    const std::vector<ThumbBrowserEntryBase*> bqe = {static_cast<BatchQueueEntry*>(actionData)};

    if (actionCode == 10) { // cancel
        cancelItems (bqe);
    } else if (actionCode == 8) { // to head
        headItems (bqe);
    } else if (actionCode == 9) { // to tail
        tailItems (bqe);
    }
}

void BatchQueue::notifyListener ()
{
    const bool queueRunning = processing;
    if (listener) {
        BatchQueueListener* const bql = listener;

        int qsize = 0;
        {
            MYREADERLOCK(l, entryRW);
            qsize = fd.size();
        }

        idle_register.add(
            [bql, qsize, queueRunning]() -> bool
            {
                bql->queueSizeChanged(qsize, queueRunning, false, {});
                return false;
            }
        );
    }
}

void BatchQueue::redrawNeeded (LWButton* button)
{
    GThreadLock lock;
    queue_draw ();
}

void BatchQueue::selectionChanged()
{
    updateDestinationPathPreview();
}
