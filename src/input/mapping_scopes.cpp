#include "mapping_scopes.hpp"

namespace opensu::input {

const ButtonMap* MappingScopes::at(const settings::Layer& layer) const {
    const auto found = maps_.find(layer);
    return found == maps_.end() ? nullptr : &found->second;
}

void MappingScopes::set(const settings::Layer& layer, ButtonMap map) {
    maps_.insert_or_assign(layer, std::move(map));
}

void MappingScopes::clear(const settings::Layer& layer) {
    maps_.erase(layer);
}

std::optional<settings::Layer> MappingScopes::source(const settings::LayerKeys& keys) const {
    for (const settings::Layer& layer : settings::layersOf(keys)) {
        if (maps_.contains(layer)) {
            return layer;
        }
    }
    return std::nullopt;
}

ButtonMap MappingScopes::resolve(const settings::LayerKeys& keys) const {
    if (steamInput(keys.game)) {
        return {};
    }
    const std::optional<settings::Layer> from = source(keys);
    return from ? *at(*from) : ButtonMap{};
}

bool MappingScopes::holdsGame(const std::string& id) const {
    return maps_.contains(settings::Layer{settings::Layer::Kind::Game, id}) ||
           steamInputGames_.contains(id);
}

void MappingScopes::copyGame(const std::string& from, const std::string& to) {
    const settings::Layer source{settings::Layer::Kind::Game, from};
    if (const auto map = maps_.find(source); map != maps_.end()) {
        maps_[settings::Layer{settings::Layer::Kind::Game, to}] = map->second;
    }
    if (const auto steam = steamInputGames_.find(from); steam != steamInputGames_.end()) {
        steamInputGames_[to] = steam->second;
    }
}

std::optional<bool> MappingScopes::steamInputOf(const std::string& game) const {
    const auto found = steamInputGames_.find(game);
    return found == steamInputGames_.end() ? std::nullopt : std::optional<bool>{found->second};
}

void MappingScopes::setSteamInput(const std::string& game, std::optional<bool> on) {
    if (on) {
        steamInputGames_[game] = *on;
    } else {
        steamInputGames_.erase(game);
    }
}

bool MappingScopes::steamInput(const std::string& game) const {
    return steamInputOf(game).value_or(steamInputGlobal_);
}

} // namespace opensu::input
