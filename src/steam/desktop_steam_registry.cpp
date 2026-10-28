#include "desktop_steam.hpp"

#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace opensu::steam {
namespace {

/// HKCU\Software\Valve\Steam\ActiveProcess's `pid`, which the client sets while it runs; 0 when
/// it is not set.
DWORD registeredPid() {
    DWORD pid = 0;
    DWORD size = sizeof(pid);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"pid",
                     RRF_RT_REG_DWORD, nullptr, &pid, &size) != ERROR_SUCCESS) {
        return 0;
    }
    return pid;
}

/// Whether `pid` is a live steam.exe: a pid Steam left behind may name a process that reused it.
bool liveSteam(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return false;
    }
    DWORD code = 0;
    std::wstring image(MAX_PATH, L'\0');
    auto length = static_cast<DWORD>(image.size());
    const bool alive = GetExitCodeProcess(process, &code) != 0 && code == STILL_ACTIVE &&
                       QueryFullProcessImageNameW(process, 0, image.data(), &length) != 0;
    CloseHandle(process);
    if (!alive) {
        return false;
    }
    image.resize(length);
    constexpr std::wstring_view name = L"\\steam.exe";
    return image.size() >= name.size() &&
           CompareStringOrdinal(image.data() + image.size() - name.size(),
                                static_cast<int>(name.size()), name.data(),
                                static_cast<int>(name.size()), TRUE) == CSTR_EQUAL;
}

} // namespace

DesktopSteam::DesktopSteam(const std::filesystem::path& /*home*/) {
}

std::optional<launch::ProcessId> DesktopSteam::running() const {
    const DWORD pid = registeredPid();
    if (pid == 0 || !liveSteam(pid)) {
        return std::nullopt;
    }
    return static_cast<launch::ProcessId>(pid);
}

/// openSU owns no Steam of its own on Windows, so every running client is outside it.
bool DesktopSteam::runningOutside(const std::string& /*instanceUnit*/) const {
    return running().has_value();
}

} // namespace opensu::steam
