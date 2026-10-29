// steam — finds a Steam client that runs outside opensu's instance. One backend per OS, picked in
// CMakeLists.txt: `~/.steam/steam.pid` and /proc on Linux (`desktop_steam_pid_file.cpp`); the
// registry's ActiveProcess pid on Windows (`desktop_steam_registry.cpp`); a `steam_osx` process on
// macOS (`desktop_steam_process.cpp`).
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "launch/process_id.hpp"

namespace opensu::steam {

/// A launch into a Steam that is already running would hand off to that client
/// and escape opensu's ownership, so such a client has to be found first.
class DesktopSteam {
  public:
    explicit DesktopSteam(const std::filesystem::path& home);

    /// True when `<home>/.steam/steam.pid` names a live process whose cgroup does
    /// not contain `instanceUnit`. An empty unit means no instance runs, so any
    /// live Steam counts. On Windows and macOS any running client counts.
    [[nodiscard]] bool runningOutside(const std::string& instanceUnit) const;

    /// The Steam client running now: the live process `<home>/.steam/steam.pid` (on Windows the
    /// registry's ActiveProcess) names, when it is a `steam` process; on macOS the running
    /// `steam_osx`. Nothing when there is none.
    [[nodiscard]] std::optional<launch::ProcessId> running() const;

  private:
    std::filesystem::path pidFile_;
};

} // namespace opensu::steam
