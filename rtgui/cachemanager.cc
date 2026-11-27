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

#include <memory>
#include <iostream>

#include <dirent.h>
#include <giomm.h>
#include <glib/gstdio.h>

#ifdef _WIN32
#include <fileapi.h>
#endif

#include "cachemanager.h"

#include "guiutils.h"
#include "options.h"
#include "thumbnail.h"
#include "procparamchangers.h"

namespace
{

constexpr int cacheDirMode = 0777;
constexpr const char* cacheDirs[] = { "profiles", "images", "embprofiles", "data" };

}

CacheManager* CacheManager::getInstance ()
{
    static CacheManager instance;
    return &instance;
}

void CacheManager::init ()
{
    MyMutex::MyLock lock (mutex);

    openEntries.clear ();
    baseDir = App::get().options().cacheBaseDir;

    auto error = g_mkdir_with_parents (baseDir.c_str(), cacheDirMode);

    for (const auto& cacheDir : cacheDirs) {
        error |= g_mkdir_with_parents (Glib::build_filename (baseDir, cacheDir).c_str(), cacheDirMode);
    }

    if (error != 0 && rtengine::settings->verbose) {
        std::cerr << "Failed to create all cache directories: " << g_strerror(errno) << std::endl;
    }
}

Thumbnail* CacheManager::getEntry (const Glib::ustring& fname)
{
    std::unique_ptr<Thumbnail> thumbnail;

    // take manager lock and search for entry,
    // if found return it,
    // else release lock and create it
    {
        MyMutex::MyLock lock (mutex);

        // if it is open, return it
        const auto iterator = openEntries.find (fname);

        if (iterator != openEntries.end ()) {

            auto cachedThumbnail = iterator->second;

            cachedThumbnail->increaseRef ();
            return cachedThumbnail;
        }
    }

    // build path name
    const auto md5 = getMD5 (fname);

    if (md5.empty ()) {
        return nullptr;
    }

    const auto cacheName = getCacheFileName ("data", fname, ".txt", md5);
    const auto xmpSidecarMd5 =
        rtengine::settings->metadata_xmp_sync != rtengine::Settings::MetadataXmpSync::NONE
        ? getMD5(Thumbnail::xmpSidecarPath(fname))
        : "";

    // let's see if we have it in the cache
    {
        CacheImageData imageData;

        const auto error = imageData.load (cacheName);

        if (error == 0 && imageData.supported) {

            if (xmpSidecarMd5 != imageData.xmpSidecarMd5) {
                updateImageInfo(fname, imageData, xmpSidecarMd5);
                imageData.save(cacheName);
            }

            thumbnail.reset (new Thumbnail (this, fname, &imageData));

            if (!thumbnail->isSupported ()) {
                thumbnail.reset ();
            }
        }
    }

    // if not, create a new one
    if (!thumbnail) {

        thumbnail.reset (new Thumbnail (this, fname, md5, xmpSidecarMd5));

        if (!thumbnail->isSupported ()) {
            thumbnail.reset ();
        }
    }

    // retake the lock and see if it was added while we we're unlocked, if it
    // was use it over our version. if not added we create the cache entry
    if (thumbnail) {
        MyMutex::MyLock lock (mutex);

        const auto iterator = openEntries.find (fname);

        if (iterator != openEntries.end ()) {

            auto cachedThumbnail = iterator->second;

            cachedThumbnail->increaseRef ();
            return cachedThumbnail;
        }

        // it wasn't, create a new entry
        openEntries.emplace (fname, thumbnail.get ());
    }

    return thumbnail.release ();
}


void CacheManager::deleteEntry (const Glib::ustring& fname)
{
    MyMutex::MyLock lock (mutex);

    // check if it is opened
    auto iterator = openEntries.find (fname);

    if (iterator == openEntries.end ()) {
        deleteFiles (fname, getMD5 (fname), true, true);
        return;
    }

    auto thumbnail = iterator->second;

    if (thumbnail->decreaseRefCacheMgr () == 0) {
        // If the ref count is not zero, it is open in the editor and the thumbnail can not be deleted.
        openEntries.erase (fname);
        deleteFiles (fname, thumbnail->getMD5 (), true, true);
        delete thumbnail;
    }
}

