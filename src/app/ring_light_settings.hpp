// ring_light_settings — Devices › Controllers' stick lights where the handheld has them
// (`device::RingLights`): left as the system set them, off, or one of a palette of colours at a
// brightness, saved in `settings::Settings::ringLightColour` and `ringLightBrightness` and set on
// the lights, as the saved choice is at start. Without the lights it has no rows and sets nothing.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "device/ring_lights.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class RingLightSettings {
  public:
    /// Sets the saved choice on `lights`; `refused` says why a write failed.
    RingLightSettings(Preferences& preferences, std::optional<device::RingLights> lights,
                      std::function<void(const std::string& why)> refused);

    /// The rows: the colour and, for a colour, its brightness.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps the colour.
    void run(const std::string& id, int step);
    /// Sets the brightness, in percent.
    void setLevel(const std::string& id, int level);
    /// Sets the saved choice on the lights, as after a change made outside its rows.
    void applySaved();

  private:

    Preferences& preferences_;
    std::optional<device::RingLights> lights_;
    std::function<void(const std::string& why)> refused_;
};

} // namespace opensu::app
