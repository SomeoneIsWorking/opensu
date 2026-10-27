// steamgriddb_covers — SteamGridDB's covers on the shelf: the covers the player chose or that were
// fitted in place of a game's own (`artwork::CoverOverrides`), the fitting of covers whose shape
// the Home grid's square tiles would crop (`artwork::CoverFitter`), and "Choose artwork"
// (`CoverPickerController`). The game's own cover stays on disk throughout.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "artwork/cover_fitter.hpp"
#include "artwork/cover_offers.hpp"
#include "artwork/cover_overrides.hpp"
#include "artwork/steamgriddb.hpp"
#include "audio/sdl_sound_player.hpp"
#include "cover_picker_controller.hpp"
#include "library/game.hpp"
#include "library/shelf.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class SteamGridDbCovers {
  public:
    struct Places {
        /// The player's choices, kept with the data.
        std::filesystem::path chosen;
        /// Fitted covers and the picker's thumbnails, both cache.
        std::filesystem::path fitted;
        std::filesystem::path thumbs;
    };

    struct Hooks {
        /// Game `id` as its source has it, its own cover in `artwork`; null when it is gone.
        std::function<const library::Game*(const std::string& id)> original;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Starts connecting SteamGridDB, on Settings › Artwork where the sign-in shows.
        std::function<void()> connect;
    };

    SteamGridDbCovers(ui::Shell& shell, audio::SdlSoundPlayer& sounds,
                      const artwork::SteamGridDb& source, const Places& places, Hooks hooks);

    /// A new catalog: its covers are checked against the Home grid's tiles.
    void catalogChanged(const std::vector<library::Game>& games);
    /// Puts each shelf item's shown cover in place of its own.
    void apply(std::vector<library::ShelfItem>& shelf) const;
    /// Game `id`'s own cover was saved as `file`: shows it, or what replaces it, and checks it.
    void coverArrived(const std::string& id, const std::filesystem::path& file);
    /// Opens "Choose artwork" for game `id`.
    void choose(const std::string& id);
    [[nodiscard]] CoverPickerController& picker() noexcept {
        return picker_;
    }
    [[nodiscard]] const CoverPickerController& picker() const noexcept {
        return picker_;
    }
    /// Shows fitted covers and what the picker found. Main loop only.
    void service();

  private:
    /// Shows game `id`'s cover as it stands.
    void show(const std::string& id);

    ui::Shell& shell_;
    Hooks hooks_;
    artwork::CoverOverrides overrides_;
    artwork::CoverFitter fitter_;
    artwork::CoverOffers offers_;
    CoverPickerController picker_;
};

} // namespace opensu::app
