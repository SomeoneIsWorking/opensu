// chord_hint_painter — draws the held modifier's chords: a solid panel in the volume display's
// colours, centred on the screen, with the modifier's glyph over one row per chord (the partner's
// glyph, then what the chord does).
#pragma once

#include "button_glyph.hpp"
#include "chord_hint.hpp"
#include "paint.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class ChordHintPainter {
  public:
    ChordHintPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Draws `hint` centred in a frame of `size` at `dp` pixels per dp.
    void paint(Canvas& canvas, const ChordHint& hint, Point size, float dp,
               const PanelLook& look) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
