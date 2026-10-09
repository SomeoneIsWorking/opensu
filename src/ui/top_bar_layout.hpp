// top_bar_layout — where the top bar's status pill and the launcher badges inside it stand, in
// pixels. The Hud paints from it and hit-tests against it, so both read one geometry.
#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "home_layout.hpp"
#include "top_bar_metrics.hpp"

namespace opensu::ui {

/// What the layout depends on.
struct TopBarFrame {
    float width{};
    float height{};
    /// Pixels per dp.
    float dp{1.0f};
    bool clockHasLetters{false};
    /// The text row's measured width in pixels (`StatusPillPainter::rowWidth`).
    float contentWidth{};
    /// Launchers shown, which take the column iiSU's bell had.
    std::size_t launchers{0};
};

struct TopBarLayout {
    /// The pill, launcher column included.
    Rect status;
    StatusPillMetrics pill;
    /// The width from the pill's left edge to where the text row's padding starts.
    float launcherColumn{};
    /// One square per launcher, in order, each the badge circle's bounds.
    std::vector<Rect> launchers;
    /// The x of the line between the launchers and the clock, or 0 with no launcher.
    float divider{};
    /// The row's top, below the row padding.
    float top{};
    /// How far a launcher's hit extends past its circle.
    float hitSlop{};

    /// The launcher under the point, by its place in `launchers`.
    [[nodiscard]] std::optional<std::size_t> launcherAt(float x, float y) const noexcept;
};

[[nodiscard]] TopBarLayout layoutTopBar(const TopBarMetrics& metrics, const TopBarFrame& frame);

} // namespace opensu::ui
