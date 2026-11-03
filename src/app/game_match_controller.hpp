// game_match_controller — Match a different game, from a game's details or "Choose artwork": for a
// store game or one the player added, the Steam apps Steam's store search finds for its title;
// for a ROM, the box art titles its system's libretro-thumbnails listing has for it. The title is
// searched without its last words while nothing is found, or the player types a name; each
// candidate's cover (and a Steam app's release year) arrives on a worker. A keeps the one picked
// as the game's match (`library::matchKey`), which its cover and SteamGridDB lookup follow, and an
// added game's Proton fixes; B closes.
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

#include "artwork/artwork_store.hpp"
#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "library/game.hpp"
#include "library/match_pick.hpp"
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
        /// libretro-thumbnails' root (`artwork::RemoteSources::libretro`).
        std::string libretro;
    };

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The player picked `key` (a Steam app id, or a box art title), titled `title`, as what
        /// `game` is.
        std::function<void(const library::Game& game, const std::string& key,
                           const std::string& title)>
            matched;
    };

    /// One game the game could be: a Steam app, or a box art title.
    struct Candidate {
        /// What is kept as the match: the app id, or the box art title.
        std::string key;
        std::string title;
        /// The year it came out on Steam; empty until known, when the store says none, and for a
        /// box art title.
        std::string year;
        /// Its cover; empty while it downloads or when it has none.
        std::filesystem::path cover;
        /// Whether its cover and year have been asked for.
        bool complete{false};

        bool operator==(const Candidate&) const = default;
    };

    /// `listings` keeps the systems' box art listings.
    GameMatchController(Parts parts, Places places, const artwork::ArtworkStore& listings,
                        Hooks hooks);
    ~GameMatchController();
    GameMatchController(const GameMatchController&) = delete;
    GameMatchController& operator=(const GameMatchController&) = delete;

    /// Opens for `game`, searching for its title; nothing for a game that cannot be matched.
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
    /// What the game is matched to now; empty for none.
    [[nodiscard]] const std::string& current() const noexcept {
        return game_.artworkKey;
    }
    [[nodiscard]] library::MatchKind kind() const noexcept {
        return kind_;
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
        /// The name the candidates were found by: the one searched, or it without its last words.
        std::string term;
        std::string error;
    };

    [[nodiscard]] const gpu2d::ui::Screen* screen() const;
    /// Searches for `term` on the worker.
    void search(std::string term);
    void find(const std::stop_token& stop, const std::string& term, std::uint64_t generation);
    void findSteamApps(const std::stop_token& stop, const std::string& term, Found& found);
    void findBoxArt(const std::stop_token& stop, const std::string& term, Found& found);
    /// The file at `url` kept in the cache as `name`, downloaded when it is not there; empty when
    /// there is none.
    [[nodiscard]] std::filesystem::path cached(const std::string& url,
                                               const std::string& name) const;
    /// Where the candidates are searched, as the player reads it.
    [[nodiscard]] std::string searched() const;
    void publish(const Found& found);
    void closeName();

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Places places_;
    const artwork::ArtworkStore& listings_;
    Hooks hooks_;
    library::Game game_;
    library::MatchKind kind_{library::MatchKind::None};
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
