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

void paintSwitch(const Rect& track, bool on, float dp) {
    const float radius = track.height * 0.5f;
    fillRoundRect(RoundRect{track, radius}, [on](Vector2, float) {
        return on ? rowInk : rowOff;
    });
    const float thumb = radius - 3.0f * dp;
    const float thumbX = on ? track.right() - radius : track.x + radius;
    DrawCircleV(Vector2{thumbX, track.centreY()}, thumb, WHITE);
}

void paintSlider(const Rect& track, int level, int low, int high, float dp) {
    const float radius = sliderBarDp * dp * 0.5f;
    const Rect bar{track.x, track.centreY() - radius, track.width, radius * 2.0f};
    fillRoundRect(RoundRect{bar, radius}, [](Vector2, float) {
        return rowOff;
    });
    const float along = static_cast<float>(level - low) / static_cast<float>(high - low);
    const float thumbX = track.x + along * track.width;
    fillRoundRect(RoundRect{Rect{bar.x, bar.y, thumbX - bar.x, bar.height}, radius},
                  [](Vector2, float) {
                      return rowInk;
                  });
    DrawCircleV(Vector2{thumbX, track.centreY()}, sliderThumbDp * dp, rowInk);
    DrawCircleV(Vector2{thumbX, track.centreY()}, sliderThumbDp * dp * 0.4f, WHITE);
    const TextStyle number{valueSp * dp};
    type().drawCentred(std::to_string(level), track.right() + sliderNumberGapDp * dp,
                       track.centreY(), number, palette::ink);
}

void paintChevron(float x, float centreY, float dp) {
    const float half = chevronHalfHeightDp * dp;
    const float depth = chevronWidthDp * dp * 0.5f;
    const float thickness = chevronStrokeDp * dp;
    DrawLineEx(Vector2{x - depth, centreY - half}, Vector2{x, centreY}, thickness,
               static_cast<Color>(palette::inkSoft));
    DrawLineEx(Vector2{x, centreY}, Vector2{x - depth, centreY + half}, thickness,
               static_cast<Color>(palette::inkSoft));
}

} // namespace opensu::ui
