// grid_focus — which tile a D-pad press moves focus to.
//
// iiSU hx2.z for a horizontal grid: a discrete neighbour search within the page,
// then a pixel pass over every page's tile frames, which is how focus crosses
// pages. No wrap at any edge (input-sound.md §1.4). hx2.z line numbers cite
// scratch/iisu-re/ghidra/hx2-paging-classes2.dex.c.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "home_layout.hpp"

namespace iideck::ui {

enum class Direction : std::uint8_t {
    Left,
    Right,
    Up,
    Down,
};

/// What a move searches: every focusable tile's lane cell and frame, and the lane centres.
struct FocusGrid {
    std::vector<GridCell> cells;
    /// Tile frames in content pixels, each page offset by page x pageStride.
    std::vector<Rect> frames;
    /// Centre of each row, in content pixels.
    std::vector<float> rowCentres;
    /// Centre of each column on page 0; a later page adds page x pageStride.
    std::vector<float> columnCentres;
    float pageStride{};
    int pageCount{1};
    /// Columns in the whole strip (Flow) or on one page (Paged).
    int columns{1};
    bool paged{false};

    /// The focusable tiles of `layout`.
    [[nodiscard]] static FocusGrid of(const HomeLayout& layout);
};

class GridFocus {
  public:
    /// Focuses `index`, whose cell is `cell`, and remembers its column and row.
    void reset(std::size_t index, const GridCell& cell) noexcept;

    /// Moves focus one step; reports whether it moved.
    bool move(Direction direction, const FocusGrid& grid);

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
    [[nodiscard]] std::size_t search(Direction direction, const FocusGrid& grid) const noexcept;
    /// Whether the page-edge check lets the pixel pass run.
    [[nodiscard]] bool mayLeave(Direction direction, const FocusGrid& grid) const noexcept;
    /// The nearest tile on any page by frame, or `index_` when there is none.
    [[nodiscard]] std::size_t pixelSearch(Direction direction, const FocusGrid& grid) const;
    /// The perpendicular coordinate the pixel pass measures against, if any.
    [[nodiscard]] std::optional<float> reference(Direction direction,
                                                 const FocusGrid& grid) const noexcept;
    void land(std::size_t index, Direction direction, const GridCell& cell) noexcept;

    std::size_t index_{};
    int column_{};
    int row_{};
};

} // namespace iideck::ui
