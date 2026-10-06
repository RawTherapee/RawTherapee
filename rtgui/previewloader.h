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
#pragma once

#include <set>

#include "rtengine/noncopyable.h"

namespace Glib
{

class ustring;

}

class FileBrowserEntry;

class PreviewLoaderListener
{
public:
    /**
     * @brief Reason the PreviewLoader failed to generate thumbnail
     */
    enum class FailReason{
        FILEDOESNOTEXIST,   ///< File was not found
        THUMBNAILFAILED     ///< Generating thumbnail from the file failed.
    };

    virtual ~PreviewLoaderListener() = default;

    /**
     * @brief a preview is ready
     *
     * @param dir_id directory ID this is for
     * @param fd entry
     */
    virtual void previewReady(int dir_id, FileBrowserEntry* fd) = 0;

    /**
     * @brief all previews have finished loading
     */
    virtual void previewsFinished(int dir_id_) = 0;

    /**
     * @brief creating the preview failed
     * 
     * @param dir_id directory ID this is for
     * @param file name of the failed file, including the path
     * @param reason a more specific explanation for the failure
     */
    virtual void previewFailed(int dir_id, Glib::ustring file, FailReason reason) = 0;
};

class PreviewLoader :
    public rtengine::NonCopyable
{
public:
    /**
     * @brief Singleton entry point.
     *
     * @note expects to be called inside gtk thread lock
     *
     * @return Pointer to thumbnail image updater.
     */
    static PreviewLoader* getInstance(void);

    /**
     * @brief Add an thumbnail image update request.
     *
     * Code will add the request to the queue and, if needed, start a pool
     * thread to process it.
     *
     * @param dir_id directory we're looking at
     * @param dir_entry entry in it
     * @param l listener
     */
    void add(int dir_id, const Glib::ustring& dir_entry, PreviewLoaderListener* l);

    /**
     * @brief Stop processing and remove all jobs.
     *
     * Will not return till all jobs have completed.
     *
     * @note expects to be called inside gtk thread lock
     */
    void removeAllJobs(void);

private:

    PreviewLoader();
    ~PreviewLoader();

    class Impl;
    Impl* impl_;
};

/**
 * @brief Singleton boiler plate.
 *
 * To use: \c previewLoader->start() ,
 */
#define previewLoader PreviewLoader::getInstance()
