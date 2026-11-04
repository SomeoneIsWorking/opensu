// lan_node — this openSU on the LAN while the player has LAN on: its book of paired peers, its
// listener, its DNS-SD announcement and browsing, the pairing it hosts and the one it asks for,
// the saves it shares (`SaveSync`), and a worker that keeps the paired peers' games in
// `RemoteLibrary` while they are announced (again when one comes or goes, a pairing changes, and
// every minute), their covers cached under `coverCache`; a paired peer's request to play a game
// here goes to `play`, and this one asks a host to play one of its games (`askToPlay`). A pairing
// is one secret both sides keep, so each lists the other's games. Made on the shell's loop and
// asked from it; its own threads only fill what it reports.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "dns_sd.hpp"
#include "guest_pairing.hpp"
#include "host_pairing.hpp"
#include "lan_listener.hpp"
#include "net/web_client.hpp"
#include "peer_book.hpp"
#include "peer_client.hpp"
#include "remote_library.hpp"
#include "save_sync.hpp"
#include "secrets/secret_store.hpp"
#include "shared_library.hpp"

namespace opensu::lan {

class LanNode {
  public:
    struct Setup {
        std::filesystem::path bookFile;
        secrets::SecretStore secrets;
        std::filesystem::path coverCache;
        std::string name;
        /// The listener's port; 0 for a free one.
        std::uint16_t port{0};
        std::shared_ptr<RemoteLibrary> remote;
        /// What `remote` lists changed, or a peer unpaired itself; called on a thread of the
        /// node's.
        std::function<void()> changed;
        /// Where saves shared with paired peers are recorded, staged and backed up, and when they
        /// are in use (`SaveSync::Setup`; its `changed` is called on a thread of the node's).
        SaveSync::Setup saves;
        /// A paired peer asks to play a listed game here (`LanListener::Sources::play`); empty
        /// refuses every such request.
        decltype(LanListener::Sources::play) play;
        /// Announces this openSU and finds the others.
        std::function<std::unique_ptr<ServiceDiscovery>(const Announcement&, std::string&)>
            discovery{startDiscovery};
    };

    /// Starts listening, announcing and browsing; throws `std::runtime_error` when the book
    /// cannot be kept. A listener or announcement that cannot start is reported by `status`.
    explicit LanNode(Setup setup);
    ~LanNode();
    LanNode(const LanNode&) = delete;
    LanNode& operator=(const LanNode&) = delete;
    LanNode(LanNode&&) = delete;
    LanNode& operator=(LanNode&&) = delete;

    struct Status {
        /// The name it is announced under.
        std::string name;
        std::uint16_t port{0};
        /// Why it cannot be found or reached; empty when it can.
        std::string problem;
    };
    [[nodiscard]] Status status() const;
    [[nodiscard]] const std::string& id() const noexcept {
        return book_.id();
    }
    [[nodiscard]] std::vector<Sighting> sightings() const;
    [[nodiscard]] std::vector<Peer> paired() const {
        return book_.paired();
    }
    /// Shares the installed games among `games` with paired peers from now on, and their saves
    /// with those the player shares saves with, found through `catalog`.
    void share(const std::vector<library::Game>& games,
               const library::roms::EmulatorCatalog& catalog) {
        library_.share(games);
        saves_.know(games, catalog);
    }
    /// The saves shared with paired peers.
    [[nodiscard]] SaveSync& saves() noexcept {
        return saves_;
    }
    /// Shares saves with `peerId` from now on, or no longer. False with `error` when that could
    /// not be kept.
    [[nodiscard]] bool shareSaves(const std::string& peerId, bool on, std::string& error) {
        return book_.shareSaves(peerId, on, error);
    }

    /// The pairing this one hosts that waits for the player.
    [[nodiscard]] std::optional<HostPairing::Question> question() const {
        return pairing_.question(HostPairing::Clock::now());
    }
    /// The player's answer to the question about `guestId`. False with `error` when an accepted
    /// pairing could not be kept, which the guest is then told was declined.
    [[nodiscard]] bool answer(const std::string& guestId, bool accept, std::string& error);

    /// Starts pairing with the openSU announced as `peerId`. False with `refusal` when a pairing
    /// is under way or it is no longer announced.
    [[nodiscard]] bool pair(const std::string& peerId, std::string& refusal);
    /// The pairing this one asks for, while it is kept.
    [[nodiscard]] std::optional<GuestPairing::State> asking() const;
    /// Stops the pairing this one asks for and lets it go.
    void endAsking();

    /// What a host agreed to stream: where its stream takes packets and the session's keys.
    struct PlayOffer {
        Endpoint stream;
        StreamKeys keys;
    };
    /// Asks the paired, announced openSU `hostId` to start its game `gameId` (its own id) and
    /// stream it here. Blocks for the call, so never on the loop. Nothing with `error` when it is
    /// gone, unpaired or refuses.
    [[nodiscard]] std::optional<PlayOffer> askToPlay(const std::string& hostId,
                                                     const std::string& gameId,
                                                     std::string& error) const;

    /// Forgets `peerId` here, and tells it to forget this one when it is announced. False with
    /// `error` when it could not be forgotten here.
    [[nodiscard]] bool forget(const std::string& peerId, std::string& error);

  private:
    /// An unpairing to tell a peer of, off the loop.
    struct Notice {
        Endpoint endpoint;
        crypto::Bytes secret;
    };

    void wake();
    void work(const std::stop_token& stop);
    /// Every announced paired peer's games, their covers fetched as needed and those of peers no
    /// longer paired dropped; `complete` false when a peer's could not be listed.
    [[nodiscard]] std::vector<library::Game> fetch(const std::vector<Sighting>& online,
                                                   bool& complete);
    [[nodiscard]] std::filesystem::path coverOf(const Peer& host, const PeerClient& client,
                                                const crypto::Bytes& secret,
                                                const std::string& token) const;
    void tell(const Notice& notice) const;

    net::WebClient web_;
    std::filesystem::path coverCache_;
    std::shared_ptr<RemoteLibrary> remote_;
    std::function<void()> changed_;
    PeerBook book_;
    std::string name_;
    HostPairing pairing_{book_.id()};
    SharedLibrary library_;
    SaveSync saves_;
    LanListener listener_;
    std::string problem_;
    std::unique_ptr<ServiceDiscovery> discovery_;
    /// Loop only.
    std::unique_ptr<GuestPairing> asking_;
    std::mutex mutex_;
    std::condition_variable_any wake_;
    bool woken_{false};
    std::vector<Notice> notices_;
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::lan
