#include "sign_in.hpp"

#include <exception>
#include <vector>

#include "launch/command.hpp"
#include "library/gog_token.hpp"
#include "lucent/log.h"

namespace iideck::app {
namespace {

// Legendary's own sign-in page; it redirects to Epic's login with Legendary's client, and the
// page that follows shows the authorization code as JSON.
constexpr std::string_view epicLoginUrl = "https://legendary.gl/epiclogin";

std::string_view name(Store store) {
    return store == Store::Gog ? "GOG" : "Epic";
}

} // namespace

StoreSignIn::StoreSignIn(Options options)
    : options_{std::move(options)},
      gog_{library::gog::TokenStore::under(options_.dataDir), options_.gog, web_} {
}

SignInResult StoreSignIn::open(Store store) {
    const std::string url = store == Store::Gog ? gog_.loginUrl() : std::string{epicLoginUrl};
    // xdg-open resolves the default browser, Flatpak ones included, as the desktop does.
    const std::optional<int> status =
        launch::runCommand(options_.browser, {url}, options_.commandTimeout);
    if (!status || *status != 0) {
        lucent::warn("signin", "{}: the browser did not open", name(store));
        return {false, "cannot open a browser with " + options_.browser};
    }
    lucent::info("signin", "{}: sign-in page opened", name(store));
    return {true, std::string{name(store)} + " sign-in opened in the browser"};
}

SignInResult StoreSignIn::complete(Store store, std::string_view code) {
    if (store == Store::Gog) {
        try {
            gog_.signIn(code);
        } catch (const std::exception& error) {
            lucent::warn("signin", "GOG: {}", error.what());
            return {false, error.what()};
        }
        return {true, "GOG signed in"};
    }
    // Legendary exits 0 even when Epic refuses the code; the catalog read that follows shows
    // whether the session took.
    const std::optional<int> status = launch::runCommand(
        options_.legendary, {"auth", "--code", std::string{code}}, options_.commandTimeout);
    if (!status || *status != 0) {
        lucent::warn("signin", "Epic: legendary auth failed");
        return {false, "legendary could not sign in"};
    }
    lucent::info("signin", "Epic: legendary auth finished");
    return {true, "Epic code given to legendary"};
}

} // namespace iideck::app
