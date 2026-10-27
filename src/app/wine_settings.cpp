#include "wine_settings.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <system_error>
#include <utility>

#include "library/ge_proton.hpp"
#include "library/lutris_wine.hpp"
#include "library/steam.hpp"
#include "library/steam_compat.hpp"
#include "lucent/log.h"
#include "steam_library_settings.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using ui::RowKind;

constexpr const char* runnerId = "wine.runner";
constexpr const char* keepId = "wine.keep";
constexpr const char* checkId = "wine.check";
constexpr const char* testId = "wine.test";
constexpr const char* prefixId = "wine.prefix";
constexpr const char* ownId = "wine.own";
constexpr const char* steamId = "wine.steam";
/// A runtime's page: `wine.runtime:` and the runner's Wine binary, or `umu` or `steamrt`.
constexpr std::string_view runtimePage = "wine.runtime:";
constexpr std::string_view umuKey = "umu";
constexpr std::string_view steamRuntimeKey = "steamrt";
/// `wine.pin:<game id>\t<Wine binary>`; an empty binary unpins.
constexpr std::string_view pinId = "wine.pin:";
constexpr const char* testProgram[] = {"cmd", "/c", "ver"};

std::string countText(std::size_t count) {
    return std::to_string(count) + (count == 1 ? " game" : " games");
}

constexpr library::Source storeOrder[] = {library::Source::Steam, library::Source::Gog,
                                          library::Source::Epic};

/// How many games use a runner by store: "3 games from Steam, 1 from Epic", or "Not used".
std::string usageText(const std::vector<library::Game>& games) {
    std::string text;
    for (const library::Source source : storeOrder) {
        const auto count =
            static_cast<std::size_t>(std::ranges::count(games, source, &library::Game::source));
        if (count == 0) {
            continue;
        }
        text += text.empty() ? countText(count) : ", " + std::to_string(count);
        text += " from " + std::string{library::label(source)};
    }
    return text.empty() ? "Not used" : text;
}

/// `games` of one store.
std::vector<library::Game> fromStore(std::vector<library::Game> games, library::Source source) {
    std::erase_if(games, [source](const library::Game& game) {
        return game.source != source;
    });
    return games;
}

fs::path realPath(const fs::path& dir) {
    std::error_code ec;
    fs::path real = fs::weakly_canonical(dir, ec);
    return ec ? dir : real;
}

/// The installed Steam games among `games` that Steam runs under a tool, by the tool's folder.
std::map<fs::path, std::vector<library::Game>>
steamGamesByTool(const library::steam::Library& steam, std::vector<library::Game> games) {
    std::erase_if(games, [](const library::Game& game) {
        return game.source != library::Source::Steam || !game.installed;
    });
    std::vector<std::string> apps;
    apps.reserve(games.size());
    for (const library::Game& game : games) {
        apps.push_back(game.sourceId);
    }
    const auto tools = library::steam::compatTools(steam, apps);
    std::map<fs::path, std::vector<library::Game>> byTool;
    for (library::Game& game : games) {
        if (const auto tool = tools.find(game.sourceId);
            tool != tools.end() && !tool->second.dir.empty()) {
            byTool[realPath(tool->second.dir)].push_back(std::move(game));
        }
    }
    return byTool;
}

/// Where a runner lives, as the player reads it.
std::string whereOf(const library::wine::Runner& runner) {
    switch (runner.origin) {
    case library::wine::Origin::OpenSu:
        return "Downloaded by openSU";
    case library::wine::Origin::Steam:
        return "From Steam";
    case library::wine::Origin::Lutris:
        return "From Lutris";
    case library::wine::Origin::System:
        break;
    }
    return "Installed on the system";
}

bool isManaged(const library::wine::Runner& runner) {
    return runner.origin == library::wine::Origin::OpenSu;
}

} // namespace

library::wine::Search WineSettings::search(const library::steam::Library* steam) const {
    library::wine::Search search{.executablePath = config_.executablePath,
                                 .steamRoots = {},
                                 .steamLibraries = {},
                                 .managed = library::ge_proton::runnersDir(config_.dataDir),
                                 .lutris = library::lutris::dataDir(config_.xdgDataHome)};
    if (steam == nullptr) {
        return search;
    }
    search.steamRoots = steam->roots();
    for (const library::steam::LibraryFolder& folder : steam->libraryFolders()) {
        search.steamLibraries.push_back(folder.path);
    }
    return search;
}

