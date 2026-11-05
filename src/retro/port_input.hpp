// port_input — what a core's input queries read: a RetroPad per port, from this machine's pads
// (`Joypad`) or from a netplay session (`NetplayLink`).
#pragma once

#include <cstdint>

namespace opensu::retro {

class PortInput {
  public:
    PortInput() = default;
    PortInput(const PortInput&) = default;
    PortInput& operator=(const PortInput&) = default;
    PortInput(PortInput&&) = default;
    PortInput& operator=(PortInput&&) = default;
    virtual ~PortInput() = default;

    /// One button's state on `port`, or with RETRO_DEVICE_ID_JOYPAD_MASK every button as bits.
    [[nodiscard]] virtual std::int16_t state(unsigned port, unsigned id) const = 0;
    /// One stick axis on `port` (RETRO_DEVICE_INDEX_ANALOG_LEFT or _RIGHT,
    /// RETRO_DEVICE_ID_ANALOG_X or _Y) from -32768 to 32767, y down.
    [[nodiscard]] virtual std::int16_t analog(unsigned port, unsigned index, unsigned id) const = 0;
};

} // namespace opensu::retro
