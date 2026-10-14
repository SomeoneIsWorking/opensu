#include "pad_owner.hpp"

#include <algorithm>

namespace opensu::app {

using gamepad::Claim;
using session::FocusHolder;

std::filesystem::path PadOwner::inputDirectory(const std::filesystem::path& dataDir, bool hidden) {
    if (!hidden) {
        return "/dev/input";
    }
    const std::filesystem::path empty = dataDir / "hidden-pads";
    std::filesystem::create_directories(empty);
    return empty;
}

bool PadOwner::listens(const Situation& situation) {
    const bool elsewhere =
        situation.holder == FocusHolder::Other || situation.holder == FocusHolder::BigPicture;
    return !elsewhere || (situation.game && situation.menuOver);
}

Claim PadOwner::claimFor(const Situation& situation) {
    const bool ours = listens(situation);
    if (situation.game) {
        return ours ? Claim::Game : Claim::Away;
    }
    return ours && situation.followsFocus ? Claim::Shell : Claim::Free;
}

std::vector<std::string> PadOwner::hold(const input::ButtonMap& map) {
    chords_.setGame(map.chords());
    syncChords();
    situation_.game = true;
    std::vector<std::string> environment = pads_.hold(input::forwardMap(map, chords_.shell()));
    claim_ = Claim::Game;
    follow(situation_.followsFocus, situation_.holder, situation_.menuOver);
    return environment;
}

void PadOwner::release() {
    situation_.game = false;
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
