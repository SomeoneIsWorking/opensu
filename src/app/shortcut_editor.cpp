#include "shortcut_editor.hpp"

namespace opensu::app {

void ShortcutEditor::begin(input::Action action) {
    target_ = action;
    held_.clear();
    loneB_ = false;
}

void ShortcutEditor::cancel() {
    const bool was = target_.has_value();
    target_.reset();
    held_.clear();
    loneB_ = false;
    if (was) {
        stopped_();
    }
}

void ShortcutEditor::keep(const std::string& refused, const std::string& what) {
    if (!refused.empty()) {
        say_(refused, true);
        return;
    }
    save();
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
            std::erase(held_, event.button);
            // B alone cancels on its release, so B can still be held for a chord.
            if (event.button == gamepad::Button::B && loneB_) {
                cancel();
            }
            continue;
        }
        const bool chords = input::takesPadChord(*target_);
        if (event.button == gamepad::Button::B && held_.empty() && !chords) {
            cancel();
            continue;
        }
        loneB_ = event.button == gamepad::Button::B && held_.empty();
        if (chords && !held_.empty()) {
            const input::PadChord chord{held_.front(), event.button};
            keep(shortcuts_.rebind(*target_, chord), input::describe(chord));
        }
        held_.push_back(event.button);
    }
}

void ShortcutEditor::resetAll() {
    shortcuts_.resetAll();
    save();
    say_("shortcuts restored", false);
    cancel();
}

void ShortcutEditor::resetChords() {
    shortcuts_.resetPads();
    save();
    say_("chords restored", false);
    cancel();
}

void ShortcutEditor::save() {
    preferences_.values().shortcuts = shortcuts_.overrides();
    preferences_.save();
    changed_();
}

} // namespace opensu::app
