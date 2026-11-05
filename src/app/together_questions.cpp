#include "together_questions.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::app {

void TogetherQuestions::service() {
    std::optional<RemotePlay::JoinQuestion> question = remote_.joinQuestion();
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
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Let in",
                                                .chosen =
                                                    [this] {
                                                        answer(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = false});
    const std::string& guest = asked_->peer.name;
    title_ = "Let " + guest + " join?";
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        title_,
        guest + " asks to join " + asked_->title + ". Let in, " + guest +
            " sees the game and plays it with their own controller as the next player. The saves "
            "stay this profile's.",
        std::move(choices), 0));
}

void TogetherQuestions::answer(bool allow) {
    title_.clear();
    if (asked_) {
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
