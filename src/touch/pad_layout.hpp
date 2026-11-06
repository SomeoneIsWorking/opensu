// pad_layout — the on-screen pad every system but arcade shows: a d-pad, A B X Y, L and R, Select
// and Start, and a pause button that holds Guide. In dp inside the safe area, hit regions larger
// than what is drawn.
#pragma once

#include "touch/touch_art.hpp"
#include "touch/touch_controls.hpp"

namespace opensu::touch {

[[nodiscard]] ControlPlan padLayout(const WindowSurface& surface, const ArtSource& art);

} // namespace opensu::touch
