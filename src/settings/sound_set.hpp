// sound_set — the interface sounds the player chose (Settings › Audio › Interface sounds): one of
// openSU's own sets or iiSU's, and how the settings file spells it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::settings {

enum class SoundSet : std::uint8_t {
    Marimba,
    Glass,
    Pluck,
    /// iiSU's, read from the iiSU file the player gave.
    Iisu,
};

inline constexpr std::array allSoundSets{SoundSet::Marimba, SoundSet::Glass, SoundSet::Pluck,
                                         SoundSet::Iisu};

/// The choice as the settings file spells it, which is also an openSU set's folder under
/// `assets/sounds/`: "marimba", "glass", "pluck", "iisu".
[[nodiscard]] std::string_view key(SoundSet choice) noexcept;

/// The choice a spelling names, or nothing; an older file's "opensu" is Marimba.
[[nodiscard]] std::optional<SoundSet> soundSetOf(std::string_view spelling) noexcept;

/// The choice as the player reads it: "Marimba", "Glass", "Pluck", "iiSU".
[[nodiscard]] std::string_view label(SoundSet choice) noexcept;

/// The openSU set loaded for the choice: itself, or Marimba under iiSU's, for the effects iiSU's
/// file lacks.
[[nodiscard]] SoundSet shippedSet(SoundSet choice) noexcept;

} // namespace opensu::settings
