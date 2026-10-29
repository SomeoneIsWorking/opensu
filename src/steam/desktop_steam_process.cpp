#include "desktop_steam.hpp"

#include <filesystem>
#include <string>

#include "launch/process_table.hpp"

namespace opensu::steam {
namespace {

/// The client's executable in `Steam.AppBundle/Steam/Contents/MacOS`.
constexpr std::string_view clientName = "steam_osx";

} // namespace

DesktopSteam::DesktopSteam(const std::filesystem::path& /*home*/) {
}

std::optional<launch::ProcessId> DesktopSteam::running() const {
    for (const launch::ProcessId pid : launch::table::pids()) {
        const std::string line = launch::table::commandLine(pid);
        const std::string program = line.substr(0, line.find('\0'));
        if (std::filesystem::path{program}.filename() == clientName) {
            return pid;
        }
    }
    return std::nullopt;
}

/// openSU owns no Steam of its own on macOS, so every running client is outside it.
bool DesktopSteam::runningOutside(const std::string& /*instanceUnit*/) const {
    return running().has_value();
}

} // namespace opensu::steam
