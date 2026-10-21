// mapping_scopes — every button mapping the player keeps, one per setting layer
// (`settings::Layer`), and which one applies to a game: the first of the game's own, its
// emulator's (every game an emulator runs shares that one), its system's and the global one that
// keeps a mapping, else none. A mapping replaces the one under it whole, so what applies is always
// one visible map. Also the Steam Input switch, global and per game: while Steam Input has a game,
// no mapping applies to it, and the game's own mapping cannot be changed.
#pragma once

#include <map>
#include <optional>
#include <string>

#include "button_map.hpp"
#include "settings/setting_layers.hpp"

namespace opensu::input {

class MappingScopes {
  public:
    /// The mapping kept at `layer`, or null when it keeps none.
    [[nodiscard]] const ButtonMap* at(const settings::Layer& layer) const;
    /// Keeps `map` at `layer`.
    void set(const settings::Layer& layer, ButtonMap map);
    /// Keeps nothing at `layer`, so the layer under it applies.
    void clear(const settings::Layer& layer);

    /// The layer whose mapping applies to `keys`: the first of them that keeps one, or nothing.
    [[nodiscard]] std::optional<settings::Layer> source(const settings::LayerKeys& keys) const;
    /// The mapping that applies to `keys`; empty while Steam Input has the game.
    [[nodiscard]] ButtonMap resolve(const settings::LayerKeys& keys) const;

    [[nodiscard]] bool steamInputGlobal() const noexcept {
        return steamInputGlobal_;
    }
    void setSteamInputGlobal(bool on) noexcept {
        steamInputGlobal_ = on;
    }
    /// The game's own Steam Input choice, or nothing when the global one applies.
    [[nodiscard]] std::optional<bool> steamInputOf(const std::string& game) const;
    void setSteamInput(const std::string& game, std::optional<bool> on);
    /// Whether Steam Input has the game: its own choice, else the global one.
    [[nodiscard]] bool steamInput(const std::string& game) const;

    /// Every layer that keeps a mapping.
    [[nodiscard]] const std::map<settings::Layer, ButtonMap>& maps() const noexcept {
        return maps_;
    }
    [[nodiscard]] const std::map<std::string, bool>& steamInputGames() const noexcept {
        return steamInputGames_;
    }

    bool operator==(const MappingScopes&) const = default;

  private:
    std::map<settings::Layer, ButtonMap> maps_;
    bool steamInputGlobal_{false};
    std::map<std::string, bool> steamInputGames_;
};

} // namespace opensu::input
