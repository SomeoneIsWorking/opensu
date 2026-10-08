// launch_panel_painter — draws the launch panel as a card over the dimmed home screen.
#pragma once

#include "button_glyph.hpp"
#include "launch_panel.hpp"

namespace iideck::ui {

class LaunchPanelPainter {
  public:
    /// Draws `panel` into a `size` frame at `dp` pixels per dp. `seconds` drives the
    /// activity dots shown while the stage has no measure.
    void paint(const LaunchPanel& panel, Vector2 size, float dp, double seconds) const;

  private:
    ButtonGlyphPainter glyphs_;
};

} // namespace iideck::ui
