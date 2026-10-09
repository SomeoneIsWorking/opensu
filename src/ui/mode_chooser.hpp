// mode_chooser — the Library options panel: the three layout cards (Standard, XMB, Carousel), the
// icon size slider and the "Pin navigation bar" option under them, then openSU's own sort, source
// and installed/hidden filters and a way into the search.
//
// iiSU offers the cards in its "Customize Games" menu (`roms_layout_cards`, navigation.md §2.1)
// with the icon size slider (`rom_browser_xmb_icon_size`, §2.3) and the pin option; its Global
// Search is an entry of the same START menu (screens.md `g73.b`). The sort and the filters are not
// iiSU's. On Home the panel holds the rows that apply there: sort, filters and search. The panel
// scrolls when its rows are taller than the frame. Pure state, tested without a window.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "icon_size.hpp"
#include "library/sections.hpp"

namespace opensu::ui {

/// What the panel holds, top to bottom.
enum class ChooserRow : std::uint8_t {
    Cards,
    IconSize,
    Pin,
    Sort,
    Source,
    Installed,
    Hidden,
    Search,
};

/// What each option stands at, as the panel shows it.
struct ChooserValues {
    bool pinned{true};
    int iconSize{defaultIconLevel};
    std::string sort;
    std::string source;
    bool installedOnly{false};
    bool hiddenOnly{false};
};

/// One row of the panel as it stands in a frame.
struct ChooserRowBox {
    ChooserRow row;
    Rect rect;
    /// The row's switch, or the icon size slider's track; empty for the others.
    Rect control;
};

/// Where the panel, its cards and its rows stand (navigation.md §5.2,
/// `roms_layout_chooser_*_light.png`: panel 523.6 dp wide, cards 156.5 dp square 10.7 dp apart, 16
/// dp in from the panel's sides and 21 dp down from its top).
struct ChooserLayout {
    Rect panel;
    /// The part of the panel the rows scroll in.
    Rect content;
    /// Where the cards stand; empty when the panel has none.
    std::array<Rect, library::allLibraryModes.size()> cards;
    /// The rows, scrolled; a row may stand partly or wholly outside `content`.
    std::vector<ChooserRowBox> rows;
    float panelRadius{};
    float cardRadius{};

    /// The card under the point, or nothing.
    [[nodiscard]] std::optional<library::LibraryMode> cardAt(float x, float y) const noexcept;
    /// The row under the point, or nothing.
    [[nodiscard]] std::optional<ChooserRow> rowAt(float x, float y) const noexcept;
    /// The icon size a point on the slider's track stands for, or nothing off it.
    [[nodiscard]] std::optional<int> iconSizeAt(float x, float y) const noexcept;
    /// The row `row`, or null when the panel has none.
    [[nodiscard]] const ChooserRowBox* find(ChooserRow row) const noexcept;
};

/// The panel in a frame at `dp` pixels per dp, holding `rows` with `focused` scrolled into view.
[[nodiscard]] ChooserLayout layoutChooser(const Rect& frame, float dp,
                                          const std::vector<ChooserRow>& rows, ChooserRow focused);

class ModeChooser {
  public:
    /// Opens on the cards with `current` focused. `inLibrary` is whether the layout rows (cards,
    /// icon size, pin) are there; on Home they are not.
    void open(library::LibraryMode current, const ChooserValues& values, bool inLibrary);
    void close() noexcept;

    /// Moves focus `delta` cards, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Focuses `mode`; reports whether focus changed.
    bool focus(library::LibraryMode mode) noexcept;

    /// Moves between the rows (`delta` 1 is down), stopping at the ends; reports whether it moved.
    bool moveRow(int delta) noexcept;
    /// Focuses `row`; reports whether focus changed. A row the panel does not hold is not focused.
    bool focusRow(ChooserRow row) noexcept;

    /// The rows the panel holds now.
    [[nodiscard]] const std::vector<ChooserRow>& rows() const noexcept {
        return rows_;
    }
    [[nodiscard]] ChooserRow row() const noexcept {
        return row_;
    }
    [[nodiscard]] ChooserValues& values() noexcept {
        return values_;
    }
    [[nodiscard]] const ChooserValues& values() const noexcept {
        return values_;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    [[nodiscard]] library::LibraryMode focused() const noexcept {
        return focused_;
    }
    /// Where the panel stands in a frame.
    [[nodiscard]] ChooserLayout layout(const Rect& frame, float dp) const {
        return layoutChooser(frame, dp, rows_, row_);
    }

  private:
    library::LibraryMode focused_{library::LibraryMode::Standard};
    ChooserRow row_{ChooserRow::Cards};
    ChooserValues values_;
    std::vector<ChooserRow> rows_;
    bool open_{false};
};

} // namespace opensu::ui
