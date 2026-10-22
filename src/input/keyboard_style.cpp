#include "keyboard_style.hpp"

#include <algorithm>

namespace opensu::input {

std::string_view key(KeyboardStyle style) noexcept {
    switch (style) {
    case KeyboardStyle::DaisyWheel:
        return "daisy";
    case KeyboardStyle::Regular:
        return "regular";
    }
    return {};
}

std::optional<KeyboardStyle> keyboardStyleOf(std::string_view spelling) noexcept {
    const auto found = std::ranges::find_if(allKeyboardStyles, [spelling](KeyboardStyle style) {
        return key(style) == spelling;
    });
    return found == allKeyboardStyles.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(KeyboardStyle style) noexcept {
    switch (style) {
    case KeyboardStyle::DaisyWheel:
        return "Daisy wheel";
    case KeyboardStyle::Regular:
        return "Regular";
    }
    return {};
}

} // namespace opensu::input
