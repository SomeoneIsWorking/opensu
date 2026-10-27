// on_screen_keyboard_painter — draws the on-screen keyboard docked along the bottom edge on a
// translucent card: the daisy wheel's ring of petals with their characters in miniature, the
// picked petal's characters large in the A/B/X/Y diamond and between them the set strip and the
// hints; or the regular grid's keys beside the hints. The field it types into is drawn above it by
// the screen that holds the field.
#pragma once

#include <vector>

#include "button_glyph.hpp"
#include "hint_row.hpp"
#include "on_screen_keyboard.hpp"
#include "panel_fade.hpp"
#include "interface_text.hpp"

namespace opensu::ui {

class OnScreenKeyboardPainter {
  public:
    OnScreenKeyboardPainter(const input::Prompts& prompts, InterfaceText& text) noexcept
        : prompts_{&prompts}, text_{text}, glyphs_{prompts, text} {
    }

    /// Draws `keyboard` docked in a `size` frame at `dp` pixels per dp, as faded and scaled by
    /// `look`.
    void paint(Canvas& canvas, const OnScreenKeyboard& keyboard, Point size, float dp,
               const PanelLook& look) const;

    /// The hints the keyboard shows for the device last used.
    [[nodiscard]] std::vector<HintPrompt> hints(const OnScreenKeyboard& keyboard) const;

  private:
    void paintGrid(Canvas& canvas, const OnScreenKeyboard& keyboard, const KeyboardLayout& layout,
                   float alpha) const;
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
    InterfaceText& text_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
