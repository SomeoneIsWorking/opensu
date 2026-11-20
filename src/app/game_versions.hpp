// game_versions — a version a game's page made its game's default, kept in the settings
// (`library::VersionDefaults`) and applied to the library at once.
#pragma once

#include <functional>
#include <vector>

#include "library/game.hpp"
#include "preferences.hpp"

namespace opensu::app {

class GameVersions {
  public:
    /// `games` is the library; `changed` marks it again and shows it.
    GameVersions(Preferences& preferences, const std::vector<library::Game>& games,
                 std::function<void()> changed)
        : preferences_{preferences}, games_{games}, changed_{std::move(changed)} {
    }

    void makeDefault(const library::Game& version);

  private:
    Preferences& preferences_;
    const std::vector<library::Game>& games_;
    std::function<void()> changed_;
};

} // namespace opensu::app
