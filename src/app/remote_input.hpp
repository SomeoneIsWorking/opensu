// remote_input — the player's controls as remote play sends them to the host: the pad's controls
// held, from the shell's own pad events (a real pad and the control channel take one path), the
// first pad's sticks and triggers' travel, the pointer over the picture with the buttons held
// and the wheel's notches, and the keyboard's keys held. Each is sampled once a frame (`state`), so
// a press and its release in one frame are sent held for that frame. Guide is never one of them: it
// opens this openSU's own menu. The game's mapping is applied on the host
// (`stream::wire::PadMapping`).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "display/letterbox.hpp"
#include "gamepad/event.hpp"
#include "gamepad/pad_translator.hpp"
#include "shell_window.hpp"
#include "stream/stream_wire.hpp"
#include "touch/shooter_keys.hpp"

namespace opensu::app {

class RemoteInput {
  public:
    /// A control the player pressed or let go.
    void button(const gamepad::Event& event);
    /// The pads' sticks and triggers; the first pad's stand for the player's.
    void axes(const std::map<std::string, gamepad::PadAxes>& axes);
    /// The touch controls' sticks, which the host gets in place of a pad's while they lean.
    void touchSticks(const gamepad::Sticks& sticks);
    /// The Shooter's mouse buttons held, and its look moved `lookDp` across and down since the
    /// last frame, which moves the mouse by `touch::lookCountsPerDp` counts a dp.
    void touchMouse(std::uint8_t buttons, std::pair<float, float> lookDp);
    /// The keyboard's keys held this frame (USB usages). A key held when a menu took the controls,
    /// such as the Shift-Tab that closes it, stays here until it is let go.
    void keys(const std::vector<std::uint8_t>& held);
    /// The keys held now made a shortcut this openSU acted on: they stay here until let go.
    void keepKeys();
    /// The pointer's frame, with the picture drawn at `picture` in the window.
    void pointer(const PointerFrame& frame, const display::Placed& picture);
    /// Lets go of the pointer's buttons, as when fingers play the touch controls, not the picture.
    void releasePointer();
    /// Lets go of everything, as when a menu takes the controls.
    void release();

    /// The pad, keys and pointer as they stand this frame; the presses this frame ended are let go
    /// from the next one.
    struct State {
        stream::wire::PadState pad;
        stream::wire::KeyState keys;
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
    gamepad::Sticks touchSticks_{};
    std::uint8_t touchButtons_{0};
    /// The motion's part counts, kept until they add up to one.
    std::pair<float, float> moveRest_{};
    stream::wire::MouseState mouse_;
    /// The keys sent this frame, and those this openSU keeps until they are let go.
    std::vector<std::uint8_t> keysSent_;
    std::vector<std::uint8_t> keysKept_;
    /// A shortcut or a menu took the keys: whatever is held at the next `keys` is kept.
    bool keepHeld_{false};
    /// The pointer's buttons that went down this frame, held for it even when let go in it.
    std::uint8_t clicked_{0};
    /// The wheel's part notch, kept until it adds up to one.
    float wheelRest_{0};
};

} // namespace opensu::app
