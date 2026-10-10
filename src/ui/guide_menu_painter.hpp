// guide_menu_painter — draws the Guide menu as a panel down the left edge over a dimmed screen or
// game.
#pragma once

#include "button_glyph.hpp"
#include "guide_menu.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class GuideMenuPainter {
  public:
    GuideMenuPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Where the panel and its rows stand in a `width` x `height` frame; what a pointer hits.
    [[nodiscard]] GuideLayout layout(const GuideMenu& menu, float width, float height,
                                     float dp) const;

    /// Draws `menu` into a `width` x `height` frame at `dp` pixels per dp.
    void paint(const GuideMenu& menu, float width, float height, float dp) const;

  private:
    void paintPowerButton(const Rect& box, bool focused, float dp) const;
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
