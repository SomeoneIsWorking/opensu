// backlight — the display's brightness: read from /sys/class/backlight, set through logind's
// Session.SetBrightness (so no root and no brightnessctl). A machine without a backlight device
// (a desktop monitor) has no brightness control.
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "runner.hpp"

namespace opensu::host {

class Backlight {
  public:
    virtual ~Backlight() = default;

    /// The brightness now, 0 to 100, or nothing when it cannot be read.
    [[nodiscard]] virtual std::optional<int> percent() = 0;
    /// Sets the brightness (clamped to 1..100 so the screen never goes dark). Empty when done,
    /// else why it was refused.
    virtual std::string setPercent(int percent) = 0;
};

/// The first backlight device under `root`, or null when there is none.
[[nodiscard]] std::unique_ptr<Backlight>
makeLogindBacklight(Runner run, const std::filesystem::path& root = "/sys/class/backlight");

/// Wraps `inner` so reads pass and changes are held in memory, never written; for hidden runs.
[[nodiscard]] std::unique_ptr<Backlight> makeReadOnlyBacklight(std::unique_ptr<Backlight> inner);

} // namespace opensu::host
