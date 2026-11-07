#include "cover_picker_controller.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "cover_picker_screen.hpp"
#include "library/match_pick.hpp"
#include "library/titles.hpp"

namespace opensu::app {

const gpu2d::ui::Screen* CoverPickerController::picker() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const CoverPickerScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool CoverPickerController::isOpen() const {
    return picker() != nullptr;
}

void CoverPickerController::open(const library::Game& game) {
    if (isOpen()) {
        return;
    }
    gameId_ = game.id;
    keptId_ = library::dataId(game);
    title_ = "Choose artwork: " + game.title;
    ownCover_ = game.artwork;
    const bool hosted = game.source == library::Source::Remote;
    host_ = hosted ? game.hostName : std::string{};
    offers_ = hosted ? &hosted_ : &own_;
    matchable_ = library::matchKindOf(game) != library::MatchKind::None;
    listing_.reset();
    saving_ = false;
    offers_->list(game);
    sounds_.play(audio::Effect::OpenContextMenu);
    show();
    navigator_.push(std::make_unique<CoverPickerScreen>(*this, chrome_, images_));
}

void CoverPickerController::close() {
    const gpu2d::ui::Screen* open = picker();
    if (open == nullptr) {
        return;
    }
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*open);
}

void CoverPickerController::connect() {
    if (!disconnected()) {
        return;
    }
    close();
    hooks_.connect();
}

void CoverPickerController::match() {
    if (!matchable_) {
        return;
    }
    close();
    hooks_.match(gameId_);
}

void CoverPickerController::choose(std::size_t index) {
    if (saving_ || index >= entries_.size()) {
        return;
    }
    const Entry entry = entries_[index];
    switch (entry.kind) {
    case Entry::Kind::Now:
        close();
        return;
    case Entry::Kind::Own:
        offers_->chooseOwn(keptId_);
        break;
    case Entry::Kind::Offer:
        if (!listing_ || entry.offer >= listing_->offers.size()) {
            return;
        }
        offers_->choose(keptId_, listing_->offers[entry.offer].cover);
        break;
    }
    saving_ = true;
    show();
}

void CoverPickerController::service() {
    if (std::optional<artwork::CoverListing> listing = offers_->takeListing();
        listing && listing->gameId == gameId_ && isOpen()) {
        listing_ = std::move(listing);
        show();
    }
    for (artwork::CoverSource* source : {&own_, &hosted_}) {
        for (const artwork::CoverChoice& choice : source->takeChoices()) {
            if (!choice.kept) {
                hooks_.say("cannot keep the cover: " + choice.error, true);
            } else {
                hooks_.changed(choice.gameId == keptId_ ? gameId_ : choice.gameId);
            }
            if (choice.gameId == keptId_ && isOpen()) {
                saving_ = false;
                if (choice.kept) {
                    close();
                } else {
                    show();
                }
            }
        }
    }
}

void CoverPickerController::show() {
    picks_.clear();
    entries_.clear();
    const std::filesystem::path replaced = overrides_.shown(gameId_, keptId_);
    picks_.push_back(Pick{"Now", replaced.empty() ? ownCover_ : replaced});
    entries_.push_back(Entry{Entry::Kind::Now});
    const std::filesystem::path own = listing_ && !listing_->own.empty() ? listing_->own
                                      : replaced.empty()                 ? std::filesystem::path{}
                                                                         : ownCover_;
    if (!own.empty()) {
        picks_.push_back(Pick{"Own", own});
        entries_.push_back(Entry{Entry::Kind::Own});
    }
    if (listing_) {
        for (std::size_t i = 0; i < listing_->offers.size(); ++i) {
            const artwork::CoverOffer& offer = listing_->offers[i];
            picks_.push_back(
                Pick{std::to_string(offer.cover.width) + "x" + std::to_string(offer.cover.height),
                     offer.thumb});
            entries_.push_back(Entry{Entry::Kind::Offer, i});
        }
    }
}

bool CoverPickerController::disconnected() const {
    using Status = artwork::SteamGridDb::Covers::Status;
    return host_.empty() && listing_ &&
           (listing_->status == Status::Disconnected || listing_->status == Status::Refused);
}

std::string CoverPickerController::status() const {
    using Status = artwork::SteamGridDb::Covers::Status;
    if (saving_) {
        return "Keeping the cover...";
    }
    if (!listing_) {
        return "Asking SteamGridDB...";
    }
    switch (listing_->status) {
    case Status::Disconnected:
        return host_.empty() ? "Connect SteamGridDB for more covers"
                             : "Connect SteamGridDB on " + host_ + " for more covers";
    case Status::Refused:
        return "SteamGridDB refused the key";
    case Status::Unreachable:
        return listing_->error.empty() || host_.empty() ? "SteamGridDB cannot be reached"
                                                        : host_ + ": " + listing_->error;
    case Status::Listed:
        break;
    }
    if (listing_->offers.empty()) {
        return "SteamGridDB has no covers for this game";
    }
    return std::to_string(listing_->offers.size()) + " covers from SteamGridDB";
}

} // namespace opensu::app
