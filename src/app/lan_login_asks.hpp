// lan_login_asks — a paired openSU's ask for this openSU's Steam sign-in, put to the player here on
// the shell's loop as a question (Decline focused, Allow); the node's `lan::LoginCopies` hands the
// sign-in to the asker once allowed. The asker's side is the integration wizard's Copy from <name>.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "lan/peer_book.hpp"
#include "lan_service.hpp"
#include "peer_question.hpp"

namespace opensu::app {

class LanLoginAsks {
  public:
    /// Tells the player something; `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    LanLoginAsks(LanService& lan, gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                 Say say)
        : lan_{lan}, question_{navigator, sounds}, say_{std::move(say)} {
    }

    /// Shows the question as the asks stand. Every frame, main loop only.
    void service();

  private:
    void answer(bool allow);

    LanService& lan_;
    /// The openSU asking now, and its question.
    std::optional<lan::Peer> asked_;
    PeerQuestion question_;
    Say say_;
};

} // namespace opensu::app
