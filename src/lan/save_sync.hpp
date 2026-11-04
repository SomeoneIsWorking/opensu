// save_sync — the saves of the games installed here shared with each paired openSU the player
// shares saves with (`Peer::sharesSaves`) while it is announced. A game's saves are those of the
// emulator it starts with (`library::saves::launchedSaves`), matched across openSUs by
// `SaveKey`; each side's version is in `SaveVersions`, and `syncStateOf` decides. A newer copy
// there is pulled (`/lan/saves/get`) into a staging folder and copied over the saves here by
// `library::saves::SaveCopier`, which backs them up first, and refused while the emulator runs
// (`Setup::inUse`). A newer copy here is pushed by telling the peer (`/lan/saves/news`), which
// then pulls it. A save changed on both sides is left alone until the player picks one
// (`keep`). Both sides record the hash they synced at (`/lan/saves/synced`).
//
// The shell asks for a sync before a game starts (`prepare`, which it waits for), after it ends
// and from "Sync now" (`sync`), and how its saves stand when its details open (`look`); peers ask
// on the listener's threads. The work runs on one thread of its own, one game at a time, and never
// holds a lock while it waits on a peer.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "dns_sd.hpp"
#include "library/emulator_catalog.hpp"
#include "library/game.hpp"
#include "library/game_saves.hpp"
#include "library/save_copy.hpp"
#include "net/web_client.hpp"
#include "peer_book.hpp"
#include "peer_client.hpp"
#include "save_bundle.hpp"
#include "save_versions.hpp"
#include "wire.hpp"

namespace opensu::lan {

/// How a game's saves here stand with one peer, as the details page shows them.
struct PeerSaves {
    enum class Standing : std::uint8_t {
        InSync,
        /// The peer's are newer and are taken here before the game starts.
        NewerThere,
        /// These are newer; the peer takes them.
        NewerHere,
        /// Both changed since they last synced: the player picks.
        Conflict,
        /// It is not on the LAN now.
        Offline,
        /// It does not share saves with this openSU.
        NotShared,
        /// It could not be asked, or its saves could not be taken (`problem`).
        Failed,
    };
    Peer peer;
    Standing standing{Standing::Offline};
    wire::SaveStatus ours;
    /// While it is announced and answered.
    std::optional<wire::SaveStatus> theirs;
    std::string problem;

    bool operator==(const PeerSaves&) const = default;
};

class SaveSync {
  public:
    struct Setup {
        std::filesystem::path versionsFile;
        /// `SaveCopier`'s root: its backups go under it.
        std::filesystem::path backups;
        /// Where another openSU's saves wait before they are copied in.
        std::filesystem::path staging;
        /// The built-in runner's saves (`Config::savesDir`).
        std::filesystem::path runnerSaves;
        /// Why `saves` of `game` cannot be replaced now ("Close Eden first: it is running"); empty
        /// when they can. Called on the sync's thread.
        std::function<std::string(const library::Game& game,
                                  const library::saves::EmulatorSaves& saves)>
            inUse;
        /// What `standings` gives changed; called on the sync's thread.
        std::function<void()> changed;
    };

    SaveSync(Setup setup, PeerBook& book, const net::WebClient& web,
             std::function<std::vector<Sighting>()> online);
    ~SaveSync();
    SaveSync(const SaveSync&) = delete;
    SaveSync& operator=(const SaveSync&) = delete;
    SaveSync(SaveSync&&) = delete;
    SaveSync& operator=(SaveSync&&) = delete;

    /// Ends the work under way and takes no more; the listener may still ask.
    void stop();

    /// The games installed here and the catalog their emulators' saves are found through.
    void know(const std::vector<library::Game>& games, library::roms::EmulatorCatalog catalog);

