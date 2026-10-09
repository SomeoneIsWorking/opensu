// gog_auth — GOG's OAuth sign-in as every open GOG client does it.
//
// GOG offers no third-party registration, so this uses the client id and secret of GOG Galaxy's
// own client, as minigalaxy and gogdl do. The sign-in page is GOG's; the player signs in there in
// their own browser, and the authorization code it ends on is exchanged here for a token.
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "gog_token.hpp"
#include "net/web_client.hpp"

namespace opensu::library::gog {

/// The GOG hosts, which a test points at a local server.
struct Endpoints {
    std::string auth{"https://auth.gog.com"};
    std::string embed{"https://embed.gog.com"};
};

/// Thrown when no token is saved or the saved one can no longer be refreshed.
class NotSignedIn : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class Auth {
  public:
    Auth(TokenStore store, Endpoints endpoints, const net::WebClient& web);

    /// The page the player signs in on; it ends on `embed.gog.com/on_login_success?...&code=`.
    [[nodiscard]] std::string loginUrl() const;

    /// Exchanges an authorization code for a token and saves it. Throws when GOG refuses the code.
    void signIn(std::string_view code) const;

    /// A valid access token, refreshed and saved first when the saved one has expired. Throws
    /// NotSignedIn when there is none to use.
    [[nodiscard]] std::string accessToken() const;

    [[nodiscard]] const Endpoints& endpoints() const noexcept {
        return endpoints_;
    }

  private:
    /// Asks the token endpoint with `grant` ("grant_type=..."), saves what it grants and returns
    /// it.
    Token request(const std::string& grant) const;

    TokenStore store_;
    Endpoints endpoints_;
    const net::WebClient& web_;
};

} // namespace opensu::library::gog
