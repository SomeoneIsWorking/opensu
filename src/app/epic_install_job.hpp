// epic_install_job — one Epic install or update: runs `legendary install`, or `legendary update`
// for an installed game, and shows the progress it logs.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "cli_install_job.hpp"

namespace opensu::app {

class EpicInstallJob final : public CliInstallJob {
  public:
    /// Uses the `legendary` on PATH.
    EpicInstallJob();

    /// Uses a specific executable, which the tests point at a stub.
    explicit EpicInstallJob(std::string binary);
    ~EpicInstallJob();

    /// The folder the next install goes under (legendary's `--base-path`); none leaves legendary's
    /// own. Call before start(), while idle.
    void setFolder(std::optional<std::filesystem::path> folder) {
        folder_ = std::move(folder);
    }

    /// Whether the next start updates the installed game rather than installing it. Call before
    /// start(), while idle.
    void setUpdate(bool update) noexcept {
        update_ = update;
    }

  private:
    std::vector<std::string> arguments(const std::string& appName) override;
    std::optional<double> progressIn(std::string_view line) const override;
    std::optional<std::string> failureIn(std::string_view line) const override;

    std::optional<std::filesystem::path> folder_;
    bool update_{false};
};

} // namespace opensu::app
