// search_panel — iiSU's Global Search panel (`g73.b`, `kl2.a`; screens.md): a field, the results
// under it and, for a player with only a pad, an on-screen keyboard. The panel fades and grows in
// over about 120 ms (motion.md §2.5; the Friends search's 180/120 is another panel). A key from a
// physical keyboard goes straight into the field; the keys of the drawn keyboard are walked with
// the D-pad. Pure state, tested without a window.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "host/secret.hpp"

namespace opensu::ui {

/// What a key of the drawn keyboard does.
enum class KeyKind : std::uint8_t { Character, Space, Backspace, Clear, Done };

/// One key: its place in the ten-column grid and what it types or does.
struct SearchKey {
    KeyKind kind;
    /// The character typed, for `Character`.
    char character;
    std::string_view label;
    int row;
    int column;
    int span;
};

inline constexpr int searchKeyColumns = 10;
inline constexpr int searchKeyRows = 5;
/// The results listed at once.
inline constexpr std::size_t searchResultRows = 3;
/// The most characters the field holds.
inline constexpr std::size_t searchMostLength = 64;
/// The most bytes a typed path holds, a Linux PATH_MAX.
inline constexpr std::size_t pathMostLength = 4096;
/// The most bytes a typed password holds.
inline constexpr std::size_t passwordMostLength = 256;

/// The keys, row by row, left to right.
[[nodiscard]] std::span<const SearchKey> searchKeys() noexcept;

/// A line of the results list: a title and what it is.
struct SearchResult {
    std::string title;
    std::string detail;

    bool operator==(const SearchResult&) const = default;
};

/// Where the panel's parts stand in a frame, in pixels.
struct SearchLayout {
    Rect panel;
    Rect field;
    /// The results listed, top to bottom.
    std::array<Rect, searchResultRows> results;
    /// One rectangle per `searchKeys()` entry.
    std::vector<Rect> keys;
    /// The line the button hints stand on, and its height.
    Rect hints;
    float panelRadius{};

    [[nodiscard]] std::optional<std::size_t> keyAt(float x, float y) const noexcept;
    /// The listed row under the point (0 is the first listed), or nothing.
    [[nodiscard]] std::optional<std::size_t> resultAt(float x, float y) const noexcept;
};

/// The panel in `frame`; without `lists`, the room for the results is left out.
[[nodiscard]] SearchLayout layoutSearch(const Rect& frame, float dp, bool lists = true);

/// Which part of the panel the D-pad is in.
enum class SearchZone : std::uint8_t { Keys, Results };

/// What pressing the focused key or result asked for.
struct SearchPress {
    /// The text in the field changed.
    bool edited{false};
    /// The panel should close.
    bool close{false};
    /// The result to open, by its index in the results.
    std::optional<std::size_t> open;
};

class SearchPanel {
  public:
    /// What the empty field says, and whether the panel lists results. A search does; a text
    /// entry such as a folder path does not. A `secret` entry is a password: the field shows dots
    /// instead of the text, the keyboard has a Caps key, and the text is wiped when the panel
    /// closes.
    void configure(std::string prompt, bool lists, bool secret = false) {
        prompt_ = std::move(prompt);
        lists_ = lists;
        secret_ = secret;
        shifted_ = false;
        mostLength_ = secret ? passwordMostLength : (lists ? searchMostLength : pathMostLength);
    }
    [[nodiscard]] bool secret() const noexcept {
        return secret_;
    }
    /// Whether the letter keys type capitals; only a secret entry has the toggle.
    [[nodiscard]] bool shifted() const noexcept {
        return shifted_;
    }
    void toggleShift() noexcept {
        shifted_ = !shifted_;
    }
    [[nodiscard]] const std::string& prompt() const noexcept {
        return prompt_;
    }
    [[nodiscard]] bool lists() const noexcept {
        return lists_;
    }

    /// Opens with `text` in the field and the first key focused.
    void open(std::string text);
    void close() noexcept {
        open_ = false;
        if (secret_) {
            host::wipe(text_);
        }
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    [[nodiscard]] const std::string& text() const noexcept {
        return text_;
    }
    /// Hands the text over and leaves the field empty. The caller owns wiping it.
    [[nodiscard]] std::string takeText() noexcept {
        return std::exchange(text_, std::string{});
    }
    /// Appends typed text (UTF-8) to the field, up to its length; reports whether it changed.
    bool type(std::string_view text);
    /// Removes the last character; reports whether there was one.
    bool backspace();
    /// Empties the field; reports whether it held anything.
    bool clear();

    [[nodiscard]] SearchZone zone() const noexcept {
        return zone_;
    }
    [[nodiscard]] std::size_t keyFocus() const noexcept {
        return key_;
    }
    /// Moves the D-pad focus; reports whether it moved. Up from the top key row enters the
    /// results and Down from the last result returns to the keys.
    bool move(Direction direction);
    /// Switches between the keys and the results (iiSU's X, "navigate"); reports whether it
    /// switched.
    bool switchZone();
    /// Focuses key `index` or result `index`; reports whether focus changed.
    bool focusKey(std::size_t index);
    bool focusResult(std::size_t index);
    /// Presses the focused key, or opens the focused result.
    SearchPress press();

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

    std::string prompt_{"Search ROMs, consoles, apps, collections, and folders"};
    bool lists_{true};
    bool secret_{false};
    bool shifted_{false};
    std::size_t mostLength_{searchMostLength};
    std::string text_;
    std::vector<SearchResult> results_;
    SearchZone zone_{SearchZone::Keys};
    std::size_t key_{0};
    /// The column the focus returns to when it moves between rows, in key-grid columns.
    float anchor_{0.0f};
    std::size_t result_{0};
    std::size_t first_{0};
    bool open_{false};
};

} // namespace opensu::ui
