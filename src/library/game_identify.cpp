#include "game_identify.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <span>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/checked_bytes.hpp"
#include "net/web_client.hpp"
#include "pe_resources.hpp"
#include "titles.hpp"

namespace opensu::library::identify {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;

/// Folders above the file that are searched for clues.
constexpr int mostFolders = 4;
/// The most clues looked up in the store.
constexpr std::size_t mostSearches = 6;

/// Names of engines' placeholders, as `titleKey`s.
constexpr std::array<std::string_view, 10> placeholderNames{
    "bootstrappackagedgame", "unrealgame", "ue4game",  "ue5game",
    "unityplayer",           "unity",      "launcher", "game",
    "application",           "setup"};
/// File names that say nothing about the game.
constexpr std::array<std::string_view, 8> launcherNames{"start", "apprun", "launcher", "launch",
                                                        "game",  "run",    "play",     "setup"};
/// Build words at the end of a file name.
constexpr std::array<std::string_view, 15> buildWords{
    "steam", "shipping", "win64", "win32", "x64",  "x86",    "x8664", "amd64",
    "linux", "dx11",     "dx12",  "gog",   "epic", "retail", "egs"};
/// Folders inside a game that say nothing about it, as `titleKey`s.
constexpr std::array<std::string_view, 13> innerFolders{
    "bin", "bin32", "bin64", "binaries", "win32",  "win64",  "x64",
    "x86", "x8664", "game",  "system",   "retail", "release"};
/// Folders that hold games rather than being one, as `titleKey`s: the walk up stops there.
constexpr std::array<std::string_view, 18> libraryFolders{
    "games",        "appimages",    "applications",    "apps",   "downloads",
    "desktop",      "programfiles", "programfilesx86", "drivec", "common",
    "steamapps",    "opt",          "goggames",        "gog",    "epicgames",
    "steamlibrary", "steam",        "xboxgames"};

template <std::size_t N>
bool among(const std::array<std::string_view, N>& names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

/// The value of String `key` in a VS_VERSIONINFO block.
std::string versionString(std::span<const std::uint8_t> block, std::string_view key) {
    Bytes pattern;
    for (const char c : key) {
        pattern.push_back(static_cast<unsigned char>(c));
        pattern.push_back(0);
    }
    pattern.push_back(0);
    pattern.push_back(0);
    const auto found = std::ranges::search(block, pattern);
    if (found.empty()) {
        return {};
    }
    const auto keyAt = static_cast<std::size_t>(found.begin() - block.begin());
    // A String is wLength, wValueLength, wType, then its key; the value starts 32-bit aligned.
    if (keyAt < 6) {
        return {};
    }
    const fileio::CheckedBytes bytes{block};
    const std::size_t end = keyAt - 6 + bytes.u16(keyAt - 6).value_or(0);
    const std::size_t valueAt = (keyAt + pattern.size() + 3) & ~static_cast<std::size_t>(3);
    std::string value = end > valueAt ? bytes.utf16(valueAt, (end - valueAt) / 2) : std::string{};
    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    return value;
}

/// `name` without the trademark signs a store puts in titles.
std::string unmarked(std::string name) {
    for (const std::string_view mark : {"\xE2\x84\xA2", "\xC2\xAE", "\xC2\xA9"}) {
        for (std::size_t at = name.find(mark); at != std::string::npos; at = name.find(mark)) {
            name.erase(at, mark.size());
        }
    }
    return name;
}

/// The `Name=` of a .desktop entry in `dir` whose `Exec=` names `file`.
std::string desktopName(const fs::path& dir, const fs::path& file) {
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir, error}) {
        if (lowered(entry.path().extension().string()) != ".desktop") {
            continue;
        }
        std::ifstream in{entry.path()};
        std::string name;
        bool starts = false;
        for (std::string line; std::getline(in, line);) {
            if (line.starts_with("Name=") && name.empty()) {
                name = line.substr(5);
            } else if (line.starts_with("Exec=")) {
                starts = line.find(file.filename().string()) != std::string::npos;
            }
        }
        if (starts && !name.empty()) {
            return name;
        }
    }
    return {};
}

