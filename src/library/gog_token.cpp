#include "gog_token.hpp"

#include <fstream>
#include <stdexcept>
#include <system_error>

#include <nlohmann/json.hpp>

#include "fileio/owner_only_write.hpp"

namespace opensu::library::gog {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

} // namespace

TokenStore::TokenStore(fs::path file) : file_{std::move(file)} {
}

TokenStore TokenStore::under(const fs::path& dataDir) {
    return TokenStore{dataDir / "gog-token.json"};
}

std::optional<Token> TokenStore::load() const {
    std::error_code ec;
    if (!fs::exists(file_, ec)) {
        return std::nullopt;
    }
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object() || !document.contains("access_token") ||
        !document.contains("refresh_token") || !document.contains("expires_at")) {
        throw std::runtime_error{"the saved GOG sign-in is not a token"};
    }
    Token token;
    token.accessToken = document.value("access_token", "");
    token.refreshToken = document.value("refresh_token", "");
    token.userId = document.value("user_id", "");
    token.expiresAt = document.value("expires_at", std::int64_t{0});
    return token;
}

void writeOwnerOnly(const fs::path& file, const std::string& bytes) {
    std::string error;
    if (!fileio::writeOwnerOnly(file, bytes, error)) {
        throw std::runtime_error{"cannot save the GOG sign-in: " + error};
    }
}

void TokenStore::save(const Token& token) const {
    const json document{{"access_token", token.accessToken},
                        {"refresh_token", token.refreshToken},
                        {"user_id", token.userId},
                        {"expires_at", token.expiresAt}};
    writeOwnerOnly(file_, document.dump());
}

} // namespace opensu::library::gog
