// netplay_run — one netplay session on this machine, however the two sides found each other
// (`LanNetplay`, `OnlinePlay`): what this side would run a game with, the session folder holding
// both players' saves (never a profile's), the session's file, the runner started as the active
// profile, and when the game ends only this side's player's save back into its profile.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lan/wire.hpp"
#include "library/game.hpp"
#include "secrets/crypto.hpp"

namespace opensu::app {

class NetplayRun {
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
        /// The active profile's save folder, as a launch would use it.
        std::function<std::filesystem::path()> profileSaves;
        /// Starts `game` with `saves` as its save folder; false when it was refused.
        std::function<bool(const library::Game& game, const std::filesystem::path& saves)> launch;
        /// The core options the link core would run `game` with here.
        std::function<std::map<std::string, std::string>(const library::Game& game)> linkOptions;
        /// The runner's system folder, where the link core looks for its BIOS.
        std::function<std::filesystem::path()> biosFolder;
    };

    /// A session both sides agreed to.
    struct Start {
        library::Game game;
        unsigned slot{0};
        /// `address:port` to send to; empty to learn it from the first datagram.
        std::string peer;
        /// The UDP port to bind, 0 for one of the system's choosing.
        std::uint16_t port{0};
        secrets::crypto::Bytes sendKey;
        secrets::crypto::Bytes receiveKey;
        /// What each player's save holds, by slot.
        std::array<std::string, 2> saves;
        lan::wire::NetplayClock clock;
        /// Frames of input delay; 0 leaves it to the runner.
        unsigned delay{0};
    };

    NetplayRun(Places places, Hooks hooks) : places_{std::move(places)}, hooks_{std::move(hooks)} {
    }

    /// The whole of `file`; empty when it cannot be read.
    [[nodiscard]] static std::string contentsOf(const std::filesystem::path& file);
    [[nodiscard]] static std::string sha256Of(const std::filesystem::path& rom);
    /// The ROM of `games` an ask names, by console, size and hash; its file name need not match.
    [[nodiscard]] static const library::Game* romOf(const std::vector<library::Game>& games,
                                                    const lan::wire::NetplayAsk& rom);

    /// What this side would run `game` with.
    [[nodiscard]] lan::wire::NetplayFacts factsOf(const library::Game& game) const;
    /// Where the active profile keeps `game`'s save.
    [[nodiscard]] std::filesystem::path profileSave(const library::Game& game) const;
    /// Writes the session's folder and file and starts the runner; why it did not, or empty.
    [[nodiscard]] std::string begin(const Start& start);
    /// `game` ended: a session's save goes back into the profile.
    void ended(const library::Game& game);

  private:
    struct Running {
        std::string gameId;
        std::filesystem::path own;
        std::filesystem::path profile;
    };

    Places places_;
    Hooks hooks_;
    std::optional<Running> running_;
};

} // namespace opensu::app
