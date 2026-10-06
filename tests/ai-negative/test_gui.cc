/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rtgui/ainegativegui.h"
#include "rtgui/tools/filmnegative.h"
#include "rtgui/options.h"
#include "rtgui/multilangmgr.h"
#include "rtgui/rtscalable.h"
#include "rtengine/cJSON.h"
#include "rtengine/rtengine.h"
#include "rtengine/rtapp.h"
#include "rtengine/iimage.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <cstring>
#include <thread>

namespace
{
int checks = 0;
void check(bool value, const char *why)
{
    ++checks;
    if (!value) throw std::runtime_error(why);
}
struct Listener : ToolPanelListener {
    int events = 0;
    void refreshPreview(const rtengine::ProcEvent &) override {}
    void panelChanged(const rtengine::ProcEvent &, const Glib::ustring &) override { ++events; }
    void setTweakOperator(rtengine::TweakOperator *) override {}
    void unsetTweakOperator(rtengine::TweakOperator *) override {}
};
std::string envelope(const std::string &value)
{
    auto j = cJSON_CreateObject();
    auto m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "content", value.c_str());
    cJSON_AddItemToObject(j, "message", m);
    cJSON_AddBoolToObject(j, "done", true);
    char *p = cJSON_PrintUnformatted(j);
    std::string s(p);
    cJSON_free(p);
    cJSON_Delete(j);
    return s;
}
Gtk::Button *findButton(Gtk::Container &container, const Glib::ustring &label)
{
    for (auto child : container.get_children()) {
        auto button = dynamic_cast<Gtk::Button *>(child);
        if (button && button->get_label() == label) return button;
        auto nested = dynamic_cast<Gtk::Container *>(child);
        if (nested)
            if (auto result = findButton(*nested, label)) return result;
    }
    return nullptr;
}
void testCropSeed(ai_negative::Settings baseline)
{
    rtengine::procparams::CropParams crop;
    crop.enabled = true;
    crop.x = 422;
    crop.y = 842;
    crop.w = 625;
    crop.h = 429;
    int samples = 0;
    auto sample = [&](int x, int y, int size, std::array<double, 3> &rgb) {
        check(x > crop.x && x < crop.x + crop.w && y > crop.y && y < crop.y + crop.h && size == 8,
            "Crop seed samples inside the selected film frame");
        ++samples;
        rgb = {{1000. + samples, 2000. + samples, 3000. + samples}};
        if (samples == 1) rgb[0] = std::numeric_limits<double>::quiet_NaN();
        return true;
    };
    const auto original = baseline;
    auto seed = makeAiNegativeCropSeed(crop, baseline, sample);
    check(seed && samples == 25 && seed->refInput[0] == 1014, "Crop seed uses valid channel medians");
    check(seed->greenExp == baseline.greenExp && seed->outputLevel == 7.53 && seed->blueBalance == 0,
        "Crop seed preserves inversion strength and supplies a neutral midtone");
    check(baseline.refInput == original.refInput && baseline.outputLevel == original.outputLevel, "Seed leaves baseline untouched");
    crop.enabled = false;
    check(!makeAiNegativeCropSeed(crop, baseline, sample) && samples == 25, "No crop means no extra sampling");
    crop.enabled = true;
    crop.w = 5;
    check(!makeAiNegativeCropSeed(crop, baseline, sample), "Tiny crop ignored");
    crop.w = 625;
    check(!makeAiNegativeCropSeed(crop, baseline, [](int, int, int, std::array<double, 3> &) { return false; }), "Failed samples produce no seed");
}
void testPreferencesLifecycle()
{
    std::atomic_int requests{0};
    AiNegativePreferences panel([&](const ai_negative::Config &, const ai_negative::Request &, std::atomic_bool &) {
        ++requests;
        return envelope("{\"ok\":true}");
    });
    ai_negative::Config config;
    config.model = "offline-test";
    panel.read(config);
    panel.validateForSave();
    auto invalid = config;
    invalid.credentialEnv = "not-a-variable-name";
    panel.read(invalid);
    bool rejected = false;
    try {
        panel.validateForSave();
    } catch (const ai_negative::Error &) {
        rejected = true;
    }
    check(rejected, "Invalid credential variable cannot be saved through Preferences");
    panel.read(ai_negative::Config{});
    panel.validateForSave();
    check(panel.write().model.empty(), "Saving unconfigured defaults does not select a model");
    panel.read(config);
    auto test = findButton(panel, M("AI_TEST_CONNECTION"));
    check(test != nullptr, "Preferences connection test exists");
    test->clicked();
    // Deliberately do not dispatch GTK timers before the second click: this
    // covers a completed worker whose previous completion callback is pending.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(requests == 1, "First preferences test completed");
    test->clicked();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && test->get_label() != M("AI_TEST_CONNECTION")) {
        while (Gtk::Main::events_pending())
            Gtk::Main::iteration();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    check(requests == 1 && test->get_label() == M("AI_TEST_CONNECTION"), "Cancel after completion cannot start another request");
    test->clicked();
    const auto nextDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < nextDeadline && test->get_label() != M("AI_TEST_CONNECTION")) {
        while (Gtk::Main::events_pending())
            Gtk::Main::iteration();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    check(requests == 2 && test->get_label() == M("AI_TEST_CONNECTION"), "Repeated connection test has one completion callback");
    std::atomic_bool cancelled{false};
    {
        AiNegativePreferences pending([&](const ai_negative::Config &, const ai_negative::Request &, std::atomic_bool &cancel) {
            while (!cancel)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cancelled = true;
            return envelope("{\"ok\":true}");
        });
        pending.read(config);
        findButton(pending, M("AI_TEST_CONNECTION"))->clicked();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    check(cancelled, "Closing preferences cancels and joins the connection test");
    check(aiNegativeErrorKey(std::runtime_error("private path or provider content")) == "AI_ERROR_UNEXPECTED", "Unexpected errors do not expose private details");
}
void savePreview(const ai_negative::Preview &preview, const std::string &path)
{
    gsize bytes = 0;
    auto png = g_base64_decode(preview.pngBase64.c_str(), &bytes);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(png), bytes);
    g_free(png);
}
void checkPixelOnlyPng(const ai_negative::Preview &preview)
{
    gsize length = 0;
    auto bytes = std::unique_ptr<guchar, decltype(&g_free)>(g_base64_decode(preview.pngBase64.c_str(), &length), g_free);
    const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    check(length >= 8 && std::memcmp(bytes.get(), signature, 8) == 0, "Preview has a PNG signature");
    std::size_t offset = 8;
    bool ended = false;
    while (offset + 12 <= length) {
        const auto p = bytes.get() + offset;
        const std::size_t size = (std::size_t(p[0]) << 24) | (std::size_t(p[1]) << 16) | (std::size_t(p[2]) << 8) | p[3];
        check(size <= length - offset - 12, "PNG chunk length is bounded");
        const std::string type(reinterpret_cast<const char *>(p + 4), 4);
        check(type != "eXIf" && type != "tEXt" && type != "zTXt" && type != "iTXt" && type != "iCCP", "PNG contains no EXIF, text or source profile metadata");
        offset += size + 12;
        if (type == "IEND") {
            ended = true;
            break;
        }
    }
    check(ended && offset == length, "PNG contains no trailing source data");
}
void testPhoto(const std::string &filename, const std::string &profile, const std::string &scratch,
    const std::string &model)
{
    struct PreviewListener : rtengine::PreviewImageListener {
        std::atomic_bool ready{false};
        void setImage(rtengine::IImage8 *, double, const rtengine::procparams::CropParams &,
            const rtengine::procparams::CropGuideParams &) override {}
        void delImage(rtengine::IImage8 *image) override { delete image; }
        void imageReady(const rtengine::procparams::CropParams &, const rtengine::procparams::CropGuideParams &) override { ready = true; }
    } listener;
    rtengine::procparams::ProcParams params;
    check(params.load(profile) == 0, "Load photo profile");
    params.filmNegative.enabled = true;
    int error = 0;
    auto initial = rtengine::InitialImage::load(filename, false, &error);
    check(initial && !error, "Load photo");
    std::unique_ptr<rtengine::StagedImageProcessor, decltype(&rtengine::StagedImageProcessor::destroy)> processor(
        rtengine::StagedImageProcessor::create(initial), rtengine::StagedImageProcessor::destroy);
    processor->setPreviewImageListener(&listener);
    processor->setPreviewScale(4);
    *processor->beginUpdateParams() = params;
    processor->endUpdateParams(rtengine::EvPhotoLoaded);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!listener.ready && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    check(listener.ready, "Photo preview ready");
    processor->stopProcessing();
    processor->getParams(&params);
    FilmNegative panel;
    panel.show_all_children();
    panel.read(&params);
    const auto baseline = panel.getAiSettings();
    auto seed = makeAiNegativeCropSeed(params.crop, baseline, [&](int x, int y, int size, std::array<double, 3> &rgb) {
        rtengine::procparams::FilmNegativeParams::RGB input, output;
        if (!processor->getFilmNegativeSpot(x, y, size, input, output)) return false;
        rgb = {{input.r, input.g, input.b}};
        return true;
    });
    check(bool(seed), "Photo crop seed available");
    std::atomic_bool cancel{false};
    auto render = makeAiNegativeRenderer(filename, false, params);
    savePreview(render(baseline, cancel), scratch + "/photo-baseline.png");
    savePreview(render(*seed, cancel), scratch + "/photo-crop-reference.png");
    if (!model.empty()) {
        ai_negative::Config config;
        config.model = model;
        auto result = ai_negative::optimize(config, baseline, render, cancel, [](const std::string &progress) { std::cout << progress << std::endl; }, ai_negative::httpTransport, seed);
        savePreview(result.chosen.preview, scratch + "/photo-ai.png");
        std::cout << "Photo selected: " << result.chosen.id << std::endl;
    }
}
void testDialogs(Gtk::Window &parent, ai_negative::Settings s)
{
    ai_negative::Config config;
    config.model = "offline-test";
    for (const auto mode : {"apply", "cancel", "stale", "close", "escape", "baseline", "error", "config-change"}) {
        config.model = "offline-test";
        int calls = 0, polls = 0;
        bool current = true;
        bool originalConfig = true;
        ai_negative::Settings selected;
        selected.greenExp = .7;
        auto tick = Glib::signal_timeout().connect([&] {
            ++polls;
            if (std::string(mode) == "config-change") config.model = "changed-after-start";
            for (auto window : Gtk::Window::list_toplevels()) {
                auto dialog = dynamic_cast<Gtk::Dialog *>(window);
                if (!dialog || dialog->get_title() != M("AI_NEGATIVE_TITLE")) continue;
                if (std::string(mode) == "stale") {
                    current = false;
                    return false;
                }
                if (std::string(mode) == "close") {
                    dialog->close();
                    return false;
                }
                if (std::string(mode) == "escape") {
                    auto event = gdk_event_new(GDK_KEY_PRESS);
                    event->key.keyval = GDK_KEY_Escape;
                    gboolean handled = false;
                    g_signal_emit_by_name(dialog->gobj(), "key-press-event", event, &handled);
                    gdk_event_free(event);
                    return false;
                }
                if (std::string(mode) == "cancel") {
                    auto button = findButton(*dialog, M("GENERAL_CANCEL"));
                    if (button) button->clicked();
                    return false;
                }
                if ((std::string(mode) == "baseline" || std::string(mode) == "error") && polls > 8) {
                    auto apply = dialog->get_widget_for_response(Gtk::RESPONSE_APPLY);
                    check(apply && !apply->get_sensitive(), "Baseline winner or failure cannot apply an edit");
                    findButton(*dialog, M("GENERAL_CANCEL"))->clicked();
                    return false;
                }
                auto apply = dialog->get_widget_for_response(Gtk::RESPONSE_APPLY);
                if ((std::string(mode) == "apply" || std::string(mode) == "config-change") && apply && apply->get_sensitive()) {
                    dialog->response(Gtk::RESPONSE_APPLY);
                    return false;
                }
                if (polls > 80) {
                    dialog->response(Gtk::RESPONSE_CANCEL);
                    return false;
                }
            }
            return true;
        },
            50);
        auto render = [mode](const ai_negative::Settings &, std::atomic_bool &) {
            if (std::string(mode) == "error") throw ai_negative::Error("AI_ERROR_RENDER");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return ai_negative::Preview{2, 1, {60, 70, 80, 180, 190, 200}, "mock"};
        };
        auto transport = [&](const ai_negative::Config &requestConfig, const ai_negative::Request &, std::atomic_bool &) {
            originalConfig = originalConfig && requestConfig.model == "offline-test";
            if (++calls == 1) return envelope("{\"candidates\":[" + ai_negative::serializeSettings(s) + "]}");
            return envelope(std::string("{\"selected\":\"") + (std::string(mode) == "baseline" ? "baseline" : "candidate_1") + "\",\"refinement\":null}");
        };
        const auto changed = runAiNegative(&parent, config, s, render, [&] { return current; }, selected, transport);
        tick.disconnect();
        check(changed == (std::string(mode) == "apply" || std::string(mode) == "config-change"), "Dialog apply/cancel/stale/close/escape contract");
        check(originalConfig, "Running job retains its original provider configuration");
        if (!changed) check(selected.greenExp == .7, "Cancelled dialog leaves caller settings untouched");
    }
}
} // namespace
int main(int argc, char **argv)
{
    try {
        if (argc < 3) throw std::runtime_error("Usage: gui-test source-root scratch-directory");
        const std::string root = argv[1], scratch = argv[2];
        const bool liveOllama = argc > 3 && std::string(argv[3]) == "--ollama";
        if (liveOllama && argc != 5) throw std::runtime_error("Usage: gui-test source-root scratch-directory --ollama MODEL_NAME");
        const std::string liveModel = liveOllama ? argv[4] : "";
        Gtk::Main main(argc, argv);
        App::get().setArgv0(root + "/rtdata");
        langMgr.load("default", {root + "/rtdata/languages/default"});
        Gtk::Window parent;
        RTScalable::init(&parent);
        Gtk::IconTheme::get_default()->append_search_path(root + "/rtdata/icons");
        Gtk::Settings::get_default()->property_gtk_icon_theme_name() = "rawtherapee";
        rtengine::init(&App::get().options().rtSettings, root + "/rtdata", scratch, false);
        if (argc >= 6 && std::string(argv[3]) == "--photo") {
            testPhoto(argv[4], argv[5], scratch, argc > 6 ? argv[6] : "");
            rtengine::cleanup();
            return 0;
        }
        {
            Options options;
            options.aiNegative = {"compatible", "http://localhost:1234/v1", "fixture-vision", "TEST_API_KEY", 99};
            options.saveToFile(scratch + "/options");
            Options loaded;
            loaded.readFromFile(scratch + "/options");
            check(loaded.aiNegative.provider == "compatible" && loaded.aiNegative.timeoutSeconds == 99 &&
                      loaded.aiNegative.model == "fixture-vision" && loaded.aiNegative.credentialEnv == "TEST_API_KEY",
                "AI preferences persist");
            Gtk::OffscreenWindow preferencesPreview;
            preferencesPreview.set_default_size(680, 420);
            AiNegativePreferences panel;
            panel.read(loaded.aiNegative);
            preferencesPreview.add(panel);
            preferencesPreview.show_all_children();
            preferencesPreview.show();
            while (Gtk::Main::events_pending())
                Gtk::Main::iteration();
            if (auto pixels = preferencesPreview.get_pixbuf()) pixels->save(scratch + "/preferences.png", "png");
            check(panel.write().endpoint == loaded.aiNegative.endpoint && panel.write().model == loaded.aiNegative.model, "Preferences UI roundtrip");
            loaded.setDefaults();
            check(loaded.aiNegative.provider == "ollama" && loaded.aiNegative.model.empty(), "Defaults restore local unconfigured provider");
        }
        ai_negative::Settings settings;
        settings.refInput = {{18000, 12000, 6000}};
        settings.outputLevel = 5.42;
        testCropSeed(settings);
        testPreferencesLifecycle();
        {
            Gtk::OffscreenWindow filmPreview;
            filmPreview.set_default_size(420, 650);
            FilmNegative panel;
            panel.show_all_children();
            Listener listener;
            panel.setListener(&listener);
            filmPreview.add(*panel.getExpander());
            panel.setExpanded(true);
            rtengine::procparams::ProcParams params;
            params.filmNegative = aiNegativeParams(settings, params.filmNegative);
            panel.read(&params);
            const auto initial = params;
            filmPreview.show_all_children();
            filmPreview.show();
            while (Gtk::Main::events_pending())
                Gtk::Main::iteration();
            if (auto pixels = filmPreview.get_pixbuf()) pixels->save(scratch + "/film-negative.png", "png");
            settings.outputLevel = 5.65;
            settings.blueBalance = .1;
            panel.applyAiSettings(settings);
            panel.write(&params);
            check(listener.events == 1, "Apply emits exactly one history event");
            check(params.filmNegative == aiNegativeParams(settings, initial.filmNegative), "Applied sliders exactly match rendered engine parameters");
            auto expected = initial;
            expected.filmNegative = params.filmNegative;
            check(expected == params, "Only Film Negative changes");
            check(params.save(scratch + "/conversion.pp3") == 0, "Save profile");
            rtengine::procparams::ProcParams loaded;
            check(loaded.load(scratch + "/conversion.pp3") == 0 && loaded.filmNegative == params.filmNegative, "Accepted conversion profile roundtrip");
            panel.read(&initial);
            panel.write(&loaded);
            check(loaded.filmNegative == initial.filmNegative, "History restoration reproduces previous settings");
            panel.setBatchMode(true);
            auto button = findButton(panel, M("AI_USE"));
            check(button && !button->get_sensitive(), "AI disabled in batch mode");
            for (auto compatibility : {rtengine::procparams::FilmNegativeParams::BackCompat::V1, rtengine::procparams::FilmNegativeParams::BackCompat::V2}) {
                auto legacy = initial;
                legacy.filmNegative.backCompat = compatibility;
                panel.read(&legacy);
                bool rejected = false;
                try {
                    panel.getAiSettings();
                } catch (...) {
                    rejected = true;
                }
                check(rejected, "Unresolved legacy settings are not silently reinterpreted");
                panel.write(&legacy);
                check(legacy.filmNegative.backCompat == compatibility, "Restoring legacy history resets the previous AI upgrade flag");
            }
        }
        testDialogs(parent, settings);
        // A synthetic orange-mask negative: engine smoke test with crop and rotation.
        {
            auto image = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 120, 80);
            for (int y = 0; y < 80; ++y)
                for (int x = 0; x < 120; ++x) {
                    auto p = image->get_pixels() + y * image->get_rowstride() + x * 3;
                    p[0] = 100 + x;
                    p[1] = 40 + y;
                    p[2] = 30 + x / 3;
                }
            image->save(scratch + "/negative.png", "png");
            rtengine::procparams::ProcParams params;
            params.filmNegative = aiNegativeParams(settings, params.filmNegative);
            params.crop.enabled = true;
            params.crop.x = 10;
            params.crop.y = 10;
            params.crop.w = 60;
            params.crop.h = 40;
            params.coarse.rotate = 90;
            const auto copy = params;
            std::atomic_bool cancelled{false};
            auto result = makeAiNegativeRenderer(scratch + "/negative.png", false, params)(settings, cancelled);
            check(result.width == 60 && result.height == 40, "Renderer honors oriented crop without upscaling");
            check(params == copy, "Renderer does not mutate processing snapshot");
            check(!result.pngBase64.empty(), "Renderer encodes pixel-only PNG");
            checkPixelOnlyPng(result);
            if (liveOllama) {
                ai_negative::Config config;
                config.model = liveModel;
                auto ai = ai_negative::optimize(config, settings, makeAiNegativeRenderer(scratch + "/negative.png", false, params),
                    cancelled, [](const std::string &progress) { std::cout << progress << std::endl; });
                std::cout << "Live Ollama selected: " << ai.chosen.id << std::endl;
                gsize bytes = 0;
                auto png = g_base64_decode(ai.chosen.preview.pngBase64.c_str(), &bytes);
                std::ofstream file(scratch + "/ollama-result.png", std::ios::binary);
                file.write(reinterpret_cast<const char *>(png), bytes);
                g_free(png);
            }
            auto accepted = aiNegativeParams(settings, params.filmNegative);
            params.filmNegative = accepted;
            params.icm.outputProfile = rtengine::procparams::ColorManagementParams::NoICMString;
            params.resize.enabled = false;
            params.framing.enabled = false;
            int error = 0;
            std::unique_ptr<rtengine::IImagefloat> full(rtengine::processImage(rtengine::ProcessingJob::create(scratch + "/negative.png", false, params), error));
            check(full && !error && full->getWidth() == result.width && full->getHeight() == result.height, "Applied export dimensions match preview");
            for (int y = 0; y < result.height; ++y)
                for (int x = 0; x < result.width; ++x) {
                    auto byte = [](float v) { return static_cast<unsigned char>(std::round(std::max(0.f, std::min(255.f, v / 257.f)))); };
                    check(result.rgb[(y * result.width + x) * 3] == byte(full->r(y, x)) &&
                              result.rgb[(y * result.width + x) * 3 + 1] == byte(full->g(y, x)) &&
                              result.rgb[(y * result.width + x) * 3 + 2] == byte(full->b(y, x)),
                        "Preview matches applied export RGB pixels");
                }
        }
        {
            rtengine::procparams::ProcParams params;
            params.filmNegative = aiNegativeParams(settings, params.filmNegative);
            params.coarse.rotate = 90;
            params.crop.enabled = true;
            params.crop.x = 10;
            params.crop.y = 10;
            params.crop.w = 60;
            params.crop.h = 40;
            std::atomic_bool cancelled{false};
            auto preview = makeAiNegativeRenderer(scratch + "/negative.dng", true, params)(settings, cancelled);
            check(preview.width == 60 && preview.height == 40 && !preview.pngBase64.empty(), "RAW Bayer DNG renders with oriented crop");
            params.crop.enabled = false;
            preview = makeAiNegativeRenderer(scratch + "/negative.dng", true, params)(settings, cancelled);
            check(preview.width < preview.height && preview.height <= 1024, "RAW rotation retained and preview bounded");
        }
        rtengine::cleanup();
        std::cout << checks << " GUI, lifecycle, preferences, profile and renderer assertions passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
