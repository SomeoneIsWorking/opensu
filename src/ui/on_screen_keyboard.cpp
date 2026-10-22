#include "on_screen_keyboard.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>

#include "host/secret.hpp"

namespace opensu::ui {
namespace {

/// The dock with its margin is a fifth of iiSU's 480 dp reference height.
constexpr float dockHeightDp = 92.0f;
constexpr float dockMarginDp = 4.0f;
constexpr float dockPaddingDp = 5.0f;
constexpr float dockRadiusDp = 14.0f;
constexpr float dockShare = 0.2f;
constexpr float columnGapDp = 14.0f;
constexpr float previewHeightDp = 24.0f;
constexpr float stripHeightDp = 24.0f;
constexpr float hintHeightDp = 22.0f;
constexpr float rowGapDp = 5.0f;
constexpr float petalDistanceDp = 30.0f;
constexpr float petalRadiusDp = 10.5f;
constexpr float faceDistanceDp = 26.0f;
constexpr float faceRadiusDp = 13.5f;
constexpr float wheelColumnDp = 340.0f;
constexpr float keyWidthDp = 40.0f;
constexpr float keyGapDp = 3.0f;
constexpr float keyRowGapDp = 2.5f;
constexpr float gridColumnDp = 220.0f;

constexpr std::size_t keyCount = static_cast<std::size_t>(gridKeyColumns) * 4 + 5;

// What the grid's character keys type, and what they are drawn as (letters as capitals).
constexpr std::string_view typed = "1234567890qwertyuiopasdfghjkl'zxcvbnm-.&";
constexpr std::string_view caps = "1234567890QWERTYUIOPASDFGHJKL'ZXCVBNM-.&";

constexpr std::array<GridKey, keyCount> makeKeys() {
    std::array<GridKey, keyCount> keys{};
    for (std::size_t i = 0; i < typed.size(); ++i) {
        keys[i] = GridKey{KeyKind::Character,
                          typed[i],
                          caps.substr(i, 1),
                          static_cast<int>(i) / gridKeyColumns,
                          static_cast<int>(i) % gridKeyColumns,
                          1};
    }
    // The slash makes folder paths typeable; the space key gives up a column to it.
    keys[typed.size()] = GridKey{KeyKind::Character, '/', "/", 4, 0, 1};
    keys[typed.size() + 1] = GridKey{KeyKind::Space, ' ', "Space", 4, 1, 3};
    keys[typed.size() + 2] = GridKey{KeyKind::Backspace, '\0', "Delete", 4, 4, 2};
    keys[typed.size() + 3] = GridKey{KeyKind::Clear, '\0', "Clear", 4, 6, 2};
    keys[typed.size() + 4] = GridKey{KeyKind::Done, '\0', "Done", 4, 8, 2};
    return keys;
}

constexpr std::array<GridKey, keyCount> keyTable = makeKeys();

// Each set's petals, clockwise from the top, four characters each in the order X, Y, B, A.
constexpr std::array<std::string_view, daisySets> daisyTable{
    "abcdefghijklmnopqrstuvwxyz.,/-_'",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ!?:;&\"",
    "1234567890.,+-*/=%()<>[]#$&@_:;~",
    "!?'\"@#$%^&*~()[]{}<>-_=+\\|/`:;,.",
};
static_assert(daisyTable[0].size() == daisyPetals * daisyFaces &&
              daisyTable[1].size() == daisyPetals * daisyFaces &&
              daisyTable[2].size() == daisyPetals * daisyFaces &&
              daisyTable[3].size() == daisyPetals * daisyFaces);

float centreOf(const GridKey& key) noexcept {
    return static_cast<float>(key.column) + static_cast<float>(key.span - 1) * 0.5f;
}

/// Bytes the character before `at` in `text` takes.
std::size_t characterBytesBefore(const std::string& text, std::size_t at) {
    std::size_t start = at;
    while (start > 0 && (static_cast<unsigned char>(text[start - 1]) & 0xC0U) == 0x80U) {
        --start;
    }
    return start > 0 ? at - (start - 1) : 0;
}

/// Bytes the character at `at` in `text` takes.
std::size_t characterBytesAt(const std::string& text, std::size_t at) {
    if (at >= text.size()) {
        return 0;
    }
    std::size_t end = at + 1;
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
        ++end;
    }
    return end - at;
}

/// `OnScreenKeyboard::stick_`'s slot for `direction`.
std::size_t stickSlot(Direction direction) noexcept {
    switch (direction) {
    case Direction::Up:
        return 0;
    case Direction::Down:
        return 1;
    case Direction::Left:
        return 2;
    case Direction::Right:
        return 3;
    }
    return 0;
}

} // namespace

