#include "shelf.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
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
    return game.source == Source::Rom && game.sourceId == system && !game.inExtras;
}

bool holds(const Launcher& launcher, const Game& game) {
    return game.source == launcher.source && !game.inExtras &&
           (launcher.source != Source::Remote || game.hostId == launcher.hostId);
}

/// One launcher per openSU on the LAN with games listed, in the order their games come.
std::vector<Launcher> hostLaunchers(const std::vector<Game>& games, bool ready) {
    std::vector<Launcher> hosts;
    for (const Game& game : games) {
        if (game.source != Source::Remote) {
            continue;
        }
        const std::string playing = game.playingAs.empty() ? std::string{} : game.title;
        const auto known = std::ranges::find(hosts, game.hostId, &Launcher::hostId);
        if (known != hosts.end()) {
            ++known->games;
            if (!playing.empty()) {
                known->playing = playing;
            }
            continue;
        }
        hosts.push_back(Launcher{.source = Source::Remote,
                                 .games = 1,
                                 .ready = ready,
                                 .loading = false,
                                 .hostId = game.hostId,
                                 .hostName = game.hostName,
                                 .online = game.hostOnline,
                                 .playing = playing});
    }
    std::ranges::sort(hosts, [](const Launcher& a, const Launcher& b) {
        return std::tie(a.hostName, a.hostId) < std::tie(b.hostName, b.hostId);
    });
    return hosts;
}

/// The stores of `title`, in icon order.
std::vector<Source> storesOf(const Title& title) {
    std::vector<Source> stores;
    for (const Source source : iconOrder) {
        const bool owned = std::ranges::any_of(title.copies, [source](const Game* copy) {
            return copy->source == source;
        });
        if (owned) {
            stores.push_back(source);
        }
    }
    return stores;
}

/// `copy` as an entry standing for `title` across stores, listing the stores it is owned in.
Game entry(const Title& title, const Game& copy) {
    Game out = copy;
    out.ownedIn = storesOf(title);
    return out;
}

/// What a game of `versions` (preferred first) is kept under in `HomeChoices`, its first version
/// that is no ROM or else its preferred, so a change of default keeps it; and whether it is on Home
/// by default, as any of its versions not in Extras is.
std::pair<std::string, bool> homeSlot(std::span<const Game* const> versions) {
    const auto named = std::ranges::find_if(versions, [](const Game* version) {
        return version->source != Source::Rom;
    });
    const bool byDefault = std::ranges::any_of(versions, [](const Game* version) {
        return !version->inExtras && onHomeByDefault(*version);
    });
    return {choiceKey(**(named == versions.end() ? versions.begin() : named)), byDefault};
}

/// What `item` is kept under in `HomeChoices`, and whether it is on Home by default.
std::optional<std::pair<std::string, bool>> homeSlotOf(const std::vector<Game>& games,
                                                       const ShelfItem& item) {
    if (const auto* game = std::get_if<Game>(&item)) {
        const std::vector<Game> copies = copiesOf(games, *game);
        std::vector<const Game*> versions;
        versions.reserve(copies.size());
        for (const Game& copy : copies) {
            versions.push_back(&copy);
        }
        return homeSlot(versions);
    }
    if (const auto* console = std::get_if<Console>(&item)) {
        return std::pair{HomeChoices::consoleKey(console->system), consoleOnHomeByDefault};
    }
    return std::nullopt;
}

std::size_t countOf(const std::vector<Game>& games, Source source) {
    return static_cast<std::size_t>(std::ranges::count_if(games, [source](const Game& game) {
        return game.source == source && !game.inExtras;
    }));
}

