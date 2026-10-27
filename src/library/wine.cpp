#include "wine.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "ge_proton.hpp"
#include "launch/command.hpp"
#include "lucent/log.h"
#include "lutris_wine.hpp"

namespace opensu::library::wine {
namespace {

namespace fs = std::filesystem;

constexpr const char* sharedPrefixName = "wine-prefix";
constexpr const char* ownPrefixesName = "wine-prefixes";
/// umu looks a game's fixes up by id; openSU knows none, so every game gets the default.
constexpr const char* umuGameId = "umu-0";

/// The Wine binary inside Proton build `dir`: `files/bin/wine`, older builds `dist/bin/wine`.
fs::path protonWine(const fs::path& dir) {
    for (const char* layout : {"files/bin/wine", "dist/bin/wine"}) {
        std::error_code error;
        if (fs::is_regular_file(dir / layout, error)) {
            return dir / layout;
        }
    }
    return {};
}

/// The Proton build a Wine binary belongs to: `<build>/files/bin/wine` with a `proton` script.
fs::path protonOf(const fs::path& wine) {
    const fs::path build = wine.parent_path().parent_path().parent_path();
    std::error_code error;
    return !build.empty() && fs::is_regular_file(build / "proton", error) ? build : fs::path{};
}

/// The Proton builds (and, for Lutris, plain Wine builds with `bin/wine`) directly inside
/// `parent` whose name passes `accept`.
template <typename Accept>
std::vector<Runner> buildsIn(const fs::path& parent, Origin origin, Accept accept) {
    std::vector<Runner> runners;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{parent, error}) {
        const std::string name = entry.path().filename().string();
        if (!accept(name)) {
            continue;
        }
        if (fs::path binary = protonWine(entry.path()); !binary.empty()) {
            const fs::path build = protonOf(binary);
            runners.push_back(
                Runner{.name = name, .wine = std::move(binary), .proton = build, .origin = origin});
        } else if (std::error_code ec; origin == Origin::Lutris &&
                                       fs::is_regular_file(entry.path() / "bin" / "wine", ec)) {
            runners.push_back(
                Runner{.name = name, .wine = entry.path() / "bin" / "wine", .origin = origin});
        }
    }
    return runners;
}

/// Newest first by the version in the name; builds without one after, by name.
void sortNewestFirst(std::vector<Runner>& runners) {
    std::ranges::stable_sort(runners, [](const Runner& a, const Runner& b) {
        const std::optional<ge_proton::Version> va = ge_proton::protonVersionOf(a.name);
        const std::optional<ge_proton::Version> vb = ge_proton::protonVersionOf(b.name);
        if (va.has_value() != vb.has_value()) {
            return va.has_value();
        }
        return va && *va != *vb ? *va > *vb : a.name < b.name;
    });
}

void addNew(std::vector<Runner>& runners, std::vector<Runner> more) {
    sortNewestFirst(more);
    for (Runner& runner : more) {
        if (std::ranges::find(runners, runner.wine, &Runner::wine) == runners.end()) {
            runners.push_back(std::move(runner));
        }
    }
}

bool canRun(const Runner& runner, const Choice& choice) {
    return runner.proton.empty() || !choice.umuRun.empty();
}

std::optional<Resolved> firstOf(const Choice& choice, Origin origin, Reason reason) {
    const auto found = std::ranges::find_if(choice.found, [&choice, origin](const Runner& runner) {
        return runner.origin == origin && canRun(runner, choice);
    });
    return found == choice.found.end()
               ? std::nullopt
               : std::optional{Resolved{.runner = *found, .reason = reason}};
}

bool isGameStore(Source source) {
    return source == Source::Gog || source == Source::Epic;
}

/// umu's name for the store, which picks the store's fixes.
std::string_view umuStore(Source source) {
    return source == Source::Gog ? "gog" : "egs";
}

/// `word` as one word for a POSIX shell, which is how gogdl and legendary split `--wrapper`.
std::string shellWord(const std::string& word) {
    std::string quoted = "'";
    for (const char c : word) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    return quoted + "'";
}

bool hasEntry(const fs::path& path) {
    std::error_code error;
    return fs::exists(fs::symlink_status(path, error));
}

/// umu gives a prefix Proton runs in a `pfx` link to itself.
bool isProtonPrefix(const fs::path& prefix) {
    return hasEntry(prefix / "pfx");
}

} // namespace

