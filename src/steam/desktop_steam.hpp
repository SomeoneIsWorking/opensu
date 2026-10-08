// steam — finds a Steam client that runs outside iideck's instance.
#pragma once

#include <filesystem>
#include <string>

namespace iideck::steam {

/// A launch into a Steam that is already running would hand off to that client
/// and escape iideck's ownership, so such a client has to be found first.
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

} // namespace iideck::steam
