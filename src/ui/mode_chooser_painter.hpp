// mode_chooser_painter — draws the Library layout picker the way iiSU's layout page looks
// (navigation.md §5.2, `roms_layout_chooser_*_light.png`): a white panel over the dimmed screen
// holding three picture cards, Standard, XMB and Carousel, each a small sketch of its layout with
// its name over the bottom edge; the chosen card has a dark outline and a radio dot. iiSU's own
// previews are artwork it ships; these are drawn from the layouts' proportions.
#pragma once

#include "mode_chooser.hpp"
#include "raylib.h"

namespace iideck::ui {

class ModeChooserPainter {
  public:
    /// Draws `chooser` into a frame of `size` at `dp` pixels per dp; does nothing when it is
    /// closed. The focused card carries the outline: it is the mode A would choose.
    void paint(const ModeChooser& chooser, Vector2 size, float dp) const;
};

} // namespace iideck::ui
