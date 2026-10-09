// emulator_choice — the emulator the player picked for a ROM, kept per game. A game without a
// pick runs on the emulator the search found for its system.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class EmulatorChoices {
  public:
    /// Game id to the emulator's name.
    using Entries = std::map<std::string, std::string>;

    EmulatorChoices() = default;
    explicit EmulatorChoices(Entries entries) : entries_{std::move(entries)} {
    }

    /// Picks `emulator` for the game `gameId`.
    void set(const std::string& gameId, const std::string& emulator);

    /// Points each ROM with a pick at its emulator: the game's `launch` and `emulator` follow the
    /// pick, and `unavailable` says so when the pick is not installed. A ROM with no pick, or one
    /// whose emulator is no longer listed for it, keeps what the search found.
    void apply(std::vector<Game>& games) const;

    [[nodiscard]] const Entries& entries() const noexcept {
        return entries_;
    }

    bool operator==(const EmulatorChoices&) const = default;

  private:
    Entries entries_;
};

} // namespace opensu::library
