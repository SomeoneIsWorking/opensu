// search_panel_painter — draws the Global Search panel in the look of iiSU's other panels: a pale
// card over the dimmed screen, a white field, white result rows and white keys, the focused one
// outlined in ink.
#pragma once

#include "button_glyph.hpp"
#include "panel_fade.hpp"
#include "search_panel.hpp"

namespace opensu::ui {

class SearchPanelPainter {
  public:
    explicit SearchPanelPainter(const input::LastDevice& device) noexcept
        : device_{&device}, glyphs_{device} {
    }

    /// Draws `panel` into a frame of `size` at `dp` pixels per dp, as faded and scaled by `look`;
    /// `seconds` blinks the caret.
    void paint(const SearchPanel& panel, Vector2 size, float dp, const PanelLook& look,
               double seconds) const;

  private:
    const input::LastDevice* device_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
