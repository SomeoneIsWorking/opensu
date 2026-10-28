// The mapping page's board: focus moving spatially over the chips and the strip, the strip
// scrolling to a chord, the picker's tiles in lines and moved over in 2D, and the popover kept
// inside the card beside the chip.
#include "mapping_page.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "input/shortcuts.hpp"

namespace {

using namespace opensu::ui;
using opensu::gamepad::Button;
using opensu::test::expect;
using opensu::test::need;

constexpr std::array padButtons{Button::A,  Button::B,    Button::X,      Button::Y,
                                Button::L1, Button::R1,   Button::L2,     Button::R2,
                                Button::L3, Button::R3,   Button::Select, Button::Start,
                                Button::Up, Button::Down, Button::Left,   Button::Right};

MappingItem stripItem(std::string id, bool trailing, std::vector<std::string> glyphs = {}) {
    MappingItem item;
    item.id = std::move(id);
    item.trailing = trailing;
    item.glyphs = std::move(glyphs);
    item.label = item.id;
    return item;
}

/// A game's items as the editor fills them: Steam Input, the pad buttons, `chords` chords, Add
/// chord and Reset.
std::vector<MappingItem> gameItems(std::size_t chords) {
    std::vector<MappingItem> items{stripItem("steam", true)};
    for (const Button button : padButtons) {
        MappingItem item;
        item.id = "button." + std::string{opensu::gamepad::name(button)};
        item.button = button;
        item.glyphs = {std::string{opensu::input::glyphOf(button)}};
        items.push_back(std::move(item));
    }
    for (std::size_t i = 0; i < chords; ++i) {
        items.push_back(stripItem("chord.l2." + std::to_string(i), false, {"LT", "A"}));
    }
    items.push_back(stripItem("add", false));
    items.push_back(stripItem("reset", true));
    return items;
}

MappingPage openPage(std::size_t chords) {
    MappingPage page;
    page.open("Controls · Kirby", gameItems(chords), "This game's own mapping");
    return page;
}

/// Moves `page` once `direction`'s way and names where focus is.
std::string step(MappingPage& page, Direction direction) {
    page.move(direction);
    return std::string{page.focusName()};
}

/// Moves `page`'s focus (its choice while picking) to `target` with direction presses, found on
/// copies of the page; reports whether a path got there.
bool walkTo(MappingPage& page, std::size_t target) {
    const bool picking = page.mode() == MappingMode::Picking;
    const auto at = [picking](const MappingPage& shown) {
        return picking ? shown.choice() : shown.focus();
    };
    std::deque<MappingPage> queue{page};
    std::set<std::size_t> seen{at(page)};
    while (!queue.empty()) {
        MappingPage next = std::move(queue.front());
        queue.pop_front();
        if (at(next) == target) {
            page = std::move(next);
            return true;
        }
        for (const Direction direction :
             {Direction::Up, Direction::Down, Direction::Left, Direction::Right}) {
            MappingPage moved = next;
            if (moved.move(direction) && seen.insert(at(moved)).second) {
                queue.push_back(std::move(moved));
            }
        }
    }
    return false;
}

bool inside(const Rect& inner, const Rect& outer) {
    constexpr float slack = 0.01f;
    return inner.x >= outer.x - slack && inner.y >= outer.y - slack &&
           inner.right() <= outer.right() + slack && inner.bottom() <= outer.bottom() + slack;
}

bool overlaps(const Rect& one, const Rect& two) {
    return one.x < two.right() && two.x < one.right() && one.y < two.bottom() &&
           two.y < one.bottom();
}

/// The index of the item `id` in `page`.
std::size_t indexOf(const MappingPage& page, std::string_view id) {
    const auto found = std::ranges::find(page.items(), id, &MappingItem::id);
    expect(found != page.items().end(), "the item is on the page");
    return static_cast<std::size_t>(found - page.items().begin());
}

/// `page` focused on the item `id`, walked there with direction presses.
MappingPage focusedOn(MappingPage page, std::string_view id) {
    expect(walkTo(page, indexOf(page, id)), "focus reaches the item");
    return page;
}

void focusMovesOverTheBoard() {
    MappingPage page = openPage(0);
    expect(page.state() == "board" && page.focusName() == "button.a", "opens on A");
    expect(step(page, Direction::Up) == "button.y", "Y straight above A, past the diagonals");
    expect(step(page, Direction::Left) == "button.start", "Start level with Y");
    page = focusedOn(openPage(0), "button.b");
    expect(step(page, Direction::Left) == "button.x", "X across the diamond from B");
    expect(step(page, Direction::Left) == "button.right", "the d-pad across the middle");
    page = openPage(0);
    expect(step(page, Direction::Down) == "steam", "the strip under A, the stick too far aside");
    expect(step(page, Direction::Left) == "add", "Left along the strip, past the board above");
    expect(step(page, Direction::Right) == "steam", "and back");

    page = focusedOn(openPage(0), "button.l2");
    expect(!page.move(Direction::Up), "the shoulders are the top edge");
    expect(step(page, Direction::Right) == "button.l1", "L1 beside L2");
    page = focusedOn(openPage(0), "button.select");
    expect(step(page, Direction::Down) == "button.l3", "the left stick below Select");
    expect(step(page, Direction::Right) == "button.r3", "the right stick beside it");
    page = focusedOn(openPage(0), "button.left");
    expect(step(page, Direction::Left) == "add", "past the d-pad's left the strip's first chip");
    expect(!page.move(Direction::Left), "which is the left edge");
}

void theStripScrollsToTheFocusedChord() {
    MappingPage page = openPage(8);
    const Rect frame{0.0f, 0.0f, 1280.0f, 800.0f};
    const MappingLayout before = page.layout(frame, 1.5f);
    const std::size_t add = indexOf(page, "add");
    expect(!inside(before.slots[add].chip, before.chordWindow),
           "with eight chords Add chord starts out of view");
    page = focusedOn(page, "add");
    const MappingLayout after = page.layout(frame, 1.5f);
    expect(inside(after.slots[add].chip, after.chordWindow), "the strip scrolls it into view");
    const Rect& reset = after.slots.back().chip;
    expect(!overlaps(reset, after.chordWindow) && inside(reset, after.card),
           "Reset stays at the right end, apart");
    expect(step(page, Direction::Left) == "chord.l2.7", "Left along the strip to the last chord");
}

void theBoardFitsTheCard() {
    const MappingPage page = openPage(1);
    for (const Rect frame : {Rect{0, 0, 1280, 800}, Rect{0, 0, 1920, 1080}, Rect{0, 0, 800, 480}}) {
        const MappingLayout layout = page.layout(frame, 1.5f);
        expect(inside(layout.card, frame), "the card fits the frame");
        expect(inside(layout.board, layout.card) && inside(layout.hints, layout.card),
               "the board and hints are on it");
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const MappingSlot& slot = layout.slots[i];
            if (slot.side == CalloutSide::Inside) {
                continue;
            }
            expect(inside(slot.chip, layout.board) && inside(slot.callout, layout.board),
                   "each chip and callout is on the board");
            for (std::size_t j = 0; j < layout.slots.size(); ++j) {
                if (j != i && layout.slots[j].side != CalloutSide::Inside) {
                    expect(!overlaps(slot.callout, layout.slots[j].chip) &&
                               (j < i || !overlaps(slot.callout, layout.slots[j].callout)),
                           "no callout runs into another chip or callout");
                }
            }
        }
    }
}

