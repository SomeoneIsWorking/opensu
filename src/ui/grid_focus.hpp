// grid_focus — which tile a D-pad press moves focus to.
//
// iiSU hx2.z's discrete neighbour search for a horizontal grid: within the page,
// no wrap, ranked by overlap, gap, remembered lane, then index.
#pragma once

#include <cstddef>
#include <span>

#include "home_layout.hpp"

namespace iideck::ui {

enum class Direction {
    Left,
    Right,
    Up,
    Down,
};

class GridFocus {
  public:
    /// Focuses `index`, whose cell is `cell`, and remembers its column and row.
    void reset(std::size_t index, const GridCell& cell) noexcept;

    /// Moves focus one step; reports whether it moved.
    bool move(Direction direction, std::span<const GridCell> cells) noexcept;

    [[nodiscard]] std::size_t index() const noexcept {
        return index_;
    }
    [[nodiscard]] int rememberedColumn() const noexcept {
        return column_;
    }
    [[nodiscard]] int rememberedRow() const noexcept {
        return row_;
    }

  private:
    /// The best candidate on the current page, or `index_` when there is none.
    [[nodiscard]] std::size_t search(Direction direction, std::span<const GridCell> cells,
                                     bool requireLane) const noexcept;
    /// The nearest tile on the adjacent page, or `index_` when there is none.
    [[nodiscard]] std::size_t crossPage(Direction direction,
                                        std::span<const GridCell> cells) const noexcept;
    void land(std::size_t index, Direction direction, const GridCell& cell) noexcept;

    std::size_t index_{};
    int column_{};
    int row_{};
};

} // namespace iideck::ui
