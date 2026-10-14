#include "chord_settings.hpp"

#include <optional>

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view rowPrefix = "chord.";
constexpr std::string_view resetId = "chords.reset";

} // namespace

ui::SettingsCategory ChordSettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Guide chords", {}};
    for (const input::Action action : input::allActions) {
        if (!input::takesPadChord(action)) {
            continue;
        }
        const bool listening = editor_.target() == action;
        const std::optional<input::PadChord> chord = shortcuts_.padChord(action);
        std::string value = chord ? input::describe(*chord) : "None";
        if (listening) {
            value = "Hold Guide and press a button";
        }
        page.rows.push_back(
            ui::makeRow(std::string{rowPrefix} + std::string{input::spelling(action)},
                        RowKind::Action, std::string{input::label(action)},
                        listening ? std::string{ShortcutEditor::listeningNote} : "", value));
    }
    page.rows.push_back(ui::makeRow(std::string{resetId}, RowKind::Action, "Restore default chords",
                                    "Every pad chord back to how it shipped; keys stay", ""));
    return page;
}

bool ChordSettings::owns(const std::string& id) {
    return id.starts_with(rowPrefix) || id == resetId;
}

void ChordSettings::run(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    if (id == resetId) {
        editor_.resetChords();
    } else if (const std::optional<input::Action> action =
                   input::actionOf(std::string_view{id}.substr(rowPrefix.size()))) {
        editor_.begin(*action);
    }
}

} // namespace opensu::app
