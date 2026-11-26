// system_controls — the machine's own controls as Settings rows: the volume and mute through
// `VolumeControl`, the brightness through `BrightnessControl`, the power profile through
// `host::PowerProfiles`, the interface size, night light and the screen's turn through
// `Preferences`, the stick lights through `RingLightSettings`. Settings shows each on its page; a
// control the machine cannot change is not offered.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "brightness_control.hpp"
#include "host/power_profiles.hpp"
#include "preferences.hpp"
#include "ring_light_settings.hpp"
#include "ui/row_widgets.hpp"
#include "ui/setting_row.hpp"
#include "volume_control.hpp"

namespace opensu::app {

enum class SystemControl : std::uint8_t {
    Volume,
    Mute,
    Brightness,
    PowerProfile,
    InterfaceSize,
    NightLight,
    /// Landscape or portrait, where the screen can turn.
    Screen,
    StickLights,
};

class SystemControls final : public ui::RowActions {
  public:
    struct Services {
        VolumeControl& volume;
        BrightnessControl& brightness;
        /// Null without PowerStation; no power profile is offered.
        host::PowerProfiles* powerProfiles;
        Preferences& preferences;
        RingLightSettings& lights;
        /// The platform can warm the screen (`nightLightSupported`).
        bool nightLight;
        /// openSU is the login session, where its own night light shows.
        bool loginSession;
    };

    struct Hooks {
        /// Gives the screen the interface size and turn the preferences now hold.
        std::function<void()> applyLayout;
        /// Tells the player why a change was refused.
        std::function<void(const std::string& text)> refused;
    };

    SystemControls(Services services, Hooks hooks);

    /// Reads what is slow to read: the brightness and the power profile. As a panel opens.
    void read();
    /// Whether this machine can change `setting` now.
    [[nodiscard]] bool offered(SystemControl setting) const;
    /// `setting`'s rows; none when it is not offered, but Volume says why it is missing.
    [[nodiscard]] std::vector<ui::SettingsRow> rows(SystemControl setting) const;
    /// Whether row `id` is one of these controls'.
    [[nodiscard]] static bool owns(const std::string& id);
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;

  private:
    void refuse(const std::string& reason) const;

    VolumeControl& volume_;
    BrightnessControl& brightness_;
    host::PowerProfiles* powerProfiles_;
    Preferences& preferences_;
    RingLightSettings& lights_;
    bool nightLight_;
    bool loginSession_;
    Hooks hooks_;
    std::optional<int> brightnessLevel_;
    std::optional<host::PowerProfileState> powerProfile_;
};

} // namespace opensu::app
