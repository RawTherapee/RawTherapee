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

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <set>
#include "cachemanager.h"
#include "filebrowserentry.h"
#include "previewloader.h"
#include "guiutils.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#define DEBUG(format,args...)
//#define DEBUG(format,args...) printf("PreviewLoader::%s: " format "\n", __FUNCTION__, ## args)

class PreviewLoader::Impl :
    public rtengine::NonCopyable
{
public:
    struct Job {
        Job(int dir_id, const Glib::ustring& dir_entry, PreviewLoaderListener* listener):
            dir_id_(dir_id),
            dir_entry_(dir_entry),
            listener_(listener)
        {}

        Job():
            dir_id_(0),
            listener_(nullptr)
        {}

        int dir_id_;
        Glib::ustring dir_entry_;
        PreviewLoaderListener* listener_;
    };

    struct JobCompare {
        bool operator()(const Job& lhs, const Job& rhs) const
        {
            if ( lhs.dir_id_ == rhs.dir_id_ ) {
                return lhs.dir_entry_ < rhs.dir_entry_;
            }

            return lhs.dir_id_ < rhs.dir_id_;
        }
    };

    typedef std::set<Job, JobCompare> JobSet;

    Impl(): 
        nConcurrentThreads(0),
        jobs_removed_(false)
    {
#ifdef _OPENMP
        int threadCount = omp_get_num_procs();
#else
        int threadCount = 2;
#endif

        threadPool_.reset(new Glib::ThreadPool(threadCount, 0));

        if (App::get().options().rtSettings.verbose) {
            printf("PreviewLoader::Impl pool thread count is %d\n", threadCount);
            printf("PreviewLoader::Impl nConcurrentThreads is ");
            printf(nConcurrentThreads.is_lock_free() ? "lock free\n" : "not lock free\n");
        }
    }

    std::unique_ptr<Glib::ThreadPool> threadPool_;
    JobSet jobs_;
    std::atomic<int> nConcurrentThreads;

    // Need to be a std::mutex because used in a std::condition_variable object...
    // This is the only exception besides ThumbImageUpdater and GThreadMutex (guiutils.cc). MyMutex is used everywhere else
    std::mutex mutex_;
    bool jobs_removed_;

    std::condition_variable inactive_;

    void processNextJob()
    {
        Job j;
    
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // nothing to do; could be jobs have been removed
            if ( jobs_.empty() ) {
                DEBUG("processing: nothing to do");
                return;
            }

            // copy and remove front job
            j = *jobs_.begin();
            jobs_.erase(jobs_.begin());
            DEBUG("processing %s", j.dir_entry_.c_str());
            DEBUG("%ld job(s) remaining", jobs_.size());

            nConcurrentThreads++; // to detect when last thread in pool has run out
        }

        // do processing unlocked
        try {
            Thumbnail* tmb = nullptr;

            if (Glib::file_test(j.dir_entry_, Glib::FILE_TEST_EXISTS)) {
                tmb = cacheMgr->getEntry(j.dir_entry_);

                if (tmb) {
                    DEBUG("Preview Ready\n");
                    j.listener_->previewReady(j.dir_id_, new FileBrowserEntry(tmb, j.dir_entry_));
                } else {
                    j.listener_->previewFailed(j.dir_id_, j.dir_entry_, PreviewLoaderListener::FailReason::THUMBNAILFAILED);
                }
            } else {
                j.listener_->previewFailed(j.dir_id_, j.dir_entry_, PreviewLoaderListener::FailReason::FILEDOESNOTEXIST);
            }
        } catch (Glib::Error &e) {} catch(...) {}

        bool notifyListener = false;

        if (--nConcurrentThreads == 0) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!jobs_removed_ && !nConcurrentThreads && jobs_.empty()) {
                // re-check nConcurrentThreads under mutex because it is possible for another
                // thread to race to take the last job from jobs_ and to avoid double call to
                // previewsFinished when that happens
                notifyListener = true;    
            }
            inactive_.notify_all();
        }

        if (notifyListener) {
            DEBUG("Previews Finished\n");
            j.listener_->previewsFinished(j.dir_id_);
        }
    }
};

PreviewLoader::PreviewLoader():
    impl_(new Impl())
{
}

PreviewLoader::~PreviewLoader()
{
    delete impl_;
}

PreviewLoader* PreviewLoader::getInstance()
{
    static PreviewLoader instance_;
    return &instance_;
}

void PreviewLoader::add(int dir_id, const Glib::ustring& dir_entry, PreviewLoaderListener* l)
{
    // somebody listening?
    if ( l != nullptr ) {
        {
            std::lock_guard<std::mutex> lock(impl_->mutex_);

            // create a new job and append to queue
            DEBUG("saving job %s", dir_entry.c_str());
            impl_->jobs_.insert(Impl::Job(dir_id, dir_entry, l));
        }

        // queue a run request
        DEBUG("adding run request %s", dir_entry.c_str());
        impl_->threadPool_->push(sigc::mem_fun(*impl_, &PreviewLoader::Impl::processNextJob));
    }
}

void PreviewLoader::removeAllJobs()
{
    DEBUG("stop %d", impl_->nConcurrentThreads.load());

    std::unique_lock<std::mutex> lock(impl_->mutex_);
    impl_->jobs_.clear();

    if (impl_->nConcurrentThreads.load() != 0) {
        DEBUG("waiting for running jobs2");

        impl_->jobs_removed_ = true;
        impl_->inactive_.wait(lock, [this] { return impl_->nConcurrentThreads.load() == 0; });
        impl_->jobs_removed_ = false;
    }
}