void WineSettings::rescan() {
    std::optional<library::steam::Library> steam;
    if (integrations_.lists(library::Source::Steam)) {
        steam = library::steam::Library::discover(config_.home, config_.steamRoots);
    }
    const library::wine::Search where = search(steam ? &*steam : nullptr);
    found_ = library::wine::discover(where);
    lutris_ = library::wine::lutrisDefault(where.lutris, found_);
    umuRun_ = library::wine::umuRunOf(runners_.umuRun(), where);
    steamOn_ = steam ? steamGamesByTool(*steam, hooks_.games())
                     : std::map<fs::path, std::vector<library::Game>>{};
}

library::wine::Choice WineSettings::choice() const {
    const settings::Settings& v = preferences_.values();
    return library::wine::Choice{.runner = v.wineRunner,
                                 .pins = v.wineRunnerPins,
                                 .found = found_,
                                 .lutris = lutris_,
                                 .dataDir = config_.dataDir,
                                 .own = v.ownWinePrefixes,
                                 .umuRun = umuRun_};
}

std::vector<library::Game> WineSettings::windowsGames() const {
    std::vector<library::Game> games = hooks_.games();
    std::erase_if(games, [](const library::Game& game) {
        return !game.windowsInstall ||
               (game.source != library::Source::Gog && game.source != library::Source::Epic);
    });
    return games;
}

std::vector<library::Game> WineSettings::gamesOn(const fs::path& wine) const {
    const library::wine::Choice resolved = choice();
    std::vector<library::Game> games = windowsGames();
    std::erase_if(games, [&resolved, &wine](const library::Game& game) {
        const std::optional<library::wine::Runner> runner =
            library::wine::runnerOf(resolved, game.id);
        return !runner || (wine.empty() ? runner->proton.empty() : runner->wine != wine);
    });
    return games;
}

std::vector<library::Game> WineSettings::steamGamesOn(const library::wine::Runner& runner) const {
    if (runner.proton.empty()) {
        return {};
    }
    const auto found = steamOn_.find(realPath(runner.proton));
    return found != steamOn_.end() ? found->second : std::vector<library::Game>{};
}

WineRunners::InUse WineSettings::inUse() const {
    const fs::path runners = library::ge_proton::runnersDir(config_.dataDir);
    WineRunners::InUse inUse;
    const settings::Settings& v = preferences_.values();
    const auto buildOf = [&runners](const fs::path& wine) {
        const fs::path build = wine.parent_path().parent_path().parent_path();
        return library::ge_proton::managed(runners, build) ? build : fs::path{};
    };
    if (const fs::path picked = buildOf(v.wineRunner); !picked.empty()) {
        inUse[picked] = "Kept because it runs every game";
    }
    std::map<fs::path, std::size_t> pinned;
    for (const auto& [game, wine] : v.wineRunnerPins) {
        if (const fs::path build = buildOf(wine); !build.empty()) {
            ++pinned[build];
        }
    }
    for (const auto& [build, count] : pinned) {
        inUse[build] =
            "Kept because " + countText(count) + (count == 1 ? " is" : " are") + " pinned to it";
    }
    if (!launched_.empty() && hooks_.gameRunning()) {
        inUse[launched_] = "Kept because a game is running on it";
    }
    return inUse;
}

