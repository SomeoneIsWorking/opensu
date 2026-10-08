// gogdl_auth — the token in the file format gogdl reads with `--auth-config-path`.
//
// iideck's TokenStore stays the one saved token. For an install the token is handed to gogdl in
// this transient file; whatever gogdl refreshed during a long download is taken back afterwards.
#pragma once

#include <filesystem>
#include <optional>

#include "gog_token.hpp"

namespace iideck::library::gog {

class GogdlAuthFile {
  public:
    explicit GogdlAuthFile(std::filesystem::path file);

    /// Writes `token` for gogdl, owner-only. Throws when it cannot.
    void write(const Token& token, std::int64_t now) const;

    /// The token gogdl holds now; nothing when the file is missing or holds none.
    [[nodiscard]] std::optional<Token> read() const;

    /// Deletes the file.
    void remove() const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
};

} // namespace iideck::library::gog
