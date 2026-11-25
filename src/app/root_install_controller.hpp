// root_install_controller — the flow that installs something with one root step
// (`host::RootInstall`): a question says what it is and what gets installed; Install hands the root
// step to a worker, which runs it through polkit, while a card says polkit is waiting for
// administrator authentication (asked in openSU by `polkit_prompt_controller`, or by the desktop's
// agent without openSU's). The outcome is said: installed, cancelled at authentication, or the
// reason it failed. Cancel, or B, at the question installs nothing. The root step waits on openSU's
// polkit agent while it asks, so ending the flow shuts the agent's authentication down before
// waiting for it.
#pragma once

#include <functional>
#include <future>
#include <string>
#include <string_view>
#include <utility>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "host/polkit_agent.hpp"
#include "host/root_install.hpp"

namespace opensu::app {

/// What the flow says, from the owner of what is installed.
struct RootInstallWords {
    /// The waiting card's screen id.
    std::string cardId;
    std::string title;
    /// The question: what it is and what gets installed.
    std::string explanation;
    /// Said once it is in place.
    std::string installed;
};

class RootInstallController {
  public:
    /// What the flow tells the player: `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// The question and the waiting card go on `navigator`; `agent`, openSU's polkit agent when
    /// there is one, outlives it.
    RootInstallController(gpu2d::ui::Navigator& navigator, host::RootInstall& install,
                          RootInstallWords words, host::PolkitAgent* agent,
                          audio::SdlSoundPlayer& sounds, Say say)
        : navigator_{navigator}, install_{install}, words_{std::move(words)}, agent_{agent},
          sounds_{sounds}, say_{std::move(say)} {
    }
    /// Shuts the agent's authentication down, then waits for the install to end.
    ~RootInstallController();
    RootInstallController(const RootInstallController&) = delete;
    RootInstallController& operator=(const RootInstallController&) = delete;
    RootInstallController(RootInstallController&&) = delete;
    RootInstallController& operator=(RootInstallController&&) = delete;

    /// Asks with the explanation, unless installing cannot start from here (then says why).
    void begin();
    /// Whether the root step is running, when no menu opens.
    [[nodiscard]] bool active() const noexcept {
        return installing();
    }
    [[nodiscard]] bool installing() const noexcept {
        return job_.valid();
    }
    /// Takes the finished install, if there is one; true when it is now in place. Main loop only.
    bool service();

  private:
    void start();
    bool finish(const host::InstallResult& result);
    /// The open screen `id`, or null.
    [[nodiscard]] const gpu2d::ui::Screen* openScreen(std::string_view id) const;

    gpu2d::ui::Navigator& navigator_;
    host::RootInstall& install_;
    RootInstallWords words_;
    host::PolkitAgent* agent_;
    audio::SdlSoundPlayer& sounds_;
    Say say_;
    // Last: an std::async future's destructor waits for the install, which uses the members above.
    std::future<host::InstallResult> job_;
};

} // namespace opensu::app