ui::SettingsCategory WineSettings::page() const {
    const settings::Settings& v = preferences_.values();
    const WineRunners::Look look = runners_.look();
    ui::SettingsCategory page{std::string{categoryId}, "Wine", {}};

    // Choice 0 is Automatic, then each runner found, then a pick not found.
    ui::Options runners;
    library::wine::Choice automatic = choice();
    automatic.runner.clear();
    automatic.pins.clear();
    const std::optional<library::wine::Runner> now = library::wine::runnerOf(automatic, {});
    runners.labels.push_back(now ? "Automatic (" + library::wine::displayName(now->name) + ")"
                                 : "Automatic (none installed)");
    for (const library::wine::Runner& runner : found_) {
        runners.labels.push_back(library::wine::displayName(runner.name));
    }
    if (!v.wineRunner.empty()) {
        const auto at = std::ranges::find(found_, v.wineRunner, &library::wine::Runner::wine);
        if (at == found_.end()) {
            runners.labels.emplace_back("Missing runner");
            runners.chosen = runners.labels.size() - 1;
        } else {
            runners.chosen = static_cast<std::size_t>(at - found_.begin()) + 1;
        }
    }
    page.rows.push_back(ui::makeChoice(
        runnerId, "Wine runner", "What Windows GOG and Epic games run under", std::move(runners)));

    std::string status = look.lastCheck.empty() ? "Not checked yet" : look.lastCheck;
    if (const std::optional<WineRunners::Progress> running = runners_.progress()) {
        if (running->stage == library::ArchiveStage::Checking) {
            status = "Checking " + running->name;
        } else if (running->stage == library::ArchiveStage::Unpacking) {
            status = "Unpacking " + running->name;
        } else {
            status = "Downloading " + running->name;
            if (running->total > 0) {
                status += " · " + std::to_string(running->received * 100 / running->total) + "%";
            }
        }
    } else if (look.checking) {
        status = "Checking for updates…";
    }
    page.rows.push_back(ui::makeToggle(keepId, "Keep GE-Proton up to date",
                                       "Downloads the newest in the background and removes all "
                                       "but the newest two",
                                       v.keepGeProtonCurrent));
    page.rows.push_back(ui::makeRow(checkId, RowKind::Action, "Check for GE-Proton now",
                                    v.keepGeProtonCurrent ? "Also checked at start and every day"
                                                          : "Downloads only when pressed",
                                    status));
    page.rows.push_back(ui::makeRow(testId, RowKind::Action, "Test the runner",
                                    "Starts a small Windows program with the runner",
                                    look.testing            ? "Running…"
                                    : look.lastTest.empty() ? "Not run yet"
                                                            : look.lastTest));

    page.rows.push_back(ui::makeHeading("Runners and runtimes"));
    const auto sizeOf = [&look](const fs::path& dir) -> std::string {
        const auto piece = std::ranges::find(look.builds, dir, &WineRunners::Piece::dir);
        return piece != look.builds.end() && piece->bytes > 0 ? " · " + sizeText(piece->bytes)
                                                              : std::string{};
    };
    for (const library::wine::Runner& runner : found_) {
        std::string note = whereOf(runner) + sizeOf(runner.proton);
        if (const auto kept = look.kept.find(runner.name);
            isManaged(runner) && kept != look.kept.end()) {
            note = kept->second;
        }
        std::vector<library::Game> using_ = steamGamesOn(runner);
        std::ranges::move(gamesOn(runner.wine), std::back_inserter(using_));
        page.rows.push_back(ui::makePage(std::string{runtimePage} + runner.wine.string(),
                                         library::wine::displayName(runner.name), note,
                                         usageText(using_)));
    }
    const std::string onProton = usageText(gamesOn({}));
    if (look.umu) {
        page.rows.push_back(ui::makePage(
            std::string{runtimePage} + std::string{umuKey}, look.umu->name,
            "Runs Proton games outside Steam · " + sizeText(look.umu->bytes), onProton));
    }
    if (look.steamRuntime) {
        page.rows.push_back(ui::makePage(
            std::string{runtimePage} + std::string{steamRuntimeKey}, look.steamRuntime->name,
            "The container Proton games run in · " + sizeText(look.steamRuntime->bytes), onProton));
    }

    page.rows.push_back(ui::makeHeading("Prefixes"));
    page.rows.push_back(ui::makeRow(prefixId, RowKind::Info, "Shared Wine prefix",
                                    "Every Windows game without its own prefix",
                                    library::wine::sharedPrefix(config_.dataDir).string()));
    const std::size_t own = v.ownWinePrefixes.ids().size();
    page.rows.push_back(ui::makeRow(ownId, RowKind::Info, "Own Wine prefixes",
                                    "Set per game on its details page",
                                    own == 0 ? "None" : countText(own)));
    if (integrations_.lists(library::Source::Steam)) {
        page.rows.push_back(ui::makeRow(steamId, RowKind::Info, "Steam games",
                                        "Steam runs them with the tool picked in Steam",
                                        "Left to Steam"));
    }
    return page;
}

