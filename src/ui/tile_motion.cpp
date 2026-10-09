#include "tile_motion.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui::motion {
namespace {

// iiSU w70 (w70.java:813-840): 1.00 -> 1.05 over 145 ms, -> 1.03 by 265 ms.
constexpr float focusRiseMs = 145.0f;
constexpr float focusPeak = 1.05f;
constexpr float focusRest = 1.03f;

// iiSU nx2.R (nx2.java:6151-6166): 34 ms stagger, 165 ms to 1.025 from 0.78, 115 ms to 1.
constexpr float dominoStaggerMs = 34.0f;
constexpr float dominoRiseMs = 165.0f;
constexpr float dominoSettleMs = 115.0f;
constexpr float dominoStartScale = 0.78f;
constexpr float dominoOvershoot = 1.025f;

// iiSU nx2.java:6191-6206: segments end at 38% and 72%.
constexpr float pulseDipEnd = 0.38f;
constexpr float pulseRiseEnd = 0.72f;
constexpr float pulseDip = 0.96f;
constexpr float pulsePeak = 1.025f;

// iiSU tw2: one turn per 5600 ms of uptime.
constexpr double ringTurnMs = 5600.0;

double bezier(double t, double p1, double p2) noexcept {
    const double u = 1.0 - t;
    return 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t;
}

float lerp(float from, float to, float t) noexcept {
    return from + (to - from) * t;
}

} // namespace

