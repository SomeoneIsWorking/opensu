// keyboard_pad — the on-screen keyboard as the pad's way of typing: A on a text field opens it
// (`gpu2d::ui::Navigator::setKeyboard`), and it is up exactly while the focused field is being
// edited (`gpu2d::ui::Navigator::edit`): the pad's buttons type into the field through the
// navigator's typed text, Erase, Left, Right and Submit. It closes when the field stops being
// edited (the focus leaves it, Back), and Done submits the field.
//
// Regular grid: the D-pad walks the keys, A presses the focused key, B deletes and closes when the
// field is empty, X toggles capitals, Y types a space, Start is Done.
// Daisy wheel: the left stick picks a petal (`stick`, fed from stick-origin directions) and A, B, X
// and Y type its characters; with no petal picked X deletes, Y types a space and B closes. LB
// deletes and RB types a space either way, LT and RT step the set, D-pad Left and Right move the
// caret, Start is Done.
#pragma once

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
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

    /// Opens the keyboard over the focused field.
    void open();
    void close();
    [[nodiscard]] bool isOpen() const noexcept {
        return keyboard_.isOpen();
    }
    /// Closes the keyboard once the focused field is not being edited. Each frame.
    void follow();

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
    /// Whether the focused field holds nothing.
    [[nodiscard]] bool fieldEmpty() const;

    ui::OnScreenKeyboard& keyboard_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
};

} // namespace opensu::app
