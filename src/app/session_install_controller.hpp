// session_install_controller — the Install session mode flow. A question explains what session
// mode is and what gets installed; Install hands the root step to `host::SessionMode` on a worker,
// which runs it through polkit, while a card says polkit is waiting for administrator
// authentication (asked in openSU by `polkit_prompt_controller`, or by the desktop's agent without
// openSU's). The outcome is said: installed, cancelled at authentication, or the reason it failed.
// Cancel, or B, at the question installs nothing. The root step waits on openSU's polkit agent
// while it asks, so ending the flow shuts the agent's authentication down before waiting for it.
#pragma once

#include <functional>
#include <future>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "host/polkit_agent.hpp"
#include "host/session_mode.hpp"

namespace opensu::app {

class SessionInstallController {
  public:
    /// What the flow tells the player: `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// The question and the waiting card go on `navigator`; `agent`, openSU's polkit agent when
    /// there is one, outlives it.
    SessionInstallController(gpu2d::ui::Navigator& navigator, host::SessionMode& session,
                             host::PolkitAgent* agent, audio::SdlSoundPlayer& sounds, Say say)
        : navigator_{navigator}, session_{session}, agent_{agent}, sounds_{sounds},
          say_{std::move(say)} {
    }
    /// Shuts the agent's authentication down, then waits for the install to end.
    ~SessionInstallController();
    SessionInstallController(const SessionInstallController&) = delete;
    SessionInstallController& operator=(const SessionInstallController&) = delete;
    SessionInstallController(SessionInstallController&&) = delete;
    SessionInstallController& operator=(SessionInstallController&&) = delete;

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
    host::PolkitAgent* agent_;
    audio::SdlSoundPlayer& sounds_;
    Say say_;
    // Last: an std::async future's destructor waits for the install, which uses the members above.
    std::future<host::InstallResult> job_;
};

} // namespace opensu::app
