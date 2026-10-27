#include "setting_scope.hpp"

#include <algorithm>

namespace opensu::settings {

SettingLookup scopeLookup(const Schema& schema, const SettingLayers& layers, const LayerKeys& keys,
                          const Layer& scope) {
    return [&schema, &layers, keys, scope](std::string_view key) {
        if (const std::string* own = layers.at(scope, key)) {
            return *own;
        }
        const Setting* setting = schema.find(key);
        std::string fallback = setting != nullptr ? setting->fallback : std::string{};
        if (scope.kind == Layer::Kind::Global) {
            return fallback;
        }
        return layers.inherited(keys, scope, key, std::move(fallback)).value;
    };
}

std::vector<ScopedValue> scopedValues(const Schema& schema, const SettingLayers& layers,
                                      const LayerKeys& keys, const Layer& scope) {
    const SettingLookup applies = scopeLookup(schema, layers, keys, scope);
    std::vector<ScopedValue> rows;
    for (const Setting& setting : schema.settings()) {
        if (!setting.layers.has(scope.kind) || (setting.shownWhen && !setting.shownWhen(applies))) {
            continue;
        }
        ScopedValue row;
        row.setting = &setting;
        row.inherited = scope.kind == Layer::Kind::Global
                            ? Resolved{setting.fallback, std::nullopt}
                            : layers.inherited(keys, scope, setting.key, setting.fallback);
        if (const std::string* own = layers.at(scope, setting.key)) {
            row.own = true;
            row.value = *own;
            row.from = scope;
        } else {
            row.value = row.inherited.value;
            row.from = row.inherited.from;
        }
        row.overriddenBy = layers.over(keys, scope, setting.key);
        if (row.overriddenBy) {
            row.overridingValue = *layers.at(*row.overriddenBy, setting.key);
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::size_t keptCount(const std::vector<ScopedValue>& rows) {
    return static_cast<std::size_t>(std::ranges::count_if(rows, &ScopedValue::own));
}

} // namespace opensu::settings
