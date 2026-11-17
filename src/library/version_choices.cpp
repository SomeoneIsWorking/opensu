#include "version_choices.hpp"

namespace opensu::library {

void VersionChoices::apply(std::vector<Game>& games) const {
    for (Game& game : games) {
        const auto joined = with_.find(game.id);
        game.versionWith = joined == with_.end() ? std::string{} : joined->second;
        game.versionApart = apart_.contains(game.id);
        game.versionDefault = defaults_.contains(game.id);
        game.inExtras = extras_.contains(game.id);
    }
}

void VersionChoices::groupWith(const std::string& id, const std::string& other) {
    if (id == other) {
        return;
    }
    apart_.erase(id);
    // The other's game is where it is now: a version kept apart stays the game it was.
    with_.insert_or_assign(id, other);
}

void VersionChoices::separate(const std::string& id) {
    with_.erase(id);
    std::erase_if(with_, [&id](const auto& join) {
        return join.second == id;
    });
    defaults_.erase(id);
    extras_.erase(id);
    apart_.insert(id);
}

void VersionChoices::makeDefault(const std::string& id, const std::vector<std::string>& game) {
    for (const std::string& version : game) {
        defaults_.erase(version);
    }
    defaults_.insert(id);
    extras_.erase(id);
}

void VersionChoices::setExtra(const std::string& id, bool extra) {
    if (extra) {
        extras_.insert(id);
        defaults_.erase(id);
    } else {
        extras_.erase(id);
    }
}

} // namespace opensu::library
