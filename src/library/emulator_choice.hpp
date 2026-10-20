// emulator_choice — the emulator the player picked, kept per game and per system. A game's own
// pick comes first, then its system's, then the emulator the search found for the system.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class EmulatorChoices {
  public:
    /// Game id, or system key, to the emulator's name.
    using Entries = std::map<std::string, std::string>;

    EmulatorChoices() = default;
    explicit EmulatorChoices(Entries games, Entries systems = {})
        : entries_{std::move(games)}, systems_{std::move(systems)} {
    }

    /// Picks `emulator` for the game `gameId`.
    void set(const std::string& gameId, const std::string& emulator);
    /// Picks `emulator` for every game of `system` without a pick of its own; an empty name goes
    /// back to the emulator the search finds.
    void setSystem(const std::string& system, const std::string& emulator);

    /// Points each ROM with a pick, its own or its system's, at that emulator: the game's
    /// `launch` and `emulator` follow the pick, and `unavailable` says so when the pick is not
    /// installed. A ROM with no pick, one whose emulator is no longer listed for it, or a
    /// system's pick its arcade romset does not match, keeps what the search found.
    void apply(std::vector<Game>& games) const;

    /// The games' picks.
    [[nodiscard]] const Entries& entries() const noexcept {
        return entries_;
    }
    /// The systems' picks.
    [[nodiscard]] const Entries& systems() const noexcept {
        return systems_;
    }
    /// Forgets every game's pick; the systems' stay.
    void clearGames() noexcept {
        entries_.clear();
    }

    bool operator==(const EmulatorChoices&) const = default;

  private:
    Entries entries_;
    Entries systems_;
};

} // namespace opensu::library
