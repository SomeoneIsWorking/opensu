// context_menu_painter — draws the context menu as iiSU's dialogs look: a white card over the
// dimmed screen, the tile's name on top, its rows under it, the focused one outlined.
#pragma once

#include "button_glyph.hpp"
#include "context_menu.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class ContextMenuPainter {
  public:
    ContextMenuPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Draws `menu` into a frame of `size` at `dp` pixels per dp, as faded and scaled by `look`.
    void paint(const ContextMenu& menu, Vector2 size, float dp, const PanelLook& look) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
