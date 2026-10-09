#include "page_pill.hpp"

#include <algorithm>

#include "raylib.h"

#include "round_shape.hpp"

namespace opensu::ui {
namespace {

// iiSU wf7.n.
constexpr Color nearShadow{0, 0, 0, 0x22};
constexpr Color farShadow{0, 0, 0, 0x14};
// iiSU vi5.s #222933 at alpha 214 (dark), vi5.t #FFFFFF at 250 (light).
constexpr Color darkBody{0x22, 0x29, 0x33, 214};
constexpr Color lightBody{0xFF, 0xFF, 0xFF, 250};
constexpr Color darkOuterStroke{0xFF, 0xFF, 0xFF, 0x66};
constexpr Color lightOuterStroke{0xFF, 0xFF, 0xFF, 0xF2};
constexpr Color darkInnerStroke{0xFF, 0xFF, 0xFF, 0x24};
constexpr Color lightInnerStroke{0xFF, 0xFF, 0xFF, 0xDF};
// iiSU wf7.l: lime active dot with a green halo.
constexpr Color activeDot{0x7C, 0xFF, 0x4F, 0xFF};
constexpr Color halo{0xB8, 0xFF, 0x8A, 0x55};
constexpr Color darkInactiveDot{0xFF, 0xFF, 0xFF, 0x78};
constexpr Color lightInactiveDot{0x4D, 0x46, 0x55, 0x84};

VertexColour flat(Color colour) {
    return [colour](Vector2, float) {
        return colour;
    };
}

} // namespace

void PagePillPainter::paint(const PagePill& pill, float dp, bool dark) const {
    if (pill.dots.empty()) {
        return;
    }
    const float radius = pill.body.height * 0.5f;
    const RoundRect shape{pill.body, radius};
    for (const auto& [offset, colour] : {std::pair{2.0f, nearShadow}, std::pair{3.6f, farShadow}}) {
        RoundRect shadow = shape;
        shadow.rect.y += offset * dp;
        fillRoundRect(shadow, flat(colour));
    }
    fillRoundRect(shape, flat(dark ? darkBody : lightBody));
    const float outerWidth = 1.4f * dp;
    strokeSoft(shape, outerWidth, 0.0f, shape.grown(outerWidth),
               flat(dark ? darkOuterStroke : lightOuterStroke));
    const Rect insetRect{pill.body.x + 2.0f * dp, pill.body.y + 2.0f * dp,
                         pill.body.width - 4.0f * dp, pill.body.height - 4.0f * dp};
    const RoundRect inner{insetRect, std::max(insetRect.height * 0.5f, 1.0f)};
    strokeSoft(inner, 1.1f * dp, 0.0f, inner.grown(dp),
               flat(dark ? darkInnerStroke : lightInnerStroke));

    for (const PageDot& dot : pill.dots) {
        if (dot.active) {
            DrawCircleV(Vector2{dot.x, dot.y}, pill.haloRadius, halo);
        }
        DrawCircleV(Vector2{dot.x, dot.y}, dot.radius,
                    dot.active ? activeDot : (dark ? darkInactiveDot : lightInactiveDot));
    }
}

} // namespace opensu::ui
