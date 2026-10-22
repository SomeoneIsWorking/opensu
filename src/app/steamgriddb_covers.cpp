#include "steamgriddb_covers.hpp"

namespace opensu::app {
namespace {

// Home's cells are square (ui::HomeLayout), so a cover of another shape is cropped there.
constexpr artwork::CoverShape homeTile = artwork::CoverShape::Square;

} // namespace

SteamGridDbCovers::SteamGridDbCovers(ui::Shell& shell, audio::SdlSoundPlayer& sounds,
                                     const artwork::SteamGridDb& source, const Places& places,
                                     Hooks hooks)
    : shell_{shell}, hooks_{std::move(hooks)}, overrides_{places.chosen, places.fitted},
      fitter_{overrides_, source}, offers_{overrides_, source, places.thumbs},
      picker_{shell.coverPicker(), sounds, offers_, overrides_,
              CoverPickerController::Hooks{[this](const std::string& id) {
                                               show(id);
                                           },
                                           [this](const std::string& text, bool error) {
                                               hooks_.say(text, error);
                                           }}} {
}

void SteamGridDbCovers::catalogChanged(const std::vector<library::Game>& games) {
    fitter_.request(games, homeTile);
}

void SteamGridDbCovers::apply(std::vector<library::ShelfItem>& shelf) const {
    overrides_.apply(shelf);
}

void SteamGridDbCovers::coverArrived(const std::string& id, const std::filesystem::path& file) {
    const std::filesystem::path replaced = overrides_.shown(id);
    shell_.setArtwork(id, replaced.empty() ? file : replaced);
    if (const library::Game* game = hooks_.original(id)) {
        library::Game arrived = *game;
        arrived.artwork = file;
        fitter_.check(arrived, homeTile);
    }
}

void SteamGridDbCovers::choose(const std::string& id) {
    if (const library::Game* game = hooks_.original(id)) {
        picker_.open(*game);
    }
}

void SteamGridDbCovers::show(const std::string& id) {
    const std::filesystem::path replaced = overrides_.shown(id);
    if (!replaced.empty()) {
        shell_.setArtwork(id, replaced);
    } else if (const library::Game* game = hooks_.original(id);
               game != nullptr && !game->artwork.empty()) {
        shell_.setArtwork(id, game->artwork);
    }
}

void SteamGridDbCovers::service() {
    picker_.service();
    for (const artwork::FittedCover& fitted : fitter_.take()) {
        show(fitted.id);
    }
}

} // namespace opensu::app
