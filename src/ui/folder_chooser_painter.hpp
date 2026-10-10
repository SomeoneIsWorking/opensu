// folder_chooser_painter — draws the folder chooser in the look of the options panel: a pale card
// over the dimmed screen, white rows, the focused one outlined.
#pragma once

#include "button_glyph.hpp"
#include "folder_chooser.hpp"
#include "panel_fade.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class FolderChooserPainter {
  public:
    FolderChooserPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Draws `chooser` into a frame of `size` at `dp` pixels per dp, as faded and scaled by `look`.
    void paint(const FolderChooser& chooser, Vector2 size, float dp, const PanelLook& look) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
