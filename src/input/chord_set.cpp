#include "chord_set.hpp"

#include <variant>

namespace opensu::input {

gamepad::ChordTriggers ChordSet::triggers() const {
    gamepad::ChordTriggers triggers{{}, holdDelay};
    for (const Action action : allActions) {
        if (const std::optional<PadChord> chord = shell_->padChord(action)) {
            triggers.chords.insert(*chord);
        }
    }
    for (const ChordMapping& mapping : game_) {
        triggers.chords.insert(mapping.chord);
    }
    return triggers;
}

std::optional<Action> ChordSet::actionFor(const PadChord& chord) const {
    if (const std::optional<Action> action = shell_->actionFor(chord)) {
        return action;
    }
    for (const ChordMapping& mapping : game_) {
        if (const auto* action = std::get_if<Action>(&mapping.output);
            action != nullptr && mapping.chord == chord) {
            return *action;
        }
    }
    return std::nullopt;
}

std::vector<ChordListing> ChordSet::on(gamepad::Button modifier) const {
    std::vector<ChordListing> listed;
    for (const ChordBinding& binding : shell_->chordsOn(modifier)) {
        listed.push_back(ChordListing{binding.chord, std::string{label(binding.action)}, false});
    }
    for (const ChordMapping& mapping : game_) {
        if (mapping.chord.modifier != modifier || shell_->actionFor(mapping.chord)) {
            continue;
        }
        const bool sends = std::holds_alternative<PadOutput>(mapping.output);
        listed.push_back(
            ChordListing{mapping.chord, (sends ? "Sends " : "") + describe(mapping.output), true});
    }
    return listed;
}

} // namespace opensu::input
