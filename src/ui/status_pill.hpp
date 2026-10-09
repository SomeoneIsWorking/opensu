// status_pill — iiSU's top-right status pill (a32.o): clock and battery, with the launchers' column
// where iiSU has the bell.
#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "device/battery.hpp"
#include "glass.hpp"
#include "home_layout.hpp"
#include "launcher_badges.hpp"
#include "top_bar_metrics.hpp"

namespace opensu::ui {

/// What one status pill shows.
struct StatusPillView {
    /// The pill body on the canvas, in pixels.
    Rect body;
    StatusPillMetrics metrics;
    /// Pixels per dp.
    float dp{1.0f};
    std::string_view clock;
    std::optional<device::BatteryStatus> battery;
    /// Pixels at the pill's left that the launchers hold; the clock's area is the rest.
    float launcherColumn{};
    /// The x of the line between the launchers and the clock; 0 with no launcher.
    float divider{};
    /// The launchers and where each visible one stands (`TopBarLayout::launchers`).
    std::span<const LauncherBadge> launchers;
    BadgeRow badges;
};

class StatusPillPainter {
  public:
    void paint(const StatusPillView& view);

  private:
    void paintLaunchers(const StatusPillView& view);
    void paintBattery(float x, float centreY, float size,
                      const device::BatteryStatus& battery) const;

    GlassPainter glass_;
    LauncherBadgePainter badges_;
};

} // namespace opensu::ui