std::vector<Runner> discover(const Search& search) {
    std::vector<Runner> runners;
    for (const ge_proton::Build& build : ge_proton::installed(search.managed)) {
        runners.push_back(Runner{.name = build.name,
                                 .wine = build.dir / "files" / "bin" / "wine",
                                 .proton = build.dir,
                                 .origin = Origin::OpenSu});
    }
    const auto any = [](const std::string&) {
        return true;
    };
    std::vector<Runner> steam;
    for (const fs::path& root : search.steamRoots) {
        std::ranges::move(buildsIn(root / "compatibilitytools.d", Origin::Steam, any),
                          std::back_inserter(steam));
    }
    for (const fs::path& library : search.steamLibraries) {
        std::ranges::move(buildsIn(library / "steamapps" / "common", Origin::Steam,
                                   [](const std::string& name) {
                                       return name.starts_with("Proton");
                                   }),
                          std::back_inserter(steam));
    }
    addNew(runners, std::move(steam));
    if (!search.lutris.empty()) {
        addNew(runners, buildsIn(lutris::wineDir(search.lutris), Origin::Lutris, any));
    }
    if (fs::path system = launch::resolveExecutable("wine", search.executablePath);
        !system.empty()) {
        runners.push_back(Runner{.name = "System Wine", .wine = std::move(system)});
    }
    return runners;
}

fs::path umuRunOf(const fs::path& managed, const Search& search) {
    if (!managed.empty()) {
        return managed;
    }
    if (fs::path onPath = launch::resolveExecutable("umu-run", search.executablePath);
        !onPath.empty()) {
        return onPath;
    }
    return search.lutris.empty() ? fs::path{} : lutris::umuRun(search.lutris);
}

std::optional<Runner> lutrisDefault(const fs::path& lutrisData, const std::vector<Runner>& found) {
    if (lutrisData.empty()) {
        return std::nullopt;
    }
    const std::string version = lutris::defaultWineVersion(lutrisData);
    if (version == lutris::geProtonLatest) {
        // umu's GE-Proton lives in a compatibilitytools.d; `found` is newest first.
        const auto newest = std::ranges::find_if(found, [](const Runner& runner) {
            return runner.origin == Origin::Steam && runner.name.starts_with("GE-Proton") &&
                   runner.proton.parent_path().filename() == "compatibilitytools.d";
        });
        return newest == found.end() ? std::nullopt : std::optional{*newest};
    }
    if (const auto named = std::ranges::find(found, version, &Runner::name); named != found.end()) {
        return *named;
    }
    if (const std::optional<fs::path> system = lutris::systemWineOf(version)) {
        const auto same = std::ranges::find_if(found, [&system](const Runner& runner) {
            return runner.origin == Origin::System &&
                   (system->is_relative() ? runner.wine.filename() == *system
                                          : runner.wine == *system);
        });
        if (same != found.end()) {
            return *same;
        }
        std::error_code error;
        if (system->is_absolute() && fs::is_regular_file(*system, error)) {
            return Runner{.name = version, .wine = *system, .origin = Origin::Lutris};
        }
    }
    return std::nullopt;
}

void OwnPrefixes::set(const std::string& gameId, bool own) {
    if (own) {
        ids_.insert(gameId);
    } else {
        ids_.erase(gameId);
    }
}

fs::path sharedPrefix(const fs::path& dataDir) {
    return dataDir / sharedPrefixName;
}

