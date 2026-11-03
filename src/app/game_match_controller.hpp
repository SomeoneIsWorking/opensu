// game_match_controller — Match a different game, from a game's details or "Choose artwork": the
// Steam apps Steam's store search finds for the game's title (without its last words while it
// finds none), or a name the player types, each
// with its Steam cover and release year as they arrive on a worker. A keeps the one picked as the
// game's match (`OwnGamesController::match`), which its cover, SteamGridDB lookup and Proton fixes
// follow; B closes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "library/game.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class GameMatchController {
  public:
    /// What the controller shows its screens with.
    struct Parts {
        gpu2d::ui::Navigator& navigator;
        const ui::Chrome& chrome;
        ui::ScreenImages& images;
        KeyboardPad& keyboard;
        audio::SdlSoundPlayer& sounds;
    };

    /// Where the candidates come from and where their covers are kept.
    struct Places {
        /// The candidates' covers, cache.
        std::filesystem::path covers;
        /// Steam's CDN app root (`artwork::RemoteSources::steam`).
        std::string steamCdn;
    };

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The player picked Steam app `appId`, titled `title`, as what `game` is.
        std::function<void(const library::Game& game, const std::string& appId,
                           const std::string& title)>
            matched;
    };

    /// One Steam app the game could be.
    struct Candidate {
        std::string appId;
        std::string title;
        /// The year it came out on Steam; empty until known or when the store says none.
        std::string year;
        /// Its Steam cover; empty while it downloads or when it has none.
        std::filesystem::path cover;
        /// Whether its cover and year have been asked for.
        bool complete{false};

        bool operator==(const Candidate&) const = default;
    };

    GameMatchController(Parts parts, Places places, Hooks hooks);
    ~GameMatchController();
    GameMatchController(const GameMatchController&) = delete;
    GameMatchController& operator=(const GameMatchController&) = delete;

    /// Opens for `game`, searching Steam for its title.
    void open(const library::Game& game);
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Shows what the search found. Main loop only.
    void service();

    // What the screen asks.
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    /// What the search is doing or found, as a line.
    [[nodiscard]] std::string status() const;
    [[nodiscard]] const std::vector<Candidate>& candidates() const noexcept {
        return candidates_;
    }
    /// The Steam app the game is matched to now; empty for none.
    [[nodiscard]] const std::string& current() const noexcept {
        return game_.artworkKey;
    }
    /// Keeps candidate `index` as the game's match; the current one just closes.
    void choose(std::size_t index);
    /// Asks for another name to search for.
    void searchAnother();

  private:
    /// What the worker found for one search.
    struct Found {
        std::uint64_t generation{0};
        std::vector<Candidate> candidates;
        bool searched{false};
        /// The name Steam found the apps by: the one searched, or it without its last words.
        std::string term;
        std::string error;
    };

    [[nodiscard]] const gpu2d::ui::Screen* screen() const;
    /// Searches Steam for `term` on the worker.
    void search(const std::string& term);
    void find(const std::stop_token& stop, const std::string& term, std::uint64_t generation);
    /// The cover of app `appId` in the cache, downloaded when it is not there; empty for none.
    [[nodiscard]] std::filesystem::path coverOf(const std::string& appId) const;
    void publish(const Found& found);
    void closeName();

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Places places_;
    Hooks hooks_;
    library::Game game_;
    std::string title_;
    std::string term_;
    /// The name card's text.
    std::string name_;
    std::vector<Candidate> candidates_;
    bool searched_{false};
    std::string error_;
    std::uint64_t generation_{0};

    std::mutex mutex_;
    std::optional<Found> found_;
    /// Last, so it stops before anything it writes is destroyed.
    std::jthread finder_;
};

} // namespace opensu::app
