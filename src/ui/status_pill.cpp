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
// Between the outline and the fill.
constexpr float batteryGap = 14.0f;
// In pixels: the least a nonzero level shows.
constexpr float fillSliver = 1.0f;
constexpr std::array<Point, 6> bolt{Point{318.0f, 103.0f}, Point{203.0f, 270.0f},
                                    Point{283.0f, 270.0f}, Point{237.0f, 405.0f},
                                    Point{350.0f, 232.0f}, Point{270.0f, 232.0f}};
// An ink edge keeps the level colours readable on the glass; on the ink fill they read alone.
constexpr float boltEdgeWidth = 36.0f;

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

/// `rect` on whole pixels: the icon's outline, gap and fill stay even at a few pixels tall.
Rect snapped(const Rect& rect) noexcept {
    const float left = std::round(rect.x);
    const float top = std::round(rect.y);
    return Rect{left, top, std::round(rect.right()) - left, std::round(rect.bottom()) - top};
}

/// `grid` units in whole pixels, at least one.
float wholePixels(float grid, float k) noexcept {
    return std::max(std::round(grid * k), 1.0f);
}

/// The charging bolt at the icon's (x, y), `k` pixels per grid unit, inside its edge.
void paintBolt(Canvas& canvas, float x, float y, float k, Colour colour) {
    std::array<Point, bolt.size()> points{};
    for (std::size_t i = 0; i < bolt.size(); ++i) {
        points[i] = Point{x + bolt[i].x * k, y + bolt[i].y * k};
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        canvas.line(points[i], points[(i + 1) % points.size()], boltEdgeWidth * k, iconInk);
    }
    // The top half's triangle covers its notch's corner 5; two triangles make the bottom half.
    canvas.triangle(points[0], points[1], points[2], colour);
    canvas.triangle(points[5], points[2], points[4], colour);
    canvas.triangle(points[2], points[3], points[4], colour);
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
    const RoundRect outer{snapped(onIcon(batteryBody, x, y, size)), batteryRadius * k};
    const float stroke = wholePixels(batteryStroke, k);
    fillBand(canvas, outer, outer.grown(-stroke), flat(iconInk));
    fillRoundRect(canvas, RoundRect{snapped(onIcon(batteryNub, x, y, size)), stroke * 0.5f},
                  flat(iconInk));

    // The nub is on the left, so the level grows from the right.
    const Rect inner = outer.grown(-(stroke + wholePixels(batteryGap, k))).rect;
    const float width = BatteryIcon::fillWidth(battery.percent, inner.width, fillSliver);
    if (width > 0.0f) {
        canvas.fillRect(onCanvas(Rect{inner.right() - width, inner.y, width, inner.height}),
                        iconInk);
    }
    if (battery.charging) {
        paintBolt(canvas, x, y, k, unpack(BatteryIcon::boltColour(battery.percent)));
    }
}

} // namespace opensu::ui
