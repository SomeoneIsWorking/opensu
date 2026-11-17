// game_versions — what a game's page changes about its versions: a version made its game's
// default, moved to its Extras or back, separated, and the game grouped with another, kept in the
// settings (`library::VersionChoices`) and applied to the library at once.
#pragma once

#include <functional>
#include <string>
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
    void setExtra(const library::Game& version, bool extra);
    void separate(const library::Game& version);
    /// Groups every version of `game` with game `otherId`'s.
    void groupWith(const library::Game& game, const std::string& otherId);
    /// The other games `game` can be grouped with, each as its preferred version: those whose
    /// title starts as `game`'s does first, then the rest, each by title.
    [[nodiscard]] std::vector<library::Game> candidates(const library::Game& game) const;

  private:
    /// The ids of every version of `game`'s game.
    [[nodiscard]] std::vector<std::string> versionsOf(const library::Game& game) const;
    void keep();

    Preferences& preferences_;
    const std::vector<library::Game>& games_;
    std::function<void()> changed_;
};

} // namespace opensu::app
