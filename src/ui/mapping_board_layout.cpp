#include "mapping_board_layout.hpp"

#include <algorithm>
#include <array>

namespace opensu::ui {
namespace {

using gamepad::Button;

// The card at design size, in dp.
constexpr float cardWidthDp = 860.0f;
constexpr float cardHeightDp = 454.0f;
constexpr float frameMarginDp = 16.0f;
constexpr float sideDp = 30.0f;
constexpr float radiusDp = 17.0f;
constexpr Rect titleBox{sideDp, 18.0f, cardWidthDp - 2.0f * sideDp, 32.0f};
constexpr Rect noteBox{sideDp, 50.0f, cardWidthDp - 2.0f * sideDp, 22.0f};
constexpr Rect boardBox{sideDp, 80.0f, cardWidthDp - 2.0f * sideDp, 272.0f};
constexpr Rect stripBox{sideDp, 366.0f, cardWidthDp - 2.0f * sideDp, 42.0f};
constexpr Rect hintsBox{sideDp, 418.0f, cardWidthDp - 2.0f * sideDp, 28.0f};

// The board, relative to `boardBox`.
constexpr float chipWidthDp = 38.0f;
constexpr float chipHeightDp = 32.0f;
constexpr float calloutGapDp = 8.0f;
constexpr float calloutHeightDp = 22.0f;
constexpr float sideCalloutDp = 150.0f;
constexpr float innerCalloutDp = 72.0f;
constexpr float shoulderCalloutDp = 72.0f;
constexpr float middleCalloutDp = 64.0f;
constexpr float stickCalloutDp = 92.0f;
constexpr Rect bodyBox{150.0f, 50.0f, 500.0f, 172.0f};
constexpr float bodyRadiusDp = 72.0f;
constexpr Rect leftGripBox{150.0f, 120.0f, 186.0f, 152.0f};
constexpr Rect rightGripBox{464.0f, 120.0f, 186.0f, 152.0f};
constexpr float gripRadiusDp = 76.0f;

/// A pad button's place: its chip's centre and where its callout goes.
struct Place {
    Button button;
    float x;
    float y;
    CalloutSide side;
    float calloutWidth;
};

constexpr std::array<Place, 16> places{{
    {Button::L2, 210.0f, 42.0f, CalloutSide::Above, shoulderCalloutDp},
    {Button::L1, 290.0f, 42.0f, CalloutSide::Above, shoulderCalloutDp},
    {Button::R1, 510.0f, 42.0f, CalloutSide::Above, shoulderCalloutDp},
    {Button::R2, 590.0f, 42.0f, CalloutSide::Above, shoulderCalloutDp},
    {Button::Select, 366.0f, 84.0f, CalloutSide::Above, middleCalloutDp},
    {Button::Start, 434.0f, 84.0f, CalloutSide::Above, middleCalloutDp},
    {Button::Up, 255.0f, 88.0f, CalloutSide::Left, sideCalloutDp},
    {Button::Left, 213.0f, 128.0f, CalloutSide::Left, sideCalloutDp},
    {Button::Right, 297.0f, 128.0f, CalloutSide::Right, innerCalloutDp},
    {Button::Down, 255.0f, 168.0f, CalloutSide::Left, sideCalloutDp},
    {Button::Y, 545.0f, 88.0f, CalloutSide::Right, sideCalloutDp},
    {Button::X, 503.0f, 128.0f, CalloutSide::Left, innerCalloutDp},
    {Button::B, 587.0f, 128.0f, CalloutSide::Right, sideCalloutDp},
    {Button::A, 545.0f, 168.0f, CalloutSide::Right, sideCalloutDp},
    {Button::L3, 352.0f, 204.0f, CalloutSide::Below, stickCalloutDp},
    {Button::R3, 448.0f, 204.0f, CalloutSide::Below, stickCalloutDp},
}};

// The strip.
constexpr float chipGapDp = 8.0f;
constexpr float chordChipDp = 208.0f;
constexpr float plainChipDp = 120.0f;
constexpr float trailingChipDp = 220.0f;
constexpr float trailingGapDp = 12.0f;

// The picker.
constexpr float popoverPadDp = 10.0f;
constexpr float popoverGapDp = 10.0f;
constexpr float headerDp = 30.0f;
constexpr float gridWidthDp = 530.0f;
constexpr float tileHeightDp = 32.0f;
constexpr float glyphTileDp = 46.0f;
constexpr float extraGlyphDp = 28.0f;
constexpr float textTileDp = 128.0f;
constexpr float tileGapDp = 6.0f;
constexpr float groupGapDp = 16.0f;
constexpr float lineGapDp = 4.0f;
constexpr float popoverMinWidthDp = 180.0f;

Rect moved(const Rect& box, float dx, float dy) noexcept {
    return Rect{box.x + dx, box.y + dy, box.width, box.height};
}

Rect united(const Rect& one, const Rect& two) noexcept {
    const float left = std::min(one.x, two.x);
    const float top = std::min(one.y, two.y);
    return Rect{left, top, std::max(one.right(), two.right()) - left,
                std::max(one.bottom(), two.bottom()) - top};
}

/// The callout on `side` of `chip`, `width` long.
Rect calloutOf(const Rect& chip, CalloutSide side, float width) noexcept {
    const float y = chip.centreY() - calloutHeightDp * 0.5f;
    switch (side) {
    case CalloutSide::Left:
        return Rect{chip.x - calloutGapDp - width, y, width, calloutHeightDp};
    case CalloutSide::Right:
        return Rect{chip.right() + calloutGapDp, y, width, calloutHeightDp};
    case CalloutSide::Above:
        return Rect{chip.centreX() - width * 0.5f, chip.y - calloutGapDp * 0.5f - calloutHeightDp,
                    width, calloutHeightDp};
    case CalloutSide::Below:
        return Rect{chip.centreX() - width * 0.5f, chip.bottom() + calloutGapDp * 0.5f, width,
                    calloutHeightDp};
    case CalloutSide::Inside:
        break;
    }
    return chip;
}

const Place* placeOf(Button button) noexcept {
    const auto* place = std::ranges::find(places, button, &Place::button);
    return place == places.end() ? nullptr : place;
}

/// Whether `item` is a chip on the strip: not a pad button with a place on the board.
bool onStrip(const MappingItem& item) noexcept {
    return placeOf(item.button) == nullptr;
}

/// Whether `item` is one of the strip's scrolling chips, left of the trailing ones.
bool scrolls(const MappingItem& item) noexcept {
    return onStrip(item) && !item.trailing;
}

float stripChipWidth(const MappingItem& item) noexcept {
    if (item.trailing) {
        return trailingChipDp;
    }
    return item.glyphs.empty() ? plainChipDp : chordChipDp;
}

/// Every item's slot at design size, card at the origin, strip unscrolled.
std::vector<MappingSlot> placeItems(const std::vector<MappingItem>& items) {
    std::vector<MappingSlot> slots(items.size());
    float trailingLeft = stripBox.right();
    for (std::size_t i = items.size(); i-- > 0;) {
        if (onStrip(items[i]) && items[i].trailing) {
            const float width = stripChipWidth(items[i]);
            trailingLeft -= width;
            slots[i].chip = Rect{trailingLeft, stripBox.y, width, stripBox.height};
            slots[i].callout = slots[i].chip;
            trailingLeft -= chipGapDp;
        }
    }
    float x = stripBox.x;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const Place* place = placeOf(items[i].button);
        if (place == nullptr) {
            if (!items[i].trailing) {
                const float width = stripChipWidth(items[i]);
                slots[i].chip = Rect{x, stripBox.y, width, stripBox.height};
                slots[i].callout = slots[i].chip;
                x += width + chipGapDp;
            }
            continue;
        }
        const Rect chip{boardBox.x + place->x - chipWidthDp * 0.5f,
                        boardBox.y + place->y - chipHeightDp * 0.5f, chipWidthDp, chipHeightDp};
        slots[i] =
            MappingSlot{chip, calloutOf(chip, place->side, place->calloutWidth), place->side};
    }
    return slots;
}

/// The right edge of the strip's scrolling part, left of the trailing chips.
float chordWindowRight(const std::vector<MappingItem>& items,
                       const std::vector<MappingSlot>& slots) noexcept {
    float right = stripBox.right();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (onStrip(items[i]) && items[i].trailing) {
            right = std::min(right, slots[i].chip.x - trailingGapDp);
        }
    }
    return right;
}

