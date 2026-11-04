// remote_input — the player's controls as remote play sends them to the host: the pad's controls
// held, from the shell's own pad events (a real pad and the control channel take one path), the
// first pad's sticks, and the pointer over the picture with its buttons and wheel. Guide is never
// one of them: it opens this openSU's own menu.
#pragma once

#include <map>
#include <string>

#include "display/letterbox.hpp"
#include "gamepad/event.hpp"
#include "gamepad/pad_translator.hpp"
#include "shell_window.hpp"
#include "stream/stream_wire.hpp"

namespace opensu::app {

class RemoteInput {
  public:
    /// A control the player pressed or let go.
    void button(const gamepad::Event& event);
    /// The pads' sticks; the first pad's stand for the player's.
    void sticks(const std::map<std::string, gamepad::Sticks>& sticks);
    /// The pointer's frame, with the picture drawn at `picture` in the window.
    void pointer(const PointerFrame& frame, const display::Placed& picture);
    /// Lets go of everything, as when a menu takes the controls.
    void release();

    [[nodiscard]] const stream::wire::PadState& pad() const noexcept {
        return pad_;
    }
    [[nodiscard]] const stream::wire::MouseState& mouse() const noexcept {
        return mouse_;
    }

  private:
    stream::wire::PadState pad_;
    stream::wire::MouseState mouse_;
    /// Stick directions held, as the left stick at full travel when no pad reports its sticks.
    std::uint32_t stickHeld_{0};
    bool padSticks_{false};
};

} // namespace opensu::app
