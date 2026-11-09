/* White balance feedback ordering regression tests. GPL-3.0-or-later. */
#include "options.h"
#include "rtengine/refreshmap.h"
#include "tools/whitebalance.h"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

using namespace rtengine;
using namespace rtengine::procparams;

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void drain()
{
    while (Gtk::Main::events_pending()) {
        Gtk::Main::iteration(false);
    }
}

template <class T> T* find(Gtk::Container& container)
{
    for (auto* child : container.get_children()) {
        if (auto* found = dynamic_cast<T*>(child)) {
            return found;
        }
        if (auto* nested = dynamic_cast<Gtk::Container*>(child)) {
            if (auto* found = find<T>(*nested)) {
                return found;
            }
        }
    }
    return nullptr;
}

void collect(Gtk::Container& container, std::vector<Adjuster*>& adjusters)
{
    for (auto* child : container.get_children()) {
        if (auto* a = dynamic_cast<Adjuster*>(child)) {
            adjusters.push_back(a);
        } else if (auto* nested = dynamic_cast<Gtk::Container*>(child)) {
            collect(*nested, adjusters);
        }
    }
}

// Observe all labels and sensitivity so a stale callback cannot pass by merely
// protecting the two numeric controls while corrupting diagnostics or mode UI.
std::string presentation(Gtk::Container& container)
{
    std::string result;
    for (auto* child : container.get_children()) {
        result += child->get_sensitive() ? "1" : "0";
        if (auto* label = dynamic_cast<Gtk::Label*>(child)) {
            result += label->get_text();
        }
        if (auto* nested = dynamic_cast<Gtk::Container*>(child)) {
            result += presentation(*nested);
        }
    }
    return result;
}

struct Listener : ToolPanelListener
{
    int notifications = 0;

    void refreshPreview(const ProcEvent&) override {}

    void panelChanged(const ProcEvent&, const Glib::ustring&) override
    {
        ++notifications;
    }

    void setTweakOperator(TweakOperator*) override {}

    void unsetTweakOperator(TweakOperator*) override {}
};

struct Camera : WBProvider
{
    void getCamWB(double& temp, double& green, StandardObserver) override
    {
        temp = 5100.;
        green = 1.;
    }
};

struct Panel
{
    Listener listener;
    Camera camera;
    WhiteBalance wb;
    std::vector<Adjuster*> adjusters;
    Adjuster *temp = nullptr, *tint = nullptr, *equal = nullptr;

    Panel()
    {
        wb.setListener(&listener);
        wb.setWBProvider(&camera);
        collect(wb, adjusters);
        for (auto* a : adjusters) {
            a->setDelay(
                60000);  // Keep the delayed notification pending during deterministic ordering tests.
            auto* label = find<Gtk::Label>(*a);
            if (!label) {
                continue;
            }
            if (label->get_text() == M("TP_WBALANCE_TEMPERATURE")) {
                temp = a;
            }
            if (label->get_text() == M("TP_WBALANCE_GREEN")) {
                tint = a;
            }
            if (label->get_text() == M("TP_WBALANCE_EQBLUERED")) {
                equal = a;
            }
        }
        require(temp && tint && equal, "missing WB controls");
        load("Custom");
    }

    ~Panel()
    {
        // Disconnect outstanding delay sources before destroying the widgets.
        for (auto* a : adjusters) {
            a->setDelay(0);
        }
    }

    void load(const char* method)
    {
        ProcParams p;
        p.wb.method = method;
        p.wb.enabled = true;
        p.wb.temperature = 5000;
        p.wb.green = 1.;
        wb.read(&p);
    }

    ProcParams values()
    {
        ProcParams p;
        wb.write(&p);
        return p;
    }

