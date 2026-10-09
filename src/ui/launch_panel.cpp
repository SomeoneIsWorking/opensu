#include "launch_panel.hpp"

#include <algorithm>
#include <utility>

namespace opensu::ui {

namespace {

constexpr float cardWidthDp = 420.0f;
constexpr float paddingDp = 28.0f;
constexpr float gapDp = 14.0f;
constexpr float meterDp = 8.0f;
constexpr float hintSpacingDp = 20.0f;

} // namespace

std::optional<std::size_t> PanelLayout::hintAt(float x, float y) const noexcept {
    const float slack = hintSpacing * 0.5f;
    for (std::size_t i = 0; i < hints.size(); ++i) {
        const Rect& hint = hints[i];
        const Rect reach{hint.x - slack, hint.y - glyph * 0.25f, hint.width + 2.0f * slack,
                         hint.height + glyph * 0.5f};
        if (reach.contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

PanelLayout layoutPanel(float width, float height, float dp, const PanelMetrics& metrics) {
    PanelLayout layout;
    layout.padding = paddingDp * dp;
    const float gap = gapDp * dp;
    layout.meter = meterDp * dp;
    layout.glyph = panelGlyphDp * dp;
    layout.hintSpacing = hintSpacingDp * dp;
    const float cardHeight = layout.padding + metrics.titleBox + gap + metrics.lineBox + gap +
                             layout.meter + gap * 1.5f + layout.glyph + layout.padding;
    const float cardWidth = std::min(cardWidthDp * dp, width - 2.0f * layout.padding);
    layout.card =
        Rect{(width - cardWidth) * 0.5f, (height - cardHeight) * 0.5f, cardWidth, cardHeight};
    layout.centreX = layout.card.centreX();
    float y = layout.card.y + layout.padding + metrics.titleBox * 0.5f;
    layout.titleY = y;
    y += metrics.titleBox * 0.5f + gap + metrics.lineBox * 0.5f;
    layout.lineY = y;
    y += metrics.lineBox * 0.5f + gap + layout.meter * 0.5f;
    layout.meterY = y;
    y += layout.meter * 0.5f + gap * 1.5f + layout.glyph * 0.5f;
    layout.hintY = y;

    float total = 0.0f;
    for (const float hint : metrics.hintWidths) {
        total += hint + layout.hintSpacing;
    }
    float x = layout.centreX - (total - layout.hintSpacing) * 0.5f;
    for (const float hint : metrics.hintWidths) {
        layout.hints.push_back(Rect{x, y - layout.glyph * 0.5f, hint, layout.glyph});
        x += hint + layout.hintSpacing;
    }
    return layout;
}

void LaunchPanel::open(std::string title) {
    title_ = std::move(title);
    line_ = "Starting";
    fraction_.reset();
    hints_ = {PanelHint{"B", "Cancel"}};
    busy_ = true;
    open_ = true;
}

void LaunchPanel::setHints(std::vector<PanelHint> hints) {
    hints_ = std::move(hints);
}

void LaunchPanel::update(std::string line, std::optional<double> fraction, bool busy) {
    line_ = std::move(line);
    busy_ = busy;
    fraction_.reset();
    if (fraction) {
        fraction_ = std::clamp(*fraction, 0.0, 1.0);
    }
}

void LaunchPanel::close() noexcept {
    open_ = false;
}

} // namespace opensu::ui
