#include "row_activation.hpp"

namespace opensu::ui {

RowStep rowEffect(gamepad::Button button, const SettingsRow* focused, bool editing, bool zoned) {
    switch (button) {
    case gamepad::Button::Left:
    case gamepad::Button::Right: {
        const int step = button == gamepad::Button::Left ? -1 : 1;
        if (editing && focused != nullptr) {
            return RowStep{RowEffect::Change, step};
        }
        if (focused == nullptr) {
            // In the categories: Right enters the rows, Left has nothing to leave.
            return step > 0 && zoned ? RowStep{RowEffect::Enter, 0} : RowStep{};
        }
        // In the rows: Left goes back, whatever the row is; Right does nothing until A opens it.
        return step < 0 ? RowStep{RowEffect::Leave, 0} : RowStep{};
    }
    case gamepad::Button::A:
        if (editing) {
            return RowStep{RowEffect::Close, 0};
        }
        if (focused == nullptr) {
            return zoned ? RowStep{RowEffect::Enter, 0} : RowStep{};
        }
        return RowStep{rowOpens(focused->kind) ? RowEffect::Open : RowEffect::Act, 0};
    case gamepad::Button::B:
        return RowStep{editing ? RowEffect::Close : RowEffect::Leave, 0};
    default:
        return {};
    }
}

} // namespace opensu::ui
