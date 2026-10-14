// pad_translator — turns one physical pad's evdev events into an Xbox 360-layout virtual pad's
// events and into opensu's controls.
//
// Codes follow the kernel's gamepad API (BTN_SOUTH, ABS_X, ABS_HAT0X, ...), which xpad,
// hid-playstation and hid-nintendo share; only ranges and d-pad style differ, so a pad's axes are
// rescaled to the virtual pad's and a d-pad reported as buttons becomes the hat. Analogue triggers
// are also L2 and R2 controls, with hysteresis. Guide is never forwarded: it belongs to opensu.
// What the virtual pad gets passes a `ChordGate`, so a bound chord's buttons stay with opensu,
// while the controls carry every edge. The game's mapping (`ForwardMap`) applies to the virtual
// pad only: a button presses its mapped buttons, a tap of a modifier taps them, and a game chord
// holds its buttons while its partner is down. Pure state, so it is tested without devices.
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "chord_gate.hpp"
#include "event.hpp"
#include "forward_map.hpp"

namespace opensu::gamepad {

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
/// How far an analogue trigger travels before it is L2 or R2, and how far back before it is
/// released; the gap keeps a trigger resting near one point from chattering.
inline constexpr float triggerPressAt = 0.5f;
inline constexpr float triggerReleaseAt = 0.3f;

class PadTranslator {
  public:
    explicit PadTranslator(PadCapabilities physical);

    /// The chords whose buttons are kept from the virtual pad.
    void setChords(ChordTriggers chords);
    /// What the virtual pad sends for each button and game chord from now on. The game chords are
    /// kept from the virtual pad as `setChords`'s are.
    void setForwardMap(ForwardMap map);

    /// Translates one physical event read at `at`: what the virtual pad should receive goes to
    /// `forward`, control presses and releases for opensu to `controls`.
    void translate(const PadEvent& in, std::vector<PadEvent>& forward, std::vector<Event>& controls,
                   ChordGate::Clock::time_point at);

    /// Events that bring the virtual pad to rest: every held key released, every axis centred.
    [[nodiscard]] std::vector<PadEvent> rest() const;
    /// Events that take the virtual pad from rest to the physical pad's axes. Held keys stay up
    /// until pressed again, so the press that closed the menu does not reach the game.
    [[nodiscard]] std::vector<PadEvent> resume() const;

  private:
    void forwardKey(std::uint16_t code, std::int32_t value, std::vector<PadEvent>& forward);
    void forwardAxis(std::uint16_t code, std::int32_t value, std::vector<PadEvent>& forward);
    /// Hears `edges` through the chord gate: each goes to `controls`, and the virtual pad follows
    /// what the gate lets pass.
    void gate(const std::vector<Event>& edges, std::vector<PadEvent>& forward,
              std::vector<Event>& controls, ChordGate::Clock::time_point at);
    /// The gate's chords: `setChords`'s and the forward map's.
    void applyTriggers();
    /// Gives the virtual pad what physical `button` presses, as the pad holds it, or at rest where
    /// the gate keeps it back.
    void sync(Button button, std::vector<PadEvent>& forward);
    /// Gives the virtual pad each of `outputs` as every source of it stands now.
    void syncOutputs(const std::set<Button>& outputs, std::vector<PadEvent>& forward);
    /// What `button` presses, in a report of its own, for a modifier let out as a tap.
    void tap(Button button, std::vector<PadEvent>& forward);
    /// Holds the buttons of game chord `chord` until its partner is released.
    void holdChord(const Chord& chord, std::vector<PadEvent>& forward);
    /// Whether the physical pad holds `button`; an analogue trigger past its press point.
    [[nodiscard]] bool held(Button button) const;
    /// Virtual button `out`'s value from every source of it: 0 or 1, or a trigger's travel.
    [[nodiscard]] std::int32_t level(Button out) const;
    void control(Button button, bool pressed, std::vector<Event>& controls);
    void hatControls(const PadEvent& axis, std::vector<Event>& controls);
    void stickControls(const PadEvent& axis, std::vector<Event>& controls);

    PadCapabilities physical_;
    ChordGate gate_;
    ChordTriggers chords_;
    ForwardMap map_;
    /// The buttons of each game chord played, by its partner, until the partner is released.
    std::map<Button, std::set<Button>> chordHeld_;
    /// The buttons a tap presses for the length of its report.
    std::set<Button> tapping_;
    /// The virtual pad's state as last forwarded, by (type, code).
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::int32_t> state_;
    /// The physical pad's keys held, by code.
    std::map<std::uint16_t, bool> keys_;
    /// The physical triggers on the virtual pad's scale, and whether each is L2 or R2 held.
    std::map<Button, std::int32_t> triggers_;
    std::map<Button, bool> triggerHeld_;
    /// D-pad buttons held, for pads that report the d-pad as buttons.
    std::map<std::uint16_t, bool> dpad_;
    /// Directions held from the hat and from the left stick, so each reports once.
    std::map<Button, bool> hatHeld_;
    std::map<Button, bool> stickHeld_;
};

} // namespace opensu::gamepad
