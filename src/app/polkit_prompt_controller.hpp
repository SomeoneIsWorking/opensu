// polkit_prompt_controller — what polkit asks openSU's own agent for (`host::PolkitAgent`), drawn
// in openSU: one card over whatever shows for each authentication, from the moment polkit begins
// it until its verdict, named for the user and the action, with what PAM says as it says it. While
// a module works without the player (a fingerprint reader, a face, a phone) the card waits with
// Cancel; when PAM asks, a field is added, masked for a secret, typed on the pad's keyboard or the
// system's. Submit hands the answer to the agent and B cancels the authentication. The card renders
// the agent's state and keeps none of its own.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "host/polkit_agent.hpp"
#include "keyboard_pad.hpp"
#include "ui/authentication_screen.hpp"

namespace opensu::app {

class PolkitPromptController {
  public:
    PolkitPromptController(gpu2d::ui::Navigator& navigator, KeyboardPad& keyboard,
                           audio::SdlSoundPlayer& sounds, host::PolkitAgent& agent)
        : navigator_{navigator}, keyboard_{keyboard}, sounds_{sounds}, agent_{agent} {
    }

    /// Opens, updates or closes the card for the agent's authentication. Main loop only.
    void service();
    /// Whether the card is up.
    [[nodiscard]] bool asking() const;

  private:
    void open(const host::polkit::Prompt& prompt);
    void close();
    void submit();
    void dismiss();

    gpu2d::ui::Navigator& navigator_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    host::PolkitAgent& agent_;
    /// The authentication the card is for, as the agent last showed it; nothing while none is up.
    std::optional<host::polkit::Prompt> shown_;
    /// What the open card draws, kept by the card as it closes.
    std::shared_ptr<ui::AuthenticationCard> card_;
    std::string typed_;
};

} // namespace opensu::app