double cubicBezierEase(double x, const CubicBezier& curve) noexcept {
    double low = 0.0;
    double high = 1.0;
    for (int i = 0; i < 40; ++i) {
        const double mid = (low + high) * 0.5;
        if (bezier(mid, curve.x1, curve.x2) < x) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return bezier((low + high) * 0.5, curve.y1, curve.y2);
}

double fastOutSlowIn(double x) noexcept {
    return cubicBezierEase(x, CubicBezier{0.4, 0.0, 0.2, 1.0});
}

double fastOutLinearIn(double x) noexcept {
    return cubicBezierEase(x, CubicBezier{0.4, 0.0, 1.0, 1.0});
}

double linearOutSlowIn(double x) noexcept {
    return cubicBezierEase(x, CubicBezier{0.0, 0.0, 0.2, 1.0});
}

float easeOutCubic(float t) noexcept {
    const float rest = 1.0f - std::clamp(t, 0.0f, 1.0f);
    return 1.0f - rest * rest * rest;
}

float smoothstep(float t) noexcept {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return (3.0f - 2.0f * c) * c * c;
}

float railEntranceAlpha(const RailEntrance& at) noexcept {
    const float sinceMs = at.sinceMs;
    const int distance = at.distance;
    constexpr float focusedFadeMs = 130.0f;
    constexpr float neighbourStartMs = 170.0f;
    constexpr float neighbourFadeMs = 100.0f;
    if (distance <= 0) {
        return std::clamp(sinceMs / focusedFadeMs, 0.0f, 1.0f);
    }
    const float start =
        neighbourStartMs + neighbourFadeMs * static_cast<float>(std::min(distance, 2) - 1);
    return std::clamp((sinceMs - start) / neighbourFadeMs, 0.0f, 1.0f);
}

float focusScale(float sinceFocusMs) noexcept {
    const float t = std::max(sinceFocusMs, 0.0f);
    if (t < focusRiseMs) {
        return lerp(1.0f, focusPeak, easeOutCubic(t / focusRiseMs));
    }
    if (t < focusSettleMs) {
        return lerp(focusPeak, focusRest,
                    easeOutCubic((t - focusRiseMs) / (focusSettleMs - focusRiseMs)));
    }
    return focusRest;
}

int dominoStep(const GridCell& cell) noexcept {
    return cell.left - (cell.top + (cell.bottom - cell.top + 1) - 1);
}

Entrance domino(float sinceStartMs, int stepsFromFirst) noexcept {
    const float local =
        sinceStartMs - static_cast<float>(std::max(stepsFromFirst, 0)) * dominoStaggerMs;
    if (local < 0.0f) {
        return Entrance{0.0f, dominoStartScale};
    }
    if (local < dominoRiseMs) {
        const float eased = easeOutCubic(local / dominoRiseMs);
        return Entrance{eased, lerp(dominoStartScale, dominoOvershoot, eased)};
    }
    const float settle = local - dominoRiseMs;
    if (settle < dominoSettleMs) {
        return Entrance{1.0f, lerp(dominoOvershoot, 1.0f, easeOutCubic(settle / dominoSettleMs))};
    }
    return Entrance{};
}

float dominoDurationMs(int stepSpread) noexcept {
    return static_cast<float>(std::max(stepSpread, 0)) * dominoStaggerMs + dominoRiseMs +
           dominoSettleMs;
}

float pulseScale(float sincePressMs) noexcept {
    if (sincePressMs < 0.0f || sincePressMs >= pulseMs) {
        return 1.0f;
    }
    const float t = sincePressMs / pulseMs;
    if (t < pulseDipEnd) {
        return lerp(1.0f, pulseDip, smoothstep(t / pulseDipEnd));
    }
    if (t < pulseRiseEnd) {
        return lerp(pulseDip, pulsePeak,
                    smoothstep((t - pulseDipEnd) / (pulseRiseEnd - pulseDipEnd)));
    }
    return lerp(pulsePeak, 1.0f, smoothstep((t - pulseRiseEnd) / (1.0f - pulseRiseEnd)));
}

float ringAngleDegrees(double uptimeMs) noexcept {
    const double phase = std::fmod(std::max(uptimeMs, 0.0), ringTurnMs) / ringTurnMs;
    return static_cast<float>(phase * 360.0);
}

void VisualIndexEaser::snap(float index) noexcept {
    value_ = index;
    target_ = index;
}

void VisualIndexEaser::retarget(float index) noexcept {
    target_ = index;
}

void VisualIndexEaser::step(float dtMs) noexcept {
    const float rest = 1.0f - std::min(std::max(dtMs, 0.0f), 20.0f) / 245.0f;
    const float follow = 1.0f - rest * rest * rest * rest;
    value_ += (target_ - value_) * follow;
    // Far under a pixel of a tile, so the frame stops redrawing a still row.
    if (std::abs(target_ - value_) < 0.0005f) {
        value_ = target_;
    }
}

void ScrollEaser::snap(float offset) noexcept {
    offset_ = offset;
    target_ = offset;
    settled_ = true;
}

void ScrollEaser::retarget(float target) noexcept {
    target_ = target;
    settled_ = false;
}

bool ScrollEaser::step(float dtMs, float pitch, float viewport, ScrollMode mode) noexcept {
    const float delta = target_ - offset_;
    const float distance = std::abs(delta);
    // iiSU hx2.P: snap under half a pixel.
    if (distance < 0.5f) {
        offset_ = target_;
        settled_ = true;
        return false;
    }
    const bool flow = mode == ScrollMode::Flow;
    const bool longJump = flow && distance >= std::max(1.35f * pitch, 0.32f * viewport);
    const bool quick = !settled_ || longJump;
    // iiSU hx2.P: tau 50 ms (min 0.14) while unsettled or on a long Flow jump, else 72 ms (min
    // 0.08).
    const float tau = quick ? 50.0f : 72.0f;
    const float alpha =
        std::clamp(1.0f - std::exp(-std::max(dtMs, 1.0f) / tau), quick ? 0.14f : 0.08f, 1.0f);
    const float wanted = std::clamp(alpha, 0.05f, 1.0f) * delta;
    float limit = std::min(std::max(std::min(std::abs(pitch) * 0.28f, distance), 42.0f), distance);
    const float wide =
        std::min(std::max(0.38f * distance, 96.0f),
                 std::min(distance, std::max(0.22f * viewport, std::abs(pitch) * 0.85f)));
    if (!settled_ || !flow) {
        limit = wide;
    } else if (longJump) {
        limit = std::max(limit, wide);
    }
    offset_ += std::clamp(wanted, -limit, limit);
    return true;
}

} // namespace opensu::ui::motion
