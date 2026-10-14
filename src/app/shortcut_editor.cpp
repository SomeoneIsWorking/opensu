#include "shortcut_editor.hpp"

namespace opensu::app {

void ShortcutEditor::begin(input::Action action) {
    target_ = action;
    capture_.begin();
}

void ShortcutEditor::cancel() {
    const bool was = target_.has_value();
    target_.reset();
    capture_.begin();
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
        if (!input::takesPadChord(*target_)) {
            // Only keys reach an action without a chord; B on the pad cancels.
            if (event.pressed && event.button == gamepad::Button::B && !capture_.holding()) {
                cancel();
            } else {
                static_cast<void>(capture_.feed(event));
            }
            continue;
        }
        const std::optional<ChordCapture::Heard> heard = capture_.feed(event);
        if (heard && heard->cancelled) {
            cancel();
        } else if (heard) {
            keep(shortcuts_.rebind(*target_, heard->chord), input::describe(heard->chord));
        }
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
