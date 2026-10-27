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

#include "gpu2d/ui/typesetter.hpp"

#include "input/prompts.hpp"
#include "on_screen_keyboard.hpp"

namespace opensu::ui {

/// With the keyboard and mouse last used, a glyph draws as the key bound to its button: a rounded
/// outline in the ring's size and stroke around the key's label.
class ButtonGlyphPainter {
  public:
    ButtonGlyphPainter(const input::Prompts& prompts, gpu2d::ui::Typesetter& text) noexcept
        : prompts_{&prompts}, text_{text} {
    }

    /// How wide the glyph for `key` is in a `box`-sized square: `box` for a controller button, and
    /// at least that for a key whose label is long.
    [[nodiscard]] float advance(std::string_view key, float box) const;

    /// Draws the glyph for `key` (`input::glyphOf`: "A", "+", "LB", "LT", "UP", "L3", "GUIDE") in a
    /// `box`-sized square centred on `centre`.
    void paint(Canvas& canvas, std::string_view key, Point centre, float box, Colour ink) const;
    /// Whether glyphs draw as keyboard keys: the keyboard and mouse were last used.
    [[nodiscard]] bool keys() const noexcept;

  private:
    /// The key cap text to draw for `key`, or nothing to draw the controller's glyph.
    [[nodiscard]] std::optional<std::string> capLabel(std::string_view key) const;

    const input::Prompts* prompts_;
    gpu2d::ui::Typesetter& text_;
};

/// The interface toolkit's prompts, drawn as the glyphs of the device last used.
class InterfacePrompts final : public gpu2d::ui::Prompts {
  public:
    InterfacePrompts(const ButtonGlyphPainter& glyphs, const OnScreenKeyboard& keyboard) noexcept
        : glyphs_{glyphs}, keyboard_{keyboard} {
    }

    [[nodiscard]] float width(gpu2d::ui::Action action, float height) override;
    void draw(gpu2d::Canvas& canvas, gpu2d::ui::Action action, const gpu2d::Rect& box,
              gpu2d::Colour ink) override;
    [[nodiscard]] bool bound(gpu2d::ui::Action action, bool typing) override;

  private:
    /// The glyph `action` shows: its button, or with the keyboard the key that does it.
    [[nodiscard]] std::string_view glyph(gpu2d::ui::Action action) const;

    const ButtonGlyphPainter& glyphs_;
    const OnScreenKeyboard& keyboard_;
};

} // namespace opensu::ui
