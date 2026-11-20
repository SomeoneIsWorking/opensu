#include "fumarole_installs.hpp"

#include <algorithm>
#include <stdexcept>
#include <system_error>

#include "lucent/log.h"
#include <nlohmann/json.hpp>

#include "fileio/host_path.hpp"
#include "launch/command.hpp"
#include "vdf/node.hpp"

namespace opensu::library::fumarole {

namespace fs = std::filesystem;

Installs Installs::find(const std::vector<fs::path>& executablePath, const fs::path& dataDir) {
    return Installs{.program = launch::resolveExecutable("fumarole", executablePath),
                    .dataDir = dataDir};
}

std::vector<Game> Installs::games() const {
    std::vector<Game> found;
    std::error_code error;
    if (program.empty() || !fs::is_directory(dataDir / "installed", error)) {
        return found;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator{dataDir / "installed", error}) {
        if (entry.path().extension() != ".vdf") {
            continue;
        }
        if (std::optional<Game> game = gameOf(entry.path(), program)) {
            found.push_back(std::move(*game));
        }
    }
    std::ranges::sort(found, {}, &Game::id);
    return found;
}

std::vector<Game> Installs::owned() const {
    if (program.empty()) {
        return {};
    }
    const std::optional<launch::Captured> result =
        launch::runCaptured(program.string(), {"list", "--json"});
    if (!result || result->status != 0) {
        lucent::warn("fumarole", "the owned games could not be listed; is fumarole signed in?");
        return {};
    }
    try {
        return parseOwned(result->output);
    } catch (const std::runtime_error& error) {
        lucent::warn("fumarole", "the owned games list is unreadable: {}", error.what());
        return {};
    }
}

std::vector<Game> parseOwned(std::string_view text) {
    const nlohmann::json list = nlohmann::json::parse(text, nullptr, false);
    if (!list.is_array()) {
        throw std::runtime_error{"not a JSON array"};
    }
    std::vector<Game> games;
    for (const nlohmann::json& app : list) {
        if (!app.is_object() || !app.value("app_id", nlohmann::json{}).is_number_unsigned() ||
            !app.value("name", nlohmann::json{}).is_string()) {
            continue;
        }
        const nlohmann::json systems = app.value("os", nlohmann::json::array());
        const auto has = [&systems](std::string_view os) {
            return systems.is_array() &&
                   std::ranges::any_of(systems, [os](const nlohmann::json& system) {
                       return system.is_string() && system.get<std::string>() == os;
                   });
        };
        Game game;
        game.sourceId = std::to_string(app["app_id"].get<std::uint64_t>());
        game.id = "steam:" + game.sourceId;
        game.source = Source::Steam;
        game.title = app["name"].get<std::string>();
        game.builds = {.windows = has("windows"), .linuxNative = has("linux"), .mac = has("macos")};
        game.fromFumarole = true;
        if (game.builds.windows) {
            games.push_back(std::move(game));
        }
    }
    return games;
}

std::optional<double> installProgress(std::string_view line) {
    const nlohmann::json event = nlohmann::json::parse(line, nullptr, false);
    if (!event.is_object() || event.value("event", "") != "progress") {
        return std::nullopt;
    }
    const double total = event.value("bytes_total", 0.0);
    if (total <= 0) {
        return std::nullopt;
    }
    return std::clamp(event.value("bytes_done", 0.0) / total, 0.0, 1.0);
}

std::optional<std::string> installFailure(std::string_view line) {
    constexpr std::string_view marker = "[fumarole:error] ";
    const std::size_t at = line.find(marker);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string{line.substr(at + marker.size())};
}

std::optional<Game> gameOf(const fs::path& record, const fs::path& program) {
    const std::optional<vdf::Node> doc = vdf::parseFile(record);
    const std::optional<vdf::Node> install = doc ? doc->block({"install"}) : std::nullopt;
    if (!install) {
        return std::nullopt;
    }
    const std::optional<std::string> appId = install->str({"appid"});
    const std::optional<std::string> name = install->str({"name"});
    const std::optional<std::string> path = install->str({"path"});
    if (!appId || appId->empty() || !name || !path || install->str({"os"}) != "windows" ||
        install->str({"state"}) != "installed") {
        return std::nullopt;
    }
    std::error_code error;
    if (!fs::is_directory(*path, error)) {
        return std::nullopt;
    }
    Game game;
    game.id = "steam:" + *appId;
    game.source = Source::Steam;
    game.sourceId = *appId;
    game.title = *name;
    game.installed = true;
    game.windowsInstall = true;
    game.fromFumarole = true;
    game.launch = LaunchSpec{.program = program.string(), .args = {"launch", *appId}};
    // Wine starts the game's programs outside fumarole's process group, named by their Windows
    // path.
    game.processHint = fileio::pathInWine(fs::path{*path} / "");
    return game;
}

} // namespace opensu::library::fumarole
