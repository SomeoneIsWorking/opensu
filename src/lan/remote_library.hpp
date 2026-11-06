// remote_library — the games of the paired openSUs on the LAN as this one's catalog lists them:
// each a `Source::Remote` game marked with its host and its id there, its cover the one the host
// sent; starting one asks that host to stream it (`app::RemotePlay`). The LAN node fills it as
// hosts answer; the catalog's provider reads it. A paired host that does not answer keeps its last
// listing, offline, kept in a file across starts. Safe from any thread.
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
    RemoteLibrary() = default;
    /// Keeps its listing in `file`, read now with every host offline.
    explicit RemoteLibrary(std::filesystem::path file);

    [[nodiscard]] std::vector<library::Game> games() const;
    /// Lists `games` from now on, and nothing else; true when that changed what is listed.
    bool replace(std::vector<library::Game> games);
    /// `online` are the games of the hosts that answered; the other `paired` hosts' stay listed as
    /// offline. True when that changed what is listed.
    bool update(std::vector<library::Game> online, const std::vector<Peer>& paired);

  private:
    void save() const;

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    std::vector<library::Game> games_;
};

/// The catalog's provider of `library`'s games.
[[nodiscard]] std::unique_ptr<library::Provider>
remoteProvider(std::shared_ptr<const RemoteLibrary> library);

} // namespace opensu::lan
