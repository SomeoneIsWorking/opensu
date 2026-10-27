// setting_scope — the settings one scope of the layered editor shows, read from the schema: those
// that may be kept at the scope's layer and whose condition holds on the values in effect at the
// scope (its own, else what it inherits in the context it was opened in), each with the value in
// effect at the scope, where it comes from, what the scope would inherit without its own, and the
// layer above that wins over it for that game. "N custom settings" is the count of these rows the
// scope keeps, so it never names a value no row shows.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "setting_layers.hpp"
#include "setting_schema.hpp"

namespace opensu::settings {

/// One setting as a scope shows it.
struct ScopedValue {
    const Setting* setting{nullptr};
    /// In effect at the scope: its own value, else what it inherits.
    std::string value;
    /// Where `value` comes from; nothing for the built-in default.
    std::optional<Layer> from;
    /// The scope keeps a value of its own.
    bool own{false};
    /// What applies at the scope without its own value.
    Resolved inherited;
    /// The layer of the context above the scope that keeps its own value, which therefore wins
    /// over the scope's; nothing when none does.
    std::optional<Layer> overriddenBy;
    /// That layer's value.
    std::string overridingValue;
};

/// A setting's value in effect at `scope` for the context `keys`: its own, else what it inherits,
/// else the schema's default. Holds references to `schema` and `layers`.
[[nodiscard]] SettingLookup scopeLookup(const Schema& schema, const SettingLayers& layers,
                                        const LayerKeys& keys, const Layer& scope);

/// The rows of `scope` for the context `keys` (whose layers `scope` must be one of, or Global).
[[nodiscard]] std::vector<ScopedValue> scopedValues(const Schema& schema,
                                                    const SettingLayers& layers,
                                                    const LayerKeys& keys, const Layer& scope);

/// How many of `rows` the scope keeps.
[[nodiscard]] std::size_t keptCount(const std::vector<ScopedValue>& rows);

} // namespace opensu::settings
