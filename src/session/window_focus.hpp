// window_focus — whose window Gamescope has focused, and what that means for openSU.
//
// Gamescope names the focused window in GAMESCOPE_FOCUSED_WINDOW on the root and lists each window
// it could focus in GAMESCOPE_FOCUSABLE_WINDOWS as (window, app id, pid) triples. Outside Steam
// mode (`-e`, which openSU does not pass) a window's app id is its own X id, except a window that
// carries STEAM_BIGPICTURE, whose app id is Steam's, 769 (steamcompmgr.cpp:5825-5839, 6930-6939).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sys/types.h>

namespace opensu::session {

/// The app id Gamescope gives a window that carries STEAM_BIGPICTURE.
inline constexpr std::uint32_t steamAppId = 769;

/// One entry of GAMESCOPE_FOCUSABLE_WINDOWS.
struct FocusableWindow {
    std::uint64_t window{};
    std::uint32_t appId{};
    pid_t pid{};

    bool operator==(const FocusableWindow&) const = default;
};

/// The window Gamescope has focused; `window` 0 when none is.
struct FocusedWindow {
    std::uint64_t window{};
    /// From the focusable list; empty for a window the list leaves out (an override-redirect,
    /// a 1x1, or a skip-taskbar window that is not fullscreen).
    std::optional<std::uint32_t> appId;
    std::optional<pid_t> pid;

    bool operator==(const FocusedWindow&) const = default;
};

/// Whose window has the focus.
enum class FocusHolder : std::uint8_t {
    /// openSU's own window, or nothing focused.
    OpenSU,
    /// The game openSU launched.
    Game,
    /// Steam's Big Picture.
    BigPicture,
    /// Anything else: a Steam dialog, a window another program put up.
    Other,
};

/// Who openSU is.
struct FocusParties {
    pid_t self{};
    /// openSU's own X window, 0 before it has one.
    std::uint64_t ownWindow{};
    /// The running game's processes; empty while none runs.
    std::vector<pid_t> game;
};

/// The triples of a GAMESCOPE_FOCUSABLE_WINDOWS value; a trailing partial triple is dropped.
[[nodiscard]] std::vector<FocusableWindow> parseFocusable(std::span<const long> values);

/// `window` with its app id and pid from `focusable`.
[[nodiscard]] FocusedWindow describeFocus(std::uint64_t window,
                                          const std::vector<FocusableWindow>& focusable);

/// Whose window `focused` is. A window whose pid Gamescope does not list counts as openSU's side
/// (the game's while one runs), so a game's unlisted popup never takes the pads from it.
[[nodiscard]] FocusHolder holderOf(const FocusedWindow& focused, const FocusParties& parties);

/// Where Gamescope's focus is read from: GamescopeWindows, or a fake in tests.
class FocusSource {
  public:
    virtual ~FocusSource() = default;

    /// The focused window when it may have changed since the last call; the first call always
    /// answers.
    [[nodiscard]] virtual std::optional<FocusedWindow> takeChange() = 0;
};

} // namespace opensu::session
