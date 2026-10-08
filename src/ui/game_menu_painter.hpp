// game_menu_painter — draws the Guide menu as a panel down the left edge over a dimmed game.
#pragma once

#include "button_glyph.hpp"
#include "game_menu.hpp"

namespace iideck::ui {

class GameMenuPainter {
  public:
    /// Draws `menu` into a `width` x `height` frame at `dp` pixels per dp.
    void paint(const GameMenu& menu, float width, float height, float dp) const;

  private:
    ButtonGlyphPainter glyphs_;
};

} // namespace iideck::ui
