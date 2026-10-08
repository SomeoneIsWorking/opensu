// gog — lists the player's GOG library from GOG's own account API.
//
// iideck holds the sign-in itself (`gog_auth`); a game is listed from the account and is not
// installed until installs are built. GOG's installers and downloads are out of scope here.
#pragma once

#include <vector>

#include "game.hpp"
#include "gog_auth.hpp"

namespace iideck::library::gog {

class Provider final : public library::Provider {
  public:
    explicit Provider(TokenStore store, Endpoints endpoints = {});

    [[nodiscard]] Source source() const override {
        return Source::Gog;
    }

    /// Lists the owned games, a page of GOG's answer per request. Throws NotSignedIn when there
    /// is no usable sign-in, and std::runtime_error when GOG cannot be reached or answers wrongly.
    [[nodiscard]] std::vector<Game> list() override;

  private:
    net::WebClient web_;
    Auth auth_;
};

} // namespace iideck::library::gog
