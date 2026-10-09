// sign_in — the stores' account sign-ins that opensu starts and finishes itself.
//
// The player signs in on the store's own page in their own browser. A browser extension
// (`extension/opensu-signin`) sees the page end on an authorization code and hands it to the
// control channel, which gives it to this. GOG's code becomes a token opensu keeps; Epic's goes
// to Legendary, which keeps the session.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "library/gog_auth.hpp"
#include "net/web_client.hpp"

namespace opensu::app {

enum class Store : std::uint8_t {
    Gog,
    Epic,
};

/// How a sign-in step ended, with words for the player.
struct SignInResult {
    bool ok{false};
    std::string message;
};

/// What the control channel needs from a sign-in, so it can be tested without a browser or a store.
class SignInService {
  public:
    virtual ~SignInService() = default;

    /// Opens the store's sign-in page in the player's browser.
    [[nodiscard]] virtual SignInResult open(Store store) = 0;

    /// Finishes a sign-in with the authorization code the sign-in page ended on.
    [[nodiscard]] virtual SignInResult complete(Store store, std::string_view code) = 0;
};

class StoreSignIn final : public SignInService {
  public:
    struct Options {
        /// Where GOG's token is kept.
        std::filesystem::path dataDir;
        library::gog::Endpoints gog;
        /// Opens a URL in the player's default browser.
        std::string browser{"xdg-open"};
        std::string legendary{"legendary"};
        /// How long a browser opener or Legendary may take.
        std::chrono::milliseconds commandTimeout{std::chrono::seconds{60}};
    };

    explicit StoreSignIn(Options options);

    [[nodiscard]] SignInResult open(Store store) override;
    [[nodiscard]] SignInResult complete(Store store, std::string_view code) override;

  private:
    Options options_;
    net::WebClient web_;
    library::gog::Auth gog_;
};

} // namespace opensu::app
