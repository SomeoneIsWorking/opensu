#include "together_questions.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::app {

std::optional<TogetherQuestions::Question> TogetherQuestions::next() const {
    if (std::optional<RemotePlay::JoinQuestion> join = remote_.joinQuestion()) {
        return Question{.kind = Question::Kind::Join,
                        .peer = std::move(join->peer),
                        .title = std::move(join->title)};
    }
    if (std::optional<RemotePlay::Invited> invited = remote_.invitation()) {
        return Question{.kind = Question::Kind::Invitation,
                        .peer = std::move(invited->peer),
                        .title = std::move(invited->title)};
    }
    if (std::optional<CloseAsks::Question> close = closes_.question()) {
        return Question{.kind = Question::Kind::Close,
                        .peer = std::move(close->peer),
                        .title = std::move(close->title)};
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
    const Question::Kind kind = asked_->kind;
    choices.push_back(gpu2d::ui::Dialog::Choice{
        .label = kind == Question::Kind::Close ? "Keep playing" : "Decline",
        .chosen =
            [this] {
                answer(false);
            },
        .kind = gpu2d::ui::ButtonKind::Normal,
        .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = kind == Question::Kind::Invitation ? "Join"
                                                         : kind == Question::Kind::Close
                                                             ? "Close it"
                                                             : "Let in",
                                                .chosen =
                                                    [this] {
                                                        answer(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = false});
    const std::string& peer = asked_->peer.name;
    const std::string& game = asked_->title;
    std::string body;
    switch (kind) {
    case Question::Kind::Invitation:
        title_ = "Play with " + peer + "?";
        body = peer + " invites you to play " + game +
               ". You see the game from there and play it with your own controller as the next "
               "player. Its saves stay there.";
        break;
    case Question::Kind::Join:
        title_ = "Let " + peer + " join?";
        body = peer + " asks to join " + game + ". Let in, " + peer +
               " sees the game and plays it with their own controller as the next player. The "
               "saves stay this profile's.";
        break;
    case Question::Kind::Close:
        title_ = "Close " + game + " for " + peer + "?";
        body = peer + " asks to close " + game +
               " to play something else. A game on a built-in core is saved first; anything "
               "another game has not saved is lost.";
        break;
    }
    sounds_.play(audio::Effect::Open);
    navigator_.push(
        std::make_unique<gpu2d::ui::Dialog>(title_, std::move(body), std::move(choices), 0));
}

void TogetherQuestions::answer(bool allow) {
    title_.clear();
    if (!asked_) {
        return;
    }
    switch (asked_->kind) {
    case Question::Kind::Invitation:
        remote_.answerInvitation(asked_->peer.id, allow);
        break;
    case Question::Kind::Join:
        remote_.answerJoin(asked_->peer.id, allow);
        break;
    case Question::Kind::Close:
        closes_.answer(asked_->peer.id, allow);
        break;
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
