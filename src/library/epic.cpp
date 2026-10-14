#include "epic.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "install_log.hpp"
#include "launch/command.hpp"
#include "lucent/log.h"

namespace opensu::library::epic {
namespace {

using nlohmann::json;

/// One owned title from `legendary list --json`.
struct Owned {
    std::string appName;
    std::string title;
    std::string artworkUrl;
    /// `asset_infos.<platform>.build_version`: the build Epic offers per platform.
    std::map<std::string, std::string, std::less<>> builds;
};

/// One installed title from `legendary list-installed --json`.
struct Install {
    std::string installPath;
    bool dlc{false};
    std::string version;
    /// legendary's platform name, a key of `asset_infos`.
    std::string platform;
};

/// Key image types in preference order: the portrait box, then the wide box, then the thumbnail.
constexpr std::array<std::string_view, 3> imageTypes{"DieselGameBoxTall", "DieselGameBox",
                                                     "Thumbnail"};

/// Some key image URLs carry spaces ("public testing EGS 1200 x 1600 library_..."), which curl
/// refuses as an illegal URL and which would end a whole artwork round.
std::string encodeSpaces(std::string url) {
    for (std::size_t at = url.find(' '); at != std::string::npos; at = url.find(' ', at)) {
        url.replace(at, 1, "%20");
    }
    return url;
}

/// The URL of the preferred key image in `metadata.keyImages`; empty when there is none.
std::string artworkUrl(const json& entry) {
    const json::const_iterator metadata = entry.find("metadata");
    if (metadata == entry.end() || !metadata->is_object()) {
        return {};
    }
    const json::const_iterator images = metadata->find("keyImages");
    if (images == metadata->end() || !images->is_array()) {
        return {};
    }
    for (const std::string_view type : imageTypes) {
        for (const json& image : *images) {
            if (image.value("type", "") == type && image.value("url", "").starts_with("https://")) {
                return encodeSpaces(image.value("url", ""));
            }
        }
    }
    return {};
}

/// `asset_infos` as platform to build version.
std::map<std::string, std::string, std::less<>> buildsOf(const json& entry) {
    std::map<std::string, std::string, std::less<>> builds;
    const json::const_iterator assets = entry.find("asset_infos");
    if (assets == entry.end() || !assets->is_object()) {
        return builds;
    }
    for (const auto& [platform, asset] : assets->items()) {
        if (asset.is_object()) {
            builds[platform] = asset.value("build_version", "");
        }
    }
    return builds;
}

std::vector<Owned> parseOwned(std::string_view output) {
    std::vector<Owned> owned;
    for (const json& entry : json::parse(output)) {
        owned.push_back(Owned{.appName = entry.value("app_name", ""),
                              .title = entry.value("app_title", ""),
                              .artworkUrl = artworkUrl(entry),
                              .builds = buildsOf(entry)});
    }
    return owned;
}

std::map<std::string, Install, std::less<>> parseInstalls(std::string_view output) {
    std::map<std::string, Install, std::less<>> installs;
    for (const json& entry : json::parse(output)) {
        installs[entry.value("app_name", "")] =
            Install{.installPath = entry.value("install_path", ""),
                    .dlc = entry.value("is_dlc", false),
                    .version = entry.value("version", ""),
                    .platform = entry.value("platform", "Windows")};
    }
    return installs;
}

/// legendary's own rule (`cli.py:list_installed`, the `--check-updates` column): the installed
/// version differs from the build Epic offers for the installed platform. Unknown is no update.
bool updateAvailable(const Owned& title, const Install& install) {
    const auto offered = title.builds.find(install.platform);
    return offered != title.builds.end() && !offered->second.empty() && !install.version.empty() &&
           offered->second != install.version;
}

constexpr std::string_view failureMarker = " ! Failure: ";

} // namespace

std::optional<std::string> installFailure(std::string_view line) {
    if (std::optional<std::string> logged = install_log::loggedError(line)) {
        return logged;
    }
    line = install_log::trimmed(line);
    if (line.starts_with(failureMarker)) {
        return std::string{line.substr(failureMarker.size())};
    }
    return std::nullopt;
}

std::string Provider::run(const std::vector<std::string>& arguments) const {
    // Legendary logs to stderr, so only stdout is captured.
    const std::optional<launch::Captured> result = launch::runCaptured(binary_, arguments);
    if (!result) {
        throw SourceAbsent{"legendary is not installed"};
    }
    if (result->status != 0) {
        // Legendary exits non-zero when it has no saved credentials.
        throw std::runtime_error{"legendary is not logged in"};
    }
    return result->output;
}

Provider::Provider() : binary_{"legendary"} {
}

Provider::Provider(std::string binary) : binary_{std::move(binary)} {
}

std::vector<Game> Provider::list() {
    const std::vector<Owned> owned = parseOwned(run({"list", "--json"}));
    const auto installs = parseInstalls(run({"list-installed", "--json"}));

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
        game.artworkUrl = title.artworkUrl;
        game.installed = install != installs.end();
        game.updateAvailable = game.installed && updateAvailable(title, install->second);
        game.launch = LaunchSpec{.program = "legendary", .args = {"launch", title.appName}};
        game.windowsInstall = game.installed && install->second.platform == "Windows";
        if (game.installed && !install->second.installPath.empty()) {
            // The install folder name identifies the running game.
            game.processHint =
                std::filesystem::path{install->second.installPath}.filename().string();
        }
        games.push_back(std::move(game));
    }
    lucent::info("epic", "legendary listed {} owned titles, {} installed, {} with an update",
                 games.size(), installs.size(),
                 std::ranges::count_if(games, &Game::updateAvailable));
    return games;
}

std::filesystem::path legendaryInstallDir(const std::filesystem::path& legendaryConfigDir,
                                          const std::filesystem::path& home) {
    namespace fs = std::filesystem;
    std::ifstream in{legendaryConfigDir / "config.ini"};
    std::string line;
    bool inSection = false;
    const auto trimmed = [](const std::string& text) {
        const auto first = text.find_first_not_of(" \t\r");
        const auto last = text.find_last_not_of(" \t\r");
        return first == std::string::npos ? std::string{} : text.substr(first, last - first + 1);
    };
    while (std::getline(in, line)) {
        line = trimmed(line);
        if (line.starts_with("[")) {
            inSection = line == "[Legendary]";
            continue;
        }
        const std::size_t equals = line.find_first_of("=:");
        if (!inSection || equals == std::string::npos ||
            trimmed(line.substr(0, equals)) != "install_dir") {
            continue;
        }
        const std::string value = trimmed(line.substr(equals + 1));
        if (value == "~" || value.starts_with("~/")) {
            return home / value.substr(std::min<std::size_t>(value.size(), 2));
        }
        if (!value.empty()) {
            return fs::path{value};
        }
    }
    return home / "Games";
}

} // namespace opensu::library::epic