// fumarole_install_job — one Steam install through fumarole, for a game it lists as owned: runs
// `fumarole install <appid> --os <os> --json` and shows the progress it prints. The build is this
// OS's own where fumarole installs it (macOS) unless the player picked Windows, else the Windows
// one, run under Wine (`fumaroleInstallOs`).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli_install_job.hpp"
#include "library/game.hpp"

namespace opensu::app {

class FumaroleInstallJob final : public CliInstallJob {
  public:
    /// `program` is fumarole's executable.
    explicit FumaroleInstallJob(std::string program);
    ~FumaroleInstallJob();

    /// The folder the next install goes in (`--path`), where any chunk already on disk is kept;
    /// none leaves fumarole's own. Call before start(), while idle.
    void setFolder(std::optional<std::filesystem::path> folder) {
        folder_ = std::move(folder);
    }

    /// The next install's game builds, and whether the player picked its Windows one. Call before
    /// start(), while idle.
    void setBuilds(const library::Game::Builds& builds, bool windowsPicked) {
        builds_ = builds;
        windowsPicked_ = windowsPicked;
    }

  private:
    std::vector<std::string> arguments(const std::stop_token& stop,
                                       const std::string& appId) override;
    std::optional<double> progressIn(std::string_view line) const override;
    std::optional<std::string> failureIn(std::string_view line) const override;

    std::optional<std::filesystem::path> folder_;
    library::Game::Builds builds_;
    bool windowsPicked_{false};
};

} // namespace opensu::app
