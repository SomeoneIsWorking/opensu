// chord_hint_painter — draws the held modifier's chords: a solid panel in the volume display's
// colours, centred on the screen, with the modifier's glyph over one row per chord (the partner's
// glyph, then what the chord does).
#pragma once

#include "button_glyph.hpp"
#include "chord_hint.hpp"
#include "gpu2d/ui/typesetter.hpp"
#include "paint.hpp"
#include "panel_fade.hpp"

namespace opensu::ui {

class ChordHintPainter {
  public:
    ChordHintPainter(const input::Prompts& prompts, gpu2d::ui::Typesetter& text) noexcept
        : text_{text}, glyphs_{prompts, text} {
    }

    /// Draws `hint` centred in a frame of `size` at `dp` pixels per dp.
    void paint(Canvas& canvas, const ChordHint& hint, Point size, float dp,
               const PanelLook& look) const;

  private:
    gpu2d::ui::Typesetter& text_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
