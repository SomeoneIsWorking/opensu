#include "setting_schema.hpp"

#include <algorithm>
#include <charconv>
#include <optional>

#include "lucent/log.h"

namespace opensu::settings {
namespace {

std::optional<int> numberIn(std::string_view value) {
    int number = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (error != std::errc{} || end != value.data() + value.size()) {
        return std::nullopt;
    }
    return number;
}

} // namespace

Setting choiceSetting(const ChoiceKey& choices, std::string help, std::string section,
                      SettingType type) {
    Setting setting;
    setting.key = std::string{choices.key};
    setting.label = std::string{choices.label};
    setting.help = std::move(help);
    setting.section = std::move(section);
    setting.type = type;
    for (const ChoiceText& each : choices.choices) {
        setting.choices.push_back(SettingChoice{std::string{each.key}, std::string{each.label}});
    }
    setting.fallback = std::string{choices.defaultKey()};
    return setting;
}

std::string Setting::labelOf(std::string_view value) const {
    const auto found = std::ranges::find(choices, value, &SettingChoice::key);
    if (found != choices.end()) {
        return found->label;
    }
    if (value == fallback && !fallbackLabel.empty()) {
        return fallbackLabel;
    }
    if (type == SettingType::Slider) {
        return std::string{value} + unit;
    }
    return std::string{value};
}

bool Setting::accepts(std::string_view value) const {
    switch (type) {
    case SettingType::Choice:
    case SettingType::Toggle:
    case SettingType::Preset:
        return std::ranges::find(choices, value, &SettingChoice::key) != choices.end();
    case SettingType::Slider: {
        const std::optional<int> number = numberIn(value);
        return number && *number >= range.low && *number <= range.high &&
               (*number - range.low) % std::max(range.step, 1) == 0;
    }
    case SettingType::Path:
        return true;
    }
    return false;
}

Schema& Schema::add(Setting setting) {
    settings_.push_back(std::move(setting));
    return *this;
}

Schema& Schema::addFamily(std::string prefix) {
    families_.push_back(std::move(prefix));
    return *this;
}

Schema& Schema::addOpen(std::string key) {
    open_.push_back(std::move(key));
    return *this;
}

const Setting* Schema::find(std::string_view key) const {
    const auto found = std::ranges::find(settings_, key, &Setting::key);
    return found == settings_.end() ? nullptr : &*found;
}

bool Schema::owns(std::string_view key) const {
    return find(key) != nullptr || std::ranges::find(open_, key) != open_.end() ||
           std::ranges::any_of(families_, [key](const std::string& prefix) {
               return key.starts_with(prefix) && key.size() > prefix.size();
           });
}

bool Schema::accepts(std::string_view key, std::string_view value) const {
    if (const Setting* setting = find(key)) {
        return setting->accepts(value);
    }
    return owns(key) && !value.empty();
}

std::size_t Schema::clear(SettingLayers& layers, const Layer& layer) const {
    const auto kept = layers.layers().find(layer);
    if (kept == layers.layers().end()) {
        return 0;
    }
    std::vector<std::string> owned;
    for (const auto& [key, value] : kept->second) {
        if (owns(key)) {
            owned.push_back(key);
        }
    }
    for (const std::string& key : owned) {
        layers.clear(layer, key);
    }
    return owned.size();
}

std::size_t Schema::validate(SettingLayers& layers, std::string_view where) const {
    std::vector<std::pair<Layer, std::string>> dropped;
    for (const auto& [layer, values] : layers.layers()) {
        for (const auto& [key, value] : values) {
            if (!accepts(key, value)) {
                lucent::warn("settings", "{}: {} = {} is no setting's value; left out", where, key,
                             value);
                dropped.emplace_back(layer, key);
            }
        }
    }
    for (const auto& [layer, key] : dropped) {
        layers.clear(layer, key);
    }
    return dropped.size();
}

} // namespace opensu::settings
