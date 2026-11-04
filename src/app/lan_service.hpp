// lan_service — LAN remote play on the shell's loop while the player has LAN on (Integrations):
// the `lan::LanNode`, made when LAN is turned on or this openSU is renamed and gone when it is
// turned off; the question the host's half of a pairing asks the player, with the code; the card
// a pairing this openSU asks for shows, with the same code, until the host answers; and the news
// the node's threads leave for the loop (the paired peers' games changed, peers came or went).
// The remote games themselves are in the `RemoteLibrary` the catalog's provider reads.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "lan/lan_node.hpp"
#include "lan/remote_library.hpp"
#include "library/game.hpp"
#include "preferences.hpp"
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
    };
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The remote games changed: the catalog reads them again.
        std::function<void()> remoteChanged;
        /// Who is found or paired changed: the Settings page shows it.
        std::function<void()> peersChanged;
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
    /// The name this openSU goes by: the player's, else the machine's.
    [[nodiscard]] std::string name() const;

    /// Shares the installed games among `games` with paired peers.
    void share(const std::vector<library::Game>& games);
    /// Starts pairing with the openSU announced as `peerId` and shows its card.
    void pair(const std::string& peerId);
    /// Forgets `peerId` and says so.
    void forget(const std::string& peerId);

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
};

} // namespace opensu::app