/// Whether `title` is one of All games: a game with a version that is no ROM here.
bool inAllGames(const Title& title) {
    return std::ranges::any_of(title.copies, [](const Game* copy) {
        return copy->source != Source::Rom;
    });
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

namespace {

/// The games on this Home, in its order, each as its title.
std::vector<const Title*> homeTitles(const std::vector<Game>& games,
                                     const std::vector<Title>& titles, const HomeChoices& choices) {
    std::unordered_map<const Game*, const Title*> preferredOf;
    for (const Title& title : titles) {
        preferredOf.emplace(title.copies.front(), &title);
    }
    std::vector<const Title*> home;
    for (const Game& game : games) {
        const auto title = preferredOf.find(&game);
        if (title == preferredOf.end()) {
            continue;
        }
        if (const auto [key, byDefault] = homeSlot(title->second->copies);
            choices.holds(key, byDefault)) {
            home.push_back(title->second);
        }
    }
    return home;
}

/// Each paired openSU's Home after this one's: its launcher, then the games on its Home in its
/// order, as their titles, leaving out the titles in `shown`.
void appendPairedHomes(std::vector<ShelfItem>& shelf, const std::vector<Game>& games,
                       const std::vector<Title>& titles, const std::vector<SourceStatus>& sources,
                       std::unordered_set<const Title*> shown) {
    const auto lan = std::ranges::find(sources, Source::Remote, &SourceStatus::source);
    if (lan == sources.end()) {
        return;
    }
    std::unordered_map<const Game*, const Title*> titleOf;
    for (const Title& title : titles) {
        for (const Game* copy : title.copies) {
            titleOf.emplace(copy, &title);
        }
    }
    for (Launcher& host : hostLaunchers(games, lan->availability == Availability::Ready)) {
        std::vector<const Game*> listed;
        for (const Game& game : games) {
            if (game.source == Source::Remote && game.hostId == host.hostId && game.remote.home) {
                listed.push_back(&game);
            }
        }
        std::ranges::stable_sort(listed, {}, [](const Game* game) {
            return *game->remote.home;
        });
        std::vector<ShelfItem> section;
        for (const Game* game : listed) {
            const auto title = titleOf.find(game);
            if (title != titleOf.end() && shown.insert(title->second).second) {
                section.emplace_back(entry(*title->second, *title->second->copies.front()));
            }
        }
        if (!section.empty()) {
            shelf.emplace_back(std::move(host));
            std::ranges::move(section, std::back_inserter(shelf));
        }
    }
}

} // namespace

std::vector<std::string> homeGameIds(const std::vector<Game>& games, const HomeChoices& choices) {
    std::vector<std::string> ids;
    const std::vector<Title> titles = gameTitles(games);
    for (const Title* title : homeTitles(games, titles, choices)) {
        // Its version here: another openSU's is that one's to list.
        const auto here = std::ranges::find_if(title->copies, [](const Game* copy) {
            return copy->source != Source::Remote;
        });
        if (here != title->copies.end()) {
            ids.push_back((*here)->id);
        }
    }
    return ids;
}

const Game* playingOn(const std::vector<Game>& games, std::string_view hostId) {
    const auto found = std::ranges::find_if(games, [hostId](const Game& game) {
        return game.source == Source::Remote && game.hostId == hostId && !game.playingAs.empty();
    });
    return found == games.end() ? nullptr : &*found;
}

std::vector<ShelfItem> homeShelf(const std::vector<Game>& games,
                                 const std::vector<SourceStatus>& sources,
                                 const std::vector<AddTile>& adds, const HomeChoices& choices) {
    std::vector<ShelfItem> shelf;
    for (Console& console : consoles(games)) {
        if (choices.holds(HomeChoices::consoleKey(console.system), consoleOnHomeByDefault)) {
            shelf.emplace_back(std::move(console));
        }
    }
    const std::vector<Title> titles = gameTitles(games);
    const std::vector<const Title*> home = homeTitles(games, titles, choices);
    for (const Title* title : home) {
        shelf.emplace_back(entry(*title, *title->copies.front()));
    }
    const bool loading = std::ranges::any_of(sources, [](const SourceStatus& status) {
        return status.availability == Availability::Loading;
    });
    if (shelf.empty() && !loading) {
        shelf.assign(adds.begin(), adds.end());
    }
    appendPairedHomes(shelf, games, titles, sources, {home.begin(), home.end()});
    return shelf;
}

std::optional<bool> onHome(const std::vector<Game>& games, const ShelfItem& item,
                           const HomeChoices& choices) {
    const auto slot = homeSlotOf(games, item);
    if (!slot) {
        return std::nullopt;
    }
    return choices.holds(slot->first, slot->second);
}

void setOnHome(const std::vector<Game>& games, const ShelfItem& item, bool on,
               HomeChoices& choices) {
    if (const auto slot = homeSlotOf(games, item)) {
        choices.set(slot->first, slot->second, on);
    }
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
    const std::vector<Title> titles = gameTitles(games);
    if (const auto all = static_cast<std::size_t>(std::ranges::count_if(titles, inAllGames));
        all > 0) {
        shelf.emplace_back(AllGames{all});
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
    std::vector<ShelfItem> shelf;
    for (const Game& game : games) {
        if (holds(launcher, game)) {
            shelf.emplace_back(game);
        }
    }
    return shelf;
}

std::vector<ShelfItem> allGamesShelf(const std::vector<Game>& games) {
    std::vector<ShelfItem> shelf;
    for (const Title& title : gameTitles(games)) {
        if (inAllGames(title)) {
            shelf.emplace_back(entry(title, *title.copies.front()));
        }
    }
    return shelf;
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
                                           const std::vector<AddTile>& adds,
                                           const HomeChoices& home) {
    if (folder_) {
        std::vector<ShelfItem> inside = folderShelf(games, *folder_);
        if (!inside.empty()) {
            return inside;
        }
        returned_ = back();
    }
    return section() == Section::Home ? homeShelf(games, sources, adds, home)
                                      : libraryShelf(games, sources, adds);
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