MappingPick buttonPick() {
    MappingPick pick{{"A"}, {}, {}, 0};
    const std::array<std::pair<const char*, std::size_t>, 16> keys{{{"A", 0},
                                                                    {"B", 0},
                                                                    {"X", 0},
                                                                    {"Y", 0},
                                                                    {"LT", 1},
                                                                    {"LB", 1},
                                                                    {"RB", 1},
                                                                    {"RT", 1},
                                                                    {"UP", 2},
                                                                    {"DOWN", 2},
                                                                    {"LEFT", 2},
                                                                    {"RIGHT", 2},
                                                                    {"L3", 3},
                                                                    {"R3", 3},
                                                                    {"-", 4},
                                                                    {"+", 4}}};
    for (const auto& [key, group] : keys) {
        pick.choices.push_back(MappingChoice{{key}, key, group});
    }
    pick.choices.push_back(MappingChoice{{}, "Nothing", 5});
    pick.choices.push_back(MappingChoice{{"-", "+"}, "Select + Start", 5});
    return pick;
}

void thePickerIsAGrid() {
    const std::vector<Rect> tiles = pickerFocusBoxes(buttonPick().choices);
    expect(tiles[0].y == tiles[7].y && tiles[8].y == tiles[15].y && tiles[16].y == tiles[17].y &&
               tiles[0].y < tiles[8].y && tiles[8].y < tiles[16].y,
           "face and shoulders, then d-pad, sticks and Select/Start, then Nothing and both");
    expect(tiles[4].x - tiles[3].right() > tiles[1].x - tiles[0].right(),
           "a group stands apart from the next");
    expect(tiles[17].width > tiles[0].width && tiles[16].width > tiles[17].width,
           "two glyphs take a wider tile, a word wider still");

    MappingPage page = openPage(0);
    MappingPick pick = buttonPick();
    pick.current = 1;
    page.pick(pick);
    expect(page.state() == "picking" && page.choice() == 1 && page.focusName() == "B",
           "the picker opens on the output in force");
    expect(step(page, Direction::Right) == "X" && step(page, Direction::Down) == "LEFT",
           "Right along the line, Down to the tile below");
    expect(step(page, Direction::Down) == "Select + Start" && !page.move(Direction::Down),
           "Down to the nearest tile of the last line, an edge");
    expect(step(page, Direction::Left) == "Nothing", "Nothing beside it");
    expect(step(page, Direction::Up) == "DOWN", "Up to the tile nearest above");
    page.showBoard("note");
    expect(page.state() == "board" && page.focusName() == "button.a", "B goes back to the board");
}

