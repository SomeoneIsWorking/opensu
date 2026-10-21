// setting_layers — the layers a per-game setting is kept at and the one rule for which applies:
// a game's own, else its emulator's (the core layer), else its console's, else the one set
// everywhere, else the built-in default. Each layer keeps only the keys it sets, so a missing key
// inherits and clearing one deletes it. Every resolved value names the layer it came from.
#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::settings {

/// Where a setting is kept.
struct Layer {
    /// `Emulator` is the core layer: a built-in core's, or a standalone emulator's for mappings.
    enum class Kind : std::uint8_t { Global, System, Emulator, Game };

    Kind kind{Kind::Global};
    /// The system's key, the emulator's catalog id or the game's id; empty for the global layer.
    std::string id;

    auto operator<=>(const Layer&) const = default;
};

/// What a game's layers are found by.
struct LayerKeys {
    /// `library::Game::id`.
    std::string game;
    /// The ROM's system key; empty for a game of a store.
    std::string system;
    /// The catalog id of the emulator it runs on; empty for a native game.
    std::string emulator;

    bool operator==(const LayerKeys&) const = default;
};

/// The layers that apply to `keys`, the one that wins first; a key that is empty has no layer.
[[nodiscard]] std::vector<Layer> layersOf(const LayerKeys& keys);

/// A setting's value for a game and where it came from: a layer, or the built-in default when
/// `from` is empty.
struct Resolved {
    std::string value;
    std::optional<Layer> from;

    bool operator==(const Resolved&) const = default;
};

class SettingLayers {
  public:
    /// A layer's keys and their values.
    using Values = std::map<std::string, std::string, std::less<>>;

    /// The value `layer` keeps for `key`, or null when it keeps none.
    [[nodiscard]] const std::string* at(const Layer& layer, std::string_view key) const;
    void set(const Layer& layer, std::string_view key, std::string value);
    /// Deletes `key` from `layer`, so the layer under it applies.
    void clear(const Layer& layer, std::string_view key);

    /// The first layer of `keys` that keeps `key`, or nothing.
    [[nodiscard]] std::optional<Layer> source(const LayerKeys& keys, std::string_view key) const;
    /// `key` for `keys`: the winning layer's value, else `fallback`.
    [[nodiscard]] Resolved resolve(const LayerKeys& keys, std::string_view key,
                                   std::string fallback) const;
    /// `key` for `keys` as it would be without `layer` and the layers that win over it: what
    /// `layer` inherits.
    [[nodiscard]] Resolved inherited(const LayerKeys& keys, const Layer& layer,
                                     std::string_view key, std::string fallback) const;
    /// The layer of `keys` that wins over `layer` for `key`, or nothing when none above it keeps
    /// one.
    [[nodiscard]] std::optional<Layer> over(const LayerKeys& keys, const Layer& layer,
                                            std::string_view key) const;

    /// Every layer that keeps a key.
    [[nodiscard]] const std::map<Layer, Values>& layers() const noexcept {
        return layers_;
    }

    bool operator==(const SettingLayers&) const = default;

  private:
    std::map<Layer, Values> layers_;
};

} // namespace opensu::settings
