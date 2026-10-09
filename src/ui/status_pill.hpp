// status_pill — iiSU's top-right status pill (a32.o): bell, clock, battery, R2 glyph.
#pragma once

#include <optional>
#include <string_view>

#include "device/battery.hpp"
#include "glass.hpp"
#include "home_layout.hpp"
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
    /// Seconds into the bell's spinner while background tasks run; nothing when none do.
    std::optional<double> busySeconds;
};

class StatusPillPainter {
  public:
    void paint(const StatusPillView& view) const;

  private:
    void paintBell(float centreX, float centreY, float size) const;
    void paintBattery(float x, float centreY, float size,
                      const device::BatteryStatus& battery) const;
    void paintGlyph(float x, float y, float size) const;

    GlassPainter glass_;
};

} // namespace opensu::ui
