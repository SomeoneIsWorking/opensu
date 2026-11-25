// ring_light_settings — Devices › Controllers' stick lights where the handheld has them
// (`device::RingLights`): left as the system set them, off, or one of a palette of colours at a
// brightness, saved in `settings::Settings::ringLightColour` and `ringLightBrightness` and set on
// the lights, as the saved choice is at start. Where the kernel's files carry no colour (the ROG
// Ally) the colour and its brightness go through the root helper (`host::RingLightHelper`), the
// brightness folded into the colour, and Rainbow, the controller's own, is offered too; until the
// helper is installed only off is offered, and while an older one is in place no Rainbow, each
// with a row that starts its install (`Hooks::allowColours`). Without the lights it has no rows
// and sets nothing.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "device/ring_lights.hpp"
#include "host/ring_light_helper.hpp"
#include "preferences.hpp"
#include "root_install_controller.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class RingLightSettings {
  public:
    struct Hooks {
        /// Says why setting the lights failed.
        std::function<void(const std::string& why)> refused;
        /// Starts installing the helper (`installWords`).
        std::function<void()> allowColours;
    };

    /// Sets the saved choice on `lights`; `helper`, null where there is none, outlives it.
    RingLightSettings(Preferences& preferences, std::optional<device::RingLights> lights,
                      host::RingLightHelper* helper, Hooks hooks);

    /// The rows: the colour, for a colour its brightness, and the helper's install where it is
    /// needed and missing.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps the colour, or starts the helper's install.
    void run(const std::string& id, int step);
    /// Sets the brightness, in percent.
    void setLevel(const std::string& id, int level);
    /// Sets the saved choice on the lights, as after a change made outside its rows or once the
    /// helper is installed.
    void applySaved();

    /// What installing the helper asks and says.
    [[nodiscard]] static RootInstallWords installWords();

  private:
    /// Whether the helper carries the colour, the kernel's files cannot.
    [[nodiscard]] bool throughHelper() const;
    /// Whether a colour can be set now.
    [[nodiscard]] bool colourful() const;
    /// Whether the controller's rainbow can be set now: the helper in place is this openSU's.
    [[nodiscard]] bool rainbow() const;

    Preferences& preferences_;
    std::optional<device::RingLights> lights_;
    host::RingLightHelper* helper_;
    Hooks hooks_;
};

} // namespace opensu::app
