#include "grid_focus.hpp"

#include <algorithm>
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

bool overlapsAcross(Direction direction, const GridCell& a, const GridCell& b) noexcept {
    if (horizontal(direction)) {
        return a.top <= b.bottom && b.top <= a.bottom;
    }
    return a.left <= b.right && b.left <= a.right;
}

} // namespace

void GridFocus::reset(std::size_t index, const GridCell& cell) noexcept {
    index_ = index;
    column_ = cell.left;
    row_ = cell.top;
}

std::size_t GridFocus::search(Direction direction, std::span<const GridCell> cells,
                              bool requireLane) const noexcept {
    const GridCell& current = cells[index_];
    // Left/Right ranks by the remembered row; Up/Down by the remembered column.
    const int lane = horizontal(direction) ? row_ : column_;
    using Rank = std::tuple<int, int, int, std::size_t>;
    Rank best{std::numeric_limits<int>::max(), 0, 0, 0};
    std::size_t chosen = index_;
    for (std::size_t candidate = 0; candidate < cells.size(); ++candidate) {
        const GridCell& cell = cells[candidate];
        // iiSU hx2.z: only cells on the same page, strictly beyond the current one.
        if (candidate == index_ || cell.page != current.page) {
            continue;
        }
        const int gap = gapAlong(direction, current, cell);
        if (gap <= 0) {
            continue;
        }
        const int low = horizontal(direction) ? cell.top : cell.left;
        const int high = horizontal(direction) ? cell.bottom : cell.right;
        // iiSU hx2.O: an along-flow move must land on a tile covering the remembered row.
        if (requireLane && horizontal(direction) && (lane < low || lane > high)) {
            continue;
        }
        const Rank rank{overlapsAcross(direction, current, cell) ? 0 : 1, gap,
                        laneDistance(lane, low, high), candidate};
        if (rank < best) {
            best = rank;
            chosen = candidate;
        }
    }
    return chosen;
}

std::size_t GridFocus::crossPage(Direction direction,
                                 std::span<const GridCell> cells) const noexcept {
    const GridCell& current = cells[index_];
    const int page = current.page + (direction == Direction::Right ? 1 : -1);
    // The column nearest the page edge being crossed.
    int edge = direction == Direction::Right ? std::numeric_limits<int>::max() : -1;
    for (const GridCell& cell : cells) {
        if (cell.page != page) {
            continue;
        }
        edge =
            direction == Direction::Right ? std::min(edge, cell.left) : std::max(edge, cell.right);
    }
    std::size_t chosen = index_;
    std::tuple<int, std::size_t> best{std::numeric_limits<int>::max(), 0};
    for (std::size_t candidate = 0; candidate < cells.size(); ++candidate) {
        const GridCell& cell = cells[candidate];
        const bool atEdge = direction == Direction::Right ? cell.left == edge : cell.right == edge;
        if (cell.page != page || !atEdge) {
            continue;
        }
        const std::tuple<int, std::size_t> rank{laneDistance(row_, cell.top, cell.bottom),
                                                candidate};
        if (rank < best) {
            best = rank;
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

bool GridFocus::move(Direction direction, std::span<const GridCell> cells) noexcept {
    if (index_ >= cells.size()) {
        return false;
    }
    std::size_t next = search(direction, cells, true);
    if (next == index_ && horizontal(direction)) {
        // STOPGAP: iiSU hx2.z's geometric fallback (rule 4) is not decoded; reuse the ranking
        // without the lane requirement because that is the smallest rule that enters a short
        // column.
        next = search(direction, cells, false);
    }
    if (next == index_ && horizontal(direction)) {
        // STOPGAP: how iiSU crosses to the next page is not decoded; take the adjacent page's
        // nearest column at the remembered row because it is the minimal page-edge move.
        next = crossPage(direction, cells);
    }
    if (next == index_) {
        return false;
    }
    land(next, direction, cells[next]);
    return true;
}

} // namespace iideck::ui
