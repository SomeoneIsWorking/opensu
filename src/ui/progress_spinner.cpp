#include "progress_spinner.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "tile_motion.hpp"

namespace opensu::ui {
namespace {

// compose-material3 ProgressIndicator.kt indeterminate circular constants.
constexpr int rotationsPerCycle = 5;
constexpr double rotationDuration = 1.332;
constexpr float startAngleOffset = -90.0f;
constexpr float baseRotationAngle = 286.0f;
constexpr float jumpRotationAngle = 290.0f;
constexpr float rotationAngleOffset = 216.0f;
constexpr double headAndTailDuration = 0.666;

/// A keyframe pair: 0 until `delay`, eased up to the jump over the next half rotation.
float headOrTail(double phase, double delay) noexcept {
    const double x = (phase - delay) / headAndTailDuration;
    if (x <= 0.0) {
        return 0.0f;
    }
    if (x >= 1.0) {
        return jumpRotationAngle;
    }
    return static_cast<float>(motion::fastOutSlowIn(x)) * jumpRotationAngle;
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

void drawSpinner(Canvas& canvas, Point centre, float size, float stroke, Colour colour,
                 double seconds) {
    const SpinnerArc arc = spinnerArc(seconds);
    const float outer = size * 0.5f;
    // The square cap reaches half a stroke past each end of the arc.
    const float cap =
        (stroke * 0.5f) / (outer - stroke * 0.5f) * 180.0f / std::numbers::pi_v<float>;
    canvas.ring(gpu2d::Ring{.centre = centre,
                            .inner = outer - stroke,
                            .outer = outer,
                            .startDegrees = arc.start - cap,
                            .endDegrees = arc.start + arc.sweep + cap},
                colour);
}

} // namespace opensu::ui
