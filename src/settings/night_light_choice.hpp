// night_light_choice — what the player chose for the night light (Settings › Appearance › Night
// light): the desktop's own, off or on, and how the settings file spells it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::settings {

enum class NightLightChoice : std::uint8_t {
    /// The desktop's night light, its schedule and temperatures.
    Desktop,
    Off,
    /// The desktop's night temperature all day.
    On,
};

inline constexpr std::array allNightLightChoices{NightLightChoice::Desktop, NightLightChoice::Off,
                                                 NightLightChoice::On};

/// The choice as the settings file spells it: "desktop", "off", "on".
[[nodiscard]] std::string_view key(NightLightChoice choice) noexcept;

/// The choice a spelling names, or nothing.
[[nodiscard]] std::optional<NightLightChoice>
nightLightChoiceOf(std::string_view spelling) noexcept;

/// The choice as the player reads it: "Same as desktop", "Off", "On".
[[nodiscard]] std::string_view label(NightLightChoice choice) noexcept;

} // namespace opensu::settings
