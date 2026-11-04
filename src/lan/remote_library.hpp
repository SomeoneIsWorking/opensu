// remote_library — the games of the paired openSUs on the LAN as this one's catalog lists them:
// each a `Source::Remote` game marked with its host and its id there, its cover the one the host
// sent; starting one asks that host to stream it (`app::RemotePlay`). The LAN node fills it as hosts answer; the catalog's
// provider reads it. Safe from any thread.
#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include "library/game.hpp"
#include "peer_book.hpp"
#include "wire.hpp"

namespace opensu::lan {

/// `listed` on `host`, its cover at `cover` (empty for none).
[[nodiscard]] library::Game remoteGame(const Peer& host, const wire::ListedGame& listed,
                                       const std::filesystem::path& cover);

class RemoteLibrary {
  public:
    [[nodiscard]] std::vector<library::Game> games() const;
    /// Lists `games` from now on; true when that changed what is listed.
    bool replace(std::vector<library::Game> games);

  private:
    mutable std::mutex mutex_;
    std::vector<library::Game> games_;
};

/// The catalog's provider of `library`'s games.
[[nodiscard]] std::unique_ptr<library::Provider>
remoteProvider(std::shared_ptr<const RemoteLibrary> library);

} // namespace opensu::lan
