// page_pill — draws WiiSu's page indicator (iiSU wf7.l, wf7.n).
#pragma once

#include "home_layout.hpp"

namespace opensu::ui {

class PagePillPainter {
  public:
    /// Draws `pill` in the dark or light chrome variant; `dp` is pixels per dp. Nothing is drawn
    /// for a pill with no dots.
    void paint(const PagePill& pill, float dp, bool dark) const;
};

} // namespace opensu::ui
