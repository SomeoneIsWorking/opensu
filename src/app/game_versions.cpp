#include "game_versions.hpp"

#include <string>

#include "library/titles.hpp"

namespace opensu::app {

void GameVersions::makeDefault(const library::Game& version) {
    std::vector<std::string> ids;
    for (const library::Game& copy : library::copiesOf(games_, version)) {
        ids.push_back(copy.id);
    }
    preferences_.values().versionDefaults.makeDefault(version.id, ids);
    preferences_.save();
    changed_();
}

} // namespace opensu::app
