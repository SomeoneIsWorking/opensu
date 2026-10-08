// tile_motion — the home grid's motion as functions of elapsed milliseconds.
//
// iiSU's grid renderer is clocked by uptime and frame time, not by the system
// animation scale (motion.md §5), so every curve here takes plain ms.
#pragma once

#include "home_layout.hpp"

namespace iideck::ui::motion {

/// iiSU w70.m and ox2.g: 1 - (1 - t)^3.
[[nodiscard]] float easeOutCubic(float t) noexcept;
/// A CSS-style cubic-bezier easing from (0, 0) through control points (x1, y1) and (x2, y2) to (1,
/// 1).
struct CubicBezier {
    double x1;
    double y1;
    double x2;
    double y2;
};
/// The curve's y where its x is `x`, for `x` in [0, 1].
[[nodiscard]] double cubicBezierEase(double x, const CubicBezier& curve) noexcept;
/// Compose's FastOutSlowInEasing, CubicBezierEasing(0.4, 0, 0.2, 1), which Material's
/// CircularEasing is too.
[[nodiscard]] double fastOutSlowIn(double x) noexcept;
/// Material's FastOutLinearInEasing, (0.4, 0, 1, 1): slow start, for things leaving.
[[nodiscard]] double fastOutLinearIn(double x) noexcept;
/// Material's LinearOutSlowInEasing, (0, 0, 0.2, 1): fast start, for things arriving.
[[nodiscard]] double linearOutSlowIn(double x) noexcept;
/// iiSU nx2.P: (3 - 2t) t^2.
[[nodiscard]] float smoothstep(float t) noexcept;

/// Section entry of an XMB or a Carousel (navigation.md §5.3,
/// `rec_roms_carousel_light_section_entry`): the focused tile fades in over 130 ms, its neighbours
/// from 170 ms to 270 ms, the next ones from 270 ms to 370 ms. `distance` is the tile's whole-tile
/// distance from the focus.
inline constexpr float railEntranceMs = 370.0f;
struct RailEntrance {
    float sinceMs;
    int distance;
};
[[nodiscard]] float railEntranceAlpha(const RailEntrance& at) noexcept;

/// The focused tile's scale `sinceFocusMs` after focus arrived on it.
[[nodiscard]] float focusScale(float sinceFocusMs) noexcept;
/// When the focus scale stops changing.
inline constexpr float focusSettleMs = 265.0f;

/// One tile's state during the domino entrance.
struct Entrance {
    float alpha{1.0f};
    float scale{1.0f};
};

/// A tile's wave step (iiSU ox2.j, horizontal flow): column - (row + rowSpan - 1).
[[nodiscard]] int dominoStep(const GridCell& cell) noexcept;
/// A tile `sinceStartMs` into the entrance, `stepsFromFirst` steps after the first to move.
[[nodiscard]] Entrance domino(float sinceStartMs, int stepsFromFirst) noexcept;
/// The entrance's length for a spread of steps (iiSU nx2.R).
[[nodiscard]] float dominoDurationMs(int stepSpread) noexcept;
/// A re-trigger this soon after the last start is dropped (iiSU nx2.R).
inline constexpr float dominoDedupeMs = 1500.0f;

/// The pressed tile's scale `sincePressMs` after A.
[[nodiscard]] float pulseScale(float sincePressMs) noexcept;
inline constexpr float pulseMs = 260.0f;

/// The focus ring's sweep rotation in degrees at an uptime.
[[nodiscard]] float ringAngleDegrees(double uptimeMs) noexcept;

/// The focus position of an XMB or Carousel, following the focused index with a lag (iiSU
/// `w70.S`: each frame covers `1 - (1 - min(dt, 20 ms) / 245) ^ 4` of what is left).
class VisualIndexEaser {
  public:
    /// Jumps to `index`, settled.
    void snap(float index) noexcept;
    /// Heads for `index`.
    void retarget(float index) noexcept;
    /// Advances by one frame of `dtMs`.
    void step(float dtMs) noexcept;

    [[nodiscard]] float value() const noexcept {
        return value_;
    }
    [[nodiscard]] bool settled() const noexcept {
        return value_ == target_;
    }

  private:
    float value_{};
    float target_{};
};

/// Eases Flow's scroll offset towards its target (iiSU hx2.P).
class ScrollEaser {
  public:
    /// Jumps to an offset, settled.
    void snap(float offset) noexcept;
    /// Sets a new target from a focus move.
    void retarget(float target) noexcept;
    /// Advances by `dtMs`; reports whether the offset is still moving.
    bool step(float dtMs, float pitch, float viewport, ScrollMode mode) noexcept;

    [[nodiscard]] float offset() const noexcept {
        return offset_;
    }
    [[nodiscard]] float target() const noexcept {
        return target_;
    }

  private:
    float offset_{};
    float target_{};
    bool settled_{true};
};

} // namespace iideck::ui::motion
