#include "game_folder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <system_error>
#include <utility>

#include "fileio/host_path.hpp"
#include "game_identify.hpp"
#include "ini_file.hpp"
#include "lucent/log.h"
#include "net/web_client.hpp"
#include "shell_link.hpp"
#include "titles.hpp"

namespace opensu::library::folder {
namespace {

namespace fs = std::filesystem;
using fileio::pathOnHost;
using own::Entry;

/// Whole words that say a file is no game.
constexpr std::array<std::string_view, 17> toolWords{
    "install",     "installer",     "installation", "setup",           "uninstall",
    "uninstaller", "uninst",        "redist",       "redistributable", "redistributables",
    "prereq",      "prerequisites", "directx",      "dotnet",          "updater",
    "patcher",     "autoupdate"};
/// What a tool's `titleKey` holds anywhere.
constexpr std::array<std::string_view, 16> toolParts{
    "crashreport", "crashhandler",  "crashsender", "crashpad", "crashdump", "bugreport",
    "errorreport", "easyanticheat", "battleye",    "vcredist", "dxsetup",   "dxwebsetup",
    "dotnetfx",    "ue4prereq",     "unins000",    "beservice"};
/// Store clients and game launchers, as whole `titleKey`s: each only starts other games.
constexpr std::array<std::string_view, 21> storeClients{"steam",
                                                        "steamwebhelper",
                                                        "epicgameslauncher",
                                                        "epicgames",
                                                        "galaxyclient",
                                                        "goggalaxy",
                                                        "battlenet",
                                                        "origin",
                                                        "eadesktop",
                                                        "eaapp",
                                                        "ubisoftconnect",
                                                        "upc",
                                                        "uplay",
                                                        "rockstargameslauncher",
                                                        "rockstarlauncher",
                                                        "launcher",
                                                        "heroic",
                                                        "lutris",
                                                        "playnite",
                                                        "itch",
                                                        "amazongames"};
/// URL schemes that open a page rather than start a game.
constexpr std::array<std::string_view, 5> pageSchemes{"http", "https", "ftp", "mailto", "about"};
/// A Linux entry's `Exec=` placeholders that have no file to stand for here.
constexpr std::string_view fieldCode = "%";

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

std::string extensionOf(const fs::path& file) {
    return lowered(file.extension().string());
}

/// `name`'s words: runs of letters and digits, a capital starting a new one ("UnityCrashHandler").
std::vector<std::string> wordsOf(std::string_view name) {
    std::vector<std::string> words;
    std::string word;
    char before = '\0';
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) == 0) {
            if (!word.empty()) {
                words.push_back(std::exchange(word, {}));
            }
        } else {
            if (std::isupper(u) != 0 && std::islower(static_cast<unsigned char>(before)) != 0) {
                words.push_back(std::exchange(word, {}));
            }
            word += static_cast<char>(std::tolower(u));
        }
        before = c;
    }
    if (!word.empty()) {
        words.push_back(std::move(word));
    }
    return words;
}

/// The id a game found at `found` keeps: FNV-1a of the path, which no run or build changes.
std::string idOf(const fs::path& found) {
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offsetBasis;
    for (const char c : found.generic_string()) {
        hash = (hash ^ static_cast<unsigned char>(c)) * prime;
    }
    std::array<char, 16> digits{};
    const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), hash, 16);
    return "folder-" + std::string{digits.data(), end.ptr};
}

/// `line` as words: spaces separate them, double quotes hold them together.
std::vector<std::string> splitCommand(std::string_view line) {
    std::vector<std::string> words;
    std::string word;
    bool quoted = false;
    bool started = false;
    for (const char c : line) {
        if (c == '"') {
            quoted = !quoted;
            started = true;
        } else if ((c == ' ' || c == '\t') && !quoted) {
            if (started) {
                words.push_back(std::exchange(word, {}));
                started = false;
            }
        } else {
            word += c;
            started = true;
        }
    }
    if (started) {
        words.push_back(std::move(word));
    }
    return words;
}

