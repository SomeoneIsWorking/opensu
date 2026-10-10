#include "session_dialog.hpp"

#include <algorithm>
#include <utility>

namespace opensu::ui {
namespace {

constexpr float cardWidthDp = 460.0f;
constexpr float paddingDp = 28.0f;
constexpr float gapDp = 14.0f;
constexpr float buttonHeightDp = 44.0f;
constexpr float buttonGapDp = 12.0f;
constexpr float radiusDp = 24.0f;

} // namespace

std::optional<std::size_t> SessionDialogLayout::buttonAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (buttons[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

float dialogBodyWidth(float frameWidth, float dp) noexcept {
    const float card = std::min(cardWidthDp * dp, frameWidth - 2.0f * paddingDp * dp);
    return card - 2.0f * paddingDp * dp;
}

SessionDialogLayout layoutSessionDialog(const PanelFrame& frame,
                                        const SessionDialogMetrics& metrics, bool buttons) {
    const auto [width, height, dp] = frame;
    const auto [titleBox, lineBox, bodyHeight] = metrics;
    SessionDialogLayout layout;
    layout.padding = paddingDp * dp;
    layout.radius = radiusDp * dp;
    const float gap = gapDp * dp;
    const float buttonHeight = buttonHeightDp * dp;
    const float cardWidth = std::min(cardWidthDp * dp, width - 2.0f * layout.padding);
    const float foot = buttons ? buttonHeight : lineBox;
    const float cardHeight =
        layout.padding + titleBox + gap + bodyHeight + gap * 1.5f + foot + layout.padding;
    layout.card =
        Rect{(width - cardWidth) * 0.5f, (height - cardHeight) * 0.5f, cardWidth, cardHeight};
    float y = layout.card.y + layout.padding;
    layout.titleY = y + titleBox * 0.5f;
    y += titleBox + gap;
    layout.body =
        Rect{layout.card.x + layout.padding, y, cardWidth - 2.0f * layout.padding, bodyHeight};
    y += bodyHeight + gap * 1.5f;
    layout.progressY = y + lineBox * 0.5f;
    if (buttons) {
        const float each = (layout.body.width - buttonGapDp * dp) * 0.5f;
        layout.buttons[dialogAccept] = Rect{layout.body.x, y, each, buttonHeight};
        layout.buttons[dialogCancel] =
            Rect{layout.body.x + each + buttonGapDp * dp, y, each, buttonHeight};
    }
    return layout;
}

void SessionDialog::open(SessionDialogText text) {
    text_ = std::move(text);
    progress_.reset();
    focus_ = dialogAccept;
    open_ = true;
}

void SessionDialog::showProgress(std::string line) {
    progress_ = std::move(line);
}

bool SessionDialog::move(Direction direction) noexcept {
    const bool forward = direction == Direction::Right || direction == Direction::Down;
    return focusButton(forward ? dialogCancel : dialogAccept);
}

bool SessionDialog::focusButton(std::size_t index) noexcept {
    if (index > dialogCancel || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

} // namespace opensu::ui
