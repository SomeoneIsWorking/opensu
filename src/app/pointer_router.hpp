// pointer_router — maps the mouse onto the shell's existing actions.
//
// Hover moves the same focus the D-pad moves, a left click is focus plus the A press, a right click
// asks for the context menu of what is under it (it is never Back) and the wheel steps the way the
// D-pad does. Nothing here knows geometry: the host says what is under the pointer.
#pragma once

#include <cstddef>
#include <optional>

#include "gamepad/event.hpp"
#include "input/last_device.hpp"
#include "raylib.h"
#include "ui/pointer_target.hpp"

namespace opensu::app {

/// The pointer as one frame saw it.
struct PointerFrame {
    /// The pointer inside the window, or nothing.
    std::optional<Vector2> point;
    /// How far it moved since the last frame.
    Vector2 delta{};
    /// Wheel notches this frame: positive is away from the player.
    float wheel{};
    /// Buttons that went down this frame.
    bool left{false};
    bool right{false};
    bool middle{false};
};

/// Reads the pointer from raylib. Main loop only.
[[nodiscard]] PointerFrame readPointerFrame();

/// What the router asks of the shell.
class PointerHost {
  public:
    virtual ~PointerHost() = default;

    /// What is under `point`, or nothing when the pointer is not driving the shell.
    virtual ui::PointerTarget pointAt(std::optional<Vector2> point) = 0;
    /// Moves the focus the pad moves to `target`.
    virtual void focus(const ui::PointerTarget& target) = 0;
    /// A button pressed as the pad would press it.
    virtual void press(gamepad::Button button) = 0;
    /// A click on a dock item.
    virtual void activateSection(library::Section section) = 0;
    /// A click on a launcher's badge in the top bar.
    virtual void selectLauncher(library::Source source) = 0;
    /// A click on a level of the breadcrumb trail: go back to it.
    virtual void activateCrumb(std::size_t index) = 0;
    /// A click on the icon size slider at `level`.
    virtual void chooseIconSize(int level) = 0;
    /// A right click on `target`: its context menu opens, an open one closes, and a target
    /// without a menu does nothing.
    virtual void contextMenu(const ui::PointerTarget& target) = 0;
    /// One wheel step: `steps` is 1 towards the end of the list or page, -1 towards its start.
    virtual void scroll(int steps) = 0;
};

class PointerRouter {
  public:
    PointerRouter(input::LastDevice& device, PointerHost& host) noexcept
        : device_{device}, host_{host} {
    }

    /// Notes the device and routes one frame of the pointer.
    void route(const PointerFrame& frame);

  private:
    /// A left click on `target`.
    void click(const ui::PointerTarget& target);

    input::LastDevice& device_;
    PointerHost& host_;
};

} // namespace opensu::app
