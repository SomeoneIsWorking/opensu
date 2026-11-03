#include "touch_button.hpp"

namespace opensu::ui {
namespace {

/// The icon's square and its stroke, in dp (xe4.i's 24 dp icon, Material's 2 dp stroke).
constexpr float iconDp = 24.0f;
constexpr float strokeDp = 2.5f;
constexpr Colour ink{0x4D, 0x46, 0x55, 255};
constexpr Colour light{0xFF, 0xFF, 0xFF, 90};

} // namespace

void TouchButtonPainter::paint(Canvas& canvas, TouchButton button, const Rect& body, float dp,
                               bool lit) const {
    glass_.paint(canvas, body);
    if (lit) {
        canvas.fillRoundRect(onCanvas(body), body.height * 0.5f, light);
    }
    const float half = iconDp * dp * 0.5f;
    const float stroke = strokeDp * dp;
    const Point centre{body.centreX(), body.centreY()};
    if (button == TouchButton::Back) {
        // Material's ArrowBack: a shaft and a head opening right.
        const Point tip{centre.x - half * 0.66f, centre.y};
        canvas.line(tip, Point{centre.x + half * 0.66f, centre.y}, stroke, ink);
        canvas.line(tip, Point{tip.x + half * 0.58f, centre.y - half * 0.58f}, stroke, ink);
        canvas.line(tip, Point{tip.x + half * 0.58f, centre.y + half * 0.58f}, stroke, ink);
        return;
    }
    // Material's Menu: three bars.
    for (const float row : {-0.5f, 0.0f, 0.5f}) {
        const float y = centre.y + row * half * 1.1f;
        canvas.line(Point{centre.x - half * 0.75f, y}, Point{centre.x + half * 0.75f, y}, stroke,
                    ink);
    }
}

} // namespace opensu::ui
