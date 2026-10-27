// session_install_controller — the Install session mode flow. A question explains what session
// mode is and what gets installed; Install asks for the administrator password in a masked field
// with the on-screen keyboard up (or the physical one), which `host::SessionMode` hands to sudo on
// a worker while a card says it is installing; a wrong password says so and asks again; Cancel, or
// B, ends it. The password lives only in this controller and then in the worker's job, each copy
// wiped as soon as it is replaced or sudo has had it.
#pragma once

#include <functional>
#include <future>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "host/session_mode.hpp"
#include "keyboard_pad.hpp"

namespace opensu::app {

class SessionInstallController {
  public:
    /// What the flow tells the player: `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// The question, the password and the installing card go on `navigator`; `keyboard` comes up
    /// for the password.
    SessionInstallController(gpu2d::ui::Navigator& navigator, KeyboardPad& keyboard,
                             host::SessionMode& session, audio::SdlSoundPlayer& sounds, Say say)
        : navigator_{navigator}, keyboard_{keyboard}, session_{session}, sounds_{sounds},
          say_{std::move(say)} {
    }
    ~SessionInstallController();
    SessionInstallController(const SessionInstallController&) = delete;
    SessionInstallController& operator=(const SessionInstallController&) = delete;

    /// Asks with the explanation, unless installing cannot start from here (then says why).
    void begin();
    /// Whether the password is asked for or sudo is running the install, when no menu opens.
    [[nodiscard]] bool active() const {
        return typing() || installing();
    }
    /// Whether the password is asked for.
    [[nodiscard]] bool typing() const;
    /// Whether sudo is running the install.
    [[nodiscard]] bool installing() const noexcept {
        return job_.valid();
    }
    /// Takes the finished install, if there is one. Main loop only.
    void service();

  private:
    void openPassword();
    void cancel();
    void submit();
    /// The field changed: the old copy is wiped before it goes.
    void typed(std::string text);
    void finish(const host::InstallResult& result);
    /// The open screen `id` (the password card, the card saying it is installing), or null.
    [[nodiscard]] const gpu2d::ui::Screen* openScreen(std::string_view id) const;

    gpu2d::ui::Navigator& navigator_;
    KeyboardPad& keyboard_;
    host::SessionMode& session_;
    audio::SdlSoundPlayer& sounds_;
    Say say_;
    std::string password_;
    std::future<host::InstallResult> job_;
};

} // namespace opensu::app
