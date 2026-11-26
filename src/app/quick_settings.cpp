#include "quick_settings.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

#include "interface_size_setting.hpp"
#include "settings/night_light_choice.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;
using ui::SettingsRow;

constexpr std::string_view prefix{"quick."};
constexpr const char* volumeId = "quick.volume";
constexpr const char* volumeMissingId = "quick.volumeMissing";
constexpr const char* muteId = "quick.mute";
constexpr const char* brightnessId = "quick.brightness";
constexpr const char* powerProfileId = "quick.powerProfile";
constexpr const char* scaleId = "quick.scale";
constexpr const char* nightLightId = "quick.nightLight";
constexpr std::string_view placePrefix{"quick.place."};
constexpr const char* restoreId = "quick.restore";
constexpr std::string_view notShown{"Not shown"};

/// How a PowerStation profile reads.
std::string profileLabel(const std::string& profile) {
    if (profile == "quiet") {
        return "Quiet";
    }
    if (profile == "balanced") {
        return "Balanced";
    }
    if (profile == "performance") {
        return "Performance";
    }
    return profile;
}

std::string placeId(QuickSetting setting) {
    return std::string{placePrefix} + std::string{key(setting)};
}

} // namespace

std::string_view key(QuickSetting setting) noexcept {
    switch (setting) {
    case QuickSetting::Volume:
        return "volume";
    case QuickSetting::Mute:
        return "mute";
    case QuickSetting::Brightness:
        return "brightness";
    case QuickSetting::PowerProfile:
        return "powerProfile";
    case QuickSetting::InterfaceSize:
        return "interfaceSize";
    case QuickSetting::NightLight:
        return "nightLight";
    case QuickSetting::StickLights:
        return "stickLights";
    }
    return {};
}

