// hint_row — a centred row of button prompts, each a glyph and its label, as the cards draw along
// their foot (the context menu, the mapping editor).
#pragma once

#include <span>
#include <utility>

#include "button_glyph.hpp"
#include "paint.hpp"
#include "gpu2d/ui/typesetter.hpp"

namespace opensu::ui {

/// A glyph key ("A") and what its button does ("Close").
using HintPrompt = std::pair<const char*, const char*>;

/// How wide `hints` stand at `dp` pixels per dp.
[[nodiscard]] float hintRowWidth(const ButtonGlyphPainter& glyphs, gpu2d::ui::Typesetter& text,
                                 std::span<const HintPrompt> hints, float dp);

/// Draws `hints` centred on `centre` at `dp` pixels per dp in `ink`.
void paintHintRow(Canvas& canvas, const ButtonGlyphPainter& glyphs, gpu2d::ui::Typesetter& text,
                  std::span<const HintPrompt> hints, Point centre, float dp, Colour ink);

} // namespace opensu::ui