/// The `name` of a GOG `goggame-*.info` in `dir`.
std::string gogName(const fs::path& dir) {
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir, error}) {
        const std::string name = entry.path().filename().string();
        if (!name.starts_with("goggame-") || !name.ends_with(".info")) {
            continue;
        }
        std::ifstream in{entry.path()};
        const nlohmann::json info = nlohmann::json::parse(in, nullptr, false);
        if (info.is_object() && info.contains("name") && info["name"].is_string()) {
            return info["name"].get<std::string>();
        }
    }
    return {};
}

/// The app id in `dir/steam_appid.txt`.
std::string steamAppIdIn(const fs::path& dir) {
    std::ifstream in{dir / "steam_appid.txt"};
    std::string id;
    in >> id;
    return isSteamAppId(id) ? id : std::string{};
}

void addClue(std::vector<Clue>& names, std::string name, const char* from) {
    const std::string key = titleKey(name);
    if (key.empty() || std::ranges::any_of(names, [&key](const Clue& clue) {
            return titleKey(clue.name) == key;
        })) {
        return;
    }
    names.push_back({std::move(name), from});
}

} // namespace

bool mayNameAGame(const fs::path& dir) {
    std::vector<std::string> parts;
    for (const fs::path& part : dir.relative_path()) {
        if (!part.empty()) {
            parts.push_back(part.string());
        }
    }
    const bool drive = dir.has_root_name() || (!parts.empty() && parts.front().ends_with(':'));
    if (!dir.has_root_name() && drive) {
        parts.erase(parts.begin());
    }
    if (drive && (parts.empty() || titleKey(parts.front()) != "users")) {
        return !parts.empty();
    }
    return parts.size() > 2;
}

std::string versionName(const fs::path& exe) {
    const std::optional<pe::Resources> resources = pe::Resources::of(exe);
    if (!resources) {
        return {};
    }
    const std::span<const std::uint8_t> block = resources->at(pe::versionType, 0);
    if (block.empty()) {
        return {};
    }
    for (const std::string_view key : {"ProductName", "FileDescription"}) {
        std::string name = versionString(block, key);
        if (!titleKey(name).empty() && !among(placeholderNames, titleKey(name))) {
            return name;
        }
    }
    return {};
}

std::string fileTitle(const fs::path& file) {
    const std::string stem = file.stem().string();
    if (among(launcherNames, lowered(stem))) {
        return {};
    }
    std::vector<std::string> words;
    std::string word;
    for (const char c : stem + " ") {
        if (c == '_' || c == '-' || c == ' ' || c == '.') {
            if (!word.empty()) {
                words.push_back(std::exchange(word, {}));
            }
        } else {
            word += c;
        }
    }
    while (words.size() > 1 && among(buildWords, titleKey(words.back()))) {
        words.pop_back();
    }
    const bool lower = std::ranges::none_of(stem, [](unsigned char c) {
        return std::isupper(c) != 0;
    });
    std::string title;
    for (std::string& each : words) {
        if (lower && !each.empty()) {
            each.front() =
                static_cast<char>(std::toupper(static_cast<unsigned char>(each.front())));
        }
        title += (title.empty() ? "" : " ") + each;
    }
    return title;
}

