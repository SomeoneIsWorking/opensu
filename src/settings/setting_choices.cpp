#include "setting_choices.hpp"

#include <algorithm>

#include "lucent/log.h"

namespace opensu::settings {

std::optional<std::size_t> choiceIndex(const ChoiceKey& setting, std::string_view spelling) {
    const auto found = std::ranges::find(setting.choices, spelling, &ChoiceText::key);
    if (found == setting.choices.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - setting.choices.begin());
}

std::string_view choiceLabel(const ChoiceKey& setting, std::string_view spelling) {
    const std::optional<std::size_t> index = choiceIndex(setting, spelling);
    return index ? setting.choices[*index].label : spelling;
}

std::size_t choiceIndexOrDefault(const ChoiceKey& setting, std::string_view spelling,
                                 std::string_view where) {
    if (const std::optional<std::size_t> index = choiceIndex(setting, spelling)) {
        return *index;
    }
    lucent::warn("settings", "{}: {} is not a {} choice; using {}", where, spelling, setting.key,
                 setting.defaultKey());
    return setting.fallback;
}

} // namespace opensu::settings
