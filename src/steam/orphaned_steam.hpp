// steam — finds and ends the Steam client of an openSU that died without shutting it down.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace opensu::steam {

/// openSU runs Steam in `opensu-<pid>-steam.scope`, which outlives a killed openSU. The scope is
/// an orphan when `<pid>` is no longer a live openSU. A Steam started outside openSU never sits
/// in such a scope, so it is never an orphan.
class OrphanedSteam {
  public:
    explicit OrphanedSteam(const std::filesystem::path& home);

    /// Active scopes named `opensu-<pid>-steam.scope` whose pid is not a live process of this
    /// program, which also excludes the caller's own.
    [[nodiscard]] std::vector<std::string> find() const;

    /// Ends every orphan: `steam -shutdown` when the pid file names a process in it, waiting up to
    /// `wait` for the scope to empty, then stops the scope. Returns how many were ended.
    int stop(const std::filesystem::path& steam, std::chrono::milliseconds wait) const;

  private:
    std::filesystem::path pidFile_;
};

} // namespace opensu::steam
