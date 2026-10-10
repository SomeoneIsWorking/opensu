#include "search_panel.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace opensu::ui {
namespace {

constexpr float panelWidthDp = 600.0f;
constexpr float panelPaddingDp = 14.0f;
constexpr float panelRadiusDp = 17.0f;
constexpr float fieldHeightDp = 40.0f;
constexpr float resultHeightDp = 28.0f;
constexpr float keyHeightDp = 34.0f;
constexpr float keyGapDp = 4.0f;
constexpr float sectionGapDp = 8.0f;
constexpr float hintHeightDp = 22.0f;

constexpr std::size_t keyCount = static_cast<std::size_t>(searchKeyColumns) * 4 + 5;

// What the character keys type, and what they are drawn as (letters as capitals).
constexpr std::string_view typed = "1234567890qwertyuiopasdfghjkl'zxcvbnm-.&";
constexpr std::string_view caps = "1234567890QWERTYUIOPASDFGHJKL'ZXCVBNM-.&";

constexpr std::array<SearchKey, keyCount> makeKeys() {
    std::array<SearchKey, keyCount> keys{};
    for (std::size_t i = 0; i < typed.size(); ++i) {
        keys[i] = SearchKey{KeyKind::Character,
                            typed[i],
                            caps.substr(i, 1),
                            static_cast<int>(i) / searchKeyColumns,
                            static_cast<int>(i) % searchKeyColumns,
                            1};
    }
    // The slash makes folder paths typeable; the space key gives up a column to it.
    keys[typed.size()] = SearchKey{KeyKind::Character, '/', "/", 4, 0, 1};
    keys[typed.size() + 1] = SearchKey{KeyKind::Space, ' ', "Space", 4, 1, 3};
    keys[typed.size() + 2] = SearchKey{KeyKind::Backspace, '\0', "Delete", 4, 4, 2};
    keys[typed.size() + 3] = SearchKey{KeyKind::Clear, '\0', "Clear", 4, 6, 2};
    keys[typed.size() + 4] = SearchKey{KeyKind::Done, '\0', "Done", 4, 8, 2};
    return keys;
}

constexpr std::array<SearchKey, keyCount> keyTable = makeKeys();

float centreOf(const SearchKey& key) noexcept {
    return static_cast<float>(key.column) + static_cast<float>(key.span - 1) * 0.5f;
}

/// Bytes the last UTF-8 character of `text` takes.
std::size_t lastCharacterBytes(const std::string& text) {
    std::size_t at = text.size();
    while (at > 0 && (static_cast<unsigned char>(text[at - 1]) & 0xC0U) == 0x80U) {
        --at;
    }
    return at > 0 ? text.size() - (at - 1) : 0;
}

SearchPress edited(bool changed) {
    SearchPress press;
    press.edited = changed;
    return press;
}

SearchPress closing() {
    SearchPress press;
    press.close = true;
    return press;
}

SearchPress opening(std::size_t result) {
    SearchPress press;
    press.open = result;
    return press;
}

} // namespace

std::span<const SearchKey> searchKeys() noexcept {
    return keyTable;
}

SearchLayout layoutSearch(const Rect& frame, float dp, bool lists) {
    SearchLayout layout;
    const float pad = panelPaddingDp * dp;
    const float gap = keyGapDp * dp;
    const float section = sectionGapDp * dp;
    const float width = std::min(panelWidthDp * dp, frame.width);
    const float listed =
        lists ? static_cast<float>(searchResultRows) * resultHeightDp * dp + section : 0.0f;
    const float height = 2.0f * pad + fieldHeightDp * dp + section + listed +
                         static_cast<float>(searchKeyRows) * keyHeightDp * dp +
                         static_cast<float>(searchKeyRows - 1) * gap + section + hintHeightDp * dp;
    layout.panel = Rect{frame.x + (frame.width - width) * 0.5f,
                        frame.y + (frame.height - height) * 0.5f, width, height};
    layout.panelRadius = panelRadiusDp * dp;
    const float inner = width - 2.0f * pad;
    float y = layout.panel.y + pad;
    layout.field = Rect{layout.panel.x + pad, y, inner, fieldHeightDp * dp};
    y += fieldHeightDp * dp + section;
    if (lists) {
        for (Rect& row : layout.results) {
            row = Rect{layout.panel.x + pad, y, inner, resultHeightDp * dp};
            y += resultHeightDp * dp;
        }
        y += section;
    }
    const float cell = (inner - static_cast<float>(searchKeyColumns - 1) * gap) /
                       static_cast<float>(searchKeyColumns);
    const float keysTop = y;
    for (const SearchKey& key : searchKeys()) {
        const auto span = static_cast<float>(key.span);
        layout.keys.push_back(
            Rect{layout.panel.x + pad + static_cast<float>(key.column) * (cell + gap),
                 keysTop + static_cast<float>(key.row) * (keyHeightDp * dp + gap),
                 span * cell + (span - 1.0f) * gap, keyHeightDp * dp});
    }
    y = keysTop + static_cast<float>(searchKeyRows) * keyHeightDp * dp +
        static_cast<float>(searchKeyRows - 1) * gap + section;
    layout.hints = Rect{layout.panel.x + pad, y, inner, hintHeightDp * dp};
    return layout;
}

