// play_time — play time that emulators count themselves, merged into the ROM games. Each source
// (a plugin's play-time source) reports seconds per game; a ROM game's play time is the sum over
// the sources that report it. An emulator counts only its own sessions, so the sum counts each
// session once. Store games keep their store's value.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class PlayTimeReports {
  public:
    /// Game id to seconds played.
    using Report = std::map<std::string, std::int64_t>;

    /// Replaces what `source` reported before.
    void set(const std::string& source, Report report);
    /// Forgets `source`, such as a plugin that was turned off.
    void erase(const std::string& source);

    /// Gives each ROM game the sum of the reported seconds, in whole minutes; 0 when no source
    /// reports it.
    void apply(std::vector<Game>& games) const;

    [[nodiscard]] const std::map<std::string, Report>& sources() const noexcept {
        return sources_;
    }

  private:
    std::map<std::string, Report> sources_;
};

} // namespace opensu::library
