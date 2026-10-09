// epic_install_job — one Epic install: runs `legendary install` and shows the progress it logs.
#pragma once

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

  private:
    std::vector<std::string> arguments(const std::string& appName) override;
    std::optional<double> progressIn(std::string_view line) const override;
    std::optional<std::string> failureIn(std::string_view line) const override;
};

} // namespace opensu::app
