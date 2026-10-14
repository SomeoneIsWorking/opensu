// mapping_scopes — every button mapping the player keeps and which one applies to a game: the
// game's own, else its emulator's (every game an emulator runs shares that one), else the global
// one, else none. A mapping replaces the one under it whole, so what applies is always one visible
// map. Also the Steam Input switch, global and per game: while Steam Input has a game, no mapping
// applies to it, and the game's own mapping cannot be changed.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "button_map.hpp"

namespace opensu::input {

/// Where a mapping is kept.
struct MappingScope {
    enum class Kind : std::uint8_t { Global, Emulator, Game };

    Kind kind{Kind::Global};
    /// The emulator's catalog id or the game's id; empty for the global scope.
    std::string id;

    bool operator==(const MappingScope&) const = default;
};

/// The keys a game's mapping is found by.
struct GameKeys {
    /// `library::Game::id`.
    std::string game;
    /// The catalog id of the emulator it runs on; empty for a native game.
    std::string emulator;

    bool operator==(const GameKeys&) const = default;
};

class MappingScopes {
  public:
    /// The mapping kept at `scope`, or null when it keeps none.
    [[nodiscard]] const ButtonMap* at(const MappingScope& scope) const;
    /// Keeps `map` at `scope`.
    void set(const MappingScope& scope, ButtonMap map);
    /// Keeps nothing at `scope`, so the scope under it applies.
    void clear(const MappingScope& scope);

    /// The scope whose mapping applies to `keys`: the first of game, emulator and global that keeps
    /// one, or nothing.
    [[nodiscard]] std::optional<MappingScope> source(const GameKeys& keys) const;
    /// The mapping that applies to `keys`; empty while Steam Input has the game.
    [[nodiscard]] ButtonMap resolve(const GameKeys& keys) const;

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

    [[nodiscard]] const std::optional<ButtonMap>& global() const noexcept {
        return global_;
    }
    [[nodiscard]] const std::map<std::string, ButtonMap>& emulators() const noexcept {
        return emulators_;
    }
    [[nodiscard]] const std::map<std::string, ButtonMap>& games() const noexcept {
        return games_;
    }
    [[nodiscard]] const std::map<std::string, bool>& steamInputGames() const noexcept {
        return steamInputGames_;
    }

    bool operator==(const MappingScopes&) const = default;

  private:
    std::optional<ButtonMap> global_;
    std::map<std::string, ButtonMap> emulators_;
    std::map<std::string, ButtonMap> games_;
    bool steamInputGlobal_{false};
    std::map<std::string, bool> steamInputGames_;
};

} // namespace opensu::input
