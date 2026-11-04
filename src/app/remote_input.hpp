// remote_input — the player's controls as remote play sends them to the host: the pad's controls
// held, from the shell's own pad events (a real pad and the control channel take one path), the
// first pad's sticks and triggers' travel, and the pointer over the picture with the buttons held
// and the wheel's notches. Each is sampled once a frame (`state`), so a press and its release in
// one frame are sent held for that frame. Guide is never one of them: it opens this openSU's own
// menu. The game's mapping is applied on the host (`stream::wire::PadMapping`).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
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
    /// The pads' sticks and triggers; the first pad's stand for the player's.
    void axes(const std::map<std::string, gamepad::PadAxes>& axes);
    /// The pointer's frame, with the picture drawn at `picture` in the window.
    void pointer(const PointerFrame& frame, const display::Placed& picture);
    /// Lets go of everything, as when a menu takes the controls.
    void release();

    /// The pad and pointer as they stand this frame; the presses this frame ended are let go
    /// from the next one.
    struct State {
        stream::wire::PadState pad;
        stream::wire::MouseState mouse;
    };
    [[nodiscard]] State take();

  private:
    /// Buttons held (bit `gamepad::Button` value), and those let go this frame after a press in it.
    std::uint32_t held_{0};
    std::uint32_t endedThisFrame_{0};
    std::uint32_t pressedThisFrame_{0};
    /// Stick directions held, as the left stick at full travel when no pad reports its axes.
    std::uint32_t stickHeld_{0};
    /// The first pad's axes, when a pad reports them.
    std::optional<gamepad::PadAxes> axes_;
    stream::wire::MouseState mouse_;
    /// The pointer's buttons that went down this frame, held for it even when let go in it.
    std::uint8_t clicked_{0};
    /// The wheel's part notch, kept until it adds up to one.
    float wheelRest_{0};
};

} // namespace opensu::app
