// button_glyph — iiSU's controller button glyphs (res/drawable*/input_glyph_*).
//
// iiSU draws each as a white image tinted with one colour: a ring with the button's letter or
// sign inside. The images cannot ship, so this draws the same shapes: + and - from their
// 24-unit vectors (drawable-anydpi), the letters at the PNGs' measured proportions. The shoulder
// buttons' (`lb_button.png`, `rb_button.png`) are a rounded outline with one large corner and the
// button's two letters; LT and RT take the same shape. iiSU has no d-pad, stick-click or Guide
// glyph: openSU draws a direction as the ring around an arrowhead and the rest as a cap with the
// button's name.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "paint.hpp"

#include "input/prompts.hpp"
#include "typeface.hpp"

namespace opensu::ui {

/// With the keyboard and mouse last used, a glyph draws as the key bound to its button: a rounded
/// outline in the ring's size and stroke around the key's label.
class ButtonGlyphPainter {
  public:
    ButtonGlyphPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : prompts_{&prompts}, typeface_{typeface} {
    }

    /// How wide the glyph for `key` is in a `box`-sized square: `box` for a controller button, and
    /// at least that for a key whose label is long.
    [[nodiscard]] float advance(std::string_view key, float box) const;

    /// Draws the glyph for `key` (`input::glyphOf`: "A", "+", "LB", "LT", "UP", "L3", "GUIDE") in a
    /// `box`-sized square centred on `centre`.
    void paint(Canvas& canvas, std::string_view key, Point centre, float box, Colour ink) const;

  private:
    /// The key cap text to draw for `key`, or nothing to draw the controller's glyph.
    [[nodiscard]] std::optional<std::string> capLabel(std::string_view key) const;

    const input::Prompts* prompts_;
    Typeface& typeface_;
};

} // namespace opensu::ui
