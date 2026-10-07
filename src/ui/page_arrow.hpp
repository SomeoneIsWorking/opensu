// page_arrow — draws WiiSu's previous and next page arrows (iiSU nx2.o).
#pragma once

#include "home_layout.hpp"

namespace iideck::ui {

class PageArrowPainter {
  public:
    /// Draws whichever arrows `arrows` holds, in the dark or light chrome variant.
    void paint(const PageArrows& arrows, bool dark) const;
};

} // namespace iideck::ui
