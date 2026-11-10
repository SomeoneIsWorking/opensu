// keyboard_pad — the on-screen keyboard as the pad's way of typing: A on a text field opens it
// (`gpu2d::ui::Navigator::setKeyboard`), and it is up exactly while the focused field is being
// edited (`gpu2d::ui::Navigator::edit`): the pad's buttons type into the field through the
// navigator's typed text, Erase, Left, Right and Submit. It closes when the field stops being
// edited (the focus leaves it, Back), and Done submits the field.
//
// Regular grid: the D-pad walks the keys, A presses the focused key, B deletes and closes when the
// field is empty, X toggles capitals, Y types a space, Start is Done.
// Daisy wheel: the left stick picks a petal (`stick`, fed from stick-origin directions) and A, B, X
// and Y type its characters; with no petal picked A pastes, X deletes, Y types a space and B
// closes. LB deletes and RB types a space either way, LT and RT step the set, D-pad Left and Right
// move the caret, Start is Done. While touch controls show, a field types on the system's on-screen
// keyboard where there is one (Android's), the way iiSU does; the grid is the pad's.
// Over a game, opened from the Guide menu or its chord, the same buttons type into the game in
// front through `GameTyping`, which keeps the text; the keyboard echoes it on its preview line,
// Start is Return and closes it, and it closes when a screen opens over it or the game ends.
#pragma once

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "game_typing.hpp"
#include "gamepad/event.hpp"
#include "ui/on_screen_keyboard.hpp"

namespace opensu::app {

/// The direction a D-pad button points; Right for any other button.
[[nodiscard]] ui::Direction directionOf(gamepad::Button button) noexcept;

class KeyboardPad {
  public:
    KeyboardPad(ui::OnScreenKeyboard& keyboard, gpu2d::ui::Navigator& navigator,
                audio::SdlSoundPlayer& sounds)
        : keyboard_{keyboard}, navigator_{navigator}, sounds_{sounds} {
    }

    /// Whether the system has an on-screen keyboard; set once the window is up.
    void setSystemKeyboard(bool available) noexcept {
        systemKeyboard_ = available;
    }
    /// Starts editing the focused field. While touch controls show, on the system's keyboard, or
    /// where there is none on the regular grid a finger taps; otherwise on the grid in the
    /// chosen style.
    void open();
    /// Opens the keyboard over the running game, typing into it through `game`.
    void openOverGame(GameTyping& game);
    void close();
    /// Whether it is up over a game, typing into it.
    [[nodiscard]] bool overGame() const noexcept {
        return game_ != nullptr;
    }
    /// Whether openSU's own keyboard is up.
    [[nodiscard]] bool isOpen() const noexcept {
        return keyboard_.isOpen();
    }
    /// Whether the field is typed into on the system's keyboard now.
    [[nodiscard]] bool systemTyping() const noexcept {
        return systemTyping_;
    }
    /// Closes the keyboard once the focused field is not being edited, or over a game once a screen
    /// opens over it or no game is running (`inGame`). Each frame.
    void follow(bool inGame);

    /// A pad button while it is up.
    void act(gamepad::Button button);
    /// Whether the left stick picks petals now: the wheel is up.
    [[nodiscard]] bool takesStick() const noexcept;
    /// A left stick direction held or let go.
    void stick(ui::Direction direction, bool held);

  private:
    void onGrid(gamepad::Button button);
    void onWheel(gamepad::Button button);
    /// Sends what a key press asked for to the field.
    void apply(const ui::KeyPress& press);
    void type(const std::string& typed);
    void erase();
    /// Done: closes the keyboard and submits the field.
    void done();
    /// The caret a character left or right; reports whether it moved.
    bool caret(bool left);
    /// Whether the focused field, or over a game the preview line, holds nothing.
    [[nodiscard]] bool fieldEmpty() const;

    ui::OnScreenKeyboard& keyboard_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    bool systemKeyboard_{false};
    bool systemTyping_{false};
    /// What it types into while it is up over a game.
    GameTyping* game_{};
};

} // namespace opensu::app
