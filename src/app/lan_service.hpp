// lan_service — LAN remote play and save sharing on the shell's loop while the player has LAN on
// (Integrations): the `lan::LanNode`, made when LAN is turned on or this openSU is renamed and gone
// when it is turned off; the question the host's half of a pairing asks the player, with the code;
// the card a pairing this openSU asks for shows, with the same code, until the host answers; its
// paired peers as targets of the shell's shared saves (`SaveSharing::engine`); a paired peer's
// request to play a game here, passed to `Hooks::play`, and this one's to play a host's
// (`askToPlay`, on a thread of its own); and the news the node's threads leave for the loop (the
// paired peers' games changed, peers came or went). The remote games themselves are in the
// `RemoteLibrary` the catalog's provider reads.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "lan/lan_node.hpp"
#include "lan/remote_library.hpp"
#include "library/game.hpp"
#include "peer_question.hpp"
#include "preferences.hpp"
#include "save_sync/save_sync.hpp"
#include "secrets/secret_store.hpp"

namespace opensu::app {

class LanService {
  public:
    struct Places {
        std::filesystem::path bookFile;
        secrets::SecretStore secrets;
        /// Where the paired peers' covers are kept.
        std::filesystem::path coverCache;
        /// The listener's port; 0 for a free one.
        std::uint16_t port{0};
        /// The shared saves the node adds the paired peers to; it outlives the service.
        save_sync::SaveSync& saves;
        /// The profiles here, whose saves the paired peers ask for.
        profiles::ProfileBook& profiles;
        /// Where ROMs imported from peers are staged and how each is put in place; when their
        /// progress changes is this service's to say.
        lan::RomImport::Setup roms;
    };
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The remote games changed: the catalog reads them again.
        std::function<void()> remoteChanged;
        /// Who is found or paired changed: the Settings page shows it.
        std::function<void()> peersChanged;
        /// How the ROMs imported from peers stand changed: the import page shows it.
        std::function<void()> romsChanged;
        /// A paired peer asks to play a game here, and how its ask to join one stands; on the
        /// listener's thread.
        decltype(lan::LanNode::Setup::play) play;
        decltype(lan::LanNode::Setup::playStatus) playStatus;
        decltype(lan::LanNode::Setup::close) close;
        /// A paired peer invites this one to its game; on the listener's thread.
        decltype(lan::LanNode::Setup::invited) invited;
        /// An ask to join passed here, and the answer to one passed on; on the listener's thread.
        decltype(lan::LanNode::Setup::joinAsked) joinAsked;
        decltype(lan::LanNode::Setup::joinAnswered) joinAnswered;
        /// The frame generation shaders a paired peer asks for; on the listener's thread.
        decltype(lan::LanNode::Setup::frameGenModules) frameGenModules;
        /// This openSU's Steam sign-in, for a paired peer its player lets copy it; on the
        /// listener's thread.
        decltype(lan::LanNode::Setup::steamLogin) steamLogin;
    };

    /// On from the start when `active`. The question and the card go on `navigator`.
    LanService(Places places, Preferences& preferences, gpu2d::ui::Navigator& navigator,
               audio::SdlSoundPlayer& sounds, std::shared_ptr<lan::RemoteLibrary> remote,
               bool active, Hooks hooks);
    ~LanService();
    LanService(const LanService&) = delete;
    LanService& operator=(const LanService&) = delete;
    LanService(LanService&&) = delete;
    LanService& operator=(LanService&&) = delete;

    /// Turns LAN on or off; off forgets the remote games until it is on again.
    void setActive(bool on);
    /// Starts again under the name the settings now hold.
    void rename();
    /// The node while LAN is on; null while it is off.
    [[nodiscard]] const lan::LanNode* node() const noexcept {
        return node_.get();
    }
    [[nodiscard]] lan::LanNode* node() noexcept {
        return node_.get();
    }
    /// The name this openSU goes by: the player's, else the machine's.
    [[nodiscard]] std::string name() const;

    /// Shares the installed games among `games` with paired peers, with why they cannot be
    /// streamed from here (`hosting`, empty when they can), and the ids on Home in its order.
    void share(const std::vector<library::Game>& games, std::string hosting,
               std::vector<std::string> home);
    /// Offers the ROMs and BIOS files of `contents` to paired peers to import.
    void shareRoms(lan::RomShelf::Contents contents);
    /// The game `id` runs here with `profiles` in its seats; the paired openSUs hear of it.
    void playing(std::string id, std::vector<std::string> profiles);
    /// The paired openSUs on the LAN now.
    [[nodiscard]] std::vector<lan::Peer> announced() const;
    /// The name the paired openSU `peerId` goes by, or "that openSU" when it is not paired.
    [[nodiscard]] std::string peerName(const std::string& peerId) const;
    /// Runs `work` on the node off the loop, its stop token asked to stop as the node goes, which
    /// waits for it. False while LAN is off or another such call is under way.
    [[nodiscard]] bool
    askPeers(std::function<void(const lan::LanNode&, const std::stop_token&)> work);

    /// What a host answered an ask for its Steam sign-in: the sign-in (a secret), or why there is
    /// none.
    using LoginAnswer = std::function<void(std::optional<std::string>, std::string)>;
    /// Asks the paired openSU `hostId` for its Steam sign-in through `askPeers`, waiting while its
    /// player is asked; `answer` is called on that thread, unless `cancel` or the node going ended
    /// the ask first. False when `askPeers` is.
    [[nodiscard]] bool askSteamLogin(std::string hostId, std::stop_token cancel,
                                     LoginAnswer answer);

