// event — the controls and state changes every controller source reports, whatever reads the
// device.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace opensu::gamepad {

/// A named control, presentation-facing rather than device-facing.
enum class Button : std::uint8_t {
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
    /// Opens the search; no pad button is bound to it (the options panel has its entry).
    Search,
};

/// The control's lowercase spelling ("up", "l2", "select"), as the control channel and the settings
/// file write it; empty for `None`.
[[nodiscard]] std::string_view name(Button button);
/// The control spelled `text`, or `None` for another spelling.
[[nodiscard]] Button buttonNamed(std::string_view text) noexcept;

/// The prompt glyph for a control, as the reference labels its buttons.
[[nodiscard]] std::string_view prompt(Button button);

/// One input state change.
struct Event {
    enum class Kind : std::uint8_t {
        Button,
        Connected,
        Disconnected,
    };

    Kind kind{Kind::Button};
    Button button{Button::None};
    bool pressed{false};
    /// The controller's name, for Connected and Disconnected.
    std::string device;
    /// Which pad the event came from (`PadInfo::id`); empty for keys and the control channel.
    std::string source;
};

} // namespace opensu::gamepad
