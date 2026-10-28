// sdl_pads — every controller SDL knows as a gamepad, read from SDL's gamepad events: the
// `PadSource` where openSU has no evdev. SDL delivers them while someone pumps events (the shell's
// window, the runner's presenter), so they are caught with an event watch. Nothing is grabbed and
// no virtual pad is made: every claim leaves the pads to everyone, and the game reads them itself.
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include "pad_source.hpp"

namespace opensu::gamepad {

/// One gamepad's edges: SDL's buttons and axes as openSU's controls.
class SdlPadState {
  public:
    /// The control SDL's `button` is, or `None` for one openSU has no name for.
    [[nodiscard]] static Button buttonOf(SDL_GamepadButton button) noexcept;

    /// A button went down or up.
    void button(SDL_GamepadButton button, bool pressed, std::vector<Event>& controls);
    /// An axis moved to `value` (sticks -32768 to 32767, triggers 0 to 32767).
    void axis(SDL_GamepadAxis axis, std::int16_t value, std::vector<Event>& controls);

    [[nodiscard]] const Sticks& sticks() const noexcept {
        return sticks_;
    }

  private:
    void set(Button button, bool on, std::map<Button, bool>& held, std::vector<Event>& controls);
    /// `set` for a left stick direction, its event marked as from the stick.
    void stick(Button button, bool on, std::map<Button, bool>& held, std::vector<Event>& controls);

    Sticks sticks_;
    std::map<Button, bool> buttonHeld_;
    std::map<Button, bool> stickHeld_;
    std::map<Button, bool> triggerHeld_;
};

class SdlPads final : public PadSource {
  public:
    /// Starts SDL's gamepad subsystem and reads every gamepad; `reads` false (a hidden run) reads
    /// none. Throws std::runtime_error when the subsystem cannot start.
    explicit SdlPads(bool reads);
    SdlPads(const SdlPads&) = delete;
    SdlPads& operator=(const SdlPads&) = delete;
    SdlPads(SdlPads&&) = delete;
    SdlPads& operator=(SdlPads&&) = delete;
    ~SdlPads() override;

    [[nodiscard]] bool grabs() const noexcept override {
        return false;
    }
    [[nodiscard]] std::vector<Event> takeEvents() override;
    [[nodiscard]] std::vector<PadInfo> connected() const override;
    [[nodiscard]] std::map<std::string, Sticks> sticks() const override;
    /// What SDL reads of the pad's battery (XInput's battery level, a PlayStation pad's report).
    [[nodiscard]] std::optional<device::BatteryStatus> battery(const PadInfo& pad) const override;
    std::vector<std::string> hold(ForwardMap map) override;
    void release(Claim after) override;
    void claim(Claim claim) override;
    void setBlocked(bool blocked) override;
    void setChords(ChordTriggers chords) override;
    std::vector<std::filesystem::path> keepVirtualPads(std::size_t count) override;

  private:
    struct Pad {
        PadInfo info;
        SDL_Gamepad* gamepad{nullptr};
        SdlPadState state;
    };

    /// Keeps SDL's gamepad events for `takeEvents`; called wherever SDL pushes them.
    static bool SDLCALL watch(void* self, SDL_Event* event);
    /// Turns one SDL event into openSU's, opening and closing pads as they come and go.
    void hear(const SDL_Event& event, std::vector<Event>& events);
    /// The pad opened, or nothing when it was already or cannot be.
    std::optional<PadInfo> open(SDL_JoystickID id);
    /// The pad closed, or nothing when it was not open.
    std::optional<PadInfo> close(SDL_JoystickID id);

    bool reads_;
    mutable std::mutex mutex_;
    /// SDL's gamepad events since the last `takeEvents`.
    std::vector<SDL_Event> heard_;
    /// By SDL's joystick id, which also orders them as they connected.
    std::map<SDL_JoystickID, Pad> pads_;
};

} // namespace opensu::gamepad
