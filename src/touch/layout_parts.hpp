// layout_parts — pieces every layout makes alike.
#pragma once

#include <vector>

#include "gamepad/event.hpp"
#include "touch/touch_controls.hpp"

namespace opensu::touch {

/// `art`, a word or a drawing, centred in `box`, stronger while a finger holds any of `lit`.
[[nodiscard]] Piece labelIn(const Box& box, Art art, std::vector<gamepad::Button> lit, float idle);

} // namespace opensu::touch
