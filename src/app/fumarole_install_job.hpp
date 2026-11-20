// fumarole_install_job — one Steam install through fumarole, for a game it lists as owned: runs
// `fumarole install <appid> --os windows --json` and shows the progress it prints. The Windows
// build, as fumarole launches only those (under Wine).
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli_install_job.hpp"

namespace opensu::app {

class FumaroleInstallJob final : public CliInstallJob {
  public:
    /// `program` is fumarole's executable.
    explicit FumaroleInstallJob(std::string program);
    ~FumaroleInstallJob();

  private:
    std::vector<std::string> arguments(const std::stop_token& stop,
                                       const std::string& appId) override;
    std::optional<double> progressIn(std::string_view line) const override;
    std::optional<std::string> failureIn(std::string_view line) const override;
};

} // namespace opensu::app
