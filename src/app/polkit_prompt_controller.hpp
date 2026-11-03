// polkit_prompt_controller — the password polkit asks openSU's own agent for (`host::PolkitAgent`),
// drawn in openSU: an entry card over whatever shows, named for the action and the user, the
// field masked for a secret, typed on the pad's keyboard or the system's. Submit hands the answer
// to the agent and B cancels the authentication. A refused password comes back as a new card that
// says so; the card goes once polkit has its verdict.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "host/polkit_agent.hpp"
#include "keyboard_pad.hpp"

namespace opensu::app {

class PolkitPromptController {
  public:
    PolkitPromptController(gpu2d::ui::Navigator& navigator, KeyboardPad& keyboard,
                           audio::SdlSoundPlayer& sounds, host::PolkitAgent& agent)
        : navigator_{navigator}, keyboard_{keyboard}, sounds_{sounds}, agent_{agent} {
    }

    /// Opens, renews or closes the card for the agent's question. Main loop only.
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
    /// The question the card is for; nothing while none is up.
    std::optional<std::uint64_t> serial_;
    /// The last question answered or dismissed, which the agent may still show for a moment.
    std::uint64_t handled_{};
    std::string typed_;
};

} // namespace opensu::app
