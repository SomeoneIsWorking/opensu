#include "wine.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "launch/command.hpp"

namespace opensu::library::wine {
namespace {

namespace fs = std::filesystem;

constexpr const char* sharedPrefixName = "wine-prefix";
constexpr const char* ownPrefixesName = "wine-prefixes";

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
            runners.push_back(Runner{.name = name, .wine = std::move(binary)});
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

} // namespace

std::vector<Runner> discover(const Search& search) {
    std::vector<Runner> runners;
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

fs::path runnerOf(const Choice& choice) {
    if (!choice.runner.empty()) {
        return choice.runner;
    }
    return choice.found.empty() ? fs::path{} : choice.found.front().wine;
}

std::string prepare(Game& game, const Choice& choice) {
    if (!isGameStore(game.source) || !game.windowsInstall || game.launch.empty()) {
        return {};
    }
    const fs::path runner = runnerOf(choice);
    if (runner.empty()) {
        return "No Wine found to run " + game.title + "; install Wine or a Proton build";
    }
    std::error_code error;
    if (!fs::is_regular_file(runner, error)) {
        return "The Wine runner " + runner.string() + " is gone; pick another in Settings";
    }
    const fs::path prefix = prefixFor(choice.dataDir, game, choice.own);
    fs::create_directories(prefix, error);
    if (error) {
        return "Cannot make the Wine prefix " + prefix.string() + ": " + error.message();
    }
    game.launch.args.insert(game.launch.args.end(),
                            {"--wine", runner.string(), "--wine-prefix", prefix.string()});
    return {};
}

} // namespace opensu::library::wine
