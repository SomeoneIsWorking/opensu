#include "titles.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace opensu::library {
namespace {

/// Steam, GOG, Epic: the store a title is taken from when more than one will do.
int rank(Source source) noexcept {
    switch (source) {
    case Source::Steam:
        return 0;
    case Source::Gog:
        return 1;
    case Source::Epic:
        return 2;
    case Source::Rom:
    case Source::Own:
        break;
    case Source::Remote:
        return 4;
    }
    return 3;
}

bool holds(const std::vector<const Game*>& copies, Source source) {
    return std::ranges::any_of(copies, [source](const Game* copy) {
        return copy->source == source;
    });
}

bool isStore(Source source) noexcept {
    return source == Source::Steam || source == Source::Gog || source == Source::Epic;
}

/// When `copy` was last played, as `TitleCopies` orders it; a copy of no store never leads by it.
std::optional<std::chrono::system_clock::time_point> playedAt(const Game& copy) {
    return isStore(copy.source) ? copy.lastPlayed : std::nullopt;
}

} // namespace

std::string titleKey(std::string_view title) {
    std::string key;
    for (const char c : title) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '&') {
            key += "and";
        } else if (byte < 0x80 && std::isalnum(byte) != 0) {
            key += static_cast<char>(std::tolower(byte));
        }
    }
    return key;
}

std::string choiceKey(const Game& game) {
    if (game.source != Source::Rom) {
        if (const std::string title = titleKey(game.title); !title.empty()) {
            return "title:" + title;
        }
    }
    return "id:" + game.id;
}

std::vector<Title> storeTitles(const std::vector<Game>& games) {
    std::vector<Title> titles;
    std::unordered_map<std::string, std::vector<std::size_t>> byKey;
    for (const Game& game : games) {
        if (game.source == Source::Rom) {
            continue;
        }
        const std::string key = titleKey(game.title);
        std::vector<std::size_t>& candidates = byKey[key];
        const auto found =
            key.empty() ? candidates.end() : std::ranges::find_if(candidates, [&](std::size_t at) {
                return !holds(titles[at].copies, game.source);
            });
        if (found != candidates.end()) {
            titles[*found].copies.push_back(&game);
            continue;
        }
        candidates.push_back(titles.size());
        titles.push_back(Title{{&game}});
    }
    for (Title& title : titles) {
        std::ranges::stable_sort(title.copies, [](const Game* a, const Game* b) {
            if (a->installed != b->installed) {
                return a->installed;
            }
            return rank(a->source) < rank(b->source);
        });
    }
    // Pointers into one vector order as the games do.
    std::ranges::stable_sort(titles, [](const Title& a, const Title& b) {
        return std::less<const Game*>{}(a.copies.front(), b.copies.front());
    });
    return titles;
}

std::vector<Game> copiesOf(const std::vector<Game>& games, const Game& game) {
    for (const Title& title : storeTitles(games)) {
        if (std::ranges::any_of(title.copies, [&game](const Game* copy) {
                return copy->id == game.id;
            })) {
            std::vector<Game> out;
            out.reserve(title.copies.size());
            for (const Game* copy : title.copies) {
                out.push_back(*copy);
            }
            return out;
        }
    }
    return {game};
}

void markCopies(std::vector<Game>& games) {
    std::unordered_set<std::string> several;
    for (const Title& title : storeTitles(games)) {
        if (std::ranges::count_if(title.copies, isStore, &Game::source) < 2) {
            continue;
        }
        for (const Game* copy : title.copies) {
            if (isStore(copy->source)) {
                several.insert(copy->id);
            }
        }
    }
    for (Game& game : games) {
        game.severalStores = several.contains(game.id);
    }
}

void shareTitleData(std::vector<Game>& games, const std::vector<KeptData>& kept) {
    std::vector<Title> titles = storeTitles(games);
    const auto storeCopies = [](const Title& title) {
        std::vector<const Game*> copies;
        std::ranges::copy_if(title.copies, std::back_inserter(copies), isStore, &Game::source);
        return copies;
    };
    // Same-titled games of one store are titles of their own: one with several stores goes first.
    std::ranges::stable_partition(titles, [&storeCopies](const Title& title) {
        return storeCopies(title).size() > 1;
    });
    std::unordered_map<std::string, std::string> shared;
    std::unordered_set<std::string> taken;
    for (const Title& title : titles) {
        std::vector<const Game*> copies = storeCopies(title);
        if (copies.empty()) {
            continue;
        }
        const std::string key = choiceKey(*copies.front());
        const bool held = std::ranges::any_of(kept, [&key](const KeptData& data) {
            return data.holds(key);
        });
        if (key.starts_with("id:") || (copies.size() < 2 && !held) || !taken.insert(key).second) {
            continue;
        }
        std::ranges::stable_sort(copies, [](const Game* a, const Game* b) {
            return a->lastPlayed > b->lastPlayed;
        });
        for (const KeptData& data : kept) {
            const auto from = std::ranges::find_if(copies, [&data](const Game* copy) {
                return data.holds(copy->id);
            });
            if (!data.holds(key) && from != copies.end()) {
                data.copy((*from)->id, key);
            }
        }
        for (const Game* copy : copies) {
            shared.emplace(copy->id, key);
        }
    }
    for (Game& game : games) {
        const auto found = shared.find(game.id);
        game.sharedId = found == shared.end() ? std::string{} : found->second;
    }
}

TitleCopies::TitleCopies(const std::vector<Game>& games) : titles_{storeTitles(games)} {
    for (std::size_t at = 0; at < titles_.size(); ++at) {
        for (const Game* copy : titles_[at].copies) {
            byGame_.emplace(copy->id, at);
        }
    }
}

const Title* TitleCopies::titleOf(const Game& game) const {
    const auto found = byGame_.find(game.id);
    return found == byGame_.end() ? nullptr : &titles_[found->second];
}

std::vector<const Game*> TitleCopies::ordered(const Game& game) const {
    const Title* title = titleOf(game);
    if (title == nullptr) {
        return {};
    }
    std::vector<const Game*> copies = title->copies;
    std::ranges::stable_sort(copies, [](const Game* a, const Game* b) {
        if (a->installed != b->installed) {
            return a->installed;
        }
        return playedAt(*a) > playedAt(*b);
    });
    return copies;
}

const Game* TitleCopies::top(const Game& game) const {
    const std::vector<const Game*> copies = ordered(game);
    return copies.empty() ? nullptr : copies.front();
}

bool TitleCopies::updatePending(const Game& game) const {
    return std::ranges::any_of(ordered(game), &Game::updateAvailable);
}

std::vector<Game> TitleCopies::offered(const Game& game) const {
    const std::vector<const Game*> copies = ordered(game);
    if (std::ranges::count_if(copies, isStore, &Game::source) < 2) {
        return {};
    }
    std::vector<Game> out;
    for (const Game* copy : copies) {
        if (isStore(copy->source) || copy == copies.front()) {
            out.push_back(*copy);
        }
    }
    return out;
}

} // namespace opensu::library
