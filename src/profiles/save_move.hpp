// save_move — the one-time move of the saves kept before profiles into the first profile's: the
// built-in runner's `<saves>/<system>/` into `<saves>/profiles/<id>/<system>/` (backed up first
// under `<state>/save-backups/before-profiles/<UTC time>/`, only the files that move), Copy saves'
// backups into the profile's, and the sync record's entries that name no profile. Each file is
// renamed into place, so a move cut short goes on from where it stopped the next time; a file whose
// place is taken stays where it is. Once nothing is left outside `profiles/` it does nothing.
#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace opensu::profiles {

/// Under `<state>/save-backups`: the copy of the saves made before they moved.
inline constexpr std::string_view beforeProfilesFolder = "before-profiles";

struct SaveMove {
    /// The runner's saves (`Config::savesDir`).
    std::filesystem::path saves;
    /// The state dir Copy saves' backups and the sync record are under.
    std::filesystem::path state;
    /// The sync record (`save_sync::saveVersionsFile` under `state`).
    std::filesystem::path versions;
};

struct Moved {
    std::size_t files{0};
    /// Files left where they were: their place in the profile is taken.
    std::size_t left{0};
    /// The backup made before the move; empty when there was nothing to move.
    std::filesystem::path backup;
};

/// Moves the saves `move` names that predate profiles into `profileId`'s. Throws
/// `std::filesystem::filesystem_error` when the backup cannot be made (nothing moved then) or a
/// file cannot be moved (those before it moved).
Moved moveIntoProfile(const SaveMove& move, std::string_view profileId,
                      std::chrono::system_clock::time_point now);

} // namespace opensu::profiles
