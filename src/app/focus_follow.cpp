#include "focus_follow.hpp"

#include "launch/process_id.hpp"
#include "lucent/log.h"

namespace opensu::app {

using display::FocusHolder;

void FocusFollow::attach(std::unique_ptr<display::FocusSource> source, std::uint64_t ownWindow) {
    source_ = std::move(source);
    ownWindow_ = ownWindow;
}

void FocusFollow::service(bool game) {
    if (!source_) {
        return;
    }
    std::optional<display::FocusedWindow> change = source_->takeChange();
    if (change) {
        focused_ = *change;
    } else if (game == game_ || !focused_) {
        return;
    }
    game_ = game;
    const display::FocusParties parties{
        .self = launch::ownProcessId(),
        .ownWindow = ownWindow_,
        .game = game ? hooks_.gamePids() : std::vector<launch::ProcessId>{},
    };
    const FocusHolder was = holder_;
    holder_ = display::holderOf(*focused_, parties);
    if (holder_ == was) {
        return;
    }
    lucent::info("session", "focus on 0x{:x}: {}", focused_->window,
                 holder_ == FocusHolder::OpenSU       ? "openSU"
                 : holder_ == FocusHolder::Game       ? "the game"
                 : holder_ == FocusHolder::BigPicture ? "Big Picture"
                                                      : "another window");
    if (holder_ == FocusHolder::BigPicture && closesBigPicture_) {
        requestClose();
        hooks_.takeFocus();
    }
}

void FocusFollow::requestClose() {
    {
        const std::lock_guard lock{closeMutex_};
        closePending_ = true;
    }
    closeWanted_.notify_one();
    if (!closer_.joinable()) {
        closer_ = std::jthread{[this](const std::stop_token& stop) {
            closeOnRequest(stop);
        }};
    }
}

void FocusFollow::closeOnRequest(const std::stop_token& stop) {
    std::unique_lock lock{closeMutex_};
    while (closeWanted_.wait(lock, stop, [this] {
        return closePending_;
    })) {
        closePending_ = false;
        lock.unlock();
        hooks_.closeBigPicture();
        lock.lock();
    }
}

bool FocusFollow::guideReturns(bool inGame) const noexcept {
    return !inGame && (holder_ == FocusHolder::Other || holder_ == FocusHolder::BigPicture);
}

} // namespace opensu::app
