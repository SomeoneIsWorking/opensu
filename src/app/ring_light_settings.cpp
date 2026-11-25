#include "ring_light_settings.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace opensu::app {
namespace {

constexpr const char* colourId = "lights.colour";
constexpr const char* brightnessId = "lights.brightness";

struct Named {
    std::string_view name;
    device::RingColour colour;
};

constexpr std::array<Named, 9> palette{{
    {"White", {255, 255, 255}},
    {"Red", {255, 0, 0}},
    {"Orange", {255, 96, 0}},
    {"Yellow", {255, 200, 0}},
    {"Green", {0, 255, 0}},
    {"Cyan", {0, 255, 255}},
    {"Blue", {0, 0, 255}},
    {"Purple", {160, 0, 255}},
    {"Pink", {255, 0, 160}},
}};

constexpr std::string_view off{"Off"};
constexpr std::string_view leave{"As the system set them"};

/// The choices in order: leave, off, then the palette; `settings` spells leave as empty.
std::vector<std::string> choices() {
    std::vector<std::string> labels{std::string{leave}, std::string{off}};
    for (const Named& each : palette) {
        labels.emplace_back(each.name);
    }
    return labels;
}

std::size_t chosenOf(const std::string& saved) {
    if (saved.empty()) {
        return 0;
    }
    if (saved == off) {
        return 1;
    }
    const auto at = std::ranges::find(palette, saved, &Named::name);
    return at == palette.end() ? 0 : 2 + static_cast<std::size_t>(at - palette.begin());
}

} // namespace

RingLightSettings::RingLightSettings(Preferences& preferences,
                                     std::optional<device::RingLights> lights,
                                     std::function<void(const std::string& why)> refused)
    : preferences_{preferences}, lights_{std::move(lights)}, refused_{std::move(refused)} {
    apply();
}

std::vector<ui::SettingsRow> RingLightSettings::rows() const {
    std::vector<ui::SettingsRow> rows;
    if (!lights_) {
        return rows;
    }
    const settings::Settings& values = preferences_.values();
    const std::size_t chosen = chosenOf(values.ringLightColour);
    rows.push_back(ui::makeChoice(colourId, "Stick lights", "The rings around the sticks",
                                  ui::Options{.labels = choices(), .chosen = chosen}));
    if (chosen >= 2) {
        rows.push_back(ui::makeSlider(brightnessId, "Stick light brightness", {},
                                      ui::SliderRange{values.ringLightBrightness, 5, 100, 5}));
    }
    return rows;
}

bool RingLightSettings::owns(const std::string& id) {
    return id == colourId || id == brightnessId;
}

void RingLightSettings::run(const std::string& id, int step) {
    if (id != colourId) {
        return;
    }
    const auto count = static_cast<int>(2 + palette.size());
    const int from = static_cast<int>(chosenOf(preferences_.values().ringLightColour));
    const auto to = static_cast<std::size_t>(((from + (step == 0 ? 1 : step)) % count + count) %
                                             count);
    preferences_.values().ringLightColour = to == 0   ? std::string{}
                                            : to == 1 ? std::string{off}
                                                      : std::string{palette[to - 2].name};
    preferences_.save();
    apply();
}

void RingLightSettings::setLevel(const std::string& id, int level) {
    if (id != brightnessId) {
        return;
    }
    preferences_.values().ringLightBrightness = std::clamp(level, 5, 100);
    preferences_.save();
    apply();
}

void RingLightSettings::apply() {
    const settings::Settings& values = preferences_.values();
    const std::size_t chosen = chosenOf(values.ringLightColour);
    if (!lights_ || chosen == 0) {
        return;
    }
    const std::string why = chosen == 1 ? lights_->set({}, 0)
                                        : lights_->set(palette[chosen - 2].colour,
                                                       values.ringLightBrightness);
    if (!why.empty()) {
        refused_(why);
    }
}

} // namespace opensu::app
