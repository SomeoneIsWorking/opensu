// lan_node — this openSU on the LAN while the player has LAN on: its book of paired peers, its
// listener, its DNS-SD announcement and browsing, the pairing it hosts and the one it asks for,
// its paired peers as targets of `save_sync::SaveSync` for the profiles linked with each
// (`PeerSaveTarget`, the group `lan`), the copies of its profiles they ask for (`ProfileCopies`)
// and those it asks of them, and a worker that keeps the paired peers' games in
// `RemoteLibrary` while they are announced (again when one comes or goes, a pairing changes, and
// every minute), their covers cached under `coverCache`; a paired peer's request to play a game
// here goes to `play`, and this one asks a host to play one of its games (`askToPlay`). It offers
// its ROMs to paired peers (`RomShelf`) and imports theirs (`romsOf`, `imports`). A pairing
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

#include "cover_desk.hpp"
#include "dns_sd.hpp"
#include "guest_pairing.hpp"
#include "host_pairing.hpp"
#include "lan_listener.hpp"
#include "net/web_client.hpp"
#include "netplay_asks.hpp"
#include "peer_book.hpp"
#include "peer_client.hpp"
#include "profile_copies.hpp"
#include "remote_library.hpp"
#include "rom_import.hpp"
#include "rom_shelf.hpp"
#include "save_sync/save_sync.hpp"
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
        /// The shared saves the paired peers are targets of while the node runs; it outlives the
        /// node.
        save_sync::SaveSync& saves;
        /// The profiles here, copied to the peers, whose linked ones' saves they are asked for and
        /// given.
        profiles::ProfileBook& profiles;
        /// A paired peer asks to play a listed game here (`LanListener::Sources::play`); empty
        /// refuses every such request.
        decltype(LanListener::Sources::play) play;
        /// How a peer's ask to join the game here stands (`LanListener::Sources::playStatus`).
        decltype(LanListener::Sources::playStatus) playStatus;
        /// A peer invites this one to its game (`LanListener::Sources::invited`).
        decltype(LanListener::Sources::invited) invited;
        /// The frame generation shaders a peer asks for (`LanListener::Sources::frameGenModules`).
        decltype(LanListener::Sources::frameGenModules) frameGenModules;
        /// Where ROMs being imported are staged, where each one taken goes, and when an import's
        /// progress changes (`RomImport::Setup`; this node gives its id and the hosts' links).
        RomImport::Setup roms;
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
        /// The service instance it is announced as: its name unless another has that one.
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
    [[nodiscard]] std::optional<Peer> find(const std::string& peerId) const {
        return book_.find(peerId);
    }
    /// Shares the installed games among `games` with paired peers from now on.
    /// The paired, announced openSUs are told to read the listing again when it changed.
    void share(const std::vector<library::Game>& games);
    /// The guests' asks about a listed game's cover and match.
    [[nodiscard]] CoverDesk& covers() noexcept {
        return covers_;
    }
    /// What `hostId` offers as a cover of its game `ask.game`. Blocks for the call, so never on
    /// the loop. Nothing with `error` when it is gone, unpaired or refuses.
    [[nodiscard]] std::optional<wire::CoverOffersAnswer> coverOffers(const std::stop_token& stop,
                                                                     const std::string& hostId,
                                                                     const wire::CoverAsk& ask,
                                                                     std::string& error) const;
    /// Hands `hostId` a pick among those covers. Blocks for the call.
    [[nodiscard]] bool pickCover(const std::stop_token& stop, const std::string& hostId,
                                 const wire::CoverPick& pick, std::string& error) const;
    /// Tells `hostId` what its game is. Blocks for the call.
    [[nodiscard]] bool matchGame(const std::stop_token& stop, const std::string& hostId,
                                 const wire::GameMatch& match, std::string& error) const;
    /// The thumbnail `hostId` offers as `token`, fetched as needed; empty when it cannot be had.
    [[nodiscard]] std::filesystem::path offeredCover(const std::stop_token& stop,
                                                     const std::string& hostId,
                                                     const std::string& token) const;
    /// The copy of a profile here a paired peer asks for that waits for the player.
    [[nodiscard]] std::optional<ProfileCopies::Question> copyQuestion() const {
        return copies_.question(ProfileCopies::Clock::now());
    }
    /// The player's answer to `peerId`'s ask for `profileId`. False with `error` when an allowed
    /// copy's link could not be kept, which the peer is then told was declined.
    [[nodiscard]] bool answerCopy(const std::string& peerId, const std::string& profileId,
                                  bool allow, std::string& error) {
        return copies_.decide(peerId, profileId, allow, ProfileCopies::Clock::now(), error);
    }

    // Asked of the paired, announced openSU `hostId`. Each blocks for the call, so never on the
    // loop, and is nothing or false with `error` when it is gone, unpaired or refuses, or without
    // one when `stop` ended the call.
    /// Its profiles, each saying whether it is linked with this one.
    [[nodiscard]] std::optional<std::vector<wire::ListedProfile>>
    profilesOf(const std::stop_token& stop, const std::string& hostId, std::string& error) const;
    /// The frame generation shaders `hostId` sends (`wire::frameGenModulesRoute`). Blocks for the
    /// call, so never on the loop. Nothing with `error` when it is gone, unpaired or has none.
    [[nodiscard]] std::optional<std::string> frameGenModulesOf(const std::stop_token& stop,
                                                               const std::string& hostId,
                                                               std::string& error) const;
    /// Asks it for a copy of its profile `profileId`, which its player is asked about.
    [[nodiscard]] bool askCopy(const std::stop_token& stop, const std::string& hostId,
                               const std::string& profileId, std::string& error) const;
    /// How the ask for `profileId` stands there.
    [[nodiscard]] std::optional<wire::CopyVerdict> copyStatus(const std::stop_token& stop,
                                                              const std::string& hostId,
                                                              const std::string& profileId,
                                                              std::string& error) const;
    /// Drops the link of `profileId` with `peerId` here, then tells it to drop it too when it is
    /// announced; both keep the profile. False with `error` when it could not be dropped here.
    [[nodiscard]] bool unlink(const std::stop_token& stop, const std::string& peerId,
                              const std::string& profileId, std::string& error) const;

    /// Offers the ROMs and BIOS files of `contents` to paired peers from now on.
    void shareRoms(RomShelf::Contents contents) {
        shelf_.share(std::move(contents));
    }
    /// The ROMs the paired, announced openSU `hostId` offers. Blocks for the call, so never on the
    /// loop. Nothing with `error` when it is gone, unpaired or refuses; nothing without one when
    /// `stop` ended the call.
    [[nodiscard]] std::optional<std::vector<wire::RomConsole>>
    romsOf(const std::stop_token& stop, const std::string& hostId, std::string& error) const;
    /// The ROMs being imported from paired peers.
    [[nodiscard]] RomImport& imports() noexcept {
        return *imports_;
    }
    [[nodiscard]] const RomImport& imports() const noexcept {
        return *imports_;
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

    /// What a host agreed to stream: where its stream takes packets, the session's keys, and
    /// whether this one joined a game that was running there already.
    struct PlayOffer {
        Endpoint stream;
        StreamKeys keys;
        bool joined{false};
        /// This one took over a seat of its profile.
        bool continued{false};
    };
    /// Asks the paired, announced openSU `hostId` to start its game `gameId` (its own id) as
    /// `profile`, this one's active profile, and stream it here, or to let this one join it while
    /// it runs there: then `waiting` is called and its answer is waited for, asked every second,
    /// for as long as its player may take. Blocks for the calls, so never on the loop. Nothing with
    /// `error` when it is gone, unpaired, refuses or its player declines; nothing without one when
    /// `stop` ended the call.
    [[nodiscard]] std::optional<PlayOffer>
    askToPlay(const std::stop_token& stop, const std::string& hostId, const std::string& gameId,
              const std::string& profile, const std::function<void()>& waiting,
              std::string& error) const;

    /// The asks to play a ROM together, this one's and the paired openSUs'.
    [[nodiscard]] NetplayAsks& netplay() const noexcept {
        return netplay_;
    }
    /// Asks the paired, announced `hostId` to play `ask`'s ROM together. Each of these blocks for
    /// the call, so never on the loop.
    [[nodiscard]] bool askNetplay(const std::stop_token& stop, const std::string& hostId,
                                  const wire::NetplayAsk& ask, std::string& error) const;
    /// How that ask stands.
    [[nodiscard]] std::optional<wire::NetplayVerdict> netplayStatus(const std::stop_token& stop,
                                                                    const std::string& hostId,
                                                                    const std::string& sha256,
                                                                    std::string& error) const;
    /// The save `peerId` handed over for the session on `sha256`.
    [[nodiscard]] std::optional<std::string> netplaySave(const std::stop_token& stop,
                                                         const std::string& peerId,
                                                         const std::string& sha256,
                                                         std::string& error) const;
    /// Where the announced `peerId` is.
    [[nodiscard]] std::optional<std::string> addressOf(const std::string& peerId,
                                                       std::string& error) const;
    /// A session's keys from the pairing with `peerId` and both salts.
    [[nodiscard]] std::optional<StreamKeys> netplayKeys(const std::string& peerId,
                                                        std::span<const std::uint8_t> guestSalt,
                                                        std::span<const std::uint8_t> hostSalt,
                                                        std::string& error) const;
    /// The game `id` runs here with `profiles` in its seats (an empty id, none runs): listed so,
    /// and the paired, announced openSUs are told to read the listing again.
    void playing(std::string id, std::vector<std::string> profiles);
    /// Invites the paired, announced openSU `peerId` to `game` running here. Blocks for the call,
    /// so never on the loop. False with `error` when it is gone or will not be asked.
    [[nodiscard]] bool invite(const std::stop_token& stop, const std::string& peerId,
                              const wire::Invitation& game, std::string& error) const;

    /// Forgets `peerId` here, and tells it to forget this one when it is announced. False with
    /// `error` when it could not be forgotten here.
    [[nodiscard]] bool forget(const std::string& peerId, std::string& error);

  private:
    /// An unpairing to tell a peer of, off the loop.
    struct Notice {
        Endpoint endpoint;
        secrets::crypto::Bytes secret;
    };

    void wake();
    void work(const std::stop_token& stop);
    /// Every announced paired peer's games, their covers fetched as needed and those of peers no
    /// longer paired dropped; `complete` false when a peer's could not be listed or `stop` ended
    /// its call.
    [[nodiscard]] std::vector<library::Game>
    fetch(const std::stop_token& stop, const std::vector<Sighting>& online, bool& complete);
    /// The cover `token` of `host`, kept in `folder`.
    [[nodiscard]] std::filesystem::path coverOf(const Peer& host, const PeerClient& client,
                                                const secrets::crypto::Bytes& secret,
                                                const std::string& token,
                                                const std::filesystem::path& folder) const;
    /// Where the thumbnails `hostId` offers are kept, apart from its listed covers.
    [[nodiscard]] std::filesystem::path offeredFolder(const std::string& hostId) const;
    void tell(const std::stop_token& stop, const Notice& notice) const;
    /// Where the paired, announced `hostId` listens and the pairing's secret; nothing with why.
    [[nodiscard]] std::optional<RomImport::Link> linkTo(const std::string& hostId,
                                                        std::string& why) const;
    /// `method` `route` with `body` asked of `hostId`: the answer's body when it says OK, at most
    /// `largest` bytes; nothing with `error` otherwise.
    [[nodiscard]] std::optional<std::string>
    askHost(const std::stop_token& stop, const std::string& hostId, std::string_view method,
            std::string_view route, const std::string& body, std::size_t largest,
            std::string& error) const;

    net::WebClient web_;
    std::filesystem::path coverCache_;
    std::shared_ptr<RemoteLibrary> remote_;
    std::function<void()> changed_;
    PeerBook book_;
    std::string name_;
    HostPairing pairing_{book_.id()};
    SharedLibrary library_;
    /// The paired peers are to be told the listing changed.
    bool newsDue_{false};
    save_sync::SaveSync& saves_;
    profiles::ProfileBook& profiles_;
    ProfileCopies copies_{profiles_};
    /// Mutable: a worker holding a const node hands saves over through it; it locks itself.
    mutable NetplayAsks netplay_;
    RomShelf shelf_;
    CoverDesk covers_;
    LanListener listener_;
    std::string problem_;
    std::unique_ptr<ServiceDiscovery> discovery_;
    /// Stopped first, so no import asks for a link while the node goes.
    std::unique_ptr<RomImport> imports_;
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
