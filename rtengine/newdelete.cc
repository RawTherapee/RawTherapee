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

#include "newdelete.h"

#ifdef WITH_TRACY_MEMORY_PROFILING

#ifndef TRACY_ENABLE
#error "Missing TRACY_ENABLE"
#endif

#include <tracy/Tracy.hpp>

#ifndef RT_USING_TCMALLOC

#include <cstdlib>

void* operator new(size_t count)
{
    auto ptr = std::malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

namespace rtengine {

// NOOP when tcmalloc is disabled
void initTcmallocHooks() {}

}  // rtengine

#else  // defined(RT_USING_TCMALLOC)

#include <gperftools/malloc_hook.h>

namespace {

void tracyNewHook(const void* ptr, size_t size) {
    TracyAlloc(ptr, size);
}

void tracyDeleteHook(const void* ptr) {
    TracyFree(ptr);
}

}  // namespace

namespace rtengine {

// Use tcmalloc hooks since tcmalloc already overrides new/delete
void initTcmallocHooks()
{
    if (MallocHook::AddNewHook(&tracyNewHook)) {
        TracyMessageL("Added Tracy hook to tcmalloc new");
        if (!MallocHook::AddDeleteHook(&tracyDeleteHook)) {
            MallocHook::RemoveNewHook(&tracyNewHook);
            TracyMessageL("Failed to add Tracy hook to tcmalloc delete");
        } else {
            TracyMessageL("Added Tracy hook to tcmalloc delete");
        }
    } else {
        TracyMessageL("Failed to add Tracy hook to tcmalloc new");
    }
}

}  // rtengine

#endif  // RT_USING_TCMALLOC

#else  // !defined(WITH_TRACY_MEMORY_PROFILING)

namespace rtengine {

// NOOP when tcmalloc is diabled or Tracy memory profiling is disabled
void initTcmallocHooks() {}

}  // namespace rtengine

#endif  // WITH_TRACY_MEMORY_PROFILING
