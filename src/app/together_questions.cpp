#include "together_questions.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::app {

std::optional<TogetherQuestions::Question> TogetherQuestions::next() const {
    if (std::optional<RemotePlay::JoinQuestion> join = remote_.joinQuestion()) {
        return Question{
            .invitation = false, .peer = std::move(join->peer), .title = std::move(join->title)};
    }
    if (std::optional<RemotePlay::Invited> invited = remote_.invitation()) {
        return Question{.invitation = true,
                        .peer = std::move(invited->peer),
                        .title = std::move(invited->title)};
    }
    return std::nullopt;
}

void TogetherQuestions::service() {
    std::optional<Question> question = next();
    if (question == asked_) {
        return;
    }
    close();
    asked_ = std::move(question);
    if (!asked_) {
        return;
    }
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Decline",
                                                .chosen =
                                                    [this] {
                                                        answer(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    const bool invitation = asked_->invitation;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = invitation ? "Join" : "Let in",
                                                .chosen =
                                                    [this] {
                                                        answer(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = false});
    const std::string& peer = asked_->peer.name;
    title_ = invitation ? "Play with " + peer + "?" : "Let " + peer + " join?";
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        title_,
        invitation
            ? peer + " invites you to play " + asked_->title +
                  ". You see the game from there and play it with your own controller as the "
                  "next player. Its saves stay there."
            : peer + " asks to join " + asked_->title + ". Let in, " + peer +
                  " sees the game and plays it with their own controller as the next player. The "
                  "saves stay this profile's.",
        std::move(choices), 0));
}

void TogetherQuestions::answer(bool allow) {
    title_.clear();
    if (asked_ && asked_->invitation) {
        remote_.answerInvitation(asked_->peer.id, allow);
    } else if (asked_) {
        remote_.answerJoin(asked_->peer.id, allow);
    }
}

void TogetherQuestions::close() {
    if (title_.empty()) {
        return;
    }
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == "dialog" && screen->title() == title_) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
    title_.clear();
}

} // namespace opensu::app
