#include "night_light_choice.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(NightLightChoice choice) noexcept {
    switch (choice) {
    case NightLightChoice::Desktop:
        return "desktop";
    case NightLightChoice::Off:
        return "off";
    case NightLightChoice::On:
        return "on";
    }
    return {};
}

std::optional<NightLightChoice> nightLightChoiceOf(std::string_view spelling) noexcept {
    const auto found =
        std::ranges::find_if(allNightLightChoices, [spelling](NightLightChoice choice) {
            return key(choice) == spelling;
        });
    return found == allNightLightChoices.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(NightLightChoice choice) noexcept {
    switch (choice) {
    case NightLightChoice::Desktop:
        return "Same as desktop";
    case NightLightChoice::Off:
        return "Off";
    case NightLightChoice::On:
        return "On";
    }
    return {};
}

} // namespace opensu::settings