float tileWidth(const MappingChoice& choice) noexcept {
    if (choice.glyphs.empty()) {
        return choice.wide ? 2.0f * textTileDp + tileGapDp : textTileDp;
    }
    return glyphTileDp + static_cast<float>(choice.glyphs.size() - 1) * extraGlyphDp;
}

/// How far to scroll a window `[low, high)` so `[start, end)` is whole in view.
float scrollFor(float start, float end, float low, float high) noexcept {
    return end > high ? end - high : (start < low ? start - low : 0.0f);
}

/// A design-size box scaled by `unit` and moved to the card's corner, `card` being the card in
/// pixels.
class Scaler {
  public:
    Scaler(const Rect& card, float unit) noexcept : card_{card}, unit_{unit} {
    }
    [[nodiscard]] Rect operator()(const Rect& box) const noexcept {
        return Rect{card_.x + box.x * unit_, card_.y + box.y * unit_, box.width * unit_,
                    box.height * unit_};
    }

  private:
    Rect card_;
    float unit_;
};

MappingPopover placePopover(const MappingPick& pick, std::size_t choice, const Rect& anchor,
                            const Rect& card) {
    const std::vector<Rect> grid = pickerFocusBoxes(pick.choices);
    float contentWidth = 0.0f;
    float contentHeight = 0.0f;
    for (const Rect& tile : grid) {
        contentWidth = std::max(contentWidth, tile.right());
        contentHeight = std::max(contentHeight, tile.bottom());
    }
    // Inside the card, under its title and note and clear of the button hints along its foot.
    const Rect inner{card.x + popoverPadDp, noteBox.bottom(), card.width - 2.0f * popoverPadDp,
                     hintsBox.y - popoverPadDp - noteBox.bottom()};
    const float width = std::max(contentWidth, popoverMinWidthDp) + 2.0f * popoverPadDp;
    const float bodyHeight = std::min(contentHeight, inner.height - headerDp - 2.0f * popoverPadDp);
    const float height = headerDp + bodyHeight + 2.0f * popoverPadDp;

    const float rightX = anchor.right() + popoverGapDp;
    const float leftX = anchor.x - popoverGapDp - width;
    float x = rightX;
    if (rightX + width > inner.right()) {
        x = leftX >= inner.x
                ? leftX
                : (inner.right() - anchor.right() > anchor.x - inner.x ? rightX : leftX);
    }
    x = std::clamp(x, inner.x, inner.right() - width);
    const float y = std::clamp(anchor.centreY() - height * 0.5f, inner.y, inner.bottom() - height);

    MappingPopover popover;
    popover.box = Rect{x, y, width, height};
    popover.header =
        Rect{x + popoverPadDp, y + popoverPadDp, width - 2.0f * popoverPadDp, headerDp};
    popover.body =
        Rect{popover.header.x, popover.header.bottom(), popover.header.width, bodyHeight};
    const float scroll =
        choice < grid.size() ? std::max(grid[choice].bottom() - bodyHeight, 0.0f) : 0.0f;
    popover.tiles.reserve(grid.size());
    for (const Rect& tile : grid) {
        popover.tiles.push_back(moved(tile, popover.body.x, popover.body.y - scroll));
    }
    return popover;
}

} // namespace

