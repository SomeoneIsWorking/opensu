// paint — what the painters draw with: gpu2d's canvas, its colours and points, the layout's Rect as
// the canvas takes it, and scopes that clip or scale what is drawn inside them.
#pragma once

#include "gpu2d/canvas.hpp"
#include "gpu2d/geometry.hpp"
#include "gpu2d/texture.hpp"

#include "home_layout.hpp"

namespace opensu::ui {

using Canvas = gpu2d::Canvas;
using Colour = gpu2d::Colour;
using Point = gpu2d::Point;

inline constexpr Colour white{255, 255, 255, 255};
inline constexpr Colour black{0, 0, 0, 255};

/// The layout's rectangle as the canvas takes it.
[[nodiscard]] constexpr gpu2d::Rect onCanvas(const Rect& rect) noexcept {
    return gpu2d::Rect{rect.x, rect.y, rect.width, rect.height};
}

/// The corner radius a `roundness` gives `rect`: 0 square, 1 half its shorter side.
[[nodiscard]] constexpr float cornerRadius(const Rect& rect, float roundness) noexcept {
    const float shorter = rect.width < rect.height ? rect.width : rect.height;
    const float clamped = roundness < 0.0f ? 0.0f : (roundness > 1.0f ? 1.0f : roundness);
    return shorter * clamped * 0.5f;
}

/// The whole of `texture`, as an image's source rectangle.
[[nodiscard]] constexpr gpu2d::Rect wholeOf(const gpu2d::Texture& texture) noexcept {
    return gpu2d::Rect{0.0f, 0.0f, static_cast<float>(texture.width()),
                       static_cast<float>(texture.height())};
}

/// Clips what is drawn to `area` (whole pixels inside it, within any clip already pushed) for as
/// long as it lives.
class ScopedClip {
  public:
    ScopedClip(Canvas& canvas, const Rect& area) : canvas_{canvas} {
        canvas_.pushClip(onCanvas(area));
    }
    ~ScopedClip() {
        canvas_.popClip();
    }
    ScopedClip(const ScopedClip&) = delete;
    ScopedClip& operator=(const ScopedClip&) = delete;
    ScopedClip(ScopedClip&&) = delete;
    ScopedClip& operator=(ScopedClip&&) = delete;

  private:
    Canvas& canvas_;
};

/// Draws what follows scaled about a point for as long as it lives, as a panel grows in or
/// shrinks out.
class PanelScope {
  public:
    PanelScope(Canvas& canvas, Point about, float scale) : canvas_{canvas} {
        canvas_.pushTransform(gpu2d::Transform::scaleAbout(about, scale));
    }
    ~PanelScope() {
        canvas_.popTransform();
    }
    PanelScope(const PanelScope&) = delete;
    PanelScope& operator=(const PanelScope&) = delete;
    PanelScope(PanelScope&&) = delete;
    PanelScope& operator=(PanelScope&&) = delete;

  private:
    Canvas& canvas_;
};

} // namespace opensu::ui
