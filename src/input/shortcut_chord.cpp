#include "shortcut_chord.hpp"

namespace opensu::input {

std::optional<Combo> ShortcutChord::key(int key, bool pressed) {
    if (const Modifier modifier = modifierOf(key); modifier != Modifier::Neither) {
        (modifier == Modifier::Ctrl ? ctrl_ : shift_) = pressed;
        return std::nullopt;
    }
    if (!pressed) {
        down_.erase(key);
        return std::nullopt;
    }
    const bool repeat = !down_.insert(key).second;
    if (repeat) {
        return std::nullopt;
    }
    return Combo{key, ctrl_, shift_};
}

} // namespace opensu::input
