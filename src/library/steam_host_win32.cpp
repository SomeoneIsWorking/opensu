#include "steam_host.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace opensu::library::steam {
namespace {

/// Where the Steam installer puts the client unless told otherwise.
constexpr const wchar_t* defaultRoot = L"C:\\Program Files (x86)\\Steam";

/// HKCU\Software\Valve\Steam's `SteamPath`, which the client writes on every start.
std::filesystem::path registeredRoot() {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ,
                     nullptr, nullptr, &size) != ERROR_SUCCESS ||
        size < sizeof(wchar_t)) {
        return {};
    }
    std::wstring path(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ,
                     nullptr, path.data(), &size) != ERROR_SUCCESS) {
        return {};
    }
    path.resize(size / sizeof(wchar_t) - 1);
    return std::filesystem::path{path}.make_preferred();
}

} // namespace

std::vector<std::filesystem::path> standardRoots(const std::filesystem::path& /*home*/) {
    std::vector<std::filesystem::path> roots;
    if (std::filesystem::path registered = registeredRoot(); !registered.empty()) {
        roots.push_back(std::move(registered));
    }
    roots.emplace_back(defaultRoot);
    return roots;
}

/// The game's executable sits in its install folder; the separators around the name keep "Portal"
/// from matching "Portal 2".
std::string processHint(const std::string& /*appId*/, const std::string& installDir) {
    if (installDir.empty()) {
        return {};
    }
    return "\\common\\" + installDir + "\\";
}

} // namespace opensu::library::steam