    /// Runs `work` on the node off the loop for a cover of a host's game, as `askPeers` does, once
    /// the last such work ended. False while LAN is off.
    [[nodiscard]] bool
    askCovers(std::function<void(const lan::LanNode&, const std::stop_token&)> work);
    /// The guests' asks about a listed game's cover and match; null while LAN is off.
    [[nodiscard]] lan::CoverDesk* coverDesk() noexcept {
        return node_ == nullptr ? nullptr : &node_->covers();
    }

    /// Starts pairing with the openSU announced as `peerId` and shows its card.
    void pair(const std::string& peerId);
    /// Forgets `peerId` and says so.
    void forget(const std::string& peerId);

    /// What a host answered a request to play: its offer, or why there is none.
    using PlayAnswer = std::function<void(std::optional<lan::LanNode::PlayOffer>, std::string)>;
    /// Asks the paired openSU `hostId` to stream its game `choice` here, played as `profile`, off
    /// the loop; `waiting` is called on that thread when its player is asked to let this one join,
    /// and `answer` when it is over. False while LAN is off or another request is under way.
    [[nodiscard]] bool askToPlay(std::string hostId, lan::wire::PlayChoice choice,
                                 std::string profile, std::function<void()> waiting,
                                 PlayAnswer answer);

    /// What a host answered a request for its ROMs and installs: the listing, or why there is
    /// none.
    using RomsAnswer = std::function<void(std::optional<lan::wire::Shelf>, std::string)>;
    /// Asks the paired openSU `hostId` for the ROMs and installs it offers, off the loop; `answer`
    /// is called on that thread. False while LAN is off or another request for ROMs is under way.
    [[nodiscard]] bool askRoms(std::string hostId, RomsAnswer answer);
    /// What a host answered a request for an install's files: the files, or why there are none.
    using FilesAnswer =
        std::function<void(std::optional<std::vector<lan::wire::RomFile>>, std::string)>;
    /// Asks `hostId` for the files of its install `installId`, off the loop; `answer` is called on
    /// that thread. False while LAN is off or another such request is under way.
    [[nodiscard]] bool askInstallFiles(std::string hostId, std::string installId,
                                       FilesAnswer answer);

    /// Shows the question and the card as the pairings stand and passes on the node's news.
    /// Every frame, main loop only.
    void service();

    /// What the control channel reports.
    struct Glance {
        bool on{false};
        std::size_t seen{0};
        std::size_t paired{0};
        /// The code a pairing on either side shows now; empty when none does.
        std::string code;
    };
    [[nodiscard]] Glance glance() const;

  private:
    void start();
    void stop();
    /// Stops the requests to a host under way and waits for them.
    void stopAskers();
    void serviceQuestion();
    /// The player's answer to the question its dialog asks.
    void answerQuestion(bool accept);
    void serviceCard();
    void closeCard();

    Places places_;
    Preferences& preferences_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    std::shared_ptr<lan::RemoteLibrary> remote_;
    Hooks hooks_;
    /// Set on the node's threads, taken on the loop.
    std::shared_ptr<std::atomic<bool>> news_{std::make_shared<std::atomic<bool>>(false)};
    /// What was shared last, shared again after a restart.
    std::vector<library::Game> shared_;
    std::string hosting_;
    std::vector<std::string> home_;
    /// Set on the import's thread, taken on the loop.
    std::shared_ptr<std::atomic<bool>> romsNews_{std::make_shared<std::atomic<bool>>(false)};
    /// The ROMs offered last, offered again after a restart.
    lan::RomShelf::Contents roms_;
    /// The question asked now, and its dialog.
    std::optional<lan::HostPairing::Question> asked_;
    PeerQuestion question_{navigator_, sounds_};
    /// Whether the pairing card is up.
    bool carded_{false};
    /// Who was found and paired when last looked, and when.
    std::vector<lan::Sighting> seen_;
    std::vector<lan::Peer> paired_;
    std::chrono::steady_clock::time_point looked_{};
    /// Last, so it stops before what its callbacks reach goes.
    std::unique_ptr<lan::LanNode> node_;
    /// A request to play a host's game under way; stopped and joined before the node goes.
    std::jthread asker_;
    std::shared_ptr<std::atomic<bool>> asking_{std::make_shared<std::atomic<bool>>(false)};
    /// A request for a host's ROMs under way; stopped and joined before the node goes.
    std::jthread romAsker_;
    std::shared_ptr<std::atomic<bool>> askingRoms_{std::make_shared<std::atomic<bool>>(false)};
    std::jthread installAsker_;
    std::shared_ptr<std::atomic<bool>> askingInstall_{std::make_shared<std::atomic<bool>>(false)};
    /// A call of `askPeers` under way; stopped and joined before the node goes.
    std::jthread peerAsker_;
    std::shared_ptr<std::atomic<bool>> askingPeers_{std::make_shared<std::atomic<bool>>(false)};
    /// A call of `askCovers` under way; stopped and joined before the node goes.
    std::jthread coverAsker_;
};

} // namespace opensu::app
