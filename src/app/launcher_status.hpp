// launcher_status — what the top bar's launcher badges show: Steam from the client iideck owns
// and its download queue, Epic and GOG from the catalog's last read of them.
#pragma once

#include <span>
#include <string>
#include <vector>

#include "launch/steam_gate.hpp"
#include "launcher_badges.hpp"
#include "library/game.hpp"
#include "steam/downloads.hpp"

namespace iideck::app {

/// One badge per launcher, Steam first. A launcher not on this machine is hidden.
[[nodiscard]] std::vector<ui::LauncherBadge>
launcherBadges(launch::SteamState steam, std::span<const steam::Download> downloads,
               std::span<const library::SourceStatus> sources);

/// The badges as `steam=ready epic=failed`, hidden ones left out, for the control channel.
[[nodiscard]] std::string describe(std::span<const ui::LauncherBadge> badges);

} // namespace iideck::app
