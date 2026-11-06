#include "steamgriddb_covers.hpp"

#include <algorithm>
#include <utility>

namespace opensu::app {
namespace {

// Home's cells are square (ui::HomeLayout), so a cover of another shape is cropped there.
constexpr artwork::CoverShape homeTile = artwork::CoverShape::Square;

} // namespace

SteamGridDbCovers::SteamGridDbCovers(ui::Shell& shell, audio::SdlSoundPlayer& sounds,
                                     const artwork::SteamGridDb& source,
                                     const artwork::ArtworkStore& store, const Places& places,
                                     Hooks hooks)
    : shell_{shell}, hooks_{std::move(hooks)}, overrides_{places.chosen, places.fitted},
      fitter_{overrides_, source, store}, offers_{overrides_, source, places.thumbs},
      picker_{shell.screens().navigator(),
              shell.screens().chrome(),
              shell.screens().images(),
              sounds,
              CoverPickerController::Sources{offers_, overrides_},
              CoverPickerController::Hooks{[this](const std::string& id) {
                                               show(id);
                                           },
                                           [this](const std::string& text, bool error) {
                                               hooks_.say(text, error);
                                           },
                                           [this] {
                                               hooks_.connect();
                                           },
                                           [this](const std::string& id) {
                                               hooks_.match(id);
                                           }}} {
}

void SteamGridDbCovers::catalogChanged(const std::vector<library::Game>& games) {
    fitter_.request(games, homeTile);
}

void SteamGridDbCovers::apply(std::vector<library::ShelfItem>& shelf) const {
    overrides_.apply(shelf);
}

std::filesystem::path SteamGridDbCovers::replaced(const std::string& id) const {
    const library::Game* game = hooks_.original(id);
    return overrides_.shown(id, game == nullptr ? std::string_view{} : library::dataId(*game));
}

library::KeptData SteamGridDbCovers::keptData() const {
    return library::KeptData{[this](const std::string& id) {
                                 return overrides_.holdsChoice(id);
                             },
                             [this](const std::string& from, const std::string& to) {
                                 overrides_.copyChoice(from, to);
                             }};
}

void SteamGridDbCovers::coverArrived(const std::string& id, const std::filesystem::path& file) {
    const std::filesystem::path shown = replaced(id);
    shell_.grid().setArtwork(id, shown.empty() ? file : shown);
    if (const library::Game* game = hooks_.original(id)) {
        library::Game arrived = *game;
        arrived.artwork = file;
        fitter_.check(arrived, homeTile);
    }
}

void SteamGridDbCovers::coverMissing(const std::string& id) {
    if (const library::Game* game = hooks_.original(id)) {
        fitter_.check(*game, homeTile);
    }
}

void SteamGridDbCovers::choose(const std::string& id) {
    if (const library::Game* game = hooks_.original(id)) {
        picker_.open(*game);
    }
}

void SteamGridDbCovers::show(const std::string& id) {
    const std::filesystem::path shown = replaced(id);
    if (!shown.empty()) {
        shell_.grid().setArtwork(id, shown);
    } else if (const library::Game* game = hooks_.original(id);
               game != nullptr && !game->artwork.empty()) {
        shell_.grid().setArtwork(id, game->artwork);
    } else {
        return;
    }
    hooks_.shown(id);
}

void SteamGridDbCovers::service() {
    picker_.service();
    for (const artwork::FittedCover& fitted : fitter_.take()) {
        show(fitted.id);
    }
    std::vector<std::string> pending = fitter_.pendingGames();
    if (pending == pending_) {
        return;
    }
    for (const std::string& id : pending_) {
        if (std::ranges::find(pending, id) == pending.end()) {
            shell_.grid().artworkSettled(id);
        }
    }
    for (const std::string& id : pending) {
        shell_.grid().artworkStarted(id);
    }
    pending_ = std::move(pending);
}

} // namespace opensu::app
