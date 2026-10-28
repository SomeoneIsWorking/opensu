#include "cover_picker_controller.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "cover_picker_screen.hpp"

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
    title_ = "Choose artwork: " + game.title;
    own_ = game.artwork;
    listing_.reset();
    saving_ = false;
    offers_.list(game);
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
        offers_.chooseOwn(gameId_);
        break;
    case Entry::Kind::Offer:
        if (!listing_ || entry.offer >= listing_->offers.size()) {
            return;
        }
        offers_.choose(gameId_, listing_->offers[entry.offer].cover);
        break;
    }
    saving_ = true;
    show();
}

void CoverPickerController::service() {
    if (std::optional<artwork::CoverListing> listing = offers_.takeListing();
        listing && listing->gameId == gameId_ && isOpen()) {
        listing_ = std::move(listing);
        show();
    }
    for (const artwork::CoverChoice& choice : offers_.takeChoices()) {
        if (!choice.kept) {
            hooks_.say("cannot keep the cover: " + choice.error, true);
        } else {
            hooks_.changed(choice.gameId);
        }
        if (choice.gameId == gameId_ && isOpen()) {
            saving_ = false;
            if (choice.kept) {
                close();
            } else {
                show();
            }
        }
    }
}

void CoverPickerController::show() {
    picks_.clear();
    entries_.clear();
    const std::filesystem::path replaced = overrides_.shown(gameId_);
    picks_.push_back(Pick{"Now", replaced.empty() ? own_ : replaced});
    entries_.push_back(Entry{Entry::Kind::Now});
    if (!replaced.empty() && !own_.empty()) {
        picks_.push_back(Pick{"Own", own_});
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
    return listing_ &&
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
        return "Connect SteamGridDB for more covers";
    case Status::Refused:
        return "SteamGridDB refused the key";
    case Status::Unreachable:
        return "SteamGridDB cannot be reached";
    case Status::Listed:
        break;
    }
    if (listing_->offers.empty()) {
        return "SteamGridDB has no covers for this game";
    }
    return std::to_string(listing_->offers.size()) + " covers from SteamGridDB";
}

} // namespace opensu::app
