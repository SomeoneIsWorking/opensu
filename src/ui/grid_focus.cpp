#include "grid_focus.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace iideck::ui {
namespace {

bool horizontal(Direction direction) noexcept {
    return direction == Direction::Left || direction == Direction::Right;
}

/// Lanes between a remembered lane and a span (iiSU hx2.j).
int laneDistance(int lane, int low, int high) noexcept {
    if (lane < low) {
        return low - lane;
    }
    if (lane > high) {
        return lane - high;
    }
    return 0;
}

/// Lanes from `from` to `to` along the move, or negative when `to` is not strictly beyond.
int gapAlong(Direction direction, const GridCell& from, const GridCell& to) noexcept {
    switch (direction) {
    case Direction::Left:
        return from.left - to.right;
    case Direction::Right:
        return to.left - from.right;
    case Direction::Up:
        return from.top - to.bottom;
    case Direction::Down:
        return to.top - from.bottom;
    }
    return -1;
}

/// Whether a cell covers `lane` on the axis across `direction`.
bool covers(Direction direction, const GridCell& cell, int lane) noexcept {
    const int low = horizontal(direction) ? cell.top : cell.left;
    const int high = horizontal(direction) ? cell.bottom : cell.right;
    return low <= lane && lane <= high;
}

bool overlapsAcross(Direction direction, const GridCell& a, const GridCell& b) noexcept {
    if (horizontal(direction)) {
        return a.top <= b.bottom && b.top <= a.bottom;
    }
    return a.left <= b.right && b.left <= a.right;
}

} // namespace

FocusGrid FocusGrid::of(const HomeLayout& layout) {
    FocusGrid grid;
    grid.cells = layout.cells();
    grid.frames.reserve(grid.cells.size());
    for (std::size_t index = 0; index < grid.cells.size(); ++index) {
        grid.frames.push_back(layout.contentRect(index));
    }
    for (int row = 0; row < layout.rows(); ++row) {
        grid.rowCentres.push_back(layout.rowTop(row) + layout.cellHeight() * 0.5f);
    }
    for (int column = 0; column < layout.columns(); ++column) {
        grid.columnCentres.push_back(layout.columnLeft(column, 0) + layout.cellWidth() * 0.5f);
    }
    grid.pageStride = layout.pageStride();
    grid.pageCount = layout.pageCount();
    grid.columns = layout.columns();
    grid.paged = layout.mode() == ScrollMode::Paged;
    return grid;
}

void GridFocus::reset(std::size_t index, const GridCell& cell) noexcept {
    index_ = index;
    column_ = cell.left;
    row_ = cell.top;
}

std::size_t GridFocus::search(Direction direction, const FocusGrid& grid) const noexcept {
    const GridCell& current = grid.cells[index_];
    // Left/Right ranks by the remembered row; Up/Down by the remembered column.
    const int lane = horizontal(direction) ? row_ : column_;
    using Rank = std::tuple<int, int, int, std::size_t>;
    Rank best{std::numeric_limits<int>::max(), 0, 0, 0};
    std::size_t chosen = index_;
    for (std::size_t candidate = 0; candidate < grid.cells.size(); ++candidate) {
        const GridCell& cell = grid.cells[candidate];
        // iiSU hx2.z: only cells on the same page, strictly beyond the current one.
        if (candidate == index_ || cell.page != current.page) {
            continue;
        }
        const int gap = gapAlong(direction, current, cell);
        if (gap <= 0) {
            continue;
        }
        // iiSU hx2.O: an along-flow move must land on a tile covering the remembered row.
        if (horizontal(direction) && !covers(direction, cell, lane)) {
            continue;
        }
        const int low = horizontal(direction) ? cell.top : cell.left;
        const int high = horizontal(direction) ? cell.bottom : cell.right;
        const Rank rank{overlapsAcross(direction, current, cell) ? 0 : 1, gap,
                        laneDistance(lane, low, high), candidate};
        if (rank < best) {
            best = rank;
            chosen = candidate;
        }
    }
    return chosen;
}

bool GridFocus::mayLeave(Direction direction, const FocusGrid& grid) const noexcept {
    // iiSU hx2.z :439-467: only an along-flow move is checked against the grid's edge.
    if (!horizontal(direction)) {
        return true;
    }
    const GridCell& cell = grid.cells[index_];
    const bool forward = direction == Direction::Right;
    if (grid.paged && (forward ? cell.page < grid.pageCount - 1 : cell.page > 0)) {
        return true;
    }
    return forward ? cell.right < grid.columns - 1 : cell.left > 0;
}

