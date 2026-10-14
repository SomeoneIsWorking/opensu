// mapping_board_layout — where the mapping page's parts go: the card, the controller board with a
// chip per pad button in its place and its callout toward the nearest edge, the strip of chords and
// settings under it (scrolled to the focused chord), and the picker popover beside the focused chip
// with its tiles in lines by group (scrolled to the focused tile). Everything is laid out at a
// design size in dp and scaled down to fit the frame, so focus moves on the design-size boxes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gamepad/event.hpp"
#include "home_layout.hpp"

namespace opensu::ui {

/// One focusable thing on the page: a pad button's chip, a chord, Add chord, Steam Input or Reset.
struct MappingItem {
    /// The controller's name for it, stable across refreshes.
    std::string id;
    /// The pad button whose place on the board this is; `None` puts it on the strip.
    gamepad::Button button{gamepad::Button::None};
    /// On the strip: kept at its right end, apart from the chords.
    bool trailing{false};
    /// Glyph keys of its button, or of a chord's two.
    std::vector<std::string> glyphs;
    std::string label;
    /// What it sends, or its setting, in words.
    std::string value;
    /// What it sends as glyph keys, when that is pad buttons.
    std::vector<std::string> valueGlyphs;
    /// Whether it sends something other than itself.
    bool changed{false};

    bool operator==(const MappingItem&) const = default;
};

/// One tile of the picker.
struct MappingChoice {
    /// Drawn on the tile; a tile without glyphs shows its label.
    std::vector<std::string> glyphs;
    std::string label;
    /// Tiles of one group are kept together on a line where they fit.
    std::size_t group{0};
    /// A word tile twice as wide, for a long label.
    bool wide{false};

    bool operator==(const MappingChoice&) const = default;
};

/// What the picker offers for the focused item.
struct MappingPick {
    /// The header's glyphs, the source button or chord; without any it shows `label`.
    std::vector<std::string> glyphs;
    std::string label;
    std::vector<MappingChoice> choices;
    /// The choice in force now, marked and focused first.
    std::size_t current{0};
};

/// Which side of its chip a callout is on.
enum class CalloutSide : std::uint8_t {
    Left,
    Right,
    Above,
    Below,
    /// A strip chip writes what it does inside itself.
    Inside,
};

/// An item's chip and its callout, at `MappingPage::items()` index.
struct MappingSlot {
    Rect chip;
    Rect callout;
    CalloutSide side{CalloutSide::Inside};
};

/// The faint controller drawn under the board's chips.
struct MappingSilhouette {
    Rect body;
    Rect leftGrip;
    Rect rightGrip;
    float bodyRadius{};
    float gripRadius{};
};

/// The picker beside the focused chip.
struct MappingPopover {
    Rect box;
    Rect header;
    /// The tiles' window; tiles outside it are scrolled away.
    Rect body;
    /// One per choice.
    std::vector<Rect> tiles;
};

/// The card and its parts in pixels.
struct MappingLayout {
    /// Pixels per design dp; every size the painter draws scales with it.
    float unit{};
    Rect card;
    float radius{};
    Rect title;
    Rect note;
    Rect board;
    MappingSilhouette silhouette;
    /// One per item, the strip's chords scrolled to the focused one.
    std::vector<MappingSlot> slots;
    /// The strip's chord chips scroll inside this.
    Rect chordWindow;
    /// The line the button hints stand on.
    Rect hints;
    std::optional<MappingPopover> popover;
};

/// The chips of `items` at design size, unscrolled: what focus moves over on the board.
[[nodiscard]] std::vector<Rect> mappingFocusBoxes(const std::vector<MappingItem>& items);
/// The tiles of `choices` at design size, unscrolled: what focus moves over in the picker.
[[nodiscard]] std::vector<Rect> pickerFocusBoxes(const std::vector<MappingChoice>& choices);

/// The page for `items` with `focus` in view and, when `pick` is set, its picker beside the
/// focused chip with `choice` in view, in `frame` at `dp` pixels per dp.
[[nodiscard]] MappingLayout layoutMapping(const std::vector<MappingItem>& items, std::size_t focus,
                                          const MappingPick* pick, std::size_t choice,
                                          const Rect& frame, float dp);

} // namespace opensu::ui
