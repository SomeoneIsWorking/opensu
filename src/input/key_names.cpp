#include "key_names.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "raylib.h"

namespace opensu::input {
namespace {

struct NamedKey {
    int key;
    const char* name;
};

constexpr std::array<NamedKey, 22> namedKeys{{
    {KEY_KB_MENU, "Menu"},   {KEY_UP, "Up"},
    {KEY_DOWN, "Down"},      {KEY_LEFT, "Left"},
    {KEY_RIGHT, "Right"},    {KEY_ENTER, "Enter"},
    {KEY_SPACE, "Space"},    {KEY_ESCAPE, "Esc"},
    {KEY_TAB, "Tab"},        {KEY_BACKSPACE, "Backspace"},
    {KEY_LEFT_BRACKET, "["}, {KEY_RIGHT_BRACKET, "]"},
    {KEY_SLASH, "/"},        {KEY_COMMA, ","},
    {KEY_PERIOD, "."},       {KEY_SEMICOLON, ";"},
    {KEY_MINUS, "-"},        {KEY_EQUAL, "="},
    {KEY_HOME, "Home"},      {KEY_END, "End"},
    {KEY_PAGE_UP, "PageUp"}, {KEY_PAGE_DOWN, "PageDown"},
}};

constexpr std::size_t bindableCount = namedKeys.size() + 26 + 10 + 12;

constexpr std::array<int, bindableCount> bindable = [] {
    std::array<int, bindableCount> out{};
    std::size_t at = 0;
    for (const NamedKey& named : namedKeys) {
        out[at++] = named.key;
    }
    for (int key = KEY_A; key <= KEY_Z; ++key) {
        out[at++] = key;
    }
    for (int key = KEY_ZERO; key <= KEY_NINE; ++key) {
        out[at++] = key;
    }
    for (int key = KEY_F1; key <= KEY_F12; ++key) {
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
    {KEY_ENTER, "Return"},
    {KEY_ESCAPE, "Escape"},
    {KEY_BACKSPACE, "BackSpace"},
    {KEY_SPACE, "space"},
    {KEY_LEFT_BRACKET, "bracketleft"},
    {KEY_RIGHT_BRACKET, "bracketright"},
    {KEY_SLASH, "slash"},
    {KEY_COMMA, "comma"},
    {KEY_PERIOD, "period"},
    {KEY_SEMICOLON, "semicolon"},
    {KEY_MINUS, "minus"},
    {KEY_EQUAL, "equal"},
    {KEY_PAGE_UP, "Prior"},
    {KEY_PAGE_DOWN, "Next"},
}};

constexpr std::array<int, 4> modifiers{KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_CONTROL,
                                       KEY_RIGHT_CONTROL};

} // namespace

Modifier modifierOf(int key) noexcept {
    if (key == KEY_LEFT_CONTROL || key == KEY_RIGHT_CONTROL) {
        return Modifier::Ctrl;
    }
    if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT) {
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
    if ((key >= KEY_A && key <= KEY_Z) || (key >= KEY_ZERO && key <= KEY_NINE)) {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= KEY_F1 && key <= KEY_F12) {
        return "F" + std::to_string(key - KEY_F1 + 1);
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
    case KEY_LEFT_SHIFT:
        return "Shift_L";
    case KEY_RIGHT_SHIFT:
        return "Shift_R";
    case KEY_LEFT_CONTROL:
        return "Control_L";
    case KEY_RIGHT_CONTROL:
        return "Control_R";
    default:
        break;
    }
    for (const X11Name& special : x11Specials) {
        if (special.key == key) {
            return special.name;
        }
    }
    if (key >= KEY_A && key <= KEY_Z) {
        return std::string(1, static_cast<char>(std::tolower(key)));
    }
    // Digits, F-keys, the arrows, Tab, Menu, Home and End are spelled as their caps are.
    return keyName(key).value_or("");
}

} // namespace opensu::input
