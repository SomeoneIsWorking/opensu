#include "wine.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "ge_proton.hpp"
#include "launch/command.hpp"
#include "lucent/log.h"

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

/// The Proton builds directly inside `parent` whose name passes `accept`, by name.
template <typename Accept> std::vector<Runner> protonsIn(const fs::path& parent, Accept accept) {
    std::vector<Runner> runners;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{parent, error}) {
        const std::string name = entry.path().filename().string();
        if (!accept(name)) {
            continue;
        }
        if (fs::path binary = protonWine(entry.path()); !binary.empty()) {
            runners.push_back(Runner{.name = name,
                                     .wine = std::move(binary),
                                     .proton = protonOf(protonWine(entry.path())),
                                     .managed = false});
        }
    }
    std::ranges::sort(runners, {}, &Runner::name);
    return runners;
}

void addNew(std::vector<Runner>& runners, std::vector<Runner> more) {
    for (Runner& runner : more) {
        if (std::ranges::find(runners, runner.wine, &Runner::wine) == runners.end()) {
            runners.push_back(std::move(runner));
        }
    }
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

} // namespace

std::vector<Runner> discover(const Search& search) {
    std::vector<Runner> runners;
    for (const ge_proton::Build& build : ge_proton::installed(search.managed)) {
        runners.push_back(Runner{.name = build.name,
                                 .wine = build.dir / "files" / "bin" / "wine",
                                 .proton = build.dir,
                                 .managed = true});
    }
    if (fs::path system = launch::resolveExecutable("wine", search.executablePath);
        !system.empty()) {
        runners.push_back(Runner{.name = "System Wine", .wine = std::move(system)});
    }
    const auto any = [](const std::string&) {
        return true;
    };
    for (const fs::path& root : search.steamRoots) {
        addNew(runners, protonsIn(root / "compatibilitytools.d", any));
    }
    for (const fs::path& library : search.steamLibraries) {
        addNew(runners, protonsIn(library / "steamapps" / "common", [](const std::string& name) {
                   return name.starts_with("Proton");
               }));
    }
    return runners;
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

std::optional<Runner> runnerOf(const Choice& choice, const std::string& gameId) {
    const fs::path picked = pickOf(choice, gameId);
    if (picked.empty()) {
        const auto newest = std::ranges::find_if(choice.found, &Runner::managed);
        return newest == choice.found.end() ? std::nullopt : std::optional{*newest};
    }
    if (const auto found = std::ranges::find(choice.found, picked, &Runner::wine);
        found != choice.found.end()) {
        return *found;
    }
    return Runner{.name = picked.string(), .wine = picked, .proton = protonOf(picked)};
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

std::string keepWineSaves(const fs::path& prefix) {
    const fs::path copy = wineSavesCopy(prefix);
    if (!hasEntry(prefix / "system.reg") || hasEntry(prefix / "pfx") || hasEntry(copy)) {
        return {};
    }
    std::error_code error;
    fs::create_directories(copy / "drive_c", error);
    for (const char* tree : {"users", "ProgramData"}) {
        if (!error && hasEntry(prefix / "drive_c" / tree)) {
            fs::copy(prefix / "drive_c" / tree, copy / "drive_c" / tree,
                     fs::copy_options::recursive | fs::copy_options::copy_symlinks, error);
        }
    }
    for (const char* hive : {"system.reg", "user.reg", "userdef.reg"}) {
        if (!error && hasEntry(prefix / hive)) {
            fs::copy_file(prefix / hive, copy / hive, error);
        }
    }
    if (error) {
        return "Cannot copy the saves of " + prefix.string() +
               " before Proton runs in it: " + error.message();
    }
    lucent::info("wine", "{} was made by plain Wine; its saves are copied to {}", prefix.string(),
                 copy.string());
    return {};
}

Prepared prepare(Game& game, const Choice& choice) {
    if (!isGameStore(game.source) || !game.windowsInstall || game.launch.empty()) {
        return {};
    }
    const std::optional<Runner> runner = runnerOf(choice, game.id);
    if (!runner) {
        return {.refusal =
                    "No Wine runner to run " + game.title + "; GE-Proton is not installed yet",
                .environment = {}};
    }
    std::error_code error;
    if (!fs::is_regular_file(runner->wine, error)) {
        return {.refusal = "The Wine runner " + runner->wine.string() +
                           " is gone; pick another in Settings",
                .environment = {}};
    }
    if (!runner->proton.empty() && choice.umuRun.empty()) {
        return {.refusal = "umu-launcher, which runs " + runner->name +
                           ", is not installed yet; check for it in Settings › Wine",
                .environment = {}};
    }
    const fs::path prefix = prefixFor(choice.dataDir, game, choice.own);
    fs::create_directories(prefix, error);
    if (error) {
        return {.refusal =
                    "Cannot make the Wine prefix " + prefix.string() + ": " + error.message(),
                .environment = {}};
    }
    if (runner->proton.empty()) {
        game.launch.args.insert(game.launch.args.end(), {"--wine", runner->wine.string(),
                                                         "--wine-prefix", prefix.string()});
        return {};
    }
    if (std::string refused = keepWineSaves(prefix); !refused.empty()) {
        return {.refusal = std::move(refused), .environment = {}};
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
