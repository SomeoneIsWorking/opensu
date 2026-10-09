#include "gogdl_auth.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

namespace opensu::library::gog {
namespace {

using nlohmann::json;

// GOG Galaxy's client id, the key gogdl files its credentials under (gogdl/auth.py CLIENT_ID).
constexpr const char* clientId = "46899977096215655";

} // namespace

GogdlAuthFile::GogdlAuthFile(std::filesystem::path file) : file_{std::move(file)} {
}

void GogdlAuthFile::write(const Token& token, std::int64_t now) const {
    // gogdl treats the token as expired at loginTime + expires_in.
    const json credentials{{"access_token", token.accessToken},
                           {"refresh_token", token.refreshToken},
                           {"user_id", token.userId},
                           {"loginTime", now},
                           {"expires_in", std::max<std::int64_t>(token.expiresAt - now, 0)}};
    writeOwnerOnly(file_, json{{clientId, credentials}}.dump());
}

std::optional<Token> GogdlAuthFile::read() const {
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object() || !document.contains(clientId)) {
        return std::nullopt;
    }
    const json& credentials = document[clientId];
    if (!credentials.is_object() || !credentials.contains("access_token") ||
        !credentials.contains("refresh_token")) {
        return std::nullopt;
    }
    Token token;
    token.accessToken = credentials.value("access_token", "");
    token.refreshToken = credentials.value("refresh_token", "");
    token.userId = credentials.value("user_id", "");
    token.expiresAt = static_cast<std::int64_t>(credentials.value("loginTime", 0.0)) +
                      credentials.value("expires_in", std::int64_t{0});
    return token;
}

void GogdlAuthFile::remove() const {
    std::error_code ec;
    std::filesystem::remove(file_, ec);
}

} // namespace opensu::library::gog