    void feedback(std::uint64_t revision, double temperature = 5400., double green = 1.1)
    {
        const auto method = values().wb.method;
        const auto mode = method == "autitcgreen"
                              ? AutoWBListener::AWBMode::TEMP_CORRELATION_RAW
                          : method == "autold" ? AutoWBListener::AWBMode::RGB_GREY
                                               : AutoWBListener::AWBMode::NONE;
        wb.WBChanged(revision, temperature, green, 2., 1., 3., 5000.f, .1f, 2, 20, 1.f,
                     .2f, 20, 100.f, 1000.f, mode);
    }
};

void edit(Adjuster& a, double value)
{
    auto* spin = find<Gtk::SpinButton>(a);
    require(spin, "missing spin button");
    spin->set_value(value);  // Real GTK value_changed path, including the immediate hook.
}

void selectMethod(WhiteBalance& wb, const Glib::ustring& label)
{
    auto* combo = find<MyComboBox>(wb);
    require(combo, "missing method combo");
    struct Selection
    {
        GtkComboBox* combo;
        const char* label;
        bool found;
    } selection{ combo->gobj(), label.c_str(), false };
    gtk_tree_model_foreach(
        gtk_combo_box_get_model(combo->gobj()),
        [](GtkTreeModel* model, GtkTreePath*, GtkTreeIter* iter,
           gpointer data) -> gboolean {
            auto& s = *static_cast<Selection*>(data);
            gchar* text = nullptr;
            gtk_tree_model_get(model, iter, 1, &text, -1);
            const bool matches = text && s.label == std::string(text);
            g_free(text);
            if (matches) {
                gtk_combo_box_set_active_iter(s.combo, iter);
                s.found = true;
            }
            return matches;
        },
        &selection);
    require(selection.found, "method missing from model");
}

