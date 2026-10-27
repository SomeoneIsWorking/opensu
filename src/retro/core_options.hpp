// core_options — the options a core defines (v0 variables, v1 and v2 definitions), their values and
// the current one.
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
    struct Option {
        std::string key;
        std::string label;
        /// The core's line about it; empty when it gives none.
        std::string info;
        std::vector<std::string> values;
        /// The core's label for each of `values`, the value itself where it gives none.
        std::vector<std::string> valueLabels;
        /// What the core starts it at.
        std::string fallback;
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
    [[nodiscard]] const std::vector<Option>& options() const noexcept {
        return options_;
    }
    bool set(std::string_view key, std::string_view value);
    bool takeUpdated() noexcept {
        return std::exchange(updated_, false);
    }

    [[nodiscard]] std::size_t size() const {
        return options_.size();
    }

  private:
    /// What one definition gives besides its key.
    struct Defined {
        std::string label;
        std::string info;
        std::string fallback;
        std::vector<std::string> offered;
        std::vector<std::string> labels;
    };
    void define(const std::string& key, Defined defined);

    OptionValues chosen_;
    std::vector<Option> options_;
    bool updated_{false};
};

} // namespace opensu::retro
