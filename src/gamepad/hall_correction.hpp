// hall_correction — undoes the ROG Ally's hall-effect interference: the left trigger's magnet pulls
// the left stick's x axis as it is pressed, and both sticks rest off centre. The left stick's x is
// moved back by the trigger's travel, each stick's rest offset is taken out near centre, and a
// deadzone remapped to the full travel hides what is left.
#pragma once

#include <cstdint>

namespace opensu::gamepad {

/// Whether the correction runs, and with whose terms.
enum class HallMode : std::uint8_t {
    Off,
    /// `HallTerms`' own defaults.
    Default,
    /// The player's terms.
    Manual,
};

/// The correction's terms, as fractions of a stick's half travel.
struct HallTerms {
    /// Where the left and right sticks' x rests uncorrected.
    float leftIdleX{-0.15f};
    float rightIdleX{0.10f};
    /// How far a fully pressed left trigger pulls the left stick's x.
    float triggerPullX{-0.28f};
    /// Inside these, with both axes, a stick reads centred.
    float leftDeadzone{0.15f};
    float rightDeadzone{0.05f};
    /// Travel past the deadzone is stretched to the full range, so no step shows at its edge.
    bool remapDeadzone{true};

    friend bool operator==(const HallTerms&, const HallTerms&) = default;
};

/// Both sticks, each axis from -1 to 1.
struct StickPositions {
    float leftX{};
    float leftY{};
    float rightX{};
    float rightY{};

    friend bool operator==(const StickPositions&, const StickPositions&) = default;
};

/// `sticks` as the player meant them, with the left trigger at `leftTrigger` (0 to 1).
[[nodiscard]] StickPositions corrected(StickPositions sticks, float leftTrigger,
                                       const HallTerms& terms);

} // namespace opensu::gamepad
