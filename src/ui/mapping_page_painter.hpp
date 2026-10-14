// mapping_page_painter — draws the button mapping editor as the context menu looks: a white card
// over the dimmed screen, the title and the note line on top, the rows (glyphs, label, and what
// each sends at the right) or an output list under them, the focused one outlined, and the buttons
// along the foot.
#pragma once

#include "button_glyph.hpp"
#include "mapping_page.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class MappingPagePainter {
  public:
    MappingPagePainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Draws `page` into a frame of `size` at `dp` pixels per dp, as faded and scaled by `look`.
    void paint(Canvas& canvas, const MappingPage& page, Point size, float dp,
               const PanelLook& look) const;

  private:
    /// The button prompts along the foot of the card.
    void paintHints(Canvas& canvas, const MappingPage& page, const MappingLayout& layout, float dp,
                    float alpha) const;

    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
