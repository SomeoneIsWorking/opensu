// forward_map — what a held pad's virtual pad sends for its buttons: each physical button's virtual
// buttons, and the game chords with the virtual buttons each holds while its partner is down. The
// shell builds it from the game's mapping (`input::forwardMap`); `PadTranslator` applies it to what
// the game reads, never to the shell's own controls. Sticks are not mapped.
#pragma once

#include <array>
#include <map>
#include <set>

#include "chord_gate.hpp"
#include "event.hpp"

namespace opensu::gamepad {

/// The buttons the virtual pad carries: what a mapping takes from and sends.
inline constexpr std::array padButtons{Button::A,  Button::B,    Button::X,      Button::Y,
                                       Button::L1, Button::R1,   Button::L2,     Button::R2,
                                       Button::L3, Button::R3,   Button::Select, Button::Start,
                                       Button::Up, Button::Down, Button::Left,   Button::Right};

struct ForwardMap {
    /// The virtual buttons a physical button presses; a button not listed presses itself, and an
    /// empty set sends nothing.
    std::map<Button, std::set<Button>> buttons;
    /// The virtual buttons each game chord holds while its partner is held.
    std::map<Chord, std::set<Button>> chords;

    /// What `button` presses on the virtual pad.
    [[nodiscard]] std::set<Button> outputOf(Button button) const;
    /// Whether `button` presses virtual button `out`.
    [[nodiscard]] bool sends(Button button, Button out) const;

    bool operator==(const ForwardMap&) const = default;
};

} // namespace opensu::gamepad
