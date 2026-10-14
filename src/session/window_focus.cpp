#include "window_focus.hpp"

#include <algorithm>

namespace opensu::session {

std::vector<FocusableWindow> parseFocusable(std::span<const long> values) {
    std::vector<FocusableWindow> windows;
    for (std::size_t i = 0; i + 2 < values.size(); i += 3) {
        windows.push_back(FocusableWindow{static_cast<std::uint64_t>(values[i]),
                                          static_cast<std::uint32_t>(values[i + 1]),
                                          static_cast<pid_t>(values[i + 2])});
    }
    return windows;
}

FocusedWindow describeFocus(std::uint64_t window, const std::vector<FocusableWindow>& focusable) {
    FocusedWindow focused{.window = window};
    const auto found = std::ranges::find(focusable, window, &FocusableWindow::window);
    if (window != 0 && found != focusable.end()) {
        focused.appId = found->appId;
        focused.pid = found->pid;
    }
    return focused;
}

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

} // namespace opensu::session
