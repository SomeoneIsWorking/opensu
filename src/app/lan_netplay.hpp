// lan_netplay — two paired openSUs playing one ROM linked, on the shell's loop. As the asker: Play
// with <name> hashes the ROM and hands over its save, asks, and waits (`play`). As the asked: the
// question put to the player, the same ROM looked for by console, name and hash, and refused at
// once when the core, its options or the BIOS differ or the cartridge has a clock. Each side takes
// the other's save, writes both into a session folder of its own (never a profile's), writes the
// session's file and starts `opensu-retro --netplay` as its own active profile: the asked side
// first, its runner binding a UDP port of the system's choosing, which the asker is then told. When
// the game ends only this side's player's save goes back into its profile (`ended`). The calls run
// off the loop through `LanService::askPeers`.
#pragma once

#include <filesystem>
#include <functional>
#include <map>
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
#include "ui/game_details.hpp"

namespace opensu::app {

class LanNetplay {
  public:
    struct Places {
        /// The session's saves, emptied as each session starts.
        std::filesystem::path sessionSaves;
        /// The session's file, keys and all; owner-only, gone when the session ends.
        std::filesystem::path sessionFile;
    };
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The games of this catalog.
        std::function<const std::vector<library::Game>&()> games;
        /// The active profile's save folder, as a launch would use it.
        std::function<std::filesystem::path()> profileSaves;
        /// Starts `game` with `saves` as its save folder; false when it was refused.
        std::function<bool(const library::Game& game, const std::filesystem::path& saves)> launch;
        /// The core options the link core would run `game` with here.
        std::function<std::map<std::string, std::string>(const library::Game& game)> linkOptions;
        /// The runner's system folder, where the link core looks for its BIOS.
        std::function<std::filesystem::path()> biosFolder;
        /// The UDP port the session's runner bound; nothing until it says.
        std::function<std::optional<std::uint16_t>()> sessionPort;
    };

    LanNetplay(LanService& lan, gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
               Places places, Hooks hooks);

    /// The paired openSUs that can be asked now.
    [[nodiscard]] std::vector<ui::GameSettingsView::Partner> partners() const;

    /// Asks `peerId` to play `game` together.
    void play(const library::Game& game, const std::string& peerId);
    /// Once a frame: the question, and what the threads left.
    void service();
    /// `game` ended: a session's save goes back into the profile.
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
    };
    struct Running {
        std::string gameId;
        std::filesystem::path own;
        std::filesystem::path profile;
    };

    /// What this side would run `game` with.
    [[nodiscard]] lan::wire::NetplayFacts factsOf(const library::Game& game) const;
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
    Places places_;
    Hooks hooks_;
    std::shared_ptr<Arrived> arrived_{std::make_shared<Arrived>()};
    std::optional<lan::NetplayAsks::Question> asked_;
    std::string questionTitle_;
    std::optional<Starting> starting_;
    std::optional<Running> running_;
};

} // namespace opensu::app
