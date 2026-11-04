// steam — finds and ends the Steam client of an openSU that died without shutting it down.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "desktop_steam.hpp"

namespace opensu::steam {

/// openSU runs Steam in `<family><pid>-steam.scope`, and Isolated's Secret Service relay in
/// `<family><pid>-steam-bus.scope` (`launch::sessionFamily`: `opensu-<pid>-steam.scope`); both
/// outlive a killed openSU. A scope is an orphan when `<pid>` is no longer a live openSU. A Steam
/// started outside openSU never sits in such a scope, so it is never an orphan, and no other
/// family's scope is ever looked at.
class OrphanedSteam {
  public:
    OrphanedSteam(const std::filesystem::path& home, std::string family);

    /// Active scopes named `<family><pid>-steam.scope` or `<family><pid>-steam-bus.scope` whose
    /// pid is not a live openSU and not the caller; client scopes first.
    [[nodiscard]] std::vector<std::string> find() const;

    /// Ends every orphan: `steam -shutdown` when the running Steam client is in it, waiting up to
    /// `wait` for the scope to empty, then stops the scope. Returns how many were ended.
    int stop(const std::filesystem::path& steam, std::chrono::milliseconds wait) const;

  private:
    DesktopSteam desktop_;
    std::string family_;
};

} // namespace opensu::steam
