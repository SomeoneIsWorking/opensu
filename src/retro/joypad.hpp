// joypad — the pads and the keyboard as RetroPads, with one fixed mapping: their buttons
// (RETRO_DEVICE_JOYPAD), positions following the Xbox layout (A bottom is libretro's B) so every
// core sees the same buttons, and their two sticks (RETRO_DEVICE_ANALOG). For one player every
// source is port 0; for more, each pad takes the lowest free port when it is first heard from and
// keeps it until it leaves, and the keyboard is port 0.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>

#include "gamepad/event.hpp"
#include "gamepad/pad_translator.hpp"

namespace opensu::retro {

/// The most ports a launch spreads pads over.
inline constexpr unsigned maxPlayers = 8;

class Joypad {
  public:
    /// The ports pads are spread over; at least one.
    explicit Joypad(unsigned players = 1) : players_{players == 0 ? 1 : players} {
    }

    /// The RETRO_DEVICE_ID_JOYPAD_* a control drives, or nothing (Guide).
    [[nodiscard]] static std::optional<unsigned> idOf(gamepad::Button button);
    /// The RETRO_DEVICE_ID_JOYPAD_* an SDL scancode drives, or nothing.
    [[nodiscard]] static std::optional<unsigned> idOfKey(int scancode);

    /// A press, release or pad leaving from the pads' reader.
    void apply(const gamepad::Event& event);
    /// The pad named `source` holds `held` alone: what it held before is let go.
    void hold(const std::string& source, std::span<const gamepad::Button> held);
    /// A key going down or up, by SDL scancode.
    void key(int scancode, bool down);
    /// Where the sticks of the pad named `source` stand now.
    void sticks(const std::string& source, const gamepad::Sticks& sticks);

    /// One button's state on `port`, or with RETRO_DEVICE_ID_JOYPAD_MASK every button as bits.
    [[nodiscard]] std::int16_t state(unsigned port, unsigned id) const;
    /// One stick axis on `port` (RETRO_DEVICE_INDEX_ANALOG_LEFT or _RIGHT,
    /// RETRO_DEVICE_ID_ANALOG_X or _Y) from -32768 to 32767, y down: of the port's pads, the one
    /// furthest from centre.
    [[nodiscard]] std::int16_t analog(unsigned port, unsigned index, unsigned id) const;
    /// The port `source` plays on, or nothing when every port is taken.
    [[nodiscard]] std::optional<unsigned> portOf(const std::string& source) const;

  private:
    static constexpr std::size_t buttons = 16;

    [[nodiscard]] std::uint16_t mask(unsigned port) const;
    /// `source`'s port, given to it now when it has none.
    std::optional<unsigned> join(const std::string& source);
    void release(const std::string& source);

    unsigned players_;
    /// Each pad's port while there is more than one.
    std::map<std::string, unsigned, std::less<>> ports_;

    /// Presses per button per pad: a pad's hat and stick report the same direction separately.
    std::map<std::string, std::array<int, buttons>, std::less<>> pads_;
    std::map<std::string, gamepad::Sticks, std::less<>> sticks_;
    std::array<bool, buttons> keys_{};
};

} // namespace opensu::retro