Clues cluesOf(const fs::path& file, const fs::path& stopAt) {
    Clues clues;
    std::vector<Clue> folders;
    addClue(clues.names, desktopName(file.parent_path(), file), "its .desktop entry");
    fs::path dir = file.parent_path();
    for (int level = 0;
         level < mostFolders && dir != stopAt && (!stopAt.empty() || mayNameAGame(dir)); ++level) {
        if (clues.steamAppId.empty()) {
            clues.steamAppId = steamAppIdIn(dir);
        }
        const std::string name = dir.filename().string();
        if (among(libraryFolders, titleKey(name))) {
            break;
        }
        addClue(clues.names, gogName(dir), "GOG's game info");
        if (!among(innerFolders, titleKey(name))) {
            addClue(folders, name, "the folder's name");
        }
        dir = dir.parent_path();
    }
    for (Clue& folder : folders) {
        addClue(clues.names, std::move(folder.name), "the folder's name");
    }
    if (lowered(file.extension().string()) == ".exe") {
        addClue(clues.names, versionName(file), "the program's version info");
    }
    addClue(clues.names, fileTitle(file), "the file's name");
    return clues;
}

std::optional<StoreApp> sameTitle(const std::vector<StoreApp>& apps, std::string_view title) {
    const std::string key = titleKey(unmarked(std::string{title}));
    const auto found = std::ranges::find_if(apps, [&key](const StoreApp& app) {
        return !key.empty() && titleKey(unmarked(app.title)) == key;
    });
    return found == apps.end() ? std::nullopt : std::optional{*found};
}

Guess identify(const fs::path& file, const StoreSearch& search, const std::string& shortcut) {
    Clues clues = cluesOf(file);
    if (!shortcut.empty()) {
        std::erase_if(clues.names, [&shortcut](const Clue& clue) {
            return titleKey(clue.name) == titleKey(shortcut);
        });
        clues.names.insert(clues.names.begin(), Clue{shortcut, "the shortcut's name"});
    }
    Guess guess{.title = clues.names.empty() ? file.stem().string() : clues.names.front().name,
                .kind = own::kindOf(file),
                .steamAppId = clues.steamAppId,
                .appIdOnDisk = !clues.steamAppId.empty(),
                .from = clues.names.empty() ? "the file's name" : clues.names.front().from};
    std::size_t searched = 0;
    for (const Clue& clue : clues.names) {
        if (searched++ == mostSearches) {
            break;
        }
        const std::vector<StoreApp> apps = search(clue.name);
        const auto named = std::ranges::find(apps, clues.steamAppId, &StoreApp::appId);
        const std::optional<StoreApp> match =
            clues.steamAppId.empty() ? sameTitle(apps, clue.name)
                                     : (named == apps.end() ? std::nullopt : std::optional{*named});
        if (match) {
            guess.title = unmarked(match->title);
            guess.steamAppId = match->appId;
            guess.from = "Steam's store, from " + clue.from;
            break;
        }
    }
    return guess;
}

std::vector<StoreApp> parseStoreSearch(std::string_view json) {
    const nlohmann::json answer = nlohmann::json::parse(json, nullptr, false);
    std::vector<StoreApp> apps;
    if (!answer.is_object() || !answer.contains("items") || !answer["items"].is_array()) {
        return apps;
    }
    for (const nlohmann::json& item : answer["items"]) {
        if (!item.is_object() || item.value("type", "") != "app" || !item.contains("id") ||
            !item["id"].is_number_unsigned() || !item.contains("name") ||
            !item["name"].is_string()) {
            continue;
        }
        apps.push_back(
            {std::to_string(item["id"].get<std::uint64_t>()), item["name"].get<std::string>()});
    }
    return apps;
}

std::string storeSearchUrl(std::string_view name, std::string_view store) {
    std::string term;
    for (const char c : name) {
        const auto byte = static_cast<unsigned char>(c);
        const bool kept = byte >= 0x80 || std::isalnum(byte) != 0;
        if (kept) {
            term += c;
        } else if (!term.empty() && term.back() != ' ') {
            term += ' ';
        }
    }
    while (!term.empty() && term.back() == ' ') {
        term.pop_back();
    }
    return std::string{store} + "/api/storesearch/?term=" + net::percentEncode(term) +
           "&l=english&cc=US";
}

} // namespace opensu::library::identify
