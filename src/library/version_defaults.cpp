#include "version_defaults.hpp"

namespace opensu::library {

void VersionDefaults::apply(std::vector<Game>& games) const {
    for (Game& game : games) {
        game.versionDefault = ids_.contains(game.id);
    }
}

void VersionDefaults::makeDefault(const std::string& id, const std::vector<std::string>& game) {
    for (const std::string& version : game) {
        ids_.erase(version);
    }
    ids_.insert(id);
}

} // namespace opensu::library