    // A peer asks, on the listener's threads.
    /// The version of `key`'s saves here; nothing when no game here has that key.
    [[nodiscard]] std::optional<wire::SaveStatus> statusOf(const library::saves::SaveKey& key);
    /// `key`'s saves here as sent (`write(SaveBundle)`); nothing, with `error`, when there is no
    /// such game or they cannot be read.
    [[nodiscard]] std::optional<std::string> sendable(const library::saves::SaveKey& key,
                                                      std::string& error);
    /// `peer` has newer saves of `ask`'s game, or the player there picked its saves over these.
    void heard(const Peer& peer, const wire::SaveAsk& ask);
    /// `peer` now holds `ask.hash` of the game, taken from here.
    void syncedWith(const Peer& peer, const wire::SaveAsk& ask);

    // The shell asks.
    /// Syncs `game` with every sharing peer before it starts; `prepared` says when that is done.
    [[nodiscard]] std::uint64_t prepare(const library::Game& game);
    [[nodiscard]] bool prepared(std::uint64_t ticket) const;
    /// Syncs `game` with every sharing peer: after it ended, and from "Sync now".
    void sync(const library::Game& game);
    /// Finds how `game`'s saves stand with each sharing peer, for `standings`.
    void look(const library::Game& game);
    /// Whether `game` has saves to share and a peer it shares them with is announced.
    [[nodiscard]] bool sharing(const library::Game& game) const;
    /// The player's pick for `game`'s conflict with `peerId`: its saves (`theirs`) or these.
    void keep(const library::Game& game, const std::string& peerId, bool theirs);
    /// How `game`'s saves stood with each sharing peer when last looked; empty before that or when
    /// no peer shares saves.
    [[nodiscard]] std::vector<PeerSaves> standings(const std::string& gameId) const;
    /// Whether a sync of `gameId` is waiting or under way.
    [[nodiscard]] bool busy(const std::string& gameId) const;

  private:
    /// Whose saves the player picked in a conflict.
    enum class Pick : std::uint8_t { None, Ours, Theirs };
    struct Job {
        library::Game game;
        /// Only this peer; all sharing ones when empty.
        std::string peerId;
        Pick pick{Pick::None};
        /// Only how the saves stand; nothing is taken or told.
        bool lookOnly{false};
        std::uint64_t ticket{0};
    };
    /// The game's saves here: which emulator's, and their bundle.
    struct Local {
        library::saves::SaveKey key;
        std::vector<library::saves::EmulatorSaves> all;
        library::saves::EmulatorSaves launched;
        SaveBundle bundle;
        wire::SaveStatus status;
    };

    void queue(Job job);
    void work(const std::stop_token& stop);
    void run(const Job& job);
    /// One peer's standing, after taking or telling what `job` allows; nothing when the game is
    /// not installed there. Off the work lock.
    [[nodiscard]] std::optional<PeerSaves> syncWith(const Job& job, const Peer& peer,
                                                    const Endpoint& at,
                                                    const std::vector<std::uint8_t>& secret);
    /// `game`'s saves here, read under the work lock; nothing, with `error`, when it has none to
    /// sync.
    [[nodiscard]] std::optional<Local> localOf(const library::Game& game, std::string& error);
    /// Takes `bundle` from `peer` over the saves here. Empty when done, else why not.
    [[nodiscard]] std::string take(const library::Game& game, const Peer& peer,
                                   const SaveBundle& bundle, const wire::SaveStatus& theirs,
                                   const std::string& expected);
    [[nodiscard]] std::optional<library::Game> gameOf(const library::saves::SaveKey& key) const;

    Setup setup_;
    PeerBook& book_;
    const net::WebClient& web_;
    std::function<std::vector<Sighting>()> online_;
    SaveVersions versions_;
    library::saves::SaveCopier copier_;

    mutable std::mutex mutex_;
    std::vector<library::Game> games_;
    library::roms::EmulatorCatalog catalog_;
    std::map<std::string, std::vector<PeerSaves>, std::less<>> standings_;
    std::deque<Job> jobs_;
    std::set<std::string, std::less<>> running_;
    std::uint64_t issued_{0};
    std::set<std::uint64_t> finished_;
    std::condition_variable_any wake_;
    /// Held while the saves here are read or replaced.
    std::mutex work_;
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::lan
