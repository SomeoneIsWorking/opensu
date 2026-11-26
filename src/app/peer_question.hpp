// peer_question — the dialog a paired openSU's ask puts to the player here (a pairing, a profile
// copy, the Steam sign-in): Decline focused beside the choice that grants it, closed again when the
// ask goes away unanswered. Main loop only.
#pragma once

#include <functional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"

namespace opensu::app {

class PeerQuestion {
  public:
    PeerQuestion(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds)
        : navigator_{navigator}, sounds_{sounds} {
    }

    /// Asks `title` with `body`, closing a question asked before: `answered(true)` once `grant`
    /// is chosen, `answered(false)` for Decline.
    void ask(std::string title, const std::string& body, std::string grant,
             std::function<void(bool)> answered);
    /// Closes the dialog while it is open.
    void close();

  private:
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    /// The open dialog's title; empty when none is open.
    std::string title_;
};

} // namespace opensu::app
