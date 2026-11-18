#include "touch/layout_parts.hpp"

#include <utility>

namespace opensu::touch {

Piece labelIn(const Box& box, Art art, std::vector<gamepad::Button> lit, float idle) {
    const auto width = static_cast<float>(art.width);
    const auto height = static_cast<float>(art.height);
    return Piece{
        .box = {box.centreX() - (width * 0.5F), box.centreY() - (height * 0.5F), width, height},
        .art = std::move(art),
        .down = {},
        .lit = std::move(lit),
        .idle = idle,
        .lean = 0.0F};
}

} // namespace opensu::touch
