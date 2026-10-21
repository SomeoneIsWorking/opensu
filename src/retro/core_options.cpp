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

} // namespace

void CoreOptions::define(const std::string& key, const std::string& fallback,
                         const std::vector<std::string>& offered) {
    const auto chosen = chosen_.find(key);
    if (chosen == chosen_.end()) {
        values_.insert_or_assign(key, fallback);
        return;
    }
    if (std::ranges::find(offered, chosen->second) == offered.end()) {
        lucent::warn("retro", "core option {} has no value {}; keeping {}", key, chosen->second,
                     fallback);
        values_.insert_or_assign(key, fallback);
        return;
    }
    lucent::info("retro", "core option {} = {}", key, chosen->second);
    values_.insert_or_assign(key, chosen->second);
}

void CoreOptions::defineVariables(const retro_variable* variables) {
    values_.clear();
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
        define(variable->key, offered.front(), offered);
    }
}

void CoreOptions::defineDefinitions(const retro_core_option_definition* definitions) {
    values_.clear();
    for (const retro_core_option_definition* definition = definitions;
         definition != nullptr && definition->key != nullptr; ++definition) {
        define(definition->key, defaultOf(definition->default_value, definition->values),
               offeredBy(definition->values));
    }
}

void CoreOptions::defineV2(const retro_core_options_v2& options) {
    values_.clear();
    for (const retro_core_option_v2_definition* definition = options.definitions;
         definition != nullptr && definition->key != nullptr; ++definition) {
        define(definition->key, defaultOf(definition->default_value, definition->values),
               offeredBy(definition->values));
    }
}

const char* CoreOptions::value(std::string_view key) const {
    const auto found = values_.find(key);
    return found != values_.end() ? found->second.c_str() : nullptr;
}

} // namespace opensu::retro
