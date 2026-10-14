#include "key_names.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace opensu::input {
namespace {

struct NamedKey {
    int key;
    const char* name;
};

constexpr std::array<NamedKey, 22> namedKeys{{
    {keys::Menu, "Menu"},     {keys::Up, "Up"},
    {keys::Down, "Down"},     {keys::Left, "Left"},
    {keys::Right, "Right"},   {keys::Enter, "Enter"},
    {keys::Space, "Space"},   {keys::Escape, "Esc"},
    {keys::Tab, "Tab"},       {keys::Backspace, "Backspace"},
    {keys::LeftBracket, "["}, {keys::RightBracket, "]"},
    {keys::Slash, "/"},       {keys::Comma, ","},
    {keys::Period, "."},      {keys::Semicolon, ";"},
    {keys::Minus, "-"},       {keys::Equal, "="},
    {keys::Home, "Home"},     {keys::End, "End"},
    {keys::PageUp, "PageUp"}, {keys::PageDown, "PageDown"},
}};

constexpr std::size_t bindableCount = namedKeys.size() + 26 + 10 + 12;

constexpr std::array<int, bindableCount> bindable = [] {
    std::array<int, bindableCount> out{};
    std::size_t at = 0;
    for (const NamedKey& named : namedKeys) {
        out[at++] = named.key;
    }
    for (int key = keys::A; key <= keys::Z; ++key) {
        out[at++] = key;
    }
    for (int key = keys::Zero; key <= keys::Nine; ++key) {
        out[at++] = key;
    }
    for (int key = keys::F1; key <= keys::F12; ++key) {
        out[at++] = key;
    }
    return out;
}();

std::string lowered(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

struct X11Name {
    int key;
    const char* name;
};

constexpr std::array<X11Name, 14> x11Specials{{
    {keys::Enter, "Return"},
    {keys::Escape, "Escape"},
    {keys::Backspace, "BackSpace"},
    {keys::Space, "space"},
    {keys::LeftBracket, "bracketleft"},
    {keys::RightBracket, "bracketright"},
    {keys::Slash, "slash"},
    {keys::Comma, "comma"},
    {keys::Period, "period"},
    {keys::Semicolon, "semicolon"},
    {keys::Minus, "minus"},
    {keys::Equal, "equal"},
    {keys::PageUp, "Prior"},
    {keys::PageDown, "Next"},
}};

constexpr std::array<int, 4> modifiers{keys::LeftShift, keys::RightShift, keys::LeftControl,
                                       keys::RightControl};

} // namespace

Modifier modifierOf(int key) noexcept {
    if (key == keys::LeftControl || key == keys::RightControl) {
        return Modifier::Ctrl;
    }
    if (key == keys::LeftShift || key == keys::RightShift) {
        return Modifier::Shift;
    }
    return Modifier::Neither;
}

std::span<const int> modifierKeys() noexcept {
    return modifiers;
}

std::optional<std::string> keyName(int key) {
    for (const NamedKey& named : namedKeys) {
        if (named.key == key) {
            return std::string{named.name};
        }
    }
    if ((key >= keys::A && key <= keys::Z) || (key >= keys::Zero && key <= keys::Nine)) {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= keys::F1 && key <= keys::F12) {
        return "F" + std::to_string(key - keys::F1 + 1);
    }
    return std::nullopt;
}

std::span<const int> bindableKeys() noexcept {
    return bindable;
}

std::string describe(const Combo& combo) {
    return std::string{combo.ctrl ? "Ctrl+" : ""} + (combo.shift ? "Shift+" : "") +
           keyName(combo.key).value_or("?");
}

std::optional<Combo> comboNamed(std::string_view text) {
    Combo combo;
    std::string rest = lowered(text);
    for (bool more = true; more;) {
        more = false;
        if (rest.starts_with("ctrl+")) {
            combo.ctrl = true;
            rest.erase(0, 5);
            more = true;
        } else if (rest.starts_with("shift+")) {
            combo.shift = true;
            rest.erase(0, 6);
            more = true;
        }
    }
    for (const int key : bindableKeys()) {
        if (lowered(keyName(key).value_or("")) == rest) {
            combo.key = key;
            return combo;
        }
    }
    return std::nullopt;
}

std::string x11Name(int key) {
    switch (key) {
    case keys::LeftShift:
        return "Shift_L";
    case keys::RightShift:
        return "Shift_R";
    case keys::LeftControl:
        return "Control_L";
    case keys::RightControl:
        return "Control_R";
    default:
        break;
    }
    for (const X11Name& special : x11Specials) {
        if (special.key == key) {
            return special.name;
        }
    }
    if (key >= keys::A && key <= keys::Z) {
        return std::string(1, static_cast<char>(std::tolower(key)));
    }
    // Digits, F-keys, the arrows, Tab, Menu, Home and End are spelled as their caps are.
    return keyName(key).value_or("");
}

} // namespace opensu::input
