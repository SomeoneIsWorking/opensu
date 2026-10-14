#include "volume_osd_painter.hpp"

#include <string>

#include "round_shape.hpp"
#include "row_controls.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr float widthDp = 300.0f;
constexpr float heightDp = 46.0f;
constexpr float gapBelowBarDp = 10.0f;
constexpr float paddingDp = 16.0f;
constexpr float iconDp = 22.0f;
constexpr float barDp = 5.0f;
constexpr float percentSp = 16.0f;
constexpr float percentRoomDp = 58.0f;

/// A speaker in a `size` box at (x, y): the cone and, for sound, up to two waves; a slash for mute.
void paintSpeaker(Canvas& canvas, const Rect& box, int waves, bool muted, Colour ink, float dp) {
    const float x = box.x;
    const float y = box.y;
    const float k = box.width / 24.0f;
    canvas.fillRect(gpu2d::Rect{x + 2.0f * k, y + 8.5f * k, 4.0f * k, 7.0f * k}, ink);
    canvas.triangle(Point{x + 6.0f * k, y + 8.5f * k}, Point{x + 6.0f * k, y + 15.5f * k},
                    Point{x + 12.5f * k, y + 20.0f * k}, ink);
    canvas.triangle(Point{x + 6.0f * k, y + 8.5f * k}, Point{x + 12.5f * k, y + 20.0f * k},
                    Point{x + 12.5f * k, y + 4.0f * k}, ink);
    const Point centre{x + 11.0f * k, y + 12.0f * k};
    for (int wave = 0; wave < waves; ++wave) {
        const float radius = (7.5f + 4.0f * static_cast<float>(wave)) * k;
        canvas.ring(gpu2d::Ring{.centre = centre,
                                .inner = radius,
                                .outer = radius + 1.8f * dp,
                                .startDegrees = -38.0f,
                                .endDegrees = 38.0f},
                    ink);
    }
    if (muted) {
        canvas.line(Point{x + 15.0f * k, y + 8.0f * k}, Point{x + 22.0f * k, y + 16.0f * k},
                    2.0f * dp, ink);
        canvas.line(Point{x + 22.0f * k, y + 8.0f * k}, Point{x + 15.0f * k, y + 16.0f * k},
                    2.0f * dp, ink);
    }
}

} // namespace

Rect volumeOsdBody(Point size, float dp, float topInset) noexcept {
    const float width = widthDp * dp;
    const float height = heightDp * dp;
    return Rect{(size.x - width) * 0.5f, topInset + gapBelowBarDp * dp, width, height};
}

void VolumeOsdPainter::paint(Canvas& canvas, const VolumeLevel& level, Point size, float dp,
                             float topInset, const PanelLook& look) const {
    const Rect rest = volumeOsdBody(size, dp, topInset);
    const float width = rest.width * look.scale;
    const float height = rest.height * look.scale;
    const Rect pill{rest.centreX() - width * 0.5f, rest.centreY() - height * 0.5f, width, height};
    // Solid, so the page behind never shows through the bar and the number.
    const RoundRect shape{pill, pill.height * 0.5f};
    fillRoundRect(canvas, shape, [alpha = look.alpha](Point, float) {
        return withAlpha(Colour{0xF7, 0xF6, 0xFA, 255}, alpha);
    });
    fillBand(canvas, shape, shape.grown(-1.5f * dp), [alpha = look.alpha](Point, float) {
        return withAlpha(rowOff, alpha);
    });

    const float unit = dp * look.scale;
    const Colour ink = withAlpha(rowInk, look.alpha);
    const float icon = iconDp * unit;
    const float left = pill.x + paddingDp * unit;
    const int waves = level.muted || level.percent == 0 ? 0 : (level.percent < 50 ? 1 : 2);
    paintSpeaker(canvas, Rect{left, pill.centreY() - icon * 0.5f, icon, icon}, waves, level.muted,
                 ink, unit);

    const float trackLeft = left + icon + paddingDp * 0.75f * unit;
    const float trackRight = pill.right() - paddingDp * unit - percentRoomDp * unit;
    const float radius = barDp * unit * 0.5f;
    const Rect bar{trackLeft, pill.centreY() - radius, trackRight - trackLeft, radius * 2.0f};
    const Colour track = withAlpha(rowOff, look.alpha);
    fillRoundRect(canvas, RoundRect{bar, radius}, [track](Point, float) {
        return track;
    });
    const float shown = level.muted ? 0.0f : static_cast<float>(level.percent) / 100.0f;
    if (shown > 0.0f) {
        fillRoundRect(canvas, RoundRect{Rect{bar.x, bar.y, bar.width * shown, bar.height}, radius},
                      [ink](Point, float) {
                          return ink;
                      });
    }
    const TextStyle text{percentSp * unit};
    const std::string percent = level.muted ? "Muted" : std::to_string(level.percent) + "%";
    typeface_.drawCentred(canvas, percent, trackRight + paddingDp * 0.6f * unit, pill.centreY(),
                          text, ink);
}

} // namespace opensu::ui
