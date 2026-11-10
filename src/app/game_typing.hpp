// game_typing — the on-screen keyboard's way into the game in front, where openSU has no field of
// its own: each character, Backspace, Return and the caret keys go straight to the game's window,
// which keeps the text. Inside Gamescope on Linux they are XTest input on Gamescope's Xwayland
// (`session::XTestInput`), which delivers them to the window Gamescope focused while the overlay
// leaves it the keyboard (`display::OverlayMode::Typing`); elsewhere there is none
// (`game_typing_none.cpp`), so the keyboard types only into openSU's own fields.
#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace opensu::app {

class GameTyping {
  public:
    virtual ~GameTyping() = default;
    GameTyping() = default;
    GameTyping(const GameTyping&) = delete;
    GameTyping& operator=(const GameTyping&) = delete;
    GameTyping(GameTyping&&) = delete;
    GameTyping& operator=(GameTyping&&) = delete;

    /// Types `text`; false when a character of it has no key on the game's display, which is
    /// then left out.
    virtual bool type(std::string_view text) = 0;
    /// The character before the game's caret goes.
    virtual void erase() = 0;
    /// Return: the game takes the line.
    virtual void enter() = 0;
    /// Moves the game's caret a character left or right.
    virtual void caret(bool left) = 0;
};

/// Whether openSU can type into the game in front here: inside Gamescope, on Linux.
[[nodiscard]] bool typesIntoGames(bool insideGamescope) noexcept;
/// Typing into the games on the X display `x11Display`; throws std::runtime_error saying why it
/// cannot.
[[nodiscard]] std::unique_ptr<GameTyping> openGameTyping(const std::string& x11Display);

} // namespace opensu::app