std::pair<float, float> faceStep(DaisyFace face) noexcept {
    switch (face) {
    case DaisyFace::X:
        return {-1.0f, 0.0f};
    case DaisyFace::Y:
        return {0.0f, -1.0f};
    case DaisyFace::B:
        return {1.0f, 0.0f};
    case DaisyFace::A:
        return {0.0f, 1.0f};
    }
    return {};
}

std::span<const GridKey> gridKeys() noexcept {
    return keyTable;
}

char daisyCharacter(DaisySet set, std::size_t petal, DaisyFace face) noexcept {
    return daisyTable[static_cast<std::size_t>(set)]
                     [petal * daisyFaces + static_cast<std::size_t>(face)];
}

std::string_view daisySetLabel(DaisySet set) noexcept {
    switch (set) {
    case DaisySet::Letters:
        return "abc";
    case DaisySet::Capitals:
        return "ABC";
    case DaisySet::Numbers:
        return "123";
    case DaisySet::Symbols:
        return "#+=";
    }
    return {};
}

std::optional<std::size_t> petalPointedAt(bool up, bool down, bool left, bool right) noexcept {
    const int x = (right ? 1 : 0) - (left ? 1 : 0);
    const int y = (down ? 1 : 0) - (up ? 1 : 0);
    // Clockwise from the top, by (x, y) with y growing downwards.
    constexpr std::array<std::pair<int, int>, daisyPetals> petals{
        {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};
    for (std::size_t petal = 0; petal < petals.size(); ++petal) {
        if (petals[petal] == std::pair{x, y}) {
            return petal;
        }
    }
    return std::nullopt;
}

KeyboardLayout layoutKeyboard(const Rect& frame, float dp, input::KeyboardStyle style,
                              bool preview) {
    KeyboardLayout layout;
    const bool wheel = style == input::KeyboardStyle::DaisyWheel;
    const float keysDp = wheel ? 2.0f * (petalDistanceDp + petalRadiusDp)
                               : static_cast<float>(gridKeyColumns) * keyWidthDp +
                                     static_cast<float>(gridKeyColumns - 1) * keyGapDp;
    const float columnDp = wheel ? wheelColumnDp : gridColumnDp;
    const float diamondDp = wheel ? 2.0f * (faceDistanceDp + faceRadiusDp) + columnGapDp : 0.0f;
    const float widthDp = 2.0f * dockPaddingDp + keysDp + columnGapDp + columnDp + diamondDp;
    const float unit = std::min({dp, frame.height * dockShare / (dockHeightDp + dockMarginDp),
                                 frame.width / (widthDp + 2.0f * dockMarginDp)});
    layout.unit = unit;
    const float width = widthDp * unit;
    const float height = dockHeightDp * unit;
    layout.panel = Rect{frame.x + (frame.width - width) * 0.5f,
                        frame.bottom() - dockMarginDp * unit - height, width, height};
    layout.panelRadius = dockRadiusDp * unit;
    const float pad = dockPaddingDp * unit;
    const float inner = height - 2.0f * pad;
    const float top = layout.panel.y + pad;
    float x = layout.panel.x + pad;
    if (wheel) {
        layout.ring = Rect{x, top + (inner - keysDp * unit) * 0.5f, keysDp * unit, keysDp * unit};
        layout.petalDistance = petalDistanceDp * unit;
        layout.petalRadius = petalRadiusDp * unit;
        const float diamond = 2.0f * (faceDistanceDp + faceRadiusDp) * unit;
        layout.diamond = Rect{layout.panel.right() - pad - diamond, top + (inner - diamond) * 0.5f,
                              diamond, diamond};
        layout.faceDistance = faceDistanceDp * unit;
        layout.faceRadius = faceRadiusDp * unit;
        for (std::size_t petal = 0; petal < daisyPetals; ++petal) {
            layout.keys.push_back(layout.petalBounds(petal));
        }
        for (std::size_t face = 0; face < daisyFaces; ++face) {
            layout.keys.push_back(layout.faceBounds(static_cast<DaisyFace>(face)));
        }
    } else {
        const float keyHeight = (inner - static_cast<float>(gridKeyRows - 1) * keyRowGapDp * unit) /
                                static_cast<float>(gridKeyRows);
        for (const GridKey& key : gridKeys()) {
            const auto span = static_cast<float>(key.span);
            layout.keys.push_back(
                Rect{x + static_cast<float>(key.column) * (keyWidthDp + keyGapDp) * unit,
                     top + static_cast<float>(key.row) * (keyHeight + keyRowGapDp * unit),
                     (span * keyWidthDp + (span - 1.0f) * keyGapDp) * unit, keyHeight});
        }
    }
    x += (keysDp + columnGapDp) * unit;
    const float column = columnDp * unit;
    const float gap = rowGapDp * unit;
    const float rows = (stripHeightDp + hintHeightDp) * unit + gap +
                       (preview ? previewHeightDp * unit + gap : 0.0f);
    float y = top + (inner - rows) * 0.5f;
    if (preview) {
        layout.preview = Rect{x, y, column, previewHeightDp * unit};
        y += previewHeightDp * unit + gap;
    }
    layout.strip = Rect{x, y, column, stripHeightDp * unit};
    y += stripHeightDp * unit + gap;
    layout.hints = Rect{x, y, column, hintHeightDp * unit};
    return layout;
}

std::optional<std::size_t> KeyboardLayout::keyAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

Rect KeyboardLayout::petalBounds(std::size_t petal) const noexcept {
    const double angle =
        static_cast<double>(petal) * 2.0 * std::numbers::pi / static_cast<double>(daisyPetals);
    const float x = ring.centreX() + petalDistance * static_cast<float>(std::sin(angle));
    const float y = ring.centreY() - petalDistance * static_cast<float>(std::cos(angle));
    return Rect{x - petalRadius, y - petalRadius, 2.0f * petalRadius, 2.0f * petalRadius};
}

Rect KeyboardLayout::faceBounds(DaisyFace face) const noexcept {
    const auto [stepX, stepY] = faceStep(face);
    const float x = diamond.centreX() + stepX * faceDistance;
    const float y = diamond.centreY() + stepY * faceDistance;
    return Rect{x - faceRadius, y - faceRadius, 2.0f * faceRadius, 2.0f * faceRadius};
}

void OnScreenKeyboard::configure(std::string prompt, EntryKind kind) noexcept {
    prompt_ = std::move(prompt);
    kind_ = kind;
    shifted_ = false;
}

void OnScreenKeyboard::open(std::string text) {
    style_ = chosen_;
    text_ = std::move(text);
    if (secret()) {
        // No growth while it is typed, so no stale copy is left behind.
        text_.reserve(passwordMostLength + 1);
    }
    cursor_ = text_.size();
    key_ = 0;
    anchor_ = centreOf(gridKeys()[0]);
    set_ = DaisySet::Letters;
    stick_ = {};
    pointed_ = false;
    pointerPetal_.reset();
    open_ = true;
}

void OnScreenKeyboard::close() noexcept {
    open_ = false;
    stick_ = {};
    pointed_ = false;
    pointerPetal_.reset();
    if (secret()) {
        host::wipe(text_);
        cursor_ = 0;
    }
}

bool OnScreenKeyboard::type(std::string_view text) {
    std::size_t most = searchMostLength;
    if (kind_ == EntryKind::Path) {
        most = pathMostLength;
    } else if (kind_ == EntryKind::Password) {
        most = passwordMostLength;
    }
    if (text.empty() || text_.size() + text.size() > most) {
        return false;
    }
    text_.insert(cursor_, text);
    cursor_ += text.size();
    return true;
}

bool OnScreenKeyboard::typeSpace() {
    if (kind_ == EntryKind::Search && (cursor_ == 0 || text_[cursor_ - 1] == ' ' ||
                                       (cursor_ < text_.size() && text_[cursor_] == ' '))) {
        return false;
    }
    return type(" ");
}

bool OnScreenKeyboard::backspace() {
    const std::size_t bytes = characterBytesBefore(text_, cursor_);
    text_.erase(cursor_ - bytes, bytes);
    cursor_ -= bytes;
    return bytes > 0;
}

bool OnScreenKeyboard::clear() {
    const bool held = !text_.empty();
    if (secret()) {
        host::wipe(text_);
    } else {
        text_.clear();
    }
    cursor_ = 0;
    return held;
}

bool OnScreenKeyboard::moveCursor(int step) noexcept {
    const std::size_t bytes =
        step < 0 ? characterBytesBefore(text_, cursor_) : characterBytesAt(text_, cursor_);
    if (bytes == 0) {
        return false;
    }
    cursor_ = step < 0 ? cursor_ - bytes : cursor_ + bytes;
    return true;
}

bool OnScreenKeyboard::focusKey(std::size_t index) {
    if (style_ == input::KeyboardStyle::DaisyWheel) {
        if (index >= daisyKeys) {
            return false;
        }
        const bool changed = index != key_ || !pointed_;
        key_ = index;
        pointed_ = true;
        if (index < daisyPetals) {
            pointerPetal_ = index;
        }
        return changed;
    }
    if (index >= gridKeys().size()) {
        return false;
    }
    const bool changed = index != key_;
    key_ = index;
    anchor_ = centreOf(gridKeys()[key_]);
    return changed;
}

bool OnScreenKeyboard::onTopRow() const noexcept {
    return style_ == input::KeyboardStyle::Regular && gridKeys()[key_].row == 0;
}

bool OnScreenKeyboard::move(Direction direction) {
    if (style_ != input::KeyboardStyle::Regular) {
        return false;
    }
    const std::span<const GridKey> keys = gridKeys();
    const GridKey& here = keys[key_];
    if (direction == Direction::Left || direction == Direction::Right) {
        const int step = direction == Direction::Left ? -1 : 1;
        const auto next = static_cast<long>(key_) + step;
        if (next < 0 || next >= static_cast<long>(keys.size()) ||
            keys[static_cast<std::size_t>(next)].row != here.row) {
            return false;
        }
        return focusKey(static_cast<std::size_t>(next));
    }
    const int row = here.row + (direction == Direction::Up ? -1 : 1);
    if (row < 0 || row >= gridKeyRows) {
        return false;
    }
    std::size_t best = key_;
    float nearest = 1e9f;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const float distance = std::abs(centreOf(keys[i]) - anchor_);
        if (keys[i].row == row && distance < nearest) {
            nearest = distance;
            best = i;
        }
    }
    const float keep = anchor_;
    const bool moved = focusKey(best);
    anchor_ = keep;
    return moved;
}