void guiTests()
{
    for (const char* method : { "Camera", "autold", "autitcgreen" }) {
        Panel p;
        p.load(method);
        const auto rev = p.wb.getWBRevision();
        p.feedback(rev);
        drain();
        require(p.temp->getValue() == 5400. && p.tint->getValue() == 1.1,
                "current computed WB not applied");
        require(p.wb.getWBRevision() == rev && p.listener.notifications == 0,
                "feedback generated another edit");
        p.feedback(rev, 6300., 1.3);  // queued before the user edit
        edit(*p.temp, 5500.);
        require(p.wb.getWBRevision() > rev, "edit did not invalidate synchronously");
        require(p.values().wb.method == "Custom",
                "manual edit still submitted as auto/camera");
        require(p.listener.notifications == 0, "immediate hook bypassed delay");
        const auto ui = presentation(p.wb);
        drain();
        require(p.temp->getValue() == 5500. && p.tint->getValue() == 1.1,
                "stale feedback overwrote pending edit");
        require(presentation(p.wb) == ui, "stale diagnostics or sensitivity applied");
        p.temp->resetValue(true);
        require(p.temp->getIntValue() == 5400, "stale feedback changed reset default");
    }
    {
        Panel p;
        p.load("autold");
        const auto rev = p.wb.getWBRevision();
        p.feedback(rev);
        auto* slider = find<Gtk::Scale>(*p.tint);
        require(slider, "missing tint scale");
        slider->set_value(slider->get_value() + 1.);
        const double chosen = p.tint->getValue();
        require(p.wb.getWBRevision() > rev && p.values().wb.method == "Custom",
                "scale edit did not immediately invalidate/select Custom");
        drain();
        require(p.tint->getValue() == chosen, "scale edit was overwritten");
    }
    for (const char* method : { "Custom", "Daylight" }) {
        Panel p;
        p.load(method);
        const auto before = p.values().wb;
        const auto ui = presentation(p.wb);
        p.feedback(p.wb.getWBRevision());
        drain();
        require(p.values().wb.temperature == before.temperature
                    && p.values().wb.green == before.green,
                "feedback changed user/preset values");
        require(presentation(p.wb) != ui, "current manual diagnostics not updated");
    }
    {
        Panel p;
        p.load("autold");
        auto rev = p.wb.getWBRevision();
        edit(*p.equal, 1.2);
        require(p.wb.getWBRevision() > rev && p.values().wb.method == "autold",
                "auto equalizer switched to custom");
        p.feedback(p.wb.getWBRevision());
        drain();
        require(p.temp->getIntValue() == 5400, "new auto result not applied");
    }
    // Exercise invalidation paths with an automatic target, where ignoring all
    // numeric feedback would otherwise make an inadequate fix pass the tests.
    for (int transition = 0; transition < 6; ++transition) {
        Panel p;
        p.load("autold");
        auto rev = p.wb.getWBRevision();
        p.feedback(rev, 6300., 1.3);
        switch (transition) {
            case 0:
                p.load("autold");
                break;  // history/profile, even with the same method
            case 1:
                selectMethod(p.wb, M("TP_WBALANCE_CAMERA"));
                break;
            case 2:
                p.wb.setWB(5700, 1.2);
                break;
            case 3:
                p.wb.invalidateWBFeedback();
                break;  // image detach/replacement boundary
            case 4:
                find<CheckBox>(p.wb)->set_active(!find<CheckBox>(p.wb)->get_active());
                break;
            case 5:
                p.wb.getExpander()->setEnabled(false);
                break;
        }
        require(p.wb.getWBRevision() > rev, "state transition did not invalidate");
        const auto before = p.values().wb;
        const auto ui = presentation(p.wb);
        drain();
        require(p.values().wb.temperature == before.temperature
                    && p.values().wb.green == before.green,
                "feedback survived state transition");
        require(presentation(p.wb) == ui, "state transition accepted stale presentation");
    }
    {
        Panel p;
        p.load("autold");
        const auto old = p.wb.getWBRevision();
        edit(*p.equal, 1.1);
        const auto next = p.wb.getWBRevision();
        edit(*p.equal, 1.2);
        const auto current = p.wb.getWBRevision();
        p.feedback(current, 5800., 1.2);
        p.feedback(old, 6200., 1.4);
        p.feedback(next, 6100., 1.3);
        drain();
        require(p.temp->getIntValue() == 5800 && p.tint->getValue() == 1.2,
                "older results overwrote a newer accepted result");
    }
    {
        Panel p;
        const auto rev = p.wb.getWBRevision();
        p.temp->setValue(5600.);
        require(p.wb.getWBRevision() == rev,
                "programmatic setValue reported as user input");
        p.wb.setBatchMode(true);
        p.temp->setEditedState(UnEdited);
        edit(*p.temp, 5700.);
        p.temp->notifyListener();
        p.temp->spinChanged();
        ParamsEdited edited;
        ProcParams params;
        p.wb.write(&params, &edited);
        require(edited.wb.temperature && params.wb.method == "Custom",
                "batch edit lost edited state");
    }
    {
        // A listener that does not override the new hook retains delayed delivery.
        struct Ordinary : AdjusterListener
        {
            int count = 0;
            void adjusterChanged(Adjuster*, double) override { ++count; }
        } listener;
        Adjuster a("ordinary", 0., 10., 1., 0.);
        a.setAdjusterListener(&listener);
        a.setDelay(10);
        edit(a, 1.);
        require(listener.count == 0, "ordinary adjuster notified immediately");
        auto loop = Glib::MainLoop::create();
        auto timer = Glib::signal_timeout().connect(
            [&]() {
                loop->quit();
                return false;
            },
            100);
        loop->run();
        timer.disconnect();
        require(listener.count == 1, "ordinary delayed notification missing/duplicated");
        a.setDelay(0);
    }
    std::cout
        << "GUI ordering, current feedback, transitions, batch and delay tests passed\n";
}

struct WorkerFeedback : AutoWBListener, ImageTypeListener
{
    std::mutex mutex;
    std::condition_variable cv;
    bool release = false;
    bool snapshotTaken = false;
    std::vector<std::pair<std::uint64_t, double>> results;

