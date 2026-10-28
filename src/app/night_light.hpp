// night_light — the one applier of the night light: what the player chose in Settings › Appearance
// (the desktop's own night light, off, or on at the desktop's night temperature) as a colour
// temperature for now, looked at again as the schedule moves and handed to the platform's output
// when it changes. The output is the Gamescope fork's GAMESCOPE_COLOR_NIGHT_MODE root atom when
// openSU is the login session on its top-level Gamescope (`night_light_gamescope.cpp`); there is
// none on a desktop, whose compositor already warms openSU's nested Gamescope, so nothing is
// warmed twice, and none on other systems (`night_light_none.cpp`). The desktop's own night light
// is KDE Plasma's, read from kwinrc and knighttimed on Linux (`desktop_night_kde.cpp`); elsewhere
// there is none and the desktop choice is neutral.
#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>

#include "settings/night_light_choice.hpp"

namespace opensu::app {

/// The night light the desktop is set to.
class DesktopNight {
  public:
    DesktopNight() = default;
    DesktopNight(const DesktopNight&) = delete;
    DesktopNight& operator=(const DesktopNight&) = delete;
    DesktopNight(DesktopNight&&) = delete;
    DesktopNight& operator=(DesktopNight&&) = delete;
    virtual ~DesktopNight() = default;

    /// The temperature the desktop shows at `now`.
    [[nodiscard]] virtual int kelvinAt(std::chrono::system_clock::time_point now) = 0;
    /// The desktop's night temperature.
    [[nodiscard]] virtual int nightKelvin() const = 0;
};

/// The desktop's night light, configured under `xdgConfigHome`; null where the platform has none.
[[nodiscard]] std::unique_ptr<DesktopNight>
openDesktopNight(const std::filesystem::path& xdgConfigHome);

/// Where the night light is shown: the compositor in front of everything.
class NightLightOutput {
  public:
    NightLightOutput() = default;
    NightLightOutput(const NightLightOutput&) = delete;
    NightLightOutput& operator=(const NightLightOutput&) = delete;
    NightLightOutput(NightLightOutput&&) = delete;
    NightLightOutput& operator=(NightLightOutput&&) = delete;
    virtual ~NightLightOutput() = default;

    /// Shows the screen at `kelvin`; 6500 K and above is neutral.
    virtual void show(int kelvin) = 0;
};

/// Whether this platform can warm the screen at all; Settings offers the night light only then.
[[nodiscard]] bool nightLightSupported();

/// The platform's output when openSU is the login session inside its Gamescope, else null.
/// Throws std::runtime_error when it has one that cannot be opened.
[[nodiscard]] std::unique_ptr<NightLightOutput> openNightLightOutput(bool loginSession,
                                                                     bool insideGamescope);

class NightLight {
  public:
    /// A null `desktop` reads as neutral.
    NightLight(std::unique_ptr<DesktopNight> desktop, std::unique_ptr<NightLightOutput> output);

    /// Whether openSU shows the night light itself: in its login session only.
    [[nodiscard]] bool active() const noexcept {
        return output_ != nullptr;
    }

    /// The temperature `choice` asks for at `now`.
    [[nodiscard]] int kelvinFor(settings::NightLightChoice choice,
                                std::chrono::system_clock::time_point now);

    /// The temperature of the last look; 6500 K before the first.
    [[nodiscard]] int kelvin() const noexcept {
        return kelvin_;
    }

    /// Looks at `choice` for `now` when it changed or the last look is ten seconds old, and shows
    /// the temperature when it changed and there is an output.
    void service(settings::NightLightChoice choice, std::chrono::system_clock::time_point now);

  private:
    std::unique_ptr<DesktopNight> desktop_;
    std::unique_ptr<NightLightOutput> output_;
    std::optional<settings::NightLightChoice> choice_;
    std::optional<std::chrono::system_clock::time_point> looked_;
    int kelvin_{6500};
    std::optional<int> shown_;
};

} // namespace opensu::app
