// mode_chooser — the Library layout picker: three cards, Standard, XMB and Carousel, one focused.
//
// iiSU offers them in its "Customize Games" menu (`roms_layout_cards`, navigation.md §2.1) with the
// "Pin navigation bar" option below; opensu has no such menu, so START on Library opens just this.
// Pure state, tested without a window.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "home_layout.hpp"
#include "library/sections.hpp"

namespace opensu::ui {

/// Where the picker's panel, its three cards and the pin option stand (navigation.md §5.2,
/// `roms_layout_chooser_*_light.png`: panel 523.6 dp wide, cards 156.5 dp square 10.7 dp apart, 16
/// dp in from the panel's sides and 21 dp down from its top). opensu's picker has the cards only,
/// so the panel is as tall as they and their padding, centred in the frame.
struct ChooserLayout {
    Rect panel;
    std::array<Rect, library::allLibraryModes.size()> cards;
    /// The "Pin navigation bar" row under the cards, and its switch inside it.
    Rect pinRow;
    Rect pinSwitch;
    float panelRadius{};
    float cardRadius{};

    /// The card under the point, or nothing.
    [[nodiscard]] std::optional<library::LibraryMode> cardAt(float x, float y) const noexcept;
};

/// What the picker holds: the cards, and the option row under them.
enum class ChooserRow : std::uint8_t { Cards, Pin };

[[nodiscard]] ChooserLayout layoutChooser(const Rect& frame, float dp) noexcept;

class ModeChooser {
  public:
    /// Opens with `current` focused and the pin option as it stands.
    void open(library::LibraryMode current, bool pinned) noexcept;
    void close() noexcept;

    /// Moves focus `delta` cards, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Focuses `mode`; reports whether focus changed.
    bool focus(library::LibraryMode mode) noexcept;

    /// Moves between the cards and the pin row (`delta` 1 is down); reports whether it moved.
    bool moveRow(int delta) noexcept;
    /// Focuses the pin row; reports whether focus changed.
    bool focusPin() noexcept;
    /// Flips the pin option and returns what it is now.
    bool togglePin() noexcept;

    [[nodiscard]] ChooserRow row() const noexcept {
        return row_;
    }
    [[nodiscard]] bool pinned() const noexcept {
        return pinned_;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    [[nodiscard]] library::LibraryMode focused() const noexcept {
        return focused_;
    }

  private:
    library::LibraryMode focused_{library::LibraryMode::Standard};
    ChooserRow row_{ChooserRow::Cards};
    bool pinned_{true};
    bool open_{false};
};

} // namespace opensu::ui
