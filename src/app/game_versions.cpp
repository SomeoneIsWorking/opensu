#include "game_versions.hpp"

#include <algorithm>
#include <tuple>

#include "library/titles.hpp"

namespace opensu::app {
namespace {

/// The start of a title's key two titles of one series share: its first word's.
std::string seriesOf(const std::string& title) {
    const std::size_t space = title.find(' ');
    return library::titleKey(title.substr(0, space));
}

} // namespace

std::vector<std::string> GameVersions::versionsOf(const library::Game& game) const {
    std::vector<std::string> ids;
    for (const library::Game& version : library::copiesOf(games_, game)) {
        ids.push_back(version.id);
    }
    return ids;
}

void GameVersions::keep() {
    preferences_.save();
    changed_();
}

void GameVersions::makeDefault(const library::Game& version) {
    preferences_.values().versions.makeDefault(version.id, versionsOf(version));
    keep();
}

void GameVersions::setExtra(const library::Game& version, bool extra) {
    preferences_.values().versions.setExtra(version.id, extra);
    keep();
}

void GameVersions::separate(const library::Game& version) {
    preferences_.values().versions.separate(version.id);
    keep();
}

void GameVersions::groupWith(const library::Game& game, const std::string& otherId) {
    for (const std::string& id : versionsOf(game)) {
        preferences_.values().versions.groupWith(id, otherId);
    }
    keep();
}

std::vector<library::Game> GameVersions::candidates(const library::Game& game) const {
    const std::vector<std::string> own = versionsOf(game);
    const std::string series = seriesOf(game.title);
    std::vector<library::Game> out;
    for (const library::Title& title : library::gameTitles(games_)) {
        const library::Game& preferred = *title.copies.front();
        if (std::ranges::find(own, preferred.id) == own.end()) {
            out.push_back(preferred);
        }
    }
    std::ranges::stable_sort(out, {}, [&series](const library::Game& other) {
        return std::tuple{series.empty() || seriesOf(other.title) != series,
                          library::titleKey(other.title)};
    });
    return out;
}

} // namespace opensu::app
