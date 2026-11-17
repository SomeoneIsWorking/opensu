#include "titles.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "own_games.hpp"
#include "rom_systems.hpp"
#include "store_builds.hpp"

namespace opensu::library {
namespace {

/// The order versions are taken in when nothing else tells them apart: Steam, GOG, Epic, the
/// player's own, ROMs, another openSU's.
int rank(Source source) noexcept {
    switch (source) {
    case Source::Steam:
        return 0;
    case Source::Gog:
        return 1;
    case Source::Epic:
        return 2;
    case Source::Own:
        return 3;
    case Source::Rom:
        return 4;
    case Source::Remote:
        return 5;
    }
    return 6;
}

bool isStore(Source source) noexcept {
    return source == Source::Steam || source == Source::Gog || source == Source::Epic;
}

/// What holds at most one version of a game: its store, or a store on the openSU it is on; empty
/// for a source that may hold several.
std::string holderOf(const Game& game) {
    if (isStore(game.source)) {
        return key(game.source);
    }
    if (game.source != Source::Remote) {
        return {};
    }
    const bool store = game.remote.source == key(Source::Steam) ||
                       game.remote.source == key(Source::Gog) ||
                       game.remote.source == key(Source::Epic);
    return store ? "lan:" + game.hostId + ":" + game.remote.source : std::string{};
}

/// When `copy` was last played, as `TitleCopies` orders it; another openSU's never leads by it.
std::optional<std::chrono::system_clock::time_point> playedAt(const Game& copy) {
    return copy.source == Source::Remote ? std::nullopt : copy.lastPlayed;
}

/// Whether `a` goes before `b` among one game's versions, play history aside.
bool before(const Game& a, const Game& b) {
    if (a.inExtras != b.inExtras) {
        return b.inExtras;
    }
    if (a.versionDefault != b.versionDefault) {
        return a.versionDefault;
    }
    if (a.installed != b.installed) {
        return a.installed;
    }
    const bool aRemote = a.source == Source::Remote;
    const bool bRemote = b.source == Source::Remote;
    if (aRemote != bRemote) {
        return bRemote;
    }
    return rank(a.source) < rank(b.source);
}

/// Where `title`'s subtitle starts: " - ", ": ", an en or em dash; npos for none.
std::size_t subtitleAt(std::string_view title) {
    std::size_t first = std::string_view::npos;
    for (const std::string_view mark : {" - ", ": ", " \xE2\x80\x93 ", " \xE2\x80\x94 "}) {
        first = std::min(first, title.find(mark));
    }
    return first;
}

/// The games' versions as groups of indices into them, in the order their first members come.
class Grouping {
  public:
    explicit Grouping(const std::vector<Game>& games) : games_{games}, groupOf_(games.size()) {
    }

    [[nodiscard]] std::size_t groupOf(std::size_t game) const {
        return groupOf_[game];
    }
    [[nodiscard]] std::vector<std::vector<std::size_t>>& groups() noexcept {
        return groups_;
    }

    /// `game` as a group of its own.
    void alone(std::size_t game) {
        groupOf_[game] = groups_.size();
        groups_.push_back({game});
    }
    /// `game` into `group`.
    void into(std::size_t game, std::size_t group) {
        groupOf_[game] = group;
        groups_[group].push_back(game);
    }
    /// Whether `group` holds a version of `holder` already (`holderOf`).
    [[nodiscard]] bool holds(std::size_t group, const std::string& holder) const {
        return !holder.empty() && std::ranges::any_of(groups_[group], [&](std::size_t member) {
            return holderOf(games_[member]) == holder;
        });
    }
    /// Whether two groups share a holder, so they cannot be one game unless the player says so.
    [[nodiscard]] bool clash(std::size_t a, std::size_t b) const {
        return std::ranges::any_of(groups_[b], [&](std::size_t member) {
            return holds(a, holderOf(games_[member]));
        });
    }
    /// Moves `from`'s members into `to`; `from` is left empty.
    void merge(std::size_t from, std::size_t to) {
        if (from == to) {
            return;
        }
        for (const std::size_t member : groups_[from]) {
            into(member, to);
        }
        groups_[from].clear();
    }