fs::path ownPrefix(const fs::path& dataDir, const Game& game) {
    std::string name{label(game.source)};
    std::ranges::transform(name, name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    name += '-';
    for (const char c : game.sourceId) {
        const bool plain =
            std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '.' || c == '_' || c == '-';
        name += plain ? c : '_';
    }
    return dataDir / ownPrefixesName / name;
}

fs::path prefixFor(const fs::path& dataDir, const Game& game, const OwnPrefixes& own) {
    return own.contains(game.id) ? ownPrefix(dataDir, game) : sharedPrefix(dataDir);
}

fs::path umuFolders(const fs::path& dataDir) {
    return dataDir / "wine";
}

fs::path pickOf(const Choice& choice, const std::string& gameId) {
    const auto pinned = choice.pins.find(gameId);
    return pinned != choice.pins.end() ? pinned->second : choice.runner;
}

std::optional<Resolved> resolve(const Choice& choice, const std::string& gameId) {
    if (const fs::path picked = pickOf(choice, gameId); !picked.empty()) {
        const Reason reason = choice.pins.contains(gameId) ? Reason::Pinned : Reason::Picked;
        if (const auto found = std::ranges::find(choice.found, picked, &Runner::wine);
            found != choice.found.end()) {
            return Resolved{.runner = *found, .reason = reason};
        }
        return Resolved{
            .runner = Runner{.name = picked.string(), .wine = picked, .proton = protonOf(picked)},
            .reason = reason};
    }
    if (std::optional<Resolved> ours = firstOf(choice, Origin::OpenSu, Reason::OpenSuGeProton)) {
        return ours;
    }
    if (std::optional<Resolved> steam = firstOf(choice, Origin::Steam, Reason::SteamProton)) {
        return steam;
    }
    if (choice.lutris && canRun(*choice.lutris, choice)) {
        return Resolved{.runner = *choice.lutris, .reason = Reason::Lutris};
    }
    return firstOf(choice, Origin::System, Reason::SystemWine);
}

std::optional<Runner> runnerOf(const Choice& choice, const std::string& gameId) {
    std::optional<Resolved> resolved = resolve(choice, gameId);
    return resolved ? std::optional{std::move(resolved->runner)} : std::nullopt;
}

Invocation invocationOf(const Runner& runner, const fs::path& prefix, const fs::path& umuRun,
                        const fs::path& dataDir, std::string_view store) {
    if (runner.proton.empty()) {
        return Invocation{.command = {runner.wine.string()},
                          .environment = {"WINEPREFIX=" + prefix.string()}};
    }
    return Invocation{
        .command = {umuRun.string()},
        .environment = {"WINEPREFIX=" + prefix.string(), "PROTONPATH=" + runner.proton.string(),
                        std::string{"GAMEID="} + umuGameId, "STORE=" + std::string{store},
                        "UMU_FOLDERS_PATH=" + umuFolders(dataDir).string()}};
}

fs::path wineSavesCopy(const fs::path& prefix) {
    return prefix.parent_path() / (prefix.filename().string() + ".system-wine-saves");
}

fs::path plainWinePrefix(const fs::path& prefix) {
    return prefix.parent_path() / (prefix.filename().string() + ".plain-wine");
}

ReadyPrefix readyPrefix(const fs::path& prefix, const Runner& runner) {
    const bool proton = !runner.proton.empty();
    const bool madeByProton = isProtonPrefix(prefix);
    const fs::path into = proton ? wineSavesCopy(prefix) : plainWinePrefix(prefix);
    const bool copy = proton ? hasEntry(prefix / "system.reg") && !madeByProton && !hasEntry(into)
                             : madeByProton && !hasEntry(into / "drive_c" / "users");
    const fs::path runsIn = proton || !madeByProton ? prefix : into;
    std::error_code error;
    fs::create_directories(runsIn, error);
    if (copy && !error) {
        fs::create_directories(into / "drive_c", error);
        for (const char* tree : {"users", "ProgramData"}) {
            if (!error && hasEntry(prefix / "drive_c" / tree)) {
                fs::copy(prefix / "drive_c" / tree, into / "drive_c" / tree,
                         fs::copy_options::recursive | fs::copy_options::copy_symlinks, error);
            }
        }
        for (const char* hive : {"system.reg", "user.reg", "userdef.reg"}) {
            if (proton && !error && hasEntry(prefix / hive)) {
                fs::copy_file(prefix / hive, into / hive, error);
            }
        }
        if (!error) {
            lucent::info("wine", "{} was made by {}; its saves are copied to {} before {} runs",
                         prefix.string(), proton ? "plain Wine" : "Proton", into.string(),
                         runner.name);
        }
    }
    if (error) {
        return {.prefix = runsIn,
                .refusal = "Cannot ready the Wine prefix " + prefix.string() + " for " +
                           runner.name + ": " + error.message()};
    }
    return {.prefix = runsIn, .refusal = {}};
}

Prepared prepare(Game& game, const Choice& choice) {
    if (!isGameStore(game.source) || !game.windowsInstall || game.launch.empty()) {
        return {};
    }
    const std::optional<Runner> runner = runnerOf(choice, game.id);
    if (!runner) {
        return {.refusal = "No Wine runner can run " + game.title +
                           ": no GE-Proton, Steam Proton or Lutris runner with umu-run, and no "
                           "system Wine",
                .environment = {}};
    }
    std::error_code error;
    if (!fs::is_regular_file(runner->wine, error)) {
        return {.refusal = "The Wine runner " + runner->wine.string() +
                           " is gone; pick another in Settings",
                .environment = {}};
    }
    if (!runner->proton.empty() && choice.umuRun.empty()) {
        return {.refusal =
                    "No umu-run to run " + runner->name + "; pick another runner in Settings",
                .environment = {}};
    }
    ReadyPrefix ready = readyPrefix(prefixFor(choice.dataDir, game, choice.own), *runner);
    if (!ready.refusal.empty()) {
        return {.refusal = std::move(ready.refusal), .environment = {}};
    }
    const fs::path& prefix = ready.prefix;
    if (runner->proton.empty()) {
        game.launch.args.insert(game.launch.args.end(), {"--wine", runner->wine.string(),
                                                         "--wine-prefix", prefix.string()});
        lucent::info("wine", "{} runs under {} in {}", game.title, runner->name, prefix.string());
        return {};
    }
    Invocation invocation =
        invocationOf(*runner, prefix, choice.umuRun, choice.dataDir, umuStore(game.source));
    game.launch.args.insert(game.launch.args.end(),
                            {"--no-wine", "--wrapper", shellWord(invocation.command.front())});
    lucent::info("wine", "{} runs under {} through umu-run in {}", game.title, runner->name,
                 prefix.string());
    return {.refusal = {}, .environment = std::move(invocation.environment)};
}

} // namespace opensu::library::wine
