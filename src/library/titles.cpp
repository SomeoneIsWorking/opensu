#include "titles.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <unordered_map>

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

/// The copy of `title` from `source`, or null.
const Game* copyFrom(const Title& title, Source source) {
    const auto found = std::ranges::find(title.copies, source, &Game::source);
    return found == title.copies.end() ? nullptr : *found;
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

std::optional<Source> CopyChoices::chosen(const Game& game) const {
    const auto found = entries_.find(choiceKey(game));
    return found == entries_.end() ? std::nullopt : std::optional{found->second};
}

void CopyChoices::choose(const Game& copy) {
    entries_[choiceKey(copy)] = copy.source;
}

TitleCopies::TitleCopies(const std::vector<Game>& games, const CopyChoices& choices)
    : choices_{choices}, titles_{storeTitles(games)} {
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

const Game* TitleCopies::use(const Game& game, std::optional<Source> page) const {
    const Title* title = titleOf(game);
    if (title == nullptr) {
        return nullptr;
    }
    const std::optional<Source> chosen = choices_.chosen(game);
    if (chosen && (!page || isStore(*page))) {
        if (const Game* copy = copyFrom(*title, *chosen)) {
            return copy;
        }
    }
    if (page) {
        if (const Game* copy = copyFrom(*title, *page)) {
            return copy;
        }
    }
    return title->copies.front();
}

std::vector<Game> TitleCopies::installable(const Game& game, std::optional<Source> page) const {
    const Title* title = titleOf(game);
    if (title == nullptr) {
        return {game};
    }
    const std::optional<Source> chosen = choices_.chosen(game);
    if (page || (chosen && copyFrom(*title, *chosen) != nullptr)) {
        return {*use(game, page)};
    }
    std::vector<Game> out;
    out.reserve(title->copies.size());
    for (const Game* copy : title->copies) {
        out.push_back(*copy);
    }
    return out;
}

std::vector<Game> TitleCopies::switchable(const Game& game, std::optional<Source> page) const {
    const Title* title = titleOf(game);
    if (title == nullptr || (page && !isStore(*page))) {
        return {};
    }
    std::vector<Game> out;
    for (const Game* copy : title->copies) {
        if (isStore(copy->source)) {
            out.push_back(*copy);
        }
    }
    if (out.size() < 2) {
        return {};
    }
    std::ranges::sort(out, {}, [](const Game& copy) {
        return rank(copy.source);
    });
    return out;
}

} // namespace opensu::library