/// Whether `file` starts as a program does: an ELF header, or a `#!` line. A drive without Unix
/// modes marks every file executable, so the bit alone says nothing.
bool looksRunnable(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    std::array<char, 4> head{};
    in.read(head.data(), head.size());
    if (in.gcount() < 4) {
        return false;
    }
    const std::string_view start{head.data(), head.size()};
    return start.starts_with("#!") || start == "\x7F"
                                               "ELF";
}

/// Whether `file` is a program of a Windows game here.
bool isWindowsProgram(const fs::path& file) {
    return extensionOf(file) == ".exe";
}

/// Whether `file` is a Linux game's own program, not a library or data with an execute bit.
bool isLinuxProgram(const fs::path& file) {
    const std::string extension = extensionOf(file);
    if (extension == ".appimage" || extension == ".sh") {
        return true;
    }
    return extension.empty() && own::kindOf(file).has_value() && looksRunnable(file);
}

/// A program's entry, named by the clues around it inside `folder`.
Entry programEntry(const fs::path& file, const fs::path& folder) {
    const identify::Clues clues = identify::cluesOf(file, folder);
    return Entry{.id = idOf(file),
                 .title = clues.names.empty() ? file.stem().string() : clues.names.front().name,
                 .file = file,
                 .origin = own::Origin::Folder,
                 .originId = file.string(),
                 .steamAppId = clues.steamAppId};
}

std::string urlPath(std::string_view url) {
    constexpr std::string_view scheme = "file:///";
    std::string decoded = net::percentDecode(url.substr(scheme.size()));
#ifndef _WIN32
    decoded.insert(0, "/");
#endif
    return decoded;
}

/// The number at the end of a `steam://rungameid/<n>` link; empty for any other.
std::string steamAppOf(std::string_view url) {
    constexpr std::string_view prefix = "steam://rungameid/";
    if (!url.starts_with(prefix)) {
        return {};
    }
    const std::string id{url.substr(prefix.size())};
    return isSteamAppId(id) ? id : std::string{};
}

/// What opens `url` here.
std::vector<std::string> openerOf(const std::string& url) {
#ifdef _WIN32
    return {"explorer.exe", url};
#elif defined(__APPLE__)
    return {"open", url};
#else
    return {"xdg-open", url};
#endif
}

std::optional<Entry> urlEntry(const fs::path& file) {
    const std::string url = iniValue(file, "InternetShortcut", "URL").value_or("");
    const std::size_t colon = url.find(':');
    if (colon == std::string::npos || among(pageSchemes, lowered(url.substr(0, colon)))) {
        return std::nullopt;
    }
    Entry entry{.id = idOf(file),
                .title = file.stem().string(),
                .file = file,
                .origin = own::Origin::Folder,
                .originId = file.string()};
    if (const std::string icon = iniValue(file, "InternetShortcut", "IconFile").value_or("");
        !icon.empty()) {
        entry.icon = pathOnHost(icon);
    }
    const std::string index = iniValue(file, "InternetShortcut", "IconIndex").value_or("");
    std::from_chars(index.data(), index.data() + index.size(), entry.iconIndex);
    if (url.starts_with("file:///")) {
        entry.file = pathOnHost(urlPath(url));
        if (!own::kindOf(entry.file)) {
            return std::nullopt;
        }
        return entry;
    }
    entry.steamAppId = steamAppOf(url);
    entry.command = openerOf(url);
    return entry;
}

std::optional<Entry> linkEntry(const fs::path& file) {
    const std::optional<ShellLink> link = readShellLink(file);
    if (!link || link->target.empty()) {
        return std::nullopt;
    }
    const fs::path target = pathOnHost(link->target);
    if (!own::kindOf(target) || notAGame(target.stem().string())) {
        return std::nullopt;
    }
    return Entry{.id = idOf(file),
                 .title = file.stem().string(),
                 .file = target,
                 .args = splitCommand(link->arguments),
                 .workingDirectory = link->workingDirectory.empty()
                                         ? fs::path{}
                                         : pathOnHost(link->workingDirectory),
                 .origin = own::Origin::Folder,
                 .originId = file.string(),
                 .icon = link->iconLocation.empty() ? fs::path{} : pathOnHost(link->iconLocation),
                 .iconIndex = link->iconIndex};
}

