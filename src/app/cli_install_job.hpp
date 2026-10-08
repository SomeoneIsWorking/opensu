// cli_install_job — an install done by a store's downloader program (legendary, gogdl): run it,
// turn the progress it logs into reports, and say why it failed. A store's job supplies the
// command line and reads the downloader's output.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "install_job.hpp"

namespace iideck::app {

class CliInstallJob : public InstallJob {
  protected:
    /// `program` is the downloader's executable; `tool` its name in messages to the player;
    /// `logTag` names the store in the log.
    CliInstallJob(std::string program, std::string tool, std::string logTag);
    ~CliInstallJob() = default;

    /// The downloader's arguments for installing `appId`. Runs on the job's thread before the
    /// program starts; a std::runtime_error fails the install with its message.
    virtual std::vector<std::string> arguments(const std::string& appId) = 0;

    /// How far the install is, from one line of the downloader's output.
    virtual std::optional<double> progressIn(std::string_view line) const = 0;

    /// Why the install failed, from one line of the downloader's output.
    virtual std::optional<std::string> failureIn(std::string_view line) const = 0;

    /// Called once the program is over, or was stopped, with whether it installed the game; a
    /// store takes its own records here. A returned text fails the install.
    virtual std::optional<std::string> ended(const std::string& appId, bool installed);

  private:
    void run(const std::stop_token& stop, const std::string& appId) final;

    std::string program_;
    std::string tool_;
    std::string logTag_;
};

} // namespace iideck::app
