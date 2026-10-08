// gog_install_job — one GOG install: runs `gogdl download` and shows the progress it logs.
//
// Authentication: iideck's TokenStore is the one saved token. Before the download the token is
// refreshed through `gog::Auth` (iideck's own refresh) and handed to gogdl in a transient
// owner-only file; whatever gogdl refreshed meanwhile is saved back and the file deleted. The
// download takes the Linux build when the library listing says GOG has one, else the Windows
// build, else the install fails.
#pragma once

#include <string>

#include "cli_install_job.hpp"
#include "library/game.hpp"
#include "library/gog_auth.hpp"
#include "library/gog_installs.hpp"
#include "library/gogdl_auth.hpp"
#include "net/web_client.hpp"

namespace iideck::app {

class GogInstallJob final : public CliInstallJob {
  public:
    struct Options {
        /// iideck's data directory, where the token, the install records and the games are.
        std::filesystem::path dataDir;
        /// The gogdl executable; the tests point it at a stub.
        std::string gogdl{"gogdl"};
        /// GOG's hosts, which the tests point at a local server.
        library::gog::Endpoints endpoints{};
    };

    explicit GogInstallJob(Options options);
    ~GogInstallJob();

    /// The builds GOG lists for the game about to be started. Call before start(), while idle.
    void setBuilds(const library::Game::Builds& builds) {
        builds_ = builds;
    }

  private:
    std::vector<std::string> arguments(const std::string& gameId) override;
    std::optional<double> progressIn(std::string_view line) const override;
    std::optional<std::string> failureIn(std::string_view line) const override;
    std::optional<std::string> ended(const std::string& gameId, bool installed) override;

    library::gog::Paths paths_;
    library::gog::TokenStore store_;
    net::WebClient web_;
    library::gog::Auth auth_;
    library::gog::GogdlAuthFile authFile_;
    library::gog::InstallRecords records_;
    /// The token handed to gogdl, to tell whether it refreshed it. Job thread only.
    std::string handedRefreshToken_;
    /// The builds the listing gave for the install being started. Set while idle.
    library::Game::Builds builds_;
    /// The platform being downloaded. Job thread only.
    std::string platform_;
};

} // namespace iideck::app
