// button_glyph — iiSU's controller button glyphs (res/drawable*/input_glyph_*).
//
// iiSU draws each as a white image tinted with one colour: a ring with the button's letter or
// sign inside. The images cannot ship, so this draws the same shapes: + and - from their
// 24-unit vectors (drawable-anydpi), the letters at the PNGs' measured proportions. The shoulder
// buttons' (`lb_button.png`, `rb_button.png`) are a rounded outline with one large corner and the
// button's two letters.
#pragma once

#include <string_view>

#include "raylib.h"

namespace iideck::ui {

class ButtonGlyphPainter {
  public:
    /// Draws the glyph for `key` ("A", "B", "X", "Y", "+", "-", "LB", "RB") in a `box`-sized square
    /// centred on `centre`.
    void paint(std::string_view key, Vector2 centre, float box, Color ink) const;
};

} // namespace iideck::ui
