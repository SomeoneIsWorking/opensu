// context_menu_painter — draws the context menu as iiSU's dialogs look: a white card over the
// dimmed screen, the tile's name on top, its rows under it, the focused one outlined.
#pragma once

#include "button_glyph.hpp"
#include "context_menu.hpp"
#include "panel_fade.hpp"

namespace opensu::ui {

class ContextMenuPainter {
  public:
    explicit ContextMenuPainter(const input::LastDevice& device) noexcept : glyphs_{device} {
    }

    /// Draws `menu` into a frame of `size` at `dp` pixels per dp, as faded and scaled by `look`.
    void paint(const ContextMenu& menu, Vector2 size, float dp, const PanelLook& look) const;

  private:
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
