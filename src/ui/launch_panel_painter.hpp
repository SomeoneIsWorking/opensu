// launch_panel_painter — draws the launch panel as a card over the dimmed home screen.
#pragma once

#include "button_glyph.hpp"
#include "launch_panel.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class LaunchPanelPainter {
  public:
    LaunchPanelPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface} {
    }

    /// Where the card and its hints stand in a `size` frame; what a pointer hits.
    [[nodiscard]] PanelLayout layout(const LaunchPanel& panel, Vector2 size, float dp) const;

    /// Draws `panel` into a `size` frame at `dp` pixels per dp. `seconds` drives the
    /// activity dots shown while the stage has no measure.
    void paint(const LaunchPanel& panel, Vector2 size, float dp, double seconds) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
