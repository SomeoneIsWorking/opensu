// interface_input — the pad's buttons as the interface toolkit's actions, one rule both ways: the
// D-pad moves, A accepts, B goes back, X and Y are a widget's second and third actions, the
// shoulders step tabs and Start is the menu button.
#pragma once

#include <optional>

#include "gpu2d/ui/input.hpp"

#include "gamepad/event.hpp"

namespace opensu::ui {

/// The toolkit action `button` stands for, or nothing for a button the toolkit does not take.
[[nodiscard]] std::optional<gpu2d::ui::Action> interfaceActionOf(gamepad::Button button) noexcept;
/// The button the hint for `action` shows.
[[nodiscard]] gamepad::Button buttonOfInterfaceAction(gpu2d::ui::Action action) noexcept;

} // namespace opensu::ui
