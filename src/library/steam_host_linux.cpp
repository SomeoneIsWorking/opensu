#include "steam_host.hpp"

#include <array>
#include <string_view>

namespace opensu::library::steam {
namespace {

/// The standard install locations, in the order Steam itself prefers.
constexpr std::array<std::string_view, 5> knownRoots{
    ".local/share/Steam",
    ".steam/steam",
    ".steam/root",
    ".steam/debian-installation",
    ".var/app/com.valvesoftware.Steam/.local/share/Steam",
};

} // namespace

std::vector<std::filesystem::path> standardRoots(const std::filesystem::path& home) {
    std::vector<std::filesystem::path> roots;
    if (home.empty()) {
        return roots;
    }
    for (const std::string_view rel : knownRoots) {
        roots.push_back(home / rel);
    }
    return roots;
}

/// The trailing NUL ends the argument so AppId=44 cannot match AppId=440.
std::string processHint(const std::string& appId, const std::string& /*installDir*/) {
    return "AppId=" + appId + std::string(1, '\0');
}

} // namespace opensu::library::steam
