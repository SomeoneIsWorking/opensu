#include "polkit_prompt_controller.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "secrets/secret.hpp"

namespace opensu::app {
namespace {

using host::polkit::Prompt;

constexpr const char* cardId = "polkit.authentication";
/// PAM's longest answer (PAM_MAX_RESP_SIZE) less its terminating byte.
constexpr std::size_t answerMostBytes = 511;

const gpu2d::ui::Screen* openCard(const gpu2d::ui::Navigator& navigator) {
    const std::vector<const gpu2d::ui::Screen*> open = navigator.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return screen->id() == cardId && !screen->closing();
    });
    return found == open.end() ? nullptr : *found;
}

ui::AuthenticationCard cardOf(const Prompt& prompt) {
    ui::AuthenticationCard card{
        .title = "Authenticate as " + prompt.user, .note = prompt.message, .field = std::nullopt};
    if (!prompt.note.empty()) {
        card.note = prompt.note + "\n\n" + card.note;
    }
    if (prompt.state == Prompt::State::Asking) {
        card.field = ui::AuthenticationCard::Field{
            .label = prompt.question.empty() ? std::string{"Password"} : prompt.question,
            .secret = !prompt.shown};
    }
    return card;
}

/// Whether `now` asks a question `before` did not.
bool asksAnew(const std::optional<Prompt>& before, const Prompt& now) {
    return now.state == Prompt::State::Asking &&
           (!before || before->state != Prompt::State::Asking || before->serial != now.serial);
}

} // namespace

void PolkitPromptController::service() {
    std::optional<Prompt> now = agent_.prompt();
    if (shown_ && (!now || now->authentication != shown_->authentication)) {
        close();
    }
    if (!now) {
        return;
    }
    const bool fresh = !shown_;
    const bool question = asksAnew(shown_, *now);
    shown_ = now;
    if (fresh) {
        open(*now);
    }
    *card_ = cardOf(*now);
    if (question) {
        secrets::wipe(typed_);
        if (navigator_.focus(ui::AuthenticationScreen::fieldId)) {
            keyboard_.open();
        }
    }
}

bool PolkitPromptController::asking() const {
    return shown_.has_value();
}

void PolkitPromptController::open(const Prompt& prompt) {
    secrets::wipe(typed_);
    card_ = std::make_shared<ui::AuthenticationCard>(cardOf(prompt));
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::AuthenticationScreen>(
        ui::Authenticating{.id = cardId,
                           .card =
                               [card = card_] {
                                   return *card;
                               },
                           .mostBytes = answerMostBytes,
                           .text = [this]() -> const std::string& {
                               return typed_;
                           },
                           .changed =
                               [this](std::string text) {
                                   secrets::wipe(typed_);
                                   typed_ = std::move(text);
                               },
                           .submitted =
                               [this] {
                                   submit();
                               },
                           .cancelled =
                               [this] {
                                   dismiss();
                               }}));
}

void PolkitPromptController::close() {
    if (const gpu2d::ui::Screen* card = openCard(navigator_)) {
        navigator_.closeFrom(*card);
    }
    shown_.reset();
    card_.reset();
    secrets::wipe(typed_);
}

void PolkitPromptController::submit() {
    if (!shown_ || shown_->state != Prompt::State::Asking) {
        return;
    }
    std::string secret;
    secret.swap(typed_);
    sounds_.play(audio::Effect::Close);
    agent_.answer(shown_->serial, secret);
    secrets::wipe(secret);
    service();
}

void PolkitPromptController::dismiss() {
    sounds_.play(audio::Effect::Close);
    agent_.cancel();
    service();
}

} // namespace opensu::app
