// play_wait — Play on a game that cannot start yet. The launch card stays up while the game's
// emulator downloads or its romset is checked, and the game starts when the library says it can.
// A wait that cannot end tells the player why. Cancel stops the wait and the download it started.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "emulator_downloads.hpp"
#include "library/game.hpp"

namespace opensu::app {

class PlayWait {
  public:
    struct Hooks {
        /// The game as the library has it now.
        std::function<std::optional<library::Game>(const std::string& id)> find;
        /// Plays the game, which can start now.
        std::function<void(const library::Game& game, std::vector<std::string> environment,
                           const std::string& profile)>
            play;
        /// Puts the launch card up.
        std::function<void(const library::Game& game)> show;
        /// The card's line, and how far along when that is known.
        std::function<void(std::string line, std::optional<double> fraction)> update;
        /// Tells the player why the game will not start.
        std::function<void(const std::string& text)> refuse;
        /// A running download, if any.
        std::function<std::optional<EmulatorDownloads::Progress>()> progress;
        /// The last download to end, if any.
        std::function<std::optional<EmulatorDownloads::Finished>()> finished;
        /// Starts a download; empty on success, else why not.
        std::function<std::string(const std::string& id)> download;
        /// Stops the running download of `id`.
        std::function<void(const std::string& id)> cancelDownload;
    };

    explicit PlayWait(Hooks hooks);

    /// Waits for `game` if something can still make it startable; false if nothing can.
    [[nodiscard]] bool begin(const library::Game& game, std::vector<std::string> environment,
                             const std::string& profile);
    [[nodiscard]] bool waiting() const noexcept {
        return held_.has_value();
    }
    void cancel();
    /// Moves the wait on. Main loop only.
    void service();
    /// The card's line when it changed since last asked.
    [[nodiscard]] std::optional<std::string> newLine();

  private:
    void show(std::string line, std::optional<double> fraction);

    struct Held {
        std::string gameId;
        std::vector<std::string> environment;
        std::string profile;
        /// The last download to end before this wait, which says nothing about it.
        std::uint64_t seenDownload{0};
        /// The download this wait started.
        std::string started;
    };

    Hooks hooks_;
    std::optional<Held> held_;
    std::string line_;
    bool lineChanged_{false};
};

} // namespace opensu::app
