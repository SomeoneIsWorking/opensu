// mapping_page — the button mapping editor: a card over the details page or the Settings screen,
// titled with whose mapping it is, with a line saying where the mapping comes from or why it cannot
// be changed. Under it a controller board, each pad button a chip in its place with what it sends
// beside it, and a strip of the game chords, Add chord, Steam Input and Reset. Picking an output
// opens a popover of tiles next to the focused chip; adding a chord dims the board and asks for it.
// Focus moves spatially on the layout at its design size (`mapping_board_layout`). iiSU has no
// mapping editor; the card follows its context menu's look. Pure state, tested without a window;
// `MappingPagePainter` draws it and `app::MappingEditor` fills and acts on it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "grid_focus.hpp"
#include "mapping_board_layout.hpp"

namespace opensu::ui {

/// What the page asks of the pad now.
enum class MappingMode : std::uint8_t {
    /// The direction buttons move over the board and the strip.
    Board,
    /// The direction buttons move over the picker's tiles.
    Picking,
    /// A chord is being held.
    Listening,
};

class MappingPage {
  public:
    /// Opens focused on the first board button.
    void open(std::string title, std::vector<MappingItem> items, std::string note);
    void close() noexcept {
        open_ = false;
        mode_ = MappingMode::Board;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    /// Shows `items` and `note` in place of the ones shown, keeping the focused item (by id) where
    /// it still is.
    void refresh(std::vector<MappingItem> items, std::string note);

    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    [[nodiscard]] const std::string& note() const noexcept {
        return note_;
    }
    [[nodiscard]] const std::vector<MappingItem>& items() const noexcept {
        return items_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// The focused item, or null when there is none.
    [[nodiscard]] const MappingItem* focused() const;
    [[nodiscard]] MappingMode mode() const noexcept {
        return mode_;
    }
    /// "board", "picking" or "listening", or empty while closed: what the control channel reports.
    [[nodiscard]] std::string_view state() const noexcept;
    /// The focused item's id, or the focused choice's label while picking: what the control
    /// channel reports.
    [[nodiscard]] std::string_view focusName() const noexcept;

    /// Moves focus to the nearest item, or tile while picking, `direction`'s way; reports whether
    /// it moved.
    bool move(Direction direction);

    /// Opens the picker for the focused item.
    void pick(MappingPick pick);
    [[nodiscard]] const MappingPick& picker() const noexcept {
        return pick_;
    }
    [[nodiscard]] std::size_t choice() const noexcept {
        return choice_;
    }
    /// Listens for a chord: the board dims under `prompt`.
    void listen(std::string prompt);
    [[nodiscard]] const std::string& prompt() const noexcept {
        return prompt_;
    }
    /// Back to the board from the picker or from listening.
    void showBoard(std::string note);

    [[nodiscard]] MappingLayout layout(const Rect& frame, float dp) const {
        return layoutMapping(items_, focus_, mode_ == MappingMode::Picking ? &pick_ : nullptr,
                             choice_, frame, dp);
    }

  private:
    std::string title_;
    std::string note_;
    std::vector<MappingItem> items_;
    std::size_t focus_{0};
    MappingMode mode_{MappingMode::Board};
    MappingPick pick_;
    std::size_t choice_{0};
    std::string prompt_;
    bool open_{false};
};

} // namespace opensu::ui
