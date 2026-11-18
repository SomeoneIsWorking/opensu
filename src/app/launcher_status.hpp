// launcher_status — what the top bar's badges show: Steam from the client opensu owns, Epic and GOG
// from the catalog's last read of them, and Downloads from the Downloads page's live rows.
#pragma once

#include <span>
#include <string>
#include <vector>

#include "downloads_board.hpp"
#include "launch/steam_gate.hpp"
#include "launcher_badges.hpp"
#include "library/game.hpp"

namespace opensu::app {

/// One badge per launcher, Steam first, then Downloads. A launcher not on this machine is hidden,
/// and Downloads while nothing downloads (an update only on offer does not count); its progress is
/// the bytes moved of all live rows that count them, else the mean of their fractions, else none
/// and it spins.
[[nodiscard]] std::vector<ui::LauncherBadge>
launcherBadges(launch::SteamState steam, std::span<const DownloadRow> downloads,
               std::span<const library::SourceStatus> sources);

/// The badges as `steam=ready epic=failed`, hidden ones left out, for the control channel.
[[nodiscard]] std::string describe(std::span<const ui::LauncherBadge> badges);

} // namespace opensu::app