  private:
    const std::vector<Game>& games_;
    std::vector<std::size_t> groupOf_;
    std::vector<std::vector<std::size_t>> groups_;
};

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

std::string baseKey(std::string_view title) {
    return titleKey(title.substr(0, subtitleAt(title)));
}

std::string choiceKey(const Game& game) {
    if (game.source != Source::Rom) {
        if (const std::string title = titleKey(game.title); !title.empty()) {
            return "title:" + title;
        }
    }
    return "id:" + game.id;
}

std::string versionPlatform(const Game& game) {
    switch (game.source) {
    case Source::Rom: {
        const roms::RomSystem* system = roms::systemByKey(game.sourceId);
        return system != nullptr ? std::string{system->label} : game.sourceId;
    }
    case Source::Own: {
        if (!own::nativeOwn(game)) {
            return "Windows \xC2\xB7 Wine";
        }
        const std::string system{ownSystemName};
        return game.appImage ? system + " \xC2\xB7 AppImage" : system;
    }
    case Source::Remote:
        return game.remote.platform;
    case Source::Steam:
    case Source::Epic:
    case Source::Gog:
        break;
    }
    return std::string{label(game.source)};
}

std::vector<Title> gameTitles(const std::vector<Game>& games) {
    Grouping grouping{games};
    // Same key: one game, but a store or an openSU holds one version of it.
    std::unordered_map<std::string, std::vector<std::size_t>> byKey;
    std::vector<std::string> keys(games.size());
    for (std::size_t at = 0; at < games.size(); ++at) {
        const Game& game = games[at];
        keys[at] = game.versionApart ? std::string{} : titleKey(game.title);
        std::vector<std::size_t>& candidates = byKey[keys[at]];
        const std::string holder = holderOf(game);
        const auto found = keys[at].empty()
                               ? candidates.end()
                               : std::ranges::find_if(candidates, [&](std::size_t group) {
                                     return !grouping.holds(group, holder);
                                 });
        if (found != candidates.end()) {
            grouping.into(at, *found);
            continue;
        }
        if (!keys[at].empty()) {
            candidates.push_back(grouping.groups().size());
        }
        grouping.alone(at);
    }
    // A title and the one subtitled title of its base: one game.
    std::unordered_map<std::string, std::set<std::string>> subtitled;
    for (std::size_t at = 0; at < games.size(); ++at) {
        if (!keys[at].empty()) {
            const std::string base = baseKey(games[at].title);
            if (!base.empty() && base != keys[at]) {
                subtitled[base].insert(keys[at]);
            }
        }
    }
    for (const auto& [base, full] : subtitled) {
        const auto bare = byKey.find(base);
        if (full.size() != 1 || bare == byKey.end()) {
            continue;
        }
        const std::size_t to = byKey.at(*full.begin()).front();
        if (grouping.groups()[to].empty()) {
            continue;
        }
        for (const std::size_t group : bare->second) {
            if (!grouping.clash(to, group)) {
                grouping.merge(group, to);
            }
        }
    }
    // The player's groupings, whatever the titles say.
    std::unordered_map<std::string, std::size_t> indexOf;
    for (std::size_t at = 0; at < games.size(); ++at) {
        indexOf.emplace(games[at].id, at);
    }
    for (std::size_t at = 0; at < games.size(); ++at) {
        if (const auto other = indexOf.find(games[at].versionWith);
            !games[at].versionWith.empty() && other != indexOf.end()) {
            const std::size_t from = grouping.groupOf(at);
            const std::size_t to = grouping.groupOf(other->second);
            if (grouping.groups()[from].size() == 1) {
                grouping.merge(from, to);
            } else {
                grouping.groups()[from].erase(std::ranges::find(grouping.groups()[from], at));
                grouping.into(at, to);
            }
        }
    }
    std::vector<Title> titles;
    for (const std::vector<std::size_t>& group : grouping.groups()) {
        if (group.empty()) {
            continue;
        }
        Title title;
        for (const std::size_t member : group) {
            title.copies.push_back(&games[member]);
        }
        std::ranges::stable_sort(title.copies, [](const Game* a, const Game* b) {
            return before(*a, *b);
        });
        titles.push_back(std::move(title));
    }
    // Pointers into one vector order as the games do.
    std::ranges::stable_sort(titles, [](const Title& a, const Title& b) {
        return std::less<const Game*>{}(a.copies.front(), b.copies.front());
    });
    return titles;
}

std::vector<Game> copiesOf(const std::vector<Game>& games, const Game& game) {
    for (const Title& title : gameTitles(games)) {
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
    for (const Title& title : gameTitles(games)) {
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
    std::vector<Title> titles = gameTitles(games);
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

TitleCopies::TitleCopies(const std::vector<Game>& games) : titles_{gameTitles(games)} {
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
        if (a->inExtras != b->inExtras || a->versionDefault != b->versionDefault ||
            a->installed != b->installed ||
            (a->source == Source::Remote) != (b->source == Source::Remote)) {
            return before(*a, *b);
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
    if (copies.size() < 2) {
        return {};
    }
    std::vector<Game> out;
    out.reserve(copies.size());
    for (const Game* copy : copies) {
        out.push_back(*copy);
    }
    return out;
}

} // namespace opensu::library
