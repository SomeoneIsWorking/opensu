#include "page_pill.hpp"

#include <algorithm>

#include "round_shape.hpp"

namespace opensu::ui {
namespace {

// iiSU wf7.n.
constexpr Colour nearShadow{0, 0, 0, 0x22};
constexpr Colour farShadow{0, 0, 0, 0x14};
// iiSU vi5.s #222933 at alpha 214 (dark), vi5.t #FFFFFF at 250 (light).
constexpr Colour darkBody{0x22, 0x29, 0x33, 214};
constexpr Colour lightBody{0xFF, 0xFF, 0xFF, 250};
constexpr Colour darkOuterStroke{0xFF, 0xFF, 0xFF, 0x66};
constexpr Colour lightOuterStroke{0xFF, 0xFF, 0xFF, 0xF2};
constexpr Colour darkInnerStroke{0xFF, 0xFF, 0xFF, 0x24};
constexpr Colour lightInnerStroke{0xFF, 0xFF, 0xFF, 0xDF};
// iiSU wf7.l: lime active dot with a green halo.
constexpr Colour activeDot{0x7C, 0xFF, 0x4F, 0xFF};
constexpr Colour halo{0xB8, 0xFF, 0x8A, 0x55};
constexpr Colour darkInactiveDot{0xFF, 0xFF, 0xFF, 0x78};
constexpr Colour lightInactiveDot{0x4D, 0x46, 0x55, 0x84};

VertexColour flat(Colour colour) {
    return [colour](Point, float) {
        return colour;
    };
}

} // namespace

void PagePillPainter::paint(Canvas& canvas, const PagePill& pill, float dp, bool dark) const {
    if (pill.dots.empty()) {
        return;
    }
    const float radius = pill.body.height * 0.5f;
    const RoundRect shape{pill.body, radius};
    for (const auto& [offset, colour] : {std::pair{2.0f, nearShadow}, std::pair{3.6f, farShadow}}) {
        RoundRect shadow = shape;
        shadow.rect.y += offset * dp;
        fillRoundRect(canvas, shadow, flat(colour));
    }
    fillRoundRect(canvas, shape, flat(dark ? darkBody : lightBody));
    const float outerWidth = 1.4f * dp;
    strokeSoft(canvas, shape, outerWidth, 0.0f, shape.grown(outerWidth),
               flat(dark ? darkOuterStroke : lightOuterStroke));
    const Rect insetRect{pill.body.x + 2.0f * dp, pill.body.y + 2.0f * dp,
                         pill.body.width - 4.0f * dp, pill.body.height - 4.0f * dp};
    const RoundRect inner{insetRect, std::max(insetRect.height * 0.5f, 1.0f)};
    strokeSoft(canvas, inner, 1.1f * dp, 0.0f, inner.grown(dp),
               flat(dark ? darkInnerStroke : lightInnerStroke));

    for (const PageDot& dot : pill.dots) {
        if (dot.active) {
            canvas.fillCircle(Point{dot.x, dot.y}, pill.haloRadius, halo);
        }
        canvas.fillCircle(Point{dot.x, dot.y}, dot.radius,
                          dot.active ? activeDot : (dark ? darkInactiveDot : lightInactiveDot));
    }
}

} // namespace opensu::ui
