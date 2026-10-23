// row_activation — the one rule for a list of settings-like rows: A opens a choice or slider,
// Left and Right change it while it is open, and A or B closes it; Left on a closed row is a back.
// The Settings screen, the Devices page and the quick menu all route their rows through it, so a
// row behaves the same wherever it is.
#pragma once

#include <cstdint>

#include "gamepad/event.hpp"
#include "settings_page.hpp"

namespace opensu::ui {

/// What a button does to the focused row.
enum class RowEffect : std::uint8_t {
    /// Nothing to the row.
    Nothing,
    /// The value changes by `step` (Left -1, Right 1).
    Change,
    /// A opened the focused row.
    Open,
    /// A or B closed the open row.
    Close,
    /// A entered the rows from the categories, or Right did, on a screen that keeps them apart.
    Enter,
    /// Left went back out of the rows, or B with no row open.
    Leave,
    /// A acted on the row without opening it (a switch, folder or action).
    Act,
};

struct RowStep {
    RowEffect effect{RowEffect::Nothing};
    int step{0};
};

/// The effect of `button` on the focused row. `editing` is whether A has opened it; `zoned` is
/// whether the screen keeps its categories apart from its rows (Settings, Devices), so Right and A
/// can enter them and Left can leave. The caller maps each effect to its own work: `Change` runs
/// the row with `step`, and `Enter`, `Open`, `Close`, `Leave` and `Act` its page's own calls.
[[nodiscard]] RowStep rowEffect(gamepad::Button button, const SettingsRow* focused, bool editing,
                                bool zoned);

} // namespace opensu::ui
