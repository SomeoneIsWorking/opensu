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
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "lan/lan_node.hpp"
#include "lan/remote_library.hpp"
#include "library/game.hpp"
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
        /// A paired peer asks to play a game here; on the listener's thread.
        decltype(lan::LanNode::Setup::play) play;
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

    /// Shares the installed games among `games` with paired peers.
    void share(const std::vector<library::Game>& games);
    /// Offers the ROMs and BIOS files of `contents` to paired peers to import.
    void shareRoms(lan::RomShelf::Contents contents);
    /// Shares saves with `peerId` from now on, or no longer, and says so.
    void shareSaves(const std::string& peerId, bool on);

    /// Starts pairing with the openSU announced as `peerId` and shows its card.
    void pair(const std::string& peerId);
    /// Forgets `peerId` and says so.
    void forget(const std::string& peerId);

    /// What a host answered a request to play: its offer, or why there is none.
    using PlayAnswer = std::function<void(std::optional<lan::LanNode::PlayOffer>, std::string)>;
    /// Asks the paired openSU `hostId` to stream its game `gameId` here, off the loop; `answer` is
    /// called on that thread. False while LAN is off or another request is under way.
    [[nodiscard]] bool askToPlay(std::string hostId, std::string gameId, PlayAnswer answer);

    /// What a host answered a request for its ROMs: the listing, or why there is none.
    using RomsAnswer =
        std::function<void(std::optional<std::vector<lan::wire::RomConsole>>, std::string)>;
    /// Asks the paired openSU `hostId` for the ROMs it offers, off the loop; `answer` is called on
    /// that thread. False while LAN is off or another request for ROMs is under way.
    [[nodiscard]] bool askRoms(std::string hostId, RomsAnswer answer);

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
    void serviceQuestion();
    /// The player's answer to the question its dialog asks.
    void answerQuestion(bool accept);
    void serviceCard();
    void closeQuestion();
    void closeCard();
    /// The open screen with `id` and `title`.
    [[nodiscard]] const gpu2d::ui::Screen* openScreen(std::string_view id,
                                                      std::string_view title) const;

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
    /// Set on the import's thread, taken on the loop.
    std::shared_ptr<std::atomic<bool>> romsNews_{std::make_shared<std::atomic<bool>>(false)};
    /// The ROMs offered last, offered again after a restart.
    lan::RomShelf::Contents roms_;
    /// The question asked now, and its dialog's title while the dialog is open.
    std::optional<lan::HostPairing::Question> asked_;
    std::string questionTitle_;
    /// Whether the pairing card is up.
    bool carded_{false};
    /// Who was found and paired when last looked, and when.
    std::vector<lan::Sighting> seen_;
    std::vector<lan::Peer> paired_;
    std::chrono::steady_clock::time_point looked_{};
    /// Last, so it stops before what its callbacks reach goes.
    std::unique_ptr<lan::LanNode> node_;
    /// A request to play a host's game under way; joined before the node goes.
    std::jthread asker_;
    std::shared_ptr<std::atomic<bool>> asking_{std::make_shared<std::atomic<bool>>(false)};
    /// A request for a host's ROMs under way; joined before the node goes.
    std::jthread romAsker_;
    std::shared_ptr<std::atomic<bool>> askingRoms_{std::make_shared<std::atomic<bool>>(false)};
};

} // namespace opensu::app
