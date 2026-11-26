// system_controls — the machine's own controls as Settings rows: the volume and mute through
// `VolumeControl`, the brightness through `BrightnessControl`, the power profile through
// `host::PowerProfiles`, the interface size and night light through `Preferences`, the stick
// lights through `RingLightSettings`. Settings shows each on its page; a control the machine cannot
// change is not offered.
#pragma once

#include <array>
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
    StickLights,
};

inline constexpr std::array allSystemControls{
    SystemControl::Volume,       SystemControl::Mute,          SystemControl::Brightness,
    SystemControl::PowerProfile, SystemControl::InterfaceSize, SystemControl::NightLight,
    SystemControl::StickLights};

/// What the panel shows until the player chooses.
inline constexpr std::array defaultSystemControls{
    SystemControl::Volume, SystemControl::Mute, SystemControl::Brightness, SystemControl::PowerProfile,
    SystemControl::InterfaceSize};

/// How the settings file spells it ("volume", "powerProfile").
[[nodiscard]] std::string_view key(SystemControl setting) noexcept;
/// The setting a spelling names, or nothing.
[[nodiscard]] std::optional<SystemControl> systemControlOf(std::string_view spelling) noexcept;
/// How the player reads it ("Power profile").
[[nodiscard]] std::string_view label(SystemControl setting) noexcept;

class SystemControls final : public ui::RowActions {
  public:
    /// Settings › Quick settings.
    static constexpr std::string_view categoryId{"quickSettings"};

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
        /// Gives the screen the interface size the preferences now hold.
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
    /// The settings the player picked, in order, whether offered or not.
    [[nodiscard]] std::vector<SystemControl> chosen() const;
    /// The rows of the picked settings that are offered, in order.
    [[nodiscard]] std::vector<ui::SettingsRow> shown() const;
    /// Settings › Quick settings: each offered setting's place in the order, or not shown.
    [[nodiscard]] ui::SettingsCategory page() const;

    /// Whether row `id` is one of these settings' or the order's.
    [[nodiscard]] static bool owns(const std::string& id);
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;

  private:
    /// Moves `setting` to place `to` of the order (0 first), or out of it when `to` is past
    /// the end.
    void place(SystemControl setting, std::size_t to);
    void keep(const std::vector<SystemControl>& order);
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
