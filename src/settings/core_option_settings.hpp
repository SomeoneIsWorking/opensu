// core_option_settings — a built-in core's own options as layered settings: each kept as
// `core.<option key>` at the core's layer or a game's, declared in the schema from the definitions
// the core gives (label, values with their labels, default), and resolved for a game into the
// option values `opensu-retro --option` and its control channel take.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "setting_layers.hpp"
#include "setting_schema.hpp"

namespace opensu::settings {

/// The prefix every core option's key is kept under.
inline constexpr std::string_view coreOptionPrefix = "core.";
/// The heading core options stand under.
inline constexpr std::string_view coreOptionSection = "Core options";

/// One option as the core defines it.
struct CoreOptionDefinition {
    std::string key;
    std::string label;
    /// The core's line about it; empty when it gives none.
    std::string help;
    /// Its values in the core's order, each with the core's label for it.
    std::vector<SettingChoice> values;
    std::string fallback;

    bool operator==(const CoreOptionDefinition&) const = default;
};

/// Declares `options` in `schema`, each settable at a core's layer and a game's.
void declareCoreOptions(Schema& schema, const std::vector<CoreOptionDefinition>& options);
/// Declares the family every core option's key belongs to, for clearing and validating where the
/// core's own definitions are not known.
void declareCoreOptionFamily(Schema& schema);

/// The core options `keys` keep at any of their layers, each resolved by the layers' one rule:
/// option key (without the prefix) to value.
[[nodiscard]] std::map<std::string, std::string> resolveCoreOptions(const SettingLayers& layers,
                                                                    const LayerKeys& keys);

} // namespace opensu::settings
