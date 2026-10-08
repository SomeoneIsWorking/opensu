#include "dock_metrics.hpp"

#include <algorithm>
#include <numeric>

namespace iideck::ui {
namespace {

/// iiSU `jj2.q0`: the height of a two-row button prompt panel at a scale.
float promptHeight(float f) noexcept {
    const float scale = std::clamp(f, 0.65f, 1.1f);
    return 2.0f * std::clamp(22.0f * scale, 15.0f, 24.0f) +
           2.0f * std::clamp(8.0f * scale, 5.0f, 7.0f) + std::clamp(2.0f * scale, 1.0f, 3.0f);
}

} // namespace

DockMetrics::DockMetrics(float screenWidthDp, float screenHeightDp,
                         const std::vector<float>& widths)
    : itemWidths{widths} {
    // gh3.i1/j1 with fill = compact = false. f0 is jj2.f0.
    const float f0 = std::clamp(
        std::min(std::min(screenWidthDp / 853.0f, 1.0f), std::min(screenHeightDp / 480.0f, 1.0f)) *
            0.92f,
        0.68f, 1.0f);
    const float available = std::max(240.0f, screenWidthDp);
    const float prompt = promptHeight(std::clamp(0.94f * f0, 0.68f, 1.0f));
    const float sizeScale = std::min(std::clamp(1.18f * f0, 0.82f, 1.04f),
                                     std::clamp(available / 390.0f, 0.72f, 1.06f));

    const auto fit = [&](float scale) {
        height =
            std::min(std::clamp(68.0f * std::clamp(scale, 0.54f, 1.08f), 36.0f, 60.0f), prompt);
        iconSize = std::clamp(0.86f * height, 30.0f, 58.0f);
        spacing = std::clamp(0.03f * height, 1.0f, 4.0f);
        horizontalPadding = std::clamp(0.12f * height, 5.0f, 14.0f);
        verticalPadding = std::clamp(0.06f * height, 3.0f, 8.0f);
        iconPadding = std::clamp(0.1f * iconSize, 4.0f, 6.0f);
        const float items = std::accumulate(itemWidths.begin(), itemWidths.end(), 0.0f) * iconSize;
        const auto gaps = itemWidths.empty() ? 0.0f : static_cast<float>(itemWidths.size() - 1);
        width = items + gaps * spacing + 2.0f * horizontalPadding;
    };
    fit(sizeScale);
    if (width > available) {
        fit(sizeScale * std::clamp(available / width, 0.72f, 1.0f));
    }
    // jj2.o0(clamp(H / 64, 0.65, 1.1)) = clamp(2.4 f, 1.6, 2.8).
    borderWidth = std::clamp(2.4f * std::clamp(height / 64.0f, 0.65f, 1.1f), 1.6f, 2.8f);
    badgeSize = std::clamp(0.42f * height, 20.0f, 30.0f);
    badgeOutset = std::clamp(0.08f * height, 2.0f, 8.0f);
    badgeRise = std::clamp(0.11f * height, 4.0f, 8.0f);
    // The widths are factors until the icon size is known.
    for (float& item : itemWidths) {
        item = std::max(iconSize, item * iconSize);
    }
}

DockLayout layoutDock(const DockMetrics& metrics, float width, float height, float dp) {
    DockLayout out;
    out.bar = Rect{(width - metrics.width * dp) * 0.5f,
                   height - (DockMetrics::bottomGap + metrics.height) * dp, metrics.width * dp,
                   metrics.height * dp};
    float x = out.bar.x + metrics.horizontalPadding * dp;
    const float iconTop = out.bar.centreY() - metrics.iconSize * dp * 0.5f;
    for (const float itemWidth : metrics.itemWidths) {
        const Rect item{x, iconTop, itemWidth * dp, metrics.iconSize * dp};
        out.items.push_back(item);
        const float pad = metrics.iconPadding * dp;
        out.icons.push_back(Rect{item.x + pad + DockMetrics::iconShiftX * dp, item.y + pad,
                                 item.width - 2.0f * pad, item.height - 2.0f * pad});
        x += item.width + metrics.spacing * dp;
    }
    const float badge = metrics.badgeSize * dp;
    const float top = out.bar.y - metrics.badgeRise * dp;
    out.leftBadge = Rect{out.bar.x - metrics.badgeOutset * dp, top, badge, badge};
    out.rightBadge = Rect{out.bar.right() + metrics.badgeOutset * dp - badge, top, badge, badge};
    return out;
}

} // namespace iideck::ui
