// button_glyph — iiSU's controller button glyphs (res/drawable*/input_glyph_*).
//
// iiSU draws each as a white image tinted with one colour: a ring with the button's letter or
// sign inside. The images cannot ship, so this draws the same shapes: + and - from their
// 24-unit vectors (drawable-anydpi), the letters at the PNGs' measured proportions. The shoulder
// buttons' (`lb_button.png`, `rb_button.png`) are a rounded outline with one large corner and the
// button's two letters.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "raylib.h"

#include "input/last_device.hpp"

namespace iideck::ui {

/// With the keyboard and mouse last used, a glyph draws as the key bound to its button: a rounded
/// outline in the ring's size and stroke around the key's label.
class ButtonGlyphPainter {
  public:
    explicit ButtonGlyphPainter(const input::LastDevice& device) noexcept : device_{&device} {
    }

    /// How wide the glyph for `key` is in a `box`-sized square: `box` for a controller button, and
    /// at least that for a key whose label is long.
    [[nodiscard]] float advance(std::string_view key, float box) const;

    /// Draws the glyph for `key` ("A", "B", "X", "Y", "+", "-", "LB", "RB") in a `box`-sized square
    /// centred on `centre`.
    void paint(std::string_view key, Vector2 centre, float box, Color ink) const;

  private:
    /// The key cap text to draw for `key`, or nothing to draw the controller's glyph.
    [[nodiscard]] std::optional<std::string> capLabel(std::string_view key) const;

    const input::LastDevice* device_;
};

} // namespace iideck::ui
