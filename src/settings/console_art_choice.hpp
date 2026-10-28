// console_art_choice — whose console art the player chose (Settings › Artwork › Console art):
// openSU's own drawings or iiSU's, and how the settings file spells it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::settings {

enum class ConsoleArtChoice : std::uint8_t {
    OpenSu,
    /// iiSU's cards and tab glyphs, downloaded from iiSU's release on request.
    Iisu,
};

inline constexpr std::array allConsoleArtChoices{ConsoleArtChoice::OpenSu, ConsoleArtChoice::Iisu};

/// The choice as the settings file spells it: "opensu", "iisu".
[[nodiscard]] std::string_view key(ConsoleArtChoice choice) noexcept;

/// The choice a spelling names, or nothing.
[[nodiscard]] std::optional<ConsoleArtChoice>
consoleArtChoiceOf(std::string_view spelling) noexcept;

/// The choice as the player reads it: "openSU", "iiSU".
[[nodiscard]] std::string_view label(ConsoleArtChoice choice) noexcept;

} // namespace opensu::settings
