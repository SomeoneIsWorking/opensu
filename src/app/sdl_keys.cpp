#include "sdl_keys.hpp"

#include <array>
#include <utility>

#include "input/key_names.hpp"

namespace opensu::app {
namespace {

namespace keys = input::keys;

constexpr std::array<std::pair<SDL_Scancode, int>, 30> namedKeys{{
    {SDL_SCANCODE_SPACE, keys::Space},
    {SDL_SCANCODE_COMMA, keys::Comma},
    {SDL_SCANCODE_MINUS, keys::Minus},
    {SDL_SCANCODE_PERIOD, keys::Period},
    {SDL_SCANCODE_SLASH, keys::Slash},
    {SDL_SCANCODE_SEMICOLON, keys::Semicolon},
    {SDL_SCANCODE_EQUALS, keys::Equal},
    {SDL_SCANCODE_LEFTBRACKET, keys::LeftBracket},
    {SDL_SCANCODE_RIGHTBRACKET, keys::RightBracket},
    {SDL_SCANCODE_ESCAPE, keys::Escape},
    {SDL_SCANCODE_RETURN, keys::Enter},
    {SDL_SCANCODE_TAB, keys::Tab},
    {SDL_SCANCODE_BACKSPACE, keys::Backspace},
    {SDL_SCANCODE_RIGHT, keys::Right},
    {SDL_SCANCODE_LEFT, keys::Left},
    {SDL_SCANCODE_DOWN, keys::Down},
    {SDL_SCANCODE_UP, keys::Up},
    {SDL_SCANCODE_PAGEUP, keys::PageUp},
    {SDL_SCANCODE_PAGEDOWN, keys::PageDown},
    {SDL_SCANCODE_HOME, keys::Home},
    {SDL_SCANCODE_END, keys::End},
    {SDL_SCANCODE_KP_ENTER, keys::KeypadEnter},
    {SDL_SCANCODE_LSHIFT, keys::LeftShift},
    {SDL_SCANCODE_LCTRL, keys::LeftControl},
    {SDL_SCANCODE_LALT, keys::LeftAlt},
    {SDL_SCANCODE_RSHIFT, keys::RightShift},
    {SDL_SCANCODE_RCTRL, keys::RightControl},
    {SDL_SCANCODE_RALT, keys::RightAlt},
    {SDL_SCANCODE_APPLICATION, keys::Menu},
    {SDL_SCANCODE_MENU, keys::Menu},
}};

using KeyPair = std::pair<SDL_Scancode, int>;

/// Every scancode the shell has a key for: the letters, the digits (SDL orders them 1 to 9, then
/// 0), F1 to F12 and the named keys.
constexpr std::array<KeyPair, 26 + 10 + 12 + namedKeys.size()> keyTable = [] {
    std::array<KeyPair, 26 + 10 + 12 + namedKeys.size()> table{};
    std::size_t at = 0;
    const auto range = [&table, &at](SDL_Scancode first, int count, int firstKey) {
        for (int i = 0; i < count; ++i) {
            table[at++] = {static_cast<SDL_Scancode>(first + i), firstKey + i};
        }
    };
    range(SDL_SCANCODE_A, 26, keys::A);
    range(SDL_SCANCODE_1, 9, keys::Zero + 1);
    range(SDL_SCANCODE_0, 1, keys::Zero);
    range(SDL_SCANCODE_F1, 12, keys::F1);
    for (const KeyPair& named : namedKeys) {
        table[at++] = named;
    }
    return table;
}();

} // namespace

std::optional<int> keyOf(SDL_Scancode scancode) noexcept {
    for (const auto& [code, key] : keyTable) {
        if (code == scancode) {
            return key;
        }
    }
    return std::nullopt;
}

std::optional<SDL_Scancode> scancodeOf(int key) noexcept {
    for (const auto& [code, mapped] : keyTable) {
        if (mapped == key) {
            return code;
        }
    }
    return std::nullopt;
}

} // namespace opensu::app
