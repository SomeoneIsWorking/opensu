// gog — lists the player's GOG library from GOG's own account API.
//
// iideck holds the sign-in itself (`gog_auth`) and lists the games from the account. A game is
// installed when iideck recorded an install of it (`gog_installs`, made by gogdl through the
// install job), and launches through `gogdl launch`.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "game.hpp"
#include "gog_auth.hpp"
#include "gog_installs.hpp"

namespace iideck::library::gog {

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
    /// Marks `game` installed and says how it launches, when iideck installed it.
    void applyInstall(Game& game,
                      const std::map<std::string, Installed, std::less<>>& installs) const;

    net::WebClient web_;
    Auth auth_;
    Setup setup_;
};

} // namespace iideck::library::gog
