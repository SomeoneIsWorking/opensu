#include "stream_meter_painter.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace opensu::ui {
namespace {

constexpr float marginDp = 12.0f;
constexpr float heightDp = 28.0f;
constexpr float paddingDp = 12.0f;
constexpr float dotDp = 8.0f;
constexpr float dotGapDp = 8.0f;
constexpr float textSp = 14.0f;
constexpr Colour ground{0x10, 0x10, 0x14, 0xB8};
constexpr Colour ink{0xF2, 0xF2, 0xF5, 0xFF};
constexpr Colour good{0x4C, 0xD9, 0x64, 0xFF};
constexpr Colour fair{0xFF, 0xB3, 0x2E, 0xFF};
constexpr Colour poor{0xFF, 0x45, 0x3A, 0xFF};

Colour colourOf(stream::StreamQuality quality) noexcept {
    switch (quality) {
    case stream::StreamQuality::Good:
        return good;
    case stream::StreamQuality::Fair:
        return fair;
    case stream::StreamQuality::Poor:
        return poor;
    }
    return poor;
}

} // namespace

StreamMeterLine streamMeterLine(const stream::StreamReading& reading) {
    std::string text =
        std::format("{} ms \xC2\xB7 ping {} ms \xC2\xB7 {}.{} Mb/s \xC2\xB7 {} fps",
                    reading.latencyMs, reading.roundTripMs, reading.tenthsOfMegabits / 10,
                    reading.tenthsOfMegabits % 10, reading.framesPerSecond);
    if (reading.lostPerSecond > 0) {
        text += std::format(" \xC2\xB7 {} lost", reading.lostPerSecond);
    }
    return StreamMeterLine{.reading = reading, .text = std::move(text)};
}

void StreamMeterPainter::paint(Canvas& canvas, const StreamMeterLine& line, const gpu2d::Rect& area,
                               float dp) const {
    const float size = textSp * dp;
    const float padding = paddingDp * dp;
    const float dot = dotDp * dp;
    const float gap = dotGapDp * dp;
    const float width = padding + dot + gap + text_.measure(line.text, size) + padding;
    const float height = heightDp * dp;
    const float margin = marginDp * dp;
    const float right = area.right() - margin;
    const gpu2d::Rect pill{std::max(area.x + margin, right - width), area.y + margin, width,
                           height};
    canvas.fillRoundRect(pill, height * 0.5f, ground);
    const float centreY = pill.y + height * 0.5f;
    canvas.fillCircle(Point{pill.x + padding + dot * 0.5f, centreY}, dot * 0.5f,
                      colourOf(line.reading.quality));
    text_.drawCentred(canvas, line.text, Point{pill.x + padding + dot + gap, centreY}, size, ink);
}

} // namespace opensu::ui
