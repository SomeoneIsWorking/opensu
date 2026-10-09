// steam — finds a Steam client that runs outside opensu's instance.
#pragma once

#include <filesystem>
#include <string>

namespace opensu::steam {

/// A launch into a Steam that is already running would hand off to that client
/// and escape opensu's ownership, so such a client has to be found first.
class DesktopSteam {
  public:
    explicit DesktopSteam(const std::filesystem::path& home);

    /// True when `<home>/.steam/steam.pid` names a live process whose cgroup does
    /// not contain `instanceUnit`. An empty unit means no instance runs, so any
    /// live Steam counts.
    [[nodiscard]] bool runningOutside(const std::string& instanceUnit) const;

  private:
    std::filesystem::path pidFile_;
};

} // namespace opensu::steam
