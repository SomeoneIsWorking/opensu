// core_options — the options a core defines (v0 variables, v1 and v2 definitions) and their current
// values, which are the core's defaults.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include <libretro.h>

namespace opensu::retro {

class CoreOptions {
  public:
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
    /// Each define replaces what the core defined before.
    std::map<std::string, std::string, std::less<>> values_;
};

} // namespace opensu::retro