void thePopoverStaysInsideTheCardBesideTheChip() {
    MappingPick chord{{"LT", "A"}, {}, {}, 0};
    for (std::size_t i = 0; i < 11; ++i) {
        chord.choices.push_back(MappingChoice{{}, "Action " + std::to_string(i), 0});
    }
    for (const MappingChoice& button : buttonPick().choices) {
        if (!button.glyphs.empty() && button.glyphs.size() == 1) {
            chord.choices.push_back(MappingChoice{button.glyphs, button.label, 1 + button.group});
            chord.choices.push_back(MappingChoice{{"-", button.glyphs.front()}, button.label, 7});
        }
    }
    chord.choices.push_back(MappingChoice{{}, "Remove", 8});
    const MappingPick steam{
        {}, "Steam Input", {{{}, "As every game (Off)", 0, true}, {{}, "On"}, {{}, "Off"}}, 0};
    const Rect frame{0.0f, 0.0f, 1280.0f, 800.0f};
    for (const MappingPick& pick : {buttonPick(), chord, steam}) {
        for (std::size_t item = 0; item < gameItems(2).size(); ++item) {
            MappingPage page = openPage(2);
            expect(walkTo(page, item), "focus reaches every item");
            page.pick(pick);
            for (std::size_t choice = 0; choice < pick.choices.size(); ++choice) {
                expect(walkTo(page, choice), "focus reaches every tile");
                const MappingLayout layout = page.layout(frame, 1.5f);
                const MappingPopover& popover = need(layout.popover, "the picker is up");
                expect(inside(popover.box, layout.card) && !overlaps(popover.box, layout.hints) &&
                           !overlaps(popover.box, layout.title) &&
                           !overlaps(popover.box, layout.note),
                       "the popover is inside the card, clear of the title, note and hints");
                expect(inside(popover.body, popover.box), "its tiles' window is inside it");
                expect(inside(popover.tiles[choice], popover.body),
                       "the focused tile is scrolled into view");
            }
        }
    }

    // Beside the chip when there is room: A on the right flips the popover to its left.
    MappingPage page = openPage(0);
    page.pick(buttonPick());
    const MappingLayout layout = page.layout(frame, 1.5f);
    const MappingSlot& a = layout.slots[page.focus()];
    const Rect& box = need(layout.popover, "the picker is up").box;
    expect(!overlaps(box, a.chip) && !overlaps(box, a.callout),
           "the popover leaves the chip and its callout in view");
    expect(box.right() <= a.chip.x, "flipped to the chip's left");
}

} // namespace

int main() {
    focusMovesOverTheBoard();
    theStripScrollsToTheFocusedChord();
    theBoardFitsTheCard();
    thePickerIsAGrid();
    thePopoverStaysInsideTheCardBesideTheChip();
    std::printf("mapping_page: all checks passed\n");
    return 0;
}
