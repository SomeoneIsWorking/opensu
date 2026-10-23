// core_options — the options a core defines (v0 variables, v1 and v2 definitions), their values and
// the current one: the core's default, except where the launch chose one of the option's own values
// or the player changed it during play.
#pragma once

#include <cstddef>
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
    /// One option as the shell lists it.
    struct Option {
        std::string key;
        std::string label;
        std::vector<std::string> values;
        std::string current;
    };

    /// `chosen` replaces a default when the core offers that value for the key.
    explicit CoreOptions(OptionValues chosen = {}) : chosen_{std::move(chosen)} {
    }

    /// v0: "Description; first|second|...", whose first value is the default.
    void defineVariables(const retro_variable* variables);
    /// v1: definitions ending at a null key.
    void defineDefinitions(const retro_core_option_definition* definitions);
    /// v2: definitions ending at a null key; categories carry no values.
    void defineV2(const retro_core_options_v2& options);

    /// The option's value, or null for a key the core never defined.
    [[nodiscard]] const char* value(std::string_view key) const;
    /// Every option the core defined, in the order it defined them.
    [[nodiscard]] const std::vector<Option>& options() const noexcept {
        return options_;
    }
    /// Makes `value` the option's current one when the core offers it and it differs; true when it
    /// changed, so the core is told to read its variables again.
    bool set(std::string_view key, std::string_view value);
    /// Whether a variable changed since the last call, clearing it.
    bool takeUpdated() noexcept {
        return std::exchange(updated_, false);
    }

    [[nodiscard]] std::size_t size() const {
        return options_.size();
    }

  private:
    /// Adds an option `key`: the chosen value when `offered` holds it, else `fallback`.
    void define(const std::string& key, const std::string& label, const std::string& fallback,
                const std::vector<std::string>& offered);

    OptionValues chosen_;
    /// Each define replaces what the core defined before.
    std::vector<Option> options_;
    bool updated_{false};
};

} // namespace opensu::retro
