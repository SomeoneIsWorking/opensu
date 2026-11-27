#include "hall_correction.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::gamepad {
namespace {

/// The rest offset counts in full at centre and fades out towards the edges.
float edgeGain(float value) {
    const float near = std::max(0.0f, 1.0f - std::abs(value));
    return near * near;
}

/// Takes out a shift of `pull`. The reading stops at the end the pull moves it toward, so that
/// side is stretched back to its end rather than shifted off it.
float withoutPull(float value, float pull) {
    if (pull < 0.0f && value < pull) {
        return (value - pull) / (1.0f + pull);
    }
    if (pull > 0.0f && value > pull) {
        return (value - pull) / (1.0f - pull);
    }
    return value - pull;
}

float remapped(float value, float deadzone) {
    const float past = (std::abs(value) - deadzone) / (1.0f - deadzone);
    return std::copysign(std::max(0.0f, past), value);
}

/// A stick inside `deadzone` on both axes reads centred.
void applyDeadzone(float& x, float& y, float deadzone, bool remap) {
    if (deadzone <= 0.0f || deadzone >= 1.0f) {
        return;
    }
    if (std::abs(x) < deadzone && std::abs(y) < deadzone) {
        x = 0.0f;
        y = 0.0f;
    } else if (remap) {
        x = remapped(x, deadzone);
        y = remapped(y, deadzone);
    }
}

} // namespace

StickPositions corrected(StickPositions sticks, float leftTrigger, const HallTerms& terms) {
    const float trigger = std::clamp(leftTrigger, 0.0f, 1.0f);
    float& leftX = sticks.leftX;
    float& rightX = sticks.rightX;
    leftX = std::clamp(withoutPull(leftX, terms.triggerPullX * trigger), -1.0f, 1.0f);
    leftX = std::clamp(leftX - terms.leftIdleX * edgeGain(leftX), -1.0f, 1.0f);
    rightX = std::clamp(rightX - terms.rightIdleX * edgeGain(rightX), -1.0f, 1.0f);
    applyDeadzone(leftX, sticks.leftY, terms.leftDeadzone, terms.remapDeadzone);
    applyDeadzone(rightX, sticks.rightY, terms.rightDeadzone, terms.remapDeadzone);
    return sticks;
}

} // namespace opensu::gamepad
