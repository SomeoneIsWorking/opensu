// on_screen_keyboard_painter — draws the on-screen keyboard in the look of iiSU's other panels: a
// pale card over the dimmed screen, a white field with the caret at the cursor, and the keys: white
// grid keys with the focused one outlined in ink, or the daisy wheel's white petals with the picked
// one filled in ink, the sets on its left and the shoulder buttons on its right.
#pragma once

#include <functional>
#include <vector>

#include "button_glyph.hpp"
#include "hint_row.hpp"
#include "on_screen_keyboard.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

/// What a caller adds to the keyboard it draws.
struct KeyboardExtras {
    /// Draws into the layout's `reserved` room, at the panel's alpha.
    std::function<void(float alpha)> reserved;
    /// The hints along the foot in place of the keyboard's own; empty keeps them.
    std::vector<HintPrompt> hints;
    /// Whether the keys hold the pad's focus, which outlines the focused grid key.
    bool keysFocused{true};
};

class OnScreenKeyboardPainter {
  public:
    OnScreenKeyboardPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : prompts_{&prompts}, typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Draws `keyboard` in its own layout of a `size` frame at `dp` pixels per dp, as faded and
    /// scaled by `look`; `seconds` blinks the caret.
    void paint(Canvas& canvas, const OnScreenKeyboard& keyboard, Point size, float dp,
               const PanelLook& look, double seconds) const;
    /// The same in `layout`, with what `extras` adds.
    void paint(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
               Point size, float dp, const PanelLook& look, double seconds,
               const KeyboardExtras& extras) const;

    /// The hints the keyboard shows along its foot for the device last used.
    [[nodiscard]] std::vector<HintPrompt> hints(const OnScreenKeyboard& keyboard) const;

    /// A white rounded bar; outlined when `focused`.
    static void paintBar(Canvas& canvas, const Rect& rect, bool focused, float dp, float alpha);
    /// Draws `text` from `x` cut to `room` pixels, ending in dots when it is cut.
    void drawFitted(Canvas& canvas, std::string text, float x, float centreY, float room,
                    const TextStyle& style, Colour colour) const;

  private:
    void paintField(Canvas& canvas, const OnScreenKeyboard& keyboard, const Rect& field, float dp,
                    float alpha, double seconds) const;
    void paintGrid(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
                   float dp, float alpha, bool focused) const;
    void paintWheel(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
                    float dp, float alpha) const;

    const input::Prompts* prompts_;
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
