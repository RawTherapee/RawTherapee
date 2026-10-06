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

#include "dynlib.h"

#ifdef _WIN32
#include "leanwindows.h"
#else
#include <dlfcn.h>
#endif

#include <fmt/format.h>

namespace {

#ifdef _WIN32
std::string getLastError()
{
    DWORD err = GetLastError();
    if (err == 0) return "";

    char* buf = nullptr;
    size_t size =
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                           | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<LPSTR>(&buf), 0, nullptr);

    if (size == 0) return fmt::format("Windows error {}", err);

    std::string msg(buf, size);
    LocalFree(buf);

    return msg;
}
#endif

}  // namespace

namespace rt {

tl::expected<DynamicLib, DynamicLib::ErrorMsg> DynamicLib::load(std::string_view path)
{
    void* handle = nullptr;

#ifdef _WIN32
    const int len = MultiByteToWideChar(CP_UTF8, 0, path.data(), -1, nullptr, 0);
    if (len == 0) {
        return tl::unexpected(
            fmt::format("MultiByteToWideChar failed: {}", getLastError()));
    }

    std::wstring wstr(len, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, path.data(), -1, wstr.data(), len) == 0) {
        return tl::unexpected(
            fmt::format("MultiByteToWideChar failed: {}", getLastError()));
    }

    handle = reinterpret_cast<void*>(LoadLibraryW(wstr.c_str()));
    if (!handle) {
        return tl::unexpected(fmt::format("LoadLibrary failed: {}", getLastError()));
    }
#else
    handle = dlopen(path.data(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return tl::unexpected(fmt::format("dlopen failed: {}", dlerror()));
    }
#endif

    return DynamicLib(handle);
}

tl::expected<void*, DynamicLib::ErrorMsg>
DynamicLib::getSymbol(const std::string& name) const
{
    if (!m_handle) {
        return tl::unexpected("Cannot look up a symbol in an unloaded dynamic library");
    }

#ifdef _WIN32
    auto h = reinterpret_cast<HMODULE>(m_handle);
    void* sym = reinterpret_cast<void*>(GetProcAddress(h, name.c_str()));
    if (!sym) {
        return tl::unexpected(
            fmt::format("GetProcAddress failed for '{}': {}", name, getLastError()));
    }
#else
    dlerror();  // Clear previous errors
    void* sym = dlsym(m_handle, name.c_str());
    const char* err = dlerror();
    if (err) {
        return tl::unexpected(fmt::format("dlsym failed for '{}': {}", name, err));
    }
#endif

    return sym;
}

void DynamicLib::close() noexcept
{
    if (m_handle) {
#ifdef _WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#else
        dlclose(m_handle);
#endif
        m_handle = nullptr;
    }
}

}  // namespace rt
