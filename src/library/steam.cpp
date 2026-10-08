#include "steam.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <map>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <system_error>

#include "lucent/log.h"

#include "vdf/node.hpp"

namespace iideck::library::steam {
namespace {

namespace fs = std::filesystem;
using vdf::Node;

/// The standard install locations, in the order Steam itself prefers.
const std::vector<std::string>& knownRoots() {
    static const std::vector<std::string> roots{
        ".local/share/Steam",
        ".steam/steam",
        ".steam/root",
        ".steam/debian-installation",
        ".var/app/com.valvesoftware.Steam/.local/share/Steam",
    };
    return roots;
}

/// Artwork candidates, tried in order. Steam stores portrait art under one name
/// and, since 2023, landscape hero art under another; older caches hold only the
/// legacy grid images.
const std::vector<std::string>& portraitCandidates() {
    static const std::vector<std::string> candidates{
        "appcache/librarycache/{}/library_600x900.jpg",
        "appcache/librarycache/{}/library_600x900.png",
        "appcache/librarycache/{}_600x900.jpg",
        "config/grid/{}p.jpg",
        "config/grid/{}.jpg",
        "config/grid/{}p.png",
    };
    return candidates;
}

const std::vector<std::string>& wideCandidates() {
    static const std::vector<std::string> candidates{
        "appcache/librarycache/{}_library_hero.jpg",
        "appcache/librarycache/{}_library_hero.png",
        "appcache/librarycache/{}/library_hero.jpg",
        "appcache/librarycache/{}/library_hero.png",
    };
    return candidates;
}

/// What a user's configuration says about one app.
struct PlayRecord {
    std::chrono::system_clock::time_point lastPlayed{};
    int playtimeMinutes{0};
    bool favourite{false};
};

/// True when the path exists and is a directory, with symlinks resolved so
/// artwork lookups do not depend on which path was discovered.
std::optional<fs::path> existingDirectory(const fs::path& path) {
    std::error_code ec;
    const fs::path resolved = fs::weakly_canonical(path, ec);
    if (ec || !fs::is_directory(resolved, ec)) {
        return std::nullopt;
    }
    return resolved;
}

/// True when the directory holds both halves of a Steam install.
bool isInstallRoot(const fs::path& path) {
    return fs::is_directory(path / "steamapps") && fs::is_directory(path / "config");
}

/// The first candidate that is a readable file.
fs::path firstExisting(const fs::path& root, const std::vector<std::string>& patterns,
                       std::string_view appId) {
    for (const std::string& pattern : patterns) {
        auto view = std::string_view{pattern};
        const std::size_t slot = view.find("{}");
        if (slot == std::string_view::npos) {
            continue;
        }
        std::string filled{view.substr(0, slot)};
        filled.append(appId);
        filled.append(view.substr(slot + 2));
        const fs::path candidate = root / filled;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

/// Steam's own components are not games. App manifests carry no tool-versus-game
/// marker; that lives in appinfo.vdf, whose v29 layout is not decoded yet, so
/// these are matched by the directory names Steam installs its components
/// under. Narrow and documented rather than silent, and recorded as a
/// stopgap in docs/project-state.md.
bool isSteamComponent(const std::string& installDir) {
    constexpr std::string_view prefixes[]{
        "Proton ", "SteamLinuxRuntime", "Steamworks Shared", "Steam Controller Configs", "SteamVR",
    };
    for (const std::string_view prefix : prefixes) {
        if (installDir.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

/// Steam's own install flag, authoritative even when the game directory has been
/// moved or is a symlink.
/// StateFlags bits: the app is fully installed; an update must be applied before it runs.
constexpr long long kFullyInstalled = 4;
constexpr long long kUpdateRequired = 2;

bool stateFlagsInstalled(const Node& state) {
    const std::optional<long long> flags = state.integer({"StateFlags"});
    return flags.has_value() && (*flags & kFullyInstalled) != 0;
}

/// Merges every user profile's app data. Later profiles win, which matches how
/// Steam treats a shared machine.
std::map<std::string, PlayRecord, std::less<>> loadUserData(const fs::path& root) {
    // Keyed by app id, because an app appears under both Apps and Favorites and
    // the two must merge rather than overwrite each other.
    std::map<std::string, PlayRecord, std::less<>> records;

    std::error_code ec;
    fs::directory_iterator entries{root / "userdata", ec};
    if (ec) {
        return records;
    }

    for (const fs::directory_entry& entry : entries) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        // "0" and "ac" are Steam's own bookkeeping directories.
        if (name == "0" || name == "ac") {
            continue;
        }
        const std::optional<Node> doc = vdf::parseFile(entry.path() / "config" / "localconfig.vdf");
        if (!doc) {
            continue;
        }
        // The whole document is wrapped in this key.
        const Node& user = doc->block({"UserLocalConfigStore"}).value_or(*doc);

        if (const std::optional<Node> apps = user.block({"Software", "Valve", "Steam", "Apps"})) {
            for (const std::string& appId : apps->keys()) {
                const std::optional<Node> app = apps->block({appId});
                if (!app) {
                    continue;
                }
                PlayRecord record;
                if (const std::optional<std::string> raw = app->str({"LastPlayed"})) {
                    long long seconds = 0;
                    const auto [ptr, conv] =
                        std::from_chars(raw->data(), raw->data() + raw->size(), seconds);
                    if (conv == std::errc{} && ptr == raw->data() + raw->size() && seconds > 0) {
                        record.lastPlayed = std::chrono::system_clock::from_time_t(
                            static_cast<std::time_t>(seconds));
                    }
                }
                if (const std::optional<long long> minutes = app->integer({"Playtime"});
                    minutes && *minutes > 0) {
                    record.playtimeMinutes = static_cast<int>(*minutes);
                }
                if (app->block({"Categories"}).value_or(Node{}).has({"Favorites"})) {
                    record.favourite = true;
                }
                PlayRecord& stored = records[appId];
                stored.lastPlayed = record.lastPlayed;
                stored.playtimeMinutes = record.playtimeMinutes;
                stored.favourite = stored.favourite || record.favourite;
            }
        }

        if (const std::optional<Node> favourites =
                user.block({"Software", "Valve", "Steam", "Favorites"})) {
            for (const std::string& appId : favourites->keys()) {
                records[appId].favourite = true;
            }
        }
    }
    return records;
}

/// The app id from a manifest filename, ordered numerically so the catalog is
/// stable regardless of directory order.
std::string appIdOf(const fs::path& manifest) {
    std::string name = manifest.filename().string();
    constexpr std::string_view prefix{"appmanifest_"};
    if (name.starts_with(prefix)) {
        name.erase(0, prefix.size());
    }
    if (name.ends_with(".acf")) {
        name.erase(name.size() - 4);
    }
    return name;
}

bool lessByAppId(const fs::path& a, const fs::path& b) {
    return appIdOf(a) < appIdOf(b);
}

/// Reads one app manifest. Returns nothing when the manifest is unreadable or
/// carries no name.
std::optional<Game> readManifest(const fs::path& root, const fs::path& libraryPath,
                                 const fs::path& manifestPath,
                                 const std::map<std::string, PlayRecord, std::less<>>& play) {
    const std::optional<Node> doc = vdf::parseFile(manifestPath);
    if (!doc) {
        return std::nullopt;
    }
    const std::optional<Node> state = doc->block({"AppState"});
    if (!state) {
        return std::nullopt;
    }
    const std::optional<std::string> name = state->str({"name"});
    if (!name || name->empty()) {
        return std::nullopt;
    }

    std::string appId = appIdOf(manifestPath);
    if (const std::optional<long long> declared = state->integer({"appid"});
        declared && *declared != 0) {
        appId = std::to_string(*declared);
    }
    const std::string installDir = state->str({"installdir"}).value_or("");
    if (isSteamComponent(installDir)) {
        return std::nullopt;
    }

    bool installed = stateFlagsInstalled(*state);
    if (!installDir.empty()) {
        std::error_code ec;
        if (fs::is_directory(libraryPath / "steamapps" / "common" / installDir, ec)) {
            installed = true;
        }
    }

    Game game;
    // The id prefix is a stable key, not the display label, and matches the
    // other sources' lowercase scheme.
    game.id = "steam:" + appId;
    game.source = Source::Steam;
    game.sourceId = appId;
    game.title = *name;
    game.installed = installed;
    game.launch = LaunchSpec{.program = "steam", .args = {"-applaunch", appId}};
    // Steam runs every game, native or Proton, under `reaper SteamLaunch AppId=<id>`;
    // the trailing NUL ends the argument so AppId=44 cannot match AppId=440.
    game.processHint = "AppId=" + appId + std::string(1, '\0');

    if (const auto found = play.find(appId); found != play.end()) {
        const PlayRecord& record = found->second;
        game.playtimeMinutes = record.playtimeMinutes;
        game.favourite = record.favourite;
        if (record.lastPlayed != std::chrono::system_clock::time_point{}) {
            game.lastPlayed = record.lastPlayed;
        }
    }

    game.artwork = firstExisting(root, portraitCandidates(), appId);
    game.artworkWide = firstExisting(root, wideCandidates(), appId);
    return game;
}

/// Reads every app manifest in one library folder.
std::vector<Game> readLibrary(const fs::path& root, const fs::path& libraryPath,
                              const std::map<std::string, PlayRecord, std::less<>>& play) {
    std::vector<fs::path> manifests;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{libraryPath / "steamapps", ec}) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename().string().starts_with("appmanifest_")) {
            manifests.push_back(entry.path());
        }
    }
    if (ec) {
        return {};
    }
    std::ranges::sort(manifests, lessByAppId);

    std::vector<Game> games;
    for (const fs::path& manifest : manifests) {
        if (std::optional<Game> game = readManifest(root, libraryPath, manifest, play)) {
            games.push_back(std::move(*game));
        }
    }
    return games;
}

/// Drops folders with no steamapps directory, then returns one entry per
/// distinct path and per distinct content id. Steam keeps entries for drives
/// that are not mounted, and a stale one must never win over a mounted one that
/// holds the games.
std::vector<LibraryFolder> presentFolders(std::vector<LibraryFolder> candidates) {
    std::vector<LibraryFolder> out;
    std::vector<fs::path> seenPaths;
    std::vector<std::string> seenIds;

    for (const LibraryFolder& folder : candidates) {
        if (std::ranges::find(seenPaths, folder.path) != seenPaths.end()) {
            continue;
        }
        if (!fs::is_directory(folder.path / "steamapps")) {
            continue;
        }
        seenPaths.push_back(folder.path);
        if (!folder.contentId.empty()) {
            if (std::ranges::find(seenIds, folder.contentId) != seenIds.end()) {
                continue;
            }
            seenIds.push_back(folder.contentId);
        }
        out.push_back(folder);
    }
    return out;
}

/// The extra libraries Steam records, accepting both layouts.
std::vector<LibraryFolder> readExtraFolders(const fs::path& root) {
    const std::optional<Node> doc = vdf::parseFile(root / "steamapps" / "libraryfolders.vdf");
    if (!doc) {
        return {};
    }
    for (std::string_view key : {"libraryfolders", "LibraryFolders"}) {
        const std::optional<Node> block = doc->block({key});
        if (!block) {
            continue;
        }
        std::vector<LibraryFolder> folders;
        for (const std::string& entry : block->keys()) {
            const std::optional<vdf::Node::Value> value = block->find(entry);
            if (!value) {
                continue;
            }
            LibraryFolder folder;
            if (const std::string* text = std::get_if<std::string>(&*value); text != nullptr) {
                // Legacy layout: "1" "/mnt/games/SteamLibrary".
                folder.path = *text;
            } else if (const Node* child = std::get_if<Node>(&*value); child != nullptr) {
                // Current layout: "1" { "path" "..." "contentid" "..." }.
                folder.path = child->str({"path"}).value_or("");
                folder.contentId = child->str({"contentid"}).value_or("");
            }
            if (folder.path.empty()) {
                continue;
            }
            if (std::optional<fs::path> resolved = existingDirectory(folder.path)) {
                folder.path = *resolved;
            }
            folders.push_back(std::move(folder));
        }
        return folders;
    }
    return {};
}

} // namespace

Library Library::discover(const fs::path& home, const std::vector<fs::path>& explicitRoots) {
    std::vector<fs::path> candidates = explicitRoots;
    if (candidates.empty() && !home.empty()) {
        for (const std::string& rel : knownRoots()) {
            candidates.push_back(home / rel);
        }
    }

    Library library;
    std::vector<fs::path> seen;
    for (const fs::path& candidate : candidates) {
        const std::optional<fs::path> resolved = existingDirectory(candidate);
        if (!resolved) {
            continue;
        }
        if (std::ranges::find(seen, *resolved) != seen.end()) {
            continue;
        }
        if (!isInstallRoot(*resolved)) {
            continue;
        }
        seen.push_back(*resolved);
        library.roots_.push_back(*resolved);
    }
    return library;
}

std::vector<LibraryFolder> Library::libraryFolders() const {
    std::vector<LibraryFolder> folders;
    for (const fs::path& root : roots_) {
        // The install root always comes first so its own manifests win.
        folders.push_back(LibraryFolder{.path = root});
        for (LibraryFolder& extra : readExtraFolders(root)) {
            if (extra.path != root) {
                folders.push_back(std::move(extra));
            }
        }
    }
    return presentFolders(std::move(folders));
}

double AppUpdate::progress() const noexcept {
    const std::uint64_t total = toDownload + toStage;
    if (total == 0) {
        return 0.0;
    }
    return std::min(1.0, static_cast<double>(downloaded + staged) / static_cast<double>(total));
}

std::optional<AppUpdate> Library::pendingUpdate(std::string_view appId) const {
    const std::string file = "appmanifest_" + std::string{appId} + ".acf";
    for (const LibraryFolder& folder : libraryFolders()) {
        const fs::path manifest = folder.path / "steamapps" / file;
        std::error_code ec;
        if (!fs::is_regular_file(manifest, ec)) {
            continue;
        }
        const std::optional<Node> doc = vdf::parseFile(manifest);
        const std::optional<Node> state = doc ? doc->block({"AppState"}) : std::nullopt;
        if (!state) {
            continue;
        }
        const long long flags = state->integer({"StateFlags"}).value_or(0);
        if ((flags & kUpdateRequired) == 0) {
            return std::nullopt;
        }
        const auto bytes = [&state](std::string_view key) {
            return static_cast<std::uint64_t>(std::max(0LL, state->integer({key}).value_or(0)));
        };
        return AppUpdate{.downloaded = bytes("BytesDownloaded"),
                         .toDownload = bytes("BytesToDownload"),
                         .staged = bytes("BytesStaged"),
                         .toStage = bytes("BytesToStage")};
    }
    return std::nullopt;
}

std::vector<Game> Library::list() const {
    if (roots_.empty()) {
        throw std::runtime_error{"no Steam installation found"};
    }

    std::vector<Game> games;
    std::vector<std::string> seen;
    std::vector<std::string> problems;

    for (const fs::path& root : roots_) {
        const std::map<std::string, PlayRecord, std::less<>> play = loadUserData(root);
        for (const LibraryFolder& folder : libraryFolders()) {
            std::vector<Game> found;
            try {
                found = readLibrary(root, folder.path, play);
            } catch (const std::exception& error) {
                problems.push_back(folder.path.string() + ": " + error.what());
                continue;
            }
            for (Game& game : found) {
                // One app id is one tile, even when Steam lists the same app in
                // two library folders.
                if (std::ranges::find(seen, game.id) != seen.end()) {
                    continue;
                }
                seen.push_back(game.id);
                games.push_back(std::move(game));
            }
        }
    }

    if (games.empty() && !problems.empty()) {
        throw std::runtime_error{std::accumulate(problems.begin(), problems.end(), std::string{},
                                                 [](std::string acc, const std::string& p) {
                                                     return acc.empty() ? p : acc + "; " + p;
                                                 })};
    }
    for (const std::string& problem : problems) {
        lucent::warn("steam", "library folder unreadable: {}", problem);
    }
    return games;
}

Provider::Provider(Library library) : library_{std::move(library)} {
}

std::vector<Game> Provider::list() {
    return library_.list();
}

} // namespace iideck::library::steam