std::optional<Entry> desktopEntry(const fs::path& file) {
    const auto value = [&file](std::string_view key) {
        return iniValue(file, "Desktop Entry", key).value_or("");
    };
    if (value("Type") != "Application" || value("NoDisplay") == "true" ||
        value("Hidden") == "true") {
        return std::nullopt;
    }
    std::vector<std::string> command = splitCommand(value("Exec"));
    std::erase_if(command, [](const std::string& word) {
        return word.size() == 2 && word.starts_with(fieldCode);
    });
    if (command.empty()) {
        return std::nullopt;
    }
    Entry entry{.id = idOf(file),
                .title = value("Name").empty() ? file.stem().string() : value("Name"),
                .file = file,
                .workingDirectory = fs::path{value("Path")},
                .origin = own::Origin::Folder,
                .originId = file.string(),
                .icon = fs::path{value("Icon")},
                .command = std::move(command)};
    for (const std::string& word : entry.command) {
        if (std::string app = steamAppOf(word); !app.empty()) {
            entry.steamAppId = std::move(app);
        }
    }
    return entry;
}

/// How well `file` stands for the game in `subfolder`: its name the folder's beats the larger.
std::pair<bool, std::uintmax_t> scoreOf(const fs::path& file, const fs::path& subfolder) {
    std::error_code error;
    const std::uintmax_t size = fs::file_size(file, error);
    const std::string stem = titleKey(file.stem().string());
    return {!stem.empty() && titleKey(subfolder.filename().string()).starts_with(stem),
            error ? 0 : size};
}

/// The program that stands for the game in `subfolder`; none when it holds no game.
std::optional<fs::path> programIn(const fs::path& subfolder, Host host) {
    std::optional<fs::path> best;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{subfolder, error}) {
        const fs::path& file = entry.path();
        std::error_code type;
        const bool program =
            (isWindowsProgram(file) || (host == Host::Linux && isLinuxProgram(file))) &&
            entry.is_regular_file(type);
        if (!program || notAGame(file.stem().string())) {
            continue;
        }
        if (!best || scoreOf(file, subfolder) > scoreOf(*best, subfolder)) {
            best = file;
        }
    }
    return best;
}

/// The value after `<key>name</key>` in an XML property list; empty when it has none.
std::string plistValue(const std::string& plist, std::string_view name) {
    const std::string key = "<key>" + std::string{name} + "</key>";
    const std::size_t at = plist.find(key);
    if (at == std::string::npos) {
        return {};
    }
    const std::size_t open = plist.find("<string>", at + key.size());
    const std::size_t close = plist.find("</string>", at + key.size());
    if (open == std::string::npos || close == std::string::npos || close < open) {
        return {};
    }
    const std::size_t from = open + std::string_view{"<string>"}.size();
    return plist.substr(from, close - from);
}

std::optional<Entry> bundleEntry(const fs::path& app) {
    std::ifstream in{app / "Contents" / "Info.plist", std::ios::binary};
    const std::string plist{std::istreambuf_iterator<char>{in}, {}};
    std::string program = plistValue(plist, "CFBundleExecutable");
    if (program.empty()) {
        program = app.stem().string();
    }
    fs::path executable = app / "Contents" / "MacOS" / program;
    if (!own::kindOf(executable)) {
        return std::nullopt;
    }
    std::string title = plistValue(plist, "CFBundleDisplayName");
    if (title.empty()) {
        title = plistValue(plist, "CFBundleName");
    }
    std::string icon = plistValue(plist, "CFBundleIconFile");
    if (!icon.empty() && !icon.ends_with(".icns")) {
        icon += ".icns";
    }
    return Entry{.id = idOf(app),
                 .title = title.empty() ? app.stem().string() : std::move(title),
                 .file = std::move(executable),
                 .origin = own::Origin::Folder,
                 .originId = app.string(),
                 .icon = icon.empty() ? fs::path{} : app / "Contents" / "Resources" / icon};
}

