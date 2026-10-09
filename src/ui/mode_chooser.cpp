#include "mode_chooser.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {

namespace {

constexpr float panelWidthDp = 523.6f;
constexpr float sidePaddingDp = 16.0f;
constexpr float topPaddingDp = 21.0f;
constexpr float cardGapDp = 10.7f;
constexpr float panelRadiusDp = 17.0f;
constexpr float cardRadiusDp = 12.0f;
constexpr float rowHeightDp = 44.0f;
constexpr float rowGapDp = 4.0f;
constexpr float switchWidthDp = 40.0f;
constexpr float switchHeightDp = 22.0f;
constexpr float switchInsetDp = 16.0f;
constexpr float sliderWidthDp = 190.0f;
constexpr float sliderHeightDp = 22.0f;
constexpr float sliderInsetDp = 56.0f;
// The panel stays this share of the frame's height at most.
constexpr float panelMostHeight = 0.92f;

constexpr std::array<ChooserRow, 9> everyRow{
    ChooserRow::Cards,  ChooserRow::IconSize, ChooserRow::Pin,
    ChooserRow::Sort,   ChooserRow::Source,   ChooserRow::Installed,
    ChooserRow::Hidden, ChooserRow::Search,   ChooserRow::Settings};

bool hasSwitch(ChooserRow row) noexcept {
    return row == ChooserRow::Pin || row == ChooserRow::Installed || row == ChooserRow::Hidden;
}

} // namespace

ChooserLayout layoutChooser(const Rect& frame, float dp, const std::vector<ChooserRow>& rows,
                            ChooserRow focused) {
    ChooserLayout layout;
    const float side = sidePaddingDp * dp;
    const float gap = cardGapDp * dp;
    const float rowHeight = rowHeightDp * dp;
    const float rowGap = rowGapDp * dp;
    const auto count = static_cast<float>(library::allLibraryModes.size());
    const float panelWidth = std::min(panelWidthDp * dp, frame.width);
    const float card = (panelWidth - 2.0f * side - (count - 1.0f) * gap) / count;

    // Rows stacked from the top of the content, before scrolling.
    std::vector<ChooserRowBox> stacked;
    float y = topPaddingDp * dp;
    float focusedTop = 0.0f;
    float focusedBottom = 0.0f;
    for (const ChooserRow row : rows) {
        const float height = row == ChooserRow::Cards ? card : rowHeight;
        stacked.push_back(ChooserRowBox{row, Rect{side, y, panelWidth - 2.0f * side, height}, {}});
        if (row == focused) {
            focusedTop = y;
            focusedBottom = y + height;
        }
        y += height + (row == ChooserRow::Cards ? gap : rowGap);
    }
    const float contentHeight = y + side - rowGap;
    const float panelHeight = std::min(contentHeight, frame.height * panelMostHeight);
    layout.panel = Rect{frame.x + (frame.width - panelWidth) * 0.5f,
                        frame.y + (frame.height - panelHeight) * 0.5f, panelWidth, panelHeight};
    layout.content = layout.panel;
    layout.panelRadius = panelRadiusDp * dp;
    layout.cardRadius = cardRadiusDp * dp;

    // Scroll just far enough that the focused row is whole inside the panel.
    float scroll = 0.0f;
    if (focusedBottom + side > panelHeight) {
        scroll = focusedBottom + side - panelHeight;
    }
    scroll = std::min(scroll, std::max(contentHeight - panelHeight, 0.0f));
    if (focusedTop - scroll < topPaddingDp * dp * 0.5f) {
        scroll = std::max(focusedTop - topPaddingDp * dp * 0.5f, 0.0f);
    }
    for (ChooserRowBox& box : stacked) {
        box.rect.x += layout.panel.x;
        box.rect.y += layout.panel.y - scroll;
        if (box.row == ChooserRow::Cards) {
            for (std::size_t i = 0; i < layout.cards.size(); ++i) {
                layout.cards[i] =
                    Rect{box.rect.x + static_cast<float>(i) * (card + gap), box.rect.y, card, card};
            }
        } else if (hasSwitch(box.row)) {
            box.control = Rect{box.rect.right() - switchInsetDp * dp - switchWidthDp * dp,
                               box.rect.centreY() - switchHeightDp * dp * 0.5f, switchWidthDp * dp,
                               switchHeightDp * dp};
        } else if (box.row == ChooserRow::IconSize) {
            box.control = Rect{box.rect.right() - sliderInsetDp * dp - sliderWidthDp * dp,
                               box.rect.centreY() - sliderHeightDp * dp * 0.5f, sliderWidthDp * dp,
                               sliderHeightDp * dp};
        }
    }
    layout.rows = std::move(stacked);
    return layout;
}

std::optional<library::LibraryMode> ChooserLayout::cardAt(float x, float y) const noexcept {
    if (!content.contains(x, y)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < cards.size(); ++i) {
        if (cards[i].contains(x, y)) {
            return library::allLibraryModes[i];
        }
    }
    return std::nullopt;
}

std::optional<ChooserRow> ChooserLayout::rowAt(float x, float y) const noexcept {
    if (!content.contains(x, y)) {
        return std::nullopt;
    }
    for (const ChooserRowBox& box : rows) {
        if (box.row != ChooserRow::Cards && box.rect.contains(x, y)) {
            return box.row;
        }
    }
    return std::nullopt;
}

std::optional<int> ChooserLayout::iconSizeAt(float x, float y) const noexcept {
    const ChooserRowBox* slider = find(ChooserRow::IconSize);
    if (slider == nullptr || !content.contains(x, y) || !slider->rect.contains(x, y)) {
        return std::nullopt;
    }
    const Rect& track = slider->control;
    if (x < track.x || x >= track.right()) {
        return std::nullopt;
    }
    const float along = (x - track.x) / track.width;
    return clampIconLevel(minIconLevel +
                          static_cast<int>(std::lround(along * (maxIconLevel - minIconLevel))));
}

const ChooserRowBox* ChooserLayout::find(ChooserRow row) const noexcept {
    for (const ChooserRowBox& box : rows) {
        if (box.row == row) {
            return &box;
        }
    }
    return nullptr;
}

void ModeChooser::open(library::LibraryMode current, const ChooserValues& values, bool inLibrary) {
    focused_ = current;
    values_ = values;
    rows_.clear();
    for (const ChooserRow row : everyRow) {
        const bool layoutRow =
            row == ChooserRow::Cards || row == ChooserRow::IconSize || row == ChooserRow::Pin;
        if (inLibrary || !layoutRow) {
            rows_.push_back(row);
        }
    }
    row_ = rows_.front();
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
    const auto at = std::ranges::find(rows_, row_);
    const auto index = static_cast<int>(at - rows_.begin());
    const int next = std::clamp(index + delta, 0, static_cast<int>(rows_.size()) - 1);
    row_ = rows_[static_cast<std::size_t>(next)];
    return next != index;
}

bool ModeChooser::focusRow(ChooserRow row) noexcept {
    if (std::ranges::find(rows_, row) == rows_.end()) {
        return false;
    }
    const bool changed = row != row_;
    row_ = row;
    return changed;
}

bool ModeChooser::focus(library::LibraryMode mode) noexcept {
    const bool changed = mode != focused_ || row_ != ChooserRow::Cards;
    focused_ = mode;
    row_ = ChooserRow::Cards;
    return changed;
}

} // namespace opensu::ui
