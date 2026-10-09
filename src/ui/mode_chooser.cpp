#include "mode_chooser.hpp"

#include <algorithm>

namespace opensu::ui {

namespace {

constexpr float panelWidthDp = 523.6f;
constexpr float sidePaddingDp = 16.0f;
constexpr float topPaddingDp = 21.0f;
constexpr float cardGapDp = 10.7f;
constexpr float panelRadiusDp = 17.0f;
constexpr float cardRadiusDp = 12.0f;
constexpr float pinRowHeightDp = 48.0f;
constexpr float switchWidthDp = 40.0f;
constexpr float switchHeightDp = 22.0f;
constexpr float switchInsetDp = 16.0f;

} // namespace

ChooserLayout layoutChooser(const Rect& frame, float dp) noexcept {
    const float width = frame.width;
    const float height = frame.height;
    const auto count = static_cast<float>(library::allLibraryModes.size());
    const float side = sidePaddingDp * dp;
    const float gap = cardGapDp * dp;
    const float panelWidth = std::min(panelWidthDp * dp, width);
    const float card = (panelWidth - 2.0f * side - (count - 1.0f) * gap) / count;
    const float panelHeight =
        topPaddingDp * dp + card + gap + pinRowHeightDp * dp + sidePaddingDp * dp;
    ChooserLayout layout;
    layout.panel =
        Rect{(width - panelWidth) * 0.5f, (height - panelHeight) * 0.5f, panelWidth, panelHeight};
    for (std::size_t i = 0; i < layout.cards.size(); ++i) {
        layout.cards[i] = Rect{layout.panel.x + side + static_cast<float>(i) * (card + gap),
                               layout.panel.y + topPaddingDp * dp, card, card};
    }
    layout.pinRow = Rect{layout.panel.x + side, layout.cards[0].bottom() + gap,
                         panelWidth - 2.0f * side, pinRowHeightDp * dp};
    layout.pinSwitch = Rect{layout.pinRow.right() - switchInsetDp * dp - switchWidthDp * dp,
                            layout.pinRow.centreY() - switchHeightDp * dp * 0.5f,
                            switchWidthDp * dp, switchHeightDp * dp};
    layout.panelRadius = panelRadiusDp * dp;
    layout.cardRadius = cardRadiusDp * dp;
    return layout;
}

std::optional<library::LibraryMode> ChooserLayout::cardAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < cards.size(); ++i) {
        if (cards[i].contains(x, y)) {
            return library::allLibraryModes[i];
        }
    }
    return std::nullopt;
}

void ModeChooser::open(library::LibraryMode current, bool pinned) noexcept {
    focused_ = current;
    pinned_ = pinned;
    row_ = ChooserRow::Cards;
    open_ = true;
}

void ModeChooser::close() noexcept {
    open_ = false;
}

bool ModeChooser::move(int delta) noexcept {
    const int last = static_cast<int>(library::allLibraryModes.size()) - 1;
    const int next = std::clamp(static_cast<int>(focused_) + delta, 0, last);
    const bool moved = next != static_cast<int>(focused_);
    focused_ = library::allLibraryModes[static_cast<std::size_t>(next)];
    return moved;
}

bool ModeChooser::moveRow(int delta) noexcept {
    const ChooserRow next = delta > 0 ? ChooserRow::Pin : ChooserRow::Cards;
    const bool moved = next != row_;
    row_ = next;
    return moved;
}

bool ModeChooser::focusPin() noexcept {
    return moveRow(1);
}

bool ModeChooser::togglePin() noexcept {
    pinned_ = !pinned_;
    return pinned_;
}

bool ModeChooser::focus(library::LibraryMode mode) noexcept {
    const bool changed = mode != focused_ || row_ != ChooserRow::Cards;
    focused_ = mode;
    row_ = ChooserRow::Cards;
    return changed;
}

} // namespace opensu::ui
