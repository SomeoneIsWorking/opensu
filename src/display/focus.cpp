#include "focus.hpp"

#include <algorithm>

namespace opensu::display {

FocusHolder holderOf(const FocusedWindow& focused, const FocusParties& parties) {
    if (focused.window == 0 || focused.window == parties.ownWindow) {
        return FocusHolder::OpenSU;
    }
    if (focused.appId == steamAppId) {
        return FocusHolder::BigPicture;
    }
    const bool gameRuns = !parties.game.empty();
    if (!focused.pid) {
        return gameRuns ? FocusHolder::Game : FocusHolder::OpenSU;
    }
    if (*focused.pid == parties.self) {
        return FocusHolder::OpenSU;
    }
    if (std::ranges::find(parties.game, *focused.pid) != parties.game.end()) {
        return FocusHolder::Game;
    }
    return FocusHolder::Other;
}

} // namespace opensu::display
