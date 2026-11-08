// HEIF workflow integration checks against the production GUI object files.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "rtgui/saveformatpanel.h"
#include "rtgui/windows/saveasdlg.h"
#include "rtgui/batchqueueformat.h"
#include "rtgui/batchqueue.h"
#include "rtgui/batchqueueentry.h"
#include "rtgui/cachemanager.h"
#include "rtgui/thumbnail.h"
#include "rtengine/image16.h"
#include "rtengine/imageio.h"
#include "rtengine/previewimage.h"
#include "rtengine/utils.h"
#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
std::recursive_mutex gtkMutex;
void gtkEnter() { gtkMutex.lock(); }
void gtkLeave() { gtkMutex.unlock(); }
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::vector<std::string> columns(const std::string& text)
{
    std::istringstream in(text);
    std::vector<std::string> result;
    std::string column;
    while (std::getline(in, column, '|')) result.push_back(column);
    return result;
}
struct Panel : SaveFormatPanel {
    int count() { return format->get_model()->children().size(); }
    void choose(int row) { format->set_active(row); }
    bool unavailableVisible() { return heifUnavailable->get_visible(); }
    bool heifVisible() { return heifOpts->get_visible(); }
    bool jpegVisible() { return jpegOpts->get_visible(); }
};
struct QueueListener : BatchQueueListener {
    bool done = false, failed = false;
    Glib::ustring error;
    void queueSizeChanged(int size, bool running, bool failure, const Glib::ustring& message) override
    {
        if (failure || (!size && !running)) { done = true; failed = failure; error = message; }
    }
    bool canStartNext() override { return true; }
    void setDestinationPreviewText(const Glib::ustring&) override {}
};
void pump() { while (Gtk::Main::events_pending()) Gtk::Main::iteration(false); }
void processQueue(BatchQueue& queue, QueueListener& listener, bool available)
{
    queue.setBatchQueueListener(&listener);
    queue.startProcessing();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!listener.done && std::chrono::steady_clock::now() < deadline) {
        pump();
        gdk_threads_leave();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        gdk_threads_enter();
    }
    require(listener.done, "batch queue timeout");
    require(listener.failed != available, "batch queue availability result");
    if (!available) require(listener.error.find("HEIF") != Glib::ustring::npos, "batch availability error");
    gdk_threads_leave();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    gdk_threads_enter();
    pump();
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 3, "usage: gui-workflows <source rtdata directory> <artifacts>");
        const std::string data = argv[1], root = argv[2];
        gdk_threads_set_lock_functions(G_CALLBACK(gtkEnter), G_CALLBACK(gtkLeave));
        gdk_threads_init();
        Gtk::Main main(argc, argv);
        GThreadLock guiLock; // Match main.cc, including modal dialog lock handoff.
        g_mkdir_with_parents(root.c_str(), 0755);
        App::get().setArgv0(data);
        Options::load(true);
        auto& options = App::get().mut_options();
        const bool available = rtengine::ImageIO::isHEIFExportAvailable();
        SaveFormat sf;
        sf.format = "heic"; sf.heifBits = 12; sf.heifQuality = 77; sf.heifSubSamp = 3;
        sf.jpegQuality = 85; sf.jpegSubSamp = 2; sf.saveParams = true;
        options.overwriteOutputFile = true;
        options.saveFormat = sf;
        options.saveFormatBatch = sf;
        options.saveFormatBatch.heifBits = 8;
        options.saveFormatBatch.heifQuality = 61;
        options.saveFormatBatch.heifSubSamp = 2;
        options.saveToFile(root + "/roundtrip-options");
        Options loaded;
        loaded.readFromFile(root + "/roundtrip-options");
        require(loaded.saveFormat.format == "heic" && loaded.saveFormat.heifBits == 12 &&
            loaded.saveFormat.heifQuality == 77 && loaded.saveFormat.heifSubSamp == 3, "individual preferences round trip");
        require(loaded.saveFormatBatch.format == "heic" && loaded.saveFormatBatch.heifBits == 8 &&
            loaded.saveFormatBatch.heifQuality == 61 && loaded.saveFormatBatch.heifSubSamp == 2, "batch preferences round trip");
        { std::ofstream old(root + "/old-options"); old << "[Output]\nFormat=png\nPngBps=16\nFormatBatch=tif\nTiffBpsBatch=32\nTiffFloatBatch=true\n"; }
        Options old;
        old.readFromFile(root + "/old-options");
        require(old.saveFormat.format == "png" && old.saveFormat.pngBits == 16 &&
            old.saveFormat.heifBits == 10 && old.saveFormat.heifQuality == 92 && old.saveFormat.heifSubSamp == 1,
            "old individual preferences");
        require(old.saveFormatBatch.format == "tif" && old.saveFormatBatch.tiffBits == 32 && old.saveFormatBatch.tiffFloat,
            "old batch preferences");
        loaded.setDefaults();
        require(loaded.saveFormat.heifBits == 10 && loaded.saveFormat.heifQuality == 92 &&
            loaded.saveFormatBatch.heifSubSamp == 1, "reset HEIF defaults");
        auto values = columns("tif|85|2|16|32|1|0|1|1|1|1|");
        auto record = readBatchQueueFormat(values.cbegin(), values.cend(), sf, false);
        require(record.save.format == "tif" && record.save.tiffBits == 32 && record.save.tiffFloat &&
            !record.save.tiffUncompressed && record.save.bigTiff && record.force && record.fast &&
            record.save.heifQuality == 92 && record.save.heifBits == 10 && record.save.heifSubSamp == 1,
            "legacy queue columns and HEIF defaults");
        std::ostringstream encoded;
        writeBatchQueueFormat(encoded, {sf, true, false});
        values = columns(encoded.str());
        record = readBatchQueueFormat(values.cbegin(), values.cend(), SaveFormat(), false);
        require(values.size() == 14 && record.save.heifBits == 12 && record.save.heifQuality == 77 &&
            record.save.heifSubSamp == 3 && record.force && !record.fast, "new queue format round trip");
        values = columns("jpg|bad|2|8|16|0|1|1|0|");
        record = readBatchQueueFormat(values.cbegin(), values.cend(), sf, false);
        require(record.save.jpegQuality == 85 && !record.force && !record.fast && record.save.heifBits == 10,
            "short and malformed old queue fields");
        Panel panel;
        require(panel.count() == (available ? 10 : 7), "runtime preset visibility");
        for (int bits : {8, 10, 12}) {
            sf.heifBits = bits;
            panel.init(sf);
            auto selected = panel.getFormat();
            require(selected.format == "heic" && selected.heifBits == bits && selected.heifQuality == 77 &&
                selected.heifSubSamp == 3 && panel.heifVisible() && !panel.jpegVisible(), "HEIF panel state");
            require(panel.unavailableVisible() != available, "unavailable saved format warning");
        }
        for (int row = 0; row < 7; ++row) {
            panel.choose(row);
            const auto legacy = panel.getFormat();
            const char* formats[] = {"jpg", "tif", "tif", "tif", "tif", "png", "png"};
            require(legacy.format == formats[row], "legacy GUI preset order");
            require((row == 2 || row == 3) ? legacy.tiffBits == 16 :
                row == 4 ? legacy.tiffBits == 32 : row == 6 ? legacy.pngBits == 16 : true,
                "legacy GUI preset precision");
            require(legacy.tiffFloat == (row == 3 || row == 4), "legacy GUI float presets");
        }
        panel.choose(0);
        require(panel.getFormat().format == "jpg" && panel.getFormat().jpegQuality == 85 &&
            !panel.heifVisible() && panel.jpegVisible(), "JPEG controls preserved");
        require(rtengine::hasHeifExtension("photo.HeIf") && rtengine::hasHeifExtension("photo.HEIC") &&
            !rtengine::hasHeifExtension("photo.avif"), "HEIF export aliases");
        Gtk::IconTheme::get_default()->append_search_path(data + "/icons");
        Gtk::Window parent;
        SaveAsDialog dialog(root, &parent);
        for (int i = 0; i < 20; ++i) { pump(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
        dialog.setInitialFileName("photo.HEIF");
        dialog.formatChanged("heic");
        require(dialog.getFormat().format == "heic" && dialog.getFormat().heifBits == 12,
            "Save As restores HEIF preferences");
        bool accepted = false, warningShown = false;
        dialog.signal_response().connect([&](int response) { accepted = response == Gtk::RESPONSE_OK; });
        if (!available) {
            Glib::signal_idle().connect([&]() {
                for (auto* window : Gtk::Window::list_toplevels()) {
                    if (auto* message = dynamic_cast<Gtk::MessageDialog*>(window)) {
                        warningShown = true;
                        message->response(Gtk::RESPONSE_OK);
                        return false;
                    }
                }
                return true;
            });
        }
        dialog.okPressed();
        require(accepted == available && warningShown != available, "Save As availability result");
        require(dialog.getFileName() == root + "/photo.HEIF", "Save As retains extension alias");
        // Exercise the actual queue serializer, recovery loader, and worker.
        CacheManager::getInstance()->init();
        rtengine::Image16 input(64, 48);
        for (int y = 0; y < 48; ++y) for (int x = 0; x < 64; ++x) {
            input.r(y,x) = x * 1000; input.g(y,x) = y * 1300; input.b(y,x) = 30000;
        }
        const auto source = root + "/input.png";
        require(input.saveAsPNG(source, 16) == 0, "create queue input");
        input.setOutputProfile(Glib::file_get_contents(data + "/iccprofiles/output/RTv2_sRGB.icc"));
        const auto direct = root + "/direct.HEIF";
        require((input.saveToFile(direct) == 0) == available, "generic save dispatch availability");
        auto* thumb = CacheManager::getInstance()->getEntry(source);
        require(thumb, "queue thumbnail");
        rtengine::procparams::ProcParams params;
        params.icm.outputProfile = "file:" + data + "/iccprofiles/output/RTv2_sRGB.icc";
        for (int mode = 0; mode < 3; ++mode) {
            const std::string destination = root + "/batch-" + std::to_string(mode) + ".heif";
            options.saveFormatBatch = sf;
            options.saveFormatBatch.heifBits = mode == 1 ? 8 : 10;
            options.saveFormatBatch.heifSubSamp = mode == 1 ? 2 : 1;
            options.savePathFolder = root;
            options.saveUsePathTemplate = false;
            {
                BatchQueue queue(nullptr);
                auto* job = rtengine::ProcessingJob::create(source, false, params);
                auto* entry = new BatchQueueEntry(job, params, source, 64, 48, thumb, true);
                entry->saveFormat = sf;
                entry->forceFormatOpts = mode == 0;
                if (mode != 2) entry->outFileName = destination;
                queue.addEntries({entry});
            }
            BatchQueue restored(nullptr);
            require(restored.loadBatchQueue() && restored.hasJobs(), "queue recovery loader");
            QueueListener listener;
            const auto expected = mode == 2 ? root + "/input.heic" : destination;
            { std::ofstream preserved(expected); preserved << "previous output"; }
            processQueue(restored, listener, available);
            if (available) {
                require(Glib::file_test(expected, Glib::FILE_TEST_EXISTS) &&
                    Glib::file_test(expected + ".out.pp3", Glib::FILE_TEST_EXISTS), "batch image and sidecar");
            } else {
                std::ifstream preserved(expected);
                std::string content; std::getline(preserved, content);
                require(content == "previous output" && !Glib::file_test(expected + ".out.pp3", Glib::FILE_TEST_EXISTS),
                    "failed batch preserves destination and writes no sidecar");
            }
        }
        // A pre-HEIF queue row must still recover and export its original format.
        const auto legacyParams = Options::rtdir + "/batch/legacy.pp3";
        require(params.save(legacyParams) == 0, "legacy queue parameters");
        const auto legacyOutput = root + "/legacy-queue.jpg";
        {
            std::ofstream oldQueue(Options::rtdir + "/batch/queue.csv");
            oldQueue << "legacy header\n" << source << '|' << legacyParams << '|' << legacyOutput
                << "|jpg|85|2|8|16|0|1|1|1|0|0|\n";
        }
        {
            BatchQueue queue(nullptr);
            QueueListener listener;
            require(queue.loadBatchQueue() && queue.hasJobs(), "actual legacy queue recovery");
            processQueue(queue, listener, true);
            require(Glib::file_test(legacyOutput, Glib::FILE_TEST_EXISTS) &&
                Glib::file_test(legacyOutput + ".out.pp3", Glib::FILE_TEST_EXISTS), "legacy queue export and sidecar");
        }
        if (available) {
            rtengine::IIOSampleFormat format;
            rtengine::IIOSampleArrangement arrangement;
            if (rtengine::ImageIO::getHEIFSampleFormat(direct, format, arrangement) == IMIO_SUCCESS) {
                auto* imported = CacheManager::getInstance()->getEntry(direct);
                require(imported, "HEIF browser cache thumbnail");
                rtengine::PreviewImage preview(direct, "HEIF", rtengine::PreviewImage::PIM_EmbeddedOrRaw);
                require(preview.getImage() && preview.getImage()->get_width() == 64 &&
                    preview.getImage()->get_height() == 48, "HEIF inspector geometry");
                int error = 0;
                auto* initial = rtengine::InitialImage::load(direct, false, &error);
                require(initial && !error && initial->getEmbeddedProfile(), "HEIF editor input");
                initial->decreaseRef();
                params.toneCurve.expcomp = 0.5;
                BatchQueue queue(nullptr);
                auto* job = rtengine::ProcessingJob::create(direct, false, params);
                auto* entry = new BatchQueueEntry(job, params, direct, 64, 48, imported, true);
                entry->saveFormat = sf;
                entry->saveFormat.format = "tif";
                entry->saveFormat.tiffBits = 16;
                entry->forceFormatOpts = true;
                entry->outFileName = root + "/import-edit.tif";
                queue.addEntries({entry});
                QueueListener listener; processQueue(queue, listener, true);
                require(Glib::file_test(root + "/import-edit.tif", Glib::FILE_TEST_EXISTS), "HEIF edited queue export");
                imported->decreaseRef();
            }
        }
        thumb->decreaseRef();
        std::cout << "GUI, saved preferences, old/new queue records, recovery and batch exports passed; HEIF available=" << available << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
