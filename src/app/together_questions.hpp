// together_questions — what play together asks the player here, one dialog at a time, over
// whatever is shown, a running game included: a guest's ask to join the game that runs here
// ("Let <guest> join?") and a host's invitation to its game ("Play with <host>?"), answered into
// `RemotePlay`, and a paired openSU's ask to close the game here ("Close <game> for <peer>?"),
// answered into `CloseAsks`; the answer that changes nothing focused. The dialog closes by itself
// when the question lapses.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "close_asks.hpp"
#include "remote_play.hpp"

namespace opensu::app {

class TogetherQuestions {
  public:
    TogetherQuestions(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                      RemotePlay& remote, CloseAsks& closes)
        : navigator_{navigator}, sounds_{sounds}, remote_{remote}, closes_{closes} {
    }

    /// Shows the question as the asks stand. Every frame, main loop only.
    void service();

  private:
    /// The question a dialog shows.
    struct Question {
        enum class Kind : std::uint8_t { Join, Invitation, Close };
        Kind kind{Kind::Join};
        lan::Peer peer;
        std::string title;

        bool operator==(const Question&) const = default;
    };

    [[nodiscard]] std::optional<Question> next() const;
    void answer(bool allow);
    void close();

    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    RemotePlay& remote_;
    CloseAsks& closes_;
    /// The question the dialog shows, and the dialog's title while it is open.
    std::optional<Question> asked_;
    std::string title_;
};

} // namespace opensu::app