std::optional<float> GridFocus::reference(Direction direction,
                                          const FocusGrid& grid) const noexcept {
    const GridCell& cell = grid.cells[index_];
    if (!horizontal(direction)) {
        // iiSU hx2.z :497-513: a cross-flow move aims at the remembered column on this page.
        if (column_ < 0 || static_cast<std::size_t>(column_) >= grid.columnCentres.size()) {
            return std::nullopt;
        }
        return grid.columnCentres[static_cast<std::size_t>(column_)] +
               static_cast<float>(cell.page) * grid.pageStride;
    }
    // iiSU hx2.z :518-543: along-flow aims at the remembered row if the tile covers it.
    if (!covers(direction, cell, row_) || row_ < 0 ||
        static_cast<std::size_t>(row_) >= grid.rowCentres.size()) {
        return std::nullopt;
    }
    return grid.rowCentres[static_cast<std::size_t>(row_)];
}

std::size_t GridFocus::pixelSearch(Direction direction, const FocusGrid& grid) const {
    const bool alongX = horizontal(direction);
    const bool forward = direction == Direction::Right || direction == Direction::Down;
    // The perpendicular span of a frame: rows for Left/Right, columns for Up/Down.
    const auto low = [alongX](const Rect& rect) {
        return alongX ? rect.y : rect.x;
    };
    const auto high = [alongX](const Rect& rect) {
        return alongX ? rect.bottom() : rect.right();
    };
    const auto nearSide = [alongX](const Rect& rect) {
        return alongX ? rect.x : rect.y;
    };
    const auto farSide = [alongX](const Rect& rect) {
        return alongX ? rect.right() : rect.bottom();
    };
    const Rect& current = grid.frames[index_];
    const std::optional<float> aim = reference(direction, grid);
    const float centre = aim.value_or((low(current) + high(current)) * 0.5f);

    std::size_t chosen = index_;
    float best = std::numeric_limits<float>::max();
    for (std::size_t candidate = 0; candidate < grid.frames.size(); ++candidate) {
        if (candidate == index_) {
            continue;
        }
        // iiSU hx2.O: along-flow candidates must cover the remembered row.
        if (alongX && !covers(direction, grid.cells[candidate], row_)) {
            continue;
        }
        const Rect& frame = grid.frames[candidate];
        const float gap =
            forward ? nearSide(frame) - farSide(current) : nearSide(current) - farSide(frame);
        // iiSU hx2.z :592: the near edge is at least -0.5 px past the current far edge.
        if (gap < -0.5f) {
            continue;
        }
        float miss = 0.0f;
        if (aim) {
            miss = std::max({low(frame) - *aim, *aim - high(frame), 0.0f});
        } else {
            miss = std::max({low(frame) - high(current), low(current) - high(frame), 0.0f});
        }
        const float offset = aim ? 0.0f : std::abs((low(frame) + high(frame)) * 0.5f - centre);
        // iiSU hx2.z :621: 10000 x forward gap + 1000 x perpendicular miss + centre offset.
        const float score = gap * 10000.0f + miss * 1000.0f + offset;
        if (score < best) {
            best = score;
            chosen = candidate;
        }
    }
    return chosen;
}

void GridFocus::land(std::size_t index, Direction direction, const GridCell& cell) noexcept {
    index_ = index;
    // The axis moved along takes the lane entered; the other keeps what was intended.
    if (horizontal(direction)) {
        column_ = direction == Direction::Right ? cell.left : cell.right;
        return;
    }
    row_ = direction == Direction::Down ? cell.top : cell.bottom;
}

bool GridFocus::move(Direction direction, const FocusGrid& grid) {
    if (index_ >= grid.cells.size() || grid.frames.size() != grid.cells.size()) {
        return false;
    }
    std::size_t next = search(direction, grid);
    if (next == index_ && mayLeave(direction, grid)) {
        next = pixelSearch(direction, grid);
    }
    // iiSU hx2.z :636-639: an along-flow move with no pixel hit stays.
    // STOPGAP: hx2.z's cross-flow fallback for multi-lane tiles (:640-703) is not ported because
    // every iideck home tile is 1x1, where it never applies.
    if (next == index_) {
        return false;
    }
    land(next, direction, grid.cells[next]);
    return true;
}

} // namespace iideck::ui
