#include "setting_layers.hpp"

#include <algorithm>

namespace opensu::settings {

std::vector<Layer> layersOf(const LayerKeys& keys) {
    std::vector<Layer> layers;
    if (!keys.game.empty()) {
        layers.push_back(Layer{Layer::Kind::Game, keys.game});
    }
    if (!keys.emulator.empty()) {
        layers.push_back(Layer{Layer::Kind::Emulator, keys.emulator});
    }
    if (!keys.system.empty()) {
        layers.push_back(Layer{Layer::Kind::System, keys.system});
    }
    layers.push_back(Layer{});
    return layers;
}

const std::string* SettingLayers::at(const Layer& layer, std::string_view key) const {
    const auto kept = layers_.find(layer);
    if (kept == layers_.end()) {
        return nullptr;
    }
    const auto value = kept->second.find(key);
    return value == kept->second.end() ? nullptr : &value->second;
}

void SettingLayers::set(const Layer& layer, std::string_view key, std::string value) {
    layers_[layer].insert_or_assign(std::string{key}, std::move(value));
}

void SettingLayers::clear(const Layer& layer, std::string_view key) {
    const auto kept = layers_.find(layer);
    if (kept == layers_.end()) {
        return;
    }
    if (const auto value = kept->second.find(key); value != kept->second.end()) {
        kept->second.erase(value);
    }
    if (kept->second.empty()) {
        layers_.erase(kept);
    }
}

std::optional<Layer> SettingLayers::source(const LayerKeys& keys, std::string_view key) const {
    for (const Layer& layer : layersOf(keys)) {
        if (at(layer, key) != nullptr) {
            return layer;
        }
    }
    return std::nullopt;
}

Resolved SettingLayers::resolve(const LayerKeys& keys, std::string_view key,
                                std::string fallback) const {
    if (const std::optional<Layer> from = source(keys, key)) {
        return Resolved{*at(*from, key), from};
    }
    return Resolved{std::move(fallback), std::nullopt};
}

Resolved SettingLayers::inherited(const LayerKeys& keys, const Layer& layer, std::string_view key,
                                  std::string fallback) const {
    const std::vector<Layer> order = layersOf(keys);
    auto below = std::ranges::find(order, layer);
    if (below != order.end()) {
        ++below;
    }
    for (; below != order.end(); ++below) {
        if (const std::string* value = at(*below, key)) {
            return Resolved{*value, *below};
        }
    }
    return Resolved{std::move(fallback), std::nullopt};
}

std::optional<Layer> SettingLayers::over(const LayerKeys& keys, const Layer& layer,
                                         std::string_view key) const {
    for (const Layer& above : layersOf(keys)) {
        if (above == layer) {
            break;
        }
        if (at(above, key) != nullptr) {
            return above;
        }
    }
    return std::nullopt;
}

} // namespace opensu::settings
