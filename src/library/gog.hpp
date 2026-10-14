// gog — lists the player's GOG library from GOG's own account API.
//
// opensu holds the sign-in itself (`gog_auth`) and lists the games from the account. A game is
// installed when opensu recorded an install of it (`gog_installs`, made by gogdl through the
// install job), and launches through `gogdl launch`. An installed game has an update when GOG
// offers a build other than the installed one (`gog_builds`), asked once per installed game per
// listing.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "game.hpp"
#include "gog_auth.hpp"
#include "gog_installs.hpp"

namespace opensu::library::gog {

/// What the provider needs besides the sign-in.
struct Setup {
    Endpoints endpoints;
    Paths paths;
    /// The gogdl executable a launch runs.
    std::string gogdl{"gogdl"};
    /// The Wine a Windows install runs under.
    std::string wine{"wine"};
};

class Provider final : public library::Provider {
  public:
    Provider(TokenStore store, Setup setup);

    [[nodiscard]] Source source() const override {
        return Source::Gog;
    }

    /// Lists the owned games, a page of GOG's answer per request. Throws NotSignedIn when there
    /// is no usable sign-in, and std::runtime_error when GOG cannot be reached or answers wrongly.
    [[nodiscard]] std::vector<Game> list() override;

  private:
    /// Marks `game` installed and says how it launches, when opensu installed it.
    void applyInstall(Game& game,
                      const std::map<std::string, Installed, std::less<>>& installs) const;
    /// Marks installed `game` updatable when GOG offers another build; one that cannot be asked
    /// is logged and left without.
    void checkUpdate(Game& game, const Installed& install,
                     const std::vector<std::string>& headers) const;

    net::WebClient web_;
    Auth auth_;
    Setup setup_;
};

} // namespace opensu::library::gog
