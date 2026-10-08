// event — the controls and state changes every controller source reports, whatever reads the
// device.
#pragma once

#include <string>
#include <string_view>

namespace iideck::gamepad {

/// A named control, presentation-facing rather than device-facing.
enum class Button {
    None,
    A,
    B,
    X,
    Y,
    L1,
    R1,
    L2,
    R2,
    L3,
    R3,
    Select,
    Start,
    Guide,
    Up,
    Down,
    Left,
    Right,
};

/// The prompt glyph for a control, as the reference labels its buttons.
[[nodiscard]] std::string_view prompt(Button button);

/// One input state change.
struct Event {
    enum class Kind {
        Button,
        Connected,
        Disconnected,
    };

    Kind kind{Kind::Button};
    Button button{Button::None};
    bool pressed{false};
    /// The controller's name, for Connected and Disconnected.
    std::string device;
};

} // namespace iideck::gamepad
