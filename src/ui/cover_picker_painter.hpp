// cover_picker_painter — draws "Choose artwork" as the context menu's card: the dimmed screen, the
// game's name and the status line, a row of cells each showing a cover whole with its label under
// it, the focused cell outlined, the button hints along the foot.
#pragma once

#include "button_glyph.hpp"
#include "cover_picker.hpp"
#include "cover_picker_art.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class CoverPickerPainter {
  public:
    CoverPickerPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    void paint(Canvas& canvas, const CoverPicker& picker, const CoverPickerArt& art, Point size,
               float dp, const PanelLook& look) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
