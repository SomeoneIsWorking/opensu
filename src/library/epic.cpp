#include "epic.hpp"

#include <array>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <sys/wait.h>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace iideck::library::epic {
namespace {

using nlohmann::json;

/// One owned title from `legendary list --json`.
struct Owned {
    std::string appName;
    std::string title;
};

/// One installed title from `legendary list-installed --json`.
struct Install {
    std::string installPath;
    bool dlc{false};
};

std::vector<Owned> parseOwned(std::string_view output) {
    std::vector<Owned> owned;
    for (const json& entry : json::parse(output)) {
        owned.push_back(
            Owned{.appName = entry.value("app_name", ""), .title = entry.value("app_title", "")});
    }
    return owned;
}

std::map<std::string, Install, std::less<>> parseInstalls(std::string_view output) {
    std::map<std::string, Install, std::less<>> installs;
    for (const json& entry : json::parse(output)) {
        installs[entry.value("app_name", "")] = Install{
            .installPath = entry.value("install_path", ""), .dlc = entry.value("is_dlc", false)};
    }
    return installs;
}

} // namespace

std::string Provider::run(std::string_view arguments) const {
    // Legendary logs to stderr, so only stdout is captured.
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen((binary_ + " " + std::string{arguments} + " 2>/dev/null").c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error{"legendary is not ready"};
    }
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int status = pclose(pipe);
    // The shell's code for a command it cannot find.
    constexpr int commandNotFound = 127;
    if (WIFEXITED(status) && WEXITSTATUS(status) == commandNotFound) {
        throw SourceAbsent{"legendary is not installed"};
    }
    if (status != 0) {
        // Legendary exits non-zero when it has no saved credentials.
        throw std::runtime_error{"legendary is not logged in"};
    }
    return output;
}

Provider::Provider() : binary_{"legendary"} {
}

Provider::Provider(std::string binary) : binary_{std::move(binary)} {
}

std::vector<Game> Provider::list() {
    const std::vector<Owned> owned = parseOwned(run("list --json"));
    const auto installs = parseInstalls(run("list-installed --json"));

    std::vector<Game> games;
    for (const Owned& title : owned) {
        const auto install = installs.find(title.appName);
        // DLC is not a tile of its own; it belongs to its base game.
        if (title.appName.empty() || (install != installs.end() && install->second.dlc)) {
            continue;
        }
        Game game;
        game.id = "epic:" + title.appName;
        game.source = Source::Epic;
        game.sourceId = title.appName;
        game.title = title.title.empty() ? title.appName : title.title;
        game.installed = install != installs.end();
        game.launch = LaunchSpec{.program = "legendary", .args = {"launch", title.appName}};
        if (game.installed && !install->second.installPath.empty()) {
            // The install folder name identifies the running game.
            game.processHint =
                std::filesystem::path{install->second.installPath}.filename().string();
        }
        games.push_back(std::move(game));
    }
    lucent::info("epic", "legendary listed {} owned titles, {} installed", games.size(),
                 installs.size());
    return games;
}

} // namespace iideck::library::epic