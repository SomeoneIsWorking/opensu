// session_install_controller — the Install session mode flow. A question explains what session
// mode is and what gets installed; Install hands the root step to `host::SessionMode` on a worker,
// which runs it through polkit, while a card says polkit is waiting for the administrator password
// (asked in openSU by `polkit_prompt_controller`, or by the desktop's agent without openSU's). The
// outcome is said: installed, cancelled at the password, or the reason it failed. Cancel, or B, at
// the question installs nothing.
#pragma once

#include <functional>
#include <future>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "host/session_mode.hpp"

namespace opensu::app {

class SessionInstallController {
  public:
    /// What the flow tells the player: `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// The question and the waiting card go on `navigator`.
    SessionInstallController(gpu2d::ui::Navigator& navigator, host::SessionMode& session,
                             audio::SdlSoundPlayer& sounds, Say say)
        : navigator_{navigator}, session_{session}, sounds_{sounds}, say_{std::move(say)} {
    }

    /// Asks with the explanation, unless installing cannot start from here (then says why).
    void begin();
    /// Whether the root step is running, when no menu opens.
    [[nodiscard]] bool active() const noexcept {
        return installing();
    }
    [[nodiscard]] bool installing() const noexcept {
        return job_.valid();
    }
    /// Takes the finished install, if there is one. Main loop only.
    void service();

  private:
    void start();
    void finish(const host::InstallResult& result);
    /// The open screen `id`, or null.
    [[nodiscard]] const gpu2d::ui::Screen* openScreen(std::string_view id) const;

    gpu2d::ui::Navigator& navigator_;
    host::SessionMode& session_;
    audio::SdlSoundPlayer& sounds_;
    Say say_;
    // Last: an std::async future's destructor waits for the install, which uses the members above.
    std::future<host::InstallResult> job_;
};

} // namespace opensu::app
