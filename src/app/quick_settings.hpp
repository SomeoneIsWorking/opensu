// quick_settings — the settings a quick panel can hold and the player's pick of them. Each one's
// row and what the row does are here, through the owner of the thing: the volume and mute through
// `VolumeControl`, the brightness through `BrightnessControl`, the power profile through
// `host::PowerProfiles`, the interface size and night light through `Preferences`, the stick
// lights through `RingLightSettings`. The Quick settings panel and the Guide menu's card show the
// ones the player picked (`settings::Settings::quickSettings`, the default set while unset), and
// Settings shows each on its own page with the same row; Settings › Quick settings orders them. A
// setting the machine cannot change is not offered.
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

enum class QuickSetting : std::uint8_t {
    Volume,
    Mute,
    Brightness,
    PowerProfile,
    InterfaceSize,
    NightLight,
    StickLights,
};

inline constexpr std::array allQuickSettings{
    QuickSetting::Volume,       QuickSetting::Mute,          QuickSetting::Brightness,
    QuickSetting::PowerProfile, QuickSetting::InterfaceSize, QuickSetting::NightLight,
    QuickSetting::StickLights};

/// What the panel shows until the player chooses.
inline constexpr std::array defaultQuickSettings{
    QuickSetting::Volume, QuickSetting::Mute, QuickSetting::Brightness, QuickSetting::PowerProfile,
    QuickSetting::InterfaceSize};

/// How the settings file spells it ("volume", "powerProfile").
[[nodiscard]] std::string_view key(QuickSetting setting) noexcept;
/// The setting a spelling names, or nothing.
[[nodiscard]] std::optional<QuickSetting> quickSettingOf(std::string_view spelling) noexcept;
/// How the player reads it ("Power profile").
[[nodiscard]] std::string_view label(QuickSetting setting) noexcept;

class QuickSettings final : public ui::RowActions {
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

    QuickSettings(Services services, Hooks hooks);

    /// Reads what is slow to read: the brightness and the power profile. As a panel opens.
    void read();
    /// Whether this machine can change `setting` now.
    [[nodiscard]] bool offered(QuickSetting setting) const;
    /// `setting`'s rows; none when it is not offered, but Volume says why it is missing.
    [[nodiscard]] std::vector<ui::SettingsRow> rows(QuickSetting setting) const;
    /// The settings the player picked, in order, whether offered or not.
    [[nodiscard]] std::vector<QuickSetting> chosen() const;
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
    void place(QuickSetting setting, std::size_t to);
    void keep(const std::vector<QuickSetting>& order);
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
