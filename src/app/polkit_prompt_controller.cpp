#include "polkit_prompt_controller.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "secrets/secret.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

constexpr const char* cardId = "polkit.password";
/// PAM's longest answer (PAM_MAX_RESP_SIZE) less its terminating byte.
constexpr std::size_t answerMostBytes = 511;

const gpu2d::ui::Screen* openCard(const gpu2d::ui::Navigator& navigator) {
    const std::vector<const gpu2d::ui::Screen*> open = navigator.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return screen->id() == cardId && !screen->closing();
    });
    return found == open.end() ? nullptr : *found;
}

std::string noteOf(const host::polkit::Prompt& prompt) {
    std::string note = prompt.message;
    if (!prompt.note.empty()) {
        note = prompt.note + "\n\n" + note;
    }
    return note;
}

} // namespace

void PolkitPromptController::service() {
    const std::optional<host::polkit::Prompt> prompt = agent_.prompt();
    if (!prompt || prompt->serial == handled_) {
        if (serial_) {
            close();
        }
        return;
    }
    if (serial_ != prompt->serial) {
        if (serial_) {
            close();
        }
        open(*prompt);
    }
}

bool PolkitPromptController::asking() const {
    return serial_.has_value();
}

void PolkitPromptController::open(const host::polkit::Prompt& prompt) {
    serial_ = prompt.serial;
    secrets::wipe(typed_);
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = cardId,
                  .title = "Password for " + prompt.user,
                  .note = noteOf(prompt),
                  .label = prompt.question.empty() ? std::string{"Password"} : prompt.question,
                  .placeholder = "Password",
                  .secret = !prompt.shown,
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
    keyboard_.open();
}

void PolkitPromptController::close() {
    if (const gpu2d::ui::Screen* card = openCard(navigator_)) {
        navigator_.closeFrom(*card);
    }
    serial_.reset();
    secrets::wipe(typed_);
}

void PolkitPromptController::submit() {
    if (!serial_) {
        return;
    }
    const std::uint64_t serial = *serial_;
    handled_ = serial;
    std::string secret;
    secret.swap(typed_);
    sounds_.play(audio::Effect::Close);
    close();
    agent_.answer(serial, secret);
    secrets::wipe(secret);
}

void PolkitPromptController::dismiss() {
    if (serial_) {
        handled_ = *serial_;
    }
    sounds_.play(audio::Effect::Close);
    close();
    agent_.cancel();
}

} // namespace opensu::app
