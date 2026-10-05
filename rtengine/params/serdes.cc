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

#include "serdes.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/keyfile.h>

Glib::ustring expandRelativePath(const Glib::ustring &procparams_fname, const Glib::ustring &prefix, Glib::ustring embedded_fname)
{
    if (embedded_fname.empty() || !Glib::path_is_absolute(procparams_fname)) {
        return embedded_fname;
    }

    if (!prefix.empty()) {
        if (embedded_fname.length() < prefix.length() || embedded_fname.substr(0, prefix.length()) != prefix) {
            return embedded_fname;
        }

        embedded_fname = embedded_fname.substr(prefix.length());
    }

    if (Glib::path_is_absolute(embedded_fname)) {
        return prefix + embedded_fname;
    }

    Glib::ustring absPath = prefix + Glib::path_get_dirname(procparams_fname) + G_DIR_SEPARATOR_S + embedded_fname;
    return absPath;
}

Glib::ustring expandRelativePath2(const Glib::ustring &procparams_fname, const Glib::ustring &procparams_fname2, const Glib::ustring &prefix, Glib::ustring embedded_fname)
{
#if defined (_WIN32)
    // if this is Windows, replace any "/" in the filename with "\\"
    size_t pos = embedded_fname.find("/");
    while (pos != Glib::ustring::npos) {
        embedded_fname.replace(pos, 1, "\\");
        pos = embedded_fname.find("/", pos);
    }
#endif
#if !defined (_WIN32)
    // if this is not Windows, replace any "\\" in the filename with "/"
    size_t pos = embedded_fname.find("\\");
    while (pos != Glib::ustring::npos) {
        embedded_fname.replace(pos, 1, "/");
        pos = embedded_fname.find("\\", pos);
    }
#endif

    // if embedded_fname is not already an absolute path,
    // try to convert it using procparams_fname (the directory of the raw file) as prefix
    Glib::ustring rPath = expandRelativePath(procparams_fname, prefix, embedded_fname);
    if (rPath.length() >= prefix.length()
        && !Glib::file_test(rPath.substr(prefix.length()), Glib::FILE_TEST_IS_REGULAR)
        && !procparams_fname2.empty()
        && Glib::path_is_absolute(procparams_fname2)) {
        // embedded_fname is not a valid path;
        // try with procparams_fname2 (the path defined in Preferences) as a prefix
        rPath = expandRelativePath(procparams_fname2 + G_DIR_SEPARATOR_S, prefix, embedded_fname);
    }
    return(rPath);
}


Glib::ustring relativePathIfInside(const Glib::ustring &procparams_fname, bool fnameAbsolute, Glib::ustring embedded_fname)
{
    if (fnameAbsolute || embedded_fname.empty() || !Glib::path_is_absolute(procparams_fname)) {
        return embedded_fname;
    }

    Glib::ustring prefix;

    if (embedded_fname.length() > 5 && embedded_fname.substr(0, 5) == "file:") {
        embedded_fname = embedded_fname.substr(5);
        prefix = "file:";
    }

    if (!Glib::path_is_absolute(embedded_fname)) {
        return prefix + embedded_fname;
    }

    Glib::ustring dir1 = Glib::path_get_dirname(procparams_fname) + G_DIR_SEPARATOR_S;
    Glib::ustring dir2 = Glib::path_get_dirname(embedded_fname) + G_DIR_SEPARATOR_S;

    if (dir2.substr(0, dir1.length()) != dir1) {
        // it's in a different directory, ie not inside
        return prefix + embedded_fname;
    }

    return prefix + embedded_fname.substr(dir1.length());
}

Glib::ustring relativePathIfInside2(const Glib::ustring &procparams_fname, const Glib::ustring &procparams_fname2, bool fnameAbsolute, Glib::ustring embedded_fname)
{
    // try to convert embedded_fname to a path relative to procparams_fname
    // (the directory of the raw file)
    // (note: fnameAbsolute seems to be always true, so this will never return a relative path)
    Glib::ustring rPath = relativePathIfInside(procparams_fname, fnameAbsolute, embedded_fname);
    if ((Glib::path_is_absolute(rPath)
            || (rPath.length() >= 5 && rPath.substr(0, 5) == "file:"
                && Glib::path_is_absolute(rPath.substr(5))))
        && !procparams_fname2.empty()
        && Glib::path_is_absolute(procparams_fname2)) {
        // if path is not relative to the directory of the raw file,
        // try to convert embedded_fname to a path relative to procparams_fname2
        // (the path defined in Preferences)
        rPath = relativePathIfInside(procparams_fname2 + G_DIR_SEPARATOR_S, false, embedded_fname);
    }
    return(rPath);
}