std::optional<ui::SettingsCategory> WineSettings::subPage(const std::string& id) const {
    if (!id.starts_with(runtimePage)) {
        return std::nullopt;
    }
    const std::string key = id.substr(runtimePage.size());
    const WineRunners::Look look = runners_.look();
    ui::SettingsCategory page{id, "", {}};
    if (key == umuKey || key == steamRuntimeKey) {
        const std::optional<WineRunners::Piece>& piece =
            key == umuKey ? look.umu : look.steamRuntime;
        if (!piece) {
            return page;
        }
        page.label = piece->name;
        page.rows.push_back(ui::makeRow("wine.where", RowKind::Info, "Where",
                                        "Downloaded by openSU", piece->dir.string()));
        page.rows.push_back(
            ui::makeRow("wine.size", RowKind::Info, "Size", "On disk", sizeText(piece->bytes)));
        const std::vector<library::Game> onProton = gamesOn({});
        for (const library::Source source : storeOrder) {
            const std::vector<library::Game> games = fromStore(onProton, source);
            if (!games.empty()) {
                page.rows.push_back(ui::makeHeading("From " + std::string{library::label(source)}));
            }
            for (const library::Game& game : games) {
                page.rows.push_back(
                    ui::makeRow("wine.game:" + game.id, RowKind::Info, game.title, "", ""));
            }
        }
        if (onProton.empty()) {
            page.rows.push_back(ui::makeRow("wine.none", RowKind::Info, "No games use it", "", ""));
        }
        return page;
    }

    const fs::path wine{key};
    const auto runner = std::ranges::find(found_, wine, &library::wine::Runner::wine);
    if (runner == found_.end()) {
        return page;
    }
    const settings::Settings& v = preferences_.values();
    page.label = library::wine::displayName(runner->name);
    page.rows.push_back(
        ui::makeRow("wine.where", RowKind::Info, "Where", whereOf(*runner),
                    (runner->proton.empty() ? runner->wine : runner->proton).string()));
    if (const auto piece = std::ranges::find(look.builds, runner->proton, &WineRunners::Piece::dir);
        piece != look.builds.end()) {
        page.rows.push_back(ui::makeRow("wine.size", RowKind::Info, "Size", "On disk",
                                        piece->bytes > 0 ? sizeText(piece->bytes) : "Measuring"));
    }
    if (const auto kept = look.kept.find(runner->name);
        isManaged(*runner) && kept != look.kept.end()) {
        page.rows.push_back(ui::makeRow("wine.kept", RowKind::Info, "Not removed",
                                        "openSU keeps only the newest two GE-Proton builds",
                                        kept->second));
    }
    const std::vector<library::Game> fromSteam = steamGamesOn(*runner);
    if (!fromSteam.empty()) {
        page.rows.push_back(ui::makeHeading("From Steam"));
    }
    for (const library::Game& game : fromSteam) {
        page.rows.push_back(ui::makeRow("wine.steamGame:" + game.id, RowKind::Info, game.title,
                                        "Change it in Steam; it can't be pinned here",
                                        "Set in Steam"));
    }
    const std::vector<library::Game> using_ = gamesOn(wine);
    for (const library::Source source : {library::Source::Gog, library::Source::Epic}) {
        const std::vector<library::Game> games = fromStore(using_, source);
        if (!games.empty()) {
            page.rows.push_back(ui::makeHeading("From " + std::string{library::label(source)}));
        }
        for (const library::Game& game : games) {
            const bool pinned = v.wineRunnerPins.contains(game.id);
            page.rows.push_back(ui::makeRow(std::string{pinId} + game.id + "\t",
                                            pinned ? RowKind::Action : RowKind::Info, game.title,
                                            pinned ? "Pinned to this runner; press A to unpin"
                                                   : "Uses it for every game",
                                            pinned ? "Pinned" : "Default"));
        }
    }
    bool listed = !fromSteam.empty() || !using_.empty();
    bool headed = false;
    // A pin of a game the library does not list now still keeps the runner.
    for (const auto& [game, pinned] : v.wineRunnerPins) {
        if (pinned == wine && std::ranges::find(using_, game, &library::Game::id) == using_.end()) {
            if (!std::exchange(headed, true)) {
                page.rows.push_back(ui::makeHeading("Pinned, not in the library now"));
            }
            page.rows.push_back(ui::makeRow(std::string{pinId} + game + "\t", RowKind::Action, game,
                                            "Press A to unpin", "Pinned"));
            listed = true;
        }
    }
    if (!listed) {
        page.rows.push_back(ui::makeRow("wine.none", RowKind::Info, "No games use it", "", ""));
    }
    std::vector<library::Game> others = windowsGames();
    std::erase_if(others, [&using_](const library::Game& game) {
        return std::ranges::find(using_, game.id, &library::Game::id) != using_.end();
    });
    if (!others.empty()) {
        page.rows.push_back(ui::makeHeading("Pin a game to it"));
        for (const library::Game& game : others) {
            page.rows.push_back(ui::makeRow(
                std::string{pinId} + game.id + "\t" + wine.string(), RowKind::Action, game.title,
                "Run it under " + library::wine::displayName(runner->name), "Pin"));
        }
    }
    return page;
}

