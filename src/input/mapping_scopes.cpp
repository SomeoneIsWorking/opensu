#include "mapping_scopes.hpp"

namespace opensu::input {

const ButtonMap* MappingScopes::at(const MappingScope& scope) const {
    switch (scope.kind) {
    case MappingScope::Kind::Global:
        return global_ ? &*global_ : nullptr;
    case MappingScope::Kind::Emulator: {
        const auto found = emulators_.find(scope.id);
        return found == emulators_.end() ? nullptr : &found->second;
    }
    case MappingScope::Kind::Game: {
        const auto found = games_.find(scope.id);
        return found == games_.end() ? nullptr : &found->second;
    }
    }
    return nullptr;
}

void MappingScopes::set(const MappingScope& scope, ButtonMap map) {
    switch (scope.kind) {
    case MappingScope::Kind::Global:
        global_ = std::move(map);
        break;
    case MappingScope::Kind::Emulator:
        emulators_[scope.id] = std::move(map);
        break;
    case MappingScope::Kind::Game:
        games_[scope.id] = std::move(map);
        break;
    }
}

void MappingScopes::clear(const MappingScope& scope) {
    switch (scope.kind) {
    case MappingScope::Kind::Global:
        global_.reset();
        break;
    case MappingScope::Kind::Emulator:
        emulators_.erase(scope.id);
        break;
    case MappingScope::Kind::Game:
        games_.erase(scope.id);
        break;
    }
}

std::optional<MappingScope> MappingScopes::source(const GameKeys& keys) const {
    if (!keys.game.empty() && games_.contains(keys.game)) {
        return MappingScope{MappingScope::Kind::Game, keys.game};
    }
    if (!keys.emulator.empty() && emulators_.contains(keys.emulator)) {
        return MappingScope{MappingScope::Kind::Emulator, keys.emulator};
    }
    if (global_) {
        return MappingScope{};
    }
    return std::nullopt;
}

ButtonMap MappingScopes::resolve(const GameKeys& keys) const {
    if (steamInput(keys.game)) {
        return {};
    }
    const std::optional<MappingScope> from = source(keys);
    return from ? *at(*from) : ButtonMap{};
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
