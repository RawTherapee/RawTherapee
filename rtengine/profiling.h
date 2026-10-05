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

#include <cstdint>
#include <string_view>

#ifdef RT_TRACING_ENABLE

#ifndef TRACY_ENABLE
#error "Missing TRACY_ENABLE"
#endif

#include <tracy/Tracy.hpp>

#define RT_DEFAULT_ZONE_NAME ___tracy_scoped_zone // Matches tracy/Tracy.hpp

// Mark the end of a frame
#define RT_MARK_FRAME_TICK() FrameMark

#define RT_IS_ACTIVE_ZONE(TAG) Profiling::ZONE_FILTER & Profiling::Tag:: TAG

// Auto-generates zone name based on current function
#define RT_PROFILE_FUNC(TAG) ZoneNamed(RT_DEFAULT_ZONE_NAME, RT_IS_ACTIVE_ZONE(TAG))

// Name must be a constexpr string literal
#define RT_PROFILE(NAME, TAG) ZoneNamedN(RT_DEFAULT_ZONE_NAME, NAME, RT_IS_ACTIVE_ZONE(TAG))
#define RT_PROFILE_COLORED(NAME, COLOR, TAG) ZoneNamedNC(RT_DEFAULT_ZONE_NAME, NAME, COLOR, RT_IS_ACTIVE_ZONE(TAG))

#define RT_PROFILE_TEXT(TEXT) ZoneText(TEXT, std::string_view(TEXT).size())
#define RT_PROFILE_UINT64(NUM) ZoneValue(NUM)

#define RT_PROFILE_IS_ACTIVE() RT_DEFAULT_ZONE_NAME.IsActive()

#else // RT_TRACING_ENABLE not defined

#define RT_MARK_FRAME_TICK()
#define RT_PROFILE_FUNC(TAG)
#define RT_PROFILE(NAME, TAG)
#define RT_PROFILE_COLORED(NAME, COLOR, TAG)
#define RT_PROFILE_TEXT(TEXT)
#define RT_PROFILE_UINT64(NUM)
#define RT_PROFILE_IS_ACTIVE() false

#endif // RT_TRACING_ENABLE

// clang-format off
struct Profiling {
    enum Tag : std::uint64_t {
        // Binary flags for each profiling tag. Keep the flags one-hot.
        // The current size only allows for 64 different tags.
        PROCESS_IMAGE         = 0x1,
        GUI_EDITOR            = 0x2,
        GUI_THUMBNAIL_BROWSER = 0x4,

        // Enable all GUI-related profiling zones
        GUI_ALL = GUI_EDITOR | GUI_THUMBNAIL_BROWSER,

        // Enable all profiling zones
        ALL = ~(std::uint64_t{0})
    };

    // To profile only specific tags, modify ZONE_FILTER. For example, setting
    // ZONE_FILTER = GUI_EDITOR | GUI_THUMBNAIL_BROWSER will only trace zones
    // in the GUI_EDITOR and GUI_THUMBNAIL_BROWSER categories.
    static constexpr Tag ZONE_FILTER = ALL;

    // Vibrant X11 colors for use with known problematic zones
    enum Color {
#ifdef TRACY_ENABLE
        IDLE   = tracy::Color::DimGray,
        WAIT   = tracy::Color::Crimson,
        SLEEP  = tracy::Color::Tomato,
        LOCK   = tracy::Color::Coral,
        MEMORY = tracy::Color::Goldenrod,
        SYSTEM = tracy::Color::Gold,
        BIG_O  = tracy::Color::LimeGreen,
        IO     = tracy::Color::Blue,
        FICKLE = tracy::Color::Magenta,
#else
        IDLE   = 0x696969,
        WAIT   = 0xdc143c,
        SLEEP  = 0xff6347,
        LOCK   = 0xff7f50,
        MEMORY = 0xdaa520,
        SYSTEM = 0xffd700,
        BIG_O  = 0x32cd32,
        IO     = 0x0000ff,
        FICKLE = 0xff00ff,
#endif // TRACY_ENABLE
    };
};
// clang-format on

inline bool isProfilerConnected()
{
#ifdef TRACY_ENABLE
    return TracyIsConnected;
#else
    return false;
#endif
}
