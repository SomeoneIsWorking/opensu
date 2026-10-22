// integrations — which of openSU's optional integrations the player turned on, and the one rule
// for what is in force from that choice: the ROM library and the libretro runner are always on;
// Steam, Epic, GOG, standalone emulators and Lua plugins only when chosen. Steam runs in a mode:
// Normal uses the player's desktop Steam, Managed is openSU's own in its Gamescope, Isolated is
// openSU's own in a sandbox that shows it only openSU's display and the controllers it is given.
// Only the modes the build's Steam client has are offered (`steam::clientModes`), and a login
// session has no desktop Steam, so there Normal is not offered.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
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
    /// As Managed, inside a bwrap sandbox: the player's home and login, openSU's Gamescope as its
    /// only display, no desktop session bus, and only the controllers `SteamPads` names.
    Isolated,
};

[[nodiscard]] std::string_view key(SteamMode mode) noexcept;
[[nodiscard]] std::optional<SteamMode> steamModeOf(std::string_view spelling) noexcept;
[[nodiscard]] std::string_view label(SteamMode mode) noexcept;

/// The controllers Isolated Steam's sandbox has.
enum class SteamPads : std::uint8_t {
    /// openSU's four fixed virtual pads alone; the player's controllers are not in its /dev.
    VirtualOnly,
    /// Every input node, as on the desktop; openSU grabs the player's pads while it is in front.
    All,
};

[[nodiscard]] std::string_view key(SteamPads pads) noexcept;
[[nodiscard]] std::optional<SteamPads> steamPadsOf(std::string_view spelling) noexcept;
/// What Steam can see, as the choice reads.
[[nodiscard]] std::string_view label(SteamPads pads) noexcept;
/// What that means for the player's controllers, in one line.
[[nodiscard]] std::string_view note(SteamPads pads) noexcept;

/// What the player chose, as the settings file keeps it.
struct IntegrationChoices {
    bool steam{false};
    /// Absent until the player picks one; the default mode applies.
    std::optional<SteamMode> steamMode;
    /// Absent until the player picks one; the default applies. Isolated only.
    std::optional<SteamPads> steamPads{};
    bool epic{false};
    bool gog{false};
    /// Standalone emulators: their downloads and finding them on this machine.
    bool emulators{false};
    bool plugins{false};

    bool operator==(const IntegrationChoices&) const = default;
};

/// Steam as it is in force.
enum class SteamUse : std::uint8_t { Off, Normal, Managed, Isolated };

[[nodiscard]] std::string_view label(SteamUse use) noexcept;

/// The integrations in force.
struct Integrations {
    SteamUse steam{SteamUse::Off};
    /// What Isolated Steam's sandbox has of the controllers; ignored in the other modes.
    SteamPads steamPads{SteamPads::VirtualOnly};
    bool epic{false};
    bool gog{false};
    bool emulators{false};
    bool plugins{false};

    /// What `choices` puts in force; nothing chosen yet is everything off. `loginSession` is
    /// openSU running as the login session (`--session`); `built` the modes the build has.
    [[nodiscard]] static Integrations inForce(const std::optional<IntegrationChoices>& choices,
                                              bool loginSession, std::span<const SteamMode> built);
    /// The modes the player can pick, in `built`'s order: no Normal in a login session.
    [[nodiscard]] static std::vector<SteamMode> modesOffered(bool loginSession,
                                                             std::span<const SteamMode> built);
    /// The mode in force when Steam is on and none was picked.
    [[nodiscard]] static SteamMode defaultMode(bool loginSession) noexcept;
    /// The controller choices Isolated offers, the default first.
    [[nodiscard]] static std::vector<SteamPads> padsOffered();

    /// Whether the catalog lists `source`; ROMs always.
    [[nodiscard]] bool lists(library::Source source) const noexcept;

    bool operator==(const Integrations&) const = default;
};

} // namespace opensu::settings
