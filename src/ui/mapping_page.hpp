// mapping_page — the button mapping editor: a card over the details page or the Settings screen,
// titled with whose mapping it is, a line saying where the mapping comes from or why it cannot be
// changed, and one row per button and game chord with what it sends. A row's output is chosen from
// a list that replaces the rows while it is up. iiSU has no mapping editor; the card follows its
// context menu's look. Pure state and geometry, tested without a window; `MappingPagePainter` draws
// it and `app::MappingEditor` fills and acts on it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

/// One line of the editor.
struct MappingRow {
    /// The controller's name for the row, stable across refreshes.
    std::string id;
    /// Glyph keys drawn before the label: a button's, or a chord's two.
    std::vector<std::string> glyphs;
    std::string label;
    /// What the row's button or chord sends, or the row's setting.
    std::string value;

    bool operator==(const MappingRow&) const = default;
};

/// What the page asks of the pad now.
enum class MappingMode : std::uint8_t {
    /// Up and Down move along the rows.
    Rows,
    /// Up and Down move along an output list.
    Picking,
    /// A chord is being held.
    Listening,
};

/// The card and its parts in pixels.
struct MappingLayout {
    Rect card;
    Rect title;
    Rect note;
    /// The rows shown, from `first` on.
    std::vector<Rect> rows;
    std::size_t first{0};
    /// The line the button hints stand on, under the card.
    Rect hints;
    float radius{};
};

/// How many rows the card shows at once.
inline constexpr std::size_t mappingRowsShown = 8;

/// The card for `count` rows with `focus` scrolled into view, in `frame` at `dp` pixels per dp.
[[nodiscard]] MappingLayout layoutMapping(const Rect& frame, float dp, std::size_t count,
                                          std::size_t focus);

class MappingPage {
  public:
    /// Opens on the first row.
    void open(std::string title, std::vector<MappingRow> rows, std::string note);
    void close() noexcept {
        open_ = false;
        mode_ = MappingMode::Rows;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    /// Shows `rows` and `note` in place of the ones shown, keeping the focused row (by id) where it
    /// still is.
    void refresh(std::vector<MappingRow> rows, std::string note);

    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    [[nodiscard]] const std::string& note() const noexcept {
        return note_;
    }
    [[nodiscard]] const std::vector<MappingRow>& rows() const noexcept {
        return rows_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// The focused row, or null when there is none.
    [[nodiscard]] const MappingRow* focused() const;
    [[nodiscard]] MappingMode mode() const noexcept {
        return mode_;
    }
    /// "rows", "picking" or "listening", or empty while closed: what the control channel reports.
    [[nodiscard]] std::string_view state() const noexcept;

    /// Moves focus `delta` rows, or `delta` choices while picking, stopping at the ends; reports
    /// whether it moved.
    bool move(int delta) noexcept;

    /// Puts up `choices` for the focused row, titled `title`, focused on `focus`.
    void pick(std::string title, std::vector<std::string> choices, std::size_t focus);
    [[nodiscard]] const std::string& pickTitle() const noexcept {
        return pickTitle_;
    }
    [[nodiscard]] const std::vector<std::string>& choices() const noexcept {
        return choices_;
    }
    [[nodiscard]] std::size_t choice() const noexcept {
        return choice_;
    }
    /// Listens for a chord: the rows stay, the note says what to do.
    void listen(std::string note);
    /// Back to the rows from a list or from listening.
    void showRows(std::string note);

    /// The lines shown now: the rows' labels, or the choices.
    [[nodiscard]] std::size_t lines() const noexcept {
        return mode_ == MappingMode::Picking ? choices_.size() : rows_.size();
    }
    /// The line focused now.
    [[nodiscard]] std::size_t line() const noexcept {
        return mode_ == MappingMode::Picking ? choice_ : focus_;
    }
    [[nodiscard]] MappingLayout layout(const Rect& frame, float dp) const {
        return layoutMapping(frame, dp, lines(), line());
    }

  private:
    std::string title_;
    std::string note_;
    std::vector<MappingRow> rows_;
    std::size_t focus_{0};
    MappingMode mode_{MappingMode::Rows};
    std::string pickTitle_;
    std::vector<std::string> choices_;
    std::size_t choice_{0};
    bool open_{false};
};

} // namespace opensu::ui
