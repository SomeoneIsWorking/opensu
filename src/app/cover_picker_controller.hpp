// cover_picker_controller — "Choose artwork" from the details page: lists the cover the game shows
// now, its own when another replaces it, and SteamGridDB's covers as `artwork::CoverOffers` finds
// them; A keeps the focused one as the game's choice, B closes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "artwork/cover_offers.hpp"
#include "artwork/cover_overrides.hpp"
#include "audio/sdl_sound_player.hpp"
#include "gamepad/event.hpp"
#include "library/game.hpp"
#include "ui/cover_picker.hpp"

namespace opensu::app {

class CoverPickerController {
  public:
    struct Hooks {
        /// Game `id`'s cover changed: the shelf shows it again.
        std::function<void(const std::string& id)> changed;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    CoverPickerController(ui::CoverPicker& picker, audio::SdlSoundPlayer& sounds,
                          artwork::CoverOffers& offers, const artwork::CoverOverrides& overrides,
                          Hooks hooks)
        : picker_{picker}, sounds_{sounds}, offers_{offers}, overrides_{overrides},
          hooks_{std::move(hooks)} {
    }

    /// Opens for `game` as its source has it, its own cover in `artwork`.
    void open(const library::Game& game);
    [[nodiscard]] bool isOpen() const noexcept {
        return picker_.isOpen();
    }
    /// A button while the picker is up: it takes them all.
    void act(gamepad::Button button);
    /// Shows what SteamGridDB listed and ends choices. Main loop only.
    void service();

  private:
    /// What a pick stands for: the cover shown now, the game's own, or offer `index`.
    struct Entry {
        enum class Kind : std::uint8_t { Now, Own, Offer } kind{Kind::Now};
        std::size_t offer{0};
    };

    void close();
    void choose();
    void show();
    [[nodiscard]] std::string status() const;

    ui::CoverPicker& picker_;
    audio::SdlSoundPlayer& sounds_;
    artwork::CoverOffers& offers_;
    const artwork::CoverOverrides& overrides_;
    Hooks hooks_;
    std::string gameId_;
    std::string title_;
    std::filesystem::path own_;
    std::optional<artwork::CoverListing> listing_;
    std::vector<Entry> entries_;
    bool saving_{false};
};

} // namespace opensu::app
