// together_questions — what play together asks the player here, one dialog at a time, over
// whatever is shown, a running game included: a guest's ask to join the game that runs here
// ("Let <guest> join?") and a host's invitation to its game ("Play with <host>?"), Decline focused,
// answered into `RemotePlay`. The dialog closes by itself when the question lapses.
#pragma once

#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "remote_play.hpp"

namespace opensu::app {

class TogetherQuestions {
  public:
    TogetherQuestions(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                      RemotePlay& remote)
        : navigator_{navigator}, sounds_{sounds}, remote_{remote} {
    }

    /// Shows the question as the asks stand. Every frame, main loop only.
    void service();

  private:
    /// The question a dialog shows.
    struct Question {
        bool invitation{false};
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
    /// The question the dialog shows, and the dialog's title while it is open.
    std::optional<Question> asked_;
    std::string title_;
};

} // namespace opensu::app
