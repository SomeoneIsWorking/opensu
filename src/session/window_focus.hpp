// window_focus — Gamescope's focus properties read as `display::FocusedWindow`.
//
// Gamescope names the focused window in GAMESCOPE_FOCUSED_WINDOW on the root and lists each window
// it could focus in GAMESCOPE_FOCUSABLE_WINDOWS as (window, app id, pid) triples. Outside Steam
// mode (`-e`, which openSU does not pass) a window's app id is its own X id, except a window that
// carries STEAM_BIGPICTURE, whose app id is Steam's, 769 (steamcompmgr.cpp:5825-5839, 6930-6939).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "display/focus.hpp"
#include "launch/process_id.hpp"

namespace opensu::session {

/// One entry of GAMESCOPE_FOCUSABLE_WINDOWS.
struct FocusableWindow {
    std::uint64_t window{};
    std::uint32_t appId{};
    launch::ProcessId pid{};

    bool operator==(const FocusableWindow&) const = default;
};

/// The triples of a GAMESCOPE_FOCUSABLE_WINDOWS value; a trailing partial triple is dropped.
[[nodiscard]] std::vector<FocusableWindow> parseFocusable(std::span<const long> values);

/// `window` with its app id and pid from `focusable`.
[[nodiscard]] display::FocusedWindow describeFocus(std::uint64_t window,
                                                   const std::vector<FocusableWindow>& focusable);

} // namespace opensu::session