bool WineSettings::owns(const std::string& id) {
    return id.starts_with("wine.");
}

void WineSettings::run(const std::string& id, int step) {
    if (id == runnerId) {
        stepRunner(step);
    } else if (id == keepId) {
        bool& keep = preferences_.values().keepGeProtonCurrent;
        keep = !keep;
        lucent::info("wine", "keeping GE-Proton up to date {}", keep ? "on" : "off");
        preferences_.save();
    } else if (id == checkId) {
        if (const std::string refused = runners_.checkNow(); !refused.empty()) {
            hooks_.say(refused, true);
        }
    } else if (id == testId) {
        test();
    } else if (id.starts_with(pinId)) {
        const std::string rest = id.substr(pinId.size());
        const std::size_t tab = rest.find('\t');
        const std::string game = rest.substr(0, tab);
        const fs::path wine =
            tab == std::string::npos ? fs::path{} : fs::path{rest.substr(tab + 1)};
        std::map<std::string, fs::path>& pins = preferences_.values().wineRunnerPins;
        if (wine.empty()) {
            pins.erase(game);
        } else {
            pins[game] = wine;
        }
        lucent::info("wine", "{} {}", game,
                     wine.empty() ? "unpinned" : "pinned to " + wine.string());
        preferences_.save();
    }
}

void WineSettings::stepRunner(int step) {
    // Choice 0 is Automatic, then each runner found, then a pick no longer found.
    fs::path& chosen = preferences_.values().wineRunner;
    const auto at = std::ranges::find(found_, chosen, &library::wine::Runner::wine);
    const bool missing = !chosen.empty() && at == found_.end();
    const int count = static_cast<int>(found_.size()) + (missing ? 2 : 1);
    int from = 0;
    if (missing) {
        from = count - 1;
    } else if (!chosen.empty()) {
        from = static_cast<int>(at - found_.begin()) + 1;
    }
    const int to = (from + (step == 0 ? 1 : step) % count + count) % count;
    if (missing && to == count - 1) {
        return;
    }
    chosen = to == 0 ? fs::path{} : found_[static_cast<std::size_t>(to - 1)].wine;
    preferences_.save();
}

void WineSettings::test() {
    const library::wine::Choice resolved = choice();
    const std::optional<library::wine::Runner> runner = library::wine::runnerOf(resolved, {});
    if (!runner) {
        hooks_.say("No Wine or Proton is installed to test", true);
        return;
    }
    const library::wine::ReadyPrefix ready =
        library::wine::readyPrefix(config_.dataDir / "wine" / "test-prefix", *runner);
    if (!ready.refusal.empty()) {
        hooks_.say(ready.refusal, true);
        return;
    }
    const fs::path& prefix = ready.prefix;
    library::wine::Invocation invocation =
        library::wine::invocationOf(*runner, prefix, resolved.umuRun, config_.dataDir, "");
    std::vector<std::string> command = std::move(invocation.environment);
    command.insert(command.end(), invocation.command.begin(), invocation.command.end());
    command.insert(command.end(), std::begin(testProgram), std::end(testProgram));
    if (const std::string refused =
            runners_.test(library::wine::displayName(runner->name), std::move(command));
        !refused.empty()) {
        hooks_.say(refused, true);
    }
}

library::wine::Prepared WineSettings::prepare(library::Game& game) {
    if (!game.windowsInstall) {
        return {};
    }
    rescan();
    const library::wine::Choice resolved = choice();
    library::wine::Prepared prepared = library::wine::prepare(game, resolved);
    if (const std::optional<library::wine::Runner> runner =
            library::wine::runnerOf(resolved, game.id);
        prepared.refusal.empty() && runner) {
        launched_ = runner->proton;
    }
    return prepared;
}

} // namespace opensu::app
