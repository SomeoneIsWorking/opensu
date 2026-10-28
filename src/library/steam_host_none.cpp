#include "steam_host.hpp"

namespace opensu::library::steam {

std::vector<std::filesystem::path> standardRoots(const std::filesystem::path& /*home*/) {
    return {};
}

std::string processHint(const std::string& /*appId*/, const std::string& /*installDir*/) {
    return {};
}

} // namespace opensu::library::steam
