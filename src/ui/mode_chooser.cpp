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

} // namespace

ChooserLayout layoutChooser(const Rect& frame, float dp) noexcept {
    const float width = frame.width;
    const float height = frame.height;
    const auto count = static_cast<float>(library::allLibraryModes.size());
    const float side = sidePaddingDp * dp;
    const float gap = cardGapDp * dp;
    const float panelWidth = std::min(panelWidthDp * dp, width);
    const float card = (panelWidth - 2.0f * side - (count - 1.0f) * gap) / count;
    const float panelHeight = card + 2.0f * topPaddingDp * dp;
    ChooserLayout layout;
    layout.panel =
        Rect{(width - panelWidth) * 0.5f, (height - panelHeight) * 0.5f, panelWidth, panelHeight};
    for (std::size_t i = 0; i < layout.cards.size(); ++i) {
        layout.cards[i] = Rect{layout.panel.x + side + static_cast<float>(i) * (card + gap),
                               layout.panel.y + topPaddingDp * dp, card, card};
    }
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

void ModeChooser::open(library::LibraryMode current) noexcept {
    focused_ = current;
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

bool ModeChooser::focus(library::LibraryMode mode) noexcept {
    const bool changed = mode != focused_;
    focused_ = mode;
    return changed;
}

} // namespace opensu::ui
