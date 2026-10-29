#include "steam_host.hpp"

namespace opensu::library::steam {

/// The one place Steam for macOS keeps its client files and the default library.
std::vector<std::filesystem::path> standardRoots(const std::filesystem::path& home) {
    if (home.empty()) {
        return {};
    }
    return {home / "Library" / "Application Support" / "Steam"};
}

/// The game's executable sits in its install folder, often inside an `.app`; the separators
/// around the name keep "Portal" from matching "Portal 2".
std::string processHint(const std::string& /*appId*/, const std::string& installDir) {
    if (installDir.empty()) {
        return {};
    }
    return "/common/" + installDir + "/";
}

} // namespace opensu::library::steam
