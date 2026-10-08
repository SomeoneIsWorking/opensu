#include "progress_spinner.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace iideck::ui {
namespace {

// compose-material3 ProgressIndicator.kt indeterminate circular constants.
constexpr int rotationsPerCycle = 5;
constexpr double rotationDuration = 1.332;
constexpr float startAngleOffset = -90.0f;
constexpr float baseRotationAngle = 286.0f;
constexpr float jumpRotationAngle = 290.0f;
constexpr float rotationAngleOffset = 216.0f;
constexpr double headAndTailDuration = 0.666;
// CircularEasing = CubicBezierEasing(0.4, 0, 0.2, 1).
constexpr double easeX1 = 0.4;
constexpr double easeY1 = 0.0;
constexpr double easeX2 = 0.2;
constexpr double easeY2 = 1.0;
constexpr int segmentsPerTurn = 64;

double bezier(double t, double p1, double p2) noexcept {
    const double u = 1.0 - t;
    return 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t;
}

/// The eased value of `x` in [0, 1]: the curve's y where its x is `x`.
double circularEasing(double x) noexcept {
    double low = 0.0;
    double high = 1.0;
    for (int i = 0; i < 40; ++i) {
        const double mid = (low + high) * 0.5;
        if (bezier(mid, easeX1, easeX2) < x) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return bezier((low + high) * 0.5, easeY1, easeY2);
}

/// A keyframe pair: 0 until `delay`, eased up to the jump over the next half rotation.
float headOrTail(double phase, double delay) noexcept {
    const double x = (phase - delay) / headAndTailDuration;
    if (x <= 0.0) {
        return 0.0f;
    }
    if (x >= 1.0) {
        return jumpRotationAngle;
    }
    return static_cast<float>(circularEasing(x)) * jumpRotationAngle;
}

} // namespace

SpinnerArc spinnerArc(double seconds) noexcept {
    const double cycle = rotationDuration * rotationsPerCycle;
    const double inCycle = std::fmod(std::max(seconds, 0.0), cycle);
    // An integer animation: it steps once per rotation.
    const int rotation = static_cast<int>(inCycle / rotationDuration);
    const double phase = std::fmod(inCycle, rotationDuration);
    const float baseRotation = static_cast<float>(phase / rotationDuration) * baseRotationAngle;
    const float head = headOrTail(phase, 0.0);
    const float tail = headOrTail(phase, headAndTailDuration);
    const float offset = startAngleOffset +
                         std::fmod(static_cast<float>(rotation) * rotationAngleOffset, 360.0f) +
                         baseRotation;
    return SpinnerArc{.start = tail + offset, .sweep = std::abs(head - tail)};
}

void drawSpinner(Vector2 centre, float size, float stroke, Color colour, double seconds) {
    const SpinnerArc arc = spinnerArc(seconds);
    const float outer = size * 0.5f;
    // The square cap reaches half a stroke past each end of the arc.
    const float cap =
        (stroke * 0.5f) / (outer - stroke * 0.5f) * 180.0f / std::numbers::pi_v<float>;
    const float from = arc.start - cap;
    const float to = arc.start + arc.sweep + cap;
    const int segments = std::max(2, static_cast<int>((to - from) / 360.0f * segmentsPerTurn));
    DrawRing(centre, outer - stroke, outer, from, to, segments, colour);
}

} // namespace iideck::ui
