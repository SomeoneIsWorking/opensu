// joypad — the pads' controls and the keyboard as one RETRO_DEVICE_JOYPAD, with one fixed mapping:
// positions follow the Xbox layout (A bottom is libretro's B), so every core sees the same buttons.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "gamepad/event.hpp"

namespace opensu::retro {

class Joypad {
  public:
    /// The RETRO_DEVICE_ID_JOYPAD_* a control drives, or nothing (Guide).
    [[nodiscard]] static std::optional<unsigned> idOf(gamepad::Button button);
    /// The RETRO_DEVICE_ID_JOYPAD_* an SDL scancode drives, or nothing.
    [[nodiscard]] static std::optional<unsigned> idOfKey(int scancode);

    /// A press, release or pad leaving from the pads' reader.
    void apply(const gamepad::Event& event);
    /// A key going down or up, by SDL scancode.
    void key(int scancode, bool down);

    /// One button's state, or with RETRO_DEVICE_ID_JOYPAD_MASK every button as bits.
    [[nodiscard]] std::int16_t state(unsigned id) const;

  private:
    static constexpr std::size_t buttons = 16;

    [[nodiscard]] std::uint16_t mask() const;

    /// Presses per button per pad: a pad's hat and stick report the same direction separately.
    std::map<std::string, std::array<int, buttons>, std::less<>> pads_;
    std::array<bool, buttons> keys_{};
};

} // namespace opensu::retro
