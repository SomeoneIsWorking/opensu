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

} // namespace

std::optional<int> keyOf(SDL_Scancode scancode) noexcept {
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return keys::A + (scancode - SDL_SCANCODE_A);
    }
    // SDL orders the digits 1 to 9, then 0.
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return keys::Zero + 1 + (scancode - SDL_SCANCODE_1);
    }
    if (scancode == SDL_SCANCODE_0) {
        return keys::Zero;
    }
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
        return keys::F1 + (scancode - SDL_SCANCODE_F1);
    }
    for (const auto& [code, key] : namedKeys) {
        if (code == scancode) {
            return key;
        }
    }
    return std::nullopt;
}

std::optional<SDL_Scancode> scancodeOf(int key) noexcept {
    for (int code = SDL_SCANCODE_UNKNOWN; code < SDL_SCANCODE_COUNT; ++code) {
        const auto scancode = static_cast<SDL_Scancode>(code);
        if (keyOf(scancode) == key) {
            return scancode;
        }
    }
    return std::nullopt;
}

} // namespace opensu::app
