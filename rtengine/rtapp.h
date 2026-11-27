/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
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

#include <glibmm/ustring.h>
#include <sigc++/sigc++.h>

#include <memory>

class Options;

namespace rtengine {
class Settings;

namespace procparams {
struct ColorManagementParams;
} // namespace procparams

} // namespace rtengine

class App {
public:
    // This signal is emitted when an options file is loaded, or the user
    // clicks OK in the preferences window.
    using PreferencesChanged = sigc::signal<void()>;

    static const Glib::ustring VERSION;
    static const Glib::ustring PARAM_FILE_EXTENSION;

    static App& get();

    const Options& options() const { return *m_options; }
    Options& mut_options() { return *m_options; }
    const rtengine::Settings& settings() const;

    const rtengine::procparams::ColorManagementParams& fallbackColorCmp() const;

    const Glib::ustring& argv0() const { return m_argv0; }
    const Glib::ustring& argv1() const { return m_argv1; }
    const Glib::ustring& creditsPath() const { return m_credits_path; }
    const Glib::ustring& licensePath() const { return m_license_path; }

    bool isSimpleEditor() const { return m_simple_editor; }
    bool isGimpPlugin() const { return m_gimp_plugin; }
    bool isRemote() const { return m_remote; }

    void setArgv0(const Glib::ustring& val) { m_argv0 = val; }
    void setArgv1(const Glib::ustring& val) { m_argv1 = val; }
    void setCreditsPath(const Glib::ustring& path) { m_credits_path = path; }
    void setLicensePath(const Glib::ustring& path) { m_license_path = path; }

    void setIsSimpleEditor(bool val) { m_simple_editor = val; }
    void setIsGimpPlugin(bool val) { m_gimp_plugin = val; }
    void setIsRemote(bool val) { m_remote = val; }

    PreferencesChanged signal_preferences_changed()
    {
        return m_signal_preferences_changed;
    }

private:
    App();

    Glib::ustring m_argv0;
    Glib::ustring m_argv1;
    Glib::ustring m_credits_path;
    Glib::ustring m_license_path;

    std::unique_ptr<Options> m_options;

    PreferencesChanged m_signal_preferences_changed;

    bool m_simple_editor;
    bool m_gimp_plugin;
    bool m_remote;
};