std::optional<std::size_t> SearchLayout::keyAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> SearchLayout::resultAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (results[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

void SearchPanel::open(std::string text) {
    text_ = std::move(text);
    if (secret_) {
        // No growth while it is typed, so no stale copy is left behind.
        text_.reserve(mostLength_ + 1);
    }
    zone_ = SearchZone::Keys;
    key_ = 0;
    anchor_ = centreOf(searchKeys()[0]);
    open_ = true;
}

bool SearchPanel::type(std::string_view text) {
    if (text.empty() || text_.size() + text.size() > mostLength_) {
        return false;
    }
    text_ += text;
    return true;
}

bool SearchPanel::backspace() {
    const std::size_t bytes = lastCharacterBytes(text_);
    text_.resize(text_.size() - bytes);
    return bytes > 0;
}

bool SearchPanel::clear() {
    const bool held = !text_.empty();
    if (secret_) {
        host::wipe(text_);
    } else {
        text_.clear();
    }
    return held;
}

bool SearchPanel::focusKey(std::size_t index) {
    if (index >= searchKeys().size()) {
        return false;
    }
    const bool changed = index != key_ || zone_ != SearchZone::Keys;
    key_ = index;
    zone_ = SearchZone::Keys;
    anchor_ = centreOf(searchKeys()[key_]);
    return changed;
}

bool SearchPanel::focusResult(std::size_t index) {
    if (index >= results_.size()) {
        return false;
    }
    const bool changed = index != result_ || zone_ != SearchZone::Results;
    result_ = index;
    zone_ = SearchZone::Results;
    keepResultInView();
    return changed;
}

bool SearchPanel::switchZone() {
    if (zone_ == SearchZone::Results) {
        zone_ = SearchZone::Keys;
        return true;
    }
    return focusResult(std::min(result_, results_.empty() ? std::size_t{0} : results_.size() - 1));
}

bool SearchPanel::move(Direction direction) {
    if (zone_ == SearchZone::Results) {
        if (direction == Direction::Up && result_ > 0) {
            return focusResult(result_ - 1);
        }
        if (direction == Direction::Down) {
            if (result_ + 1 < results_.size()) {
                return focusResult(result_ + 1);
            }
            zone_ = SearchZone::Keys;
            return true;
        }
        return false;
    }
    const std::span<const SearchKey> keys = searchKeys();
    const SearchKey& here = keys[key_];
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
    if (row < 0) {
        if (results_.empty()) {
            return false;
        }
        return focusResult(std::min(result_, results_.size() - 1));
    }
    if (row >= searchKeyRows) {
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

SearchPress SearchPanel::press() {
    if (zone_ == SearchZone::Results) {
        return results_.empty() ? SearchPress{} : opening(result_);
    }
    const SearchKey& key = searchKeys()[key_];
    switch (key.kind) {
    case KeyKind::Character: {
        const bool capital = shifted_ && std::isalpha(static_cast<unsigned char>(key.character));
        const char typedCharacter =
            capital ? static_cast<char>(std::toupper(key.character)) : key.character;
        return edited(type(std::string_view{&typedCharacter, 1}));
    }
    case KeyKind::Space:
        return edited(!text_.empty() && text_.back() != ' ' && type(" "));
    case KeyKind::Backspace:
        return edited(backspace());
    case KeyKind::Clear:
        return edited(clear());
    case KeyKind::Done:
        return closing();
    }
    return {};
}

void SearchPanel::setResults(std::vector<SearchResult> results) {
    results_ = std::move(results);
    result_ = std::min(result_, results_.empty() ? std::size_t{0} : results_.size() - 1);
    if (results_.empty() && zone_ == SearchZone::Results) {
        zone_ = SearchZone::Keys;
    }
    keepResultInView();
}

void SearchPanel::keepResultInView() noexcept {
    if (result_ < first_) {
        first_ = result_;
    } else if (result_ >= first_ + searchResultRows) {
        first_ = result_ + 1 - searchResultRows;
    }
    first_ =
        std::min(first_, results_.size() > searchResultRows ? results_.size() - searchResultRows
                                                            : std::size_t{0});
}

} // namespace opensu::ui
