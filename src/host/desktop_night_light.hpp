// desktop_night_light — the night light KDE Plasma's desktop is set to, read where KWin is not
// running (openSU's login session): KWin's own settings from kwinrc's [NightColor] group (on or
// off, constant or day and night, the two temperatures) and the day and night schedule from
// knighttimed (org.kde.NightTime on the session bus, which KWin follows too and which keeps the
// location and the transition times), with KWin's rule for the temperature at a moment.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "runner.hpp"

namespace opensu::host {

/// KWin's night light settings (`nightlightsettings.kcfg`).
struct NightColorSettings {
    /// Active: the night light is on.
    bool active{false};
    /// Mode=Constant: the night temperature all day; any other mode follows the schedule.
    bool constant{false};
    int dayKelvin{6500};
    int nightKelvin{4500};

    bool operator==(const NightColorSettings&) const = default;
};

/// One day of knighttimed's schedule.
struct NightTimeDay {
    std::chrono::system_clock::time_point morningStart;
    std::chrono::system_clock::time_point morningEnd;
    std::chrono::system_clock::time_point eveningStart;
    std::chrono::system_clock::time_point eveningEnd;
};

/// The [NightColor] group of `kwinrc`'s text, KWin's defaults for what it leaves out.
[[nodiscard]] NightColorSettings parseNightColor(std::string_view kwinrc);

/// The days of `busctl --json=short` output of org.kde.NightTime.Manager.Subscribe; empty when it
/// holds no dynamic schedule.
[[nodiscard]] std::vector<NightTimeDay> parseNightTimeSchedule(std::string_view json);

/// KWin's temperature at `now`: the day temperature when off, the night one when constant, else
/// the day or night one by the schedule, moving linearly across a transition. Nothing when the
/// schedule has no transition before and after `now`.
[[nodiscard]] std::optional<int> nightColorKelvin(const NightColorSettings& settings,
                                                  std::span<const NightTimeDay> schedule,
                                                  std::chrono::system_clock::time_point now);

class DesktopNightLight {
  public:
    /// `kwinrc` is KWin's configuration file; `run` reaches knighttimed through busctl.
    DesktopNightLight(Runner run, std::filesystem::path kwinrc);

    /// KWin's settings, read from the file now.
    [[nodiscard]] NightColorSettings settings() const;

    /// The temperature the desktop shows at `now`. Asks knighttimed for the schedule when the one
    /// held does not cover `now`, at most every ten minutes; the day temperature while there is
    /// none.
    [[nodiscard]] int kelvinAt(std::chrono::system_clock::time_point now);

  private:
    Runner run_;
    std::filesystem::path kwinrc_;
    std::vector<NightTimeDay> schedule_;
    std::optional<std::chrono::system_clock::time_point> lastAsked_;
};

} // namespace opensu::host
