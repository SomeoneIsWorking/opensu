#include "shortcut_editor.hpp"

namespace opensu::app {

void ShortcutEditor::begin(input::Action action) {
    target_ = action;
    held_.clear();
}

void ShortcutEditor::cancel() {
    const bool was = target_.has_value();
    target_.reset();
    held_.clear();
    if (was) {
        stopped_();
    }
}

void ShortcutEditor::keep(const std::string& refused, const std::string& what) {
    if (!refused.empty()) {
        say_(refused, true);
        return;
    }
    preferences_.values().shortcuts = shortcuts_.overrides();
    preferences_.save();
    const std::string name{target_ ? input::label(*target_) : std::string_view{}};
    say_(name + ": " + what, false);
    cancel();
}

void ShortcutEditor::captureKey(const input::Combo& combo) {
    if (!target_) {
        return;
    }
    keep(shortcuts_.rebind(*target_, combo), input::describe(combo));
}

void ShortcutEditor::capturePad(const std::vector<gamepad::Event>& events) {
    for (const gamepad::Event& event : events) {
        if (!target_ || event.kind != gamepad::Event::Kind::Button) {
            continue;
        }
        if (!event.pressed) {
            held_.erase(event.button);
            continue;
        }
        if (event.button == gamepad::Button::B && held_.empty()) {
            cancel();
            continue;
        }
        if (!input::takesPadChord(*target_)) {
            continue;
        }
        for (const gamepad::Button modifier : held_) {
            if (input::canHold(modifier)) {
                const input::PadChord chord{modifier, event.button};
                keep(shortcuts_.rebind(*target_, chord), input::describe(chord));
                break;
            }
        }
        held_.insert(event.button);
    }
}

void ShortcutEditor::resetAll() {
    shortcuts_.resetAll();
    preferences_.values().shortcuts = shortcuts_.overrides();
    preferences_.save();
    say_("shortcuts restored", false);
    cancel();
}

} // namespace opensu::app
