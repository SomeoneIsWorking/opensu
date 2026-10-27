#include "wine_settings.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>

#include "library/ge_proton.hpp"
#include "library/steam.hpp"
#include "lucent/log.h"
#include "steam_library_settings.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using ui::RowKind;

constexpr const char* runnerId = "wine.runner";
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
constexpr const char* defaultName = "Latest GE-Proton";
constexpr const char* testProgram[] = {"cmd", "/c", "ver"};
constexpr double percent = 100.0;

std::string countText(std::size_t count) {
    return std::to_string(count) + (count == 1 ? " game" : " games");
}

/// Where a runner lives, as the player reads it.
std::string whereOf(const library::wine::Runner& runner) {
    if (runner.managed) {
        return "Kept up to date by openSU";
    }
    return runner.proton.empty() ? "System" : "Found in Steam's folders";
}

} // namespace

library::wine::Search WineSettings::search() const {
    library::wine::Search search{.executablePath = config_.executablePath,
                                 .steamRoots = {},
                                 .steamLibraries = {},
                                 .managed = library::ge_proton::runnersDir(config_.dataDir)};
    if (!integrations_.lists(library::Source::Steam)) {
        return search;
    }
    const library::steam::Library steam =
        library::steam::Library::discover(config_.home, config_.steamRoots);
    search.steamRoots = steam.roots();
    for (const library::steam::LibraryFolder& folder : steam.libraryFolders()) {
        search.steamLibraries.push_back(folder.path);
    }
    return search;
}

void WineSettings::rescan() {
    found_ = library::wine::discover(search());
}

