#include "shelf.hpp"

#include <algorithm>
#include <array>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "rom_systems.hpp"
#include "titles.hpp"

namespace opensu::library {
namespace {

/// The order the launchers sit in in Library, as the top bar's badges do, the player's own last.
constexpr std::array<Source, 4> launcherOrder{Source::Steam, Source::Epic, Source::Gog,
                                              Source::Own};
/// The order store icons are drawn in.
constexpr std::array<Source, 3> iconOrder{Source::Steam, Source::Gog, Source::Epic};

bool isRomOf(const Game& game, std::string_view system) {
    return game.source == Source::Rom && game.sourceId == system;
}

bool holds(const Launcher& launcher, const Game& game) {
    return game.source == launcher.source &&
           (launcher.source != Source::Remote || game.hostId == launcher.hostId);
}

/// One launcher per openSU on the LAN with games listed, in the order their games come.
std::vector<Launcher> hostLaunchers(const std::vector<Game>& games, bool ready) {
    std::vector<Launcher> hosts;
    for (const Game& game : games) {
        if (game.source != Source::Remote) {
            continue;
        }
        const auto known = std::ranges::find(hosts, game.hostId, &Launcher::hostId);
        if (known != hosts.end()) {
            ++known->games;
            continue;
        }
        hosts.push_back(Launcher{.source = Source::Remote,
                                 .games = 1,
                                 .ready = ready,
                                 .loading = false,
                                 .hostId = game.hostId,
                                 .hostName = game.hostName});
    }
    std::ranges::sort(hosts, [](const Launcher& a, const Launcher& b) {
        return std::tie(a.hostName, a.hostId) < std::tie(b.hostName, b.hostId);
    });
    return hosts;
}

/// The stores of `title`, in icon order, without `leaving`.
std::vector<Source> storesOf(const Title& title, std::optional<Source> leaving) {
    std::vector<Source> stores;
    for (const Source source : iconOrder) {
        const bool owned = std::ranges::any_of(title.copies, [source](const Game* copy) {
            return copy->source == source;
        });
        if (owned && source != leaving) {
            stores.push_back(source);
        }
    }
    return stores;
}

/// The title's preferred copy, as a shelf entry that lists the stores it is owned in.
Game entry(const Title& title, const Game& copy, std::optional<Source> leaving) {
    Game out = copy;
    out.ownedIn = storesOf(title, leaving);
    return out;
}

std::vector<ShelfItem> titleShelf(const std::vector<Game>& games, bool installedOnly) {
    std::vector<ShelfItem> shelf;
    for (const Title& title : storeTitles(games)) {
        const Game& preferred = *title.copies.front();
        if (!installedOnly || (preferred.installed && preferred.source != Source::Remote)) {
            shelf.emplace_back(entry(title, *title.copies.front(), std::nullopt));
        }
    }
    return shelf;
}

std::size_t countOf(const std::vector<Game>& games, Source source) {
    return static_cast<std::size_t>(std::ranges::count(games, source, &Game::source));
}

} // namespace

std::optional<Folder> folderOf(const ShelfItem& item) {
    if (const auto* console = std::get_if<Console>(&item)) {
        return *console;
    }
    if (const auto* launcher = std::get_if<Launcher>(&item)) {
        return *launcher;
    }
    if (const auto* all = std::get_if<AllGames>(&item)) {
        return *all;
    }
    return std::nullopt;
}

std::string key(const Folder& folder) {
    struct Visitor {
        std::string operator()(const Console& console) const {
            return console.system;
        }
        std::string operator()(const Launcher& launcher) const {
            const std::string store = "launcher:" + library::key(launcher.source);
            return launcher.hostId.empty() ? store : store + ":" + launcher.hostId;
        }
        std::string operator()(const AllGames&) const {
            return "all";
        }
    };
    return std::visit(Visitor{}, folder);
}

std::string name(const Folder& folder) {
    struct Visitor {
        std::string operator()(const Console& console) const {
            return console.label;
        }
        std::string operator()(const Launcher& launcher) const {
            return launcher.hostName.empty() ? std::string{label(launcher.source)}
                                             : launcher.hostName;
        }
        std::string operator()(const AllGames&) const {
            return "All games";
        }
    };
    return std::visit(Visitor{}, folder);
}

std::vector<Console> consoles(const std::vector<Game>& games) {
    std::vector<Console> out;
    for (const roms::RomSystem& system : roms::romSystems()) {
        const auto count =
            static_cast<std::size_t>(std::ranges::count_if(games, [&system](const Game& game) {
                return isRomOf(game, system.key);
            }));
        if (count > 0) {
            out.push_back(Console{std::string{system.key}, std::string{system.label}, count});
        }
    }
    return out;
}

std::vector<ShelfItem> homeShelf(const std::vector<Game>& games) {
    return titleShelf(games, true);
}

std::vector<ShelfItem> libraryShelf(const std::vector<Game>& games,
                                    const std::vector<SourceStatus>& sources,
                                    const std::vector<AddTile>& adds) {
    std::vector<ShelfItem> shelf;
    for (const Source source : launcherOrder) {
        const auto status = std::ranges::find(sources, source, &SourceStatus::source);
        if (status != sources.end() && status->availability != Availability::Absent) {
            shelf.emplace_back(Launcher{source, countOf(games, source),
                                        status->availability == Availability::Ready,
                                        status->availability == Availability::Loading});
        }
    }
    if (const auto lan = std::ranges::find(sources, Source::Remote, &SourceStatus::source);
        lan != sources.end()) {
        for (Launcher& host : hostLaunchers(games, lan->availability == Availability::Ready)) {
            shelf.emplace_back(std::move(host));
        }
    }
    if (std::ranges::any_of(shelf, [](const ShelfItem& item) {
            const Source source = std::get<Launcher>(item).source;
            return source != Source::Own && source != Source::Remote;
        })) {
        shelf.emplace_back(Giveaways{});
    }
    shelf.insert(shelf.end(), adds.begin(), adds.end());
    const std::vector<Title> titles = storeTitles(games);
    if (!titles.empty()) {
        shelf.emplace_back(AllGames{titles.size()});
    }
    for (Console& console : consoles(games)) {
        shelf.emplace_back(std::move(console));
    }
    return shelf;
}

std::vector<ShelfItem> consoleShelf(const std::vector<Game>& games, std::string_view system) {
    std::vector<ShelfItem> shelf;
    for (const Game& game : games) {
        if (isRomOf(game, system)) {
            shelf.emplace_back(game);
        }
    }
    return shelf;
}

std::vector<ShelfItem> launcherShelf(const std::vector<Game>& games, const Launcher& launcher) {
    const std::vector<Title> titles = storeTitles(games);
    std::unordered_map<std::string, const Title*> byGame;
    for (const Title& title : titles) {
        for (const Game* copy : title.copies) {
            byGame[copy->id] = &title;
        }
    }
    std::vector<ShelfItem> shelf;
    for (const Game& game : games) {
        if (holds(launcher, game)) {
            shelf.emplace_back(entry(*byGame.at(game.id), game, launcher.source));
        }
    }
    return shelf;
}

std::vector<ShelfItem> allGamesShelf(const std::vector<Game>& games) {
    return titleShelf(games, false);
}

std::vector<ShelfItem> folderShelf(const std::vector<Game>& games, const Folder& folder) {
    struct Visitor {
        const std::vector<Game>& games;
        std::vector<ShelfItem> operator()(const Console& console) const {
            return consoleShelf(games, console.system);
        }
        std::vector<ShelfItem> operator()(const Launcher& launcher) const {
            return launcherShelf(games, launcher);
        }
        std::vector<ShelfItem> operator()(const AllGames&) const {
            return allGamesShelf(games);
        }
    };
    return std::visit(Visitor{games}, folder);
}

std::vector<ShelfItem> ShelfBrowser::shelf(const std::vector<Game>& games,
                                           const std::vector<SourceStatus>& sources,
                                           const std::vector<AddTile>& adds) {
    if (folder_) {
        std::vector<ShelfItem> inside = folderShelf(games, *folder_);
        if (!inside.empty()) {
            return inside;
        }
        returned_ = back();
    }
    return section() == Section::Home ? homeShelf(games) : libraryShelf(games, sources, adds);
}

void ShelfBrowser::open(const Folder& folder, std::size_t sectionFocus) {
    folder_ = folder;
    parentFocus_ = sectionFocus;
}

std::optional<std::size_t> ShelfBrowser::back() {
    if (!canBack()) {
        return std::nullopt;
    }
    folder_.reset();
    return parentFocus_;
}

void ShelfBrowser::leave(std::size_t focus) {
    left_[static_cast<std::size_t>(section())] = folder_ ? parentFocus_ : focus;
}

std::size_t ShelfBrowser::cycle(int delta) {
    folder_.reset();
    return left_[static_cast<std::size_t>(sections_.cycle(delta))];
}

} // namespace opensu::library
