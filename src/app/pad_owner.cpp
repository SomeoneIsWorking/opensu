#include "pad_owner.hpp"

#include <algorithm>

namespace opensu::app {

using display::FocusHolder;
using gamepad::Claim;

gamepad::PadSourceOptions PadOwner::padsFor(const std::filesystem::path& dataDir, bool hidden) {
    if (!hidden) {
        return {};
    }
    return {.hiddenInputs = dataDir / "hidden-pads"};
}

bool PadOwner::listens(const Situation& situation) {
    const bool elsewhere =
        situation.holder == FocusHolder::Other || situation.holder == FocusHolder::BigPicture;
    return !elsewhere || (situation.game && situation.menuOver);
}

Claim PadOwner::claimFor(const Situation& situation) {
    if (!situation.grabs) {
        return situation.game ? Claim::Away : Claim::Free;
    }
    const bool ours = listens(situation);
    if (situation.game) {
        return ours || situation.holder == situation.serves ? Claim::Game : Claim::Away;
    }
    return ours && situation.followsFocus && situation.guardsSteam ? Claim::Shell : Claim::Free;
}

std::vector<std::string> PadOwner::hold(const input::ButtonMap& map) {
    if (situation_.game) {
        // A game's launch takes the pads back from Steam.
        release();
    }
    return take(map, FocusHolder::Game);
}

std::vector<std::string> PadOwner::take(const input::ButtonMap& map, FocusHolder serves) {
    chords_.setGame(map.chords());
    syncChords();
    situation_.game = true;
    situation_.serves = serves;
    std::vector<std::string> environment = pads_.hold(input::forwardMap(map, chords_.shell()));
    claim_ = Claim::Game;
    follow(situation_.followsFocus, situation_.holder, situation_.menuOver);
    return environment;
}

void PadOwner::lendToSteam(bool lend) {
    const bool lent = situation_.game && situation_.serves == FocusHolder::BigPicture;
    const bool wanted = lend && situation_.guardsSteam && situation_.grabs && !held();
    if (wanted == lent) {
        return;
    }
    if (wanted) {
        static_cast<void>(take(input::ButtonMap{}, FocusHolder::BigPicture));
        return;
    }
    release();
}

void PadOwner::restOutsideBigPicture() {
    const bool rest = situation_.game && situation_.serves == FocusHolder::BigPicture &&
                      situation_.followsFocus && situation_.holder != FocusHolder::BigPicture;
    if (rest != resting_) {
        resting_ = rest;
        pads_.setBlocked(rest);
    }
}

void PadOwner::release() {
    situation_.game = false;
    situation_.serves = FocusHolder::Game;
    resting_ = false;
    situation_.menuOver = false;
    claim_ = claimFor(situation_);
    pads_.release(claim_);
    chords_.setGame({});
    syncChords();
}

void PadOwner::setBlocked(bool blocked) {
    if (situation_.game) {
        pads_.setBlocked(blocked);
    }
}

void PadOwner::syncChords() {
    pads_.setChords(chords_.triggers());
}

void PadOwner::follow(bool followsFocus, FocusHolder holder, bool menuOver) {
    situation_.followsFocus = followsFocus;
    situation_.holder = holder;
    situation_.menuOver = menuOver;
    const Claim wanted = claimFor(situation_);
    if (wanted != claim_) {
        claim_ = wanted;
        pads_.claim(wanted);
    }
    restOutsideBigPicture();
}

std::vector<gamepad::Event> PadOwner::admit(std::vector<gamepad::Event> events) const {
    if (listens(situation_)) {
        return events;
    }
    std::erase_if(events, [](const gamepad::Event& event) {
        return event.kind == gamepad::Event::Kind::Button && event.button != gamepad::Button::Guide;
    });
    return events;
}

} // namespace opensu::app
