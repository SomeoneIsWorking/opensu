// play_history — when each game was last launched from opensu, kept for the stores that do not
// say (Epic, GOG, ROMs). Steam's own last-played time is kept when it is later.
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class PlayHistory {
  public:
    /// Game id to seconds since the epoch.
    using Entries = std::map<std::string, std::int64_t>;

    PlayHistory() = default;
    explicit PlayHistory(Entries entries) : entries_{std::move(entries)} {
    }

    /// Notes that `id` was launched at `at`.
    void record(const std::string& id, std::chrono::system_clock::time_point at);

    /// Gives each game its recorded time when that is later than the one it has.
    void apply(std::vector<Game>& games) const;

    [[nodiscard]] const Entries& entries() const noexcept {
        return entries_;
    }

    bool operator==(const PlayHistory&) const = default;

  private:
    Entries entries_;
};

} // namespace opensu::library