std::optional<QuickSetting> quickSettingOf(std::string_view spelling) noexcept {
    const auto found = std::ranges::find_if(allQuickSettings, [spelling](QuickSetting setting) {
        return key(setting) == spelling;
    });
    return found == allQuickSettings.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(QuickSetting setting) noexcept {
    switch (setting) {
    case QuickSetting::Volume:
        return "Volume";
    case QuickSetting::Mute:
        return "Mute";
    case QuickSetting::Brightness:
        return "Brightness";
    case QuickSetting::PowerProfile:
        return "Power profile";
    case QuickSetting::InterfaceSize:
        return "Interface size";
    case QuickSetting::NightLight:
        return "Night light";
    case QuickSetting::StickLights:
        return "Stick lights";
    }
    return {};
}

QuickSettings::QuickSettings(Services services, Hooks hooks)
    : volume_{services.volume}, brightness_{services.brightness},
      powerProfiles_{services.powerProfiles}, preferences_{services.preferences},
      lights_{services.lights}, nightLight_{services.nightLight},
      loginSession_{services.loginSession}, hooks_{std::move(hooks)} {
}

void QuickSettings::read() {
    brightnessLevel_ = brightness_.percent();
    powerProfile_ = powerProfiles_ != nullptr ? powerProfiles_->read() : std::nullopt;
}

bool QuickSettings::offered(QuickSetting setting) const {
    switch (setting) {
    case QuickSetting::Volume:
        // Without a mixer its row says why.
        return true;
    case QuickSetting::Mute:
        return volume_.system().available();
    case QuickSetting::Brightness:
        return brightnessLevel_.has_value();
    case QuickSetting::PowerProfile:
        return powerProfile_ &&
               std::ranges::find(powerProfile_->profiles, powerProfile_->profile) !=
                   powerProfile_->profiles.end();
    case QuickSetting::InterfaceSize:
        return true;
    case QuickSetting::NightLight:
        return nightLight_;
    case QuickSetting::StickLights:
        return !lights_.rows().empty();
    }
    return false;
}

std::vector<SettingsRow> QuickSettings::rows(QuickSetting setting) const {
    if (!offered(setting)) {
        return {};
    }
    const settings::Settings& values = preferences_.values();
    switch (setting) {
    case QuickSetting::Volume: {
        const audio::SystemVolume& volume = volume_.system();
        if (!volume.available()) {
            return {ui::makeRow(volumeMissingId, RowKind::Info, "Volume", volume.unavailable(),
                                "Not available")};
        }
        const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
        return {ui::makeSlider(volumeId, "Volume", {},
                               ui::SliderRange{state.percent, 0, 100, audio::volumeStep})};
    }
    case QuickSetting::Mute:
        return {ui::makeToggle(muteId, "Mute", {},
                               volume_.system().state().value_or(audio::VolumeState{}).muted)};
    case QuickSetting::Brightness:
        if (!brightnessLevel_) {
            return {};
        }
        return {ui::makeSlider(brightnessId, "Brightness", {},
                               ui::SliderRange{*brightnessLevel_, 1, 100, brightnessStep})};
    case QuickSetting::PowerProfile: {
        if (!powerProfile_) {
            return {};
        }
        ui::Options profiles;
        for (const std::string& profile : powerProfile_->profiles) {
            if (profile == powerProfile_->profile) {
                profiles.chosen = profiles.labels.size();
            }
            profiles.labels.push_back(profileLabel(profile));
        }
        return {ui::makeChoice(powerProfileId, "Power profile", {}, std::move(profiles))};
    }
    case QuickSetting::InterfaceSize:
        return {interfaceSizeRow(scaleId, values)};
    case QuickSetting::NightLight:
        return {ui::makeChoice(nightLightId, "Night light",
                               loginSession_
                                   ? "Warms the screen's colours at night"
                                   : "For session mode; here the desktop's own night light applies",
                               ui::optionsOf(settings::allNightLightChoices, values.nightLight,
                                             [](settings::NightLightChoice choice) {
                                                 return std::string{settings::label(choice)};
                                             }))};
    case QuickSetting::StickLights:
        return lights_.rows();
    }
    return {};
}

std::vector<QuickSetting> QuickSettings::chosen() const {
    const std::optional<std::vector<std::string>>& kept = preferences_.values().quickSettings;
    if (!kept) {
        return {defaultQuickSettings.begin(), defaultQuickSettings.end()};
    }
    std::vector<QuickSetting> order;
    for (const std::string& spelling : *kept) {
        const std::optional<QuickSetting> setting = quickSettingOf(spelling);
        if (setting && std::ranges::find(order, *setting) == order.end()) {
            order.push_back(*setting);
        }
    }
    return order;
}

std::vector<SettingsRow> QuickSettings::shown() const {
    std::vector<SettingsRow> all;
    for (const QuickSetting setting : chosen()) {
        std::ranges::move(rows(setting), std::back_inserter(all));
    }
    return all;
}

ui::SettingsCategory QuickSettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Quick settings", {}};
    std::vector<QuickSetting> listed;
    std::ranges::copy_if(chosen(), std::back_inserter(listed), [this](QuickSetting setting) {
        return offered(setting);
    });
    const std::size_t shownCount = listed.size();
    for (const QuickSetting setting : allQuickSettings) {
        if (offered(setting) && std::ranges::find(listed, setting) == listed.end()) {
            listed.push_back(setting);
        }
    }
    page.rows.push_back(ui::makeHeading("Shown in Quick settings, in order"));
    for (std::size_t at = 0; at < listed.size(); ++at) {
        const bool shown = at < shownCount;
        // A setting not shown can go in at any place, after the last too.
        const std::size_t places = shown ? shownCount : shownCount + 1;
        ui::Options options;
        for (std::size_t i = 0; i < places; ++i) {
            options.labels.push_back(std::to_string(i + 1));
        }
        options.labels.emplace_back(notShown);
        options.chosen = shown ? at : places;
        page.rows.push_back(ui::makeChoice(placeId(listed[at]), std::string{label(listed[at])}, {},
                                           std::move(options)));
    }
    std::string defaults;
    for (const QuickSetting setting : defaultQuickSettings) {
        defaults += (defaults.empty() ? "" : ", ") + std::string{label(setting)};
    }
    SettingsRow restore =
        ui::makeRow(restoreId, RowKind::Action, "Restore defaults", std::move(defaults), {});
    restore.confirm = "Quick settings shows the settings it shipped with, in their order.";
    page.rows.push_back(std::move(restore));
    return page;
}

