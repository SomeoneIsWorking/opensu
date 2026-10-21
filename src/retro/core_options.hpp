// core_options — the options a core defines (v0 variables, v1 and v2 definitions) and their current
// values: the core's defaults, except where the launch chose one of the option's own values.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <libretro.h>

namespace opensu::retro {

/// Option key to value, as `--option key=value` gives them.
using OptionValues = std::map<std::string, std::string, std::less<>>;

class CoreOptions {
  public:
    /// `chosen` replaces a default when the core offers that value for the key.
    explicit CoreOptions(OptionValues chosen = {}) : chosen_{std::move(chosen)} {
    }

    /// v0: "Description; first|second|...", whose first value is the default.
    void defineVariables(const retro_variable* variables);
    /// v1: definitions ending at a null key.
    void defineDefinitions(const retro_core_option_definition* definitions);
    /// v2: definitions ending at a null key; categories carry no values.
    void defineV2(const retro_core_options_v2& options);

    /// The option's value, or null for a key the core never defined. Valid until the next define.
    [[nodiscard]] const char* value(std::string_view key) const;

    [[nodiscard]] std::size_t size() const {
        return values_.size();
    }

  private:
    /// The value for `key`: the chosen one when `offered` holds it, else `fallback`.
    void define(const std::string& key, const std::string& fallback,
                const std::vector<std::string>& offered);

    OptionValues chosen_;
    /// Each define replaces what the core defined before.
    OptionValues values_;
};

} // namespace opensu::retro
