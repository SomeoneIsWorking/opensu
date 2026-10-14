#include "button_map.hpp"

#include <algorithm>

namespace opensu::input {
namespace {

using gamepad::Button;

/// Why `output` cannot be sent, or empty.
std::string refusal(const PadOutput& output) {
    for (const Button button : output) {
        if (!mappable(button)) {
            return buttonLabel(button) + " cannot be sent to a game";
        }
    }
    return {};
}

} // namespace

bool mappable(Button button) noexcept {
    return std::ranges::find(gamepad::padButtons, button) != gamepad::padButtons.end();
}

std::string describe(const PadOutput& output) {
    if (output.empty()) {
        return "Nothing";
    }
    std::string text;
    for (const Button button : output) {
        text += (text.empty() ? "" : " + ") + buttonLabel(button);
    }
    return text;
}

std::string describe(const ChordOutput& output) {
    if (const auto* action = std::get_if<Action>(&output)) {
        return std::string{label(*action)};
    }
    return describe(std::get<PadOutput>(output));
}

PadOutput ButtonMap::output(Button button) const {
    const auto found = buttons_.find(button);
    return found == buttons_.end() ? PadOutput{button} : found->second;
}

const ChordOutput* ButtonMap::chord(const PadChord& chord) const {
    const auto found = std::ranges::find(chords_, chord, &ChordMapping::chord);
    return found == chords_.end() ? nullptr : &found->output;
}

std::string ButtonMap::setButton(Button button, PadOutput output) {
    if (!mappable(button)) {
        return buttonLabel(button) + " is openSU's and cannot be mapped";
    }
    if (std::string refused = refusal(output); !refused.empty()) {
        return refused;
    }
    if (output == PadOutput{button}) {
        buttons_.erase(button);
    } else {
        buttons_[button] = std::move(output);
    }
    return {};
}

std::string ButtonMap::setChord(const PadChord& chord, ChordOutput output) {
    if (chord.modifier == chord.trigger || !mappable(chord.modifier) || !mappable(chord.trigger)) {
        return "hold a button and press another; Guide's chords are openSU's";
    }
    if (const auto* action = std::get_if<Action>(&output); action != nullptr) {
        if (!takesPadChord(*action)) {
            return std::string{label(*action)} + " cannot be a chord";
        }
    } else if (std::string refused = refusal(std::get<PadOutput>(output)); !refused.empty()) {
        return refused;
    }
    const auto found = std::ranges::find(chords_, chord, &ChordMapping::chord);
    if (found != chords_.end()) {
        found->output = std::move(output);
    } else {
        chords_.push_back(ChordMapping{chord, std::move(output)});
    }
    return {};
}

void ButtonMap::removeChord(const PadChord& chord) {
    std::erase_if(chords_, [&chord](const ChordMapping& mapping) {
        return mapping.chord == chord;
    });
}

gamepad::ForwardMap forwardMap(const ButtonMap& map, const Shortcuts& shell) {
    gamepad::ForwardMap forward;
    forward.buttons = map.buttons();
    for (const ChordMapping& mapping : map.chords()) {
        const auto* buttons = std::get_if<PadOutput>(&mapping.output);
        if (buttons != nullptr && !shell.actionFor(mapping.chord)) {
            forward.chords[mapping.chord] = *buttons;
        }
    }
    return forward;
}

} // namespace opensu::input