library::wine::Choice WineSettings::choice() const {
    const settings::Settings& v = preferences_.values();
    return library::wine::Choice{.runner = v.wineRunner,
                                 .pins = v.wineRunnerPins,
                                 .found = found_,
                                 .dataDir = config_.dataDir,
                                 .own = v.ownWinePrefixes,
                                 .umuRun = runners_.umuRun()};
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

WineRunners::InUse WineSettings::inUse() const {
    const fs::path runners = library::ge_proton::runnersDir(config_.dataDir);
    WineRunners::InUse inUse;
    const settings::Settings& v = preferences_.values();
    const auto buildOf = [&runners](const fs::path& wine) {
        const fs::path build = wine.parent_path().parent_path().parent_path();
        return library::ge_proton::managed(runners, build) ? build : fs::path{};
    };
    if (const fs::path picked = buildOf(v.wineRunner); !picked.empty()) {
        inUse[picked] = "Picked for every game in Settings › Wine";
    }
    std::map<fs::path, std::size_t> pinned;
    for (const auto& [game, wine] : v.wineRunnerPins) {
        if (const fs::path build = buildOf(wine); !build.empty()) {
            ++pinned[build];
        }
    }
    for (const auto& [build, count] : pinned) {
        inUse[build] = "Kept: " + countText(count) + " pinned to it";
    }
    if (!launched_.empty() && hooks_.gameRunning()) {
        inUse[launched_] = "Kept: a game runs on it";
    }
    return inUse;
}

ui::SettingsCategory WineSettings::page() const {
    const settings::Settings& v = preferences_.values();
    const WineRunners::Look look = runners_.look();
    ui::SettingsCategory page{std::string{categoryId}, "Wine", {}};

    // Choice 0 is GE-Proton kept up to date, then each runner found, then a pick not found.
    const auto newest = std::ranges::find_if(found_, &library::wine::Runner::managed);
    ui::Options runners;
    runners.labels.push_back(std::string{defaultName} + " (" +
                             (newest == found_.end()
                                  ? "not installed yet"
                                  : newest->name.substr(std::string_view{"GE-Proton"}.size())) +
                             ")");
    for (const library::wine::Runner& runner : found_) {
        runners.labels.push_back(runner.name);
    }
    if (!v.wineRunner.empty()) {
        const auto at = std::ranges::find(found_, v.wineRunner, &library::wine::Runner::wine);
        if (at == found_.end()) {
            runners.labels.push_back(v.wineRunner.string() + " (not found)");
            runners.chosen = runners.labels.size() - 1;
        } else {
            runners.chosen = static_cast<std::size_t>(at - found_.begin()) + 1;
        }
    }
    page.rows.push_back(ui::makeChoice(
        runnerId, "Wine runner", "What Windows GOG and Epic games run under", std::move(runners)));

    std::string status = look.lastCheck.empty() ? "Not checked yet" : look.lastCheck;
    if (const std::optional<WineRunners::Progress> running = runners_.progress()) {
        status = "Getting " + running->name;
        if (running->stage == library::ArchiveStage::Checking) {
            status += " · checking";
        } else if (running->stage == library::ArchiveStage::Unpacking) {
            status += " · unpacking";
        } else if (running->total > 0) {
            status += " · " + std::to_string(running->received * 100 / running->total) + "%";
        }
    } else if (look.checking) {
        status = "Checking…";
    }
    page.rows.push_back(ui::makeRow(checkId, RowKind::Action, "Check for GE-Proton now",
                                    "Also checked at start and every day", status));
    page.rows.push_back(ui::makeRow(testId, RowKind::Action, "Test the runner",
                                    "Runs cmd /c ver under the runner in a test prefix",
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
            runner.managed && kept != look.kept.end()) {
            note += " · " + kept->second;
        }
        page.rows.push_back(ui::makePage(std::string{runtimePage} + runner.wine.string(),
                                         runner.name, note,
                                         countText(gamesOn(runner.wine).size())));
    }
    const std::size_t onProton = gamesOn({}).size();
    if (look.umu) {
        page.rows.push_back(
            ui::makePage(std::string{runtimePage} + std::string{umuKey}, look.umu->name,
                         "Runs Proton outside Steam · openSU's · " + sizeText(look.umu->bytes),
                         countText(onProton)));
    }
    if (look.steamRuntime) {
        page.rows.push_back(ui::makePage(
            std::string{runtimePage} + std::string{steamRuntimeKey}, look.steamRuntime->name,
            "Proton's container · fetched by umu-launcher · " + sizeText(look.steamRuntime->bytes),
            countText(onProton)));
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
        page.rows.push_back(ui::makeRow("wine.where", RowKind::Info, "Where", "Managed by openSU",
                                        piece->dir.string()));
        page.rows.push_back(
            ui::makeRow("wine.size", RowKind::Info, "Size", "On disk", sizeText(piece->bytes)));
        page.rows.push_back(ui::makeHeading("Games using it (every game on a Proton build)"));
        for (const library::Game& game : gamesOn({})) {
            page.rows.push_back(
                ui::makeRow("wine.game:" + game.id, RowKind::Info, game.title, "", ""));
        }
        return page;
    }

    const fs::path wine{key};
    const auto runner = std::ranges::find(found_, wine, &library::wine::Runner::wine);
    if (runner == found_.end()) {
        return page;
    }
    const settings::Settings& v = preferences_.values();
    page.label = runner->name;
    page.rows.push_back(
        ui::makeRow("wine.where", RowKind::Info, "Where", whereOf(*runner),
                    (runner->proton.empty() ? runner->wine : runner->proton).string()));
    if (const auto piece = std::ranges::find(look.builds, runner->proton, &WineRunners::Piece::dir);
        piece != look.builds.end()) {
        page.rows.push_back(ui::makeRow("wine.size", RowKind::Info, "Size", "On disk",
                                        piece->bytes > 0 ? sizeText(piece->bytes) : "Measuring"));
    }
    if (const auto kept = look.kept.find(runner->name);
        runner->managed && kept != look.kept.end()) {
        page.rows.push_back(ui::makeRow("wine.kept", RowKind::Info, "Not removed",
                                        "Older than the newest two GE-Proton builds",
                                        kept->second));
    }
    page.rows.push_back(ui::makeHeading("Games using it"));
    const std::vector<library::Game> using_ = gamesOn(wine);
    for (const library::Game& game : using_) {
        const bool pinned = v.wineRunnerPins.contains(game.id);
        page.rows.push_back(ui::makeRow(
            std::string{pinId} + game.id + "\t", pinned ? RowKind::Action : RowKind::Info,
            game.title, pinned ? "Pinned to it · A unpins" : "By the runner picked for every game",
            pinned ? "Pinned" : "Default"));
    }
    bool listed = !using_.empty();
    // A pin of a game the library does not list now still keeps the runner.
    for (const auto& [game, pinned] : v.wineRunnerPins) {
        if (pinned == wine && std::ranges::find(using_, game, &library::Game::id) == using_.end()) {
            page.rows.push_back(ui::makeRow(std::string{pinId} + game + "\t", RowKind::Action, game,
                                            "Pinned to it, not in the library now · A unpins",
                                            "Pinned"));
            listed = true;
        }
    }
    if (!listed) {
        page.rows.push_back(ui::makeRow("wine.none", RowKind::Info, "No game", "", ""));
    }
    std::vector<library::Game> others = windowsGames();
    std::erase_if(others, [&using_](const library::Game& game) {
        return std::ranges::find(using_, game.id, &library::Game::id) != using_.end();
    });
    if (!others.empty()) {
        page.rows.push_back(ui::makeHeading("Pin a game to it"));
        for (const library::Game& game : others) {
            page.rows.push_back(ui::makeRow(std::string{pinId} + game.id + "\t" + wine.string(),
                                            RowKind::Action, game.title,
                                            "Runs it under " + runner->name, "Pin"));
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
    // Choice 0 is GE-Proton kept up to date, then each runner found, then a pick no longer found.
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
    if (!runner || (!runner->proton.empty() && resolved.umuRun.empty())) {
        const std::string refused = runners_.ensure();
        hooks_.say(refused.empty() ? "Installing GE-Proton first; test again when it is installed"
                                   : refused,
                   !refused.empty());
        return;
    }
    const fs::path prefix = config_.dataDir / "wine" / "test-prefix";
    std::error_code error;
    fs::create_directories(prefix, error);
    if (!runner->proton.empty()) {
        if (const std::string refused = library::wine::keepWineSaves(prefix); !refused.empty()) {
            hooks_.say(refused, true);
            return;
        }
    }
    library::wine::Invocation invocation =
        library::wine::invocationOf(*runner, prefix, resolved.umuRun, config_.dataDir, "");
    std::vector<std::string> command = std::move(invocation.environment);
    command.insert(command.end(), invocation.command.begin(), invocation.command.end());
    command.insert(command.end(), std::begin(testProgram), std::end(testProgram));
    if (const std::string refused = runners_.test(runner->name, std::move(command));
        !refused.empty()) {
        hooks_.say(refused, true);
    }
}

WineSettings::Prepared WineSettings::prepare(library::Game& game) {
    if (!game.windowsInstall) {
        return {};
    }
    rescan();
    const library::wine::Choice resolved = choice();
    const std::optional<library::wine::Runner> runner = library::wine::runnerOf(resolved, game.id);
    const bool needsGe = !runner && library::wine::pickOf(resolved, game.id).empty();
    const bool needsUmu = runner && !runner->proton.empty() && resolved.umuRun.empty();
    if (needsGe || needsUmu) {
        lucent::info("wine", "{} waits for GE-Proton and umu-launcher to install", game.title);
        held_ = game;
        heldDropped_ = false;
        heldChecking_ = runners_.ensure().empty() || runners_.busy();
        return {.refusal = {}, .environment = {}, .waiting = true};
    }
    library::wine::Prepared prepared = library::wine::prepare(game, resolved);
    if (prepared.refusal.empty() && runner) {
        launched_ = runner->proton;
    }
    return {.refusal = std::move(prepared.refusal),
            .environment = std::move(prepared.environment),
            .waiting = false};
}

std::optional<WineSettings::Held> WineSettings::serviceHeld() {
    if (!held_) {
        return std::nullopt;
    }
    if (heldDropped_) {
        held_.reset();
        return Held{.line = {}, .fraction = {}, .ready = {}, .failed = true};
    }
    if (runners_.busy()) {
        Held held{.line = "Getting GE-Proton", .fraction = {}, .ready = {}, .failed = false};
        if (const std::optional<WineRunners::Progress> running = runners_.progress()) {
            held.line = "Getting " + running->name;
            if (running->stage != library::ArchiveStage::Downloading) {
                held.line += running->stage == library::ArchiveStage::Checking ? " · checking"
                                                                               : " · unpacking";
            } else if (running->total > 0) {
                held.fraction =
                    static_cast<double>(running->received) / static_cast<double>(running->total);
                held.line +=
                    " · " + std::to_string(static_cast<int>(*held.fraction * percent)) + "%";
            }
        }
        return held;
    }
    if (!heldChecking_) {
        heldChecking_ = runners_.ensure().empty();
        return Held{.line = "Getting GE-Proton", .fraction = {}, .ready = {}, .failed = false};
    }
    library::Game game = std::move(*held_);
    held_.reset();
    rescan();
    const library::wine::Choice resolved = choice();
    const std::optional<library::wine::Runner> runner = library::wine::runnerOf(resolved, game.id);
    if (!runner || (!runner->proton.empty() && resolved.umuRun.empty())) {
        lucent::warn("wine", "{} cannot start: no GE-Proton was installed", game.title);
        return Held{.line = {}, .fraction = {}, .ready = {}, .failed = true};
    }
    return Held{.line = {}, .fraction = {}, .ready = std::move(game), .failed = false};
}

bool WineSettings::dropHeld() {
    if (!held_) {
        return false;
    }
    heldDropped_ = true;
    return true;
}

} // namespace opensu::app
