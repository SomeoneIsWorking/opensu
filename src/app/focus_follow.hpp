// focus_follow — follows the window Gamescope has focused (`session::FocusSource`) and answers it.
// Steam's Big Picture is never wanted: whenever it takes the focus, by Guide, a steam:// link or
// Steam itself, it is closed through the Steam client and the shell's window is put back in front.
// Guide pressed while another window is in front of the shell brings the shell back. Without a
// source (outside Gamescope, a hidden run) openSU always holds the focus.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <sys/types.h>

#include "session/window_focus.hpp"

namespace opensu::app {

class FocusFollow {
  public:
    struct Hooks {
        /// The running game's processes.
        std::function<std::vector<pid_t>()> gamePids;
        /// Asks Steam to leave Big Picture; called on a thread of its own, as Steam may be slow.
        std::function<void()> closeBigPicture;
        /// Puts the shell's window in front (`GameScreen::takeFocus`); over a running game it does
        /// nothing, as Gamescope gives the game back the focus once Big Picture is gone.
        std::function<void()> takeFocus;
    };

    explicit FocusFollow(Hooks hooks) : hooks_{std::move(hooks)} {
    }

    /// Starts following `source`; `ownWindow` is openSU's X window.
    void attach(std::unique_ptr<session::FocusSource> source, std::uint64_t ownWindow);
    [[nodiscard]] bool following() const noexcept {
        return source_ != nullptr;
    }

    /// Reads a focus change and answers it. `game` is whether a game is launching or running;
    /// its start and end also count as a change.
    void service(bool game);

    [[nodiscard]] session::FocusHolder holder() const noexcept {
        return holder_;
    }
    /// What Guide does now: true when it puts the shell back in front, rather than toggling the
    /// Guide menu. Over a game the menu shows as the overlay whatever is focused.
    [[nodiscard]] bool guideReturns(bool inGame) const noexcept;

  private:
    Hooks hooks_;
    std::unique_ptr<session::FocusSource> source_;
    std::uint64_t ownWindow_{};
    std::optional<session::FocusedWindow> focused_;
    bool game_{false};
    session::FocusHolder holder_{session::FocusHolder::OpenSU};
    /// Last, so a close in flight ends before the hooks it calls go.
    std::jthread closer_;
};

} // namespace opensu::app
