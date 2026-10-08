// pad_translator — turns one physical pad's evdev events into an Xbox 360-layout virtual pad's
// events and into iideck's controls.
//
// Codes follow the kernel's gamepad API (BTN_SOUTH, ABS_X, ABS_HAT0X, ...), which xpad,
// hid-playstation and hid-nintendo share; only ranges and d-pad style differ, so a pad's axes are
// rescaled to the virtual pad's and a d-pad reported as buttons becomes the hat. Guide is never
// forwarded: it belongs to iideck. Pure state, so it is tested without devices.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "event.hpp"

namespace iideck::gamepad {

/// One evdev event, without its timestamp.
struct PadEvent {
    std::uint16_t type{};
    std::uint16_t code{};
    std::int32_t value{};

    friend bool operator==(const PadEvent&, const PadEvent&) = default;
};

struct AxisRange {
    std::int32_t min{};
    std::int32_t max{};
};

/// What a physical pad reports: its keys and its absolute axes with their ranges.
struct PadCapabilities {
    std::set<std::uint16_t> keys;
    std::map<std::uint16_t, AxisRange> axes;

    /// A device is a gamepad when it has the south face button and a left stick.
    [[nodiscard]] bool isGamepad() const;
};

/// The virtual pad's ranges (xpad's): sticks signed 16-bit, triggers 8-bit.
inline constexpr AxisRange virtualStick{-32768, 32767};
inline constexpr AxisRange virtualTrigger{0, 255};

class PadTranslator {
  public:
    explicit PadTranslator(PadCapabilities physical);

    /// Translates one physical event: what the virtual pad should receive goes to `forward`,
    /// control presses and releases for iideck to `controls`.
    void translate(const PadEvent& in, std::vector<PadEvent>& forward,
                   std::vector<Event>& controls);

    /// Events that bring the virtual pad to rest: every held key released, every axis centred.
    [[nodiscard]] std::vector<PadEvent> rest() const;
    /// Events that take the virtual pad from rest to the physical pad's axes. Held keys stay up
    /// until pressed again, so the press that closed the menu does not reach the game.
    [[nodiscard]] std::vector<PadEvent> resume() const;

  private:
    void forwardKey(std::uint16_t code, std::int32_t value, std::vector<PadEvent>& forward);
    void forwardAxis(std::uint16_t code, std::int32_t value, std::vector<PadEvent>& forward);
    void control(Button button, bool pressed, std::vector<Event>& controls);
    void hatControls(const PadEvent& axis, std::vector<Event>& controls);
    void stickControls(const PadEvent& axis, std::vector<Event>& controls);

    PadCapabilities physical_;
    /// The virtual pad's state as last forwarded, by (type, code).
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::int32_t> state_;
    /// D-pad buttons held, for pads that report the d-pad as buttons.
    std::map<std::uint16_t, bool> dpad_;
    /// Directions held from the hat and from the left stick, so each reports once.
    std::map<Button, bool> hatHeld_;
    std::map<Button, bool> stickHeld_;
};

} // namespace iideck::gamepad
