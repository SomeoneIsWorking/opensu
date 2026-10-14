#include "spatial_focus.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {
namespace {

// Offset across the direction counts for far more than distance along it: Up from the bottom of a
// diamond reaches its top, not a side, and Left along a sparse row stays in the row.
constexpr float alongWeight = 0.05f;
// A box more than this many times as far across as along is not that way at all.
constexpr float coneSlope = 2.0f;

} // namespace

std::optional<std::size_t> nearestInDirection(std::span<const Rect> boxes, std::size_t from,
                                              Direction direction) noexcept {
    if (from >= boxes.size()) {
        return std::nullopt;
    }
    const bool horizontal = direction == Direction::Left || direction == Direction::Right;
    const float sign = direction == Direction::Left || direction == Direction::Up ? -1.0f : 1.0f;
    const Rect& here = boxes[from];
    std::optional<std::size_t> best;
    float bestScore = 0.0f;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const float dx = boxes[i].centreX() - here.centreX();
        const float dy = boxes[i].centreY() - here.centreY();
        const float along = sign * (horizontal ? dx : dy);
        if (i == from || along <= 0.0f) {
            continue;
        }
        const float across = horizontal ? dy : dx;
        if (std::abs(across) > coneSlope * along) {
            continue;
        }
        const Rect& there = boxes[i];
        const float gap = std::max(
            0.0f, horizontal ? (sign > 0.0f ? there.x - here.right() : here.x - there.right())
                             : (sign > 0.0f ? there.y - here.bottom() : here.y - there.bottom()));
        const float score = alongWeight * gap * gap + across * across;
        if (!best || score < bestScore) {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

} // namespace opensu::ui
