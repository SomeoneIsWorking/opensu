// on_screen_keyboard — the one on-screen keyboard every openSU field types on (search, a folder
// path, the administrator password): the line being typed with its cursor, and the keys in the
// style the player chose. The daisy wheel has eight petals of four characters in four sets; the
// left stick picks a petal and A, B, X and Y type its characters. The regular grid is walked with
// the D-pad. A physical keyboard types into the line either way. Pure state, tested without a
// window; `app/keyboard_pad` is its pad buttons and `on_screen_keyboard_painter` draws it.
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
#include "input/keyboard_style.hpp"

namespace opensu::ui {

/// What the line is for, which sets how long it may grow and whether it is a secret.
enum class EntryKind : std::uint8_t {
    /// The Global Search field: spaces never lead or double.
    Search,
    /// A folder path, up to a Linux PATH_MAX.
    Path,
    /// A password: shown as dots, wiped when the keyboard closes.
    Password,
};

/// The most bytes each kind of line holds.
inline constexpr std::size_t searchMostLength = 64;
inline constexpr std::size_t pathMostLength = 4096;
inline constexpr std::size_t passwordMostLength = 256;

/// What a key of the regular grid does.
enum class KeyKind : std::uint8_t { Character, Space, Backspace, Clear, Done };

/// One key of the regular grid: its place in the ten-column grid and what it types or does.
struct GridKey {
    KeyKind kind;
    /// The character typed, for `Character`.
    char character;
    std::string_view label;
    int row;
    int column;
    int span;
};

inline constexpr int gridKeyColumns = 10;
inline constexpr int gridKeyRows = 5;

/// The regular grid's keys, row by row, left to right.
[[nodiscard]] std::span<const GridKey> gridKeys() noexcept;

/// The daisy wheel's petals, clockwise from the top, and the four characters on each.
inline constexpr std::size_t daisyPetals = 8;
inline constexpr std::size_t daisyFaces = 4;

/// A face button on a petal, in the order a petal shows them: left, top, right, bottom.
enum class DaisyFace : std::uint8_t { X, Y, B, A };

/// The wheel's character sets, which LT and RT cycle.
enum class DaisySet : std::uint8_t { Letters, Capitals, Numbers, Symbols };
inline constexpr std::size_t daisySets = 4;

/// The character on `face` of `petal` in `set`.
[[nodiscard]] char daisyCharacter(DaisySet set, std::size_t petal, DaisyFace face) noexcept;
/// What the set is called on the wheel: "abc", "ABC", "123", "#+=".
[[nodiscard]] std::string_view daisySetLabel(DaisySet set) noexcept;
/// The petal the left stick's held directions point at (0 is up, then clockwise), or nothing when
/// it is centred.
[[nodiscard]] std::optional<std::size_t> petalPointedAt(bool up, bool down, bool left,
                                                        bool right) noexcept;

/// The wheel's keys a pointer can press: the eight petals, then the four faces of the picked one
/// in `DaisyFace` order.
inline constexpr std::size_t daisyKeys = daisyPetals + daisyFaces;

/// Where the keyboard's parts stand in a frame, in pixels. It docks along the bottom edge, centred,
/// no taller than a fifth of the frame.
struct KeyboardLayout {
    Rect panel;
    float panelRadius{};
    /// Pixels per dp inside the dock: the frame's, shrunk when the dock would not fit.
    float unit{};
    /// The keys a pointer can press: one per `gridKeys()` entry, or the wheel's `daisyKeys`.
    std::vector<Rect> keys;
    /// The square the wheel's ring of petals fills, the distance from its centre to each petal and
    /// a petal's radius.
    Rect ring;
    float petalDistance{};
    float petalRadius{};
    /// The square the picked petal's characters fill in the A/B/X/Y diamond, the distance from its
    /// centre to each face and a face's radius.
    Rect diamond;
    float faceDistance{};
    float faceRadius{};
    /// The one-line preview of the text; empty when the caller shows its own field.
    Rect preview;
    /// The wheel's sets and shoulder buttons, or the grid's first line of hints.
    Rect strip;
    /// The line the button hints stand on.
    Rect hints;

