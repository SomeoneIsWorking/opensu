#include "focus_follow.hpp"

#include <unistd.h>

#include "lucent/log.h"

namespace opensu::app {

using session::FocusHolder;

void FocusFollow::attach(std::unique_ptr<session::FocusSource> source, std::uint64_t ownWindow) {
    source_ = std::move(source);
    ownWindow_ = ownWindow;
}

void FocusFollow::service(bool game) {
    if (!source_) {
        return;
    }
    std::optional<session::FocusedWindow> change = source_->takeChange();
    if (change) {
        focused_ = *change;
    } else if (game == game_ || !focused_) {
        return;
    }
    game_ = game;
    const session::FocusParties parties{
        .self = ::getpid(),
        .ownWindow = ownWindow_,
        .game = game ? hooks_.gamePids() : std::vector<pid_t>{},
    };
    const FocusHolder was = holder_;
    holder_ = session::holderOf(*focused_, parties);
    if (holder_ == was) {
        return;
    }
    lucent::info("session", "focus on 0x{:x}: {}", focused_->window,
                 holder_ == FocusHolder::OpenSU       ? "openSU"
                 : holder_ == FocusHolder::Game       ? "the game"
                 : holder_ == FocusHolder::BigPicture ? "Big Picture"
                                                      : "another window");
    if (holder_ == FocusHolder::BigPicture) {
        closer_ = std::jthread{[close = hooks_.closeBigPicture] {
            close();
        }};
        hooks_.takeFocus();
    }
}

bool FocusFollow::guideReturns(bool inGame) const noexcept {
    return !inGame && (holder_ == FocusHolder::Other || holder_ == FocusHolder::BigPicture);
}

} // namespace opensu::app
