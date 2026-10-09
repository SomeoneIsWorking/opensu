#include "status_pill.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "raylib.h"

#include "battery_icon.hpp"
#include "progress_spinner.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// iiSU res/drawable/battery_*_dark.png and bell_icon.png ink.
constexpr Color iconInk{0x4D, 0x46, 0x55, 255};
// iiSU a32.n: the spinner's stroke, and its colour, the theme's surfaceTint (light primary).
constexpr float spinnerStroke = 3.0f;
constexpr Color spinnerInk = iconInk;
// iiSU res/drawable/bell_icon.png: the bell over the ink disc.
constexpr Color bellFace{0xF5, 0xF5, 0xF5, 255};
// iiSU res/drawable/rt_button.png: white cap, #726B78 outline and letters.
constexpr Color glyphFace{0xFF, 0xFF, 0xFF, 255};
constexpr Color glyphInk{0x72, 0x6B, 0x78, 255};
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

void StatusPillPainter::paint(const StatusPillView& view) const {
    const StatusPillMetrics& m = view.metrics;
    const float dp = view.dp;
    const Rect& body = view.body;
    glass_.paint(body);
    const float centreY = body.centreY();

    // 1. The bell column (iiSU a32.n).
    const float bellColumn = m.bellColumn * dp;
    // STOPGAP: the bell is drawn at the progress ring's size because a32.n's icon size is not in
    // the spec.
    paintBell(body.x + bellColumn * 0.5f, centreY, m.ringSize * dp);
    if (view.busySeconds) {
        // STOPGAP: the spinner circles the bell, one stroke clear of it, because a32.n's ring sits
        // under a bell icon larger than itself (f4 22c, f9 50c) and the Box alignments that keep
        // it visible (wj0.o, wj0.k) are unresolved.
        const float stroke = spinnerStroke * dp;
        drawSpinner(Vector2{body.x + bellColumn * 0.5f, centreY}, m.ringSize * dp + stroke * 4.0f,
                    stroke, spinnerInk, *view.busySeconds);
    }

    // 2. The text row (iiSU a32.p): clock | NN% battery.
    const TextStyle text{m.fontSize * dp};
    const float spacing = m.textSpacing * dp;
    const std::string clock{view.clock};
    float width = type().measure(clock, text);
    std::string percent;
    const float icon = m.batteryIcon * dp;
    if (view.battery) {
        percent = std::to_string(view.battery->percent) + "%";
        width +=
            spacing * 3.0f + type().measure(separator, text) + type().measure(percent, text) + icon;
    }
    // STOPGAP: the row is centred in the space after the bell column because a32.o's
    // arrangement of the text row is not in the spec.
    const float free = body.right() - (body.x + bellColumn);
    float pen = body.x + bellColumn + std::max((free - width) * 0.5f, 0.0f);
    type().drawCentred(clock, pen, centreY, text, textInk);
    pen += type().measure(clock, text) + spacing;
    if (view.battery) {
        type().drawCentred(separator, pen, centreY, text, textInk);
        pen += type().measure(separator, text) + spacing;
        type().drawCentred(percent, pen, centreY, text, textInk);
        pen += type().measure(percent, text) + spacing;
        paintBattery(pen, centreY, icon, *view.battery);
    }

    // 3. The R2 glyph at the pill's top-left, offset (iiSU a32.o).
    paintGlyph(body.x + m.glyphOffsetX * dp, body.y + m.glyphOffsetY * dp, m.glyphSize * dp);
}

void StatusPillPainter::paintBell(float centreX, float centreY, float size) const {
    DrawCircleV(Vector2{centreX, centreY}, size * 0.5f, iconInk);
    // A bell: dome, flared rim and clapper, in the drawable's light face.
    const float u = size / 10.0f;
    DrawCircleV(Vector2{centreX, centreY - u * 0.6f}, u * 2.2f, bellFace);
    DrawRectangleV(Vector2{centreX - u * 2.2f, centreY - u * 0.6f}, Vector2{u * 4.4f, u * 2.2f},
                   bellFace);
    DrawRectangleRounded(Rectangle{centreX - u * 3.0f, centreY + u * 1.4f, u * 6.0f, u * 0.9f},
                         1.0f, 6, bellFace);
    DrawCircleV(Vector2{centreX, centreY + u * 2.9f}, u * 0.8f, bellFace);
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

void StatusPillPainter::paintGlyph(float x, float y, float size) const {
    // iiSU res/drawable/rt_button.png: a rounded cap with the trigger's name.
    const RoundRect cap{Rect{x, y, size, size * 0.86f}, size * 0.2f};
    fillRoundRect(cap, flat(glyphFace));
    fillBand(cap, cap.grown(-std::max(size * 0.06f, 1.0f)), flat(glyphInk));
    const TextStyle text{type().emForLineBox(size * 0.42f)};
    const float width = type().measure("R2", text);
    type().drawCentred("R2", cap.rect.centreX() - width * 0.5f, cap.rect.centreY(), text, glyphInk);
}

} // namespace opensu::ui
