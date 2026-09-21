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

#include "rtapp.h"

#include "rtengine/procparams.h"
#include "rtengine/settings.h"
#include "rtgui/options.h"
#include "rtgui/version.h"

using namespace rtengine;

const Glib::ustring App::VERSION = RTVERSION;
const Glib::ustring App::VERSIONED_SOFTWARE_NAME = "RawTherapee " RTVERSION;
const Glib::ustring App::CACHE_FOLDER_NAME = CACHEFOLDERNAME;
const Glib::ustring App::PARAM_FILE_EXTENSION = ".pp3";

App& App::get()
{
    static App inst;
    return inst;
}

App::App()
    : m_options(new Options())
    , m_simple_editor(false)
    , m_gimp_plugin(false)
    , m_remote(false)
{
}

const rtengine::Settings& App::settings() const
{
    return m_options->rtSettings;
}

const rtengine::procparams::ColorManagementParams& App::fallbackColorCmp() const
{
    // Constructor accesses App::get().options(), so we can't initialize this
    // in the constructor of App. It would be a recursive init.
    static rtengine::procparams::ColorManagementParams cmp;
    return cmp;
}
