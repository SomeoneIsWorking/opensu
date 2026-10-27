// cover_picker_controller — "Choose artwork" from the details page: lists the cover the game shows
// now, its own when another replaces it, and SteamGridDB's covers as `artwork::CoverOffers` finds
// them; A keeps the chosen one as the game's choice, Y opens Settings › Artwork while SteamGridDB
// is not connected, B closes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "artwork/cover_offers.hpp"
#include "artwork/cover_overrides.hpp"
#include "audio/sdl_sound_player.hpp"
#include "library/game.hpp"
#include "ui/screen_images.hpp"

namespace opensu::app {

class CoverPickerController {
  public:
    struct Hooks {
        /// Game `id`'s cover changed: the shelf shows it again.
        std::function<void(const std::string& id)> changed;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Starts connecting SteamGridDB, on Settings › Artwork where the sign-in shows.
        std::function<void()> connect;
    };

    /// What the picker offers and keeps.
    struct Sources {
        artwork::CoverOffers& offers;
        const artwork::CoverOverrides& overrides;
    };

    /// One cover offered.
    struct Pick {
        /// Under the picture: "Now", "Own" or the picture's size.
        std::string label;
        /// The image, or empty while it downloads.
        std::filesystem::path file;

        bool operator==(const Pick&) const = default;
    };

    /// The picker goes on `navigator`'s stack, its pictures among `images`.
    CoverPickerController(gpu2d::ui::Navigator& navigator, ui::ScreenImages& images,
                          audio::SdlSoundPlayer& sounds, Sources sources, Hooks hooks)
        : navigator_{navigator}, images_{images}, sounds_{sounds}, offers_{sources.offers},
          overrides_{sources.overrides}, hooks_{std::move(hooks)} {
    }

    /// Opens for `game` as its source has it, its own cover in `artwork`.
    void open(const library::Game& game);
    /// Closes the picker with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Shows what SteamGridDB listed and ends choices. Main loop only.
    void service();

    // What the screen asks.
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    /// What the picker is doing or found, as a line.
    [[nodiscard]] std::string status() const;
    [[nodiscard]] const std::vector<Pick>& picks() const noexcept {
        return picks_;
    }
    /// Whether SteamGridDB needs connecting before it lists covers, which Y starts.
    [[nodiscard]] bool disconnected() const;
    /// Keeps pick `index` as the game's cover; the one shown now just closes the picker.
    void choose(std::size_t index);
    /// Closes the picker and starts connecting SteamGridDB, while it is disconnected.
    void connect();

  private:
    /// What a pick stands for: the cover shown now, the game's own, or offer `index`.
    struct Entry {
        enum class Kind : std::uint8_t { Now, Own, Offer } kind{Kind::Now};
        std::size_t offer{0};
    };

    /// The picker on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* picker() const;
    /// Reads the picks again from what is shown and listed.
    void show();

    gpu2d::ui::Navigator& navigator_;
    ui::ScreenImages& images_;
    audio::SdlSoundPlayer& sounds_;
    artwork::CoverOffers& offers_;
    const artwork::CoverOverrides& overrides_;
    Hooks hooks_;
    std::string gameId_;
    std::string title_;
    std::filesystem::path own_;
    std::optional<artwork::CoverListing> listing_;
    std::vector<Pick> picks_;
    std::vector<Entry> entries_;
    bool saving_{false};
};

} // namespace opensu::app
