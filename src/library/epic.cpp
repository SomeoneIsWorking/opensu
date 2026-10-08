#include "epic.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <sys/wait.h>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace iideck::library::epic {
namespace {

using nlohmann::json;

/// One entry of Legendary's install list.
struct Install {
    std::string appName;
    std::string title;
    std::string installPath;
    bool installed{false};
    bool dlc{false};

    [[nodiscard]] static Install from(const json& entry, std::string fallbackName) {
        Install out;
        out.appName = entry.value("app_name", fallbackName);
        out.title = entry.value("title", "");
        out.installPath = entry.value("install_path", "");
        out.installed = entry.value("installed", false);
        out.dlc = entry.value("is_dlc", false);
        return out;
    }
};

/// Legendary has emitted both a bare array and an object keyed by app name,
/// depending on version, so both shapes are accepted.
std::vector<Install> parseInstalls(std::string_view output) {
    const json document = json::parse(output);

    std::vector<Install> installs;
    if (document.is_array()) {
        for (const json& entry : document) {
            installs.push_back(Install::from(entry, ""));
        }
        return installs;
    }
    if (document.is_object()) {
        for (const auto& [name, entry] : document.items()) {
            if (entry.is_object()) {
                installs.push_back(Install::from(entry, name));
            }
        }
    }
    return installs;
}

} // namespace

Provider::Provider() : binary_{"legendary"} {
}

Provider::Provider(std::string binary) : binary_{std::move(binary)} {
}

std::vector<Game> Provider::list() {
    // Legendary logs to stderr, so only stdout is captured.
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen((binary_ + " list --output json 2>/dev/null").c_str(), "r");
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
        // Legendary exits non-zero when it has no saved credentials, which means
        // there is nothing to list.
        throw std::runtime_error{"legendary is not logged in"};
    }

    std::vector<Game> games;
    for (const Install& install : parseInstalls(output)) {
        // DLC is not a tile of its own; it belongs to its base game.
        if (install.dlc || install.appName.empty()) {
            continue;
        }
        Game game;
        game.id = "epic:" + install.appName;
        game.source = Source::Epic;
        game.sourceId = install.appName;
        game.title = install.title.empty() ? install.appName : install.title;
        game.installed = install.installed;
        game.launch = LaunchSpec{.program = "legendary", .args = {"launch", install.appName}};
        if (!install.installPath.empty()) {
            // The install folder name identifies the running game.
            game.processHint = std::filesystem::path{install.installPath}.filename().string();
        }
        games.push_back(std::move(game));
    }
    lucent::info("epic", "legendary listed {} titles", games.size());
    return games;
}

} // namespace iideck::library::epic