#include "glass.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "round_shape.hpp"

namespace opensu::ui {
namespace {

// iiSU ea3.e, light: na3.e #F2F4F7 at 0.55 to na3.f #E6E9EE at 0.65, top-left to bottom-right.
constexpr Colour glassFrom{0xF2, 0xF4, 0xF7, 140};
constexpr Colour glassTo{0xE6, 0xE9, 0xEE, 166};

// iiSU ea3.e, dark: #1D1D1D at 0.65 to 0.75.
constexpr Colour darkFrom{0x1D, 0x1D, 0x1D, 166};
constexpr Colour darkTo{0x1D, 0x1D, 0x1D, 191};
// iiSU ea3.d: the tint over the backdrop, alpha 0.08, black in the dark theme and white in the
// light.
constexpr float tintAlpha = 0.08f;
// iiSU ea3.t(dark, 0.45): dark #B0B0B0 at 0.45 x 0.62, light white at 0.45.
constexpr Colour darkBorder{0xB0, 0xB0, 0xB0, 71};
constexpr Colour lightBorder{0xFF, 0xFF, 0xFF, 115};
// iiSU ea3.s: Shadow(radius 5, spread 0.5, offset (0, 2), black 14 %), in dp.
constexpr float shadowRadiusDp = 5.0f;
constexpr float shadowSpreadDp = 0.5f;
constexpr float shadowOffsetDp = 2.0f;
constexpr Colour shadowColour{0, 0, 0, 36};

} // namespace

void GlassPainter::paint(Canvas& canvas, const Rect& body) const {
    paint(canvas, body, body.height * 0.5f);
}

void GlassPainter::paint(Canvas& canvas, const Rect& body, float radius) const {
    // STOPGAP: only ea3.e's fill is drawn because ea3.n's blur, border and shadow layers are not
    // in the spec.
    const float span = std::max(body.width * body.width + body.height * body.height, 1.0f);
    fillRoundRect(canvas, RoundRect{body, radius}, [body, span](Point point, float) {
        // Compose resolves the infinite end offset to the size, so the axis is (w, h).
        const float t = ((point.x - body.x) * body.width + (point.y - body.y) * body.height) / span;
        return mix(glassFrom, glassTo, std::clamp(t, 0.0f, 1.0f));
    });
}

void GlassPainter::paintPill(Canvas& canvas, const Rect& body, const PillStyle& style) const {
    const RoundRect shape{body, body.height * 0.5f};
    const float dp = style.dp;
    const float alpha = style.alpha;
    const RoundRect shadow =
        RoundRect{Rect{body.x, body.y + shadowOffsetDp * dp, body.width, body.height}, shape.radius}
            .grown(shadowSpreadDp * dp);
    fillSoft(canvas, shadow, shadowRadiusDp * dp * 0.5f, [alpha](Point, float) {
        return withAlpha(shadowColour, alpha);
    });

    // The backdrop blur is drawn under this by the shell. The inner rim highlight is not
    // drawn: how iiSU clips it is not recovered (navigation.md §1.3).
    const Colour from = style.dark ? darkFrom : glassFrom;
    const Colour to = style.dark ? darkTo : glassTo;
    const float span = std::max(body.width * body.width + body.height * body.height, 1.0f);
    const Colour tint = style.dark ? Colour{0, 0, 0, 255} : Colour{255, 255, 255, 255};
    fillRoundRect(canvas, shape, [&](Point point, float) {
        const float t = ((point.x - body.x) * body.width + (point.y - body.y) * body.height) / span;
        Colour fill = mix(from, to, std::clamp(t, 0.0f, 1.0f));
        // The tint is laid over the fill: its colour by 8 %, and the fill's opacity covered by 8 %.
        const float covered = static_cast<float>(fill.a) / 255.0f;
        fill = mix(fill, tint, tintAlpha);
        fill.a = static_cast<std::uint8_t>(
            std::lround((covered + (1.0f - covered) * tintAlpha) * 255.0f));
        return withAlpha(fill, alpha);
    });
    const Colour border = style.dark ? darkBorder : lightBorder;
    fillBand(canvas, shape, shape.grown(-style.borderWidth), [&](Point, float) {
        return withAlpha(border, alpha);
    });
}

} // namespace opensu::ui
