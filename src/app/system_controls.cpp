#include "system_controls.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

#include "interface_size_setting.hpp"
#include "screen_orientation.hpp"
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
constexpr const char* screenId = "system.screen";

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

} // namespace

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
    case SystemControl::Screen:
        return screen_orientation::turnable();
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
    case SystemControl::Screen:
        return {ui::makeChoice(screenId, "Screen", "Which way the interface stands",
                               ui::Options{{"Landscape", "Portrait"}, values.portrait ? 1U : 0U})};
    case SystemControl::StickLights:
        return lights_.rows();
    }
    return {};
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
    } else if (id == screenId) {
        values.portrait = !values.portrait;
        preferences_.save();
        hooks_.applyLayout();
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

void SystemControls::refuse(const std::string& reason) const {
    if (!reason.empty()) {
        hooks_.refused(reason);
    }
}

} // namespace opensu::app
