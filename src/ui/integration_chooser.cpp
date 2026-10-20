#include "integration_chooser.hpp"

#include <algorithm>
#include <utility>

namespace opensu::ui {
namespace {

constexpr float cardWidthDp = 600.0f;
constexpr float marginDp = 16.0f;
constexpr float paddingDp = 20.0f;
constexpr float gapDp = 10.0f;
constexpr float rowHeightDp = 44.0f;
constexpr float rowGapDp = 5.0f;
constexpr float doneHeightDp = 40.0f;
constexpr float doneWidthDp = 180.0f;
constexpr float radiusDp = 24.0f;

} // namespace

std::optional<std::size_t> IntegrationChooserLayout::itemAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].rect.contains(x, y)) {
            return i;
        }
    }
    if (done.contains(x, y)) {
        return rows.size();
    }
    return std::nullopt;
}

float chooserIntroWidth(float frameWidth, float dp) noexcept {
    const float card = std::min(cardWidthDp * dp, frameWidth - 2.0f * marginDp * dp);
    return card - 2.0f * paddingDp * dp;
}

IntegrationChooserLayout layoutIntegrationChooser(const PanelFrame& frame, float titleBox,
                                                  float introHeight, std::size_t rows) {
    const auto [width, height, dp] = frame;
    IntegrationChooserLayout layout;
    layout.padding = paddingDp * dp;
    layout.radius = radiusDp * dp;
    const float gap = gapDp * dp;
    const float rowGap = rowGapDp * dp;
    const float doneHeight = doneHeightDp * dp;
    const float cardWidth = std::min(cardWidthDp * dp, width - 2.0f * marginDp * dp);
    const float count = static_cast<float>(rows);
    const float fixed = 2.0f * layout.padding + titleBox + introHeight + 3.0f * gap + doneHeight +
                        (count > 0.0f ? (count - 1.0f) * rowGap : 0.0f);
    const float room = height - 2.0f * marginDp * dp - fixed;
    const float rowHeight = count > 0.0f ? std::min(rowHeightDp * dp, room / count) : 0.0f;
    const float cardHeight = fixed + count * rowHeight;
    layout.card =
        Rect{(width - cardWidth) * 0.5f, (height - cardHeight) * 0.5f, cardWidth, cardHeight};
    float y = layout.card.y + layout.padding;
    layout.titleY = y + titleBox * 0.5f;
    y += titleBox + gap;
    layout.intro =
        Rect{layout.card.x + layout.padding, y, cardWidth - 2.0f * layout.padding, introHeight};
    y += introHeight + gap;
    for (std::size_t i = 0; i < rows; ++i) {
        layout.rows.push_back(settingsRowBox(Rect{layout.intro.x, y, layout.intro.width, rowHeight},
                                             RowKind::Toggle, dp));
        y += rowHeight + rowGap;
    }
    y += gap - rowGap;
    const float doneWidth = doneWidthDp * dp;
    layout.done = Rect{layout.card.centreX() - doneWidth * 0.5f, y, doneWidth, doneHeight};
    return layout;
}

void IntegrationChooser::open(std::vector<IntegrationOption> options) {
    options_ = std::move(options);
    focus_ = 0;
    open_ = true;
}

bool IntegrationChooser::move(int delta) noexcept {
    const auto last = static_cast<long>(options_.size());
    const long next = std::clamp(static_cast<long>(focus_) + delta, 0L, last);
    return focusItem(static_cast<std::size_t>(next));
}

bool IntegrationChooser::focusItem(std::size_t index) noexcept {
    if (index > options_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

bool IntegrationChooser::toggleFocused() noexcept {
    if (onDone()) {
        return false;
    }
    options_[focus_].on = !options_[focus_].on;
    return true;
}

std::vector<SettingsRow> IntegrationChooser::rows() const {
    std::vector<SettingsRow> rows;
    rows.reserve(options_.size());
    for (const IntegrationOption& option : options_) {
        rows.push_back(makeToggle(option.id, option.label, option.note, option.on));
    }
    return rows;
}

} // namespace opensu::ui
