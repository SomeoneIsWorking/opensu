// glass — the HUD's glass pill body (iiSU ea3.n with its fill from ea3.e).
#pragma once

#include "home_layout.hpp"

namespace iideck::ui {

class GlassPainter {
  public:
    /// A fully rounded pill over `body`, in the light theme's glass fill.
    void paint(const Rect& body) const;
};

} // namespace iideck::ui
