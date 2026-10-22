// on_screen_keyboard_painter — draws the on-screen keyboard docked along the bottom edge on a
// translucent card: the daisy wheel's ring of petals with their characters in miniature, the
// picked petal's characters large in the A/B/X/Y diamond and between them the set strip and the
// hints; or the regular grid's keys beside the hints. Without a field of the caller's own on
// screen, a one-line preview of the text sits above the strip.
#pragma once

#include <vector>

#include "button_glyph.hpp"
#include "hint_row.hpp"
#include "on_screen_keyboard.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

/// What a caller changes about the keyboard it draws.
struct KeyboardExtras {
    /// The hints in place of the keyboard's own; empty keeps them.
    std::vector<HintPrompt> hints;
    /// Whether the keys hold the pad's focus, which outlines the focused grid key.
    bool keysFocused{true};
};

class OnScreenKeyboardPainter {
  public:
    OnScreenKeyboardPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : prompts_{&prompts}, typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Draws `keyboard` docked in a `size` frame at `dp` pixels per dp with its preview line, as
    /// faded and scaled by `look`; `seconds` blinks the caret.
    void paint(Canvas& canvas, const OnScreenKeyboard& keyboard, Point size, float dp,
               const PanelLook& look, double seconds) const;
    /// The same in `layout`, with what `extras` changes.
    void paint(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
               const PanelLook& look, double seconds, const KeyboardExtras& extras) const;

    /// The hints the keyboard shows for the device last used.
    [[nodiscard]] std::vector<HintPrompt> hints(const OnScreenKeyboard& keyboard) const;

    /// A white rounded bar; outlined when `focused`.
    static void paintBar(Canvas& canvas, const Rect& rect, bool focused, float dp, float alpha);
    /// The line in a white bar sized by `field`'s height: a search glass, the text or the prompt
    /// (dots for a secret) and the caret at the cursor.
    void paintField(Canvas& canvas, const OnScreenKeyboard& keyboard, const Rect& field,
                    float alpha, double seconds) const;

  private:
    void paintGrid(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
                   float alpha, bool focused) const;
    void paintWheel(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
                    float alpha) const;
    /// The sets between LT and RT and the shoulder buttons' actions, centred on `centre` at `unit`
    /// pixels per dp; returns their width. Only measures when `draw` is false.
    float paintStrip(Canvas& canvas, const OnScreenKeyboard& keyboard, Point centre, float unit,
                     float alpha, bool draw) const;
    /// `hints` centred in `line`, shrunk from `unit` to fit its width.
    void paintHints(Canvas& canvas, std::span<const HintPrompt> hints, const Rect& line, float unit,
                    float alpha) const;

    const input::Prompts* prompts_;
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
