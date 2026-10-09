#include "status_pill.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "raylib.h"

#include "battery_icon.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// iiSU res/drawable/battery_*_dark.png ink.
constexpr Color iconInk{0x4D, 0x46, 0x55, 255};
// STOPGAP: text uses the icons' ink because a32.p's ja3 text colours are not in the spec.
constexpr Color textInk = iconInk;
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
constexpr std::array<Vector2, 6> bolt{Vector2{318.0f, 103.0f}, Vector2{203.0f, 270.0f},
                                      Vector2{283.0f, 270.0f}, Vector2{237.0f, 405.0f},
                                      Vector2{350.0f, 232.0f}, Vector2{270.0f, 232.0f}};

Color unpack(std::uint32_t rgb) noexcept {
    return Color{static_cast<unsigned char>((rgb >> 16) & 0xffu),
                 static_cast<unsigned char>((rgb >> 8) & 0xffu),
                 static_cast<unsigned char>(rgb & 0xffu), 255};
}

VertexColour flat(Color colour) {
    return [colour](Vector2, float) {
        return colour;
    };
}

/// A rect on the icon grid placed in a `size` box at (x, y).
Rect onIcon(const Rect& grid, float x, float y, float size) noexcept {
    const float k = size / iconGrid;
    return Rect{x + grid.x * k, y + grid.y * k, grid.width * k, grid.height * k};
}

} // namespace

void StatusPillPainter::paint(const StatusPillView& view) {
    const StatusPillMetrics& m = view.metrics;
    const float dp = view.dp;
    const Rect& body = view.body;
    glass_.paint(body);
    paintLaunchers(view);
    const float centreY = body.centreY();
    // The text row (iiSU a32.p): clock | NN% battery.
    const TextStyle text{m.fontSize * dp};
    const float spacing = m.textSpacing * dp;
    const std::string clock{view.clock};
    const float icon = m.batteryIcon * dp;
    std::string percent;
    if (view.battery) {
        percent = std::to_string(view.battery->percent) + "%";
    }
    float pen = body.x + view.launcherColumn + m.paddingHorizontal * dp;
    type().drawCentred(clock, pen, centreY, text, textInk);
    pen += type().measure(clock, text) + spacing;
    if (view.battery) {
        type().drawCentred(separator, pen, centreY, text, textInk);
        pen += type().measure(separator, text) + spacing;
        type().drawCentred(percent, pen, centreY, text, textInk);
        pen += type().measure(percent, text) + spacing;
        paintBattery(pen, centreY, icon, *view.battery);
    }
}

float StatusPillPainter::rowWidth(const StatusPillMetrics& metrics, float dp,
                                  std::string_view clock,
                                  const std::optional<device::BatteryStatus>& battery) {
    const TextStyle text{metrics.fontSize * dp};
    float width = type().measure(std::string{clock}, text);
    if (battery) {
        width += metrics.textSpacing * dp * 3.0f + type().measure(separator, text) +
                 type().measure(std::to_string(battery->percent) + "%", text) +
                 metrics.batteryIcon * dp;
    }
    return width;
}

void StatusPillPainter::paintLaunchers(const StatusPillView& view) {
    if (view.badges.cells.empty()) {
        return;
    }
    badges_.paint(view.launchers, view.badges);
    const float height = view.body.height * 0.5f;
    DrawLineEx(Vector2{view.divider, view.body.centreY() - height * 0.5f},
               Vector2{view.divider, view.body.centreY() + height * 0.5f}, std::max(view.dp, 1.0f),
               Color{iconInk.r, iconInk.g, iconInk.b, 70});
}

void StatusPillPainter::paintBattery(float x, float centreY, float size,
                                     const device::BatteryStatus& battery) const {
    const float y = centreY - size * 0.5f;
    const float k = size / iconGrid;
    const RoundRect outer{onIcon(batteryBody, x, y, size), batteryRadius * k};
    fillBand(outer, outer.grown(-batteryStroke * k), flat(iconInk));
    fillRoundRect(RoundRect{onIcon(batteryNub, x, y, size), batteryStroke * k * 0.5f},
                  flat(iconInk));

    const int slot = BatteryIcon::slot(battery.percent, battery.charging);
    if (battery.charging && slot > 0) {
        const Color colour = unpack(BatteryIcon::boltColours[static_cast<std::size_t>(slot)]);
        std::array<Vector2, bolt.size()> points{};
        for (std::size_t i = 0; i < bolt.size(); ++i) {
            points[i] = Vector2{x + bolt[i].x * k, y + bolt[i].y * k};
        }
        // Two triangles each side of the bolt's waist, wound for raylib's front face.
        DrawTriangle(points[0], points[1], points[2], colour);
        DrawTriangle(points[0], points[2], points[5], colour);
        DrawTriangle(points[5], points[2], points[4], colour);
        DrawTriangle(points[2], points[3], points[4], colour);
        return;
    }
    // Charged full shows four bars in the bolt's green; discharging fills from the right.
    const int count = battery.charging ? 4 : BatteryIcon::bars[static_cast<std::size_t>(slot)];
    const Color fill = battery.charging ? unpack(BatteryIcon::boltColours[0]) : iconInk;
    for (int bar = 0; bar < count; ++bar) {
        const float left = barLefts[barLefts.size() - 1 - static_cast<std::size_t>(bar)];
        fillRoundRect(
            RoundRect{onIcon(Rect{left, barTop, barWidth, barHeight}, x, y, size), 12.0f * k},
            flat(fill));
    }
    if (count == 0) {
        fillRoundRect(RoundRect{onIcon(lowMark, x, y, size), 8.0f * k}, flat(iconInk));
    }
}

} // namespace opensu::ui
