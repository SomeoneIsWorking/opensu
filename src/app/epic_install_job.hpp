// epic_install_job — one Epic install: runs `legendary install` and shows the progress it logs.
#pragma once

#include <string>

#include "install_job.hpp"

namespace iideck::app {

class EpicInstallJob final : public InstallJob {
  public:
    /// Uses the `legendary` on PATH.
    EpicInstallJob();

    /// Uses a specific executable, which the tests point at a stub.
    explicit EpicInstallJob(std::string binary);
    ~EpicInstallJob();

  private:
    void run(const std::stop_token& stop, const std::string& appName) override;

    std::string binary_;
};

} // namespace iideck::app