    void WBChanged(std::uint64_t revision,
                   double temperature,
                   double,
                   double,
                   double,
                   double,
                   float,
                   float,
                   int,
                   int,
                   float,
                   float,
                   int,
                   float,
                   float,
                   AWBMode) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        results.emplace_back(revision, temperature);
        cv.notify_all();
    }

    void imageTypeChanged(bool, bool, bool, bool, bool) override
    {
        // This production callback runs after the parameter snapshot is copied,
        // but before WB computation/reporting. Submit the next input here so
        // reading nextWBRevision instead of the captured revision would fail.
        std::unique_lock<std::mutex> lock(mutex);
        snapshotTaken = true;
        cv.notify_all();
        cv.wait(lock, [&]() { return release; });
    }

    bool waitForSnapshot()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(30),
                           [&]() { return snapshotTaken; });
    }

    bool wait(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(30),
                           [&]() { return results.size() >= count; });
    }

    void unblock()
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        cv.notify_all();
    }
};

void workerTest(const std::string& directory)
{
    const auto path = directory + "/input.png";
    auto pixels = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 128, 96);
    pixels->fill(0x8090'a0ff);
    pixels->save(path, "png");
    int error = 0;
    auto* image = InitialImage::load(path, false, &error, nullptr);
    require(image && !error, "failed to load generated test image");
    image->increaseRef();
    auto* processor = StagedImageProcessor::create(image);
    WorkerFeedback feedback;
    processor->setAutoWBListener(&feedback);
    processor->setImageTypeListener(&feedback);
    processor->setPreviewScale(1);
    bool first = false, second = false;
    auto* params = processor->beginUpdateParams();
    params->setDefaults();
    params->wb.enabled = true;
    params->wb.method = "Custom";
    params->wb.temperature = 5000;
    params->wb.green = 1.;
    processor->setWBRevision(41);
    processor->endUpdateParams(EvPhotoLoaded);
    first = feedback.waitForSnapshot();
    if (first) {
        params = processor->beginUpdateParams();
        params->wb.temperature = 6000;
        processor->setWBRevision(42);
        processor->endUpdateParams(EvWBTemp);
    }
    feedback.unblock();
    if (first) {
        second = feedback.wait(2);
    }
    processor->stopProcessing();
    bool retained = false;
    if (second) {
        processor->startProcessing(ALL);
        retained = feedback.wait(3);
        processor->stopProcessing();
    }
    processor->setAutoWBListener(nullptr);
    processor->setImageTypeListener(nullptr);
    StagedImageProcessor::destroy(processor);
    image->decreaseRef();
    require(first && second, "timed out waiting for real worker feedback");
    require(feedback.results[0] == std::make_pair(std::uint64_t(41), 5000.),
            "first snapshot revision/value mismatch");
    require(feedback.results[1] == std::make_pair(std::uint64_t(42), 6000.),
            "next snapshot revision/value mismatch");
    require(retained && feedback.results[2] == std::make_pair(std::uint64_t(42), 6000.),
            "processing-only request lost the submitted revision");
    std::cout << "Real worker snapshot test passed\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (!gtk_init_check(&argc, &argv)) {
        std::cerr << "A GTK display is required (Xvfb or Broadway can be used).\n";
        return 77;
    }
    Gtk::Main gtk(argc, argv);
    if (argc != 2) {
        return 2;
    }
    gchar* temporary = g_dir_make_tmp("rt-wb-feedback-XXXXXX", nullptr);
    if (!temporary) {
        return 2;
    }
    const std::string directory(temporary);
    g_free(temporary);
    int result = 0;
    try {
        g_setenv("RT_SETTINGS", (directory + "/settings").c_str(), TRUE);
        g_setenv("RT_CACHE", (directory + "/cache").c_str(), TRUE);
        App::get().setArgv0(argv[1]);
        Options::load(true);
        Gtk::IconTheme::get_default()->append_search_path(std::string(argv[1])
                                                          + "/icons");
        guiTests();
        workerTest(directory);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    std::filesystem::remove_all(directory);
    return result;
}