    [[nodiscard]] std::optional<std::size_t> keyAt(float x, float y) const noexcept;
    /// The circle `petal` fills.
    [[nodiscard]] Rect petalBounds(std::size_t petal) const noexcept;
    /// The circle `face` fills in the diamond.
    [[nodiscard]] Rect faceBounds(DaisyFace face) const noexcept;
};

/// Where `face` sits on a petal or in the diamond, as a unit step from its centre (y down).
[[nodiscard]] std::pair<float, float> faceStep(DaisyFace face) noexcept;

/// The keyboard in `style` docked in `frame`, with a preview line when `preview`.
[[nodiscard]] KeyboardLayout layoutKeyboard(const Rect& frame, float dp, input::KeyboardStyle style,
                                            bool preview = true);

/// What pressing the focused key asked for.
struct KeyPress {
    /// The line changed.
    bool edited{false};
    /// The player is done (the grid's Done key).
    bool done{false};
};

class OnScreenKeyboard {
  public:
    /// The style the next `open` uses; an open keyboard keeps its own.
    void setStyle(input::KeyboardStyle style) noexcept {
        chosen_ = style;
    }
    /// The style the keyboard shows.
    [[nodiscard]] input::KeyboardStyle style() const noexcept {
        return style_;
    }

    /// What the empty line says and what it is for.
    void configure(std::string prompt, EntryKind kind) noexcept;
    [[nodiscard]] const std::string& prompt() const noexcept {
        return prompt_;
    }
    [[nodiscard]] EntryKind kind() const noexcept {
        return kind_;
    }
    [[nodiscard]] bool secret() const noexcept {
        return kind_ == EntryKind::Password;
    }
    /// Whether the grid's letter keys type capitals; only a secret entry has the toggle (X).
    [[nodiscard]] bool shifted() const noexcept {
        return shifted_;
    }
    void toggleShift() noexcept {
        shifted_ = !shifted_;
    }

    /// Opens with `text` in the line, the cursor after it, the first key focused and the wheel on
    /// letters with no petal picked.
    void open(std::string text);
    /// Closes; a secret line is wiped.
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    [[nodiscard]] const std::string& text() const noexcept {
        return text_;
    }
    /// Hands the text over and leaves the line empty. The caller owns wiping it.
    [[nodiscard]] std::string takeText() noexcept {
        cursor_ = 0;
        return std::exchange(text_, std::string{});
    }
    /// The cursor, as a byte offset into the line on a character boundary.
    [[nodiscard]] std::size_t cursor() const noexcept {
        return cursor_;
    }
    /// Inserts typed text (UTF-8) at the cursor, up to the line's length; reports whether it
    /// changed.
    bool type(std::string_view text);
    /// Types a space, which a search refuses at the start of the line or after a space.
    bool typeSpace();
    /// Removes the character before the cursor; reports whether there was one.
    bool backspace();
    /// Empties the line; reports whether it held anything.
    bool clear();
    /// Moves the cursor one character left (`step` -1) or right (1); reports whether it moved.
    bool moveCursor(int step) noexcept;

    /// The focused key: a `gridKeys()` index, or the wheel key the pointer is on (a petal, or
    /// `daisyPetals + face`).
    [[nodiscard]] std::size_t keyFocus() const noexcept {
        return key_;
    }
    /// Focuses key `index` (a `KeyboardLayout::keys` index); reports whether focus changed.
    bool focusKey(std::size_t index);
    /// Walks the grid with the D-pad; reports whether the focus moved.
    bool move(Direction direction);
    /// Whether the grid's focus is on its top row, from which Up leaves the keys.
    [[nodiscard]] bool onTopRow() const noexcept;
    /// Presses the focused key: the grid's, or the wheel face the pointer is on (a petal under the
    /// pointer is already picked, so pressing it types nothing).
    KeyPress press();

    /// The wheel's set, and LT or RT stepping it (`step` -1 or 1), wrapping.
    [[nodiscard]] DaisySet set() const noexcept {
        return set_;
    }
    void cycleSet(int step) noexcept;
    /// A left stick direction held or let go; reports whether the picked petal changed.
    bool stick(Direction direction, bool held) noexcept;
    /// The petal picked: the stick's, else the last one the pointer rested on.
    [[nodiscard]] std::optional<std::size_t> petal() const noexcept;
    /// Whether the pointer is on a wheel key, which A then presses.
    [[nodiscard]] bool pointed() const noexcept {
        return pointed_;
    }
    /// Types `face`'s character on the picked petal; false with no petal picked.
    bool typeFace(DaisyFace face);

  private:
    std::string prompt_;
    EntryKind kind_{EntryKind::Search};
    input::KeyboardStyle chosen_{input::KeyboardStyle::DaisyWheel};
    input::KeyboardStyle style_{input::KeyboardStyle::DaisyWheel};
    bool shifted_{false};
    std::string text_;
    std::size_t cursor_{0};
    std::size_t key_{0};
    /// The column the grid focus returns to when it moves between rows, in key-grid columns.
    float anchor_{0.0f};
    DaisySet set_{DaisySet::Letters};
    /// The stick directions held: up, down, left, right.
    std::array<bool, 4> stick_{};
    bool pointed_{false};
    /// The petal the pointer last rested on, kept until the stick picks one.
    std::optional<std::size_t> pointerPetal_;
    bool open_{false};
};

} // namespace opensu::ui