/// Adds `entry` unless another way to the same file was found first.
void keep(std::vector<Entry>& games, std::set<fs::path>& started, std::optional<Entry> entry) {
    if (!entry || (entry->command.empty() && !started.insert(entry->file).second)) {
        return;
    }
    // A shortcut's command may name a program that the scan reaches on its own.
    for (const std::string& word : entry->command) {
        started.emplace(word);
    }
    games.push_back(*std::move(entry));
}

/// `folder`'s visible files and folders, sorted, so a scan comes out the same each time.
std::vector<fs::directory_entry> listed(const fs::path& folder) {
    std::vector<fs::directory_entry> entries;
    std::error_code error;
    for (const fs::directory_entry& entry :
         fs::directory_iterator{folder, fs::directory_options::skip_permission_denied, error}) {
        if (!entry.path().filename().string().starts_with(".")) {
            entries.push_back(entry);
        }
    }
    if (error) {
        lucent::warn("own-games", "games folder {}: {}", folder.string(), error.message());
    }
    std::ranges::sort(entries, {}, [](const fs::directory_entry& entry) {
        return entry.path();
    });
    return entries;
}

bool isDirectory(const fs::directory_entry& entry) {
    std::error_code error;
    return entry.is_directory(error);
}

void scanMac(const fs::path& folder, std::vector<Entry>& games, std::set<fs::path>& started) {
    const auto bundles = [&](const fs::path& within) {
        for (const fs::directory_entry& entry : listed(within)) {
            if (extensionOf(entry.path()) == ".app" && !notAGame(entry.path().stem().string())) {
                keep(games, started, bundleEntry(entry.path()));
            }
        }
    };
    bundles(folder);
    for (const fs::directory_entry& entry : listed(folder)) {
        if (isDirectory(entry) && extensionOf(entry.path()) != ".app") {
            bundles(entry.path());
        }
    }
}

} // namespace

Host thisHost() noexcept {
#ifdef _WIN32
    return Host::Windows;
#elif defined(__APPLE__)
    return Host::Mac;
#else
    return Host::Linux;
#endif
}

bool notAGame(std::string_view name) {
    const std::string key = titleKey(name);
    if (among(storeClients, key)) {
        return true;
    }
    if (std::ranges::any_of(toolParts, [&key](std::string_view part) {
            return key.find(part) != std::string::npos;
        })) {
        return true;
    }
    return std::ranges::any_of(wordsOf(name), [](const std::string& word) {
        return among(toolWords, word);
    });
}

bool isShortcut(const fs::path& file) {
    const std::string extension = extensionOf(file);
    return extension == ".lnk" || extension == ".url" || extension == ".desktop";
}

std::optional<Entry> shortcutEntry(const fs::path& file) {
    if (notAGame(file.stem().string())) {
        return std::nullopt;
    }
    const std::string extension = extensionOf(file);
    if (extension == ".lnk") {
        return linkEntry(file);
    }
    if (extension == ".url") {
        return urlEntry(file);
    }
    return extension == ".desktop" ? desktopEntry(file) : std::nullopt;
}

std::vector<Entry> scan(const fs::path& folder, Host host) {
    std::vector<Entry> games;
    std::set<fs::path> started;
    if (host == Host::Mac) {
        scanMac(folder, games, started);
        return games;
    }
    const std::vector<fs::directory_entry> entries = listed(folder);
    for (const fs::directory_entry& entry : entries) {
        const std::string extension = extensionOf(entry.path());
        if (host == Host::Windows ? extension == ".lnk" || extension == ".url"
                                  : extension == ".desktop") {
            keep(games, started, shortcutEntry(entry.path()));
        }
    }
    for (const fs::directory_entry& entry : entries) {
        const fs::path& file = entry.path();
        if (host == Host::Linux && !isDirectory(entry) && isLinuxProgram(file) &&
            !notAGame(file.stem().string())) {
            keep(games, started, programEntry(file, folder));
        }
    }
    for (const fs::directory_entry& entry : entries) {
        if (!isDirectory(entry) || notAGame(entry.path().filename().string())) {
            continue;
        }
        if (const std::optional<fs::path> program = programIn(entry.path(), host)) {
            keep(games, started, programEntry(*program, folder));
        }
    }
    return games;
}

} // namespace opensu::library::folder