KeyPress OnScreenKeyboard::press() {
    if (style_ == input::KeyboardStyle::DaisyWheel) {
        if (!pointed_) {
            return {};
        }
        pointed_ = false;
        if (key_ < daisyPetals) {
            return {};
        }
        return KeyPress{.edited = typeFace(static_cast<DaisyFace>(key_ - daisyPetals))};
    }
    const GridKey& key = gridKeys()[key_];
    switch (key.kind) {
    case KeyKind::Character: {
        const bool capital = shifted_ && std::isalpha(static_cast<unsigned char>(key.character));
        const char character =
            capital ? static_cast<char>(std::toupper(key.character)) : key.character;
        return KeyPress{.edited = type(std::string_view{&character, 1})};
    }
    case KeyKind::Space:
        return KeyPress{.edited = typeSpace()};
    case KeyKind::Backspace:
        return KeyPress{.edited = backspace()};
    case KeyKind::Clear:
        return KeyPress{.edited = clear()};
    case KeyKind::Done:
        return KeyPress{.done = true};
    }
    return {};
}

void OnScreenKeyboard::cycleSet(int step) noexcept {
    const auto count = static_cast<int>(daisySets);
    set_ = static_cast<DaisySet>((static_cast<int>(set_) + step % count + count) % count);
}

bool OnScreenKeyboard::stick(Direction direction, bool held) noexcept {
    const std::optional<std::size_t> before = petal();
    stick_[stickSlot(direction)] = held;
    if (petalPointedAt(stick_[0], stick_[1], stick_[2], stick_[3])) {
        // The stick takes the petal from the pointer.
        pointed_ = false;
        pointerPetal_.reset();
    }
    return petal() != before;
}

std::optional<std::size_t> OnScreenKeyboard::petal() const noexcept {
    if (const std::optional<std::size_t> held =
            petalPointedAt(stick_[0], stick_[1], stick_[2], stick_[3])) {
        return held;
    }
    return pointerPetal_;
}

bool OnScreenKeyboard::typeFace(DaisyFace face) {
    const std::optional<std::size_t> picked = petal();
    if (!picked) {
        return false;
    }
    pointed_ = false;
    const char character = daisyCharacter(set_, *picked, face);
    return type(std::string_view{&character, 1});
}

} // namespace opensu::ui
