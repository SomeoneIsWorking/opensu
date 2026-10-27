#include "core_option_store.hpp"

#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::app {

std::filesystem::path CoreOptionStore::fileOf(std::string_view core) const {
    return folder_ / (std::string{core} + ".json");
}

std::vector<settings::CoreOptionDefinition> CoreOptionStore::known(std::string_view core) const {
    std::vector<settings::CoreOptionDefinition> options;
    const std::filesystem::path file = fileOf(core);
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return options;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    const nlohmann::json list = nlohmann::json::parse(text, nullptr, false);
    if (!list.is_array()) {
        lucent::warn("settings", "{} is not a list of core options; left out", file.string());
        return options;
    }
    for (const nlohmann::json& entry : list) {
        if (!entry.is_object() || !entry.value("key", nlohmann::json{}).is_string()) {
            lucent::warn("settings", "{}: {} is not a core option; left out", file.string(),
                         entry.dump());
            continue;
        }
        settings::CoreOptionDefinition option{.key = entry.value("key", std::string{}),
                                              .label = entry.value("label", std::string{}),
                                              .help = entry.value("help", std::string{}),
                                              .fallback = entry.value("default", std::string{})};
        for (const nlohmann::json& value : entry.value("values", nlohmann::json::array())) {
            if (value.is_object() && value.value("value", nlohmann::json{}).is_string()) {
                option.values.push_back(settings::SettingChoice{
                    value.value("value", std::string{}), value.value("label", std::string{})});
            }
        }
        if (!option.key.empty() && !option.values.empty()) {
            options.push_back(std::move(option));
        }
    }
    return options;
}

void CoreOptionStore::remember(std::string_view core,
                               const std::vector<settings::CoreOptionDefinition>& options) const {
    nlohmann::json list = nlohmann::json::array();
    for (const settings::CoreOptionDefinition& option : options) {
        nlohmann::json values = nlohmann::json::array();
        for (const settings::SettingChoice& value : option.values) {
            values.push_back(nlohmann::json{{"value", value.key}, {"label", value.label}});
        }
        list.push_back(nlohmann::json{{"key", option.key},
                                      {"label", option.label},
                                      {"help", option.help},
                                      {"default", option.fallback},
                                      {"values", std::move(values)}});
    }
    std::string error;
    if (!fileio::writeWhole(fileOf(core), list.dump(2), error)) {
        lucent::error("settings", "cannot keep {}'s core options: {}", core, error);
    }
}

} // namespace opensu::app
