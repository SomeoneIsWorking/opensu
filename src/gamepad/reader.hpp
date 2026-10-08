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

#include "event.hpp"
#include "raylib.h"

namespace iideck::gamepad {

/// Watches every controller the system reports.
///
/// The reader has no background thread: it is polled once per frame, which is
/// what raylib's own input model expects and keeps ordering deterministic.
class Reader {
  public:
    /// The number of controller slots checked.
    static constexpr int maxGamepads = 4;

    /// A device must report at least this many axes to be treated as a
    /// controller.
    static constexpr int minAxes = 2;

    /// Accepts only devices whose name contains one of these. Empty accepts
    /// every device SDL reports.
    void setNameFilter(std::vector<std::string> needles);

    /// Reports whether a device's name passes the filter.
    [[nodiscard]] bool nameAccepted(const char* name) const;

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
    std::vector<std::string> nameFilter_;
};

/// Maps a raylib gamepad button to a named control.
[[nodiscard]] Button fromGamepadButton(int button);

} // namespace iideck::gamepad