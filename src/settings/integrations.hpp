// integrations — which of openSU's optional integrations the player turned on, and the one rule
// for what is in force from that choice: the ROM library and the libretro runner are always on;
// Steam, Epic, GOG, standalone emulators and Lua plugins only when chosen. Steam runs in a mode:
// Normal uses the player's desktop Steam, Managed is openSU's own in its Gamescope. A login
// session has no desktop Steam, so there Steam is always Managed.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "library/game.hpp"

namespace opensu::settings {

/// How openSU uses Steam once it is on.
enum class SteamMode : std::uint8_t {
    /// The player's own Steam on the desktop: attached to, or started there, never ended.
    Normal,
    /// openSU's own Steam, started with `-silent` in its Gamescope scope and ended with it.
    Managed,
};

[[nodiscard]] std::string_view key(SteamMode mode) noexcept;
[[nodiscard]] std::optional<SteamMode> steamModeOf(std::string_view spelling) noexcept;
[[nodiscard]] std::string_view label(SteamMode mode) noexcept;

/// What the player chose, as the settings file keeps it.
struct IntegrationChoices {
    bool steam{false};
    /// Absent until the player picks one; the default mode applies.
    std::optional<SteamMode> steamMode;
    bool epic{false};
    bool gog{false};
    /// Standalone emulators: their downloads and finding them on this machine.
    bool emulators{false};
    bool plugins{false};

    bool operator==(const IntegrationChoices&) const = default;
};

/// Steam as it is in force.
enum class SteamUse : std::uint8_t { Off, Normal, Managed };

[[nodiscard]] std::string_view label(SteamUse use) noexcept;

/// The integrations in force.
struct Integrations {
    SteamUse steam{SteamUse::Off};
    bool epic{false};
    bool gog{false};
    bool emulators{false};
    bool plugins{false};

    /// What `choices` puts in force; nothing chosen yet is everything off. `loginSession` is
    /// openSU running as the login session (`--session`).
    [[nodiscard]] static Integrations inForce(const std::optional<IntegrationChoices>& choices,
                                              bool loginSession) noexcept;
    /// The modes the player can pick: Managed alone in a login session.
    [[nodiscard]] static std::vector<SteamMode> modesOffered(bool loginSession);
    /// The mode in force when Steam is on and none was picked.
    [[nodiscard]] static SteamMode defaultMode(bool loginSession) noexcept;

    /// Whether the catalog lists `source`; ROMs always.
    [[nodiscard]] bool lists(library::Source source) const noexcept;

    bool operator==(const Integrations&) const = default;
};

} // namespace opensu::settings