std::vector<Rect> mappingFocusBoxes(const std::vector<MappingItem>& items) {
    std::vector<Rect> boxes;
    boxes.reserve(items.size());
    for (const MappingSlot& slot : placeItems(items)) {
        boxes.push_back(slot.chip);
    }
    return boxes;
}

std::vector<Rect> pickerFocusBoxes(const std::vector<MappingChoice>& choices) {
    std::vector<Rect> tiles;
    tiles.reserve(choices.size());
    float x = 0.0f;
    float y = 0.0f;
    std::size_t start = 0;
    while (start < choices.size()) {
        std::size_t end = start;
        float groupWidth = -tileGapDp;
        while (end < choices.size() && choices[end].group == choices[start].group) {
            groupWidth += tileWidth(choices[end]) + tileGapDp;
            ++end;
        }
        if (x > 0.0f) {
            x += groupGapDp;
            if (x + groupWidth > gridWidthDp) {
                x = 0.0f;
                y += tileHeightDp + lineGapDp;
            }
        }
        for (std::size_t i = start; i < end; ++i) {
            const float width = tileWidth(choices[i]);
            if (x > 0.0f && x + width > gridWidthDp) {
                x = 0.0f;
                y += tileHeightDp + lineGapDp;
            }
            tiles.push_back(Rect{x, y, width, tileHeightDp});
            x += width + tileGapDp;
        }
        x -= tileGapDp;
        start = end;
    }
    return tiles;
}

