// interface_input — the pad's buttons as the interface toolkit's actions, one rule both ways: the
// D-pad moves, A accepts, B goes back, X and Y are a widget's second and third actions, the
// shoulders step tabs and Start is the menu button, or Done while a text field is focused. A field
// typed into from the keyboard takes only the keys in `typingKeys`.
#pragma once

#include <array>
#include <optional>

#include "gpu2d/ui/input.hpp"

#include "gamepad/event.hpp"
#include "input/keyboard_bindings.hpp"

namespace opensu::ui {

/// The toolkit action `button` stands for, or nothing for a button the toolkit does not take;
/// `typing` while the focus is on a text field.
[[nodiscard]] std::optional<gpu2d::ui::Action> interfaceActionOf(gamepad::Button button,
                                                                 bool typing) noexcept;
/// The button the hint for `action` shows.
[[nodiscard]] gamepad::Button buttonOfInterfaceAction(gpu2d::ui::Action action) noexcept;

/// A key a field typed into from the keyboard takes, and the action it does.
struct TypingKey {
    bool input::TextInput::* pressed;
    gpu2d::ui::Action action;
};
inline constexpr std::array<TypingKey, 5> typingKeys{{
    {&input::TextInput::backspace, gpu2d::ui::Action::Erase},
    {&input::TextInput::up, gpu2d::ui::Action::Up},
    {&input::TextInput::down, gpu2d::ui::Action::Down},
    {&input::TextInput::enter, gpu2d::ui::Action::Submit},
    {&input::TextInput::escape, gpu2d::ui::Action::Back},
}};

} // namespace opensu::ui
