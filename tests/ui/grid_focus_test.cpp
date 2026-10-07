// GridFocus against iiSU's neighbour rules (docs/reference/iisu/input-sound.md §1.4).
#include "grid_focus.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

#include "check.hpp"
#include "home_layout.hpp"

namespace {

using iideck::test::expect;
using iideck::ui::Direction;
using iideck::ui::FocusGrid;
using iideck::ui::GridCell;
using iideck::ui::GridFocus;
using iideck::ui::HomeLayout;
using iideck::ui::HomeLayoutInput;
using iideck::ui::Rect;
using iideck::ui::ScrollMode;

HomeLayout window(std::size_t items, ScrollMode mode) {
    return HomeLayout{HomeLayoutInput{.width = 1280.0f,
                                      .height = 800.0f,
                                      .dp = 1.5f,
                                      .items = items,
                                      .mode = mode,
                                      .topInset = 60.0f,
                                      .bottomInset = 40.0f}};
}

GridFocus at(const FocusGrid& grid, std::size_t index) {
    GridFocus focus;
    focus.reset(index, grid.cells[index]);
    return focus;
}

/// A hand-made grid: 100 px lanes, pages 1000 px apart.
FocusGrid handGrid(std::vector<GridCell> cells) {
    FocusGrid grid;
    for (const GridCell& cell : cells) {
        grid.frames.push_back(Rect{static_cast<float>(cell.page * 1000 + cell.left * 100),
                                   static_cast<float>(cell.top * 100),
                                   static_cast<float>((cell.right - cell.left + 1) * 100),
                                   static_cast<float>((cell.bottom - cell.top + 1) * 100)});
    }
    grid.cells = std::move(cells);
    grid.rowCentres = {50.0f, 150.0f, 250.0f};
    grid.columnCentres = {50.0f, 150.0f, 250.0f};
    grid.pageStride = 1000.0f;
    grid.pageCount = 2;
    grid.columns = 3;
    grid.paged = true;
    return grid;
}

/// `count` 1 x 1 tiles filled column-major into 3 rows on one page, laid out by handGrid.
std::vector<GridCell> columnMajor(int count, int page = 0) {
    std::vector<GridCell> cells;
    for (int index = 0; index < count; ++index) {
        cells.push_back(GridCell{index / 3, index % 3, index / 3, index % 3, page});
    }
    return cells;
}

/// A hand-made Flow grid: one page, as many columns as the cells reach.
FocusGrid flowGrid(std::vector<GridCell> cells) {
    FocusGrid grid = handGrid(std::move(cells));
    int columns = 0;
    for (const GridCell& cell : grid.cells) {
        columns = std::max(columns, cell.right + 1);
    }
    grid.columnCentres.clear();
    for (int column = 0; column < columns; ++column) {
        grid.columnCentres.push_back(static_cast<float>(column) * 100.0f + 50.0f);
    }
    grid.pageCount = 1;
    grid.columns = columns;
    grid.paged = false;
    return grid;
}

void flowNeighbours() {
    // 11 items: columns 0-2 full, column 3 holds rows 0 and 1.
    const FocusGrid grid = FocusGrid::of(window(11, ScrollMode::Flow));
    GridFocus focus = at(grid, 4);
    expect(focus.move(Direction::Up, grid) && focus.index() == 3, "up stays in the column");
    expect(focus.move(Direction::Down, grid) && focus.index() == 4, "down stays in the column");
    expect(focus.move(Direction::Right, grid) && focus.index() == 7, "right keeps the row");
    expect(focus.move(Direction::Left, grid) && focus.index() == 4, "left keeps the row");
}

void noWrap() {
    const FocusGrid grid = FocusGrid::of(window(11, ScrollMode::Flow));
    GridFocus focus = at(grid, 0);
    expect(!focus.move(Direction::Left, grid) && focus.index() == 0, "no wrap at the left edge");
    expect(!focus.move(Direction::Up, grid) && focus.index() == 0, "no wrap at the top edge");
    focus = at(grid, 2);
    expect(!focus.move(Direction::Down, grid) && focus.index() == 2, "no wrap at the bottom edge");
    const std::size_t last = grid.cells.size() - 1;
    focus = at(grid, last);
    expect(!focus.move(Direction::Right, grid) && focus.index() == last,
           "no wrap past the last column");
}

void shortColumn() {
    // 11 tiles: columns 0-2 full, column 3 holds rows 0 and 1.
    const FocusGrid grid = flowGrid(columnMajor(11));
    GridFocus focus = at(grid, 8);
    expect(!focus.move(Direction::Right, grid) && focus.index() == 8,
           "a short column with no tile on the remembered row is not entered");
    focus = at(grid, 7);
    expect(focus.move(Direction::Right, grid) && focus.index() == 10,
           "a short column is entered on a row it has");
}

void ranking() {
    // Current spans columns 1-2 on row 2; moving up.
    const FocusGrid grid = handGrid({
        GridCell{1, 2, 2, 2, 0}, // 0: current
        GridCell{0, 1, 0, 1, 0}, // 1: one row up, no column overlap
        GridCell{1, 0, 1, 0, 0}, // 2: two rows up, overlaps
        GridCell{2, 1, 2, 1, 0}, // 3: one row up, overlaps, one lane from memory
        GridCell{1, 1, 1, 1, 0}, // 4: one row up, overlaps, on the remembered lane
        GridCell{1, 1, 1, 1, 1}, // 5: same as 4 on another page
    });
    GridFocus focus = at(grid, 0);
    expect(focus.move(Direction::Up, grid) && focus.index() == 4,
           "overlap, then gap, then remembered column");

    const FocusGrid overlapFirst = handGrid({
        GridCell{1, 2, 1, 2, 0}, // current
        GridCell{0, 1, 0, 1, 0}, // nearer but not overlapping
        GridCell{1, 0, 1, 0, 0}, // further but overlapping
    });
    focus = at(overlapFirst, 0);
    expect(focus.move(Direction::Up, overlapFirst) && focus.index() == 2,
           "an overlapping cell beats a nearer one");

    const FocusGrid tie = handGrid({
        GridCell{1, 1, 1, 1, 0}, // current
        GridCell{0, 0, 2, 0, 0}, // same rank as the next
        GridCell{0, 0, 2, 0, 0},
    });
    focus = at(tie, 0);
    expect(focus.move(Direction::Up, tie) && focus.index() == 1, "ties go to the lower index");
}

void pixelPassScore() {
    // Off the page edge, a nearer tile off the remembered row loses to one on it.
    const FocusGrid grid = handGrid({
        GridCell{2, 1, 2, 1, 0}, // 0: current, last column, row 1
        GridCell{0, 0, 1, 1, 1}, // 1: next page, rows 0-1, covers row 1
        GridCell{0, 2, 0, 2, 1}, // 2: next page, row 2 only
        GridCell{1, 1, 1, 1, 1}, // 3: next page, row 1, one column further
    });
    GridFocus focus = at(grid, 0);
    expect(focus.move(Direction::Right, grid) && focus.index() == 1,
           "the nearest frame covering the remembered row wins");
    expect(focus.rememberedColumn() == 0, "landing on a page takes its entered column");
}

void pageCrossing() {
    // 5 columns x 3 rows per page; 37 items over 4 pages of slots.
    const FocusGrid grid = FocusGrid::of(window(37, ScrollMode::Paged));
    GridFocus focus = at(grid, 13);
    expect(focus.move(Direction::Right, grid) && focus.index() == 16,
           "right off a page lands on the next page's first column, same row");
    expect(focus.move(Direction::Left, grid) && focus.index() == 13,
           "left off a page lands on the previous page's last column, same row");
    focus = at(grid, 14);
    expect(focus.move(Direction::Right, grid) && focus.index() == 17, "the bottom row crosses");
    focus = at(grid, 0);
    expect(!focus.move(Direction::Left, grid), "no page before the first");
    focus = at(grid, grid.cells.size() - 1);
    expect(!focus.move(Direction::Right, grid), "no page after the last");
    focus = at(grid, 14);
    expect(!focus.move(Direction::Down, grid), "up and down never leave the page");
}

void pageCrossingNeedsRow() {
    // Page 0 is full; page 1 holds one tile, on row 0.
    std::vector<GridCell> cells = columnMajor(9);
    cells.push_back(GridCell{0, 0, 0, 0, 1});
    const FocusGrid grid = handGrid(std::move(cells));
    GridFocus focus = at(grid, 8);
    expect(!focus.move(Direction::Right, grid) && focus.index() == 8,
           "no tile on the next page covers the remembered row");
    expect(focus.move(Direction::Up, grid) && focus.move(Direction::Up, grid) && focus.index() == 6,
           "up the last column");
    expect(focus.move(Direction::Right, grid) && focus.index() == 9,
           "the next page's tile on the remembered row");
}

} // namespace

int main() {
    flowNeighbours();
    noWrap();
    shortColumn();
    ranking();
    pixelPassScore();
    pageCrossing();
    pageCrossingNeedsRow();
    std::printf("grid_focus: all checks passed\n");
    return 0;
}