MappingLayout layoutMapping(const std::vector<MappingItem>& items, std::size_t focus,
                            const MappingPick* pick, std::size_t choice, const Rect& frame,
                            float dp) {
    std::vector<MappingSlot> slots = placeItems(items);
    const Rect window{stripBox.x, stripBox.y, chordWindowRight(items, slots) - stripBox.x,
                      stripBox.height};
    if (focus < items.size() && scrolls(items[focus])) {
        const float scroll =
            scrollFor(slots[focus].chip.x, slots[focus].chip.right(), window.x, window.right());
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (scrolls(items[i])) {
                slots[i].chip = moved(slots[i].chip, -scroll, 0.0f);
                slots[i].callout = slots[i].chip;
            }
        }
    }

    const float fit = std::min({1.0f, (frame.width / dp - 2.0f * frameMarginDp) / cardWidthDp,
                                (frame.height / dp - 2.0f * frameMarginDp) / cardHeightDp});
    MappingLayout layout;
    layout.unit = dp * std::max(fit, 0.1f);
    const float width = cardWidthDp * layout.unit;
    const float height = cardHeightDp * layout.unit;
    layout.card = Rect{frame.x + (frame.width - width) * 0.5f,
                       frame.y + (frame.height - height) * 0.5f, width, height};
    const Scaler at{layout.card, layout.unit};
    const Rect card{0.0f, 0.0f, cardWidthDp, cardHeightDp};
    layout.radius = radiusDp * layout.unit;
    layout.title = at(titleBox);
    layout.note = at(noteBox);
    layout.board = at(boardBox);
    layout.silhouette = MappingSilhouette{at(moved(bodyBox, boardBox.x, boardBox.y)),
                                          at(moved(leftGripBox, boardBox.x, boardBox.y)),
                                          at(moved(rightGripBox, boardBox.x, boardBox.y)),
                                          bodyRadiusDp * layout.unit, gripRadiusDp * layout.unit};
    layout.chordWindow = at(window);
    layout.hints = at(hintsBox);
    if (pick != nullptr && focus < slots.size()) {
        MappingPopover popover =
            placePopover(*pick, choice, united(slots[focus].chip, slots[focus].callout), card);
        popover.box = at(popover.box);
        popover.header = at(popover.header);
        popover.body = at(popover.body);
        for (Rect& tile : popover.tiles) {
            tile = at(tile);
        }
        layout.popover = std::move(popover);
    }
    layout.slots.reserve(slots.size());
    for (const MappingSlot& slot : slots) {
        layout.slots.push_back(MappingSlot{at(slot.chip), at(slot.callout), slot.side});
    }
    return layout;
}

} // namespace opensu::ui
