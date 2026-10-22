// search_panel — iiSU's Global Search panel (`g73.b`, `kl2.a`; screens.md): a card with the field
// and the results standing above the on-screen keyboard docked along the bottom edge. The panel
// fades and grows in over about 120 ms (motion.md §2.5; the Friends search's 180/120 is another
// panel). Pure state, tested without a window.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "on_screen_keyboard.hpp"

namespace opensu::ui {

/// The results listed at once.
inline constexpr std::size_t searchResultRows = 5;

/// A line of the results list: a title and what it is.
struct SearchResult {
    std::string title;
    std::string detail;

    bool operator==(const SearchResult&) const = default;
};

/// Where the panel's parts stand in a frame, in pixels: the docked keyboard, and above it the card
/// with the field and the results.
struct SearchLayout {
    KeyboardLayout keyboard;
    Rect card;
    float cardRadius{};
    Rect field;
    /// The results listed, top to bottom.
    std::array<Rect, searchResultRows> results;

    /// The card holding the field and `listed` rows; it grows down from where the field stands.
    [[nodiscard]] Rect cardFor(std::size_t listed) const noexcept;
    /// The listed row under the point (0 is the first listed), or nothing.
    [[nodiscard]] std::optional<std::size_t> resultAt(float x, float y) const noexcept;
};

/// The panel in `frame` with the keyboard in `style`.
[[nodiscard]] SearchLayout layoutSearch(const Rect& frame, float dp, input::KeyboardStyle style);

/// Which part of the panel the pad is in.
enum class SearchZone : std::uint8_t { Keys, Results };

class SearchPanel {
  public:
    SearchPanel() {
        keyboard_.configure("Search ROMs, consoles, apps, collections, and folders",
                            EntryKind::Search);
    }

    [[nodiscard]] OnScreenKeyboard& keyboard() noexcept {
        return keyboard_;
    }
    [[nodiscard]] const OnScreenKeyboard& keyboard() const noexcept {
        return keyboard_;
    }

    /// Opens with `text` in the field, on the keys.
    void open(std::string text) {
        keyboard_.open(std::move(text));
        zone_ = SearchZone::Keys;
    }
    void close() noexcept {
        keyboard_.close();
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return keyboard_.isOpen();
    }

    [[nodiscard]] SearchZone zone() const noexcept {
        return zone_;
    }
    /// Back to the keys; reports whether the zone changed.
    bool enterKeys() noexcept;
    /// Into the results at the focused one; reports whether the zone changed.
    bool enterResults();
    /// Switches between the keys and the results (iiSU's X, "navigate"); reports whether it
    /// switched.
    bool switchZone();
    /// Focuses result `index`, entering the results; reports whether focus changed.
    bool focusResult(std::size_t index);
    /// Moves the focus in the results; Down from the last returns to the keys. Reports whether it
    /// moved.
    bool moveInResults(Direction direction);

    /// The results listed, and the first one in view.
    void setResults(std::vector<SearchResult> results);
    [[nodiscard]] const std::vector<SearchResult>& results() const noexcept {
        return results_;
    }
    [[nodiscard]] std::size_t resultFocus() const noexcept {
        return result_;
    }
    [[nodiscard]] std::size_t firstListed() const noexcept {
        return first_;
    }

  private:
    void keepResultInView() noexcept;

    OnScreenKeyboard keyboard_;
    std::vector<SearchResult> results_;
    SearchZone zone_{SearchZone::Keys};
    std::size_t result_{0};
    std::size_t first_{0};
};

} // namespace opensu::ui
