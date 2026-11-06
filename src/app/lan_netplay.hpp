// lan_netplay — two paired openSUs playing one ROM linked, on the shell's loop. As the asker: Play
// with <name> hashes the ROM and hands over its save, asks, and waits (`play`). As the asked: the
// question put to the player, the same ROM looked for by console, name and hash, and refused at
// once when the core, its options or the BIOS differ. Each side takes the other's save and starts
// the session through `NetplayRun`, with the clock both run from, the asker's: the asked side
// first, its runner binding a UDP port of the system's choosing, which the asker is then told. The
// calls run off the loop through `LanService::askPeers`.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "lan/lan_node.hpp"
#include "lan/netplay_asks.hpp"
#include "lan_service.hpp"
#include "library/game.hpp"
#include "netplay_run.hpp"
#include "ui/game_details.hpp"

namespace opensu::app {

class LanNetplay {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The games of this catalog.
        std::function<const std::vector<library::Game>&()> games;
        /// The UDP port the session's runner bound; nothing until it says.
        std::function<std::optional<std::uint16_t>()> sessionPort;
    };

    LanNetplay(LanService& lan, gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
               NetplayRun& run, Hooks hooks);

    /// The paired openSUs that can be asked now.
    [[nodiscard]] std::vector<ui::GameSettingsView::Partner> partners() const;

    /// Asks `peerId` to play `game` together.
    void play(const library::Game& game, const std::string& peerId);
    /// Once a frame: the question, and what the threads left.
    void service();
    /// `game` ended before the asker could be told it started.
    void ended(const library::Game& game);

  private:
    /// A session both sides agreed to, as the loop gets it.
    struct Agreed {
        library::Game game;
        unsigned slot{0};
        std::string peerName;
        /// The asker's id and the salt it is told, on the asked side only.
        std::string askerId;
        secrets::crypto::Bytes salt;
        /// The address the asker sends to.
        std::string peer;
        lan::StreamKeys keys;
        /// What each side's save holds, by slot.
        std::string saves[2];
        lan::wire::NetplayClock clock;
    };
    struct Arrived {
        std::mutex mutex;
        std::vector<Agreed> agreed;
        std::vector<std::string> refusals;
    };
    /// The asked side's session, its runner started, the asker not told yet.
    struct Starting {
        std::string askerId;
        secrets::crypto::Bytes salt;
        std::int64_t clockMs{0};
    };
    /// Refuses the ask now put to this side when `game` cannot be played with its facts, in the
    /// words of each side; true when it did.
    [[nodiscard]] bool refused(lan::LanNode& node, const lan::NetplayAsks::Question& asked,
                               const library::Game& game);
    void serviceQuestion();
    void serviceStart();
    void answer(bool allow);
    void closeQuestion();
    void begin(Agreed agreed);
    /// Declines `askerId`'s ask with `why`; an asker's own side has none to tell.
    void tellAsker(const std::string& askerId, const std::string& why);

    LanService& lan_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    NetplayRun& run_;
    Hooks hooks_;
    std::shared_ptr<Arrived> arrived_{std::make_shared<Arrived>()};
    std::optional<lan::NetplayAsks::Question> asked_;
    std::string questionTitle_;
    std::optional<Starting> starting_;
};

} // namespace opensu::app
