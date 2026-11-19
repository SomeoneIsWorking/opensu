#include "fumarole_installs.hpp"

#include <algorithm>
#include <system_error>

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
