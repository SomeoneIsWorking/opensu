#include "emulator_game_list.hpp"

#include <set>

#include "titles.hpp"

namespace opensu::library {
namespace {

/// Title key to the indices that have it, among those not yet matched.
template <class Item, class Key>
std::map<std::string, std::vector<std::size_t>>
byTitle(const std::vector<Item>& items, const std::set<std::size_t>& taken, Key keyOf) {
    std::map<std::string, std::vector<std::size_t>> keyed;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (taken.contains(index)) {
            continue;
        }
        if (std::string key = titleKey(keyOf(items[index])); !key.empty()) {
            keyed[std::move(key)].push_back(index);
        }
    }
    return keyed;
}

} // namespace

std::map<std::string, std::size_t> matchGameList(const std::vector<Game>& games,
                                                 const std::vector<EmulatorGameEntry>& entries) {
    std::map<std::string, std::size_t> matched;
    std::set<std::size_t> gamesTaken;
    std::set<std::size_t> entriesTaken;
    std::map<std::filesystem::path, std::size_t> byPath;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!entries[index].path.empty()) {
            byPath.emplace(entries[index].path.lexically_normal(), index);
        }
    }
    for (std::size_t index = 0; index < games.size(); ++index) {
        if (games[index].file.empty()) {
            continue;
        }
        if (const auto found = byPath.find(games[index].file.lexically_normal());
            found != byPath.end()) {
            matched[games[index].id] = found->second;
            gamesTaken.insert(index);
            entriesTaken.insert(found->second);
        }
    }
    const auto gameTitles = byTitle(games, gamesTaken, [](const Game& game) {
        return game.title;
    });
    const auto entryTitles = byTitle(entries, entriesTaken, [](const EmulatorGameEntry& entry) {
        return entry.title;
    });
    for (const auto& [key, gameIndices] : gameTitles) {
        const auto found = entryTitles.find(key);
        if (found != entryTitles.end() && gameIndices.size() == 1 && found->second.size() == 1) {
            matched[games[gameIndices.front()].id] = found->second.front();
        }
    }
    return matched;
}

} // namespace opensu::library
