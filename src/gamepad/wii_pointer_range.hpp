// wii_pointer_range — which part of a Wii Remote's camera view is the screen, found by pointing at
// two of the screen's corners. The aims are the camera's (`WiiPointer`): 0 to 1 across and down its
// view, mirrored across. Two corners give the screen's edges in that view whatever the sensor bar's
// place, the camera's range or either axis's direction, and the bar's place against the screen.
#pragma once

#include <cstdint>
#include <optional>

namespace opensu::gamepad {

/// Where the sensor bar stands against the screen.
enum class SensorBarPlace : std::uint8_t {
    Above,
    Below,
    /// Within the screen's height: beside it, or the range is not the player's.
    Level,
};

/// The camera aims at the screen's edges. The default is the camera's whole view.
struct WiiPointerRange {
    float left{0.0f};
    float right{1.0f};
    float top{0.0f};
    float bottom{1.0f};

    /// The range from the aims read pointing at the screen's top-left and bottom-right corners;
    /// nothing when the two are too close to tell the screen's size.
    [[nodiscard]] static std::optional<WiiPointerRange>
    fromCorners(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY);

    /// A camera aim across the screen, 0 at its left edge and 1 at its right.
    [[nodiscard]] float across(float aim) const noexcept;
    /// A camera aim down the screen, 0 at its top and 1 at its bottom.
    [[nodiscard]] float down(float aim) const noexcept;
    /// Where the bar is: the spot a remote aiming straight at it (its dots in the middle of the
    /// camera's view) points at, above or below the screen or within its height.
    [[nodiscard]] SensorBarPlace barPlace() const noexcept;

    bool operator==(const WiiPointerRange&) const = default;
};

} // namespace opensu::gamepad
