#include "ring_light_settings.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace opensu::app {
namespace {

constexpr const char* colourId = "lights.colour";
constexpr const char* brightnessId = "lights.brightness";
constexpr const char* allowId = "lights.allow";

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

/// Leave, off, then the palette when a colour can be set; `settings` spells leave as empty.
std::vector<std::string> choices(bool colourful) {
    std::vector<std::string> labels{std::string{leave}, std::string{off}};
    if (colourful) {
        for (const Named& each : palette) {
            labels.emplace_back(each.name);
        }
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

/// `colour` at `percent`, packed 0xRRGGBB.
std::uint32_t scaled(device::RingColour colour, int percent) {
    const auto channel = [percent](std::uint8_t value) {
        return static_cast<std::uint32_t>(value * std::clamp(percent, 0, 100) / 100);
    };
    return (channel(colour.red) << 16U) | (channel(colour.green) << 8U) | channel(colour.blue);
}

} // namespace

RingLightSettings::RingLightSettings(Preferences& preferences,
                                     std::optional<device::RingLights> lights,
                                     host::RingLightHelper* helper, Hooks hooks)
    : preferences_{preferences}, lights_{std::move(lights)}, helper_{helper},
      hooks_{std::move(hooks)} {
    applySaved();
}

bool RingLightSettings::throughHelper() const {
    return lights_ && !lights_->colours() && helper_ != nullptr;
}

bool RingLightSettings::colourful() const {
    return lights_ && (lights_->colours() || (throughHelper() && helper_->installed()));
}

std::vector<ui::SettingsRow> RingLightSettings::rows() const {
    std::vector<ui::SettingsRow> rows;
    if (!lights_) {
        return rows;
    }
    const settings::Settings& values = preferences_.values();
    const bool colours = colourful();
    std::size_t chosen = chosenOf(values.ringLightColour);
    if (!colours && chosen >= 2) {
        chosen = 0;
    }
    rows.push_back(ui::makeChoice(colourId, "Stick lights", "The rings around the sticks",
                                  ui::Options{.labels = choices(colours), .chosen = chosen}));
    if (chosen >= 2) {
        rows.push_back(ui::makeSlider(brightnessId, "Stick light brightness", {},
                                      ui::SliderRange{values.ringLightBrightness, 5, 100, 5}));
    }
    if (throughHelper() && !helper_->installed()) {
        rows.push_back(ui::makeRow(allowId, ui::RowKind::Action, "Stick light colours",
                                   "This handheld's driver turns them off only; one "
                                   "administrator password lets openSU set a colour",
                                   "Allow"));
    }
    return rows;
}

bool RingLightSettings::owns(const std::string& id) {
    return id == colourId || id == brightnessId || id == allowId;
}

void RingLightSettings::run(const std::string& id, int step) {
    if (id == allowId) {
        hooks_.allowColours();
        return;
    }
    if (id != colourId) {
        return;
    }
    const int count = static_cast<int>(choices(colourful()).size());
    const int from = std::min(static_cast<int>(chosenOf(preferences_.values().ringLightColour)),
                              count - 1);
    const auto to = static_cast<std::size_t>(((from + (step == 0 ? 1 : step)) % count + count) %
                                             count);
    preferences_.values().ringLightColour = to == 0   ? std::string{}
                                            : to == 1 ? std::string{off}
                                                      : std::string{palette[to - 2].name};
    preferences_.save();
    applySaved();
}

void RingLightSettings::setLevel(const std::string& id, int level) {
    if (id != brightnessId) {
        return;
    }
    preferences_.values().ringLightBrightness = std::clamp(level, 5, 100);
    preferences_.save();
    applySaved();
}

void RingLightSettings::applySaved() {
    const settings::Settings& values = preferences_.values();
    const std::size_t chosen = chosenOf(values.ringLightColour);
    if (!lights_ || chosen == 0) {
        return;
    }
    const device::RingColour colour =
        chosen == 1 ? device::RingColour{} : palette[chosen - 2].colour;
    const int percent = chosen == 1 ? 0 : values.ringLightBrightness;
    std::string why;
    if (!throughHelper()) {
        why = lights_->set(colour, percent);
    } else if (helper_->installed()) {
        why = helper_->set(scaled(colour, percent));
    } else if (chosen == 1) {
        // The kernel's files turn the lights off even where they carry no colour.
        why = lights_->set(colour, 0);
    }
    if (!why.empty()) {
        hooks_.refused(why);
    }
}

RootInstallWords RingLightSettings::installWords() {
    return RootInstallWords{
        .cardId = "lights.installing",
        .title = "Stick light colours",
        .explanation =
            std::string{"This handheld's kernel driver cannot set the stick lights' colour, so "
                        "openSU sends the controller's own light command as root.\n\n"
                        "This installs a small helper at "} +
            host::ringLightHelperPath + " and a sudo rule in " + host::ringLightRulePath +
            " that lets you run only that helper without a password. It takes a colour and "
            "writes nothing but the lights.\n\npolkit then asks for the administrator password.",
        .installed = "Stick light colours allowed"};
}

} // namespace opensu::app
