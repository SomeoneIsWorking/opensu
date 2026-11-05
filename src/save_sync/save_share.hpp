// save_share — where a folder or WebDAV target keeps its files: named blobs under one root, read
// whole and replaced whole (`FolderShare`, `WebDavShare`), each on a stop that ends it. Names are
// `/`-separated and relative.
#pragma once

#include <cstdint>
#include <stop_token>
#include <string>

namespace opensu::save_sync {

class SaveShare {
  public:
    struct Read {
        enum class Kind : std::uint8_t { Found, Missing, Failed, Stopped };
        Kind kind{Kind::Failed};
        /// The bytes when found, else why not.
        std::string bytes;
    };

    SaveShare() = default;
    virtual ~SaveShare() = default;
    SaveShare(const SaveShare&) = delete;
    SaveShare& operator=(const SaveShare&) = delete;
    SaveShare(SaveShare&&) = delete;
    SaveShare& operator=(SaveShare&&) = delete;

    /// Where it is, as the player names it.
    [[nodiscard]] virtual std::string where() const = 0;
    /// Whether it can be reached now: the folder is there, the server is configured.
    [[nodiscard]] virtual bool reachable() const = 0;
    [[nodiscard]] virtual Read read(const std::stop_token& stop, const std::string& name) const = 0;
    /// `name` holds `bytes` afterwards, or as it was when this fails (`error`) or `stop` ends it;
    /// its folder is made.
    [[nodiscard]] virtual bool write(const std::stop_token& stop, const std::string& name,
                                     const std::string& bytes, std::string& error) const = 0;
};

} // namespace opensu::save_sync
