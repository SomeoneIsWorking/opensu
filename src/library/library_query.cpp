#include "library_query.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

#include "rom_systems.hpp"
#include "text_fold.hpp"
#include "titles.hpp"

namespace opensu::library {
namespace {

/// The folder key of the store or system `game` belongs to.
std::string sourceKeyOf(const Game& game) {
    if (game.source == Source::Rom) {
        return game.sourceId;
    }
    return key(Folder{Launcher{game.source, 0, true, false}});
}

int storeRank(Source source) noexcept {
    return static_cast<int>(source);
}

/// Orders `games` by `sort`; `Recent` is the catalog's own order.
void sortGames(std::vector<Game>& games, SortKey sort) {
    if (sort == SortKey::Recent) {
        order(games);
        return;
    }
    std::vector<std::string> names;
    names.reserve(games.size());
    for (const Game& game : games) {
        names.push_back(foldText(game.title));
    }
    std::vector<std::size_t> at(games.size());
    std::iota(at.begin(), at.end(), std::size_t{0});
    std::ranges::stable_sort(at, [&](std::size_t a, std::size_t b) {
        if (sort == SortKey::Store) {
            if (games[a].source != games[b].source) {
                return storeRank(games[a].source) < storeRank(games[b].source);
            }
            if (games[a].sourceId != games[b].sourceId) {
                return games[a].sourceId < games[b].sourceId;
            }
        }
        return names[a] < names[b];
    });
    std::vector<Game> sorted;
    sorted.reserve(games.size());
    for (const std::size_t index : at) {
        sorted.push_back(std::move(games[index]));
    }
    games = std::move(sorted);
}

/// How well `folded` (a name, folded) matches the folded `needle`: 0 begins with it, 1 has a word
/// that does, 2 holds it, or nothing for no match.
std::optional<int> matchRank(const std::string& folded, const std::string& needle) {
    const std::size_t first = folded.find(needle);
    if (first == std::string::npos) {
        return std::nullopt;
    }
    if (first == 0) {
        return 0;
    }
    for (std::size_t at = first; at != std::string::npos; at = folded.find(needle, at + 1)) {
        if (folded[at - 1] == ' ') {
            return 1;
        }
    }
    return 2;
}

/// A result and how it ranks.
struct Hit {
    ShelfItem item;
    int rank;
    bool folder;
};

} // namespace

std::string_view key(SortKey sort) noexcept {
    switch (sort) {
    case SortKey::Recent:
        return "recent";
    case SortKey::Name:
        return "name";
    case SortKey::Store:
        return "store";
    }
    return "recent";
}

std::optional<SortKey> sortKeyOf(std::string_view spelling) noexcept {
    for (const SortKey sort : allSortKeys) {
        if (key(sort) == spelling) {
            return sort;
        }
    }
    return std::nullopt;
}

std::string_view label(SortKey sort) noexcept {
    switch (sort) {
    case SortKey::Recent:
        return "Recently played";
    case SortKey::Name:
        return "Name";
    case SortKey::Store:
        return "Store";
    }
    return "Recently played";
}

HiddenGames::HiddenGames(std::vector<std::string> keys) : keys_{std::move(keys)} {
    std::ranges::sort(keys_);
    keys_.erase(std::ranges::unique(keys_).begin(), keys_.end());
}

std::string HiddenGames::keyOf(const Game& game) {
    if (game.source != Source::Rom) {
        if (const std::string title = titleKey(game.title); !title.empty()) {
            return "title:" + title;
        }
    }
    return "id:" + game.id;
}

bool HiddenGames::contains(const Game& game) const {
    return std::ranges::binary_search(keys_, keyOf(game));
}

void HiddenGames::set(const Game& game, bool hidden) {
    const std::string wanted = keyOf(game);
    const auto at = std::ranges::lower_bound(keys_, wanted);
    const bool present = at != keys_.end() && *at == wanted;
    if (hidden && !present) {
        keys_.insert(at, wanted);
    } else if (!hidden && present) {
        keys_.erase(at);
    }
}

std::vector<SourceChoice> sourceChoices(const std::vector<Game>& games,
                                        const std::vector<SourceStatus>& sources) {
    std::vector<SourceChoice> choices{SourceChoice{"", "All sources"}};
    for (const ShelfItem& item : libraryShelf(games, sources)) {
        const std::optional<Folder> folder = folderOf(item);
        if (folder && !std::holds_alternative<AllGames>(*folder)) {
            choices.push_back(SourceChoice{key(*folder), name(*folder)});
        }
    }
    return choices;
}

bool narrowing(const ViewOptions& options) noexcept {
    return options.installedOnly || !options.source.empty() || options.hiddenOnly ||
           !options.search.empty();
}

LibraryView applyOptions(const std::vector<Game>& games, const std::vector<SourceStatus>& sources,
                         const ViewOptions& options, const HiddenGames& hidden) {
    LibraryView view;
    for (const Game& game : games) {
        if (options.installedOnly && !game.installed) {
            continue;
        }
        if (!options.source.empty() && sourceKeyOf(game) != options.source) {
            continue;
        }
        if (hidden.contains(game) != options.hiddenOnly) {
            continue;
        }
        view.games.push_back(game);
    }
    sortGames(view.games, options.sort);
    view.sources = sources;
    if (!options.source.empty()) {
        for (SourceStatus& status : view.sources) {
            if (key(Folder{Launcher{status.source, 0, true, false}}) != options.source) {
                status.availability = Availability::Absent;
            }
        }
    }
    return view;
}

std::vector<ShelfItem> searchShelf(const LibraryView& view, std::string_view text) {
    const std::string needle = foldText(text);
    std::vector<Hit> hits;
    if (needle.empty()) {
        return {};
    }
    const auto consider = [&](ShelfItem item, const std::string& title, bool folder) {
        if (const std::optional<int> rank = matchRank(foldText(title), needle)) {
            hits.push_back(Hit{std::move(item), *rank, folder});
        }
    };
    for (ShelfItem& item : libraryShelf(view.games, view.sources)) {
        const std::optional<Folder> folder = folderOf(item);
        consider(item, folder ? name(*folder) : std::string{}, true);
    }
    for (ShelfItem& item : allGamesShelf(view.games)) {
        consider(item, std::get<Game>(item).title, false);
    }
    for (const Game& game : view.games) {
        if (game.source == Source::Rom) {
            consider(game, game.title, false);
        }
    }
    std::ranges::stable_sort(hits, [](const Hit& a, const Hit& b) {
        if (a.rank != b.rank) {
            return a.rank < b.rank;
        }
        return a.folder && !b.folder;
    });
    std::vector<ShelfItem> out;
    out.reserve(hits.size());
    for (Hit& hit : hits) {
        out.push_back(std::move(hit.item));
    }
    return out;
}

std::string describeResult(const ShelfItem& item) {
    if (const auto* game = std::get_if<Game>(&item)) {
        if (game->source == Source::Rom) {
            const roms::RomSystem* system = roms::systemByKey(game->sourceId);
            return system != nullptr ? std::string{system->label} : game->sourceId;
        }
        std::string stores;
        for (const Source source : game->ownedIn) {
            stores += (stores.empty() ? "" : " \xC2\xB7 ") + std::string{label(source)};
        }
        return stores.empty() ? std::string{label(game->source)} : stores;
    }
    if (std::holds_alternative<Console>(item)) {
        return "Console";
    }
    return std::holds_alternative<Launcher>(item) ? "Store" : "Library";
}

std::vector<ShelfItem> visibleShelf(ShelfBrowser& browser, const std::vector<Game>& games,
                                    const std::vector<SourceStatus>& sources,
                                    const ViewOptions& options, const HiddenGames& hidden) {
    const LibraryView view = applyOptions(games, sources, options, hidden);
    if (!options.search.empty()) {
        return searchShelf(view, options.search);
    }
    return browser.shelf(view.games, view.sources);
}

} // namespace opensu::library
