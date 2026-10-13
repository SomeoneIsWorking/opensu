#include "core_options.hpp"

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

} // namespace

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
        values_.insert_or_assign(variable->key, std::string{choices.substr(0, choices.find('|'))});
    }
}

void CoreOptions::defineDefinitions(const retro_core_option_definition* definitions) {
    values_.clear();
    for (const retro_core_option_definition* definition = definitions;
         definition != nullptr && definition->key != nullptr; ++definition) {
        values_.insert_or_assign(definition->key,
                                 defaultOf(definition->default_value, definition->values));
    }
}

void CoreOptions::defineV2(const retro_core_options_v2& options) {
    values_.clear();
    for (const retro_core_option_v2_definition* definition = options.definitions;
         definition != nullptr && definition->key != nullptr; ++definition) {
        values_.insert_or_assign(definition->key,
                                 defaultOf(definition->default_value, definition->values));
    }
}

const char* CoreOptions::value(std::string_view key) const {
    const auto found = values_.find(key);
    return found != values_.end() ? found->second.c_str() : nullptr;
}

} // namespace opensu::retro
