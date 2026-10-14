// mapping_page_painter — draws the button mapping editor in the context menu's look: a white card
// over the dimmed screen with the title and note on top, a faint controller with a chip per pad
// button and what it sends beside it (dim when it sends itself, in the accent when changed), the
// strip of chords and settings under it, the picker popover of tiles, the listening prompt over the
// dimmed board, and the button hints along the foot.
#pragma once

#include <span>
#include <string>

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
    /// The silhouette, each pad button's chip and its callout.
    void paintBoard(Canvas& canvas, const MappingPage& page, const MappingLayout& layout,
                    float alpha) const;
    /// What `item` sends, beside its chip on `slot`'s side; returns where it was drawn.
    Rect paintCallout(Canvas& canvas, const MappingItem& item, const MappingSlot& slot, float unit,
                      float alpha) const;
    /// The chords, Add chord, Steam Input and Reset under the board.
    void paintStrip(Canvas& canvas, const MappingPage& page, const MappingLayout& layout,
                    float alpha) const;
    void paintStripChip(Canvas& canvas, const MappingItem& item, const Rect& chip, float unit,
                        float alpha) const;
    /// The picker beside the focused chip.
    void paintPopover(Canvas& canvas, const MappingPage& page, const MappingPopover& popover,
                      float unit, float alpha) const;
    /// The board dimmed under the prompt to hold a chord.
    void paintListening(Canvas& canvas, const MappingPage& page, const MappingLayout& layout,
                        float alpha) const;
    /// The button prompts along the foot of the card.
    void paintHints(Canvas& canvas, const MappingPage& page, const MappingLayout& layout,
                    float alpha) const;

    /// How wide `keys` draw in a row at glyph size `box`.
    [[nodiscard]] float glyphRunWidth(std::span<const std::string> keys, float box) const;
    /// Draws `keys` in a row from `x`, centred on `centreY`; returns where the row ends.
    float paintGlyphRun(Canvas& canvas, std::span<const std::string> keys, float x, float centreY,
                        float box, Colour ink) const;

    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
