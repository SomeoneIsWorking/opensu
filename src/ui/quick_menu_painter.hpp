// quick_menu_painter — draws the quick menu as a panel down the right edge over a dimmed screen or
// game, its rows painted as the Settings screen paints them.
#pragma once

#include "button_glyph.hpp"
#include "quick_menu.hpp"

namespace opensu::ui {

class QuickMenuPainter {
  public:
    explicit QuickMenuPainter(const input::Prompts& prompts) noexcept : glyphs_{prompts} {
    }

    /// Where the panel and its rows stand in a `width` x `height` frame; what a pointer hits.
    [[nodiscard]] QuickLayout layout(const QuickMenu& menu, float width, float height,
                                     float dp) const;

    /// Draws `menu` into a `width` x `height` frame at `dp` pixels per dp.
    void paint(const QuickMenu& menu, float width, float height, float dp) const;

  private:
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
