#include "status_pill.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "battery_icon.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

// iiSU res/drawable/battery_*_dark.png ink.
constexpr Colour iconInk{0x4D, 0x46, 0x55, 255};
// STOPGAP: text uses the icons' ink because a32.p's ja3 text colours are not in the spec.
constexpr Colour textInk = iconInk;
// iiSU res/values/strings.xml status_separator_pipe.
constexpr const char* separator = "|";

// iiSU res/drawable/battery_*.png geometry on its 512 px canvas.
constexpr float iconGrid = 512.0f;
constexpr Rect batteryBody{40.0f, 127.0f, 472.0f, 258.0f};
constexpr Rect batteryNub{0.0f, 195.0f, 40.0f, 122.0f};
constexpr float batteryStroke = 26.0f;
constexpr float batteryRadius = 40.0f;
constexpr std::array<float, 4> barLefts{80.0f, 181.0f, 282.0f, 383.0f};
constexpr float barWidth = 89.0f;
constexpr float barTop = 165.0f;
constexpr float barHeight = 182.0f;
constexpr Rect lowMark{255.0f, 180.0f, 36.0f, 153.0f};
constexpr std::array<Point, 6> bolt{Point{318.0f, 103.0f}, Point{203.0f, 270.0f},
                                    Point{283.0f, 270.0f}, Point{237.0f, 405.0f},
                                    Point{350.0f, 232.0f}, Point{270.0f, 232.0f}};

Colour unpack(std::uint32_t rgb) noexcept {
    return Colour{static_cast<std::uint8_t>((rgb >> 16) & 0xffu),
                  static_cast<std::uint8_t>((rgb >> 8) & 0xffu),
                  static_cast<std::uint8_t>(rgb & 0xffu), 255};
}

VertexColour flat(Colour colour) {
    return [colour](Point, float) {
        return colour;
    };
}

/// A rect on the icon grid placed in a `size` box at (x, y).
Rect onIcon(const Rect& grid, float x, float y, float size) noexcept {
    const float k = size / iconGrid;
    return Rect{x + grid.x * k, y + grid.y * k, grid.width * k, grid.height * k};
}

} // namespace

void StatusPillPainter::paint(Canvas& canvas, const StatusPillView& view) {
    const StatusPillMetrics& m = view.metrics;
    const float dp = view.dp;
    const Rect& body = view.body;
    glass_.paint(canvas, body);
    paintLaunchers(canvas, view);
    const float centreY = body.centreY();
    // The text row (iiSU a32.p): clock | NN% battery.
    const float size = m.fontSize * dp;
    const float spacing = m.textSpacing * dp;
    const std::string clock{view.clock};
    const float icon = m.batteryIcon * dp;
    std::string percent;
    if (view.battery) {
        percent = std::to_string(view.battery->percent) + "%";
    }
    float pen = body.x + view.launcherColumn + m.paddingHorizontal * dp;
    text_.drawCentred(canvas, clock, Point{pen, centreY}, size, textInk);
    pen += text_.measure(clock, size) + spacing;
    if (view.battery) {
        text_.drawCentred(canvas, separator, Point{pen, centreY}, size, textInk);
        pen += text_.measure(separator, size) + spacing;
        text_.drawCentred(canvas, percent, Point{pen, centreY}, size, textInk);
        pen += text_.measure(percent, size) + spacing;
        paintBattery(canvas, pen, centreY, icon, *view.battery);
    }
}

float StatusPillPainter::rowWidth(const StatusPillMetrics& metrics, float dp,
                                  std::string_view clock,
                                  const std::optional<device::BatteryStatus>& battery) const {
    const float size = metrics.fontSize * dp;
    float width = text_.measure(std::string{clock}, size);
    if (battery) {
        width += metrics.textSpacing * dp * 3.0f + text_.measure(separator, size) +
                 text_.measure(std::to_string(battery->percent) + "%", size) +
                 metrics.batteryIcon * dp;
    }
    return width;
}

void StatusPillPainter::paintLaunchers(Canvas& canvas, const StatusPillView& view) {
    if (view.badges.cells.empty()) {
        return;
    }
    badges_.paint(canvas, view.launchers, view.badges);
    const float height = view.body.height * 0.5f;
    canvas.line(Point{view.divider, view.body.centreY() - height * 0.5f},
                Point{view.divider, view.body.centreY() + height * 0.5f}, std::max(view.dp, 1.0f),
                Colour{iconInk.r, iconInk.g, iconInk.b, 70});
}

void StatusPillPainter::paintBattery(Canvas& canvas, float x, float centreY, float size,
                                     const device::BatteryStatus& battery) const {
    const float y = centreY - size * 0.5f;
    const float k = size / iconGrid;
    const RoundRect outer{onIcon(batteryBody, x, y, size), batteryRadius * k};
    fillBand(canvas, outer, outer.grown(-batteryStroke * k), flat(iconInk));
    fillRoundRect(canvas, RoundRect{onIcon(batteryNub, x, y, size), batteryStroke * k * 0.5f},
                  flat(iconInk));

    const int slot = BatteryIcon::slot(battery.percent, battery.charging);
    if (battery.charging && slot > 0) {
        const Colour colour = unpack(BatteryIcon::boltColours[static_cast<std::size_t>(slot)]);
        std::array<Point, bolt.size()> points{};
        for (std::size_t i = 0; i < bolt.size(); ++i) {
            points[i] = Point{x + bolt[i].x * k, y + bolt[i].y * k};
        }
        // The top half's triangle covers its notch's corner 5; two triangles make the bottom half.
        canvas.triangle(points[0], points[1], points[2], colour);
        canvas.triangle(points[5], points[2], points[4], colour);
        canvas.triangle(points[2], points[3], points[4], colour);
        return;
    }
    // Charged full shows four bars in the bolt's green; discharging fills from the right.
    const int count = battery.charging ? 4 : BatteryIcon::bars[static_cast<std::size_t>(slot)];
    const Colour fill = battery.charging ? unpack(BatteryIcon::boltColours[0]) : iconInk;
    for (int bar = 0; bar < count; ++bar) {
        const float left = barLefts[barLefts.size() - 1 - static_cast<std::size_t>(bar)];
        fillRoundRect(
            canvas,
            RoundRect{onIcon(Rect{left, barTop, barWidth, barHeight}, x, y, size), 12.0f * k},
            flat(fill));
    }
    if (count == 0) {
        fillRoundRect(canvas, RoundRect{onIcon(lowMark, x, y, size), 8.0f * k}, flat(iconInk));
    }
}

} // namespace opensu::ui
