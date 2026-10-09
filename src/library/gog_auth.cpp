#include "gog_auth.hpp"

#include <array>
#include <chrono>
#include <cstdio>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::gog {
namespace {

using nlohmann::json;

// GOG Galaxy's client, public in every open GOG client (minigalaxy, gogdl).
constexpr std::string_view clientId = "46899977096215655";
constexpr std::string_view clientSecret =
    "9d85c43b1482497dbbce61f6e4aa173a433796eeae2ca8c5f6129f2dc4de46d9";
// Percent-encoded `https://embed.gog.com/on_login_success?origin=client`.
constexpr std::string_view redirectUri =
    "https%3A%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient";
// A token this close to expiry is refreshed rather than risked mid-request.
constexpr std::int64_t refreshMarginSeconds = 60;

std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// Percent-encodes everything but RFC 3986's unreserved characters.
std::string encode(std::string_view text) {
    std::string out;
    for (const char c : text) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (plain) {
            out.push_back(c);
        } else {
            std::array<char, 4> escape{};
            std::snprintf(escape.data(), escape.size(), "%%%02X", static_cast<unsigned char>(c));
            out.append(escape.data());
        }
    }
    return out;
}

} // namespace

Auth::Auth(TokenStore store, Endpoints endpoints, const net::WebClient& web)
    : store_{std::move(store)}, endpoints_{std::move(endpoints)}, web_{web} {
}

std::string Auth::loginUrl() const {
    return endpoints_.auth + "/auth?client_id=" + std::string{clientId} +
           "&redirect_uri=" + std::string{redirectUri} + "&response_type=code&layout=client2";
}

void Auth::signIn(std::string_view code) const {
    request("grant_type=authorization_code&code=" + encode(code) +
            "&redirect_uri=" + std::string{redirectUri});
}

std::string Auth::accessToken() const {
    std::optional<Token> token = store_.load();
    if (!token) {
        throw NotSignedIn{"GOG is not signed in"};
    }
    if (token->expiresAt - refreshMarginSeconds <= now()) {
        return request("grant_type=refresh_token&refresh_token=" + encode(token->refreshToken))
            .accessToken;
    }
    return token->accessToken;
}

Token Auth::request(const std::string& grant) const {
    const std::string url = endpoints_.auth + "/token?client_id=" + std::string{clientId} +
                            "&client_secret=" + std::string{clientSecret} + "&" + grant;
    std::string error;
    const std::optional<std::string> body = web_.get(url, error);
    if (!body) {
        // GOG answers a spent or wrong code, and a revoked refresh token, with a 4xx.
        throw NotSignedIn{"GOG sign-in failed (" + error + ")"};
    }
    const json granted = json::parse(*body, nullptr, false);
    if (!granted.is_object() || !granted.contains("access_token") ||
        !granted.contains("refresh_token") || !granted.contains("expires_in")) {
        throw NotSignedIn{"GOG's answer holds no token"};
    }
    Token token;
    token.accessToken = granted.value("access_token", "");
    token.refreshToken = granted.value("refresh_token", "");
    token.userId = granted.value("user_id", "");
    token.expiresAt = now() + granted.value("expires_in", std::int64_t{0});
    store_.save(token);
    lucent::info("gog", "token granted");
    return token;
}

} // namespace opensu::library::gog
