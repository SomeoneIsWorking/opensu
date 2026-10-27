#include "pointer_router.hpp"

#include <variant>

namespace opensu::app {
namespace {

/// Whether resting on `target` focuses it. A page arrow turns the page, so only a click does.
struct FocusesOnHover {
    bool operator()(const ui::OnTile&) const {
        return true;
    }
    bool operator()(const ui::OnKeyboardKey&) const {
        return true;
    }
    template <class Other> bool operator()(const Other&) const {
        return false;
    }
};

} // namespace

void PointerRouter::route(const PointerFrame& frame) {
    const bool moved = frame.delta.x != 0.0f || frame.delta.y != 0.0f;
    device_.notePointer(frame.delta.x, frame.delta.y,
                        frame.left || frame.right || frame.middle || frame.wheel != 0.0f);
    const bool active = device_.current() == input::Device::KeyboardMouse;
    const ui::PointerTarget target =
        host_.pointAt(active ? frame.point : std::optional<ui::Point>{});
    if (std::holds_alternative<ui::OnScreens>(target)) {
        host_.screens(frame);
        return;
    }
    // Only a pointer that moved takes focus, so a keyboard or pad move is not undone by a pointer
    // resting where the focus used to be.
    if (active && moved && std::visit(FocusesOnHover{}, target)) {
        host_.focus(target);
    }
    if (frame.left && active) {
        click(target);
    }
    if (frame.right) {
        host_.contextMenu(target);
    }
    if (frame.wheel != 0.0f) {
        host_.scroll(frame.wheel < 0.0f ? 1 : -1);
    }
}

void PointerRouter::click(const ui::PointerTarget& target) {
    if (const auto* dock = std::get_if<ui::OnDock>(&target)) {
        host_.activateSection(dock->section);
    } else if (const auto* launcher = std::get_if<ui::OnLauncher>(&target)) {
        if (launcher->downloading) {
            host_.openDownloads();
        } else {
            host_.selectLauncher(launcher->source);
        }
    } else if (const auto* crumb = std::get_if<ui::OnCrumb>(&target)) {
        host_.activateCrumb(crumb->index);
    } else if (std::holds_alternative<ui::OnPage>(target)) {
        // A page turns.
        host_.focus(target);
    } else if (!std::holds_alternative<std::monostate>(target)) {
        host_.focus(target);
        host_.press(gamepad::Button::A);
    }
}

} // namespace opensu::app
