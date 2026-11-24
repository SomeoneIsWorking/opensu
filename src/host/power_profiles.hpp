// power_profiles — the handheld's power profile (Quiet, Balanced, Performance) through PowerStation
// (ShadowBlip's power daemon on Bazzite, ChimeraOS and the like), which lets the user change it
// without root. A profile sets PowerStation's own profile and its share of the processor's power
// limit. A machine without PowerStation has no power profile row.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "runner.hpp"

namespace opensu::host {

struct PowerProfileState {
    /// The profile in force and the ones to choose from (`quiet`, `balanced`, `performance`).
    std::string profile;
    std::vector<std::string> profiles;
};

class PowerProfiles {
  public:
    virtual ~PowerProfiles() = default;

    /// The profiles now, or nothing when they cannot be read.
    [[nodiscard]] virtual std::optional<PowerProfileState> read() = 0;
    /// Puts `profile` in force; empty when done, else why not.
    virtual std::string set(const std::string& profile) = 0;
};

/// The power limit `profile` gives within `minWatts`..`maxWatts`: a third of the most for quiet,
/// half for balanced, all of it for performance.
[[nodiscard]] int profileWatts(const std::string& profile, int minWatts, int maxWatts);

/// PowerStation's profiles for the first GPU that has a power limit, or null when it has none.
[[nodiscard]] std::unique_ptr<PowerProfiles> makePowerStationProfiles(Runner run);

/// Wraps `inner` so reads pass and changes are refused with `reason`; for hidden runs.
[[nodiscard]] std::unique_ptr<PowerProfiles>
makeReadOnlyPowerProfiles(std::unique_ptr<PowerProfiles> inner, std::string reason);

} // namespace opensu::host
