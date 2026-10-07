// page_pill — draws WiiSu's page indicator (iiSU wf7.l, wf7.n).
#pragma once

#include "home_layout.hpp"

namespace iideck::ui {

class PagePillPainter {
  public:
    /// Draws `pill`; `dp` is pixels per dp. Nothing is drawn for a pill with no dots.
    void paint(const PagePill& pill, float dp) const;
};

} // namespace iideck::ui
