// side_panel_painter — the chrome of a panel down the right edge (the quick menu, the game
// settings tabs): the dimmed screen or game, the panel's ground, its heading and the button hints
// along its bottom.
#pragma once

#include <string>
#include <vector>

#include "button_glyph.hpp"
#include "home_layout.hpp"
#include "launch_panel.hpp"
#include "paint.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class SidePanelPainter {
  public:
    SidePanelPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// The heading line's height and the hints' room at the bottom, in pixels.
    [[nodiscard]] float headingBox(float dp) const;
    [[nodiscard]] static float footer(float dp) noexcept;

    /// The scrim over a `width` x `height` frame and the panel's ground in `panel`.
    void paintGround(Canvas& canvas, const Rect& panel, float width, float height) const;
    /// `text` as the heading, left-aligned in `heading`.
    void paintHeading(Canvas& canvas, const std::string& text, const Rect& heading, float dp) const;
    /// `hints` along the bottom of a `height` frame from `x`, `padding` above its edge.
    void paintHints(Canvas& canvas, const std::vector<PanelHint>& hints, float x, float height,
                    float padding, float dp) const;
    /// The glyph for `key` in a `box`-sized square centred on `centre`.
    void paintGlyph(Canvas& canvas, const std::string& key, Point centre, float box,
                    Colour ink) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