void CacheManager::clearFromCache (const Glib::ustring& fname, bool purge) const
{
    MyMutex::MyLock lock (mutex);

    auto iterator = openEntries.find (fname);

    if (iterator != openEntries.end ()) {
        deleteFiles (fname, iterator->second->getMD5(), true, true);
        return;
    }

    const auto md5 = getMD5 (fname);
    if (!md5.empty()) {
        deleteFiles (fname, md5, true, purge);
        return;
    }

    if (rtengine::settings->verbose) {
        std::cerr << "clearFromCache failed because MD5 couldn't be resolved for " << fname << std::endl;
    }
}

void CacheManager::renameEntry (const std::string& oldfilename, const std::string& oldmd5, const std::string& newfilename)
{
    MyMutex::MyLock lock (mutex);

    const auto newmd5 = getMD5 (newfilename);

    auto error = g_rename (getCacheFileName ("profiles", oldfilename, App::PARAM_FILE_EXTENSION, oldmd5).c_str (), getCacheFileName ("profiles", newfilename, App::PARAM_FILE_EXTENSION, newmd5).c_str ());
    error |= g_rename (getCacheFileName ("images", oldfilename, ".rtti", oldmd5).c_str (), getCacheFileName ("images", newfilename, ".rtti", newmd5).c_str ());
    error |= g_rename (getCacheFileName ("embprofiles", oldfilename, ".icc", oldmd5).c_str (), getCacheFileName ("embprofiles", newfilename, ".icc", newmd5).c_str ());
    error |= g_rename (getCacheFileName ("data", oldfilename, ".txt", oldmd5).c_str (), getCacheFileName ("data", newfilename, ".txt", newmd5).c_str ());

    if (error != 0 && rtengine::settings->verbose) {
        std::cerr << "Failed to rename all files for cache entry '" << oldfilename << "': " << g_strerror(errno) << std::endl;
    }

    // check if it is opened
    // if it is open, update md5
    const auto iterator = openEntries.find (oldfilename);

    if (iterator == openEntries.end ()) {
        return;
    }

    auto thumbnail = iterator->second;
    openEntries.erase (iterator);
    openEntries.emplace (newfilename, thumbnail);

    thumbnail->setFileName (newfilename);
    thumbnail->updateCache ();
    thumbnail->saveThumbnail ();
}

void CacheManager::closeThumbnail (Thumbnail* thumbnail)
{
    MyMutex::MyLock lock (mutex);

    openEntries.erase (thumbnail->getFileName ());
    delete thumbnail;
}

void CacheManager::closeCache () const
{
    MyMutex::MyLock lock (mutex);

    applyCacheSizeLimitation ();
}

void CacheManager::clearAll () const
{
    MyMutex::MyLock lock (mutex);

    for (const auto& cacheDir : cacheDirs) {
        deleteDir (cacheDir);
    }
}

void CacheManager::clearImages () const
{
    MyMutex::MyLock lock (mutex);

    deleteDir ("data");
    deleteDir ("images");
    deleteDir ("embprofiles");
}

void CacheManager::clearProfiles () const
{
    MyMutex::MyLock lock (mutex);

    deleteDir ("profiles");

}


void CacheManager::deleteDir (const Glib::ustring& dirName) const
{
    try {

        Glib::Dir dir (Glib::build_filename (baseDir, dirName));

        auto error = 0;

        for (auto entry = dir.begin (); entry != dir.end (); ++entry) {
            error |= g_remove (Glib::build_filename (baseDir, dirName, *entry).c_str ());
        }

        if (error != 0 && rtengine::settings->verbose) {
            std::cerr << "Failed to delete all entries in cache directory '" << dirName << "': " << g_strerror(errno) << std::endl;
        }

    } catch (Glib::Error&) {}
}

void CacheManager::deleteFiles (const Glib::ustring& fname, const std::string& md5, bool purgeData, bool purgeProfile) const
{
    if (md5.empty ()) {
        return;
    }

    auto error = g_remove (getCacheFileName ("images", fname, ".rtti", md5).c_str ());
    error |= g_remove (getCacheFileName ("embprofiles", fname, ".icc", md5).c_str ());

    if (purgeData) {
        error |= g_remove (getCacheFileName ("data", fname, ".txt", md5).c_str ());
    }

    if (purgeProfile) {
        error |= g_remove (getCacheFileName ("profiles", fname, App::PARAM_FILE_EXTENSION, md5).c_str ());
    }

    if (error != 0 && rtengine::settings->verbose) {
        std::cerr << "Failed to delete all files for cache entry '" << fname << "': " << g_strerror(errno) << std::endl;
    }
}

