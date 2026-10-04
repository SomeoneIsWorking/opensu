// gamepad — reads controllers through raylib, which sits on SDL and already
// knows the button and axis layout every mainstream pad uses.
//
// Reading the device here rather than through a webview keeps navigation
// edge-driven and leaves haptics one call away.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"

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
        Axis,
        Connected,
        Disconnected,
    };

    Kind kind{Kind::Button};
    Button button{Button::None};
    bool pressed{false};
    /// "lx", "ly", "rx" or "ry", normalised to -1..1.
    const char* axis{nullptr};
    float value{0.0f};
    /// The controller's name, for the status line.
    const char* device{""};
};

/// Watches every controller the system reports.
///
/// The reader has no background thread: it is polled once per frame, which is
/// what raylib's own input model expects and keeps ordering deterministic.
class Reader {
public:
    /// The number of controller slots checked, including keyboards.
    static constexpr int maxGamepads = 4;

    /// Collects this frame's state changes into `events`.
    void poll(std::vector<Event>& events);

    /// True when at least one controller is connected.
    [[nodiscard]] bool anyConnected() const;

    /// The name of a connected controller, or nothing.
    [[nodiscard]] std::optional<std::string> primaryName() const;

    /// Plays a rumble effect. False when no controller is connected; raylib
    /// applies the effect to whichever pad is in the slot.
    bool rumble(float strong, float weak, float seconds) const;

private:
    /// Per-slot held state, so a press is reported once and a release once.
    struct Slot {
        bool connected{false};
        std::array<bool, 32> held{};
        float axisLX{0.0f};
        float axisLY{0.0f};
    };

    std::array<Slot, maxGamepads> slots_{};
    /// Directions synthesised from the left stick, so the shell sees sticks and
    /// hats as the same control.
    std::array<bool, 4> directionHeld_{};
};

/// Maps a raylib gamepad button to a named control.
[[nodiscard]] Button fromGamepadButton(int button);

} // namespace iideck::gamepad