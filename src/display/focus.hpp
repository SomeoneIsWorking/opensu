// focus — whose window the display has focused, and what that means for openSU. Where the focus is
// read from is the platform's (`FocusSource`; Gamescope's root properties on Linux).
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "launch/process_id.hpp"

namespace opensu::display {

/// The app id Gamescope gives a window that carries STEAM_BIGPICTURE.
inline constexpr std::uint32_t steamAppId = 769;

/// The focused window; `window` 0 when none is.
struct FocusedWindow {
    std::uint64_t window{};
    /// Empty for a window the platform does not describe (on Gamescope: one left out of its
    /// focusable list, an override-redirect, a 1x1, or a skip-taskbar window that is not
    /// fullscreen).
    std::optional<std::uint32_t> appId;
    std::optional<launch::ProcessId> pid;

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
    launch::ProcessId self{};
    /// openSU's own window, 0 before it has one.
    std::uint64_t ownWindow{};
    /// The running game's processes; empty while none runs.
    std::vector<launch::ProcessId> game;
};

/// Whose window `focused` is. A window whose pid is unknown counts as openSU's side (the game's
/// while one runs), so a game's unlisted popup never takes the pads from it.
[[nodiscard]] FocusHolder holderOf(const FocusedWindow& focused, const FocusParties& parties);

/// Where the focus is read from: the platform's, or a fake in tests.
class FocusSource {
  public:
    virtual ~FocusSource() = default;

    /// The focused window when it may have changed since the last call; the first call always
    /// answers.
    [[nodiscard]] virtual std::optional<FocusedWindow> takeChange() = 0;
};

} // namespace opensu::display
