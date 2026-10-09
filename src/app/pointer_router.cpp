#include "pointer_router.hpp"

#include <variant>

namespace opensu::app {
namespace {

/// Whether resting on `target` focuses it. A page arrow turns the page, so only a click does.
struct FocusesOnHover {
    bool operator()(const ui::OnTile&) const {
        return true;
    }
    bool operator()(const ui::OnLayoutCard&) const {
        return true;
    }
    bool operator()(const ui::OnMenuItem&) const {
        return true;
    }
    template <class Other> bool operator()(const Other&) const {
        return false;
    }
};

} // namespace

PointerFrame readPointerFrame() {
    PointerFrame frame;
    if (IsCursorOnScreen()) {
        frame.point = GetMousePosition();
    }
    frame.delta = GetMouseDelta();
    frame.wheel = GetMouseWheelMove();
    frame.left = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    frame.right = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    frame.middle = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    return frame;
}

void PointerRouter::route(const PointerFrame& frame) {
    const bool moved = frame.delta.x != 0.0f || frame.delta.y != 0.0f;
    device_.notePointer(frame.delta.x, frame.delta.y,
                        frame.left || frame.right || frame.middle || frame.wheel != 0.0f);
    const bool active = device_.current() == input::Device::KeyboardMouse;
    const ui::PointerTarget target = host_.pointAt(active ? frame.point : std::optional<Vector2>{});
    // Only a pointer that moved takes focus, so a keyboard or pad move is not undone by a pointer
    // resting where the focus used to be.
    if (active && moved && std::visit(FocusesOnHover{}, target)) {
        host_.focus(target);
    }
    if (frame.left && active) {
        click(target);
    }
    if (frame.right) {
        host_.press(gamepad::Button::B);
    }
    if (frame.wheel != 0.0f) {
        host_.scroll(frame.wheel < 0.0f ? 1 : -1);
    }
}

void PointerRouter::click(const ui::PointerTarget& target) {
    if (const auto* dock = std::get_if<ui::OnDock>(&target)) {
        host_.activateSection(dock->section);
    } else if (const auto* button = std::get_if<ui::OnPanelButton>(&target)) {
        host_.press(button->button);
    } else if (std::holds_alternative<ui::OnPage>(target)) {
        host_.focus(target);
    } else if (!std::holds_alternative<std::monostate>(target)) {
        host_.focus(target);
        host_.press(gamepad::Button::A);
    }
}

} // namespace opensu::app