bool QuickSettings::owns(const std::string& id) {
    return id.starts_with(prefix) || RingLightSettings::owns(id);
}

void QuickSettings::run(const std::string& id, int step) {
    settings::Settings& values = preferences_.values();
    if (RingLightSettings::owns(id)) {
        lights_.run(id, step);
    } else if (id == muteId) {
        refuse(volume_.system().toggleMute());
    } else if (id == powerProfileId) {
        if (powerProfiles_ == nullptr || !powerProfile_ || powerProfile_->profiles.empty()) {
            return;
        }
        refuse(powerProfiles_->set(
            ui::stepped(powerProfile_->profiles, powerProfile_->profile, step)));
        powerProfile_ = powerProfiles_->read();
    } else if (id == nightLightId) {
        const std::vector<settings::NightLightChoice> all{settings::allNightLightChoices.begin(),
                                                          settings::allNightLightChoices.end()};
        values.nightLight = ui::stepped(all, values.nightLight, step);
        preferences_.save();
    } else if (id == restoreId) {
        values.quickSettings.reset();
        preferences_.save();
    } else if (id.starts_with(placePrefix)) {
        const std::optional<QuickSetting> setting =
            quickSettingOf(std::string_view{id}.substr(placePrefix.size()));
        if (!setting) {
            return;
        }
        const ui::SettingsCategory order = page();
        const auto row = std::ranges::find(order.rows, id, &SettingsRow::id);
        if (row == order.rows.end() || row->choices.empty()) {
            return;
        }
        const auto last = static_cast<int>(row->choices.size()) - 1;
        const int to = std::clamp(static_cast<int>(row->chosen) + step, 0, last);
        place(*setting, static_cast<std::size_t>(to));
    }
}

void QuickSettings::setLevel(const std::string& id, int level) {
    if (RingLightSettings::owns(id)) {
        lights_.setLevel(id, level);
    } else if (id == volumeId) {
        refuse(volume_.system().setPercent(std::clamp(level, 0, 100)));
    } else if (id == brightnessId) {
        refuse(brightness_.set(level));
        brightnessLevel_ = brightness_.percent();
    } else if (id == scaleId) {
        setInterfaceSize(preferences_, level);
        hooks_.applyLayout();
    }
}

void QuickSettings::place(QuickSetting setting, std::size_t to) {
    std::vector<QuickSetting> order = chosen();
    std::erase(order, setting);
    // The places count only what is offered here; the rest keep their spots for another machine.
    std::vector<std::size_t> offeredAt;
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (offered(order[i])) {
            offeredAt.push_back(i);
        }
    }
    if (to > offeredAt.size()) {
        keep(order);
        return;
    }
    const auto before = to == offeredAt.size()
                            ? order.end()
                            : order.begin() + static_cast<std::ptrdiff_t>(offeredAt[to]);
    order.insert(before, setting);
    keep(order);
}

void QuickSettings::keep(const std::vector<QuickSetting>& order) {
    std::vector<std::string> spellings;
    spellings.reserve(order.size());
    for (const QuickSetting setting : order) {
        spellings.emplace_back(key(setting));
    }
    preferences_.values().quickSettings = std::move(spellings);
    preferences_.save();
}

void QuickSettings::refuse(const std::string& reason) const {
    if (!reason.empty()) {
        hooks_.refused(reason);
    }
}

} // namespace opensu::app
