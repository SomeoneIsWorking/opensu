#include "system_controls.hpp"

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

constexpr std::string_view prefix{"system."};
constexpr const char* volumeId = "system.volume";
constexpr const char* volumeMissingId = "system.volumeMissing";
constexpr const char* muteId = "system.mute";
constexpr const char* brightnessId = "system.brightness";
constexpr const char* powerProfileId = "system.powerProfile";
constexpr const char* scaleId = "system.scale";
constexpr const char* nightLightId = "system.nightLight";
constexpr std::string_view placePrefix{"system.place."};
constexpr const char* restoreId = "system.restore";
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

std::string placeId(SystemControl setting) {
    return std::string{placePrefix} + std::string{key(setting)};
}

} // namespace

std::string_view key(SystemControl setting) noexcept {
    switch (setting) {
    case SystemControl::Volume:
        return "volume";
    case SystemControl::Mute:
        return "mute";
    case SystemControl::Brightness:
        return "brightness";
    case SystemControl::PowerProfile:
        return "powerProfile";
    case SystemControl::InterfaceSize:
        return "interfaceSize";
    case SystemControl::NightLight:
        return "nightLight";
    case SystemControl::StickLights:
        return "stickLights";
    }
    return {};
}

std::optional<SystemControl> systemControlOf(std::string_view spelling) noexcept {
    const auto found = std::ranges::find_if(allSystemControls, [spelling](SystemControl setting) {
        return key(setting) == spelling;
    });
    return found == allSystemControls.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(SystemControl setting) noexcept {
    switch (setting) {
    case SystemControl::Volume:
        return "Volume";
    case SystemControl::Mute:
        return "Mute";
    case SystemControl::Brightness:
        return "Brightness";
    case SystemControl::PowerProfile:
        return "Power profile";
    case SystemControl::InterfaceSize:
        return "Interface size";
    case SystemControl::NightLight:
        return "Night light";
    case SystemControl::StickLights:
        return "Stick lights";
    }
    return {};
}

SystemControls::SystemControls(Services services, Hooks hooks)
    : volume_{services.volume}, brightness_{services.brightness},
      powerProfiles_{services.powerProfiles}, preferences_{services.preferences},
      lights_{services.lights}, nightLight_{services.nightLight},
      loginSession_{services.loginSession}, hooks_{std::move(hooks)} {
}

void SystemControls::read() {
    brightnessLevel_ = brightness_.percent();
    powerProfile_ = powerProfiles_ != nullptr ? powerProfiles_->read() : std::nullopt;
}

bool SystemControls::offered(SystemControl setting) const {
    switch (setting) {
    case SystemControl::Volume:
        // Without a mixer its row says why.
        return true;
    case SystemControl::Mute:
        return volume_.system().available();
    case SystemControl::Brightness:
        return brightnessLevel_.has_value();
    case SystemControl::PowerProfile:
        return powerProfile_ &&
               std::ranges::find(powerProfile_->profiles, powerProfile_->profile) !=
                   powerProfile_->profiles.end();
    case SystemControl::InterfaceSize:
        return true;
    case SystemControl::NightLight:
        return nightLight_;
    case SystemControl::StickLights:
        return !lights_.rows().empty();
    }
    return false;
}

std::vector<SettingsRow> SystemControls::rows(SystemControl setting) const {
    if (!offered(setting)) {
        return {};
    }
    const settings::Settings& values = preferences_.values();
    switch (setting) {
    case SystemControl::Volume: {
        const audio::SystemVolume& volume = volume_.system();
        if (!volume.available()) {
            return {ui::makeRow(volumeMissingId, RowKind::Info, "Volume", volume.unavailable(),
                                "Not available")};
        }
        const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
        return {ui::makeSlider(volumeId, "Volume", {},
                               ui::SliderRange{state.percent, 0, 100, audio::volumeStep})};
    }
    case SystemControl::Mute:
        return {ui::makeToggle(muteId, "Mute", {},
                               volume_.system().state().value_or(audio::VolumeState{}).muted)};
    case SystemControl::Brightness:
        if (!brightnessLevel_) {
            return {};
        }
        return {ui::makeSlider(brightnessId, "Brightness", {},
                               ui::SliderRange{*brightnessLevel_, 1, 100, brightnessStep})};
    case SystemControl::PowerProfile: {
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
    case SystemControl::InterfaceSize:
        return {interfaceSizeRow(scaleId, values)};
    case SystemControl::NightLight:
        return {ui::makeChoice(nightLightId, "Night light",
                               loginSession_
                                   ? "Warms the screen's colours at night"
                                   : "For session mode; here the desktop's own night light applies",
                               ui::optionsOf(settings::allNightLightChoices, values.nightLight,
                                             [](settings::NightLightChoice choice) {
                                                 return std::string{settings::label(choice)};
                                             }))};
    case SystemControl::StickLights:
        return lights_.rows();
    }
    return {};
}

std::vector<SystemControl> SystemControls::chosen() const {
    const std::optional<std::vector<std::string>>& kept = preferences_.values().quickSettings;
    if (!kept) {
        return {defaultSystemControls.begin(), defaultSystemControls.end()};
    }
    std::vector<SystemControl> order;
    for (const std::string& spelling : *kept) {
        const std::optional<SystemControl> setting = systemControlOf(spelling);
        if (setting && std::ranges::find(order, *setting) == order.end()) {
            order.push_back(*setting);
        }
    }
    return order;
}

std::vector<SettingsRow> SystemControls::shown() const {
    std::vector<SettingsRow> all;
    for (const SystemControl setting : chosen()) {
        std::ranges::move(rows(setting), std::back_inserter(all));
    }
    return all;
}

ui::SettingsCategory SystemControls::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Quick settings", {}};
    std::vector<SystemControl> listed;
    std::ranges::copy_if(chosen(), std::back_inserter(listed), [this](SystemControl setting) {
        return offered(setting);
    });
    const std::size_t shownCount = listed.size();
    for (const SystemControl setting : allSystemControls) {
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
    for (const SystemControl setting : defaultSystemControls) {
        defaults += (defaults.empty() ? "" : ", ") + std::string{label(setting)};
    }
    SettingsRow restore =
        ui::makeRow(restoreId, RowKind::Action, "Restore defaults", std::move(defaults), {});
    restore.confirm = "Quick settings shows the settings it shipped with, in their order.";
    page.rows.push_back(std::move(restore));
    return page;
}

bool SystemControls::owns(const std::string& id) {
    return id.starts_with(prefix) || RingLightSettings::owns(id);
}

void SystemControls::run(const std::string& id, int step) {
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
        const std::optional<SystemControl> setting =
            systemControlOf(std::string_view{id}.substr(placePrefix.size()));
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

void SystemControls::setLevel(const std::string& id, int level) {
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

void SystemControls::place(SystemControl setting, std::size_t to) {
    std::vector<SystemControl> order = chosen();
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

void SystemControls::keep(const std::vector<SystemControl>& order) {
    std::vector<std::string> spellings;
    spellings.reserve(order.size());
    for (const SystemControl setting : order) {
        spellings.emplace_back(key(setting));
    }
    preferences_.values().quickSettings = std::move(spellings);
    preferences_.save();
}

void SystemControls::refuse(const std::string& reason) const {
    if (!reason.empty()) {
        hooks_.refused(reason);
    }
}

} // namespace opensu::app
