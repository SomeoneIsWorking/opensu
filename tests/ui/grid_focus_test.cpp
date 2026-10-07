// GridFocus against iiSU's neighbour rules (docs/reference/iisu/input-sound.md §1.4).
#include "grid_focus.hpp"

#include <cstdio>
#include <vector>

#include "check.hpp"
#include "home_layout.hpp"

namespace {

using iideck::test::expect;
using iideck::ui::Direction;
using iideck::ui::GridCell;
using iideck::ui::GridFocus;
using iideck::ui::HomeLayout;
using iideck::ui::HomeLayoutInput;
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

GridFocus at(const std::vector<GridCell>& cells, std::size_t index) {
    GridFocus focus;
    focus.reset(index, cells[index]);
    return focus;
}

void flowNeighbours() {
    // 11 items: columns 0-2 full, column 3 holds rows 0 and 1.
    const std::vector<GridCell> cells = window(11, ScrollMode::Flow).cells();
    GridFocus focus = at(cells, 4);
    expect(focus.move(Direction::Up, cells) && focus.index() == 3, "up stays in the column");
    expect(focus.move(Direction::Down, cells) && focus.index() == 4, "down stays in the column");
    expect(focus.move(Direction::Right, cells) && focus.index() == 7, "right keeps the row");
    expect(focus.move(Direction::Left, cells) && focus.index() == 4, "left keeps the row");
}

void noWrap() {
    const std::vector<GridCell> cells = window(11, ScrollMode::Flow).cells();
    GridFocus focus = at(cells, 0);
    expect(!focus.move(Direction::Left, cells) && focus.index() == 0, "no wrap at the left edge");
    expect(!focus.move(Direction::Up, cells) && focus.index() == 0, "no wrap at the top edge");
    focus = at(cells, 2);
    expect(!focus.move(Direction::Down, cells) && focus.index() == 2, "no wrap at the bottom edge");
    focus = at(cells, 10);
    expect(!focus.move(Direction::Right, cells) && focus.index() == 10,
           "no wrap past the last column");
}

void rememberedRowAcrossShortColumn() {
    const std::vector<GridCell> cells = window(11, ScrollMode::Flow).cells();
    GridFocus focus = at(cells, 8);
    expect(focus.move(Direction::Right, cells) && focus.index() == 10,
           "into a short column, the nearest row to the remembered one");
    expect(focus.rememberedRow() == 2, "the intended row is kept");
    expect(focus.move(Direction::Left, cells) && focus.index() == 8,
           "back out, focus returns to the remembered row");
}

void ranking() {
    // Current spans columns 1-2 on row 2; moving up.
    const std::vector<GridCell> cells{
        GridCell{1, 2, 2, 2, 0}, // 0: current
        GridCell{0, 1, 0, 1, 0}, // 1: one row up, no column overlap
        GridCell{1, 0, 1, 0, 0}, // 2: two rows up, overlaps
        GridCell{2, 1, 2, 1, 0}, // 3: one row up, overlaps, one lane from memory
        GridCell{1, 1, 1, 1, 0}, // 4: one row up, overlaps, on the remembered lane
        GridCell{1, 1, 1, 1, 1}, // 5: same as 4 on another page
    };
    GridFocus focus = at(cells, 0);
    expect(focus.move(Direction::Up, cells) && focus.index() == 4,
           "overlap, then gap, then remembered column");

    const std::vector<GridCell> overlapFirst{
        GridCell{1, 2, 1, 2, 0}, // current
        GridCell{0, 1, 0, 1, 0}, // nearer but not overlapping
        GridCell{1, 0, 1, 0, 0}, // further but overlapping
    };
    focus = at(overlapFirst, 0);
    expect(focus.move(Direction::Up, overlapFirst) && focus.index() == 2,
           "an overlapping cell beats a nearer one");

    const std::vector<GridCell> tie{
        GridCell{1, 1, 1, 1, 0}, // current
        GridCell{0, 0, 2, 0, 0}, // same rank as the next
        GridCell{0, 0, 2, 0, 0},
    };
    focus = at(tie, 0);
    expect(focus.move(Direction::Up, tie) && focus.index() == 1, "ties go to the lower index");
}

void pageCrossing() {
    // 5 columns x 3 rows per page; 37 items over 3 pages.
    const std::vector<GridCell> cells = window(37, ScrollMode::Paged).cells();
    GridFocus focus = at(cells, 13);
    expect(focus.move(Direction::Right, cells) && focus.index() == 16,
           "right off a page lands on the next page's first column, same row");
    expect(focus.move(Direction::Left, cells) && focus.index() == 13,
           "left off a page lands on the previous page's last column, same row");
    focus = at(cells, 0);
    expect(!focus.move(Direction::Left, cells), "no page before the first");
    focus = at(cells, 36);
    expect(!focus.move(Direction::Right, cells), "no page after the last");
    focus = at(cells, 14);
    expect(!focus.move(Direction::Down, cells), "up and down never leave the page");
}

} // namespace

int main() {
    flowNeighbours();
    noWrap();
    rememberedRowAcrossShortColumn();
    ranking();
    pageCrossing();
    std::printf("grid_focus: all checks passed\n");
    return 0;
}
