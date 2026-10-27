#include "core_options.hpp"

#include <algorithm>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

/// The default a definition names, else its first value; empty when it has none.
std::string defaultOf(const char* named, const retro_core_option_value* values) {
    if (named != nullptr) {
        return named;
    }
    if (values[0].value != nullptr) {
        return values[0].value;
    }
    return {};
}

/// The values a definition offers.
std::vector<std::string> offeredBy(const retro_core_option_value* values) {
    std::vector<std::string> offered;
    for (const retro_core_option_value* value = values; value->value != nullptr; ++value) {
        offered.emplace_back(value->value);
    }
    return offered;
}

/// The label of each value a definition offers; the value where it names none.
std::vector<std::string> labelsOf(const retro_core_option_value* values) {
    std::vector<std::string> labels;
    for (const retro_core_option_value* value = values; value->value != nullptr; ++value) {
        labels.emplace_back(value->label != nullptr && value->label[0] != '\0' ? value->label
                                                                               : value->value);
    }
    return labels;
}

std::string textOf(const char* text) {
    return text != nullptr ? text : "";
}

std::string labelOf(const retro_core_option_v2_definition& definition) {
    if (definition.desc != nullptr && definition.desc[0] != '\0') {
        return definition.desc;
    }
    if (definition.desc_categorized != nullptr && definition.desc_categorized[0] != '\0') {
        return definition.desc_categorized;
    }
    return definition.key;
}

} // namespace

void CoreOptions::define(const std::string& key, Defined defined) {
    const auto chosen = chosen_.find(key);
    const bool keep = chosen != chosen_.end() &&
                      std::ranges::find(defined.offered, chosen->second) != defined.offered.end();
    if (chosen != chosen_.end() && !keep) {
        lucent::warn("retro", "core option {} has no value {}; keeping {}", key, chosen->second,
                     defined.fallback);
    } else if (keep) {
        lucent::info("retro", "core option {} = {}", key, chosen->second);
    }
    if (defined.labels.size() != defined.offered.size()) {
        defined.labels = defined.offered;
    }
    if (defined.label.empty()) {
        defined.label = key;
    }
    std::string current = keep ? chosen->second : defined.fallback;
    options_.push_back(Option{.key = key,
                              .label = std::move(defined.label),
                              .info = std::move(defined.info),
                              .values = std::move(defined.offered),
                              .valueLabels = std::move(defined.labels),
                              .fallback = std::move(defined.fallback),
                              .current = std::move(current)});
}

void CoreOptions::defineVariables(const retro_variable* variables) {
    options_.clear();
    for (const retro_variable* variable = variables;
         variable != nullptr && variable->key != nullptr; ++variable) {
        const std::string_view text = variable->value != nullptr ? variable->value : "";
        const std::size_t separator = text.find("; ");
        if (separator == std::string_view::npos) {
            lucent::warn("retro", "core option {} has no values: {}", variable->key, text);
            continue;
        }
        const std::string_view choices = text.substr(separator + 2);
        std::vector<std::string> offered;
        for (std::size_t at = 0; at <= choices.size();) {
            const std::size_t bar = std::min(choices.find('|', at), choices.size());
            offered.emplace_back(choices.substr(at, bar - at));
            at = bar + 1;
        }
        std::string fallback = offered.front();
        define(variable->key, Defined{.label = std::string{text.substr(0, separator)},
                                      .info = {},
                                      .fallback = std::move(fallback),
                                      .offered = std::move(offered),
                                      .labels = {}});
    }
}

void CoreOptions::defineDefinitions(const retro_core_option_definition* definitions) {
    options_.clear();
    for (const retro_core_option_definition* definition = definitions;
         definition != nullptr && definition->key != nullptr; ++definition) {
        define(definition->key,
               Defined{.label = textOf(definition->desc),
                       .info = textOf(definition->info),
                       .fallback = defaultOf(definition->default_value, definition->values),
                       .offered = offeredBy(definition->values),
                       .labels = labelsOf(definition->values)});
    }
}

void CoreOptions::defineV2(const retro_core_options_v2& options) {
    options_.clear();
    for (const retro_core_option_v2_definition* definition = options.definitions;
         definition != nullptr && definition->key != nullptr; ++definition) {
        define(definition->key,
               Defined{.label = labelOf(*definition),
                       .info = textOf(definition->info != nullptr ? definition->info
                                                                  : definition->info_categorized),
                       .fallback = defaultOf(definition->default_value, definition->values),
                       .offered = offeredBy(definition->values),
                       .labels = labelsOf(definition->values)});
    }
}

const char* CoreOptions::value(std::string_view key) const {
    for (const Option& option : options_) {
        if (option.key == key) {
            return option.current.c_str();
        }
    }
    return nullptr;
}

bool CoreOptions::set(std::string_view key, std::string_view value) {
    for (Option& option : options_) {
        if (option.key != key) {
            continue;
        }
        if (std::ranges::find(option.values, value) == option.values.end() ||
            option.current == value) {
            return false;
        }
        option.current = value;
        updated_ = true;
        lucent::info("retro", "core option {} = {}", key, value);
        return true;
    }
    return false;
}

} // namespace opensu::retro