std::string CacheManager::getMD5 (const Glib::ustring& fname)
{

#ifdef _WIN32

    std::unique_ptr<wchar_t, GFreeFunc> wfname(reinterpret_cast<wchar_t*>(g_utf8_to_utf16 (fname.c_str (), -1, NULL, NULL, NULL)), g_free);

    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    if (GetFileAttributesExW(wfname.get(), GetFileExInfoStandard, &fileAttr)) {
        // We use name, size and creation time to identify a file.
        const auto identifier = Glib::ustring::compose("%1-%2-%3-%4", fileAttr.nFileSizeLow, fileAttr.ftCreationTime.dwHighDateTime, fileAttr.ftCreationTime.dwLowDateTime, fname);
        return Glib::Checksum::compute_checksum(Glib::Checksum::CHECKSUM_MD5, identifier);
    }

#else

    const auto file = Gio::File::create_for_path(fname);
    if (file) {

        try
        {
            const auto info = file->query_info("standard::*");
            if (info) {
                // We only use name and size to identify a file.
                const auto identifier = Glib::ustring::compose("%1%2", fname, info->get_size());
                return Glib::Checksum::compute_checksum(Glib::Checksum::CHECKSUM_MD5, identifier);
            }

        } catch(Gio::Error&) {}
    }

#endif

    return {};
}

Glib::ustring CacheManager::getCacheFileName (const Glib::ustring& subDir,
        const Glib::ustring& fname,
        const Glib::ustring& fext,
        const Glib::ustring& md5) const
{
    const auto dirName = Glib::build_filename (baseDir, subDir);
    const auto baseName = Glib::path_get_basename (fname) + "." + md5;
    return Glib::build_filename (dirName, baseName + fext);
}

void CacheManager::applyCacheSizeLimitation () const
{
    // first count files without fetching file name and timestamp.
    auto cachedir = opendir(Glib::build_filename(baseDir, "data").c_str());
    if (!cachedir) {
        return;
    }

    std::size_t numFiles = 0;
    while (readdir(cachedir)) {
        ++numFiles;
    }

    closedir(cachedir);
    if (numFiles > 2) {
        numFiles -= 2; // because . and .. are counted
    }

    const auto& options = App::get().options();
    if (numFiles <= options.maxCacheEntries) {
        return;
    }

    using FNameMTime = std::pair<Glib::ustring, Glib::TimeVal>;

    std::vector<FNameMTime> files;
    files.reserve(numFiles);

    constexpr std::size_t md5_size = 32;
    // get filenames and timestamps
    try {
        const auto dir = Gio::File::create_for_path(Glib::build_filename(baseDir, "data"));
        const auto enumerator = dir->enumerate_children("standard::name,time::modified");

        while (const auto file = enumerator->next_file()) {
            const auto name = file->get_name();
            if (name.size() >= md5_size + 5) {
                files.emplace_back(name, file->modification_time());
            }
        }

    } catch (Glib::Exception&) {}

    if (files.size() <= options.maxCacheEntries) {
        // limit not reached
        return;
    }

    const std::size_t toDelete = files.size() - options.maxCacheEntries + options.maxCacheEntries * 5 / 100; // reserve 5% free cache space

    std::nth_element(
        files.begin(),
        files.begin() + toDelete,
        files.end(),
        [](const FNameMTime& lhs, const FNameMTime& rhs) -> bool
        {
            return lhs.second < rhs.second;
        }
    );

    for (std::vector<FNameMTime>::const_iterator entry = files.begin(), end = files.begin() + toDelete; entry != end; ++entry) {
        const auto& name = entry->first;
        const auto name_size = name.size() - md5_size;
        const auto fname = name.substr(0, name_size - 5);
        const auto md5 = name.substr(name_size - 4, md5_size);

        deleteFiles(fname, md5, true, false);
    }
}

void CacheManager::updateImageInfo(const Glib::ustring &fname, CacheImageData &imageData, const Glib::ustring &xmpSidecarMd5) const
{
    Thumbnail::infoFromImage(fname, imageData);
    imageData.xmpSidecarMd5 = xmpSidecarMd5;
}

