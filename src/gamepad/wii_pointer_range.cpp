#include "wii_pointer_range.hpp"

#include <cmath>

namespace opensu::gamepad {
namespace {

// Corners closer than this in the camera's view are one spot pointed at twice.
constexpr float leastSpan = 0.05f;
// The middle of the camera's view.
constexpr float middle = 0.5f;

} // namespace

std::optional<WiiPointerRange> WiiPointerRange::fromCorners(float topLeftX, float topLeftY,
                                                            float bottomRightX,
                                                            float bottomRightY) {
    if (std::abs(bottomRightX - topLeftX) < leastSpan ||
        std::abs(bottomRightY - topLeftY) < leastSpan) {
        return std::nullopt;
    }
    return WiiPointerRange{
        .left = topLeftX, .right = bottomRightX, .top = topLeftY, .bottom = bottomRightY};
}

float WiiPointerRange::across(float aim) const noexcept {
    return (aim - left) / (right - left);
}

float WiiPointerRange::down(float aim) const noexcept {
    return (aim - top) / (bottom - top);
}

SensorBarPlace WiiPointerRange::barPlace() const noexcept {
    const float bar = down(middle);
    if (bar < 0.0f) {
        return SensorBarPlace::Above;
    }
    if (bar > 1.0f) {
        return SensorBarPlace::Below;
    }
    return SensorBarPlace::Level;
}

} // namespace opensu::gamepad
