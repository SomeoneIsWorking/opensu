#include "steamgriddb_covers.hpp"

#include <algorithm>
#include <span>
#include <string_view>
#include <utility>

namespace opensu::app {
namespace {

// Home's cells are square (ui::HomeLayout), so a cover of another shape is cropped there.
constexpr artwork::CoverShape homeTile = artwork::CoverShape::Square;

std::string_view statusWord(artwork::SteamGridDb::Covers::Status status) {
    using Status = artwork::SteamGridDb::Covers::Status;
    switch (status) {
    case Status::Disconnected:
        return lan::wire::coversDisconnected;
    case Status::Refused:
        return lan::wire::coversRefused;
    case Status::Unreachable:
        return lan::wire::coversUnreachable;
    case Status::Listed:
        break;
    }
    return lan::wire::coversListed;
}

/// `listing` as a guest reads it, its thumbnails put in `thumbs`.
lan::wire::CoverOffersAnswer answerOf(const artwork::CoverListing& listing,
                                      lan::CoverDesk::Thumbs& thumbs) {
    lan::wire::CoverOffersAnswer answer{.game = listing.gameId,
                                        .status = std::string{statusWord(listing.status)},
                                        .complete = listing.complete};
    for (const artwork::CoverOffer& offer : listing.offers) {
        std::string token;
        if (!offer.thumb.empty()) {
            token = lan::CoverDesk::tokenOf(offer.thumb);
            thumbs.emplace(token, offer.thumb);
        }
        answer.offers.push_back(lan::wire::OfferedCover{.id = offer.cover.id,
                                                        .width = offer.cover.width,
                                                        .height = offer.cover.height,
                                                        .thumb = std::move(token)});
    }
    return answer;
}

} // namespace

SteamGridDbCovers::SteamGridDbCovers(ui::Shell& shell, audio::SdlSoundPlayer& sounds,
                                     const artwork::SteamGridDb& source,
                                     const artwork::ArtworkStore& store, const Places& places,
                                     Hooks hooks)
    : shell_{shell}, hooks_{std::move(hooks)}, overrides_{places.chosen, places.fitted},
      fitter_{overrides_, source, store}, offers_{overrides_, source, places.thumbs},
      hosted_{hooks_.askHost}, guestOffers_{overrides_, source, places.thumbs},
      picker_{shell.screens().navigator(),
              shell.screens().chrome(),
              shell.screens().images(),
              sounds,
              CoverPickerController::Sources{offers_, hosted_, overrides_},
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

std::vector<library::Game> SteamGridDbCovers::shown(std::vector<library::Game> games) const {
    for (library::Game& game : games) {
        if (std::filesystem::path cover = overrides_.shown(game.id, library::dataId(game));
            !cover.empty()) {
            game.artwork = std::move(cover);
        }
    }
    return games;
}

void SteamGridDbCovers::serve(lan::CoverDesk& desk) {
    if (std::optional<std::string> wanted = desk.takeWanted()) {
        guestListing_.reset();
        guestGame_ = std::move(*wanted);
        if (const library::Game* game = hooks_.original(guestGame_)) {
            guestOffers_.list(*game);
        } else {
            desk.publish(
                lan::wire::CoverOffersAnswer{.game = guestGame_,
                                             .status = std::string{lan::wire::coversUnreachable},
                                             .complete = true},
                {});
        }
    }
    if (std::optional<artwork::CoverListing> listing = guestOffers_.takeListing();
        listing && listing->gameId == guestGame_) {
        guestListing_ = std::move(listing);
        lan::CoverDesk::Thumbs thumbs;
        lan::wire::CoverOffersAnswer answer = answerOf(*guestListing_, thumbs);
        const library::Game* game = hooks_.original(guestGame_);
        if (game != nullptr && !game->artwork.empty() && !replaced(game->id).empty()) {
            answer.own = lan::CoverDesk::tokenOf(game->artwork);
            thumbs.emplace(answer.own, game->artwork);
        }
        desk.publish(std::move(answer), std::move(thumbs));
    }
    for (const lan::wire::CoverPick& pick : desk.takePicks()) {
        const library::Game* game = hooks_.original(pick.game);
        const auto offers = guestListing_ && pick.game == guestGame_
                                ? std::span<const artwork::CoverOffer>{guestListing_->offers}
                                : std::span<const artwork::CoverOffer>{};
        const auto offer = std::ranges::find_if(offers, [&pick](const artwork::CoverOffer& each) {
            return pick.cover == each.cover.id;
        });
        if (game == nullptr || (pick.cover && offer == offers.end())) {
            desk.picked(pick.game, "that cover is no longer offered");
            continue;
        }
        const std::string kept{library::dataId(*game)};
        guestPicks_[kept] = pick.game;
        if (pick.cover) {
            guestOffers_.choose(kept, offer->cover);
        } else {
            guestOffers_.chooseOwn(kept);
        }
    }
    for (const artwork::CoverChoice& choice : guestOffers_.takeChoices()) {
        const auto pick = guestPicks_.find(choice.gameId);
        if (pick == guestPicks_.end()) {
            continue;
        }
        desk.picked(pick->second, choice.kept ? std::string{lan::wire::coverKept} : choice.error);
        if (choice.kept) {
            show(pick->second);
            hooks_.shared();
        }
        guestPicks_.erase(pick);
    }
}

void SteamGridDbCovers::service() {
    picker_.service();
    const std::vector<artwork::FittedCover> fitted = fitter_.take();
    for (const artwork::FittedCover& cover : fitted) {
        show(cover.id);
    }
    if (!fitted.empty()) {
        hooks_.shared();
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
