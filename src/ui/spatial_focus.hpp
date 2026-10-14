// spatial_focus — which of a set of boxes a direction press moves to: the nearest one that way
// within a cone, by the gap between facing edges and the centres' offset across the direction, the
// offset costing more, so a box straight ahead beats a nearer diagonal one. For pages laid out
// freely, such as the mapping board and its picker; the home grid has its own (`grid_focus`).
#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "grid_focus.hpp"
#include "home_layout.hpp"

namespace opensu::ui {

/// The box `direction` moves to from `boxes[from]`, or nothing at that edge.
[[nodiscard]] std::optional<std::size_t>
nearestInDirection(std::span<const Rect> boxes, std::size_t from, Direction direction) noexcept;

} // namespace opensu::ui
