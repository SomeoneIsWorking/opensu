#include "titles.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <unordered_map>

namespace iideck::library {
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
        break;
    }
    return 3;
}

bool holds(const std::vector<const Game*>& copies, Source source) {
    return std::ranges::any_of(copies, [source](const Game* copy) {
        return copy->source == source;
    });
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

} // namespace iideck::library
