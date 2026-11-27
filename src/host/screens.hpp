// screens — the screens connected to the machine and the one openSU is shown on, through the
// desktop's display settings (KScreen's `kscreen-doctor` on KDE, which reaches KWin over D-Bus from
// inside openSU's Gamescope too). Using a screen turns it on in the mode the desktop last gave it
// and makes it the main one, and turns every other screen off. Where the desktop has no KScreen
// (openSU's own session, another desktop) there are none to choose.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runner.hpp"

namespace opensu::host {

struct Screen {
    /// The connector, as the desktop names it (`eDP-1`, `DP-2`, `HDMI-A-1`).
    std::string id;
    /// As the player reads it: "Built-in screen", or the connection and its connector.
    std::string name;
    /// Its mode, "1920x1080 at 60 Hz"; empty when the desktop gives none.
    std::string mode;
    /// Whether openSU is shown on it: on, and the main screen among those on.
    bool inUse{false};

    bool operator==(const Screen&) const = default;
};

/// The connected screens in `kscreenJson` (`kscreen-doctor -j`), the built-in one first; nothing
/// when it is no such answer.
[[nodiscard]] std::optional<std::vector<Screen>> screensOf(std::string_view kscreenJson);

class Screens {
  public:
    virtual ~Screens() = default;

    /// The connected screens now, or nothing when the desktop's display settings cannot be read.
    [[nodiscard]] virtual std::optional<std::vector<Screen>> read() = 0;
    /// Shows openSU on screen `id` alone; empty when done, else why not.
    virtual std::string use(const std::string& id) = 0;
};

/// The screens through `kscreen-doctor` on the desktop's Wayland socket `desktopWayland`; none
/// when it is empty.
[[nodiscard]] std::unique_ptr<Screens> makeKScreenScreens(Runner run, std::string desktopWayland);

/// Wraps `inner` so reads pass and changes are refused with `reason`; for hidden runs.
[[nodiscard]] std::unique_ptr<Screens> makeReadOnlyScreens(std::unique_ptr<Screens> inner,
                                                           std::string reason);

} // namespace opensu::host
