/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
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

#include <tl/expected.hpp>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rt {

/**
 * A RAII wrapper around platform-specific code for loading dynamic libraries.
 */
class DynamicLib
{
public:
    using ErrorMsg = std::string;

    static tl::expected<DynamicLib, ErrorMsg> load(std::string_view path);

    DynamicLib(const DynamicLib&) = delete;
    DynamicLib& operator=(const DynamicLib&) = delete;

    DynamicLib(DynamicLib&& other) noexcept :
        m_handle(std::exchange(other.m_handle, nullptr))
    {
    }
    DynamicLib& operator=(DynamicLib&& other) noexcept
    {
        if (this != &other) {
            close();
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    ~DynamicLib() { close(); }

    bool isLoaded() const { return m_handle != nullptr; }
    explicit operator bool() const { return isLoaded(); }

    /**
     * Get a raw function pointer by symbol name.
     *
     * DynamicLib must outlive the returned symbol pointer and its uses.
     *
     * @throws std::runtime_error on symbol lookup failure
     */
    tl::expected<void*, ErrorMsg> getSymbol(const std::string& name) const;

    /**
     * Get a typed function pointer by symbol name.
     *
     * DynamicLib must outlive the returned symbol pointer and its uses.
     *
     * Calling with the wrong signature for the symbol will lead to undefined
     * behaviour.
     *
     * @throws std::runtime_error on symbol lookup failure
     */
    template <class Signature>
    tl::expected<Signature*, ErrorMsg> getFunction(const std::string& name) const
    {
        static_assert(std::is_function<Signature>::value,
                      "Signature must be a plain function type, e.g. void(int, int)");
        return getSymbol(name).map(
            [](void* func) { return reinterpret_cast<Signature*>(func); });
    }

private:
    DynamicLib(void* handle) noexcept : m_handle(handle) {}

    void close() noexcept;

    void* m_handle;
};

}  // namespace rt
