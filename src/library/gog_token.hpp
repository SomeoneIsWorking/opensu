// gog_token — the GOG sign-in iideck keeps between runs.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace iideck::library::gog {

/// What GOG's token endpoint granted.
struct Token {
    std::string accessToken;
    std::string refreshToken;
    std::string userId;
    /// Unix time after which the access token is no longer accepted.
    std::int64_t expiresAt{0};
};

/// The token as one JSON file, owner-only, replaced atomically so a reader never sees half of one.
class TokenStore {
  public:
    explicit TokenStore(std::filesystem::path file);

    /// The store in iideck's data directory, which sign-in and the library both open.
    [[nodiscard]] static TokenStore under(const std::filesystem::path& dataDir);

    /// The saved token; nothing when none was saved yet. Throws when the file is unreadable or
    /// not a token.
    [[nodiscard]] std::optional<Token> load() const;

    /// Replaces the saved token. Throws when it cannot be written.
    void save(const Token& token) const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
};

} // namespace iideck::library::gog
