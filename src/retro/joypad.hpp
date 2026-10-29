// joypad — the pads and the keyboard as one RetroPad, with one fixed mapping: its buttons
// (RETRO_DEVICE_JOYPAD), positions following the Xbox layout (A bottom is libretro's B) so every
// core sees the same buttons, and its two sticks (RETRO_DEVICE_ANALOG).
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

class Joypad {
  public:
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

    /// One button's state, or with RETRO_DEVICE_ID_JOYPAD_MASK every button as bits.
    [[nodiscard]] std::int16_t state(unsigned id) const;
    /// One stick axis (RETRO_DEVICE_INDEX_ANALOG_LEFT or _RIGHT, RETRO_DEVICE_ID_ANALOG_X or _Y)
    /// from -32768 to 32767, y down: of every pad, the one furthest from centre.
    [[nodiscard]] std::int16_t analog(unsigned index, unsigned id) const;

  private:
    static constexpr std::size_t buttons = 16;

    [[nodiscard]] std::uint16_t mask() const;

    /// Presses per button per pad: a pad's hat and stick report the same direction separately.
    std::map<std::string, std::array<int, buttons>, std::less<>> pads_;
    std::map<std::string, gamepad::Sticks, std::less<>> sticks_;
    std::array<bool, buttons> keys_{};
};

} // namespace opensu::retro
