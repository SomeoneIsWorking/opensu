// dock_metrics — where iiSU's primary navigation bar sits and how big it is, in dp.
//
// The plain bar of `gh3.K` (navigation.md §1.2): a capsule at the bottom centre holding one square
// icon per section, with the LB and RB badges poking out of its top corners. Pure arithmetic from
// the screen's size in dp.
#pragma once

#include <vector>

#include "home_layout.hpp"

namespace iideck::ui {

/// How wide a section's item is, as a multiple of the icon size (iiSU: ROMs is `max(I, 1.235 I)`,
/// the others `I`).
inline constexpr float plainItem = 1.0f;
inline constexpr float wideItem = 1.235f;

/// The bar's sizes in dp, from the screen's.
struct DockMetrics {
    /// `itemWidths` is each item's width as a multiple of the icon size, in dock order.
    DockMetrics(float screenWidthDp, float screenHeightDp, const std::vector<float>& itemWidths);

    float height{};
    float iconSize{};
    float spacing{};
    float horizontalPadding{};
    float verticalPadding{};
    /// Inside an item, on every side.
    float iconPadding{};
    float width{};
    /// The glass border (iiSU `jj2.o0`).
    float borderWidth{};
    float badgeSize{};
    /// How far a badge sits outside its corner, and above the bar's top.
    float badgeOutset{};
    float badgeRise{};
    std::vector<float> itemWidths;

    /// The gap below the bar and above it (iiSU `iq3`: padding bottom 6, top 8).
    static constexpr float bottomGap = 6.0f;
    static constexpr float topGap = 8.0f;
    /// A drawn icon sits this far left of its item's centre (iiSU `gt3`: offset x -2).
    static constexpr float iconShiftX = -2.0f;
};

/// Where the bar and its parts sit on a canvas, in pixels.
struct DockLayout {
    Rect bar;
    /// Each item's box, full bar height.
    std::vector<Rect> items;
    /// Each item's icon box: the item inset by the icon padding and shifted.
    std::vector<Rect> icons;
    Rect leftBadge;
    Rect rightBadge;
};

/// The layout of a bar of `metrics` on a `width` x `height` pixel canvas of `dp` pixels per dp.
[[nodiscard]] DockLayout layoutDock(const DockMetrics& metrics, float width, float height,
                                    float dp);

} // namespace iideck::ui
