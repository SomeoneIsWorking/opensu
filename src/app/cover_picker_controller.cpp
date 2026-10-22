#include "cover_picker_controller.hpp"

#include <utility>

namespace opensu::app {

void CoverPickerController::open(const library::Game& game) {
    gameId_ = game.id;
    title_ = "Choose artwork: " + game.title;
    own_ = game.artwork;
    listing_.reset();
    saving_ = false;
    offers_.list(game);
    sounds_.play(audio::Effect::OpenContextMenu);
    show();
}

void CoverPickerController::close() {
    picker_.close();
    gameId_.clear();
    listing_.reset();
    sounds_.play(audio::Effect::Close);
}

void CoverPickerController::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Left:
        if (picker_.move(-1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Right:
        if (picker_.move(1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        choose();
        break;
    case gamepad::Button::B:
        close();
        break;
    default:
        break;
    }
}

void CoverPickerController::choose() {
    if (saving_ || picker_.focus() >= entries_.size()) {
        return;
    }
    const Entry entry = entries_[picker_.focus()];
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
    sounds_.play(audio::Effect::Navigation);
    show();
}

void CoverPickerController::service() {
    if (std::optional<artwork::CoverListing> listing = offers_.takeListing();
        listing && listing->gameId == gameId_ && picker_.isOpen()) {
        listing_ = std::move(listing);
        show();
    }
    for (const artwork::CoverChoice& choice : offers_.takeChoices()) {
        if (!choice.kept) {
            hooks_.say("cannot keep the cover: " + choice.error, true);
        } else {
            hooks_.changed(choice.gameId);
        }
        if (choice.gameId == gameId_ && picker_.isOpen()) {
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
    std::vector<ui::CoverPick> picks;
    entries_.clear();
    const std::filesystem::path replaced = overrides_.shown(gameId_);
    picks.push_back(ui::CoverPick{"Now", replaced.empty() ? own_ : replaced});
    entries_.push_back(Entry{Entry::Kind::Now});
    if (!replaced.empty() && !own_.empty()) {
        picks.push_back(ui::CoverPick{"Own", own_});
        entries_.push_back(Entry{Entry::Kind::Own});
    }
    if (listing_) {
        for (std::size_t i = 0; i < listing_->offers.size(); ++i) {
            const artwork::CoverOffer& offer = listing_->offers[i];
            picks.push_back(ui::CoverPick{std::to_string(offer.cover.width) + "x" +
                                              std::to_string(offer.cover.height),
                                          offer.thumb});
            entries_.push_back(Entry{Entry::Kind::Offer, i});
        }
    }
    if (picker_.isOpen()) {
        picker_.update(std::move(picks), status());
    } else {
        picker_.open(title_, std::move(picks), status());
    }
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
        return "Connect SteamGridDB in Settings > Artwork for more covers";
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
