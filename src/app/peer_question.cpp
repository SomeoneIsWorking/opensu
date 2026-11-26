#include "peer_question.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::app {

void PeerQuestion::ask(std::string title, const std::string& body, std::string grant,
                       std::function<void(bool)> answered) {
    close();
    const auto answer = [this, answered = std::move(answered)](bool granted) {
        title_.clear();
        answered(granted);
    };
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Decline",
                                                .chosen =
                                                    [answer] {
                                                        answer(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = std::move(grant),
                                                .chosen =
                                                    [answer] {
                                                        answer(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = false});
    title_ = std::move(title);
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(title_, body, std::move(choices), 0));
}

void PeerQuestion::close() {
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
