/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ainegative.h"
#include "rtengine/procparams.h"
#include <gtkmm.h>
#include <thread>
#include <mutex>

class AiNegativePreferences final : public Gtk::Box
{
public:
    explicit AiNegativePreferences(ai_negative::Transport transport = ai_negative::httpTransport);
    ~AiNegativePreferences() override;
    void read(const ai_negative::Config &config);
    ai_negative::Config write() const;
    void validateForSave() const;

private:
    Gtk::ComboBoxText provider;
    Gtk::Entry endpoint, model, credential;
    Gtk::SpinButton timeout;
    Gtk::Button test;
    Gtk::Label status;
    sigc::connection poll;
    std::thread worker;
    std::atomic_bool cancelled{false}, done{false};
    std::string testResult;
    ai_negative::Transport transport;
    void startTest();
};

std::string aiNegativeErrorKey(const std::exception &error);

// Reuse the editor's transformed linear spot sampler, never rendered JPEG RGB.
std::optional<ai_negative::Settings> makeAiNegativeCropSeed(
    const rtengine::procparams::CropParams &crop, const ai_negative::Settings &baseline,
    const std::function<bool(int, int, int, std::array<double, 3> &)> &sample);

// Snapshot and filename are copied; no editor, GTK widget or live ImageSource is used by the worker.
ai_negative::Renderer makeAiNegativeRenderer(const Glib::ustring &filename, bool raw,
    const rtengine::procparams::ProcParams &snapshot);
rtengine::procparams::FilmNegativeParams aiNegativeParams(const ai_negative::Settings &settings,
    rtengine::procparams::FilmNegativeParams base);
// Modal comparison, with cancellable asynchronous rendering/network work.
bool runAiNegative(Gtk::Window *parent, ai_negative::Config config,
    const ai_negative::Settings &baseline, ai_negative::Renderer render,
    std::function<bool()> stillCurrent, ai_negative::Settings &selected,
    ai_negative::Transport transport = ai_negative::httpTransport,
    std::optional<ai_negative::Settings> cropSeed = std::nullopt);
