/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ainegativegui.h"
#include "multilangmgr.h"
#include "rtengine/rtengine.h"
#include "rtengine/iimage.h"
#include "rtengine/colortemp.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace
{
Glib::RefPtr<Gdk::Pixbuf> pixbuf(const ai_negative::Preview &preview)
{
    if (preview.width < 1 || preview.height < 1 || preview.width > 1024 || preview.height > 1024 ||
        preview.rgb.size() != static_cast<std::size_t>(preview.width * preview.height * 3))
        throw ai_negative::Error("AI_ERROR_PREVIEW_INVALID");
    auto result = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, preview.width, preview.height);
    for (int y = 0; y < preview.height; ++y)
        std::memcpy(result->get_pixels() + y * result->get_rowstride(), preview.rgb.data() + y * preview.width * 3, preview.width * 3);
    return result;
}
void message(Gtk::Window *parent, const std::string &text)
{
    Gtk::MessageDialog dialog(text, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
    if (parent) dialog.set_transient_for(*parent);
    dialog.run();
}
} // namespace

std::string aiNegativeErrorKey(const std::exception &error)
{
    return dynamic_cast<const ai_negative::Error *>(&error) ? error.what() : "AI_ERROR_UNEXPECTED";
}

AiNegativePreferences::AiNegativePreferences(ai_negative::Transport transport) : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8), test(M("AI_TEST_CONNECTION")), transport(std::move(transport))
{
    set_border_width(12);
    provider.append("ollama", "Ollama");
    provider.append("openai", "OpenAI");
    provider.append("compatible", M("AI_COMPATIBLE"));
    timeout.set_range(1, 600);
    timeout.set_increments(1, 30);
    timeout.set_digits(0);
    model.set_placeholder_text(M("AI_MODEL_PLACEHOLDER"));
    credential.set_placeholder_text("OPENAI_API_KEY");
    auto grid = Gtk::manage(new Gtk::Grid());
    grid->set_row_spacing(8);
    grid->set_column_spacing(12);
    auto row = [&](int y, const char *label, Gtk::Widget &widget) {
        auto l = Gtk::manage(new Gtk::Label(M(label)));
        l->set_halign(Gtk::ALIGN_START);
        grid->attach(*l, 0, y, 1, 1);
        widget.set_hexpand(true);
        grid->attach(widget, 1, y, 1, 1);
    };
    row(0, "AI_PROVIDER", provider);
    row(1, "AI_ENDPOINT", endpoint);
    row(2, "AI_VISION_MODEL", model);
    row(3, "AI_CREDENTIAL_ENV", credential);
    row(4, "AI_TIMEOUT", timeout);
    pack_start(*grid, Gtk::PACK_SHRINK);
    auto disclosure = Gtk::manage(new Gtk::Label(M("AI_PRIVACY")));
    disclosure->set_line_wrap(true);
    disclosure->set_max_width_chars(65);
    disclosure->set_xalign(0);
    pack_start(*disclosure, Gtk::PACK_SHRINK);
    pack_start(test, Gtk::PACK_SHRINK);
    status.set_line_wrap(true);
    status.set_max_width_chars(65);
    pack_start(status, Gtk::PACK_SHRINK);
    provider.signal_changed().connect([this] {
        const auto id = provider.get_active_id();
        if (id == "ollama") {
            endpoint.set_text("http://localhost:11434");
            credential.set_text("");
        } else if (id == "openai") {
            endpoint.set_text("https://api.openai.com/v1");
            credential.set_text("OPENAI_API_KEY");
        } else {
            endpoint.set_text("");
            credential.set_text("");
        }
        model.set_text("");
    });
    test.signal_clicked().connect(sigc::mem_fun(*this, &AiNegativePreferences::startTest));
}
AiNegativePreferences::~AiNegativePreferences()
{
    poll.disconnect();
    cancelled = true;
    if (worker.joinable()) worker.join();
}
void AiNegativePreferences::read(const ai_negative::Config &c)
{
    provider.set_active_id(c.provider);
    endpoint.set_text(c.endpoint);
    model.set_text(c.model);
    credential.set_text(c.credentialEnv);
    timeout.set_value(c.timeoutSeconds);
}
ai_negative::Config AiNegativePreferences::write() const
{
    return {provider.get_active_id(), endpoint.get_text(), model.get_text(), credential.get_text(), timeout.get_value_as_int()};
}
void AiNegativePreferences::validateForSave() const
{
    auto config = write();
    // An unused feature may retain the default empty model. Validate the other
    // fields before Options is updated, especially the credential variable name.
    if (config.model.empty()) config.model = "unconfigured";
    ai_negative::validateConfig(config);
}
void AiNegativePreferences::startTest()
{
    if (worker.joinable()) {
        // The button still means Cancel until the completion callback has run,
        // even if the request finished between the user's click and this slot.
        cancelled = true;
        return;
    }
    poll.disconnect();
    const auto c = write();
    try {
        ai_negative::validateConfig(c);
    } catch (const std::exception &e) {
        status.set_text(M(aiNegativeErrorKey(e)));
        return;
    }
    cancelled = false;
    done = false;
    status.set_text(M("AI_TESTING"));
    test.set_label(M("GENERAL_CANCEL"));
    provider.set_sensitive(false);
    endpoint.set_sensitive(false);
    model.set_sensitive(false);
    credential.set_sensitive(false);
    timeout.set_sensitive(false);
    worker = std::thread([this, c] {
        try {
            ai_negative::testConnection(c, cancelled, this->transport);
            testResult = "AI_TEST_SUCCESS";
        } catch (const std::exception &e) {
            testResult = aiNegativeErrorKey(e);
        }
        done = true;
    });
    poll = Glib::signal_timeout().connect([this] {
        if (!done.load()) return true;
        worker.join();
        status.set_text(M(cancelled.load() ? "AI_ERROR_CANCELLED" : testResult));
        test.set_label(M("AI_TEST_CONNECTION"));
        provider.set_sensitive(true);
        endpoint.set_sensitive(true);
        model.set_sensitive(true);
        credential.set_sensitive(true);
        timeout.set_sensitive(true);
        return false;
    },
        100);
}
std::optional<ai_negative::Settings> makeAiNegativeCropSeed(
    const rtengine::procparams::CropParams &crop, const ai_negative::Settings &baseline,
    const std::function<bool(int, int, int, std::array<double, 3> &)> &sample)
{
    if (!crop.enabled || crop.w < 12 || crop.h < 12 || crop.x < 0 || crop.y < 0) return std::nullopt;
    std::array<std::vector<double>, 3> channels;
    const int spot = std::min({8, crop.w / 10, crop.h / 10});
    // Interior samples avoid the holder and film borders. The spot sampler maps
    // crop coordinates through the active rotation, flips and lens corrections.
    for (int row = 1; row <= 5; ++row)
        for (int col = 1; col <= 5; ++col) {
            std::array<double, 3> rgb;
            if (!sample(crop.x + col * crop.w / 6, crop.y + row * crop.h / 6, spot, rgb)) continue;
            if (std::any_of(rgb.begin(), rgb.end(), [](double v) { return !std::isfinite(v) || v < 1 || v > 65535; })) continue;
            for (int c = 0; c < 3; ++c)
                channels[c].push_back(rgb[c]);
        }
    if (channels[0].size() < 5) return std::nullopt;
    auto result = baseline;
    for (int c = 0; c < 3; ++c) {
        auto &values = channels[c];
        std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
        result.refInput[c] = static_cast<float>(values[values.size() / 2]);
    }
    // A neutral midtone is a useful candidate, not an edit to the current image.
    result.outputLevel = 7.53;
    result.blueBalance = result.greenBalance = 0;
    ai_negative::validateSettings(result);
    return result;
}
rtengine::procparams::FilmNegativeParams aiNegativeParams(const ai_negative::Settings &s,
    rtengine::procparams::FilmNegativeParams p)
{
    ai_negative::validateSettings(s);
    p.enabled = true;
    p.backCompat = rtengine::procparams::FilmNegativeParams::BackCompat::CURRENT;
    p.greenExp = s.greenExp;
    p.redRatio = s.redRatio;
    p.blueRatio = s.blueRatio;
    p.refInput = {static_cast<float>(s.refInput[0]), static_cast<float>(s.refInput[1]), static_cast<float>(s.refInput[2])};
    const rtengine::ColorTemp neutral(1., 1., 1., 1., rtengine::ColorTemp::DEFAULT_OBSERVER);
    const rtengine::ColorTemp color(neutral.getTemp() / std::pow(2., s.blueBalance),
        neutral.getGreen() / std::pow(2., s.greenBalance), 1., "Custom", rtengine::ColorTemp::DEFAULT_OBSERVER);
    double r, g, b;
    color.getMultipliers(r, g, b);
    const double scale = std::pow(2., s.outputLevel + 6.) / std::max({r, g, b});
    p.refOutput = {static_cast<float>(r * scale), static_cast<float>(g * scale), static_cast<float>(b * scale)};
    return p;
}
ai_negative::Renderer makeAiNegativeRenderer(const Glib::ustring &filename, bool raw,
    const rtengine::procparams::ProcParams &snapshot)
{
    return [filename, raw, snapshot](const ai_negative::Settings &s, std::atomic_bool &cancelled) {
        if (cancelled.load()) throw ai_negative::Error("AI_ERROR_CANCELLED");
        auto params = snapshot;
        params.filmNegative = aiNegativeParams(s, params.filmNegative);
        params.resize.enabled = true;
        params.resize.dataspec = 3;
        params.resize.width = params.resize.height = 1024;
        params.resize.allowUpscaling = false;
        params.resize.appliesTo = "Cropped area";
        params.resize.method = "Lanczos";
        params.framing.enabled = false;
        params.icm.outputProfile = rtengine::procparams::ColorManagementParams::NoICMString;
        int error = 0;
        // A file-based job loads a separate ImageSource, avoiding races with the editor's engine.
        std::unique_ptr<rtengine::IImagefloat> image(rtengine::processImage(
            rtengine::ProcessingJob::create(filename, raw, params, false), error));
        if (cancelled.load()) throw ai_negative::Error("AI_ERROR_CANCELLED");
        if (!image || error) throw ai_negative::Error("AI_ERROR_RENDER");
        ai_negative::Preview out;
        out.width = image->getWidth();
        out.height = image->getHeight();
        if (out.width < 1 || out.height < 1 || out.width > 1024 || out.height > 1024)
            throw ai_negative::Error("AI_ERROR_PREVIEW_SIZE");
        out.rgb.resize(out.width * out.height * 3);
        auto channel = [](float value) -> unsigned char {
            if (!std::isfinite(value)) return 0;
            return static_cast<unsigned char>(std::round(std::max(0.f, std::min(255.f, value / 257.f))));
        };
        for (int y = 0; y < out.height; ++y)
            for (int x = 0; x < out.width; ++x) {
                auto p = &out.rgb[(y * out.width + x) * 3];
                p[0] = channel(image->r(y, x));
                p[1] = channel(image->g(y, x));
                p[2] = channel(image->b(y, x));
            }
        // Encode only copied pixels; no EXIF, filenames or source profile metadata are attached.
        auto pixels = pixbuf(out);
        gchar *buffer = nullptr;
        gsize size = 0;
        pixels->save_to_buffer(buffer, size, "png");
        std::unique_ptr<gchar, decltype(&g_free)> owned(buffer, g_free);
        gchar *encoded = g_base64_encode(reinterpret_cast<const guchar *>(buffer), size);
        out.pngBase64 = encoded;
        g_free(encoded);
        return out;
    };
}
bool runAiNegative(Gtk::Window *parent, const ai_negative::Config config,
    const ai_negative::Settings &baseline, ai_negative::Renderer render,
    std::function<bool()> stillCurrent, ai_negative::Settings &selected, ai_negative::Transport transport, std::optional<ai_negative::Settings> cropSeed)
{
    try {
        ai_negative::validateConfig(config);
        ai_negative::validateSettings(baseline);
    } catch (const std::exception &e) {
        message(parent, M(aiNegativeErrorKey(e)));
        return false;
    }
    Gtk::Dialog dialog(M("AI_NEGATIVE_TITLE"), true);
    if (parent) dialog.set_transient_for(*parent);
    dialog.set_default_size(900, 500);
    Gtk::Label disclosure(config.provider + " / " + config.model + "\n" + M("AI_PRIVACY"));
    disclosure.set_line_wrap(true);
    disclosure.set_max_width_chars(95);
    Gtk::Label status(M("AI_RENDERING"));
    status.set_line_wrap(true);
    status.set_max_width_chars(95);
    Gtk::Box images(Gtk::ORIENTATION_HORIZONTAL, 8);
    Gtk::Box beforeBox(Gtk::ORIENTATION_VERTICAL, 4), afterBox(Gtk::ORIENTATION_VERTICAL, 4);
    Gtk::Label beforeLabel(M("AI_BEFORE")), afterLabel(M("AI_AFTER"));
    Gtk::Image before, after;
    beforeBox.pack_start(beforeLabel, Gtk::PACK_SHRINK);
    beforeBox.pack_start(before);
    afterBox.pack_start(afterLabel, Gtk::PACK_SHRINK);
    afterBox.pack_start(after);
    images.pack_start(beforeBox);
    images.pack_start(afterBox);
    auto box = dialog.get_content_area();
    box->set_border_width(12);
    box->set_spacing(8);
    box->pack_start(disclosure, Gtk::PACK_SHRINK);
    box->pack_start(status, Gtk::PACK_SHRINK);
    box->pack_start(images);
    auto apply = dialog.add_button(M("GENERAL_APPLY"), Gtk::RESPONSE_APPLY);
    apply->set_sensitive(false);
    Gtk::Button cancel(M("GENERAL_CANCEL"));
    dialog.get_action_area()->pack_start(cancel);
    std::atomic_bool cancelled{false}, done{false};
    std::mutex mutex;
    std::string progress, error;
    ai_negative::Result result;
    std::thread worker([&] {
        try {
            result = ai_negative::optimize(config, baseline, render, cancelled, [&](const std::string &p) {
                std::lock_guard<std::mutex> lock(mutex); progress = p; }, transport, cropSeed);
        } catch (const std::exception &e) {
            error = aiNegativeErrorKey(e);
        }
        done = true;
    });
    bool displayed = false, stale = false;
    auto requestCancel = [&] {
        cancelled = true;
        apply->set_sensitive(false);
        status.set_text(M("AI_CANCELLING"));
        if (done.load()) dialog.response(Gtk::RESPONSE_CANCEL);
    };
    cancel.signal_clicked().connect(requestCancel);
    // Keep the main loop alive while an in-flight engine render finishes safely.
    auto deletion = dialog.signal_delete_event().connect([&](GdkEventAny *) { requestCancel(); return true; }, false);
    auto key = dialog.signal_key_press_event().connect([&](GdkEventKey *event) {
        if (event->keyval == GDK_KEY_Escape) {
            requestCancel();
            return true;
        }
        return false;
    },
        false);
    auto poll = Glib::signal_timeout().connect([&] {
        if (!stillCurrent()) {
            stale = true;
            requestCancel();
        }
        if (!done.load()) {
            if (!cancelled.load()) {
                std::lock_guard<std::mutex> lock(mutex);
                if (!progress.empty()) status.set_text(M(progress));
            }
            return true;
        }
        if (cancelled.load()) {
            dialog.response(Gtk::RESPONSE_CANCEL);
            return false;
        }
        if (!displayed) {
            displayed = true;
            if (!error.empty()) status.set_text(M(error));
            else {
                try {
                    auto display = [](const ai_negative::Preview &p) {
                        const double scale = std::min(420. / p.width, 420. / p.height);
                        return pixbuf(p)->scale_simple(std::max(1, static_cast<int>(p.width * scale)),
                            std::max(1, static_cast<int>(p.height * scale)), Gdk::INTERP_BILINEAR);
                    };
                    before.set(display(result.baseline.preview));
                    after.set(display(result.chosen.preview));
                    status.set_text(result.chosen.id == "baseline" ? M("AI_BASELINE_BEST") : M("AI_READY"));
                    apply->set_sensitive(result.chosen.id != "baseline");
                } catch (const std::exception &e) {
                    error = aiNegativeErrorKey(e);
                    status.set_text(M(error));
                }
            }
        }
        return true;
    },
        100);
    dialog.show_all_children();
    const auto response = dialog.run();
    cancelled = true;
    poll.disconnect();
    deletion.disconnect();
    key.disconnect();
    worker.join();
    if (stale) return false;
    if (response != Gtk::RESPONSE_APPLY || !error.empty() || !stillCurrent()) return false;
    selected = result.chosen.settings;
    return true;
}
