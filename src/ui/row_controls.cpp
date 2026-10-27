#include "row_controls.hpp"

#include <string>

#include "hud.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr float valueSp = 16.0f;
constexpr float chevronHalfHeightDp = 5.5f;
constexpr float chevronStrokeDp = 2.0f;
constexpr float sliderBarDp = 4.0f;
constexpr float sliderThumbDp = 8.0f;
constexpr float sliderNumberGapDp = 14.0f;

} // namespace

void paintSwitch(Canvas& canvas, const Rect& track, bool on, float dp) {
    const float radius = track.height * 0.5f;
    fillRoundRect(canvas, RoundRect{track, radius}, [on](Point, float) {
        return on ? rowInk : rowOff;
    });
    const float thumb = radius - 3.0f * dp;
    const float thumbX = on ? track.right() - radius : track.x + radius;
    canvas.fillCircle(Point{thumbX, track.centreY()}, thumb, white);
}

void paintSlider(Canvas& canvas, Typeface& typeface, const Rect& track, int level, int low,
                 int high, float dp) {
    const float radius = sliderBarDp * dp * 0.5f;
    const Rect bar{track.x, track.centreY() - radius, track.width, radius * 2.0f};
    fillRoundRect(canvas, RoundRect{bar, radius}, [](Point, float) {
        return rowOff;
    });
    const float along = static_cast<float>(level - low) / static_cast<float>(high - low);
    const float thumbX = track.x + along * track.width;
    fillRoundRect(canvas, RoundRect{Rect{bar.x, bar.y, thumbX - bar.x, bar.height}, radius},
                  [](Point, float) {
                      return rowInk;
                  });
    canvas.fillCircle(Point{thumbX, track.centreY()}, sliderThumbDp * dp, rowInk);
    canvas.fillCircle(Point{thumbX, track.centreY()}, sliderThumbDp * dp * 0.4f, white);
    const TextStyle number{valueSp * dp};
    typeface.drawCentred(canvas, std::to_string(level), track.right() + sliderNumberGapDp * dp,
                         track.centreY(), number, palette::ink);
}

void paintChevron(Canvas& canvas, float x, float centreY, float dp) {
    const float half = chevronHalfHeightDp * dp;
    const float depth = chevronWidthDp * dp * 0.5f;
    const float thickness = chevronStrokeDp * dp;
    canvas.line(Point{x - depth, centreY - half}, Point{x, centreY}, thickness, palette::inkSoft);
    canvas.line(Point{x, centreY}, Point{x - depth, centreY + half}, thickness, palette::inkSoft);
}

} // namespace opensu::ui
