#include "core_option_settings.hpp"

#include <set>

namespace opensu::settings {

void declareCoreOptions(Schema& schema, const std::vector<CoreOptionDefinition>& options) {
    for (const CoreOptionDefinition& option : options) {
        Setting setting;
        setting.key = std::string{coreOptionPrefix} + option.key;
        setting.label = option.label;
        setting.help = option.help;
        setting.section = std::string{coreOptionSection};
        setting.choices = option.values;
        setting.fallback = option.fallback;
        setting.layers = LayerKinds{Layer::Kind::Emulator, Layer::Kind::Game};
        schema.add(std::move(setting));
    }
}

void declareCoreOptionFamily(Schema& schema) {
    schema.addFamily(std::string{coreOptionPrefix});
}

std::map<std::string, std::string> resolveCoreOptions(const SettingLayers& layers,
                                                      const LayerKeys& keys) {
    std::set<std::string, std::less<>> kept;
    for (const Layer& layer : layersOf(keys)) {
        const auto values = layers.layers().find(layer);
        if (values == layers.layers().end()) {
            continue;
        }
        for (const auto& [key, value] : values->second) {
            if (key.starts_with(coreOptionPrefix) && key.size() > coreOptionPrefix.size()) {
                kept.insert(key);
            }
        }
    }
    std::map<std::string, std::string> resolved;
    for (const std::string& key : kept) {
        resolved.emplace(key.substr(coreOptionPrefix.size()), layers.resolve(keys, key, {}).value);
    }
    return resolved;
}

} // namespace opensu::settings
