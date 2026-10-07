#include "glass.hpp"

#include <algorithm>

#include "raylib.h"

#include "round_shape.hpp"

namespace iideck::ui {
namespace {

// iiSU ea3.e, light: na3.e #F2F4F7 at 0.55 to na3.f #E6E9EE at 0.65, top-left to bottom-right.
constexpr Color glassFrom{0xF2, 0xF4, 0xF7, 140};
constexpr Color glassTo{0xE6, 0xE9, 0xEE, 166};

} // namespace

void GlassPainter::paint(const Rect& body) const {
    // STOPGAP: only ea3.e's fill is drawn because ea3.n's blur, border and shadow layers are not
    // in the spec.
    const float span = std::max(body.width * body.width + body.height * body.height, 1.0f);
    fillRoundRect(RoundRect{body, body.height * 0.5f}, [body, span](Vector2 point, float) {
        // Compose resolves the infinite end offset to the size, so the axis is (w, h).
        const float t = ((point.x - body.x) * body.width + (point.y - body.y) * body.height) / span;
        return mix(glassFrom, glassTo, std::clamp(t, 0.0f, 1.0f));
    });
}

} // namespace iideck::ui
